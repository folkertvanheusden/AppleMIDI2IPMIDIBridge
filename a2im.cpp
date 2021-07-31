#include <atomic>
#include <stdio.h>
#include <string_view>
#include <thread>
#include <unistd.h>
#include <rtpmidid/iobytes.hpp>
#include <rtpmidid/poller.hpp>
#include <rtpmidid/rtpserver.hpp>
#include <sys/socket.h>
#include <sys/types.h>

#include "mdns_rtpmidi.hpp"


void send(rtpmidid::rtpserver *const am, const uint8_t *msg, const size_t len)
{
	rtpmidid::io_bytes b((uint8_t *)msg, len);

	am->send_midi_to_all_peers(rtpmidid::io_bytes_reader(b));
}

void poller_thread()
{
	while (rtpmidid::poller.is_open())
		rtpmidid::poller.wait();
}

void a2i_thread(rtpmidid::rtpserver *const am, const int fd, const struct sockaddr_in *const ipmidi_mc_addr, std::atomic_uint32_t *const counter)
{
	am->midi_event.connect([fd, ipmidi_mc_addr, counter](rtpmidid::io_bytes_reader buffer) {
			size_t len = buffer.size();

			uint8_t *msg = (uint8_t *)malloc(len);

			for(size_t i=0; i<len; i++)
			msg[i] = buffer.read_uint8();

			if (sendto(fd, msg, len, 0, (struct sockaddr *)ipmidi_mc_addr, sizeof(*ipmidi_mc_addr)) == -1)
				perror("sendto");

			(*counter)++;

			free(msg);
			});

	for(;;)
		sleep(1);
}

void i2a_thread(const int fd, rtpmidid::rtpserver *const am, std::atomic_uint32_t *const counter)
{
	uint8_t buffer[128];  // more is silly

	for(;;) {
		int recv_len = -1;

		if ((recv_len = recvfrom(fd, buffer, sizeof buffer, 0, nullptr, nullptr)) == -1) {
			perror("recvfrom");
			exit(1);
		}

		send(am, buffer, recv_len);

		(*counter)++;
	}
}

void usage()
{
	printf("-p x   port to listen on for AppleMidi (RTPMIDI, hopefully no need to configure this)\n");
	printf("-a x   IPv4 multicast address for ipmidi\n");
	printf("-P x   multicast port for ipmidi\n");
	printf("-V     show version\n");
	printf("-h     this help\n");
}

int main(int argc, char *argv[])
{
	std::string port = "15115", name = "AppleMidi2IPMidiBridge";
	const char *ipmidi_addr = "225.0.0.37";
	int ipmidi_port = 21928;

	int c = -1;
	while((c = getopt(argc, argv, "p:a:P:Vh")) != -1) {
		if (c == 'p')
			port = optarg;
		else if (c == 'a')
			ipmidi_addr = optarg;
		else if (c == 'P')
			ipmidi_port = atoi(optarg);
		else if (c == 'V') {
			printf("%s (C) 2021 by Folkert van Heusden\n", name.c_str());
			return 0;
		}
		else if (c == 'h') {
			usage();
			return 0;
		}
		else {
			usage();
			return 1;
		}
	}

	int fd = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (fd == -1) {
		perror("socket");
		return 1;
	}

	struct sockaddr_in ipmidi_mc_addr;
	memset(&ipmidi_mc_addr, 0, sizeof(ipmidi_mc_addr));
	ipmidi_mc_addr.sin_family = AF_INET;
	ipmidi_mc_addr.sin_addr.s_addr = htonl(INADDR_ANY);
	ipmidi_mc_addr.sin_port = htons(ipmidi_port);

	if ((bind(fd, (struct sockaddr *)&ipmidi_mc_addr, sizeof(ipmidi_mc_addr))) == -1) {
		perror("bind");
		return 1;
	}

	struct ip_mreq m;
	m.imr_interface.s_addr = htonl(INADDR_ANY);
	m.imr_multiaddr.s_addr = inet_addr(ipmidi_addr);
	if (setsockopt(fd, IPPROTO_IP, IP_ADD_MEMBERSHIP, (char *)&m, sizeof(m)) == -1) {
		perror("setsockopt(IP_ADD_MEMBERSHIP)");
		return 1;
	}

	int flag_on = 1;
	if ((setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, (char*)&flag_on, sizeof(flag_on))) == -1)
		perror("setsockopt(SO_REUSEADDR)");

	rtpmidid::mdns_rtpmidi mdns_rtpmidi;

	rtpmidid::rtpserver *am = new rtpmidid::rtpserver(name, port);

	mdns_rtpmidi.announce_rtpmidi(name, atoi(port.c_str()));

	am->connected_event.connect([port](std::shared_ptr<::rtpmidid::rtppeer> peer) {
		INFO("Remote client connects to local server at port {}. Name: {}", atoi(port.c_str()), peer->remote_name);
	});

	std::thread poller(poller_thread);

	std::atomic_uint32_t a2i_counter { 0 };
	std::thread a2i(a2i_thread, am, fd, &ipmidi_mc_addr, &a2i_counter);

	std::atomic_uint32_t i2a_counter { 0 };
	std::thread i2a(i2a_thread, fd, am, &i2a_counter);

	for(;;) {
		sleep(1);

		printf("a2i: %u, i2a: %u\n", a2i_counter.load(), i2a_counter.load());
	}

	return 0;
}
