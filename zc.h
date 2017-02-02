/*
 * Header for zerocopy tester
 *
 * Andrew Morton <andrewm@uow.edu.au>
 */

#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <errno.h>
#include <string.h>
#include <sys/time.h>
#include <sys/signal.h>
#include <sys/socket.h>
#include <sched.h>
#include <sys/mman.h>

#ifndef u32
#define u32 u_int32_t
#endif


extern void set_sndbuf(int sock, int size);
extern void set_rcvbuf(int sock, int size);
extern int get_mhz(void);
extern void bond_to_cpus(unsigned long cpu);
extern void bond_to_cpu(unsigned long cpu);

