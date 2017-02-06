/*
 * lightweighttypedef.hh
 *
 *  Created on: 4 Aug 2016
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

#ifndef NAP_TRANSPORT_LIGHTWEIGHTTYPEDEF_HH_
#define NAP_TRANSPORT_LIGHTWEIGHTTYPEDEF_HH_

#include <list>
#include <map>

#include <enumerations.hh>
#include <namespaces/httptypedef.hh>

#define ENIGMA 23 // https://en.wikipedia.org/wiki/23_enigma

typedef map<uint32_t, map<uint32_t, map<uint32_t, map<uint16_t, map<uint16_t,
		pair<uint8_t*, uint16_t>>>>>> icn_packet_buffer_t ; /*!< map<rCId,
		map<0, map<NID, map<Session, map<Sequence, PACKET>>>>>> */

typedef map<uint32_t, map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t *,
		uint16_t>>>>> proxy_packet_buffer_t ; /*!< map<rCID, map<0,
		map<SessionKey, map<Sequence number, pair<PACKET>>>>> */

/*!
 * \brief LTP CTRL control header
 */
struct ltp_hdr_ctrl_t
{
	ltp_message_types_t messageType;/*!< LTP message type (CTRL || DATA) */
	ltp_ctrl_control_types_t controlType;
	uint32_t ripd = 0;
	uint16_t sessionKey;
};
/*!
 * \brief LTP CTRL session end header
 */
struct ltp_hdr_ctrl_se_t
{
	ltp_message_types_t messageType = LTP_CONTROL;/*!< LTP message type (CTRL)*/
	ltp_ctrl_control_types_t controlType = LTP_CONTROL_SESSION_END; /* SE */
	uint32_t ripd = 0;
	uint16_t sessionKey;
};
/*!
 * \brief LTP CTRL session ended header
 */
struct ltp_hdr_ctrl_sed_t
{
	ltp_message_types_t messageType = LTP_CONTROL;/*!< LTP message type (CTRL)*/
	ltp_ctrl_control_types_t controlType = LTP_CONTROL_SESSION_ENDED; /* SED */
	uint32_t ripd = 0;
	uint16_t sessionKey;
};

/*!
 * \brief LTP control header for window update CTRL messages
 */
struct ltp_hdr_ctrl_wu_t
{
	ltp_message_types_t messageType;/*!< LTP message type (CTRL || DATA) */
	ltp_ctrl_control_types_t controlType;
	uint32_t ripd = 0;
	uint16_t sessionKey;
};
/*!
 * \brief LTP control header for window updated CTRL messages
 */
struct ltp_hdr_ctrl_wud_t
{
	ltp_message_types_t messageType = LTP_CONTROL;/*!< LTP message type (CTRL)*/
	ltp_ctrl_control_types_t controlType = LTP_CONTROL_WINDOW_UPDATED;
	uint32_t ripd = 0;
	uint16_t sessionKey;
};
/*!
 * \brief LTP control header for window end CTRL messages
 */
struct ltp_hdr_ctrl_we_t
{
	ltp_message_types_t messageType;/*!< LTP message type (CTRL || DATA) */
	ltp_ctrl_control_types_t controlType;
	uint32_t ripd = 0;
	uint16_t sessionKey;
	uint16_t sequenceNumber;/*!< The sequence number of the last
	data packet published to subscriber */
};
/*!
 * \brief LTP control header for window ended CTRL messages
 */
struct ltp_hdr_ctrl_wed_t
{
	ltp_message_types_t messageType;/*!< LTP message type (CTRL || DATA) */
	ltp_ctrl_control_types_t controlType;
	uint32_t ripd = 0;
	uint16_t sessionKey;
};
/*!
 * \brief LTP control header (NACKs)
 */
struct ltp_hdr_ctrl_nack_t
{
	ltp_message_types_t messageType;/*!< LTP message type (CTRL || DATA) */
	ltp_ctrl_control_types_t controlType;
	uint32_t ripd = 0;
	uint16_t sessionKey;
	uint16_t start;
	uint16_t end;
};
/*!
 * \brief LTP data header
 *
 * The order in this struct also indicates the order in which the
 * header has been written
 */
struct ltp_hdr_data_t
{
	ltp_message_types_t messageType;/*!< LTP message type (CTRL || DATA) */
	uint32_t ripd = 0;
	uint16_t sessionKey;
	uint16_t sequenceNumber;/*!< Sequentially generated sequence number for
	a particular LTP session */
	uint16_t payloadLength;
};
/*!
 * \brief NACK group description
 */
struct nack_group_t
{
	list<NodeId> nodeIds;/*!< List of NIDs in a particular NACK group*/
	uint16_t startSequence;/*!< Start sequence number for this NACK group*/
	uint16_t endSequence;/*!< End sequence number for this NACK group*/
};

#endif /* NAP_TRANSPORT_LIGHTWEIGHTTYPEDEF_HH_ */
