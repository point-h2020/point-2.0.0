/*
 * lightweight.hh
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

#ifndef NAP_TRANSPORT_LIGHTWEIGHT_HH_
#define NAP_TRANSPORT_LIGHTWEIGHT_HH_

#include <blackadder.hpp>
#include <boost/date_time.hpp>
#include <boost/thread.hpp>
#include <log4cxx/logger.h>
#include <forward_list>

#include <configuration.hh>
#include <enumerations.hh>
#include <monitoring/statistics.hh>
#include <trafficcontrol/trafficcontrol.hh>
#include <transport/lightweighttimeout.hh>
#include <transport/lightweighttypedef.hh>
#include <types/icnid.hh>
#include <types/nodeid.hh>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

#define MAX_PAYLOAD_LENGTH 65535

using namespace configuration;
using namespace log4cxx;
using namespace monitoring::statistics;
using namespace trafficcontrol;

namespace transport
{

namespace lightweight
{
/*!
 * \brief Implementation of the lightweight transport protocol for
 * HTTP-over-ICN pub/subs
 */
class Lightweight: public TrafficControl
{
	static LoggerPtr logger;
public:
	/*!
	 * \brief Constructor
	 */
	Lightweight(Blackadder *icnCore, Configuration &configuration,
			boost::mutex &icnCoreMutex, Statistics &statistics);
	/*!
	 * \brief Destructor
	 */
	virtual ~Lightweight();
	/*!
	 * \brief Initialise LTP with pointers to maps residing in the HTTP
	 * handler (for separation of concern reasons).
	 *
	 * LTP requires knowledge about the forwarding policy of particular
	 * NIDs indicated by the ICN core through START_PUBLISH_iSUB which
	 * is handled by HTTP handler. However, in order to send CTRL data
	 * LTP requires knowledge whether or not the START_PUBLISH_iSUB has
	 * been received for a particular NID.
	 * The pointer to the CMC group map is passed to LTP, as only LTP
	 * understands the structure of its protocol and fills up the map of
	 * potential CMC groups on behalf of the HTTP handler.
	 *
	 * \param potentialCmcGroup Pointer to potential CMC group map
	 * \param potentialCmcGroupMutex Pointer to mutex for transaction-safe
	 * operations on potentialCmcGroup map
	 * \param knownNIds Pointer to knownNIds map
	 * \param knownNIdsMutex Pointer to mutex for transaction-safe operations on
	 * knownNIds map
	 */
	void initialise(void *potentialCmcGroup, void *potentialCmcGroupMutex,
			void *knownNIds, void *knownNIdsMutex, void *cmcGroup,
			void *cmcGroupMutex);
	/*!
	 * \brief Publish HTTP responses (co-incidental multicast)
	 *
	 * \param cId Reference to the content identifier
	 * \param sessionKey The session key under which this packet should be
	 * published
	 * \param packet Pointer to the data
	 * \param dataSize Pointer to the length of the packet
	 */
	void publish(IcnId &rCid, uint16_t &sessionKey, list<NodeId> &nodeIds,
			uint8_t *data, uint16_t &dataSize);
	/*!
	 * \brief Publish HTTP responses (co-incidental multicast)
	 *
	 * Blocking method. This method is called when packet from TCP server can be
	 * immediately published to sNAP (
	 *
	 * \param cId Reference to the content identifier
	 * \param iSubIcnId Reference to the iSub content identifier
	 * \param packet Pointer to the data
	 * \param dataSize Pointer to the length of the packet
	 *
	 * \return Boolean indicating if packet has been successfully
	 * published
	 */
	void publish(IcnId &cId, IcnId &iSubIcnId, uint16_t &sessionKey,
			uint8_t *data, uint16_t &dataSize);
	/*!
	 * \brief Publish HTTP session ended (co-incidental multicast)
	 *
	 * \param cId Reference to the content identifier
	 * \param sessionKey The session key under which this packet should be
	 * published
	 */
	void publishEndOfSession(IcnId &rCid, uint16_t &sessionKey);
	/*!
	 * \brief Publish HTTP requests
	 *
	 * Non-blocking method. It calls a thread to check for successful delivery
	 *
	 * \param cId Reference to the content identifier
	 * \param rCId Reference to the iSub content identifier (rCID)
	 * \param packet Pointer to the data
	 * \param dataSize Pointer to the length of the packet
	 */
	void publishFromBuffer(IcnId &cId, IcnId &rCId, uint16_t &sessionKey,
			uint8_t *data, uint16_t &dataSize);
protected:
	/*!
	 * \brief Handle an incoming ICN message (iSub) which is using LTP
	 *
	 * This implementation of the NAP uses LTP for the HTTP handler
	 * only.
	 *
	 * \param cId The CID under which the packet has been published
	 * \param packet Pointer to the ICN payload
	 */
	TpState handle(IcnId &cId, uint8_t *packet, uint16_t &sessionKey);
	/*!
	 * \brief Handle an incoming ICN message (iSub) which is using LTP
	 *
	 * This implementation of the NAP uses LTP for the HTTP handler
	 * only.
	 *
	 * \param cId The CID under which the packet has been published
	 * \param rCId The rCID under which the response must be
	 * published
	 * \param nodeIdStr The NID which must be used when publishing the
	 * response
	 * \param packet Pointer to the ICN payload
	 * \param sessionKey If WE has been received and all fragments were
	 * received this variable holds the corresponding session key
	 *
	 * \return The TP state indicating if packet can be sent to IP endpoint
	 */
	TpState handle(IcnId &cId, IcnId &rCId, string &nodeIdStr, uint8_t *packet,
			uint16_t &sessionKey);
	/*!
	 * \brief Retrieve a packet from _icnPacketBuffer map
	 *
	 * In order to call this method handle() must have returned
	 * TP_STATE_ALL_FRAGMENTS_RECEIVED. Note that when calling this method the
	 * packet pointer must point to already allocated memory.
	 *
	 * \param rCId The rCID for which the packet should be retrieved from the
	 * buffer
	 * \param nodeIdStr The NID for which the packet should be retrieved from
	 * the buffer
	 * \param sessionKey The SK for which the packet should be retrieved from
	 * the buffer
	 * \param packet Pointer to the memory to which the packet will be copied
	 * \param packetSize The number of octets representing the packet size
	 *
	 * \return boolean indicating if the packet has been successfully retrieved
	 * from ICN buffer
	 */
	bool retrievePacket(IcnId &rCId, string &nodeIdStr, uint16_t &sessionKey,
			uint8_t *packet, uint16_t &packetSize);
private:
	Blackadder *_icnCore;/*!< Pointer to the Blackadder instance */
	Configuration &_configuration;
	boost::mutex &_icnCoreMutex;/*!< Reference to ICN core mutex */
	Statistics &_statistics;/*!< Reference to the Statistics class*/
	map<uint32_t, IcnId> _cIdReverseLookUp;/*!< map<rCID, cId> When	LTP CTRL
	arrives from sNAP (underrCID) the CID (FQDN) is required for potential
	retransmissions*/
	map<uint32_t, IcnId>::iterator _cIdReverseLookUpIt;/*!< Iterator for
	_cIdReverseLookUp*/
	boost::mutex _cIdReverseLookUpMutex; /*!< Mutex for _cIdReverseLookUp map
	operations */
	icn_packet_buffer_t _icnPacketBuffer; /*!< Packet buffer for incoming ICN
	packets */
	icn_packet_buffer_t::iterator _icnPacketBufferIt; /*!< Iterator
	for _icnPacketBuffer map*/
	boost::mutex _icnPacketBufferMutex; /*!< Mutex for _icnPacketBuffer map*/
	proxy_packet_buffer_t _proxyPacketBuffer; /*!< Packet buffer for incoming IP
	packets from the HTTP proxy */
	proxy_packet_buffer_t::iterator _proxyPacketBufferIt;/*!< Iterator for
	packetBuffer map */
	boost::mutex _proxyPacketBufferMutex;/*!< mutex for _packetBuffer map*/
	map<uint32_t, map<uint32_t, map<uint16_t, map<uint16_t, pair<uint8_t *,
	uint16_t>>>>> _ltpPacketBuffer;/*!< map<rCID, map<0, map<SK,
	map<Sequence, Packet>>>> Packet buffer for sent datagrams over LTP (in case
	NACK comes back)*/
	boost::mutex _ltpPacketBufferMutex;/*!< mutex for transaction-safe
	operations on _ltpPacketBuffer */
	/*
	 * (Potential) CMC Group
	 */
	cmc_groups_t *_cmcGroups;/*!< Pointer to Http::_cmcGroups map */
	cmc_groups_t::iterator _cmcGroupsIt; /*!< Iterator for _cmcGroups */
	boost::mutex *_cmcGroupsMutex; /*!< Pointer to Http::_cmcGroupsMutex mutex*/
	potential_cmc_groups_t * _potentialCmcGroups; /*!< Pointer to
	Http::_potentialCmcGroups */
	potential_cmc_groups_t::iterator _potentialCmcGroupsIt; /*!< Iterator for
	_potentialCmcGroups map*/
	boost::mutex *_potentialCmcGroupsMutex; /*!< Pointer to
	Http::_potentialCmcGroups mutex*/
	map<uint32_t, bool> *_knownNIds; /*!< map<NID, startPublishReceived> This
	boolean is used to indicate that the FID for this particular NID has been
	received.*/
	map<uint32_t, bool>::iterator _knownNIdsIt; /*!<Iterator for _fidKnown map*/
	boost::mutex *_knownNIdsMutex; /*!< Mutex for _knownNIds */
	forward_list<uint16_t> _rtts;/*!< Round trip time list*/
	boost::mutex _rttsMutex;/*!< Mutex for writing _rtt*/
	uint16_t _rttMultiplier;/*!< Multiplier for LTP-CTRL timeout using RTT */
	unordered_map<uint32_t, unordered_map<uint32_t, unordered_map<uint32_t,
	unordered_map<uint16_t, bool>>>> _sessionEndedResponses;
	/*!< map<rCID, map<0, map<NID, map<SK, SED received>>>>*/
	unordered_map<uint32_t, unordered_map<uint32_t, unordered_map<uint32_t,
		unordered_map<uint16_t, bool>>>>::iterator _sessionEndedResponsesIt;
	/*!< Iterator for _sessionEndedResponses map*/
	boost::mutex _sessionEndedResponsesMutex;/*!< Mutex for operations on
	_sessionEndedResponses map*/
	map<uint32_t, map<uint32_t, map<uint16_t, bool>>> _windowEndedRequests;
	/*!<map<rCID, map<0, map<Session Key, WED received>>> */
	map<uint32_t, map<uint32_t, map<uint16_t, bool>>>::iterator
	_windowEndedRequestsIt;/*!<Iterator for _windowEndedRequests map*/
	boost::mutex _windowEndedRequestsMutex;/*!< Mutex for _windowEndedRequests
	maps */
	map<uint32_t, map<uint32_t, map<uint32_t, map<uint16_t, bool>>>>
	_windowEndedResponses; /*!<map<rCID, map<0, map<NID, map<Session Key,
	WED received>>> */
	map<uint32_t, map<uint32_t, map<uint32_t, map<uint16_t, bool>>>>::iterator
	_windowEndedResponsesIt;/*!<Iterator for _windowEndedResponses map*/
	boost::mutex _windowEndedResponsesMutex;/*!< Mutex for _windowEndedResponses
	maps */
	map<uint32_t, map<uint32_t, map<uint16_t, map<uint32_t, bool>>>>
	_windowUpdate; /*!< map<rCID, map<0, map<SK, map<NID, WUD received*/
	map<uint32_t, map<uint32_t, map<uint16_t, map<uint32_t, bool>>>>::iterator
	_windowUpdateIt; /*!< Iterator for operations on _windowUpdate map*/
	boost::mutex _windowUpdateMutex; /*!< Mutex for transaction-safe operations
	on _windowUpdate map */
	map<uint32_t, map<uint32_t, map<uint16_t, nack_group_t>>> _nackGroups;/*!<
	map<rCID, map<0, map<SK, nackGroup>>> Store the NIDs which sent a NACK so
	that if all NIDs have responded to the WE message (either WED or NACK) the
	NAP can eventually re-submit the range of missing segments */
	map<uint32_t, map<uint32_t, map<uint16_t, nack_group_t>>>::iterator
	_nackGroupsIt; /*!< Iterator for _nackGroups mak */
	boost::mutex _nackGroupsMutex;/*!< mutex for _nackGroups map */
	boost::thread_group _timeoutThreads;
	/*!
	 * \brief Add a NACK to _nackGroups
	 *
	 * \param rCid The rCID under which the NACK message has been received
	 * \param ltpHeaderNack The LTP NACK header received
	 * \param nodeId The NID from which the NACK message has been received
	 */
	void _addNackNodeId(IcnId &rCid, ltp_hdr_ctrl_nack_t &ltpHeaderNack,
			NodeId &nodeId);
	/*!
	 * \brief Add NID to list of known NIDs and their forwarding states
	 *
	 * This method checks the _fidKnown map if the given NID exists as a key.
	 * The local ICN core preserves a list of NIDs and their FID to allow the
	 * realisation of implicit subscriptions (iSub).
	 *
	 * \param nodeId The NID to be added
	 */
	void _addNodeId(NodeId &nodeId);
	/*!
	 * \brief Add a NID to the _windowUpdate map
	 *
	 * This methods adds the given NID to the _windowUpdate map and sets its
	 * state to false.
	 *
	 * \param rCid The rCID under which the NID should be added
	 * \param sessionKey The SK under which the NID should be added
	 * \param nodeId The NID to be added
	 */
	void _addNodeIdToWindowUpdate(IcnId &rCid, uint16_t &sessionKey,
			NodeId &nodeId);
	/*!
	 * \brief Add rCID > CID look-up
	 *
	 * When sNAP replies only the rCID is inluded. Hence, the NAP requires to
	 * keep a reverse look up mapping
	 *
	 * \param cid The CID of the request
	 * \param rCid The rCID of hte request
	 */
	void _addReverseLookUp(IcnId &cid, IcnId &rCid);
	/*!
	 * \brief Add rCID >   > NID > SK > NIDs to _sessionEndedResponses map
	 *
	 * \param rCid The rCID used as the primary key
	 * \param ltpHdrCtrlSe The LTP SE header comprising   and SK
	 * \param nodeIds The list of NIDs from which a SED is expected
	 */
	void _addSessionEnd(IcnId &rCid, ltp_hdr_ctrl_se_t &ltpHdrCtrlSe,
			list<NodeId> nodeIds);
	/*!
	 * \brief Add sent LTP CTRL-WE message to _windowEndedRequests map
	 *
	 * \param rCid The rCID which should be added
	 * \param ltpHeaderCtrlWe The LTP CTRL-WE header holding the required
	 * and session key values
	 */
	void _addWindowEnd(IcnId &rCid, ltp_hdr_ctrl_we_t &ltpHeaderCtrlWe);
	/*!
	 * \brief Add sent LTP CTRL-WE message to _windowEndedResponses map
	 *
	 * \param rCid The rCID which should be added
	 * \param nodeIds List of NIDs this WE has been published to
	 * \param ltpHeaderCtrlWe The LTP CTRL-WE header holding the required
	 * and session key values
	 */
	void _addWindowEnd(IcnId &rCid, list<NodeId> &nodeIds,
			ltp_hdr_ctrl_we_t &ltpHeaderCtrlWe);
	/*!
	 * \brief Add packet received from ICN core to LTP buffer
	 *
	 * This method adds the actual packet to the LTP buffer always using rCID as
	 * the map's key for both HTTP requests and responses
	 *
	 * Note (to myself probably): do NOT delete packetDescriptor.first memory
	 * after inserting it into the map. This will delete the packet in the map
	 * too. Funny enough, when erasing a map entry the map.erase() method
	 * deletes the memory though.
	 *
	 * \param rCId iSub CID (URL)
	 * \param nodeId Node ID of the publisher
	 * \param ltpHeader LTP header for data
	 * \param packet Pointer to packet
	 * \param packetSize Size of the allocated packet memory
	 */
	void _bufferIcnPacket(IcnId &rCId, NodeId &nodeId,
			ltp_hdr_data_t &ltpHeader, uint8_t *packet);
	/*!
	 * \brief Buffer a sent LTP packet for NACK scenarios
	 *
	 * \param rCid The rCid for which the packet should be buffered
	 * \param ltpHeaderData The LTP information required to create rCID >
	 * SK > Sequence relational map
	 * \param packet Pointer to the packet
	 * \param packetSize Total size of the packet (must be multiple of 8)
	 */
	void _bufferLtpPacket(IcnId &rCid,
			ltp_hdr_data_t &ltpHeaderData, uint8_t *packet,
			uint16_t packetSize);
	/*!
	 * \brief Add packet received from proxy to LTP buffer
	 *
	 * This method adds the actual packet to the LTP buffer (_proxyPacketBuffer
	 * map) and also stores the iSub CID <> CID mapping for reverse look-ups to
	 * the _cIdReverseLookUp map
	 *
	 * \param cId Content ID (FQDN) - only used for better loggin output
	 * \param rCId iSub CID (URL) - key in proxy buffer map
	 * \param ltpHeaderData LTP header for data
	 * \param packet Pointer to packet
	 * \param packetSize Size of the allocated packet memory
	 */
	void _bufferProxyPacket(IcnId &cId, IcnId &rCId,
			ltp_hdr_data_t &ltpHeaderData, uint8_t *packet,
			uint16_t &packetSize);
	/*!
	 * \brief Delete an entire HTTP packet from the LTP packet storage
	 *
	 * \param rCid The rCID under which the packet has been buffered
	 * \param sessionKey The SK under which the packet has been buffered
	 */
	void _deleteBufferedLtpPacket(IcnId &rCid, uint16_t &sessionKey);
	/*!
	 * \brief Delete an entire NACK group
	 *
	 * \param rCid The rCID identifying the NACK group
	 * \param ltpHeaderNack The LTP header holding SK to identify the
	 * NACK group to be deleted
	 */
	void _deleteNackGroup(IcnId &rCid, ltp_hdr_ctrl_nack_t &ltpHeaderNack);
	/*!
	 * \brief Delete a buffered proxy packet from LTP buffer
	 *
	 * \param rCId The rCID for which the proxy packet buffer should be look up
	 * \param ltpHeader The LTP header holding all the required information to
	 * delete the right packet
	 */
	void _deleteProxyPacket(IcnId &rCId, ltp_hdr_ctrl_wed_t &ltpHeader);
	/*!
	 * \brief Delete a rCID > NID(s) > SK entry from
	 * _sessionEndedResponses map
	 *
	 * \param rCid The rCID under which the other values are stored
	 * \param ltpHdrCtrlSed  The LTP CTRL SED header which holds the SK
	 * \param nodeIds The list of NIDs for which the SKs should be deleted
	 */
	void _deleteSessionEnd(IcnId &rCid, ltp_hdr_ctrl_se_t &ltpHdrCtrlSe,
			list<NodeId> &nodeIds);

