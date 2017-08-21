#define NETNS_RUN_DIR "/run/netns"

int netns_get_fd(const char *name);
int new_ns(const char *name);
