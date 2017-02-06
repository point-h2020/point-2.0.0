/*
 * lightweight.cc
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

#include "../proxies/http/httpproxy.hh"
#include "../proxies/http/tcpclient.hh"
#include "lightweight.hh"
#include "lightweighttimeout.hh"
#include "lightweighttypedef.hh"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

using namespace namespaces;
using namespace proxies::http::tcpclient;
using namespace transport::lightweight;

LoggerPtr Lightweight::logger(Logger::getLogger("transport.lightweight"));

Lightweight::Lightweight(Blackadder *icnCore, Configuration &configuration,
		boost::mutex &icnCoreMutex, Statistics &statistics)
	: TrafficControl(configuration),
	  _icnCore(icnCore),
	  _configuration(configuration),
	  _icnCoreMutex(icnCoreMutex),
	  _statistics(statistics)
{
	_knownNIds = NULL;// will be set in initialise() method
	_knownNIdsMutex = NULL;// will be set in initialise() method
	_cmcGroups = NULL;// will be set in initialise() method
	_cmcGroupsMutex = NULL;// will be set in initialise() method
	_potentialCmcGroups = NULL;// will be set in initialise() method
	_potentialCmcGroupsMutex = NULL;// will be set in initialise() method
	uint16_t rttListSize = 0;
	while (rttListSize < _configuration.ltpRttListSize())
	{
		_rtts.push_front(200);// milli seconds
		rttListSize++;
	}
	_rttMultiplier = _configuration.ltpRttMultiplier();
}

Lightweight::~Lightweight(){}

void Lightweight::initialise(void *potentialCmcGroup,
		void *potentialCmcGroupMutex, void *knownNIds, void *knownNIdsMutex,
		void *cmcGroups, void *cmcGroupsMutex)
{
	_potentialCmcGroups = (potential_cmc_groups_t *) potentialCmcGroup;
	_potentialCmcGroupsMutex = (boost::mutex *) potentialCmcGroupMutex;
	_knownNIds = (map<uint32_t, bool> *) knownNIds;
	_knownNIdsMutex = (boost::mutex *) knownNIdsMutex;
	_cmcGroups = (cmc_groups_t *)cmcGroups;
	_cmcGroupsMutex = (boost::mutex *)cmcGroupsMutex;
}

void Lightweight::publish(IcnId &rCid, uint16_t &sessionKey,
		list<NodeId> &nodeIds, uint8_t *data, uint16_t &dataSize)
{
	ltp_hdr_ctrl_we_t ltpHeaderCtrlWe;
	list<NodeId>::iterator nodeIdsIt;
	ltpHeaderCtrlWe.ripd = 0;
	ltpHeaderCtrlWe.sessionKey = sessionKey;
	if(nodeIds.empty())
	{
		LOG4CXX_WARN(logger, "List of NIDs is empty! Must not happen ... ")
	}
	_publishData(rCid, ltpHeaderCtrlWe, nodeIds, data, dataSize);
	_publishWindowEnd(rCid, nodeIds, ltpHeaderCtrlWe);
	// check for CTRL-WED received
	//map<Session
	map<uint16_t, bool>::iterator sessionKeyIt;
	//map<NID,    map<Session
	map<uint32_t, map<uint16_t, bool>>::iterator nidIt;
	//map<0,   map<NID,      map<Session
	map<uint32_t, map<uint32_t, map<uint16_t, bool>>>::iterator pridIt;
	// Now start WE timer
	uint8_t attempts = ENIGMA;
	uint16_t rtt = _rtt();
	boost::posix_time::ptime startStartTime =
					boost::posix_time::microsec_clock::local_time();
	while (attempts != 0)
	{
		// check boolean continuously for 2 * RRT (timeout)
		boost::posix_time::ptime startTime =
				boost::posix_time::microsec_clock::local_time();
		boost::posix_time::ptime currentTime = startTime;
		boost::posix_time::time_duration timeWaited;
		timeWaited = currentTime - startTime;
		// check for received WED until _timeout * RTT has been reached
		while (timeWaited.total_milliseconds() < (_rttMultiplier * rtt))
		{
			currentTime = boost::posix_time::microsec_clock::local_time();
			timeWaited = currentTime - startTime;
			_windowEndedResponsesMutex.lock();
			_windowEndedResponsesIt = _windowEndedResponses.find(rCid.uint());
			if (_windowEndedResponsesIt == _windowEndedResponses.end())
			{
				LOG4CXX_WARN(logger, "rCID " << rCid.print() << " does not "
						"exist in CTRL-WED map");
				_windowEndedResponsesMutex.unlock();
				return;
			}
			pridIt = _windowEndedResponsesIt->second.find(0);
			uint16_t numberOfConfirmedWeds = 0;
			for (nodeIdsIt = nodeIds.begin(); nodeIdsIt != nodeIds.end();
					nodeIdsIt++)
			{
				nidIt = pridIt->second.find(nodeIdsIt->uint());
				// NID found
				if (nidIt != pridIt->second.end())
				{
					sessionKeyIt = nidIt->second.find(
							ltpHeaderCtrlWe.sessionKey);
					// session key found
					if (sessionKeyIt != nidIt->second.end())
					{
						// WED received
						if (sessionKeyIt->second)
						{
							numberOfConfirmedWeds++;
						}
					}
				}
			}
			// Now check if the number of confirmed NIDs equals the CMC size
			if (numberOfConfirmedWeds == nodeIds.size())
			{
				timeWaited = currentTime - startStartTime;
				LOG4CXX_TRACE(logger, "All CTRL-WEDs received for rCID CMC "
						<< rCid.print() << " after waiting "
						<< timeWaited.total_milliseconds() << "ms");
				_rtt(timeWaited.total_milliseconds());
				_windowEndedResponsesMutex.unlock();
				return;
			}
			_windowEndedResponsesMutex.unlock();
		}
		LOG4CXX_TRACE(logger, "LTP CTRL-WED has not been received within given "
				"timeout of " << timeWaited.total_milliseconds() << "ms for "
				"rCID "	<< rCid.print());
		_publishWindowEnd(rCid, nodeIds, ltpHeaderCtrlWe);
		attempts--;
		if (attempts == 0)
		{
			LOG4CXX_DEBUG(logger, "Subscriber to rCID " << rCid.print()
					<< " has not responded to LTP CTRL-WE message after "
					<< (int)ENIGMA << " attempts. Stopping here");
		}
	}
}

void Lightweight::publish(IcnId &cId, IcnId &rCId, uint16_t &sessionKey,
		uint8_t *data, uint16_t &dataSize)
{
	_addReverseLookUp(cId, rCId);
	uint16_t sequenceNumber;
	// First publish the data
	sequenceNumber = _publishData(cId, rCId, sessionKey, data,
			dataSize);
	map<uint16_t, bool>::iterator sessionKeyMapIt;
	map<uint32_t, map<uint16_t, bool>>::iterator pridMapIt;
	// Now start WE timer
	uint8_t attempts = ENIGMA;
	uint16_t rtt = _rtt();
	boost::posix_time::ptime startStartTime =
					boost::posix_time::microsec_clock::local_time();
	while (attempts != 0)
	{
		// check boolean continuously for 2 * RRT (timeout)
		boost::posix_time::ptime startTime =
				boost::posix_time::microsec_clock::local_time();
		boost::posix_time::ptime currentTime = startTime;
		boost::posix_time::time_duration timeWaited;
		timeWaited = currentTime - startTime;
		// check for received WED until _timeout * RTT has been reached
		while (timeWaited.total_milliseconds() < (_rttMultiplier * rtt))
		{
			currentTime = boost::posix_time::microsec_clock::local_time();
			timeWaited = currentTime - startTime;
			_windowEndedRequestsMutex.lock();
			_windowEndedRequestsIt = _windowEndedRequests.find(rCId.uint());
			if (_windowEndedRequestsIt == _windowEndedRequests.end())
			{
				LOG4CXX_WARN(logger, "rCID " << rCId.print() << " does not "
						"exist in CTRL-WED map");
				_windowEndedRequestsMutex.unlock();
				return;
			}
			pridMapIt =	_windowEndedRequestsIt->second.find(0);
			sessionKeyMapIt = pridMapIt->second.find(sessionKey);
			// window ended received. stop here, update RTT and leave
			if (sessionKeyMapIt->second)
			{
				timeWaited = currentTime - startStartTime;
				LOG4CXX_TRACE(logger, "LTP CTRL-WED received for CID "
						<< cId.print() << " after waiting "
						<< timeWaited.total_milliseconds() << "ms");
				// Update RTT
				_rtt(timeWaited.total_milliseconds());
				_statistics.roundTripTime(cId, timeWaited.total_milliseconds());
				// Erase entry from WED request map
				pridMapIt->second.erase(sessionKeyMapIt);
				LOG4CXX_TRACE(logger, "SK (FD) " << sessionKey << " removed "
						"from CTRL-WED map for rCID " << rCId.print());
				// Delete entire key if empty
				if (pridMapIt->second.size() == 0)
				{
					_windowEndedRequestsIt->second.erase(pridMapIt);
					LOG4CXX_TRACE(logger, "Entire CTRL-WED entry in "
							"_windowEndedRequests map deleted for rCID "
							<< rCId.print());
				}
				// Delete the entire rCID key if values are empty
				if (_windowEndedRequestsIt->second.size() == 0)
				{
					_windowEndedRequests.erase(_windowEndedRequestsIt);
					LOG4CXX_TRACE(logger, "Entire rCID " << rCId.print()
							<< " entry in _windowEndedRequests map "
									"deleted");
				}
				_windowEndedRequestsMutex.unlock();
				return;
			}
			_windowEndedRequestsMutex.unlock();
		}
		//rtt = rtt + rtt;
		LOG4CXX_TRACE(logger, "WED CTRL has not been received within given "
				"timeout of " << _rttMultiplier * rtt << "ms for CID "
				<< cId.print());
		_publishWindowEnd(cId, rCId, sessionKey, sequenceNumber);
		attempts--;
		if (attempts == 0)
		{
			LOG4CXX_DEBUG(logger, "Subscriber to CID " << cId.print() << " has "
					"not responded to WE CTRL message after " << (int)ENIGMA
					<< " attempts. Stopping here");
		}
	}
}

void Lightweight::publishEndOfSession(IcnId &rCid,uint16_t &sessionKey)
{
	ltp_hdr_ctrl_se_t ltpHeaderCtrlSe;
	list<NodeId> nodeIds;
	list<NodeId>::iterator nodeIdsIt;
	ltpHeaderCtrlSe.ripd = 0;
	ltpHeaderCtrlSe.sessionKey = sessionKey;
	_cmcGroupsMutex->lock();
	_cmcGroupsIt = _cmcGroups->find(rCid.uint());
	if (_cmcGroupsIt == _cmcGroups->end())
	{
		LOG4CXX_TRACE(logger, "CMC group for rCID " << rCid.print()
				<< " already closed");
		_cmcGroupsMutex->unlock();
		return;
	}
	map<uint32_t, map<uint16_t, list<NodeId>>>::iterator cmcPridIt;
	cmcPridIt = _cmcGroupsIt->second.find(0);
	if (cmcPridIt == _cmcGroupsIt->second.end())
	{
		LOG4CXX_TRACE(logger, "CMC group for rCID " << rCid.print()
				<< " > 0 already closed");
		_cmcGroupsMutex->unlock();
		return;
	}
	map<uint16_t, list<NodeId>>::iterator cmcSkIt;
	cmcSkIt = cmcPridIt->second.find(sessionKey);
	if (cmcSkIt == cmcPridIt->second.end())
	{
		LOG4CXX_TRACE(logger, "CMC group for rCID " << rCid.print()
				<< " > 0 > SK " << sessionKey << " already closed");
		_cmcGroupsMutex->unlock();
		return;
	}
	nodeIds = cmcSkIt->second;
	_cmcGroupsMutex->unlock();
	if(nodeIds.empty())
	{
		LOG4CXX_WARN(logger, "List of NIDs is empty! End of LTP session cannot "
				"be sent to cNAPs");
		return;
	}
	_addSessionEnd(rCid, ltpHeaderCtrlSe, nodeIds);
	_publishSessionEnd(rCid, nodeIds, ltpHeaderCtrlSe);
	// check for CTRL-SED received
	// u_map<SK, SED received
	unordered_map<uint16_t, bool>::iterator skIt;
	// u_map<NID,           u_map<SK,               SED received
	unordered_map<uint32_t, unordered_map<uint16_t, bool>>::iterator nidIt;
	// u_map<0,          u_map<NID,              u_map<SK
	unordered_map<uint32_t, unordered_map<uint32_t, unordered_map<uint16_t,
		bool>>>::iterator pridIt;
	// Now start SE timer
	uint8_t attempts = ENIGMA;
	uint16_t rtt = _rtt();
	boost::posix_time::ptime startStartTime =
					boost::posix_time::microsec_clock::local_time();
	while (attempts != 0)
	{
		// check boolean continuously for 2 * RRT (timeout)
		boost::posix_time::ptime startTime =
				boost::posix_time::microsec_clock::local_time();
		boost::posix_time::ptime currentTime = startTime;
		boost::posix_time::time_duration timeWaited;
		timeWaited = currentTime - startTime;
		// check for received WED until _timeout * RTT has been reached
		while (timeWaited.total_milliseconds() < (_rttMultiplier * rtt))
		{
			currentTime = boost::posix_time::microsec_clock::local_time();
			timeWaited = currentTime - startTime;
			_sessionEndedResponsesMutex.lock();
			_sessionEndedResponsesIt = _sessionEndedResponses.find(rCid.uint());
			if (_sessionEndedResponsesIt == _sessionEndedResponses.end())
			{
				LOG4CXX_WARN(logger, "rCID " << rCid.print() << " does not "
						"exist in CTRL-SED map");
				_sessionEndedResponsesMutex.unlock();
				return;
			}
			pridIt = _sessionEndedResponsesIt->second.find(0);
			uint16_t numberOfConfirmedSeds = 0;
			for (nodeIdsIt = nodeIds.begin(); nodeIdsIt != nodeIds.end();
					nodeIdsIt++)
			{
				nidIt = pridIt->second.find(nodeIdsIt->uint());
				// NID found
				if (nidIt != pridIt->second.end())
				{
					skIt = nidIt->second.find(ltpHeaderCtrlSe.sessionKey);
					// SK found
					if (skIt != nidIt->second.end())
					{
						// SED received
						if (skIt->second)
						{
							numberOfConfirmedSeds++;
						}
					}
				}
			}
			// Now check if the number of confirmed NIDs equals the CMC size
			if (numberOfConfirmedSeds == nodeIds.size())
			{
				timeWaited = currentTime - startStartTime;
				LOG4CXX_TRACE(logger, "All CTRL-SEDs received for rCID CMC "
						<< rCid.print() << " after waiting "
						<< timeWaited.total_milliseconds() << "ms");
				_rtt(timeWaited.total_milliseconds());
				_sessionEndedResponsesMutex.unlock();
				// cleaning up
				_deleteSessionEnd(rCid, ltpHeaderCtrlSe, nodeIds);
				return;
			}
			_sessionEndedResponsesMutex.unlock();
		}
		LOG4CXX_TRACE(logger, "LTP CTRL-SED has not been received within given "
				"timeout of " << timeWaited.total_milliseconds() << "ms for "
				"rCID "	<< rCid.print() << " > 0 > SK " << sessionKey);
		_publishSessionEnd(rCid, nodeIds, ltpHeaderCtrlSe);
		attempts--;
		//rtt = rtt + rtt;
		if (attempts == 0)
		{
			LOG4CXX_DEBUG(logger, "Subscriber to rCID " << rCid.print()
					<< " has not responded to LTP CTRL-SE message after "
					<< (int)ENIGMA << " attempts. Stopping here");
		}
	}
}

void Lightweight::publishFromBuffer(IcnId &cId, IcnId &rCId,
		uint16_t &sessionKey, uint8_t *data, uint16_t &dataSize)
{
	_addReverseLookUp(cId, rCId);
	ltp_hdr_data_t ltpHeader;
	// First publish the packet (and send WE)
	ltpHeader.sequenceNumber = _publishData(cId, rCId, sessionKey,
			data, dataSize);
	// Starting timer in a thread and go back to the ICN handler
	LightweightTimeout ltpTimeout(cId, rCId, sessionKey,
			ltpHeader.sequenceNumber, _icnCore, _icnCoreMutex, _rtt(),
			(void *)&_proxyPacketBuffer, _proxyPacketBufferMutex,
			(void *)&_windowEndedRequests, _windowEndedRequestsMutex);
	_timeoutThreads.create_thread(ltpTimeout);
	// TODO check for obsolete threads in vector and erase them
}

TpState Lightweight::handle(IcnId &rCid, uint8_t *packet, uint16_t &sessionKey)
{
	TpState ltpState;
	// First get the LTP message type (data || control)
	ltp_hdr_ctrl_t header;
	// [1] message type
	memcpy(&header.messageType, packet, sizeof(header.messageType));
	switch (header.messageType)
	{
	case LTP_DATA:
		_handleData(rCid, packet);
		ltpState = TP_STATE_NO_ACTION_REQUIRED;
		break;
	case LTP_CONTROL:
		ltpState = _handleControl(rCid, packet, sessionKey);
		break;
	default:
		LOG4CXX_WARN(logger, "Unknown LTP message type " << header.messageType
				<< " received under rCID " << rCid.print());
		ltpState = TP_STATE_NO_ACTION_REQUIRED;
	}
	return ltpState;
}

TpState Lightweight::handle(IcnId &cId, IcnId &rCId, string &nodeIdStr,
		uint8_t *packet, uint16_t &sessionKey)
{
	TpState ltpState;
	NodeId nodeId = nodeIdStr;
	_addNodeId(nodeId);
	// First get the LTP message type (data || control)
	ltp_hdr_ctrl_t ltpHeader;
	// [1] message type
	memcpy(&ltpHeader.messageType, packet, sizeof(ltpHeader.messageType));
	switch (ltpHeader.messageType)
	{
	case LTP_DATA:
		_handleData(cId, rCId, nodeId, packet);
		ltpState = TP_STATE_NO_ACTION_REQUIRED;
		break;
	case LTP_CONTROL:
		ltpState = _handleControl(cId, rCId, nodeId, packet, sessionKey);
		break;
	default:
		ltpState = TP_STATE_NO_ACTION_REQUIRED;
	}
	return ltpState;
}

bool Lightweight::retrievePacket(IcnId &rCId, string &nodeIdStr,
		uint16_t &sessionKey, uint8_t *packet, uint16_t &packetSize)
{
	NodeId nodeId = nodeIdStr;
	// map<SeqNum,PACKET>>>
	map<uint16_t, pair<uint8_t*, uint16_t>>::iterator sequenceMapIt;
	// map<SK,    map<SeqNum,   PACKET>>
	map<uint16_t, map<uint16_t, pair<uint8_t *, uint16_t>>>::iterator
			sessionKeyMapIt;
	// map<NID,   map<SK,       map<SeqNum,   PACKET>>>
	map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t*,
			uint16_t>>>>::iterator nIdMapIt;
	// map<0,  map<NID,      map<SK,       map<SeqNum,   PACKET>>>
	map<uint32_t, map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t*,
			uint16_t>>>>>::iterator prIdMapIt;
	_icnPacketBufferMutex.lock();
	_icnPacketBufferIt = _icnPacketBuffer.find(rCId.uint());
	if (_icnPacketBufferIt == _icnPacketBuffer.end())
	{
		LOG4CXX_DEBUG(logger, "rCID " << rCId.print() << " does not exist in "
				"ICN packet buffer. Nothing to retrieve");
		_icnPacketBufferMutex.unlock();
		return false;
	}
	prIdMapIt = _icnPacketBufferIt->second.find(0);
	if (prIdMapIt == _icnPacketBufferIt->second.end())
	{
		LOG4CXX_DEBUG(logger, "0 for known rCID "
				<< rCId.print() << " does not exist in ICN packet buffer. "
						"Nothing to retrieve");
		_icnPacketBufferMutex.unlock();
		return false;
	}
	nIdMapIt = prIdMapIt->second.find(nodeId.uint());
	if (nIdMapIt == prIdMapIt->second.end())
	{
		LOG4CXX_DEBUG(logger, "NID " << nodeId.str() << " for known 0 and rCID "
				<< rCId.print() << " does not exist in ICN packet buffer. "
						"Nothing to retrieve");
		_icnPacketBufferMutex.unlock();
		return false;
	}
	sessionKeyMapIt = nIdMapIt->second.find(sessionKey);
	if (sessionKeyMapIt == nIdMapIt->second.end())
	{
		LOG4CXX_DEBUG(logger, "SK " << sessionKey << " for rCID "
				<< rCId.print() << " > 0 > NID "
				<< nodeId.uint() << " could not be found");
		_icnPacketBufferMutex.unlock();
		return false;
	}
	// now retrieve all fragments and make packet
	// first, get the packet length to allocate memory
	packetSize = 0;
	for (sequenceMapIt = sessionKeyMapIt->second.begin();
			sequenceMapIt != sessionKeyMapIt->second.end(); sequenceMapIt++)
	{
		packetSize += sequenceMapIt->second.second;
	}
	// now reassemble the packets
	uint16_t offset = 0;
	for (sequenceMapIt = sessionKeyMapIt->second.begin();
			sequenceMapIt != sessionKeyMapIt->second.end(); sequenceMapIt++)
	{
		memcpy(packet + offset, sequenceMapIt->second.first,
				sequenceMapIt->second.second);
		offset += sequenceMapIt->second.second;// add size of current fragment
		free(sequenceMapIt->second.first);// free up the memory
	}
	LOG4CXX_TRACE(logger, "Packet of length " << packetSize << " retrieved from"
			" ICN packet buffer");
	// Delete this packet from the buffer
	nIdMapIt->second.erase(sessionKeyMapIt);
	// check if this was the last session key which had been deleted
	// map<rCId, map<0, map<NID, map<SK
	if (nIdMapIt->second.size() == 0)
	{
		LOG4CXX_TRACE(logger, "Last SK for rCID " << rCId.print()
				<< " > 0 > NID " << nodeId.uint()
				<< ". Deleting entire NID key from ICN buffer map");
		prIdMapIt->second.erase(nIdMapIt);
		if (prIdMapIt->second.size())
		{
			LOG4CXX_TRACE(logger, "Last NID for rCID " << rCId.print()
					<< " > 0. Deleting entire 0 from ICN buffer map");
			_icnPacketBufferIt->second.erase(prIdMapIt);
		}
		// if this was the last 0, delete rCID entry too
		if (_icnPacketBufferIt->second.size() == 0)
		{
			LOG4CXX_TRACE(logger, "Last 0 entry for rCID " << rCId.print()
					<< ". Deleting entire rCID key from ICN buffer map");
			_icnPacketBuffer.erase(_icnPacketBufferIt);
		}
	}
	_icnPacketBufferMutex.unlock();
	return true;
}

void Lightweight::_addNackNodeId(IcnId &rCid,
		ltp_hdr_ctrl_nack_t &ltpHeaderNack, NodeId &nodeId)
{

	_nackGroupsIt = _nackGroups.find(rCid.uint());
	// rCID does not exist yet
	if (_nackGroupsIt == _nackGroups.end())
	{
		// map<0,  map<SK,       struct<nackGroup
		map<uint32_t, map<uint16_t, nack_group_t>> pridMap;
		// map<SK,       struct<nackGroup
		map<uint16_t, nack_group_t> skMap;
		nack_group_t nackGroup;
		nackGroup.nodeIds.push_back(nodeId);
		nackGroup.startSequence = ltpHeaderNack.start;
		nackGroup.endSequence = ltpHeaderNack.end;
		skMap.insert(pair<uint16_t, nack_group_t>(ltpHeaderNack.sessionKey,
				nackGroup));
		pridMap.insert(pair<uint32_t, map<uint16_t, nack_group_t>>(
				ltpHeaderNack.ripd, skMap));
		_nackGroups.insert(pair<uint32_t, map<uint32_t, map<uint16_t,
				nack_group_t>>>(rCid.uint(), pridMap));
		LOG4CXX_TRACE(logger, "New NACK group created for rCID " << rCid.print()
				<< " > 0 " << ltpHeaderNack.ripd << " > SK "
				<< ltpHeaderNack.sessionKey << " with segment range "
				<< ltpHeaderNack.start << " - " << ltpHeaderNack.end
				<< " and NID " << nodeId.uint());
		return;
	}
	// rCID exists
	map<uint32_t, map<uint16_t, nack_group_t>>::iterator pridMapIt;
	pridMapIt = _nackGroupsIt->second.find(ltpHeaderNack.ripd);
	// 0 does not exist
	if (pridMapIt == _nackGroupsIt->second.end())
	{
		// map<SK,       struct<nackGroup
		map<uint16_t, nack_group_t> skMap;
		nack_group_t nackGroup;
		nackGroup.nodeIds.push_back(nodeId);
		nackGroup.startSequence = ltpHeaderNack.start;
		nackGroup.endSequence = ltpHeaderNack.end;
		skMap.insert(pair<uint16_t, nack_group_t>(ltpHeaderNack.sessionKey,
				nackGroup));
		_nackGroupsIt->second.insert(pair<uint32_t, map<uint16_t,
				nack_group_t>>(ltpHeaderNack.ripd, skMap));
		LOG4CXX_TRACE(logger, "New NACK group created for known rCID "
				<< rCid.print()	<< " but new 0 " << ltpHeaderNack.ripd
				<< " > SK " << ltpHeaderNack.sessionKey << " with segment "
				"range " << ltpHeaderNack.start << " - " << ltpHeaderNack.end
				<< " and NID " << nodeId.uint());
		return;
	}
	// 0 exists
	map<uint16_t, nack_group_t>::iterator skMapIt;
	skMapIt = pridMapIt->second.find(ltpHeaderNack.sessionKey);
	// SK does not exist
	if (skMapIt == pridMapIt->second.end())
	{
		nack_group_t nackGroup;
		nackGroup.nodeIds.push_back(nodeId);
		nackGroup.startSequence = ltpHeaderNack.start;
		pridMapIt->second.insert(pair<uint16_t, nack_group_t>(
				ltpHeaderNack.sessionKey, nackGroup));
		LOG4CXX_TRACE(logger, "New NACK group created for known rCID "
				<< rCid.print()	<< " > 0 " << ltpHeaderNack.ripd
				<< " but new SK " << ltpHeaderNack.sessionKey << " with segment"
				" range " << ltpHeaderNack.start << " - " << ltpHeaderNack.end
				<< " and NID " << nodeId.uint());
		return;
	}
	// SK exists. Add NID, if it's unknown
	list<NodeId>::iterator nodeIdsIt;
	bool nidFound = false;
	for (nodeIdsIt = skMapIt->second.nodeIds.begin();
			nodeIdsIt != skMapIt->second.nodeIds.end(); nodeIdsIt++)
	{
		if (nodeIdsIt->uint() == nodeId.uint())
		{
			nidFound = true;
			LOG4CXX_TRACE(logger, "NID " << nodeId.uint() << " already known "
					"in the NACK group. This NID won't be added then");
			break;
		}
	}
	// add NID if it doesn't exist yet
	if (!nidFound)
	{
		skMapIt->second.nodeIds.push_back(nodeId);
		LOG4CXX_TRACE(logger, "New NID " << nodeId.uint() << " added to rCID "
				<< rCid.print() << " > 0 " << ltpHeaderNack.ripd
				<< " > SK " << ltpHeaderNack.sessionKey);
	}
	// decrease the start segment number if required
	if (ltpHeaderNack.start < skMapIt->second.startSequence)
	{
		skMapIt->second.startSequence = ltpHeaderNack.start;
		LOG4CXX_TRACE(logger, "Start segment decreased to "
				<< skMapIt->second.startSequence << " for rCID " << rCid.print()
				<< " > 0 " << ltpHeaderNack.ripd << " > SK "
				<< ltpHeaderNack.sessionKey);
	}
	// increase the end segment number if required
	if (ltpHeaderNack.end > skMapIt->second.endSequence)
	{
		skMapIt->second.endSequence = ltpHeaderNack.end;
		LOG4CXX_TRACE(logger, "Start segment increased to "
				<< skMapIt->second.endSequence << " for rCID " << rCid.print()
				<< " > 0 " << ltpHeaderNack.ripd << " > SK "
				<< ltpHeaderNack.sessionKey);
	}
}

void Lightweight::_addNodeId(NodeId &nodeId)
{
	_knownNIdsMutex->lock();
	_knownNIdsIt = _knownNIds->find(nodeId.uint());
	if (_knownNIdsIt == _knownNIds->end())
	{
		_knownNIds->insert(pair<uint16_t, bool>(nodeId.uint(), false));
		LOG4CXX_DEBUG(logger, "New NID " << nodeId.uint() << " added to the "
				"list of known remote NAPs (forwarding set to false)");
	}
	_knownNIdsMutex->unlock();
}

void Lightweight::_addNodeIdToWindowUpdate(IcnId &rCid, uint16_t &sessionKey,
		NodeId &nodeId)
{
	//map<NID,    WUD has not been received
	map<uint32_t, bool> nidMap;
	map<uint32_t, bool>::iterator nidIt;
	//map<SK,     map<NID
	map<uint16_t, map<uint32_t, bool>> skMap;
	map<uint16_t, map<uint32_t, bool>>::iterator skIt;
	//map<0,   map<SK,       map<NID
	map<uint32_t, map<uint16_t, map<uint32_t, bool>>> pridMap;
	map<uint32_t, map<uint16_t, map<uint32_t, bool>>>::iterator pridIt;
	_windowUpdateMutex.lock();
	_windowUpdateIt = _windowUpdate.find(rCid.uint());
	// rCID not found
	if (_windowUpdateIt == _windowUpdate.end())
	{
		nidMap.insert(pair<uint32_t, bool>(nodeId.uint(), false));
		skMap.insert(pair<uint16_t, map<uint32_t, bool>>(sessionKey, nidMap));
		pridMap.insert(pair<uint32_t, map<uint16_t, map<uint32_t, bool>>>(0,
				skMap));
		_windowUpdate.insert(pair<uint32_t, map<uint32_t, map<uint16_t,
				map<uint32_t, bool>>>>(rCid.uint(), pridMap));
		LOG4CXX_TRACE(logger, "NID " << nodeId.uint() << " added to new rCID "
				<< rCid.print() << " > SK "
				<< sessionKey);
		_windowUpdateMutex.unlock();
		return;
	}
	// rCID found
	pridIt = _windowUpdateIt->second.find(0);
	// 0 does not exist
	if (pridIt == _windowUpdateIt->second.end())
	{
		nidMap.insert(pair<uint32_t, bool>(nodeId.uint(), false));
		skMap.insert(pair<uint16_t, map<uint32_t, bool>>(sessionKey, nidMap));
		_windowUpdateIt->second.insert(pair<uint32_t, map<uint16_t,
				map<uint32_t, bool>>>(0, skMap));
		LOG4CXX_TRACE(logger, "NID " << nodeId.uint() << " added to existing "
				"rCID "	<< rCid.print() << " but new 0 > SK " << sessionKey);
		_windowUpdateMutex.unlock();
		return;
	}
	// 0 found
	skIt = pridIt->second.find(sessionKey);
	// SK does not exist
	if (skIt == pridIt->second.end())
	{
		nidMap.insert(pair<uint32_t, bool>(nodeId.uint(), false));
		pridIt->second.insert(pair<uint16_t, map<uint32_t, bool>>(
				sessionKey, nidMap));
		LOG4CXX_TRACE(logger, "NID " << nodeId.uint() << " added to existing "
				"rCID "	<< rCid.print() << " > 0 but "
				<< "new SK " << sessionKey);
		_windowUpdateMutex.unlock();
		return;
	}
	nidIt = skIt->second.find(nodeId.uint());
	if (nidIt != skIt->second.end())
	{
		_windowUpdateMutex.unlock();
		return;
	}
	skIt->second.insert(pair<uint16_t, bool>(nodeId.uint(), false));
	LOG4CXX_TRACE(logger, "NID " << nodeId.uint() << " added to existing rCID "
			<< rCid.print() << " > 0 > SK "
			<< sessionKey << ". " << skIt->second.size() << " NID(s) in this "
			"list");
	_windowUpdateMutex.unlock();
}


void Lightweight::_addReverseLookUp(IcnId &cid, IcnId &rCid)
{
	_cIdReverseLookUpMutex.lock();
	_cIdReverseLookUpIt = _cIdReverseLookUp.find(rCid.uint());
	if (_cIdReverseLookUpIt == _cIdReverseLookUp.end())
	{
		_cIdReverseLookUp.insert(pair<uint32_t, IcnId>(rCid.uint(), cid));
		LOG4CXX_TRACE(logger, "Reverse rCID to CID lookup entry created: "
				<< rCid.print() << " -> " << cid.print());
	}
	_cIdReverseLookUpMutex.unlock();
}

void Lightweight::_addSessionEnd(IcnId &rCid, ltp_hdr_ctrl_se_t &ltpHdrCtrlSe,
			list<NodeId> nodeIds)
{
	// 		u_map<SK, SED received
	unordered_map<uint16_t, bool>::iterator skIt;
	//		u_map<NID,		u_map<SK
	unordered_map<uint32_t, unordered_map<uint16_t, bool>>::iterator nidIt;
	//		u_map<0,				u_map<NID,				u_map<SK
	unordered_map<uint32_t, unordered_map<uint32_t, unordered_map<uint16_t,
			bool>>>::iterator pridIt;
	_sessionEndedResponsesMutex.lock();
	_sessionEndedResponsesIt = _sessionEndedResponses.find(rCid.uint());
	// rCID does not exist
	if (_sessionEndedResponsesIt == _sessionEndedResponses.end())
	{
		unordered_map<uint16_t, bool> skMap;
		unordered_map<uint32_t, unordered_map<uint16_t, bool>> nidMap;
		unordered_map<uint32_t, unordered_map<uint32_t, unordered_map<uint16_t,
				bool>>> pridMap;
		skMap.insert(pair<uint16_t, bool>(ltpHdrCtrlSe.sessionKey, false));
		for (list<NodeId>::iterator it = nodeIds.begin(); it != nodeIds.end();
				it++)
		{
			nidMap.insert(pair<uint32_t, unordered_map<uint16_t, bool>>
					(it->uint(), skMap));
		}
		pridMap.insert(pair<uint32_t, unordered_map<uint32_t,
				unordered_map<uint16_t, bool>>>(ltpHdrCtrlSe.ripd,
						nidMap));
		_sessionEndedResponses.insert(pair<uint32_t, unordered_map<uint32_t,
				unordered_map<uint32_t, unordered_map<uint16_t, bool>>>>
				(rCid.uint(), pridMap));
		LOG4CXX_TRACE(logger, "New CTRL-SED boolean added for rCid "
				<< rCid.print() << " > 0 " << ltpHdrCtrlSe.ripd
				<< " > " << nodeIds.size() << " NID(s) > SK "
				<< ltpHdrCtrlSe.sessionKey);
		_sessionEndedResponsesMutex.unlock();
		return;
	}
	pridIt = _sessionEndedResponsesIt->second.find(ltpHdrCtrlSe.ripd);
	// 0 does not exist
	if (pridIt == _sessionEndedResponsesIt->second.end())
	{
		unordered_map<uint16_t, bool> skMap;
		unordered_map<uint32_t, unordered_map<uint16_t, bool>> nidMap;
		skMap.insert(pair<uint16_t, bool>(ltpHdrCtrlSe.sessionKey, false));
		for (list<NodeId>::iterator it = nodeIds.begin(); it != nodeIds.end();
				it++)
		{
			nidMap.insert(pair<uint32_t, unordered_map<uint16_t, bool>>
					(it->uint(), skMap));
		}
		_sessionEndedResponsesIt->second.insert(pair<uint32_t,
				unordered_map<uint32_t, unordered_map<uint16_t, bool>>>
				(ltpHdrCtrlSe.ripd, nidMap));
		LOG4CXX_TRACE(logger, "New CTRL-SED boolean added for known rCid "
				<< rCid.print() << " but new 0 " << ltpHdrCtrlSe.ripd
				<< " > " << nodeIds.size() << " NID(s) > SK "
				<< ltpHdrCtrlSe.sessionKey);
		_sessionEndedResponsesMutex.unlock();
		return;
	}
	// iterate over list of NIDs and set boolean to false
	for (list<NodeId>::iterator it = nodeIds.begin(); it != nodeIds.end(); it++)
	{
		nidIt = pridIt->second.find(it->uint());
		// NID does not exist
		if (nidIt == pridIt->second.end())
		{
			unordered_map<uint16_t, bool> skMap;
			skMap.insert(pair<uint16_t, bool>(ltpHdrCtrlSe.sessionKey, false));
			pridIt->second.insert(pair<uint32_t, unordered_map<uint16_t, bool>>
					(it->uint(), skMap));
			LOG4CXX_TRACE(logger, "New CTRL-SED boolean added for known rCid "
					<< rCid.print() << " > 0 " << ltpHdrCtrlSe.ripd
					<< " but new " << nodeIds.size() << " NID(s) > SK "
					<< ltpHdrCtrlSe.sessionKey);
		}
		// NID exists
		else
		{
			skIt = nidIt->second.find(ltpHdrCtrlSe.sessionKey);
			// SK does not exist
			if (skIt == nidIt->second.end())
			{
				nidIt->second.insert(pair<uint16_t, bool>(
						ltpHdrCtrlSe.sessionKey, false));
				LOG4CXX_TRACE(logger, "New CTRL-SED boolean added for known"
						"rCid " << rCid.print() << " > 0 "
						<< ltpHdrCtrlSe.ripd << " > NID " << it->uint()
						<< " but new SK " << ltpHdrCtrlSe.sessionKey);
			}
			// SK exists
			else
			{
				skIt->second = false;
				LOG4CXX_TRACE(logger, "CTRL-SED boolean reset to false for"
						"known rCid " << rCid.print() << " > 0 "
						<< ltpHdrCtrlSe.ripd << " > NID " << it->uint()
						<< " > SK " << ltpHdrCtrlSe.sessionKey);
			}
		}
	}
	_sessionEndedResponsesMutex.unlock();
}

void Lightweight::_addWindowEnd(IcnId &rCid, ltp_hdr_ctrl_we_t &ltpHeaderCtrlWe)
{
	// Inform endpoint that all data has been sent
	_windowEndedRequestsMutex.lock();
	_windowEndedRequestsIt = _windowEndedRequests.find(rCid.uint());
	// rCID does not exist in WED map
	if (_windowEndedRequestsIt == _windowEndedRequests.end())
	{
		map<uint16_t, bool> sessionKeyMap;
		map<uint32_t, map<uint16_t, bool>> pridMap;
		sessionKeyMap.insert(pair<uint16_t, bool>(ltpHeaderCtrlWe.sessionKey,
				false));
		pridMap.insert(pair<uint32_t, map<uint16_t, bool>>(
				ltpHeaderCtrlWe.ripd, sessionKeyMap));
		_windowEndedRequests.insert(pair<uint32_t, map<uint32_t, map<uint16_t,
				bool>>>(rCid.uint(), pridMap));
		LOG4CXX_TRACE(logger, "New CTRL-WED boolean added for rCID "
				<< rCid.print()	<< " > 0 " << ltpHeaderCtrlWe.ripd
				<< " > SK " << ltpHeaderCtrlWe.sessionKey);
	}
	// rCID exists
	else
	{
		map<uint32_t, map<uint16_t, bool>>::iterator pridMapIt;
		pridMapIt = _windowEndedRequestsIt->second.find(
				ltpHeaderCtrlWe.ripd);
		// 0 does not exist
		if (pridMapIt == _windowEndedRequestsIt->second.end())
		{
			map<uint16_t, bool> sessionKeyMap;
			sessionKeyMap.insert(pair<uint16_t, bool>(
					ltpHeaderCtrlWe.sessionKey, false));
			_windowEndedRequestsIt->second.insert(pair<uint32_t, map<uint16_t,
					bool>>(ltpHeaderCtrlWe.ripd, sessionKeyMap));
			LOG4CXX_TRACE(logger, "New CTRL-WED boolean added for existing "
					"rCID " << rCid.print() << " but new 0 "
					<< ltpHeaderCtrlWe.ripd << " > SK "
					<< ltpHeaderCtrlWe.sessionKey);
		}
		// 0 exists. Simply set it to false (even though this should never be
		// the case
		else
		{
			map<uint16_t, bool>::iterator sessionKeyMapIt;
			sessionKeyMapIt = pridMapIt->second.find(
					ltpHeaderCtrlWe.sessionKey);
			// Session key doesn't exist
			if (sessionKeyMapIt == pridMapIt->second.end())
			{
				pridMapIt->second.insert(pair<uint16_t, bool>(
						ltpHeaderCtrlWe.sessionKey,	false));
				LOG4CXX_TRACE(logger, "New CTRL-WED boolean added for existing "
						"rCID "	<< rCid.print() << " > 0 "
						<< ltpHeaderCtrlWe.ripd << " but new session "
								"key " << ltpHeaderCtrlWe.sessionKey);
			}
			sessionKeyMapIt->second = false;
			LOG4CXX_TRACE(logger, "CTRL-WED boolean reset to false for rCID "
					<< rCid.print() << " > 0 " << ltpHeaderCtrlWe.ripd
					<< " > SK " << ltpHeaderCtrlWe.sessionKey);
		}
	}
	_windowEndedRequestsMutex.unlock();
}

void Lightweight::_addWindowEnd(IcnId &rCid, list<NodeId> &nodeIds,
		ltp_hdr_ctrl_we_t &ltpHeaderCtrlWe)
{
	ostringstream oss;
	// Inform endpoint that all data has been sent
	_windowEndedResponsesMutex.lock();
	_windowEndedResponsesIt = _windowEndedResponses.find(rCid.uint());
	// rCID does not exist in WED map
	if (_windowEndedResponsesIt == _windowEndedResponses.end())
	{
		map<uint16_t, bool> sessionKeyMap;
		map<uint32_t, map<uint16_t, bool>> nidMap;
		list<NodeId>::iterator nidsIt;
		map<uint32_t, map<uint32_t, map<uint16_t, bool>>> pridMap;
		sessionKeyMap.insert(pair<uint16_t, bool>(ltpHeaderCtrlWe.sessionKey,
				false));
		for (nidsIt = nodeIds.begin(); nidsIt != nodeIds.end(); nidsIt++)
		{
			nidMap.insert(pair<uint32_t, map<uint16_t, bool>>(
					nidsIt->uint(), sessionKeyMap));
			oss << nidsIt->uint() << " ";
		}
		pridMap.insert(pair<uint32_t, map<uint32_t, map<uint16_t, bool>>>(
				ltpHeaderCtrlWe.ripd, nidMap));
		_windowEndedResponses.insert(pair<uint32_t, map<uint32_t, map<uint32_t,
				map<uint16_t, bool>>>>(rCid.uint(), pridMap));
		LOG4CXX_TRACE(logger, "New CTRL-WED boolean added for rCID "
				<< rCid.print()	<< " > 0 " << ltpHeaderCtrlWe.ripd
				<< " > NIDs " << oss.str() << "> SK "
				<< ltpHeaderCtrlWe.sessionKey);
		_windowEndedResponsesMutex.unlock();
		return;
	}
	// rCID exists
	map<uint32_t, map<uint32_t, map<uint16_t, bool>>>::iterator pridMapIt;
	pridMapIt = _windowEndedResponsesIt->second.find(
			ltpHeaderCtrlWe.ripd);
	// 0 does not exist
	if (pridMapIt == _windowEndedResponsesIt->second.end())
	{
		map<uint16_t, bool> sessionKeyMap;
		map<uint32_t, map<uint16_t, bool>> nidMap;
		list<NodeId>::iterator nidsIt;
		map<uint32_t, map<uint32_t, map<uint16_t, bool>>> pridMap;
		sessionKeyMap.insert(pair<uint16_t, bool>(ltpHeaderCtrlWe.sessionKey,
				false));
		for (nidsIt = nodeIds.begin(); nidsIt != nodeIds.end(); nidsIt++)
		{
			nidMap.insert(pair<uint32_t, map<uint16_t, bool>>(
					nidsIt->uint(), sessionKeyMap));
			oss << nidsIt->uint() << " ";
		}
		_windowEndedResponsesIt->second.insert(pair<uint32_t, map<uint32_t,
				map<uint16_t, bool>>>(ltpHeaderCtrlWe.ripd, nidMap));
		LOG4CXX_TRACE(logger, "New CTRL-WED boolean added for existing "
				"rCID " << rCid.print() << " but new 0 "
				<< ltpHeaderCtrlWe.ripd << " > NIDs " << oss.str()
				<< " > SK " << ltpHeaderCtrlWe.sessionKey);
		_windowEndedResponsesMutex.unlock();
		return;
	}
	// 0 exists
	//map<NID,	  map<SK
	map<uint32_t, map<uint16_t, bool>>::iterator nidMapIt;
	list<NodeId>::iterator nidsIt;
	for (nidsIt = nodeIds.begin(); nidsIt != nodeIds.end(); nidsIt++)
	{
		nidMapIt = pridMapIt->second.find(nidsIt->uint());
		// NID does not exist
		if (nidMapIt == pridMapIt->second.end())
		{	//map<SK
			map<uint16_t, bool> sessionKeyMap;
			sessionKeyMap.insert(pair<uint16_t, bool>(
					ltpHeaderCtrlWe.sessionKey, false));
			pridMapIt->second.insert(pair<uint32_t, map<uint16_t, bool>>(
					nidsIt->uint(), sessionKeyMap));
			LOG4CXX_TRACE(logger, "New CTRL-WED boolean added for existing "
					"rCID " << rCid.print() << " > 0 "
					<< ltpHeaderCtrlWe.ripd << " but new NID "
					<< nidsIt->uint() << " > SK "
					<< ltpHeaderCtrlWe.sessionKey);
		}
		//NID does exist
		else
		{	//map<SK
			map<uint16_t, bool>::iterator sessionKeyMapIt;
			sessionKeyMapIt = nidMapIt->second.find(ltpHeaderCtrlWe.sessionKey);
			// Session key does not exist
			if (sessionKeyMapIt == nidMapIt->second.end())
			{
				nidMapIt->second.insert(pair<uint16_t, bool>(
						ltpHeaderCtrlWe.sessionKey, false));
				LOG4CXX_TRACE(logger, "New CTRL-WED boolean added for existing "
						"rCID " << rCid.print() << " > 0 "
						<< ltpHeaderCtrlWe.ripd << " > NID "
						<< nidsIt->uint() << " but new SK "
						<< ltpHeaderCtrlWe.sessionKey);
			}
			// Session key does exist. Simply reset it to false
			else
			{
				sessionKeyMapIt->second = false;
				LOG4CXX_TRACE(logger, "Existing CTRL-WED boolean reset to "
						"false for existing rCID " << rCid.print() << " > 0 "
						<< ltpHeaderCtrlWe.ripd << " > NID "
						<< nidsIt->uint() << " > SK "
						<< ltpHeaderCtrlWe.sessionKey);
			}
		}
	}
	_windowEndedResponsesMutex.unlock();
}

void Lightweight::_bufferIcnPacket(IcnId &rCId, NodeId &nodeId,
		ltp_hdr_data_t &ltpHeader, uint8_t *packet)
{
	//map<Seq,	  Packet
	map<uint16_t, pair<uint8_t*, uint16_t>> sequenceMap;
	map<uint16_t, pair<uint8_t*, uint16_t>>::iterator sequenceMapIt;
	//map<SK,	  map<Seq,		Packet
	map<uint16_t, map<uint16_t, pair<uint8_t*, uint16_t>>> sessionKeyMap;
	map<uint16_t, map<uint16_t, pair<uint8_t*, uint16_t>>>::iterator
			sessionKeyMapIt;
	//map<NID,	  map<SK,		map<Seq,	  Packet
	map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t*, uint16_t>>>>
			nodeIdMap;
	map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t*,
			uint16_t>>>>::iterator nodeIdMapIt;
	//map<0,	  map<NID,	    map<SK,		  map<Seq,	    Packet
	map<uint32_t, map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t*,
			uint16_t>>>>> pridMap;
	map<uint32_t, map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t*,
			uint16_t>>>>>::iterator pridMapIt;
	pair<uint8_t *, uint16_t> packetDescriptor;
	packetDescriptor.first = (uint8_t *)malloc(ltpHeader.payloadLength);
	memcpy(packetDescriptor.first, packet, ltpHeader.payloadLength);
	packetDescriptor.second = ltpHeader.payloadLength;
	_icnPacketBufferMutex.lock();
	_icnPacketBufferIt = _icnPacketBuffer.find(rCId.uint());
	// First packet received for rCId
	if (_icnPacketBufferIt == _icnPacketBuffer.end())
	{
		// sequenceMap<Sequence, Packet>
		sequenceMap.insert(pair<uint16_t, pair<uint8_t*, uint16_t>>(
				ltpHeader.sequenceNumber, packetDescriptor));
		sessionKeyMap.insert(pair<uint16_t, map<uint16_t, pair<uint8_t*,
				uint16_t>>>(ltpHeader.sessionKey, sequenceMap));
		// nodeIdMap<NID, sequenceMap>
		nodeIdMap.insert(pair<uint32_t, map<uint16_t, map<uint16_t,
				pair<uint8_t*, uint16_t>>>>(nodeId.uint(), sessionKeyMap));
		// prIdMap<0, nodeIdMap>
		pridMap.insert(pair<uint32_t, map<uint32_t, map<uint16_t, map<uint16_t,
				pair<uint8_t*, uint16_t>>>>>(ltpHeader.ripd, nodeIdMap));
		// map<rCId, pridMap>
		_icnPacketBuffer.insert(pair<uint32_t, map<uint32_t, map<uint32_t,
				map<uint16_t, map<uint16_t, pair<uint8_t*, uint16_t>>>>>>
				(rCId.uint(), pridMap));
		LOG4CXX_TRACE(logger, "Packet of length " << ltpHeader.payloadLength
				<< " for new rCID " << rCId.print() << " added to LTP packet "
				"buffer for received ICN packets (0 "
				<< ltpHeader.ripd  << " > NID " << nodeId.uint()
				<< " > SK " << ltpHeader.sessionKey << " > Sequence "
				<< ltpHeader.sequenceNumber << ")");
		_icnPacketBufferMutex.unlock();
		return;
	}
	// rCId is known
	pridMapIt = _icnPacketBufferIt->second.find(ltpHeader.ripd);
	// 0 entry does not exist. Create it
	if (pridMapIt == _icnPacketBufferIt->second.end())
	{
		// sequenceMap<Sequence, Packet>
		sequenceMap.insert(pair<uint16_t, pair<uint8_t*, uint16_t>>(
				ltpHeader.sequenceNumber, packetDescriptor));
		sessionKeyMap.insert(pair<uint16_t, map<uint16_t, pair<uint8_t*,
				uint16_t>>>(ltpHeader.sessionKey, sequenceMap));
		// nodeIdMap<NID, sequenceMap>
		nodeIdMap.insert(pair<uint32_t, map<uint16_t, map<uint16_t,
				pair<uint8_t*, uint16_t>>>> (nodeId.uint(), sessionKeyMap));
		_icnPacketBufferIt->second.insert(pair<uint32_t, map<uint32_t,
				map<uint16_t, map<uint16_t, pair<uint8_t*, uint16_t>>>>>(
						ltpHeader.ripd,	nodeIdMap));
		LOG4CXX_TRACE(logger, "Packet of length " << ltpHeader.payloadLength
				<< " for existing rCID " << rCId.print() << " but new 0 "
				<< ltpHeader.ripd << " added to ICN buffer (NID "
				<< nodeId.uint() << " > SK "<< ltpHeader.sessionKey
				<< " > Sequence " << ltpHeader.sequenceNumber << ")");
		_icnPacketBufferMutex.unlock();
		return;
	}
	// 0 does exist
	nodeIdMapIt = pridMapIt->second.find(nodeId.uint());
	// This NID does not exist
	if (nodeIdMapIt == pridMapIt->second.end())
	{
		// sequenceMap<Sequence, Packet>
		sequenceMap.insert(pair<uint16_t, pair<uint8_t*, uint16_t>>(
				ltpHeader.sequenceNumber, packetDescriptor));
		sessionKeyMap.insert(pair<uint16_t, map<uint16_t, pair<uint8_t*,
				uint16_t>>>(ltpHeader.sessionKey, sequenceMap));
		// map<NID, sequenceMap>
		pridMapIt->second.insert(pair<uint32_t, map<uint16_t,
				map<uint16_t, pair<uint8_t*, uint16_t>>>>(nodeId.uint(),
						sessionKeyMap));
		LOG4CXX_TRACE(logger, "Packet of length "
				<< ltpHeader.payloadLength << " for existing rCID "
				<< rCId.print() << " > 0 " << ltpHeader.ripd
				<< " but new NID " << nodeId.uint() << " > SK "
				<< ltpHeader.sessionKey << " > Sequence "
				<< ltpHeader.sequenceNumber << " added to ICN buffer");
		_icnPacketBufferMutex.unlock();
		return;
	}
	// NID exists
	sessionKeyMapIt = nodeIdMapIt->second.find(ltpHeader.sessionKey);
	// Session key does not exist
	if (sessionKeyMapIt == nodeIdMapIt->second.end())
	{
		sequenceMap.insert(pair<uint16_t, pair<uint8_t*, uint16_t>>(
				ltpHeader.sequenceNumber, packetDescriptor));
		nodeIdMapIt->second.insert(pair<uint32_t, map<uint16_t, pair<uint8_t*,
				uint16_t>>>(ltpHeader.sessionKey, sequenceMap));
		LOG4CXX_TRACE(logger, "Packet of length " << ltpHeader.payloadLength
				<< " for existing rCID " << rCId.print() << " > 0 "
				<< ltpHeader.ripd << " > NID " << nodeId.uint()
				<< " but new SK " << ltpHeader.sessionKey	<< " > "
				"Sequence " << ltpHeader.sequenceNumber << " added to ICN "
				"packet buffer");
		_icnPacketBufferMutex.unlock();
		return;
	}
	// Session key does exist
	sequenceMapIt = sessionKeyMapIt->second.find(ltpHeader.sequenceNumber);
	// Session key exists -> check packet length is identical and leave
	if (sequenceMapIt != sessionKeyMapIt->second.end())
	{
		// check if length is identical
		if (sequenceMapIt->second.second == ltpHeader.payloadLength)
		{
			LOG4CXX_TRACE(logger, "Sequence " << ltpHeader.sequenceNumber
					<< " already exists for rCID " << rCId.print() << " > 0 "
					<< ltpHeader.ripd << " > NID " << nodeId.uint()
					<< " > Session " << ltpHeader.sessionKey);
			_icnPacketBufferMutex.unlock();
			return;
		}
		else
		{// not identical -> delete old packet and continue
			LOG4CXX_TRACE(logger, "Sequence " << ltpHeader.sequenceNumber
					<< " already exists for rCID " << rCId.print() << " > 0 "
					<< ltpHeader.ripd << " > NID " << nodeId.uint()
					<< " > Session " << ltpHeader.sessionKey << ". But packet "
					"size is different ... overwriting packet");
			free(sequenceMapIt->second.first);
		}
	}
	// Adding new packet
	sessionKeyMapIt->second.insert(pair<uint16_t, pair<uint8_t*,uint16_t>>
			(ltpHeader.sequenceNumber, packetDescriptor));
	LOG4CXX_TRACE(logger, "Packet of length " << ltpHeader.payloadLength
			<< " and Sequence " << ltpHeader.sequenceNumber << " added to ICN "
			"buffer for existing rCID " << rCId.print() << " > 0 "
			<< ltpHeader.ripd << ", NID " << nodeId.uint() << " Session "
			"key " << ltpHeader.sessionKey);
	_icnPacketBufferMutex.unlock();
}

void Lightweight::_bufferLtpPacket(IcnId &rCid, ltp_hdr_data_t &ltpHeaderData,
		uint8_t *packet, uint16_t packetSize)
{
	//map<rCID,   map<0,     map<SK,       map<SN        pair<Packet
	map<uint32_t, map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t *,
		uint16_t>>>>>::iterator rCidIt;
	//map<0,   map<SK,       map<SN        pair<Packet
	map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t *,
		uint16_t>>>>::iterator pridIt;
	//map<SK,     map<SN        pair<Packet
	map<uint16_t, map<uint16_t, pair<uint8_t *, uint16_t>>>::iterator skIt;
	//map<SN      pair<Packet
	map<uint16_t, pair<uint8_t *, uint16_t>>::iterator snIt;
	pair<uint8_t *, uint16_t> packetDescription;
	// Note, ltpHeaderData.payloadLength only has the HTTP msg. paketSize is
	// padded to be a multiple of 4!
	packetDescription.first = (uint8_t *)malloc(packetSize);
	memcpy(packetDescription.first, packet, packetSize);
	packetDescription.second = packetSize;
	_ltpPacketBufferMutex.lock();
	rCidIt = _ltpPacketBuffer.find(rCid.uint());
	// rCID not found
	if (rCidIt == _ltpPacketBuffer.end())
	{
		// Sequence
		map<uint16_t, pair<uint8_t *, uint16_t>> snMap;
		snMap.insert(pair<uint16_t, pair<uint8_t *, uint16_t>>(
				ltpHeaderData.sequenceNumber, packetDescription));
		// SK
		map<uint16_t, map<uint16_t, pair<uint8_t *, uint16_t>>> skMap;
		skMap.insert(pair<uint16_t, map<uint16_t, pair<uint8_t *, uint16_t>>>(
				ltpHeaderData.sessionKey, snMap));
		// 0
		map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t *,
				uint16_t>>>> pridMap;
		pridMap.insert(pair<uint32_t, map<uint16_t, map<uint16_t,
				pair<uint8_t *,	uint16_t>>>>(ltpHeaderData.ripd, skMap));
		// rCID
		_ltpPacketBuffer.insert(pair<uint32_t, map<uint32_t, map<uint16_t,
				map<uint16_t, pair<uint8_t *, uint16_t>>>>>(rCid.uint(),
						pridMap));
		_ltpPacketBufferMutex.unlock();
		LOG4CXX_TRACE(logger, "Sent LTP packet of length "
				<< ltpHeaderData.payloadLength << " and Sequence "
				<< ltpHeaderData.sequenceNumber << " buffered under new rCID "
				<< rCid.print() << " > 0 " << ltpHeaderData.ripd
				<< " > SK " << ltpHeaderData.sessionKey);
		return;
	}
	pridIt = rCidIt->second.find(ltpHeaderData.ripd);
	// 0 not found
	if (pridIt == rCidIt->second.end())
	{
		// Sequence
		map<uint16_t, pair<uint8_t *, uint16_t>> snMap;
		snMap.insert(pair<uint16_t, pair<uint8_t *, uint16_t>>(
				ltpHeaderData.sequenceNumber, packetDescription));
		// SK
		map<uint16_t, map<uint16_t, pair<uint8_t *, uint16_t>>> skMap;
		skMap.insert(pair<uint16_t, map<uint16_t, pair<uint8_t *, uint16_t>>>(
				ltpHeaderData.sessionKey, snMap));
		rCidIt->second.insert(pair<uint32_t, map<uint16_t, map<uint16_t,
				pair<uint8_t *, uint16_t>>>>(ltpHeaderData.ripd, skMap));
		_ltpPacketBufferMutex.unlock();
		LOG4CXX_TRACE(logger, "Sent LTP packet of length "
				<< ltpHeaderData.payloadLength << " and Sequence "
				<< ltpHeaderData.sequenceNumber << " buffered under known rCID "
				<< rCid.print() << " but new 0 " << ltpHeaderData.ripd
				<< " > SK " << ltpHeaderData.sessionKey);
		return;
	}
	skIt = pridIt->second.find(ltpHeaderData.sessionKey);
	// SK not found
	if (skIt == pridIt->second.end())
	{
		// Sequence
		map<uint16_t, pair<uint8_t *, uint16_t>> snMap;
		snMap.insert(pair<uint16_t, pair<uint8_t *, uint16_t>>(
				ltpHeaderData.sequenceNumber, packetDescription));
		pridIt->second.insert(pair<uint16_t, map<uint16_t, pair<uint8_t *,
				uint16_t>>>(ltpHeaderData.sessionKey, snMap));
		_ltpPacketBufferMutex.unlock();
		LOG4CXX_TRACE(logger, "Sent LTP packet of length "
				<< ltpHeaderData.payloadLength << " and Sequence "
				<< ltpHeaderData.sequenceNumber << " buffered under known rCID "
				<< rCid.print() << " > 0 " << ltpHeaderData.ripd
				<< " but new SK " << ltpHeaderData.sessionKey);
		return;
	}
	snIt = skIt->second.find(ltpHeaderData.sequenceNumber);
	// SN exists
	if (snIt != skIt->second.end())
	{
		_ltpPacketBufferMutex.unlock();
		LOG4CXX_TRACE(logger, "Sequence number " << ltpHeaderData.sequenceNumber
				<< " already exists in LTP buffer for rCID " << rCid.print()
				<< " > 0 " << ltpHeaderData.ripd << " > SK "
				<< ltpHeaderData.sessionKey);
		return;
	}
	skIt->second.insert(pair<uint16_t, pair<uint8_t *, uint16_t>>(
			ltpHeaderData.sequenceNumber, packetDescription));
	_ltpPacketBufferMutex.unlock();
	LOG4CXX_TRACE(logger, "Sent LTP packet of length "
			<< ltpHeaderData.payloadLength << " and Sequence "
			<< ltpHeaderData.sequenceNumber << " buffered under known rCID "
			<< rCid.print() << " > 0 " << ltpHeaderData.ripd
			<< " > SK " << ltpHeaderData.sessionKey);
}

void Lightweight::_bufferProxyPacket(IcnId &cId, IcnId &rCId,
		ltp_hdr_data_t &ltpHeaderData, uint8_t *packet,
		uint16_t &packetSize)
{
	pair<uint8_t *, uint16_t> packetDescriptor;
	_proxyPacketBufferMutex.lock();
	_proxyPacketBufferIt = _proxyPacketBuffer.find(rCId.uint());
	// rCID does not exist, create new entry
	if (_proxyPacketBufferIt == _proxyPacketBuffer.end())
	{
		/* First create the map with the sequence number as the key and the
		 * packet (pointer/size) as the map's value
		 */
		map<uint16_t, pair<uint8_t *, uint16_t>> sequenceMap;//map<Seq, PACKET>
		map<uint16_t, map<uint16_t, pair<uint8_t *, uint16_t>>> sessionKeyMap;
		map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t *, uint16_t>>>>
		pridMap;
		packetDescriptor.first = (uint8_t *)malloc(packetSize);
		memcpy(packetDescriptor.first, packet, packetSize);
		packetDescriptor.second = packetSize;
		sequenceMap.insert(pair<uint16_t, pair<uint8_t *, uint16_t>>(
				ltpHeaderData.sequenceNumber, packetDescriptor));
		// Secondly, create a new session key map
		sessionKeyMap.insert(pair<uint16_t, map<uint16_t, pair<uint8_t *,
				uint16_t>>>(ltpHeaderData.sessionKey, sequenceMap));
		// Thirdly, create a new 0 map with the session key Map above as its
		// value
		pridMap.insert(pair<uint32_t, map<uint16_t, map<uint16_t,
				pair<uint8_t *, uint16_t>>>>(ltpHeaderData.ripd,
						sessionKeyMap));
		/* Fourthly, create new rCID key entry in _proxyPacketBuffer map with
		 * the pridMap as its value
		 */
		_proxyPacketBuffer.insert(pair<uint32_t, map<uint32_t, map<uint16_t,
				map<uint16_t, pair<uint8_t *, uint16_t>>>>>(rCId.uint(),
						pridMap));
		LOG4CXX_TRACE(logger, "Packet of length " << packetSize << " published "
				"under " << cId.print() << " ("	<< cId.printFqdn() << ") added "
				"to LTP packet buffer with rCID " << rCId.print() << " > 0 "
				<< ltpHeaderData.ripd << " > Session "
				<< ltpHeaderData.sessionKey << " > Sequence "
				<< ltpHeaderData.sequenceNumber);
		_proxyPacketBufferMutex.unlock();
		return;
	}
	// rCID exists
	//map<0,   map<SK,       map<SN
	map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t *,
			uint16_t>>>>::iterator pridIt;
	pridIt = _proxyPacketBufferIt->second.find(ltpHeaderData.ripd);
	// 0 does not exist
	if (pridIt == _proxyPacketBufferIt->second.end())
	{
		//map<SN, 	  Packet
		map<uint16_t, pair<uint8_t *, uint16_t>> sequenceMap;
		//map<SK,     map<SN,       Packet
		map<uint16_t, map<uint16_t, pair<uint8_t *, uint16_t>>> sessionKeyMap;
		packetDescriptor.first = (uint8_t *)malloc(packetSize);
		memcpy(packetDescriptor.first, packet, packetSize);
		packetDescriptor.second = packetSize;
		sequenceMap.insert(pair<uint16_t, pair<uint8_t*, uint16_t>>(
				ltpHeaderData.sequenceNumber, packetDescriptor));
		sessionKeyMap.insert(pair<uint16_t, map<uint16_t, pair<uint8_t *,
				uint16_t>>>(ltpHeaderData.sessionKey, sequenceMap));
		_proxyPacketBufferIt->second.insert(pair<uint32_t, map<uint16_t,
				map<uint16_t, pair<uint8_t *, uint16_t>>>>(
						ltpHeaderData.ripd, sessionKeyMap));
		LOG4CXX_TRACE(logger, "Packet of length " << packetSize << " published "
				"under " << cId.print() << " ("	<< cId.printFqdn() << ") added "
				"to LTP packet buffer for existing rCID " << rCId.print()
				<< " but new 0 " << ltpHeaderData.ripd << " > SK "
				<< ltpHeaderData.sessionKey << " > SN "
				<< ltpHeaderData.sequenceNumber);
		_proxyPacketBufferMutex.unlock();
		return;
	}
	// 0 exists
	map<uint16_t, map<uint16_t, pair<uint8_t *,	uint16_t>>>::iterator skIt;
	skIt = pridIt->second.find(ltpHeaderData.sessionKey);
	// SK does not exist
	if (skIt == pridIt->second.end())
	{
		//map<SN, 	  Packet
		map<uint16_t, pair<uint8_t *, uint16_t>> sequenceMap;
		packetDescriptor.first = (uint8_t *)malloc(packetSize);
		memcpy(packetDescriptor.first, packet, packetSize);
		packetDescriptor.second = packetSize;
		sequenceMap.insert(pair<uint16_t, pair<uint8_t*, uint16_t>>(
				ltpHeaderData.sequenceNumber, packetDescriptor));
		pridIt->second.insert(pair<uint16_t, map<uint16_t, pair<uint8_t *,
				uint16_t>>>(ltpHeaderData.sessionKey, sequenceMap));
		LOG4CXX_TRACE(logger, "Packet of length " << packetSize << " published "
				"under " << cId.print() << " ("	<< cId.printFqdn() << ") added "
				"to LTP packet buffer for existing rCID " << rCId.print()
				<< " > 0 " << ltpHeaderData.ripd << " but new SK "
				<< ltpHeaderData.sessionKey << " > SN "
				<< ltpHeaderData.sequenceNumber);
		_proxyPacketBufferMutex.unlock();
		return;
	}
	// SK exists
	map<uint16_t, pair<uint8_t *,	uint16_t>>::iterator snIt;
	snIt = skIt->second.find(ltpHeaderData.sequenceNumber);
	// Sequence also exists?!?!
	if (snIt != skIt->second.end())
	{
		LOG4CXX_WARN(logger, " Sequence " << ltpHeaderData.sequenceNumber
				<< " already exists for rCID " << rCId.print() << " > 0 "
				<< ltpHeaderData.ripd << " > SK "
				<< ltpHeaderData.sessionKey);
		_proxyPacketBufferMutex.unlock();
		return;
	}
	packetDescriptor.first = (uint8_t *)malloc(packetSize);
	memcpy(packetDescriptor.first, packet, packetSize);
	packetDescriptor.second = packetSize;
	skIt->second.insert(pair<uint16_t, pair<uint8_t *, uint16_t>>(
			ltpHeaderData.sequenceNumber, packetDescriptor));
	LOG4CXX_TRACE(logger, "Packet with SN " << ltpHeaderData.sequenceNumber
			<< " and length " << packetSize << " published under "
			<< cId.print() << " ("	<< cId.printFqdn() << ") added to LTP "
			"packet buffer for existing rCID " << rCId.print() << " > 0 "
			<< ltpHeaderData.ripd << " > SK "
			<< ltpHeaderData.sessionKey);
	_proxyPacketBufferMutex.unlock();
}

