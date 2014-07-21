/**********************************************************************
 * $Id: mitab_coordsys.cpp,v 1.42 2011-06-11 00:35:00 fwarmerdam Exp $
 *
 * Name:     mitab_coordsys.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation translation between MIF CoordSys format, and
 *           and OGRSpatialRef format.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 1999-2001, Frank Warmerdam
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************
 *
 * $Log: mitab_coordsys.cpp,v $
 * Revision 1.42  2011-06-11 00:35:00  fwarmerdam
 * add support for reading google mercator (#4115)
 *
 * Revision 1.41  2010-10-07 18:46:26  aboudreault
 * Fixed bad use of atof when locale setting doesn't use . for float (GDAL bug #3775)
 *
 * Revision 1.40  2010-09-07 16:48:08  aboudreault
 * Removed incomplete patch for affine params support in mitab. (bug 1155)
 *
 * Revision 1.39  2010-07-07 19:00:15  aboudreault
 * Cleanup Win32 Compile Warnings (GDAL bug #2930)
 *
 * Revision 1.38  2010-07-05 18:32:48  aboudreault
 * Fixed memory leaks in mitab_capi.cpp and mitab_coordsys.cpp
 *
 * Revision 1.37  2010-07-05 17:20:14  aboudreault
 * Added Krovak projection suppoprt (bug 2230)
 *
 * Revision 1.36  2007-11-21 21:15:45  dmorissette
 * Fix asDatumInfoList[] and asSpheroidInfoList[] defns/refs (bug 1826)
 *
 * Revision 1.35  2007/06/21 13:23:43  fwarmerdam
 * Fixed support for predefined datums with non-greenwich prime meridians
 *
 * Revision 1.34  2006/03/10 19:50:45  fwarmerdam
 * Coordsys false easting and northing are in the units of the coordsys, not
 * necessarily meters.  Adjusted mitab_coordsys.cpp to reflect this.
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=1113
 *
 * Revision 1.33  2005/09/29 20:13:57  dmorissette
 * MITABCoordSys2SpatialRef() patches from Anthony D (bug 1155):
 * Improved support for modified TM projections 21-24.
 * Added support for affine parameters (inside #ifdef MITAB_AFFINE_PARAMS since
 * affine params cannot be stored directly in OGRSpatialReference)
 *
 * Revision 1.32  2005/08/07 21:00:38  fwarmerdam
 * Initialize adfDatumParm[] to avoid warnings with gcc 4.
 *
 * Revision 1.31  2005/05/12 22:07:52  dmorissette
 * Improved handling of Danish modified TM proj#21-24 (hss, bugs 976,1010)
 *
 * Revision 1.30  2005/03/22 23:24:54  dmorissette
 * Added support for datum id in .MAP header (bug 910)
 *
 * Revision 1.29  2004/06/03 19:36:53  fwarmerdam
 * fixed memory leak processing non-earth coordsys
 *
 * Revision 1.28  2003/03/21 14:20:42  warmerda
 * fixed up regional mercator handling, was screwing up transverse mercator
 *
 * Revision 1.27  2003/01/09 17:33:26  warmerda
 * fixed ellipsoid extraction for datum 999/9999
 *
 * Revision 1.26  2002/12/12 20:12:18  warmerda
 * fixed signs of rotational parameters for TOWGS84 in WKT
 *
 * Revision 1.25  2002/10/15 14:33:30  warmerda
 * Added untested support in mitab_spatialref.cpp, and mitab_coordsys.cpp for
 * projections Regional Mercator (26), Polyconic (27), Azimuthal Equidistant -
 * All origin latitudes (28), and Lambert Azimuthal Equal Area - any aspect (29).
 *
 * Revision 1.24  2002/09/23 13:16:04  warmerda
 * fixed leak in MITABExtractCoordSysBounds()
 *
 * Revision 1.23  2002/04/01 19:49:24  warmerda
 * added support for cassini/soldner - proj 30
 *
 * Revision 1.22  2002/03/01 19:00:15  warmerda
 * False Easting/Northing should be in the linear units of measure in MapInfo,
 * but in OGRSpatialReference/WKT they are always in meters.  Convert accordingly.
 *
 * Revision 1.21  2001/04/04 21:43:19  warmerda
 * added code to set WGS84 values
 *
 * Revision 1.20  2001/01/23 21:23:42  daniel
 * Added projection bounds lookup table, called from TABFile::SetProjInfo()
 *
 * Revision 1.19  2001/01/22 16:00:53  warmerda
 * reworked swiss projection support
 *
 * Revision 1.18  2001/01/19 21:56:18  warmerda
 * added untested support for Swiss Oblique Mercator
 **********************************************************************/

#include "mitab.h"
#include "mitab_utils.h"

extern const MapInfoDatumInfo asDatumInfoList[];
extern const MapInfoSpheroidInfo asSpheroidInfoList[];

/************************************************************************/
/*                             GetMIFParm()                             */
/************************************************************************/

static double GetMIFParm( char ** papszFields, int iField, double dfDefault )

{
    if( iField >= CSLCount(papszFields) )
        return dfDefault;
    else
        return atof(papszFields[iField]);
}

/************************************************************************/
/*                      MITABCoordSys2SpatialRef()                      */
/*                                                                      */
/*      Convert a MIF COORDSYS string into a new OGRSpatialReference    */
/*      object.                                                         */
/************************************************************************/

OGRSpatialReference *MITABCoordSys2SpatialRef( const char * pszCoordSys )

