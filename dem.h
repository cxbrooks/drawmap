/*
 * =========================================================================
 * dem.h - A header file to define parameters for DEM files.
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


#define DEM_RECORD_LENGTH	1024

/*
 * This is the structure of the data in a DEM Logical Record Type A.
 * It is generally the first record in a DEM, before the actual data
 * starts.  It is 1024 bytes long, but not all bytes are used.  The
 * old format ends at byte 864 (numbered starting from 1).  The new
 * format is basically the same, but adds additional data after byte 864.
 * Normally, the record is padded with blanks to round it out to 1024
 * bytes.  However, some people put a newline at the end of the valid
 * data, and don't include the padding, in spite of the fact that the
 * standards document specifically says to pad with blanks.
 * (In fact, according to the standard, bytes 1021-1024 of every DEM
 * record, of any type, should contain blanks.)
 *
 * No attempt has been made to make the sizes of individual variables in this
 * structure match the sizes specified in the DEM specification.  (For example,
 * using short integers for record elements specified as two-byte integers.)
 * Memory isn't at such a premium that we need to use short instead of long, or float
 * instead of double.
 */
struct dem_record_type_a  {
	/*
	 * The title block has somewhat variable contents, and does not always contain what is
	 * officially specified.  With that warning, here is the official specification.
	 * Note that bytes in the spec are numbered starting with 1 rather than 0, and that
	 * what I call the title block occupies bytes 1 through 140.
	 *	1-40: file name		(SDTS location:  IDEN/IDEN/TITL)
	 *          (The SDTS subfield also normally contains the map scale, and typically looks like:
	 *           "BOZEMAN, MT - 24000")
	 *	41-80: free-format text	(SDTS location:  DQHL/DQHL/COMT, record 1)
	 *	81-109: blank fill
	 *	110-135: SE corner longitude/latitude in SDDDMMSS.SSSS format
	 *          (SDTS location:  IDEN/IDEN/DAID)
	 *          (The SDTS subfield also contains the map scale, and typically looks like:
	 *           "LAT:: 45.625 LONG:: -111 SCALE:: 24000")
	 *	136: process code (single integer)
	 *             0= "Unspecified."
	 *             1= "Autocorrelation RESAMPLE Simple bilinear."
	 *             2= "Manual Profiling (GRIDEM) from stereomodels; Simple bilinear."
	 *             3= "DLG/hypsography CTOG 8-direction linear."
	 *             4= "Interpolation from photogrammetric system contours DCASS 8-direction linear."
	 *             5= "DLG/hypsography LINETRACE, LT4X Complex linear."
	 *             6= "DLG/hypsography CPS-3, ANUDEM, GRASS Complex polynomial."
	 *             7= "Electronic imaging (non-photogrammetric), active or passive, sensor systems."
	 *		(SDTS location:  DQHL/DQHL/COMT, record 2)
	 *		(The SDTS subfield typically looks like:
	 *		 "PROCESS CODE 1: PROCESS USED:  Autocorrelation RESAMPLE Simple bilinear.")
	 *	137: blank fill
	 *	138-140: sectional indicator, specific to 30-minute DEMs
	 *       (SDTS location:  IDEN/IDEN/TITL)
	 */
	char	title[80];	// Includes the free-format text, if we choose to pry it out.  We usually don't so choose.
	double	se_lat;
	double	se_long;
	int32_t	process_code;
//	char	sectional_ind[3];
	/*
	 * Free format 4-byte origin code.
	 *  NMD = "DEM PRODUCER: National Mapping Division, Reston, VA."
	 *  EMC = "DEM PRODUCER: Eastern Mapping Center (Mapping Applications Center), Reston, VA."
	 *  WMC = "DEM PRODUCER: Western Mapping Center, Menlo Park, CA."
	 * MCMC = "DEM PRODUCER: Mid-Continent Mapping Center, Rolla, MO."
	 * RMMC = "DEM PRODUCER: Rocky Mountain Mapping Center, Denver, CO."
	 *   FS = "DEM PRODUCER: Forest Service"
	 * GPM2 = "DEM PRODUCER: Gestalt Photo Mapper low resolution DEM."
	 * CONT = "DEM PRODUCER: Contractor."
	 * <  > = "DEM PRODUCER: <  >."
	 * blank= "DEM PRODUCER: Unspecified."
	 *
	 * (SDTS location:  DQHL/DQHL/COMT, record 3)
	 * (The SDTS subfield typically looks like:
	 *  "DEM PRODUCER:  Unspecified.")
	 */
	char	origin_code[4];	// bytes 141-144
	/*
	 * DEM level.  Can take values from 1 through 4.
	 *
	 * (SDTS location:  DQHL/DQHL/COMT, record 4.  Also DQPA/DQPA/COMT, record 1.)
	 * (The SDTS subfield typically looks like:
	 *  "DEM LEVEL 1 means: DEM created by auto correlation or manual profiling from aerial photographs.  Source photography is typically from National Aerial Photography Program or National High Altitude Photography Program.  30-minute DEM's may be derived or resampled from level 1 7.5-minute DEM's."
	 */
	int32_t	level_code;	// bytes 145-150
	/*
	 * 1=regular.  2=random, reserved for future use.
	 *
	 * (SDTS location:  RSDF/RSDF/OBRP)
	 * (The SDTS subfield typically looks like:
	 *  "G2"
	 *  Note that the conversion spec says this should be "G2" for regular, and "" for random.
	 *  I think that this basically means random is still unused and reserved for future use.)
	 */
	int32_t	elevation_pattern;	// bytes 151-156
	/*
	 * Planimetric reference system.
	 *   0=Geographic (lat/long)
	 *   1=Universal Transverse Mercator (UTM)
	 *   2=State Plane Coordinate System
	 *   3-20 defined in spec.
	 *
	 * (SDTS location:  XREF/XREF/RSNM)
	 * (The SDTS subfield typically looks like:
	 *  "UTM"
	 *  Note:  valid SDTS values are "UTM", "GEO", and "SPCS".  Codes 3-20 are apparently unsupported.)
	 */
	int32_t	plane_ref;	// bytes 157-162
	/*
	 * This is the zone for state plane and UTM systems.  It is zero for the geographic system.
	 *
	 * (SDTS location:  XREF/XREF/ZONE)
	 * (The SDTS subfield typically looks like:
	 *  "12")
	 */
	int32_t	zone;	// bytes 163-168
	/*
	 * Map projection parameters are in bytes 169-528.  There are 15 fields of
	 * floating-point data.  Since these field are set to zero when plane_ref = 0,1,2
	 * we don't normally care much about them.  In SDTS, they aren't encoded.
	 */
//	double p1, p2, p3, p4, p5, p6, p7, p8, p9, p10, p11, p12, p13, p14, p15;
	/*
	 * Unit of measure for planimetric coordinates.
	 *  0=radians
	 *  1=feet
	 *  2=meters
	 *  3=arc-seconds
	 *
	 * (These aren't encoded in SDTS.  SDTS simply requires that the units
	 * be meters for UTM and decimal degrees for geographic units.)
	 */
	int32_t plane_units;	// bytes 529-534
	/*
	 * Unit of measure for elevation coordinates.
	 * 1=feet
	 * 2=meters
	 *
	 * (SDTS location:  DDSH/DDSH/UNIT.  Also in DDOM/DDOM/ADMU, in association with individual data items, where it may be in lower case.)
	 * (The SDTS subfield typically looks like:
	 *  "METERS".
	 *  Valid SDTS values are "FEET" and "METERS".)
	 */
	int32_t elev_units;	// bytes 535-540
	/*
	 * Number of sides of the bounding polygon for the DEM file.
	 * Presently, appears to always be set to 4.
	 * This is not encoded in SDTS since it is always 4.
	 */
//	int32_t num_sides;	// bytes 541-546
	/*
	 * Four pairs of floating point numbers defining the four corners of the
	 * bounding quadrangle.  Clockwise from sw corner.  Stored as eastings and
	 * northings (with false eastings/northings included).  Bytes 547-738
	 *
	 * (SDTS location:  SPDM/DMSA/X, SPDM/DMSA/Y)
	 * (The SDTS subfield typically looks like:
	 *  "490255.500000")
	 * (There is also information in SPDM/SPDM/DTYP indicating that the 4 points form a RING.)
	 */
	double sw_x_gp;	// (the _gp stands for ground planimetric)
	double sw_y_gp;
	double nw_x_gp;
	double nw_y_gp;
	double ne_x_gp;
	double ne_y_gp;
	double se_x_gp;
	double se_y_gp;
	/*
	 * A pair of floating point numbers giving the minimum and maximum elevations.
	 * Bytes 739-786
	 *
	 * (SDTS location:  DDOM/DDOM/DVAL.  The two values normally seem to be in records
	 * 3 and 4.  They can also be distinguished by looking at DDOM/DDOM/RAVA, which will
	 * contain "MIN" or "MAX".)
	 * (The SDTS subfield typically looks like:
	 *  "1367")
	 */
	int32_t min_elev;
	int32_t max_elev;
	/*
	 * Angle between primary axis of ground planimetric reference and primary
	 * axis of DEM local reference system.
	 * Should always be zero for arc-second-structured DEMs.  Is probably zero
	 * for UTM-structured DEMs, but could be 90 degrees if some non-USGS someone
	 * has decided to change over to storing the profiles as rows rather than columns.
	 * The USGS apparently always sets this to zero.  In SDTS it is not encoded.
	 */
	double angle;	// bytes 787-810
	/*
	 * Accuracy code for elevations.
	 * 0=unknown
	 * 1=accuracy information is given in logical record type C.
	 *
	 * (SDTS location:  DQPA/DQPA/COMT, record 2)
	 * (The SDTS subfield typically looks like:
	 *  "ACCURACY: RMSE of the DEM data relative to the file's datum (x,y,z) is (0, 0, 3); accuracy has been calculated based on a sample size of 20."
	 *  or
	 *  "ACCURACY: RMSE of the file's datum relative to the absolute datum (x,y,z) is (0, 0, 0); accuracy is estimated, not computed.  RMSE of the DEM data relative to the file's datum (x,y,z) is (0, 0, 1); accuracy has been calculated based on a sample size of 30."
	 *  or
	 *  "ACCURACY: Unspecified."
	 * Actually, this field is pretty variable in appearance, and hard to pry data out of reliably.)
	 */
	int32_t accuracy;	// bytes 811-816
	/*
	 * Three-element array of spatial-resolution units.  These are only permitted to
	 * be integer units, but are stored as floating point.  For a 1-degree DEM, they
	 * are typically 3,3,1.  For non-Alaska 7.5min dem, they are typically 30,30,1 or 10,10,1.
	 * Bytes 817-852.
	 *
	 * (SDTS location:  IREF/IREF/XHRS, IREF/IREF/YHRS, DDSH/DDSH/PREC)
	 * (The SDTS subfield typically looks like:
	 *  "30.000000")
	 */
	double x_res;
	double y_res;
	double z_res;
	/*
	 * Number of rows and columns (m,n) of profiles in the DEM.
	 * The standard isn't really clear on what happens if m != 1.
	 * However, it says that, when m == 1, then n is equal to the
	 * number of rows in DEM file.  In my experience, m has always
	 * been equal to 1 in the main DEM header.  The true column values
	 * appear at the beginning of each profile.
	 * Bytes 853-864.
	 *
	 * (SDTS location:  LDEF/LDEF/NROW, LDEF/LDEF/NCOL.  Also in RSDF/RSDF/RWXT, RSDF/RSDF/CLXT)
	 * (The SDTS subfield typically looks like:
	 *  "325")
	 */
	int32_t cols;
	int32_t rows;
	/*
	 * This concludes the info in the old format.
	 * The new format tacks on some extra fields.
	 * The new fields follow.
	 */
	/*
	 * Largest primary contour interval.
	 * (Present only if two or more primary intervals exist (level 2 DEMs only).)
	 * Bytes 865-869.
	 *
	 * This is apparently only rarely present.  Haven't found an example yet.
	 */
//	int32_t largest_contour;
	/*
	 * Source contour interval units.
	 * Corresponds to the units of the map largest primary contour interval. (level 2 DEMs only.)
	 * Byte 870.
	 *  0=NA
	 *  1=feet
	 *  2=meters
	 *
	 * This is apparently only rarely present.  Haven't found an example yet.
	 */
//	int32_t largest_contour_units;
	/*
	 * Smallest primary contour interval.
	 * Smallest or only primary contour interval (level 2 DEMs only).
	 * Bytes 871-875.
	 *
	 * (SDTS location:  DQPA/DQPA/COMT, record 3.
	 * (The SDTS subfield typically looks like:
	 *  "CONTOUR INTERVAL:  Unspecified."
	 *  or
	 *  "CONTOUR INTERVAL:Primary contour interval of source is 20 feet.")
	 */
//	int32_t smallest_contour;
	/*
	 * Source contour interval units.
	 * Corresponds to the units of the map smallest primary contour interval. (level 2 DEMs only).
	 * Byte 876.
	 *  1=feet
	 *  2=meters
	 *
	 * (In SDTS, the value is part of the text string for the contour itself.  See entry for smallest_contour.)
	 */
//	int32_t smallest_contour_units;
	/*
	 * Data source data.
	 * "YYYY" 4 character year, e.g. 1975, 1997, 2001, etc.
	 * Synonymous with the original compilation date and/or the
	 * date of the photography.
	 * Bytes 877-880
	 *
	 * (SDTS location:  IDEN/IDEN/MPDT for map date.)
	 * (The SDTS subfield typically looks like:
	 *  "1999" or "19971121" and may not be present at all)
	 *
	 * (Also appears in DQHL/DQHL/COMT, record 5, in the
	 * form: "SOURCE DATE OF PUBLISHED MAP OR PHOTOGRAPHY:  Unspecified"
	 *   or: "SOURCE DATE OF PUBLISHED MAP OR PHOTOGRAPHY:  1981.")
	 *
	 * Apparently you are supposed to choose the latest of the available dates.
	 */
//	int32_t source_date;
	/*
	 * Data inspection and revision date.
	 * "YYYY" 4 character year.
	 * Synonymous with the date of completion and/or the date of revision.
	 * Bytes 881-884.
	 * (SDTS location:  DQHL/DQHL/COMT, record 6, for inspection/revision date.)
	 * (The SDTS subfield typically looks like:
	 *  "INSPECTION FLAG: I, DATE THAT DEM WAS INSPECTED ON A DEM EDIT SYSTEM:  1999. RMSE computed from test points. Water body edits done. Visual inspection on DEM edit system and errors edited."
	 *  and I guess you are supposed to pry the data out of this string.  You are supposed to find the latest
	 *  available date, so there may be multiple records, or (more likely) there may sometimes be multiple
	 *  dates embedded within this record.)
	 */
//	int32_t revision_date;
	/*
	 * Inspection flag.
	 * "I" indicates all processes of part3, Quality Control have been performed.
	 * "R" indicates existing DEM has been revised and re-archived.
	 * Byte 885.
	 *
	 * (SDTS location:  DQHL/DQHL/COMT, record 6)
	 * (The SDTS subfield typically looks like:
	 *  "INSPECTION FLAG: I, DATE THAT DEM WAS INSPECTED ON A DEM EDIT SYSTEM:  1999. RMSE computed from test points. Water body edits done. Visual inspection on DEM edit system and errors edited.")
	 */
//	int32_t inspection_flag;
	/*
	 * Data validation flag.
	 *  0 = No validation performed.
	 *  1 = RMSE computed from test points, no quantitative test, no interactive DEM editing or review.
	 *  2 = Batch process water body edit and RMSE computation.
	 *  3 = Review and edit, including water edit.  No RMSE computed from test points.
	 *  4 = Level 1 DEM's reviewed and edited.  Includes water body editing.  RMSE computed from test points.
	 *  5 = Level 2 and 3 DEM's reviewed and edited.  Includes water body editing and verification or vertical integration of planimetric categories (other than hypsography or hydrography if authorized).  RMSE computed from test points.
	 * Byte 886.
	 *
	 * (SDTS location:  DQHL/DQHL/COMT, record 7)
	 * (The SDTS subfield typically looks like:
	 *  "DATA VALIDATION FLAG: 5; Level 2 and 3 DEM's reviewed and edited.  Includes water body editing and verification or vertical integration of planimetric categories (other than hypsography or hydrography if authorized).  RMSE computed from test points."
	 */
//	int32_t validation_flag;
	/*
	 * Suspect and void area flag.
	 *  0 = none
	 *  1 = suspect areas
	 *  2 = void areas
	 *  3 = suspect and void areas.
	 * Byte 887-888.
	 *
	 * (SDTS location:  DQPA/DQPA/COMT, record 4, for suspect areas.)
	 * (The SDTS subfield typically looks like:
	 *  "SUSPECT AREAS:No suspect areas."
	 *  or
	 *  "SUSPECT AREAS:Suspect areas exist in the data.")
	 *
	 * (SDTS location:  DQCG/DQCG/COMT, record 1, for void areas.)
	 * (The SDTS subfield typically looks like:
	 *  "VOID AREAS:No void areas."
	 *  or
	 *  "VOID AREAS:Void areas exist in the data.")
	 */
//	int32_t suspect_and_void_flag;
	/*
	 * Vertical datum.
	 *  1 = local mean sea level
	 *  2 = National Geodetic Vertical Datum 1929 (NGVD 29)
	 *  3 = North American Vertical Datum 1988 (NAVD 88).
	 * Byte 889-890.
	 *
	 * (SDTS location:  XREF/VATT/VDAT)
	 * (The SDTS subfield typically looks like:
	 *  "NGVD")
	 * (Valid SDTS values are "LMSL", "NGVD", and "NAVD")
	 */
	int32_t vertical_datum;
	/*
	 * Horizontal datum.
	 *  1 = North American Datum 1927 (NAD 27)
	 *  2 = World Geodetic System 1972 (WGS 72)
	 *  3 = WGS 84
	 *  4 = NAD 83
	 *  5 = Old Hawaii Datum
	 *  6 = Puerto Rico Datum
	 * Bytes 891-892.
	 *
	 * (SDTS location:  XREF/XREF/HDAT)
	 * (The SDTS subfield typically looks like:
	 *  "NAS")
	 * (Valid SDTS values are "NAS", "WGC", "WGE", "NAX", "OHD", and "PRD",
	 * corresponding directly to the 6 numbers in the "classic" DEM format.)
	 */
	int32_t horizontal_datum;
	/*
	 * Data Edition.  01-99.  Primarily A DMA specific field.  (For USGS use, set to 01).
	 * Bytes 893-896.
	 *
	 * (SDTS location:  DQHL/DQHL/COMT, record ?)
	 * Not certain where or how this appears, when it is present.  For the USGS, it is
	 * always 01 and is not encoded in SDTS.
	 */
//	int32_t data_edition;
	/*
	 * Percent Void.
	 * If the Suspect and Void Area Flag indicates a void, this field (right justified)
	 * contains the percentage of nodes in the file set to void (-32,767).
	 * Bytes 897-900.
	 *
	 * (SDTS location:  DQCG/DQCG/COMT, record ?)
	 * Not certain where or how this appears, when it is present.  Probably tacked onto
	 * the end of the COMT in record 1, in the form: "##% of nodes in the data are set to void.")
	 */
//	int32_t percent_void;
	/*
	 * Edge Match Flag.
	 * Edge match status flag.  Ordered West, North, East, and South.  Described in
	 * the DEM standards document.
	 * Bytes 901-908.
	 *
	 * (SDTS location:  DQLC/DQLC/COMT, record 1)
	 * (The SDTS subfield typically looks like:
	 *  "EDGE MATCH STATUS: West  (1), North (1), East (1), South (1) Edge matching is a process of matching elevation values along common quadrangle edges. The objective of edge matching is to improve the alignment of ridges and drains, and overall topographic shaping and representation. Code of 0 = not edge matched; 1 = edge match checked and joined; 2 = not edge matched because adjoining DEM is on a different horizontal or vertical datum; 3 = not edge matched because the adjoining DEM is not part of the current project; 4 = not edge matched because the adjoining DEM has a different vertical unit.")
	 */
//	int32_t edge_match_flag;
	/*
	 * Vertical Datum Shift.
	 * Value is in the form of SFFF.DD
	 * Value is the average shift value for the four quadrangle corners obtained from
	 * program VERTCON.  Always add this value to convert to NAVD88.
	 * Bytes 909-915.
	 *
	 * (SDTS location:  XREF/XREF/COMT)
	 * (The SDTS subfield typically looks like:
	 *  "Vertical datum shift= 1.220000; always add to convert from National Geodetic Vertical Datum 1929 to North American Vertical Datum 1988.")
	 */
	double vertical_datum_shift;
	/*
	 * These two elements are not present in the DEM Type A profile.  They have been
	 * added to accommodate SDTS files.  (SDTS files differ from the old DEM format
	 * in that the profiles run in rows from west to east, and each row is padded
	 * out to full length, so that a single (x_gp, y_gp) pair is sufficient to
	 * fix every elevation sample in space.)
	 */
	double x_gp_first;
	double y_gp_first;
	/*
	 * These two elements are not present in the DEM Type A profile.  They have been
	 * added to accommodate SDTS files.  (SDTS files differ from the old DEM format
	 * in that the profiles run in rows from west to east, and each row is padded
	 * out to full length.  These two values are the filler that are used to fill
	 * unknown voids within the valid data, and to pad the edges of the data to
	 * make the grid rectangular.)
	 */
	int32_t void_fill;
	int32_t edge_fill;
};



