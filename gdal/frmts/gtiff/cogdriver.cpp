/******************************************************************************
 *
 * Project:  COG Driver
 * Purpose:  Cloud optimized GeoTIFF write support.
 * Author:   Even Rouault <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2019, Even Rouault <even dot rouault at spatialys dot com>
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

#include "gdal_priv.h"
#include "gtiff.h"
#include "gt_overview.h"
#include "gdal_utils.h"

#include <memory>
#include <vector>

extern "C" CPL_DLL void GDALRegister_COG();

/************************************************************************/
/*                        HasZSTDCompression()                          */
/************************************************************************/

static bool HasZSTDCompression()
{
    TIFFCodec *codecs = TIFFGetConfiguredCODECs();
    bool bHasZSTD = false;
    for( TIFFCodec *c = codecs; c->name; ++c )
    {
        if( c->scheme == COMPRESSION_ZSTD )
        {
            bHasZSTD = true;
            break;
        }
    }
    _TIFFfree( codecs );
    return bHasZSTD;
}

/************************************************************************/
/*                           GetTmpFilename()                           */
/************************************************************************/

static CPLString GetTmpFilename(GDALDataset *poSrcDS,
                                const char* pszExt)
{
    CPLString osTmpOverviewFilename;
    // Check if we can create a temporary file close to the source
    // dataset
    VSIStatBufL sStatBuf;
    if( VSIStatL(poSrcDS->GetDescription(), &sStatBuf) == 0 )
    {
        osTmpOverviewFilename.Printf("%s.%s", poSrcDS->GetDescription(), pszExt);
        VSILFILE* fp = VSIFOpenL(osTmpOverviewFilename, "wb");
        if( fp == nullptr )
            osTmpOverviewFilename.clear();
        else
            VSIFCloseL(fp);
    }
    if( osTmpOverviewFilename.empty() )
    {
        osTmpOverviewFilename = CPLGenerateTempFilename(
            CPLGetBasename(poSrcDS->GetDescription()));
        osTmpOverviewFilename += '.';
        osTmpOverviewFilename += pszExt;
    }
    return osTmpOverviewFilename;
}

/************************************************************************/
/*                            COGCreateCopy()                           */
/************************************************************************/

