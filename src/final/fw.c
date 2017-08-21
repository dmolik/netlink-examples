
#define _DEFAULT_SOURCE 1
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <stdlib.h>

#include <libiptc/libiptc.h>
#include <linux/netfilter/nf_nat.h>
#include <linux/netfilter/x_tables.h>
#include <arpa/inet.h>

#include "fw.h"

struct _addr_t *  _init_addr(const char *ip)
{
	struct _addr_t *addr = malloc(sizeof(struct _addr_t));
	memset(addr, 0, sizeof(struct _addr_t));

	char *readdr, *prefix, *ptr;

	ptr = strrchr(ip, '/');
	prefix = strdup(ptr); prefix++;
	unsigned long mask = (0xFFFFFFFF << (32 - (unsigned int) atoi(prefix))) & 0xFFFFFFFF;

	readdr = strdup(ip);
	readdr[strlen(ip) - strlen(ptr)] = '\0';
	inet_pton(AF_INET, readdr, &addr->addr);
	addr->mask = htonl(mask);
	return addr;
}

void _free_addr(struct _addr_t *addr)
{
	free(addr);
}

int _ipt_rule(struct _rule *rule)
{
	struct xtc_handle *h = iptc_init(rule->table);
	int result = 0;
	if (!rule->table)
		return -1;
	if (!rule->type)
		return -1;
	if (!rule->entry)
		return -1;

	if (!h) {
		printf( "error condition  %s\n", iptc_strerror(errno));
		return -1;
	}

	unsigned int targetOffset =  XT_ALIGN(sizeof(struct ipt_entry));
	unsigned int totalLen     = targetOffset + XT_ALIGN(sizeof(struct xt_standard_target));

	if (strcmp(rule->type, "MASQUERADE") == 0)
		totalLen +=  XT_ALIGN(sizeof(struct nf_nat_ipv4_multi_range_compat));

	struct ipt_entry* e = (struct ipt_entry *)calloc(1, totalLen);
	if (e == NULL) {
		printf("calloc failure :%s\n", strerror(errno));
		return -1;
	}

	e->target_offset = targetOffset;
	e->next_offset   = totalLen;

	if (rule->oface)
		strncpy(e->ip.outiface, rule->oface, strlen(rule->oface) + 1);
	if (rule->iface)
		strncpy(e->ip.iniface,  rule->iface, strlen(rule->iface) + 1);

	if (rule->saddr) {
		struct _addr_t *addr = _init_addr(rule->saddr);
		e->ip.src.s_addr  = addr->addr;
		e->ip.smsk.s_addr = addr->mask;
		_free_addr(addr);
	}
	if (rule->daddr) {
		struct _addr_t *addr = _init_addr(rule->daddr);
		e->ip.dst.s_addr  = addr->addr;
		e->ip.dmsk.s_addr = addr->mask;
		_free_addr(addr);
	}

	if (strcmp(rule->type, "MASQUERADE") == 0) {
		struct xt_entry_target* target = (struct xt_entry_target  *) e->elems;
		target->u.target_size          = XT_ALIGN(sizeof(struct xt_entry_target))
			+ XT_ALIGN(sizeof(struct nf_nat_ipv4_multi_range_compat));
		strncpy(target->u.user.name, rule->type, strlen(rule->type) + 1);
		target->u.user.revision = 0;
		struct nf_nat_ipv4_multi_range_compat* masquerade = (struct nf_nat_ipv4_multi_range_compat  *) target->data;
		masquerade->rangesize   = 1;
	} else {
		struct xt_standard_target* target  = (struct xt_standard_target  *) e->elems;
		target->target.u.target_size = XT_ALIGN(sizeof(struct xt_standard_target));
		strncpy(target->target.u.user.name, rule->type, strlen(rule->type) + 1);
		target->target.u.user.revision = 0;
		target->verdict                = -NF_ACCEPT - 1;
	}

	if (iptc_append_entry(rule->entry, e, h) == 0) {
		printf("iptc_append_entry::Error insert/append entry: %s\n", iptc_strerror(errno));
		result = -1;
		goto end;
	}
	if (iptc_commit(h) == 0) {
		printf("iptc_commit::Error commit: %s\n", iptc_strerror(errno));
		result = -1;
	}

	end:
		free(e);
		iptc_free(h);
		return result;
}

