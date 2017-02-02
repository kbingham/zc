/*
 * Run a program as SCHED_FIFO.  If no args are provided, run a SCHED_RR
 * interactive shell.
 *
 * Requires root.
 */

#include "zc.h"

int set_realtime_priority(void)
{
	struct sched_param schp;
	/*
	 * set the process to realtime privs
	 */
	memset(&schp, 0, sizeof(schp));
	schp.sched_priority = sched_get_priority_max(SCHED_RR);
	
	if (sched_setscheduler(0, SCHED_RR, &schp) != 0) {
		fprintf(stderr, "you need to run as root for CPU load monitoring\n");
		perror("sched_setscheduler");
		exit(1);
	}

	return 0;
}

static void root(char *diag)
{
	fprintf(stderr, "Error running `%s': %s\n", diag, strerror(errno));
	fprintf(stderr, "Art thou not root?\n");
	exit(1);
}

int main(int argc, char *argv[])
{
	if (setuid(0) < 0)
		root("setuid");
	if (setgid(0) < 0)
		root("setgid");

	set_realtime_priority();

	if (argc == 1)
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
		execvp(argv[1], argv + 1);
		perror(argv[1]);
	}
	exit(1);
}
