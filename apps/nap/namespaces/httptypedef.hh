/*
 * httptypedef.hh
 *
 *  Created on: 8 Aug 2016
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

#ifndef NAP_NAMESPACES_HTTPTYPEDEF_HH_
#define NAP_NAMESPACES_HTTPTYPEDEF_HH_

#include <stack>

#include <enumerations.hh>
#include <types/icnid.hh>
#include <types/nodeid.hh>

#define ENIGMA 23 // https://en.wikipedia.org/wiki/23_enigma

/*!
 * \brief Struct to hold an HTTP request packet
 */
struct request_packet_t
{
	IcnId cId;/*!< CID of the HTTP request */
	IcnId rCId;/*!< rCID of the HTTP response*/
	uint32_t ripd;
	uint16_t sessionKey;/*!< Session key for this particular HTTP packet */
	http_methods_t httpMethod;/*!< */
	uint8_t *packet;/*!< */
	uint16_t packetSize;/*!< */
	boost::posix_time::ptime timestamp;/*!< */
};
/*!
 * \brief Struct to hold an HTTP response packet
 */
struct response_packet_t
{
	IcnId rCId;/*!< rCID of the HTTP response*/
	uint32_t ripd;/*!< Proxy rule identifier*/
	uint16_t sessionKey; /*!< Session key under which this response */
	uint8_t *packet;/*!< */
	uint16_t packetSize;/*!< */
	boost::posix_time::ptime timestamp;/*!< */
};

typedef map<uint32_t, map<uint32_t, map<uint16_t, list<NodeId>>>>
		cmc_groups_t ; /*!< map<rCID, map<0, map<Session key, list<NIDs>>>>*/

typedef map<uint32_t, map<uint32_t, map<uint32_t, bool>>> potential_cmc_groups_t
		; /*!< map<rCId, map<0, map<NID, bool>>>> */

typedef map<uint32_t, map<uint32_t, request_packet_t>> packet_buffer_requests_t
		; /*!< map<cId, map<rCId, Packet Struct>> */

typedef map<uint32_t, map<uint32_t, stack<response_packet_t>>>
		packet_buffer_responses_t ; /*!< map<rCID, map<0, stack<Packet Struct
		>>>*/



#endif /* NAP_NAMESPACES_HTTPTYPEDEF_HH_ */
