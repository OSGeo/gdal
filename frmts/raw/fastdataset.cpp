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
 * Revision 1.20  2006/08/09 12:13:21  dron
 * Fixes to handle more non-standard datasets as per bug
 * http://bugzilla.remotesensing.org/show_bug.cgi?id=988
 *
 * Revision 1.19  2005/12/20 20:07:30  dron
 * Added support for 16-bit data.
 *
 * Revision 1.18  2005/09/14 13:18:32  dron
 * Avoid warnings.
 *
 * Revision 1.17  2005/05/05 13:55:42  fwarmerdam
 * PAM Enable
 *
 * Revision 1.16  2004/07/12 18:24:23  gwalter
 * Fixed geotransform calculation.
 *
 * Revision 1.15  2004/04/27 14:25:41  warmerda
 * Cast to avoid warning on Solaris.
 *
 * Revision 1.14  2004/03/16 18:27:39  dron
 * Fixes in projection parameters parsing code.
 *
 * Revision 1.13  2004/02/18 20:22:10  dron
 * Create RawRasterBand objects in "large" mode; more datums and ellipsoids.
 *
 * Revision 1.12  2004/02/17 08:05:45  dron
 * Do not calculate projection definition if corner coordinates are not set.
 *
 * Revision 1.11  2004/02/03 20:38:50  dron
 * Fixes in coordinate hadling; recognize more projections.
 *
 * Revision 1.10  2004/02/01 17:29:34  dron
 * Format parsing logic completely rewritten. Start using importFromUSGS().
 *
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

#include "cpl_string.h"
#include "cpl_conv.h"
#include "ogr_spatialref.h"
#include "rawdataset.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_FAST(void);
CPL_C_END

#define ADM_STD_HEADER_SIZE	4608    // XXX: Format specification says it
#define ADM_HEADER_SIZE		5000    // should be 4608, but some vendors
                                        // ship broken large datasets.
#define ADM_MIN_HEADER_SIZE     1536    // ...and sometimes it can be
                                        // even 1/3 of standard size

#define ACQUISITION_DATE        "ACQUISITION DATE"
#define ACQUISITION_DATE_SIZE   8

#define SATELLITE_NAME          "SATELLITE"
#define SATELLITE_NAME_SIZE     10

#define SENSOR_NAME             "SENSOR"
#define SENSOR_NAME_SIZE        10

#define FILENAME                "FILENAME"
#define FILENAME_SIZE           29

#define PIXELS                  "PIXELS PER LINE"
#define PIXELS_SIZE             5

#define LINES1                  "LINES PER BAND"
#define LINES2                  "LINES PER IMAGE"
#define LINES_SIZE              5

#define BITS_PER_PIXEL          "OUTPUT BITS PER PIXEL"
#define BITS_PER_PIXEL_SIZE     2

#define PROJECTION_NAME         "MAP PROJECTION"
#define PROJECTION_NAME_SIZE    4

#define ELLIPSOID_NAME          "ELLIPSOID"
#define ELLIPSOID_NAME_SIZE     18

#define DATUM_NAME              "DATUM"
#define DATUM_NAME_SIZE         6

#define ZONE_NUMBER             "USGS MAP ZONE"
#define ZONE_NUMBER_SIZE        6

#define USGS_PARAMETERS         "USGS PROJECTION PARAMETERS"

#define CORNER_UPPER_LEFT       "UL"
#define CORNER_UPPER_RIGHT      "UR"
#define CORNER_LOWER_LEFT       "LL"
#define CORNER_LOWER_RIGHT      "LR"
#define CORNER_VALUE_SIZE       13

#define VALUE_SIZE              24

enum FASTSatellite  // Satellites:
{
    LANDSAT,	    // Landsat 7
    IRS		    // IRS 1C/1D
};

/************************************************************************/
/* ==================================================================== */
/*				FASTDataset				*/
/* ==================================================================== */
/************************************************************************/

