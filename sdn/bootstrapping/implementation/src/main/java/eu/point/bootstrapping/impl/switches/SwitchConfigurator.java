/*
 * Copyright © 2016 George Petropoulos.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
package eu.point.bootstrapping.impl.switches;

import java.math.BigInteger;
import java.util.ArrayList;
import java.util.List;
import java.util.concurrent.atomic.AtomicLong;

import com.google.common.util.concurrent.*;
import eu.point.registry.impl.LoggingFuturesCallBack;
import org.opendaylight.controller.md.sal.binding.api.DataBroker;
import org.opendaylight.controller.md.sal.binding.api.WriteTransaction;
import org.opendaylight.controller.md.sal.common.api.data.LogicalDatastoreType;
import org.opendaylight.controller.md.sal.common.api.data.TransactionCommitFailedException;
import org.opendaylight.openflowplugin.api.OFConstants;
import org.opendaylight.yang.gen.v1.urn.ietf.params.xml.ns.yang.ietf.inet.types.rev130715.Ipv6Address;
import org.opendaylight.yang.gen.v1.urn.ietf.params.xml.ns.yang.ietf.inet.types.rev130715.Uri;
import org.opendaylight.yang.gen.v1.urn.ietf.params.xml.ns.yang.ietf.yang.types.rev130715.MacAddress;
import org.opendaylight.yang.gen.v1.urn.opendaylight.action.types.rev131112.action.action.OutputActionCaseBuilder;
import org.opendaylight.yang.gen.v1.urn.opendaylight.action.types.rev131112.action.action.output.action._case.OutputActionBuilder;
import org.opendaylight.yang.gen.v1.urn.opendaylight.action.types.rev131112.action.list.Action;
import org.opendaylight.yang.gen.v1.urn.opendaylight.action.types.rev131112.action.list.ActionBuilder;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.inventory.rev130819.FlowCapableNode;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.inventory.rev130819.FlowId;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.inventory.rev130819.tables.Table;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.inventory.rev130819.tables.TableKey;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.inventory.rev130819.tables.table.Flow;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.inventory.rev130819.tables.table.FlowBuilder;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.inventory.rev130819.tables.table.FlowKey;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.service.rev130819.*;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.types.rev131026.FlowCookie;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.types.rev131026.FlowModFlags;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.types.rev131026.flow.Instructions;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.types.rev131026.flow.InstructionsBuilder;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.types.rev131026.flow.Match;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.types.rev131026.flow.MatchBuilder;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.types.rev131026.instruction.instruction.ApplyActionsCaseBuilder;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.types.rev131026.instruction.instruction.apply.actions._case.ApplyActions;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.types.rev131026.instruction.instruction.apply.actions._case.ApplyActionsBuilder;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.types.rev131026.instruction.list.InstructionBuilder;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.types.rev131026.instruction.list.Instruction;
import org.opendaylight.yang.gen.v1.urn.opendaylight.inventory.rev130819.NodeId;
import org.opendaylight.yang.gen.v1.urn.opendaylight.inventory.rev130819.NodeRef;
import org.opendaylight.yang.gen.v1.urn.opendaylight.inventory.rev130819.Nodes;
import org.opendaylight.yang.gen.v1.urn.opendaylight.inventory.rev130819.nodes.Node;
import org.opendaylight.yang.gen.v1.urn.opendaylight.inventory.rev130819.nodes.NodeKey;
import org.opendaylight.yang.gen.v1.urn.opendaylight.l2.types.rev130827.EtherType;
import org.opendaylight.yang.gen.v1.urn.opendaylight.model.match.types.rev131026.ethernet.match.fields.EthernetDestinationBuilder;
import org.opendaylight.yang.gen.v1.urn.opendaylight.model.match.types.rev131026.ethernet.match.fields.EthernetTypeBuilder;
import org.opendaylight.yang.gen.v1.urn.opendaylight.model.match.types.rev131026.match.EthernetMatchBuilder;
import org.opendaylight.yang.gen.v1.urn.opendaylight.model.match.types.rev131026.match.layer._3.match.Ipv6MatchArbitraryBitMaskBuilder;
import org.opendaylight.yang.gen.v1.urn.opendaylight.opendaylight.ipv6.arbitrary.bitmask.fields.rev160224.Ipv6ArbitraryMask;
import org.opendaylight.yangtools.yang.binding.InstanceIdentifier;
import org.opendaylight.yangtools.yang.common.RpcResult;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.google.common.collect.ImmutableList;

/**
 * The switch configurator class.
 *
 * @author George Petropoulos
 * @version cycle-2
 */
public class SwitchConfigurator {

