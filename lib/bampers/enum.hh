/**
 * enum.hh
 *
 * Created on: Nov 25, 2015
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

#include <moly/enum.hh>
#include <stdint.h>
#include <string>
#include "typedef.hh"
#define ID_LEN 16
using namespace std;

/*!
 * \brief Primitives for application > MA API
 */
enum BampersPrimitives
{
	LINK_ADDED, /*!< /monitoring/topology/links/linkId/destinationId/linkType/
	sourceNodeId */
	LINK_STATUS_CHANGED, /*!< /monitoring/topology/links/linkId/destinationId/
	linkType/state */
	LINK_REMOVED /*!< /monitoring/topology/links/linkId/destinationId/linkType*/
};

/*!
 * \brief Information items
 */
enum BampersInformationItems {
	II_UNKNOWN,
	II_CMC_GROUP_SIZE,
	II_FQDN,
	II_HTTP_REQUESTS_PER_FQDN, /*!< /monitoring/nodes/nap||gw/
	httpRequestsPerFqdn */
	II_NAME, /*!< The name of a node */
	II_NETWORK_LATENCY_PER_FQDN, /*!< Average network latency reported by a NAP
	for a particular FQDN*/
	II_NODE_ID,
	II_NUMBER_OF_STATES,
	II_PREFIX,
	II_ROLE, /*!< The role of a node */
	II_SOURCE_NODE_ID, /*!< /monitoring/topology/links/linkId/destinationNodeId/
	linkType/sourceNodeId */
	II_STATE,
	II_TRANSMITTED_BYTES
};

/*!
 * \brief Topology
 */
enum BampersTopologyTypes {
	TOPOLOGY_NODES,
	TOPOLOGY_LINKS
};
