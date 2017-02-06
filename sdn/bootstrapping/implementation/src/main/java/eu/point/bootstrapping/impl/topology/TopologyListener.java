/*
 * Copyright © 2016 George Petropoulos.  All rights reserved.
 *
 * This program and the accompanying materials are made available under the
 * terms of the Eclipse Public License v1.0 which accompanies this distribution,
 * and is available at http://www.eclipse.org/legal/epl-v10.html
 */
package eu.point.bootstrapping.impl.topology;

import eu.point.bootstrapping.impl.icn.IcnIdConfigurator;
import org.opendaylight.controller.md.sal.binding.api.DataChangeListener;
import org.opendaylight.controller.md.sal.common.api.data.AsyncDataChangeEvent;
import org.opendaylight.yang.gen.v1.urn.opendaylight.flow.service.rev130819.SalFlowService;
import org.opendaylight.yang.gen.v1.urn.tbd.params.xml.ns.yang.network.topology.rev131021.network.topology.topology.Link;
import org.opendaylight.yangtools.yang.binding.DataObject;
import org.opendaylight.yangtools.yang.binding.InstanceIdentifier;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.Set;

/**
 * The class which listens for topology changes.
 *
 * @author George Petropoulos
 * @version cycle-2
 */
public class TopologyListener implements DataChangeListener {

    private static final Logger LOG = LoggerFactory.getLogger(TopologyListener.class);
    private SalFlowService salFlowService;

    /**
     * The constructor class.
     */
    public TopologyListener() {
        LOG.info("Initializing topology listener.");
    }

    /**
     * The method which monitors changes in data store for topology.
     * @param dataChangeEvent The data change event, including the new, updated or deleted data.
     */
    @Override
    public void onDataChanged(AsyncDataChangeEvent<InstanceIdentifier<?>, DataObject> dataChangeEvent) {
        List<Link> linkList;

        if (dataChangeEvent == null) {
            return;
        }
        Map<InstanceIdentifier<?>, DataObject> createdData = dataChangeEvent.getCreatedData();
        Set<InstanceIdentifier<?>> removedPaths = dataChangeEvent.getRemovedPaths();
        Map<InstanceIdentifier<?>, DataObject> originalData = dataChangeEvent.getOriginalData();

        if (createdData != null && !createdData.isEmpty()) {
            Set<InstanceIdentifier<?>> linksIds = createdData.keySet();
            linkList = new ArrayList<>();
            for (InstanceIdentifier<?> linkId : linksIds) {
                if (Link.class.isAssignableFrom(linkId.getTargetType())) {
                    Link link = (Link) createdData.get(linkId);
                    LOG.info("Graph is updated! Added Link {}", link.getLinkId().getValue());
                    linkList.add(link);
                }
            }
            NetworkGraphImpl.getInstance().addLinks(linkList);
            IcnIdConfigurator.getInstance().addLinks(linkList);
        }

        if (removedPaths != null && !removedPaths.isEmpty() && originalData != null && !originalData.isEmpty()) {
            linkList = new ArrayList<>();
            for (InstanceIdentifier<?> instanceId : removedPaths) {
                if (Link.class.isAssignableFrom(instanceId.getTargetType())) {
                    Link link = (Link) originalData.get(instanceId);
                    LOG.info("Graph is updated! Removed Link {}", link.getLinkId().getValue());
                    linkList.add(link);
                }
            }
            NetworkGraphImpl.getInstance().removeLinks(linkList);
            IcnIdConfigurator.getInstance().removeLinks(linkList);
        }
    }
}