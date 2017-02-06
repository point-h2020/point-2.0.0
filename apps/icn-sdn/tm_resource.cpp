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

#include "tm_resource.h"

using namespace std;

TMResource::TMResource() {

}

TMResource::TMResource(ResourceManager *rm_) {
	rm = rm_;
}


TMResource::~TMResource() {
	// TODO Auto-generated destructor stub
}

char *TMResource::substring(char const *input, size_t start, size_t len) {
	char *ret = (char *) malloc(len + 1);
	memcpy(ret, input + start, len);
	ret[len] = '\0';
	return ret;
}

void TMResource::init() {

	std::cout << "Initializing TM Resource Manager." << endl;

	ba = Blackadder::Instance(true);

	//subscribe to resource requests
	ba->subscribe_scope(bin_request_id, bin_resource_id, BROADCAST_IF, NULL, 0);
	cout << "Subscribed to "
			<< chararray_to_hex(bin_resource_id + bin_request_id)
			<< " scope and waiting for resource requests." << endl;
	string full_id;
	bool operation = true;

	while (operation) {
		Event ev;
		ba->getEvent(ev);
		//cout << "Inside loop, received event " << ev.type << endl;
		switch (ev.type) {
		case PUBLISHED_DATA:
			full_id = chararray_to_hex(ev.id);
			cout << "Published data " << full_id << endl;
			cout << "Received resource request publication." << endl;
			//extract the received parameters of the new node.
			char* data = (char *) ev.data;
			cout << "Received data: " << data << endl;
			string received_tmfid = substring(data, 0, 256);
			cout << "Received TMFID: " << received_tmfid << endl;
			string received_mac = substring(data, 256, 256 + 17);
			cout << "Received MAC Address: " << received_mac << endl;

			//request by the resource manager to generate unique node and link ids for the new node.
			string node_id_link_id = rm->generate_link_id_from_icn(
					received_tmfid, received_mac);
			string node_id = substring(node_id_link_id.c_str(), 0, 8);
			cout << "Node id " << node_id << endl;
			//calculate the FID to the new node.
			string fidtotm = rm->get_fid_to_node(node_id);
			fid_to_node = new Bitvector(fidtotm);

			snprintf(offer, 8 + 256 + 256 + 1, node_id_link_id.c_str());

			//publish resource offer to new node
			ba->publish_data(bin_resource_id + bin_offer_id,
			IMPLICIT_RENDEZVOUS, (char *) fid_to_node->_data, FID_LEN, offer,
				8 + 256 + 256 + 1);
			break;
		}
	}
}

