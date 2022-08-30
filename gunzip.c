/*
 * gunzip - a library allowing programs to directly read gzipped files.
 * Copyright (c) 1997,1998,2008  Fred M. Erickson
 *
 * This software is a stripped-down modification of the standard
 * gzip 1.2.4 distribution.  The original distribution contains
 * the following copyright information:
 *
 * =========================================================================
 * Copyright (C) 1992-1993 Jean-loup Gailly
 * The unzip code was written and put in the public domain by Mark Adler.
 * Portions of the lzw code are derived from the public domain 'compress'
 * written by Spencer Thomas, Joe Orost, James Woods, Jim McKie, Steve Davies,
 * Ken Turkowski, Dave Mack and Peter Jannesen.
 *
 * See the licensing information below, and the file COPYING, for the software license.
 *
 *  Copyright (C) 1992-1993 Jean-loup Gailly
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2, or (at your option)
 *  any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 * =========================================================================
 *
 * The GNU General Public License applies equally to this modified version.
 *
 *
 *
 *
 * This file contains incredible ugliness.  In August 1997,
 * I extracted code from gzip version 1.2.4, and twisted
 * it into a surreal new shape so that it can be used
 * as a library to unzip files.
 *
 * To use it, one simply calls zread(), which takes the
 * same arguments as read(), except that there is an extra argument
 * to signal the start of reading a new file, and except that it
 * returns uncompressed data from a gzip file.
 *
 * Note that a call to zread() MUST request at least as many bytes
 * as WSIZE, or this crude gob of duct tape and baling wire will
 * catastrophically fail.  Thus, it is usually best to use zread() by
 * going through the routines in big_buf_io_z.c
 *
 * I took code from gzip.h, gzip.c, unzip.c, inflate.c, and util.c,
 * (and maybe other files that I have forgotten).
 * All files except for gzip.h have been pulled into this
 * single file.
 * The modifications consisted first of extracting only the
 * gzip code that is relevant for decoding that subset of the
 * various file types that is encountered in files actually
 * produced by gzip.  In other words, this library won't decode
 * things like the ".Z" files created by "compress" (basically because
 * I didn't care about them, not because it was inherently difficult
 * to handle them).
 * Furthermore, I have made no effort to verify that it
 * unzips every possible type of file that it ostensibly supports.
 *
 * Once I had the extracted code, I went in and added brute-force
 * code to make the various inflation functions re-entrant.
 * Not pretty.  Be afraid.  Be very afraid.
 *
 * I did this because I was writing an application with large data
 * requirements, and didn't like the idea of doing 100 fork-exec
 * operations to set up 100 pipes to funnel 100 files through
 * 100 actual running copies of gzip.  I also did it to find out if
 * it could be done.  It turned out to be surprisingly easy, as long
 * as I didn't worry about maintainability or robustness or efficiency.
 */

/* ======================= parts of gzip.c, with additional code ======== */
#include <stdint.h>
#include <stdlib.h>
#include <unistd.h>
#include "gzip.h"
#include <setjmp.h>

#define MAX_PATH_LEN 1024
#define RECORD_IO 0
int32_t last_member;      /* set for .zip and .Z files */
int32_t force = 0;
int32_t list = 0;         /* list the file contents (-l) */
int32_t header_bytes;   /* number of bytes in gzip header */
int32_t verbose = 0;      /* be verbose (-v) */

#ifndef MAKE_LEGAL_NAME
#  ifdef NO_MULTIPLE_DOTS
#    define MAKE_LEGAL_NAME(name)   make_simple_name(name)
#  else
#    define MAKE_LEGAL_NAME(name)
#  endif
#endif

#define get_char() get_byte()

int32_t ifd, ofd;
uint32_t insize;           /* valid bytes in inbuf */
uint32_t inptr;            /* index of next byte to be processed in inbuf */
uint32_t outcnt;           /* bytes in output buffer */
unsigned char inbuf[INBUFSIZ +INBUF_EXTRA];
unsigned char outbuf[OUTBUFSIZ+OUTBUF_EXTRA];
int32_t to_stdout = 0;     /* output to stdout (-c) */
int32_t quiet = 0;         /* be very quiet (-q) */
int32_t test = 0;          /* test .gz file integrity */
char *progname;            /* program name */
int32_t method = DEFLATED; /* compression method */
int32_t exit_code = OK;    /* program exit code */
int32_t (*work) OF((int32_t infile, int32_t outfile)) = unzip; /* function to call */
int32_t part_nb;           /* number of parts in .gz file */
int32_t time_stamp;        /* original time stamp (modification time) */
int32_t ifile_size;        /* input file size, -1 for devices (debug only) */

int32_t bytes_in;          /* number of input bytes */
int32_t bytes_out;         /* number of output bytes */
char ifname[MAX_PATH_LEN]; /* input file name */
char ofname[MAX_PATH_LEN]; /* output file name */
unsigned char window[WSIZE];
local int32_t get_method(int32_t);

int32_t z_control;
int32_t ofd = -1;

jmp_buf	global_env;

char	*global_buf;
int32_t	global_count;


/* Test routine.  unzips a file from stdin onto stdout. */
/* main()
 * {
 *	int32_t ret_val;
 *	char my_buf[WSIZE];
 *
 *	while ((ret_val = zread(0, my_buf, WSIZE)) > 0)  {
 *		write(1, my_buf, ret_val);
 *	}
 *}
 */

int32_t
zread(int32_t fd, char *buf, int32_t length, int32_t new_flag)
{

	ifd = fd;

	if (new_flag != 0)  {
		z_control = 0;
		strcpy(ifname, "<file name unavailable>");
		strcpy(ofname, "<file name unavailable>");
		time_stamp = 0; /* time unknown by default */
		ifile_size = -1L; /* convention for unknown size */
		clear_bufs(); /* clear input and output buffers */
		to_stdout = 1;
		part_nb = 1;	/* Assume one part.  If there are more, they will be ignored. */
		method = get_method(ifd);
		if (method < 0) {
			exit(0);
		}
		if (work != unzip)  {
			fprintf(stderr, "Gzipped file of un-handle-able type.\n");
			exit(0);
		}
	}
/*	if ((*work)(ifd, ofd) != OK) return; */

/*	read(ifd, inbuf, INBUFSIZ); */

	global_buf = buf;
	global_count = length;

	if (setjmp(global_env) == 0)  {
		unzip(ifd, ofd);
	}

	return global_count;
}


RETSIGTYPE
abort_gzip(void)
{
	exit(0);
}


/* ========================================================================
 * Check the magic number of the input file and update ofname if an
 * original name was given and to_stdout is not set.
 * Return the compression method, -1 for error, -2 for warning.
 * Set inptr to the offset of the next byte to be processed.
 * Updates time_stamp if there is one and --no-time is not used.
 * This function may be called repeatedly for an input file consisting
 * of several contiguous gzip'ed members.
 * IN assertions: there is at least one remaining compressed member.
 *   If the member is a zip file, it must be the only one.
 */
