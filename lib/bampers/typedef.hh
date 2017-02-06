/*
 * typedef.hh
 *
 *  Created on: Dec 18, 2015
 *      Author: Sebastian Robitzsch <sebastian.robitzsch@interdigital.com>
 *
 * This file is part of BlAckadder Monitoring wraPpER clasS (BAMPERS).
 *
 * BAMPERS is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * BAMPERS is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * BAMPERS. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef LIB_BAMPERS_TYPEDEF_HH_
#define LIB_BAMPERS_TYPEDEF_HH_

typedef uint16_t INFORMATION_ITEM;
typedef uint32_t LINK_ID; /*!<  */
typedef uint8_t LINK_TYPE; /*!< Technology used for a particular link ID */
typedef uint32_t NODE_ID; /*!< 0 - 65535 */
typedef uint8_t STATE;
typedef uint8_t TOPOLOGY_TYPE;
typedef uint32_t TX_BYTES;

#endif /* LIB_BAMPERS_TYPEDEF_HH_ */
