/*
 * Copyright © 2016 George Petropoulos.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
package eu.point.monitoring.impl.statistics;

import java.math.BigInteger;
import java.util.List;
import java.util.TimerTask;

import eu.point.client.Client;
import eu.point.registry.impl.NodeConnectorRegistryUtils;
import eu.point.tmsdn.impl.TmSdnMessages;
import org.opendaylight.controller.md.sal.binding.api.DataBroker;
import org.opendaylight.controller.md.sal.binding.api.ReadOnlyTransaction;
import org.opendaylight.controller.md.sal.common.api.data.LogicalDatastoreType;
import org.opendaylight.controller.md.sal.common.api.data.ReadFailedException;
import org.opendaylight.yang.gen.v1.urn.eu.point.registry.rev150722.node.connector.registry.NodeConnectorRegistryEntry;
import org.opendaylight.yang.gen.v1.urn.opendaylight.inventory.rev130819.Nodes;
import org.opendaylight.yang.gen.v1.urn.opendaylight.inventory.rev130819.node.NodeConnector;
import org.opendaylight.yang.gen.v1.urn.opendaylight.inventory.rev130819.nodes.Node;
import org.opendaylight.yang.gen.v1.urn.opendaylight.port.statistics.rev131214.FlowCapableNodeConnectorStatisticsData;
import org.opendaylight.yang.gen.v1.urn.opendaylight.port.statistics.rev131214.flow.capable.node.connector.statistics.FlowCapableNodeConnectorStatistics;
import org.opendaylight.yangtools.yang.binding.InstanceIdentifier;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.google.common.base.Optional;
import com.google.common.util.concurrent.CheckedFuture;

/**
 * The class which implements the traffic monitoring timer task.
 *
 * @author George Petropoulos
 * @version cycle-2
 */
public class TrafficMonitorTool extends TimerTask {

    private DataBroker db;
    private static final Logger LOG = LoggerFactory.getLogger(TrafficMonitorTool.class);

    /**
     * The constructor class.
     */
    public TrafficMonitorTool(DataBroker db) {
        this.db = db;
    }

    /**
     * The method which runs the monitoring task.
     * It monitors the node connector statistics, specifically packets and
     * bytes received and transmitted and sends them to the TM.
     */
    public void run() {
        //Read Nodes from datastore
        List<Node> nodeList = null;
        InstanceIdentifier<Nodes> nodesIid = InstanceIdentifier
                .builder(Nodes.class)
                .build();
        ReadOnlyTransaction nodesTransaction = db.newReadOnlyTransaction();
        CheckedFuture<Optional<Nodes>, ReadFailedException> nodesFuture = nodesTransaction.read(LogicalDatastoreType.OPERATIONAL, nodesIid);
        Optional<Nodes> nodesOptional = Optional.absent();
        try {
            nodesOptional = nodesFuture.checkedGet();
        } catch (ReadFailedException e) {
            LOG.warn("Nodes reading failed:", e);
        }
        if (nodesOptional.isPresent()) {
            nodeList = nodesOptional.get().getNode();
        }

        //Scan each node and get each node's connectors
        if (nodeList != null) {
            for (Node node : nodeList) {
                //LOG.info("Node ID: " + node.getId().getValue());
                List<NodeConnector> nodeConnectorList = node.getNodeConnector();
                if (nodeConnectorList != null) {
                    for (NodeConnector nodeConnector : nodeConnectorList) {
                        NodeConnectorRegistryEntry ncEntry = NodeConnectorRegistryUtils
                                .getInstance().readFromNodeConnectorRegistry(nodeConnector.getId().getValue());
                        //If there is some entry for this node connector in the registry then send data.
                        if (ncEntry != null) {
                            //extract the relevant statistics
                            FlowCapableNodeConnectorStatisticsData statData = nodeConnector.getAugmentation(FlowCapableNodeConnectorStatisticsData.class);
                            FlowCapableNodeConnectorStatistics statistics = statData.getFlowCapableNodeConnectorStatistics();
                            BigInteger packetsReceived = statistics.getPackets().getReceived();
                            BigInteger packetsTransmitted = statistics.getPackets().getTransmitted();
                            BigInteger bytesReceived = statistics.getBytes().getReceived();
                            BigInteger bytesTransmitted = statistics.getBytes().getTransmitted();
                            String nodeId1 = ncEntry.getSrcNode();
                            String nodeId2 = ncEntry.getDstNode();
                            LOG.info("nodeId1 {}, nodeId2 {}", nodeId1, nodeId2);
                            LOG.info("PT, PR, BT, BR:" + packetsTransmitted.toString() + " " + packetsReceived.toString() + " " + bytesTransmitted.toString() + " " + bytesReceived.toString());
                            //prepare the traffic monitoring message and send it to TM
                            TmSdnMessages.TmSdnMessage.TrafficMonitoringMessage.Builder tmBuilder =
                                    TmSdnMessages.TmSdnMessage.TrafficMonitoringMessage.newBuilder();
                            tmBuilder.setNodeID1(nodeId1)
                                    .setNodeID2(nodeId2)
                                    .setBytesReceived(bytesReceived.longValue())
                                    .setBytesTransmitted(bytesTransmitted.longValue())
                                    .setPacketsReceived(packetsReceived.longValue())
                                    .setPacketsTransmitted(packetsTransmitted.longValue());
                            TmSdnMessages.TmSdnMessage message = TmSdnMessages.TmSdnMessage.newBuilder()
                                    .setTrafficMonitoringMessage(tmBuilder.build())
                                    .setType(TmSdnMessages.TmSdnMessage.TmSdnMessageType.TM)
                                    .build();
                            new Client().sendMessage(message);
                        }
                    }
                }
            }
        }
    }
}
