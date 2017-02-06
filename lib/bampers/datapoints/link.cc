/*
 * link.cc
 *
 *  Created on: 19 Dec 2015
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

#include "link.hh"

Link::Link(Namespace &namespaceHelper)
	: _namespaceHelper(namespaceHelper)
{}

Link::~Link(){}

void Link::add(LINK_ID linkId, NODE_ID sourceNodeId, NODE_ID destinationNodeId,
			LINK_TYPE linkType)
{
	string scopePath;
	uint32_t epoch = time(0);
	scopePath = _namespaceHelper.getScopePath(linkId, destinationNodeId,
			linkType, II_SOURCE_NODE_ID);
	uint32_t dataLength = sizeof(epoch) + sizeof(NODE_ID);
	uint8_t *data = (uint8_t *)malloc(dataLength);
	// [1] EPOCH
	memcpy(data, &epoch, sizeof(epoch));
	// [2] NID
	memcpy(data + sizeof(epoch), &sourceNodeId, sizeof(NODE_ID));
	_namespaceHelper.prepareDataToBePublished(scopePath, data, dataLength);
	free(data);
}

void Link::remove(LINK_ID linkId, NODE_ID sourceNodeId,
		NODE_ID destinationNodeId, LINK_TYPE linkType)
{
	string scopePath;
	uint8_t *data;
	uint32_t dataLength;
	uint32_t epoch = time(0);
	scopePath = _namespaceHelper.getScopePath(linkId, destinationNodeId,
			linkType, II_STATE);
	dataLength = sizeof(epoch) + sizeof(NODE_ID);
	data = (uint8_t *)malloc(dataLength);
	// [1] EPOCH
	memcpy(data, &epoch, sizeof(epoch));
	// [2] DST NID
	memcpy(data + sizeof(epoch), &destinationNodeId, sizeof(NODE_ID));
	_namespaceHelper.prepareDataToBePublished(scopePath, data, dataLength);
	free(data);
	_namespaceHelper.removePublishedScopePath(scopePath);
}
