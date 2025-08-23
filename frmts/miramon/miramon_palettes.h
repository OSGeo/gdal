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
    MMRPalettes(MMRRel &fRel, CPLString osBandSectionIn);
    MMRPalettes(const MMRPalettes &) =
        delete;  // I don't want to construct a MMRPalettes from another MMRBand (effc++)
    MMRPalettes &operator=(const MMRPalettes &) =
        delete;  // I don't want to assing a MMRPalettes to another MMRBand (effc++)
    ~MMRPalettes();

    bool IsValid() const
    {
        return bIsValid;
    }

    bool IsCategorical() const
    {
        return bIsCategorical;
    }

    void SetIsCategorical(bool bIsCategoricalIn)
    {
        bIsCategorical = bIsCategoricalIn;
    }

    bool IsConstantColor() const
    {
        return bIsConstantColor;
    }

    GDALColorEntry GetDefaultColorRGB() const
    {
        return sDefaultColorRGB;
    }

    GDALColorEntry GetConstantColorRGB() const
    {
        return sConstantColorRGB;
    }

    void SetConstantColorRGB(GDALColorEntry sConstantColorRGBIn)
    {
        sConstantColorRGB = sConstantColorRGBIn;
    }

    void SetConstantColorRGB(short c1, short c2, short c3)
    {
        sConstantColorRGB.c1 = c1;
        sConstantColorRGB.c2 = c2;
        sConstantColorRGB.c3 = c3;
    }

    void SetConstantColorRGB(short c1, short c2, short c3, short c4)
    {
        sConstantColorRGB.c1 = c1;
        sConstantColorRGB.c2 = c2;
        sConstantColorRGB.c3 = c3;
        sConstantColorRGB.c4 = c4;
    }

    bool HasNodata() const
    {
        return bHasNodata;
    }

    void SetHasNodata(bool bHasNodataIn)
    {
        bHasNodata = bHasNodataIn;
    }

    int GetNoDataPaletteIndex() const
    {
        return nNoDataPaletteIndex;
    }

    void SetNoDataPaletteIndex(bool nNoDataPaletteIndexIn)
    {
        nNoDataPaletteIndex = nNoDataPaletteIndexIn;
    }

    GDALColorEntry GetNoDataDefaultColor() const
    {
        return sNoDataColorRGB;
    }

    double GetPaletteColorsValue(int nIComponent, int nIColor) const
    {
        return aadfPaletteColors[nIComponent][nIColor];
    }

    int GetSizeOfPaletteColors() const
    {
        return static_cast<int>(aadfPaletteColors[0].size());
    }

    int GetNumberOfColors() const
    {
        return nNPaletteColors;
    }

    // Real means with no nodata.
    int GetNumberOfColorsIncludingNodata() const
    {
        return nRealNPaletteColors;
    }

    void UpdateColorInfo();

    ColorTreatment ColorScaling = ColorTreatment::DEFAULT_SCALING;

  private:
    static CPLErr GetPaletteColors_DBF_Indexs(
        struct MM_DATA_BASE_XP &oColorTable, MM_EXT_DBF_N_FIELDS &nClauSimbol,
        MM_EXT_DBF_N_FIELDS &nRIndex, MM_EXT_DBF_N_FIELDS &nGIndex,
        MM_EXT_DBF_N_FIELDS &nBIndex);
    CPLErr GetPaletteColors_DBF(CPLString os_Color_Paleta_DBF);
    CPLErr GetPaletteColors_PAL_P25_P65(CPLString os_Color_Paleta_DBF);
    void AssignColorFromDBF(struct MM_DATA_BASE_XP &oColorTable,
                            char *pzsRecord, char *pszField,
                            MM_EXT_DBF_N_FIELDS &nRIndex,
                            MM_EXT_DBF_N_FIELDS &nGIndex,
                            MM_EXT_DBF_N_FIELDS &nBIndex, int nIPaletteIndex);
    CPLErr UpdateConstantColor();

    std::array<std::vector<double>, 4> aadfPaletteColors{};
    bool bIsCategorical = false;

    // Palette info
    GDALColorEntry sDefaultColorRGB = {0, 0, 0, 127};

    bool bHasNodata = false;
    // index in the DBF that gives nodata color
    int nNoDataPaletteIndex = 0;
    // Default color for nodata
    GDALColorEntry sNoDataColorRGB = {0, 0, 0, 0};

    bool bIsConstantColor = false;
    GDALColorEntry sConstantColorRGB = {0, 0, 0, 0};

    int nNPaletteColors = 0;
    int nRealNPaletteColors = 0;  // Without nodata

    MMRRel *pfRel = nullptr;  // Rel where metadata is readed from
    CPLString osBandSection;

    bool bIsValid = false;  // Determines if the created object is valid or not.
};

#endif  // MMRPALETTES_H_INCLUDED
