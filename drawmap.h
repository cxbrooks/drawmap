/*
 * =========================================================================
 * drawmap.h - A header file containing global information for all of drawmap.
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



/* #define COPYRIGHT_NAME	"Fred M. Erickson" */	/* Now defined in the Makefile */

#define ONE_DEGREE_DEM_SIZE	1201

#define BUF_SIZE	16384	// Generic buffer size --- should be large enough to never cause trouble
#define MAX_GNIS_RECORD	1024	// Assumed by the code to be less than or equal to DEM_RECORD_LENGTH, defined in dem.h
#define MAX_FILE_NAME	1000


/* The borders should be at least 60, if possible.  They must be even numbers. */
#define TOP_BORDER	60
#define BOTTOM_BORDER	80
#define LEFT_BORDER	60
#define RIGHT_BORDER	60

extern int32_t bottom_border;
extern int32_t right_border;

#define	NUM_DEM	1000	/* Number of DEM files allowed on input */

#define HIGHEST_ELEVATION	32000	/* Elevation higher than any elevation expected in the DEM data */

#define SMOOTH_MAX	10	/* maximum radius of smoothing kernel */

#define OMIT_NEATLINES	1	/* If this is non-zero, then neatlines won't be drawn on the image. */


/*
 * These are SUN color map index values for shaded relief.  They are defined here
 * because most drawing functions need access to them, and because users normally
 * shouldn't change them.  User-changeable color information is defined
 * in colors.h.
 */
#define	C_INDEX_0		0
#define	C_INDEX_1		16
#define	C_INDEX_2		32	// Really more like brown than orange
#define	C_INDEX_3		48
#define	C_INDEX_4		64
#define	C_INDEX_5		80
#define	C_INDEX_6		96
#define	C_INDEX_7		112
#define	C_INDEX_8		128
#define	C_INDEX_9		144
#define	C_INDEX_10		160
#define	C_INDEX_11		176
#define	C_INDEX_12		192
#define	C_INDEX_13		208
#define	C_INDEX_14		224	// Currently unused
#define	C_INDEX_15		240
#define COLOR_CHART_WIDTH	C_INDEX_14




/*
 * These are the SUN rasterfile color map index values for various primary
 * colors.  They are included here since most drawing functions need them.
 * The actual color definition is done in colors.h
 */
#define	B_RED		(C_INDEX_15)		// BRIGHT RED
#define	B_GREEN		(C_INDEX_15 + 1)	// BRIGHT GREEN
#define	B_BLUE		(C_INDEX_15 + 2)	// BRIGHT BLUE
#define	BLACK		(C_INDEX_15 + 3)	// BLACK
#define GRAY		(C_INDEX_15 + 4)	// GRAY
#define L_ORANGE	(C_INDEX_15 + 5)	// LIGHT ORANGE
#define WHITE		(C_INDEX_15 + 6)	// WHITE



/*
 * Structure used to pass datum parameters between functions.
 */
struct datum  {
	double a;		// Semimajor ellipsoid radius (equatorial radius)
	double b;		// Semiminor ellipsoid radius (polar radius)
	double e_2;		// Eccentricity squared,  e^2 = 2*f - f*f = 1 - b*b / a*a
	double f_inv;		// Inverse flattening,  1/f = a / (a - b)
	double k0;		// scale factor along central meridian
	double a0;		// First coefficient in Redfearn integral expansion
	double a2;		// Second coefficient in Redfearn integral expansion
	double a4;		// Third coefficient in Redfearn integral expansion
	double a6;		// Fourth coefficient in Redfearn integral expansion
};

/*
 * These are the parameters for the Clarke 1866 ellipsoid, which is used with the
 * North American Datum of 1927 (NAD-27) datum.  The NAD-27 used a point on
 * Meade Ranch in Kansas as its reference origin.
 */
