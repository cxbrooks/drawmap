/*
 * =========================================================================
 * dem_sdts.c - Routines to handle DEM data.
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
#include <string.h>
#include <math.h>
#include <errno.h>
#include "drawmap.h"
#include "dem.h"
#include "sdts_utils.h"


/*
 * The routines in this file are uniquely-dedicated to handling
 * DEM files in the Spatial Data Transfer System (SDTS) format.
 *
 * For a general description of SDTS, see sdts_utils.c.
 * For a description of the `classic' DEM format, and a discussion of
 * where all of the data has been moved to in the SDTS format, see dem.h.
 *
 * The routines in this file are fairly repetitive.  They open an SDTS
 * file, find the few things we care about, close the SDTS file,
 * and move on to the next SDTS file.  Thus, there isn't a lot of
 * unique code in this file, just a lot of tedious searching for data.
 */



/*
 * Note to the reader of this code.  This code will probably be difficult
 * to understand unless you are very familiar with the internals of SDTS files
 * and `classic' DEM files.  Normally I would provide a lot of descriptive
 * comments to help you along.  However, in this case, such comments would
 * probably end up being several times the length of the code.  I wrote this
 * program with two large documents available for reference.  If you want to
 * follow the operation of the code, you will probably need those documents
 * too.  The documents were:
 *
 * The Spatial Data Transfer Standard Mapping of the USGS Digital Elevation Model,
 * 11/13/97 version 1, by Mid-Continent Mapping Center Branch of Research, Technology
 * and Applications.
 *
 * Standards for Digital Elevation Models, US Department of the Interior,
 * US Geological Survey, National Mapping Division, 8/97.
 *
 * There are comments at key points in the code, but they are not adequate
 * for a full understanding unless you have the reference materials at hand.
 *
 * Even the documents aren't really enough.  It is also useful to have
 * both sample SDTS files and sample `classic' DEM files for reference as well.
 */



/*
 * This routine parses informational data from various SDTS files
 * and inserts the converted data into the given storage structure.
 * The storage structure is based on the type A header record from the old
 * DEM format.  While this structure doesn't correpond to the layout
 * of SDTS files, it is still a useful place to stuff the interesting
 * data.  More importantly, it lets us parse both SDTS and non-SDTS files
 * and still end up with the same data structure full of data.
 *
 * Here are the meanings of the various module names associated with DEM files:
 *
 * There is one module associated with Identification:
 *   IDEN --- Identification
 *
 * Misc:
 *   STAT --- Transfer Statistics
 *   CATD --- Catalog/Directory
 *   CATS --- Catalog/Spatial Domain
 *   LDEF --- Layer Definition
 *   RSDF --- Raster Definition
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
 *   DDOM --- Data Dictionary/Domain
 *   DDDF --- Data Dictionary/Definition
 *
 * There are three modules associated with spatial reference and domain:
 *   XREF --- External Spatial Reference
 *   IREF --- Internal Spatial Reference
 *   SPDM --- Spatial Domain
 *
 * Files associated with data:
 *   CELL --- Actual data
 *
 *
 * Here are the particular items we are interested in, within this morass of files,
 * given as module/field/subfield triples, along with the associated fields in the
 * dem_a structure.  (Note that some fields, present in the old DEM files, are no
 * longer present in SDTS.)
 *	IDEN/IDEN/TITL	dem_a->title
 *	DQPA/DQPA/COMT	dem_a->level_code		also in DQHL/DQHL/COMT
 *	DQPA/DQPA/COMT	dem_a->accuracy
 *	XREF/XREF/RSNM	dem_a->plane_ref
 *	XREF/XREF/ZONE	dem_a->zone
 *	XREF/XREF/HDAT	dem_a->horizontal_datum
 *	NONE          	dem_a->plane_units		Old-format field not encoded in SDTS.  SDTS requires meters for UTM and decimal degrees for GEO
 *	SPDM/DMSA/X   	dem_a->sw_x_gp			see also SPDM/SPDM/DTYP
 *	SPDM/DMSA/Y   	dem_a->sw_y_gp
 *	SPDM/DMSA/X   	dem_a->nw_x_gp
 *	SPDM/DMSA/Y   	dem_a->nw_y_gp
 *	SPDM/DMSA/X   	dem_a->ne_x_gp
 *	SPDM/DMSA/Y   	dem_a->ne_y_gp
 *	SPDM/DMSA/X   	dem_a->se_x_gp
 *	SPDM/DMSA/Y   	dem_a->se_y_gp
 *	DDOM/DDOM/DVAL	dem_a->void_fill
 *	DDOM/DDOM/DVAL	dem_a->edge_fill
 *	DDOM/DDOM/DVAL	dem_a->min_elev
 *	DDOM/DDOM/DVAL	dem_a->max_elev
 *	NONE          	dem_a->angle			Old-format field not encoded in SDTS.  Not in use.  Assumed to be always 0.
 *	IREF/IREF/XHRS	dem_a->x_res
 *	IREF/IREF/YHRS	dem_a->y_res
 *	IREF/IREF/SFAX	x_scale_factor
 *	IREF/IREF/SFAY	y_scale_factor
 *	DDSH/DDSH/UNIT	dem_a->elev_units
 *	DDSH/DDSH/PREC	dem_a->z_res
 *	LDEF/LDEF/NROW	dem_a->rows			see also RSDF/RSDF/RWXT (row extent)
 *	LDEF/LDEF/NCOL	dem_a->cols			see also RSDF/RSDF/CLXT (col extent)
 *	RSDF/SADR/X   	dem_a->x_gp_first		First elevation in profile (profiles in SDTS run from W to E and are padded to full row length)
 *	RSDF/SADR/Y   	dem_a->y_gp_first		First elevation in profile (profiles in SDTS run from W to E and are padded to full row length)
 *	CELL/CVLS/ELEVATION				An array of elevations (the actual DEM data)
 */
