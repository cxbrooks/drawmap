/*
 * =========================================================================
 * colors.h - A header file to define user-defineable color parameters.
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

/*
 * This file defines the color scheme for a drawmap-generated map.
 * Some limited color information is included in drawmap.h, because it isn't
 * intended to be changed and because most drawing functions need it.
 * The remainder of the color information is in this file, so that it
 * can be collected in one place.  The intent is that, if users want to
 * experiment with different color schemes, they should be able to do so
 * by modifying only this file, and the color map code in drawmap.c.
 *
 * We begin with a general discussion of the color scheme, and the
 * philosophy behind it.
 *
 * The CIE diagram (not shown here, but readily available from reference sources)
 * is a horseshoe-shaped chart that illustrates the range of colors that humans
 * can perceive.  CMYK printers and computer monitors roughly (but not completely)
 * coincide in the portion of the CIE diagram that they occupy.  They occupy triangular
 * regions (with their respective primary colors at the corners) that encompass
 * the center of the diagram, but don't extend to the edges of the chart.  Thus,
 * neither a monitor nor a printer can generally display all human-viewable colors,
 * but rather only the colors that fall within their respective triangular regions.
 * The triangular region for a monitor or printer is called its "gamut".
 *
 * In general, a spiral on the CIE chromaticity diagram is considered
 * a good way to encode sequential data.  The spiral begins with a relatively
 * saturated version of a color and spirals inward to reach less-saturated, whiter
 * colors.  Since relief maps generally start with a greenish color near sea
 * level, the spiral should go something like:  green, yellow, orange, red,
 * magenta, blue, cyan, lighter green, lighter yellow, ligher orange, lighter
 * red, lighter magenta, lighter blue, lighter cyan, white.
 *
 * After trying the above color sequence, it appears prudent to eliminate blue
 * from the rotation, because it makes it look like there are big lakes
 * everywhere.
 *
 * Because the monitor and printer gamuts don't, in general, completely coincide,
 * if we want to be able to display and print on a wide variety of hardware,
 * fully-saturated colors are not a good idea for the outer portions of the spiral.
 * (A fully-saturated color for a monitor, might fall outside of the triangular
 * region for a printer.)  We will use fully-saturated colors anyway, because they
 * look good on a monitor.
 * 
 * For elevations below sea level, it seems reasonable to run
 * the spiral backwards through cyan, blue, magenta, and black, as the
 * elevations proceed from 0 to the trench bottoms.  For now, we aren't going
 * to have any really deep depths, so we will just use cyan for any elevations
 * below zero.
 *
 * Since most Americans prefer to deal in feet, we will use 1000-foot
 * intervals, which are pretty close to 300-meter intervals.  Because the DEM
 * data is in meters, and because I prefer metric units, drawmap internally
 * uses metric values.
 *
 * If we just wanted to indicate bands of elevation, the color spiral described
 * above would be enough.  However, we also want to give some indication of
 * the variations in the terrain within each elevation band.  This is normally
 * done with shaded relief, in which we shade the map as if it were illuminated
 * with sunlight from above one of the corners.  Thus, for each color in the
 * color spiral, we need to define a range of shades that will give some
 * indication of light and shadow.  We do this by defining color bands.
 * Each band represents a single color, with 16 different intensities.
 *
 * The default colors are defined as follows, where the [R,G,B] values
 * represent the most intense shade in each color band:
 *
 *       Feet		Color		[R,G,B]			Meters
 *   --------------	----------	-------------		----------------------
 *         Below 0	Cyan		[ 60,255,255]		(center of Earth to 0)
 *       0 to 1000	Green		[ 60,255, 60]		(   0 to  305)
 *    1000 to 2000	Yellow		[255,255, 60]		( 305 to  610)
 *    2000 to 3000	Orange		[255,165, 60]		( 610 to  914)
 *    3000 to 4000	Red		[255, 60, 60]		( 914 to 1219)
 *    4000 to 5000	Magenta		[255, 60,255]		(1219 to 1524)
 *    5000 to 6000	Cyan		[ 60,255,255]		(1524 to 1829)
 *    6000 to 7000	Light Green	[165,255,165]		(1829 to 2134)
 *    7000 to 8000	Light Yellow	[255,255,165]		(2134 to 2438)
 *    8000 to 9000 	Light Orange	[255,210,165]		(2438 to 2743)
 *    9000 to 10000	Light Red	[255,165,165]		(2743 to 3048)
 *   10000 to 11000	Light Magenta	[255,165,255]		(3048 to 3353)
 *   11000 to 12000	Light Blue	[165,165,255]		(3353 to 3658)
 *   12000 to 13000	Light Cyan	[165,255,255]		(3658 to 3962)
 *     Above 13000	White		[255,255,255]		(3962 to infinity)
 *
 * Also provided, below, is a more natural-looking color map.  The user can
 * select it via a command-line option.
 *
 * Thus ends the discussion of philosophy.  We now go on to define the specifics
 * of how the color table is encoded.
 */