{
    char        **papszFields;
    OGRSpatialReference *poSR;

    if( pszCoordSys == NULL )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Parse the passed string into words.                             */
/* -------------------------------------------------------------------- */
    while(*pszCoordSys == ' ') pszCoordSys++;  // Eat leading spaces
    if( EQUALN(pszCoordSys,"CoordSys",8) )
        pszCoordSys += 9;
    
    papszFields = CSLTokenizeStringComplex( pszCoordSys, " ,", TRUE, FALSE );

/* -------------------------------------------------------------------- */
/*      Clip off Bounds information.                                    */
/* -------------------------------------------------------------------- */
    int         iBounds = CSLFindString( papszFields, "Bounds" );

    while( iBounds != -1 && papszFields[iBounds] != NULL )
    {
        CPLFree( papszFields[iBounds] );
        papszFields[iBounds] = NULL;
        iBounds++;
    }

/* -------------------------------------------------------------------- */
/*      Create a spatialreference object to operate on.                 */
/* -------------------------------------------------------------------- */
    poSR = new OGRSpatialReference;

/* -------------------------------------------------------------------- */
/*      Fetch the projection.                                           */
/* -------------------------------------------------------------------- */
    char        **papszNextField;
    int nProjection = 0;

    if( CSLCount( papszFields ) >= 3
        && EQUAL(papszFields[0],"Earth")
        && EQUAL(papszFields[1],"Projection") )
    {
        nProjection = atoi(papszFields[2]);
        papszNextField = papszFields + 3;
    }
    else if (CSLCount( papszFields ) >= 2
             && EQUAL(papszFields[0],"NonEarth") )
    {
        // NonEarth Units "..." Bounds (x, y) (x, y)
        nProjection = 0;
        papszNextField = papszFields + 2;

        if( papszNextField[0] != NULL && EQUAL(papszNextField[0],"Units") )
            papszNextField++;
    }
    else
    {
        // Invalid projection string ???
        if (CSLCount(papszFields) > 0)
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "Failed parsing CoordSys: '%s'", pszCoordSys);
        CSLDestroy(papszFields);
        delete poSR;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Fetch the datum information.                                    */
/* -------------------------------------------------------------------- */
    int         nDatum = 0;
    double      adfDatumParm[8] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};
    int         nEllipsoid=0;

    if( nProjection != 0 && CSLCount(papszNextField) > 0 )
    {
        nDatum = atoi(papszNextField[0]);
        papszNextField++;
    }

    if( (nDatum == 999 || nDatum == 9999)
        && CSLCount(papszNextField) >= 4 )
    {
        nEllipsoid = atoi(papszNextField[0]);
        adfDatumParm[0] = atof(papszNextField[1]);
        adfDatumParm[1] = atof(papszNextField[2]);
        adfDatumParm[2] = atof(papszNextField[3]);
        papszNextField += 4;
    }

    if( nDatum == 9999
        && CSLCount(papszNextField) >= 5 )
    {
        adfDatumParm[3] = atof(papszNextField[0]);
        adfDatumParm[4] = atof(papszNextField[1]);
        adfDatumParm[5] = atof(papszNextField[2]);
        adfDatumParm[6] = atof(papszNextField[3]);
        adfDatumParm[7] = atof(papszNextField[4]);
        papszNextField += 5;
    }

/* -------------------------------------------------------------------- */
/*      Fetch the units string.                                         */
/* -------------------------------------------------------------------- */
    const char  *pszMIFUnits = NULL;
    const char  *pszUnitsName = NULL;
    double dfUnitsConv = 1.0;
    
    if( CSLCount(papszNextField) > 0 )
    {
        pszMIFUnits = papszNextField[0];
        papszNextField++;
    }

    if( nProjection == 1 || pszMIFUnits == NULL )
        /* do nothing */;
    else if( EQUAL(pszMIFUnits,"km") )
    {
        pszUnitsName = "Kilometer"; 
        dfUnitsConv = 1000.0;
    }
    else if( EQUAL(pszMIFUnits, "in" ) )
    {
        pszUnitsName = "IINCH"; 
        dfUnitsConv = 0.0254; 
    }
    else if( EQUAL(pszMIFUnits, "ft" ) )
    {
        pszUnitsName = SRS_UL_FOOT;
        dfUnitsConv = CPLAtof(SRS_UL_FOOT_CONV);
    }
    else if( EQUAL(pszMIFUnits, "yd" ) )
    {
        pszUnitsName = "IYARD";
        dfUnitsConv = 0.9144;
    }
    else if( EQUAL(pszMIFUnits, "mm" ) )
    {
        pszUnitsName = "Millimeter";
        dfUnitsConv = 0.001;
    }
    else if( EQUAL(pszMIFUnits, "cm" ) )
    {
        pszUnitsName = "Centimeter";
        dfUnitsConv = 0.01;
    }
    else if( EQUAL(pszMIFUnits, "m" ) )
    {
        pszUnitsName = SRS_UL_METER;
        dfUnitsConv = 1.0;
    }   
    else if( EQUAL(pszMIFUnits, "survey foot" )
             || EQUAL(pszMIFUnits, "survey ft" ) )
    {
        pszUnitsName = SRS_UL_US_FOOT;
        dfUnitsConv = CPLAtof(SRS_UL_US_FOOT_CONV);
    }   
    else if( EQUAL(pszMIFUnits, "nmi" ) )
    {
        pszUnitsName = SRS_UL_NAUTICAL_MILE;
        dfUnitsConv = CPLAtof(SRS_UL_NAUTICAL_MILE_CONV);
    }   
    else if( EQUAL(pszMIFUnits, "li" ) )
    {
        pszUnitsName = SRS_UL_LINK;
        dfUnitsConv = CPLAtof(SRS_UL_LINK_CONV);
    }
    else if( EQUAL(pszMIFUnits, "ch" ) )
    {
        pszUnitsName = SRS_UL_CHAIN;
        dfUnitsConv = CPLAtof(SRS_UL_CHAIN_CONV);
    }   
    else if( EQUAL(pszMIFUnits, "rd" ) )
    {
        pszUnitsName = SRS_UL_ROD;
        dfUnitsConv = CPLAtof(SRS_UL_ROD);
    }   
    else if( EQUAL(pszMIFUnits, "mi" ) )
    {
        pszUnitsName = "Mile";
        dfUnitsConv = 1609.344;
    }

