/******************************************************************************
 * 
 * Project:  VICAR Driver; JPL/MIPL VICAR Format
 * Purpose:  Implementation of VICARDataset
 * Author:   Sebastian Walter <sebastian dot walter at fu-berlin dot de>
 *
 * NOTE: This driver code is loosely based on the ISIS and PDS drivers.
 * It is not intended to diminish the contribution of the original authors 
 ******************************************************************************
 * Copyright (c) 2014, Sebastian Walter <sebastian dot walter at fu-berlin dot de>
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
#define NULL3 -32768.

#ifndef PI
#  define PI 3.1415926535897932384626433832795
#endif

#include "rawdataset.h"
#include "ogr_spatialref.h"
#include "cpl_string.h" 
#include "vicarkeywordhandler.h"

#include <string>

CPL_CVSID("$Id$");

CPL_C_START
void GDALRegister_VICAR(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*			       VICARDataset		                */
/* ==================================================================== */
/************************************************************************/

class VICARDataset : public RawDataset
{
    VSILFILE	*fpImage;

    GByte	abyHeader[10000];
    CPLString   osExternalCube;
    
    VICARKeywordHandler  oKeywords;
  
    int         bGotTransform;
    double      adfGeoTransform[6];
  
    CPLString   osProjection;

    const char *GetKeyword( const char *pszPath, 
                            const char *pszDefault = "");
    
public:
    VICARDataset();
    ~VICARDataset();
  
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
/*                            VICARDataset()                            */
/************************************************************************/

VICARDataset::VICARDataset()
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
/*                            ~VICARDataset()                            */
/************************************************************************/

VICARDataset::~VICARDataset()

{
    FlushCache();
    if( fpImage != NULL )
        VSIFCloseL( fpImage );
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **VICARDataset::GetFileList()

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

const char *VICARDataset::GetProjectionRef()

{
    if( strlen(osProjection) > 0 )
        return osProjection;
    else
        return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr VICARDataset::GetGeoTransform( double * padfTransform )

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

int VICARDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->pabyHeader == NULL )
        return FALSE;

    return strstr((char*)poOpenInfo->pabyHeader,"LBLSIZE") != NULL &&
           strstr((char*)poOpenInfo->pabyHeader,"FORMAT") != NULL &&
           strstr((char*)poOpenInfo->pabyHeader,"NL") != NULL &&
           strstr((char*)poOpenInfo->pabyHeader,"NS") != NULL &&
           strstr((char*)poOpenInfo->pabyHeader,"NB") != NULL;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *VICARDataset::Open( GDALOpenInfo * poOpenInfo )
{
/* -------------------------------------------------------------------- */
/*      Does this look like a VICAR dataset?                             */
/* -------------------------------------------------------------------- */
    if( !Identify( poOpenInfo ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Open the file using the large file API.                         */
/* -------------------------------------------------------------------- */
    VSILFILE *fpQube = VSIFOpenL( poOpenInfo->pszFilename, "rb" );

    if( fpQube == NULL )
        return NULL;

    VICARDataset 	*poDS;

    poDS = new VICARDataset();
    if( ! poDS->oKeywords.Ingest( fpQube, poOpenInfo->pabyHeader ) ) {
        VSIFCloseL( fpQube );
        delete poDS;
        return NULL;
    }
    
    VSIFCloseL( fpQube );

    CPLString osQubeFile;
    osQubeFile = poOpenInfo->pszFilename;
    GDALDataType eDataType = GDT_Byte;

    int	nRows = -1;
    int nCols = -1;
    int nBands = 1;
    int nSkipBytes = 0;
    int bIsDTM = FALSE;
    char chByteOrder = 'M';
    double dfNoData = 0.0;
    const char *value;
    
    /***** CHECK ENDIANNESS **************/
    
    value = poDS->GetKeyword( "INTFMT" );
    if (!EQUAL(value,"LOW") ) {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "%s layout not supported. Abort\n\n", value);
        return FALSE;
    }
    value = poDS->GetKeyword( "REALFMT" );
    if (!EQUAL(value,"RIEEE") ) {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "%s layout not supported. Abort\n\n", value);
        return FALSE;
    }
    value = poDS->GetKeyword( "BREALFMT" );
    if (EQUAL(value,"VAX") ) {
        chByteOrder = 'I';
    }
    
    /************ CHECK INSTRUMENT *****************/
    /************ ONLY HRSC TESTED *****************/
    
    value = poDS->GetKeyword( "DTM.DTM_OFFSET" );
    if (!EQUAL(value,"") ) {
        bIsDTM = TRUE;
    }
    
    
    value = poDS->GetKeyword( "BLTYPE" );
    if (!EQUAL(value,"M94_HRSC") && bIsDTM==FALSE ) {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "%s instrument not tested. Continue with caution!\n\n", value);
    }
    
    /***********   Grab layout type (BSQ, BIP, BIL) ************/
    
    char szLayout[10] = "BSQ"; //default to band seq.
    value = poDS->GetKeyword( "ORG" );
    if (EQUAL(value,"BSQ") ) {
        strcpy(szLayout,"BSQ");
        nCols = atoi(poDS->GetKeyword("NS"));
        nRows = atoi(poDS->GetKeyword("NL"));
        nBands = atoi(poDS->GetKeyword("NB"));
    }
    else {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "%s layout not supported. Abort\n\n", value);
        return FALSE;
    }
    
