/*
 * enum.hh
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

#ifndef ENUM_HH_
#define ENUM_HH_

/*!
 * \brief Message types for bootstrapping of applications and agents
 */
enum BoostrappingMessageTypes
{
	BOOTSTRAP_OK,
	BOOTSTRAP_ERROR,
	BOOTSTRAP_MY_PID,
	BOOTSTRAP_START_REPORTING,
	BOOTSTRAP_SERVER_UP
};

enum LinkStates {
	LINK_STATES_UNKNOWN,/*!< 0 */
	LINK_STATES_UP,/*!< 1 */
	LINK_STATES_DOWN/*!< 2 */
};
/*!
 * \brief Link types
 */
enum LinkTypes {
	LINK_TYPE_UNKNOWN,/*!< 0 */
	LINK_TYPE_802_3,/*!< 1 */
	LINK_TYPE_802_11,/*!< 2 */
	LINK_TYPE_802_11_A,/*!< 3 */
	LINK_TYPE_802_11_B,/*!< 4 */
	LINK_TYPE_802_11_G,/*!< 5 */
	LINK_TYPE_802_11_N,/*!< 6 */
	LINK_TYPE_802_11_AA,/*!< 7 */
	LINK_TYPE_802_11_AC,/*!< 8 */
	LINK_TYPE_SDN_802_3_Z, /*!< 9: IEEE spec deployed by pica8 SDN switches for
	1G ports*/
	LINK_TYPE_SDN_802_3_AE,/*!< 10: IEEE spec deployed by pica8 SDN switches for
	10G optical ports*/
	LINK_TYPE_GPRS,/*!< 11 */
	LINK_TYPE_UMTS,/*!< 12 */
	LINK_TYPE_LTE,/*!< 13 */
	LINK_TYPE_LTE_A,/*!< 14 */
	LINK_TYPE_OPTICAL,/*!< 15 */

};

enum PrimitiveTypes {
	PRIMITIVE_TYPE_UNKNOWN,
	PRIMITIVE_TYPE_ADD_LINK,
	PRIMITIVE_TYPE_ADD_NODE,
	PRIMITIVE_TYPE_ADD_NODE_TYPE,
	PRIMITIVE_TYPE_CMC_GROUP_SIZE,
	PRIMITIVE_TYPE_HTTP_REQUESTS_PER_FQDN,
	PRIMITIVE_TYPE_LINK_STATE,
	PRIMITIVE_TYPE_NETWORK_LATENCY_PER_FQDN,
	PRIMITIVE_TYPE_NODE_STATE
};

/*!
 * \brief Node types
 */
enum NodeTypes {
	NODE_TYPE_UNKNOWN,/*!< 0 */
	NODE_TYPE_GW,/*!< 1 */
	NODE_TYPE_FN,/*!< 2 */
	NODE_TYPE_NAP,/*!< 3 */
	NODE_TYPE_RV,/*!< 4 */
	NODE_TYPE_SERVER,/*!< 5 */
	NODE_TYPE_TM,/*!< 6 */
	NODE_TYPE_UE,/*!< 7 */
	NODE_TYPE_LNK = 18/*!< 18 */
};

enum States {
	STATE_UNKNOWN,/*!< 0 */
	STATE_BOOTED,/*!< 1 */
	STATE_DOWN = 4,/*!< 4 */
	STATE_UP = 8/*!< 8 */
};

#endif /* ENUM_HH_ */
