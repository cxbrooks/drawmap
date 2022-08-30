/*
 * =========================================================================
 * sdts_utils.c - Utility routines for SDTS files.
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
#include <string.h>
#include "drawmap.h"
#include "sdts_utils.h"


/*
 * The routines in this file are used to open, read, and close
 * files in the Spatial Data Transfer System (SDTS) format.
 * Specifically, they read files in the ISO 8211 encoding format.
 * The truly SDTS-specific stuff is handled at a higher layer.
 *
 *
 * There is a lot of code in this file, but only three
 * functions that are normally used as entry points:
 *
 *	int32_t begin_ddf(char *file_name)
 *
 *	int32_t get_subfield(struct subfield *subfield)
 *
 *	void end_ddf()
 *
 * Call begin_ddf(), with the name of an SDTS file as argument,
 * to begin parsing an ISO 8211 file.  Call end_ddf() when you are
 * done with that file.  YOU CAN ONLY HAVE ONE FILE OPEN AT ONCE.
 *
 * get_subfield() returns the subfields of the file, one at a time.
 * If you call it, and it returns 1, then you have retrieved a
 * subfield.  If it returns 0, you have reached end of file.
 * If there is an error, the function calls exit().
 *
 * If you want to try parsing some sample DDF files, there is a
 * commented-out test program at the end of this file which
 * will try to read and print out the contents of a DDF file.
 * It illustrates the use of the three functions.
 *
 *
 * Note that these routines are nowhere near a complete
 * implementation of the ISO 8211 standard.  Large chunks are
 * missing, since the purpose of these routines is only to parse
 * USGS-formatted SDTS files.  Furthermore, the routines may
 * contain errors due to me misunderstanding the standard.  (I
 * don't have a copy of the standard, and have gleaned the information
 * contained here via various tutorials on the Internet.)  Also,
 * some sections of code remain untested because I don't have any
 * sample files to test them against.  (The code to read records
 * longer than 100000 bytes is a case in point.)  Thus,
 * you might be ill-advised to try to use these routines to parse
 * randomly-selected ISO 8211 files.  There are library packages
 * available to do that sort of thing.  fips123 and sdts++ are
 * two possible starting points, but I don't know how complete
 * their ISO 8211 support is.  Furthermore, there are at least
 * two versions of ISO 8211, a 1985 version, and a newer 1994
 * version.  We attempt to support only the older version here.
 */


/*
 * We begin with a general description of SDTS, in an attempt to
 * demystify things a bit.  Be warned that my understanding of SDTS
 * has not reached a high-enough level for me to be considered an
 * expert on it.  Furthermore, I have never seen a copy of the ISO 8211
 * standard, which is probably necessary for a full understanding of SDTS.
 * The information here is gleaned from tutorial sources on the Internet,
 * and by prying various SDTS files apart to look at the insides.
 * Thus, any or all of the description that follows may be wrong.  (The
 * information was, however, sufficient to write a program that can obtain
 * useful data from SDTS files.)  In particular, be wary of any
 * terminology or acronyms.  In some cases, I had to guess what the correct
 * terminology might be, since the sources I looked at varied in the names
 * they called things.
 *
 * First of all, SDTS is intended as a standard for data transfer between
 * dissimilar machines.  It is designed to package spatial data into a
 * commonly-readable form so that the data can be passed back and forth
 * between users with different computer systems.  As such, it is conceptually
 * similar to parts of many other protocol suites, like the ISO 7-layer transfer
 * protocols, the TCP/IP suite used on the Internet, morse code, and so on.
 * In fact, I have seen ISO 8211 described as a possible implementation of
 * the Presentation Layer (layer 6) of the ISO protocol.  I guess that would
 * make SDTS an Application Layer (layer 7) protocol.  The 8211 standard is
 * similar in form and function to other transfer encodings, such as Abstract
 * Syntax Notation One (ASN.1), which is used in (among other things) the SNMP
 * network-management suite.
 *
 * An SDTS transfer is composed of a number of files, all with the same
 * basic format.  The files are called modules, and their names all end with the
 * letters ".DDF" which stand for Data Definition File.  (This may not be
 * a true statement for the 1994 version of ISO 8211.)  The files are
 * structured according to the ISO/IEC 8211 standard.  There have been at
 * least two versions of this standard, 1985 and 1994.  I think that SDTS files
 * are restricted to the 1985 version, but can't claim this with certainty.
 *
 * In general, the file names have the form AAAABBBB.DDF, where AAAA is a name
 * unique to the specific transfer, like HY01 for a DLG hydrography transfer.
 * The BBBB part of the name identifies the various modules in the transfer.
 * For example, there is a HY01CATD.DDF file, which is a catalog of all of the
 * modules in the transfer.  A 100K DLG hydrography transfer might contain four
 * separate linear feature data files, named HY01LE01.DDF, HY01LE02.DDF,
 * HY01LE03.DDF, and HY01LE04.DDF.  (I think that, in SDTS jargon, these
 * would be called separate layers or separate topological manifolds, but I am
 * not sure of the terminology.)  Each of these files corresponds to one of the
 * four files in a hydrography directory under the old optional format.  However,
 * unlike the old optional-format files, these files are not self-contained DLG
 * packages.  Some of the information has been split off into other files.
 * For example, in order to find the polygonal areas,
 * we need to examine the polygon files, named HY01PC01.DDF, HY01PC02.DDF,
 * HY01PC03.DDF, and HY01PC04.DDF.  These files contain record IDs that
 * cross-reference attributes in the attribute file HY01AHYF.DDF.
 *
 * This brief description may not make much sense unless you are familiar with
 * the contents of the old optional-format DLG files, so let's look into them a bit.
 * (This is a simplified description.  Not horribly simplified, but simplified.
 * Documents are available from the USGS web sites that describe the DLG format.)
 * Each optional-format DLG file has a large header, which contains things like
 * the latitude/longitude of the four corners of the data, and other global
 * information.  Most, or all, of this has been moved into the SDTS HY01AHDR.DDF
 * module.  Following the header are a list of Node records, which define all
 * of the nodal points in the data.  (The points may define locations where line
 * segments meet, or they may be points were small point-sized features are
 * located, like a tiny pond, and so on.)  Each node is defined by a pair of
 * UTM coordinates, and contains cross-references to the lines that
 * intersect the node.  Following the nodes are the areas.
 * Each area includes a pair of UTM coordinates that define a representative
 * point for the area.  There are also a set of cross references to the lines
 * that bound the area.  Following this may be one or more attribute codes
 * that describe the type of area (e.g. a lake, or a marsh).  Following the areas
 * are the linear features.  These contain sets of UTM coordinates that
 * define a sequence of line segments that form (say) a river or road,
 * or that form part or all of the boundary of (say) a lake.
 * The linear features may also have attributes associated with them.
 *
 * In SDTS, all of this stuff gets swept into separate files.  The linear features
 * are defined in one file, which also contains cross-references to attributes in another
 * file, cross-references to polygons in yet another file, and cross-references to
 * nodes in yet another file still.  It isn't so hard to understand how everything
 * fits together, but it can be painful to chase down all of the data you need
 * in the various files.
 *
 * Within each file, all of this information is encoded according to the 8211
 * standard.  This standard is concerned with how to represent different types of data,
 * and agglomerate them into larger structures.  Files (modules) are composed of
 * records, records are composed of fields, and fields are composed of subfields.
 * (This can be somewhat confusing, since common usage would lean more toward calling
 * the smallest subdivision a field, and would then construct composite records
 * from these fields.  C'est la vie.)
 *
 * The first record in a module is the Data Definition Record (or Data Descriptive
 * Record, depending on who you talk to), and is called the DDR for short.  It
 * contains some general information, and also a definition of the types of data
 * contained in the various fields.
 *
 * This is followed by one or more Data Records (DRs).  These contain actual
 * fields and subfield data.
 *
 * Whether it is a DDR or a DR, each record consists of a Leader, a Directory, and a
 * Field Area.  The record starts with a leader, which contains some general info about
 * the record, such as its length.  This is followed by a Directory, which contains
 * (for each field) a tag, a data length, and an offset into the Field Area at which
 * additional information appears.  For the DDR, the Field Area contains information
 * that describes a particular field and the data it will contain.  For the DR,
 * the Field Area contains the actual data for the field (which may include
 * multiple chunks of data if the field is composed of several subfields).
 *
 * The 8211 standard defines how data is encoded for storage or transmission.
 * However, at a higher level we need to define what that data is.  That is,
 * we need to define all of the various tags, data types, data lengths, and
 * so on, that constitute a DEM transfer, or a DLG transfer, or whatever.
 * This is where SDTS provides "value-added".  It defines various kinds of
 * modules that collectively allow the transfer of spatial data.  This is
 * the modulization that we discussed above, in connection with a hypothetical
 * hydrography transfer.  However, even this is not yet enough.  We still need
 * more detail about the various subfields.  For example, in DLG files from
 * the USGS, in SDTS format, the horizontal datum is stored in the XREF
 * module, in the XREF field, in the HDAT subfield, as a three-character
 * ASCII string.  If the string is "NAS", then the datum is NAD-27.  If
 * the string is "NAX", then the datum is NAD-83.  Etcetera.
 * As I understand it, this kind of information is not part of the SDTS
 * standard, per se.  It is defined by the end users of the standard,
 * in this case the USGS.  Thus, we need to understand SDTS at three
 * levels:  (1) the low-level data encoding of ISO 8211, defined in the
 * ISO 8211 standards document, (2) the higher level of abstraction of SDTS
 * modules, defined in the SDTS standards document, and (3) the particular
 * data encoding for a particular application, such as a DEM or DLG file.
 * Each of these things is documented separately and, to a large extent,
 * independently.
 *
 * Part of item (3), above, is the definition of SDTS "profiles".  These
 * are specific implementations of SDTS, for specific purposes, that
 * restrict SDTS to specific subsets of the possible options.  As far as
 * I know, there are two such profiles:  the Topological Vector Profile (TVP)
 * for vector information (such as DLG data); and the Raster Profile for information
 * that naturally falls on a grid (such as DEM data).
 *
 * SDTS isn't conceptually difficult, but the details needed for a full
 * understanding are scattered hither and yon.
 */