/*
 * This is the structure of the data in a DEM Logical Record Type B.
 * It generally follows the first record in a DEM (the Type A record),
 * and contains an actual elevation profile.
 * It is nominally 1024 bytes long, with blank-padding at the end;
 * but multiple 1024-byte chunks are generally required to contain an
 * entire elevation profile.
 *
 * We don't currently use this structure, but it is included here
 * so that the contents of the record can be documented.
 */
struct dem_record_type_b  {
	/*
	 * A two-element array containing the row and column identification number of the DEM profile
	 * contained in this record.  The row and column numbers may range from 1 to m and 1 to n.
	 * The row number is normally set to 1.  The column identification is the profile sequence number.
	 * Bytes 1-12.
	 *
	 * (SDTS location:  CEL0/CELL/ROWI, CEL0/CELL/COLI)
	 * (The SDTS subfield typically looks like:
	 *  "2")
	 */
	int32_t row_number;
	int32_t column_number;
	/*
	 * A two-element array containing the number (m, n) of elevations in the DEM profile.
	 * The first element in the field corresponds to the number of rows of nodes in this
	 * profile.  The second element is set to 1, specifiying 1 column per B record.
	 * Bytes 13-24.
	 *
	 * Not encoded in SDTS because all rows/columns are padded to the same size.
	 */
	int32_t rows;
	int32_t columns;
	/*
	 * A two-element array containing the ground planimetric coordinates (x_gp, y_gp) of the first elevation
	 * in the profile.
	 * Bytes 25-72.
	 *
	 * Not encoded in SDTS, on a row-by-row basis, because all rows/columns are padded to the same size.
	 * The values for the very first sample, at the northwest corner, can be obtained from
	 * RSDF/SADR/X, RSDF/SADR/Y.  The locations of all other samples in the rectangular array
	 * can be derived from this one location by using the x and y resolution values.
	 */
	double x_gp;
	double y_gp;
	/*
	 * Elevation of local datum for the profile.
	 * The values are in units of measure given by the elev_units element in the Type A record.
	 * Bytes 73-96.
	 *
	 * Not encoded in SDTS.
	 */
	double datum_elev;
	/*
	 * A two-element array of minimum and maximum elevations for the profile.
	 * The values are in the units of measure given by the elev_units element in the Type A record.
	 * Bytes 97-144.
	 *
	 * Not encoded in SDTS.
	 */
	double min_elev;
	double max_elev;
	/*
	 * An (m, n) array of elevations for the profile.  Elevations are expressed in units of resolution.
	 * A maximum of six characters for each integer elevation value.  A value in this array would be
	 * multiplied by the "z" spatial resolution (data element z_res in record Type A) and added to the
	 * datum_elev value for this profile to obtain the elevation for the point.
	 * Bytes 6x(146 or 170), 146 = max for first block, 170 = max for subsequent blocks.
	 *
	 * (SDTS location:  CEL0/CVLS/ELEVATION
	 * (The SDTS subfield typically looks like:
	 *  a 16-bit binary value, which must be converted to binary and properly swabbed.)
	 */
//	int32_t *elevations;
};



