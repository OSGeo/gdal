/******************************************************************************
 * $Id$
 *
 * Project:  USGS DOQ Driver (Second Generation Format)
 * Purpose:  Implementation of DOQ2Dataset
 * Author:   Derrick J Brashear, shadow@dementia.org
 *
 ******************************************************************************
 * Copyright (c) 2000, Derrick J Brashear
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "rawdataset.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_DOQ2(void);
CPL_C_END

#define UTM_FORMAT \
"PROJCS[\"%s / UTM zone %dN\",GEOGCS[%s,PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]],PROJECTION[\"Transverse_Mercator\"],PARAMETER[\"latitude_of_origin\",0],PARAMETER[\"central_meridian\",%d],PARAMETER[\"scale_factor\",0.9996],PARAMETER[\"false_easting\",500000],PARAMETER[\"false_northing\",0],%s]"

#define WGS84_DATUM \
"\"WGS 84\",DATUM[\"WGS_1984\",SPHEROID[\"WGS 84\",6378137,298.257223563]]"

#define WGS72_DATUM \
"\"WGS 72\",DATUM[\"WGS_1972\",SPHEROID[\"NWL 10D\",6378135,298.26]]"

#define NAD27_DATUM \
"\"NAD27\",DATUM[\"North_American_Datum_1927\",SPHEROID[\"Clarke 1866\",6378206.4,294.978698213901]]"

#define NAD83_DATUM \
"\"NAD83\",DATUM[\"North_American_Datum_1983\",SPHEROID[\"GRS 1980\",6378137,298.257222101]]"

/************************************************************************/
/* ==================================================================== */
/*				DOQ2Dataset				*/
/* ==================================================================== */
/************************************************************************/

class DOQ2Dataset : public RawDataset
{
    VSILFILE	*fpImage;	// image data file.
    
    double	dfULX, dfULY;
    double	dfXPixelSize, dfYPixelSize;

    char	*pszProjection;

  public:
    		DOQ2Dataset();
    	        ~DOQ2Dataset();

