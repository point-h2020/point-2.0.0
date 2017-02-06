/*
 * http.cc
 *
 *  Created on: 16 Apr 2016
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

#include "http.hh"

#ifdef DMALLOC
#include "dmalloc.h"
#endif

using namespace cleaners::httpbuffer;
using namespace namespaces::http;

LoggerPtr Http::logger(Logger::getLogger("namespaces.http"));

Http::Http(Blackadder *icnCore, Configuration &configuration,
		Transport &transport, Statistics &statistics)
	: _icnCore(icnCore),
	  _configuration(configuration),
	  _transport(transport),
	  _statistics(statistics)
{
	// Initialising HTTP buffer cleaner
	HttpBufferCleaner httpBufferCleaner((void *)&_packetBufferRequests,
			(void *)&_packetBufferRequestsMutex, _configuration);
	boost::thread httpBufferThread(httpBufferCleaner);
	_httpBufferThread = &httpBufferThread;
}

Http::~Http(){}

void Http::addReversNidTorCIdLookUp(string &nodeId, IcnId &rCId)
{
	NodeId nid(nodeId);
	_reverseNidTorCIdLookUpMutex.lock();
	_reverseNidTorCIdLookUpIt = _reverseNidTorCIdLookUp.find(nid.uint());
	// NID does not exist, add it
	if (_reverseNidTorCIdLookUpIt == _reverseNidTorCIdLookUp.end())
	{
		list<uint32_t> rCIdsList;
		rCIdsList.push_back(rCId.uint());
		_reverseNidTorCIdLookUp.insert(pair<uint32_t, list<uint32_t>>(
				nid.uint(),	rCIdsList));
		LOG4CXX_TRACE(logger, "New NID " << nid.uint() << " added to reverse "
				"rCID look up map with rCID " << rCId.print());
		_reverseNidTorCIdLookUpMutex.unlock();
		return;
	}
	// NID does exist
	list<uint32_t>::iterator it;
	it = _reverseNidTorCIdLookUpIt->second.begin();
	while (it != _reverseNidTorCIdLookUpIt->second.end())
	{
		// rCID already in list
		if (*it == rCId.uint())
		{
			_reverseNidTorCIdLookUpMutex.unlock();
			return;
		}
		it++;
	}
	// rCID not in list -> add it now
	_reverseNidTorCIdLookUpIt->second.push_back(rCId.uint());
	LOG4CXX_TRACE(logger, "rCID " << rCId.print() << " added to reverse rCID "
			"look up map for NID " << nid.uint());
	_reverseNidTorCIdLookUpMutex.unlock();
}

void Http::deleteSessionKey(uint16_t &sessionKey)
{
	//map<RIPD,   list<Session keys
	map<uint32_t, list<uint16_t>>::iterator pridIt;
	//list<Session keys
	list<uint16_t>::iterator sessionKeyIt;
	_ipEndpointSessionsMutex.lock();
	_ipEndpointSessionsIt = _ipEndpointSessions.begin();
	// Iterator over rCIDs
	while (_ipEndpointSessionsIt != _ipEndpointSessions.end())
	{
		pridIt = _ipEndpointSessionsIt->second.begin();
		// Iterate over 0s
		while (pridIt != _ipEndpointSessionsIt->second.end())
		{
			sessionKeyIt = pridIt->second.begin();
			// Iterate over Session keys
			while (sessionKeyIt != pridIt->second.end())
			{
				// Session key found
				if (*sessionKeyIt == sessionKey)
				{
					pridIt->second.erase(sessionKeyIt);
					LOG4CXX_TRACE(logger, "SK " << sessionKey
							<< " deleted from IP endpoint session list for "
							"hashed rCID "  << _ipEndpointSessionsIt->first
							<< " > RIPD " << pridIt->first);
					break;//continue searching in other rCID > RIPD lists
				}
				// check next session key
				else
				{
					sessionKeyIt++;
				}
			}
			pridIt++;
		}
		_ipEndpointSessionsIt++;
	}
	// delete all empty RIPD maps
	_ipEndpointSessionsIt = _ipEndpointSessions.begin();
	while (_ipEndpointSessionsIt != _ipEndpointSessions.end())
	{
		pridIt = _ipEndpointSessionsIt->second.begin();
		// Iterate over 0s
		while (pridIt != _ipEndpointSessionsIt->second.end())
		{
			if (pridIt->second.empty())
			{
				LOG4CXX_TRACE(logger, "List of SKs/FDs is empty for hashed rCID"
						<< " " << _ipEndpointSessionsIt->first << " > RIPD "
						<< pridIt->first << ". Deleting it");
				_ipEndpointSessionsIt->second.erase(pridIt);
				// reset iterator and continue searching for this rCID > RIPD
				if (!_ipEndpointSessionsIt->second.empty())
				{
					pridIt = _ipEndpointSessionsIt->second.begin();
				}
				else
				{
					break;
				}
			}
			pridIt++;
		}
		_ipEndpointSessionsIt++;
	}
	// delete all empty rCID maps
	_ipEndpointSessionsIt = _ipEndpointSessions.begin();
	while (_ipEndpointSessionsIt != _ipEndpointSessions.end())
	{
		if (_ipEndpointSessionsIt->second.empty())
		{
			LOG4CXX_TRACE(logger, "List of 0s empty for hashed rCID "
					<< _ipEndpointSessionsIt->first << ". Deleting it");
			_ipEndpointSessions.erase(_ipEndpointSessionsIt);
			if (!_ipEndpointSessions.empty())
			{
				_ipEndpointSessionsIt = _ipEndpointSessions.begin();
			}
			else
			{
				break;
			}
		}
		_ipEndpointSessionsIt++;
	}
	_ipEndpointSessionsMutex.unlock();
}

void Http::closeCmcGroup(IcnId &rCid, uint16_t &sessionKey)
{
	//signal socket closure down to all cNAPs (this blocks until all NIDs in CMC
	// group have replied
	_transport.Lightweight::publishEndOfSession(rCid, sessionKey);
	// now close CMC group (i.e. deleting it)
	_cmcGroupsMutex.lock();
	_cmcGroupsIt = _cmcGroups.find(rCid.uint());
	if (_cmcGroupsIt == _cmcGroups.end())
	{
		LOG4CXX_TRACE(logger, "CMC group for rCID " << rCid.print()
				<< " already closed");
		_cmcGroupsMutex.unlock();
		return;
	}
	map<uint32_t, map<uint16_t, list<NodeId>>>::iterator pridIt;
	pridIt = _cmcGroupsIt->second.find(0);
	if (pridIt == _cmcGroupsIt->second.end())
	{
		LOG4CXX_TRACE(logger, "CMC group for rCID " << rCid.print()
				<< " already closed");
		_cmcGroupsMutex.unlock();
		return;
	}
	map<uint16_t, list<NodeId>>::iterator skIt;
	skIt = pridIt->second.find(sessionKey);
	if (skIt == pridIt->second.end())
	{
		LOG4CXX_TRACE(logger, "CMC group for rCID " << rCid.print() << " > SK "
				<< sessionKey << " already closed");
		_cmcGroupsMutex.unlock();
		return;
	}
	// Delete entire list<NIDs> from rCID > RIPD > SK
	LOG4CXX_TRACE(logger, "CMC group of " << skIt->second.size() <<" deleted "
			"for rCID " << rCid.print() << " > SK " << sessionKey);
	pridIt->second.erase(skIt);
	// Check if this was the last SK for rCID > RIPD
	if (pridIt->second.empty())
	{
		_cmcGroupsIt->second.erase(pridIt);
		LOG4CXX_TRACE(logger, "Last SK for rCID " << rCid.print()
				<< ". Removed RIPD");
	}
	// Check if that was the last RIPD for this rCID
	if (_cmcGroupsIt->second.empty())
	{
		_cmcGroups.erase(_cmcGroupsIt);
		LOG4CXX_TRACE(logger, "Last RIPD for rCID " << rCid.print()
				<< ". Removed rCID too");
	}
	LOG4CXX_TRACE(logger, _cmcGroups.size() << " CMC group(s) still active");
	_cmcGroupsMutex.unlock();
}

void Http::endSession(IcnId &rCid, uint16_t &sessionKey)
{
	//list<Socket FD
	list<uint16_t>::iterator socketFdIt;
	//map<RIPD,   list<Socket FD
	map<uint32_t, list<uint16_t>>::iterator pridIt;
	// First get the socket FD for this IP endpoints (multiple possible!)
	_ipEndpointSessionsMutex.lock();
	_ipEndpointSessionsIt = _ipEndpointSessions.find(rCid.uint());
	if (_ipEndpointSessionsIt == _ipEndpointSessions.end())
	{
		LOG4CXX_TRACE(logger, "rCID " << rCid.print() << " does not exist in "
				"IP endpoint sessions map. Socket probably already closed")
		_ipEndpointSessionsMutex.unlock();
		return;
	}
	pridIt = _ipEndpointSessionsIt->second.find(0);
	if (pridIt == _ipEndpointSessionsIt->second.end())
	{
		LOG4CXX_ERROR(logger, "Does not exist in IP endpoint session map under "
				"rCID " << rCid.print());
		_ipEndpointSessionsMutex.unlock();
		return;
	}
	// now iterate over all IP endpoints and shutdown their socket
	list<uint16_t> deleteSockets;
	list<uint16_t>::iterator deleteSocketsIt;
	fd_set rset;
	for (socketFdIt = pridIt->second.begin();
			socketFdIt != pridIt->second.end(); socketFdIt++)
	{
		FD_ZERO(&rset);
		FD_SET(*socketFdIt, &rset);
		if (FD_ISSET(*socketFdIt, &rset))
		{
			LOG4CXX_DEBUG(logger, "Shutting down socket FD " << *socketFdIt
					<< " for rCID " << rCid.print());
			if (shutdown(*socketFdIt, SHUT_WR) == -1)
			{//FIXME RW shutdown must be aware of slow LTP session with NACKs
				LOG4CXX_DEBUG(logger, "Shutdown (WR) of socket FD "
						<< *socketFdIt << " failed: " << strerror(errno));
			}
		}
		else
		{
			LOG4CXX_DEBUG(logger, "Socket FD " << *socketFdIt << " not "
					"readable anymore");
			break;
		}
	}
	// iterate over sockets that should be removed from list
	for (deleteSocketsIt = deleteSockets.begin();
			deleteSocketsIt != deleteSockets.end(); deleteSocketsIt++)
	{
		for (socketFdIt = pridIt->second.begin();
					socketFdIt != pridIt->second.end(); socketFdIt++)
		{
			// Delete socket FD
			if (*socketFdIt == *deleteSocketsIt)
			{
				LOG4CXX_TRACE(logger, "Socket FD " << *deleteSocketsIt
						<< " deleted from list of IP endpoints for rCID "
						<< rCid.print());
				pridIt->second.erase(socketFdIt);
				break;
			}
		}
	}
	_ipEndpointSessionsMutex.unlock();
}

void Http::handleDnsLocal(uint32_t &fqdn)
{
	_mutexIcnIds.lock();
	_cIdsIt = _cIds.find(fqdn);
	if (_cIdsIt == _cIds.end())
	{
		LOG4CXX_TRACE(logger, "Hashed FQDN " << fqdn << " could not be found as"
				" key in CID map. So none of the IP endpoints connected to this"
				" NAP have ever issues an HTTP request to this FQDN");
		_mutexIcnIds.unlock();
		return;
	}
	_cIdsIt->second.forwarding(false);
	LOG4CXX_TRACE(logger, "Forwarding state set to false for CID "
			<< _cIdsIt->second.print());
	_icnCore->unpublish_info(_cIdsIt->second.binId(),
			_cIdsIt->second.binPrefixId(), DOMAIN_LOCAL, NULL, 0);
	LOG4CXX_TRACE(logger, "Unsubscribed from CID " << _cIdsIt->second.print());
	_mutexIcnIds.unlock();
}

void Http::handleRequest(string &fqdn, string &resource,
		http_methods_t httpMethod, uint8_t *packet, uint16_t &packetSize,
		uint16_t &sessionKey)
{
	_statistics.httpRequestsPerFqdn(fqdn, 1);
	// Create corresponding CIDs
	IcnId cId(fqdn);
	IcnId rCId(fqdn, resource);
	// Add rCID and session to known list of IP endpoint sessions
	_addIpEndpointSession(rCId, sessionKey);
	// Create scopes in RV if necessary
	_mutexIcnIds.lock();
	_cIdsIt = _cIds.find(cId.uint());
	if (_cIdsIt == _cIds.end())
	{
		_cIds.insert(pair<uint32_t, IcnId>(cId.uint(), cId));
		_mutexIcnIds.unlock();
		LOG4CXX_DEBUG(logger, "New CID " << cId.print() << " added to "
				"local database");
		_bufferRequest(cId, rCId, sessionKey, httpMethod, packet, packetSize);
		_icnCore->publish_scope(cId.binRootScopeId(),
				cId.binEmpty(), DOMAIN_LOCAL, NULL, 0);
		LOG4CXX_DEBUG(logger, "Root scope " << cId.rootScopeId()
				<< " published to domain local RV");
		/* iterate over the number of scope levels and publish them to the RV.
		 * Note, the root scope and the information item are excluded!
		 */
		for (size_t i = 2; i < cId.length() / 16; i++)
		{
			_icnCore->publish_scope(cId.binScopeId(i),
					cId.binScopePath(i - 1), DOMAIN_LOCAL, NULL, 0);
			LOG4CXX_DEBUG(logger, "Scope ID " << cId.scopeId(i)
					<< " published under scope path "
					<< cId.printScopePath(i - 1) << " to domain local "
							"RV");
		}
		LOG4CXX_DEBUG(logger, "Advertised " << cId.id() << " under scope path "
				<< cId.printPrefixId());
		_icnCore->publish_info(cId.binId(), cId.binPrefixId(), DOMAIN_LOCAL,
				NULL, 0);
		return;
	}
	// Advertise availability if forwarding rule set to disabled
	if (!(*_cIdsIt).second.forwarding())
	{
		_mutexIcnIds.unlock();
		_icnCore->publish_info(cId.binId(), cId.binPrefixId(), DOMAIN_LOCAL,
				NULL, 0);
		LOG4CXX_DEBUG(logger, "Advertised " << cId.id() << " again "
				"under scope path " << cId.printPrefixId());
		_bufferRequest(cId, rCId, sessionKey, httpMethod, packet, packetSize);
		return;
	}
	_mutexIcnIds.unlock();
	// Publish packet (blocking method to adjust TCP server socket to the NAP's
	// speed
	_transport.Lightweight::publish(cId, rCId, sessionKey, packet, packetSize);
}

