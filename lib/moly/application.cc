/*
 * application.cc
 *
 *  Created on: 20 Feb 2016
 *      Author: Sebastian Robitzsch <sebastian.robitzsch@interdigital.com>
 *      		Mays Al-Naday <mfhaln@essex.ac.uk>
 *
 * This file is part of the MOnitoring LibrarY (MOLY).
 *
 * MOLY is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * MOLY is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * MOLY. If not, see <http://www.gnu.org/licenses/>.
 */

#include "application.hh"

Application::Application() {
	_bootstrapSocket = -1;
	struct sockaddr_nl src_addr;
	int ret;
	_reportingSocket = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
	if (_reportingSocket < 0)
	{
		//cout << "((MOLY)) " << __FUNCTION__ << "Reporting socket towards the "
		//		"agent could not be created: "	<< strerror(errno) << endl;
		return;
	}
	memset(&src_addr, 0, sizeof(src_addr));
	memset(&_agentSocketAddress, 0, sizeof(_agentSocketAddress));
	// SRC Address
	src_addr.nl_family = AF_NETLINK;
	_pid = getpid() + (rand() % 100 + 5);
	src_addr.nl_pid = _pid;
	src_addr.nl_groups = 0;  /* not in mcast groups */
	src_addr.nl_pad = 0;
	ret = bind(_reportingSocket, (struct sockaddr*) &src_addr, sizeof(src_addr));
	if (ret < 0)
	{
		//cout << "((MOLY)) " << __FUNCTION__ << "Could not bind to reporting "
		//		"socket using PID " << _pid	<< ": " << strerror(errno)
		//		<< "\n Re-try every second" << endl;
		while (ret < 0)
		{
			sleep(1);
			_pid = getpid() + (rand() % 100 + 5);
			src_addr.nl_pid = _pid;
			ret = bind(_reportingSocket, (struct sockaddr*) &src_addr,
					sizeof(src_addr));
		}
	}
	//cout << "((MOLY)) " << __FUNCTION__ << "Bound to SRC PID " << _pid << endl;
	//DST netlink socket
	_agentSocketAddress.nl_family = AF_NETLINK;
	_agentSocketAddress.nl_pid = PID_MOLY_LISTENER;
	_agentSocketAddress.nl_groups = 0;
	_agentSocketAddress.nl_pad = 0;
}

Application::~Application() {
	if (_bootstrapSocket != -1)
	{
		//cout << "((MOLY)) " << __FUNCTION__ << "Closing bootstrapping socket (F"
		//		"D = " << _bootstrapSocket << ")\n";
		close(_bootstrapSocket);
	}
	if (_reportingSocket != -1)
	{
		//cout << "((MOLY)) " << __FUNCTION__ << "Closing sending socket (FD = "
		//		<< _reportingSocket << ")\n";
		close(_reportingSocket);
	}
}

bool Application::addNode(string name, uint32_t nodeId, uint8_t nodeType)
{
	AddNode addNode(name, nodeId, nodeType);
	return _send(PRIMITIVE_TYPE_ADD_NODE, addNode.pointer(), addNode.size());
}

bool Application::addNodeType(string name, uint8_t nodeType)
{
	AddNodeType addNodeType(name, nodeType);
	return _send(PRIMITIVE_TYPE_ADD_NODE_TYPE, addNodeType.pointer(),
			addNodeType.size());
}

bool Application::addLink(string name, string linkId, uint32_t sourceNodeId,
		uint32_t destinationNodeId, uint8_t linkType)
{
	AddLink addLink(name, linkId, sourceNodeId, destinationNodeId, linkType);
	return _send(PRIMITIVE_TYPE_ADD_LINK, addLink.pointer(), addLink.size());
}

bool Application::cmcGroupSize(uint32_t groupSize)
{
	CmcGroupSize cmcGroupSize(groupSize);
	return _send(PRIMITIVE_TYPE_CMC_GROUP_SIZE, cmcGroupSize.pointer(),
			cmcGroupSize.size());
}

