/*
 * enumerations.hh
 *
 *  Created on: 9 Feb 2016
 *      Author: Sebastian Robitzsch <sebastian.robitzsch@interdigital.com>
 *
 * This file is part of the ICN application MOnitOring SErver (MOOSE) which
 * comes with Blackadder.
 *
 * MOOSE is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * MOOSE is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * MOOSE. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef APPS_MONITORING_SERVER_ENUMERATIONS_HH_
#define APPS_MONITORING_SERVER_ENUMERATIONS_HH_

#include <blackadder_enums.hpp>
#include <moly/enum.hh>

enum Attributes
{
	ATTRIBUTE_GW_LIST_OF_FQDNS = 1,
	ATTRIBUTE_GW_PREFIXES,/*!< 2 */
	ATTRIBUTE_GW_HTTP_REQUESTS,/*!< 3 */
	ATTRIBUTE_GW_HTTP_REQ_RES_RATIO,/*!< 4 */
	ATTRIBUTE_GW_INCOMING_BYTES,/*!< 5 */
	ATTRIBUTE_GW_OUTGOING_BYTES,/*!< 6 */
	ATTRIBUTE_GW_STATE,/*!< 7 */
	ATTRIBUTE_FN_NAME,/*!< 8 */
	ATTRIBUTE_FN_LINK_ID,/*!< 9 */
	ATTRIBUTE_FN_STATE,/*!< 10 */
	ATTRIBUTE_NAP_LIST_OF_FQDNS,/*!< 11 */
	ATTRIBUTE_NAP_PREFIXES,/*!< 12 */
	ATTRIBUTE_NAP_HTTP_REQUESTS,/*!< 13 */
	ATTRIBUTE_NAP_HTTP_REQ_RES_RATIO, /*!< 14 CMC group size */
	ATTRIBUTE_NAP_INCOMING_BYTES,/*!< 15 */
	ATTRIBUTE_NAP_OUTGOING_BYTES,/*!< 16 */
	ATTRIBUTE_NAP_STATE,/*!< 17 */
	ATTRIBUTE_RV_NUMBER_OF_STATES,/*!< 18 */
	ATTRIBUTE_RV_NUMBER_OF_MATCH_REQUESTS,/*!< 19 */
	ATTRIBUTE_RV_CPU_LOAD,/*!< 20 */
	ATTRIBUTE_RV_CPU_MEMORY_USAGE,/*!< 21 */
	ATTRIBUTE_RV_CPU_THREADS,/*!< 22 */
	ATTRIBUTE_RV_STATE,/*!< 23 */
	ATTRIBUTE_SV_FQDN,/*!< 24 */
	ATTRIBUTE_SV_STATE,/*!< 25 */
	ATTRIBUTE_TM_NUMBER_OF_UNICAST_FIDS,/*!< 26 */
	ATTRIBUTE_TM_NUMBER_OF_PATH_REQUESTS,/*!< 27 */
	ATTRIBUTE_TM_AVERAGE_PATH_LENGTH,/*!< 28 */
	ATTRIBUTE_TM_MIN_PATH_LENGTH,/*!< 29 */
	ATTRIBUTE_TM_MAX_PATH_LENGTH,/*!< 30 */
	ATTRIBUTE_TM_CPU_LOAD,/*!< 31 */
	ATTRIBUTE_TM_CPU_MEMORY_USAGE,/*!< 32 */
	ATTRIBUTE_TM_CPU_THREADS,/*!< 33 */
	ATTRIBUTE_TM_STATE,/*!< 34 */
	ATTRIBUTE_UE_NAME,/*!< 35 */
	ATTRIBUTE_UE_DASH_JITTER,/*!< 36 */
	ATTRIBUTE_UE_DASH_DROPPED_FRAMES,/*!< 37 */
	ATTRIBUTE_UE_STATE,/*!< 38 */
	ATTRIBUTE_LNK_DESTINATIONS,/*!< 39 */
	ATTRIBUTE_LNK_BYTES_PER_TIME_INTERVAL,/*!< 40 */
	ATTRIBUTE_LNK_STATE,/*!< 41 */
	ATTRIBUTE_LNK_BUFFER_OCCUPANCY,/*!< 42 */
	ATTRIBUTE_LNK_BUFFER_FULL_EVENT,/*!< 43 */
	ATTRIBUTE_NAP_NETWORK_LATENCY/*!< 44 */
};

enum ElementTypes
{
	ELEMENT_TYPE_GW = 1, /*!< 1 */
	ELEMENT_TYPE_FN, /*!< 2 */
	ELEMENT_TYPE_NAP, /*!< 3 */
	ELEMENT_TYPE_RV, /*!< 4 */
	ELEMENT_TYPE_SERVER, /*!< 5 */
	ELEMENT_TYPE_TM, /*!< 6 */
	ELEMENT_TYPE_UE, /*!< 7 */
	/* Combined node types (2) */
	ELEMENT_TYPE_FN_NAP, /*!< 8 */
	ELEMENT_TYPE_FN_GW, /*!< 9 */
	ELEMENT_TYPE_FN_TM, /*!< 10 */
	ELEMENT_TYPE_FN_RV, /*!< 11 */
	/* Combined node types (3) */
	ELEMENT_TYPE_FN_NAP_TM, /*!< 12 */
	ELEMENT_TYPE_FN_NAP_RV, /*!< 13 */
	ELEMENT_TYPE_FN_GW_TM, /*!< 14 */
	ELEMENT_TYPE_FN_GW_RV, /*!< 15 */
	/* Combined node types (4) */
	ELEMENT_TYPE_FN_NAP_TM_RV, /*!< 16 */
	ELEMENT_TYPE_FN_GW_TM_RV, /*!< 17 */
	ELEMENT_TYPE_LNK, /*!< 18 */
	ELEMENT_TYPE_FN_TM_RV, /*!< 19*/
	ELEMENT_TYPE_FN_GW_NAP /*!< 20 */
};
#endif /* APPS_MONITORING_SERVER_ENUMERATIONS_HH_ */
