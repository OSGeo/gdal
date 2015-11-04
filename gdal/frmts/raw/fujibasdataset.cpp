/******************************************************************************
 * $Id$
 *
 * Project:  eCognition
 * Purpose:  Implementation of FUJI BAS Format
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
 * Copyright (c) 2009, Even Rouault <even dot rouault at mines-paris dot org>
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
void	GDALRegister_FujiBAS(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				FujiBASDataset				*/
/* ==================================================================== */
/************************************************************************/

class FujiBASDataset : public RawDataset
{
    FILE	*fpImage;	// image data file.

    char	**papszHeader;

  public:
    		FujiBASDataset();
    	        ~FujiBASDataset();

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            FujiBASDataset()                          */
/************************************************************************/

FujiBASDataset::FujiBASDataset() :
    fpImage(NULL),
    papszHeader(NULL)
{}

/************************************************************************/
/*                            ~FujiBASDataset()                            */
/************************************************************************/

FujiBASDataset::~FujiBASDataset()

{
    FlushCache();
    if( fpImage != NULL )
        VSIFClose( fpImage );
    CSLDestroy( papszHeader );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *FujiBASDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      We assume the user is pointing to the header (.pcb) file.       */
/*      Does this appear to be a pcb file?                              */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 80 || poOpenInfo->fpL == NULL )
        return NULL;

    if( !STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "[Raw data]")
        || strstr((const char *)poOpenInfo->pabyHeader, "Fuji BAS") == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*	Load the header file.						*/
/* -------------------------------------------------------------------- */
    char **papszHeader = CSLLoad( poOpenInfo->pszFilename );

    if( papszHeader == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Munge header information into form suitable for CSL functions.  */
/* -------------------------------------------------------------------- */
    for( int i = 0; papszHeader[i] != NULL; i++ )
    {
        char	*pszSep = strstr(papszHeader[i]," = ");

        if( pszSep != NULL )
        {
            memmove( pszSep + 1, pszSep + 3, strlen(pszSep+3)+1 );
            *pszSep = '=';
        }
    }

/* -------------------------------------------------------------------- */
/*      Fetch required fields.                                          */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue(papszHeader, "width") == NULL
        || CSLFetchNameValue(papszHeader, "height") == NULL
        || CSLFetchNameValue(papszHeader, "OrgFile") == NULL )
    {
        CSLDestroy( papszHeader );
        return NULL;
    }

    const int nYSize = atoi(CSLFetchNameValue(papszHeader,"width"));
    const int nXSize = atoi(CSLFetchNameValue(papszHeader,"height"));

    const char *pszOrgFile = CSLFetchNameValue(papszHeader,"OrgFile");

    if( nXSize < 1 || nYSize < 1 )
    {
        CSLDestroy( papszHeader );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The FUJIBAS driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to open the original data file.                             */
/* -------------------------------------------------------------------- */
    char       *pszPath = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
    const char *pszRawFile = CPLFormCIFilename( pszPath, pszOrgFile, "IMG" );
    CPLFree( pszPath );

    FILE *fpRaw = VSIFOpen( pszRawFile, "rb" );
    if( fpRaw == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Trying to open Fuji BAS image with the header file:\n"
                  "  Header=%s\n"
                  "but expected raw image file doesn't appear to exist.  Trying to open:\n"
                  "  Raw File=%s\n"
                  "Perhaps the raw file needs to be renamed to match expected?",
                  poOpenInfo->pszFilename, 
                  pszRawFile );
        CSLDestroy( papszHeader );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    FujiBASDataset *poDS = new FujiBASDataset();

/* -------------------------------------------------------------------- */
/*      Capture some information from the file that is of interest.     */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->papszHeader = papszHeader;
    poDS->fpImage = fpRaw;

/* -------------------------------------------------------------------- */
/*      Create band information object.                                 */
/* -------------------------------------------------------------------- */
    const int bNativeOrder =
#ifdef CPL_MSB
    TRUE
#else
    FALSE
#endif
        ;
    poDS->SetBand( 1, 
                   new RawRasterBand( poDS, 1, poDS->fpImage, 
                                      0, 2, nXSize * 2, GDT_UInt16, bNativeOrder ));

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
/*                         GDALRegister_FujiBAS()                       */
/************************************************************************/

void GDALRegister_FujiBAS()

{
    if( GDALGetDriverByName( "FujiBAS" ) != NULL )
        return;

    GDALDriver	*poDriver = new GDALDriver();

    poDriver->SetDescription( "FujiBAS" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Fuji BAS Scanner Image" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "frmt_various.html#FujiBAS" );

    poDriver->pfnOpen = FujiBASDataset::Open;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
