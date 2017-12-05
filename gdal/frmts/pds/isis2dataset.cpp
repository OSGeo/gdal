/******************************************************************************
 *
 * Project:  ISIS Version 2 Driver
 * Purpose:  Implementation of ISIS2Dataset
 * Author:   Trent Hare (thare@usgs.gov),
 *           Robert Soricone (rsoricone@usgs.gov)
 *           Ludovic Mercier (ludovic.mercier@gmail.com)
 *           Frank Warmerdam (warmerdam@pobox.com)
 *
 * NOTE: Original code authored by Trent and Robert and placed in the public
 * domain as per US government policy.  I have (within my rights) appropriated
 * it and placed it under the following license.  This is not intended to
 * diminish Trent and Roberts contribution.
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

static const int NULL1 = 0;
static const int NULL2 = -32768;
static const double NULL3 = -3.4028226550889044521e+38;

static const int RECORD_SIZE = 512;

#include "cpl_string.h"
#include "gdal_frmts.h"
#include "nasakeywordhandler.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                      ISISDataset     version2                        */
/* ==================================================================== */
/************************************************************************/

class ISIS2Dataset : public RawDataset
{
    VSILFILE     *fpImage;      // image data file.
    CPLString    osExternalCube;

    NASAKeywordHandler  oKeywords;

    int         bGotTransform;
    double      adfGeoTransform[6];

    CPLString   osProjection;

    int parse_label(const char *file, char *keyword, char *value);
    int strstrip(char instr[], char outstr[], int position);

    CPLString   oTempResult;

    void        CleanString( CPLString &osInput );

    const char *GetKeyword( const char *pszPath,
                            const char *pszDefault = "");
    const char *GetKeywordSub( const char *pszPath,
                               int iSubscript,
                               const char *pszDefault = "");

public:
    ISIS2Dataset();
    virtual ~ISIS2Dataset();

    virtual CPLErr GetGeoTransform( double * padfTransform ) override;
    virtual const char *GetProjectionRef(void) override;

    virtual char **GetFileList() override;

    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );

    // Write related.
    static int WriteRaster(CPLString osFilename, bool includeLabel, GUIntBig iRecord, GUIntBig iLabelRecords, GDALDataType eType, const char * pszInterleaving);

    static int WriteLabel(CPLString osFilename, CPLString osRasterFile, CPLString sObjectTag, unsigned int nXSize, unsigned int nYSize, unsigned int nBands, GDALDataType eType,
                          GUIntBig iRecords, const char * pszInterleaving, GUIntBig & iLabelRecords, bool bRelaunch=false);
    static int WriteQUBE_Information(VSILFILE *fpLabel, unsigned int iLevel, unsigned int & nWritingBytes,
                                     unsigned int nXSize, unsigned int nYSize, unsigned int nBands, GDALDataType eType, const char * pszInterleaving);

    static unsigned int WriteKeyword(VSILFILE *fpLabel, unsigned int iLevel, CPLString key, CPLString value);
    static unsigned int WriteFormatting(VSILFILE *fpLabel, CPLString data);
    static GUIntBig RecordSizeCalculation(unsigned int nXSize, unsigned int nYSize, unsigned int nBands, GDALDataType eType );
};

/************************************************************************/
/*                            ISIS2Dataset()                            */
/************************************************************************/

ISIS2Dataset::ISIS2Dataset() :
    fpImage(NULL),
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
/*                            ~ISIS2Dataset()                            */
/************************************************************************/

ISIS2Dataset::~ISIS2Dataset()

