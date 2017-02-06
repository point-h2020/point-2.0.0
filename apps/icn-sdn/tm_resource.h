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

#ifndef APPS_RESOURCE_MANAGER_TM_RESOURCE_H_
#define APPS_RESOURCE_MANAGER_TM_RESOURCE_H_

#include <blackadder.hpp>
#include <signal.h>
#include "../../deployment/bitvector.hpp"
#include "resource_manager.h"

/**@brief The class for TM resource manager to handle resource requests by new nodes.
 *
 * @author George Petropoulos
 * @version cycle-2
 *
 */
class TMResource {
public:

	/**@brief The constructor.
	 *
	 */
	TMResource();

	/**@brief The constructor with argument.
	 *
	 * @param rm_ The pointer to the resource manager object.
	 *
	 */
	TMResource(ResourceManager *rm_);
	virtual ~TMResource();

	/**@brief The method which returns the substring of a given string.
	 *
	 * @param input The given string as input.
	 * @param start The starting position of the substring.
	 * @param len The length of the substring.
	 *
	 * @return The calculated substring.
	 *
	 */
	char *substring(char const *input, size_t start, size_t len);

	/**@brief The method which initializes the class.
	 *
	 * It listens for resource requests at a given scope.
	 * When it receives one, it then extracts the received parameters,
	 * including the TMFID and MAC address, and requests by the resource
	 * manager unique node id and link id and publishes them as
	 * resource offer.
	 *
	 */
	void init();

private:
	/**@brief The blackadder instance.
	 *
	 */
	Blackadder *ba;

	/**@brief The resource manager instance.
	 *
	 */
	ResourceManager *rm;

	/**@brief The FID to reach the new node.
	 *
	 */
	Bitvector *fid_to_node;
	bool operation = true;
	pthread_t _event_listener, *event_listener = NULL;

	/**@brief The offer to be sent as a response to the new node.
	 *
	 */
	char* offer = (char *) malloc(8 + 256 + 256 + 1);

	/**@brief The resource scope.
	 *
	 */
	string resource_id = "4E4E4E4E4E4E4E4E"; // RESOURCE scope: "4E4E..4E";
	string bin_resource_id = hex_to_chararray(resource_id);

	/**@brief The offer scope.
	 *
	 */
	string offer_id = "4545454545454545"; // OFFER scope: "4545..45";
	string bin_offer_id = hex_to_chararray(offer_id);

	/**@brief The request scope.
	 *
	 */
	string request_id = "5555555555555555"; // REQUEST scope: "5555..55";
	string bin_request_id = hex_to_chararray(request_id);
};

#endif /* APPS_RESOURCE_MANAGER_TM_RESOURCE_H_ */
