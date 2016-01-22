/******************************************************************************
 * $Id: isis3dataset.cpp 10646 2007-01-18 02:38:10Z warmerdam $
 *
 * Project:  ISIS Version 3 Driver
 * Purpose:  Implementation of ISIS3Dataset
 * Author:   Trent Hare (thare@usgs.gov)
 *           Frank Warmerdam (warmerdam@pobox.com)
 *
 * NOTE: Original code authored by Trent and placed in the public domain as 
 * per US government policy.  I have (within my rights) appropriated it and 
 * placed it under the following license.  This is not intended to diminish 
 * Trents contribution. 
 ******************************************************************************
 * Copyright (c) 2007, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2010, Even Rouault <even dot rouault at mines-paris dot org>
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

#define NULL1 0
#define NULL2 -32768
//#define NULL3 0xFF7FFFFB //in hex
//Same as ESRI_GRID_FLOAT_NO_DATA
//#define NULL3 -340282346638528859811704183484516925440.0
#define NULL3 -3.4028226550889044521e+38

#ifndef PI
#  define PI 3.1415926535897932384626433832795
#endif

#include "rawdataset.h"
#include "ogr_spatialref.h"
#include "cpl_string.h" 
#include "nasakeywordhandler.h"

CPL_CVSID("$Id: isis3dataset.cpp 10646 2007-09-18 02:38:10Z xxxx $");

CPL_C_START
void GDALRegister_ISIS3(void);
CPL_C_END

class ISIS3Dataset;

/************************************************************************/
/* ==================================================================== */
/*			       ISISTiledBand		                */
/* ==================================================================== */
/************************************************************************/

class ISISTiledBand : public GDALPamRasterBand
{
    VSILFILE      *fpVSIL;
    GIntBig   nFirstTileOffset;
    GIntBig   nXTileOffset;
    GIntBig   nYTileOffset;
    int       bNativeOrder;

  public:

                ISISTiledBand( GDALDataset *poDS, VSILFILE *fpVSIL,
                               int nBand, GDALDataType eDT,
                               int nTileXSize, int nTileYSize, 
                               GIntBig nFirstTileOffset, 
                               GIntBig nXTileOffset,
                               GIntBig nYTileOffset,
                               int bNativeOrder );
    virtual     ~ISISTiledBand() {}

    virtual CPLErr          IReadBlock( int, int, void * );
};

/************************************************************************/
/*                           ISISTiledBand()                            */
/************************************************************************/

ISISTiledBand::ISISTiledBand( GDALDataset *poDS, VSILFILE *fpVSIL,
                              int nBand, GDALDataType eDT,
                              int nTileXSize, int nTileYSize, 
                              GIntBig nFirstTileOffset, 
                              GIntBig nXTileOffset,
                              GIntBig nYTileOffset,
                              int bNativeOrder )

{
    this->poDS = poDS;
    this->nBand = nBand;
    this->fpVSIL = fpVSIL;
    this->bNativeOrder = bNativeOrder;
    eDataType = eDT;
    nBlockXSize = nTileXSize;
    nBlockYSize = nTileYSize;

    int nBlocksPerRow = 
            (poDS->GetRasterXSize() + nTileXSize - 1) / nTileXSize;
    int nBlocksPerColumn = 
            (poDS->GetRasterYSize() + nTileYSize - 1) / nTileYSize;

    if( nXTileOffset == 0 && nYTileOffset == 0 )
    {
        nXTileOffset = (GDALGetDataTypeSize(eDT)/8) * nTileXSize * nTileYSize;
        nYTileOffset = nXTileOffset * nBlocksPerRow;
    }

    this->nFirstTileOffset = nFirstTileOffset
        + (nBand-1) * nYTileOffset * nBlocksPerColumn;

    this->nXTileOffset = nXTileOffset;
    this->nYTileOffset = nYTileOffset;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ISISTiledBand::IReadBlock( int nXBlock, int nYBlock, void *pImage )

{
    GIntBig  nOffset = nFirstTileOffset + 
        nXBlock * nXTileOffset + nYBlock * nYTileOffset;
    size_t nBlockSize = 
        (GDALGetDataTypeSize(eDataType)/8) * nBlockXSize * nBlockYSize;

    if( VSIFSeekL( fpVSIL, nOffset, SEEK_SET ) != 0 )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Failed to seek to offset %d to read tile %d,%d.",
                  (int) nOffset, nXBlock, nYBlock );
        return CE_Failure;
    }

    if( VSIFReadL( pImage, 1, nBlockSize, fpVSIL ) != nBlockSize )
    {
        CPLError( CE_Failure, CPLE_FileIO, 
                  "Failed to read %d bytes for tile %d,%d.",
                  (int) nBlockSize, nXBlock, nYBlock );
        return CE_Failure;
    }

    if( !bNativeOrder )
        GDALSwapWords( pImage, GDALGetDataTypeSize(eDataType)/8, 
                       nBlockXSize*nBlockYSize, 
                       GDALGetDataTypeSize(eDataType)/8 );

    return CE_None;
}

