/******************************************************************************
 *
 * Project:  EarthWatch .TIL Driver
 * Purpose:  Implementation of the TILDataset class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam
 * Copyright (c) 2009-2011, Even Rouault <even dot rouault at spatialys.com>
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

#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cplkeywordparser.h"
#include "gdal_mdreader.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"
#include "ogr_spatialref.h"
#include "vrtdataset.h"

CPL_CVSID("$Id$")

/************************************************************************/
/* ==================================================================== */
/*                              TILDataset                              */
/* ==================================================================== */
/************************************************************************/

class TILDataset final : public GDALPamDataset
{
    VRTDataset *poVRTDS;
    std::vector<std::string> m_aosFilenames;

    char **papszMetadataFiles;

  protected:
    virtual int         CloseDependentDatasets() override;

  public:
    TILDataset();
    virtual ~TILDataset();

    virtual char **GetFileList(void) override;

    static GDALDataset *Open( GDALOpenInfo * );
    static int Identify( GDALOpenInfo *poOpenInfo );
};

/************************************************************************/
/* ==================================================================== */
/*                            TILRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class TILRasterBand final: public GDALPamRasterBand
{
    friend class TILDataset;

    VRTSourcedRasterBand *poVRTBand;

  public:
                   TILRasterBand( TILDataset *, int, VRTSourcedRasterBand * );
    virtual       ~TILRasterBand() {}

    virtual CPLErr IReadBlock( int, int, void * ) override;
    virtual CPLErr IRasterIO( GDALRWFlag, int, int, int, int,
                              void *, int, int, GDALDataType,
                              GSpacing nPixelSpace, GSpacing nLineSpace,
                              GDALRasterIOExtraArg* psExtraArg ) override;
};

/************************************************************************/
/*                           TILRasterBand()                            */
/************************************************************************/

TILRasterBand::TILRasterBand( TILDataset *poTILDS, int nBandIn,
                              VRTSourcedRasterBand *poVRTBandIn )

{
    poDS = poTILDS;
    poVRTBand = poVRTBandIn;
    nBand = nBandIn;
    eDataType = poVRTBandIn->GetRasterDataType();

    poVRTBandIn->GetBlockSize( &nBlockXSize, &nBlockYSize );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr TILRasterBand::IReadBlock( int iBlockX, int iBlockY, void *pBuffer )

{
    return poVRTBand->ReadBlock( iBlockX, iBlockY, pBuffer );
}

/************************************************************************/
/*                             IRasterIO()                              */
/************************************************************************/

CPLErr TILRasterBand::IRasterIO( GDALRWFlag eRWFlag,
                                 int nXOff, int nYOff, int nXSize, int nYSize,
                                 void * pData, int nBufXSize, int nBufYSize,
                                 GDALDataType eBufType,
                                 GSpacing nPixelSpace, GSpacing nLineSpace,
                                 GDALRasterIOExtraArg* psExtraArg )

{
    if(GetOverviewCount() > 0)
    {
        return GDALPamRasterBand::IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                 pData, nBufXSize, nBufYSize, eBufType,
                                 nPixelSpace, nLineSpace, psExtraArg );
    }

    // If not exist TIL overviews, try to use band source overviews.
    return poVRTBand->IRasterIO( eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                 pData, nBufXSize, nBufYSize, eBufType,
                                 nPixelSpace, nLineSpace, psExtraArg );
}

/************************************************************************/
/* ==================================================================== */
/*                             TILDataset                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             TILDataset()                             */
/************************************************************************/

TILDataset::TILDataset() :
    poVRTDS(nullptr),
    papszMetadataFiles(nullptr)
{}

/************************************************************************/
/*                            ~TILDataset()                             */
/************************************************************************/

TILDataset::~TILDataset()

{
    TILDataset::CloseDependentDatasets();
    CSLDestroy(papszMetadataFiles);
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int TILDataset::CloseDependentDatasets()
{
    int bHasDroppedRef = GDALPamDataset::CloseDependentDatasets();

    if( poVRTDS )
    {
        bHasDroppedRef = TRUE;
        delete poVRTDS;
        poVRTDS = nullptr;
    }

    return bHasDroppedRef;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int TILDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes < 200
        || !EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"TIL") )
        return FALSE;

    if( strstr((const char *) poOpenInfo->pabyHeader,"numTiles") == nullptr )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *TILDataset::Open( GDALOpenInfo * poOpenInfo )