/*
 * This is the master color table for maps generated by drawmap.
 *
 * It is designed so that you can easily change the color scheme if you want to.
 *
 * The SUN rasterfile color map provides 256 RGB colors.  We divide them
 * into 16 color bands.  This table defines how the SUN color map is filled
 * with colors.
 *
 * Except for band 14, which is currently unused, and band 15, which is
 * reserved for some special colors, the bands each represent a range of
 * elevations.  Within each band, there is a range of intensities for a given
 * color, with each intensity representing a different steepness of terrain
 * within that range of elevations.
 *
 * The first entry in each row of the table is an index into the SUN color map.
 * Currently, these are simply spaced by 16 colors.  (They are defined in
 * drawmap.h.)  Don't mess with them unless you really understand how the
 * drawmap code works.
 *
 * The second entry in each row is a maximum elevation for that
 * color band (in meters).  For example, color band 6 is used to for coloring
 * terrain that has elevations greater than the maximum elevation for color
 * band 5, but less than or equal to the maximum elevation for color band 6.
 * By default, color band 6 is Light Green.  The red, green, and blue values
 * for color band 6 are set so that the first color in that color band is
 * Light Green.  The remaining 15 colors in that color band are derived from
 * the initial entry by simply dimming them more and more until the last color
 * in the band is black.
 *
 * All of color bands 0 through 13 work in the same way.  Color band
 * 14 is totally unused, and is available for experimentation.  Color
 * band 15 is really not a band of colors at all, but rather just a collection
 * of individually-defined colors for special uses.  The colors in that
 * band are bright primary colors and the like, intended for applications such as
 * coloring streams bright blue and roads bright red.  Changing any of
 * the numbers, in color_tab, for color band 15 is a waste of time, because
 * they aren't currently used.  The color band 15 entry is included for
 * completeness, and in case we some day want to truly use all 16 color bands
 * for elevation shading.  The colors for band 15 are actually defined by the
 * "brights" array, further on in this file.
 *
 * Elevations 0 and HIGHEST_ELEVATION get some special handling.  In
 * particular, since elevations below 0 are rare, we simply re-use the CYAN
 * from band 5 when we color a sub-sea-level point.  HIGHEST_ELEVATION is
 * in the mid-stratosphere to ensure that there are no land features higher
 * than it is.  We use HIGHEST_ELEVATION as a special flag, to set regions
 * of the map to white by giving them an elevation of exactly HIGHEST_ELEVATION.
 * Thus, any regions of the map for which the user does not provide DEM data,
 * are initialized to HIGHEST_ELEVATION and end up as simple white background.
 *
 * By default, the elevations are selected so that each color band
 * represents a 1000-foot range of elevations.  You can change the
 * elevations as you wish, as long as you follow these rules:
 *
 *   (1) The elevation for color band 0 should always be greater than zero.
 *   (2) The elevation for the last valid color band (normally band 13,
 *       but could be band 14 if you choose to use it) must be set to
 *       HIGHEST_ELEVATION.  (If you do choose to use band 14, you will
 *       need to make some simple changes in the drawmap code.)
 *   (3) The elevations must be arranged in ascending order.
 *   (4) Remember that elevations below 0 re-use the CYAN color band
 *       from band 5.  See to it that C_INDEX_SEA_? (defined below) is set to the
 *       same value as the C_INDEX_? value for a CYAN color band.  (Or choose
 *       some other color band appropriate for sub-sea-level terrain.)
 *
 * You can also change the default colors, by changing their red, green, and blue
 * components.  The default color scheme is not very "natural" since it is based on
 * the CIE spiral described above.
 *
 * There are actually four color tables defined:
 * One very-garish one based on the spiral, another more natural one that includes the colors
 * typically associated with the vegetation and rocks at various altitudes,
 * a neutral one that is intended to look natural without being very obtrusive, and
 * one that is reminiscent of maps in school textbooks and is sort of midway between
 * the spiral and the natural.
 *
 * The -n option selects between the tables.
 *
 * Of course, "naturalness" is highly subjective, and your preferred colors may be different.
 */
struct color_tab  {
	unsigned char	c_index;	// index into the color map for the start of this color band [0-255]
	int32_t		max_elevation;	// maximum elevation for this color band (in meters)
	unsigned char	red;		// red component of the color for this color band [0 - 255]
	unsigned char	green;		// green component of the color for this color band [0 - 255]
	unsigned char	blue;		// blue component of the color for this color band [0 - 255]
};