/************************************************************************/
/* ==================================================================== */
/*			       ISISDataset		                */
/* ==================================================================== */
/************************************************************************/

class ISIS3Dataset : public RawDataset
{
    VSILFILE	*fpImage;	// image data file.

    CPLString   osExternalCube;
    
    NASAKeywordHandler  oKeywords;
  
    int         bGotTransform;
    double      adfGeoTransform[6];
  
    CPLString   osProjection;

    int parse_label(const char *file, char *keyword, char *value);
    int strstrip(char instr[], char outstr[], int position);

    CPLString   oTempResult;

    const char *GetKeyword( const char *pszPath, 
                            const char *pszDefault = "");
    const char *GetKeywordSub( const char *pszPath, 
                               int iSubscript, 
                               const char *pszDefault = "");
    
public:
    ISIS3Dataset();
    ~ISIS3Dataset();
  
    virtual CPLErr GetGeoTransform( double * padfTransform );
    virtual const char *GetProjectionRef(void);
  
    virtual char **GetFileList();

    static int          Identify( GDALOpenInfo * );
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
};



/************************************************************************/
/*                            ISIS3Dataset()                            */
/************************************************************************/

ISIS3Dataset::ISIS3Dataset()
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
/*                            ~ISIS3Dataset()                            */
/************************************************************************/

ISIS3Dataset::~ISIS3Dataset()

