/******************************************************************************
 * $Id$
 *
 * Project:  Erdas Imagine (.img) Translator
 * Purpose:  Mapping Imagine georeferencing to GeoTIFF georeferencing.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Intergraph Corporation
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
 ****************************************************************************/

#include "hfa_p.h"

#include "cpl_conv.h"

#include "geotiff.h"
#include "geo_tiffp.h"
#include "geo_keyp.h"
#include "geovalues.h"

CPL_CVSID("$Id$");

#ifndef PI
#  define PI 3.14159265358979323846
#endif

#define RAD_TO_DEG	(180/PI)
#define MapSys_State_Plane	-9003


CPL_C_START
int	GTIFMapSysToPCS( int MapSys, int Datum, int nZone );
CPLErr ImagineToGeoTIFFProjection( HFAHandle hHFA, TIFF * hTIFF );
CPL_C_END

/* ==================================================================== */
/*      Table relating USGS and ESRI state plane zones.                 */
/* ==================================================================== */
int anUsgsEsriZones[] =
{
  101, 3101,
  102, 3126,
  201, 3151,
  202, 3176,
  203, 3201,
  301, 3226,
  302, 3251,
  401, 3276,
  402, 3301,
  403, 3326,
  404, 3351,
  405, 3376,
  406, 3401,
  407, 3426,
  501, 3451,
  502, 3476,
  503, 3501,
  600, 3526,
  700, 3551,
  901, 3601,
  902, 3626,
  903, 3576,
 1001, 3651,
 1002, 3676,
 1101, 3701,
 1102, 3726,
 1103, 3751,
 1201, 3776,
 1202, 3801,
 1301, 3826,
 1302, 3851,
 1401, 3876,
 1402, 3901,
 1501, 3926,
 1502, 3951,
 1601, 3976,
 1602, 4001,
 1701, 4026,
 1702, 4051,
 1703, 6426,
 1801, 4076,
 1802, 4101,
 1900, 4126,
 2001, 4151,
 2002, 4176,
 2101, 4201,
 2102, 4226,
 2103, 4251,
 2111, 6351,
 2112, 6376,
 2113, 6401,
 2201, 4276,
 2202, 4301,
 2203, 4326,
 2301, 4351,
 2302, 4376,
 2401, 4401,
 2402, 4426,
 2403, 4451,
 2500,    0,
 2501, 4476,
 2502, 4501,
 2503, 4526,
 2600,    0,
 2601, 4551,
 2602, 4576,
 2701, 4601,
 2702, 4626,
 2703, 4651,
 2800, 4676,
 2900, 4701,
 3001, 4726,
 3002, 4751,
 3003, 4776,
 3101, 4801,
 3102, 4826,
 3103, 4851,
 3104, 4876,
 3200, 4901,
 3301, 4926,
 3302, 4951,
 3401, 4976,
 3402, 5001,
 3501, 5026,
 3502, 5051,
 3601, 5076,
 3602, 5101,
 3701, 5126,
 3702, 5151,
 3800, 5176,
 3900,    0,
 3901, 5201,
 3902, 5226,
 4001, 5251,
 4002, 5276,
 4100, 5301,
 4201, 5326,
 4202, 5351,
 4203, 5376,
 4204, 5401,
 4205, 5426,
 4301, 5451,
 4302, 5476,
 4303, 5501,
 4400, 5526,
 4501, 5551,
 4502, 5576,
 4601, 5601,
 4602, 5626,
 4701, 5651,
 4702, 5676,
 4801, 5701,
 4802, 5726,
 4803, 5751,
 4901, 5776,
 4902, 5801,
 4903, 5826,
 4904, 5851,
 5001, 6101,
 5002, 6126,
 5003, 6151,
 5004, 6176,
 5005, 6201,
 5006, 6226,
 5007, 6251,
 5008, 6276,
 5009, 6301,
 5010, 6326,
 5101, 5876,
 5102, 5901,
 5103, 5926,
 5104, 5951,
 5105, 5976,
 5201, 6001,
 5200, 6026,
 5200, 6076,
 5201, 6051,
 5202, 6051,
 5300,    0, 
 5400,    0
};

