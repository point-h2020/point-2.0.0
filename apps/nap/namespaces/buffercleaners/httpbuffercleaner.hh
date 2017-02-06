/*
 * httpbuffercleaner.hh
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

#ifndef NAMESPACES_BUFFERCLEANERS_HTTPBUFFERCLEANER_HH_
#define NAMESPACES_BUFFERCLEANERS_HTTPBUFFERCLEANER_HH_

#include <boost/date_time.hpp>
#include <boost/thread/mutex.hpp>
#include <log4cxx/logger.h>

#include <configuration.hh>
#include <namespaces/httptypedef.hh>
#include <types/icnid.hh>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

using namespace configuration;
using namespace log4cxx;

namespace cleaners {

namespace httpbuffer {
/*!
 * \brief HTTP buffer cleaner
 */
class HttpBufferCleaner {
	static LoggerPtr logger;
public:
	/*!
	 * \brief Constructor
	 */
	HttpBufferCleaner(void *buffer, void *bufferMutex,
			Configuration &configuration);
	/*!
	 * \brief Destructor
	 */
	~HttpBufferCleaner();
	/*!
	 * \brief Functor to place this into a thread
	 */
	void operator()();
private:
	Configuration &_configuration;
	packet_buffer_requests_t *_buffer; /*!< Buffer for
	HTTP messages to be published map<hashedCId, hashediSubCId, PACKET> */
	packet_buffer_requests_t::iterator _bufferIt;/*!<
	Iterator for _packetBuffer map */
	boost::mutex *_mutex;/*!< Reference to IP packet buffer mutex */
};

} /* namespace httpbuffer */

} /* namespace cleaners */

#endif /* NAMESPACES_BUFFERCLEANERS_HTTPBUFFERCLEANER_HH_ */