void Lightweight::_deleteBufferedLtpPacket(IcnId &rCid, uint16_t &sessionKey)
{
	map<uint32_t, map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t *,
		uint16_t>>>>>::iterator rCidIt;
	_ltpPacketBufferMutex.lock();
	rCidIt = _ltpPacketBuffer.find(rCid.uint());
	if (rCidIt == _ltpPacketBuffer.end())
	{
		LOG4CXX_TRACE(logger, "rCID " << rCid.print() << " not found in LTP "
				"buffer");
		_ltpPacketBufferMutex.unlock();
		return;
	}
	map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t *,
		uint16_t>>>>::iterator pridIt;
	pridIt = rCidIt->second.find(0);
	if (pridIt == rCidIt->second.end())
	{
		LOG4CXX_TRACE(logger, "rCID " << rCid.print() << " > 0 0 not found "
				"in LTP buffer");
		_ltpPacketBufferMutex.unlock();
		return;
	}
	map<uint16_t, map<uint16_t, pair<uint8_t *, uint16_t>>>::iterator skIt;
	skIt = pridIt->second.find(sessionKey);
	if (skIt == pridIt->second.end())
	{
		LOG4CXX_TRACE(logger, "rCID " << rCid.print() << " > 0 > SK "
				<< sessionKey << " not found in LTP "
						"buffer");
		_ltpPacketBufferMutex.unlock();
		return;
	}
	// free up all allocated memory
	map<uint16_t, pair<uint8_t *, uint16_t>>::iterator seqIt;
	for (seqIt = skIt->second.begin(); seqIt != skIt->second.end(); seqIt++)
	{
		free(seqIt->second.first);
	}
	// erase entire session map
	pridIt->second.erase(skIt);
	LOG4CXX_TRACE(logger, "HTTP packet deleted from LTP packet store for rCID "
			<< rCid.print() << " > 0 > SK " << sessionKey);
	if (!pridIt->second.empty())
	{
		_ltpPacketBufferMutex.unlock();
		return;
	}
	LOG4CXX_TRACE(logger, "That was the last SK for rCID " << rCid.print()
			<< " > 0 in LTP packet store. Deleted 0");
	rCidIt->second.erase(pridIt);
	if (!rCidIt->second.empty())
	{
		_ltpPacketBufferMutex.unlock();
		return;
	}
	LOG4CXX_TRACE(logger, "That was the last 0 for rCID " << rCid.print()
			<< " in LTP packet store. Deleted rCID");
	_ltpPacketBuffer.erase(rCidIt);
	_ltpPacketBufferMutex.unlock();
}

