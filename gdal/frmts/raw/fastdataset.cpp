/******************************************************************************
 * $Id$
 *
 * Project:  EOSAT FAST Format reader
 * Purpose:  Reads Landsat FAST-L7A, IRS 1C/1D
 * Author:   Andrey Kiselev, dron@ak4719.spb.edu
 *
 ******************************************************************************
 * Copyright (c) 2002, Andrey Kiselev <dron@ak4719.spb.edu>
 * Copyright (c) 2007-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#define BANDS_PRESENT           "BANDS PRESENT"
#define BANDS_PRESENT_SIZE      32

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

#define CORNER_UPPER_LEFT       "UL "
#define CORNER_UPPER_RIGHT      "UR "
#define CORNER_LOWER_LEFT       "LL "
#define CORNER_LOWER_RIGHT      "LR "
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

    VSILFILE	*fpHeader;
    CPLString apoChannelFilenames[7];
    VSILFILE	*fpChannels[7];
    const char	*pszFilename;
    char	*pszDirname;
    GDALDataType eDataType;
    FASTSatellite iSatellite;
    
    int         OpenChannel( const char *pszFilename, int iBand );

  public:
                FASTDataset();
		~FASTDataset();
    
    static GDALDataset *Open( GDALOpenInfo * );

    CPLErr 	GetGeoTransform( double * );
    const char	*GetProjectionRef();
    VSILFILE	*FOpenChannel( const char *, int iBand, int iFASTBand );
    void        TryEuromap_IRS_1C_1D_ChannelNameConvention();
    
    virtual  char** GetFileList();
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

    		FASTRasterBand( FASTDataset *, int, VSILFILE *, vsi_l_offset,
				int, int, GDALDataType, int );
};


/************************************************************************/
/*                           FASTRasterBand()                           */
/************************************************************************/

