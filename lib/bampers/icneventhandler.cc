/*
 * icneventhandler.cc
 *
 *  Created on: Dec 6, 2015
 *      Author: Sebastian Robitzsch <sebastian.robitzsch@interdigital.com>
 *
 * This file is part of BlAckadder Monitoring wraPpER clasS (BAMPERS).
 *
 * BAMPERS is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * BAMPERS is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * BAMPERS. If not, see <http://www.gnu.org/licenses/>.
 */

#include "icneventhandler.hh"

IcnEventHandler::IcnEventHandler(Namespace &namespaceHelper)
	: _namespaceHelper(namespaceHelper)
{
}
IcnEventHandler::~IcnEventHandler()
{

}
void IcnEventHandler::operator ()()
{
	cout << "ICN event handler thread started\n";
	Blackadder *blackadder = _namespaceHelper.getBlackadderInstance();
	while (true) {
		Event ev;
		string scopePath;
		blackadder->getEvent(ev);
		scopePath = chararray_to_hex(ev.id);
		switch (ev.type) {
			case SCOPE_PUBLISHED:
				cout << "SCOPE_PUBLISHED received for scope path " << scopePath
				<< endl;
				break;
			case SCOPE_UNPUBLISHED:
				cout << "SCOPE_UNPUBLISHED received for scope path "
				<< scopePath << endl;
				_namespaceHelper.removePublishedScopePath(scopePath);
				break;
			case START_PUBLISH:
			{
				uint8_t *data = (uint8_t *)malloc(1500);
				uint32_t dataLength;
				cout << "START_PUBLISH received for scope path " << scopePath
						<< endl;
				_namespaceHelper.setSubscriberAvailability(scopePath, true);
				if (_namespaceHelper.getDataFromBuffer(scopePath, data,
						dataLength))
				{
					blackadder->publish_data(ev.id, DOMAIN_LOCAL, NULL, 0, data,
							dataLength);
					cout << "Buffered data of length " << dataLength
							<< " published under scope path " << scopePath
							<< endl;
				}
				free(data);
				break;
			}
			case STOP_PUBLISH:
			{
				cout << "STOP_PUBLISH received for scope path" << scopePath
						<< endl;
				_namespaceHelper.setSubscriberAvailability(scopePath, false);
				break;
			}
			case PUBLISHED_DATA:
			{
				cout << "PUBLISHED_DATA under scope " << scopePath << endl;
				int sendingSocket;
				struct sockaddr_nl bampersAddr, molyAddress;
				struct nlmsghdr *nlh = NULL;
				struct iovec iov;
				struct msghdr msg;
				int ret;
				sendingSocket = socket(AF_NETLINK, SOCK_RAW, NETLINK_GENERIC);
				if (sendingSocket < 0)
				{
					cout<< "Socket towards MOLY::Bootstrapping could not be "
							"created\n";
					return;
				}
				cout << "Socket towards MOLY::Bootstrapping created\n";
				memset(&msg, 0, sizeof(msg));
				memset(&bampersAddr, 0, sizeof(bampersAddr));
				memset(&molyAddress, 0, sizeof(molyAddress));
				bampersAddr.nl_family = AF_NETLINK;
				bampersAddr.nl_pid = 0;
				bampersAddr.nl_pad = 0;
				bampersAddr.nl_groups = 0;
				ret = bind(sendingSocket, (struct sockaddr*) &bampersAddr,
						sizeof(bampersAddr));
				if (ret == -1)
				{
					cout << "Could not bind to socket towards "
							"MOLY::Agent::bootstrapApplications()\n";
					close(sendingSocket);
					return;
				}
				//DST netlink socket
				molyAddress.nl_family = AF_NETLINK;
				molyAddress.nl_pid = PID_MOLY_BOOTSTRAP_LISTENER;
				molyAddress.nl_groups = 0;
				molyAddress.nl_pad = 0;
				/* Allocate the required memory (NLH + payload) */
				nlh= (struct nlmsghdr *) malloc(NLMSG_SPACE(
						sizeof(bampersAddr.nl_pid)));
				/* Fill the netlink message header */
				nlh->nlmsg_len = NLMSG_SPACE(sizeof(bampersAddr.nl_pid));
				nlh->nlmsg_pid = bampersAddr.nl_pid;
				nlh->nlmsg_flags = NLM_F_REQUEST;
				nlh->nlmsg_type = BOOTSTRAP_SERVER_UP;
				nlh->nlmsg_seq = rand();
				memcpy(NLMSG_DATA(nlh), &bampersAddr.nl_pid,
						sizeof(bampersAddr.nl_pid));
				iov.iov_base = (void *)nlh;
				iov.iov_len = nlh->nlmsg_len;
				// Creating message
				msg.msg_name = (void *)&molyAddress;
				msg.msg_namelen = sizeof(molyAddress);
				msg.msg_iov = &iov;
				msg.msg_iovlen = 1;
				ret = sendmsg(sendingSocket, &msg, 0);
				if (ret == -1)
				{
					cout << "MOLY::Agent::bootstrapApplications() thread could"
							" not be informed\n";
				}
				cout << "MOLY::Agent::bootstrapApplications() thread informed "
						"that monitoring server is up\n";
				close(sendingSocket);
				break;
			}
			default:
				cout<<"unknown event\n";
		}
	}
}
