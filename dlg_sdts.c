/*
 * =========================================================================
 * dlg_sdts.c - Routines to handle DLG data from SDTS files.
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
#include <ctype.h>
#include "drawmap.h"
#include "dlg.h"
#include "sdts_utils.h"


/*
 * The routines in this file are uniquely-dedicated to handling
 * DLG files in the Spatial Data Transfer System (SDTS) format.
 *
 * For a general description of SDTS, see sdts_utils.c.
 */



/*
 * Note to the reader of this code.  This code will probably be difficult
 * to understand unless you are very familiar with the internals of SDTS files
 * and optional-format DLG-3 files.  Normally I would provide a lot of descriptive
 * comments to help you along.  However, in this case, such comments would
 * probably end up being several times the length of the code.  I wrote this
 * program with two large documents available for reference.  If you want to
 * follow the operation of the code, you will probably need those documents
 * too.  The documents were:
 *
 * DLG-3 SDTS Transfer Description, May 7, 1996
 *
 * Standards for Digital Line Graphs, September 1999,
 * Department of the Interior, US Geological Survey,
 * National Mapping Division.
 *
 * There are comments at key points in the code, but they are not adequate
 * for a full understanding unless you have the reference materials at hand.
 *
 * Even the documents aren't really enough.  It is also useful to have
 * both sample SDTS files and sample optional-format DLG-3 files for reference as well.
 */



/*
 * The code that processes DLG files is very spaghetti-like, since
 * it got squeezed and twisted and stretched while I figured out how
 * DLG files are put together.
 *
 * Because of this, and because I don't like to write functions that
 * take 35 arguments, there are a lot of global variables used by the
 * DLG code.  Most of them are accumulated here.
 *
 * Many global variables are already allocated by dlg.c.
 * We re-use them via extern directives.
 */


/*
 * Storage for attribute types.
 */
extern int32_t num_A_attrib;
extern int32_t num_L_attrib;
extern struct maj_min attributes_A[MAX_A_ATTRIB];
extern struct maj_min attributes_L[MAX_L_ATTRIB];

/*
 * The sizes of the nodes, areas, and lines arrays are their theoretical maximum values.
 * It would probably be cooler to malloc() these as we go, but coolness was not an
 * objective of this program.  It would still be cool to read the maximum values from
 * the DLG file headers and check them against the values below to verify that
 * the standards haven't changed and left this program behind.
 */
extern struct nodes nodes[MAX_NODES];
extern struct areas areas[MAX_AREAS];
extern struct lines lines[MAX_LINES];


/*
 * Arrays to keep track of attributes from various SDTS files.
 */
static struct polygon_attrib polygon_attrib[MAX_POLY_NUM];
static struct attrib_files  {
	char module_name[4];
	int32_t num_attrib;
	struct attribute_list *attrib;
} attrib_files[MAX_ATTRIB_FILES];

/*
 * Array for building line lists for output
 */
static int32_t line_list[MAX_LINE_LIST];


/*
 * comparison function for use with qsort.
 */
static int
compare_lines(const void *lines1, const void *lines2)
{
	if (((struct lines *)lines1)->id < ((struct lines *)lines2)->id)  {
		return -1;
	}
	else if (((struct lines *)lines1)->id > ((struct lines *)lines2)->id)  {
		return 1;
	}
	else  {
		return 0;
	}
}


extern double lat_se, long_se, lat_sw, long_sw, lat_ne, long_ne, lat_nw, long_nw;
extern int32_t dlg_x_low, dlg_y_low, dlg_x_high, dlg_y_high;
extern int32_t x_prime;

extern int32_t utm_zone;



int32_t get_extra_attrib(int32_t, int32_t *major, int32_t *minor, int32_t *major2, int32_t *minor2, struct subfield *subfield);
int32_t process_attrib_sdts(char *, char *, int32_t *, int32_t *, int32_t, int32_t);
void uniq_attrib(struct attribute **, short *);
void get_theme(char *, char *, int32_t, int32_t);


/*
 * This routine parses informational data from the various SDTS files
 * comprising a DLG file set and inserts the converted data into
 * internal variables.
 * If you haven't read the DLG file guide and looked at a
 * DLG file, this code will probably be incomprehensible.
 *
 * Here are the meanings of the various module names associated with DLG files:
 *
 * There is one module associated with Identification:
 *   IDEN --- Identification
 *
 * Misc:
 *   STAT --- Transfer Statistics
 *   CATD --- Catalog/Directory
 *   CATS --- Catalog/Spatial Domain
 *   CATX --- Catalog/Cross-Reference
 *
 * There are five modules involved in data quality:
 *   DQHL --- Data Quality/Lineage
 *   DQPA --- Data Quality/Positional Accuracy
 *   DQAA --- Data Quality/Attribute Accuracy
 *   DQCG --- Data Quality/Completeness
 *   DQLC --- Data Quality/Logical Consistency
 *
 * There are three data dictionary modules:
 *   DDSH --- Data Dictionary/Schema
 *   MDEF --- Data Dictionary/Definition	(Part of Master Data Dictionary)
 *   MDOM --- Data Dictionary/Domain		(Part of Master Data Dictionary)
 *
 * There are three modules associated with spatial reference and domain:
 *   XREF --- External Spatial Reference
 *   IREF --- Internal Spatial Reference
 *
 * Files associated with data:
 *   AHDR --- Attribute Primary Header
 *   A??F --- main Attribute Primary
 *   ACOI --- Coincidence Attribute Primary
 *   ABDM --- Agencies for Boundaries
 *   ARDM --- Route Numbers for Roads and Trails
 *   AHPR --- Elevation in meters for Hypsography
 *   B??? --- Secondary attribute files (only used for 2,000,000-scale transfers)
 *   FF01 --- Composite Surfaces
 *   LE01 --- Line (Chain of Points)
 *   NA01 --- Point-Node (Area Points --- Representative points within defined areas.)
 *   NE01 --- Point-Node (Entity Points --- The location of point features like buildings and towers.)
 *   NO01 --- Point-Node (Planar Node --- The junction of two or more lines.)
 *   NP01 --- Point-Node (Registration Points --- This generally defines the four corner points of the data in UTM.
 *                        The same data is available in AHDR in the form of latitude/longitude.)
 *   PC01 --- Polygon
 */
int32_t
process_dlg_sdts(char *passed_file_name, char *output_file_name, int32_t gz_flag,
		struct image_corners *image_corners, int32_t info_flag, int32_t file_image_flag)
{
	int32_t i, j = 100000000, k = 100000000;	// bogus initializers to expose errors.
	int32_t start_node, current_node;
	int32_t number_of_islands;
	int32_t module_num = -1;
	int32_t layer;
	int32_t count;
	int32_t color;
	int32_t d, m;
	double s;
	double x = -100000000.0, y = -100000000.0;	// bogus initializers to expose errors.
	char code1, code2;
	char output_file[12];
	int output_fdesc = -1;
	char buf[DLG_RECORD_LENGTH + 1];
	char buf3[DLG_RECORD_LENGTH + 1];
	struct point **current_point = (struct point **)0;		// bogus initializer to expose errors.
	struct point **current_point2= (struct point **)0;		// bogus initializer to expose errors.
	struct point *tmp_point;
	struct attribute **current_attrib = (struct attribute **)0;	// bogus initializer to expose errors.
	struct attribute *tmp_attrib;
	int32_t attrib;
	int32_t current_poly = -100000000;				// bogus initializer to expose errors.
	int32_t num_polys;
	int32_t num_areas = -1;
	int32_t num_lines = -1;
	int32_t num_nodes = -1;
	int32_t num_NO_nodes;
	int32_t data_type = 0;
	double latitude1, longitude1, latitude2, longitude2;
	// ssize_t (* read_function)(int, void *, size_t);
	int32_t plane_ref = -1;
	char save_byte;
	int32_t file_name_length;
	char file_name[MAX_FILE_NAME + 1];
	int32_t byte_order;
	int32_t upper_case_flag;
	int32_t need;
	double x_scale_factor = -100000000.0, y_scale_factor = -100000000.0;	// bogus initializers to expose errors.
	double x_origin = -100000000.0, y_origin = -100000000.0;		// bogus initializers to expose errors.
	double x_resolution = -100000000.0, y_resolution = -100000000.0;	// bogus initializers to expose errors.
	char postal_code[30];
	int32_t num_attrib_files;
	struct subfield subfield;
	char source_date[4];
	char sectional_indicator[3];
	char category_name[21];
	int32_t vertical_datum = -1;
	int32_t horizontal_datum = -1;
	int32_t dlg_level = -1;
	int32_t line_list_size;
	double se_x = -100000000.0,	// bogus initializers to expose errors.
	       se_y = -100000000.0,
	       sw_x = -100000000.0,
	       sw_y = -100000000.0,
	       nw_x = -100000000.0,
	       nw_y = -100000000.0,
	       ne_x = -100000000.0,
	       ne_y = -100000000.0;
	struct datum datum;
	int32_t record_id;


	if (file_image_flag == 0)  {
		x_prime = image_corners->x + LEFT_BORDER + right_border;
	}


	/* find the native byte-order on this machine. */
	byte_order = swab_type();


	/*
	 * Make a copy of the file name.  The one we were originally
	 * given is still stored in the command line arguments.
	 * It is probably a good idea not to alter those, lest we
	 * scribble something we don't want to scribble.
	 */
	file_name_length = strlen(passed_file_name);
	if (file_name_length > MAX_FILE_NAME)  {
		fprintf(stderr, "File name is too long.\n");
		return 1;
	}
	strncpy(file_name, passed_file_name, MAX_FILE_NAME);
	file_name[MAX_FILE_NAME] = '\0';
	if (file_name_length < 12)  {
		/*
		 * Excluding the initial path, the file name should have the form
		 * ????LE??.DDF, perhaps with a ".gz" on the end.  If it isn't
		 * at least long enough to have this form, then reject it.
		 */
		fprintf(stderr, "File name doesn't look right.\n");
		return 1;
	}
	/* Check the case of the characters in the file name by examining a single character. */
	if (gz_flag == 0)  {
		if (file_name[file_name_length - 1] == 'f')  {
			upper_case_flag = 0;
		}
		else  {
			upper_case_flag = 1;
		}
	}
	else  {
		if (file_name[file_name_length - 4] == 'f')  {
			upper_case_flag = 0;
		}
		else  {
			upper_case_flag = 1;
		}
	}