void Lightweight::_deleteNackGroup(IcnId &rCid,
		ltp_hdr_ctrl_nack_t &ltpHeaderNack)
{
	_nackGroupsIt = _nackGroups.find(rCid.uint());
	// rCID not found
	if (_nackGroupsIt == _nackGroups.end())
	{
		LOG4CXX_DEBUG(logger, "No NACK group does exist for rCID "
				<< rCid.print());
		return;
	}
	map<uint32_t, map<uint16_t, nack_group_t>>::iterator pridIt;
	pridIt = _nackGroupsIt->second.find(ltpHeaderNack.ripd);
	// 0 not found
	if (pridIt == _nackGroupsIt->second.end())
	{
		LOG4CXX_DEBUG(logger, "No NACK group does exist for rCID "
				<< rCid.print() << " > 0 " << ltpHeaderNack.ripd);
		return;
	}
	map<uint16_t, nack_group_t>::iterator skIt;
	skIt = pridIt->second.find(ltpHeaderNack.sessionKey);
	// SK not found
	if (skIt == pridIt->second.end())
	{
		LOG4CXX_DEBUG(logger, "No NACK group does exist for rCID "
				<< rCid.print() << " > 0 " << ltpHeaderNack.ripd
				<< " > SK " << ltpHeaderNack.sessionKey);
		return;
	}
	pridIt->second.erase(skIt);
	LOG4CXX_TRACE(logger, "NACK group for rCID " << rCid.print() << " > 0 "
			<< ltpHeaderNack.ripd << " > SK "
			<< ltpHeaderNack.sessionKey << " deleted");
	if (pridIt->second.empty())
	{
		_nackGroupsIt->second.erase(pridIt);
		LOG4CXX_TRACE(logger, "This was the last SK for rCID " << rCid.print()
				<< " > 0 " << ltpHeaderNack.ripd << ". 0 deleted");
		if (_nackGroupsIt->second.empty())
		{
			_nackGroups.erase(_nackGroupsIt);
			LOG4CXX_TRACE(logger, "This was the last 0 for rCID "
					<< rCid.print() << ". rCID deleted");
		}
	}
}

