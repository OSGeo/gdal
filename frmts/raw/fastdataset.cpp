/******************************************************************************
 * $Id$
 *
 * Project:  EOSAT FAST Format reader
 * Purpose:  Reads Landsat FAST-L7A, IRS 1C/1D
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
 * Revision 1.7  2003/07/08 21:10:19  warmerda
 * avoid warnings
 *
 * Revision 1.6  2003/03/14 17:28:10  dron
 * CPLFormCIFilename() used instead of CPLFormFilename() for FAST-L7 datasets.
 *
 * Revision 1.5  2003/03/05 15:49:59  dron
 * Fixed typo when reading SENSOR metadata record.
 *
 * Revision 1.4  2003/02/18 15:07:49  dron
 * IRS-1C/1D support added.
 *
 * Revision 1.3  2003/02/14 21:05:40  warmerda
 * Don't use path for rawdataset.h.
 *
 * Revision 1.2  2002/12/30 14:55:01  dron
 * SetProjCS() removed, added unit setting.
 *
 * Revision 1.1  2002/10/05 12:35:31  dron
 * FAST driver moved to the RAW directory.
 *
 * Revision 1.5  2002/10/04 16:06:06  dron
 * Some redundancy removed.
 *
 * Revision 1.4  2002/10/04 12:33:02  dron
 * Added calibration coefficients extraction.
 *
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
#include "rawdataset.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_FAST(void);
CPL_C_END

#define FAST_FILENAME_SIZE	29
#define FAST_VALUE_SIZE		24
#define FAST_DATE_SIZE		8
#define FAST_SATNAME_SIZE	10
#define FAST_SENSORNAME_SIZE	10
#define ADM_HEADER_SIZE		4608

typedef enum {	// Satellites:
    LANDSAT,	// Landsat 7
    IRS		// IRS 1C/1D
} FASTSatellite;

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
    const char	*pszFilename;
    char	*pszDirname;
    GDALDataType eDataType;
    FASTSatellite iSatellite;

    void	ComputeGeoref();
    
  public:
                FASTDataset();
		~FASTDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );

    CPLErr 	GetGeoTransform( double * padfTransform );
    const char	*GetProjectionRef();
    FILE	*FOpenChannel( char *pszFilename, int iBand );
};

/************************************************************************/
/* ==================================================================== */
/*                            FASTRasterBand                            */
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
/*                           FASTRasterBand()                           */
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
/*                           FASTDataset()                              */
/************************************************************************/

FASTDataset::FASTDataset()

{
    fpHeader = NULL;
    pszDirname = NULL;
    pszProjection = CPLStrdup( "" );
    nBands = 0;
}

/************************************************************************/
/*                            ~FASTDataset()                            */
/************************************************************************/

FASTDataset::~FASTDataset()

