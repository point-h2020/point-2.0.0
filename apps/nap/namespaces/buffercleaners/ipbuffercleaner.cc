/*
 * ipbuffercleaner.cc
 *
 *  Created on: 7 Jul 2016
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

#include "ipbuffercleaner.hh"

using namespace cleaners::ipbuffer;
using namespace log4cxx;

LoggerPtr IpBufferCleaner::logger(Logger::getLogger("cleaners.ipbuffer"));

IpBufferCleaner::IpBufferCleaner(void *buffer, void *bufferMutex,
		Configuration &configuration)
	: _configuration(configuration)
{
	_mutex = (boost::mutex *)bufferMutex;
	// Ugly casting from void into unordered_map. namespace namespaces::ip
	// prevents to pass reference or pointer into here
	_buffer = (packet_buffer_t *)buffer;
}

IpBufferCleaner::~IpBufferCleaner() {}

void IpBufferCleaner::operator()()
{
	LOG4CXX_DEBUG(logger, "Starting IP buffer cleaner with interval of "
			<< _configuration.bufferCleanerInterval() << "s");
	boost::posix_time::ptime currentTime;
	boost::posix_time::time_duration packetAge;
	while (true)
	{
		_mutex->lock();
		if (!_buffer->empty())
		{
			LOG4CXX_TRACE(logger, "Start checking IP buffer with "
					<< _buffer->size() << " packets");
		}
		currentTime = boost::posix_time::microsec_clock::local_time();
		_bufferIt = _buffer->begin();
		while (_bufferIt != _buffer->end())
		{
			packetAge = currentTime - _bufferIt->second.second.timestamp;
			if ((uint32_t)packetAge.seconds() >
					_configuration.bufferCleanerInterval())
			{
				LOG4CXX_DEBUG(logger, "Packet of length "
						<< _bufferIt->second.second.packetSize << " to be "
						"published under " << _bufferIt->second.first.print()
						<< " deleted from IP buffer");
				free(_bufferIt->second.second.packet);
				_buffer->erase(_bufferIt);
				// Now start from scratch
				_bufferIt = _buffer->begin();
			}
			else
			{
				LOG4CXX_TRACE(logger, "Packet of length "
						<< _bufferIt->second.second.packetSize << " to be "
						"published under " << _bufferIt->second.first.print()
						<< " is only " << packetAge.seconds() << "s old. Thus, "
						"it doesn't have to be deleted");
				_bufferIt++;
			}
		}
		_mutex->unlock();
		sleep(_configuration.bufferCleanerInterval());
	}
}
