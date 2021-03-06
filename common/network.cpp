
/*
 * CloudVPN
 *
 * This program is a free software: You can redistribute and/or modify it
 * under the terms of GNU GPLv3 license, or any later version of the license.
 * The program is distributed in a good hope it will be useful, but without
 * any warranty - see the aforementioned license for more details.
 * You should have received a copy of the license along with this program;
 * if not, see <http://www.gnu.org/licenses/>.
 */

#include "network.h"
#define LOGNAME "common/network"
#include "log.h"
#include "conf.h"
#include "security.h"

#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>


static bool tcp_nodelay = false;
static int ip_tos = 0;
static int listen_backlog_size = 32;

bool sock_nonblock (int fd)
{
#ifndef __WIN32__
	return fcntl (fd, F_SETFL, O_NONBLOCK) >= 0;
#else
	u_long a = 1;
	return ioctlsocket (fd, FIONBIO, &a) >= 0;
#endif
}

int tcp_socket_writeable (int sock)
{
	fd_set s;
	struct timeval t = {0, 0};
	FD_ZERO (&s);
	FD_SET (sock, &s);
	select (sock + 1, 0, &s, 0, &t);
	if (FD_ISSET (sock, &s) ) return 1;
	return 0;
}

int network_init()
{
#ifndef __WIN32__
	tcp_nodelay = config_is_true ("tcp_nodelay");
	if (tcp_nodelay) Log_info ("TCP_NODELAY is set for all sockets");
	string t;
	if (!config_get ("ip_tos", t) ) goto no_tos;
	if (t == "lowdelay") ip_tos = IPTOS_LOWDELAY;
	else if (t == "throughput") ip_tos = IPTOS_THROUGHPUT;
	else if (t == "reliability") ip_tos = IPTOS_RELIABILITY;
#ifdef IPTOS_MINCOST  //not available on some platforms.
	else if (t == "mincost") ip_tos = IPTOS_MINCOST;
#endif
	if (ip_tos) Log_info ("type of service is `%s' for all sockets",
		                      t.c_str() );
#endif
no_tos:
	int i;
	if (config_get_int ("listen_backlog", i) ) listen_backlog_size = i;
	Log_info ("listen backlog size is %d", listen_backlog_size);

	return 0;
}

int sockoptions_set (int s)
{
#ifndef __WIN32__
	int t;
	if (tcp_nodelay) {
		t = 1;
		if (setsockopt (s, IPPROTO_TCP, TCP_NODELAY, &t, sizeof (t) ) )
			Log_warn ( "setsockopt(%d,TCP,NODELAY) failed with %d: %s",
			           s, errno, strerror (errno) );
	}
	if (ip_tos) {
		t = ip_tos;
		if (setsockopt (s, IPPROTO_IP, IP_TOS, &t, sizeof (t) ) )
			Log_warn ("setsockopt(%d,IP,TOS) failed with %d: %s",
			          s, errno, strerror (errno) );
	}
#endif
	return 0;
}


/*
 * raw network stuff
 *
 * backends to listen/connect/accept network operations
 */

int tcp_listen_socket (const char* addr)
{
	sockaddr_type sa;
	int sa_len, domain;
	if (!sockaddr_from_str (addr, & (sa.sa), &sa_len, &domain) ) {
		Log_error ("could not resolve address and port `%s'", addr);
		return -1;
	}

	int s = socket (domain, SOCK_STREAM, 0);

	if (s < 0) {
		Log_error ("socket() failed with %d: %s",  errno, strerror (errno) );
		return -2;
	}

	int opt = 1;
	if (setsockopt (s, SOL_SOCKET, SO_REUSEADDR,
#ifdef __WIN32__
	                (const char*)
#endif
	                &opt, sizeof (opt) ) < 0)
		Log_warn ("setsockopt(%d,SO_REUSEADDR) failed, may cause errors.", s);

	if (!sock_nonblock (s) ) {
		Log_error ("can't set socket %d to nonblocking mode", s);
		close (s);
		return -3;
	}

	if (bind (s, & (sa.sa), sa_len) ) {
		Log_error ("binding socket %d failed with %d: %s", s, errno, strerror (errno) );
		close (s);
		return -4;
	}

	if (domain == AF_UNIX)
		fix_file_owner (addr);

	if (listen (s, listen_backlog_size) ) {
		Log_error ("listen(%d,%d) failed with %d: %s",
		           s, listen_backlog_size, errno, strerror (errno) );
		close (s);
		return -5;
	}

	Log_info ("created listening socket %d", s);

	return s;
}