    /***********   Grab record bytes  **********/
    nSkipBytes = atoi(poDS->GetKeyword("NBB"));
    
    if (EQUAL( poDS->GetKeyword( "FORMAT" ), "BYTE" )) {
        eDataType = GDT_Byte;
        dfNoData = NULL1;
    }
    else if (EQUAL( poDS->GetKeyword( "FORMAT" ), "HALF" )) {
        eDataType = GDT_Int16;
        dfNoData = NULL2;
        chByteOrder = 'I';
    }
    else if (EQUAL( poDS->GetKeyword( "FORMAT" ), "FULL" )) {
        eDataType = GDT_UInt32;
        dfNoData = 0;
    } 
    else if (EQUAL( poDS->GetKeyword( "FORMAT" ), "REAL" )) {
        eDataType = GDT_Float32;
        dfNoData = NULL3;
        chByteOrder = 'I';
    }
    else {
	    printf("Could not find known VICAR label entries!\n");
        delete poDS;
        return NULL;
    }

    if( nRows < 1 || nCols < 1 || nBands < 1 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "File %s appears to be a VICAR file, but failed to find some required keywords.", 
                  poDS->GetDescription() );
        return FALSE;
    }
/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;

    double dfULXMap=0.5;
    double dfULYMap = 0.5;
    double dfXDim = 1.0;
    double dfYDim = 1.0;
    double xulcenter = 0.0;
    double yulcenter = 0.0;

    value = poDS->GetKeyword("MAP.MAP_SCALE");
    if (strlen(value) > 0 ) {
        dfXDim = CPLAtof(value);
        dfYDim = CPLAtof(value) * -1;
        dfXDim = dfXDim * 1000.0;
        dfYDim = dfYDim * 1000.0;
    }

    double   dfSampleOffset_Shift;
    double   dfLineOffset_Shift;
    double   dfSampleOffset_Mult;
    double   dfLineOffset_Mult;
    
    dfSampleOffset_Shift = 
        CPLAtof(CPLGetConfigOption( "PDS_SampleProjOffset_Shift", "-0.5" ));
    
    dfLineOffset_Shift = 
        CPLAtof(CPLGetConfigOption( "PDS_LineProjOffset_Shift", "-0.5" ));

    dfSampleOffset_Mult =
        CPLAtof(CPLGetConfigOption( "PDS_SampleProjOffset_Mult", "-1.0") );

    dfLineOffset_Mult = 
        CPLAtof( CPLGetConfigOption( "PDS_LineProjOffset_Mult", "1.0") );

    /***********   Grab LINE_PROJECTION_OFFSET ************/
    value = poDS->GetKeyword("MAP.LINE_PROJECTION_OFFSET");
    if (strlen(value) > 0) {
        yulcenter = CPLAtof(value);
        dfULYMap = ((yulcenter + dfLineOffset_Shift) * -dfYDim * dfLineOffset_Mult);
    }
    /***********   Grab SAMPLE_PROJECTION_OFFSET ************/
    value = poDS->GetKeyword("MAP.SAMPLE_PROJECTION_OFFSET");
    if( strlen(value) > 0 ) {
        xulcenter = CPLAtof(value);
        dfULXMap = ((xulcenter + dfSampleOffset_Shift) * dfXDim * dfSampleOffset_Mult);
    }
    
