/*
 * =========================================================================
 * drawmap - A program to draw maps using data from USGS geographic data files.
 * Copyright (c) 1997,1998,1999,2000,2001,2008  Fred M. Erickson
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
 * Program to process 250K Digital Elevation Model (DEM),
 * 24K Digital Elevation Model,
 * 100K (optional-format) Digital Line Graph (DLG),
 * 24K Digital Line Graph,
 * and Geographic Names Information System (GNIS)
 * files and produce colored maps in SUN Rasterfile format.
 *
 * At the time this program was written, some DEM, DLG, and GNIS files were available
 * for free download by following appropriate links from http://mapping.usgs.gov/
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
#include "raster.h"
#include "colors.h"
#include "dem.h"
#include "dlg.h"
#include "font_5x8.h"
#include "font_6x10.h"



#define VERSION "Version 2.6"



/*
 * data from the header of a dem file
 */
struct dem_record_type_a dem_a;
struct dem_record_type_c dem_c;

int32_t x_prime;

int32_t bottom_border = BOTTOM_BORDER;
extern int32_t right_border;	// Defined and initialized in dlg.c because needed in programs that don't include drawmap.o

// int32_t histogram[256];	/* For debugging. */
// int32_t angle_hist[100000];	/* For debugging. */
// int32_t total;	/* For debugging. */


#define CONTOUR_INTVL	(100.0)

int32_t get_factor(double);
void add_text(struct image_corners *, char *, int32_t, int32_t, int32_t, char *, int32_t, int32_t, int32_t, int32_t);
void get_short_array(short **, int32_t, int32_t);
void gen_texture(int32_t, int32_t, struct color_tab *, char *);


void
usage(char *program_name)
{
	fprintf(stderr, "\nDrawmap, %s.\n\n", VERSION);
	fprintf(stderr, "Usage:  %s [-L]\n", program_name);
	fprintf(stderr, "          [-o output_file.sun] [-l latitude1,longitude1,latitude2,longitude2]\n");
	fprintf(stderr, "          [-d dem_file1 [-d dem_file2 [...]]] [-a attribute_file] [-z] [-w]\n");
	fprintf(stderr, "          [-c contour_interval] [-C contour_interval] [-g gnis_file] [-t]\n");
	fprintf(stderr, "          [-x x_size] [-y y_size] [-r relief_factor] [-m relief_mag] [-i] [-h]\n");
	fprintf(stderr, "          [-n color_table_number] [dlg_file1 [dlg_file2 [...]]]\n");
	fprintf(stderr, "\nNote that the DLG files are processed in order, and each one overlays the\n");
	fprintf(stderr, "last.  If you want (for example) roads on top of streams, put the\n");
	fprintf(stderr, "transportation data after the hydrography data.  Note also that\n");
	fprintf(stderr, "latitude/longitude values are in decimal degrees, and that east and north\n");
	fprintf(stderr, "are positive, while west and south are negative.\n");
	fprintf(stderr, "A contour interval specified with the -c or -C option must be in meters.\n");
}



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
	int32_t i, j, k = 100000000, l, m, n;	// bogus initializer to expose errors.
	int32_t tick_width = 1000;		// bogus initializer to expose errors.
	double f;
	int32_t file_index;
	int32_t xx, yy;
	double red, green, blue;
	unsigned char a;
	int32_t *lptr;
	int32_t lsize;
	int32_t smooth[SMOOTH_MAX + SMOOTH_MAX + 1][SMOOTH_MAX + SMOOTH_MAX + 1];
	int32_t smooth_size = 1000000;		// bogus initializer to expose errors.
	double gradient, gradient1, gradient2, gradient3;
	double latitude;
	double longitude;
	int32_t factor;
	int32_t sum;
	int32_t sum_count;
	struct rasterfile hdr;
	unsigned char  map[3][256];
	int dem_fdesc;
	int gnis_fdesc;
	int dlg_fdesc;
	int output_fdesc;
	ssize_t ret_val;
	int32_t length = 100000000;	// bogus initializer to expose errors.
	int32_t start_x, start_y;
	char buf[DEM_RECORD_LENGTH];
	char *ptr;
	char *tok_ptr;
	char *font;
	int32_t font_width, font_height;
	time_t time_val;
	char dem_name[135];
	int32_t dem_flag;
	int32_t contour_flag;
	int32_t capital_c_flag;
	int32_t seacoast_flag;
	int32_t info_flag;
	int32_t height_field_flag;
	int32_t color_table_number;
	int32_t smooth_data_flag;
	int32_t smooth_image_flag;
	int32_t z_flag;
	int32_t tick_flag;
	double relief_factor;
	double relief_mag;
	double latitude1, longitude1, latitude2, longitude2;
	int32_t tmp_width, tmp_height, tmp_x, tmp_y;
	char *dem_files[NUM_DEM];
	int32_t num_dem, num_dlg;
	char *gnis_file;
	char *attribute_file;
	char *output_file;
	int32_t option;
	int32_t x_low, x_high, y_low, y_high;
	double res_y, res_xy;
	short *image_tmp;
	short *image_in = (short *)0;
	int32_t gz_flag, lat_flag;
	double contour_trunc;
	double contour_intvl = CONTOUR_INTVL;
	int32_t max_elevation = -100000, min_elevation = 100000;
	int32_t min_e_lat = 100000000;				// bogus initializer to expose errors.
	int32_t min_e_long = 100000000;				// bogus initializer to expose errors.
	int32_t max_e_lat = -100000000;				// bogus initializer to expose errors.
	int32_t max_e_long = -100000000;			// bogus initializer to expose errors.
	char *gnis_feature_name;
	struct image_corners image_corners;
	struct dem_corners dem_corners;
	double res_x_data = -1000000.0;				// bogus initializer to expose errors.
	double res_y_data = -1000000.0;				// bogus initializer to expose errors.
	double res_x_image = -1000000.0;			// bogus initializer to expose errors.
	double res_y_image = -1000000.0;			// bogus initializer to expose errors.
	int32_t c_index_sea = -1000000;				// bogus initializer to expose errors.
	struct color_tab *color_tab = (struct color_tab *)0;	// bogus initializer to expose errors.
	short *sptr, *sptr2, *sptr_down, *tmp_row;
	short s0 = -32000, s1 = -32000, s2 = -32000;	// bogus initializers to expose errors.
	ssize_t (*read_function)();
	FILE *pgm_stream;
	struct datum datum =  {
		/* Fill in the datum parameters for the default program-wide datum:  NAD-27. */
		NAD27_SEMIMAJOR,
		NAD27_SEMIMINOR,
		NAD27_E_SQUARED,
		NAD27_F_INV,
		UTM_K0,
		NAD27_A0,
		NAD27_A2,
		NAD27_A4,
		NAD27_A6,
	};
	struct datum dem_datum;	// The datum of a given DEM file
	int32_t sdts_flag;		// When nonzero, we are processing an SDTS file
	int32_t gtopo30_flag;	// When nonzero, we are processing a GTOPO30 file
	int32_t byte_order;
	double utm_x, utm_y;
	int32_t utm_zone;

	if (argc == 1)  {
		usage(argv[0]);
		exit(0);
	}


	/* Process arguments */
	image_corners.x = -1;
	image_corners.y = -1;
	image_corners.sw_lat = 91.0;
	image_corners.sw_long = 181.0;
	image_corners.ne_lat = -91.0;
	image_corners.ne_long = -181.0;
	gnis_file = (char *)0;
	attribute_file = (char *)0;
	output_file = (char *)0;
	num_dem = 0;
	dem_flag = 0;		/* When set to 1, this flag says that at least some DEM data was read in. */
	contour_flag = 0;	/* When set to 1, this flag says that we should produce contours instead of shaded relief. */
	capital_c_flag = 0;	/* When set to 1, this flag indicates that the user specified '-C' instead of '-c' */
	lat_flag = 0;		/* When set to 1, this flag says that either the user explicitly specified the map boundaries, or we took them from the DEM data. */
	seacoast_flag = 0;	/* When set to 1, drawmap attempts to fill in the sea with B_BLUE */
	info_flag = 0;		/* When set to 1, drawmap prints out information about DEM and DLG files and does nothing else */
	z_flag = 0;		/* When set to 1, drawmap adjusts the elevations in the color table so as to use the entire table */
	tick_flag = 1;		/* When set to 1, tick marks and numeric latitudes/longitudes are added around the map. */
	height_field_flag = 0;	/* When set to 1, drawmap generates a height-field file instead of an image. */
	color_table_number = 2;	/* Select default color scheme. */
	opterr = 0;		/* Shut off automatic unrecognized-argument messages. */
	relief_factor = -1.0;	/* Valid values are real numbers between 0 and 1, inclusive.  Initialize to invalid value. */
	relief_mag = 1.0;	/* Valid values are real numbers between 0 and 1, inclusive.  Initialize to default value. */

	while ((option = getopt(argc, argv, "o:d:c:C:g:a:x:y:r:m:l:n:Lwihzt")) != -1)  {
		switch(option)  {
		case 'o':
			if (output_file != (char *)0)  {
				fprintf(stderr, "More than one output file specified with -o\n");
				usage(argv[0]);
				exit(0);
			}
			if (optarg == (char *)0)  {
				fprintf(stderr, "No output file specified\n");
				usage(argv[0]);
				exit(0);
			}
			output_file = optarg;
			break;
		case 'd':
			if (num_dem >= NUM_DEM)  {
				fprintf(stderr, "Out of storage for DEM file names (max %d)\n", NUM_DEM);
				exit(0);
			}
			if (optarg == (char *)0)  {
				fprintf(stderr, "No DEM file specified with -d\n");
				usage(argv[0]);
				exit(0);
			}
			dem_files[num_dem++] = optarg;
			break;
		case 'C':
			capital_c_flag = 1;
		case 'c':
			if (contour_flag != 0)  {
				fprintf(stderr, "More than one -c or -C option given\n");
				usage(argv[0]);
				exit(0);
			}
			if (optarg == (char *)0)  {
				fprintf(stderr, "No contour interval specified with -c\n");
				usage(argv[0]);
				exit(0);
			}
			contour_intvl = atof(optarg);
			contour_flag = 1;
			break;
		case 'g':
			if (gnis_file != (char *)0)  {
				fprintf(stderr, "More than one GNIS file specified\n");
				usage(argv[0]);
				exit(0);
			}
			if (optarg == (char *)0)  {
				fprintf(stderr, "No GNIS file specified with -g\n");
				usage(argv[0]);
				exit(0);
			}
			gnis_file = optarg;
			break;
		case 'a':
			if (attribute_file != (char *)0)  {
				fprintf(stderr, "More than one attribute file specified\n");
				usage(argv[0]);
				exit(0);
			}
			if (optarg == (char *)0)  {
				fprintf(stderr, "No attribute file specified with -a\n");
				usage(argv[0]);
				exit(0);
			}
			attribute_file = optarg;
			break;
		case 'x':
			if (image_corners.x >= 0)  {
				fprintf(stderr, "More than one -x value specified\n");
				usage(argv[0]);
				exit(0);
			}
			if (optarg == (char *)0)  {
				fprintf(stderr, "No value specified with -x\n");
				usage(argv[0]);
				exit(0);
			}
			image_corners.x = atoi(optarg);
			break;
		case 'y':
			if (image_corners.y >= 0)  {
				fprintf(stderr, "More than one -y value specified\n");
				usage(argv[0]);
				exit(0);
			}
			if (optarg == (char *)0)  {
				fprintf(stderr, "No value specified with -y\n");
				usage(argv[0]);
				exit(0);
			}
			image_corners.y = atoi(optarg);
			break;
		case 'r':
			if (relief_factor >= 0.0)  {
				fprintf(stderr, "More than one -r value specified\n");
				usage(argv[0]);
				exit(0);
			}
			if (optarg == (char *)0)  {
				fprintf(stderr, "No value specified with -r\n");
				usage(argv[0]);
				exit(0);
			}
			relief_factor = atof(optarg);
			if ((relief_factor < 0.0) || (relief_factor > 1.0))  {
				fprintf(stderr, "The relief factor given with -r must be a real number between 0 and 1, inclusive.\n");
				exit(0);
			}
			break;
		case 'm':
			if (relief_mag != 1.0)  {
				fprintf(stderr, "More than one -m value specified\n");
				usage(argv[0]);
				exit(0);
			}
			if (optarg == (char *)0)  {
				fprintf(stderr, "No value specified with -m\n");
				usage(argv[0]);
				exit(0);
			}
			relief_mag = atof(optarg);
			if (relief_mag < 1.0)  {
				fprintf(stderr, "The relief magnification given with -m must be a real number greater than or equal to 1.\n");
				exit(0);
			}
			break;
		case 'l':
			if ((image_corners.sw_lat != 91.0) || (image_corners.sw_long != 181.0) ||
			    (image_corners.ne_lat != -91.0) || (image_corners.ne_long != -181.0))  {
				fprintf(stderr, "More than one set of -l values specified\n");
				usage(argv[0]);
				exit(0);
			}
			if (optarg == (char *)0)  {
				fprintf(stderr, "No values specified with -l\n");
				usage(argv[0]);
				exit(0);
			}
			ptr = optarg;
			if (*ptr != '\0')  {
				image_corners.sw_lat = strtod(ptr, &ptr);
			}
			ptr++;
			if (*ptr != '\0')  {
				image_corners.sw_long = strtod(ptr, &ptr);
			}
			ptr++;
			if (*ptr != '\0')  {
				image_corners.ne_lat = strtod(ptr, &ptr);
			}
			ptr++;
			if (*ptr != '\0')  {
				image_corners.ne_long = strtod(ptr, &ptr);
			}
			if ((image_corners.sw_lat == 91.0) || (image_corners.sw_long == 181.0) ||
			    (image_corners.ne_lat == -91.0) || (image_corners.ne_long == -181.0))  {
				fprintf(stderr, "Incomplete set of -l values specified\n");
				usage(argv[0]);
				exit(0);
			}
			/* Used to check against the limits [-80,84] but GTOPO30 data can fall outside that. */
			if ((image_corners.sw_lat < -90.0) || (image_corners.sw_lat > 90.0) ||
			    (image_corners.ne_lat < -90.0) || (image_corners.ne_lat > 90.0))  {
				fprintf(stderr, "Latitude must fall between -90 and 90 degrees, inclusive\n");
				usage(argv[0]);
				exit(0);
			}
			if ((image_corners.sw_long < -180.0) || (image_corners.sw_long > 180.0) ||
			    (image_corners.ne_long < -180.0) || (image_corners.ne_long > 180.0))  {
				fprintf(stderr, "Longitude must fall between -180 and 180 degrees, inclusive\n");
				usage(argv[0]);
				exit(0);
			}
			if (image_corners.sw_lat > image_corners.ne_lat)  {
				f = image_corners.sw_lat;
				image_corners.sw_lat = image_corners.ne_lat;
				image_corners.ne_lat = f;
			}
			if (image_corners.sw_long > image_corners.ne_long)  {
				f = image_corners.sw_long;
				image_corners.sw_long = image_corners.ne_long;
				image_corners.ne_long = f;
			}
			(void)redfearn(&datum, &image_corners.sw_x_gp, &image_corners.sw_y_gp, &image_corners.sw_zone,
					image_corners.sw_lat, image_corners.sw_long, 1);
			(void)redfearn(&datum, &image_corners.ne_x_gp, &image_corners.ne_y_gp, &image_corners.ne_zone,
					image_corners.ne_lat, image_corners.ne_long, 0);
			lat_flag = 1;
			break;
		case 'n':
			if (optarg == (char *)0)  {
				fprintf(stderr, "No color table number specified with -n\n");
				usage(argv[0]);
				exit(0);
			}
			color_table_number = atoi(optarg);
			if ((color_table_number < 1) || (color_table_number > NUM_COLOR_TABS))  {
				fprintf(stderr, "Invalid color table number specified with -n, valid range is [1-%d]\n", NUM_COLOR_TABS);
				usage(argv[0]);
				exit(0);
			}
			break;
		case 'L':
			license();
			exit(0);
			break;
		case 'w':
			seacoast_flag = 1;
			break;
		case 'i':
			info_flag = 1;
			break;
		case 'h':
			height_field_flag = 1;
			break;
		case 'z':
			z_flag = 1;
			break;
		case 't':
			tick_flag = 0;
			break;
		default:
			usage(argv[0]);
			exit(0);
			break;
		}
	}
	num_dlg = argc - optind;

	/*
	 * If info_flag is non-zero, then don't bother checking the other options.
	 * They will be ignored, except for -d.
	 */
	if (info_flag == 0)  {
		/* Clean up the options. */
		if (output_file == (char *)0)  {
			if (height_field_flag != 0)  {
				output_file = "drawmap.pgm";
			}
			else  {
				output_file = "drawmap.sun";
			}
		}
		if ((image_corners.x < 0) && (num_dem != 1))  {
			/*
			 * The user didn't specify an x value.  Provide one that is half
			 * of full resolution for a 1-degree DEM.
			 *
			 * If there is only one DEM file, the x and y values will be selected later, based on its contents.
			 */
			if (lat_flag != 0)  {
				image_corners.x = drawmap_round(0.5 * (image_corners.ne_long - image_corners.sw_long) * (double)(ONE_DEGREE_DEM_SIZE - 1));
			}
			else  {
				image_corners.x = (ONE_DEGREE_DEM_SIZE - 1) >> 1;
			}
			fprintf(stderr, "x-width of actual map area set to %d pixels.  (%d elevation samples)\n",
					image_corners.x, image_corners.x + 1);
		}
		if ((image_corners.x > 0) && (image_corners.x & 1))  {
			/*
			 * Odd dimensions are potential problems.  Make them even.
			 * Absorb the odd-ness in the border.
			 */
			right_border++;
		}
		if ((image_corners.y < 0) && (num_dem != 1))  {
			/*
			 * The user didn't specify a y value.  Provide one that is half
			 * of full resolution for a 1-degree DEM.
			 *
			 * If there is only one DEM file, the x and y values will be selected later, based on its contents.
			 */
			if (lat_flag != 0)  {
				image_corners.y = drawmap_round(0.5 * (image_corners.ne_lat - image_corners.sw_lat) * (double)(ONE_DEGREE_DEM_SIZE - 1));
			}
			else  {
				image_corners.y = (ONE_DEGREE_DEM_SIZE - 1) >> 1;
			}
			fprintf(stderr, "y-height of actual map area set to %d pixels.  (%d elevation samples)\n",
					image_corners.y, image_corners.y + 1);
		}
		if ((image_corners.y > 0) && (image_corners.y & 1))  {
			/*
			 * Odd dimensions are potential problems (although not generally in the vertical direction).
			 * Absorb the odd-ness in the border.
			 */
			bottom_border++;
		}
		if (((image_corners.x > 0) && (image_corners.x < 4)) || ((image_corners.y > 0) && (image_corners.y < 4)))  {
			/*
			 * Avoid nonsensically small x or y.  The reason for this is that
			 * the code was written under the assumption that the image is at
			 * least of a certain minimal size.  By checking the size once,
			 * at the top, we don't have to check it throughout the body of the code.
			 */
			fprintf(stderr, "x and or y dimension too small.\n");
			exit(0);
		}
		if ((num_dem != 1) && (lat_flag == 0))  {
			fprintf(stderr, "The -l option is required unless there is exactly one -d option given.\n");
			usage(argv[0]);
			exit(0);
		}
		if (contour_intvl <= 0.0)  {
			fprintf(stderr, "The -c option includes a non-positive contour value (%f).\n", contour_intvl);
			usage(argv[0]);
			exit(0);
		}
		if (relief_factor < 0.0)  {
			relief_factor = 1.0;
		}
	}


	/*
	 * Set up the rasterfile color map.  See colors.h for a description of the map.
	 *
	 * Begin by setting up the initial colors in each color band.
	 */
	if (color_table_number == 1)  {
		color_tab = color_tab_neutral;
		c_index_sea = C_INDEX_SEA_NEUTRAL;
	}
	else if (color_table_number == 2)  {
		color_tab = color_tab_natural;
		c_index_sea = C_INDEX_SEA_NATURAL;
	}
	else if (color_table_number == 3)  {
		color_tab = color_tab_textbook;
		c_index_sea = C_INDEX_SEA_TEXTBOOK;
	}
	else if (color_table_number == 4)  {
		color_tab = color_tab_spiral;
		c_index_sea = C_INDEX_SEA_SPIRAL;
	}