{
    FlushCache();
    if( fpImage != NULL )
        VSIFCloseL( fpImage );
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **ISIS2Dataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    if( !osExternalCube.empty() )
        papszFileList = CSLAddString( papszFileList, osExternalCube );

    return papszFileList;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *ISIS2Dataset::GetProjectionRef()

{
    if( !osProjection.empty() )
        return osProjection;

    return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ISIS2Dataset::GetGeoTransform( double * padfTransform )

{
    if( bGotTransform )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
        return CE_None;
    }

    return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int ISIS2Dataset::Identify( GDALOpenInfo * poOpenInfo )
{
    if( poOpenInfo->pabyHeader == NULL )
        return FALSE;

    if( strstr((const char *)poOpenInfo->pabyHeader,"^QUBE") == NULL )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ISIS2Dataset::Open( GDALOpenInfo * poOpenInfo )
{
/* -------------------------------------------------------------------- */
/*      Does this look like a CUBE or an IMAGE Primary Data Object?     */
/* -------------------------------------------------------------------- */
    if( !Identify( poOpenInfo ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Open the file using the large file API.                         */
/* -------------------------------------------------------------------- */
    VSILFILE *fpQube = VSIFOpenL( poOpenInfo->pszFilename, "rb" );

    if( fpQube == NULL )
        return NULL;

    ISIS2Dataset *poDS = new ISIS2Dataset();

    if( ! poDS->oKeywords.Ingest( fpQube, 0 ) )
    {
        VSIFCloseL( fpQube );
        delete poDS;
        return NULL;
    }

    VSIFCloseL( fpQube );

/* -------------------------------------------------------------------- */
/*      We assume the user is pointing to the label (i.e. .lab) file.   */
/* -------------------------------------------------------------------- */
    // QUBE can be inline or detached and point to an image name
    // ^QUBE = 76
    // ^QUBE = ("ui31s015.img",6441<BYTES>) - has another label on the image
    // ^QUBE = "ui31s015.img" - which implies no label or skip value

    const char *pszQube = poDS->GetKeyword( "^QUBE" );
    GUIntBig nQube = 0;
    int bByteLocation = FALSE;
    CPLString osTargetFile = poOpenInfo->pszFilename;

    if( pszQube[0] == '"' )
    {
        const CPLString osTPath = CPLGetPath(poOpenInfo->pszFilename);
        CPLString osFilename = pszQube;
        poDS->CleanString( osFilename );
        osTargetFile = CPLFormCIFilename( osTPath, osFilename, NULL );
        poDS->osExternalCube = osTargetFile;
    }
    else if( pszQube[0] == '(' )
    {
        const CPLString osTPath = CPLGetPath(poOpenInfo->pszFilename);
        CPLString osFilename = poDS->GetKeywordSub("^QUBE",1,"");
        poDS->CleanString( osFilename );
        osTargetFile = CPLFormCIFilename( osTPath, osFilename, NULL );
        poDS->osExternalCube = osTargetFile;

        nQube = atoi(poDS->GetKeywordSub("^QUBE",2,"1"));
        if( strstr(poDS->GetKeywordSub("^QUBE",2,"1"),"<BYTES>") != NULL )
            bByteLocation = true;
    }
    else
    {
        nQube = atoi(pszQube);
        if( strstr(pszQube,"<BYTES>") != NULL )
            bByteLocation = true;
    }

/* -------------------------------------------------------------------- */
/*      Check if file an ISIS2 header file?  Read a few lines of text   */
/*      searching for something starting with nrows or ncols.           */
/* -------------------------------------------------------------------- */

    /* -------------------------------------------------------------------- */
    /*      Checks to see if this is valid ISIS2 cube                       */
    /*      SUFFIX_ITEM tag in .cub file should be (0,0,0); no side-planes  */
    /* -------------------------------------------------------------------- */
    const int s_ix = atoi(poDS->GetKeywordSub( "QUBE.SUFFIX_ITEMS", 1 ));
    const int s_iy = atoi(poDS->GetKeywordSub( "QUBE.SUFFIX_ITEMS", 2 ));
    const int s_iz = atoi(poDS->GetKeywordSub( "QUBE.SUFFIX_ITEMS", 3 ));

    if( s_ix != 0 || s_iy != 0 || s_iz != 0 )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "*** ISIS 2 cube file has invalid SUFFIX_ITEMS parameters:\n"
                  "*** gdal isis2 driver requires (0, 0, 0), thus no sideplanes or backplanes\n"
                  "found: (%i, %i, %i)\n\n", s_ix, s_iy, s_iz );
        delete poDS;
        return NULL;
    }

    /**************** end SUFFIX_ITEM check ***********************/

    /***********   Grab layout type (BSQ, BIP, BIL) ************/
    //  AXIS_NAME = (SAMPLE,LINE,BAND)
    /***********************************************************/

    char szLayout[10] = "BSQ"; //default to band seq.
    const char *value = poDS->GetKeyword( "QUBE.AXIS_NAME", "" );
    if (EQUAL(value,"(SAMPLE,LINE,BAND)") )
        strcpy(szLayout,"BSQ");
    else if (EQUAL(value,"(BAND,LINE,SAMPLE)") )
        strcpy(szLayout,"BIP");
    else if (EQUAL(value,"(SAMPLE,BAND,LINE)") || EQUAL(value,"") )
        strcpy(szLayout,"BSQ");
    else {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "%s layout not supported. Abort\n\n", value);
        delete poDS;
        return NULL;
    }

    /***********   Grab samples lines band ************/
    const int nCols = atoi(poDS->GetKeywordSub("QUBE.CORE_ITEMS",1));
    const int nRows = atoi(poDS->GetKeywordSub("QUBE.CORE_ITEMS",2));
    const int nBands = atoi(poDS->GetKeywordSub("QUBE.CORE_ITEMS",3));

    /***********   Grab Qube record bytes  **********/
    const int record_bytes = atoi(poDS->GetKeyword("RECORD_BYTES"));

    GUIntBig nSkipBytes = 0;
    if (nQube > 0 && bByteLocation )
        nSkipBytes = (nQube - 1);
    else if( nQube > 0 )
        nSkipBytes = (nQube - 1) * record_bytes;
    else
        nSkipBytes = 0;

    /***********   Grab samples lines band ************/
    char chByteOrder = 'M';  //default to MSB
    CPLString osCoreItemType = poDS->GetKeyword( "QUBE.CORE_ITEM_TYPE" );
    if( (EQUAL(osCoreItemType,"PC_INTEGER")) ||
        (EQUAL(osCoreItemType,"PC_UNSIGNED_INTEGER")) ||
        (EQUAL(osCoreItemType,"PC_REAL")) ) {
        chByteOrder = 'I';
    }

    /********   Grab format type - isis2 only supports 8,16,32 *******/
    GDALDataType eDataType = GDT_Byte;
    bool bNoDataSet = false;
    double dfNoData = 0.0;

    int itype = atoi(poDS->GetKeyword("QUBE.CORE_ITEM_BYTES",""));
    switch(itype) {
      case 1 :
        eDataType = GDT_Byte;
        dfNoData = NULL1;
        bNoDataSet = true;
        break;
      case 2 :
        if( strstr(osCoreItemType,"UNSIGNED") != NULL )
        {
            dfNoData = 0;
            eDataType = GDT_UInt16;
        }
        else
        {
            dfNoData = NULL2;
            eDataType = GDT_Int16;
        }
        bNoDataSet = true;
        break;
      case 4 :
        eDataType = GDT_Float32;
        dfNoData = NULL3;
        bNoDataSet = true;
        break;
      case 8 :
        eDataType = GDT_Float64;
        dfNoData = NULL3;
        bNoDataSet = true;
        break;
      default :
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Itype of %d is not supported in ISIS 2.",
                  itype);
        delete poDS;
        return NULL;
    }

    /***********   Grab Cellsize ************/
    double dfXDim = 1.0;
    double dfYDim = 1.0;

    value = poDS->GetKeyword("QUBE.IMAGE_MAP_PROJECTION.MAP_SCALE");
    if (strlen(value) > 0 ) {
        // Convert km to m
        dfXDim = static_cast<float>( CPLAtof(value) * 1000.0 );
        dfYDim = static_cast<float>( CPLAtof(value) * 1000.0 * -1 );
    }

    /***********   Grab LINE_PROJECTION_OFFSET ************/
    double dfULYMap = 0.5;
    double yulcenter = 0.0;

    value = poDS->GetKeyword("QUBE.IMAGE_MAP_PROJECTION.LINE_PROJECTION_OFFSET");
    if (strlen(value) > 0) {
        yulcenter = static_cast<float>( CPLAtof(value) );
        yulcenter = ((yulcenter) * dfYDim);
        dfULYMap = yulcenter - (dfYDim/2);
    }

    /***********   Grab SAMPLE_PROJECTION_OFFSET ************/
    double dfULXMap = 0.5;
    double xulcenter = 0.0;

    value = poDS->GetKeyword("QUBE.IMAGE_MAP_PROJECTION.SAMPLE_PROJECTION_OFFSET");
    if( strlen(value) > 0 ) {
        xulcenter= static_cast<float>( CPLAtof(value) );
        xulcenter = ((xulcenter) * dfXDim);
        dfULXMap = xulcenter - (dfXDim/2);
    }

    /***********  Grab TARGET_NAME  ************/
    /**** This is the planets name i.e. MARS ***/
    CPLString target_name = poDS->GetKeyword("QUBE.TARGET_NAME");

    /***********   Grab MAP_PROJECTION_TYPE ************/
    CPLString map_proj_name =
        poDS->GetKeyword( "QUBE.IMAGE_MAP_PROJECTION.MAP_PROJECTION_TYPE");
    poDS->CleanString( map_proj_name );

    /***********   Grab SEMI-MAJOR ************/
    const double semi_major =
        CPLAtof(poDS->GetKeyword( "QUBE.IMAGE_MAP_PROJECTION.A_AXIS_RADIUS")) * 1000.0;

    /***********   Grab semi-minor ************/
    const double semi_minor =
        CPLAtof(poDS->GetKeyword( "QUBE.IMAGE_MAP_PROJECTION.C_AXIS_RADIUS")) * 1000.0;

    /***********   Grab CENTER_LAT ************/
    const double center_lat =
        CPLAtof(poDS->GetKeyword( "QUBE.IMAGE_MAP_PROJECTION.CENTER_LATITUDE"));

    /***********   Grab CENTER_LON ************/
    const double center_lon =
        CPLAtof(poDS->GetKeyword( "QUBE.IMAGE_MAP_PROJECTION.CENTER_LONGITUDE"));

    /***********   Grab 1st std parallel ************/
    const double first_std_parallel =
        CPLAtof(poDS->GetKeyword( "QUBE.IMAGE_MAP_PROJECTION.FIRST_STANDARD_PARALLEL"));

    /***********   Grab 2nd std parallel ************/
    const double second_std_parallel =
        CPLAtof(poDS->GetKeyword( "QUBE.IMAGE_MAP_PROJECTION.SECOND_STANDARD_PARALLEL"));

    /*** grab  PROJECTION_LATITUDE_TYPE = "PLANETOCENTRIC" ****/
    // Need to further study how ocentric/ographic will effect the gdal library.
    // So far we will use this fact to define a sphere or ellipse for some projections
    // Frank - may need to talk this over
    bool bIsGeographic = true;
    value = poDS->GetKeyword("CUBE.IMAGE_MAP_PROJECTION.PROJECTION_LATITUDE_TYPE");
    if (EQUAL( value, "\"PLANETOCENTRIC\"" ))
        bIsGeographic = false;

    CPLDebug("ISIS2","using projection %s", map_proj_name.c_str() );

    OGRSpatialReference oSRS;
    bool bProjectionSet = true;

    //Set oSRS projection and parameters
    if ((EQUAL( map_proj_name, "EQUIRECTANGULAR_CYLINDRICAL" )) ||
        (EQUAL( map_proj_name, "EQUIRECTANGULAR" )) ||
        (EQUAL( map_proj_name, "SIMPLE_CYLINDRICAL" )) ) {
        oSRS.OGRSpatialReference::SetEquirectangular2 ( 0.0, center_lon, center_lat, 0, 0 );
    } else if (EQUAL( map_proj_name, "ORTHOGRAPHIC" )) {
        oSRS.OGRSpatialReference::SetOrthographic ( center_lat, center_lon, 0, 0 );
    } else if ((EQUAL( map_proj_name, "SINUSOIDAL" )) ||
               (EQUAL( map_proj_name, "SINUSOIDAL_EQUAL-AREA" ))) {
        oSRS.OGRSpatialReference::SetSinusoidal ( center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "MERCATOR" )) {
        oSRS.OGRSpatialReference::SetMercator ( center_lat, center_lon, 1, 0, 0 );
    } else if (EQUAL( map_proj_name, "POLAR_STEREOGRAPHIC" )) {
        oSRS.OGRSpatialReference::SetPS ( center_lat, center_lon, 1, 0, 0 );
    } else if (EQUAL( map_proj_name, "TRANSVERSE_MERCATOR" )) {
        oSRS.OGRSpatialReference::SetTM ( center_lat, center_lon, 1, 0, 0 );
    } else if (EQUAL( map_proj_name, "LAMBERT_CONFORMAL_CONIC" )) {
        oSRS.OGRSpatialReference::SetLCC ( first_std_parallel, second_std_parallel, center_lat, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "") ) {
        /* no projection */
        bProjectionSet = false;
    } else {
        CPLDebug( "ISIS2",
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
        double iflattening = 0.0;
        if ((semi_major - semi_minor) < 0.0000001)
            iflattening = 0;
        else
            iflattening = semi_major / (semi_major - semi_minor);

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
                  (EQUAL( map_proj_name, "ORTHOGRAPHIC" )) ||
                  (EQUAL( map_proj_name, "STEREOGRAPHIC" )) ||
                  (EQUAL( map_proj_name, "SINUSOIDAL_EQUAL-AREA" )) ||
                  (EQUAL( map_proj_name, "SINUSOIDAL" ))  ) {
            // ISIS uses the spherical equation for these projections so force
            // a sphere.
            oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                            semi_major, 0.0,
                            "Reference_Meridian", 0.0 );
        }
        else if  ((EQUAL( map_proj_name, "EQUIRECTANGULAR_CYLINDRICAL" )) ||
                  (EQUAL( map_proj_name, "EQUIRECTANGULAR" )) ) {
            //Calculate localRadius using ISIS3 simple elliptical method
            //  not the more standard Radius of Curvature method
            //PI = 4 * atan(1);
            if (center_lon == 0) { //No need to calculate local radius
                oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                                semi_major, 0.0,
                                "Reference_Meridian", 0.0 );
            } else {
                const double radLat = center_lat * M_PI / 180;  // in radians
                const double localRadius
                    = semi_major * semi_minor
                    / sqrt(pow(semi_minor*cos(radLat),2)
                           + pow(semi_major*sin(radLat),2) );
                sphere_name += "_localRadius";
                oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                                localRadius, 0.0,
                                "Reference_Meridian", 0.0 );
                CPLDebug( "ISIS2", "local radius: %f", localRadius);
            }
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
        poDS->osProjection = pszResult;
        CPLFree( pszResult );
    }

