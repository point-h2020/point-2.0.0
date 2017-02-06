/*
 * icnproxythreadcleaner.cc
 *
 *  Created on: 5 Jan 2017
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

#include <chrono>

#include "icnproxythreadcleaner.hh"

using namespace log4cxx;
using namespace icn;

LoggerPtr IcnProxyThreadCleaner::logger(Logger::getLogger("icn"));

IcnProxyThreadCleaner::IcnProxyThreadCleaner(std::vector<std::thread> &threads,
		std::mutex &mutex)
	: _threads(threads),
	  _mutex(mutex)
{}

IcnProxyThreadCleaner::~IcnProxyThreadCleaner() {}

void IcnProxyThreadCleaner::operator ()()
{
	LOG4CXX_DEBUG(logger, "Starting proxy thread cleaner");

	while (true)
	{
		_mutex.lock();

		if (!_threads.empty())
		{
			for (auto it = _threads.begin(); it != _threads.end(); it++)
			{
				if (!it->joinable())
				{
					_threads.erase(it);
					LOG4CXX_TRACE(logger, "Thread cleaned");
					break;
				}
			}
		}

		_mutex.unlock();
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}