	/*
	 * The first file name we need is the Attribute Primary Header (AHDR) module, which contains
	 * the latitude/longitude registration points of the four corners of the data.
	 *
	 * There is a lot of stuff in AHDR that goes into record 3.
	 * Thus we provide an extra record buffer so that we can build
	 * record 3 while processing AHDR.  The buffer is buf3[], and
	 * before we start we fill it with blanks.
	 *
	 * Note:  We fill buffer 3 even when file_image_flag == 0,
	 * since it doesn't take much processing, and that way we
	 * don't have to clutter up the code with "if" statements.
	 */
	for (i = 0; i < DLG_RECORD_LENGTH; i++)  {
		buf3[i] = ' ';
	}
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "ahdr.ddf", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "ahdr.ddf", 8);
		}
	}
	else  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "AHDR.DDF", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "AHDR.DDF", 8);
		}
	}
	/*
	 * Open the file in preparation for parsing.
	 */
	if (begin_ddf(file_name) < 0)  {
		fprintf(stderr, "Can't open %s for reading, errno = %d\n", file_name, errno);
		exit(0);
	}
	/*
	 * We also need the layer number from the file name.
	 * Some SDTS transfers have multiple LE files, and we need
	 * to pull the correct record out of the AHDR file.
	 *
	 * In theory, we should probably pull this out of the FF01
	 * module, but this would be a lot of work to get the same
	 * information.  As long as USGS files are named with the
	 * layer number in the file name, this approach is more efficient.
	 */
	if (gz_flag != 0)  {
		layer = strtol(&passed_file_name[file_name_length - 9], (char **)0, 10);
	}
	else  {
		layer = strtol(&passed_file_name[file_name_length - 6], (char **)0, 10);
	}
	if (layer <= 0)  {
		fprintf(stderr, "Got bad layer number (%d) from file %s.\n", layer, passed_file_name);
		return 1;
	}
	/*
	 * Loop through the subfields until we find what we want.
	 */
	need = 25;
	while (get_subfield(&subfield) != 0)  {
		if (strcmp(subfield.tag, "ATPR") == 0)  {
			if ((strstr(subfield.format, "I") != (char *)0) && (strncmp(subfield.label, "RCID", 4) == 0))  {
				/*
				 * Check for the correct layer.
				 * set layer = -1 as a flag if you find it.
				 */
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				if (layer == strtol(subfield.value, (char **)0, 10))  {
					layer = -1;
				}
				subfield.value[subfield.length] = save_byte;
			}
		}
		else if ((layer < 0) && (strcmp(subfield.tag, "ATTP") == 0))  {
			save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
			if ((strstr(subfield.format, "R") != (char *)0) && (strncmp(subfield.label, "SW_LATITUDE", 11) == 0))  {
				lat_sw = strtod(subfield.value, (char **)0);
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strncmp(subfield.label, "SW_LONGITUDE", 12) == 0))  {
				long_sw = strtod(subfield.value, (char **)0);
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strncmp(subfield.label, "NW_LATITUDE", 11) == 0))  {
				lat_nw = strtod(subfield.value, (char **)0);
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strncmp(subfield.label, "NW_LONGITUDE", 12) == 0))  {
				long_nw = strtod(subfield.value, (char **)0);
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strncmp(subfield.label, "NE_LATITUDE", 11) == 0))  {
				lat_ne = strtod(subfield.value, (char **)0);
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strncmp(subfield.label, "NE_LONGITUDE", 12) == 0))  {
				long_ne = strtod(subfield.value, (char **)0);
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strncmp(subfield.label, "SE_LATITUDE", 11) == 0))  {
				lat_se = strtod(subfield.value, (char **)0);
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strncmp(subfield.label, "SE_LONGITUDE", 12) == 0))  {
				long_se = strtod(subfield.value, (char **)0);
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "BANNER", 6) == 0))  {
				strncpy(buf, subfield.value, subfield.length);
				for (i = subfield.length; i < DLG_RECORD_LENGTH; i++)  {
					buf[i] = ' ';
				}
				buf[DLG_RECORD_LENGTH] = '\0';
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "SOURCE_DATE", 11) == 0))  {
				/*
				 * Copy the original source date for later use.
				 */
				if (subfield.length == 4)  {
					strncpy(source_date, subfield.value, 4);
				}
				else  {
					strncpy(source_date, "    ", 4);
				}
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "QUAD_NUMBER", 11) == 0))  {
				/*
				 * Copy the sectional indicator for later use.
				 */
				if (subfield.length == 3)  {
					strncpy(sectional_indicator, subfield.value, 3);
				}
				else  {
					strncpy(sectional_indicator, "   ", 3);
				}
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strncmp(subfield.label, "L_PRIM_INTERVAL", 15) == 0))  {
				/*
				 * Put the largest primary interval into buf3.
				 * Append the comma separator.
				 *
				 * Note:  SDTS stores this in a 5-byte field Real subfield
				 * but there are only 4 bytes reserved in the DLG-3 header.
				 * The discrepancy arises from the fact that the value is
				 * stored in the header as a three-digit integer (the integer
				 * part of the 5-byte Real), followed by a single digit
				 * describing the units (which for 24K and 100K DLG files
				 * is always 2, for meters).
				 */
				if ((subfield.length == 5) && (strncmp(subfield.value, "     ", 5) != 0))  {
					i = (int32_t)strtod(subfield.value, (char **)0);
					sprintf(&buf3[41], "%3d2,", i);
				}
				else  {
					strncpy(&buf3[41], "     ", 5);
				}
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strncmp(subfield.label, "L_PB_INTERVAL", 13) == 0))  {
				/*
				 * Put the largest primary bathymetric interval into buf3.
				 * Append the single-space filler.
				 *
				 * Note:  SDTS stores this in a 5-byte field Real subfield
				 * but there are only 4 bytes reserved in the DLG-3 header.
				 * The discrepancy arises from the fact that the value is
				 * stored in the header as a three-digit integer (the integer
				 * part of the 5-byte Real), followed by a single digit
				 * describing the units (which for 24K and 100K DLG files
				 * is always 2, for meters).
				 */
				if ((subfield.length == 5) && (strncmp(subfield.value, "     ", 5) != 0))  {
					i = (int32_t)strtod(subfield.value, (char **)0);
					sprintf(&buf3[46], "%3d2 ", i);
				}
				else  {
					strncpy(&buf3[46], "     ", 5);
				}
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strncmp(subfield.label, "S_PRIM_INTERVAL", 15) == 0))  {
				/*
				 * Put the smallest primary interval into buf3.
				 * Append the comma separator.
				 *
				 * Note:  SDTS stores this in a 5-byte field Real subfield
				 * but there are only 4 bytes reserved in the DLG-3 header.
				 * The discrepancy arises from the fact that the value is
				 * stored in the header as a three-digit integer (the integer
				 * part of the 5-byte Real), followed by a single digit
				 * describing the units (which for 24K and 100K DLG files
				 * is always 2, for meters).
				 */
				if ((subfield.length == 5) && (strncmp(subfield.value, "     ", 5) != 0))  {
					i = (int32_t)strtod(subfield.value, (char **)0);
					sprintf(&buf3[51], "%3d2,", i);
				}
				else  {
					strncpy(&buf3[51], "     ", 5);
				}
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strncmp(subfield.label, "S_PB_INTERVAL", 13) == 0))  {
				/*
				 * Put the smallest primary bathymetric interval into buf3.
				 *
				 * Note:  SDTS stores this in a 5-byte field Real subfield
				 * but there are only 4 bytes reserved in the DLG-3 header.
				 * The discrepancy arises from the fact that the value is
				 * stored in the header as a three-digit integer (the integer
				 * part of the 5-byte Real), followed by a single digit
				 * describing the units (which for 24K and 100K DLG files
				 * is always 2, for meters).
				 */
				if ((subfield.length == 5) && (strncmp(subfield.value, "     ", 5) != 0))  {
					i = (int32_t)strtod(subfield.value, (char **)0);
					sprintf(&buf3[56], "%3d2", i);
				}
				else  {
					strncpy(&buf3[56], "    ", 4);
				}
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "CODED_FLAG", 10) == 0))  {
				/*
				 * Copy the coded flag into buf3.
				 * It is preceded by three bytes reserved for future use as coded flags.
				 * These 3 reserved bytes normally appear to be stored as null characters in USGS DLG-3
				 * files.  We will do the same, although it is ugly.
				 */
				if (subfield.length == 1)  {
					buf3[63] = subfield.value[0];
				}
				else  {
					buf3[63] = ' ';
				}
				buf3[60] = '\0';
				buf3[61] = '\0';
				buf3[62] = '\0';
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "EDGEWS", 6) == 0))  {
				/*
				 * Copy the west-edge status flag into buf3.
				 */
				if (subfield.length == 1)  {
					buf3[64] = subfield.value[0];
				}
				else  {
					buf3[64] = ' ';
				}
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "EDGEWR", 6) == 0))  {
				/*
				 * Copy the west-edge status flag reason into buf3.
				 */
				if (subfield.length == 1)  {
					buf3[65] = subfield.value[0];
				}
				else  {
					buf3[65] = ' ';
				}
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "EDGENS", 6) == 0))  {
				/*
				 * Copy the north-edge status flag into buf3.
				 */
				if (subfield.length == 1)  {
					buf3[66] = subfield.value[0];
				}
				else  {
					buf3[66] = ' ';
				}
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "EDGENR", 6) == 0))  {
				/*
				 * Copy the north-edge status flag reason into buf3.
				 */
				if (subfield.length == 1)  {
					buf3[67] = subfield.value[0];
				}
				else  {
					buf3[67] = ' ';
				}
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "EDGEES", 6) == 0))  {
				/*
				 * Copy the east-edge status flag into buf3.
				 */
				if (subfield.length == 1)  {
					buf3[68] = subfield.value[0];
				}
				else  {
					buf3[68] = ' ';
				}
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "EDGEER", 6) == 0))  {
				/*
				 * Copy the east-edge status flag reason into buf3.
				 */
				if (subfield.length == 1)  {
					buf3[69] = subfield.value[0];
				}
				else  {
					buf3[69] = ' ';
				}
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "EDGESS", 6) == 0))  {
				/*
				 * Copy the south-edge status flag into buf3.
				 */
				if (subfield.length == 1)  {
					buf3[70] = subfield.value[0];
				}
				else  {
					buf3[70] = ' ';
				}
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "EDGESR", 6) == 0))  {
				/*
				 * Copy the south-edge status flag reason into buf3.
				 */
				if (subfield.length == 1)  {
					buf3[71] = subfield.value[0];
				}
				else  {
					buf3[71] = ' ';
				}
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "VERTICAL_DATUM", 14) == 0))  {
				/*
				 * Convert and save the vertical datum for later use.
				 */
				if (strncmp(subfield.value, "NGVD", (subfield.length > 4 ? 4 : subfield.length)) == 0)  {
					vertical_datum = 0;
				}
				else if (strncmp(subfield.value, "NAVD", (subfield.length > 4 ? 4 : subfield.length)) == 0)  {
					vertical_datum = 1;
				}
				else if (strncmp(subfield.value, "LOCAL MEAN SEA LEVEL", subfield.length) == 0)  {
					vertical_datum = 2;
				}
				else  {
					vertical_datum = -1;
				}
				need--;
			}
			subfield.value[subfield.length] = save_byte;
			if (need == 0)  {
				/* This is all we need.  Break out of the loop. */
				break;
			}
		}
	}
	/* We are done with this file, so close it. */
	end_ddf();
	/* Check that we found what we wanted. */
	if (need > 0)  {
		fprintf(stderr, "Failed to get needed data from file %s.\n", file_name);
		return 1;
	}
	else if (need < 0)  {
		fprintf(stderr, "Warning:  Got more data from file %s than expected.\n", file_name);
	}


	/*
	 * We now have enough information to open the output file.
	 *
	 * file_image_flag controls whether this routine writes to a file (file_image_flag != 0)
	 * or writes to the drawmap image buffer (file_image_flag == 0).  This is ugly, but
	 * it lets us maintain only a single version of this SDTS parsing code.
	 */
	if (file_image_flag != 0)  {
		if (output_file_name != (char *)0)  {
			if ((output_fdesc = open(output_file_name, O_WRONLY | O_CREAT | O_EXCL, 0644)) < 0)  {
				fprintf(stderr, "Can't create %s for writing, errno = %d\n", output_file_name, errno);
				exit(0);
			}
		}
		else  {
			code1 = 'a' + floor((fabs(lat_se) + (lat_se < 0 ? -1.0 : 1.0) * 0.02 - floor(fabs(lat_se) + (lat_se < 0 ? -1.0 : 1.0) * 0.02)) * 8.0);
			code2 = '1' + floor((fabs(long_se) + (long_se < 0 ? 1.0 : -1.0) * 0.02 - floor(fabs(long_se) + (long_se < 0 ? 1.0 : -1.0) * 0.02)) * 8.0);
			sprintf(output_file, "%2.2d%3.3d%c%c.dlg",
				(int)(fabs(lat_se) + (lat_se < 0 ? -1.0 : 1.0) * 0.02),
				(int)(fabs(long_se) + (long_se < 0 ? 1.0 : -1.0) * 0.02),
				code1, code2);
			if ((output_fdesc = open(output_file, O_WRONLY | O_CREAT | O_EXCL, 0644)) < 0)  {
				fprintf(stderr, "Can't create %s for writing, errno = %d\n", output_file, errno);
				exit(0);
			}
		}
		/*
		 * The first record of the file is simply the banner.
		 * We stored the banner in buf[].  Write it out.
		 */
		if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
			fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
			exit(0);
		}
	}



	/*
	 * The next file we need is the IDEN module, which contains the official DLG
	 * name.  The name is normally followed (in SDTS) by " / " and a category name (theme),
	 * such as HYDROGRAPHY.  We need to strip off this extra category bit, since it isn't
	 * normally part of an optional-format record 1.  However, we also need to save
	 * it because it shows up at the beginning of record 15.  We also want to
	 * extract the postal code if the user has given the info_flag.
	 *
	 * Here is a typical SDTS TITL subfield:
	 *
	 * ALZADA, MT-SD-WY / BOUNDARIES
	 */
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "iden.ddf", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "iden.ddf", 8);
		}
	}
	else  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "IDEN.DDF", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "IDEN.DDF", 8);
		}
	}
	/*
	 * Open the file in preparation for parsing.
	 */
	if (begin_ddf(file_name) < 0)  {
		fprintf(stderr, "Can't open %s for reading, errno = %d\n", file_name, errno);
		exit(0);
	}
	/*
	 * Loop through the subfields until we find what we want.
	 */
	need = 3;
	while (get_subfield(&subfield) != 0)  {
		if (strcmp(subfield.tag, "IDEN") == 0)  {
			if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "TITL", 4) == 0))  {
				for (i = 0; i < subfield.length; i++)  { 
					if (subfield.value[i] == '/')  { 
						j = i;
						break;
					}
				}
				if (i < subfield.length)  {
					while ((i - 1) >= 0)  {
						i--;
						if (subfield.value[i] != ' ')  {
							i++;
							break;
						}
					}
				}
				if (info_flag != 0)  {
					/*
					 * Grab the postal code.
					 */
					for (k = 0; k < i; k++)  {
						if (subfield.value[k] == ',')  {
							break;
						}
					}
					if (k < i)  {
						k++;
						for ( ; k < i; k++)  {
							if (k != ' ')  {
								break;
							}
						}
						strncpy(postal_code, &subfield.value[k], (i - k + 1) > 29 ? 29 : (i - k + 1));
						postal_code[(i - k + 1) > 29 ? 29 : (i - k + 1)] = '\0';
					}
					else  {
						postal_code[0] = 0;
					}
				}
				/*
				 * Copy the official DLG name into the beginning of the
				 * record buffer.
				 * It is followed by a 1-byte space filler.
				 */
				if (i > 40)  {
					/* Truncate the name if it is too long.  It shouldn't be. */
					i = 40;
				}
				strncpy(buf, subfield.value, i);
				for ( ; i < 41; i++)  {
					buf[i] = ' ';
				}
				j++;
				while (j < subfield.length)  {
					if (subfield.value[j] != ' ')  {
						break;
					}
					j++;
				}
				if (j != subfield.length)  {
					if ((subfield.length - j) <= 20)  {
						strncpy(category_name, &subfield.value[j], subfield.length - j);
						category_name[subfield.length - j] = '\0';
					}
					else  {
						strncpy(category_name, &subfield.value[j], 20);
						category_name[20] = '\0';
					}
				}
				else  {
					category_name[0] = '\0';
				}
				for (i = strlen(category_name) - 1; i >= 0; i--)  {
					if (category_name[i] != ' ')  {
						category_name[i + 1] = '\0';
						break;
					}
				}
				if (i == -1)  {
					category_name[0] = '\0';
				}
				need--;
			}
			else if ((strstr(subfield.format, "I") != (char *)0) && (strncmp(subfield.label, "SCAL", 4) == 0))  {
				/*
				 * Copy the source material scale into the
				 * record buffer.
				 * It is followed by space filler out to byte 63.
				 */
				if (subfield.length >= 8)  {
					strncpy(&buf[52], subfield.value, 8);
				}
				else  {
					strncpy(&buf[52], "        ", 8);
					strncpy(&buf[52 + 8 - subfield.length], subfield.value, subfield.length);
				}
				for (i = 60; i < 63; i++)  {
					buf[i] = ' ';
				}
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "DAST", 4) == 0))  {
				/*
				 * Save the DLG level for later use.
				 */
				if (subfield.length == 5)  {
					dlg_level = subfield.value[4] - '0';
				}
				else  {
					dlg_level = -1;
				}
				if ((dlg_level != 3) && (dlg_level != 2))  {
					fprintf(stderr, "Warning:  This does not appear to be a level 2 or 3 DLG.\n");
				}
				need--;
			}
		}
		if (need == 0)  {
			/* This is all we need.  Break out of the loop. */
			break;
		}
	}
	/* We are done with this file, so close it. */
	end_ddf();
	/* Check that we found what we wanted. */
	if (need > 0)  {
		fprintf(stderr, "Failed to get needed data from file %s.\n", file_name);
		return 1;
	}
	else if (need < 0)  {
		fprintf(stderr, "Warning:  Got more data from file %s than expected.\n", file_name);
	}
	if (file_image_flag != 0)  {
		/*
		 * Copy the source date into the buffer.
		 * It is followed by blank filler out to byte 52.
		 */
		strncpy(&buf[41], source_date, 4);
		buf[45] = ',';
		for (i = 46; i < 52; i++)  {
			buf[i] = ' ';
		}
		/*
		 * Copy the sectional indicator into the buffer.
		 * It is followed by blank filler out to the end of the record.
		 */
		strncpy(&buf[63], sectional_indicator, 3);
		for (i = 66; i < DLG_RECORD_LENGTH; i++)  {
			buf[i] = ' ';
		}
		/*
		 * The second record of the file is set up.
		 * Write it out.
		 *
		 * Also, we completely built record 3 while processing the ADHR
		 * module.  Write it out as well.
		 */
		if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
			fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
			exit(0);
		}
		if (write(output_fdesc, buf3, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
			fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
			exit(0);
		}
	}


	
	/*
	 * As a "just-in-case" measure, we grabbed the category name (theme) from
	 * the DLG name above.  This would be fine, for everything except TRANSPORTATION.
	 * In TRANSPORTATION files, the name we grabbed above will be "TRANSPORTATION".
	 * However, what we really need is one of the three themes:  "ROADS AND TRAILS",
	 * "RAILROADS", or "PIPE & TRANS LINES".  In order to get this information,
	 * we have at least two possible routes of attack.  We can try to deduce it from
	 * the types of attributes referenced in the Node/Area/Lines records.  Or, we
	 * can read the CATS module and try to get the THEM subfield (which stands for theme)
	 * for our passed_file_name.  We take the latter course here, but we will also
	 * try the other approach inside the process_attrib_sdts() function.
	 */
	get_theme(passed_file_name, category_name, upper_case_flag, gz_flag);



	if (info_flag != 0)  {
		/* If info_flag is nonzero, then all we need to do is print some info and return. */
		for (i = 0; i < 40; i++)  {
			/* We want only that part of the official name up to the comma. */
			if (buf[i] == ',')  {
				break;
			}
		}
		fprintf(stdout, "\t%.*s\t%s\t%.20s\t%g:%g:%g:%g\n", i, buf, postal_code, category_name, lat_se, long_se, lat_nw, long_nw);
		return 0;
	}



	/*
	 * Within the DLG data, locations are specified with pairs of
	 * Universal Transverse Mercator (x,y) coordinates.
	 *
	 * The header information for the DLG data gives 4 reference
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
	 * in which no two sides have the same length.
	 *
	 * If file_image_flag == 0, then we are supposed to be writing to the image buffer.
	 * There is no point in this if the data won't affect the image.
	 * Do a quick check here to find out if the data is off the map boundaries.
	 * If so, then we can return now and save a lot of work.
	 */
	if (file_image_flag == 0)  {
		if ((lat_sw > image_corners->ne_lat) ||
		    (long_sw > image_corners->ne_long) ||
		    (lat_ne < image_corners->sw_lat) ||
		    (long_ne < image_corners->sw_long))  {
			return 0;
		}
	}


	num_attrib_files = process_attrib_sdts(passed_file_name, category_name, &data_type, &color, gz_flag, upper_case_flag);



	/*
	 * The next file name we need is the XREF module, which contains
	 * the Planar Reference System, Horizontal Datum, and UTM Zone.
	 */
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "xref.ddf", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "xref.ddf", 8);
		}
	}
	else  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "XREF.DDF", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "XREF.DDF", 8);
		}
	}
	/*
	 * Open the file in preparation for parsing.
	 */
	if (begin_ddf(file_name) < 0)  {
		fprintf(stderr, "Can't open %s for reading, errno = %d\n", file_name, errno);
		exit(0);
	}
	/*
	 * Loop through the subfields until we find what we want.
	 */
	need = 3;
	while (get_subfield(&subfield) != 0)  {
		if (strcmp(subfield.tag, "XREF") == 0)  {
			if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "HDAT") == 0))  {
				/*
				 * Valid choices are "NAS" for NAD-27, "NAX" for NAD-83,
				 * "Puerto Rico", "Old Hawaiian", and "Local (Astro)".
				 */
				if (strncmp(subfield.value, "NAS", subfield.length) == 0)  {
					horizontal_datum = 0;
				}
				else if (strncmp(subfield.value, "NAX", subfield.length) == 0)  {
					horizontal_datum = 1;
				}
				else if (strncmp(subfield.value, "Puerto Rico", subfield.length) == 0)  {
					horizontal_datum = 2;
				}
				else if (strncmp(subfield.value, "Old Hawaiian", subfield.length) == 0)  {
					horizontal_datum = 3;
				}
				else if (strncmp(subfield.value, "Local (Astro)", subfield.length) == 0)  {
					horizontal_datum = 4;
				}
				else  {
					horizontal_datum = -1;
				}
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "RSNM") == 0))  {
				/*
				 * Valid choices are "UTM" and "GEO"
				 */
				if (strncmp(subfield.value, "GEO", subfield.length) == 0)  {
					plane_ref = 0;
				}
				else if (strncmp(subfield.value, "UTM", subfield.length) == 0)  {
					plane_ref = 1;
				}
				else  {
					plane_ref = -1;
				}
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "ZONE") == 0))  {
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				utm_zone = strtol(subfield.value, (char **)0, 10);
				subfield.value[subfield.length] = save_byte;
				need--;
			}
			if (need == 0)  {
				/* This is all we need.  Break out of the loop. */
				break;
			}
		}
	}
	/* We are done with this file, so close it. */
	end_ddf();
	/* Check that we found what we wanted. */
	if (need > 0)  {
		fprintf(stderr, "Failed to get needed data from file %s.\n", file_name);
		return 1;
	}
	else if (need < 0)  {
		fprintf(stderr, "Warning:  Got more data from file %s than expected.\n", file_name);
	}

	/* Check the just-acquired results. */
	if (plane_ref != 1)  {
		fprintf(stderr, "DLG file does not use UTM ground planimetric coordinates.  (Plane_ref = %d)\n", plane_ref);
		exit(0);
	}
	if ((utm_zone < 1) || (utm_zone > 60))  {
		fprintf(stderr, "DLG file contains bad UTM zone %d.\n", utm_zone);
		exit(0);
	}
	if (horizontal_datum == 0)  {
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
	else if (horizontal_datum == 1)  {
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

		fprintf(stderr, "DLG file uses a horizontal datum that I don't handle (%d).\n", horizontal_datum);
		fprintf(stderr, "Defaulting to NAD-27.  This may result in positional errors in the data.\n");
	}



	/*
	 * The next file name we need is the IREF module, which contains
	 * the x and y scale factors that are multiplied times the x and y UTM coordinate values,
	 * the x and y origins, which are added to the UTM coordinate values, and the
	 * x and y horizontal resolutions.
	 */
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "iref.ddf", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "iref.ddf", 8);
		}
	}
	else  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "IREF.DDF", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "IREF.DDF", 8);
		}
	}
	/*
	 * Open the file in preparation for parsing.
	 */
	if (begin_ddf(file_name) < 0)  {
		fprintf(stderr, "Can't open %s for reading, errno = %d\n", file_name, errno);
		exit(0);
	}
	/*
	 * Loop through the subfields until we find what we want.
	 */
	need = 6;
	while (get_subfield(&subfield) != 0)  {
		if (strcmp(subfield.tag, "IREF") == 0)  {
			save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
			if ((strstr(subfield.format, "R") != (char *)0) && (strcmp(subfield.label, "SFAX") == 0))  {
				x_scale_factor = strtod(subfield.value, (char **)0);
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strcmp(subfield.label, "SFAY") == 0))  {
				y_scale_factor = strtod(subfield.value, (char **)0);
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strcmp(subfield.label, "XORG") == 0))  {
				x_origin = strtod(subfield.value, (char **)0);
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strcmp(subfield.label, "YORG") == 0))  {
				y_origin = strtod(subfield.value, (char **)0);
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strcmp(subfield.label, "XHRS") == 0))  {
				x_resolution = strtod(subfield.value, (char **)0);
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strcmp(subfield.label, "YHRS") == 0))  {
				y_resolution = strtod(subfield.value, (char **)0);
				need--;
			}
			subfield.value[subfield.length] = save_byte;
			if (need == 0)  {
				/* This is all we need.  Break out of the loop. */
				break;
			}
		}
	}
	/* We are done with this file, so close it. */
	end_ddf();
	/* Check that we found what we wanted. */
	if (need > 0)  {
		fprintf(stderr, "Failed to get needed data from file %s.\n", file_name);
		return 1;
	}
	else if (need < 0)  {
		fprintf(stderr, "Warning:  Got more data from file %s than expected.\n", file_name);
	}



	/*
	 * Parse with the NP?? file, which contains the UTM x/y
	 * coordinates of the four corner registration points.
	 */
	strncpy(file_name, passed_file_name, MAX_FILE_NAME);
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			file_name[file_name_length - 11] = 'n';
			file_name[file_name_length - 10] = 'p';
		}
		else  {
			file_name[file_name_length -  8] = 'n';
			file_name[file_name_length -  7] = 'p';
		}
	}
	else  {
		if (gz_flag != 0)  {
			file_name[file_name_length - 11] = 'N';
			file_name[file_name_length - 10] = 'P';
		}
		else  {
			file_name[file_name_length -  8] = 'N';
			file_name[file_name_length -  7] = 'P';
		}
	}
	/*
	 * Open the file in preparation for parsing.
	 */
	if (begin_ddf(file_name) < 0)  {
		fprintf(stderr, "Can't open %s for reading, errno = %d\n", file_name, errno);
		exit(0);
	}
	/*
	 * Loop through the subfields until we find what we want.
	 */
	record_id = -1;
	need = 8;
	while (get_subfield(&subfield) != 0)  {
		if (strcmp(subfield.tag, "PNTS") == 0)  {
			if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				record_id = strtol(subfield.value, (char **)0, 10);
				subfield.value[subfield.length] = save_byte;
			}
		}
		else if (strcmp(subfield.tag, "SADR") == 0)  {
			if ((strstr(subfield.format, "B") != (char *)0) && (strcmp(subfield.label, "X") == 0))  {
				if (subfield.length != 4)  {
					/* Error */
					x = -1.0;
				}
				else  {
					i = (((int32_t)subfield.value[3] & 0xff) << 24) |
					    (((int32_t)subfield.value[2] & 0xff) << 16) |
					    (((int32_t)subfield.value[1] & 0xff) <<  8) |
					     ((int32_t)subfield.value[0] & 0xff);
					if (byte_order == 0)  {
						x = (double)i * x_scale_factor + x_origin;
					}
					else if (byte_order == 1)  {
						LE_SWAB(&i);
						x = (double)i * x_scale_factor + x_origin;
					}
					else if (byte_order == 2)  {
						PDP_SWAB(&i);
						x = (double)i * x_scale_factor + x_origin;
					}
				}
				switch (record_id)  {
				case 1:
					sw_x = x;
					break;
				case 2:
					nw_x = x;
					break;
				case 3:
					ne_x = x;
					break;
				case 4:
					se_x = x;
					break;
				default:
					fprintf(stderr, "Problem parsing NP?? module record %d in file %s.\n", record_id, file_name);
					break;
				}
				need--;
			}
			else if ((strstr(subfield.format, "B") != (char *)0) && (strcmp(subfield.label, "Y") == 0))  {
				if (subfield.length != 4)  {
					/* Error */
					y = -1.0;
				}
				else  {
					i = (((int32_t)subfield.value[3] & 0xff) << 24) |
					    (((int32_t)subfield.value[2] & 0xff) << 16) |
					    (((int32_t)subfield.value[1] & 0xff) <<  8) |
					     ((int32_t)subfield.value[0] & 0xff);
					if (byte_order == 0)  {
						y = (double)i * y_scale_factor + y_origin;
					}
					else if (byte_order == 1)  {
						LE_SWAB(&i);
						y = (double)i * y_scale_factor + y_origin;
					}
					else if (byte_order == 2)  {
						PDP_SWAB(&i);
						y = (double)i * y_scale_factor + y_origin;
					}
				}
				switch (record_id)  {
				case 1:
					sw_y = y;
					break;
				case 2:
					nw_y = y;
					break;
				case 3:
					ne_y = y;
					break;
				case 4:
					se_y = y;
					break;
				default:
					fprintf(stderr, "Problem parsing NP?? module record %d in file %s.\n", record_id, file_name);
					break;
				}
				need--;
			}
		}
		if (need == 0)  {
			/* This is all we need.  Break out of the loop. */
			break;
		}
	}
	/* We are done with this file, so close it. */
	end_ddf();
	/* Check that we found what we wanted. */
	if (need > 0)  {
		fprintf(stderr, "Failed to get needed data from file %s.\n", file_name);
		return 1;
	}
	else if (need < 0)  {
		fprintf(stderr, "Warning:  Got more data from file %s than expected.\n", file_name);
	}



	/*
	 * We are now in a position to write a bunch more records to the output file.
	 */
	if (file_image_flag != 0)  {
		/*
		 * Prepare and write record 4.
		 * The 2 code says that the file uses meters to measure horizontal distance.
		 * (There is no other choice.)
		 * The first 4 code is the number of file to map transformation paramters.
		 * (There is no other choice.)
		 * The 0 code is the number of accuracy/miscellaneous records.
		 * (There is no other choice.)
		 * The second 4 code is the number of control points.
		 * (There is no other choice.)
		 * The 1 code is the number of categories in the DLG file.
		 * (I assume this means hydrography versus transportation versus whatever)
		 * (There is no other choice.)
		 */
		sprintf(buf, "%6d%6d%6d%6d% 18.11E%6d%6d%6d%6d%3d%3d        ",
				dlg_level, plane_ref, utm_zone, 2, x_resolution, 4, 0, 4, 1, horizontal_datum, vertical_datum);
		for (i = 24; i < 42; i++)  {
			if (buf[i] == 'E')  buf[i] = 'D';	// USGS files use 'D' for exponentiation.
		}
		if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
			fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
			exit(0);
		}


		/*
		 * Records 5 through 9 contain 15 map transformation parameters,
		 * three per record.
		 * For 24K and 100K DLGs, that use UTM coordinates, only the first
		 * two parameters are non-zero.  The first one is the longitude of
		 * the center of the cell.  The second one is the latitude of the
		 * center of the cell.
		 *
		 * The values are in the form:  degrees * 1000000 + minutes * 1000 + seconds
		 */
		latitude1 = (lat_se + lat_ne) / 2.0;
		decimal_degrees_to_dms(latitude1, &d, &m, &s);
		latitude2 = (d < 0 ? -1.0 : 1.0) * (fabs((double)d) * 1000000.0 + (double)m * 1000.0 + s);
		longitude1 = (long_se + long_sw) / 2.0;
		decimal_degrees_to_dms(longitude1, &d, &m, &s);
		longitude2 = (d < 0 ? -1.0 : 1.0) * (fabs((double)d) * 1000000.0 + (double)m * 1000.0 + s);

		sprintf(buf, "% 24.15E% 24.15E% 24.15E        ", longitude2, latitude2, 0.0);
		for (i = 0; i < 72; i++)  {
			if (buf[i] == 'E')  buf[i] = 'D';	// USGS files use 'D' for exponentiation.
		}

		if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
			fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
			exit(0);
		}
		if (write(output_fdesc, "   0.000000000000000D+00   0.000000000000000D+00   0.000000000000000D+00        ", DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
			fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
			exit(0);
		}
		if (write(output_fdesc, "   0.000000000000000D+00   0.000000000000000D+00   0.000000000000000D+00        ", DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
			fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
			exit(0);
		}
		if (write(output_fdesc, "   0.000000000000000D+00   0.000000000000000D+00   0.000000000000000D+00        ", DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
			fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
			exit(0);
		}
		if (write(output_fdesc, "   0.000000000000000D+00   0.000000000000000D+00   0.000000000000000D+00        ", DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
			fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
			exit(0);
		}



		/*
		 * Record 10 contains 4 internal file-to-map projection transformation parameters.
		 * These apparently are used to convert internal file coordinates into a ground
		 * planimetric system (presumably UTM).  Since the internal coordinates are already
		 * in UTM, the record takes the form:  1.0 0.0 0.0 0.0.
		 */
		if (write(output_fdesc, " 0.10000000000D+01 0.00000000000D+00 0.00000000000D+00 0.00000000000D+00        ", DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
			fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
			exit(0);
		}



		/*
		 * Records 11 through 14 define the four corners of the data.  Each record takes
		 * a form like:
		 *
		 * SW       45.750000 -111.250000         480554.09  5066083.19
		 *
		 * where there is a two-letter code defining the corner, the latitude/longitude
		 * in decimal degrees, and the UTM x and y coordinates.
		 */
		sprintf(buf, "%2.2s    % 12.6f%12.6f      % 12.2f% 12.2f                    ", "SW", lat_sw, long_sw, sw_x, sw_y);
		if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
			fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
			exit(0);
		}
		sprintf(buf, "%2.2s    % 12.6f%12.6f      % 12.2f% 12.2f                    ", "NW", lat_nw, long_nw, nw_x, nw_y);
		if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
			fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
			exit(0);
		}
		sprintf(buf, "%2.2s    % 12.6f%12.6f      % 12.2f% 12.2f                    ", "NE", lat_ne, long_ne, ne_x, ne_y);
		if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
			fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
			exit(0);
		}
		sprintf(buf, "%2.2s    % 12.6f%12.6f      % 12.2f% 12.2f                    ", "SE", lat_se, long_se, se_x, se_y);
		if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
			fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
			exit(0);
		}


		/*
		 * Record 15 has to wait until we read in all of the node/line/area data,
		 * since it contains numbers that tell how many nodes/lines/areas there are.
		 */
	}



	/*
	 * Parse the LE?? file, which contains segmented lines, this was the file name
	 * originally passed into this routine.
	 */
	/*
	 * Open the file in preparation for parsing.
	 */
	if (begin_ddf(passed_file_name) < 0)  {
		fprintf(stderr, "Can't open %s for reading, errno = %d\n", passed_file_name, errno);
		exit(0);
	}
	/*
	 * Loop through the subfields until we find what we want.
	 */
	attrib = -1;	// Use this convenient variable as a non-related flag for first trip through loop.
	count = 0;
	while (get_subfield(&subfield) != 0)  {
		if (strcmp(subfield.tag, "LINE") == 0)  {
			if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
				/* We are starting a new line.  Initialize what need initializing. */
				if (attrib >= 0)  {
					/*
					 * If we aren't starting the first line,
					 * then terminate the attribute string and node list of the
					 * previous line and update the counts.
					 */
					*current_attrib = (struct attribute *)0;
					*current_point = (struct point *)0;
					lines[num_lines].number_attrib = attrib;
					lines[num_lines].number_coords = count;
					uniq_attrib(&lines[num_lines].attribute, &lines[num_lines].number_attrib);
				}
				module_num = -1;
				num_lines++;
				count = 0;
				attrib = 0;
				if (num_lines >= MAX_LINES)  {
					fprintf(stderr, "Ran out of space to store lines.  Some lines may be missing.\n");
					break;
				}
				current_attrib = &lines[num_lines].attribute;
				current_point = &lines[num_lines].point;
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				lines[num_lines].id = strtol(subfield.value, (char **)0, 10);
				subfield.value[subfield.length] = save_byte;
			}
		}
		else if (strcmp(subfield.tag, "ATID") == 0)  {
			if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "MODN") == 0))  {
				if (subfield.length != 4)  {
					fprintf(stderr, "Attribute module name (%.*s) is not 4 characters long.\n", subfield.length, subfield.value);
					module_num = -1;
					continue;
				}
				for (module_num = 0; module_num < num_attrib_files; module_num++)  {
					if (strncmp(subfield.value, attrib_files[module_num].module_name, 4) == 0)  {
						break;
					}
				}
				if (module_num == num_attrib_files)  {
					fprintf(stderr, "Warning:  Attribute module has unexpected name (%.*s).  Attributes may be in error.\n", subfield.length, subfield.value);
					module_num = -1;
				}
			}
			else if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
				if (module_num < 0)  {
					continue;
				}
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				i =  strtol(subfield.value, (char **)0, 10);
				subfield.value[subfield.length] = save_byte;
				if (i <= attrib_files[module_num].num_attrib)  {
					for (j = 0; j < MAX_EXTRA; j++)  {
						if (attrib_files[module_num].attrib[i - 1].major[j] != 0)  {
							*current_attrib = (struct attribute *)malloc(sizeof(struct attribute));
							if (*current_attrib == (struct attribute *)0)  {
								fprintf(stderr, "malloc failed\n");
								exit(0);
							}
							(*current_attrib)->major = attrib_files[module_num].attrib[i - 1].major[j];
							(*current_attrib)->minor = attrib_files[module_num].attrib[i - 1].minor[j];

							current_attrib = &((*current_attrib)->attribute);
							attrib++;
						}
					}
				}
			}
		}
		else if (strcmp(subfield.tag, "PIDL") == 0)  {
			if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				lines[num_lines].left_area = strtol(subfield.value, (char **)0, 10);
				subfield.value[subfield.length] = save_byte;
			}
		}
		else if (strcmp(subfield.tag, "PIDR") == 0)  {
			if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				lines[num_lines].right_area = strtol(subfield.value, (char **)0, 10);
				subfield.value[subfield.length] = save_byte;
			}
		}
		else if (strcmp(subfield.tag, "SNID") == 0)  {
			if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				lines[num_lines].start_node = strtol(subfield.value, (char **)0, 10);
				subfield.value[subfield.length] = save_byte;
			}
		}
		else if (strcmp(subfield.tag, "ENID") == 0)  {
			if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				lines[num_lines].end_node = strtol(subfield.value, (char **)0, 10);
				subfield.value[subfield.length] = save_byte;
			}
		}
		else if (strcmp(subfield.tag, "SADR") == 0)  {
			/*
			 * We assume that the X coordinate always comes first.
			 */
			if ((strstr(subfield.format, "B") != (char *)0) && (strcmp(subfield.label, "X") == 0))  {
				*current_point = (struct point *)malloc(sizeof(struct point));
				if (*current_point == (struct point *)0)  {
					fprintf(stderr, "malloc failed\n");
					exit(0);
				}

				if (subfield.length != 4)  {
					/* Error */
					(*current_point)->x = -1.0;
				}
				else  {
					i = (((int32_t)subfield.value[3] & 0xff) << 24) |
					    (((int32_t)subfield.value[2] & 0xff) << 16) |
					    (((int32_t)subfield.value[1] & 0xff) <<  8) |
					     ((int32_t)subfield.value[0] & 0xff);
					if (byte_order == 0)  {
						(*current_point)->x = (double)i * x_scale_factor + x_origin;
					}
					else if (byte_order == 1)  {
						LE_SWAB(&i);
						(*current_point)->x = (double)i * x_scale_factor + x_origin;
					}
					else if (byte_order == 2)  {
						PDP_SWAB(&i);
						(*current_point)->x = (double)i * x_scale_factor + x_origin;
					}
				}
			}
			else if ((strstr(subfield.format, "B") != (char *)0) && (strcmp(subfield.label, "Y") == 0))  {
				if (subfield.length != 4)  {
					/* Error */
					(*current_point)->y = -1.0;
				}
				else  {
					i = (((int32_t)subfield.value[3] & 0xff) << 24) |
					    (((int32_t)subfield.value[2] & 0xff) << 16) |
					    (((int32_t)subfield.value[1] & 0xff) <<  8) |
					     ((int32_t)subfield.value[0] & 0xff);
					if (byte_order == 0)  {
						(*current_point)->y = (double)i * y_scale_factor + y_origin;
					}
					else if (byte_order == 1)  {
						LE_SWAB(&i);
						(*current_point)->y = (double)i * y_scale_factor + y_origin;
					}
					else if (byte_order == 2)  {
						PDP_SWAB(&i);
						(*current_point)->y = (double)i * y_scale_factor + y_origin;
					}
				}

				current_point = &((*current_point)->point);
				count++;
			}
		}
	}
	if (num_lines >= 0)  {
		/*
		 * If we had at least one line
		 * then close out the attribute and node information of the
		 * previous line and update the counts.
		 */
		*current_attrib = (struct attribute *)0;
		*current_point = (struct point *)0;
		lines[num_lines].number_attrib = attrib;
		lines[num_lines].number_coords = count;
		uniq_attrib(&lines[num_lines].attribute, &lines[num_lines].number_attrib);
	}
	num_lines++;
	/* We are done with this file, so close it. */
	end_ddf();



	/*
	 * Parse with the NO?? file, which contains normal planar nodes.
	 * These nodes lie at either end of a piecewise-linear feature.
	 */
	strncpy(file_name, passed_file_name, MAX_FILE_NAME);
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			file_name[file_name_length - 11] = 'n';
			file_name[file_name_length - 10] = 'o';
		}
		else  {
			file_name[file_name_length -  8] = 'n';
			file_name[file_name_length -  7] = 'o';
		}
	}
	else  {
		if (gz_flag != 0)  {
			file_name[file_name_length - 11] = 'N';
			file_name[file_name_length - 10] = 'O';
		}
		else  {
			file_name[file_name_length -  8] = 'N';
			file_name[file_name_length -  7] = 'O';
		}
	}
	/*
	 * Open the file in preparation for parsing.
	 */
	if (begin_ddf(file_name) < 0)  {
		fprintf(stderr, "Can't open %s for reading, errno = %d\n", file_name, errno);
		exit(0);
	}
	/*
	 * Loop through the subfields until we find what we want.
	 */
	attrib = -1;	// Use this convenient variable as a non-related flag for first trip through loop.
	while (get_subfield(&subfield) != 0)  {
		if (strcmp(subfield.tag, "PNTS") == 0)  {
			if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
				/* We are starting a new node.  Initialize what needs initializing. */
				if (attrib >= 0)  {
					/*
					 * If we aren't starting the first node,
					 * then terminate the attribute string of the
					 * previous node and update the count.
					 */
					*current_attrib = (struct attribute *)0;
					nodes[num_nodes].number_attrib = attrib;
					uniq_attrib(&nodes[num_nodes].attribute, &nodes[num_nodes].number_attrib);
				}
				module_num = -1;
				num_nodes++;
				attrib = 0;
				if (num_nodes >= MAX_NODES)  {
					fprintf(stderr, "Ran out of space to store nodes.  Some nodes may be missing.\n");
					break;
				}
				current_attrib = &nodes[num_nodes].attribute;
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				nodes[num_nodes].id = strtol(subfield.value, (char **)0, 10);
				subfield.value[subfield.length] = save_byte;
			}
		}
		else if (strcmp(subfield.tag, "ATID") == 0)  {
			if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "MODN") == 0))  {
				if (subfield.length != 4)  {
					fprintf(stderr, "Attribute module name (%.*s) is not 4 characters long.\n", subfield.length, subfield.value);
					module_num = -1;
					continue;
				}
				for (module_num = 0; module_num < num_attrib_files; module_num++)  {
					if (strncmp(subfield.value, attrib_files[module_num].module_name, 4) == 0)  {
						break;
					}
				}
				if (module_num == num_attrib_files)  {
					fprintf(stderr, "Warning:  Attribute module has unexpected name (%.*s).  Attributes may be in error.\n", subfield.length, subfield.value);
					module_num = -1;
				}
			}
			else if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
				if (module_num < 0)  {
					continue;
				}
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				i =  strtol(subfield.value, (char **)0, 10);
				subfield.value[subfield.length] = save_byte;
				if (i <= attrib_files[module_num].num_attrib)  {
					for (j = 0; j < MAX_EXTRA; j++)  {
						if (attrib_files[module_num].attrib[i - 1].major[j] != 0)  {
							*current_attrib = (struct attribute *)malloc(sizeof(struct attribute));
							if (*current_attrib == (struct attribute *)0)  {
								fprintf(stderr, "malloc failed\n");
								exit(0);
							}
							(*current_attrib)->major = attrib_files[module_num].attrib[i - 1].major[j];
							(*current_attrib)->minor = attrib_files[module_num].attrib[i - 1].minor[j];

							current_attrib = &((*current_attrib)->attribute);
							attrib++;
						}
					}
				}
			}
		}
		else if (strcmp(subfield.tag, "SADR") == 0)  {
			/*
			 * We assume that the X coordinate always comes first.
			 */
			if ((strstr(subfield.format, "B") != (char *)0) && (strcmp(subfield.label, "X") == 0))  {
				if (subfield.length != 4)  {
					/* Error */
					nodes[num_nodes].x = -1.0;
				}
				else  {
					i = (((int32_t)subfield.value[3] & 0xff) << 24) |
					    (((int32_t)subfield.value[2] & 0xff) << 16) |
					    (((int32_t)subfield.value[1] & 0xff) <<  8) |
					     ((int32_t)subfield.value[0] & 0xff);
					if (byte_order == 0)  {
						nodes[num_nodes].x = (double)i * x_scale_factor + x_origin;
					}
					else if (byte_order == 1)  {
						LE_SWAB(&i);
						nodes[num_nodes].x = (double)i * x_scale_factor + x_origin;
					}
					else if (byte_order == 2)  {
						PDP_SWAB(&i);
						nodes[num_nodes].x = (double)i * x_scale_factor + x_origin;
					}
				}
			}
			else if ((strstr(subfield.format, "B") != (char *)0) && (strcmp(subfield.label, "Y") == 0))  {
				if (subfield.length != 4)  {
					/* Error */
					nodes[num_nodes].y = -1.0;
				}
				else  {
					i = (((int32_t)subfield.value[3] & 0xff) << 24) |
					    (((int32_t)subfield.value[2] & 0xff) << 16) |
					    (((int32_t)subfield.value[1] & 0xff) <<  8) |
					     ((int32_t)subfield.value[0] & 0xff);
					if (byte_order == 0)  {
						nodes[num_nodes].y = (double)i * y_scale_factor + y_origin;
					}
					else if (byte_order == 1)  {
						LE_SWAB(&i);
						nodes[num_nodes].y = (double)i * y_scale_factor + y_origin;
					}
					else if (byte_order == 2)  {
						PDP_SWAB(&i);
						nodes[num_nodes].y = (double)i * y_scale_factor + y_origin;
					}
				}
			}
		}
	}
	if (num_nodes >= 0)  {
		/*
		 * If we had at least one node
		 * then close out the attribute information of the
		 * previous line and node and update the counts.
		 */
		*current_attrib = (struct attribute *)0;
		nodes[num_nodes].number_attrib = attrib;
		uniq_attrib(&nodes[num_nodes].attribute, &nodes[num_nodes].number_attrib);
	}
	num_nodes++;
	num_NO_nodes = num_nodes;
	/* We are done with this file, so close it. */
	end_ddf();



	/*
	 * Parse the NE?? file, which contains degenerate lines.
	 * In the optional-format DLG files, degenerate lines appear both in the
	 * node section and the line section.  In SDTS, they appear only in the
	 * NE?? module.  We simultaneously add them to the line list and the node list.
	 *
	 * When we add the node to the line array, we must artificially build
	 * a node list that is two nodes long, although the two nodes are
	 * identical.
	 *
	 * We have already read nodes from the NO?? module into the nodes array.
	 * Add the new nodes at the end of the array.
	 *
	 * The only other node files are NP??, which contains registration
	 * points for the four corners of the data, and NA??, which contains
	 * area representative points.  Neither of these files affects the nodes
	 * array.  Thus, after this block of code, the nodes array should be complete.
	 */
	strncpy(file_name, passed_file_name, MAX_FILE_NAME);
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			file_name[file_name_length - 11] = 'n';
			file_name[file_name_length - 10] = 'e';
		}
		else  {
			file_name[file_name_length -  8] = 'n';
			file_name[file_name_length -  7] = 'e';
		}
	}
	else  {
		if (gz_flag != 0)  {
			file_name[file_name_length - 11] = 'N';
			file_name[file_name_length - 10] = 'E';
		}
		else  {
			file_name[file_name_length -  8] = 'N';
			file_name[file_name_length -  7] = 'E';
		}
	}
	/*
	 * Open the file in preparation for parsing.
	 */
	if (begin_ddf(file_name) >= 0)  {
		/*
		 * Loop through the subfields until we find what we want.
		 */
		attrib = -1;	// Use this convenient variable as a non-related flag for first trip through loop.
		count = 0;
		num_lines--;
		num_nodes--;
		while (get_subfield(&subfield) != 0)  {
			if (strcmp(subfield.tag, "PNTS") == 0)  {
				if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
					/* We are starting a new node.  Initialize what needs initializing. */
					if (attrib >= 0)  {
						/*
						 * If we aren't starting the first node,
						 * then terminate the attribute string of the
						 * previous line and update the counts.
						 */
						*current_attrib = (struct attribute *)0;
						*current_point = (struct point *)0;
						lines[num_lines].number_attrib = attrib;
						lines[num_lines].number_coords = count;
						uniq_attrib(&lines[num_lines].attribute, &lines[num_lines].number_attrib);
	
//						*current_attrib2 = (struct attribute *)0;
//						nodes[num_nodes].number_attrib = attrib;
//						uniq_attrib(&nodes[num_nodes].attribute, &nodes[num_nodes].number_attrib);
					}
					module_num = -1;
					num_nodes++;
					num_lines++;
					count = 0;
					attrib = 0;
					if (num_nodes >= MAX_NODES)  {
						fprintf(stderr, "Ran out of space to store nodes.  Some nodes may be missing.\n");
						break;
					}
					if (num_lines >= MAX_LINES)  {
						fprintf(stderr, "Ran out of space to store lines.  Some lines may be missing.\n");
						break;
					}
//					current_attrib2 = &nodes[num_nodes].attribute;
					save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
					nodes[num_nodes].id = strtol(subfield.value, (char **)0, 10);
					subfield.value[subfield.length] = save_byte;
	
					current_attrib = &lines[num_lines].attribute;
					current_point = &lines[num_lines].point;
					lines[num_lines].id = nodes[num_nodes].id;
					lines[num_lines].start_node = nodes[num_nodes].id;
					lines[num_lines].end_node = nodes[num_nodes].id;
				}
			}
			else if (strcmp(subfield.tag, "ATID") == 0)  {
				if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "MODN") == 0))  {
					if (subfield.length != 4)  {
						fprintf(stderr, "Attribute module name (%.*s) is not 4 characters long.\n", subfield.length, subfield.value);
						module_num = -1;
						continue;
					}
					for (module_num = 0; module_num < num_attrib_files; module_num++)  {
						if (strncmp(subfield.value, attrib_files[module_num].module_name, 4) == 0)  {
							break;
						}
					}
					if (module_num == num_attrib_files)  {
						fprintf(stderr, "Warning:  Attribute module has unexpected name (%.*s).  Attributes may be in error.\n", subfield.length, subfield.value);
						module_num = -1;
					}
				}
				else if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
					if (module_num < 0)  {
						continue;
					}
					save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
					i =  strtol(subfield.value, (char **)0, 10);
					subfield.value[subfield.length] = save_byte;
					if (i <= attrib_files[module_num].num_attrib)  {
						for (j = 0; j < MAX_EXTRA; j++)  {
							if (attrib_files[module_num].attrib[i - 1].major[j] != 0)  {
								*current_attrib = (struct attribute *)malloc(sizeof(struct attribute));
								if (*current_attrib == (struct attribute *)0)  {
									fprintf(stderr, "malloc failed\n");
									exit(0);
								}
//								*current_attrib2 = (struct attribute *)malloc(sizeof(struct attribute));
//								if (*current_attrib2 == (struct attribute *)0)  {
//									fprintf(stderr, "malloc failed\n");
//									exit(0);
//								}

								(*current_attrib)->major = attrib_files[module_num].attrib[i - 1].major[j];	// line attribute entry
								(*current_attrib)->minor = attrib_files[module_num].attrib[i - 1].minor[j];

//								(*current_attrib2)->major = attrib_files[module_num].attrib[i - 1].major[j];	// node attribute entry
//								(*current_attrib2)->minor = attrib_files[module_num].attrib[i - 1].minor[j];

								current_attrib = &((*current_attrib)->attribute);
//								current_attrib2 = &((*current_attrib2)->attribute);
								attrib++;
							}
						}
					}
				}
			}
			else if (strcmp(subfield.tag, "ARID") == 0)  {
				if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
					save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
					i =  strtol(subfield.value, (char **)0, 10);
					subfield.value[subfield.length] = save_byte;
					lines[num_lines].left_area = i;
					lines[num_lines].right_area = i;
				}
			}
			else if (strcmp(subfield.tag, "SADR") == 0)  {
				/*
				 * We assume that the X coordinate always comes first.
				 */
				if ((strstr(subfield.format, "B") != (char *)0) && (strcmp(subfield.label, "X") == 0))  {
					/* We must add an extra endpoint, so get two storage slots instead of one. */
					*current_point = (struct point *)malloc(sizeof(struct point));
					if (*current_point == (struct point *)0)  {
						fprintf(stderr, "malloc failed\n");
						exit(0);
					}
					current_point2 = &((*current_point)->point);
					*current_point2 = (struct point *)malloc(sizeof(struct point));
					if (*current_point2 == (struct point *)0)  {
						fprintf(stderr, "malloc failed\n");
						exit(0);
					}
	
					if (subfield.length != 4)  {
						/* Error */
						nodes[num_nodes].x = -1.0;
						(*current_point)->x = -1.0;
						(*current_point2)->x = -1.0;
					}
					else  {
						i = (((int32_t)subfield.value[3] & 0xff) << 24) |
						    (((int32_t)subfield.value[2] & 0xff) << 16) |
						    (((int32_t)subfield.value[1] & 0xff) <<  8) |
						     ((int32_t)subfield.value[0] & 0xff);
						if (byte_order == 0)  {
							nodes[num_nodes].x = (double)i * x_scale_factor + x_origin;
							(*current_point)->x = nodes[num_nodes].x;
							(*current_point2)->x = nodes[num_nodes].x;
						}
						else if (byte_order == 1)  {
							LE_SWAB(&i);
							nodes[num_nodes].x = (double)i * x_scale_factor + x_origin;
							(*current_point)->x = nodes[num_nodes].x;
							(*current_point2)->x = nodes[num_nodes].x;
						}
						else if (byte_order == 2)  {
							PDP_SWAB(&i);
							nodes[num_nodes].x = (double)i * x_scale_factor + x_origin;
							(*current_point)->x = nodes[num_nodes].x;
							(*current_point2)->x = nodes[num_nodes].x;
						}
					}
				}
				else if ((strstr(subfield.format, "B") != (char *)0) && (strcmp(subfield.label, "Y") == 0))  {
					if (subfield.length != 4)  {
						/* Error */
						nodes[num_nodes].y = -1.0;
						(*current_point)->y = -1.0;
						(*current_point2)->y = -1.0;
					}
					else  {
						i = (((int32_t)subfield.value[3] & 0xff) << 24) |
						    (((int32_t)subfield.value[2] & 0xff) << 16) |
						    (((int32_t)subfield.value[1] & 0xff) <<  8) |
						     ((int32_t)subfield.value[0] & 0xff);
						if (byte_order == 0)  {
							nodes[num_nodes].y = (double)i * y_scale_factor + y_origin;
							(*current_point)->y = nodes[num_nodes].y;
							(*current_point2)->y = nodes[num_nodes].y;
						}
						else if (byte_order == 1)  {
							LE_SWAB(&i);
							nodes[num_nodes].y = (double)i * y_scale_factor + y_origin;
							(*current_point)->y = nodes[num_nodes].y;
							(*current_point2)->y = nodes[num_nodes].y;
						}
						else if (byte_order == 2)  {
							PDP_SWAB(&i);
							nodes[num_nodes].y = (double)i * y_scale_factor + y_origin;
							(*current_point)->y = nodes[num_nodes].y;
							(*current_point2)->y = nodes[num_nodes].y;
						}
					}
	
					current_point = &((*current_point2)->point);
					count++;
					count++;
				}
			}
		}
		if (num_nodes >= 0)  {
			/*
			 * If we had at least one node
			 * then close out the attribute information of the
			 * previous line and node and update the counts.
			 */
			*current_attrib = (struct attribute *)0;
			*current_point = (struct point *)0;
			lines[num_lines].number_attrib = attrib;
			lines[num_lines].number_coords = count;
			uniq_attrib(&lines[num_lines].attribute, &lines[num_lines].number_attrib);
	
//			*current_attrib2 = (struct attribute *)0;
//			nodes[num_nodes].number_attrib = attrib;
//			uniq_attrib(&nodes[num_nodes].attribute, &nodes[num_nodes].number_attrib);
// For degenerate lines, the nodes don't appear to have the attributes attached.
// Thus, we simply zero out the attribute count.  However, we will leave the
// code in place for now, that keeps an attribute list for the nodes, just in
// case I am misunderstanding something.
			nodes[num_nodes].number_attrib = 0;
		}
		num_lines++;
		num_nodes++;
		/* We are done with this file, so close it. */
		end_ddf();
	}



	/*
	 * Before we can process the areas, we need to read the Polygon module
	 * and find out which attributes are associated with each polygon.
	 * Then we can read the NA?? module and associate the Area representative
	 * points, from the NA?? module, with specific attributes, referenced in
	 * the polygon (PC??) module, via the polygon linkage in the NA?? module.
	 */
	strncpy(file_name, passed_file_name, MAX_FILE_NAME);
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			file_name[file_name_length - 11] = 'p';
			file_name[file_name_length - 10] = 'c';
		}
		else  {
			file_name[file_name_length -  8] = 'p';
			file_name[file_name_length -  7] = 'c';
		}
	}
	else  {
		if (gz_flag != 0)  {
			file_name[file_name_length - 11] = 'P';
			file_name[file_name_length - 10] = 'C';
		}
		else  {
			file_name[file_name_length -  8] = 'P';
			file_name[file_name_length -  7] = 'C';
		}
	}
	/*
	 * Open the file in preparation for parsing.
	 */
	num_polys = 0;
	if (begin_ddf(file_name) >= 0)  {
		/*
		 * Loop through the subfields until we find what we want.
		 */
		while (get_subfield(&subfield) != 0)  {
			if (strcmp(subfield.tag, "POLY") == 0)  {
				if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
					save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
					current_poly =  strtol(subfield.value, (char **)0, 10);
					subfield.value[subfield.length] = save_byte;
				}
			}
			else if (strcmp(subfield.tag, "ATID") == 0)  {
				if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "MODN") == 0))  {
					if (subfield.length != 4)  {
						fprintf(stderr, "Attribute module name (%.*s) is not 4 characters long.\n", subfield.length, subfield.value);
						module_num = -1;
						continue;
					}
					for (module_num = 0; module_num < num_attrib_files; module_num++)  {
						if (strncmp(subfield.value, attrib_files[module_num].module_name, 4) == 0)  {
							break;
						}
					}
					if (module_num == num_attrib_files)  {
						fprintf(stderr, "Warning:  Attribute module has unexpected name (%.*s).  Attributes may be in error.\n", subfield.length, subfield.value);
						module_num = -1;
					}
				}
				else if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
					if (module_num < 0)  {
						continue;
					}
					if (num_polys >= MAX_POLY_NUM)  {
						fprintf(stderr, "Ran out of polygon space.  Some attributes may not show up.\n");
						break;
					}
					polygon_attrib[num_polys].poly_id = current_poly;
					save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
					polygon_attrib[num_polys].attrib = strtol(subfield.value, (char **)0, 10);
					subfield.value[subfield.length] = save_byte;
					polygon_attrib[num_polys++].module_num = module_num;
				}
			}
		}
		/* We are done with this file, so close it. */
		end_ddf();
	}


	/*
	 * Now read in the Areas Module and store the data.
	 *
	 * Before we do, though, generate an entry for area 1.
	 * This is the neatline polygon that surrounds the data
	 * area.  It is not encoded in the NA??  module,
	 * but there is an entry for the polygon in
	 * the PC?? module.  Since we won't find it while searching
	 * the NA?? module, and since we need it for a complete
	 * DLG-3 file, insert it artificially at the beginning
	 * of the array.  There does not appear to be any way
	 * to recover the original pre-SDTS representative point for this
	 * area, so just insert the southwest registration point,
	 * since the few sample files I looked at seemed to use a
	 * representative point near that corner.
	 */
	num_areas++;
	areas[num_areas].id = 1;
	areas[num_areas].x = sw_x;
	areas[num_areas].y = sw_y;
	areas[num_areas].number_attrib = 0;
	areas[num_areas].attribute = (struct attribute *)0;

	strncpy(file_name, passed_file_name, MAX_FILE_NAME);
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			file_name[file_name_length - 11] = 'n';
			file_name[file_name_length - 10] = 'a';
		}
		else  {
			file_name[file_name_length -  8] = 'n';
			file_name[file_name_length -  7] = 'a';
		}
	}
	else  {
		if (gz_flag != 0)  {
			file_name[file_name_length - 11] = 'N';
			file_name[file_name_length - 10] = 'A';
		}
		else  {
			file_name[file_name_length -  8] = 'N';
			file_name[file_name_length -  7] = 'A';
		}
	}
	/*
	 * Open the file in preparation for parsing.
	 */
	if (begin_ddf(file_name) >= 0)  {
		/*
		 * Loop through the subfields until we find what we want.
		 */
		attrib = -1;	// Use this convenient variable as a non-related flag for first trip through loop.
		while (get_subfield(&subfield) != 0)  {
			if (strcmp(subfield.tag, "PNTS") == 0)  {
				if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
					/* We are starting a new line.  Initialize what need initializing. */
					if (attrib >= 0)  {
						/*
						 * If we aren't starting the first area,
						 * then terminate the attribute string of the
						 * previous area and update the counts.
						 */
						*current_attrib = (struct attribute *)0;
						areas[num_areas].number_attrib = attrib;
						uniq_attrib(&areas[num_areas].attribute, &areas[num_areas].number_attrib);
					}
					num_areas++;
					attrib = 0;
					if (num_areas >= MAX_AREAS)  {
						fprintf(stderr, "Ran out of space to store areas.  Some areas may be missing.\n");
						break;
					}
					current_attrib = &areas[num_areas].attribute;
					save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
					areas[num_areas].id = strtol(subfield.value, (char **)0, 10);
					subfield.value[subfield.length] = save_byte;
				}
			}
			else if (strcmp(subfield.tag, "SADR") == 0)  {
				/*
				 * We assume that the X coordinate always comes first.
				 */
				if ((strstr(subfield.format, "B") != (char *)0) && (strcmp(subfield.label, "X") == 0))  {
					if (subfield.length != 4)  {
						/* Error */
						areas[num_areas].x = -1.0;
					}
					else  {
						i = (((int32_t)subfield.value[3] & 0xff) << 24) |
						    (((int32_t)subfield.value[2] & 0xff) << 16) |
						    (((int32_t)subfield.value[1] & 0xff) <<  8) |
						     ((int32_t)subfield.value[0] & 0xff);
						if (byte_order == 0)  {
							areas[num_areas].x = (double)i * x_scale_factor + x_origin;
						}
						else if (byte_order == 1)  {
							LE_SWAB(&i);
							areas[num_areas].x = (double)i * x_scale_factor + x_origin;
						}
						else if (byte_order == 2)  {
							PDP_SWAB(&i);
							areas[num_areas].x = (double)i * x_scale_factor + x_origin;
						}
					}
				}
				else if ((strstr(subfield.format, "B") != (char *)0) && (strcmp(subfield.label, "Y") == 0))  {
					if (subfield.length != 4)  {
						/* Error */
						areas[num_areas].y = -1.0;
					}
					else  {
						i = (((int32_t)subfield.value[3] & 0xff) << 24) |
						    (((int32_t)subfield.value[2] & 0xff) << 16) |
						    (((int32_t)subfield.value[1] & 0xff) <<  8) |
						     ((int32_t)subfield.value[0] & 0xff);
						if (byte_order == 0)  {
							areas[num_areas].y = (double)i * y_scale_factor + y_origin;
						}
						else if (byte_order == 1)  {
							LE_SWAB(&i);
							areas[num_areas].y = (double)i * y_scale_factor + y_origin;
						}
						else if (byte_order == 2)  {
							PDP_SWAB(&i);
							areas[num_areas].y = (double)i * y_scale_factor + y_origin;
						}
					}
				}
			}
			else if (strcmp(subfield.tag, "ARID") == 0)  {
				if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
					save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
					i =  strtol(subfield.value, (char **)0, 10);
					subfield.value[subfield.length] = save_byte;
	
					for (j = 0; j < num_polys; j++)  {
						if (polygon_attrib[j].poly_id == i)  {
							if (polygon_attrib[j].attrib <= attrib_files[(int32_t)polygon_attrib[j].module_num].num_attrib)  {
								for (k = 0; k < MAX_EXTRA; k++)  {
									if (attrib_files[(int32_t)polygon_attrib[j].module_num].attrib[polygon_attrib[j].attrib - 1].major[k] != 0)  {
										*current_attrib = (struct attribute *)malloc(sizeof(struct attribute));
										if (*current_attrib == (struct attribute *)0)  {
											fprintf(stderr, "malloc failed\n");
											exit(0);
										}
	
										(*current_attrib)->major = attrib_files[(int32_t)polygon_attrib[j].module_num].attrib[polygon_attrib[j].attrib - 1].major[k];
										(*current_attrib)->minor = attrib_files[(int32_t)polygon_attrib[j].module_num].attrib[polygon_attrib[j].attrib - 1].minor[k];
	
										current_attrib = &((*current_attrib)->attribute);
										attrib++;
									}
								}
							}
						}
					}
				}
			}
		}
		if (num_areas >= 0)  {
			/*
			 * If we had at least one area
			 * then close out the attribute and node information of the
			 * previous area and update the counts.
			 */
			*current_attrib = (struct attribute *)0;
			areas[num_areas].number_attrib = attrib;
			uniq_attrib(&areas[num_areas].attribute, &areas[num_areas].number_attrib);
		}
		/* We are done with this file, so close it. */
		end_ddf();
	}
	num_areas++;



	/*
	 * Because the various cross-links between Nodes, Lines, and Areas depend
	 * on the Record IDs, we are faced with a choice.  Either we make sure
	 * that each array index corresponds to the Record ID stored therein,
	 * or we have to search the array every time we want a specific record.
	 * We choose to do the former, so that the Record ID stored in array
	 * element i is equal to i + 1.
	 *
	 * Rather than try to fill the arrays based on record IDs, and have to
	 * do error checking to find empty slots in the arrays, we choose to
	 * pack records into the arrays without regard for ordering.  After the
	 * arrays are full, we sort them into order with qsort().  This should
	 * be reasonably low-cost, in terms of CPU time, because the arrays
	 * should be pretty close to the correct order before we start.
	 *
	 * The areas array should be in order and won't need sorting.
	 * The lines array may have some deviations from correct
	 * ordering because the degenerate lines are stored in a separate file (the NE?? file),
	 * and read in separately at the end of the array.
	 *
	 * The nodes array will not have the record-ID-to-index correspondence
	 * because it results from reading both the NE?? module and the NO?? module,
	 * with the corresponding records appearing in two separate groups in the
	 * array.
	 *
	 * We won't sort the nodes array with qsort.  This is because the degenerate lines,
	 * from the NE?? module, have the same Record IDs as the associated entry in
	 * the lines array.  (In other words, in the original data, Node 42 and Line
	 * 243 may have both represented the same degenerate line.  In the SDTS data,
	 * both the line and the node have Record ID 243.  (I think this may be a bug
	 * in the USGS conversion procedure, but maybe they did it on purpose.)  The
	 * big problem with this is that we could easily have a node with the same
	 * Record ID of 243 in the NO?? module, resulting in two nodes in the nodes array
	 * that have the same ID.)  We will reconstruct the original node
	 * numbers and sort the array at the same time.
	 * We do this by assuming that holes in the NO?? node numbering should be
	 * sequentially filled by the points that were placed into the NE?? module.
	 */
	qsort(lines, num_lines, sizeof(struct lines), compare_lines);



	/*
	 * All of the useful data is parsed.
	 * Now do something with it.
	 *
	 * Either enter the big block of code that writes all of the
	 * data to an optional-format DLG file, or enter the big block
	 * of code that writes the data to the image buffer.
	 *
	 * We sort the nodes array within the first big block, because
	 * the node data isn't used when drawing a map.  If we ever
	 * start using the node data for maps, then we will have to
	 * move the sorting up to where we do the qsort() above.
	 */
	if (file_image_flag != 0)  {
		/*
		 * This is the big block of code that writes the data
		 * to an optional-format DLG file.
		 *
		 * We now have enough information to write Record 15.
		 */
		sprintf(buf, "%-20.20s   0%6d%6d 010%6d%6d 010%6d%6d   1        ",
			category_name, num_nodes, num_nodes, num_areas, num_areas, num_lines, num_lines);
		if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
			fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
			exit(0);
		}



		/*
		 * Now we are ready to write the node records to the file.
		 * Begin by reconstructing the node list.
		 * Fold all of the degenerate-line nodes into
		 * the missing node slots in the node list.
		 */
		if (num_nodes > num_NO_nodes)  {
			if (num_nodes >= MAX_NODES)  {	// We need one extra node slot for temporary storage
				fprintf(stderr, "Ran out of space to store nodes.  Non-recoverable error.\n");
				exit(0);
			}
			i = num_NO_nodes;
			for (j = 0; j < num_nodes; j++)  {
				if (nodes[j].id != (j + 1))  {
					nodes[num_nodes].id = nodes[i].id;
					nodes[num_nodes].x = nodes[i].x;
					nodes[num_nodes].y = nodes[i].y;
					nodes[num_nodes].number_attrib = nodes[i].number_attrib;
					nodes[num_nodes].attribute = nodes[i].attribute;
					for (k = i; k > j; k--)  {
						nodes[k].id = nodes[k - 1].id;
						nodes[k].x = nodes[k - 1].x;
						nodes[k].y = nodes[k - 1].y;
						nodes[k].number_attrib = nodes[k - 1].number_attrib;
						nodes[k].attribute = nodes[k - 1].attribute;
					}
					nodes[j].id = nodes[num_nodes].id;
					nodes[j].x = nodes[num_nodes].x;
					nodes[j].y = nodes[num_nodes].y;
					nodes[j].number_attrib = nodes[num_nodes].number_attrib;
					nodes[j].attribute = nodes[num_nodes].attribute;
					lines[nodes[j].id - 1].end_node = j + 1;
					lines[nodes[j].id - 1].start_node = j + 1;
					nodes[j].id = j + 1;
					i++;
					if (i == num_nodes)  {
						break;
					}
				}
			}
		}
		if (nodes[num_nodes - 1].id != num_nodes)  {
			fprintf(stderr, "Warning:  The node section may have some problems.\n");
		}
		/*
		 * Now go through the patched up nodes, and build a line list for
		 * each node, and then print out the node record.
		 * (The line list consists of all linear features for which this node is an endpoint.
		 * The lines appear in no particular order.)
		 */
		for (i = 0; i < num_nodes; i++)  {
			line_list_size = -1;
			for (j = 0; j < num_lines; j++)  {
				if (lines[j].start_node == nodes[i].id)  {
					if ((line_list_size + 1) == MAX_LINE_LIST)  {
						fprintf(stderr, "Ran out of space for a nodal line list (node %d).  Some lines are missing.\n", i + 1);
						break;
					}
					line_list_size++;
					line_list[line_list_size] = lines[j].id;
				}
				/* Note:  This is not an "else if" because degenerate lines must appear twice in the list. */
				if (lines[j].end_node == nodes[i].id)  {
					if ((line_list_size + 1) == MAX_LINE_LIST)  {
						fprintf(stderr, "Ran out of space for a nodal line list (node %d).  Some lines are missing.\n", i + 1);
						break;
					}
					line_list_size++;
					line_list[line_list_size] = -lines[j].id;
				}
			}
			line_list_size++;

			/*
			 * Print the first record of the node.
			 */
			sprintf(buf, "N%5d%12.2f%12.2f      %6d      %6d     0                    ",
				i + 1, nodes[i].x, nodes[i].y, line_list_size, nodes[i].number_attrib);
			if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
				fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
				exit(0);
			}

			/*
			 * Print the line-list records.
			 */
			j = 0;
			for (k = 0; k < line_list_size; k++)  {
				sprintf(&buf[j], "%6d", line_list[k]);
				j = j + 6;
				if (j == 72)  {
					for ( ; j < DLG_RECORD_LENGTH; j++)  {
						buf[j] = ' ';
					}
					if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
						fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
						exit(0);
					}
					j = 0;
				}
			}
			if (j > 0)  {
				for ( ; j < DLG_RECORD_LENGTH; j++)  {
					buf[j] = ' ';
				}
				if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
					fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
					exit(0);
				}
			}

			/*
			 * Print the attribute records.
			 */
			j = 0;
			current_attrib = &nodes[i].attribute;
			for (k = 0; k < nodes[i].number_attrib; k++)  {
				sprintf(&buf[j], "%6d%6d", (*current_attrib)->major, (*current_attrib)->minor);
				j = j + 12;
				if (j == 72)  {
					for ( ; j < DLG_RECORD_LENGTH; j++)  {
						buf[j] = ' ';
					}
					if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
						fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
						exit(0);
					}
					j = 0;
				}
				current_attrib = &((*current_attrib)->attribute);
			}
			if (j > 0)  {
				for ( ; j < DLG_RECORD_LENGTH; j++)  {
					buf[j] = ' ';
				}
				if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
					fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
					exit(0);
				}
			}
		}

		/*
		 * Now go through the areas, and build a line list for
		 * each area, and then print out the area record.
		 * (The line list is a list of the lines that bound this area.
		 * The lines should appear in clockwise order around the perimeter
		 * of the area.  If there are islands, their sublists are delimited by
		 * inserting a line number of zero into the list ahead of them.
		 * Island nodes are listed in counterclockwise order.)
		 */
		for (i = 0; i < num_areas; i++)  {
			line_list_size = -1;
			for (j = 0; j < num_lines; j++)  {
				if (lines[j].left_area == lines[j].right_area)  {
					continue;
				}
				if ((lines[j].right_area == areas[i].id) || (lines[j].left_area == areas[i].id))  {
					if ((line_list_size + 2) == MAX_LINE_LIST)  {	// Reserve an empty spot at the end of the list for later.
						fprintf(stderr, "Ran out of space for an areal line list.  Some lines are missing for area %d.\n", i + 1);
						break;
					}
					line_list_size++;
					line_list[line_list_size] = lines[j].id;
				}
			}
			line_list_size++;

			/*
			 * Now we have the line list, but it still must be sorted
			 * so that the linear features are contiguous.
			 *
			 * First, though, we want to make sure that we get the
			 * primary area first in the list, with any islands coming after it
			 * in the list.  In order to do this, we search for the
			 * point in the line list that has the greatest y value,
			 * and we put that point first in the list.  (No matter what
			 * the shape of the enclosing polygon, no island should have
			 * any points that are as far north as the northernmost point
			 * in the enclosing polygon.)  The sorting
			 * algorithm will pull contiguous points toward this first point,
			 * and thus pull the primary area toward the front of the list.
			 */
			y = -11000000.0;
			for (j = 0; j < line_list_size; j++)  {
				current_point = &lines[line_list[j] - 1].point;
				while (*current_point != (struct point *)0)  {
					if ((*current_point)->y > y)  {
						y = (*current_point)->y;
						k = j;
					}
					current_point = &((*current_point)->point);
				}
			}
			j = line_list[k];
			line_list[k] = line_list[0];
			line_list[0] = j;
			/*
			 * We have the northernmost point first in the list.
			 * Now do the sorting.
			 *
			 * Whether sorting the main polygon, or an island,
			 * the nodes are ordered so that the referenced area
			 * is always on the right as we traverse each linear
			 * component of the boundary.
			 */
			number_of_islands = 0;
			for (j = 0; j < line_list_size; j++)  {
				if (lines[line_list[j] - 1].right_area == areas[i].id)  {
					start_node = lines[line_list[j] - 1].start_node;
					current_node = lines[line_list[j] - 1].end_node;
				}
				else  {
					start_node = lines[line_list[j] - 1].end_node;
					current_node = lines[line_list[j] - 1].start_node;
				}
				if (start_node != current_node)  {
					for (k = j + 1; k < line_list_size; k++)  {
						if (lines[line_list[k] - 1].right_area == areas[i].id)  {
							if (lines[line_list[k] - 1].start_node == current_node)  {
								line_list[MAX_LINE_LIST - 1] = line_list[j + 1];
								line_list[j + 1] = line_list[k];
								line_list[k] = line_list[MAX_LINE_LIST - 1];
								break;
							}
						}
						else  {
							if (lines[line_list[k] - 1].end_node == current_node)  {
								line_list[MAX_LINE_LIST - 1] = line_list[j + 1];
								line_list[j + 1] = line_list[k];
								line_list[k] = line_list[MAX_LINE_LIST - 1];
								break;
							}
						}
					}
				}
				if (lines[line_list[j] - 1].left_area == areas[i].id)  {
					line_list[j] = - line_list[j];
				}
				if (((start_node == current_node) || (k == line_list_size)) && (j < (line_list_size - 1)))  {
					if ((line_list_size + 2) == MAX_LINE_LIST)  {	// Reserve an empty spot at the end of the list for later.
						fprintf(stderr, "Ran out of space for an areal line list.  There may be errors in the line list for area %d.\n", i + 1);
						break;
					}
					j++;
					line_list[line_list_size] = line_list[j];
					line_list[j] = 0;
					line_list_size++;
					number_of_islands++;
				}
			}

			/*
			 * Print the first record of the area.
			 *
			 * Special case for the Universe polygon, which needs a single attribute
			 * that isn't stored in the attribute list.  It isn't in the list because
			 * we artificially generated the Universe polygon from scratch before reading
			 * in the NA?? module.
			 */
			if (i == 0)  {
				sprintf(buf, "A%5d%12.2f%12.2f      %6d     0%6d     0%6d              ",
					i + 1, areas[i].x, areas[i].y, line_list_size, 1, number_of_islands);
			}
			else  {
				sprintf(buf, "A%5d%12.2f%12.2f      %6d     0%6d     0%6d              ",
					i + 1, areas[i].x, areas[i].y, line_list_size, areas[i].number_attrib, number_of_islands);
			}
			if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
				fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
				exit(0);
			}

			/*
			 * Print the line-list records.
			 */
			j = 0;
			for (k = 0; k < line_list_size; k++)  {
				sprintf(&buf[j], "%6d", line_list[k]);
				j = j + 6;
				if (j == 72)  {
					for ( ; j < DLG_RECORD_LENGTH; j++)  {
						buf[j] = ' ';
					}
					if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
						fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
						exit(0);
					}
					j = 0;
				}
			}
			if (j > 0)  {
				for ( ; j < DLG_RECORD_LENGTH; j++)  {
					buf[j] = ' ';
				}
				if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
					fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
					exit(0);
				}
			}

			/*
			 * Print the attribute records.
			 */
			j = 0;
			current_attrib = &areas[i].attribute;
			for (k = 0; k < areas[i].number_attrib; k++)  {
				sprintf(&buf[j], "%6d%6d", (*current_attrib)->major, (*current_attrib)->minor);
				j = j + 12;
				if (j == 72)  {
					for ( ; j < DLG_RECORD_LENGTH; j++)  {
						buf[j] = ' ';
					}
					if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
						fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
						exit(0);
					}
					j = 0;
				}
				current_attrib = &((*current_attrib)->attribute);
			}
			if (j > 0)  {
				for ( ; j < DLG_RECORD_LENGTH; j++)  {
					buf[j] = ' ';
				}
				if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
					fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
					exit(0);
				}
			}
			/*
			 * Special case for the Universe polygon, which is
			 * supposed to have an attribute of 0, 0.
			 */
			if (i == 0)  {
				sprintf(buf, "%6d%6d", 0, 0);
				for (j = 12 ; j < DLG_RECORD_LENGTH; j++)  {
					buf[j] = ' ';
				}
				if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
					fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
					exit(0);
				}
			}
		}

		/*
		 * Now go through the lines, and print out the records.
		 */
		for (i = 0; i < num_lines; i++)  {
			/*
			 * Print the first record of the node.
			 */
			sprintf(buf, "L%5d%6d%6d%6d%6d            %6d%6d     0                    ",
				i + 1, lines[i].start_node, lines[i].end_node, lines[i].left_area, lines[i].right_area,
				lines[i].number_coords, lines[i].number_attrib);
			if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
				fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
				exit(0);
			}

			/*
			 * Print the coordinate-list records.
			 */
			j = 0;
			current_point = &lines[i].point;
			for (k = 0; k < lines[i].number_coords; k++)  {
				sprintf(&buf[j], "%12.2f%12.2f", (*current_point)->x, (*current_point)->y);
				j = j + 24;
				if (j == 72)  {
					for ( ; j < DLG_RECORD_LENGTH; j++)  {
						buf[j] = ' ';
					}
					if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
						fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
						exit(0);
					}
					j = 0;
				}
				current_point = &((*current_point)->point);
			}
			if (j > 0)  {
				for ( ; j < DLG_RECORD_LENGTH; j++)  {
					buf[j] = ' ';
				}
				if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
					fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
					exit(0);
				}
			}

			/*
			 * Print the attribute records.
			 */
			j = 0;
			current_attrib = &lines[i].attribute;
			for (k = 0; k < lines[i].number_attrib; k++)  {
				sprintf(&buf[j], "%6d%6d", (*current_attrib)->major, (*current_attrib)->minor);
				j = j + 12;
				if (j == 72)  {
					for ( ; j < DLG_RECORD_LENGTH; j++)  {
						buf[j] = ' ';
					}
					if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
						fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
						exit(0);
					}
					j = 0;
				}
				current_attrib = &((*current_attrib)->attribute);
			}
			if (j > 0)  {
				for ( ; j < DLG_RECORD_LENGTH; j++)  {
					buf[j] = ' ';
				}
				if (write(output_fdesc, buf, DLG_RECORD_LENGTH) != DLG_RECORD_LENGTH)  {
					fprintf(stderr, "Failed to write output file.  errno = %d.\n", errno);
					exit(0);
				}
			}
		}
		close(output_fdesc);
	}
	else  {
		/*
		 * This is the big block of code that writes the data
		 * to the drawmap image buffer.
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
			 * (In SDTS, this is apparently no longer encodes as an area,
			 * but is stored in connection with the definition of the data boundaries.)
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
	for (i = 0; i < num_nodes; i++)  {
		if (nodes[i].number_attrib > 0)  {
			current_attrib = &nodes[i].attribute;

			while (*current_attrib != (struct attribute *)0)  {
				tmp_attrib = (*current_attrib)->attribute;
				free(*current_attrib);
				*current_attrib = tmp_attrib;
			}
		}
	}
	for (i = 0; i < num_attrib_files; i++)  {
		free(attrib_files[i].attrib);
	}

	return 0;
}





/*
 * Feature definitions for the additional
 * features associated with primary attributes.
 */
#define NUM_FEATURES 117
struct	features  {
	int32_t key;		// An internal key used to keep track of the entries
	int32_t main_major;	// The major attribute number for this category
	int32_t major;		// The major attribute number for this category, modified to suit individual features
	int32_t minor;		// The minor attribute number for this feature
	char *feature_name;
	int32_t feature_name_length;
} features[NUM_FEATURES] =  {
{   0,	 20,	 20,	202,	"SUPPLEMENTARY",	13, },	// Hypsography
{   1,	 20,	 20,	204,	"AMENDED",		 7, },
{   2,	 20,	 20,	610,	"APPROXIMATE",		11, },
{   3,	 20,	 20,	611,	"DEPRESSION",		10, },
{   4,	 20,	 20,	612,	"GLACIER_OR_SNOW",	15, },
{   5,	 20,	 20,	613,	"UNDERWATER",		10, },
{   6,	 20,	 20,	614,	"BEST_ESTIMATE",	13, },
{   7,	 20,	 26,	 -1,	"SPOT_CATEGORY",	13, },
{   8,	 20,	 26,	  0,	"PHOTOREVISED",		12, },
{   9,	 50,	 50,	  0,	"PHOTOREVISED",		12, },	// Hydrography
{  10,	 50,	 50,	 -1,	"RELATION_TO_GROUND",	18, },
{  11,	 50,	 50,	 -1,	"VERTICAL_RELATION",	17, },
{  12,	 50,	 50,	 -1,	"BANK",			 4, },
{  13,	 50,	 50,	 -1,	"OPERATIONAL_STATUS",	18, },
{  14,	 50,	 50,	608,	"SALT",			 4, },
{  15,	 50,	 50,	609,	"UNSURVEYED",		10, },
{  16,	 50,	 50,	610,	"INTERMITTENT",		12, },
{  17,	 50,	 50,	612,	"SUBMERGED",		 9, },
{  18,	 50,	 50,	614,	"DRY",			 3, },
{  19,	 50,	 50,	615,	"MINERAL_OR_HOT",	14, },
{  20,	 50,	 50,	616,	"NAVIGABLE",		 9, },
{  21,	 50,	 50,	618,	"EARTHEN",		 7, },
{  22,	 50,	 50,	619,	"INTERPOLATED",		12, },
{  23,	 50,	 -1,	 -1,	"ELEVATION",		 9, },
{  24,	 50,	 53,	 -1,	"ROTATION_ANGLE",	14, },
{  25,	 50,	 55,	 -1,	"RIVER_MILE",		10, },
{  26,	 50,	 58,	  0,	"BEST_ESTIMATE",	13, },
{  27,	 70,	 78,	  0,	"BEST_ESTIMATE",	13, },	// Vegetative Surface Cover
{  28,	 80,	 80,	  0,	"PHOTOREVISED",		12, },	// Non-Vegetative Features
{  29,	 80,	 88,	  0,	"BEST_ESTIMATE",	13, },
{  30,	 90,	 90,	100,	"CIVIL_TOWNSHIP",	14, },	// Boundaries
{  31,	 90,	 90,	101,	"CITY",			 4, },
{  32,	 90,	 90,	104,	"NATIONAL_FOREST",	15, },
{  33,	 90,	 90,	106,	"WILDERNESS_AREA",	15, },
{  34,	 90,	 90,	135,	"AHUPUAA",		 7, },
{  35,	 90,	 90,	136,	"HAWAIIAN_HOMESTEAD",	18, },
{  36,	 90,	 90,	401,	"FEDERALLY_ADMIN",	15, },
{  37,	 90,	 90,	601,	"IN_DISPUTE",		10, },
{  38,	 90,	 91,	 -1,	"STATE",		 5, },
{  39,	 90,	 92,	 -1,	"COUNTY",		 6, },
{  40,	 90,	 -1,	 -1,	"TOWNSHIP_CODE",	13, },
{  41,	 90,	 90,	  0,	"PHOTOREVISED",		12, },
{  42,	 90,	 -1,	 -1,	"MONUMENT_NUMBER",	15, },
{  43,	 90,	 98,	  0,	"BEST_ESTIMATE",	13, },
{  44,	150,	151,	 -1,	"STATE",		 5, },	// Survey Control
{  45,	150,	152,	 -1,	"COUNTY",		 6, },
{  46,	150,	 -1,	 -1,	"ELEVATION",		 9, },
{  47,	170,	170,	216,	"ARBITRARY_EXT",	13, },	// Roads and Trails
{  48,	170,	170,	 -1,	"RELATION_TO_GROUND",	18, },
{  49,	170,	170,	 -1,	"VERTICAL_RELATION",	17, },
{  50,	170,	170,	 -1,	"OPERATIONAL_STATUS",	18, },
{  51,	170,	170,	 -1,	"ACCESS_RESTRICTION",	18, },
{  52,	170,	170,	605,	"OLD_RAILROAD_GRADE",	18, },
{  53,	170,	170,	623,	"WITH_RAILROAD",	13, },
{  54,	170,	170,	624,	"COVERED",		 7, },
{  55,	170,	170,	600,	"HISTORICAL",		10, },
{  56,	170,	170,	608,	"LIMITED_ACCESS",	14, },
{  57,	170,	170,	  0,	"PHOTOREVISED",		12, },
{  58,	170,	171,	 -1,	"LANES",		 5, },
{  59,	170,	170,	 -1,	"ROAD_WIDTH",		10, },
{  60,	170,	178,	  0,	"BEST_ESTIMATE",	13, },
{  61,	180,	180,	 -1,	"RELATION_TO_GROUND",	18, },	// Railroads
{  62,	180,	180,	 -1,	"VERTICAL_RELATION",	17, },
{  63,	180,	180,	 -1,	"OPERATIONAL_STATUS",	18, },
{  64,	180,	180,	 -1,	"ACCESS_RESTRICTIONS",	19, },
{  65,	180,	180,	606,	"NARROW_GAUGE",		12, },
{  66,	180,	180,	607,	"IN_SNOWSHED",		11, },
{  67,	180,	180,	610,	"RAPID_TRANSIT",	13, },
{  68,	180,	180,	614,	"JUXTAPOSITION",	13, },
{  69,	180,	180,	210,	"ARBITRARY_EXT",	13, },
{  70,	180,	180,	600,	"HISTORICAL",		10, },
{  71,	180,	180,	  0,	"PHOTOREVISED",		12, },
{  72,	180,	181,	 -1,	"TRACKS",		 6, },
{  73,	180,	183,	 -1,	"ROTATION_ANGLE",	14, },
{  74,	180,	188,	  0,	"BEST_ESTIMATE",	13, },
{  75,	190,	190,	 -1,	"RELATION_TO_GROUND",	18, },	// Pipelines
{  76,	190,	190,	 -1,	"OPERATIONAL_STATUS",	18, },
{  77,	190,	190,	605,	"UNIMPROVED",		10, },
{  78,	190,	190,	607,	"NUCLEAR",		 7, },
{  79,	190,	190,	205,	"ARBITRARY_EXT",	13, },
{  80,	190,	190,	  0,	"PHOTOREVISED",		12, },
{  81,	190,	193,	 -1,	"ROTATION_ANGLE",	14, },
{  82,	190,	198,	  0,	"BEST_ESTIMATE",	13, },
{  83,	190,	196,	 -1,	"STATE",		 5, },
{  84,	190,	197,	 -1,	"AIRPORT",		 7, },
{  85,	200,	200,	 -1,	"RELATION_TO_GROUND",	18, },	// Manmade Features
{  86,	200,	200,	 -1,	"OPERATIONAL_STATUS",	18, },
{  87,	200,	200,	 -1,	"PRODUCT",		 7, },
{  88,	200,	200,	608,	"COVERED",		 7, },
{  89,	200,	200,	 -1,	"TOWER_TYPE",		10, },
{  90,	200,	200,	615,	"UNINCORPORATED",	14, },
{  91,	200,	200,	616,	"NO_POPULATION",	13, },
{  92,	200,	200,	690,	"NATIONAL_CAPITAL",	16, },
{  93,	200,	200,	691,	"STATE_CAPITAL",	13, },
{  94,	200,	200,	692,	"COUNTY_SEAT",		11, },
{  95,	200,	200,	 -1,	"POPULATION_CLASS",	16, },
{  96,	200,	200,	  0,	"PHOTOREVISED",		12, },
{  97,	200,	202,	 -1,	"WIDTH",		 5, },
{  98,	200,	203,	 -1,	"ROTATION_ANGLE",	14, },
{  99,	200,	208,	  0,	"BEST_ESTIMATE",	13, },
{ 100,	200,	206,	 -1,	"STATE",		 5, },
{ 101,	200,	207,	 -1,	"POPULATED_PLACE",	15, },
{ 102,	300,	300,	 40,	"ID_IN_FIELD",		11, },	// Public Land Survey
{ 103,	300,	300,	 41,	"WITH_HORIZONTAL",	15, },
{ 104,	300,	300,	 42,	"WITH_ELEVATION",	14, },
{ 105,	300,	300,	201,	"APPROXIMATE_POS",	15, },
{ 106,	300,	300,	202,	"PROTRACTED_POS",	14, },
{ 107,	300,	306,	 -1,	"ORIGIN_OF_SURVEY",	16, },
{ 108,	300,	 -1,	 -1,	"TOWNSHIP",		 8, },
{ 109,	300,	 -1,	 -1,	"RANGE",		 5, },
{ 110,	300,	301,	 -1,	"SECTION",		 7, },
{ 111,	300,	307,	 -1,	"LAND_GRANT",		10, },
{ 112,	300,	 -1,	 -1,	"MONUMENT_NUMBER",	15, },
{ 113,	300,	308,	  0,	"BEST_ESTIMATE",	13, },
{ 114,	300,	306,	 -1,	"OHIO_NAMED_SURVEY",	17, },
{ 115,	300,	300,	612,	"REFUGEE_LANDS",	13, },
{ 116,	190,	190,	605,	"UNPAVED",		 7, },
};





/*
 * A primary attribute may have additional attribute characteristics
 * associated with it.
 * Process this addtional attribute information.
 */
int32_t
get_extra_attrib(int32_t category_major, int32_t *major, int32_t *minor, int32_t *major2, int32_t *minor2, struct subfield *subfield)
{
	int32_t i;
	double f;
	char save_byte;
	char *end_ptr;


	/*
	 * Do some preliminary checks to avoid a lot of
	 * unnecessary processing.
	 */
	if (subfield->length <= 0)  {
		return -1;
	}
	if ((subfield->length == 1)  && (subfield->value[0] == ' '))  {
		return 1;
	}
	if ((subfield->length == 2) && (subfield->value[0] == ' ') && (subfield->value[1] == ' '))  {
		return 1;
	}

	for (i = 0; i < NUM_FEATURES; i++)  {
		if (features[i].main_major == category_major)  {
			if (strncmp(subfield->label, features[i].feature_name, features[i].feature_name_length) == 0)  {
				break;
			}
		}
	}
	if (i == NUM_FEATURES)  {
		fprintf(stderr, "Couldn't find attribute feature name (%s) for major %d.  Attribute feature ignored.\n", subfield->label, category_major);
		return 1;
	}



	switch (category_major)  {
	case HYPSOGRAPHY:
		switch (features[i].key)  {
		case 0:
		case 1:
		case 2:
		case 3:
		case 4:
		case 5:
		case 6:
		case 8:
			if (subfield->value[0] == 'Y')  {
				*major = features[i].major;
				*minor = features[i].minor;
				return 0;
			}
			else  {
				return 1;
			}
		case 7:
			if ((subfield->length != 2) || ((subfield->value[0] == ' ') && (subfield->value[1] == ' ')))  {
				return 1;
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
		default:
			break;
		}
		break;

	case HYDROGRAPHY:
		switch (features[i].key)  {
		case  9:
	 	case 14:
	 	case 15:
	 	case 16:
	 	case 17:
	 	case 18:
	 	case 19:
	 	case 20:
	 	case 21:
	 	case 22:
	 	case 26:
			if (subfield->value[0] == 'Y')  {
				*major = features[i].major;
				*minor = features[i].minor;
				return 0;
			}
			else  {
				return 1;
			}
	 	case 10:
			*major = features[i].major;
	 		if (subfield->value[0] == 'U')  {
				*minor = 601;
			}
	 		else if (subfield->value[0] == 'E')  {
				*minor = 603;
			}
	 		else if (subfield->value[0] == 'T')  {
				*minor = 604;
			}
			else  {
				return 1;
			}
			return 0;
	 	case 11:
			*major = features[i].major;
	 		if (subfield->value[0] == 'O')  {
				*minor = 602;
			}
	 		else if (subfield->value[0] == 'U')  {
				*minor = 617;
			}
			else  {
				return 1;
			}
			return 0;
	 	case 12:
			*major = features[i].major;
	 		if (subfield->value[0] == 'R')  {
				*minor = 605;
			}
	 		else if (subfield->value[0] == 'L')  {
				*minor = 606;
			}
			else  {
				return 1;
			}
			return 0;
	 	case 13:
			*major = features[i].major;
	 		if (subfield->value[0] == 'U')  {
				*minor = 607;
			}
	 		else if (subfield->value[0] == 'A')  {
				*minor = 611;
			}
			else  {
				return 1;
			}
			return 0;
	 	case 23:
			if (strncmp(subfield->value, "-9999.99", 8) == 0)  {
				return 1;	// -9999.99 indicates unused
			}
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			f =  strtod(subfield->value, (char **)0);
			subfield->value[subfield->length] = save_byte;
			if (f < 0.0)  {
				*major = 57;
				*minor = -drawmap_round(f);
			}
			else  {
				*major = 52;
				*minor = drawmap_round(f);
			}
			return 0;
	 	case 24:
			if (strncmp(subfield->value, "-99", 3) == 0)  {
				return 1;	// -99 indicates unused
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
	 	case 25:
			if (strncmp(subfield->value, "-999.99", 6) == 0)  {
				return 1;	// -999.99 indicates unused
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			f =  strtod(subfield->value, (char **)0);
			subfield->value[subfield->length] = save_byte;
			*minor = drawmap_round(f);
			return 0;
		default:
			break;
		};
		break;

	case VEG_SURFACE_COVER:
		switch (features[i].key)  {
 		case 27:
			if (subfield->value[0] == 'Y')  {
				*major = features[i].major;
				*minor = features[i].minor;
				return 0;
			}
			else  {
				return 1;
			}
		default:
			break;
		};
		break;

	case NON_VEG_FEATURES:
		switch (features[i].key)  {
 		case 28:
 		case 29:
			if (subfield->value[0] == 'Y')  {
				*major = features[i].major;
				*minor = features[i].minor;
				return 0;
			}
			else  {
				return 1;
			}
		default:
			break;
		};
		break;

	case BOUNDARIES:
		switch (features[i].key)  {
 		case 30:
 		case 31:
 		case 32:
 		case 33:
 		case 34:
 		case 35:
 		case 36:
 		case 37:
 		case 41:
 		case 43:
			if (subfield->value[0] == 'Y')  {
				*major = features[i].major;
				*minor = features[i].minor;
				return 0;
			}
			else  {
				return 1;
			}
		case 38:
			if ((subfield->length != 2) || ((subfield->value[0] == ' ') &&
							(subfield->value[1] == ' ')))  {
				return 1;
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
		case 39:
			if ((subfield->length != 3) || ((subfield->value[0] == ' ') &&
							(subfield->value[1] == ' ') &&
							(subfield->value[2] == ' ')))  {
				return 1;
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
		case 40:
			if ((subfield->length != 5) || ((subfield->value[0] == ' ') &&
							(subfield->value[1] == ' ') &&
							(subfield->value[2] == ' ') &&
							(subfield->value[3] == ' ') &&
							(subfield->value[4] == ' ')))  {
				return 1;
			}
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			*major = 93;	// Carries first two digits of 5-digit code
			*major2 = 94;	// Carries last three digits of 5-digit code
			*minor2 = *minor % 1000;
			*minor = *minor / 1000;
			return 0;
		case 42:
			if ((subfield->length != 8) || ((subfield->value[0] == ' ') &&
							(subfield->value[1] == ' ') &&
							(subfield->value[2] == ' ') &&
							(subfield->value[3] == ' ') &&
							(subfield->value[4] == ' ') &&
							(subfield->value[5] == ' ') &&
							(subfield->value[6] == ' ') &&
							(subfield->value[7] == ' ')))  {
				return 1;
			}
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			*major = 95;
			/*
			 * So far, when I check the original DLG-3 (non-SDTS) source material,
			 * the monument number attributes aren't included.  In other words, if I comment out
			 * this whole case, the resulting SDTS-to-DLG conversion will match the
			 * original.  If I leave the case uncommented, the conversion won't match
			 * the original.
			 *
			 * Note that there is also a major code 96, for the alphabetic portion of
			 * a monument number.  I'm not sure how this is handled, since I've yet
			 * to find an example.
			 */
			return 0;
		default:
			break;
		};
		break;

	case SURVEY_CONTROL:
		switch (features[i].key)  {
		case 44:
			if ((subfield->length != 2) || ((subfield->value[0] == ' ') &&
							(subfield->value[1] == ' ')))  {
				return 1;
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
		case 45:
			if ((subfield->length != 3) || ((subfield->value[0] == ' ') &&
							(subfield->value[1] == ' ') &&
							(subfield->value[2] == ' ')))  {
				return 1;
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
	 	case 46:
			/*
			 * The spec says 9999.99 means not applicable.
			 * This is at odds with other categories of data, which use -9999.99.
			 * Assume that the spec is wrong, since it wouldn't make much sense to
			 * restrict ourselves to elevations that don't equal 9999.99.
			 */
//			if (strncmp(subfield->value, "9999.99", 7) == 0)  {
//				return 1;	// 9999.99 indicates unused
//			}
			if (strncmp(subfield->value, "-9999.99", 8) == 0)  {
				return 1;	// -9999.99 indicates unused
			}
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			f =  strtod(subfield->value, (char **)0);
			subfield->value[subfield->length] = save_byte;
			if (f < 0.0)  {
				*major = 157;
				*minor = -drawmap_round(f);
			}
			else  {
				*major = 154;
				*minor = drawmap_round(f);
			}
			return 0;
		default:
			break;
		};
		break;

	case ROADS_AND_TRAILS:
		switch (features[i].key)  {
 		case 47:
 		case 52:
 		case 53:
 		case 54:
 		case 55:
 		case 56:
 		case 57:
 		case 60:
			if (subfield->value[0] == 'Y')  {
				*major = features[i].major;
				*minor = features[i].minor;
				return 0;
			}
			else  {
				return 1;
			}
		case 48:
			*major = features[i].major;
	 		if (subfield->value[0] == 'T')  {
				*minor = 601;
			}
	 		else if (subfield->value[0] == 'S')  {
				*minor = 606;
			}
	 		else if (subfield->value[0] == 'D')  {
				*minor = 612;
			}
	 		else if (subfield->value[0] == 'E')  {
				*minor = 614;
			}
	 		else if (subfield->value[0] == 'R')  {
				*minor = 618;
			}
			else  {
				return 1;
			}
			return 0;
	 	case 49:
			*major = features[i].major;
	 		if (subfield->value[0] == 'O')  {
				*minor = 602;
			}
	 		else if (subfield->value[0] == 'U')  {
				*minor = 607;
			}
			else  {
				return 1;
			}
			return 0;
	 	case 50:
			*major = features[i].major;
	 		if (subfield->value[0] == 'U')  {
				*minor = 603;
			}
	 		else if (subfield->value[0] == 'X')  {
				*minor = 604;
			}
	 		else if (subfield->value[0] == 'P')  {
				*minor = 611;
			}
			else  {
				return 1;
			}
			return 0;
	 	case 51:
			*major = features[i].major;
	 		if (subfield->value[0] == 'T')  {
				*minor = 609;
			}
	 		else if (subfield->value[0] == 'P')  {
				*minor = 610;
			}
			else  {
				return 1;
			}
			return 0;
		case 58:
			/*
			 * The spec says this field is all blanks if unused.
			 * However, sample files have it as "-9".
			 * Check it both ways.
			 */
			if ((subfield->length != 2) || ((subfield->value[0] == ' ') &&
							(subfield->value[1] == ' ')))  {
				return 1;
			}
			if (strncmp(subfield->value, "-9", 2) == 0)  {
				return 1;	// -9 indicates unused
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
	 	case 59:
			if (strncmp(subfield->value, "-99", 3) == 0)  {
				return 1;	// -99 indicates unused
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor = 600 + strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
		default:
			break;
		};
		break;

	case RAILROADS:
		switch (features[i].key)  {
 		case 65:
 		case 66:
 		case 67:
 		case 68:
 		case 69:
 		case 70:
 		case 71:
 		case 74:
			if (subfield->value[0] == 'Y')  {
				*major = features[i].major;
				*minor = features[i].minor;
				return 0;
			}
			else  {
				return 1;
			}
		case 61:
			*major = features[i].major;
	 		if (subfield->value[0] == 'T')  {
				*minor = 601;
			}
	 		else if (subfield->value[0] == 'E')  {
				*minor = 609;
			}
	 		else if (subfield->value[0] == 'R')  {
				*minor = 611;
			}
			else  {
				return 1;
			}
			return 0;
	 	case 62:
			*major = features[i].major;
	 		if (subfield->value[0] == 'O')  {
				*minor = 602;
			}
	 		else if (subfield->value[0] == 'U')  {
				*minor = 605;
			}
			else  {
				return 1;
			}
			return 0;
	 	case 63:
			*major = features[i].major;
	 		if (subfield->value[0] == 'A')  {
				*minor = 603;
			}
	 		else if (subfield->value[0] == 'D')  {
				*minor = 604;
			}
	 		else if (subfield->value[0] == 'U')  {
				*minor = 608;
			}
			else  {
				return 1;
			}
			return 0;
	 	case 64:
			*major = features[i].major;
	 		if (subfield->value[0] == 'P')  {
				*minor = 612;
			}
	 		else if (subfield->value[0] == 'G')  {
				*minor = 613;
			}
			else  {
				return 1;
			}
			return 0;
		case 72:
			if (strncmp(subfield->value, "-9", 2) == 0)  {
				return 1;	// -9 indicates unused
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
	 	case 73:
			if (strncmp(subfield->value, "-99", 3) == 0)  {
				return 1;	// -99 indicates unused
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor = strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
		default:
			break;
		};
		break;

	case PIPE_TRANS_LINES:
		switch (features[i].key)  {
 		case 77:
 		case 78:
 		case 79:
 		case 80:
 		case 82:
 		case 116:
			if (subfield->value[0] == 'Y')  {
				*major = features[i].major;
				*minor = features[i].minor;
				return 0;
			}
			else  {
				return 1;
			}
	 	case 75:
			*major = features[i].major;
	 		if (subfield->value[0] == 'U')  {
				*minor = 600;
			}
	 		else if (subfield->value[0] == 'A')  {
				*minor = 603;
			}
	 		else if (subfield->value[0] == 'S')  {
				*minor = 606;
			}
			else  {
				return 1;
			}
			return 0;
	 	case 76:
			*major = features[i].major;
	 		if (subfield->value[0] == 'U')  {
				*minor = 601;
			}
	 		else if (subfield->value[0] == 'A')  {
				*minor = 602;
			}
	 		else if (subfield->value[0] == 'C')  {
				*minor = 604;
			}
			else  {
				return 1;
			}
			return 0;
	 	case 81:
			if (strncmp(subfield->value, "-99", 3) == 0)  {
				return 1;	// -99 indicates unused
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor = strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
		case 83:
			if ((subfield->length != 2) || ((subfield->value[0] == ' ') &&
							(subfield->value[1] == ' ')))  {
				return 1;
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
		case 84:
			if ((subfield->length != 4) || ((subfield->value[0] == ' ') &&
							(subfield->value[1] == ' ') &&
							(subfield->value[3] == ' ') &&
							(subfield->value[4] == ' ')))  {
				return 1;
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
		default:
			break;
		};
		break;

	case MANMADE_FEATURES:
		switch (features[i].key)  {
 		case 88:
 		case 90:
 		case 91:
 		case 92:
 		case 93:
 		case 94:
 		case 96:
 		case 99:
			if (subfield->value[0] == 'Y')  {
				*major = features[i].major;
				*minor = features[i].minor;
				return 0;
			}
			else  {
				return 1;
			}
	 	case 85:
			*major = features[i].major;
	 		if (subfield->value[0] == 'U')  {
				*minor = 601;
			}
	 		else if (subfield->value[0] == 'S')  {
				*minor = 617;
			}
			else  {
				return 1;
			}
			return 0;
	 	case 86:
			*major = features[i].major;
	 		if (subfield->value[0] == 'C')  {
				*minor = 602;
			}
	 		else if (subfield->value[0] == 'A')  {
				*minor = 603;
			}
	 		else if (subfield->value[0] == 'R')  {
				*minor = 618;
			}
			else  {
				return 1;
			}
			return 0;
	 	case 87:
			*major = features[i].major;
	 		if (subfield->value[0] == 'W')  {
				*minor = 604;
			}
	 		else if (subfield->value[0] == 'O')  {
				*minor = 605;
			}
	 		else if (subfield->value[0] == 'G')  {
				*minor = 606;
			}
	 		else if (subfield->value[0] == 'C')  {
				*minor = 607;
			}
	 		else if (subfield->value[0] == 'V')  {
				*minor = 609;
			}
	 		else if (subfield->value[0] == 'S')  {
				*minor = 610;
			}
	 		else if (subfield->value[0] == 'L')  {
				*minor = 611;
			}
	 		else if (subfield->value[0] == 'B')  {
				*minor = 612;
			}
	 		else if (subfield->value[0] == 'A')  {
				*minor = 619;
			}
	 		else if (subfield->value[0] == 'H')  {
				*minor = 620;
			}
	 		else if (subfield->value[0] == 'I')  {
				*minor = 621;
			}
	 		else if (subfield->value[0] == 'P')  {
				*minor = 622;
			}
	 		else if (subfield->value[0] == 'E')  {
				*minor = 623;
			}
	 		else if (subfield->value[0] == 'R')  {
				*minor = 624;
			}
			else  {
				return 1;
			}
			return 0;
	 	case 89:
			*major = features[i].major;
	 		if (subfield->value[0] == 'R')  {
				*minor = 613;
			}
	 		else if (subfield->value[0] == 'L')  {
				*minor = 614;
			}
			else  {
				return 1;
			}
			return 0;
		case 95:
			if (strncmp(subfield->value, "-9", 2) == 0)  {
				return 1;	// -9 indicates unused
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor = 680 + strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
	 	case 97:
			if (strncmp(subfield->value, "-999", 4) == 0)  {
				return 1;	// -999 indicates unused
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
	 	case 98:
			if (strncmp(subfield->value, "-99", 3) == 0)  {
				return 1;	// -99 indicates unused
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor = strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
		case 100:
			// Only for 2M-scale DLGs.
			if ((subfield->length != 2) || ((subfield->value[0] == ' ') &&
							(subfield->value[1] == ' ')))  {
				return 1;
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
		case 101:
			// Not sure this is handled properly, but it is only for 2M-scale DLGs.
			if ((subfield->length != 4) || ((subfield->value[0] == ' ') &&
							(subfield->value[1] == ' ') &&
							(subfield->value[3] == ' ') &&
							(subfield->value[4] == ' ')))  {
				return 1;
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			// Minor code may be an alphabetic database key.  Don't know.
			*minor =  strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
		default:
			break;
		};
		break;

	case PUBLIC_LAND_SURVEYS:
		switch (features[i].key)  {
 		case 102:
 		case 103:
 		case 104:
 		case 105:
 		case 106:
 		case 113:
 		case 115:
			if (subfield->value[0] == 'Y')  {
				*major = features[i].major;
				*minor = features[i].minor;
				return 0;
			}
			else  {
				return 1;
			}
		case 107:
		case 114:
			if (strncmp(subfield->value, "-9", 2) == 0)  {
				return 1;	// -9 indicates unused
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor = strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
		case 108:
			if ((subfield->length != 8) || ((subfield->value[0] == ' ') &&
							(subfield->value[1] == ' ') &&
							(subfield->value[2] == ' ') &&
							(subfield->value[3] == ' ') &&
							(subfield->value[4] == ' ') &&
							(subfield->value[5] == ' ') &&
							(subfield->value[6] == ' ') &&
							(subfield->value[7] == ' ')))  {
				return 1;
			}
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, &end_ptr, 10);
			/*
			 * If there is a fractional number, we need to add in the special code
			 * for it.
			 */
			if (strncmp(end_ptr, " 1/4", 4) == 0)  {
				end_ptr += 4;
				*minor += 2000;
			}
			else if (strncmp(end_ptr, " 1/2", 4) == 0)  {
				end_ptr += 4;
				*minor += 4000;
			}
			else if (strncmp(end_ptr, " 3/4", 4) == 0)  {
				end_ptr += 4;
				*minor += 6000;
			}
			while ((*end_ptr != '\0') && (*end_ptr == ' '))  { end_ptr++; }
			if (*end_ptr != '\0')  {
				if (*end_ptr == 'N')  {
					*major = 302;
				}
				else if (*end_ptr == 'S')  {
					*major = 303;
				}
				else  {
					fprintf(stderr, "Warning:  Township number (SDTS=%.*s) has an unknown form.  Assuming this is a northern township.\n", subfield->length, subfield->value);
					*major = 302;
				}
			}
			else  {
				fprintf(stderr, "Warning:  Township number (SDTS=%.*s) has no N/S designator.  N assumed.\n", subfield->length, subfield->value);
				*major = 302;
			}
			subfield->value[subfield->length] = save_byte;
			return 0;
		case 109:
			if ((subfield->length != 8) || ((subfield->value[0] == ' ') &&
							(subfield->value[1] == ' ') &&
							(subfield->value[2] == ' ') &&
							(subfield->value[3] == ' ') &&
							(subfield->value[4] == ' ') &&
							(subfield->value[5] == ' ') &&
							(subfield->value[6] == ' ') &&
							(subfield->value[7] == ' ')))  {
				return 1;
			}
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, &end_ptr, 10);
			/*
			 * If there is a fractional number, we need to add in the special digit for it at the
			 * beginning of the minor attribute code.  Note that there are theoretically also
			 * two other special digits:  8 for duplicate to north
			 * or east of original township, 9 for triplicate to north or east of original
			 * township.  As yet, I don't know if or how these latter two codes are encoded
			 * in SDTS, so they aren't handled here.
			 *
			 * I have also found some odd range specifiers in the file:
			 *
			 *       DLG/LARGE_SCALE/H/hooven_OH/public_lands/PL01APLF.DDF
			 *
			 * that take the form "1AE" and "2AE".  I have no idea what the "A" is for.
			 * So far, out of several thousands of files checked, this is the only file that I
			 * have found these in.  Until I find out otherwise, I am assuming that these
			 * are an error.  However, since there are 14 instances in this one SDTS file,
			 * error may not be the correct explanation.
			 */
			if (strncmp(end_ptr, " 1/4", 4) == 0)  {
				end_ptr += 4;
				*minor += 2000;
			}
			else if (strncmp(end_ptr, " 1/2", 4) == 0)  {
				end_ptr += 4;
				*minor += 4000;
			}
			else if (strncmp(end_ptr, " 3/4", 4) == 0)  {
				end_ptr += 4;
				*minor += 6000;
			}
			while ((*end_ptr != '\0') && (*end_ptr == ' '))  { end_ptr++; }
			if (*end_ptr != '\0')  {
				if (*end_ptr == 'E')  {
					*major = 304;
				}
				else if (*end_ptr == 'W')  {
					*major = 305;
				}
				else  {
					fprintf(stderr, "Warning:  Range number (SDTS=%.*s) has an unknown form.  Assuming this is an eastern range.\n", subfield->length, subfield->value);
					*major = 304;
				}
			}
			else  {
				fprintf(stderr, "Warning:  Range number (SDTS=%.*s) has no E/W designator.  E assumed.\n", subfield->length, subfield->value);
				*major = 304;
			}
			subfield->value[subfield->length] = save_byte;
			return 0;
		case 110:
			/*
			 * There are theoretically some possible alphabetic modifiers
			 * for this case, but I have yet to find a file that contains any.
			 */
			if ((subfield->length != 4) || ((subfield->value[0] == ' ') &&
							(subfield->value[1] == ' ') &&
							(subfield->value[2] == ' ') &&
							(subfield->value[3] == ' ')))  {
				return 1;
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
		case 111:
			/*
			 * There are theoretically some possible alphabetic modifiers
			 * for this case, but I have yet to find a file that contains any.
			 */
			if ((subfield->length != 4) || ((subfield->value[0] == ' ') &&
							(subfield->value[1] == ' ') &&
							(subfield->value[2] == ' ') &&
							(subfield->value[3] == ' ')))  {
				return 1;
			}
			*major = features[i].major;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
		case 112:
			/*
			 * In theory, there can be monument numbers that contain
			 * fractions of 1/2.  They require an extra "300 0625" to
			 * be emitted.  I have yet to find an example, so we don't
			 * handle such fractions yet.
			 */
			if ((subfield->length != 8) || ((subfield->value[0] == ' ') &&
							(subfield->value[1] == ' ') &&
							(subfield->value[2] == ' ') &&
							(subfield->value[3] == ' ') &&
							(subfield->value[4] == ' ') &&
							(subfield->value[5] == ' ') &&
							(subfield->value[6] == ' ') &&
							(subfield->value[7] == ' ')))  {
				return 1;
			}
			*major = 308;
			save_byte = subfield->value[subfield->length]; subfield->value[subfield->length] = '\0';
			*minor =  strtol(subfield->value, (char **)0, 10);
			subfield->value[subfield->length] = save_byte;
			return 0;
		default:
			break;
		};
		break;

	default:
		fprintf(stderr, "Couldn't find attribute feature name (%s).  Attribute feature ignored.  Internal codes: %d,%d\n", subfield->label, i, features[i].key);
		return 1;
		break;
	}

	return 1;
}




/*
 * Read in all of the attribute files that affect this SDTS transfer.
 *
 * The process returns the number of attribute file entries in the attrib_files structure.
 */
int32_t
process_attrib_sdts(char *passed_file_name, char *category_name, int32_t *data_type, int32_t *color, int32_t gz_flag, int32_t upper_case_flag)
{
	struct subfield subfield;
	int32_t i, j, k = 100000000;	// bogus initializer to expose errors.
	double f;
	int32_t num_attrib_files;
	int32_t file_name_length;
	char file_name[MAX_FILE_NAME + 1];
	int32_t current_size;
	char type[2];
	int32_t parse_type;
	int32_t record_id = 100000000;	// bogus initializer to expose errors.
	char save_byte;
	char *ptr;
	int32_t major, minor;
	int32_t major2, minor2;


	num_attrib_files = 0;
	file_name_length = strlen(passed_file_name);
	file_name[MAX_FILE_NAME] = '\0';


	/*
	 * There are separate SDTS DLG files containing data records for
	 * Nodes, Areas, and Lines.  We are going to open all of them and
	 * search them for the attribute files that they reference.
	 * The relevant modules are LE??, PC??, NE??, and NO??.
	 * We will eventually read these line/area/node files again to get their other information.
	 *
	 * The Line, Area, and Node files reference the attribute Record IDs to attach
	 * attributes to lines, areas, and nodes (degenerate lines) so we need this
	 * information before we read in the Line, Area, and Node files, so that we
	 * can build complete entries for each of the Nodes, Lines, and Areas.
	 *
	 * We store the attributes from each file in Record ID order.  (The Record IDs don't have
	 * to be sequential numbers, under the SDTS standard, but we assume that they
	 * are because the attribute files would have had to be generated from scratch during
	 * conversion to SDTS.
	 *
	 * If attributes are used, they will be used in one of the four (NE??, PC??, NE??, NO??)
	 * files.  Note that, while the main primary attribute file is named A??F, there may
	 * be other primary attribute files, named ACOI (attributes that describe
	 * concident features), AHPR (elevation attributes, in meters, for hypsography files),
	 * AHPT (elevation attributes, in feet, for hypsography files), ARDM (route numbers
	 * and route types for roads and trails), or ABDM (agency attributes for boundary
	 * data, mostly for 2M-scale files, but occasionally for 100K-scale files).
	 * (There are several types of secondary attribute files too, but they are only used for
	 * 2,000,000 scale DLG data, so we ignore them.)
	 *
	 * We could also obtain the name of the attribute files by reading the CATD module.
	 * By doing it the way we do here, we find out if any attributes are actually used.
	 * The code was written the way it is because there might be multiple primary
	 * attribute files in a given transfer, which would make the
	 * CATD information ambiguous.  For example, a Transportation transfer could
	 * contain ARDF, AMTF, and ARRF for roads, misc features, and railroads.  Generally,
	 * we should only need one of these to process a given LE?? module.
	 */
	/*
	 * Open the LE?? module in preparation for parsing.
	 */
	if (begin_ddf(passed_file_name) < 0)  {
		fprintf(stderr, "Can't open %s for reading, errno = %d\n", passed_file_name, errno);
		exit(0);
	}
	/*
	 * Loop through the subfields until we find what we want.
	 */
	while (get_subfield(&subfield) != 0)  {
		if (strcmp(subfield.tag, "ATID") == 0)  {
			if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "MODN") == 0))  {
				if (subfield.length == 4)  {
					for (i = 0; i < num_attrib_files; i++)  {
						if ((attrib_files[i].module_name[0] == subfield.value[0]) &&
						    (attrib_files[i].module_name[1] == subfield.value[1]) &&
						    (attrib_files[i].module_name[2] == subfield.value[2]) &&
						    (attrib_files[i].module_name[3] == subfield.value[3]))  {
							break;
						}
					}
					if (i == num_attrib_files)  {
						if (num_attrib_files == MAX_ATTRIB_FILES)  {
							fprintf(stderr, "Ran out of space for attribute file names.\n");
							break;
						}
						attrib_files[i].module_name[0] = subfield.value[0];
						attrib_files[i].module_name[1] = subfield.value[1];
						attrib_files[i].module_name[2] = subfield.value[2];
						attrib_files[i].module_name[3] = subfield.value[3];
						num_attrib_files++;
					}
				}
				else  {
					fprintf(stderr, "Attribute module ID %.*s does not appear correct.\n", subfield.length, subfield.value);
				}
			}
		}
	}
	/* We are done with this file, so close it. */
	end_ddf();



	/* Now go on the the PC?? module (the polygon module). */
	strncpy(file_name, passed_file_name, MAX_FILE_NAME);
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			file_name[file_name_length - 11] = 'p';
			file_name[file_name_length - 10] = 'c';
		}
		else  {
			file_name[file_name_length -  8] = 'p';
			file_name[file_name_length -  7] = 'c';
		}
	}
	else  {
		if (gz_flag != 0)  {
			file_name[file_name_length - 11] = 'P';
			file_name[file_name_length - 10] = 'C';
		}
		else  {
			file_name[file_name_length -  8] = 'P';
			file_name[file_name_length -  7] = 'C';
		}
	}
	/*
	 * Open the file in preparation for parsing.
	 */
	if (begin_ddf(file_name) >= 0)  {
		while (get_subfield(&subfield) != 0)  {
			if (strcmp(subfield.tag, "ATID") == 0)  {
				if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "MODN") == 0))  {
					if (subfield.length == 4)  {
						for (i = 0; i < num_attrib_files; i++)  {
							if ((attrib_files[i].module_name[0] == subfield.value[0]) &&
							    (attrib_files[i].module_name[1] == subfield.value[1]) &&
							    (attrib_files[i].module_name[2] == subfield.value[2]) &&
							    (attrib_files[i].module_name[3] == subfield.value[3]))  {
								break;
							}
						}
						if (i == num_attrib_files)  {
							if (num_attrib_files == MAX_ATTRIB_FILES)  {
								fprintf(stderr, "Ran out of space for attribute file names.\n");
								break;
							}
							attrib_files[i].module_name[0] = subfield.value[0];
							attrib_files[i].module_name[1] = subfield.value[1];
							attrib_files[i].module_name[2] = subfield.value[2];
							attrib_files[i].module_name[3] = subfield.value[3];
							num_attrib_files++;
						}
					}
					else  {
						fprintf(stderr, "Attribute module ID %.*s does not appear correct.\n", subfield.length, subfield.value);
					}
				}
			}
		}
		/* We are done with this file, so close it. */
		end_ddf();
	}



	/* Now go on the the NO?? module (the planar-node module). */
	strncpy(file_name, passed_file_name, MAX_FILE_NAME);
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			file_name[file_name_length - 11] = 'n';
			file_name[file_name_length - 10] = 'o';
		}
		else  {
			file_name[file_name_length -  8] = 'n';
			file_name[file_name_length -  7] = 'o';
		}
	}
	else  {
		if (gz_flag != 0)  {
			file_name[file_name_length - 11] = 'N';
			file_name[file_name_length - 10] = 'O';
		}
		else  {
			file_name[file_name_length -  8] = 'N';
			file_name[file_name_length -  7] = 'O';
		}
	}
	/*
	 * Open the file in preparation for parsing.
	 */
	if (begin_ddf(file_name) >= 0)  {
		while (get_subfield(&subfield) != 0)  {
			if (strcmp(subfield.tag, "ATID") == 0)  {
				if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "MODN") == 0))  {
					if (subfield.length == 4)  {
						for (i = 0; i < num_attrib_files; i++)  {
							if ((attrib_files[i].module_name[0] == subfield.value[0]) &&
							    (attrib_files[i].module_name[1] == subfield.value[1]) &&
							    (attrib_files[i].module_name[2] == subfield.value[2]) &&
							    (attrib_files[i].module_name[3] == subfield.value[3]))  {
								break;
							}
						}
						if (i == num_attrib_files)  {
							if (num_attrib_files == MAX_ATTRIB_FILES)  {
								fprintf(stderr, "Ran out of space for attribute file names.\n");
								break;
							}
							attrib_files[i].module_name[0] = subfield.value[0];
							attrib_files[i].module_name[1] = subfield.value[1];
							attrib_files[i].module_name[2] = subfield.value[2];
							attrib_files[i].module_name[3] = subfield.value[3];
							num_attrib_files++;
						}
					}
					else  {
						fprintf(stderr, "Attribute module ID %.*s does not appear correct.\n", subfield.length, subfield.value);
					}
				}
			}
		}
		/* We are done with this file, so close it. */
		end_ddf();
	}



	/* Now go on the the NE?? module (the node-entity module). */
	strncpy(file_name, passed_file_name, MAX_FILE_NAME);
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			file_name[file_name_length - 11] = 'n';
			file_name[file_name_length - 10] = 'e';
		}
		else  {
			file_name[file_name_length -  8] = 'n';
			file_name[file_name_length -  7] = 'e';
		}
	}
	else  {
		if (gz_flag != 0)  {
			file_name[file_name_length - 11] = 'N';
			file_name[file_name_length - 10] = 'E';
		}
		else  {
			file_name[file_name_length -  8] = 'N';
			file_name[file_name_length -  7] = 'E';
		}
	}
	/*
	 * Open the file in preparation for parsing.
	 */
	if (begin_ddf(file_name) >= 0)  {
		while (get_subfield(&subfield) != 0)  {
			if (strcmp(subfield.tag, "ATID") == 0)  {
				if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "MODN") == 0))  {
					if (subfield.length == 4)  {
						for (i = 0; i < num_attrib_files; i++)  {
							if ((attrib_files[i].module_name[0] == subfield.value[0]) &&
							    (attrib_files[i].module_name[1] == subfield.value[1]) &&
							    (attrib_files[i].module_name[2] == subfield.value[2]) &&
							    (attrib_files[i].module_name[3] == subfield.value[3]))  {
								break;
							}
						}
						if (i == num_attrib_files)  {
							if (num_attrib_files == MAX_ATTRIB_FILES)  {
								fprintf(stderr, "Ran out of space for attribute file names.\n");
								break;
							}
							attrib_files[i].module_name[0] = subfield.value[0];
							attrib_files[i].module_name[1] = subfield.value[1];
							attrib_files[i].module_name[2] = subfield.value[2];
							attrib_files[i].module_name[3] = subfield.value[3];
							num_attrib_files++;
						}
					}
					else  {
						fprintf(stderr, "Attribute module ID %.*s does not appear correct.\n", subfield.length, subfield.value);
					}
				}
			}
		}
		/* We are done with this file, so close it. */
		end_ddf();
	}



	/*
	 * Check to see that we didn't get any unknown file types.
	 *
	 * There are additional file types for 2,000,000-scale files,
	 * but we don't handle such files so ignore those file types.
	 */
	for (i = 0; i < num_attrib_files; i++)  {
		if ((strncmp(attrib_files[i].module_name, "AHPF", 4) != 0) &&
		    (strncmp(attrib_files[i].module_name, "AHYF", 4) != 0) &&
		    (strncmp(attrib_files[i].module_name, "ASCF", 4) != 0) &&
		    (strncmp(attrib_files[i].module_name, "ANVF", 4) != 0) &&
		    (strncmp(attrib_files[i].module_name, "ABDF", 4) != 0) &&
		    (strncmp(attrib_files[i].module_name, "AMTF", 4) != 0) &&
		    (strncmp(attrib_files[i].module_name, "ARDF", 4) != 0) &&
		    (strncmp(attrib_files[i].module_name, "ARRF", 4) != 0) &&
		    (strncmp(attrib_files[i].module_name, "AMSF", 4) != 0) &&
		    (strncmp(attrib_files[i].module_name, "ASMF", 4) != 0) &&
		    (strncmp(attrib_files[i].module_name, "APLF", 4) != 0) &&
		    (strncmp(attrib_files[i].module_name, "ACOI", 4) != 0) &&
		    (strncmp(attrib_files[i].module_name, "AHPR", 4) != 0) &&
		    (strncmp(attrib_files[i].module_name, "AHPT", 4) != 0) &&
		    (strncmp(attrib_files[i].module_name, "ABDM", 4) != 0) &&
		    (strncmp(attrib_files[i].module_name, "ARDM", 4) != 0))  {
			fprintf(stderr, "Unknown attribute file type (%s).  File type ignored.\n", attrib_files[i].module_name);
			attrib_files[i].module_name[0] = attrib_files[num_attrib_files - 1].module_name[0];
			attrib_files[i].module_name[1] = attrib_files[num_attrib_files - 1].module_name[1];
			attrib_files[i].module_name[2] = attrib_files[num_attrib_files - 1].module_name[2];
			attrib_files[i].module_name[3] = attrib_files[num_attrib_files - 1].module_name[3];
			num_attrib_files--;
		}
	}



	/*
	 * The two middle characters of the Attribute Features file name
	 * tell us the type of data in this SDTS theme.
	 *
	 * We can't reliably get this from the file name or category name
	 * because Transportation files may have TR in their file name
	 * or TRANSPORTATION as a category name.  (If we could pry it out,
	 * we updated the category name from TRANSPORTATION to the proper
	 * theme by looking in CATS.  However, we may have failed in the update.)
	 * We need the more detailed RD, RR, or MT designators for transportation.
	 *
	 * Check that we have one and only one main Primary Attribute File.
	 * If so, then get the theme from the module name.
	 * If there is no main Primary Attribute File, or more than one, then
	 * do our best to deduce the data type from the category name or
	 * the passed_file_name.
	 *
	 * This may seem like a lot of work for a piece of data of so-so
	 * importance.  However, we choose the color scheme of the map
	 * based on this data, and it would be quite confusing to get
	 * the color wrong.
	 */
	j = 0;
	for (i = 0; i < num_attrib_files; i++)  {
		if (attrib_files[i].module_name[3] == 'F')  {
			j++;
			k = i;
		}
	}
	if (j != 1)  {
		if (j > 1)  {
			fprintf(stderr, "Warning:  More than one main primary attribute file.  Handling ambiguity as best I can.\n");
		}
		if ((category_name[0] != '\0') && (category_name[1] != '\0'))  {
			switch(category_name[0])  {
			case 'B':	/* BOUNDARIES */
				type[0] = 'B';
				type[1] = 'D';
				break;
			case 'H':
				type[0] = 'H';
				if (category_name[2] == 'D')  {
					/* HYDROGRAPHY */
					type[1] = 'Y';
					break;
				}
				else  {
					/* HYPSOGRAPHY */
					type[1] = 'P';
					break;
				}
			case 'P':
				if (category_name[1] == 'I')  {
					/* PIPE & TRANS LINES */
					type[0] = 'M';
					type[1] = 'T';
					break;
				}
				else  {
					/* PUBLIC LAND SURVEYS */
					type[0] = 'P';
					type[1] = 'L';
					break;
				}
			case 'R':
				type[0] = 'R';
				if (category_name[1] == 'A')  {
					/* RAILROADS */
					type[1] = 'R';
					break;
				}
				else  {
					/* ROADS AND TRAILS */
					type[1] = 'D';
					break;
				}
			case 'M':	/* MANMADE FEATURES */
				type[0] = 'M';
				type[1] = 'S';
				break;
			case 'S':	/* SURVEY CONTROL */
				type[0] = 'S';
				type[1] = 'M';
				break;
			case 'V':	/* VEG SURFACE COVER */
				type[0] = 'S';
				type[1] = 'C';
				break;
			case 'N':	/* NON-VEG FEATURES */
				type[0] = 'N';
				type[1] = 'V';
				break;
			default:
				fprintf(stderr, "Unknown theme %20.20s\n", category_name);
				exit(0);
				break;
			}
		}
		else  {
			if ((gz_flag != 0) && (file_name_length >= 15))  {
				type[0] = toupper(passed_file_name[file_name_length - 15]);
				type[1] = toupper(passed_file_name[file_name_length - 14]);
			}
			else if ((gz_flag == 0) && (file_name_length >= 12))  {
				type[0] = toupper(passed_file_name[file_name_length - 12]);
				type[1] = toupper(passed_file_name[file_name_length - 11]);
			}
			else  {
				type[0] = '\0';
				type[1] = '\0';
			}
		}
	}
	else {
		type[0] = attrib_files[k].module_name[1];
		type[1] = attrib_files[k].module_name[2];
	}
	switch(type[0])  {
	case 'B':	/* BD: BOUNDARIES */
		*color = GRAY;
		*data_type = BOUNDARIES;
		break;
	case 'H':
		if (type[1] == 'Y')  {
			/* HY: HYDROGRAPHY */
			*color = B_BLUE;
			*data_type = HYDROGRAPHY;
			break;
		}
		else  {
			/* HP: HYPSOGRAPHY */
			*color = L_ORANGE;
			*data_type = HYPSOGRAPHY;
			break;
		}
	case 'P':	/* PL: US PUBLIC LAND SURVEY SYSTEM */
		*color = BLACK;
		*data_type = PUBLIC_LAND_SURVEYS;
		break;
	case 'R':
		if (type[1] == 'R')  {
			/* RR: RAILROADS */
			*color = BLACK;
			*data_type = RAILROADS;
			break;
		}
		else  {
			/* RD: ROADS AND TRAILS */
			*color = B_RED;
			*data_type = ROADS_AND_TRAILS;
			break;
		}
	case 'T':	/* TR: general transportation features.  Roads and trails assumed. */
		*color = B_RED;
		*data_type = ROADS_AND_TRAILS;
		break;
	case 'M':
		if (type[1] == 'T')  {
			/* MT: PIPELINES, TRANSMISSION LINES, and MISC TRANSPORTATION FEATURES */
			*color = BLACK;
			*data_type = PIPE_TRANS_LINES;
		}
		else  {
			/* MS: MANMADE FEATURES */
			*color = BLACK;
			*data_type = MANMADE_FEATURES;
		}
		break;
	case 'S':
		if (type[1] == 'C')  {
			/* SC: VEGETATIVE SURFACE COVER */
			*color = B_GREEN;
			*data_type = VEG_SURFACE_COVER;
		}
		else  {
			/* SM: SURVEY CONTROL AND MARKERS */
			*color = BLACK;
			*data_type = SURVEY_CONTROL;
		}
		break;
	case 'N':	/* NV: NON-VEG FEATURES */
		*color = BLACK;
		*data_type = NON_VEG_FEATURES;
		break;
	default:
		fprintf(stderr, "Unknown data type %c%c, assuming Boundaries\n", type[0], type[1]);
		*color = BLACK;
		*data_type = BOUNDARIES;
		break;
	}



	/*
	 * At this point, we should have a complete list of attribute files
	 * that need to be read in.  Thus, we go ahead and do the reading.
	 */
	for (i = 0; i < num_attrib_files; i++)  {
		attrib_files[i].num_attrib = -1;
		attrib_files[i].attrib = (struct attribute_list *)0;
		current_size = 0;


		/*
		 * Convert the module name into a single number for later use.
		 */
		switch (attrib_files[i].module_name[3])  {
		case 'F':	// main Primary Attribute Module.  (Files with names of the form A??F.)
			parse_type = 0;
			break;
		case 'I':	// Coincidence Attribute Primary Module.  (Files with names of the form ACOI.)
			parse_type = 1;
			break;
		case 'R':	// Elevation Attribute Primary Module (meters). (Files with names of the form AHPR.)
			parse_type = 2;
			break;
		case 'T':	// Elevation Attribute Primary Module (feet). (Files with names of the form AHPT.)
			parse_type = 3;
			break;
		case 'M':
			if (attrib_files[i].module_name[1] == 'R')  {
				// Route Attribute Primary Module. (Files with names of the form ARDM.)
				parse_type = 4;
			}
			else  {
				// Agency Attribute Primary Module. (Files with names of the form ABDM.)
				parse_type = 5;
			}
			break;
		default:
			fprintf(stderr, "Unknown attribute file type (%s).  Should have been detected earlier.\n", attrib_files[i].module_name);
			exit(0);
			break;
		}


		/*
		 * Generate the file name to be opened.
		 */
		strncpy(file_name, passed_file_name, MAX_FILE_NAME);
		if (upper_case_flag == 0)  {
			if (gz_flag != 0)  {
				file_name[file_name_length - 11] = tolower(attrib_files[i].module_name[0]);
				file_name[file_name_length - 10] = tolower(attrib_files[i].module_name[1]);
				file_name[file_name_length -  9] = tolower(attrib_files[i].module_name[2]);
				file_name[file_name_length -  8] = tolower(attrib_files[i].module_name[3]);
			}
			else  {
				file_name[file_name_length -  8] = tolower(attrib_files[i].module_name[0]);
				file_name[file_name_length -  7] = tolower(attrib_files[i].module_name[1]);
				file_name[file_name_length -  6] = tolower(attrib_files[i].module_name[2]);
				file_name[file_name_length -  5] = tolower(attrib_files[i].module_name[3]);
			}
		}
		else  {
			if (gz_flag != 0)  {
				file_name[file_name_length - 11] = attrib_files[i].module_name[0];
				file_name[file_name_length - 10] = attrib_files[i].module_name[1];
				file_name[file_name_length -  9] = attrib_files[i].module_name[2];
				file_name[file_name_length -  8] = attrib_files[i].module_name[3];
			}
			else  {
				file_name[file_name_length -  8] = attrib_files[i].module_name[0];
				file_name[file_name_length -  7] = attrib_files[i].module_name[1];
				file_name[file_name_length -  6] = attrib_files[i].module_name[2];
				file_name[file_name_length -  5] = attrib_files[i].module_name[3];
			}
		}
		/*
		 * Open and process the file.
		 */
		if (begin_ddf(file_name) >= 0)  {
			while (get_subfield(&subfield) != 0)  {
				if (strcmp(subfield.tag, "ATPR") == 0)  {
					if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "MODN") == 0))  {
						if (subfield.length != 4)  {
							fprintf(stderr, "Attribute module name (%.*s) is not 4 characters long.\n", subfield.length, subfield.value);
							continue;
						}
						if (strncmp(subfield.value, attrib_files[i].module_name, 4) != 0)  {
							fprintf(stderr, "Module name in record (%.*s) doesn't match global module name.  Entry ignored.\n",
									subfield.length, subfield.value);
							continue;
						}
					}
					else if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
						save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
						record_id = strtol(subfield.value, (char **)0, 10);
						subfield.value[subfield.length] = save_byte;

						/*
						 * Hopefully, record IDs won't backtrack very often,
						 * but I have seen it happen.  Print a warning if it
						 * does happen.
						 */
						if ((record_id - 1) < attrib_files[i].num_attrib)  {
							fprintf(stderr, "Warning:  Record IDs don't appear to be sequential in file %s.  Some attributes may be lost or corrupted.\n", file_name);
						}
						else  {
							attrib_files[i].num_attrib++;

							/*
							 * If the record_id numbers exceed the number of attribs,
							 * then make a hole in the attrib table to accommodate them.
							 */
							if ((record_id - 1) > attrib_files[i].num_attrib)  {
								attrib_files[i].num_attrib = record_id - 1;
							}

							/*
							 * If we need more space, get it.
							 */
							if (attrib_files[i].num_attrib > (current_size - 1))  {
								current_size = attrib_files[i].num_attrib + 100;
								attrib_files[i].attrib = (struct attribute_list *)realloc(attrib_files[i].attrib, sizeof(struct attribute_list) * current_size);
								if (attrib_files[i].attrib == (struct attribute_list *)0)  {
									fprintf(stderr, "realloc of attrib_files[].attrib failed.\n");
									exit(0);
								}
							}

							/*
							 * Null out all of the attributes for the new entry.
							 */
							for (j = 0; j < MAX_EXTRA; j++)  {
								attrib_files[i].attrib[attrib_files[i].num_attrib].minor[j] = 0;
								attrib_files[i].attrib[attrib_files[i].num_attrib].major[j] = 0;
							}
						}
					}
				}
				else if (strcmp(subfield.tag, "ATTP") == 0)  {
					if (attrib_files[i].num_attrib < 0)  {
						fprintf(stderr, "Attribute labels out of sequence in %s.\n", file_name);
						exit(0);
					}
					switch (parse_type)  {
					case 0:	// main Primary Attribute Module.  (Modules with names of the form A??F.)
						if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "ENTITY_LABEL", 12) == 0))  {
							if (subfield.length == 7)  {
								save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
								attrib_files[i].attrib[record_id - 1].minor[0] = strtol(&subfield.value[3], (char **)0, 10);
								subfield.value[subfield.length] = save_byte;
		
								save_byte = subfield.value[3]; subfield.value[3] = '\0';
								attrib_files[i].attrib[record_id - 1].major[0] =  strtol(subfield.value, (char **)0, 10);
								subfield.value[3] = save_byte;
							}
							else  {
								fprintf(stderr, "unexpected attribute length (%d) in file %s\n", subfield.length, file_name);
							}
						}
						else  {
							/*
							 * Rather than have multiple attribute codes, the SDTS TVP
							 * modules encode extra attributes as additional features
							 * of the primary attribute, as part of the single ISO 8211
							 * record associated with the attribute.  (Occasionally there may still be
							 * multiple primary attributes, and hence multiple ISO 8211 records.)
							 * Because the code to parse the extra information is quite bulky,
							 * it is relegated to a separate function.
							 *
							 * If we haven't already found an attribute, then there is something
							 * wrong.  Avoid a core dump by checking num_attrib.
							 */
							major2 = 0;
							if (get_extra_attrib(*data_type, &major, &minor, &major2, &minor2, &subfield) == 0)  {
								for (j = 0; j < MAX_EXTRA; j++)  {
									if (attrib_files[i].attrib[record_id - 1].major[j] == 0)  {
										break;
									}
								}
								if (j == MAX_EXTRA)  {
									fprintf(stderr, "Ran out of space for attribute features.  One attribute is missing.\n");
									continue;
								}
								attrib_files[i].attrib[record_id - 1].major[j] = major;
								attrib_files[i].attrib[record_id - 1].minor[j] = minor;
								if (major2 != 0)  {
									if ((j + 1) == MAX_EXTRA)  {
										fprintf(stderr, "Ran out of space for attribute features.  One attribute is missing.\n");
										continue;
									}
									attrib_files[i].attrib[record_id - 1].major[j + 1] = major2;
									attrib_files[i].attrib[record_id - 1].minor[j + 1] = minor2;
								}
							}
						}
						break;
					case 1:	// Coincidence Attribute Primary Module.  (Modules with names of the form ACOI.)
						if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "COINCIDENT", 10) == 0))  {
							if (subfield.length == 2)  {
								save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
								attrib_files[i].attrib[record_id - 1].minor[0] = strtol(subfield.value, (char **)0, 10);
								subfield.value[subfield.length] = save_byte;
		
								attrib_files[i].attrib[record_id - 1].major[0] =  *data_type + 9;
							}
							else  {
								fprintf(stderr, "unexpected attribute length (%d) in file %s\n", subfield.length, file_name);
							}
						}
						else  {
							fprintf(stderr, "Unrecognized attribute label (%s) in file %s.\n", subfield.label, file_name);
						}
						break;
					case 2:	// Elevation Attribute Primary Module (meters). (Modules with names of the form AHPR.)
						if ((strstr(subfield.format, "R") != (char *)0) && (strncmp(subfield.label, "ELEVATION", 9) == 0))  {
							if (subfield.length == 8)  {
								save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
								f = strtod(subfield.value, (char **)0);
								subfield.value[subfield.length] = save_byte;
								if (f < 0.0)  {
									attrib_files[i].attrib[record_id - 1].major[0] = 25;
									attrib_files[i].attrib[record_id - 1].minor[0] = -f;
								}
								else  {
									attrib_files[i].attrib[record_id - 1].major[0] = 24;
									attrib_files[i].attrib[record_id - 1].minor[0] = f;
								}
							}
							else  {
								fprintf(stderr, "unexpected attribute length (%d) in file %s\n", subfield.length, file_name);
							}
						}
						else  {
							fprintf(stderr, "Unrecognized attribute label (%s) in file %s.\n", subfield.label, file_name);
						}
						break;
					case 3:	// Elevation Attribute Primary Module (feet). (Modules with names of the form AHPT.)
						if ((strstr(subfield.format, "R") != (char *)0) && (strncmp(subfield.label, "ELEVATION", 9) == 0))  {
							if (subfield.length == 8)  {
								save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
								f = strtod(subfield.value, (char **)0);
								subfield.value[subfield.length] = save_byte;
								if (f < 0.0)  {
									attrib_files[i].attrib[record_id - 1].major[0] = 23;
									attrib_files[i].attrib[record_id - 1].minor[0] = -f;
								}
								else  {
									if (f > 9999.0)  {
										attrib_files[i].attrib[record_id - 1].major[0] = 21;
										attrib_files[i].attrib[record_id - 1].minor[0] = f - 10000.0;
									}
									else  {
										attrib_files[i].attrib[record_id - 1].major[0] = 22;
										attrib_files[i].attrib[record_id - 1].minor[0] = f;
									}
								}
							}
							else  {
								fprintf(stderr, "unexpected attribute length (%d) in file %s\n", subfield.length, file_name);
							}
						}
						else  {
							fprintf(stderr, "Unrecognized attribute label (%s) in file %s.\n", subfield.label, file_name);
						}
						break;
					case 4:	// Route Attribute Primary Module. (Modules with names of the form ARDM.)
						if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "ROUTE_NUMBER", 12) == 0))  {
							/*
							 * In theory, these can be quite complicated.
							 * In general, they presumably can take the form:
							 *
							 * US A27Z
							 *
							 * Where US is the jurisdiction, and the route
							 * number may be all alphabetic, all numeric, or
							 * mixed.  We assume that these don't get any more
							 * complicated than this:  jurisdiction, followed
							 * by a numeric number, with something alphabetic in
							 * front or behind (but not both).
							 *
							 * The spec also seems to imply that, if there is a route type,
							 * it should follow the jurisdiction and route number,
							 * but preceed any trailing alphabetic on the route number.
							 * This would require some amazing contortions to do,
							 * so we will just tack on the route type after everything
							 * else.  (It is handled in another code block, below.)
							 */
							if (subfield.length == 7)  {
								for (j = 0; j < MAX_EXTRA; j++)  {
									if (attrib_files[i].attrib[record_id - 1].major[j] == 0)  {
										break;
									}
								}
								if (j == MAX_EXTRA)  {
									fprintf(stderr, "Ran out of space for attribute features.  One attribute is missing.\n");
									continue;
								}
								save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
								if ((ptr = strstr(subfield.value, "I")) != (char *)0)  {
									attrib_files[i].attrib[record_id - 1].major[j] = 172;
									k = ptr - subfield.value + 1;
								}
								else if ((ptr = strstr(subfield.value, "US")) != (char *)0)  {
									attrib_files[i].attrib[record_id - 1].major[j] = 173;
									k = ptr - subfield.value + 2;
								}
								else if ((ptr = strstr(subfield.value, "SR")) != (char *)0)  {
									attrib_files[i].attrib[record_id - 1].major[j] = 174;
									k = ptr - subfield.value + 2;
								}
								else if ((ptr = strstr(subfield.value, "RR")) != (char *)0)  {
									attrib_files[i].attrib[record_id - 1].major[j] = 175;
									k = ptr - subfield.value + 2;
								}
								else if ((ptr = strstr(subfield.value, "CR")) != (char *)0)  {
									attrib_files[i].attrib[record_id - 1].major[j] = 176;
									k = ptr - subfield.value + 2;
								}
								else  {
									k = 0;
								}
								ptr += k;
								while ((*ptr != '\0') && (*ptr == ' '))  {ptr++;}
								if (*ptr != '\0')  {
									/* We have jurisdiction, followed by numeric route. */
									if ((*ptr >= '0') && (*ptr <= '9'))  {
										attrib_files[i].attrib[record_id - 1].minor[j] = strtol(ptr, &ptr, 10);
										while ((*ptr != '\0') && (*ptr == ' '))  {ptr++;}
										if (*ptr != '\0')  {
											if ((j + 1) == MAX_EXTRA)  {
												fprintf(stderr, "Ran out of space for attribute features.  One attribute is missing.\n");
												continue;
											}
											j++;
											attrib_files[i].attrib[record_id - 1].major[j] = 177;
											attrib_files[i].attrib[record_id - 1].minor[j] = *ptr - 'A' + 1;
											ptr++;
											if ((*ptr != '\0') && (*ptr != ' '))  {
												attrib_files[i].attrib[record_id - 1].minor[j] *= 100;
												attrib_files[i].attrib[record_id - 1].minor[j] += *ptr - 'A' + 1;
											}
										}
									}
									else  {
										/*
										 * We have jurisdiction, followed by alphabetic route.
										 * The spec seems to imply that the alpha code should
										 * preceed the jurisdictional code, so do it.
										 */
										if ((j + 1) == MAX_EXTRA)  {
											fprintf(stderr, "Ran out of space for attribute features.  One attribute is missing.\n");
											continue;
										}
										attrib_files[i].attrib[record_id - 1].major[j + 1] = attrib_files[i].attrib[record_id - 1].major[j];
										attrib_files[i].attrib[record_id - 1].minor[j] = *ptr - 'A' + 1;
										ptr++;
										if ((*ptr != '\0') && (*ptr != ' '))  {
											attrib_files[i].attrib[record_id - 1].minor[j] *= 100;
											attrib_files[i].attrib[record_id - 1].minor[j] += *ptr - 'A' + 1;
										}
										attrib_files[i].attrib[record_id - 1].minor[j + 1] = strtol(ptr, (char **)0, 10);
									}
								}
								else  {
									attrib_files[i].attrib[record_id - 1].minor[j] = 0;
								}
							}
							else  {
								fprintf(stderr, "unexpected attribute length (%d) in file %s\n", subfield.length, file_name);
							}
						}
						else if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "ROUTE_TYPE", 10) == 0))  {
							if (subfield.length == 9)  {
								for (j = 0; j < MAX_EXTRA; j++)  {
									if (attrib_files[i].attrib[record_id - 1].major[j] == 0)  {
										break;
									}
								}
								if (j == MAX_EXTRA)  {
									fprintf(stderr, "Ran out of space for attribute features.  One attribute is missing.\n");
									continue;
								}
								if (strncmp(subfield.value, "Bypass", 6) == 0)  {
									attrib_files[i].attrib[record_id - 1].minor[j] = 615;
								}
								else if (strncmp(subfield.value, "Alternate", 9) == 0)  {
									attrib_files[i].attrib[record_id - 1].minor[j] = 616;
								}
								else if (strncmp(subfield.value, "Business", 8) == 0)  {
									attrib_files[i].attrib[record_id - 1].minor[j] = 617;
								}
								else if (strncmp(subfield.value, "Spur", 4) == 0)  {
									attrib_files[i].attrib[record_id - 1].minor[j] = 619;
								}
								else if (strncmp(subfield.value, "Loop", 4) == 0)  {
									attrib_files[i].attrib[record_id - 1].minor[j] = 620;
								}
								else if (strncmp(subfield.value, "Connector", 9) == 0)  {
									attrib_files[i].attrib[record_id - 1].minor[j] = 621;
								}
								else if (strncmp(subfield.value, "Truck", 5) == 0)  {
									attrib_files[i].attrib[record_id - 1].minor[j] = 622;
								}
								else  {
									continue;
								}
								attrib_files[i].attrib[record_id - 1].major[j] = 170;
							}
							else  {
								fprintf(stderr, "unexpected attribute length (%d) in file %s\n", subfield.length, file_name);
							}
						}
						else  {
							fprintf(stderr, "Unrecognized attribute label (%s) in file %s.\n", subfield.label, file_name);
						}
						break;
					case 5:	// Agency Attribute Primary Module. (Modules with names of the form ABDM.)
						if ((strstr(subfield.format, "A") != (char *)0) && (strncmp(subfield.label, "AGENCY", 6) == 0))  {
							if (subfield.length == 3)  {
								attrib_files[i].attrib[record_id - 1].major[0] = 97;
								save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
								attrib_files[i].attrib[record_id - 1].minor[0] = strtol(subfield.value, (char **)0, 10);
								subfield.value[subfield.length] = save_byte;
							}
							else  {
								fprintf(stderr, "unexpected attribute length (%d) in file %s\n", subfield.length, file_name);
							}
						}
						else  {
							fprintf(stderr, "Unrecognized attribute label (%s) in file %s.\n", subfield.label, file_name);
						}
						break;
					}
				}
			}
			/* We are done with this file, so close it. */
			end_ddf();
		}
		attrib_files[i].num_attrib++;
	}

	return num_attrib_files;
}







