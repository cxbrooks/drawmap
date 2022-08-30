/*
 * =========================================================================
 * dlg.c - Routines to handle DLG data.
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
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "drawmap.h"
#include "dlg.h"



/*
 * Storage for attribute types.
 */
int32_t num_A_attrib;
int32_t num_L_attrib;
struct maj_min attributes_A[MAX_A_ATTRIB];
struct maj_min attributes_L[MAX_L_ATTRIB];


/*
 * The code that processes DLG files is very spaghetti-like, since
 * it got squeezed and twisted and stretched while I figured out how
 * DLG files are put together.
 *
 * Because of this, and because I don't like to write functions that
 * take 35 arguments, there are a lot of global variables used by the
 * DLG code.  Most of them are accumulated here.
 */

/*
 * The sizes of the nodes, areas, and lines arrays are their theoretical maximum values.
 * It would probably be cooler to malloc() these as we go, but coolness was not an
 * objective of this program.  It would still be cool to read the maximum values from
 * the DLG file headers and check them against the values below to verify that
 * the standards haven't changed and left this program behind.
 */
struct nodes nodes[MAX_NODES];
struct areas areas[MAX_AREAS];
struct lines lines[MAX_LINES];

double lat_se, long_se, lat_sw, long_sw, lat_ne, long_ne, lat_nw, long_nw;
static double grid_x_se, grid_y_se, grid_x_sw, grid_y_sw, grid_x_ne, grid_y_ne, grid_x_nw, grid_y_nw;
int32_t dlg_x_low, dlg_y_low, dlg_x_high, dlg_y_high;
int32_t x_prime;

int32_t utm_zone;

int32_t right_border = RIGHT_BORDER;


/*
 * Process the data from an optional-format DLG file.
 * If you haven't read the DLG file guide and looked at a
 * DLG file, this code will probably be incomprehensible.
 */
