/*
 * =========================================================================
 * dlg.h - A header file to define parameters for DLG files.
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


#define DLG_RECORD_LENGTH		80
#define MAX_ATTRIB_RECORD_LENGTH	1000


#define	HYPSOGRAPHY		20
#define	HYDROGRAPHY		50
#define	VEG_SURFACE_COVER	70
#define	NON_VEG_FEATURES	80
#define	BOUNDARIES		90
#define	SURVEY_CONTROL		150
#define	ROADS_AND_TRAILS	170
#define	RAILROADS		180
#define	PIPE_TRANS_LINES	190
#define	MANMADE_FEATURES	200
#define	PUBLIC_LAND_SURVEYS	300

#define MAX_A_ATTRIB		100		// Maximum number of Area attributes
#define MAX_L_ATTRIB		100		// Maximum number of Line attributes
#define MAX_POLY_NUM		MAX_AREAS	// Maximum number of stored polygon attribute references
#define MAX_LINE_LIST		2000		// Maximum size of a line list for output.
#define MAX_EXTRA		8		// Maximum number of attributes per line, area, or node entry
#define MAX_ATTRIB_FILES	10		// Maximum number of SDTS attribute files that we can read in

#define MAX_LINES 25938		// Theoretical maximum number of lines in a 100K DLG file.  This number may be out of date.
#define MAX_AREAS 25960		// Theoretical maximum number of areas in a 100K DLG file.  This number may be out of date.
#define MAX_NODES 25960		// Theoretical maximum number of nodes in a 100K DLG file.  This number may be out of date.


/*
 * For storing linked lists of points.
 */
struct point  {
	double x;
	double y;
	struct point *point;
};


/*
 * Storage for attribute types.
 */
struct maj_min {
	short major;
	short minor;
};
struct attribute  {
	short major;
	short minor;
	struct attribute *attribute;
};

/*
 * The sizes of the nodes, areas, and lines arrays are their theoretical maximum values.
 * It would probably be cooler to malloc() these as we go, but coolness was not an
 * objective of this program.  It would still be cool to read the maximum values from
 * the DLG file headers and check them against the values below to verify that
 * the standards haven't changed and left this program behind.
 */
struct nodes  {
	short id;
	double x;
	double y;
	short number_attrib;
	struct attribute *attribute;
};

struct areas  {
	short id;
	double x;
	double y;
	short number_attrib;
	struct attribute *attribute;
};

struct lines  {
	short id;
	short start_node;
	short end_node;
	short left_area;
	short right_area;
	short number_coords;
	struct point *point;
	short number_attrib;
	struct attribute *attribute;
};


/*
 * Arrays to keep track of attributes from various SDTS files.
 */
struct attribute_list  {
	short major[MAX_EXTRA];
	short minor[MAX_EXTRA];
};
struct polygon_attrib  {
	short poly_id;
	int32_t attrib;
	char module_num;
};



void fill_area(struct datum *, double, double, int32_t, struct image_corners *);
void process_dlg_optional(int, int, struct image_corners *, int32_t);
int32_t process_dlg_sdts(char *, char *, int32_t, struct image_corners *, int32_t, int32_t);
void draw_lines(struct datum *, struct point *, int32_t, struct image_corners *);
void process_attrib(char *);