bool Http::handleResponse(IcnId &rCId, uint16_t &sessionKey, bool &firstPacket,
		uint8_t *packet, uint16_t &packetSize)
{
	uint8_t attempts = ENIGMA;
	list<NodeId> cmcGroup;
	while (cmcGroup.empty())
	{
		cmcGroup.clear();
		// ICN core doesn't have the FID for any NID which awaits the response.
		// Note, _getCmcGroup returns true if packet must be buffered!
		if (_getCmcGroup(rCId, sessionKey, firstPacket, cmcGroup))
		{
			LOG4CXX_TRACE(logger, "Trying to form CMC group in 200ms again for "
					<< "rCID " << rCId.print() << " > RIPD " << " > SK "
					<< sessionKey);
			// Sleep 200ms (default RTT)
			boost::this_thread::sleep(boost::posix_time::milliseconds(200));
		}
		if (attempts == 0)
		{
			LOG4CXX_DEBUG(logger, "Giving up trying to get a CMC group for rCID"
					<< " " << rCId.print() << " > SK " << sessionKey);
			return false;
		}
		attempts--;
	}
	/*Old code below which adds the packet to the buffer. Issue is that when
	 * eventually publishing the packet the method gets called from Icn class
	 * which must not go into a blocking method. But as HTTP responses are
	 * significantly larger then HTTP requests, flow control (waiting for WUDs)
	 * will block the entire NAP and incoming WUD cannot be read. Placing the
	 * WUD reader into a thread is feasible but going back to where the
	 * publication of the HTTP response got interrupted is quite tricky. That is
	 * why the HTTP response gets published via LTP if _getCmcGroup above
	 * returns true which includes the reception of START_PUBLISH_iSUB for at
	 * least one NID in the potential CMC group
	if(_getCmcGroup(rCId, proxyRuleId, sessionKey, firstPacket, cmcGroup))
	{
		_bufferResponse(rCId, proxyRuleId, sessionKey, packet, packetSize);
	}
	// doesn't exist or not ready (all NIDs in potential CMC group may still
	// wait for receive the entire HTTP request
	if (cmcGroup.empty())
	{
		LOG4CXX_TRACE(logger, "No START_PUBLISH_iSUB has been received for any "
				"NID in the potential CMC group map. Packet will be buffered");
		return;
	}*/
	_transport.Lightweight::publish(rCId, sessionKey, cmcGroup, packet,
			packetSize);
	return true;
}

