/*
 * state.hh
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
#ifndef LIB_BAMPERS_DATAPOINTS_STATE_HH_
#define LIB_BAMPERS_DATAPOINTS_STATE_HH_

#include "../namespace.hh"
/*!
 * \brief
 */
class State {
public:
	/*!
	 * \brief Constructor
	 */
	State(Namespace &namespaceHelper);
	/*!
	 * \brief Destructor
	 */
	~State();
	/*!
	 * \brief Update the state of an ICN network element
	 *
	 * \param nodeType The node type (role)
	 * \param nodeId The NID for which the state is supposed to be updated
	 * \param state The new state of this node type
	 */
	void update(uint8_t nodeType, uint32_t nodeId, STATE state);
	/*!
	 * \brief Update the state of an IP endpoint in the network
	 */
	void update(uint8_t nodeType, string hostname, STATE state);
	/*!
	 * \brief Update the state of a link in the network
	 *
	 * This method allows the topology manager to update the state of a
	 * previously added link ID. The scope used is:
	 *
	 * /monitoring/links/linkId/dNodeId/state
	 *
	 * with 'state' being the information item, 'linkId' the hashed value of
	 * this particular link ID and 'dNodeId' the node ID of the destination
	 * node. The state and link type are added to the payload of the published
	 * ICN packet.
	 */
	void update(LINK_ID linkId, NODE_ID destinationNodeId, LINK_TYPE linkType,
			STATE state);
private:
	Namespace &_namespaceHelper; /*!< Reference to the class Namespace */
};
#endif /* LIB_BAMPERS_DATAPOINTS_STATE_HH_ */