{
    int i;
    if ( pszDirname )
	CPLFree( pszDirname );
    if ( pszProjection )
	CPLFree( pszProjection );
    for ( i = 0; i < nBands; i++ )
	if ( fpChannels[i] )
	    VSIFClose( fpChannels[i] );
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
/************************************************************************/
/*                             FOpenChannel()                           */
/************************************************************************/

FILE *FASTDataset::FOpenChannel( char *pszFilename, int iBand )
{
    const char	*pszChannelFilename = NULL;
    char	*pszPrefix = CPLStrdup( CPLGetBasename( this->pszFilename ) );
    char	*pszSuffix = CPLStrdup( CPLGetExtension( this->pszFilename ) );

    switch ( iSatellite )
    {
	case LANDSAT:
	if ( pszFilename[0] != ' ' )
	{
	    pszChannelFilename =
                CPLFormCIFilename( pszDirname, pszFilename, NULL );
	    fpChannels[iBand] = VSIFOpen( pszChannelFilename, "rb" );
	}
	else
	    fpChannels[iBand] = NULL;
	break;
	case IRS:
	default:
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "%s.%d", pszPrefix, iBand + 1 ), pszSuffix );
	fpChannels[iBand] = VSIFOpen( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "IMAGERY%d", iBand + 1 ), pszSuffix );
	fpChannels[iBand] = VSIFOpen( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "imagery%d", iBand + 1 ), pszSuffix );
	fpChannels[iBand] = VSIFOpen( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "IMAGERY%d.DAT", iBand + 1 ), NULL );
	fpChannels[iBand] = VSIFOpen( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "imagery%d.dat", iBand + 1 ), NULL );
	fpChannels[iBand] = VSIFOpen( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "IMAGERY%d.dat", iBand + 1 ), NULL );
	fpChannels[iBand] = VSIFOpen( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "imagery%d.DAT", iBand + 1 ), NULL );
	fpChannels[iBand] = VSIFOpen( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "BAND%d", iBand + 1 ), pszSuffix );
	fpChannels[iBand] = VSIFOpen( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "band%d", iBand + 1 ), pszSuffix );
	fpChannels[iBand] = VSIFOpen( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "BAND%d.DAT", iBand + 1 ), NULL );
	fpChannels[iBand] = VSIFOpen( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "band%d.dat", iBand + 1 ), NULL );
	fpChannels[iBand] = VSIFOpen( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "BAND%d.dat", iBand + 1 ), NULL );
	fpChannels[iBand] = VSIFOpen( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "band%d.DAT", iBand + 1 ), NULL );
	fpChannels[iBand] = VSIFOpen( pszChannelFilename, "rb" );
	break;
    }
    
    CPLDebug( "FAST", "Band %d filename: %s", iBand + 1, pszChannelFilename);

    CPLFree( pszPrefix );
    CPLFree( pszSuffix );
    return fpChannels[iBand];
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *FASTDataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		i;
	
    if( poOpenInfo->fp == NULL )
        return NULL;

    if( !EQUALN((const char *) poOpenInfo->pabyHeader + 52,
		"ACQUISITION DATE =", 18) &&
	!EQUALN((const char *) poOpenInfo->pabyHeader + 80,
		"SATELLITE =", 11) && 
	!EQUALN((const char *) poOpenInfo->pabyHeader + 101, " SENSOR =", 9) && 
	!EQUALN((const char *) poOpenInfo->pabyHeader + 183, " LOCATION =", 11) )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    FASTDataset	*poDS;

    poDS = new FASTDataset();

    poDS->fpHeader = poOpenInfo->fp;
    poOpenInfo->fp = NULL;
    poDS->pszFilename = poOpenInfo->pszFilename;
    poDS->pszDirname = CPLStrdup( CPLGetDirname( poOpenInfo->pszFilename ) );
    
