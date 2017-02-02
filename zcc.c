/*
 * zerocopy client.  We're given a bunch of files, a server and a port.
 * We squirt the files to the server.
 *
 * We have a protocol!  We send the number of loops, then for each
 * loop we send the number of files.  For each file we send the length of
 * the file followed by the file itself.
 */

#include "zc.h"

#ifndef u32
#define u32 u_int32_t
#endif

#ifndef TCP_CORK
#define TCP_CORK 3
#endif

extern int sendfile(int out_fd, int in_fd, off_t *offset, size_t count);

static int debug;
static int socket_debug;
static int input_buffer = 65536;
static int output_buffer = 65536;
static int no_sendfile;
static int do_write = 0;
static int use_tcp_cork = 0;
static unsigned long send_buffer_size = 64*1024;
static char *send_buffer;
static unsigned long long byte_count;

struct file_to_send {
	char *name;
	off_t size;
	int fd;
};

int opensock(struct hostent *he, int port)
{
	int sock;
	struct sockaddr_in ad;
	int one;
	int amount;

	memset(&ad, 0, sizeof(ad));
	ad.sin_family = AF_INET;

	memcpy(&ad.sin_addr, he->h_addr_list[0], sizeof(ad.sin_addr));
        ad.sin_port = htons(port);
	sock = socket(PF_INET, SOCK_STREAM, 0);
	if (sock < 0) {
		perror("socket");
		exit(1);
	}

	if (use_tcp_cork) {
		struct protoent *p;

		p = getprotobyname("tcp");
		if (p == 0) {
			fprintf(stderr, "where's TCP?\n");
			exit(1);
		}

		one = 1;
		if (setsockopt(sock, p->p_proto, TCP_CORK, &one, sizeof(one)) < 0) {
			fprintf(stderr, "setsockopt(TCP_CORK) failed: %s\n", strerror(errno));
			exit(1);
		}

		one = 0;		// :-)
		amount = sizeof(one);

		if (getsockopt(sock, p->p_proto, TCP_CORK, &one, &amount) < 0) {
			fprintf(stderr, "getsockopt(TCP_CORK) failed: %s\n", strerror(errno));
			exit(1);
		}
		if (!one) {
			fprintf(stderr, "setting TCP_CORK didn't stick\n");
			exit(1);
		}

	}

	if (socket_debug) {
		one = 1;
		if (setsockopt(sock, SOL_SOCKET, SO_DEBUG, &one, sizeof(one)) < 0) {
			fprintf(stderr, "setsockopt(SO_DEBUG) failed: %s\n", strerror(errno));
			exit(1);
		}

		one = 0;
		amount = sizeof(one);
		if (getsockopt(sock, SOL_SOCKET, SO_DEBUG, &one, &amount) < 0) {
			fprintf(stderr, "getsockopt(SO_DEBUG) failed: %s\n", strerror(errno));
			exit(1);
		}
		if (!one) {
			fprintf(stderr, "setting SO_DEBUG didn't stick\n");
			exit(1);
		} else {
			printf("socket in debug mode\n");
		}
	}

	if (connect(sock, (struct sockaddr *) &ad, sizeof(ad)) < 0) {
		perror("connect");
		exit(1);
	}

	return sock;
}

void ul_write(int sock, u32 val, char *desc)
{
	u32 data;
	int ret;

	if (debug > 1)
		printf("sending %u for `%s'\n", val, desc);

	data = htonl(val);
	ret = do_write ?	write(sock, &data, 4) :
				send(sock, &data, 4, 0);
	if (ret != 4) {
		fprintf(stderr, "error writing 4 bytes for %s: %s.  Sent %d\n",
				desc, strerror(errno), ret);
		exit(1);
	}
}