class FASTDataset : public GDALPamDataset
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
                               nLineOffset, eDataType, bNativeOrder, TRUE)
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
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    nBands = 0;
}

/************************************************************************/
/*                            ~FASTDataset()                            */
/************************************************************************/

FASTDataset::~FASTDataset()

{
    int i;

    FlushCache();

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
	if ( pszFilename && !EQUAL( pszFilename, "" ) )
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
    
    CPLDebug( "FAST", "Band %d filename=%s", iBand + 1, pszChannelFilename);

    CPLFree( pszPrefix );
    CPLFree( pszSuffix );
    return fpChannels[iBand];
}

/************************************************************************/
/*                          GetValue()                                  */
/************************************************************************/

static char *GetValue( const char *pszString, const char *pszName,
                       int iValueSize, int iNormalize )
{
    char    *pszTemp = strstr( (char *) pszString, pszName );

    if ( pszTemp )
    {
        // Skip the parameter name
        pszTemp += strlen( pszName );
        fprintf(stderr, "pszTemp=%s\n", pszTemp);
        // Skip whitespaces and equal signs
        while ( *pszTemp == ' ' )
            pszTemp++;
        while ( *pszTemp == '=' )
            pszTemp++;
        pszTemp = CPLScanString( pszTemp, iValueSize, TRUE, iNormalize );
    }

    return pszTemp;
}

/************************************************************************/
/*                        USGSMnemonicToCode()                          */
/************************************************************************/

static long USGSMnemonicToCode( const char* pszMnemonic )
{
    if ( EQUAL(pszMnemonic, "UTM") )
        return 1L;
    else if ( EQUAL(pszMnemonic, "LCC") )
        return 4L;
    else if ( EQUAL(pszMnemonic, "PS") )
        return 6L;
    else if ( EQUAL(pszMnemonic, "PC") )
        return 7L;
    else if ( EQUAL(pszMnemonic, "TM") )
        return 9L;
    else if ( EQUAL(pszMnemonic, "OM") )
        return 20L;
    else if ( EQUAL(pszMnemonic, "SOM") )
        return 22L;
    else
        return 1L;  // UTM by default
}

/************************************************************************/
/*                        USGSEllipsoidToCode()                         */
/************************************************************************/

static long USGSEllipsoidToCode( const char* pszMnemonic )
{
    if ( EQUAL(pszMnemonic, "CLARKE_1866") )
        return 0L;
    else if ( EQUAL(pszMnemonic, "CLARKE_1880") )
        return 1L;
    else if ( EQUAL(pszMnemonic, "BESSEL") )
        return 2L;
    else if ( EQUAL(pszMnemonic, "INTERNATL_1967") )
        return 3L;
    else if ( EQUAL(pszMnemonic, "INTERNATL_1909") )
        return 4L;
    else if ( EQUAL(pszMnemonic, "WGS72") || EQUAL(pszMnemonic, "WGS_72") )
        return 5L;
    else if ( EQUAL(pszMnemonic, "EVEREST") )
        return 6L;
    else if ( EQUAL(pszMnemonic, "WGS66") || EQUAL(pszMnemonic, "WGS_66") )
        return 7L;
    else if ( EQUAL(pszMnemonic, "GRS_80") )
        return 8L;
    else if ( EQUAL(pszMnemonic, "AIRY") )
        return 9L;
    else if ( EQUAL(pszMnemonic, "MODIFIED_EVEREST") )
        return 10L;
    else if ( EQUAL(pszMnemonic, "MODIFIED_AIRY") )
        return 11L;
    else if ( EQUAL(pszMnemonic, "WGS84") || EQUAL(pszMnemonic, "WGS_84") )
        return 12L;
    else if ( EQUAL(pszMnemonic, "SOUTHEAST_ASIA") )
        return 13L;
    else if ( EQUAL(pszMnemonic, "AUSTRALIAN_NATL") )
        return 14L;
    else if ( EQUAL(pszMnemonic, "KRASSOVSKY") )
        return 15L;
    else if ( EQUAL(pszMnemonic, "HOUGH") )
        return 16L;
    else if ( EQUAL(pszMnemonic, "MERCURY_1960") )
        return 17L;
    else if ( EQUAL(pszMnemonic, "MOD_MERC_1968") )
        return 18L;
    else if ( EQUAL(pszMnemonic, "6370997_M_SPHERE") )
        return 19L;
    else
        return 0L;
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
		"ACQUISITION DATE =", 18)
        && !EQUALN((const char *) poOpenInfo->pabyHeader + 36,
		"ACQUISITION DATE =", 18) )
        return NULL;
    
