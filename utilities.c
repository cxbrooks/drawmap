/*
 * =========================================================================
 * utilities - A library of utility functions used by the drawmap program.
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

#include <stdint.h>
#include <math.h>
#include <sys/types.h>
#include "drawmap.h"


/*
 * Convert latitudes in degree/min/sec format into decimal degrees.
 *
 * We assume that there is no decimal point, or other punctuation,
 * and that the numeric latitude is in the DDMMSS format,
 * and that the latitude is immediately followed by 'N' or 'S'.
 */
double lat_conv(char *ptr)
{
	double lat;
	double min;
	double sec;

	lat = *ptr - '0';
	lat = lat * 10.0 + *(ptr + 1) - '0';
	min = *(ptr + 2) - '0';
	min = min * 10.0 + *(ptr + 3) - '0';
	sec = *(ptr + 4) - '0';
	sec = sec * 10.0 + *(ptr + 5) - '0';
	lat = lat + min / 60.0 + sec / 3600.0;
	if (*(ptr + 6) == 'S')  {
		lat = -lat;
	}
	return(lat);
}


/*
 * Convert longitudes in degree/min/sec format into decimal degrees.
 *
 * We assume that there is no decimal point, or other punctuation,
 * and that the numeric longitude is in the DDDMMSS format,
 * and that the longitude is immediately followed by 'W' or 'E'.
 */
double lon_conv(char *ptr)
{
	double lon;
	double min;
	double sec;

	lon = *ptr - '0';
	lon = lon * 10.0 + *(ptr + 1) - '0';
	lon = lon * 10.0 + *(ptr + 2) - '0';
	min = *(ptr + 3) - '0';
	min = min * 10.0 + *(ptr + 4) - '0';
	sec = *(ptr + 5) - '0';
	sec = sec * 10.0 + *(ptr + 6) - '0';
	lon = lon + min / 60.0 + sec / 3600.0;
	if (*(ptr + 7) == 'W')  {
		lon = -lon;
	}
	return(lon);
}




/* Round double values to int32_t integers. */
int32_t
drawmap_round(double f)
{
	int32_t i;
	double ff;

	i = (int32_t)f;
	ff = (double)i;

	if (f < 0.0)  {
		if ((ff - f) >= 0.5)  {
			return(i - 1);
		}
	}
	else  {
		if ((f - ff) >= 0.5)  {
			return(i + 1);
		}
	}

	return(i);
}



/* Find the maximum of two int32_t integers. */
int32_t
max(int32_t a, int32_t b)
{
	if (a > b)  {
		return(a);
	}
	else  {
		return(b);
	}
}



/* Find the minimum of three doubles */
double
min3(double a, double b, double c)
{
	if (a < b)  {
		if (a < c)  {
			return(a);
		}
		else  {
			return(c);
		}
	}
	else  {
		if (b < c)  {
			return(b);
		}
		else  {
			return(c);
		}
	}
}



/* Find the maximum of three doubles */
double
max3(double a, double b, double c)
{
	if (a > b)  {
		if (a > c)  {
			return(a);
		}
		else  {
			return(c);
		}
	}
	else  {
		if (b > c)  {
			return(b);
		}
		else  {
			return(c);
		}
	}
}




/*
 * Convert decimal degrees into degrees, minutes, seconds.
 */
void
decimal_degrees_to_dms(double decimal, int32_t *d, int32_t *m, double *s)
{
	int32_t sign;

	if (decimal < 0.0)  {
		sign = -1;
		decimal = -decimal;
	}
	else  {
		sign = 1;
	}

	*d = (int32_t)decimal;
	*m = (int32_t)((decimal - (double)*d) * 60.0);
	*s = (decimal - (double)*d - (double)*m / 60.) * 3600.00;
	*d = sign * *d;
}




