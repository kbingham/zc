/*
 * Dump the gdbm database
 *
 * Lucas Brasilino <brasilino@recife.pe.gov.br>
 */

#include <stdio.h>
#include <strings.h>
#include <gdbm.h>
#include <stdlib.h>
#include <getopt.h>
#include <unistd.h>

#define BLOCK_SIZE	512

static GDBM_FILE 	db_file;
static datum		db_key, db_key_next, db_data;
static char		*db_filename;
static char		*program_name;

static void help(FILE *s, int exit_code)
{
	fprintf(s,"Error!!\n");
	fprintf(s,"No option!!\n");
	exit(exit_code);
};

int main(int argc, char *argv[])
{
	int opt;

	opterr = 0; //desabilita geração de erro pelo getopt

	while ((opt = getopt(argc, argv, "hD:")) != -1)
	{
		switch (opt) 
		{
			case 'h':
				help(stdout, 0);
				break;
			case 'D':
				db_filename = optarg;
				break;
			case '?':
				help(stderr, 1);
				break;
			default:
				help(stderr, 1);
				break;
		};
	};
	program_name = argv[0];

	if (!db_filename)
	{
		fprintf (stderr,"%s: Tell me database\n", program_name);
		exit(1);
	}

	db_file = gdbm_open(db_filename, BLOCK_SIZE, GDBM_READER, 0644, NULL);

	if (!db_file)
	{
		fprintf (stderr,"%s: Can't open database: %s\n", program_name, db_filename);
		exit(1);
	}
	db_key = gdbm_firstkey(db_file);
	while (db_key.dptr) 
	{
		db_key_next = gdbm_nextkey(db_file, db_key);
		db_data = gdbm_fetch(db_file,db_key);
		printf("%s\t%s\n",db_key.dptr,db_data.dptr);
		free(db_key.dptr);
		db_key = db_key_next;
	};
	gdbm_close(db_file);
	exit(0);
}
