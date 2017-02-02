/*
 * zerocopy client.  We're given a bunch of files, a server and a port.
 * We squirt the files to the server.
 *
 * We have a protocol!  We send the number of loops, then for each
 * loop we send the number of files.  For each file we send the length of
 * the file followed by the file itself.
 */

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/resource.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sched.h>
#include <sys/mman.h>
#include <gdbm.h>
#include <time.h>

#include "zc.h"

#define N_CPUS	64
#define CACHE_LINE_SIZE	128		/* Plenty */
#define BLOCK_SIZE	512

#define DB_KEY_SIZE	64
#define DB_DATA_SIZE	64

extern int sendfile(int out_fd, int in_fd, off_t *offset, size_t count);

static int		debug;
static int		do_calibrate = 0;
static int		calibrate_state = 0;
static int		nr_cpus;
static unsigned long	calibrated_loops_per_sec = 0;
static struct timeval	alarm_time;
static unsigned long	counts_per_sec;
static double		d_counts_per_sec;
static 	char		*busyloop_buf;
static unsigned long	busyloop_size = 1000000;
static unsigned long	*busyloop_progress;
static int		period_millisecs = 1000;
static int		rw_ratio = 5;
static int		cacheline_size = 32;
static unsigned long	sum;
static unsigned long	twiddle = 1000;
static int		do_bonding;

static GDBM_FILE	db_file;
static datum		db_key,db_data; //db_key => time, db_data => cpu_load
static char		*db_filename;
static int		use_db=0;

#define CPS_FILE "counts_per_sec"

static void busyloop(int instance)
{
	int idx = 0;
	int rw = 0;

	for ( ; ; ) {
		int thumb;

		rw++;
		if (rw == rw_ratio) {
			busyloop_buf[idx]++;			/* Dirty a cacheline */
			rw = 0;
		} else {
			sum += busyloop_buf[idx];
		}

		for (thumb = 0; thumb < twiddle; thumb++)
			;				/* twiddle */

		busyloop_progress[instance * CACHE_LINE_SIZE]++;

		idx += cacheline_size;
		if (idx >= busyloop_size)
			idx = 0;
	}
}

static void itimer(int sig)
{
	struct timeval tv;
	unsigned long delta;
	unsigned long long blp[N_CPUS];
	unsigned long long total_blp = 0;
	unsigned long long blp_snapshot[N_CPUS];
	static unsigned long long old_blp[N_CPUS];
	int i;

	static time_t		current_time;
	static struct tm	*stru_tm;

	gettimeofday(&tv, 0);
	delta = (tv.tv_sec - alarm_time.tv_sec) * 1000000;
	delta += tv.tv_usec - alarm_time.tv_usec;
	delta /= 1000;		/* Milliseconds */

	if (use_db)
	{
		db_file = gdbm_open(db_filename,BLOCK_SIZE,GDBM_WRCREAT,0644,NULL);
	};

	for (i = 0; i < nr_cpus; i++) {
		blp_snapshot[i] = busyloop_progress[i * CACHE_LINE_SIZE];
		blp[i] = (blp_snapshot[i] - old_blp[i]);
		if (debug)
			printf("CPU%d: delta=%lu, blp_snapshot=%llu, old_blp=%llu, diff=%llu ",
				i, delta, blp_snapshot[i], old_blp[i], blp[i]);
		old_blp[i] = blp_snapshot[i];
		blp[i] *= 1000;
		blp[i] /= delta;
		total_blp += blp[i];
	}

	alarm_time = tv;

	if (do_calibrate) {
		if (calibrate_state++ == 3) {
			calibrated_loops_per_sec = total_blp;
		}
		printf("calibrating: %llu loops/sec\n", total_blp);
	} else {
		double cpu_load, d_blp;

		d_blp = total_blp;
		cpu_load = 1.0 - (d_blp / d_counts_per_sec);
		printf("System load:%5.1f%%", cpu_load * 100.0);
		if (use_db)
		{
			
			current_time = time(NULL);
			stru_tm = localtime(&current_time);
			
			db_key.dptr = (char *)malloc(DB_KEY_SIZE);
			strftime(db_key.dptr,DB_KEY_SIZE,"%T",stru_tm);
			db_key.dsize = strlen(db_key.dptr) + 1;

			db_data.dptr = (char *)malloc(DB_DATA_SIZE);
			sprintf(db_data.dptr,"%5.1f",cpu_load * 100.0);
			db_data.dsize = strlen(db_data.dptr) + 1;
			if (gdbm_store(db_file,db_key,db_data,GDBM_REPLACE))
			{
				fprintf(stderr,"Can't store in database.\n");
				exit(1);
			};
			gdbm_close(db_file);
			
		};
			
		if (do_bonding) {
			printf(" || Free:");
			for (i = 0; i < nr_cpus; i++) {
				d_blp = blp[i];
				cpu_load = (d_blp / (d_counts_per_sec / nr_cpus));
				printf(" %5.1f%%(%d)", cpu_load * 100.0, i);
			}
		}
		printf("\n");
				
	}
}