FASTRasterBand::FASTRasterBand( FASTDataset *poDS, int nBand, VSILFILE * fpRaw,
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
        VSIFCloseL( fpHeader );
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
/*                             GetFileList()                            */
/************************************************************************/

char** FASTDataset::GetFileList()
{
    char** papszFileList = GDALPamDataset::GetFileList();
    int i;
    for(i=0;i<6;i++)
    {
        if (apoChannelFilenames[i].size() > 0)
            papszFileList =
                CSLAddString(papszFileList, apoChannelFilenames[i].c_str());
    }
    
    return papszFileList;
}

/************************************************************************/
/*                             OpenChannel()                            */
/************************************************************************/

int FASTDataset::OpenChannel( const char *pszFilename, int iBand )
{
    fpChannels[iBand] = VSIFOpenL( pszFilename, "rb" );
    if (fpChannels[iBand])
        apoChannelFilenames[iBand] = pszFilename;
    return fpChannels[iBand] != NULL;
}

/************************************************************************/
/*                             FOpenChannel()                           */
/************************************************************************/


VSILFILE *FASTDataset::FOpenChannel( const char *pszBandname, int iBand, int iFASTBand )
{
    const char	*pszChannelFilename = NULL;
    char	*pszPrefix = CPLStrdup( CPLGetBasename( pszFilename ) );
    char	*pszSuffix = CPLStrdup( CPLGetExtension( pszFilename ) );

    fpChannels[iBand] = NULL;
    
    switch ( iSatellite )
    {
	case LANDSAT:
            if ( pszBandname && !EQUAL( pszBandname, "" ) )
            {
                pszChannelFilename =
                    CPLFormCIFilename( pszDirname, pszBandname, NULL );
                if ( OpenChannel( pszChannelFilename, iBand ) )
                    break;
                pszChannelFilename =
                    CPLFormFilename( pszDirname,
                            CPLSPrintf( "%s.b%02d", pszPrefix, iFASTBand ),
                            NULL );
                OpenChannel( pszChannelFilename, iBand );
            }
            break;
	case IRS:
	default:
            pszChannelFilename = CPLFormFilename( pszDirname,
                CPLSPrintf( "%s.%d", pszPrefix, iFASTBand ), pszSuffix );
            if ( OpenChannel( pszChannelFilename, iBand ) )
                break;
            pszChannelFilename = CPLFormFilename( pszDirname,
                CPLSPrintf( "IMAGERY%d", iFASTBand ), pszSuffix );
            if ( OpenChannel( pszChannelFilename, iBand ) )
                break;
            pszChannelFilename = CPLFormFilename( pszDirname,
                CPLSPrintf( "imagery%d", iFASTBand ), pszSuffix );
            if ( OpenChannel( pszChannelFilename, iBand ) )
                break;
            pszChannelFilename = CPLFormFilename( pszDirname,
                CPLSPrintf( "IMAGERY%d.DAT", iFASTBand ), NULL );
            if ( OpenChannel( pszChannelFilename, iBand ) )
                break;
            pszChannelFilename = CPLFormFilename( pszDirname,
                CPLSPrintf( "imagery%d.dat", iFASTBand ), NULL );
            if ( OpenChannel( pszChannelFilename, iBand ) )
                break;
            pszChannelFilename = CPLFormFilename( pszDirname,
                CPLSPrintf( "IMAGERY%d.dat", iFASTBand ), NULL );
            if ( OpenChannel( pszChannelFilename, iBand ) )
                break;
            pszChannelFilename = CPLFormFilename( pszDirname,
                CPLSPrintf( "imagery%d.DAT", iFASTBand ), NULL );
            if ( OpenChannel( pszChannelFilename, iBand ) )
                break;
            pszChannelFilename = CPLFormFilename( pszDirname,
                CPLSPrintf( "BAND%d", iFASTBand ), pszSuffix );
            if ( OpenChannel( pszChannelFilename, iBand ) )
                break;
            pszChannelFilename = CPLFormFilename( pszDirname,
                CPLSPrintf( "band%d", iFASTBand ), pszSuffix );
            if ( OpenChannel( pszChannelFilename, iBand ) )
                break;
            pszChannelFilename = CPLFormFilename( pszDirname,
                CPLSPrintf( "BAND%d.DAT", iFASTBand ), NULL );
            if ( OpenChannel( pszChannelFilename, iBand ) )
                break;
            pszChannelFilename = CPLFormFilename( pszDirname,
                CPLSPrintf( "band%d.dat", iFASTBand ), NULL );
            if ( OpenChannel( pszChannelFilename, iBand ) )
                break;
            pszChannelFilename = CPLFormFilename( pszDirname,
                CPLSPrintf( "BAND%d.dat", iFASTBand ), NULL );
            if ( OpenChannel( pszChannelFilename, iBand ) )
                break;
            pszChannelFilename = CPLFormFilename( pszDirname,
                CPLSPrintf( "band%d.DAT", iFASTBand ), NULL );
            OpenChannel( pszChannelFilename, iBand );
            break;
    }
    
    CPLDebug( "FAST", "Band %d filename=%s", iBand + 1, pszChannelFilename);

    CPLFree( pszPrefix );
    CPLFree( pszSuffix );
    return fpChannels[iBand];
}

/************************************************************************/
/*                TryEuromap_IRS_1C_1D_ChannelNameConvention()                     */
/************************************************************************/

void FASTDataset::TryEuromap_IRS_1C_1D_ChannelNameConvention()
{
    /* Filename convention explained in http://www.euromap.de/download/em_names.pdf */

    char chLastLetterHeader = pszFilename[strlen(pszFilename)-1];
    if (EQUAL(GetMetadataItem("SENSOR"), "PAN"))
    {
        /* Converting upper-case to lower case */
        if (chLastLetterHeader >= 'A' && chLastLetterHeader <= 'M')
            chLastLetterHeader += 'a' - 'A';

        if (chLastLetterHeader >= 'a' && chLastLetterHeader <= 'j')
        {
            char chLastLetterData = chLastLetterHeader - 'a' + '0';
            char* pszChannelFilename = CPLStrdup(pszFilename);
            pszChannelFilename[strlen(pszChannelFilename)-1] = chLastLetterData;
            if (OpenChannel( pszChannelFilename, 0 ))
                nBands++;
            else
                CPLDebug("FAST", "Could not find %s", pszChannelFilename);
            CPLFree(pszChannelFilename);
        }
        else if (chLastLetterHeader >= 'k' && chLastLetterHeader <= 'm')
        {
            char chLastLetterData = chLastLetterHeader - 'k' + 'n';
            char* pszChannelFilename = CPLStrdup(pszFilename);
            pszChannelFilename[strlen(pszChannelFilename)-1] = chLastLetterData;
            if (OpenChannel( pszChannelFilename, 0 ))
                nBands++;
            else
            {
                /* Trying upper-case */
                pszChannelFilename[strlen(pszChannelFilename)-1] = chLastLetterData - 'a' + 'A';
                if (OpenChannel( pszChannelFilename, 0 ))
                    nBands++;
                else
                    CPLDebug("FAST", "Could not find %s", pszChannelFilename);
            }
            CPLFree(pszChannelFilename);
        }
        else
        {
            CPLDebug("FAST", "Unknown last letter (%c) for a IRS PAN Euromap FAST dataset", chLastLetterHeader);
        }
    }
    else if (EQUAL(GetMetadataItem("SENSOR"), "LISS3"))
    {
        const char apchLISSFilenames[7][5] = {
            { '0', '2', '3', '4', '5' },
            { '6', '7', '8', '9', 'a' },
            { 'b', 'c', 'd', 'e', 'f' },
            { 'g', 'h', 'i', 'j', 'k' },
            { 'l', 'm', 'n', 'o', 'p' },
            { 'q', 'r', 's', 't', 'u' },
            { 'v', 'w', 'x', 'y', 'z' } };
        int i;
        for (i = 0; i < 7 ; i++)
        {
            if (chLastLetterHeader == apchLISSFilenames[i][0] ||
                (apchLISSFilenames[i][0] >= 'a' && apchLISSFilenames[i][0] <= 'z' &&
                    (apchLISSFilenames[i][0] - chLastLetterHeader == 0 ||
                    apchLISSFilenames[i][0] - chLastLetterHeader == 32)))
            {
                for (int j = 0; j < 4; j ++)
                {
                    char* pszChannelFilename = CPLStrdup(pszFilename);
                    pszChannelFilename[strlen(pszChannelFilename)-1] = apchLISSFilenames[i][j+1];
                    if (OpenChannel( pszChannelFilename, nBands ))
                        nBands++;
                    else if (apchLISSFilenames[i][j+1] >= 'a' && apchLISSFilenames[i][j+1] <= 'z')
                    {
                        /* Trying upper-case */
                        pszChannelFilename[strlen(pszChannelFilename)-1] = apchLISSFilenames[i][j+1] - 'a' + 'A';
                        if (OpenChannel( pszChannelFilename, nBands ))
                            nBands++;
                        else
                        {
                            CPLDebug("FAST", "Could not find %s", pszChannelFilename);
                        }
                    }
                    else
                    {
                        CPLDebug("FAST", "Could not find %s", pszChannelFilename);
                    }
                    CPLFree(pszChannelFilename);
                }
                break;
            }
        }
        if (i == 7)
        {
            CPLDebug("FAST", "Unknown last letter (%c) for a IRS LISS3 Euromap FAST dataset", chLastLetterHeader);
        }
    }
    else if (EQUAL(GetMetadataItem("SENSOR"), "WIFS"))
    {
        if (chLastLetterHeader == '0')
        {
            for (int j = 0; j < 2; j ++)
            {
                char* pszChannelFilename = CPLStrdup(pszFilename);
                pszChannelFilename[strlen(pszChannelFilename)-1] 
                    = (char) ('1' + j);
                if (OpenChannel( pszChannelFilename, nBands ))
                    nBands++;
                else
                {
                    CPLDebug("FAST", "Could not find %s", pszChannelFilename);
                }
                CPLFree(pszChannelFilename);
            }
        }
        else
        {
            CPLDebug("FAST", "Unknown last letter (%c) for a IRS WIFS Euromap FAST dataset", chLastLetterHeader);
        }
    }
    else
    {
        CPLAssert(0);
    }
}

/************************************************************************/
/*                          GetValue()                                  */
/************************************************************************/

static char *GetValue( const char *pszString, const char *pszName,
                       int iValueSize, int bNormalize )
{
    char    *pszTemp = strstr( (char *) pszString, pszName );

    if ( pszTemp )
    {
        // Skip the parameter name
        pszTemp += strlen( pszName );
        // Skip whitespaces and equal signs
        while ( *pszTemp == ' ' )
            pszTemp++;
        while ( *pszTemp == '=' )
            pszTemp++;

        pszTemp = CPLScanString( pszTemp, iValueSize, TRUE, bNormalize );
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

    if( poOpenInfo->nHeaderBytes < 1024)
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

    poDS->fpHeader = VSIFOpenL(poOpenInfo->pszFilename, "rb");
    if (poDS->fpHeader == NULL)
    {
        delete poDS;
        return NULL;
    }

    poDS->pszFilename = poOpenInfo->pszFilename;
    poDS->pszDirname = CPLStrdup( CPLGetDirname( poOpenInfo->pszFilename ) );
    
/* -------------------------------------------------------------------- */
/*  Read the administrative record.                                     */
/* -------------------------------------------------------------------- */
    char	*pszTemp;
    char	*pszHeader = (char *) CPLMalloc( ADM_HEADER_SIZE + 1 );
    size_t      nBytesRead;
 
    VSIFSeekL( poDS->fpHeader, 0, SEEK_SET );
    nBytesRead = VSIFReadL( pszHeader, 1, ADM_HEADER_SIZE, poDS->fpHeader );
    if ( nBytesRead < ADM_MIN_HEADER_SIZE )
    {
	CPLDebug( "FAST", "Header file too short. Reading failed" );
        CPLFree(pszHeader);
	delete poDS;
	return NULL;
    }
    pszHeader[nBytesRead] = '\0';

    // Read acquisition date
    pszTemp = GetValue( pszHeader, ACQUISITION_DATE,
                        ACQUISITION_DATE_SIZE, TRUE );
    if (pszTemp == NULL)
    {
        CPLDebug( "FAST", "Cannot get ACQUISITION_DATE, using empty value." );
        pszTemp = CPLStrdup("");
    }
    poDS->SetMetadataItem( "ACQUISITION_DATE", pszTemp );
    CPLFree( pszTemp );

    // Read satellite name (will read the first one only)
    pszTemp = GetValue( pszHeader, SATELLITE_NAME, SATELLITE_NAME_SIZE, TRUE );
    if (pszTemp == NULL)
    {
        CPLDebug( "FAST", "Cannot get SATELLITE_NAME, using empty value." );
        pszTemp = CPLStrdup( "" );
    }
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
    if (pszTemp == NULL)
    {
        CPLDebug( "FAST", "Cannot get SENSOR_NAME, using empty value." );
        pszTemp = CPLStrdup("");
    }
    poDS->SetMetadataItem( "SENSOR", pszTemp );
    CPLFree( pszTemp );

    // Read filenames
    poDS->nBands = 0;
    
    if (strstr( pszHeader, FILENAME ) == NULL)
    {
        if (strstr(pszHeader, "GENERATING AGENCY =EUROMAP"))
        {
            /* If we don't find the FILENAME field, let's try with the Euromap */
            /* PAN / LISS3 / WIFS IRS filename convention */
            if ((EQUAL(poDS->GetMetadataItem("SATELLITE"), "IRS 1C") ||
                 EQUAL(poDS->GetMetadataItem("SATELLITE"), "IRS 1D")) &&
                (EQUAL(poDS->GetMetadataItem("SENSOR"), "PAN") ||
                 EQUAL(poDS->GetMetadataItem("SENSOR"), "LISS3") ||
                 EQUAL(poDS->GetMetadataItem("SENSOR"), "WIFS")))
            {
                poDS->TryEuromap_IRS_1C_1D_ChannelNameConvention();
            }
            else if (EQUAL(poDS->GetMetadataItem("SATELLITE"), "CARTOSAT-1") &&
                     (EQUAL(poDS->GetMetadataItem("SENSOR"), "FORE") ||
                      EQUAL(poDS->GetMetadataItem("SENSOR"), "AFT")))
            {
                /* See http://www.euromap.de/download/p5fast_20050301.pdf, appendix F */
                CPLString     osSuffix = CPLGetExtension( poDS->pszFilename );
                const char    *papszBasenames[] = { "BANDF", "bandf", "BANDA", "banda" };
                for ( i=0;i<4;i++)
                {
                    CPLString osChannelFilename = CPLFormFilename( poDS->pszDirname, papszBasenames[i], osSuffix );
                    if (poDS->OpenChannel( osChannelFilename, 0 ))
                    {
                        poDS->nBands = 1;
                        break;
                    }
                }
            }
            else if (EQUAL(poDS->GetMetadataItem("SATELLITE"), "IRS P6"))
            {
                /* If BANDS_PRESENT="2345", the file bands are "BAND2.DAT", "BAND3.DAT", etc. */
                pszTemp = GetValue( pszHeader, BANDS_PRESENT, BANDS_PRESENT_SIZE, TRUE );
                if (pszTemp)
                {
                    for( i=0; pszTemp[i] != '\0'; i++)
                    {
                        if (pszTemp[i] >= '2' && pszTemp[i] <= '5')
                        {
                            if (poDS->FOpenChannel(poDS->pszFilename, poDS->nBands, pszTemp[i] - '0'))
                                poDS->nBands++;
                        }
                    }
                    CPLFree( pszTemp );
                }
            }
        }
    }
    
    /* If the previous lookup for band files didn't success, fallback to the */
    /* standard way of finding them, either by the FILENAME field, either with */
    /* the usual patterns like bandX.dat, etc... */
    if ( !poDS->nBands )
    {
        pszTemp = pszHeader;
        for ( i = 0; i < 7; i++ )
        {
            char *pszFilename = NULL ;
            if ( pszTemp )
                pszTemp = strstr( pszTemp, FILENAME );
            if ( pszTemp )
            {
                // Skip the parameter name
                pszTemp += strlen(FILENAME);
                // Skip whitespaces and equal signs
                while ( *pszTemp == ' ' )
                    pszTemp++;
                while ( *pszTemp == '=' )
                    pszTemp++;
                pszFilename = CPLScanString( pszTemp, FILENAME_SIZE, TRUE, FALSE );
            }
            else
                pszTemp = NULL;
            if ( poDS->FOpenChannel( pszFilename, poDS->nBands, poDS->nBands + 1 ) )
                poDS->nBands++;
            if ( pszFilename )
                CPLFree( pszFilename );
        }
    }

    if ( !poDS->nBands )
    {
	CPLError( CE_Failure, CPLE_NotSupported,
                  "Failed to find and open band data files." );
        CPLFree(pszHeader);
	delete poDS;
	return NULL;
    }

    // Read number of pixels/lines and bit depth
    pszTemp = GetValue( pszHeader, PIXELS, PIXELS_SIZE, FALSE );
    if ( pszTemp )
    {
        poDS->nRasterXSize = atoi( pszTemp );
        CPLFree( pszTemp );
    }
    else
    {
        CPLDebug( "FAST", "Failed to find number of pixels in line." );
        CPLFree(pszHeader);
        delete poDS;
	return NULL;
    }

    pszTemp = GetValue( pszHeader, LINES1, LINES_SIZE, FALSE );
    if ( !pszTemp )
        pszTemp = GetValue( pszHeader, LINES2, LINES_SIZE, FALSE );
    if ( pszTemp )
    {
        poDS->nRasterYSize = atoi( pszTemp );
        CPLFree( pszTemp );
    }
    else
    {
        CPLDebug( "FAST", "Failed to find number of lines in raster." );
        CPLFree(pszHeader);
        delete poDS;
	return NULL;
    }


    
    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        CPLFree(pszHeader);
        delete poDS;
        return NULL;
    }

    pszTemp = GetValue( pszHeader, BITS_PER_PIXEL, BITS_PER_PIXEL_SIZE, FALSE );
    if ( pszTemp )
    {
        switch( atoi(pszTemp) )
        {
            case 8:
            default:
                poDS->eDataType = GDT_Byte;
                break;
            case 10: /* For a strange reason, some Euromap products declare 10 bits output, but are 16 bits */
            case 16:
                poDS->eDataType = GDT_UInt16;
                break;
        }
        CPLFree( pszTemp );
    }
    else
        poDS->eDataType = GDT_Byte;

/* -------------------------------------------------------------------- */
/*  Read radiometric record.    					*/
/* -------------------------------------------------------------------- */
    const char  *pszFirst, *pszSecond;

    // Read gains and biases. This is a trick!
    pszTemp = strstr( pszHeader, "BIASES" );// It may be "BIASES AND GAINS"
                                            // or "GAINS AND BIASES"
    if ( pszTemp > strstr( pszHeader, "GAINS" ) )
    {
        pszFirst = "GAIN%d";
        pszSecond = "BIAS%d";
    }
    else
    {
        pszFirst = "BIAS%d";
        pszSecond = "GAIN%d";
    }

    // Now search for the first number occurance after that string
    for ( i = 1; i <= poDS->nBands; i++ )
    {
        char    *pszValue = NULL;
        size_t  nValueLen = VALUE_SIZE;

        pszTemp = strpbrk( pszTemp, "-.0123456789" );
        if ( pszTemp )
        {
            nValueLen = strspn( pszTemp, "+-.0123456789" );
            pszValue = CPLScanString( pszTemp, nValueLen, TRUE, TRUE );
            poDS->SetMetadataItem( CPLSPrintf(pszFirst, i ), pszValue );
            CPLFree( pszValue );
        }
        pszTemp += nValueLen;
        pszTemp = strpbrk( pszTemp, "-.0123456789" );
        if ( pszTemp )
        {
            nValueLen = strspn( pszTemp, "+-.0123456789" );
            pszValue = CPLScanString( pszTemp, nValueLen, TRUE, TRUE );
            poDS->SetMetadataItem( CPLSPrintf(pszSecond, i ), pszValue );
            CPLFree( pszValue );
        }
        pszTemp += nValueLen;
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
                adfProjParms[i] = CPLScanDouble( pszTemp, VALUE_SIZE );
            pszTemp = strpbrk( pszTemp, " \t" );
        }
    }

    // Coordinates should follow the word "PROJECTION", otherwise we can
    // be confused by other occurences of the corner keywords.
    char        *pszGeomRecord = strstr( pszHeader, "PROJECTION" );
    // Read corner coordinates
    pszTemp = strstr( pszGeomRecord, CORNER_UPPER_LEFT );
    if ( pszTemp && !EQUAL( pszTemp, "" ) )
    {
        pszTemp += strlen( CORNER_UPPER_LEFT ) + 28;
        dfULX = CPLScanDouble( pszTemp, CORNER_VALUE_SIZE );
        pszTemp += CORNER_VALUE_SIZE + 1;
        dfULY = CPLScanDouble( pszTemp, CORNER_VALUE_SIZE );
    }

    pszTemp = strstr( pszGeomRecord, CORNER_UPPER_RIGHT );
    if ( pszTemp && !EQUAL( pszTemp, "" ) )
    {
        pszTemp += strlen( CORNER_UPPER_RIGHT ) + 28;
        dfURX = CPLScanDouble( pszTemp, CORNER_VALUE_SIZE );
        pszTemp += CORNER_VALUE_SIZE + 1;
        dfURY = CPLScanDouble( pszTemp, CORNER_VALUE_SIZE );
    }

    pszTemp = strstr( pszGeomRecord, CORNER_LOWER_LEFT );
    if ( pszTemp && !EQUAL( pszTemp, "" ) )
    {
        pszTemp += strlen( CORNER_LOWER_LEFT ) + 28;
        dfLLX = CPLScanDouble( pszTemp, CORNER_VALUE_SIZE );
        pszTemp += CORNER_VALUE_SIZE + 1;
        dfLLY = CPLScanDouble( pszTemp, CORNER_VALUE_SIZE );
    }

    pszTemp = strstr( pszGeomRecord, CORNER_LOWER_RIGHT );
    if ( pszTemp && !EQUAL( pszTemp, "" ) )
    {
        pszTemp += strlen( CORNER_LOWER_RIGHT ) + 28;
        dfLRX = CPLScanDouble( pszTemp, CORNER_VALUE_SIZE );
        pszTemp += CORNER_VALUE_SIZE + 1;
        dfLRY = CPLScanDouble( pszTemp, CORNER_VALUE_SIZE );
    }

    if ( dfULX != 0.0 && dfULY != 0.0
         && dfURX != 0.0 && dfURY != 0.0
         && dfLLX != 0.0 && dfLLY != 0.0
         && dfLRX != 0.0 && dfLRY != 0.0 )
    {
        int transform_ok=FALSE;
        GDAL_GCP *pasGCPList;
        int bAnglesInPackedDMSFormat;

        // Strip out zone number from the easting values, if either
        if ( dfULX >= 1000000.0 )
            dfULX -= (double)iZone * 1000000.0;
        if ( dfURX >= 1000000.0 )
            dfURX -= (double)iZone * 1000000.0;
        if ( dfLLX >= 1000000.0 )
            dfLLX -= (double)iZone * 1000000.0;
        if ( dfLRX >= 1000000.0 )
            dfLRX -= (double)iZone * 1000000.0;

        // In EOSAT FAST Rev C, the angles are in decimal degrees
        // otherwise they are in packed DMS format.
        if (strstr(pszHeader, "REV            C") != NULL)
            bAnglesInPackedDMSFormat = FALSE;
        else
            bAnglesInPackedDMSFormat = TRUE;

        // Create projection definition
        OGRErr eErr =
            oSRS.importFromUSGS( iProjSys, iZone, adfProjParms, iDatum, bAnglesInPackedDMSFormat );
        if ( eErr != OGRERR_NONE )
            CPLDebug("FAST", "Import projection from USGS failed: %d", eErr);
        oSRS.SetLinearUnits( SRS_UL_METER, 1.0 );
        
        // Read datum name
        pszTemp = GetValue( pszHeader, DATUM_NAME, DATUM_NAME_SIZE, FALSE );
        if ( pszTemp )
        {
            if ( EQUAL( pszTemp, "WGS84" ) )
                oSRS.SetWellKnownGeogCS( "WGS84" );
            else if ( EQUAL( pszTemp, "NAD27" ) )
                oSRS.SetWellKnownGeogCS( "NAD27" );
            else if ( EQUAL( pszTemp, "NAD83" ) )
                oSRS.SetWellKnownGeogCS( "NAD83" );
            CPLFree( pszTemp );
        }
        else
        {
            // Reasonable fallback
            oSRS.SetWellKnownGeogCS( "WGS84" );
        }

        if ( poDS->pszProjection )
            CPLFree( poDS->pszProjection );
        eErr = oSRS.exportToWkt( &poDS->pszProjection );
        if ( eErr != OGRERR_NONE )
            CPLDebug("FAST", "Export projection to WKT USGS failed: %d", eErr);

        // Generate GCPs
        pasGCPList = (GDAL_GCP *) CPLCalloc( sizeof( GDAL_GCP ), 4 );
        GDALInitGCPs( 4, pasGCPList );
        CPLFree(pasGCPList[0].pszId);
        CPLFree(pasGCPList[1].pszId);
        CPLFree(pasGCPList[2].pszId);
        CPLFree(pasGCPList[3].pszId);

        /* Let's order the GCP in TL, TR, BR, BL order to benefit from the */
        /* GDALGCPsToGeoTransform optimization */ 
        pasGCPList[0].pszId = CPLStrdup("UPPER_LEFT");
        pasGCPList[0].dfGCPX = dfULX;
        pasGCPList[0].dfGCPY = dfULY;
        pasGCPList[0].dfGCPZ = 0.0;
        pasGCPList[0].dfGCPPixel = 0.5;
        pasGCPList[0].dfGCPLine = 0.5;
        pasGCPList[1].pszId = CPLStrdup("UPPER_RIGHT");
        pasGCPList[1].dfGCPX = dfURX;
        pasGCPList[1].dfGCPY = dfURY;
        pasGCPList[1].dfGCPZ = 0.0;
        pasGCPList[1].dfGCPPixel = poDS->nRasterXSize-0.5;
        pasGCPList[1].dfGCPLine = 0.5;
        pasGCPList[2].pszId = CPLStrdup("LOWER_RIGHT");
        pasGCPList[2].dfGCPX = dfLRX;
        pasGCPList[2].dfGCPY = dfLRY;
        pasGCPList[2].dfGCPZ = 0.0;
        pasGCPList[2].dfGCPPixel = poDS->nRasterXSize-0.5;
        pasGCPList[2].dfGCPLine = poDS->nRasterYSize-0.5;
        pasGCPList[3].pszId = CPLStrdup("LOWER_LEFT");
        pasGCPList[3].dfGCPX = dfLLX;
        pasGCPList[3].dfGCPY = dfLLY;
        pasGCPList[3].dfGCPZ = 0.0;
        pasGCPList[3].dfGCPPixel = 0.5;
        pasGCPList[3].dfGCPLine = poDS->nRasterYSize-0.5;

        // Calculate transformation matrix, if accurate
        transform_ok =
            GDALGCPsToGeoTransform(4,pasGCPList,poDS->adfGeoTransform,0);
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

        GDALDeinitGCPs(4, pasGCPList);
        CPLFree(pasGCPList);
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int	nPixelOffset = GDALGetDataTypeSize(poDS->eDataType) / 8;
    int nLineOffset = poDS->nRasterXSize * nPixelOffset;

    for( i = 1; i <= poDS->nBands; i++ )
    {
        poDS->SetBand( i, new FASTRasterBand( poDS, i, poDS->fpChannels[i - 1],
	    0, nPixelOffset, nLineOffset, poDS->eDataType, TRUE));
    }

    CPLFree( pszHeader );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();
    
    // opens overviews.
    poDS->oOvManager.Initialize(poDS, poDS->pszFilename);
    
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        delete poDS;
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The FAST driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
    
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
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = FASTDataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

