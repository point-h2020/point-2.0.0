/*
 * ipsocket.hh
 *
 *  Created on: 4 May 2016
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

#ifndef NAP_IPSOCKET_HH_
#define NAP_IPSOCKET_HH_

#include <arpa/inet.h>
#include <boost/thread/mutex.hpp>
#include <libnet.h>
#include <log4cxx/logger.h>
#include <netinet/ip.h>
#include <sys/socket.h>

#include <configuration.hh>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

using namespace configuration;
using namespace log4cxx;

namespace ipsocket {
/*!
 * \brief Implementation of the IP socket towards IP endpoints
 */
class IpSocket
{
	static LoggerPtr logger;
public:
	/*!
	 * \brief Constructor
	 */
	IpSocket(Configuration &configuration);
	/*!
	 * \brief Destructor
	 */
	~IpSocket();
	/*!
	 * \brief Send an IP packet to an IP endpoint
	 *
	 * This method determines whether or not IP fragmentation is required. If
	 * so, it calls _sendFragments(). If not, it reads the IP destination from
	 * the IP header, skips it an uses a raw IPv4 socket to send off the IP
	 * packet payload.
	 *
	 * \param data Pointer to the IP packet (header+payload) to be sent
	 * \param dataSize The length of the IP packet
	 *
	 * \return Boolean indicating if packet has been successfully sent
	 */
	bool sendPacket(uint8_t *data, uint16_t &dataSize);
	/*!
	 * \brief Informing a particular IP endpoint that the destination is
	 * unreachable
	 *
	 * https://tools.ietf.org/html/rfc792
	 *
	 * \param sourceIpAddress IP address of the endpoint which was the
	 * supposed to be reached
	 * \param destinationIpAddress The IP address of the endpoint which
	 * should receive the ICMP message
	 * \param code The ICMP code following RFC 792
	 */
	void icmpUnreachable(IpAddress sourceIpAddress,
			IpAddress destinationIpAddress, uint16_t code);
private:
	Configuration &_configuration; /*!< Reference to the configuration */
	int _socketRaw4;
	boost::mutex _mutexSocketRaw4;/*!< Ensure transaction safe write socket
	operations*/
	libnet_t *_libnetRaw4; /*!< Fragment and send IP packets to endpoints */
	//libnet_t *_libnetLink; /*!< Send ICMP unreachable messages */
	libnet_ptag_t _libnetPtag;
	boost::mutex _mutexLibnetRaw4;
	uint16_t _maxIpPacketPayloadSize;
	struct addrinfo _hints;
	char _errbuf[LIBNET_ERRBUF_SIZE];
	/*!
	 * \brief Fragment IP packet and send fragments
	 *
	 * In case an incoming IP packet does not fit into a single MTU towards the
	 * IP endpoint it is aimed for, this method sends multiple IP fragments to
	 * the endpoint by splitting the packet according to the MTU of the IP
	 * interface and setting the appropriate offset byte in the IP header
	 *
	 * \param data Pointer to the data
	 * \param dataSize Reference to the length of the packet
	 *
	 * \return Boolean indicating whether or not the pacekt was sent off
	 * successfully
	 */
	bool _sendFragments(uint8_t *data, uint16_t &dataSize);
	/*!
	 * \brief Send an IP packet to an IP endpoint using libnet socket
	 *
	 * \param data Pointer to the IP packet (header+payload) to be sent
	 * \param dataSize The length of the IP packet
	 *
	 * \return Boolean indicating if packet has been successfully sent
	 */
	bool _sendPacketLibnet(uint8_t *data, uint16_t &dataSize);
	/*!
	 * \brief Send an IP packet to an IP endpoint using RAWIP socket
	 *
	 * \param data Pointer to the IP packet (header+payload) to be sent
	 * \param dataSize The length of the IP packet
	 *
	 * \return Boolean indicating if packet has been successfully sent
	 */
	bool _sendPacketRawIp(uint8_t *data, uint16_t &dataSize);
};

} /* namespace ipsocket */

#endif /* NAP_IPSOCKET_HH_ */
