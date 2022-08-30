/*
 * =========================================================================
 * dem.c - Routines to handle DEM data.
 * Copyright (c) 2000,2001,2008  Fred M. Erickson
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
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include "drawmap.h"
#include "dem.h"



/*
 * This routine parses relevant data from a DEM file type A record
 * and inserts the converted data into the given storage structure.
 */
void
parse_dem_a(char *buf, struct dem_record_type_a *dem_a, struct datum *dem_datum)
{
	int32_t i;
	unsigned char save_byte;

	/*
	 * Parse all of the data from the header that we care about.
	 * For now, don't waste time parsing things that aren't
	 * currently interesting.
	 * Since it is possible for numbers to butt together at field
	 * edges, we do the little save_byte thing to ensure that we
	 * don't convert two at a time.
	 *
	 * There are a lot of comments in dem.h describing the various
	 * header fields, so this block of code is presented largely
	 * sans comments.
	 */
	strncpy(dem_a->title, buf, 80);
	save_byte = buf[150]; buf[150] = '\0'; dem_a->level_code =  strtol(&buf[144], (char **)0, 10); buf[150] = save_byte;
	save_byte = buf[162]; buf[162] = '\0'; dem_a->plane_ref =   strtol(&buf[156], (char **)0, 10); buf[162] = save_byte;
	save_byte = buf[168]; buf[168] = '\0'; dem_a->zone =        strtol(&buf[162], (char **)0, 10); buf[168] = save_byte;
	save_byte = buf[534]; buf[534] = '\0'; dem_a->plane_units = strtol(&buf[528], (char **)0, 10); buf[534] = save_byte;
	save_byte = buf[540]; buf[540] = '\0'; dem_a->elev_units =  strtol(&buf[534], (char **)0, 10); buf[540] = save_byte;
	for (i = 546; i < 786; i++)  {
		/* The DEM files use both 'D' and 'E' for exponentiation.  strtod() expects 'E' or 'e'. */
		if (buf[i] == 'D') buf[i] = 'E';
	}
	save_byte = buf[570]; buf[570] = '\0'; dem_a->sw_x_gp = strtod(&buf[546], (char **)0); buf[570] = save_byte;
	save_byte = buf[594]; buf[594] = '\0'; dem_a->sw_y_gp = strtod(&buf[570], (char **)0); buf[594] = save_byte;
	save_byte = buf[618]; buf[618] = '\0'; dem_a->nw_x_gp = strtod(&buf[594], (char **)0); buf[618] = save_byte;
	save_byte = buf[642]; buf[642] = '\0'; dem_a->nw_y_gp = strtod(&buf[618], (char **)0); buf[642] = save_byte;
	save_byte = buf[666]; buf[666] = '\0'; dem_a->ne_x_gp = strtod(&buf[642], (char **)0); buf[666] = save_byte;
	save_byte = buf[690]; buf[690] = '\0'; dem_a->ne_y_gp = strtod(&buf[666], (char **)0); buf[690] = save_byte;
	save_byte = buf[714]; buf[714] = '\0'; dem_a->se_x_gp = strtod(&buf[690], (char **)0); buf[714] = save_byte;
	save_byte = buf[738]; buf[738] = '\0'; dem_a->se_y_gp = strtod(&buf[714], (char **)0); buf[738] = save_byte;
	save_byte = buf[762]; buf[762] = '\0'; dem_a->min_elev = strtod(&buf[738], (char **)0); buf[762] = save_byte;
	save_byte = buf[786]; buf[786] = '\0'; dem_a->max_elev = strtod(&buf[762], (char **)0); buf[786] = save_byte;
	save_byte = buf[810]; buf[810] = '\0'; dem_a->angle = strtod(&buf[786], (char **)0); buf[810] = save_byte;
	save_byte = buf[816]; buf[816] = '\0'; dem_a->accuracy = strtol(&buf[810], (char **)0, 10); buf[816] = save_byte;
	for (i = 816; i < 852; i++)  {
		/* The DEM files use both 'D' and 'E' for exponentiation.  strtod() expects 'E' or 'e'. */
		if (buf[i] == 'D') buf[i] = 'E';
	}
	save_byte = buf[828]; buf[828] = '\0'; dem_a->x_res = drawmap_round(strtod(&buf[816], (char **)0)); buf[828] = save_byte;
	save_byte = buf[840]; buf[840] = '\0'; dem_a->y_res = drawmap_round(strtod(&buf[828], (char **)0)); buf[840] = save_byte;
	save_byte = buf[852]; buf[852] = '\0'; dem_a->z_res = drawmap_round(strtod(&buf[840], (char **)0)); buf[852] = save_byte;
	save_byte = buf[858]; buf[858] = '\0'; dem_a->rows = strtol(&buf[852], (char **)0, 10); buf[858] = save_byte;
	save_byte = buf[864]; buf[864] = '\0'; dem_a->cols = strtol(&buf[858], (char **)0, 10); buf[864] = save_byte;
	/*
	 * The following element is only present in the new Type A format.
	 * We thus need to check for its presence rather than just doing the conversion.
	 */
	if (buf[891] == ' ')  {
		/* There is no entry, so we probably have an old-style record. */
		dem_a->horizontal_datum = -1;
	}
	else  {
		/* There is an entry, so we must have a new-style record. */
		save_byte = buf[892]; buf[892] = '\0'; dem_a->horizontal_datum = strtol(&buf[890], (char **)0, 10); buf[892] = save_byte;
	}
	if ((dem_a->horizontal_datum == -1) || (dem_a->horizontal_datum == 1))  {
		/* The datum is NAD-27.  Initialize the parameters. */
		dem_datum->a = NAD27_SEMIMAJOR; dem_datum->b = NAD27_SEMIMINOR;
		dem_datum->e_2 = NAD27_E_SQUARED;
		dem_datum->f_inv = NAD27_F_INV;
		dem_datum->k0 = UTM_K0;
		dem_datum->a0 = NAD27_A0;
		dem_datum->a2 = NAD27_A2;
		dem_datum->a4 = NAD27_A4;
		dem_datum->a6 = NAD27_A6;
	}
	else if (dem_a->horizontal_datum == 3)  {
		/* The datum is WGS-84.  Initialize the parameters. */
		dem_datum->a = WGS84_SEMIMAJOR;
		dem_datum->b = WGS84_SEMIMINOR;
		dem_datum->e_2 = WGS84_E_SQUARED;
		dem_datum->f_inv = WGS84_F_INV;
		dem_datum->k0 = UTM_K0;
		dem_datum->a0 = WGS84_A0;
		dem_datum->a2 = WGS84_A2;
		dem_datum->a4 = WGS84_A4;
		dem_datum->a6 = WGS84_A6;
	}
	else if (dem_a->horizontal_datum == 4)  {
		/* The datum is NAD-83.  Initialize the parameters. */
		dem_datum->a = NAD83_SEMIMAJOR;
		dem_datum->b = NAD83_SEMIMINOR;
		dem_datum->e_2 = NAD83_E_SQUARED;
		dem_datum->f_inv = NAD83_F_INV;
		dem_datum->k0 = UTM_K0;
		dem_datum->a0 = NAD83_A0;
		dem_datum->a2 = NAD83_A2;
		dem_datum->a4 = NAD83_A4;
		dem_datum->a6 = NAD83_A6;
	}
	else  {
		/* We don't handle any other datums yet.  Default to NAD-27. */
		dem_datum->a = NAD27_SEMIMAJOR;
		dem_datum->b = NAD27_SEMIMINOR;
		dem_datum->e_2 = NAD27_E_SQUARED;
		dem_datum->f_inv = NAD27_F_INV;
		dem_datum->k0 = UTM_K0;
		dem_datum->a0 = NAD27_A0;
		dem_datum->a2 = NAD27_A2;
		dem_datum->a4 = NAD27_A4;
		dem_datum->a6 = NAD27_A6;

		fprintf(stderr, "Warning:  The DEM data aren't in a horizontal datum that drawmap\n");
		fprintf(stderr, "knows about.  Defaulting to NAD-27.  This may result in\npositional errors in the map.\n");
	}

	/*
	 * A few fields are only used for SDTS.  Initialize them
	 * to zero.
	 */
	dem_a->x_gp_first = 0.0;
	dem_a->y_gp_first = 0.0;
	dem_a->void_fill = 0;
	dem_a->edge_fill = 0;

	return;
}



