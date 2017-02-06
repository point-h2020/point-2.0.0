/*
 * name.cc
 *
 *  Created on: 24 Dec 2015
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

#include "name.hh"

Name::Name(Namespace &namespaceHelper)
	: _namespaceHelper(namespaceHelper)
{}

Name::~Name(){}

void Name::updateNodeName(LINK_ID linkId, NODE_ID destinationNodeId,
			LINK_TYPE linkType, string name)
{
	string scopePath;
	uint32_t dataLength;
	uint8_t offset = 0;
	uint32_t epoch = time(0);
	scopePath = _namespaceHelper.getScopePath(linkId, destinationNodeId,
			linkType, II_NAME);
	dataLength = sizeof(epoch) + sizeof(uint32_t) + name.length();
	uint8_t *data = (uint8_t *)malloc(dataLength);
	// [1] EPOCH
	memcpy(data, &epoch, sizeof(epoch));
	offset += sizeof(uint32_t);
	// [2] Name length
	uint32_t nameLength = name.length();
	memcpy(data + offset, &nameLength, nameLength);
	offset += nameLength;
	// [3] Name
	memcpy(data + offset, name.c_str(), name.length());
	_namespaceHelper.prepareDataToBePublished(scopePath, data, dataLength);
	free(data);
}

void Name::updateNodeRole(LINK_ID linkId, NODE_ID destinationNodeId,
			LINK_TYPE linkType, string role)
{
	string scopePath;
	uint32_t dataLength;
	uint8_t *data;
	uint8_t offset = 0;
	uint32_t epoch = time(0);
	scopePath = _namespaceHelper.getScopePath(linkId, destinationNodeId,
			linkType, II_ROLE);
	dataLength = sizeof(epoch) + sizeof(uint32_t) + role.length();
	data = (uint8_t *)malloc(dataLength);
	// [1] EPOCH
	memcpy(data, &epoch, sizeof(epoch));
	offset += sizeof(epoch);
	// [2] Role length
	uint32_t roleLength = role.length();
	memcpy(data + offset, &roleLength, sizeof(uint32_t));
	offset += roleLength;
	// [3] Role
	memcpy(data + offset, role.c_str(), roleLength);
	_namespaceHelper.prepareDataToBePublished(scopePath, data, dataLength);
	free(data);
}