    CPLErr 	GetGeoTransform( double * padfTransform );
    const char  *GetProjectionRef( void );
    
    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            DOQ2Dataset()                             */
/************************************************************************/

DOQ2Dataset::DOQ2Dataset()
{
    pszProjection = NULL;
    fpImage = NULL;
}

/************************************************************************/
/*                            ~DOQ2Dataset()                            */
/************************************************************************/

DOQ2Dataset::~DOQ2Dataset()

{
    FlushCache();

    CPLFree( pszProjection );
    if( fpImage != NULL )
        VSIFCloseL( fpImage );
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr DOQ2Dataset::GetGeoTransform( double * padfTransform )

{
    padfTransform[0] = dfULX;
    padfTransform[1] = dfXPixelSize;
    padfTransform[2] = 0.0;
    padfTransform[3] = dfULY;
    padfTransform[4] = 0.0;
    padfTransform[5] = -1 * dfYPixelSize;
    
    return( CE_None );
}

/************************************************************************/
/*                        GetProjectionString()                         */
/************************************************************************/

const char *DOQ2Dataset::GetProjectionRef()

{
    return pszProjection;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *DOQ2Dataset::Open( GDALOpenInfo * poOpenInfo )

{
    int		nWidth=0, nHeight=0, nBandStorage=0, nBandTypes=0;
    
/* -------------------------------------------------------------------- */
/*	We assume the user is pointing to the binary (ie. .bil) file.	*/
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 212 )
        return NULL;

    int         nLineCount = 0;
    const char *pszLine;
    int		nBytesPerPixel=0;
    const char *pszDatumLong=NULL, *pszDatumShort=NULL;
    const char *pszUnits=NULL;
    char *pszQuadname = NULL;
    char *pszQuadquad = NULL;
    char *pszState = NULL;
    int	        nZone=0, nProjType=0;
    int		nSkipBytes=0, nBytesPerLine, i, nBandCount = 0;
    double      dfULXMap=0.0, dfULYMap = 0.0;
    double      dfXDim=0.0, dfYDim=0.0;
    char	**papszMetadata = NULL;

    if(! EQUALN((const char *) poOpenInfo->pabyHeader,
                "BEGIN_USGS_DOQ_HEADER", 21) )
        return NULL;

    VSILFILE* fp = VSIFOpenL(poOpenInfo->pszFilename, "rb");
    if (fp == NULL)
        return NULL;

    /* read and discard the first line */
    pszLine = CPLReadLineL( fp );

    while( (pszLine = CPLReadLineL( fp )) != NULL )
    {
	char    **papszTokens;

        nLineCount++;

	if( EQUAL(pszLine,"END_USGS_DOQ_HEADER") )
            break;

	papszTokens = CSLTokenizeString( pszLine );
        if( CSLCount( papszTokens ) < 2 )
        {
            CSLDestroy( papszTokens );
            break;
        }
        
        if( EQUAL(papszTokens[0],"SAMPLES_AND_LINES") && CSLCount(papszTokens) >= 3 )
        {
            nWidth = atoi(papszTokens[1]);
            nHeight = atoi(papszTokens[2]);
        }
        else if( EQUAL(papszTokens[0],"BYTE_COUNT") )
        {
            nSkipBytes = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"XY_ORIGIN") && CSLCount(papszTokens) >= 3 )
        {
            dfULXMap = atof(papszTokens[1]);
            dfULYMap = atof(papszTokens[2]);
        }
        else if( EQUAL(papszTokens[0],"HORIZONTAL_RESOLUTION") )
        {
            dfXDim = dfYDim = atof(papszTokens[1]);
        }
	else if( EQUAL(papszTokens[0],"BAND_ORGANIZATION") )
        {
            if( EQUAL(papszTokens[1],"SINGLE FILE") )
                nBandStorage = 1;
            if( EQUAL(papszTokens[1],"BSQ") )
                nBandStorage = 1;
            if( EQUAL(papszTokens[1],"BIL") )
                nBandStorage = 1;
            if( EQUAL(papszTokens[1],"BIP") )
                nBandStorage = 4;
	}
	else if( EQUAL(papszTokens[0],"BAND_CONTENT") )
        {
            if( EQUAL(papszTokens[1],"BLACK&WHITE") )
                nBandTypes = 1;
            else if( EQUAL(papszTokens[1],"COLOR") )
                nBandTypes = 5;
            else if( EQUAL(papszTokens[1],"RGB") )
                nBandTypes = 5;
            else if( EQUAL(papszTokens[1],"RED") )
                nBandTypes = 5;
            else if( EQUAL(papszTokens[1],"GREEN") )
                nBandTypes = 5;
            else if( EQUAL(papszTokens[1],"BLUE") )
                nBandTypes = 5;

            nBandCount++;
        }
        else if( EQUAL(papszTokens[0],"BITS_PER_PIXEL") )
        {
	    nBytesPerPixel = (atoi(papszTokens[1]) / 8);
        }
        else if( EQUAL(papszTokens[0],"HORIZONTAL_COORDINATE_SYSTEM") )
        {
	    if( EQUAL(papszTokens[1],"UTM") ) 
                nProjType = 1;
	    else if( EQUAL(papszTokens[1],"SPCS") ) 
                nProjType = 2;
	    else if( EQUAL(papszTokens[1],"GEOGRAPHIC") ) 
                nProjType = 0;
        }
        else if( EQUAL(papszTokens[0],"COORDINATE_ZONE") )
        {
            nZone = atoi(papszTokens[1]);
        }
        else if( EQUAL(papszTokens[0],"HORIZONTAL_UNITS") )
        {
	    if( EQUAL(papszTokens[1],"METERS") )
                pszUnits = "UNIT[\"metre\",1]";
	    else if( EQUAL(papszTokens[1],"FEET") )
                pszUnits = "UNIT[\"US survey foot\",0.304800609601219]";
        }
        else if( EQUAL(papszTokens[0],"HORIZONTAL_DATUM") )
        {
	    if( EQUAL(papszTokens[1],"NAD27") ) 
            {
		pszDatumLong = NAD27_DATUM;
		pszDatumShort = "NAD 27";
            }
	    else if( EQUAL(papszTokens[1],"WGS72") ) 
            {
		pszDatumLong = WGS72_DATUM;
		pszDatumShort = "WGS 72";
            }
	    else if( EQUAL(papszTokens[1],"WGS84") ) 
            {
		pszDatumLong = WGS84_DATUM;
		pszDatumShort = "WGS 84";
            }
	    else if( EQUAL(papszTokens[1],"NAD83") ) 
            {
		pszDatumLong = NAD83_DATUM;
		pszDatumShort = "NAD 83";
            }
	    else
            {
		pszDatumLong = "DATUM[\"unknown\"]";
		pszDatumShort = "unknown";
            }
        }    
        else
        {
            /* we want to generically capture all the other metadata */
            CPLString osMetaDataValue;
            int  iToken;

            for( iToken = 1; papszTokens[iToken] != NULL; iToken++ )
            {
                if( EQUAL(papszTokens[iToken],"*") )
                    continue;

                if( iToken > 1 )
                    osMetaDataValue += " " ;
                osMetaDataValue += papszTokens[iToken];
            }
            papszMetadata = CSLAddNameValue( papszMetadata, 
                                             papszTokens[0], 
                                             osMetaDataValue );
        }
	
        CSLDestroy( papszTokens );
    }

    CPLReadLineL( NULL );

/* -------------------------------------------------------------------- */
/*      Do these values look coherent for a DOQ file?  It would be      */
/*      nice to do a more comprehensive test than this!                 */
/* -------------------------------------------------------------------- */
    if( nWidth < 500 || nWidth > 25000
        || nHeight < 500 || nHeight > 25000
        || nBandStorage < 0 || nBandStorage > 4
        || nBandTypes < 1 || nBandTypes > 9 )
    {
        CSLDestroy( papszMetadata );
        VSIFCloseL(fp);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Check the configuration.  We don't currently handle all         */
/*      variations, only the common ones.                               */
/* -------------------------------------------------------------------- */
    if( nBandTypes > 5 )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "DOQ Data Type (%d) is not a supported configuration.\n",
                  nBandTypes );
        CSLDestroy( papszMetadata );
        VSIFCloseL(fp);
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CSLDestroy( papszMetadata );
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The DOQ2 driver does not support update access to existing"
                  " datasets.\n" );
        VSIFCloseL(fp);
        return NULL;
    }