#define FIELD_TERMINATOR	0x1e
#define UNIT_TERMINATOR		0x1f
#define REC_LEN_LEN		5
#define REC_LEADER_LEN		24
#define MAX_TAGS		10	// Maximum number of field tags *we* allow in a record (not an ISO 8211 limit)
#define MAX_SUBFIELDS		100	// Maximum number of subfield labels *we* allow in a user record (not an ISO 8211 limit)



/*
 * Some global state variables used by many of the subroutines.
 *
 * Note that the fact that these are global means that we can only
 * have one DDF file open at once.
 *
 * If it becomes necessary to have more than one file open at once,
 * we could put these variables into an array of structures, indexed
 * by the file descriptor.
 */
static int32_t leaderless_flag;		// When non-zero, we have encountered a record leader with a Leader ID of 'R'
static char *ddr_buf = (char *)0;	// DDR record buffer.
static char *dr_buf = (char *)0;	// DR record buffer.
static int32_t gz_flag;	// If non-zero, we are reading a gzip-compressed file.
static ssize_t (*read_function)(int, void *, size_t);
static int fdesc;	// File descriptor of the open DDF file.
static int32_t dr_tag;	// Next-available field in the DR.
static int32_t dr_label;	// Next-available subfield in the field.


/*
 * SDTS files are organized into records, fields, and subfields.
 * The basic structure of each record, whether it is the DDR, or one of the
 * DRs, is the same.  (The low-level details differ.)
 *
 * We begin by describing the DDR.
 *
 * The record begins with a 24-byte Leader, containing general information
 * about the record.
 */
struct record_leader  {
	int32_t length;		// Record Length.  Integer stored as five ASCII bytes.
	int32_t ichg_level;	// Interchange Level.  Integer stored as one ASCII byte.
				// '1', '2', or '3' for DDR. ' ' for DR.
				//
	char leader_id;		// Leader Identifier.  A single ASCII byte.  Must be 'L' for DDR, 'D' or 'R' for DR.
	char ice_ind;		// Inline Code Extension.  A single ASCII byte.
	char reserved_space;	// Reserved Space.  A single ASCII byte.
	char application;	// Application Indicator.  A single ASCII byte.
	int32_t field_cntrl_len;// Field Control Indicator.  Integer stored as two ASCII bytes.
				// (In the DDR, must be 0 if Interchange Level is 1, must be 6 for
				// Interchange Level 2 or 3.  In DR, it appears to always be "  ".)
				//
	int32_t fa_addr;	// Base address of Field Area.  Integer stored as 5 ASCII bytes.
				// (Addresses start at zero.)
				//
	char ccs[3];		// Code Character Set Indicator.  Three ASCII bytes.
	int32_t field_len_len;	// Size of Field Length.  Integer stored as one ASCII byte.  (1 <= length <=9)
	int32_t field_pos_len;	// Size of Field Position.  Integer stored as one ASCII byte.  (1 <= length <=9)
	int32_t reserved_digit;	// Reserved Digit.  Integer stored as one ASCII byte.
				// (As near as I can tell, this is simply reserved for future use.)
				//
	int32_t field_tag_len;	// Size of Field Tag.  Integer stored as one ASCII byte.  (1 <= length <=7)
				// Subfield labels are not restricted by this length.
};
/*
 * Following this is the DDR Directory,
 * whose length is equal to the Base Address of the Field Area
 * minus the Leader length minus 1.  The minus 1 is for the field terminator,
 * which immediately follows the Directory.
 *
 * The Directory consists of consecutive instances of the triple
 * (tag, field length, field position), one triple for each tag defined.
 *
 * Following the Directory, there is a Field Area, containing information
 * about each field.  This area contains the Field Control string,
 * Field Name, subfield labels (similar to tags), and subfield formats.
 * Although not technically part of the directory, these items are also
 * included in the ddr_directory structure so that everything is in one place.
 *
 * Not all of the extra items will be present for each field.
 * Rather than do a bunch of malloc/free operations to
 * allocate appropriate space, we just provide arrays of pre-defined
 * size, which are hopefully big enough for all files we will encounter.
 * This is somewhat wasteful, and subtracts from the generality of the
 * code, but also makes it easier to write and debug the code.
 * Later, after the code is more seasoned, we can change it to be more
 * efficient (and more general) if we wish.
 *
 * We don't provide storage space for subfield labels and formats.
 * Instead, we will just provide pointers into the storage buffer
 * for the entire DDR.
 */
struct ddr_directory  {
	/*
	 * Tag "0000" indicates a file-control entry.
	 *    If Interchange Level is 2 or 3, we need to read 6-byte field control.
	 * Tag "0001" indicates the DDF Record Identifier.
	 *    If Interchange Level is 2 or 3, we need to read 6-byte field control.
	 * Tag "0002" indicates user-augmented file description.
	 *    No field control, as far as I know.
	 * Tag "????" indicates user data.
	 *    If Interchange Level is 2 or 3, we need to read 6-byte field control.
	 */
	char *tag;		// Tag.  ASCII bytes.  Its length is specified by the
				// Size of Field Tag in the record_leader.  I think these are
				// restricted to 7 bytes, and we include an extra byte for a null.
	int32_t field_len;	// Field Length.  Integer stored as ASCII bytes.
				// Its length is specified by the Size of Field Length in the record_leader.
	int32_t field_pos;	// Field Position.  Integer stored as ASCII bytes.  Its length is
				// specified by the Size of Field Position in the record_leader.
	char *field_cntrl;	// Field Control.  ASCII bytes.  Its length is specified by the Field
				// Control Indicator in the record_leader.
	char *name;		// The human-friendly name of the field.
	char *labels[MAX_SUBFIELDS];	// Labels (similar to tags) for subfields.
	char *formats[MAX_SUBFIELDS];	// Formats for subfields.
	int  sizes[MAX_SUBFIELDS];	// The sizes from the formats, if they were provided.  (0 if they were not.)
	char cartesian[MAX_SUBFIELDS];	// Cartesian delimiter flag.  If non-zero, the label had a '*' in front of it.
	int32_t num_labels;	// The number of user subfield labels in the record.
	int32_t num_formats;	// The number of user subfield formats in the record.
};


/*
 * This is a structure for the DDR contents.
 * It includes space for the Leader and Directory, but the
 * contents of the Field Area reside in the DDR buffer, ddr_buf.
 */
static struct ddr  {
	struct record_leader record_leader;
	struct ddr_directory f0000;	// DDR Directory entry for file-control entry
//	struct ddr_directory f0002;	// DDR Directory entry for user-augmented file description (currently unsupported)
	struct ddr_directory user[MAX_TAGS]; // DDR Directory entry for user data entry
	int32_t num_tags;		// Total number of field tags stored in user[]
} ddr;



/*
 * The Data Records also contain directories, but the Field Area
 * contains actual data, rather than field description information.
 * Thus we define a directory structure for the DRs that doesn't
 * contain the extra field description information.
 */
struct dr_directory  {
	char *tag;		// Tag.  ASCII bytes.  Its length is specified by the
				// Size of Field Tag in the record_leader.  I think these are
				// restricted to 7 bytes, and we include an extra byte for a null.
	int32_t field_len;	// Field Length.  Integer stored as ASCII bytes.
				// Its length is specified by the Size of Field Length in the record_leader.
	int32_t field_pos;	// Field Position.  Integer stored as ASCII bytes.  Its length is
				// specified by the Size of Field Position in the record_leader.
};


/*
 * This is a structure for the DR contents.
 * It includes space for the Leader and Directory, but the
 * contents of the Field Area reside in the DR buffer, dr_buf.
 */
static struct dr  {
	struct record_leader record_leader;
	struct dr_directory user[MAX_TAGS]; // DR Directory entries
	int32_t num_tags;		// Total number of field tags stored in user[]
} dr;




/*
 * Read in an ISO 8211 record.
 * Fill in the record leader structure.
 */