/* ==================================================================== */
/*      Get the coordinate system.                                      */
/* ==================================================================== */
    int	bProjectionSet = TRUE;
    double semi_major = 0.0;
    double semi_minor = 0.0;
    double iflattening = 0.0;
    double center_lat = 0.0;
    double center_lon = 0.0;
    double first_std_parallel = 0.0;
    double second_std_parallel = 0.0;    
    OGRSpatialReference oSRS;
    
    /***********  Grab TARGET_NAME  ************/
    /**** This is the planets name i.e. MARS ***/
    CPLString target_name = poDS->GetKeyword("MAP.TARGET_NAME");
     
    /**********   Grab MAP_PROJECTION_TYPE *****/
    CPLString map_proj_name = 
        poDS->GetKeyword( "MAP.MAP_PROJECTION_TYPE");
     
    /******  Grab semi_major & convert to KM ******/
    semi_major = 
        CPLAtof(poDS->GetKeyword( "MAP.A_AXIS_RADIUS")) * 1000.0;
    
    /******  Grab semi-minor & convert to KM ******/
    semi_minor = 
        CPLAtof(poDS->GetKeyword( "MAP.C_AXIS_RADIUS")) * 1000.0;

    /***********   Grab CENTER_LAT ************/
    center_lat = 
        CPLAtof(poDS->GetKeyword( "MAP.CENTER_LATITUDE"));

    /***********   Grab CENTER_LON ************/
    center_lon = 
        CPLAtof(poDS->GetKeyword( "MAP.CENTER_LONGITUDE"));

    /**********   Grab 1st std parallel *******/
    first_std_parallel = 
        CPLAtof(poDS->GetKeyword( "MAP.FIRST_STANDARD_PARALLEL"));

    /**********   Grab 2nd std parallel *******/
    second_std_parallel = 
        CPLAtof(poDS->GetKeyword( "MAP.SECOND_STANDARD_PARALLEL"));
     
    /*** grab  PROJECTION_LATITUDE_TYPE = "PLANETOCENTRIC" ****/
    // Need to further study how ocentric/ographic will effect the gdal library.
    // So far we will use this fact to define a sphere or ellipse for some projections
    // Frank - may need to talk this over
    char bIsGeographic = TRUE;
    value = poDS->GetKeyword("MAP.COORDINATE_SYSTEM_NAME");
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
        CPLDebug( "VICAR",
                  "Dataset projection %s is not supported. Continuing...",
                  map_proj_name.c_str() );
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
            GDALReadWorldFile( osQubeFile, "psw", 
                               poDS->adfGeoTransform );

    if( !poDS->bGotTransform )
        poDS->bGotTransform = 
            GDALReadWorldFile( osQubeFile, "wld", 
                               poDS->adfGeoTransform );


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
/*      Compute the line offsets.                                        */
/* -------------------------------------------------------------------- */

    long int        nItemSize = GDALGetDataTypeSize(eDataType)/8;
    long int	    nLineOffset=0, nPixelOffset=0, nBandOffset=0;
    nPixelOffset = nItemSize;
    nLineOffset = nPixelOffset * nCols + atoi(poDS->GetKeyword("NBB")) ;
    nBandOffset = nLineOffset * nRows;

    nSkipBytes = atoi(poDS->GetKeyword("LBLSIZE"));

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int i;

    for( i = 0; i < nBands; i++ )
    {
        GDALRasterBand	*poBand;

        poBand = new RawRasterBand( poDS, i+1, poDS->fpImage, nSkipBytes + nBandOffset * i, 
                                   nPixelOffset, nLineOffset, eDataType,
#ifdef CPL_LSB                               
                                   chByteOrder == 'I' || chByteOrder == 'L',
#else
                                   chByteOrder == 'M',
#endif
                                   TRUE );

        poDS->SetBand( i+1, poBand );
        poBand->SetNoDataValue( dfNoData );
        if (bIsDTM==TRUE) {
            poBand->SetScale( (double) CPLAtof(poDS->GetKeyword( "DTM.DTM_SCALING_FACTOR") ) );
            poBand->SetOffset( (double) CPLAtof(poDS->GetKeyword( "DTM.DTM_OFFSET") ) );
            const char* pszMin = poDS->GetKeyword( "DTM.DTM_MINIMUM_DN", NULL );
            const char* pszMax = poDS->GetKeyword( "DTM.DTM_MAXIMUM_DN", NULL );
            if (pszMin != NULL && pszMax != NULL )
                poBand->SetStatistics(CPLAtofM(pszMin),CPLAtofM(pszMax),0,0);
            const char* pszNoData = poDS->GetKeyword( "DTM.DTM_MISSING_DN", NULL );
            if (pszNoData != NULL )
                poBand->SetNoDataValue( CPLAtofM(pszNoData) );
        } else if (EQUAL( poDS->GetKeyword( "BLTYPE"), "M94_HRSC" )) {
            float scale=CPLAtof(poDS->GetKeyword("DLRTO8.REFLECTANCE_SCALING_FACTOR","-1."));
            if (scale < 0.) {
                scale = CPLAtof(poDS->GetKeyword( "HRCAL.REFLECTANCE_SCALING_FACTOR","1."));
            }
            poBand->SetScale( scale );
            float offset=CPLAtof(poDS->GetKeyword("DLRTO8.REFLECTANCE_OFFSET","-1."));
            if (offset < 0.) {
                offset = CPLAtof(poDS->GetKeyword( "HRCAL.REFLECTANCE_OFFSET","0."));
            }
            poBand->SetOffset( offset );
        }
        const char* pszMin = poDS->GetKeyword( "STATISTICS.MINIMUM", NULL );
        const char* pszMax = poDS->GetKeyword( "STATISTICS.MAXIMUM", NULL );
        const char* pszMean = poDS->GetKeyword( "STATISTICS.MEAN", NULL );
        const char* pszStdDev = poDS->GetKeyword( "STATISTICS.STANDARD_DEVIATION", NULL );
        if (pszMin != NULL && pszMax != NULL && pszMean != NULL && pszStdDev != NULL )
                poBand->SetStatistics(CPLAtofM(pszMin),CPLAtofM(pszMax),CPLAtofM(pszMean),CPLAtofM(pszStdDev));
    }