void send_em(int sock, struct file_to_send *files_to_send, int n_files, int n_loops)
{
	int i, loop;

	ul_write(sock, n_loops, "n_loops");

	for (loop = 0; loop < n_loops; loop++) {
		ul_write(sock, n_files, "n_files");
		for (i = 0; i < n_files; i++) {
			int res;

			if (debug > 1) {
				off_t off;
				printf("sending `%s': %lu bytes\n", files_to_send[i].name, files_to_send[i].size);
				if (debug > 2) {
					off = lseek(files_to_send[i].fd, 0, SEEK_CUR);
					printf("offset=%ld\n", off);
				}
			}

			ul_write(sock, files_to_send[i].size, "size");

			if (no_sendfile) {
				unsigned long to_send = files_to_send[i].size;
				unsigned long chunk;

				lseek(files_to_send[i].fd, SEEK_SET, 0);
				while (to_send) {
					int ret;
					unsigned long to_write;
					off_t off = 0;

					chunk = to_send;
					if (chunk > send_buffer_size)
						chunk = send_buffer_size;
					ret = read(files_to_send[i].fd, send_buffer, chunk);
					if (ret != chunk) {
						fprintf(stderr, "%s: tried to read %lu bytes, got %d instead\n",
							files_to_send[i].name, chunk, ret);
						if (ret < 0)
							perror("read");
						exit(1);
					}
					to_write = chunk;
					while (to_write) {
						ret = do_write ? write(sock, send_buffer + off, to_write) :
								 send(sock, send_buffer + off, to_write, 0);
						if (ret < 0) {
							fprintf(stderr, "%s: error sending %lu bytes\n",
								files_to_send[i].name, chunk);
							perror("write");
							exit(1);
						}
						to_write -= ret;
						off += ret;
					}
					to_send -= chunk;
				}
			} else {
				unsigned long to_send = files_to_send[i].size;
				off_t start_off = 0;

				while (to_send) {
					if (debug > 2)
						printf("before send, start_off=%lu, file_off=%lu\n",
							start_off, lseek(files_to_send[i].fd, 0, SEEK_CUR));
					res = sendfile(sock, files_to_send[i].fd, &start_off, to_send);
					if (debug > 2)
						printf("after send, start_off=%lu, file_off=%lu\n",
							start_off, lseek(files_to_send[i].fd, 0, SEEK_CUR));
					if (res <= 0) {
						fprintf(stderr, "error sending `%s': %s.  Sent %d, not %lu\n",
								files_to_send[i].name,
								strerror(errno),
								res,
								files_to_send[i].size);
						exit(1);
					}
					to_send -= res;
				}
			}
			byte_count += files_to_send[i].size;
		}
	}
}

void doit(char **filenames, int n_files, char *server, int port, int n_loops)
{
	struct hostent *he;
	int sock;
	int i;
	struct file_to_send *files_to_send;
	int out_i;
	unsigned long out_n_files = 0;
	unsigned long long total_bytes = 0;

	he = gethostbyname(server);
	if (he == NULL) {
		perror("gethostbyname");
		exit(1);
	}

	sock = opensock(he, port);
	set_sndbuf(sock, input_buffer);
	set_rcvbuf(sock, output_buffer);
	files_to_send = malloc(n_files * sizeof(*files_to_send));
	if (files_to_send == 0) {
		fprintf(stderr, "no mem\n");
		exit(1);
	}
		
	for (i = 0, out_i = 0; i < n_files; i++) {
		struct stat statbuf;

		files_to_send[out_i].name = filenames[i];
		files_to_send[out_i].fd = open(filenames[i], O_RDONLY);
		if (files_to_send[out_i].fd < 0) {
			if (debug)
				printf("Can't read %s: Skipping.\n", filenames[i]);
			continue;
		}
		if (fstat(files_to_send[out_i].fd, &statbuf) < 0) {
			perror("fstat");
			exit(1);
		}
		if (!S_ISREG(statbuf.st_mode)) {
			if (debug)
				printf("Can't read %s: Skipping.\n", filenames[i]);
			close(files_to_send[out_i].fd);
			continue;
		}
		files_to_send[out_i].size = statbuf.st_size;
		total_bytes += files_to_send[out_i].size;
		out_i++;
		out_n_files++;
	}

	if (debug) {
		printf("Sending %llu bytes in %lu files %d times, for %llu bytes total\n",
			total_bytes, out_n_files, n_loops, n_loops * total_bytes);
	}

	send_em(sock, files_to_send, out_n_files, n_loops);

	close(sock);
	for (i = 0; i < out_n_files; i++) {
		close(files_to_send[i].fd);
	}
	free(files_to_send);
}