static int32_t
read_record(struct record_leader *record_leader, char **rec_buf)
{
	int32_t i;
	char tmp[REC_LEN_LEN + 1];
	char *ptr;
	ssize_t ret_val;
	char save_byte;
	int32_t long_record_flag = 0;
	int32_t field_pos, field_len;

	/*
	 * Read in the record length, which is the first thing in the
	 * record.
	 */
	if ((ret_val = read_function(fdesc, tmp, REC_LEN_LEN)) != REC_LEN_LEN)  {
		if (ret_val == 0)  {
			return 0;
		}
		else  {
			fprintf(stderr, "Couldn't read record size from SDTS record.\n");
			return -1;
		}
	}
	tmp[REC_LEN_LEN] = '\0';
	record_leader->length = strtol(tmp, (char **)0, 10);
	if (record_leader->length == 0)  {
		/*
		 * Note: If the record length is zero, then that means the
		 * record is over 100,000 bytes.  The procedure to deal with
		 * this is to read and parse the Directory and find
		 * the field length and field position of the last Directory
		 * entry (the one just before the field terminator).
		 * Adding these to the base address of the Field Area
		 * yields the record length.  We will do this after
		 * we parse the leader.  For now, we just set the record
		 * length to be long enough for the Leader.
		 *
		 * However, before we give do this, make sure that
		 * the record length is actually zero.  There may be
		 * non-numeric characters where the record length is supposed
		 * to be.  strtol() will ignore these and return zero, which
		 * isn't strictly-speaking the correct interpretation of
		 * a garbage-filled record length.
		 */
		for (i = 0; i < REC_LEN_LEN; i++) {
			/* Check for leading blanks. */
			if (tmp[i] != ' ') break;
		}
		for ( ; i < REC_LEN_LEN; i++)  {
			/* Check for all zeros, following the blanks. */
			if (tmp[i] != '0') break;
		}
		if (i < REC_LEN_LEN)  {
			fprintf(stderr, "Warning: Record length is nonsensical.  Assuming end of file.\n");
			return 0;
		}

		record_leader->length = REC_LEADER_LEN;
		long_record_flag = 1;
	}
	else if (record_leader->length < REC_LEADER_LEN)  {
		fprintf(stderr, "Record length is less than %d in SDTS record.\n", REC_LEADER_LEN);
		return -1;
	}

	/*
	 * Get space for the record, copy the already-read record length
	 * into the beginning of the space, and read the remainder of the record.
	 * (Or read just the Leader, if this is a long record.)
	 */
	if ((*rec_buf = (char *)malloc(record_leader->length + 1)) == (char *)0)  {
		fprintf(stderr, "malloc(%d) returns null.\n", record_leader->length + 1);
		return -1;
	}
	for (i = 0; i < REC_LEN_LEN; i++)  {
		(*rec_buf)[i] = tmp[i];
	}
	if ((ret_val = read_function(fdesc, *rec_buf + REC_LEN_LEN, record_leader->length - REC_LEN_LEN)) !=
									(record_leader->length - REC_LEN_LEN))  {
		fprintf(stderr, "Couldn't read SDTS record.  Ret_val = %d.\n", (int)ret_val);
		free(*rec_buf);
		return -1;
	}
	(*rec_buf)[record_leader->length] = '\0';	// Not really necessary, but I like to do it.


	/*
	 * Parse the record leader and put the results into the
	 * record_leader structure.
	 */
	if ((*rec_buf)[REC_LEN_LEN] == ' ')  {	// The Interchange Level is a single digit, or blank
		record_leader->ichg_level = -1;
	}
	else  {
		record_leader->ichg_level = (*rec_buf)[REC_LEN_LEN] - '0';
	}
	record_leader->leader_id = (*rec_buf)[REC_LEN_LEN + 1];	// Leader Identifier.  A single ASCII byte.  'L' for DDR, 'D' or 'R' for DR.
	record_leader->ice_ind = (*rec_buf)[REC_LEN_LEN + 2];		// Inline Code Extension.  A single ASCII byte.
	record_leader->reserved_space = (*rec_buf)[REC_LEN_LEN + 3];	// Reserved Space.  A single ASCII byte.
	record_leader->application = (*rec_buf)[REC_LEN_LEN + 4];	// Application Indicator.  A single ASCII byte.
	if ((*rec_buf)[REC_LEN_LEN + 6] == ' ')  {	// Field Control Indicator.  Integer stored as two ASCII bytes.
		record_leader->field_cntrl_len = -1;
	}
	else  {
		record_leader->field_cntrl_len = ((*rec_buf)[REC_LEN_LEN + 5] - '0') * 10 + (*rec_buf)[REC_LEN_LEN + 6] - '0';
	}
	save_byte = (*rec_buf)[REC_LEN_LEN + 12]; (*rec_buf)[REC_LEN_LEN + 12] = '\0';
	record_leader->fa_addr = strtol(*rec_buf + REC_LEN_LEN + 7, (char **)0, 10);	// Base address of Field Area.  Integer stored as 5 ASCII bytes.
	(*rec_buf)[REC_LEN_LEN + 12] = save_byte;
	record_leader->ccs[0] = (*rec_buf)[REC_LEN_LEN + 12];	// Code Character Set Indicator.  Three ASCII bytes.
	record_leader->ccs[1] = (*rec_buf)[REC_LEN_LEN + 13];
	record_leader->ccs[2] = (*rec_buf)[REC_LEN_LEN + 14];
	record_leader->field_len_len = (*rec_buf)[REC_LEN_LEN + 15] - '0';	// Size of Field Length.  Integer stored as one ASCII byte.  (1 <= length <=9)
	if ((record_leader->field_len_len < 1) || (record_leader->field_len_len > 9))  {
		free(*rec_buf);
		fprintf(stderr, "Field length length in record leader (%d) is out of bounds.\n", record_leader->field_len_len);
		return -1;
	}
	record_leader->field_pos_len = (*rec_buf)[REC_LEN_LEN + 16] - '0';	// Size of Field Position.  Integer stored as one ASCII byte.  (1 <= length <=9)
	if ((record_leader->field_pos_len < 1) || (record_leader->field_pos_len > 9))  {
		free(*rec_buf);
		fprintf(stderr, "Field position length in record leader (%d) is out of bounds.\n", record_leader->field_pos_len);
		return -1;
	}
	if ((*rec_buf)[REC_LEN_LEN + 17] != ' ')  {
		record_leader->reserved_digit = (*rec_buf)[REC_LEN_LEN + 17] - '0';	// Reserved Digit.  Integer stored as one ASCII byte.
	}
	else  {
		record_leader->reserved_digit = -1;	// Reserved Digit.  Integer stored as one ASCII byte.
	}
	record_leader->field_tag_len = (*rec_buf)[REC_LEN_LEN + 18] - '0';	// Size of Field Tag.  Integer stored as one ASCII byte.  (1 <= length <=7)
	if ((record_leader->field_tag_len < 1) || (record_leader->field_tag_len > 7))  {
		free(*rec_buf);
		fprintf(stderr, "Field tag length in record leader (%d) is out of bounds.\n", record_leader->field_tag_len);
		return -1;
	}

	if (long_record_flag != 0)  {
		/*
		 * The record is longer than 100000 bytes.  Get the record the hard way.
		 *
		 * Start by re-sizing the record buffer to 100000 bytes, since
		 * we know the record must be at least that long.
		 */
		if ((ptr = (char *)realloc(*rec_buf, 100000)) == (char *)0)  {
			free(*rec_buf);
			fprintf(stderr, "realloc(100000) returns null.\n");
			return -1;
		}
		*rec_buf = ptr;

		/*
		 * Now search after the Leader to find the first FIELD_TERMINATOR.
		 * This should be the end of the Directory.
		 */
		i = REC_LEADER_LEN;
		while (((ret_val = read_function(fdesc, *rec_buf + i, 1)) == 1) && ((*rec_buf)[i] != FIELD_TERMINATOR))  {
			i++;
			if (i == 100000)  {
				fprintf(stderr, "Failed to find end of Directory in first 100000 bytes.  This seems implausible.  Giving up.\n");
				free(*rec_buf);
				return -1;
			}
		}
		if (ret_val != 1)  {
			fprintf(stderr, "Couldn't read SDTS record.  Ret_val = %d.\n", (int)ret_val);
			free(*rec_buf);
			return -1;
		}

		/*
		 * If we make it to this point, then i should be the index of the Directory FIELD_TERMINATOR.
		 * We need to backtrack from here to determine the last Field Length Length
		 * and Field Position Length in the Directory.
		 */
		save_byte = (*rec_buf)[i]; (*rec_buf)[i] = '\0';
		field_pos = strtol(*rec_buf + i - record_leader->field_pos_len, (char **)0, 10);
		(*rec_buf)[i] = save_byte;
		save_byte = (*rec_buf)[i - record_leader->field_pos_len]; (*rec_buf)[i - record_leader->field_pos_len] = '\0';
		field_len = strtol(*rec_buf + i - record_leader->field_pos_len - record_leader->field_len_len, (char **)0, 10);
		(*rec_buf)[i - record_leader->field_pos_len] = save_byte;

		/*
		 * Now we have what we need to figure out the
		 * record length.  Figure it out and then re-size the
		 * buffer accordingly.
		 */
		record_leader->length = record_leader->fa_addr + field_pos + field_len;
		if ((ptr = (char *)realloc(*rec_buf, record_leader->length + 1)) == (char *)0)  {
			free(*rec_buf);
			fprintf(stderr, "realloc(%d) returns null.\n", record_leader->length + 1);
			return -1;
		}
		*rec_buf = ptr;

		/*
		 * Now, at last, we are ready to read in the remainder of the record.
		 * The remainder consists of the Field Area, since we have already read in
		 * the Leader and the Directory.
		 */
		if ((ret_val = read_function(fdesc, *rec_buf + i + 1, record_leader->length - i - 1)) !=
									(record_leader->length - i - 1))  {
			fprintf(stderr, "Couldn't read SDTS record.  Ret_val = %d.\n", (int)ret_val);
			free(*rec_buf);
			return -1;
		}
	}
	(*rec_buf)[record_leader->length] = '\0';	// Not really necessary, but I like to do it.

	return record_leader->length;
}




