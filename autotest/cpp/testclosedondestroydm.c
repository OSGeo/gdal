#include <cpl_conv.h>
#include <gdal.h>
#include <gdal_alg.h>

int main(int argc, char* argv[])
{
    CPLSetConfigOption("GDAL_SKIP", "GIF");

    GDALAllRegister();
    GDALDatasetH hDS;
    GDALDatasetH hSrcDS;

    hDS = GDALOpen("../gcore/data/byte.tif", GA_ReadOnly);
    if (hDS)
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));

    hDS = GDALOpen("../gcore/data/byte.vrt", GA_ReadOnly);
    if (hDS)
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));

    hDS = GDALOpen("../gdrivers/data/rgb_warp.vrt", GA_ReadOnly);
    if (hDS)
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));

    hDS = GDALOpen("../gdrivers/data/A.TOC", GA_ReadOnly);

    hDS = GDALOpen("NITF_TOC_ENTRY:CADRG_ONC_1,000,000_2_0:../gdrivers/data/A.TOC", GA_ReadOnly);
    if (hDS)
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));

    hDS = GDALOpen("../gdrivers/data/testtil.til", GA_ReadOnly);
    if (hDS)
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));

    hDS = GDALOpen("../gdrivers/data/product.xml", GA_ReadOnly);
    if (hDS)
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));

    hDS = GDALOpen("../gdrivers/data/METADATA.DIM", GA_ReadOnly);
    if (hDS)
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));

    hDS = GDALOpen("../gdrivers/tmp/cache/file9_j2c.ntf", GA_ReadOnly);
    if (hDS)
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));

    hDS = GDALOpen("../gdrivers/data/bug407.gif", GA_ReadOnly);
    if (hDS)
    {
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));
        GDALSetCacheMax(0);
        GDALChecksumImage(GDALGetRasterBand(hDS, 1), 0, 0, GDALGetRasterXSize(hDS), GDALGetRasterYSize(hDS));
    }

    /* Create external overviews */
    hSrcDS = GDALOpen("../gcore/data/byte.tif", GA_ReadOnly);
    hDS = GDALCreateCopy(GDALGetDriverByName("GTiff"), "byte.tif", hSrcDS, 0, NULL, NULL, NULL);
    GDALClose(hSrcDS);
    hSrcDS = NULL;
    hDS = GDALOpen("byte.tif", GA_ReadOnly);
    int nOvrLevel = 2;
    int nBandNum = 1;
    GDALBuildOverviews( hDS, "NEAR", 1, &nOvrLevel, 1, &nBandNum, NULL, NULL);
    GDALClose(hDS);

    hDS = GDALOpen("byte.tif", GA_ReadOnly);
    GDALGetOverviewCount(GDALGetRasterBand(hDS, 1));

    /* Create internal overviews */
    hSrcDS = GDALOpen("../gcore/data/byte.tif", GA_ReadOnly);
    hDS = GDALCreateCopy(GDALGetDriverByName("GTiff"), "byte2.tif", hSrcDS, 0, NULL, NULL, NULL);
    GDALClose(hSrcDS);
    hSrcDS = NULL;
    hDS = GDALOpen("byte2.tif", GA_Update);
    nOvrLevel = 2;
    nBandNum = 1;
    GDALBuildOverviews( hDS, "NEAR", 1, &nOvrLevel, 1, &nBandNum, NULL, NULL);
    GDALClose(hDS);

    hDS = GDALOpen("byte2.tif", GA_ReadOnly);
    GDALGetOverviewCount(GDALGetRasterBand(hDS, 1));

    /* Create external mask */
    hSrcDS = GDALOpen("../gcore/data/byte.tif", GA_ReadOnly);
    hDS = GDALCreateCopy(GDALGetDriverByName("GTiff"), "byte3.tif", hSrcDS, 0, NULL, NULL, NULL);
    GDALClose(hSrcDS);
    hSrcDS = NULL;
    hDS = GDALOpen("byte3.tif", GA_ReadOnly);
    GDALCreateDatasetMaskBand(hDS, GMF_PER_DATASET);
    GDALClose(hDS);

    hDS = GDALOpen("byte3.tif", GA_ReadOnly);
    GDALGetMaskFlags(GDALGetRasterBand(hDS, 1));

    FILE* f = fopen("byte.vrt", "wb");
    fprintf(f, "%s", "<VRTDataset rasterXSize=\"20\" rasterYSize=\"20\">"
  "<VRTRasterBand dataType=\"Byte\" band=\"1\">"
    "<SimpleSource>"
      "<SourceFilename relativeToVRT=\"1\">../gcore/data/byte.tif</SourceFilename>"
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
    GDALBuildOverviews( hDS, "NEAR", 1, &nOvrLevel, 1, &nBandNum, NULL, NULL);
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

    hDS = GDALOpenShared("../gcore/data/byte.tif", GA_ReadOnly);
    hDS = GDALOpenShared("../gcore/data/byte.tif", GA_ReadOnly);

    hDS = GDALOpenShared("../gdrivers/data/mercator.sid", GA_ReadOnly);

    hDS = GDALOpen("RASTERLITE:../gdrivers/data/rasterlite_pyramids.sqlite,table=test", GA_ReadOnly);
    hDS = GDALOpen("RASTERLITE:../gdrivers/data/rasterlite_pyramids.sqlite,table=test,level=1", GA_ReadOnly);

    CPLDebug("TEST","Call GDALDestroyDriverManager()");
    GDALDestroyDriverManager();

    unlink("byte.tif");
    unlink("byte.tif.ovr");
    unlink("byte2.tif");
    unlink("byte3.tif");
    unlink("byte3.tif.msk");
    unlink("byte.vrt");

    return 0;
}
