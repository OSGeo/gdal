/******************************************************************************
 * $Id$
 *
 * Project:  Multi-resolution Seamless Image Database (MrSID)
 * Purpose:  Shared code between MrSID 3 and 4 drivers.
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 ******************************************************************************
 * Copyright (c) 2003, Andrey Kiselev <dron@remotesensing.org>
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
 * Revision 1.1  2005/02/16 22:07:29  fwarmerdam
 * New
 *
 */

/************************************************************************/
/*                    EPSGProjMethodToCTProjMethod()                    */
/*                                                                      */
/*      Convert between the EPSG enumeration for projection methods,    */
/*      and the GeoTIFF CT codes.                                       */
/*      Explicitly copied from geo_normalize.c of the GeoTIFF package   */
/************************************************************************/

static int EPSGProjMethodToCTProjMethod( int nEPSG )

{
    /* see trf_method.csv for list of EPSG codes */
    
    switch( nEPSG )
    {
      case 9801:
        return( CT_LambertConfConic_1SP );

      case 9802:
        return( CT_LambertConfConic_2SP );

      case 9803:
        return( CT_LambertConfConic_2SP ); /* Belgian variant not supported */

      case 9804:
        return( CT_Mercator );  /* 1SP and 2SP not differentiated */

      case 9805:
        return( CT_Mercator );  /* 1SP and 2SP not differentiated */

      case 9806:
        return( CT_CassiniSoldner );

      case 9807:
        return( CT_TransverseMercator );

      case 9808:
        return( CT_TransvMercator_SouthOriented );

      case 9809:
        return( CT_ObliqueStereographic );

      case 9810:
        return( CT_PolarStereographic );

      case 9811:
        return( CT_NewZealandMapGrid );

      case 9812:
        return( CT_ObliqueMercator ); /* is hotine actually different? */

      case 9813:
        return( CT_ObliqueMercator_Laborde );

      case 9814:
        return( CT_ObliqueMercator_Rosenmund ); /* swiss  */

      case 9815:
        return( CT_ObliqueMercator );

      case 9816: /* tunesia mining grid has no counterpart */
        return( KvUserDefined );
    }

    return( KvUserDefined );
}

/* EPSG Codes for projection parameters.  Unfortunately, these bear no
   relationship to the GeoTIFF codes even though the names are so similar. */

#define EPSGNatOriginLat         8801
#define EPSGNatOriginLong        8802
#define EPSGNatOriginScaleFactor 8805
#define EPSGFalseEasting         8806
#define EPSGFalseNorthing        8807
#define EPSGProjCenterLat        8811
#define EPSGProjCenterLong       8812
#define EPSGAzimuth              8813
#define EPSGAngleRectifiedToSkewedGrid 8814
#define EPSGInitialLineScaleFactor 8815
#define EPSGProjCenterEasting    8816
#define EPSGProjCenterNorthing   8817
#define EPSGPseudoStdParallelLat 8818
#define EPSGPseudoStdParallelScaleFactor 8819
#define EPSGFalseOriginLat       8821
#define EPSGFalseOriginLong      8822
#define EPSGStdParallel1Lat      8823
#define EPSGStdParallel2Lat      8824
#define EPSGFalseOriginEasting   8826
#define EPSGFalseOriginNorthing  8827
#define EPSGSphericalOriginLat   8828
#define EPSGSphericalOriginLong  8829
#define EPSGInitialLongitude     8830
#define EPSGZoneWidth            8831

/************************************************************************/
/*                            SetGTParmIds()                            */
/*                                                                      */
/*      This is hardcoded logic to set the GeoTIFF parameter            */
/*      identifiers for all the EPSG supported projections.  As the     */
/*      trf_method.csv table grows with new projections, this code      */
/*      will need to be updated.                                        */
/*      Explicitly copied from geo_normalize.c of the GeoTIFF package.  */
/************************************************************************/

static int SetGTParmIds( int nCTProjection, 
                         int *panProjParmId, 
                         int *panEPSGCodes )

