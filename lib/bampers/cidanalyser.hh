/*
 * cidanalyser.hh
 *
 *  Created on: 03 June 2016
 *      Author: Sebastian Robitzsch <sebastian.robitzsch@interdigital.com>
 *
 * This file is part of BAMPERS.
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

#ifndef LIB_BAMPERS_CIDANALYSER_HH_
#define LIB_BAMPERS_CIDANALYSER_HH_

#include <iostream>
#include <moly/enum.hh>
#include <sstream>

#include "types/icnid.hh"

using namespace std;

/*!
 * \brief Class to parse a content identifier and the data received
 */
class CIdAnalyser {
public:
	/*!
	 * \brief Constructor
	 */
	CIdAnalyser();
	/*!
	 * \brief Constructor with IcnId as argument
	 */
	CIdAnalyser(IcnId icnId);
	/*!
	 * \brief Destructor
	 */
	~CIdAnalyser();
	/*!
	 * \brief Obtain the destination node ID from the ICN ID
	 *
	 * /monitoring/links/linkId/destinationNodeId
	 */
	uint32_t destinationNodeId();
	/*!
	 * \brief Obtain the link ID from the ICN ID
	 *
	 * /monitoring/links/linkId/
	 */
	string linkId();
	/*!
	 * \brief Obtain if this ICN ID points to /monitoring/TOPOLOGY_NODES branch
	 *
	 * \return Boolean
	 */
	bool linksScope();
	/*!
	 * \brief Obtain the link type from the ICN ID
	 *
	 * /monitoring/links/linkId/destinationNodeId/linkType
	 */
	uint8_t linkType();
	/*!
	 * \brief
	 *
	 * /monitoring/nodes/nodeType/nodeId/
	 */
	uint32_t nodeId();
	/*!
	 * \brief Obtain if this ICN ID points to /monitoring/TOPOLOGY_LINKS branch
	 *
	 * \return Boolean
	 */
	bool nodesScope();
	/*!
	 * \brief
	 *
	 * /monitoring/nodes/nodeType/
	 */
	uint8_t nodeType();
	/*!
	 * \brief In case default constructor was used this method overloads the
	 * equal sign operator to assign a new CID to the analyser class
	 *
	 * \param str The CID as a string after converting it from hex to char
	 */
	//void operator=(string &str);
	/*!
	 * \brief Obtain the primitive type and fill up the corresponding fields
	 *
	 * All primitive types are derived from the MOnitoring LibrarY (MOLY)
	 * specification in <moly/enum.hh>
	 */
	uint8_t primitive();
	/*!
	 * \brief Obtain the state
	 *
	 * If UPDATE_LINK_STATE was called by the application, the following scope
	 * path applies:
	 *
	 * /monitoring/links/linkId/destinationNodeId/linkType/state
	 *
	 * and the ICN data field consists of the new state of the link.
	 *
	 * If ADD_LINK was called by the application the default state value
	 * STATE_DOWN is set.
	 */
	uint8_t state();
	/*!
	 * \brief Obtain if this ICN ID points to /monitoring/TOPOLOGY_NODES branch
	 *
	 * \return Boolean
	 */
	bool statisticsScope();
private:
	IcnId _icnId;
	string _linkId;
	uint8_t _linkType;
	uint32_t _destinationNodeId;
	uint8_t _state;
	string _name;
	uint8_t _nodeType;
	uint32_t _nodeId;
	uint16_t _informationItem();
	uint8_t _scopeTypeLevel1();
};

#endif /* LIB_BAMPERS_CIDANALYSER_HH_ */
