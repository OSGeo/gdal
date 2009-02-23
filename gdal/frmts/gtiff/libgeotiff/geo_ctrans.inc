/******************************************************************************
 * $Id: geo_ctrans.inc 657 2005-03-04 03:59:11Z fwarmerdam $
 *
 * Project:  libgeotiff
 * Purpose:  GeoTIFF Projection Method codes. 
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.3  2005/03/04 03:59:11  fwarmerdam
 * Added header.
 *
 */

/* C database for Geotiff include files.   */
/* the macro ValuePair() must be defined   */
/* by the enclosing include file           */

/*
 *  Revised 12 Jul 1995   NDR -- changed South Oriented to a code 
 *  Revised 28 Sep 1995   NDR -- Added Rev. 1.0 aliases. 
 */

ValuePair(CT_TransverseMercator,	1)
ValuePair(CT_TransvMercator_Modified_Alaska, 2)
ValuePair(CT_ObliqueMercator,	3)
ValuePair(CT_ObliqueMercator_Laborde,	4)
ValuePair(CT_ObliqueMercator_Rosenmund,	5)
ValuePair(CT_ObliqueMercator_Spherical,	6)   /* not advisable */
ValuePair(CT_Mercator,	7)
ValuePair(CT_LambertConfConic_2SP,	8)
ValuePair(CT_LambertConfConic,CT_LambertConfConic_2SP)         /* Alias */
ValuePair(CT_LambertConfConic_1SP,	9)
ValuePair(CT_LambertConfConic_Helmert,CT_LambertConfConic_1SP) /* alias */
ValuePair(CT_LambertAzimEqualArea,	10)
ValuePair(CT_AlbersEqualArea,	11)
ValuePair(CT_AzimuthalEquidistant,	12)
ValuePair(CT_EquidistantConic,	13)
ValuePair(CT_Stereographic,	14)
ValuePair(CT_PolarStereographic,	15)
ValuePair(CT_ObliqueStereographic,	16)   /* not advisable */
ValuePair(CT_Equirectangular,	17)
ValuePair(CT_CassiniSoldner,	18)
ValuePair(CT_Gnomonic,	19)
ValuePair(CT_MillerCylindrical,	20)
ValuePair(CT_Orthographic,	21)
ValuePair(CT_Polyconic,	22)
ValuePair(CT_Robinson,	23)
ValuePair(CT_Sinusoidal,	24)
ValuePair(CT_VanDerGrinten,	25)
ValuePair(CT_NewZealandMapGrid,	26)
/* Added for 1.0 */
ValuePair(CT_TransvMercator_SouthOrientated, 27)

/* Added Feb 2005 */
ValuePair(CT_CylindricalEqualArea, 28)


/* Aliases */

ValuePair(CT_SouthOrientedGaussConformal,CT_TransvMercator_SouthOrientated)
ValuePair(CT_AlaskaConformal,	CT_TransvMercator_Modified_Alaska)
ValuePair(CT_TransvEquidistCylindrical,	CT_CassiniSoldner)
ValuePair(CT_ObliqueMercator_Hotine,	CT_ObliqueMercator)
ValuePair(CT_SwissObliqueCylindrical,	CT_ObliqueMercator_Rosenmund)
ValuePair(CT_GaussBoaga,	CT_TransverseMercator)
ValuePair(CT_GaussKruger,	CT_TransverseMercator)
ValuePair(CT_TransvMercator_SouthOriented, CT_TransvMercator_SouthOrientated)