local int32_t get_method(in)
    int32_t in;        /* input file descriptor */
{
    uch flags;     /* compression flags */
    char magic[2]; /* magic header */
    ulg stamp;     /* time stamp */

    /* If --force and --stdout, zcat == cat, so do not complain about
     * premature end of file: use try_byte instead of get_byte.
     */
    if (force && to_stdout) {
	magic[0] = (char)try_byte();
	magic[1] = (char)try_byte();
	/* If try_byte returned EOF, magic[1] == 0xff */
    } else {
	magic[0] = (char)get_byte();
	magic[1] = (char)get_byte();
    }
    method = -1;                 /* unknown yet */
    part_nb++;                   /* number of parts in gzip file */
    header_bytes = 0;
    last_member = RECORD_IO;
    /* assume multiple members in gzip file except for record oriented I/O */

    if (memcmp(magic, GZIP_MAGIC, 2) == 0
        || memcmp(magic, OLD_GZIP_MAGIC, 2) == 0) {

	method = (int32_t)get_byte();
	if (method != DEFLATED) {
	    fprintf(stderr,
		    "%s: %s: unknown method %d -- get newer version of gzip\n",
		    progname, ifname, method);
	    exit_code = ERROR;
	    return -1;
	}
	work = unzip;
	flags  = (uch)get_byte();

	if ((flags & ENCRYPTED) != 0) {
	    fprintf(stderr,
		    "%s: %s is encrypted -- get newer version of gzip\n",
		    progname, ifname);
	    exit_code = ERROR;
	    return -1;
	}
	if ((flags & CONTINUATION) != 0) {
	    fprintf(stderr,
	   "%s: %s is a a multi-part gzip file -- get newer version of gzip\n",
		    progname, ifname);
	    exit_code = ERROR;
	    if (force <= 1) return -1;
	}
	if ((flags & RESERVED) != 0) {
	    fprintf(stderr,
		    "%s: %s has flags 0x%x -- get newer version of gzip\n",
		    progname, ifname, flags);
	    exit_code = ERROR;
	    if (force <= 1) return -1;
	}
	stamp  = (ulg)get_byte();
	stamp |= ((ulg)get_byte()) << 8;
	stamp |= ((ulg)get_byte()) << 16;
	stamp |= ((ulg)get_byte()) << 24;
	if (stamp != 0 /* && !no_time */) time_stamp = stamp;

	(void)get_byte();  /* Ignore extra flags for the moment */
	(void)get_byte();  /* Ignore OS type for the moment */

	if ((flags & CONTINUATION) != 0) {
	    uint32_t part = (uint32_t)get_byte();
	    part |= ((uint32_t)get_byte())<<8;
	    if (verbose) {
		fprintf(stderr,"%s: %s: part number %u\n",
			progname, ifname, part);
	    }
	}
	if ((flags & EXTRA_FIELD) != 0) {
	    uint32_t len = (uint32_t)get_byte();
	    len |= ((uint32_t)get_byte())<<8;
	    if (verbose) {
		fprintf(stderr,"%s: %s: extra field of %u bytes ignored\n",
			progname, ifname, len);
	    }
	    while (len--) (void)get_byte();
	}

	/* Get original file name if it was truncated */
	if ((flags & ORIG_NAME) != 0) {
	    if (/* no_name || */ (to_stdout && !list) || part_nb > 1) {
		/* Discard the old name */
		char c; /* dummy used for NeXTstep 3.0 cc optimizer bug */
		do {c=get_byte();} while (c != 0);
	    } else {
		/* Copy the base name. Keep a directory prefix intact. */
                char *p = local_basename(ofname);
                char *base = p;
		for (;;) {
		    *p = (char)get_char();
		    if (*p++ == '\0') break;
		    if (p >= ofname+sizeof(ofname)) {
			error("corrupted input -- file name too large");
		    }
		}
                /* If necessary, adapt the name to local OS conventions: */
                if (!list) {
                   MAKE_LEGAL_NAME(base);
		   if (base) list=0; /* avoid warning about unused variable */
                }
	    } /* no_name || to_stdout */
	} /* ORIG_NAME */

	/* Discard file comment if any */
	if ((flags & COMMENT) != 0) {
	    while (get_char() != 0) /* null */ ;
	}
	if (part_nb == 1) {
	    header_bytes = inptr + 2*sizeof(int32_t); /* include crc and size */
	}

    } else if (memcmp(magic, PKZIP_MAGIC, 2) == 0 && inptr == 2
	    && memcmp((char*)inbuf, PKZIP_MAGIC, 4) == 0) {
	/* To simplify the code, we support a zip file when alone only.
         * We are thus guaranteed that the entire local header fits in inbuf.
         */
        inptr = 0;
	work = unzip;
	if (check_zipfile(in) != OK) return -1;
	/* check_zipfile may get ofname from the local header */
	last_member = 1;

    }
/*    else if (memcmp(magic, PACK_MAGIC, 2) == 0) {
 *	work = unpack;
 *	method = PACKED;
 *
 *    }
 *    else if (memcmp(magic, LZW_MAGIC, 2) == 0) {
 *	work = unlzw;
 *	method = COMPRESSED;
 *	last_member = 1;
 *
 *    } else if (memcmp(magic, LZH_MAGIC, 2) == 0) {
 *	work = unlzh;
 *	method = LZHED;
 *	last_member = 1;
 *
 *    }
 */
    else if (force && to_stdout && !list) { /* pass input unchanged */
	method = STORED;
	work = copy;
        inptr = 0;
	last_member = 1;
    }
    if (method >= 0) return method;

    if (part_nb == 1) {
	fprintf(stderr, "\n%s: %s: not in gzip format\n", progname, ifname);
	exit_code = ERROR;
	return -1;
    } else {
	WARN((stderr, "\n%s: %s: decompression OK, trailing garbage ignored\n",
	      progname, ifname));
	return -2;
    }
}

/* =========================== unzip.c, with modifications ========== */
/* unzip.c -- decompress files in gzip or pkzip format.
 * Copyright (C) 1992-1993 Jean-loup Gailly
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 *
 * The code in this file is derived from the file funzip.c written
 * and put in the public domain by Mark Adler.
 */

/*
   This version can extract files in gzip or pkzip format.
   For the latter, only the first entry is extracted, and it has to be
   either deflated or stored.
 */

#ifdef RCSID
static char rcsid[] = "$Id: unzip.c,v 0.13 1993/06/10 13:29:00 jloup Exp $";
#endif

/* #include "tailor.h" */
#include "gzip.h"
/* #include "crypt.h" */
#define RAND_HEAD_LEN  12  /* length of encryption random header */

extern int32_t z_control;
extern int32_t global_count;

/* PKZIP header definitions */
#define LOCSIG 0x04034b50L      /* four-byte lead-in (lsb first) */
#define LOCFLG 6                /* offset of bit flag */
#define  CRPFLG 1               /*  bit for encrypted entry */
#define  EXTFLG 8               /*  bit for extended local header */
#define LOCHOW 8                /* offset of compression method */
#define LOCTIM 10               /* file mod time (for decryption) */
#define LOCCRC 14               /* offset of crc */
#define LOCSIZ 18               /* offset of compressed size */
#define LOCLEN 22               /* offset of uncompressed length */
#define LOCFIL 26               /* offset of file name field length */
#define LOCEXT 28               /* offset of extra field length */
#define LOCHDR 30               /* size of local header, including sig */
#define EXTHDR 16               /* size of extended local header, inc sig */


/* Globals */

int32_t decrypt;        /* flag to turn on decryption */
char *key;              /* not used--needed to link crypt.c */
int32_t pkzip = 0;      /* set for a pkzip file */
int32_t ext_header = 0; /* set if extended local header */

/* ===========================================================================
 * Check zip file and advance inptr to the start of the compressed data.
 * Get ofname from the local header if necessary.
 */
int32_t check_zipfile(in)
    int32_t in;   /* input file descriptors */
{
    uch *h = inbuf + inptr; /* first local header */

    ifd = in;

    /* Check validity of local header, and skip name and extra fields */
    inptr += LOCHDR + SH(h + LOCFIL) + SH(h + LOCEXT);

    if (inptr > insize || LG(h) != LOCSIG) {
	fprintf(stderr, "\n%s: %s: not a valid zip file\n",
		progname, ifname);
	exit_code = ERROR;
	return ERROR;
    }
    method = h[LOCHOW];
    if (method != STORED && method != DEFLATED) {
	fprintf(stderr,
		"\n%s: %s: first entry not deflated or stored -- use unzip\n",
		progname, ifname);
	exit_code = ERROR;
	return ERROR;
    }

    /* If entry encrypted, decrypt and validate encryption header */
    if ((decrypt = h[LOCFLG] & CRPFLG) != 0) {
	fprintf(stderr, "\n%s: %s: encrypted file -- use unzip\n",
		progname, ifname);
	exit_code = ERROR;
	return ERROR;
    }

    /* Save flags for unzip() */
    ext_header = (h[LOCFLG] & EXTFLG) != 0;
    pkzip = 1;

    /* Get ofname and time stamp from local header (to be done) */
    return OK;
}

/* ===========================================================================
 * Unzip in to out.  This routine works on both gzip and pkzip files.
 *
 * IN assertions: the buffer inbuf contains already the beginning of
 *   the compressed data, from offsets inptr to insize-1 included.
 *   The magic header has already been checked. The output buffer is cleared.
 */
