/*
 * statistics.cc
 *
 *  Created on: 2 Sep 2016
 *      Author: Sebastian Robitzsch
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

#include "statistics.hh"

using namespace monitoring::statistics;

LoggerPtr Statistics::logger(Logger::getLogger("monitoring.statistics"));

Statistics::Statistics() {
	_cmcGroupSizes.first = 0;
	_cmcGroupSizes.second = 0;
}

uint32_t Statistics::averageCmcGroupSize()
{
	uint32_t averageCmcGroupSize;
	_mutex.lock();
	if (_cmcGroupSizes.first != 0)
	{
		averageCmcGroupSize =
				floor(_cmcGroupSizes.second / _cmcGroupSizes.first);
	}
	else
	{
		averageCmcGroupSize = 0;
	}
	_cmcGroupSizes.first = 0;
	_cmcGroupSizes.second = 0;
	_mutex.unlock();
	return averageCmcGroupSize;
}

unordered_map<uint32_t, uint16_t> Statistics::averageNetworkDelayPerFqdn()
{
	unordered_map<uint32_t, uint16_t> latencyPerFqdn;
	forward_list<uint16_t>::iterator fListIt;
	uint32_t rttSum;
	uint16_t listSize;
	uint16_t latency;
	_mutex.lock();
	for (_rttPerFqdnIt = _rttPerFqdn.begin();
			_rttPerFqdnIt != _rttPerFqdn.end(); _rttPerFqdnIt++)
	{
		listSize = 0;
		rttSum = 0;
		// Get RTT sum
		for (fListIt = _rttPerFqdnIt->second.begin();
				fListIt != _rttPerFqdnIt->second.end(); fListIt++)
		{
			rttSum += *fListIt;
			listSize++;
		}
		// Calculate & insert sum of not 0
		if (rttSum != 0)
		{
			latency = ceil((rttSum / listSize) / 2);
			latencyPerFqdn.insert(pair<uint32_t, uint16_t>(_rttPerFqdnIt->first,
					latency));
			// Now reset all values for this FQDN
			_rttPerFqdnIt->second.clear();
		}
	}
	_mutex.unlock();
	return latencyPerFqdn;
}

void Statistics::cmcGroupSize(uint32_t cmcGroupSize)
{
	_mutex.lock();
	_cmcGroupSizes.first++;
	_cmcGroupSizes.second += cmcGroupSize;
	_mutex.unlock();
}

void Statistics::ipEndpointAdd(IpAddress ipAddress)
{
	ip_endpoints_t::iterator ipEndpointsIt;
	_mutex.lock();
	ipEndpointsIt = _ipEndpoints.find(ipAddress.uint());
	// new IP endpoint
	if (ipEndpointsIt == _ipEndpoints.end())
	{
		ip_endpoint_t ipEndpoint;
		ipEndpoint.ipAddress = ipAddress;
		ipEndpoint.nodeType = NODE_TYPE_UE;
		ipEndpoint.surrogate = false;
		ipEndpoint.reported = false;
		_ipEndpoints.insert(pair<uint32_t, ip_endpoint_t>(ipAddress.uint(),
				ipEndpoint));
		LOG4CXX_TRACE(logger, "New UE added. IP address " << ipAddress.str());
	}
	_mutex.unlock();
}

void Statistics::ipEndpointAdd(IpAddress ipAddress, uint16_t port)
{
	ip_endpoints_t::iterator ipEndpointsIt;
	_mutex.lock();
	ipEndpointsIt = _ipEndpoints.find(ipAddress.uint());
	// new IP endpoint
	if (ipEndpointsIt == _ipEndpoints.end())
	{
		ip_endpoint_t ipEndpoint;
		ipEndpoint.ipAddress = ipAddress;
		ipEndpoint.nodeType = NODE_TYPE_SERVER;
		ipEndpoint.surrogate = true;
		ipEndpoint.port = port;
		ipEndpoint.reported = false;
		_ipEndpoints.insert(pair<uint32_t, ip_endpoint_t>(ipAddress.uint(),
				ipEndpoint));
		LOG4CXX_TRACE(logger, "New surrogate server added on "
				<< ipAddress.str() << ":" << port)
	}
	_mutex.unlock();
}

void Statistics::ipEndpointAdd(IpAddress ipAddress, string fqdn, uint16_t port)
{
	ip_endpoints_t::iterator ipEndpointsIt;
	_mutex.lock();
	ipEndpointsIt = _ipEndpoints.find(ipAddress.uint());
	// new IP endpoint
	if (ipEndpointsIt == _ipEndpoints.end())
	{
		ip_endpoint_t ipEndpoint;
		ipEndpoint.ipAddress = ipAddress;
		ipEndpoint.nodeType = NODE_TYPE_SERVER;
		ipEndpoint.surrogate = false;
		ipEndpoint.fqdn = fqdn;
		ipEndpoint.port = port;
		ipEndpoint.reported = false;
		_ipEndpoints.insert(pair<uint32_t, ip_endpoint_t>(ipAddress.uint(),
				ipEndpoint));
		LOG4CXX_TRACE(logger, "New server added. FQDN " << fqdn << ", IP "
				"address " << ipAddress.str() << ":" << port);
	}
	_mutex.unlock();
}

ip_endpoints_t Statistics::ipEndpoints()
{
	ip_endpoints_t newIpEndpoints;
	ip_endpoints_t::iterator ipEndpointsIt;
	_mutex.lock();
	ipEndpointsIt = _ipEndpoints.begin();
	while (ipEndpointsIt != _ipEndpoints.end())
	{
		// hasn't been reported
		if (!ipEndpointsIt->second.reported)
		{
			newIpEndpoints.insert(pair<uint32_t, ip_endpoint_t>(
					ipEndpointsIt->first, ipEndpointsIt->second));
			ipEndpointsIt->second.reported = true;
			LOG4CXX_TRACE(logger, "IP endpoint "
					<< ipEndpointsIt->second.ipAddress.str() << " marked as "
							"being reported");
		}
		ipEndpointsIt++;
	}
	_mutex.unlock();
	return newIpEndpoints;
}

http_requests_per_fqdn_t Statistics::httpRequestsPerFqdn()
{
	http_requests_per_fqdn_t httpRequestsPerFqdn;
	_mutex.lock();
	httpRequestsPerFqdn = _httpRequestsPerFqdn;
	_httpRequestsPerFqdn.clear();
	_mutex.unlock();
	return httpRequestsPerFqdn;
}

void Statistics::httpRequestsPerFqdn(string fqdn, uint32_t numberOfRequests)
{
	http_requests_per_fqdn_t::iterator httpRequestsPerFqdnIt;
	_mutex.lock();
	httpRequestsPerFqdnIt = _httpRequestsPerFqdn.find(fqdn);
	// FQDN key does not exist
	if(httpRequestsPerFqdnIt == _httpRequestsPerFqdn.end())
	{
		_httpRequestsPerFqdn.insert(pair<string, uint32_t>(fqdn,
				numberOfRequests));
	}
	// FQDN does exist
	else
	{
		httpRequestsPerFqdnIt->second += numberOfRequests;
	}
	_mutex.unlock();
}

void Statistics::roundTripTime(IcnId cid, uint16_t rtt)
{
	_mutex.lock();
	_rttPerFqdnIt = _rttPerFqdn.find(cid.uintId());//only get the information
	// item which is the hashed FQDN
	// FQDN not found
	if (_rttPerFqdnIt == _rttPerFqdn.end())
	{
		forward_list<uint16_t> rtts;
		rtts.push_front(rtt);
		_rttPerFqdn.insert(pair<uint32_t, forward_list<uint16_t>>(cid.uintId(),
				rtts));
		LOG4CXX_TRACE(logger, "New hashed FQDN " << cid.uintId() << " added to"
				" RTT statistics table")
		_mutex.unlock();
		return;
	}
	// simply add the new rtt value
	_rttPerFqdnIt->second.push_front(rtt);
	_mutex.unlock();
}
