/******************************************************************************
 * $Id$
 *
 * Project:  GView
 * Purpose:  Implementation of Atlantis HKV labelled blob support
 * Author:   Frank Warmerdam, warmerda@home.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam
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
 * Revision 1.6  2000/07/12 19:21:59  warmerda
 * added support for _reading_ georef file
 *
 * Revision 1.5  2000/06/05 17:24:06  warmerda
 * added real complex support
 *
 * Revision 1.4  2000/05/15 14:18:27  warmerda
 * added COMPLEX_INTERPRETATION metadata
 *
 * Revision 1.3  2000/04/05 19:28:48  warmerda
 * Fixed MSB case.
 *
 * Revision 1.2  2000/03/13 14:34:42  warmerda
 * avoid const problem on write
 *
 * Revision 1.1  2000/03/07 21:33:42  warmerda
 * New
 *
 */

#include "rawdataset.h"
#include "cpl_string.h"
#include <ctype.h>
#include "ogr_spatialref.h"

static GDALDriver	*poHKVDriver = NULL;

CPL_C_START
void	GDALRegister_HKV(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*                            HKVRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class HKVDataset;

class HKVRasterBand : public RawRasterBand
{
    friend	HKVDataset;

    int         nOverviews;
    RawRasterBand *papoOverviewBands;

  public:
    		HKVRasterBand( HKVDataset *poDS, int nBand, FILE * fpRaw, 
                               unsigned int nImgOffset, int nPixelOffset,
                               int nLineOffset,
                               GDALDataType eDataType, int bNativeOrder );
};

/************************************************************************/
/* ==================================================================== */
/*				HKVDataset				*/
/* ==================================================================== */
/************************************************************************/

class HKVDataset : public RawDataset
{
    FILE	*fpBlob;

    int         nOverviews;
    int         *panOverviewLevel;
    FILE        **pafpOverviewBlob;

    int         nGCPCount;
    GDAL_GCP    *pasGCPList;

    void        ProcessGeoref(const char *);
    void        ProcessGeorefGCP(char **, const char *, double, double);

    char        *pszProjection;
    double      adfGeoTransform[6];

  protected:    
    virtual CPLErr IBuildOverviews( const char *, int, int *,
                                    int, int *, GDALProgressFunc, void * );
  public:
    		HKVDataset();
    	        ~HKVDataset();
    
    char	**papszAttrib;
    
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    virtual const char *GetProjectionRef(void);
    virtual CPLErr GetGeoTransform( double * );

    static GDALDataset *Open( GDALOpenInfo * );
    static GDALDataset *Create( const char * pszFilename,
                                int nXSize, int nYSize, int nBands,
                                GDALDataType eType, char ** papszParmList );
};

/************************************************************************/
/* ==================================================================== */
/*                            HKVRasterBand                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           HKVRasterBand()                            */
/************************************************************************/

HKVRasterBand::HKVRasterBand( HKVDataset *poDS, int nBand, FILE * fpRaw, 
                              unsigned int nImgOffset, int nPixelOffset,
                              int nLineOffset,
                              GDALDataType eDataType, int bNativeOrder )
        : RawRasterBand( (GDALDataset *) poDS, nBand, 
                         fpRaw, nImgOffset, nPixelOffset, 
                         nLineOffset, eDataType, bNativeOrder )

{
    this->poDS = poDS;
    this->nBand = nBand;
    
    eDataType = GDT_Byte;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/* ==================================================================== */
/*				HKVDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            HKVDataset()                             */
/************************************************************************/

HKVDataset::HKVDataset()
{
    papszAttrib = NULL;
    nGCPCount = 0;
    pasGCPList = NULL;
    pszProjection = CPLStrdup("");
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
    CSLDestroy( papszAttrib );
    if( fpBlob != NULL )
        VSIFClose( fpBlob );

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    CPLFree( pszProjection );
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr HKVDataset::IBuildOverviews( const char * pszResample, 
                                    int nOverviews, int * panOverviewList, 
                                    int nBands, int * panBandList, 
                                    GDALProgressFunc pfnProgress, 
                                    void * pProgressData )

{
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
    if( nGCPCount > 0 )
        return "GEOGCS[\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563,AUTHORITY[\"EPSG\",7030]],TOWGS84[0,0,0,0,0,0,0],AUTHORITY[\"EPSG\",6326]],PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",8901]],UNIT[\"DMSH\",0.0174532925199433,AUTHORITY[\"EPSG\",9108]],AXIS[\"Lat\",NORTH],AXIS[\"Long\",EAST],AUTHORITY[\"EPSG\",4326]]";
    else
        return "";
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

void HKVDataset::ProcessGeorefGCP( char **papszGeoref, const char *pszBase,
                                   double dfRasterX, double dfRasterY )

{
    char      szFieldName[128];
    double    dfLat, dfLong;

/* -------------------------------------------------------------------- */
/*      Fetch the GCP from the string list.                             */
/* -------------------------------------------------------------------- */
    sprintf( szFieldName, "%s.latitude", pszBase );
    if( CSLFetchNameValue(papszGeoref, szFieldName) == NULL )
        return;
    else
        dfLat = atof(CSLFetchNameValue(papszGeoref, szFieldName));

    sprintf( szFieldName, "%s.longitude", pszBase );
    if( CSLFetchNameValue(papszGeoref, szFieldName) == NULL )
        return;
    else
        dfLong = atof(CSLFetchNameValue(papszGeoref, szFieldName));

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
    char  **papszGeoref;
    int   i;

/* -------------------------------------------------------------------- */
/*      Load the georef file, and boil white space away from around     */
/*      the equal sign.                                                 */
/* -------------------------------------------------------------------- */
    papszGeoref = CSLLoad( pszFilename );
    if( papszGeoref == NULL )
        return;

    for( i = 0; papszGeoref[i] != NULL; i++ )
    {
        int       bAfterEqual = FALSE;
        int       iSrc, iDst;
        char     *pszLine = papszGeoref[i];

        for( iSrc=0, iDst=0; pszLine[iSrc] != '\0'; iSrc++ )
        {
            if( bAfterEqual || pszLine[iSrc] != ' ' )
            {
                pszLine[iDst++] = pszLine[iSrc];
            }

            if( iDst > 0 && pszLine[iDst-1] == '=' )
                bAfterEqual = FALSE;
        }
        pszLine[iDst] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Try to get GCPs.                                                */
/* -------------------------------------------------------------------- */
    nGCPCount = 0;
    pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),5);

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

/* -------------------------------------------------------------------- */
/*      Do we have a recognised projection?                             */
/* -------------------------------------------------------------------- */
    const char *pszProjName, *pszOriginLong;

    pszProjName = CSLFetchNameValue(papszGeoref, 
                                    "projection.name");
    pszOriginLong = CSLFetchNameValue(papszGeoref, 
                                      "projection.origin_longitude");

    if( pszProjName != NULL && EQUAL(pszProjName,"utm") 
        && pszOriginLong != NULL && nGCPCount == 5 )
    {
        int nZone = (int)((atof(pszOriginLong)+184.5) / 6.0);
        OGRSpatialReference oUTM;
        OGRSpatialReference oLL;
        OGRCoordinateTransformation *poTransform = NULL;
        double dfUtmULX, dfUtmULY, dfUtmLRX, dfUtmLRY;
        int    bSuccess = TRUE;

        oUTM.SetUTM( nZone );
        oUTM.SetWellKnownGeogCS( "WGS84" );

        oLL.SetWellKnownGeogCS( "WGS84" );
        
        poTransform = OGRCreateCoordinateTransformation( &oLL, &oUTM );
        if( poTransform == NULL )
            bSuccess = FALSE;

        dfUtmULX = pasGCPList[0].dfGCPX;
        dfUtmULY = pasGCPList[0].dfGCPY;
        if( bSuccess && !poTransform->Transform( 1, &dfUtmULX, &dfUtmULY ) )
            bSuccess = FALSE;

        dfUtmLRX = pasGCPList[3].dfGCPX;
        dfUtmLRY = pasGCPList[3].dfGCPY;
        if( bSuccess && !poTransform->Transform( 1, &dfUtmLRX, &dfUtmLRY ) )
            bSuccess = FALSE;

        if( bSuccess )
        {
            CPLFree( pszProjection );
            pszProjection = NULL;
            oUTM.exportToWkt( &pszProjection );
            
            adfGeoTransform[0] = dfUtmULX;
            adfGeoTransform[1] = (dfUtmLRX - dfUtmULX) / GetRasterXSize();
            adfGeoTransform[2] = 0.0;
            adfGeoTransform[3] = dfUtmULY;
            adfGeoTransform[4] = 0.0;
            adfGeoTransform[5] = (dfUtmLRY - dfUtmULY) / GetRasterYSize();
        }

        if( poTransform != NULL )
            delete poTransform;
    }
    
/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszGeoref );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *HKVDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i;
    char        **papszAttrib;
    const char  *pszFilename, *pszValue;
    VSIStatBuf  sStat;
    
/* -------------------------------------------------------------------- */
/*      We assume the dataset is passed as a directory.  Check for      */
/*      an attrib and blob file as a minimum.                           */
/* -------------------------------------------------------------------- */
    if( !poOpenInfo->bStatOK || !VSI_ISDIR(poOpenInfo->sStat.st_mode) )
        return NULL;
    
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
    papszAttrib = CSLLoad( pszFilename );
    if( papszAttrib == NULL )
        return NULL;

    for( i = 0; papszAttrib[i] != NULL; i++ )
    {
        int       bAfterEqual = FALSE;
        int       iSrc, iDst;
        char     *pszLine = papszAttrib[i];

        for( iSrc=0, iDst=0; pszLine[iSrc] != '\0'; iSrc++ )
        {
            if( bAfterEqual || pszLine[iSrc] != ' ' )
            {
                pszLine[iDst++] = pszLine[iSrc];
            }

            if( iDst > 0 && pszLine[iDst-1] == '=' )
                bAfterEqual = FALSE;
        }
        pszLine[iDst] = '\0';
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    HKVDataset 	*poDS;

    poDS = new HKVDataset();

    poDS->poDriver = poHKVDriver;
    poDS->papszAttrib = papszAttrib;
    
/* -------------------------------------------------------------------- */
/*      Set some dataset wide information.                              */
/* -------------------------------------------------------------------- */
    int bNative, bComplex;
    int nRawBands = 0;

    if( CSLFetchNameValue( papszAttrib, "extent.cols" ) == NULL 
        || CSLFetchNameValue( papszAttrib, "extent.rows" ) == NULL )
        return NULL;

    poDS->RasterInitialize( 
        atoi(CSLFetchNameValue(papszAttrib,"extent.cols")),
        atoi(CSLFetchNameValue(papszAttrib,"extent.rows")) );

    pszValue = CSLFetchNameValue(papszAttrib,"pixel.order");
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

    pszValue = CSLFetchNameValue(papszAttrib,"channel.enumeration");
    if( pszValue != NULL )
        nRawBands = atoi(pszValue);
    else
        nRawBands = 1;

    pszValue = CSLFetchNameValue(papszAttrib,"pixel.field");
    if( pszValue != NULL && strstr(pszValue,"*complex") != NULL )
        bComplex = TRUE;
    else
        bComplex = FALSE;

/* -------------------------------------------------------------------- */
/*      Figure out the datatype                                         */
/* -------------------------------------------------------------------- */
    const char * pszEncoding;
    int          nSize = 1;
    int          nPseudoBands;
    GDALDataType eType;

    pszEncoding = CSLFetchNameValue(papszAttrib,"pixel.encoding");
    if( pszEncoding == NULL )
        pszEncoding = "{ *unsigned }";

    if( CSLFetchNameValue(papszAttrib,"pixel.size") != NULL )
        nSize = atoi(CSLFetchNameValue(papszAttrib,"pixel.size"))/8;
        
    if( bComplex )
        nPseudoBands = 2;
    else 
        nPseudoBands = 1;

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
                  poOpenInfo->pszFilename, nSize, pszEncoding );
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open the blob file.                                             */
/* -------------------------------------------------------------------- */
    pszFilename = CPLFormFilename(poOpenInfo->pszFilename, "blob", NULL );
    if( poOpenInfo->eAccess == GA_ReadOnly )
    {
        poDS->fpBlob = VSIFOpen( pszFilename, "rb" );
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
        poDS->fpBlob = VSIFOpen( pszFilename, "rb+" );
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
/*      Define the bands.                                               */
/* -------------------------------------------------------------------- */
    int    nPixelOffset, nLineOffset, nOffset;

    nPixelOffset = nRawBands * nSize;
    nLineOffset = nPixelOffset * poDS->GetRasterXSize();
    nOffset = 0;

    for( int iRawBand=0; iRawBand < nRawBands; iRawBand++ )
    {
        poDS->SetBand( poDS->GetRasterCount()+1, 
            new RawRasterBand( poDS, poDS->GetRasterCount()+1, poDS->fpBlob,
                               nOffset, nPixelOffset, nLineOffset, 
                               eType, bNative ) );
        nOffset += GDALGetDataTypeSize( eType ) / 8;
    }

/* -------------------------------------------------------------------- */
/*      Process the georef file if there is one.                        */
/* -------------------------------------------------------------------- */
    pszFilename = CPLFormFilename(poOpenInfo->pszFilename, "georef", NULL );
    if( VSIStat(pszFilename,&sStat) == 0 )
        poDS->ProcessGeoref(pszFilename);

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
    if( eType != GDT_Byte && eType != GDT_Float32 
        && eType != GDT_UInt16 && eType != GDT_Int16 
        && eType != GDT_CInt16 && eType != GDT_CInt32
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
    VSIStatBuf  sStat;

    if( strlen(CPLGetPath(pszFilenameIn)) == 0 )
        pszBaseDir = CPLStrdup(".");
    else
        pszBaseDir = CPLStrdup(CPLGetPath(pszFilenameIn));

    if( VSIStat( pszBaseDir, &sStat ) != 0 || !VSI_ISDIR( sStat.st_mode ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to create HKV dataset under %s,\n"
                  "but this is not a valid directory.\n", 
                  pszBaseDir);
        CPLFree( pszBaseDir );
        return NULL;
    }

    if( VSIMkdir( pszFilenameIn, 0755 ) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create directory %s.\n", 
                  pszFilenameIn );
        return NULL;
    }

    CPLFree( pszBaseDir );

/* -------------------------------------------------------------------- */
/*      Create the header file.                                         */
/* -------------------------------------------------------------------- */
    FILE       *fp;
    const char *pszFilename;

    pszFilename = CPLFormFilename( pszFilenameIn, "attrib", NULL );

    fp = VSIFOpen( pszFilename, "wt" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Couldn't create %s.\n", pszFilename );
        return NULL;
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
    VSIFClose( fp );
   
/* -------------------------------------------------------------------- */
/*      Create the blob file.                                           */
/* -------------------------------------------------------------------- */
    pszFilename = CPLFormFilename( pszFilenameIn, "blob", NULL );
    fp = VSIFOpen( pszFilename, "wb" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Couldn't create %s.\n", pszFilename );
        return NULL;
    }
    
    VSIFWrite( (void*)"", 1, 1, fp );
    VSIFClose( fp );

/* -------------------------------------------------------------------- */
/*      Open the dataset normally.                                      */
/* -------------------------------------------------------------------- */
    return (GDALDataset *) GDALOpen( pszFilenameIn, GA_Update );
}

/************************************************************************/
/*                         GDALRegister_HKV()                          */
/************************************************************************/

void GDALRegister_HKV()

{
    GDALDriver	*poDriver;

    if( poHKVDriver == NULL )
    {
        poHKVDriver = poDriver = new GDALDriver();
        
        poDriver->pszShortName = "HKV";
        poDriver->pszLongName = "Atlantis HKV Raster";
        
        poDriver->pfnOpen = HKVDataset::Open;
         poDriver->pfnCreate = HKVDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