/* -------------------------------------------------------------------- */
/*      Instrument-specific keywords as metadata.                       */
/* -------------------------------------------------------------------- */

/******************   HRSC    ******************************/
   
    if (EQUAL( poDS->GetKeyword( "BLTYPE"), "M94_HRSC" ) ) {
        poDS->SetMetadataItem( "SPACECRAFT_NAME", poDS->GetKeyword( "M94_INSTRUMENT.INSTRUMENT_HOST_NAME") );
        poDS->SetMetadataItem( "PRODUCT_TYPE", poDS->GetKeyword( "TYPE"));
    	
        if (EQUAL( poDS->GetKeyword( "M94_INSTRUMENT.DETECTOR_ID"), "MEX_HRSC_SRC" )) {
            static const char *apszKeywords[] =  {
                        "M94_ORBIT.IMAGE_TIME",
                        "FILE.EVENT_TYPE",
                        "FILE.PROCESSING_LEVEL_ID",
                        "M94_INSTRUMENT.DETECTOR_ID", 
                        "M94_CAMERAS.EXPOSURE_DURATION",
                        "HRCONVER.INSTRUMENT_TEMPERATURE", NULL
                    };
            for( i = 0; apszKeywords[i] != NULL; i++ ) {
                const char *pszKeywordValue = poDS->GetKeyword( apszKeywords[i] );
                if( pszKeywordValue != NULL )
                    poDS->SetMetadataItem( apszKeywords[i], pszKeywordValue );
            }
        } else {
            static const char *apszKeywords[] =  {
                "M94_ORBIT.START_TIME", "M94_ORBIT.STOP_TIME",
                "M94_INSTRUMENT.DETECTOR_ID",
                "M94_CAMERAS.MACROPIXEL_SIZE",
                "FILE.EVENT_TYPE",
                "M94_INSTRUMENT.MISSION_PHASE_NAME",
                "HRORTHO.SPICE_FILE_NAME",
                "HRCONVER.MISSING_FRAMES", "HRCONVER.OVERFLOW_FRAMES", "HRCONVER.ERROR_FRAMES",
                "HRFOOT.BEST_GROUND_SAMPLING_DISTANCE",
                "DLRTO8.RADIANCE_SCALING_FACTOR", "DLRTO8.RADIANCE_OFFSET",
                "DLRTO8.REFLECTANCE_SCALING_FACTOR", "DLRTO8.REFLECTANCE_OFFSET",
                "HRCAL.RADIANCE_SCALING_FACTOR", "HRCAL.RADIANCE_OFFSET",
                "HRCAL.REFLECTANCE_SCALING_FACTOR", "HRCAL.REFLECTANCE_OFFSET",
                "HRORTHO.DTM_NAME", "HRORTHO.EXTORI_FILE_NAME", "HRORTHO.GEOMETRIC_CALIB_FILE_NAME",
                NULL
            };
            for( i = 0; apszKeywords[i] != NULL; i++ ) {
                const char *pszKeywordValue = poDS->GetKeyword( apszKeywords[i], NULL );
                if( pszKeywordValue != NULL )
                    poDS->SetMetadataItem( apszKeywords[i], pszKeywordValue );
            }
        }
    }
    if (bIsDTM==TRUE && EQUAL( poDS->GetKeyword( "MAP.TARGET_NAME"), "MARS" )) {
        poDS->SetMetadataItem( "SPACECRAFT_NAME", "MARS_EXPRESS" );
        poDS->SetMetadataItem( "PRODUCT_TYPE", "DTM");
        static const char *apszKeywords[] = {
            "DTM.DTM_MISSING_DN", "DTM.DTM_OFFSET", "DTM.DTM_SCALING_FACTOR", "DTM.DTM_A_AXIS_RADIUS", 
            "DTM.DTM_B_AXIS_RADIUS", "DTM.DTM_C_AXIS_RADIUS", "DTM.DTM_DESC", "DTM.DTM_MINIMUM_DN",
            "DTM.DTM_MAXIMUM_DN", NULL };
        for( i = 0; apszKeywords[i] != NULL; i++ ) {
            const char *pszKeywordValue = poDS->GetKeyword( apszKeywords[i] );
            if( pszKeywordValue != NULL )
                poDS->SetMetadataItem( apszKeywords[i], pszKeywordValue );
        }
    }


