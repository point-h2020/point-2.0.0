/*
 * application.hh
 *
 *  Created on: 20 Feb 2016
 *      Author: Sebastian Robitzsch <sebastian.robitzsch@interdigital.com>
 *      		Mays Al-Naday <mfhaln@essex.ac.uk>
 *
 * This file is part of the MOnitoring LibrarY (MOLY).
 *
 * MOLY is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * MOLY is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * MOLY. If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef MOLY_APPLICATION_HH_
#define MOLY_APPLICATION_HH_

#include <blackadder_enums.hpp>
#include <boost/thread/mutex.hpp>
#include "enum.hh"
#include <errno.h>
#include <iostream>
#include <linux/netlink.h>
#include "primitives.hh"
#include <sys/socket.h>
#include "typedef.hh"

using namespace std;

/*!
 * \brief Implementation of boostrapping class for applications
 */
class Application {
public:
	/*!
	 * \brief Constructor
	 */
	Application();
	/*!
	 * \brief Denstructor
	 */
	~Application();
	/*!
	 * \brief Implementation of the ADD_LINK primitive
	 *
	 * STATE_UP by default
	 *
	 */
	bool addLink(string name, string linkId, uint32_t sourceNodeId,
			uint32_t destinationNodeId, uint8_t linkType);
	/*!
	 * \brief Implementation of the ADD_NODE primitive
	 */
	bool addNode(string name, uint32_t nodeId, uint8_t nodeType);
	/*!
	 * \brief Implementation of the ADD_NODE_TYPE primitive
	 */
	bool addNodeType(string name, uint8_t nodeType);
	/*!
	 * \brief Implementation of the CMC_GROUP_SIZE primitive
	 *
	 * \param groupSize The CMC group size to be reported
	 *
	 * \return Boolean indicating whether or not the data point has been
	 * successfully reported
	 */
	bool cmcGroupSize(uint32_t groupSize);
	/*!
	 * \brief Implementation of the HTTP_REQUESTS_PER_FQDN primitive
	 *
	 * \param fqdn The FQDN
	 * \param httpRequests The number of HTTP requests traversed the node
	 *
	 * \return Boolean indicating whether or not the primitive call was
	 * successful
	 */
	bool httpRequestsPerFqdn(string fqdn, uint32_t httpRequests);
	/*!
	 * \brief Implementation of the LINK_STATE primitive
	 *
	 * \param linkId The link identifier
	 * \param destinationNodeId The DST NID
	 * \param linkType The link type
	 * \param state The new state
	 *
	 * \return  Boolean indicating whether or not the data point has been
	 * successfully reported
	 */
	bool linkState(uint32_t linkId, uint32_t destinationNodeId,
			uint8_t linkType, uint8_t state);
	/*!
	 * \brief Implementation of the NETWORK_LATENCY primitive
	 *
	 * \param fqdn The FQDN for which the latency is reported
	 * \param latency The network latency to be reported
	 *
	 * \return  Boolean indicating whether or not the data point has been
	 * successfully reported
	 */
	bool networkLatencyPerFqdn(uint32_t fqdn, uint16_t latency);
	/*!
	 * \brief Implementation of the NODE_STATE primitive
	 *
	 * \param nodeType The node's role/type
	 * \param nodeId The NID
	 * \param state The new state
	 *
	 * \return  Boolean indicating whether or not the data point has been
	 * successfully reported
	 */
	bool nodeState(uint8_t nodeType, uint32_t nodeId, uint8_t state);
	/*!
	 * \brief Blocking method waiting for the start trigger message from the
	 * monitoring server
	 *
	 * In order to not shoot monitoring data too soon towards the monitoring
	 * agent, the application that requires to report monitoring data must wait
	 * for the trigger.
	 */
	bool startReporting();
protected:
	int _reportingSocket;
	uint32_t _pid;
	struct sockaddr_nl _agentSocketAddress;
	boost::mutex _reportingSocketMutex;
private:
	int _bootstrapSocket;
	/*!
	 * \brief Send off the generated message
	 */
	bool _send(uint8_t messageType, void *data, uint32_t dataSize);
};

#endif /* MOLY_APPLICATION_HH_ */
