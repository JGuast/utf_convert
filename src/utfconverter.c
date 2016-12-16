#include <utfconverter.h>
#include <sys/utsname.h>
#include <sys/stat.h>
#include <math.h>

char* filename;
/*char* out_file;*/
endianness source;
endianness conversion;
int out_fd;
int verbosity;
clock_t start, stop, start_r;
struct tms* timebuff;
double w_time[3];
double r_time[3];
double c_time[3];
int counters[3];
int out_index, in_index;



int main(int argc, char** argv)
{
	/* variable declarations as per ISO C90 */
	int fd, rv, size, append_flag, bom_flag;
	unsigned char buf[4];
	unsigned char outbom_buf[3];
	Glyph* glyph;
	void* memset_return;
	unsigned char daBOM, BOM2;
	char absbuff[50000];
	char* abspath;
	char hostname[1024];
	struct utsname unameData;
	double a_count, s_count;

	/* arrays to track time */
	/* real, user, system */
	memset(w_time, 0, sizeof(w_time));
	memset(r_time, 0, sizeof(r_time));
	memset(c_time, 0, sizeof(c_time));

	memset(counters, 0, sizeof(counters));

	/* set default verbosity*/
	verbosity = 0;
	out_fd = -1;
	out_index = -2;
	in_index = 1;
	append_flag = 0;
	bom_flag = 1;

	/* After calling parse_args(), filename and conversion should be set. */
	parse_args(argc, argv);
	timebuff = malloc(sizeof(struct tms) + 1);

	/* try to open the input file */
	fd = open(argv[in_index], O_RDONLY);
	/* check to make sure the file opened properly */
	if (fd == -1)
	{
		fprintf(stderr, "Invalid input filename given.\n");
		free(timebuff);
		print_help(1);
	}

	/* if output file has been specified */
	if (out_fd == -1)
	{

		out_fd = open(argv[out_index], O_RDWR | O_APPEND); /*out_fd = open(out_file, O_WRONLY | O_APPEND | O_CREAT, 00600);*/
		/* If the file wasn't there, create it with write permissions */
		if (out_fd == -1) 
		{
			out_fd = open(argv[out_index], O_CREAT, S_IWUSR | S_IRUSR);
			close(out_fd);
			out_fd = open(argv[out_index], O_WRONLY | O_APPEND);
		}
		else 
			append_flag = 1;
	}

	/* initialize some vars */
	buf[0] = 0;
	buf[1] = 0;
	buf[2] = 0;
	buf[3] = 0;
	rv = 0; 

	outbom_buf[0] = 0;
	outbom_buf[1] = 0;
	outbom_buf[2] = 0;

	glyph = malloc(sizeof(Glyph)+1);  /* malloc space for glyph */
	glyph->end = conversion; /* set the output endianness */
	
	/* Handle BOM bytes for UTF16 specially. 
     * Read our values into the first and second elements. */
	start_r = clock();
	if((rv = read(fd, &buf[0], 1)) == 1 && (rv = read(fd, &buf[1], 1)) == 1)
	{ 
		if(buf[0] == 0xfe && buf[1] == 0xff) /* BE --> 0xfeff */
		{
			/*file is big endian*/
			source = BIG; 
		} 
		else if(buf[0] == 0xff && buf[1] == 0xfe) /* LE --> 0xfffe */
		{
			/*file is little endian*/
			source = LITTLE;
		} 
		else if (buf[0] != 0xef || buf[1] != 0xbb) /* UTF-8 --> 0xefbbbf */
		{
			/*file has no BOM*/
			free(glyph); 
			free(timebuff);
			fprintf(stderr, "File has no BOM.\n");
			quit_converter(fd, 1); /* CHANGE NO_FD to fd*/
		}
		else if ((rv = read(fd, &buf[2], 1)) != 1 || buf[2] != 0xbf) /* HANDLE A UTF-8 BOM */
		{
			/* file has no BOM */
			free(glyph); 
			free(timebuff);
			fprintf(stderr, "File has no BOM.\n");
			quit_converter(fd, 1); /* CHANGE NO_FD to fd */
		}
		else 
		{
			source = EIGHT;
		}
		
		/* IF APPENDING, DOES SRC BOM == CONVERSION  */
		if (append_flag == 1) /* if appending */
		{
			lseek(out_fd, 0, SEEK_SET); /* lseek to beginning */
			if ((rv = read(out_fd, &outbom_buf[0], 1)) == 1 && (rv = read(out_fd, &outbom_buf[1], 1)) == 1) /* try reading first 2 bytes */
			{
				 /* if they are the same as conversion */
				if (((outbom_buf[0] == 0xfe && outbom_buf[1] == 0xff) && conversion == BIG) || 
					((outbom_buf[0] == 0xff && outbom_buf[1] == 0xfe) && conversion == LITTLE))
				{
					bom_flag = 0;
					lseek(out_fd, 0, SEEK_END);
				}
				else 
				{
					fprintf(stderr, "Attempting to append to a file with invalid format.\n");
				 	free(glyph); 
					free(timebuff);
					close(out_fd);
					quit_converter(fd, 1);
				}
			}
		}

		memset_return = memset(glyph, 0, sizeof(Glyph)+1);
		/* Memory write failed, recover from it: */
		if(memset_return == NULL)
			memset(glyph, 0, sizeof(Glyph)+1);
	}
	stop = clock();
	times(timebuff);
	r_time[0] += (double)(stop - start_r) / CLOCKS_PER_SEC;
	r_time[1] += (double)timebuff->tms_utime;
	r_time[2] += (double)timebuff->tms_stime;

	/* out_fd = open("../rsrc/out.txt", O_RDWR); */
	if (bom_flag == 1) /* only write BOM if not appending */
	{
		daBOM = '0';
		if (conversion == LITTLE)
		{
			daBOM = 0xff;
			BOM2 = 0xfe;
		}
		else if (conversion == BIG)
		{
			daBOM = 0xfe;
			BOM2 = 0xff;
		}

		write(out_fd, &daBOM, 1);
		write(out_fd, &BOM2, 1);
	}

	if (source == EIGHT)
	{
		/* Now deal with the rest of the bytes. 8 to 16 only*/
		while((rv = read(fd, &buf[0], 1)) == 1)
		{
			write_glyph(fill_glyph(glyph, buf, source, &fd)); /* write_glyph(fill_glyph()) */
			memset_return = memset(glyph, 0, sizeof(Glyph)+1);
	        	/* Memory write failed, recover from it: */
	        	if(memset_return == NULL)
			        memset(glyph, 0, sizeof(Glyph)+1);
		}
	}
	else 
	{
		/* Now deal with the rest of the bytes. 16 to 16 only*/
		start_r = clock();
		while((rv = read(fd, &buf[0], 1)) == 1 && (rv = read(fd, &buf[1], 1)) == 1)
		{
			write_glyph(fill_glyph(glyph, buf, source, &fd)); /* write_glyph(fill_glyph()) */
			memset_return = memset(glyph, 0, sizeof(Glyph)+1);
	        	/* Memory write failed, recover from it: */
	        	if(memset_return == NULL)
			        memset(glyph, 0, sizeof(Glyph)+1);
		}
	}


	/* Verbosity */
	if (verbosity > 0)
	{
		/* input file size */
		size = lseek(fd, 0, SEEK_END);
		fprintf(stderr, "Input file size: %d kb\n", (int)round(((double)size)/1000)); 

		/* absolute path (stat) */
		abspath = realpath(argv[in_index], absbuff);
		fprintf(stderr, "Input file path: %s\n", abspath);

		/* source */
		if (source == LITTLE)
			fprintf(stderr, "Input file encoding: UTF-16LE\n"); 
		else if (source == BIG)
			fprintf(stderr, "Input file encoding: UTF-16BE\n");
		else 
			fprintf(stderr, "Input file encoding: UTF-8\n");

		/* conversion */
		if (conversion == LITTLE)
			fprintf(stderr, "Output encoding: UTF-16LE\n"); 
		else if (conversion == BIG)
			fprintf(stderr, "Output encoding: UTF-16BE\n");

		/* Host */
		memset(hostname, 0, 1024);
		gethostname(hostname, 1023);
		fprintf(stderr, "Hostmachine: %s\n", hostname);

		uname(&unameData);
		fprintf(stderr, "Operating system: %s\n", unameData.sysname); /* OS */
	
		if (verbosity == 2) /* Verbosity level 2 */
		{
			a_count = (((double)counters[0])/ counters[2]) * 100;
			s_count = (((double)counters[1])/ counters[2]) * 100;

			fprintf(stderr, "Reading: real=%.1f user=%.1f sys=%.1f\n", r_time[0], r_time[1], r_time[2]);
			fprintf(stderr, "Converting: real=%.1f user=%.1f sys=%.1f\n", c_time[0], c_time[1], c_time[2]);
			fprintf(stderr, "Writing: real=%.1f user=%.1f sys=%.1f\n", w_time[0], w_time[1], w_time[2]);
			fprintf(stderr, "ASCII: %d%% \n", (int)round(a_count));
			fprintf(stderr, "Surrogates: %d%% \n", (int)round(s_count));
			fprintf(stderr, "Glyphs: %d\n", counters[2]);
		}
	}

	free(timebuff);
	free(glyph);
	quit_converter(fd, 1); /* CHANGE NO_FD to fd */
	return 0;
}