{
    FlushCache();
    if( fpImage != NULL )
        VSIFCloseL( fpImage );
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **ISIS3Dataset::GetFileList()

{
    char **papszFileList = NULL;

    papszFileList = GDALPamDataset::GetFileList();

    if( strlen(osExternalCube) > 0 )
        papszFileList = CSLAddString( papszFileList, osExternalCube );

    return papszFileList;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *ISIS3Dataset::GetProjectionRef()

{
    if( strlen(osProjection) > 0 )
        return osProjection;
    else
        return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ISIS3Dataset::GetGeoTransform( double * padfTransform )

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
/*                              Identify()                              */
/************************************************************************/
int ISIS3Dataset::Identify( GDALOpenInfo * poOpenInfo )
    
{
    if( poOpenInfo->pabyHeader != NULL
        && strstr((const char *)poOpenInfo->pabyHeader,"IsisCube") != NULL )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ISIS3Dataset::Open( GDALOpenInfo * poOpenInfo )
{
/* -------------------------------------------------------------------- */
/*      Does this look like a CUBE dataset?                             */
/* -------------------------------------------------------------------- */
    if( !Identify( poOpenInfo ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Open the file using the large file API.                         */
/* -------------------------------------------------------------------- */
    VSILFILE *fpQube = VSIFOpenL( poOpenInfo->pszFilename, "rb" );

    if( fpQube == NULL )
        return NULL;

    ISIS3Dataset 	*poDS;

    poDS = new ISIS3Dataset();

    if( ! poDS->oKeywords.Ingest( fpQube, 0 ) )
    {
        VSIFCloseL( fpQube );
        delete poDS;
        return NULL;
    }
    
    VSIFCloseL( fpQube );

/* -------------------------------------------------------------------- */
/*	Assume user is pointing to label (ie .lbl) file for detached option */
/* -------------------------------------------------------------------- */
    //  Image can be inline or detached and point to an image name
    //  the Format can be Tiled or Raw
    //  Object = Core
    //      StartByte   = 65537
    //      Format      = Tile
    //      TileSamples = 128
    //      TileLines   = 128
    //OR-----
    //  Object = Core
    //      StartByte = 1
    //      ^Core     = r0200357_detatched.cub
    //      Format    = BandSequential
    //OR-----
    //  Object = Core
    //      StartByte = 1
    //      ^Core     = r0200357_detached_tiled.cub
    //      Format      = Tile
    //      TileSamples = 128
    //      TileLines   = 128
    
/* -------------------------------------------------------------------- */
/*      What file contains the actual data?                             */
/* -------------------------------------------------------------------- */
    const char *pszCore = poDS->GetKeyword( "IsisCube.Core.^Core" );
    CPLString osQubeFile;

    if( EQUAL(pszCore,"") )
        osQubeFile = poOpenInfo->pszFilename;
    else
    {
        CPLString osPath = CPLGetPath( poOpenInfo->pszFilename );
        osQubeFile = CPLFormFilename( osPath, pszCore, NULL );
        poDS->osExternalCube = osQubeFile;
    }

/* -------------------------------------------------------------------- */
/*      Check if file an ISIS3 header file?  Read a few lines of text   */
/*      searching for something starting with nrows or ncols.           */
/* -------------------------------------------------------------------- */
    GDALDataType eDataType = GDT_Byte;
    OGRSpatialReference oSRS;

    int	nRows = -1;
    int nCols = -1;
    int nBands = 1;
    int nSkipBytes = 0;
    int tileSizeX = 0;
    int tileSizeY = 0;
    double dfULXMap=0.5;
    double dfULYMap = 0.5;
    double dfXDim = 1.0;
    double dfYDim = 1.0;
    double scaleFactor = 1.0;
    double dfNoData = 0.0;
    int	bNoDataSet = FALSE;
    char chByteOrder = 'M';  //default to MSB
    char szLayout[32] = "BandSequential"; //default to band seq.
    const char *target_name; //planet name
    //projection parameters
    const char *map_proj_name;
    int	bProjectionSet = TRUE;
    char proj_target_name[200]; 
    char geog_name[60];  
    char datum_name[60];  
    char sphere_name[60];
    char bIsGeographic = TRUE;
    double semi_major = 0.0;
    double semi_minor = 0.0;
    double iflattening = 0.0;
    double center_lat = 0.0;
    double center_lon = 0.0;
    double first_std_parallel = 0.0;
    double second_std_parallel = 0.0;
    double radLat, localRadius;
    VSILFILE	*fp;

    /*************   Skipbytes     *****************************/
    nSkipBytes = atoi(poDS->GetKeyword("IsisCube.Core.StartByte","")) - 1;

    /*******   Grab format type (BandSequential, Tiled)  *******/
    const char *value;

    value = poDS->GetKeyword( "IsisCube.Core.Format", "" );
    if (EQUAL(value,"Tile") )  { //Todo
        strcpy(szLayout,"Tiled");
       /******* Get Tile Sizes *********/
       tileSizeX = atoi(poDS->GetKeyword("IsisCube.Core.TileSamples",""));
       tileSizeY = atoi(poDS->GetKeyword("IsisCube.Core.TileLines",""));
       if (tileSizeX <= 0 || tileSizeY <= 0)
       {
           CPLError( CE_Failure, CPLE_OpenFailed, "Wrong tile dimensions : %d x %d",
                     tileSizeX, tileSizeY);
           delete poDS;
           return NULL;
       }
    }
    else if (EQUAL(value,"BandSequential") )
        strcpy(szLayout,"BSQ");
    else {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "%s layout not supported. Abort\n\n", value);
        delete poDS;
        return NULL;
    }

    /***********   Grab samples lines band ************/
    nCols = atoi(poDS->GetKeyword("IsisCube.Core.Dimensions.Samples",""));
    nRows = atoi(poDS->GetKeyword("IsisCube.Core.Dimensions.Lines",""));
    nBands = atoi(poDS->GetKeyword("IsisCube.Core.Dimensions.Bands",""));
     
    /****** Grab format type - ISIS3 only supports 8,U16,S16,32 *****/
    const char *itype;

    itype = poDS->GetKeyword( "IsisCube.Core.Pixels.Type" );
    if (EQUAL(itype,"UnsignedByte") ) {
        eDataType = GDT_Byte;
        dfNoData = NULL1;
        bNoDataSet = TRUE;
    }
    else if (EQUAL(itype,"UnsignedWord") ) {
        eDataType = GDT_UInt16;
        dfNoData = NULL1;
        bNoDataSet = TRUE;
    }
    else if (EQUAL(itype,"SignedWord") ) {
        eDataType = GDT_Int16;
        dfNoData = NULL2;
        bNoDataSet = TRUE;
    }
    else if (EQUAL(itype,"Real") || EQUAL(value,"") ) {
        eDataType = GDT_Float32;
        dfNoData = NULL3;
        bNoDataSet = TRUE;
    }
    else {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "%s layout type not supported. Abort\n\n", itype);
        delete poDS;
        return NULL;
    }

    /***********   Grab samples lines band ************/
    value = poDS->GetKeyword( "IsisCube.Core.Pixels.ByteOrder");
    if (EQUAL(value,"Lsb"))
        chByteOrder = 'I';
    
    /***********   Grab Cellsize ************/
    value = poDS->GetKeyword("IsisCube.Mapping.PixelResolution");
    if (strlen(value) > 0 ) {
        dfXDim = atof(value); /* values are in meters */
        dfYDim = -atof(value);
    }
    
    /***********   Grab UpperLeftCornerY ************/
    value = poDS->GetKeyword("IsisCube.Mapping.UpperLeftCornerY");
    if (strlen(value) > 0) {
        dfULYMap = atof(value);
    }
     
    /***********   Grab UpperLeftCornerX ************/
    value = poDS->GetKeyword("IsisCube.Mapping.UpperLeftCornerX");
    if( strlen(value) > 0 ) {
        dfULXMap = atof(value);
    }
     
    /***********  Grab TARGET_NAME  ************/
    /**** This is the planets name i.e. Mars ***/
    target_name = poDS->GetKeyword("IsisCube.Mapping.TargetName");
     
    /***********   Grab MAP_PROJECTION_TYPE ************/
    map_proj_name = 
        poDS->GetKeyword( "IsisCube.Mapping.ProjectionName");

    /***********   Grab SEMI-MAJOR ************/
    semi_major = 
        atof(poDS->GetKeyword( "IsisCube.Mapping.EquatorialRadius"));

    /***********   Grab semi-minor ************/
    semi_minor = 
        atof(poDS->GetKeyword( "IsisCube.Mapping.PolarRadius"));

    /***********   Grab CENTER_LAT ************/
    center_lat = 
        atof(poDS->GetKeyword( "IsisCube.Mapping.CenterLatitude"));

    /***********   Grab CENTER_LON ************/
    center_lon = 
        atof(poDS->GetKeyword( "IsisCube.Mapping.CenterLongitude"));

    /***********   Grab 1st std parallel ************/
    first_std_parallel = 
        atof(poDS->GetKeyword( "IsisCube.Mapping.FirstStandardParallel"));

    /***********   Grab 2nd std parallel ************/
    second_std_parallel = 
        atof(poDS->GetKeyword( "IsisCube.Mapping.SecondStandardParallel"));
     
    /***********   Grab scaleFactor ************/
    scaleFactor = 
        atof(poDS->GetKeyword( "IsisCube.Mapping.scaleFactor", "1.0"));
     
    /*** grab      LatitudeType = Planetographic ****/
    // Need to further study how ocentric/ographic will effect the gdal library
    // So far we will use this fact to define a sphere or ellipse for some 
    // projections

    // Frank - may need to talk this over
    value = poDS->GetKeyword("IsisCube.Mapping.LatitudeType");
    if (EQUAL( value, "Planetocentric" ))
        bIsGeographic = FALSE; 
     
    //Set oSRS projection and parameters
    //############################################################
    //ISIS3 Projection types
    //  Equirectangular 
    //  LambertConformal 
    //  Mercator 
    //  ObliqueCylindrical //Todo
    //  Orthographic 
    //  PolarStereographic 
    //  SimpleCylindrical 
    //  Sinusoidal 
    //  TransverseMercator
    
#ifdef DEBUG
    CPLDebug( "ISIS3", "using projection %s", map_proj_name);
#endif

    if ((EQUAL( map_proj_name, "Equirectangular" )) ||
        (EQUAL( map_proj_name, "SimpleCylindrical" )) )  {
        oSRS.OGRSpatialReference::SetEquirectangular2 ( 0.0, center_lon, center_lat, 0, 0 );
    } else if (EQUAL( map_proj_name, "Orthographic" )) {
        oSRS.OGRSpatialReference::SetOrthographic ( center_lat, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "Sinusoidal" )) {
        oSRS.OGRSpatialReference::SetSinusoidal ( center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "Mercator" )) {
        oSRS.OGRSpatialReference::SetMercator ( center_lat, center_lon, scaleFactor, 0, 0 );
    } else if (EQUAL( map_proj_name, "PolarStereographic" )) {
        oSRS.OGRSpatialReference::SetPS ( center_lat, center_lon, scaleFactor, 0, 0 );
    } else if (EQUAL( map_proj_name, "TransverseMercator" )) {
        oSRS.OGRSpatialReference::SetTM ( center_lat, center_lon, scaleFactor, 0, 0 );
    } else if (EQUAL( map_proj_name, "LambertConformal" )) {
        oSRS.OGRSpatialReference::SetLCC ( first_std_parallel, second_std_parallel, center_lat, center_lon, 0, 0 );
    } else {
        CPLDebug( "ISIS3",
                  "Dataset projection %s is not supported. Continuing...",
                  map_proj_name );
        bProjectionSet = FALSE;
    }

    if (bProjectionSet) {
        //Create projection name, i.e. MERCATOR MARS and set as ProjCS keyword
        strcpy(proj_target_name, map_proj_name);
        strcat(proj_target_name, " ");
        strcat(proj_target_name, target_name);
        oSRS.SetProjCS(proj_target_name); //set ProjCS keyword
     
        //The geographic/geocentric name will be the same basic name as the body name
        //'GCS' = Geographic/Geocentric Coordinate System
        strcpy(geog_name, "GCS_");
        strcat(geog_name, target_name);
        
        //The datum name will be the same basic name as the planet
        strcpy(datum_name, "D_");
        strcat(datum_name, target_name);
     
        strcpy(sphere_name, target_name);
        //strcat(sphere_name, "_IAU_IAG");  //Might not be IAU defined so don't add
          
        //calculate inverse flattening from major and minor axis: 1/f = a/(a-b)
        if ((semi_major - semi_minor) < 0.0000001) 
           iflattening = 0;
        else
           iflattening = semi_major / (semi_major - semi_minor);
     
        //Set the body size but take into consideration which proj is being used to help w/ proj4 compatibility
        //The use of a Sphere, polar radius or ellipse here is based on how ISIS does it internally
        if ( ( (EQUAL( map_proj_name, "Stereographic" ) && (fabs(center_lat) == 90)) ) || 
	           (EQUAL( map_proj_name, "PolarStereographic" )) )  
         {
            if (bIsGeographic) { 
                //Geograpraphic, so set an ellipse
                oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                                semi_major, iflattening, 
                               "Reference_Meridian", 0.0 );
            } else {
              //Geocentric, so force a sphere using the semi-minor axis. I hope... 
              strcat(sphere_name, "_polarRadius");
              oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                              semi_minor, 0.0, 
                              "Reference_Meridian", 0.0 );
            }
        }
        else if ( (EQUAL( map_proj_name, "SimpleCylindrical" )) || 
  	               (EQUAL( map_proj_name, "Orthographic" )) || 
	               (EQUAL( map_proj_name, "Stereographic" )) || 
	               (EQUAL( map_proj_name, "Sinusoidal" )) ) {
            //isis uses the sphereical equation for these projections so force a sphere
            oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                            semi_major, 0.0, 
                            "Reference_Meridian", 0.0 );
        } 
        else if  (EQUAL( map_proj_name, "Equirectangular" )) { 
            //Calculate localRadius using ISIS3 simple elliptical method 
            //  not the more standard Radius of Curvature method
            //PI = 4 * atan(1);
            radLat = center_lat * PI / 180;  // in radians
            localRadius = semi_major * semi_minor / sqrt(pow(semi_minor*cos(radLat),2) 
                          + pow(semi_major*sin(radLat),2) );
            strcat(sphere_name, "_localRadius");
            oSRS.SetGeogCS( geog_name, datum_name, sphere_name,
                            localRadius, 0.0, 
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

/* END ISIS3 Label Read */
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
    
/* -------------------------------------------------------------------- */
/*     Is the CUB detached - if so, reset name to binary file?          */
/* -------------------------------------------------------------------- */
#ifdef notdef
    // Frank - is this correct?
    //The extension already added on so don't add another. But is this needed?
    char *pszPath = CPLStrdup( CPLGetPath( poOpenInfo->pszFilename ) );
    char *pszName = CPLStrdup( CPLGetBasename( poOpenInfo->pszFilename ) );
    if (bIsDetached)
        pszCUBFilename = CPLFormCIFilename( pszPath, detachedCub, "" );
#endif

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
        poDS->fpImage = VSIFOpenL( osQubeFile, "r" );
    else
        poDS->fpImage = VSIFOpenL( osQubeFile, "r+" );

    if( poDS->fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to open %s with write permission.\n%s", 
                  osQubeFile.c_str(),
                  VSIStrerror( errno ) );
        delete poDS;
        return NULL;
    }

    poDS->eAccess = poOpenInfo->eAccess;

/* -------------------------------------------------------------------- */
/*      Compute the line offset.                                        */
/* -------------------------------------------------------------------- */
    int     nItemSize = GDALGetDataTypeSize(eDataType)/8;
    int	    nLineOffset=0, nPixelOffset=0, nBandOffset=0;
    
    if( EQUAL(szLayout,"BSQ") )
    {
        nPixelOffset = nItemSize;
        nLineOffset = nPixelOffset * nCols;
        nBandOffset = nLineOffset * nRows;
    }
    else /* Tiled */
    {
    }
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int i;

#ifdef CPL_LSB                               
    int bNativeOrder = !(chByteOrder == 'M');
#else
    int bNativeOrder = (chByteOrder == 'M');
#endif        


    for( i = 0; i < nBands; i++ )
    {
        GDALRasterBand	*poBand;

        if( EQUAL(szLayout,"Tiled") )
        {
            poBand = new ISISTiledBand( poDS, poDS->fpImage, i+1, eDataType,
                                        tileSizeX, tileSizeY, 
                                        nSkipBytes, 0, 0, 
                                        bNativeOrder );
        }
        else
        {
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
        }

        poDS->SetBand( i+1, poBand );

        if( bNoDataSet )
            ((GDALPamRasterBand *) poBand)->SetNoDataValue( dfNoData );

        // Set offset/scale values at the PAM level.
        poBand->SetOffset( 
            CPLAtofM(poDS->GetKeyword("IsisCube.Core.Pixels.Base","0.0")));
        poBand->SetScale( 
          CPLAtofM(poDS->GetKeyword("IsisCube.Core.Pixels.Multiplier","1.0")));
    }

/* -------------------------------------------------------------------- */
/*      Check for a .prj file. For ISIS3 I would like to keep this in   */
/* -------------------------------------------------------------------- */
    CPLString osPath, osName;

    osPath = CPLGetPath( poOpenInfo->pszFilename );
    osName = CPLGetBasename(poOpenInfo->pszFilename);
    const char  *pszPrjFile = CPLFormCIFilename( osPath, osName, "prj" );

    fp = VSIFOpenL( pszPrjFile, "r" );
    if( fp != NULL )
    {
        char	**papszLines;
        OGRSpatialReference oSRS;

        VSIFCloseL( fp );
        
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

    return( poDS );
}

/************************************************************************/
/*                             GetKeyword()                             */
/************************************************************************/

const char *ISIS3Dataset::GetKeyword( const char *pszPath, 
                                      const char *pszDefault )

{
    return oKeywords.GetKeyword( pszPath, pszDefault );
}

/************************************************************************/
/*                            GetKeywordSub()                           */
/************************************************************************/

const char *ISIS3Dataset::GetKeywordSub( const char *pszPath, 
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
    else
    {
        CSLDestroy( papszTokens );
        return pszDefault;
    }
}


/************************************************************************/
/*                         GDALRegister_ISIS3()                         */
/************************************************************************/

void GDALRegister_ISIS3()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "ISIS3" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "ISIS3" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "USGS Astrogeology ISIS cube (Version 3)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_isis3.html" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = ISIS3Dataset::Open;
        poDriver->pfnIdentify = ISIS3Dataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

