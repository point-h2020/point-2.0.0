/*
 * tcpclient.cc
 *
 *  Created on: 28 Jul 2016
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

#include <list>

#include "tcpclient.hh"

using namespace proxies::http::tcpclient;

LoggerPtr TcpClient::logger(Logger::getLogger("proxies.http.tcpclient"));

TcpClient::TcpClient(Configuration &configuration, Namespaces &namespaces)
	: _configuration(configuration),
	  _namespaces(namespaces)
{
	_reverseLookup = new reverse_lookup_t;
	_reverseLookupMutex = new boost::mutex;
	_socketFds = new socket_fds_t;
	_socketFdsMutex = new boost::mutex;
	_socketState = new unordered_map<uint16_t, bool>;
	_socketStateMutex = new boost::mutex;
	_packetTmp = NULL;
	_packetSizeTmp = 0;
	_sessionKeyTmp = 0;
}

TcpClient::~TcpClient()
{
	//delete _reverseLookupMutex;
	//delete _socketFdsMutex;
}

void TcpClient::preparePacketToBeSent(IcnId &cId, IcnId &rCId,
		uint16_t &sessionKey, string &nodeId,
		uint8_t *packet, uint16_t &packetSize)
{
	_cidTmp = cId;
	_rCidTmp = rCId;
	_sessionKeyTmp = sessionKey;
	_nodeId = nodeId;
	_packetTmp = (uint8_t *)malloc(packetSize);
	memcpy(_packetTmp, packet, packetSize);
	_packetSizeTmp = packetSize;
}

void TcpClient::operator ()()
{
	bool callRead;
	int socketFd;
	// first copy the packet and several variables to thread safe memory
	// locations
	NodeId nodeId = _nodeId;
	IcnId cid = _cidTmp;
	IcnId rCid = _rCidTmp;
	uint16_t remoteSocketFd = _sessionKeyTmp;
	uint16_t packetSize = _packetSizeTmp;
	uint8_t *packet = (uint8_t *)malloc(packetSize);
	memcpy(packet, _packetTmp, packetSize);
	free(_packetTmp);
	socketFd = _tcpSocket(cid, nodeId, remoteSocketFd, callRead);

	if (socketFd == -1)
	{
		LOG4CXX_ERROR(logger, "Socket could not be created to. server for CID "
				<< cid.print() << ": " << strerror(errno));
		free(packet);
		return;
	}

	// Send off the data using the new FD _nextTcpClientFd
	int bytesWritten = -1;

	if (!_socketWriteable(socketFd))
	{
		LOG4CXX_TRACE(logger, "Socket FD " << socketFd << " not writeable "
				"anymore. Packet of length " << packetSize << " will be "
						"dropped");
		free(packet);
		return;
	}

	bytesWritten = write(socketFd, packet, packetSize);

	if (bytesWritten <= 0)
	{
		LOG4CXX_DEBUG(logger, "HTTP request of length " << packetSize
				<< " could not be sent to IP endpoint using FD "
				<< socketFd	<< ": " << strerror(errno));
		free(packet);
		return;
	}
	else
	{
		LOG4CXX_TRACE(logger, "HTTP request of length " << bytesWritten
				<< " sent off to IP endpoint using socket FD "
				<< socketFd);
	}

	_addReverseLookup(socketFd, rCid);
	free(packet);

	if (!callRead)
	{
		LOG4CXX_TRACE(logger, "Another functor is already calling read()");
		return;
	}

	// Now prepare to run the reading actions on this socket
	fd_set rset;
	packet = (uint8_t *)malloc(_configuration.tcpClientSocketBufferSize());
	bool firstPacket = true;
	uint16_t sessionKey = socketFd;
	FD_SET(socketFd, &rset);
	int bytesReceived;

	while (FD_ISSET(socketFd, &rset))
	{
		bytesReceived = read(socketFd, packet,
				_configuration.tcpClientSocketBufferSize());

		if (bytesReceived <= 0)
		{
			_setSocketState(socketFd, false);

			if (errno > 0)
			{
				LOG4CXX_DEBUG(logger,  "Socket " << socketFd << " closed "
						"unexpectedly: " << strerror(errno));
			}
			else
			{
				LOG4CXX_TRACE(logger,  "Socket " << socketFd << " closed: "
						<< strerror(errno));
			}

			shutdown(socketFd, SHUT_RDWR);
			break;
		}

		packetSize = bytesReceived;
		LOG4CXX_TRACE(logger, "Packet of length " << packetSize
				<< " received via socket FD " << socketFd);

		if (!_namespaces.Http::handleResponse(_rCid(socketFd),
				sessionKey, firstPacket, packet, packetSize))
		{
			LOG4CXX_TRACE(logger, "Response read from socket FD " << socketFd
					<< " was not sent to cNAP(s) for rCID "
					<< _rCid(socketFd).print() << " > SK " << sessionKey);
			break;
		}
		firstPacket = false;
	}
	_deleteSocketState(socketFd);
	// Now clean up states in the HTTP handler
	_namespaces.Http::closeCmcGroup(_rCid(socketFd), sessionKey);
	// Cleaning up _reverseLookup map
	_reverseLookupMutex->lock();
	_reverseLookupIt = _reverseLookup->find(socketFd);

	if (_reverseLookupIt == _reverseLookup->end())
	{
		LOG4CXX_TRACE(logger, "Socket FD " << socketFd << " cannot be found in "
				"reverse lookup map");
		_reverseLookupMutex->unlock();
		free(packet);
		return;
	}

	_reverseLookup->erase(_reverseLookupIt);
	_reverseLookupMutex->unlock();
	// Cleaning up remote socket FD
	unordered_map<uint16_t, uint16_t>::iterator rSksIt;
	_socketFdsMutex->lock();
	_socketFdsIt = _socketFds->find(nodeId.uint());

	// NID found
	if (_socketFdsIt != _socketFds->end())
	{
		rSksIt = _socketFdsIt->second.find(remoteSocketFd);

		// remote socket FD found
		if (rSksIt != _socketFdsIt->second.end())
		{
			_socketFdsIt->second.erase(rSksIt);
			LOG4CXX_TRACE(logger, "Remote socket FD (SK) " << remoteSocketFd
					<< " deleted for NID " << nodeId.uint() << " > "
					<< _socketFdsIt->first);
		}

		// If this was the last rSFD, delete the entire NID entry
		if (_socketFdsIt->second.empty())
		{
			LOG4CXX_TRACE(logger, "That was the last remote socket FD (SK) for "
					"NID " << nodeId.uint() << ". " << "Deleting map key "
					<< _socketFdsIt->first);
			_socketFds->erase(_socketFdsIt);
		}
	}

	_socketFdsMutex->unlock();
	close(socketFd);//finally closing socket
	free(packet);
}

void TcpClient::_addReverseLookup(int &socketFd, IcnId &rCId)
{
	_reverseLookupMutex->lock();
	_reverseLookupIt = _reverseLookup->find(socketFd);
	if (_reverseLookupIt == _reverseLookup->end())
	{
		rcid_ripd_t socketFdStruct;
		socketFdStruct.ripd = 0;
		socketFdStruct.rCId = rCId;
		_reverseLookup->insert(pair<int, rcid_ripd_t>(socketFd,
				socketFdStruct));
		LOG4CXX_TRACE(logger, "Reverse look-up for socket FD " << socketFd
				<< " added with rCID " << rCId.print());
	}
	else
	{
		LOG4CXX_TRACE(logger, "Reverse look-up already exists for socket FD "
				<< socketFd << " > rCID "  << rCId.print());
	}
	_reverseLookupMutex->unlock();
}

void TcpClient::_deleteSocketState(uint16_t socketFd)
{
	_socketStateMutex->lock();
	_socketStateIt = _socketState->find(socketFd);
	if (_socketStateIt != _socketState->end())
	{
		_socketState->erase(_socketStateIt);
		LOG4CXX_TRACE(logger, "Socket FD " << socketFd << " deleted from list "
				"of writable socket FDs");
	}
	_socketStateMutex->unlock();
}

IcnId &TcpClient::_rCid(int &socketFd)
{
	_reverseLookupMutex->lock();
	_reverseLookupIt = _reverseLookup->find(socketFd);
	if (_reverseLookupIt == _reverseLookup->end())
	{
		LOG4CXX_FATAL(logger, "Socket FD not found in reverse lookup map ... "
				"seg fault!");
	}
	_reverseLookupMutex->unlock();
	return _reverseLookupIt->second.rCId;
}

void TcpClient::_setSocketState(uint16_t socketFd, bool state)
{
	_socketStateMutex->lock();
	_socketStateIt = _socketState->find(socketFd);
	if (_socketStateIt == _socketState->end())
	{
		_socketState->insert(pair<uint16_t, bool>(socketFd, state));
		LOG4CXX_TRACE(logger, "New socket FD " << socketFd << " added to list "
				"of known sockets with state " << state);
		_socketStateMutex->unlock();
		return;
	}
	_socketStateIt->second = state;
	LOG4CXX_TRACE(logger, "Socket FD " << socketFd << " updated with state "
			<< state);
	_socketStateMutex->unlock();
}

bool TcpClient::_socketWriteable(uint16_t socketFd)
{
	bool state = false;
	_socketStateMutex->lock();
	_socketStateIt = _socketState->find(socketFd);
	if (_socketStateIt != _socketState->end())
	{
		state = _socketStateIt->second;
	}
	_socketStateMutex->unlock();
	return state;
}

int TcpClient::_tcpSocket(IcnId &cId, NodeId &nodeId,
		uint16_t &remoteSocketFd, bool &callRead)
{
	callRead = false;
	int socketFd;
	unordered_map<uint16_t, uint16_t>::iterator rSksIt;
	// First check if socket already exists for this HTTP session
	_socketFdsMutex->lock();
	_socketFdsIt = _socketFds->find(nodeId.uint());

	// NID found
	if (_socketFdsIt != _socketFds->end())
	{
		rSksIt = _socketFdsIt->second.find(remoteSocketFd);

		// remote socket FD found
		if (rSksIt != _socketFdsIt->second.end())
		{
			uint16_t localSocketFd = rSksIt->second;
			_socketFdsMutex->unlock();
			LOG4CXX_TRACE(logger, "Existing local socket FD " << localSocketFd
					<< " used for sending off this HTTP request from NID "
					<< nodeId.uint() << " for CID " << cId.print());
			return localSocketFd;
		}
		else
		{
			LOG4CXX_TRACE(logger, "NID " << nodeId.uint() << " > SFD "
					<< remoteSocketFd << " does not exist in socketFds map");
		}
	}
	else
	{
		LOG4CXX_DEBUG(logger, "NID " << nodeId.uint() << " does not exist in "
				"socketFds map");
	}
	/* DO NOT unlock socketFdsMutex, as this would allow the second received
	 * HTTP fragment (over ICN) to think there's no socket available and a new
	 * one must be created. This is because the socket creation takes slightly
	 * longer then receiving the next fragment and retrieving it from the ICN
	 * buffer
	 */
	callRead = true;//as new socket is created and the functor must call read(()
	bool endpointFound = false;
	IpAddress ipAddressEndpoint;
	uint16_t port;
	list<pair<IcnId, pair<IpAddress, uint16_t>>> ipEndpoints;
	list<pair<IcnId, pair<IpAddress, uint16_t>>>::iterator ipEndpointsIt;
	ipEndpoints = _configuration.fqdns();
	// get IP address for this CID (FQDN)
	ipEndpointsIt = ipEndpoints.begin();

	while(ipEndpointsIt != ipEndpoints.end())
	{
		ipEndpointsIt->first;
		if (ipEndpointsIt->first.uint() == cId.uint())
		{
			ipAddressEndpoint = ipEndpointsIt->second.first;
			port = ipEndpointsIt->second.second;
			endpointFound = true;
			break;
		}
		ipEndpointsIt++;
	}

	if (!endpointFound)
	{
		LOG4CXX_DEBUG(logger, "IP endpoint for " << cId.print() << " was not "
				"configured");
		_socketFdsMutex->unlock();
		return -1;
	}

	// create socket
	socketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);

	if (socketFd == -1)
	{
		LOG4CXX_ERROR(logger, "Socket could not be created: "
				<< strerror(errno));
		socketFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
		_socketFdsMutex->unlock();
		return socketFd;
	}

	struct sockaddr_in serverAddress;
	bzero(&serverAddress, sizeof(serverAddress));
	serverAddress.sin_family = AF_INET;
	serverAddress.sin_port = htons(port);
	inet_pton(AF_INET, ipAddressEndpoint.str().c_str(),
			&serverAddress.sin_addr);

	// Connect to server
	if (connect(socketFd, (struct sockaddr *) &serverAddress,
			sizeof(serverAddress)) < 0)
	{
		LOG4CXX_ERROR(logger, "TCP client socket towards "
				<< ipAddressEndpoint.str() << ":" << port << " could not be "
						"established: "	<< strerror(errno));
		close(socketFd);
		_socketFdsMutex->unlock();
		return -1;
	}

	LOG4CXX_TRACE(logger, "TCP socket opened with FD " << socketFd
			<< " towards " << ipAddressEndpoint.str() << ":" << port);
	// add new socket to socket FD map
	_socketFdsIt = _socketFds->find(nodeId.uint());

	// NID does not exists
	if (_socketFdsIt == _socketFds->end())
	{
		unordered_map<uint16_t, uint16_t> rSks;
		rSks.insert(pair<uint16_t, uint16_t> (remoteSocketFd, socketFd));
		_socketFds->insert(pair<uint32_t, unordered_map<uint16_t, uint16_t>>
				(nodeId.uint(), rSks));
		LOG4CXX_TRACE(logger, "New local socket " << socketFd << " towards "
				<< ipAddressEndpoint.str() << ":" << port << " added to "
				"socketFds map for new NID " << nodeId.uint() << " > rSFD "
				<< remoteSocketFd);
		_socketFdsMutex->unlock();
		_setSocketState(socketFd, true);
		return socketFd;
	}

	// NID exists
	rSksIt = _socketFdsIt->second.find(remoteSocketFd);

	// rSFD  does not exist
	if (rSksIt == _socketFdsIt->second.end())
	{
		_socketFdsIt->second.insert(pair<uint16_t, uint16_t> (remoteSocketFd,
				socketFd));
		LOG4CXX_TRACE(logger, "New local socket " << socketFd << " towards "
				<< ipAddressEndpoint.str() << ":" << port << " added to "
				"socketFds map for known NID " << nodeId.uint() << " but new "
						"rSFD "	<< remoteSocketFd);
	}
	else
	{
		LOG4CXX_WARN(logger, "NID " << nodeId.uint() << " and rSFD "
				<< remoteSocketFd << " do exist in socketFds map out of a "
						"sudden");
	}

	_socketFdsMutex->unlock();
	_setSocketState(socketFd, true);
	return socketFd;
}
