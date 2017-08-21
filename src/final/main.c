#define _GNU_SOURCE
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <wait.h>

#include <linux/rtnetlink.h>
#include <linux/netlink.h>
#include <net/if.h>
#include <arpa/inet.h>

#include "fw.h"
#include "nl.h"
#include "namespace.h"

int main(void)
{
	int fd;
	if ((fd = _nl_socket_init()) == 0)
		exit(1);

	struct nlmsghdr *nlmsg = malloc(4096);
	memset(nlmsg, 0, 4096);
	nlmsg->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	nlmsg->nlmsg_type  = RTM_NEWLINK;
	nlmsg->nlmsg_flags = NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL|NLM_F_ACK;
	nlmsg->nlmsg_seq   = time(NULL);

	struct ifinfomsg *ifmsg;
	ifmsg = (struct ifinfomsg *) NLMSG_DATA(nlmsg);
	ifmsg->ifi_family = AF_UNSPEC;

	struct rtattr *nest1, *nest2, *nest3;

	NLMSG_STRING(nlmsg, IFLA_IFNAME, "veth1");

	nest1 = NLMSG_TAIL(nlmsg);
	NLMSG_ATTR(nlmsg, IFLA_LINKINFO);

	NLMSG_STRING(nlmsg, IFLA_INFO_KIND, "veth");

	nest2 = NLMSG_TAIL(nlmsg);
	NLMSG_ATTR(nlmsg, IFLA_INFO_DATA);

	nest3 = NLMSG_TAIL(nlmsg);
	NLMSG_ATTR(nlmsg, VETH_INFO_PEER);

	nlmsg->nlmsg_len += sizeof(struct ifinfomsg);

	NLMSG_STRING(nlmsg, IFLA_IFNAME, "vpeer1");

	nest3->rta_len = (unsigned char *)NLMSG_TAIL(nlmsg) - (unsigned char *)nest3;
	nest2->rta_len = (unsigned char *)NLMSG_TAIL(nlmsg) - (unsigned char *)nest2;
	nest1->rta_len = (unsigned char *)NLMSG_TAIL(nlmsg) - (unsigned char *)nest1;

	if (_nlmsg_send(fd, nlmsg) != 0)
		exit(1);

	if (_nlmsg_recieve(fd) != 0) {
		close(fd);
		exit(1);
	}

	int addrlen = sizeof(struct in_addr);
	free(nlmsg);
	nlmsg = malloc(4096);
	memset(nlmsg, 0, 4096);
	nlmsg->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
	nlmsg->nlmsg_flags = NLM_F_ACK|NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL;
	nlmsg->nlmsg_type  = RTM_NEWADDR;
	nlmsg->nlmsg_seq   = time(NULL);

	struct ifaddrmsg *ifa;
	ifa = (struct ifaddrmsg *) NLMSG_DATA(nlmsg);
	ifa->ifa_prefixlen = atoi("24");
	if (!(ifa->ifa_index = if_nametoindex("veth1"))) {
		printf("failed to get veth1 name: %s\n", strerror(errno));
		return 1;
	}
	ifa->ifa_family = AF_INET;
	ifa->ifa_scope = 0;

	struct in_addr addr;
	struct in_addr bcast;

	if (inet_pton(AF_INET, "172.16.1.1", &addr) < 0)
		exit(1);
	if (inet_pton(AF_INET, "172.16.1.255", &bcast) < 0)
		exit(1);

	_nlmsg_put(nlmsg, IFA_LOCAL,     &addr,  addrlen);
	_nlmsg_put(nlmsg, IFA_ADDRESS,   &addr,  addrlen);
	_nlmsg_put(nlmsg, IFA_BROADCAST, &bcast, addrlen);

	if (_nlmsg_send(fd, nlmsg) != 0)
		exit(1);
	if (_nlmsg_recieve(fd) != 0) {
		close(fd);
		exit(1);
	}

	pid_t child;
	if ((child = fork()) == 0) {
		if (new_ns("ns1") != 0)
			exit(1);
		exit(0);
	}
	int rc;
	waitpid(child, &rc, 0);
	if (rc != 0)
		fprintf(stderr, "namespace not created correctly\n");

	free(nlmsg);
	nlmsg = malloc(4096);
	memset(nlmsg, 0, 4096);
	nlmsg->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	nlmsg->nlmsg_type  = RTM_NEWLINK;
	nlmsg->nlmsg_flags = NLM_F_REQUEST|NLM_F_ACK;
	nlmsg->nlmsg_seq   = time(NULL);

	ifmsg = (struct ifinfomsg *) NLMSG_DATA(nlmsg);
	ifmsg->ifi_family = AF_UNSPEC;
	if (!(ifmsg->ifi_index = if_nametoindex("vpeer1"))) {
		fprintf(stderr, "failed to get vpeer1 name: %s\n", strerror(errno));
		return 1;
	}
	int nsfd = netns_get_fd("ns1");
	_nlmsg_put(nlmsg, IFLA_NET_NS_FD, &nsfd, sizeof(nsfd));

	if (_nlmsg_send(fd, nlmsg) != 0)
		exit(1);
	if (_nlmsg_recieve(fd) != 0) {
		close(fd);
		exit(1);
	}

	free(nlmsg);
	nlmsg = malloc(4096);
	memset(nlmsg, 0, 4096);
	nlmsg->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	nlmsg->nlmsg_flags = NLM_F_ACK|NLM_F_REQUEST;
	nlmsg->nlmsg_type  = RTM_NEWLINK;
	nlmsg->nlmsg_seq   = time(NULL);

	ifmsg = (struct ifinfomsg *) NLMSG_DATA(nlmsg);
	ifmsg->ifi_family  = AF_UNSPEC;
	ifmsg->ifi_change |= IFF_UP;
	ifmsg->ifi_flags  |= IFF_UP;
	if (!(ifmsg->ifi_index = if_nametoindex("veth1"))) {
		printf("failed to get veth1 name: %s\n", strerror(errno));
		return 1;
	}

	if (_nlmsg_send(fd, nlmsg) != 0)
		exit(1);
	if (_nlmsg_recieve(fd) != 0) {
		close(fd);
		exit(1);
	}

	if ((child = fork()) == 0) {
		setns(nsfd, CLONE_NEWNET);

		if ((fd = _nl_socket_init()) == 0)
			exit(1);

		free(nlmsg);
		nlmsg = malloc(4096);
		memset(nlmsg, 0, 4096);
		nlmsg->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifaddrmsg));
		nlmsg->nlmsg_flags = NLM_F_ACK|NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL;
		nlmsg->nlmsg_type  = RTM_NEWADDR;
		nlmsg->nlmsg_seq   = time(NULL);

		struct ifaddrmsg *ifa;
		ifa = (struct ifaddrmsg *) NLMSG_DATA(nlmsg);
		ifa->ifa_prefixlen = atoi("24");
		if (!(ifa->ifa_index = if_nametoindex("vpeer1"))) {
			printf("failed to get veth1 name: %s\n", strerror(errno));
			exit(1);
		}
		ifa->ifa_family = AF_INET;
		ifa->ifa_scope = 0;

		struct in_addr addr;
		struct in_addr bcast;

		if (inet_pton(AF_INET, "172.16.1.2", &addr) < 0)
			exit(1);
		if (inet_pton(AF_INET, "172.16.1.255", &bcast) < 0)
			exit(1);

		_nlmsg_put(nlmsg, IFA_LOCAL,     &addr,  addrlen);
		_nlmsg_put(nlmsg, IFA_ADDRESS,   &addr,  addrlen);
		_nlmsg_put(nlmsg, IFA_BROADCAST, &bcast, addrlen);

		if (_nlmsg_send(fd, nlmsg) != 0)
			exit(1);
		if (_nlmsg_recieve(fd) != 0) {
			close(fd);
			exit(1);
		}

		free(nlmsg);
		nlmsg = malloc(4096);
		memset(nlmsg, 0, 4096);
		nlmsg->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
		nlmsg->nlmsg_flags = NLM_F_ACK|NLM_F_REQUEST;
		nlmsg->nlmsg_type  = RTM_NEWLINK;
		nlmsg->nlmsg_seq   = time(NULL);

		ifmsg = (struct ifinfomsg *) NLMSG_DATA(nlmsg);
		ifmsg->ifi_family  = AF_UNSPEC;
		ifmsg->ifi_change |= IFF_UP;
		ifmsg->ifi_flags  |= IFF_UP;
		if (!(ifmsg->ifi_index = if_nametoindex("vpeer1"))) {
			printf("failed to get veth1 name: %s\n", strerror(errno));
			exit(1);
		}

		if (_nlmsg_send(fd, nlmsg) != 0)
			exit(1);
		if (_nlmsg_recieve(fd) != 0) {
			close(fd);
			exit(1);
		}

		free(nlmsg);
		nlmsg = malloc(4096);
		memset(nlmsg, 0, 4096);
		nlmsg->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
		nlmsg->nlmsg_flags = NLM_F_ACK|NLM_F_REQUEST;
		nlmsg->nlmsg_type  = RTM_NEWLINK;
		nlmsg->nlmsg_seq   = time(NULL);

		ifmsg = (struct ifinfomsg *) NLMSG_DATA(nlmsg);
		ifmsg->ifi_family  = AF_UNSPEC;
		ifmsg->ifi_change |= IFF_UP;
		ifmsg->ifi_flags  |= IFF_UP;
		if (!(ifmsg->ifi_index = if_nametoindex("lo"))) {
			printf("failed to get veth1 name: %s\n", strerror(errno));
			return 1;
		}

		if (_nlmsg_send(fd, nlmsg) != 0)
			exit(1);
		if (_nlmsg_recieve(fd) != 0) {
			close(fd);
			exit(1);
		}

		free(nlmsg);
		nlmsg = malloc(4096);
		memset(nlmsg, 0, 4096);
		nlmsg->nlmsg_len   = NLMSG_LENGTH(sizeof(struct rtmsg));
		nlmsg->nlmsg_flags = NLM_F_ACK|NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL;
		nlmsg->nlmsg_type  = RTM_NEWROUTE;
		nlmsg->nlmsg_seq   = time(NULL);

		struct rtmsg *rtm;
		rtm = (struct rtmsg *) NLMSG_DATA(nlmsg);

		rtm->rtm_family   = AF_INET;
		rtm->rtm_table    = RT_TABLE_MAIN;
		rtm->rtm_scope    = RT_SCOPE_UNIVERSE;
		rtm->rtm_protocol = RTPROT_BOOT;
		rtm->rtm_type     = RTN_UNICAST;
		rtm->rtm_dst_len  = 0;

		if (inet_pton(AF_INET, "172.16.1.1", &addr) < 0)
			exit(1);

		_nlmsg_put(nlmsg, RTA_GATEWAY, &addr, addrlen);

		if (_nlmsg_send(fd, nlmsg) != 0)
			exit(1);
		if (_nlmsg_recieve(fd) != 0) {
			close(fd);
			exit(1);
		}
		close(fd);
	}
	waitpid(child, &rc, 0);
	if (rc != 0)
		fprintf(stderr, "failed to run commands in child network namespace\n");

	struct _rule r;
	r.table = "nat";
	r.entry = "POSTROUTING";
	r.type  = "MASQUERADE";
	r.saddr = "172.16.1.0/24";
	r.oface = "eth0";
	_ipt_rule(&r);
	r.saddr = NULL;
	r.table = "filter";
	r.entry = "FORWARD";
	r.type  = "ACCEPT";
	r.oface = "eth0";
	r.iface = "veth0";
	_ipt_rule(&r);
	r.oface = "veth0";
	r.iface = "eth0";
	_ipt_rule(&r);

	return 0;
}