/*
 * Print out relevant fields from a DEM type A record, for debugging purposes.
 * This version prints out only a limited selection of the fields in a Type A header.
 */
void
print_dem_a(struct dem_record_type_a *dem_a)
{
	fprintf(stderr, "DEM Type A Record:\n");
	fprintf(stderr, "  Title       = %.80s\n", dem_a->title);
	fprintf(stderr, "  level_code  = %d\n", dem_a->level_code);
	fprintf(stderr, "  plane_ref   = %d\n", dem_a->plane_ref);
	fprintf(stderr, "  zone        = %d\n", dem_a->zone);
	fprintf(stderr, "  plane_units = %d\n", dem_a->plane_units);
	fprintf(stderr, "  elev_units  = %d\n", dem_a->elev_units);
	fprintf(stderr, "  sw_x_gp     = %g\n", dem_a->sw_x_gp);
	fprintf(stderr, "  sw_y_gp     = %g\n", dem_a->sw_y_gp);
	fprintf(stderr, "  nw_x_gp     = %g\n", dem_a->nw_x_gp);
	fprintf(stderr, "  nw_y_gp     = %g\n", dem_a->nw_y_gp);
	fprintf(stderr, "  ne_x_gp     = %g\n", dem_a->ne_x_gp);
	fprintf(stderr, "  ne_y_gp     = %g\n", dem_a->ne_y_gp);
	fprintf(stderr, "  se_x_gp     = %g\n", dem_a->se_x_gp);
	fprintf(stderr, "  se_y_gp     = %g\n", dem_a->se_y_gp);
	fprintf(stderr, "  min_elev    = %d\n", dem_a->min_elev);
	fprintf(stderr, "  max_elev    = %d\n", dem_a->max_elev);
	fprintf(stderr, "  angle       = %g\n", dem_a->angle);
	fprintf(stderr, "  accuracy    = %d\n", dem_a->accuracy);
	fprintf(stderr, "  x_res       = %g\n", dem_a->x_res);
	fprintf(stderr, "  y_res       = %g\n", dem_a->y_res);
	fprintf(stderr, "  z_res       = %g\n", dem_a->z_res);
	fprintf(stderr, "  cols        = %d\n", dem_a->cols);
	fprintf(stderr, "  rows        = %d\n", dem_a->rows);
	fprintf(stderr, "  horiz_datum = %d\n", dem_a->horizontal_datum);
	fprintf(stderr, "  x_gp_first  = %g\n", dem_a->x_gp_first);
	fprintf(stderr, "  y_gp_first  = %g\n", dem_a->y_gp_first);
	fprintf(stderr, "  void_fill   = %d\n", dem_a->void_fill);
	fprintf(stderr, "  edge_fill   = %d\n", dem_a->edge_fill);

	return;
}




/*
 * Print out all relevant fields from a DEM type A record and a DEM type C record.
 * This version of print_dem() prints out pretty much everything
 * in both structures, except for some of the fields from the newer format.
 */
void
print_dem_a_c(struct dem_record_type_a *dem_a, struct dem_record_type_c *dem_c)
{
	fprintf(stdout, "DEM Type A Record:\n");
	fprintf(stdout, "  title                   = %.40s\n", dem_a->title);
	fprintf(stdout, "  se_latitude             = %g\n", dem_a->se_lat);
	fprintf(stdout, "  se_longitude            = %g\n", dem_a->se_long);
	fprintf(stdout, "  process_code            = %d\n", dem_a->process_code);
	fprintf(stdout, "  origin_code             = %4.4s\n", dem_a->origin_code);
	fprintf(stdout, "  level_code              = %d\n", dem_a->level_code);
	fprintf(stdout, "  elevation_pattern       = %d\n", dem_a->elevation_pattern);
	fprintf(stdout, "  plane_ref               = %d\n", dem_a->plane_ref);
	fprintf(stdout, "  zone                    = %d\n", dem_a->zone);
	fprintf(stdout, "  plane_units             = %d\n", dem_a->plane_units);
	fprintf(stdout, "  elev_units              = %d\n", dem_a->elev_units);
	fprintf(stdout, "  sw_x_gp                 = %g\n", dem_a->sw_x_gp);
	fprintf(stdout, "  sw_y_gp                 = %g\n", dem_a->sw_y_gp);
	fprintf(stdout, "  nw_x_gp                 = %g\n", dem_a->nw_x_gp);
	fprintf(stdout, "  nw_y_gp                 = %g\n", dem_a->nw_y_gp);
	fprintf(stdout, "  ne_x_gp                 = %g\n", dem_a->ne_x_gp);
	fprintf(stdout, "  ne_y_gp                 = %g\n", dem_a->ne_y_gp);
	fprintf(stdout, "  se_x_gp                 = %g\n", dem_a->se_x_gp);
	fprintf(stdout, "  se_y_gp                 = %g\n", dem_a->se_y_gp);
	fprintf(stdout, "  min_elev                = %d\n", dem_a->min_elev);
	fprintf(stdout, "  max_elev                = %d\n", dem_a->max_elev);
	fprintf(stdout, "  angle                   = %g\n", 0.0);
	fprintf(stdout, "  accuracy                = %d\n", dem_a->accuracy);
	fprintf(stdout, "  x_res                   = %g\n", dem_a->x_res);
	fprintf(stdout, "  y_res                   = %g\n", dem_a->y_res);
	fprintf(stdout, "  z_res                   = %g\n", dem_a->z_res);
	fprintf(stdout, "  cols                    = %d  (This value is set to 1 in the main header.)\n", dem_a->cols);
	fprintf(stdout, "  rows                    = %d\n", dem_a->rows);
	fprintf(stdout, "  vertical_datum          = %d\n", dem_a->vertical_datum);
	fprintf(stdout, "  horizontal_datum        = %d\n", dem_a->horizontal_datum);
	fprintf(stdout, "  vertical_datum_shift    = %g\n", dem_a->vertical_datum_shift);
	fprintf(stdout, "Other useful information, not in DEM Type A Record:\n");
	fprintf(stdout, "  UTM x, NW corner sample = %g\n", dem_a->x_gp_first);
	fprintf(stdout, "  UTM y, NW corner sample = %g\n", dem_a->y_gp_first);
	fprintf(stdout, "  edge_fill               = %d\n", dem_a->edge_fill);
	fprintf(stdout, "  void_fill               = %d\n", dem_a->void_fill);
	fprintf(stdout, "DEM Type C Record:\n");
	fprintf(stdout, "  datum_stats_flag        = %d\n", dem_c->datum_stats_flag);
	fprintf(stdout, "  datum_rmse_x            = %d\n", dem_c->datum_rmse_x);
	fprintf(stdout, "  datum_rmse_y            = %d\n", dem_c->datum_rmse_y);
	fprintf(stdout, "  datum_rmse_z            = %d\n", dem_c->datum_rmse_z);
	fprintf(stdout, "  datum_sample_size       = %d\n", dem_c->datum_sample_size);
	fprintf(stdout, "  dem_stats_flag          = %d\n", dem_c->dem_stats_flag);
	fprintf(stdout, "  dem_rmse_x              = %d\n", dem_c->dem_rmse_x);
	fprintf(stdout, "  dem_rmse_y              = %d\n", dem_c->dem_rmse_y);
	fprintf(stdout, "  dem_rmse_z              = %d\n", dem_c->dem_rmse_z);
	fprintf(stdout, "  dem_sample_size         = %d\n", dem_c->dem_sample_size);

	return;
}