/*
 * For testing purposes, print an SDTS DDR record structure.
 */
void
print_ddr()
{
	int32_t i;
	int32_t j;

	fprintf(stderr, "ddr.record_leader.length = %d\n", ddr.record_leader.length);
	fprintf(stderr, "ddr.record_leader.ichg_level = %d\n", ddr.record_leader.ichg_level);
	if (ddr.record_leader.ichg_level >= 0)  {
		if ((ddr.record_leader.ichg_level < 1) || (ddr.record_leader.ichg_level > 3))  {
			fprintf(stderr, "Bad interchange level in DDR = %d.\n", ddr.record_leader.ichg_level);
			exit(0);
		}
	}
	fprintf(stderr, "ddr.record_leader.leader_id = \"%c\"\n", ddr.record_leader.leader_id);
	fprintf(stderr, "ddr.record_leader.ice_ind = \"%c\"\n", ddr.record_leader.ice_ind);
	fprintf(stderr, "ddr.record_leader.reserved_space = \"%c\"\n", ddr.record_leader.reserved_space);
	fprintf(stderr, "ddr.record_leader.application = \"%c\"\n", ddr.record_leader.application);
	fprintf(stderr, "ddr.record_leader.field_cntrl_len = %d\n", ddr.record_leader.field_cntrl_len);
	if (ddr.record_leader.ichg_level >= 0)  {
		if (((ddr.record_leader.ichg_level == 1) && (ddr.record_leader.field_cntrl_len != 0)) ||
		    ((ddr.record_leader.ichg_level == 2) && (ddr.record_leader.field_cntrl_len != 6)) ||
		    ((ddr.record_leader.ichg_level == 3) && (ddr.record_leader.field_cntrl_len != 6)))  {
			fprintf(stderr, "Bad field control length in DDR = %d.\n", ddr.record_leader.field_cntrl_len);
			exit(0);
		}
	}
	fprintf(stderr, "ddr.record_leader.fa_addr = %d\n", ddr.record_leader.fa_addr);
	if (ddr.record_leader.fa_addr < REC_LEADER_LEN)  {
		fprintf(stderr, "Bad DDA address in DDR = %d.\n", ddr.record_leader.fa_addr);
		exit(0);
	}
	fprintf(stderr, "ddr.record_leader.ccs = \"%c%c%c\"\n", ddr.record_leader.ccs[0], ddr.record_leader.ccs[1], ddr.record_leader.ccs[2]);
	fprintf(stderr, "ddr.record_leader.field_len_len = %d\n", ddr.record_leader.field_len_len);
	fprintf(stderr, "ddr.record_leader.field_pos_len = %d\n", ddr.record_leader.field_pos_len);
	fprintf(stderr, "ddr.record_leader.reserved_digit = %d\n", ddr.record_leader.reserved_digit);
	fprintf(stderr, "ddr.record_leader.field_tag_len = %d\n", ddr.record_leader.field_tag_len);
	fprintf(stderr, "\n");

	if (ddr.f0000.tag == (char *)0)  {
		fprintf(stderr, "ddr.f0000 did not appear\n");
	}
	else  {
		fprintf(stderr, "ddr.f0000.tag = \"%.*s\"\n", ddr.record_leader.field_tag_len, ddr.f0000.tag);
		fprintf(stderr, "ddr.f0000.field_len = %d\n", ddr.f0000.field_len);
		fprintf(stderr, "ddr.f0000.field_pos = %d\n", ddr.f0000.field_pos);
		fprintf(stderr, "ddr.f0000.field_cntrl = \"%.*s\"\n", ddr.record_leader.ichg_level > 1 ? 6 : 0, ddr.f0000.field_cntrl);
		fprintf(stderr, "ddr.f0000.name = \"%s\"\n", ddr.f0000.name);
	}
	fprintf(stderr, "\n");

	for (i = 0; i < ddr.num_tags; i++)  {
		fprintf(stderr, "ddr.user[%d].tag = \"%.*s\"\n", i, ddr.record_leader.field_tag_len, ddr.user[i].tag);
		fprintf(stderr, "ddr.user[%d].field_len = %d\n", i, ddr.user[i].field_len);
		fprintf(stderr, "ddr.user[%d].field_pos = %d\n", i, ddr.user[i].field_pos);
		fprintf(stderr, "ddr.user[%d].field_cntrl = \"%.*s\"\n", i, ddr.record_leader.ichg_level > 1 ? 6 : 0, ddr.user[i].field_cntrl);
		fprintf(stderr, "ddr.user[%d].name = \"%s\"\n", i, ddr.user[i].name);

		for (j = 0; j < ddr.user[i].num_labels; j++)  {
			fprintf(stderr, "ddr.user[%d].labels[%d] = \"%s\"\n", i, j, ddr.user[i].labels[j]);
			fprintf(stderr, "ddr.user[%d].formats[%d] = \"%s\"\n", i, j, ddr.user[i].formats[j]);
		}

		fprintf(stderr, "\n");
	}
}





/*
 * Parse the DDR record and put all of the
 * information into the DDR structure.
 *
 * We have already parsed the record leader, in the read_record() function.
 * The values have been stored away in ddr.record_leader.
 * This was done separately because the leader always has the same
 * interpretation for every record, and we can parse and check it
 * right after reading any record.
 *
 * Now we need to parse the rest of the DDR.  The DDR is a collection
 * of fields that describe the fields and subfields in the following
 * Data Records (DRs).
 *
 * We will be concerned with the two remaining (non-leader) portions of the DDR:
 * the Directory and the Field Area.
 *
 * The Directory immediately follows the leader, without any intervening
 * field terminator, and is followed by a field terminator (which I guess
 * makes it the first field in the DDR).  The directory contains consecutive
 * entries for each field description in the DDR.  Each entry consists of a field tag,
 * followed by the field length (which is not the length of the corresponding field in
 * the DR, but rather the length of the field (in the Field Area) that describes how
 * the field will appear in the DR), followed by the field position
 * (which is the offset of the field description from the start of the Field Area).
 * The lengths (in the Directory) of each of these three items are specified in the leader.
 * Thus, we read the leader to find the length of the "field length" in the Directory.
 * We read the Directory to find the actual field length (which is not the length of the
 * corresponding field in the DRs, but rather the length of the field in the Field Area
 * that describes the DR field).
 *
 * Given the information from the Directory, we then parse the corresponding fields in
 * the Field Area.  In general, the format of these appears to be:  a format control
 * field (whose length is specified in the leader), a field name (which is the
 * human-friendly name of the corresponding field ---  and is not the same as
 * the tag of the corresponding field), a list of subfield labels, and a set
 * of subfield format specifiers.  The latter two items only seem to appear for
 * user-defined fields, and not for the special fields with tags "0000",
 * "0001", "0002", and so on.  (I'm not so sure about field types "0002" and above,
 * since I haven't come across any examples yet.)
 *
 * We need to read all of these things, and store them away in appropriate
 * parts of the ddr structure.
 */