{
    int anWorkingDummy[7];

    if( panEPSGCodes == NULL )
        panEPSGCodes = anWorkingDummy;
    if( panProjParmId == NULL )
        panProjParmId = anWorkingDummy;

    memset( panEPSGCodes, 0, sizeof(int) * 7 );

    /* psDefn->nParms = 7; */
    
    switch( nCTProjection )
    {
      case CT_CassiniSoldner:
      case CT_NewZealandMapGrid:
        panProjParmId[0] = ProjNatOriginLatGeoKey;
        panProjParmId[1] = ProjNatOriginLongGeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGNatOriginLat;
        panEPSGCodes[1] = EPSGNatOriginLong;
        panEPSGCodes[5] = EPSGFalseEasting;
        panEPSGCodes[6] = EPSGFalseNorthing;
        return TRUE;

      case CT_ObliqueMercator:
        panProjParmId[0] = ProjCenterLatGeoKey;
        panProjParmId[1] = ProjCenterLongGeoKey;
        panProjParmId[2] = ProjAzimuthAngleGeoKey;
        panProjParmId[3] = ProjRectifiedGridAngleGeoKey;
        panProjParmId[4] = ProjScaleAtCenterGeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGProjCenterLat;
        panEPSGCodes[1] = EPSGProjCenterLong;
        panEPSGCodes[2] = EPSGAzimuth;
        panEPSGCodes[3] = EPSGAngleRectifiedToSkewedGrid;
        panEPSGCodes[4] = EPSGInitialLineScaleFactor;
        panEPSGCodes[5] = EPSGProjCenterEasting;
        panEPSGCodes[6] = EPSGProjCenterNorthing;
        return TRUE;

      case CT_ObliqueMercator_Laborde:
        panProjParmId[0] = ProjCenterLatGeoKey;
        panProjParmId[1] = ProjCenterLongGeoKey;
        panProjParmId[2] = ProjAzimuthAngleGeoKey;
        panProjParmId[4] = ProjScaleAtCenterGeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGProjCenterLat;
        panEPSGCodes[1] = EPSGProjCenterLong;
        panEPSGCodes[2] = EPSGAzimuth;
        panEPSGCodes[4] = EPSGInitialLineScaleFactor;
        panEPSGCodes[5] = EPSGProjCenterEasting;
        panEPSGCodes[6] = EPSGProjCenterNorthing;
        return TRUE;
        
      case CT_LambertConfConic_1SP:
      case CT_Mercator:
      case CT_ObliqueStereographic:
      case CT_PolarStereographic:
      case CT_TransverseMercator:
      case CT_TransvMercator_SouthOriented:
        panProjParmId[0] = ProjNatOriginLatGeoKey;
        panProjParmId[1] = ProjNatOriginLongGeoKey;
        panProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGNatOriginLat;
        panEPSGCodes[1] = EPSGNatOriginLong;
        panEPSGCodes[4] = EPSGNatOriginScaleFactor;
        panEPSGCodes[5] = EPSGFalseEasting;
        panEPSGCodes[6] = EPSGFalseNorthing;
        return TRUE;

      case CT_LambertConfConic_2SP:
        panProjParmId[0] = ProjFalseOriginLatGeoKey;
        panProjParmId[1] = ProjFalseOriginLongGeoKey;
        panProjParmId[2] = ProjStdParallel1GeoKey;
        panProjParmId[3] = ProjStdParallel2GeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        panEPSGCodes[0] = EPSGFalseOriginLat;
        panEPSGCodes[1] = EPSGFalseOriginLong;
        panEPSGCodes[2] = EPSGStdParallel1Lat;
        panEPSGCodes[3] = EPSGStdParallel2Lat;
        panEPSGCodes[5] = EPSGFalseOriginEasting;
        panEPSGCodes[6] = EPSGFalseOriginNorthing;
        return TRUE;

      case CT_SwissObliqueCylindrical:
        panProjParmId[0] = ProjCenterLatGeoKey;
        panProjParmId[1] = ProjCenterLongGeoKey;
        panProjParmId[5] = ProjFalseEastingGeoKey;
        panProjParmId[6] = ProjFalseNorthingGeoKey;

        /* EPSG codes? */
        return TRUE;

      default:
        return( FALSE );
    }
}

static char *papszDatumEquiv[] =
{
    "Militar_Geographische_Institut",
    "Militar_Geographische_Institute",
    "World_Geodetic_System_1984",
    "WGS_1984",
    "WGS_72_Transit_Broadcast_Ephemeris",
    "WGS_1972_Transit_Broadcast_Ephemeris",
    "World_Geodetic_System_1972",
    "WGS_1972",
    "European_Terrestrial_Reference_System_89",
    "European_Reference_System_1989",
    NULL
};

/************************************************************************/
/*                          WKTMassageDatum()                           */
/*                                                                      */
/*      Massage an EPSG datum name into WMT format.  Also transform     */
/*      specific exception cases into WKT versions.                     */
/*      Explicitly copied from the gt_wkt_srs.cpp.                      */
/************************************************************************/

static void WKTMassageDatum( char ** ppszDatum )

{
    int		i, j;
    char	*pszDatum;

/* -------------------------------------------------------------------- */
/*      First copy string and allocate with our CPLStrdup() to so we    */
/*      know when we are done this function we will have a CPL          */
/*      string, not a GTIF one.                                         */
/* -------------------------------------------------------------------- */
    pszDatum = CPLStrdup(*ppszDatum);
    GTIFFreeMemory( *ppszDatum );
    *ppszDatum = pszDatum;

/* -------------------------------------------------------------------- */
/*      Translate non-alphanumeric values to underscores.               */
/* -------------------------------------------------------------------- */
    for( i = 0; pszDatum[i] != '\0'; i++ )
    {
        if( !(pszDatum[i] >= 'A' && pszDatum[i] <= 'Z')
            && !(pszDatum[i] >= 'a' && pszDatum[i] <= 'z')
            && !(pszDatum[i] >= '0' && pszDatum[i] <= '9') )
        {
            pszDatum[i] = '_';
        }
    }

/* -------------------------------------------------------------------- */
/*      Remove repeated and trailing underscores.                       */
/* -------------------------------------------------------------------- */
    for( i = 1, j = 0; pszDatum[i] != '\0'; i++ )
    {
        if( pszDatum[j] == '_' && pszDatum[i] == '_' )
            continue;

        pszDatum[++j] = pszDatum[i];
    }
    if( pszDatum[j] == '_' )
        pszDatum[j] = '\0';
    else
        pszDatum[j+1] = '\0';
    
/* -------------------------------------------------------------------- */
/*      Search for datum equivelences.  Specific massaged names get     */
/*      mapped to OpenGIS specified names.                              */
/* -------------------------------------------------------------------- */
    for( i = 0; papszDatumEquiv[i] != NULL; i += 2 )
    {
        if( EQUAL(*ppszDatum,papszDatumEquiv[i]) )
        {
            CPLFree( *ppszDatum );
            *ppszDatum = CPLStrdup( papszDatumEquiv[i+1] );
            return;
        }
    }
}