/*
 * Process a DEM file that uses the Geographic Planimetric Reference System.
 * These include 30-minute, 1-degree, and Alaska DEMs.  (The routine is so
 * far untested with 30-minute DEMS, so they may not work.)
 *
 * Note:  All 250K DEM files represent 1-degree by 1-degree blocks
 * (as far as I know), including Alaska DEMs.  (The Alaska DEMs
 * have smaller numbers of longitude samples, but still cover
 * 1-degree each.)
 *
 * Note that this function has a side effect:  it converts the
 * latitude/longitude code in dem_a->title into blanks.  This is done
 * so that the latitude/longitude code won't be included as part of
 * the DEM name when we capture the DEM name a few lines hence.
 * The routine has the additional side effect of setting
 * dem_a->zone to a valid value.  The zone field in the header
 * is zero for Geographic DEMs.
 *
 * This function returns 0 if it allocates memory and reads in the data.
 * It returns 1 if it doesn't allocate memory.
 */
int
process_geo_dem(int dem_fdesc, ssize_t (*read_function)(), struct image_corners *image_corners,
		struct dem_corners *dem_corners, struct dem_record_type_a *dem_a, struct datum *dem_datum)
{
	int32_t i, j;
	double f, g;
	char e_w_code;
	char latitude_code;
	int32_t location_code;
	char ll_code[8];
	char *ptr;
	char buf[8 * DEM_RECORD_LENGTH];
	ssize_t ret_val;
	int32_t interp_size;
	int32_t dem_size_x, dem_size_y;


	/*
	 * Apparently, when the USGS digitized the 250K DEM data, they didn't decide
	 * on a consistent format for the file headers.  Some files have the
	 * latitude/longitude code (described in a later comment) at byte 49
	 * in the header, some files have it right at the beginning of the header,
	 * and others may have it somewhere else (although I have no examples of
	 * such at this time).
	 *
	 * It appears that the code always appears in the first 144-bytes,
	 * so we will just search the whole thing for something that looks like the
	 * code.  This is a pain, but it should be reliable.
	 *
	 * We go all the way to the end of the field, even after finding a code,
	 * because we want to null it out and it might appear twice.
	 */
	for (i = 0; i < 137; i++)  {
		if ((dem_a->title[i] != 'N') && (dem_a->title[i] != 'S'))  {
			continue;
		}

		if ((dem_a->title[i + 1] < 'A') || (dem_a->title[i + 1] > 'Z') ||
		    (dem_a->title[i + 2] < '0') || (dem_a->title[i + 2] > '9') ||
		    (dem_a->title[i + 3] < '0') || (dem_a->title[i + 3] > '9') ||
		    (dem_a->title[i + 4] != '-')  ||
		    (dem_a->title[i + 5] < '0') || (dem_a->title[i + 5] > '9') ||
		    (dem_a->title[i + 6] < '0') || (dem_a->title[i + 6] > '9') ||
		    ((dem_a->title[i + 7] != 'E') && (dem_a->title[i + 7] != 'W')))  {
			continue;
		}

		strncpy(ll_code, &dem_a->title[i], 8);
		strncpy(&dem_a->title[i], "        ", 8);
	}

	/*
	 * The longitude/latitude code in a DEM file is cryptic, and apparently is the
	 * name of a corresponding paper map sheet.  It basically encodes the description
	 * of a UTM grid.  It takes a form that is illustrated by the following example:
	 *
	 *        NL12-08W
	 *
	 * where I think the N simply means "Northern Hemisphere".
	 * The L12 is a code that gives a 4 degree by 6 degree block.
	 * Starting at the equator, with 'A', the letter represents 4 degree
	 * chunks of latitude.  Thus 'L' represents the block from 44N to 48N.
	 * The 12 is the UTM zone number.  The calculation "-186 + (6 * zone)"
	 * gives the lower longitude of a 6 degree zone.  Thus, zone 12 represents
	 * longitudes from -114 to -108 (108W to 114W).
	 *
	 * The 4 degree by 6 degree block is divided into 12 rectangular areas,
	 * each of which contains an east and west chunk.  (The W tells us that
	 * this is the western 1 degree by 1 degree block.)  The areas are numbered
	 * as follows:
	 *
	 *       1       2        3
	 *       4       5        6
	 *       7       8        9
	 *      10      11       12
	 *
	 * Area 1 defines the highest-latitude, highest-longitude
	 * block which, in this case, spans 47N-48N and 112W-114W.
	 * Area 12 defines the lowest-latitude, lowest-longitude
	 * block which, in this case, spans 44N-46N and 108W-110W.
	 * For our specific example code, area 08 is at 45N-46N and 110W-112W,
	 * so NL12-08W is at 45N-46N and 111W-112W.
	 *
	 * I don't know if there is an equivalent code for the
	 * southern hemisphere.  We assume here that there is, and
	 * that it is a mirror image of the northern hemisphere code.
	 */
	latitude_code = ll_code[1];
	dem_a->zone = strtol(&ll_code[2], (char **)0, 10);
	location_code = strtol(&ll_code[5], (char **)0, 10);
	e_w_code = ll_code[7];
	dem_corners->sw_lat = (double)((latitude_code - 'A') * 4);
	dem_corners->sw_long = -186.0 + (double)(dem_a->zone * 6);
	i = (location_code - 1) / 3;
	j = (location_code + 2) % 3;
	dem_corners->sw_lat = dem_corners->sw_lat + 3.0 - (double)i;
	dem_corners->sw_long = dem_corners->sw_long + (double)j * 2.0 + (e_w_code == 'W' ? 0.0 : 1.0);
	if (ll_code[0] == 'S')  {
		dem_corners->sw_lat = -dem_corners->sw_lat;
	}
	dem_corners->ne_lat = dem_corners->sw_lat + 1.0;
	dem_corners->ne_long = dem_corners->sw_long + 1.0;
	dem_corners->nw_lat = dem_corners->ne_lat;
	dem_corners->nw_long = dem_corners->sw_long;
	dem_corners->se_lat = dem_corners->sw_lat;
	dem_corners->se_long = dem_corners->ne_long;