void Http::forwarding(IcnId &cId, bool state)
{
	_mutexIcnIds.lock();
	_cIdsIt = _cIds.find(cId.uint());
	// CID does not exist in icn maps
	if (_cIdsIt == _cIds.end())
	{
		_rCIdsIt = _rCIds.find(cId.uint());
		// CID does not exist in iSub icn map either
		if (_rCIdsIt == _rCIds.end())
		{
			LOG4CXX_WARN(logger, "CID " << cId.print() << " is unknown. "
				"Forwarding state cannot be changed");
			_mutexIcnIds.unlock();
			return;
		}
	}
	// CID is HTTP request
	if (_cIdsIt != _cIds.end())
	{
		_cIdsIt->second.forwarding(state);
		LOG4CXX_DEBUG(logger, "Forwarding for CID " << cId.print() << " set "
				<< "to " << state);
	}
	// CID is HTTP response
	else if (_rCIdsIt != _rCIds.end())
	{
		_rCIdsIt->second.forwarding(state);
		LOG4CXX_DEBUG(logger, "Forwarding for iSub CID " << cId.print()
				<< " set to " << state);
	}
	// error case. should not occur, but just in case
	else
	{
		LOG4CXX_ERROR(logger, "CID " << cId.print() << " initially found but "
				"iterators say differently?!");
	}
	_mutexIcnIds.unlock();
}

void Http::forwarding(NodeId &nodeId, bool state)
{
	_nIdsMutex.lock();
	_nIdsIt = _nIds.find(nodeId.uint());
	// New NID
	if (_nIdsIt == _nIds.end())
	{
		LOG4CXX_WARN(logger, "NID " << nodeId.uint() << " is not known. "
				"Ignoring forwarding state change");
	}
	// known NID
	else
	{
		_nIdsIt->second = state;
		LOG4CXX_DEBUG(logger, "State for existing NID " << nodeId.uint()
				<< " updated to " << state);
	}
	_nIdsMutex.unlock();
}