/*
 * This is the structure of the data in a DEM Logical Record Type C.
 * The standards document doesn't clearly state (as near as I can tell)
 * where the Type C record would appear in a file, although it does
 * mention it in the context of a file "header".  However, inspection
 * of a sample file shows that it is at the end of the file.
 * It should be 1024 bytes long, since all DEM records are supposed
 * to be blank-padded to 1024 bytes.
 *
 * We don't currently use this structure, but it is included here
 * so that the contents of the record can be documented.
 *
 * In SDTS all of the useful values are encoded within long text
 * strings in DQPA/DQPA/COMT.  See the accuracy description in
 * connection with the type A record, above, for typical text strings.
 */
struct dem_record_type_c  {
	/*
	 * Code indicating availability of statistics in datum_rmse.
	 * 1 = available, 0 = unavailable.
	 * Bytes 1-6.
	 *
	 * Not encoded in SDTS.
	 */
	int32_t	datum_stats_flag;
	/*
	 * RMSE of file's datum relative to absolute datum (x, y, z).
	 * RMSE integer values are in the same unit of measure given by
	 * elements plane_units and elev_units of record Type A.
	 * Bytes 7-24.
	 */
	int32_t datum_rmse_x;
	int32_t datum_rmse_y;
	int32_t datum_rmse_z;
	/*
	 * Sample size on which statistics in datum_rmse are based.
	 * If 0, then accuracy will be assumed to be estimated rather than computed.
	 * Bytes 25-30.
	 */
	int32_t datum_sample_size;
	/*
	 * Code indicating availability of statistics in dem_data_rmse.
	 * 1 = available, 0 = unavailable.
	 * Bytes 31-36.
	 */
	int32_t dem_stats_flag;
	/*
	 * RMSE of DEM data relative to file's datum (x, y, z).
	 * RMSE integer values are in the same units of measure given by
	 * elements plane_units and elev_units of record Type A.
	 * Bytes 37-54.
	 */
	int32_t dem_rmse_x;
	int32_t dem_rmse_y;
	int32_t dem_rmse_z;
	/*
	 * Sample size on which statistics in dem_rmse are based.
	 * If 0, then accuracy will be assumed to be estimated rather than computed.
	 * Bytes 55-60.
	 */
	int32_t dem_sample_size;
};