	dem_corners->x = ONE_DEGREE_DEM_SIZE;
	dem_corners->y = ONE_DEGREE_DEM_SIZE;


	/*
	 * If the DEM data don't overlap the image, then ignore them.
	 *
	 * If the user didn't specify latitude/longitude ranges for the image,
	 * then we simply use this DEM to determine those boundaries.  In this
	 * latter case, no overlap check is necessary (or possible) since the
	 * image boundaries will be determined later.
	 */
	if (image_corners->sw_lat < image_corners->ne_lat)  {
		/* The user has specified image boundaries.  Check for overlap. */
		if ((dem_corners->sw_lat >= image_corners->ne_lat) || ((dem_corners->sw_lat + 1.0) <= image_corners->sw_lat) ||
		    (dem_corners->sw_long >= image_corners->ne_long) || ((dem_corners->sw_long + 1.0) <= image_corners->sw_long))  {
			return 1;
		}
	}

	/*
	 * Get the number of profiles in the data set.
	 * For all states in the USA, except Alaska, this value should be 1201.
	 * In Alaska, it can be 401 or 601.
	 *
	 * We use the number of profiles to calculate an interpolation step size,
	 * which will be used to interpolate to fill out the dataset to 1201 by 1201 samples.
	 */
	dem_size_x = dem_a->cols;
	if ((dem_size_x != 401) && (dem_size_x != 601) && (dem_size_x != 1201))  {
		fprintf(stderr, "Unexpected number of south-north profiles in DEM data: %d\n", dem_size_x);
		exit(0);
	}
	interp_size = (ONE_DEGREE_DEM_SIZE - 1) / (dem_size_x - 1);

	/*
	 * Read in the entire DEM file into dem_corners->ptr.
	 *
	 * Each record we read is a south-to-north slice of the DEM block.  Successive records move from
	 * west to east.  Thus, we read each profiles into a one-dimensional array, and then copy it
	 * into the desired two-dimensional storage area, simulaneously rotating the data so that north
	 * is at row zero and west is at column zero.
	 */
	dem_size_y = -1;
	for (i = 0; i < ONE_DEGREE_DEM_SIZE; i = i + interp_size)  {
		if ((ret_val = read_function(dem_fdesc, buf, 8 * DEM_RECORD_LENGTH)) < (DEM_RECORD_LENGTH - 4))  {
			fprintf(stderr, "read from DEM file returns %d\n", (int)ret_val);
			exit(0);
		}
		if ((buf[ret_val - 1] == '\n') || (buf[ret_val - 1] == '\r')) ret_val--;
		if ((buf[ret_val - 1] == '\n') || (buf[ret_val - 1] == '\r')) ret_val--;

		if (dem_size_y < 0)  {
			dem_size_y = strtol(&buf[12], (char **)0, 10);
			if (dem_size_y != ONE_DEGREE_DEM_SIZE)  {
				fprintf(stderr, "Number of rows in DEM file is %d, and should be %d.\n", dem_size_y, ONE_DEGREE_DEM_SIZE);
				exit(0);
			}

			/*
			 * Need to allocate space to store the DEM data.
			 * This space must be freed by the calling function.
			 */
			dem_corners->ptr = (short *)malloc(sizeof(short) * ONE_DEGREE_DEM_SIZE * ONE_DEGREE_DEM_SIZE);
			if (dem_corners->ptr == (short *)0)  {
				fprintf(stderr, "malloc of dem_corners->ptr failed\n");
				exit(0);
			}
		}

		ptr = &buf[144];	/* Ignore header information on each block */

		for (j = ONE_DEGREE_DEM_SIZE - 1; j >=0; j--)  {
			if ((ptr - buf) > (ret_val - 6))  {
				/* We are out of data.  Read some more. */
				if ((ret_val = read_function(dem_fdesc, buf, 8 * DEM_RECORD_LENGTH)) < (DEM_RECORD_LENGTH - 4))  {
					fprintf(stderr, "2 read from DEM file returns %d\n", (int)ret_val);
					exit(0);
				}
				if ((buf[ret_val - 1] == '\n') || (buf[ret_val - 1] == '\r')) ret_val--;
				if ((buf[ret_val - 1] == '\n') || (buf[ret_val - 1] == '\r')) ret_val--;
				ptr = buf;
			}

			*(dem_corners->ptr + j * ONE_DEGREE_DEM_SIZE + i) = strtol((char *)ptr, (char **)&ptr, 10);
			if (dem_a->elev_units == 1)  {
				/*
				 * The main body of drawmap likes to work in meters.
				 * We satisfy that desire by changing feet into meters
				 * before passing the data back.  (I think that all GEO
				 * format DEMs are in meters, but we do the check, just
				 * to be sure.)
				 *
				 * We alter the header information below, after all data
				 * points have been processed.
				 */
				*(dem_corners->ptr + j * ONE_DEGREE_DEM_SIZE + i) =
					(short)drawmap_round((double)*(dem_corners->ptr + j * ONE_DEGREE_DEM_SIZE + i) * 0.3048);
			}

			/*
			 * If there are less than 1201 south-north profiles, then interpolate to form
			 * a full 1201x1201 dataset.  That way the program only needs to handle one
			 * dataset size, and things are a lot easier.
			 */
			if ((interp_size > 1) && (i > 0))  {
				if (interp_size == 2)  {
					*(dem_corners->ptr + j * ONE_DEGREE_DEM_SIZE + i - 1) = drawmap_round(0.5 * (double)(*(dem_corners->ptr + j * ONE_DEGREE_DEM_SIZE + i) +
									       *(dem_corners->ptr + j * ONE_DEGREE_DEM_SIZE + i - 2)));
				}
				else  {
					f = (double)(*(dem_corners->ptr + j * ONE_DEGREE_DEM_SIZE + i) -
						     *(dem_corners->ptr + j * ONE_DEGREE_DEM_SIZE + i - 3)) / 3.0;
					g = (double)*(dem_corners->ptr + j * ONE_DEGREE_DEM_SIZE + i - 3);
					*(dem_corners->ptr + j * ONE_DEGREE_DEM_SIZE + i - 2) = drawmap_round(g + f);
					*(dem_corners->ptr + j * ONE_DEGREE_DEM_SIZE + i - 1) = drawmap_round(g + f + f);
				}
			}
		}
	}

