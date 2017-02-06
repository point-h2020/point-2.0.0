/*
 * lightweighttimeout.hh
 *
 *  Created on: 20 Jul 2016
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

#ifndef NAP_TRANSPORT_LIGHTWEIGHTTIMEOUT_HH_
#define NAP_TRANSPORT_LIGHTWEIGHTTIMEOUT_HH_

#include <blackadder.hpp>
#include <boost/date_time.hpp>
#include <log4cxx/logger.h>
#include <map>

#include <configuration.hh>
#include <enumerations.hh>
#include <types/icnid.hh>
#include <transport/lightweighttypedef.hh>
#include <types/nodeid.hh>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

using namespace configuration;
using namespace log4cxx;
using namespace std;

namespace transport
{

namespace lightweight
{
/*!
 * \brief Implementation of the LTP timer as a non-blocking operation
 */
class LightweightTimeout
{
	static LoggerPtr logger;
public:
	/*!
	 * \brief Constructor for HTTP requests in sNAP
	 *
	 * Expensive class instantiation, but only required if START_PUBLISH has not
	 * been received
	 *
	 * \param cId Reference to the CID under which WE has been published
	 * \param icnCore
	 */
	LightweightTimeout(IcnId cId, IcnId rCId, uint16_t sessionKey,
			uint16_t sequenceNumber, Blackadder *icnCore,
			boost::mutex &icnCoreMutex, uint16_t rtt, void *proxyPacketBuffer,
			boost::mutex &proxyPacketBufferMutex, void *windowEnded,
			boost::mutex &windowEndedMutex);
	/*!
	 * \brief Destructor
	 */
	~LightweightTimeout();
	/*!
	 * \brief Functor
	 */
	void operator()();
private:
	IcnId _cId;
	IcnId _rCId;
	ltp_hdr_data_t _ltpHeaderData;
	NodeId _nodeId;
	Blackadder *_icnCore;
	boost::mutex &_icnCoreMutex;
	uint16_t _rtt;
	proxy_packet_buffer_t *_proxyPacketBuffer; /*!< Pointer to proxy packet
	buffer */
	proxy_packet_buffer_t::iterator _proxyPacketBufferIt;/*!< Iterator for
	packetBuffer map */
	boost::mutex &_proxyPacketBufferMutex;/*!< mutex for _packetBuffer map*/
	map<uint32_t, map<uint32_t, bool>> *_windowEndedRequests;/*!<
	map<rCID, map<0, WE received>> */
	map<uint32_t, map<uint32_t, bool>>::iterator _windowEndedRequestsIt;/*!<
	Iterator for _windowEndedRequests map */
	map<uint32_t, map<uint32_t, map<uint32_t, bool>>> *_windowEndedResponses;
	/*!<map<rCID, map<0, map<NID, WE received>>> */
	map<uint32_t, map<uint32_t, map<uint32_t, bool>>>::iterator
	_windowEndedResponsesIt;/*!<Iterator for _windowEndedRequests map */
	boost::mutex &_windowEndedMutex;/*!<Reference to the mutex allowing
	transaction safe operation*/
	/*!
	 * \brief Delete a buffered proxy packet from LTP buffer
	 *
	 * \param rCId The rCID for which the proxy packet buffer should be look up
	 * \param ltpHeader The LTP header holding all the required information to
	 * delete the right packet
	 */
	void _deleteProxyPacket(IcnId &rCId, ltp_hdr_ctrl_wed_t &ltpHeader);
	/*!
	 * \brief Re-publish WED CTRL message
	 */
	void _rePublishWindowEnd();
};

} /* namespace lightweight */

} /* namespace transport */

#endif /* NAP_TRANSPORT_LIGHTWEIGHTTIMEOUT_HH_ */