// If you want to define your own color table, add it to colors.h,
// increase NUM_COLOR_TABS (in colors.h) to 5,
// and uncomment the following four lines.
//	else if (color_table_number == 5)  {
//		color_tab = color_tab_my_table;
//		c_index_sea = C_INDEX_SEA_MY_TABLE;
//	}

	for (i = 0; i < MAX_VALID_BANDS; i++)  {
		map[0][color_tab[i].c_index] = color_tab[i].red;
		map[1][color_tab[i].c_index] = color_tab[i].green;
		map[2][color_tab[i].c_index] = color_tab[i].blue;
	}
	/* Put black into the unused part of the table. */
	if (MAX_VALID_BANDS == 14)  {
		map[0][color_tab[MAX_VALID_BANDS].c_index] = 0;
		map[1][color_tab[MAX_VALID_BANDS].c_index] = 0;
		map[2][color_tab[MAX_VALID_BANDS].c_index] = 0;
	}
	/* Initialize the special color block to black.  We will put in the individual colors later. */
	map[0][color_tab[15].c_index] = 0;
	map[1][color_tab[15].c_index] = 0;
	map[2][color_tab[15].c_index] = 0;

	/*
	 * We have the most intense color values inserted into the table.
	 * Now insert progressively less intense versions of each color.
	 * Each color decreases in intensity all the way to black.
	 */
	for (i = 0; i < 16; i++)  {
		red = relief_factor * (double)map[0][color_tab[i].c_index] / 15.0;
		blue = relief_factor * (double)map[1][color_tab[i].c_index] / 15.0;
		green = relief_factor * (double)map[2][color_tab[i].c_index] / 15.0;

		for (j = 1; j <= 15; j++)  {
			map[0][color_tab[i].c_index + j] = map[0][color_tab[i].c_index] - (unsigned char)drawmap_round(((double)j * red));
			map[1][color_tab[i].c_index + j] = map[1][color_tab[i].c_index] - (unsigned char)drawmap_round(((double)j * blue));
			map[2][color_tab[i].c_index + j] = map[2][color_tab[i].c_index] - (unsigned char)drawmap_round(((double)j * green));
		}
		if (relief_factor == 1.0)  {
			/*
			 * Make sure that we shade all the way exactly to black when
			 * the relief factor is at its default value of 1.0.
			 */
			map[0][color_tab[i].c_index + 15] = 0;
			map[1][color_tab[i].c_index + 15] = 0;
			map[2][color_tab[i].c_index + 15] = 0;
		}
	}

	/* Insert miscellaneous colors for drawing roads, streams, and such. */
	for (i = 0; i < 16; i++)  {
		map[0][brights[i].c_index] = brights[i].red;
		map[1][brights[i].c_index] = brights[i].green;
		map[2][brights[i].c_index] = brights[i].blue;
	}


	/* If an attribute file was specified, then parse it now. */
	if ((info_flag == 0) && (attribute_file != (char *)0))  {
		process_attrib(attribute_file);
	}


	/*
	 * Before we begin processing map data, here is a short lecture on the
	 * Universal Transverse Mercator (UTM) coordinate system, which is commonly
	 * used in topographical maps, and by the military (it has been adopted by
	 * NATO and is used by the US military for ground operations).  UTM coordinates
	 * take the place of latitude and longitude, which can be cumbersome to deal
	 * with in the context of a small-area map.
	 *
	 * (UTM coordinates are used in the optional-format DLG files, and in the
	 * 24K DEM files, and there is some reference to them in the 250K DEM files.
	 * Old-style GNIS files use latitude and longitude, in DDDMMSS format,
	 * while new ones have both DDDMMSS and decimal degrees.)
	 *
	 * The UTM system begins by dividing the earth into 60 zones, each of
	 * which represents a slice (like a colored panel in a beach ball) that
	 * spans 6 degrees of longitude.  Zone 1 runs from 180 degrees West
	 * Longitude to 174 degrees West Longitude.  Zone 2 runs from 175W to
	 * 168W.  Zone 60 runs from 174E to 180E.
	 *
	 * UTM is only used from 84N to 80S.  At the poles, the Universal Polar
	 * Stereographic (UPS) projection is used.
	 *
	 * In each zone, points are represented by rectangular (x,y) coordinates
	 * that give distances, in meters, from the zone reference point.  This
	 * reference point is at the intersection of the Equator and the Central
	 * Meridian (the longitude line that runs down the center of the zone).
	 * The (x,y) coordinates give the distance in meters to the east and north
	 * of the reference point.
	 *
	 * In order to avoid having negative values for the UTM coordinates,
	 * some adjustments are made.  In the northern hemisphere, the y
	 * coordinate is simply measured from zero at the equator, but the
	 * Central Meridian is assigned a value of 500,000 meters (called a
	 * false easting), meaning that the distance (to the east) of a
	 * given point in the zone is the UTM x coordinate minus 500,000.
	 * In the southern hemisphere, the Central Meridian is again assigned
	 * a false easting of 500,000 meters; but the equator is no longer
	 * assigned a value of 0, and rather is assigned a value of 10,000,000
	 * meters north (called a false northing).
	 *
	 * Note that a Mercator projection can be visualized by imagining
	 * a cylinder, sitting on one of its ends, with a globe inside.
	 * If a light is shined from, say, the center of the globe, the longitude
	 * lines will be projected onto the cylinder as vertical lines, and the
	 * latitude lines will be projected as circles around the cylinder.
	 * The longitude lines will be evenly spaced, but the latitude lines
	 * will be farther apart as the latitude increases.  One advantage
	 * of this projection is that it is conformal, meaning that angles and
	 * shapes are preserved during the transformation, for any given small
	 * region of the map.
	 *
	 * The Transverse Mercator projection is the same deal, except that the
	 * cylinder is tipped on its side and the desired Central Meridian is
	 * aligned so that it is vertical and tangent to the cylinder wall.
	 * Because of this orientation, shapes and areas near the Central
	 * Meridian are preserved, while shapes and areas distant from it
	 * are less accurate, especially when the top and/or bottom of the map
	 * is close to one of the poles so that the zone slice must be considerably
	 * stretched to form a rectangular grid.  Within a given UTM zone, however,
	 * the distortion is relatively small.
	 *
	 * UTM is a Transverse Mercator projection, standardized for international
	 * use.
	 */


	/*
	 * This large loop processes elevation data in DEM format, SDTS DEM format,
	 * and GTOPO30 DEM format.  By the time the loop ends, the data from all
	 * files has all been consolidated into a single internal array, image_in.
	 *
	 * Ordinary DEM files have a lot of header information, much of which we
	 * throw away.  Initially, we simply read in the header and use it to figure
	 * out which type of DEM file we have, normally either a 1-degree DEM or a
	 * 7.5-minute DEM.
	 *
	 * In 1-degree DEMs, at least for the contiguous 48 United States, the
	 * elevations are stored as samples separated one from another by 3 arc
	 * seconds, making it easy to store the data in a latitude/longitude grid.
	 * In 7.5-minute DEMs, the data samples are separated by 10 meters or 30
	 * meters, and locations are in terms of UTM coordinates.  These files are
	 * considerably more difficult to translate onto a latitude/longitude grid.
	 *
	 * SDTS files contain the same types of data as DEM files, just in a
	 * radically different format, spanning multiple files.
	 *
	 * GTOPO30 files have samples spaced 30 arc-seconds apart.
	 * They have yet another special format, so we provide a separate routine
	 * to convert them into data that looks like it came from a DEM file.
	 */
	dem_name[0] = '\0';
	file_index = 0;
	smooth_image_flag = 0;
	if ((info_flag == 0) && (image_corners.x > 0) && (image_corners.y > 0))  {
		get_short_array(&image_in, image_corners.x, image_corners.y);
	}
	while (file_index < num_dem)  {
		length = strlen(dem_files[file_index]);

		/*
		 * We begin by figuring out if the file is gzip-compressed or not, and then we open it.
		 */
		if ((length > 3) && ((strcmp(&dem_files[file_index][length - 3], ".gz") == 0) ||
		    (strcmp(&dem_files[file_index][length - 3], ".GZ") == 0)))  {
			gz_flag = 1;
			if ((dem_fdesc = buf_open_z(dem_files[file_index], O_RDONLY)) < 0)  {
				fprintf(stderr, "Can't open %s for reading, errno = %d\n", dem_files[file_index], errno);
				exit(0);
			}
			read_function = buf_read_z;
		}
		else  {
			gz_flag = 0;
			if ((dem_fdesc = buf_open(dem_files[file_index], O_RDONLY)) < 0)  {
				fprintf(stderr, "Can't open %s for reading, errno = %d\n", dem_files[file_index], errno);
				exit(0);
			}
			read_function = buf_read;
		}

		if (info_flag == 0)  {
			fprintf(stderr, "Processing DEM file:  %s\n", dem_files[file_index]);
		}

		file_index++;

		sdts_flag = 0;
		gtopo30_flag = 0;
		/*
		 * Files in Spatial Data Transfer System (SDTS) format are markedly
		 * different from the old DEM files.  (As a side note, there does not
		 * appear to be a specific name for the DEM format.  Most documents
		 * just call it DEM format, and use "SDTS DEM", or some equivalent
		 * when they refer to SDTS formatted files.  I usually just call it
		 * the ordinary DEM format.
		 *
		 * Since SDTS files are so different, we detect them and then do
		 * all of the initial parsing in a separate function.
		 *
		 * We insist that the user specify one, single, SDTS file (with the
		 * -d option on the command line) for each SDTS DEM layer.
		 * The file must be the one whose name has the form ????CEL?.DDF
		 * (or ????cel?.ddf), and it may have a .gz on the end if it is gzip
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
		if (((length >= 15) && (gz_flag != 0) &&
		     ((strncmp(&dem_files[file_index - 1][length - 7], ".ddf", 4) == 0) ||
		      (strncmp(&dem_files[file_index - 1][length - 7], ".DDF", 4) == 0))) ||
		    ((length >= 12) && (gz_flag == 0) &&
		     ((strcmp(&dem_files[file_index - 1][length - 4], ".ddf") == 0) ||
		      (strcmp(&dem_files[file_index - 1][length - 4], ".DDF") == 0))))  {
			/* SDTS file */

			/* Close the file.  We will reopen it in parse_dem_sdts(). */
			if (gz_flag == 0)  {
				buf_close(dem_fdesc);
			}
			else  {
				buf_close_z(dem_fdesc);
			}

			/*
			 * Check that the file name takes the form that we expect.
			 */
			if (((gz_flag != 0) &&
			     ((strncmp(&dem_files[file_index - 1][length - 11], "ce", 2) != 0) &&
			      (strncmp(&dem_files[file_index - 1][length - 11], "CE", 2) != 0))) ||
			    ((gz_flag == 0) &&
			     (strncmp(&dem_files[file_index - 1][length - 8], "ce", 2) != 0) &&
			     (strncmp(&dem_files[file_index - 1][length - 8], "CE", 2) != 0)))  {
				fprintf(stderr, "The file %s looks like an SDTS file, but the name doesn't look right.  Ignoring file.\n", dem_files[file_index - 1]);
				continue;
			}

			/*
			 * The file name looks okay.  Let's launch into the information parsing.
			 */
			if (parse_dem_sdts(dem_files[file_index - 1], &dem_a, &dem_c, &dem_datum, gz_flag) != 0)  {
				continue;
			}

			sdts_flag = 1;
		}
		/*
		 * Files in GTOPO30 format are in their own format.  It is similar
		 * to SDTS format in that the data is spread through a number of
		 * files.  (However, any similarities end there.)  We only need to
		 * look at two files, the file whose name ends in ".HDR" and the
		 * file whose name ends in ".DEM".
		 *
		 * We insist that the user specify one, single, GTOPO30 file (with the
		 * -d option on the command line) for each GTOPO30 file collection.
		 * The file must be the one whose name has the form *.HDR
		 * (or *.hdr), and it may have a .gz on the end if it is gzip
		 * compressed.
		 *
		 * We allow the files to be gzip-compressed, and they can have either
		 * ".gz" or ".GZ" on the end.  However, we insist that the rest of
		 * the file name have consistent case.  That is, if the 'R' or 'r'
		 * in the ".HDR" or ".hdr" is in a given case, the rest of the file
		 * had better be in that same case.
		 *
		 * If the following "if" test succeeds, we assume we have an GTOPO30 file.
		 */
		else if (((length > 7) && (gz_flag != 0) &&
		     ((strncmp(&dem_files[file_index - 1][length - 7], ".hdr", 4) == 0) ||
		      (strncmp(&dem_files[file_index - 1][length - 7], ".HDR", 4) == 0))) ||
		    ((length > 4) && (gz_flag == 0) &&
		     ((strcmp(&dem_files[file_index - 1][length - 4], ".hdr") == 0) ||
		      (strcmp(&dem_files[file_index - 1][length - 4], ".HDR") == 0))))  {
			/* GTOPO30 file */

			/* Close the file.  We will reopen it in parse_gtopo30(). */
			if (gz_flag == 0)  {
				buf_close(dem_fdesc);
			}
			else  {
				buf_close_z(dem_fdesc);
			}

			gtopo30_flag = 1;
		}
		else  {
			/* Not an SDTS file or GTOPO30 file */

			/*
			 * Some people (in apparent violation of the DEM standards documents) put
			 * a newline immediately after the last valid data item in a record
			 * (rather than padding with blanks to make the record 1024 bytes long.
			 * This may simply be due to blocking the files with the:
			 *     dd if=inputfilename of=outputfilename ibs=4096 cbs=1024 conv=unblock
			 * command, and then forgetting to convert them back.
			 *
			 * We read the first record (the Type A header record) a byte at a time,
			 * searching for a newline, trying to determine if this is one of those files.
			 *
			 * We attempt to handle such files, but we don't try very hard.  There are
			 * many ways to add newlines to the files, and some pathological patterns
			 * will probably cause drawmap to give up and exit.  I didn't deem it worth
			 * a lot of effort to try to support every possible non-standard file.
			 */
			for (i = 0; i < DEM_RECORD_LENGTH; i++)  {
				if ((ret_val = read_function(dem_fdesc, &buf[i], 1)) != 1)  {
					fprintf(stderr, "read from DEM file returns %d, expected 1\n", (int)ret_val);
					exit(0);
				}
				if ((buf[i] == '\n') || (buf[i] == '\r'))  {
					if (read_function == buf_read)  {
						read_function = get_a_line;
					}
					else  {
						read_function = get_a_line_z;
					}
					break;
				}
			}
			/* Set ret_val as if we had done one big read. */
		        ret_val = i;


			/*
			 * Parse all of the data from the header that we care about.
			 * Rather than make parse_dem_a() handle variable length
			 * header records, pad the record out to 1024.
			 */
			for (i = ret_val; i < DEM_RECORD_LENGTH; i++)  {
				buf[i] = ' ';
			}
			parse_dem_a(buf, &dem_a, &dem_datum);
		}


		/*
		 * Depending on the type of data, call the appropriate
		 * routine to allocate space for the data and read it in.
		 * Note that we must later free the space pointed to by dem_corners.ptr.
		 */
		dem_corners.ptr = (short *)0;
		if (sdts_flag != 0)  {
			ret_val = process_dem_sdts(dem_files[file_index - 1], &image_corners, &dem_corners, &dem_a, &dem_datum);
		}
		else if (gtopo30_flag != 0)  {
			ret_val = process_gtopo30(dem_files[file_index - 1], &image_corners, &dem_corners, &dem_a, &dem_datum, info_flag);
		}
		else if (dem_a.plane_ref == 0)  {		// Check for Geographic Planimetric Reference System
			/*
			 * Note that this function has a side effect:  it converts the
			 * latitude/longitude code in dem_a.title into all spaces.
			 * This is done so that the code won't be included as part of
			 * the DEM name when we capture the DEM name a few lines hence.
			 * The routine has the additional side effect of setting
			 * dem_a->zone to a valid value.  The zone field in the DEM file
			 * header is zero for Geographic DEMs.
			 *
			 * Files with this Planimetric Reference System code are:  30-minute, 1-degree, and Alaska DEMs.
			 * I have no samples of 30-minute files, so I don't know of process_geo_dem will work with
			 * them.  It should work for 1-degree and Alaska DEMs.
			 */
			ret_val = process_geo_dem(dem_fdesc, read_function, &image_corners, &dem_corners, &dem_a, &dem_datum);
		}
		else if (dem_a.plane_ref == 1)  {		// Check for UTM Planimetric Reference System
			/*
			 * Files with this Planimetric Reference System code are:  7.5-minute DEMs.
			 */
			ret_val = process_utm_dem(dem_fdesc, read_function, &image_corners, &dem_corners, &dem_a, &dem_datum);

			/*
			 * We must choose whether to keep these data in UTM coordinates or
			 * inverse project them onto a latitude/longitude grid.
			 *
			 * We choose here to inverse project onto a latitude/longitude grid.
			 * This will be done below.
			 */
		}
		else  {
			fprintf(stderr, "Unsupported Planimetric Reference System (code = %d) in DEM file.  File ignored.\n", dem_a.plane_ref);
			ret_val = 1;	// Simulate error return from processing function.
		}
		if ((sdts_flag == 0) && (gtopo30_flag == 0))  {
			if (gz_flag == 0)  {
				buf_close(dem_fdesc);
			}
			else  {
				buf_close_z(dem_fdesc);
			}
		}


		/*
		 * Print all of the parsed header data.
		 */
