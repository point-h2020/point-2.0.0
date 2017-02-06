/*
 * statistics.hh
 *
 *  Created on: 2 Sep 2016
 *      Author: Sebastian Robitzsch <sebastian.robitzsch@interdigital.com>
 *
 * This file is part of Blackadder.
 *
 * Blackadder is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
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

#ifndef NAP_MONITORING_STATISTICS_HH_
#define NAP_MONITORING_STATISTICS_HH_

#include <boost/thread/mutex.hpp>
#include <forward_list>
#include <log4cxx/logger.h>
#include <unordered_map>

#include <monitoring/monitoringtypedefs.hh>
#include <types/icnid.hh>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

using namespace log4cxx;

namespace monitoring {

namespace statistics {

class Statistics {
	static LoggerPtr logger;
public:
	Statistics();
	/*!
	 * \brief Calculate the average CMC group size over the configured reporting
	 * interval
	 *
	 * \return The average CMC group size of this NAP
	 */
	uint32_t averageCmcGroupSize();
	/*!
	 * \brief Calculate the average network delay based on the RTT values
	 * measured as reported by LTP
	 *
	 * This method also resets the pair in which the RTT is stored
	 * (_roundTripTimes)
	 *
	 * \return The network delay in milliseconds per FQDN
	 */
	unordered_map<uint32_t, uint16_t> averageNetworkDelayPerFqdn();
	/*!
	 * \brief Update the CMC group size counter
	 *
	 * \param cmcGroupSize The CMC group size which should be recorded
	 */
	void cmcGroupSize(uint32_t cmcGroupSize);
	/*!
	 * \brief Obtain the number of HTTP request and their FQDN which have
	 * traversed the NAP
	 *
	 * \return A map with the values (key: fqdn, value: # of requests)
	 */
	http_requests_per_fqdn_t httpRequestsPerFqdn();
	/*!
	 * \brief Update the number of HTTP request for a particular hashed FQDN
	 *
	 * \param fqdn The FQDN for which the number of requests is
	 * reported
	 * \param numberOfRequests The number of HTTP requests
	 */
	void httpRequestsPerFqdn(string fqdn, uint32_t numberOfRequests);
	/*!
	 * \brief Add an IP endpoint to the list of known endpoints
	 *
	 * When using this method the endpoint reported is going to be an UE
	 *
	 * \param ipAddress The IP address of the endpoint
	 */
	void ipEndpointAdd(IpAddress ipAddress);
	/*!
	 * \brief Add a surrogate server to the list of known endpoints
	 *
	 * When using this method the endpoint reported is going to be a surrogate
	 * server
	 *
	 * \param ipAddress IP address of the server
	 * \param port The port to which HTTP requests will be sent
	 */
	void ipEndpointAdd(IpAddress ipAddress, uint16_t port);
	/*!
	 * \brief Add a server to the list of known endpoints
	 *
	 * When using this method the endpoint reported is going to be a server
	 *
	 * \param ipAddress IP address of the server
	 * \param fqdn The FQDN this endpoint serves
	 * \param port The port to which HTTP requests will be sent
	 */
	void ipEndpointAdd(IpAddress ipAddress, string fqdn, uint16_t port);
	/*!
	 * \brief Obtain a full copy of the list of IP endpoints
	 *
	 * \return The list of IP endpoints
	 */
	ip_endpoints_t ipEndpoints();
	/*!
	 * \brief Update the RTT counter for a particular FQDN
	 *
	 * \param cid The CID which holds the FQDN
	 * \param rtt The RTT which should be recorded
	 */
	void roundTripTime(IcnId cid, uint16_t rtt);
private:
	boost::mutex _mutex;
	pair<uint32_t, uint32_t> _cmcGroupSizes;/*!< pair<number of CMC reports, sum
	of all CMC group sizes */
	unordered_map<uint32_t, forward_list<uint16_t>> _rttPerFqdn;
	/*!< u_map<hashed FQDN, forward_list<RTT>> The list size is configurable via
	ltpRttListSize in nap.cfg */
	unordered_map<uint32_t, forward_list<uint16_t>>::iterator _rttPerFqdnIt;/*!<
	iterator for _rttPerFqdn*/
	http_requests_per_fqdn_t _httpRequestsPerFqdn; /*!< pair<fqdn, number of HTTP
	requests> */
	ip_endpoints_t _ipEndpoints;/*!< List if IP	endpoints with their IP
	addresses as the key */
};

} /* namespace statistics */

} /* namespace monitoring */

#endif /* NAP_MONITORING_STATISTICS_HH_ */
