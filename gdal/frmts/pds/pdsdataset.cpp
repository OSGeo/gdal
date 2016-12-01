/******************************************************************************
 *
 * Project:  PDS Driver; Planetary Data System Format
 * Purpose:  Implementation of PDSDataset
 * Author:   Trent Hare (thare@usgs.gov),
 *           Robert Soricone (rsoricone@usgs.gov)
 *
 * NOTE: Original code authored by Trent and Robert and placed in the public
 * domain as per US government policy.  I have (within my rights) appropriated
 * it and placed it under the following license.  This is not intended to
 * diminish Trent and Roberts contribution.
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

// Set up PDS NULL values
static const int NULL1 = 0;
static const int NULL2 = -32768;
//#define NULL3 -0.3402822655089E+39
//Same as ESRI_GRID_FLOAT_NO_DATA
//#define NULL3 -340282346638528859811704183484516925440.0
static const double NULL3 = -3.4028226550889044521e+38;

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "gdal_proxy.h"
#include "nasakeywordhandler.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

CPL_CVSID("$Id$");

enum PDSLayout
{
    PDS_BSQ,
    PDS_BIP,
    PDS_BIL
};

/************************************************************************/
/* ==================================================================== */
/*                             PDSDataset                               */
/* ==================================================================== */
/************************************************************************/

class PDSDataset : public RawDataset
{
    VSILFILE    *fpImage;  // image data file.
    GDALDataset *poCompressedDS;

    NASAKeywordHandler  oKeywords;

    int         bGotTransform;
    double      adfGeoTransform[6];

    CPLString   osProjection;

    CPLString   osTempResult;

    CPLString   osExternalCube;

    void        ParseSRS();
    int         ParseCompressedImage();
    int         ParseImage( CPLString osPrefix, CPLString osFilenamePrefix );
    static void        CleanString( CPLString &osInput );

    const char *GetKeyword( std::string osPath,
                            const char *pszDefault = "");
    const char *GetKeywordSub( std::string osPath,
                               int iSubscript,
                               const char *pszDefault = "");
    const char *GetKeywordUnit( const char *pszPath,
                               int iSubscript,
                               const char *pszDefault = "");

  protected:
    virtual int         CloseDependentDatasets() override;

public:
    PDSDataset();
    virtual ~PDSDataset();

    virtual CPLErr GetGeoTransform( double * padfTransform ) override;
    virtual const char *GetProjectionRef(void) override;

    virtual char      **GetFileList(void) override;

    virtual CPLErr IBuildOverviews( const char *, int, int *,
                                    int, int *, GDALProgressFunc, void * ) override;

    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              int, int *,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg) override;

    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
};

/************************************************************************/
/*                            PDSDataset()                            */
/************************************************************************/

