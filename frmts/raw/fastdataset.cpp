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
 * Revision 1.9  2003/10/17 07:08:21  dron
 * Use locale selection option in CPLScanDouble().
 *
 * Revision 1.8  2003/10/05 15:31:00  dron
 * TM projection support implemented.
 *
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
	    VSIFCloseL( fpChannels[i] );
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
    if( pszProjection )
        return pszProjection;
    else
        return "";
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
	if ( !EQUAL( pszFilename, "" ) )
	{
	    pszChannelFilename =
                CPLFormCIFilename( pszDirname, pszFilename, NULL );
	    fpChannels[iBand] = VSIFOpenL( pszChannelFilename, "rb" );
	}
	else
	    fpChannels[iBand] = NULL;
	break;
	case IRS:
	default:
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "%s.%d", pszPrefix, iBand + 1 ), pszSuffix );
	fpChannels[iBand] = VSIFOpenL( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "IMAGERY%d", iBand + 1 ), pszSuffix );
	fpChannels[iBand] = VSIFOpenL( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "imagery%d", iBand + 1 ), pszSuffix );
	fpChannels[iBand] = VSIFOpenL( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "IMAGERY%d.DAT", iBand + 1 ), NULL );
	fpChannels[iBand] = VSIFOpenL( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "imagery%d.dat", iBand + 1 ), NULL );
	fpChannels[iBand] = VSIFOpenL( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "IMAGERY%d.dat", iBand + 1 ), NULL );
	fpChannels[iBand] = VSIFOpenL( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "imagery%d.DAT", iBand + 1 ), NULL );
	fpChannels[iBand] = VSIFOpenL( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "BAND%d", iBand + 1 ), pszSuffix );
	fpChannels[iBand] = VSIFOpenL( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "band%d", iBand + 1 ), pszSuffix );
	fpChannels[iBand] = VSIFOpenL( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "BAND%d.DAT", iBand + 1 ), NULL );
	fpChannels[iBand] = VSIFOpenL( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "band%d.dat", iBand + 1 ), NULL );
	fpChannels[iBand] = VSIFOpenL( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "BAND%d.dat", iBand + 1 ), NULL );
	fpChannels[iBand] = VSIFOpenL( pszChannelFilename, "rb" );
	if ( fpChannels[iBand] )
	    break;
	pszChannelFilename = CPLFormFilename( pszDirname,
	    CPLSPrintf( "band%d.DAT", iBand + 1 ), NULL );
	fpChannels[iBand] = VSIFOpenL( pszChannelFilename, "rb" );
	break;
    }
    
    CPLDebug( "FAST", "Band %d filename: %s", iBand + 1, pszChannelFilename);

    CPLFree( pszPrefix );
    CPLFree( pszSuffix );
    return fpChannels[iBand];
}

/************************************************************************/
/*                         PackedDMSToDec()                             */
/*    Convert a packed DMS value (dddmmssss) into decimal degrees.     */
/************************************************************************/