//		print_dem_a(&dem_a);


		if (info_flag != 0)  {
			/*
			 * We only need to print out some information about the DEM file.
			 * We aren't going to produce an image.
			 */
			if (ret_val != 0)  {
				dem_corners.y = -1;		// If parsing failed, we may not know the y dimension
			}
			else  {
				free(dem_corners.ptr);
			}

			fprintf(stdout, "%s\t%40.40s\t%g:%g:%g:%g\t%d:%d\t%d:%d\t%s\n",
					dem_files[file_index - 1], dem_a.title,
					dem_corners.se_lat, dem_corners.se_long, dem_corners.nw_lat, dem_corners.nw_long,
					dem_a.min_elev, dem_a.max_elev, dem_a.cols, dem_corners.y,
					(read_function == get_a_line || read_function == get_a_line_z) ? "linefeeds=yes" : "linefeeds=no");
			continue;
		}
		if (ret_val == 0)  {
			dem_flag = 1;
		}
		else  {
			continue;
		}

		/*
		 * If the user didn't specify an image size, and there is only one DEM,
		 * initialize the image size from the DEM size.
		 */
		if (num_dem == 1)  {
			/* There was only one DEM file. */
			if (image_corners.x < 0)  {
				/*
				 * The user didn't specify an x value.  Select it to display.
				 * the single DEM file at full resolution.
				 */
				image_corners.x = dem_corners.x - 1;

				fprintf(stderr, "x-width of actual map area set to %d pixels.  (%d elevation samples)\n",
						image_corners.x, image_corners.x + 1);

				if (image_corners.x & 1)  {
					/*
					 * Odd dimensions are potential problems.  Make them even
					 * by absorbing the odd-ness in the border.
					 */
					right_border++;
				}
			}
			if (image_corners.y < 0)  {
				/*
				 * The user didn't specify an x value.  Select it to display.
				 * the single DEM file at full resolution.
				 */
				image_corners.y = dem_corners.y - 1;

				fprintf(stderr, "y-width of actual map area set to %d pixels.  (%d elevation samples)\n",
						image_corners.y, image_corners.y + 1);

				if (image_corners.y & 1)  {
					/*
					 * Odd dimensions are potential problems.  Make them even
					 * by absorbing the odd-ness in the border.
					 */
					bottom_border++;
				}
			}
		}
		/*
		 * If user did not provide the -l option, then initialize image boundary specifications.
		 * Note that, in this case, we know there is only a single DEM file, because we
		 * explicitly checked for this when we checked the input arguments.
		 * Thus it is safe to simply initialize the image corners from the dem corners.
		 */
		if (lat_flag == 0)  {
			image_corners.sw_y_gp = dem_corners.sw_y_gp;
			image_corners.sw_lat = dem_corners.sw_lat;
			image_corners.sw_x_gp = dem_corners.sw_x_gp;
			image_corners.sw_long = dem_corners.sw_long;
			image_corners.sw_zone = dem_a.zone;

			image_corners.ne_y_gp = dem_corners.ne_y_gp;
			image_corners.ne_lat = dem_corners.ne_lat;
			image_corners.ne_x_gp = dem_corners.ne_x_gp;
			image_corners.ne_long = dem_corners.ne_long;
			image_corners.ne_zone = dem_a.zone;

			lat_flag = 1;
		}


		/*
		 * We at last are sure that we have enough information to allocate space
		 * for the big DEM data array.  Allocate it now, so that it will
		 * be ready for use.
		 */
		if (image_in == (short *)0)  {
			get_short_array(&image_in, image_corners.x, image_corners.y);
		}


		/*
		 * Save the name of the DEM block for later use.
		 *
		 * This is more difficult than it might at first appear.
		 * People put all kinds of free-form text into the beginning
		 * of a DEM header record.  Only some of it can really be called a
		 * name.  (For example, there may be latitude/longitude information,
		 * or various codes describing aspects of the DEM file that people think
		 * should be remembered but don't have a legitimate place for in the
		 * standard record structure.)
		 * In an attempt to get just the name, we assume that it comes first in the record
		 * (which is not always true), and take everything up until 40 characters
		 * or until we come across three blanks in a row.
		 */
		if (dem_name[0] == '\0')  {
			i = 0;

			for (j = 0; j < 40; j++)  {
				if (dem_a.title[j] != ' ')  {
					/* If the character is not a space, just copy it. */
					dem_name[i++] = dem_a.title[j];
				}
				else  {
					/* Allow a maximum of two spaces in a row */
					if ((dem_a.title[j    ] == ' ') &&
					    (dem_a.title[j + 1] == ' ') &&
					    (dem_a.title[j + 2] == ' '))  {
						break;
					}
					else  {
						dem_name[i++] = dem_a.title[j];
					}
				}
			}

			dem_name[i] = '\0';
		}
		else  {
			strcpy(dem_name, "Data from multiple DEM files");
		}


		/*
		 * Figure out the area of the image that will be covered by this set of DEM file data.
		 * Fill in that area with data from corners.ptr.
		 *
		 * Because the relative sizes can take any ratio (in either the x or y direction)
		 * we simply choose the point from corners.ptr that lies closest to the relative
		 * location in the covered area.  The exception to this is when the image is
		 * being subsampled, in which case we smooth the data to get average representative data points.
		 * (If the data is being oversampled, we will smooth it later to get rid of the
		 * checkerboard effect that occurs when whole blocks of the image are at the same
		 * elevation.)
		 */
		latitude1 = max3(-91.0, dem_corners.sw_lat, image_corners.sw_lat);
		longitude1 = max3(-181.0, dem_corners.sw_long, image_corners.sw_long);
		latitude2 = min3(91.0, dem_corners.ne_lat, image_corners.ne_lat);
		longitude2 = min3(181.0, dem_corners.ne_long, image_corners.ne_long);
		tmp_width = drawmap_round((double)(dem_corners.x - 1) * (longitude2 - longitude1) /
				  (dem_corners.ne_long - dem_corners.sw_long));
		tmp_height = drawmap_round((double)(dem_corners.y - 1) * (latitude2 - latitude1) /
				   (dem_corners.ne_lat - dem_corners.sw_lat));
		tmp_x = drawmap_round((double)(dem_corners.x - 1) * (longitude1 - dem_corners.sw_long) /
			      (dem_corners.ne_long - dem_corners.sw_long));
		tmp_y = (dem_corners.y - 1) - drawmap_round((double)(dem_corners.y - 1) * (latitude2 - dem_corners.sw_lat) /
						    (dem_corners.ne_lat - dem_corners.sw_lat));

		x_low = drawmap_round((double)image_corners.x * (longitude1 - image_corners.sw_long) /
			      (image_corners.ne_long - image_corners.sw_long));
		x_high = drawmap_round((double)(image_corners.x + 1) * (longitude2 - image_corners.sw_long) /
			       (image_corners.ne_long - image_corners.sw_long));
		y_low = image_corners.y - drawmap_round((double)image_corners.y * (latitude2 - image_corners.sw_lat) /
						(image_corners.ne_lat - image_corners.sw_lat));
		y_high = image_corners.y + 1 - drawmap_round((double)image_corners.y * (latitude1 - image_corners.sw_lat) /
						     (image_corners.ne_lat - image_corners.sw_lat));

		if ((x_low < 0) || (x_high > (image_corners.x + 1)) || (y_low < 0) || (y_high > (image_corners.y + 1)))  {
			fprintf(stderr, "One of x_low=%d, x_high=%d, y_low=%d, y_high=%d out of range\n",
				x_low, x_high, y_low, y_high);
			exit(0);
		}