	/*!
	 * \brief Enable a NID to be ready to receive an HTTP response
	 *
	 * \param rCId The rCID for which the NID should be enabled
	 * \param nodeId The NID which should be enabled
	 */
	void _enableNIdInCmcGroup(IcnId &rCId, NodeId &nodeId);
	/*!
	 * \brief Check if data can be published to a particular NID
	 *
	 * \param nodeId The NID which shall be checked
	 *
	 * \return Boolean indicating if NID can be used to publish data to
	 */
	bool _forwardingEnabled(NodeId &nodeId);
	/*!
	 * \brief Obtain the list of NIDs in a particular CMC group
	 *
	 * Note, this method acts on pointers to the CMC group map declared and
	 * maintained in the class Http.
	 *
	 * This method does NOT lock the CMC group's mutex
	 *
	 */
	bool _getCmcGroup(IcnId &rCid, uint16_t &sessionKey,
			list<NodeId> &cmcGroup);
	/*!
	 * \brief Handle an incoming LTP control message
	 *
	 * \param rCId The rCID under which the packet had been published
	 * \param packet Pointer to the entire CTRL message
	 * \param sessionKey When returning TP_STATE_ALL_FRAGMENTS_RECEIVED this
	 * argument contains the sesion key the LTP CTRL header carried
	 */
	TpState _handleControl(IcnId &rCId, uint8_t *packet, uint16_t &sessionKey);
	/*!
	 * \brief Handle an incoming LTP control message (iSub)
	 *
	 * \param cId The CID under which the packet has been published
	 * \param rCId The iSub CID under which the response must be published
	 * \param nodeId The NID which must be used when publishing the response
	 * \param packet Pointer to the CTRL message
	 * \param sessionKey This field is filled out if WE has been received and
	 * all fragment were received
	 */
	TpState _handleControl(IcnId &cId, IcnId &rCId, NodeId &nodeId,
			uint8_t *packet, uint16_t &sessionKey);
	/*!
	 * \brief Handling an incoming HTTP response
	 *
	 * \param rCid The rCID under which the packet has been received
	 * \param packet Pointer to the packet
	 */
	void _handleData(IcnId &rCid, uint8_t *packet);
	/*!
	 * \brief Handle an incoming HTTP request
	 *
	 * Note, packet must not point to the entire ICN payload anymore.  Instead,
	 * it has been already moved to the correct memory field
	 *
	 * \param cId The CID under which the packet has been published
	 * \param rCId The iSub CID under which the response must be published
	 * \param nodeId The NID which must be used when publishing the response
	 * \param packet Pointer to data
	 * \param packetSize Pointer to the length of data
	 */
	void _handleData(IcnId cId, IcnId &rCId, NodeId &nodeId,
			uint8_t *packet);
	/*!
	 * \brief Obtain the group of NIDs that have sent a NACK in response to
	 * a WE CTRL message
	 *
	 * \param rCid The rCID for which the group size should be obtained
	 * \param ltpHeaderNack The LTP header of the NACK message received
	 *
	 * \return The list of NIDs in the NACK group for this particular rCID > SK
	 */
	nack_group_t _nackGroup(IcnId &rCid, ltp_hdr_ctrl_nack_t &ltpHeaderNack);
	/*!
	 * \brief Update the RTT
	 *
	 * RTT is used by the NACK timer to determine whether or not a control
	 * message to one of the subscribers got lost
	 *
	 * \param rtt The value which should be used to update _rtt
	 */
	void _recalculateRtt(boost::posix_time::time_duration &rtt);
	/*!
	 * \brief Publish HTTP responses
	 *
	 * When this method returns the LTP CTRL-WE header is filled with the last
	 * sequence number and the randomly generated session key
	 *
	 * \param rCId Reference to the iSub content identifier
	 * \param ltpHeaderCtrlWe Reference to the LTP CTRL-WE header
	 * \param nodeIds List of NIDs to which the data should be sent
	 * \param packet Pointer to the data
	 * \param dataSize Pointer to the length of the packet
	 */
	void _publishData(IcnId &rCId, ltp_hdr_ctrl_we_t &ltpHeaderCtrlWe,
			list<NodeId> &nodeIds, uint8_t *data, uint16_t &dataSize);
	/*!
	 * \brief Publish HTTP requests
	 *
	 * Called by public publish*() methods
	 *
	 * \param cId Reference to the content identifier
	 * \param rCId Reference to the iSub content identifier
	 * \param sessionKey Reference to the session key provided by the caller
	 * \param packet Pointer to the data
	 * \param dataSize Pointer to the length of the packet
	 *
	 * \return This method returns the sequence number of the last fragment to
	 * create LTP CTRL messages based on this important piece of information
	 */
	uint16_t _publishData(IcnId &cId, IcnId &rCId, uint16_t &sessionKey,
			uint8_t *data, uint16_t &dataSize);
	/*!
	 * \brief Publish a range of segments from an HTTP request in response to
	 * previously received NACKs
	 *
	 * This method looks up the CID for the provided rCID and publishes the
	 * range of LTP fragments provided in the LTP header
	 *
	 * \param rCid The rCID under which the packet(s) shall be published
	 * \param ltpCtrlNack The LTP CTRL NACK message received from an sNAP
	 */
	void _publishDataRange(IcnId &rCid, ltp_hdr_ctrl_nack_t &ltpCtrlNack);
	/*!
	 * \brief Publish a range of segments from an HTTP response in response to
	 * previously received NACKs
	 *
	 * \param rCid The rCID under which the packet(s) shall be published
	 * \param sessionKey The SK used in the LTP header to publish the packets
	 * \param nackGroup The NIDs to which the range of segments shall be sent
	 */
	void _publishDataRange(IcnId &rCid, uint16_t &sessionKey,
			nack_group_t &nackGroup);
	/*!
	 * \brief Publish NACK in response to missing segments from an HTTP request
	 * session
	 *
	 * \param cid The CID under which the packet should be published
	 * \param nodeId The NID to which the NACK should be sent
	 * \param sessionKey The SK to which teh sequence range belongs
	 * \param firstMissingSequence The start of the range of sequence numbers
	 * which need to be re-send
	 * \param lastMissingSequence The end of the range of sequence numbers
	 * which need to be re-send
	 */
	void _publishNegativeAcknowledgement(IcnId &rCid, NodeId &nodeId,
			uint16_t &sessionKey, uint16_t &firstMissingSequence,
			uint16_t &lastMissingSequence);
	/*!
	 * \brief Publish NACK in response to missing segments from an HTTP response
	 * session
	 *
	 * \param cid The CID under which the packet should be published
	 * \param rCid The rCID which will be included in the publication
	 * \param sessionKey The SK to which teh sequence range belongs
	 * \param firstMissingSequence The start of the range of sequence numbers
	 * which need to be re-send
	 * \param lastMissingSequence The end of the range of sequence numbers
	 * which need to be re-senduint8_t attempts
	 */
	void _publishNegativeAcknowledgement(IcnId &cid, IcnId &rCid,
			uint16_t &sessionKey, uint16_t &firstMissingSequence,
			uint16_t &lastMissingSequence);
	/*!
	 * \brief Publish session end notification
	 *
	 * \param rCid The rCID under which the CTRL message shall be published
	 * \param nodeIds The NIDs to which the SE notification shall be published
	 * \param ltpHeaderCtrlSe The LTP CTRL-SE header
	 */
	void _publishSessionEnd(IcnId &rCid, list<NodeId> &nodeIds,
			ltp_hdr_ctrl_se_t &ltpHeaderCtrlSe);
	/*!
	 * \brief Publish session ended in response to a received session end
	 *
	 * \param cid The CID under which SED will be published
	 * \param rCid The rCid used for the publish_data_isub
	 * \param sessionKey The SK for the LTP header
	 */
	void _publishSessionEnded(IcnId &cid, IcnId &rCid, uint16_t &sessionKey);
	/*!
	 * \brief Publish CTRL-WE message to a subscriber via iSub
	 *
	 * \param rCId The rCID under which the CTRL message should be published
	 * \param nodeIds The list of NIDs in the CMC group
	 * \param ltpHeader The LTP CTRL-WE header with the sequence number
	 */
	void _publishWindowEnd(IcnId &rCId, list<NodeId> &nodeIds,
			ltp_hdr_ctrl_we_t &ltpHeader);
	/*!
	 * \brief Publish CTRL-WE message to a subscriber via iSub
	 *
	 * \param cId The CID under which the CTRL WE message is published
	 * \param rCId The iSub CId used when this method calls
	 * Blackadder::publish_data_iSub
	 * \param sessionKey The session key for this HTTP session
	 * \param sequenceNumber The sequence number of the last segment published
	 */
	void _publishWindowEnd(IcnId &cId, IcnId &rCId, uint16_t &sessionKey,
			uint16_t &sequenceNumber);
	/*!
	 * \brief Publish CTRL-WED message to the sNAP in response to a received
	 * CTRL-WE message
	 *
	 * \param cid The CID under which the packet should be published
	 * \param rCid The rCID used to use implicit subscription
	 * \param ltpHeaderCtrlWed The LTP CTRL-WED header which already carries
	 * a session key
	 */
	void _publishWindowEnded(IcnId &cid, IcnId &rCid,
			ltp_hdr_ctrl_wed_t &ltpHeaderCtrlWed);
	/*!
	 * \brief Send CTRL WED message to a publisher subscribed to the rCID
	 *
	 * \param rCId The CID under which the message gets published
	 * \param nodeId The NID of the NAP waiting for the WED message
	 * \param sessionKey The session key for this HTTP session
	 */
	void _publishWindowEnded(IcnId &rCId, NodeId &nodeId, uint16_t &sessionKey);
	/*!
	 * \brief Send CTRL WU message to a list of subscribers (via CMC)
	 *
	 * \param rCid The rCID under which the packet is going to be published
	 * \param sessionKey The sesion key used in the LTP header
	 * \param nodeIds List of NIDs to which the packet should be published
	 */
	void _publishWindowUpdate(IcnId &rCid, uint16_t &sessionKey,
			list<NodeId> &nodeIds);
	/*!
	 * \brief Publish CTRL-WUD message to the sNAP in response to a received
	 * CTRL-WU message
	 *
	 * \param cid The CID under which the packet should be published
	 * \param rCid The rCID used to use implicit subscription
	 * \param ltpHeaderControlWud SK used for the LTP CTRL-WUD
	 * header
	 */
	void _publishWindowUpdated(IcnId &cid, IcnId &rCid,
			ltp_hdr_ctrl_wud_t &ltpHeaderControlWud);
	/*!
	 * \brief Remove a list of NIDs from the list of NIDs in a locked CMC group
	 *
	 * This method works on the _cmcGroups map
	 *
	 * \param rCid The rCID for which the list of NIDs is supposed to be removed
	 * \param SK The SK for which the list of NIDS is supposed to be removed
	 * \param nodeIds The list of NIDs which are supposed to be removed
	 */
	void _removeNidsFromCmcGroup(IcnId &rCid, uint16_t &sessionKey,
			list<NodeId> &nodeIds);
	/*!
	 * \brief Remove a list of NIDs from the list of NIDs from where WEDs are
	 * expected for a particular rCID > SK
	 *
	 * This method works on the _windowUpdate map
	 *
	 * \param rCid The rCID for which the list of NIDs is supposed to be removed
	 * \param SK The SK for which the list of NIDS is supposed to be removed
	 * \param nodeIds The list of NIDs which are supposed to be removed
	 */
	void _removeNidsFromWindowUpdate(IcnId &rCid, uint16_t &sessionKey,
			list<NodeId> &nodeIds);
	/*!
	 * \brief Obtain the current median over _rtts list
	 *
	 * \return RTT in ms
	 */
	uint16_t _rtt();
	/*!
	 * \brief Update the round trip time (if applicable)
	 *
	 * \param rtt An obtained RTT which should be checked to be updated
	 */
	void _rtt(uint16_t rtt);
	/*!
	 * \brief Obtain the list of NIDs for which a WED was not received
	 *
	 * When publishing HTTP responses and running out of credit a CTRL-WU is
	 * published to all subscribers. This method allows the sNAP to check for
	 * which NID the WED had been received.
	 *
	 * Note, _windowUpdateMutex is not used in this private method, as it is
	 * called after the mutex was locked when an sNAP was running out of traffic
	 *
	 * \param rCid The rCID for which the list of NIDs should be obtained
	 * \param sessionKey The SK for which the list of NIDs should be obtained
	 *
	 * \return A list with the NIDs
	 */
	list<NodeId> _wudsNotReceived(IcnId &rCid, uint16_t &sessionKey);
};

} /* namespace lightweight */

} /* namespace transport */

#endif /* NAP_TRANSPORT_LEIGHTWEIGHT_HH_ */
