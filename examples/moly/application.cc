/*
 * client.cc
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

//#include <blackadderenum.hh>
#include <moly/moly.hh>
#include <iostream>
using namespace std;
int main()
{
	Moly moly;
	// blocking method. returns
	//if (!moly.Application::startReporting())
	//{
	//	cout << "Communication with monitoring agent failed ... exiting\n";
	//	return EXIT_FAILURE;
	//}
	// ADD_NODE
	if (!moly.Application::addNode("TM", 120, NODE_TYPE_TM))
	{
		cout << "Sending ADD_NODE failed\n";
	}
	else
	{
		cout << "ADD_NODE(\"TM\", 120, " << NODE_TYPE_TM << ") sent\n";
	}
	moly.Application::updateCmcGroupSize(5);

	// ADD_LINK
	if (!moly.Application::addLink("Ethernet", "123", 20, 25, LINK_TYPE_802_3))
	{
		cout << "Sending ADD_LINK failed\n";
	}
	else
	{
		cout << "ADD_LINK(\"Ethernet\", \"123\", 20, 25, " << LINK_TYPE_802_3
				<< ") sent\n";
	}
	// ADD_NODE
	if (!moly.Application::addNode("TM", 120, NODE_TYPE_TM))
	{
		cout << "Sending ADD_NODE failed\n";
	}
	else
	{
		cout << "ADD_NODE(\"TM\", 120, " << NODE_TYPE_TM << ") sent\n";
	}
	return 0;
	// ADD_NODE_TYPE
	if (!moly.addNodeType("NAP IDE", NODE_TYPE_NAP))
	{
		cout << "Sending ADD_NODE_TYPE failed\n";
	}
	else
	{
		cout << "ADD_NODE_TYPE(\"NAP IDE\", " << NODE_TYPE_NAP << ") sent\n";
	}
	return 0;
	// UPDATE_LINK_STATE
	if (!moly.updateLinkState(2, 1, LINK_TYPE_802_3, STATE_UP))
	{
		cout << "Sending UPDATE_LINK_STATE failed\n";
	}
	else
	{
		cout << "UPDATE_LINK_STATE sent\n";
	}
	return EXIT_SUCCESS;
}
