#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <linux/rtnetlink.h>
#include <linux/netlink.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <unistd.h>

#ifndef VETH_INFO_PEER
#define VETH_INFO_PEER 1
#endif

#define NLMSG_TAIL(nmsg) \
	((struct rtattr *) (((void *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

void _nlmsg_put(struct nlmsghdr *nlmsg, int type, void *data, size_t len)
{
	struct rtattr *rta;
	size_t  rtalen = RTA_LENGTH(len);
	rta = NLMSG_TAIL(nlmsg);
	rta->rta_type = type;
	rta->rta_len  = rtalen;
	if (data)
		memcpy(RTA_DATA(rta), data, len);
	nlmsg->nlmsg_len = NLMSG_ALIGN(nlmsg->nlmsg_len) + RTA_ALIGN(rtalen);
}

#define NLMSG_STRING(nl, attr, data) \
	_nlmsg_put((nl), (attr), (data), (strlen((data)) + 1))
#define NLMSG_ATTR(nl, attr) \
	_nlmsg_put((nl), (attr), (NULL), (0))

int _nlmsg_send(int fd, struct nlmsghdr *nlmsg)
{
	struct sockaddr_nl *sa = malloc(sizeof(struct sockaddr_nl));
	memset(sa, 0, sizeof(struct sockaddr_nl));
	sa->nl_family = AF_NETLINK;

	struct iovec  iov = { nlmsg, nlmsg->nlmsg_len };
	struct msghdr msg = { sa, sizeof(*sa), &iov, 1, NULL, 0, 0 };

	if (sendmsg(fd, &msg, 0) < 0) {
		fprintf(stderr, "failed to get socket: %s\n", strerror(errno));
		return 1;
	}

	return 0;
}
int _nlmsg_recieve(int fd)
{
	struct sockaddr_nl *sa = malloc(sizeof(struct sockaddr_nl));
	memset(sa, 0, sizeof(struct sockaddr_nl));
	sa->nl_family = AF_NETLINK;

	int len = 4096;
	char buf[len];
	struct iovec  iov = { buf, len };
	struct msghdr msg = { sa, sizeof(*sa), &iov, 1, NULL, 0, 0 };

	recvmsg(fd, &msg, 0);
	struct nlmsghdr *ret = (struct nlmsghdr*)buf;
	if (ret->nlmsg_type == NLMSG_ERROR) {
		struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(ret);
		if (err->error < 0) {
			fprintf(stderr, "recieve error: %d\n", err->error);
			return 1;
		}
	} else {
		fprintf(stderr, "invalid recieve type\n");
		return 1;
	}

	return 0;
}

int main(void)
{
	int fd = 0;
	if ((fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0) {
		fprintf(stderr, "failed to get socket: %s\n", strerror(errno));
		exit(1);
	}

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

	nest3->rta_len = (void *)NLMSG_TAIL(nlmsg) - (void *)nest3;
	nest2->rta_len = (void *)NLMSG_TAIL(nlmsg) - (void *)nest2;
	nest1->rta_len = (void *)NLMSG_TAIL(nlmsg) - (void *)nest1;

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
	if (_nlmsg_recieve(fd) != 0)
		exit(1);

	return 0;
}
