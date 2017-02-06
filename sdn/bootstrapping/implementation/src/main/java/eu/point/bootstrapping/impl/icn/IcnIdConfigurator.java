/*
 * Copyright © 2016 George Petropoulos.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
package eu.point.bootstrapping.impl.icn;

import eu.point.bootstrapping.impl.BootstrappingServiceImpl;
import eu.point.bootstrapping.impl.switches.SwitchConfigurator;
import eu.point.bootstrapping.impl.topology.NetworkGraphImpl;
import eu.point.client.Client;
import eu.point.registry.impl.NodeConnectorRegistryUtils;
import eu.point.registry.impl.LinkRegistryUtils;
import eu.point.registry.impl.NodeRegistryUtils;
import eu.point.tmsdn.impl.TmSdnMessages;
import org.opendaylight.controller.md.sal.binding.api.DataBroker;
import org.opendaylight.yang.gen.v1.urn.eu.point.registry.rev150722.link.registry.LinkRegistryEntryBuilder;
import org.opendaylight.yang.gen.v1.urn.eu.point.registry.rev150722.node.connector.registry.NodeConnectorRegistryEntryBuilder;
import org.opendaylight.yang.gen.v1.urn.eu.point.registry.rev150722.node.registry.NodeRegistryEntryBuilder;
import org.opendaylight.yang.gen.v1.urn.tbd.params.xml.ns.yang.network.topology.rev131021.network.topology.topology.Link;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.net.Inet6Address;
import java.net.UnknownHostException;
import java.util.*;

/**
 * The ICN configurator class.
 * It includes all the required methods to map opendaylight/sdn information to ICN ones.
 *
 * @author George Petropoulos
 * @version cycle-2
 */
public class IcnIdConfigurator {

    private static final Logger LOG = LoggerFactory.getLogger(IcnIdConfigurator.class);
    public static String tmOpenflowId;
    public static String tmNodeId;
    public static String tmAttachedOpenflowSwitchId;
    public static int tmLidPosition;
    public static int tmInternalLidPosition;
    private static IcnIdConfigurator instance = null;
    private DataBroker db;

    /**
     * The constructor class.
     */
    private IcnIdConfigurator() {

    }

    /**
     * The method which sets the databroker.
     *
     * @param db The data broker object.
     * @see IcnIdConfigurator
     */
    public void setDb(DataBroker db) {
        this.db = db;
    }

    /**
     * The method which creates a single instance of the ICN Configurator class.
     * It also writes to the registry some initial required parameters,
     * e.g which host acts as TM, with which node id, and link id.
     *
     * @return It returns the icn id configurator instance.
     * @see IcnIdConfigurator
     */
    public static IcnIdConfigurator getInstance() {
        if (instance == null) {
            instance = new IcnIdConfigurator();
        }
        return instance;
    }

    /**
     * The method which adds links to the ICN topology.
     * If bootstrapping application is active, then for each link in the list,
     * it calls the relevant method.
     * Otherwise it adds them to the to-be-configured list.
     *
     * @param links The list of links to be added.
     * @see Link
     */
    public void addLinks(List<Link> links) {
        if (BootstrappingServiceImpl.applicationActivated) {
            LOG.info("Application active, hence generating LIDs for {}", links);
            for (Iterator<Link> iter = links.listIterator(); iter.hasNext(); ) {
                Link l = iter.next();
                if (!l.getLinkId().getValue().startsWith("host")) {
                    String sourceNodeId = l.getSource().getSourceNode().getValue();
                    String sourceNodeConnectorId = l.getSource().getSourceTp().getValue();
                    String dstNodeId = l.getDestination().getDestNode().getValue();
                    boolean linkAdded = addLink(sourceNodeId, dstNodeId, sourceNodeConnectorId);
                    if (linkAdded)
                        iter.remove();
                }
            }
        } else {
            LOG.info("Application not active, hence adding {} to unconfigured.", links);
            BootstrappingServiceImpl.unconfiguredLinks.addAll(links);
        }
    }