	/*
	 * The main body of drawmap likes to work in meters.
	 * We satisfy that desire by changing feet into meters
	 * before passing the data back.
	 *
	 * Here we change the header information.  We already changed the
	 * actual elevation data above.
	 */
	if (dem_a->elev_units == 1)  {
		dem_a->elev_units = 2;
	}

	return 0;
}



/*
 * Process a DEM file that uses the UTM Planimetric Reference System.
 * These include 7.5-minute DEMs.
 *
 * This function returns 0 if it allocates memory and reads in the data.
 * It returns 1 if it doesn't allocate memory.
 */
int
process_utm_dem(int dem_fdesc, ssize_t (*read_function)(), struct image_corners *image_corners,
		struct dem_corners *dem_corners, struct dem_record_type_a *dem_a, struct datum *dem_datum)
{
	int32_t i, j, k;
	double f, g;
	int32_t x = 100000000, y = 100000000;	// bogus initializers to expose errors.
	short *sptr;
	char buf[DEM_RECORD_LENGTH];
	ssize_t ret_val;
	int32_t profile_rows, profile_columns;
	int32_t dem_size_x, dem_size_y;
	double lat_min, long_min;
	double lat_max, long_max;
	double x_gp_min, y_gp_min;
	double x_gp_max, y_gp_max;
	double x_gp, y_gp;
	int32_t longest_profile = -1;
	int32_t easternmost_full_profile = 100000000;	// bogus initializer to expose errors.
	unsigned char save_byte;
	struct profile  {
		double x_gp;
		double y_gp;
		int32_t num_samples;
		short *data;
	} *profiles;


	/*
	 * Drawmap doesn't contain code to handle anything other than south-to-north
	 * profiles.  As far as I know, none of the data currently use anything else.
	 * Check, just to be sure.
	 */
	if (dem_a->angle != 0.0)  {
		fprintf(stderr, "DEM data oriented at a non-zero angle.  Ignoring file.\n");
		return 1;
	}


	/*
	 * Make sure that the UTM zone information isn't bogus.
	 */
	if ((dem_a->zone < 1) || (dem_a->zone > 60))  {
		fprintf(stderr, "DEM file contains a bad UTM zone (%d).  File ignored.\n", dem_a->zone);
		return 1;
	}


	/*
	 * We need to find the location of the first elevation sample in the first
	 * profile.  This procedure is laid out in detail in the DEM standards documents,
	 * complete with nice pictures of the geometry, so I won't describe all of the
	 * details here.  Basically, though, the samples are at UTM coordinates that are
	 * evenly divisible by the 30-meter sample spacing (or divisible by 10 meters if
	 * the sample spacing is 10 meters).  We need to find the first set of coordinates
	 * that have round-numbered values just inside the SW corner.  The procedure varies
	 * depending on whether the data block is west or east of the central meridian.
	 *
	 * Actually, we don't need to do this the hard way, since each profile header
	 * contains the starting UTM coordinates of the profile.  However, the method is
	 * worth encapulating here in case we need to do something like it later.  The
	 * method comes straight from the DEM standards documents.
	 */
//	if ((0.5 * (dem_a->sw_x + dem_a->se_x)) < 500000.0)  {
//		/* West of central meridian. */
//		sw_x = dem_a->x_res * ceil(dem_a->sw_x / dem_a->x_res);
//		m = (dem_a->se_y - dem_a->sw_y) / (dem_a->se_x - dem_a->sw_x);
//		b = dem_a->sw_y - m * dem_a->sw_x;
//		sw_y = dem_a->y_res * ceil((b + m * sw_x) / dem_a->y_res);
//	}
//	else  {
//		/* East of central meridian. */
//		sw_x = dem_a->x_res * ceil(dem_a->nw_x / dem_a->x_res);
//		m = (dem_a->nw_y - dem_a->sw_y) / (dem_a->nw_x - dem_a->sw_x);
//		b = dem_a->sw_y - m * dem_a->sw_x;
//		sw_y = dem_a->y_res * ceil((b + m * sw_x) / dem_a->y_res);
//	}


	/*
	 * Convert UTM coordinates of corners into latitude/longitude pairs.
	 */
	(void)redfearn_inverse(dem_datum, dem_a->sw_x_gp, dem_a->sw_y_gp, dem_a->zone, &(dem_corners->sw_lat), &(dem_corners->sw_long));
	(void)redfearn_inverse(dem_datum, dem_a->nw_x_gp, dem_a->nw_y_gp, dem_a->zone, &(dem_corners->nw_lat), &(dem_corners->nw_long));
	(void)redfearn_inverse(dem_datum, dem_a->ne_x_gp, dem_a->ne_y_gp, dem_a->zone, &(dem_corners->ne_lat), &(dem_corners->ne_long));
	(void)redfearn_inverse(dem_datum, dem_a->se_x_gp, dem_a->se_y_gp, dem_a->zone, &(dem_corners->se_lat), &(dem_corners->se_long));
	dem_corners->sw_x_gp = dem_a->sw_x_gp; dem_corners->sw_y_gp = dem_a->sw_y_gp;
	dem_corners->nw_x_gp = dem_a->nw_x_gp; dem_corners->nw_y_gp = dem_a->nw_y_gp;
	dem_corners->ne_x_gp = dem_a->ne_x_gp; dem_corners->ne_y_gp = dem_a->ne_y_gp;
	dem_corners->se_x_gp = dem_a->se_x_gp; dem_corners->se_y_gp = dem_a->se_y_gp;
	// fprintf(stderr, "%g %g %d      %g %g\n", dem_corners->sw_x_gp, dem_corners->sw_y_gp, dem_a->zone, dem_corners->sw_lat, dem_corners->sw_long);
	// fprintf(stderr, "%g %g %d      %g %g\n", dem_corners->se_x_gp, dem_corners->se_y_gp, dem_a->zone, dem_corners->se_lat, dem_corners->se_long);
	// fprintf(stderr, "%g %g %d      %g %g\n", dem_corners->ne_x_gp, dem_corners->ne_y_gp, dem_a->zone, dem_corners->ne_lat, dem_corners->ne_long);
	// fprintf(stderr, "%g %g %d      %g %g\n", dem_corners->nw_x_gp, dem_corners->nw_y_gp, dem_a->zone, dem_corners->nw_lat, dem_corners->nw_long);