static void
parse_ddr()
{
	int32_t i;		// We use this as an index into the Directory
	int32_t j;		// We use this as an index into the Field Area
	int32_t k;
	int32_t size;
	int32_t repeat_count;
	char save_byte;
	char *ptr, *end_ptr;
	ssize_t ret_val;

	if ((ret_val = read_record(&(ddr.record_leader), &ddr_buf)) <= 0)  {
		/* If ret_val is < 0, read_record() already printed an error message. */
		if (ret_val == 0)  {
			fprintf(stderr, "At end of file, reading DDR.  This should not happen.\n");
		}
		exit(0);
	}
	if (ddr.record_leader.leader_id != 'L')  {
		fprintf(stderr, "DDR Leader ID is '%c'.  Can't handle this.\n", ddr.record_leader.leader_id);
		exit(0);
	}
	/*
	 * Should probably check, at this point, that the Interchange Level is not 3,
	 * since we can't handle such files in general.  However, it
	 * is possible that we can still parse them well enough to read
	 * an SDTS file, so we will take the chance and try handling
	 * level 3 files anyway.
	 */

	i = REC_LEADER_LEN;			// Start of DDR Directory
	j = ddr.record_leader.fa_addr;	// Start of Field Area

	/* Initialize so that we know nothing is initially present. */
	ddr.f0000.tag = (char *)0;
	ddr.f0000.name = (char *)0;
	ddr.f0000.field_cntrl = (char *)0;
	ddr.f0000.num_labels = 0;
	ddr.num_tags = 0;

	/*
	 * First we examine the field tag to find out if it is one of the special
	 * types of fields.  The special tags appear to be strings of all '0'
	 * characters, with a '0', '1', or '2' on the end.  Because the whole tag,
	 * except for the last digit, must be filled with '0' characters (at least
	 * as far as I know, without a copy of the standard), we play some pointer games
	 * and index into the comparison strings to get comparison strings of the
	 * correct length.  This gives us the odd-looking construct:
	 * "0000002" + 7 - ddr.record_leader.field_tag_len
	 * If the tag length is 4, then we are really comparing against "0002".
	 */
	while (i < (ddr.record_leader.fa_addr - 1))  {
		if (strncmp(&ddr_buf[i], "0000000", ddr.record_leader.field_tag_len) == 0)  {
			/* This is a file-control tag. */
			ddr.f0000.tag = &ddr_buf[i];
			i = i + ddr.record_leader.field_tag_len;

			k = i;
			save_byte = ddr_buf[i + ddr.record_leader.field_len_len];
			ddr_buf[i + ddr.record_leader.field_len_len] = '\0';
			ddr.f0000.field_len = strtol(&ddr_buf[i], (char **)0, 10);
			ddr_buf[i + ddr.record_leader.field_len_len] = save_byte;
			i = i + ddr.record_leader.field_len_len;
			ddr_buf[k] = '\0';	// Null-terminate tag.

			save_byte = ddr_buf[i + ddr.record_leader.field_pos_len];
			ddr_buf[i + ddr.record_leader.field_pos_len] = '\0';
			ddr.f0000.field_pos = strtol(&ddr_buf[i], (char **)0, 10);
			ddr_buf[i + ddr.record_leader.field_pos_len] = save_byte;
			i = i + ddr.record_leader.field_pos_len;

			if ((ddr.record_leader.ichg_level == 2) || (ddr.record_leader.ichg_level == 3))  {
				ddr.f0000.field_cntrl = &ddr_buf[j];
				j = j + ddr.record_leader.field_cntrl_len;
			}
			else  {
				ddr.f0000.field_cntrl = (char *)0;
			}

			ddr.f0000.name = &ddr_buf[j];
			k = ddr.record_leader.fa_addr + ddr.f0000.field_pos + ddr.f0000.field_len;
			for ( ; j < k; j++)  {	// Search the whole field, if necessary, for a terminator.
				if ((ddr_buf[j] == UNIT_TERMINATOR) || (ddr_buf[j] == FIELD_TERMINATOR))  {
					break;
				}
			}
			if (j == k)  {
				fprintf(stderr, "The file appears defective.  Can't proceed.\n");
				exit(0);
			}
			ddr_buf[j++] = '\0';	// Null terminate the end of the name.  The null-terminator may end up being the whole name if no name was present.
			j = k;
		}
		else if (strncmp(&ddr_buf[i], "0000002" + 7 - ddr.record_leader.field_tag_len, ddr.record_leader.field_tag_len) == 0)  {
			/* This is a user-augmented file description.  We don't know how to handle these. */
			fprintf(stderr, "File contains field tag of \"0..2\".  Can't handle this.\n");
			exit(0);
		}
		else if ((strncmp(&ddr_buf[i], "0000000" + 7 - ddr.record_leader.field_tag_len, ddr.record_leader.field_tag_len - 1) == 0) &&
			 (ddr_buf[i + ddr.record_leader.field_tag_len - 1] >= '3') &&
			 (ddr_buf[i + ddr.record_leader.field_tag_len - 1] <= '9'))  {
			/* This is one of the other special tags that we can't handle. */
			fprintf(stderr, "File contains field tag of \"0..%c\".  Can't handle this.\n", ddr_buf[i + ddr.record_leader.field_tag_len - 1]);
			exit(0);
		}
		else  {
			/* This is a plain old non-special tag.  (Which includes the pseudo-special "0..1" tag. */
			if (ddr.num_tags == MAX_TAGS)  {
				fprintf(stderr, "Ran out of space for field tags.  Can't proceed.\n");
				exit(0);
			}

			/*
			 * Initialize the various subfield storage spaces, in
			 * case we don't find any labels and/or formats.
			 */
			for (k = 0; k < MAX_SUBFIELDS; k++)  {
				ddr.user[ddr.num_tags].labels[k] = "";
				ddr.user[ddr.num_tags].formats[k] = "";
				ddr.user[ddr.num_tags].sizes[k] = 0;
				ddr.user[ddr.num_tags].cartesian[k] = 0;
			}

			ddr.user[ddr.num_tags].tag = &ddr_buf[i];
			i = i + ddr.record_leader.field_tag_len;

			k = i;
			save_byte = ddr_buf[i + ddr.record_leader.field_len_len];
			ddr_buf[i + ddr.record_leader.field_len_len] = '\0';
			ddr.user[ddr.num_tags].field_len = strtol(&ddr_buf[i], (char **)0, 10);
			ddr_buf[i + ddr.record_leader.field_len_len] = save_byte;
			i = i + ddr.record_leader.field_len_len;
			ddr_buf[k] = '\0';	// Null-terminate tag.

			save_byte = ddr_buf[i + ddr.record_leader.field_pos_len];
			ddr_buf[i + ddr.record_leader.field_pos_len] = '\0';
			ddr.user[ddr.num_tags].field_pos = strtol(&ddr_buf[i], (char **)0, 10);
			ddr_buf[i + ddr.record_leader.field_pos_len] = save_byte;
			i = i + ddr.record_leader.field_pos_len;

			if ((ddr.record_leader.ichg_level == 2) || (ddr.record_leader.ichg_level == 3))  {
				ddr.user[ddr.num_tags].field_cntrl = &ddr_buf[j];
				j = j + ddr.record_leader.field_cntrl_len;
			}
			else  {
				ddr.user[ddr.num_tags].field_cntrl = (char *)0;
			}

			ddr.user[ddr.num_tags].name = &ddr_buf[j];
			k = ddr.record_leader.fa_addr + ddr.user[ddr.num_tags].field_pos + ddr.user[ddr.num_tags].field_len;
			for ( ; j < k; j++)  {	// Search the whole field, if necessary, for a terminator.
				if ((ddr_buf[j] == UNIT_TERMINATOR) || (ddr_buf[j] == FIELD_TERMINATOR))  {
					break;
				}
			}
			if (j == k)  {
				fprintf(stderr, "The file appears defective.  Can't proceed.\n");
				exit(0);
			}
			if ((ddr_buf[j] != UNIT_TERMINATOR) ||
			    (ddr.record_leader.ichg_level == 1) ||
			    (ddr.user[ddr.num_tags].field_cntrl[0] == '0'))  {
				ddr_buf[j++] = '\0';	// Null terminate the end of the name.
				ddr.num_tags++;
				j = k;
				continue;
			}
			ddr_buf[j++] = '\0';	// Null terminate the end of the name.

			/*
			 * If there are any subfield labels, we need to parse them.
			 * They are normally separated by the '!' character, which is the vector
			 * delimiter.  We null out the '!' characters so that the labels
			 * will be null-terminated.
			 *
			 * We may also come across the '*' character, which is the
			 * cartesian delimiter.  If we come across the latter, we remember
			 * it for later use, and then null it just like the '!' delimiters.
			 */
			ddr.user[ddr.num_tags].num_labels = 0;
			if (ddr_buf[j] != UNIT_TERMINATOR)  {
				/*
				 * Either we have some labels, or we are at end of field.
				 * If we have labels, we locate them.
				 * If we are at end-of-field, we fall through the loop.
				 */

				/* Check for leading '*' delimiter */
				if ((j != k) && (ddr_buf[j] == '*'))  {
					j++;
					ddr.user[ddr.num_tags].cartesian[0] = 1;
				}

				/* Now, process the labels, if there are any. */
				while ((j != k) && (ddr_buf[j] != UNIT_TERMINATOR) && (ddr_buf[j] != FIELD_TERMINATOR))  {
					if (ddr.user[ddr.num_tags].num_labels == MAX_SUBFIELDS)  {
						fprintf(stderr, "Ran out of space for subfield labels.  Can't proceed.\n");
						exit(0);
					}

					ddr.user[ddr.num_tags].labels[ddr.user[ddr.num_tags].num_labels] = &ddr_buf[j];
					j++;
					for ( ; j < (k - 1); j++)  {
						if ((ddr_buf[j] == '!') || (ddr_buf[j] == '*') ||
						    (ddr_buf[j] == UNIT_TERMINATOR) || (ddr_buf[j] == FIELD_TERMINATOR))  {
							break;
						}
					}
					if (ddr_buf[j] == '*')  {
						if (ddr.user[ddr.num_tags].num_labels == (MAX_SUBFIELDS - 1))  {
							fprintf(stderr, "Ran out of space for subfield labels.  Can't proceed.\n");
							exit(0);
						}
						ddr.user[ddr.num_tags].cartesian[ddr.user[ddr.num_tags].num_labels + 1] = 1;
					}
					if (ddr_buf[j] != '!')  {
						ddr_buf[j++] = '\0';	// Null terminate the end of the tag.
						ddr.user[ddr.num_tags].num_labels++;
						break;
					}
					ddr_buf[j++] = '\0';	// Null terminate the end of the tag.
					ddr.user[ddr.num_tags].num_labels++;
				}
			}
			else  {
				j++;
			}

			/*
			 * If there are any format specifiers, we need to parse them.
			 * They are delimited by '(' and ')', and separated by ',' characters.
			 * They might look something like "(A,I,B,3I)" or "(A(3),I(4))".
			 * The leading 3 is a repeat count, and the numbers in parentheses are
			 * subfield sizes.
			 *
			 * Note that the format strings can be vastly more complex than what
			 * we can handle here.  (They can include constructs like "(A(4),3(I(2),I(3)))"; and
			 * "(A(,),I(5))" (where the "A(,)" is a comma-delimited ASCII string); and even
			 * more complex forms.)  We provide just enough functionality to handle
			 * known USGS data.  If it becomes necessary to handle more complex format
			 * strings, we should probably move the parsing to a separate function.
			 */
			ddr.user[ddr.num_tags].num_formats = 0;
			if (ddr_buf[j] != FIELD_TERMINATOR)  {
				if ((k - j) > 3)  {	// We need at least "(?)" followed by a field terminator before we can proceed.
					if ((ddr_buf[j] != '(') || (ddr_buf[k - 2] != ')') || (ddr_buf[k - 1] != FIELD_TERMINATOR))  {
						fprintf(stderr, "Subfield format specification looks wrong.  Can't proceed.\n");
						exit(0);
					}
					j++;
					ddr_buf[k - 2] = '\0';
					ddr_buf[k - 1] = '\0';

					/*
					 * Sometimes the () are double nested.  Haven't figured
					 * out why yet, but it seems harmless to strip off the extra pair.
					 */
					if ((ddr_buf[j] == '(') && (ddr_buf[k - 3] == ')'))  {
						j++;
						ddr_buf[k - 3] = '\0';
					}

					while ((j < (k - 2)) && (ddr_buf[j] != '\0'))  {
						repeat_count = 1;
						if ((ddr_buf[j] >= '0') && (ddr_buf[j] <= '9'))  {
							repeat_count = strtol(&ddr_buf[j], &end_ptr, 10);
							j = j + end_ptr - &ddr_buf[j];
						}
						if (ddr_buf[j] == '\0')  {
							fprintf(stderr, "Subfield format specification looks wrong.  Can't proceed.\n");
							exit(0);
						}

						/* Find the subfield size, if there is one. */
						if ((ptr = strchr(&ddr_buf[j], '(')) == (char *)0)  {
							size = 0;
						}
						else  {
							size = strtol(ptr + 1, (char **)0, 10);
							if ((ddr_buf[j] == 'B') || (ddr_buf[j] == 'b'))  {	// Don't know if 'b' ever gets used. Check just in case.
								/*
								 * If binary format, convert bit size into byte size.
								 * Bit fields whose size is not divisible by 8 are
								 * allowed by the standard, but we can't handle them.
								 * We assume all bit fields are simply 16-bit or
								 * 32-bit numbers stored in binary format.
								 */
								if (size & 0x7)  {
									fprintf(stderr, "Subfield size (%d) is not divisible by eight.\nThe standard allows this, but drawmap can't handle it.\n", size);
									exit(0);
								}
								size = size >> 3;
							}
							if (size < 0)  {
								fprintf(stderr, "Subfield size (%d) is unusable.\n", size);
								exit(0);
							}
							if (size == 0)  {
								/*
								 * Note:  If size == 0, then there was something non-numeric
								 * inside the parentheses.  We might want to check for
								 * delimited strings at this point.  I don't want to
								 * make the format parsing more complicated, though, until
								 * I know exactly how the format is specified in the
								 * standard.
								 */
								fprintf(stderr, "Warning: Subfield format string %s is unusual.  May cause trouble.\n", &ddr_buf[j]);
							}
						}

						while(repeat_count > 0)  {
							ddr.user[ddr.num_tags].formats[ddr.user[ddr.num_tags].num_formats] = &ddr_buf[j];
							ddr.user[ddr.num_tags].sizes[ddr.user[ddr.num_tags].num_formats++] = size;
							repeat_count--;
						}

						while ((ddr_buf[j] != ',') && (ddr_buf[j] != '\0'))  {
							j++;
						}
						ddr_buf[j++] = '\0';
					}
				}
			}

			/*
			 * It is okay for the labels and or formats to be missing.
			 * However, if they are both present, we insist that there be
			 * an equal number of each, because otherwise, we don't know
			 * what to do.
			 */
			if ((ddr.user[ddr.num_tags].num_formats > 0) && (ddr.user[ddr.num_tags].num_labels > 0))  {
				if (ddr.user[ddr.num_tags].num_formats != ddr.user[ddr.num_tags].num_labels)  {
					fprintf(stderr, "File does not contain a format descriptor for each subfield.  Can't handle this.\n");
					exit(0);
				}
			}

			ddr.num_tags++;
			j = k;
		}
	}
}





