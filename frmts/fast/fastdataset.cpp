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

static GDALDriver	*poFASTDriver = NULL;

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
    char	*pszHeaderFilename;
    char	*pszHeader;
    
    FILE	*fpChannels[6];
    char	*pszDirname;
    const char	*pszChannelFilenames[6];
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
    
//    virtual CPLErr IReadBlock( int, int, void * );
};


/************************************************************************/
/*                           FASTRasterBand()                            */
/************************************************************************/

FASTRasterBand::FASTRasterBand( FASTDataset *poDS, int nBand, FILE * fpRaw,
		vsi_l_offset nImgOffset, int nPixelOffset,
                              int nLineOffset,
                              GDALDataType eDataType, int bNativeOrder) :
                RawRasterBand( poDS, nBand, fpRaw, 
                              nImgOffset, nPixelOffset,
                              nLineOffset,
                              eDataType, bNativeOrder,
                              FALSE)
{
/*    this->poDS = poDS;
    this->nBand = nBand;
    eDataType = GDT_UInt16;

    nBlockXSize = poDS->GetRasterXSize();
    nBlockYSize = 1;*/

/*    nBlocksPerRow = 1;
    nBlocksPerColumn = 0;*/

}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

/*CPLErr FASTRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                      void * pImage )
{
//    return CE_None;
}*/

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
    if( fpHeader != NULL )
        VSIFClose( fpHeader );
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

