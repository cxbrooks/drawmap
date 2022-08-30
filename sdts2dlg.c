/*
 * =========================================================================
 * sdts2dlg.c - A program to convert USGS SDTS DLG files to optional-format USGS DLG files.
 * Copyright (c) 2000,2008  Fred M. Erickson
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * =========================================================================
 */


#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <errno.h>
#include <time.h>
#include <string.h>
#include "drawmap.h"
#include "dlg.h"
#include "sdts_utils.h"



int32_t get_extra_attrib(int32_t, int32_t *major, int32_t *minor, struct subfield *subfield);


void
license(void)
{
	fprintf(stderr, "This program is free software; you can redistribute it and/or modify\n");
	fprintf(stderr, "it under the terms of the GNU General Public License as published by\n");
	fprintf(stderr, "the Free Software Foundation; either version 2, or (at your option)\n");
	fprintf(stderr, "any later version.\n\n");

	fprintf(stderr, "This program is distributed in the hope that it will be useful,\n");
	fprintf(stderr, "but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
	fprintf(stderr, "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
	fprintf(stderr, "GNU General Public License for more details.\n\n");

	fprintf(stderr, "You should have received a copy of the GNU General Public License\n");
	fprintf(stderr, "along with this program; if not, write to the Free Software\n");
	fprintf(stderr, "Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.\n");
}



/*
 * This program takes an SDTS file set as input,
 * and produces a single optional-format DLG file
 * as output.
 */
int
main(int argc, char *argv[])
{
	int dlg_fdesc;
	int32_t length;
	int32_t gz_flag;
	ssize_t (*read_function)();


	if ((argc == 2) && (argv[1][0] == '-') && (argv[1][1] == 'L'))  {
		license();
		exit(0);
	}
	if ((argc != 2) && (argc != 3))  {
		fprintf(stderr, "Usage:  %s ????LE@@.DDF [output_file_name]\n", argv[0]);
		fprintf(stderr, "        Where the ???? are alphanumeric characters, and @ represents a digit.\n");
		exit(0);
	}


	/* Find file name length. */
	length = strlen(argv[1]);
	if (length < 12)  {
		fprintf(stderr, "File name %s appears too short to be valid.  Should look like ????LE@@.DDF\n", argv[1]);
		exit(0);
	}

	/*
	 * Figure out if the file is gzip-compressed or not.
	 */
	if ((strcmp(&argv[1][length - 3], ".gz") == 0) ||
	    (strcmp(&argv[1][length - 3], ".GZ") == 0))  {
		gz_flag = 1;
		if ((dlg_fdesc = buf_open_z(argv[1], O_RDONLY)) < 0)  {
			fprintf(stderr, "Can't open %s for reading, errno = %d\n", argv[1], errno);
			exit(0);
		}
		read_function = buf_read_z;
	}
	else  {
		gz_flag = 0;
		if ((dlg_fdesc = buf_open(argv[1], O_RDONLY)) < 0)  {
			fprintf(stderr, "Can't open %s for reading, errno = %d\n", argv[1], errno);
			exit(0);
		}
		read_function = buf_read;
	}


	/*
	 * Files in Spatial Data Transfer System (SDTS) format are markedly
	 * different from the optional-format DLG files.
	 *
	 * We insist that the user specify one, single, SDTS file
	 * on the command line.
	 * The file must be the one whose name has the form ????LE??.DDF
	 * (or ????le??.ddf), and it may have a .gz on the end if it is gzip
	 * compressed.
	 *
	 * We allow the files to be gzip-compressed, and they can have either
	 * ".gz" or ".GZ" on the end.  However, we insist that the rest of
	 * the file name have consistent case.  That is, if the 'F' or 'f'
	 * in the ".DDF" or ".ddf" is in a given case, the rest of the file
	 * had better be in that same case.
	 *
	 * If the following "if" test succeeds, we assume we have an SDTS file.
	 */
	if (((length >= 7) && (gz_flag != 0) &&
	     ((strncmp(&argv[1][length - 7], ".ddf", 4) == 0) ||
	      (strncmp(&argv[1][length - 7], ".DDF", 4) == 0))) ||
	    ((length >= 4) && (gz_flag == 0) &&
	     ((strcmp(&argv[1][length - 4], ".ddf") == 0) ||
	      (strcmp(&argv[1][length - 4], ".DDF") == 0))))  {
		/* SDTS file */

		/* Close the file.  We will reopen it in parse_full_dlg_sdts(). */
		if (gz_flag == 0)  {
			buf_close(dlg_fdesc);
		}
		else  {
			buf_close_z(dlg_fdesc);
		}

		/*
		 * Check that the file name takes the form that we expect.
		 */
		if (((gz_flag != 0) &&
		     ((strncmp(&argv[1][length - 11], "le", 2) != 0) &&
		      (strncmp(&argv[1][length - 11], "LE", 2) != 0))) ||
		    ((gz_flag == 0) &&
		     (strncmp(&argv[1][length - 8], "le", 2) != 0) &&
		     (strncmp(&argv[1][length - 8], "LE", 2) != 0)))  {
			fprintf(stderr, "The file %s looks like an SDTS file, but the name doesn't look right.\n", argv[1]);
			exit(0);
		}


		/*
		 * The input file name looks okay.  Let's launch into the information parsing.
		 *
		 * process_dlg_sdts() will create and write the output file.
		 */
		if (argc == 3)  {
			if (process_dlg_sdts(argv[1], argv[2], gz_flag, (struct image_corners *)0, 0, 1) != 0)  {
				exit(0);
			}
		}
		else  {
			if (process_dlg_sdts(argv[1], (char *)0, gz_flag, (struct image_corners *)0, 0, 1) != 0)  {
				exit(0);
			}
		}
	}

	exit(0);
}
