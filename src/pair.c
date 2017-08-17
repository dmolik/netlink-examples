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

int main(void)
{
	int fd = 0;
	if ((fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0) {
		fprintf(stderr, "failed to get socket: %s\n", strerror(errno));
		exit(1);
	}

	struct sockaddr_nl *sa = malloc(sizeof(struct sockaddr_nl));
	memset(sa, 0, sizeof(struct sockaddr_nl));
	sa->nl_family = AF_NETLINK;

	struct nlmsghdr *nlmsg = malloc(4096);
	memset(nlmsg, 0, 4096);
	nlmsg->nlmsg_len   = NLMSG_LENGTH(sizeof(struct ifinfomsg));
	nlmsg->nlmsg_type  = RTM_NEWLINK;
	nlmsg->nlmsg_flags = NLM_F_REQUEST|NLM_F_CREATE|NLM_F_EXCL|NLM_F_ACK;
	nlmsg->nlmsg_seq   = time(NULL);

	struct ifinfomsg *ifmsg;
	ifmsg = (struct ifinfomsg *) NLMSG_DATA(nlmsg);
	ifmsg->ifi_family = AF_UNSPEC;

	struct rtattr *rta, *nest1, *nest2, *nest3;
	size_t rtalen;

	rta = NLMSG_TAIL(nlmsg);
	rtalen        = RTA_LENGTH(strlen("veth1") + 1);
	rta->rta_type = IFLA_IFNAME;
	rta->rta_len  = rtalen;
	memcpy(RTA_DATA(rta), "veth1", strlen("veth1") + 1);
	nlmsg->nlmsg_len = NLMSG_ALIGN(nlmsg->nlmsg_len) + RTA_ALIGN(rtalen);

	nest1 = NLMSG_TAIL(nlmsg);
	rta = NLMSG_TAIL(nlmsg);
	rtalen        = RTA_LENGTH(0);
	rta->rta_type = IFLA_LINKINFO;
	rta->rta_len  = rtalen;
	nlmsg->nlmsg_len = NLMSG_ALIGN(nlmsg->nlmsg_len) + RTA_ALIGN(rtalen);

	rta = NLMSG_TAIL(nlmsg);
	rtalen        = RTA_LENGTH(strlen("veth") + 1);
	rta->rta_type = IFLA_INFO_KIND;
	rta->rta_len  = rtalen;
	memcpy(RTA_DATA(rta), "veth", strlen("veth") + 1);
	nlmsg->nlmsg_len = NLMSG_ALIGN(nlmsg->nlmsg_len) + RTA_ALIGN(rtalen);

	nest2 = NLMSG_TAIL(nlmsg);
	rta = NLMSG_TAIL(nlmsg);
	rta->rta_type = IFLA_INFO_DATA;
	rtalen = RTA_LENGTH(0);
	rta->rta_len  = rtalen;
	nlmsg->nlmsg_len = NLMSG_ALIGN(nlmsg->nlmsg_len) + RTA_ALIGN(rtalen);

	nest3 = NLMSG_TAIL(nlmsg);
	rta = NLMSG_TAIL(nlmsg);
	rta->rta_type = VETH_INFO_PEER;
	rtalen = RTA_LENGTH(0);
	rta->rta_len  = rtalen;
	nlmsg->nlmsg_len = NLMSG_ALIGN(nlmsg->nlmsg_len) + RTA_ALIGN(rtalen);

	nlmsg->nlmsg_len += sizeof(struct ifinfomsg);

	rta = NLMSG_TAIL(nlmsg);
	rtalen        = RTA_LENGTH(strlen("vpeer1") + 1);
	rta->rta_type = IFLA_IFNAME;
	rta->rta_len  = rtalen;
	memcpy(RTA_DATA(rta), "vpeer1", strlen("vpeer1") + 1);
	nlmsg->nlmsg_len = NLMSG_ALIGN(nlmsg->nlmsg_len) + RTA_ALIGN(rtalen);

	nest3->rta_len = (void *)NLMSG_TAIL(nlmsg) - (void *)nest3;
	nest2->rta_len = (void *)NLMSG_TAIL(nlmsg) - (void *)nest2;
	nest1->rta_len = (void *)NLMSG_TAIL(nlmsg) - (void *)nest1;

	struct iovec  iov = { nlmsg, nlmsg->nlmsg_len };
	struct msghdr msg = { sa, sizeof(*sa), &iov, 1, NULL, 0, 0 };

	if (sendmsg(fd, &msg, 0) < 0) {
		fprintf(stderr, "failed to get socket: %s\n", strerror(errno));
		exit(1);
	}

	int len = 4096;
	char buf[len];
	iov.iov_base = buf;
	iov.iov_len = len;

	memset(&msg, 0, sizeof(msg));
	msg.msg_name = sa;
	msg.msg_namelen = sizeof(*sa);
	msg.msg_iov = &iov;
	msg.msg_iovlen = 1;

	recvmsg(fd, &msg, 0);
	close(fd);
	struct nlmsghdr *ret = (struct nlmsghdr*)buf;
	if (ret->nlmsg_type == NLMSG_ERROR) {
		struct nlmsgerr *err = (struct nlmsgerr*)NLMSG_DATA(ret);
		if (err->error < 0) {
			printf("error: %d\n, failed to create links\n", err->error);
			return 1;
		}
	} else {
		printf("failed to create links\n");
		return 1;
	}

	return 0;
}
