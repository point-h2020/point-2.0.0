/*
 * link.hh
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

#ifndef LIB_BAMPERS_DATAPOINTS_LINK_HH_
#define LIB_BAMPERS_DATAPOINTS_LINK_HH_

#include "../namespace.hh"

class Link {
public:
	Link(Namespace &namespaceHelper);
	~Link();
	/*!
	 * \brief Report the existence of a new link between two nodes
	 *
	 * This method allows the topology manager to report a new link to the
	 * monitoring server.
	 *
	 * The namespace scope path used is:
	 *
	 * /monitoring/topology/links/linkId/destinationNodeId/linkType/state
	 *
	 * with 'state' being the information item, 'linkId' the hashed value of
	 * this particular link ID, 'destinationNodeId' the node ID of the
	 * destination node and 'linkType' the technology used for the new link.
	 * The state and source node ID are added to the payload of the published
	 * ICN packet.
	 *
	 * The state (STATE_ADDED) is automatically added by this method.
	 *
	 * \param linkId The new link to be reported
	 * \param sourceNodeId The source node ID of the new link
	 * \param destinationNodeId The destination node ID of the new link
	 * \param linkType The link type as specified in enum.hh -> LinkTypes
	 */
	void add(LINK_ID linkId, NODE_ID sourceNodeId, NODE_ID destinationNodeId,
			LINK_TYPE linkType);
	/*!
	 * \brief Report that a particular link between two nodes disappeared
	 *
	 * This method allows the topology manager to report that a particular link
	 * was completely removed form the topology.
	 *
	 * The namespace scope path used is:
	 *
	 * /monitoring/topology/links/linkId/destinationNodeId/linkType/state
	 *
	 * with 'state' being the information item, 'linkId' the hashed value of
	 * this particular link ID, 'destinationNodeId' the node ID of the
	 * destination node and 'linkType' the technology which is used by the link.
	 * The state is added to the payload of the published ICN packet.
	 *
	 * The state (STATE_REMOVED) is automatically added by this method. Once the
	 * monitoring server removed the link, it unsubscribes from the scope path
	 * /monitoring/topology/links/linkId which causes a STOP_PUBLISH event. This
	 * is handled by the IcnEventHandler class which removes the scope path from
	 * the list of published scope paths.
	 */
	void remove(LINK_ID linkId, NODE_ID sourceNodeId, NODE_ID destinationNodeId,
			LINK_TYPE linkType);
private:
	Namespace &_namespaceHelper; /*!< Reference to the class Namespace */
};

#endif /* LIB_BAMPERS_DATAPOINTS_LINK_HH_ */
