/*
 * demux.cc
 *
 *  Created on: 19 Apr 2015
 *      Author: Sebastian Robitzsch <sebastian.robitzsch@interdigital.com>
 *
 * This file is part of Blackadder.
 *
 * Blackadder is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
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

#include "demux.hh"

using namespace configuration;
using namespace demux;
using namespace log4cxx;
using namespace namespaces::ip;

LoggerPtr Demux::logger(Logger::getLogger("demux"));

Demux::Demux(Namespaces &namespaces, Configuration &configuration,
		Statistics &statistics)
	: _namespaces(namespaces),
	  _configuration(configuration),
	  _statistics(statistics)
{
	_linkHdrLen = 0;
	_pcapSocket = NULL;
}

Demux::~Demux() {}

void Demux::operator()() {
	char errbuf[PCAP_ERRBUF_SIZE];	/*!< Error string */
	string pcapPacketFilter = string();
	struct bpf_program bpf;
	// More info abt the pcap fields here: http://www.tcpdump.org/pcap3_man.html
	// _device: the interface on which libpcap should sniff
	// 65535/2500: the max size of the memory for a single packet. wpasupplicant
	// uses 2500
	// 1||0: promiscuous mode en-/disabled
	// 0: Readout time delay to decrease CPU load (adds delay if not 0!)
	// errbuf: Error string
	_pcapSocket = pcap_open_live(_configuration.networkDevice().c_str(),
			65535, 0, 0, errbuf);
	if (_pcapSocket == NULL)
	{
		LOG4CXX_FATAL(logger, "Cannot open PCAP listener on "
				<< _configuration.networkDevice());
		return;
	}
	LOG4CXX_DEBUG(logger, "PCAP socket opened for device "
			<< _configuration.networkDevice());
	// Obtaining the IP address of the NAP
	uint32_t ipAddress, netmask;
	if (pcap_lookupnet(_configuration.networkDevice().c_str(), &ipAddress,
			&netmask, errbuf) < 0)
	{
		LOG4CXX_FATAL(logger, "Local IP address of interface "
				<< _configuration.networkDevice() << " could not be obtained");
		pcap_close(_pcapSocket);
		return;
	}
	_napIpAddress = ipAddress;
	LOG4CXX_TRACE(logger, "Local IP address of '"
			<< _configuration.networkDevice() << "' is "
			<< _napIpAddress.str());
	// set PCAP filters to capture IPv4 packets only
	// The PCAP filter follows the TCPDUMP syntax:
	// http://www.tcpdump.org/manpages/pcap-filter.7.html
	ostringstream oss;
	oss << "ip and !(dst net 224.0.0.0/24) and ";
	if (_configuration.httpHandler())
	{
		oss << "!(port 80) and ";
	}
	oss << "!(dst 255.255.255.255)";
	if (_configuration.hostBasedNap())
	{
		oss << " and !(dst " << _configuration.endpointIpAddress().str() << ")";
	}
	// Set filter for non(!) ICN GW scenarios
	else if (!_configuration.icnGateway())
	{
		oss << " and !(dst net "
				<< _configuration.hostRoutingPrefix().networkAddress().str()
				<< " mask "
				<< _configuration.hostRoutingPrefix().netmask().str() << ")";

	}
	// PCAP filter for ICN GW
	// TODO build PCAP filter over all configured routing prefixes in the ICN GW
	else
	{			// DST net is any NAP
		oss << " and (dst net "
				<< _configuration.icnGatewayRoutingPrefix().networkAddress().str()
				<< " mask "
				<< _configuration.icnGatewayRoutingPrefix().netmask().str()
				<< ")";
	}
	pcapPacketFilter = oss.str();
	LOG4CXX_DEBUG(logger, "Set PCAP filter to " << pcapPacketFilter);
	// compile the PCAP filter
	if (pcap_compile(_pcapSocket, &bpf, pcapPacketFilter.c_str(), 0, 0))
			//_configuration.hostRoutingPrefix().netmask().uint()))
	{
		LOG4CXX_FATAL(logger, "PCAP filter could not be compiled: " <<
						pcap_geterr(_pcapSocket));
		pcap_close(_pcapSocket);
		return;
	}
	LOG4CXX_DEBUG(logger, "PCAP filter compiled");
	if (pcap_setfilter(_pcapSocket, &bpf) < 0)
	{
		LOG4CXX_FATAL(logger, "PCAP filter could not be set: " <<
				pcap_geterr(_pcapSocket));
		pcap_close(_pcapSocket);
		return;
	}
	LOG4CXX_DEBUG(logger, "PCAP filter set");
	// Determine the datalink layer type
	LOG4CXX_TRACE(logger, "Reading data link layer header size");
	int linktype;
	if ((linktype = pcap_datalink(_pcapSocket)) < 0)
	{
		LOG4CXX_FATAL(logger, "pcap_datalink() " << pcap_geterr(_pcapSocket));
		pcap_close(_pcapSocket);
		return;
	}
	// Set the datalink layer header size.
	switch (linktype)
	{
	case DLT_NULL:
		_linkHdrLen = 4;
		break;
	case DLT_EN10MB:
		_linkHdrLen = 14;
		break;
	case DLT_SLIP:
	case DLT_PPP:
		_linkHdrLen = 24;
		break;
	default:
		LOG4CXX_FATAL(logger, "Unsupported datalink " << _linkHdrLen);
		pcap_close(_pcapSocket);
		return;
	}
	// Start sniffing for packets
	const u_char *packet;
	struct pcap_pkthdr h;
	// Now read packets as long as this thread has not been interrupted
	LOG4CXX_DEBUG(logger, "Start listening for IP packets");
	while (true)
	{
		packet = pcap_next(_pcapSocket, &h);
		if (packet != NULL)
		{
			_processPacket(&h, packet);
		}
	}
	pcap_close(_pcapSocket);
	LOG4CXX_DEBUG(logger, "PCAP listener closed");
}

