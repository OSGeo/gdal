/******************************************************************************
 * $Id$
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
#define NULL1 0
#define NULL2 -32768
//#define NULL3 -0.3402822655089E+39
//Same as ESRI_GRID_FLOAT_NO_DATA
//#define NULL3 -340282346638528859811704183484516925440.0
#define NULL3 -3.4028226550889044521e+38

#include "rawdataset.h"
#include "ogr_spatialref.h"
#include "cpl_string.h" 
#include "nasakeywordhandler.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_PDS(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*			       PDSDataset	                        */
/* ==================================================================== */
/************************************************************************/

class PDSDataset : public RawDataset
{
    FILE	*fpImage;	// image data file.

    NASAKeywordHandler  oKeywords;
  
    int         bGotTransform;
    double      adfGeoTransform[6];
  
    CPLString   osProjection;

    //functions replaced by "NASAKeywordHandler" - ReadWord, ReadPair, ReadGroup
    //int parse_label(const char *file, char *keyword, char *value);
    //int strstrip(char instr[], char outstr[], int position);

    CPLString   osTempResult;

    void        CleanString( CPLString &osInput );

    const char *GetKeyword( const char *pszPath, 
                            const char *pszDefault = "");
    const char *GetKeywordSub( const char *pszPath, 
                               int iSubscript, 
                               const char *pszDefault = "");
    const char *GetKeywordUnit( const char *pszPath, 
                               int iSubscript, 
                               const char *pszDefault = "");

public:
    PDSDataset();
    ~PDSDataset();
  
    virtual CPLErr GetGeoTransform( double * padfTransform );
    virtual const char *GetProjectionRef(void);
  
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
};

/************************************************************************/
/*                            PDSDataset()                            */
/************************************************************************/