static double PackedDMSToDec( double dfPacked )
{
    double  dfDegrees, dfMinutes, dfSeconds, dfTemp;
    
    dfTemp = floor( dfPacked / 10000.0 );
    dfDegrees = floor( dfPacked / 1000000.0 );
    dfMinutes = dfTemp - floor( dfTemp / 100.0 ) * 100.0;
    dfSeconds = (dfPacked - dfTemp * 10000.0) / 100.0;

    return dfDegrees + dfMinutes / 60.0 + dfSeconds / 3600.0;
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
    char	szHeader[ADM_HEADER_SIZE];
    char	*pszTemp;
 
    VSIFSeek( poDS->fpHeader, 0, SEEK_SET );
    if ( VSIFRead( szHeader, 1, ADM_HEADER_SIZE, poDS->fpHeader ) <
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
    pszTemp = CPLScanString( szHeader + 70, FAST_DATE_SIZE, TRUE, TRUE );
    poDS->SetMetadataItem( "ACQUISITION_DATE", pszTemp );
    CPLFree( pszTemp );

    // Read satellite name
    pszTemp = CPLScanString( szHeader + 91, FAST_SATNAME_SIZE, TRUE, TRUE );
    poDS->SetMetadataItem( "SATELLITE", pszTemp );
    if ( EQUALN(pszTemp, "LANDSAT", 7) )
	poDS->iSatellite = LANDSAT;
    else if ( EQUALN(pszTemp, "IRS", 3) )
	poDS->iSatellite = IRS;
    else
	poDS->iSatellite = IRS;
    CPLFree( pszTemp );

    // Read sensor name
    pszTemp = CPLScanString( szHeader + 110, FAST_SENSORNAME_SIZE, TRUE, TRUE );
    poDS->SetMetadataItem( "SENSOR", pszTemp );
    CPLFree( pszTemp );

    // Read filenames
    poDS->nBands = 0;
    pszTemp = CPLScanString( szHeader + 1130, FAST_FILENAME_SIZE, TRUE, FALSE );
    if ( poDS->FOpenChannel( pszTemp, poDS->nBands ) )
    {
	poDS->nBands++;
        CPLFree( pszTemp );
	pszTemp =
            CPLScanString( szHeader + 1616, FAST_VALUE_SIZE, TRUE, FALSE );
	poDS->SetMetadataItem( CPLSPrintf("BIAS%d",  poDS->nBands), pszTemp );
        CPLFree( pszTemp );
	pszTemp =
            CPLScanString( szHeader + 1641, FAST_VALUE_SIZE, TRUE, FALSE );
	poDS->SetMetadataItem( CPLSPrintf("GAIN%d",  poDS->nBands), pszTemp );
    }
    CPLFree( pszTemp );
    pszTemp = CPLScanString( szHeader + 1169, FAST_FILENAME_SIZE, TRUE, FALSE );
    if ( poDS->FOpenChannel( pszTemp, poDS->nBands ) )
    {
	poDS->nBands++;
        CPLFree( pszTemp );
	pszTemp =
            CPLScanString( szHeader + 1696, FAST_VALUE_SIZE, TRUE, FALSE );
	poDS->SetMetadataItem( CPLSPrintf("BIAS%d",  poDS->nBands), pszTemp );
        CPLFree( pszTemp );
	pszTemp =
            CPLScanString( szHeader + 1721, FAST_VALUE_SIZE, TRUE, FALSE );
	poDS->SetMetadataItem( CPLSPrintf("GAIN%d",  poDS->nBands), pszTemp );
    }
    CPLFree( pszTemp );
    pszTemp = CPLScanString( szHeader + 1210, FAST_FILENAME_SIZE, TRUE, FALSE );
    if ( poDS->FOpenChannel( pszTemp, poDS->nBands ) )
    {
	poDS->nBands++;
        CPLFree( pszTemp );
	pszTemp =
            CPLScanString( szHeader + 1776, FAST_VALUE_SIZE, TRUE, FALSE );
	poDS->SetMetadataItem( CPLSPrintf("BIAS%d",  poDS->nBands), pszTemp );
        CPLFree( pszTemp );
	pszTemp =
            CPLScanString( szHeader + 1801, FAST_VALUE_SIZE, TRUE, FALSE );
	poDS->SetMetadataItem( CPLSPrintf("GAIN%d",  poDS->nBands), pszTemp );
    }
    CPLFree( pszTemp );
    pszTemp = CPLScanString( szHeader + 1249, FAST_FILENAME_SIZE, TRUE, FALSE );
    if ( poDS->FOpenChannel( pszTemp, poDS->nBands ) )
    {
	poDS->nBands++;
        CPLFree( pszTemp );
	pszTemp =
            CPLScanString( szHeader + 1856, FAST_VALUE_SIZE, TRUE, FALSE );
	poDS->SetMetadataItem( CPLSPrintf("BIAS%d",  poDS->nBands), pszTemp );
        CPLFree( pszTemp );
	pszTemp =
            CPLScanString( szHeader + 1881, FAST_VALUE_SIZE, TRUE, FALSE );
	poDS->SetMetadataItem( CPLSPrintf("GAIN%d",  poDS->nBands), pszTemp );
    }
    CPLFree( pszTemp );
    pszTemp = CPLScanString( szHeader + 1290, FAST_FILENAME_SIZE, TRUE, FALSE );
    if ( poDS->FOpenChannel( pszTemp, poDS->nBands ) )
    {
	poDS->nBands++;
        CPLFree( pszTemp );
	pszTemp =
            CPLScanString( szHeader + 1936, FAST_VALUE_SIZE, TRUE, FALSE );
	poDS->SetMetadataItem( CPLSPrintf("BIAS%d",  poDS->nBands), pszTemp );
        CPLFree( pszTemp );
	pszTemp =
            CPLScanString( szHeader + 1961, FAST_VALUE_SIZE, TRUE, FALSE );
	poDS->SetMetadataItem( CPLSPrintf("GAIN%d",  poDS->nBands), pszTemp );
    }
    CPLFree( pszTemp );
    pszTemp = CPLScanString( szHeader + 1329, FAST_FILENAME_SIZE, TRUE, FALSE );
    if ( poDS->FOpenChannel( pszTemp, poDS->nBands ) )
    {
	poDS->nBands++;
        CPLFree( pszTemp );
	pszTemp =
            CPLScanString( szHeader + 2016, FAST_VALUE_SIZE, TRUE, FALSE );
	poDS->SetMetadataItem( CPLSPrintf("BIAS%d",  poDS->nBands), pszTemp );
        CPLFree( pszTemp );
	pszTemp =
            CPLScanString( szHeader + 2041, FAST_VALUE_SIZE, TRUE, FALSE );
	poDS->SetMetadataItem( CPLSPrintf("GAIN%d",  poDS->nBands), pszTemp );
    }
    CPLFree( pszTemp );

    if ( !poDS->nBands )
    {
	CPLDebug( "FAST", "Failed to find and open band data files." );
	delete poDS;
	return NULL;
    }
    
    poDS->nRasterXSize = atoi( szHeader + 842 );
    poDS->nRasterYSize = atoi( szHeader + 870 );

    switch( atoi( szHeader + 983 ) )
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
    // Coordinates of corner pixel's centers
    double	dfULX = 0.5, dfULY = 0.5;
    double	dfURX = poDS->nRasterXSize - 0.5, dfURY = 0.5;
    double	dfLLX = 0.5, dfLLY = poDS->nRasterYSize - 0.5;
    double	dfLRX = poDS->nRasterXSize - 0.5, dfLRY = poDS->nRasterYSize - 0.5;
    
    if ( EQUALN(szHeader + 3145, "WGS84", 5) || EQUALN(szHeader + 3145, " ", 1) )
	oSRS.SetWellKnownGeogCS( "WGS84" );
    
    if ( EQUALN(szHeader + 3103, "UTM", 3) )
    {
        int		iUTMZone;

        if ( poDS->iSatellite == LANDSAT )
	    iUTMZone = atoi( szHeader + 3592 );
	else
	    iUTMZone = (int)atof( szHeader + 3232 );
        iUTMZone = ABS( iUTMZone );
        if( *(szHeader + 3662) == 'N' )	        // Northern hemisphere
	    oSRS.SetUTM( iUTMZone, TRUE );
        else					// Southern hemisphere
	    oSRS.SetUTM( iUTMZone, FALSE );

	// Coordinates in meters
        dfULX = CPLScanDouble( szHeader + 3664, 13, "C" );
        dfULY = CPLScanDouble( szHeader + 3678, 13, "C" );
        dfURX = CPLScanDouble( szHeader + 3744, 13, "C" );
        dfURY = CPLScanDouble( szHeader + 3758, 13, "C" );
        dfLRX = CPLScanDouble( szHeader + 3824, 13, "C" );
        dfLRY = CPLScanDouble( szHeader + 3838, 13, "C" );
        dfLLX = CPLScanDouble( szHeader + 3904, 13, "C" );
        dfLLY = CPLScanDouble( szHeader + 3918, 13, "C" );
    }
    
    else if ( EQUALN(szHeader + 3103, "TM", 2) )
    {
        double  dfZone = 0.0;

        oSRS.SetTM( // Center latitude
                    PackedDMSToDec( CPLScanDouble(szHeader + 3312, 24, "C") ),
                    // Center longitude
                    PackedDMSToDec( CPLScanDouble(szHeader + 3282, 24, "C") ),
                    CPLScanDouble(szHeader + 3232, 24, "C"),   // Scale factor
                    CPLScanDouble(szHeader + 3337, 24, "C"),   // False easting
                    CPLScanDouble(szHeader + 3362, 24, "C") ); // False northing

        dfZone = atoi( szHeader + 3592 ) * 1000000.0;
	// Coordinates in meters
        dfULX = CPLScanDouble( szHeader + 3664, 13, "C" ) - dfZone;
        dfULY = CPLScanDouble( szHeader + 3678, 13, "C" );
        dfURX = CPLScanDouble( szHeader + 3744, 13, "C" ) - dfZone;
        dfURY = CPLScanDouble( szHeader + 3758, 13, "C" );
        dfLRX = CPLScanDouble( szHeader + 3824, 13, "C" ) - dfZone;
        dfLRY = CPLScanDouble( szHeader + 3838, 13, "C" );
        dfLLX = CPLScanDouble( szHeader + 3904, 13, "C" ) - dfZone;
        dfLLY = CPLScanDouble( szHeader + 3918, 13, "C" );
    }

    oSRS.SetLinearUnits( SRS_UL_METER, 1.0 );
    
    if ( poDS->pszProjection )
        CPLFree( poDS->pszProjection );
    oSRS.exportToWkt( &poDS->pszProjection );

    poDS->adfGeoTransform[1] = (dfURX - dfLLX) / (poDS->nRasterXSize - 1);
    if( *(szHeader + 3662) == 'N' )
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

