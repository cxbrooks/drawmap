/*
 * =========================================================================
 * gtopo30.c - Routines to handle GTOPO30 data.
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

#include <stdint.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdio.h>
#include <math.h>
#include <errno.h>
#include <string.h>
#include "drawmap.h"
#include "dem.h"



int parse_gtopo30_hdr(char *, struct dem_corners *, struct dem_record_type_a *, struct datum *, int32_t *, int32_t *, int32_t *);




/*
 * Process a GTOPO30 file.  These files use the Geographic Planimetric
 * Reference System and have samples spaced by 30 arc-seconds.
 *
 * This function returns 0 if it allocates memory and reads in the data.
 * It returns 1 if it doesn't allocate memory.
 */
int
process_gtopo30(char *file_name, struct image_corners *image_corners,
		struct dem_corners *dem_corners, struct dem_record_type_a *dem_a, struct datum *dem_datum, int32_t info_flag)
{
	int32_t i, j;
	int32_t j_size;
	ssize_t ret_val;
	int32_t nbytes;
	int32_t nodata;
	double lat_low, lat_high, long_low, long_high;
	int32_t i_low, j_low;
	int32_t i_high, j_high;
	int32_t min_elev = 100000000, max_elev = -100000000;
	int32_t length;
	int fdesc_in;
	int32_t gz_flag;
	ssize_t (*read_function)();
	char *unswabbed;
	short *swabbed;
	int32_t byte_order;
	int32_t upper_case_flag;
	char tmp_file_name[MAX_FILE_NAME];
	double lat_tmp;



	/*
	 * find the native byte order of this machine.
	 */
	byte_order = swab_type();


	/*
	 * Parse the GTOPO30 HDR file.
	 */
	if (parse_gtopo30_hdr(file_name, dem_corners, dem_a, dem_datum, &nbytes, &nodata, &gz_flag) != 0)  {
		/* If there was a failure, the error message was printed by parse_gtopo30_hdr(). */
		return 1;
	}


	/*
	 * If the DEM data don't overlap the image, then ignore them.
	 *
	 * If the user didn't specify latitude/longitude ranges for the image,
	 * then we simply use this DEM to determine those boundaries.  In this
	 * latter case, no overlap check is necessary (or possible) since the
	 * image boundaries will be determined later.
	 *
	 * The GTOPO30 data generally cover very large areas.
	 * Because of this, we don't pass back an array of the
	 * entire GTOPO30 elevation data.  Rather, we only pass
	 * back the portion that falls inside the image boundaries.
	 * If there were no image boundaries specified, then we
	 * pass back the whole gigantic thing.
	 */
	if ((info_flag == 0) && (image_corners->sw_lat < image_corners->ne_lat))  {
		/* The user has specified image boundaries.  Check for overlap. */
		if ((dem_corners->sw_lat >= image_corners->ne_lat) || ((dem_corners->nw_lat) <= image_corners->sw_lat) ||
		    (dem_corners->sw_long >= image_corners->ne_long) || ((dem_corners->se_long) <= image_corners->sw_long))  {
			return 1;
		}

		if (dem_corners->sw_lat < image_corners->sw_lat)  {
			lat_low = image_corners->sw_lat;
		}
		else  {
			lat_low = dem_corners->sw_lat;
		}
		if (dem_corners->nw_lat > image_corners->ne_lat)  {
			lat_high = image_corners->ne_lat;
		}
		else  {
			lat_high = dem_corners->nw_lat;
		}
		if (dem_corners->sw_long < image_corners->sw_long)  {
			long_low = image_corners->sw_long;
		}
		else  {
			long_low = dem_corners->sw_long;
		}
		if (dem_corners->ne_long > image_corners->ne_long)  {
			long_high = image_corners->ne_long;
		}
		else  {
			long_high = dem_corners->ne_long;
		}
	}
	else  {
		lat_low = dem_corners->sw_lat;
		lat_high = dem_corners->nw_lat;
		long_low = dem_corners->sw_long;
		long_high = dem_corners->ne_long;
	}


