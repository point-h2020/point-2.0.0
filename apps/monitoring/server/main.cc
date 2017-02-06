/*
 * main.cc
 *
 *  Created on: 8 Feb 2016
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

#include <blackadder.hpp>
#include <boost/program_options.hpp>
#include <libconfig.h++>
#include <log4cxx/basicconfigurator.h>
#include <log4cxx/consoleappender.h>
#include <log4cxx/helpers/pool.h>
#include <log4cxx/fileappender.h>
#include <log4cxx/logger.h>
#include <log4cxx/simplelayout.h>
#include <signal.h>

#include "enumerations.hh"
#include "filesystemchecker.hh"
#include "messagestack.hh"
#include "mysqlconnector.hh"
#include "scopes.hh"
#include "types/typedef.hh"

using namespace libconfig;
namespace po = boost::program_options;
using namespace std;
// A helper log4cxx function to simplify the main part.
template<class T>
ostream& operator<<(ostream& os, const vector<T>& v)
{
    copy(v.begin(), v.end(), ostream_iterator<T>(os, " "));
    return os;
}
log4cxx::ConsoleAppender * consoleAppender =
		new log4cxx::ConsoleAppender(log4cxx::LayoutPtr(
				new log4cxx::SimpleLayout()));
log4cxx::LoggerPtr logger = log4cxx::Logger::getLogger("logger");
Blackadder *ba;
IcnId rootScope;
Scopes scopes(logger);
boost::thread *mysqlConnectorThreadPointer;
boost::thread *filesystemCheckerThreadPointer;
/*!
 * \brief Callback if Ctrl+C was hit
 */