/*
 * Because of the way attributes are stored in the main Primary Attribute File,
 * we can end up with duplicate attributes in the list.  This routine removes
 * the duplicates.  It removes the earliest occurence(s) of the dups since
 * that appears to be most likely to make the list ordering match the ordering
 * of the original (pre-SDTS) files.  As far as I know, there is no preferred
 * ordering for attribute lists.
 *
 * As an example, the following attribute list:
 *
 * "    50   412    50   202    50   610"
 *
 * contains the "50   610" attribute (which stands for "INTERMITTENT").
 * This attribute is coded in SDTS as a flag in the attribute record.
 * The attribute list will be encoded as two SDTS attribute records,
 * one for "50   412" (Stream) and one for "50   202" (Closure Line).
 * Each of these records will have the "INTERMITTENT" flag set.
 *
 * When we decode these records, we will end up with two copies of "50   610".
 * One of these should be removed.
 */
void
uniq_attrib(struct attribute **initial_attrib, short *attrib)
{
	short i;
	struct attribute **current_base;
	struct attribute **current_attrib;
	struct attribute **search_attrib;
	struct attribute **next_attrib;
	struct attribute **prev_attrib;

	current_base = initial_attrib;
	for (i = 0; i < *attrib; i++)  {
		search_attrib = current_base;
		while (*(search_attrib = &((*search_attrib)->attribute)) != (struct attribute *)0)  {
			if (((*search_attrib)->major != 177) &&		// 177 is a special case for spelling out alphabetic items
			    ((*search_attrib)->major == (*current_base)->major) &&
			    ((*search_attrib)->minor == (*current_base)->minor))  {
				/*
				 * We have found a duplicate.  Remove the earlier entry.
				 *
				 * It is easier to alter the list by unlinking the
				 * last entry than by removing an entry from the middle
				 * of the list.  Thus, we shift the data backwards
				 * to fill in the unwanted entry, rather than removing the
				 * actual list element of the unwanted entry.
				 * Then we simply unlink and free the final entry.
				 */
				prev_attrib = (struct attribute **)0;	// Induce a core dump if there is a bug in the code.
				current_attrib = current_base;
				next_attrib = current_base;
				while (*(next_attrib = &((*next_attrib)->attribute)) != (struct attribute *)0)  {
					(*current_attrib)->major = (*next_attrib)->major;
					(*current_attrib)->minor = (*next_attrib)->minor;
					prev_attrib = current_attrib;
					current_attrib = next_attrib;
				}
				(*prev_attrib)->attribute = (struct attribute *)0;
				free(*current_attrib);
				(*attrib)--;
				search_attrib = current_base;
			}
		}
		current_base = &((*current_base)->attribute);
	}
}




