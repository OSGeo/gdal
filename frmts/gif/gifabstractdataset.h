/******************************************************************************
 *
 * Project:  GIF Driver
 * Purpose:  GIF Abstract Dataset
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ****************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GIFABSTRACTDATASET_H_INCLUDED
#define GIFABSTRACTDATASET_H_INCLUDED

#include "gdal_pam.h"

CPL_C_START
#include "gif_lib.h"
CPL_C_END

constexpr int InterlacedOffset[] = {0, 4, 2, 1};
constexpr int InterlacedJumps[] = {8, 8, 4, 2};

/************************************************************************/
/* ==================================================================== */
/*                        GIFAbstractDataset                            */
/* ==================================================================== */
/************************************************************************/

class GIFAbstractDataset CPL_NON_FINAL : public GDALPamDataset
{
  protected:
    friend class GIFAbstractRasterBand;

    VSILFILE *fp;

    GifFileType *hGifFile;

    int bGeoTransformValid;
    double adfGeoTransform[6];

    int nGCPCount;
    GDAL_GCP *pasGCPList;

    int bHasReadXMPMetadata;
    void CollectXMPMetadata();

    CPLString osWldFilename;

    void DetectGeoreferencing(GDALOpenInfo *poOpenInfo);

  public:
    GIFAbstractDataset();
    ~GIFAbstractDataset() override;

    CPLErr GetGeoTransform(double *) override;
    int GetGCPCount() override;
    const GDAL_GCP *GetGCPs() override;

    char **GetMetadataDomainList() override;
    char **GetMetadata(const char *pszDomain = "") override;

    char **GetFileList() override;

    static GifFileType *myDGifOpen(void *userPtr, InputFunc readFunc);
    static int myDGifCloseFile(GifFileType *hGifFile);
    static int myEGifCloseFile(GifFileType *hGifFile);
    static int ReadFunc(GifFileType *psGFile, GifByteType *pabyBuffer,
                        int nBytesToRead);
    static GifRecordType FindFirstImage(GifFileType *hGifFile);
};

/************************************************************************/
/* ==================================================================== */
/*                        GIFAbstractRasterBand                         */
/* ==================================================================== */
/************************************************************************/

class GIFAbstractRasterBand CPL_NON_FINAL : public GDALPamRasterBand
{
  protected:
    SavedImage *psImage;

    int *panInterlaceMap;

    GDALColorTable *poColorTable;

    int nTransparentColor;

  public:
    GIFAbstractRasterBand(GIFAbstractDataset *poDS, int nBand,
                          SavedImage *psSavedImage, int nBackground,
                          int bAdvertiseInterlacedMDI);
    ~GIFAbstractRasterBand() override;

    double GetNoDataValue(int *pbSuccess = nullptr) override;
    GDALColorInterp GetColorInterpretation() override;
    GDALColorTable *GetColorTable() override;
};

#endif