// For debugging.
//		fprintf(stderr, "image_corners.x=%d  image_corners.y=%d  dem_corners.x=%d  dem_corners.y=%d\n     x_low=%d  x_high=%d  y_low=%d  y_high=%d\n",
//			image_corners.x, image_corners.y, dem_corners.x, dem_corners.y, x_low, x_high, y_low, y_high);
//		fprintf(stderr, "dem_corners: (%g %g) (%g %g) (%d %d)\n     image_corners: (%g %g) (%g %g) (%d %d)\n     tmp_width=%d   tmp_height=%d   tmp_x=%d   tmp_y=%d\n",
//			dem_corners.sw_x_gp, dem_corners.sw_y_gp, dem_corners.ne_x_gp, dem_corners.ne_y_gp, dem_corners.x, dem_corners.y,
//			image_corners.sw_x_gp, image_corners.sw_y_gp, image_corners.ne_x_gp, image_corners.ne_y_gp, image_corners.x, image_corners.y,
//			tmp_width, tmp_height, tmp_x, tmp_y);


		/*
		 * Calculate some ratios that we use to determine whether or not
		 * smoothing is required.
		 *
		 * If we have DEM data of greater resolution than the target image,
		 * then we smooth the DEM data (average data points over small areas)
		 * so that each target image pixel represents an average of the available
		 * DEM data points for locations near that pixel.  This throws away
		 * some of the "crispness" of the data, so we don't want do it willy-nilly.
		 * (However, if the resolutions are very much different, then the
		 * terrain can look quite peculiar without smoothing, because elevation
		 * samples from widely-separated areas can be thrown next to each other
		 * on the image.)
		 *
		 * If we have DEM data of lesser resolution than the target image,
		 * then we smooth the target image to reduce the stairstep effect
		 * that comes from spreading too little data over too large an area.
		 * In this case, the data is a little too "crisp", in the sense that
		 * we don't have enough of it, so we need to spread the available
		 * data out to fill the desired image.
		 *
		 * If the data and image resolution are nearly the same, we don't do
		 * any smoothing.  Thus we check to make sure that the two resolutions
		 * differ by at least a certain amount.  For data smoothing, the amount
		 * is 50%, because we don't want to smear up the data unless we
		 * have a good reason.  For image smoothing, we are a lot less
		 * tolerant, because even a relatively small resolution difference can
		 * create image stairstepping.
		 *
		 * The decision of whether or not to smooth is somewhat subjective,
		 * so our choice may not always make everyone happy.  However, the user
		 * can always display the data at full resolution if the smoothing results
		 * don't meet expectations.
		 *
		 * We check the x and y resolutions separately, and do the smoothing
		 * if either direction meets the criterion.
		 *
		 * There is still an image glitch that isn't dealt with here.  When
		 * the resolutions of the target image and the DEM data are close,
		 * but not identical (roughly within 30% of each other), then there
		 * may be a tiny checkerboard pattern on the areas of the image that
		 * represent low-gradient terrain.  This appears to be caused by the
		 * process by which indexes into the DEM data are derived from indexes
		 * into the target image.  Since the indexes are approximately congruent,
		 * (but not quite) a set of image indexes (in, say, the x direction) like:
		 * 0 1 2 3 4 5 6 7 ...
		 * can translate into a set of DEM indexes like:
		 * 0 1 3 4 6 7 9 10 ...
		 * This means that the target image contains pairs of adjacent elevations
		 * that come from adjacent locations in the DEM data.  Adjacent to each
		 * of these pairs (on the target image) are pairs that came from not-quite-
		 * adjacent data in the DEM data.  This creates small-scale stairstepping
		 * in the target image, where each pair of elevations is bounded by pairs
		 * that have small elevation discontinuities.  The result are anomalous
		 * bands of light or shadow at the discontinuities.  The problem only
		 * shows up in areas where the elevation is changing slowly (that is, the gradient
		 * has a small magnitude) because only in those regions does a small elevation
		 * change result in a relatively large color change.
		 * I tried various simple things to eliminate this problem, including
		 * various filters, and even some simple jittering of the data.  None
		 * of these techniques improved the image enough to be worthwhile
		 * (at least in my subjective opinion).  Until I figure out a good way
		 * to approach this problem, the manual page simply says not to select
		 * nearly-the-same-but-not-the-same source and target resolutions.
		 * It seems unlikely that people would want to do this very often anyway.
		 */
		smooth_data_flag = 0;
	    	res_x_data = (double)(dem_corners.x - 1) / (dem_corners.ne_long - dem_corners.sw_long);
		res_x_image = (double)image_corners.x / (image_corners.ne_long - image_corners.sw_long);
		res_y_data = (double)(dem_corners.y - 1) / (dem_corners.ne_lat - dem_corners.sw_lat);
		res_y_image = (double)image_corners.y / (image_corners.ne_lat - image_corners.sw_lat);
		if (((1.5 * res_y_image) < res_y_data) || ((1.5 * res_x_image) < res_x_data))  {
			smooth_data_flag = 1;
		}
		if (((1.05 * res_y_data) < res_y_image) || ((1.05 * res_x_data) < res_x_image))  {
			smooth_image_flag = 1;
		}

		/*
		 * Prepare a smoothing kernel in case we have more data than pixels to display it.
		 * The kernel is a square, a maximum of 2*SMOOTH_MAX+1 on a side.
		 *
		 * Here is one possible kernel, that I have tried:
		 *    If a kernel element is a distance of sqrt(k*k + l*l) from the
		 *    center, then its weight is 10*1.5^(-x/2)
		 *    Implemented by:
		 *       smooth[k + smooth_size][l + smooth_size] = drawmap_round(10.0 * pow(1.5, - sqrt(k * k + l * l) / 2.0));
		 *
		 * For now, we just take the straight average over the kernel, since it seems to work reasonbly
		 * well.
		 *
		 * The kernel width/height will be 1+2*smooth_size pixels.
		 * In the calculation of smooth_size, we take the minimum of SMOOTH_MAX,
		 * pixels_per_degree_resolution_of_source_data_in_y_direction / pixels_per_degree_resolution_of_target_image_in_y_direction - 1, and
		 * pixels_per_degree_resolution_of_source_data_in_x_direction / pixels_per_degree_resolution_of_target_image_in_x_direction - 1
		 *
		 * The more excess data we have, the more source pixels we average to get a single
		 * data point for the target image.
		 */
		if (smooth_data_flag != 0)  {
			smooth_size = drawmap_round(min3(SMOOTH_MAX,
						 -1.0 + res_y_data / res_y_image,
						 -1.0 + res_x_data / res_x_image));
			if (smooth_size < 1)  {
				/*
				 * If the y resolution and x resolution differ,
				 * it is possible for one to call for smoothing and the other not.
				 * This would result in smooth_size = 0, which we don't want.
				 * We correct that problem here.
				 */
				smooth_size = 1;
			}
			for (k = -smooth_size; k <= smooth_size; k++)  {
				for (l = -smooth_size; l <= smooth_size; l++)  {
					smooth[k + smooth_size][l + smooth_size] = 1;
				}
			}
		}


		/*
		 * This is the loop that transfers the data for a single DEM into the image_in array.
		 * The image_in array will eventually hold the data from all DEM files given by the user.
		 *
		 * Note:  The mapping of DEM data into the image is done by simple linear interpolation
		 * from the edges of the DEM data.  This is quite straightforward for DEM data that uses
		 * geographical planimetric coordinates (latitudes and longitudes).  However for 7.5-minute
		 * DEM data, which use UTM coordinates, we have to map from UTM into latitude/longitude
		 * coordinates.  This mapping works as follows:
		 *
		 *	Use the (i, j) location in the image to determine an accurate latitude/longitude.
		 *      Map the latitude/longitude into UTM coordinates with the redfearn() function.
		 *      Use these UTM coordinates, along with the known UTM range of the DEM data,
		 *          to accurately determine the correct (k, l) point in the DEM data that
		 *          corresponds most closely to the specified latitude/longitude within the map image.
		 *      Use that correct point to produce an elevation value to stuff into the
		 *          (i, j) location in the image.
		 *
		 * Technically speaking, this is about as accurate a job as can be done without implementing
		 * some between-point interpolation.  I have so far resisted using inter-point interpolation in
		 * drawmap, mostly because it changes the data in ways that are non-obvious to the user.  (Call
		 * it a personal preference.)  However, 7.5-minute DEMs might benefit from it because they get
		 * warped and twisted during the conversion to latitude/longitude coordinates.  This sometimes
		 * results in some diagonal linear artifacts in the map.  Interpolation might (in theory)
		 * eliminate these.  A potential future feature for drawmap is to provide such interpolation,
		 * perhaps as a command line option.  Another potential feature is to provide an option to
		 * plot maps on a UTM grid instead of a latitude/longitude grid.  This would work better
		 * for 7.5-minute UTM data.
		 */
		if ((tmp_width != 0) && (tmp_height != 0))  {
			for (i = y_low; i < y_high; i++)  {
				if (dem_a.plane_ref != 1)  {
					/* Geographic Planimetric coordinates. */
					k = tmp_y + drawmap_round((double)(tmp_height * (i - y_low)) / (double)(y_high - 1 - y_low));
				}

				for (j = x_low; j < x_high; j++)  {
					if (dem_a.plane_ref != 1)  {
						/* Geographic planimetric coordinates. */
						l = tmp_x + drawmap_round((double)(tmp_width * (j - x_low)) / (double)(x_high - 1 - x_low));
						if ((l < 0) || (l > (dem_corners.x - 1)) || (k < 0) || (k > (dem_corners.y - 1)))  {
							fprintf(stderr, "One of l=%d, k=%d out of range, (i=%d, j=%d, tmp_y=%d, tmp_x=%d, tmp_height=%d, tmp_width=%d)\n",
								l, k, i, j, tmp_y, tmp_x, tmp_height, tmp_width);
							exit(0);
						}
					}
					else  {
						/*
						 * UTM Planimetric coordinates.
						 *
						 * Find UTM equivalents of the latitude/longitude represented by (i, j)
						 * and round those UTM equivalents to the nearest round 10 or 30
						 * meter increment.  (Whether the increment is 10 or 30 is determined
						 * by the value in dem_a.x_res or dem_a.y_res.)
						 *
						 * Afterward, use these values to interpolate index values for
						 * the DEM data array.
						 */
						(void)redfearn(&dem_datum, &utm_x, &utm_y, &utm_zone,
							latitude2  - (double)(i - y_low) * (latitude2  - latitude1)  / (double)(y_high - y_low - 1),
							longitude1 + (double)(j - x_low) * (longitude2 - longitude1) / (double)(x_high - x_low - 1), 0);
						utm_x = rint(utm_x / dem_a.x_res) * dem_a.x_res;
						utm_y = rint(utm_y / dem_a.y_res) * dem_a.y_res;

						k = dem_corners.y - 1 - drawmap_round((((double)dem_corners.y - 1.0) * (utm_y - dem_corners.y_gp_min)) / (dem_corners.y_gp_max - dem_corners.y_gp_min));
						l = drawmap_round((((double)dem_corners.x - 1.0) * (utm_x - dem_corners.x_gp_min)) / (dem_corners.x_gp_max - dem_corners.x_gp_min));

						if ((l < 0) || (l > (dem_corners.x - 1)) || (k < 0) || (k > (dem_corners.y - 1)))  {
							/*
							 * The data in a 7.5-minute DEM is localized at round-numbered
							 * UTM values.  Thus, it rarely falls exactly on the boundaries
							 * of the latitude/longitude bounding box for a DEM.  Thus,
							 * as we index back and forth across the latitude/longitude
							 * bounding box, it is not at all uncommon to get index values
							 * that slop slightly over the edges of the DEM data array.
							 * Because of this, we don't print a warning message for those
							 * slop-overs.  We simply ignore them.
							 */
							//fprintf(stderr, "One of l=%d, k=%d out of range, (i=%d, j=%d, tmp_y=%d, tmp_x=%d, tmp_height=%d, tmp_width=%d)\n",
							//	l, k, i, j, tmp_y, tmp_x, tmp_height, tmp_width);
							continue;
						}
					}


					if (*(dem_corners.ptr + k * dem_corners.x + l) == HIGHEST_ELEVATION)  {
						/*
						 * It is possible, for 7.5-minute DEMs, to have some samples
						 * at HIGHEST_ELEVATION around the non-rectangular boundaries
						 * of the DEM data.  Don't attempt copy these into the image array.
						 */
						continue;
					}

					if (smooth_data_flag != 0)  {
						/*
						 * We have DEM data whose resolution, in pixels per degree,
						 * is greater than the resolution of the target image.  Since
						 * we have excess data, do some smoothing of the data so that
						 * the elevation of a point in the target image is an average
						 * over a group of points in the source DEM data.
						 */
						sum = 0;
						sum_count = 0;
						for (m = -smooth_size; m <= smooth_size; m++)  {
							for (n = -smooth_size; n <= smooth_size; n++)  {
								if (((k + m) < 0) || ((k + m) >= dem_corners.y) || ((l + n) < 0) || ((l + n) >= dem_corners.x))  {
									continue;
								}

								if (*(sptr = dem_corners.ptr + (k + m) * dem_corners.x + l + n) == HIGHEST_ELEVATION)  {
									continue;
								}
								sum += *sptr * smooth[m + smooth_size][n + smooth_size];
								sum_count += smooth[m + smooth_size][n + smooth_size];

								/*
								 * Here, we are trying to find the latitude and longitude of the
								 * high and low elevation points in the map.
								 * When there is heavy smoothing, the derived location may
								 * be pretty approximate.
								 * Note also that there may be more than one point in the
								 * map that takes on the highest (or lowest) elevation.
								 * We only select the first one we find.
								 *
								 * It is somewhat inefficient to do these checks here,
								 * since data points will generally get checked multiple
								 * times; but doing it here lets us easily associate
								 * a given DEM data point with values of i and j,
								 * which give us the latitude/longitude of the point.
								 */
								if (*sptr < min_elevation)  {
									min_elevation = *sptr;
									min_e_lat = i;
									min_e_long = j;
								}
								if (*sptr > max_elevation)  {
									max_elevation = *sptr;
									max_e_lat = i;
									max_e_long = j;
								}
							}
						}
						*(image_in + i * (image_corners.x + 1) + j) = drawmap_round((double)sum / (double)sum_count);
					}
					else  {
						/*
						 * We have an image that is either one-to-one with the DEM data, or that needs
						 * more pixels per degree of longitude than the DEM data can supply.
						 *
						 * Don't do any smoothing.  Simply pick the nearest
						 * point from dem_corners.ptr.
						 *
						 * If the x and y image size, given by the user, is
						 * not related by an integer factor to the number of elevation samples
						 * in the available data, then the image will contain some
						 * stripe anomalies because the rounding (above) to arrive
						 * at the k and l values will periodically give two k or
						 * l values in a row that have the same value.  Since
						 * the image color at a given point depends on changes in
						 * elevation around that point, having repeated elevation
						 * values can result in anomalous flat areas (with a neutral
						 * color) in an area of generally steep terrain (with generally
						 * bright or dark colors).  We can do some smoothing later
						 * in an attempt to lessen this problem.
						 */
						if (*(sptr = dem_corners.ptr + k * dem_corners.x + l) == HIGHEST_ELEVATION)  {
							continue;
						}
						*(image_in + i * (image_corners.x + 1) + j) = *sptr;

						/*
						 * Here, we are trying to find the latitude and longitude of the
						 * high and low elevation points in the map.
						 * Note that there may be more than one point in the
						 * map that takes on the highest (or lowest) elevation.
						 * We only select the first one we find.
						 */
						if (*sptr < min_elevation)  {
							min_elevation = *sptr;
							min_e_lat = i;
							min_e_long = j;
						}
						if (*sptr > max_elevation)  {
							max_elevation = *sptr;
							max_e_lat = i;
							max_e_long = j;
						}
					}
				}
			}
		}
		free(dem_corners.ptr);
	}
	/*
	 * If we have reached this point and we still don't know the image dimensions,
	 * then just give up and exit.  We could put in a big slug of code here
	 * and come up with some image dimensions, but we have reached the point of
	 * diminishing returns.
	 *
	 * If we reach this point without image dimensions, it probably means that
	 * the user has provided a single DEM file, but that it falls outside of
	 * the specified latitude/longitude range.  We could limp along under these
	 * conditions, and process the DLG or GNIS information (if any), but it
	 * doesn't seem worthwhile.  It is usually best to localize decisions, to the
	 * extent possible.  We have violated that rule here, with the laudable goal
	 * of trying to not force the user to specify image dimensions and latitude/longitude
	 * ranges.  However, the image-size decision has been smeared over enough of the
	 * code to make it hard to understand and maintain.  (There are even little bits
	 * of it slopped over into dem.c.)  It is time to call a halt.  (Perhaps past time.)
	 */
	if ((info_flag == 0) && ((image_corners.x < 0) || (image_corners.y < 0)))  {
		fprintf(stderr, "Image dimensions are ambiguous.  There may be a problem with -l, -x, and/or -y.\n");
		fprintf(stderr, "If you provide a single DEM file, you can leave out -l, -x, and -y,\n");
		fprintf(stderr, "and drawmap will choose them for you.\n");
		exit(0);
	}


	/*
	 * When dealing with 7.5-minute DEMs, there are sometimes gaps between the data
	 * for a pair of adjacent DEMs.  This is sometimes because it is difficult to
	 * choose image dimensions so that there is an exact correspondence between data
	 * points and image points --- under these conditions, rounding quantization can
	 * cause a small gap to occur between quads.
	 * Occasionally, there are also actual gaps between the data in adjacent files.
	 * Either of these difficulties can result white gaps in the image,
	 * between the data for adjacent quads.
	 *
	 * We fill in these voids by averaging neighboring points that contain valid data.
	 * We look for spots on the image where a non-valid point has valid points, on either
	 * side, in diametric opposition.
	 *
	 * We stretch out further to the left and right because the quads are generally
	 * fairly even on top and bottom, but ragged on the left and right.  Thus, any
	 * gaps usually show up at the vertical joints between quads, and the gaps can
	 * be two pixels wide, when the joints are particularly ragged.
	 *
	 * The purpose of this block of code is to get rid of gaps that occur when
	 * the target image resolution is about the same as the data resolution.
	 * (We will call this the resolution-parity case.)  When the target image
	 * resolution is considerably smaller than the data resolution, then the
	 * data smoothing (performed above) should eliminate any gaps.  When
	 * the target image resolution is considerably greater than the data
	 * resolution, then we have to rely on the image smoothing (performed later)
	 * to fill the gaps, because the gaps can be magnified in width by oversampling.
	 * The current block of code falls between the two extremes.  Unlike either of
	 * the smoothing operations, this code does its job without modifying
	 * any existing elevation data.  It steps lightly, and only modifies
	 * the empty regions between blocks of valid data.  (Of course, this
	 * current block of code may also fill in some gaps that would otherwise
	 * have been filled in by the image smoothing below.)
	 *
	 * We could combine this operation with the image smoothing operation, below,
	 * but the latter operation is currently written to require a complete extra
	 * copy of the image.  By doing a separate interpolation operation here, we
	 * avoid having to double our memory needs for images that have approximate
	 * resolution parity with the data.  This is important, because such images
	 * are often quite large, sometimes several times as large as can be displayed on
	 * a single screen.  Since the image smoothing operation is used when
	 * a small amount of data is blown up into a larger size, the images there
	 * are likely to be more than moderate in size, perhaps comparable to the
	 * size of a display screen.  Thus, in the image-smoothing case, the doubled
	 * memory requirements are an acceptable trade off for simpler code; while,
	 * in the resolution-parity case, it is worthwhile to try to minimize memory
	 * use and thus maximize the size of the allowable maps.
	 *
	 *
	 * We don't want to allocate space for another image, so we allocate space for
	 * another image row.  We use this temporary space, and the variables s0, s1, and s2,
	 * to save the data we have already looked at, so that we can change the data
	 * in the image array itself, but still have a copy of the old data to do our
	 * searching and averaging with.  This adds a small amount of complexity to
	 * this block of code, but can greatly decrease our memory needs.
	 *
	 * sptr is the pointer to the row we are currently examining and changing.
	 * sptr_down is a pointer to the next row down the image (the row we will examine next).
	 * tmp_row holds a pre-change version of the previously examined row.
	 * s2 holds the pre-change version of the point we are currently looking at.
	 * s1 holds the pre-change version of the previous point.
	 * s0 holds the pre-change version of the point before s1.
	 */
	if (info_flag == 0)  {
		tmp_row = (short *)malloc(sizeof(short) * (image_corners.x + 1));
		if (tmp_row == (short *)0)  {
			fprintf(stderr, "malloc of tmp_row failed\n");
			exit(0);
		}
		sptr = image_in - image_corners.x - 1;
		sptr_down = image_in;
		for (i = 0; i <= image_corners.y; i++)  {
			sptr += (image_corners.x + 1);
			sptr_down += (image_corners.x + 1);
			for (j = 0; j <= image_corners.x; j++)  {
				s2 = sptr[j];
				if (s2 == HIGHEST_ELEVATION)  {
					f = 0.0;
					k = 0;
					if ((j > 0) && (j < image_corners.x))  {
						if ((sptr[j - 1] != HIGHEST_ELEVATION) && (sptr[j + 1] != HIGHEST_ELEVATION))  {
							f += sptr[j - 1];
							f += sptr[j + 1];
							k = k + 2;
						}
						if ((i > 0) && (i < image_corners.y))  {
							if ((tmp_row[j - 1] != HIGHEST_ELEVATION) && (sptr_down[j + 1] != HIGHEST_ELEVATION))  {
								f += tmp_row[j - 1];
								f += sptr_down[j + 1];
								k = k + 2;
							}
							if ((tmp_row[j + 1] != HIGHEST_ELEVATION) && (sptr_down[j - 1] != HIGHEST_ELEVATION))  {
								f += tmp_row[j + 1];
								f += sptr_down[j - 1];
								k = k + 2;
							}
							if ((j > 1) && (j < (image_corners.x - 1)))  {
								if ((tmp_row[j - 2] != HIGHEST_ELEVATION) && (sptr_down[j + 2] != HIGHEST_ELEVATION))  {
									f += tmp_row[j - 2];
									f += sptr_down[j + 2];
									k = k + 2;
								}
								if ((tmp_row[j + 2] != HIGHEST_ELEVATION) && (sptr_down[j - 2] != HIGHEST_ELEVATION))  {
									f += tmp_row[j + 2];
									f += sptr_down[j - 2];
									k = k + 2;
								}
							}
						}
						if ((j > 1) && (j < (image_corners.x - 1)))  {
							if ((sptr[j - 2] != HIGHEST_ELEVATION) && (sptr[j + 2] != HIGHEST_ELEVATION))  {
								f += sptr[j - 2];
								f += sptr[j + 2];
								k = k + 2;
							}
						}
					}
					if ((i > 0) && (i < image_corners.y) &&
					    (tmp_row[j] != HIGHEST_ELEVATION) && (sptr_down[j] != HIGHEST_ELEVATION))  {
						f += tmp_row[j];
						f += sptr_down[j];
						k = k + 2;
					}
					if (k > 1)  {
						sptr[j] = f / (double)k;
					}
				}

				if (j > 1)  {
					tmp_row[j - 2] = s0;
				}
				s0 = s1;
				s1 = s2;
			}
			tmp_row[j - 2] = s0;
			tmp_row[j - 1] = s1;
		}
		free(tmp_row);
	}



	/*
	 * If the image data has been oversampled (meaning that we have spread too little actual
	 * DEM data over too many image pixels), then we smooth it out a little so that there isn't
	 * a checkerboard effect from the sparse data.  The size of the smoothing kernel, and
	 * its shape, are heuristically chosen to produce pleasing results.  However, the whole
	 * process is inherently imperfect, so don't expect amazing results.  After all, there
	 * really isn't any way to accurately interpolate the data that isn't there.  We are just
	 * trying to get rid of some of the annoying artifacts of the oversampling process.  This
	 * makes the image look better, but it does so essentially by removing some false data,
	 * and adding new more-pleasant-looking false data to replace it.
	 */
	if (info_flag == 0)  {
		if ((dem_flag != 0) && (smooth_image_flag != 0))  {
			/*
			 * Prepare a smoothing kernel.
			 * The kernel is a square, a maximum of 2*SMOOTH_MAX+1 on a side.
			 *
			 * If a kernel element is a distance of sqrt(k*k + l*l) from the
			 * center, then its weight is:
			 * 	smooth[k + smooth_size][l + smooth_size] = drawmap_round(10.0 * exp(- (k * k + l * l) / (2.0 * (smooth_size / 2.0) * (smooth_size / 2.0))));
			 *
			 * This is basically a gaussian distribution, with a mean of zero and a variance of
			 * (smooth_size / 2.0)^2
			 *
			 * The parameters of the equation were chosen by trial and error.
			 *
			 * We choose smooth_size in the same way that we chose it above,
			 * except that the two ratios are inverted.
			 */
			smooth_size = drawmap_round(min3(SMOOTH_MAX, res_y_image / res_y_data, res_x_image / res_x_data));
			if (smooth_size < 1)  {
				/*
				 * If the y resolution and x resolution differ,
				 * it is possible for one to call for smoothing and the other not.
				 * This could result in smooth_size = 0, which we don't want.
				 * We correct that problem here.
				 */
				smooth_size = 1;
			}
			for (k = -smooth_size; k <= smooth_size; k++)  {
				for (l = -smooth_size; l <= smooth_size; l++)  {
					smooth[k + smooth_size][l + smooth_size] = drawmap_round(10.0 * exp(- (k * k + l * l) / (2.0 * (smooth_size / 2.0) * (smooth_size / 2.0))));
				}
			}

			/*
			 * We need a new block of memory so that we can read the source data
			 * from one block and write smoothed data into the other.
			 */
	//		get_short_array(&image_tmp, image_corners.x, image_corners.y);
			image_tmp = (short *)malloc(sizeof(short) * (image_corners.y + 1) * (image_corners.x + 1));
			if (image_tmp == (short *)0)  {
				fprintf(stderr, "malloc of image_tmp failed\n");
				exit(0);
			}

			/*
			 * Do the smoothing.
			 *
			 * Slop over slightly into the areas that are set to HIGHEST_ELEVATION
			 * so that we can fill in any remaining gaps between 7.5-minute quads.
			 */
			for (i = 0; i <= image_corners.y; i++)  {
				for (j = 0; j <= image_corners.x; j++)  {
					sum = 0;
					sum_count = 0;
					for (m = -smooth_size; m <= smooth_size; m++)  {
						for (n = -smooth_size; n <= smooth_size; n++)  {
							if (((i + m) < 0) || ((i + m) > image_corners.y) || ((j + n) < 0) || ((j + n) > image_corners.x))  {
								continue;
							}

							sptr = (image_in + (i + m) * (image_corners.x + 1) + j + n);
							if (*sptr == HIGHEST_ELEVATION)  {
								continue;
							}
							sum += *sptr * smooth[m + smooth_size][n + smooth_size];
							sum_count += smooth[m + smooth_size][n + smooth_size];
						}
					}
					if (sum_count == 0)  {
						*(image_tmp + i * (image_corners.x + 1) + j) = HIGHEST_ELEVATION;
					}
					else  {
						*(image_tmp + i * (image_corners.x + 1) + j) = drawmap_round((double)sum / (double)sum_count);
					}
				}
			}

			free(image_in);
			image_in = image_tmp;
		}
	}



	/*
	 * If height_field_flag is non-zero, then we don't generate an image.
	 * Instead we create a file full of height field information for use
	 * by other programs, such as the povray ray tracer.
	 *
	 * The file is a Portable Graymap (PGM) format file
	 * which is a simple ASCII dump of the elevation data.
	 */
	if ((info_flag == 0) && (height_field_flag != 0))  {
		if ((pgm_stream = fopen(output_file, "w+")) < 0)  { 
			fprintf(stderr, "Can't create %s for writing, errno = %d\n", output_file, errno); 
			exit(0);
		}

		/*
		 * We need to recalculate the maximium and minimum elevations
		 * since they may have been altered by the smoothing
		 * operations, and we need to print out the new values.
		 */
		min_elevation = 100000;
		max_elevation = -100000;
		l = 0;
		k = 0;
		for (i = 0; i <= image_corners.y; i++)  { 
			for (j = 0; j <= image_corners.x; j++)  { 
				if (*(image_in + i * (image_corners.x + 1) + j) == HIGHEST_ELEVATION)  {
					*(image_in + i * (image_corners.x + 1) + j) = 0;
					k = 1;
					continue;
				}
				if (*(image_in + i * (image_corners.x + 1) + j) < 0)  {
					*(image_in + i * (image_corners.x + 1) + j) = 0;
					l = 1;
				}
				if (*(image_in + i * (image_corners.x + 1) + j) > max_elevation)  {
	                		max_elevation = *(image_in + i * (image_corners.x + 1) + j);
	        		}
	        		if (*(image_in + i * (image_corners.x + 1) + j) < min_elevation)  {
	                		min_elevation = *(image_in + i * (image_corners.x + 1) + j);
	        		}
			}
		}
		fprintf(stderr, "minimum elevation = %d, maximum elevation = %d%s%s\n", min_elevation, max_elevation,
				k != 0 ? ",\nSome points that didn't contain valid data had their elevations set to zero." : "",
				l != 0 ? ",\nSome points with elevations below zero had their elevations set to zero." : "");

		fprintf(pgm_stream, "P2\n");
// This print statement is for use when you want elevations normalized to 65535.
		fprintf(pgm_stream, "%d %d\n%d\n", image_corners.x + 1, image_corners.y + 1, 65535);
// This print statement is for use when you don't want elevations normalized to 65535.
//		fprintf(pgm_stream, "%d %d\n%d\n", image_corners.x + 1, image_corners.y + 1, max_elevation);

		for (i = 0; i <= image_corners.y; i++)  { 
			for (j = 0; j <= image_corners.x; j++)  { 
				/*
				 * This print statement is for use when you want elevations normalized to 65535.
				 * It would be nice to normalize the range from min_elevation to max_elevation
				 * onto the range from 0 to 65535.  However, occasionally, some elevations
				 * are non-valid and get arbitrarily set to zero.  Thus, it is easier to just
				 * normalize the range from 0 to max_elevation onto the range from 0 to 65535.
				 */
				fprintf(pgm_stream, "%d\n", drawmap_round(65535.0 * (double)(((int32_t)*(image_in + i *
					(image_corners.x + 1) + j) & 0xffff) /* - 0 min_elevation */) /
					(double)(max_elevation == 0 /* min_elevation */ ? 1.0 : max_elevation /* - min_elevation */)));
				/*
				 * This print statement is for use when you don't want elevations normalized to 65535.
				 */
//				fprintf(pgm_stream, "%d\n", *(image_in + i * (image_corners.x + 1) + j));
			}
		}

		fprintf(pgm_stream, "# Height-field map of Digital Elevation Model data, in Plain PGM Format.\n");
		fprintf(pgm_stream, "# This output was produced by the drawmap program, %s.\n", VERSION);
		fprintf(pgm_stream, "# %g %g %g %g Latitude/longitude of southeast and northwest corners\n",
					image_corners.sw_lat, image_corners.ne_long,
					image_corners.ne_lat, image_corners.sw_long);
		fprintf(pgm_stream, "# %d %d Mimimum and maximum elevations%s%s\n",
					min_elevation, max_elevation,
					k != 0 ? "\n# Some points that didn't contain valid data had their elevations set to zero." : "",
					l != 0 ? "\n# Some points with elevations below zero had their elevations set to zero." : "");

		fclose(pgm_stream);

		/*
		 * Produce a povray texture map, suitable for use
		 * with the height-field data.
		 */
		gen_texture(min_elevation, max_elevation, color_tab, output_file);

		exit(0);
	}
	if ((info_flag == 0) && (dem_flag != 0))  {
		fprintf(stderr, "minimum elevation = %d, maximum elevation = %d\n", min_elevation, max_elevation);
	}



	/*
	 * Get memory for the actual output image.  We need space for the map itself,
	 * and for the white borders that go around it.  Note that the code indexes
	 * through the map portion of the image area as though there were no borders
	 * around the map.  Then, when the indexes are actually used to index into
	 * the image_corners.ptr array, the code adds the border widths to the index values
	 * to arrive at true indices.  This makes the code look messy, but it allows
	 * us to separate the task of navigating around the map area from the task
	 * of navigating around the output image area.  This makes it easier (for me)
	 * to understand what is going on.  On the image, the x index increases toward
	 * the right and the y index increases going toward the bottom.  latitude
	 * increases going from bottom to top, and longitude increases going from
	 * left to right.  (Remember, though, that west longitudes are negative,
	 * so that 109W is actually smaller than 108W, when treated as -109 and -108.)
	 *
	 * The index values (that is, the unbordered-map area index values), in the "y" and
	 * "x" directions, can each be -1 (when the latitude goes to image_corners.ne_lat or
	 * the longitude goes to image_corners.sw_long, respectively).
	 * We allow this negative index because it makes it conceptually easier to
	 * translate latitudes and longitudes into x and y index values.
	 * We depend on the borders around the image to absorb these negative values
	 * so that we don't scribble memory outside the memory assigned to image_corners.ptr.
	 *
	 *       Example:
	 *	 Say that we have an image that covers a 1x1 degree block, and a map area of
	 *	 1200x1200 pixels.  Thus, we have a map area 1200x1200 pixels in extent, but
	 *       we have actual DEM data that extends over 1201x1201 samples.  (250K DEM files
	 *       are mostly 1201x1201 samples in extent.)  Obviously, some
	 *       of the available data won't fit on the image.  One could just make the image area
	 *       be 1201x1201 pixels wide, but I chose a different approach.  I make the map area
	 *       span slightly less than 1 degree by 1 degree, so that some of the DEM data won't
	 *       fit on the image.   This is accomplished by aligning the DEM data with two edges
	 *       of the image, and letting the DEM data that slop over the other two edges be discarded.
	 *
	 *       It makes sense that we assign image_corners.sw_lat to pixels along
	 *       the bottom edge of the map area.  It would also make esthetic sense to put
	 *       image_corners.sw_long along the pixels that run down the left side of the map area.
	 *       However, early in development (when I was still treating west longitudes as
	 *       positive numbers) I decided to put image_corners.ne_long down the right-hand side of
	 *       the map area. (When I was treating west longitude as a positive number, the
	 *       roles of image_corners.sw_long and image_corners.ne_long were reversed.)  I decided not
	 *       to change this convention when I started using negative longitudes.
	 *       Thus, in the current program, the point represented by image_corners.sw_lat/image_corners.ne_long
	 *       is exactly the bottom right-hand corner of the map area, with map-area (x, y)
	 *       index values of (1199, 1199).  image_corners.ne_lat/image_corners.sw_long is just outside the upper
	 *       left corner of the map area, with map-area (x, y) index values of (-1, -1).
	 *
	 *	 If you think about it, it makes sense that image_corners.ne_lat and image_corners.sw_long are actually
	 *	 outside the image space.  Adjacent DEM files overlap by the width of one elevation sample
	 *       along their common boundary.  Thus, it is natural to assign the boundary to one of the 1x1
	 *       degree blocks but not to the other.  I have chosen to include the boundaries along the
	 *       bottom and right-hand sides of each DEM block, and to assign the other two boundaries to
	 *       adjacent DEM blocks.  Thus, if you display only a single DEM block, the left-hand and top
	 *       edges of the DEM data won't appear, since they fall at index values of -1.  Actually, they
	 *       wouldn't appear in any case.  The raw DEM data is converted into image data by cycling
	 *       through all of the DEM points and finding the elevation gradient at each point by taking
	 *       differences between adjacent elevation samples.  This gradient operation reduces the
	 *       1201x1201 DEM array to a 1200x1200 set of image points.  Thus, the gradient operation
	 *       gobbles up either the top or bottom row, and either the left or right column, of the raw
	 *       DEM data.  I arbitrarily chose to gobble up the top row and left column.  The resulting
	 *       array of image points exactly fits into the available 1200x1200 image array.  Thus, two
	 *       edges of the DEM block would naturally fall on the negative index values, but they don't
	 *       because we discard them in the gradient operation.
	 *
	 *       So why make such a fuss about the negative index values, since they don't get used
	 *       anyway?  The answer lies in the handling of DLG data.  Those data aren't array-based
	 *       the way the DEM data are.  Instead, they are vector data --- that is, they are sequences
	 *       of points that define piecewise-linear "curves" (like roads, streams, and so on) on the
	 *       map.  The points are given in terms of UTM coordinates, but we convert them to
	 *       latitude/longitude coordinates for entry on the map.  (I could have defined the map area
	 *       in terms of UTM coordinates instead of latitude/longitude coordinates, but latitudes and
	 *       longitudes seemed more natural --- particularly since a map could span multiple UTM zones,
	 *       which could get confusing.)  The subroutines that plot DLG data make use of the negative
	 *       index values, since roads and such can go right up to the edge of the map.  Thus, when
	 *       all of the DLG data are plotted, there will generally be roads and streams that overlap
	 *       the two white strips represented by the negative index values.  To clean up these two
	 *       strips, we fill them in with WHITE after the DLG plotting is done.
	 *
	 *	 If you look closely at a map for a 1 degree by 1 degree block, you
	 *	 will note that the tick marks for the low longitude and high latitude
	 *	 are actually one pixel beyond the edge of the map area, at the locations
	 *	 that would be specified by horizontal and vertical indices of -1.
	 */
	if (info_flag == 0)  {
		x_prime = image_corners.x + LEFT_BORDER + right_border;
		image_corners.ptr = (unsigned char *)malloc((image_corners.y + TOP_BORDER + bottom_border) * x_prime);
		if (image_corners.ptr == (unsigned char *)0)  {
			fprintf(stderr, "malloc of image_corners.ptr failed\n");
			exit(0);
		}
	}



	/*
	 * For areas of flat terrain, most of the color table goes unused,
	 * and the shaded relief is pretty boring, with only a few colors
	 * (or even only a single color) and not much shading.  When the
	 * "-z" option is given by the user, we adjust the elevation
	 * thresholds in the color table so that the full color table is
	 * used to span the elevations between min_elevation and max_elevation.
	 */
	if (z_flag != 0)  {
		for (k = 0; k < (MAX_VALID_BANDS - 1); k++)  {
			i = min_elevation < 0 ? 0 : min_elevation;
			color_tab[k].max_elevation = i + drawmap_round((double)((k + 1) * (max_elevation - i)) / (double)MAX_VALID_BANDS);
		}
		color_tab[MAX_VALID_BANDS - 1].max_elevation = HIGHEST_ELEVATION;
	}


	/*
	 * Do the big calculation for processing the DEM data.
	 *
	 * This is where we transform elevation data into pixel colors,
	 * or elevation contours, in the output image.
	 *
	 * Note that the zeroeth row and zeroeth column of the elevation data
	 * (in image_in) are discarded during this process.  They consist of the
	 * data that would be plotted at the -1 horizontal and vertical index values
	 * in image_corners.ptr.
	 */
	if (info_flag == 0)  {
		if (contour_flag == 0)  {
			/*
			 * Produce a shaded relief map.
			 */
			for (i = 1; i <= image_corners.y; i++)  {
				for (j = 1; j <= image_corners.x; j++)  {
					/*
					 * When producing shaded relief, we vary the shade of the DEM data to
					 * correspond to the gradient of the terrain at each point.  The gradient
					 * calculations determine the slope in two directions and choose the
					 * largest of the two.
					 *
					 * The basic idea is to assume that the sun is shining from the northwest
					 * corner of the image.  Then, terrain with a negative gradient (toward
					 * the northwest or west) will be brightly colored, and terrain with a
					 * positive gradient will be dark, and level terrain will be somewhere in between.
					 *
					 * In order to find the gradient, the numerator is the difference in elevation
					 * between two adjacent pixels.  The denominator is the horizontal ground distance
					 * between the locations represented by those two pixels.  In the DEM data,
					 * elevations are expressed in meters.  We also need to find the ground distance
					 * in meters.
					 *
					 * We can readily find the ground distance in terms of degrees (of latitude/longitude)
					 * per pixel.  We do that now, using the geometry of the target image.
					 * (Note that this calculation is pretty bogus, because we are treating latitudes
					 * and longitudes as rectangular coordinates.  However, we only need a crude
					 * result since the goal is to produce color shadings that give a subjective
					 * view of the gradient of the terrain.  We aren't trying to make the shadings
					 * correspond exactly to some gradient metric --- we are only trying to give the
					 * impression of a gradient.)
					 */
					res_y = (double)(image_corners.ne_lat - image_corners.sw_lat) / (double)image_corners.y;
					res_xy = sqrt((pow(image_corners.ne_lat - image_corners.sw_lat, 2.0) + pow(image_corners.ne_long - image_corners.sw_long, 2.0)) /
							    (pow((double)image_corners.x, 2.0) + pow((double)image_corners.y, 2.0)));

					/*
					 * Now we need to convert our ground distance, in degrees per pixel,
					 * into a distance in meters per pixel.  This requires that we know
					 * how many meters per degree a latitude/longitude respresents.
					 *
					 * 4.0076594e7 meters is the equatorial circumference of the earth.
					 * 3.9942e7 meters is the polar circumference of the earth.
					 *
					 * Thus, along the equator, there are 1.1132e5 meters per degree.
					 * Along a line of longitude, there are 1.1095e5 meters per degree.
					 * (The Earth has a slightly irregular shape, so these numbers are to
					 * a first approximation only.)
					 * The latter number should be reasonably accurate for any latitude,
					 * anywhere on the earth.  The former number is only accurate near
					 * the equator.  As we move further north or south, the number changes
					 * according to the cosine of the latitude:  1.1132e5 * cos(latitude).
					 *
					 * Thus, we need to multiply res_y by 1.1095e5 to get the resolution
					 * in terms of meters per pixel.  For res_xy, we use a more complicated
					 * factor:
					 *    sqrt((1.1095e5)^2 + (1.1132e5 * cos(latitude))^2)
					 */
					res_y *= 1.1095e5;
					/*
					 * f is the latitude (in degrees), found by interpolation.
					 * We still need to convert it to radians, which we do inside
					 * the cosine function call.
					 */
					f = image_corners.ne_lat - ((double)i / (double)image_corners.y) * (image_corners.ne_lat - image_corners.sw_lat);
					res_xy *= sqrt(pow(1.1095e5, 2.0) + pow(1.1132e5 * cos(f * M_PI / 180.0), 2.0));

					/*
					 * Now we are ready to find the gradients.
					 * However, if we are at the edge of the image and one or more of the
					 * gradient points is invalid, then don't find the gradient.
					 * Just set that point in the map image to WHITE.
					 */
					sptr = image_in + (i - 1) * (image_corners.x + 1) + j;
					sptr2 = image_in + i * (image_corners.x + 1) + j;
					if ((*sptr == HIGHEST_ELEVATION) || (*(sptr - 1) == HIGHEST_ELEVATION) ||
					    (*sptr2 == HIGHEST_ELEVATION))  {
						*(image_corners.ptr + (i - 1 + TOP_BORDER) * x_prime + j - 1 + LEFT_BORDER) = WHITE;
						continue;
					}
					else  {
						gradient1 = (((double)*(sptr - 1)) - ((double)*sptr2)) / res_xy;
						gradient2 = (((double)*sptr) - ((double)*sptr2)) / res_y;
						gradient3 = -10000000000.0;
						gradient = relief_mag * max3(gradient1, gradient2, gradient3);

						factor = get_factor(gradient);
//						histogram[factor]++;	/* Information for debugging. */
					}


					/*
					 * Set the color based on the elevation and the factor
					 * retrieved from the gradient calculations.
					 * This is called a "factor" for historical reasons.
					 * At one time, I experimented with finding a multiplicative
					 * factor instead of the current additive modifier.
					 * It wasn't worth going through the code and changing the name.
					 * Besides, I might want to try a factor again someday.
					 *
					 * See the file "colors.h" for a description of the color
					 * scheme.  The information is collected there so that it
					 * is easy to change the color scheme, if desired.
					 *
					 * We do a few special cases and then launch into a loop to
					 * check the bulk of the cases.
					 */
					if (*(sptr = image_in + i * (image_corners.x + 1) + j) < 0)  {
						/*
						 * Elevations can theoretically be less than 0, but it's unusual, so report it.
						 * Below sea level, we shade everything with CYAN.
						 */
//						fprintf(stderr, "An elevation was less than 0:  %d\n", *(image_in + i * (image_corners.x + 1) + j));

						*(image_corners.ptr + (i - 1 + TOP_BORDER) * x_prime + j - 1 + LEFT_BORDER) = c_index_sea + factor;
					}
					else if (*sptr == 0)  {
						/*
						 * Special case for sea level.  If things are totally flat,
						 * assume it's water.  Otherwise treat it like it's Death Valley.
						 *
						 * The reason for this special case is that the DLG files for coastal regions
						 * don't appear to treat oceans as bodies of water.  This was resulting
						 * in the ocean areas being set to GREEN (the normal color for sea-level land).
						 * Thus, I kludged in this special check; and it appears to work fine, in general.
						 *
						 * I later made it an option since, for example, sacramento-w.gz gets colored
						 * oddly, because there are areas below sea level within areas that meet the
						 * criterion for ocean.
						 */
						if (seacoast_flag != 0)  {
							if (gradient == 0.0)  {
								*(image_corners.ptr + (i - 1 + TOP_BORDER) * x_prime + j - 1 + LEFT_BORDER) = B_BLUE;
							}
							else  {
								*(image_corners.ptr + (i - 1 + TOP_BORDER) * x_prime + j - 1 + LEFT_BORDER) = c_index_sea + factor;
							}
						}
						else  {
							*(image_corners.ptr + (i - 1 + TOP_BORDER) * x_prime + j - 1 + LEFT_BORDER) = C_INDEX_0 + factor;
						}
					}
					else if (*sptr == HIGHEST_ELEVATION)  {
						/*
						 * Special case for creating WHITE areas by setting the
						 * DEM elevation data to exactly HIGHEST_ELEVATION.
						 */
						*(image_corners.ptr + (i - 1 + TOP_BORDER) * x_prime + j - 1 + LEFT_BORDER) = WHITE;
					}
					else  {
						for (k = 0; k < MAX_VALID_BANDS; k++)  {
							if (*sptr <= color_tab[k].max_elevation)  {
								*(image_corners.ptr + (i - 1 + TOP_BORDER) * x_prime + j - 1 + LEFT_BORDER) = color_tab[k].c_index + factor;
								break;
							}
						}
					}
				}
			}
		}
		else  {
			/*
			 * Instead of a shaded relief map, produce a contour map.
			 * Note that some regions have hypsographic DLG files
			 * that can be used to produce a contour map.  However, these
			 * tend to be too dense for my taste, and it seems easier to produce
			 * a contour map from scratch than to try to winnow out the
			 * relevant chunks from a DLG file.  Producing the contour maps
			 * from scratch has the added advantage that it works even if
			 * there is no DLG hypsography data available.
			 */

			/*
			 * In this pair of nested loops, we round all of the elevation
			 * data to the nearest contour interval.
			 */
			for (i = 0; i <= image_corners.y; i++)  {
				for (j = 0; j <= image_corners.x; j++)  {
					contour_trunc = floor((double)*(image_in + i * (image_corners.x + 1) + j) / contour_intvl);
					*(image_in + i * (image_corners.x + 1) + j) = (short)drawmap_round(ceil(contour_trunc * (double)contour_intvl));
				}
			}


			/*
			 * In this pair of nested loops, we use the rounded elevation
			 * data to produce a set of contours.  The algorithm is simple:
			 * If the elevation at the center of a 3x3 square is greater than
			 * at any of the locations on the border of the square, then we
			 * plot an L_ORANGE contour point.  Otherwise, we make the point WHITE
			 * (if capital_c_flag==0), or set the point to a color from the color table
			 * (if capital_c_flag!=0) where the colors are chosen by rotation.
			 */
			for (i = 1; i < image_corners.y; i++)  {
				for (j = 1; j < image_corners.x; j++)  {
					k = *(image_in + (i    ) * (image_corners.x + 1) + j    );

					if ((k > (*(image_in + (i - 1) * (image_corners.x + 1) + j - 1))) ||
					    (k > (*(image_in + (i - 1) * (image_corners.x + 1) + j    ))) ||
					    (k > (*(image_in + (i - 1) * (image_corners.x + 1) + j + 1))) ||
					    (k > (*(image_in + (i    ) * (image_corners.x + 1) + j - 1))) ||
					    (k > (*(image_in + (i    ) * (image_corners.x + 1) + j + 1))) ||
					    (k > (*(image_in + (i + 1) * (image_corners.x + 1) + j - 1))) ||
					    (k > (*(image_in + (i + 1) * (image_corners.x + 1) + j    ))) ||
					    (k > (*(image_in + (i + 1) * (image_corners.x + 1) + j + 1))))  {
						*(image_corners.ptr + (i - 1 + TOP_BORDER) * x_prime + j - 1 + LEFT_BORDER) = L_ORANGE;
					}
					else  {
						if (capital_c_flag == 0)  {
							*(image_corners.ptr + (i - 1 + TOP_BORDER) * x_prime + j - 1 + LEFT_BORDER) = WHITE;
						}
						else  {
							/*
							 * We divide by (MAX_VALID_BANDS-1), rather than by MAX_VALID_BANDS,
							 * so as to exclude the color in slot MAX_VALID_BANDS.
							 * Since this color is normally bright white, which
							 * can be a bit intrusive, we exclude it on esthetic grounds.
							 */
							k = drawmap_round(floor((double)k / contour_intvl)) % (MAX_VALID_BANDS - 1);
							*(image_corners.ptr + (i - 1 + TOP_BORDER) * x_prime + j - 1 + LEFT_BORDER) = color_tab[k].c_index;
						}
					}
				}
			}

			/*
			 * Set the pixels along the right side and bottom of the image to WHITE.
			 * They have not yet had a color defined.
			 *
			 * Note that this is only half a loaf.  If the DEM data supplied by the user
			 * doesn't cover the latitude/longitude area spanned by the image, then there
			 * will be WHITE areas within the image.  Unfortunately, there will normally
			 * be incorrect contour lines around the boundaries of these WHITE areas because
			 * the elevation in each WHITE area is initialized to HIGHEST_ELEVATION.  Thus,
			 * there will be a discontinuity in elevation between areas with valid DEM
			 * data and areas without valid DEM data.  This produces a contour line
			 * at the boundary.  This is a bummer, but it is a minor cosmetic bummer,
			 * and I'm not in the mood to fix it at this time.
			 */
			for (i = 0; i <= image_corners.y; i++)  {
				*(image_corners.ptr + (i - 1 + TOP_BORDER) * x_prime + image_corners.x - 1 + LEFT_BORDER) = WHITE;
			}
			for (j = 0; j <= image_corners.x; j++)  {
				*(image_corners.ptr + (image_corners.y - 1 + TOP_BORDER) * x_prime + j - 1 + LEFT_BORDER) = WHITE;
			}
		}
		free(image_in);
	}


	/*
	 * Process any DLG files.
	 * These files contain line and area information, for drawing
	 * things like streams, roads, boundaries, lakes, and such.
	 */
	file_index = 0;
	while (file_index < num_dlg)  {
		length = strlen(argv[optind + file_index]);

		if ((length > 3) && ((strcmp(&argv[optind + file_index][length - 3], ".gz") == 0) ||
		    (strcmp(&argv[optind + file_index][length - 3], ".GZ") == 0)))  {
			gz_flag = 1;
		}
		else  {
			gz_flag = 0;
		}

		/*
		 * Files in Spatial Data Transfer System (SDTS) format are markedly
		 * different from the optional-format DLG files.
		 *
		 * Since SDTS files are so different, we must detect them handle
		 * them separately.
		 *
		 * We insist that the user specify one, single, SDTS file on the command
		 * line for each SDTS DLG directory.  The file must be the one whose
		 * name has the form ????LE??.DDF (or ????le??.ddf), and it may have
		 * a .gz on the end if it is gzip compressed.
		 *
		 * We allow the files to be gzip-compressed, and they can have either
		 * ".gz" or ".GZ" on the end.  However, we insist that the rest of
		 * the file name have consistent case.  That is, if the 'f' or 'F'
		 * in the ".DDF" or ".ddf" is in a given case, the rest of the file
		 * had better be in that same case.
		 *
		 * If the following "if" test succeeds, we assume we have an SDTS file.
		 */
		if (((length >= 15) && (gz_flag != 0) &&
		     ((strncmp(&argv[optind + file_index][length - 7], ".ddf", 4) == 0) ||
		      (strncmp(&argv[optind + file_index][length - 7], ".DDF", 4) == 0))) ||
		    ((length >= 12) && (gz_flag == 0) &&
		     ((strcmp(&argv[optind + file_index][length - 4], ".ddf") == 0) ||
		      (strcmp(&argv[optind + file_index][length - 4], ".DDF") == 0))))  {
			/* SDTS file */

			/*
			 * Check that the file name takes the form that we expect.
			 */
			if (((gz_flag != 0) &&
			     (strncmp(&argv[optind + file_index][length - 11], "le", 2) != 0) &&
			     (strncmp(&argv[optind + file_index][length - 11], "LE", 2) != 0)) ||
			    ((gz_flag == 0) &&
			     (strncmp(&argv[optind + file_index][length - 8], "le", 2) != 0) &&
			     (strncmp(&argv[optind + file_index][length - 8], "LE", 2) != 0)))  {
				fprintf(stderr, "The file %s looks like an SDTS file, but the name doesn't look right.  Ignoring file.\n", argv[optind + file_index]);
				file_index++;
				continue;
			}

			/* If info_flag is nonzero, then just print some info about the DLG file. */
			if (info_flag == 0)  {
				fprintf(stderr, "Processing DLG file:  %s\n", argv[optind + file_index]);
			}
			else  {
				fprintf(stdout, "%s", argv[optind + file_index]);
			}

			/*
			 * The file name looks okay.  Let's launch into the information parsing.
			 */
			(void)process_dlg_sdts(argv[optind + file_index], (char *)0, gz_flag, &image_corners, info_flag, 0);
		}
		else  {
			/* Not an SDTS file. */

			if (gz_flag != 0)  {
				if ((dlg_fdesc = buf_open_z(argv[optind + file_index], O_RDONLY)) < 0)  {
					fprintf(stderr, "Can't open %s for reading, errno = %d\n", argv[optind + file_index], errno);
					exit(0);
				}
			}
			else  {
				if ((dlg_fdesc = buf_open(argv[optind + file_index], O_RDONLY)) < 0)  {
					fprintf(stderr, "Can't open %s for reading, errno = %d\n", argv[optind + file_index], errno);
					exit(0);
				}
			}

			/* If info_flag is nonzero, then just print some info about the DLG file. */
			if (info_flag == 0)  {
				fprintf(stderr, "Processing DLG file:  %s\n", argv[optind + file_index]);
			}
			else  {
				fprintf(stdout, "%s", argv[optind + file_index]);
			}

			/*
			 * With the DEM files, we parsed the header first, and then
			 * called a separate processing function, and then did some
			 * more processing here in the main body of drawmap.  DLG files are
			 * more complicated to parse, and we don't need to return any DLG
			 * data to this main processing loop.  Thus, we just encapsulate
			 * all parsing and processing into a single function call.
			 */
			process_dlg_optional(dlg_fdesc, gz_flag, &image_corners, info_flag);

			if (gz_flag == 0)  {
				buf_close(dlg_fdesc);
			}
			else  {
				buf_close_z(dlg_fdesc);
			}
		}

		file_index++;
	}
	if (info_flag != 0)  {
		exit(0);
	}


	/* Select a font size, based on the image size. */
	if ((image_corners.x >= 1000) && (image_corners.y >= 1000))  {
		font_width = 6;
		font_height = 10;
		font = &font_6x10[0][0];
	}
	else  {
		font_width = 5;
		font_height = 8;
		font = &font_5x8[0][0];
	}


	/*
	 * Process any GNIS data.
	 * GNIS data consists of place names, with specific latitude/longitude
	 * coordinates, and other data.  We put a cursor at each given location
	 * and add the place name beside it.
	 */
	if (gnis_file != (char *)0)  {
		if (strcmp(gnis_file + strlen(gnis_file) - 3, ".gz") == 0)  {
			gz_flag = 1;
			if ((gnis_fdesc = buf_open_z(gnis_file, O_RDONLY)) < 0)  {
				fprintf(stderr, "Can't open %s for reading, errno = %d\n", gnis_file, errno);
				exit(0);
			}
		}
		else  {
			gz_flag = 0;
			if ((gnis_fdesc = buf_open(gnis_file, O_RDONLY)) < 0)  {
				fprintf(stderr, "Can't open %s for reading, errno = %d\n", gnis_file, errno);
				exit(0);
			}
		}

		fprintf(stderr, "Processing GNIS file:  %s\n", gnis_file);

		while ( 1 )  {
			/*
			 * There are two kinds of GNIS files at the http://mapping.usgs.gov/
			 * web site.  I call them old-style and new-style, because for years the
			 * old-style files were all that were available; and then, in 1998, the new-style
			 * files appeared as well.  In the old-style files each record is fixed length
			 * (147 bytes with a newline, or 148 bytes with a newline and carriage return).
			 * The fields are at fixed positions within this record, with white-space padding
			 * between the fields.  Here is a sample (I added the <> delimiters at the beginning
			 * and end of the record):
			 *
			 * <MT Blue Mountain Saddle                               locale    Missoula        464828N 1141302W                    5640 Blue Mountain             >
			 *
			 * New style records are similar, but have delimiters of the form ',' as shown in
			 * this sample:
			 *
			 * <"MT","Blue Mountain Saddle","locale","Missoula","30","063","464828N","1141302W","46.80778","-114.21722","","","","","5640","","Blue Mountain">
			 *
			 * We attempt to handle both formats here.
			 *
			 *
			 * HISTORICAL NOTE:
			 * Apparently, early in 2000 (although I am not sure exactly when), the format of both
			 * the old and new style GNIS files changed.  Here (simply for historical completeness)
			 * are some samples of the pre-change versions:
			 * <Blue Mountain Saddle                                                                                locale   Missoula                           30063464828N1141302W                 5640Blue Mountain                                        >
			 * <Blue Mountain Saddle','locale','Missoula','30','063','464828N','1141302W','46.80778','-114.21722','','','','','5640','','Blue Mountain                                                                                                               >
			 * Beginning with drawmap version 1.10, these older versions are no longer handled.
			 */
			if (gz_flag == 0)  {
				if ((ret_val = get_a_line(gnis_fdesc, buf, MAX_GNIS_RECORD - 1)) <= 0)  {
					break;
				}
			}
			else  {
				if ((ret_val = get_a_line_z(gnis_fdesc, buf, MAX_GNIS_RECORD - 1)) <= 0)  {
					break;
				}
			}
			buf[ret_val] = '\0';
			/* Strip off trailing CR and/or LF */
			if ((buf[ret_val - 1] == '\n') || (buf[ret_val - 1] == '\r'))  {
				ret_val--;
				buf[ret_val] = '\0';
			}
			if ((buf[ret_val - 1] == '\n') || (buf[ret_val - 1] == '\r'))  {
				ret_val--;
				buf[ret_val] = '\0';
			}

			/*
			 * We need to figure out whether it is an old-style or new-style record.
			 */
			if ((tok_ptr = strstr(buf, "\",\"")) != (char *)0)  {
				/* New-style record. */
				if ((tok_ptr + 3) < (buf + ret_val))  {
					tok_ptr += 3;
					gnis_feature_name = tok_ptr;
				}
				else  {
					fprintf(stderr, "Defective GNIS record:  <%s>\n", buf);
					continue;
				}
				for (i = 0; i < 7; i++)  {
					if (((tok_ptr = strstr(tok_ptr, "\",\"")) != (char *)0) && (*tok_ptr != '\0'))  {
						if (i == 0)  {
							/*
							 * Capture the feature name for later use.
							 * Skip over the state name at the front.
							 */
							length = tok_ptr - gnis_feature_name;
						}
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
					fprintf(stderr, "Defective GNIS record:  <%s>\n", buf);
					continue;
				}

				/*
				 * Capture the feature name for later use.
				 * Begin by skipping over the state name at the front.
				 */
				gnis_feature_name = buf;
				while (*gnis_feature_name != ' ')  gnis_feature_name++;
				while (*gnis_feature_name == ' ')  gnis_feature_name++;

				/* Work backwards from the end of the field to remove trailing blanks. */
				for (length = 53; length >= 0; length--)  {
					if (buf[length] != ' ')  {
						break;
					}
				}
				length++;
				length = length - (gnis_feature_name - buf);

				/*
				 * Note:  We assume latitude_low, longitude_low, latitude_high, and longitude_high
				 * were entered in decimal degrees.
				 * latitude and longitude from the old-style GNIS files, however are in DDDMMSS format, and
				 * require special conversion functions.
				 */
				if ((buf[86] != 'N') && (buf[86] != 'S'))  {
					/* Defective record */
					fprintf(stderr, "Defective GNIS record:  <%s>\n", buf);
					continue;
				}
				if ((buf[95] != 'E') && (buf[95] != 'W'))  {
					/* Defective record */
					fprintf(stderr, "Defective GNIS record:  <%s>\n", buf);
					continue;
				}
				latitude = lat_conv(&buf[80]);
				longitude = lon_conv(&buf[88]);
			}

			/* Ignore this entry if it is out of the map area. */
			if ((latitude < image_corners.sw_lat) || (latitude > image_corners.ne_lat))  {
				continue;
			}
			if ((longitude < image_corners.sw_long) || (longitude > image_corners.ne_long))  {
				continue;
			}

			/* draw a cursor at the specified point */
			xx = - 1 + drawmap_round((longitude - image_corners.sw_long) * (double)image_corners.x / (image_corners.ne_long - image_corners.sw_long));
			yy = image_corners.y - 1 - drawmap_round((latitude - image_corners.sw_lat) * (double)image_corners.y / (image_corners.ne_lat - image_corners.sw_lat));

			a = WHITE;
			for (i = -3; i <= 3; i++)  {
				if (((xx + i) >= 0) && ((xx + i) <= (image_corners.x - 1)))  {
					if (*(image_corners.ptr + (yy + TOP_BORDER) * x_prime + xx + LEFT_BORDER + i) == WHITE)  {
						a = BLACK;
						break;
					}
				}
				if (((yy + i) >= 0) && ((yy + i) <= (image_corners.y - 1)))  {
					if (*(image_corners.ptr + (yy + TOP_BORDER + i) * x_prime + xx + LEFT_BORDER) == WHITE)  {
						a = BLACK;
						break;
					}
				}
			}
			for (i = -3; i <= 3; i++)  {
				if (((xx + i) >= 0) && ((xx + i) <= (image_corners.x - 1)))  {
					*(image_corners.ptr + (yy + TOP_BORDER) * x_prime + xx + LEFT_BORDER + i) = a;
				}
				if (((yy + i) >= 0) && ((yy + i) <= (image_corners.y - 1)))  {
					*(image_corners.ptr + (yy + TOP_BORDER + i) * x_prime + xx + LEFT_BORDER) = a;
				}
			}

			/* If there was a feature name, then put it into the image near the cursor */
			if (length > 0)  {
				if ((xx + 5 + length * font_width) >= image_corners.x)  {
					start_x = xx - 5 - length * font_width;
				}
				else  {
					start_x = xx + 5;
				}
				if ((yy + (font_height >> 1) - 1) >= image_corners.y)  {
					start_y = image_corners.y - font_height;
				}
				else if ((yy - (font_height >> 1)) < 0)  {
					start_y = 0;
				}
				else  {
					start_y = yy - (font_height >> 1);
				}

				gnis_feature_name[length] = '\0';
				add_text(&image_corners, gnis_feature_name, length, start_x + LEFT_BORDER,
					 start_y + TOP_BORDER, font, font_width, font_height, WHITE, -1);
			}
		}
		if (gz_flag == 0)  {
			buf_close(gnis_fdesc);
		}
		else  {
			buf_close_z(gnis_fdesc);
		}
	}


	/*
	 * Put a white border around the edges of the output image.
	 * Note that this will cover up the one-pixel slop over the left
	 * and top edges that is the result of the fact that we
	 * set the latitude and longitude to whole-number values, while
	 * the pixels don't quite cover that whole area.
	 * This was discussed at length in a previous comment.
	 *
	 * Note that the DEM file data don't slop over the edges because,
	 * when we process them, they are already in the form of an array of
	 * points, and we can cleanly discard the data we don't need.
	 * However, the DLG and GNIS data are in the form of
	 * latitude/longitude or UTM grid coordinates, and it is possible
	 * for array index values of -1 to crop up at the image edges.
	 * (In the case of GNIS data, we explicitly check for this and
	 * don't slop over.  For DLG data, we don't bother because it is
	 * cheaper in CPU time to just null out the border here.)
	 */
	for (i = 0; i < TOP_BORDER; i++)  {
		for (j = 0; j < (image_corners.x + LEFT_BORDER + right_border); j++)  {
			*(image_corners.ptr + i * x_prime + j) = WHITE;
		}
	}
	for (i = image_corners.y + TOP_BORDER; i < (image_corners.y + TOP_BORDER + bottom_border); i++)  {
		for (j = 0; j < (image_corners.x + LEFT_BORDER + right_border); j++)  {
			*(image_corners.ptr + i * x_prime + j) = WHITE;
		}
	}
	for (i = TOP_BORDER; i < (image_corners.y + TOP_BORDER); i++)  {
		for (j = 0; j < LEFT_BORDER; j++)  {
			*(image_corners.ptr + i * x_prime + j) = WHITE;
		}
	}
	for (i = TOP_BORDER; i < (image_corners.y + TOP_BORDER); i++)  {
		for (j = image_corners.x + LEFT_BORDER; j < (image_corners.x + LEFT_BORDER + right_border); j++)  {
			*(image_corners.ptr + i * x_prime + j) = WHITE;
		}
	}


	/*
	 * Add a copyright notice to the image if, when the program was compiled, the
	 * makefile contained a non-null COPYRIGHT_NAME.
	 */
	if (COPYRIGHT_NAME[0] != '\0')  {
		time_val = time((time_t *)0);
		sprintf(buf, "Copyright (c) %4.4s  %s", ctime(&time_val) + 20, COPYRIGHT_NAME);
		length = strlen(buf);
		add_text(&image_corners, buf, length, image_corners.x + LEFT_BORDER + right_border - (length * font_width + 4),
			 image_corners.y + TOP_BORDER + bottom_border - font_height - 4, font, font_width, font_height, BLACK, WHITE);
	}


	if (tick_flag != 0)  {
		/*
		 * Put some latitude/longitude tick marks on the edges of the image.
		 *
		 * The purpose of the 0.049999999999 is to round the latitude/longitude up to
		 * the nearest tenth.  Since we put a tick mark every tenth of a degree,
		 * we need to find the first round tenth above image_corners.sw_lat/image_corners.sw_long.
		 */
		i = drawmap_round((image_corners.sw_lat + 0.049999999999) * 10.0);
		for (; i <= ((image_corners.ne_lat + 0.0000001) * 10.0); i++)  {
			k = TOP_BORDER - 1 + image_corners.y - drawmap_round((double)image_corners.y * ((double)i * 0.1 - image_corners.sw_lat) / (image_corners.ne_lat - image_corners.sw_lat));
			if (((i % 10) == 0) || ((i % 10) == 5) || ((i % 10) == -5))  {
				tick_width = 6;

				sprintf(buf, "%.2f%c", fabs((double)i / 10.0), i < 0 ? 'S' : 'N');
				length = strlen(buf);
				add_text(&image_corners, buf, length, image_corners.x + LEFT_BORDER + 7, k - (font_height >> 1), font, font_width, font_height, BLACK, WHITE);
				add_text(&image_corners, buf, length, LEFT_BORDER - 8 - font_width * length, k - (font_height >> 1), font, font_width, font_height, BLACK, WHITE);
			}
			else  {
				tick_width = 4;
			}

			for (j = LEFT_BORDER - 1; j > (LEFT_BORDER - 1 - tick_width); j--)  {	/* Left side */
				*(image_corners.ptr + k * x_prime + j) = BLACK;
			}
			for (j = image_corners.x + LEFT_BORDER; j < (image_corners.x + LEFT_BORDER + tick_width); j++)  {	/* Right side */
				*(image_corners.ptr + k * x_prime + j) = BLACK;
			}
		}
		i = drawmap_round((image_corners.sw_long + 0.049999999999) * 10.0);
		for (; i <= ((image_corners.ne_long + 0.0000001) * 10.0); i++)  {
			k = LEFT_BORDER - 1 + drawmap_round((double)image_corners.x * ((double)i * 0.1 - image_corners.sw_long) / (image_corners.ne_long - image_corners.sw_long));

			if (((i % 10) == 0) || ((i % 10) == 5) || ((i % 10) == -5))  {
				if (((i % 10) == 0) || (res_x_image > ((double)font_width * 15.0)))  {
					tick_width = 6;

					sprintf(buf, "%.2f%c", fabs((double)i / 10.0), i < 0 ? 'W' : 'E');
					length = strlen(buf);
					add_text(&image_corners, buf, length, k - ((length * font_width) >> 1), image_corners.y + TOP_BORDER + 6, font, font_width, font_height, BLACK, WHITE);
					add_text(&image_corners, buf, length, k - ((length * font_width) >> 1), TOP_BORDER - 7 - font_height, font, font_width, font_height, BLACK, WHITE);
				}
			}
			else  {
				tick_width = 4;
			}

			for (j = TOP_BORDER - 1; j > (TOP_BORDER - 1 - tick_width); j--)  {	/* Top */
				*(image_corners.ptr + j * x_prime + k) = BLACK;
			}
			for (j = image_corners.y + TOP_BORDER; j < (image_corners.y + TOP_BORDER + tick_width); j++)  {	/* Bottom */
				*(image_corners.ptr + j * x_prime + k) = BLACK;
			}
		}
	}


	/* Add some information at the top of the image, as an image label (if there is room). */
	if (dem_name[0] != '\0')  {
		sprintf(buf, "%s --- ", dem_name);
	}
	else  {
		buf[0] = '\0';
	}
	sprintf(buf + strlen(buf), "%.5g%c, %.6g%c to %.5g%c, %.6g%c",
		fabs(image_corners.sw_lat), image_corners.sw_lat < 0 ? 'S' : 'N',
		fabs(image_corners.sw_long), image_corners.sw_long < 0 ? 'W' : 'E',
		fabs(image_corners.ne_lat), image_corners.ne_lat < 0 ? 'S' : 'N',
		fabs(image_corners.ne_long), image_corners.ne_long < 0 ? 'W' : 'E');
	length = strlen(buf);
	if ((length * font_width) <= (image_corners.x + LEFT_BORDER + right_border - 2))  {
		add_text(&image_corners, buf, length, (image_corners.x >> 1) + LEFT_BORDER - 1 - ((length * font_width) >> 1),
			 (TOP_BORDER >> 1) - 1 - (font_height >> 1) - font_height, font, font_width,
			 font_height, BLACK, WHITE);

		if ((max_elevation != -100000) && (min_elevation != 100000))  {
			/*
			 * If the max/min elevation data is valid, then indicate
			 * the maximum/minimum elevation
			 */
			latitude1 = image_corners.sw_lat + (image_corners.ne_lat - image_corners.sw_lat) * (double)(image_corners.y - min_e_lat) / (double)image_corners.y;
			longitude1 = image_corners.sw_long + (image_corners.ne_long - image_corners.sw_long) * (double)min_e_long / (double)image_corners.x;
			latitude2 = image_corners.sw_lat + (image_corners.ne_lat - image_corners.sw_lat) * (double)(image_corners.y - max_e_lat) / (double)image_corners.y;
			longitude2 = image_corners.sw_long + (image_corners.ne_long - image_corners.sw_long) * (double)max_e_long / (double)image_corners.x;
			sprintf(buf, "Elevations: %dm (%dft) at %.5g%c %.6g%c, %dm (%dft) at %.5g%c %.6g%c",
				min_elevation,
				drawmap_round((double)min_elevation * 3.28084),
				fabs(latitude1), latitude1 < 0 ? 'S' : 'N',
				fabs(longitude1), longitude1 < 0 ? 'W' : 'E',
				max_elevation,
				drawmap_round((double)max_elevation * 3.28084),
				fabs(latitude2), latitude2 < 0 ? 'S' : 'N',
				fabs(longitude2), longitude2 < 0 ? 'W' : 'E');
			length = strlen(buf);
			if ((length * font_width) <= (image_corners.x + LEFT_BORDER + right_border - 2))  {
				add_text(&image_corners, buf, length, (image_corners.x >> 1) + LEFT_BORDER - 1 - ((length * font_width) >> 1),
					 (TOP_BORDER >> 1) - 1 - (font_height >> 1) + 2, font, font_width,
					 font_height, BLACK, WHITE);
			}
		}
	}


	if (contour_flag == 0)  {
		/* Add an elevation color chart at the bottom of the image, if there is room. */
		if ((num_dem > 0) && ((image_corners.x + LEFT_BORDER + right_border - 2) >= COLOR_CHART_WIDTH) &&
		    (bottom_border >= (30 + 3 * font_height)))  {
			for (i = 0; i < COLOR_CHART_WIDTH; i++)  {
				for (j = 0; j < 16; j++)  {
					/*
					 * To represent a given range of elevation, we draw a square of
					 * a given color.  We pick one of the 16 possible colors for each elevation.
					 * This is not perfect, but it at least gives the user some
					 * clue as to how to decode the image.  I tried filling in
					 * all 16 colors within each elevation square, but it didn't
					 * look all that good.
					 */
					*(image_corners.ptr + (TOP_BORDER + image_corners.y + (bottom_border >> 1) - ((16 + 4 + font_height * 2) >> 1) + j) * x_prime +
						LEFT_BORDER + (image_corners.x >> 1) - (COLOR_CHART_WIDTH >> 1) + i) = (i & ~0xf) + 3;
				}
				if ((i & 0xf) == 0)  {
					/* Add a tick mark */
					*(image_corners.ptr + (TOP_BORDER + image_corners.y + (bottom_border >> 1) + 6 - font_height) * x_prime +
						LEFT_BORDER + (image_corners.x >> 1) - (COLOR_CHART_WIDTH >> 1) + (i & 0xf0)) = BLACK;
					*(image_corners.ptr + (TOP_BORDER + image_corners.y + (bottom_border >> 1) + 7 - font_height) * x_prime +
						LEFT_BORDER + (image_corners.x >> 1) - (COLOR_CHART_WIDTH >> 1) + (i & 0xf0)) = BLACK;
					*(image_corners.ptr + (TOP_BORDER + image_corners.y + (bottom_border >> 1) + 8 - font_height) * x_prime +
						LEFT_BORDER + (image_corners.x >> 1) - (COLOR_CHART_WIDTH >> 1) + (i & 0xf0)) = BLACK;
					if (z_flag == 0)  {
						/* Put a text label under the tick mark. */
						sprintf(buf, "%d", (i >> 4));
						length = strlen(buf);
						add_text(&image_corners, buf, length, LEFT_BORDER + (image_corners.x >> 1) - (COLOR_CHART_WIDTH >> 1) + (i & 0xf0) - ((font_width * length) >> 1),
							TOP_BORDER + image_corners.y + (bottom_border >> 1) + 9 - font_height,
							font, font_width, font_height, BLACK, WHITE);
					}
				}
			}

			/* Add a tick mark at the right end of the scale */
			*(image_corners.ptr + (TOP_BORDER + image_corners.y + (bottom_border >> 1) + 6 - font_height) * x_prime +
				LEFT_BORDER + (image_corners.x >> 1) - (COLOR_CHART_WIDTH >> 1) + (i & 0xf0)) = BLACK;
			*(image_corners.ptr + (TOP_BORDER + image_corners.y + (bottom_border >> 1) + 7 - font_height) * x_prime +
				LEFT_BORDER + (image_corners.x >> 1) - (COLOR_CHART_WIDTH >> 1) + (i & 0xf0)) = BLACK;
			*(image_corners.ptr + (TOP_BORDER + image_corners.y + (bottom_border >> 1) + 8 - font_height) * x_prime +
				LEFT_BORDER + (image_corners.x >> 1) - (COLOR_CHART_WIDTH >> 1) + (i & 0xf0)) = BLACK;

			if (z_flag == 0)  {
				/* Put in an "infinity" sign by jamming two 'o' characters together. */
				sprintf(buf, "o");
				length = strlen(buf);
				add_text(&image_corners, buf, length, LEFT_BORDER + (image_corners.x >> 1) - (COLOR_CHART_WIDTH >> 1) + (i & 0xf0) - 1,
					TOP_BORDER + image_corners.y + (bottom_border >> 1) + 9 - font_height,
					font, font_width, font_height, BLACK, -2);
				add_text(&image_corners, buf, length, LEFT_BORDER + (image_corners.x >> 1) - (COLOR_CHART_WIDTH >> 1) + (i & 0xf0) - ((font_width * length) >> 1) - 2,
					TOP_BORDER + image_corners.y + (bottom_border >> 1) + 9 - font_height,
					font, font_width, font_height, BLACK, -2);
			}
			else  {
				/*
				 * If z_flag is set, then we have altered the elevations in the color
				 * map so that the entire color map gets used between min_elevation
				 * and max_elevation.  In this case, we don't try to label every
				 * tick mark.  We just label the two end tick marks with min_elevation
				 * and max_elevation.
				 */
				i = min_elevation < 0 ? 0 : min_elevation;
				sprintf(buf, "%-5.4g", (double)drawmap_round((double)i * 3.28084) / 1000.0);
				length = strlen(buf);
				add_text(&image_corners, buf, length, LEFT_BORDER + (image_corners.x >> 1) - (COLOR_CHART_WIDTH >> 1) - (font_width >> 1),
					TOP_BORDER + image_corners.y + (bottom_border >> 1) + 9 - font_height,
					font, font_width, font_height, BLACK, WHITE);

				sprintf(buf, "%5.4g", (double)drawmap_round((double)max_elevation * 3.28084) / 1000.0);
				length = strlen(buf);
				add_text(&image_corners, buf, length, LEFT_BORDER + (image_corners.x >> 1) - (COLOR_CHART_WIDTH >> 1) + (COLOR_CHART_WIDTH & 0xf0) - (font_width >> 1) * ((length << 1) - 1),
					TOP_BORDER + image_corners.y + (bottom_border >> 1) + 9 - font_height,
					font, font_width, font_height, BLACK, WHITE);
			}

			/* Add a line to describe the units. */
			sprintf(buf, "Thousands of feet.");
			length = strlen(buf);
			add_text(&image_corners, buf, length, (image_corners.x >> 1) + LEFT_BORDER - 1 - ((length * font_width) >> 1),
				 TOP_BORDER + image_corners.y + (bottom_border >> 1) + 9, font, font_width,
				 font_height, BLACK, WHITE);
		}
	}
	else  {
		/* Add a message about the contour interval at the bottom of the image, if there is room. */
		if (num_dem > 0)  {
			sprintf(buf, "Contour interval is %.2f meters (%.2f feet).", contour_intvl, contour_intvl * 3.28084);
			length = strlen(buf);
			if ((length * font_width) <= (image_corners.x + LEFT_BORDER + right_border - 2))  {
				add_text(&image_corners, buf, length, (image_corners.x >> 1) + LEFT_BORDER - 1 - ((length * font_width) >> 1),
					 TOP_BORDER + image_corners.y + (bottom_border >> 1) + 1 + (font_height >> 1), font, font_width,
					 font_height, BLACK, WHITE);
			}
		}
	}


	/* Create the output file. */
	if ((output_fdesc = open(output_file, O_WRONLY | O_CREAT | O_TRUNC, 0644)) < 0)  {
		fprintf(stderr, "Can't create %s for writing, errno = %d\n", output_file, errno);
		exit(0);
	}


	/* Initialize SUN rasterfile header. */
	hdr.magic = MAGIC;
	hdr.width = image_corners.x + LEFT_BORDER + right_border;
	hdr.height = image_corners.y + TOP_BORDER + bottom_border;
	hdr.depth = 8;
	hdr.length = (image_corners.x + LEFT_BORDER + right_border) * (image_corners.y + TOP_BORDER + bottom_border);
	hdr.type = STANDARD;
	hdr.maptype = EQUAL_RGB;
	hdr.maplength = 768;

	/*
	 * Write SUN rasterfile header and color map.
	 * My X86 Linux machine (LITTLE_ENDIAN) requires some swabbing
	 * (byte swapping) in the rasterfile header.
	 * You may have a BIG_ENDIAN machine (which should require no
	 * swabbing at all), a PDP_ENDIAN machine (which requires a
	 * more complicated swabbing), or something else (with its
	 * own form of swabbing).
	 */
	byte_order = swab_type();
	if (byte_order == 0)  {
		/* BIG_ENDIAN: Do nothing */
	}
	else if (byte_order == 1)  {
		/* LITTLE_ENDIAN */
		lsize = sizeof(struct rasterfile) / 4;
		lptr = (int32_t *)&hdr;
		for (i = 0; i < lsize; i++)  {
			LE_SWAB(lptr);
			lptr++;
		}
	}
	else if (byte_order == 2)  {
		/* PDP_ENDIAN */
		lsize = sizeof(struct rasterfile) / 4;
		lptr = (int32_t *)&hdr;
		for (i = 0; i < lsize; i++)  {
			PDP_SWAB(lptr);
			lptr++;
		}
	}
	else  {
		/* Unknown */
		fprintf(stderr, "Unknown machine type:  you will need to modify drawmap.c to do proper swabbing.\n");
		exit(0);
	}
	write(output_fdesc, &hdr, sizeof(struct rasterfile));
	write(output_fdesc, map, sizeof(map));


	/* Output the image data. */
	for (i = 0; i < (image_corners.y + TOP_BORDER + bottom_border); i++)  {
		write(output_fdesc, image_corners.ptr + i * x_prime, image_corners.x + LEFT_BORDER + right_border);
	}

	free(image_corners.ptr);
	close(output_fdesc);


	/* For debugging. */
/*	for (i = 0; i < 256; i++)  {
 *		if (histogram[i] != 0)  {
 *			fprintf(stderr, "histogram[%3d] = %d\n", i, histogram[i]);
 *		}
 *	}
 */

	exit(0);
}