/**
 * A function that converts a UTF-8 glyph to a UTF-16LE or UTF16BE
 * glyph, and returns the result as a pointer to the converted glyph.
 *
 * @param glyph - the UTF-8 glyph to convert.
 * @param end - the endianness to convert to (UTF16LE or UTF16BE).
 * @return - the converted glyph.
 */
Glyph* convert(Glyph* glyph, endianness end)
{
	end = LITTLE;
	glyph->end = end;
	/* Do some witchcaraft to make this work */
	return glyph;
}

Glyph* swap_endianness(Glyph* glyph)
{
	/* Use XOR to be more efficient with how we swap values. */
	glyph->bytes[0] ^= glyph->bytes[1];
	glyph->bytes[1] ^= glyph->bytes[0];
	if(glyph->surrogate){  /* If a surrogate pair, swap the next two bytes. */
		glyph->bytes[2] ^= glyph->bytes[3];
		glyph->bytes[3] ^= glyph->bytes[2];
	}
	glyph->end = conversion;
	return glyph;
}

/**
 * Fills in a glyph with the given data in data[2], with the endianness given
 * by end.
 *
 * @param glyph 	The pointer to the glyph struct to fill in with bytes.
 * @param data[2]	The array of data to fill the glyph struct with.
 * @param end	   	The endianness enum of the glyph.
 * @param fd 		The int pointer to the file descriptor of the input file.
 * @return Returns a pointer to the filled-in glyph.
 */
