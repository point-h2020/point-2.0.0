/*
 * addnode.cc
 *
 *  Created on: 13 Jan 2016
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

#include "addnode.hh"
#include <sstream>
AddNode::AddNode(string name, uint32_t nodeId, uint8_t nodeType)
	: _name(name),
	  _nodeId(nodeId),
	  _nodeType(nodeType)
{
	_size = sizeof(uint32_t) + name.size() + sizeof(uint32_t) + sizeof(uint8_t);
	_pointer = (uint8_t *)malloc(_size);
	_composePacket();
}

AddNode::AddNode(uint8_t * pointer, size_t size)
	: _size(size)
{
	_pointer = (uint8_t *)malloc(size);
	memcpy(_pointer, pointer, size);
	_decomposePacket();
}

AddNode::~AddNode()
{
	free(_pointer);
}

string AddNode::name()
{
	return _name;
}

uint32_t AddNode::nodeId()
{
	return _nodeId;
}

uint8_t * AddNode::pointer()
{
	return _pointer;
}

string AddNode::print()
{
	ostringstream oss;
	oss << "| Node name: " << name();
	oss << " | Node ID: " << (int)nodeId();
	oss << " | Node type: ";
	switch (nodeType())
	{
	case NODE_TYPE_GW:
		oss << "NODE_TYPE_GW";
		break;
	case NODE_TYPE_FN:
		oss << "NODE_TYPE_FN";
		break;
	case NODE_TYPE_NAP:
		oss << "NODE_TYPE_NAP";
		break;
	case NODE_TYPE_RV:
		oss << "NODE_TYPE_RV";
		break;
	case NODE_TYPE_SERVER:
		oss << "NODE_TYPE_SERVER";
		break;
	case NODE_TYPE_TM:
		oss << "NODE_TYPE_TM";
		break;
	case NODE_TYPE_UE:
		oss << "NODE_TYPE_UE";
		break;
	default:
		oss << "-";
	}
	oss << " |";
	return oss.str();
}

size_t AddNode::size()
{
	return _size;
}

uint8_t AddNode::nodeType()
{
	return _nodeType;
}

void AddNode::_composePacket()
{
	uint8_t * bufferPointer = _pointer;
	uint32_t nameLength = _name.length();
	// [1] Length of name
	memcpy(bufferPointer, &nameLength, sizeof(uint32_t));
	bufferPointer += sizeof(uint32_t);
	// [2] Name
	memcpy(bufferPointer, _name.c_str(), nameLength);
	bufferPointer += nameLength;
	// [3] Node ID
	memcpy(bufferPointer, &_nodeId, sizeof(uint32_t));
	bufferPointer += sizeof(uint32_t);
	// [4] Node Type
	memcpy(bufferPointer, &_nodeType, sizeof(uint8_t));
}

void AddNode::_decomposePacket()
{
	uint8_t * bufferPointer = _pointer;
	uint32_t nameLength;
	// [1] Length of name
	memcpy(&nameLength, bufferPointer, sizeof(uint32_t));
	bufferPointer += sizeof(uint32_t);
	// [2] Name
	char name[nameLength + 1];
	memcpy(name, bufferPointer, nameLength);
	name[nameLength] = '\0';
	_name.assign(name);
	bufferPointer += nameLength;
	// [3] Node ID
	memcpy(&_nodeId, bufferPointer, sizeof(uint32_t));
	bufferPointer += sizeof(uint32_t);
	// [4] Node Type
	memcpy(&_nodeType, bufferPointer, sizeof(uint8_t));
}
