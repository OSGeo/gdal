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

    bool IsAutomatic() const
    {
        return m_bIsAutomatic;
    }

    ColorTreatment GetColorScaling() const
    {
        return m_ColorScaling;
    }

    void SetColorScaling(enum ColorTreatment colorScaling)
    {
        m_ColorScaling = colorScaling;
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

    CPLErr UpdateColorInfo();

  private:
    static CPLErr GetPaletteColors_DBF_Indexes(
        struct MM_DATA_BASE_XP &oColorTable, MM_EXT_DBF_N_FIELDS &nClauSimbol,
        MM_EXT_DBF_N_FIELDS &nRIndex, MM_EXT_DBF_N_FIELDS &nGIndex,
        MM_EXT_DBF_N_FIELDS &nBIndex);
    CPLErr GetPaletteColors_DBF(const CPLString &os_Color_Paleta_DBF);
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

    bool m_bIsAutomatic = false;
    ColorTreatment m_ColorScaling = ColorTreatment::DEFAULT_SCALING;

    bool m_bIsConstantColor = false;
    GDALColorEntry m_sConstantColorRGB = {0, 0, 0, 0};

    int m_nNPaletteColors = 0;
    int m_nRealNPaletteColors = 0;  // Without nodata

    MMRRel *m_pfRel = nullptr;  // Rel where metadata is read from
    CPLString m_osBandSection;

    bool m_bIsValid =
        false;  // Determines if the created object is valid or not.

    // To be used in a categorical raster with Automatic palette associated
    const std::vector<GDALColorEntry> m_ThematicPalette = {
        {0, 0, 255, 0},     {0, 255, 255, 0},   {0, 255, 0, 0},
        {255, 255, 0, 0},   {255, 0, 0, 0},     {255, 0, 255, 0},
        {191, 191, 191, 0}, {0, 128, 255, 0},   {128, 0, 255, 0},
        {0, 255, 128, 0},   {128, 255, 0, 0},   {255, 128, 0, 0},
        {255, 0, 128, 0},   {128, 255, 255, 0}, {128, 128, 255, 0},
        {128, 255, 128, 0}, {255, 128, 255, 0}, {255, 128, 128, 0},
        {255, 255, 128, 0}, {128, 128, 128, 0}, {0, 0, 128, 0},
        {0, 128, 128, 0},   {0, 128, 0, 0},     {128, 128, 0, 0},
        {128, 0, 0, 0},     {128, 0, 128, 0},   {64, 64, 64, 0},
        {0, 0, 191, 0},     {128, 128, 191, 0}, {0, 191, 191, 0},
        {0, 191, 0, 0},     {191, 191, 0, 0},   {191, 0, 0, 0},
        {191, 0, 191, 0},   {0, 128, 191, 0},   {128, 0, 191, 0},
        {128, 191, 191, 0}, {0, 191, 128, 0},   {128, 191, 0, 0},
        {128, 191, 128, 0}, {191, 191, 128, 0}, {191, 128, 0, 0},
        {191, 128, 128, 0}, {191, 0, 128, 0},   {191, 128, 191, 0},
        {0, 0, 64, 0},      {0, 64, 64, 0},     {0, 64, 0, 0},
        {64, 64, 0, 0},     {64, 0, 0, 0},      {64, 0, 64, 0},
        {0, 128, 64, 0},    {128, 0, 64, 0},    {0, 64, 128, 0},
        {128, 64, 0, 0},    {64, 128, 0, 0},    {64, 0, 128, 0},
        {128, 64, 64, 0},   {128, 128, 64, 0},  {128, 64, 128, 0},
        {64, 128, 64, 0},   {64, 128, 128, 0},  {64, 64, 128, 0},
        {0, 191, 64, 0},    {191, 0, 64, 0},    {0, 64, 191, 0},
        {191, 64, 0, 0},    {64, 191, 0, 0},    {64, 0, 191, 0},
        {191, 64, 64, 0},   {191, 191, 64, 0},  {191, 64, 191, 0},
        {64, 191, 64, 0},   {64, 191, 191, 0},  {64, 64, 191, 0},
        {15, 177, 228, 0},  {184, 91, 96, 0},   {105, 246, 240, 0},
        {139, 224, 27, 0},  {113, 111, 125, 0}, {188, 184, 147, 0},
        {125, 225, 235, 0}, {78, 166, 108, 0},  {185, 87, 250, 0},
        {171, 224, 154, 0}, {60, 25, 133, 0},   {227, 239, 158, 0},
        {140, 139, 108, 0}, {101, 195, 115, 0}, {67, 245, 217, 0},
        {150, 123, 223, 0}, {71, 86, 92, 0},    {206, 18, 20, 0},
        {255, 99, 85, 0},   {233, 235, 42, 0},  {254, 235, 235, 0},
        {18, 82, 160, 0},   {43, 82, 250, 0},   {33, 5, 223, 0},
        {132, 212, 136, 0}, {166, 250, 155, 0}, {95, 116, 2, 0},
        {249, 5, 22, 0},    {5, 221, 152, 0},   {56, 5, 194, 0},
        {6, 243, 169, 0},   {29, 149, 23, 0},   {87, 85, 251, 0},
        {128, 200, 197, 0}, {73, 120, 48, 0},   {211, 29, 1, 0},
        {97, 13, 26, 0},    {201, 31, 248, 0},  {163, 224, 32, 0},
        {46, 82, 238, 0},   {212, 53, 216, 0},  {101, 255, 186, 0},
        {205, 131, 99, 0},  {49, 191, 141, 0},  {23, 115, 53, 0},
        {11, 97, 56, 0},    {108, 208, 111, 0}, {181, 80, 251, 0},
        {53, 14, 0, 0},     {205, 190, 17, 0},  {79, 221, 250, 0},
        {40, 182, 251, 0},  {227, 91, 248, 0},  {119, 235, 88, 0},
        {93, 224, 88, 0},   {149, 185, 129, 0}, {245, 143, 30, 0},
        {23, 219, 5, 0},    {211, 59, 65, 0},   {31, 125, 29, 0},
        {49, 251, 93, 0},   {78, 112, 183, 0},  {142, 195, 201, 0},
        {206, 74, 49, 0},   {45, 221, 241, 0},  {61, 28, 13, 0},
        {139, 41, 68, 0},   {178, 130, 74, 0},  {140, 229, 251, 0},
        {119, 165, 107, 0}, {53, 175, 23, 0},   {100, 38, 228, 0},
        {111, 88, 65, 0},   {196, 157, 233, 0}, {131, 162, 134, 0},
        {58, 171, 196, 0},  {115, 116, 93, 0},  {159, 232, 239, 0},
        {217, 200, 153, 0}, {171, 59, 69, 0},   {73, 206, 236, 0},
        {11, 171, 170, 0},  {101, 142, 165, 0}, {156, 147, 175, 0},
        {156, 199, 79, 0},  {212, 47, 90, 0},   {65, 2, 123, 0},
        {120, 20, 65, 0},   {153, 51, 45, 0},   {248, 171, 167, 0},
        {59, 143, 51, 0},   {137, 68, 226, 0},  {161, 30, 43, 0},
        {96, 97, 26, 0},    {155, 184, 199, 0}, {105, 53, 146, 0},
        {49, 131, 17, 0},   {109, 139, 71, 0},  {139, 39, 226, 0},
        {230, 90, 151, 0},  {232, 237, 215, 0}, {127, 242, 248, 0},
        {202, 181, 215, 0}, {52, 220, 166, 0},  {29, 144, 124, 0},
        {125, 237, 13, 0},  {190, 115, 135, 0}, {192, 57, 127, 0},
        {57, 33, 111, 0},   {62, 87, 175, 0},   {46, 73, 248, 0},
        {101, 179, 212, 0}, {186, 243, 111, 0}, {123, 165, 115, 0},
        {92, 86, 217, 0},   {6, 18, 182, 0},    {4, 204, 57, 0},
        {152, 11, 205, 0},  {239, 127, 56, 0},  {15, 45, 141, 0},
        {2, 0, 222, 0},     {101, 253, 206, 0}, {45, 37, 74, 0},
        {152, 30, 232, 0},  {22, 10, 16, 0},    {229, 249, 42, 0},
        {80, 69, 96, 0},    {240, 49, 187, 0},  {81, 81, 239, 0},
        {54, 178, 244, 0},  {100, 159, 34, 0},  {73, 43, 105, 0},
        {217, 177, 211, 0}, {57, 102, 188, 0},  {132, 48, 72, 0},
        {34, 19, 46, 0},    {240, 210, 212, 0}, {187, 139, 121, 0},
        {70, 51, 9, 0},     {123, 149, 2, 0},   {11, 70, 191, 0},
        {39, 193, 154, 0},  {243, 67, 63, 0},   {212, 126, 180, 0},
        {153, 246, 241, 0}, {28, 231, 8, 0},    {36, 31, 157, 0},
        {123, 28, 124, 0},  {182, 87, 95, 0},   {150, 227, 203, 0},
        {141, 181, 102, 0}, {37, 60, 149, 0},   {241, 106, 178, 0},
        {110, 7, 30, 0},    {124, 194, 194, 0}, {194, 243, 216, 0},
        {248, 36, 43, 0},   {10, 134, 25, 0},   {30, 106, 213, 0},
        {80, 213, 173, 0},  {6, 128, 32, 0},    {117, 148, 210, 0},
        {19, 181, 45, 0},   {126, 217, 120, 0}, {105, 49, 187, 0},
        {158, 62, 93, 0},   {248, 36, 30, 0},   {23, 188, 200, 0},
        {251, 123, 1, 0},   {60, 169, 20, 0},   {91, 186, 69, 0},
        {95, 33, 90, 0},    {245, 203, 159, 0}, {153, 152, 163, 0},
        {247, 103, 177, 0}, {229, 43, 38, 0},   {210, 183, 17, 0},
        {197, 29, 172, 0},  {53, 248, 147, 0},  {195, 29, 185, 0},
        {38, 142, 215, 0}};
};

#endif  // MMRPALETTES_H_INCLUDED
