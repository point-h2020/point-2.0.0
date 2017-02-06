/*
 * linkstate.cc
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

#include "linkstate.hh"

LinkState::LinkState(uint32_t linkId, uint32_t destinationNodeId,
		uint8_t linkType, uint8_t state)
	: _linkId(linkId),
	  _destinationNodeId(destinationNodeId),
	  _linkType(linkType),
	  _state(state)
{
	_size = sizeof(uint32_t) /* Link ID */ +
			sizeof(uint32_t) /* Destination Node ID */+
			sizeof(uint8_t) /* Link Type */+
			sizeof(uint8_t) /* State */;
	_pointer = (uint8_t *)malloc(_size);
	_composePacket();
}

LinkState::LinkState(uint8_t * pointer, size_t size)
	: _size(size)
{
	_pointer = (uint8_t *)malloc(size);
	memcpy(_pointer, pointer, size);
	_decomposePacket();
}

LinkState::~LinkState()
{
	free(_pointer);
}

uint32_t LinkState::destinationNodeId()
{
	return _destinationNodeId;
}

uint32_t LinkState::linkId()
{
	return _linkId;
}

uint8_t LinkState::linkType()
{
	return _linkType;
}

uint8_t * LinkState::pointer()
{
	return _pointer;
}

string LinkState::print()
{
	ostringstream oss;
	oss << " | Link ID: " << linkId();
	oss << " | Link Type: " << linkType();
	oss << " | DST NID: " << destinationNodeId();
	oss << " | State: " << state();
	return oss.str();
}

size_t LinkState::size()
{
	return _size;
}

uint8_t LinkState::state()
{
	return _state;
}

void LinkState::_composePacket()
{
	uint8_t * bufferPointer = _pointer;
	// [1] Link ID
	memcpy(bufferPointer, &_linkId, sizeof(uint32_t));
	bufferPointer += sizeof(uint32_t);
	// [2] DST Node ID
	memcpy(bufferPointer, &_destinationNodeId, sizeof(uint32_t));
	bufferPointer += sizeof(uint32_t);
	// [3] Link Type
	memcpy(bufferPointer, &_linkType, sizeof(uint8_t));
	bufferPointer += sizeof(uint8_t);
	// [4] State
	memcpy(bufferPointer, &_state, sizeof(uint8_t));
}

void LinkState::_decomposePacket()
{
	uint8_t * bufferPointer = _pointer;
	// [1] Link ID
	memcpy(&_linkId, bufferPointer, sizeof(uint32_t));
	bufferPointer += sizeof(uint32_t);
	// [2] DST Node ID
	memcpy(&_destinationNodeId, bufferPointer, sizeof(uint32_t));
	bufferPointer += sizeof(uint32_t);
	// [3] Link Type
	memcpy(&_linkType, bufferPointer, sizeof(uint8_t));
	bufferPointer += sizeof(uint8_t);
	// [4] State
	memcpy(&_state, bufferPointer, sizeof(uint8_t));
}