	/*
	 * If the DEM data don't overlap the image, then ignore them.
	 *
	 * If the user didn't specify latitude/longitude ranges for the image,
	 * then we simply use this DEM to determine those boundaries.  In this
	 * latter case, no overlap check is necessary (or possible) since the
	 * image boundaries will be determined later.
	 *
	 * Actually, no overlap check is needed, anyway, since the main routine
	 * will ignore data that is out of bounds.  But we can save a whole
	 * lot of processing if we can detect out-of-bounds data here.
	 *
	 * Because the quads are not rectangular, we might reject a quad that
	 * slightly overlaps the image, but we won't reject one that overlaps
	 * by very much.
	 */
	if (image_corners->sw_lat < image_corners->ne_lat)  {
		/* The user has specified image boundaries.  Check for overlap. */
		if ((dem_corners->sw_lat >= image_corners->ne_lat) || ((dem_corners->ne_lat) <= image_corners->sw_lat) ||
		    (dem_corners->sw_long >= image_corners->ne_long) || ((dem_corners->ne_long) <= image_corners->sw_long))  {
			return 1;
		}
	}

	/*
	 * Elevation samples are stored in south-to-north profiles, that move
	 * from west to east.
	 *
	 * The number of south-to-north profiles is specified in the main Type A record.
	 * We have already parsed the Type A record, so we know this number.
	 * We need to read some Type B records before
	 * we will know the number of elevations in each profile.
	 */
	dem_size_x = dem_a->cols;


	/*
	 * We read in all of the data first.  The data is stored in a list of lists.
	 * We begin by allocating space for a profiles[] array.  Each element of the array is
	 * a structure that contains information about a given profile, including a pointer
	 * to an array containing the actual elevation data for the profile.
	 *
	 * Later, we will convert this information into a two-dimensional array of elevation
	 * data.  We follow this two-step process because the quads aren't, in general,
	 * rectangular; but we would like to pass a rectangular array back to the calling
	 * process for insertion into the image.  By reading in all of the data first, we
	 * accumulate enough information to build an appropriate rectangular array, which
	 * is of the right size to hold every data point, and with the right
	 * parameters to represent the correct geometry.
	 */
	profiles = (struct profile *)malloc(sizeof(struct profile) * dem_size_x);
	if (profiles == (struct profile *)0)  {
		fprintf(stderr, "malloc of *profiles failed\n");
		exit(0);
	}


	/*
	 * The first and/or last profile of elevations are likely to be smaller than the
	 * others because they can intersect the neatline at one end or the other.
	 * (In fact, the first or last profile can actually contain zero elevations if
	 * it intersects the neatlines at a corner of the quad.  The quads take the form
	 * of quadrilaterals, which generally have no two sides of the same length.
	 * The imaginary line segments that bound each quadrilateral are called the neatlines.)
	 *
	 * We have allocated temporary space for the profiles[] array.  Now we loop through
	 * the profiles, allocating space for the actual elevation arrays, and reading in
	 * and converting all profile data.
	 */
	for (i = 0; i < dem_size_x; i++)  {
		/* Read in the first record of the profile.  It contains header information describing the profile. */
		if ((ret_val = read_function(dem_fdesc, buf, DEM_RECORD_LENGTH)) < 144)  {
			fprintf(stderr, "read from DEM file returns %d\n", (int)ret_val);
			exit(0);
		}
		if ((buf[ret_val - 1] == '\n') || (buf[ret_val - 1] == '\r')) ret_val--;
		if ((buf[ret_val - 1] == '\n') || (buf[ret_val - 1] == '\r')) ret_val--;

		/* Parse the relevant header information from the front of the record. */
		save_byte = buf[18]; buf[18] = '\0'; profiles[i].num_samples = strtol(&buf[12], (char **)0, 10); buf[18] = save_byte;
		save_byte = buf[24]; buf[24] = '\0'; profile_columns = strtol(&buf[18], (char **)0, 10); buf[24] = save_byte;
		for (j = 24; j < 72; j++)  {
			/* The DEM files use both 'D' and 'E' for exponentiation.  strtod() expects 'E' or 'e'. */
			if (buf[j] == 'D') buf[j] = 'E';
		}
		save_byte = buf[48]; buf[48] = '\0'; profiles[i].x_gp = strtod(&buf[24], (char **)0); buf[48] = save_byte;
		save_byte = buf[72]; buf[72] = '\0'; profiles[i].y_gp = strtod(&buf[48], (char **)0); buf[72] = save_byte;
		profile_rows = profiles[i].num_samples;
		if (profiles[i].num_samples > longest_profile)  {
			longest_profile = profiles[i].num_samples;
		}

		/*
		 * Drawmap assumes that each profile has 1 column.
		 * As far as I know, all data files have this characteristic, but check just in case.
		 */
		if (profile_columns != 1)  {
			fprintf(stderr, "DEM profile %d has %d columns.  Drawmap cannot parse it.  File ignored.\n", i, profile_columns);
			return 1;
		}

		if (profile_rows <= 0)  {
			profiles[i].data = (short *)0;
			/*
			 * This print statement is included merely because I want to find
			 * example files that have profiles with 0 samples.
			 */
//			fprintf(stderr, "FYI:  Profile %d out of %d has %d rows.\n", i + 1, dem_size_x, profile_rows);
			continue;
		}

		/* Allocate space for the profile elevation data. */
		profiles[i].data = (short *)malloc(sizeof(short) * profile_rows);
		if (profiles[i].data == (short *)0)  {
			fprintf(stderr, "malloc of profiles[%d].data failed\n", i);
			exit(0);
		}

		/* The first record in a profile has 144-bytes of header on the front, so skip over it. */
		k = 144;
		for (j = 0; j < profile_rows; j++)  {
			if ((k > (ret_val - 6)) || (buf[k + 5] == ' '))  {
				/* 
				 * We have run out of data in this record.
				 * We need to read in another one.
				 */
				if ((ret_val = read_function(dem_fdesc, buf, DEM_RECORD_LENGTH)) < 6)  {
					fprintf(stderr, "2 read from DEM file returns %d\n", (int)ret_val);
					exit(0);
				}
				if ((buf[ret_val - 1] == '\n') || (buf[ret_val - 1] == '\r')) ret_val--;
				if ((buf[ret_val - 1] == '\n') || (buf[ret_val - 1] == '\r')) ret_val--;
				k = 0;
			}

			save_byte = buf[k + 6]; buf[k + 6] = '\0'; profiles[i].data[j] = strtol(&buf[k], (char **)0, 10); buf[k + 6] = save_byte;
			k += 6;
		}
	}


