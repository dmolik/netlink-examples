#define _GNU_SOURCE
#include <sched.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/mount.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <linux/limits.h>
#include <linux/rtnetlink.h>
#include <linux/netlink.h>
#include <linux/net_namespace.h>
#include <arpa/inet.h>
#include <net/if.h>
#include <unistd.h>

#ifndef VETH_INFO_PEER
#define VETH_INFO_PEER 1
#endif

#define NETNS_RUN_DIR "/run/netns"

#define NLMSG_TAIL(nmsg) \
	((struct rtattr *) (((void *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

static void _nlmsg_put(struct nlmsghdr *nlmsg, int type, void *data, size_t len)
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

static int _nlmsg_send(int fd, struct nlmsghdr *nlmsg)
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
static int _nlmsg_recieve(int fd)
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

static int create_netns_dir(void)
{
	if (mkdir(NETNS_RUN_DIR, S_IRWXU|S_IRGRP|S_IXGRP|S_IROTH|S_IXOTH)) {
		if (errno != EEXIST) {
			fprintf(stderr, "mkdir %s failed: %s\n",
				NETNS_RUN_DIR, strerror(errno));
			return -1;
		}
	}

	return 0;
}

int netns_get_fd(const char *name)
{
	char pathbuf[PATH_MAX];
	const char *path, *ptr;

	path = name;
	ptr = strchr(name, '/');
	if (!ptr) {
		snprintf(pathbuf, sizeof(pathbuf), "%s/%s",
			NETNS_RUN_DIR, name );
		path = pathbuf;
	}
	return open(path, O_RDONLY);
}

int new_ns(const char *name)
{
	char netns_path[PATH_MAX];
	int fd;
	int made_netns_run_dir_mount = 0;

	snprintf(netns_path, sizeof(netns_path), "%s/%s", NETNS_RUN_DIR, name);

	if (create_netns_dir())
		return -1;

	while (mount("", NETNS_RUN_DIR, "none", MS_SHARED | MS_REC, NULL)) {
		if (errno != EINVAL || made_netns_run_dir_mount) {
			fprintf(stderr, "mount --make-shared %s failed: %s\n",
				NETNS_RUN_DIR, strerror(errno));
			return -1;
		}

		if (mount(NETNS_RUN_DIR, NETNS_RUN_DIR, "none", MS_BIND | MS_REC, NULL)) {
			fprintf(stderr, "mount --bind %s %s failed: %s\n",
				NETNS_RUN_DIR, NETNS_RUN_DIR, strerror(errno));
			return -1;
		}
		made_netns_run_dir_mount = 1;
	}

	fd = open(netns_path, O_RDONLY|O_CREAT|O_EXCL, 0);
	if (fd < 0) {
		fprintf(stderr, "Cannot create namespace file \"%s\": %s\n",
			netns_path, strerror(errno));
		return -1;
	}
	close(fd);
	if (unshare(CLONE_NEWNET) < 0) {
		fprintf(stderr, "Failed to create a new network namespace \"%s\": %s\n",
			name, strerror(errno));
		goto out_delete;
	}

	if (mount("/proc/self/ns/net", netns_path, "none", MS_BIND, NULL) < 0) {
		fprintf(stderr, "Bind /proc/self/ns/net -> %s failed: %s\n",
			netns_path, strerror(errno));
		goto out_delete;
	}
	return 0;
out_delete:
	return -1;
}

int main(void)
{
	int fd = 0;
	if ((fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0) {
		fprintf(stderr, "failed to get socket: %s\n", strerror(errno));
		exit(1);
	}

	int sndbuf = 32768;
	int rcvbuf = 32768;
	if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
		&sndbuf, sizeof(sndbuf)) < 0) {
		fprintf(stderr, "failed to set send buffer: %s\n", strerror(errno));
		exit(1);
	}
	if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
		&rcvbuf,sizeof(rcvbuf)) < 0) {
		fprintf(stderr, "failed to set recieve buffer: %s\n", strerror(errno));
		exit(1);
	}
	struct sockaddr_nl *sa = malloc(sizeof(struct sockaddr_nl));
	memset(sa, 0, sizeof(struct sockaddr_nl));
	sa->nl_family = AF_NETLINK;
	sa->nl_groups = 0;
	if (bind(fd, (struct sockaddr *) sa, sizeof(struct sockaddr)) < 0) {
		fprintf(stderr, "failed to bind socket: %s\n", strerror(errno));
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

	if ((child = fork()) == 0) {
		setns(nsfd, CLONE_NEWNET);

		if ((fd = socket(AF_NETLINK, SOCK_RAW, NETLINK_ROUTE)) < 0) {
			fprintf(stderr, "failed to get socket: %s\n", strerror(errno));
			exit(1);
		}
		if (setsockopt(fd, SOL_SOCKET, SO_SNDBUF,
			&sndbuf, sizeof(sndbuf)) < 0) {
			fprintf(stderr, "failed to set send buffer: %s\n", strerror(errno));
			exit(1);
		}
		if (setsockopt(fd, SOL_SOCKET, SO_RCVBUF,
			&rcvbuf,sizeof(rcvbuf)) < 0) {
			fprintf(stderr, "failed to set recieve buffer: %s\n", strerror(errno));
			exit(1);
		}
		struct sockaddr_nl *sa = malloc(sizeof(struct sockaddr_nl));
		memset(sa, 0, sizeof(struct sockaddr_nl));
		sa->nl_family = AF_NETLINK;
		sa->nl_groups = 0;
		if (bind(fd, (struct sockaddr *) sa, sizeof(struct sockaddr)) < 0) {
			fprintf(stderr, "failed to bind socket: %s\n", strerror(errno));
			exit(1);
		}


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
			return 1;
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

	return 0;
}