/************************************************************************/
/*                           FetchProjParms()                           */
/*                                                                      */
/*      Fetch the projection parameters for a particular projection     */
/*      from MrSID metadata, and fill the GTIFDefn structure out        */
/*      with them.                                                      */
/*      Copied from geo_normalize.c of the GeoTIFF package.             */
/************************************************************************/

void MrSIDDataset::FetchProjParms()
{
    double dfNatOriginLong = 0.0, dfNatOriginLat = 0.0, dfRectGridAngle = 0.0;
    double dfFalseEasting = 0.0, dfFalseNorthing = 0.0, dfNatOriginScale = 1.0;
    double dfStdParallel1 = 0.0, dfStdParallel2 = 0.0, dfAzimuth = 0.0;

/* -------------------------------------------------------------------- */
/*      Get the false easting, and northing if available.               */
/* -------------------------------------------------------------------- */
    if( !GetMetadataElement( "GEOTIFF_NUM::3082::ProjFalseEastingGeoKey",
                             &dfFalseEasting )
        && !GetMetadataElement( "GEOTIFF_NUM::3090:ProjCenterEastingGeoKey",
                                &dfFalseEasting ) )
        dfFalseEasting = 0.0;
        
    if( !GetMetadataElement( "GEOTIFF_NUM::3083::ProjFalseNorthingGeoKey",
                             &dfFalseNorthing )
        && !GetMetadataElement( "GEOTIFF_NUM::3091::ProjCenterNorthingGeoKey",
                                &dfFalseNorthing ) )
        dfFalseNorthing = 0.0;

    switch( psDefn->CTProjection )
    {
/* -------------------------------------------------------------------- */
      case CT_Stereographic:
/* -------------------------------------------------------------------- */
        if( GetMetadataElement( "GEOTIFF_NUM::3080::ProjNatOriginLongGeoKey",
                                &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3084::ProjFalseOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3088::ProjCenterLongGeoKey",
                                   &dfNatOriginLong ) == 0 )
            dfNatOriginLong = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3081::ProjNatOriginLatGeoKey",
                                &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3085::ProjFalseOriginLatGeoKey",
                                   &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3089::ProjCenterLatGeoKey",
                                   &dfNatOriginLat ) == 0 )
            dfNatOriginLat = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3092::ProjScaleAtNatOriginGeoKey",
                                &dfNatOriginScale ) == 0 )
            dfNatOriginScale = 1.0;
            
        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_LambertConfConic_1SP:
      case CT_Mercator:
      case CT_ObliqueStereographic:
      case CT_TransverseMercator:
      case CT_TransvMercator_SouthOriented:
/* -------------------------------------------------------------------- */
        if( GetMetadataElement( "GEOTIFF_NUM::3080::ProjNatOriginLongGeoKey",
                                &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3084::ProjFalseOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3088::ProjCenterLongGeoKey", 
                                   &dfNatOriginLong ) == 0 )
            dfNatOriginLong = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3081::ProjNatOriginLatGeoKey",
                                &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3085::ProjFalseOriginLatGeoKey",
                                   &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3089::ProjCenterLatGeoKey",
                                   &dfNatOriginLat ) == 0 )
            dfNatOriginLat = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3092::ProjScaleAtNatOriginGeoKey",
                                &dfNatOriginScale ) == 0 )
            dfNatOriginScale = 1.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_ObliqueMercator: /* hotine */
/* -------------------------------------------------------------------- */
        if( GetMetadataElement( "GEOTIFF_NUM::3080::ProjNatOriginLongGeoKey",
                                &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3084::ProjFalseOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3088::ProjCenterLongGeoKey", 
                                   &dfNatOriginLong ) == 0 )
            dfNatOriginLong = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3081::ProjNatOriginLatGeoKey",
                                &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3085::ProjFalseOriginLatGeoKey",
                                   &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3089::ProjCenterLatGeoKey",
                                   &dfNatOriginLat ) == 0 )
            dfNatOriginLat = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3094::ProjAzimuthAngleGeoKey",
                                &dfAzimuth ) == 0 )
            dfAzimuth = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3096::ProjRectifiedGridAngleGeoKey",
                                &dfRectGridAngle ) == 0 )
            dfRectGridAngle = 90.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3092::ProjScaleAtNatOriginGeoKey",
                                &dfNatOriginScale ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3093::ProjScaleAtCenterGeoKey",
                                   &dfNatOriginScale ) == 0 )
            dfNatOriginScale = 1.0;
            
        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[2] = dfAzimuth;
        psDefn->ProjParmId[2] = ProjAzimuthAngleGeoKey;
        psDefn->ProjParm[3] = dfRectGridAngle;
        psDefn->ProjParmId[3] = ProjRectifiedGridAngleGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtCenterGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_CassiniSoldner:
      case CT_Polyconic:
/* -------------------------------------------------------------------- */
        if( GetMetadataElement( "GEOTIFF_NUM::3080::ProjNatOriginLongGeoKey",
                                &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3084::ProjFalseOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3088::ProjCenterLongGeoKey", 
                                   &dfNatOriginLong ) == 0 )
            dfNatOriginLong = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3081::ProjNatOriginLatGeoKey",
                                &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3085::ProjFalseOriginLatGeoKey",
                                   &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3089::ProjCenterLatGeoKey",
                                   &dfNatOriginLat ) == 0 )
            dfNatOriginLat = 0.0;


        if( GetMetadataElement( "GEOTIFF_NUM::3092::ProjScaleAtNatOriginGeoKey",
                                &dfNatOriginScale ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3093::ProjScaleAtCenterGeoKey",
                                   &dfNatOriginScale ) == 0 )
            dfNatOriginScale = 1.0;
            
        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_AzimuthalEquidistant:
      case CT_MillerCylindrical:
      case CT_Equirectangular:
      case CT_Gnomonic:
      case CT_LambertAzimEqualArea:
      case CT_Orthographic:
