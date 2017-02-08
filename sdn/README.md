# The ICN-over-SDN Opendaylight Application

## General information & structure

The ICN-over-SDN application performs required ICN functions on top of the Opendaylight SDN controller. Specifically, it executes:

 - Bootstrapping of SDN switches and their configuration with arbitrary bitmask rules. This includes monitoring the topology for new SDN switches and links, requesting unique LID for each interface by the Resource Manager and translating them to relevant Openflow rules.
 - Bootstrapping of new ICN nodes, by listening for incoming ICN packets (packets of IPv6 type, and destination ethernet address of 00::00) and responding with the calculated FID to reach the TM.
 - Monitoring of SDN topology for nodes and node connectors' additions, updates or deletions and informing the Topology Manager for such events.
 - Monitoring of SDN topology for all configured node connectors' statistics, and specifically, received and transmitted packets and bytes.

To realize these functionalities, the implemented Opendaylight application consists of the following modules:

 - **bootstrapping**: The module responsible for performing all bootstrapping functions for SDN switches and end-hosts.
 - **monitoring**: The module responsible for monitoring the configured SDN topology for links' updates and statistics' aggregation.
 - **registry**: The module handling the generation of the necessary registries (similar to database concept), and exposing all the methods for insertion and retrieval of stored entries.
 - **tm-sdn**: The client side of the TM-SDN interface, also including the generated protobuf protocol class.
 - **icn-sdn-ui**: The User Interface bundle, implementing the page presenting the ICN-over-SDN topology and the available controls for bootstrapping and monitoring.
 - **commons**: It includes all the common properties for proper build and execution.
 - **features**: It imports required Opendaylight bundles and features and creates the project-specific ones to be installed in the Karaf artifact for proper execution.
 - **distribution**: The module which creates the Karaf artifact to be executed, including the ICN-over-SDN application modules and required Opendaylight dependencies.