/* END ISIS2 Label Read */
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/

/* -------------------------------------------------------------------- */
/*      Did we get the required keywords?  If not we return with        */
/*      this never having been considered to be a match. This isn't     */
/*      an error!                                                       */
/* -------------------------------------------------------------------- */
    if( nRows < 1 || nCols < 1 || nBands < 1 )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;

/* -------------------------------------------------------------------- */
/*      Open target binary file.                                        */
/* -------------------------------------------------------------------- */

    if( poOpenInfo->eAccess == GA_ReadOnly )
        poDS->fpImage = VSIFOpenL( osTargetFile, "rb" );
    else
        poDS->fpImage = VSIFOpenL( osTargetFile, "r+b" );

    if( poDS->fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Failed to open %s with write permission.\n%s",
                  osTargetFile.c_str(), VSIStrerror( errno ) );
        delete poDS;
        return NULL;
    }

    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Compute the line offset.                                        */
/* -------------------------------------------------------------------- */
    int nItemSize = GDALGetDataTypeSizeBytes(eDataType);
    int nLineOffset, nPixelOffset;
    vsi_l_offset nBandOffset;

    if( EQUAL(szLayout,"BIP") )
    {
        nPixelOffset = nItemSize * nBands;
        if( nPixelOffset > INT_MAX / nBands )
        {
            delete poDS;
            return NULL;
        }
        nLineOffset = nPixelOffset * nCols;
        nBandOffset = nItemSize;
    }
    else if( EQUAL(szLayout,"BSQ") )
    {
        nPixelOffset = nItemSize;
        if( nPixelOffset > INT_MAX / nCols )
        {
            delete poDS;
            return NULL;
        }
        nLineOffset = nPixelOffset * nCols;
        nBandOffset = static_cast<vsi_l_offset>(nLineOffset) * nRows;
    }
    else /* assume BIL */
    {
        nPixelOffset = nItemSize;
        if( nPixelOffset > INT_MAX / nBands ||
            nPixelOffset * nBands > INT_MAX / nCols )
        {
            delete poDS;
            return NULL;
        }
        nLineOffset = nItemSize * nBands * nCols;
        nBandOffset = static_cast<vsi_l_offset>(nItemSize) * nCols;
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    poDS->nBands = nBands;
    for( int i = 0; i < poDS->nBands; i++ )
    {
        RawRasterBand *poBand =
            new RawRasterBand( poDS, i+1, poDS->fpImage,
                               nSkipBytes + nBandOffset * i,
                               nPixelOffset, nLineOffset, eDataType,
#ifdef CPL_LSB
                               chByteOrder == 'I' || chByteOrder == 'L',
#else
                               chByteOrder == 'M',
#endif
                               TRUE );

        if( bNoDataSet )
            poBand->SetNoDataValue( dfNoData );

        poDS->SetBand( i+1, poBand );

        // Set offset/scale values at the PAM level.
        poBand->SetOffset(
            CPLAtofM(poDS->GetKeyword("QUBE.CORE_BASE","0.0")));
        poBand->SetScale(
            CPLAtofM(poDS->GetKeyword("QUBE.CORE_MULTIPLIER","1.0")));
    }

/* -------------------------------------------------------------------- */
/*      Check for a .prj file. For isis2 I would like to keep this in   */
/* -------------------------------------------------------------------- */
    const CPLString osPath = CPLGetPath( poOpenInfo->pszFilename );
    const CPLString osName = CPLGetBasename(poOpenInfo->pszFilename);
    const char  *pszPrjFile = CPLFormCIFilename( osPath, osName, "prj" );

    VSILFILE *fp = VSIFOpenL( pszPrjFile, "r" );
    if( fp != NULL )
    {
        VSIFCloseL( fp );

        char **papszLines = CSLLoad( pszPrjFile );

        OGRSpatialReference oSRS2;
        if( oSRS2.importFromESRI( papszLines ) == OGRERR_NONE )
        {
            char *pszResult = NULL;
            oSRS2.exportToWkt( &pszResult );
            poDS->osProjection = pszResult;
            CPLFree( pszResult );
        }

        CSLDestroy( papszLines );
    }

    if( dfULXMap != 0.5 || dfULYMap != 0.5 || dfXDim != 1.0 || dfYDim != 1.0 )
    {
        poDS->bGotTransform = TRUE;
        poDS->adfGeoTransform[0] = dfULXMap;
        poDS->adfGeoTransform[1] = dfXDim;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = dfULYMap;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = dfYDim;
    }

    if( !poDS->bGotTransform )
        poDS->bGotTransform =
            GDALReadWorldFile( poOpenInfo->pszFilename, "cbw",
                               poDS->adfGeoTransform );

    if( !poDS->bGotTransform )
        poDS->bGotTransform =
            GDALReadWorldFile( poOpenInfo->pszFilename, "wld",
                               poDS->adfGeoTransform );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
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

const char *ISIS2Dataset::GetKeyword( const char *pszPath,
                                      const char *pszDefault )

{
    return oKeywords.GetKeyword( pszPath, pszDefault );
}

/************************************************************************/
/*                            GetKeywordSub()                           */
/************************************************************************/

const char *ISIS2Dataset::GetKeywordSub( const char *pszPath,
                                         int iSubscript,
                                         const char *pszDefault )

{
    const char *pszResult = oKeywords.GetKeyword( pszPath, NULL );

    if( pszResult == NULL )
        return pszDefault;

    if( pszResult[0] != '(' )
        return pszDefault;

    char **papszTokens = CSLTokenizeString2( pszResult, "(,)",
                                             CSLT_HONOURSTRINGS );

    if( iSubscript <= CSLCount(papszTokens) )
    {
        oTempResult = papszTokens[iSubscript-1];
        CSLDestroy( papszTokens );
        return oTempResult.c_str();
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

void ISIS2Dataset::CleanString( CPLString &osInput )

{
   if(  ( osInput.size() < 2 ) ||
        ((osInput.at(0) != '"'   || osInput.back() != '"' ) &&
        ( osInput.at(0) != '\'' || osInput.back() != '\'')) )
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
/*                           Create()                                   */
/************************************************************************/
/**
 * Hidden Creation Options:
 * INTERLEAVE=BSQ/BIP/BIL: Force the generation specified type of interleaving.
 *  BSQ --- band sequental (default),
 *  BIP --- band interleaved by pixel,
 *  BIL --- band interleaved by line.
 * OBJECT=QUBE/IMAGE/SPECTRAL_QUBE, if null default is QUBE
 */

GDALDataset *ISIS2Dataset::Create(const char* pszFilename,
                                  int nXSize, int nYSize, int nBands,
                                  GDALDataType eType, char** papszParmList) {

    /* Verify settings. In Isis 2 core pixel values can be represented in
     * three different ways : 1, 2 4, or 8 Bytes */
    if( eType != GDT_Byte && eType != GDT_Int16 && eType != GDT_Float32
        && eType != GDT_UInt16 && eType != GDT_Float64 ){
        CPLError(CE_Failure, CPLE_AppDefined,
                 "The ISIS2 driver does not supporting creating files of type %s.",
                 GDALGetDataTypeName( eType ) );
        return NULL;
    }

    /*  (SAMPLE, LINE, BAND) - Band Sequential (BSQ) - default choice
        (SAMPLE, BAND, LINE) - Band Interleaved by Line (BIL)
        (BAND, SAMPLE, LINE) - Band Interleaved by Pixel (BIP) */
    const char *pszInterleaving = "(SAMPLE,LINE,BAND)";
    const char *pszInterleavingParam = CSLFetchNameValue( papszParmList, "INTERLEAVE" );
    if ( pszInterleavingParam ) {
        if ( STARTS_WITH_CI(pszInterleavingParam, "bip") )
            pszInterleaving = "(BAND,SAMPLE,LINE)";
        else if ( STARTS_WITH_CI(pszInterleavingParam, "bil") )
            pszInterleaving = "(SAMPLE,BAND,LINE)";
        else
            pszInterleaving = "(SAMPLE,LINE,BAND)";
    }

    /* default labeling method is attached */
    bool bAttachedLabelingMethod = true;
    /* check if labeling method is set : check the all three first chars */
    const char *pszLabelingMethod = CSLFetchNameValue( papszParmList, "LABELING_METHOD" );
    if ( pszLabelingMethod ){
        if ( STARTS_WITH_CI( pszLabelingMethod, "det" /* "detached" */ ) ){
            bAttachedLabelingMethod = false;
        }
        if ( STARTS_WITH_CI( pszLabelingMethod, "att" /* attached" */ ) ){
            bAttachedLabelingMethod = true;
        }
    }

    /*  set the label and data files */
    CPLString osLabelFile, osRasterFile, osOutFile;
    if( bAttachedLabelingMethod ) {
        osLabelFile = "";
        osRasterFile = pszFilename;
        osOutFile = osRasterFile;
    }
    else
    {
        CPLString sExtension = "cub";
        const char* pszExtension = CSLFetchNameValue( papszParmList, "IMAGE_EXTENSION" );
        if( pszExtension ){
            sExtension = pszExtension;
        }

        if( EQUAL(CPLGetExtension( pszFilename ), sExtension) )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "IMAGE_EXTENSION (%s) cannot match LABEL file extension.",
                      sExtension.c_str() );
            return NULL;
        }

        osLabelFile = pszFilename;
        osRasterFile = CPLResetExtension( osLabelFile, sExtension );
        osOutFile = osLabelFile;
    }

    const char *pszObject = CSLFetchNameValue( papszParmList, "OBJECT" );
    CPLString sObject = "QUBE"; // default choice
    if (pszObject) {
        if ( EQUAL( pszObject, "IMAGE") ){
            sObject = "IMAGE";
        }
        if ( EQUAL( pszObject, "SPECTRAL_QUBE")){
            sObject = "SPECTRAL_QUBE";
        }
    }

    GUIntBig iRecords = ISIS2Dataset::RecordSizeCalculation(nXSize, nYSize, nBands, eType);
    GUIntBig iLabelRecords(2);

    CPLDebug("ISIS2","irecord = %i",static_cast<int>(iRecords));

    if( bAttachedLabelingMethod )
    {
        ISIS2Dataset::WriteLabel(osRasterFile, "", sObject, nXSize, nYSize, nBands, eType, iRecords, pszInterleaving, iLabelRecords, true);
    }
    else
    {
        ISIS2Dataset::WriteLabel(osLabelFile, osRasterFile, sObject, nXSize, nYSize, nBands, eType, iRecords, pszInterleaving, iLabelRecords);
    }

    if( !ISIS2Dataset::WriteRaster(osRasterFile, bAttachedLabelingMethod,
                                   iRecords, iLabelRecords, eType,
                                   pszInterleaving) )
        return NULL;

    return reinterpret_cast<GDALDataset *>( GDALOpen( osOutFile, GA_Update ) );
}

/************************************************************************/
/*                            WriteRaster()                             */
/************************************************************************/

int ISIS2Dataset::WriteRaster(CPLString osFilename,
                              bool includeLabel,
                              GUIntBig iRecords,
                              GUIntBig iLabelRecords,
                              CPL_UNUSED GDALDataType eType,
                              CPL_UNUSED const char * pszInterleaving)
{
    CPLString pszAccess("wb");
    if(includeLabel)
        pszAccess = "ab";

    VSILFILE *fpBin = VSIFOpenL( osFilename, pszAccess.c_str() );
    if( fpBin == NULL ) {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to create %s:\n%s",
                  osFilename.c_str(), VSIStrerror( errno ) );
        return FALSE;
    }

    GUIntBig nSize = iRecords * RECORD_SIZE;
    CPLDebug("ISIS2","nSize = %i", static_cast<int>(nSize));

    if(includeLabel)
        nSize = iLabelRecords * RECORD_SIZE + nSize;

    // write last byte
    const GByte byZero(0);
    if(VSIFSeekL( fpBin, nSize-1, SEEK_SET ) != 0 ||
       VSIFWriteL( &byZero, 1, 1, fpBin ) != 1) {
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to write %s:\n%s",
                  osFilename.c_str(), VSIStrerror( errno ) );
        VSIFCloseL( fpBin );
        return FALSE;
    }
    VSIFCloseL( fpBin );

    return TRUE;
}

