/*
 * linkstate.hh
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

#ifndef MOLY_PRIMITIVES_LINKSTATE_HH_
#define MOLY_PRIMITIVES_LINKSTATE_HH_

#include "../typedef.hh"

/*!
 * \brief Implementation of the LINK_STATE primitive
 */
class LinkState {
public:
	/*!
	 * \brief Constructor to write packet
	 */
	LinkState(uint32_t linkId, uint32_t destinationNodeId,
			uint8_t linkType, uint8_t state);
	/*!
	 * \brief Constructor to read packet
	 */
	LinkState(uint8_t * pointer, size_t _size);
	/*!
	 * \brief Deconstructor
	 */
	~LinkState();
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
	 * \brief Pointer to the packet
	 *
	 * \return
	 */
	uint8_t * pointer();
	/*!
	 * \brief Print out the content of the LINK_STATE primitive
	 *
	 * The print out order is determined by the MOLY specification
	 */
	string print();
	/*!
	 * \brief Size
	 *
	 * \return Return the size of the pointer returned by LinkState::pointer()
	 */
	size_t size();
	/*!
	 * \brief Obtain the state
	 */
	uint8_t state();
private:
	uint32_t _linkId;
	uint32_t _destinationNodeId;
	uint8_t _linkType;
	uint8_t _state;
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
};

#endif /* MOLY_PRIMITIVES_LINKSTATE_HH_ */
