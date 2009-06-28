#if defined(JPEG_DUAL_MODE_8_12)
#define LIBJPEG_12_PATH   "libjpeg12/jpeglib.h" 
#define JPGDataset        JPGDataset12
#define JPGRasterBand     JPGRasterBand12
#define JPGMaskBand       JPGMaskBand12
#define JPEGCreateCopy    JPEGCreateCopy12
#include "jpgdataset.cpp"

GDALDataset* JPEGDataset12Open(GDALOpenInfo* poOpenInfo)
{
    return JPGDataset12::Open(poOpenInfo);
}
#endif /* defined(JPEG_DUAL_MODE_8_12) */
