/******************************************************************************
 * $Id$
 *
 * Project:  EOSAT FAST Format reader
 * Purpose:  Reads Landsat FAST-L7A
 * Author:   Andrey Kiselev, dron@at1895.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@at1895.spb.edu>
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
 * Revision 1.3  2002/09/04 06:50:37  warmerda
 * avoid static driver pointers
 *
 * Revision 1.2  2002/08/15 09:35:50  dron
 * Fixes in georeferencing
 *
 * Revision 1.1  2002/08/13 16:55:41  dron
 * Initial release
 *
 *
 */

#include "gdal_priv.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"
#include "../raw/rawdataset.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_FAST(void);
CPL_C_END

#define FAST_FILENAME_SIZE	29
#define ADM_HEADER_SIZE		4608

/************************************************************************/
/* ==================================================================== */
/*				FASTDataset				*/
/* ==================================================================== */
/************************************************************************/

class FASTDataset : public GDALDataset
{
    friend class FASTRasterBand;

    double      adfGeoTransform[6];
    char        *pszProjection;

    FILE	*fpHeader;
    FILE	*fpChannels[6];
    char	*pszDirname;
    GDALDataType eDataType;

    void	ComputeGeoref();
    
  public:
                FASTDataset();
		~FASTDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );

    CPLErr 	GetGeoTransform( double * padfTransform );
    const char *GetProjectionRef();

};

/************************************************************************/
/* ==================================================================== */
/*                            FASTRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class FASTRasterBand : public RawRasterBand
{
    friend class FASTDataset;

  public:

    		FASTRasterBand( FASTDataset *, int, FILE *, vsi_l_offset,
				int, int, GDALDataType, int );
};


/************************************************************************/
/*                           FASTRasterBand()                            */
/************************************************************************/

FASTRasterBand::FASTRasterBand( FASTDataset *poDS, int nBand, FILE * fpRaw,
                                vsi_l_offset nImgOffset, int nPixelOffset,
                                int nLineOffset, GDALDataType eDataType,
				int bNativeOrder) :
                 RawRasterBand( poDS, nBand, fpRaw, nImgOffset, nPixelOffset,
                               nLineOffset, eDataType, bNativeOrder, FALSE)
{

}

/************************************************************************/
/* ==================================================================== */
/*				FASTDataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           FASTDataset()                           */
/************************************************************************/

FASTDataset::FASTDataset()

{
    fpHeader = NULL;
    pszProjection = "";
    nBands = 0;
}

/************************************************************************/
/*                            ~FASTDataset()                         */
/************************************************************************/

FASTDataset::~FASTDataset()