	/*
	 * Make a copy of the file name.  The one we were originally
	 * given is still stored in the command line arguments.
	 * It is probably a good idea not to alter those, lest we
	 * scribble something we don't want to scribble.
	 */
	strncpy(tmp_file_name, file_name, MAX_FILE_NAME - 1);
	tmp_file_name[MAX_FILE_NAME - 1] = '\0';
	if ((length = strlen(tmp_file_name)) < 5)  {
		/*
		 * Excluding the initial path, the file name should have the form
		 * *.HDR, perhaps with a ".gz" on the end.  If it isn't
		 * at least long enough to have this form, then reject it.
		 */
		fprintf(stderr, "File name %s doesn't look right.\n", tmp_file_name);
		return 1;
	}
	/* Check the case of the characters in the file name by examining a single character. */
	if (gz_flag == 0)  {
		if (tmp_file_name[length - 1] == 'r')  {
			upper_case_flag = 0;
		}
		else  {
			upper_case_flag = 1;
		}
	}
	else  {
		if (tmp_file_name[length - 4] == 'r')  {
			upper_case_flag = 0;
		}
		else  {
			upper_case_flag = 1;
		}
	}


	/*
	 * We need to modify the file name from *.HDR to *.DEM.
	 */
	if (upper_case_flag == 0)  {
		if (gz_flag != 0)  {
			strncpy(&tmp_file_name[length - 6], "dem", 3);
		}
		else  {
			strncpy(&tmp_file_name[length - 3], "dem", 3);
		}
	}
	else  {
		if (gz_flag != 0)  {
			strncpy(&tmp_file_name[length - 6], "DEM", 3);
		}
		else  {
			strncpy(&tmp_file_name[length - 3], "DEM", 3);
		}
	}

	/*
	 * Open DEM file.
	 */
	length = strlen(tmp_file_name);
	if ((strcmp(&tmp_file_name[length - 3], ".gz") == 0) || (strcmp(&tmp_file_name[length - 3], ".GZ") == 0))  {
		gz_flag = 1;
		if ((fdesc_in = buf_open_z(tmp_file_name, O_RDONLY)) < 0)  {
			fprintf(stderr, "Can't open %s for reading, errno = %d\n", tmp_file_name, errno);
			exit(0);
		}
		read_function = buf_read_z;
	}
	else  {
		gz_flag = 0;
		if ((fdesc_in = buf_open(tmp_file_name, O_RDONLY)) < 0)  {
			fprintf(stderr, "Can't open %s for reading, errno = %d\n", tmp_file_name, errno);
			exit(0);
		}
		read_function = buf_read;
	}


	/*
	 * Now we need to read in the GTOPO30 data, and build an
	 * array of elevation samples to return to the caller.
	 *
	 * First we need some space to read in the GTOPO30 DEM elevations,
	 * and a buffer to put the data after getting the byte-order correct.
	 */
	unswabbed = (char *)malloc(nbytes * dem_corners->x);
	if (unswabbed == (char *)0)  {
		fprintf(stderr, "malloc of unswabbed failed\n");
		exit(0);
	}
	swabbed = (short *)malloc(nbytes * dem_corners->x);
	if (swabbed == (short *)0)  {
		fprintf(stderr, "malloc of swabbed failed\n");
		exit(0);
	}

	/*
	 * Now we read in a row of data, get it into the correct byte order,
	 * and stuff it into the array.
	 *
	 * i_low and i_high give us the lowest and (highest+1) "i" values in
	 * the GTOPO30 source array that get copied into our output block.
	 * Ditto for j_low and j_high.
	 *
	 * In the dem_corners->ptr array, the first index runs from the highest
	 * latitude for index=0 to the lowest latitude for index=?.  The second
	 * index runs from the lowest longitude for index=0 to the highest
	 * longitude for index=?.  Remember that both longitudes and latitudes
	 * are signed, so that 111W is less than 110W, and 45S is less than 44S.
	 *
	 * We fill the array with an extra sample in each direction, just like
	 * the 1-degree DEMs, which have 1201 by 1201 samples.
	 *
	 * Note that the first sample, in the northwest corner, is offset inside
	 * the latitude/longitude boundaries by half of one sample width.  This
	 * is because they produced the data by combining a bunch of surrounding
	 * samples and then put the data point smack dab in the middle of the sample
	 * cell.  We ignore this, and just move each sample to the northwest corner
	 * of the sample cell.  (Actually, this movement was done for us during our
	 * call to parse_gtopo30_hdr().)  We could do some kind of fancy interpolation
	 * to get a more "correct" value for the corner of the sample cell; or we
	 * could change the latitude/longitude corners of the data to correspond
	 * exactly to the locations of the samples.  However, neither of these
	 * things seems worthwhile for the relatively low resolution GTOPO30
	 * data.
	 */
	i_low = drawmap_round((double)dem_corners->y * (dem_corners->nw_lat - lat_high) / (dem_corners->nw_lat - dem_corners->se_lat));
	i_high = drawmap_round((double)dem_corners->y * (dem_corners->nw_lat - lat_low) / (dem_corners->nw_lat - dem_corners->se_lat));
	j_low = drawmap_round((double)dem_corners->x * (long_low - dem_corners->nw_long) / (dem_corners->se_long - dem_corners->nw_long));
	j_high = drawmap_round((double)dem_corners->x * (long_high - dem_corners->nw_long) / (dem_corners->se_long - dem_corners->nw_long));

