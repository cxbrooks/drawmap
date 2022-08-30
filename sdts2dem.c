/*
 * =========================================================================
 * sdts2dem.c - A program to convert USGS SDTS DEM files to ordinary USGS DEM files.
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
#include "drawmap.h"
#include "dem.h"
#include "sdts_utils.h"



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





void gen_header(char *, struct dem_record_type_a *, struct datum *);


void
license(void)
{
	fprintf(stderr, "This program is free software; you can redistribute it and/or modify\n");
	fprintf(stderr, "it under the terms of the GNU General Public License as published by\n");
	fprintf(stderr, "the Free Software Foundation; either version 2, or (at your option)\n");
	fprintf(stderr, "any later version.\n\n");

	fprintf(stderr, "This program is distributed in the hope that it will be useful,\n");
	fprintf(stderr, "but WITHOUT ANY WARRANTY; without even the implied warranty of\n");
	fprintf(stderr, "MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the\n");
	fprintf(stderr, "GNU General Public License for more details.\n\n");

	fprintf(stderr, "You should have received a copy of the GNU General Public License\n");
	fprintf(stderr, "along with this program; if not, write to the Free Software\n");
	fprintf(stderr, "Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.\n");
}



int
main(int argc, char *argv[])
{
	int32_t i, j, k, l;
	union {
		uint32_t i;
		float f;
	} conv;
	int dem_fdesc;
	int output_fdesc;
	int32_t length;
	char buf[DEM_RECORD_LENGTH + 1];
	char output_file[12];
	int32_t gz_flag;
	ssize_t (*read_function)();
	int32_t byte_order;
	struct dem_record_type_a dem_a;
	struct dem_record_type_c dem_c;
	struct datum datum;
	char code1, code2;
	struct subfield subfield;
	int32_t get_ret;
	short *ptr, *sptr;
	int32_t num_elevs;
	double x, y;
	int32_t min_elev_prof, max_elev_prof;


	if ((argc == 2) && (argv[1][0] == '-') && (argv[1][1] == 'L'))  {
		license();
		exit(0);
	}
	if ((argc != 2) && (argc != 3))  {
		fprintf(stderr, "Usage:  %s ????CEL@.DDF [output_file_name]\n", argv[0]);
		fprintf(stderr, "        Where the ???? are alphanumeric characters, and @ represents a digit.\n");
		exit(0);
	}


	/*
	 * Find the byte order of this machine.
	 */
	byte_order = swab_type();


	/* Find file name length. */
	length = strlen(argv[1]);
	/* Find file name length. */
	length = strlen(argv[1]);
	if (length < 12)  {
		fprintf(stderr, "File name %s appears too short to be valid.  Should look like ????CEL@.DDF\n", argv[1]);
		exit(0);
	}

	/*
	 * Figure out if the file is gzip-compressed or not.
	 */
	if ((strcmp(&argv[1][length - 3], ".gz") == 0) ||
	    (strcmp(&argv[1][length - 3], ".GZ") == 0))  {
		gz_flag = 1;
		if ((dem_fdesc = buf_open_z(argv[1], O_RDONLY)) < 0)  {
			fprintf(stderr, "Can't open %s for reading, errno = %d\n", argv[1], errno);
			exit(0);
		}
		read_function = buf_read_z;
	}
	else  {
		gz_flag = 0;
		if ((dem_fdesc = buf_open(argv[1], O_RDONLY)) < 0)  {
			fprintf(stderr, "Can't open %s for reading, errno = %d\n", argv[1], errno);
			exit(0);
		}
		read_function = buf_read;
	}


	/*
	 * Files in Spatial Data Transfer System (SDTS) format are markedly
	 * different from the old DEM files.  (As a side note, there does not
	 * appear to be a specific name for the DEM format.  Most documents
	 * just call it DEM format, and use "SDTS DEM", or some equivalent
	 * when they refer to SDTS formatted files.  I usually just call it
	 * the ordinary DEM format.
	 *
	 * We insist that the user specify one, single, SDTS file
	 * on the command line.
	 * The file must be the one whose name has the form ????CEL?.DDF
	 * (or ????cel?.ddf), and it may have a .gz on the end if it is gzip
	 * compressed.
	 *
	 * We allow the files to be gzip-compressed, and they can have either
	 * ".gz" or ".GZ" on the end.  However, we insist that the rest of
	 * the file name have consistent case.  That is, if the 'F' or 'f'
	 * in the ".DDF" or ".ddf" is in a given case, the rest of the file
	 * had better be in that same case.
	 *
	 * If the following "if" test succeeds, we assume we have an SDTS file.
	 */
	if (((length >= 7) && (gz_flag != 0) &&
	     ((strncmp(&argv[1][length - 7], ".ddf", 4) == 0) ||
	      (strncmp(&argv[1][length - 7], ".DDF", 4) == 0))) ||
	    ((length >= 4) && (gz_flag == 0) &&
	     ((strcmp(&argv[1][length - 4], ".ddf") == 0) ||
	      (strcmp(&argv[1][length - 4], ".DDF") == 0))))  {
		/* SDTS file */

		/* Close the file.  We will reopen it in parse_dem_sdts(). */
		if (gz_flag == 0)  {
			buf_close(dem_fdesc);
		}
		else  {
			buf_close_z(dem_fdesc);
		}

		/*
		 * Check that the file name takes the form that we expect.
		 */
		if (((gz_flag != 0) &&
		     ((strncmp(&argv[1][length - 11], "ce", 2) != 0) &&
		      (strncmp(&argv[1][length - 11], "CE", 2) != 0))) ||
		    ((gz_flag == 0) &&
		     (strncmp(&argv[1][length - 8], "ce", 2) != 0) &&
		     (strncmp(&argv[1][length - 8], "CE", 2) != 0)))  {
			fprintf(stderr, "The file %s looks like an SDTS file, but the name doesn't look right.\n", argv[1]);
			exit(0);
		}

		/*
		 * The file name looks okay.  Let's launch into the information parsing.
		 */
		if (parse_dem_sdts(argv[1], &dem_a, &dem_c, &datum, gz_flag) != 0)  {
			exit(0);
		}
	}


	/*
	 * Print all of the parsed header data.
	 */