int32_t unzip(in, out)
    int32_t in, out;   /* input and output file descriptors */
{
    static ulg orig_crc = 0;           /* original crc */
    static ulg orig_len = 0;           /* original uncompressed length */
    static int32_t n = -100000000;     /* Bogus initializer to expose errors. */
    static uch buf[EXTHDR];            /* extended local header */
    static int32_t res;
    
    if (z_control == 5)  {
	goto LABEL5;
    } else if (z_control == 6)  {
    	goto LABEL6;
    }
    else if ((z_control & 0xf) != 0)  {
    	goto LABEL7;
    }
    else  {
	z_control = 0;
    	orig_crc = 0;
    	orig_len = 0;
    }

    updcrc(NULL, 0);           /* initialize crc */

    if (pkzip && !ext_header) {  /* crc and length at the end otherwise */
	orig_crc = LG(inbuf + LOCCRC);
	orig_len = LG(inbuf + LOCLEN);
    }

    /* Decompress */
    if (method == DEFLATED)  {

LABEL7:
	res = inflate();
	global_count = 0;

	if (res == 3) {
	    error("out of memory");
	} else if (res != 0) {
	    error("invalid compressed data--format violated");
	}

    } else if (pkzip && method == STORED) {

	/* register ulg */ n = LG(inbuf + LOCLEN);

	if (n != LG(inbuf + LOCSIZ) - (decrypt ? RAND_HEAD_LEN : 0)) {

	    fprintf(stderr, "len %d, siz %d\n", n, LG(inbuf + LOCSIZ));
	    error("invalid compressed data--length mismatch");
	}
	while (n--) {
	    uch c = (uch)get_byte();
#ifdef CRYPT
	    if (decrypt) zdecode(c);
#endif
/*	    put_ubyte(c); */
window[outcnt++]=(uch)(c);
if (outcnt==WSIZE)  {
z_control = 6;
flush_window();
}
LABEL6:
z_control = 0;
	}
	z_control = 5;
	flush_window();
LABEL5:
z_control = 0;
    } else {
	error("internal error, invalid method");
    }

    /* Get the crc and original length */
    if (!pkzip) {
        /* crc32  (see algorithm.doc)
	 * uncompressed input size modulo 2^32
         */
	for (n = 0; n < 8; n++) {
	    buf[n] = (uch)get_byte(); /* may cause an error if EOF */
	}
	orig_crc = LG(buf);
	orig_len = LG(buf+4);

    } else if (ext_header) {  /* If extended header, check it */
	/* signature - 4bytes: 0x50 0x4b 0x07 0x08
	 * CRC-32 value
         * compressed size 4-bytes
         * uncompressed size 4-bytes
	 */
	for (n = 0; n < EXTHDR; n++) {
	    buf[n] = (uch)get_byte(); /* may cause an error if EOF */
	}
	orig_crc = LG(buf+4);
	orig_len = LG(buf+12);
    }

    /* Validate decompression */
    if (orig_crc != updcrc(outbuf, 0)) {
	error("invalid compressed data--crc error");
    }
    if (orig_len != (ulg)bytes_out) {
	error("invalid compressed data--length error");
    }

    /* Check if there are more entries in a pkzip file */
    if (pkzip && inptr + 4 < insize && LG(inbuf+inptr) == LOCSIG) {
	if (to_stdout) {
	    WARN((stderr,
		  "%s: %s has more than one entry--rest ignored\n",
		  progname, ifname));
	} else {
	    /* Don't destroy the input zip file */
	    fprintf(stderr,
		    "%s: %s has more than one entry -- unchanged\n",
		    progname, ifname);
	    exit_code = ERROR;
	    ext_header = pkzip = 0;
	    return ERROR;
	}
    }
    ext_header = pkzip = 0; /* for next file */
    return OK;
}


/* =========================== inflate.c, with modifications ========== */
/* inflate.c -- Not copyrighted 1992 by Mark Adler
   version c10p1, 10 January 1993 */

/* You can do whatever you like with this source file, though I would
   prefer that if you modify it and redistribute it that you include
   comments to that effect with your name and the date.  Thank you.
   [The history has been moved to the file ChangeLog.]
 */

/*
   Inflate deflated (PKZIP's method 8 compressed) data.  The compression
   method searches for as much of the current string of bytes (up to a
   length of 258) in the previous 32K bytes.  If it doesn't find any
   matches (of at least length 3), it codes the next byte.  Otherwise, it
   codes the length of the matched string and its distance backwards from
   the current position.  There is a single Huffman code that codes both
   single bytes (called "literals") and match lengths.  A second Huffman
   code codes the distance information, which follows a length code.  Each
   length or distance code actually represents a base value and a number
   of "extra" (sometimes zero) bits to get to add to the base value.  At
   the end of each deflated block is a special end-of-block (EOB) literal/
   length code.  The decoding process is basically: get a literal/length
   code; if EOB then done; if a literal, emit the decoded byte; if a
   length then get the distance and emit the referred-to bytes from the
   sliding window of previously emitted data.

   There are (currently) three kinds of inflate blocks: stored, fixed, and
   dynamic.  The compressor deals with some chunk of data at a time, and
   decides which method to use on a chunk-by-chunk basis.  A chunk might
   typically be 32K or 64K.  If the chunk is uncompressible, then the
   "stored" method is used.  In this case, the bytes are simply stored as
   is, eight bits per byte, with none of the above coding.  The bytes are
   preceded by a count, since there is no longer an EOB code.

   If the data is compressible, then either the fixed or dynamic methods
   are used.  In the dynamic method, the compressed data is preceded by
   an encoding of the literal/length and distance Huffman codes that are
   to be used to decode this block.  The representation is itself Huffman
   coded, and so is preceded by a description of that code.  These code
   descriptions take up a little space, and so for small blocks, there is
   a predefined set of codes, called the fixed codes.  The fixed method is
   used if the block codes up smaller that way (usually for quite small
   chunks), otherwise the dynamic method is used.  In the latter case, the
   codes are customized to the probabilities in the current block, and so
   can code it much better than the pre-determined fixed codes.
 
   The Huffman codes themselves are decoded using a mutli-level table
   lookup, in order to maximize the speed of decoding plus the speed of
   building the decoding tables.  See the comments below that precede the
   lbits and dbits tuning parameters.
 */


/*
   Notes beyond the 1.93a appnote.txt:

   1. Distance pointers never point before the beginning of the output
      stream.
   2. Distance pointers can point back across blocks, up to 32k away.
   3. There is an implied maximum of 7 bits for the bit length table and
      15 bits for the actual data.
   4. If only one code exists, then it is encoded using one bit.  (Zero
      would be more efficient, but perhaps a little confusing.)  If two
      codes exist, they are coded using one bit each (0 and 1).
   5. There is no way of sending zero distance codes--a dummy must be
      sent if there are none.  (History: a pre 2.0 version of PKZIP would
      store blocks with no distance codes, but this was discovered to be
      too harsh a criterion.)  Valid only for 1.93a.  2.04c does allow
      zero distance codes, which is sent as one code of zero bits in
      length.
   6. There are up to 286 literal/length codes.  Code 256 represents the
      end-of-block.  Note however that the static length tree defines
      288 codes just to fill out the Huffman codes.  Codes 286 and 287
      cannot be used though, since there is no length base or extra bits
      defined for them.  Similarly, there are up to 30 distance codes.
      However, static trees define 32 codes (all 5 bits) to fill out the
      Huffman codes, but the last two had better not show up in the data.
   7. Unzip can check dynamic Huffman blocks for complete code sets.
      The exception is that a single code would not be complete (see #4).
   8. The five bits following the block type is really the number of
      literal codes sent minus 257.
   9. Length codes 8,16,16 are interpreted as 13 length codes of 8 bits
      (1+6+6).  Therefore, to output three times the length, you output
      three codes (1+1+1), whereas to output four times the same length,
      you only need two codes (1+3).  Hmm.
  10. In the tree reconstruction algorithm, Code = Code + Increment
      only if BitLength(i) is not zero.  (Pretty obvious.)
  11. Correction: 4 Bits: # of Bit Length codes - 4     (4 - 19)
  12. Note: length code 284 can represent 227-258, but length code 285
      really is 258.  The last length deserves its own, short code
      since it gets used a lot in very redundant files.  The length
      258 is special since 258 - 3 (the min match length) is 255.
  13. The literal/length and distance code bit lengths are read as a
      single stream of lengths.  It is possible (and advantageous) for
      a repeat code (16, 17, or 18) to go across the boundary between
      the two sets of lengths.
 */

#ifdef RCSID
static char rcsid[] = "$Id: inflate.c,v 0.14 1993/06/10 13:27:04 jloup Exp $";
#endif

#include <sys/types.h>

/* #include "tailor.h" */

#if defined(STDC_HEADERS) || !defined(NO_STDLIB_H)
#  include <stdlib.h>
#endif

#include "gzip.h"
#define slide window

extern int32_t z_control;

/* Huffman code lookup table entry--this entry is four bytes for machines
   that have 16-bit pointers (e.g. PC's in the small or medium model).
   Valid extra bits are 0..13.  e == 15 is EOB (end of block), e == 16
   means that v is a literal, 16 < e < 32 means that v is a pointer to
   the next table, which codes e - 16 bits, and lastly e == 99 indicates
   an unused code.  If a code with e == 99 is looked up, this implies an
   error in the data. */
struct huft {
  uch e;                /* number of extra bits or operation */
  uch b;                /* number of bits in this code or subcode */
  union {
    ush n;              /* literal, length base, or distance base */
    struct huft *t;     /* pointer to next level of table */
  } v;
};


/* Function prototypes */
int32_t huft_build OF((uint32_t *, uint32_t, uint32_t, ush *, ush *,
                   struct huft **, int32_t *));
int32_t huft_free OF((struct huft *));
int32_t inflate_codes OF((struct huft *, struct huft *, int32_t, int32_t));
int32_t inflate_stored OF((void));
int32_t inflate_fixed OF((void));
int32_t inflate_dynamic OF((void));
int32_t inflate_block OF((int32_t *));
int32_t inflate OF((void));


/* The inflate algorithm uses a sliding 32K byte window on the uncompressed
   stream to find repeated byte strings.  This is implemented here as a
   circular buffer.  The index is updated simply by incrementing and then
   and'ing with 0x7fff (32K-1). */
/* It is left to other modules to supply the 32K area.  It is assumed
   to be usable as if it were declared "uch slide[32768];" or as just
   "uch *slide;" and then malloc'ed in the latter case.  The definition
   must be in unzip.h, included above. */
/* uint32_t wp;             current position in slide */
#define wp outcnt
#define flush_output(w) (wp=(w),flush_window())

