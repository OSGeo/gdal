/******************************************************************************
 * $Id$
 *
 * Project:  GView
 * Purpose:  Implementation of Atlantis HKV labelled blob support
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2008-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
 ****************************************************************************/

#include "cpl_string.h"
#include <ctype.h>
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"
#include "atlsci_spheroid.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                            HKVRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class HKVDataset;

class HKVRasterBand : public RawRasterBand
{
    friend class HKVDataset;

  public:
    		HKVRasterBand( HKVDataset *poDS, int nBand, VSILFILE * fpRaw, 
                               unsigned int nImgOffset, int nPixelOffset,
                               int nLineOffset,
                               GDALDataType eDataType, int bNativeOrder );
    virtual     ~HKVRasterBand() {};

    virtual CPLErr SetNoDataValue( double );
};

/************************************************************************/
/*                      HKV Spheroids                                   */
/************************************************************************/

class HKVSpheroidList : public SpheroidList
{
public:

  HKVSpheroidList();
  ~HKVSpheroidList() {};
};

HKVSpheroidList :: HKVSpheroidList()
{
  num_spheroids = 58;
  epsilonR = 0.1;
  epsilonI = 0.000001;

  spheroids[0].SetValuesByEqRadiusAndInvFlattening("airy-1830",6377563.396,299.3249646);
  spheroids[1].SetValuesByEqRadiusAndInvFlattening("modified-airy",6377340.189,299.3249646);
  spheroids[2].SetValuesByEqRadiusAndInvFlattening("australian-national",6378160,298.25);
  spheroids[3].SetValuesByEqRadiusAndInvFlattening("bessel-1841-namibia",6377483.865,299.1528128);
  spheroids[4].SetValuesByEqRadiusAndInvFlattening("bessel-1841",6377397.155,299.1528128);
  spheroids[5].SetValuesByEqRadiusAndInvFlattening("clarke-1858",6378294.0,294.297);
  spheroids[6].SetValuesByEqRadiusAndInvFlattening("clarke-1866",6378206.4,294.9786982);
  spheroids[7].SetValuesByEqRadiusAndInvFlattening("clarke-1880",6378249.145,293.465);
  spheroids[8].SetValuesByEqRadiusAndInvFlattening("everest-india-1830",6377276.345,300.8017);
  spheroids[9].SetValuesByEqRadiusAndInvFlattening("everest-sabah-sarawak",6377298.556,300.8017);
  spheroids[10].SetValuesByEqRadiusAndInvFlattening("everest-india-1956",6377301.243,300.8017);
  spheroids[11].SetValuesByEqRadiusAndInvFlattening("everest-malaysia-1969",6377295.664,300.8017);
  spheroids[12].SetValuesByEqRadiusAndInvFlattening("everest-malay-sing",6377304.063,300.8017);
  spheroids[13].SetValuesByEqRadiusAndInvFlattening("everest-pakistan",6377309.613,300.8017);
  spheroids[14].SetValuesByEqRadiusAndInvFlattening("modified-fisher-1960",6378155,298.3);
  spheroids[15].SetValuesByEqRadiusAndInvFlattening("helmert-1906",6378200,298.3);
  spheroids[16].SetValuesByEqRadiusAndInvFlattening("hough-1960",6378270,297);
  spheroids[17].SetValuesByEqRadiusAndInvFlattening("hughes",6378273.0,298.279);
  spheroids[18].SetValuesByEqRadiusAndInvFlattening("indonesian-1974",6378160,298.247);
  spheroids[19].SetValuesByEqRadiusAndInvFlattening("international-1924",6378388,297);
  spheroids[20].SetValuesByEqRadiusAndInvFlattening("iugc-67",6378160.0,298.254);
  spheroids[21].SetValuesByEqRadiusAndInvFlattening("iugc-75",6378140.0,298.25298);
  spheroids[22].SetValuesByEqRadiusAndInvFlattening("krassovsky-1940",6378245,298.3);
  spheroids[23].SetValuesByEqRadiusAndInvFlattening("kaula",6378165.0,292.308);
  spheroids[24].SetValuesByEqRadiusAndInvFlattening("grs-80",6378137,298.257222101);
  spheroids[25].SetValuesByEqRadiusAndInvFlattening("south-american-1969",6378160,298.25);
  spheroids[26].SetValuesByEqRadiusAndInvFlattening("wgs-72",6378135,298.26);
  spheroids[27].SetValuesByEqRadiusAndInvFlattening("wgs-84",6378137,298.257223563);
  spheroids[28].SetValuesByEqRadiusAndInvFlattening("ev-wgs-84",6378137.0,298.252841);
  spheroids[29].SetValuesByEqRadiusAndInvFlattening("ev-bessel",6377397.0,299.1976073);

  spheroids[30].SetValuesByEqRadiusAndInvFlattening("airy_1830",6377563.396,299.3249646);
  spheroids[31].SetValuesByEqRadiusAndInvFlattening("modified_airy",6377340.189,299.3249646);
  spheroids[32].SetValuesByEqRadiusAndInvFlattening("australian_national",6378160,298.25);
  spheroids[33].SetValuesByEqRadiusAndInvFlattening("bessel_1841_namibia",6377483.865,299.1528128);
  spheroids[34].SetValuesByEqRadiusAndInvFlattening("bessel_1841",6377397.155,299.1528128);
  spheroids[35].SetValuesByEqRadiusAndInvFlattening("clarke_1858",6378294.0,294.297);
  spheroids[36].SetValuesByEqRadiusAndInvFlattening("clarke_1866",6378206.4,294.9786982);
  spheroids[37].SetValuesByEqRadiusAndInvFlattening("clarke_1880",6378249.145,293.465);
  spheroids[38].SetValuesByEqRadiusAndInvFlattening("everest_india_1830",6377276.345,300.8017);
  spheroids[39].SetValuesByEqRadiusAndInvFlattening("everest_sabah_sarawak",6377298.556,300.8017);
  spheroids[40].SetValuesByEqRadiusAndInvFlattening("everest_india_1956",6377301.243,300.8017);
  spheroids[41].SetValuesByEqRadiusAndInvFlattening("everest_malaysia_1969",6377295.664,300.8017);
  spheroids[42].SetValuesByEqRadiusAndInvFlattening("everest_malay_sing",6377304.063,300.8017);
  spheroids[43].SetValuesByEqRadiusAndInvFlattening("everest_pakistan",6377309.613,300.8017);
  spheroids[44].SetValuesByEqRadiusAndInvFlattening("modified_fisher_1960",6378155,298.3);
  spheroids[45].SetValuesByEqRadiusAndInvFlattening("helmert_1906",6378200,298.3);
  spheroids[46].SetValuesByEqRadiusAndInvFlattening("hough_1960",6378270,297);
  spheroids[47].SetValuesByEqRadiusAndInvFlattening("indonesian_1974",6378160,298.247);
  spheroids[48].SetValuesByEqRadiusAndInvFlattening("international_1924",6378388,297);
  spheroids[49].SetValuesByEqRadiusAndInvFlattening("iugc_67",6378160.0,298.254);
  spheroids[50].SetValuesByEqRadiusAndInvFlattening("iugc_75",6378140.0,298.25298);
  spheroids[51].SetValuesByEqRadiusAndInvFlattening("krassovsky_1940",6378245,298.3);
  spheroids[52].SetValuesByEqRadiusAndInvFlattening("grs_80",6378137,298.257222101);
  spheroids[53].SetValuesByEqRadiusAndInvFlattening("south_american_1969",6378160,298.25);
  spheroids[54].SetValuesByEqRadiusAndInvFlattening("wgs_72",6378135,298.26);
  spheroids[55].SetValuesByEqRadiusAndInvFlattening("wgs_84",6378137,298.257223563);
  spheroids[56].SetValuesByEqRadiusAndInvFlattening("ev_wgs_84",6378137.0,298.252841);
  spheroids[57].SetValuesByEqRadiusAndInvFlattening("ev_bessel",6377397.0,299.1976073);
}