/*
 * Convert elevation gradient information into an index that
 * can be used to select a color from the color table.
 * This routine was largely developed by trial and error.
 * There is no deep theory associated with the numeric values
 * contained herein.
 */
int32_t
get_factor(double gradient)
{
	double angle, fraction;

	/*
	 * A table that works fairly well:
	 *
	 *	0.405,
	 *	0.445,
	 *	0.470,
	 *	0.485,
	 *	0.495,
	 *	0.497,
	 *	0.499,
	 *	0.500,
	 *	0.501,
	 *	0.503,
	 *	0.505,
	 *	0.515,
	 *	0.530,
	 *	0.555,
	 *	0.595,
	 *
	 * The table is duplicated in this comment so that we can
	 * play with the actual table without losing track of a set of
	 * values that work reasonably well.
	 */
	double table[15] =  {
		0.405,
		0.445,
		0.470,
		0.485,
		0.495,
		0.497,
		0.499,
		0.500,
		0.501,
		0.503,
		0.505,
		0.515,
		0.530,
		0.555,
		0.595,
	};

	/* One possible way to create the table automatically. */
//	for (i = 0; i < 15; i++)  {
//		table[i] = table[0] + (table[14] - table[0]) * pow((table[i] - table[0]) / (table[14] - table[0]), 0.9);
//	}

	angle = atan(gradient) + (M_PI/2.0);
	fraction = angle / (M_PI);
//	angle_hist[drawmap_round(fraction * 100000.0)]++;	/* For debugging. */
//	total++;	/* For debugging. */

	if (fraction > 1.0)  {
		fprintf(stderr, "bad fraction in get_factor(%f):  %f\n", gradient, fraction);
	}

	if (fraction < table[0])  {
		return(0);
	}
	else if (fraction < table[1])  {
		return(1);
	}
	else if (fraction < table[2])  {
		return(2);
	}
	else if (fraction < table[3])  {
		return(3);
	}
	else if (fraction < table[4])  {
		return(4);
	}
	else if (fraction < table[5])  {
		return(5);
	}
	else if (fraction < table[6])  {
		return(6);
	}
	else if (fraction < table[7])  {
		return(7);
	}
	else if (fraction < table[8])  {
		return(8);
	}
	else if (fraction < table[9])  {
		return(9);
	}
	else if (fraction < table[10])  {
		return(10);
	}
	else if (fraction < table[11])  {
		return(11);
	}
	else if (fraction < table[12])  {
		return(12);
	}
	else if (fraction < table[13])  {
		return(13);
	}
	else if (fraction < table[14])  {
		return(14);
	}
	else  {
		return(15);
	}
}