	/*
	 * Now we have all of the data stored away in memory.
	 * The next step is to calculate the size of the two-dimensional array
	 * that we need to pass the data back to the calling function.
	 * The dimensions of the array come from the maximum and minimum UTM
	 * coordinates.  The latitude/longitude spanned by the array come
	 * from the maximum and minimum latitude/longitude.
	 *
	 * We check all data points around the edges of the data to find
	 * the largest and smallest latitudes and longitudes, and the largest
	 * and smallest UTM coordinates, x_gp and y_gp.  (We currently make
	 * no use of the latitude/longitude values, but calculate them anyway,
	 * for use in algorithm development and debugging.)
	 *
	 * In general the largest (smallest) UTM coordinate doesn't appear to
	 * necessarily coincide with the largest (smallest) latitude/longitude.
	 * The code that follows contains (commented out) instrumentation
	 * to find the (i, j) coordinates of the largest and smallest dimensions.
	 *
	 * Note that, while we calculate all of this data, for possible future
	 * use, not all of it is currently used.
	 */
	x_gp_min = 1.0e7;
	y_gp_min = 1.0e8;
	x_gp_max = -1.0e7;
	y_gp_max = -1.0e8;
	lat_min = 91.0;
	long_min = 181.0;
	lat_max = -91.0;
	long_max = -181.0;
	for (i = 0; i < dem_size_x; i++)  {
		/* Check profiles along the western edge until we have checked a full-sized one. */
		x_gp = profiles[i].x_gp;
		y_gp = profiles[i].y_gp;
		if (x_gp < x_gp_min)  {
			x_gp_min = x_gp;
//			x_gp_min_loc_i = i; x_gp_min_loc_j = -1;
		}
		if (x_gp > x_gp_max)  {
			x_gp_max = x_gp;
//			x_gp_max_loc_i = i; x_gp_max_loc_j = -1;
		}

		for (j = 0; j < profiles[i].num_samples; j++)  {
			if (y_gp < y_gp_min)  {
				y_gp_min = y_gp;
//				y_gp_min_loc_i = i; y_gp_min_loc_j = j;
			}
			if (y_gp > y_gp_max)  {
				y_gp_max = y_gp;
//				y_gp_max_loc_i = i; y_gp_max_loc_j = j;
			}
			(void)redfearn_inverse(dem_datum, x_gp, y_gp, dem_a->zone, &f, &g);
			if (f < lat_min)  {
				lat_min = f;
//				lat_min_loc_i = i; lat_min_loc_j = j;
			}
			if (f > lat_max)  {
				lat_max = f;
//				lat_max_loc_i = i; lat_max_loc_j = j;
			}
			if (g < long_min)  {
				long_min = g;
//				long_min_loc_i = i; long_min_loc_j = j;
			}
			if (g > long_max)  {
				long_max = g;
//				long_max_loc_i = i; long_max_loc_j = j;
			}

			y_gp += dem_a->y_res;
		}
		if (profiles[i].num_samples == longest_profile)  {
			easternmost_full_profile = i;
			break;
		}
	}
	for ( ; i < dem_size_x; i++)  {
		/* Check the two endpoints of each profile all the way to the eastern edge. */
		if (profiles[i].x_gp < x_gp_min)  {
			x_gp_min = profiles[i].x_gp;
//			x_gp_min_loc_i = i; x_gp_min_loc_j = -1;
		}
		if (profiles[i].x_gp > x_gp_max)  {
			x_gp_max = profiles[i].x_gp;
//			x_gp_max_loc_i = i; x_gp_max_loc_j = -1;
		}
		if (profiles[i].y_gp < y_gp_min)  {
			y_gp_min = profiles[i].y_gp;
//			y_gp_min_loc_i = i; x_gp_min_loc_j = -1;
		}
		if (profiles[i].y_gp > y_gp_max)  {
			y_gp_max = profiles[i].y_gp;
//			y_gp_max_loc_i = i; x_gp_max_loc_j = -1;
		}
		(void)redfearn_inverse(dem_datum, profiles[i].x_gp, profiles[i].y_gp, dem_a->zone, &f, &g);
		if (f < lat_min)  {
			lat_min = f;
//			lat_min_loc_i = i; lat_min_loc_j = 0;
		}
		if (f > lat_max)  {
			lat_max = f;
//			lat_max_loc_i = i; lat_max_loc_j = 0;
		}
		if (g < long_min)  {
			long_min = g;
//			long_min_loc_i = i; long_min_loc_j = 0;
		}
		if (g > long_max)  {
			long_max = g;
//			long_max_loc_i = i; long_max_loc_j = 0;
		}

		y_gp = profiles[i].y_gp + dem_a->y_res * (double)(profiles[i].num_samples - 1);
		if (y_gp < y_gp_min)  {
			y_gp_min = y_gp;
//			y_gp_min_loc_i = i; x_gp_min_loc_j = -1;
		}
		if (y_gp > y_gp_max)  {
			y_gp_max = y_gp;
//			y_gp_max_loc_i = i; x_gp_max_loc_j = -1;
		}
		(void)redfearn_inverse(dem_datum, profiles[i].x_gp, y_gp, dem_a->zone, &f, &g);
		if (f < lat_min)  {
			lat_min = f;
//			lat_min_loc_i = i; lat_min_loc_j = profiles[i].num_samples - 1;
		}
		if (f > lat_max)  {
			lat_max = f;
//			lat_max_loc_i = i; lat_max_loc_j = profiles[i].num_samples - 1;
		}
		if (g < long_min)  {
			long_min = g;
//			long_min_loc_i = i; long_min_loc_j = profiles[i].num_samples - 1;
		}
		if (g > long_max)  {
			long_max = g;
//			long_max_loc_i = i; long_max_loc_j = profiles[i].num_samples - 1;
		}
		if (profiles[i].num_samples == longest_profile)  {
			easternmost_full_profile = i;
		}
	}
	for (i = easternmost_full_profile; i < dem_size_x; i++)  {
		x_gp = profiles[i].x_gp;
		y_gp = profiles[i].y_gp + dem_a->y_res;
		if (x_gp < x_gp_min)  {
			x_gp_min = x_gp;
//			x_gp_min_loc_i = i; x_gp_min_loc_j = -1;
		}
		if (x_gp > x_gp_max)  {
			x_gp_max = x_gp;
//			x_gp_max_loc_i = i; x_gp_max_loc_j = -1;
		}

		/* Check the profiles along the eastern edge. */
		for (j = 1; j < (profiles[i].num_samples - 1); j++)  {
			if (y_gp < y_gp_min)  {
				y_gp_min = y_gp;
//				y_gp_min_loc_i = i; y_gp_min_loc_j = j;
			}
			if (y_gp > y_gp_max)  {
				y_gp_max = y_gp;
//				y_gp_max_loc_i = i; y_gp_max_loc_j = j;
			}
			(void)redfearn_inverse(dem_datum, x_gp, y_gp, dem_a->zone, &f, &g);
			if (f < lat_min)  {
				lat_min = f;
//				lat_min_loc_i = i; lat_min_loc_j = j;
			}
			if (f > lat_max)  {
				lat_max = f;
//				lat_max_loc_i = i; lat_max_loc_j = j;
			}
			if (g < long_min)  {
				long_min = g;
//				long_min_loc_i = i; long_min_loc_j = j;
			}
			if (g > long_max)  {
				long_max = g;
//				long_max_loc_i = i; long_max_loc_j = j;
			}

			y_gp += dem_a->y_res;
		}
	}
//	fprintf(stderr, "\n");
//	fprintf(stderr, "x_gp_min_loc_i = %d      x_gp_min_loc_j = %d\n", x_gp_min_loc_i, x_gp_min_loc_j);
//	fprintf(stderr, "long_min_loc_i = %d      long_min_loc_j = %d\n", long_min_loc_i, long_min_loc_j);
//	fprintf(stderr, "\n");
//	fprintf(stderr, "y_gp_min_loc_i = %d      y_gp_min_loc_j = %d\n", y_gp_min_loc_i, y_gp_min_loc_j);
//	fprintf(stderr, "lat_min_loc_i = %d      lat_min_loc_j = %d\n", lat_min_loc_i, lat_min_loc_j);
//	fprintf(stderr, "\n");
//	fprintf(stderr, "x_gp_max_loc_i = %d      x_gp_max_loc_j = %d\n", x_gp_max_loc_i, x_gp_max_loc_j);
//	fprintf(stderr, "long_max_loc_i = %d      long_max_loc_j = %d\n", long_max_loc_i, long_max_loc_j);
//	fprintf(stderr, "\n");
//	fprintf(stderr, "y_gp_max_loc_i = %d      y_gp_max_loc_j = %d\n", y_gp_max_loc_i, y_gp_max_loc_j);
//	fprintf(stderr, "lat_max_loc_i = %d      lat_max_loc_j = %d\n", lat_max_loc_i, lat_max_loc_j);
//	fprintf(stderr, "\n");