CPLErr SaveHKVAttribFile( const char *pszFilenameIn,
                          int nXSize, int nYSize, int nBands,
                          GDALDataType eType, int bNoDataSet,
                          double dfNoDataValue );

/************************************************************************/
/* ==================================================================== */
/*				HKVDataset				*/
/* ==================================================================== */
/************************************************************************/

class HKVDataset : public RawDataset
{
    friend class HKVRasterBand;

    char	*pszPath;
    VSILFILE	*fpBlob;

    int         nGCPCount;
    GDAL_GCP    *pasGCPList;

    void        ProcessGeoref(const char *);
    void        ProcessGeorefGCP(char **, const char *, double, double);
    void        SetVersion( float version_number );
    float       GetVersion();
    float       MFF2version;

    CPLErr      SetGCPProjection(const char *); /* for use in CreateCopy */

    GDALDataType eRasterType;

    void SetNoDataValue( double );

    char        *pszProjection;
    char        *pszGCPProjection;
    double      adfGeoTransform[6];

    char	**papszAttrib;

    int		bGeorefChanged;
    char	**papszGeoref;

   /* NOTE: The MFF2 format goes against GDAL's API in that nodata values are set
    *       per-dataset rather than per-band.  To compromise, for writing out, the
    *       dataset's nodata value will be set to the last value set on any of the
    *       raster bands.
    */

    int         bNoDataSet;
    int         bNoDataChanged;
    double      dfNoDataValue;

  public:
    		HKVDataset();
    virtual     ~HKVDataset();

    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );

    virtual CPLErr SetGeoTransform( double * );
    virtual CPLErr SetProjection( const char * );

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
    static GDALDataset *CreateCopy( const char * pszFilename, 
                                    GDALDataset *poSrcDS, 
                                    int bStrict, char ** papszOptions, 
                                    GDALProgressFunc pfnProgress, 
                                    void * pProgressData );

    static CPLErr Delete( const char * pszName );
};

/************************************************************************/
/* ==================================================================== */
/*                            HKVRasterBand                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           HKVRasterBand()                            */
/************************************************************************/

HKVRasterBand::HKVRasterBand( HKVDataset *poDSIn, int nBandIn, VSILFILE * fpRawIn,
                              unsigned int nImgOffsetIn, int nPixelOffsetIn,
                              int nLineOffsetIn,
                              GDALDataType eDataTypeIn, int bNativeOrderIn ) :
    RawRasterBand( reinterpret_cast<GDALDataset *>( poDSIn ), nBandIn, fpRawIn,
                   nImgOffsetIn, nPixelOffsetIn, nLineOffsetIn, eDataTypeIn,
                   bNativeOrderIn, TRUE )