void Http::initialise()
{
	if (!_configuration.httpHandler())
	{
		return;
	}
	list<pair<IcnId, pair<IpAddress, uint16_t>>> fqdns = _configuration.fqdns();
	list<pair<IcnId, pair<IpAddress, uint16_t>>>::iterator fqdnsIt;
	IcnId cid("video.point");//Dummy CID to publish root scope
	_icnCore->publish_scope(cid.binRootScopeId(), cid.binEmpty(), DOMAIN_LOCAL,
			NULL, 0);
	LOG4CXX_DEBUG(logger, "Publish HTTP root scope to domain local RV");
	for (fqdnsIt = fqdns.begin(); fqdnsIt != fqdns.end(); fqdnsIt++)
	{
		subscribeToFqdn(fqdnsIt->first);
		// reporting this server to the statistics module
		_statistics.ipEndpointAdd(fqdnsIt->second.first,
				fqdnsIt->first.printFqdn(), fqdnsIt->second.second);
	}
	LOG4CXX_INFO(logger, "HTTP namespace successfully initialised");
	// Initialise LTP with the pointers to the CMC and NID maps and mutexes
	_transport.Lightweight::initialise((void *)&_potentialCmcGroups,
			(void *)&_potentialCmcGroupsMutex, (void *)&_nIds,
			(void *)&_nIdsMutex, (void *)&_cmcGroups, (void *)&_cmcGroupsMutex);
}

void Http::publishFromBuffer(IcnId &cId)
{
	map<uint32_t, request_packet_t>::iterator packetBufferIt;
	_packetBufferRequestsMutex.lock();
	_packetBufferRequestsIt = _packetBufferRequests.find(cId.uint());
	// HTTP request(s) to be published
	if (_packetBufferRequestsIt != _packetBufferRequests.end())
	{
		// Iterate over the entire iSub CID map which is the value to the key
		for (packetBufferIt = _packetBufferRequestsIt->second.begin();
				packetBufferIt != _packetBufferRequestsIt->second.end();
				packetBufferIt++)
		{
			LOG4CXX_TRACE(logger, "Packet of length "
					<< packetBufferIt->second.packetSize << " retrieved from "
					"buffer and handed over to LTP for publication under CID "
					<< packetBufferIt->second.cId.print());
			_transport.Lightweight::publishFromBuffer(
					packetBufferIt->second.cId,
					packetBufferIt->second.rCId,
					packetBufferIt->second.sessionKey,
					packetBufferIt->second.packet,
					packetBufferIt->second.packetSize);
			// free up allocated memory for packet
			free(packetBufferIt->second.packet);
		}
		// now delete all packets that were buffered for this particular CID
		LOG4CXX_TRACE(logger, _packetBufferRequestsIt->second.size()
				<< " packet(s) deleted from buffer for CID " << cId.print());
		_packetBufferRequests.erase(_packetBufferRequestsIt);
		_packetBufferRequestsMutex.unlock();
		return;
	}
	_packetBufferRequestsMutex.unlock();
	LOG4CXX_TRACE(logger, "No packet could be found in buffer for CID "
			<< cId.print());
}

void Http::publishFromBuffer(NodeId &nodeId)
{
	list<uint32_t> rCids;
	list<uint32_t>::iterator rCidsIt;
	rCids = _getrCids(nodeId);
	bool firstPacket = true;
	if (rCids.size() == 0)
	{
		LOG4CXX_DEBUG(logger, "List of rCIDs is empty for NID " << nodeId.uint()
				<< " nothing to be published to this node");
		return;
	}
	// First check the CMC group which
	//walk through buffer and publish the buffered HTTP response(s)
	map<uint32_t, stack<response_packet_t>>::iterator pridMapIt;
	_packetBufferResponsesMutex.lock();
	// Iterate over the rCIDs obtained for this particular NID
	for (rCidsIt = rCids.begin(); rCidsIt != rCids.end(); rCidsIt++)
	{
		_packetBufferResponsesIt = _packetBufferResponses.find(*rCidsIt);
		if (_packetBufferResponsesIt == _packetBufferResponses.end())
		{
			LOG4CXX_TRACE(logger, "Nothing to be retrieved from ICN packet "
					"buffer. rCID could not be found for NID "
					<< nodeId.uint());
			_packetBufferResponsesMutex.unlock();
			return;
		}
		pridMapIt = _packetBufferResponsesIt->second.begin();
		// Iterate over 0s
		while (pridMapIt != _packetBufferResponsesIt->second.end())
		{
			LOG4CXX_TRACE(logger, pridMapIt->second.size() << " packet(s) "
					"available to retrieve from ICN packet buffer for NID "
					<< nodeId.uint());
			// publish from stack as long as it is not empty
			while (!pridMapIt->second.empty())
			{
				bool bufferPacket = false;
				list<NodeId> nodeIds;
				bufferPacket = _getCmcGroup(pridMapIt->second.top().rCId,
						pridMapIt->second.top().sessionKey,
						firstPacket, nodeIds);
				_transport.Lightweight::publish(pridMapIt->second.top().rCId,
						pridMapIt->second.top().sessionKey, nodeIds,
						pridMapIt->second.top().packet,
						pridMapIt->second.top().packetSize);
				// free allocated packet memory
				free(pridMapIt->second.top().packet);
				if (!bufferPacket)
				{
					pridMapIt->second.pop();
				}
				/*
				 * This sNAP has received a START_PUBLISH_iSUB for a particular
				 * NID and this method has been called from the class Icn. To
				 * avoid creating a new CMC group when reading any further
				 * packet for this particular RIPD session, this boolean must be
				 * set to false. For more information on CMC group management
				 * please read the corresponding section in the doc file located
				 * in nap/doc/tex
				 */
				firstPacket = false;
			}
			pridMapIt++;
		}
		// and delete this packet from buffer
		_packetBufferResponses.erase(_packetBufferResponsesIt);
	}
	_packetBufferResponsesMutex.unlock();
	// delete the rCIDs from the reverse lookup table
	_deleterCids(nodeId);
	//TODO check somehow if CMC was closed by actual web server
}

