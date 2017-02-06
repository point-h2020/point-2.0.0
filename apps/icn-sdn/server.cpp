/*
 * Copyright (C) 2016  George Petropoulos
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License version
 * 3 as published by the Free Software Foundation.
 *
 * See LICENSE and COPYING for more details.
 */


#include <iostream>
#include "tm_resource.h"
#include "resource_manager.h"
#include <boost/bind.hpp>
#include <boost/asio.hpp>
#include <boost/thread.hpp>
#include <boost/shared_ptr.hpp>
#include <boost/enable_shared_from_this.hpp>

#include "messages/asio_rw.h"
#include "messages/messages.pb.h"
#include "messages/protobuf_rw.h"

using boost::asio::ip::tcp;
namespace posix = boost::asio::posix;
using namespace std;
using namespace eu::point::tmsdn::impl;

/**@brief The server class, implementing the server side of the TM-SDN interface,
 * and also listening for bootstrapping protocol messages.
 *
 * @author George Petropoulos
 * @version cycle-2
 *
 */
class server {
public:
	/**@brief The constructor with arguments.
	 *
	 */
	server(boost::asio::io_service& io_service, short port, Blackadder *ba,
			ResourceManager* rm) :
			io_service_(io_service), rm_(rm), ba_(ba), acceptor_(io_service,
					tcp::endpoint(tcp::v4(), port)) {

		cout << "Starting server at port " << port << endl;

		string prefix_id = "FFFFFFFFFFFFFFFC";
		string bin_prefix_id = hex_to_chararray(prefix_id);
		string id;
		string bin_id = hex_to_chararray(id);
		//ba_->publish_scope(bin_prefix_id, "", NODE_LOCAL, NULL, 0);

	}

	/**@brief The method which listens for incoming TM-SDN messages and acts accordingly.
	 *
	 * It accepts incoming messages, and checks their type.
	 * If it is a traffic monitoring message, it just prints the received statistics.
	 * If it is a link status message, it prints the received status.
	 * If it is a resource request message, it then requests by the resource manager for
	 * unique resources and responds with the resource offer.
	 *
	 */
	void start_accept() {
		while (true) {
			tcp::socket socket_(io_service_);
			acceptor_.accept(socket_);
			AsioInputStream<tcp::socket> ais(socket_);
			CopyingInputStreamAdaptor cis_adp(&ais);
			TmSdnMessage message;
			bool read = google::protobuf::io::readDelimitedFrom(&cis_adp,
					&message);

			if (read) {
				switch (message.type()) {
				case TmSdnMessage::TM: {
					std::cout << "Received traffic monitoring message. "
							<< std::endl;
					const TmSdnMessage::TrafficMonitoringMessage &tm =
							message.trafficmonitoringmessage();
					std::cout << tm.nodeid1() << ", " << tm.nodeid2()
							<< ": [packets rec/tra] " << tm.packetsreceived()
							<< "/" << tm.packetstransmitted()
							<< ", [bytes rec/tra] " << tm.bytesreceived() << "/"
							<< tm.bytestransmitted() << std::endl;

					break;
				}
				case TmSdnMessage::LS: {
					std::cout << "Received link status message. " << std::endl;
					const TmSdnMessage::LinkStatusMessage &ls =
							message.linkstatusmessage();
					std::cout << ls.nodeid1() << ", " << ls.nodeid2()
							<< ": [type] " << ls.lsmtype() << std::endl;

					switch (ls.lsmtype()) {
					case TmSdnMessage::LinkStatusMessage::ADD: {
						std::cout << "Received LSM ADD message. " << std::endl;
						//TODO

						break;
					}
					case TmSdnMessage::LinkStatusMessage::RMV: {
						std::cout << "Received LSM RMV message. " << std::endl;
						//TODO

						break;
					}
					default: {
						std::cout << "Invalid LSM message type. " << std::endl;
						//TODO

						break;
					}
					}

					break;
				}
				case TmSdnMessage::RR: {
					std::cout << "Received resource request message. "
							<< std::endl;
					AsioOutputStream<boost::asio::ip::tcp::socket> aos(socket_);
					CopyingOutputStreamAdaptor cos_adp(&aos);
					TmSdnMessage offer;
					offer.set_type(TmSdnMessage::RO);
					TmSdnMessage::ResourceOfferMessage *resource_offer(
							offer.mutable_resourceoffermessage());

					std::cout << "Size = "
							<< message.resourcerequestmessage().requests_size()
							<< std::endl;
					//for each resource request in the list, get resources
					//and add them to offer
					for (int i = 0;
							i < message.resourcerequestmessage().requests_size();
							i++) {
						const TmSdnMessage::ResourceRequestMessage::RecourceRequest &rr =
								message.resourcerequestmessage().requests(i);
						std::cout << "src/dst: " << rr.srcnode() << "/"
								<< rr.dstnode() << std::endl;
						TmSdnMessage::ResourceOfferMessage::RecourceOffer *ro =
								resource_offer->add_offers();
						rm_->generate_link_id(rr, ro);
						std::cout << "nid/lid/ilid: " << ro->nid() << "/"
								<< ro->lid() << "/" << ro->ilid() << std::endl;
					}

					//send resource offer back to socket
					google::protobuf::io::writeDelimitedTo(offer, &cos_adp);
					cos_adp.Flush();
					break;
				}
				default: {
					std::cout << "Invalid message type. " << std::endl;
					//TODO
					break;
				}
				}
			}

		}
	}

private:
	boost::asio::io_service& io_service_;
	tcp::acceptor acceptor_;
	Blackadder *ba_;
	ResourceManager *rm_;
};

/**@brief The main method initializing the required threads.
 *
 * It requires the parameters of the TM, specifically its ICN node identifier,
 * its Openflow identifier in the form of host:<mac_address> and
 * the Openflow identifier of its attached SDN switch in the form of openflow:x.
 *
 * Execute the server process by './server' for default Mininet settings, or
 * './server <node_id> host:<mac_address> openflow:X',
 * e.g. './server 00000019 host:af:dd:45:44:22:3e openflow:156786896342474'.
 *
 */
int main(int argc, char* argv[]) {
	string tm_nid, tm_of_id, attached_switch_of_id;
	if (argc == 4)
	{
		//read from cmd
		tm_nid = string(argv[1]);
		tm_of_id = string(argv[2]);
		attached_switch_of_id = string(argv[3]);
	}
	else
	{
		//assign default values
		tm_nid = "00000001";
		tm_of_id = "host:00:00:00:00:00:01";
		attached_switch_of_id = "openflow:1";
	}

	try
	{
		ResourceManager *rm = new ResourceManager(tm_nid, tm_of_id, attached_switch_of_id);
		Blackadder *ba = Blackadder::Instance(true);

		//start the tm resource thread.
		TMResource t = TMResource(rm);
		boost::thread* tm_thread = new boost::thread(
				boost::bind(&TMResource::init, &t));

		//start the tm-sdn server thread.
		boost::asio::io_service io_service;
		server s(io_service, 12345, ba, rm);
		boost::thread* s_thread = new boost::thread(
				boost::bind(&server::start_accept, &s));

		s_thread->join();
		tm_thread->join();

	} catch (std::exception& e) {
		std::cerr << "Exception: " << e.what() << "\n";
	}

	return 0;
}