struct utm_zones  {
	int32_t	zone;
	double	central_meridian;
	double	low_boundary;
	double	high_boundary;
} utm_zones[61] =  {
{	 0,	   0.0,	   0.0,	   0.0, },	// Dummy entry so that the zone indices are correct
{	 1,	-177.0,	-180.0,	-174.0, },
{	 2,	-171.0,	-174.0,	-168.0, },
{	 3,	-165.0,	-168.0,	-162.0, },
{	 4,	-159.0,	-162.0,	-156.0, },
{	 5,	-153.0,	-156.0,	-150.0, },
{	 6,	-147.0,	-150.0,	-144.0, },
{	 7,	-141.0,	-144.0,	-138.0, },
{	 8,	-135.0,	-138.0,	-132.0, },
{	 9,	-129.0,	-132.0,	-126.0, },
{	10,	-123.0,	-126.0,	-120.0, },
{	11,	-117.0,	-120.0,	-114.0, },
{	12,	-111.0,	-114.0,	-108.0, },
{	13,	-105.0,	-108.0,	-102.0, },
{	14,	- 99.0,	-102.0,	- 96.0, },
{	15,	- 93.0,	-096.0,	- 90.0, },
{	16,	- 87.0,	-090.0,	- 84.0, },
{	17,	- 81.0,	-084.0,	- 78.0, },
{	18,	- 75.0,	-078.0,	- 72.0, },
{	19,	- 69.0,	-072.0,	- 66.0, },
{	20,	- 63.0,	-066.0,	- 60.0, },
{	21,	- 57.0,	-060.0,	- 54.0, },
{	22,	- 51.0,	-054.0,	- 48.0, },
{	23,	- 45.0,	-048.0,	- 42.0, },
{	24,	- 39.0,	-042.0,	- 36.0, },
{	25,	- 33.0,	-036.0,	- 30.0, },
{	26,	- 27.0,	-030.0,	- 24.0, },
{	27,	- 21.0,	-024.0,	- 18.0, },
{	28,	- 15.0,	-018.0,	- 12.0, },
{	29,	-  9.0,	-012.0,	-  6.0, },
{	30,	-  3.0,	-006.0,	   0.0, },
{	31,	   3.0,	 000.0,	   6.0, },
{	32,	   9.0,	 006.0,	  12.0, },
{	33,	  15.0,	 012.0,	  18.0, },
{	34,	  21.0,	 018.0,	  24.0, },
{	35,	  27.0,	 024.0,	  30.0, },
{	36,	  33.0,	 030.0,	  36.0, },
{	37,	  39.0,	 036.0,	  42.0, },
{	38,	  45.0,	 042.0,	  48.0, },
{	39,	  51.0,	 048.0,	  54.0, },
{	40,	  57.0,	 054.0,	  60.0, },
{	41,	  63.0,	 060.0,	  66.0, },
{	42,	  69.0,	 066.0,	  72.0, },
{	43,	  75.0,	 072.0,	  78.0, },
{	44,	  81.0,	 078.0,	  84.0, },
{	45,	  87.0,	 084.0,	  90.0, },
{	46,	  93.0,	 090.0,	  96.0, },
{	47,	  99.0,	 096.0,	 102.0, },
{	48,	 105.0,	 102.0,	 108.0, },
{	49,	 111.0,	 108.0,	 114.0, },
{	50,	 117.0,	 114.0,	 120.0, },
{	51,	 123.0,	 120.0,	 126.0, },
{	52,	 129.0,	 126.0,	 132.0, },
{	53,	 135.0,	 132.0,	 138.0, },
{	54,	 141.0,	 138.0,	 144.0, },
{	55,	 147.0,	 144.0,	 150.0, },
{	56,	 153.0,	 150.0,	 156.0, },
{	57,	 159.0,	 156.0,	 162.0, },
{	58,	 165.0,	 162.0,	 168.0, },
{	59,	 171.0,	 168.0,	 174.0, },
{	60,	 177.0,	 174.0,	 180.0, },
};

