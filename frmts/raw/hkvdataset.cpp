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
 * Revision 1.24  2002/11/23 18:54:17  warmerda
 * added CREATIONDATATYPES metadata for drivers
 *
 * Revision 1.23  2002/09/04 06:50:37  warmerda
 * avoid static driver pointers
 *
 * Revision 1.22  2002/06/19 18:21:08  warmerda
 * removed stat buf from GDALOpenInfo
 *
 * Revision 1.21  2002/06/12 21:12:25  warmerda
 * update to metadata based driver info
 *
 * Revision 1.20  2002/04/24 19:27:00  warmerda
 * Added HKV nodata read support (pixel.no_data).
 *
 * Revision 1.19  2001/12/12 18:15:46  warmerda
 * preliminary update for large raw file support
 *
 * Revision 1.18  2001/12/12 17:19:06  warmerda
 * Use CPLStat for directories.
 *
 * Revision 1.17  2001/12/08 04:45:59  warmerda
 * fixed south setting for UTM
 *
 * Revision 1.16  2001/11/11 23:51:00  warmerda
 * added required class keyword to friend declarations
 *
 * Revision 1.15  2001/07/18 04:51:57  warmerda
 * added CPL_CVSID
 *
 * Revision 1.14  2001/07/11 18:09:06  warmerda
 * Correct corner GCPs. BOTTOM_RIGHT actually refers to the top left corner of
 * the bottom right pixel!
 *
 * Revision 1.13  2001/06/20 16:10:02  warmerda
 * overhauled to TIFF style overviews
 *
 * Revision 1.12  2001/06/13 19:42:10  warmerda
 * Changed blob->image_data, and HKV to MFF2.
 *
 * Revision 1.11  2000/12/14 17:34:13  warmerda
 * Added dataset delete method.
 *
 * Revision 1.10  2000/12/05 22:40:09  warmerda
 * Added very limited SetProjection support, includes Bessel
 *
 * Revision 1.9  2000/11/29 21:31:28  warmerda
 * added support for writing georef on SetGeoTransform
 *
 * Revision 1.8  2000/08/16 15:51:39  warmerda
 * added support for reading overviews
 *
 * Revision 1.7  2000/08/15 19:28:26  warmerda
 * added help topic
 *
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

CPL_CVSID("$Id$");

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
    friend class HKVDataset;

  public:
    		HKVRasterBand( HKVDataset *poDS, int nBand, FILE * fpRaw, 
                               unsigned int nImgOffset, int nPixelOffset,
                               int nLineOffset,
                               GDALDataType eDataType, int bNativeOrder );
    virtual     ~HKVRasterBand();
};

/************************************************************************/
/* ==================================================================== */
/*				HKVDataset				*/
/* ==================================================================== */
/************************************************************************/

class HKVDataset : public RawDataset
{
    friend class HKVRasterBand;

    char	*pszPath;
    FILE	*fpBlob;

    int         nGCPCount;
    GDAL_GCP    *pasGCPList;

    void        ProcessGeoref(const char *);
    void        ProcessGeorefGCP(char **, const char *, double, double);

    char        *pszProjection;
    double      adfGeoTransform[6];

    char	**papszAttrib;

    int		bGeorefChanged;
    char	**papszGeoref;
    
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

HKVRasterBand::HKVRasterBand( HKVDataset *poDS, int nBand, FILE * fpRaw, 
                              unsigned int nImgOffset, int nPixelOffset,
                              int nLineOffset,
                              GDALDataType eDataType, int bNativeOrder )
        : RawRasterBand( (GDALDataset *) poDS, nBand, 
                         fpRaw, nImgOffset, nPixelOffset, 
                         nLineOffset, eDataType, bNativeOrder, TRUE )

{
    this->poDS = poDS;
    this->nBand = nBand;
    
    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;
}

/************************************************************************/
/*                           ~HKVRasterBand()                           */
/************************************************************************/

HKVRasterBand::~HKVRasterBand()

{
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
    pszPath = NULL;
    papszAttrib = NULL;
    papszGeoref = NULL;
    bGeorefChanged = FALSE;

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

    if( bGeorefChanged )
    {
        const char	*pszFilename;

        pszFilename = CPLFormFilename(pszPath, "georef", NULL );

        CSLSave( papszGeoref, pszFilename );
    }

    if( fpBlob != NULL )
        VSIFCloseL( fpBlob );

    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }

    CPLFree( pszProjection );

    CPLFree( pszPath );
    CSLDestroy( papszGeoref );
    CSLDestroy( papszAttrib );
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
    char	szValue[128];

    memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );

/* -------------------------------------------------------------------- */
/*      top left                                                        */
/* -------------------------------------------------------------------- */
    sprintf( szValue, "%.10f", padfTransform[3] );
    papszGeoref = CSLSetNameValue( papszGeoref, "top_left.latitude", 
                                   szValue );

    sprintf( szValue, "%.10f", padfTransform[0] );
    papszGeoref = CSLSetNameValue( papszGeoref, "top_left.longitude", 
                                   szValue );

/* -------------------------------------------------------------------- */
/*      top_right                                                       */
/* -------------------------------------------------------------------- */
    sprintf( szValue, "%.10f", padfTransform[3] );
    papszGeoref = CSLSetNameValue( papszGeoref, "top_right.latitude", 
                                   szValue );

    sprintf( szValue, "%.10f", 
             padfTransform[0] + GetRasterXSize() * padfTransform[1] );
    papszGeoref = CSLSetNameValue( papszGeoref, "top_right.longitude", 
                                   szValue );

/* -------------------------------------------------------------------- */
/*      bottom_left                                                     */
/* -------------------------------------------------------------------- */
    sprintf( szValue, "%.10f", 
             padfTransform[3] + GetRasterYSize() * padfTransform[5] );
    papszGeoref = CSLSetNameValue( papszGeoref, "bottom_left.latitude", 
                                   szValue );

    sprintf( szValue, "%.10f", padfTransform[0] );
    papszGeoref = CSLSetNameValue( papszGeoref, "bottom_left.longitude", 
                                   szValue );

/* -------------------------------------------------------------------- */
/*      bottom_right                                                    */
/* -------------------------------------------------------------------- */
    sprintf( szValue, "%.10f", 
             padfTransform[3] + GetRasterYSize() * padfTransform[5] );
    papszGeoref = CSLSetNameValue( papszGeoref, "bottom_right.latitude", 
                                   szValue );

    sprintf( szValue, "%.10f", 
             padfTransform[0] + GetRasterXSize() * padfTransform[1] );
    papszGeoref = CSLSetNameValue( papszGeoref, "bottom_right.longitude", 
                                   szValue );

/* -------------------------------------------------------------------- */
/*      Center                                                          */
/* -------------------------------------------------------------------- */
    sprintf( szValue, "%.10f", 
             padfTransform[3] + GetRasterYSize() * padfTransform[5] * 0.5 );
    papszGeoref = CSLSetNameValue( papszGeoref, "centre.latitude", 
                                   szValue );

    sprintf( szValue, "%.10f", 
             padfTransform[0] + GetRasterXSize() * padfTransform[1] * 0.5 );
    papszGeoref = CSLSetNameValue( papszGeoref, "centre.longitude", 
                                   szValue );

/* -------------------------------------------------------------------- */
/*      Set projection to LL if not previously set.                     */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue( papszGeoref, "projection.name" ) == NULL )
    {
        papszGeoref = CSLSetNameValue( papszGeoref, "projection.name", "LL" );
        papszGeoref = CSLSetNameValue( papszGeoref, "spheroid.name", 
                                       "ev-wgs-84" );
    }

    bGeorefChanged = TRUE;

    return( CE_None );
}

/************************************************************************/
/*                           SetProjection()                            */
/*                                                                      */
/*      We provide very limited support for setting the projection.     */
/*      Currently all that is supported is controlling whether the      */
/*      datum is WGS84 (the default), or Bessel Ellipsoid (as is        */
/*      found with Japanese DEM data with Tokyo datum).                 */
/************************************************************************/

CPLErr HKVDataset::SetProjection( const char * pszProjection )

{
    printf( "HKVDataset::SetProjection(%s)\n", pszProjection );

    papszGeoref = CSLSetNameValue( papszGeoref, "projection.name", "LL" );

    if( strstr(pszProjection,"Bessel") != NULL )
    {
        papszGeoref = CSLSetNameValue( papszGeoref, "spheroid.name", 
                                       "ev-bessel" );
    }
    else
    {
        papszGeoref = CSLSetNameValue( papszGeoref, "spheroid.name", 
                                       "ev-wgs-84" );
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
    int   i;

/* -------------------------------------------------------------------- */
/*      Load the georef file, and boil white space away from around     */
/*      the equal sign.                                                 */
/* -------------------------------------------------------------------- */
    CSLDestroy( papszGeoref );
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
                      GetRasterXSize()-1, 0 );
    ProcessGeorefGCP( papszGeoref, "bottom_left", 
                      0, GetRasterYSize()-1 );
    ProcessGeorefGCP( papszGeoref, "bottom_right", 
                      GetRasterXSize()-1, GetRasterYSize()-1 );
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

        if( pasGCPList[4].dfGCPY < 0 )
            oUTM.SetUTM( nZone, 0 );
        else
            oUTM.SetUTM( nZone, 1 );

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
            adfGeoTransform[1] = (dfUtmLRX - dfUtmULX) / (GetRasterXSize()-1);
            adfGeoTransform[2] = 0.0;
            adfGeoTransform[3] = dfUtmULY;
            adfGeoTransform[4] = 0.0;
            adfGeoTransform[5] = (dfUtmLRY - dfUtmULY) / (GetRasterYSize()-1);
        }

        if( poTransform != NULL )
            delete poTransform;
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *HKVDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i, bNoDataSet = FALSE;
    double      dfNoDataValue = 0.0;
    char        **papszAttrib;
    const char  *pszFilename, *pszValue;
    VSIStatBuf  sStat;
    
