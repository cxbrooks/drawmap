/*
 * =========================================================================
 * sdts_utils.h - Some global definitions for SDTS files.
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
 * This structure retrieves a subfield from a DDF file.
 */
struct subfield  {
	char *tag;
	char *label;
	char *value;
	char *format;
	int32_t length;
};


void print_ddr();
int32_t get_subfield(struct subfield *);
int32_t begin_ddf(char *);
void end_ddf();