	/*
	 * malloc the space for the data array we will pass back.
	 */
	j_size = j_high - j_low + 1;
	dem_corners->ptr = (short *)malloc(nbytes * (i_high - i_low + 1) * j_size);
	if (dem_corners->ptr == (short *)0)  {
		fprintf(stderr, "malloc of dem_corners->ptr failed\n");
		exit(0);
	}

	for (i = 0; i < dem_corners->y; i++) {
		/*
		 * Read in the data, and convert it into an array of properly-byte-ordered
		 * short integers.
		 */
		if ((ret_val = read_function(fdesc_in, unswabbed, nbytes * dem_corners->x)) != (nbytes * dem_corners->x))  {
			fprintf(stderr, "Read failure on DEM file.  ret_val = %d\n", (int)ret_val);
			exit(0);
		}
		if (i < i_low)  {
			continue;
		}
		for (j = 0; j < dem_corners->x; j++)  {
			if (nbytes == 1)  {
				swabbed[j] = 0x00ff & (short)unswabbed[j];
			}
			else  {
				swabbed[j] = 0x00ff & (short)unswabbed[j];
				if (byte_order == 0)  {
					swabbed[j] = (((short)unswabbed[(j << 1) + 1] << 8) & 0xff00) + ((short)unswabbed[j << 1] & 0x00ff);
				}
				else  {
					swabbed[j] = (((short)unswabbed[j << 1] << 8) & 0xff00) + ((short)unswabbed[(j << 1) + 1] & 0x00ff);
				}
				/*
				 * Sub-sea-level areas may be filled with a flag number instead of
				 * elevations.  If so, then set the elevation to zero.
				 */
				if (swabbed[j] == nodata)  {
					swabbed[j] = 0;
				}
			}
		}

		/*
		 * transfer the data into the dem_corners->ptr array.
		 */
		if (i <= (i_high - 1))  {
			for (j = j_low; j < j_high; j++)  {

// Debugging code
//if (((i - i_low) < 0) || ((i - i_low) > (i_high - i_low - 1)))  {
//fprintf(stderr, "i index out: %d       i=%d  j=%d i_low=%d i_high=%d j_low=%d j_high=%d k=%d l=%d\n", i - i_low, i, j, i_low, i_high, j_low, j_high, k, l);
//}
//if (((j - j_low) < 0) || ((j - j_low) > (j_high - j_low - 1)))  {
//fprintf(stderr, "j index out: %d       i=%d  j=%d i_low=%d i_high=%d j_low=%d j_high=%d k=%d l=%d\n", j - j_low, i, j, i_low, i_high, j_low, j_high, k, l);
//}
//if (((i - i_low) == 0) || ((i - i_low) == (i_high - i_low - 1)))  {
//fprintf(stderr, "i_index %d touches boundary\n", i - i_low);
//}
//if (((j - j_low) == 0) || ((j - j_low) == (j_high - j_low - 1)))  {
//fprintf(stderr, "j_index %d touches boundary\n", j - j_low);
//}

				*(dem_corners->ptr + j_size * (i - i_low) + j - j_low) = swabbed[j];
				if (swabbed[j] > max_elev)  {
					max_elev = swabbed[j];
				}
				if (swabbed[j] < min_elev)  {
					min_elev = swabbed[j];
				}
			}
			/*
			 * We still have the last column of the array unfilled.
			 * If there is additional data available adjacent to the region
			 * of interest, then fill this column from there.  Otherwise
			 * we will fill it later by duplicating the second-to-last column.
			 */
			if (j_high < dem_corners->x)  {

// Debugging code
//if (((i - i_low) < 0) || ((i - i_low) > (i_high - i_low - 1)))  {
//fprintf(stderr, "i index out: %d       i=%d  j=%d i_low=%d i_high=%d j_low=%d j_high=%d k=%d l=%d\n", i - i_low, i, j, i_low, i_high, j_low, j_high, k, l);
//}
//if (((i - i_low) == 0) || ((i - i_low) == (i_high - i_low - 1)))  {
//fprintf(stderr, "i_index %d touches boundary\n", i - i_low);
//}

				*(dem_corners->ptr + j_size * (i - i_low) + j_high - j_low) = swabbed[j_high];
				if (swabbed[j_high] > max_elev)  {
					max_elev = swabbed[j_high];
				}
				if (swabbed[j_high] < min_elev)  {
					min_elev = swabbed[j_high];
				}
			}
		}
		if ((i == (i_high - 1)) && (i_high >= dem_corners->y))  {
			break;
		}
		if (i == i_high)  {
			/*
			 * We still have the last row of the array unfilled.
			 * If there is additional data available adjacent to the region
			 * of interest, then fill this row from there.  Otherwise
			 * we will fill it later by duplicating the second-to-last row.
			 */
			for (j = j_low; j < j_high; j++)  {

// Debugging code
//if (((j - j_low) < 0) || ((j - j_low) > (j_high - j_low - 1)))  {
//fprintf(stderr, "j index out: %d       i=%d  j=%d i_low=%d i_high=%d j_low=%d j_high=%d k=%d l=%d\n", j - j_low, i, j, i_low, i_high, j_low, j_high, k, l);
//}
//if (((j - j_low) == 0) || ((j - j_low) == (j_high - j_low - 1)))  {
//fprintf(stderr, "j_index %d touches boundary\n", j - j_low);
//}

				*(dem_corners->ptr + j_size * (i_high - i_low) + j - j_low) = swabbed[j];
				if (swabbed[j] > max_elev)  {
					max_elev = swabbed[j];
				}
				if (swabbed[j] < min_elev)  {
					min_elev = swabbed[j];
				}
			}
			/*
			 * Now check that last little corner sample in the last column and last row.
			 */
			if (j_high < dem_corners->x)  {
				*(dem_corners->ptr + j_size * (i_high - i_low) + j_high - j_low) = swabbed[j_high];
				if (swabbed[j_high] > max_elev)  {
					max_elev = swabbed[j_high];
				}
				if (swabbed[j_high] < min_elev)  {
					min_elev = swabbed[j_high];
				}
			}
			break;
		}
	}
	free(swabbed);
	free(unswabbed);