/*
 * Parse the DR record and put all of the
 * information into the DR structure.
 *
 * We have already parsed the record leader, in the read_record() function.
 * The values have been stored away in dr.record_leader.
 * This was done separately because the leader always has the same
 * interpretation for every record, and we can parse and check it
 * right after reading any record.
 *
 * Now we need to parse the rest of the DR.  The DR is similar in structure
 * to the DDR.  There is again a Directory, right after the Leader, that gives a
 * (tag, field length, field position) triple for each data field present.
 * Following this is the Field Area, which contains the actual data.
 *
 * Parsing is quite similar to parsing the DDR.  The Directory is parsed
 * in exactly the same way.  The Field Area is a simple listing of data,
 * corresponding to the subfields of each of the fields.
 */
static void
parse_dr()
{
	int32_t i;		// We use this as an index into the Directory
	int32_t j;		// We use this as an index into the Field Area
	int32_t k;
	char save_byte;

	if (leaderless_flag != 0)  {
		/*
		 * Once the leaderless_flag has been set, we shouldn't be calling this function.
		 */
		fprintf(stderr, "parse_dr() called during leaderless processing.  Something is wrong.\n");
		exit(0);
	}

	j = dr.record_leader.fa_addr;	// Start of Field Area

	dr.num_tags = 0;


	/*
	 * Iterate through the directory entries and stick the data
	 * into the dr structure.
	 */
	i = REC_LEADER_LEN;			// Start of DR Directory

	while (i < (dr.record_leader.fa_addr - 1))  {
		if (ddr.num_tags == MAX_TAGS)  {
			fprintf(stderr, "Ran out of space for field tags.  Can't proceed.\n");
			exit(0);
		}

		dr.user[dr.num_tags].tag = &dr_buf[i];
		i = i + dr.record_leader.field_tag_len;

		k = i;
		save_byte = dr_buf[i + dr.record_leader.field_len_len];
		dr_buf[i + dr.record_leader.field_len_len] = '\0';
		dr.user[dr.num_tags].field_len = strtol(&dr_buf[i], (char **)0, 10);
		dr_buf[i + dr.record_leader.field_len_len] = save_byte;
		i = i + dr.record_leader.field_len_len;
		dr_buf[k] = '\0';	// Null-terminate tag.

		save_byte = dr_buf[i + dr.record_leader.field_pos_len];
		dr_buf[i + dr.record_leader.field_pos_len] = '\0';
		dr.user[dr.num_tags].field_pos = strtol(&dr_buf[i], (char **)0, 10);
		dr_buf[i + dr.record_leader.field_pos_len] = save_byte;
		i = i + dr.record_leader.field_pos_len;

		dr.num_tags++;
		j = k;
	}

	/*
	 * If a record has a Leader ID of 'R' instead of 'D',
	 * then this is the last Record Leader and Directory
	 * in the file.  From this point on, we just keep reading
	 * the Field Area over and over again, until we reach the
	 * end of file, and we interpret it using the Directory
	 * entry in the just-parsed Directory.
	 */
	if (dr.record_leader.leader_id == 'R')  {
		leaderless_flag = 1;
	}
}




/*
 * When the user calls this function,
 * we return the next available subfield from the
 * file.  This routine depends on global
 * static state information, since it must
 * remember its state from one invocation to the next.
 *
 * This function returns 1 when it finds a subfield.
 * It returns 0 at end of file.
 * It exits on errors.
 *
 * In the subfield structure returned by this function,
 * the subfield.tag, subfield.label, and subfield.format
 * elements will be null-terminated.  The subfield.value
 * element will not be null-terminated, and you must
 * use the subfield.length element to find its end.
 */
