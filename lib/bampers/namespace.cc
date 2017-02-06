/*
 * namespace.cc
 *
 *  Created on: Dec 4, 2015
 *      Author: Sebastian Robitzsch <sebastian.robitzsch@interdigital.com>
 *
 * This file is part of BlAckadder Monitoring wraPpER clasS (BAMPERS).
 *
 * BAMPERS is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * BAMPERS is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * BAMPERS. If not, see <http://www.gnu.org/licenses/>.
 */

#include "namespace.hh"

Namespace::Namespace(Blackadder *blackadder)
	: _blackadder(blackadder)
{
	_nodeId = 0;
}

Namespace::~Namespace() {}

void Namespace::addPublishedScopePath(string scopePath)
{
	_mutexPublishedScopePaths.lock();
	_publishedScopePaths.insert((pair<string, bool>(scopePath, false)));
	_mutexPublishedScopePaths.unlock();
}

void Namespace::advertiseInformationItem(string scopePath)
{
	string id, prefixId;
	id = scopePath.substr(scopePath.length() - ID_LEN, ID_LEN);
	prefixId = scopePath.substr(0, scopePath.length() - ID_LEN);
	_blackadder->publish_info(hex_to_chararray(id), hex_to_chararray(prefixId),
			DOMAIN_LOCAL, NULL, 0);
}

Blackadder * Namespace::getBlackadderInstance()
{
	return _blackadder;
}

bool Namespace::getDataFromBuffer(string scopePath, uint8_t *data,
		uint32_t &dataSize)
{
	_mutexBuffer.lock();
	_itBuffer = _buffer.find(scopePath);
	if (_itBuffer == _buffer.end())
	{
		_mutexBuffer.unlock();
		return false;
	}
	dataSize = _itBuffer->second.dataSize;
	memcpy(data, _itBuffer->second.data, dataSize);
	cout << "Obtained " << dataSize << " bytes from buffer";
	free(_itBuffer->second.data);
	_buffer.erase(scopePath);
	_mutexBuffer.unlock();
	return true;
}

string Namespace::getScopePath(uint8_t nodeType,
		INFORMATION_ITEM infoItem)
{
	ostringstream oss;
	oss << setw(ID_LEN) << setfill('0') << (int)NAMESPACE_MONITORING;
	oss << setw(ID_LEN) << setfill('0') << (int)TOPOLOGY_NODES;
	oss << setw(ID_LEN) << setfill('0') << (int)nodeType;
	oss << setw(ID_LEN) << setfill('0') << (int)_nodeId;
	oss << setw(ID_LEN) << setfill('0') << (int)infoItem;
	return oss.str();
}

string Namespace::getScopePath(uint8_t nodeType, uint32_t nodeId,
		INFORMATION_ITEM infoItem)
{
	ostringstream oss;
	oss << setw(ID_LEN) << setfill('0') << (int)NAMESPACE_MONITORING;
	oss << setw(ID_LEN) << setfill('0') << (int)TOPOLOGY_NODES;
	oss << setw(ID_LEN) << setfill('0') << (int)nodeType;
	oss << setw(ID_LEN) << setfill('0') << (int)nodeId;
	oss << setw(ID_LEN) << setfill('0') << (int)infoItem;
	return oss.str();
}

string Namespace::getScopePath(LINK_ID linkId, NODE_ID destinationNodeId,
		LINK_TYPE linkType, INFORMATION_ITEM infoItem)
{
	ostringstream oss;
	oss << setw(ID_LEN) << setfill('0') << (int)NAMESPACE_MONITORING;
	oss << setw(ID_LEN) << setfill('0') << (int)TOPOLOGY_LINKS;
	oss << setw(ID_LEN) << setfill('0') << linkId;
	oss << setw(ID_LEN) << setfill('0') << (int)destinationNodeId;
	oss << setw(ID_LEN) << setfill('0') << (int)linkType;
	oss << setw(ID_LEN) << setfill('0') << (int)infoItem;
	return oss.str();
}

NODE_ID Namespace::nodeId()
{
	return _nodeId;
}

void Namespace::nodeId(NODE_ID nodeId)
{
	_nodeId = nodeId;
	cout << "Node ID set to " << _nodeId << endl;
}

void Namespace::prepareDataToBePublished(string scopePath, uint8_t *data,
		uint32_t dataLength)
{
	// Scope not published
	if (!publishedScopePath(scopePath))
	{
		cout << "Scope path\n" << scopePath << " has not been published. "
				<< _publishedScopePaths.size() << " known scope paths known\n";
		_mutexPublishedScopePaths.lock();
		_addDataToBuffer(scopePath, data, dataLength);
		_publishScopePath(scopePath);
		_mutexPublishedScopePaths.unlock();
		return;
	}
	// No subscriber available
	if (!subscriberAvailable(scopePath))
	{
		cout << "No subscriber available for " << scopePath << " available\n";
		_addDataToBuffer(scopePath, data, dataLength);
		advertiseInformationItem(scopePath);
		return;
	}
	// Scope published, subscribers available. Go off and publish it now
	_blackadder->publish_data(hex_to_chararray(scopePath), DOMAIN_LOCAL, NULL,
			0, data, dataLength);
	 cout << "Data of length " << dataLength << " published under scope path "
			<< scopePath << endl;
}

