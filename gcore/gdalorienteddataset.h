/**********************************************************************
 *
 * Project:  GDAL
 * Purpose:  Dataset that modifies the orientation of an underlying dataset
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 **********************************************************************
 * Copyright (c) 2022, Even Rouault, <even dot rouault at spatialys dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef GDAL_ORIENTED_DATASET_H
#define GDAL_ORIENTED_DATASET_H

#include "gdal_priv.h"

//! @cond Doxygen_Suppress

/************************************************************************/
/*                         GDALOrientedDataset                          */
/************************************************************************/

class CPL_DLL GDALOrientedDataset : public GDALDataset
{
  public:
    /** Origin of the source dataset.
     *
     * Defines of the point at (row, col) = (0, 0) in the source dataset
     * should be interpreted to generate the dataset taking into account
     * this orientation.
     *
     * Numeric values are the same as in TIFF and EXIF Orientation tags.
     *
     * See http://sylvana.net/jpegcrop/exif_orientation.html for clear
     * explanations.
     */
    enum class Origin
    {
        TOP_LEFT = 1,  /* row 0 top, col 0 lhs */
        TOP_RIGHT = 2, /* row 0 top, col 0 rhs */
        BOT_RIGHT = 3, /* row 0 bottom, col 0 rhs */
        BOT_LEFT = 4,  /* row 0 bottom, col 0 lhs */
        LEFT_TOP = 5,  /* row 0 lhs, col 0 top */
        RIGHT_TOP = 6, /* row 0 rhs, col 0 top */
        RIGHT_BOT = 7, /* row 0 rhs, col 0 bottom */
        LEFT_BOT = 8,  /* row 0 lhs, col 0 bottom */
    };

    GDALOrientedDataset(GDALDataset *poSrcDataset, Origin eOrigin);
    GDALOrientedDataset(std::unique_ptr<GDALDataset> &&poSrcDataset,
                        Origin eOrigin);

    char **GetMetadataDomainList() override
    {
        return m_poSrcDS->GetMetadataDomainList();
    }

    char **GetMetadata(const char *pszDomain = "") override;
    const char *GetMetadataItem(const char *pszName,
                                const char *pszDomain = "") override;

  private:
    friend class GDALOrientedRasterBand;

    std::unique_ptr<GDALDataset> m_poSrcDSHolder{};
    GDALDataset *m_poSrcDS = nullptr;
    Origin m_eOrigin;
    CPLStringList m_aosSrcMD{};
    CPLStringList m_aosSrcMD_EXIF{};

    GDALOrientedDataset(const GDALOrientedDataset &) = delete;
    GDALOrientedDataset &operator=(const GDALOrientedDataset &) = delete;
};

//! @endcond

#endif