/*
 * Write a text string into the image.
 */
void
add_text(struct image_corners *image_corners, char *text_string, int32_t text_string_length, int32_t top_left_x,
	 int32_t top_left_y, char *font, int32_t font_width, int32_t font_height, int32_t foreground, int32_t background)
{
	int32_t i, j, k;
	int32_t bit;

	/*
	 * Cycle through the font table for each given character in the text string.
	 * Characters are represented as bit maps, with a 1 indicating part of the
	 * character, and a 0 indicating part of the background.
	 */
	for (i = 0; i < text_string_length; i++)  {
		for (j = 0; j < font_width; j++)  {
			for (k = 0; k < font_height; k++)  {
				bit = (*(font + k * 128 + *(text_string + i)) >> (font_width - 1 - j)) & 1;
				if (bit != 0)  {
					/* foreground */
					*(image_corners->ptr + (top_left_y + k) * x_prime + top_left_x + i * font_width + j) = foreground;
				}
				else  {
					/* background */
					if (background < 0)  {
						/*
						 * If the background color map index is -1, then
						 * we don't insert a specific background value, but rather
						 * reduce the existing background in intensity.
						 *
						 * If the background color map index is any other negative
						 * number, then we use a clear background.
						 */
						if (background == -1)  {
							*(image_corners->ptr + (top_left_y + k) * x_prime + top_left_x + i * font_width + j) +=
								(16 - (*(image_corners->ptr + (top_left_y + k) * x_prime + top_left_x + i * font_width + j) & 0xf)) >> 1;
						}
					}
					else  {
						*(image_corners->ptr + (top_left_y + k) * x_prime + top_left_x + i * font_width + j) = background;
					}
				}
			}
		}
	}
}