/*
 * The following two functions use Redfearn's formulas to calculate the
 * forward and inverse projections between UTM coordinates and geographic
 * (latitude/longitude) coordinates.
 *
 * Given some parameters for the selected ellipsoid, Redfearn's formulas
 * allow one to translate back and forth between UTM
 * coordinates and latitude/longitude coordinates.
 * Before we examine Redfearn's formulas, here are some preliminary notes.
 *
 * These formulas were apparently originally published in 1948:
 *
 *	Redfearn, J.C.B., "Transverse Mercator Formulae", Empire Survey Review, 69, 1948, 318-322.
 *
 * I was unable to find a copy of this reference for verification, but did
 * find several other documents that described the formulas.  From them, I
 * pieced together the formulas here.
 *
 * A good reference for projection calculations of all kinds is supposed to be:
 *      Snyder, John P., Map Projections -- A Working Manual:
 *      U.S. Geological Survey Professional Paper 1395,
 *      United States Government Printing Office, Washington D.C., 1987.
 * Although I haven't personnally had a chance to read it, it is frequently
 * recommended on the Internet.
 *
 * A software package for doing projections is the PROJ package.  It is
 * available on the Internet.  I chose not to use it because my needs are
 * limited, and it was easier to just write the software I need than integrate
 * PROJ.  It is, however, a fine package, as far as I can tell.
 *
 * As far as I know, most or all of the currently-available 7.5min USGS data assumes
 * the Clarke 1866 ellipsoid with the North American Datum of 1927 (nad-27).
 * According to the DEM standards document, they are nad-27 if they have the
 * old-format Type A record.  The new Type A record contains a field to specify the datum.
 * I gather that the data will ultimately be re-referenced to the new GRS80
 * ellipsoid and nad-83.
 *
 * The 1-degree DEMs may or may not be in the new WGS 84 datum.  The standards document
 * says that recomputed data have been made available to the USGS, but doesn't say
 * if these new data are what is available for download.  We probably don't care,
 * for the time being, because the 1-degree data come in latitude/longitude format,
 * and we don't convert them to any other form.
 *
 * Parameters for various ellipsoids are given in drawmap.h.
 *
 * Now, on to Redfearn's formulas.
 *
 *    Note:  In the equations that follow, y is the true northing, utm_y is the northing with
 *           the false northing (10,000,000m) added in, x is the true easting, utm_y is the
 *           easting with the false easting (500,000m) added in.
 *
 * They begin with the calculation of the length of arc of a meridian (a great
 * circle passing through the poles).  The formula (in Macsyma format) is:
 *
 *     m = a * (1 - e^2) * int(1 - (e^2 sin^2(lat))^(-3/2), lat, lat1, lat2)
 *
 * (where the caret sign, '^', represents exponentiation and 'int' represents
 * a definite integral of the given expression over the variable lat, from lat1 to lat2).
 *
 * We are interested in the case where lat1 = 0 (and the integral starts at the equator).
 *
 * This integral is normally calculated via a series expansion:
 *
 *     m = a * (A0 * lat - A2 * sin(2*lat) + A4 * sin(4*lat) - A6 * sin(6*lat))
 *
 *         A0 = 1 - (e^2 / 4) - (3 * e^4 / 64) - (5 * e^6 / 256)
 *         A2 = (3/8) * (e^2 + e^4 / 4 + 15 e^6 / 128)
 *         A4 = (15 / 256) * (e^4 + 3 * e^6 / 4)
 *         A6 = 35 * e^6 / 3072
 *
 * With the value of m in hand, we move on to calculate the foot-point latitude, lat', which is
 * the latitude for which m = y / k0
 * where y is the true northing (which is just the northing in the northern climes, but is the
 * nominal northing - 10,000,000 in the southern hemisphere) and k0 is the centeral-meridian
 * scale factor.
 *
 * The foot-point latitude is found as follows:
 *
 *      n = (a - b) / (a + b) = f / (2 - f)
 *      G = a * (1 - n) * (1 - n^2) * (1 + (9/4) * n^2 + (225/64) * n^4) * (pi / 180)
 *      sigma = (m * pi) / (180 * G)
 *      lat' = sigma + ((3*n/2) - (27*n^3/32)) * sin(2*sigma) + ((21*n^2/16) - (55*n^4/32)) * sin(4*sigma) + (151*n^3/96) * sin(6*sigma) + (1097*n^4/512) * sin(8*sigma)        in units of radians
 *
 * For the inverse projection, where the latitude is to-be-determined, there may
 * be some snazzy way to find lat', but I chose to do it iteratively using
 * Newton's nethod.  In my limited testing, this approach appears to converge
 * quite rapidly, in about 2 or 3 iterations.  (I have not, however, tested the convergence
 * rate rigorously.)
 *
 * Next, we need the radii of curvature, found from the formulas.
 *
 *      rho = a * (1 - e^2) / (1 - e^2 * sin^2(lat))^(3/2)
 *      nu  = a / (1 - e^2 * sin^2(lat))^(1/2)
 *      phi = nu / rho
 *
 * These are general formulas.  When we evaluate them specifically for the foot-point
 * latitude, then we prime each of them:
 *
 *      rho' = a * (1 - e^2) / (1 - e^2 * sin^2(lat'))^(3/2)
 *      nu'  = a / (1 - e^2 * sin^2(lat'))^(1/2)
 *      phi' = nu' / rho'
 *
 *
 * This completes the preliminary calculations.  Now, on to the actual conversions.
 *
 *
 * lat/long to UTM, performed by function redfearn():
 *    t = tan(lat)
 *    omega = longitude * pi / 180 - central_meridian
 *
 *    utm_x:
 *    x = k0 * nu * omega * cos(lat) * (1 +
 *                                       (omega^2 / 6) * cos^2(lat) * (phi - t^2) +
 *                                       (omega^4 / 120) * cos^4(lat) * (4 * phi^3 * (1 - 6 * t^2) + phi^2 * (1 + 8*t^2) - 2*phi*t^2 + t^4) +
 *                                       (omega^6 / 5040) * cos^6(lat) * (61 - 479*t^2 + 179*t^4 - t^6))
 *    utm_x = x + 500,000
 *
 *    utm_y:
 *    y = k0 * (m + (omega^2 / 2) * nu * sin(lat) * cos(lat) +
 *                   (omega^4 / 24) * nu * sin(lat) * cos^3(lat) * (4 * phi^2 + phi - t^2) +
 *                   (omega^6 / 720) * nu * sin(lat) * cos^5(lat) * (8 * phi^4 * (11 - 24*t^2) -
 *                    28 * phi^3 * (1 - 6*t^2) + phi^2 * (1 - 32*t^2) - 2*phi*t^2 + t^4) +
 *                   (omega^8 / 40320) * nu * sin(lat) * cos^7(lat) * (1385 - 3111*t^2 + 543*t^4 - t^6))
 *    utm_y = y + 10,000,000
 *
 *    grid convergence:
 *    gamma = -omega * sin(lat) -
 *            (omega^3 / 3) * sin(lat) * cos^2(lat) * (2*phi^2 - phi) -
 *            (omega^5 / 15) * sin(lat) * cos^4(lat) * (phi^4 * (11 - 24*t^2) - phi^3 * (11 - 36*t^2) +
 *             2*phi^2 * (1 - 7*t^2) + phi*t^2) -
 *            (omega^7 / 315) * sin(lat) * cos^6(lat) * (17 - 26*t^2 + 2*t^4)
 *
 *    point scale factor:
 *    k = k0 * (1 + (omega^2 / 2) * phi * cos^2(lat) +
 *                  (omega^4 / 24) * cos^4(lat) * (4*phi^3 * (1 - 6*t^2) + phi^2 * (1 + 24*t^2) - 4*phi*t^2) +
 *                  (omega^6 / 720) * cos^6(lat) * (61 - 148*t^2 + 16*t^4))
 *
 *
 * UTM to lat/long, performed by function redfearn_inverse():
 *    Note that the ugly construct, phi'^3, represents phi' taken to the third power.  It's
 *    ugly, but adding enough parentheses to clarify it would arguably be uglier still.
 *
 *    x = utm_x - 500,000
 *    d = x / (k0 * nu')
 *    t' = tan(lat')
 *
 *    latitude:
 *    lat = lat' - (nu' * t' / rho') * (d^2 / 2) +
 *                 (nu' * t' / rho') * (d^4 / 24) * (-4*phi'^2 + 9*phi' * (1 - t'^2) + 12*t'^2) -
 *                 (nu' * t' / rho') * (d^6 / 720) * (8*phi'^4 * (11 - 24*t'^2) - 12*phi'^3 * (21 - 71*t'^2) +
 *                  15*phi'^2 * (15 - 98*t'^2 + 15*t'^4) + 180*phi' * (5*t'^2 - 3*t'^4) + 360*t'^4) +
 *                 (nu' * t' / rho') * (d^8 / 40320) * (1385 + 3633*t'^2 + 4095*t'^4 + 1575*t'^6)
 *
 *    longitude:
 *    omega = d * sec(lat') -
 *            (d^3 / 6) * sec(lat') * (phi' + 2*t'^2) +
 *            (d^5 / 120) * sec(lat') * (-4*phi'^3 * (1 - 6*t'^2) + phi'^2 * (9 - 68*t'^2) + 72*phi'*t'^2 + 24*t'^4) -
 *            (d^7 / 5040) * sec(lat') * (61 + 662*t'^2 + 1320*t'^4 + 720*t'^6)
 *
 *    long = central_meridian + omega * 180 / pi
 *
 *    grid convergence:
 *    gamma = -t' * d +
 *            (t' * d^3 / 3) * (-2*phi'^2 + 3*phi' + t'^2) -
 *            (t' * d^5 / 15) * (phi'^4 * (11 - 24*t'^2) - 3*phi'^3 * (8 - 23*t'^2) +
 *             5*phi'^2 * (3 - 14*t'^2) + 30*phi'*t'^2 + 3*t'^4) +
 *            (t' * d^7 / 315) * (17 + 77*t'^2 + 105*t'^4 + 45*t'^6)
 *
 *    point scale factor:
 *    dd = x^2 / (k0^2 * rho' * nu')
 *    k = k0 * (1 + dd/2 + (dd^2 / 24) * (4*phi' * (1 - 6*t'^2) - 3 * (1 - 16*t'^2) - 24*t'^2 / phi') + dd^3 / 720)
 *
 * Now, on to the actual code.
 *
 * Note:  redfearn_inverse() returns 0 if the conversion appears to be successful, nonzero otherwise.
 *
 * Note further:  This function has not been tuned for efficiency.
 */