    /**
     * The method which adds multiple links to the ICN topology at the same time.
     * If bootstrapping application is active, then it sends a single request
     * to the resource manager for all these links.
     * Otherwise it adds them to the to-be-configured list.
     *
     * @param links The list of links to be added.
     * @see Link
     */
    public void addMultipleLinks(List<Link> links) {
        if (BootstrappingServiceImpl.applicationActivated) {
            LOG.info("Application active, hence generating LIDs for {}", links);
            List<TmSdnMessages.TmSdnMessage.ResourceRequestMessage.RecourceRequest> requests =
                    new ArrayList<>();

            for (Iterator<Link> iter = links.listIterator(); iter.hasNext(); ) {
                Link l = iter.next();
                if (!l.getLinkId().getValue().startsWith("host")) {
                    String sourceNodeId = l.getSource().getSourceNode().getValue();
                    String sourceNodeConnectorId = l.getSource().getSourceTp().getValue();
                    String dstNodeId = l.getDestination().getDestNode().getValue();
                    TmSdnMessages.TmSdnMessage.ResourceRequestMessage.RecourceRequest req =
                            prepareRequest(sourceNodeId, dstNodeId, sourceNodeConnectorId);
                    if (req != null) {
                        requests.add(req);
                        iter.remove();
                    }
                }
            }
            TmSdnMessages.TmSdnMessage.ResourceRequestMessage.Builder rrBuilder =
                    TmSdnMessages.TmSdnMessage.ResourceRequestMessage.newBuilder();
            rrBuilder.addAllRequests(requests);
            TmSdnMessages.TmSdnMessage message = TmSdnMessages.TmSdnMessage.newBuilder()
                    .setResourceRequestMessage(rrBuilder.build())
                    .setType(TmSdnMessages.TmSdnMessage.TmSdnMessageType.RR)
                    .build();
            TmSdnMessages.TmSdnMessage.ResourceOfferMessage resourceOffer =
                    new Client().sendMessage(message);
            LOG.debug("Received resource offer message {}", resourceOffer);

            for (int i = 0; i < resourceOffer.getOffersCount(); i++) {
                TmSdnMessages.TmSdnMessage.ResourceOfferMessage.RecourceOffer ro =
                        resourceOffer.getOffers(i);

                String sourceNodeId = requests.get(i).getSrcNode();
                String dstNodeId = requests.get(i).getDstNode();
                String sourceNodeConnectorId = requests.get(i).getNodeConnector();

                LOG.debug("Received the node link information {}: ", ro);
                String nodeid = ro.getNid();
                String genlid = ro.getLid();

                BootstrappingServiceImpl.assignedNodeIds.put(sourceNodeId, nodeid);
                BootstrappingServiceImpl.assignedLinkIds.put(sourceNodeId + "," + dstNodeId, genlid);

                //add given resources to the registries for future need
                NodeRegistryUtils.getInstance().writeToNodeRegistry(new NodeRegistryEntryBuilder()
                        .setNoneName(sourceNodeId)
                        .setNoneId(nodeid)
                        .build());
                LinkRegistryUtils.getInstance().writeToLinkRegistry(new LinkRegistryEntryBuilder()
                        .setLinkName(sourceNodeId + "," + dstNodeId)
                        .setLinkId(genlid)
                        .build());
                NodeConnectorRegistryUtils.getInstance().writeToNodeConnectorRegistry(
                        new NodeConnectorRegistryEntryBuilder()
                                .setNoneConnectorName(sourceNodeConnectorId)
                                .setSrcNode(sourceNodeId)
                                .setDstNode(dstNodeId)
                                .build());

                String lid = new StringBuilder(genlid).reverse().toString();
                String downLid = lid.substring(0, 128);
                String upLid = lid.substring(128, 256);
                String srcAddress = "";
                String dstAddress = "";
                try {
                    srcAddress = lidToIpv6(downLid);
                    dstAddress = lidToIpv6(upLid);
                    LOG.info("Src and dst ipv6 addresses {} and {}", srcAddress, dstAddress);
                } catch (Exception e) {
                    LOG.warn(e.getMessage());
                    return;
                }
                //configure switch with the ABM rule
                new SwitchConfigurator(db).addFlowConfig(sourceNodeId, sourceNodeConnectorId,
                        srcAddress, dstAddress);
            }
        } else {
            LOG.info("Application not active, hence adding {} to unconfigured.", links);
            BootstrappingServiceImpl.unconfiguredLinks.addAll(links);
        }
    }