void Http::sendToEndpoint(IcnId &rCid, uint8_t *packet, uint16_t &packetSize)
{
	//list<Socket FD
	list<uint16_t>::iterator socketFdIt;
	//map<RIPD,   list<Socket FD
	map<uint32_t, list<uint16_t>>::iterator pridIt;
	// First get the socket FD for this IP endpoints (multiple possible!)
	_ipEndpointSessionsMutex.lock();
	_ipEndpointSessionsIt = _ipEndpointSessions.find(rCid.uint());
	if (_ipEndpointSessionsIt == _ipEndpointSessions.end())
	{
		LOG4CXX_DEBUG(logger, "rCID " << rCid.print() << " does not exist in "
				"IP endpoint sessions map with " << _ipEndpointSessions.size()
				<< " keys. Dropping packet")
		_ipEndpointSessionsMutex.unlock();
		return;
	}
	pridIt = _ipEndpointSessionsIt->second.find(0);
	if (pridIt == _ipEndpointSessionsIt->second.end())
	{
		_ipEndpointSessionsMutex.unlock();
		return;
	}
	LOG4CXX_TRACE(logger, "IP packet will be send to " << pridIt->second.size()
			<< " IP endpoint(s)");
	// now iterate over all IP endpoints that await the HTTP response
	int bytesWritten;
	uint8_t offset;
	list<uint16_t> deleteSockets;
	list<uint16_t>::iterator deleteSocketsIt;
	fd_set rset;
	for (socketFdIt = pridIt->second.begin();
			socketFdIt != pridIt->second.end(); socketFdIt++)
	{
		bytesWritten = 0;
		offset = 0;
		FD_ZERO(&rset);
		FD_SET(*socketFdIt, &rset);
		// Write until all bytes have been sent off
		while(bytesWritten != packetSize)
		{
			if (FD_ISSET(*socketFdIt, &rset))
			{
				bytesWritten = write(*socketFdIt, packet + offset, packetSize
						- offset);
			}
			else
			{
				LOG4CXX_DEBUG(logger, "Socket FD " << *socketFdIt << " not "
						"readable anymore");
				break;
			}
			// write again
			if (bytesWritten < 0 && errno == EINTR)
			{
				LOG4CXX_DEBUG(logger, "HTTP response of length "
						<< packetSize << " could not be sent to IP endpoint"
						<< " using FD " << *socketFdIt << ". Will try again"
						);
				bytesWritten = 0;
			}
			else if (bytesWritten == -1)
			{
				LOG4CXX_ERROR(logger, "HTTP response of length "
						<< packetSize << " could not be sent to IP endpoint"
						" using FD " << *socketFdIt << ": "
						<< strerror(errno));
				bytesWritten = packetSize;
				deleteSockets.push_back(*socketFdIt);
				LOG4CXX_TRACE(logger, "Closing socket FD " << *socketFdIt);
				close(*socketFdIt);
			}
			else if(bytesWritten < (packetSize - offset))
			{
				offset += bytesWritten;
				LOG4CXX_TRACE(logger, "HTTP response of length "
						<< packetSize << " could not be fully sent to IP "
						"endpoint using FD " << *socketFdIt << ". Only "
						<< bytesWritten << " bytes were written");//. Sending "
						//"off the remaining bits");
				bytesWritten = packetSize;
				deleteSockets.push_back(*socketFdIt);
			}
			else
			{
				LOG4CXX_TRACE(logger, "Entire HTTP response of length "
						<< bytesWritten	<< " sent off to IP endpoint using "
						"socket FD " << *socketFdIt);
			}
		}
	}
	// iterate over sockets that should be removed from list
	for (deleteSocketsIt = deleteSockets.begin();
			deleteSocketsIt != deleteSockets.end(); deleteSocketsIt++)
	{
		for (socketFdIt = pridIt->second.begin();
					socketFdIt != pridIt->second.end(); socketFdIt++)
		{
			// Delete socket FD
			if (*socketFdIt == *deleteSocketsIt)
			{
				LOG4CXX_TRACE(logger, "Socket FD " << *deleteSocketsIt
						<< " deleted from list of IP endpoints for rCID "
						<< rCid.print());
				pridIt->second.erase(socketFdIt);
				break;
			}
		}
	}
	_ipEndpointSessionsMutex.unlock();
}

void Http::subscribeToFqdn(IcnId &cid)
{
	_icnCore->subscribe_info(cid.binId(), cid.binPrefixId(), DOMAIN_LOCAL, NULL,
			0);
	LOG4CXX_DEBUG(logger, "Subscribed to " << cid.print() << " ("
			<< cid.printFqdn() << ")");
}

void Http::uninitialise()
{
	if (!_configuration.httpHandler())
	{
		return;
	}
	_httpBufferThread->interrupt();
	//TODO _icnCore->unsubscribe_info()
	//TODO clean LTP buffers (deleting allocated memory for packet pointers)
	LOG4CXX_INFO(logger, "HTTP namespace uninitialised");
}

void Http::unsubscribeFromFqdn(IcnId &cid)
{
	_icnCore->unsubscribe_info(cid.binId(), cid.binPrefixId(), DOMAIN_LOCAL,
			NULL, 0);
	LOG4CXX_DEBUG(logger, "Unsubscribed from " << cid.print());
}

void Http::_addIpEndpointSession(IcnId &rCId, uint16_t &sessionKey)
{
	_ipEndpointSessionsMutex.lock();
	_ipEndpointSessionsIt = _ipEndpointSessions.find(rCId.uint());
	// New rCID
	if (_ipEndpointSessionsIt == _ipEndpointSessions.end())
	{
		list<uint16_t> sessionKeyList;
		map<uint32_t, list<uint16_t>> proxyRuleIdMap;
		sessionKeyList.push_back(sessionKey);
		proxyRuleIdMap.insert(pair<uint32_t, list<uint16_t>>(0,
				sessionKeyList));
		_ipEndpointSessions.insert(pair<uint32_t, map<uint32_t, list<uint16_t>>>
				(rCId.uint(), proxyRuleIdMap));
		LOG4CXX_TRACE(logger, "New SK " << sessionKey << " added to new rCID "
				<< rCId.print() << " map");
		_ipEndpointSessionsMutex.unlock();
		return;
	}
	// rCID exists
	map<uint32_t, list<uint16_t>>::iterator proxyRuleIdMapIt;
	proxyRuleIdMapIt = _ipEndpointSessionsIt->second.find(0);
	// new RIPD
	if (proxyRuleIdMapIt == _ipEndpointSessionsIt->second.end())
	{
		list<uint16_t> sessionKeyList;
		sessionKeyList.push_back(sessionKey);
		_ipEndpointSessionsIt->second.insert(pair<uint32_t, list<uint16_t>>(0,
				sessionKeyList));
		LOG4CXX_TRACE(logger, "New SK " << sessionKey << " added to known rCID "
				<< rCId.print() << " map");
		_ipEndpointSessionsMutex.unlock();
		return;
	}
	// RIPD exists. Iterate over list to find session key. If it does exist,
	// exit. If not, add it
	list<uint16_t>::iterator sessionKeyListIt;
	sessionKeyListIt = proxyRuleIdMapIt->second.begin();
	while (sessionKeyListIt != proxyRuleIdMapIt->second.end())
	{
		if (*sessionKeyListIt == sessionKey)
		{
			_ipEndpointSessionsMutex.unlock();
			return;
		}
		sessionKeyListIt++;
	}
	// Session key does not exist on list. Add it now
	proxyRuleIdMapIt->second.push_back(sessionKey);
	LOG4CXX_TRACE(logger, "New SK " << sessionKey << " added to rCID "
			<< rCId.print() << " map of known IP endpoint sessions");
	_ipEndpointSessionsMutex.unlock();
}

