/*
 * ipsocket.cc
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

#include "ipsocket.hh"

using namespace ipsocket;

LoggerPtr IpSocket::logger(Logger::getLogger("ipsocket"));

IpSocket::IpSocket(Configuration &configuration)
	: _configuration(configuration)
{
	// IPv4 socket
	if ((_socketRaw4 = socket(AF_INET, SOCK_RAW, IPPROTO_RAW)) < 0)
	{
		LOG4CXX_FATAL(logger, "Failed to get IPv4 socket descriptor ");
	}
	if (setuid(getuid()) < 0)//no special permissions needed anymore
	{
		LOG4CXX_ERROR(logger, "Could not set UID of to " << getuid());
	}
	else
	{
		LOG4CXX_DEBUG(logger, "UID set to " << getuid());
	}
	//set option to send pre-made IP packets over raw socket
	const int on = 1;
	if (setsockopt(_socketRaw4, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0)
	{
		LOG4CXX_ERROR(logger, "Socket option IP_HDRINCL could not be set for "
				"raw IPv4 socket");
	}
	else
	{
		LOG4CXX_DEBUG(logger, "Socket option IP_HDRINCL enabled");
	}
	// Libnet socket for packet fragmentation
	_libnetPtag = 0;
	_libnetRaw4 = libnet_init(LIBNET_RAW4,
			_configuration.networkDevice().c_str(), _errbuf);
	if (_libnetRaw4 == NULL)
	{
		LOG4CXX_FATAL(logger, "Libnet Raw4 socket could not be created: "
				<< _errbuf);
	}
	else
	{
		IpAddress ipAddressNic =  libnet_get_ipaddr4(_libnetRaw4);
		LOG4CXX_DEBUG(logger, "Libnet Raw4 socket created on interface with IP "
				<< ipAddressNic.str());
	}
	/* Getting max payload size */
	_maxIpPacketPayloadSize = _configuration.mtu() - LIBNET_IPV4_H;
	/* making it a multiple of 8 */
	_maxIpPacketPayloadSize -= (_maxIpPacketPayloadSize % 8);
	// Set hints for IPv4 raw socket
	bzero(&_hints, sizeof(struct addrinfo));
	_hints.ai_flags = AI_CANONNAME;  /* always return canonical name */
	_hints.ai_family = 0;//AF_INET;
	_hints.ai_socktype = 0;//IPPROTO_RAW;
	// Libnet socket for ICMP messages
	/*_libnetLink = libnet_init(LIBNET_LINK,
			_configuration.networkDevice().c_str(), _errbuf);
	if (_libnetLink == NULL)
	{
		LOG4CXX_FATAL(logger, "Libnet Link socket for ICMP messages could not "
				"be created: " << _errbuf);
	}
	else
	{
		LOG4CXX_DEBUG(logger, "Libnet Link socket for ICMP messages created");
	}*/
}

IpSocket::~IpSocket()
{
	_mutexLibnetRaw4.lock();
	libnet_destroy(_libnetRaw4);
	_mutexLibnetRaw4.unlock();
	LOG4CXX_DEBUG(logger, "Libnet Raw4 socket destroyed");
	//libnet_destroy(_libnetLink);
	//LOG4CXX_DEBUG(logger, "Libnet Link socket destroyed");
}

void IpSocket::icmpUnreachable(IpAddress sourceIpAddress,
		IpAddress destinationIpAddress, uint16_t code)
{
	//u_char payload[8] = {0x11, 0x11, 0x22, 0x22, 0x00, 0x08, 0xc6, 0xa5};
	//u_long payloadSize = 8;
	/*_errbuf = libnet_build_ipv4(LIBNET_IPV4_H + 8, IPTOS_LOWDELAY |
			IPTOS_THROUGHPUT, 0, 0, 64, IPPROTO_UDP, 0, sourceIpAddress.uint(),
			destinationIpAddress.uint(), payload, payloadSize, _libnetLink,
			_errbuf);
	if (_errbuf == -1)
	{
		LOG4CXX_ERROR(logger, "IPv4 header could not be built");
		return;
	}
	icmp = libnet_build_icmpv4_unreach(ICMP_UNREACH, ICMP_UNREACH_NEEDFRAG, 0,
			NULL, 0, _libnetLink, icmp);
	if (icmp == -1)
	{
		LOG4CXX_ERROR(stderr, "Can't build ICMP header: "
				<< libnet_geterror(_libnetLink));
		return;
	}*/
}

bool IpSocket::sendPacket(uint8_t *data, uint16_t &dataSize)
{
	struct ip *ipHeader;
	IpAddress destinationIpAddress;
	ipHeader = (struct ip *)data;
	destinationIpAddress = ipHeader->ip_dst.s_addr;
	// single packet, but choose socket
	if (_configuration.socketType() == RAWIP)
	{
		return _sendPacketRawIp(data, dataSize);
	}
	else if (_configuration.socketType() == LIBNET)
	{
		uint16_t ipHeaderLength = 4 * ipHeader->ip_hl;
		if (_maxIpPacketPayloadSize < (dataSize  - ipHeaderLength))
		{
			LOG4CXX_DEBUG(logger, "IP fragmentation required, as packet of"
					"length " << dataSize << " is larger than MTU of "
					<< _maxIpPacketPayloadSize <<". As the IP header gets "
					"rewritten, this could cause issues with already fragmented"
					" IP packets");
			return _sendFragments(data, dataSize);
		}
		return _sendPacketLibnet(data, dataSize);
	}
	return false;
}

