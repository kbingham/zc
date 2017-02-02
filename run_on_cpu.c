/*
 * Run a program as SCHED_FIFO.  If no args are provided, run a SCHED_RR
 * interactive shell.
 *
 * Requires root.
 */

#include "zc.h"

static void root(char *diag)
{
	fprintf(stderr, "Error running `%s': %s\n", diag, strerror(errno));
	fprintf(stderr, "Art thou not root?\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	unsigned long mask;

	if (argc < 2) {
		fprintf(stderr, "Usage: %s CPU_MASK(hex) [command]\n", argv[0]);
		exit(1);
	}

	mask = strtoul(argv[1], NULL, 16);
	if (setuid(0) < 0)
		root("setuid");
	if (setgid(0) < 0)
		root("setgid");

	bond_to_cpus(mask);

	if (argc == 2)
	{
		const char *s = getenv("SHELL");
		if (s)
		{
			execl(s, s, 0);
			perror(s);
		}
	}
	else
	{
		execvp(argv[2], argv + 2);
		perror(argv[2]);
	}
	exit(1);
}