void Lightweight::_deleteProxyPacket(IcnId &rCId, ltp_hdr_ctrl_wed_t &ltpHeader)
{
	// map<Sequence, Packet
	map<uint16_t, pair<uint8_t *, uint16_t>>::iterator sequenceMapIt;
	//map<SK,     map<Sequence, Packet
	map<uint16_t, map<uint16_t, pair<uint8_t *, uint16_t>>>::iterator
			sessionKeyMapIt;
	//map<0,   map<SK,       map<Sequence, Packet
	map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t *,
			uint16_t>>>>::iterator pridMapIt;
	_proxyPacketBufferMutex.lock();
	_proxyPacketBufferIt = _proxyPacketBuffer.find(rCId.uint());
	// rCID does not exist
	if (_proxyPacketBufferIt == _proxyPacketBuffer.end())
	{
		LOG4CXX_DEBUG(logger, "rCID " << rCId.print() << " cannot be found "
				"in proxy packet buffer map");
		_proxyPacketBufferMutex.unlock();
		return;
	}
	pridMapIt = _proxyPacketBufferIt->second.find(ltpHeader.ripd);
	// 0 does not exist
	if (pridMapIt == _proxyPacketBufferIt->second.end())
	{
		LOG4CXX_DEBUG(logger, "0 " << ltpHeader.ripd << " could "
				"not be found in proxy packet buffer map for rCID "
				<< rCId.print());
		_proxyPacketBufferMutex.unlock();
		return;
	}
	sessionKeyMapIt = pridMapIt->second.find(ltpHeader.sessionKey);
	// SK does not exist
	if (sessionKeyMapIt == pridMapIt->second.end())
	{
		LOG4CXX_DEBUG(logger, "SK " << ltpHeader.sessionKey << " could"
				" not be found in proxy packet buffer map for rCID "
				<< rCId.print() << " > 0 " << ltpHeader.ripd);
		_proxyPacketBufferMutex.unlock();
		return;
	}
	// deleting packet
	for (sequenceMapIt = sessionKeyMapIt->second.begin();
			sequenceMapIt != sessionKeyMapIt->second.end(); sequenceMapIt++)
	{
		free(sequenceMapIt->second.first);
	}
	pridMapIt->second.erase(sessionKeyMapIt);
	_proxyPacketBufferMutex.unlock();
	LOG4CXX_TRACE(logger, "Packet deleted from proxy packet buffer for rCID "
			<< rCId.print() << " > 0 " << ltpHeader.ripd
			<< " > SK " << ltpHeader.sessionKey);
}

void Lightweight::_deleteSessionEnd(IcnId &rCid,
		ltp_hdr_ctrl_se_t &ltpHdrCtrlSe, list<NodeId> &nodeIds)
{
	// 		u_map<SK
	unordered_map<uint16_t, bool>::iterator skIt;
	// 		u_map<NID, 				u_map<SK
	unordered_map<uint32_t, unordered_map<uint16_t, bool>>::iterator nidIt;
	// 		u_map<0, 			u_map<NID, 				u_map<SK
	unordered_map<uint32_t, unordered_map<uint32_t, unordered_map<uint16_t,
			bool>>>::iterator pridIt;
	_sessionEndedResponsesMutex.lock();
	_sessionEndedResponsesIt = _sessionEndedResponses.find(rCid.uint());
	// rCID does not exist
	if (_sessionEndedResponsesIt == _sessionEndedResponses.end())
	{
		LOG4CXX_DEBUG(logger, "rCID " << rCid.print() << " does not exist in "
				"SED responses map");
		_sessionEndedResponsesMutex.unlock();
		return;
	}
	pridIt = _sessionEndedResponsesIt->second.find(ltpHdrCtrlSe.ripd);
	// 0 does not exist
	if (pridIt == _sessionEndedResponsesIt->second.end())
	{
		LOG4CXX_DEBUG(logger, "rCID " << rCid.print() << " > 0 "
				<< ltpHdrCtrlSe.ripd << " does not exist in SED"
						" responses map");
		_sessionEndedResponsesMutex.unlock();
		return;
	}
	// iterate over NIDs
	list<NodeId> nodeIdsToBeDeleted;
	for (auto it = nodeIds.begin(); it != nodeIds.end(); it++)
	{
		nidIt = pridIt->second.find(it->uint());
		// NID found
		if (nidIt != pridIt->second.end())
		{
			skIt = nidIt->second.find(ltpHdrCtrlSe.sessionKey);
			// SK found, delete it
			if (skIt != nidIt->second.end())
			{
				nidIt->second.erase(skIt);
				LOG4CXX_TRACE(logger, "SK " << ltpHdrCtrlSe.sessionKey
						<< " deleted for rCID " << rCid.print() << " > 0 "
						<< ltpHdrCtrlSe.ripd << " NID "
						<< it->uint() << " from CTRL-SED responses map");
				// if this was the last SK for this NID, delete it
				if (nidIt->second.empty())
				{
					LOG4CXX_TRACE(logger, "NID " << it->uint() << " deleted for"
							" rCID " << rCid.print() << " > 0 "
							<< ltpHdrCtrlSe.ripd << " from CTRL-SED"
									" responses map");
					pridIt->second.erase(nidIt);
				}
			}
		}
	}
	// Now check for empty 0 and rCID values and erase them if necessary
	// No NIDs left
	if (pridIt->second.empty())
	{
		LOG4CXX_TRACE(logger, "0 " << ltpHdrCtrlSe.ripd << " deleted "
				"for rCID " << rCid.print() << " from CTRL-SED responses map");
		_sessionEndedResponsesIt->second.erase(pridIt);
	}
	// No 0s left
	if (_sessionEndedResponsesIt->second.empty())
	{
		LOG4CXX_TRACE(logger, "rCID " << rCid.print() << " deleted from "
				"CTRL-SED responses map");
		_sessionEndedResponses.erase(_sessionEndedResponsesIt);
	}
	_sessionEndedResponsesMutex.unlock();
}

void Lightweight::_enableNIdInCmcGroup(IcnId &rCId, NodeId &nodeId)
{
	_potentialCmcGroupsMutex->lock();
	_potentialCmcGroupsIt = _potentialCmcGroups->find(rCId.uint());
	if (_potentialCmcGroupsIt == _potentialCmcGroups->end())
	{
		LOG4CXX_TRACE(logger, "rCID " << rCId.print() << " could not be found "
				"in potential CMC group map");
		_potentialCmcGroupsMutex->unlock();
		return;
	}
	map<uint32_t, map<uint32_t, bool>>::iterator proxyRuleIdMapIt;
	proxyRuleIdMapIt = _potentialCmcGroupsIt->second.find(0);
	if (proxyRuleIdMapIt == _potentialCmcGroupsIt->second.end())
	{
		LOG4CXX_TRACE(logger, "0 could not be found for rCID " << rCId.print()
				<< " in potential CMC group map");
		_potentialCmcGroupsMutex->unlock();
		return;
	}
	map<uint32_t, bool>::iterator nodeIdMapIt;
	nodeIdMapIt = proxyRuleIdMapIt->second.find(nodeId.uint());
	if (nodeIdMapIt == proxyRuleIdMapIt->second.end())
	{
		LOG4CXX_TRACE(logger, "NID " << nodeId.uint() << " could not be found "
				<< " and " << " rCID " << rCId.print() << " in potential CMC "
						"group map");
		_potentialCmcGroupsMutex->unlock();
		return;
	}
	nodeIdMapIt->second = true;
	LOG4CXX_DEBUG(logger, "NID " << nodeId.uint() << " enabled in potential CMC"
			" group");
	_potentialCmcGroupsMutex->unlock();
}

bool Lightweight::_forwardingEnabled(NodeId &nodeId)
{
	bool state;
	_knownNIdsMutex->lock();
	_knownNIdsIt = _knownNIds->find(nodeId.uint());
	if (_knownNIdsIt == _knownNIds->end())
	{
		LOG4CXX_DEBUG(logger, "NID " << nodeId.uint() << " doesn't exist in "
				"list of known NIDs");
		_knownNIdsMutex->unlock();
		return false;
	}
	state = _knownNIdsIt->second;
	_knownNIdsMutex->unlock();
	return state;
}

bool Lightweight::_getCmcGroup(IcnId &rCid, uint16_t &sessionKey,
		list<NodeId> &cmcGroup)
{
	//map<0,   map<SK,       list<NID
	map<uint32_t, map<uint16_t, list<NodeId>>>::iterator cmcPridIt;
	//map<SK,     list<NID
	map<uint16_t, list<NodeId>>::iterator cmcSkIt;
	_cmcGroupsIt = _cmcGroups->find(rCid.uint());
	// rCID does not exist
	if (_cmcGroupsIt == _cmcGroups->end())
	{
		LOG4CXX_TRACE(logger, "CMC group with rCID "
				<< rCid.print() << " does not exist (anymore)");
		return false;
	}
	cmcPridIt = _cmcGroupsIt->second.find(0);
	// does not exist
	if (cmcPridIt == _cmcGroupsIt->second.end())
	{
		LOG4CXX_TRACE(logger, "CMC group cannot be found  (anymore) for rCID "
				<< rCid.print());
		return false;
	}//0 exists
	cmcSkIt = cmcPridIt->second.find(sessionKey);
	// SK does not exist
	if (cmcSkIt == cmcPridIt->second.end())
	{
		LOG4CXX_TRACE(logger, "CMC group cannot be found  (anymore) for rCID "
				<< rCid.print() << " > SK "
				<< sessionKey);
		return false;
	}
	cmcGroup = cmcSkIt->second;
	_statistics.cmcGroupSize(cmcGroup.size());
	return true;
}