/*
 * This routine prepares a storage array for DEM data.
 * Since the code is fairly long, and appears several times
 * in the program, it has been encapsulated here.
 */
void
get_short_array(short **ptr, int32_t x, int32_t y)
{
	int32_t i, j;

	/*
	 * Get memory for the DEM data.
	 *
	 * As all of the DEM files are read in, their data
	 * eventually get combined into this storage area.
	 * On the way, the data may get cropped, smoothed, or
	 * subsampled.
	 */
	*ptr = (short *)malloc(sizeof(short) * (y + 1) * (x + 1));
	if (*ptr == (short *)0)  {
		fprintf(stderr, "malloc of *ptr failed\n");
		exit(0);
	}


	/*
	 * Before reading in the DEM data, initialize the entire image to
	 * HIGHEST_ELEVATION, which will eventually be translated to the color WHITE.
	 * This is because we don't know, in advance, which parts of the image will
	 * be covered with data from the various DEM files.  The user does not have
	 * to provide enough DEM files to fully tile the user-specified range of
	 * latitude and longitude.
	 */
	for (i = 0; i <= y; i++)  {
		for (j = 0; j <= x; j++)  {
			*(*ptr + i * (x + 1) + j) = HIGHEST_ELEVATION;
		}
	}
}






/*
 * This function produces a texture map, for use with the "povray"
 * ray tracing package, that corresponds to the height-field map
 * produced in response to the "-h" option.
 *
 * If you aren't familiar with ray tracing, and povray, this
 * function probably won't mean much to you.  If you are
 * familiar with povray, then the function's purpose should be
 * fairly obvious.
 */
