/*
 * tcpclient.hh
 *
 *  Created on: 28 Jul 2016
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

#ifndef NAP_PROXIES_HTTP_TCPCLIENT_HH_
#define NAP_PROXIES_HTTP_TCPCLIENT_HH_

#include <arpa/inet.h>
#include <log4cxx/logger.h>
#include <map>
#include <unistd.h>
#include <unordered_map>

#include <configuration.hh>
#include <namespaces/namespaces.hh>
#include <proxies/http/httpproxytypedefs.hh>
#include <types/nodeid.hh>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

using namespace log4cxx;
using namespace std;

namespace proxies {

namespace http {

namespace tcpclient {
/*
 * \brief
 */
class TcpClient {
	static LoggerPtr logger;
public:
	/*
	 * \brief Constructor.
	 *
	 * \param configuration Reference to the class Configuration
	 * \param namespaces Reference to the class Namespaces
	 */
	TcpClient(Configuration &configuration, Namespaces &namespaces);
	/*
	 * \brief Destructor
	 */
	~TcpClient();
	/*!
	 * \brief Functor
	 *
	 * Only used if new TCP client session is required. If so, the sendData
	 * method returned true to indicate that a new TCP client session is
	 * required.
	 *
	 * This method will always create a new TCP socket towards the registered
	 * IP service endpoint.
	 */
	void operator()();
	/*!
	 * \brief Send data to IP endpoint
	 *
	 * This method stores the required information
	 *
	 * \param cId Reference to the CID to allow the TCP client to look up the
	 * registered IP address for the hashed FQDN this CID represents
	 * \param rCId The rCID the cNAP generated and send over to the sNAP for
	 * this particular web resource
	 * \param sessionKey The session key of the LTP key the HTTP request has
	 * been received
	 * \param nodeId The NID which sent the HTTP request
	 * \param packet Pointer to the HTTP request packet
	 * \param packetSize Size of HTTP request packet
	 */
	void preparePacketToBeSent(IcnId &cId, IcnId &rCId, uint16_t &sessionKey,
			string &nodeId, uint8_t *packet, uint16_t &packetSize);
private:
	Configuration &_configuration;/*!< Reference to Configuration class */
	Namespaces &_namespaces;/*!< Reference to Namespaces class */
	reverse_lookup_t *_reverseLookup;/*!< get
	rCID and 0 for a particular file descriptor to easier delete it
	eventually from _openedTcpSessions if required. */
	reverse_lookup_t::iterator _reverseLookupIt;/*!< Iterator for
	_reverseLookup map */
	boost::mutex *_reverseLookupMutex;/*!< mutex for _reverseLookup map */
	socket_fds_t *_socketFds;/*!<
	map<NID, map<remote SK, local SK*/
	socket_fds_t::iterator _socketFdsIt;/*!< Iterator for _socketFdMap */
	boost::mutex *_socketFdsMutex;/*!< Mutex for transaction safe operations on
	_socketFds map */
	unordered_map<uint16_t, bool> *_socketState;/*!< u_map<SFD, writeable> make
	socket closure states available to all threads*/
	unordered_map<uint16_t, bool>::iterator _socketStateIt;/*!< Iterator for
	_socketState map*/
	boost::mutex *_socketStateMutex;/*!< Mutex for _socketState */
	IcnId _cidTmp; /*!< Temporary variable to start TCP client thread */
	IcnId _rCidTmp; /*!< Temporary variable to start TCP client thread */
	uint8_t *_packetTmp; /*!< Temporary variable to start TCP client thread */
	uint16_t _packetSizeTmp; /*!< Temporary variable to start TCP client thread
	*/
	uint16_t _sessionKeyTmp; /*!< Temporary variable to start TCP client thread
	*/
	NodeId _nodeId; /*!< The NID this NAP is running on*/
	/*!
	 * \brief Add rCID to reverse look-up table based on FD
	 */
	void _addReverseLookup(int &socketFd, IcnId &rCId);
	/*!
	 * \brief Delete a socket FD from the socket state map
	 *
	 * \param socketFd The FD to be deleted
	 */
	void _deleteSocketState(uint16_t socketFd);
	/*!
	 * \brief Obtain the rCID from the _reverseLookup map
	 *
	 * \param socketFd The socket FD which should be looked up
	 *
	 * \return Reference to the rCID which has been stored for this socket FD
	 */
	IcnId &_rCid(int &socketFd);
	/*!
	 * \brief Set the status of a socket file descriptor
	 *
	 * \param socketFd The socket FD for which the status is supposed to be set
	 * \param state The writeable state of the socketFd
	 */
	void _setSocketState(uint16_t socketFd, bool state);
	/*!
	 * \brief Obtain socket state
	 *
	 * \param socketFd The FD for which the state should be retrieved
	 *
	 * \return The state for the FD stored in the map
	 */
	bool _socketWriteable(uint16_t socketFd);
	/*!
	 * \brief Create/obtain TCP socket to send off HTTP request to server
	 *
	 * The CID is required here because it is used to look up the IP address
	 * from the FQDN registration
	 *
	 * \param cId The CID used to store IP endpoint configuration (sNAP/eNAP)
	 * \param nodeId The NID which has sent the HTTP request
	 * \param remoteSocketFd The session key under which the HTTP reqeust has
	 * been received (which corresponds to the socket FD)
	 * \param callRead Reference to boolean indicating of the functor must call
	 * read() or if there's another functor which is already reading from the FD
	 *
	 * \return Socket file descriptor
	 */
	int _tcpSocket(IcnId &cId, NodeId &nodeId, uint16_t &remoteSocketFd,
			bool &callRead);
};

} /* namsepace tcpclient */

} /* namsepace http */

} /* namespace proxies */

#endif /* NAP_PROXIES_HTTP_TCPCLIENT_HH_ */
