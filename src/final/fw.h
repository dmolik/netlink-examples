struct _rule {
	char *table;
	char *entry;
	char *type;
	char *iface;
	char *oface;
	char *saddr;
	char *daddr;
};

struct _addr_t {
	unsigned int addr;
	unsigned int mask;
};

struct _addr_t *  _init_addr(const char *ip);

void _free_addr(struct _addr_t *addr);

int _ipt_rule(struct _rule *rule);