PDSDataset::PDSDataset()
{
    fpImage = NULL;
    bGotTransform = FALSE;
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
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *PDSDataset::GetProjectionRef()

{
    if( strlen(osProjection) > 0 )
        return osProjection;
    else
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
    else
    {
        return GDALPamDataset::GetGeoTransform( padfTransform );
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *PDSDataset::Open( GDALOpenInfo * poOpenInfo )
{
/* -------------------------------------------------------------------- */
/*      Does this look like a PDS Img dataset?                          */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->pabyHeader == NULL )
        return NULL;

    if ((strstr((const char *)poOpenInfo->pabyHeader,"^IMAGE") != NULL ) &&
        (strstr((const char *)poOpenInfo->pabyHeader,"PDS3") == NULL ))
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "It appears this is an older PDS image type.  Only PDS_VERSION_ID = PDS3 are currently supported by this gdal PDS reader.");
        return NULL;
    }

    if( strstr((const char *)poOpenInfo->pabyHeader,"^IMAGE") == NULL )
        return NULL;
  
/* -------------------------------------------------------------------- */
/*      Open the file using the large file API.                         */
/* -------------------------------------------------------------------- */
    FILE *fpQube = VSIFOpenL( poOpenInfo->pszFilename, "rb" );

    if( fpQube == NULL )
        return NULL;

    PDSDataset 	*poDS;

    poDS = new PDSDataset();

    const char* pszPDSVersionID = strstr((const char *)poOpenInfo->pabyHeader,"PDS_VERSION_ID");
    int nOffset = 0;
    if (pszPDSVersionID)
        nOffset = pszPDSVersionID - (const char *)poOpenInfo->pabyHeader;

    if( ! poDS->oKeywords.Ingest( fpQube, nOffset ) )
    {
        delete poDS;
        return NULL;
    }
    VSIFCloseL( fpQube );

/* ------------------------------------------------------------------- */
/*	We assume the user is pointing to the label (ie. .lbl) file.  	   */
/* ------------------------------------------------------------------- */
    // IMAGE can be inline or detached and point to an image name
    // ^IMAGE = 3
    // ^IMAGE                         = "GLOBAL_ALBEDO_8PPD.IMG"
    // ^IMAGE                         = "MEGT90N000CB.IMG"
    // ^SPECTRAL_QUBE = 5  for multi-band images

    const char *pszQube = poDS->GetKeyword( "^IMAGE", "" );
    CPLString osTargetFile = poOpenInfo->pszFilename;

    if (EQUAL(pszQube,"")) {
        pszQube = poDS->GetKeyword( "^SPECTRAL_QUBE" );
    }
    int nQube = atoi(pszQube);

    if( pszQube[0] == '"' )
    {
        CPLString osTPath = CPLGetPath(poOpenInfo->pszFilename);
        CPLString osFilename = pszQube;
        poDS->CleanString( osFilename );
        osTargetFile = CPLFormCIFilename( osTPath, osFilename, NULL );
    }
    else if( pszQube[0] == '(' )
    {
        CPLAssert( FALSE ); // TODO
    }

    GDALDataType eDataType = GDT_Byte;
    OGRSpatialReference oSRS;

    
    //image parameters
    int	nRows, nCols, nBands = 1;
    int nSkipBytes = 0;
    int itype;
    int record_bytes;
    int	bNoDataSet = FALSE;
    char chByteOrder = 'M';  //default to MSB
 
    //Georef parameters
    double dfULXMap=0.5;
    double dfULYMap = 0.5;
    double dfXDim = 1.0;
    double dfYDim = 1.0;
    double dfNoData = 0.0;
    double xulcenter = 0.0;
    double yulcenter = 0.0;

    //projection parameters
    int	bProjectionSet = TRUE;
    double semi_major = 0.0;
    double semi_minor = 0.0;
    double iflattening = 0.0;
    float center_lat = 0.0;
    float center_lon = 0.0;
    float first_std_parallel = 0.0;
    float second_std_parallel = 0.0;
    FILE	*fp;
    
    /* -------------------------------------------------------------------- */
    /*      Checks to see if this is raw PDS image not compressed image     */
    /*      so ENCODING_TYPE either does not exist or it equals "N/A".      */
    /*      Compressed types will not be supported in this routine          */
    /* -------------------------------------------------------------------- */
    const char *value;

    value = poDS->GetKeyword( "IMAGE.ENCODING_TYPE", "N/A" );
    if ( !(EQUAL(value,"N/A") ) )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "*** PDS image file has an ENCODING_TYPE parameter:\n"
                  "*** gdal pds driver does not support compressed image types\n"
                  "found: (%s)\n\n", value );
        delete poDS;
        return NULL;
    } 
    /**************** end ENCODING_TYPE check ***********************/
    
    
    /***********   Grab layout type (BSQ, BIP, BIL) ************/
    //  AXIS_NAME = (SAMPLE,LINE,BAND)
    /***********   Grab samples lines band        **************/
    /** if AXIS_NAME = "" then Bands=1 and Sample and Lines   **/
    /** are there own keywords  "LINES" and "LINE_SAMPLES"    **/
    /** if not NULL then CORE_ITEMS keyword i.e. (234,322,2)  **/
    /***********************************************************/
    char szLayout[10] = "BSQ"; //default to band seq.
    value = poDS->GetKeyword( "IMAGE.AXIS_NAME", "" );
    if (EQUAL(value,"(SAMPLE,LINE,BAND)") ) {
        strcpy(szLayout,"BSQ");
        nCols = atoi(poDS->GetKeywordSub("IMAGE.CORE_ITEMS",1));
        nRows = atoi(poDS->GetKeywordSub("IMAGE.CORE_ITEMS",2));
        nBands = atoi(poDS->GetKeywordSub("IMAGE.CORE_ITEMS",3));
    }
    else if (EQUAL(value,"(BAND,LINE,SAMPLE)") ) {
        strcpy(szLayout,"BIP");
        nBands = atoi(poDS->GetKeywordSub("IMAGE.CORE_ITEMS",1));
        nRows = atoi(poDS->GetKeywordSub("IMAGE.CORE_ITEMS",2));
        nCols = atoi(poDS->GetKeywordSub("IMAGE.CORE_ITEMS",3));
    }
    else if (EQUAL(value,"(SAMPLE,BAND,LINE)") ) {
        strcpy(szLayout,"BIL");
        nCols = atoi(poDS->GetKeywordSub("IMAGE.CORE_ITEMS",1));
        nBands = atoi(poDS->GetKeywordSub("IMAGE.CORE_ITEMS",2));
        nRows = atoi(poDS->GetKeywordSub("IMAGE.CORE_ITEMS",3));
    }
    else if ( EQUAL(value,"") ) {
        strcpy(szLayout,"BSQ");
        nCols = atoi(poDS->GetKeyword("IMAGE.LINE_SAMPLES",""));
        nRows = atoi(poDS->GetKeyword("IMAGE.LINES",""));
        nBands = atoi(poDS->GetKeyword("IMAGE.BANDS","1"));        
    }
    else {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "%s layout not supported. Abort\n\n", value);
        delete poDS;
        return NULL;
    }
    
    /***********   Grab Qube record bytes  **********/
    record_bytes = atoi(poDS->GetKeyword("IMAGE.RECORD_BYTES"));
    if (record_bytes == 0)
        record_bytes = atoi(poDS->GetKeyword("RECORD_BYTES"));

    if (nQube > 0)
        nSkipBytes = (nQube - 1) * record_bytes;     
    else
        nSkipBytes = 0;     
    
    /**** Grab format type - pds supports 1,2,4,8,16,32,64 (in theory) **/
    /**** I have only seen 8, 16, 32 (float) in released datasets      **/
    itype = atoi(poDS->GetKeyword("IMAGE.SAMPLE_BITS",""));
    switch(itype) {
      case 8 :
        eDataType = GDT_Byte;
        dfNoData = NULL1;
        bNoDataSet = TRUE;
        break;
      case 16 :
        eDataType = GDT_Int16;
        dfNoData = NULL2;
        bNoDataSet = TRUE;
        break;
      case 32 :
        eDataType = GDT_Float32;
        dfNoData = NULL3;
        bNoDataSet = TRUE;
        break;
      case 64 :
        eDataType = GDT_Float64;
        dfNoData = NULL3;
        bNoDataSet = TRUE;
        break;
      default :
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Sample_bits of %d is not supported in this gdal PDS reader.",
                  itype); 
        delete poDS;
        return NULL;
    }

    /***********   Grab SAMPLE_TYPE *****************/
    /** if keyword not found leave as "M" or "MSB" **/
    value = poDS->GetKeyword( "IMAGE.SAMPLE_TYPE" );
    if( (EQUAL(value,"LSB_INTEGER")) || 
        (EQUAL(value,"LSB")) || // just incase
        (EQUAL(value,"LSB_UNSIGNED_INTEGER")) || 
        (EQUAL(value,"LSB_SIGNED_INTEGER")) || 
        (EQUAL(value,"UNSIGNED_INTEGER")) || 
        (EQUAL(value,"VAX_REAL")) || 
        (EQUAL(value,"VAX_INTEGER")) || 
        (EQUAL(value,"PC_INTEGER")) ||  //just incase 
        (EQUAL(value,"PC_REAL")) ) {
        chByteOrder = 'I';
    }
    
    /***********   Grab Cellsize ************/
    //example:
    //MAP_SCALE   = 14.818 <KM/PIXEL>
    //added search for unit (only checks for CM, KM - defaults to Meters)
    value = poDS->GetKeyword("IMAGE_MAP_PROJECTION.MAP_SCALE");
    if (strlen(value) > 0 ) {
        dfXDim = (float) atof(value);
        dfYDim = (float) atof(value) * -1;
        
        CPLString unit = poDS->GetKeywordUnit("IMAGE_MAP_PROJECTION.MAP_SCALE",2); //KM
        //value = poDS->GetKeywordUnit("IMAGE_MAP_PROJECTION.MAP_SCALE",3); //PIXEL
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
    
    // Calculate upper left corner of pixel in meters from the upper left center pixel which
    // should be correct for what is documented in the PDS manual
    // It doesn't mean it will work perfectly for every PDS image, as they tend to be released in different ways.
    // both dfULYMap, dfULXMap were update October 11, 2007 to correct 0.5 cellsize offset
    /***********   Grab LINE_PROJECTION_OFFSET ************/
    value = poDS->GetKeyword("IMAGE_MAP_PROJECTION.LINE_PROJECTION_OFFSET");
    if (strlen(value) > 0) {
        yulcenter = (float) atof(value);
        dfULYMap = ((yulcenter - 0.5) * dfYDim * -1); 
        //notice dfYDim is negative here which is why it is negated again
    }
    /***********   Grab SAMPLE_PROJECTION_OFFSET ************/
    value = poDS->GetKeyword("IMAGE_MAP_PROJECTION.SAMPLE_PROJECTION_OFFSET");
    if( strlen(value) > 0 ) {
        xulcenter = (float) atof(value);
        dfULXMap = ((xulcenter - 0.5) * dfXDim * -1);
    }
     
    /***********  Grab TARGET_NAME  ************/
    /**** This is the planets name i.e. MARS ***/
    CPLString target_name = poDS->GetKeyword("TARGET_NAME");
     
    /**********   Grab MAP_PROJECTION_TYPE *****/
    CPLString map_proj_name = 
        poDS->GetKeyword( "IMAGE_MAP_PROJECTION.MAP_PROJECTION_TYPE");
    poDS->CleanString( map_proj_name );
     
    /******  Grab semi_major & convert to KM ******/
    semi_major = 
        atof(poDS->GetKeyword( "IMAGE_MAP_PROJECTION.A_AXIS_RADIUS")) * 1000.0;
    
    /******  Grab semi-minor & convert to KM ******/
    semi_minor = 
        atof(poDS->GetKeyword( "IMAGE_MAP_PROJECTION.C_AXIS_RADIUS")) * 1000.0;

    /***********   Grab CENTER_LAT ************/
    center_lat = 
        atof(poDS->GetKeyword( "IMAGE_MAP_PROJECTION.CENTER_LATITUDE"));

    /***********   Grab CENTER_LON ************/
    center_lon = 
        atof(poDS->GetKeyword( "IMAGE_MAP_PROJECTION.CENTER_LONGITUDE"));

    /**********   Grab 1st std parallel *******/
    first_std_parallel = 
        atof(poDS->GetKeyword( "IMAGE_MAP_PROJECTION.FIRST_STANDARD_PARALLEL"));

    /**********   Grab 2nd std parallel *******/
    second_std_parallel = 
        atof(poDS->GetKeyword( "IMAGE_MAP_PROJECTION.SECOND_STANDARD_PARALLEL"));
     
    /*** grab  PROJECTION_LATITUDE_TYPE = "PLANETOCENTRIC" ****/
    // Need to further study how ocentric/ographic will effect the gdal library.
    // So far we will use this fact to define a sphere or ellipse for some projections
    // Frank - may need to talk this over
    char bIsGeographic = TRUE;
    value = poDS->GetKeyword("IMAGE_MAP_PROJECTION.COORDINATE_SYSTEM_NAME");
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
        CPLError( CE_Warning, CPLE_AppDefined,
                  "No projection define or supported! Are you sure this is a map projected image?" );
        bProjectionSet = FALSE;
    }

    if (bProjectionSet) {
        //Create projection name, i.e. MERCATOR MARS and set as ProjCS keyword
        CPLString proj_target_name = map_proj_name + " " + target_name;
        oSRS.SetProjCS(proj_target_name); //set ProjCS keyword
     
        //The geographic/geocentric name will be the same basic name as the body name
        //'GCS' = Geographic/Geocentric Coordinate System
        CPLString geog_name = "GCS_" + target_name;
        
        //The datum and sphere names will be the same basic name aas the planet
        CPLString datum_name = "D_" + target_name;
        CPLString sphere_name = target_name; // + "_IAU_IAG");  //Might not be IAU defined so don't add
          
        //calculate inverse flattening from major and minor axis: 1/f = a/(a-b)
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
        poDS->osProjection = pszResult;
        CPLFree( pszResult );
    }

