#if defined(JPEG_DUAL_MODE_8_12)
#define LIBJPEG_12_PATH   "libjpeg12/jpeglib.h" 
#define JPGDataset        JPGDataset12
#define JPGRasterBand     JPGRasterBand12
#define JPGMaskBand       JPGMaskBand12
#include "jpgdataset.cpp"

GDALDataset* JPEGDataset12Open(GDALOpenInfo* poOpenInfo)
{
    return JPGDataset12::Open(poOpenInfo);
}

GDALDataset* JPEGDataset12CreateCopy( const char * pszFilename,
                                    GDALDataset *poSrcDS,
                                    int bStrict, char ** papszOptions,
                                    GDALProgressFunc pfnProgress,
                                    void * pProgressData )
{
    return JPGDataset12::CreateCopy(pszFilename, poSrcDS,
                                    bStrict, papszOptions,
                                    pfnProgress, pProgressData);
}

#endif /* defined(JPEG_DUAL_MODE_8_12) */
