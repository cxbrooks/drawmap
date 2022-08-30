/*
 * =========================================================================
 * big_buf_io_z - A library to allow efficient small reads and writes from gzipped files.
 * Copyright (c) 1997,2001,2008  Fred M. Erickson
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
 *
 *
 * Routines that let you do a lot of small reads from a gzip-compressed file,
 * without a lot of OS penalty.  Simply call buf_read_z() instead of read().
 * These routines read a file in large chunks and pass you the data in the
 * small chunks that you ask for.  Note some caveats:
 *
 *	You can't reset the file pointer with lseek() or you will
 *	mess up these routines.
 *
 *	You can only use these routines with one file at a time,
 *	since there is only one buffer to hold the data.
 *
 *      If you only have one file to read, you can simply open it
 *      and begin calling these routines.  If you have more than
 *      one file to read (consecutively, of course), you should use
 *      buf_open_z() to open the files, so that proper initialization
 *      gets done.  buf_close_z() has been added for completeness, but
 *      it doesn't do anything except call close().
 *
 * get_a_line_z() fills a buffer with information until it finds a newline,
 * or runs out of space.
 *
 * These routines depend on the zread() function, in the file gunzip.c
 */

#include <stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <string.h>
#include "gzip.h"

int buf_open_z(const char *, int);
// int buf_open_z(const char *, int, mode_t, ...);
int buf_close_z(int);
ssize_t buf_read_z(int, void *, size_t);
ssize_t get_a_line_z(int, void *, size_t);
int zread(int, char *, int, int);

#define BUF_SIZE  WSIZE		/* This MUST be at least as large as WSIZE, or zread() won't work properly */
static int32_t r_place = 0;
static int32_t r_size = 0;
static int32_t w_place = 0;
static int32_t new_flag = 1;




// int
// buf_open_z(const char *pathname, int flags, mode_t mode, ...)
int
buf_open_z(const char *pathname, int flags)
{
	r_place = 0;
	r_size = 0;
	w_place = 0;
	new_flag = 1;

	if (flags & O_CREAT)  {
//		return(open(pathname, flags, mode));
		return(open(pathname, flags, S_IRUSR | S_IWUSR | S_IRGRP));
	}
	else  {
		return(open(pathname, flags));
	}
}




int
buf_close_z(int fdesc)
{
	return(close(fdesc));
}




ssize_t
buf_read_z(int filedes, void *buf, size_t nbyte)
{
	static char bigbuf[BUF_SIZE];
	int32_t amount;
	int32_t tmp_nbyte;
	char *local_buf;

	local_buf = (char *)buf;

	tmp_nbyte = nbyte;

	while (tmp_nbyte > 0)  {
		if ((r_size <= 0) || (r_place == r_size))  {
			r_size = zread(filedes, bigbuf, BUF_SIZE, new_flag);
			if (r_size <= 0)  {
				return(r_size);
			}
			r_place = 0;
			new_flag = 0;
		}

		amount = (r_size - r_place) >= tmp_nbyte ? tmp_nbyte : r_size - r_place;
		memcpy(local_buf, &bigbuf[r_place], amount);
		local_buf = local_buf + amount;
		r_place = r_place + amount;
		tmp_nbyte = tmp_nbyte - amount;
	}

	return(nbyte);
}




ssize_t
get_a_line_z(int filedes, void *buf, size_t nbyte)
{
	int32_t i = 0;
	ssize_t ret_val;

	while (i < (int32_t)nbyte)  {
		ret_val = buf_read_z(filedes, (unsigned char *)buf + i, 1);
		if (ret_val < 0)  {
			return(ret_val);
		}
		else if (ret_val == 0)  {
			return((ssize_t)i);
		}

		if (*((unsigned char *)buf + i) == '\n')  {
			return(i + 1);
		}

		i++;
	}

	return((ssize_t)nbyte);
}
