/*
 * state.cc
 *
 *  Created on: Nov 25, 2015
 *      Author: Sebastian Robitzsch <sebastian.robitzsch@interdigital.com>
 *
 * This file is part of BlAckadder Monitoring wraPpER clasS (BAMPERS).
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

#include "state.hh"
State::State(Namespace &namespaceHelper)
: _namespaceHelper(namespaceHelper)
{}

State::~State(){}

void State::update(uint8_t nodeType, uint32_t nodeId, STATE state)
{
	string scopePath;
	uint32_t epoch = time(0);
	scopePath = _namespaceHelper.getScopePath(nodeType, nodeId, state);
	size_t dataLength = sizeof(epoch) + sizeof(STATE);
	uint8_t *data = (uint8_t *)malloc(dataLength);
	uint8_t offset = 0;
	// [1] EPOCH
	memcpy(data, &epoch, sizeof(epoch));
	offset += sizeof(epoch);
	// [2] State
	memcpy(data + offset, &state, sizeof(STATE));
	offset += sizeof(STATE);
	_namespaceHelper.prepareDataToBePublished(scopePath, data, dataLength);
	free(data);
}

void State::update(uint8_t nodeType, string hostname, STATE state)
{
	string scopePath;
	uint32_t hostnameLength;
	uint32_t epoch = time(0);
	scopePath = _namespaceHelper.getScopePath(nodeType, state);
	size_t dataLength = sizeof(epoch) + sizeof(STATE) + sizeof(hostnameLength)
			+ hostname.length();
	uint8_t *data = (uint8_t *)malloc(dataLength);
	uint8_t offset = 0;
	// [1] EPOCH
	memcpy(data, &epoch, sizeof(epoch));
	offset += sizeof(epoch);
	// [2] State
	memcpy(data + offset, &state, sizeof(STATE));
	offset += sizeof(STATE);
	// [3] Hostname length
	hostnameLength = hostname.length();
	memcpy(data + offset, &hostnameLength, sizeof(hostnameLength));
	offset += sizeof(hostnameLength);
	// [4] Hostname
	memcpy(data + offset, &hostname, hostnameLength);
	_namespaceHelper.prepareDataToBePublished(scopePath, data, dataLength);
	free(data);
}

void State::update(LINK_ID linkId, NODE_ID destinationNodeId,
		LINK_TYPE linkType, STATE state)
{
	string scopePath;
	uint8_t offset = 0;
	uint32_t epoch = time(0);
	scopePath = _namespaceHelper.getScopePath(linkId, destinationNodeId,
			linkType, II_STATE);
	size_t dataLength = sizeof(epoch) + sizeof(STATE);
	uint8_t *data = (uint8_t *)malloc(dataLength);
	// [1] EPOCH
	memcpy(data, &epoch, sizeof(epoch));
	offset += sizeof(epoch);
	// [2] State
	memcpy(data + offset, &state, sizeof(STATE));
	_namespaceHelper.prepareDataToBePublished(scopePath, data, dataLength);
	free(data);
}
