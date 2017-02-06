/*
 * nodestate.cc
 *
 *  Created on: 20 Jan 2017
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

#include "nodestate.hh"

NodeState::NodeState(uint8_t nodeType, uint32_t nodeId, uint8_t state)
	: _nodeType(nodeType),
	  _nodeId(nodeId),
	  _state(state)
{
	_size = sizeof(_nodeType) + sizeof(_nodeId) + sizeof(_state);
	_pointer = (uint8_t *)malloc(_size);
	_composePacket();
}

NodeState::NodeState(uint8_t * pointer, size_t size)
	: _size(size)
{
	_pointer = (uint8_t *)malloc(size);
	memcpy(_pointer, pointer, size);
	_decomposePacket();
}

NodeState::~NodeState()
{
	free(_pointer);
}

uint32_t NodeState::nodeId()
{
	return _nodeId;
}

uint8_t NodeState::nodeType()
{
	return _nodeType;
}

uint8_t * NodeState::pointer()
{
	return _pointer;
}

string NodeState::print()
{
	ostringstream oss;
	oss << " | Node Type: " << nodeType();
	oss << " | NID: " << nodeId();
	oss << " | State: " << state();
	return oss.str();
}

size_t NodeState::size()
{
	return _size;
}

uint8_t NodeState::state()
{
	return _state;
}

void NodeState::_composePacket()
{
	uint8_t * bufferPointer = _pointer;
	// [1] Node type
	memcpy(bufferPointer, &_nodeType, sizeof(_nodeType));
	bufferPointer += sizeof(_nodeType);
	// [2] Node ID
	memcpy(bufferPointer, &_nodeId, sizeof(_nodeId));
	bufferPointer += sizeof(_nodeId);
	// [3] State
	memcpy(bufferPointer, &_state, sizeof(_state));
}

void NodeState::_decomposePacket()
{
	uint8_t * bufferPointer = _pointer;
	// [1] Node type
	memcpy(&_nodeType, bufferPointer, sizeof(_nodeType));
	bufferPointer += sizeof(_nodeType);
	// [2] Node ID
	memcpy(&_nodeId, bufferPointer, sizeof(_nodeId));
	bufferPointer += sizeof(_nodeId);
	// [3] State
	memcpy(&_state, bufferPointer, sizeof(_state));
}