/************************************************************************/
/*                           ESRIToUSGSZone()                           */
/*                                                                      */
/*      Convert ESRI style state plane zones to USGS style state        */
/*      plane zones.                                                    */
/************************************************************************/

static int ESRIToUSGSZone( int nESRIZone )

{
    int		nPairs = sizeof(anUsgsEsriZones) / (2*sizeof(int));
    int		i;
    
    for( i = 0; i < nPairs; i++ )
    {
        if( anUsgsEsriZones[i*2+1] == nESRIZone )
            return anUsgsEsriZones[i*2];
    }

    return 0;
}

/************************************************************************/
/*                     ImagineToGeoTIFFProjection()                     */
/*                                                                      */
/*      Apply ``Geo'' information to the GeoTIFF file based on the      */
/*      source Imagine file.                                            */
/************************************************************************/

CPLErr ImagineToGeoTIFFProjection( HFAHandle hHFA, TIFF * hTIFF )

{
    GTIF	*hGTiff;
    const Eprj_Datum	*psDatum;
    const Eprj_ProParameters *psProParameters;
    const Eprj_MapInfo *psMapInfo;
    int16	nPCS = KvUserDefined;

/* -------------------------------------------------------------------- */
/*      Get info on Imagine file projection.                            */
/* -------------------------------------------------------------------- */
    psMapInfo = HFAGetMapInfo( hHFA );
    psProParameters = HFAGetProParameters( hHFA );
    psDatum = HFAGetDatum( hHFA );
    
    if( psMapInfo == NULL )
        return CE_None;

    hGTiff = GTIFNew( hTIFF );

/* -------------------------------------------------------------------- */
/*      Write out the pixelisarea marker.                               */
/* -------------------------------------------------------------------- */
    GTIFKeySet(hGTiff, GTRasterTypeGeoKey, TYPE_SHORT,  1,
               RasterPixelIsArea );

/* -------------------------------------------------------------------- */
/*      Write out the corner coordinates.                               */
/* -------------------------------------------------------------------- */
    if( psMapInfo != NULL )
    {
        double adfPixelScale[3], adfTiePoints[6];

        adfPixelScale[0] = psMapInfo->pixelSize.width;
        adfPixelScale[1] = psMapInfo->pixelSize.height;
        adfPixelScale[2] = 0.0;

        TIFFSetField( hTIFF, TIFFTAG_GEOPIXELSCALE, 3, adfPixelScale );

        adfTiePoints[0] = 0.5;
        adfTiePoints[1] = 0.5;
        adfTiePoints[2] = 0.0;
        adfTiePoints[3] = psMapInfo->upperLeftCenter.x;
        adfTiePoints[4] = psMapInfo->upperLeftCenter.y;
        adfTiePoints[5] = 0.0;
        
        TIFFSetField( hTIFF, TIFFTAG_GEOTIEPOINTS, 6, adfTiePoints );
    }

/* -------------------------------------------------------------------- */
/*      Try to write out units information.                             */
/* -------------------------------------------------------------------- */
    if( psMapInfo != NULL )
    {
        if( EQUAL(psMapInfo->units,"meters") )
        {
            GTIFKeySet(hGTiff, ProjLinearUnitsGeoKey, TYPE_SHORT,  1,
                       Linear_Meter );
        }
        else if( EQUAL(psMapInfo->units,"feet") )
        {
            GTIFKeySet(hGTiff, ProjLinearUnitsGeoKey, TYPE_SHORT,  1,
                       Linear_Foot_US_Survey);
        }
        else if( EQUAL(psMapInfo->units,"dd") )
        {
            GTIFKeySet(hGTiff, GeogAngularUnitsGeoKey, TYPE_SHORT,  1,
                       Angular_Degree);
        }
    }

/* -------------------------------------------------------------------- */
/*      If this is not geographic, write out the model type as          */
/*      projected.  Note that this will get written even if we end      */
/*      up not translating the projection which may be unwise.          */
/* -------------------------------------------------------------------- */
    if( psProParameters != NULL )
    {
        if( psProParameters->proNumber == 0 )
            GTIFKeySet(hGTiff, GTModelTypeGeoKey, TYPE_SHORT,  1,
                       ModelTypeGeographic );
        else
            GTIFKeySet(hGTiff, GTModelTypeGeoKey, TYPE_SHORT,  1,
                       ModelTypeProjected );
    }
    
/* -------------------------------------------------------------------- */
/*      Do we have a ``nice'' UTM PCS situation?                        */
/* -------------------------------------------------------------------- */
    if( psProParameters != NULL
        && psProParameters->proNumber == 1 /* UTM */
        && psProParameters->proParams[3] >= 0.0 /* north */
        && psDatum != NULL
        && EQUAL(psDatum->datumname,"NAD27") )
    {
        nPCS = 26700 + psProParameters->proZone;
    }

    if( psProParameters != NULL
        && psProParameters->proNumber == 1 /* UTM */
        && psProParameters->proParams[3] >= 0.0 /* north */
        && psDatum != NULL
        && EQUAL(psDatum->datumname,"NAD83") )
    {
        nPCS = 26800 + psProParameters->proZone;
    }

    if( psProParameters != NULL
        && psProParameters->proNumber == 1 /* UTM */
        && psProParameters->proParams[3] >= 0.0 /* north */
        && psDatum != NULL
        && EQUAL(psDatum->datumname,"WGS 84") )
    {
        /* I don't know if the above datum names really work ... just a guess*/
        nPCS = 32600 + psProParameters->proZone;
    }

    if( psProParameters != NULL
        && psProParameters->proNumber == 1 /* UTM */
        && psProParameters->proParams[3] < 0.0 /* south */
        && psDatum != NULL
        && EQUAL(psDatum->datumname,"WGS 84") )
    {
        /* I don't know if the above datum names really work ... just a guess*/
        nPCS = 32700 + psProParameters->proZone;
    }

/* -------------------------------------------------------------------- */
/*      Do we have a nice State Plane situation?                        */
/*                                                                      */
/*      For this we have to translate the Erdas (ESRI) zone number      */
/*      to a USGS zone number.  This we pass to GTIFMapSysToPCS()       */
/*      which turns it into a Proj_ code, and then it does a lookup     */
/*      to map this to a PCS_ number.  Oiii!                            */
/* -------------------------------------------------------------------- */
    if( psProParameters != NULL
        && psProParameters->proNumber == 2 )
    {
        if( psProParameters->proParams[0] == 0.0 ) /* NAD27 */
        {
            nPCS = GTIFMapSysToPCS( MapSys_State_Plane, GCS_NAD27,
                                    ESRIToUSGSZone(psProParameters->proZone));
        }
        else
        {
            nPCS = GTIFMapSysToPCS( MapSys_State_Plane, GCS_NAD83,
                                    ESRIToUSGSZone(psProParameters->proZone));
        }
    }

/* -------------------------------------------------------------------- */
/*      Write with a PCS if we have one.                                */
/* -------------------------------------------------------------------- */
    if( nPCS != KvUserDefined )
    {
        GTIFKeySet(hGTiff, ProjectedCSTypeGeoKey, TYPE_SHORT,  1,
                   nPCS );
    }
    else if( psProParameters != NULL
             && psProParameters->proNumber != 0 )
    {
        GTIFKeySet(hGTiff, ProjectedCSTypeGeoKey, TYPE_SHORT,  1,
                   KvUserDefined );
        GTIFKeySet(hGTiff, ProjectionGeoKey, TYPE_SHORT,  1,
                   KvUserDefined );
    }

/* ==================================================================== */
/*      Handle various non-PCS situations                               */
/* ==================================================================== */
    if( nPCS != KvUserDefined )
    {
        /* already handled */
    }
    else if( psProParameters == NULL )
    {
        /* do nothing */
    }

/* -------------------------------------------------------------------- */
/*      Lat/long.                                                       */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 0 )
    {
        /* GTModelType and GeogLinearUnits already written */
    }

