/*
 * icn.cc
 *
 *  Created on: 25 Apr 2016
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

#include <mutex>

#include <icn.hh>
#include <icnproxythreadcleaner.hh>
#include <proxies/http/tcpclient.hh>

using namespace icn;
using namespace log4cxx;
using namespace namespaces::ip;
using namespace namespaces::http;
using namespace proxies::http::tcpclient;
using namespace std;
using namespace transport::lightweight;
using namespace transport::unreliable;

LoggerPtr Icn::logger(Logger::getLogger("icn"));

Icn::Icn(Blackadder *icnCore, Configuration &configuration,
		Namespaces &namespaces, Transport &transport)
	: _icnCore(icnCore),
	  _configuration(configuration),
	  _namespaces(namespaces),
	  _transport(transport)
{}

Icn::~Icn(){}

void Icn::operator ()()
{
	IcnId icnId;
	IcnId rCId;
	string icnIdStr;
	string rCIdStr;
	uint16_t dataLength = 0;
	uint8_t *retrievedPacket = (uint8_t *)malloc(65535);
	uint16_t retrievedPacketSize = 0;
	std::mutex tcpClientThreadsMutex;
	vector<std::thread> tcpClientThreads;
	TcpClient tcpClient(_configuration, _namespaces);//, tcpClientMutex);
	LOG4CXX_DEBUG(logger, "ICN listener thread started");

	//starting TCP client thread cleaner (HTTP proxy)
	IcnProxyThreadCleaner icnProxyThreadCleaner(tcpClientThreads,
				tcpClientThreadsMutex);
	std::thread proxyCleanerThread = std::thread(icnProxyThreadCleaner);

	while (true)
	{
		Event event;
		_icnCore->getEvent(event);
		icnIdStr = chararray_to_hex(event.id);
		icnId = icnIdStr;

		switch (event.type)
		{
		/*
		 * Scope published
		 */
		case SCOPE_PUBLISHED:
			LOG4CXX_DEBUG(logger, "SCOPE_PUBLISHED received for ICN ID "
					<< icnId.print());
			_namespaces.subscribeScope(icnId);
			break;
		/*
		 * Scope unpublished
		 */
		case SCOPE_UNPUBLISHED:
			LOG4CXX_DEBUG(logger, "SCOPE_UNPUBLISHED received for ICN ID "
					<< icnId.print());
			break;
		/*
		 * Start publish
		 */
		case START_PUBLISH:
		{
			LOG4CXX_DEBUG(logger, "START_PUBLISH received for CID "
					<< icnId.print());
			_namespaces.forwarding(icnId, true);
			_namespaces.publishFromBuffer(icnId);
			break;
		}
		/*
		 * Start publish iSub
		 */
		case START_PUBLISH_iSUB:
		{
			NodeId nodeId = event.id;
			LOG4CXX_DEBUG(logger, "START_PUBLISH_iSUB received for NID "
					<< nodeId.uint());
			_namespaces.forwarding(nodeId, true);
			//_namespaces.publishFromBuffer(nodeId);//should be always empty
			break;
		}
		/*
		 * Stop publish
		 */
		case STOP_PUBLISH:
		{
			LOG4CXX_DEBUG(logger, "STOP_PUBLISH received for CID "
					<< icnId.print());
			_namespaces.forwarding(icnId, false);
			break;
		}
		//FIXME No STOP_PUBLISH_iSUB???
		/*
		 * Pause publish
		 */
		case PAUSE_PUBLISH:
			_namespaces.forwarding(icnId, false);
			break;
		/*
		 * Published data
		 */
		case PUBLISHED_DATA:
		{
			dataLength = event.data_len;
			uint16_t sessionKey = 0;
			LOG4CXX_TRACE(logger, "PUBLISHED_DATA of length " << event.data_len
					<< " received under (r)CID " << icnId.print());
			TpState tpState = _transport.handle(icnId, event.data, dataLength,
				sessionKey);

			if (tpState == TP_STATE_ALL_FRAGMENTS_RECEIVED)
			{
				bzero(retrievedPacket, 65535);
				string nodeId = _configuration.nodeId().str();
				// If packet could be retrieve, send it
				if (_transport.retrievePacket(icnId, nodeId,
						sessionKey, retrievedPacket, retrievedPacketSize))
				{
					_namespaces.sendToEndpoint(icnId,
							retrievedPacket, retrievedPacketSize);
				}
				else
				{
					LOG4CXX_WARN(logger, "Packet could not be retrieved");
				}
			}
			else if (tpState == TP_STATE_SESSION_ENDED)
			{
				_namespaces.endOfSession(icnId, sessionKey);
			}
			else if (tpState == TP_STATE_NO_TRANSPORT_PROTOCOL_USED)
			{
				_namespaces.handlePublishedData(icnId, event.data,
						event.data_len);
			}
			break;
		}
		/*
		 * Published data iSub for HTTP-over-ICN namespace (requests)
		 */
		case PUBLISHED_DATA_iSUB:
		{
			rCIdStr = chararray_to_hex(event.isubID);
			rCId = rCIdStr;
			dataLength = event.data_len;
			uint16_t sessionKey;
			LOG4CXX_TRACE(logger, "PUBLISHED_DATA_iSUB received for CID "
						<< icnId.print() << " and rCID " << rCId.print()
						<< " of length " << event.data_len);
			// add NID > rCID look up to HTTP handler so when START_PUBLISH_iSUB
			// arrives the corresponding rCID can be looked up
			_namespaces.Http::addReversNidTorCIdLookUp(event.nodeId, rCId);

			// handle received publication
			if (_transport.handle(icnId, rCId, event.nodeId, event.data,
					dataLength, sessionKey)
					== TP_STATE_ALL_FRAGMENTS_RECEIVED)
			{
				bzero(retrievedPacket, 65535);

				if (!_transport.retrievePacket(rCId, event.nodeId,
						sessionKey, retrievedPacket, retrievedPacketSize))
				{//packet retrieval failed. Break here
					LOG4CXX_TRACE(logger, "HTTP request packet retrieval failed"
							"for CID " << icnId.print() << ", rCID "
							<< rCId.print() << " and NID " << event.nodeId);
					break;
				}

				// switch over root scope to know which proxy should be called
				switch (icnId.rootNamespace())
				{
				case NAMESPACE_HTTP:
				{
					tcpClient.preparePacketToBeSent(icnId, rCId, sessionKey,
							event.nodeId, retrievedPacket, retrievedPacketSize);
					//start TCP client thread
					tcpClientThreadsMutex.lock();
					tcpClientThreads.push_back(std::thread(tcpClient));
					tcpClientThreadsMutex.unlock();
					// TODO clean up old thread entries
					break;
				}
				case NAMESPACE_COAP://example entry point for other proxies
					break;
				}
			}
			break;
		}
		/*
		 * Re-publish
		 */
		case RE_PUBLISH:
			LOG4CXX_TRACE(logger, "RE_PUBLISH received for CID "
						<< icnId.print());
			break;
		/*
		 * Resume publish
		 */
		case RESUME_PUBLISH:
			LOG4CXX_DEBUG(logger, "RESUME_PUBLISH received for CID "
						<< icnId.print());
			break;
		default:
			LOG4CXX_WARN(logger, "Unknown BA API event type received.");
		}
	}

	free(retrievedPacket);//clean up memory
}
