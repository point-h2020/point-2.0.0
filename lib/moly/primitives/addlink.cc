/*
 * addlink.cc
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

#include "addlink.hh"
#include <iomanip>
#include <iostream>
#include <sstream>
AddLink::AddLink(string name, string linkId, uint32_t sourceNodeId,
		uint32_t destinationNodeId, uint8_t linkType)
	: _name(name),
	  _linkId(linkId),
	  _sourceNodeId(sourceNodeId),
	  _destinationNodeId(destinationNodeId),
	  _linkType(linkType)
{
	_size = sizeof(uint32_t) /* Length of name*/ +
			name.length() /* Name */ +
			sizeof(uint32_t) /* Link ID */ +
			sizeof(uint32_t) /* Source Node ID*/ +
			sizeof(uint32_t) /* Destination Node ID */ +
			sizeof(uint8_t) /* Link Type */;
	_pointer = (uint8_t *)malloc(_size);
	hashLinkId();
	_composePacket();
}

AddLink::AddLink(uint8_t * pointer, size_t size)
	: _size(size)
{
	_pointer = (uint8_t *)malloc(size);
	memcpy(_pointer, pointer, size);
	_decomposePacket();
}

AddLink::~AddLink()
{
	free(_pointer);
}

uint32_t AddLink::destinationNodeId()
{
	return _destinationNodeId;
}

uint32_t AddLink::linkId()
{
	return _linkIdHashed;
}

uint8_t AddLink::linkType()
{
	return _linkType;
}

string AddLink::name()
{
	return _name;
}

uint8_t * AddLink::pointer()
{
	return _pointer;
}

string AddLink::print()
{
	ostringstream oss;
	oss << "| Link name: " << name();
	oss << " | Link ID: " << linkId();
	oss << " | SRC Node ID: " << (int)sourceNodeId();
	oss << " | DST Node ID: " << (int)destinationNodeId();
	oss << " | Link type: ";
	switch (linkType())
	{
	case LINK_TYPE_802_3:
		oss << "802.3";
		break;
	case LINK_TYPE_802_11:
		oss << "802.11";
		break;
	case LINK_TYPE_802_11_A:
		oss << "802.11a";
		break;
	case LINK_TYPE_802_11_B:
		oss << "802.11b";
		break;
	case LINK_TYPE_802_11_G:
		oss << "802.11g";
		break;
	case LINK_TYPE_802_11_N:
		oss << "802.11n";
		break;
	case LINK_TYPE_802_11_AA:
		oss << "802.11aa";
		break;
	case LINK_TYPE_802_11_AC:
		oss << "802.11ac";
		break;
	case LINK_TYPE_SDN_802_3_Z:
		oss << "802.3z";
		break;
	case LINK_TYPE_SDN_802_3_AE:
		oss << "802.3.ae";
		break;
	case LINK_TYPE_GPRS:
		oss << "GPRS";
		break;
	case LINK_TYPE_UMTS:
		oss << "UMTS";
		break;
	case LINK_TYPE_LTE:
		oss << "LTE";
		break;
	case LINK_TYPE_LTE_A:
		oss << "LTE-A";
		break;
	case LINK_TYPE_OPTICAL:
		oss << "Optical";
		break;
	case LINK_TYPE_UNKNOWN:
		oss << "Unknown";
		break;
	default:
		oss << "-";
	}
	oss << " |";
	return oss.str();
}

size_t AddLink::size()
{
	return _size;
}

uint32_t AddLink::sourceNodeId()
{
	return _sourceNodeId;
}

void AddLink::_composePacket()
{
	uint8_t * bufferPointer = _pointer;
	uint32_t nameLength = _name.length();
	// [1] Length
	memcpy(bufferPointer, &nameLength, sizeof(uint32_t));
	bufferPointer += sizeof(nameLength);
	// [2] Name
	memcpy(bufferPointer, _name.c_str(), nameLength);
	bufferPointer += nameLength;
	// [3] Link ID
	memcpy(bufferPointer, &_linkIdHashed, sizeof(uint32_t));
	bufferPointer += sizeof(uint32_t);
	// [4] SRC Node ID
	memcpy(bufferPointer, &_sourceNodeId, sizeof(uint32_t));
	bufferPointer += sizeof(uint32_t);
	// [5] DST Node ID
	memcpy(bufferPointer, &_destinationNodeId, sizeof(uint32_t));
	bufferPointer += sizeof(uint32_t);
	// [6] Type
	memcpy(bufferPointer, &_linkType, sizeof(uint8_t));
}

void AddLink::_decomposePacket()
{
	uint8_t * bufferPointer = _pointer;
	uint32_t nameLength;
	// [1] Length
	memcpy(&nameLength, bufferPointer, sizeof(uint32_t));
	bufferPointer += sizeof(uint32_t);
	// [2] Name
	char name[nameLength + 1];
	memcpy(name, bufferPointer, nameLength);
	name[nameLength] = '\0';
	_name.assign(name);
	bufferPointer += nameLength;
	// [3] Link ID
	memcpy(&_linkIdHashed, bufferPointer, sizeof(uint32_t));
	bufferPointer += sizeof(uint32_t);
	// [4] SRC Node ID
	memcpy(&_sourceNodeId, bufferPointer, sizeof(uint32_t));
	bufferPointer += sizeof(uint32_t);
	// [5] DST Node ID
	memcpy(&_destinationNodeId, bufferPointer, sizeof(uint32_t));
	bufferPointer += sizeof(uint32_t);
	// [6] Type
	memcpy(&_linkType, bufferPointer, sizeof(uint8_t));
}

void AddLink::hashLinkId()
{
	_linkIdHashed = 0;
	const char *string = _linkId.c_str();
	while (*string != 0)
	{
		_linkIdHashed = _linkIdHashed * 31 + *string++;
	}
}
