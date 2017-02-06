/*
 * node.hh
 *
 *  Created on: 17 Feb 2016
 *      Author: point
 */

#ifndef LIB_BAMPERS_DATAPOINTS_NODE_HH_
#define LIB_BAMPERS_DATAPOINTS_NODE_HH_

#include <string>

#include "../namespace.hh"

using namespace std;

class Node {
public:
	Node(Namespace &namespaceHelper);
	virtual ~Node();
	/*!
	 * \brief
	 *
	 * /monitoring/nodeType/
	 */
	void add(uint8_t nodeType, string nodeName, NODE_ID nodeId);
private:
	Namespace &_namespaceHelper; /*!< Reference to the class Namespace */
};

#endif /* LIB_BAMPERS_DATAPOINTS_NODE_HH_ */