void Http::_bufferRequest(IcnId &cId, IcnId &rCId, uint16_t &sessionKey,
		http_methods_t httpMethod, uint8_t *packet, uint16_t &packetSize)
{
	map<uint32_t, request_packet_t>::iterator packetBufferIt;
	_packetBufferRequestsMutex.lock();
	_packetBufferRequestsIt = _packetBufferRequests.find(cId.uint());
	// ICN ID does not exist
	if (_packetBufferRequestsIt == _packetBufferRequests.end())
	{
		request_packet_t packetDescription;
		packetDescription.cId = cId;
		packetDescription.rCId = rCId;
		packetDescription.ripd = 0;
		packetDescription.sessionKey = sessionKey;
		packetDescription.httpMethod = httpMethod;
		packetDescription.packet = (uint8_t *)malloc(packetSize);
		memcpy(packetDescription.packet, packet, packetSize);
		packetDescription.packetSize = packetSize;
		packetDescription.timestamp =
				boost::posix_time::second_clock::local_time();
		map<uint32_t, request_packet_t> rCIdMap;
		rCIdMap.insert(pair<uint32_t, request_packet_t>(rCId.uint(),
				packetDescription));
		_packetBufferRequests.insert(pair<uint32_t,map<uint32_t,
				request_packet_t>>(cId.uint(), rCIdMap));
		LOG4CXX_TRACE(logger, "Packet for new CID " << cId.print() << " (FQDN: "
				<< cId.printFqdn() << "), rCID " << rCId.print() << " and "
				"SK " << sessionKey <<" of length " << packetSize << " has been"
						" added to HTTP packet buffer");
	}
	// ICN ID does exist
	else
	{
		// check if rCID exist
		packetBufferIt = (*_packetBufferRequestsIt).second.find(rCId.uint());
		if (packetBufferIt == (*_packetBufferRequestsIt).second.end())
		{
			request_packet_t packetDescription;
			packetDescription.cId = cId;
			packetDescription.rCId = rCId;
			packetDescription.ripd = 0;
			packetDescription.sessionKey = sessionKey;
			packetDescription.httpMethod = httpMethod;
			packetDescription.packet = (uint8_t *)malloc(packetSize);
			memcpy(packetDescription.packet, packet, packetSize);
			packetDescription.packetSize = packetSize;
			packetDescription.timestamp =
							boost::posix_time::second_clock::local_time();
			(*_packetBufferRequestsIt).second.insert(pair<uint32_t,
					request_packet_t>(rCId.uint(),packetDescription));
			LOG4CXX_TRACE(logger, "Packet for existing CID " << cId.print()
					<< " (FQDN: " << cId.printFqdn() << ") but new rCID "
					"of length " << packetSize << " has been added to HTTP "
					"packet buffer SK "	<< sessionKey << ")");
		}
		// rCId exists. updating packet description
		else
		{
			packetBufferIt->second.cId = cId;
			packetBufferIt->second.rCId = rCId;
			packetBufferIt->second.ripd = 0;
			packetBufferIt->second.sessionKey = sessionKey;
			packetBufferIt->second.httpMethod = httpMethod;
			free(packetBufferIt->second.packet);
			packetBufferIt->second.packet = (uint8_t *)malloc(packetSize);
			memcpy(packetBufferIt->second.packet, packet, packetSize);
			packetBufferIt->second.packetSize = packetSize;
			packetBufferIt->second.timestamp =
							boost::posix_time::second_clock::local_time();
			LOG4CXX_TRACE(logger, "Packet for existing CID and iSub CID"
					<< cId.print() << " (FQDN: " << cId.printFqdn() << ") "
					"of length " << packetSize << " has been added to HTTP "
					"packet buffer");
		}
	}
	_packetBufferRequestsMutex.unlock();
}

void Http::_bufferResponse(IcnId &rCId, uint16_t &sessionKey, uint8_t *packet,
		uint16_t &packetSize)
{
	response_packet_t packetStruct;
	packetStruct.rCId = rCId;
	packetStruct.ripd = 0;
	packetStruct.sessionKey = sessionKey;
	packetStruct.packet = (uint8_t *)malloc(packetSize);
	memcpy(packetStruct.packet, packet, packetSize);
	packetStruct.timestamp = boost::posix_time::second_clock::local_time();
	_packetBufferResponsesMutex.lock();
	_packetBufferResponsesIt = _packetBufferResponses.find(rCId.uint());
	//rCID does not exists
	if (_packetBufferResponsesIt == _packetBufferResponses.end())
	{
		stack<response_packet_t> packetStack;
		map<uint32_t, stack<response_packet_t>> pridMap;
		packetStack.push(packetStruct);
		pridMap.insert(pair<uint32_t, stack<response_packet_t>>(0,
				packetStack));
		_packetBufferResponses.insert(pair<uint32_t, map<uint32_t,
				stack<response_packet_t>>>(rCId.uint(), pridMap));
		_packetBufferResponsesMutex.unlock();
		LOG4CXX_TRACE(logger, "Proxy packet of length " << packetSize << " has "
				"been added to response packet buffer for new rCID "
				<< rCId.print());
		return;
	}
	map<uint32_t, stack<response_packet_t>>::iterator pridMapIt;
	pridMapIt = _packetBufferResponsesIt->second.find(0);
	// RIPD does not exist
	if (pridMapIt == _packetBufferResponsesIt->second.end())
	{
		stack<response_packet_t> packetStack;
		packetStack.push(packetStruct);
		_packetBufferResponsesIt->second.insert(pair<uint32_t,
				stack<response_packet_t>>(0, packetStack));
		_packetBufferResponsesMutex.unlock();
		LOG4CXX_TRACE(logger, "Proxy packet of length " << packetSize << " has "
			"been added to response packet buffer for existing rCID "
				<< rCId.print());
		return;
	}
	// RIPD does exist
	pridMapIt->second.push(packetStruct);
	LOG4CXX_TRACE(logger, "Proxy packet of length " << packetSize << " has "
		"been added to response packet buffer for existing rCID "
			<< rCId.print());
	_packetBufferResponsesMutex.unlock();
}