/* END PDS Label Read */
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
    
/* -------------------------------------------------------------------- */
/*      Did we get the required keywords?  If not we return with        */
/*      this never having been considered to be a match. This isn't     */
/*      an error!                                                       */
/* -------------------------------------------------------------------- */
    if( nRows < 1 || nCols < 1 || nBands < 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "File %s appears to be a PDS file, but failed to find some required keywords.", 
                  poOpenInfo->pszFilename );
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
                  osTargetFile.c_str(),
                  VSIStrerror( errno ) );
        delete poDS;
        return NULL;
    }

    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Compute the line offset.                                        */
/* -------------------------------------------------------------------- */
    int     nItemSize = GDALGetDataTypeSize(eDataType)/8;
    int		nLineOffset, nPixelOffset, nBandOffset;
    
    if( EQUAL(szLayout,"BIP") )
    {
        nPixelOffset = nItemSize * nBands;
        nLineOffset = nPixelOffset * nCols;
        nBandOffset = nItemSize;
    }
    else if( EQUAL(szLayout,"BSQ") )
    {
        nPixelOffset = nItemSize;
        nLineOffset = nPixelOffset * nCols;
        nBandOffset = nLineOffset * nRows;
    }
    else /* assume BIL */
    {
        nPixelOffset = nItemSize;
        nLineOffset = nItemSize * nBands * nCols;
        nBandOffset = nItemSize * nCols;
    }
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int i;

    poDS->nBands = nBands;;
    for( i = 0; i < poDS->nBands; i++ )
    {
        RawRasterBand	*poBand;

        poBand = 
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
            CPLAtofM(poDS->GetKeyword("IMAGE.OFFSET","0.0")));
        poBand->SetScale( 
            CPLAtofM(poDS->GetKeyword("IMAGE.SCALING_FACTOR","1.0")));
    }