    /**
     * The method which prepares a resource request for a single link.
     *
     * @param sourceNodeId The source node id of the link.
     * @param dstNodeId The destination node id of the link.
     * @param sourceNodeConnectorId The source node connector of the link.
     * @return The resource request.
     */
    private TmSdnMessages.TmSdnMessage.ResourceRequestMessage.RecourceRequest prepareRequest(
            String sourceNodeId, String dstNodeId, String sourceNodeConnectorId) {
        TmSdnMessages.TmSdnMessage.ResourceRequestMessage.RecourceRequest request = null;
        LOG.info("Adding link {}, {}, {}", sourceNodeId, dstNodeId, sourceNodeConnectorId);
        if (!BootstrappingServiceImpl.assignedLinkIds.containsKey(sourceNodeId + "," + dstNodeId)) {
            request = TmSdnMessages.TmSdnMessage.ResourceRequestMessage.RecourceRequest.newBuilder()
                    .setSrcNode(sourceNodeId)
                    .setDstNode(dstNodeId)
                    .setNodeConnector(sourceNodeConnectorId)
                    .build();
        }
        return request;
    }


    /**
     * The method which adds a single link to the ICN topology.
     * If it is not configured already, then it prepares a resource request,
     * sends it to the resource manager and prepares the arbitrary bitmask
     * rule and configures it to the switch.
     * It finally adds the assigned ids to the registry.
     *
     * @param sourceNodeId The source node id of the link.
     * @param dstNodeId The destination node id of the link.
     * @param sourceNodeConnectorId The source node connector of the link.
     * @return True or false, depending on the outcome of operation.
     */
    private boolean addLink(String sourceNodeId, String dstNodeId, String sourceNodeConnectorId) {
        LOG.info("Adding link {}, {}, {}", sourceNodeId, dstNodeId, sourceNodeConnectorId);
        if (!BootstrappingServiceImpl.assignedLinkIds.containsKey(sourceNodeId + "," + dstNodeId)) {
            LOG.info("Link not configured, generating parameters.");

            //if link is not configured, then request resources from resource manager
            List<TmSdnMessages.TmSdnMessage.ResourceRequestMessage.RecourceRequest> requests =
                    new ArrayList<>();
            requests.add(TmSdnMessages.TmSdnMessage.ResourceRequestMessage.RecourceRequest.newBuilder()
                    .setSrcNode(sourceNodeId)
                    .setDstNode(dstNodeId)
                    .build());
            TmSdnMessages.TmSdnMessage.ResourceRequestMessage.Builder rrBuilder =
                    TmSdnMessages.TmSdnMessage.ResourceRequestMessage.newBuilder();
            rrBuilder.addAllRequests(requests);

            TmSdnMessages.TmSdnMessage message = TmSdnMessages.TmSdnMessage.newBuilder()
                    .setResourceRequestMessage(rrBuilder.build())
                    .setType(TmSdnMessages.TmSdnMessage.TmSdnMessageType.RR)
                    .build();

            TmSdnMessages.TmSdnMessage.ResourceOfferMessage resourceOffer =
                    new Client().sendMessage(message);

            LOG.debug("Received resource offer {}", resourceOffer);
            if (resourceOffer == null || resourceOffer.getOffersList().size() == 0)
                return false;
            LOG.debug("Received the node link information {}: ", resourceOffer.getOffersList().get(0));
            String nodeid = resourceOffer.getOffersList().get(0).getNid();
            String genlid = resourceOffer.getOffersList().get(0).getLid();

            BootstrappingServiceImpl.assignedNodeIds.put(sourceNodeId, nodeid);
            BootstrappingServiceImpl.assignedLinkIds.put(sourceNodeId + "," + dstNodeId, genlid);
            LOG.info("Generated lid for source {} and dst {} is {} ", sourceNodeId, dstNodeId, genlid);

            //add given resources to the registries for future need
            NodeRegistryUtils.getInstance().writeToNodeRegistry(new NodeRegistryEntryBuilder()
                    .setNoneName(sourceNodeId)
                    .setNoneId(nodeid)
                    .build());
            LinkRegistryUtils.getInstance().writeToLinkRegistry(new LinkRegistryEntryBuilder()
                    .setLinkName(sourceNodeId + "," + dstNodeId)
                    .setLinkId(genlid)
                    .build());
            NodeConnectorRegistryUtils.getInstance().writeToNodeConnectorRegistry(
                    new NodeConnectorRegistryEntryBuilder()
                            .setNoneConnectorName(sourceNodeConnectorId)
                            .setSrcNode(sourceNodeId)
                            .setDstNode(dstNodeId)
                            .build());

            String lid = new StringBuilder(genlid).reverse().toString();
            String downLid = lid.substring(0, 128);
            String upLid = lid.substring(128, 256);
            String srcAddress = "";
            String dstAddress = "";
            try {
                srcAddress = lidToIpv6(downLid);
                dstAddress = lidToIpv6(upLid);
                LOG.info("Src and dst ipv6 addresses {} and {}", srcAddress, dstAddress);
            } catch (Exception e) {
                LOG.warn(e.getMessage());
                return false;
            }
            //configure switch with the ABM rule
            new SwitchConfigurator(db).addFlowConfig(sourceNodeId, sourceNodeConnectorId,
                    srcAddress, dstAddress);
        }
        return true;
    }