    private static final Logger LOG = LoggerFactory.getLogger(SwitchConfigurator.class);

    private SalFlowService salFlowService;

    private DataBroker db;

    /**
     * The constructor class.
     */
    public SwitchConfigurator(SalFlowService salFlowService) {
        this.salFlowService = salFlowService;
    }

    /**
     * The constructor class.
     */
    public SwitchConfigurator(DataBroker db) {
        this.db = db;
    }


    private AtomicLong flowCookieInc = new AtomicLong(0x2a00000000000000L);

    /**
     * The method which installs a flow to the switch, through the sal flow service.
     *
     * @param edge_switch       The switch to be configured.
     * @param edgeNodeConnector The node connector to be configured.
     * @param srcAddress        The source IPv6 address to be added to the arbitrary bitmask rule.
     * @param dstAddress        The destination IPv6 address to be added to the arbitrary bitmask rule.
     */
    public void addFlow(String edge_switch, String edgeNodeConnector, String srcAddress, String dstAddress) {

        LOG.debug("Start executing RPC");

        // create the flow
        Flow createdFlow = createAddedFlow(edgeNodeConnector, srcAddress, dstAddress);

        // build instance identifier for flow
        InstanceIdentifier<Flow> flowPath = InstanceIdentifier
                .builder(Nodes.class)
                .child(Node.class, new NodeKey(new NodeId(edge_switch)))
                .augmentation(FlowCapableNode.class)
                .child(Table.class, new TableKey(createdFlow.getTableId()))
                .child(Flow.class, new FlowKey(createdFlow.getId())).build();


        final AddFlowInputBuilder builder = new AddFlowInputBuilder(createdFlow);
        final InstanceIdentifier<Table> tableInstanceId = flowPath
                .<Table>firstIdentifierOf(Table.class);
        final InstanceIdentifier<Node> nodeInstanceId = flowPath
                .<Node>firstIdentifierOf(Node.class);
        builder.setNode(new NodeRef(nodeInstanceId));
        builder.setFlowTable(new FlowTableRef(tableInstanceId));
        builder.setTransactionUri(new Uri(createdFlow.getId().getValue()));
        final AddFlowInput flow = builder.build();

        LOG.debug("onPacketReceived - About to write flow (via SalFlowService) {}", flow);
        // add flow to sal
        ListenableFuture<RpcResult<AddFlowOutput>> result = JdkFutureAdapters
                .listenInPoolThread(salFlowService.addFlow(flow));
        Futures.addCallback(result, new FutureCallback<RpcResult<AddFlowOutput>>() {
            @Override
            public void onSuccess(final RpcResult<AddFlowOutput> o) {
                LOG.debug("Successful outcome.");
            }

            @Override
            public void onFailure(final Throwable throwable) {
                LOG.debug("Failure.");
                throwable.printStackTrace();
            }
        });
    }


    /**
     * The method which installs a flow to the switch, through the configuration datastore of the controller.
     *
     * @param edge_switch       The switch to be configured.
     * @param edgeNodeConnector The node connector to be configured.
     * @param srcAddress        The source IPv6 address to be added to the arbitrary bitmask rule.
     * @param dstAddress        The destination IPv6 address to be added to the arbitrary bitmask rule.
     */
    public void addFlowConfig(String edge_switch, String edgeNodeConnector, String srcAddress, String dstAddress) {

        LOG.debug("Adding flow to config datastore.");

        // create the flow
        Flow createdFlow = createAddedFlow(edgeNodeConnector, srcAddress, dstAddress);

        // build instance identifier for flow
        InstanceIdentifier<Flow> flowPath = InstanceIdentifier
                .builder(Nodes.class)
                .child(Node.class, new NodeKey(new NodeId(edge_switch)))
                .augmentation(FlowCapableNode.class)
                .child(Table.class, new TableKey(createdFlow.getTableId()))
                .child(Flow.class, new FlowKey(createdFlow.getId())).build();


        WriteTransaction transaction = db.newWriteOnlyTransaction();
        transaction.put(LogicalDatastoreType.CONFIGURATION, flowPath, createdFlow, true);
        CheckedFuture<Void, TransactionCommitFailedException> future = transaction.submit();
        Futures.addCallback(future,
                new LoggingFuturesCallBack<>("Failed to add flow.", LOG));
        LOG.debug("Added flow {} to config datastore.", createdFlow);
    }