/* -------------------------------------------------------------------- */
        if( GetMetadataElement( "GEOTIFF_NUM::3080::ProjNatOriginLongGeoKey",
                                &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3084::ProjFalseOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3088::ProjCenterLongGeoKey", 
                                   &dfNatOriginLong ) == 0 )
            dfNatOriginLong = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3081::ProjNatOriginLatGeoKey",
                                &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3085::ProjFalseOriginLatGeoKey",
                                   &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3089::ProjCenterLatGeoKey",
                                   &dfNatOriginLat ) == 0 )
            dfNatOriginLat = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjCenterLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_Robinson:
      case CT_Sinusoidal:
      case CT_VanDerGrinten:
/* -------------------------------------------------------------------- */
        if( GetMetadataElement( "GEOTIFF_NUM::3080::ProjNatOriginLongGeoKey",
                                &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3084::ProjFalseOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3088::ProjCenterLongGeoKey", 
                                   &dfNatOriginLong ) == 0 )
            dfNatOriginLong = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjCenterLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_PolarStereographic:
/* -------------------------------------------------------------------- */
        if( GetMetadataElement( "GEOTIFF_NUM::3095::ProjStraightVertPoleLongGeoKey",
                                &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3080::ProjNatOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3084::ProjFalseOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3088::ProjCenterLongGeoKey",
                                   &dfNatOriginLong ) == 0 )
            dfNatOriginLong = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3081::ProjNatOriginLatGeoKey",
                                &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3085::ProjFalseOriginLatGeoKey",
                                   &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3089::ProjCenterLatGeoKey",
                                   &dfNatOriginLat ) == 0 )
            dfNatOriginLat = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3092::ProjScaleAtNatOriginGeoKey",
                                &dfNatOriginScale ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3093::ProjScaleAtCenterGeoKey",
                                   &dfNatOriginScale ) == 0 )
            dfNatOriginScale = 1.0;
            
        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjStraightVertPoleLongGeoKey;
        psDefn->ProjParm[4] = dfNatOriginScale;
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_LambertConfConic_2SP:
/* -------------------------------------------------------------------- */
        if( GetMetadataElement( "GEOTIFF_NUM::3078::ProjStdParallel1GeoKey",
                                &dfStdParallel1 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3079::ProjStdParallel2GeoKey",
                                &dfStdParallel2 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3080::ProjNatOriginLongGeoKey",
                                &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3084::ProjFalseOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3088::ProjCenterLongGeoKey", 
                                   &dfNatOriginLong ) == 0 )
            dfNatOriginLong = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3081::ProjNatOriginLatGeoKey",
                                &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3085::ProjFalseOriginLatGeoKey",
                                   &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3089::ProjCenterLatGeoKey",
                                   &dfNatOriginLat ) == 0 )
            dfNatOriginLat = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfNatOriginLat;
        psDefn->ProjParmId[0] = ProjFalseOriginLatGeoKey;
        psDefn->ProjParm[1] = dfNatOriginLong;
        psDefn->ProjParmId[1] = ProjFalseOriginLongGeoKey;
        psDefn->ProjParm[2] = dfStdParallel1;
        psDefn->ProjParmId[2] = ProjStdParallel1GeoKey;
        psDefn->ProjParm[3] = dfStdParallel2;
        psDefn->ProjParmId[3] = ProjStdParallel2GeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;

/* -------------------------------------------------------------------- */
      case CT_AlbersEqualArea:
      case CT_EquidistantConic:
/* -------------------------------------------------------------------- */
        if( GetMetadataElement( "GEOTIFF_NUM::3078::ProjStdParallel1GeoKey",
                                &dfStdParallel1 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3079::ProjStdParallel2GeoKey",
                                &dfStdParallel2 ) == 0 )
            dfStdParallel1 = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3080::ProjNatOriginLongGeoKey",
                                &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3084::ProjFalseOriginLongGeoKey",
                                   &dfNatOriginLong ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3088::ProjCenterLongGeoKey", 
                                   &dfNatOriginLong ) == 0 )
            dfNatOriginLong = 0.0;

        if( GetMetadataElement( "GEOTIFF_NUM::3081::ProjNatOriginLatGeoKey",
                                &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3085::ProjFalseOriginLatGeoKey",
                                   &dfNatOriginLat ) == 0
            && GetMetadataElement( "GEOTIFF_NUM::3089::ProjCenterLatGeoKey",
                                   &dfNatOriginLat ) == 0 )
            dfNatOriginLat = 0.0;

        /* notdef: should transform to decimal degrees at this point */

        psDefn->ProjParm[0] = dfStdParallel1;
        psDefn->ProjParmId[0] = ProjStdParallel1GeoKey;
        psDefn->ProjParm[1] = dfStdParallel2;
        psDefn->ProjParmId[1] = ProjStdParallel2GeoKey;
        psDefn->ProjParm[2] = dfNatOriginLat;
        psDefn->ProjParmId[2] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[3] = dfNatOriginLong;
        psDefn->ProjParmId[3] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[5] = dfFalseEasting;
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[6] = dfFalseNorthing;
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        psDefn->nParms = 7;
        break;
    }
}

/************************************************************************/
/*                            GetGTIFDefn()                             */
/*      This function borrowed from the GTIFGetDefn() function.         */
/*      See geo_normalize.c from the GeoTIFF package.                   */
/************************************************************************/