/*
 * Read the CATS module and try to find the theme.
 */
void get_theme(char *passed_file_name, char *category_name, int32_t upper_case_flag, int32_t gz_flag)
{
	struct subfield subfield;
	int32_t file_name_length;
	char file_name[MAX_FILE_NAME + 1];
	char lookin_for[4];
	int32_t got_it;
	int32_t i;


	/*
	 * Generate the file name for the CATS module.
	 */
	strncpy(file_name, passed_file_name, MAX_FILE_NAME);
	file_name[MAX_FILE_NAME] = '\0';
	file_name_length = strlen(file_name);
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			lookin_for[0] = toupper(file_name[file_name_length - 11]);
			lookin_for[1] = toupper(file_name[file_name_length - 10]);
			lookin_for[2] = toupper(file_name[file_name_length -  9]);
			lookin_for[3] = toupper(file_name[file_name_length -  8]);

			file_name[file_name_length - 11] = 'c';
			file_name[file_name_length - 10] = 'a';
			file_name[file_name_length -  9] = 't';
			file_name[file_name_length -  8] = 's';
		}
		else  {
			lookin_for[0] = toupper(file_name[file_name_length -  8]);
			lookin_for[1] = toupper(file_name[file_name_length -  7]);
			lookin_for[2] = toupper(file_name[file_name_length -  6]);
			lookin_for[3] = toupper(file_name[file_name_length -  5]);

			file_name[file_name_length -  8] = 'c';
			file_name[file_name_length -  7] = 'a';
			file_name[file_name_length -  6] = 't';
			file_name[file_name_length -  5] = 's';
		}
	}
	else  {
		if (gz_flag != 0)  {
			lookin_for[0] = file_name[file_name_length - 11];
			lookin_for[1] = file_name[file_name_length - 10];
			lookin_for[2] = file_name[file_name_length -  9];
			lookin_for[3] = file_name[file_name_length -  8];

			file_name[file_name_length - 11] = 'C';
			file_name[file_name_length - 10] = 'A';
			file_name[file_name_length -  9] = 'T';
			file_name[file_name_length -  8] = 'S';
		}
		else  {
			lookin_for[0] = file_name[file_name_length -  8];
			lookin_for[1] = file_name[file_name_length -  7];
			lookin_for[2] = file_name[file_name_length -  6];
			lookin_for[3] = file_name[file_name_length -  5];

			file_name[file_name_length -  8] = 'C';
			file_name[file_name_length -  7] = 'A';
			file_name[file_name_length -  6] = 'T';
			file_name[file_name_length -  5] = 'S';
		}
	}
	/*
	 * Open and process the file.
	 */
	got_it = 0;
	if (begin_ddf(file_name) >= 0)  {
		while (get_subfield(&subfield) != 0)  {
			if (strcmp(subfield.tag, "CATS") == 0)  {
				if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "NAME") == 0))  {
					if (subfield.length != 4)  {
						fprintf(stderr, "Attribute module name (%.*s) is not 4 characters long.\n", subfield.length, subfield.value);
						continue;
					}
					if (strncmp(subfield.value, lookin_for, 4) == 0)  {
						got_it = 1;
					}
					else  {
						got_it = 0;
					}
				}
				else if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "THEM") == 0))  {
					if (got_it != 0)  {
						if ((subfield.length != 20) || (subfield.value[0] == ' '))  {
							continue;
						}
						strncpy(category_name, subfield.value, subfield.length);
						for (i = 19; i >= 0; i--)  {
							if (category_name[i] != ' ')  {
								category_name[i + 1] = '\0';
								break;
							}
						}

						return;
					}
				}
			}
		}
	}

	return;
}
