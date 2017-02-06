/*
 * lightweighttimeout.cc
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

#include "lightweighttimeout.hh"

using namespace transport::lightweight;

LoggerPtr LightweightTimeout::logger(Logger::getLogger("transport.lightweight"));

LightweightTimeout::LightweightTimeout(IcnId cId, IcnId rCId,
		uint16_t sessionKey, uint16_t sequenceNumber, Blackadder *icnCore,
		boost::mutex &icnCoreMutex, uint16_t rtt, void *proxyPacketBuffer,
		boost::mutex &proxyPacketBufferMutex, void *windowEnded,
		boost::mutex &windowEndedMutex)
	: _cId(cId),
	  _rCId(rCId),
	  _icnCore(icnCore),
	  _icnCoreMutex(icnCoreMutex),
	  _rtt(rtt),
	  _proxyPacketBufferMutex(proxyPacketBufferMutex),
	  _windowEndedMutex(windowEndedMutex)
{
	_ltpHeaderData.sessionKey = sessionKey;
	_ltpHeaderData.sequenceNumber = sequenceNumber;
	_nodeId = NodeId(0);
	_proxyPacketBuffer = (proxy_packet_buffer_t *) proxyPacketBuffer;
	_windowEndedRequests = (map<uint32_t, map<uint32_t, bool>> *) windowEnded;
	_windowEndedResponses = NULL;
}

LightweightTimeout::~LightweightTimeout() {}

void LightweightTimeout::operator()()
{
	/*
	 * First it must be checked what type of WE map must be used (is this timer
	 * for a request or a response)
	 */
	// Request
	uint8_t attempts = ENIGMA;
	if (_nodeId.uint() == 0)
	{
		map<uint32_t, bool>::iterator pridRequestsIt;
		// check boolean continuously for 2 * RRT (timeout)
		while (attempts != 0)
		{
			boost::posix_time::ptime startTime =
					boost::posix_time::microsec_clock::local_time();
			boost::posix_time::ptime currentTime = startTime;
			boost::posix_time::time_duration timeWaited;
			timeWaited = currentTime - startTime;
			// check for received WED until 2 * RTT has been reached
			while (timeWaited.total_milliseconds() < (2 * _rtt))
			{
				currentTime = boost::posix_time::microsec_clock::local_time();
				timeWaited = currentTime - startTime;
				_windowEndedMutex.lock();

				_windowEndedRequestsIt =
						_windowEndedRequests->find(_rCId.uint());
				// rCID does not exist (unlikely - just to avoid seg faults)
				if (_windowEndedRequestsIt == _windowEndedRequests->end())
				{
					LOG4CXX_ERROR(logger, "rCID " << _rCId.print()
							<< " does not exist in list of sent WE CTRL "
									"messages");
					_windowEndedMutex.unlock();
					return;
				}
				pridRequestsIt =
						_windowEndedRequestsIt->second.find(0);
				// 0 does not exit (unlikely - just to avoid seg faults)
				if (pridRequestsIt == _windowEndedRequestsIt->second.end())
				{
					LOG4CXX_ERROR(logger, "rCID "<< _rCId.print() << " does not "
							"exist in list of sent WE CTRL messages");
					_windowEndedMutex.unlock();
					return;
				}
				// WED received. stop here
				if (pridRequestsIt->second)
				{
					LOG4CXX_TRACE(logger, "LTP CTRL-WED received for CID "
							<< _cId.print());
					LOG4CXX_TRACE(logger, "0 " << pridRequestsIt->first
							<< " removed from WED map");
					_windowEndedRequestsIt->second.erase(pridRequestsIt);
					// Delete the entire rCID key if values are empty
					if (_windowEndedRequestsIt->second.size() == 0)
					{
						_windowEndedRequests->erase(_windowEndedRequestsIt);
						LOG4CXX_TRACE(logger, "Entire rCID "
								<< _rCId.print() << " entry in "
										"_windowEndedRequests map deleted");
					}
					_windowEndedMutex.unlock();
					// Delete packet from LTP proxy packet buffer
					ltp_hdr_ctrl_wed_t ltpHeaderWed;
					ltpHeaderWed.sessionKey = _ltpHeaderData.sessionKey;
					_deleteProxyPacket(_rCId, ltpHeaderWed);
					return;
				}
				_windowEndedMutex.unlock();
			}/* RTT reached */
			LOG4CXX_TRACE(logger, "LTP CTRL-WED has not been received within"
					"given timeout of " << 2 * _rtt << "ms for rCID "
					<< _rCId.print() << " waited "
					<< timeWaited.total_milliseconds());
			_rePublishWindowEnd();
			attempts--;
			//_rtt = _rtt + _rtt;
		} /* ENIGMA attempts reached*/
	}
	// WE timer for Responses
	else
	{
		//TODO
	}
}