bool Namespace::publishedScopePath(string scopePath)
{
	_itPublishedScopePaths = _publishedScopePaths.find(scopePath);
	if (_itPublishedScopePaths == _publishedScopePaths.end())
	{
		_mutexPublishedScopePaths.unlock();
		return false;
	}
	_mutexPublishedScopePaths.unlock();
	return true;
}

void Namespace::removePublishedScopePath(string scopePath)
{

	if (!publishedScopePath(scopePath))
	{
		return;
	}
	_mutexPublishedScopePaths.lock();
	_publishedScopePaths.erase(scopePath);
	_mutexPublishedScopePaths.unlock();
}

void Namespace::setSubscriberAvailability(string scopePath, bool state)
{
	if (!publishedScopePath(scopePath))
	{
		return;
	}
	_mutexPublishedScopePaths.lock();
	_itPublishedScopePaths = _publishedScopePaths.find(scopePath);
	(*_itPublishedScopePaths).second = state;
	_mutexPublishedScopePaths.unlock();
	cout << "Subscriber availability for scope path " << scopePath << " set to "
			<< state << endl;
}

bool Namespace::subscriberAvailable(string scopePath)
{
	if (!publishedScopePath(scopePath))
	{
		return false;
	}
	_mutexPublishedScopePaths.lock();
	_itPublishedScopePaths = _publishedScopePaths.find(scopePath);
	// Subscriber available
	if ((*_itPublishedScopePaths).second)
	{
		_mutexPublishedScopePaths.unlock();
		return true;
	}
	// No subscriber available
	_mutexPublishedScopePaths.unlock();
	return false;
}

void Namespace::_addDataToBuffer(string scopePath, uint8_t *data,
		uint32_t dataSize)
{
	_mutexBuffer.lock();
	_itBuffer = _buffer.find(scopePath);
	// Add new entry
	if (_itBuffer == _buffer.end())
	{
		data_t itemStruct;
		itemStruct.data = (uint8_t *)malloc(dataSize);
		memcpy(itemStruct.data, data, dataSize);
		itemStruct.dataSize = dataSize;
		_buffer.insert(pair<string, data_t>(scopePath, itemStruct));
	}
	// Update existing one
	else
	{
		if (realloc((*_itBuffer).second.data, dataSize) == NULL)
		{//could not be allocated. Try to free it and malloc again
			free((*_itBuffer).second.data);
			(*_itBuffer).second.data = (uint8_t *)malloc(dataSize);
		}
		memcpy((*_itBuffer).second.data, data, dataSize);
		(*_itBuffer).second.dataSize = dataSize;
	}
	_mutexBuffer.unlock();
}

void Namespace::_addPublishedScopePath(string scopePath)
{
	_publishedScopePaths.insert((pair<string, bool>(scopePath, false)));
}

void Namespace::_publishScopePath(string scopePath)
{
	string id, prefixId;
	size_t lengthIterator = 0;
	ostringstream ossId, ossPrefixId;
	ossPrefixId.str("");
	if ((scopePath.length() % ID_LEN) != 0)
	{
		cout << "ERROR: Scope path\n" << scopePath << " " << scopePath.length()
				<< " is not multiple of " << ID_LEN << endl;
		return;
	}
	while (lengthIterator < scopePath.length())
	{
		// Get ID
		ossId.str("");
		for (size_t i = lengthIterator; i < (lengthIterator + ID_LEN); i++)
		{
			ossId << scopePath[i];
		}
		// Publish ID under father scope
		// check if information item has been reached
		if ((lengthIterator + ID_LEN) == scopePath.length())
		{
			_blackadder->publish_info(hex_to_chararray(ossId.str()),
					hex_to_chararray(ossPrefixId.str()), DOMAIN_LOCAL, NULL, 0);
			cout << "Info item " << ossId.str() << " advertised under father "
					<< "scope " << ossPrefixId.str() << endl;
		}
		else
		{
			_blackadder->publish_scope(hex_to_chararray(ossId.str()),
					hex_to_chararray(ossPrefixId.str()), DOMAIN_LOCAL, NULL, 0);
			cout << "Scope ID " << ossId.str() << " published under father scope "
					<< ossPrefixId.str() << endl;
		}
		// Prepare ID and PrefixID for next scope level
		ossPrefixId << ossId.str();
		lengthIterator += ID_LEN;
	}
	_addPublishedScopePath(scopePath);
}