PDSDataset::PDSDataset() :
    fpImage(NULL),
    poCompressedDS(NULL),
    bGotTransform(FALSE)
{
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~PDSDataset()                            */
/************************************************************************/

PDSDataset::~PDSDataset()

{
    FlushCache();
    if( fpImage != NULL )
        VSIFCloseL( fpImage );

    CloseDependentDatasets();
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int PDSDataset::CloseDependentDatasets()
{
    int bHasDroppedRef = GDALPamDataset::CloseDependentDatasets();

    if( poCompressedDS )
    {
        bHasDroppedRef = FALSE;
        delete poCompressedDS;
        poCompressedDS = NULL;
    }

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
       delete papoBands[iBand];
    }
    nBands = 0;

    return bHasDroppedRef;
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **PDSDataset::GetFileList()

{
    char **papszFileList = RawDataset::GetFileList();

    if( poCompressedDS != NULL )
    {
        char **papszCFileList = poCompressedDS->GetFileList();

        papszFileList = CSLInsertStrings( papszFileList, -1,
                                          papszCFileList );
        CSLDestroy( papszCFileList );
    }

    if( !osExternalCube.empty() )
    {
        papszFileList = CSLAddString( papszFileList, osExternalCube );
    }

    return papszFileList;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr PDSDataset::IBuildOverviews( const char *pszResampling,
                                    int nOverviews, int *panOverviewList,
                                    int nListBands, int *panBandList,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData )
{
    if( poCompressedDS != NULL )
        return poCompressedDS->BuildOverviews( pszResampling,
                                               nOverviews, panOverviewList,
                                               nListBands, panBandList,
                                               pfnProgress, pProgressData );

    return RawDataset::IBuildOverviews( pszResampling,
                                        nOverviews, panOverviewList,
                                        nListBands, panBandList,
                                        pfnProgress, pProgressData );
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr PDSDataset::IRasterIO( GDALRWFlag eRWFlag,
                              int nXOff, int nYOff, int nXSize, int nYSize,
                              void * pData, int nBufXSize, int nBufYSize,
                              GDALDataType eBufType,
                              int nBandCount, int *panBandMap,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GSpacing nBandSpace,
                              GDALRasterIOExtraArg* psExtraArg)

{
    if( poCompressedDS != NULL )
        return poCompressedDS->RasterIO( eRWFlag,
                                         nXOff, nYOff, nXSize, nYSize,
                                         pData, nBufXSize, nBufYSize,
                                         eBufType, nBandCount, panBandMap,
                                         nPixelSpace, nLineSpace, nBandSpace,
                                         psExtraArg);

    return RawDataset::IRasterIO( eRWFlag,
                                  nXOff, nYOff, nXSize, nYSize,
                                  pData, nBufXSize, nBufYSize,
                                  eBufType, nBandCount, panBandMap,
                                  nPixelSpace, nLineSpace, nBandSpace, psExtraArg );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *PDSDataset::GetProjectionRef()

{
    if( strlen(osProjection) > 0 )
        return osProjection;

    return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr PDSDataset::GetGeoTransform( double * padfTransform )

{
    if( bGotTransform )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                              ParseSRS()                              */
/************************************************************************/

void PDSDataset::ParseSRS()

{
    const char *pszFilename = GetDescription();

    CPLString osPrefix;
    if( strlen(GetKeyword( "IMAGE_MAP_PROJECTION.MAP_PROJECTION_TYPE")) == 0 &&
        strlen(GetKeyword( "UNCOMPRESSED_FILE.IMAGE_MAP_PROJECTION.MAP_PROJECTION_TYPE")) != 0 )
        osPrefix = "UNCOMPRESSED_FILE.";

/* ==================================================================== */
/*      Get the geotransform.                                           */
/* ==================================================================== */
    /***********   Grab Cellsize ************/
    //example:
    //MAP_SCALE   = 14.818 <KM/PIXEL>
    //added search for unit (only checks for CM, KM - defaults to Meters)
    //Georef parameters
    double dfXDim = 1.0;
    double dfYDim = 1.0;

    const char *value = GetKeyword(osPrefix + "IMAGE_MAP_PROJECTION.MAP_SCALE");
    if (strlen(value) > 0 ) {
        dfXDim = CPLAtof(value);
        dfYDim = CPLAtof(value) * -1;

        CPLString osKey(osPrefix + "IMAGE_MAP_PROJECTION.MAP_SCALE");
        CPLString unit = GetKeywordUnit(osKey,2); //KM
        //value = GetKeywordUnit("IMAGE_MAP_PROJECTION.MAP_SCALE",3); //PIXEL
        if((EQUAL(unit,"M"))  || (EQUAL(unit,"METER")) || (EQUAL(unit,"METERS"))) {
            // do nothing
        }
        else if (EQUAL(unit,"CM")) {
            // convert from cm to m
            dfXDim = dfXDim / 100.0;
            dfYDim = dfYDim / 100.0;
        } else {
            //defaults to convert km to m
            dfXDim = dfXDim * 1000.0;
            dfYDim = dfYDim * 1000.0;
        }
    }

/* -------------------------------------------------------------------- */
/*      Calculate upper left corner of pixel in meters from the         */
/*      upper  left center pixel sample/line offsets.  It doesn't       */
/*      mean the defaults will work for every PDS image, as these       */
/*      values are used inconsistently.  Thus we have included          */
/*      conversion options to allow the user to override the            */
/*      documented PDS3 default. Jan. 2011, for known mapping issues    */
/*      see GDAL PDS page or mapping within ISIS3 source (USGS)         */
/*      $ISIS3DATA/base/translations/pdsProjectionLineSampToXY.def      */
/* -------------------------------------------------------------------- */

    // defaults should be correct for what is documented in the PDS3 standard

    // https://trac.osgeo.org/gdal/ticket/5941 has the history of the default
    /* value of PDS_SampleProjOffset_Shift and PDS_LineProjOffset_Shift */
    double dfSampleOffset_Shift =
        CPLAtof(CPLGetConfigOption( "PDS_SampleProjOffset_Shift", "0.5" ));

    const double dfLineOffset_Shift =
        CPLAtof(CPLGetConfigOption( "PDS_LineProjOffset_Shift", "0.5" ));

    const double dfSampleOffset_Mult =
        CPLAtof(CPLGetConfigOption( "PDS_SampleProjOffset_Mult", "-1.0") );

    const double dfLineOffset_Mult =
        CPLAtof( CPLGetConfigOption( "PDS_LineProjOffset_Mult", "1.0") );

    /***********   Grab LINE_PROJECTION_OFFSET ************/
    double dfULYMap = 0.5;
    double yulcenter = 0.0;

    value = GetKeyword(osPrefix + "IMAGE_MAP_PROJECTION.LINE_PROJECTION_OFFSET");
    if (strlen(value) > 0) {
        yulcenter = CPLAtof(value);
        dfULYMap = ((yulcenter + dfLineOffset_Shift) * -dfYDim * dfLineOffset_Mult);
        //notice dfYDim is negative here which is why it is again negated here
    }
    /***********   Grab SAMPLE_PROJECTION_OFFSET ************/
    double dfULXMap = 0.5;
    double xulcenter = 0.0;

    value = GetKeyword(osPrefix + "IMAGE_MAP_PROJECTION.SAMPLE_PROJECTION_OFFSET");
    if( strlen(value) > 0 ) {
        xulcenter = CPLAtof(value);
        dfULXMap = ((xulcenter + dfSampleOffset_Shift) * dfXDim * dfSampleOffset_Mult);
    }

/* ==================================================================== */
/*      Get the coordinate system.                                      */
/* ==================================================================== */

    /***********  Grab TARGET_NAME  ************/
    /**** This is the planets name i.e. MARS ***/
    CPLString target_name = GetKeyword("TARGET_NAME");
    CleanString( target_name );

    /**********   Grab MAP_PROJECTION_TYPE *****/
    CPLString map_proj_name =
        GetKeyword( osPrefix + "IMAGE_MAP_PROJECTION.MAP_PROJECTION_TYPE");
    CleanString( map_proj_name );

    /******  Grab semi_major & convert to KM ******/
    const double semi_major =
        CPLAtof(GetKeyword( osPrefix + "IMAGE_MAP_PROJECTION.A_AXIS_RADIUS")) * 1000.0;

    /******  Grab semi-minor & convert to KM ******/
    const double semi_minor =
        CPLAtof(GetKeyword( osPrefix + "IMAGE_MAP_PROJECTION.C_AXIS_RADIUS")) * 1000.0;

    /***********   Grab CENTER_LAT ************/
    const double center_lat =
        CPLAtof(GetKeyword( osPrefix + "IMAGE_MAP_PROJECTION.CENTER_LATITUDE"));

    /***********   Grab CENTER_LON ************/
    const double center_lon =
        CPLAtof(GetKeyword( osPrefix + "IMAGE_MAP_PROJECTION.CENTER_LONGITUDE"));

    /**********   Grab 1st std parallel *******/
    const double first_std_parallel =
        CPLAtof(GetKeyword( osPrefix + "IMAGE_MAP_PROJECTION.FIRST_STANDARD_PARALLEL"));

    /**********   Grab 2nd std parallel *******/
    const double second_std_parallel =
        CPLAtof(GetKeyword( osPrefix + "IMAGE_MAP_PROJECTION.SECOND_STANDARD_PARALLEL"));

    /*** grab  PROJECTION_LATITUDE_TYPE = "PLANETOCENTRIC" ****/
    // Need to further study how ocentric/ographic will effect the gdal library.
    // So far we will use this fact to define a sphere or ellipse for some projections
    // Frank - may need to talk this over
    char bIsGeographic = TRUE;
    value = GetKeyword(osPrefix + "IMAGE_MAP_PROJECTION.COORDINATE_SYSTEM_NAME");
    if (EQUAL( value, "PLANETOCENTRIC" ))
        bIsGeographic = FALSE;

/**   Set oSRS projection and parameters --- all PDS supported types added if apparently supported in oSRS
      "AITOFF",  ** Not supported in GDAL??
      "ALBERS",
      "BONNE",
      "BRIESEMEISTER",   ** Not supported in GDAL??
      "CYLINDRICAL EQUAL AREA",
      "EQUIDISTANT",
      "EQUIRECTANGULAR",
      "GNOMONIC",
      "HAMMER",    ** Not supported in GDAL??
      "HENDU",     ** Not supported in GDAL??
      "LAMBERT AZIMUTHAL EQUAL AREA",
      "LAMBERT CONFORMAL",
      "MERCATOR",
      "MOLLWEIDE",
      "OBLIQUE CYLINDRICAL",
      "ORTHOGRAPHIC",
      "SIMPLE CYLINDRICAL",
      "SINUSOIDAL",
      "STEREOGRAPHIC",
      "TRANSVERSE MERCATOR",
      "VAN DER GRINTEN",     ** Not supported in GDAL??
      "WERNER"     ** Not supported in GDAL??
**/
    CPLDebug( "PDS","using projection %s\n\n", map_proj_name.c_str());

    bool bProjectionSet = true;
    OGRSpatialReference oSRS;

    if ((EQUAL( map_proj_name, "EQUIRECTANGULAR" )) ||
        (EQUAL( map_proj_name, "SIMPLE_CYLINDRICAL" )) ||
        (EQUAL( map_proj_name, "EQUIDISTANT" )) )  {
        oSRS.SetEquirectangular2 ( 0.0, center_lon, center_lat, 0, 0 );
    } else if (EQUAL( map_proj_name, "ORTHOGRAPHIC" )) {
        oSRS.SetOrthographic ( center_lat, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "SINUSOIDAL" )) {
        oSRS.SetSinusoidal ( center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "MERCATOR" )) {
        oSRS.SetMercator ( center_lat, center_lon, 1, 0, 0 );
    } else if (EQUAL( map_proj_name, "STEREOGRAPHIC" )) {
        oSRS.SetStereographic ( center_lat, center_lon, 1, 0, 0 );
    } else if (EQUAL( map_proj_name, "POLAR_STEREOGRAPHIC")) {
        oSRS.SetPS ( center_lat, center_lon, 1, 0, 0 );
    } else if (EQUAL( map_proj_name, "TRANSVERSE_MERCATOR" )) {
        oSRS.SetTM ( center_lat, center_lon, 1, 0, 0 );
    } else if (EQUAL( map_proj_name, "LAMBERT_CONFORMAL_CONIC" )) {
        oSRS.SetLCC ( first_std_parallel, second_std_parallel,
                      center_lat, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "LAMBERT_AZIMUTHAL_EQUAL_AREA" )) {
        oSRS.SetLAEA( center_lat, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "CYLINDRICAL_EQUAL_AREA" )) {
        oSRS.SetCEA  ( first_std_parallel, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "MOLLWEIDE" )) {
        oSRS.SetMollweide ( center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "ALBERS" )) {
        oSRS.SetACEA ( first_std_parallel, second_std_parallel,
                       center_lat, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "BONNE" )) {
        oSRS.SetBonne ( first_std_parallel, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "GNOMONIC" )) {
        oSRS.SetGnomonic ( center_lat, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "OBLIQUE_CYLINDRICAL" )) {
        // hope Swiss Oblique Cylindrical is the same
        oSRS.SetSOC ( center_lat, center_lon, 0, 0 );
    } else {
        CPLDebug( "PDS",
                  "Dataset projection %s is not supported. Continuing...",
                  map_proj_name.c_str() );
        bProjectionSet = false;
    }

    if (bProjectionSet) {
        //Create projection name, i.e. MERCATOR MARS and set as ProjCS keyword
        const CPLString proj_target_name = map_proj_name + " " + target_name;
        oSRS.SetProjCS(proj_target_name); //set ProjCS keyword

        //The geographic/geocentric name will be the same basic name as the body name
        //'GCS' = Geographic/Geocentric Coordinate System
        const CPLString geog_name = "GCS_" + target_name;

        //The datum and sphere names will be the same basic name aas the planet
        const CPLString datum_name = "D_" + target_name;
        // Might not be IAU defined so don't add.
        CPLString sphere_name = target_name; // + "_IAU_IAG");

        //calculate inverse flattening from major and minor axis: 1/f = a/(a-b)
        double iflattening;
        if ((semi_major - semi_minor) < 0.0000001)
            iflattening = 0;
        else
            iflattening = semi_major / (semi_major - semi_minor);

        //Set the body size but take into consideration which proj is being used to help w/ compatibility
        //Notice that most PDS projections are spherical based on the fact that ISIS/PICS are spherical
        //Set the body size but take into consideration which proj is being used to help w/ proj4 compatibility
        //The use of a Sphere, polar radius or ellipse here is based on how ISIS does it internally
        if ( ( (EQUAL( map_proj_name, "STEREOGRAPHIC" ) && (fabs(center_lat) == 90)) ) ||
             (EQUAL( map_proj_name, "POLAR_STEREOGRAPHIC" )))
        {
            if (bIsGeographic) {
                //Geograpraphic, so set an ellipse
                oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                                semi_major, iflattening,
                                "Reference_Meridian", 0.0 );
            } else {
                //Geocentric, so force a sphere using the semi-minor axis. I hope...
                sphere_name += "_polarRadius";
                oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                                semi_minor, 0.0,
                                "Reference_Meridian", 0.0 );
            }
        }
        else if ( (EQUAL( map_proj_name, "SIMPLE_CYLINDRICAL" )) ||
                  (EQUAL( map_proj_name, "EQUIDISTANT" )) ||
                  (EQUAL( map_proj_name, "ORTHOGRAPHIC" )) ||
                  (EQUAL( map_proj_name, "STEREOGRAPHIC" )) ||
                  (EQUAL( map_proj_name, "SINUSOIDAL" )) ) {
            //isis uses the spherical equation for these projections so force a sphere
            oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                            semi_major, 0.0,
                            "Reference_Meridian", 0.0 );
        }
        else if (EQUAL( map_proj_name, "EQUIRECTANGULAR" )) {
            //isis uses local radius as a sphere, which is pre-calculated in the PDS label as the semi-major
            sphere_name += "_localRadius";
            oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                            semi_major, 0.0,
                            "Reference_Meridian", 0.0 );
        }
        else {
            //All other projections: Mercator, Transverse Mercator, Lambert Conformal, etc.
            //Geographic, so set an ellipse
            if (bIsGeographic) {
                oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                                semi_major, iflattening,
                                "Reference_Meridian", 0.0 );
            } else {
                //Geocentric, so force a sphere. I hope...
                oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                                semi_major, 0.0,
                                "Reference_Meridian", 0.0 );
            }
        }

        // translate back into a projection string.
        char *pszResult = NULL;
        oSRS.exportToWkt( &pszResult );
        osProjection = pszResult;
        CPLFree( pszResult );
    }

/* ==================================================================== */
/*      Check for a .prj and world file to override the georeferencing. */
/* ==================================================================== */
    {
        const CPLString osPath = CPLGetPath( pszFilename );
        const CPLString osName = CPLGetBasename(pszFilename);
        const char  *pszPrjFile = CPLFormCIFilename( osPath, osName, "prj" );

        VSILFILE *fp = VSIFOpenL( pszPrjFile, "r" );
        if( fp != NULL )
        {
            OGRSpatialReference oSRS2;

            VSIFCloseL( fp );

            char **papszLines = CSLLoad( pszPrjFile );

            if( oSRS2.importFromESRI( papszLines ) == OGRERR_NONE )
            {
                char *pszResult = NULL;
                oSRS2.exportToWkt( &pszResult );
                osProjection = pszResult;
                CPLFree( pszResult );
            }

            CSLDestroy( papszLines );
        }
    }

    if( dfULXMap != 0.5 || dfULYMap != 0.5 || dfXDim != 1.0 || dfYDim != 1.0 )
    {
        bGotTransform = TRUE;
        adfGeoTransform[0] = dfULXMap;
        adfGeoTransform[1] = dfXDim;
        adfGeoTransform[2] = 0.0;
        adfGeoTransform[3] = dfULYMap;
        adfGeoTransform[4] = 0.0;
        adfGeoTransform[5] = dfYDim;
    }

    if( !bGotTransform )
        bGotTransform =
            GDALReadWorldFile( pszFilename, "psw",
                               adfGeoTransform );

    if( !bGotTransform )
        bGotTransform =
            GDALReadWorldFile( pszFilename, "wld",
                               adfGeoTransform );
}

/************************************************************************/
/*                        PDSConvertFromHex()                           */
/************************************************************************/

static GUInt32 PDSConvertFromHex(const char* pszVal)
{
    if( !STARTS_WITH_CI(pszVal, "16#") )
        return 0;

    pszVal += 3;
    GUInt32 nVal = 0;
    while( *pszVal != '#' && *pszVal != '\0' )
    {
        nVal <<= 4;
        if( *pszVal >= '0' && *pszVal <= '9' )
            nVal += *pszVal - '0';
        else if( *pszVal >= 'A' && *pszVal <= 'F' )
            nVal += *pszVal - 'A' + 10;
        else
            return 0;
        pszVal ++;
    }

    return nVal;
}

/************************************************************************/
/*                             ParseImage()                             */
/************************************************************************/

int PDSDataset::ParseImage( CPLString osPrefix, CPLString osFilenamePrefix )
{
/* ------------------------------------------------------------------- */
/*      We assume the user is pointing to the label (i.e. .lbl) file.  */
/* ------------------------------------------------------------------- */
    // IMAGE can be inline or detached and point to an image name
    // ^IMAGE = 3
    // ^IMAGE             = "GLOBAL_ALBEDO_8PPD.IMG"
    // ^IMAGE             = "MEGT90N000CB.IMG"
    // ^IMAGE             = ("BLAH.IMG",1)       -- start at record 1 (1 based)
    // ^IMAGE             = ("BLAH.IMG")         -- still start at record 1 (equiv of "BLAH.IMG")
    // ^IMAGE             = ("BLAH.IMG", 5 <BYTES>) -- start at byte 5 (the fifth byte in the file)
    // ^IMAGE             = 10851 <BYTES>
    // ^SPECTRAL_QUBE = 5  for multi-band images

    CPLString osImageKeyword = "IMAGE";
    CPLString osQube = GetKeyword( osPrefix + "^" + osImageKeyword, "" );
    CPLString osTargetFile = GetDescription();

    if (EQUAL(osQube,"")) {
        osImageKeyword = "SPECTRAL_QUBE";
        osQube = GetKeyword( osPrefix + "^" + osImageKeyword );
    }

    const int nQube = atoi(osQube);
    int nDetachedOffset = 0;
    bool bDetachedOffsetInBytes = false;

    if( osQube.size() && osQube[0] == '(' )
    {
        osQube = "\"";
        osQube += GetKeywordSub( osPrefix + "^" + osImageKeyword, 1 );
        osQube +=  "\"";
        nDetachedOffset = atoi(GetKeywordSub( osPrefix + "^" + osImageKeyword, 2, "1")) - 1;

        // If this is not explicitly in bytes, then it is assumed to be in
        // records, and we need to translate to bytes.
        if (strstr(GetKeywordSub(osPrefix + "^" + osImageKeyword, 2),"<BYTES>") != NULL)
            bDetachedOffsetInBytes = true;
    }

    if( osQube.size() && osQube[0] == '"' )
    {
        CPLString osFilename = osQube;
        CleanString( osFilename );
        if( osFilenamePrefix.size() )
        {
            osTargetFile = osFilenamePrefix + osFilename;
        }
        else
        {
            CPLString osTPath = CPLGetPath(GetDescription());
            osTargetFile = CPLFormCIFilename( osTPath, osFilename, NULL );
            osExternalCube = osTargetFile;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Checks to see if this is raw PDS image not compressed image     */
    /*      so ENCODING_TYPE either does not exist or it equals "N/A".      */
    /*      Compressed types will not be supported in this routine          */
    /* -------------------------------------------------------------------- */

    CPLString osEncodingType = GetKeyword(osPrefix+"IMAGE.ENCODING_TYPE","N/A");
    CleanString(osEncodingType);
    if ( !EQUAL(osEncodingType.c_str(),"N/A") )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "*** PDS image file has an ENCODING_TYPE parameter:\n"
                  "*** gdal pds driver does not support compressed image types\n"
                  "found: (%s)\n\n", osEncodingType.c_str() );
        return FALSE;
    }
    /**************** end ENCODING_TYPE check ***********************/

    /***********   Grab layout type (BSQ, BIP, BIL) ************/
    //  AXIS_NAME = (SAMPLE,LINE,BAND)
    /***********   Grab samples lines band        **************/
    /** if AXIS_NAME = "" then Bands=1 and Sample and Lines   **/
    /** are there own keywords  "LINES" and "LINE_SAMPLES"    **/
    /** if not NULL then CORE_ITEMS keyword i.e. (234,322,2)  **/
    /***********************************************************/
    int eLayout = PDS_BSQ; //default to band seq.
    int nRows, nCols, l_nBands = 1;

    CPLString value = GetKeyword( osPrefix+osImageKeyword+".AXIS_NAME", "" );
    if (EQUAL(value,"(SAMPLE,LINE,BAND)") ) {
        eLayout = PDS_BSQ;
        nCols = atoi(GetKeywordSub(osPrefix+osImageKeyword+".CORE_ITEMS",1));
        nRows = atoi(GetKeywordSub(osPrefix+osImageKeyword+".CORE_ITEMS",2));
        l_nBands = atoi(GetKeywordSub(osPrefix+osImageKeyword+".CORE_ITEMS",3));
    }
    else if (EQUAL(value,"(BAND,LINE,SAMPLE)") ) {
        eLayout = PDS_BIP;
        l_nBands = atoi(GetKeywordSub(osPrefix+osImageKeyword+".CORE_ITEMS",1));
        nRows = atoi(GetKeywordSub(osPrefix+osImageKeyword+".CORE_ITEMS",2));
        nCols = atoi(GetKeywordSub(osPrefix+osImageKeyword+".CORE_ITEMS",3));
    }
    else if (EQUAL(value,"(SAMPLE,BAND,LINE)") ) {
        eLayout = PDS_BIL;
        nCols = atoi(GetKeywordSub(osPrefix+osImageKeyword+".CORE_ITEMS",1));
        l_nBands = atoi(GetKeywordSub(osPrefix+osImageKeyword+".CORE_ITEMS",2));
        nRows = atoi(GetKeywordSub(osPrefix+osImageKeyword+".CORE_ITEMS",3));
    }
    else if ( EQUAL(value,"") ) {
        eLayout = PDS_BSQ;
        nCols = atoi(GetKeyword(osPrefix+osImageKeyword+".LINE_SAMPLES",""));
        nRows = atoi(GetKeyword(osPrefix+osImageKeyword+".LINES",""));
        l_nBands = atoi(GetKeyword(osPrefix+osImageKeyword+".BANDS","1"));
    }
    else {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "%s layout not supported. Abort\n\n", value.c_str() );
        return FALSE;
    }

    /***********   Grab Qube record bytes  **********/
    int record_bytes = atoi(GetKeyword(osPrefix+"IMAGE.RECORD_BYTES"));
    if (record_bytes == 0)
        record_bytes = atoi(GetKeyword(osPrefix+"RECORD_BYTES"));

    // this can happen with "record_type = undefined".
    if( record_bytes == 0 )
        record_bytes = 1;

    int nSkipBytes = 0;
    if( nQube >0 && osQube.find("<BYTES>") != CPLString::npos )
        nSkipBytes = nQube - 1;
    else if (nQube > 0 )
        nSkipBytes = (nQube - 1) * record_bytes;
    else if( nDetachedOffset > 0 )
    {
        if (bDetachedOffsetInBytes)
            nSkipBytes = nDetachedOffset;
        else
            nSkipBytes = nDetachedOffset * record_bytes;
    }
    else
        nSkipBytes = 0;

    nSkipBytes += atoi(GetKeyword(osPrefix+"IMAGE.LINE_PREFIX_BYTES",""));

    /***********   Grab SAMPLE_TYPE *****************/
    /** if keyword not found leave as "M" or "MSB" **/

    CPLString osST = GetKeyword( osPrefix+"IMAGE.SAMPLE_TYPE" );
    if( osST.size() >= 2 && osST[0] == '"' && osST[osST.size()-1] == '"' )
        osST = osST.substr( 1, osST.size() - 2 );

    char chByteOrder = 'M';  //default to MSB
    if( (EQUAL(osST,"LSB_INTEGER")) ||
        (EQUAL(osST,"LSB")) || // just in case
        (EQUAL(osST,"LSB_UNSIGNED_INTEGER")) ||
        (EQUAL(osST,"LSB_SIGNED_INTEGER")) ||
        (EQUAL(osST,"UNSIGNED_INTEGER")) ||
        (EQUAL(osST,"VAX_REAL")) ||
        (EQUAL(osST,"VAX_INTEGER")) ||
        (EQUAL(osST,"PC_INTEGER")) ||  //just in case
        (EQUAL(osST,"PC_REAL")) ) {
        chByteOrder = 'I';
    }

    /**** Grab format type - pds supports 1,2,4,8,16,32,64 (in theory) **/
    /**** I have only seen 8, 16, 32 (float) in released datasets      **/
    GDALDataType eDataType = GDT_Byte;
    int nSuffixItems = 0;
    int nSuffixLines = 0;
    int nSuffixBytes = 4; // Default as per PDS specification
    double dfNoData = 0.0;
    double dfScale = 1.0;
    double dfOffset = 0.0;
    const char *pszUnit = NULL;
    const char *pszDesc = NULL;

    CPLString osSB = GetKeyword(osPrefix+"IMAGE.SAMPLE_BITS","");
    if ( osSB.size() > 0 )
    {
        const int itype = atoi(osSB);
        switch(itype) {
          case 8 :
            eDataType = GDT_Byte;
            dfNoData = NULL1;
            break;
          case 16 :
            if( strstr(osST,"UNSIGNED") != NULL )
            {
                dfNoData = NULL1;
                eDataType = GDT_UInt16;
            }
            else
            {
                eDataType = GDT_Int16;
                dfNoData = NULL2;
            }
            break;
          case 32 :
            eDataType = GDT_Float32;
            dfNoData = NULL3;
            break;
          case 64 :
            eDataType = GDT_Float64;
            dfNoData = NULL3;
            break;
          default :
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Sample_bits of %d is not supported in this gdal PDS reader.",
                      itype);
            return FALSE;
        }

        dfOffset = CPLAtofM(GetKeyword(osPrefix+"IMAGE.OFFSET","0.0"));
        dfScale = CPLAtofM(GetKeyword(osPrefix+"IMAGE.SCALING_FACTOR","1.0"));
    }
    else /* No IMAGE object, search for the QUBE. */
    {
        osSB = GetKeyword(osPrefix+"SPECTRAL_QUBE.CORE_ITEM_BYTES","");
        const int itype = atoi(osSB);
        switch(itype) {
          case 1 :
            eDataType = GDT_Byte;
            break;
          case 2 :
            if( strstr(osST,"UNSIGNED") != NULL )
                eDataType = GDT_UInt16;
            else
                eDataType = GDT_Int16;
            break;
          case 4 :
            eDataType = GDT_Float32;
            break;
          default :
            CPLError( CE_Failure, CPLE_AppDefined,
                      "CORE_ITEM_BYTES of %d is not supported in this gdal PDS reader.",
                      itype);
            return FALSE;
        }

        /* Parse suffix dimensions if defined. */
        value = GetKeyword( osPrefix + "SPECTRAL_QUBE.SUFFIX_ITEMS", "" );
        if ( value.size() > 0 )
        {
            value = GetKeyword(osPrefix + "SPECTRAL_QUBE.SUFFIX_BYTES", "");
            if ( value.size() > 0 )
                nSuffixBytes = atoi( value );

            nSuffixItems = atoi(
                GetKeywordSub(osPrefix+"SPECTRAL_QUBE.SUFFIX_ITEMS",1));
            nSuffixLines = atoi(
                GetKeywordSub(osPrefix+"SPECTRAL_QUBE.SUFFIX_ITEMS",2));
        }

        value = GetKeyword( osPrefix + "SPECTRAL_QUBE.CORE_NULL", "" );
        if ( value.size() > 0 )
            dfNoData = CPLAtofM( value );

        dfOffset = CPLAtofM(
            GetKeyword(osPrefix + "SPECTRAL_QUBE.CORE_BASE", "0.0") );
        dfScale = CPLAtofM(
            GetKeyword(osPrefix + "SPECTRAL_QUBE.CORE_MULTIPLIER", "1.0") );
        pszUnit = GetKeyword(osPrefix + "SPECTRAL_QUBE.CORE_UNIT", NULL);
        pszDesc = GetKeyword(osPrefix + "SPECTRAL_QUBE.CORE_NAME", NULL);
    }