int32_t
redfearn_inverse(struct datum *datum, double utm_x, double utm_y, int32_t zone, double *latitude, double *longitude)
{
	double x, y;	// UTM coordinates with false easting and northing removed and scale factor applied.
	double lat_pm;	// foot-point latitude
	double d;
	double t_pm;
	double m, m_pm;
	double nu_pm;
	double rho_pm;
	double phi_pm;
	double slat, slat_2, clat, d_2, d_3, d_4, d_5, d_6, d_7, d_8;
	double t_pm_2, t_pm_4, t_pm_6, phi_pm_2, phi_pm_3, phi_pm_4;
	int32_t i;

	x = (utm_x - 500000.0) / datum->k0;

	if ((zone > 60) || (zone == 0) || (zone < -60)) {
		return -1;
	}
	if (zone < 0)  {
		/* southern hemisphere */
		zone = -zone;
		y = (utm_y - 10000000.0) / datum->k0;
		lat_pm = -M_PI / 4.0;
	}
	else  {
		y = utm_y / datum->k0;
		lat_pm = M_PI / 4.0;
	}

	/*
	 * Find lat_pm, via iterative Newton's method.
	 * The goal is to find lat_pm, such that m == y, or equivalently
	 * to find a root of m-y.
	 */
	for (i = 0; i < 100; i++)  {
		m = datum->a * (datum->a0 * lat_pm - datum->a2 * sin(2.0 * lat_pm) + datum->a4 * sin(4.0 * lat_pm) - datum->a6 * sin(6.0 * lat_pm)) - y;
		m_pm = datum->a * (datum->a0 - datum->a2 * 2.0 * cos(2.0 * lat_pm) + datum->a4 * 4.0 * cos(4.0 * lat_pm) - datum->a6 * 6.0 * cos(6.0 * lat_pm));
		if (fabs(m / m_pm) < 1.0e-12)  {
			break;
		}
		lat_pm -= m / m_pm;
	}

	slat = sin(lat_pm);
	slat_2 = slat * slat;
	clat = sqrt(1.0 - slat_2);
	t_pm = slat / clat;

	nu_pm = datum->a / sqrt(1.0 - datum->e_2 * slat_2);
	rho_pm = datum->a * (1.0 - datum->e_2) / pow(1.0 - datum->e_2 * slat_2, 1.5);
	phi_pm = nu_pm / rho_pm;
	d = x / nu_pm;

	d_2 = d * d;
	d_3 = d_2 * d;
	d_4 = d_3 * d;
	d_5 = d_4 * d;
	d_6 = d_5 * d;
	d_7 = d_6 * d;
	d_8 = d_7 * d;
	t_pm_2 = t_pm * t_pm;
	t_pm_4 = t_pm_2 * t_pm_2;
	t_pm_6 = t_pm_2 * t_pm_4;
	phi_pm_2 = phi_pm * phi_pm;
	phi_pm_3 = phi_pm_2 * phi_pm;
	phi_pm_4 = phi_pm_3 * phi_pm;

	*latitude = lat_pm - (nu_pm * t_pm / rho_pm) * ((d_2 / 2.0) -
			 (d_4 / 24.0) * (-4.0 * phi_pm_2 + 9.0 * phi_pm * (1.0 - t_pm_2) + 12.0 * t_pm_2) +
			 (d_6 / 720.0) * (8.0 * phi_pm_4 * (11.0 - 24.0 * t_pm_2) - 12.0 * phi_pm_3 *
				(21.0 - 71.0 * t_pm_2) + 15.0 * phi_pm_2 * (15.0 - 98.0 * t_pm_2 + 15.0 * t_pm_4) +
				180.0 * phi_pm * (5.0 * t_pm_2 - 3.0 * t_pm_4) + 360.0 * t_pm_4) -
			 (d_8 / 40320.0) * (1385.0 + 3633.0 * t_pm_2 + 4095.0*t_pm_4 + 1575.0*t_pm_6));
	*longitude = d / clat - (d_3 / 6.0) * (phi_pm + 2.0 * t_pm_2) / clat +
		     (d_5 / 120.0) * (-4.0 * phi_pm_3 * (1.0 - 6.0 * t_pm_2) + phi_pm_2 * (9.0 - 68.0 * t_pm_2) +
			72.0 * phi_pm * t_pm_2 + 24.0 * t_pm_4) / clat -
		     (d_7 / 5040.0) * (61.0 + 662.0 * t_pm_2 + 1320.0 * t_pm_4 + 720.0 * t_pm_6) / clat;
	*latitude = *latitude * 180.0 / M_PI;
	*longitude = utm_zones[zone].central_meridian + *longitude * 180.0 / M_PI;

	return 0;
}