{
    if( !Identify( poOpenInfo ) || poOpenInfo->fpL == nullptr )
        return nullptr;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The TIL driver does not support update access to existing"
                  " datasets.\n" );
        return nullptr;
    }

    CPLString osDirname = CPLGetDirname(poOpenInfo->pszFilename);

// get metadata reader

    GDALMDReaderManager mdreadermanager;
    GDALMDReaderBase* mdreader = mdreadermanager.GetReader(poOpenInfo->pszFilename,
                                         poOpenInfo->GetSiblingFiles(), MDR_DG);

    if(nullptr == mdreader)
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to open .TIL dataset due to missing metadata file." );
        return nullptr;
    }
/* -------------------------------------------------------------------- */
/*      Try to find the corresponding .IMD file.                        */
/* -------------------------------------------------------------------- */
    char **papszIMD = mdreader->GetMetadataDomain(MD_DOMAIN_IMD);

    if( papszIMD == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Unable to open .TIL dataset due to missing .IMD file." );
        return nullptr;
    }

    if( CSLFetchNameValue( papszIMD, "numRows" ) == nullptr
        || CSLFetchNameValue( papszIMD, "numColumns" ) == nullptr
        || CSLFetchNameValue( papszIMD, "bitsPerPixel" ) == nullptr )
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Missing a required field in the .IMD file." );
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Try to load and parse the .TIL file.                            */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = poOpenInfo->fpL;
    poOpenInfo->fpL = nullptr;

    CPLKeywordParser oParser;

    if( !oParser.Ingest( fp ) )
    {
        VSIFCloseL( fp );
        return nullptr;
    }

    VSIFCloseL( fp );

    char **papszTIL = oParser.GetAllKeywords();

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/* -------------------------------------------------------------------- */
    TILDataset *poDS = new TILDataset();
    poDS->papszMetadataFiles = mdreader->GetMetadataFiles();
    mdreader->FillMetadata(&poDS->oMDMD);
    poDS->nRasterXSize = atoi(CSLFetchNameValueDef(papszIMD,"numColumns","0"));
    poDS->nRasterYSize = atoi(CSLFetchNameValueDef(papszIMD,"numRows","0"));
    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      We need to open one of the images in order to establish         */
/*      details like the band count and types.                          */
/* -------------------------------------------------------------------- */
    const char *pszFilename = CSLFetchNameValue( papszTIL, "TILE_1.filename" );
    if( pszFilename == nullptr )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Missing TILE_1.filename in .TIL file." );
        delete poDS;
        return nullptr;
    }

    // trim double quotes.
    if( pszFilename[0] == '"' )
        pszFilename++;
    if( pszFilename[strlen(pszFilename)-1] == '"' )
        const_cast<char *>( pszFilename )[strlen(pszFilename)-1] = '\0';

    CPLString osFilename = CPLFormFilename(osDirname, pszFilename, nullptr);
    GDALDataset *poTemplateDS = reinterpret_cast<GDALDataset *>(
        GDALOpen( osFilename, GA_ReadOnly ) );
    if( poTemplateDS == nullptr || poTemplateDS->GetRasterCount() == 0)
    {
        delete poDS;
        if (poTemplateDS != nullptr)
            GDALClose( poTemplateDS );
        return nullptr;
    }

    GDALRasterBand *poTemplateBand = poTemplateDS->GetRasterBand(1);
    const GDALDataType eDT = poTemplateBand->GetRasterDataType();
    const int nBandCount = poTemplateDS->GetRasterCount();

    //we suppose the first tile have the same projection as others (usually so)
    CPLString pszProjection(poTemplateDS->GetProjectionRef());
    if(!pszProjection.empty())
        poDS->SetProjection(pszProjection);

    //we suppose the first tile have the same GeoTransform as others (usually so)
    double      adfGeoTransform[6];
    if( poTemplateDS->GetGeoTransform( adfGeoTransform ) == CE_None )
    {
        // According to https://www.digitalglobe.com/sites/default/files/ISD_External.pdf, ulx=originX and
        // is "Easting of the center of the upper left pixel of the image."
        adfGeoTransform[0] = CPLAtof(CSLFetchNameValueDef(papszIMD,"MAP_PROJECTED_PRODUCT.ULX","0")) - adfGeoTransform[1] / 2;
        adfGeoTransform[3] = CPLAtof(CSLFetchNameValueDef(papszIMD,"MAP_PROJECTED_PRODUCT.ULY","0")) - adfGeoTransform[5] / 2;
        poDS->SetGeoTransform(adfGeoTransform);
    }

    poTemplateBand = nullptr;
    GDALClose( poTemplateDS );