{
    poDS = poDSIn;
    nBand = nBandIn;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr HKVRasterBand::SetNoDataValue( double dfNewValue )

{
    HKVDataset *poHKVDS = reinterpret_cast<HKVDataset *>( poDS );
    RawRasterBand::SetNoDataValue( dfNewValue );
    poHKVDS->SetNoDataValue( dfNewValue );

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*				HKVDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            HKVDataset()                             */
/************************************************************************/

HKVDataset::HKVDataset() :
    pszPath(NULL),
    fpBlob(NULL),
    nGCPCount(0),
    pasGCPList(NULL),
    // Initialize datasets to new version; change if necessary.
    MFF2version(1.1f),
    eRasterType(GDT_Unknown),
    pszProjection(CPLStrdup("")),
    pszGCPProjection(CPLStrdup("")),
    papszAttrib(NULL),
    bGeorefChanged(FALSE),
    papszGeoref(NULL),
    bNoDataSet(FALSE),
    bNoDataChanged(FALSE),
    dfNoDataValue(0.0)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~HKVDataset()                            */
/************************************************************************/

HKVDataset::~HKVDataset()

{
    FlushCache();
    if( bGeorefChanged )
    {
        const char *pszFilename = CPLFormFilename(pszPath, "georef", NULL );
        CSLSave( papszGeoref, pszFilename );
    }

    if( bNoDataChanged )
    {
        SaveHKVAttribFile(pszPath, 
                             this->nRasterXSize,
                             this->nRasterYSize,
                             this->nBands,
                             this->eRasterType,
                             this->bNoDataSet, 
                             this->dfNoDataValue );
    }

    if( fpBlob != NULL )
        VSIFCloseL( fpBlob );

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    CPLFree( pszProjection );
    CPLFree( pszGCPProjection );
    CPLFree( pszPath );
    CSLDestroy( papszGeoref );
    CSLDestroy( papszAttrib );
}

/************************************************************************/
/*                          SetVersion()                                */
/************************************************************************/

void HKVDataset::SetVersion(float version_number)

{
  //update stored info
  MFF2version = version_number;
}

/************************************************************************/
/*                          GetVersion()                                */
/************************************************************************/

float HKVDataset::GetVersion()

{
    return( MFF2version );
}

/************************************************************************/
/*                          SetNoDataValue()                            */
/************************************************************************/

void HKVDataset::SetNoDataValue( double dfNewValue )

{
    bNoDataSet = TRUE;
    bNoDataChanged = TRUE;
    dfNoDataValue = dfNewValue;
}

/************************************************************************/
/*                          SaveHKVAttribFile()                            */
/************************************************************************/

CPLErr SaveHKVAttribFile( const char *pszFilenameIn,
                                    int nXSize, int nYSize, int nBands,
				    GDALDataType eType, int bNoDataSet,
                                    double dfNoDataValue )

{
    const char *pszFilename = CPLFormFilename( pszFilenameIn, "attrib", NULL );

    FILE *fp = VSIFOpen( pszFilename, "wt" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Couldn't create %s.\n", pszFilename );
        return CE_Failure;
    }

    fprintf( fp, "channel.enumeration = %d\n", nBands );
    fprintf( fp, "channel.interleave = { *pixel tile sequential }\n" );
    fprintf( fp, "extent.cols = %d\n", nXSize );
    fprintf( fp, "extent.rows = %d\n", nYSize );

    switch( eType )
    {
      case GDT_Byte:
        fprintf( fp, "pixel.encoding = "
                 "{ *unsigned twos-complement ieee-754 }\n" );
        break;

      case GDT_UInt16:
        fprintf( fp, "pixel.encoding = "
                 "{ *unsigned twos-complement ieee-754 }\n" );
        break;

      case GDT_CInt16:
      case GDT_Int16:
        fprintf( fp, "pixel.encoding = "
                 "{ unsigned *twos-complement ieee-754 }\n" );
        break;

      case GDT_CFloat32:
      case GDT_Float32:
        fprintf( fp, "pixel.encoding = "
                 "{ unsigned twos-complement *ieee-754 }\n" );
        break;

      default:
        CPLAssert( FALSE );
    }

    fprintf( fp, "pixel.size = %d\n", GDALGetDataTypeSize(eType) );
    if( GDALDataTypeIsComplex( eType ) )
        fprintf( fp, "pixel.field = { real *complex }\n" );
    else
        fprintf( fp, "pixel.field = { *real complex }\n" );

#ifdef CPL_MSB
    fprintf( fp, "pixel.order = { lsbf *msbf }\n" );
#else
    fprintf( fp, "pixel.order = { *lsbf msbf }\n" );
#endif

    if ( bNoDataSet )
        fprintf( fp, "pixel.no_data = %s\n", CPLSPrintf("%f", dfNoDataValue) );

    /* version information- only create the new style */
    fprintf( fp, "version = 1.1");


    VSIFClose( fp );
    return CE_None;
}


/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *HKVDataset::GetProjectionRef()

{
    return( pszProjection );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr HKVDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform,  adfGeoTransform, sizeof(double) * 6 ); 
    return( CE_None );
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr HKVDataset::SetGeoTransform( double * padfTransform )

{
    /* NOTE:  Geotransform coordinates must match the current projection   */
    /* of the dataset being changed (not the geotransform source).         */
    /* i.e. be in lat/longs for LL projected; UTM for UTM projected.       */
    /* SET PROJECTION BEFORE SETTING GEOTRANSFORM TO AVOID SYNCHRONIZATION */
    /* PROBLEMS!                                                           */

    /* Update the geotransform itself */
    memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );

    /* Clear previous gcps */
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }
    nGCPCount = 0;
    pasGCPList = NULL;

    /* Return if the identity transform is set */
    if (adfGeoTransform[0] == 0.0 && adfGeoTransform[1] == 1.0
        && adfGeoTransform[2] == 0.0 && adfGeoTransform[3] == 0.0
        && adfGeoTransform[4] == 0.0 && adfGeoTransform[5] == 1.0 )
        return CE_None;

    /* Update georef text info for saving later, and */
    /* update GCPs to match geotransform.            */

    OGRCoordinateTransformation *poTransform = NULL;
    int bSuccess = true;

    /* Projection parameter checking will have been done */
    /* in SetProjection.                                 */
    if(( CSLFetchNameValue( papszGeoref, "projection.name" ) != NULL ) &&
       ( EQUAL(CSLFetchNameValue( papszGeoref, "projection.name" ),"UTM" )))
    {
        /* pass copies of projection info, not originals (pointers */
        /* get updated by importFromWkt)                           */
        char *pszPtemp = CPLStrdup(pszProjection);
        OGRSpatialReference oUTM;
        oUTM.importFromWkt(&pszPtemp);
    char *pszGCPtemp;
        (oUTM.GetAttrNode("GEOGCS"))->exportToWkt(&pszGCPtemp);

        OGRSpatialReference oLL;
        oLL.importFromWkt(&pszGCPtemp);
        poTransform = OGRCreateCoordinateTransformation( &oUTM, &oLL );
        if( poTransform == NULL )
        {
            bSuccess = false;
            CPLErrorReset();
        }
    }
    else if ((( CSLFetchNameValue( papszGeoref, "projection.name" ) != NULL ) &&
              ( !EQUAL(CSLFetchNameValue( papszGeoref, "projection.name" ),"LL" ))) ||
             (CSLFetchNameValue( papszGeoref, "projection.name" ) == NULL ))
    {
      return CE_Failure;
    }

    nGCPCount = 0;
    pasGCPList = reinterpret_cast<GDAL_GCP *>( CPLCalloc( sizeof(GDAL_GCP), 5 ) );

    /* -------------------------------------------------------------------- */
    /*      top left                                                        */
    /* -------------------------------------------------------------------- */
    GDALInitGCPs( 1, pasGCPList + nGCPCount );
    CPLFree( pasGCPList[nGCPCount].pszId );
    pasGCPList[nGCPCount].pszId = CPLStrdup( "top_left" );

    double temp_lat, temp_long;
    if (MFF2version > 1.0)
    {
        temp_lat = padfTransform[3];
        temp_long = padfTransform[0];
        pasGCPList[nGCPCount].dfGCPPixel = 0.0;
        pasGCPList[nGCPCount].dfGCPLine = 0.0;
    }
    else
    {
        temp_lat = padfTransform[3] + 0.5 * padfTransform[4] + 0.5 * padfTransform[5];
        temp_long = padfTransform[0] + 0.5 * padfTransform[1]+ 0.5 * padfTransform[2];
        pasGCPList[nGCPCount].dfGCPPixel = 0.5;
        pasGCPList[nGCPCount].dfGCPLine = 0.5;
    }
    pasGCPList[nGCPCount].dfGCPX = temp_long;
    pasGCPList[nGCPCount].dfGCPY = temp_lat;
    pasGCPList[nGCPCount].dfGCPZ = 0.0;
    nGCPCount++;

    if (poTransform != NULL)
    {
        if( !bSuccess || !poTransform->Transform( 1, &temp_long, &temp_lat ) )
            bSuccess = false;
    }

    if (bSuccess)
    {
        char szValue[128];
        CPLsnprintf( szValue, sizeof(szValue), "%.10f", temp_lat );
        papszGeoref = CSLSetNameValue( papszGeoref, "top_left.latitude", 
                                       szValue );

        CPLsnprintf( szValue, sizeof(szValue), "%.10f", temp_long );
        papszGeoref = CSLSetNameValue( papszGeoref, "top_left.longitude", 
                                       szValue );
    }

    /* -------------------------------------------------------------------- */
    /*      top_right                                                       */
    /* -------------------------------------------------------------------- */
    GDALInitGCPs( 1, pasGCPList + nGCPCount );
    CPLFree( pasGCPList[nGCPCount].pszId );
    pasGCPList[nGCPCount].pszId = CPLStrdup( "top_right" );

    if (MFF2version > 1.0)
    {
        temp_lat = padfTransform[3] + GetRasterXSize() * padfTransform[4];
        temp_long = padfTransform[0] + GetRasterXSize() * padfTransform[1];
        pasGCPList[nGCPCount].dfGCPPixel = GetRasterXSize();
        pasGCPList[nGCPCount].dfGCPLine = 0.0;
    }
    else
    {
        temp_lat = padfTransform[3] + (GetRasterXSize()-0.5) * padfTransform[4] + 0.5 * padfTransform[5];
        temp_long = padfTransform[0] + (GetRasterXSize()-0.5) * padfTransform[1] + 0.5 * padfTransform[2];
        pasGCPList[nGCPCount].dfGCPPixel = GetRasterXSize()-0.5;
        pasGCPList[nGCPCount].dfGCPLine = 0.5;
    }
    pasGCPList[nGCPCount].dfGCPX = temp_long;
    pasGCPList[nGCPCount].dfGCPY = temp_lat;
    pasGCPList[nGCPCount].dfGCPZ = 0.0;
    nGCPCount++;

    if (poTransform != NULL)
    {
        if( !bSuccess || !poTransform->Transform( 1, &temp_long, &temp_lat ) )
            bSuccess = false;
    }

    if (bSuccess)
    {
        char szValue[128];
        CPLsnprintf( szValue, sizeof(szValue), "%.10f", temp_lat );
        papszGeoref = CSLSetNameValue( papszGeoref, "top_right.latitude", 
                                       szValue );

        CPLsnprintf( szValue, sizeof(szValue), "%.10f", temp_long );
        papszGeoref = CSLSetNameValue( papszGeoref, "top_right.longitude", 
                                       szValue );
    }

    /* -------------------------------------------------------------------- */
    /*      bottom_left                                                     */
    /* -------------------------------------------------------------------- */
    GDALInitGCPs( 1, pasGCPList + nGCPCount );
    CPLFree( pasGCPList[nGCPCount].pszId );
    pasGCPList[nGCPCount].pszId = CPLStrdup( "bottom_left" );

    if (MFF2version > 1.0)
    {
        temp_lat = padfTransform[3] + GetRasterYSize() * padfTransform[5];
        temp_long = padfTransform[0] + GetRasterYSize() * padfTransform[2];
        pasGCPList[nGCPCount].dfGCPPixel = 0.0;
        pasGCPList[nGCPCount].dfGCPLine = GetRasterYSize();
    }
    else
    {
        temp_lat = padfTransform[3] + 0.5 * padfTransform[4] + (GetRasterYSize()-0.5) * padfTransform[5];
        temp_long = padfTransform[0] + 0.5 * padfTransform[1] + (GetRasterYSize()-0.5) * padfTransform[2];
        pasGCPList[nGCPCount].dfGCPPixel = 0.5;
        pasGCPList[nGCPCount].dfGCPLine = GetRasterYSize()-0.5;
    }
    pasGCPList[nGCPCount].dfGCPX = temp_long;
    pasGCPList[nGCPCount].dfGCPY = temp_lat;
    pasGCPList[nGCPCount].dfGCPZ = 0.0;
    nGCPCount++;

    if (poTransform != NULL)
    {
        if( !bSuccess || !poTransform->Transform( 1, &temp_long, &temp_lat ) )
            bSuccess = FALSE;
    }

    if (bSuccess)
    {
        char szValue[128];
        CPLsnprintf( szValue, sizeof(szValue), "%.10f", temp_lat );
        papszGeoref = CSLSetNameValue( papszGeoref, "bottom_left.latitude",
                                       szValue );

        CPLsnprintf( szValue, sizeof(szValue), "%.10f", temp_long );
        papszGeoref = CSLSetNameValue( papszGeoref, "bottom_left.longitude",
                                       szValue );
    }

    /* -------------------------------------------------------------------- */
    /*      bottom_right                                                    */
    /* -------------------------------------------------------------------- */
    GDALInitGCPs( 1, pasGCPList + nGCPCount );
    CPLFree( pasGCPList[nGCPCount].pszId );
    pasGCPList[nGCPCount].pszId = CPLStrdup( "bottom_right" );

    if (MFF2version > 1.0)
    {
        temp_lat = padfTransform[3] + GetRasterXSize() * padfTransform[4] + 
          GetRasterYSize() * padfTransform[5];
        temp_long = padfTransform[0] + GetRasterXSize() * padfTransform[1] + 
          GetRasterYSize() * padfTransform[2];
        pasGCPList[nGCPCount].dfGCPPixel = GetRasterXSize();
        pasGCPList[nGCPCount].dfGCPLine = GetRasterYSize();
    }
    else
    {
        temp_lat = padfTransform[3] + (GetRasterXSize()-0.5) * padfTransform[4] + 
          (GetRasterYSize()-0.5) * padfTransform[5];
        temp_long = padfTransform[0] + (GetRasterXSize()-0.5) * padfTransform[1] + 
          (GetRasterYSize()-0.5) * padfTransform[2];
        pasGCPList[nGCPCount].dfGCPPixel = GetRasterXSize()-0.5;
        pasGCPList[nGCPCount].dfGCPLine = GetRasterYSize()-0.5;
    }
    pasGCPList[nGCPCount].dfGCPX = temp_long;
    pasGCPList[nGCPCount].dfGCPY = temp_lat;
    pasGCPList[nGCPCount].dfGCPZ = 0.0;
    nGCPCount++;

    if (poTransform != NULL)
    {
        if( !bSuccess || !poTransform->Transform( 1, &temp_long, &temp_lat ) )
            bSuccess = FALSE;
    }

    if (bSuccess)
    {
        char szValue[128];
        CPLsnprintf( szValue, sizeof(szValue), "%.10f", temp_lat );
        papszGeoref = CSLSetNameValue( papszGeoref, "bottom_right.latitude",
                                       szValue );

        CPLsnprintf( szValue, sizeof(szValue), "%.10f", temp_long );
        papszGeoref = CSLSetNameValue( papszGeoref, "bottom_right.longitude",
                                       szValue );
    }

    /* -------------------------------------------------------------------- */
    /*      Center                                                          */
    /* -------------------------------------------------------------------- */
    GDALInitGCPs( 1, pasGCPList + nGCPCount );
    CPLFree( pasGCPList[nGCPCount].pszId );
    pasGCPList[nGCPCount].pszId = CPLStrdup( "centre" );

    temp_lat = padfTransform[3] + GetRasterXSize() * padfTransform[4] * 0.5 +
      GetRasterYSize() * padfTransform[5] * 0.5;
    temp_long = padfTransform[0] + GetRasterXSize() * padfTransform[1] * 0.5 +
             GetRasterYSize() * padfTransform[2] * 0.5;
    pasGCPList[nGCPCount].dfGCPPixel = GetRasterXSize()/2.0;
    pasGCPList[nGCPCount].dfGCPLine = GetRasterYSize()/2.0;

    pasGCPList[nGCPCount].dfGCPX = temp_long;
    pasGCPList[nGCPCount].dfGCPY = temp_lat;
    pasGCPList[nGCPCount].dfGCPZ = 0.0;
    nGCPCount++;

    if (poTransform != NULL)
    {
        if( !bSuccess || !poTransform->Transform( 1, &temp_long, &temp_lat ) )
            bSuccess = FALSE;
    }

    if (bSuccess)
    {
        char szValue[128];
        CPLsnprintf( szValue, sizeof(szValue), "%.10f", temp_lat );
        papszGeoref = CSLSetNameValue( papszGeoref, "centre.latitude", 
                                       szValue );

        CPLsnprintf( szValue, sizeof(szValue), "%.10f", temp_long );
        papszGeoref = CSLSetNameValue( papszGeoref, "centre.longitude", 
                                       szValue );
    }

    if (!bSuccess)
    {
      CPLError(CE_Warning,CPLE_AppDefined,
               "Warning- error setting header info in SetGeoTransform. Changes may not be saved properly.\n"); 
    }

    if (poTransform != NULL)
        delete poTransform;

    bGeorefChanged = TRUE;

    return( CE_None );
}