	/*
	 * We now know the extent of the data in terms of both UTM coordinates and
	 * latitude/longitude coordinates.
	 *
	 * This gives us everything we need to set up the data array that will be
	 * returned to the calling function.
	 *
	 * We store the data in an array that is based on the UTM coordinates
	 * since that is the native mode for the data.  The calling function
	 * can decide whether to leave them that way or inverse project them
	 * onto a latitude/longitude grid.
	 */
	if (dem_size_x != drawmap_round(1.0 + (x_gp_max - x_gp_min) / dem_a->x_res))  {
		fprintf(stderr, "Number of profiles (%d) in data does not match actual data extent (%d).  File ignored.\n",
			dem_size_x, drawmap_round(1.0 + (x_gp_max - x_gp_min) / dem_a->x_res));
	}
	dem_size_y = drawmap_round(1.0 + (y_gp_max - y_gp_min) / dem_a->y_res);

	/*
	 * Set up the array to be returned to the calling function.
	 */
	dem_corners->x_gp_min = x_gp_min;
	dem_corners->y_gp_min = y_gp_min;
	dem_corners->x_gp_max = x_gp_max;
	dem_corners->y_gp_max = y_gp_max;
	dem_corners->x = dem_size_x;
	dem_corners->y = dem_size_y;

	//fprintf(stderr, "x=%d    y=%d    x_range=%.8g - %.8g    y_range=%.9g - %.9g    lat_range=%.7g - %.7g    long_range=%.8g - %.8g\n",
	//	dem_size_x, dem_size_y, x_gp_min, x_gp_max, y_gp_min, y_gp_max, lat_min, lat_max, long_min, long_max);

	dem_corners->ptr = (short *)malloc(sizeof(short) * dem_size_x * dem_size_y);
	if (dem_corners->ptr == (short *)0)  {
		fprintf(stderr, "malloc of dem_corners->ptr failed\n");
		exit(0);
	}


	/*
	 * Initialize the new array to HIGHEST_ELEVATION.
	 */
	for (j = 0; j < (dem_size_x * dem_size_y); j++)  {
		(dem_corners->ptr)[j] = HIGHEST_ELEVATION;
	}


	/*
	 * Now that we have a storage array, we need to stuff the parsed elevations from
	 * the various profiles into the appropriate locations in the array.
	 *
	 * The data is stored such that, somewhere in the i==0 column of the array,
	 * is/are the sample(s) with the lowest x_gp value (x_gp_min).  Somewhere in
	 * the bottom row is/are the sample(s) with the lowest y_gp value (y_gp_min).
	 * The rows and columns of the array are spaced by dem_a->y_res and dem_a->x_res,
	 * respectively.  Thus, given the data in dem_corners, and in the array, the calling
	 * function can reconstruct the exact x_gp and y_gp coordinates, and the elevation,
	 * of each data point.  How the calling function maps this data into an image
	 * is none of our concern.  Our job is merely to return the parsed data in a usable form.
	 */
	for (i = 0; i < dem_size_x; i++)  {
		/* The profile doesn't have to begin at the lowest y_gp.  Find the starting offset. */
		k = drawmap_round((profiles[i].y_gp - y_gp_min) / dem_a->y_res);

		for (j = 0; j < profiles[i].num_samples; j++)  {
			sptr = (dem_corners->ptr + (dem_size_y - 1 - j - k) * dem_size_x + i);
			if (*sptr != HIGHEST_ELEVATION)  {
				fprintf(stderr, "FYI:  Overwrite in process_utm_dem at (%d, %d)\n", x, y);
			}
			*sptr = profiles[i].data[j];
			if ((*sptr == 32767) || (*sptr == -32767))  {
				/*
				 * Some non-SDTS DEM files appear to mark non-valid data with
				 * either the value 32767 or -32767.  At least for some files,
				 * these may represent void_fill and/or edge_fill values, but
				 * I don't know for sure.  One thing is certain:  they aren't valid
				 * elevations.  Normally, in drawmap, we convert edge_fill values
				 * into HIGHEST_ELEVATION, and convert void_fill to an elevation
				 * of zero.  However, since it isn't clear what these values
				 * represent, we convert them into HIGHEST_ELEVATION.
				 * At least that way, the rest of drawmap doesn't have to deal
				 * with this quirk.
				 */
				*sptr = HIGHEST_ELEVATION;
			}
			else if (dem_a->elev_units == 1)  {
				/*
				 * The main body of drawmap likes to work in meters.
				 * We satisfy that desire by changing feet into meters
				 * before passing the data back.
				 *
				 * We alter the header information below, after all data
				 * points have been processed.
				 */
				*sptr = (short)drawmap_round((double)*sptr * 0.3048);
			}
		}
		free(profiles[i].data);
	}
	free(profiles);

	/*
	 * The main body of drawmap likes to work in meters.
	 * We satisfy that desire by changing feet into meters
	 * before passing the data back.
	 *
	 * Here we change the header information.  We already changed the
	 * actual elevation data above.
	 */
	if (dem_a->elev_units == 1)  {
		dem_a->elev_units = 2;
	}



// For debugging.
//	for (i = 0; i < dem_size_x; i++)  {
//		for (j = 0; j < dem_size_y; j++)  {
//			if (*(dem_corners->ptr + j * dem_size_x + i) == HIGHEST_ELEVATION)  {
//				fprintf(stderr, "FYI:  HIGHEST_ELEVATION at %d %d\n", i, j);
//			}
//		}
//	}

	return 0;
}
