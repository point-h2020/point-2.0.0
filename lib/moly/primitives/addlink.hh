/*
 * addlink.hh
 *
 *  Created on: 13 Jan 2016
 *      Author: Sebastian Robitzsch <sebastian.robitzsch@interdigital.com>
 *      		Mays Al-Naday <mfhaln@essex.ac.uk>
 *
 * This file is part of the MOnitoring LibrarY (MOLY).
 *
 * MOLY is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * MOLY is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * MOLY. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MOLY_PRIMITIVES_ADDLINK_HH_
#define MOLY_PRIMITIVES_ADDLINK_HH_

#include "../typedef.hh"

/*!
 * \brief Implementation of the ADD_LINK primitive
 */
class AddLink {
public:
	/*!
	 * \brief Constructor
	 */
	AddLink(string name, string linkId, uint32_t sourceNodeId,
			uint32_t destinationNodeId, uint8_t linkType);
	/*!
	 * \brief Constructor
	 */
	AddLink(uint8_t * pointer, size_t size);
	/*!
	 * \brief Deconstructor
	 */
	~AddLink();
	/*!
	 * \brief Obtain the destination node ID
	 */
	uint32_t destinationNodeId();
	/*!
	 * \brief Obtain the link ID
	 */
	uint32_t linkId();
	/*!
	 * \brief Obtain the link type
	 */
	uint8_t linkType();
	/*!
	 * \brief Obtain the string
	 */
	string name();
	/*!
	 * \brief Pointer to the packet
	 */
	uint8_t * pointer();
	/*!
	 * \brief Print out the content of the ADD_LINK primitive
	 *
	 * The print out order is determined by the MOLY specification
	 */
	string print();
	/*!
	 * \brief Size
	 *
	 */
	size_t size();
	/*!
	 * \brief Obtain the source node ID
	 */
	uint32_t sourceNodeId();
private:
	string _name;
	string _linkId;
	uint32_t _linkIdHashed;
	uint32_t _sourceNodeId;
	uint32_t _destinationNodeId;
	uint8_t _linkType;
	uint8_t * _pointer; /*!< Pointer to the generated packet */
	size_t _size; /*!< Size of the generated packet */
	/*!
	 * \brief Compose packet
	 *
	 * This method composes the packet according to the specification.
	 */
	void _composePacket();
	/*!
	 * \brief Decompose packet
	 *
	 * This method decomposes the packet according to the specification.
	 */
	void _decomposePacket();
	/*!
	 * \brief Convert the link ID from a string into a uint32_t
	 *
	 * The link ID becomes a scope ID and cannot stay a string
	 */
	void hashLinkId();
};

#endif /* MOLY_PRIMITIVES_ADDLINK_HH_ */