    /**
     * The method which prepares the flow to be added to the switch.
     *
     * @param edgeNodeConnector The node connector to be configured.
     * @param srcAddress        The source IPv6 address to be added to the arbitrary bitmask rule.
     * @param dstAddress        The destination IPv6 address to be added to the arbitrary bitmask rule.
     * @return The flow to be added.
     */
    private Flow createAddedFlow(String edgeNodeConnector, String srcAddress, String dstAddress) {

        FlowBuilder flowBuilder = new FlowBuilder() //
                .setTableId((short) 0) //
                .setFlowName(edgeNodeConnector);

        flowBuilder.setId(new FlowId(edgeNodeConnector));

        Match match = new MatchBuilder()
                .setEthernetMatch(new EthernetMatchBuilder()
                        .setEthernetType(new EthernetTypeBuilder()
                                .setType(new EtherType(0x86DDL))
                                .build())
                        .setEthernetDestination(new EthernetDestinationBuilder()
                                .setAddress(new MacAddress("00:00:00:00:00:00"))
                                .build())
                        .build())
                .setLayer3Match(new Ipv6MatchArbitraryBitMaskBuilder()
                        .setIpv6SourceAddressNoMask(new Ipv6Address(srcAddress))
                        .setIpv6SourceArbitraryBitmask(new Ipv6ArbitraryMask(srcAddress))
                        .setIpv6DestinationAddressNoMask(new Ipv6Address(dstAddress))
                        .setIpv6DestinationArbitraryBitmask(new Ipv6ArbitraryMask(dstAddress))
                        .build())
                .build();


        ActionBuilder actionBuilder = new ActionBuilder();
        List<Action> actions = new ArrayList<Action>();
        //Actions
        Action outputNodeConnectorAction = actionBuilder
                .setOrder(0).setAction(new OutputActionCaseBuilder()
                        .setOutputAction(new OutputActionBuilder()
                                .setOutputNodeConnector(new Uri(edgeNodeConnector.split(":")[2]))
                                .build())
                        .build())
                .build();
        actions.add(outputNodeConnectorAction);

        //ApplyActions
        ApplyActions applyActions = new ApplyActionsBuilder().setAction(actions).build();

        //Instruction
        Instruction applyActionsInstruction = new InstructionBuilder() //
                .setOrder(0).setInstruction(new ApplyActionsCaseBuilder()//
                        .setApplyActions(applyActions) //
                        .build()) //
                .build();

        Instructions applyInstructions = new InstructionsBuilder()
                .setInstruction(ImmutableList.of(applyActionsInstruction))
                .build();

        // Put our Instruction in a list of Instructions
        flowBuilder
                .setMatch(match)
                .setBufferId(OFConstants.OFP_NO_BUFFER)
                .setInstructions(applyInstructions)
                .setPriority(1000)
                .setHardTimeout(30000)
                .setIdleTimeout(30000)
                .setCookie(new FlowCookie(BigInteger.valueOf(flowCookieInc.getAndIncrement())))
                .setFlags(new FlowModFlags(false, false, false, false, false));

        return flowBuilder.build();
    }

    /**
     * The method which deletes a flow from the switch, through the sal flow service.
     *
     * @param edge_switch       The switch to be configured.
     * @param edgeNodeConnector The node connector to be configured.
     * @param srcAddress        The source IPv6 address to be added to the arbitrary bitmask rule.
     * @param dstAddress        The destination IPv6 address to be added to the arbitrary bitmask rule.
     */
    public void deleteFlow(String edge_switch, String edgeNodeConnector, String srcAddress, String dstAddress) {

        LOG.debug("Start executing RPC");

        // create the flow
        Flow removedFlow = createDeletedFlow(edgeNodeConnector, srcAddress, dstAddress);

        // build instance identifier for flow
        InstanceIdentifier<Flow> flowPath = InstanceIdentifier
                .builder(Nodes.class)
                .child(Node.class, new NodeKey(new NodeId(edge_switch)))
                .augmentation(FlowCapableNode.class)
                .child(Table.class, new TableKey(removedFlow.getTableId()))
                .child(Flow.class, new FlowKey(removedFlow.getId())).build();


        final RemoveFlowInputBuilder builder = new RemoveFlowInputBuilder(removedFlow);
        final InstanceIdentifier<Table> tableInstanceId = flowPath
                .<Table>firstIdentifierOf(Table.class);
        final InstanceIdentifier<Node> nodeInstanceId = flowPath
                .<Node>firstIdentifierOf(Node.class);
        builder.setNode(new NodeRef(nodeInstanceId));
        builder.setFlowTable(new FlowTableRef(tableInstanceId));
        builder.setTransactionUri(new Uri(removedFlow.getId().getValue()));
        final RemoveFlowInput flow = builder.build();

        LOG.debug("onPacketReceived - About to remove flow (via SalFlowService) {}", flow);
        // add flow to sal
        ListenableFuture<RpcResult<RemoveFlowOutput>> result = JdkFutureAdapters
                .listenInPoolThread(salFlowService.removeFlow(flow));
        Futures.addCallback(result, new FutureCallback<RpcResult<RemoveFlowOutput>>() {
            @Override
            public void onSuccess(final RpcResult<RemoveFlowOutput> o) {
                LOG.debug("Successful outcome.");
            }

            @Override
            public void onFailure(final Throwable throwable) {
                LOG.debug("Failure.");
                throwable.printStackTrace();
            }
        });
    }