/* Tables for deflate from PKZIP's appnote.txt. */
static uint32_t border[] = {    /* Order of the bit length code lengths */
        16, 17, 18, 0, 8, 7, 9, 6, 10, 5, 11, 4, 12, 3, 13, 2, 14, 1, 15};
static ush cplens[] = {         /* Copy lengths for literal codes 257..285 */
        3, 4, 5, 6, 7, 8, 9, 10, 11, 13, 15, 17, 19, 23, 27, 31,
        35, 43, 51, 59, 67, 83, 99, 115, 131, 163, 195, 227, 258, 0, 0};
        /* note: see note #13 above about the 258 in this list. */
static ush cplext[] = {         /* Extra bits for literal codes 257..285 */
        0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2, 2,
        3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0, 99, 99}; /* 99==invalid */
static ush cpdist[] = {         /* Copy offsets for distance codes 0..29 */
        1, 2, 3, 4, 5, 7, 9, 13, 17, 25, 33, 49, 65, 97, 129, 193,
        257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145,
        8193, 12289, 16385, 24577};
static ush cpdext[] = {         /* Extra bits for distance codes */
        0, 0, 0, 0, 1, 1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6,
        7, 7, 8, 8, 9, 9, 10, 10, 11, 11,
        12, 12, 13, 13};



/* Macros for inflate() bit peeking and grabbing.
   The usage is:
   
        NEEDBITS(j)
        x = b & mask_bits[j];
        DUMPBITS(j)

   where NEEDBITS makes sure that b has at least j bits in it, and
   DUMPBITS removes the bits from b.  The macros use the variable k
   for the number of bits in b.  Normally, b and k are register
   variables for speed, and are initialized at the beginning of a
   routine that uses these macros from a global bit buffer and count.

   If we assume that EOB will be the longest code, then we will never
   ask for bits with NEEDBITS that are beyond the end of the stream.
   So, NEEDBITS should not read any more bytes than are needed to
   meet the request.  Then no bytes need to be "returned" to the buffer
   at the end of the last block.

   However, this assumption is not true for fixed blocks--the EOB code
   is 7 bits, but the other literal/length codes can be 8 or 9 bits.
   (The EOB code is shorter than other codes because fixed blocks are
   generally short.  So, while a block always has an EOB, many other
   literal/length codes have a significantly lower probability of
   showing up at all.)  However, by making the first table have a
   lookup of seven bits, the EOB code will be found in that first
   lookup, and so will not require that too many bits be pulled from
   the stream.
 */

ulg bb;                         /* bit buffer */
uint32_t bk;                    /* bits in bit buffer */

ush mask_bits[] = {
    0x0000,
    0x0001, 0x0003, 0x0007, 0x000f, 0x001f, 0x003f, 0x007f, 0x00ff,
    0x01ff, 0x03ff, 0x07ff, 0x0fff, 0x1fff, 0x3fff, 0x7fff, 0xffff
};

#ifdef CRYPT
  uch cc;
#  define NEXTBYTE() \
     (decrypt ? (cc = get_byte(), zdecode(cc), cc) : get_byte())
#else
#  define NEXTBYTE()  (uch)get_byte()
#endif
#define NEEDBITS(n) {while(k<(n)){b|=((ulg)NEXTBYTE())<<k;k+=8;}}
#define DUMPBITS(n) {b>>=(n);k-=(n);}


/*
   Huffman code decoding is performed using a multi-level table lookup.
   The fastest way to decode is to simply build a lookup table whose
   size is determined by the longest code.  However, the time it takes
   to build this table can also be a factor if the data being decoded
   is not very long.  The most common codes are necessarily the
   shortest codes, so those codes dominate the decoding time, and hence
   the speed.  The idea is you can have a shorter table that decodes the
   shorter, more probable codes, and then point to subsidiary tables for
   the longer codes.  The time it costs to decode the longer codes is
   then traded against the time it takes to make longer tables.

   This results of this trade are in the variables lbits and dbits
   below.  lbits is the number of bits the first level table for literal/
   length codes can decode in one step, and dbits is the same thing for
   the distance codes.  Subsequent tables are also less than or equal to
   those sizes.  These values may be adjusted either when all of the
   codes are shorter than that, in which case the longest code length in
   bits is used, or when the shortest code is *longer* than the requested
   table size, in which case the length of the shortest code in bits is
   used.

   There are two different values for the two tables, since they code a
   different number of possibilities each.  The literal/length table
   codes 286 possible values, or in a flat code, a little over eight
   bits.  The distance table codes 30 possible values, or a little less
   than five bits, flat.  The optimum values for speed end up being
   about one bit more than those, so lbits is 8+1 and dbits is 5+1.
   The optimum values may differ though from machine to machine, and
   possibly even between compilers.  Your mileage may vary.
 */


int32_t lbits = 9;          /* bits in base literal/length lookup table */
int32_t dbits = 6;          /* bits in base distance lookup table */


/* If BMAX needs to be larger than 16, then h and x[] should be ulg. */
#define BMAX 16         /* maximum bit length of any code (16 for explode) */
#define N_MAX 288       /* maximum number of codes in any set */


uint32_t hufts;         /* track memory usage */


int32_t huft_build(b, n, s, d, e, t, m)
uint32_t *b;            /* code lengths in bits (all assumed <= BMAX) */
uint32_t n;             /* number of codes (assumed <= N_MAX) */
uint32_t s;             /* number of simple-valued codes (0..s-1) */
ush *d;                 /* list of base values for non-simple codes */
ush *e;                 /* list of extra bits for non-simple codes */
struct huft **t;        /* result: starting table */
int32_t *m;             /* maximum lookup bits, returns actual */
/* Given a list of code lengths and a maximum table size, make a set of
   tables to decode that set of codes.  Return zero on success, one if
   the given code set is incomplete (the tables are still built in this
   case), two if the input is invalid (all zero length codes or an
   oversubscribed set of lengths), and three if not enough memory. */
{
  static uint32_t a;            /* counter for codes of length k */
  static uint32_t c[BMAX+1];    /* bit length count table */
  static uint32_t f;            /* i repeats in table every f entries */
  static int32_t g;             /* maximum code length */
  static int32_t h;             /* table level */
  static uint32_t i;            /* counter, current code */
  static uint32_t j;            /* counter */
  static int32_t k;             /* number of bits in current code */
  static int32_t l;             /* bits per table (returned in m) */
  static uint32_t *p;           /* pointer into c[], b[], or v[] */
  static struct huft *q;        /* points to current table */
  static struct huft r;         /* table entry for structure assignment */
  static struct huft *u[BMAX];  /* table stack */
  static uint32_t v[N_MAX];     /* values in order of bit length */
  static int32_t w;             /* bits before this table == (l * h) */
  static uint32_t x[BMAX+1];    /* bit offsets, then code stack */
  static uint32_t *xp;          /* pointer into x */
  static int32_t y;             /* number of dummy codes added */
  static uint32_t z;            /* number of entries in current table */


  /* Generate counts for each bit length */
  memzero(c, sizeof(c));
  p = b;  i = n;
  do {
    Tracecv(*p, (stderr, (n-i >= ' ' && n-i <= '~' ? "%c %d\n" : "0x%x %d\n"), 
	    n-i, *p));
    c[*p]++;                    /* assume all entries <= BMAX */
    p++;                      /* Can't combine with above line (Solaris bug) */
  } while (--i);
  if (c[0] == n)                /* null input--all zero length codes */
  {
    *t = (struct huft *)NULL;
    *m = 0;
    return 0;
  }


  /* Find minimum and maximum length, bound *m by those */
  l = *m;
  for (j = 1; j <= BMAX; j++)
    if (c[j])
      break;
  k = j;                        /* minimum code length */
  if ((uint32_t)l < j)
    l = j;
  for (i = BMAX; i; i--)
    if (c[i])
      break;
  g = i;                        /* maximum code length */
  if ((uint32_t)l > i)
    l = i;
  *m = l;


  /* Adjust last length count to fill out codes, if needed */
  for (y = 1 << j; j < i; j++, y <<= 1)
    if ((y -= c[j]) < 0)
      return 2;                 /* bad input: more codes than bits */
  if ((y -= c[i]) < 0)
    return 2;
  c[i] += y;


  /* Generate starting offsets into the value table for each length */
  x[1] = j = 0;
  p = c + 1;  xp = x + 2;
  while (--i) {                 /* note that i == g from above */
    *xp++ = (j += *p++);
  }


  /* Make a table of values in order of bit lengths */
  p = b;  i = 0;
  do {
    if ((j = *p++) != 0)
      v[x[j]++] = i;
  } while (++i < n);


  /* Generate the Huffman codes and for each, make the table entries */
  x[0] = i = 0;                 /* first Huffman code is zero */
  p = v;                        /* grab values in bit order */
  h = -1;                       /* no tables yet--level -1 */
  w = -l;                       /* bits decoded == (l * h) */
  u[0] = (struct huft *)NULL;   /* just to keep compilers happy */
  q = (struct huft *)NULL;      /* ditto */
  z = 0;                        /* ditto */

  /* go through the bit lengths (k already is bits in shortest code) */
  for (; k <= g; k++)
  {
    a = c[k];
    while (a--)
    {
      /* here i is the Huffman code of length k bits for value *p */
      /* make tables up to required level */
      while (k > w + l)
      {
        h++;
        w += l;                 /* previous table always l bits */

        /* compute minimum size table less than or equal to l bits */
        z = (z = g - w) > (uint32_t)l ? l : z;  /* upper limit on table size */
        if ((f = 1 << (j = k - w)) > a + 1)     /* try a k-w bit table */
        {                       /* too few codes for k-w bit table */
          f -= a + 1;           /* deduct codes from patterns left */
          xp = c + k;
          while (++j < z)       /* try smaller tables up to z bits */
          {
            if ((f <<= 1) <= *++xp)
              break;            /* enough codes to use up j bits */
            f -= *xp;           /* else deduct codes from patterns */
          }
        }
        z = 1 << j;             /* table entries for j-bit table */

        /* allocate and link in new table */
        if ((q = (struct huft *)malloc((z + 1)*sizeof(struct huft))) ==
            (struct huft *)NULL)
        {
          if (h)
            huft_free(u[0]);
          return 3;             /* not enough memory */
        }
        hufts += z + 1;         /* track memory usage */
        *t = q + 1;             /* link to list for huft_free() */
        *(t = &(q->v.t)) = (struct huft *)NULL;
        u[h] = ++q;             /* table starts after link */

        /* connect to last table, if there is one */
        if (h)
        {
          x[h] = i;             /* save pattern for backing up */
          r.b = (uch)l;         /* bits to dump before this table */
          r.e = (uch)(16 + j);  /* bits in this table */
          r.v.t = q;            /* pointer to this table */
          j = i >> (w - l);     /* (get around Turbo C bug) */
          u[h-1][j] = r;        /* connect to last table */
        }
      }

      /* set up table entry in r */
      r.b = (uch)(k - w);
      if (p >= v + n)
        r.e = 99;               /* out of values--invalid code */
      else if (*p < s)
      {
        r.e = (uch)(*p < 256 ? 16 : 15);    /* 256 is end-of-block code */
        r.v.n = (ush)(*p);             /* simple code is just the value */
	p++;                           /* one compiler does not like *p++ */
      }
      else
      {
        r.e = (uch)e[*p - s];   /* non-simple--look up in lists */
        r.v.n = d[*p++ - s];
      }

      /* fill code-like entries with r */
      f = 1 << (k - w);
      for (j = i >> w; j < z; j += f)
        q[j] = r;

      /* backwards increment the k-bit code i */
      for (j = 1 << (k - 1); i & j; j >>= 1)
        i ^= j;
      i ^= j;

      /* backup over finished tables */
      while ((i & ((1 << w) - 1)) != x[h])
      {
        h--;                    /* don't need to update q */
        w -= l;
      }
    }
  }


  /* Return true (1) if we were given an incomplete table */
  return y != 0 && g != 1;
}



