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
    MMRPalettes(MMRRel &fRel, int nIBand);
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
    CPLErr GetPaletteColors_Automatic();
    CPLErr GetPaletteColors_DBF(const CPLString &os_Color_Paleta_DBF);
    CPLErr GetPaletteColors_PAL_P25_P65(const CPLString &os_Color_Paleta_DBF);
    void AssignColorFromDBF(struct MM_DATA_BASE_XP &oColorTable,
                            char *pzsRecord, char *pszField,
                            MM_EXT_DBF_N_FIELDS &nRIndex,
                            MM_EXT_DBF_N_FIELDS &nGIndex,
                            MM_EXT_DBF_N_FIELDS &nBIndex, int nIPaletteIndex);

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
    CPLString m_osBandSection = "";

    bool m_bIsValid =
        false;  // Determines if the created object is valid or not.

    // To be used in a categorical raster with Automatic palette associated
    const std::vector<GDALColorEntry> m_ThematicPalette = {
        {0, 0, 255, 255},     {0, 255, 255, 255},   {0, 255, 0, 255},
        {255, 255, 0, 255},   {255, 0, 0, 255},     {255, 0, 255, 255},
        {191, 191, 191, 255}, {0, 128, 255, 255},   {128, 0, 255, 255},
        {0, 255, 128, 255},   {128, 255, 0, 255},   {255, 128, 0, 255},
        {255, 0, 128, 255},   {128, 255, 255, 255}, {128, 128, 255, 255},
        {128, 255, 128, 255}, {255, 128, 255, 255}, {255, 128, 128, 255},
        {255, 255, 128, 255}, {128, 128, 128, 255}, {0, 0, 128, 255},
        {0, 128, 128, 255},   {0, 128, 0, 255},     {128, 128, 0, 255},
        {128, 0, 0, 255},     {128, 0, 128, 255},   {64, 64, 64, 255},
        {0, 0, 191, 255},     {128, 128, 191, 255}, {0, 191, 191, 255},
        {0, 191, 0, 255},     {191, 191, 0, 255},   {191, 0, 0, 255},
        {191, 0, 191, 255},   {0, 128, 191, 255},   {128, 0, 191, 255},
        {128, 191, 191, 255}, {0, 191, 128, 255},   {128, 191, 0, 255},
        {128, 191, 128, 255}, {191, 191, 128, 255}, {191, 128, 0, 255},
        {191, 128, 128, 255}, {191, 0, 128, 255},   {191, 128, 191, 255},
        {0, 0, 64, 255},      {0, 64, 64, 255},     {0, 64, 0, 255},
        {64, 64, 0, 255},     {64, 0, 0, 255},      {64, 0, 64, 255},
        {0, 128, 64, 255},    {128, 0, 64, 255},    {0, 64, 128, 255},
        {128, 64, 0, 255},    {64, 128, 0, 255},    {64, 0, 128, 255},
        {128, 64, 64, 255},   {128, 128, 64, 255},  {128, 64, 128, 255},
        {64, 128, 64, 255},   {64, 128, 128, 255},  {64, 64, 128, 255},
        {0, 191, 64, 255},    {191, 0, 64, 255},    {0, 64, 191, 255},
        {191, 64, 0, 255},    {64, 191, 0, 255},    {64, 0, 191, 255},
        {191, 64, 64, 255},   {191, 191, 64, 255},  {191, 64, 191, 255},
        {64, 191, 64, 255},   {64, 191, 191, 255},  {64, 64, 191, 255},
        {15, 177, 228, 255},  {184, 91, 96, 255},   {105, 246, 240, 255},
        {139, 224, 27, 255},  {113, 111, 125, 255}, {188, 184, 147, 255},
        {125, 225, 235, 255}, {78, 166, 108, 255},  {185, 87, 250, 255},
        {171, 224, 154, 255}, {60, 25, 133, 255},   {227, 239, 158, 255},
        {140, 139, 108, 255}, {101, 195, 115, 255}, {67, 245, 217, 255},
        {150, 123, 223, 255}, {71, 86, 92, 255},    {206, 18, 20, 255},
        {255, 99, 85, 255},   {233, 235, 42, 255},  {254, 235, 235, 255},
        {18, 82, 160, 255},   {43, 82, 250, 255},   {33, 5, 223, 255},
        {132, 212, 136, 255}, {166, 250, 155, 255}, {95, 116, 2, 255},
        {249, 5, 22, 255},    {5, 221, 152, 255},   {56, 5, 194, 255},
        {6, 243, 169, 255},   {29, 149, 23, 255},   {87, 85, 251, 255},
        {128, 200, 197, 255}, {73, 120, 48, 255},   {211, 29, 1, 255},
        {97, 13, 26, 255},    {201, 31, 248, 255},  {163, 224, 32, 255},
        {46, 82, 238, 255},   {212, 53, 216, 255},  {101, 255, 186, 255},
        {205, 131, 99, 255},  {49, 191, 141, 255},  {23, 115, 53, 255},
        {11, 97, 56, 255},    {108, 208, 111, 255}, {181, 80, 251, 255},
        {53, 14, 0, 255},     {205, 190, 17, 255},  {79, 221, 250, 255},
        {40, 182, 251, 255},  {227, 91, 248, 255},  {119, 235, 88, 255},
        {93, 224, 88, 255},   {149, 185, 129, 255}, {245, 143, 30, 255},
        {23, 219, 5, 255},    {211, 59, 65, 255},   {31, 125, 29, 255},
        {49, 251, 93, 255},   {78, 112, 183, 255},  {142, 195, 201, 255},
        {206, 74, 49, 255},   {45, 221, 241, 255},  {61, 28, 13, 255},
        {139, 41, 68, 255},   {178, 130, 74, 255},  {140, 229, 251, 255},
        {119, 165, 107, 255}, {53, 175, 23, 255},   {100, 38, 228, 255},
        {111, 88, 65, 255},   {196, 157, 233, 255}, {131, 162, 134, 255},
        {58, 171, 196, 255},  {115, 116, 93, 255},  {159, 232, 239, 255},
        {217, 200, 153, 255}, {171, 59, 69, 255},   {73, 206, 236, 255},
        {11, 171, 170, 255},  {101, 142, 165, 255}, {156, 147, 175, 255},
        {156, 199, 79, 255},  {212, 47, 90, 255},   {65, 2, 123, 255},
        {120, 20, 65, 255},   {153, 51, 45, 255},   {248, 171, 167, 255},
        {59, 143, 51, 255},   {137, 68, 226, 255},  {161, 30, 43, 255},
        {96, 97, 26, 255},    {155, 184, 199, 255}, {105, 53, 146, 255},
        {49, 131, 17, 255},   {109, 139, 71, 255},  {139, 39, 226, 255},
        {230, 90, 151, 255},  {232, 237, 215, 255}, {127, 242, 248, 255},
        {202, 181, 215, 255}, {52, 220, 166, 255},  {29, 144, 124, 255},
        {125, 237, 13, 255},  {190, 115, 135, 255}, {192, 57, 127, 255},
        {57, 33, 111, 255},   {62, 87, 175, 255},   {46, 73, 248, 255},
        {101, 179, 212, 255}, {186, 243, 111, 255}, {123, 165, 115, 255},
        {92, 86, 217, 255},   {6, 18, 182, 255},    {4, 204, 57, 255},
        {152, 11, 205, 255},  {239, 127, 56, 255},  {15, 45, 141, 255},
        {2, 0, 222, 255},     {101, 253, 206, 255}, {45, 37, 74, 255},
        {152, 30, 232, 255},  {22, 10, 16, 255},    {229, 249, 42, 255},
        {80, 69, 96, 255},    {240, 49, 187, 255},  {81, 81, 239, 255},
        {54, 178, 244, 255},  {100, 159, 34, 255},  {73, 43, 105, 255},
        {217, 177, 211, 255}, {57, 102, 188, 255},  {132, 48, 72, 255},
        {34, 19, 46, 255},    {240, 210, 212, 255}, {187, 139, 121, 255},
        {70, 51, 9, 255},     {123, 149, 2, 255},   {11, 70, 191, 255},
        {39, 193, 154, 255},  {243, 67, 63, 255},   {212, 126, 180, 255},
        {153, 246, 241, 255}, {28, 231, 8, 255},    {36, 31, 157, 255},
        {123, 28, 124, 255},  {182, 87, 95, 255},   {150, 227, 203, 255},
        {141, 181, 102, 255}, {37, 60, 149, 255},   {241, 106, 178, 255},
        {110, 7, 30, 255},    {124, 194, 194, 255}, {194, 243, 216, 255},
        {248, 36, 43, 255},   {10, 134, 25, 255},   {30, 106, 213, 255},
        {80, 213, 173, 255},  {6, 128, 32, 255},    {117, 148, 210, 255},
        {19, 181, 45, 255},   {126, 217, 120, 255}, {105, 49, 187, 255},
        {158, 62, 93, 255},   {248, 36, 30, 255},   {23, 188, 200, 255},
        {251, 123, 1, 255},   {60, 169, 20, 255},   {91, 186, 69, 255},
        {95, 33, 90, 255},    {245, 203, 159, 255}, {153, 152, 163, 255},
        {247, 103, 177, 255}, {229, 43, 38, 255},   {210, 183, 17, 255},
        {197, 29, 172, 255},  {53, 248, 147, 255},  {195, 29, 185, 255},
        {38, 142, 215, 255}};
};

#endif  // MMRPALETTES_H_INCLUDED