TpState Lightweight::_handleControl(IcnId &rCId, uint8_t *packet,
		uint16_t &sessionKey)
{
	ltp_hdr_ctrl_t ltpHeaderCtrl;
	// [1] message type
	packet += sizeof(ltpHeaderCtrl.messageType);
	// [2] control type
	memcpy(&ltpHeaderCtrl.controlType, packet,
			sizeof(ltpHeaderCtrl.controlType));
	packet += sizeof(ltpHeaderCtrl.controlType);
	switch (ltpHeaderCtrl.controlType)
	{
	case LTP_CONTROL_NACK:
	{// HTTP request wasn't fully received by sNAP
		ltp_hdr_ctrl_nack_t ltpHeaderNack;
		// [3] 0
		memcpy(&ltpHeaderNack.ripd, packet,
				sizeof(ltpHeaderNack.ripd));
		packet += sizeof(ltpHeaderNack.ripd);
		// [4] SK
		memcpy(&ltpHeaderNack.sessionKey, packet,
				sizeof(ltpHeaderNack.sessionKey));
		packet += sizeof(ltpHeaderNack.sessionKey);
		// [5] Start
		memcpy(&ltpHeaderNack.start, packet,
				sizeof(ltpHeaderNack.start));
		packet += sizeof(ltpHeaderNack.start);
		// [6] End
		memcpy(&ltpHeaderNack.end, packet, sizeof(ltpHeaderNack.end));
		LOG4CXX_TRACE(logger, "CTRL-NACK for Segments " << ltpHeaderNack.start
				<< " - " << ltpHeaderNack.end << " received under rCID "
				<< rCId.print() << " > 0 " << ltpHeaderNack.ripd
				<< " > SK " << ltpHeaderNack.sessionKey);
		_publishDataRange(rCId, ltpHeaderNack);
		break;
	}
	case LTP_CONTROL_SESSION_END:
	{
		// [3]
		packet += sizeof(uint32_t);
		// [4] SK
		memcpy(&sessionKey, packet,	sizeof(sessionKey));
		IcnId cid;
		LOG4CXX_TRACE(logger, "LTP CTRL-SE received for rCID " << rCId.print()
				<< " > SK " << sessionKey);
		_cIdReverseLookUpMutex.lock();
		_cIdReverseLookUpIt = _cIdReverseLookUp.find(rCId.uint());
		if (_cIdReverseLookUpIt == _cIdReverseLookUp.end())
		{
			LOG4CXX_TRACE(logger, "rCID " << rCId.print() << " does not "
					"exist as key in CID look-up table (anymore)");
			_cIdReverseLookUpMutex.unlock();
			return TP_STATE_SESSION_ENDED;
		}
		cid = _cIdReverseLookUpIt->second;
		_cIdReverseLookUpMutex.unlock();
		_publishSessionEnded(cid, rCId, sessionKey);
		return TP_STATE_SESSION_ENDED;
	}
	case LTP_CONTROL_WINDOW_END:
	{
		ltp_hdr_ctrl_we_t ltpHeaderCtrlWe;
		map<uint16_t, pair<uint8_t*, uint16_t>>::iterator sequenceMapIt;
		map<uint16_t, map<uint16_t, pair<uint8_t*, uint16_t>>>::iterator
				sessionKeyMapIt;
		map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t*,
				uint16_t>>>>::iterator nodeIdMapIt;
		map<uint32_t, map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t*,
				uint16_t>>>>>::iterator pridMapIt;
		// [3] 0
		memcpy(&ltpHeaderCtrlWe.ripd, packet,
				sizeof(ltpHeaderCtrlWe.ripd));
		packet += sizeof(ltpHeaderCtrlWe.ripd);
		// [4] Session key
		memcpy(&ltpHeaderCtrlWe.sessionKey, packet,
				sizeof(ltpHeaderCtrlWe.sessionKey));
		packet += sizeof(ltpHeaderCtrlWe.sessionKey);
		// [5] Sequence number
		memcpy(&ltpHeaderCtrlWe.sequenceNumber, packet,
				sizeof(ltpHeaderCtrlWe.sequenceNumber));
		LOG4CXX_TRACE(logger, "LTP CTRL-WE received for rCID " << rCId.print()
				<< " > 0 " << ltpHeaderCtrlWe.ripd << " > Session "
				"key " << ltpHeaderCtrlWe.sessionKey << " > Seq "
				<< ltpHeaderCtrlWe.sequenceNumber);
		/* Check in ICN packet buffer that all sequences starting from one up to
		 * the one received here have been received
		 */
		_icnPacketBufferMutex.lock();
		// Find rCID
		_icnPacketBufferIt = _icnPacketBuffer.find(rCId.uint());
		if (_icnPacketBufferIt == _icnPacketBuffer.end())
		{
			LOG4CXX_DEBUG(logger, "rCID " << rCId.print() << " unknown. "
					"Cannot check if CTRL-WED should be sent");
			_icnPacketBufferMutex.unlock();
			break;
		}
		// Find 0
		pridMapIt = _icnPacketBufferIt->second.find(
				ltpHeaderCtrlWe.ripd);
		if (pridMapIt == _icnPacketBufferIt->second.end())
		{
			LOG4CXX_DEBUG(logger, "0 " << ltpHeaderCtrlWe.ripd
					<< " unknown. Cannot check if CTRL-WED should be sent");
			_icnPacketBufferMutex.unlock();
			break;
		}
		// Find NID (for packet sent through CMC NID is always the NID of this
		// NAP)
		nodeIdMapIt = pridMapIt->second.find(_configuration.nodeId().uint());
		// NID does not exist
		if (nodeIdMapIt == pridMapIt->second.end())
		{
			LOG4CXX_TRACE(logger, "NID " << _configuration.nodeId().uint()
					<< " not known (anymore) in LTP packet store. Cannot check "
					"if WED CTRL should be sent ... sending one anyway");
			_icnPacketBufferMutex.unlock();
			IcnId cid;
			_cIdReverseLookUpMutex.lock();
			_cIdReverseLookUpIt = _cIdReverseLookUp.find(rCId.uint());
			if (_cIdReverseLookUpIt == _cIdReverseLookUp.end())
			{
				LOG4CXX_WARN(logger, "rCID " << rCId.print() << " does not "
						"exist as key in CID look-up table");
				_cIdReverseLookUpMutex.unlock();
				return TP_STATE_NO_ACTION_REQUIRED;
			}
			cid = _cIdReverseLookUpIt->second;
			_cIdReverseLookUpMutex.unlock();
			ltp_hdr_ctrl_wed_t ltpHeaderCtrlWed;
			ltpHeaderCtrlWed.ripd = ltpHeaderCtrlWe.ripd;
			ltpHeaderCtrlWed.sessionKey = ltpHeaderCtrlWe.sessionKey;
			_publishWindowEnded(cid, rCId, ltpHeaderCtrlWed);
			break;
		}
		// Find session
		sessionKeyMapIt = nodeIdMapIt->second.find(ltpHeaderCtrlWe.sessionKey);
		if (sessionKeyMapIt == nodeIdMapIt->second.end())
		{
			LOG4CXX_DEBUG(logger, "SK " << ltpHeaderCtrlWe.sessionKey
					<< " unknown. Cannot check if WED CTRL should be sent");
			_icnPacketBufferMutex.unlock();
			break;
		}
		// Check that consecutive sequence numbers exist
		bool allFragmentsReceived = true;
		uint16_t firstMissingSequence = 0;
		uint16_t lastMissingSequence = 0;
		uint16_t previousSequence = 0;
		for (sequenceMapIt = sessionKeyMapIt->second.begin();
				sequenceMapIt != sessionKeyMapIt->second.end(); sequenceMapIt++)
		{
			// First fragment is missing
			if (previousSequence == 0 && sequenceMapIt->first != 1)
			{
				firstMissingSequence = 1;
				lastMissingSequence = 1;
				cout << "both set to 1\n";
				allFragmentsReceived = false;
			}
			// Not first, but any other sequence is missing
			else if ((previousSequence + 1) < sequenceMapIt->first)
			{
				if (firstMissingSequence == 0)
				{
					firstMissingSequence = previousSequence + 1;
					cout << "firstMissingSequence set to "
							<< firstMissingSequence << endl;
				}
				lastMissingSequence = sequenceMapIt->first - 1;
				cout << "lastMissingSequence set to " << lastMissingSequence
						<< endl;
				allFragmentsReceived = false;
			}
			previousSequence = sequenceMapIt->first;

		}
		// check if sequence indicated in CTRL-WE was actually received
		map<uint16_t, pair<uint8_t*, uint16_t>>::reverse_iterator sequenceMapRIt;
		sequenceMapRIt = sessionKeyMapIt->second.rbegin();
		if (sequenceMapRIt->first < ltpHeaderCtrlWe.sequenceNumber)
		{
			if (firstMissingSequence == 0)
			{
				firstMissingSequence = ltpHeaderCtrlWe.sequenceNumber;
				LOG4CXX_TRACE(logger, "First missing sequence changed to "
						<< firstMissingSequence);
			}
			lastMissingSequence = ltpHeaderCtrlWe.sequenceNumber;
			LOG4CXX_TRACE(logger, "Last missing sequence changed to "
					<< ltpHeaderCtrlWe.sequenceNumber);
			allFragmentsReceived = false;
		}
		_icnPacketBufferMutex.unlock();
		IcnId cid;
		_cIdReverseLookUpMutex.lock();
		_cIdReverseLookUpIt = _cIdReverseLookUp.find(rCId.uint());
		if (_cIdReverseLookUpIt == _cIdReverseLookUp.end())
		{
			LOG4CXX_WARN(logger, "rCID " << rCId.print() << " does not exist as"
					" key in CID look-up table. This must not happen");
			_cIdReverseLookUpMutex.unlock();
			return TP_STATE_NO_ACTION_REQUIRED;
		}
		cid = _cIdReverseLookUpIt->second;
		_cIdReverseLookUpMutex.unlock();
		if (allFragmentsReceived)
		{
			ltp_hdr_ctrl_wed_t ltpHeaderCtrlWed;
			ltpHeaderCtrlWed.ripd = ltpHeaderCtrlWe.ripd;
			ltpHeaderCtrlWed.sessionKey = ltpHeaderCtrlWe.sessionKey;
			_publishWindowEnded(cid, rCId, ltpHeaderCtrlWed);
			sessionKey = ltpHeaderCtrlWe.sessionKey;
			return TP_STATE_ALL_FRAGMENTS_RECEIVED;
		}
		// Sequence was missing. Send off NACK message
		else
		{
			LOG4CXX_TRACE(logger, "Missing packets for rCID " << rCId.print()
					<< " > SK " << ltpHeaderCtrlWe.sessionKey << ". First: "
					<< firstMissingSequence << ", Last: "
					<< lastMissingSequence);
			_publishNegativeAcknowledgement(cid, rCId,
					ltpHeaderCtrlWe.sessionKey, firstMissingSequence,
					lastMissingSequence);
		}
		break;
	}
	case LTP_CONTROL_WINDOW_ENDED:
	{
		map<uint16_t, bool>::iterator sessionKeyMapIt;
		map<uint32_t, map<uint16_t, bool>>::iterator pridMapIt;
		ltp_hdr_ctrl_wed_t ltpHeader;
		// [3]
		memcpy(&ltpHeader.ripd, packet, sizeof(ltpHeader.ripd));
		packet += sizeof(ltpHeader.ripd);
		// [4] Session key
		memcpy(&ltpHeader.sessionKey, packet, sizeof(ltpHeader.sessionKey));
		LOG4CXX_TRACE(logger, "LTP CTRL-WED received for rCID " << rCId.print()
				<< " > 0 " << ltpHeader.ripd << " > SK "
				<< ltpHeader.sessionKey);
		_windowEndedRequestsMutex.lock();
		// Check for CID
		_windowEndedRequestsIt = _windowEndedRequests.find(rCId.uint());
		if (_windowEndedRequestsIt == _windowEndedRequests.end())
		{
			LOG4CXX_DEBUG(logger, "rCID " << rCId.print() << " could not be "
					"found in _windowEndedRequests map");
			_windowEndedRequestsMutex.unlock();
			break;
		}
		// Check for 0
		pridMapIt = _windowEndedRequestsIt->second.find(ltpHeader.ripd);
		if (pridMapIt == _windowEndedRequestsIt->second.end())
		{
			LOG4CXX_DEBUG(logger, "0 " << ltpHeader.ripd << " does "
					"not exist in _windowEndedRequests map for rCID "
					<< rCId.print());
			_windowEndedRequestsMutex.unlock();
			break;
		}
		// Check SK
		sessionKeyMapIt = pridMapIt->second.find(ltpHeader.sessionKey);
		if (sessionKeyMapIt == pridMapIt->second.end())
		{
			LOG4CXX_DEBUG(logger, "SK " << ltpHeader.sessionKey
					<< " could not be found in _windowEndedRequests map for "
					"rCID " << rCId.print()	<< " > 0 "
					<< ltpHeader.ripd);
			_windowEndedRequestsMutex.unlock();
			break;
		}
		sessionKeyMapIt->second = true;
		LOG4CXX_TRACE(logger, "CTRL-WED flag set to true in _windowEndedRequest"
				"s map for rCID " << rCId.print() << " > 0 "
				<< ltpHeader.ripd << " > SK " << ltpHeader.sessionKey);
		_windowEndedRequestsMutex.unlock();
		_deleteProxyPacket(rCId, ltpHeader);
		break;
	}
	case LTP_CONTROL_WINDOW_UPDATE:
	{
		// [3] 0
		memcpy(&ltpHeaderCtrl.ripd, packet,
				sizeof(ltpHeaderCtrl.ripd));
		packet += sizeof(ltpHeaderCtrl.ripd);
		// [4] SK
		memcpy(&ltpHeaderCtrl.sessionKey, packet,
				sizeof(ltpHeaderCtrl.sessionKey));
		LOG4CXX_TRACE(logger, "LTP CTRL-WU received for rCID " << rCId.print()
				<< " > 0 " << ltpHeaderCtrl.ripd << " > SK "
				<< ltpHeaderCtrl.sessionKey)
		IcnId cid;
		ltp_hdr_ctrl_wud_t ltpHeaderCtrlWud;
		ltpHeaderCtrlWud.ripd = ltpHeaderCtrl.ripd;
		ltpHeaderCtrlWud.sessionKey = ltpHeaderCtrl.sessionKey;
		_cIdReverseLookUpMutex.lock();
		_cIdReverseLookUpIt = _cIdReverseLookUp.find(rCId.uint());
		if (_cIdReverseLookUpIt == _cIdReverseLookUp.end())
		{
			LOG4CXX_WARN(logger, "rCID " << rCId.print() << " does not "
					"exist as key in CID look-up table. This must not "
					"happen");
			_cIdReverseLookUpMutex.unlock();
			return TP_STATE_NO_ACTION_REQUIRED;
		}
		cid = _cIdReverseLookUpIt->second;
		_cIdReverseLookUpMutex.unlock();
		_publishWindowUpdated(cid, rCId, ltpHeaderCtrlWud);
		return TP_STATE_NO_ACTION_REQUIRED;
	}
	case LTP_CONTROL_WINDOW_UPDATED:
		// [3] 0
		memcpy(&ltpHeaderCtrl.ripd, packet,
				sizeof(ltpHeaderCtrl.ripd));
		packet += sizeof(ltpHeaderCtrl.ripd);
		LOG4CXX_TRACE(logger, "LTP CTRL-WUD for 0 "
				<< ltpHeaderCtrl.ripd << " received under CID "
				<< rCId.print());
		break;
	default:
		LOG4CXX_TRACE(logger, "Unknown LTP CTRL type received for rCID "
				<< rCId.print());
	}
	return TP_STATE_NO_ACTION_REQUIRED;
}