	/*
	 * At this point the array is filled in, except for the rag-tag extra row and
	 * extra column that fills it out to even latitude/longitude.
	 * If there was data available above i_high and j_high,
	 * then we have already filled these extra slots from there.
	 * If not, then we now fill them up with data from the adjacent row/column.
	 */
	if (j_high >= dem_corners->x)  {
		for (i = 0; i < (i_high - i_low + 1); i++)  {
			*(dem_corners->ptr + j_size * i + j_high - j_low) = *(dem_corners->ptr + j_size * i + j_high - j_low - 1);
		}
	}
	if (i_high >= dem_corners->y)  {
		for (j = 0; j < (j_high - j_low + 1); j++)  {
			*(dem_corners->ptr + j_size * (i_high - i_low) + j) = *(dem_corners->ptr + j_size * (i_high - i_low - 1) + j);
		}
	}


	/*
	 * We saved the maximum and minimum elevations for the
	 * entire 1-degree block.  We need to put these into the dem_a structure.
	 */
	dem_a->max_elev = max_elev;
	dem_a->min_elev = min_elev;

	/*
	 * If (info_flag != 0)
	 * then we return unaltered information in dem_corners.
	 * Otherwise, we need to adjust dem_corners to reflect
	 * what we actually stored in dem_corners->ptr.
	 */
	if (info_flag == 0)  {
		/*
		 * Redfearn's formulas aren't happy when the latitude becomes exactly -90 or 90.
		 * twiddle them slightly for these special cases.
		 */
		if (lat_high == 90.0)  {
			lat_tmp = 89.999;
		}
		else  {
			lat_tmp = lat_high;
		}
		if (redfearn(dem_datum, &(dem_corners->nw_x_gp), &(dem_corners->nw_y_gp), &(dem_a->zone), lat_tmp, long_low, 0) != 0)  {
			fprintf(stderr, "call to redfearn() fails.\n");
			free(dem_corners->ptr);
			return 1;
		}
		if (redfearn(dem_datum, &(dem_corners->ne_x_gp), &(dem_corners->ne_y_gp), &(dem_a->zone), lat_tmp, long_high, 0) != 0)  {
			fprintf(stderr, "call to redfearn() fails.\n");
			free(dem_corners->ptr);
			return 1;
		}
		if (lat_low == -90.0)  {
			lat_tmp = -89.999;
		}
		else  {
			lat_tmp = lat_low;
		}
		if (redfearn(dem_datum, &(dem_corners->sw_x_gp), &(dem_corners->sw_y_gp), &(dem_a->zone), lat_tmp, long_low, 0) != 0)  {
			fprintf(stderr, "call to redfearn() fails.\n");
			free(dem_corners->ptr);
			return 1;
		}
		if (redfearn(dem_datum, &(dem_corners->se_x_gp), &(dem_corners->se_y_gp), &(dem_a->zone), lat_tmp, long_high, 0) != 0)  {
			fprintf(stderr, "call to redfearn() fails.\n");
			free(dem_corners->ptr);
			return 1;
		}

		dem_corners->sw_lat  = lat_low;
		dem_corners->sw_long = long_low;
		dem_corners->nw_lat  = lat_high;
		dem_corners->nw_long = long_low;
		dem_corners->ne_lat  = lat_high;
		dem_corners->ne_long = long_high;
		dem_corners->se_lat  = lat_low;
		dem_corners->se_long = long_high;

		dem_corners->x = j_size;
		dem_corners->y = i_high - i_low + 1;
	}


