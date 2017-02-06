/*
 * name.hh
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

#ifndef LIB_BAMPERS_DATAPOINTS_NAME_HH_
#define LIB_BAMPERS_DATAPOINTS_NAME_HH_

#include "../namespace.hh"

/*!
 * \brief Reporting application or hostname
 */
class Name {
public:
	/*!
	 * \brief Constructor
	 */
	Name(Namespace &namespaceHelper);
	/*!
	 * \brief Destructor
	 */
	~Name();
	/*!
	 * \brief UE or server attached to NAP
	 */
	void addHostname();
	/*!
	 * \brief Update the name of a node
	 *
	 * Scope path: /monitoring/topology/linkIds/destinationNodeId/linkType/name
	 *
	 * \param linkId The link ID of the connection between two nodes
	 * \param destinationNodeId The destination node ID of the link
	 * \param linkType The technology used for this particular link
	 * \param name The name of the node
	 */
	void updateNodeName(LINK_ID linkId, NODE_ID destinationNodeId,
			LINK_TYPE linkType, string name);
	/*!
	 * \brief Update the role of a node
	 *
	 * Scope path: /monitoring/topology/linkIds/destinationNodeId/linkType/role
	 *
	 * \param linkId The link ID of the connection between two nodes
	 * \param destinationNodeId The destination node ID of the link
	 * \param linkType The technology used for this particular link
	 * \param role The role of the node
	 */
	void updateNodeRole(LINK_ID linkId, NODE_ID destinationNodeId,
			LINK_TYPE linkType, string role);
	/*!
	 * \brief UE or server detached from NAP
	 */
	void removeHostname();
private:
	Namespace &_namespaceHelper; /*!< Reference to the class Namespace */
};

#endif /* LIB_BAMPERS_DATAPOINTS_NAME_HH_ */
