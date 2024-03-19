/******************************************************************************
 *
 * Project:  JPEG2000 driver based on OpenJPEG or Grok library
 * Purpose:  JPEG2000 driver based on OpenJPEG or Grok library
 * Authors:  Even Rouault, <even dot rouault at spatialys dot com>
 *           Aaron Boxer, <boxerab at protonmail dot com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys dot com>
 * Copyright (c) 2015, European Union (European Environment Agency)
 * Copyright (c) 2023, Grok Image Compression Inc.
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

#pragma once

#include "cpl_atomic_ops.h"
#include "cpl_multiproc.h"
#include "cpl_string.h"
#include "cpl_worker_thread_pool.h"
#include "gdaljp2abstractdataset.h"
#include "gdaljp2metadata.h"

typedef int JP2_COLOR_SPACE;
typedef int JP2_PROG_ORDER;

#define JP2_LRCP 0
#define JP2_RLCP 1
#define JP2_RPCL 2
#define JP2_PCRL 3
#define JP2_CPRL 4

enum JP2_ENUM
{
    JP2_CLRSPC_UNKNOWN,
    JP2_CLRSPC_SRGB,
    JP2_CLRSPC_GRAY,
    JP2_CLRSPC_SYCC,
    JP2_CODEC_J2K,
    JP2_CODEC_JP2
};

typedef struct
{
    VSILFILE *fp_;
    vsi_l_offset nBaseOffset;
} JP2File;

/************************************************************************/
/* ==================================================================== */
/*                           JP2DatasetBase                             */
/* ==================================================================== */
/************************************************************************/

struct JP2DatasetBase
{
    int GetNumThreads()
    {
        if (nThreads >= 1)
            return nThreads;

        const char *pszThreads =
            CPLGetConfigOption("GDAL_NUM_THREADS", "ALL_CPUS");
        if (EQUAL(pszThreads, "ALL_CPUS"))
            nThreads = CPLGetNumCPUs();
        else
            nThreads = atoi(pszThreads);
        if (nThreads > 128)
            nThreads = 128;
        if (nThreads <= 0)
            nThreads = 1;
        return nThreads;
    }

    std::string m_osFilename;
    VSILFILE *fp_ = nullptr; /* Large FILE API */
    vsi_l_offset nCodeStreamStart = 0;
    vsi_l_offset nCodeStreamLength = 0;

    int nRedIndex = 0;
    int nGreenIndex = 1;
    int nBlueIndex = 2;
    int nAlphaIndex = -1;

    int bIs420 = FALSE;

    int nParentXSize = 0;
    int nParentYSize = 0;
    int iLevel = 0;
    int nOverviewCount = 0;

    int bEnoughMemoryToLoadOtherBands = TRUE;
    int bRewrite = FALSE;
    int bHasGeoreferencingAtOpening = FALSE;

    int nThreads = -1;
    bool bUseSetDecodeArea = false;
    bool bSingleTiled = false;
    int m_nBlocksToLoad = 0;
    int m_nX0 = 0;
    int m_nY0 = 0;
    uint32_t m_nTileWidth = 0;
    uint32_t m_nTileHeight = 0;
};

/************************************************************************/
/* ==================================================================== */
/*                           JP2OPJLikeDataset                          */
/* ==================================================================== */
/************************************************************************/
template <typename CODEC, typename BASE> class JP2OPJLikeRasterBand;

template <typename CODEC, typename BASE>
class JP2OPJLikeDataset final : public GDALJP2AbstractDataset, public BASE
{
    friend class JP2OPJLikeRasterBand<CODEC, BASE>;
    JP2OPJLikeDataset **papoOverviewDS = nullptr;

  protected:
    virtual int CloseDependentDatasets() override;
    virtual VSILFILE *GetFileHandle() override;
    CPLErr Close() override;

  public:
    JP2OPJLikeDataset();
    virtual ~JP2OPJLikeDataset();

    static int Identify(GDALOpenInfo *poOpenInfo);
    static GDALDataset *Open(GDALOpenInfo *);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);

    CPLErr SetSpatialRef(const OGRSpatialReference *poSRS) override;

    virtual CPLErr SetGeoTransform(double *) override;

    CPLErr SetGCPs(int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                   const OGRSpatialReference *poSRS) override;

    virtual CPLErr SetMetadata(char **papszMetadata,
                               const char *pszDomain = "") override;
    virtual CPLErr SetMetadataItem(const char *pszName, const char *pszValue,
                                   const char *pszDomain = "") override;

    virtual CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                             int nXSize, int nYSize, void *pData, int nBufXSize,
                             int nBufYSize, GDALDataType eBufType,
                             int nBandCount, int *panBandMap,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GSpacing nBandSpace,
                             GDALRasterIOExtraArg *psExtraArg) override;

    virtual GIntBig GetEstimatedRAMUsage() override;

    CPLErr IBuildOverviews(const char *pszResampling, int nOverviews,
                           const int *panOverviewList, int nListBands,
                           const int *panBandList, GDALProgressFunc pfnProgress,
                           void *pProgressData,
                           CSLConstList papszOptions) override;

    static bool WriteBox(VSILFILE *fp, GDALJP2Box *poBox);
    static bool WriteGDALMetadataBox(VSILFILE *fp, GDALDataset *poSrcDS,
                                     char **papszOptions);
    static bool WriteXMLBoxes(VSILFILE *fp, GDALDataset *poSrcDS);
    static bool WriteXMPBox(VSILFILE *fp, GDALDataset *poSrcDS);
    static bool WriteIPRBox(VSILFILE *fp, GDALDataset *poSrcDS);

    CPLErr ReadBlock(int nBand, VSILFILE *fp, int nBlockXOff, int nBlockYOff,
                     void *pImage, int nBandCount, int *panBandMap);

    int PreloadBlocks(JP2OPJLikeRasterBand<CODEC, BASE> *poBand, int nXOff,
                      int nYOff, int nXSize, int nYSize, int nBandCount,
                      int *panBandMap);

    static void ReadBlockInThread(void *userdata);
};

/************************************************************************/
/* ==================================================================== */
/*                         JP2OPJLikeRasterBand                         */
/* ==================================================================== */
/************************************************************************/

template <typename CODEC, typename BASE>
class JP2OPJLikeRasterBand final : public GDALPamRasterBand
{
    friend class JP2OPJLikeDataset<CODEC, BASE>;
    int bPromoteTo8Bit;
    GDALColorTable *poCT;

  public:
    JP2OPJLikeRasterBand(JP2OPJLikeDataset<CODEC, BASE> *poDSIn, int nBandIn,
                         GDALDataType eDataTypeIn, int nBits,
                         int bPromoteTo8BitIn, int nBlockXSizeIn,
                         int nBlockYSizeIn);
    virtual ~JP2OPJLikeRasterBand();

    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                             int nXSize, int nYSize, void *pData, int nBufXSize,
                             int nBufYSize, GDALDataType eBufType,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GDALRasterIOExtraArg *psExtraArg) override;

    virtual GDALColorInterp GetColorInterpretation() override;
    virtual GDALColorTable *GetColorTable() override;

    virtual int GetOverviewCount() override;
    virtual GDALRasterBand *GetOverview(int iOvrLevel) override;

    virtual int HasArbitraryOverviews() override;
};

#ifdef unused
void GDALRegisterJP2();
#endif
