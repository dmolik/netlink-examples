#include <stdio.h>
#include <sys/errno.h>
#include <stdlib.h>
#include <string.h>
#include <libiptc/libiptc.h>
#include <linux/netfilter/xt_mark.h>
#include <linux/netfilter/xt_physdev.h>
#include <linux/netfilter/x_tables.h>
#include <linux/netfilter/nf_nat.h>
#include <netinet/in.h>
#include <unistd.h>
#include <arpa/inet.h>

int main()
{
	struct xtc_handle *h = iptc_init("nat");
	int           result = 0;
	if (!h) {
		printf( "error condition  %s\n", iptc_strerror(errno));
		return -1;
	}

	unsigned int targetOffset =  XT_ALIGN(sizeof(struct ipt_entry));

	unsigned int totalLen = targetOffset + XT_ALIGN(sizeof(struct xt_entry_target)) + XT_ALIGN(sizeof(struct nf_nat_ipv4_multi_range_compat));

	struct ipt_entry* e = (struct ipt_entry *)calloc(1, totalLen);
	if(e == NULL) {
		printf("calloc failure :%s\n", strerror(errno));
		return -1;
	}

	e->target_offset = targetOffset;
	e->next_offset   = totalLen;
	strcpy(e->ip.outiface, "eth0");
	unsigned int ip, mask;
	inet_pton (AF_INET, "172.16.1.0", &ip);
	inet_pton (AF_INET, "255.255.255.0", &mask);
	e->ip.src.s_addr  = ip;
	e->ip.smsk.s_addr = mask;

	struct xt_entry_target* target = (struct xt_entry_target  *) e->elems;
	target->u.target_size          = XT_ALIGN(sizeof(struct xt_entry_target)) + XT_ALIGN(sizeof(struct nf_nat_ipv4_multi_range_compat));
	strcpy(target->u.user.name, "MASQUERADE");
	target->u.user.revision = 0;
	struct nf_nat_ipv4_multi_range_compat* masquerade = (struct nf_nat_ipv4_multi_range_compat  *) target->data;
	masquerade->rangesize   = 1;

	if (iptc_append_entry("POSTROUTING", e, h) != 0) {
		printf("iptc_append_entry::Error insert/append entry: %s\n", iptc_strerror(errno));
		result = -1;
		goto end;
	}

	if (iptc_commit(h) != 0) {
		printf("iptc_commit::Error commit: %s\n", iptc_strerror(errno));
		result = -1;
	}

	end:
		free(e);
		iptc_free(h);
		return result;
}