struct color_tab color_tab_neutral[16] =  {
	{ C_INDEX_0,	              305,	190,	255,	175, },	// LIGHT GREEN
	{ C_INDEX_1,	              610,	190,	244,	174, },	// interpolated
	{ C_INDEX_2,	              914,	190,	233,	174, },	// interpolated
	{ C_INDEX_3,	             1219,	190,	222,	173, },	// interpolated
	{ C_INDEX_4,	             1524,	190,	211,	172, },	// interpolated
	{ C_INDEX_5,	             1829,	190,	200,	170, },	// OLIVE GREEN
	{ C_INDEX_6,	             2134,	185,	187,	165, },	// interpolated
	{ C_INDEX_7,	             2438,	180,	174,	160, },	// interpolated
	{ C_INDEX_8,	             2743,	175,	162,	155, },	// interpolated
	{ C_INDEX_9,	             3048,	170,	150,	150, },	// BROWN
	{ C_INDEX_10,	             3353,	160,	160,	160, },	// interpolated
	{ C_INDEX_11,	             3658,	190,	190,	190, },	// interpolated
	{ C_INDEX_12,	             3962,	220,	220,	220, },	// interpolated
	{ C_INDEX_13,	HIGHEST_ELEVATION,	255,	255,	255, },	// WHITE
	{ C_INDEX_14,	HIGHEST_ELEVATION /* stratosphere */,	  0,	  0,	  0, },	// currently unused
	{ C_INDEX_15,	               -1 /*              */,	  0,	  0,	  0, },	// This line is filler --- the entries here are unused
};
#define C_INDEX_SEA_NEUTRAL	C_INDEX_10

struct color_tab color_tab_natural[16] =  {
	{ C_INDEX_0,	              305,	190,	255,	175, },	// LIGHT GREEN
	{ C_INDEX_1,	              610,	195,	237,	165, },	// GRAYISH GREEN
	{ C_INDEX_2,	              914,	200,	210,	156, },	// GREENISH ORANGE
	{ C_INDEX_3,	             1219,	205,	194,	147, },	// OLIVE
	{ C_INDEX_4,	             1524,	210,	178,	138, },	// OLIVE ORANGE
	{ C_INDEX_5,	             1829,	215,	162,	129, },	// LIGHT ORANGE
	{ C_INDEX_6,	             2134,	220,	145,	120, },	// RED ORANGE
	{ C_INDEX_7,	             2438,	207,	150,	135, },	// RUST
	{ C_INDEX_8,	             2743,	193,	155,	150, },	// DULL BROWN
	{ C_INDEX_9,	             3048,	179,	160,	165, },	// LIGHT MAGENTA
	{ C_INDEX_10,	             3353,	165,	165,	180, },	// GRAY
	{ C_INDEX_11,	             3658,	210,	210,	255, },	// LIGHT CYAN
	{ C_INDEX_12,	             3962,	232,	232,	255, },	// LIGHTER CYAN
	{ C_INDEX_13,	HIGHEST_ELEVATION,	255,	255,	255, },	// WHITE
	{ C_INDEX_14,	HIGHEST_ELEVATION /* stratosphere */,	  0,	  0,	  0, },	// currently unused
	{ C_INDEX_15,	               -1 /*              */,	  0,	  0,	  0, },	// This line is filler --- the entries here are unused
};
#define C_INDEX_SEA_NATURAL	C_INDEX_10

struct color_tab color_tab_textbook[16] =  {
	{ C_INDEX_0,	              305,	 60,	255,	 60, },	// GREEN
	{ C_INDEX_1,	              610,	150,	255,	 30, },	// YELLOWISH GREEN
	{ C_INDEX_2,	              914,	200,	255,	  0, },	// GREENISH YELLOW
	{ C_INDEX_3,	             1219,	218,	225,	 20, },	// OLIVE
	{ C_INDEX_4,	             1524,	237,	205,	 40, },	// LIGHT ORANGE
	{ C_INDEX_5,	             1829,	255,	185,	 60, },	// ORANGE
	{ C_INDEX_6,	             2134,	255,	145,	 95, },	// RUST
	{ C_INDEX_7,	             2438,	255,	125,	115, },	// RED ORANGE
	{ C_INDEX_8,	             2743,	225,	138,	150, },	// LIGHT MAGENTA
	{ C_INDEX_9,	             3048,	195,	152,	150, },	// BROWNISH MAGENTA
	{ C_INDEX_10,	             3353,	165,	165,	180, },	// GRAY
	{ C_INDEX_11,	             3658,	210,	210,	255, },	// LIGHT CYAN
	{ C_INDEX_12,	             3962,	232,	232,	255, },	// LIGHTER CYAN
	{ C_INDEX_13,	HIGHEST_ELEVATION,	255,	255,	255, },	// WHITE
	{ C_INDEX_14,	HIGHEST_ELEVATION /* stratosphere */,	  0,	  0,	  0, },	// currently unused
	{ C_INDEX_15,	               -1 /*              */,	  0,	  0,	  0, },	// This line is filler --- the entries here are unused
};
#define C_INDEX_SEA_TEXTBOOK	C_INDEX_10

