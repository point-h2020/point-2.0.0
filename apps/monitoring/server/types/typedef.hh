/*
 * typedef.hh
 *
 *  Created on: 29 Dec 2016
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

#ifndef APPS_MONITORING_SERVER_TYPES_TYPEDEF_HH_
#define APPS_MONITORING_SERVER_TYPES_TYPEDEF_HH_

typedef struct dataPoints
{
	bool topology = false;
	bool statistics = false;
	bool networkLatencyPerFqdn = false;
	bool cmcGroupSize = false;
	bool txBytes = false;
	bool rxBytes = false;
} data_points_t;

#endif /* APPS_MONITORING_SERVER_TYPES_TYPEDEF_HH_ */