static GDALDataset* COGCreateCopy( const char * pszFilename,
                                   GDALDataset *poSrcDS,
                                   int /*bStrict*/, char ** papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void * pProgressData )
{
    if( pfnProgress == nullptr )
        pfnProgress = GDALDummyProgress;

    const char* pszCompress = CSLFetchNameValueDef(papszOptions, "COMPRESS", "NONE");
    std::unique_ptr<GDALDataset> poTmpDS;
    if( EQUAL(pszCompress, "JPEG") &&
        poSrcDS->GetRasterCount() == 4 )
    {
        char** papszArg = nullptr;
        papszArg = CSLAddString(papszArg, "-of");
        papszArg = CSLAddString(papszArg, "VRT");
        papszArg = CSLAddString(papszArg, "-b");
        papszArg = CSLAddString(papszArg, "1");
        papszArg = CSLAddString(papszArg, "-b");
        papszArg = CSLAddString(papszArg, "2");
        papszArg = CSLAddString(papszArg, "-b");
        papszArg = CSLAddString(papszArg, "3");
        papszArg = CSLAddString(papszArg, "-mask");
        papszArg = CSLAddString(papszArg, "4");
        GDALTranslateOptions* psOptions = GDALTranslateOptionsNew(papszArg, nullptr);
        CSLDestroy(papszArg);
        GDALDatasetH hRGBMaskDS = GDALTranslate("",
                                                GDALDataset::ToHandle(poSrcDS),
                                                psOptions,
                                                nullptr);
        GDALTranslateOptionsFree(psOptions);
        if( !hRGBMaskDS )
            return nullptr;
        poTmpDS.reset( GDALDataset::FromHandle(hRGBMaskDS) );
        poSrcDS = poTmpDS.get();
    }

    const int nBands = poSrcDS->GetRasterCount();
    if( nBands == 0 )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "COG driver does not support 0-band source raster");
        return nullptr;
    }
    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    CPLString osTmpOverviewFilename;
    CPLString osTmpMskOverviewFilename;
    const int nOvrThresholdSize = 512;

    const auto poFirstBand = poSrcDS->GetRasterBand(1);
    const bool bHasMask = poFirstBand->GetMaskFlags() == GMF_PER_DATASET;

    const char* pszOverviews = CSLFetchNameValueDef(
        papszOptions, "OVERVIEWS", "AUTO");
    const bool bGenerateMskOvr = 
        !EQUAL(pszOverviews, "FORCE_USE_EXISTING") &&
        bHasMask &&
        (nXSize > nOvrThresholdSize || nYSize > nOvrThresholdSize) &&
        (EQUAL(pszOverviews, "IGNORE_EXISTING") ||
         poFirstBand->GetMaskBand()->GetOverviewCount() == 0);
    const bool bGenerateOvr =
        !EQUAL(pszOverviews, "FORCE_USE_EXISTING") &&
        (nXSize > nOvrThresholdSize || nYSize > nOvrThresholdSize) &&
        (EQUAL(pszOverviews, "IGNORE_EXISTING") ||
         poFirstBand->GetOverviewCount() == 0);

    std::vector<int> anOverviewLevels;
    int nTmpXSize = nXSize;
    int nTmpYSize = nYSize;
    int nOvrFactor = 2;
    while( nTmpXSize > nOvrThresholdSize || nTmpYSize > nOvrThresholdSize )
    {
        anOverviewLevels.push_back(nOvrFactor);
        nOvrFactor *= 2;
        nTmpXSize /= 2;
        nTmpYSize /= 2;
    }

    double dfTotalPixelsProcessed = 
        (bGenerateMskOvr ? double(nXSize) * nYSize / 3 : 0) +
        (bGenerateOvr ? double(nXSize) * nYSize * nBands / 3: 0) +
        double(nXSize) * nYSize * (nBands + (bHasMask ? 1 : 0)) * 4. / 3;
    double dfCurPixels = 0;

    CPLStringList aosOverviewOptions;
    aosOverviewOptions.SetNameValue("COMPRESS",
                        HasZSTDCompression() ? "ZSTD" : "LZW");
    aosOverviewOptions.SetNameValue("NUM_THREADS",
                        CSLFetchNameValue(papszOptions, "NUM_THREADS"));

    if( bGenerateMskOvr )
    {
        CPLDebug("COG", "Generating overviews of the mask");
        osTmpMskOverviewFilename = GetTmpFilename(poSrcDS, "msk.ovr.tmp");
        GDALRasterBand* poSrcMask = poFirstBand->GetMaskBand();

        double dfNextPixels = dfCurPixels + double(nXSize) * nYSize / 3;
        void* pScaledProgress = GDALCreateScaledProgress(
                dfCurPixels / dfTotalPixelsProcessed,
                dfNextPixels / dfTotalPixelsProcessed,
                pfnProgress, pProgressData );
        dfCurPixels = dfNextPixels;

        CPLErr eErr = GTIFFBuildOverviewsEx(
            osTmpMskOverviewFilename,
            1, &poSrcMask,
            static_cast<int>(anOverviewLevels.size()),
            &anOverviewLevels[0],
            "NEAREST",
            aosOverviewOptions.List(),
            GDALScaledProgress, pScaledProgress );

        GDALDestroyScaledProgress(pScaledProgress);
        if( eErr != CE_None )
        {
            if( !osTmpMskOverviewFilename.empty() )
            {
                VSIUnlink(osTmpMskOverviewFilename);
            }
            return nullptr;
        }
    }

    if( bGenerateOvr )
    {
        CPLDebug("COG", "Generating overviews of the imagery");
        osTmpOverviewFilename = GetTmpFilename(poSrcDS, "ovr.tmp");
        std::vector<GDALRasterBand*> apoSrcBands;
        for( int i = 0; i < nBands; i++ )
            apoSrcBands.push_back( poSrcDS->GetRasterBand(i+1) );
        const char* pszResampling = CSLFetchNameValueDef(papszOptions,
            "RESAMPLING",
            poFirstBand->GetColorTable() ? "NEAREST" : "CUBIC");

        double dfNextPixels = dfCurPixels + double(nXSize) * nYSize * nBands / 3;
        void* pScaledProgress = GDALCreateScaledProgress(
                dfCurPixels / dfTotalPixelsProcessed,
                dfNextPixels / dfTotalPixelsProcessed,
                pfnProgress, pProgressData );
        dfCurPixels = dfNextPixels;

        if( !osTmpMskOverviewFilename.empty() )
        {
            aosOverviewOptions.SetNameValue("MASK_OVERVIEW_DATASET",
                                            osTmpMskOverviewFilename);
        }
        CPLErr eErr = GTIFFBuildOverviewsEx(
            osTmpOverviewFilename,
            nBands, &apoSrcBands[0],
            static_cast<int>(anOverviewLevels.size()),
            &anOverviewLevels[0],
            pszResampling,
            aosOverviewOptions.List(),
            GDALScaledProgress, pScaledProgress );

        GDALDestroyScaledProgress(pScaledProgress);
        if( eErr != CE_None )
        {
            if( !osTmpOverviewFilename.empty() )
            {
                VSIUnlink(osTmpOverviewFilename);
            }
            if( !osTmpMskOverviewFilename.empty() )
            {
                VSIUnlink(osTmpMskOverviewFilename);
            }
            return nullptr;
        }
    }

    CPLStringList aosOptions;
    aosOptions.SetNameValue("COPY_SRC_OVERVIEWS", "YES");
    CPLString osBlockSize(CSLFetchNameValueDef(papszOptions, "BLOCKSIZE", "512"));
    aosOptions.SetNameValue("COMPRESS", pszCompress);
    aosOptions.SetNameValue("TILED", "YES");
    aosOptions.SetNameValue("BLOCKXSIZE", osBlockSize);
    aosOptions.SetNameValue("BLOCKYSIZE", osBlockSize);
    if( CPLTestBool(CSLFetchNameValueDef(papszOptions, "PREDICTOR", "FALSE")) )
        aosOptions.SetNameValue("PREDICTOR", "2");
    const char* pszQuality = CSLFetchNameValue(papszOptions, "QUALITY");
    if( EQUAL(pszCompress, "JPEG") )
    {
        aosOptions.SetNameValue("JPEG_QUALITY", pszQuality);
        if( nBands == 3 )
            aosOptions.SetNameValue("PHOTOMETRIC", "YCBCR");
    }
    else if( EQUAL(pszCompress, "WEBP") )
    {
        if( pszQuality && atoi(pszQuality) == 100 )
            aosOptions.SetNameValue("WEBP_LOSSLESS", "YES");
        aosOptions.SetNameValue("WEBP_LEVEL", pszQuality);
    }
    else if( EQUAL(pszCompress, "DEFLATE") )
    {
        aosOptions.SetNameValue("ZLEVEL",
                                CSLFetchNameValue(papszOptions, "LEVEL"));
    }
    else if( EQUAL(pszCompress, "ZSTD") )
    {
        aosOptions.SetNameValue("ZSTD_LEVEL",
                                CSLFetchNameValue(papszOptions, "LEVEL"));
    }
    aosOptions.SetNameValue("BIGTIFF",
                                CSLFetchNameValue(papszOptions, "BIGTIFF"));
    aosOptions.SetNameValue("NUM_THREADS",
                                CSLFetchNameValue(papszOptions, "NUM_THREADS"));

    if( !osTmpOverviewFilename.empty() )
    {
        aosOptions.SetNameValue("@OVERVIEW_DATASET", osTmpOverviewFilename);
    }
    if( !osTmpMskOverviewFilename.empty() )
    {
        aosOptions.SetNameValue("@MASK_OVERVIEW_DATASET", osTmpMskOverviewFilename);
    }

    GDALDriver* poGTiffDrv = GDALDriver::FromHandle(GDALGetDriverByName("GTiff"));
    if( !poGTiffDrv )
        return nullptr;
    void* pScaledProgress = GDALCreateScaledProgress(
            dfCurPixels / dfTotalPixelsProcessed,
            1.0,
            pfnProgress, pProgressData );

    CPLConfigOptionSetter oSetterInternalMask(
        "GDAL_TIFF_INTERNAL_MASK", "YES", false);

    auto poRet = poGTiffDrv->CreateCopy(pszFilename, poSrcDS, false,
                                        aosOptions.List(),
                                        GDALScaledProgress, pScaledProgress);

    GDALDestroyScaledProgress(pScaledProgress);

    if( !osTmpOverviewFilename.empty() )
    {
        VSIUnlink(osTmpOverviewFilename);
    }
    if( !osTmpMskOverviewFilename.empty() )
    {
        VSIUnlink(osTmpMskOverviewFilename);
    }

    return poRet;
}