void MrSIDDataset::GetGTIFDefn()
{
    double	dfInvFlattening;

/* -------------------------------------------------------------------- */
/*      Initially we default all the information we can.                */
/* -------------------------------------------------------------------- */
    psDefn = new( GTIFDefn );
    psDefn->Model = KvUserDefined;
    psDefn->PCS = KvUserDefined;
    psDefn->GCS = KvUserDefined;
    psDefn->UOMLength = KvUserDefined;
    psDefn->UOMLengthInMeters = 1.0;
    psDefn->UOMAngle = KvUserDefined;
    psDefn->UOMAngleInDegrees = 1.0;
    psDefn->Datum = KvUserDefined;
    psDefn->Ellipsoid = KvUserDefined;
    psDefn->SemiMajor = 0.0;
    psDefn->SemiMinor = 0.0;
    psDefn->PM = KvUserDefined;
    psDefn->PMLongToGreenwich = 0.0;

    psDefn->ProjCode = KvUserDefined;
    psDefn->Projection = KvUserDefined;
    psDefn->CTProjection = KvUserDefined;

    psDefn->nParms = 0;
    for( int i = 0; i < MAX_GTIF_PROJPARMS; i++ )
    {
        psDefn->ProjParm[i] = 0.0;
        psDefn->ProjParmId[i] = 0;
    }

    psDefn->MapSys = KvUserDefined;
    psDefn->Zone = 0;

/* -------------------------------------------------------------------- */
/*	Try to get the overall model type.				*/
/* -------------------------------------------------------------------- */
    GetMetadataElement( "GEOTIFF_NUM::1024::GTModelTypeGeoKey",
                        &(psDefn->Model) );

/* -------------------------------------------------------------------- */
/*      Try to get a PCS.                                               */
/* -------------------------------------------------------------------- */
    if( GetMetadataElement( "GEOTIFF_NUM::3072::ProjectedCSTypeGeoKey",
                            &(psDefn->PCS) )
        && psDefn->PCS != KvUserDefined )
    {
        /*
         * Translate this into useful information.
         */
        GTIFGetPCSInfo( psDefn->PCS, NULL, &(psDefn->ProjCode),
                        &(psDefn->UOMLength), &(psDefn->GCS) );
    }

/* -------------------------------------------------------------------- */
/*       If we have the PCS code, but didn't find it in the CSV files   */
/*      (likely because we can't find them) we will try some ``jiffy    */
/*      rules'' for UTM and state plane.                                */
/* -------------------------------------------------------------------- */
    if( psDefn->PCS != KvUserDefined && psDefn->ProjCode == KvUserDefined )
    {
        int	nMapSys, nZone;
        int	nGCS = psDefn->GCS;

        nMapSys = GTIFPCSToMapSys( psDefn->PCS, &nGCS, &nZone );
        if( nMapSys != KvUserDefined )
        {
            psDefn->ProjCode = (short) GTIFMapSysToProj( nMapSys, nZone );
            psDefn->GCS = (short) nGCS;
        }
    }
   
/* -------------------------------------------------------------------- */
/*      If the Proj_ code is specified directly, use that.              */
/* -------------------------------------------------------------------- */
    if( psDefn->ProjCode == KvUserDefined )
        GetMetadataElement( "GEOTIFF_NUM::3074::ProjectionGeoKey",
                            &(psDefn->ProjCode) );
    
    if( psDefn->ProjCode != KvUserDefined )
    {
        /*
         * We have an underlying projection transformation value.  Look
         * this up.  For a PCS of ``WGS 84 / UTM 11'' the transformation
         * would be Transverse Mercator, with a particular set of options.
         * The nProjTRFCode itself would correspond to the name
         * ``UTM zone 11N'', and doesn't include datum info.
         */
        GTIFGetProjTRFInfo( psDefn->ProjCode, NULL, &(psDefn->Projection),
                            psDefn->ProjParm );
        
        /*
         * Set the GeoTIFF identity of the parameters.
         */
        psDefn->CTProjection = (short)
            EPSGProjMethodToCTProjMethod( psDefn->Projection );

        SetGTParmIds( psDefn->CTProjection, psDefn->ProjParmId, NULL);
        psDefn->nParms = 7;
    }

/* -------------------------------------------------------------------- */
/*      Try to get a GCS.  If found, it will override any implied by    */
/*      the PCS.                                                        */
/* -------------------------------------------------------------------- */
    GetMetadataElement( "GEOTIFF_NUM::2048::GeographicTypeGeoKey",
                        &(psDefn->GCS) );

/* -------------------------------------------------------------------- */
/*      Derive the datum, and prime meridian from the GCS.              */
/* -------------------------------------------------------------------- */
    if( psDefn->GCS != KvUserDefined )
    {
        GTIFGetGCSInfo( psDefn->GCS, NULL, &(psDefn->Datum), &(psDefn->PM),
                        &(psDefn->UOMAngle) );
    }
    
/* -------------------------------------------------------------------- */
/*      Handle the GCS angular units.  GeogAngularUnitsGeoKey           */
/*      overrides the GCS or PCS setting.                               */
/* -------------------------------------------------------------------- */
    GetMetadataElement( "GEOTIFF_NUM::2054::GeogAngularUnitsGeoKey",
                        &(psDefn->UOMAngle) );
    if( psDefn->UOMAngle != KvUserDefined )
    {
        GTIFGetUOMAngleInfo( psDefn->UOMAngle, NULL,
                             &(psDefn->UOMAngleInDegrees) );
    }

/* -------------------------------------------------------------------- */
/*      Check for a datum setting, and then use the datum to derive     */
/*      an ellipsoid.                                                   */
/* -------------------------------------------------------------------- */
    GetMetadataElement( "GEOTIFF_NUM::2050::GeogGeodeticDatumGeoKey",
                        &(psDefn->Datum) );

    if( psDefn->Datum != KvUserDefined )
    {
        GTIFGetDatumInfo( psDefn->Datum, NULL, &(psDefn->Ellipsoid) );
    }

/* -------------------------------------------------------------------- */
/*      Check for an explicit ellipsoid.  Use the ellipsoid to          */
/*      derive the ellipsoid characteristics, if possible.              */
/* -------------------------------------------------------------------- */
    GetMetadataElement( "GEOTIFF_NUM::2056::GeogEllipsoidGeoKey",
                        &(psDefn->Ellipsoid) );

    if( psDefn->Ellipsoid != KvUserDefined )
    {
        GTIFGetEllipsoidInfo( psDefn->Ellipsoid, NULL,
                              &(psDefn->SemiMajor), &(psDefn->SemiMinor) );
    }

/* -------------------------------------------------------------------- */
/*      Check for overridden ellipsoid parameters.  It would be nice    */
/*      to warn if they conflict with provided information, but for     */
/*      now we just override.                                           */
/* -------------------------------------------------------------------- */
    GetMetadataElement( "GEOTIFF_NUM::2057::GeogSemiMajorAxisGeoKey",
                        &(psDefn->SemiMajor) );
    GetMetadataElement( "GEOTIFF_NUM::2058::GeogSemiMinorAxisGeoKey",
                        &(psDefn->SemiMinor) );
    
    if( GetMetadataElement( "GEOTIFF_NUM::2059::GeogInvFlatteningGeoKey",
                            &dfInvFlattening ) == 1 )
    {
        if( dfInvFlattening != 0.0 )
            psDefn->SemiMinor = 
                psDefn->SemiMajor * (1 - 1.0/dfInvFlattening);
    }

/* -------------------------------------------------------------------- */
/*      Get the prime meridian info.                                    */
/* -------------------------------------------------------------------- */
    GetMetadataElement( "GEOTIFF_NUM::2051::GeogPrimeMeridianGeoKey",
                        &(psDefn->PM) );

    if( psDefn->PM != KvUserDefined )
    {
        GTIFGetPMInfo( psDefn->PM, NULL, &(psDefn->PMLongToGreenwich) );
    }
    else
    {
        GetMetadataElement( "GEOTIFF_NUM::2061::GeogPrimeMeridianLongGeoKey",
                            &(psDefn->PMLongToGreenwich) );

        psDefn->PMLongToGreenwich =
            GTIFAngleToDD( psDefn->PMLongToGreenwich,
                           psDefn->UOMAngle );
    }

/* -------------------------------------------------------------------- */
/*      Have the projection units of measure been overridden?  We       */
/*      should likely be doing something about angular units too,       */
/*      but these are very rarely not decimal degrees for actual        */
/*      file coordinates.                                               */
/* -------------------------------------------------------------------- */
    GetMetadataElement( "GEOTIFF_NUM::3076::ProjLinearUnitsGeoKey",
                        &(psDefn->UOMLength) );

    if( psDefn->UOMLength != KvUserDefined )
    {
        GTIFGetUOMLengthInfo( psDefn->UOMLength, NULL,
                              &(psDefn->UOMLengthInMeters) );
    }

/* -------------------------------------------------------------------- */
/*      Handle a variety of user defined transform types.               */
/* -------------------------------------------------------------------- */
    if( GetMetadataElement( "GEOTIFF_NUM::3075::ProjCoordTransGeoKey",
                            &(psDefn->CTProjection) ) )
    {
        FetchProjParms();
    }

/* -------------------------------------------------------------------- */
/*      Try to set the zoned map system information.                    */
/* -------------------------------------------------------------------- */
    psDefn->MapSys = GTIFProjToMapSys( psDefn->ProjCode, &(psDefn->Zone) );

/* -------------------------------------------------------------------- */
/*      If this is UTM, and we were unable to extract the projection    */
/*      parameters from the CSV file, just set them directly now,       */
/*      since it's pretty easy, and a common case.                      */
/* -------------------------------------------------------------------- */
    if( (psDefn->MapSys == MapSys_UTM_North
         || psDefn->MapSys == MapSys_UTM_South)
        && psDefn->CTProjection == KvUserDefined )
    {
        psDefn->CTProjection = CT_TransverseMercator;
        psDefn->nParms = 7;
        psDefn->ProjParmId[0] = ProjNatOriginLatGeoKey;
        psDefn->ProjParm[0] = 0.0;
            
        psDefn->ProjParmId[1] = ProjNatOriginLongGeoKey;
        psDefn->ProjParm[1] = psDefn->Zone*6 - 183.0;
        
        psDefn->ProjParmId[4] = ProjScaleAtNatOriginGeoKey;
        psDefn->ProjParm[4] = 0.9996;
        
        psDefn->ProjParmId[5] = ProjFalseEastingGeoKey;
        psDefn->ProjParm[5] = 500000.0;
        
        psDefn->ProjParmId[6] = ProjFalseNorthingGeoKey;

        if( psDefn->MapSys == MapSys_UTM_North )
            psDefn->ProjParm[6] = 0.0;
        else
            psDefn->ProjParm[6] = 10000000.0;
    }

    if ( pszProjection )
        CPLFree( pszProjection );
    pszProjection = GetOGISDefn( psDefn );
}