/* -------------------------------------------------------------------- */
/*  Create a corresponding GDALDataset.                                 */
/* -------------------------------------------------------------------- */
    FASTDataset	*poDS;

    poDS = new FASTDataset();

    poDS->fpHeader = poOpenInfo->fp;
    poOpenInfo->fp = NULL;
    poDS->pszFilename = poOpenInfo->pszFilename;
    poDS->pszDirname = CPLStrdup( CPLGetDirname( poOpenInfo->pszFilename ) );
    
/* -------------------------------------------------------------------- */
/*  Read the administrative record.                                     */
/* -------------------------------------------------------------------- */
    char	*pszTemp;
    char	*pszHeader = (char *) CPLMalloc( ADM_HEADER_SIZE + 1 );
    size_t      nBytesRead;
 
    VSIFSeek( poDS->fpHeader, 0, SEEK_SET );
    nBytesRead = VSIFRead( pszHeader, 1, ADM_HEADER_SIZE, poDS->fpHeader );
    if ( nBytesRead < ADM_MIN_HEADER_SIZE )
    {
	CPLDebug( "FAST", "Header file too short. Reading failed" );
	delete poDS;
	return NULL;
    }
    pszHeader[nBytesRead] = '\0';

    // Read acquisition date
    pszTemp = GetValue( pszHeader, ACQUISITION_DATE,
                        ACQUISITION_DATE_SIZE, TRUE );
    poDS->SetMetadataItem( "ACQUISITION_DATE", pszTemp );
    CPLFree( pszTemp );

    // Read satellite name (will read the first one only)
    pszTemp = GetValue( pszHeader, SATELLITE_NAME, SATELLITE_NAME_SIZE, TRUE );
    poDS->SetMetadataItem( "SATELLITE", pszTemp );
    if ( EQUALN(pszTemp, "LANDSAT", 7) )
	poDS->iSatellite = LANDSAT;
    else if ( EQUALN(pszTemp, "IRS", 3) )
	poDS->iSatellite = IRS;
    else
	poDS->iSatellite = IRS;
    CPLFree( pszTemp );

    // Read sensor name (will read the first one only)
    pszTemp = GetValue( pszHeader, SENSOR_NAME, SENSOR_NAME_SIZE, TRUE );
    poDS->SetMetadataItem( "SENSOR", pszTemp );
    CPLFree( pszTemp );

    // Read filenames
    pszTemp = pszHeader;
    poDS->nBands = 0;
    for ( i = 0; i < 6; i++ )
    {
        char *pszFilename = NULL ;

        if ( pszTemp )
            pszTemp = strstr( pszTemp, FILENAME );
        if ( pszTemp )
        {
            pszTemp += strlen(FILENAME);
            pszFilename = CPLScanString( pszTemp, FILENAME_SIZE, TRUE, FALSE );
        }
        else
            pszTemp = NULL;
        if ( poDS->FOpenChannel( pszFilename, poDS->nBands ) )
            poDS->nBands++;
        if ( pszFilename )
            CPLFree( pszFilename );
    }

    if ( !poDS->nBands )
    {
	CPLDebug( "FAST", "Failed to find and open band data files." );
	delete poDS;
	return NULL;
    }

    // Read number of pixels/lines and bit depth
    pszTemp = GetValue( pszHeader, PIXELS, PIXELS_SIZE, TRUE );
    if ( pszTemp )
        poDS->nRasterXSize = atoi( pszTemp );
    else
    {
        CPLDebug( "FAST", "Failed to find number of pixels in line." );
        delete poDS;
	return NULL;
    }

    pszTemp = GetValue( pszHeader, LINES1, LINES_SIZE, TRUE );
    if ( !pszTemp )
        pszTemp = GetValue( pszHeader, LINES2, LINES_SIZE, TRUE );
    if ( pszTemp )
        poDS->nRasterYSize = atoi( pszTemp );
    else
    {
        CPLDebug( "FAST", "Failed to find number of lines in raster." );
        delete poDS;
	return NULL;
    }

    pszTemp = GetValue( pszHeader, BITS_PER_PIXEL, BITS_PER_PIXEL_SIZE, TRUE );
    if ( pszTemp )
    {
        switch( atoi(pszTemp) )
        {
            case 8:
            default:
                poDS->eDataType = GDT_Byte;
                break;
            case 16:
                poDS->eDataType = GDT_UInt16;
                break;
        }
    }
    else
        poDS->eDataType = GDT_Byte;