CPLErr HKVDataset::SetGCPProjection( const char *pszNewProjection )
{
    CPLFree( pszGCPProjection );
    this->pszGCPProjection = CPLStrdup(pszNewProjection);

    return CE_None;
}

/************************************************************************/
/*                           SetProjection()                            */
/*                                                                      */
/*      We provide very limited support for setting the projection.     */
/************************************************************************/

CPLErr HKVDataset::SetProjection( const char * pszNewProjection )

{
    /* This function is used to update a georef file */

#ifdef DEBUG_VERBOSE
    printf( "HKVDataset::SetProjection(%s)\n", pszNewProjection );
#endif

    if( !STARTS_WITH_CI(pszNewProjection, "GEOGCS")
        && !STARTS_WITH_CI(pszNewProjection, "PROJCS")
        && !EQUAL(pszNewProjection,"") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                "Only OGC WKT Projections supported for writing to HKV.\n"
                "%s not supported.",
                  pszNewProjection );

        return CE_Failure;
    }
    else if (EQUAL(pszNewProjection,""))
    {
      CPLFree( pszProjection );
      pszProjection = reinterpret_cast<char *>( CPLStrdup( pszNewProjection ) );

      return CE_None;
    }
    CPLFree( pszProjection );
    pszProjection = reinterpret_cast<char *>( CPLStrdup( pszNewProjection ) );

    OGRSpatialReference oSRS(pszNewProjection);

    if ((oSRS.GetAttrValue("PROJECTION") != NULL) && 
        (EQUAL(oSRS.GetAttrValue("PROJECTION"),SRS_PT_TRANSVERSE_MERCATOR)))
    {
        papszGeoref = CSLSetNameValue( papszGeoref, "projection.name", "utm" );
        OGRErr ogrerrorOl = OGRERR_NONE;
        papszGeoref = CSLSetNameValue( papszGeoref, "projection.origin_longitude",
                        CPLSPrintf("%f",oSRS.GetProjParm(SRS_PP_CENTRAL_MERIDIAN,0.0,&ogrerrorOl)));
    }
    else if ((oSRS.GetAttrValue("PROJECTION") == NULL) && (oSRS.IsGeographic()))
    {
        papszGeoref = CSLSetNameValue( papszGeoref, "projection.name", "LL" );
    }
    else
    {
      CPLError( CE_Warning, CPLE_AppDefined,
                "Unrecognized projection.");
      return CE_Failure;
    }

    OGRErr ogrerrorEq = OGRERR_NONE;
    const double eq_radius = oSRS.GetSemiMajor(&ogrerrorEq);

    OGRErr ogrerrorInvf = OGRERR_NONE;
    const double inv_flattening = oSRS.GetInvFlattening(&ogrerrorInvf);

    if ((ogrerrorEq == OGRERR_NONE) && (ogrerrorInvf == OGRERR_NONE)) 
    {
        HKVSpheroidList *hkvEllipsoids = new HKVSpheroidList;
        char *spheroid_name =
            hkvEllipsoids->GetSpheroidNameByEqRadiusAndInvFlattening(
                eq_radius, inv_flattening);
        if (spheroid_name != NULL)
        {
            papszGeoref = CSLSetNameValue( papszGeoref, "spheroid.name", 
                                           spheroid_name );
        }
        CPLFree(spheroid_name);
        delete hkvEllipsoids;
    }
    else
    {
      /* default to previous behaviour if spheroid not found by */
      /* radius and inverse flattening */

        if( strstr(pszNewProjection,"Bessel") != NULL )
        {
            papszGeoref = CSLSetNameValue( papszGeoref, "spheroid.name", 
                                       "ev-bessel" );
        }
        else
        {
            papszGeoref = CSLSetNameValue( papszGeoref, "spheroid.name", 
                                       "ev-wgs-84" );
        }
    }
    bGeorefChanged = TRUE;
    return CE_None;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int HKVDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *HKVDataset::GetGCPProjection()

