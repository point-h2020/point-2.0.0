/**
 * main.cc
 *
 * Created on 19 April 2015
 * 		Author: Sebastian Robitzsch <sebastian.robitzsch@interdigital.com>
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

#include <blackadder.hpp>
#include <boost/asio/io_service.hpp>
#include <boost/asio/signal_set.hpp>
#include <boost/program_options.hpp>
#include <log4cxx/logger.h>
#include "log4cxx/basicconfigurator.h"
#include <log4cxx/propertyconfigurator.h>
#include <net/ethernet.h>
#include <moly/moly.hh>
#include <pcap.h>
#include <signal.h>
#include <stdlib.h>
#include <string>
#include <thread>

#include <api/napsa.hh>
#include <configuration.hh>
#include <monitoring/collector.hh>
#include <monitoring/statistics.hh>
#include <enumerations.hh>
#include <demux/demux.hh>
#include <icn.hh>
#include <ipsocket.hh>
#include <namespaces/namespaces.hh>
#include <proxies/http/httpproxy.hh>
#include <types/ipaddress.hh>

#ifdef DMALLOC
#include "dmalloc.h"
#endif

using namespace api::napsa;
using namespace configuration;
using namespace demux;
using namespace icn;
using namespace log4cxx;
using namespace monitoring::collector;
using namespace monitoring::statistics;
using namespace namespaces;
using namespace namespaces::ip;
using namespace namespaces::http;
using namespace proxies::http;
using namespace std;

namespace po = boost::program_options;
// A helper log4cxx function to simplify the main part.
template<class T>
ostream& operator<<(ostream& os, const vector<T>& v)
{
    copy(v.begin(), v.end(), ostream_iterator<T>(os, " "));
    return os;
}
LoggerPtr logger(Logger::getLogger("NAP"));
/*!
 * \brief Capturing user interactions
 *
 * This function is called once the user issues Ctrl+C
 *
 * \param sig The signal captured
 */
void sigproc(int sig);

void quitproc(int sig);

void shutdown();

Blackadder *icnCore;
Namespaces *namespacesPointer;
//HttpProxy *httpProxyPointer;
Configuration *configurationPointer;
/*std::thread *collectorThreadPointer;
std::thread *demuxThreadPointer;
std::thread *vdemuxThreadPointer;
std::thread *napSaThreadPointer;
std::thread *httpProxyThreadPointer;*/
std::vector<std::thread> mainThreads;

