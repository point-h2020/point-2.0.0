/*
 * icnproxythreadcleaner.hh
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

#ifndef NAP_ICNPROXYTHREADCLEANER_HH_
#define NAP_ICNPROXYTHREADCLEANER_HH_

#include <log4cxx/logger.h>
#include <mutex>
#include <thread>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

namespace icn
{
class IcnProxyThreadCleaner {
	static log4cxx::LoggerPtr logger;
public:
	IcnProxyThreadCleaner(std::vector<std::thread> &threads, std::mutex &mutex);
	~IcnProxyThreadCleaner();
	/*!
	 * \brief Functor to run this method in a boost thread
	 */
	void operator()();
private:
	std::vector<std::thread> &_threads;
	std::mutex &_mutex;
};
}
#endif /* NAP_ICNPROXYTHREADCLEANER_HH_ */