/*
 * This structure is for passing information about a block of DEM data
 * between routines.  It defines two opposite corners of the data
 * block, in terms of latitude and longitude.  It also defines the
 * x-by-y size of the block, in terms of number of data points.
 */
struct dem_corners  {
	short *ptr;		// A pointer to the block of memory containing the data

	double sw_x_gp;		// UTM x for sw corner	(the _gp stands for ground planimetric coordinates)
	double sw_y_gp;		// UTM y for sw corner
	double nw_x_gp;		// UTM x for nw corner
	double nw_y_gp;		// UTM y for nw corner
	double ne_x_gp;		// UTM x for ne corner
	double ne_y_gp;		// UTM y for ne corner
	double se_x_gp;		// UTM x for se corner
	double se_y_gp;		// UTM y for se corner

	double sw_lat;		// latitude of sw corner
	double sw_long;		// longitude of sw corner
	double nw_lat;		// latitude of nw corner
	double nw_long;		// longitude of nw corner
	double ne_lat;		// latitude of ne corner
	double ne_long;		// longitude of ne corner
	double se_lat;		// latitude of se corner
	double se_long;		// longitude of se corner

	double x_gp_min;	// smallest UTM x
	double y_gp_min;	// smallest UTM y
	double x_gp_max;	// largest UTM x
	double y_gp_max;	// largest UTM y

	int32_t x;		// number of samples in a row
	int32_t y;		// number of samples in a column
};


extern void parse_dem_a(char *, struct dem_record_type_a *, struct datum *);
extern int parse_dem_sdts(char *, struct dem_record_type_a *, struct dem_record_type_c *, struct datum *, int32_t);
extern void print_dem_a(struct dem_record_type_a *);
extern void print_dem_a_c(struct dem_record_type_a *, struct dem_record_type_c *);
extern int process_geo_dem(int, ssize_t (*)(), struct image_corners *, struct dem_corners *, struct dem_record_type_a *, struct datum *datum);
extern int process_utm_dem(int, ssize_t (*)(), struct image_corners *, struct dem_corners *, struct dem_record_type_a *, struct datum *datum);
extern int process_dem_sdts(char *, struct image_corners *, struct dem_corners *, struct dem_record_type_a *, struct datum *datum);
extern int process_gtopo30(char *, struct image_corners *, struct dem_corners *, struct dem_record_type_a *, struct datum *datum, int32_t);
