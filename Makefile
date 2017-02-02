CFLAGS	+= 	-g -O0 -Wall -Wshadow
ALL	=	zcc zcs cyclesoak run_rr run_on_cpu udpspam dumpdb

%.o : %.c
	$(CC) $(CFLAGS) -c $<

all:	$(ALL)

run_rr: run_rr.o
run_on_cpu: run_on_cpu.o zclib.o
zcc: zcc.o zclib.o
zcs: zcs.o zclib.o
udpspam: udpspam.o

dumpdb: dumpdb.o
	$(CC) $(CFLAGS) $(LDFLAGS) dumpdb.o -lgdbm -o dumpdb

cyclesoak: cyclesoak.o zclib.o
	$(CC) $(CFLAGS) $(LDFLAGS) cyclesoak.o zclib.o -lgdbm -o cyclesoak

zcc.o zcs.o zclib.o: zc.h

clean:
	$(RM) *.o $(ALL) counts_per_sec

install:
	cp cyclesoak $(BINDIR)
