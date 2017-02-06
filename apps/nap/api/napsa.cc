/*
 * napsa.cc
 *
 *  Created on: 24 May 2016
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

#include "napsa.hh"

using namespace api::napsa;
using namespace std;

LoggerPtr NapSa::logger(Logger::getLogger("api.napsa"));

NapSa::NapSa(Namespaces &namespaces, Statistics &statistics)
	: _namespaces(namespaces),
	  _statistics(statistics)
{
	_listeningSocket = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
	if (_listeningSocket == -1)
	{
		LOG4CXX_FATAL(logger, "NAP-SA listening socket could not be created");
		return;
	}
	LOG4CXX_DEBUG(logger, "NAP-SA listening socket created");
}

NapSa::~NapSa() {}

void NapSa::operator ()()
{
	struct sockaddr_nl sourceAddress, destinationAddress;
	struct nlmsghdr *nlh = NULL;
	struct msghdr msg;
	struct iovec iov;
	int bytesReceived;
	memset(&sourceAddress, 0, sizeof(sourceAddress));
	sourceAddress.nl_family = AF_NETLINK;
	sourceAddress.nl_pid = PID_NAP_SA_LISTENER;
	sourceAddress.nl_pad = 0;
	sourceAddress.nl_groups = 0; // unicast
	bytesReceived = bind(_listeningSocket, (struct sockaddr*) &sourceAddress,
			sizeof(sourceAddress));
	if (bytesReceived == -1)
	{
		LOG4CXX_FATAL(logger, "NAP-SA listener could not be bound to PID "
				<< sourceAddress.nl_pid);
		return;
	}
	LOG4CXX_INFO(logger, "NAP-SA listener socket was bound to socket with PID "
			<< sourceAddress.nl_pid);
	memset(&destinationAddress, 0, sizeof(destinationAddress));
	// Message will have two uint32_ts
	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(sizeof(uint32_t)
			+ sizeof(uint32_t)));
	memset(nlh, 0, NLMSG_SPACE(sizeof(uint32_t)	+ sizeof(uint32_t)));
	iov.iov_base = (void*)nlh;
	iov.iov_len = NLMSG_SPACE(sizeof(uint32_t) + sizeof(uint32_t));
	msg.msg_name = (void *)&destinationAddress;
	msg.msg_namelen = sizeof(destinationAddress);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	uint32_t surrogateFqdn;
	uint32_t ipAddress;
	IpAddress surrogateIpAddress;
	uint16_t surrogatePort;
	uint16_t offset;
	while(true)
	{
		bytesReceived = recvmsg(_listeningSocket, &msg, 0);
		if (bytesReceived > 0)
		{
			// [1] Hashed FQDN
			memcpy(&surrogateFqdn, NLMSG_DATA(nlh), sizeof(uint32_t));
			offset = sizeof(uint32_t);
			// [2] IP Address
			memcpy(&ipAddress, (uint8_t *)NLMSG_DATA(nlh) + offset,
					sizeof(uint32_t));
			offset += sizeof(uint32_t);
			// [3] Port
			memcpy(&surrogatePort, (uint16_t *)NLMSG_DATA(nlh) + offset,
					sizeof(uint16_t));
			surrogateIpAddress = ipAddress;
			// switch based on activate/deactivate
			switch(nlh->nlmsg_type)
			{
			case NAP_SA_ACTIVATE:
				LOG4CXX_DEBUG(logger, "Surrogate activation command received "
						"for " << surrogateFqdn << " on "
						<< surrogateIpAddress.str());
				// report new surrogate server via MOLY
				_statistics.ipEndpointAdd(surrogateIpAddress, surrogatePort);
				// TODO report enpoint was enabled (state)
				break;
			case NAP_SA_DEACTIVATE:
				LOG4CXX_DEBUG(logger, "Surrogate deactivation command received "
						"for " << surrogateFqdn << " on "
						<< surrogateIpAddress.str());
				// TODO report surrogate was disabled via MOLY
				break;
			default:
				LOG4CXX_ERROR(logger, "Unknown primitive type received from "
						"SA: " << nlh->nlmsg_type);
			}
			// de/activate surrogate
			IcnId cid(surrogateFqdn);
			_namespaces.surrogacy(cid, nlh->nlmsg_type);
		}
		else
		{
			LOG4CXX_ERROR(logger, "Message from SA could not be read: "
					<< strerror(errno));
		}
	}
	LOG4CXX_FATAL(logger, "Closing NAP-SA listening socket");
	close(_listeningSocket);
	free(nlh);
}