void Http::_createCmcGroup(IcnId &rCid, uint16_t &sessionKey,
		list<NodeId> &nodeIds)
{
	//map<SK,     list<NID
	map<uint16_t, list<NodeId>> skMap;
	map<uint16_t, list<NodeId>>::iterator skIt;
	//map<RIPD,   map<SK,       list<NID
	map<uint32_t, map<uint16_t, list<NodeId>>> pridMap;
	map<uint32_t, map<uint16_t, list<NodeId>>>::iterator pridIt;
	skMap.insert(pair<uint32_t, list<NodeId>>(sessionKey, nodeIds));
	pridMap.insert(pair<uint32_t, map<uint16_t, list<NodeId>>>(0,
			skMap));
	ostringstream nodeIdsOss;
	for (list<NodeId>::iterator it = nodeIds.begin(); it != nodeIds.end(); it++)
	{
		nodeIdsOss << it->uint() << " ";
	}
	_cmcGroupsMutex.lock();
	_cmcGroupsIt = _cmcGroups.find(rCid.uint());
	// rCID does not exist
	if (_cmcGroupsIt == _cmcGroups.end())
	{
		_cmcGroups.insert(pair<uint32_t, map<uint32_t, map<uint16_t,
				list<NodeId>>>>(rCid.uint(), pridMap));
		LOG4CXX_TRACE(logger, "CMC group created for rCID " << rCid.print()
				<< " > SK " << sessionKey << " with " << nodeIds.size()
				<< " NID(s): " << nodeIdsOss.str());
		_cmcGroupsMutex.unlock();
		return;
	}
	pridIt = _cmcGroupsIt->second.find(0);
	// RIPD does not exist
	if (pridIt == _cmcGroupsIt->second.end())
	{
		_cmcGroupsIt->second.insert(pair<uint32_t, map<uint16_t, list<NodeId>>>
				(0, skMap));
		LOG4CXX_DEBUG(logger, "CMC group created for existing rCID "
				<< rCid.print() << " > SK " << sessionKey << " with "
				<< nodeIds.size() << " NID(s): " << nodeIdsOss.str());
		_cmcGroupsMutex.unlock();
		return;
	}
	skIt = pridIt->second.find(sessionKey);
	// SK does not exist
	if (skIt == pridIt->second.end())
	{
		pridIt->second.insert(pair<uint16_t, list<NodeId>>(sessionKey,
				nodeIds));
		LOG4CXX_TRACE(logger, "CMC group create for existing rCID "
				<< rCid.print() << " but new SK " << sessionKey << " with "
				<< nodeIds.size() << " NID(s): " << nodeIdsOss.str());
		_cmcGroupsMutex.unlock();
		return;
	}
	if (skIt->second.empty())
	{
		LOG4CXX_DEBUG(logger, "CMC group for rCID " << rCid.print()
				<< " > SK " << sessionKey << " exists but is empty. "
				<< nodeIds.size() << " NIDs will be added: "
				<< nodeIdsOss.str());
		skIt->second = nodeIds;
	}
	else
	{
		LOG4CXX_TRACE(logger, "CMC for rCID " << rCid.print() << " > SK "
				<< sessionKey << " already exists");
	}
	_cmcGroupsMutex.unlock();
}

void Http::_deleterCids(NodeId &nodeId)
{
	_reverseNidTorCIdLookUpMutex.lock();
	_reverseNidTorCIdLookUpIt = _reverseNidTorCIdLookUp.find(nodeId.uint());
	if (_reverseNidTorCIdLookUpIt != _reverseNidTorCIdLookUp.end())
	{
		_reverseNidTorCIdLookUp.erase(_reverseNidTorCIdLookUpIt);
		LOG4CXX_TRACE(logger, "NID " << nodeId.uint() << " erased from reverse "
				"NID > rCID look up table");
	}
	_reverseNidTorCIdLookUpMutex.unlock();
}

bool Http::_forwardingEnabled(uint32_t nodeId)
{
	_nIdsMutex.lock();
	_nIdsIt = _nIds.find(nodeId);
	if (_nIdsIt == _nIds.end())
	{
		LOG4CXX_WARN(logger, "NID " << nodeId << " does not exist in known NIDs"
				" table");
		_nIdsMutex.unlock();
		return false;
	}
	if (_nIdsIt->second)
	{
		_nIdsMutex.unlock();
		return true;
	}
	_nIdsMutex.unlock();
	return false;
}

