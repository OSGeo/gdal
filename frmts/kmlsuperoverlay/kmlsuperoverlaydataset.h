/******************************************************************************
 *
 * Project:  KmlSuperOverlay
 * Purpose:  Implements write support for KML superoverlay - KMZ.
 * Author:   Harsh Govind, harsh.govind@spadac.com
 *
 ******************************************************************************
 * Copyright (c) 2010, SPADAC Inc. <harsh.govind@spadac.com>
 * Copyright (c) 2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef KMLSUPEROVERLAYDATASET_H_INCLUDED
#define KMLSUPEROVERLAYDATASET_H_INCLUDED

#include <array>
#include <map>

#include "cpl_minixml.h"
#include "gdal_pam.h"
#include "gdal_priv.h"

/************************************************************************/
/*                      KmlSuperOverlayReadDataset                      */
/************************************************************************/
class KmlSuperOverlayRasterBand;
class KmlSuperOverlayReadDataset;

class LinkedDataset;

class LinkedDataset
{
  public:
    KmlSuperOverlayReadDataset *poDS = nullptr;
    LinkedDataset *psPrev = nullptr;
    LinkedDataset *psNext = nullptr;
    CPLString osSubFilename{};

    LinkedDataset() = default;

  private:
    LinkedDataset(const LinkedDataset &) = delete;
    LinkedDataset &operator=(const LinkedDataset &) = delete;
};

class KmlSuperOverlayReadDataset final : public GDALDataset
{
    friend class KmlSuperOverlayRasterBand;

    OGRSpatialReference m_oSRS{};
    int nFactor = 1;
    CPLString osFilename{};
    CPLXMLNode *psRoot = nullptr;
    CPLXMLNode *psDocument = nullptr;
    std::unique_ptr<GDALDataset> poDSIcon{};
    GDALGeoTransform m_gt{};

    std::vector<std::unique_ptr<KmlSuperOverlayReadDataset>> m_apoOverviewDS{};
    bool bIsOvr = false;

    KmlSuperOverlayReadDataset *poParent = nullptr;

    std::map<CPLString, LinkedDataset *> oMapChildren{};
    LinkedDataset *psFirstLink = nullptr;
    LinkedDataset *psLastLink = nullptr;

    KmlSuperOverlayReadDataset(const KmlSuperOverlayReadDataset &) = delete;
    KmlSuperOverlayReadDataset &
    operator=(const KmlSuperOverlayReadDataset &) = delete;

  protected:
    int CloseDependentDatasets() override;

  public:
    KmlSuperOverlayReadDataset();
    ~KmlSuperOverlayReadDataset() override;

    static int Identify(GDALOpenInfo *);
    static GDALDataset *Open(const char *pszFilename,
                             KmlSuperOverlayReadDataset *poParent = nullptr,
                             int nRec = 0);
    static GDALDataset *Open(GDALOpenInfo *);

    static const int KMLSO_ContainsOpaquePixels = 0x1;
    static const int KMLSO_ContainsTransparentPixels = 0x2;
    static const int KMLSO_ContainsPartiallyTransparentPixels = 0x4;

    static int DetectTransparency(int rxsize, int rysize, int rx, int ry,
                                  int dxsize, int dysize, GDALDataset *poSrcDs);

    CPLErr GetGeoTransform(GDALGeoTransform &gt) const override;
    const OGRSpatialReference *GetSpatialRef() const override;

    CPLErr IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff, int nXSize,
                     int nYSize, void *pData, int nBufXSize, int nBufYSize,
                     GDALDataType eBufType, int nBandCount,
                     BANDMAP_TYPE panBandMap, GSpacing nPixelSpace,
                     GSpacing nLineSpace, GSpacing nBandSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;
};

/************************************************************************/
/*                      KmlSuperOverlayRasterBand                       */
/************************************************************************/

class KmlSuperOverlayRasterBand final : public GDALRasterBand
{
  public:
    KmlSuperOverlayRasterBand(KmlSuperOverlayReadDataset *poDS, int nBand);

  protected:
    CPLErr IReadBlock(int, int, void *) override;
    CPLErr IRasterIO(GDALRWFlag, int, int, int, int, void *, int, int,
                     GDALDataType, GSpacing nPixelSpace, GSpacing nLineSpace,
                     GDALRasterIOExtraArg *psExtraArg) override;
    GDALColorInterp GetColorInterpretation() override;

    int GetOverviewCount() override;
    GDALRasterBand *GetOverview(int) override;
};

#endif /* ndef KMLSUPEROVERLAYDATASET_H_INCLUDED */
