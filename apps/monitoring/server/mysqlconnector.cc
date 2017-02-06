/*
 * mysqlconnector.cc
 *
 *  Created on: 13 Feb 2016
 *      Author: Sebastian Robitzsch <sebastian.robitzsch@interdigital.com>
 *
 * This file is part of the ICN application MOnitOring SErver (MOOSE) which
 * comes with Blackadder.
 *
 * MOOSE is free software: you can redistribute it and/or modify it under
 * the terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * MOOSE is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * MOOSE. If not, see <http://www.gnu.org/licenses/>.
 */

#include <ctime>
#include <memory.h>
#include <stack>

#include "enumerations.hh"
#include "mysqlconnector.hh"

MySqlConnector::MySqlConnector(log4cxx::LoggerPtr logger, string serverIpAddress,
		string serverPort, string username, string password,
		string database, MessageStack &messageStack)
	: _logger(logger),
	  _serverIpAddress(serverIpAddress),
	  _serverPort(serverPort),
	  _username(username),
	  _password(password),
	  _messageStack(messageStack)
{
	try
	{
		string url;
		url.append("tcp://");
		url.append(serverIpAddress);
		url.append(":");
		url.append(serverPort);
		_sqlDriver = get_driver_instance();
		_sqlConnection = _sqlDriver->connect(url, username, password);
		LOG4CXX_INFO(logger, "Connected to visualisation server via " << url);
		_sqlConnection->setSchema(database);
	}
	catch (sql::SQLException &e)
	{
		cout << "# ERR: SQLException in " << __FILE__;
		cout << "(" << __FUNCTION__ << ") on line " << __LINE__ << endl;
		cout << "# ERR: " << e.what();
		cout << " (MySQL error code: " << e.getErrorCode();
		cout << ", SQLState: " << e.getSQLState() << " )" << endl;
	}
}

MySqlConnector::~MySqlConnector() {}

void MySqlConnector::operator()()
{
	LOG4CXX_DEBUG(_logger, "Starting MySQL connector thread");
	IcnId icnId;
	uint8_t *data = (uint8_t *)malloc(65535);
	uint16_t dataLength = 0;
	while(!boost::this_thread::interruption_requested())
	{
		// only read message if packet has been read correctly
		if (_messageStack.readNext(icnId, data, dataLength))
		{
			_send(icnId, data, dataLength);
			bzero(data, 65535);
		}
	}
	free(data);
	delete _sqlConnection;
}

