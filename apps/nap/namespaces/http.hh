/*
 * http.hh
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

#ifndef NAP_NAMESPACES_HTTP_HH_
#define NAP_NAMESPACES_HTTP_HH_

#include <blackadder.hpp>
#include <boost/thread/thread.hpp>
#include <boost/date_time.hpp>
#include <log4cxx/logger.h>
#include <list>
#include <map>

#include <configuration.hh>
#include <monitoring/statistics.hh>
#include <namespaces/httptypedef.hh>
#include <namespaces/buffercleaners/httpbuffercleaner.hh>
#include <transport/transport.hh>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

using namespace configuration;
using namespace log4cxx;
using namespace monitoring::statistics;
using namespace std;

namespace namespaces
{

namespace http
{
/*!
 * \brief Implementation of the HTTP handler
 */
class Http
{
	static LoggerPtr logger;
public:
	/*!
	 * \brief Constructor
	 *
	 * \param icnCore Pointer to Blackadder API
	 * \param configuration Reference to configuration class
	 * \param transport Reference to transport class
	 */
	Http(Blackadder *icnCore, Configuration &configuration,
			Transport &transport, Statistics &statistics);
	/*!
	 * \brief Destructor
	 */
	~Http();
	/*!
	 * \brief Add NID and rCID to reverse look up map _reversNidTorCIdLookUp
	 *
	 * \param nodeId The NID which should be added
	 * \param rCId The rCID which should be added to the list of known rCIDs for
	 * this NID
	 */
	void addReversNidTorCIdLookUp(string &nodeId, IcnId &rCId);
	/*!
	 * \brief Closing a CMC group using rCID and 0 information
	 *
	 * Not only does this method close a CMC group properly it also signals this
	 * action to all cNAPs which allows them to inform their HTTP proxy to
	 * shutdown the TCP socket.
	 *
	 * If a TCP client socket has been shutdown this method must be called in
	 * order to remove this session key (socket FD) from the list of active
	 * IP endpoint sessions.
	 *
	 * \param rCid The rCID which identifies a particular CMC group
	 * \param sessionKey The session key which identified a particular CMC group
	 */
	void closeCmcGroup(IcnId &rCid, uint16_t &sessionKey);
	/*!
	 * \brief Delete the session key entry for possible future communications
	 * with TCP server instances
	 *
	 * If a TCP server socket has been shutdown this method must be called in
	 * order to remove this session key (socket FD) from the list of active
	 * IP endpoint sessions.
	 *
	 * \param sessionKey The session key which should be deleted
	 */
	void deleteSessionKey(uint16_t &sessionKey);
	/*!
	 * \brief Handle HTTP requests
	 *
	 * \param fqdn The FQDN of the request
	 * \param resource The web resource of this request
	 * \param httpMethod HTTP method
	 * \param packet Pointer to the packet
	 * \param packetSize Length of the packet
	 * \param sessionKey Unique key for this session (socket FD)
	 */
	void handleRequest(string &fqdn, string &resource,
			http_methods_t httpMethod, uint8_t *packet, uint16_t &packetSize,
			uint16_t &sessionKey);
	/*!
	 * \brief Handle HTTP responses
	 *
	 * This method blocks if CMC group can not be formed due to whatever reason.
	 * If the CMC group formation fails after 7 attempts (7 * 200ms) this method
	 * returns false;
	 *
	 * \param rCId The rCID to which the HTTP response belongs to
	 * \param sessionKey The session key which identifies this particular
	 * session among multiple rCID > 0 tuples
	 * \param firstPacket Boolean indicating if this was the first packet
	 * received from the server
	 * \param packet Pointer to the packet
	 * \param packetSize Length of the packet
	 */
	bool handleResponse(IcnId &rCId, uint16_t &sessionKey, bool &firstPacket,
			uint8_t *packet, uint16_t &packetSize);
protected:
	/*!
	 * \brief End a session by shutting down all sockets towards IP endpoints
	 *
	 * \param rCid The rCID used to look up the CID for which all sockets shall
	 * be closed
	 * \param sessionKey The SK indentifying the LTP session
	 */
	void endSession(IcnId &rCid, uint16_t &sessionKey);
	/*!
	 * \brief Set the forwarding state for a particular CID
	 *
	 * \param cId Reference to the CID for which the forwarding state
	 * should be set
	 * \param state The forwarding state
	 */
	void forwarding(IcnId &cId, bool state);
	/*!
	 * \brief Set the forwarding state for a particular NID
	 *
	 * \param nodeId Reference to the NID for which the forwarding state should
	 * be set (state kept in LTP, not in HTTP handler)
	 * \param state The forwarding state
	 */
	void forwarding(NodeId &nodeId, bool state);
	/*!
	 * \brief Act upon a received DNSlocal message
	 *
	 * \param fqdn The hashed FQDN received in the DNSlocal message
	 */
	void handleDnsLocal(uint32_t &fqdn);
	/*!
	 * \brief Initialise HTTP handler
	 */
	void initialise();
	/*!
	 * \brief Publish a buffered HTTP request packet
	 *
	 * If the NAP received a START_PUBLISH event from the ICN core this
	 * method checks if any packet has been buffered for the CID
	 *
	 * \param icnId The content identifier for which the IP buffer should
	 * be checked
	 *
	 * \return void
	 */
	void publishFromBuffer(IcnId &cId);
	/*!
	 * \brief Published buffered HTTP response via CMC
	 *
	 * If the NAP received a START_PBULISH_iSUB event from the ICN core this
	 * method checks if any packet has been buffered for the given NID by using
	 * the reverse NID to rCID look up table
	 *
	 * \param nodeId The NID for which the local ICN core has received a FID
	 */
	void publishFromBuffer(NodeId &nodeId);
	/*!
	 * \brief Send HTTP response to IP endpoint
	 *
	 * When receiving the HTTP request the HTTP handler kept a mapping of the
	 * session key (socket file descriptor) to rCid in order to be able
	 * to look up the TCP endpoint which is awaiting this HTTP response
	 *
	 * \param rCid The rCid which is used to retrieve the correct socket FD from
	 * _ipEndpointSessions
	 * \param packet Pointer to the HTTP response packet
	 * \param packetSize Size of the HTTP response packet
	 */
	void sendToEndpoint(IcnId &rCid, uint8_t *packet,
			uint16_t &packetSize);
	/*!
	 * \brief Subscribe to a particular FQDN
	 *
	 * \param cid The CID for which the HTTP handler shall subscribe to
	 */
	void subscribeToFqdn(IcnId &cid);
	/*!
	 * \brief Uninitialise HTTP handler
	 */
	void uninitialise();
	/*!
	 * \brief Unsubscribe from a particular FQDN
	 *
	 * \param cid The CID supposed to be used to subscribe from
	 */
	void unsubscribeFromFqdn(IcnId &cid);
private:
	Blackadder *_icnCore; /*!< Pointer to Blackadder instance */
	Configuration &_configuration; /*!< Reference to configuration */
	Transport &_transport;/*!< Reference to transport class */
	Statistics &_statistics; /*<! Refernce to statistics class*/
	map<uint32_t, IcnId> _cIds;
	map<uint32_t, IcnId>::iterator _cIdsIt;
	map<uint32_t, IcnId> _rCIds;
	map<uint32_t, IcnId>::iterator _rCIdsIt;
	map<uint32_t, bool> _nIds; /*!< map<NID, startPublishReceived> This
	boolean is used to indicate that the FID for this particular NID has been
	received.*/
	map<uint32_t, bool>::iterator _nIdsIt; /*!< Iterator for _nIds map*/
	boost::mutex _nIdsMutex; /*!< Mutex for _nIds */
	boost::mutex _mutexIcnIds;
	map<uint32_t, map<uint32_t, list<uint16_t>>> _ipEndpointSessions;/*!<
	map<rCID, map<0, list<sessionKey>>> where the session key is the socket
	FD of TCP server instances*/
	map<uint32_t, map<uint32_t, list<uint16_t>>>::iterator
	_ipEndpointSessionsIt;/*!< Iterator for _ipEndpointSessions map*/
	boost::mutex _ipEndpointSessionsMutex;/*!< mutex for _ipEndpointSessions map
	*/
	packet_buffer_requests_t _packetBufferRequests; /*! <Buffer for HTTP
	messages to be published  */
	packet_buffer_requests_t::iterator _packetBufferRequestsIt;/*!< Iterator for
	_packetBufferRequests map */
	boost::mutex _packetBufferRequestsMutex;/*!< mutex for _packetBufferRequests
	map */
	packet_buffer_responses_t _packetBufferResponses;/*! Buffer for HTTP
	responses to be published*/
	/*!<Buffer for HTTP messages to be published map<cId, map<rCId, PACKET>> */
	packet_buffer_responses_t::iterator _packetBufferResponsesIt;/*!< Iterator
	for _packetBuffer map */
	boost::mutex _packetBufferResponsesMutex;
	potential_cmc_groups_t _potentialCmcGroups;/*!< List of NIDs for which an
	HTTP request has been received but not necessarily the entire message. The
	boolean indicates whether or not the entire HTTP request has been received
	so that the HTTP response can be sent */
	potential_cmc_groups_t::iterator _potentialCmcGroupsIt; /*!< Iterator for
	_potentialCmcGroups map*/
	boost::mutex _potentialCmcGroupsMutex; /*!< Mutex for
	_potentialMulticastGroup map */
	cmc_groups_t _cmcGroups;/*!< map to hold all locked CMC groups */
	cmc_groups_t::iterator _cmcGroupsIt; /*!< Iterator for _cmcGroups */
	boost::mutex _cmcGroupsMutex; /*!< Mutex for _cmcGroups map operation */
	map<uint32_t, list<uint32_t>> _reverseNidTorCIdLookUp;/*!< map<NID,
	list<rCIDs> Used to store the list of rCIDs a particular NID is awaiting a
	response for. When START_PUBLISH_iSUB arrives it only has NID */
	map<uint32_t, list<uint32_t>>::iterator _reverseNidTorCIdLookUpIt;
	boost::mutex _reverseNidTorCIdLookUpMutex;/*!< mutex for
	_reversNidTorCIdLookUp map access */
	boost::thread *_httpBufferThread;
	/*!
	 * \brief Add an IP endpoint session to the list of known sessions
	 *
	 * This method adds the session key to the _ipEndpointSessions map (if it
	 * does not exist yet) using rCID > list<session keys> relation.
	 *
	 * \param rCId The rCID to which this session key belongs
	 * \param sessionKey The session key which should be added (socket FD)
	 */
	void _addIpEndpointSession(IcnId &rCId, uint16_t &sessionKey);
	/*!
	 * \brief Buffer an HTTP request packet
	 */
	void _bufferRequest(IcnId &cId, IcnId &rCId, uint16_t &sessionKey,
			http_methods_t httpMethod, uint8_t *packet, uint16_t &packetSize);
	/*!
	 * \brief Buffer an HTTP request packet
	 *
	 * \param rCId The rCID for this packet
	 * \param sessionKey The session key for this packet (socket FD)
	 * \param packet Pointer to HTTP response
	 * \param packetSize The length of the HTTP response
	 */
	void _bufferResponse(IcnId &rCId, uint16_t &sessionKey, uint8_t *packet,
			uint16_t &packetSize);
	/*!
	 * \brief Create a new CMC group
	 *
	 * Gets only called when a new CMC group is created after discovering NIDs
	 * from the potential CMC group map. This method works on the _cmcGroups map
	 *
	 * \param rCid rCID for which the CMC group should be created
	 * \param sessionKey The session key for which the CMC should be created
	 * \param nodeIds List of NIDs which should be added to this group
	 */
	void _createCmcGroup(IcnId &rCid, uint16_t &sessionKey,
			list<NodeId> &nodeIds);
	/*!
	 * \brief Delete the list of rCIDs for the given NID awaits
	 *
	 * \param nodeId The NID for which the reverse look up map should be deleted
	 */
	void _deleterCids(NodeId &nodeId);
	/*!
	 * \brief Obtain the state whether or not the NAP has received a
	 * START_PUBLISH_iSUB event for a particular NID
	 *
	 * \param nodeId The NID for which the forwarding state should be checked
	 *
	 * \return Boolean indicating whether or not the forwarding state for the
	 * given NID is enabled
	 */
	bool _forwardingEnabled(uint32_t nodeId);
	/*!
	 * \brief Obtain the list of rCIDs the given NID awaits
	 *
	 * \param nodeId The NID for which the reverse look up map should be checked
	 *
	 * \return A list of rCIDs
	 */
	list<uint32_t> _getrCids(NodeId &nodeId);
	/*!
	 * \brief Obtain the list of NIDs in a locked CMC group
	 *
	 * This method first checks if a CMC group exists for the given rCID and
	 * 0 combination. If not, it will walk through the potential list of CMC
	 * group members and adds any NID for which a START_PUBLISH_iSUB has been
	 * received to a new CMC group.
	 *
	 * Note, if the group is created from Http::publishFromBuffer there is no
	 * session key available at the time and the first one found for rCID > 0
	 * is being used while the other entries in the ICN buffer are simply
	 * deleted. Also, in order to accommodate for a scenario where the
	 * START_PUBLISH_iSUB event is received while the HTTP response message
	 * fragments are being received from the TCP proxy, the sessionKey variable
	 * is being set to the one found first in the ICN buffer.
	 *
	 * \param rCid The rCID for which the list of NIDs should be obtained
	 * \param sessionKey The session key for which the list of NIDs should be
	 * obtained
	 * \param firstPacket Boolean indicating whether or not this was the first
	 * packet received from the server
	 * \param nodeIds List of NIDs which are awaiting HTTP response
	 *
	 * \return Boolean indicating if the HTTP response must be buffered
	 */
	bool _getCmcGroup(IcnId &rCid, uint16_t &sessionKey, bool firstPacket,
			list<NodeId> &nodeIds);
	/*!
	 * \brief Obtain the number of session keys stored for a particular rCID
	 *
	 * \param rCID The rCID for which the number of session keys should be
	 * checked
	 *
	 * \return The number of session keys stored
	 */
	uint8_t _numberOfSessionKeys(IcnId &rCId);
};

} /* namespace http */

} /* namespace namespaces */

#endif /* NAP_NAMESPACES_HTTP_HH_ */