/* -------------------------------------------------------------------- */
/*  Read radiometric record.    					*/
/* -------------------------------------------------------------------- */
    // Read gains and biases. This is a trick!
    pszTemp = strstr( pszHeader, "BIASES" );// It may be "BIASES AND GAINS"
                                            // or "GAINS AND BIASES"
    // Now search for the first number occurance after that string
    for ( i = 1; i <= poDS->nBands; i++ )
    {
        char *pszValue = NULL;

        pszTemp = strpbrk( pszTemp, "-.0123456789" );
        if ( pszTemp )
        {
            pszValue = CPLScanString( pszTemp, VALUE_SIZE, TRUE, TRUE );
            poDS->SetMetadataItem( CPLSPrintf("BIAS%d", i ), pszValue );
        }
        pszTemp += VALUE_SIZE;
        if ( pszValue )
            CPLFree( pszValue );
        pszTemp = strpbrk( pszTemp, "-.0123456789" );
        if ( pszTemp )
        {
            pszValue = CPLScanString( pszTemp, VALUE_SIZE, TRUE, TRUE );
            poDS->SetMetadataItem( CPLSPrintf("GAIN%d", i ), pszValue );
        }
        pszTemp += VALUE_SIZE;
        if ( pszValue )
            CPLFree( pszValue );
    }