/* --------------------------------------------------------------------*/
/*      Check for a .prj file. For pds I would like to keep this in    */
/* --------------------------------------------------------------------*/
    {
        CPLString osPath, osName;

        osPath = CPLGetPath( poOpenInfo->pszFilename );
        osName = CPLGetBasename(poOpenInfo->pszFilename);
        const char  *pszPrjFile = CPLFormCIFilename( osPath, osName, "prj" );

        fp = VSIFOpen( pszPrjFile, "r" );
        if( fp != NULL )
        {
            char	**papszLines;
            OGRSpatialReference oSRS;

            VSIFClose( fp );
        
            papszLines = CSLLoad( pszPrjFile );

            if( oSRS.importFromESRI( papszLines ) == OGRERR_NONE )
            {
                char *pszResult = NULL;
                oSRS.exportToWkt( &pszResult );
                poDS->osProjection = pszResult;
                CPLFree( pszResult );
            }

            CSLDestroy( papszLines );
        }
    }
    
    if( dfULYMap != 0.5 || dfULYMap != 0.5 || dfXDim != 1.0 || dfYDim != 1.0 )
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
            GDALReadWorldFile( poOpenInfo->pszFilename, "psw", 
                               poDS->adfGeoTransform );

    if( !poDS->bGotTransform )
        poDS->bGotTransform = 
            GDALReadWorldFile( poOpenInfo->pszFilename, "wld", 
                               poDS->adfGeoTransform );