int main(int ac, char* av[]) {
	bool baUserSpace;
	string device;
	po::options_description desc("\nNetwork Attachment Point (NAP)\n"
			"Author:\t Sebastian Robitzsch "
			"<sebastian.robitzsch@interdigital.com>\n\nAllowed options");
	desc.add_options()
	("configuration,c", po::value< string >(), "libconfig-based "
			"configuration file for the NAP. If not specified the NAP assumes "
			"it is located under /etc/nap/nap.cfg")
	//("daemon,d", "Run the NAP as a daemon")
	("help,h", "Print this help message")
	("kernel,k", "Tell NAP that Blackadder runs in kernel space")
	("version,v", "Print the version number of this software.")
	;
	po::positional_options_description p;
	po::variables_map vm;
	po::store(po::command_line_parser(ac, av).options(desc).positional(p).run(),
			vm);
	po::notify(vm);
	if (vm.count("help")) {
		cout << desc;
		return EXIT_SUCCESS;
	}
	if (vm.count("version"))
	{
		cout << "This NAP runs version 3.0.0\n";
		return EXIT_SUCCESS;
	}
	if (getuid() != 0)
	{
		if (logger->getEffectiveLevel()->toInt() > LOG4CXX_LEVELS_ERROR)
			cout << "The NAP must run with root (sudo) privileges\n";
		else
			LOG4CXX_ERROR(logger, "The NAP must run with root (sudo) "
					"privileges");
		return EXIT_FAILURE;
	}
	PropertyConfigurator::configure("/etc/nap/nap.l4j");
	// Reading whether Blackadder runs in user or kernel space
	if (vm.count("kernel"))
		baUserSpace = false;
	else
		baUserSpace = true;
	// Reading NAP config
	LOG4CXX_DEBUG(logger, "Reading NAP configuration file");
	Statistics statistics;
	Configuration configuration;
	configurationPointer = &configuration;
	if (vm.count("configuration"))
	{
		if (!configuration.parse(vm["configuration"].as< string >()))
		{
			LOG4CXX_FATAL(logger, "NAP config file could not be read");
			exit(CONFIGURATION_FILE_PARSER);
		}
	}
	else
	{
		if (!configuration.parse("/etc/nap/nap.cfg"))
		{
			LOG4CXX_FATAL(logger, "NAP config file could not be read");
			exit(CONFIGURATION_FILE);
		}
	}
	// Connecting to ICN core
	icnCore = Blackadder::Instance(baUserSpace);
	// Create TCP client instance
	boost::mutex icnCoreMutex;/*!< Shared mutex to avoid parallel calling of BA API */
	Transport transport(icnCore, configuration, icnCoreMutex, statistics);
	// Instantiate all namespace classes
	Namespaces namespaces(icnCore, icnCoreMutex, configuration, transport,
			statistics);
	namespacesPointer = &namespaces;
	// Initialise the ICN namespaces (subscribing to respective scopes, etc)
	namespaces.initialise();
	// Start ICN handler
	Icn icn(icnCore, configuration, namespaces, transport);
	mainThreads.push_back(std::thread(icn));
	// Start demux
	LOG4CXX_DEBUG(logger, "Starting Demux thread");
	Demux demux(namespaces, configuration, statistics);
	mainThreads.push_back(std::thread(demux));
	// NAP SA Listener
	if (configuration.httpHandler() && configuration.surrogacy())
	{
		LOG4CXX_DEBUG(logger, "Starting NAP-SA listener thread");
		NapSa napSa(namespaces, statistics);
		mainThreads.push_back(std::thread(napSa));
	}
	// HTTP proxy
	if (configuration.httpHandler())
	{
		LOG4CXX_DEBUG(logger, "Starting HTTP proxy thread");
		HttpProxy httpProxy(configuration, namespaces, statistics);
		mainThreads.push_back(std::thread(httpProxy));
	}
	// Monitoring
	if (configuration.molyInterval() > 0)
	{
		LOG4CXX_DEBUG(logger, "Starting Collector thread (stats + MOLY app)");
		Collector collector(configuration, statistics);
		mainThreads.push_back(std::thread(collector));
	}
	else
	{
		LOG4CXX_INFO(logger, "Monitoring will be disabled, as MOLY interval is "
				"either not set in nap.cfg or it is 0: "
				<< configuration.molyInterval());
	}
	// register all possible signals
	signal(SIGINT, sigproc);
	signal(SIGABRT, sigproc);
	signal(SIGTERM, sigproc);
	signal(SIGQUIT, sigproc);
	pause();
	return EXIT_SUCCESS;
}

void quitproc(int sig)
{
	LOG4CXX_INFO(logger, "Termination requested via signal " << sig);
	shutdown();
	exit(0);
}

void sigproc(int sig)
{
	switch (sig)
	{
	case SIGTERM:
		LOG4CXX_INFO(logger, "Termination properly requested via SIGTERM");
		shutdown();
		break;
	case SIGINT:
		LOG4CXX_INFO(logger, "Termination properly requested via SIGINT");
		shutdown();
		break;
	case SIGKILL:
		LOG4CXX_WARN(logger, "Don't SIGKILL. Think about the threads");
		break;
	default:
		LOG4CXX_INFO(logger, "Unknown signal received: " << sig << ". Just "
				"stopping. Consider using SIGTERM (-15) instead!");
	}
	exit(0);
}

void shutdown()
{
	namespacesPointer->uninitialise();

	if (configurationPointer->httpHandler())
	{
		ostringstream iptablesRule;
		iptablesRule << "iptables -D PREROUTING -t nat -p tcp -i "
				<< configurationPointer->networkDevice() << " --dport 80 -j "
				"REDIRECT --to-port " << configurationPointer->httpProxyPort();
		if (system(iptablesRule.str().c_str()) != -1)
		{
			LOG4CXX_DEBUG(logger, "iptables rule for transparent HTTP proxy "
					"removed")
		}
	}

	for (auto it = mainThreads.begin(); it != mainThreads.end(); it++)
	{
		it->detach();
	}

	LOG4CXX_INFO(logger, "Disconnecting from Blackadder");
	icnCore->disconnect();
	delete icnCore;
	LOG4CXX_INFO(logger, "Exiting ... bye, bye nappers");
}