int32_t huft_free(t)
struct huft *t;         /* table to free */
/* Free the malloc'ed tables built by huft_build(), which makes a linked
   list of the tables it made, with the links in a dummy first entry of
   each table. */
{
  static struct huft *p, *q;


  /* Go through linked list, freeing from the malloced (t[-1]) address. */
  p = t;
  while (p != (struct huft *)NULL)
  {
    q = (--p)->v.t;
    free((char*)p);
    p = q;
  } 
  return 0;
}


int32_t inflate_codes(tl, td, bl, bd)
struct huft *tl, *td;   /* literal/length and distance decoder tables */
int32_t bl, bd;             /* number of bits decoded by tl[] and td[] */
/* inflate (decompress) the codes in a deflated (compressed) block.
   Return an error code or zero if it all goes ok. */
{
  static uint32_t e;           /* table entry flag/number of extra bits */
  static uint32_t n, d;        /* length and index for copy */
  static uint32_t w;           /* current window position */
  static struct huft *t;       /* pointer to table entry */
  static uint32_t ml, md;      /* masks for bl and bd bits */
  static ulg b;                /* bit buffer */
  static uint32_t k;           /* number of bits in bit buffer */

  if ((z_control & 0xf) == 1)  {
  	goto LABEL1;
  }
  if ((z_control & 0xf) == 2)  {
  	goto LABEL2;
  }

  /* make local copies of globals */
  b = bb;                       /* initialize bit buffer */
  k = bk;
  w = wp;                       /* initialize window position */

  /* inflate the coded data */
  ml = mask_bits[bl];           /* precompute masks for speed */
  md = mask_bits[bd];
  for (;;)                      /* do until end of block */
  {
    NEEDBITS((uint32_t)bl)
    if ((e = (t = tl + ((uint32_t)b & ml))->e) > 16)
      do {
        if (e == 99)
          return 1;
        DUMPBITS(t->b)
        e -= 16;
        NEEDBITS(e)
      } while ((e = (t = t->v.t + ((uint32_t)b & mask_bits[e]))->e) > 16);
    DUMPBITS(t->b)
    if (e == 16)                /* then it's a literal */
    {
      slide[w++] = (uch)t->v.n;
      Tracevv((stderr, "%c", slide[w-1]));
      if (w == WSIZE)
      {
	z_control |= 1;
        flush_output(w);
LABEL1:
	z_control &= ~1;
        w = 0;
      }
    }
    else                        /* it's an EOB or a length */
    {
      /* exit if end of block */
      if (e == 15)
        break;

      /* get length of block to copy */
      NEEDBITS(e)
      n = t->v.n + ((uint32_t)b & mask_bits[e]);
      DUMPBITS(e);

      /* decode distance of block to copy */
      NEEDBITS((uint32_t)bd)
      if ((e = (t = td + ((uint32_t)b & md))->e) > 16)
        do {
          if (e == 99)
            return 1;
          DUMPBITS(t->b)
          e -= 16;
          NEEDBITS(e)
        } while ((e = (t = t->v.t + ((uint32_t)b & mask_bits[e]))->e) > 16);
      DUMPBITS(t->b)
      NEEDBITS(e)
      d = w - t->v.n - ((uint32_t)b & mask_bits[e]);
      DUMPBITS(e)
      Tracevv((stderr,"\\[%d,%d]", w-d, n));

      /* do the copy */
      do {
        n -= (e = (e = WSIZE - ((d &= WSIZE-1) > w ? d : w)) > n ? n : e);
#if !defined(NOMEMCPY) && !defined(DEBUG)
        if (w - d >= e)         /* (this test assumes unsigned comparison) */
        {
          memcpy(slide + w, slide + d, e);
          w += e;
          d += e;
        }
        else                      /* do it slow to avoid memcpy() overlap */
#endif /* !NOMEMCPY */
          do {
            slide[w++] = slide[d++];
	    Tracevv((stderr, "%c", slide[w-1]));
          } while (--e);
        if (w == WSIZE)
        {
	  z_control |= 2;
          flush_output(w);
LABEL2:
	z_control &= ~2;
          w = 0;
        }
      } while (n);
    }
  }


  /* restore the globals from the locals */
  wp = w;                       /* restore global window pointer */
  bb = b;                       /* restore global bit buffer */
  bk = k;

  /* done */
  return 0;
}



int32_t inflate_stored()
/* "decompress" an inflated type 0 (stored) block. */
{
  static uint32_t n;           /* number of bytes in block */
  static uint32_t w;           /* current window position */
  static ulg b;                /* bit buffer */
  static uint32_t k;           /* number of bits in bit buffer */

  if (z_control == 3)  {
  	goto LABEL3;
  }


  /* make local copies of globals */
  b = bb;                       /* initialize bit buffer */
  k = bk;
  w = wp;                       /* initialize window position */


  /* go to byte boundary */
  n = k & 7;
  DUMPBITS(n);


  /* get the length and its complement */
  NEEDBITS(16)
  n = ((uint32_t)b & 0xffff);
  DUMPBITS(16)
  NEEDBITS(16)
  if (n != (uint32_t)((~b) & 0xffff))
    return 1;                   /* error in compressed data */
  DUMPBITS(16)


  /* read and output the compressed data */
  while (n--)
  {
    NEEDBITS(8)
    slide[w++] = (uch)b;
    if (w == WSIZE)
    {
      z_control = 3;
      flush_output(w);
LABEL3:
	z_control = 0;
      w = 0;
    }
    DUMPBITS(8)
  }


  /* restore the globals from the locals */
  wp = w;                       /* restore global window pointer */
  bb = b;                       /* restore global bit buffer */
  bk = k;
  return 0;
}