/************************************************************************/
/*                       RecordSizeCalculation()                        */
/************************************************************************/
GUIntBig ISIS2Dataset::RecordSizeCalculation(unsigned int nXSize, unsigned int nYSize, unsigned int nBands, GDALDataType eType )

{
    const GUIntBig n = static_cast<GUIntBig>(nXSize) * nYSize * nBands * (  GDALGetDataTypeSize(eType) / 8);
    // size of pds file is a multiple of RECORD_SIZE Bytes.
    CPLDebug("ISIS2","n = %i", static_cast<int>(n));
    CPLDebug("ISIS2","RECORD SIZE = %i", RECORD_SIZE);
    CPLDebug("ISIS2","nXSize = %i", nXSize);
    CPLDebug("ISIS2","nYSize = %i", nYSize);
    CPLDebug("ISIS2","nBands = %i", nBands);
    CPLDebug("ISIS2","DataTypeSize = %i", GDALGetDataTypeSize(eType));
    return static_cast<GUIntBig>( ceil(static_cast<float>( n ) / RECORD_SIZE) );
}

/************************************************************************/
/*                       WriteQUBE_Information()                        */
/************************************************************************/

int ISIS2Dataset::WriteQUBE_Information(
    VSILFILE *fpLabel, unsigned int iLevel, unsigned int & nWritingBytes,
    unsigned int nXSize,  unsigned int nYSize, unsigned int nBands,
    GDALDataType eType, const char * pszInterleaving)

