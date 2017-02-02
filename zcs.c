/*
 * The world's smallest web server
 */

#include "zc.h"

static int debug;
static struct timeval alarm_time;
static unsigned long long byte_count;

static int input_buffer = 65536;
static int output_buffer = 65536;

static char *buf;
unsigned long buflen = 16*1024;

int opensock(int port)
{
	int sock;
	int one = 1;
	struct sockaddr_in ad;

	memset(&ad, 0, sizeof(ad));
	ad.sin_family = AF_INET;
	ad.sin_addr.s_addr = htonl(INADDR_ANY);
        ad.sin_port = htons(port);

	sock = socket(PF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}

	if (setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *) &one, sizeof(int)) < 0) {
		perror("setsockopt");
		exit(1);
	}

	if (bind(sock, (struct sockaddr *)&ad, sizeof(struct sockaddr_in)) == -1) {
		perror("bind");
		exit(1);
	}

	if (listen(sock, 100) < 0) {
		perror("listen");
		exit(1);
	}

	return sock;
}

static u32 ul_read(int fd, char *desc)
{
	u32 data;
	u32 val;
	int ret;
	int to_read = 4, idx = 0;
	char *pdata = (char *)&data;

	while (to_read) {
		ret = read(fd, pdata + idx, to_read);
		if (ret < 0) {
			fprintf(stderr, "read of 4 bytes for `%s' returned %d\n", desc, ret);
			perror("read");
			exit(1);
		}
		if (ret == 0) {
			fprintf(stderr, "client seems to have vanished\n");
			exit(0);
		}
		to_read -= ret;
		idx += ret;
		byte_count += ret;
	}

	val = ntohl(data);

	if (debug > 1)
		printf("read of `%s' returns %u\n", desc, val);

	return val;
}

static void bigread(int fd, unsigned long length)
{
	unsigned long chunk;
	unsigned long amount_left;

	amount_left = length;
	while (amount_left) {
		int ret;

		chunk = amount_left;
		if (chunk > buflen)
			chunk = buflen;
		
		ret = read(fd, buf, chunk);
		if (debug > 1 && ret != chunk) {
			printf("bigread: tried to read %lu, got %d instead\n", chunk, ret);
		}
		if (ret < 0) {
			fprintf(stderr, "bigread of %lu bytes returned %d\n", chunk, ret);
			perror("read");
		}
		if (ret == 0) {
			fprintf(stderr, "client seems to have vanished\n");
			exit(0);
		}
		amount_left -= ret;
		byte_count += ret;
	}

	if (debug)
		printf("big_read(%lu) succeeded\n", length);
}

static void itimer(int sig)
{
	struct timeval tv;
	unsigned long delta;
	unsigned long long bytes_per_sec;
	static unsigned long long old_byte_count;

	gettimeofday(&tv, 0);
	delta = (tv.tv_sec - alarm_time.tv_sec) * 1000000;
	delta += tv.tv_usec - alarm_time.tv_usec;
	delta /= 1000;		/* Milliseconds */
	bytes_per_sec = (byte_count - old_byte_count) * 1000;
	bytes_per_sec /= delta;
	
	printf("%lu kbytes/sec\n", ((unsigned long)bytes_per_sec) / 1024);
	old_byte_count = byte_count;
	alarm_time = tv;
}

static void fetch_em(int sock)
{
	unsigned long n_loops;
	int loop;
	struct itimerval it = {
		{ 5, 0 },
		{ 5, 0 },
	};

	signal(SIGALRM, itimer);
	gettimeofday(&alarm_time, 0);
	if (setitimer(ITIMER_REAL, &it, 0)) {
		perror("setitimer");
		exit(1);
	}

	n_loops = ul_read(sock, "n_loops");

	for (loop = 0; loop < n_loops; loop++) {
		unsigned long n_files;
		int i;

		n_files = ul_read(sock, "n_files");

		for (i = 0; i < n_files; i++) {
			u32 length;

			length = ul_read(sock, "length");
			bigread(sock, length);
		}
	}
}

static void usage(void)
{
	fprintf(stderr,	"Usage: zcs [-Bdh] [-p port] [-i input_bufsize] [-o output_bufsize]\n"
			"  -d:      Debug (more d's, more fun)\n"
			"  -B:      Bond zcs to a CPU\n"
			"  -h:      This message\n"
			"  -p:      TCP port to listen on\n"
			"  -i:      Set TCP receive buffer size (bytes)\n"
			"  -o:      Set TCP send buffer size (bytes)\n"
	);
	exit(1);
}

int main(int argc, char *argv[])
{
	int port = 2222;
	int sock;
	int c;
	int cpu = -1;

	while ((c = getopt(argc, argv, "dhB:i:o:p:")) != -1) {
		switch (c) {
		case 'B':
			cpu = strtol(optarg, NULL, 10);
			break;
		case 'd':
			debug++;
			break;
		case 'h':
			usage();
			break;
		case 'i':
			input_buffer = strtol(optarg, NULL, 10);
			break;
		case 'o':
			output_buffer = strtol(optarg, NULL, 10);
			break;
		case 'p':
			port = strtol(optarg, NULL, 10);
			break;
		case '?':
			usage();
			break;
		}
	}

	buf = malloc(buflen);
	if (buf == 0) {
		fprintf(stderr, "error allocating %lu bytes\n", buflen);
		exit(1);
	}

	if (cpu != -1)
		bond_to_cpu(cpu);

	sock = opensock(port);
	set_sndbuf(sock, input_buffer);
	set_rcvbuf(sock, output_buffer);

	for ( ; ; ) {
		struct sockaddr addr;
		int addrlen = sizeof(addr);
		int newsock;

		newsock = accept(sock, &addr, &addrlen);
		if (newsock < 0) {
			perror("accept");
			exit(1);
		}
		if (fork() == 0) {
			fetch_em(newsock);
			if (close(newsock)) {
				perror("close");
				exit(1);
			}
			printf("server exits\n");
			exit(0);
		}
		close(newsock);
	}
}		
