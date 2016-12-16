#include <stdio.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <getopt.h>
#include <string.h>
#include <stdbool.h>
#include <time.h>
#include <sys/times.h>


#define MAX_BYTES 4 /* 2 */
#define SURROGATE_SIZE 4
#define NON_SURROGATE_SIZE 2
#define NO_FD -1
#define OFFSET 2

#define FIRST  0 /* 10000000 */
#define SECOND 1 /* 10000000 */
#define THIRD  2 /* 30000000 */
#define FOURTH 3 /* 40000000 */

#ifdef __STDC__
#define P(x) x
#else
#define P(x) ()
#endif

/* The enum for endianness. */
typedef enum {LITTLE, BIG, EIGHT} endianness;

/* The struct for a codepoint glyph. */
typedef struct Glyph {
	unsigned char bytes[MAX_BYTES];
	endianness end;
	int surrogate;
} Glyph;

static struct option long_options[] = {
		{"help",     no_argument, 		0, 'h'},
		{"UTF", 	 required_argument, 0, 'u'},
		{"vv", no_argument, 0, 'v'},
		{NULL, 0, NULL, 0}
};

extern int in_index;
extern int out_index; /* sparky is dumb */

extern clock_t start;
extern clock_t stop;
extern clock_t start_r;

extern double r_time[3];
extern double w_time[3];
extern double c_time[3];

extern int counters[3];

/* file descriptor for output file */
extern int out_fd;

/* file name for output file */
/*extern char* out_file;*/

/* verbosity */
extern int verbosity;

/* The given filename. */
extern char* filename; 

/* The usage statement. */
const char* USAGE[] = { "./utf [-h|--help] [-v|--v] -u OUT_ENC | --UTF=OUT_ENC IN_FILE [OUT_FILE]\n\n", 
						"Option arguments:\n",
						"\t-h, --help\tDisplays this usage.\n", 
						"\t-v, -vv\t\tToggles the verbosity of the program to level 1 or 2.\n\n",
						"Mandatory argument:\n",
						"\t-u OUT_ENC, --UTF=OUT_ENC\tSets the output encoding.\n",
						"\t\t\t\tValid values for OUT_ENC: 16LE, 16BE\n\n", 
						"Positional Arguments:\n",
						"\tIN_FILE\t\tThe file to convert.\n",
						"\t[OUT_FILE]\tOutput file name. If not present, defaults to stdout.\n\n"};

/* Which endianness to convert to. */
extern endianness conversion;

/* Which endianness the source file is in. */
extern endianness source;

/**
 * A function that converts a UTF-8 glyph to a UTF-16LE or UTF16BE
 * glyph, and returns the result as a pointer to the converted glyph.
 *
 * @param glyph - the UTF-8 glyph to convert.
 * @param end - the endianness to conver to (UTF16LE or UTF16BE).
 * @return - the converted glyph.
 */
Glyph* convert(Glyph* glyph, endianness end);

/**
 * A function that swaps the endianness of the bytes of an encoding from
 * LE to BE and vice versa.
 *
 * @param glyph The pointer to the glyph struct to swap.
 * @return Returns a pointer to the glyph that has been swapped.
 */
Glyph* swap_endianness P((Glyph*));

/**
 * Fills in a glyph with the given data in data[2], with the given endianness 
 * by end.
 *
 * @param glyph 	The pointer to the glyph struct to fill in with bytes.
 * @param data[2]	The array of data to fill the glyph struct with.
 * @param end	   	The endianness enum of the glyph.
 * @param fd 		The int pointer to the file descriptor of the input file.
 * @return Returns a pointer to the filled-in glyph.
 */
Glyph* fill_glyph P((Glyph*, unsigned char[4], endianness, int*));

/**
 * Writes the given glyph's contents to stdout.
 *
 * @param glyph The pointer to the glyph struct to write to stdout.
 */
void write_glyph P((Glyph*));

/**
 * Calls getopt() and parses arguments.
 *
 * @param argc The number of arguments.
 * @param argv The arguments as an array of string.
 */
void parse_args P((int, char**));

/**
 * Prints the usage statement.
 */
void print_help P((int));

/**
 * Closes file descriptors and frees list and possibly does other
 * bookkeeping before exiting.
 *
 * @param The fd int of the file the program has opened. Can be given
 * the macro value NO_FD (-1) to signify that we have no open file
 * to close.
 */
void quit_converter P((int, int));
