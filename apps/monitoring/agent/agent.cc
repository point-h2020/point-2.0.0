/**
 * agent.cc
 *
 * Created on: Nov 25, 2015
 *      Author: Sebastian Robitzsch <sebastian.robitzsch@interdigital.com>
 *
 * This file is part of the ICN application MOnitOring Agent (MONA) which
 * comes with Blackadder.
 *
 * MONA is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * MONA is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * MONA. If not, see <http://www.gnu.org/licenses/>.
 */
#include <blackadder.hpp>
#include <bampers/bampers.hh>
#include <boost/thread.hpp>
#include <moly/moly.hh>
#include <signal.h>

//#define MONITORING_TRIGGER_BUGFIX

Blackadder *ba;
uint8_t *data = (uint8_t *)malloc(65535);

void sigfun(int sig) {
	(void) signal(sig, SIG_DFL);
	cout << "Disconnecting from Blackadder\n";
	ba->disconnect();
	free(data);
	delete ba;
	exit(0);
}
#ifdef MONITORING_TRIGGER_BUGFIX
int main (int argc, char* argv[])
#else
int main ()
#endif
{
	Moly moly;
#ifdef MONITORING_TRIGGER_BUGFIX
	int sleepTime = 30;
#endif
	if (getuid() != 0)
	{
		cout << "This application must run with root (sudo) privileges\n";
		return EXIT_FAILURE;
	}
#ifdef MONITORING_TRIGGER_BUGFIX
	if (argc > 1)
	{
		sleepTime = atoi(argv[1]);
	}
	cout << "Sleep for " << sleepTime << " seconds before telling all "
			"applications to start reporting\n";
	sleep(sleepTime);
#endif
	if (!moly.Agent::initialiseListener())
	{
		cout << "Initialising MOLY failed\n";
		return EXIT_FAILURE;
	}
	cout << "MOLY initialised\n";
	ba = Blackadder::Instance(true);
	(void) signal(SIGINT, sigfun);
	cout << "Blackadder instantiated\n";
	uint8_t messageType;
	uint32_t dataLength;
	// Initialise namespace for BAMPERS
	Namespace namespaceHelper(ba);
	// Initialise agent with the NodeID
	Bampers monitoringAgent(namespaceHelper);
	// Start listening for messages
	cout << "Start listening for monitoring data from applications\n";
	for ( ; ; )
	{
		if (moly.Agent::receive(messageType, data, dataLength))
		{
			switch (messageType)
			{
			case PRIMITIVE_TYPE_ADD_LINK:
			{
				AddLink addLink(data, dataLength);
				cout << "ADD_LINK received of length " << dataLength << " ";
				cout << addLink.print() << endl;
				if (addLink.sourceNodeId() == 0 ||
						addLink.destinationNodeId() == 0)
				{
					cout << "Node ID must not be 0. Discarding message\n";
					break;
				}
				monitoringAgent.Link::add(addLink.linkId(),
						addLink.sourceNodeId(),	addLink.destinationNodeId(),
						addLink.linkType());
				break;
			}
			case PRIMITIVE_TYPE_ADD_NODE:
			{
				AddNode addNode(data, dataLength);
				cout << "ADD_NODE received of length " << dataLength;
				cout << addNode.print() << endl;
				if (addNode.nodeId() == 0)
				{
					cout << "Node ID must not be 0. Discarding message\n";
					break;
				}
				namespaceHelper.nodeId(addNode.nodeId());
				monitoringAgent.Node::add(addNode.nodeType(), addNode.name(),
						addNode.nodeId());
				break;
			}
			case PRIMITIVE_TYPE_ADD_NODE_TYPE:
			{
				AddNodeType addNodeType(data, dataLength);
				cout << "ADD_NODE_TYPE received: " << addNodeType.print()
						<< endl;
				if (namespaceHelper.nodeId() == 0)
				{
					cout << "Cannot send off ADD_NODE_TYPE. Node ID is still 0"
							<< endl;
				}
				else
				{
					monitoringAgent.Node::add(addNodeType.nodeType(),
							addNodeType.name(), namespaceHelper.nodeId());
				}
				break;
			}
			case PRIMITIVE_TYPE_CMC_GROUP_SIZE:
			{
				cout << "CMC_GROUP_SIZE received: ";
				CmcGroupSize cmcGroupSize(data, dataLength);
				cout << cmcGroupSize.print() << endl;
				if (namespaceHelper.nodeId() == 0)
				{
					cout << "Cannot send off UPDATE_CMC_GROUP_SIZE. Node ID is "
							"still 0" << endl;
				}
				else
				{
					monitoringAgent.Statistics::cmcGroupSize(
							cmcGroupSize.groupSize());
				}
				break;
			}
			case PRIMITIVE_TYPE_HTTP_REQUESTS_PER_FQDN:
			{
				cout << "PRIMITIVE_TYPE_HTTP_REQUESTS_PER_FQDN received: ";
				HttpRequestsPerFqdn httpRequestsPerFqdn(data, dataLength);
				cout << httpRequestsPerFqdn.print() << endl;
				monitoringAgent.Statistics::httpRequestsPerFqdn(
						httpRequestsPerFqdn.fqdn(),
						httpRequestsPerFqdn.httpRequests());
				break;
			}
			case PRIMITIVE_TYPE_LINK_STATE:
			{
				cout << "LINK_STATE received: ";
				LinkState linkState(data, dataLength);
				cout << linkState.print() << endl;
				monitoringAgent.State::update(linkState.linkId(),
						linkState.destinationNodeId(),
						linkState.linkType(), linkState.state());
				break;
			}
			case PRIMITIVE_TYPE_NETWORK_LATENCY_PER_FQDN:
			{
				cout << "PRIMITIVE_TYPE_NETWORK_LATENCY_PER_FQDN received: ";
				NetworkLatencyPerFqdn networkLatencyPerFqdn(data, dataLength);
				cout << networkLatencyPerFqdn.print() << endl;
				monitoringAgent.Statistics::networkLatencyPerFqdn(
						networkLatencyPerFqdn.fqdn(),
						networkLatencyPerFqdn.latency());
				break;
			}
			case PRIMITIVE_TYPE_NODE_STATE:
			{
				cout << "NODE_STATE received: ";
				NodeState nodeState(data, dataLength);
				cout << nodeState.print() << endl;
				monitoringAgent.State::update(nodeState.nodeType(),
						nodeState.nodeId(), nodeState.state());
				break;
			}
			default:
				cout << "Unknown message type\n";
			}
		}
	}
	free(data);
	delete ba;
	return 0;
}
