/*
 * statistics.cc
 *
 *  Created on: Dec 17, 2015
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

#include "statistics.hh"

Statistics::Statistics(Namespace &namespaceHelper)
	: _namespaceHelper(namespaceHelper)
{}

Statistics::~Statistics(){}

void Statistics::cmcGroupSize(uint32_t groupSize)
{
	string scopePath;
	uint32_t epoch = time(0);
	scopePath = _namespaceHelper.getScopePath(NODE_TYPE_NAP, II_CMC_GROUP_SIZE);
	uint32_t dataLength = sizeof(epoch) + sizeof(uint32_t);
	uint8_t *data = (uint8_t *)malloc(dataLength);
	// [1] EPOCH
	memcpy(data, &epoch, sizeof(epoch));
	// [2] group size
	memcpy(data + sizeof(epoch), &groupSize, sizeof(uint32_t));
	_namespaceHelper.prepareDataToBePublished(scopePath, data, dataLength);
	free(data);
}

void Statistics::transmittedBytes(LINK_ID linkId, NODE_ID destinationNodeId,
		LINK_TYPE linkType, TX_BYTES transmittedBytes)
{
	string scopePath;
	uint32_t epoch = time(0);
	scopePath = _namespaceHelper.getScopePath(linkId, destinationNodeId,
			linkType, II_TRANSMITTED_BYTES);
	size_t dataLength = sizeof(epoch) + sizeof(TX_BYTES);
	uint8_t *data = (uint8_t *)malloc(dataLength);
	// [1] EPOCH
	memcpy(data, &epoch, sizeof(epoch));
	// [2] Transmitted bytes
	memcpy(data + sizeof(epoch), &transmittedBytes,	sizeof(TX_BYTES));
	_namespaceHelper.prepareDataToBePublished(scopePath, data, dataLength);
	free(data);
}

void Statistics::httpRequestsPerFqdn(string fqdn, uint32_t numberOfRequests)
{
	string scopePath;
	uint32_t fqdnLength;
	uint32_t epoch = time(0);
	scopePath = _namespaceHelper.getScopePath(NODE_TYPE_NAP,
			II_HTTP_REQUESTS_PER_FQDN);
	size_t dataLength = sizeof(epoch) +	sizeof(fqdnLength) + fqdn.length() +
			sizeof(uint32_t) /*number of requests*/;
	uint8_t *data = (uint8_t *)malloc(dataLength);
	uint8_t offset = 0;
	// [1] EPOCH
	memcpy(data, &epoch, sizeof(epoch));
	offset += sizeof(epoch);
	// [2] FQDN length
	fqdnLength = fqdn.length();
	memcpy(data + offset, &fqdnLength, sizeof(fqdnLength));
	offset += sizeof(fqdnLength);
	// [3] FQDN
	memcpy(data + offset, fqdn.c_str(), fqdn.length());
	offset += fqdn.length();
	// [4] # of HTTP requests
	memcpy(data + offset, &numberOfRequests, sizeof(uint32_t));
	_namespaceHelper.prepareDataToBePublished(scopePath, data, dataLength);
	free(data);
}

void Statistics::networkLatencyPerFqdn(uint32_t fqdn, uint16_t networkLatency)
{
	string scopePath;
	uint32_t epoch = time(0);
	scopePath = _namespaceHelper.getScopePath(NODE_TYPE_NAP,
			II_NETWORK_LATENCY_PER_FQDN);
	size_t dataLength = sizeof(epoch) + sizeof(fqdn) + sizeof(networkLatency);
	uint8_t *data = (uint8_t *)malloc(dataLength);
	uint8_t offset = 0;
	// [1] EPOCH
	memcpy(data, &epoch, sizeof(epoch));
	offset += sizeof(uint32_t);
	// [2] hashed FQDN
	memcpy(data + offset, &fqdn, sizeof(fqdn));
	offset += sizeof(fqdn);
	// [3] Network latency
	memcpy(data + offset, &networkLatency, sizeof(networkLatency));
	_namespaceHelper.prepareDataToBePublished(scopePath, data, dataLength);
	free(data);
}
