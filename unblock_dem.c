/*
 * =========================================================================
 * unblock_dem - A program to add newlines to a USGS DEM file.
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
 *
 * This program unblocks the information in a DEM file
 * by adding a newline to each record.
 * It sets the last byte in each record (which should always be
 * a blank) to a newline.
 *
 * The program reads from stdin and writes to stdout.
 */
#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>
#include "drawmap.h"
#include "dem.h"

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

int
main(int argc, char *argv[])
{
	unsigned char buf[DEM_RECORD_LENGTH];
	int32_t start_flag = 0;
	ssize_t ret_val;
	int32_t i;

	if ((argc == 2) && (argv[1][0] == '-') && (argv[1][1] == 'L'))  {
		license();
		exit(0);
	}
	else if (argc != 1)  {
		fprintf(stderr, "Usage:  %s < dem_file\n", argv[0]);
		exit(0);
	}

	while ((ret_val = read(0, buf, DEM_RECORD_LENGTH)) == DEM_RECORD_LENGTH)  {
		if (start_flag == 0)  {
			/*
			 * Check for newlines in the first read
			 * to try to prevent people from converting files
			 * that already have newlines in them.
			 */
			for (i = 0; i < DEM_RECORD_LENGTH; i++)  {
				if (buf[i] == '\n')  {
					fprintf(stderr, "This file already has newlines in it.  Aborting.\n");
					exit(0);
				}
			}
			start_flag = 1;
		}
		if (buf[DEM_RECORD_LENGTH - 1] != ' ')  {
			/*
			 * In my experience, no optional-format DEM file has non-white-space
			 * in the last byte of a record.  Don't do the conversion if there
			 * is actual data there.
			 */
			fprintf(stderr, "This file may have formatting problems.  Aborting.\n");
			exit(0);
		}
		buf[DEM_RECORD_LENGTH - 1] = '\n';
		write(1, buf, ret_val);
	}

	if ((ret_val != 0) && (ret_val != DEM_RECORD_LENGTH))  {
		fprintf(stderr, "read() returned %d\n", (int)ret_val);
		exit(0);
	}

	exit(0);
}
