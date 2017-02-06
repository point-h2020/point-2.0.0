/*
 * tcpserver.cc
 *
 *  Created on: 20 Dec 2015
 *      Author: Sebastian Robitzsch <sebastian.robitzsch@interdigital.com>
 *
 * This file is part of Blackadder.
 *
 * Blackadder is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Blackadder is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Blackadder.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <thread>

#include <proxies/http/tcpserver.hh>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define ENIGMA 23 // https://en.wikipedia.org/wiki/23_enigma

using namespace proxies::http::tcpserver;

LoggerPtr TcpServer::logger(Logger::getLogger("proxies.http.tcpserver"));

TcpServer::TcpServer(Configuration &configuration, Namespaces &namespaces,
		int socketFd, IpAddress ipAddress)
	: _configuration(configuration),
	  _namespaces(namespaces),
	  _socketFd(socketFd),
	  _ipAddress(ipAddress)
{
	_localSurrogateFd = -1;
	_httpRequestMethod = HTTP_METHOD_UNKNOWN;
}

TcpServer::~TcpServer() {
}

void TcpServer::operator ()()
{
	vector<std::thread> surrogateTcpClientThreads;
	fd_set rset;
	uint16_t sessionKey = _socketFd;
	char packet[_configuration.tcpServerSocketBufferSize()];
	bzero(packet, _configuration.tcpServerSocketBufferSize());
	string httpRequest;
	int bytesWritten;
	uint16_t packetSize;
	string fqdn, resource;
	http_methods_t httpMethod;
	LOG4CXX_TRACE(logger, "New active TCP session with IP endpoint "
			<< _ipAddress.str() << " via socket FD " << _socketFd);
	FD_ZERO(&rset);//Initialising descriptor
	FD_SET(_socketFd, &rset); // Set descriptor
	// check again that FD is >= 0
	if (_socketFd == -1)
	{
		return;
	}
	// As long as socket is readable and no thread interruption requested, read
	// data from TCP client (IP endpoint)
	while (FD_ISSET(_socketFd, &rset) &&
			!boost::this_thread::interruption_requested())
	{
		bytesWritten = read(_socketFd, packet,
				_configuration.tcpServerSocketBufferSize());
		if (bytesWritten <= 0)
		{
			switch(errno)
			{
			case 0:
				LOG4CXX_TRACE(logger, "Socket " << _socketFd << " closed "
						"correctly by client " << _ipAddress.str() << ": "
						<< strerror(errno));
				break;
			case ECONNRESET:
				LOG4CXX_DEBUG(logger, "Socket " << _socketFd << " reset by "
						"client " << _ipAddress.str() << ": "
						<< strerror(errno));
				break;
			default:
				LOG4CXX_DEBUG(logger,  "Socket " << _socketFd << " closed "
						"unexpectedly by client " << _ipAddress.str() << ": "
						<< strerror(errno));
			}
			shutdown(_socketFd, SHUT_RD);
			break;
		}
		packetSize = bytesWritten;
		LOG4CXX_TRACE(logger, "HTTP request of length " << bytesWritten
				<< " received from " << _ipAddress.str() << " via socket FD "
				<< _socketFd);

		httpMethod = _httpMethod(packet, packetSize);
		// POST or PUT and 1st++ TCP segment which only has data and no HTTP
		// header
		if (httpMethod == HTTP_METHOD_UNKNOWN)
		{
			LOG4CXX_TRACE(logger, "HTTP request is spawn over multiple TCP "
					"segments. Using HTTP method " << _httpRequestMethod
					<< ", FQDN " << _httpRequestFqdn << " and resource "
					<< _httpRequestResource);
			httpMethod = _httpRequestMethod;
			fqdn = _httpRequestFqdn;
			resource = _httpRequestResource;
		}
		else
		{
			LOG4CXX_TRACE(logger, "Entire HTTP request:\n" << packet);
			_httpRequestMethod = httpMethod;
			fqdn = _fqdn(packet, packetSize);
			_httpRequestFqdn = fqdn;
			resource = _resource(packet, packetSize);
			_httpRequestResource = resource;
		}
		//only send it if FQDN was found (resource will be at least '/')
		if (fqdn.length() > 0)
		{
			_namespaces.Http::handleRequest(fqdn, resource, httpMethod,
					(uint8_t *)packet, packetSize, sessionKey);
		}
		else
		{
			LOG4CXX_DEBUG(logger, "FQDN is empty. Dropping packet");
		}

		bzero(packet, _configuration.tcpServerSocketBufferSize());
	}
	_namespaces.Http::deleteSessionKey(sessionKey);
	// FIXME HTTP session awareness not implemented. Using sleep to not close
	// the socket immediately
	boost::this_thread::sleep(boost::posix_time::seconds(ENIGMA));
	LOG4CXX_TRACE(logger, "Closing socket FD " << _socketFd);
	close(_socketFd);
}

string TcpServer::_fqdn(char *packet, uint16_t &packetSize)
{
	string httpPacket;
	size_t pos = 0;
	ostringstream fqdn;
	// In case HTTP POST/PUT arrived w/o header
	// FIXME Find a better way to avoid parsing HTTP POST messages which do not
	// have am HTTP header
	if (packetSize > 1000)
	{//limit the packet size to not parse a large message for nothing
		httpPacket.assign(packet, 1000);
	}
	else
	{
		httpPacket.assign(packet, packetSize);
	}
	// make the string lower case
	std::transform(httpPacket.begin(), httpPacket.end(), httpPacket.begin(),
			::tolower);
	while(true)
	{
		pos = httpPacket.find("host: ", ++pos);
		if (pos != std::string::npos)
		{
			size_t fqdnLength = 0;
			size_t positionStart = pos + 6;
			string character;
			character = httpPacket[positionStart];
			// First get length of FQDN
			while (strcmp(character.c_str(), "\n") != 0 &&
					(pos + 6) <= packetSize)
			{
				fqdnLength++;
				pos++;
				character = httpPacket[pos + 6];
			}
			// Now read the FQDN and ignore last blank character
			for (size_t it = 0; it < (fqdnLength - 1); it++)
			{
				fqdn << httpPacket[positionStart + it];
			}
			LOG4CXX_TRACE(logger, "FQDN '" << fqdn.str() << "' found in HTTP"
					"request");
			break;
		}
		else
		{
			LOG4CXX_ERROR(logger, "FQDN could not be found in HTTP request");
			break;
		}
	}
	return fqdn.str();
}

http_methods_t TcpServer::_httpMethod(char *packet, uint16_t &packetSize)
{
	string httpPacket;
	http_methods_t httpMethod = HTTP_METHOD_UNKNOWN;
	size_t pos = 0;
	size_t posConnect = 0;
	size_t posDelete = 0;
	size_t posExtension = 0;
	size_t posGet = 0;
	size_t posHead = 0;
	size_t posOptions = 0;
	size_t posPost = 0;
	size_t posPut = 0;
	size_t posTrace = 0;
	httpPacket.assign(packet, 50);
	std::transform(httpPacket.begin(), httpPacket.end(), httpPacket.begin(),
				::tolower);
	while (httpMethod == HTTP_METHOD_UNKNOWN)
	{
		pos++;
		posConnect = httpPacket.find("connect", ++posConnect);
		if (posConnect != std::string::npos)
		{
			httpMethod = HTTP_METHOD_REQUEST_CONNECT;
			break;
		}
		posDelete = httpPacket.find("delete", ++posDelete);
		if (posDelete != std::string::npos)
		{
			httpMethod = HTTP_METHOD_REQUEST_DELETE;
			break;
		}
		posExtension = httpPacket.find("extension", ++posExtension);
		if (posExtension != std::string::npos)
		{
			httpMethod = HTTP_METHOD_REQUEST_EXTENSION;
			break;
		}
		posGet = httpPacket.find("get", ++posGet);
		if (posGet != std::string::npos)
		{
			httpMethod = HTTP_METHOD_REQUEST_GET;
			break;
		}
		posHead = httpPacket.find("head", ++posHead);
		if (posHead != std::string::npos)
		{
			httpMethod = HTTP_METHOD_REQUEST_HEAD;
			break;
		}
		posOptions = httpPacket.find("options", ++posOptions);
		if (posOptions != std::string::npos)
		{
			httpMethod = HTTP_METHOD_REQUEST_OPTIONS;
			break;
		}
		posPost = httpPacket.find("post", ++posPost);
		if (posPost != std::string::npos)
		{
			httpMethod = HTTP_METHOD_REQUEST_POST;
			break;
		}
		posPut = httpPacket.find("put", ++posPut);
		if (posPut != std::string::npos)
		{
			httpMethod = HTTP_METHOD_REQUEST_PUT;
			break;
		}
		posPut = httpPacket.find("delete", ++posPut);
		if (posPut != std::string::npos)
		{
			httpMethod = HTTP_METHOD_REQUEST_DELETE;
			break;
		}
		posTrace = httpPacket.find("trace", ++posTrace);
		if (posTrace != std::string::npos)
		{
			httpMethod = HTTP_METHOD_REQUEST_TRACE;
			break;
		}
		// End of packet reached
		if (pos == packetSize)
		{
			break;
		}
	}
	if (httpMethod != HTTP_METHOD_UNKNOWN)
	{
		LOG4CXX_TRACE(logger, "Method '" << httpMethod << "' found in HTTP "
				"request");
	}
	return httpMethod;
}

string TcpServer::_resource(char *packet, uint16_t &packetSize)
{
	string httpPacket;
	size_t pos = 0;
	ostringstream resource;
	httpPacket.assign((char *)packet, packetSize);
	resource << "/";
	while(true)
	{
		pos = httpPacket.find(" /", ++pos);
		if (pos != std::string::npos)
		{
			string character;
			character = httpPacket[pos + 2];
			while (strcmp(character.c_str(), " ") != 0)
			{
				resource << httpPacket[pos + 2];
				pos++;
				character = httpPacket[pos + 2];
			}
			break;
		}
		else
		{
			LOG4CXX_ERROR(logger, "Could not find resource in HTTP packet");
			break;
		}
	}
	LOG4CXX_TRACE(logger, "Resource '" << resource.str() << "' found in HTTP"
			" request");
	return resource.str();
}
