/*
 * addnodetype.hh
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

#ifndef MOLY_PRIMITIVES_ADDNODETYPE_HH_
#define MOLY_PRIMITIVES_ADDNODETYPE_HH_

#include "../typedef.hh"
/*!
 * \brief Implementation of the ADD_NODE_TYPE primitive
 */
class AddNodeType {
public:
	/*!
	 * \brief Constructor to write packet
	 */
	AddNodeType(string name, uint8_t nodeType);
	/*!
	 * \brief Constructor to read packet
	 */
	AddNodeType(uint8_t * pointer, size_t size);
	/*!
	 * \brief Deconstructor
	 */
	~AddNodeType();
	/*!
	 * \brief Obtain the name
	 *
	 * \return The name
	 */
	string name();
	/*!
	 * \brief Obtain the node type
	 *
	 * \return The node type
	 */
	uint8_t nodeType();
	/*!
	 * \brief Pointer to the packet
	 *
	 * \return Pointer to packet
	 */
	uint8_t * pointer();
	/*!
	 * \brief Print out the content of the ADD_NODE primitive
	 *
	 * The print out order is determined by the MOLY specification
	 */
	string print();
	/*!
	 * \brief Size
	 *
	 * \return The size of the memory _pointer is pointing to
	 */
	size_t size();
private:
	string _name;
	uint8_t _nodeType;
	uint8_t * _pointer;/*!< Pointer to the generated packet */
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

#endif /* MOLY_PRIMITIVES_ADDNODETYPE_HH_ */