void
gen_texture(int32_t min_elevation, int32_t max_elevation, struct color_tab *color_tab, char *output_file)
{
	FILE *texture_stream;
	int32_t i;
	double inflection;

	if ((texture_stream = fopen("drawmap.pov", "w+")) < 0)  { 
		fprintf(stderr, "Can't create %s for writing, errno = %d\n", "drawmap.pov", errno); 
		exit(0);
	}


	/*
	 * Put some useful comments at the top of the file.
	 */
	fprintf(texture_stream, "// Povray (version 3.6) file, generated by drawmap.\n");
	fprintf(texture_stream, "// Assuming that you have povray-3.6 installed in the normal place,\n// this file should be render-able by typing:\n");
	fprintf(texture_stream, "// povray +L/usr/local/share/povray-3.6/include +A +Idrawmap.pov +Odrawmap.tga +SP8 +EP1 +H600 +W600 +D11\n");
	fprintf(texture_stream, "// The file will probably require manual editing to get things the way you want them.\n\n");
	fprintf(texture_stream, "#include \"colors.inc\"\n\n");

	/*
	 * Generate a texture entry for sea-level, using bright blue from the color map.
	 */
	fprintf(texture_stream, "#declare TextureSea = texture { pigment { color rgb<%g, %g, %g> } finish { ambient 0.1 diffuse 0.4 brilliance 1.0 reflection 1.0 phong 1.0 phong_size 30.0 }}\n",
		((double)brights[2].red) / 255.0, ((double)brights[2].green) / 255.0, ((double)brights[2].blue) / 255.0);
	/*
	 * Generate texture entries for other elevations, using whichever color map is currently in use.
	 */
	for (i = 0; i < MAX_VALID_BANDS; i++)  {
		fprintf(texture_stream, "#declare Texture%d = texture { pigment { color rgb<%g, %g, %g> } finish { ambient 0.1 diffuse 0.4 brilliance 1.0 reflection 1.0 phong 1.0 phong_size 30.0 }}\n", i, 
			(double)color_tab[i].red / 255.0, (double)color_tab[i].green / 255.0, (double)color_tab[i].blue / 255.0);
	}

	/*
	 * Generate the main body of the file, including the texture map.
	 */
	fprintf(texture_stream, "camera{\n\tlocation <0.5, 15, -16>\n\tlook_at 0\n\tangle 30\n}\n\n");

	fprintf(texture_stream, "light_source{ <-1000,1000,-1000> White }\n\n");

	fprintf(texture_stream, "// height field generated for source data with elevations ranging from %d to %d.\n",
				min_elevation, max_elevation);
	fprintf(texture_stream, "// Points with negative elevations in the original data may have been set to zero.\n");
	fprintf(texture_stream, "// Points with undefined elevations in the original data may have been set to zero.\n");
	fprintf(texture_stream, "height_field {\n\tpgm \"%s\" water_level %g\n\tsmooth\n\ttexture {\n",
				output_file, (double)min_elevation / (double)max_elevation);
	fprintf(texture_stream, "\t\tgradient y\n");
	fprintf(texture_stream, "\t\ttexture_map  {\n");

	fprintf(texture_stream, "\t\t[ 0.0 TextureSea ]\n");
	fprintf(texture_stream, "\t\t[ 0.000001 Texture0 ]\n");
	for (i = 1; i < MAX_VALID_BANDS; i++)  {
		inflection = (double)color_tab[i - 1].max_elevation / (double)max_elevation;
		if (inflection > 1.0)  {
			break;
		}
		fprintf(texture_stream, "\t\t[ %g Texture%d ]\n", inflection, i);
	}

	fprintf(texture_stream, "\t\t}\n\t}\n");
	fprintf(texture_stream, "//\tThe middle scale factor in the \"scale\" line controls how much the terrain is stretched vertically.\n");
	fprintf(texture_stream, "\ttranslate <-0.5, -0.5, -0.5>\n\tscale <10, 0.8, 10>\n}\n");

	fclose(texture_stream);
}