int set_realtime_priority(void)
{
	struct sched_param schp;
	/*
	 * set the process to realtime privs
	 */
	memset(&schp, 0, sizeof(schp));
	schp.sched_priority = sched_get_priority_max(SCHED_FIFO);
	
	if (sched_setscheduler(0, SCHED_RR, &schp) != 0) {
		fprintf(stderr, "you need to run as root for CPU load monitoring\n");
		perror("sched_setscheduler");
		exit(1);
	}

	return 0;
}

static void usage(void)
{
	fprintf(stderr,	"Usage: zcc [-kdDhSw] [-b buffer_size] [-B cpu] [-i input_bufsize] [-o output_bufsize]\n"
			"           [-n n_loops] [-p port] -s server filename[s]\n"
			"\n"
			"  -b:      Specify transfer size for read/write mode (bytes)\n"
			"  -B:      Bond zcc to a CPU\n"
			"  -d:      Debug (more d's, more fun)\n"
			"  -D:      Put socket into debug mode\n"
			"  -h:      This message\n"
			"  -i:      Set TCP receive buffer size (bytes)\n"
			"  -k:      Use TCP_CORK\n"
			"  -o:      Set TCP send buffer size (bytes)\n"
			"  -n:      Send the fileset this many times\n"
			"  -p:      TCP port to connect to\n"
			"  -s:      Server to connect to\n"
			"  -S:      Don't use sendfile: use send() instead\n"
			"  -w:      use write() on socket instead of send()\n"
		);
	exit(1);
}

int main(int argc, char *argv[])
{
	int n_loops = 1;
	int c;
	char **filenames;
	int port = 2222;
	char *server = 0;
	int cpu = -1;

	while ((c = getopt(argc, argv, "kdDhwSn:b:B:o:i:s:p:")) != -1) {
		switch (c) {
		case 'b':
			send_buffer_size = strtol(optarg, NULL, 10);
			break;
		case 'B':
			cpu = strtol(optarg, NULL, 10);
			break;
		case 'd':
			debug++;
			break;
		case 'D':
			socket_debug++;
			break;
		case 'h':
			usage();
			break;
		case 'i':
			input_buffer = strtol(optarg, NULL, 10);
			break;
		case 'k':
			use_tcp_cork++;
			break;
		case 'n':
			n_loops = strtol(optarg, NULL, 10);
			break;
		case 'o':
			output_buffer = strtol(optarg, NULL, 10);
			break;
		case 'p':
			port = strtol(optarg, NULL, 10);
			break;
		case 's':
			server = optarg;
			break;
		case 'S':
			no_sendfile++;
			break;
		case 'w':
			do_write++;
			break;
		case '?':
			usage();
			break;
		}
	}

	if (server == 0 || optind == argc)
		usage();
	if (do_write && !no_sendfile)
		usage();

	if (cpu != -1)
		bond_to_cpu(cpu);

	filenames = argv + optind;
	if (no_sendfile) {
		send_buffer = malloc(send_buffer_size + 256);
		if (send_buffer == 0) {
			fprintf(stderr, "Error allocating %lu bytes\n", send_buffer_size);
			exit(1);
		}
		send_buffer = (char *)((((long)send_buffer) + 255) & ~255);
	}

	doit(filenames, argc - optind, server, port, n_loops);

	exit(0);
}