int32_t inflate_fixed()
/* decompress an inflated type 1 (fixed Huffman codes) block.  We should
   either replace this with a custom decoder, or at least precompute the
   Huffman tables. */
{
  static int32_t i;            /* temporary variable */
  static struct huft *tl;      /* literal/length code table */
  static struct huft *td;      /* distance code table */
  static int32_t bl;           /* lookup bits for tl */
  static int32_t bd;           /* lookup bits for td */
  static uint32_t l[288];      /* length list for huft_build */

  if (z_control != 0)  {
  	goto LABEL0;
  }


  /* set up literal table */
  for (i = 0; i < 144; i++)
    l[i] = 8;
  for (; i < 256; i++)
    l[i] = 9;
  for (; i < 280; i++)
    l[i] = 7;
  for (; i < 288; i++)          /* make a complete, but wrong code set */
    l[i] = 8;
  bl = 7;
  if ((i = huft_build(l, 288, 257, cplens, cplext, &tl, &bl)) != 0)
    return i;


  /* set up distance table */
  for (i = 0; i < 30; i++)      /* make an incomplete code set */
    l[i] = 5;
  bd = 5;
  if ((i = huft_build(l, 30, 0, cpdist, cpdext, &td, &bd)) > 1)
  {
    huft_free(tl);
    return i;
  }


  /* decompress until an end-of-block code */
  z_control = 0x80;
LABEL0:
  if (inflate_codes(tl, td, bl, bd))
    return 1;
    	z_control = 0;


  /* free the decoding tables, return */
  huft_free(tl);
  huft_free(td);
  return 0;
}



int32_t inflate_dynamic()
/* decompress an inflated type 2 (dynamic Huffman codes) block. */
{
  static int32_t i;            /* temporary variables */
  static uint32_t j;
  static uint32_t l;           /* last length */
  static uint32_t m;           /* mask for bit lengths table */
  static uint32_t n;           /* number of lengths to get */
  static struct huft *tl;      /* literal/length code table */
  static struct huft *td;      /* distance code table */
  static int32_t bl;           /* lookup bits for tl */
  static int32_t bd;           /* lookup bits for td */
  static uint32_t nb;          /* number of bit length codes */
  static uint32_t nl;          /* number of literal/length codes */
  static uint32_t nd;          /* number of distance codes */
#ifdef PKZIP_BUG_WORKAROUND
  static uint32_t ll[288+32];  /* literal/length and distance code lengths */
#else
  static uint32_t ll[286+30];  /* literal/length and distance code lengths */
#endif
  static ulg b;                /* bit buffer */
  static uint32_t k;           /* number of bits in bit buffer */

  if (z_control != 0)  {
  	goto LABEL12;
  }


  /* make local bit buffer */
  b = bb;
  k = bk;


  /* read in table lengths */
  NEEDBITS(5)
  nl = 257 + ((uint32_t)b & 0x1f);      /* number of literal/length codes */
  DUMPBITS(5)
  NEEDBITS(5)
  nd = 1 + ((uint32_t)b & 0x1f);        /* number of distance codes */
  DUMPBITS(5)
  NEEDBITS(4)
  nb = 4 + ((uint32_t)b & 0xf);         /* number of bit length codes */
  DUMPBITS(4)
#ifdef PKZIP_BUG_WORKAROUND
  if (nl > 288 || nd > 32)
#else
  if (nl > 286 || nd > 30)
#endif
    return 1;                   /* bad lengths */


  /* read in bit-length-code lengths */
  for (j = 0; j < nb; j++)
  {
    NEEDBITS(3)
    ll[border[j]] = (uint32_t)b & 7;
    DUMPBITS(3)
  }
  for (; j < 19; j++)
    ll[border[j]] = 0;


  /* build decoding table for trees--single level, 7 bit lookup */
  bl = 7;
  if ((i = huft_build(ll, 19, 19, NULL, NULL, &tl, &bl)) != 0)
  {
    if (i == 1)
      huft_free(tl);
    return i;                   /* incomplete code set */
  }


  /* read in literal and distance code lengths */
  n = nl + nd;
  m = mask_bits[bl];
  i = l = 0;
  while ((uint32_t)i < n)
  {
    NEEDBITS((uint32_t)bl)
    j = (td = tl + ((uint32_t)b & m))->b;
    DUMPBITS(j)
    j = td->v.n;
    if (j < 16)                 /* length of code in bits (0..15) */
      ll[i++] = l = j;          /* save last length in l */
    else if (j == 16)           /* repeat last length 3 to 6 times */
    {
      NEEDBITS(2)
      j = 3 + ((uint32_t)b & 3);
      DUMPBITS(2)
      if ((uint32_t)i + j > n)
        return 1;
      while (j--)
        ll[i++] = l;
    }
    else if (j == 17)           /* 3 to 10 zero length codes */
    {
      NEEDBITS(3)
      j = 3 + ((uint32_t)b & 7);
      DUMPBITS(3)
      if ((uint32_t)i + j > n)
        return 1;
      while (j--)
        ll[i++] = 0;
      l = 0;
    }
    else                        /* j == 18: 11 to 138 zero length codes */
    {
      NEEDBITS(7)
      j = 11 + ((uint32_t)b & 0x7f);
      DUMPBITS(7)
      if ((uint32_t)i + j > n)
        return 1;
      while (j--)
        ll[i++] = 0;
      l = 0;
    }
  }


  /* free decoding table for trees */
  huft_free(tl);


  /* restore the global bit buffer */
  bb = b;
  bk = k;


  /* build the decoding tables for literal/length and distance codes */
  bl = lbits;
  if ((i = huft_build(ll, nl, 257, cplens, cplext, &tl, &bl)) != 0)
  {
    if (i == 1) {
      fprintf(stderr, " incomplete literal tree\n");
      huft_free(tl);
    }
    return i;                   /* incomplete code set */
  }
  bd = dbits;
  if ((i = huft_build(ll + nl, nd, 0, cpdist, cpdext, &td, &bd)) != 0)
  {
    if (i == 1) {
      fprintf(stderr, " incomplete distance tree\n");
#ifdef PKZIP_BUG_WORKAROUND
      i = 0;
    }
#else
      huft_free(td);
    }
    huft_free(tl);
    return i;                   /* incomplete code set */
#endif
  }


  /* decompress until an end-of-block code */
  z_control = 0x40;
LABEL12:
  if (inflate_codes(tl, td, bl, bd))
    return 1;
    	z_control = 0;


  /* free the decoding tables, return */
  huft_free(tl);
  huft_free(td);
  return 0;
}



int32_t inflate_block(e)
int32_t *e;                 /* last block flag */
/* decompress an inflated block */
{
  static uint32_t t;    /* block type */
  static ulg b;         /* bit buffer */
  static uint32_t k;    /* number of bits in bit buffer */

  if ((z_control == 0x41) || (z_control == 0x42))  {
  	goto LABEL9;
  }
  if ((z_control == 0x81) || (z_control == 0x82))  {
  	goto LABEL10;
  }
  if (z_control == 3)  {
  	goto LABEL11;
  }

  /* make local bit buffer */
  b = bb;
  k = bk;


  /* read in last block bit */
  NEEDBITS(1)
  *e = (int32_t)b & 1;
  DUMPBITS(1)


  /* read in block type */
  NEEDBITS(2)
  t = (uint32_t)b & 3;
  DUMPBITS(2)


  /* restore the global bit buffer */
  bb = b;
  bk = k;


  /* inflate that block type */
  if (t == 2)  {
LABEL9:
    return inflate_dynamic();
  }
  if (t == 0)  {
LABEL11:
    return inflate_stored();
  }
  if (t == 1)  {
LABEL10:
    return inflate_fixed();
  }


  /* bad block type */
  return 2;
}



int32_t inflate()
/* decompress an inflated entry */
{
  static int32_t e;            /* last block flag */
  static int32_t r;            /* result code */
  static uint32_t h;           /* maximum struct huft's malloc'ed */

  if (z_control == 4)  {
  	goto LABEL4;
  }
  else if (z_control != 0)  {
  	goto LABEL8;
  }


  /* initialize window, bit buffer */
  wp = 0;
  bk = 0;
  bb = 0;


  /* decompress until the last block */
  h = 0;
  do {
    hufts = 0;
LABEL8:
    if ((r = inflate_block(&e)) != 0)
      return r;
    if (hufts > h)
      h = hufts;
  } while (!e);

  /* Undo too much lookahead. The next read will be byte aligned so we
   * can discard unused bits in the last meaningful byte.
   */
  while (bk >= 8) {
    bk -= 8;
    inptr--;
  }

  /* flush out slide */
  z_control = 4;
  flush_output(wp);
LABEL4:
	z_control = 0;


  /* return success */
#ifdef DEBUG
  fprintf(stderr, "<%u> ", h);
#endif /* DEBUG */
  return 0;
}



/* =========================== util.c, with modifications ========== */
/* util.c -- utility functions for gzip support
 * Copyright (C) 1992-1993 Jean-loup Gailly
 * This is free software; you can redistribute it and/or modify it under the
 * terms of the GNU General Public License, see the file COPYING.
 */

#ifdef RCSID
static char rcsid[] = "$Id: util.c,v 0.15 1993/06/15 09:04:13 jloup Exp $";
#endif

#include <ctype.h>
#include <errno.h>
#include <sys/types.h>
#include <setjmp.h>