    /**
     * The method which deletes a flow from the switch, through the configuration datastore of the controller.
     *
     * @param edge_switch       The switch to be configured.
     * @param edgeNodeConnector The node connector to be configured.
     */
    public void deleteFlowConfig(String edge_switch, String edgeNodeConnector) {

        LOG.debug("Deleting flow from config datastore.");

        // build instance identifier for flow
        InstanceIdentifier<Flow> flowPath = InstanceIdentifier
                .builder(Nodes.class)
                .child(Node.class, new NodeKey(new NodeId(edge_switch)))
                .augmentation(FlowCapableNode.class)
                .child(Table.class, new TableKey((short) 0))
                .child(Flow.class, new FlowKey(new FlowId(edgeNodeConnector))).build();


        WriteTransaction transaction = db.newWriteOnlyTransaction();
        transaction.delete(LogicalDatastoreType.CONFIGURATION, flowPath);
        CheckedFuture<Void, TransactionCommitFailedException> future = transaction.submit();
        Futures.addCallback(future,
                new LoggingFuturesCallBack<>("Failed to delete flow", LOG));
        LOG.debug("Deleted flow with id {} from config datastore.", new FlowId(edgeNodeConnector));
    }

    //create the flow to be deleted
    private Flow createDeletedFlow(String edgeNodeConnector, String srcAddress, String dstAddress) {

        FlowBuilder flowBuilder = new FlowBuilder() //
                .setTableId((short) 0) //
                .setFlowName(edgeNodeConnector);

        flowBuilder.setId(new FlowId(edgeNodeConnector));

        Match match = new MatchBuilder()
                .setEthernetMatch(new EthernetMatchBuilder()
                        .setEthernetType(new EthernetTypeBuilder()
                                .setType(new EtherType(0x86DDL))
                                .build())
                        .setEthernetDestination(new EthernetDestinationBuilder()
                                .setAddress(new MacAddress("00:00:00:00:00:00"))
                                .build())
                        .build())
                .setLayer3Match(new Ipv6MatchArbitraryBitMaskBuilder()
                        .setIpv6SourceAddressNoMask(new Ipv6Address(srcAddress))
                        .setIpv6SourceArbitraryBitmask(new Ipv6ArbitraryMask(srcAddress))
                        .setIpv6DestinationAddressNoMask(new Ipv6Address(dstAddress))
                        .setIpv6DestinationArbitraryBitmask(new Ipv6ArbitraryMask(dstAddress))
                        .build())
                .build();

        ActionBuilder actionBuilder = new ActionBuilder();
        List<Action> actions = new ArrayList<Action>();
        //Actions
        Action outputNodeConnectorAction = actionBuilder
                .setOrder(0).setAction(new OutputActionCaseBuilder()
                        .setOutputAction(new OutputActionBuilder()
                                .setOutputNodeConnector(new Uri(edgeNodeConnector.split(":")[2]))
                                .build())
                        .build())
                .build();
        actions.add(outputNodeConnectorAction);

        //ApplyActions
        ApplyActions applyActions = new ApplyActionsBuilder().setAction(actions).build();

        //Instruction
        Instruction applyActionsInstruction = new InstructionBuilder() //
                .setOrder(0).setInstruction(new ApplyActionsCaseBuilder()//
                        .setApplyActions(applyActions) //
                        .build()) //
                .build();

        Instructions applyInstructions = new InstructionsBuilder()
                .setInstruction(ImmutableList.of(applyActionsInstruction))
                .build();

        // Put our Instruction in a list of Instructions
        flowBuilder
                .setMatch(match)
                .setBufferId(OFConstants.OFP_NO_BUFFER)
                .setInstructions(applyInstructions)
                .setPriority(1000)
                .setHardTimeout(30000)
                .setIdleTimeout(30000)
                .setCookie(new FlowCookie(BigInteger.valueOf(flowCookieInc.getAndIncrement())))
                .setFlags(new FlowModFlags(false, false, false, false, false));

        return flowBuilder.build();
    }
}