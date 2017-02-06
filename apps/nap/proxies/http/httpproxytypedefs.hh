/*
 * httpproxytypedefs.hh
 *
 *  Created on: 2 Oct 2016
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

#ifndef NAP_PROXIES_HTTP_HTTPPROXYTYPEDEFS_HH_
#define NAP_PROXIES_HTTP_HTTPPROXYTYPEDEFS_HH_

#include <unordered_map>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

/*!
 * \brief Struct for reverse opened TCP session lookup
 */
struct rcid_ripd_t
{
	IcnId rCId;/*!< The rCID  */
	uint32_t ripd = 0;
};

typedef std::unordered_map<int, rcid_ripd_t> reverse_lookup_t ;/*!< u_map<SFD,
		rcid_ripd_t>  */

typedef std::unordered_map<uint32_t, std::unordered_map<uint16_t, uint16_t>>
		socket_fds_t ; /*!< u_map<NID, u_map<rSFD, lSFD> */

#endif /* NAP_PROXIES_HTTP_HTTPPROXYTYPEDEFS_HH_ */
