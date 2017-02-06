/*
 * cidanalyser.cc
 *
 *  Created on: 03 June 2016
 *      Author: Sebastian Robitzsch <sebastian.robitzsch@interdigital.com>
 *
 * This file is part of BAMPERS.
 *
 * BAMPERS is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * BAMPERS is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * BAMPERS. If not, see <http://www.gnu.org/licenses/>.
 */

#include "cidanalyser.hh"
#include "enum.hh"

CIdAnalyser::CIdAnalyser()
{
	_linkId = string();
	_name = string();
	_linkType = LINK_TYPE_UNKNOWN;
	_destinationNodeId = 0;
	_state = STATE_UNKNOWN;
	_nodeType = 0;
	_nodeId = 0;
}

CIdAnalyser::CIdAnalyser(IcnId icnId)
	: _icnId(icnId)
{
	_linkId = string();
	_name = string();
	_linkType = LINK_TYPE_UNKNOWN;
	_destinationNodeId = 0;
	_state = STATE_UNKNOWN;
	if (_icnId.str().length() >= (3 * ID_LEN))
	{
		_nodeType = (uint8_t)
				atoi(_icnId.str().substr(2 * ID_LEN, ID_LEN).c_str());
	}
	else
	{
		_nodeType = 0;
	}
	_nodeId = 0;
}

CIdAnalyser::~CIdAnalyser() {}

uint32_t CIdAnalyser::destinationNodeId()
{
	return _destinationNodeId;
}

string CIdAnalyser::linkId()
{
	return _linkId;
}

bool CIdAnalyser::linksScope()
{
	if (atoi(_icnId.str().substr(ID_LEN, ID_LEN).c_str()) == TOPOLOGY_LINKS)
	{
		return true;
	}
	return false;
}

uint8_t CIdAnalyser::linkType()
{
	return _linkType;
}

uint32_t CIdAnalyser::nodeId()
{
	return _nodeId;
}

bool CIdAnalyser::nodesScope()
{
	if (atoi(_icnId.str().substr(ID_LEN, ID_LEN).c_str()) == TOPOLOGY_NODES)
	{
		return true;
	}
	return false;
}

uint8_t CIdAnalyser::nodeType()
{
	return _nodeType;
}

uint8_t CIdAnalyser::primitive()
{
	 /* primitive enumeration defined in MOLY */
	uint8_t primitive = PRIMITIVE_TYPE_UNKNOWN;
	switch (_scopeTypeLevel1())
	{
	case TOPOLOGY_NODES:
		//cout << "Scope level 1 = TOPOLOGY_NODES (0)\n";
		_nodeType = atoi(_icnId.str().substr(2 * 16, 16).c_str());
		//cout << "_nodeType = " << (int)_nodeType << endl;
		_nodeId = atoi(_icnId.str().substr(3 * 16, 16).c_str());
		//cout << "_nodeId = " << (int)_nodeId << endl;
		break;
	case TOPOLOGY_LINKS:
	{
		//cout << "Scope level 1 = TOPOLOGY_LINKS (1)\n";
		string linkIdString = _icnId.str().substr(2 * 16, 16);
		_linkId = linkIdString.erase(0, linkIdString.find_first_not_of('0'));
		//cout << "_linkId = " << _linkId << endl;
		_destinationNodeId = atoi(_icnId.str().substr(3 * 16, 16).c_str());
		//cout << "_destinationNodeId = " << (int)_destinationNodeId << endl;
		_linkType = atoi(_icnId.str().substr(4 * 16, 16).c_str());
		//cout << "_linkType = " << (int)_linkType << endl;
		break;
	}
	default:
		cout << "Unknown scope type level 1: " << _scopeTypeLevel1() << endl;
	}
	/* Note the switch statement below is entirely for debugging purposes. If
	 * not desired simply replace it by:
	 *
	 * 		primitive = _informationItem();
	 */
	switch(_informationItem())
	{
	case II_FQDN:
		//cout << "II = II_FQDN\n";
		break;
	case II_HTTP_REQUESTS_PER_FQDN:
		//cout << "II = II_HTTP_REQUESTS_PER_FQDN\n";
		primitive = PRIMITIVE_TYPE_HTTP_REQUESTS_PER_FQDN;
		break;
	case II_NAME:
		primitive = PRIMITIVE_TYPE_ADD_NODE;
		//cout << "II = II_NAME\n";
		break;
	case II_NODE_ID:
		//cout << "primitive = PRIMITIVE_TYPE_ADD_NODE, II = II_NODE_ID\n";
		break;
	case II_NUMBER_OF_STATES:
		//cout << "II = II_NUMBER_OF_STATES\n";
		break;
	case II_PREFIX:
		//cout << "II = II_PREFIX\n";
		break;
	case II_ROLE:
		//cout << "II = II_ROLE\n";
		break;
	case II_SOURCE_NODE_ID:
		_state = STATE_UP;
		primitive = PRIMITIVE_TYPE_ADD_LINK;
		//cout << "II = II_SOURCE_NODE_ID\n";
		break;
	case II_STATE:
		//cout << "II = II_STATE\n";
		break;
	case II_TRANSMITTED_BYTES:
		//cout << "II = II_TRANSMITTED_BYTES\n";
		break;
	case II_CMC_GROUP_SIZE:
		//cout << "II = II_CMC_GROUP_SIZE\n";
		primitive = PRIMITIVE_TYPE_CMC_GROUP_SIZE;
		break;
	case II_NETWORK_LATENCY_PER_FQDN:
		//cout << "II = II_NETWORK_LATENCY_PER_FQDN" << endl;
		primitive = PRIMITIVE_TYPE_NETWORK_LATENCY_PER_FQDN;
		break;
	default:
		cout << "ERROR: Unknown information item: " << _informationItem()
		<< endl;
	}
	return primitive;
}

uint8_t CIdAnalyser::state()
{
	return _state;
}

bool CIdAnalyser::statisticsScope()
{
	if (atoi(_icnId.str().substr(ID_LEN, ID_LEN).c_str()) == TOPOLOGY_NODES)
	{
		return true;
	}
	return false;
}

uint16_t CIdAnalyser::_informationItem()
{
	return atoi(_icnId.str().substr(_icnId.str().length() - 16, 16).c_str());
}

uint8_t CIdAnalyser::_scopeTypeLevel1()
{
	return atoi(_icnId.str().substr(16, 16).c_str());
}
