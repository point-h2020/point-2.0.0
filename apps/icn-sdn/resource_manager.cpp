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

#include "resource_manager.h"
#include <algorithm>
#include <sstream>
#include "stdio.h"
#include <fstream>
#include <cstdlib>
#include <iostream>
#include "../../deployment/bitvector.hpp"

ResourceManager::ResourceManager() {
	dm = Domain();
	dm.fid_len = 32;
	//generate link identifier between TM and its attached switch.
	//default parameters are assumed.
	generate_internal_link_id("00000001", true);
	generate_link_id("host:00:00:00:00:00:01", "openflow:1");
}

ResourceManager::ResourceManager(string tm_nid, string tm_of_id, string attached_switch_of_id) {
	dm = Domain();
	dm.fid_len = 32;
	//generate link identifier between TM and its attached switch.
	generate_internal_link_id(tm_nid, true);
	generate_link_id(tm_of_id, attached_switch_of_id);
}

ResourceManager::~ResourceManager() {
	// TODO Auto-generated destructor stub
}

int ResourceManager::generate_node_id_number() {
	int available_id = 1;
	//go through the list of node ids and if available, assign one.
	while (find(given_nids.begin(), given_nids.end(), available_id)
			!= given_nids.end())
		available_id++;
	//cout << "Generated node id is " << available_id << endl;
	return available_id;
}

int ResourceManager::generate_link_id_bit_position() {
	int available_id = 0;
	//go through the list of link ids and if available, assign one.
	while (find(given_lids.begin(), given_lids.end(), available_id)
			!= given_lids.end())
		available_id++;
	//cout << "Generated link id is " << available_id << endl;
	return available_id;
}

string ResourceManager::number_to_node_id(int number) {
	string nid;
	ostringstream ss;
	ss << number;
	string nid_number_string = ss.str();
	nid = string(8 - nid_number_string.length(), '0').append(nid_number_string);
	return nid;
}

string ResourceManager::generate_node_id(string node_information) {
	int nid_number = generate_node_id_number();
	string nid;
	//if still available node id, then assign it.
	if (nid_number != 99999999) {
		//cout << given_node_ids[node_information]  << endl;
		if (given_node_ids[node_information] == 0) {
			bool tmflag = false;
			if (nid_number == 1)
				tmflag = true;
			given_node_ids[node_information] = nid_number;
			given_nids.push_back(nid_number);
			nid = number_to_node_id(nid_number);
			cout << "Generated node id for " << node_information << " is " + nid
					<< endl;
			addNode(nid, tmflag);
			return nid;
		} else {
			cout << "Node " << node_information << " has already id "
					<< given_node_ids[node_information] << endl;
			return number_to_node_id(given_node_ids[node_information]);
		}
	} else {
		cout
				<< "Generated node id is higher than the given limit. Returning empty string."
				<< endl;
		return "";
	}
}

string ResourceManager::generate_link_id(string node_information,
		string attached_node_information) {

	int lid_number;
	string src_node_id = generate_node_id(node_information);
	string dst_node_id = generate_node_id(attached_node_information);
	string id = src_node_id + "," + dst_node_id;

	//if node was not assigned link id, then create new one.
	//otherwise return the already assigned one.
	//then also add the node connector to the graph.
	if (given_link_ids[id] == 0) {

		lid_number = generate_link_id_bit_position();
		if (lid_number == 255) {
			cout
					<< "Reached maximum number of available lids. Returning empty string"
					<< endl;
			return "";
		}
		string lid_bits = position_to_lid(lid_number);
		string ilid_bits;
		if (src_node_id.compare("00000001") == 0) {
			ilid_bits = generate_internal_link_id(src_node_id, false);
			cout << "Src node id 00000001 has ilid " << ilid_bits << endl;
		} else {
			ilid_bits = lid_bits;
		}
		given_lids.push_back(lid_number);
		given_link_ids[id] = lid_number;
		cout << "Generated lid between " << src_node_id << ", " << dst_node_id
				<< " is " + lid_bits << endl;
		addNodeConnector(src_node_id, dst_node_id, lid_bits, ilid_bits);
	} else {
		lid_number = given_link_ids[id];
		string lid_bits = position_to_lid(lid_number);
		cout << "Lid between " << src_node_id << ", " << dst_node_id
				<< " exists and is " + lid_bits << endl;
	}
	ostringstream ss;
	ss << lid_number;
	return src_node_id + "," + ss.str();
}

