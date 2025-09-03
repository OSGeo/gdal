/******************************************************************************
 *
 * Project:  MiraMon Raster Driver
 * Purpose:  Implements MMRPalettes class: handles access to a DBF file
 *           containing color information, which is then converted into
 *           either a color table or an attribute table, depending on the
 *           context.
 * Author:   Abel Pau
 *
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MMRPALETTES_H_INCLUDED
#define MMRPALETTES_H_INCLUDED

#include <array>

#include "../miramon_common/mm_gdal_constants.h"  // For MM_EXT_DBF_N_FIELDS

class MMRRel;

enum class ColorTreatment
{
    DEFAULT_SCALING = 0,
    DIRECT_ASSIGNATION = 1,
    ORIGIN_DISPLACEMENT = 2,
    LINEAR_SCALING = 3,
    LOG_10_SCALING = 4,
    USER_INTERVALS = 5
};

/* ==================================================================== */
/*                            MMRPalettes                             */
/* ==================================================================== */

class MMRPalettes
{
  public:
    MMRPalettes(MMRRel &fRel, const CPLString &osBandSectionIn);
    MMRPalettes(const MMRPalettes &) =
        delete;  // I don't want to construct a MMRPalettes from another MMRBand (effc++)
    MMRPalettes &operator=(const MMRPalettes &) =
        delete;  // I don't want to assign a MMRPalettes to another MMRBand (effc++)
    ~MMRPalettes();

    bool IsValid() const
    {
        return m_bIsValid;
    }

    bool IsCategorical() const
    {
        return m_bIsCategorical;
    }

    void SetIsCategorical(bool bIsCategoricalIn)
    {
        m_bIsCategorical = bIsCategoricalIn;
    }

    bool IsConstantColor() const
    {
        return m_bIsConstantColor;
    }

    GDALColorEntry GetDefaultColorRGB() const
    {
        return m_sDefaultColorRGB;
    }

    GDALColorEntry GetConstantColorRGB() const
    {
        return m_sConstantColorRGB;
    }

    void SetConstantColorRGB(GDALColorEntry sConstantColorRGBIn)
    {
        m_sConstantColorRGB = sConstantColorRGBIn;
    }

    void SetConstantColorRGB(short c1, short c2, short c3)
    {
        m_sConstantColorRGB.c1 = c1;
        m_sConstantColorRGB.c2 = c2;
        m_sConstantColorRGB.c3 = c3;
    }

    void SetConstantColorRGB(short c1, short c2, short c3, short c4)
    {
        m_sConstantColorRGB.c1 = c1;
        m_sConstantColorRGB.c2 = c2;
        m_sConstantColorRGB.c3 = c3;
        m_sConstantColorRGB.c4 = c4;
    }

    bool HasNodata() const
    {
        return m_bHasNodata;
    }

    void SetHasNodata(bool bHasNodataIn)
    {
        m_bHasNodata = bHasNodataIn;
    }

    int GetNoDataPaletteIndex() const
    {
        return m_nNoDataPaletteIndex;
    }

    void SetNoDataPaletteIndex(bool nNoDataPaletteIndexIn)
    {
        m_nNoDataPaletteIndex = nNoDataPaletteIndexIn;
    }

    GDALColorEntry GetNoDataDefaultColor() const
    {
        return m_sNoDataColorRGB;
    }

    double GetPaletteColorsValue(int nIComponent, int nIColor) const
    {
        return m_aadfPaletteColors[nIComponent][nIColor];
    }

    int GetSizeOfPaletteColors() const
    {
        return static_cast<int>(m_aadfPaletteColors[0].size());
    }

    int GetNumberOfColors() const
    {
        return m_nNPaletteColors;
    }

    // Real means with no nodata.
    int GetNumberOfColorsIncludingNodata() const
    {
        return m_nRealNPaletteColors;
    }

    void UpdateColorInfo();

    ColorTreatment ColorScaling = ColorTreatment::DEFAULT_SCALING;

  private:
    static CPLErr GetPaletteColors_DBF_Indexes(
        struct MM_DATA_BASE_XP &oColorTable, MM_EXT_DBF_N_FIELDS &nClauSimbol,
        MM_EXT_DBF_N_FIELDS &nRIndex, MM_EXT_DBF_N_FIELDS &nGIndex,
        MM_EXT_DBF_N_FIELDS &nBIndex);
    CPLErr GetPaletteColors_DBF(CPLString os_Color_Paleta_DBF);
    CPLErr GetPaletteColors_PAL_P25_P65(const CPLString &os_Color_Paleta_DBF);
    void AssignColorFromDBF(struct MM_DATA_BASE_XP &oColorTable,
                            char *pzsRecord, char *pszField,
                            MM_EXT_DBF_N_FIELDS &nRIndex,
                            MM_EXT_DBF_N_FIELDS &nGIndex,
                            MM_EXT_DBF_N_FIELDS &nBIndex, int nIPaletteIndex);
    CPLErr UpdateConstantColor();

    std::array<std::vector<double>, 4> m_aadfPaletteColors{};
    bool m_bIsCategorical = false;

    // Palette info
    GDALColorEntry m_sDefaultColorRGB = {0, 0, 0, 127};

    bool m_bHasNodata = false;
    // index in the DBF that gives nodata color
    int m_nNoDataPaletteIndex = 0;
    // Default color for nodata
    GDALColorEntry m_sNoDataColorRGB = {0, 0, 0, 0};

    bool m_bIsConstantColor = false;
    GDALColorEntry m_sConstantColorRGB = {0, 0, 0, 0};

    int m_nNPaletteColors = 0;
    int m_nRealNPaletteColors = 0;  // Without nodata

    MMRRel *m_pfRel = nullptr;  // Rel where metadata is read from
    CPLString m_osBandSection;

    bool m_bIsValid =
        false;  // Determines if the created object is valid or not.
};

#endif  // MMRPALETTES_H_INCLUDED