{
    nWritingBytes += ISIS2Dataset::WriteFormatting( fpLabel, "");
    nWritingBytes += ISIS2Dataset::WriteFormatting( fpLabel, "/* Qube structure */");
    nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "OBJECT", "QUBE");
    iLevel++;
    nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "AXES", "3");
    nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "AXIS_NAME", pszInterleaving);
    nWritingBytes += ISIS2Dataset::WriteFormatting( fpLabel, "/* Core description */");

    CPLDebug("ISIS2","%d,%d,%d",nXSize,nYSize,nBands);

    nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "CORE_ITEMS",CPLString().Printf("(%d,%d,%d)",nXSize,nYSize,nBands));
    nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "CORE_NAME", "\"RAW DATA NUMBER\"");
    nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "CORE_UNIT", "\"N/A\"");
    // TODO change for eType

    if( eType == GDT_Byte )
    {
        nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "CORE_ITEM_TYPE", "PC_UNSIGNED_INTEGER");
        nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "CORE_ITEM_BYTES", "1");
    }
    else if( eType == GDT_UInt16 )
    {
        nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "CORE_ITEM_TYPE", "PC_UNSIGNED_INTEGER");
        nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "CORE_ITEM_BYTES", "2");
    }
    else if( eType == GDT_Int16 )
    {
        nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "CORE_ITEM_TYPE", "PC_INTEGER");
        nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "CORE_ITEM_BYTES", "2");
    }
    else if( eType == GDT_Float32 )
    {
        nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "CORE_ITEM_TYPE", "PC_REAL");
        nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "CORE_ITEM_BYTES", "4");
    }
    else if( eType == GDT_Float64 )
    {
        nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "CORE_ITEM_TYPE", "PC_REAL");
        nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "CORE_ITEM_BYTES", "8");
    }

    // TODO add core null value

    nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "CORE_BASE", "0.0");
    nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "CORE_MULTIPLIER", "1.0");
    nWritingBytes += ISIS2Dataset::WriteFormatting( fpLabel, "/* Suffix description */");
    nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "SUFFIX_BYTES", "4");
    nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "SUFFIX_ITEMS", "( 0, 0, 0)");
    iLevel--;
    nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "END_OBJECT", "QUBE");

    return TRUE;
}