void sigfun(int sig) {
	(void) signal(sig, SIG_DFL);
	LOG4CXX_INFO(logger, "Termination requested ... cleaning up");
	mysqlConnectorThreadPointer->interrupt();
	filesystemCheckerThreadPointer->interrupt();
	ba->unpublish_scope(rootScope.binId(), rootScope.binPrefixId(),
			DOMAIN_LOCAL, NULL, 0);
	scopes.erase(rootScope);
	ba->disconnect();
	delete ba;
	LOG4CXX_INFO(logger, "Bye for now");
	exit(0);
}
// Main function
int main(int ac, char* av[])
{
	string serverIpAddress;
	string serverPort;
	string username;
	string password;
	string database;
	string visApiDirectory;
	// data points
	data_points_t dataPoints;
	log4cxx::BasicConfigurator::configure(
			log4cxx::AppenderPtr(consoleAppender));
	ostringstream ossDescription;
	ossDescription << "\nMOnitOring SErver (MOOSE)\n\nAuthor:\tSebastian"
			" Robitzsch, InterDigital Europe, Ltd.\n\t"
			"<sebastian.robitzsch@interdigital.com>\n\nAllowed options";
	po::options_description desc(ossDescription.str().c_str());
	desc.add_options()
	("configuration,c", po::value< string >(), "libconfig-based configuration "
			"file. Template in /usr/doc/share/moose. Default read from "
			"/etc/moose")
	("debug,d", po::value< string >(), "Enable verbosity (ERROR|INFO|DEBUG|"
			"TRACE)")
	("help,h", "Print this help message");
	po::positional_options_description p;
	po::variables_map vm;
	po::store(
			po::command_line_parser(ac, av).options(desc).positional(p).run(),
			vm);
	po::notify(vm);
	// Print help and exit
	if (vm.count("help")) {
		cout << desc;
		return EXIT_SUCCESS;
	}
	MessageStack messageStack(logger);
	// Reading verbose level
	if (vm.count("debug"))
	{
		if (vm["debug"].as< string >() == "FATAL")
		{
			log4cxx::Logger::getRootLogger()->setLevel(
					log4cxx::Level::getFatal());
			LOG4CXX_ERROR(logger, "Verbose level set to FATAL");
		}
		else if (vm["debug"].as< string >() == "ERROR")
		{
			log4cxx::Logger::getRootLogger()->setLevel(
					log4cxx::Level::getError());
			LOG4CXX_ERROR(logger, "Verbose level set to ERROR");
		}
		else if (vm["debug"].as< string >() == "INFO")
		{
			log4cxx::Logger::getRootLogger()->setLevel(
					log4cxx::Level::getInfo());
			LOG4CXX_INFO(logger, "Verbose level set to INFO");
		}
		else if (vm["debug"].as< string >() == "DEBUG")
		{
			log4cxx::Logger::getRootLogger()->setLevel(
					log4cxx::Level::getDebug());
			LOG4CXX_DEBUG(logger, "Verbose level set to DEBUG");
		}
		else if (vm["debug"].as< string >() == "TRACE")
		{
			log4cxx::Logger::getRootLogger()->setLevel(
					log4cxx::Level::getTrace());
			LOG4CXX_TRACE(logger, "Verbose level set to TRACE");
		}
		else
		{
			LOG4CXX_ERROR(logger, __FILE__ << "::" << __FUNCTION__ << "() "
					<< "Unknown debug mode");
			return EXIT_FAILURE;
		}
	}
	else
	{
		log4cxx::Logger::getRootLogger()->setLevel(
				log4cxx::Level::getFatal());
	}
	if (getuid() != 0)
	{
		cout << "monitoringserver must be started with root (sudo) privileges\n";
		return EXIT_FAILURE;
	}
	// Determine CFG location NAP config
	string configurationFile;
	if (vm.count("configuration"))
	{
		configurationFile.assign(vm["configuration"].as<string>().c_str());
	}
	else
	{
		configurationFile.assign("/etc/moose/moose.cfg");
	}
	// Read CFG file
	Config cfg;
	try
	{
		cfg.readFile(configurationFile.c_str());
	}
	catch(const FileIOException &fioex)
	{
		LOG4CXX_ERROR(logger,"Cannot read " << configurationFile);
		return EXIT_FAILURE;
	}
	catch(const ParseException &pex)
	{
		LOG4CXX_ERROR(logger, "Parse error in "	<< configurationFile);
		return EXIT_FAILURE;
	}
	// Reading root
	const Setting& root = cfg.getRoot();
	try
	{
		LOG4CXX_INFO(logger, "Reading configuration file "
				<< configurationFile);
		const Setting &napConfig = root["mooseConfig"];
		if (!napConfig.lookupValue("ipAddress", serverIpAddress))
		{
			LOG4CXX_FATAL(logger, "'ipAddress' could not be read");
			return EXIT_FAILURE;
		}
		if (!napConfig.lookupValue("port", serverPort))
		{
			LOG4CXX_FATAL(logger, "'port' could not be read");
			return EXIT_FAILURE;
		}
		if (!napConfig.lookupValue("username", username))
		{
			LOG4CXX_FATAL(logger, "'username' could not be read");
			return EXIT_FAILURE;
		}
		if (!napConfig.lookupValue("password", password))
		{
			LOG4CXX_FATAL(logger, "'password' could not be read");
			return EXIT_FAILURE;
		}
		if (!napConfig.lookupValue("database", database))
		{
			LOG4CXX_FATAL(logger, "'database' could not be read");
			return EXIT_FAILURE;
		}
		if (!napConfig.lookupValue("visApiDir", visApiDirectory))
		{
			LOG4CXX_FATAL(logger, "Vis directory to enable/disable datapoints "
					<< "at run-time is missing");
			return EXIT_FAILURE;
		}
	}
	catch(const SettingNotFoundException &nfex)
	{
		LOG4CXX_FATAL(logger, "Setting not found in "
				<< vm["configuration"].as<string>());
		return false;
	}
	ba = Blackadder::Instance(true);
	// Subscribing to monitoring namespace
	rootScope.rootNamespace(NAMESPACE_MONITORING);
	scopes.add(rootScope);
	// publish root scope
	ba->publish_scope(rootScope.binId(), rootScope.binPrefixId(), DOMAIN_LOCAL,
			NULL, 0);
	scopes.setPublicationState(rootScope, true);
	//find data points, publish the respective scopes and subscribe to them
	const Setting& cfgDataPoints = root["mooseConfig"]["dataPoints"];
	try
	{
		if (cfgDataPoints.lookupValue("topology", dataPoints.topology))
		{
			if (dataPoints.topology)
			{
				LOG4CXX_DEBUG(logger, "Reading the entire topology has been "
						"enabled");
			}
			else
			{
				LOG4CXX_TRACE(logger, "Topology variable declared in "
						<< configurationFile << " but set to false");
			}
		}
		if (cfgDataPoints.lookupValue("networkLatencyPerFqdn",
				dataPoints.networkLatencyPerFqdn))
		{
			if (dataPoints.networkLatencyPerFqdn)
			{
				dataPoints.statistics = true;
				LOG4CXX_DEBUG(logger, "Data point 'networkLatencyPerFqdn'"
						"enabled");
			}
			else
			{
				LOG4CXX_TRACE(logger, "Data point 'networkLatencyPerFqdn' "
						"declared in " << configurationFile << " but set to "
								"false");
			}
		}
		if (cfgDataPoints.lookupValue("cmcGroupSize", dataPoints.cmcGroupSize))
		{
			if (dataPoints.cmcGroupSize)
			{
				dataPoints.statistics = true;
				LOG4CXX_DEBUG(logger, "Data point 'cmcGroupSize' enabled");
			}
			else
			{
				LOG4CXX_TRACE(logger, "Data point 'cmcGroupSize' declared in "
						<< configurationFile << " but set to false");
			}
		}
		if (cfgDataPoints.lookupValue("txBytes", dataPoints.txBytes))
		{
			if (dataPoints.txBytes)
			{
				dataPoints.statistics = true;
				LOG4CXX_DEBUG(logger, "Data point 'txBytes' enabled");
			}
			else
			{
				LOG4CXX_TRACE(logger, "Data point 'txBytes' declared in "
						<< configurationFile << " but set to false");
			}
		}
		if (cfgDataPoints.lookupValue("rxBytes", dataPoints.rxBytes))
		{
			if (dataPoints.rxBytes)
			{
				dataPoints.statistics = true;
				LOG4CXX_DEBUG(logger, "Data point 'rxBytes' enabled");
			}
			else
			{
				LOG4CXX_TRACE(logger, "Data point 'rxBytes' declared in "
						<< configurationFile << " but set to false");
			}
		}
	}
	catch(const SettingNotFoundException &nfex)
	{
		LOG4CXX_FATAL(logger, "Data point not found in "
				<< vm["configuration"].as<string>());
	}
	// Database connector
	MySqlConnector mySqlConnector(logger, serverIpAddress, serverPort, username,
			password, database, messageStack);
	boost::thread mySqlConnectorThread(mySqlConnector);
	mysqlConnectorThreadPointer = &mySqlConnectorThread;
	//Visualisation synchroniser
	FilesystemChecker filesystemChecker(logger, visApiDirectory, scopes, ba);
	boost::thread filesystemCheckerThread(filesystemChecker);
	filesystemCheckerThreadPointer = &filesystemCheckerThread;
	// Publish management root scope and advertise information item trigger
	IcnId managementIcnId;
	managementIcnId.rootNamespace(NAMESPACE_MANAGEMENT);
	managementIcnId.append(MANAGEMENT_II_MONITORING);
	LOG4CXX_DEBUG(logger, "Publishing management root scope "
			<< managementIcnId.rootScopeId());
	ba->publish_scope(managementIcnId.binRootScopeId(),
			managementIcnId.binEmpty(), DOMAIN_LOCAL, NULL, 0);
	LOG4CXX_DEBUG(logger, "Advertise monitoring information item "
			<< managementIcnId.id() << " under father scope path "
			<< managementIcnId.prefixId());
	ba->publish_info(managementIcnId.binId(), managementIcnId.binPrefixId(),
			DOMAIN_LOCAL, NULL, 0);
	// Publish root monitoring scope and subscribe to it
	IcnId monitoringIcnId;
	monitoringIcnId.rootNamespace(NAMESPACE_MONITORING);
	LOG4CXX_DEBUG(logger, "Publishing monitoring root scope "
				<< monitoringIcnId.rootScopeId());
	ba->publish_scope(monitoringIcnId.binRootScopeId(),
			monitoringIcnId.binEmpty(), DOMAIN_LOCAL, NULL, 0);
	ba->subscribe_scope(monitoringIcnId.binId(), monitoringIcnId.binEmpty(),
			DOMAIN_LOCAL, NULL, 0);
	LOG4CXX_DEBUG(logger, "Subscribed to monitoring root "
			<< monitoringIcnId.print());
	(void) signal(SIGINT, sigfun);
	IcnId icnId;
	string eventId;
	while (true)
	{
		Event ev;
		ba->getEvent(ev);
		eventId = chararray_to_hex(ev.id);
		icnId = eventId;
		switch (ev.type)
		{
			case SCOPE_PUBLISHED:
			{
				LOG4CXX_DEBUG(logger, "SCOPE_PUBLISHED received for scope path "
						<< icnId.print());
				scopes.add(icnId);
				scopes.setPublicationState(icnId, true);
				CIdAnalyser icnIdAnalyser(icnId);
				// nodes branch
				if (dataPoints.statistics && icnIdAnalyser.nodesScope())
				{
					/* /monitoring/topologyNodes/nodeType/nodeId/infItem
					 * /		1 /			  2 /	   3 /	  4 /	  5
					 */
					if (icnId.scopeLevels() > 3)
					{
						// NAP related data points
						if (icnIdAnalyser.nodeType() == NODE_TYPE_NAP ||
									icnIdAnalyser.nodeType() == NODE_TYPE_GW)
						{
							if (dataPoints.cmcGroupSize)
							{
								IcnId cid;
								cid = icnId;
								cid.append(II_CMC_GROUP_SIZE);
								ba->subscribe_info(cid.binId(),
										cid.binPrefixId(), DOMAIN_LOCAL, NULL,
										0);
								LOG4CXX_DEBUG(logger, "Subscribed to II 'CMC "
										"group size': " << cid.print());
							}
							if (dataPoints.networkLatencyPerFqdn)
							{
								IcnId cid;
								cid = icnId;
								cid.append(II_NETWORK_LATENCY_PER_FQDN);
								ba->subscribe_info(cid.binId(),
										cid.binPrefixId(), DOMAIN_LOCAL, NULL,
										0);
								LOG4CXX_DEBUG(logger, "Subscribed to II "
										"'Latency per FQDN': " << cid.print());
							}
						}
					}
					// less or equal to 3 scope levels > simply subscribe
					else
					{
						ba->subscribe_scope(icnId.binId(), icnId.binPrefixId(),
								DOMAIN_LOCAL, NULL, 0);
						LOG4CXX_DEBUG(logger, "Subscribed to " << icnId.print()
								<< " (TOPOLOGY_NODES branch)");
					}

				}
				// Topology (links) branch -> subscribe
				else if (dataPoints.topology && icnIdAnalyser.linksScope())
				{
					ba->subscribe_scope(icnId.binId(), icnId.binPrefixId(),
							DOMAIN_LOCAL, NULL, 0);
					LOG4CXX_DEBUG(logger, "Subscribed to " << icnId.print()
							<< " (TOPOLOGY_LINKS branch)");
				}
				// Topology (nodes) branch -> if the nodeType and NID
				// has been already published subscribe straight to name
				if (dataPoints.topology && icnIdAnalyser.nodesScope())
				{
					if (icnId.scopeLevels() < 4)
					{
						LOG4CXX_DEBUG(logger, "Subscribed to scope "
								<< icnId.print() << " (TOPOLOGY_NODES branch)");
						ba->subscribe_scope(icnId.binId(), icnId.binPrefixId(),
								DOMAIN_LOCAL, NULL, 0);
					}
					else
					{// subscribe straight to information item 'role'
						IcnId cid;
						cid = icnId;
						icnId.append(II_NAME);
						LOG4CXX_DEBUG(logger, "Subscribed to CID "
								<< icnId.print() << " (TOPOLOGY_NODES branch)");
						ba->subscribe_info(icnId.binId(), icnId.binPrefixId(),
								DOMAIN_LOCAL, NULL, 0);
					}
				}
				break;
			}
			case SCOPE_UNPUBLISHED:
				LOG4CXX_DEBUG(logger, "SCOPE_UNPUBLISHED: "
						<< chararray_to_hex(ev.id));
				break;
			case START_PUBLISH:
			{
				LOG4CXX_DEBUG(logger, "START_PUBLISH received under scope path "
						<< icnId.print());
				BoostrappingMessageTypes event = BOOTSTRAP_SERVER_UP;
				ba->publish_data(icnId.binIcnId(), DOMAIN_LOCAL, NULL, 0,
						&event, sizeof(uint8_t));
				LOG4CXX_DEBUG(logger, "Start trigger published using CID "
						<< icnId.print());
				break;
			}
			case STOP_PUBLISH:
				LOG4CXX_DEBUG(logger, "STOP_PUBLISH: " << icnId.print());
				break;
			case PUBLISHED_DATA:
				LOG4CXX_TRACE(logger, "PUBLISHED_DATA: " << icnId.print());
				scopes.add(icnId);
				messageStack.write(icnId, (uint8_t *)ev.data, ev.data_len);
				break;
		}
	}
	mysqlConnectorThreadPointer->interrupt();
	ba->unpublish_scope(rootScope.binId(), rootScope.binPrefixId(),
			DOMAIN_LOCAL, NULL, 0);
	scopes.erase(rootScope);
	ba->disconnect();
	delete ba;
	return 0;
}
