/*
 * transport.hh
 *
 *  Created on: 22 Apr 2016
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

#ifndef NAP_TRANSPORT_HH_
#define NAP_TRANSPORT_HH_

#include <blackadder.hpp>
#include <boost/thread/mutex.hpp>

#include <configuration.hh>
#include <monitoring/statistics.hh>
#include <transport/lightweight.hh>
#include <transport/unreliable.hh>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

using namespace configuration;
using namespace monitoring::statistics;
using namespace std;
using namespace transport::lightweight;
using namespace transport::unreliable;

/*!
 * \brief Implementation of all transport protocols the NAP supports
 *
 * This class inherits all transport protocols the NAP supports namely
 * unreliable and lightweight
 */
class Transport: public Lightweight, public Unreliable
{
public:
	/*!
	 * \brief Constructor
	 */
	Transport(Blackadder *icnCore, Configuration &configuration,
			boost::mutex &icnCoreMutex, Statistics &statistics);
	/*!
	 * \brief Handle an incoming PUBLISH_DATA events
	 *
	 * In case the data to be handled has been an LTP CTRL-WE message and all
	 * segments have been received this method returns a TP state indicating
	 * that the parameters proxyRuleId and sessioKey have been filled so that
	 * the correct session can be checked afterwards.
	 *
	 * \param cId The CID under which the data has been received
	 * \param packet Pointer to the data which has been received
	 * \param packetSize The size of the data which has been received
	 * \param sessionKey The session key required to send off the packet after
	 * this method returns the corresponding TP state
	 *
	 * \return The LTP state using TpState enumeration
	 */
	TpState handle(IcnId &cId, void *packet, uint16_t &packetSize,
			uint16_t &sessionKey);
	/*!
	 * \brief Handle an incoming PUBLISH_DATA_iSUB events
	 *
	 * \param cId The CID under which it has been received
	 * \param rCId The iSub CID which must be used to publish the response
	 * \param nodeId The NID which must be used when publish the response
	 * \param packet Pointer to the ICN payload
	 * \param packetSize The length of the ICN payload
	 * \param sessionKey The session key needed if all fragements have been
	 * received and retrievePacket method is being called afterwards
	 *
	 * \return The LTP state using TpState enumeration
	 */
	TpState handle(IcnId &cId, IcnId &rCId, string &nodeId, void *packet,
			uint16_t &packetSize, uint16_t &sessionKey);
	/*!
	 * \brief Retrieve a packet from ICN buffer
	 *
	 * At the moment this is only meant for LTP (HTTP-over-ICN). The UTP does
	 * not require any additional input and can directly send off IP traffic.
	 *
	 * TODO params
	 */
	bool retrievePacket(IcnId &rCId, string &nodeId, uint16_t &sessionKey,
			uint8_t *packet, uint16_t &packetSize);
private:
	TpState _tpState;
};
#endif /* NAP_TRANSPORT_HH_ */