bool Http::_getCmcGroup(IcnId &rCid, uint16_t &sessionKey, bool firstPacket,
		list<NodeId> &nodeIds)
{
	list<NodeId>::iterator nodeIdsIt;
	bool bufferPacket = false;
	// Check if an already created CMC group is awaiting more data
	_cmcGroupsMutex.lock();
	_cmcGroupsIt = _cmcGroups.find(rCid.uint());
	// rCID found
	if (_cmcGroupsIt != _cmcGroups.end())
	{
		//map<RIPD,   map<SK,       list<NIDs
		map<uint32_t, map<uint16_t, list<NodeId>>>::iterator cmcPridIt;
		cmcPridIt = _cmcGroupsIt->second.find(0);
		// RIPD found
		if (cmcPridIt != _cmcGroupsIt->second.end())
		{
			//map<SK,     list<NIDs
			map<uint16_t, list<NodeId>>::iterator cmcSkIt;
			cmcSkIt = cmcPridIt->second.find(sessionKey);
			// Session key found
			if (cmcSkIt != cmcPridIt->second.end())
			{
				//list of NIDs not empty
				if (!cmcSkIt->second.empty())
				{
					nodeIds = cmcSkIt->second;
					LOG4CXX_TRACE(logger, "CMC group of " << nodeIds.size()
							<< " NID(s) found for rCID " << rCid.print()
							<< " > SK " << sessionKey);
					_cmcGroupsMutex.unlock();
					return false;
				}
				else
				{
					LOG4CXX_TRACE(logger, "CMC group is empty for rCID "
							<< rCid.print() << " > SK "	<< sessionKey
							<< ". START_PUBLISH_iSUB is probably missing for "
									"all of the CMC's members");
				}
			}
			else
			{
				LOG4CXX_TRACE(logger, "No existing CMC group known for rCID "
						<< rCid.print() << " > SK " << sessionKey);
			}
		}
		else
		{
			LOG4CXX_TRACE(logger, "No existing CMC group known for rCID "
					<< rCid.print());
		}
	}
	else
	{
		LOG4CXX_TRACE(logger, "No existing CMC group known for rCID "
				<< rCid.print() << ". Checking potential CMC groups");

	}
	_cmcGroupsMutex.unlock();
	if (!firstPacket)
	{
		LOG4CXX_TRACE(logger, "This is not the first time a packet needs to be "
				"sent to cNAPs. No existing CMC group could be found though "
				"for rCID " << rCid.print() << " > SK " << sessionKey
				<< " (# of CMC groups = " << _cmcGroups.size() << ")");
		return false;
	}
	/* If CMC group did not have any entry for rCID > RIPD look up potential CMC
	 * groups map. Note, it is important to not add NIDs from the potential CMC
	 * groups map into an existing CMC group. So if this was the first packet
	 * from the server and no existing CMC group could be found, look up the
	 * potential list of NIDs and create a new one.
	 */
	//map<NID,    ready to receive HTTP response
	map<uint32_t, bool>::iterator pCmcNidIt;
	//map<RIPD,   map<NID,      ready to receive HTTP response
	map<uint32_t, map<uint32_t, bool>>::iterator pCmcPridIt;
	// Check potential CMC groups map in case CMC group did not have an NID
	if (nodeIds.empty())
	{
		_potentialCmcGroupsMutex.lock();
		_potentialCmcGroupsIt = _potentialCmcGroups.find(rCid.uint());
		//rCID does not exist
		if (_potentialCmcGroupsIt == _potentialCmcGroups.end())
		{
			LOG4CXX_TRACE(logger, "rCID " << rCid.print() << " could not be "
					"found in potential CMC group map");
			_potentialCmcGroupsMutex.unlock();
			return false;
		}
		pCmcPridIt = _potentialCmcGroupsIt->second.find(0);
		if (pCmcPridIt == _potentialCmcGroupsIt->second.end())
		{
			_potentialCmcGroupsMutex.unlock();
			return false;
		}
		// iterate over NIDs
		for (pCmcNidIt = pCmcPridIt->second.begin();
				pCmcNidIt != pCmcPridIt->second.end(); pCmcNidIt++)
		{
			//NID has sent off the entire request and is awaiting the HTTP
			//response
			if (pCmcNidIt->second)
			{
				// FID is still missing
				if (!_forwardingEnabled(pCmcNidIt->first))
				{
					LOG4CXX_TRACE(logger, "START_PUBLISH_iSUB has not been "
							"received for NID " << pCmcNidIt->first);
					bufferPacket = true;
				}
				else
				{
					NodeId nodeId(pCmcNidIt->first);
					nodeIds.push_back(nodeId);
					LOG4CXX_TRACE(logger, "NID " << nodeId.uint() << " moved to"
							" CMC group of size " << nodeIds.size() << " for "
							"rCID " << rCid.print() << " > RIPD " << " > SK "
							<< sessionKey);
				}
			}
			else
			{
				LOG4CXX_TRACE(logger, "NID " << pCmcNidIt->first << " hasn't "
						"completed delivery of entire HTTP request");
			}
		}
		// Add all selected NIDs to a new CMC group
		if (!nodeIds.empty())
		{
			_createCmcGroup(rCid, sessionKey, nodeIds);
		}
		else
		{
			LOG4CXX_TRACE(logger, "No NID was ready to be moved to CMC group");
			_potentialCmcGroupsMutex.unlock();
			return true;//bufferPacket = true
		}
		// Delete the NIDs which have been added to the CMC group
		for (nodeIdsIt = nodeIds.begin(); nodeIdsIt != nodeIds.end();
				nodeIdsIt++)
		{
			pCmcNidIt = pCmcPridIt->second.find(nodeIdsIt->uint());
			pCmcPridIt->second.erase(pCmcNidIt);
			LOG4CXX_TRACE(logger, "NID " << nodeIdsIt->uint()
					<< " removed from potential CMC group. FYI "
					<< pCmcPridIt->second.size() << " remaining NIDs in this "
							"list");
		}
		// If list of NIDs is empty, remove RIPD
		if (pCmcPridIt->second.empty())
		{
			_potentialCmcGroupsIt->second.erase(pCmcPridIt);
			LOG4CXX_TRACE(logger, "List of NIDs is empty in potential CMC group"
					" for rCID " << rCid.print());
		}
		// If this was the last RIPD for this rCID, delete rCID entry too
		if (_potentialCmcGroupsIt->second.empty())
		{
			_potentialCmcGroups.erase(_potentialCmcGroupsIt);
			LOG4CXX_TRACE(logger, "List of 0s is empty in potential CMC "
					"group for rCID " << rCid.print()
					<< ". rCID entry deleted");
		}
		_potentialCmcGroupsMutex.unlock();
	}
	return bufferPacket;
}
list<uint32_t> Http::_getrCids(NodeId &nodeId)
{
	list<uint32_t> rCids;
	_reverseNidTorCIdLookUpMutex.lock();
	_reverseNidTorCIdLookUpIt = _reverseNidTorCIdLookUp.find(nodeId.uint());
	if (_reverseNidTorCIdLookUpIt == _reverseNidTorCIdLookUp.end())
	{
		LOG4CXX_WARN(logger, "NID " << nodeId.uint() << " does not exist in "
				"reverse NID to rCID look up map");
		_reverseNidTorCIdLookUpMutex.unlock();
		return rCids;
	}
	rCids = _reverseNidTorCIdLookUpIt->second;
	_reverseNidTorCIdLookUpMutex.unlock();
	return rCids;
}

uint8_t Http::_numberOfSessionKeys(IcnId &rCId)
{
	uint8_t numberOfSessionKeys = 0;
	_ipEndpointSessionsMutex.lock();
	_ipEndpointSessionsIt = _ipEndpointSessions.find(rCId.uint());
	// rCID found
	if (_ipEndpointSessionsIt != _ipEndpointSessions.end())
	{
		map<uint32_t, list<uint16_t>>::iterator proxyRuleIdMapIt;
		proxyRuleIdMapIt = _ipEndpointSessionsIt->second.find(0);
		if (proxyRuleIdMapIt != _ipEndpointSessionsIt->second.end())
		{
			numberOfSessionKeys = proxyRuleIdMapIt->second.size();
		}
	}
	else
	{
		LOG4CXX_WARN(logger, "rCID " << rCId.print() << " could not be found in"
				" map of active sessions");
	}
	_ipEndpointSessionsMutex.unlock();
	return numberOfSessionKeys;
}
