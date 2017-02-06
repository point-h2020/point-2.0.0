/*
 * server.cc
 *
 *  Created on: 13 Jan 2016
 *      Author: Sebastian Robitzsch <sebastian.robitzsch@interdigital.com>
 *      		Mays Al-Naday <mfhaln@essex.ac.uk>
 *
 * This file is part of the Monitoring LibrarY (MOLY).
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

#include <moly/moly.hh>
#include <iostream>
using namespace std;
int main()
{
	Moly moly;
	uint8_t messageType;
	uint8_t *data = (uint8_t *)malloc(65535);
	uint32_t dataLength;
	if (!moly.initialiseListener())
			return EXIT_FAILURE;
	cout << "Server started\n";
	for ( ; ; )
	{
		if (moly.Agent::receive(messageType, data, dataLength))
		{
			switch (messageType)
			{
            //This will be changed back to ADD_LINK, once a global enum.h and typedefs.h is created for all the platform, at the moment this clashes with src/helper.h
			case PRIMITIVE_TYPE_ADD_LINK:
			{
				cout << "ADD_LINK received:\n";
				AddLink addLink(data, dataLength);
				cout << "\tLink name:   " << addLink.name() << endl;
				cout << "\tSRC Node ID: " << addLink.sourceNodeId() << endl;
				cout << "\tDST Node ID: " << addLink.destinationNodeId()
						<< endl;
				cout << "\tLink ID:     " << addLink.linkId() << endl;
				cout << "\tLink Type:   " << (int)addLink.linkType() << endl;
				break;
			}
			case PRIMITIVE_TYPE_ADD_NODE:
			{
				cout << "ADD_NODE received:\n";
				AddNode addNode(data, dataLength);
				cout << "\tName:    " << addNode.name() << endl;
				cout << "\tNode ID: " << addNode.nodeId() << endl;
				cout << "\tType:    " << (int)addNode.nodeType() << endl;
				break;
			}
			case PRIMITIVE_TYPE_ADD_NODE_TYPE:
			{
				cout << "ADD_NODE_TYPE received:\n";
				AddNodeType addNodeType(data, dataLength);
				cout << "\tNode name: " << addNodeType.name() << endl;
				cout << "\tNode type: " << (int)addNodeType.nodeType() << endl;
				break;
			}
			case PRIMITIVE_TYPE_UPDATE_LINK_STATE:
			{
				cout << "UPDATE_LINK_STATE received:\n";
				UpdateLinkState updateLinkState(data, dataLength);
				cout << "\tDST Node ID: " << updateLinkState.destinationNodeId()
						<< endl;
				cout << "\tLink ID:     " << updateLinkState.linkId() << endl;
				cout << "\tLink Type:   " << (int)updateLinkState.linkType()
						<< endl;
				cout << "\tState:       " << (int)updateLinkState.state()
						<< endl;
				break;
			}
			default:
				cout << "Unknown message type\n";
			}
		}
	}
	return EXIT_SUCCESS;
}
