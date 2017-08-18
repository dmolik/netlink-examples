#define _GNU_SOURCE
#include <sched.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdio.h>
#include <sys/wait.h>
#include <sys/inotify.h>
#include <sys/mount.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <string.h>
#include <fcntl.h>
#include <dirent.h>
#include <errno.h>
#include <unistd.h>
#include <ctype.h>
#include <linux/limits.h>
#include <linux/net_namespace.h>

#define NETNS_RUN_DIR "/var/run/netns"


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

int main(void)
{
	char netns_path[PATH_MAX];
	const char *name = "ns1";
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