{
  return pszGCPProjection;
}

/************************************************************************/
/*                               GetGCP()                               */
/************************************************************************/

const GDAL_GCP *HKVDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                          ProcessGeorefGCP()                          */
/************************************************************************/

void HKVDataset::ProcessGeorefGCP( char **papszGeorefIn, const char *pszBase,
                                   double dfRasterX, double dfRasterY )

{
/* -------------------------------------------------------------------- */
/*      Fetch the GCP from the string list.                             */
/* -------------------------------------------------------------------- */
    char szFieldName[128];
    snprintf( szFieldName, sizeof(szFieldName), "%s.latitude", pszBase );
    double dfLat;
    if( CSLFetchNameValue(papszGeorefIn, szFieldName) == NULL )
        return;
    else
        dfLat = CPLAtof(CSLFetchNameValue(papszGeorefIn, szFieldName));

    snprintf( szFieldName, sizeof(szFieldName), "%s.longitude", pszBase );
    double dfLong;
    if( CSLFetchNameValue(papszGeorefIn, szFieldName) == NULL )
        return;
    else
        dfLong = CPLAtof(CSLFetchNameValue(papszGeorefIn, szFieldName));

/* -------------------------------------------------------------------- */
/*      Add the gcp to the internal list.                               */
/* -------------------------------------------------------------------- */
    GDALInitGCPs( 1, pasGCPList + nGCPCount );

    CPLFree( pasGCPList[nGCPCount].pszId );

    pasGCPList[nGCPCount].pszId = CPLStrdup( pszBase );

    pasGCPList[nGCPCount].dfGCPX = dfLong;
    pasGCPList[nGCPCount].dfGCPY = dfLat;
    pasGCPList[nGCPCount].dfGCPZ = 0.0;

    pasGCPList[nGCPCount].dfGCPPixel = dfRasterX;
    pasGCPList[nGCPCount].dfGCPLine = dfRasterY;

    nGCPCount++;
}

/************************************************************************/
/*                           ProcessGeoref()                            */
/************************************************************************/

void HKVDataset::ProcessGeoref( const char * pszFilename )