#define NAD27_SEMIMAJOR (6378206.4)		// 1866 Clarke ellipsoid, equatorial radius in meters
#define NAD27_SEMIMINOR (6356583.8)		// 1866 Clarke ellipsoid, polar radius in meters
#define NAD27_E_SQUARED (0.006768658)		// 1866 Clarke ellipsoid, eccentricity squared	(e^2 = 2*f - f*f = 1 - b^2 / a^2)
#define NAD27_F_INV (294.9786982)		// 1866 Clarke ellipsoid, inverse flattening	(1/f = a / (a - b))
#define NAD27_A0 (0.99830568187775514389)	// First constant in meridian integral expansion:  1 - (e^2 / 4) - (3 * e^4 / 64) - (5 * e^6 / 256)
#define NAD27_A2 (0.00254255550867060247)	// First constant in meridian integral expansion:  (3/8) * (e^2 + e^4 / 4 + 15 e^6 / 128)
#define NAD27_A4 (0.00000269808452963108)	// First constant in meridian integral expansion:  (15 / 256) * (e^4 + 3 * e^6 / 4)
#define NAD27_A6 (0.00000000353308874387)	// First constant in meridian integral expansion:  35 * e^6 / 3072

/*
 * These are the parameters for the Geodetic Reference System (GRS) 1980 ellipsoid,
 * which is used with the North American Datum of 1983 (NAD-83) datum.  This datum
 * is based on a reference point at the center of the earth, and is defined based on
 * satellite measurements.
 *
 * Other parameters:
 *  Polar radius of curvature (c) 6399593.6259 m
 *  angular velocity (w) 7292115e-11 radians/s
 *  Gravitational Constant (G) 986005e8 m^3/s^2
 *  Flattening (f) 0.00335281068118
 */
#define NAD83_SEMIMAJOR (6378137.0)		// GRS80 ellipsoid, equatorial radius
#define NAD83_SEMIMINOR (6356752.3141)		// GRS80 ellipsoid, polar radius
#define NAD83_E_SQUARED (0.00669438002290)	// GRS80 ellipsoid, e*e	(e^2 = 2*f - f*f = 1 - b^2 / a^2)
#define NAD83_F_INV (298.257222101)		// GRS80 ellipsoid, inverse flattening	(1/f = a / (a - b))
#define NAD83_A0 (0.99832429844458494622)	// First constant in meridian integral expansion:  1 - (e^2 / 4) - (3 * e^4 / 64) - (5 * e^6 / 256)
#define NAD83_A2 (0.00251460707284452333)	// First constant in meridian integral expansion:  (3/8) * (e^2 + e^4 / 4 + 15 e^6 / 128)
#define NAD83_A4 (0.00000263904662023027)	// First constant in meridian integral expansion:  (15 / 256) * (e^4 + 3 * e^6 / 4)
#define NAD83_A6 (0.00000000341804613677)	// First constant in meridian integral expansion:  35 * e^6 / 3072

/*
 * These are the parameters for the World Geodetic System (WGS) 1984
 * ellipsoid.
 * (The WGS-84 ellipsoid is virtually identical to GRS-80.)
 *
 * Other parameters:
 *  Polar radius of curvature (c) 6399593.6258 m
 *  angular velocity (w) 7292115e-11 radians/s
 *  Gravitational Constant (G) 986005e8 m^3/s^2
 *  Flattening (f) 0.00335281066474
 */
#define WGS84_SEMIMAJOR (6378137.0)		// WGS-84 ellipsoid, equatorial radius
#define WGS84_SEMIMINOR (6356752.3142)		// WGS-84 ellipsoid, polar radius
#define WGS84_E_SQUARED (0.00669437999013)	// WGS-84 ellipsoid, e*e	(e^2 = 2*f - f*f = 1 - b^2 / a^2)
#define WGS84_F_INV (298.257223563)		// WGS-84 ellipsoid, inverse flattening	(1/f = a / (a - b))
#define WGS84_A0 (0.99832429845279809866)	// First constant in meridian integral expansion:  1 - (e^2 / 4) - (3 * e^4 / 64) - (5 * e^6 / 256)
#define WGS84_A2 (0.00251460706051444693)	// First constant in meridian integral expansion:  (3/8) * (e^2 + e^4 / 4 + 15 e^6 / 128)
#define WGS84_A4 (0.00000263904659432867)	// First constant in meridian integral expansion:  (15 / 256) * (e^4 + 3 * e^6 / 4)
#define WGS84_A6 (0.00000000341804608657)	// First constant in meridian integral expansion:  35 * e^6 / 3072


