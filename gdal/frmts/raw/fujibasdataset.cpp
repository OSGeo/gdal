/******************************************************************************
 *
 * Project:  eCognition
 * Purpose:  Implementation of FUJI BAS Format
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam
 * Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
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
#include "gdal_frmts.h"
#include "rawdataset.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                              FujiBASDataset                          */
/* ==================================================================== */
/************************************************************************/

class FujiBASDataset final: public RawDataset
{
    VSILFILE        *fpImage;  // image data file.
    CPLString        osRawFilename{};

    char        **papszHeader;

    CPL_DISALLOW_COPY_ASSIGN(FujiBASDataset)

  public:
                FujiBASDataset();
                ~FujiBASDataset();

    char **GetFileList() override;

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            FujiBASDataset()                          */
/************************************************************************/

FujiBASDataset::FujiBASDataset() :
    fpImage(nullptr),
    papszHeader(nullptr)
{}

/************************************************************************/
/*                            ~FujiBASDataset()                         */
/************************************************************************/

FujiBASDataset::~FujiBASDataset()

{
    FlushCache(true);
    if( fpImage != nullptr )
        VSIFCloseL( fpImage );
    CSLDestroy( papszHeader );
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char **FujiBASDataset::GetFileList()

{
    char **papszFileList = RawDataset::GetFileList();
    papszFileList = CSLAddString( papszFileList, osRawFilename );
    return papszFileList;
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
    if( poOpenInfo->nHeaderBytes < 80 || poOpenInfo->fpL == nullptr )
        return nullptr;

    if( !STARTS_WITH_CI( reinterpret_cast<char *>(poOpenInfo->pabyHeader),
                         "[Raw data]")
        || strstr(reinterpret_cast<char *>(poOpenInfo->pabyHeader), "Fuji BAS")
        == nullptr )
        return nullptr;

    if( !GDALIsDriverDeprecatedForGDAL35StillEnabled("FUJIBAS") )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Load the header file.                                           */
/* -------------------------------------------------------------------- */
    char **papszHeader = CSLLoad( poOpenInfo->pszFilename );

    if( papszHeader == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Munge header information into form suitable for CSL functions.  */
/* -------------------------------------------------------------------- */
    for( int i = 0; papszHeader[i] != nullptr; i++ )
    {
        char *pszSep = strstr(papszHeader[i]," = ");

        if( pszSep != nullptr )
        {
            memmove( pszSep + 1, pszSep + 3, strlen(pszSep+3)+1 );
            *pszSep = '=';
        }
    }

/* -------------------------------------------------------------------- */
/*      Fetch required fields.                                          */
/* -------------------------------------------------------------------- */
    if( CSLFetchNameValue(papszHeader, "width") == nullptr
        || CSLFetchNameValue(papszHeader, "height") == nullptr
        || CSLFetchNameValue(papszHeader, "OrgFile") == nullptr )
    {
        CSLDestroy( papszHeader );
        return nullptr;
    }

    const int nYSize = atoi(CSLFetchNameValue(papszHeader,"width"));
    const int nXSize = atoi(CSLFetchNameValue(papszHeader,"height"));

    const char *pszOrgFile = CSLFetchNameValue(papszHeader,"OrgFile");

    if( nXSize < 1 || nYSize < 1 )
    {
        CSLDestroy( papszHeader );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The FUJIBAS driver does not support update access to "
                  " existing datasets." );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Try to open the original data file.                             */
/* -------------------------------------------------------------------- */
    char *pszPath = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
    const char *pszRawFile = CPLFormCIFilename( pszPath, pszOrgFile, "IMG" );
    CPLFree( pszPath );

    VSILFILE *fpRaw = VSIFOpenL( pszRawFile, "rb" );
    if( fpRaw == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Trying to open Fuji BAS image with the header file:\n"
                  "  Header=%s\n"
                  "but expected raw image file doesn't appear to exist.  "
                  "Trying to open:\n"
                  "  Raw File=%s\n"
                  "Perhaps the raw file needs to be renamed to match expected?",
                  poOpenInfo->pszFilename,
                  pszRawFile );
        CSLDestroy( papszHeader );
        return nullptr;
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
    poDS->osRawFilename = pszRawFile;
    poDS->papszHeader = papszHeader;
    poDS->fpImage = fpRaw;

/* -------------------------------------------------------------------- */
/*      Create band information object.                                 */
/* -------------------------------------------------------------------- */
    const bool bNativeOrder =
#ifdef CPL_MSB
    true
#else
    false
#endif
        ;
    poDS->SetBand(
        1,
        new RawRasterBand( poDS, 1, poDS->fpImage,
                           0, 2, nXSize * 2, GDT_UInt16, bNativeOrder,
                           RawRasterBand::OwnFP::NO ) );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                         GDALRegister_FujiBAS()                       */
/************************************************************************/

void GDALRegister_FujiBAS()

{
    if( GDALGetDriverByName( "FujiBAS" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "FujiBAS" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Fuji BAS Scanner Image" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                               "drivers/raster/fujibas.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = FujiBASDataset::Open;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