/* -------------------------------------------------------------------- */
/*      UTM                                                             */
/*                                                                      */
/*      If we didn't match to a particular PCS we are just going to     */
/*      fallback to the TM definition of this zone.                     */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 1 )
    {
        /* notdef: 
        if( psProParameters->proParams[3] < 0.0 / * south * /
        ... */
    }

/* -------------------------------------------------------------------- */
/*      State Plane                                                     */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 2 )
    {
        /* notdef: this is complicated by the translation of ESRI state
           plane numbers to USGS (GeoTIFF) numbering */
    }

/* -------------------------------------------------------------------- */
/*      Albers Conic Equal Area                                         */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 3 )
    {
        GTIFKeySet( hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                    CT_AlbersEqualArea );
        GTIFKeySet( hGTiff, ProjStdParallelGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[2] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjStdParallel2GeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[3] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[4] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjNatOriginLatGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[5] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[6] );
        GTIFKeySet( hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[7] );
    }
    
/* -------------------------------------------------------------------- */
/*      Lambert Conformal Conic                                         */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 4 )
    {
        GTIFKeySet( hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                    CT_LambertConfConic_2SP );
        GTIFKeySet( hGTiff, ProjStdParallelGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[2] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjStdParallel2GeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[3] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjFalseOriginLongGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[4] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjNatOriginLatGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[5] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[6] );
        GTIFKeySet( hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[7] );
    }
    