/************************************************************************/
/*                             WriteLabel()                             */
/*                                                                      */
/*      osRasterFile : name of raster file but if it is empty we        */
/*                     have only one file with an attached label        */
/*      sObjectTag : QUBE, IMAGE or SPECTRAL_QUBE                       */
/*      bRelaunch : flag to allow recursive call                        */
/************************************************************************/

int ISIS2Dataset::WriteLabel(
    CPLString osFilename, CPLString osRasterFile, CPLString sObjectTag,
    unsigned int nXSize, unsigned int nYSize, unsigned int nBands,
    GDALDataType eType,
    GUIntBig iRecords, const char * pszInterleaving,
    GUIntBig &iLabelRecords,
    CPL_UNUSED bool bRelaunch)
{
    CPLDebug("ISIS2", "Write Label filename = %s, rasterfile = %s",osFilename.c_str(),osRasterFile.c_str());
    bool bAttachedLabel = EQUAL(osRasterFile, "");

    VSILFILE *fpLabel = VSIFOpenL( osFilename, "w" );

    if( fpLabel == NULL ){
        CPLError( CE_Failure, CPLE_FileIO,
                  "Failed to create %s:\n%s",
                  osFilename.c_str(), VSIStrerror( errno ) );
        return FALSE;
    }

    const unsigned int iLevel(0);
    unsigned int nWritingBytes(0);

    /* write common header */
    nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "PDS_VERSION_ID", "PDS3" );
    nWritingBytes += ISIS2Dataset::WriteFormatting( fpLabel, "");
    nWritingBytes += ISIS2Dataset::WriteFormatting( fpLabel, "/* File identification and structure */");
    nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "RECORD_TYPE", "FIXED_LENGTH" );
    nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "RECORD_BYTES", CPLString().Printf("%d",RECORD_SIZE));
    nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "FILE_RECORDS", CPLString().Printf(CPL_FRMT_GUIB,iRecords));
    nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "LABEL_RECORDS", CPLString().Printf(CPL_FRMT_GUIB,iLabelRecords));
    if(!bAttachedLabel){
        nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, "FILE_NAME", CPLGetFilename(osRasterFile));
    }
    nWritingBytes += ISIS2Dataset::WriteFormatting( fpLabel, "");

    nWritingBytes += ISIS2Dataset::WriteFormatting( fpLabel, "/* Pointers to Data Objects */");

    if(bAttachedLabel){
        nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, CPLString().Printf("^%s",sObjectTag.c_str()), CPLString().Printf(CPL_FRMT_GUIB,iLabelRecords+1));
    }else{
        nWritingBytes += ISIS2Dataset::WriteKeyword( fpLabel, iLevel, CPLString().Printf("^%s",sObjectTag.c_str()), CPLString().Printf("(\"%s\",1)",CPLGetFilename(osRasterFile)));
    }

    if(EQUAL(sObjectTag, "QUBE")){
        ISIS2Dataset::WriteQUBE_Information(fpLabel, iLevel, nWritingBytes, nXSize, nYSize, nBands, eType, pszInterleaving);
    }

    nWritingBytes += ISIS2Dataset::WriteFormatting( fpLabel, "END");

    // check if file record is correct
    const unsigned int q = nWritingBytes/RECORD_SIZE;
    if( q <= iLabelRecords){
        // correct we add space after the label end for complete from iLabelRecords
        unsigned int nSpaceBytesToWrite = static_cast<unsigned int>(
            iLabelRecords * RECORD_SIZE - nWritingBytes );
        VSIFPrintfL(fpLabel,"%*c", nSpaceBytesToWrite, ' ');
    }else{
        iLabelRecords = q+1;
        ISIS2Dataset::WriteLabel(osFilename, osRasterFile, sObjectTag, nXSize, nYSize, nBands, eType, iRecords, pszInterleaving, iLabelRecords);
    }
    VSIFCloseL( fpLabel );

    return TRUE;
}