bool Application::httpRequestsPerFqdn(string fqdn, uint32_t httpRequests)
{
	HttpRequestsPerFqdn httpRequestsPerFqdn(fqdn, httpRequests);
	return _send(PRIMITIVE_TYPE_HTTP_REQUESTS_PER_FQDN,
			httpRequestsPerFqdn.pointer(), httpRequestsPerFqdn.size());
}

bool Application::linkState(uint32_t linkId, uint32_t destinationNodeId,
			uint8_t linkType, uint8_t state)
{
	LinkState linkState(linkId, destinationNodeId, linkType, state);
	return _send(PRIMITIVE_TYPE_LINK_STATE, linkState.pointer(),
			linkState.size());
}

bool Application::networkLatencyPerFqdn(uint32_t fqdn, uint16_t latency)
{
	NetworkLatencyPerFqdn networkLatencyPerFqdn(fqdn, latency);
	return _send(PRIMITIVE_TYPE_NETWORK_LATENCY_PER_FQDN,
			networkLatencyPerFqdn.pointer(), networkLatencyPerFqdn.size());
}

bool Application::nodeState(uint8_t nodeType, uint32_t nodeId, uint8_t state)
{
	NodeState nodeState(nodeType, nodeId, state);
	return _send(PRIMITIVE_TYPE_NODE_STATE, nodeState.pointer(),
				nodeState.size());
}

bool Application::startReporting()
{
	int bytesRead;
	struct sockaddr_nl srcAddr, agentAddress;
	struct nlmsghdr *nlh = NULL;
	struct iovec iov;
	struct msghdr msg;
	int ret;
	_bootstrapSocket = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
	if (_bootstrapSocket < 0)
	{
		//cout << "((MOLY)) " << __FUNCTION__ << "Bootstrap socket could not be "
		//		"created\n";
		return false;
	}
	//cout << "((MOLY)) " << __FUNCTION__ << "Bootstrap socket (FD = "
	//		<< _bootstrapSocket << ") towards monitoring agent created\n";
	memset(&msg, 0, sizeof(msg));
	memset(&srcAddr, 0, sizeof(srcAddr));
	memset(&agentAddress, 0, sizeof(agentAddress));
	srcAddr.nl_family = AF_NETLINK;
	srcAddr.nl_pid = getpid() + (rand() % 100 + 5);
	srcAddr.nl_pad = 0;
	srcAddr.nl_groups = 0;
	ret = bind(_bootstrapSocket, (struct sockaddr*) &srcAddr, sizeof(srcAddr));
	if (ret == -1)
	{
		//cout << "((MOLY)) " << __FUNCTION__ << "Could not bind to bootstrap "
		//		"socket using PID "<< srcAddr.nl_pid << " Trying again with "
		//				"different PID" << endl;
		srcAddr.nl_pid = getpid() + (rand() % 100 + 5);
		while (true)
		{
			ret = bind(_bootstrapSocket, (struct sockaddr*) &srcAddr,
					sizeof(srcAddr));
			if (ret != -1)
			{
				break;
			}
		}
	}
	//cout << "((MOLY)) " << __FUNCTION__ << "Successfully bound to trigger "
	//		"socket " << endl;
	//DST netlink socket
	agentAddress.nl_family = AF_NETLINK;
	agentAddress.nl_pid = PID_MOLY_BOOTSTRAP_LISTENER;
	agentAddress.nl_groups = 0;
	agentAddress.nl_pad = 0;
	/* Allocate the required memory (NLH + payload) */
	nlh=(struct nlmsghdr *) malloc(NLMSG_SPACE(sizeof(srcAddr.nl_pid)));
	/* Fill the netlink message header */
	nlh->nlmsg_len = NLMSG_SPACE(sizeof(srcAddr.nl_pid));
	nlh->nlmsg_pid = srcAddr.nl_pid;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nlh->nlmsg_type = BOOTSTRAP_MY_PID;
	nlh->nlmsg_seq = rand();
	memcpy(NLMSG_DATA(nlh), &srcAddr.nl_pid, sizeof(srcAddr.nl_pid));
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	// Creating message
	msg.msg_name = (void *)&agentAddress;
	msg.msg_namelen = sizeof(agentAddress);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	//cout << "((MOLY)) " << __FUNCTION__ << "Connecting to agent on PID "
	//		<< (int)PID_MOLY_BOOTSTRAP_LISTENER	<< " to send my PID\n";
	ret = sendmsg(_bootstrapSocket, &msg, 0);
	if (ret == -1)
	{
		//cout << "((MOLY)) " << __FUNCTION__ << "Monitoring agent is not "
		//		"reachable yet. Trying again every second\n";
		while (true)
		{
			ret = sendmsg(_bootstrapSocket, &msg, 0);
			if (ret != -1)
			{
				break;
			}
			sleep(1);
		}
	}
	free(nlh);
	//cout << "((MOLY)) " << __FUNCTION__ << "Connected to monitoring agent. PID "
	//		<< srcAddr.nl_pid << " sent\n";
	// Now waiting and reading responses
	nlh = (struct nlmsghdr *)malloc(NLMSG_SPACE(MAX_MESSAGE_PAYLOAD));
	memset(nlh, 0, NLMSG_SPACE(MAX_MESSAGE_PAYLOAD));
	iov.iov_base = (void*)nlh;
	iov.iov_len = NLMSG_SPACE(MAX_MESSAGE_PAYLOAD);
	// Now waiting for intial OK
	while (true)
	{
		bytesRead = recvmsg(_bootstrapSocket, &msg, 0);
		if (bytesRead <= 0)
		{
			//cout << "((MOLY)) " << __FUNCTION__ << "Trigger socket crashed"
			//		<< strerror(errno) << endl;
			close(_bootstrapSocket);
			free(nlh);
			return false;
		}
		switch (nlh->nlmsg_type)
		{
		case BOOTSTRAP_OK:
			//cout << "((MOLY)) " << __FUNCTION__ << "Monitoring agent confirmed "
			//		"connection\n";
			//cout << "((MOLY)) " << __FUNCTION__ << "Waiting for start reporting "
			//		"trigger from monitoring agent on PID " << nlh->nlmsg_pid
			//		<< endl;
			break;
		case BOOTSTRAP_START_REPORTING:
			//cout << "((MOLY)) " << __FUNCTION__ << "Start reporting trigger "
			//		"received\n";
			close(_bootstrapSocket);
			free(nlh);
			return true;
		case BOOTSTRAP_ERROR:
			//cout << "((MOLY)) " << __FUNCTION__ << "Connection to agent "
			//		"established but boostrapping failed\n";
			close(_bootstrapSocket);
			free(nlh);
			return false;
		//default:
			//cout << "((MOLY)) " << __FUNCTION__ << "Unknown message type "
			//<< nlh->nlmsg_type << endl;
		}
	}
	close(_bootstrapSocket);
	free(nlh);
	return false;
}