    /**
     * The method which removes links from the ICN topology.
     *
     * @param links The list of links to be added.
     * @see Link
     */
    public void removeLinks(List<Link> links) {
        LOG.info("Removing LIDs for {}", links);
        for (Link l : links) {
            if (!l.getLinkId().getValue().startsWith("host")) {
                String sourceNodeId = l.getSource().getSourceNode().getValue();
                String dstNodeId = l.getDestination().getDestNode().getValue();
                String sourceNodeConnectorId = l.getSource().getSourceTp().getValue();
                removeLink(sourceNodeId, dstNodeId, sourceNodeConnectorId);
            }
        }
    }

    /**
     * The method which removes a single link from the ICN topology.
     * @param sourceNodeId The source node id of the link.
     * @param dstNodeId The destination node id of the link.
     * @param sourceNodeConnectorId The source node connector of the link.
     */
    private void removeLink(String sourceNodeId, String dstNodeId, String sourceNodeConnectorId) {
        LOG.info("Removing link {}, {}, {}", sourceNodeId, dstNodeId, sourceNodeConnectorId);
        if (BootstrappingServiceImpl.assignedLinkIds.containsKey(sourceNodeId + "," + dstNodeId)) {
            LOG.info("Configured, removing parameters.");
            String lid = BootstrappingServiceImpl.assignedLinkIds.get(sourceNodeId + "," + dstNodeId);

            BootstrappingServiceImpl.assignedLinkIds.remove(sourceNodeId + "," + dstNodeId);

            //TODO check if node has more links, otherwise delete
            //NodeRegistryUtils.getInstance().deleteFromNodeRegistry(sourceNodeId);
            LinkRegistryUtils.getInstance().deleteFromLinkRegistry(sourceNodeId + "," + dstNodeId);
            NodeConnectorRegistryUtils.getInstance().deleteFromNodeConnectorRegistry(sourceNodeConnectorId);

            String downLid = lid.substring(0, 128);
            String upLid = lid.substring(128, 256);
            String srcAddress = "";
            String dstAddress = "";
            try {
                srcAddress = lidToIpv6(downLid);
                dstAddress = lidToIpv6(upLid);
                LOG.info("Src and dst ipv6 addresses {} and {}", srcAddress, dstAddress);
            } catch (Exception e) {
                LOG.warn(e.getMessage());
            }
            new SwitchConfigurator(db).deleteFlowConfig(sourceNodeId, sourceNodeConnectorId);
        }
    }

    /**
     * The method which creates a 256-bit lid from position of '1'.
     * @param pos The position of '1'.
     * @return The generated link id.
     */
    public static String generateLid(int pos) {
        String lid = "";
        for (int i = 0; i < 256; i++) {
            if (i == pos)
                lid += "1";
            else
                lid += "0";
        }
        LOG.info("Generated LID {}", lid);
        return lid;
    }