/******************   DAWN   ******************************/
    else if (EQUAL( poDS->GetKeyword( "INSTRUMENT_ID"), "FC2" )) {
        poDS->SetMetadataItem( "SPACECRAFT_NAME", "DAWN" );
        static const char *apszKeywords[] =  {"ORBIT_NUMBER","FILTER_NUMBER",
        "FRONT_DOOR_STATUS",
        "FIRST_LINE",
        "FIRST_LINE_SAMPLE",
        "PRODUCER_INSTITUTION_NAME",
        "SOURCE_FILE_NAME",
        "PROCESSING_LEVEL_ID",
        "TARGET_NAME",
        "LIMB_IN_IMAGE",
        "POLE_IN_IMAGE",
        "REFLECTANCE_SCALING_FACTOR",
        "SPICE_FILE_NAME",
        "SPACECRAFT_CENTRIC_LATITUDE",
        "SPACECRAFT_EASTERN_LONGITUDE",
        "FOOTPRINT_POSITIVE_LONGITUDE",
            NULL };
        for( i = 0; apszKeywords[i] != NULL; i++ ) {
            const char *pszKeywordValue = poDS->GetKeyword( apszKeywords[i] );
            if( pszKeywordValue != NULL )
                poDS->SetMetadataItem( apszKeywords[i], pszKeywordValue );
        }
        
    }
    else if (bIsDTM==TRUE && EQUAL( poDS->GetKeyword( "TARGET_NAME"), "VESTA" )) {
        poDS->SetMetadataItem( "SPACECRAFT_NAME", "DAWN" );
        poDS->SetMetadataItem( "PRODUCT_TYPE", "DTM");
        static const char *apszKeywords[] = {
            "DTM_MISSING_DN", "DTM_OFFSET", "DTM_SCALING_FACTOR", "DTM_A_AXIS_RADIUS", 
            "DTM_B_AXIS_RADIUS", "DTM_C_AXIS_RADIUS", "DTM_MINIMUM_DN",
            "DTM_MAXIMUM_DN", "MAP_PROJECTION_TYPE", "COORDINATE_SYSTEM_NAME",
            "POSITIVE_LONGITUDE_DIRECTION", "MAP_SCALE",
            "CENTER_LONGITUDE", "LINE_PROJECTION_OFFSET", "SAMPLE_PROJECTION_OFFSET",
            NULL };
        for( i = 0; apszKeywords[i] != NULL; i++ ) {
            const char *pszKeywordValue = poDS->GetKeyword( apszKeywords[i] );
            if( pszKeywordValue != NULL )
                poDS->SetMetadataItem( apszKeywords[i], pszKeywordValue );
        }
    }


/* -------------------------------------------------------------------- */
/*      END Instrument-specific keywords as metadata.                   */
/* -------------------------------------------------------------------- */

    if (EQUAL(poDS->GetKeyword( "EOL"), "1" ))
        poDS->SetMetadataItem( "END-OF-DATASET_LABEL", "PRESENT" );
    poDS->SetMetadataItem( "CONVERSION_DETAILS", "http://www.lpi.usra.edu/meetings/lpsc2014/pdf/1088.pdf" );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
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

const char *VICARDataset::GetKeyword( const char *pszPath, 
                                      const char *pszDefault )

{
    return oKeywords.GetKeyword( pszPath, pszDefault );
}



/************************************************************************/
/*                         GDALRegister_VICAR()                         */
/************************************************************************/

void GDALRegister_VICAR()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "VICAR" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "VICAR" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "MIPL VICAR file" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_vicar.html" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = VICARDataset::Open;
        poDriver->pfnIdentify = VICARDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

