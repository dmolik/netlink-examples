#include <stdio.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include <linux/rtnetlink.h>
#include <linux/netlink.h>
#include <linux/net_namespace.h>

#include "nl.h"

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

int _nl_socket_init(void)
{
	int fd = 0;
	if ((fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0) {
		fprintf(stderr, "failed to get socket: %s\n", strerror(errno));
		return 0;
	}

	int sndbuf = 32768;
	int rcvbuf = 32768;
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
		&sndbuf, sizeof(sndbuf)) < 0) {
		fprintf(stderr, "failed to set send buffer: %s\n", strerror(errno));
		return 0;
	}
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
		&rcvbuf,sizeof(rcvbuf)) < 0) {
		fprintf(stderr, "failed to set recieve buffer: %s\n", strerror(errno));
		return 0;
	}
	struct sockaddr_nl *sa = malloc(sizeof(struct sockaddr_nl));
	memset(sa, 0, sizeof(struct sockaddr_nl));
	sa->nl_family = AF_NETLINK;
	sa->nl_groups = 0;
	if (bind(fd, (struct sockaddr *) sa, sizeof(struct sockaddr)) < 0) {
		fprintf(stderr, "failed to bind socket: %s\n", strerror(errno));
		return 0;
	}

	return fd;
}