/************************************************************************/
/*                          GetOGISDefn()                               */
/*  Copied from the gt_wkt_srs.cpp.                                     */
/************************************************************************/

char *MrSIDDataset::GetOGISDefn( GTIFDefn *psDefn )
{
    OGRSpatialReference	oSRS;

    if( psDefn->Model != ModelTypeProjected 
        && psDefn->Model != ModelTypeGeographic )
        return CPLStrdup("");
    
/* -------------------------------------------------------------------- */
/*      If this is a projected SRS we set the PROJCS keyword first      */
/*      to ensure that the GEOGCS will be a child.                      */
/* -------------------------------------------------------------------- */
    if( psDefn->Model == ModelTypeProjected )
    {
        char	*pszPCSName = "unnamed";
        int         bNeedFree = FALSE;

        if( psDefn->PCS != KvUserDefined )
        {

            if( GTIFGetPCSInfo( psDefn->PCS, &pszPCSName, NULL, NULL, NULL ) )
                bNeedFree = TRUE;
            
            oSRS.SetNode( "PROJCS", pszPCSName );
            if( bNeedFree )
                GTIFFreeMemory( pszPCSName );

            oSRS.SetAuthority( "PROJCS", "EPSG", psDefn->PCS );
        }
        else
        {
            char szPCSName[200];
            strcpy( szPCSName, "unnamed" );
            if ( GetMetadataElement( "GEOTIFF_NUM::1026::GTCitationGeoKey",
                                     szPCSName, sizeof(szPCSName) ) )
                oSRS.SetNode( "PROJCS", szPCSName );
        }
    }
    
/* ==================================================================== */
/*      Setup the GeogCS                                                */
/* ==================================================================== */
    char	*pszGeogName = NULL;
    char	*pszDatumName = NULL;
    char	*pszPMName = NULL;
    char	*pszSpheroidName = NULL;
    char	*pszAngularUnits = NULL;
    double	dfInvFlattening, dfSemiMajor;
    char        szGCSName[200];
    
    if( GetMetadataElement( "GEOTIFF_NUM::2049::GeogCitationGeoKey",
                            szGCSName, sizeof(szGCSName) ) )
        pszGeogName = CPLStrdup(szGCSName);
    GTIFGetGCSInfo( psDefn->GCS, &pszGeogName, NULL, NULL, NULL );
    GTIFGetDatumInfo( psDefn->Datum, &pszDatumName, NULL );
    GTIFGetPMInfo( psDefn->PM, &pszPMName, NULL );
    GTIFGetEllipsoidInfo( psDefn->Ellipsoid, &pszSpheroidName, NULL, NULL );
    
    GTIFGetUOMAngleInfo( psDefn->UOMAngle, &pszAngularUnits, NULL );
    if( pszAngularUnits == NULL )
        pszAngularUnits = CPLStrdup("unknown");

    if( pszDatumName != NULL )
        WKTMassageDatum( &pszDatumName );

    dfSemiMajor = psDefn->SemiMajor;
    if( psDefn->SemiMajor == 0.0 )
    {
        pszSpheroidName = CPLStrdup("unretrievable - using WGS84");
        dfSemiMajor = SRS_WGS84_SEMIMAJOR;
        dfInvFlattening = SRS_WGS84_INVFLATTENING;
    }
    else if( (psDefn->SemiMinor / psDefn->SemiMajor) < 0.99999999999999999
             || (psDefn->SemiMinor / psDefn->SemiMajor) > 1.00000000000000001 )
        dfInvFlattening = -1.0 / (psDefn->SemiMinor/psDefn->SemiMajor - 1.0);
    else
        dfInvFlattening = 0.0; /* special flag for infinity */

    oSRS.SetGeogCS( pszGeogName, pszDatumName, 
                    pszSpheroidName, dfSemiMajor, dfInvFlattening,
                    pszPMName,
                    psDefn->PMLongToGreenwich / psDefn->UOMAngleInDegrees,
                    pszAngularUnits,
                    psDefn->UOMAngleInDegrees * 0.0174532925199433 );

    if( psDefn->GCS != KvUserDefined )
        oSRS.SetAuthority( "GEOGCS", "EPSG", psDefn->GCS );

    if( psDefn->Datum != KvUserDefined )
        oSRS.SetAuthority( "DATUM", "EPSG", psDefn->Datum );

    if( psDefn->Ellipsoid != KvUserDefined )
        oSRS.SetAuthority( "SPHEROID", "EPSG", psDefn->Ellipsoid );

    GTIFFreeMemory( pszGeogName );
    CPLFree( pszDatumName );
    GTIFFreeMemory( pszPMName );
    GTIFFreeMemory( pszSpheroidName );
    GTIFFreeMemory( pszAngularUnits );
        
/* ==================================================================== */
/*      Handle projection parameters.                                   */
/* ==================================================================== */
    if( psDefn->Model == ModelTypeProjected )
    {
/* -------------------------------------------------------------------- */
/*      Make a local copy of parms, and convert back into the           */
/*      angular units of the GEOGCS and the linear units of the         */
/*      projection.                                                     */
/* -------------------------------------------------------------------- */
        double		adfParm[10];
        int		i;

        for( i = 0; i < MIN(10,psDefn->nParms); i++ )
            adfParm[i] = psDefn->ProjParm[i];

        adfParm[0] /= psDefn->UOMAngleInDegrees;
        adfParm[1] /= psDefn->UOMAngleInDegrees;
        adfParm[2] /= psDefn->UOMAngleInDegrees;
        adfParm[3] /= psDefn->UOMAngleInDegrees;
        
        adfParm[5] /= psDefn->UOMLengthInMeters;
        adfParm[6] /= psDefn->UOMLengthInMeters;
        
/* -------------------------------------------------------------------- */
/*      Translation the fundamental projection.                         */
/* -------------------------------------------------------------------- */
        switch( psDefn->CTProjection )
        {
          case CT_TransverseMercator:
            oSRS.SetTM( adfParm[0], adfParm[1],
                        adfParm[4],
                        adfParm[5], adfParm[6] );
            break;

          case CT_TransvMercator_SouthOriented:
            oSRS.SetTMSO( adfParm[0], adfParm[1],
                          adfParm[4],
                          adfParm[5], adfParm[6] );
            break;

          case CT_Mercator:
            oSRS.SetMercator( adfParm[0], adfParm[1],
                              adfParm[4],
                              adfParm[5], adfParm[6] );
            break;

          case CT_ObliqueStereographic:
            oSRS.SetOS( adfParm[0], adfParm[1],
                        adfParm[4],
                        adfParm[5], adfParm[6] );
            break;

          case CT_Stereographic:
            oSRS.SetOS( adfParm[0], adfParm[1],
                        adfParm[4],
                        adfParm[5], adfParm[6] );
            break;

          case CT_ObliqueMercator: /* hotine */
            oSRS.SetHOM( adfParm[0], adfParm[1],
                         adfParm[2], adfParm[3],
                         adfParm[4],
                         adfParm[5], adfParm[6] );
            break;
        
          case CT_EquidistantConic: 
            oSRS.SetEC( adfParm[0], adfParm[1],
                        adfParm[2], adfParm[3],
                        adfParm[5], adfParm[6] );
            break;
        
          case CT_CassiniSoldner:
            oSRS.SetCS( adfParm[0], adfParm[1],
                        adfParm[5], adfParm[6] );
            break;
        
          case CT_Polyconic:
            oSRS.SetPolyconic( adfParm[0], adfParm[1],
                               adfParm[5], adfParm[6] );
            break;

          case CT_AzimuthalEquidistant:
            oSRS.SetAE( adfParm[0], adfParm[1],
                        adfParm[5], adfParm[6] );
            break;
        
          case CT_MillerCylindrical:
            oSRS.SetMC( adfParm[0], adfParm[1],
                        adfParm[5], adfParm[6] );
            break;
        
          case CT_Equirectangular:
            oSRS.SetEquirectangular( adfParm[0], adfParm[1],
                                     adfParm[5], adfParm[6] );
            break;
        
          case CT_Gnomonic:
            oSRS.SetGnomonic( adfParm[0], adfParm[1],
                              adfParm[5], adfParm[6] );
            break;
        
          case CT_LambertAzimEqualArea:
            oSRS.SetLAEA( adfParm[0], adfParm[1],
                          adfParm[5], adfParm[6] );
            break;
        
          case CT_Orthographic:
            oSRS.SetOrthographic( adfParm[0], adfParm[1],
                                  adfParm[5], adfParm[6] );
            break;
        
          case CT_Robinson:
            oSRS.SetRobinson( adfParm[1],
                              adfParm[5], adfParm[6] );
            break;
        
          case CT_Sinusoidal:
            oSRS.SetSinusoidal( adfParm[1],
                                adfParm[5], adfParm[6] );
            break;
        
          case CT_VanDerGrinten:
            oSRS.SetVDG( adfParm[1],
                         adfParm[5], adfParm[6] );
            break;

          case CT_PolarStereographic:
            oSRS.SetPS( adfParm[0], adfParm[1],
                        adfParm[4],
                        adfParm[5], adfParm[6] );
            break;
        
          case CT_LambertConfConic_2SP:
            oSRS.SetLCC( adfParm[2], adfParm[3],
                         adfParm[0], adfParm[1],
                         adfParm[5], adfParm[6] );
            break;

          case CT_LambertConfConic_1SP:
            oSRS.SetLCC1SP( adfParm[0], adfParm[1],
                            adfParm[4],
                            adfParm[5], adfParm[6] );
            break;
        
          case CT_AlbersEqualArea:
            oSRS.SetACEA( adfParm[0], adfParm[1],
                          adfParm[2], adfParm[3],
                          adfParm[5], adfParm[6] );
            break;

          case CT_NewZealandMapGrid:
            oSRS.SetNZMG( adfParm[0], adfParm[1],
                          adfParm[5], adfParm[6] );
            break;
        }

/* -------------------------------------------------------------------- */
/*      Set projection units.                                           */
/* -------------------------------------------------------------------- */
        char	*pszUnitsName = NULL;
        
        GTIFGetUOMLengthInfo( psDefn->UOMLength, &pszUnitsName, NULL );

        if( pszUnitsName != NULL && psDefn->UOMLength != KvUserDefined )
        {
            oSRS.SetLinearUnits( pszUnitsName, psDefn->UOMLengthInMeters );
            oSRS.SetAuthority( "PROJCS|UNIT", "EPSG", psDefn->UOMLength );
        }
        else
            oSRS.SetLinearUnits( "unknown", psDefn->UOMLengthInMeters );

        GTIFFreeMemory( pszUnitsName );
    }
    
/* -------------------------------------------------------------------- */
/*      Return the WKT serialization of the object.                     */
/* -------------------------------------------------------------------- */
    char	*pszWKT;

    oSRS.FixupOrdering();

    if( oSRS.exportToWkt( &pszWKT ) == OGRERR_NONE )
        return pszWKT;
    else
        return NULL;
}

