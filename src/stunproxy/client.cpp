
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

/*
 * Stun Proxy, also known as stunconn. Part of CloudVPN.
 *
 * configuration:
 *
 * listen, forward: local addresses to connect
 * key: shared key to use for stunconn
 * stunconn: some address to stunconn server.
 */

#include "sq.h"
#define LOGNAME "stunproxy"
#include "log.h"
#include "conf.h"
#include "network.h"
#include "timestamp.h"

#include <stdint.h>

#include <map>
using namespace std;

int udp_fd, proxy_fd;

class conn
{
public:
	int fd;
	int id;

	//sending part
	uint32_t next_send_packet;
	uint64_t last_send_time;
	map<uint32_t, pbuffer> waiting_acks;

	//receiving part
	uint32_t next_recv_packet;
	uint64_t last_recv_time;
	map<uint32_t, pbuffer>recvd;
};

map<uint32_t, conn>conns;

uint64_t last_udp_received;

int handle_udp()
{

}

int handle_connect()
{

}

int handle_accept()
{

}

int get_external_ip()
{

}

int get_peer_ip()
{

}

int wait_and_poll()
{

}

int init()
{

}

int shutdown()
{

}

int main()
{
	return 0;
}