{
/* -------------------------------------------------------------------- */
/*      Load the georef file, and boil white space away from around     */
/*      the equal sign.                                                 */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszGeoref );
    papszGeoref = CSLLoad( pszFilename );
    if( papszGeoref == NULL )
        return;

    HKVSpheroidList *hkvEllipsoids = new HKVSpheroidList;

    for( int i = 0; papszGeoref[i] != NULL; i++ )
    {
        int iDst = 0;
        char     *pszLine = papszGeoref[i];

        for( int iSrc = 0; pszLine[iSrc] != '\0'; iSrc++ )
        {
            if( pszLine[iSrc] != ' ' )
            {
                pszLine[iDst++] = pszLine[iSrc];
            }
        }
        pszLine[iDst] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Try to get GCPs, in lat/longs                     .             */
/* -------------------------------------------------------------------- */
    nGCPCount = 0;
    pasGCPList = reinterpret_cast<GDAL_GCP *>( CPLCalloc( sizeof(GDAL_GCP), 5) );

    if (MFF2version > 1.0)
    {
        ProcessGeorefGCP( papszGeoref, "top_left", 
                          0, 0 );
        ProcessGeorefGCP( papszGeoref, "top_right", 
                          GetRasterXSize(), 0 );
        ProcessGeorefGCP( papszGeoref, "bottom_left", 
                          0, GetRasterYSize() );
        ProcessGeorefGCP( papszGeoref, "bottom_right", 
                          GetRasterXSize(), GetRasterYSize() );
        ProcessGeorefGCP( papszGeoref, "centre", 
                          GetRasterXSize()/2.0, GetRasterYSize()/2.0 );
    }
    else
    {
        ProcessGeorefGCP( papszGeoref, "top_left", 
                          0.5, 0.5 );
        ProcessGeorefGCP( papszGeoref, "top_right", 
                          GetRasterXSize()-0.5, 0.5 );
        ProcessGeorefGCP( papszGeoref, "bottom_left", 
                          0.5, GetRasterYSize()-0.5 );
        ProcessGeorefGCP( papszGeoref, "bottom_right", 
                          GetRasterXSize()-0.5, GetRasterYSize()-0.5 );
        ProcessGeorefGCP( papszGeoref, "centre", 
                          GetRasterXSize()/2.0, GetRasterYSize()/2.0 );
    }

    if (nGCPCount == 0)
    {
        CPLFree(pasGCPList);
        pasGCPList = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we have a recognised projection?                             */
/* -------------------------------------------------------------------- */
    /* double eq_radius, inv_flattening; */

    const char *pszProjName = CSLFetchNameValue(papszGeoref, "projection.name");
    const char *pszOriginLong = CSLFetchNameValue(
        papszGeoref, "projection.origin_longitude");
    const char *pszSpheroidName = CSLFetchNameValue(papszGeoref, "spheroid.name");


    if ((pszSpheroidName != NULL) && (hkvEllipsoids->SpheroidInList(pszSpheroidName)))
    {
      /* eq_radius=hkvEllipsoids->GetSpheroidEqRadius(pszSpheroidName); */
      /* inv_flattening=hkvEllipsoids->GetSpheroidInverseFlattening(pszSpheroidName); */
    }
    else if (pszProjName != NULL)
    {
      CPLError(CE_Warning,CPLE_AppDefined,"Warning- unrecognized ellipsoid.  Using wgs-84 parameters.\n");
      /* eq_radius=hkvEllipsoids->GetSpheroidEqRadius("wgs-84"); */
      /* inv_flattening=hkvEllipsoids->GetSpheroidInverseFlattening("wgs-84"); */
    }

    if( (pszProjName != NULL) && EQUAL(pszProjName,"utm") && (nGCPCount == 5) )
    {
      /*int nZone = (int)((CPLAtof(pszOriginLong)+184.5) / 6.0); */
        int nZone;

        if (pszOriginLong == NULL)
        {
            /* If origin not specified, assume 0.0 */
            CPLError(CE_Warning,CPLE_AppDefined,
                   "Warning- no projection origin longitude specified.  Assuming 0.0.");
            nZone = 31;
        }
        else
            nZone = 31 + static_cast<int>( floor( CPLAtof( pszOriginLong ) / 6.0) );

        OGRSpatialReference oUTM;

        if( pasGCPList[4].dfGCPY < 0 )
            oUTM.SetUTM( nZone, 0 );
        else
            oUTM.SetUTM( nZone, 1 );

        OGRSpatialReference oLL;
        if (pszOriginLong != NULL)
        {
            oUTM.SetProjParm(SRS_PP_CENTRAL_MERIDIAN,CPLAtof(pszOriginLong));
            oLL.SetProjParm(SRS_PP_LONGITUDE_OF_ORIGIN,CPLAtof(pszOriginLong));
        }

        if ((pszSpheroidName == NULL) || (EQUAL(pszSpheroidName,"wgs-84")) ||
            (EQUAL(pszSpheroidName,"wgs_84")))
          {
            oUTM.SetWellKnownGeogCS( "WGS84" );
            oLL.SetWellKnownGeogCS( "WGS84" );
          }
        else
        {
          if (hkvEllipsoids->SpheroidInList(pszSpheroidName))
          {
            oUTM.SetGeogCS( "unknown","unknown",pszSpheroidName,
                            hkvEllipsoids->GetSpheroidEqRadius(pszSpheroidName), 
                            hkvEllipsoids->GetSpheroidInverseFlattening(pszSpheroidName)
                          );
            oLL.SetGeogCS( "unknown","unknown",pszSpheroidName,
                            hkvEllipsoids->GetSpheroidEqRadius(pszSpheroidName), 
                            hkvEllipsoids->GetSpheroidInverseFlattening(pszSpheroidName)
                           );
          }
          else
          {
            CPLError(CE_Warning,CPLE_AppDefined,"Warning- unrecognized ellipsoid.  Using wgs-84 parameters.\n");
            oUTM.SetWellKnownGeogCS( "WGS84" );
            oLL.SetWellKnownGeogCS( "WGS84" );
          }
        }

        OGRCoordinateTransformation *poTransform
            = OGRCreateCoordinateTransformation( &oLL, &oUTM );

        bool bSuccess = true;
        if( poTransform == NULL )
        {
            CPLErrorReset();
            bSuccess = false;
        }

        double dfUtmX[5], dfUtmY[5];

        for( int gcp_index=0;gcp_index<5;gcp_index++ )
        {
            dfUtmX[gcp_index] = pasGCPList[gcp_index].dfGCPX;
            dfUtmY[gcp_index] = pasGCPList[gcp_index].dfGCPY;

            if( bSuccess && !poTransform->Transform( 1, &(dfUtmX[gcp_index]), &(dfUtmY[gcp_index]) ) )
                bSuccess = false;
        }

        if( bSuccess )
        {
            /* update GCPS to proper projection */
            for( int gcp_index = 0; gcp_index < 5; gcp_index++ )
            {
                pasGCPList[gcp_index].dfGCPX = dfUtmX[gcp_index];
                pasGCPList[gcp_index].dfGCPY = dfUtmY[gcp_index];
            }

            CPLFree( pszGCPProjection );
            pszGCPProjection = NULL;
            oUTM.exportToWkt( &pszGCPProjection );

            int transform_ok = GDALGCPsToGeoTransform(5,pasGCPList,adfGeoTransform,0);

            CPLFree( pszProjection );
            pszProjection = NULL;
            if (transform_ok == FALSE)
            {
              /* transform may not be sufficient in all cases (slant range projection) */
                adfGeoTransform[0] = 0.0;
                adfGeoTransform[1] = 1.0;
                adfGeoTransform[2] = 0.0;
                adfGeoTransform[3] = 0.0;
                adfGeoTransform[4] = 0.0;
                adfGeoTransform[5] = 1.0;
                pszProjection = CPLStrdup("");
            }
            else
                oUTM.exportToWkt( &pszProjection );

        }

        if( poTransform != NULL )
            delete poTransform;
    }
    else if ((pszProjName != NULL) && (nGCPCount == 5))
    {
        OGRSpatialReference oLL;

        if (pszOriginLong != NULL)
        {
            oLL.SetProjParm(SRS_PP_LONGITUDE_OF_ORIGIN,CPLAtof(pszOriginLong));
        }

        if ((pszSpheroidName == NULL) || (EQUAL(pszSpheroidName,"wgs-84")) ||
            (EQUAL(pszSpheroidName,"wgs_84")))
          {
            oLL.SetWellKnownGeogCS( "WGS84" );
          }
        else
        {
          if (hkvEllipsoids->SpheroidInList(pszSpheroidName))
          {
            oLL.SetGeogCS( "","",pszSpheroidName,
                            hkvEllipsoids->GetSpheroidEqRadius(pszSpheroidName), 
                            hkvEllipsoids->GetSpheroidInverseFlattening(pszSpheroidName)
                           );
          }
          else
          {
            CPLError(CE_Warning,CPLE_AppDefined,"Warning- unrecognized ellipsoid.  Using wgs-84 parameters.\n");
            oLL.SetWellKnownGeogCS( "WGS84" );
          }
        }

        const bool transform_ok
            = CPL_TO_BOOL(GDALGCPsToGeoTransform(5,pasGCPList,adfGeoTransform,0));

        CPLFree( pszProjection );
        pszProjection = NULL;

        if( !transform_ok )
        {
            adfGeoTransform[0] = 0.0;
            adfGeoTransform[1] = 1.0;
            adfGeoTransform[2] = 0.0;
            adfGeoTransform[3] = 0.0;
            adfGeoTransform[4] = 0.0;
            adfGeoTransform[5] = 1.0;
        }
        else
        {
            oLL.exportToWkt( &pszProjection );
        }

        CPLFree( pszGCPProjection );
        pszGCPProjection = NULL;
        oLL.exportToWkt( &pszGCPProjection );
    }

    delete hkvEllipsoids;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *HKVDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      We assume the dataset is passed as a directory.  Check for      */
/*      an attrib and blob file as a minimum.                           */
/* -------------------------------------------------------------------- */
    if( !poOpenInfo->bIsDirectory )
        return NULL;

    const char *pszFilename = CPLFormFilename(poOpenInfo->pszFilename, "image_data", NULL);
    VSIStatBuf sStat;
    if( VSIStat(pszFilename,&sStat) != 0 )
        pszFilename = CPLFormFilename(poOpenInfo->pszFilename, "blob", NULL );
    if( VSIStat(pszFilename,&sStat) != 0 )
        return NULL;

    pszFilename = CPLFormFilename(poOpenInfo->pszFilename, "attrib", NULL );
    if( VSIStat(pszFilename,&sStat) != 0 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Load the attrib file, and boil white space away from around     */
/*      the equal sign.                                                 */
/* -------------------------------------------------------------------- */
    char **papszAttrib = CSLLoad( pszFilename );
    if( papszAttrib == NULL )
        return NULL;

    for( int i = 0; papszAttrib[i] != NULL; i++ )
    {
        int iDst = 0;
        char *pszLine = papszAttrib[i];

        for( int iSrc = 0; pszLine[iSrc] != '\0'; iSrc++ )
        {
            if( pszLine[iSrc] != ' ' )
            {
                pszLine[iDst++] = pszLine[iSrc];
            }
        }
        pszLine[iDst] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    HKVDataset *poDS = new HKVDataset();

    poDS->pszPath = CPLStrdup( poOpenInfo->pszFilename );
    poDS->papszAttrib = papszAttrib;

    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Set some dataset wide information.                              */
/* -------------------------------------------------------------------- */
    int bNative, bComplex;
    int nRawBands = 0;

    if( CSLFetchNameValue( papszAttrib, "extent.cols" ) == NULL
        || CSLFetchNameValue( papszAttrib, "extent.rows" ) == NULL )
    {
        delete poDS;
        return NULL;
    }

    poDS->nRasterXSize = atoi(CSLFetchNameValue(papszAttrib,"extent.cols"));
    poDS->nRasterYSize = atoi(CSLFetchNameValue(papszAttrib,"extent.rows"));

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return NULL;
    }

    const char *pszValue = CSLFetchNameValue(papszAttrib,"pixel.order");
    if( pszValue == NULL )
        bNative = TRUE;
    else
    {
#ifdef CPL_MSB
        bNative = (strstr(pszValue,"*msbf") != NULL);
#else
        bNative = (strstr(pszValue,"*lsbf") != NULL);
#endif
    }

    bool bNoDataSet = false;
    double dfNoDataValue = 0.0;
    pszValue = CSLFetchNameValue(papszAttrib,"pixel.no_data");
    if( pszValue != NULL )
    {
        bNoDataSet = true;
        dfNoDataValue = CPLAtof(pszValue);
    }

    pszValue = CSLFetchNameValue(papszAttrib,"channel.enumeration");
    if( pszValue != NULL )
        nRawBands = atoi(pszValue);
    else
        nRawBands = 1;

    if (!GDALCheckBandCount(nRawBands, TRUE))
    {
        delete poDS;
        return NULL;
    }

    pszValue = CSLFetchNameValue(papszAttrib,"pixel.field");
    if( pszValue != NULL && strstr(pszValue,"*complex") != NULL )
        bComplex = TRUE;
    else
        bComplex = FALSE;

    /* Get the version number, if present (if not, assume old version. */
    /* Versions differ in their interpretation of corner coordinates.  */

    if  (CSLFetchNameValue( papszAttrib, "version" ) != NULL)
      poDS->SetVersion( static_cast<float>(
          CPLAtof( CSLFetchNameValue( papszAttrib, "version") ) ) );
    else
      poDS->SetVersion(1.0);

/* -------------------------------------------------------------------- */
/*      Figure out the datatype                                         */
/* -------------------------------------------------------------------- */
    const char *pszEncoding = CSLFetchNameValue(papszAttrib,"pixel.encoding");
    if( pszEncoding == NULL )
        pszEncoding = "{ *unsigned }";

    int          nSize = 1;
    if( CSLFetchNameValue(papszAttrib,"pixel.size") != NULL )
        nSize = atoi(CSLFetchNameValue(papszAttrib,"pixel.size"))/8;
#if 0
    int nPseudoBands;
    if( bComplex )
        nPseudoBands = 2;
    else
        nPseudoBands = 1;
#endif

    GDALDataType eType;
    if( nSize == 1 )
        eType = GDT_Byte;
    else if( nSize == 2 && strstr(pszEncoding,"*unsigned") != NULL )
        eType = GDT_UInt16;
    else if( nSize == 4 && bComplex )
        eType = GDT_CInt16;
    else if( nSize == 2 )
        eType = GDT_Int16;
    else if( nSize == 4 && strstr(pszEncoding,"*unsigned") != NULL )
        eType = GDT_UInt32;
    else if( nSize == 8 && strstr(pszEncoding,"*two") != NULL && bComplex )
        eType = GDT_CInt32;
    else if( nSize == 4 && strstr(pszEncoding,"*two") != NULL )
        eType = GDT_Int32;
    else if( nSize == 8 && bComplex )
        eType = GDT_CFloat32;
    else if( nSize == 4 )
        eType = GDT_Float32;
    else if( nSize == 16 && bComplex )
        eType = GDT_CFloat64;
    else if( nSize == 8 )
        eType = GDT_Float64;
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unsupported pixel data type in %s.\n"
                  "pixel.size=%d pixel.encoding=%s\n", 
                  poDS->pszPath, nSize, pszEncoding );
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the blob file.                                             */
/* -------------------------------------------------------------------- */
    pszFilename = CPLFormFilename(poDS->pszPath, "image_data", NULL );
    if( VSIStat(pszFilename,&sStat) != 0 )
        pszFilename = CPLFormFilename(poDS->pszPath, "blob", NULL );
    if( poOpenInfo->eAccess == GA_ReadOnly )
    {
        poDS->fpBlob = VSIFOpenL( pszFilename, "rb" );
        if( poDS->fpBlob == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Unable to open file %s for read access.\n",
                      pszFilename );
            delete poDS;
            return NULL;
        }
    }
    else
    {
        poDS->fpBlob = VSIFOpenL( pszFilename, "rb+" );
        if( poDS->fpBlob == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed, 
                      "Unable to open file %s for update access.\n",
                      pszFilename );
            delete poDS;
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Build the overview filename, as blob file = "_ovr".             */
/* -------------------------------------------------------------------- */
    const size_t nOvrFilenameLen = strlen( pszFilename ) + 5;
    char *pszOvrFilename = reinterpret_cast<char *>(
        CPLMalloc( nOvrFilenameLen ) );

    snprintf( pszOvrFilename, nOvrFilenameLen, "%s_ovr", pszFilename );

/* -------------------------------------------------------------------- */
/*      Define the bands.                                               */
/* -------------------------------------------------------------------- */
    const int nPixelOffset = nRawBands * nSize;
    const int nLineOffset = nPixelOffset * poDS->GetRasterXSize();
    int nOffset = 0;

    for( int iRawBand=0; iRawBand < nRawBands; iRawBand++ )
    {
        HKVRasterBand *poBand
            = new HKVRasterBand( poDS, poDS->GetRasterCount()+1, poDS->fpBlob,
                               nOffset, nPixelOffset, nLineOffset, 
                               eType, bNative );
        poDS->SetBand( poDS->GetRasterCount()+1, poBand );
        nOffset += GDALGetDataTypeSize( eType ) / 8;

        if( bNoDataSet )
            poBand->SetNoDataValue( dfNoDataValue );
    }

    poDS->eRasterType = eType;

/* -------------------------------------------------------------------- */
/*      Process the georef file if there is one.                        */
/* -------------------------------------------------------------------- */
    pszFilename = CPLFormFilename(poDS->pszPath, "georef", NULL );
    if( VSIStat(pszFilename,&sStat) == 0 )
        poDS->ProcessGeoref(pszFilename);

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( pszOvrFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Handle overviews.                                               */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, pszOvrFilename, NULL, TRUE );

    CPLFree( pszOvrFilename );

    return( poDS );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *HKVDataset::Create( const char * pszFilenameIn,
                                 int nXSize, int nYSize, int nBands,
                                 GDALDataType eType,
                                 char ** /* papszParmList */ )

{
/* -------------------------------------------------------------------- */
/*      Verify input options.                                           */
/* -------------------------------------------------------------------- */
    if (nBands <= 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "HKV driver does not support %d bands.\n", nBands);
        return NULL;
    }

    if( eType != GDT_Byte
        && eType != GDT_UInt16 && eType != GDT_Int16 
        && eType != GDT_CInt16 && eType != GDT_Float32
        && eType != GDT_CFloat32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
              "Attempt to create HKV file with currently unsupported\n"
              "data type (%s).\n",
              GDALGetDataTypeName(eType) );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Establish the name of the directory we will be creating the     */
/*      new HKV directory in.  Verify that this is a directory.         */
/* -------------------------------------------------------------------- */
    char	*pszBaseDir;

    if( strlen(CPLGetPath(pszFilenameIn)) == 0 )
        pszBaseDir = CPLStrdup(".");
    else
        pszBaseDir = CPLStrdup(CPLGetPath(pszFilenameIn));

    VSIStatBuf  sStat;
    if( CPLStat( pszBaseDir, &sStat ) != 0 || !VSI_ISDIR( sStat.st_mode ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to create HKV dataset under %s,\n"
                  "but this is not a valid directory.\n", 
                  pszBaseDir);
        CPLFree( pszBaseDir );
        return NULL;
    }

    CPLFree( pszBaseDir );
    pszBaseDir = NULL;

    if( VSIMkdir( pszFilenameIn, 0755 ) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create directory %s.\n", 
                  pszFilenameIn );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the header file.                                         */
/* -------------------------------------------------------------------- */
    CPLErr CEHeaderCreated
        = SaveHKVAttribFile( pszFilenameIn, nXSize, nYSize,
                             nBands, eType, FALSE, 0.0 );

    if (CEHeaderCreated != CE_None )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create the blob file.                                           */
/* -------------------------------------------------------------------- */

    const char *pszFilename
        = CPLFormFilename( pszFilenameIn, "image_data", NULL );
    FILE *fp = VSIFOpen( pszFilename, "wb" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Couldn't create %s.\n", pszFilename );
        return NULL;
    }

    bool bOK = VSIFWrite( reinterpret_cast<void *>( const_cast<char *>( "" ) ), 1, 1, fp ) == 1;
    VSIFClose( fp );

    if( !bOK )
        return NULL;
/* -------------------------------------------------------------------- */
/*      Open the dataset normally.                                      */
/* -------------------------------------------------------------------- */
    return reinterpret_cast<GDALDataset *>(
        GDALOpen( pszFilenameIn, GA_Update ) );
}

/************************************************************************/
/*                               Delete()                               */
/*                                                                      */
/*      An HKV Blob dataset consists of a bunch of files in a           */
/*      directory.  Try to delete all the files, then the               */
/*      directory.                                                      */
/************************************************************************/

CPLErr HKVDataset::Delete( const char * pszName )

{
    VSIStatBuf	sStat;
    if( CPLStat( pszName, &sStat ) != 0 || !VSI_ISDIR(sStat.st_mode) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s does not appear to be an HKV Dataset, as it is not\n"
                  "a path to a directory.", 
                  pszName );
        return CE_Failure;
    }

    char **papszFiles = CPLReadDir( pszName );
    for( int i = 0; i < CSLCount(papszFiles); i++ )
    {
        if( EQUAL(papszFiles[i],".") || EQUAL(papszFiles[i],"..") )
            continue;

        const char *pszTarget = CPLFormFilename(pszName, papszFiles[i], NULL );
        if( VSIUnlink(pszTarget) != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to delete file %s,\n"
                      "HKVDataset Delete(%s) failed.\n", 
                      pszTarget, 
                      pszName );
            CSLDestroy( papszFiles );
            return CE_Failure;
        }
    }

    CSLDestroy( papszFiles );

    if( VSIRmdir( pszName ) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to delete directory %s,\n"
                  "HKVDataset Delete() failed.\n", 
                  pszName );
        return CE_Failure;
    }

    return CE_None;
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *
HKVDataset::CreateCopy( const char * pszFilename,
                        GDALDataset *poSrcDS,
                        CPL_UNUSED int bStrict,
                        char ** papszOptions,
                        GDALProgressFunc pfnProgress,
                        void * pProgressData )
{
    int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "HKV driver does not support source dataset with zero band.\n");
        return NULL;
    }

    GDALDataType eType = poSrcDS->GetRasterBand(1)->GetRasterDataType();

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

    /* check that other bands match type- sets type */
    /* to unknown if they differ.                  */
    for( int iBand = 1; iBand < poSrcDS->GetRasterCount(); iBand++ )
    {
        GDALRasterBand *poBand = poSrcDS->GetRasterBand( iBand+1 );
        eType = GDALDataTypeUnion( eType, poBand->GetRasterDataType() );
    }

    HKVDataset	*poDS
        = reinterpret_cast<HKVDataset *>( Create( pszFilename,
                                                  poSrcDS->GetRasterXSize(),
                                                  poSrcDS->GetRasterYSize(),
                                                  poSrcDS->GetRasterCount(),
                                                  eType, papszOptions ) );

   /* Check that Create worked- return Null if it didn't */
    if (poDS == NULL)
        return NULL;

