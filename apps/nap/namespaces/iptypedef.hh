/*
 * iptypedef.hh
 *
 *  Created on: 15 Sep 2016
 *      Author: Sebastian Robitzsch <sebastian.robitzsch@interdigital.com>
 *
 * This file is part of Blackadder.
 *
 * Blackadder is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Blackadder is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Blackadder.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef NAP_NAMESPACES_IPTYPEDEF_HH_
#define NAP_NAMESPACES_IPTYPEDEF_HH_

#include <unordered_map>

#include <types/icnid.hh>

struct packet_t
	{
		uint8_t *packet; /*!< Pointer to the actual IP packet */
		uint16_t packetSize;/*!< Length of IP packet */
		boost::posix_time::ptime timestamp;/*!< Timestamp of when this packet
		was added to buffer*/
	};

typedef unordered_map<uint32_t, pair<IcnId, packet_t>> packet_buffer_t ; /*!<
 	 	 unordered_map<hashedCID, pair<CID, Packet>>*/

#endif /* NAP_NAMESPACES_IPTYPEDEF_HH_ */
