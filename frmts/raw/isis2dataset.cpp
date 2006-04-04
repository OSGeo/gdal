/******************************************************************************
 * $Id$
 *
 * Project:  ISIS Version 2 Driver
 * Purpose:  Implementation of ISIS2Dataset
 * Author:   Trent Hare (thare@usgs.gov),
 *           Robert Soricone (rsoricone@usgs.gov)
 *
 ******************************************************************************
 * Copyright (c) 2006, Frank Warmerdam <warmerdam@pobox.com>
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
 * $Log$
 * Revision 1.2  2006/04/04 04:34:12  fwarmerdam
 * Fixed copyright date.
 *
 * Revision 1.1  2006/04/04 04:33:29  fwarmerdam
 * New
 *
 */

#define NULL1 0
#define NULL2 -32768
//#define NULL3 -0.3402822655089E+39 /*0xFF7FFFFB*/

#define NULL3 0xFF7FFFFB //in hex

#include "rawdataset.h"
#include "ogr_spatialref.h"
#include "cpl_string.h" 

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_ISIS2(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                          NASAKeywordHandler                          */
/* ==================================================================== */
/************************************************************************/

class NASAKeywordHandler
{
    char     **papszKeywordList;

    CPLString osHeaderText;
    const char *pszHeaderNext;

    void    SkipWhite();
    int     ReadWord( CPLString &osWord );
    int     ReadPair( CPLString &osName, CPLString &osValue );
    int     ReadGroup( const char *pszPathPrefix );

public:
    NASAKeywordHandler();
    ~NASAKeywordHandler();

    int     Ingest( FILE *fp, int nOffset );

    const char *GetKeyword( const char *pszPath, const char *pszDefault );
};

/************************************************************************/
/* ==================================================================== */
/*			ISISDataset	version2	                */
/* ==================================================================== */
/************************************************************************/

class ISIS2Dataset : public RawDataset
{
    FILE	*fpImage;	// image data file.

    NASAKeywordHandler  oKeywords;
  
    int         bGotTransform;
    double      adfGeoTransform[6];
  
    char	*pszProjection;

    int parse_label(const char *file, char *keyword, char *value);
    int strstrip(char instr[], char outstr[], int position);

    CPLString   oTempResult;

    const char *GetKeyword( const char *pszPath, 
                            const char *pszDefault = "");
    const char *GetKeywordSub( const char *pszPath, 
                               int iSubscript, 
                               const char *pszDefault = "");
    
public:
    ISIS2Dataset();
    ~ISIS2Dataset();
  
    virtual CPLErr GetGeoTransform( double * padfTransform );
    virtual const char *GetProjectionRef(void);
  
    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
};

/************************************************************************/
/*                            ISIS2Dataset()                            */
/************************************************************************/

ISIS2Dataset::ISIS2Dataset()
{
    fpImage = NULL;
    pszProjection = CPLStrdup("");
    bGotTransform = FALSE;
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
    CPLFree( pszProjection );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *ISIS2Dataset::GetProjectionRef()

{
    return pszProjection;
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
    else
    {
        return GDALDataset::GetGeoTransform( padfTransform );
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ISIS2Dataset::Open( GDALOpenInfo * poOpenInfo )
{
/* -------------------------------------------------------------------- */
/*      Does this look like a CUBE dataset?                             */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->pabyHeader == NULL
        || strstr((const char *)poOpenInfo->pabyHeader,"^QUBE") == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Open the file using the large file API.                         */
/* -------------------------------------------------------------------- */
    FILE *fpQube = VSIFOpenL( poOpenInfo->pszFilename, "rb" );

    if( fpQube == NULL )
        return NULL;

    ISIS2Dataset 	*poDS;

    poDS = new ISIS2Dataset();
    poDS->fpImage = fpQube;

    if( ! poDS->oKeywords.Ingest( fpQube, 0 ) )
    {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*	We assume the user is pointing to the label (ie. .lab) file.  	*/
/* -------------------------------------------------------------------- */
    //  QUBE can be inline or detached and point to an image name
    // ^QUBE = 76
    // ^QUBE = ("ui31s015.img",6441<BYTES>) - has another label on the image
    // ^QUBE = "ui31s015.img" - which implies no label or skip value

    const char *pszQube = poDS->GetKeyword( "^QUBE" );
    int nQube = atoi(pszQube);

    if( pszQube[0] == '"' )
    {
        CPLAssert( FALSE ); // TODO
    }
    else if( pszQube[0] == '(' )
    {
        CPLAssert( FALSE ); // TODO
    }

/* -------------------------------------------------------------------- */
/*      Check if file an ISIS2 header file?  Read a few lines of text   */
/*      searching for something starting with nrows or ncols.           */
/* -------------------------------------------------------------------- */
    GDALDataType eDataType = GDT_Byte;
    OGRSpatialReference oSRS;

    int	nRows = -1;
    int nCols = -1;
    int nBands = 1;
    int nSkipBytes = 0;
    int itype;
    int  s_ix, s_iy, s_iz; // check SUFFIX_ITEMS params.
    int record_bytes;
    double dfULXMap=0.5;
    double dfULYMap = 0.5;
    double dfXDim = 1.0;
    double dfYDim = 1.0;
    double dfNoData = 0.0;
    int	bNoDataSet = FALSE;
    char chByteOrder = 'M';  //default to MSB
    char szLayout[10] = "BSQ"; //default to band seq.
    char target_name[60]; //planet name
    //projection parameters
    float xulcenter = 0.0;
    float yulcenter = 0.0;
    char map_proj_name[60];
    int	bProjectionSet = TRUE;
    char proj_target_name[80]; 
    char datum_name[60];  
    char sphere_name[60];
    char bIsGeographic = TRUE;
    double semi_major = 0.0;
    double semi_minor = 0.0;
    double iflattening = 0.0;
    float center_lat = 0.0;
    float center_lon = 0.0;
    float first_std_parallel = 0.0;
    float second_std_parallel = 0.0;
    FILE	*fp;

    /* -------------------------------------------------------------------- */
    /*      Checks to see if this is valid ISIS2 cube                       */
    /*      SUFFIX_ITEM tag in .cub file should be (0,0,0); no side-planes  */
    /* -------------------------------------------------------------------- */
    s_ix = atoi(poDS->GetKeywordSub( "QUBE.SUFFIX_ITEMS", 1 ));
    s_iy = atoi(poDS->GetKeywordSub( "QUBE.SUFFIX_ITEMS", 2 ));
    s_iz = atoi(poDS->GetKeywordSub( "QUBE.SUFFIX_ITEMS", 3 ));
     
    if( s_ix != 0 || s_iy != 0 || s_iz != 0 ) {
        printf( "*** ISIS 2 cube file has invalid SUFFIX_ITEMS parameters:\n");
        printf( "*** gdal isis2 driver requires (0, 0, 0), thus no sideplanes or backplanes");
        printf( "found: (%i, %i, %i)\n\n", s_ix, s_iy, s_iz );
        printf( "exit status 1\n\n" );
        exit(1);
    } 
    /**************** end SUFFIX_ITEM check ***********************/
    
    
    /***********   Grab layout type (BSQ, BIP, BIL) ************/
    //  AXIS_NAME = (SAMPLE,LINE,BAND)
    /***********************************************************/
#ifdef notdef
    i = poDS->parse_label(pszCUBFilename, "AXIS_NAME", value);
    if (i == FALSE) {
        printf("\nAXIS_NAME not found. Abort\n\n");
        exit(1);
    }
    if (EQUAL(value,"(SAMPLE,LINE,BAND)") )
        strcpy(szLayout,"BSQ");
    else if (EQUAL(value,"(BAND,LINE,SAMPLE)") )
        strcpy(szLayout,"BIP");
    else if (EQUAL(value,"(SAMPLE,BAND,LINE)") )
        strcpy(szLayout,"BSQ");
    else {
        printf( "%s layout not supported. Abort\n\n", value);
        exit(1);
    }

    /***********   Grab samples lines band ************/
    i = poDS->parse_label(pszCUBFilename, "CORE_ITEMS", value);
    if (i == FALSE) {
        printf("\nCORE_ITEMS not found. Abort\n\n");
        exit(1);
    }
    i = poDS->strstrip(value,value_strip,1);
    nCols = atoi(value_strip);
    i = poDS->strstrip(value,value_strip,2);
    nRows = atoi(value_strip);
    i = poDS->strstrip(value,value_strip,3);
    nBands = atoi(value_strip);
    
    /***********   Grab Qube record bytes  **********/
    i = poDS->parse_label(pszCUBFilename, "RECORD_BYTES", value);
    if (i == FALSE) 
    {
        printf("\nRECORD_BYTES not found. Abort\n\n");
        exit(1);
    } else {
        record_bytes = atoi(value);
    }
    if (nQube > 0)
        nSkipBytes = (nQube - 1) * record_bytes;     
    else
        nSkipBytes = 0;     
     
    /********   Grab format type - isis2 only supports 8,16,32 *******/
    i = poDS->parse_label(pszCUBFilename, "CORE_ITEM_BYTES", value);
    if (i == FALSE) {
        printf("\nCORE_ITEM_BYTES not found. Abort\n\n");
        exit(1);
    }
    itype = atoi(value);
    switch(itype) {
      case 1 :
        eDataType = GDT_Byte;
        dfNoData = NULL1;
        bNoDataSet = TRUE;
        break;
      case 2 :
        eDataType = GDT_Int16;
        dfNoData = NULL2;
        bNoDataSet = TRUE;
        break;
      case 4 :
        eDataType = GDT_Float32;
        dfNoData = NULL3;
        bNoDataSet = TRUE;
        break;
      default :
        printf("\nItype of %d is not supported in ISIS 2. Exiting\n\n",itype); 
        exit(1);
    }

    /***********   Grab samples lines band ************/
    i = poDS->parse_label(pszCUBFilename, "CORE_ITEM_TYPE", value);
    if (i == FALSE) {
        printf("\nCORE_ITEM_TYPE not found. Abort\n\n");
        exit(1);
    }
    if ( (EQUAL(value,"PC_INTEGER")) || 
         (EQUAL(value,"PC_UNSIGNED_INTEGER")) || 
         (EQUAL(value,"PC_REAL")) ) {
        chByteOrder = 'I';
    }
    
    /***********   Grab Cellsize ************/
    i = poDS->parse_label(pszCUBFilename, "MAP_SCALE", value);
    if (i) {
        dfXDim = (float) atof(value) * 1000.0; /* convert from km to m */
        dfYDim = (float) atof(value) * 1000.0 * -1;
    }
    
    /***********   Grab LINE_PROJECTION_OFFSET ************/
    i = poDS->parse_label(pszCUBFilename, "LINE_PROJECTION_OFFSET", value);
    if (i) {
        yulcenter = (float) atof(value);
        yulcenter = ((yulcenter) * dfYDim);
        dfULYMap = yulcenter - (dfYDim/2);
    }
     
    /***********   Grab SAMPLE_PROJECTION_OFFSET ************/
    i = poDS->parse_label(pszCUBFilename, "SAMPLE_PROJECTION_OFFSET", value);
    if (i) {
        xulcenter = (float) atof(value);
        xulcenter = ((yulcenter) * dfXDim);
        dfULXMap = xulcenter - (dfXDim/2);
    }
     
    /***********  Grab TARGET_NAME  ************/
    /**** This is the planets name i.e. MARS ***/
    i = poDS->parse_label(pszCUBFilename, "TARGET_NAME", target_name);
    if (i) {
        //printf("ISIS 2 Target Name: %s\n", target_name);
    }
     
    /***********   Grab MAP_PROJECTION_TYPE ************/
    i = poDS->parse_label(pszCUBFilename, "MAP_PROJECTION_TYPE", map_proj_name);
    if (i) {
        //printf("ISIS 2 projection: %s\n", map_proj_name);
    }
     
    /***********   Grab SEMI-MAJOR ************/
    i = poDS->parse_label(pszCUBFilename, "A_AXIS_RADIUS", value);
    if (i) {
        semi_major = (double) atof(value);
        printf("SemiMajor: %f\n", semi_major); 
    }

    /***********   Grab semi-minor ************/
    i = poDS->parse_label(pszCUBFilename, "C_AXIS_RADIUS", value);
    if (i) {
        semi_major = (double) atof(value);
        printf("SemiMinor: %f\n", semi_major); 
    }

    /***********   Grab CENTER_LAT ************/
    i = poDS->parse_label(pszCUBFilename, "CENTER_LATITUDE", value);
    if (i) {
        center_lat = (float) atof(value); 
        printf("center_lat: %f\n", center_lat); 
    }

    /***********   Grab CENTER_LON ************/
    i = poDS->parse_label(pszCUBFilename, "CENTER_LONGITUDE", value);
    if (i) {
        center_lon = (float) atof(value);
        printf("center_lon: %f\n", center_lon); 
    }

    /***********   Grab 1st std parallel ************/
    i = poDS->parse_label(pszCUBFilename, "FIRST_STANDARD_PARALLEL", value);
    if (i) {
        first_std_parallel = (float) atof(value);
        printf("first std par: %f\n", center_lon);
    }

    /***********   Grab 2nd std parallel ************/
    i = poDS->parse_label(pszCUBFilename, "SECOND_STANDARD_PARALLEL", value);
    if (i) {
        second_std_parallel = (float) atof(value);
        printf("second std par: %f\n", second_std_parallel);
    }
     
    /*** grab  PROJECTION_LATITUDE_TYPE = "PLANETOCENTRIC" ****/
    // Need to further study how ocentric/ographic will effect the gdal library.
    // So far we will use this fact to define a sphere or ellipse for some projections
    // Frank - may need to talk this over
    i = poDS->parse_label(pszCUBFilename, "PROJECTION_LATITUDE_TYPE", value);
    if (i) {
        if (EQUAL( value, "\"PLANETOCENTRIC\"" ))
            bIsGeographic = FALSE; 
    }
     
    //Set oSRS projection and parameters
    if ((EQUAL( map_proj_name, "\"EQUIRECTANGULAR_CYLINDRICAL\"" )) ||
        (EQUAL( map_proj_name, "\"SIMPLE_CYLINDRICAL\"" )) )  {
#ifdef DEBUG
        printf("using projection %s\n\n", map_proj_name);
#endif
        oSRS.OGRSpatialReference::SetEquirectangular ( center_lat, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "\"ORTHOGRAPHIC\"" )) {
#ifdef DEBUG
        printf ("using projection %s\n\n", map_proj_name);
#endif
        oSRS.OGRSpatialReference::SetOrthographic ( center_lat, center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "\"SINUSOIDAL\"" )) {
#ifdef DEBUG
        printf ("using projection %s\n\n", map_proj_name);
#endif
        oSRS.OGRSpatialReference::SetSinusoidal ( center_lon, 0, 0 );
    } else if (EQUAL( map_proj_name, "\"MERCATOR\"" )) {
#ifdef DEBUG
        printf ("using projection %s\n\n", map_proj_name);
#endif
        oSRS.OGRSpatialReference::SetMercator ( center_lat, center_lon, 1, 0, 0 );
    } else if (EQUAL( map_proj_name, "\"POLAR_STEREOGRAPHIC\"" )) {
#ifdef DEBUG
        printf ("using projection %s\n\n", map_proj_name);
#endif
        oSRS.OGRSpatialReference::SetPS ( center_lat, center_lon, 1, 0, 0 );
    } else if (EQUAL( map_proj_name, "\"TRANSVERSE_MERCATOR\"" )) {
#ifdef DEBUG
        printf ("using projection %s\n\n", map_proj_name);
#endif
        oSRS.OGRSpatialReference::SetTM ( center_lat, center_lon, 1, 0, 0 );
    } else if (EQUAL( map_proj_name, "\"LAMBERT_CONFORMAL_CONIC\"" )) {
#ifdef DEBUG
        printf ("using projection %s\n\n", map_proj_name);
#endif
        oSRS.OGRSpatialReference::SetLCC ( first_std_parallel, second_std_parallel, center_lat, center_lon, 0, 0 );
    } else {
        printf("*** no projection define or supported! Are you sure this is a map projected cube?\n\n" );
        bProjectionSet = FALSE;
    }

    if (bProjectionSet) {
        //Create projection name, i.e. MARS_MERCATOR
        strcpy(proj_target_name, map_proj_name);
        strcat(proj_target_name, "_");
        strcat(proj_target_name, target_name);
     
        //The datum name will be the same basic name aas the planet
        strcpy(datum_name, "D_");
        strcat(datum_name, target_name);
     
        strcpy(sphere_name, target_name);
        //strcat(sphere_name, "_IAU_IAG");  //Might not be IAU defined so don't add
          
        //calculate inverse flattening from major and minor axis: 1/f = a/(a-b)
        iflattening = semi_major / (semi_major - semi_minor);
     
        //The use of a Sphere, polar radius or ellipse here is based on how ISIS 2 does it internally
        //Notice that most ISIS 2 projections are spherical
        if ( (EQUAL( map_proj_name, "\"EQUIRECTANGULAR_CYLINDRICAL\"" )) ||
	     (EQUAL( map_proj_name, "\"SIMPLE_CYLINDRICAL\"" )) || 
	     (EQUAL( map_proj_name, "\"ORTHOGRAPHIC\"" )) || 
	     (EQUAL( map_proj_name, "\"SINUSOIDAL\"" )) ) { //flattening = 1.0 for sphere
            oSRS.SetGeogCS( proj_target_name, datum_name, sphere_name,
                            semi_major, 1.0, "Reference_Meridian", 0.0, "degree" );
            //Here isis2 uses the polar radius to define m/p, so we should use the polar radius for body
        } else if  (EQUAL( map_proj_name, "\"POLAR_STEREOGRAPHIC\"" )) { 
            //flattening = 1.0 for sphere using minor axis
            oSRS.SetGeogCS( proj_target_name, datum_name, sphere_name,
                            semi_minor, 1.0, "Reference_Meridian", 0.0, "degree" );
        } else { //ellipse => Mercator, Transverse Mercator, Lambert Conformal
            if (bIsGeographic) {
                oSRS.SetGeogCS( proj_target_name, datum_name, sphere_name,
                                semi_major, iflattening, "Reference_Meridian", 0.0, "degree" );
            } else { //we have Ocentric so use a sphere! I hope... So flattening is 1.0
                oSRS.SetGeogCS( proj_target_name, datum_name, sphere_name,
                                semi_major, 1.0, "Reference_Meridian", 0.0, "degree" );
            }
        }
    }

/* END ISIS2 Label Read */
/*++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++*/
    
/* -------------------------------------------------------------------- */
/*     Is the CUB detached - if so, reset name to binary file?          */
/* -------------------------------------------------------------------- */
    // Frank - is this correct?
    //The extension already added on so don't add another. But is this needed?
    char *pszPath = CPLStrdup( CPLGetPath( poOpenInfo->pszFilename ) );
    char *pszName = CPLStrdup( CPLGetBasename( poOpenInfo->pszFilename ) );
    if (bIsDetached)
        pszCUBFilename = CPLFormCIFilename( pszPath, detachedCub, "" );

/* -------------------------------------------------------------------- */
/*      Did we get the required keywords?  If not we return with        */
/*      this never having been considered to be a match. This isn't     */
/*      an error!                                                       */
/* -------------------------------------------------------------------- */
    if( nRows == -1 || nCols == -1 )
    {
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
    
    //printf("psztarget: %s\n", pszFilename);
    
    if( poOpenInfo->eAccess == GA_ReadOnly )
        poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "rb" );
    else
        poDS->fpImage = VSIFOpenL( poOpenInfo->pszFilename, "r+b" );

    if( poDS->fpImage == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to open %s with write permission.\n%s", 
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
            poBand->StoreNoDataValue( dfNoData );

        poDS->SetBand( i+1, poBand );
    }

/* -------------------------------------------------------------------- */
/*      Check for a .prj file. For isis2 I would like to keep this in   */
/* -------------------------------------------------------------------- */

    pszPath = CPLStrdup( CPLGetPath( poOpenInfo->pszFilename ) );
    pszName = CPLStrdup( CPLGetBasename(poOpenInfo->pszFilename) );
    const char  *pszPrjFile = CPLFormCIFilename( pszPath, pszName, "prj" );
    CPLFree( pszPath );
    CPLFree( pszName );

    fp = VSIFOpen( pszPrjFile, "r" );
    if( fp != NULL )
    {
        char	**papszLines;
        OGRSpatialReference oSRS;

        VSIFClose( fp );
        
        papszLines = CSLLoad( pszPrjFile );

        if( oSRS.importFromESRI( papszLines ) == OGRERR_NONE )
        {
            CPLFree( poDS->pszProjection );
            oSRS.exportToWkt( &(poDS->pszProjection) );
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
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );
#endif

    return( poDS );
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
    else
    {
        CSLDestroy( papszTokens );
        return pszDefault;
    }
}


/************************************************************************/
/* ==================================================================== */
/*                          NASAKeywordHandler                          */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         NASAKeywordHandler()                         */
/************************************************************************/

NASAKeywordHandler::NASAKeywordHandler()

{
    papszKeywordList = NULL;
}

/************************************************************************/
/*                        ~NASAKeywordHandler()                         */
/************************************************************************/

NASAKeywordHandler::~NASAKeywordHandler()

{
    CSLDestroy( papszKeywordList );
    papszKeywordList = NULL;
}

/************************************************************************/
/*                               Ingest()                               */
/************************************************************************/

int NASAKeywordHandler::Ingest( FILE *fp, int nOffset )

{
/* -------------------------------------------------------------------- */
/*      Read in buffer till we find END all on it's own line.           */
/* -------------------------------------------------------------------- */
    if( VSIFSeekL( fp, nOffset, SEEK_SET ) != 0 )
        return FALSE;

    for( ; TRUE; ) 
    {
        const char *pszCheck;
        char szChunk[513];

        int nBytesRead = VSIFReadL( szChunk, 1, 512, fp );

        szChunk[nBytesRead] = '\0';
        osHeaderText += szChunk;

        if( nBytesRead < 512 )
            break;

        if( osHeaderText.size() > 520 )
            pszCheck = osHeaderText.c_str() + (osHeaderText.size() - 520);
        else
            pszCheck = szChunk;

        if( strstr(pszCheck,"\r\nEND\r\n") != NULL 
            || strstr(pszCheck,"\nEND\n") != NULL )
            break;
    }

    pszHeaderNext = osHeaderText.c_str();

/* -------------------------------------------------------------------- */
/*      Process name/value pairs, keeping track of a "path stack".      */
/* -------------------------------------------------------------------- */
    return ReadGroup( "" );
}

/************************************************************************/
/*                             ReadGroup()                              */
/************************************************************************/

int NASAKeywordHandler::ReadGroup( const char *pszPathPrefix )

{
    CPLString osName, osValue;

    for( ; TRUE; )
    {
        if( !ReadPair( osName, osValue ) )
            return FALSE;

        if( osName == "OBJECT" || osName == "GROUP" )
        {
            if( !ReadGroup( (CPLString(pszPathPrefix) + osValue + ".").c_str() ) )
                return FALSE;
        }
        else if( EQUALN(osName.c_str(),"END",3) )
        {
            return TRUE;
        }
        else
        {
            osName = pszPathPrefix + osName;
            printf( "%s=%s\n", osName.c_str(), osValue.c_str() );
            papszKeywordList = CSLSetNameValue( papszKeywordList, 
                                                osName, osValue );
        }
    }
}

/************************************************************************/
/*                              ReadPair()                              */
/*                                                                      */
/*      Read a name/value pair from the input stream.  Strip off        */
/*      white space, ignore comments, split on '='.                     */
/************************************************************************/

int NASAKeywordHandler::ReadPair( CPLString &osName, CPLString &osValue )

{
    osName = "";
    osValue = "";

    if( !ReadWord( osName ) )
        return FALSE;

    SkipWhite();
    if( *pszHeaderNext != '=' )
        return FALSE;
    
    pszHeaderNext++;
    
    SkipWhite();
    
    osValue = "";

    if( *pszHeaderNext == '(' )
    {
        osValue = "";

        // TODO: Fix to capture, removing white space, honours strings.
        while( *pszHeaderNext != ')' )
        {
            if( *pszHeaderNext == '\0' )
                return FALSE;
               
            *pszHeaderNext++;
        }
        pszHeaderNext++;

        return TRUE;
    }
    else
        return ReadWord( osValue );
}

/************************************************************************/
/*                              ReadWord()                              */
/************************************************************************/

int NASAKeywordHandler::ReadWord( CPLString &osWord )

{
    osWord = "";

    SkipWhite();

    if( pszHeaderNext == '\0' )
        return FALSE;

    while( *pszHeaderNext != '\0' 
           && !isspace(*pszHeaderNext) )
    {
        if( *pszHeaderNext == '"' )
        {
            osWord += *(pszHeaderNext++);
            while( *pszHeaderNext != '"' )
            {
                if( *pszHeaderNext == '\0' )
                    return FALSE;

                osWord += *(pszHeaderNext++);
            }
            osWord += *(pszHeaderNext++);
        }
        else
        {
            osWord += *pszHeaderNext;
            pszHeaderNext++;
        }
    }
    
    return TRUE;
}

/************************************************************************/
/*                             SkipWhite()                              */
/************************************************************************/

void NASAKeywordHandler::SkipWhite()

{
    for( ; TRUE; )
    {
        // Skip white space (newline, space, tab, etc )
        if( isspace( *pszHeaderNext ) )
        {
            pszHeaderNext++; 
            continue;
        }
        
        // Skip C style comments 
        if( *pszHeaderNext == '/' && pszHeaderNext[1] == '*' )
        {
            pszHeaderNext += 2;
            
            while( *pszHeaderNext != '\0' 
                   && (*pszHeaderNext != '*' 
                       || pszHeaderNext[1] != '/' ) )
            {
                pszHeaderNext++;
            }

            pszHeaderNext += 2;
            continue;
        }

        // not white space, return. 
        return;
    }
}

/************************************************************************/
/*                             GetKeyword()                             */
/************************************************************************/

const char *NASAKeywordHandler::GetKeyword( const char *pszPath,
                                            const char *pszDefault )

{
    const char *pszResult;

    pszResult = CSLFetchNameValue( papszKeywordList, pszPath );
    if( pszResult == NULL )
        return pszDefault;
    else
        return pszResult;
}

/************************************************************************/
/*                         GDALRegister_ISIS2()                         */
/************************************************************************/

void GDALRegister_ISIS2()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "ISIS2" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "ISIS2" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "USGS Astrogeology ISIS cube (Version 2)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#ISIS2" );

        poDriver->pfnOpen = ISIS2Dataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

