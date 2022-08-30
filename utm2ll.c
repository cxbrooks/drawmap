/*
 * =========================================================================
 * utm2ll - A program to convert UTM coordinates to latitude/longitude coordinates
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
 *
 * Program to use Redfearn's formulas to convert UTM coordinates
 * to latitude/longitude geographical coordinates.
 *
 * There aren't a lot of comments in this program because it is
 * basically a wrapper that calls the appropriate conversion function
 * in the file utilities.c.  See the comments there for a description
 * of the conversion process.
 */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
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
	double utm_x, utm_y, longitude, latitude;
	int32_t zone;
	int32_t dtype;
	struct datum datum;

	if ((argc != 4) && (argc != 5))  {
		if ((argc == 2) && (argv[1][0] == '-') && (argv[1][1] == 'L'))  {
			license();
			exit(0);
		}
		fprintf(stderr, "Convert UTM coordinates to latitude/longitude coordinates.\n");
		fprintf(stderr, "Usage:  %s utm_x utm_y zone [nad27 | nad83 | wgs84]\n", argv[0]);
		fprintf(stderr, "The default is nad27\n.");
		exit(0);
	}
	utm_x = atof(argv[1]);
	utm_y = atof(argv[2]);
	zone = atoi(argv[3]);
	if (argc == 5)  {
		if (strcmp(argv[4], "nad27") == 0)  {
			dtype = 0;
		}
		else if (strcmp(argv[4], "nad83") == 0)  {
			dtype = 1;
		}
		else if (strcmp(argv[4], "wgs84") == 0)  {
			dtype = 2;
		}
		else  {
			fprintf(stderr, "Unknown datum specified.\n");
			fprintf(stderr, "Usage:  %s utm_x utm_y zone [nad27 | nad83 | wgs84]\n", argv[0]);
			fprintf(stderr, "Default is nad27.\n");
			exit(0);
		}
	}
	else  {
		dtype = 0;
	}


	if (dtype == 0)  {
		/* Fill in the datum parameters for NAD-27. */
		datum.a = NAD27_SEMIMAJOR;
		datum.b = NAD27_SEMIMINOR;
		datum.e_2 = NAD27_E_SQUARED;
		datum.f_inv = NAD27_F_INV;
		datum.k0 = UTM_K0;
		datum.a0 = NAD27_A0;
		datum.a2 = NAD27_A2;
		datum.a4 = NAD27_A4;
		datum.a6 = NAD27_A6;
	}
	else if (dtype == 1)  {
		/* Fill in the datum parameters for NAD-83. */
		datum.a = NAD83_SEMIMAJOR;
		datum.b = NAD83_SEMIMINOR;
		datum.e_2 = NAD83_E_SQUARED;
		datum.f_inv = NAD83_F_INV;
		datum.k0 = UTM_K0;
		datum.a0 = NAD83_A0;
		datum.a2 = NAD83_A2;
		datum.a4 = NAD83_A4;
		datum.a6 = NAD83_A6;
	}
	else  {
		/* Fill in the datum parameters for WGS-84. */
		datum.a = WGS84_SEMIMAJOR;
		datum.b = WGS84_SEMIMINOR;
		datum.e_2 = WGS84_E_SQUARED;
		datum.f_inv = WGS84_F_INV;
		datum.k0 = UTM_K0;
		datum.a0 = WGS84_A0;
		datum.a2 = WGS84_A2;
		datum.a4 = WGS84_A4;
		datum.a6 = WGS84_A6;
	}


	if (redfearn_inverse(&datum, utm_x, utm_y, zone, &latitude, &longitude) != 0)  {
		fprintf(stderr, "error in input parameters.\n");
		exit(0);
	}

//	fprintf(stdout, "(%.10g %.10g %d) ===> (%.10g %.10g)\n", utm_x, utm_y, zone, latitude, longitude);
	fprintf(stdout, "%.10g %.10g\n", latitude, longitude);
	exit(0);
}