void
process_dlg_optional(int fdesc, int gz_flag, struct image_corners *image_corners, int32_t info_flag)
{
	int32_t i, j, ret_val;
	int32_t count;
	int32_t color;
	char *end_ptr;
	char buf[DLG_RECORD_LENGTH + 1];
	char buf2[DLG_RECORD_LENGTH + 1];
	struct point **current_point;
	struct point *tmp_point;
	struct attribute **current_attrib;
	struct attribute *tmp_attrib;
	int32_t attrib;
	int32_t line_list;
	int32_t num_nodes = 0;
	int32_t num_areas = 0;
	int32_t num_lines = 0;
	int32_t data_type = 0;
	ssize_t (* read_function)(int, void *, size_t);
	int32_t plane_ref;
	char save_byte;
	int32_t datum_number;
	struct datum datum;

	x_prime = image_corners->x + LEFT_BORDER + right_border;


	if (gz_flag == 0)  {
		read_function = buf_read;
	}
	else  {
		read_function = buf_read_z;
	}

	/*
	 * Some newer DLG files now come with newlines embedded in them.
	 * (Older files - at least the ones I checked - did not.  They
	 * were one long blob of ASCII text, without any newlines at all.)
	 * In these newer files, the records aren't of fixed length.
	 * Figure out which type of file we have by examining the
	 * first record.  Since the old records were DLG_RECORD_LENGTH bytes long,
	 * we examine up to DLG_RECORD_LENGTH bytes of the first record.  If no newline
	 * is found, we read fixed-length records.  If a newline is found,
	 * then we switch our reading routine to be get_a_line or get_a_line_z,
	 * which read up to a newline and stop.
	 */
	for (i = 0; i < DLG_RECORD_LENGTH; i++)  {
		if ((ret_val = read_function(fdesc, &buf[i], 1)) != 1)  {
			fprintf(stderr, "1 record DLG read returns %d\n", ret_val);
			exit(0);
		}
		/*
		 * We assume here that all files with both '\n' and '\r' at the end
		 * have the '\n' at the end.
		 */
		if (buf[i] == '\n')  {
			if (read_function == buf_read)  {
				read_function = get_a_line;
			}
			else  {
				read_function = get_a_line_z;
			}
			break;
		}
	}
	/* Set ret_val, just in case we need to parse this record some day. */
	ret_val = i;
	if (buf[ret_val - 1] == '\r')  {
		ret_val--;
	}

	/*
	 * There is a lot of information in the file header.  We extract
	 * those items we care about and ignore the rest.
	 * We aren't interested in the first 10 records (for now), so ignore them.
	 * We already read the first record, so continue with the second.
	 */
	if ((ret_val = read_function(fdesc, buf, DLG_RECORD_LENGTH)) <= 0)  {
		fprintf(stderr, "2 record DLG read returns %d\n", ret_val);
		exit(0);
	}
	if (info_flag != 0)  {
		/*
		 * If we are trying to print file information, then find the text that
		 * tells which postal codes (e.g. MT, RI, TX) this DLG file touches.
		 * In the process of doing this, we will also delimit the DLG name,
		 * and can print that out as well.
		 */
		buf[ret_val] = '\0'; 
		for (i = 0; i < ret_val; i++)  { 
			if (buf[i] == ',')  { 
				fprintf(stdout, "\t%.*s", i, buf);	// Print DLG name
				i++;
				for (; i < ret_val; i++)  { 
					if (buf[i] != ' ')  { 
						break;
					}
				}
				break;
			}
		}
		for (j = i + 1; j < ret_val; j++)  { 
			/* Sometimes, postal codes are separated by a space, so check for two spaces. */
			if ((buf[j] == ' ') && (buf[j + 1] == ' '))  {
				buf[j] = '\0'; 
				break;
			}
		}
		fprintf(stdout, "\t%s", &buf[i]);
	}
	if ((ret_val = read_function(fdesc, buf, DLG_RECORD_LENGTH)) <= 0)  {
		fprintf(stderr, "3 record DLG read returns %d\n", ret_val);
		exit(0);
	}
	if ((ret_val = read_function(fdesc, buf, DLG_RECORD_LENGTH)) <= 0)  {
		/*
		 * We are interested in three fields from this record.
		 * Bytes 7-12 (numbered from 1) contain the ground
		 * planimetric reference system.  This should be 1 for
		 * UTM (used with both 24K and 100K files) or 3 for Albers
		 * Conical Equal Area (used for 2M files).
		 * Bytes 13-18 give the zone for the given planimetric
		 * reference system, which for 24K and 100K files is the
		 * UTM zone, and which is set to 9999 for 2M files.
		 * Bytes 67-69 give the horizontal datum for the given planimetric
		 * reference system.  'b' or 0 = NAD 27, 1 = NAD 83, 2 = Puerto Rico,
		 * 3 = Old Hawaiian, 4 = local (astro).
		 */
		fprintf(stderr, "4 record DLG read returns %d\n", ret_val);
		exit(0);
	}
	else  {
		save_byte = buf[12]; buf[12] = '\0'; plane_ref = strtol(&buf[6], &end_ptr, 10); buf[12] = save_byte;
		if (plane_ref != 1)  {
			fprintf(stderr, "DLG file does not use UTM ground planimetric coordinates.\nDrawmap can't handle it.  Exiting.  (Plane_ref = %d)\n", plane_ref);
			exit(0);
		}

		save_byte = buf[18]; buf[18] = '\0'; utm_zone = strtol(&buf[12], &end_ptr, 10); buf[18] = save_byte;
		if ((utm_zone < 1) || (utm_zone > 60))  {
			fprintf(stderr, "DLG file contains bad UTM zone %d.  Drawmap can't handle it.  Exiting.\n", utm_zone);
			exit(0);
		}

		if (ret_val >= 69)  {
			save_byte = buf[69]; buf[69] = '\0'; datum_number = strtol(&buf[66], &end_ptr, 10); buf[69] = save_byte;
		}
		else  {
			datum_number = 0;
		}
		if ((buf[68] == 'b') || (datum_number == 0))  {
			/*
			 * The file uses the NAD-27 datum.
			 * Initialize the datum parameters.
			 */
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
		else if (datum_number == 1)  {
			/*
			 * The file uses the NAD-83 datum.
			 * Initialize the datum parameters.
			 */
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
			/*
			 * We don't currently handle any other datums.
			 * Default to tne NAD-27 datum.
			 */
			datum.a = NAD27_SEMIMAJOR;
			datum.b = NAD27_SEMIMINOR;
			datum.e_2 = NAD27_E_SQUARED;
			datum.f_inv = NAD27_F_INV;
			datum.k0 = UTM_K0;
			datum.a0 = NAD27_A0;
			datum.a2 = NAD27_A2;
			datum.a4 = NAD27_A4;
			datum.a6 = NAD27_A6;

			fprintf(stderr, "DLG file uses a horizontal datum that drawmap doesn't know about.\n");
			fprintf(stderr, "Defaulting to NAD-27.  This may result in positional errors in the map.\n");
		}
	}
	if ((ret_val = read_function(fdesc, buf, DLG_RECORD_LENGTH)) <= 0)  {
		fprintf(stderr, "5 record DLG read returns %d\n", ret_val);
		exit(0);
	}
	if ((ret_val = read_function(fdesc, buf, DLG_RECORD_LENGTH)) <= 0)  {
		fprintf(stderr, "6 record DLG read returns %d\n", ret_val);
		exit(0);
	}
	if ((ret_val = read_function(fdesc, buf, DLG_RECORD_LENGTH)) <= 0)  {
		fprintf(stderr, "7 record DLG read returns %d\n", ret_val);
		exit(0);
	}
	if ((ret_val = read_function(fdesc, buf, DLG_RECORD_LENGTH)) <= 0)  {
		fprintf(stderr, "8 record DLG read returns %d\n", ret_val);
		exit(0);
	}
	if ((ret_val = read_function(fdesc, buf, DLG_RECORD_LENGTH)) <= 0)  {
		fprintf(stderr, "9 record DLG read returns %d\n", ret_val);
		exit(0);
	}
	if ((ret_val = read_function(fdesc, buf, DLG_RECORD_LENGTH)) <= 0)  {
		fprintf(stderr, "10 record DLG read returns %d\n", ret_val);
		exit(0);
	}
	if ((ret_val = read_function(fdesc, buf, DLG_RECORD_LENGTH)) <= 0)  {
		fprintf(stderr, "11 record DLG read returns %d\n", ret_val);
		exit(0);
	}
	else  {
		for (i = 0; i < ret_val; i++)  {
			/* The DLG files use 'D' for exponentiation.  strtod() expects 'E' or 'e'. */
			if (buf[i] == 'D') buf[i] = 'E';
		}
		i = 6;
		lat_sw = strtod(&buf[i], &end_ptr);
		i = i + end_ptr - &buf[i];

		long_sw = strtod(&buf[i], &end_ptr);
		i = i + end_ptr - &buf[i];

		grid_x_sw = strtod(&buf[i], &end_ptr);
		i = i + end_ptr - &buf[i];

		grid_y_sw = strtod(&buf[i], &end_ptr);
	}
	if ((ret_val = read_function(fdesc, buf, DLG_RECORD_LENGTH)) <= 0)  {
		fprintf(stderr, "12 record DLG read returns %d\n", ret_val);
		exit(0);
	}
	else  {
		for (i = 0; i < ret_val; i++)  {
			/* The DLG files use 'D' for exponentiation.  strtod() expects 'E' or 'e'. */
			if (buf[i] == 'D') buf[i] = 'E';
		}
		i = 6;
		lat_nw = strtod(&buf[i], &end_ptr);
		i = i + end_ptr - &buf[i];

		long_nw = strtod(&buf[i], &end_ptr);
		i = i + end_ptr - &buf[i];

		grid_x_nw = strtod(&buf[i], &end_ptr);
		i = i + end_ptr - &buf[i];

		grid_y_nw = strtod(&buf[i], &end_ptr);
	}
	if ((ret_val = read_function(fdesc, buf, DLG_RECORD_LENGTH)) <= 0)  {
		fprintf(stderr, "13 record DLG read returns %d\n", ret_val);
		exit(0);
	}
	else  {
		for (i = 0; i < ret_val; i++)  {
			/* The DLG files use 'D' for exponentiation.  strtod() expects 'E' or 'e'. */
			if (buf[i] == 'D') buf[i] = 'E';
		}
		i = 6;
		lat_ne = strtod(&buf[i], &end_ptr);
		i = i + end_ptr - &buf[i];

		long_ne = strtod(&buf[i], &end_ptr);
		i = i + end_ptr - &buf[i];

		grid_x_ne = strtod(&buf[i], &end_ptr);
		i = i + end_ptr - &buf[i];

		grid_y_ne = strtod(&buf[i], &end_ptr);
	}
	if ((ret_val = read_function(fdesc, buf, DLG_RECORD_LENGTH)) <= 0)  {
		fprintf(stderr, "14 record DLG read returns %d\n", ret_val);
		exit(0);
	}
	else  {
		for (i = 0; i < ret_val; i++)  {
			/* The DLG files use 'D' for exponentiation.  strtod() expects 'E' or 'e'. */
			if (buf[i] == 'D') buf[i] = 'E';
		}
		i = 6;
		lat_se = strtod(&buf[i], &end_ptr);
		i = i + end_ptr - &buf[i];

		long_se = strtod(&buf[i], &end_ptr);
		i = i + end_ptr - &buf[i];

		grid_x_se = strtod(&buf[i], &end_ptr);
		i = i + end_ptr - &buf[i];

		grid_y_se = strtod(&buf[i], &end_ptr);
	}
	if ((ret_val = read_function(fdesc, buf, DLG_RECORD_LENGTH)) <= 0)  {
		fprintf(stderr, "15 record DLG read returns %d\n", ret_val);
		exit(0);
	}
	else  {
		/*
		 * According to the DLG standard, the first four characters of the short-form
		 * theme name from the header are verified.  Thus, it should be okay
		 * to key off the first two characters to find the type of data.
		 */
		switch(buf[0])  {
		case 'B':	/* BOUNDARIES */
			color = GRAY;
			data_type = BOUNDARIES;
			break;
		case 'H':
			if (buf[2] == 'D')  {
				/* HYDROGRAPHY */
				color = B_BLUE;
				data_type = HYDROGRAPHY;
				break;
			}
			else  {
				/* HYPSOGRAPHY */
				color = L_ORANGE;
				data_type = HYPSOGRAPHY;
				break;
			}
		case 'P':
			if (buf[1] == 'I')  {
				/* PIPE & TRANS LINES */
				color = BLACK;
				data_type = PIPE_TRANS_LINES;
				break;
			}
			else  {
				/* PUBLIC LAND SURVEYS */
				color = BLACK;
				data_type = PUBLIC_LAND_SURVEYS;
				break;
			}
		case 'R':
			if (buf[1] == 'A')  {
				/* RAILROADS */
				color = BLACK;
				data_type = RAILROADS;
				break;
			}
			else  {
				/* ROADS AND TRAILS */
				color = B_RED;
				data_type = ROADS_AND_TRAILS;
				break;
			}
		case 'M':	/* MANMADE FEATURES */
			color = BLACK;
			data_type = MANMADE_FEATURES;
			break;
		case 'S':	/* SURVEY CONTROL */
			color = BLACK;
			data_type = SURVEY_CONTROL;
			break;
		case 'V':	/* VEG SURFACE COVER */
			color = B_GREEN;
			data_type = VEG_SURFACE_COVER;
			break;
		case 'N':	/* NON-VEG FEATURES */
			color = BLACK;
			data_type = NON_VEG_FEATURES;
			break;
		default:
			fprintf(stderr, "Unknown record type %20.20s\n", buf);
			exit(0);
			break;
		}
	}

	/* If info_flag is non-zero, then we just want to print some info about the DLG file and return. */
	if (info_flag != 0)  {
		/* Put a null character at the end of the category name (theme). */
		for (i = 19; i >= 0; i--)  {
			if (buf[i] != ' ')  {
				buf[i + 1] = '\0';
				break;
			}
		}
		if (i == -1)  {
			buf[0] = '\0';
		}
		fprintf(stdout, "\t%.20s\t%g:%g:%g:%g\t%s\n", buf, lat_se, long_se, lat_nw, long_nw,
			(read_function == get_a_line || read_function == get_a_line_z) ? "linefeeds=yes" : "linefeeds=no");
		return;
	}


	/*
	 * Within the Optional-format DLG file, locations are specified with pairs of
	 * Universal Transverse Mercator (x,y) coordinates.
	 *
	 * The header information at the top of the DLG file gives 4 reference
	 * points for the corners of the polygon represented by the DLG data.  Here is a
	 * typical set of them:
	 *
	 *	SW       45.750000 -112.000000         422218.03  5066539.80                    
	 *	NW       46.000000 -112.000000         422565.07  5094315.16                    
	 *	NE       46.000000 -111.750000         441923.83  5094103.38                    
	 *	SE       45.750000 -111.750000         441663.14  5066327.07                    
	 *
	 * Note that the latitude-longitude points form a square area in latitude/longitude
	 * space (if latitudes and longitudes on a pseudo-sphere can ever be thought of as
	 * forming a square).  The UTM (x,y) grid coordinates, however, form a quadrilateral
	 * in which no two sides have the same length.  Thus, if we are to convert the grid
	 * points in the DLG file into latitudes and longitudes, we need to develop a general
	 * transformation between these grid points and the desired latitudes and longitudes.
	 *
	 *
	 * Do a quick check here to find out if the data is off the map boundaries.
	 * If so, then we can return now and save a lot of work.
	 */
	if ((lat_sw > image_corners->ne_lat) ||
	    (long_sw > image_corners->ne_long) ||
	    (lat_ne < image_corners->sw_lat) ||
	    (long_ne < image_corners->sw_long))  {
		return;
	}


	/*
	 * Following the DLG header information, there is a sequence of data records for
	 * Nodes, Areas, and Lines.
	 * Parse these data records and put the data into the appropriate arrays.
	 * At present, we make absolutely no use of the Node information, but we parse
	 * and store it anyway.
	 */
	while ((ret_val = read_function(fdesc, buf, DLG_RECORD_LENGTH)) > 0)  {
		switch(buf[0])  {
		case 'N':
			i = 1;
			nodes[num_nodes].id = strtol(&buf[i], &end_ptr, 10);
			i = i + end_ptr - &buf[i];

			nodes[num_nodes].x = strtod(&buf[i], &end_ptr);
			i = i + end_ptr - &buf[i];

			nodes[num_nodes].y = strtod(&buf[i], &end_ptr);

			i = 36;
			line_list = strtol(&buf[i], &end_ptr, 10);

			i = 48;
			attrib = strtol(&buf[i], &end_ptr, 10);

			if (line_list != 0)  {
				while(line_list > 0)  {
					if ((ret_val = read_function(fdesc, buf2, DLG_RECORD_LENGTH)) <= 0)  {
						fprintf(stderr, "Line_list read 1 returns %d\n", ret_val);
						fprintf(stderr, "%80.80s\n", buf);
						exit(0);
					}

					line_list = line_list - 12;
				}
			}

			if (attrib != 0)  {
				while (attrib > 0)  {
					if ((ret_val = read_function(fdesc, buf2, DLG_RECORD_LENGTH)) <= 0)  {
						fprintf(stderr, "Attribute read 1 returns %d\n", ret_val);
						fprintf(stderr, "%80.80s\n", buf);
						exit(0);
					}

					attrib = attrib - 6;
				}
			}

			num_nodes++;
			break;

		case 'A':
			i = 1;
			areas[num_areas].id = strtol(&buf[i], &end_ptr, 10);
			i = i + end_ptr - &buf[i];

			areas[num_areas].x = strtod(&buf[i], &end_ptr);
			i = i + end_ptr - &buf[i];

			areas[num_areas].y = strtod(&buf[i], &end_ptr);

			i = 36;
			line_list = strtol(&buf[i], &end_ptr, 10);

			i = 48;
			attrib = strtol(&buf[i], &end_ptr, 10);
			areas[num_areas].number_attrib = attrib;

			if (line_list != 0)  {
				while (line_list > 0)  {
					if ((ret_val = read_function(fdesc, buf2, DLG_RECORD_LENGTH)) <= 0)  {
						fprintf(stderr, "Line_list read 2 returns %d\n", ret_val);
						fprintf(stderr, "%80.80s\n", buf);
						exit(0);
					}

					line_list = line_list - 12;
				}
			}

			if (attrib != 0)  {
				while (attrib > 0)  {
					if ((ret_val = read_function(fdesc, buf2, DLG_RECORD_LENGTH)) <= 0)  {
						fprintf(stderr, "Attribute read 2 returns %d\n", ret_val);
						fprintf(stderr, "%80.80s\n", buf);
						exit(0);
					}

					current_attrib = &areas[num_areas].attribute;

					if (attrib > 6)  {
						i = 6;
						attrib = attrib - 6;
					}
					else  {
						i = attrib;
						attrib = 0;
					}

					end_ptr = buf2;

					while (i > 0)  {
						*current_attrib = (struct attribute *)malloc(sizeof(struct attribute));
						if (*current_attrib == (struct attribute *)0)  {
							fprintf(stderr, "malloc failed\n");
							exit(0);
						}

						(*current_attrib)->major = strtol(end_ptr, &end_ptr, 10);
						(*current_attrib)->minor = strtol(end_ptr, &end_ptr, 10);

						current_attrib = &((*current_attrib)->attribute);
						i--;
					}
					*current_attrib = (struct attribute *)0;
				}
			}

			num_areas++;
			break;

		case 'L':
			i = 1;
			lines[num_lines].id = strtol(&buf[i], &end_ptr, 10);
			i = i + end_ptr - &buf[i];

			lines[num_lines].start_node = strtol(&buf[i], &end_ptr, 10);
			i = i + end_ptr - &buf[i];

			lines[num_lines].end_node = strtol(&buf[i], &end_ptr, 10);
			i = i + end_ptr - &buf[i];

			lines[num_lines].left_area = strtol(&buf[i], &end_ptr, 10);
			i = i + end_ptr - &buf[i];

			lines[num_lines].right_area = strtol(&buf[i], &end_ptr, 10);

			i = 42;
			lines[num_lines].number_coords = strtol(&buf[i], &end_ptr, 10);
			i = i + end_ptr - &buf[i];

			attrib = strtol(&buf[i], &end_ptr, 10);
			lines[num_lines].number_attrib = attrib;

			current_point = &lines[num_lines].point;
			count = lines[num_lines].number_coords;
			while (count != 0)  {
				if ((ret_val = read_function(fdesc, buf2, DLG_RECORD_LENGTH)) <= 0)  {
					fprintf(stderr, "Coordinate read returns %d\n", ret_val);
					fprintf(stderr, "%80.80s\n", buf);
					exit(0);
				}
				if ((buf2[ret_val - 1] == '\n') || (buf2[ret_val - 1] == '\r'))  {
					ret_val--;
				}
				if ((buf2[ret_val - 1] == '\n') || (buf2[ret_val - 1] == '\r'))  {
					ret_val--;
				}

				i = 0;
				while (i < ret_val)  {
					while ((i < ret_val) && (buf2[i] == ' '))  {
						i++;
					}
					if (i >= ret_val)  {
						break;
					}

					*current_point = (struct point *)malloc(sizeof(struct point));
					if (*current_point == (struct point *)0)  {
						fprintf(stderr, "malloc failed\n");
						exit(0);
					}

					(*current_point)->x = (int32_t)strtod(&buf2[i], &end_ptr);
					i = i + end_ptr - &buf2[i];
					(*current_point)->y = (int32_t)strtod(&buf2[i], &end_ptr);
					i = i + end_ptr - &buf2[i];

					current_point = &((*current_point)->point);
					count--;
				}
			}
			*current_point = (struct point *)0;

			if (attrib != 0)  {
				while (attrib > 0)  {
					if ((ret_val = read_function(fdesc, buf2, DLG_RECORD_LENGTH)) <= 0)  {
						fprintf(stderr, "Attribute read 3 returns %d\n", ret_val);
						fprintf(stderr, "%80.80s\n", buf);
						exit(0);
					}

					current_attrib = &lines[num_lines].attribute;

					if (attrib > 6)  {
						i = 6;
						attrib = attrib - 6;
					}
					else  {
						i = attrib;
						attrib = 0;
					}

					end_ptr = buf2;
					while (i > 0)  {
						*current_attrib = (struct attribute *)malloc(sizeof(struct attribute));
						if (*current_attrib == (struct attribute *)0)  {
							fprintf(stderr, "malloc failed\n");
							exit(0);
						}

						(*current_attrib)->major = strtol(end_ptr, &end_ptr, 10);
						(*current_attrib)->minor = strtol(end_ptr, &end_ptr, 10);

						current_attrib = &((*current_attrib)->attribute);
						i--;
					}
					*current_attrib = (struct attribute *)0;
				}
			}

			num_lines++;
			break;

		default:
			fprintf(stderr, "Unknown record type: %c  (hexadecimal: %x)\n", buf[0], buf[0]);
//			fprintf(stderr, "%80.80s\n", buf);
//			exit(0);
			break;
		}
	}


	/*
	 * All of the useful data is parsed.
	 * Now do something with it.
	 *
	 * First find the x and y image coordinates that border this DLG chunk.
	 *
	 * Then draw the lines for which we have appropriate atribute codes stored,
	 * but don't go outside the x-y border.
	 *
	 * Then fill in all of the areas for which we have
	 * appropriate attribute codes stored, but don't go outside
	 * the x-y border.
	 */
	dlg_x_low = -1 + drawmap_round((long_sw - image_corners->sw_long) * (double)image_corners->x / (image_corners->ne_long - image_corners->sw_long));
	dlg_y_low = image_corners->y - 1 - drawmap_round((lat_ne - image_corners->sw_lat) * (double)image_corners->y / (image_corners->ne_lat - image_corners->sw_lat));
	dlg_x_high = -1 + drawmap_round((long_ne - image_corners->sw_long) * (double)image_corners->x / (image_corners->ne_long - image_corners->sw_long));
	dlg_y_high = image_corners->y - 1 - drawmap_round((lat_sw - image_corners->sw_lat) * (double)image_corners->y / (image_corners->ne_lat - image_corners->sw_lat));
	if (dlg_x_low < -1)  {
		dlg_x_low = -1;
	}
	if (dlg_y_low < -1)  {
		dlg_y_low = -1;
	}
	if (dlg_x_high >= image_corners->x)  {
		dlg_x_high = image_corners->x - 1;
	}
	if (dlg_y_high >= image_corners->y)  {
		dlg_y_high = image_corners->y - 1;
	}

	/*
	 * Cycle through all of the line data and draw all of the appropriate lines
	 * onto the image (overlaying any previous data).
	 */
	for (i = 0; i < num_lines; i++)  {
		/*
		 * In the DLG-3 format, the first area element listed
		 * represents the universe outside of the map area.
		 * Thus, lines that have area 1 as a boundary should be
		 * "neatlines" that bound the map area.
		 * Since these clutter up a map, we normally discard them.
		 * (If you want to keep them, then change the #define of OMIT_NEATLINES
		 * so that it is zero, rather than non-zero.)
		 *
		 * Here are relevant quotes from the DLG-3 guide:
		 *
		 *	expressed by network data is that of connectivity.  The network case
		 *	differs from the area case in that, irrespective of the number of closed
		 *	areas forming the graph, only two areas are encoded:  (1) the area out-
		 *	side the graph, termed the outside area; and (2) the area within the
		 *	graph, termed the background area.  All lines except the graph boundary,
		 *	or neatline, are considered to be contained within the background area.
		 *
		 *	map border.  There is one outside area for each DLG-3. It is always the
		 *	first area encountered (its ID is 1) and has the attribute code 000 0000.
		 */

		/*
		 * If the user provided a file full of attributes, then
		 * use them to control whether or not the lines are drawn.
		 * If not, then just go ahead and draw everything.
		 *
		 * Note:  If a major or minor attribute code in the attribute
		 *        file (supplied by the user) is less than
		 *        zero, it is treated as a wild card and matches
		 *        anything.
		 */
		if ((num_A_attrib > 0) || (num_L_attrib > 0))  {
			if ((OMIT_NEATLINES == 0) || ((lines[i].left_area != 1) && (lines[i].right_area != 1)))  {
				current_attrib = &lines[i].attribute;
				if (*current_attrib != (struct attribute *)0)  {
					while (*current_attrib != (struct attribute *)0)  {
						for (j = 0; j < num_L_attrib; j++)  {
							if (((attributes_L[j].major < 0) ||
							     (attributes_L[j].major == ((*current_attrib)->major))) &&
							    ((attributes_L[j].minor < 0) ||
							     (attributes_L[j].minor == ((*current_attrib)->minor))))  {
								draw_lines(&datum, lines[i].point, color, image_corners);
								goto FIN1;
							}
						}
						current_attrib = &((*current_attrib)->attribute);
					}
				}
				else  {
					/*
					 * If the feature had no attribute codes, then check if
					 * it is covered by a wild card in the attributes file.
					 */
					for (j = 0; j < num_L_attrib; j++)  {
						if (((attributes_L[j].major < 0) ||
						     (attributes_L[j].major == data_type)) &&
						    (attributes_L[j].minor < 0))  {
							draw_lines(&datum, lines[i].point, color, image_corners);
							goto FIN1;
						}
					}
				}
			}

			/*
			 * For those (hopefully rare) occasions in which something
			 * goes wrong, we provide the capability for a user to
			 * specifically request a single line from a DLG file so that
			 * the cause of the problem can be isolated.
			 * The user specifies a specific line by providing a major
			 * attribute number of 10000, and a minor attribute number
			 * equal to the desired line ID number.  Since no
			 * valid attribute (as far as I know) is ever as large as
			 * 10,000, such user-specified attribute pairs will not
			 * affect the search for legitimate attributes above (since
			 * they can't possibly match anything).  If we reach this point,
			 * then we failed to draw a line due to the legitimate-attribute
			 * checks above; so we give it one more try here, based on
			 * user-requested ID numbers.
			 *
			 * Note:  If you are using this feature, then it doesn't make
			 *        a lot of sense to process more than one DLG file,
			 *        since the ID number you give (as the minor attribute)
			 *        will be matched in every DLG file that has a
			 *        Line with that ID.  If you are trying to isolate
			 *        one (or a few) Line(s), then you probably want to
			 *        be certain which file is the source of the data.
			 */
			for (j = 0; j < num_L_attrib; j++)  {
				if ((attributes_L[j].major == 10000) &&
				     (attributes_L[j].minor == lines[i].id))  {
					draw_lines(&datum, lines[i].point, color, image_corners);
					goto FIN1;
				}
			}
		}
		else  {
			if ((OMIT_NEATLINES == 0) || ((lines[i].left_area != 1) && (lines[i].right_area != 1)))  {
				draw_lines(&datum, lines[i].point, color, image_corners);
			}
		}
FIN1:
		{;}
	}

	/*
	 * Now we fill in each interesting area on the map with the
	 * same color that bounds the area.  (For example,
	 * lakes (attribute code 050 0421) might be filled in.)
	 * However, sometimes areas might be filled in improperly.
	 * The code assumes that the reference point for an area falls
	 * within the polygon of lines that define that area.
	 * According to the DLG guide, this isn't guaranteed
	 * to always be the case, but the assumption has nonetheless
	 * worked reasonably well in practice.
	 *
	 * Area attributes are processed a bit differently than the
	 * attributes for lines:  no areas are filled in automatically.
	 * If the user did not specify any Area attributes in the attribute
	 * file, then no areas are filled in.  This is because the area-fill
	 * algorithm can occasionally run amok, and therefore the appropriate
	 * default is to not give it a chance.  For extensive details on the
	 * area-fill algorithm, see the comments at the top of fill_area().
	 */
	if (num_A_attrib > 0)  {
		for (i = 0; i < num_areas; i++)  {
			if (areas[i].number_attrib <= 0)  {
				continue;
			}

			current_attrib = &areas[i].attribute;
			while (*current_attrib != (struct attribute *)0)  {
				for (j = 0; j < num_A_attrib; j++)  {
					if (((attributes_A[j].major < 0) ||
					     (attributes_A[j].major == ((*current_attrib)->major))) &&
					    ((attributes_A[j].minor < 0) ||
					     (attributes_A[j].minor == ((*current_attrib)->minor))))  {
						fill_area(&datum, areas[i].x, areas[i].y, color, image_corners);
						goto FIN2;
					}
				}
				current_attrib = &((*current_attrib)->attribute);
			}

			/*
			 * As with the Line attributes, we provide an interface
			 * for the user to select specific areas, via their IDs.
			 */
			for (j = 0; j < num_A_attrib; j++)  {
				if ((attributes_A[j].major == 10000) &&
				     (attributes_A[j].minor == areas[i].id))  {
					fill_area(&datum, areas[i].x, areas[i].y, color, image_corners);
					goto FIN2;
				}
			}
FIN2:
			{;}
		}
	}


	/* Free up all of the malloc() memory */
	for (i = 0; i < num_lines; i++)  {
		if (lines[i].number_coords > 0)  {
			current_point = &lines[i].point;

			while (*current_point != (struct point *)0)  {
				tmp_point = (*current_point)->point;
				free(*current_point);
				*current_point = tmp_point;
			}
		}
		if (lines[i].number_attrib > 0)  {
			current_attrib = &lines[i].attribute;

			while (*current_attrib != (struct attribute *)0)  {
				tmp_attrib = (*current_attrib)->attribute;
				free(*current_attrib);
				*current_attrib = tmp_attrib;
			}
		}
	}
	for (i = 0; i < num_areas; i++)  {
		if (areas[i].number_attrib > 0)  {
			current_attrib = &areas[i].attribute;

			while (*current_attrib != (struct attribute *)0)  {
				tmp_attrib = (*current_attrib)->attribute;
				free(*current_attrib);
				*current_attrib = tmp_attrib;
			}
		}
	}
}



/*
 * Draw a series of line segments, as defined by a linked list of
 * points from an optional-format DLG file.
 *
 * This routine is recursive, not because it has to be, but because
 * it was slightly simpler that way.  Since it doesn't recurse very
 * far (on average), it isn't a performance or memory problem.
 *
 * It is a nasty routine to understand, because it has a generalized
 * interpolation algorithm to capture line segments that go beyond the
 * image boundaries.
 */
void
draw_lines(struct datum *datum, struct point *cur_point, int32_t color, struct image_corners *image_corners)
{
	double latitude1, longitude1;
	double latitude2, longitude2;
	int32_t xx1, yy1;
	int32_t xx2, yy2;
	double fxx, fyy;
	double delta_x, delta_y;
	int32_t steps;
	int32_t i;
	double m_lat, m_long, b_lat, b_long;
	double p_lat1, p_long1, p_lat2, p_long2;
	double d_lat, d_long;
	int32_t pointflags = 0;
	int32_t bothflag = 0;

	/*
	 * We recurse to the end of the linked list, and then draw line
	 * segments as we pop back up the recursion stack.
	 */
	if (cur_point->point != (struct point *)0)  {
		draw_lines(datum, cur_point->point, color, image_corners);

		/*
		 * Draw a segment between this point and the next one down the linked list.
		 *
		 * Begin by figuring out the latitude and longitude of the endpoints.
		 */
		(void)redfearn_inverse(datum, cur_point->x, cur_point->y, utm_zone, &latitude1, &longitude1);
		(void)redfearn_inverse(datum, (cur_point->point)->x, (cur_point->point)->y, utm_zone, &latitude2, &longitude2);
//fprintf(stderr, "x=%g, y=%g, zone=%d, lat=%g, long=%g\n", (cur_point->point)->x, (cur_point->point)->y, utm_zone, latitude1, longitude1);


		/*
		 * Find out whether only one endpoint, or both of them, fall
		 * outside the map area.
		 */
		if ((latitude1 < image_corners->sw_lat) || (latitude1 > image_corners->ne_lat) ||
		    (longitude1 < image_corners->sw_long) || (longitude1 > image_corners->ne_long))  {
			bothflag++;
		}
		if ((latitude2 < image_corners->sw_lat) || (latitude2 > image_corners->ne_lat) ||
		    (longitude2 < image_corners->sw_long) || (longitude2 > image_corners->ne_long))  {
			bothflag++;
		}


		/*
		 * If at least one endpoint of a line segment is outside of the area
		 * covered by the map image, then interpolate the segment.
		 *
		 * This isn't just to catch errors in a DLG file.  Since the user
		 * can specify arbitrary latitude/longitude boundaries for the
		 * map image, either or both endpoints of a segment can easily
		 * be outside of the map boundaries.
		 */
		if (bothflag > 0)  {
			/*
			 * Construct two equations for the line passing through the two
			 * endpoints.  These equations can be solved for four potential
			 * intercepts with the edge of the map area, only zero or two of
			 * which should be actual intercepts.  (In theory, there can
			 * be a single intercept at a corner, but this code should find
			 * it twice.)
			 *
			 * We construct the two lines using the classic Y = m * X + b formula,
			 * where, in one case, we let Y be the latitude and X be the longitude,
			 * and in the other case they switch roles.
			 */
			m_lat = (latitude2 - latitude1) / (longitude2 - longitude1);
			b_lat = latitude1 - m_lat * longitude1;
			m_long = 1.0 / m_lat;
			b_long = longitude1 - m_long * latitude1;

			/*
			 * We need the distance (in the Manhattan (city-block) metric) between
			 * the two endpoints.
			 * It will be used to determine whether one of the intercepts with
			 * the map edges falls between the two given endpoints.
			 */
			d_lat = fabs(latitude1 - latitude2);
			d_long = fabs(longitude1 - longitude2);

			/*
			 * Solve the two equations for the four possible intercepts, and check
			 * that they are truly intercepts.
			 * Set a flag to remember which points turned out to be intercepts.
			 */
			p_lat1 = m_lat * image_corners->sw_long + b_lat;
			if ((p_lat1 >= image_corners->sw_lat) && (p_lat1 <= image_corners->ne_lat))  {
				if ((fabs(image_corners->sw_long - longitude1) <= d_long) && (fabs(image_corners->sw_long - longitude2) <= d_long))  {
					pointflags |= 1;
				}
			}
			p_lat2 = m_lat * image_corners->ne_long + b_lat;
			if ((p_lat2 >= image_corners->sw_lat) && (p_lat2 <= image_corners->ne_lat))  {
				if ((fabs(image_corners->ne_long - longitude1) <= d_long) && (fabs(image_corners->ne_long - longitude2) <= d_long))  {
					pointflags |= 2;
				}
			}
			p_long1 = m_long * image_corners->sw_lat + b_long;
			if ((p_long1 >= image_corners->sw_long) && (p_long1 <= image_corners->ne_long))  {
				if ((fabs(image_corners->sw_lat - latitude1) <= d_lat) && (fabs(image_corners->sw_lat - latitude2) <= d_lat))  {
					pointflags |= 4;
				}
			}
			p_long2 = m_long * image_corners->ne_lat + b_long;
			if ((p_long2 >= image_corners->sw_long) && (p_long2 <= image_corners->ne_long))  {
				if ((fabs(image_corners->ne_lat - latitude1) <= d_lat) && (fabs(image_corners->ne_lat - latitude2) <= d_lat))  {
					pointflags |= 8;
				}
			}

			/*
			 * If both endpoints fall outside the map area, and there aren't exactly two
			 * intercepts, then there should be none.  (In theory, when a segment
			 * just touches a corner of the map area, then there is only one intercept,
			 * but the above code will find the same intercept twice.)
			 */
			if ((bothflag == 2) && (pointflags != 3) && (pointflags != 5) && (pointflags != 6) &&
			    (pointflags != 9) && (pointflags != 10) && (pointflags != 12))  {
				if (pointflags != 0)  {
			    		fprintf(stderr, " should have had exactly two intercepts:  0x%x  (%f %f) (%f %f)\n",
						pointflags, latitude1, longitude1, latitude2, longitude2);
				}
				return;
			}

			/* If the first endpoint is out of range, then replace it with an intercept. */
			if ((latitude1 < image_corners->sw_lat) || (latitude1 > image_corners->ne_lat) ||
			    (longitude1 < image_corners->sw_long) || (longitude1 > image_corners->ne_long))  {
				if (pointflags & 1)  {
					latitude1 = p_lat1;
					longitude1 = image_corners->sw_long;
					pointflags &= ~1;
					goto DONE1;
				}
				if (pointflags & 2)  {
					latitude1 = p_lat2;
					longitude1 = image_corners->ne_long;
					pointflags &= ~2;
					goto DONE1;
				}
				if (pointflags & 4)  {
					latitude1 = image_corners->sw_lat;
					longitude1 = p_long1;
					pointflags &= ~4;
					goto DONE1;
				}
				if (pointflags & 8)  {
					latitude1 = image_corners->ne_lat;
					longitude1 = p_long2;
					pointflags &= ~8;
					goto DONE1;
				}
			}
DONE1:

			/* If the second endpoint is out of range, then replace it with an intercept. */
			if ((latitude2 < image_corners->sw_lat) || (latitude2 > image_corners->ne_lat) ||
			    (longitude2 < image_corners->sw_long) || (longitude2 > image_corners->ne_long))  {
				if (pointflags & 1)  {
					latitude2 = p_lat1;
					longitude2 = image_corners->sw_long;
					goto DONE2;
				}
				if (pointflags & 2)  {
					latitude2 = p_lat2;
					longitude2 = image_corners->ne_long;
					goto DONE2;
				}
				if (pointflags & 4)  {
					latitude2 = image_corners->sw_lat;
					longitude2 = p_long1;
					goto DONE2;
				}
				if (pointflags & 8)  {
					latitude2 = image_corners->ne_lat;
					longitude2 = p_long2;
					goto DONE2;
				}
			}
DONE2:
	;
		}



		/*
		 * Convert the latitude/longitude pairs into pixel locations within the image.
		 *
		 * Note:  because there may be small errors in longitude1, latitude1, longitude2,
		 * and latitude2, the values of xx1, yy1, xx2, or yy2 may occasionally be off by
		 * one pixel.
		 * This appears to be acceptable in the middle of the image, since one pixel
		 * doesn't amount to much linear distance in the image.  At the edges, one might
		 * worry that the discrepancy would cause us to go over the image edges.
		 * However, the interpolation code above should successfully eliminate this
		 * potential problem.
		 *
		 * As noted above, it is okay for the array index values to go to -1, since that
		 * is the appropriate value for image_corners->sw_long or image_corners->ne_lat.
		 */
		xx1 = -1 + drawmap_round((longitude1 - image_corners->sw_long) * (double)image_corners->x / (image_corners->ne_long - image_corners->sw_long));
		yy1 = image_corners->y - 1 - drawmap_round((latitude1 - image_corners->sw_lat) * (double)image_corners->y / (image_corners->ne_lat - image_corners->sw_lat));
		xx2 = -1 + drawmap_round((longitude2 - image_corners->sw_long) * (double)image_corners->x / (image_corners->ne_long - image_corners->sw_long));
		yy2 = image_corners->y - 1 - drawmap_round((latitude2 - image_corners->sw_lat) * (double)image_corners->y / (image_corners->ne_lat - image_corners->sw_lat));
		if ((xx1 < -1) || (yy1 < -1) || (xx1 >= image_corners->x) || (yy1 >= image_corners->y))  {
			fprintf(stderr, "In draw_lines(), a coordinate exceeds the image boundaries, %d %d   %d %d\n", xx1, yy1, xx2, yy2);
			exit(0);
		}


		/*
		 * Now all that remains is to draw the line segment.
		 * We begin by deciding whether x or y is the fastest-changing
		 * coordinate.
		 */
		delta_x = xx2 - xx1;
		delta_y = yy2 - yy1;

		if (fabs(delta_x) < fabs(delta_y))  {
			steps = (int32_t)fabs(delta_y) - 1;

			if (delta_y > 0.0)  {
				delta_x = delta_x / delta_y;
				delta_y = 1.0;
			}
			else if (delta_y < 0.0)  {
				delta_x = -delta_x / delta_y;
				delta_y = -1.0;
			}
			else  {
				delta_x = 1.0;
			}
		}
		else  {
			steps = (int32_t)fabs(delta_x) - 1;

			if (delta_x > 0.0)  {
				delta_y = delta_y / delta_x;
				delta_x = 1.0;
			}
			else if (delta_x < 0.0)  {
				delta_y = -delta_y / delta_x;
				delta_x = -1.0;
			}
			else  {
				delta_y = 1.0;
			}
		}

		/* Put dots at the two endpoints. */
		*(image_corners->ptr + (yy1 + TOP_BORDER) * x_prime + xx1 + LEFT_BORDER) = color;
		*(image_corners->ptr + (yy2 + TOP_BORDER) * x_prime + xx2 + LEFT_BORDER) = color;

		/* Fill in pixels between the two endpoints. */
		fxx = xx1;
		fyy = yy1;
		for (i = 0; i < steps; i++)  {
			fxx = fxx + delta_x;
			fyy = fyy + delta_y;
			*(image_corners->ptr + (drawmap_round(fyy) + TOP_BORDER) * x_prime + drawmap_round(fxx) + LEFT_BORDER) = color;
		}
	}
}



/*
 * Fill in an area bounded by a polygon of the given color, beginning at the
 * given representative point.  (The polygon was previously created by the
 * line-drawing algorithm.)  The algorithm does this by filling in a given
 * point and then recursively calling itself to fill in the four nearest neighbors
 * (to the left, right, top, and bottom).
 *
 * Two functions handle this:  fill_area() sets things up, and then
 * fill_small_area() recursively does the work.  An enterprising reader might
 * want to convert the recursion into something less likely to consume all
 * computing resources on the planet.  However, these routines generally
 * work well unless somehow the representative point falls outside of a bounded
 * polygon.  (This problem can and does occur, particularly if we aren't using a
 * one-to-one mapping between DEM elevation samples and image pixels.  Stretching
 * and scaling can goof things up and, in my experience, more often than not lead
 * to area fill problems.)
 * If this happens, then, as the routine attempts to fill large swaths of the image,
 * the recursion chomps up all available stack memory and the program goes kaboom.
 * (More commonly, the program doesn't crash, but areas of the image are incorrectly
 * covered with swaths of blue.)  Less resources would be gobbled if, instead of using
 * recursion, we simply built a stack datatype, and pushed and popped coordinates
 * onto/from it.  No program is so perfect that it can't be improved.  However,
 * the recursion itself is not the problem, but rather the errors that
 * lead to the wrong areas being filled, and thus to excess recursion.
 *
 * One other problem with the approach taken here is that, if a lake has a narrow
 * neck, the line segments at the sides of the neck may touch.  If this is the case,
 * then only one side of the lake will be filled in (the side containing the
 * representative point) because the neck forms a solid boundary.
 *
 * Yet another problem is that the representative point may be off the map boundaries
 * if, say, a lake is at the edge of the map and the whole lake doesn't show up on
 * the map.  In such a case, the lake won't get filled in because the representative
 * point is rejected by the sanity-checking code.
 *
 * Yet another possible problem (although I have not checked into this) may be that there
 * may not necessarily be a representative point in each DLG file, when a large
 * lake spans multiple DLG files.  Thus, the DLG files may depend on a single
 * representative point, in one of the files, to do duty for all of the chunks
 * of the lake in all of the relevant files.  This would be a problem for drawmap,
 * because the program is structured on the assumption that each DLG file can be processed
 * separately from all of the others.  (Again, I have not verified that this is actually
 * a problem, I am just pointing it out as a possibility based on some anomalies I
 * have seen on output maps.)
 *
 * This algorithm is very crude at this point.  We assume that the given
 * coordinates actually do fall within the bounded area that they represent,
 * something that the DLG guide says is normal for these points, but not guaranteed.
 * It would appear that a general solution not relying on this assumption would be
 * difficult to produce.  For a convex bounding polygon, one can determine if the
 * representative point is within the bounding polygon by following the line segments
 * around the boundaries of the area and checking that the point is always on the same side
 * of the line segment (relative to the direction of motion).  However, this wouldn't
 * do us a whole lot of good.  First, the polygons are not, in general, convex.
 * Second, unless we change the area fill algorithm in some fundamental way,
 * knowing a single point (one that is guaranteed to be within the boundaries of the area)
 * still won't guarantee that the area gets filled properly (see the discussion of a
 * lake with a neck, above).  Third, knowing that a point is within the boundaries of
 * the area is not adequate to guarantee that it is within the boundaries drawn on
 * the image.  The lines drawn around the boundaries are "jagged", because we try
 * to draw slanted lines using pixels that
 * can only be placed on a square grid.  (This problem is often called "aliasing,"
 * which is a reference to Nyquist Sampling Theory; but that is a subject far
 * beyond the scope of this long, rambling comment block.)  It is theoretically possible
 * for the representative point to land on a pixel that falls outside the drawn
 * boundaries, because it just happens to fall at a place where a slanted line
 * segment "jags."  This problem can be exacerbated when the image is stretched
 * (for example, when a map area that is 2 degrees of longitude by 1 degree of
 * latitude is plotted on a 2400 by 2400 pixel grid, thus stretching the latitude
 * direction by a factor of 2).
 *
 * We also assume that the area is totally bounded on the right, left, top, and
 * bottom by points of the given color (or the edges of the DLG data).  The line-drawing
 * algorithm, above, should ensure this, as long as the line segments given in the
 * DLG file don't leave gaps (which they normally don't appear to do).
 *
 * There may be some cool, sexy way to write an area-fill algorithm that would
 * be completely general and would run fast.  However, without giving it a massive
 * amount of thought, the only truly general algorithms I have come up with are very
 * slow, involving a detailed check of each candidate point to verify that it is indeed
 * withing the given area.  As an example, here is a very clunky algorithm that seems
 * likely to work without running amok:
 *
 * Determine which collection(s) of line segments is associated with the given area.
 *     (Multiple multi-segment "curves" can bound an area, including the neatlines
 *      that bound the entire area encompassed by the DLG file.)
 * Follow the line segments around the bounding polygon and break the polygon into
 *     multiple polygons, each of which is convex.  This can be done by examining the
 *     angles between successive line segments.
 * For each convex sub-polygon:
 *     Find the largest and smallest longitude and latitude associated with all of the
 *         segments in the sub-polygon.
 *     Sweep through all points within the rectangle determined by the longitude/latitude
 *         bounding box and check each point to determine whether it is within the area
 *         in question.  This can be done by following the line segments around the polygon
 *         and checking that the point is always on the same side of each segment.  (The
 *         sign of the line segment identifier(s) determines which side the point is
 *         supposed to be on.  See the DLG documentation for details.)
 *
 * Although there is a lot of handwaving in the above description, it should be obvious
 * that this algorithm would be incredibly slow.  One could obviously come up with some
 * ways to speed it up, since it is designed for simplicity of description rather than
 * efficiency of operation, but it is not immediately obvious how to make it really fast.
 * Nor is it immediately obvious (at least to me) how to come up with a different algorithm
 * that would be both robust and fast.  Also, the current version appears to work much of the
 * time, with occasional inevitable glitches.  Thus, for the time being, we are stuck with
 * the code that follows.
 *
 * The bottom line is that I have never been satisfied with the area fill algorithm, but
 * I haven't been able to convince myself to put a massive effort into replacing it.
 * Instead, I usually turn off area-fill entirely, and then use an image editor to fill
 * in the areas myself.
 */
void
fill_small_area(struct image_corners *image_corners, int32_t x1, int32_t y1, int32_t color)
{
	/*
	 * Check that we have not wandered outside of the area
	 * covered by the data from this DLG file.
	 */
	if ((x1 < dlg_x_low) || (x1 > dlg_x_high) || (y1 < dlg_y_low) || (y1 > dlg_y_high))  {
		return;
	}

	/*
	 * Fill in the given pixel, and recusively fill in the pixels to the
	 * left, right, top, and bottom.
	 */
	*(image_corners->ptr + (y1 + TOP_BORDER) * x_prime + x1 + LEFT_BORDER) = color;

	if (*(image_corners->ptr + (y1 - 1 + TOP_BORDER) * x_prime + x1 + LEFT_BORDER) != color)  {
		fill_small_area(image_corners, x1, y1 - 1, color);
	}
	if (*(image_corners->ptr + (y1 + 1 + TOP_BORDER) * x_prime + x1 + LEFT_BORDER) != color)  {
		fill_small_area(image_corners, x1, y1 + 1, color);
	}
	if (*(image_corners->ptr + (y1 + TOP_BORDER) * x_prime + x1 - 1 + LEFT_BORDER) != color)  {
		fill_small_area(image_corners, x1 - 1, y1, color);
	}
	if (*(image_corners->ptr + (y1 + TOP_BORDER) * x_prime + x1 + 1 + LEFT_BORDER) != color)  {
		fill_small_area(image_corners, x1 + 1, y1, color);
	}
}
void
fill_area(struct datum *datum, double px1, double py1, int32_t color, struct image_corners *image_corners)
{
	double latitude1, longitude1;
	int32_t xx1, yy1;

	/* Find the latitude and longitude of the representative point and convert them into index values. */
	(void)redfearn_inverse(datum, px1, py1, utm_zone, &latitude1, &longitude1);

	xx1 = -1 + drawmap_round((longitude1 - image_corners->sw_long) * (double)image_corners->x / (image_corners->ne_long - image_corners->sw_long));
	yy1 = image_corners->y - 1 - drawmap_round((latitude1 - image_corners->sw_lat) * (double)image_corners->y / (image_corners->ne_lat - image_corners->sw_lat));
	if ((xx1 < -1) || (xx1 >= image_corners->x) || (yy1 < -1) || (yy1 >= image_corners->y))  {
/*		fprintf(stderr, "fill_area() was given a starting point outside the map area:  (%d %d) (%f %f)\n", xx1, yy1, latitude1, longitude1); */
		return;
	}

	if ((xx1 < dlg_x_low) || (xx1 > dlg_x_high) || (yy1 < dlg_y_low) || (yy1 > dlg_y_high))  {
		fprintf(stderr, "fill_area() was passed a bad starting point:  (%d %d) (%f %f)\n\tlimits are: %d %d   %d %d\n",
			xx1, yy1, latitude1, longitude1, dlg_x_low, dlg_x_high, dlg_y_low, dlg_y_high);
		return;
	}


	/*
	 * Some debugging code to figure out where the representative point
	 * for each area falls on the image.
	 */
//	{
//	static int32_t h = 0;
//	int32_t la, lo;
//	double long_prime = fabs(longitude1) - 0.5;
//	la = latitude1;
//	lo = long_prime;
//	la = la * 10000 + ((int)((latitude1 - la) * 60.0)) * 100 + (int)((latitude1 - la - ((int)((latitude1 - la) * 60.0)) / 60.0) * 3600.0 + 0.5);
//	lo = lo * 10000 + ((int)((long_prime - lo) * 60.0)) * 100 + (int)((long_prime - lo - ((int)((long_prime - lo) * 60.0)) / 60.0) * 3600.0 + 0.5);
//	
//	fprintf(stderr, "lat=%f long=%f     %d %d\n", la, lo, xx1, yy1);
//	fprintf(stdout, "Area %2.2d                                                                             island   Blaine                                             30005%6.6dN%7.7dW                     %f %f                         \n", h, la, lo, px1, py1);
//	h++;
//	
//	*(image_corners.ptr + (yy1 + TOP_BORDER) * x_prime + xx1 + LEFT_BORDER) = B_GREEN;
//	return;
//	}


	/*
	 * Some small areas are so small that the lines around their borders have
	 * already filled them in.  If the representative point is already set to
	 * the target color, then we assume we are in such an area.  In such cases,
	 * we immediately return, because otherwise (if we happen to be sitting
	 * right on the boundary) we will begin filling in the area outside the
	 * boundary and potentially fill large swaths of the image.  The risk of
	 * simply returning (rather than doing a more thorough investigation of
	 * what is going on) is that the boundary lines may not have actually
	 * filled the area in, but rather
	 * that the representative point just happens to fall very near
	 * (or on) the boundary.  There is not much we can do about this potential
	 * problem, unless we re-write the whole area-filling algorithm
	 * (not necessarily a bad idea).  However, in practice, things seem
	 * to generally work out semi-okay for many of the data sets I have tried.
	 * I have detected a number of area-fill problems, but haven't done the research
	 * to determine the individual causes.  Some or all of the unfilled lakes that
	 * I have found could conveivably have been caused by this test.
	 */
	if (*(image_corners->ptr + (yy1 + TOP_BORDER) * x_prime + xx1 + LEFT_BORDER) == color)  {
		return;
	}

	/* Recursively call fill_small_area() to do most of the work. */
	fill_small_area(image_corners, xx1, yy1, color);
}




/*
 * Parse the given attribute file and store the results
 * in the appropriate storage areas.
 */
void
process_attrib(char *attribute_file)
{
	int gz_flag;
	int attribute_fdesc;
	int32_t ret_val;
	char *ptr;
	char buf[MAX_ATTRIB_RECORD_LENGTH];


	num_A_attrib = 0;
	num_L_attrib = 0;
	if (attribute_file != (char *)0)  {
		if (strcmp(attribute_file + strlen(attribute_file) - 3, ".gz") == 0)  {
			gz_flag = 1;
			if ((attribute_fdesc = buf_open_z(attribute_file, O_RDONLY)) < 0)  {
				fprintf(stderr, "Can't open %s for reading, errno = %d\n", attribute_file, errno);
				exit(0);
			}
		}
		else  {
			gz_flag = 0;
			if ((attribute_fdesc = buf_open(attribute_file, O_RDONLY)) < 0)  {
				fprintf(stderr, "Can't open %s for reading, errno = %d\n", attribute_file, errno);
				exit(0);
			}
		}

		fprintf(stderr, "Processing Attribute file:  %s\n", attribute_file);

		while ( 1 )  {
			if (gz_flag == 0)  {
				if ((ret_val = get_a_line(attribute_fdesc, buf, MAX_ATTRIB_RECORD_LENGTH)) <= 0)  {
					break;
				}
			}
			else  {
				if ((ret_val = get_a_line_z(attribute_fdesc, buf, MAX_ATTRIB_RECORD_LENGTH)) <= 0)  {
					break;
				}
			}

			buf[ret_val - 1] = '\0';	/* Put a null in place of the newline */

			switch(buf[0])  {
			case '\0':
			case '\n':
			case '\r':
			case ' ':
			case '\t':
				/* Blank line, or line that begins with white space.  Ignore. */
				break;
			case '#':
				/* Comment line.  Ignore. */
				break;
			case 'N':
				/* We don't currently use Node attributes, so do nothing with them. */
				fprintf(stderr, "Ignoring Node attribute:  %s\n", buf);
				break;
			case 'A':
				/* Area attribute. */
				if (num_A_attrib >= MAX_A_ATTRIB)  {
					fprintf(stderr, "Out of space for Area attributes, ignoring:  %s\n", buf);
					break;
				}
				attributes_A[num_A_attrib].major = strtol(&buf[1], &ptr, 10);
				attributes_A[num_A_attrib].minor = strtol(ptr, &ptr, 10);
				num_A_attrib++;
				break;
			case 'L':
				/* Line attribute. */
				if (num_L_attrib >= MAX_L_ATTRIB)  {
					fprintf(stderr, "Out of space for Line attributes, ignoring:  %s\n", buf);
					break;
				}
				attributes_L[num_L_attrib].major = strtol(&buf[1], &ptr, 10);
				attributes_L[num_L_attrib].minor = strtol(ptr, &ptr, 10);
				num_L_attrib++;
				break;
			default:
				fprintf(stderr, "Ignoring unknown attribute type:  %s\n", buf);
				break;
			}
		}

		if (gz_flag == 0)  {
			buf_close(attribute_fdesc);
		}
		else  {
			buf_close_z(attribute_fdesc);
		}
	}
}