/* -------------------------------------------------------------------- */
/*      Transfer a few interesting keywords as metadata.                */
/* -------------------------------------------------------------------- */
    static const char *apszKeywords[] = 
        { "FILTER_NAME", "DATA_SET_ID", "PRODUCT_ID", 
          "PRODUCER_INSTITUTION_NAME", "PRODUCT_TYPE", "MISSION_NAME",
          "SPACECRAFT_NAME", "INSTRUMENT_NAME", "INSTRUMENT_ID", 
          "TARGET_NAME", "CENTER_FILTER_WAVELENGTH", "BANDWIDTH",
          "PRODUCT_CREATION_TIME", "NOTE",
          NULL };
    
    for( i = 0; apszKeywords[i] != NULL; i++ )
    {
        const char *pszKeywordValue = poDS->GetKeyword( apszKeywords[i] );

        if( pszKeywordValue != NULL )
            poDS->SetMetadataItem( apszKeywords[i], pszKeywordValue );
    }

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    return( poDS );
}

/************************************************************************/
/*                             GetKeyword()                             */
/************************************************************************/

const char *PDSDataset::GetKeyword( const char *pszPath, 
                                      const char *pszDefault )

{
    return oKeywords.GetKeyword( pszPath, pszDefault );
}

/************************************************************************/
/*                            GetKeywordSub()                           */
/************************************************************************/

const char *PDSDataset::GetKeywordSub( const char *pszPath, 
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
        osTempResult = papszTokens[iSubscript-1];
        CSLDestroy( papszTokens );
        return osTempResult.c_str();
    }
    else
    {
        CSLDestroy( papszTokens );
        return pszDefault;
    }
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
    else
    {
        CSLDestroy( papszTokens );
        return pszDefault;
    }
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
    int i;

    pszWrk[strlen(pszWrk)-1] = '\0';
    
    for( i = 0; pszWrk[i] != '\0'; i++ )
    {
        if( pszWrk[i] == ' ' )
            pszWrk[i] = '_';
    }

    osInput = pszWrk;
    CPLFree( pszWrk );
}
/************************************************************************/
/*                         GDALRegister_PDS()                         */
/************************************************************************/

void GDALRegister_PDS()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "PDS" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "PDS" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "NASA Planetary Data System" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#PDS" );

        poDriver->pfnOpen = PDSDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

