#include <stdio.h>
#include <sys/errno.h>
#include <stdlib.h>
#include <string.h>
#include <libiptc/libiptc.h>

int main(void)
{
	struct xtc_handle *h = iptc_init("filter");
	int result = 0;
	if (!h) { printf( "error condition  %s\n", iptc_strerror(errno)); return -1;}

	unsigned int targetOffset =  XT_ALIGN(sizeof(struct ipt_entry));
	unsigned int totalLen     = targetOffset + XT_ALIGN(sizeof(struct xt_standard_target));

	struct ipt_entry* e = (struct ipt_entry *)calloc(1, totalLen);
	if (e == NULL) {
		printf("calloc failure :%s\n", strerror(errno));
		return -1;
	}

	e->target_offset = targetOffset;
	e->next_offset = totalLen;

	strcpy(e->ip.outiface, "eth0");
	strcpy(e->ip.iniface, "veth1");

	struct xt_standard_target* target = (struct xt_standard_target  *) e->elems;
	target->target.u.target_size = XT_ALIGN(sizeof(struct xt_standard_target));
	strncpy(target->target.u.user.name, "ACCEPT", strlen("ACCEPT") + 1);
	target->target.u.user.revision = 0;
	target->verdict                = -NF_ACCEPT - 1;

	if (iptc_append_entry("FORWARD", e, h) == 0) {
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