/*
 * Note:  redfearn() returns 0 if the conversion appears to be successful, nonzero otherwise.
 * Note further:  latitudes are negative south of the equator.  longitudes are negative west of the prime meridian.
 * Note further:  redfearn() returns a negative zone number for points in the southern hemisphere
 * Note further:  This function has been only partially tuned for efficiency.
 */
int32_t
redfearn(struct datum *datum, double *utm_x, double *utm_y, int32_t *zone, double latitude, double longitude, int32_t east_west)
{
	double t;
	double m;
	double nu;
	double rho;
	double phi;
	double o;
	double t_2, t_4, t_6, o_2, o_3, o_4, o_5, o_6, o_7, o_8;
	double slat, slat_2, clat, clat_2, clat_3, clat_4, clat_5, clat_6, clat_7;
	double phi_2, phi_3, phi_4;
	int32_t i;


	/*
	 * Note:  Originally the following check was
	 *
	 *    if ((latitude > 84.0) || (latitude < -80.0)) {
	 *
	 * because that is the valid range for UTM projections.
	 * However, in order to allow projections to be done for the
	 * GTOPO30 data, I changed the range to the full -90 to 90 span.
	 */
	if ((latitude > 90.0) || (latitude < -90.0)) {
		return -1;
	}
	if ((longitude > 180.0) || (longitude < -180.0)) {
		return -1;
	}

	/*
	 * Determine the zone.
	 *
	 * If the point falls on the boundary between zones,
	 * then use the parameter east_west to choose between zones.
	 * If east_west is nonzero, then choose the eastern zone.
	 * If east_west is 0, then choose the western zone.
	 * If we are on the boundary between zone 1 and zone 60, then ignore
	 * east_west and choose the zone based on the passed longitude.
	 */
	if (longitude == utm_zones[1].low_boundary)  {
		*zone = 1;
	}
	else if (longitude == utm_zones[60].high_boundary)  {
			*zone = 60;
	}
	else  {
		for (i = 1; i <= 60; i++)  {
			if (longitude == utm_zones[i].high_boundary)  {
				if (east_west == 0)  {
					*zone = i;
				}
				else  {
					*zone = i + 1;
				}
				break;
			}
			else if ((longitude > utm_zones[i].low_boundary) && (longitude < utm_zones[i].high_boundary))  {
				*zone = i;
				break;
			}
		}
	}

	o = (longitude - utm_zones[*zone].central_meridian) * M_PI / 180.0;

	latitude *= M_PI / 180.0;
	longitude *= M_PI / 180.0;
	slat = sin(latitude);
	slat_2 = slat * slat;
	clat = sqrt(1.0 - slat_2);	// cos(latitude)
	t = slat / clat;		// tan(latitude)

	t_2 = t * t;
	t_4 = t_2 * t_2;
	t_6 = t_2 * t_4;
	o_2 = o * o;
	o_3 = o_2 * o;
	o_4 = o_2 * o_2;
	o_5 = o_4 * o;
	o_6 = o_4 * o_2;
	o_7 = o_6 * o;
	o_8 = o_4 * o_4;
	clat_2 = clat * clat;
	clat_3 = clat_2 * clat;
	clat_4 = clat_2 * clat_2;
	clat_5 = clat_4 * clat;
	clat_6 = clat_4 * clat_2;
	clat_7 = clat_6 * clat;

	m = datum->a * (datum->a0 * latitude - datum->a2 * sin(2.0 * latitude) + datum->a4 * sin(4.0 * latitude) - datum->a6 * sin(6.0 * latitude));

	nu = datum->a / sqrt(1.0 - datum->e_2 * slat_2);
	rho = datum->a * (1.0 - datum->e_2) / pow(1.0 - datum->e_2 * slat_2, 1.5);
	phi = nu / rho;

	phi_2 = phi * phi;
	phi_3 = phi_2 * phi;
	phi_4 = phi_2 * phi_2;

	*utm_x = 500000.0 + datum->k0 * nu * clat * (o + (o_3 / 6.0) * clat_2 * (phi - t_2) +
			    (o_5 / 120.0) * clat_4 * (4.0 * phi_3 * (1.0 - 6.0 * t_2) +
				phi_2 * (1.0 + 8.0 * t_2) - 2.0 * phi * t_2 + t_4) +
			    (o_7 / 5040.0) * clat_6 * (61.0 - 479.0 * t_2 + 179.0 * t_4 - t_6));
	*utm_y = datum->k0 * (m + (o_2 / 2.0) * nu * slat * clat +
			   (o_4 / 24.0) * nu * slat * clat_3 * (4.0 * phi_2 + phi - t_2) +
			   (o_6 / 720.0) * nu * slat * clat_5 * (8.0 * phi_4 * (11.0 - 24.0 * t_2) -
				28.0 * phi_3 * (1.0 - 6.0 * t_2) + phi_2 * (1.0 - 32.0 * t_2) - 2.0 * phi * t_2 + t_4) +
			   (o_8 / 40320.0) * nu * slat * clat_7 * (1385.0 - 3111.0*t_2 + 543.0*t_4 - t_6));

	if (latitude < 0)  {
		/* In the southern hemisphere, we return a negative zone number. */
		*zone = -*zone;
		*utm_y += 10000000.0;
	}

	return 0;
}





/*
 * Check the type of swabbing needed on this machine.
 */
int32_t swab_type()
{
	union swabtest {
		uint32_t l;
		unsigned char c[4];
	} swabtest;

	swabtest.l = 0xaabbccdd;

	if ((swabtest.c[0] == 0xaa) && (swabtest.c[1] == 0xbb) &&
	    (swabtest.c[2] == 0xcc) && (swabtest.c[3] == 0xdd))  {
		/* BIG_ENDIAN: Do nothing */
		return 0;
	}
	else if ((swabtest.c[0] == 0xdd) && (swabtest.c[1] == 0xcc) &&
	    (swabtest.c[2] == 0xbb) && (swabtest.c[3] == 0xaa))  {
		/* LITTLE_ENDIAN */
		return 1;
	}
	else if ((swabtest.c[0] == 0xbb) && (swabtest.c[1] == 0xaa) &&
	    (swabtest.c[2] == 0xdd) && (swabtest.c[3] == 0xcc))  {
		/* PDP_ENDIAN */
		return 2;
	}
	else  {
		/* Unknown */
		return -1;
	}
}
