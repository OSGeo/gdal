/******************************************************************************
 *
 * Project:  HEIF read-only Driver
 * Author:   Even Rouault <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2020, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef HEIFDATASET_H_INCLUDED_
#define HEIFDATASET_H_INCLUDED_

#include "gdal_pam.h"
#include "ogr_spatialref.h"

#include "include_libheif.h"

#include "heifdrivercore.h"

#include <vector>
#include <geoheif.h>

/************************************************************************/
/*                        GDALHEIFDataset                               */
/************************************************************************/

class GDALHEIFDataset final : public GDALPamDataset
{
    friend class GDALHEIFRasterBand;

    heif_context *m_hCtxt = nullptr;
    heif_image_handle *m_hImageHandle = nullptr;
#ifndef LIBHEIF_SUPPORTS_TILES
    heif_image *m_hImage = nullptr;
#endif
    bool m_bFailureDecoding = false;
    std::vector<std::unique_ptr<GDALHEIFDataset>> m_apoOvrDS{};
    bool m_bIsThumbnail = false;

#ifdef LIBHEIF_SUPPORTS_TILES
    heif_image_tiling m_tiling;
#endif

#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 19, 0)
    void processProperties();
    gdal::GeoHEIF geoHEIF{};
#endif

#ifdef HAS_CUSTOM_FILE_READER
    heif_reader m_oReader{};
    VSILFILE *m_fpL = nullptr;
    vsi_l_offset m_nSize = 0;

    static int64_t GetPositionCbk(void *userdata);
    static int ReadCbk(void *data, size_t size, void *userdata);
    static int SeekCbk(int64_t position, void *userdata);
    static enum heif_reader_grow_status WaitForFileSizeCbk(int64_t target_size,
                                                           void *userdata);
#endif

    bool Init(GDALOpenInfo *poOpenInfo);
    void ReadMetadata();
    void OpenThumbnails();

#ifdef HAS_CUSTOM_FILE_WRITER
    static heif_error VFS_WriterCallback(struct heif_context *ctx,
                                         const void *data, size_t size,
                                         void *userdata);
#endif

  public:
    GDALHEIFDataset();
    ~GDALHEIFDataset();

    static GDALDataset *OpenHEIF(GDALOpenInfo *poOpenInfo);
#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 12, 0)
    static GDALDataset *OpenAVIF(GDALOpenInfo *poOpenInfo);
#endif

#if LIBHEIF_NUMERIC_VERSION >= BUILD_LIBHEIF_VERSION(1, 19, 0)
    void ReadUserDescription();
    const OGRSpatialReference *GetSpatialRef() const override;
    CPLErr GetGeoTransform(double *) override;
    int GetGCPCount() override;
    const GDAL_GCP *GetGCPs() override;
    const OGRSpatialReference *GetGCPSpatialRef() const override;
#endif

#ifdef HAS_CUSTOM_FILE_WRITER
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);
#endif
};
#endif /* HEIFDATASET_H_INCLUDED_ */