bool IpSocket::_sendFragments(uint8_t *data, uint16_t &dataSize)
{
	/*headerOffset = fragmentation flags + offset (in bytes) divided by 8*/
	uint16_t payloadOffset = 0;
	uint16_t headerOffset = 0; /* value of the offset */
	int bytesWritten, ipPacketPayloadSize;
	uint32_t ipHeaderId = libnet_get_prand(LIBNET_PR16);
	ipPacketPayloadSize = _maxIpPacketPayloadSize;
	struct ip *ipHeader = (struct ip *)data;
	IpAddress destinationIpAddress = ipHeader->ip_dst.s_addr;
	IpAddress sourceIpAddress = ipHeader->ip_src.s_addr;
	// This packet is already a fragmented one... discard
	if (ipHeader->ip_off == IP_MF)
	{
		LOG4CXX_WARN(logger, "IP packet towards " << destinationIpAddress.str()
				<< " is already a fragmented one. Unsupported feature! Dropping"
						"it");
		return false;
	}
	uint8_t *payload = data + (4 * ipHeader->ip_hl);
	uint16_t payloadSize = dataSize - (4 * ipHeader->ip_hl);
	_mutexLibnetRaw4.lock();
	_libnetPtag = libnet_build_ipv4((LIBNET_IPV4_H + _maxIpPacketPayloadSize),
			ipHeader->ip_tos, ipHeaderId, IP_MF, ipHeader->ip_ttl,
			ipHeader->ip_p, 0, ipHeader->ip_src.s_addr, ipHeader->ip_dst.s_addr,
			payload, _maxIpPacketPayloadSize, _libnetRaw4, 0);
	if (_libnetPtag == -1 )
	{
		LOG4CXX_ERROR(logger, "Error building fragmented IP header: "
				<< libnet_geterror(_libnetRaw4));
		libnet_clear_packet(_libnetRaw4);
		_mutexLibnetRaw4.unlock();
		return false;
	}
	bytesWritten = libnet_write(_libnetRaw4);
	if (bytesWritten == -1)
	{
		LOG4CXX_ERROR(logger, "First IP fragment with ID " << ipHeaderId
				<< "to IP endpoint" << destinationIpAddress.str() << " has not "
						"been sent. Error: " << libnet_geterror(_libnetRaw4));
		libnet_clear_packet(_libnetRaw4);
		_mutexLibnetRaw4.unlock();
		return false;
	}
	LOG4CXX_TRACE(logger, "IP fragment with ID " << ipHeaderId << ", offset "
			<< headerOffset << " and size " << ipPacketPayloadSize << " sent "
			"to IP endpoint " << destinationIpAddress.str() << " ("
			<< ipPacketPayloadSize << "/" << payloadSize << ")");
	libnet_clear_packet(_libnetRaw4);
	_libnetPtag = LIBNET_PTAG_INITIALIZER;
	/* Now send off the remaining fragments */
	payloadOffset += ipPacketPayloadSize;
	while (payloadSize > payloadOffset)
	{
		/* Building IP header */
		/* checking if there will be more fragments */
		if ((payloadSize - payloadOffset) > _maxIpPacketPayloadSize)
		{
			headerOffset = IP_MF + (payloadOffset)/8;
			ipPacketPayloadSize = _maxIpPacketPayloadSize;
		}
		else
		{
			headerOffset = payloadOffset/8;
			ipPacketPayloadSize = payloadSize - payloadOffset;
		}
		_libnetPtag = libnet_build_ipv4((LIBNET_IPV4_H + ipPacketPayloadSize),
				0, ipHeaderId, headerOffset, ipHeader->ip_ttl, ipHeader->ip_p,
				0, sourceIpAddress.uint(), destinationIpAddress.uint(), payload
				+ payloadOffset, ipPacketPayloadSize, _libnetRaw4, _libnetPtag);
		if (_libnetPtag == -1)
		{
			LOG4CXX_ERROR(logger, "Error building IP header for destination "
					<< destinationIpAddress.str() << ": "
					<< libnet_geterror(_libnetRaw4));
			libnet_clear_packet(_libnetRaw4);
			_mutexLibnetRaw4.unlock();
			return false;
		}
		bytesWritten = libnet_write(_libnetRaw4);
		if (bytesWritten == -1)
		{
			LOG4CXX_ERROR(logger, "Error writing packet for IP destination "
					<< destinationIpAddress.str() << ": "
					<< libnet_geterror(_libnetRaw4));
			libnet_clear_packet(_libnetRaw4);
			_mutexLibnetRaw4.unlock();
			return false;
		}
		LOG4CXX_TRACE(logger, "IP fragment with ID " << ipHeaderId
				<< ", offset " << headerOffset << " and size "
				<< ipPacketPayloadSize << " sent to "
				<< destinationIpAddress.str() << " ("
				<< payloadOffset + ipPacketPayloadSize << "/" << payloadSize
				<< " bytes sent)");
		/* Updating the offset */
		payloadOffset += ipPacketPayloadSize;
		//libnet_clear_packet(_libnetRaw4);// if it doesn't get cleared, the
		//second IP fragment cannot be sent: libnet_write_link(): only -1 bytes
		// written (Message too long)
	}
	libnet_clear_packet(_libnetRaw4);
	_mutexLibnetRaw4.unlock();
	return true;
}