    /**
     * The method which generates a full-form IPv6 address from a LID.
     *
     * @param lid The link id to be translated to IPv6 address.
     * @return The IPv6 address to be configured as arbitrary bitmask rule.
     */
    private String lidToIpv6(String lid) throws UnknownHostException {
        byte[] ipv6Bytes = new byte[16];
        for (int i = 0; i < 16; i++) {
            String byteString = lid.substring(i * 8, i * 8 + 8);
            String bs = new StringBuilder(byteString).reverse().toString();
            ipv6Bytes[i] = (byte) Integer.parseInt(bs, 2);
        }
        String ipv6_hex = Inet6Address.getByAddress(ipv6Bytes).getHostAddress();
        return fullFormIpv6(ipv6_hex);
    }

    /**
     * The method which generates a full form IPv6 address from a given short-form one,
     * e.g from ::4 is mapped to 0000:0000:0000:...:0004
     *
     * @param shortIpv6 The short IPv6 address.
     * @return The long IPv6 address.
     */
    private static String fullFormIpv6(String shortIpv6) {
        String fullIpv6 = "";
        String[] strings = shortIpv6.split(":");
        for (int i = 0; i < strings.length; i++) {
            String full = String.format("%4s", strings[i]).replace(' ', '0');
            fullIpv6 += full;
            if (i != 7) {
                fullIpv6 += ":";
            }
        }
        return fullIpv6;
    }

    /**
     * The method which calculates the TMFID for a host with given MAC address
     * to reach the TM.
     * It assumes that TM is host:00:00:00:00:00:01. To be changed to not-hardcoded value.
     *
     * @param hostMac The mac address of the node which is added to the network and requires the FID to the TM.
     * @return The calculated TMFID.
     */
    public String calculateTMFID(String hostMac) {
        String tmfid = "";
        List<Link> shortestPath = NetworkGraphImpl.getInstance().getShortestPath(hostMac,
                tmOpenflowId);
        List<Integer> pathLids = new ArrayList<>();
        //Add TM internal LID always
        pathLids.add(tmInternalLidPosition);
        for (Link l : shortestPath) {
            String srcNode = l.getSource().getSourceNode().getValue();
            String dstNode = l.getDestination().getDestNode().getValue();
            int pos;
            if (BootstrappingServiceImpl.assignedLinkIds.containsKey(srcNode + "," + dstNode)) {
                String lid = BootstrappingServiceImpl.assignedLinkIds.get(srcNode + "," + dstNode);
                pos = lidBitPosition(lid);
                if (pos != -1)
                    pathLids.add(pos);
                LOG.info("Node connector {}, pos {}", srcNode + "," + dstNode, pos);
            }
        }

        LOG.info("TMFID lids {}", pathLids);

        for (int i = 0; i < 256; i++) {
            if (pathLids.contains(i))
                tmfid += "1";
            else
                tmfid += "0";
        }
        LOG.info("Generated TMFID {}", tmfid);
        return tmfid;
    }

    /**
     * The method which finds the position of '1' bit from a given LID.
     * @param lid The provided link id.
     * @return The position of '1' bit.
     */
    private int lidBitPosition(String lid) {
        char[] chararray = lid.toCharArray();
        for (int i = 0; i < chararray.length; i++) {
            if (chararray[i] == '1')
                return i;
        }
        return -1;
    }

    /**
     * The method which configures a single link manually.
     *
     * @param switchId The switch id
     * @param portId The port id
     * @param lidPosition The position of the link id which is configured.
     */
    public void addLinkManually(String switchId, String portId, int lidPosition) {
        LOG.info("Adding link manually for {}, {}, {}", switchId, portId, lidPosition);
        String genlid = generateLid(lidPosition);

        String lid = new StringBuilder(genlid).reverse().toString();
        String downLid = lid.substring(0, 128);
        String upLid = lid.substring(128, 256);
        String srcAddress = "";
        String dstAddress = "";
        try {
            srcAddress = lidToIpv6(downLid);
            dstAddress = lidToIpv6(upLid);
            LOG.info("Src and dst ipv6 addresses {} and {}", srcAddress, dstAddress);
        } catch (Exception e) {
            LOG.warn(e.getMessage());
        }
        //configure switch with the ABM rule
        new SwitchConfigurator(db).addFlowConfig(switchId, portId, srcAddress, dstAddress);
    }

}