/* -------------------------------------------------------------------- */
/*      Is there a specific nodata value in the file? Either the        */
/*      MISSING or MISSING_CONSTANT keywords are nodata.                */
/* -------------------------------------------------------------------- */

    const char* pszMissing = GetKeyword( osPrefix+"IMAGE.MISSING", NULL );
    if( pszMissing == NULL )
        pszMissing = GetKeyword( osPrefix+"IMAGE.MISSING_CONSTANT", NULL );

    if( pszMissing != NULL )
    {
        if( *pszMissing == '"' )
            pszMissing ++;

        /* For example : MISSING_CONSTANT             = "16#FF7FFFFB#" */
        if( STARTS_WITH_CI(pszMissing, "16#") && strlen(pszMissing) >= 3 + 8 + 1 &&
            pszMissing[3 + 8] == '#' && (eDataType == GDT_Float32 || eDataType == GDT_Float64) )
        {
            GUInt32 nVal = PDSConvertFromHex(pszMissing);
            float fVal;
            memcpy(&fVal, &nVal, 4);
            dfNoData = fVal;
        }
        else
            dfNoData = CPLAtofM( pszMissing );
    }

/* -------------------------------------------------------------------- */
/*      Did we get the required keywords?  If not we return with        */
/*      this never having been considered to be a match. This isn't     */
/*      an error!                                                       */
/* -------------------------------------------------------------------- */
    if( nRows < 1 || nCols < 1 || l_nBands < 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "File %s appears to be a PDS file, but failed to find some required keywords.",
                  GetDescription() );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    nRasterXSize = nCols;
    nRasterYSize = nRows;

