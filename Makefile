# =========================================================================
# Makefile - Information for building drawmap, and associated programs.
# Copyright (c) 1997,1998,1999,2000,2008  Fred M. Erickson
#
# This program is free software; you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation; either version 2, or (at your option)
# any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
# =========================================================================



# If you want a copyright notice inserted into the image, then
# comment out the first version of NAME, and uncomment the
# second, and put your name inside the quotes.
NAME=\"\"
#NAME=\"Fred M. Erickson\"

CFLAGS = -O # -Wall



all: drawmap ll2utm utm2ll unblock_dlg unblock_dem llsearch sdts2dem sdts2dlg man

drawmap: drawmap.c dem.c dem_sdts.c dlg.c dlg_sdts.c sdts_utils.c big_buf_io.c big_buf_io_z.c gunzip.c \
	 utilities.c gtopo30.c gzip.h font_5x8.h font_6x10.h raster.h drawmap.h colors.h dlg.h dem.h sdts_utils.h
	$(CC) -DCOPYRIGHT_NAME="${NAME}" $(CFLAGS) -o drawmap drawmap.c dem.c dem_sdts.c dlg.c dlg_sdts.c \
		sdts_utils.c gtopo30.c big_buf_io.c big_buf_io_z.c gunzip.c utilities.c -lm

ll2utm: ll2utm.c utilities.c
	$(CC) $(CFLAGS) -o ll2utm ll2utm.c utilities.c -lm

utm2ll: utm2ll.c utilities.c
	$(CC) $(CFLAGS) -o utm2ll utm2ll.c utilities.c -lm

unblock_dlg: unblock_dlg.c
	$(CC) $(CFLAGS) -o unblock_dlg unblock_dlg.c

unblock_dem: unblock_dem.c
	$(CC) $(CFLAGS) -o unblock_dem unblock_dem.c

llsearch: llsearch.c big_buf_io.c utilities.c
	$(CC) $(CFLAGS) -o llsearch llsearch.c big_buf_io.c utilities.c -lm

sdts2dem: sdts2dem.c sdts_utils.c dem.c dem_sdts.c big_buf_io.c big_buf_io_z.c gunzip.c \
	 utilities.c gzip.h drawmap.h dem.h sdts_utils.h
	$(CC) $(CFLAGS) -o sdts2dem sdts2dem.c dem.c dem_sdts.c sdts_utils.c big_buf_io.c big_buf_io_z.c gunzip.c utilities.c -lm

sdts2dlg: sdts2dlg.c dlg.c dlg_sdts.c sdts_utils.c big_buf_io.c big_buf_io_z.c gunzip.c \
	 utilities.c gzip.h drawmap.h dlg.h sdts_utils.h
	$(CC) $(CFLAGS) -o sdts2dlg sdts2dlg.c dlg.c dlg_sdts.c sdts_utils.c big_buf_io.c big_buf_io_z.c gunzip.c utilities.c -lm

man: drawmap.1 ll2utm.1 utm2ll.1 llsearch.1 unblock_dlg.1 unblock_dem.1 sdts2dem.1 sdts2dlg.1

drawmap.1: drawmap.1n
	nroff -man drawmap.1n > drawmap.1

ll2utm.1: ll2utm.1n
	nroff -man ll2utm.1n > ll2utm.1

utm2ll.1: utm2ll.1n
	nroff -man utm2ll.1n > utm2ll.1

llsearch.1: llsearch.1n
	nroff -man llsearch.1n > llsearch.1

unblock_dlg.1: unblock_dlg.1n
	nroff -man unblock_dlg.1n > unblock_dlg.1

unblock_dem.1: unblock_dem.1n
	nroff -man unblock_dem.1n > unblock_dem.1

sdts2dem.1: sdts2dem.1n
	nroff -man sdts2dem.1n > sdts2dem.1

sdts2dlg.1: sdts2dlg.1n
	nroff -man sdts2dlg.1n > sdts2dlg.1

clean:
	rm -f drawmap ll2utm utm2ll unblock_dlg unblock_dem llsearch sdts2dem sdts2dlg \
		drawmap.1 ll2utm.1 utm2ll.1 llsearch.1 unblock_dlg.1 unblock_dem.1 sdts2dem.1 sdts2dlg.1 \
		drawmap.o dem.o dem_sdts.o dlg.o dlg_sdts.o sdts_utils.o big_buf_io.o \
		big_buf_io_z.o gunzip.o utilities.o ll2utm.o utm2ll.o unblock_dlg.o unblock_dem.o llsearch.o sdts2dem.o sdts2dlg.o