/* -------------------------------------------------------------------- */
/*      Copy the image data.                                            */
/* -------------------------------------------------------------------- */
    const int nXSize = poDS->GetRasterXSize();
    const int nYSize = poDS->GetRasterYSize();

    int nBlockXSize, nBlockYSize;
    poDS->GetRasterBand(1)->GetBlockSize( &nBlockXSize, &nBlockYSize );

    const int nBlockTotal = ((nXSize + nBlockXSize - 1) / nBlockXSize)
        * ((nYSize + nBlockYSize - 1) / nBlockYSize)
        * poSrcDS->GetRasterCount();

    int nBlocksDone = 0;
    for( int iBand = 0; iBand < poSrcDS->GetRasterCount(); iBand++ )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );
        GDALRasterBand *poDstBand = poDS->GetRasterBand( iBand+1 );

        /* Get nodata value, if relevant */
        int pbSuccess;
        double dfSrcNoDataValue = poSrcBand->GetNoDataValue( &pbSuccess );
        if ( pbSuccess )
  	    poDS->SetNoDataValue( dfSrcNoDataValue );

        void *pData = CPLMalloc(
            nBlockXSize * nBlockYSize * GDALGetDataTypeSize(eType) / 8);

        CPLErr eErr;
        for( int iYOffset = 0; iYOffset < nYSize; iYOffset += nBlockYSize )
        {
            for( int iXOffset = 0; iXOffset < nXSize; iXOffset += nBlockXSize )
            {
                if( !pfnProgress(
                       (nBlocksDone++) / static_cast<float>( nBlockTotal ),
                       NULL, pProgressData ) )
                {
                    CPLError( CE_Failure, CPLE_UserInterrupt, 
                              "User terminated" );
                    delete poDS;
                    CPLFree(pData);

                    GDALDriver *poHKVDriver =
                        reinterpret_cast<GDALDriver *>(
                            GDALGetDriverByName( "MFF2" ) );
                    poHKVDriver->Delete( pszFilename );
                    return NULL;
                }

                const int nTBXSize = MIN(nBlockXSize,nXSize-iXOffset);
                const int nTBYSize = MIN(nBlockYSize,nYSize-iYOffset);

                eErr = poSrcBand->RasterIO( GF_Read, 
                                            iXOffset, iYOffset, 
                                            nTBXSize, nTBYSize,
                                            pData, nTBXSize, nTBYSize,
                                            eType, 0, 0, NULL );
                if( eErr != CE_None )
                {
                    delete poDS;
                    CPLFree(pData);
                    return NULL;
                }

                eErr = poDstBand->RasterIO( GF_Write, 
                                            iXOffset, iYOffset, 
                                            nTBXSize, nTBYSize,
                                            pData, nTBXSize, nTBYSize,
                                            eType, 0, 0, NULL );

                if( eErr != CE_None )
                {
                    delete poDS;
                    CPLFree(pData);
                    return NULL;
                }
            }
        }

        CPLFree( pData );
    }