/* -------------------------------------------------------------------- */
/*      Open target binary file.                                        */
/* -------------------------------------------------------------------- */

    if( eAccess == GA_ReadOnly )
    {
        fpImage = VSIFOpenL( osTargetFile, "rb" );
        if( fpImage == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                    "Failed to open %s.\n%s",
                    osTargetFile.c_str(),
                    VSIStrerror( errno ) );
            return FALSE;
        }
    }
    else
    {
        fpImage = VSIFOpenL( osTargetFile, "r+b" );
        if( fpImage == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                    "Failed to open %s with write permission.\n%s",
                    osTargetFile.c_str(),
                    VSIStrerror( errno ) );
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Compute the line offset.                                        */
/* -------------------------------------------------------------------- */
    const int nItemSize = GDALGetDataTypeSize(eDataType)/8;
    int nLineOffset;
    int nPixelOffset;
    int nBandOffset;

    if( eLayout == PDS_BIP )
    {
        nPixelOffset = nItemSize * l_nBands;
        nBandOffset = nItemSize;
        nLineOffset = ((nPixelOffset * nCols + record_bytes - 1)/record_bytes)
            * record_bytes;
    }
    else if( eLayout == PDS_BSQ )
    {
        nPixelOffset = nItemSize;
        nLineOffset = ((nPixelOffset * nCols + record_bytes - 1)/record_bytes)
            * record_bytes;
        nBandOffset = nLineOffset * nRows
            + nSuffixLines * (nCols + nSuffixItems) * nSuffixBytes;
    }
    else /* assume BIL */
    {
        nPixelOffset = nItemSize;
        nBandOffset = nItemSize * nCols;
        nLineOffset = ((nBandOffset * nCols + record_bytes - 1)/record_bytes)
            * record_bytes;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < l_nBands; i++ )
    {
        RawRasterBand *poBand =
            new RawRasterBand( this, i+1, fpImage,
                               nSkipBytes + nBandOffset * i,
                               nPixelOffset, nLineOffset, eDataType,
#ifdef CPL_LSB
                               chByteOrder == 'I' || chByteOrder == 'L',
#else
                               chByteOrder == 'M',
#endif
                               TRUE );

        if( l_nBands == 1 )
        {
            const char* pszMin = GetKeyword(osPrefix+"IMAGE.MINIMUM", NULL);
            const char* pszMax = GetKeyword(osPrefix+"IMAGE.MAXIMUM", NULL);
            const char* pszMean = GetKeyword(osPrefix+"IMAGE.MEAN", NULL);
            const char* pszStdDev= GetKeyword(osPrefix+"IMAGE.STANDARD_DEVIATION", NULL);
            if (pszMin != NULL && pszMax != NULL &&
                pszMean != NULL && pszStdDev != NULL)
            {
                poBand->SetStatistics( CPLAtofM(pszMin),
                                       CPLAtofM(pszMax),
                                       CPLAtofM(pszMean),
                                       CPLAtofM(pszStdDev));
            }
        }

        poBand->SetNoDataValue( dfNoData );

        SetBand( i+1, poBand );

        // Set offset/scale values at the PAM level.
        poBand->SetOffset( dfOffset );
        poBand->SetScale( dfScale );
        if ( pszUnit )
            poBand->SetUnitType( pszUnit );
        if ( pszDesc )
            poBand->SetDescription( pszDesc );
    }

    return TRUE;
}