/* -------------------------------------------------------------------- */
/*      Mercator                                                        */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 5 )
    {
        GTIFKeySet( hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                    CT_Mercator );
        GTIFKeySet( hGTiff, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[4] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjNatOriginLatGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[5] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[6] );
        GTIFKeySet( hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[7] );
    }
    
/* -------------------------------------------------------------------- */
/*      Polar Stereographic                                             */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 6 )
    {
        GTIFKeySet( hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                    CT_PolarStereographic );
        GTIFKeySet( hGTiff, ProjStraightVertPoleLongGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[4] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjNatOriginLatGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[5] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[6] );
        GTIFKeySet( hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[7] );
    }
    
/* -------------------------------------------------------------------- */
/*      Polyconic                                                       */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 7 )
    {
        GTIFKeySet( hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                    CT_Polyconic );
        GTIFKeySet( hGTiff, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[4] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjCenterLatGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[5] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[6] );
        GTIFKeySet( hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[7] );
    }
    
/* -------------------------------------------------------------------- */
/*      Equidistant Conic                                               */
/*                                                                      */
/*      Note that Imagine files have two cases (keyed off param 8       */
/*      being zero or one) indicating whether a second standard         */
/*      parallel is present.                                            */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 8 )
    {
        GTIFKeySet( hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                    CT_EquidistantConic );
        GTIFKeySet( hGTiff, ProjStdParallelGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[2] * RAD_TO_DEG );

        if( psProParameters->proParams[8] != 0.0 )
            GTIFKeySet( hGTiff, ProjStdParallel2GeoKey, TYPE_DOUBLE, 1,
                        psProParameters->proParams[3] * RAD_TO_DEG );
        
        GTIFKeySet( hGTiff, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[4] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjNatOriginLatGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[5] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[6] );
        GTIFKeySet( hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[7] );
    }
    