TpState Lightweight::_handleControl(IcnId &cId, IcnId &rCId, NodeId &nodeId,
		uint8_t *packet, uint16_t &sessionKey)
{
	ltp_hdr_ctrl_t ltpHeader;
	uint16_t offset = 0;
	offset += sizeof(ltpHeader.messageType);
	// First get the LTP control type
	// [2] control type
	memcpy(&ltpHeader.controlType, packet + offset,
			sizeof(ltpHeader.controlType));
	offset += sizeof(ltpHeader.controlType);
	switch (ltpHeader.controlType)
	{
	case LTP_CONTROL_NACK:
	{
		ltp_hdr_ctrl_nack_t ltpHeaderNack;
		// [3] 0
		memcpy(&ltpHeaderNack.ripd, packet + offset,
				sizeof(ltpHeaderNack.ripd));
		offset += sizeof(ltpHeaderNack.ripd);
		// [4] SK
		memcpy(&ltpHeaderNack.sessionKey, packet + offset,
				sizeof(ltpHeaderNack.sessionKey));
		offset += sizeof(ltpHeaderNack.sessionKey);
		// [5] Start
		memcpy(&ltpHeaderNack.start, packet + offset,
				sizeof(ltpHeaderNack.start));
		offset += sizeof(ltpHeaderNack.start);
		// [6] End
		memcpy(&ltpHeaderNack.end, packet + offset, sizeof(ltpHeaderNack.end));
		LOG4CXX_TRACE(logger, "CTRL-NACK for Segments " << ltpHeaderNack.start
				<< " - " << ltpHeaderNack.end << " received under CID "
				<< cId.print() << " for rCID " << rCId.print() << " > 0 "
				<< ltpHeaderNack.ripd << " > SK "
				<< ltpHeaderNack.sessionKey);
		// first add the NID and the start/end
		_nackGroupsMutex.lock();
		_addNackNodeId(rCId, ltpHeaderNack, nodeId);
		// re-publish the requested range of segments
		// first check if a response from all nodes has been received. Find all
		// NID in the CMC group and check if this NID is the last one the sNAP
		// was waiting for
		list<NodeId> cmcGroup;
		_cmcGroupsMutex->lock();
		if (!_getCmcGroup(rCId, ltpHeaderNack.sessionKey, cmcGroup))
		{
			LOG4CXX_ERROR(logger, "CMC group could not be obtained");
			_cmcGroupsMutex->unlock();
			_nackGroupsMutex.unlock();
			return TP_STATE_NO_ACTION_REQUIRED;
		}
		// SK exists
		// now get the iterator to the WED response map to check for the NIDs
		// map<Session, WED received
		map<uint16_t, bool>::iterator wedSkIt;
		//map<NID     map<Session,  WED received
		map<uint32_t, map<uint16_t, bool>>::iterator wedNidIt;
		//map<0,   map<NID       map<Session,  WED received
		map<uint32_t, map<uint32_t, map<uint16_t, bool>>>::iterator wedPridIt;
		_windowEndedResponsesMutex.lock();
		_windowEndedResponsesIt = _windowEndedResponses.find(rCId.uint());
		// rCID does not exist in WED map
		if (_windowEndedResponsesIt == _windowEndedResponses.end())
		{
			LOG4CXX_TRACE(logger, "rCID " << rCId.print() << " does not exist in"
					" sent CTRL-WEs map");
			_nackGroupsMutex.unlock();
			_windowEndedResponsesMutex.unlock();
			_cmcGroupsMutex->unlock();
			return TP_STATE_NO_ACTION_REQUIRED;
		}
		wedPridIt = _windowEndedResponsesIt->second.find(
				ltpHeaderNack.ripd);
		// 0 does not exist in WED map
		if (wedPridIt == _windowEndedResponsesIt->second.end())
		{
			LOG4CXX_TRACE(logger, "0 " << ltpHeaderNack.ripd
					<< " could not be found in sent CTRL-WEs map");
			_nackGroupsMutex.unlock();
			_windowEndedResponsesMutex.unlock();
			_cmcGroupsMutex->unlock();
			return TP_STATE_NO_ACTION_REQUIRED;
		}
		// find a particular node and check if WED was received
		list<NodeId>::iterator cmcGroupIt;
		uint16_t confirmedWeds = 0;
		for (cmcGroupIt = cmcGroup.begin();	cmcGroupIt != cmcGroup.end();
				cmcGroupIt++)
		{
			// Find CMC NID in WED NID map
			wedNidIt = wedPridIt->second.find(cmcGroupIt->uint());
			if (wedNidIt != wedPridIt->second.end())
			{
				wedSkIt = wedNidIt->second.find(ltpHeaderNack.sessionKey);
				// SK exists in WED map
				if (wedSkIt != wedNidIt->second.end())
				{
					// check if WED has been received for this NID
					if (wedSkIt->second)
					{
						confirmedWeds++;
					}
				}
			}
		}
		// If sum of received WEDs + NACKs == CMC group size, send range of NACK
		// fragments
		nack_group_t nackGroup = _nackGroup(rCId, ltpHeaderNack);
		if (cmcGroup.size() > (confirmedWeds + nackGroup.nodeIds.size()))
		{
			_nackGroupsMutex.unlock();
			_windowEndedResponsesMutex.unlock();
			_cmcGroupsMutex->unlock();
			return TP_STATE_NO_ACTION_REQUIRED;
		}
		//all nodes have replied to the WE msg with either WED or NACK. Publish
		//range of segments
		_windowEndedResponsesMutex.unlock();
		_cmcGroupsMutex->unlock();
		_publishDataRange(rCId, ltpHeaderNack.sessionKey, nackGroup);
		// delete NACK group
		_deleteNackGroup(rCId, ltpHeaderNack);
		_nackGroupsMutex.unlock();
		break;
	}
	case LTP_CONTROL_SESSION_ENDED:
	{
		unordered_map<uint16_t, bool>::iterator skIt;
		unordered_map<uint32_t, unordered_map<uint16_t, bool>>::iterator nidIt;
		unordered_map<uint32_t, unordered_map<uint32_t, unordered_map<uint16_t,
			bool>>>::iterator pridIt;
		// [3]
		offset += sizeof(uint32_t);
		// [4] SK
		memcpy(&sessionKey, packet + offset, sizeof(sessionKey));
		LOG4CXX_TRACE(logger, "CTRL-SED received for CID " << cId.print()
				<< " rCID " << rCId.print() << " > 0 "
				<< " > SK " << sessionKey);
		_sessionEndedResponsesMutex.lock();
		_sessionEndedResponsesIt = _sessionEndedResponses.find(rCId.uint());
		// rCID does not exist
		if (_sessionEndedResponsesIt == _sessionEndedResponses.end())
		{
			LOG4CXX_DEBUG(logger, "rCID " << rCId.print() << " does not exist in"
					" LTP CTRL SED map (anymore). Dropping packet");
			_sessionEndedResponsesMutex.unlock();
			return TP_STATE_NO_ACTION_REQUIRED;
		}
		pridIt = _sessionEndedResponsesIt->second.find(0);
		// 0 does not exist
		if (pridIt == _sessionEndedResponsesIt->second.end())
		{
			_sessionEndedResponsesMutex.unlock();
			return TP_STATE_NO_ACTION_REQUIRED;
		}
		nidIt = pridIt->second.find(nodeId.uint());
		// NID does not exist
		if (nidIt == pridIt->second.end())
		{
			LOG4CXX_DEBUG(logger, "rCID " << rCId.print() << " > 0 "
					<< " > NID " << nodeId.uint() << " does not "
					"exist in LTP CTRL SED map (anymore). Dropping packet");
			_sessionEndedResponsesMutex.unlock();
			return TP_STATE_NO_ACTION_REQUIRED;
		}
		skIt = nidIt->second.find(sessionKey);
		if (skIt == nidIt->second.end())
		{
			LOG4CXX_DEBUG(logger, "rCID " << rCId.print() << " > 0 "
					<< " > NID " << nodeId.uint() << " > SK "
					<< sessionKey << " does not exist in LTP CTRL SED map "
							"(anymore). Dropping packet");
			_sessionEndedResponsesMutex.unlock();
			return TP_STATE_NO_ACTION_REQUIRED;
		}
		skIt->second = true;
		LOG4CXX_TRACE(logger, "SED received flag set to true for rCID "
				<< rCId.print() << " > 0 " << " > NID "
				<< nodeId.uint() << " > SK "<< sessionKey);
		_sessionEndedResponsesMutex.unlock();
		break;
	}
	case LTP_CONTROL_WINDOW_END:
	{
		ltp_hdr_ctrl_we_t ltpHeaderControlWe;
		map<uint16_t, pair<uint8_t*, uint16_t>>::iterator sequenceMapIt;
		map<uint16_t, map<uint16_t, pair<uint8_t*, uint16_t>>>::iterator
				sessionKeyMapIt;
		map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t*,
				uint16_t>>>>::iterator nodeIdMapIt;
		map<uint32_t, map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t*,
				uint16_t>>>>>::iterator pridMapIt;
		// [3] 0
		memcpy(&ltpHeaderControlWe.ripd, packet + offset,
				sizeof(ltpHeaderControlWe.ripd));
		offset += sizeof(ltpHeaderControlWe.ripd);
		// [4] Session key
		memcpy(&ltpHeaderControlWe.sessionKey, packet + offset,
				sizeof(ltpHeaderControlWe.sessionKey));
		offset += sizeof(ltpHeaderControlWe.sessionKey);
		// [5] Sequence number
		memcpy(&ltpHeaderControlWe.sequenceNumber, packet + offset,
				sizeof(ltpHeaderControlWe.sequenceNumber));
		LOG4CXX_TRACE(logger, "LTP CTRL-WE (SK "
				<< ltpHeaderControlWe.sessionKey << ", Seq "
				<< ltpHeaderControlWe.sequenceNumber << ") received for CID "
				<< cId.print() << ", rCID " << rCId.print());
		// Update CMC group that this NID is ready to receive response
		_enableNIdInCmcGroup(rCId, nodeId);
		/* Check in ICN packet buffer that all sequences starting from one up to
		 * the one received here have been received
		 */
		_icnPacketBufferMutex.lock();
		// Find rCID
		_icnPacketBufferIt = _icnPacketBuffer.find(rCId.uint());
		if (_icnPacketBufferIt == _icnPacketBuffer.end())
		{
			LOG4CXX_DEBUG(logger, "rCID " << rCId.print() << " unknown. "
					"Cannot check if CTRL-WED should be sent (START_PUBLISH_iSU"
					"B hasn't been received when CTRL-WE came. Send one now)");
			_icnPacketBufferMutex.unlock();
			_publishWindowEnded(rCId, nodeId, ltpHeaderControlWe.sessionKey);
			break;
		}
		// Find 0
		pridMapIt = _icnPacketBufferIt->second.find(ltpHeaderControlWe.ripd);
		if (pridMapIt == _icnPacketBufferIt->second.end())
		{
			LOG4CXX_TRACE(logger, "0 " << ltpHeaderControlWe.ripd
					<< " unknown. Cannot check if CTRL-WED should be sent "
							"(original WED got potentially lost");
			_icnPacketBufferMutex.unlock();
			_publishWindowEnded(rCId, nodeId, ltpHeaderControlWe.sessionKey);
			break;
		}
		// Find NID
		nodeIdMapIt = pridMapIt->second.find(nodeId.uint());
		// NID does not exist
		if (nodeIdMapIt == pridMapIt->second.end())
		{
			LOG4CXX_TRACE(logger, "NID " << nodeId.uint() << " unknown. Cannot "
					"check if WED CTRL should be sent. Simply re-publish a WED "
					"again");
			_icnPacketBufferMutex.unlock();
			_publishWindowEnded(rCId, nodeId, ltpHeaderControlWe.sessionKey);
			break;
		}
		// Find session
		sessionKeyMapIt = nodeIdMapIt->second.find(ltpHeaderControlWe.sessionKey);
		if (sessionKeyMapIt == nodeIdMapIt->second.end())
		{
			LOG4CXX_TRACE(logger, "SK " << ltpHeaderControlWe.sessionKey
					<< " unknown. Cannot check if WED CTRL should be sent. "
							"Simply re-publish a CTRL-WED again");
			_icnPacketBufferMutex.unlock();
			_publishWindowEnded(rCId, nodeId, ltpHeaderControlWe.sessionKey);
			break;
		}
		// Check that consecutive sequence numbers exist
		bool allFragmentsReceived = true;
		uint16_t firstMissingSequence = 0;
		uint16_t lastMissingSequence = 0;
		uint16_t previousSequence = 0;
		for (sequenceMapIt = sessionKeyMapIt->second.begin();
				sequenceMapIt != sessionKeyMapIt->second.end(); sequenceMapIt++)
		{
			// First fragment is missing
			if (previousSequence == 0 && sequenceMapIt->first != 1)
			{
				firstMissingSequence = 1;
				lastMissingSequence = 1;
				cout << "both set to 1\n";
				allFragmentsReceived = false;
			}
			// Not first, but any other sequence is missing
			else if ((previousSequence + 1) < sequenceMapIt->first)
			{
				if (firstMissingSequence == 0)
				{
					firstMissingSequence = previousSequence + 1;
					cout << "firstMissingSequence set to "
							<< firstMissingSequence << endl;
				}
				lastMissingSequence = sequenceMapIt->first - 1;
				cout << "lastMissingSequence set to " << lastMissingSequence
						<< endl;
				allFragmentsReceived = false;
			}
			previousSequence = sequenceMapIt->first;

		}
		// check if sequence indicated in CTRL-WE was actually received
		map<uint16_t, pair<uint8_t*, uint16_t>>::reverse_iterator sequenceMapRIt;
		sequenceMapRIt = sessionKeyMapIt->second.rbegin();
		if (sequenceMapRIt->first < ltpHeaderControlWe.sequenceNumber)
		{
			if (firstMissingSequence == 0)
			{
				firstMissingSequence = ltpHeaderControlWe.sequenceNumber;
			}
			lastMissingSequence = ltpHeaderControlWe.sequenceNumber;
			allFragmentsReceived = false;
		}
		_icnPacketBufferMutex.unlock();
		// send WED if all segments have been received
		if (allFragmentsReceived)
		{
			_publishWindowEnded(rCId, nodeId,

					ltpHeaderControlWe.sessionKey);
			sessionKey = ltpHeaderControlWe.sessionKey;
			return TP_STATE_ALL_FRAGMENTS_RECEIVED;
		}
		// Sequence was missing. Send off NACK message
		else
		{
			LOG4CXX_TRACE(logger, "Missing packets for CID " << cId.print()
					<< " rCID " << rCId.print()	<< " > 0 "
					<< ltpHeaderControlWe.ripd << " > SK "
					<< ltpHeaderControlWe.sessionKey << ". First: "
					<< firstMissingSequence << ", Last: "
					<< lastMissingSequence);
			_publishNegativeAcknowledgement(rCId, nodeId,
					ltpHeaderControlWe.sessionKey, firstMissingSequence,
					lastMissingSequence);
		}
		break;
	}
	case LTP_CONTROL_WINDOW_ENDED:
	{
		ltp_hdr_ctrl_wed_t ltpHeaderControlWed;
		// map<Session, WED received
		map<uint16_t, bool>::iterator sessionMapIt;
		//map<NID     map<Session,  WED received
		map<uint32_t, map<uint16_t, bool>>::iterator nidMapIt;
		//map<0,   map<NID       map<Session,  WED received
		map<uint32_t, map<uint32_t, map<uint16_t, bool>>>::iterator pridMapIt;
		// [3]
		ltpHeaderControlWed.ripd = 0;
		offset += sizeof(ltpHeaderControlWed.ripd);
		// [4] Session key
		memcpy(&ltpHeaderControlWed.sessionKey, packet + offset,
				sizeof(ltpHeaderControlWed.sessionKey));
		LOG4CXX_TRACE(logger, "LTP CTRL-WED received for rCID " << rCId.print()
				<< " > NID "
				<< nodeId.uint() << " > SK "
				<< ltpHeaderControlWed.sessionKey);
		_windowEndedResponsesMutex.lock();
		_windowEndedResponsesIt = _windowEndedResponses.find(rCId.uint());
		if (_windowEndedResponsesIt == _windowEndedResponses.end())
		{
			LOG4CXX_TRACE(logger, "rCID " << rCId.print() << " does not exist "
					"in sent CTRL-WEs map");
			_windowEndedResponsesMutex.unlock();
			return TP_STATE_NO_ACTION_REQUIRED;
		}
		pridMapIt = _windowEndedResponsesIt->second.find(
				ltpHeaderControlWed.ripd);
		if (pridMapIt == _windowEndedResponsesIt->second.end())
		{
			_windowEndedResponsesMutex.unlock();
			return TP_STATE_NO_ACTION_REQUIRED;
		}
		nidMapIt = pridMapIt->second.find(nodeId.uint());
		if (nidMapIt == pridMapIt->second.end())
		{
			LOG4CXX_TRACE(logger, "NID " << nodeId.uint() << " does not exist "
					"in sent CTRL-WEs map");
			_windowEndedResponsesMutex.unlock();
			return TP_STATE_NO_ACTION_REQUIRED;
		}
		sessionMapIt = nidMapIt->second.find(ltpHeaderControlWed.sessionKey);
		if (sessionMapIt == nidMapIt->second.end())
		{
			LOG4CXX_TRACE(logger, "SK " << ltpHeaderControlWed.sessionKey
					<< " does not exist in sent CTRL-WEs map");
			_windowEndedResponsesMutex.unlock();
			return TP_STATE_NO_ACTION_REQUIRED;
		}
		// Only update if WED received boolean is set to false
		if (!sessionMapIt->second)
		{
			sessionMapIt->second = true;
			LOG4CXX_TRACE(logger, "WED received boolean set to true for rCID "
					<< rCId.print() << " > NID "
					<< nodeId.uint() << " > SK "
					<< ltpHeaderControlWed.sessionKey);
		}
		// check if entire packet in LTP buffer can be deleted. Conditions: all
		// WEDs from all members of this CMC group were received
		list<NodeId> cmcGroup;
		_cmcGroupsMutex->lock();
		if (!_getCmcGroup(rCId, ltpHeaderControlWed.sessionKey, cmcGroup))
		{
			LOG4CXX_TRACE(logger, "No CMC group exists (anymore) for rCID "
					<< rCId.print() << " > SK "
					<< ltpHeaderControlWed.sessionKey);
			_windowEndedResponsesMutex.unlock();
			_cmcGroupsMutex->unlock();
			// delete LTP packet store in case the CMC group was closed due to
			// a socket shutdown of the web server
			_deleteBufferedLtpPacket(rCId, ltpHeaderControlWed.sessionKey);
			return TP_STATE_NO_ACTION_REQUIRED;
		}

		for (list<NodeId>::iterator cmcGroupIt = cmcGroup.begin();
				cmcGroupIt != cmcGroup.end(); cmcGroupIt++)
		{
			nidMapIt = pridMapIt->second.find(cmcGroupIt->uint());
			// NID found
			if (nidMapIt != pridMapIt->second.end())
			{
				sessionMapIt = nidMapIt->second.find(
						ltpHeaderControlWed.sessionKey);
				// SK found
				if (sessionMapIt != nidMapIt->second.end())
				{
					// WED still missing. exit here
					if (!sessionMapIt->second)
					{
						_cmcGroupsMutex->unlock();
						_windowEndedResponsesMutex.unlock();
						return TP_STATE_NO_ACTION_REQUIRED;
					}
				}
			}
		}
		_cmcGroupsMutex->unlock();
		_windowEndedResponsesMutex.unlock();
		// delete LTP packet buffer, as all cNAPs confirmed the reception of the
		// entire packet
		_deleteBufferedLtpPacket(rCId, ltpHeaderControlWed.sessionKey);
		break;
	}
	case LTP_CONTROL_WINDOW_UPDATE:
	{
		ltp_hdr_ctrl_wu_t ltpHeaderControlWu;
		// [3] 0
		ltpHeaderControlWu.ripd = 0;
		offset += sizeof(ltpHeaderControlWu.ripd);
		// [4] Session key
		memcpy(&ltpHeaderControlWu.sessionKey, packet + offset,
				sizeof(ltpHeaderControlWu.sessionKey));
		LOG4CXX_TRACE(logger, "LTP CTRL-WU received for CID "
				<< cId.print() << " with rCID " << rCId.print());
		break;
	}
	case LTP_CONTROL_WINDOW_UPDATED:
	{
		ltp_hdr_ctrl_wu_t ltpHeaderControlWud;
		list<uint32_t> nidsIt;
		//map<NID
		map<uint32_t, bool>::iterator nidIt;
		//map<SK,       map<NID
		map<uint16_t, map<uint32_t, bool>>::iterator skIt;
		//map<0,   map<SK,       map<NID
		map<uint32_t, map<uint16_t, map<uint32_t, bool>>>::iterator pridIt;
		// [3]
		ltpHeaderControlWud.ripd = 0;
		offset += sizeof(ltpHeaderControlWud.ripd);
		// [4] Session key
		memcpy(&ltpHeaderControlWud.sessionKey, packet + offset,
				sizeof(ltpHeaderControlWud.sessionKey));
		LOG4CXX_TRACE(logger, "LTP CTRL-WUD received for rCID " << rCId.print()
				<< " > NID " << nodeId.uint() << " > SK "
				<< ltpHeaderControlWud.sessionKey);
		_windowUpdateMutex.lock();
		_windowUpdateIt = _windowUpdate.find(rCId.uint());
		if (_windowUpdateIt == _windowUpdate.end())
		{
			LOG4CXX_DEBUG(logger, "rCID " << rCId.print() << " does not exist "
					"in WU map");
			_windowUpdateMutex.unlock();
			return TP_STATE_NO_ACTION_REQUIRED;
		}
		pridIt = _windowUpdateIt->second.find(ltpHeaderControlWud.ripd);
		if (pridIt == _windowUpdateIt->second.end())
		{
			_windowUpdateMutex.unlock();
			return TP_STATE_NO_ACTION_REQUIRED;
		}
		skIt = pridIt->second.find(ltpHeaderControlWud.sessionKey);
		if (skIt == pridIt->second.end())
		{
			LOG4CXX_DEBUG(logger, "rCID " << rCId.print() << " > SK "
					<< ltpHeaderControlWud.sessionKey << " does not exist in WU"
							" map");
			_windowUpdateMutex.unlock();
			return TP_STATE_NO_ACTION_REQUIRED;
		}
		nidIt = skIt->second.find(nodeId.uint());
		if (nidIt == skIt->second.end())
		{
			LOG4CXX_DEBUG(logger, "rCID " << rCId.print() << " > SK "
					<< ltpHeaderControlWud.sessionKey << " > NID "
					<< nodeId.uint() << " does not exist in WU map");
			_windowUpdateMutex.unlock();
			return TP_STATE_NO_ACTION_REQUIRED;
		}
		nidIt->second = true;
		LOG4CXX_TRACE(logger, "WUD state for NID " << nodeId.uint() << " set to"
				" 'received' for rCID " << rCId.print() << " > SK "
				<< ltpHeaderControlWud.sessionKey);
		_windowUpdateMutex.unlock();
		return TP_STATE_NO_ACTION_REQUIRED;
	}
	default:
		LOG4CXX_DEBUG(logger, "Unknown LTP CTRL type received");
	}
	return TP_STATE_FRAGMENTS_OUTSTANDING;
}

void Lightweight::_handleData(IcnId &rCid, uint8_t *packet)
{
	ltp_hdr_data_t ltpHeader;
	uint8_t offset = 0;
	// [1] Message type
	offset += sizeof(ltpHeader.messageType);
	// [2]
	ltpHeader.ripd = 0;
	offset += sizeof(ltpHeader.ripd);
	// [3] Session key
	memcpy(&ltpHeader.sessionKey, packet + offset,
			sizeof(ltpHeader.sessionKey));
	offset += sizeof(ltpHeader.sessionKey);
	// [4] Sequence num
	memcpy(&ltpHeader.sequenceNumber, packet + offset,
			sizeof(ltpHeader.sequenceNumber));
	offset += sizeof(ltpHeader.sequenceNumber);
	// [5] Payload length
	memcpy(&ltpHeader.payloadLength, packet + offset,
			sizeof(ltpHeader.payloadLength));
	offset += sizeof(ltpHeader.payloadLength);
	packet += offset;
	//add packet to LTP buffer
	_bufferIcnPacket(rCid, _configuration.nodeId(), ltpHeader, packet);
}

void Lightweight::_handleData(IcnId cId, IcnId &rCId, NodeId &nodeId,
		uint8_t *packet)
{
	ltp_hdr_data_t ltpHeader;
	uint8_t offset = 0;
	// [1] Message type
	offset += sizeof(ltpHeader.messageType);
	// [2]
	ltpHeader.ripd = 0;
	offset += sizeof(ltpHeader.ripd);
	// [3] Session key
	memcpy(&ltpHeader.sessionKey, packet + offset,
			sizeof(ltpHeader.sessionKey));
	offset += sizeof(ltpHeader.sessionKey);
	// [4] Sequence num
	memcpy(&ltpHeader.sequenceNumber, packet + offset,
			sizeof(ltpHeader.sequenceNumber));
	offset += sizeof(ltpHeader.sequenceNumber);
	// [5] Payload length
	memcpy(&ltpHeader.payloadLength, packet + offset,
			sizeof(ltpHeader.payloadLength));
	offset += sizeof(ltpHeader.payloadLength);
	packet += offset;
	// add packet to LTP buffer
	_bufferIcnPacket(rCId, nodeId, ltpHeader, packet);
	// Note, the _potentialCmcGroups* map and mutex are only pointers. They have
	// been declared in the HTTP handler and were passed when the LTP class was
	// initialised
	_potentialCmcGroupsMutex->lock();
	// check if rCID is known
	_potentialCmcGroupsIt = _potentialCmcGroups->find(rCId.uint());
	// rCID unknown --> new CMC group
	if (_potentialCmcGroupsIt == _potentialCmcGroups->end())
	{
		//map<NID,    entire HTTP request received
		map<uint32_t, bool> nodeIdMap;
		//map<0,   map<NID,      entire HTTP request received
		map<uint32_t, map<uint32_t, bool>> prIdMap;
		// create NID map
		nodeIdMap.insert(pair<uint32_t, bool>(nodeId.uint(), false));
		// create 0 map
		prIdMap.insert(pair<uint32_t, map<uint32_t, bool>>(
				ltpHeader.ripd, nodeIdMap));
		_potentialCmcGroups->insert(pair<uint32_t, map<uint32_t, map<uint32_t,
				bool>>>(rCId.uint(), prIdMap));
		LOG4CXX_TRACE(logger, "New potential CMC group created for rCID "
				<< rCId.print() << " with NID " << nodeId.uint());
		_potentialCmcGroupsMutex->unlock();
		return;
	}
	// rCID known. Check if 0 is known
	map<uint32_t, map<uint32_t, bool>>::iterator pridIt;
	pridIt = _potentialCmcGroupsIt->second.find(ltpHeader.ripd);
	// 0 unknown
	if (pridIt == _potentialCmcGroupsIt->second.end())
	{
		//map<NID,    entire HTTP request received
		map<uint32_t, bool> nodeIdMap;
		nodeIdMap.insert(pair<uint32_t, bool>(nodeId.uint(), false));
		_potentialCmcGroupsIt->second.insert(pair<uint32_t, map<uint32_t, bool>>
				(ltpHeader.ripd, nodeIdMap));
		LOG4CXX_TRACE(logger, "New potential CMC group created for known rCID "
				<< rCId.print() << " with NID " << nodeId.uint());
		_potentialCmcGroupsMutex->unlock();
		return;
	}
	// 0 known. Check if NID is known
	map<uint32_t, bool>::iterator nidIt;
	nidIt = pridIt->second.find(nodeId.uint());
	// NID unknown
	if (nidIt == pridIt->second.end())
	{
		pridIt->second.insert(pair<uint32_t, bool>(nodeId.uint(), false));
		LOG4CXX_TRACE(logger, "Existing potential CMC group for rCID "
				<< rCId.print() << " updated with NID " << nodeId.uint());
		_potentialCmcGroupsMutex->unlock();
		return;
	}
	LOG4CXX_TRACE(logger, "NID already exists in potential CMC group for rCID "
			<< rCId.print());
	_potentialCmcGroupsMutex->unlock();
}

nack_group_t Lightweight::_nackGroup(IcnId &rCid,
		ltp_hdr_ctrl_nack_t &ltpHeaderNack)
{
	nack_group_t nackGroup;
	_nackGroupsIt = _nackGroups.find(rCid.uint());
	// rCID not found
	if (_nackGroupsIt == _nackGroups.end())
	{
		LOG4CXX_WARN(logger, "rCID " << rCid.print() << " does not exist in "
				"NACK group");
		return nackGroup;
	}
	map<uint32_t, map<uint16_t, nack_group_t>>::iterator pridIt;
	pridIt = _nackGroupsIt->second.find(ltpHeaderNack.ripd);
	// 0 not found
	if (pridIt == _nackGroupsIt->second.end())
	{
		LOG4CXX_WARN(logger, "rCID " << rCid.print() << " does not exist in NACK "
						"group");
		return nackGroup;
	}
	map<uint16_t, nack_group_t>::iterator skIt;
	skIt = pridIt->second.find(ltpHeaderNack.sessionKey);
	// SK not found
	if (skIt == pridIt->second.end())
	{
		LOG4CXX_WARN(logger, "rCID " << rCid.print() << " > SK "
				<< ltpHeaderNack.sessionKey << " does not exist in NACK group");
		return nackGroup;
	}
	return skIt->second;
}