int
parse_dem_sdts(char *passed_file_name, struct dem_record_type_a *dem_a,
		struct dem_record_type_c *dem_c, struct datum *dem_datum, int32_t gz_flag)
{
	int32_t i;
	int32_t layer;
	int32_t file_name_length;
	char file_name[MAX_FILE_NAME];
	int32_t byte_order;
	int32_t upper_case_flag;
	int32_t need;
	struct subfield subfield;
	char save_byte;
	int32_t record_id;
	// give these variables bogus values so they will generate errors if not properly initialized.
	double x_scale_factor = 1000000.0, y_scale_factor = 1000000.0;
	double x_origin = 1000000.0, y_origin = 1000000.0;
	char *ptr;


	/*
	 * Make a copy of the file name.  The one we were originally
	 * given is still stored in the command line arguments.
	 * It is probably a good idea not to alter those, lest we
	 * scribble something we don't want to scribble.
	 */
	strncpy(file_name, passed_file_name, MAX_FILE_NAME - 1);
	file_name[MAX_FILE_NAME - 1] = '\0';
	if ((file_name_length = strlen(file_name)) < 12)  {
		/*
		 * Excluding the initial path, the file name should have the form
		 * ????CE??.DDF, perhaps with a ".gz" on the end.  If it isn't
		 * at least long enough to have this form, then reject it.
		 */
		fprintf(stderr, "File name %s doesn't look right.\n", file_name);
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
	 * Parse all of the information that we care about.
	 * For now, don't waste time parsing things that aren't
	 * currently interesting.
	 *
	 * Even to get the few things we care about, we need to
	 * examine several files.
	 *
	 * There are a lot of comments in dem.h describing the various
	 * data items, so this block of code is presented largely
	 * sans comments.
	 *
	 * Most of this routine is basically the same block of code,
	 * over and over, as we read a succession of files to get
	 * the data we need.
	 */

	/* Begin by finding the native byte-order on this machine. */
	byte_order = swab_type();



	/*
	 * The first file name we need is the IDEN module, which contains
	 * the DEM title.
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
	need = 1;
	while (get_subfield(&subfield) != 0)  {
		if (strcmp(subfield.tag, "IDEN") == 0)  {
			if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "TITL") == 0))  {
				strncpy(dem_a->title, subfield.value, subfield.length);
				dem_a->title[subfield.length] = '\0';

				/* This is all we need.  Break out of the loop. */
				need--;
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



	/*
	 * The next file name we need is the DQPA module, which contains
	 * some quality information, including the DEM level and some Root
	 * Mean Square Error (RMSE) statistics.
	 */
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "dqpa.ddf", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "dqpa.ddf", 8);
		}
	}
	else  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "DQPA.DDF", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "DQPA.DDF", 8);
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
	need = 2;
	record_id = -1;
	while (get_subfield(&subfield) != 0)  {
		if (strcmp(subfield.tag, "DQPA") == 0)  {
			if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				record_id = strtol(subfield.value, (char **)0, 10);
				subfield.value[subfield.length] = save_byte;
			}
			else if ((record_id == 1) && (strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "COMT") == 0))  {
				dem_a->level_code = -1;
				if (subfield.length > 10)  {
					if (strncmp(subfield.value, "DEM LEVEL ", 10) == 0)  {
						/*
						 * Since, in SDTS, the DEM level is part of a line of commentary,
						 * it is hard to be sure we have the correct value.  Give a
						 * reasonable try at finding it, since it always appears to be
						 * at the same place in the comment, but don't get too worked up
						 * about it, since it isn't that crucial that we know it.
						 */
						dem_a->level_code = subfield.value[10] - '0';
					}
				}

				need--;
			}
			else if ((record_id == 2) && (strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "COMT") == 0))  {
				/*
				 * The RMSE statistics are hard to dig out, because they are embedded
				 * in a long text string that can come in several basic forms.
				 * Thus, this block of code is complicated.
				 */
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				if (strncmp(subfield.value, "ACCURACY: Unspecified", strlen("ACCURACY: Unspecified")) == 0)  {
					dem_a->accuracy = 0;
				}
				else  {
					dem_c->datum_sample_size = 0;
					dem_c->datum_rmse_x = 0;
					dem_c->datum_rmse_y = 0;
					dem_c->datum_rmse_z = 0;
					dem_c->datum_stats_flag = 0;
					if ((ptr = strstr(subfield.value, "absolute datum (x")) != (char *)0)  {
						i = ptr - subfield.value + strlen("absolute datum (x");
						if ((ptr = strstr(&subfield.value[i], "(")) != (char *)0)  {
							ptr++;
							if (((*ptr >= '0') && (*ptr <= '9')) || (*ptr == ' '))  {
								dem_c->datum_rmse_x = strtol(ptr, &ptr, 10);
								if ((*ptr == ',') || (*ptr == ' '))  {
									ptr++;
									if (((*ptr >= '0') && (*ptr <= '9')) || (*ptr == ' '))  {
										dem_c->datum_rmse_y = strtol(ptr, &ptr, 10);
										if ((*ptr == ',') || (*ptr == ' '))  {
											ptr++;
											dem_c->datum_rmse_z = strtol(ptr, &ptr, 10);
											if ((ptr = strstr(ptr, "); ")) != (char *)0)  {
												ptr = ptr + 3;
												if (strncmp(ptr, "accuracy is estimated", strlen("accuracy is estimated")) == 0)  {
													dem_c->datum_stats_flag = 1;
												}
												else if (strncmp(ptr, "accuracy has been", strlen("accuracy has been")) == 0)  {
													if ((ptr = strstr(ptr, "size of ")) != (char *)0)  {
														ptr += strlen("size of ");
														dem_c->datum_sample_size = strtol(ptr, (char **)0, 10);
														dem_c->datum_stats_flag = 1;
													}
												}
											}
										}
									}
								}
							}
						}
					}

					dem_c->dem_sample_size = 0;
					dem_c->dem_rmse_x = 0;
					dem_c->dem_rmse_y = 0;
					dem_c->dem_rmse_z = 0;
					dem_c->dem_stats_flag = 0;
					if ((ptr = strstr(subfield.value, "file's datum (x")) != (char *)0)  {
						i = ptr - subfield.value + strlen("file's datum (x");
						if ((ptr = strstr(&subfield.value[i], "(")) != (char *)0)  {
							ptr++;
							if (((*ptr >= '0') && (*ptr <= '9')) || (*ptr == ' '))  {
								dem_c->dem_rmse_x = strtol(ptr, &ptr, 10);
								if ((*ptr == ',') || (*ptr == ' '))  {
									ptr++;
									if (((*ptr >= '0') && (*ptr <= '9')) || (*ptr == ' '))  {
										dem_c->dem_rmse_y = strtol(ptr, &ptr, 10);
										if ((*ptr == ',') || (*ptr == ' '))  {
											ptr++;
											dem_c->dem_rmse_z = strtol(ptr, &ptr, 10);
											if ((ptr = strstr(ptr, "); ")) != (char *)0)  {
												ptr = ptr + 3;
												if (strncmp(ptr, "accuracy is estimated", strlen("accuracy is estimated")) == 0)  {
													dem_c->dem_stats_flag = 1;
												}
												else if (strncmp(ptr, "accuracy has been", strlen("accuracy has been")) == 0)  {
													if ((ptr = strstr(ptr, "size of ")) != (char *)0)  {
														ptr += strlen("size of ");
														dem_c->dem_sample_size = strtol(ptr, (char **)0, 10);
														dem_c->dem_stats_flag = 1;
													}
												}
											}
										}
									}
								}
							}
						}
					}

					dem_a->accuracy = 1;
				}
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



	/*
	 * The next file name we need is the XREF module, which contains
	 * information relating to the planimetric reference system, zone, and datum.
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
	dem_a->vertical_datum_shift = 0.0;	// Set this one in case we don't find it.
	need = 5;
	while (get_subfield(&subfield) != 0)  {
		if (strcmp(subfield.tag, "XREF") == 0)  {
			if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "RSNM") == 0))  {
				/*
				 * Valid choices are "UTM", "GEO", or "SPCS"
				 */
				if (strncmp(subfield.value, "GEO", subfield.length) == 0)  {
					dem_a->plane_ref = 0;
					dem_a->plane_units = 3;				// For GEO, it is decimal degrees.  Cheat a bit, by putting in the old code for arc-seconds.
				}
				else if (strncmp(subfield.value, "UTM", subfield.length) == 0)  {
					dem_a->plane_ref = 1;
					dem_a->plane_units = 2;
				}
				else if (strncmp(subfield.value, "SPCS", subfield.length) == 0)  {
					dem_a->plane_ref = 2;
					dem_a->plane_units = 2;
				}
				else  {
					dem_a->plane_ref = -1;
					dem_a->plane_units = -1;
				}
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "ZONE") == 0))  {
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				dem_a->zone = strtol(subfield.value, (char **)0, 10);
				subfield.value[subfield.length] = save_byte;
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "HDAT") == 0))  {
				/*
				 * Valid choices are "NAS" for NAD-27, "WGC" for WGS-72, "WGE" for WGS-84, "NAX" for NAD-83,
				 * "OHD" for old Hawaii, and "PRD" for Puerto Rico.
				 */
				if (strncmp(subfield.value, "NAS", subfield.length) == 0)  {
					dem_a->horizontal_datum = 1;
				}
				else if (strncmp(subfield.value, "WGC", subfield.length) == 0)  {
					dem_a->horizontal_datum = 2;
				}
				else if (strncmp(subfield.value, "WGE", subfield.length) == 0)  {
					dem_a->horizontal_datum = 3;
				}
				else if (strncmp(subfield.value, "NAX", subfield.length) == 0)  {
					dem_a->horizontal_datum = 4;
				}
				else if (strncmp(subfield.value, "OHD", subfield.length) == 0)  {
					dem_a->horizontal_datum = 5;
				}
				else if (strncmp(subfield.value, "PRD", subfield.length) == 0)  {
					dem_a->horizontal_datum = 6;
				}
				else  {
					dem_a->horizontal_datum = -1;
				}
				need--;
			}
			else if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "COMT") == 0))  {
				if ((subfield.length > 0) && ((subfield.value[0] == 'V') || (subfield.value[0] == 'v')))  {
					for (i = 20; (i < 30) && (i < subfield.length); i++)  {
						if ((subfield.value[i] >= '0') && (subfield.value[i] <= '9'))  {
							dem_a->vertical_datum_shift = strtod(&subfield.value[i], (char **)0);
							break;
						}
					}
				}
				need--;
			}
			if (need == 0)  {
				/* This is all we need.  Break out of the loop. */
				break;
			}
		}
		if (strcmp(subfield.tag, "VATT") == 0)  {
			if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "VDAT") == 0))  {
				/*
				 * Valid choices are "NAS" for NAD-27, "WGC" for WGS-72, "WGE" for WGS-84, "NAX" for NAD-83,
				 * "OHD" for old Hawaii, and "PRD" for Puerto Rico.
				 */
				if (strncmp(subfield.value, "LMSL", subfield.length) == 0)  {
					dem_a->vertical_datum = 1;
				}
				else if (strncmp(subfield.value, "NGVD", subfield.length) == 0)  {
					dem_a->vertical_datum = 2;
				}
				else if (strncmp(subfield.value, "NAVD", subfield.length) == 0)  {
					dem_a->vertical_datum = 3;
				}
				else  {
					dem_a->vertical_datum = -1;
				}
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



	/*
	 * The next file name we need is the SPDM module, which contains
	 * the UTM coordinates of the corners.
	 */
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "spdm.ddf", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "spdm.ddf", 8);
		}
	}
	else  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "SPDM.DDF", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "SPDM.DDF", 8);
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
	 * Some SDTS transfers may contain multiple CE files,
	 * and we need to pull the correct record out of the AHDR file.
	 *
	 * Actually, I haven't come across any SDTS DEMs yet that
	 * have more than the ????CEL0.DDF file.  However, we include
	 * all of this layer stuff on the premise that it should work
	 * for existing files, and should also work if we come across
	 * data with multiple layers.
	 */
	if (gz_flag != 0)  {
	 	layer = strtol(&passed_file_name[file_name_length - 8], (char **)0, 10) + 1;
	}
	else  {
		layer = strtol(&passed_file_name[file_name_length - 5], (char **)0, 10) + 1;
	}
	if (layer <= 0)  {
		fprintf(stderr, "Got bad layer number (%d) from file %s.\n", layer, passed_file_name);
		return 1;
	}
	/*
	 * Loop through the subfields until we find what we want.
	 */
	need = 8;
	while (get_subfield(&subfield) != 0)  {
		if (strcmp(subfield.tag, "SPDM") == 0)  {
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
		else if ((layer < 0) && (strcmp(subfield.tag, "DMSA") == 0))  {
			if ((strstr(subfield.format, "R") != (char *)0) && ((strcmp(subfield.label, "X") == 0) || (strcmp(subfield.label, "Y") == 0)))  {
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				switch (need)  {
				case 8:
					dem_a->sw_x_gp = strtod(subfield.value, (char **)0);
					break;
				case 7:
					dem_a->sw_y_gp = strtod(subfield.value, (char **)0);
					break;
				case 6:
					dem_a->nw_x_gp = strtod(subfield.value, (char **)0);
					break;
				case 5:
					dem_a->nw_y_gp = strtod(subfield.value, (char **)0);
					break;
				case 4:
					dem_a->ne_x_gp = strtod(subfield.value, (char **)0);
					break;
				case 3:
					dem_a->ne_y_gp = strtod(subfield.value, (char **)0);
					break;
				case 2:
					dem_a->se_x_gp = strtod(subfield.value, (char **)0);
					break;
				case 1:
					dem_a->se_y_gp = strtod(subfield.value, (char **)0);
					break;
				}
				need--;
				subfield.value[subfield.length] = save_byte;
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



	/*
	 * The next file name we need is the DDOM module, which contains
	 * the elevation values for non-valid map areas, and
	 * the minimum and maximum elevations.
	 */
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "ddom.ddf", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "ddom.ddf", 8);
		}
	}
	else  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "DDOM.DDF", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "DDOM.DDF", 8);
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
	need = 4;
	while (get_subfield(&subfield) != 0)  {
		if (strcmp(subfield.tag, "DDOM") == 0)  {
			/*
			 * Some DEM files use "R" for these values instead of "I".
			 * Thus, we must check for both.
			 */
			if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "DVAL") == 0))  {
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				switch (need)  {
				case 4:
					dem_a->void_fill = strtol(subfield.value, (char **)0, 10);
					break;
				case 3:
					dem_a->edge_fill = strtol(subfield.value, (char **)0, 10);
					break;
				case 2:
					dem_a->min_elev = strtol(subfield.value, (char **)0, 10);
					break;
				case 1:
					dem_a->max_elev = strtol(subfield.value, (char **)0, 10);
					break;
				}
				need--;
				subfield.value[subfield.length] = save_byte;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strcmp(subfield.label, "DVAL") == 0))  {
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				switch (need)  {
				case 4:
					dem_a->void_fill = drawmap_round(strtod(subfield.value, (char **)0));
					break;
				case 3:
					dem_a->edge_fill = drawmap_round(strtod(subfield.value, (char **)0));
					break;
				case 2:
					dem_a->min_elev = drawmap_round(strtod(subfield.value, (char **)0));
					break;
				case 1:
					dem_a->max_elev = drawmap_round(strtod(subfield.value, (char **)0));
					break;
				}
				need--;
				subfield.value[subfield.length] = save_byte;
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



	/*
	 * The next file name we need is the IREF module, which contains
	 * the horizontal x and y resolution, and the x and y scale factors
	 * for UTM coordinates.  (Some UTM coordinates are stored as binary integer
	 * values, and then multiplied by the scale factors after conversion
	 * to floating point.  This is true for the location of the first point
	 * in the first profile, stored in RSDF/SADR/X, RSDF/SADR/Y.  However,
	 * the coordinates of the four corners of the map are stored as real
	 * numbers in SPDM/DMSA/X, SPDM/DMSA/Y.)
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
			if ((strstr(subfield.format, "R") != (char *)0) && (strcmp(subfield.label, "XHRS") == 0))  {
				dem_a->x_res = strtod(subfield.value, (char **)0);
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strcmp(subfield.label, "YHRS") == 0))  {
				dem_a->y_res = strtod(subfield.value, (char **)0);
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strcmp(subfield.label, "SFAX") == 0))  {
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
	 * The next file name we need is the DDSH module, which contains
	 * the elevation units and the vertical resolution.
	 */
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "ddsh.ddf", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "ddsh.ddf", 8);
		}
	}
	else  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "DDSH.DDF", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "DDSH.DDF", 8);
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
	need = 2;
	while (get_subfield(&subfield) != 0)  {
		if (strcmp(subfield.tag, "DDSH") == 0)  {
			if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "UNIT") == 0))  {
				if (strncmp(subfield.value, "FEET", subfield.length) == 0)  {
					dem_a->elev_units = 1;
				}
				else if (strncmp(subfield.value, "METERS", subfield.length) == 0)  {
					dem_a->elev_units = 2;
				}
				else  {
					dem_a->elev_units = -1;
				}
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strcmp(subfield.label, "PREC") == 0))  {
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				dem_a->z_res = strtod(subfield.value, (char **)0);
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



	/*
	 * The next file name we need is the LDEF module, which contains
	 * the numbers of rows and columns.
	 */
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "ldef.ddf", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "ldef.ddf", 8);
		}
	}
	else  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "LDEF.DDF", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "LDEF.DDF", 8);
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
	need = 2;
	while (get_subfield(&subfield) != 0)  {
		if (strcmp(subfield.tag, "LDEF") == 0)  {
			if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "NROW") == 0))  {
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				dem_a->rows = strtol(subfield.value, (char **)0, 10);
				subfield.value[subfield.length] = save_byte;
				need--;
			}
			else if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "NCOL") == 0))  {
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				dem_a->cols = strtol(subfield.value, (char **)0, 10);
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



	/*
	 * The next file name we need is the RSDF module, which contains
	 * the (x_gp, y_gp) coordinates of the first profile in the DEM,
	 * and the elevation pattern parameter.
	 */
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "rsdf.ddf", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "rsdf.ddf", 8);
		}
	}
	else  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "RSDF.DDF", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "RSDF.DDF", 8);
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
		if (strcmp(subfield.tag, "SADR") == 0)  {
			/*
			 * These two entries are special because they contain UTM coordinates which, unlike
			 * other numeric values, are stored in two's-complement binary format, rather than the real
			 * 'R' format that we might otherwise expect.  This type of storage is okay, for this
			 * application because the DEM coordinates are always round multiples of 10 or 30, and
			 * hence have no fractional component.  This binary data must be swabbed,
			 * if necessary, during the conversion to internal format.
			 */
			if ((strstr(subfield.format, "B") != (char *)0) && (strcmp(subfield.label, "X") == 0))  {
				if (subfield.length != 4)  {
					/* Error */
					dem_a->x_gp_first = -1.0;
				}
				else  {
					i = (((int32_t)subfield.value[3] & 0xff) << 24) |
					    (((int32_t)subfield.value[2] & 0xff) << 16) |
					    (((int32_t)subfield.value[1] & 0xff) <<  8) |
					     ((int32_t)subfield.value[0] & 0xff);
					if (byte_order == 0)  {
						dem_a->x_gp_first = (double)i;
					}
					else if (byte_order == 1)  {
						LE_SWAB(&i);
						dem_a->x_gp_first = (double)i;
					}
					else if (byte_order == 2)  {
						PDP_SWAB(&i);
						dem_a->x_gp_first = (double)i;
					}
				}
				/*
				 * Multiply the UTM coordinates by the scale factors,
				 * and add in the x and y origins.
				 * We don't need to do this for the UTM coordinates of the
				 * map corners because they are stored in real-number format
				 * rather than binary format.
				 *
				 * For some files, x_gp_first and y_gp_first are stored in real-number
				 * format also.  We handle those files below.
				 */
				dem_a->x_gp_first = x_scale_factor * dem_a->x_gp_first + x_origin;
				/*
				 * The USGS apparently didn't put in the location of the first
				 * elevation in the first profile, but rather just put in the location
				 * of the top left corner of the map area.  Thus, we need to round
				 * these two values to round multiples of dem_a->x_res and dem_a->y_res.
				 * The y value must be rounded down and the x value must be rounded up,
				 * so that the point will be inside the northwest corner of the map area.
				 */
				dem_a->x_gp_first = ceil(dem_a->x_gp_first / dem_a->x_res) * dem_a->x_res;
				need--;
			}
			else if ((strstr(subfield.format, "B") != (char *)0) && (strcmp(subfield.label, "Y") == 0))  {
				if (subfield.length != 4)  {
					/* Error */
					dem_a->y_gp_first = -1.0;
				}
				else  {
					i = (((int32_t)subfield.value[3] & 0xff) << 24) |
					    (((int32_t)subfield.value[2] & 0xff) << 16) |
					    (((int32_t)subfield.value[1] & 0xff) <<  8) |
					     ((int32_t)subfield.value[0] & 0xff);
					if (byte_order == 0)  {
						dem_a->y_gp_first = (double)i;
					}
					else if (byte_order == 1)  {
						LE_SWAB(&i);
						dem_a->y_gp_first = (double)i;
					}
					else if (byte_order == 2)  {
						PDP_SWAB(&i);
						dem_a->y_gp_first = (double)i;
					}
				}
				/*
				 * Multiply the UTM coordinates by the scale factors,
				 * and add in the x and y origins.
				 * We don't need to do this for the UTM coordinates of the
				 * map corners because they are stored in real-number format
				 * rather than binary format.
				 *
				 * For some files, x_gp_first and y_gp_first are stored in real-number
				 * format also.  We handle those files below.
				 */
				dem_a->y_gp_first = y_scale_factor * dem_a->y_gp_first + y_origin;
				/*
				 * The USGS apparently didn't put in the location of the first
				 * elevation in the first profile, but rather just put in the location
				 * of the top left corner of the map area.  Thus, we need to round
				 * these two values to round multiples of dem_a->x_res and dem_a->y_res.
				 * The y value must be rounded down and the x value must be rounded up,
				 * so that the point will be inside the northwest corner of the map area.
				 */
				dem_a->y_gp_first = floor(dem_a->y_gp_first / dem_a->y_res) * dem_a->y_res;
				need--;
			}
			/*
			 * In my experience, most files store X and Y as binary values.
			 * However, a few files use the "R" format, so we also check for it
			 * here.
			 */
			else if ((strstr(subfield.format, "R") != (char *)0) && (strcmp(subfield.label, "X") == 0))  {
				dem_a->x_gp_first = strtod(subfield.value, (char **)0);
				need--;
			}
			else if ((strstr(subfield.format, "R") != (char *)0) && (strcmp(subfield.label, "Y") == 0))  {
				dem_a->y_gp_first = strtod(subfield.value, (char **)0);
				need--;
			}
		}
		else if (strcmp(subfield.tag, "RSDF") == 0)  {
			if ((strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "OBRP") == 0))  {
				if ((subfield.length == 2) && (subfield.value[0] == 'G') && (subfield.value[1] == '2'))  {
					dem_a->elevation_pattern = 1;	// regular
				}
				else if ((subfield.length == 0) || (subfield.value[0] == ' '))  {
					dem_a->elevation_pattern = 2;	// random
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


	if ((dem_a->horizontal_datum == -1) || (dem_a->horizontal_datum == 1))  {
		/* The datum is NAD-27.  Initialize the parameters. */
		dem_datum->a = NAD27_SEMIMAJOR;
		dem_datum->b = NAD27_SEMIMINOR;
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

		fprintf(stderr, "Warning:  The DEM data aren't in a horizontal datum I currently handle.\n");
		fprintf(stderr, "Defaulting to NAD-27.  This may result in positional errors in the data.\n");
	}

	/*
	 * Convert the southeast UTM corner to latitude/longitude
	 */
	if (redfearn_inverse(dem_datum, dem_a->se_x_gp, dem_a->se_y_gp, dem_a->zone, &(dem_a->se_lat), &(dem_a->se_long)) != 0)  {
		fprintf(stderr, "refearn_inv() returns failure, (utm_x = %g, utm_y = %g, utm_zone = %d\n",
			dem_a->se_x_gp, dem_a->se_y_gp, dem_a->zone);
		exit(0);
	}



	/*
	 * The next file name we need is the DQHL module, which contains
	 * some quality information, including the source of the data.
	 */
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "dqhl.ddf", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "dqhl.ddf", 8);
		}
	}
	else  {
		if (gz_flag != 0)  {
			strncpy(&file_name[file_name_length - 11], "DQHL.DDF", 8);
		}
		else  {
			strncpy(&file_name[file_name_length - 8], "DQHL.DDF", 8);
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
	need = 2;
	record_id = -1;
	while (get_subfield(&subfield) != 0)  {
		if (strcmp(subfield.tag, "DQHL") == 0)  {
			if ((strstr(subfield.format, "I") != (char *)0) && (strcmp(subfield.label, "RCID") == 0))  {
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				record_id = strtol(subfield.value, (char **)0, 10);
				subfield.value[subfield.length] = save_byte;
			}
			else if ((record_id == 3) && (strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "COMT") == 0))  {
				dem_a->origin_code[0] = ' '; dem_a->origin_code[1] = ' '; dem_a->origin_code[2] = ' '; dem_a->origin_code[3] = ' ';
				save_byte = subfield.value[subfield.length]; subfield.value[subfield.length] = '\0';
				if (strncmp(subfield.value, "DEM PRODUCER:  Unspecified", 26) == 0)  {
					dem_a->origin_code[1] = ' '; dem_a->origin_code[2] = ' '; dem_a->origin_code[3] = ' ';
				}
				else if (strncmp(subfield.value, "DEM PRODUCER:  National M", 25) == 0)  {
					dem_a->origin_code[1] = 'N'; dem_a->origin_code[2] = 'M'; dem_a->origin_code[3] = 'D';
				}
				else if (strncmp(subfield.value, "DEM PRODUCER:  Eastern Ma", 25) == 0)  {
					dem_a->origin_code[1] = 'E'; dem_a->origin_code[2] = 'M'; dem_a->origin_code[3] = 'C';
				}
				else if (strncmp(subfield.value, "DEM PRODUCER:  Western Ma", 25) == 0)  {
					dem_a->origin_code[1] = 'W'; dem_a->origin_code[2] = 'M'; dem_a->origin_code[3] = 'C';
				}
				else if (strncmp(subfield.value, "DEM PRODUCER:  Mid-Contin", 25) == 0)  {
					dem_a->origin_code[0] = 'M'; dem_a->origin_code[1] = 'C'; dem_a->origin_code[2] = 'M'; dem_a->origin_code[3] = 'C';
				}
				else if (strncmp(subfield.value, "DEM PRODUCER:  Rocky Moun", 25) == 0)  {
					dem_a->origin_code[0] = 'R'; dem_a->origin_code[1] = 'M'; dem_a->origin_code[2] = 'M'; dem_a->origin_code[3] = 'C';
				}
				else if (strncmp(subfield.value, "DEM PRODUCER:  Forest Ser", 25) == 0)  {
					dem_a->origin_code[1] = ' '; dem_a->origin_code[2] = 'F'; dem_a->origin_code[3] = 'S';
				}
				else if (strncmp(subfield.value, "DEM PRODUCER:  Gestalt Ph", 25) == 0)  {
					dem_a->origin_code[0] = 'G'; dem_a->origin_code[1] = 'P'; dem_a->origin_code[2] = 'M'; dem_a->origin_code[3] = '2';
				}
				else if (strncmp(subfield.value, "DEM PRODUCER:  Contractor", 25) == 0)  {
					dem_a->origin_code[0] = 'C'; dem_a->origin_code[1] = 'O'; dem_a->origin_code[2] = 'N'; dem_a->origin_code[3] = 'T';
				}
				else if (strncmp(subfield.value, "DEM PRODUCER:  ", 15) == 0)  {
					if (subfield.length > 15)  {
						strncpy(&(dem_a->origin_code[4 - subfield.length >= 19 ? 4 : subfield.length - 15]),
							&subfield.value[15],
							subfield.length >= 19 ? 4 : subfield.length - 15);
					}
				}
				subfield.value[subfield.length] = save_byte;
				need--;
			}
			else if ((record_id == 2) && (strstr(subfield.format, "A") != (char *)0) && (strcmp(subfield.label, "COMT") == 0))  {
				if (subfield.length >= 14)  {
					dem_a->process_code = subfield.value[13] - '0';
				}
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

	return 0;
}




/*
 * In SDTS, all DEMs are stored as a rectangular grid.
 * Thus, we can use the same routine to process all DEMs,
 * whether they are in the UTM Planimetric Reference System,
 * or the Geographic Planimetric Reference System (latitude/longitude).
 * (Currently, we have no samples of geographic-style data.)
 *
 * This function returns 0 if it allocates memory and reads in the data.
 * It returns 1 if it doesn't allocate memory.
 */
int
process_dem_sdts(char *file_name, struct image_corners *image_corners,
		struct dem_corners *dem_corners, struct dem_record_type_a *dem_a, struct datum *dem_datum)
{
	int32_t i, j;
	union conv {
		uint32_t i;
		float f;
	} conv;
	short *sptr;
	int32_t dem_size_x, dem_size_y;
	int32_t byte_order;
	struct subfield subfield;
	int32_t get_ret;


	/* Begin by finding the native byte-order on this machine. */
	byte_order = swab_type();


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
	 */
	if (image_corners->sw_lat < image_corners->ne_lat)  {
		/* The user has specified image boundaries.  Check for overlap. */
		if ((dem_corners->sw_lat >= image_corners->ne_lat) || ((dem_corners->ne_lat) <= image_corners->sw_lat) ||
		    (dem_corners->sw_long >= image_corners->ne_long) || ((dem_corners->ne_long) <= image_corners->sw_long))  {
			return 1;
		}
	}

	dem_size_x = dem_a->cols;
	dem_size_y = dem_a->rows;

	/*
	 * Since SDTS DEMs are padded to make them rectangular, we con't have to worry about partial
	 * profiles, like we do with old-format DEMs.  Thus, we just read in the data, convert it into
	 * internal form, and store it in the array.
	 * Begin by allocating the memory array.
	 */
	dem_corners->ptr = (short *)malloc(sizeof(short) * dem_size_x * dem_size_y);
	if (dem_corners->ptr == (short *)0)  {
		fprintf(stderr, "malloc of dem_corners->ptr failed\n");
		exit(0);
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
	for (j = 0; j < dem_size_y; j++)  {
		while ((get_ret = get_subfield(&subfield)) != 0)  {
			/*
			 * Skip unwanted subfields at the beginning of the record.
			 */
			if (strcmp(subfield.tag, "CVLS") == 0)  {
				break;
			}
		}
		if (get_ret == 0)  {
			/* At end of file and we still haven't found what we need. */
			fprintf(stderr, "Ran out of data in file %s.  Ignoring file.\n", file_name);
			end_ddf();
			return 1;
		}
		for (i = 0; i < dem_size_x; i++)  {
			if ((strstr(subfield.format, "B") != (char *)0) && (strcmp(subfield.label, "ELEVATION") == 0))  {
				sptr = (dem_corners->ptr + j * dem_size_x + i);
				/*
				 * These values, rather than being stored in 'I' format (integer numbers),
				 * are stored in two's-complement binary.  Thus, they must be properly swabbed
				 * during conversion to internal form.
				 */
				if (subfield.length == 2)  {
					if (byte_order == 0)  {
						*sptr = (((int32_t)subfield.value[1] << 8) & 0x0000ff00) + ((int32_t)subfield.value[0] & 0x000000ff);
					}
					else  {
						*sptr = (((int32_t)subfield.value[0] << 8) & 0x0000ff00) + ((int32_t)subfield.value[1] & 0x000000ff);
					}
				}
				else if (subfield.length == 4)  {
					/*
					 * Note:  When the length is 4, we assume that this is a
					 * BFP32 value, which means that it is a raw binary IEEE 754
					 * floating point number.  Thus, this conversion won't work
					 * on machines where IEEE 754 is not the native floating point
					 * format.  We could convert from binary into the native floating
					 * point format the hard way, but it appears that most machines
					 * support IEEE 754, so we will try it this way for a while.
					 */
					conv.i = (((int32_t)subfield.value[3] & 0xff) << 24) |
						  (((int32_t)subfield.value[2] & 0xff) << 16) |
						  (((int32_t)subfield.value[1] & 0xff) <<  8) |
						   ((int32_t)subfield.value[0] & 0xff);
					if (byte_order == 0)  {
						/* Do nothing. */
					}
					else if (byte_order == 1)  {
						LE_SWAB(&conv.i);
					}
					else if (byte_order == 2)  {
						PDP_SWAB(&conv.i);
					}
					*sptr = drawmap_round(conv.f);
				}
				else  {
					/* Error */
					*sptr = HIGHEST_ELEVATION;
				}
				if (*sptr == dem_a->edge_fill)  {
					/* This is a point, along the edges of the quad, that doesn't contain valid data. */
					*sptr = HIGHEST_ELEVATION;
				}
				else if (*sptr == 32767)  {
					/*
					 * Some DEM files appear to mark invalid data points with 32767.
					 * I can think of two possible reasons for this; but these are just
					 * guesses, and the real reason may be entirely different.
					 * First guess:  it may have been a human data entry error, since the
					 * edge_fill value is normally -32767.  Second guess:  for a while, it
					 * may have been the standard was to use 32767 for an edge_fill marker in
					 * the original (non-SDTS) DEM files; and these values may have been carried
					 * over as part of the automated conversion process.  I don't know
					 * how these values got there, but they are clearly not valid elevations.
					 *
					 * One concern from all of this is that some of the original DEM files
					 * appear to contain either of 32767 and -32767 as non-valid data
					 * markers.  (Perhaps sometimes both, although I haven't located such a
					 * file yet.)  They may have been automatically carried over into the
					 * SDTS files during automated conversion.  For the 32767 value, this
					 * wouldn't appear to be a big problem, because we can still detect it.
					 * However, the -32767 value is identical to the normal SDTS void_fill
					 * value.  Thus, unless -32767 meant void_fill in the original DEM files,
					 * this value may be misinterpreted after the conversion to SDTS.
					 *
					 * We treat 32767 in the same way as the edge_fill marker, and convert
					 * it to HIGHEST_ELEVATION.
					 */
					*sptr = HIGHEST_ELEVATION;
				}
				else if (*sptr == dem_a->void_fill)  {
					/* This is a point, somewhere within the quad, that falls within a void in the data. */
					*sptr = 0;
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

				if (i == (dem_size_x - 1))  {
					break;
				}
			}

			if (get_subfield(&subfield) == 0)  {
				fprintf(stderr, "Shortage of data in %s.  Ignoring file.\n", file_name);
				end_ddf();
				return 1;
			}
			if (strcmp(subfield.tag, "CVLS") != 0)  {
				/* There weren't the expected number of elevations in the row. */
				fprintf(stderr, "Shortage of data in %s.  Ignoring file.\n", file_name);
				end_ddf();
				return 1;
			}
		}
	}
	/* We are done with this file, so close it. */
	end_ddf();


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


	dem_corners->x = dem_size_x;
	dem_corners->y = dem_size_y;
	dem_corners->x_gp_min = dem_a->x_gp_first;
	dem_corners->y_gp_min = dem_a->y_gp_first - ((double)dem_size_y - 1.0) * dem_a->y_res;
	dem_corners->x_gp_max = dem_a->x_gp_first + ((double)dem_size_x - 1.0) * dem_a->x_res;
	dem_corners->y_gp_max = dem_a->y_gp_first;

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