/* -------------------------------------------------------------------- */
/*      Transverse Mercator                                             */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 9 )
    {
        GTIFKeySet( hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                    CT_TransverseMercator );
        GTIFKeySet( hGTiff, ProjScaleAtNatOriginGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[2] );
        GTIFKeySet( hGTiff, ProjNatOriginLongGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[4] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjNatOriginLatGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[5] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[6] );
        GTIFKeySet( hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[7] );
    }
    
/* -------------------------------------------------------------------- */
/*      Stereographic                                                   */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 10 )
    {
        GTIFKeySet( hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                    CT_Stereographic );
        GTIFKeySet( hGTiff, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[4] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjCenterLatGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[5] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[6] );
        GTIFKeySet( hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[7] );
    }
    
    
/* -------------------------------------------------------------------- */
/*      Lambert Azimuthal Equal-Area                                    */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 11 )
    {
        GTIFKeySet( hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                    CT_LambertAzimEqualArea );
        GTIFKeySet( hGTiff, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[4] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjCenterLatGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[5] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[6] );
        GTIFKeySet( hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[7] );
    }
    
/* -------------------------------------------------------------------- */
/*      Azimuthal Equidistant                                           */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 12 )
    {
        GTIFKeySet( hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                    CT_AzimuthalEquidistant );
        GTIFKeySet( hGTiff, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[4] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjCenterLatGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[5] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[6] );
        GTIFKeySet( hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[7] );
    }

/* -------------------------------------------------------------------- */
/*      Gnomonic                                                        */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 13 )
    {
        GTIFKeySet( hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                    CT_Gnomonic );
        GTIFKeySet( hGTiff, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[4] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjCenterLatGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[5] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[6] );
        GTIFKeySet( hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[7] );
    }
    
/* -------------------------------------------------------------------- */
/*      Orthographic                                                    */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 14 )
    {
        GTIFKeySet( hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                    CT_Orthographic );
        GTIFKeySet( hGTiff, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[4] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjCenterLatGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[5] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[6] );
        GTIFKeySet( hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[7] );
    }
    
/* -------------------------------------------------------------------- */
/*      General Vertical Near-Side Perspective                          */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 15 )
    {
        /* no mapping! */
    }
    
/* -------------------------------------------------------------------- */
/*      Sinusoidal                                                      */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 16 )
    {
        GTIFKeySet( hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                    CT_Sinusoidal );
        GTIFKeySet( hGTiff, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[4] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[6] );
        GTIFKeySet( hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[7] );
    }
    
/* -------------------------------------------------------------------- */
/*      Equirectangular                                                 */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 17 )
    {
        GTIFKeySet( hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                    CT_Equirectangular );
        GTIFKeySet( hGTiff, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[4] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjCenterLatGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[5] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[6] );
        GTIFKeySet( hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[7] );
    }
    
/* -------------------------------------------------------------------- */
/*      Miller Cylindrical                                              */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 18 )
    {
        GTIFKeySet( hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                    CT_MillerCylindrical );
        GTIFKeySet( hGTiff, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[4] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[6] );
        GTIFKeySet( hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[7] );
    }
    
/* -------------------------------------------------------------------- */
/*      Van der Grinten I                                               */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 19 )
    {
        GTIFKeySet( hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                    CT_VanDerGrinten );
        GTIFKeySet( hGTiff, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[4] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[6] );
        GTIFKeySet( hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[7] );
    }
    
/* -------------------------------------------------------------------- */
/*      Oblique Mercator (Hotine) - Case 0                              */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 20
             && psProParameters->proParams[12] == 0.0 )
    {
#ifdef notdef        
        /* This does not appear to be a supported formulation for
           GeoTIFF, but I will write out the parameters and hope */
        
        GTIFKeySet( hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                    CT_ObliqueMercator );
        GTIFKeySet( hGTiff, ProjScaleAtNatOriginGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[2] );

        /* proParams[8-11] also existing in this case */
        
        GTIFKeySet( hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[6] );
        GTIFKeySet( hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[7] );
#endif        
    }
    