/* -------------------------------------------------------------------- */
/*      Copy georeferencing information, if enough is available.        */
/*      Only copy geotransform-style info (won't work for slant range). */
/* -------------------------------------------------------------------- */

    double *tempGeoTransform
        = reinterpret_cast<double *>( CPLMalloc( 6 * sizeof(double) ) );

    if (( poSrcDS->GetGeoTransform( tempGeoTransform ) == CE_None)
        && (tempGeoTransform[0] != 0.0 || tempGeoTransform[1] != 1.0
        || tempGeoTransform[2] != 0.0 || tempGeoTransform[3] != 0.0
        || tempGeoTransform[4] != 0.0 || ABS(tempGeoTransform[5]) != 1.0 ))
    {

          poDS->SetGCPProjection(poSrcDS->GetProjectionRef());
          poDS->SetProjection(poSrcDS->GetProjectionRef());
          poDS->SetGeoTransform(tempGeoTransform);

          CPLFree(tempGeoTransform);

          /* georef file will be saved automatically when dataset is deleted */
          /* because SetProjection sets a flag to indicate it's necessary.   */

    }
    else
    {
          CPLFree(tempGeoTransform);
    }

    /* Make sure image data gets flushed */
    for( int iBand = 0; iBand < poDS->GetRasterCount(); iBand++ )
    {
        RawRasterBand *poDstBand = reinterpret_cast<RawRasterBand *>(
            poDS->GetRasterBand( iBand+1 ) );
        poDstBand->FlushCache();
    }


    if( !pfnProgress( 1.0, NULL, pProgressData ) )
    {
        CPLError( CE_Failure, CPLE_UserInterrupt, "User terminated" );
        delete poDS;

        GDALDriver *poHKVDriver =
            reinterpret_cast<GDALDriver *>( GDALGetDriverByName( "MFF2" ) );
        poHKVDriver->Delete( pszFilename );
        return NULL;
    }

    poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );

    return poDS;
}


/************************************************************************/
/*                         GDALRegister_HKV()                           */
/************************************************************************/

void GDALRegister_HKV()

{
    if( GDALGetDriverByName( "MFF2" ) != NULL )
        return;

    GDALDriver*poDriver = new GDALDriver();

    poDriver->SetDescription( "MFF2" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Vexcel MFF2 (HKV) Raster" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_mff2.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Int16 UInt16 Int32 UInt32 CInt16 "
                               "CInt32 Float32 Float64 CFloat32 CFloat64" );

    poDriver->pfnOpen = HKVDataset::Open;
    poDriver->pfnCreate = HKVDataset::Create;
    poDriver->pfnDelete = HKVDataset::Delete;
    poDriver->pfnCreateCopy = HKVDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