int tcp_connect_socket (const char*addr)
{
	sockaddr_type sa;
	int sa_len, domain;
	if (!sockaddr_from_str (addr, & (sa.sa), &sa_len, &domain) ) {
		Log_error ("could not resolve address and port `%s'", addr);
		return -1;
	}

	int s = socket (domain, SOCK_STREAM, 0);

	if (s < 0) {
		Log_error ("socket() failed with %d: %s", errno, strerror (errno) );
		return -2;
	}

	if (!sock_nonblock (s) ) {
		Log_error ("can't set socket %d to nonblocking mode", s);
		close (s);
		return -3;
	}

	sockoptions_set (s);

	if (connect (s, & (sa.sa), sa_len) < 0) {
		int e = errno;
		if ( e && (e != EINPROGRESS)

#ifdef __WIN32__
		        /*
		         * Win32 developer poem about the forgotten ifdef:
		         *
		         * To anyone who first explains
		         * this Win-Thirty-Two wonder of the year,
		         * I owe a beer.
		         */
		        && (e != 2) //also, it works now.
#endif


		   ) {
			Log_error ("connect(%d) to `%s' failed with %d",
			           s, addr, e);
			close (s);
			return -4;
		}
	}

	return s;
}

int tcp_close_socket (int sock, bool do_unlink)
{
#ifndef __WIN32__
	if (do_unlink) { //we need to unlink the sockfile
		sockaddr_type sa;
		socklen_t sa_len = sizeof (sockaddr_type);
		if ( (!getsockname (sock, & (sa.sa), &sa_len) )
		        && (sa.sa.sa_family == AF_UNIX) )
			if (unlink (sa.sa_un.sun_path) ) {
				Log_warn ("could not unlink socket %s",
				          sa.sa_un.sun_path);
			}
	}
#endif
	if (close (sock) ) {
		Log_warn ("closing socket %d failed with %d: %s", sock, errno, strerror (errno) );
		return 1;
	}
	return 0;
}


/*
 * sockaddr conversion
 */

const char* sockaddr_to_str (struct sockaddr*addr)
{
#ifndef __WIN32__
	static char buf[256];
	const void*t;
	int port;
	switch (addr->sa_family) {
	case AF_INET:
		t = (const void*) & ( ( (sockaddr_in*) addr)->sin_addr);
		port = ntohs ( ( (sockaddr_in*) addr)->sin_port);
		break;
	case AF_INET6:
		t = (const void*) & ( ( (sockaddr_in6*) addr)->sin6_addr);
		port = ntohs ( ( (sockaddr_in6*) addr)->sin6_port);
		break;
	case AF_UNIX:
		strcpy (buf, "\\local");
		return buf;
	default:
		return 0;
	}

	if (!inet_ntop (addr->sa_family, t, buf, 254) ) return 0;
	snprintf (buf + strlen (buf), 16, " %d", port);
	return buf;
#else
	return "(?)";
#endif
}


bool sockaddr_from_str (const char *str,
                        struct sockaddr*addr, int*len, int*sock_domain)
{
	if (!str) return false;

#ifndef __WIN32__
	//Local socket is distinguished by having a '/' in name.
	//For sockets without '/' please use ./some.sock
	//TODO: UNIX_MAX_PATH and similar need to be determined in a better way

	if (strchr (str, '/') ) {
		if (strlen (str) > sizeof ( ( (sockaddr_un*) addr)->sun_path) ) {
			Log_error ("path too long for unix socket: %s", str);
			return false;
		}

		if (len) *len = sizeof (struct sockaddr_un);
		if (sock_domain) *sock_domain = AF_UNIX;
		addr->sa_family = AF_UNIX;
		( (sockaddr_un*) addr)->sun_family = AF_UNIX;
		strncpy ( ( (sockaddr_un*) addr)->sun_path,
		          str, sizeof ( ( (sockaddr_un*) addr)->sun_path) );
		return true;
	}
#endif

	char ip_buf[1025], port_buf[65]; //which should be enough for everyone.

	if (! (str && addr) ) return false;

	if (sscanf (str, " %1024s %64s", ip_buf, port_buf) < 2) return false;

	struct addrinfo hints, *res;
	memset (&hints, 0, sizeof (struct addrinfo) );
	hints.ai_socktype = SOCK_STREAM;

	int ret = getaddrinfo (ip_buf, port_buf, &hints, &res);
	if (ret) {
		Log_error ("getaddrinfo failed for entry `%s %s'", ip_buf, port_buf);
		Log_error ("reason was: %d: %s", ret, gai_strerror (ret) );
		return false;
	}

	if (len) *len = res->ai_addrlen;
	if (sock_domain) *sock_domain = res->ai_family;

	memcpy (addr, res->ai_addr, res->ai_addrlen);

	freeaddrinfo (res);

	return true;
}

int sock_get_error (int fd)
{
	int e;
	socklen_t e_len = sizeof (e);
	if (getsockopt (fd, SOL_SOCKET, SO_ERROR,
#ifdef __WIN32__
	                (char*)
#endif
	                & e, &e_len) ) return -666;
	if (e == EINPROGRESS) return 1;
	if (!e) return 0;
	Log_warn ("socket %d error %d", fd, e);
	return -e;
}


#ifdef __WIN32__
//WSA initializator
static struct wsa_init {
	WSADATA wd;
	wsa_init() {
		if (WSAStartup (MAKEWORD (2, 0), &wd) ) {
			Log_error ("WSAStartup somehow failed!");
			Log_error ("WSAError is: %d", WSAGetLastError() );
		}
	}
	~wsa_init() {
		WSACleanup();
	}
} _WSA_initializator_;
#endif