Glyph* fill_glyph(Glyph* glyph, unsigned char data[4], endianness end, int* fd) 
{	
	/* int num_bytes; */
	int ready, msb, lsb;
	
	unsigned int bits;
	unsigned long code_point;

	if (end == EIGHT) /* Handle the UTF-8 to UTF-16 conversion */
	{
		ready = 0;
		code_point = 0;
		/*num_bytes = 0;*/

		if (data[0] <= 0x7f) /* 0x7F for 1 byte 	  -- 0xxx xxxx */
		{
			counters[0]++; /* increment ascii count data[0] <= 127 */
			glyph->bytes[0] = data[0];
			
			/*num_bytes = 1;*/
			code_point += glyph->bytes[0];
		}
		else if (data[0] <= 0xdf) /* 0xDF for 2 bytes -- 110x xxxx 10xx xxxx*/
		{
			glyph->bytes[0] = data[0];
			if ((ready = read(*fd, &data[1], 1)) == 1)
			{
				glyph->bytes[1] = data[1];
				
				/*num_bytes = 2;*/
				code_point += (glyph->bytes[1] & 0x3f);
				code_point += (glyph->bytes[0] & 0x1f) << 6;
			}
		}
		else if (data[0] <= 0xef) /* 0xEF for 3 bytes -- 1110 xxxx 10xx xxxx 10xx xxxx*/
		{
			glyph->bytes[0] = data[0];
			if ((ready = read(*fd, &data[1], 1)) == 1 && (ready = read(*fd, &data[2], 1)) == 1)
			{
				glyph->bytes[1] = data[1];
				glyph->bytes[2] = data[2];
				
				/*num_bytes = 3;*/
				code_point += (glyph->bytes[2] & 0x3f);
				code_point += (glyph->bytes[1] & 0x3f) << 6;
				code_point += (glyph->bytes[0] & 0x0f) << 12;
			}
		}
		else if (data[0] <= 0xf7) /* 0xFF for 4 bytes -- 1111 0xxx 10xx xxxx 10xx xxxx 10xx xxxx */
		{
			glyph->bytes[0] = data[0];
			if ((ready = read(*fd, &data[1], 1)) == 1 && (ready = read(*fd, &data[2], 1)) == 1 && (ready = read(*fd, &data[3], 1)) == 1)
			{
				glyph->bytes[1] = data[1];
				glyph->bytes[2] = data[2];
				glyph->bytes[3] = data[3];
				
				/*num_bytes = 4;*/
				code_point += (glyph->bytes[3] & 0x3f);
				code_point += (glyph->bytes[2] & 0x3f) << 6;
				code_point += (glyph->bytes[1] & 0x3f) << 12;
				code_point += (glyph->bytes[0] & 0x07) << 18;
			}
		}
		else 
		{
			printf("Not a valid UTF-8 encoding.\n");
			quit_converter(*fd, 1); /* CHANGE NO_FD to */
		}

		/* Print statements for debugging on Sparky
		printf("bytes[0] -- > 0x%1x\n", glyph->bytes[0]);
		printf("bytes[1] -- > 0x%1x\n", glyph->bytes[1]);
		printf("bytes[2] -- > 0x%1x\n", glyph->bytes[2]);
		printf("bytes[3] -- > 0x%1x\n", glyph->bytes[3]);
		printf("bits ------ > 0x%lx\n", code_point);
		*/

		/* 1. Determine if the code point is greater than 0x10000 */
		if (code_point > 0x10000)
		{
			msb = 0;
			lsb = 0;

			/* 2. If yes, then subtract 0x10000 from the code point */
			code_point = code_point - 0x10000;

			/* 3. Create the MSB code unit */
			msb = (code_point >> 10) + 0xD800;

			/* 4. Crete the LSB code unit */
			lsb = (code_point & 0x3ff) + 0xDC00;

			/* 5. Combine the MSB and LSB code unit depending on endianness */
			/* Glyphs will always be LE anyway, so it will always be the same */
			/*printf("MSB - 0x%1x\n", msb);  0xd835 should be 0x35d8 in glyph */
			/*printf("LSB - 0x%1x\n", lsb);  0xdc9c should be 0x9cdc in the glyph */
			
			glyph->bytes[0] = (msb & 0x00ff); /* get the 0x35 */
			glyph->bytes[1] = (msb & 0xff00) >> 8; /* get the 0xd8 */

			glyph->bytes[2] = (lsb & 0x00ff); /* get the 9c */
			glyph->bytes[3] = (lsb & 0xff00) >> 8; /* get the dc */

			glyph->surrogate = 1; /* set surrogate flag to true */
		}
		else /* Code point >= 0x10000 */
		{
			/*
			printf("##### bytes[0] -- > 0x%1x\n", glyph->bytes[0]);
			printf("##### bytes[1] -- > 0x%1x\n", glyph->bytes[1]);
			printf("##### bytes[2] -- > 0x%1x\n", glyph->bytes[2]);
			printf("##### bytes[3] -- > 0x%1x\n", glyph->bytes[3]);
			printf("##### bits ------ > 0x%lx\n", code_point);
			*/
			/* copy the value to glyph */
			glyph->bytes[0] = (code_point & 0x00ff);
			glyph->bytes[1] = (code_point & 0xff00) >> 8;
			glyph->surrogate = 0; /* set surrogate flag to false */
		}
	}
	else
	{
		bits = 0;

		if (end == LITTLE) /* IF source == LITTLE, then data is in correct spot */
		{
			glyph->bytes[0] = data[0];
			glyph->bytes[1] = data[1];
		}
		else if (end == BIG)/* else source == BIG, so data[0] and data[1] need to be copied in reverse order */
		{
			glyph->bytes[0] = data[1];
			glyph->bytes[1] = data[0];
		}

		/* fd++; EDIT: I seriously forgot why this is here. Leave it. It's magic.*/

		if (glyph->bytes[0] <= 0x7f && glyph->bytes[1] == 0x00){
			counters[0]++; /* ASCII found -- off by one (EOF?) */
		}

		/* bytes are stored as 0x01d8 */
		/* we want to get this 0xd801 */
		bits = glyph->bytes[0] + (glyph->bytes[1] << 8);

		/* Check high surrogate pair using its special value range.*/
		if(bits >= 0xD800 && bits <= 0xDBFF)
		{ 
			ready = 0;
			if((ready = read(*fd, &data[2], 1)) == 1 && (ready = read(*fd, &data[3], 1)) == 1)
			{
				/* convert to LE glyph */
				if (source == LITTLE) 
				{
					glyph->bytes[2] = data[2];
					glyph->bytes[3] = data[3];
					bits = (glyph->bytes[2] + (glyph->bytes[3] << 8));
				}
				else /* else source == BIG, so data[0] and data[1] need to be copied in reverse order */
				{
					glyph->bytes[2] = data[3];
					glyph->bytes[3] = data[2];
					bits = (glyph->bytes[2] + (glyph->bytes[3] << 8)); 
				}

				if(bits >= 0xDC00 && bits <= 0xDFFF) 
				{
					glyph->surrogate = true;
					counters[1]++;
				}
				else 
				{	
					glyph->surrogate = false; 
					lseek(*fd, -OFFSET, SEEK_CUR); /* go to current - offset, (OFFSET = 2 bytes) */
				}
			}
		}
	}

	glyph->end = conversion;

	stop = clock();
	times(timebuff);
	c_time[0] += (double)(stop - start) / CLOCKS_PER_SEC;
	c_time[1] += (double)timebuff->tms_utime;
	c_time[2] += (double)timebuff->tms_stime;

	counters[2] += 1; /* increment glyph counter */
	return glyph;
}

