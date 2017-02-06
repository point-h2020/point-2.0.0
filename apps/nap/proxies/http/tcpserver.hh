/*
 * tcpserver.hh
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

#ifndef NAP_PROXIES_HTTP_TCPSERVER_HH_
#define NAP_PROXIES_HTTP_TCPSERVER_HH_

#include <log4cxx/logger.h>
#include <string.h>

#include <configuration.hh>
#include <enumerations.hh>
#include <types/ipaddress.hh>
#include <namespaces/namespaces.hh>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

using namespace configuration;
using namespace log4cxx;
using namespace namespaces;

namespace proxies
{
namespace http
{
namespace tcpserver
{
/*!
 * \brief TCP server
 *
 * This class handles all incoming TCP sessions from IP endpoints, i.e., session
 * which have been created by the IP endpoint (HTTP request messages).
 */
class TcpServer
{
	static LoggerPtr logger;
public:
	/*!
	 * \brief Constructor
	 */
	TcpServer(Configuration &configuration, Namespaces &namespaces,
			int socketFd, IpAddress ipAddress);
	/*!
	 * \brief Destructor
	 */
	~TcpServer();
	/*!
	 * \brief Functor
	 */
	void operator()();
private:
	Configuration &_configuration;/*!< Reference to Configuration class */
	Namespaces &_namespaces;/*!< Reference to Namespace class */
	//uint32_t _socketId;
	int _socketFd; /*!< Pointer to socket used for this particular TCP session
	*/
	string _httpRequestFqdn;/*!< In case of an HTTP request POST || PUT this variable
	allows to handle a request which is fragmented into multiple TCP segments*/
	IpAddress _ipAddress; /*!< IP address of IP endpoint (TCP client) */
	int _localSurrogateFd; /*!< If local surrogacy has been enabled this FD
	holds the socket */
	http_methods_t _httpRequestMethod;/*!< In case of an HTTP request POST ||
	PUT this variable allows to handle a request which is fragmented into
	multiple TCP segments*/
	string _httpRequestResource;/*!< In case of an HTTP request POST ||
	PUT this variable allows to handle a request which is fragmented into
	multiple TCP segments*/
	/*!
	 * \brief Obtain the FQDN from an HTTP header
	 *
	 * This method parses the HTTP message until \r\r appears and extracts the
	 * FQDN provided after 'Host: '
	 *
	 * \param packet Pointer to the HTTP packet
	 * \param packetSize Length of the HTTP packet
	 *
	 * \return FQDN of the HTTP packet provided
	 */
	string _fqdn(char *packet, uint16_t &packetSize);
	/*!
	 * \brief Obtain HTTP method from an HTTP message
	 *
	 * Wrapper for Api::getHttpRequestMethod(const string &httpRequest)
	 *
	 * \param packet Pointer to the HTTP request packet
	 * \param packetSize Reference to the size of the packet
	 *
	 * \return This method returns the HTTP request method using the enumeration
	 * HttpMethods
	 */
	http_methods_t _httpMethod(char *packet, uint16_t &packetSize);
	/*!
	 * \brief Obtain the resource from an HTTP header
	 *
	 * This method parses the HTTP message until \r\r appears and extracts the
	 * resource provided in the header
	 *
	 * \param packet Pointer to the HTTP packet
	 * \param packetSize Length of the HTTP packet
	 *
	 * \return Resource of the HTTP packet provided
	 */
	string _resource(char *packet, uint16_t &packetSize);
};

} /* namespace tcpserver */

} /* namespace http */

} /* namespace proxies */

#endif /* NAP_PROXIES_HTTP_TCPSERVER_HH_ */