struct color_tab color_tab_spiral[16] =  {
	{ C_INDEX_0,	              305 /*    1000 feet */,	 60,	255,	 60, },	// GREEN
	{ C_INDEX_1,	              610 /*    2000 feet */,	255,	255,	 60, },	// YELLOW
	{ C_INDEX_2,	              914 /*    3000 feet */,	255,	165,	 60, },	// ORANGE
	{ C_INDEX_3,	             1219 /*    4000 feet */,	255,	 60,	 60, },	// RED
	{ C_INDEX_4,	             1524 /*    5000 feet */,	255,	 60,	255, },	// MAGENTA
	{ C_INDEX_5,	             1829 /*    6000 feet */,	 60,	255,	255, },	// CYAN
	{ C_INDEX_6,	             2134 /*    7000 feet */,	165,	255,	165, },	// LIGHT GREEN
	{ C_INDEX_7,	             2438 /*    8000 feet */,	255,	255,	165, },	// LIGHT YELLOW
	{ C_INDEX_8,	             2743 /*    9000 feet */,	255,	210,	165, },	// LIGHT ORANGE
	{ C_INDEX_9,	             3048 /*   10000 feet */,	255,	165,	165, },	// LIGHT RED
	{ C_INDEX_10,	             3353 /*   11000 feet */,	255,	165,	255, },	// LIGHT MAGENTA
	{ C_INDEX_11,	             3658 /*   12000 feet */,	165,	165,	255, },	// LIGHT BLUE
	{ C_INDEX_12,	             3962 /*   13000 feet */,	165,	255,	255, },	// LIGHT CYAN
	{ C_INDEX_13,	HIGHEST_ELEVATION /* stratosphere */,	255,	255,	255, },	// WHITE
	{ C_INDEX_14,	HIGHEST_ELEVATION /* stratosphere */,	  0,	  0,	  0, },	// currently unused
	{ C_INDEX_15,	               -1 /*              */,	  0,	  0,	  0, },	// This line is filler --- the entries here are unused
};
#define C_INDEX_SEA_SPIRAL	C_INDEX_11

/*
 * Set the following parameter to the total number of color bands you have defined.
 * It is a good idea if all tables have the same number of valid bands,
 * but not strictly necessary if you are careful and know what you are
 * doing.  Note, in particular, the places in the code where MAX_VALID_BANDS is used.
 */
#define MAX_VALID_BANDS 14

#define NUM_COLOR_TABS 4	// Set this to the total number of color tables you have defined.



/*
 * These are the SUN rasterfile color map values for various individual
 * colors.  These colors occupy the slots in color band 15, as described above.
 */
struct brights  {
	unsigned char	c_index;	// index into the color map for this color [C_INDEX_15 - 255]
	unsigned char	red;		// red component of the color for this color band [0 - 255]
	unsigned char	green;		// green component of the color for this color band [0 - 255]
	unsigned char	blue;		// blue component of the color for this color band [0 - 255]
} brights[16] =  {
	{ C_INDEX_15,		210,	 15,	 30, },	// BRIGHT RED
	{ C_INDEX_15 + 1,		 50,	210,	 50, },	// BRIGHT GREEN
	{ C_INDEX_15 + 2,		 40,	 60,	190, },	// BRIGHT BLUE
	{ C_INDEX_15 + 3,		  0,	  0,	  0, },	// BLACK
	{ C_INDEX_15 + 4,		120,	120,	120, },	// GRAY
	{ C_INDEX_15 + 5,		255,	210,	165, },	// LIGHT ORANGE
	{ C_INDEX_15 + 6,		255,	255,	255, },	// WHITE
	{ C_INDEX_15 + 7,		  0,	  0,	  0, },	// unused
	{ C_INDEX_15 + 8,		  0,	  0,	  0, },	// unused
	{ C_INDEX_15 + 9,		  0,	  0,	  0, },	// unused
	{ C_INDEX_15 + 10,	  0,	  0,	  0, },	// unused
	{ C_INDEX_15 + 11,	  0,	  0,	  0, },	// unused
	{ C_INDEX_15 + 12,	  0,	  0,	  0, },	// unused
	{ C_INDEX_15 + 13,	  0,	  0,	  0, },	// unused
	{ C_INDEX_15 + 14,	  0,	  0,	  0, },	// unused
	{ C_INDEX_15 + 15,	  0,	  0,	  0, },	// unused
};