extern char	*global_buf;
extern int32_t	global_count;
extern jmp_buf	global_env;
extern int32_t	global_val;

/* #include "tailor.h" */
#define PATH_SEP '/'
#define casemap(c) (c)

#ifdef HAVE_UNISTD_H
#  include <unistd.h>
#endif
#ifndef NO_FCNTL_H
#  include <fcntl.h>
#endif

#if defined(STDC_HEADERS) || !defined(NO_STDLIB_H)
#  include <stdlib.h>
#else
   extern int32_t errno;
#endif

#include "gzip.h"
/* #include "crypt.h" */

extern ulg crc_32_tab[];   /* crc table, defined below */

/* ===========================================================================
 * Copy input to output unchanged: zcat == cat with --force.
 * IN assertion: insize bytes have already been read in inbuf.
 */
int32_t copy(in, out)
    int32_t in, out;   /* input and output file descriptors */
{
    errno = 0;
    while (insize != 0 && (int32_t)insize != EOF) {
	write_buf(out, (char*)inbuf, insize);
	bytes_out += insize;
	insize = read(in, (char*)inbuf, INBUFSIZ);
    }
    if ((int32_t)insize == EOF && errno != 0) {
	read_error();
    }
    bytes_in = bytes_out;
    return OK;
}

/* ===========================================================================
 * Run a set of bytes through the crc shift register.  If s is a NULL
 * pointer, then initialize the crc shift register contents instead.
 * Return the current crc in either case.
 */
ulg updcrc(s, n)
    uch *s;                 /* pointer to bytes to pump through */
    uint32_t n;             /* number of bytes in s[] */
{
    register ulg c;         /* temporary variable */

    static ulg crc = (ulg)0xffffffffL; /* shift register contents */

    if (s == NULL) {
	c = 0xffffffffL;
    } else {
	c = crc;
        if (n) do {
            c = crc_32_tab[((int32_t)c ^ (*s++)) & 0xff] ^ (c >> 8);
        } while (--n);
    }
    crc = c;
    return c ^ 0xffffffffL;       /* (instead of ~c for 64-bit machines) */
}

/* ===========================================================================
 * Clear input and output buffers
 */
void clear_bufs()
{
    outcnt = 0;
    insize = inptr = 0;
    bytes_in = bytes_out = 0L;
}

/* ===========================================================================
 * Fill the input buffer. This is called only when the buffer is empty.
 */
int32_t fill_inbuf(eof_ok)
    int32_t eof_ok;          /* set if EOF acceptable as a result */
{
    int32_t len;

    /* Read as much as possible */
    insize = 0;
    errno = 0;
    do {
	len = read(ifd, (char*)inbuf+insize, INBUFSIZ-insize);
        if (len == 0 || len == EOF) break;
	insize += len;
    } while (insize < INBUFSIZ);

    if (insize == 0) {
	if (eof_ok) return EOF;
	read_error();
    }
    bytes_in += (ulg)insize;
    inptr = 1;
    return inbuf[0];
}

/* ===========================================================================
 * Write the output buffer outbuf[0..outcnt-1] and update bytes_out.
 * (used for the compressed data only)
 */
void flush_outbuf()
{
    if (outcnt == 0) return;

    write_buf(ofd, (char *)outbuf, outcnt);
    bytes_out += (ulg)outcnt;
    outcnt = 0;
}

/* ===========================================================================
 * Write the output window window[0..outcnt-1] and update crc and bytes_out.
 * (Used for the decompressed data only.)
 */
void flush_window()
{
    if (outcnt == 0) {
	global_count = 0;
    	return;
    }
    updcrc(window, outcnt);

    if (!test) {
/*	write_buf(ofd, (char *)window, outcnt); */
    }
if (global_count > outcnt)  {
	memcpy(global_buf, (char *)window, outcnt);
	bytes_out += (ulg)outcnt;
	global_count = outcnt;
	outcnt = 0;
}
else  {
	memcpy(global_buf, (char *)window, global_count);
	bytes_out += (ulg)global_count;
	outcnt -= global_count;
}
longjmp(global_env, 1);
}

/* ===========================================================================
 * Does the same as write(), but also handles partial pipe writes and checks
 * for error return.
 */
void write_buf(fd, buf, cnt)
    int32_t       fd;
    voidp     buf;
    uint32_t  cnt;
{
    uint32_t  n;

    while ((n = write(fd, buf, cnt)) != cnt) {
	if (n == (uint32_t)(-1)) {
	    write_error();
	}
	cnt -= n;
	buf = (voidp)((char*)buf+n);
    }
}

/* ========================================================================
 * Put string s in lower case, return s.
 */
char *strlwr(s)
    char *s;
{
    char *t;
    for (t = s; *t; t++) *t = tolow(*t);
    return s;
}

/* ========================================================================
 * Return the base name of a file (remove any directory prefix and
 * any version suffix). For systems with file names that are not
 * case sensitive, force the base name to lower case.
 */
char *local_basename(fname)
    char *fname;
{
    char *p;

    if ((p = strrchr(fname, PATH_SEP))  != NULL) fname = p+1;
#ifdef PATH_SEP2
    if ((p = strrchr(fname, PATH_SEP2)) != NULL) fname = p+1;
#endif
#ifdef PATH_SEP3
    if ((p = strrchr(fname, PATH_SEP3)) != NULL) fname = p+1;
#endif
#ifdef SUFFIX_SEP
    if ((p = strrchr(fname, SUFFIX_SEP)) != NULL) *p = '\0';
#endif
    if (casemap('A') == 'a') strlwr(fname);
    return fname;
}

/* ========================================================================
 * Make a file name legal for file systems not allowing file names with
 * multiple dots or starting with a dot (such as MSDOS), by changing
 * all dots except the last one into underlines.  A target dependent
 * function can be used instead of this simple function by defining the macro
 * MAKE_LEGAL_NAME in tailor.h and providing the function in a target
 * dependent module.
 */
void make_simple_name(name)
    char *name;
{
    char *p = strrchr(name, '.');
    if (p == NULL) return;
    if (p == name) p++;
    do {
        if (*--p == '.') *p = '_';
    } while (p != name);
}


#if defined(NO_STRING_H) && !defined(STDC_HEADERS)

/* Provide missing strspn and strcspn functions. */

#  ifndef __STDC__
#    define const
#  endif

int32_t strspn  OF((const char *s, const char *accept));
int32_t strcspn OF((const char *s, const char *reject));

/* ========================================================================
 * Return the length of the maximum initial segment
 * of s which contains only characters in accept.
 */
int32_t strspn(s, accept)
    const char *s;
    const char *accept;
{
    register const char *p;
    register const char *a;
    register int32_t count = 0;

    for (p = s; *p != '\0'; ++p) {
	for (a = accept; *a != '\0'; ++a) {
	    if (*p == *a) break;
	}
	if (*a == '\0') return count;
	++count;
    }
    return count;
}

/* ========================================================================
 * Return the length of the maximum inital segment of s
 * which contains no characters from reject.
 */
int32_t strcspn(s, reject)
    const char *s;
    const char *reject;
{
    register int32_t count = 0;

    while (*s != '\0') {
	if (strchr(reject, *s++) != NULL) return count;
	++count;
    }
    return count;
}

#endif /* NO_STRING_H */

/* ========================================================================
 * Add an environment variable (if any) before argv, and update argc.
 * Return the expanded environment variable to be freed later, or NULL 
 * if no options were added to argv.
 */
#define SEPARATOR	" \t"	/* separators in env variable */

char *add_envopt(argcp, argvp, env)
    int32_t *argcp;      /* pointer to argc */
    char ***argvp;       /* pointer to argv */
    char *env;           /* name of environment variable */
{
    char *p;             /* running pointer through env variable */
    char **oargv;        /* runs through old argv array */
    char **nargv;        /* runs through new argv array */
    int32_t	 oargc = *argcp; /* old argc */
    int32_t  nargc = 0;  /* number of arguments in env variable */

    env = (char*)getenv(env);
    if (env == NULL) return NULL;

    p = (char*)xmalloc(strlen(env)+1);
    env = strcpy(p, env);                    /* keep env variable intact */

    for (p = env; *p; nargc++ ) {            /* move through env */
	p += strspn(p, SEPARATOR);	     /* skip leading separators */
	if (*p == '\0') break;

	p += strcspn(p, SEPARATOR);	     /* find end of word */
	if (*p) *p++ = '\0';		     /* mark it */
    }
    if (nargc == 0) {
	free(env);
	return NULL;
    }
    *argcp += nargc;
    /* Allocate the new argv array, with an extra element just in case
     * the original arg list did not end with a NULL.
     */
    nargv = (char**)calloc(*argcp+1, sizeof(char *));
    if (nargv == NULL) error("out of memory");
    oargv  = *argvp;
    *argvp = nargv;

    /* Copy the program name first */
    if (oargc-- < 0) error("argc<=0");
    *(nargv++) = *(oargv++);

    /* Then copy the environment args */
    for (p = env; nargc > 0; nargc--) {
	p += strspn(p, SEPARATOR);	     /* skip separators */
	*(nargv++) = p;			     /* store start */
	while (*p++) ;			     /* skip over word */
    }

    /* Finally copy the old args and add a NULL (usual convention) */
    while (oargc--) *(nargv++) = *(oargv++);
    *nargv = NULL;
    return env;
}

