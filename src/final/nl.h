#include <linux/rtnetlink.h>
#include <linux/netlink.h>

#ifndef VETH_INFO_PEER
#define VETH_INFO_PEER 1
#endif

#define NLMSG_TAIL(nmsg) \
	((struct rtattr *) (((unsigned char *) (nmsg)) + NLMSG_ALIGN((nmsg)->nlmsg_len)))

void _nlmsg_put(struct nlmsghdr *nlmsg, int type, void *data, size_t len);

#define NLMSG_STRING(nl, attr, data) \
	_nlmsg_put((nl), (attr), (data), (strlen((data)) + 1))
#define NLMSG_ATTR(nl, attr) \
	_nlmsg_put((nl), (attr), (NULL), (0))

int _nlmsg_send(int fd, struct nlmsghdr *nlmsg);
int _nlmsg_recieve(int fd);
int _nl_socket_init(void);