void Lightweight::_publishData(IcnId &rCId, ltp_hdr_ctrl_we_t &ltpHeaderCtrlWe,
		list<NodeId> &nodeIds, uint8_t *data, uint16_t &dataSize)
{
	//uint8_t attempts;
	ltp_hdr_data_t ltpHeaderData;
	uint32_t sentBytes = 0;
	uint16_t fragmentSize = 0;
	uint8_t *packet;
	uint16_t packetSize;
	uint8_t pad = 0;
	list<string> nodeIdsStr;
	list<NodeId>::iterator nodeIdsIt;
	ostringstream nodeIdsOss;
	uint16_t credit = _configuration.ltpInitialCredit();
	//WE check: map<NID,    WUD has not been received
	map<uint32_t, bool>::iterator nidIt;
	//WE check: map<SK,     map<NID
	map<uint16_t, map<uint32_t, bool>>::iterator skIt;
	//WE check: map<0,   map<SK,       map<NID
	map<uint32_t, map<uint16_t, map<uint32_t, bool>>>::iterator pridIt;
	// calculate the payload the LTP packet can carry
	uint32_t mitu = _configuration.mitu() - rCId.length() -
			_configuration.icnHeaderLength() - sizeof(ltp_hdr_data_t) - 20;
	mitu = mitu - (mitu % 8);//make it a multiple of 8
	ltpHeaderData.messageType = LTP_DATA;
	ltpHeaderData.ripd = ltpHeaderCtrlWe.ripd;
	ltpHeaderData.sessionKey = ltpHeaderCtrlWe.sessionKey;
	ltpHeaderData.sequenceNumber = 0;
	for (nodeIdsIt = nodeIds.begin(); nodeIdsIt != nodeIds.end(); nodeIdsIt++)
	{
		nodeIdsStr.push_back(nodeIdsIt->str());
		nodeIdsOss << nodeIdsIt->uint() << " ";
	}
	LOG4CXX_TRACE(logger, "CMC group size = " << nodeIds.size() << ": "
			<< nodeIdsOss.str());
	while (sentBytes < dataSize)
	{
		// Check if enough credit is left
		if (credit == 0)
		{
			//attempts = ENIGMA;
			uint16_t rtt = _rtt();
			LOG4CXX_TRACE(logger, "Run out of credit for rCID " << rCId.print()
					<< " > SK "
					<< ltpHeaderData.sessionKey);
			list<NodeId> unconfirmedNids = nodeIds;
			_publishWindowUpdate(rCId, ltpHeaderData.sessionKey,
					unconfirmedNids);
			while (!unconfirmedNids.empty())
			{
				boost::posix_time::ptime startTime =
						boost::posix_time::microsec_clock::local_time();
				boost::posix_time::ptime currentTime = startTime;
				boost::posix_time::time_duration timeWaited;
				timeWaited = currentTime - startTime;
				// continuously check if all WEDs have been received
				while (timeWaited.total_milliseconds() < (_rttMultiplier * rtt))
				{
					_windowUpdateMutex.lock();
					unconfirmedNids = _wudsNotReceived(rCId,
							ltpHeaderData.sessionKey);
					if (unconfirmedNids.empty())
					{
						_windowUpdateMutex.unlock();
						break;
					}
					_windowUpdateMutex.unlock();
					currentTime =
							boost::posix_time::microsec_clock::local_time();
					timeWaited = currentTime - startTime;
				}
				// Timeout or all WUDs received. Simply check and resend if
				// necessary
				if (!unconfirmedNids.empty())
				{
					LOG4CXX_TRACE(logger, unconfirmedNids.size() << " NIDs did "
							"not reply with CTRL-WED for rCID " << rCId.print()
							<< " > SK " << ltpHeaderData.sessionKey << " within"
							" " << timeWaited.total_milliseconds() << "ms");
					_publishWindowUpdate(rCId, ltpHeaderData.sessionKey,
							unconfirmedNids);
				}
			}
			_removeNidsFromWindowUpdate(rCId, ltpHeaderData.sessionKey,
					nodeIds);
			credit = _configuration.ltpInitialCredit();
		}
		ltpHeaderData.sequenceNumber++;
		// another fragment needed
		if ((dataSize - sentBytes) > mitu)
		{
			fragmentSize = mitu;
			ltpHeaderData.payloadLength = fragmentSize;
		}
		// last (or single) fragment
		else
		{
			fragmentSize = dataSize - sentBytes;
			ltpHeaderData.payloadLength = fragmentSize;
			// add padding bits, if necessary
			pad = (fragmentSize % 8);
			if (pad != 0)
			{
				pad = 8 - (fragmentSize % 8);
				fragmentSize += pad;
				LOG4CXX_TRACE(logger, (int)pad << " padding bits added to make "
						"fragment a multiple of 8 (" << fragmentSize << ")");
			}
		}
		packetSize = sizeof(ltp_hdr_data_t) + fragmentSize;
		uint8_t offsetPacket = 0;
		// Make the packet
		packet = (uint8_t *)malloc(packetSize);
		// [1] messageType;
		memcpy(packet, &ltpHeaderData.messageType,
				sizeof(ltpHeaderData.messageType));
		offsetPacket += sizeof(ltpHeaderData.messageType);
		// [2]
		memcpy(packet + offsetPacket, &ltpHeaderData.ripd,
				sizeof(ltpHeaderData.ripd));
		offsetPacket += sizeof(ltpHeaderData.ripd);
		// [3] Session key
		memcpy(packet + offsetPacket,	&ltpHeaderData.sessionKey,
				sizeof(ltpHeaderData.sessionKey));
		offsetPacket += sizeof(ltpHeaderData.sessionKey);
		// [4] Sequence number
		memcpy(packet + offsetPacket, &ltpHeaderData.sequenceNumber,
				sizeof(ltpHeaderData.sequenceNumber));
		offsetPacket += sizeof(ltpHeaderData.sequenceNumber);
		// [5] Payload length;
		memcpy(packet + offsetPacket, &ltpHeaderData.payloadLength,
				sizeof(ltpHeaderData.payloadLength));
		offsetPacket += sizeof(ltpHeaderData.payloadLength);
		// now the actual packet
		memcpy(packet + offsetPacket, data + sentBytes, fragmentSize);
		// Check if TC drop rate should be applied
		if (!TrafficControl::handle())
		{
			_icnCoreMutex.lock();
			_icnCore->publish_data(rCId.binIcnId(), DOMAIN_LOCAL, NULL, 0,
					nodeIdsStr, packet, packetSize);
			_icnCoreMutex.unlock();
		}
		sentBytes += (fragmentSize - pad);// remove padding bits
		LOG4CXX_TRACE(logger, packetSize << " bytes published to "
				<< nodeIdsStr.size() << " cNAP(s) under rCID " << rCId.print()
				<< ", 0 " << ltpHeaderData.ripd << ", SK "
				<< ltpHeaderData.sessionKey << ", Sequence "
				<< ltpHeaderData.sequenceNumber << ", Credit " << credit << " ("
				<< sentBytes << "/" << dataSize << " sent)");
		credit--;// decrement credit by one
		_bufferLtpPacket(rCId, ltpHeaderData, packet, packetSize);
		free(packet);
	}
	ltpHeaderCtrlWe.sessionKey = ltpHeaderData.sessionKey;
	ltpHeaderCtrlWe.sequenceNumber = ltpHeaderData.sequenceNumber;
}

uint16_t Lightweight::_publishData(IcnId &cId, IcnId &rCId,
		uint16_t &sessionKey, uint8_t *data, uint16_t &dataSize)
{
	uint16_t sequenceNumber = 0;
	uint32_t sentBytes = 0;
	uint16_t fragmentSize = 0;
	uint8_t *packet;
	uint16_t packetSize;
	uint8_t pad = 0;
	uint16_t credit = _configuration.ltpInitialCredit();
	// calculate the payload the LTP packet can carry
	uint32_t mtu = _configuration.mtu() - cId.length() - rCId.length() -
			_configuration.icnHeaderLength() - sizeof(ltp_hdr_data_t) - 20;
	mtu = mtu - (mtu % 8);//make it a multiple of 8
	ltp_hdr_data_t ltpHeaderData;
	ltpHeaderData.messageType = LTP_DATA;
	ltpHeaderData.ripd = 0;
	ltpHeaderData.sessionKey = sessionKey;// socket FD
	while (sentBytes < dataSize)
	{
		if (credit == 0)
		{
			LOG4CXX_INFO(logger, "WU + WUD has not been implemented for HTTP"
					"POST messages as this would block the code. Simply "
					"sleeping " << _rtt() << "ms");
			credit = _configuration.ltpInitialCredit();
//FIXME initialise credit properly (WU+WUD) before continue: blocking code!
			boost::this_thread::sleep(boost::posix_time::milliseconds(_rtt()));
		}
		sequenceNumber++;
		// another fragment needed
		if ((dataSize - sentBytes) > mtu)
		{
			fragmentSize = mtu;
			ltpHeaderData.payloadLength = fragmentSize;
		}
		// last (or single) fragment
		else
		{
			fragmentSize = dataSize - sentBytes;
			ltpHeaderData.payloadLength = fragmentSize;
			// add padding bits, if necessary
			pad = (fragmentSize % 8);
			if (pad != 0)
			{
				pad = 8 - (fragmentSize % 8);
				fragmentSize += pad;
				LOG4CXX_TRACE(logger, (int)pad << " padding bits added to make "
						"fragment a multiple of 8 (" << fragmentSize << ")");
			}
		}
		ltpHeaderData.sequenceNumber = sequenceNumber;
		packetSize = sizeof(ltp_hdr_data_t) + fragmentSize;
		uint8_t offsetPacket = 0;
		// Make the packet
		packet = (uint8_t *)malloc(packetSize);
		// [1] messageType;
		memcpy(packet, &ltpHeaderData.messageType,
				sizeof(ltpHeaderData.messageType));
		offsetPacket += sizeof(ltpHeaderData.messageType);
		// [2] 0
		memcpy(packet + offsetPacket, &ltpHeaderData.ripd,
				sizeof(ltpHeaderData.ripd));
		offsetPacket += sizeof(ltpHeaderData.ripd);
		// [3] Session key
		memcpy(packet + offsetPacket,	&ltpHeaderData.sessionKey,
				sizeof(ltpHeaderData.sessionKey));
		offsetPacket += sizeof(ltpHeaderData.sessionKey);
		// [4] Sequence number
		memcpy(packet + offsetPacket, &ltpHeaderData.sequenceNumber,
				sizeof(ltpHeaderData.sequenceNumber));
		offsetPacket += sizeof(ltpHeaderData.sequenceNumber);
		// [5] Payload length;
		memcpy(packet + offsetPacket, &ltpHeaderData.payloadLength,
				sizeof(ltpHeaderData.payloadLength));
		offsetPacket += sizeof(ltpHeaderData.payloadLength);
		// now the actual packet
		memcpy(packet + offsetPacket, data + sentBytes, fragmentSize);
		// Check if TC drop rate should be applied
		if (!TrafficControl::handle())
		{
			_icnCoreMutex.lock();
			_icnCore->publish_data_isub(cId.binIcnId(), DOMAIN_LOCAL, NULL, 0,
					rCId.binIcnId(), packet, packetSize);
			_icnCoreMutex.unlock();
		}
		sentBytes += (fragmentSize - pad);// remove padding bits
		credit--;// decrement credit by one
		LOG4CXX_TRACE(logger, packetSize << " bytes published under CID "
				<< cId.print() << ", rCID " << rCId.print() << ", SK "
				<< ltpHeaderData.sessionKey << ", Sequence "
				<< ltpHeaderData.sequenceNumber << ", Credit " << credit
				<< " ("	<< sentBytes << "/" << dataSize << " sent)");
		_bufferProxyPacket(cId, rCId, ltpHeaderData, packet,
				packetSize);
		free(packet);
	}
	_publishWindowEnd(cId, rCId, sessionKey, sequenceNumber);
	return sequenceNumber;
}

void Lightweight::_publishDataRange(IcnId &rCid,
		ltp_hdr_ctrl_nack_t &ltpCtrlNack)
{
	// First get the CID for the given rCID
	IcnId cid;
	_cIdReverseLookUpMutex.lock();
	_cIdReverseLookUpIt = _cIdReverseLookUp.find(rCid.uint());
	if (_cIdReverseLookUpIt == _cIdReverseLookUp.end())
	{
		LOG4CXX_ERROR(logger, "CID for rCID " << rCid.print() << " could not "
				"be found in reverse look-up map");
		_cIdReverseLookUpMutex.unlock();
		return;
	}
	cid = _cIdReverseLookUpIt->second;
	_cIdReverseLookUpMutex.unlock();
	_proxyPacketBufferMutex.lock();
	_proxyPacketBufferIt = _proxyPacketBuffer.find(rCid.uint());
	//rCID not found
	if (_proxyPacketBufferIt == _proxyPacketBuffer.end())
	{
		LOG4CXX_WARN(logger, "rCID " << rCid.print() << " does not exist in LTP"
				" proxy buffer");
		_proxyPacketBufferMutex.unlock();
		return;
	}
	// map<0,  map<SK,       map<SN,       Packet
	map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t *,
			uint16_t>>>>::iterator pridIt;
	pridIt = _proxyPacketBufferIt->second.find(ltpCtrlNack.ripd);
	// 0 not found
	if (pridIt == _proxyPacketBufferIt->second.end())
	{
		_proxyPacketBufferMutex.unlock();
		return;
	}
	//map<SK,     map<SN,       Packet
	map<uint16_t, map<uint16_t, pair<uint8_t *,	uint16_t>>>::iterator skIt;
	skIt = pridIt->second.find(ltpCtrlNack.sessionKey);
	if (skIt == pridIt->second.end())
	{
		LOG4CXX_WARN(logger, "SK " << ltpCtrlNack.sessionKey << " does not "
				"exist in LTP proxy buffer for rCID " << rCid.print());
		_proxyPacketBufferMutex.unlock();
		return;
	}
	uint16_t sequence = 0;
	list<string> nodeIds;
	nodeIds.push_back(_configuration.nodeId().str());
	//map<SN,     Packet
	map<uint16_t, pair<uint8_t *, uint16_t>>::iterator snIt;
	// iterate over the LTP buffer and re-publish the range of fragments
	for (sequence = ltpCtrlNack.start; sequence <= ltpCtrlNack.end; sequence++)
	{
		snIt = skIt->second.find(sequence);
		if (snIt == skIt->second.end())
		{
			LOG4CXX_ERROR(logger, "Sequence number " << sequence << " is "
					"missing in LTP buffer for rCID " << rCid.print()
					<< " > SK " << ltpCtrlNack.sessionKey);
			break;
		}
		if (!TrafficControl::handle())
		{
			_icnCoreMutex.lock();
			_icnCore->publish_data(rCid.binIcnId(), DOMAIN_LOCAL, NULL, 0,
					nodeIds, snIt->second.first, snIt->second.second);
			_icnCoreMutex.unlock();
		}
		LOG4CXX_TRACE(logger, "Packet of total length " << snIt->second.second
				<< " with Sequence " << sequence << " re-published under rCID "
				<< rCid.print() << " > SK " << ltpCtrlNack.sessionKey);
	}
	_proxyPacketBufferMutex.unlock();
}

void Lightweight::_publishDataRange(IcnId &rCid, uint16_t &sessionKey,
		nack_group_t &nackGroup)
{
	map<uint32_t, map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t *,
		uint16_t>>>>>::iterator rCidIt;
	map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t *,
		uint16_t>>>>::iterator pridIt;
	map<uint16_t, map<uint16_t, pair<uint8_t *, uint16_t>>>::iterator skIt;
	map<uint16_t, pair<uint8_t *, uint16_t>>::iterator snIt;
	_ltpPacketBufferMutex.lock();
	rCidIt = _ltpPacketBuffer.find(rCid.uint());
	// rCID not found
	if (rCidIt == _ltpPacketBuffer.end())
	{
		LOG4CXX_ERROR(logger, "rCID " << rCid.print() << " not found in LTP "
				"packet buffer");
		_ltpPacketBufferMutex.unlock();
		return;
	}
	pridIt = rCidIt->second.find(0);
	// 0 not found
	if (pridIt == rCidIt->second.end())
	{
		LOG4CXX_ERROR(logger, "rCID " << rCid.print() << " not found in LTP "
				"packet buffer");
		_ltpPacketBufferMutex.unlock();
		return;
	}
	skIt = pridIt->second.find(sessionKey);
	// SK not found
	if (skIt == pridIt->second.end())
	{
		LOG4CXX_ERROR(logger, "rCID " << rCid.print() << " > SK "
				<< sessionKey << " not found in LTP packet buffer");
		_ltpPacketBufferMutex.unlock();
		return;
	}
	// Iterate over the range of sequence number for rCID > 0 > SK and
	// re-publish the packets
	for (uint16_t sequence = nackGroup.startSequence;
			sequence <= nackGroup.endSequence; sequence++)
	{
		snIt = skIt->second.find(sequence);
		if (snIt == skIt->second.end())
		{
			LOG4CXX_ERROR(logger, "Sequence number " << sequence << " is "
					"missing in LTP buffer for rCID " << rCid.print()
					<< " > SK " << sessionKey);
			break;
		}
		// convert list<NodeId> into list<string>
		list<string> nodeIds;
		ostringstream nodeIdsOss;
		for (list<NodeId>::iterator it = nackGroup.nodeIds.begin();
				it != nackGroup.nodeIds.end(); it++)
		{
			nodeIds.push_back(it->str());
			nodeIdsOss << it->uint() << " ";
		}
		if (!TrafficControl::handle())
		{
			_icnCoreMutex.lock();
			_icnCore->publish_data(rCid.binIcnId(), DOMAIN_LOCAL, NULL, 0,
					nodeIds, snIt->second.first, snIt->second.second);
			_icnCoreMutex.unlock();
		}
		LOG4CXX_TRACE(logger, "Packet of total length " << snIt->second.second
				<< " with Sequence " << sequence << " re-published under rCID "
				<< rCid.print() << " > SK "
				<< sessionKey << " to "	<< nodeIds.size() << " NIDs: "
				<< nodeIdsOss.str());
	}
	_ltpPacketBufferMutex.unlock();
}

void Lightweight::_publishSessionEnd(IcnId &rCid, list<NodeId> &nodeIds,
		ltp_hdr_ctrl_se_t &ltpHeaderCtrlSe)
{
	list<string> nodeIdsStr;
	ostringstream nodeIdsOss;
	uint16_t packetSize = sizeof(ltp_hdr_ctrl_se_t);
	uint8_t *packet = (uint8_t *)malloc(packetSize);
	uint8_t offset = 0;
	//ltpHeaderCtrlSe.messageType = LTP_CONTROL;
	//ltpHeaderCtrlSe.controlType = LTP_CONTROL_SESSION_END;
	// [1] Message type
	memcpy(packet, &ltpHeaderCtrlSe.messageType,
			sizeof(ltpHeaderCtrlSe.messageType));
	offset += sizeof(ltpHeaderCtrlSe.messageType);
	// [2] Control type
	memcpy(packet + offset, &ltpHeaderCtrlSe.controlType,
			sizeof(ltpHeaderCtrlSe.controlType));
	offset += sizeof(ltpHeaderCtrlSe.controlType);
	// [3] 0
	memcpy(packet + offset, &ltpHeaderCtrlSe.ripd,
			sizeof(ltpHeaderCtrlSe.ripd));
	offset += sizeof(ltpHeaderCtrlSe.ripd);
	// [4] SK
	memcpy(packet + offset, &ltpHeaderCtrlSe.sessionKey,
			sizeof(ltpHeaderCtrlSe.sessionKey));
	// convert list of NIDs into strings
	for (list<NodeId>::iterator it = nodeIds.begin(); it != nodeIds.end(); it++)
	{
		nodeIdsStr.push_back(it->str());
		nodeIdsOss << it->uint() << " ";
	}
	_icnCoreMutex.lock();
	_icnCore->publish_data(rCid.binIcnId(), DOMAIN_LOCAL, NULL, 0, nodeIdsStr,
			packet, packetSize);
	_icnCoreMutex.unlock();
	LOG4CXX_TRACE(logger, "CTRL-SE published to " << nodeIds.size() << " NIDs "
			"under rCID " << rCid.print() << " > SK "
			<< ltpHeaderCtrlSe.sessionKey << ": " << nodeIdsOss.str());
	free(packet);
}

void Lightweight::_publishSessionEnded(IcnId &cid, IcnId &rCid,
		uint16_t &sessionKey)
{
	ltp_hdr_ctrl_sed_t ltpHdrCtrlSed;
	uint8_t *data;
	uint8_t offset = 0;
	uint32_t dataSize = sizeof(ltpHdrCtrlSed);
	//ltpHdrCtrlSed.messageType = LTP_CONTROL;
	//ltpHdrCtrlSed.controlType = LTP_CONTROL_SESSION_ENDED;
	ltpHdrCtrlSed.ripd = 0;
	ltpHdrCtrlSed.sessionKey = sessionKey;
	data = (uint8_t *)malloc(dataSize);
	// [1] Message type
	memcpy(data, &ltpHdrCtrlSed.messageType, sizeof(ltpHdrCtrlSed.messageType));
	offset += sizeof(ltpHdrCtrlSed.messageType);
	// [2] Control type
	memcpy(data + offset, &ltpHdrCtrlSed.controlType,
			sizeof(ltpHdrCtrlSed.controlType));
	offset += sizeof(ltpHdrCtrlSed.controlType);
	// [3] 0
	memcpy(data + offset, &ltpHdrCtrlSed.ripd,
			sizeof(ltpHdrCtrlSed.ripd));
	offset += sizeof(ltpHdrCtrlSed.ripd);
	// [4] SK
	memcpy(data + offset, &ltpHdrCtrlSed.sessionKey,
			sizeof(ltpHdrCtrlSed.sessionKey));
	_icnCoreMutex.lock();
	_icnCore->publish_data_isub(cid.binIcnId(), DOMAIN_LOCAL, NULL, 0,
				rCid.binIcnId(), data, dataSize);
	_icnCoreMutex.unlock();
	LOG4CXX_TRACE(logger, "CTRL-SED published to CID " << cid.print() << " for "
			"rCID "	<< rCid.print() << " > SK "
			<< sessionKey);
	free(data);
}

void Lightweight::_publishNegativeAcknowledgement(IcnId &rCid, NodeId &nodeId,
		uint16_t &sessionKey, uint16_t &firstMissingSequence,
		uint16_t &lastMissingSequence)
{
	ltp_hdr_ctrl_nack_t ltpHeaderControlNack;
	ltpHeaderControlNack.messageType = LTP_CONTROL;
	ltpHeaderControlNack.controlType = LTP_CONTROL_NACK;
	ltpHeaderControlNack.ripd = 0;
	ltpHeaderControlNack.sessionKey = sessionKey;
	ltpHeaderControlNack.start = firstMissingSequence;
	ltpHeaderControlNack.end = lastMissingSequence;
	uint8_t *packet = (uint8_t *)malloc(sizeof(ltp_hdr_ctrl_nack_t));
	uint8_t offset = 0;
	// [1] Message type
	memcpy(packet, &ltpHeaderControlNack.messageType,
			sizeof(ltpHeaderControlNack.messageType));
	offset += sizeof(ltpHeaderControlNack.messageType);
	// [2] Control type
	memcpy(packet + offset, &ltpHeaderControlNack.controlType,
			sizeof(ltpHeaderControlNack.controlType));
	offset += sizeof(ltpHeaderControlNack.controlType);
	// [3]
	memcpy(packet + offset, &ltpHeaderControlNack.ripd,
			sizeof(ltpHeaderControlNack.ripd));
	offset += sizeof(ltpHeaderControlNack.ripd);
	// [4] SK
	memcpy(packet + offset, &ltpHeaderControlNack.sessionKey,
			sizeof(ltpHeaderControlNack.sessionKey));
	offset += sizeof(ltpHeaderControlNack.sessionKey);
	// [5] Start sequence number
	memcpy(packet + offset, &ltpHeaderControlNack.start,
			sizeof(ltpHeaderControlNack.start));
	offset += sizeof(ltpHeaderControlNack.start);
	// [6] End sequence number
	memcpy(packet + offset, &ltpHeaderControlNack.end,
			sizeof(ltpHeaderControlNack.end));
	list<string> nodeIds;
	nodeIds.push_back(nodeId.str());
	_icnCoreMutex.lock();
	_icnCore->publish_data(rCid.binIcnId(), DOMAIN_LOCAL, NULL, 0, nodeIds,
			packet, sizeof(ltp_hdr_ctrl_nack_t));
	_icnCoreMutex.unlock();
	LOG4CXX_TRACE(logger, "CTRL-NACK for Sequence range "
			<< firstMissingSequence << " - " << lastMissingSequence
			<< " published to rCID " << rCid.print() << " > NID "
			<< nodeId.uint() << " > SK " << sessionKey);
	free(packet);
}