	/*
	 * Close all open files.
	 */
	if (gz_flag == 0)  {
		buf_close(fdesc_in);
	}
	else  {
		buf_close_z(fdesc_in);
	}


	return 0;
}





/*
 * This routine parses relevant data from a GTOPO30 ".HDR" file
 * and inserts the converted data into the given dem_a storage structure.
 */
int
parse_gtopo30_hdr(char *file_name, struct dem_corners *dem_corners, struct dem_record_type_a *dem_a, struct datum *dem_datum, int32_t *nbytes, int32_t *nodata, int32_t *gz_flag)
{
	int32_t i, j;
	int32_t length;
	int fdesc_in;
	ssize_t (*read_function)();
	char buf[DEM_RECORD_LENGTH + 1];	// Add 1 for the null terminator produced by sprintf().
	ssize_t ret_val;
	int32_t nrows = -1;	// Typical value:   6000
	int32_t ncols = -1;	// Typical value:   4800
	int32_t nbands = -1;	// Typical value:      1
	int32_t nbits = -1;	// Typical value:      8
	int32_t bandrowbytes = -1;	// Typical value:   4800
	int32_t totalrowbytes = -1;// Typical value:   4800
	int32_t bandgapbytes = -1;	// Typical value:      0
	double ulxmap = -181.0;	// Typical value:    -99.99583333333334
	double ulymap = -91.0;	// Typical value:     39.99583333333333
	double xdim = -100.0;	// Typical value:      0.00833333333333
	double ydim = -100.0;	// Typical value:      0.00833333333333
	double se_lat;		// latitude of southeast corner of data
	double se_long;		// longitude of southeast corner of data
	int32_t key_index, key_end;
	int32_t value_index, value_end;
	double lat_tmp;

	*nbytes = -1;
	*nodata = -9999;


	/*
	 * Open the header file.
	 */
	length = strlen(file_name);
	if ((strcmp(&file_name[length - 3], ".gz") == 0) || (strcmp(&file_name[length - 3], ".GZ") == 0))  {
		*gz_flag = 1;
		if ((fdesc_in = buf_open_z(file_name, O_RDONLY)) < 0)  {
			fprintf(stderr, "Can't open %s for reading, errno = %d\n", file_name, errno);
			exit(0);
		}
		read_function = get_a_line_z;
	}
	else  {
		*gz_flag = 0;
		if ((fdesc_in = buf_open(file_name, O_RDONLY)) < 0)  {
			fprintf(stderr, "Can't open %s for reading, errno = %d\n", file_name, errno);
			exit(0);
		}
		read_function = get_a_line;
	}



	/*
	 * Read and parse each line of the HDR file.
	 */
	while ((ret_val = read_function(fdesc_in, buf, DEM_RECORD_LENGTH)) > 0)  {
		/* Strip off any leading white space.  There shouldn't be any, but... */
		for (key_index = 0; key_index < ret_val; key_index++)  {
			if ((buf[key_index] != ' ') && (buf[key_index] != '\t'))  {
				break;
			}
		}
		/* Find the end of the key. */
		for (key_end = key_index; key_end < ret_val; key_end++)  {
			if ((buf[key_end] == '\n') || (buf[key_end] == '\r') ||
			    (buf[key_end] == ' ') || (buf[key_end] == '\t'))  {
				break;
			}
		}
		if ((key_end == ret_val) || (buf[key_end] == '\n') || (buf[key_end] == '\r'))  {
			fprintf(stderr, "Line:  \"%.*s\" does not contain a keyword/value pair.  Ignoring.", (int)ret_val, buf);
			continue;
		}
		/* Find the beginning of the value. */
		for (value_index = key_end; value_index < ret_val; value_index++)  {
			if ((buf[value_index] != ' ') && (buf[value_index] != '\t') &&
			    (buf[value_index] != '\n') && (buf[value_index] != '\r'))  {
				break;
			}
		}
		if (value_index == ret_val)  {
			fprintf(stderr, "Warning:  Line \"%.*s\" does not contain a keyword/value pair.  Ignoring.", (int)ret_val, buf);
			continue;
		}
		/* Find the end of the value. */
		for (value_end = value_index; value_end < ret_val; value_end++)  {
			if ((buf[value_end] == '\n') || (buf[value_end] == '\r') ||
			    (buf[value_end] == ' ') || (buf[value_end] == '\t'))  {
				break;
			}
		}

		/* Null-terminate both the key and the value. */
		buf[key_end] = '\0';
		buf[value_end] = '\0';

		/*
		 * Search for the key in the list of known keys.
		 *
		 * Note:  The comments describing each keyword are
		 * copied straight out of a gtopo30 README.TXT file.
		 */
		if (strcmp(&buf[key_index], "BYTEORDER") == 0)  {
			/*
			 * BYTEORDER      byte order in which image pixel values are stored
			 * M = Motorola byte order (most significant byte first)
			 */
			if ((buf[value_index] != 'M') || (buf[value_index + 1] != '\0'))  {
				fprintf(stderr, "Warning:  Unrecognized BYTEORDER (%s).  M is assumed.\n", &buf[value_index]);
			}
		}
		else if (strcmp(&buf[key_index], "LAYOUT") == 0)  {
			/*
			 * LAYOUT         organization of the bands in the file
			 * BIL = band interleaved by line (note: the source map is
			 * a single band image)
			 */
			if (strcmp(&buf[value_index], "BIL") != 0)  {
				fprintf(stderr, "Warning:  Unrecognized LAYOUT code (%s).  BIL is assumed.\n", &buf[value_index]);
			}
		}
		else if (strcmp(&buf[key_index], "NROWS") == 0)  {
			/*
			 * NROWS          number of rows in the image
			 */
			nrows = strtol(&buf[value_index], (char **)0, 10);
		}
		else if (strcmp(&buf[key_index], "NCOLS") == 0)  {
			/*
			 * NCOLS          number of columns in the image
			 */
			ncols = strtol(&buf[value_index], (char **)0, 10);
		}
		else if (strcmp(&buf[key_index], "NBANDS") == 0)  {
			/*
			 * NBANDS         number of spectral bands in the image (1 for the source map)
			 */
			nbands = strtol(&buf[value_index], (char **)0, 10);
			if (nbands != 1)  {
				fprintf(stderr, "NBANDS value (%d) is not 1.  Can't handle it.\n", nbands);
				return 1;
			}
		}
		else if (strcmp(&buf[key_index], "NBITS") == 0)  {
			/*
			 * NBITS          number of bits per pixel (8 for the source map)
			 */
			nbits = strtol(&buf[value_index], (char **)0, 10);
			if (nbits & 0x7)  {
				fprintf(stderr, "NBITS value (%d) not divisible by 8.  Can't handle it.\n", nbits);
				return 1;
			}
			*nbytes = nbits >> 3;
		}
		else if (strcmp(&buf[key_index], "BANDROWBYTES") == 0)  {
			/*
			 * BANDROWBYTES   number of bytes per band per row (the number of columns for
			 * an 8-bit source map)
			 */
			bandrowbytes = strtol(&buf[value_index], (char **)0, 10);
		}
		else if (strcmp(&buf[key_index], "TOTALROWBYTES") == 0)  {
			/*
			 * TOTALROWBYTES  total number of bytes of data per row (the number of columns
			 * for a single band 8-bit source map)
			 */
			totalrowbytes = strtol(&buf[value_index], (char **)0, 10);
		}
		else if (strcmp(&buf[key_index], "BANDGAPBYTES") == 0)  {
			/*
			 * BANDGAPBYTES   the number of bytes between bands in a BSQ format image
			 * (0 for the source map)
			 */
			bandgapbytes = strtol(&buf[value_index], (char **)0, 10);
			if (bandgapbytes != 0)  {
				fprintf(stderr, "BANDGAPBYTES value (%d) is not zero.  Can't handle it.\n", bandgapbytes);
				return 1;
			}
		}
		else if (strcmp(&buf[key_index], "NODATA") == 0)  {
			/*
			 * NODATA         value used for masking purposes
			 */
			*nodata = strtol(&buf[value_index], (char **)0, 10);
			if (*nodata > 0)  {
				fprintf(stderr, "Warning:  NODATA value (%d) is greater than zero.  This may not be correct.\n", *nodata);
			}
		}
		else if (strcmp(&buf[key_index], "ULXMAP") == 0)  {
			/*
			 * ULXMAP         longitude of the center of the upper-left pixel (decimal degrees)
			 */
			ulxmap = strtod(&buf[value_index], (char **)0);
		}
		else if (strcmp(&buf[key_index], "ULYMAP") == 0)  {
			/*
			 * ULYMAP         latitude  of the center of the upper-left pixel (decimal degrees)
			 */
			ulymap = strtod(&buf[value_index], (char **)0);
		}
		else if (strcmp(&buf[key_index], "XDIM") == 0)  {
			/*
			 * XDIM           x dimension of a pixel in geographic units (decimal degrees)
			 */
			xdim = strtod(&buf[value_index], (char **)0);
		}
		else if (strcmp(&buf[key_index], "YDIM") == 0)  {
			/*
			 * YDIM           y dimension of a pixel in geographic units (decimal degrees)
			 */
			ydim = strtod(&buf[value_index], (char **)0);
		}
		else  {
			/*
			 * During debugging we print out any unknown keywords.
			 * For production use, we don't.
			 */
			// fprintf(stderr, "Warning:  Unknown keyword ignored:  \"%s\".\n", &buf[key_index]);
		}
	}
	if (*gz_flag == 0)  {
		buf_close(fdesc_in);
	}
	else  {
		buf_close_z(fdesc_in);
	}

	/*
	 * Do a few more sanity checks on the parameters.
	 */
	if (nrows <= 0)  {
		fprintf(stderr, "NROWS value (%d) doesn't make sense.\n", nrows);
		return 1;
	}
	if (ncols <= 0)  {
		fprintf(stderr, "NCOLS value (%d) doesn't make sense.\n", ncols);
		return 1;
	}
	if ((*nbytes != 1) && (*nbytes != 2))  {
		fprintf(stderr, "NBITS value must be 8 or 16.  Can't deal with %d.\n", nbits);
		return 1;
	}
	if ((bandrowbytes >= 0) && (bandrowbytes != (*nbytes * ncols)))  {
		fprintf(stderr, "BANDROWBYTES value (%d) doesn't equal NBITS * NCOLS / 8.  Can't handle it.\n", bandrowbytes);
		return 1;
	}
	if ((totalrowbytes >= 0) && (totalrowbytes != (*nbytes * ncols)))  {
		fprintf(stderr, "TOTALROWBYTES value (%d) doesn't equal NBITS * NCOLS / 8.  Can't handle it.\n", totalrowbytes);
		return 1;
	}

	/*
	 * ulxmap and ulymap are offset so that the first sample is xdim/2 and ydim/2 in
	 * from the northwest corner.  Move them back to round-numbered values.
	 */
	ulxmap = ulxmap - xdim / 2.0;
	ulymap = ulymap + ydim / 2.0;
	if ((ulxmap < -180.001) || (ulxmap > 180.001))  {
		fprintf(stderr, "ULXMAP value (%g) is not in the range [-180, 180].\n", ulxmap);
		return 1;
	}
	if ((ulymap < -90.0001) || (ulymap > 90.0001))  {
		fprintf(stderr, "ULYMAP value (%g) is not in the range [-90, 90].\n", ulymap);
		return 1;
	}

	/*
	 * Find the latitude/longitude of the southeast corner.
	 * We will need these.
	 */
	se_lat = (double)drawmap_round(ulymap - ydim * (double)nrows);
	se_long = (double)drawmap_round(ulxmap + xdim * (double)ncols);


	/*
	 * All of the data from the header is processed.
	 * Now fill the passed structures with data and return.
	 */
	i = strlen(file_name);
	for (j = i - 1; j >= 0; j--)  {
		if (file_name[j] == '/')  {
			j++;
			break;
		}
	}
	if (j < 0)  {
		j = 0;
	}
	if ((i - j) > 40)  {
		strcpy(dem_a->title, "GTOPO30 data");
	}
	else  {
		strcpy(dem_a->title, &file_name[j]);
	}
	dem_a->level_code = 0;
	dem_a->plane_ref = 3;
	dem_a->plane_units = 3;
	dem_a->elev_units = 2;
	dem_a->min_elev = 100000.0;
	dem_a->max_elev = -100000.0;
	dem_a->angle = 0.0;
	dem_a->accuracy = 0;
	dem_a->x_res = 30.0;
	dem_a->y_res = 30.0;
	dem_a->z_res = 1.0;
	dem_a->cols = ncols;
	dem_a->rows = nrows;
	dem_a->horizontal_datum = 3;

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

	/*
	 * Redfearn's formulas aren't happy when the latitude becomes exactly -90 or 90.
	 * twiddle them slightly for these special cases.
	 */
	if (ulymap == 90.0)  {
		lat_tmp = 89.999;
	}
	else if (ulymap == -90.0)  {
		lat_tmp = -89.999;
	}
	else  {
		lat_tmp = ulymap;
	}
	if (redfearn(dem_datum, &(dem_a->nw_x_gp), &(dem_a->nw_y_gp), &(dem_a->zone), lat_tmp, ulxmap, 0) != 0)  {
		fprintf(stderr, "call to redfearn() fails.\n");
		return 1;
	}
	if (redfearn(dem_datum, &(dem_a->ne_x_gp), &(dem_a->ne_y_gp), &(dem_a->zone), lat_tmp, se_long, 0) != 0)  {
		fprintf(stderr, "call to redfearn() fails.\n");
		return 1;
	}
	if (se_lat == 90.0)  {
		lat_tmp = 89.999;
	}
	else if (se_lat == -90.0)  {
		lat_tmp = -89.999;
	}
	else  {
		lat_tmp = se_lat;
	}
	if (redfearn(dem_datum, &(dem_a->sw_x_gp), &(dem_a->sw_y_gp), &(dem_a->zone), lat_tmp, ulxmap, 0) != 0)  {
		fprintf(stderr, "call to redfearn() fails.\n");
		return 1;
	}
	if (redfearn(dem_datum, &(dem_a->se_x_gp), &(dem_a->se_y_gp), &(dem_a->zone), lat_tmp, se_long, 0) != 0)  {
		fprintf(stderr, "call to redfearn() fails.\n");
		return 1;
	}

	dem_corners->sw_x_gp = dem_a->sw_x_gp;
	dem_corners->sw_y_gp = dem_a->sw_y_gp;
	dem_corners->nw_x_gp = dem_a->nw_x_gp;
	dem_corners->nw_y_gp = dem_a->nw_y_gp;
	dem_corners->ne_x_gp = dem_a->ne_x_gp;
	dem_corners->ne_y_gp = dem_a->ne_y_gp;
	dem_corners->se_x_gp = dem_a->se_x_gp;
	dem_corners->se_y_gp = dem_a->se_y_gp;

	dem_corners->sw_lat  = se_lat;
	dem_corners->sw_long = ulxmap;
	dem_corners->nw_lat  = ulymap;
	dem_corners->nw_long = ulxmap;
	dem_corners->ne_lat  = ulymap;
	dem_corners->ne_long = se_long;
	dem_corners->se_lat  = se_lat;
	dem_corners->se_long = se_long;

	dem_corners->x = ncols;
	dem_corners->y = nrows;


	return 0;
}
