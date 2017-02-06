/*
 * addnodetype.cc
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

#include "addnodetype.hh"
#include <sstream>

AddNodeType::AddNodeType(string name, uint8_t nodeType)
	: _name(name),
	  _nodeType(nodeType)
{
	_size = sizeof(uint32_t) + name.size() + sizeof(uint8_t);
	_pointer = (uint8_t *)malloc(_size);
	_composePacket();
}

AddNodeType::AddNodeType(uint8_t * pointer, size_t size)
	: _size(size)
{
	_pointer = (uint8_t *)malloc(size);
	memcpy(_pointer, pointer, size);
	_decomposePacket();
}

AddNodeType::~AddNodeType()
{
	free(_pointer);
}

uint8_t AddNodeType::nodeType()
{
	return _nodeType;
}

string AddNodeType::name()
{
	return _name;
}

uint8_t * AddNodeType::pointer()
{
	return _pointer;
}

string AddNodeType::print()
{
	ostringstream oss;
	oss << "| Node name: " << name();
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

size_t AddNodeType::size()
{
	return _size;
}

void AddNodeType::_composePacket()
{
	uint8_t * bufferPointer = _pointer;
	uint32_t nameLength = _name.length();
	// [1] Name Length
	memcpy(bufferPointer, &nameLength, sizeof(uint32_t));
	bufferPointer += sizeof(uint32_t);
	// [2] Name
	memcpy(bufferPointer, _name.c_str(), nameLength);
	bufferPointer += nameLength;
	// [3] Node Type
	memcpy(bufferPointer, &_nodeType, sizeof(uint8_t));
}

void AddNodeType::_decomposePacket()
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
	// [3] Node Type
	memcpy(&_nodeType, bufferPointer, sizeof(uint8_t));
}