/* -------------------------------------------------------------------- */
/*      Create and initialize the corresponding VRT dataset used to     */
/*      manage the tiled data access.                                   */
/* -------------------------------------------------------------------- */
    poDS->poVRTDS = new VRTDataset(poDS->nRasterXSize,poDS->nRasterYSize);

    for( int iBand = 0; iBand < nBandCount; iBand++ )
        poDS->poVRTDS->AddBand( eDT, nullptr );

    /* Don't try to write a VRT file */
    poDS->poVRTDS->SetWritable(FALSE);

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    for( int iBand = 1; iBand <= nBandCount; iBand++ )
        poDS->SetBand(
            iBand,
            new TILRasterBand(
                poDS,
                iBand,
                reinterpret_cast<VRTSourcedRasterBand *>(
                    poDS->poVRTDS->GetRasterBand(iBand) ) ) );

/* -------------------------------------------------------------------- */
/*      Add tiles as sources for each band.                             */
/* -------------------------------------------------------------------- */
    const int nTileCount = atoi(CSLFetchNameValueDef(papszTIL,"numTiles","0"));
    int iTile = 0;

    for( iTile = 1; iTile <= nTileCount; iTile++ )
    {
        CPLString osKey;
        osKey.Printf( "TILE_%d.filename", iTile );
        pszFilename = CSLFetchNameValue( papszTIL, osKey );
        if( pszFilename == nullptr )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Missing TILE_%d.filename in .TIL file.", iTile );
            delete poDS;
            return nullptr;
        }

        // trim double quotes.
        if( pszFilename[0] == '"' )
            pszFilename++;
        if( pszFilename[strlen(pszFilename)-1] == '"' )
            const_cast<char *>( pszFilename )[strlen(pszFilename)-1] = '\0';
        osFilename = CPLFormFilename(osDirname, pszFilename, nullptr);
        poDS->m_aosFilenames.push_back(osFilename);

        osKey.Printf( "TILE_%d.ULColOffset", iTile );
        const int nULX = atoi(CSLFetchNameValueDef(papszTIL, osKey, "0"));

        osKey.Printf( "TILE_%d.ULRowOffset", iTile );
        const int nULY = atoi(CSLFetchNameValueDef(papszTIL, osKey, "0"));

        osKey.Printf( "TILE_%d.LRColOffset", iTile );
        const int nLRX = atoi(CSLFetchNameValueDef(papszTIL, osKey, "0"));

        osKey.Printf( "TILE_%d.LRRowOffset", iTile );
        const int nLRY = atoi(CSLFetchNameValueDef(papszTIL, osKey, "0"));

        for( int iBand = 1; iBand <= nBandCount; iBand++ )
        {
            VRTSourcedRasterBand *poVRTBand =
                reinterpret_cast<VRTSourcedRasterBand *>(
                    poDS->poVRTDS->GetRasterBand(iBand) );

            poVRTBand->AddSimpleSource( osFilename, iBand,
                                        0, 0,
                                        nLRX - nULX + 1, nLRY - nULY + 1,
                                        nULX, nULY,
                                        nLRX - nULX + 1, nLRY - nULY + 1 );
        }
    }

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
/*                            GetFileList()                             */
/************************************************************************/

char **TILDataset::GetFileList()

{
    char **papszFileList = GDALPamDataset::GetFileList();

    for( const auto& osFilename: m_aosFilenames )
        papszFileList = CSLAddString( papszFileList, osFilename.c_str());

    if(nullptr != papszMetadataFiles)
    {
        for( int i = 0; papszMetadataFiles[i] != nullptr; i++ )
        {
            papszFileList = CSLAddString( papszFileList, papszMetadataFiles[i] );
        }
    }

    return papszFileList;
}

/************************************************************************/
/*                          GDALRegister_TIL()                          */
/************************************************************************/

void GDALRegister_TIL()

{
    if( GDALGetDriverByName( "TIL" ) != nullptr )
        return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "TIL" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "EarthWatch .TIL" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/til.html" );
    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = TILDataset::Open;
    poDriver->pfnIdentify = TILDataset::Identify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
