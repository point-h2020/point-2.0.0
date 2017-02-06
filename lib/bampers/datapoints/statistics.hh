/*
 * statistics.hh
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

#ifndef LIB_BAMPERS_DATAPOINTS_STATISTICS_HH_
#define LIB_BAMPERS_DATAPOINTS_STATISTICS_HH_

#include "../namespace.hh"

class Statistics {
public:
	Statistics(Namespace &namespaceHelper);
	virtual ~Statistics();
	/*!
	 * \brief Send coincidental multicast group size under
	 * /topology/nodes/nap/nodeId/cmcGrouSize
	 *
	 * \param groupSize The averaged group size over an interval
	 *
	 */
	void cmcGroupSize(uint32_t groupSize);
	/*!
	 * \brief TODO
	 */
	void httpRequestsPerFqdn(string fqdn, uint32_t numberOfRequests);
	/*!
	 * \brief Send the average network latency over the last reporting period
	 * under /topology/nodes/nap/nodeId/networkLatency
	 *
	 * \param networkLatency The average network latency over the last reporting
	 * interval
	 */
	void networkLatencyPerFqdn(uint32_t fqdn, uint16_t networkLatency);
	/*!
	 * \brief Send transmitted bytes under /Topology/Links/LID/NodeId/TxBytes
	 *
	 * This method enables an ICN application to publish transmitted bytes over
	 * a particular Link ID.
	 *
	 * \param linkId The link ID for which the number of transmitted bytes
	 * should be updated
	 * \param destinationNodeId The destination node ID of the link
	 * \param transmittedBytes The number of transmitted bytes
	 */
	void transmittedBytes(LINK_ID linkId, NODE_ID destinationNodeId,
			LINK_TYPE linkType, TX_BYTES transmittedBytes);
private:
	Namespace &_namespaceHelper; /*!< Reference to the class Namespace */
};

#endif /* LIB_BAMPERS_DATAPOINTS_STATISTICS_HH_ */