char *CPLDirname( char *pszInputName )
{
    char   *pszDirname = CPLStrdup( pszInputName );
    int    i;

    for( i = strlen(pszDirname) - 1; i > 0; i-- )
    {
         if( pszDirname[i] == '\\' || pszDirname[i] == '/' )
         {
             if ( i == 0 )
		     pszDirname[i + 1] = '\0';
	     else
		     pszDirname[i] = '\0';
             return pszDirname;
         }
    }
    pszDirname[0] = '.';
    pszDirname[1] = '\0';

    return pszDirname;
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
    FASTDataset *poDS;

    poDS = new FASTDataset();

    poDS->poDriver = poFASTDriver;
    poDS->fpHeader = poOpenInfo->fp;
    poOpenInfo->fp = NULL;
    poDS->pszHeaderFilename = poOpenInfo->pszFilename;
    
/* -------------------------------------------------------------------- */
/*      Read the administrative header.                                 */
/* -------------------------------------------------------------------- */
    poDS->pszDirname = CPLDirname( poDS->pszHeaderFilename );
    poDS->pszHeader = (char *)CPLMalloc( ADM_HEADER_SIZE );
    fseek( poDS->fpHeader, 0, SEEK_SET );
    fread( poDS->pszHeader, 1, ADM_HEADER_SIZE, poDS->fpHeader );

    poDS->nBands = 0;
    if ( poDS->pszHeader[1130] != ' ' )
    {
	poDS->pszChannelFilenames[poDS->nBands] =
	    CPLSPrintf( "%s/%.29s", poDS->pszDirname, poDS->pszHeader + 1130 );
        poDS->fpChannels[poDS->nBands] =
	    fopen( poDS->pszChannelFilenames[poDS->nBands], "rb" );
	if ( poDS->fpChannels[poDS->nBands] )
	    poDS->nBands++;
    }
    if ( poDS->pszHeader[1169] != ' ' )
    {
	poDS->pszChannelFilenames[poDS->nBands] =
	    CPLSPrintf( "%s/%.29s", poDS->pszDirname, poDS->pszHeader + 1169 );
        poDS->fpChannels[poDS->nBands] =
	    fopen( poDS->pszChannelFilenames[poDS->nBands], "rb" );
	if ( poDS->fpChannels[poDS->nBands] )
	    poDS->nBands++;
    }
    if ( poDS->pszHeader[1210] != ' ' )
    {
	poDS->pszChannelFilenames[poDS->nBands] =
	    CPLSPrintf( "%s/%.29s", poDS->pszDirname, poDS->pszHeader + 1210 );
        poDS->fpChannels[poDS->nBands] =
	    fopen( poDS->pszChannelFilenames[poDS->nBands], "rb" );
	if ( poDS->fpChannels[poDS->nBands] )
	    poDS->nBands++;
    }
    if ( poDS->pszHeader[1249] != ' ' )
    {
	poDS->pszChannelFilenames[poDS->nBands] =
	    CPLSPrintf( "%s/%.29s", poDS->pszDirname, poDS->pszHeader + 1249 );
        poDS->fpChannels[poDS->nBands] =
	    fopen( poDS->pszChannelFilenames[poDS->nBands], "rb" );
	if ( poDS->fpChannels[poDS->nBands] )
	    poDS->nBands++;
    }
    if ( poDS->pszHeader[1290] != ' ' )
    {
	poDS->pszChannelFilenames[poDS->nBands] =
	    CPLSPrintf( "%s/%.29s", poDS->pszDirname, poDS->pszHeader + 1290 );
        poDS->fpChannels[poDS->nBands] =
	    fopen( poDS->pszChannelFilenames[poDS->nBands], "rb" );
	if ( poDS->fpChannels[poDS->nBands] )
	    poDS->nBands++;
    }
    if ( poDS->pszHeader[1329] != ' ' )
    {
	poDS->pszChannelFilenames[poDS->nBands] =
	    CPLSPrintf( "%s/%.29s", poDS->pszDirname, poDS->pszHeader + 1329 );
        poDS->fpChannels[poDS->nBands] =
	    fopen( poDS->pszChannelFilenames[poDS->nBands], "rb" );
	if ( poDS->fpChannels[poDS->nBands] )
	    poDS->nBands++;
    }

    if ( !poDS->nBands )
	return NULL;
    
    poDS->nRasterXSize = atoi( poDS->pszHeader + 842 );
    poDS->nRasterYSize = atoi( poDS->pszHeader + 870 );

    switch( atoi( poDS->pszHeader + 983 ) )
    {
        default:
	case 8:
	poDS->eDataType = GDT_Byte;
	break;
    }

    // Read geometric record
    OGRSpatialReference oSRS;
    int iUTMZone;
    double dfULX, dfULY, dfURX, dfURY, dfLLX, dfLLY, dfLRX, dfLRY;
    
    if ( EQUALN(poDS->pszHeader + 3103, "UTM", 3) )
	oSRS.SetProjCS( "UTM" );
    if ( EQUALN(poDS->pszHeader + 3145, "WGS84", 5) )
	oSRS.SetWellKnownGeogCS( "WGS84" );
    iUTMZone = atoi( poDS->pszHeader + 3592 );
    if( *(poDS->pszHeader + 3662) == 'N' )	// North hemisphere
	oSRS.SetUTM( iUTMZone, TRUE );
    else					// South hemisphere
	oSRS.SetUTM( iUTMZone, FALSE );
    oSRS.exportToWkt( &poDS->pszProjection );
    
    dfULX = atof( poDS->pszHeader + 3664 );
    dfULY = atof( poDS->pszHeader + 3678 );
    dfURX = atof( poDS->pszHeader + 3744 );
    dfURY = atof( poDS->pszHeader + 3758 );
    dfLRX = atof( poDS->pszHeader + 3824 );
    dfLRY = atof( poDS->pszHeader + 3838 );
    dfLLX = atof( poDS->pszHeader + 3904 );
    dfLLY = atof( poDS->pszHeader + 3918 );

    poDS->adfGeoTransform[0] = dfULX;
    poDS->adfGeoTransform[3] = dfULY;
    poDS->adfGeoTransform[1] = (dfURX - dfLLX) / poDS->nRasterXSize;
    poDS->adfGeoTransform[2] = 0.0;
    poDS->adfGeoTransform[4] = 0.0;
    poDS->adfGeoTransform[5] = -1 * (dfURY - dfLLY) / poDS->nRasterYSize;

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( i = 1; i <= poDS->nBands; i++ )
        poDS->SetBand( i, new FASTRasterBand( poDS, i, poDS->fpChannels[i -1 ],
	    0, 1, poDS->nRasterXSize, poDS->eDataType, TRUE));

/* -------------------------------------------------------------------- */
/*      Get and set other important information	as metadata             */
/* -------------------------------------------------------------------- */

    return( poDS );
}

/************************************************************************/
/*                        GDALRegister_FAST()				*/
/************************************************************************/

void GDALRegister_FAST()

{
    GDALDriver	*poDriver;

    if( poFASTDriver == NULL )
    {
        poFASTDriver = poDriver = new GDALDriver();
        
        poDriver->SetDescription( "FAST" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "EOSAT FAST Format" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_fast.html" );

        poDriver->pfnOpen = FASTDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