## Prerequisites
As ICN-over-SDN is an Opendaylight Boron application, it is essential that the development environment for implementing such applications must be configured properly. Hence, follow the provided instructions by the Opendaylight community, available [here](https://wiki.opendaylight.org/view/GettingStarted:Development_Environment_Setup).  Using these instructions you will be able to install and configure Maven, essential for building the project, and also setup Eclipse, in which you could check the source code and extend its functions.

Finally, the bootstrapping function can be used as standalone as explained later, but generally, this application requires the [ICN-SDN Application](https://github.com/point-h2020/point-2.0.0/tree/master/apps/icn-sdn) extension to request and retrieve unique ICN parameters and resources. 

## Build & execution instructions

 - To build the code: `mvn clean install -DskipTests`
 - To run the controller executable: `./distribution/opendaylight-karaf/target/assembly/bin/karaf`
 - In the karaf console, execute: `feature:install tm-sdn` and `feature:install point-ui`
 - To generate the javadoc documentation execute: `mvn javadoc:javadoc`. It will be available at the *target/site/apidocs/index.html*.
 - To generate the doxygen documentation execute: `doxygen doxygen.config`. It will generate the html and latex documentation at *target/doxygen/html/index.html* and *target/doxygen/latex/index.html*

## Configuration instructions
To configure the application you can either use the provided yangui tab of the Opendaylight dlux application at [http://localhost:8181/index.html#/yangui/index](http://localhost:8181/index.html#/yangui/index), or use any REST application to send a POST HTTP request.

 - In the first case, navigate to *bootstrapping > operations > configureTm* bullets, insert the appropriate configuration parameters, and press the *Send* button.
 - In the second case, send an HTTP POST request to [http://localhost:8181/restconf/operations/bootstrapping:configureTm](http://localhost:8181/restconf/operations/bootstrapping:configureTm), including headers for Basic authorization (admin/admin), Content-type and Accept: application/json, and body as indicated below:

> {
	input: {
		tmIp: "192.168.1.2",
		tmPort: 12345,
		tmOpenflowId: "host:00:00:00:00:00:01",
		tmAttachedSwitchId: "openflow:1",
		tmNodeId: "00000001",
		tmLid: 1,
		tmInternalLid: 0
	}
}

In the above example, 

 - *tmIp* indicates the IP address of the TM, *tmPort* the port in which the TM-SDN server listens to (default is 12345),
 - *tmAttachedSwitchId* is the identifier of the attached SDN switch to the TM as provided by the SDN controller, 
 - *tmNodeId* is the Node ID (NID) of the TM, 
 - *tmLid* requires the position of '1' in TM's Link ID (LID), and,
 - *tmInternalLid* is the position of '1' in TM's Internal Link ID (iLID).
 
## Arbitrary Bitmask Rules Configuration
To configure Arbitrary Bitmask (ABM) rules to SDN switches, you can use one of the following alternatives: (a) manually using the controller's existing API, (b) manually using an implemented simplified API, and (c) automatically using the implemented User Interface, as well as the [ICN-SDN Application](https://github.com/point-h2020/point-2.0.0/tree/master/apps/icn-sdn) extension.
 
 - Send an HTTP PUT request to [http://localhost:8181/restconf/config/opendaylight-inventory:nodes/node/openflow:1/table/0/flow/2](http://localhost:8181/restconf/config/opendaylight-inventory:nodes/node/openflow:1/table/0/flow/2), replacing "openflow:1", "0" and "2" with the openflow identifier of the switch you want to configure, the table number and the flow identifier you are configuring respectively, including headers for Basic authorization (admin/admin), Content-type and Accept: application/json, and sample body as indicated below. In the body, replace the id, output-node-connector, and ipv6-\*-address-\* fields with your parameters.
 
> {
  "flow": {
    "barrier": "false",
    "cookie": "10",
    "cookie_mask": "10",
    "flow-name": "example",
    "id": "2",
    "priority": "1030",
    "table_id": "0",
    "instructions": {
      "instruction": {
        "apply-actions": {
          "action": {
            "order": "0",
            "output-action": { "output-node-connector": "3" }
          }
        },
        "order": "0"
      }
    },
    "match": {
      "ethernet-match": {
        "ethernet-type": { "type": "34525" },
        "ethernet-destination": { "address": "00:00:00:00:00:00" }
      },
      "ipv6-source-address-no-mask": "0000:4000:0000:0000:0000:0000:0000:0000",
      "ipv6-source-arbitrary-bitmask": "0000:4000:0000:0000:0000:0000:0000:0000",
      "ipv6-destination-address-no-mask": "0000:0000:0000:0000:0000:0000:0000:0000",
      "ipv6-destination-arbitrary-bitmask": "0000:0000:0000:0000:0000:0000:0000:0000"
    }
  }
}

 - Send an HTTP POST request to [http://localhost:8181/restconf/operations/bootstrapping:configureSwitch](http://localhost:8181/restconf/operations/bootstrapping:configureSwitch), including headers for Basic authorization (admin/admin), Content-type and Accept: application/json, and sample body as indicated below. In the body, replace the switchId, portId, and linkId fields with your parameters.

> {
	input: {
		switchId: "openflow:1",
		portId: "openflow:1:2",
		linkId: 10
	}
}

 - Assuming that the Resource Manager is running, and the SDN controller is configured properly following the aforementioned instructions, navigate to the [ICN-SDN-UI page](http://localhost:8181/index.html#/icnsdnui), and press the *Bootstrap Topology* button to initialize the automatic bootstrapping of SDN switches. On reload, the switches and their respective links should have assigned identifiers.
 
## APIs
From the base URI (http://localhost:8181/restconf), the following set of APIs is exposed to end users and applications:

 - **POST**
	 - */operations/bootstrapping:configureTm*
	 
	 Method to configure the TM parameters. Example JSON body as indicated below.
> {
	input: {
		tmIp: "192.168.1.2",
		tmPort: 12345,
		tmOpenflowId: "host:00:00:00:00:00:01",
		tmAttachedSwitchId: "openflow:1",
		tmNodeId: "00000001",
		tmLid: 1,
		tmInternalLid: 0
	}
}

	 - */operations/bootstrapping:activateApplication*
	 
	 Method to activate/deactivate the automatic bootstrapping application. Example JSON body as indicated below.
> {
	input: {
		status: true
	}
}

	 - */operations/bootstrapping:configureSwitch*
	 
	 Method to configure an SDN switch with ABM rules manually. Example JSON body as indicated below.
> {
	input: {
		switchId: "openflow:1",
		portId: "openflow:1:2",
		linkId: 10
	}
}

	 - */operations/bootstrapping:nodeLinkInformation* 
	 
	 Method to request and retrieve the configured ICN parameters for a specific link between two nodes. Example JSON body as indicated below.
> {
	input: {
		srcNode: "openflow:1",
		dstNode: "openflow:2"
	}
}

	 - */operations/monitoring:init*
	 
	 Method which activates/deactivates the link state and traffic monitoring functionalities. Example JSON body as indicated below.
> {
	input: {
		trafficmonitor-enabled: true,
		trafficmonitor-period: 30,
		linkmonitor-enabled: true
	}
}


 - **GET**
	 - */operational/registry:node-registry*
	 
	 The method which fetches all configured node identifiers for all SDN switches and end-hosts.
	 - */operational/registry:node-registry/node-registry-entry/{entry-id}*
	 
	 The method which fetches the configured node identifier for a specific SDN switch/end-host with the given {entry-id}.
	 - */operational/registry:link-registry*
	 
	 The method which fetches all configured link identifiers for all SDN switches and end-hosts.
	 - */operational/registry:link-registry/link-registry-entry/{entry-id}*
	 
	 The method which fetches the configured link identifier for a specific SDN switch/end-host link with the given {entry-id}.
	 - */operational/registry:node-connector-registry*
	 
	 The method which fetches all configured node and link identifiers for all SDN switches and end-hosts.
	 - */operational/registry:node-connector-registry/node-connector-registry-entry/{entry-id}*
	 
	 The method which fetches the configured node and link identifier for a specific SDN switch/end-host link with the given {entry-id}.