/* -------------------------------------------------------------------- */
/*      Handle the PROJCS style projections, but add the datum          */
/*      later.                                                          */
/*                                                                      */
/*      Note that per GDAL bug 1113 the false easting and north are     */
/*      in local units, not necessarily meters.                         */
/* -------------------------------------------------------------------- */
    int nBaseProjection = nProjection;
    if (nBaseProjection>=3000) nBaseProjection -=3000;
    else if (nBaseProjection>=2000) nBaseProjection -=2000;
    else if (nBaseProjection>=1000) nBaseProjection -=1000;
    switch( nBaseProjection )
    {
        /*--------------------------------------------------------------
         * NonEarth ... we return with an empty SpatialRef.  Eventually
         * we might want to include the units, but not for now.
         *
         * __TODO__ Changed to return NULL because returning an empty
         * SpatialRef caused confusion between Latlon and NonEarth since
         * empty SpatialRefs do have a GEOGCS set and makes them look like
         * Lat/Lon SpatialRefs.
         *
         * Ideally we would like to return a SpatialRef whith no GEGOCS
         *-------------------------------------------------------------*/
      case 0:
        poSR->SetLocalCS( "Nonearth" );
        break;

        /*--------------------------------------------------------------
         * lat/long .. just add the GEOGCS later.
         *-------------------------------------------------------------*/
      case 1:
        break;

        /*--------------------------------------------------------------
         * Cylindrical Equal Area
         *-------------------------------------------------------------*/
      case 2:
        poSR->SetCEA( GetMIFParm( papszNextField, 1, 0.0 ),
                      GetMIFParm( papszNextField, 0, 0.0 ),
                      GetMIFParm( papszNextField, 2, 0.0 ),
                      GetMIFParm( papszNextField, 3, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * Lambert Conic Conformal
         *-------------------------------------------------------------*/
      case 3:
        poSR->SetLCC( GetMIFParm( papszNextField, 2, 0.0 ),
                      GetMIFParm( papszNextField, 3, 0.0 ),
                      GetMIFParm( papszNextField, 1, 0.0 ),
                      GetMIFParm( papszNextField, 0, 0.0 ),
                      GetMIFParm( papszNextField, 4, 0.0 ),
                      GetMIFParm( papszNextField, 5, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * Lambert Azimuthal Equal Area
         *-------------------------------------------------------------*/
      case 4: 
      case 29:
        poSR->SetLAEA( GetMIFParm( papszNextField, 1, 0.0 ),
                       GetMIFParm( papszNextField, 0, 0.0 ),
                       0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Azimuthal Equidistant 
         *-------------------------------------------------------------*/
      case 5:  /* polar aspect only */
      case 28: /* all aspects */
        poSR->SetAE( GetMIFParm( papszNextField, 1, 0.0 ),
                     GetMIFParm( papszNextField, 0, 0.0 ),
                     0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Equidistant Conic
         *-------------------------------------------------------------*/
      case 6:
        poSR->SetEC( GetMIFParm( papszNextField, 2, 0.0 ),
                     GetMIFParm( papszNextField, 3, 0.0 ),
                     GetMIFParm( papszNextField, 1, 0.0 ),
                     GetMIFParm( papszNextField, 0, 0.0 ),
                     GetMIFParm( papszNextField, 4, 0.0 ),
                     GetMIFParm( papszNextField, 5, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * Hotine Oblique Mercator
         *-------------------------------------------------------------*/
      case 7:
        poSR->SetHOM( GetMIFParm( papszNextField, 1, 0.0 ),
                      GetMIFParm( papszNextField, 0, 0.0 ),
                      GetMIFParm( papszNextField, 2, 0.0 ),
                      90.0,
                      GetMIFParm( papszNextField, 3, 1.0 ),
                      GetMIFParm( papszNextField, 4, 0.0 ),
                      GetMIFParm( papszNextField, 5, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * Transverse Mercator
         *-------------------------------------------------------------*/
      case 8:
        poSR->SetTM( GetMIFParm( papszNextField, 1, 0.0 ),
                     GetMIFParm( papszNextField, 0, 0.0 ),
                     GetMIFParm( papszNextField, 2, 1.0 ),
                     GetMIFParm( papszNextField, 3, 0.0 ),
                     GetMIFParm( papszNextField, 4, 0.0 ) );
        break;

        /*----------------------------------------------------------------
         * Transverse Mercator,(modified for Danish System 34 Jylland-Fyn)
         *---------------------------------------------------------------*/
      case 21:
        poSR->SetTMVariant( SRS_PT_TRANSVERSE_MERCATOR_MI_21,
                            GetMIFParm( papszNextField, 1, 0.0 ),
                            GetMIFParm( papszNextField, 0, 0.0 ),
                            GetMIFParm( papszNextField, 2, 1.0 ),
                            GetMIFParm( papszNextField, 3, 0.0 ),
                            GetMIFParm( papszNextField, 4, 0.0 ));
        break;

        /*--------------------------------------------------------------
         * Transverse Mercator,(modified for Danish System 34 Sjaelland)
         *-------------------------------------------------------------*/
      case 22:
        poSR->SetTMVariant( SRS_PT_TRANSVERSE_MERCATOR_MI_22,
                            GetMIFParm( papszNextField, 1, 0.0 ),
                            GetMIFParm( papszNextField, 0, 0.0 ),
                            GetMIFParm( papszNextField, 2, 1.0 ),
                            GetMIFParm( papszNextField, 3, 0.0 ),
                            GetMIFParm( papszNextField, 4, 0.0 ));
        break;

        /*----------------------------------------------------------------
         * Transverse Mercator,(modified for Danish System 34/45 Bornholm)
         *---------------------------------------------------------------*/
      case 23:
        poSR->SetTMVariant( SRS_PT_TRANSVERSE_MERCATOR_MI_23,
                            GetMIFParm( papszNextField, 1, 0.0 ),
                            GetMIFParm( papszNextField, 0, 0.0 ),
                            GetMIFParm( papszNextField, 2, 1.0 ),
                            GetMIFParm( papszNextField, 3, 0.0 ),
                            GetMIFParm( papszNextField, 4, 0.0 ));
        break;

        /*--------------------------------------------------------------
         * Transverse Mercator,(modified for Finnish KKJ)
         *-------------------------------------------------------------*/
      case 24:
        poSR->SetTMVariant( SRS_PT_TRANSVERSE_MERCATOR_MI_24,
                            GetMIFParm( papszNextField, 1, 0.0 ),
                            GetMIFParm( papszNextField, 0, 0.0 ),
                            GetMIFParm( papszNextField, 2, 1.0 ),
                            GetMIFParm( papszNextField, 3, 0.0 ),
                            GetMIFParm( papszNextField, 4, 0.0 ));
        break;

        /*--------------------------------------------------------------
         * Albers Conic Equal Area
         *-------------------------------------------------------------*/
      case 9:
        poSR->SetACEA( GetMIFParm( papszNextField, 2, 0.0 ),
                       GetMIFParm( papszNextField, 3, 0.0 ),
                       GetMIFParm( papszNextField, 1, 0.0 ),
                       GetMIFParm( papszNextField, 0, 0.0 ),
                       GetMIFParm( papszNextField, 4, 0.0 ),
                       GetMIFParm( papszNextField, 5, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * Mercator
         *-------------------------------------------------------------*/
      case 10:
        poSR->SetMercator( 0.0, GetMIFParm( papszNextField, 0, 0.0 ),
                           1.0, 0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Miller Cylindrical
         *-------------------------------------------------------------*/
      case 11:
        poSR->SetMC( 0.0, GetMIFParm( papszNextField, 0, 0.0 ),
                     0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Robinson
         *-------------------------------------------------------------*/
      case 12:
        poSR->SetRobinson( GetMIFParm( papszNextField, 0, 0.0 ),
                           0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Mollweide
         *-------------------------------------------------------------*/
      case 13:
        poSR->SetMollweide( GetMIFParm( papszNextField, 0, 0.0 ),
                            0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Eckert IV
         *-------------------------------------------------------------*/
      case 14:
        poSR->SetEckertIV( GetMIFParm( papszNextField, 0, 0.0 ),
                           0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Eckert VI
         *-------------------------------------------------------------*/
      case 15:
        poSR->SetEckertVI( GetMIFParm( papszNextField, 0, 0.0 ),
                           0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Sinusoidal
         *-------------------------------------------------------------*/
      case 16:
        poSR->SetSinusoidal( GetMIFParm( papszNextField, 0, 0.0 ),
                             0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Gall
         *-------------------------------------------------------------*/
      case 17:
        poSR->SetGS( GetMIFParm( papszNextField, 0, 0.0 ),
                     0.0, 0.0 );
        break;
        
        /*--------------------------------------------------------------
         * New Zealand Map Grid
         *-------------------------------------------------------------*/
      case 18:
        poSR->SetNZMG( GetMIFParm( papszNextField, 1, 0.0 ),
                       GetMIFParm( papszNextField, 0, 0.0 ),
                       GetMIFParm( papszNextField, 2, 0.0 ),
                       GetMIFParm( papszNextField, 3, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * Lambert Conic Conformal (Belgium)
         *-------------------------------------------------------------*/
      case 19:
        poSR->SetLCCB( GetMIFParm( papszNextField, 2, 0.0 ),
                       GetMIFParm( papszNextField, 3, 0.0 ),
                       GetMIFParm( papszNextField, 1, 0.0 ),
                       GetMIFParm( papszNextField, 0, 0.0 ),
                       GetMIFParm( papszNextField, 4, 0.0 ),
                       GetMIFParm( papszNextField, 5, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * Stereographic
         *-------------------------------------------------------------*/
      case 20:
      case 31: /* double stereographic */
        poSR->SetStereographic( 
            GetMIFParm( papszNextField, 1, 0.0 ),
            GetMIFParm( papszNextField, 0, 0.0 ),
            GetMIFParm( papszNextField, 2, 1.0 ),
            GetMIFParm( papszNextField, 3, 0.0 ),
            GetMIFParm( papszNextField, 4, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * Swiss Oblique Mercator / Cylindrical
         *-------------------------------------------------------------*/
      case 25:
        poSR->SetSOC( GetMIFParm( papszNextField, 1, 0.0 ),
                      GetMIFParm( papszNextField, 0, 0.0 ),
                      GetMIFParm( papszNextField, 2, 0.0 ),
                      GetMIFParm( papszNextField, 3, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * Regional Mercator
         *-------------------------------------------------------------*/
      case 26:
        poSR->SetMercator( GetMIFParm( papszNextField, 1, 0.0 ), 
                           GetMIFParm( papszNextField, 0, 0.0 ),
                           1.0, 0.0, 0.0 );
        break;

        /*--------------------------------------------------------------
         * Polygonic
         *-------------------------------------------------------------*/
      case 27:
        poSR->SetPolyconic( GetMIFParm( papszNextField, 1, 0.0 ), 
                            GetMIFParm( papszNextField, 0, 0.0 ),
                          GetMIFParm( papszNextField, 2, 0.0 ),
                          GetMIFParm( papszNextField, 3, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * CassiniSoldner
         *-------------------------------------------------------------*/
      case 30:
        poSR->SetCS( 
            GetMIFParm( papszNextField, 1, 0.0 ),
            GetMIFParm( papszNextField, 0, 0.0 ),
            GetMIFParm( papszNextField, 2, 0.0 ),
            GetMIFParm( papszNextField, 3, 0.0 ) );
        break;

        /*--------------------------------------------------------------
         * Krovak
         *-------------------------------------------------------------*/
       case 32:
         poSR->SetKrovak( GetMIFParm( papszNextField, 1, 0.0 ),  // dfCenterLat
                          GetMIFParm( papszNextField, 0, 0.0 ),  // dfCenterLong
                          GetMIFParm( papszNextField, 3, 1.0 ),  // dfAzimuth
                          GetMIFParm( papszNextField, 2, 0.0 ),  // dfPseudoStdParallelLat
                          1.0,									  // dfScale
                          GetMIFParm( papszNextField, 4, 0.0 ),  // dfFalseEasting
                          GetMIFParm( papszNextField, 5, 0.0 )); // dfFalseNorthing
         break;

     /*--------------------------------------------------------------
      * Equidistant Cylindrical / Equirectangular
      *-------------------------------------------------------------*/
      case 33:
        poSR->SetEquirectangular(
            GetMIFParm( papszNextField, 1, 0.0 ),
            GetMIFParm( papszNextField, 0, 0.0 ),
            GetMIFParm( papszNextField, 2, 0.0 ),
            GetMIFParm( papszNextField, 3, 0.0 ) );
        break;

      default:
        break;
    }

/* -------------------------------------------------------------------- */
/*      Set linear units.                                               */
/* -------------------------------------------------------------------- */
    if( pszUnitsName != NULL )
        poSR->SetLinearUnits( pszUnitsName, dfUnitsConv );

/* -------------------------------------------------------------------- */
/*      For Non-Earth projection, we're done at this point.             */
/* -------------------------------------------------------------------- */
    if (nProjection == 0)
    {
        CSLDestroy(papszFields);
        return poSR;
    }

/* ==================================================================== */
/*      Establish the GeogCS                                            */
/* ==================================================================== */
    const char *pszGeogName = "unnamed";
    const char *pszSpheroidName = "GRS_1980";
    double      dfSemiMajor = 6378137.0;
    double      dfInvFlattening = 298.257222101;
    const char *pszPrimeM = "Greenwich";
    double      dfPMLongToGreenwich = 0.0;

/* -------------------------------------------------------------------- */
/*      Find the datum, and collect it's parameters if possible.        */
/* -------------------------------------------------------------------- */
    int         iDatum;
    const MapInfoDatumInfo *psDatumInfo = NULL;
    
    for( iDatum = 0; asDatumInfoList[iDatum].nMapInfoDatumID != -1; iDatum++ )
    {
        if( asDatumInfoList[iDatum].nMapInfoDatumID == nDatum )
        {
            psDatumInfo = asDatumInfoList + iDatum;
            break;
        }
    }

    if( asDatumInfoList[iDatum].nMapInfoDatumID == -1
        && nDatum != 999 && nDatum != 9999 )
    {
        /* use WGS84 */
        psDatumInfo = asDatumInfoList + 0;
    }

    if( psDatumInfo != NULL )
    {
        nEllipsoid = psDatumInfo->nEllipsoid;
        adfDatumParm[0] =  psDatumInfo->dfShiftX;
        adfDatumParm[1] = psDatumInfo->dfShiftY;
        adfDatumParm[2] = psDatumInfo->dfShiftZ;
        adfDatumParm[3] = psDatumInfo->dfDatumParm0;
        adfDatumParm[4] = psDatumInfo->dfDatumParm1;
        adfDatumParm[5] = psDatumInfo->dfDatumParm2;
        adfDatumParm[6] = psDatumInfo->dfDatumParm3;
        adfDatumParm[7] = psDatumInfo->dfDatumParm4;
    }
    
/* -------------------------------------------------------------------- */
/*      Set the spheroid if it is known from the table.                 */
/* -------------------------------------------------------------------- */
    for( int i = 0; asSpheroidInfoList[i].nMapInfoId != -1; i++ )
    {
        if( asSpheroidInfoList[i].nMapInfoId == nEllipsoid )
        {
            dfSemiMajor = asSpheroidInfoList[i].dfA;
            dfInvFlattening = asSpheroidInfoList[i].dfInvFlattening;
            pszSpheroidName = asSpheroidInfoList[i].pszMapinfoName;
            break;
        }
    }

/* -------------------------------------------------------------------- */
/*      apply datum parameters.                                         */
/* -------------------------------------------------------------------- */
    char        szDatumName[128];

    if( nDatum == 999 )
    {
        sprintf( szDatumName,
                 "MIF 9999,%d,%.15g,%.15g,%.15g",
                 nEllipsoid,
                 adfDatumParm[0],
                 adfDatumParm[1],
                 adfDatumParm[2] );
    }
    else if( nDatum == 9999 )
    {
        sprintf( szDatumName,
                 "MIF 9999,%d,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g,%.15g",
                 nEllipsoid,
                 adfDatumParm[0],
                 adfDatumParm[1],
                 adfDatumParm[2],
                 adfDatumParm[3],
                 adfDatumParm[4],
                 adfDatumParm[5],
                 adfDatumParm[6],
                 adfDatumParm[7] );
    }
    else if( psDatumInfo->pszOGCDatumName != NULL
             && strlen(psDatumInfo->pszOGCDatumName) > 0 )
    {
        strncpy( szDatumName, psDatumInfo->pszOGCDatumName,
                 sizeof(szDatumName) );
    }
    else
    {
        sprintf( szDatumName, "MIF %d", nDatum );
    }

/* -------------------------------------------------------------------- */
/*      Set prime meridian for 9999 datums.                             */
/* -------------------------------------------------------------------- */
    if( nDatum == 9999 || adfDatumParm[7] != 0.0 )
    {
        pszPrimeM = "non-Greenwich";
        dfPMLongToGreenwich = adfDatumParm[7];
    }

/* -------------------------------------------------------------------- */
/*      Set the GeogCS.                                                 */
/* -------------------------------------------------------------------- */
    poSR->SetGeogCS( pszGeogName, szDatumName, pszSpheroidName,
                     dfSemiMajor, dfInvFlattening,
                     pszPrimeM, dfPMLongToGreenwich,
                     SRS_UA_DEGREE,
                     CPLAtof(SRS_UA_DEGREE_CONV) );

    poSR->SetTOWGS84( adfDatumParm[0], adfDatumParm[1], adfDatumParm[2],
                      adfDatumParm[3] == 0.0 ? 0.0 : -adfDatumParm[3],
                      adfDatumParm[4] == 0.0 ? 0.0 : -adfDatumParm[4],
                      adfDatumParm[5] == 0.0 ? 0.0 : -adfDatumParm[5], 
                      adfDatumParm[6] );

    /*-----------------------------------------------------------------
     * Special case for Google Mercator (datum=157, ellipse=54, gdal #4115)
     *----------------------------------------------------------------*/

    if( nBaseProjection == 10 && nDatum == 157 )
    {
        poSR->SetNode( "PROJCS", "WGS 84 / Pseudo-Mercator" );
        poSR->SetExtension( "PROJCS", "PROJ4", "+proj=merc +a=6378137 +b=6378137 +lat_ts=0.0 +lon_0=0.0 +x_0=0.0 +y_0=0 +k=1.0 +units=m +nadgrids=@null +wktext  +no_defs" );
    }

    /*-----------------------------------------------------------------
     * Special case for France Lambert-93
     *----------------------------------------------------------------*/
    if( nBaseProjection == 3
        && nDatum == 33
        && nEllipsoid == 0
        && poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0) == 3.0
        && poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0) == 46.5 )
    {
        poSR->SetNode( "PROJCS", "RGF93 / Lambert-93" );
        poSR->SetNode( "PROJCS|GEOGCS", "RGF93");
        poSR->SetNode( "PROJCS|GEOGCS|DATUM", "Reseau_Geodesique_Francais_1993");
    }

/* -------------------------------------------------------------------- */
/*      Report on translation.                                          */
/* -------------------------------------------------------------------- */
    char        *pszWKT;

    poSR->exportToWkt( &pszWKT );
    if( pszWKT != NULL )
    {
        CPLDebug( "MITAB",
                  "This CoordSys value:\n%s\nwas translated to:\n%s\n",
                  pszCoordSys, pszWKT );
        CPLFree( pszWKT );
    }

    CSLDestroy(papszFields);

    return poSR;
}

/************************************************************************/
/*                      MITABSpatialRef2CoordSys()                      */
/*                                                                      */
/*      Converts a OGRSpatialReference object into a MIF COORDSYS       */
/*      string.                                                         */
/*                                                                      */
/*      The function returns a newly allocated string that should be    */
/*      CPLFree()'d by the caller.                                      */
/************************************************************************/

char *MITABSpatialRef2CoordSys( OGRSpatialReference * poSR )

{
    if( poSR == NULL )
        return NULL;
    
    /*-----------------------------------------------------------------
     * Get the linear units.
     *----------------------------------------------------------------*/
    double      dfLinearConv;
    char        *pszLinearUnits;

    dfLinearConv = poSR->GetLinearUnits( &pszLinearUnits );

    /*-----------------------------------------------------------------
     * Transform the projection and projection parameters.
     *----------------------------------------------------------------*/
    const char *pszProjection = poSR->GetAttrValue("PROJECTION");
    double      parms[10];
    int         nProjection = 0;
    int         nParmCount = 0;

    if( pszProjection == NULL )
    {
        /*--------------------------------------------------------------
         * NULL projection.  
         * We have 2 possibilities: CoordSys NonEarth or Lat/Lon 
         * NonEarth ... is an empty SpatialRef.  
         * Lat/Lon has no "PROJECTION" but GEOGCS is set
         *-------------------------------------------------------------*/
         if ( poSR->GetAttrValue("GEOGCS") == NULL)
            nProjection = 0; // Non-Earth
        else
            nProjection = 1; // Lat/Lon
    }
    else if( EQUAL(pszProjection,SRS_PT_ALBERS_CONIC_EQUAL_AREA) )
    {
        nProjection = 9;
        parms[0] = poSR->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        parms[3] = poSR->GetProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0);
        parms[4] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[5] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 6;
    }

    else if( EQUAL(pszProjection,SRS_PT_AZIMUTHAL_EQUIDISTANT) )
    {
        nProjection = 5;
        parms[0] = poSR->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0);
        parms[2] = 90.0;
        nParmCount = 3;

        if( ABS((ABS(parms[1]) - 90)) > 0.001 )
            nProjection = 28;
    }

    else if( EQUAL(pszProjection,SRS_PT_CYLINDRICAL_EQUAL_AREA) )
    {
        nProjection = 2;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        nParmCount = 2;
    }

    else if( EQUAL(pszProjection,SRS_PT_ECKERT_IV) )
    {
        nProjection = 14;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        nParmCount = 1;
    }

    else if( EQUAL(pszProjection,SRS_PT_ECKERT_VI) )
    {
        nProjection = 15;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        nParmCount = 1;
    }

    else if( EQUAL(pszProjection,SRS_PT_EQUIDISTANT_CONIC) )
    {
        nProjection = 6;
        parms[0] = poSR->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        parms[3] = poSR->GetProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0);
        parms[4] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[5] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 6;
    }

    else if( EQUAL(pszProjection,SRS_PT_GALL_STEREOGRAPHIC) )
    {
        nProjection = 17;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        nParmCount = 1;
    }

    else if( EQUAL(pszProjection,SRS_PT_HOTINE_OBLIQUE_MERCATOR) )
    {
        nProjection = 7;
        parms[0] = poSR->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_AZIMUTH,0.0);
        parms[3] = poSR->GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        parms[4] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[5] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 6;
    }

    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_AZIMUTHAL_EQUAL_AREA) )
    {
        nProjection = 4;
        parms[0] = poSR->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0);
        parms[2] = 90.0;
        nParmCount = 3;

        if( ABS((ABS(parms[1]) - 90)) > 0.001 )
            nProjection = 28;
    }

    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP) )
    {
        nProjection = 3;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        parms[3] = poSR->GetProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0);
        parms[4] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[5] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 6;
    }

    else if( EQUAL(pszProjection,SRS_PT_LAMBERT_CONFORMAL_CONIC_2SP_BELGIUM) )
    {
        nProjection = 19;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_STANDARD_PARALLEL_1,0.0);
        parms[3] = poSR->GetProjParm(SRS_PP_STANDARD_PARALLEL_2,0.0);
        parms[4] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[5] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 6;
    }

    else if( EQUAL(pszProjection,SRS_PT_MERCATOR_1SP) )
    {
        nProjection = 10;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        nParmCount = 1;

        if( parms[1] != 0.0 )
        {
            nProjection = 26;
            nParmCount = 2;
        }
    }

    else if( EQUAL(pszProjection,SRS_PT_MILLER_CYLINDRICAL) )
    {
        nProjection = 11;
        parms[0] = poSR->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
        nParmCount = 1;
    }

    else if( EQUAL(pszProjection,SRS_PT_MOLLWEIDE) )
    {
        nProjection = 13;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        nParmCount = 1;
    }

    else if( EQUAL(pszProjection,SRS_PT_SWISS_OBLIQUE_CYLINDRICAL) )
    {
        nProjection = 25;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[3] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 4;
    }

    else if( EQUAL(pszProjection,SRS_PT_ROBINSON) )
    {
        nProjection = 12;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        nParmCount = 1;
    }

    else if( EQUAL(pszProjection,SRS_PT_SINUSOIDAL) )
    {
        nProjection = 16;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        nParmCount = 1;
    }

    else if( EQUAL(pszProjection,SRS_PT_STEREOGRAPHIC) )
    {
        nProjection = 20;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        parms[3] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[4] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 5;
    }

    else if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR) )
    {
        nProjection = 8;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
        parms[3] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[4] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 5;
    }

    // Transverse Mercator,(modified for Danish System 34 Jylland-Fyn)
    else if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR_MI_21) )
    {
       nProjection = 21;
       parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
       parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
       parms[2] = poSR->GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
       parms[3] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
       parms[4] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
       nParmCount = 5;
    }

    // Transverse Mercator,(modified for Danish System 34 Sjaelland)
    else if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR_MI_22) )
    {
       nProjection = 22;
       parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
       parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
       parms[2] = poSR->GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
       parms[3] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
       parms[4] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
       nParmCount = 5;
    }

    // Transverse Mercator,(modified for Danish System 34/45 Bornholm)
    else if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR_MI_23) )
    {
       nProjection = 23;
       parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
       parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
       parms[2] = poSR->GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
       parms[3] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
       parms[4] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
       nParmCount = 5;
    }

    // Transverse Mercator,(modified for Finnish KKJ)
    else if( EQUAL(pszProjection,SRS_PT_TRANSVERSE_MERCATOR_MI_24) )
    {
       nProjection = 24;
       parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
       parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
       parms[2] = poSR->GetProjParm(SRS_PP_SCALE_FACTOR,1.0);
       parms[3] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
       parms[4] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
       nParmCount = 5;
    }

    else if( EQUAL(pszProjection,SRS_PT_CASSINI_SOLDNER) )
    {
        nProjection = 30;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[3] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 4;
    }

    else if( EQUAL(pszProjection,SRS_PT_NEW_ZEALAND_MAP_GRID) )
    {
        nProjection = 18;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[3] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 4;
    }

    else if( EQUAL(pszProjection,SRS_PT_POLYCONIC) )
    {
        nProjection = 27;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[3] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 4;
    }

    else if( EQUAL(pszProjection,SRS_PT_KROVAK) )
    {
        nProjection = 32;
        parms[0] = poSR->GetProjParm(SRS_PP_LONGITUDE_OF_CENTER,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_CENTER,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_PSEUDO_STD_PARALLEL_1,0.0);
        parms[3] = poSR->GetProjParm(SRS_PP_AZIMUTH,0.0);
        parms[4] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[5] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 6;
    }

    else if( EQUAL(pszProjection,SRS_PT_EQUIRECTANGULAR) )
    {
        nProjection = 33;
        parms[0] = poSR->GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0);
        parms[1] = poSR->GetProjParm(SRS_PP_LATITUDE_OF_ORIGIN,0.0);
        parms[2] = poSR->GetProjParm(SRS_PP_FALSE_EASTING,0.0);
        parms[3] = poSR->GetProjParm(SRS_PP_FALSE_NORTHING,0.0);
        nParmCount = 4;
    }

    /* ==============================================================
     * Translate Datum and Ellipsoid
     * ============================================================== */
    int         nDatum = 0;
    double      adfDatumParm[8] = {0.0,0.0,0.0,0.0,0.0,0.0,0.0,0.0};
    int         nEllipsoid=0;
    
    int nDatumEPSGCode = -1;
    const char *pszDatumAuthority = poSR->GetAuthorityName("DATUM");
    const char *pszDatumCode = poSR->GetAuthorityCode("DATUM");
    
    if (pszDatumCode && pszDatumAuthority && EQUAL(pszDatumAuthority, "EPSG"))
    {
    	nDatumEPSGCode = atoi(pszDatumCode);
    }
    
    const char *pszWKTDatum = poSR->GetAttrValue("DATUM");

    if( pszWKTDatum == NULL )
    {
        nDatum = 0;
        if( nProjection == 1 )
            nProjection = 0;
    }
    
    /*-----------------------------------------------------------------
     * We know the MIF datum number, and need to look it up to
     * translate into datum parameters.
     *----------------------------------------------------------------*/
    else if( EQUALN(pszWKTDatum,"MIF ",4)
             && atoi(pszWKTDatum+4) != 999
             && atoi(pszWKTDatum+4) != 9999 )
    {
        nDatum = atoi(pszWKTDatum+4);
    }

    /*-----------------------------------------------------------------
     * We have the MIF datum parameters, and apply those directly.
     *----------------------------------------------------------------*/
    else if( EQUALN(pszWKTDatum,"MIF ",4)
             && (atoi(pszWKTDatum+4) == 999 || atoi(pszWKTDatum+4) == 9999) )
    {
        char    **papszFields;
        
        nDatum = atoi(pszWKTDatum+4);

        papszFields =
            CSLTokenizeStringComplex( pszWKTDatum+4, ",", FALSE, TRUE);

        if( CSLCount(papszFields) >= 5 )
        {
            nEllipsoid = atoi(papszFields[1]);
            adfDatumParm[0] = atof(papszFields[2]);
            adfDatumParm[1] = atof(papszFields[3]);
            adfDatumParm[2] = atof(papszFields[4]);
        }

        if( CSLCount(papszFields) >= 10 )
        {
            adfDatumParm[3] = atof(papszFields[5]);
            adfDatumParm[4] = atof(papszFields[6]);
            adfDatumParm[5] = atof(papszFields[7]);
            adfDatumParm[6] = atof(papszFields[8]);
            adfDatumParm[7] = atof(papszFields[9]);
        }

        if( CSLCount(papszFields) < 5 )
            nDatum = 104; /* WGS84 */

        CSLDestroy( papszFields );
    }
    
    /*-----------------------------------------------------------------
     * We have a "real" datum name, and possibly an EPSG code for the
     * datum.  Try to look it up (using EPSG code first) and get the
     * parameters.  If we don't find it with either just use WGS84.
     *----------------------------------------------------------------*/
    else 
    {
        int     i;

        for( i = 0; asDatumInfoList[i].nMapInfoDatumID != -1; i++ )
        {
        	if ( (nDatumEPSGCode > 0 && asDatumInfoList[i].nDatumEPSGCode == nDatumEPSGCode) ||
        		   EQUAL(pszWKTDatum,asDatumInfoList[i].pszOGCDatumName) )
            {
                nDatum = asDatumInfoList[i].nMapInfoDatumID;
                break;
            }
        }

        if( nDatum == 0 )
            nDatum = 104; /* WGS84 */
    }
    
    /*-----------------------------------------------------------------
     * Translate the units
     *----------------------------------------------------------------*/
    const char  *pszMIFUnits = "m";

    if( nProjection == 1 )
        pszMIFUnits = NULL;
    else if( pszLinearUnits == NULL )
        pszMIFUnits = "m";
    else if( dfLinearConv == 1000.0 )
        pszMIFUnits = "km";
    else if( dfLinearConv == 0.0254 || EQUAL(pszLinearUnits,"Inch")
             || EQUAL(pszLinearUnits,"IINCH"))
        pszMIFUnits = "in";
    else if( dfLinearConv == CPLAtof(SRS_UL_FOOT_CONV)
             || EQUAL(pszLinearUnits,SRS_UL_FOOT) )
        pszMIFUnits = "ft";
    else if( EQUAL(pszLinearUnits,"YARD") || EQUAL(pszLinearUnits,"IYARD") 
             || dfLinearConv == 0.9144 )
        pszMIFUnits = "yd";
    else if( dfLinearConv == 0.001 )
        pszMIFUnits = "mm";
    else if( dfLinearConv == 0.01 )
        pszMIFUnits = "cm";
    else if( dfLinearConv == 1.0 )
        pszMIFUnits = "m";
    else if( dfLinearConv == CPLAtof(SRS_UL_US_FOOT_CONV)
             || EQUAL(pszLinearUnits,SRS_UL_US_FOOT) )
        pszMIFUnits = "survey ft";
    else if( EQUAL(pszLinearUnits,SRS_UL_NAUTICAL_MILE) )
        pszMIFUnits = "nmi";
    else if( EQUAL(pszLinearUnits,SRS_UL_LINK) 
             || EQUAL(pszLinearUnits,"GUNTERLINK") )
        pszMIFUnits = "li";
    else if( EQUAL(pszLinearUnits,SRS_UL_CHAIN) 
             || EQUAL(pszLinearUnits,"GUNTERCHAIN") )
        pszMIFUnits = "ch";
    else if( EQUAL(pszLinearUnits,SRS_UL_ROD) )
        pszMIFUnits = "rd";
    else if( EQUAL(pszLinearUnits,"Mile") 
             || EQUAL(pszLinearUnits,"IMILE") )
        pszMIFUnits = "mi";
    
/* -------------------------------------------------------------------- */
/*      Build coordinate system definition.                             */
/* -------------------------------------------------------------------- */
    char        szCoordSys[256];

    if( nProjection != 0 )
    {
        sprintf( szCoordSys,
                 "Earth Projection %d",
                 nProjection );

    }
    else
        sprintf( szCoordSys,
                 "NonEarth Units" );

/* -------------------------------------------------------------------- */
/*      Append Datum                                                    */
/* -------------------------------------------------------------------- */
    if( nProjection != 0 )
    {
        sprintf( szCoordSys + strlen(szCoordSys),
                 ", %d",
                 nDatum );

        if( nDatum == 999 || nDatum == 9999 )
        {
            sprintf( szCoordSys + strlen(szCoordSys),
                     ", %d, %.15g, %.15g, %.15g",
                     nEllipsoid,
                     adfDatumParm[0], adfDatumParm[1], adfDatumParm[2] );
        }
        
        if( nDatum == 9999 )
        {
            sprintf( szCoordSys + strlen(szCoordSys),
                     ", %.15g, %.15g, %.15g, %.15g, %.15g",
                     adfDatumParm[3], adfDatumParm[4], adfDatumParm[5],
                     adfDatumParm[6], adfDatumParm[7] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Append units.                                                   */
/* -------------------------------------------------------------------- */
    if( nProjection != 1 && pszMIFUnits != NULL )
    {
        if( nProjection != 0 )
            strcat( szCoordSys, "," );
        
        sprintf( szCoordSys + strlen(szCoordSys),
                 " \"%s\"",
                 pszMIFUnits );
    }

/* -------------------------------------------------------------------- */
/*      Append Projection Parms.                                        */
/* -------------------------------------------------------------------- */
    for( int iParm = 0; iParm < nParmCount; iParm++ )
        sprintf( szCoordSys + strlen(szCoordSys),
                 ", %.15g",
                 parms[iParm] );

/* -------------------------------------------------------------------- */
/*      Report on translation                                           */
/* -------------------------------------------------------------------- */
    char        *pszWKT = NULL;

    poSR->exportToWkt( &pszWKT );
    if( pszWKT != NULL )
    {
        CPLDebug( "MITAB",
                  "This WKT Projection:\n%s\n\ntranslates to:\n%s\n",
                  pszWKT, szCoordSys );
        CPLFree( pszWKT );
    }

    return( CPLStrdup( szCoordSys ) );
}


/************************************************************************/
/*                      MITABExtractCoordSysBounds                      */
/*                                                                      */
/* Return TRUE if MIF coordsys string contains a BOUNDS parameter and   */
/* Set x/y min/max values.                                              */
/************************************************************************/

GBool MITABExtractCoordSysBounds( const char * pszCoordSys,
                                  double &dXMin, double &dYMin,
                                  double &dXMax, double &dYMax )

{
    char        **papszFields;

    if( pszCoordSys == NULL )
        return FALSE;
    
    papszFields = CSLTokenizeStringComplex( pszCoordSys, " ,()", TRUE, FALSE );

    int iBounds = CSLFindString( papszFields, "Bounds" );

    if (iBounds >= 0 && iBounds + 4 < CSLCount(papszFields))
    {
        dXMin = atof(papszFields[++iBounds]);
        dYMin = atof(papszFields[++iBounds]);
        dXMax = atof(papszFields[++iBounds]);
        dYMax = atof(papszFields[++iBounds]);
        CSLDestroy( papszFields );
        return TRUE;
    }

    CSLDestroy( papszFields );
    return FALSE;
}


/**********************************************************************
 *                     MITABCoordSys2TABProjInfo()
 *
 * Convert a MIF COORDSYS string into a TABProjInfo structure.
 *
 * Note that it would have been possible to achieve the same by calling
 * TABFile::SetSpatialRef( MITABCoordSys2SpatialRef() ) but this would 
 * involve lots of manipulations for cases where only a simple conversion
 * is required.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int MITABCoordSys2TABProjInfo(const char * pszCoordSys, TABProjInfo *psProj)

{
    char        **papszFields;

    // Set all fields to zero, equivalent of NonEarth Units "mi"
    memset(psProj, 0, sizeof(TABProjInfo));

    if( pszCoordSys == NULL )
        return -1;
    
    /*-----------------------------------------------------------------
     * Parse the passed string into words.
     *----------------------------------------------------------------*/
    while(*pszCoordSys == ' ') pszCoordSys++;  // Eat leading spaces
    if( EQUALN(pszCoordSys,"CoordSys",8) )
        pszCoordSys += 9;
    
    papszFields = CSLTokenizeStringComplex( pszCoordSys, " ,", TRUE, FALSE );

    /*-----------------------------------------------------------------
     * Clip off Bounds information.
     *----------------------------------------------------------------*/
    int         iBounds = CSLFindString( papszFields, "Bounds" );

    while( iBounds != -1 && papszFields[iBounds] != NULL )
    {
        CPLFree( papszFields[iBounds] );
        papszFields[iBounds] = NULL;
        iBounds++;
    }

    /*-----------------------------------------------------------------
     * Fetch the projection.
     *----------------------------------------------------------------*/
    char        **papszNextField;

    if( CSLCount( papszFields ) >= 3
        && EQUAL(papszFields[0],"Earth")
        && EQUAL(papszFields[1],"Projection") )
    {
        psProj->nProjId = (GByte)atoi(papszFields[2]);
        papszNextField = papszFields + 3;
    }
    else if (CSLCount( papszFields ) >= 2
             && EQUAL(papszFields[0],"NonEarth") )
    {
        // NonEarth Units "..." Bounds (x, y) (x, y)
        psProj->nProjId = 0;
        papszNextField = papszFields + 2;

        if( papszNextField[0] != NULL && EQUAL(papszNextField[0],"Units") )
            papszNextField++;
    }
    else
    {
        // Invalid projection string ???
        if (CSLCount(papszFields) > 0)
            CPLError(CE_Warning, CPLE_IllegalArg,
                     "Failed parsing CoordSys: '%s'", pszCoordSys);
        CSLDestroy(papszFields);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Fetch the datum information.
     *----------------------------------------------------------------*/
    int         nDatum = 0;

    if( psProj->nProjId != 0 && CSLCount(papszNextField) > 0 )
    {
        nDatum = atoi(papszNextField[0]);
        papszNextField++;
    }

    if( (nDatum == 999 || nDatum == 9999)
        && CSLCount(papszNextField) >= 4 )
    {
        psProj->nEllipsoidId = (GByte)atoi(papszFields[0]);
        psProj->dDatumShiftX = atof(papszNextField[1]);
        psProj->dDatumShiftY = atof(papszNextField[2]);
        psProj->dDatumShiftZ = atof(papszNextField[3]);
        papszNextField += 4;

        if( nDatum == 9999
            && CSLCount(papszNextField) >= 5 )
        {
            psProj->adDatumParams[0] = atof(papszNextField[0]);
            psProj->adDatumParams[1] = atof(papszNextField[1]);
            psProj->adDatumParams[2] = atof(papszNextField[2]);
            psProj->adDatumParams[3] = atof(papszNextField[3]);
            psProj->adDatumParams[4] = atof(papszNextField[4]);
            papszNextField += 5;
        }
    }
    else if (nDatum != 999 && nDatum != 9999)
    {
    /*-----------------------------------------------------------------
     * Find the datum, and collect it's parameters if possible.
     *----------------------------------------------------------------*/
        int         iDatum;
        const MapInfoDatumInfo *psDatumInfo = NULL;
        
        for(iDatum=0; asDatumInfoList[iDatum].nMapInfoDatumID != -1; iDatum++)
        {
            if( asDatumInfoList[iDatum].nMapInfoDatumID == nDatum )
            {
                psDatumInfo = asDatumInfoList + iDatum;
                break;
            }
        }

        if( asDatumInfoList[iDatum].nMapInfoDatumID == -1
            && nDatum != 999 && nDatum != 9999 )
        {
            /* use WGS84 */
            psDatumInfo = asDatumInfoList + 0;
        }

        if( psDatumInfo != NULL )
        {
            psProj->nEllipsoidId = (GByte)psDatumInfo->nEllipsoid;
            psProj->nDatumId = (GInt16)psDatumInfo->nMapInfoDatumID;
            psProj->dDatumShiftX = psDatumInfo->dfShiftX;
            psProj->dDatumShiftY = psDatumInfo->dfShiftY;
            psProj->dDatumShiftZ = psDatumInfo->dfShiftZ;
            psProj->adDatumParams[0] = psDatumInfo->dfDatumParm0;
            psProj->adDatumParams[1] = psDatumInfo->dfDatumParm1;
            psProj->adDatumParams[2] = psDatumInfo->dfDatumParm2;
            psProj->adDatumParams[3] = psDatumInfo->dfDatumParm3;
            psProj->adDatumParams[4] = psDatumInfo->dfDatumParm4;
        }
    }    

    /*-----------------------------------------------------------------
     * Fetch the units string.
     *----------------------------------------------------------------*/
    if( CSLCount(papszNextField) > 0 )
    {
        psProj->nUnitsId = (GByte)TABUnitIdFromString(papszNextField[0]);
        papszNextField++;
    }

    /*-----------------------------------------------------------------
     * Finally the projection parameters.
     *----------------------------------------------------------------*/
    for(int iParam=0; iParam < 6 && CSLCount(papszNextField) > 0; iParam++)
    {
        psProj->adProjParams[iParam] = atof(papszNextField[0]);
        papszNextField++;         
    }

    return 0;
}

