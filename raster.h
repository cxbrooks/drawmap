/*
 * =========================================================================
 * raster.h - A header file to define relevant portions of the SUN rasterfile format.
 * Copyright (c) 1997,2008  Fred M. Erickson
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

#define	MAGIC   0x59a66a95
#define STANDARD	1
#define EQUAL_RGB	1

struct rasterfile {
    int32_t magic;
    int32_t width;
    int32_t height;
    int32_t depth;
    int32_t length;
    int32_t type;
    int32_t maptype;
    int32_t maplength;
};
