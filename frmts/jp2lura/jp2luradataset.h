/******************************************************************************
 * Project:  GDAL
 * Author:   Raul Alonso Reyes <raul dot alonsoreyes at satcen dot europa dot
 *eu> Author:   Even Rouault, <even dot rouault at spatialys dot com> Purpose:
 *JPEG-2000 driver based on Lurawave library, driver developed by SatCen
 *
 ******************************************************************************
 * Copyright (c) 2016, SatCen - European Union Satellite Centre
 * Copyright (c) 2016, Even Rouault
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef JP2LURADATASET_H_INCLUDED
#define JP2LURADATASET_H_INCLUDED

#include "gdaljp2abstractdataset.h"
#include "jp2luracallbacks.h"
#include "gdaljp2metadata.h"

class JP2LuraDataset final : public GDALJP2AbstractDataset
{
    friend class JP2LuraRasterBand;

    VSILFILE *fp;  // Large FILE API

    int iLevel;
    int nOverviewCount;
    JP2LuraDataset **papoOverviewDS;
    GDALJP2Lura_Output_Data sOutputData;
    GDALColorTable *poCT;
    JP2_Colorspace eColorspace;
    int nRedIndex;
    int nGreenIndex;
    int nBlueIndex;
    int nAlphaIndex;

#ifdef ENABLE_MEMORY_REGISTRAR
    JP2LuraMemoryRegistrar oMemoryRegistrar;
#endif

  public:
    JP2LuraDataset();
    ~JP2LuraDataset();

    static GDALDataset *Open(GDALOpenInfo *);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);

    virtual CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                             int nXSize, int nYSize, void *pData, int nBufXSize,
                             int nBufYSize, GDALDataType eBufType,
                             int nBandCount, BANDMAP_TYPE panBandMap,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GSpacing nBandSpace,
                             GDALRasterIOExtraArg *psExtraArg) override;

    static void WriteBox(VSILFILE *fp, GDALJP2Box *poBox);
    static void WriteGDALMetadataBox(VSILFILE *fp, GDALDataset *poSrcDS,
                                     char **papszOptions);
    static void WriteXMLBoxes(VSILFILE *fp, GDALDataset *poSrcDS,
                              char **papszOptions);
    static void WriteXMPBox(VSILFILE *fp, GDALDataset *poSrcDS,
                            char **papszOptions);

    static const char *GetErrorMessage(long nErrorCode);
};

#endif