//	print_dem_a_c(&dem_a, &dem_c);


	/* Create the output file. */
	if (argc == 3)  {
		if ((output_fdesc = open(argv[2], O_WRONLY | O_CREAT | O_EXCL, 0644)) < 0)  {
			fprintf(stderr, "Can't create %s for writing, errno = %d\n", argv[2], errno);
			exit(0);
		}
	}
	else  {
		code1 = 'a' + floor((fabs(dem_a.se_lat) + (dem_a.se_lat < 0 ? -1.0 : 1.0) * 0.02 - floor(fabs(dem_a.se_lat) + (dem_a.se_lat < 0 ? -1.0 : 1.0) * 0.02)) * 8.0);
		code2 = '1' + floor((fabs(dem_a.se_long) + (dem_a.se_long < 0 ? 1.0 : -1.0) * 0.02 - floor(fabs(dem_a.se_long) + (dem_a.se_long < 0 ? 1.0 : -1.0) * 0.02)) * 8.0);
		sprintf(output_file, "%2.2d%3.3d%c%c.dem",
			(int)(fabs(dem_a.se_lat) + (dem_a.se_lat < 0 ? -1.0 : 1.0) * 0.02),
			(int)(fabs(dem_a.se_long) + (dem_a.se_long < 0 ? 1.0 : -1.0) * 0.02),
			code1, code2);
		if ((output_fdesc = open(output_file, O_WRONLY | O_CREAT | O_EXCL, 0644)) < 0)  {
			fprintf(stderr, "Can't create %s for writing, errno = %d\n", output_file, errno);
			exit(0);
		}
	}


	/*
	 * Okay.  Fill in the header and write it out to the new DEM file.
	 */
	gen_header(buf, &dem_a, &datum);
	if (write(output_fdesc, buf, DEM_RECORD_LENGTH) != DEM_RECORD_LENGTH)  {
		fprintf(stderr, "Failed to write header to DEM file.\n");
		exit(0);
	}



	/*
	 * Now.  All that remains is to read in the actual elevation data, convert
	 * it from west-to-east uniform-length profiles into south-to-north variable-length profiles
	 * and write it to the output file.
	 */


	/*
	 * Allocate the memory array to store the incoming data.
	 * Since we have to convert from west-to-east profiles into south-to-north
	 * profiles, we need to read in all of the data before we can output any.
	 */
	ptr = (short *)malloc(sizeof(short) * dem_a.cols * dem_a.rows);
	if (ptr == (short *)0)  {
		fprintf(stderr, "malloc of ptr failed\n");
		exit(0);
	}

	/*
	 * Open the file in preparation for parsing.
	 */
	if (begin_ddf(argv[1]) < 0)  {
		fprintf(stderr, "Can't open %s for reading, errno = %d\n", argv[1], errno);
		exit(0);
	}
	/*
	 * Loop through the subfields until we find all of the elevation data.
	 */
	for (j = 0; j < dem_a.rows; j++)  {
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
			fprintf(stderr, "Ran out of data in file %s.\n", argv[1]);
			end_ddf();
			exit(0);
		}
		for (i = 0; i < dem_a.cols; i++)  {
			if ((strstr(subfield.format, "B") != (char *)0) && (strcmp(subfield.label, "ELEVATION") == 0))  {
				sptr = (ptr + j * dem_a.cols + i);
				/*
				 * These values, rather than being stored in the expected 'I' format (integer numbers),
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
					*sptr = dem_a.void_fill;
				}

				if (i == (dem_a.cols - 1))  {
					break;
				}
			}

			if (get_subfield(&subfield) == 0)  {
				fprintf(stderr, "Shortage of data in %s.\n", argv[1]);
				end_ddf();
				exit(0);
			}
			if (strcmp(subfield.tag, "CVLS") != 0)  {
				/* There weren't the expected number of elevations in the row. */
				fprintf(stderr, "Shortage of data in %s.\n", argv[1]);
				end_ddf();
				exit(0);
			}
		}
	}
	/* We are done with this file, so close it. */
	end_ddf();


	/*
	 * Okay.  The array is full of data.  Now we need to
	 * write it out in DEM record format, and in south-to-north,
	 * west-to-east order.
	 */
	x = dem_a.x_gp_first - dem_a.x_res;
	for (j = 0; j < dem_a.cols; j++)  {
		x = x + dem_a.x_res;

		/*
		 * Write the profile header.
		 * First we need to find the minimum and maximum elevations for the profile,
		 * and the total number of elevations in the profile.
		 */
		min_elev_prof = 100000;
		max_elev_prof = -100000;
		num_elevs = 0;
		for (l = 0; l < dem_a.rows; l++)  {
			sptr = (ptr + l * dem_a.cols + j);

			/*
			 * Some files contain 32767 as a marker for non-valid
			 * data.  We take this opportunity to convert these values
			 * into dem_a.edge_fill so that the rest of the code
			 * doesn't have to deal with them.
			 *
			 * On second thought, one presumes that these values were
			 * present in the original DEM files, before they were
			 * converted to SDTS.  Thus, in the spirit of trying to
			 * produce a DEM file as much like the original as possible,
			 * we leave the values alone.
			 *
			 * There was also the additional problem that eliminating the
			 * 32767 values would sometimes totally elminate a profile from
			 * the DEM file, since num_elevs would be zero for a line with
			 * only 32767 values.  This would mean that the file contents
			 * would not jibe with the number of lines given in the global
			 * header at the top of the file.  We could write addtional code
			 * to deal with this, but it doesn't seem worth it to eliminate
			 * data values that were probably present in the original source
			 * file.
			 */
			//if (*sptr == 32767)  {
			//	*sptr = dem_a.edge_fill;
			//}

			if (*sptr != dem_a.edge_fill)  {
				num_elevs++;

				if (*sptr != dem_a.void_fill)  {
					if (min_elev_prof > *sptr)  {
						min_elev_prof = *sptr;
					}
					if (max_elev_prof < *sptr)  {
						max_elev_prof = *sptr;
					}
				}
			}
		}
		//if (num_elevs == 0)  {
		//	continue;
		//}
		y = dem_a.y_gp_first - (double)(dem_a.rows - 1) * dem_a.y_res;
		i = dem_a.rows - 1;
		while ((i >= 0) && (*(ptr + i * dem_a.cols + j) == dem_a.edge_fill))  {
			y = y + dem_a.y_res;
			i--;
		}
		sprintf(buf, "%6d%6d%6d%6d% 24.15E% 24.15E   0.0                  % 24.15E% 24.15E",
			1, j + 1, num_elevs, 1, x, y, (double)min_elev_prof, (double)max_elev_prof);
		for (k = 0; k < 144; k++)  {
			if (buf[k] == 'E')  buf[k] = 'D';	// USGS files use 'D' for exponentiation.
		}

		/*
		 * The header is ready to go.
		 * Now just pump out data until it is all gone.
		 */
		k = 144;
		for ( ; i >= 0; i--)  {
			sptr = ptr + i * dem_a.cols + j;
			if (*sptr == dem_a.edge_fill)  {
				break;
			}
			sprintf(&buf[k], "%6d", *sptr);
			k = k + 6;
			if ((k > (DEM_RECORD_LENGTH - 6)) || (i == 0))  {
				/* The record is full.  Write it out. */
				for ( ; k < DEM_RECORD_LENGTH; k++)  {
					buf[k] = ' ';
				}
				if (write(output_fdesc, buf, DEM_RECORD_LENGTH) != DEM_RECORD_LENGTH)  {
					fprintf(stderr, "Failed to write record to DEM file.\n");
					exit(0);
				}
				k = 0;
			}
		}
		if (k != 0)  {
			/* Write out a partial record, if necessary. */
			for ( ; k < DEM_RECORD_LENGTH; k++)  {
				buf[k] = ' ';
			}
			if (write(output_fdesc, buf, DEM_RECORD_LENGTH) != DEM_RECORD_LENGTH)  {
				fprintf(stderr, "Failed to write record to DEM file.\n");
				exit(0);
			}
		}
	}


	/*
	 * If we have data for a type C record, then output the record.
	 */
	if (dem_a.accuracy != 0)  {
		sprintf(buf, "%6d%6d%6d%6d%6d%6d%6d%6d%6d%6d",
			dem_c.datum_stats_flag, dem_c.datum_rmse_x, dem_c.datum_rmse_y,
			dem_c.datum_rmse_z, dem_c.datum_sample_size,
			dem_c.dem_stats_flag, dem_c.dem_rmse_x, dem_c.dem_rmse_y,
			dem_c.dem_rmse_z, dem_c.dem_sample_size);
		for (k =  60; k < DEM_RECORD_LENGTH; k++)  {
			buf[k] = ' ';
		}
		if (write(output_fdesc, buf, DEM_RECORD_LENGTH) != DEM_RECORD_LENGTH)  {
			fprintf(stderr, "Failed to write record to DEM file.\n");
			exit(0);
		}
	}


	close(output_fdesc);

	exit(0);
}