/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    DOQ2Dataset 	*poDS;

    poDS = new DOQ2Dataset();

    poDS->nRasterXSize = nWidth;
    poDS->nRasterYSize = nHeight;

    poDS->SetMetadata( papszMetadata );
    CSLDestroy( papszMetadata );

    poDS->fpImage = fp;

/* -------------------------------------------------------------------- */
/*      Compute layout of data.                                         */
/* -------------------------------------------------------------------- */
    if( nBandCount < 2 )
        nBandCount = nBytesPerPixel;
    else
        nBytesPerPixel *= nBandCount;

    nBytesPerLine = nBytesPerPixel * nWidth;
    
/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nBandCount; i++ )
    {
        poDS->SetBand( i+1, 
            new RawRasterBand( poDS, i+1, poDS->fpImage,
                               nSkipBytes + i, nBytesPerPixel, nBytesPerLine,
                               GDT_Byte, TRUE, TRUE ) );
    }

    if (nProjType == 1)
    {
	poDS->pszProjection = 
            CPLStrdup(CPLSPrintf( UTM_FORMAT, pszDatumShort, nZone, 
                                  pszDatumLong, nZone * 6 - 183, pszUnits ));
    } 
    else 
    {
	poDS->pszProjection = CPLStrdup("");
    }

    poDS->dfULX = dfULXMap;
    poDS->dfULY = dfULYMap;

    poDS->dfXPixelSize = dfXDim;
    poDS->dfYPixelSize = dfYDim;

    if ( pszQuadname ) CPLFree( pszQuadname );
    if ( pszQuadquad) CPLFree( pszQuadquad );
    if ( pszState) CPLFree( pszState );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                         GDALRegister_DOQ1()                          */
/************************************************************************/

void GDALRegister_DOQ2()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "DOQ2" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "DOQ2" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "USGS DOQ (New Style)" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_various.html#DOQ2" );
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = DOQ2Dataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

