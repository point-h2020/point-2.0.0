/*
 * coap_proxy.h
 *
 *  Author: Hasan Mahmood Aminul Islam <hasan02.buet@gmail.com>
 *
 * This file is part of Blackadder.
 *
 * Blackadder is free software: you can redistribute it and/or modify it under the
 * terms of the GNU General Public License as published by the Free Software
 * Foundation, either version 3 of the License, or (at your option) any later
 * version.
 *
 * Blackadder is distributed in the hope that it will be useful, but WITHOUT ANY
 * WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR
 * A PARTICULAR PURPOSE. See the GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * Blackadder.  If not, see <http://www.gnu.org/licenses/>.
 */


#include <errno.h>
#include <sys/socket.h>
#include <netdb.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#include "utils.h"
#include "logger.h"

int timeval_subtract(struct timeval *result, const struct timeval *x, struct timeval *y)
{
	/* Perform the carry for the later subtraction by updating y. */
	if (x->tv_usec < y->tv_usec) {
		int nsec = (y->tv_usec - x->tv_usec) / 1000000 + 1;
		y->tv_usec -= 1000000 * nsec;
		y->tv_sec += nsec;
	}
	if (x->tv_usec - y->tv_usec > 1000000) {
		int nsec = (x->tv_usec - y->tv_usec) / 1000000;
		y->tv_usec += 1000000 * nsec;
		y->tv_sec -= nsec;
	}

	/* Compute the time remaining to wait.
	 *			tv_usec is certainly positive. */
	result->tv_sec = x->tv_sec - y->tv_sec;
	result->tv_usec = x->tv_usec - y->tv_usec;

	/* Return 1 if result is negative. */
	return x->tv_sec < y->tv_sec;
}


int compare_sockaddr(struct sockaddr_storage* a1, struct sockaddr_storage* a2)
{
  if( a1->ss_family != a2->ss_family ) return 1;
  if( a1->ss_family == AF_INET ) {
    if( ((struct sockaddr_in*)a1)->sin_port != ((struct sockaddr_in*)a2)->sin_port ) return 2;
    if( ((struct sockaddr_in*)a1)->sin_addr.s_addr != ((struct sockaddr_in*)a2)->sin_addr.s_addr ) return 3;
    return 0;
  }
  if( a1->ss_family == AF_INET6 ) {
    if( ((struct sockaddr_in6*)a1)->sin6_port != ((struct sockaddr_in6*)a2)->sin6_port ) return 4;
    /* TODO Compare IPv6 addresses */
    return 0;
  }
  return 6;
}