/* ========================================================================
 * Error handlers.
 */
void error(m)
    char *m;
{
    fprintf(stderr, "\n%s: %s: %s\n", progname, ifname, m);
    abort_gzip();
}

void warn(a, b)
    char *a, *b;            /* message strings juxtaposed in output */
{
    WARN((stderr, "%s: %s: warning: %s%s\n", progname, ifname, a, b));
}

void read_error()
{
/*    fprintf(stderr, "\n%s: ", progname); */
    if (errno != 0) {
	perror(ifname);
    } else {
	fprintf(stderr, "unexpected end of file\n");
    }
    abort_gzip();
}

void write_error()
{
    fprintf(stderr, "\n%s: ", progname);
    perror(ofname);
    abort_gzip();
}

/* ========================================================================
 * Display compression ratio on the given stream on 6 characters.
 */
void display_ratio(num, den, file)
    int32_t num;
    int32_t den;
    FILE *file;
{
    int32_t ratio;  /* 1000 times the compression ratio */

    if (den == 0) {
	ratio = 0; /* no compression */
    } else if (den < 2147483L) { /* (2**31 -1)/1000 */
	ratio = 1000L*num/den;
    } else {
	ratio = num/(den/1000L);
    }
    if (ratio < 0) {
	putc('-', file);
	ratio = -ratio;
    } else {
	putc(' ', file);
    }
    fprintf(file, "%2ld.%1ld%%", ratio / 10L, ratio % 10L);
}


/* ========================================================================
 * Semi-safe malloc -- never returns NULL.
 */
voidp xmalloc (size)
    uint32_t size;
{
    voidp cp = (voidp)malloc (size);

    if (cp == NULL) error("out of memory");
    return cp;
}

/* ========================================================================
 * Table of CRC-32's of all single-byte values (made by makecrc.c)
 */
ulg crc_32_tab[] = {
  0x00000000L, 0x77073096L, 0xee0e612cL, 0x990951baL, 0x076dc419L,
  0x706af48fL, 0xe963a535L, 0x9e6495a3L, 0x0edb8832L, 0x79dcb8a4L,
  0xe0d5e91eL, 0x97d2d988L, 0x09b64c2bL, 0x7eb17cbdL, 0xe7b82d07L,
  0x90bf1d91L, 0x1db71064L, 0x6ab020f2L, 0xf3b97148L, 0x84be41deL,
  0x1adad47dL, 0x6ddde4ebL, 0xf4d4b551L, 0x83d385c7L, 0x136c9856L,
  0x646ba8c0L, 0xfd62f97aL, 0x8a65c9ecL, 0x14015c4fL, 0x63066cd9L,
  0xfa0f3d63L, 0x8d080df5L, 0x3b6e20c8L, 0x4c69105eL, 0xd56041e4L,
  0xa2677172L, 0x3c03e4d1L, 0x4b04d447L, 0xd20d85fdL, 0xa50ab56bL,
  0x35b5a8faL, 0x42b2986cL, 0xdbbbc9d6L, 0xacbcf940L, 0x32d86ce3L,
  0x45df5c75L, 0xdcd60dcfL, 0xabd13d59L, 0x26d930acL, 0x51de003aL,
  0xc8d75180L, 0xbfd06116L, 0x21b4f4b5L, 0x56b3c423L, 0xcfba9599L,
  0xb8bda50fL, 0x2802b89eL, 0x5f058808L, 0xc60cd9b2L, 0xb10be924L,
  0x2f6f7c87L, 0x58684c11L, 0xc1611dabL, 0xb6662d3dL, 0x76dc4190L,
  0x01db7106L, 0x98d220bcL, 0xefd5102aL, 0x71b18589L, 0x06b6b51fL,
  0x9fbfe4a5L, 0xe8b8d433L, 0x7807c9a2L, 0x0f00f934L, 0x9609a88eL,
  0xe10e9818L, 0x7f6a0dbbL, 0x086d3d2dL, 0x91646c97L, 0xe6635c01L,
  0x6b6b51f4L, 0x1c6c6162L, 0x856530d8L, 0xf262004eL, 0x6c0695edL,
  0x1b01a57bL, 0x8208f4c1L, 0xf50fc457L, 0x65b0d9c6L, 0x12b7e950L,
  0x8bbeb8eaL, 0xfcb9887cL, 0x62dd1ddfL, 0x15da2d49L, 0x8cd37cf3L,
  0xfbd44c65L, 0x4db26158L, 0x3ab551ceL, 0xa3bc0074L, 0xd4bb30e2L,
  0x4adfa541L, 0x3dd895d7L, 0xa4d1c46dL, 0xd3d6f4fbL, 0x4369e96aL,
  0x346ed9fcL, 0xad678846L, 0xda60b8d0L, 0x44042d73L, 0x33031de5L,
  0xaa0a4c5fL, 0xdd0d7cc9L, 0x5005713cL, 0x270241aaL, 0xbe0b1010L,
  0xc90c2086L, 0x5768b525L, 0x206f85b3L, 0xb966d409L, 0xce61e49fL,
  0x5edef90eL, 0x29d9c998L, 0xb0d09822L, 0xc7d7a8b4L, 0x59b33d17L,
  0x2eb40d81L, 0xb7bd5c3bL, 0xc0ba6cadL, 0xedb88320L, 0x9abfb3b6L,
  0x03b6e20cL, 0x74b1d29aL, 0xead54739L, 0x9dd277afL, 0x04db2615L,
  0x73dc1683L, 0xe3630b12L, 0x94643b84L, 0x0d6d6a3eL, 0x7a6a5aa8L,
  0xe40ecf0bL, 0x9309ff9dL, 0x0a00ae27L, 0x7d079eb1L, 0xf00f9344L,
  0x8708a3d2L, 0x1e01f268L, 0x6906c2feL, 0xf762575dL, 0x806567cbL,
  0x196c3671L, 0x6e6b06e7L, 0xfed41b76L, 0x89d32be0L, 0x10da7a5aL,
  0x67dd4accL, 0xf9b9df6fL, 0x8ebeeff9L, 0x17b7be43L, 0x60b08ed5L,
  0xd6d6a3e8L, 0xa1d1937eL, 0x38d8c2c4L, 0x4fdff252L, 0xd1bb67f1L,
  0xa6bc5767L, 0x3fb506ddL, 0x48b2364bL, 0xd80d2bdaL, 0xaf0a1b4cL,
  0x36034af6L, 0x41047a60L, 0xdf60efc3L, 0xa867df55L, 0x316e8eefL,
  0x4669be79L, 0xcb61b38cL, 0xbc66831aL, 0x256fd2a0L, 0x5268e236L,
  0xcc0c7795L, 0xbb0b4703L, 0x220216b9L, 0x5505262fL, 0xc5ba3bbeL,
  0xb2bd0b28L, 0x2bb45a92L, 0x5cb36a04L, 0xc2d7ffa7L, 0xb5d0cf31L,
  0x2cd99e8bL, 0x5bdeae1dL, 0x9b64c2b0L, 0xec63f226L, 0x756aa39cL,
  0x026d930aL, 0x9c0906a9L, 0xeb0e363fL, 0x72076785L, 0x05005713L,
  0x95bf4a82L, 0xe2b87a14L, 0x7bb12baeL, 0x0cb61b38L, 0x92d28e9bL,
  0xe5d5be0dL, 0x7cdcefb7L, 0x0bdbdf21L, 0x86d3d2d4L, 0xf1d4e242L,
  0x68ddb3f8L, 0x1fda836eL, 0x81be16cdL, 0xf6b9265bL, 0x6fb077e1L,
  0x18b74777L, 0x88085ae6L, 0xff0f6a70L, 0x66063bcaL, 0x11010b5cL,
  0x8f659effL, 0xf862ae69L, 0x616bffd3L, 0x166ccf45L, 0xa00ae278L,
  0xd70dd2eeL, 0x4e048354L, 0x3903b3c2L, 0xa7672661L, 0xd06016f7L,
  0x4969474dL, 0x3e6e77dbL, 0xaed16a4aL, 0xd9d65adcL, 0x40df0b66L,
  0x37d83bf0L, 0xa9bcae53L, 0xdebb9ec5L, 0x47b2cf7fL, 0x30b5ffe9L,
  0xbdbdf21cL, 0xcabac28aL, 0x53b39330L, 0x24b4a3a6L, 0xbad03605L,
  0xcdd70693L, 0x54de5729L, 0x23d967bfL, 0xb3667a2eL, 0xc4614ab8L,
  0x5d681b02L, 0x2a6f2b94L, 0xb40bbe37L, 0xc30c8ea1L, 0x5a05df1bL,
  0x2d02ef8dL
};
