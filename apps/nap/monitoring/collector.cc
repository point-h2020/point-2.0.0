/*
 * collector.cc
 *
 *  Created on: 3 Feb 2016
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

#include <moly/moly.hh>

#include "collector.hh"

using namespace monitoring::collector;

LoggerPtr Collector::logger(Logger::getLogger("monitoring.collector"));

Collector::Collector(Configuration &configuration, Statistics &statistics)
	: _configuration(configuration),
	  _statistics(statistics)
{}

Collector::~Collector() {}

void Collector::operator ()()
{
	Moly moly;
	http_requests_per_fqdn_t httpRequestsPerFqdn;
	uint32_t averageCmcGroupSize = 0;
	LOG4CXX_INFO(logger, "Statistics collector thread started");
	if (!moly.Application::startReporting())
	{
		LOG4CXX_ERROR(logger, "Bootstrapping process with monitoring agent "
				"failed");
		return;
	}
	string nodeName;
	NodeTypes nodeType;
	// FIXME get IP GW, surrogate and Server node types properly implemented
	if (_configuration.icnGateway())
	{
		nodeType = NODE_TYPE_GW;
		nodeName = "ICN GW";
	}
	else
	{
		nodeType = NODE_TYPE_NAP;
		nodeName = "NAP ";
		nodeName.append(_configuration.hostRoutingPrefix().str());
	}
	if (moly.Application::addNode(nodeName, _configuration.nodeId().uint(),
			nodeType))
	{
		LOG4CXX_TRACE(logger, "moly.ADD_NODE(" << nodeName << ", "
				<< _configuration.nodeId().uint() << ", " << nodeType
				<< ") sent");
	}
	else
	{
		LOG4CXX_WARN(logger, "Sending moly.ADD_NODE to monitoring agent "
				"failed");
	}
	while (true)
	{
		sleep(_configuration.molyInterval());
		// collect the data points
		averageCmcGroupSize = _statistics.averageCmcGroupSize();
		if (averageCmcGroupSize != 0)
		{
			LOG4CXX_TRACE(logger, "Average CMC group size of "
					<< averageCmcGroupSize << " reported");
			moly.Application::cmcGroupSize(averageCmcGroupSize);
		}
		// Number of HTTP requests per FQDN
		httpRequestsPerFqdn = _statistics.httpRequestsPerFqdn();
		http_requests_per_fqdn_t::iterator httpRequestsPerFqdnIt;
		if (httpRequestsPerFqdn.size() > 0)
		{
			for (httpRequestsPerFqdnIt = httpRequestsPerFqdn.begin();
					httpRequestsPerFqdnIt != httpRequestsPerFqdn.end();
					httpRequestsPerFqdnIt++)
			{
				if(!moly.Application::httpRequestsPerFqdn(
						httpRequestsPerFqdnIt->first,
						httpRequestsPerFqdnIt->second))
				{
					LOG4CXX_WARN(logger, "Statistics for FQDN "
							<< httpRequestsPerFqdnIt->first << " could not be"
							" sent");
				}
				else
				{
					LOG4CXX_TRACE(logger, "Number of HTTP requests for FQDN "
							<< httpRequestsPerFqdnIt->first << " of "
							<< httpRequestsPerFqdnIt->second << " reported");
				}
			}
		}
		// latency in the network
		unordered_map <uint32_t, uint16_t> latencyPerFqdn;
		unordered_map <uint32_t, uint16_t>::iterator latencyPerFqdnIt;
		latencyPerFqdn = _statistics.averageNetworkDelayPerFqdn();
		for (latencyPerFqdnIt = latencyPerFqdn.begin();
				latencyPerFqdnIt != latencyPerFqdn.end(); latencyPerFqdnIt++)
		{
			if (latencyPerFqdnIt->second != 0)
			{
				if (!moly.networkLatencyPerFqdn(latencyPerFqdnIt->first,
						latencyPerFqdnIt->second))
				{
					LOG4CXX_DEBUG(logger, "Reporting network latency of "
							<< latencyPerFqdnIt->second << "ms for hashed FQDN "
							<< latencyPerFqdnIt->first << " failed");
				}
				LOG4CXX_TRACE(logger, "Reporting network latency of "
						<< latencyPerFqdnIt->second << "ms for hashed FQDN "
						<< latencyPerFqdnIt->first);
			}
		}
		// report newly added IP endpoints
		ip_endpoints_t ipEndpoints = _statistics.ipEndpoints();
		if (!ipEndpoints.empty())
		{
			ip_endpoints_t::iterator ipEndpointsIt;
			ipEndpointsIt = ipEndpoints.begin();
			while (ipEndpointsIt != ipEndpoints.end())
			{
				if (!ipEndpointsIt->second.reported)
				{
					ostringstream nodeName;
					switch (ipEndpointsIt->second.nodeType)
					{
					case NODE_TYPE_UE:
						nodeName << "UE: "
								<< ipEndpointsIt->second.ipAddress.str();
						break;
					case NODE_TYPE_SERVER:
						if (ipEndpointsIt->second.surrogate)
						{
							nodeName << "Surrogate: "
									<< ipEndpointsIt->second.ipAddress.str()
									<< ":" << ipEndpointsIt->second.port;
						}
						else
						{
							nodeName << "Server: " << ipEndpointsIt->second.fqdn
									<< " -> "
									<< ipEndpointsIt->second.ipAddress.str()
									<< ":" << ipEndpointsIt->second.port;
						}
						break;
					default:
						LOG4CXX_WARN(logger, "Unknown node type for IP endpoint"
								<< " "<< ipEndpointsIt->second.ipAddress.str());
					}
					if (!moly.addNodeType(nodeName.str(),
							ipEndpointsIt->second.nodeType))
					{
						LOG4CXX_DEBUG(logger, "IP endpoint reporting failed for"
								<< " " << nodeName.str());
					}
					else
					{
						LOG4CXX_TRACE(logger, "IP endpoint '" << nodeName.str()
								<< "' reported to MA");
						ipEndpointsIt->second.reported = true;
					}
				}
				ipEndpointsIt++;
			}
		}
		// cleaning up
		httpRequestsPerFqdn.clear();
	}
	LOG4CXX_INFO(logger, "Statistics collector thread stopped");
}