/* -------------------------------------------------------------------- */
/*    Read the administrative header and calibration coefficients from  */
/*    radiometric record.                                               */
/* -------------------------------------------------------------------- */
    char	pszHeader[ADM_HEADER_SIZE];
    char	pszDate[FAST_DATE_SIZE + 1];
    char	pszSatName[FAST_SATNAME_SIZE + 1];
    char	pszSensorName[FAST_SENSORNAME_SIZE + 1];
    char	pszFilename[FAST_FILENAME_SIZE + 1];
    char	pszValue[FAST_VALUE_SIZE + 1];
 
    VSIFSeek( poDS->fpHeader, 0, SEEK_SET );
    if ( VSIFRead( pszHeader, 1, ADM_HEADER_SIZE, poDS->fpHeader ) <
	 ADM_HEADER_SIZE )
    {
	CPLError( CE_Failure, CPLE_AppDefined,
		  "Failed to read FAST header file: %s\n."
		  "Expected %d bytes in header.\n",
		  poOpenInfo->pszFilename, ADM_HEADER_SIZE );
	delete poDS;
	return NULL;
    }

    // Read acquisition date
    pszDate[FAST_DATE_SIZE] = '\0';
    memcpy( pszDate, pszHeader + 70, FAST_DATE_SIZE );
    poDS->SetMetadataItem( "ACQUISITION_DATE", pszDate );

    // Read satellite name
    pszSatName[FAST_SATNAME_SIZE] = '\0';
    memcpy( pszSatName, pszHeader + 91, FAST_SATNAME_SIZE );
    poDS->SetMetadataItem( "SATELLITE", pszSatName );
    if ( EQUALN(pszSatName, "LANDSAT", 7) )
	poDS->iSatellite = LANDSAT;
    else if ( EQUALN(pszSatName, "IRS", 3) )
	poDS->iSatellite = IRS;
    else
	poDS->iSatellite = IRS;

    // Read sensor name
    pszSensorName[FAST_SENSORNAME_SIZE] = '\0';
    memcpy( pszSensorName, pszHeader + 110, FAST_SENSORNAME_SIZE );
    poDS->SetMetadataItem( "SENSOR", pszSensorName );

    pszFilename[FAST_FILENAME_SIZE] = '\0';
    pszValue[FAST_VALUE_SIZE] = '\0';
    poDS->nBands = 0;
    memcpy( pszFilename, pszHeader + 1130, FAST_FILENAME_SIZE );
    if ( poDS->FOpenChannel( pszFilename, poDS->nBands ) )
    {
	poDS->nBands++;
	memcpy( pszValue, pszHeader + 1616, FAST_VALUE_SIZE );
	poDS->SetMetadataItem( CPLSPrintf("BIAS%d",  poDS->nBands), pszValue );
	memcpy( pszValue, pszHeader + 1641, FAST_VALUE_SIZE );
	poDS->SetMetadataItem( CPLSPrintf("GAIN%d",  poDS->nBands), pszValue );
    }
    memcpy( pszFilename, pszHeader + 1169, FAST_FILENAME_SIZE );
    if ( poDS->FOpenChannel( pszFilename, poDS->nBands ) )
    {
	poDS->nBands++;
	memcpy( pszValue, pszHeader + 1696, FAST_VALUE_SIZE );
	poDS->SetMetadataItem( CPLSPrintf("BIAS%d",  poDS->nBands), pszValue );
	memcpy( pszValue, pszHeader + 1721, FAST_VALUE_SIZE );
	poDS->SetMetadataItem( CPLSPrintf("GAIN%d",  poDS->nBands), pszValue );
    }
    memcpy( pszFilename, pszHeader + 1210, FAST_FILENAME_SIZE );
    if ( poDS->FOpenChannel( pszFilename, poDS->nBands ) )
    {
	poDS->nBands++;
	memcpy( pszValue, pszHeader + 1776, FAST_VALUE_SIZE );
	poDS->SetMetadataItem( CPLSPrintf("BIAS%d",  poDS->nBands), pszValue );
	memcpy( pszValue, pszHeader + 1801, FAST_VALUE_SIZE );
	poDS->SetMetadataItem( CPLSPrintf("GAIN%d",  poDS->nBands), pszValue );
    }
    memcpy( pszFilename, pszHeader + 1249, FAST_FILENAME_SIZE );
    if ( poDS->FOpenChannel( pszFilename, poDS->nBands ) )
    {
	poDS->nBands++;
	memcpy( pszValue, pszHeader + 1856, FAST_VALUE_SIZE );
	poDS->SetMetadataItem( CPLSPrintf("BIAS%d",  poDS->nBands), pszValue );
	memcpy( pszValue, pszHeader + 1881, FAST_VALUE_SIZE );
	poDS->SetMetadataItem( CPLSPrintf("GAIN%d",  poDS->nBands), pszValue );
    }
    memcpy( pszFilename, pszHeader + 1290, FAST_FILENAME_SIZE );
    if ( poDS->FOpenChannel( pszFilename, poDS->nBands ) )
    {
	poDS->nBands++;
	memcpy( pszValue, pszHeader + 1936, FAST_VALUE_SIZE );
	poDS->SetMetadataItem( CPLSPrintf("BIAS%d",  poDS->nBands), pszValue );
	memcpy( pszValue, pszHeader + 1961, FAST_VALUE_SIZE );
	poDS->SetMetadataItem( CPLSPrintf("GAIN%d",  poDS->nBands), pszValue );
    }
    memcpy( pszFilename, pszHeader + 1329, FAST_FILENAME_SIZE );
    if ( poDS->FOpenChannel( pszFilename, poDS->nBands ) )
    {
	poDS->nBands++;
	memcpy( pszValue, pszHeader + 2016, FAST_VALUE_SIZE );
	poDS->SetMetadataItem( CPLSPrintf("BIAS%d",  poDS->nBands), pszValue );
	memcpy( pszValue, pszHeader + 2041, FAST_VALUE_SIZE );
	poDS->SetMetadataItem( CPLSPrintf("GAIN%d",  poDS->nBands), pszValue );
    }

    if ( !poDS->nBands )
    {
	CPLDebug( "FAST", "Failed to find and open band data files." );
	delete poDS;
	return NULL;
    }
    
    poDS->nRasterXSize = atoi( pszHeader + 842 );
    poDS->nRasterYSize = atoi( pszHeader + 870 );

    switch( atoi( pszHeader + 983 ) )
    {
	case 8:
        default:
	poDS->eDataType = GDT_Byte;
	break;
    }

/* -------------------------------------------------------------------- */
/*          Read geometric record					*/
/* -------------------------------------------------------------------- */
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
        if ( poDS->iSatellite == LANDSAT )
	    iUTMZone = atoi( pszHeader + 3592 );
	else
	    iUTMZone = (int)atof( pszHeader + 3232 );
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

    oSRS.SetLinearUnits( SRS_UL_METER, 1.0 );
    
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
        poDS->SetBand( i, new FASTRasterBand( poDS, i, poDS->fpChannels[i - 1],
	    0, 1, poDS->nRasterXSize, poDS->eDataType, TRUE));

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

