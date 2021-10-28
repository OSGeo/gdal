/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Test block cache & writing behaviour under multi-threading
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault <even dot rouault at spatialys dot com>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "gdal_alg.h"
#include "gdal_priv.h"
#include "gdal.h"

#include <cassert>

#include "test_data.h"

static void OpenJPEG2000(const char* pszFilename)
{
    const int N_DRIVERS = 6;
    const char* const apszDrivers[] = {"JP2ECW", "JP2OpenJPEG", "JPEG2000" , "JP2MrSID", "JP2KAK", "JP2Lura" };
    GDALDriverH aphDrivers[ N_DRIVERS ];
    GDALDatasetH hDS;
    int i, j;

    for(i=0;i<N_DRIVERS;i++)
        aphDrivers[i] = GDALGetDriverByName(apszDrivers[i]);

    for(i=0;i<N_DRIVERS;i++)
    {
        if( aphDrivers[i] == nullptr )
            continue;
        for(j=0;j<N_DRIVERS;j++)
        {
            if( i == j || aphDrivers[j] == nullptr )
                continue;
            GDALDeregisterDriver(aphDrivers[j]);
        }

        hDS = GDALOpen(pszFilename, GA_ReadOnly);
        if( !EQUAL(apszDrivers[i], "JP2Lura") && !EQUAL(apszDrivers[i], "JPEG2000") )
        {
            assert( hDS != nullptr );
        }
        for(j=0;j<N_DRIVERS;j++)
        {
            if( i == j || aphDrivers[j] == nullptr )
                continue;
            GDALRegisterDriver(aphDrivers[j]);
        }
    }
}