void write_glyph(Glyph* glyph)
{
	unsigned char b1;
	unsigned char b2;

	/* Only to be used if a surrogate pair is found */
	unsigned char s1;
	unsigned char s2;

	start = clock();
	
	if (glyph->end == LITTLE) /* Swapped 0 and 1 for both value aggisnments */
	{
		b1 = glyph->bytes[0];
		b2 = glyph->bytes[1];

		write(out_fd, &b1, 1);
		write(out_fd, &b2, 1);

		if (glyph->surrogate)
		{
			s1 = glyph->bytes[2];
			s2 = glyph->bytes[3];

			write(out_fd, &s1, 1);
			write(out_fd, &s2, 1);
		}
	}
	else if (glyph->end == BIG)
	{
		b1 = glyph->bytes[1];
		b2 = glyph->bytes[0];

		write(out_fd, &b1, 1);
		write(out_fd, &b2, 1);

		if (glyph->surrogate)
		{
			s1 = glyph->bytes[3];
			s2 = glyph->bytes[2];

			write(out_fd, &s1, 1);
			write(out_fd, &s2, 1);
		}
	}
	
	stop = clock();
	times(timebuff);
	w_time[0] += (double)(stop - start) / CLOCKS_PER_SEC;
	w_time[1] += (double)timebuff->tms_utime;
	w_time[2] += (double)timebuff->tms_stime;
}