{
    int i;
    for ( i = 0; i < nBands; i++ )
	VSIFClose( fpChannels[i] );
    if( fpHeader != NULL )
        VSIFClose( fpHeader );
    CPLFree( pszDirname );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr FASTDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *FASTDataset::GetProjectionRef()

{
        return pszProjection;
}

/*double ToDouble( char *pszString, int iLength )
{
    
    char *pszDouble = (char *)CPLMalloc( iLength + 1 );
    memcpy( pszDouble, pszString, iLength );
    pszDouble[iLength] = '\0';
    return atof(pszDouble);
}

double ToDegrees( char *pszString )
{
    double dfDegrees;
    
    dfDegrees = ToDouble( pszString, 3) +
	ToDouble( pszString + 3, 2) / 60.0 +
	ToDouble( pszString + 5, 7) / 3600.0;
    
    switch ( pszString[13] )
    {
        case 'W':
	case 'S':
	dfDegrees = -dfDegrees;
	break;
	default:
	break;
    }
    
    return dfDegrees;
}*/

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *FASTDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i;
    
    char        *pszPrefixName = (char *)CPLMalloc(23);
	
    if( poOpenInfo->fp == NULL ||
            strlen(poOpenInfo->pszFilename) < FAST_FILENAME_SIZE )
        return NULL;

    if ( !EQUALN(poOpenInfo->pszFilename + strlen(poOpenInfo->pszFilename) - 4,
                ".FST", 4) )
	return NULL;

    if( !EQUALN((const char *) poOpenInfo->pabyHeader, "REQ ID =", 8) &&
	!EQUALN((const char *) poOpenInfo->pabyHeader + 80, "SATELLITE =", 11) && 
	!EQUALN((const char *) poOpenInfo->pabyHeader + 101, " SENSOR =", 9) && 
	!EQUALN((const char *) poOpenInfo->pabyHeader + 183, " LOCATION =", 11) )
        return NULL;
    
    strncpy( pszPrefixName, poOpenInfo->pszFilename +
	    strlen(poOpenInfo->pszFilename) - FAST_FILENAME_SIZE, 22);
    pszPrefixName[22] = '\0';
	
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    FASTDataset	*poDS;

    poDS = new FASTDataset();

    poDS->fpHeader = poOpenInfo->fp;
    poOpenInfo->fp = NULL;
    poDS->pszDirname = CPLStrdup( CPLGetDirname( poOpenInfo->pszFilename ) );
    
/* -------------------------------------------------------------------- */
/*      Read the administrative header.                                 */
/* -------------------------------------------------------------------- */
    char	*pszHeader;
    const char	*pszChannelFilename;
    char	pszTempName[FAST_FILENAME_SIZE + 1];
    
    pszHeader = (char *)CPLMalloc( ADM_HEADER_SIZE );
    VSIFSeek( poDS->fpHeader, 0, SEEK_SET );
    VSIFRead( pszHeader, 1, ADM_HEADER_SIZE, poDS->fpHeader );

    pszTempName[FAST_FILENAME_SIZE] = '\0';
    poDS->nBands = 0;
    if ( pszHeader[1130] != ' ' )
    {
	memcpy( pszTempName, pszHeader + 1130, FAST_FILENAME_SIZE );
	pszChannelFilename = CPLFormFilename( poDS->pszDirname, pszTempName, NULL );
        poDS->fpChannels[poDS->nBands] = VSIFOpen( pszChannelFilename, "rb" );
	if ( poDS->fpChannels[poDS->nBands] )
	    poDS->nBands++;
    }
    if ( pszHeader[1169] != ' ' )
    {
	memcpy( pszTempName, pszHeader + 1169, FAST_FILENAME_SIZE );
	pszChannelFilename =
	    CPLStrdup( CPLFormFilename( poDS->pszDirname, pszTempName, NULL ) );
	poDS->fpChannels[poDS->nBands] = VSIFOpen( pszChannelFilename, "rb" );
	if ( poDS->fpChannels[poDS->nBands] )
	    poDS->nBands++;
    }
    if ( pszHeader[1210] != ' ' )
    {
	memcpy( pszTempName, pszHeader + 1210, FAST_FILENAME_SIZE );
	pszChannelFilename =
	    CPLStrdup( CPLFormFilename( poDS->pszDirname, pszTempName, NULL ) );
        poDS->fpChannels[poDS->nBands] = VSIFOpen( pszChannelFilename, "rb" );
	if ( poDS->fpChannels[poDS->nBands] )
	    poDS->nBands++;
    }
    if ( pszHeader[1249] != ' ' )
    {
	memcpy( pszTempName, pszHeader + 1249, FAST_FILENAME_SIZE );
	pszChannelFilename =
	    CPLStrdup( CPLFormFilename( poDS->pszDirname, pszTempName, NULL ) );
        poDS->fpChannels[poDS->nBands] = VSIFOpen( pszChannelFilename, "rb" );
	if ( poDS->fpChannels[poDS->nBands] )
	    poDS->nBands++;
    }
    if ( pszHeader[1290] != ' ' )
    {
	memcpy( pszTempName, pszHeader + 1290, FAST_FILENAME_SIZE );
	pszChannelFilename =
	    CPLStrdup( CPLFormFilename( poDS->pszDirname, pszTempName, NULL ) );
        poDS->fpChannels[poDS->nBands] = VSIFOpen( pszChannelFilename, "rb" );
	if ( poDS->fpChannels[poDS->nBands] )
	    poDS->nBands++;
    }
    if ( pszHeader[1329] != ' ' )
    {
	memcpy( pszTempName, pszHeader + 1329, FAST_FILENAME_SIZE );
	pszChannelFilename =
	    CPLStrdup( CPLFormFilename( poDS->pszDirname, pszTempName, NULL ) );
        poDS->fpChannels[poDS->nBands] = VSIFOpen( pszChannelFilename, "rb" );
	if ( poDS->fpChannels[poDS->nBands] )
	    poDS->nBands++;
    }

    if ( !poDS->nBands )
	return NULL;
    
    poDS->nRasterXSize = atoi( pszHeader + 842 );
    poDS->nRasterYSize = atoi( pszHeader + 870 );

    switch( atoi( pszHeader + 983 ) )
    {
        default:
	case 8:
	poDS->eDataType = GDT_Byte;
	break;
    }

    // Read geometric record
    OGRSpatialReference oSRS;
    int		iUTMZone;
    // Coordinates of corner pixel's centers
    double	dfULX = 0.5, dfULY = 0.5;
    double	dfURX = poDS->nRasterXSize - 0.5, dfURY = 0.5;
    double	dfLLX = 0.5, dfLLY = poDS->nRasterYSize - 0.5;
    double	dfLRX = poDS->nRasterXSize - 0.5, dfLRY = poDS->nRasterYSize - 0.5;
    
    if ( EQUALN(pszHeader + 3145, "WGS84", 5) )
	oSRS.SetWellKnownGeogCS( "WGS84" );
    
    if ( EQUALN(pszHeader + 3103, "UTM", 3) )
    {
	oSRS.SetProjCS( "UTM" );
        iUTMZone = atoi( pszHeader + 3592 );
        if( *(pszHeader + 3662) == 'N' )	// North hemisphere
	    oSRS.SetUTM( iUTMZone, TRUE );
        else					// South hemisphere
	    oSRS.SetUTM( iUTMZone, FALSE );
	// UTM coordinates
        dfULX = atof( pszHeader + 3664 );
        dfULY = atof( pszHeader + 3678 );
        dfURX = atof( pszHeader + 3744 );
        dfURY = atof( pszHeader + 3758 );
        dfLRX = atof( pszHeader + 3824 );
        dfLRY = atof( pszHeader + 3838 );
        dfLLX = atof( pszHeader + 3904 );
        dfLLY = atof( pszHeader + 3918 );
    }
    
    oSRS.exportToWkt( &poDS->pszProjection );

    poDS->adfGeoTransform[1] = (dfURX - dfLLX) / (poDS->nRasterXSize - 1);
    if( *(pszHeader + 3662) == 'N' )
	poDS->adfGeoTransform[5] = (dfURY - dfLLY) / (poDS->nRasterYSize - 1);
    else	    
	poDS->adfGeoTransform[5] =  (dfLLY - dfURY) / (poDS->nRasterYSize - 1);
    poDS->adfGeoTransform[0] = dfULX - poDS->adfGeoTransform[1] / 2;
    poDS->adfGeoTransform[3] = dfULY - poDS->adfGeoTransform[5] / 2;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[4] = 0.0;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( i = 1; i <= poDS->nBands; i++ )
        poDS->SetBand( i, new FASTRasterBand( poDS, i, poDS->fpChannels[i -1 ],
	    0, 1, poDS->nRasterXSize, poDS->eDataType, TRUE));

    CPLFree( pszPrefixName );
    CPLFree( pszHeader );

    return( poDS );
}

/************************************************************************/
/*                        GDALRegister_FAST()				*/
/************************************************************************/

void GDALRegister_FAST()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "FAST" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "FAST" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "EOSAT FAST Format" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_fast.html" );

        poDriver->pfnOpen = FASTDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