/************************************************************************/
/*                            WriteKeyword()                            */
/************************************************************************/

    unsigned int ISIS2Dataset::WriteKeyword(
        VSILFILE *fpLabel, unsigned int iLevel, CPLString key, CPLString value)

    {
        CPLString tab = "";
        iLevel *= 4; // each struct is indented by 4 spaces.

        return VSIFPrintfL(fpLabel,"%*s%s=%s\n", iLevel, tab.c_str(),
                           key.c_str(), value.c_str());
    }

/************************************************************************/
/*                          WriteFormatting()                           */
/************************************************************************/

    unsigned int ISIS2Dataset::WriteFormatting(VSILFILE *fpLabel, CPLString data)

    {
        return VSIFPrintfL(fpLabel,"%s\n", data.c_str());
    }

/************************************************************************/
/*                         GDALRegister_ISIS2()                         */
/************************************************************************/

void GDALRegister_ISIS2()

{
    if( GDALGetDriverByName( "ISIS2" ) != NULL )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "ISIS2" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "USGS Astrogeology ISIS cube (Version 2)" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_isis2.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Int16 UInt16 Float32 Float64");

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
"<CreationOptionList>\n"
"   <Option name='LABELING_METHOD' type='string-select' default='ATTACHED'>\n"
"     <Value>ATTACHED</Value>"
"     <Value>DETACHED</Value>"
"   </Option>"
"   <Option name='IMAGE_EXTENSION' type='string' default='cub'/>\n"
"</CreationOptionList>\n" );

    poDriver->pfnIdentify = ISIS2Dataset::Identify;
    poDriver->pfnOpen = ISIS2Dataset::Open;
    poDriver->pfnCreate = ISIS2Dataset::Create;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