/************************************************************************/
/*                          GDALRegister_COG()                          */
/************************************************************************/

void GDALRegister_COG()

{
    if( GDALGetDriverByName( "COG" ) != nullptr )
        return;

    bool bHasLZW = false;
    bool bHasDEFLATE = false;
    bool bHasLZMA = false;
    bool bHasZSTD = false;
    bool bHasJPEG = false;
    bool bHasWebP = false;
    CPLString osCompressValues(GTiffGetCompressValues(
        bHasLZW, bHasDEFLATE, bHasLZMA, bHasZSTD, bHasJPEG, bHasWebP,
        true /* bForCOG */));

    CPLString osOptions;
    osOptions = "<CreationOptionList>"
                "   <Option name='COMPRESS' type='string-select'>";
    osOptions += osCompressValues;
    osOptions += "   </Option>";
    if( bHasLZW || bHasDEFLATE || bHasZSTD )
    {
        osOptions += "   <Option name='LEVEL' type='int' "
            "description='DEFLATE/ZSTD compression level: 1 (fastest)'/>";
        osOptions += "   <Option name='PREDICTOR' type='boolean' default='FALSE'/>";
    }
    if( bHasJPEG || bHasWebP )
    {
        osOptions += "   <Option name='QUALITY' type='int' "
                     "description='JPEG/WEBP quality 1-100' default='75'/>";
    }
    osOptions +=
"   <Option name='NUM_THREADS' type='string' "
        "description='Number of worker threads for compression. "
        "Can be set to ALL_CPUS' default='1'/>"
"   <Option name='BLOCKSIZE' type='int' "
        "description='Tile size in pixels' min='128' default='512'/>"
"   <Option name='BIGTIFF' type='string-select' description='Force creation of BigTIFF file'>"
"     <Value>YES</Value>"
"     <Value>NO</Value>"
"     <Value>IF_NEEDED</Value>"
"     <Value>IF_SAFER</Value>"
"   </Option>"
"   <Option name='RESAMPLING' type='string' "
        "description='Resampling method for overviews'/>"
"   <Option name='OVERVIEWS' type='string-select' description='Behaviour regarding overviews'>"
"     <Value>AUTO</Value>"
"     <Value>IGNORE_EXISTING</Value>"
"     <Value>FORCE_USE_EXISTING</Value>"
"   </Option>"
"</CreationOptionList>";

    auto poDriver = new GDALDriver();
    poDriver->SetDescription( "COG" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "Cloud optimized GeoTIFF generator" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "cog.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, osOptions );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte UInt16 Int16 UInt32 Int32 Float32 "
                               "Float64 CInt16 CInt32 CFloat32 CFloat64" );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnCreateCopy = COGCreateCopy;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