bool Demux::_ignorePacket(IpAddress *sourceIpAddress,
		IpAddress *destinationIpAddress)
{
	// Ignore packets sent to the NAP directly
	if (destinationIpAddress->uint() == _napIpAddress.uint())
	{
		return true;
	}
	return false;
}

void Demux::_processPacket(const struct pcap_pkthdr *header,
		const u_char *packet)
{
	struct ip *ipHeader;
	struct ether_header *eptr;
	IpAddress sourceIpAddress, destinationIpAddress;
	eptr = (struct ether_header *) packet;
	LOG4CXX_TRACE(logger, "Processing Ethernet packet: Type "
			<< ntohs(eptr->ether_type) << ", Length " << header->len << ", Cap "
					"Length " << header->caplen);
	if (header->len > header->caplen)
	{
		LOG4CXX_DEBUG(logger, "Number of captured bytes is smaller than the"
				"actual packet. Dropping it ...");
		return;
	}
	if (ntohs(eptr->ether_type) == ETHERTYPE_IP)
	{
		packet += _linkHdrLen; // Skipping Ethernet header
		ipHeader = (struct ip *)packet;
		sourceIpAddress = ipHeader->ip_src.s_addr;
		destinationIpAddress = ipHeader->ip_dst.s_addr;
		int ttl = ipHeader->ip_ttl;
		int ipHeaderLength = 4 * ipHeader->ip_hl;
		LOG4CXX_TRACE(logger, "IP packet received with header: "
				<< "SRC: " << sourceIpAddress.str()
				<< "\tDST: " << destinationIpAddress.str()
				<< "\tID: " << ntohs(ipHeader->ip_id)
				<< "\tTOS: " << ntohs(ipHeader->ip_tos)
				<< "\tTTL: " << ttl
				<< "\tFlags: " << ipHeader->ip_off
				<< "\tIP HL: " << ipHeaderLength
				<< "\tPayload Length: " << ntohs(ipHeader->ip_len));
		if (!_configuration.icnGateway())
		{//only report if this NAP is not an ICN GW
			_statistics.ipEndpointAdd(sourceIpAddress);
		}
		packet += 4 * ipHeader->ip_hl;
		switch (ipHeader->ip_p)
		{
		case IPPROTO_TCP:
		{
			struct tcphdr *tcpHeader;
			tcpHeader = (struct tcphdr*)packet;
			LOG4CXX_TRACE(logger, "TCP Header: "
					<< "Src Port=" << ntohs(tcpHeader->source)
					<< "\tDst Port=" << ntohs(tcpHeader->dest)
					<< "\tSeq=" << ntohl(tcpHeader->seq)
					<< "\tAckSeq=" << ntohl(tcpHeader->ack_seq)
					<< "\tWindow=" << ntohs(tcpHeader->window)
					<< "\tTCP Length=" << 4 * tcpHeader->doff);
			LOG4CXX_TRACE(logger, "Invoking IP handler (TCP)");
			break;
		}
		case IPPROTO_UDP:
		{
			struct udphdr *udpHeader;
			udpHeader = (struct udphdr *)packet;
			LOG4CXX_TRACE(logger, "UDP Header: "
					<< "Src port:" << ntohs(udpHeader->source)
					<< " -> Dst port: " << ntohs(udpHeader->dest));
			break;
		}
		case IPPROTO_ICMP:
		{
			struct icmphdr *icmpHeader;
			icmpHeader = (struct icmphdr *)packet;
			LOG4CXX_TRACE(logger, "ICMP Header: "
					<< "Code: " << ntohs(icmpHeader->code) << " | "
					<< "Type: " << ntohs(icmpHeader->type) << " | "
					<< "ID: " << ntohs(icmpHeader->un.echo.id) << " | "
					<< "Seq: " << ntohs(icmpHeader->un.echo.sequence)
					);
			break;
		}
		case IPPROTO_RAW:
		{
			LOG4CXX_TRACE(logger, "RAW IP packet received. "
					<< "No Layer 4 Header available");
			break;
		}
		}
		packet -= 4 * ipHeader->ip_hl;
		uint16_t packetSize = header->len - _linkHdrLen;
		_namespaces.Ip::handle(destinationIpAddress, (uint8_t *)packet,
				packetSize);
	}
	// ARP packet inside Ethernet frame
	else if (ntohs(eptr->ether_type) == ETHERTYPE_IPV6)
	{
		LOG4CXX_TRACE(logger, "IPv6 packets are not supported");
	}
	else
	{
		LOG4CXX_TRACE(logger, "Ethernet type " << ntohs(eptr->ether_type)
				<< " does not need to be handled. Dropping packet");
	}
}