int main(int /* argc*/ , char* /* argv */[])
{
    int nOvrLevel;
    int nBandNum;
    GDALDatasetH hDS;
    GDALDatasetH hSrcDS;
    FILE* f;

    const char* pszGDAL_SKIP = CPLGetConfigOption("GDAL_SKIP", nullptr);
    if( pszGDAL_SKIP == nullptr )
        CPLSetConfigOption("GDAL_SKIP", "GIF");
    else
        CPLSetConfigOption("GDAL_SKIP", CPLSPrintf("%s GIF", pszGDAL_SKIP));

    GDALAllRegister();

    hDS = GDALOpen(GCORE_DATA_DIR "byte.tif", GA_ReadOnly);
    if (hDS)
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));

    hDS = GDALOpen(GCORE_DATA_DIR "byte.vrt", GA_ReadOnly);
    if (hDS)
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));

    hDS = GDALOpen(GDRIVERS_DIR "data/vrt/rgb_warp.vrt", GA_ReadOnly);
    if (hDS)
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));

    hDS = GDALOpen(GDRIVERS_DIR "data/nitf/A.TOC", GA_ReadOnly);

    hDS = GDALOpen("NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:" GDRIVERS_DIR "data/nitf/A.TOC", GA_ReadOnly);
    if (hDS)
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));

    hDS = GDALOpen(GDRIVERS_DIR "data/til/testtil.til", GA_ReadOnly);
    if (hDS)
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));

    hDS = GDALOpen(GDRIVERS_DIR "data/rs2/product.xml", GA_ReadOnly);
    if (hDS)
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));

    hDS = GDALOpen(GDRIVERS_DIR "data/dimap/METADATA.DIM", GA_ReadOnly);
    if (hDS)
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));

    hDS = GDALOpen(GDRIVERS_DIR "tmp/cache/file9_j2c.ntf", GA_ReadOnly);
    if (hDS)
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));

    hDS = GDALOpen(GDRIVERS_DIR "data/gif/bug407.gif", GA_ReadOnly);
    if (hDS)
    {
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));
        GDALSetCacheMax(0);
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));
    }

    /* Create external overviews */
    hSrcDS = GDALOpen(GCORE_DATA_DIR "byte.tif", GA_ReadOnly);
    hDS = GDALCreateCopy(GDALGetDriverByName("GTiff"), "byte.tif", hSrcDS, 0, nullptr, nullptr, nullptr);
    GDALClose(hSrcDS);
    hSrcDS = nullptr;
    hDS = GDALOpen("byte.tif", GA_ReadOnly);
    nOvrLevel = 2;
    nBandNum = 1;
    CPL_IGNORE_RET_VAL(GDALBuildOverviews( hDS, "NEAR", 1, &nOvrLevel, 1, &nBandNum, nullptr, nullptr));
    GDALClose(hDS);

    hDS = GDALOpen("byte.tif", GA_ReadOnly);
    GDALGetOverviewCount(GDALGetRasterBand(hDS, 1));

    /* Create internal overviews */
    hSrcDS = GDALOpen(GCORE_DATA_DIR "byte.tif", GA_ReadOnly);
    hDS = GDALCreateCopy(GDALGetDriverByName("GTiff"), "byte2.tif", hSrcDS, 0, nullptr, nullptr, nullptr);
    GDALClose(hSrcDS);
    hSrcDS = nullptr;
    hDS = GDALOpen("byte2.tif", GA_Update);
    nOvrLevel = 2;
    nBandNum = 1;
    CPL_IGNORE_RET_VAL(GDALBuildOverviews( hDS, "NEAR", 1, &nOvrLevel, 1, &nBandNum, nullptr, nullptr));
    GDALClose(hDS);

    hDS = GDALOpen("byte2.tif", GA_ReadOnly);
    GDALGetOverviewCount(GDALGetRasterBand(hDS, 1));

    /* Create external mask */
    hSrcDS = GDALOpen(GCORE_DATA_DIR "byte.tif", GA_ReadOnly);
    hDS = GDALCreateCopy(GDALGetDriverByName("GTiff"), "byte3.tif", hSrcDS, 0, nullptr, nullptr, nullptr);
    GDALClose(hSrcDS);
    hSrcDS = nullptr;
    hDS = GDALOpen("byte3.tif", GA_ReadOnly);
    GDALCreateDatasetMaskBand(hDS, GMF_PER_DATASET);
    GDALClose(hDS);

    hDS = GDALOpen("byte3.tif", GA_ReadOnly);
    GDALGetMaskFlags(GDALGetRasterBand(hDS, 1));

    f = fopen("byte.vrt", "wb");
    fprintf(f, "%s", "<VRTDataset rasterXSize=\"20\" rasterYSize=\"20\">"
  "<VRTRasterBand dataType=\"Byte\" band=\"1\">"
    "<SimpleSource>"
      "<SourceFilename relativeToVRT=\"1\">" GCORE_DATA_DIR "byte.tif</SourceFilename>"
      "<SourceBand>1</SourceBand>"
      "<SourceProperties RasterXSize=\"20\" RasterYSize=\"20\" DataType=\"Byte\" BlockXSize=\"20\" BlockYSize=\"20\" />"
      "<SrcRect xOff=\"0\" yOff=\"0\" xSize=\"20\" ySize=\"20\"/>"
      "<DstRect xOff=\"0\" yOff=\"0\" xSize=\"20\" ySize=\"20\"/>"
    "</SimpleSource>"
  "</VRTRasterBand>"
"</VRTDataset>");
    fclose(f);

    hDS = GDALOpen("byte.vrt", GA_ReadOnly);
    nOvrLevel = 2;
    nBandNum = 1;
    CPL_IGNORE_RET_VAL(GDALBuildOverviews( hDS, "NEAR", 1, &nOvrLevel, 1, &nBandNum, nullptr, nullptr));
    GDALClose(hDS);

    hDS = GDALOpen("byte.vrt", GA_ReadOnly);
    GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));
    GDALGetOverviewCount(GDALGetRasterBand(hDS, 1));

    hDS = GDALOpen("<VRTDataset rasterXSize=\"20\" rasterYSize=\"20\">"
  "<VRTRasterBand dataType=\"Byte\" band=\"1\">"
    "<SimpleSource>"
      "<SourceFilename relativeToVRT=\"1\">byte.vrt</SourceFilename>"
      "<SourceBand>1</SourceBand>"
      "<SourceProperties RasterXSize=\"20\" RasterYSize=\"20\" DataType=\"Byte\" BlockXSize=\"20\" BlockYSize=\"20\" />"
      "<SrcRect xOff=\"0\" yOff=\"0\" xSize=\"20\" ySize=\"20\"/>"
      "<DstRect xOff=\"0\" yOff=\"0\" xSize=\"20\" ySize=\"20\"/>"
    "</SimpleSource>"
  "</VRTRasterBand>"
"</VRTDataset>", GA_ReadOnly);
    GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));

    hDS = GDALOpenShared(GCORE_DATA_DIR "byte.tif", GA_ReadOnly);
    hDS = GDALOpenShared(GCORE_DATA_DIR "byte.tif", GA_ReadOnly);

    hDS = GDALOpenShared(GDRIVERS_DIR "data/sid/mercator.sid", GA_ReadOnly);

    hDS = GDALOpen("RASTERLITE:" GDRIVERS_DIR "data/rasterlite/rasterlite_pyramids.sqlite,table=test", GA_ReadOnly);
    hDS = GDALOpen("RASTERLITE:" GDRIVERS_DIR "data/rasterlite/rasterlite_pyramids.sqlite,table=test,level=1", GA_ReadOnly);

    OpenJPEG2000(GDRIVERS_DIR "data/jpeg2000/rgbwcmyk01_YeGeo_kakadu.jp2");

    hDS = GDALOpen(GDRIVERS_DIR "tmp/cache/Europe 2001_OZF.map", GA_ReadOnly);

    CPLDebug("TEST","Call GDALDestroyDriverManager()");
    GDALDestroyDriverManager();

    unlink("byte.tif");
    unlink("byte.tif.ovr");
    unlink("byte2.tif");
    unlink("byte3.tif");
    unlink("byte3.tif.msk");
    unlink("byte.vrt");
    unlink("byte.vrt.ovr");

    return 0;
}