/*
 * Using the parameters from the dem_a structure, prepare an output DEM
 * file header in the classic format (the non-SDTS format).
 */
void
gen_header(char *buf, struct dem_record_type_a *dem_a, struct datum *datum)
{
	int32_t i;
	int32_t d, m;
	double s;

	/*
	 * Begin by putting a title at the start of the header.
	 * There is space at the start of the header for 40 bytes of title, followed
	 * by 40 bytes of free-form text, followed by 29 bytes of blank fill.
	 * Just fill the first 40 bytes with whatever is in the dem_a.title array,
	 * fill the free form area with blanks,
	 * and add the blank fill.  (The dem_a.title array contains all of the title
	 * text that we found in the *IDEN.DDF SDTS file.)
	 */
	sprintf(buf, "%-40.40s                                                                     ", dem_a->title);

	/*
	 * Now we need the southeast corner latitude/longitude in SDDDMMSS.SSSS format.
	 * Followed by the level code, elevation pattern, plane ref, and zone.
	 *
	 * Used to print out the seconds with 7.4f, which is what the standard
	 * says.  However, the last two digits should always be zero anyway,
	 * and quantization error can produce incorrect-looking results.  By
	 * setting the format to 5.2f, we round the data to more correct-looking
	 * values.
	 */
	decimal_degrees_to_dms(dem_a->se_long, &d, &m, &s);
	sprintf(&buf[109], "% 3d%2.2d%05.2f  ", d, m, s);
	decimal_degrees_to_dms(dem_a->se_lat, &d, &m, &s);
	sprintf(&buf[122], " % 2d%2.2d%05.2f  ", d, m, s);
	/*
	 * Byte 136 is a process code.
	 * Byte 137 is blank fill. Bytes 138-140 are a sectional indicator for
	 * 30-minute DEMs.  Until we find a sample 30-minute DEM, we
	 * will just make this field blank.
	 */
	sprintf(&buf[135], "%1.1d    ", dem_a->process_code);
	sprintf(&buf[140], "%-4.4s%6d%6d%6d%6d", dem_a->origin_code, dem_a->level_code,
		dem_a->elevation_pattern, dem_a->plane_ref, dem_a->zone);
	
	/*
	 * There are 360 bytes of projection parameters, which are unused.
	 * They are ordinarily set to 0.
	 */
	strcat(&buf[168], "   0.0                     0.0                     0.0                     0.0                     0.0                  ");
	strcat(&buf[288], "   0.0                     0.0                     0.0                     0.0                     0.0                  ");
	strcat(&buf[408], "   0.0                     0.0                     0.0                     0.0                     0.0                  ");

	/*
	 * Some more misc parameters:  plane units, elevation units, number of sides.
	 */
	sprintf(&buf[528], "%6d%6d%6d", dem_a->plane_units, dem_a->elev_units, 4);

	/*
	 * The next block of data contains the four corners of the DEM,
	 * in ground planimetric coordinates, followd by the minimum and
	 * maximum elevation.
	 * The USGS uses 'D' for exponentiation, rather than 'E', so change the 'E'
	 * characters to 'D'.  The USGS appears to always have the pre-decimal-point
	 * digit be a zero or a blank.  That is hard to do, so we won't bother.
	 */
	sprintf(&buf[546], "% 24.15E% 24.15E", dem_a->sw_x_gp, dem_a->sw_y_gp);
	sprintf(&buf[594], "% 24.15E% 24.15E", dem_a->nw_x_gp, dem_a->nw_y_gp);
	sprintf(&buf[642], "% 24.15E% 24.15E", dem_a->ne_x_gp, dem_a->ne_y_gp);
	sprintf(&buf[690], "% 24.15E% 24.15E", dem_a->se_x_gp, dem_a->se_y_gp);
	sprintf(&buf[738], "% 24.15E% 24.15E", (double)dem_a->min_elev, (double)dem_a->max_elev);
	for (i = 546; i < 786; i++)  {
		if (buf[i] == 'E')  buf[i] = 'D';	// USGS files use 'D' for exponentiation.
	}

	/*
	 * Now comes the angle between the reference system and the DEM.
	 * This should always be zero.
	 */
	sprintf(&buf[786], "   0.0                  ");

	/*
	 * The accuracy code.  Zero means unknown.
	 */
	sprintf(&buf[810], "%6d", dem_a->accuracy);

	/*
	 * Now comes a three-element array of spatial resolution units:  x, y, and z.
	 */
	sprintf(&buf[816], "%12.6E%12.6E%12.6E", dem_a->x_res, dem_a->y_res, dem_a->z_res);
	for (i = 816; i < 852; i++)  {
		if (buf[i] == 'E')  buf[i] = 'D';	// USGS files use 'D' for exponentiation.
	}

	/*
	 * Now we insert the columns/rows.  The number of columns is set to 1 at this
	 * point.  Each elevation profile contains the correct number of columns for
	 * that profile.  However, since we will be rotating the data by 90 degrees,
	 * we need to put in the columns value for what would otherwise be the row value.
	 */
	sprintf(&buf[852], "%6d%6d", 1, dem_a->cols);

	/*
	 * From this point on, the values are only defined if one is using the
	 * newer version of the DEM header.  We will leave them all blank, except
	 * that we will insert the horizontal and vertical datum information, since
	 * they are quite useful.
	 *
	 * We could also pry the other fields out of SDTS, but it is painful
	 * because they are generally embedded in long text strings.
	 */
	sprintf(&buf[864], "                        ");
	sprintf(&buf[888], "%2d", dem_a->vertical_datum);
	sprintf(&buf[890], "%2d", dem_a->horizontal_datum);
	sprintf(&buf[892], "                ");
	sprintf(&buf[908], "% 7.2f", dem_a->vertical_datum_shift);
	sprintf(&buf[915], "                                                                                                             ");
}