/* -------------------------------------------------------------------- */
/*  Read geometric record.					        */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;
    long        iProjSys, iZone, iDatum;
    // Coordinates of pixel's centers
    double	dfULX = 0.0, dfULY = 0.0;
    double	dfURX = 0.0, dfURY = 0.0;
    double	dfLLX = 0.0, dfLLY = 0.0;
    double	dfLRX = 0.0, dfLRY = 0.0;
    double      adfProjParms[15];

    // Read projection name
    pszTemp = GetValue( pszHeader, PROJECTION_NAME,
                        PROJECTION_NAME_SIZE, FALSE );
    if ( pszTemp && !EQUAL( pszTemp, "" ) )
        iProjSys = USGSMnemonicToCode( pszTemp );
    else
        iProjSys = 1L;  // UTM by default
    CPLFree( pszTemp );

    // Read ellipsoid name
    pszTemp = GetValue( pszHeader, ELLIPSOID_NAME, ELLIPSOID_NAME_SIZE, FALSE );
    if ( pszTemp && !EQUAL( pszTemp, "" ) )
        iDatum = USGSEllipsoidToCode( pszTemp );
    else
        iDatum = 0L;   // Clarke, 1866 (NAD1927) by default
    CPLFree( pszTemp );

    // Read zone number
    pszTemp = GetValue( pszHeader, ZONE_NUMBER, ZONE_NUMBER_SIZE, FALSE );
    if ( pszTemp && !EQUAL( pszTemp, "" ) )
        iZone = atoi( pszTemp );
    else
        iZone = 0L;
    CPLFree( pszTemp );

    // Read 15 USGS projection parameters
    for ( i = 0; i < 15; i++ )
        adfProjParms[i] = 0.0;
    pszTemp = strstr( pszHeader, USGS_PARAMETERS );
    if ( pszTemp && !EQUAL( pszTemp, "" ) )
    {
        pszTemp += strlen( USGS_PARAMETERS );
        for ( i = 0; i < 15; i++ )
        {
            pszTemp = strpbrk( pszTemp, "-.0123456789" );
            if ( pszTemp )
            {
                adfProjParms[i] = CPLScanDouble( pszTemp, VALUE_SIZE, "C" );
#if DEBUG
                CPLDebug("FAST", "USGS parameter %2d=%f.", i, adfProjParms[i]);
#endif
            }
            pszTemp = strpbrk( pszTemp, " \t" );
        }
    }

    // Read corner coordinates
    pszTemp = strstr( pszHeader, CORNER_UPPER_LEFT );
    if ( pszTemp && !EQUAL( pszTemp, "" ) )
    {
        pszTemp += strlen( CORNER_UPPER_LEFT ) + 28;
        dfULX = CPLScanDouble( pszTemp, CORNER_VALUE_SIZE, "C" );
        pszTemp += CORNER_VALUE_SIZE + 1;
        dfULY = CPLScanDouble( pszTemp, CORNER_VALUE_SIZE, "C" );
    }

    pszTemp = strstr( pszHeader, CORNER_UPPER_RIGHT );
    if ( pszTemp && !EQUAL( pszTemp, "" ) )
    {
        pszTemp += strlen( CORNER_UPPER_RIGHT ) + 28;
        dfURX = CPLScanDouble( pszTemp, CORNER_VALUE_SIZE, "C" );
        pszTemp += CORNER_VALUE_SIZE + 1;
        dfURY = CPLScanDouble( pszTemp, CORNER_VALUE_SIZE, "C" );
    }

    pszTemp = strstr( pszHeader, CORNER_LOWER_LEFT );
    if ( pszTemp && !EQUAL( pszTemp, "" ) )
    {
        pszTemp += strlen( CORNER_LOWER_LEFT ) + 28;
        dfLLX = CPLScanDouble( pszTemp, CORNER_VALUE_SIZE, "C" );
        pszTemp += CORNER_VALUE_SIZE + 1;
        dfLLY = CPLScanDouble( pszTemp, CORNER_VALUE_SIZE, "C" );
    }

    pszTemp = strstr( pszHeader, CORNER_LOWER_RIGHT );
    if ( pszTemp && !EQUAL( pszTemp, "" ) )
    {
        pszTemp += strlen( CORNER_LOWER_RIGHT ) + 28;
        dfLRX = CPLScanDouble( pszTemp, CORNER_VALUE_SIZE, "C" );
        pszTemp += CORNER_VALUE_SIZE + 1;
        dfLRY = CPLScanDouble( pszTemp, CORNER_VALUE_SIZE, "C" );
    }

    if ( dfULX != 0.0 && dfULY != 0.0
         && dfURX != 0.0 && dfURY != 0.0
         && dfLLX != 0.0 && dfLLY != 0.0
         && dfLRX != 0.0 && dfLRY != 0.0 )
    {
        int transform_ok=FALSE;
        GDAL_GCP *pasGCPList;

        // Strip out zone number from the easting values, if either
        if ( dfULX >= 1000000.0 )
            dfULX -= (double)iZone * 1000000.0;
        if ( dfURX >= 1000000.0 )
            dfURX -= (double)iZone * 1000000.0;
        if ( dfLLX >= 1000000.0 )
            dfLLX -= (double)iZone * 1000000.0;
        if ( dfLRX >= 1000000.0 )
            dfLRX -= (double)iZone * 1000000.0;

        // Create projection definition
        oSRS.importFromUSGS( iProjSys, iZone, adfProjParms, iDatum );
        oSRS.SetLinearUnits( SRS_UL_METER, 1.0 );
        
        // Read datum name
        pszTemp = GetValue( pszHeader, DATUM_NAME, DATUM_NAME_SIZE, FALSE );
        if ( EQUAL( pszTemp, "WGS84" ) )
            oSRS.SetWellKnownGeogCS( "WGS84" );
        else if ( EQUAL( pszTemp, "NAD27" ) )
            oSRS.SetWellKnownGeogCS( "NAD27" );
        else if ( EQUAL( pszTemp, "NAD83" ) )
            oSRS.SetWellKnownGeogCS( "NAD83" );
        CPLFree( pszTemp );

        if ( poDS->pszProjection )
            CPLFree( poDS->pszProjection );
        oSRS.exportToWkt( &poDS->pszProjection );

        // Generate GCPs
        pasGCPList = (GDAL_GCP *) CPLCalloc( sizeof( GDAL_GCP ), 4 );
        GDALInitGCPs( 4, pasGCPList );

        pasGCPList[0].pszId = "UPPER_LEFT";
        pasGCPList[0].dfGCPX = dfULX;
        pasGCPList[0].dfGCPY = dfULY;
        pasGCPList[0].dfGCPZ = 0.0;
        pasGCPList[0].dfGCPPixel = 0.5;
        pasGCPList[0].dfGCPLine = 0.5;
        pasGCPList[1].pszId = "UPPER_RIGHT";
        pasGCPList[1].dfGCPX = dfURX;
        pasGCPList[1].dfGCPY = dfURY;
        pasGCPList[1].dfGCPZ = 0.0;
        pasGCPList[1].dfGCPPixel = poDS->nRasterXSize-0.5;
        pasGCPList[1].dfGCPLine = 0.5;
        pasGCPList[2].pszId = "LOWER_LEFT";
        pasGCPList[2].dfGCPX = dfLLX;
        pasGCPList[2].dfGCPY = dfLLY;
        pasGCPList[2].dfGCPZ = 0.0;
        pasGCPList[2].dfGCPPixel = 0.5;
        pasGCPList[2].dfGCPLine = poDS->nRasterYSize-0.5;
        pasGCPList[3].pszId = "LOWER_RIGHT";
        pasGCPList[3].dfGCPX = dfLRX;
        pasGCPList[3].dfGCPY = dfLRY;
        pasGCPList[3].dfGCPZ = 0.0;
        pasGCPList[3].dfGCPPixel = poDS->nRasterXSize-0.5;
        pasGCPList[3].dfGCPLine = poDS->nRasterYSize-0.5;

        // Calculate transformation matrix, if accurate
        transform_ok = GDALGCPsToGeoTransform(4,pasGCPList,poDS->adfGeoTransform,0);
        if (transform_ok == FALSE)
        {
            
            poDS->adfGeoTransform[0] = 0.0;
            poDS->adfGeoTransform[1] = 1.0;
            poDS->adfGeoTransform[2] = 0.0;
            poDS->adfGeoTransform[3] = 0.0;
            poDS->adfGeoTransform[4] = 0.0;
            poDS->adfGeoTransform[5] = 1.0;
            if ( poDS->pszProjection )
                CPLFree( poDS->pszProjection );
            poDS->pszProjection = CPLStrdup("");
        }
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int	nPixelOffset = GDALGetDataTypeSize(poDS->eDataType) / 8;
    int nLineOffset = poDS->nRasterXSize * nPixelOffset;

    for( i = 1; i <= poDS->nBands; i++ )
        poDS->SetBand( i, new FASTRasterBand( poDS, i, poDS->fpChannels[i - 1],
	    0, nPixelOffset, nLineOffset, poDS->eDataType, TRUE));

    CPLFree( pszHeader );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();
    
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