/* -------------------------------------------------------------------- */
/*      Oblique Mercator (Hotine) - Case 1                              */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 20
             && psProParameters->proParams[12] > 0.0 )
    {
        GTIFKeySet( hGTiff, ProjCoordTransGeoKey, TYPE_SHORT, 1,
                    CT_ObliqueMercator );
        GTIFKeySet( hGTiff, ProjScaleAtNatOriginGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[2] );
        GTIFKeySet( hGTiff, ProjAzimuthAngleGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[3] * RAD_TO_DEG );
        
        GTIFKeySet( hGTiff, ProjCenterLongGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[4] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjCenterLatGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[5] * RAD_TO_DEG );
        GTIFKeySet( hGTiff, ProjFalseEastingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[6] );
        GTIFKeySet( hGTiff, ProjFalseNorthingGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proParams[7] );
    }
    
/* -------------------------------------------------------------------- */
/*      Space Oblique Mercator                                          */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 21 )
    {
        /* not supported by GeoTIFF */
    }
    
/* -------------------------------------------------------------------- */
/*      Modified Transverse Mercator (MTM)                              */
/* -------------------------------------------------------------------- */
    else if( psProParameters->proNumber == 22 )
    {
        /* not supported by GeoTIFF */
    }

/* -------------------------------------------------------------------- */
/*      Ellipsoid                                                       */
/* -------------------------------------------------------------------- */
    if( psProParameters != NULL )
    {
        int	nEllipsoid = KvUserDefined;

        if( EQUAL(psProParameters->proSpheroid.sphereName,"WGS 84") )
            nEllipsoid = Ellipse_WGS_84;
        else if( EQUAL(psProParameters->proSpheroid.sphereName,"GRS 80") )
            nEllipsoid = Ellipse_GRS_1980;
        else if( EQUAL(psProParameters->proSpheroid.sphereName,"Clarke 1866") )
            nEllipsoid = Ellipse_Clarke_1866;
        else if( EQUAL(psProParameters->proSpheroid.sphereName,"Clarke 1880") )
            nEllipsoid = Ellipse_Clarke_1880;

        GTIFKeySet( hGTiff, GeogEllipsoidGeoKey, TYPE_SHORT, 1,
                    nEllipsoid );

        GTIFKeySet( hGTiff, GeogCitationGeoKey, TYPE_ASCII, 0,
                    psProParameters->proSpheroid.sphereName );

        GTIFKeySet( hGTiff, GeogSemiMajorAxisGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proSpheroid.a );
        GTIFKeySet( hGTiff, GeogSemiMinorAxisGeoKey, TYPE_DOUBLE, 1,
                    psProParameters->proSpheroid.b );
    }
    
/* -------------------------------------------------------------------- */
/*      Datum (GCS) - We always assume Greenwich                        */
/*                                                                      */
/*      If we used the parameter information available through the      */
/*      EPSG tables we could likely find an appropriate name for        */
/*      this datum if it has a seven term (or three term) Molondosky    */
/*      transform.                                                      */
/* -------------------------------------------------------------------- */
    if( psDatum != NULL )
    {
        int	nGCS = KvUserDefined;

        if( EQUAL(psDatum->datumname,"NAD27") )
            nGCS = GCS_NAD27;
        else if( EQUAL(psDatum->datumname,"NAD83") )
            nGCS = GCS_NAD83;
        else if( EQUAL(psDatum->datumname,"WGS 84") )
            nGCS = GCS_WGS_84;
        else if( EQUAL(psDatum->datumname,"WGS 72") )
            nGCS = GCS_WGS_72;

        GTIFKeySet( hGTiff, GeographicTypeGeoKey, TYPE_SHORT, 1,
                    nGCS );
    }
    
/* -------------------------------------------------------------------- */
/*      Write information to header and clean up.                       */
/* -------------------------------------------------------------------- */

    GTIFWriteKeys( hGTiff );
    GTIFFree( hGTiff );

    return CE_None;
}