bool IpSocket::_sendPacketLibnet(uint8_t *data, uint16_t &dataSize)
{
	int bytesWritten;
	struct ip *ipHeader = (struct ip *)data;
	IpAddress destinationIpAddress = ipHeader->ip_dst.s_addr;
	IpAddress sourceIpAddress = ipHeader->ip_src.s_addr;
	uint16_t payloadSize = dataSize - (4 * ipHeader->ip_hl);
	uint8_t *payload = data + (4 * ipHeader->ip_hl);
	_mutexLibnetRaw4.lock();
	// calculate the payload of the transport
	_libnetPtag = libnet_build_ipv4((LIBNET_IPV4_H + payloadSize),
			ipHeader->ip_tos, ipHeader->ip_id, ipHeader->ip_off,
			ipHeader->ip_ttl, ipHeader->ip_p, 0, sourceIpAddress.uint(),
			destinationIpAddress.uint(), payload, payloadSize, _libnetRaw4, 0);
	if (_libnetPtag == -1)
	{
		LOG4CXX_ERROR(logger, "Error building fragmented IP header: "
				<< libnet_geterror(_libnetRaw4));
		libnet_clear_packet(_libnetRaw4);
		_mutexLibnetRaw4.unlock();
		return false;
	}
	bytesWritten = libnet_write(_libnetRaw4);
	if (bytesWritten == -1)
	{
		LOG4CXX_ERROR(logger, "IP packet with ID " << ipHeader->ip_id
				<< "to IP endpoint" << destinationIpAddress.str() << " has not "
						"been sent. Error: " << libnet_geterror(_libnetRaw4));
		libnet_clear_packet(_libnetRaw4);
		_mutexLibnetRaw4.unlock();
		return false;
	}
	LOG4CXX_TRACE(logger, "IP packet of size " << LIBNET_IPV4_H + payloadSize
			<< " from " << sourceIpAddress.str() << " sent to IP endpoint "
			<< destinationIpAddress.str());
	libnet_clear_packet(_libnetRaw4);
	_mutexLibnetRaw4.unlock();
	return true;
}

bool IpSocket::_sendPacketRawIp(uint8_t *data, uint16_t &dataSize)
{
	struct ip *ipHeader;
	int bytesWritten;
	ipHeader = (struct ip *)data;
	IpAddress destinationIpAddress = ipHeader->ip_dst.s_addr;
	// optional way START
	struct hostent *hp;
	if ((hp = gethostbyname(destinationIpAddress.str().c_str())) == NULL)
	{
		LOG4CXX_ERROR(logger, "Could not resolve DST IP "
				<< destinationIpAddress.str());
		return false;
	}
	bcopy(hp->h_addr_list[0], &ipHeader->ip_dst.s_addr, hp->h_length);
	struct sockaddr_in dst;
	dst.sin_addr = ipHeader->ip_dst;
	dst.sin_family = AF_INET;
	_mutexSocketRaw4.lock();
	bytesWritten = sendto(_socketRaw4, data, dataSize, 0,
			(struct sockaddr *)&dst, sizeof(dst));
	// END
	/*
	int n;
	struct addrinfo *addressInfo;
	n = getaddrinfo(destinationIpAddress.str().c_str(), NULL, &_hints,
				&addressInfo);
	if (n == -1)
	{
		LOG4CXX_ERROR(logger, "Could not get address info for "
				<< destinationIpAddress.str() << ": " << gai_strerror(n));
	}
	bytesWritten = sendto(_socketRaw4, data, dataSize, 0, addressInfo->ai_addr,
			sizeof(*addressInfo->ai_addr));*/
	if (bytesWritten == -1)
	{
		if (errno == EMSGSIZE)
		{
			LOG4CXX_DEBUG(logger, "IP packet of length " << dataSize << " could"
					" not be sent to " << destinationIpAddress.str() << " via "
					"raw socket: "	<< strerror(errno));
			_mutexSocketRaw4.unlock();
			LOG4CXX_DEBUG(logger, "Using libnet to send it as IP fragments");
			return _sendFragments(data, dataSize);
		}
		else
		{
			LOG4CXX_WARN(logger, "IP packet of length " << dataSize << " could "
					"not be sent to " << destinationIpAddress.str() << ": "
					<< strerror(errno));
			_mutexSocketRaw4.unlock();
		}
		return false;
	}
	_mutexSocketRaw4.unlock();
	LOG4CXX_TRACE(logger, "IP packet of length " << dataSize << " sent to IP "
			"endpoint " << destinationIpAddress.str());
	return true;
}