/**
 * Calls getopt() and parses arguments.
 *
 * @param argc The number of arguments.
 * @param argv The arguments as an array of string.
 */
void parse_args(int argc, char** argv)
{
	int option_index, c;
	
	/* If getopt() returns with a valid return code (its working correctly),
	 * then process the args! */    
	while((c = getopt_long(argc, argv, "hu:v", long_options, &option_index)) != -1)
	{
		switch(c)
		{ 
			case 'h':
				print_help(0);
				break;
			case 'u':
				if (optarg == NULL)
				{
					/* Conversion mode not given */
					fprintf(stderr, "Error: Unrecognized argument.\n");
					print_help(1);
				}
				if (strcmp(optarg, "16LE") == 0)
				{
					conversion = LITTLE;
				}
				else if (strcmp(optarg, "16BE") == 0)
				{
					conversion = BIG;
				}
				else
				{
					/* Conversion mode not recognized */
					fprintf(stderr, "Error: Unrecognized argument.\n");
					print_help(1);
				}
				break;
			case 'v':
				/* Check for vv */
				if (verbosity < 2)
					verbosity++;
				break;
			default:
				fprintf(stderr, "Error: Unrecognized argument.\n");
				print_help(1);
				break;
		}

	}

	if(optind < argc)
	{
		if((optind + 1) < argc) /* if optind+1 < argc then you got an aout file, bitch */
			out_index = optind + 1;
		else 
			out_fd = 1; /* if no out_file, then output fd is 1 for stdout*/

		in_index = optind;

		/* If there is an output file, check that it's not the same as the input */
		if (out_index != -2)
		{
			if (strcmp(argv[in_index], argv[out_index]) == 0)
			{
				fprintf(stderr, "Error: Input and output file are the same.\n");
				print_help(1);
			}
	}
	} 
	else 
	{
		fprintf(stderr, "Error: Filename not given.\n");
		print_help(1);
	}
}

void print_help(int code) 
{
	int i;
	for(i = 0; i < 10; i++)
	{
		printf("%s", USAGE[i]); 
	}
	quit_converter(NO_FD, code);
}

/* Be sure to pass flags 0 for success, and 1 for failure*/
void quit_converter(int fd, int flag)
{
	close(STDERR_FILENO);
	close(STDIN_FILENO);
	close(STDOUT_FILENO);

	/* if output fd is -1 dont close, if 1 then already closed */
	if (out_fd != -1 && out_fd != 1) 
		close(out_fd);

	if(fd != NO_FD)
		close(fd);

	if (flag == 0)
		exit(EXIT_SUCCESS);
	else
		exit(EXIT_FAILURE);
}