bool Application::_send(uint8_t messageType, void *data, uint32_t dataSize)
{
	struct msghdr msg;
	int ret;
	struct iovec iov;
	struct nlmsghdr *nlh = NULL;
	memset(&msg, 0, sizeof(msg));
	/* Allocate the required memory (NLH + payload) */
	nlh=(struct nlmsghdr *) malloc(NLMSG_SPACE(dataSize));
	/* Fill the netlink message header */
	nlh->nlmsg_len = NLMSG_SPACE(dataSize);
	nlh->nlmsg_pid = _pid;
	nlh->nlmsg_flags = NLM_F_REQUEST;
	nlh->nlmsg_type = messageType;
	nlh->nlmsg_seq = rand();
	memcpy(NLMSG_DATA(nlh), data, dataSize);
	iov.iov_base = (void *)nlh;
	iov.iov_len = nlh->nlmsg_len;
	// Creating message and send it off
	msg.msg_name = (void *)&_agentSocketAddress;
	msg.msg_namelen = sizeof(_agentSocketAddress);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;
	_reportingSocketMutex.lock();
	ret = sendmsg(_reportingSocket, &msg, 0);
	if (ret < 0)
	{
		_reportingSocketMutex.unlock();
		//cout << "((MOLY)) " << __FUNCTION__ << "Could not send off data to the "
		//		"monitoring agent: " << strerror(errno) << endl;
		free(nlh);
		return false;
	}
	_reportingSocketMutex.unlock();
	free(nlh);
	return true;
}