int32_t
get_subfield(struct subfield *subfield)
{
	ssize_t ret_val;
	int32_t i;
	static int32_t data_index;
	int32_t ddr_index;
	int32_t field_limit;
	char *tag_wanted;
	int32_t max_labels_formats;	// contains the maximum of the number of labels or the number of formats

	/*
	 * Check whether we have used up all of the data from the last record we
	 * read.  If so, try to read another record.
	 */
	if (dr_tag >= dr.num_tags)  {
		/* We have finished with the old record and need to read another. */
		if (leaderless_flag == 0)  {
			if ((ret_val = read_record(&dr.record_leader, &dr_buf)) < 0)  {
				/* Error message was printed by read_record(), so just exit. */
				exit(0);
			}
			else if (ret_val == 0)  {
				return 0;
			}

			parse_dr(&dr);
		}
		else  {
			if ((ret_val = read_function(fdesc, &dr_buf[dr.record_leader.fa_addr],
			     dr.record_leader.length - dr.record_leader.fa_addr)) !=
			     (dr.record_leader.length - dr.record_leader.fa_addr))  {
				if (ret_val == 0)  {
					return 0;
				}
				else  {
					fprintf(stderr, "Tried to read %d bytes from SDTS record.  Got ret_val = %d\n",
						dr.record_leader.length - dr.record_leader.fa_addr, (int)ret_val);
					exit(0);
				}
			}
		}

		dr_tag = 0;
		dr_label = 0;
		data_index = dr.record_leader.fa_addr;
	}

	/*
	 * We are trying to pry the next tag/label pair out of the record.
	 * Set a pointer to the tag we are looking for, and then search
	 * for that tag in the DDR.
	 */
	tag_wanted = dr.user[dr_tag].tag;
	for (ddr_index = 0; ddr_index < ddr.num_tags; ddr_index++)  {
		if (strncmp(tag_wanted, ddr.user[ddr_index].tag, ddr.record_leader.field_tag_len) == 0)  {
			break;
		}
	}
	if (ddr_index == ddr.num_tags)  {
		fprintf(stderr, "Failed to find user tag %.*s in DDR.\n", ddr.record_leader.field_tag_len, tag_wanted);
		exit(0);
	}

	/*
	 * Handle the data based on its type.  This particular
	 * bunch of code is probably not anywhere near as
	 * complex as a full implementation of the standard
	 * would require.  Thus, if the program fails to parse a file,
	 * this block of code may need to be beefed up.
	 *
	 * The first byte of the format control string gives the structure type:
	 *   0 = Elementary Data	(A single data item per field)
	 *   1 = Vector Data		(Multiple data items per field.  One dimensional.)
	 *   2 = Array Data		(Multiple data items per field.  Two dimensional.)
	 *
	 * The second byte of the format control string gives the data type:
	 *   0 = Character (Simple character data:  ABC)
	 *   1 = Implicit point	(Numeric string with no explicit decimal point:  123)
	 *   2 = Explicit point (Numeric string with an explicit decimal point:  1.23)
	 *   3 = Explicit point scales (Numeric string with scale factor:  1.23E+04)
	 *   4 = Character mode bit string (binary bits: 01011101)
	 *   5 = Bit field (Similar to character mode bit string, but data is actual binary bit string)
	 *   6 = Mixed data types
	 *   7 = Haven't ever seen any above 6
	 *
	 * At this level of abstraction, we don't care much about the second byte.
	 * The interpretation of the data in the subfields is done at a higher level.
	 * However, the first byte adds some complications.  Structure types 0 and
	 * 1 are fairly straightforward, type 2 can take a variety of forms.
	 */
	if ((ddr.record_leader.ichg_level == -1) || (ddr.record_leader.ichg_level == 1) || (ddr.user[ddr_index].field_cntrl[0] == '0'))  {
		/*
		 * We have a simple atomic data field, with no subfield label.
		 */
		subfield->tag = dr.user[dr_tag].tag;
		subfield->label = "";
		subfield->value = dr_buf + data_index;
		subfield->format = "";
		subfield->length = dr.user[dr_tag].field_len - 1;	// Subtract 1 for the terminator

		data_index = data_index + dr.user[dr_tag].field_len;
		dr_buf[data_index - 1] = '\0';

		dr_label++;
		if (dr_label >= ddr.user[ddr_index].num_labels)  {
			dr_label = 0;
			dr_tag++;
		}
	}
	else if (ddr.user[ddr_index].field_cntrl[0] == '1')  {
		/*
		 * We have a vector of subfields, which may be of various types, each with its own label.
		 *
		 * The complications here arise when labels and/or formats
		 * are not present.
		 */
		subfield->tag = dr.user[dr_tag].tag;
		subfield->label = ddr.user[ddr_index].labels[dr_label];
		subfield->value = dr_buf + data_index;
		subfield->format = ddr.user[ddr_index].formats[dr_label];

		field_limit = dr.record_leader.fa_addr + dr.user[dr_tag].field_pos + dr.user[dr_tag].field_len;

		if (ddr.user[ddr_index].sizes[dr_label] > 0)  {
			/*
			 * A size was provided in the format string.  Use it.
			 * There shouldn't be any UNIT_TERMINATORS between subfields.
			 */
			subfield->length = ddr.user[ddr_index].sizes[dr_label];
			data_index = data_index + subfield->length;
			if (data_index == (field_limit - 1))  {
				/* If at end of field, step over FIELD_TERMINATOR */
				data_index++;
			}
		}
		else  {
			/*
			 * No size was provided in the format string.  (Or there was no format string.)
			 * Must find the end of the subfield via the terminator.
			 */
			for (i = data_index; i < field_limit; i++)  {
				if ((dr_buf[i] == UNIT_TERMINATOR) || (dr_buf[i] == FIELD_TERMINATOR))  {
					break;
				}
			}
			if (i == field_limit)  {
				fprintf(stderr, "Ran out of data in DR.\n");
				exit(0);
			}
			subfield->length = i - data_index;

			data_index = i + 1;
			dr_buf[data_index - 1] = '\0';
		}

		if ((ddr.user[ddr_index].num_labels > 0) || (ddr.user[ddr_index].num_formats > 0))  {
			dr_label++;
			if (dr_label >= ddr.user[ddr_index].num_labels)  {
				dr_label = 0;
				/*
				 * I added the following "if" statement, around the "dr_tag++;"
				 * statement, because some files don't define a sequence of
				 * X,Y pairs as cartesian arrays (which are handled below), but
				 * instead define such pairs as a non-cartesian pair of labels that
				 * gets re-used until the field_limit is reached.  I am somewhat
				 * doubtful that this type of construct is standards-conforming, but
				 * the construct is present in some USGS DEM files, so we attempt to
				 * handle it whether it is conforming or not.
				 */
				if (data_index == field_limit)  {
					dr_tag++;
				}
			}
		}
		else if (data_index == field_limit)  {
			dr_label = 0;
			dr_tag++;
		}
	}
	else if (ddr.user[ddr_index].field_cntrl[0] == '2')  {
		/*
		 * We have an array of subfields.
		 *
		 * Here is how I understand arrays, based on the sketchy data
		 * at hand.  (This understanding may be wrong.)
		 * The label field, in its most general form (which is called a
		 * cartesian label), looks like:
		 *
		 *  A!B!C*D!E
		 *
		 * where the number of labels before and after the '*' may
		 * differ from the example shown here.
		 * In front of the cartesian delimiter (the '*') are the
		 * row labels, and following it are the column labels.
		 * The data, in the Field Area will fill the A row
		 * with D and E values, then the B row with D and E
		 * values, then the C row with D and E values.
		 * (Actually, the situation is a bit more complicated, since
		 * the array concept is not limited to two dimensions; but
		 * let's ignore that complication for this simple routine.)
		 *
		 * This most general form is fairly straightforward to
		 * handle.  (We would probably need to figure out some way to
		 * return two or more subfield names at a time, but that isn't
		 * a big deal.  A single subfield string, of the form
		 * "(ROW_LABEL,COLUMN_LABEL)" would do the job.)
		 *
		 * The complications of this structure type arise when either
		 * the row or column labels are missing.
		 *
		 * In order to keep things simple, we are only going to
		 * support a single case, which is the only case I have
		 * found so far in the USGS files.  This is the case
		 * that looks like:
		 *
		 *   *ELEVATIONS
		 *
		 * or
		 *
		 *   *X!Y
		 *
		 * Technically, we should probably return a subfield label
		 * like "(,ELEVATIONS)", "(,X)", or "(,Y)", but we will keep
		 * it simple and just return "ELEVATIONS", "X", or "Y" for now.
		 *
		 * In the DDR parsing routine, we have stored away the locations where
		 * the '*' delimiter appears.  We don't use this information now, but
		 * it is available if we eventually need to handle some other cases.
		 */
		max_labels_formats = ddr.user[ddr_index].num_labels > ddr.user[ddr_index].num_formats ?
					ddr.user[ddr_index].num_labels : ddr.user[ddr_index].num_formats;

		subfield->tag = dr.user[dr_tag].tag;
		subfield->label = ddr.user[ddr_index].labels[dr_label];
		subfield->value = dr_buf + data_index;
		subfield->format = ddr.user[ddr_index].formats[dr_label];

		field_limit = dr.record_leader.fa_addr + dr.user[dr_tag].field_pos + dr.user[dr_tag].field_len;

		if (ddr.user[ddr_index].sizes[dr_label] > 0)  {
			/*
			 * A size was provided in the format string.  Use it.
			 * There shouldn't be any UNIT_TERMINATORS between subfields.
			 */
			subfield->length = ddr.user[ddr_index].sizes[dr_label];
			data_index = data_index + subfield->length;
			if (data_index == (field_limit - 1))  {
				/* If at end of field, step over FIELD_TERMINATOR */
				data_index++;
			}
		}
		else  {
			/*
			 * No size was provided in the format string.  (Or there was no format string.)
			 * Must find the end of the subfield via the terminator.
			 */
			for (i = data_index; i < field_limit; i++)  {
				if ((dr_buf[i] == UNIT_TERMINATOR) || (dr_buf[i] == FIELD_TERMINATOR))  {
					break;
				}
			}
			if (i == field_limit)  {
				fprintf(stderr, "Ran out of data in DR.\n");
				exit(0);
			}
			subfield->length = i - data_index;

			data_index = i + 1;
			dr_buf[data_index - 1] = '\0';
		}

		/*
		 * It is this little chunk of decision-making code that
		 * has been simplified to handle only the single
		 * case we discussed above.
		 */
		if (data_index >= (field_limit - 1))  {
			dr_label = 0;
			dr_tag++;
		}
		else  {
			dr_label++;
			if (dr_label >= max_labels_formats)  {
				dr_label = 0;
			}
		}
	}
	else  {
		fprintf(stderr, "Field structure type %c is unknown.\n", ddr.user[ddr_index].field_cntrl[0]);
		exit(0);
	}

	return 1;

}