void MySqlConnector::_send(IcnId &icnId, uint8_t *data, uint32_t dataLength)
{
	sql::Statement *sqlStatement;
	CIdAnalyser cIdAnalyser(icnId);
	ostringstream insertOss;
	// First get the EPOCH which comes in all messages
	uint32_t epoch;
	uint8_t offset = 0;
	memcpy(&epoch, data, sizeof(epoch));
	offset = sizeof(epoch);
	switch (cIdAnalyser.primitive())
	{
	case PRIMITIVE_TYPE_ADD_LINK:
	{
		LOG4CXX_TRACE(_logger, "PRIMITIVE_TYPE_ADD_LINK received with "
				"timestamp " << epoch);
		uint32_t sourceNodeId;
		memcpy(&sourceNodeId, data + offset, sizeof(uint32_t));
		insertOss << "INSERT INTO links (link_id, src, dest, link_type)"
				"VALUES ('" << cIdAnalyser.linkId() << "',"
				<< (int)sourceNodeId << ","
				<< (int)cIdAnalyser.destinationNodeId() << ","
				<< (int)cIdAnalyser.linkType() << ")";
		sqlStatement = _sqlConnection->createStatement();
		LOG4CXX_TRACE(_logger, "Execute query: " << insertOss.str());
		sqlStatement->executeUpdate(insertOss.str());
		delete sqlStatement;
		insertOss.str("");
		//Inserting to values table (TODO should be reported properly)
		sqlStatement = _sqlConnection->createStatement();
		insertOss << "INSERT INTO links_values (timestamp, link_id, dest, value"
				", attr_id) VALUES ("
				<< (int)epoch << ",'"
				<< cIdAnalyser.linkId() << "',"
				<< (int)cIdAnalyser.destinationNodeId() << ",'"
				<< _stateStr(STATE_UP) << "',"
				<< (int)ATTRIBUTE_LNK_STATE << ")";
		LOG4CXX_TRACE(_logger, "Execute query: " << insertOss.str());
		sqlStatement->executeUpdate(insertOss.str());
		delete sqlStatement;
		break;
	}
	case PRIMITIVE_TYPE_ADD_NODE:
	{
		sql::ResultSet *sqlResult;
		uint32_t nodeNameLength;
		memcpy(&nodeNameLength, data + offset, sizeof(nodeNameLength));
		offset += sizeof(nodeNameLength);
		char nodeName[nodeNameLength + 1];
		memcpy(nodeName, data + offset, nodeNameLength);
		nodeName[nodeNameLength] = '\0';
		LOG4CXX_TRACE(_logger, "PRIMITIVE_TYPE_ADD_NODE received with "
				"timestamp " << epoch);
		// If UE, simply add it to database. Links are created in
		if (cIdAnalyser.nodeType() == ELEMENT_TYPE_UE ||
				cIdAnalyser.nodeType() == ELEMENT_TYPE_SERVER)
		{
			//First check if node ID does exist in DB and if so, call update
			ostringstream query;
			query << "SELECT * FROM nodes WHERE (name= '" << nodeName
					<< "' AND (node_type = " << ELEMENT_TYPE_UE << " OR "
							"node_type = " << ELEMENT_TYPE_SERVER << "))";
			sqlStatement = _sqlConnection->createStatement();
			sqlResult = sqlStatement->executeQuery(query.str());
			if (sqlResult->next())
			{
				if (sqlResult->getString(3).compare(nodeName) == 0)
				{
					LOG4CXX_TRACE(_logger, "IP endpoint with name '" << nodeName
							<< "' already exists in DB");
					delete sqlStatement;
					delete sqlResult;
					return;
				}
			}
			insertOss << "INSERT INTO nodes (node_id, name, node_type) "
					"VALUES (" << cIdAnalyser.nodeId() << ", '"
					<< nodeName << "', "
					<< (int)cIdAnalyser.nodeType() << ");";
			sqlStatement = _sqlConnection->createStatement();
			LOG4CXX_TRACE(_logger, "Execute query: " << insertOss.str());
			sqlStatement->executeUpdate(insertOss.str());
			delete sqlStatement;
			break;
		}
		//First check if node ID does exist in DB and if so, call update
		ostringstream query;
		query << "SELECT * FROM nodes WHERE (node_id= " << cIdAnalyser.nodeId()
				<< " AND NOT (node_type = " << ELEMENT_TYPE_UE << " OR "
						"node_type = " << ELEMENT_TYPE_SERVER << "))";
		sqlStatement = _sqlConnection->createStatement();
		sqlResult = sqlStatement->executeQuery(query.str());
		if (sqlResult->next())
		{
			ElementTypes elementType;
			// switch over the node type in the database
			switch(sqlResult->getUInt(4))
			{
			case ELEMENT_TYPE_GW:
				if (cIdAnalyser.nodeType() == ELEMENT_TYPE_FN)
				{
					elementType = ELEMENT_TYPE_FN_GW;
				}
				else
				{
					LOG4CXX_ERROR(_logger, "Updating existing node type 'GW' ("
							<< sqlResult->getUInt(4) << ") with "
							<< (int)cIdAnalyser.nodeType()
							<< " has not been implemented");
					delete sqlStatement;
					delete sqlResult;
					return;
				}
				break;
			case ELEMENT_TYPE_FN:
				if (cIdAnalyser.nodeType() == ELEMENT_TYPE_NAP)
				{
					elementType = ELEMENT_TYPE_FN_NAP;
				}
				else if (cIdAnalyser.nodeType() == ELEMENT_TYPE_GW)
				{
					elementType = ELEMENT_TYPE_FN_GW;
				}
				else if (cIdAnalyser.nodeType() == ELEMENT_TYPE_TM)
				{
					elementType = ELEMENT_TYPE_FN_TM;
				}
				else if (cIdAnalyser.nodeType() == ELEMENT_TYPE_RV)
				{
					elementType = ELEMENT_TYPE_FN_RV;
				}
				else
				{
					LOG4CXX_ERROR(_logger, "Updating existing node type 'FN' ("
							<< sqlResult->getUInt(4) << ") with "
							<< (int)cIdAnalyser.nodeType()
							<< " has not been implemented");
					delete sqlStatement;
					delete sqlResult;
					return;
				}
				break;
			case ELEMENT_TYPE_NAP:
				if (cIdAnalyser.nodeType() == ELEMENT_TYPE_FN)
				{
					elementType = ELEMENT_TYPE_FN_NAP;
				}
				else if (cIdAnalyser.nodeType() == ELEMENT_TYPE_TM)
				{
					LOG4CXX_DEBUG(_logger, "Element type NAP+TM does not exist."
							" Silently assuming this is FN+NAP+TM");
					elementType = ELEMENT_TYPE_FN_NAP_TM;
				}
				else if (cIdAnalyser.nodeType() == ELEMENT_TYPE_RV)
				{
					LOG4CXX_DEBUG(_logger, "Element type NAP+RV does not exist."
							" Silently assuming this is FN+NAP+RV");
					elementType = ELEMENT_TYPE_FN_NAP_RV;
				}
				else if (cIdAnalyser.nodeType() == ELEMENT_TYPE_NAP)
				{
					LOG4CXX_WARN(_logger, "Updating existing node type 'NAP' ("
							<< sqlResult->getUInt(4) << ") with "
							<< (int)cIdAnalyser.nodeType()
							<< " again has not been implemented");
					delete sqlStatement;
					delete sqlResult;
					return;
				}
				else
				{
					LOG4CXX_ERROR(_logger, "Updating existing node type 'NAP' ("
							<< sqlResult->getUInt(4) << ") with "
							<< (int)cIdAnalyser.nodeType()
							<< " has not been implemented");
					delete sqlStatement;
					delete sqlResult;
					return;
				}
				break;
			case ELEMENT_TYPE_RV:
				if (cIdAnalyser.nodeType() == ELEMENT_TYPE_FN)
				{
					elementType = ELEMENT_TYPE_FN_RV;
				}
				else
				{
					LOG4CXX_ERROR(_logger, "Updating existing node type 'RV' ("
							<< sqlResult->getUInt(4) << ") with "
							<< (int)cIdAnalyser.nodeType()
							<< " has not been implemented");
					delete sqlStatement;
					delete sqlResult;
					return;
				}
				break;
			case ELEMENT_TYPE_SERVER:
				LOG4CXX_DEBUG(_logger, "Node type 'server' has been already "
						"reported for node ID " << cIdAnalyser.nodeId());
				delete sqlStatement;
				delete sqlResult;
				return;
			case ELEMENT_TYPE_TM:
				if (cIdAnalyser.nodeType() == ELEMENT_TYPE_FN)
				{
					elementType = ELEMENT_TYPE_FN_TM;
				}
				else
				{
					LOG4CXX_ERROR(_logger, "Updating existing node type 'TM' ("
							<< sqlResult->getUInt(4) << ") with "
							<< (int)cIdAnalyser.nodeType()
							<< " has not been implemented");
					delete sqlStatement;
					delete sqlResult;
					return;
				}
				break;
			case ELEMENT_TYPE_UE:
				LOG4CXX_DEBUG(_logger, "Node type 'ue' has been already "
						"reported for node ID " << cIdAnalyser.nodeId());
				delete sqlStatement;
				delete sqlResult;
				return;
			case ELEMENT_TYPE_FN_NAP:
				// TM role reported
				if (cIdAnalyser.nodeType() == ELEMENT_TYPE_TM)
				{
					elementType = ELEMENT_TYPE_FN_NAP_TM;
				}
				// RV role reported
				else if (cIdAnalyser.nodeType() == ELEMENT_TYPE_RV)
				{
					elementType = ELEMENT_TYPE_FN_NAP_RV;
				}
				else
				{
					LOG4CXX_ERROR(_logger, "Updating existing node type 'FN+"
							"NAP' ("
							<< sqlResult->getUInt(4) << ") with "
							<< (int)cIdAnalyser.nodeType()
							<< " has not been implemented");
					delete sqlStatement;
					delete sqlResult;
					return;
				}
				break;
			case ELEMENT_TYPE_FN_GW:
				// TM role reported
				if (cIdAnalyser.nodeType() == ELEMENT_TYPE_TM)
				{
					elementType = ELEMENT_TYPE_FN_GW_TM;
				}
				// RV role reported
				else if (cIdAnalyser.nodeType() == ELEMENT_TYPE_RV)
				{
					elementType = ELEMENT_TYPE_FN_GW_RV;
				}
				else
				{
					LOG4CXX_ERROR(_logger, "Updating existing node type 'FN+"
							"GW' ("	<< sqlResult->getUInt(4) << ") with "
							<< (int)cIdAnalyser.nodeType()
							<< " has not been implemented");
					delete sqlStatement;
					delete sqlResult;
					return;
				}
				break;
			case ELEMENT_TYPE_FN_TM:
				// NAP role reported
				if (cIdAnalyser.nodeType() == ELEMENT_TYPE_NAP)
				{
					elementType = ELEMENT_TYPE_FN_NAP_TM;
				}
				// RV role reported
				else if (cIdAnalyser.nodeType() == ELEMENT_TYPE_RV)
				{
					elementType = ELEMENT_TYPE_FN_TM_RV;
				}
				// GW role reported
				else if (cIdAnalyser.nodeType() == ELEMENT_TYPE_GW)
				{
					elementType = ELEMENT_TYPE_FN_GW_TM;
				}
				else
				{
					LOG4CXX_ERROR(_logger, "Updating existing node type 'FN+TM'"
							" (" << sqlResult->getUInt(4) << ") with "
							<< (int)cIdAnalyser.nodeType()
							<< " has not been implemented");
					delete sqlStatement;
					delete sqlResult;
					return;
				}
				break;
			case ELEMENT_TYPE_FN_RV:
				// NAP role reported
				if (cIdAnalyser.nodeType() == ELEMENT_TYPE_NAP)
				{
					elementType = ELEMENT_TYPE_FN_NAP_RV;
				}
				// TM role reported
				else if (cIdAnalyser.nodeType() == ELEMENT_TYPE_TM)
				{
					elementType = ELEMENT_TYPE_FN_TM_RV;
				}
				// GW role reported
				else if (cIdAnalyser.nodeType() == ELEMENT_TYPE_GW)
				{
					elementType = ELEMENT_TYPE_FN_GW_RV;
				}
				else
				{
					LOG4CXX_ERROR(_logger, "Updating existing node type 'FN+TM'"
							" (" << sqlResult->getUInt(4) << ") with "
							<< (int)cIdAnalyser.nodeType()
							<< " has not been implemented");
					delete sqlStatement;
					delete sqlResult;
					return;
				}
				break;
			case ELEMENT_TYPE_FN_NAP_TM:
				// RV role reported
				if (cIdAnalyser.nodeType() == ELEMENT_TYPE_RV)
				{
					elementType = ELEMENT_TYPE_FN_NAP_TM_RV;
				}
				else
				{
					LOG4CXX_ERROR(_logger, "Updating existing node type 'FN+"
							"NAP+TM'"
							" (" << sqlResult->getUInt(4) << ") with "
							<< (int)cIdAnalyser.nodeType()
							<< " has not been implemented");
					delete sqlStatement;
					delete sqlResult;
					return;
				}
				break;
			case ELEMENT_TYPE_FN_NAP_RV:
				// TM role reported
				if (cIdAnalyser.nodeType() == ELEMENT_TYPE_TM)
				{
					elementType = ELEMENT_TYPE_FN_NAP_TM_RV;
				}
				else
				{
					LOG4CXX_ERROR(_logger, "Updating existing node type 'FN+"
							"NAP+RV' (" << sqlResult->getUInt(4) << ") with "
							<< (int)cIdAnalyser.nodeType()
							<< " has not been implemented");
					delete sqlStatement;
					delete sqlResult;
					return;
				}
				break;
			case ELEMENT_TYPE_FN_GW_TM:
				// RV role reported
				if (cIdAnalyser.nodeType() == ELEMENT_TYPE_RV)
				{
					elementType = ELEMENT_TYPE_FN_GW_TM_RV;
				}
				else
				{
					LOG4CXX_ERROR(_logger, "Updating existing node type 'FN+"
							"GW+RV' (" << sqlResult->getUInt(4) << ") with "
							<< (int)cIdAnalyser.nodeType()
							<< " has not been implemented");
					delete sqlStatement;
					delete sqlResult;
					return;
				}
				break;
			case ELEMENT_TYPE_FN_GW_RV:
				// TM role reported
				if (cIdAnalyser.nodeType() == ELEMENT_TYPE_TM)
				{
					elementType = ELEMENT_TYPE_FN_GW_TM_RV;
				}
				else
				{
					LOG4CXX_ERROR(_logger, "Updating existing node type 'FN+"
							"GW+RV' (" << sqlResult->getUInt(4) << ") with "
							<< (int)cIdAnalyser.nodeType()
							<< " has not been implemented");
					delete sqlStatement;
					delete sqlResult;
					return;
				}
				break;
			case ELEMENT_TYPE_FN_NAP_TM_RV:
				LOG4CXX_ERROR(_logger, "This node has already all possible "
						"roles, i.e., FN + NAP + TM + RV");
				delete sqlStatement;
				delete sqlResult;
				return;
			case ELEMENT_TYPE_FN_GW_TM_RV:
				LOG4CXX_ERROR(_logger, "This node has already all possible "
						"roles, i.e., FN + GW + TM + RV");
				delete sqlStatement;
				delete sqlResult;
				return;
			case ELEMENT_TYPE_LNK:
				break;
			case ELEMENT_TYPE_FN_TM_RV:
				break;
			default:
				LOG4CXX_WARN(_logger, "Unknown node type value "
						<< sqlResult->getUInt(4));
			}
			string name = sqlResult->getString(3);
			// Only append if name != nodeName
			if (name.compare(nodeName) != 0)
			{
				name.append(", ");
				name.append(nodeName);
				LOG4CXX_TRACE(_logger, "Node name appended to " << name);
			}
			insertOss << "UPDATE nodes SET node_type = " << elementType << ", "
					"name = '" << name << "' WHERE node_id= "
					<< cIdAnalyser.nodeId() << ";";
		}
		else
		{
			insertOss << "INSERT INTO nodes (node_id, name, node_type) "
					"VALUES (" << cIdAnalyser.nodeId() << ", '"
					<< nodeName << "', "
					<< (int)cIdAnalyser.nodeType() << ");";
		}
		sqlStatement = _sqlConnection->createStatement();
		LOG4CXX_TRACE(_logger, "Execute query: " << insertOss.str());
		sqlStatement->executeUpdate(insertOss.str());
		delete sqlStatement;
		delete sqlResult;
		break;
	}
	case PRIMITIVE_TYPE_CMC_GROUP_SIZE:
	{
		uint32_t cmcGroupSize;
		LOG4CXX_TRACE(_logger, "PRIMITIVE_TYPE_CMC_GROUP_SIZE received "
				"with timestamp " << epoch);
		memcpy(&cmcGroupSize, data + offset, sizeof(cmcGroupSize));
		insertOss << "INSERT INTO nodes_values (timestamp, node_id, value, "
				"attr_id) VALUES ("
				<< epoch << ", "
				<< (int)cIdAnalyser.nodeId() << ", '"
				<< (int)cmcGroupSize << "', "
				<< (int)ATTRIBUTE_NAP_HTTP_REQ_RES_RATIO << ")";
		sqlStatement = _sqlConnection->createStatement();
		LOG4CXX_TRACE(_logger, "Execute query: " << insertOss.str());
		sqlStatement->executeUpdate(insertOss.str());
		delete sqlStatement;
		break;
	}
	case PRIMITIVE_TYPE_HTTP_REQUESTS_PER_FQDN:
	{
		LOG4CXX_TRACE(_logger, "PRIMITIVE_TYPE_HTTP_REQUESTS_PER_FQDN "
				"received with timestamp " << epoch);
		uint32_t httpRequests;
		uint32_t fqdnLength;
		uint16_t attributeId;
		memcpy(&fqdnLength, data + offset, sizeof(fqdnLength));
		offset += sizeof(fqdnLength);
		char fqdn[fqdnLength + 1];
		memcpy(fqdn, data + offset, fqdnLength);
		fqdn[fqdnLength] = '\0';
		offset += fqdnLength;
		memcpy(&httpRequests, data + offset, sizeof(httpRequests));
		LOG4CXX_TRACE(_logger, "# of HTTP Requests for " << fqdn << ": "
				<< httpRequests);
		switch(cIdAnalyser.nodeType())
		{
		case NODE_TYPE_NAP:
			attributeId = 13;
			break;
		case NODE_TYPE_GW:
			attributeId = 3;
			break;
		default:
			attributeId = 0;
			LOG4CXX_WARN(_logger, "Unknown node type for PRIMITIVE_TYPE_HTTP_"
					"REQUESTS_PER_FQDN");
		}
		insertOss << "INSERT INTO nodes_values (timestamp, node_id, value, "
				"attr_id) VALUES ("
				<< epoch << ", "
				<< (int)cIdAnalyser.nodeId() << ", '"
				<< httpRequests << "', "
				<< attributeId << ")";
		sqlStatement = _sqlConnection->createStatement();
		LOG4CXX_TRACE(_logger, "Execute query: " << insertOss.str());
		sqlStatement->executeUpdate(insertOss.str());
		delete sqlStatement;
		break;
	}
	case PRIMITIVE_TYPE_LINK_STATE:
	{
		uint8_t state;
		LOG4CXX_TRACE(_logger, "PRIMITIVE_TYPE_LINK_STATE "
						"received with timestamp " << epoch);
		memcpy(&state, data + offset, sizeof(state));
		insertOss << "INSERT INTO link_values (timestamp, link_id, dest, value,"
				" attr_id) VALUES ("
				<< epoch << ", "
				<< cIdAnalyser.linkId() << ", "
				<< (int)cIdAnalyser.destinationNodeId() << ", '";
		switch (state)
		{
		case LINK_STATES_UP:
			insertOss << "STATE_UP";
			break;
		case LINK_STATES_DOWN:
			insertOss << "STATE_DOWN";
			break;
		}
		insertOss << "', " << (int)ATTRIBUTE_LNK_STATE << ")";
		sqlStatement = _sqlConnection->createStatement();
		LOG4CXX_TRACE(_logger, "Execute query: " << insertOss.str());
		sqlStatement->executeUpdate(insertOss.str());
		delete sqlStatement;
		break;
	}
	case PRIMITIVE_TYPE_NETWORK_LATENCY_PER_FQDN:
	{
		uint32_t fqdn;
		uint16_t networkLatency;
		LOG4CXX_TRACE(_logger, "PRIMITIVE_TYPE_NETWORK_LATENCY_PER_FQDN "
				"received with timestamp " << epoch);
		memcpy(&fqdn, data + offset, sizeof(fqdn));
		offset += sizeof(fqdn);
		memcpy(&networkLatency, data + offset, sizeof(networkLatency));
		LOG4CXX_TRACE(_logger, "Network latency at NAP " << cIdAnalyser.nodeId()
				<< " for hashed FQDN " << fqdn << ": " << (int)networkLatency);
		insertOss << "INSERT INTO nodes_values (timestamp, node_id, value, "
				"attr_id) VALUES ("
				<< epoch << ", "
				<< (int)cIdAnalyser.nodeId() << ", '"
				<< networkLatency << "', "
				<< (int)ATTRIBUTE_NAP_NETWORK_LATENCY << ")";
		sqlStatement = _sqlConnection->createStatement();
		LOG4CXX_TRACE(_logger, "Execute query: " << insertOss.str());
		sqlStatement->executeUpdate(insertOss.str());
		delete sqlStatement;
		break;
	}
	default:
		LOG4CXX_WARN(_logger, "Unknown primitive type: value "
				<< cIdAnalyser.primitive());
	}
}

string MySqlConnector::_stateStr(uint8_t state)
{//FIXME Leave STATE as an integer
	ostringstream ossState;
	switch (state)
	{
	case STATE_BOOTED:
		ossState << "STATE_BOOTED";
		break;
	case STATE_DOWN:
		ossState << "STATE_DOWN";
		break;
	case STATE_UP:
		ossState << "STATE_UP";
		break;
	case STATE_UNKNOWN:
		ossState << "STATE_UNKNOWN";
		break;
	}
	return ossState.str();
}