string ResourceManager::generate_internal_link_id(string src_node_id, bool flag) {
	string ilid_bits = "";
	int ilid_number;
	//if node was not assigned internal link id, then create new one.
	//otherwise return the already assigned one.
	if (given_internal_link_ids[src_node_id] == 0 && flag) {

		ilid_number = generate_link_id_bit_position();
		if (ilid_number == 255) {
			cout
					<< "Reached maximum number of available lids. Returning empty string"
					<< endl;
			return "";
		}
		ilid_bits = position_to_lid(ilid_number);
		given_lids.push_back(ilid_number);
		given_internal_link_ids[src_node_id] = ilid_number;
		cout << "Generated internal lid for " << src_node_id
				<< " is " + ilid_bits << endl;

	} else {
		ilid_number = given_internal_link_ids[src_node_id];
		ilid_bits = position_to_lid(ilid_number);
		cout << "Internal Lid for " << src_node_id
				<< " exists and is " + ilid_bits << endl;
	}
	return ilid_bits;
}

string ResourceManager::position_to_lid(int number) {
	string lid_bits = "";
	for (int i = 0; i < 256; i++) {
		if (i == number)
			lid_bits += '1';
		else
			lid_bits += '0';
	}
	return lid_bits;
}

void ResourceManager::generate_link_id(
		TmSdnMessage::ResourceRequestMessage::RecourceRequest request,
		TmSdnMessage::ResourceOfferMessage::RecourceOffer *offer) {
	int lid_number;
	string src_node_id = generate_node_id(request.srcnode());
	string dst_node_id = generate_node_id(request.dstnode());
	string id = src_node_id + "," + dst_node_id;
	string lid_bits;
	string ilid_bits;
	//if node was not assigned link id, then create new one.
	//otherwise return the already assigned one.
	//then also add the node connector to the graph.
	if (given_link_ids[id] == 0) {

		lid_number = generate_link_id_bit_position();
		if (lid_number == 255) {
			cout
					<< "Reached maximum number of available lids. Returning empty string"
					<< endl;
		}
		lid_bits = position_to_lid(lid_number);
		if (src_node_id.compare("00000001") == 0) {
			ilid_bits = generate_internal_link_id(src_node_id, false);
			cout << "Src node id 00000001 has ilid " << ilid_bits << endl;
		} else {
			ilid_bits = lid_bits;
		}
		given_lids.push_back(lid_number);
		given_link_ids[id] = lid_number;
		cout << "Generated lid between " << src_node_id << ", " << dst_node_id
				<< " is " + lid_bits << endl;
		addNodeConnector(src_node_id, dst_node_id, lid_bits, ilid_bits);
	} else {
		lid_number = given_link_ids[id];
		lid_bits = position_to_lid(lid_number);
		cout << "Lid between " << src_node_id << ", " << dst_node_id
				<< " exists and is " + lid_bits << endl;
	}
	//fill the offer with the assigned resources.
	offer->set_nid(src_node_id);
	offer->set_lid(lid_bits);
	offer->set_ilid(ilid_bits);
	offer->set_srcmac(request.srcmac());
}