/*
 * To convert to/from UTM coordinates we need to know the scale factor on the central meridian.
 * For UTM, this is always 0.9996.
 */
#define UTM_K0 (0.9996)			// UTM Scale factor on the central meridian


/*
 * This structure is for passing information about a block of image data
 * between routines.  It defines two opposite corners of the data
 * block, in terms of latitude and longitude.  It also defines the
 * x-by-y size of the block, in terms of number of data points.
 */
struct image_corners  {
	unsigned char *ptr;	// A pointer to the block of memory containing the data

	double sw_x_gp;		// lowest x UTM coordinate in data block (the _gp stands for ground planimetric coordinates)
	double sw_y_gp;		// lowest y UTM coordinate in data block
	int32_t   sw_zone;	// UTM zone of southwest corner

	double ne_x_gp;		// highest x UTM coordinate in data block
	double ne_y_gp;		// highest y UTM coordinate in data block
	int32_t   ne_zone;	// UTM zone of northeast corner

	double sw_lat;		// lowest latitude in data block
	double sw_long;		// lowest longitude in data block
	double ne_lat;		// highest latitude in data block
	double ne_long;		// highest longitude in data block

	int32_t x;		// number of samples in a row
	int32_t y;		// number of samples in a column
};



int32_t drawmap_round(double);
int32_t max(int32_t, int32_t);
double max3(double, double, double);
double min3(double, double, double);
int buf_open(const char *, int);
ssize_t buf_read(int, void *, size_t);
ssize_t buf_write(int, const void *, size_t);
ssize_t get_a_line(int, void *, size_t);
int buf_close(int);
int buf_open_z(const char *, int);
ssize_t buf_read_z(int, void *, size_t);
ssize_t get_a_line_z(int, void *, size_t);
int buf_close_z(int);
double lat_conv(char *);
double lon_conv(char *);
double find_latitude(double, double);
double find_longitude(double, double);
int32_t redfearn(struct datum *, double *, double *, int32_t *, double, double, int32_t);
int32_t redfearn_inverse(struct datum *, double, double, int32_t, double *, double *);
void decimal_degrees_to_dms(double, int32_t *, int32_t *, double *);
int32_t swab_type();

/*
 * Some macros to do swabbing.
 * LE_SWAB() does Little-Endian swabbing.
 * PDP_SWAB() does PDP-Endian swabbing.
 *
 * The argument "num" is a pointer to a int32_t word to be swabbed.
 */
#define LE_SWAB(num)  { \
	unsigned char a, b, c, d; \
	a = ((*(num)) >> 24) & 0xff; \
	b = ((*(num)) >> 16) & 0xff; \
	c = ((*(num)) >> 8) & 0xff; \
	d = (*(num)) & 0xff; \
	*(num) = d << 8; \
	*(num) = (*(num) | c) << 8; \
	*(num) = (*(num) | b) << 8; \
	*(num) = *(num) | a; \
}
#define PDP_SWAB(num)  { \
	unsigned char a, b, c, d; \
	a = ((*(num)) >> 24) & 0xff; \
	b = ((*(num)) >> 16) & 0xff; \
	c = ((*(num)) >> 8) & 0xff; \
	d = (*(num)) & 0xff; \
	*(num) = b << 8; \
	*(num) = (*(num) | a) << 8; \
	*(num) = (*(num) | d) << 8; \
	*(num) = *(num) | c; \
}