/* -------------------------------------------------------------------- */
/*      We assume the dataset is passed as a directory.  Check for      */
/*      an attrib and blob file as a minimum.                           */
/* -------------------------------------------------------------------- */
    if( !poOpenInfo->bIsDirectory )
        return NULL;
    
    pszFilename = CPLFormFilename(poOpenInfo->pszFilename, "image_data", NULL);
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

    poDS->pszPath = CPLStrdup( poOpenInfo->pszFilename );
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

    pszValue = CSLFetchNameValue(papszAttrib,"pixel.no_data");
    if( pszValue != NULL )
    {
        bNoDataSet = TRUE;
        dfNoDataValue = atof(pszValue);
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
    char	*pszOvrFilename = (char *) CPLMalloc(strlen(pszFilename)+5);

    sprintf( pszOvrFilename, "%s_ovr", pszFilename );

/* -------------------------------------------------------------------- */
/*      Define the bands.                                               */
/* -------------------------------------------------------------------- */
    int    nPixelOffset, nLineOffset, nOffset;

    nPixelOffset = nRawBands * nSize;
    nLineOffset = nPixelOffset * poDS->GetRasterXSize();
    nOffset = 0;

    for( int iRawBand=0; iRawBand < nRawBands; iRawBand++ )
    {
        HKVRasterBand *poBand;

        poBand = 
            new HKVRasterBand( poDS, poDS->GetRasterCount()+1, poDS->fpBlob,
                               nOffset, nPixelOffset, nLineOffset, 
                               eType, bNative );
        poDS->SetBand( poDS->GetRasterCount()+1, poBand );
        nOffset += GDALGetDataTypeSize( eType ) / 8;

        if( bNoDataSet )
            poBand->StoreNoDataValue( dfNoDataValue );
    }

/* -------------------------------------------------------------------- */
/*      Process the georef file if there is one.                        */
/* -------------------------------------------------------------------- */
    pszFilename = CPLFormFilename(poDS->pszPath, "georef", NULL );
    if( VSIStat(pszFilename,&sStat) == 0 )
        poDS->ProcessGeoref(pszFilename);

/* -------------------------------------------------------------------- */
/*      Handle overviews.                                               */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, pszOvrFilename, TRUE );

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

    if( CPLStat( pszBaseDir, &sStat ) != 0 || !VSI_ISDIR( sStat.st_mode ) )
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
    pszFilename = CPLFormFilename( pszFilenameIn, "image_data", NULL );
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
/*                               Delete()                               */
/*                                                                      */
/*      An HKV Blob dataset consists of a bunch of files in a           */
/*      directory.  Try to delete all the files, then the               */
/*      directory.                                                      */
/************************************************************************/

CPLErr HKVDataset::Delete( const char * pszName )

{
    VSIStatBuf	sStat;
    char        **papszFiles;
    int         i;

    if( CPLStat( pszName, &sStat ) != 0 
        || !VSI_ISDIR(sStat.st_mode) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s does not appear to be an HKV Dataset, as it is not\n"
                  "a path to a directory.", 
                  pszName );
        return CE_Failure;
    }

    papszFiles = CPLReadDir( pszName );
    for( i = 0; i < CSLCount(papszFiles); i++ )
    {
        const char *pszTarget;

        if( EQUAL(papszFiles[i],".") || EQUAL(papszFiles[i],"..") )
            continue;

        pszTarget = CPLFormFilename(pszName, papszFiles[i], NULL );
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
/*                         GDALRegister_HKV()                          */
/************************************************************************/

void GDALRegister_HKV()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "MFF2" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "MFF2" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Atlantis MFF2 (HKV) Raster" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#MFF2" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16 Int32 UInt32 CInt16 CInt32 Float32 Float64 CFloat32 CFloat64" );
        
        poDriver->pfnOpen = HKVDataset::Open;
        poDriver->pfnCreate = HKVDataset::Create;
        poDriver->pfnDelete = HKVDataset::Delete;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