void Lightweight::_publishNegativeAcknowledgement(IcnId &cid, IcnId &rCid,
		uint16_t &sessionKey, uint16_t &firstMissingSequence,
		uint16_t &lastMissingSequence)
{
	ltp_hdr_ctrl_nack_t ltpHeaderControlNack;
	ltpHeaderControlNack.messageType = LTP_CONTROL;
	ltpHeaderControlNack.controlType = LTP_CONTROL_NACK;
	ltpHeaderControlNack.ripd = 0;
	ltpHeaderControlNack.sessionKey = sessionKey;
	ltpHeaderControlNack.start = firstMissingSequence;
	ltpHeaderControlNack.end = lastMissingSequence;
	uint8_t *packet = (uint8_t *)malloc(sizeof(ltp_hdr_ctrl_nack_t));
	uint8_t offset = 0;
	// [1] Message type
	memcpy(packet, &ltpHeaderControlNack.messageType,
			sizeof(ltpHeaderControlNack.messageType));
	offset += sizeof(ltpHeaderControlNack.messageType);
	// [2] Control type
	memcpy(packet + offset, &ltpHeaderControlNack.controlType,
			sizeof(ltpHeaderControlNack.controlType));
	offset += sizeof(ltpHeaderControlNack.controlType);
	// [3] 0
	memcpy(packet + offset, &ltpHeaderControlNack.ripd,
			sizeof(ltpHeaderControlNack.ripd));
	offset += sizeof(ltpHeaderControlNack.ripd);
	// [4] SK
	memcpy(packet + offset, &ltpHeaderControlNack.sessionKey,
			sizeof(ltpHeaderControlNack.sessionKey));
	offset += sizeof(ltpHeaderControlNack.sessionKey);
	// [5] Start sequence number
	memcpy(packet + offset, &ltpHeaderControlNack.start,
			sizeof(ltpHeaderControlNack.start));
	offset += sizeof(ltpHeaderControlNack.start);
	// [6] End sequence number
	memcpy(packet + offset, &ltpHeaderControlNack.end,
			sizeof(ltpHeaderControlNack.end));
	_icnCoreMutex.lock();
	_icnCore->publish_data_isub(cid.binIcnId(), DOMAIN_LOCAL, NULL, 0,
				rCid.binIcnId(), packet, sizeof(ltp_hdr_ctrl_nack_t));
	_icnCoreMutex.unlock();
	LOG4CXX_TRACE(logger, "CTRL-NACK for Sequence range "
			<< firstMissingSequence << " - " << lastMissingSequence
			<< " published to CID " << cid.print() << " for rCID "
			<< rCid.print() << " > SK " << sessionKey);
	free(packet);
}

void Lightweight::_publishWindowEnd(IcnId &rCId, list<NodeId> &nodeIds,
			ltp_hdr_ctrl_we_t &ltpHeader)
{
	list<string> nodeIdsStr;
	list<NodeId>::iterator nodeIdsIt;
	uint8_t *packet;
	uint16_t packetSize = sizeof(ltp_hdr_ctrl_we_t);
	for (nodeIdsIt = nodeIds.begin(); nodeIdsIt != nodeIds.end(); nodeIdsIt++)
	{
		nodeIdsStr.push_back(nodeIdsIt->str());
	}
	// Fill up the LTP CTRL header
	ltpHeader.messageType = LTP_CONTROL;
	ltpHeader.controlType = LTP_CONTROL_WINDOW_END;
	// first create WED boolean so that there's no potential raise condition
	_addWindowEnd(rCId, nodeIds, ltpHeader);
	// make packet
	packet = (uint8_t *)malloc(packetSize);
	// [1] messageType
	memcpy(packet, &ltpHeader.messageType, sizeof(ltpHeader.messageType));
	// [2] controlType
	memcpy(packet + sizeof(ltpHeader.messageType), &ltpHeader.controlType,
			sizeof(ltpHeader.controlType));
	// [3] ripd
	memcpy(packet + sizeof(ltpHeader.messageType)
			+ sizeof(ltpHeader.controlType), &ltpHeader.ripd,
			sizeof(ltpHeader.ripd));
	// [4] session key
	memcpy(packet + sizeof(ltpHeader.messageType)
			+ sizeof(ltpHeader.controlType) + sizeof(ltpHeader.ripd),
			&ltpHeader.sessionKey,	sizeof(ltpHeader.sessionKey));
	// [5] sequenceNumber
	memcpy(packet + sizeof(ltpHeader.messageType)
				+ sizeof(ltpHeader.controlType) + sizeof(ltpHeader.ripd)
				+ sizeof(ltpHeader.sessionKey), &ltpHeader.sequenceNumber,
				sizeof(ltpHeader.sequenceNumber));
	_icnCoreMutex.lock();
	_icnCore->publish_data(rCId.binIcnId(), DOMAIN_LOCAL, NULL, 0,
			nodeIdsStr, packet, packetSize);
	_icnCoreMutex.unlock();
	LOG4CXX_TRACE(logger, "LTP CTRL-WE published under " << rCId.print()
			<< " > SK " << ltpHeader.sessionKey << " > Sequence "
			<< ltpHeader.sequenceNumber);

	free(packet);
}

void Lightweight::_publishWindowEnd(IcnId &cId, IcnId &rCId,
		uint16_t &sessionKey, uint16_t &sequenceNumber)
{
	ltp_hdr_ctrl_we_t ltpHeader;
	uint8_t *packet;
	uint16_t packetSize = sizeof(ltp_hdr_ctrl_we_t);
	// Fill up the LTP CTRL header
	ltpHeader.messageType = LTP_CONTROL;
	ltpHeader.controlType = LTP_CONTROL_WINDOW_END;
	ltpHeader.ripd = 0;
	ltpHeader.sessionKey = sessionKey;
	ltpHeader.sequenceNumber = sequenceNumber;
	// make packet
	packet = (uint8_t *)malloc(packetSize);
	// [1] messageType
	memcpy(packet, &ltpHeader.messageType, sizeof(ltpHeader.messageType));
	// [2] controlType
	memcpy(packet + sizeof(ltpHeader.messageType), &ltpHeader.controlType,
			sizeof(ltpHeader.controlType));
	// [3] ripd
	memcpy(packet + sizeof(ltpHeader.messageType)
			+ sizeof(ltpHeader.controlType), &ltpHeader.ripd,
			sizeof(ltpHeader.ripd));
	// [4] session key
	memcpy(packet + sizeof(ltpHeader.messageType)
			+ sizeof(ltpHeader.controlType) + sizeof(ltpHeader.ripd),
			&ltpHeader.sessionKey,	sizeof(ltpHeader.sessionKey));
	// [5] sequenceNumber
	memcpy(packet + sizeof(ltpHeader.messageType)
				+ sizeof(ltpHeader.controlType) + sizeof(ltpHeader.ripd)
				+ sizeof(ltpHeader.sessionKey), &ltpHeader.sequenceNumber,
				sizeof(ltpHeader.sequenceNumber));
	_icnCoreMutex.lock();
	_icnCore->publish_data_isub(cId.binIcnId(), DOMAIN_LOCAL, NULL, 0,
			rCId.binIcnId(), packet, packetSize);
	_icnCoreMutex.unlock();
	LOG4CXX_TRACE(logger, "LTP CTRL-WE published under " << cId.print()
			<< " > SK " << ltpHeader.sessionKey << " > Sequence "
			<< sequenceNumber);
	_addWindowEnd(rCId, ltpHeader);
	free(packet);
}

void Lightweight::_publishWindowEnded(IcnId &cid, IcnId &rCid,
			ltp_hdr_ctrl_wed_t &ltpHeaderCtrlWed)
{
	uint8_t *packet = (uint8_t *)malloc(sizeof(ltp_hdr_ctrl_wed_t));
	uint8_t offset = 0;
	ltpHeaderCtrlWed.messageType = LTP_CONTROL;
	ltpHeaderCtrlWed.controlType = LTP_CONTROL_WINDOW_ENDED;
	// [1] message type
	memcpy(packet, &ltpHeaderCtrlWed.messageType,
			sizeof(ltpHeaderCtrlWed.messageType));
	offset += sizeof(ltpHeaderCtrlWed.messageType);
	// [2] control type
	memcpy(packet + offset, &ltpHeaderCtrlWed.controlType,
			sizeof(ltpHeaderCtrlWed.controlType));
	offset += sizeof(ltpHeaderCtrlWed.controlType);
	// [3]
	memcpy(packet + offset, &ltpHeaderCtrlWed.ripd,
			sizeof(ltpHeaderCtrlWed.ripd));
	offset += sizeof(ltpHeaderCtrlWed.ripd);
	// [4] Session key
	memcpy(packet + offset, &ltpHeaderCtrlWed.sessionKey,
			sizeof(ltpHeaderCtrlWed.sessionKey));
	_icnCoreMutex.lock();
	_icnCore->publish_data_isub(cid.binIcnId(), DOMAIN_LOCAL, NULL, 0,
			rCid.binIcnId(), packet, sizeof(ltp_hdr_ctrl_wed_t));
	_icnCoreMutex.unlock();
	LOG4CXX_TRACE(logger, "LTP CTRL-WED published under CID "
			<< cid.print() << " with rCID " << rCid.print() << " > SK "
			<< ltpHeaderCtrlWed.sessionKey);
	free(packet);
}

void Lightweight::_publishWindowEnded(IcnId &rCId, NodeId &nodeId,
		uint16_t &sessionKey)
{
	if (!_forwardingEnabled(nodeId))
	{
		LOG4CXX_DEBUG(logger, "WED message could not be published. Forwarding "
				"state for NID " << nodeId.uint() << " is still disabled");
		return;
	}
	ltp_hdr_ctrl_wed_t ltpHeader;
	list<string> nodeIdList;
	uint8_t *packet = (uint8_t *)malloc(sizeof(ltp_hdr_ctrl_wed_t));
	ltpHeader.messageType = LTP_CONTROL;
	ltpHeader.controlType = LTP_CONTROL_WINDOW_ENDED;
	ltpHeader.ripd = 0;
	ltpHeader.sessionKey = sessionKey;
	// [1] message type
	memcpy(packet, &ltpHeader.messageType, sizeof(ltpHeader.messageType));
	// [2] control type
	memcpy(packet + sizeof(ltpHeader.messageType), &ltpHeader.controlType,
			sizeof(ltpHeader.controlType));
	// [3]
	memcpy(packet + sizeof(ltpHeader.messageType)
			+ sizeof(ltpHeader.controlType), &ltpHeader.ripd,
			sizeof(ltpHeader.ripd));
	// [4] Session key
	memcpy(packet + sizeof(ltpHeader.messageType)
			+ sizeof(ltpHeader.controlType) + sizeof(ltpHeader.ripd),
			&ltpHeader.sessionKey, sizeof(ltpHeader.sessionKey));
	nodeIdList.push_back(nodeId.str());
	_icnCoreMutex.lock();
	_icnCore->publish_data(rCId.binIcnId(), DOMAIN_LOCAL, NULL, 0,
			nodeIdList, packet, sizeof(ltp_hdr_ctrl_wed_t));
	_icnCoreMutex.unlock();
	LOG4CXX_TRACE(logger, "LTP CTRL-WED published to NID " << nodeId.uint()
			<< " under rCID " << rCId.print() << " > SK "
			<< ltpHeader.sessionKey);
	free(packet);
}

void Lightweight::_publishWindowUpdate(IcnId &rCid, uint16_t &sessionKey,
		list<NodeId> &nodeIds)
{
	list<string> nodeIdsStr;
	list<NodeId>::iterator nodeIdsIt;
	ostringstream oss;
	for (nodeIdsIt = nodeIds.begin(); nodeIdsIt != nodeIds.end(); nodeIdsIt++)
	{
		if (!_forwardingEnabled(*nodeIdsIt))
		{
			LOG4CXX_DEBUG(logger, "Forwarding state of NID "
					<< nodeIdsIt->uint() << " is currently disabled.");
		}
		else
		{
			_addNodeIdToWindowUpdate(rCid, sessionKey, *nodeIdsIt);
			nodeIdsStr.push_back(nodeIdsIt->str());
			oss << nodeIdsIt->uint() << " ";
		}
	}
	if (nodeIdsStr.empty())
	{
		LOG4CXX_WARN(logger, "All NIDs are not reachable");
		return;
	}
	ltp_hdr_ctrl_wu_t ltpHeader;
	uint8_t *packet = (uint8_t *)malloc(sizeof(ltp_hdr_ctrl_wed_t));
	ltpHeader.messageType = LTP_CONTROL;
	ltpHeader.controlType = LTP_CONTROL_WINDOW_UPDATE;
	ltpHeader.ripd = 0;
	ltpHeader.sessionKey = sessionKey;
	// [1] message type
	memcpy(packet, &ltpHeader.messageType, sizeof(ltpHeader.messageType));
	// [2] control type
	memcpy(packet + sizeof(ltpHeader.messageType), &ltpHeader.controlType,
			sizeof(ltpHeader.controlType));
	// [3] 0
	memcpy(packet + sizeof(ltpHeader.messageType)
			+ sizeof(ltpHeader.controlType), &ltpHeader.ripd,
			sizeof(ltpHeader.ripd));
	// [4] Session key
	memcpy(packet + sizeof(ltpHeader.messageType)
			+ sizeof(ltpHeader.controlType) + sizeof(ltpHeader.ripd),
			&ltpHeader.sessionKey, sizeof(ltpHeader.sessionKey));
	_icnCoreMutex.lock();
	_icnCore->publish_data(rCid.binIcnId(), DOMAIN_LOCAL, NULL, 0, nodeIdsStr,
			packet, sizeof(ltp_hdr_ctrl_wed_t));
	_icnCoreMutex.unlock();
	LOG4CXX_TRACE(logger, "LTP CTRL-WU published under rCID "
			<< rCid.print() << " > SK "
			<< sessionKey << " to " << nodeIds.size() << " NID(s): "
			<< oss.str());
	free(packet);
}

void Lightweight::_publishWindowUpdated(IcnId &cid, IcnId &rCid,
		ltp_hdr_ctrl_wud_t &ltpHeaderControlWud)
{
	uint8_t *packet = (uint8_t *)malloc(sizeof(ltp_hdr_ctrl_wud_t));
	uint8_t offset = 0;
	// [1] message type
	memcpy(packet, &ltpHeaderControlWud.messageType,
			sizeof(ltpHeaderControlWud.messageType));
	offset += sizeof(ltpHeaderControlWud.messageType);
	// [2] control type
	memcpy(packet + offset, &ltpHeaderControlWud.controlType,
			sizeof(ltpHeaderControlWud.controlType));
	offset += sizeof(ltpHeaderControlWud.controlType);
	// [3] 0
	memcpy(packet + offset, &ltpHeaderControlWud.ripd,
			sizeof(ltpHeaderControlWud.ripd));
	offset += sizeof(ltpHeaderControlWud.ripd);
	// [4] Session key
	memcpy(packet + offset, &ltpHeaderControlWud.sessionKey,
			sizeof(ltpHeaderControlWud.sessionKey));
	_icnCoreMutex.lock();
	_icnCore->publish_data_isub(cid.binIcnId(), DOMAIN_LOCAL, NULL, 0,
			rCid.binIcnId(), packet, sizeof(ltp_hdr_ctrl_wud_t));
	_icnCoreMutex.unlock();
	LOG4CXX_TRACE(logger, "LTP CTRL-WUD published under CID "
			<< cid.print() << " with rCID " << rCid.print() << " > 0 "
			<< ltpHeaderControlWud.ripd << " > SK "
			<< ltpHeaderControlWud.sessionKey);
	free(packet);
}

void Lightweight::_removeNidsFromCmcGroup(IcnId &rCid, uint16_t &sessionKey,
		list<NodeId> &nodeIds)
{
	//map<0,   map<SK,       list<NID
	map<uint32_t, map<uint16_t, list<NodeId>>>::iterator cmcPridIt;
	//map<SK,     list<NID
	map<uint16_t, list<NodeId>>::iterator cmcSkIt;
	_cmcGroupsMutex->lock();
	_cmcGroupsIt = _cmcGroups->find(rCid.uint());
	if (_cmcGroupsIt == _cmcGroups->end())
	{
		_cmcGroupsMutex->unlock();
		return;
	}
	cmcPridIt = _cmcGroupsIt->second.find(0);
	if (cmcPridIt == _cmcGroupsIt->second.end())
	{
		_cmcGroupsMutex->unlock();
		return;
	}
	cmcSkIt = cmcPridIt->second.find(sessionKey);
	if (cmcSkIt == cmcPridIt->second.end())
	{
		_cmcGroupsMutex->unlock();
		return;
	}
	list<NodeId>::iterator cmcNidsIt;
	list<NodeId>::iterator unconfirmedNidsIt;
	// Iterate over unconfirmed NIDs
	unconfirmedNidsIt = nodeIds.begin();
	while (unconfirmedNidsIt != nodeIds.end())
	{
		cmcNidsIt = cmcSkIt->second.begin();
		// Find this NID in CMC group
		while (cmcNidsIt != cmcSkIt->second.end())
		{
			// NID found
			if (unconfirmedNidsIt->uint() == cmcNidsIt->uint())
			{
				cmcSkIt->second.erase(cmcNidsIt);
				LOG4CXX_TRACE(logger, "NID " << unconfirmedNidsIt->uint()
						<< " removed from CMC group rCID " << rCid.print()
						<< " > SK " << sessionKey);
				break;
			}
			cmcNidsIt++;
		}
		unconfirmedNidsIt++;
	}
	_cmcGroupsMutex->unlock();
}

void Lightweight::_removeNidsFromWindowUpdate(IcnId &rCid,
		uint16_t &sessionKey, list<NodeId> &nodeIds)
{
	list<NodeId>::iterator nidsIt;
	//map<NID
	map<uint32_t, bool>::iterator nidIt;
	//map<SK      map<NID
	map<uint16_t, map<uint32_t, bool>>::iterator skIt;
	//map<0    map<SK        map<NID
	map<uint32_t, map<uint16_t, map<uint32_t, bool>>>::iterator pridIt;
	_windowUpdateMutex.lock();
	_windowUpdateIt = _windowUpdate.find(rCid.uint());
	// rCID does not exist
	if (_windowUpdateIt == _windowUpdate.end())
	{
		LOG4CXX_WARN(logger, "rCID " << rCid.print() << " does not exist in "
				"list of awaited CTRL-WEDs");
		_windowUpdateMutex.unlock();
		return;
	}
	pridIt = _windowUpdateIt->second.find(0);
	//  does not exist
	if (pridIt == _windowUpdateIt->second.end())
	{
		LOG4CXX_WARN(logger, "rCID " << rCid.print() << " does not exist in "
				"list of awaited CTRL-WEDs");
		_windowUpdateMutex.unlock();
		return;
	}
	skIt = pridIt->second.find(sessionKey);
	// SK does not exist
	if (skIt == pridIt->second.end())
	{
		LOG4CXX_WARN(logger, "rCID " << rCid.print() << " > SK " << sessionKey
				<< " does not exist in list of awaited CTRL-WEDs");
		_windowUpdateMutex.unlock();
		return;
	}
	// Iterate over list of NIDs and remove them if they exist
	for (nidsIt = nodeIds.begin(); nidsIt != nodeIds.end(); nidsIt++)
	{
		nidIt = skIt->second.find(nidsIt->uint());
		// NID found
		if (nidIt != skIt->second.end())
		{
			skIt->second.erase(nidIt);
			LOG4CXX_TRACE(logger, "NID " << nidsIt->uint() << " removed from "
					"awaited WED list for rCID " << rCid.print() << " > SK "
					<< sessionKey << ". " << skIt->second.size()
					<< " NIDs left in list");
		}
	}
	_windowUpdateMutex.unlock();
}

list<NodeId> Lightweight::_wudsNotReceived(IcnId &rCid, uint16_t &sessionKey)
{
	list<NodeId> unconfirmedNids;
	//map<NID
	map<uint32_t, bool>::iterator nidIt;
	//map<SK,     map<NID
	map<uint16_t, map<uint32_t, bool>>::iterator skIt;
	//map<0,   map<SK,       map<NID
	map<uint32_t, map<uint16_t, map<uint32_t, bool>>>::iterator pridIt;
	_windowUpdateIt = _windowUpdate.find(rCid.uint());
	if (_windowUpdateIt == _windowUpdate.end())
	{
		LOG4CXX_WARN(logger, "rCID " << rCid.print() << " not found in WU map");
		return unconfirmedNids;
	}
	pridIt = _windowUpdateIt->second.find(0);
	if (pridIt == _windowUpdateIt->second.end())
	{
		LOG4CXX_WARN(logger, "rCID " << rCid.print() << " not found in WU map");
		return unconfirmedNids;
	}
	skIt = pridIt->second.find(sessionKey);
	if (skIt == pridIt->second.end())
	{
		LOG4CXX_WARN(logger, "rCID " << rCid.print() << " > SK " << sessionKey
				<< " not found in WU map");
		return unconfirmedNids;
	}
	for (nidIt = skIt->second.begin(); nidIt != skIt->second.end(); nidIt++)
	{
		if (!nidIt->second)
		{
			NodeId nodeId(nidIt->first);
			unconfirmedNids.push_back(nodeId);
		}
	}
	return unconfirmedNids;
}

uint16_t Lightweight::_rtt()
{
	uint32_t rtt = 0;
	forward_list<uint16_t>::iterator it;
	_rttsMutex.lock();
	it = _rtts.begin();
	// calculate sum
	while (it != _rtts.end())
	{
		rtt = rtt + (uint32_t)*it;
		it++;
	}
	_rttsMutex.unlock();
	// calculate mean
	rtt = ceil(rtt / _configuration.ltpRttListSize());
	LOG4CXX_TRACE(logger, "RTT mean is " << rtt << "ms");
	return (uint16_t)rtt;
}

void Lightweight::_rtt(uint16_t rtt)
{
	if (rtt == 0)
	{
		rtt = 1;//[ms]
		LOG4CXX_TRACE(logger, "RTT was given as 0. Assuming 1ms instead");
	}
	_rttsMutex.lock();
	// delete oldest entry first if list size has reached its max size
	_rtts.resize(_configuration.ltpRttListSize());
	_rtts.push_front(rtt);
	_rttsMutex.unlock();
}