/************************************************************************/
/* ==================================================================== */
/*                         PDSWrapperRasterBand                         */
/*                                                                      */
/*      proxy for the jp2 or other compressed bands.                    */
/* ==================================================================== */
/************************************************************************/
class PDSWrapperRasterBand : public GDALProxyRasterBand
{
  GDALRasterBand* poBaseBand;

  protected:
    virtual GDALRasterBand* RefUnderlyingRasterBand() override { return poBaseBand; }

  public:
    explicit PDSWrapperRasterBand( GDALRasterBand* poBaseBandIn )
        {
            this->poBaseBand = poBaseBandIn;
            eDataType = poBaseBand->GetRasterDataType();
            poBaseBand->GetBlockSize(&nBlockXSize, &nBlockYSize);
        }
    ~PDSWrapperRasterBand() {}
};

/************************************************************************/
/*                       ParseCompressedImage()                         */
/************************************************************************/

int PDSDataset::ParseCompressedImage()

{
    CPLString osFileName = GetKeyword( "COMPRESSED_FILE.FILE_NAME", "" );
    CleanString( osFileName );

    const CPLString osPath = CPLGetPath(GetDescription());
    const CPLString osFullFileName = CPLFormFilename( osPath, osFileName, NULL );

    poCompressedDS = reinterpret_cast<GDALDataset *>(
        GDALOpen( osFullFileName, GA_ReadOnly ) );

    if( poCompressedDS == NULL )
        return FALSE;

    nRasterXSize = poCompressedDS->GetRasterXSize();
    nRasterYSize = poCompressedDS->GetRasterYSize();

    for( int iBand = 0; iBand < poCompressedDS->GetRasterCount(); iBand++ )
    {
        SetBand( iBand+1, new PDSWrapperRasterBand( poCompressedDS->GetRasterBand( iBand+1 ) ) );
    }

    return TRUE;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int PDSDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->pabyHeader == NULL )
        return FALSE;

    return strstr(reinterpret_cast<char *>( poOpenInfo->pabyHeader ),
                  "PDS_VERSION_ID") != NULL ||
           strstr(reinterpret_cast<char *>( poOpenInfo->pabyHeader ),
                  "ODL_VERSION_ID") != NULL;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *PDSDataset::Open( GDALOpenInfo * poOpenInfo )
{
    if( !Identify( poOpenInfo ) )
        return NULL;

    if( strstr(reinterpret_cast<char *>( poOpenInfo->pabyHeader ),
                  "PDS_VERSION_ID") != NULL &&
        strstr((const char *)poOpenInfo->pabyHeader,"PDS3") == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "It appears this is an older PDS image type.  Only PDS_VERSION_ID = PDS3 are currently supported by this gdal PDS reader.");
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open and parse the keyword header.  Sometimes there is stuff    */
/*      before the PDS_VERSION_ID, which we want to ignore.             */
/* -------------------------------------------------------------------- */
    VSILFILE *fpQube = VSIFOpenL( poOpenInfo->pszFilename, "rb" );

    if( fpQube == NULL )
        return NULL;

    PDSDataset *poDS = new PDSDataset();
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->eAccess = poOpenInfo->eAccess;

    const char* pszPDSVersionID = strstr((const char *)poOpenInfo->pabyHeader,"PDS_VERSION_ID");
    int nOffset = 0;
    if (pszPDSVersionID)
        nOffset = static_cast<int>(pszPDSVersionID - (const char *)poOpenInfo->pabyHeader);

    if( ! poDS->oKeywords.Ingest( fpQube, nOffset ) )
    {
        delete poDS;
        VSIFCloseL( fpQube );
        return NULL;
    }
    VSIFCloseL( fpQube );

/* -------------------------------------------------------------------- */
/*      Is this a compressed image with COMPRESSED_FILE subdomain?      */
/*                                                                      */
/*      The corresponding parse operations will read keywords,          */
/*      establish bands and raster size.                                */
/* -------------------------------------------------------------------- */
    CPLString osEncodingType = poDS->GetKeyword( "COMPRESSED_FILE.ENCODING_TYPE", "" );

    CPLString osCompressedFilename = poDS->GetKeyword( "COMPRESSED_FILE.FILE_NAME", "" );
    CleanString( osCompressedFilename );

    CPLString osUncompressedFilename = poDS->GetKeyword( "UNCOMPRESSED_FILE.IMAGE.NAME", "");
    if( osUncompressedFilename.size() == 0 )
        osUncompressedFilename = poDS->GetKeyword( "UNCOMPRESSED_FILE.FILE_NAME", "");
    CleanString( osUncompressedFilename );

    VSIStatBufL sStat;
    CPLString osFilenamePrefix;

    if( EQUAL(osEncodingType, "ZIP") &&
        osCompressedFilename.size() != 0 &&
        osUncompressedFilename.size() != 0 )
    {
        const CPLString osPath = CPLGetPath(poDS->GetDescription());
        osCompressedFilename = CPLFormFilename( osPath, osCompressedFilename, NULL );
        osUncompressedFilename = CPLFormFilename( osPath, osUncompressedFilename, NULL );
        if( VSIStatExL(osCompressedFilename, &sStat, VSI_STAT_EXISTS_FLAG) == 0 &&
            VSIStatExL(osUncompressedFilename, &sStat, VSI_STAT_EXISTS_FLAG) != 0 )
        {
            osFilenamePrefix = "/vsizip/" + osCompressedFilename + "/";
            poDS->osExternalCube = osCompressedFilename;
        }
        osEncodingType = "";
    }

    if( osEncodingType.size() != 0 )
    {
        if( !poDS->ParseCompressedImage() )
        {
            delete poDS;
            return NULL;
        }
    }
    else
    {
        CPLString osPrefix;

        if( osUncompressedFilename != "" )
            osPrefix = "UNCOMPRESSED_FILE.";

        if( !poDS->ParseImage(osPrefix, osFilenamePrefix) )
        {
            delete poDS;
            return NULL;
        }
    }

/* -------------------------------------------------------------------- */
/*      Set the coordinate system and geotransform.                     */
/* -------------------------------------------------------------------- */
    poDS->ParseSRS();

/* -------------------------------------------------------------------- */
/*      Transfer a few interesting keywords as metadata.                */
/* -------------------------------------------------------------------- */
    static const char * const apszKeywords[] =
        { "FILTER_NAME", "DATA_SET_ID", "PRODUCT_ID",
          "PRODUCER_INSTITUTION_NAME", "PRODUCT_TYPE", "MISSION_NAME",
          "SPACECRAFT_NAME", "INSTRUMENT_NAME", "INSTRUMENT_ID",
          "TARGET_NAME", "CENTER_FILTER_WAVELENGTH", "BANDWIDTH",
          "PRODUCT_CREATION_TIME", "NOTE",
          NULL };

    for( int i = 0; apszKeywords[i] != NULL; i++ )
    {
        const char *pszKeywordValue = poDS->GetKeyword( apszKeywords[i] );

        if( pszKeywordValue != NULL )
            poDS->SetMetadataItem( apszKeywords[i], pszKeywordValue );
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                             GetKeyword()                             */
/************************************************************************/

const char *PDSDataset::GetKeyword( std::string osPath,
                                    const char *pszDefault )

{
    return oKeywords.GetKeyword( osPath.c_str(), pszDefault );
}

/************************************************************************/
/*                            GetKeywordSub()                           */
/************************************************************************/

const char *PDSDataset::GetKeywordSub( std::string osPath,
                                       int iSubscript,
                                       const char *pszDefault )

{
    const char *pszResult = oKeywords.GetKeyword( osPath.c_str(), NULL );

    if( pszResult == NULL )
        return pszDefault;

    if( pszResult[0] != '(' )
        return pszDefault;

    char **papszTokens = CSLTokenizeString2( pszResult, "(,)",
                                             CSLT_HONOURSTRINGS );

    if( iSubscript <= CSLCount(papszTokens) )
    {
        osTempResult = papszTokens[iSubscript-1];
        CSLDestroy( papszTokens );
        return osTempResult.c_str();
    }

    CSLDestroy( papszTokens );
    return pszDefault;
}

/************************************************************************/
/*                            GetKeywordUnit()                          */
/************************************************************************/

const char *PDSDataset::GetKeywordUnit( const char *pszPath,
                                         int iSubscript,
                                         const char *pszDefault )

{
    const char *pszResult = oKeywords.GetKeyword( pszPath, NULL );

    if( pszResult == NULL )
        return pszDefault;

    char **papszTokens = CSLTokenizeString2( pszResult, "</>",
                                             CSLT_HONOURSTRINGS );

    if( iSubscript <= CSLCount(papszTokens) )
    {
        osTempResult = papszTokens[iSubscript-1];
        CSLDestroy( papszTokens );
        return osTempResult.c_str();
    }

    CSLDestroy( papszTokens );
    return pszDefault;
}

/************************************************************************/
/*                            CleanString()                             */
/*                                                                      */
/* Removes single or double quotes, and converts spaces to underscores. */
/* The change is made in-place to CPLString.                            */
/************************************************************************/

void PDSDataset::CleanString( CPLString &osInput )

{
   if(  ( osInput.size() < 2 ) ||
        ((osInput.at(0) != '"'   || osInput.at(osInput.size()-1) != '"' ) &&
        ( osInput.at(0) != '\'' || osInput.at(osInput.size()-1) != '\'')) )
        return;

    char *pszWrk = CPLStrdup(osInput.c_str() + 1);

    pszWrk[strlen(pszWrk)-1] = '\0';

    for( int i = 0; pszWrk[i] != '\0'; i++ )
    {
        if( pszWrk[i] == ' ' )
            pszWrk[i] = '_';
    }

    osInput = pszWrk;
    CPLFree( pszWrk );
}
/************************************************************************/
/*                         GDALRegister_PDS()                           */
/************************************************************************/

void GDALRegister_PDS()

{
    if( GDALGetDriverByName( "PDS" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "PDS" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "NASA Planetary Data System" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "frmt_pds.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = PDSDataset::Open;
    poDriver->pfnIdentify = PDSDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