/*
 * Open a DDF file for processing.
 */
int32_t
begin_ddf(char *file_name)
{
	int32_t length;

	leaderless_flag = 0;
	dr_tag = MAX_TAGS;
	dr_label = MAX_SUBFIELDS;

	length = strlen(file_name);

	if ((length > 3) && ((strcmp(file_name + length - 3, ".gz") == 0) ||
	    (strcmp(file_name + length - 3, ".GZ") == 0)))  {
		gz_flag = 1;
		read_function = buf_read_z;
		if ((fdesc = buf_open_z(file_name, O_RDONLY)) < 0)  {
			return(fdesc);
		}
	}
	else  {
		gz_flag = 0;
		read_function = buf_read;
		if ((fdesc = buf_open(file_name, O_RDONLY)) < 0)  {
			return(fdesc);
		}
	}

	/*
	 * Read and parse the DDR.
	 */
	parse_ddr();

	return fdesc;
}




/*
 * Close an open DDF file.
 */
void
end_ddf()
{
	if (gz_flag == 0)  {
		buf_close(fdesc);
	}
	else  {
		buf_close_z(fdesc);
	}

	if (ddr_buf != (char *)0)  {
		free(ddr_buf);
	}
	if (dr_buf != (char *)0)  {
		free(dr_buf);
	}
}





/*
 * This is a simple program to exercise the above code.
 * Given a DDF file, the program
 * simply prints out every bit of data in the file.
 *
 * The first argument is the file name to open and parse.
 * If there is a second argument (and we don't care what it looks
 * like) the output will be in a compact form.
 *
 * To compile this program, do:
 *
 * cc -g -o sdts_test sdts_utils.c big_buf_io.c big_buf_io_z.c gunzip.c utilities.c -lm
 */
//main(int argc, char *argv[])
//{
//	struct subfield subfield;
//	int32_t compact_flag;
//	int32_t byte_order;
//	int32_t i;
//	short j;
//	int32_t length;
//
//
//	if ((argc != 2) && (argc != 3))  {
//		fprintf(stderr, "Usage:  %s file_name.ddf [compact_flag]\n", argv[0]);
//		exit(0);
//	}
//	if (argc == 3)  {
//		compact_flag = 1;
//	}
//	else  {
//		compact_flag = 0;
//	}
//
//	/* find the native byte-order on this machine. */
//	byte_order = swab_type();
//
//
//	/* Open the DDF file. */
//	if (begin_ddf(argv[1]) < 0)  {
//		fprintf(stderr, "Couldn't open input file.\n");
//		exit(0);
//	}
//
//	/* print out the DDR for examination */
////	print_ddr();
//
//	/*
//	 * Read and parse a DR.
//	 */
//	while (get_subfield(&subfield) > 0)  {
//		if (compact_flag == 0)  {
//			fprintf(stdout, "subfield.tag = %s\n", subfield.tag);
//			fprintf(stdout, "subfield.label = %s\n", subfield.label);
//			fprintf(stdout, "subfield.format = %s\n", subfield.format);
//			fprintf(stdout, "subfield.length = %d\n", subfield.length);
//			if (strstr(subfield.format, "B") != (char *)0)  {
//				if (subfield.length == 4)  {
//					/* Special handling for 4-byte binary values. */
//					fprintf(stdout, "subfield.value = unswabbed bin: 0x%2.2x%2.2x%2.2x%2.2x\t",
//						0x000000ff & (int32_t)subfield.value[0],
//						0x000000ff & (int32_t)subfield.value[1],
//						0x000000ff & (int32_t)subfield.value[2],
//						0x000000ff & (int32_t)subfield.value[3]);
//					i = (((int32_t)subfield.value[3] & 0xff) << 24) |
//					    (((int32_t)subfield.value[2] & 0xff) << 16) |
//					    (((int32_t)subfield.value[1] & 0xff) <<  8) |
//					     ((int32_t)subfield.value[0] & 0xff);
//					if (byte_order == 1)  {
//						LE_SWAB(&i);
//					}
//					else if (byte_order == 2)  {
//						PDP_SWAB(&i);
//					}
//					fprintf(stdout, "swabbed dec: %10lu (unsigned)\t", i);
//					fprintf(stdout, "%11ld (signed)\n\n", i);
//				}
//				else  {
//					/* Special handling for 2-byte binary values. */
//					fprintf(stdout, "subfield.value = unswabbed bin: 0x%2.2x%2.2x\t",
//						0x000000ff & (int32_t)subfield.value[0],
//						0x000000ff & (int32_t)subfield.value[1]);
//					if (byte_order == 0)  {
//						j = (((int32_t)subfield.value[1] << 8) & 0x0000ff00) + ((int32_t)subfield.value[0] & 0x000000ff);
//					}
//					else  {
//						j = (((int32_t)subfield.value[0] << 8) & 0x0000ff00) + ((int32_t)subfield.value[1] & 0x000000ff);
//					}
//					fprintf(stdout, "swabbed dec: %5hu (unsigned)\t", j);
//					fprintf(stdout, "%6hd (signed)\n\n", j);
//				}
//			}
//			else  {
//				/* Non-binary subfields can just be printed as ASCII strings. */
//				fprintf(stdout, "subfield.value = \"%.*s\"\n\n", subfield.length, subfield.value);
//			}
//		}
//		else  {
//			/*
//			 * If we are at the beginning of a record, print out an extra newline.
//			 */
//			length = strlen(subfield.tag);
//			for (i = 0; i < (length - 1); i++)  {
//				if (subfield.tag[i] != '0')  {
//					break;
//				}
//			}
//			if ((i == (length - 1)) && (subfield.tag[length - 1] >= '0') && (subfield.tag[length - 1] <= '9'))  {
//				fprintf(stdout, "\n");
//			}
//
//			fprintf(stdout, "%s\t", subfield.tag);
//			fprintf(stdout, "%s\t", subfield.label);
//			fprintf(stdout, "%s\t", subfield.format);
//			fprintf(stdout, "%d\t", subfield.length);
//			if (strstr(subfield.format, "B") != (char *)0)  {
//				if (subfield.length == 4)  {
//					/* Special handling for 4-byte binary values. */
//					fprintf(stdout, "unswabbed bin: 0x%2.2x%2.2x%2.2x%2.2x\t",
//						0x000000ff & (int32_t)subfield.value[0],
//						0x000000ff & (int32_t)subfield.value[1],
//						0x000000ff & (int32_t)subfield.value[2],
//						0x000000ff & (int32_t)subfield.value[3]);
//					i = (((int32_t)subfield.value[3] & 0xff) << 24) |
//					    (((int32_t)subfield.value[2] & 0xff) << 16) |
//					    (((int32_t)subfield.value[1] & 0xff) <<  8) |
//					     ((int32_t)subfield.value[0] & 0xff);
//					if (byte_order == 1)  {
//						LE_SWAB(&i);
//					}
//					else if (byte_order == 2)  {
//						PDP_SWAB(&i);
//					}
//					fprintf(stdout, "swabbed dec: %10lu (unsigned)\t", i);
//					fprintf(stdout, "%11ld (signed)\n", i);
//				}
//				else  {
//					/* Special handling for 2-byte binary values. */
//					fprintf(stdout, "unswabbed bin: 0x%2.2x%2.2x\t",
//						0x000000ff & (int32_t)subfield.value[0],
//						0x000000ff & (int32_t)subfield.value[1]);
//					if (byte_order == 0)  {
//						j = (((int32_t)subfield.value[1] << 8) & 0x0000ff00) + ((int32_t)subfield.value[0] & 0x000000ff);
//					}
//					else  {
//						j = (((int32_t)subfield.value[0] << 8) & 0x0000ff00) + ((int32_t)subfield.value[1] & 0x000000ff);
//					}
//					fprintf(stdout, "swabbed dec: %5hu (unsigned)\t", j);
//					fprintf(stdout, "%6hd (signed)\n", j);
//				}
//			}
//			else  {
//				/* Non-binary subfields can just be printed as ASCII strings. */
//				fprintf(stdout, "\"%.*s\"\n", subfield.length, subfield.value);
//			}
//		}
//	}
//
//	end_ddf();
//
//	exit(0);
//}
