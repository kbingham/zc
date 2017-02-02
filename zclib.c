/*
 * Common stuff for sendfile tester
 *
 * Andrew Morton <andrewm@uow.edu.au>
 */

#include "zc.h"

void set_sndbuf(int sock, int size)
{
	int ret;
	int size_in;
	int got_size;

	ret = setsockopt(sock, SOL_SOCKET, SO_SNDBUF, &size, sizeof(size));
	if (ret < 0) {
		perror("setsockopt SO_SNDBUF");
		exit(1);
	}
	got_size = sizeof(size_in);
	ret = getsockopt(sock, SOL_SOCKET, SO_SNDBUF, &size_in, &got_size);
	if (ret < 0) {
		perror("getsockopt SO_SNDBUF");
		exit(1);
	}
	if (got_size != sizeof(size_in)) {
		fprintf(stderr, "getsockopt returned wrong amount of data: %d, not %d\n",
				got_size, sizeof(size_in));
		exit(1);
	}
	if (size_in < size) {
		fprintf(stderr, "error setting sndbuf to %d: only got %d\n", size, size_in);
		exit(1);
	}
}

void set_rcvbuf(int sock, int size)
{
	int ret;
	int size_in;
	int got_size;

	ret = setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &size, sizeof(size));
	if (ret < 0) {
		perror("setsockopt SO_RCVBUF");
		exit(1);
	}
	got_size = sizeof(size_in);
	ret = getsockopt(sock, SOL_SOCKET, SO_RCVBUF, &size_in, &got_size);
	if (ret < 0) {
		perror("getsockopt SO_RCVBUF");
		exit(1);
	}
	if (got_size != sizeof(size_in)) {
		fprintf(stderr, "getsockopt returned wrong amount of data: %d, not %d\n",
				got_size, sizeof(size_in));
		exit(1);
	}
	if (size_in < size) {
		fprintf(stderr, "error setting rcvbuf to %d: only got %d\n", size, size_in);
		exit(1);
	}
}

int get_mhz(void)
{
	FILE *f = fopen("/proc/cpuinfo", "r");
	if (f == 0) {
		perror("can't open /proc/cpuinfo\n");
		exit(1);
	}

	for ( ; ; ) {
		int mhz;
		int ret;
		char buf[1000];

		if (fgets(buf, sizeof(buf), f) == NULL) {
			fprintf(stderr, "cannot locate cpu MHz in /proc/cpuinfo\n");
			exit(1);
		}

		ret = sscanf(buf, "cpu MHz         : %d", &mhz);

		if (ret == 1) {
			fclose(f);
			return mhz;
		}
	}
}

int get_cpus(void)
{
	int num = 0;

	FILE *f = fopen("/proc/cpuinfo", "r");
	if (f == 0) {
		perror("can't open /proc/cpuinfo\n");
		exit(1);
	}

	for ( ; ; ) {
		char buf[1000];

		if (fgets(buf, sizeof(buf), f) == NULL) {
			fclose(f);
			return num;
		}
		if (!strncmp(buf, "processor", 9))
			num++;
	}
}

#ifndef SYS_sched_setaffinity
#ifdef __powerpc__
#define SYS_sched_setaffinity 222
#elif defined(__i386__)
#define SYS_sched_setaffinity 241
#else
#warning please define SYS_sched_setaffinity
#endif
#endif

#define sched_setaffinity(pid, len, new_mask) \
	syscall(SYS_sched_setaffinity,(pid),(len),(new_mask))

void bond_to_cpus(unsigned long cpus)
{
	if (sched_setaffinity(getpid(), sizeof(cpus), &cpus)) {
		fprintf(stderr, "sched_setaffinity failed: %s\n",
			strerror(errno));
		exit(1);
	}
}

void bond_to_cpu(unsigned long cpu)
{
	bond_to_cpus(1UL << cpu);
}