string ResourceManager::generate_link_id_from_icn(string tmfid,
		string mac_address) {
	string lid_bits;
	int lid_number;
	//find the attached node using the given TMFID
	string dst_node_id = get_attached_node(tmfid);
	//string dst_node_id = "00000003";
	string src_node_id = generate_node_id("host:" + mac_address);
	string id = src_node_id + "," + dst_node_id;
	string ilid_bits = generate_internal_link_id(src_node_id, true);

	//if node was not assigned link id, then create new one.
	//otherwise return the already assigned one.
	//then also add the node connector to the graph.
	if (given_link_ids[id] == 0) {

		lid_number = generate_link_id_bit_position();
		if (lid_number == 255) {
			cout
					<< "Reached maximum number of available lids. Returning empty string"
					<< endl;
			return "";
		}
		lid_bits = position_to_lid(lid_number);
		given_lids.push_back(lid_number);
		given_link_ids[id] = lid_number;
		cout << "Generated lid between " << src_node_id << ", " << dst_node_id
				<< " is " + lid_bits << endl;
		addNodeConnector(src_node_id, dst_node_id, lid_bits, ilid_bits);
		generateGraphmlFile();

	} else {
		lid_number = given_link_ids[id];
		lid_bits = position_to_lid(lid_number);
		cout << "Lid between " << src_node_id << ", " << dst_node_id
				<< " exists and is " + lid_bits << endl;
	}

	return src_node_id + lid_bits + ilid_bits;
}

void ResourceManager::addNode(string src, bool tmrv) {
	NetworkNode *nn = new NetworkNode();
	nn->label = src;
	nn->isRV = tmrv;
	nn->isTM = tmrv;
	if (tmrv) {
		cout << "Is tm" << endl;
		dm.RV_node = nn;
		dm.TM_node = nn;
		dm.TM_node->running_mode = "user";
	}
	dm.network_nodes.push_back(nn);
}

void ResourceManager::addNodeConnector(string src_node, string dst_node,
		string lid, string ilid) {
	for (int i = 0; i < dm.network_nodes.size(); i++) {
		if (dm.network_nodes[i]->label.compare(src_node) == 0) {
			dm.network_nodes[i]->iLid = Bitvector(ilid);
			NetworkConnection *nc = new NetworkConnection();
			nc->src_label = src_node;
			nc->dst_label = dst_node;
			nc->LID = Bitvector(lid);
			//cout << "LID = " << nc->LID.to_string() << endl;
			dm.network_nodes[i]->connections.push_back(nc);
		}
	}
}

string ResourceManager::get_attached_node(string tmfid) {
	updateGraph();
	for (int i = 0; i < dm.network_nodes.size(); i++) {
		cout << "Node " << dm.network_nodes[i]->label << ", TMFID = "
				<< dm.network_nodes[i]->FID_to_TM.to_string() << endl;
		if (dm.network_nodes[i]->FID_to_TM.to_string().compare(tmfid) == 0) {
			cout << "Attached node is " << dm.network_nodes[i]->label << endl;
			return dm.network_nodes[i]->label;
		}
	}
	return "";
}

string ResourceManager::get_fid_to_node(string node) {
	updateGraph();
	string tm_label = "00000001";
	return graph.calculateFID(tm_label, node).to_string();
}

void ResourceManager::updateGraph() {
	graph = GraphRepresentation(&dm, false);
	graph.buildIGraphTopology();
	graph.calculateTMFIDs();
}

void ResourceManager::generateGraphmlFile() {
	updateGraph();

	//cout << "TMFID to " << dm.TM_node->label << " is " << dm.network_nodes[1]->FID_to_TM.to_string() << endl;

	igraph_cattribute_GAN_set(&graph.igraph, "FID_LEN", dm.fid_len);
	igraph_cattribute_GAS_set(&graph.igraph, "TM", dm.TM_node->label.c_str());
	igraph_cattribute_GAS_set(&graph.igraph, "RV", dm.RV_node->label.c_str());
	igraph_cattribute_GAS_set(&graph.igraph, "TM_MODE",
			dm.TM_node->running_mode.c_str());
	FILE * outstream_graphml = fopen(string("/tmp/topology.graphml").c_str(),
			"w");
#if IGRAPH_V >= IGRAPH_V_0_7
	igraph_write_graph_graphml(&graph.igraph, outstream_graphml,true);
#else
	igraph_write_graph_graphml(&graph.igraph, outstream_graphml);
#endif
	fclose(outstream_graphml);

}