void LightweightTimeout::_deleteProxyPacket(IcnId &rCId,
		ltp_hdr_ctrl_wed_t &ltpHeader)
{
	//map<Sequence, Packet
	map<uint16_t, pair<uint8_t *, uint16_t>>::iterator	sequenceKeyMapIt;
	//map<SK,     map<Sequence, Packet
	map<uint16_t, map<uint16_t, pair<uint8_t *, uint16_t>>>::iterator
			sessionKeyMapIt;
	//map<0,   map<SK,       map<Sequence, Packet
	map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t *,
			uint16_t>>>>::iterator pridMapIt;
	_proxyPacketBufferMutex.lock();
	_proxyPacketBufferIt = _proxyPacketBuffer->find(rCId.uint());
	// rCID does not exist
	if (_proxyPacketBufferIt == _proxyPacketBuffer->end())
	{
		LOG4CXX_WARN(logger, "rCID " << rCId.print() << " cannot be found "
				"in proxy packet buffer map");
		_proxyPacketBufferMutex.unlock();
		return;
	}
	pridMapIt = _proxyPacketBufferIt->second.find(0);
	// 0 does not exist
	if (pridMapIt == _proxyPacketBufferIt->second.end())
	{
		_proxyPacketBufferMutex.unlock();
		return;
	}
	sessionKeyMapIt = pridMapIt->second.find(ltpHeader.sessionKey);
	// SK does not exist
	if (sessionKeyMapIt == pridMapIt->second.end())
	{
		LOG4CXX_WARN(logger, "SK " << ltpHeader.sessionKey << " could "
				"not be found in proxy packet buffer map for rCID "
				<< rCId.print());
		_proxyPacketBufferMutex.unlock();
		return;
	}
	// deleting packets
	for (sequenceKeyMapIt = sessionKeyMapIt->second.begin();
			sequenceKeyMapIt != sessionKeyMapIt->second.end();
			sequenceKeyMapIt++)
	{
		free(sequenceKeyMapIt->second.first);
	}
	// deleting entire session key
	pridMapIt->second.erase(sessionKeyMapIt);
	_proxyPacketBufferMutex.unlock();
	LOG4CXX_TRACE(logger, "Packet deleted from proxy packet buffer for rCID "
			<< rCId.print() << " > SK " << ltpHeader.sessionKey);
}

void LightweightTimeout::_rePublishWindowEnd()
{
	ltp_hdr_ctrl_we_t ltpHeader;
	uint8_t *packet;
	uint16_t packetSize = sizeof(ltp_hdr_ctrl_we_t);
	// Fill up the LTP CTRL header
	ltpHeader.messageType = LTP_CONTROL;
	ltpHeader.controlType = LTP_CONTROL_WINDOW_END;
	ltpHeader.sessionKey = _ltpHeaderData.sessionKey;
	ltpHeader.sequenceNumber = _ltpHeaderData.sequenceNumber;
	// make packet
	packet = (uint8_t *)malloc(packetSize);
	// [1] message type
	memcpy(packet, &ltpHeader.messageType, sizeof(ltpHeader.messageType));
	// [2] control type
	memcpy(packet + sizeof(ltpHeader.messageType), &ltpHeader.controlType,
			sizeof(ltpHeader.controlType));
	// [3] proxy rule Id
	memcpy(packet + sizeof(ltpHeader.messageType)
			+ sizeof(ltpHeader.controlType), &ltpHeader.ripd,
			sizeof(ltpHeader.ripd));
	// [4] Session key
	memcpy(packet + sizeof(ltpHeader.messageType)
			+ sizeof(ltpHeader.controlType) + sizeof(ltpHeader.ripd),
			&ltpHeader.sessionKey, sizeof(ltpHeader.sessionKey));
	// [5] Sequence number
	memcpy(packet + sizeof(ltpHeader.messageType)
			+ sizeof(ltpHeader.controlType) + sizeof(ltpHeader.ripd)
			+ sizeof(ltpHeader.sessionKey), &ltpHeader.sequenceNumber,
			sizeof(ltpHeader.sequenceNumber));
	_icnCoreMutex.lock();
	_icnCore->publish_data_isub(_cId.binIcnId(), DOMAIN_LOCAL, NULL, 0,
			_rCId.binIcnId(), packet, packetSize);
	_icnCoreMutex.unlock();
	LOG4CXX_TRACE(logger, "LTP Window End CTRL published under " << _cId.print()
			<< ", Sequence " << ltpHeader.sequenceNumber);
	free(packet);
}