static void prep_cyclesoak(void)
{
	struct itimerval it = {
		{ 0, period_millisecs * 1000},
		{ 1, 0 },
	};
	FILE *f;
	char buf[80];
	int cpu;

	/* Cope with user-defined periods of > 1s */
	if (it.it_interval.tv_usec >= 1000000) {
		it.it_interval.tv_sec = it.it_interval.tv_usec / 1000000;
		it.it_interval.tv_usec = it.it_interval.tv_usec % 1000000;
	}

	if (!do_calibrate) {
		f = fopen(CPS_FILE, "r");
		if (!f) {
			fprintf(stderr, "Please run `cyclesoak -C' on an unloaded system\n");
			exit(1);
		}
		if (fgets(buf, sizeof(buf), f) == 0) {
			fprintf(stderr, "fgets failed!\n");
			exit(1);
		}
		fclose(f);
		counts_per_sec = strtoul(buf, 0, 10);
		if (counts_per_sec == 0) {
			fprintf(stderr, "something went wrong\n");
			exit(1);
		}
		d_counts_per_sec = counts_per_sec;
	}

	busyloop_buf = malloc(busyloop_size);
	if (busyloop_buf == 0) {
		fprintf(stderr, "busyloop_buf: no mem\n");
		exit(1);
	}
	busyloop_progress = mmap(0, nr_cpus * CACHE_LINE_SIZE * sizeof(long),
				PROT_READ|PROT_WRITE, MAP_SHARED|MAP_ANON, 0, 0);
	if (busyloop_progress == MAP_FAILED) {
		perror("mmap");
		exit(1);
	}
	memset(busyloop_progress, 0, nr_cpus * CACHE_LINE_SIZE);

	for (cpu = 0; cpu < nr_cpus; cpu++) {
		if (fork() == 0) {
			if (setpriority(PRIO_PROCESS, getpid(), 40)) {	/* As low as as we can go */
				perror("setpriority");
				exit(1);
			}
			if (do_bonding)
				bond_to_cpu(cpu);
			busyloop(cpu);
		}
	}

	signal(SIGALRM, itimer);
	gettimeofday(&alarm_time, 0);
	if (setitimer(ITIMER_REAL, &it, 0)) {
		perror("setitimer");
		exit(1);
	}
}

static void calibrate(void)
{
	prep_cyclesoak();
	for ( ; ; ) {
		sleep(10);
		if (calibrated_loops_per_sec) {
			FILE *f = fopen(CPS_FILE, "w");
			if (f == 0) {
				fprintf(stderr, "error opening `%s' for writing\n", CPS_FILE);
				perror("fopen");
				exit(1);
			}
			fprintf(f, "%lu\n", calibrated_loops_per_sec);
			fclose(f);
			return;
		}
	}
}

static void do_cyclesoak(void)
{
	prep_cyclesoak();
	for ( ; ; )
		sleep(1000);
}

static void exit_handler(void)
{
	killpg(0, SIGINT);
}

static void usage(void)
{
	fprintf(stderr,	"Usage: cyclesoak [-BCdhm] [-N nr_cpus] [-p period]\n"
			"\n"
			"  -B:      Generate per-CPU statistics\n"
			"  -C:      Calibrate CPU load\n"
			"  -d:      Debug (more d's, more fun)\n"
			"  -h:      This message\n"
			"  -m:      Set the load sampling period (milliseconds)\n"
			"  -N:      Tell cyclesoak how many CPUs you have\n"
			"  -p:      Set the load sampling period (seconds)\n"
			"  -D:      Store statistics in a dbm file\n"
		);
	exit(1);
}

int main(int argc, char *argv[])
{
	int c;
	nr_cpus = -1;
	int set_secs = 0, set_millisecs = 0;

	/* Allow the output to be piped to another process */
	setlinebuf(stdout);

	while ((c = getopt(argc, argv, "BCdhmN:p:D:")) != -1) {
		switch (c) {
		case 'B':
			do_bonding++;
			break;
		case 'C':
			do_calibrate++;
			break;
		case 'd':
			debug++;
			break;
		case 'h':
			usage();
			break;
		case 'm':
			period_millisecs = strtol(optarg, NULL, 10);
			set_millisecs = 1;
			break;
		case 'N':
			nr_cpus = strtol(optarg, NULL, 10);
			break;
		case 'p':
			period_millisecs = strtol(optarg, NULL, 10) * 1000;
			set_secs = 1;
			break;
		case 'D':
			use_db++;
			db_filename = optarg;
			break;
		case '?':
			usage();
			break;
		}
	}

	if (set_secs && set_millisecs) {
		printf("Error: cannot specify period in both seconds (-p) and milliseconds (-m)\n");
		exit(1);
	}

	if (nr_cpus == -1) {
		nr_cpus = sysconf(_SC_NPROCESSORS_ONLN);
		printf("using %d CPUs\n", nr_cpus);
	}

	setpgrp();
	atexit(exit_handler);

	if (do_calibrate) {
		calibrate();
		printf("calibrated OK.  %lu loops/sec\n", calibrated_loops_per_sec);
		exit(0);
	}

	do_cyclesoak();
	exit(0);
}
