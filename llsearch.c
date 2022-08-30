/*
 * =========================================================================
 * llsearch - A program that extracts a subset of the data in a GNIS file.
 * Copyright (c) 1997,1998,1999,2000,2008  Fred M. Erickson
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
 *
 * This is a filter to search a GNIS file for any entries within
 * the specified latitude/longitude box.
 *
 * Each US state has its own GNIS file, but some of the larger
 * states span many degrees of latitude and longitude.
 * It is convenient to be able to pull out just the data you need
 * for a given map.
 *
 * The program reads from stdin and writes to stdout.
 */
#include <sys/types.h>
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include "drawmap.h"


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
	char buf[2001];
	char *tok_ptr;
	double latitude_low;
	double latitude_high;
	double longitude_low;
	double longitude_high;
	double latitude;
	double longitude;
	ssize_t ret_val;
	int i;

	if (argc != 5)  {
		if ((argc == 2) && (argv[1][0] == '-') && (argv[1][1] == 'L'))  {
			license();
			exit(0);
		}

		fprintf(stderr, "Usage:  %s latitude_low longitude_low latitude_high longitude_high\n", argv[0]);
		fprintf(stderr, "        (The latitude/longitude values are in decimal degrees.)\n");
		fprintf(stderr, "        (West longitude is negative and south latitude is negative.)\n");
		fprintf(stderr, "        (%s reads from stdin and writes to stdout.)\n", argv[0]);
		exit(0);
	}
	latitude_low = strtod(argv[1], (char **)0);	/* We assume that these are in decimal degrees */
	longitude_low = strtod(argv[2], (char **)0);	/* We assume that these are in decimal degrees */
	latitude_high = strtod(argv[3], (char **)0);	/* We assume that these are in decimal degrees */
	longitude_high = strtod(argv[4], (char **)0);	/* We assume that these are in decimal degrees */

	if (latitude_low > latitude_high)  {
		latitude = latitude_high;
		latitude_high = latitude_low;
		latitude_low = latitude;
	}
	if (longitude_low > longitude_high)  {
		longitude = longitude_high;
		longitude_high = longitude_low;
		longitude_low = longitude;
	}

	if ((latitude_high < -90.0) || (latitude_high > 90.0) ||
	    (latitude_low < -90.0) || (latitude_low > 90.0) ||
	    (longitude_high < -180.0) || (longitude_high > 180.0) ||
	    (longitude_low < -180.0) || (longitude_low > 180.0))  {
		fprintf(stderr, "Error:  Parameters appear incorrect\n");
		exit(0);
	}
	if ((latitude_high < latitude_low) || (longitude_high < longitude_low))  {
		fprintf(stderr, "Error:  Parameters appear incorrect\n");
		exit(0);
	}

	while ((ret_val = get_a_line(0, buf, 2000)) > 0)  {
		buf[ret_val] = '\0';

		/*
		 * We need to figure out whether it is an old-style or new-style record.
		 */
		if ((tok_ptr = strstr(buf, "\",\"")) != (char *)0)  {
			/* New-style record. */
			if ((tok_ptr + 3) < (buf + ret_val))  {
				tok_ptr += 3;
			}
			else  {
				fprintf(stderr, "Defective GNIS record:  <%s>\n", buf);
				continue;
			}
			for (i = 0; i < 7; i++)  {
				if (((tok_ptr = strstr(tok_ptr, "\",\"")) != (char *)0) && (*tok_ptr != '\0'))  {
					if ((tok_ptr + 3) < (buf + ret_val))  {
						tok_ptr += 3;
					}
					else  {
						break;
					}
				}
				else  {
					break;
				}
			}
			if (i != 7)  {
				/*
				 * If i != 7, then we ran out of data before finding
				 * the latitude.  Skip the record.
				 */
				fprintf(stderr, "Defective GNIS record:  <%s>\n", buf);
				continue;
			}
			latitude = atof(tok_ptr);
			if (((tok_ptr = strstr(tok_ptr, "\",\"")) != (char *)0) && (*tok_ptr != '\0') && (*(tok_ptr + 3) != '\0'))  {
				tok_ptr += 3;
				longitude = atof(tok_ptr);
			}
			else  {
				fprintf(stderr, "Defective GNIS record:  <%s>\n", buf);
				continue;
			}
		}
		else  {
			/* Old-style record. */
			if (ret_val < 96)  {
				/* The record is too short to process.  Ignore it. */
				fprintf(stderr, "Defective GNIS record (too short):  <%s>\n", buf);
				continue;
			}

			/*
			 * Note:  We assume latitude_low, longitude_low, latitude_high, and longitude_high
			 * were entered in decimal degrees.
			 * latitude and longitude from the old-style GNIS files, however are in DDDMMSS format, and
			 * require special conversion functions.
			 */
			if ((buf[86] != 'N') && (buf[86] != 'S'))  {
				/* Defective record */
				fprintf(stderr, "Defective GNIS record (latitude defective):  <%s>\n", buf);
				continue;
			}
			if ((buf[95] != 'E') && (buf[95] != 'W'))  {
				/* Defective record */
				fprintf(stderr, "Defective GNIS record (longitude defective):  <%s>\n", buf);
				continue;
			}
			latitude = lat_conv(&buf[80]);
			longitude = lon_conv(&buf[88]);
		}

		if ((latitude >= latitude_low) &&
		    (latitude <= latitude_high) &&
		    (longitude >= longitude_low) &&
		    (longitude <= longitude_high))  {
			write(1, buf, ret_val);
		}
	}

	exit(0);
}
