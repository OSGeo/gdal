/******************************************************************************
 *
 * Project:  MiraMon Raster Driver
 * Purpose:  Implements MMRRasterBand class: responsible for converting the
 *           information stored in an MMRBand into a GDAL RasterBand
 * Author:   Abel Pau
 *
 ******************************************************************************
 * Copyright (c) 2025, Xavier Pons
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MMRRASTERBAND_H_INCLUDED
#define MMRRASTERBAND_H_INCLUDED

#include <cstddef>
#include <vector>
#include <optional>
#include <array>

#include "gdal_pam.h"
#include "gdal_rat.h"

#include "../miramon_common/mm_gdal_constants.h"  // For MM_EXT_DBF_N_FIELDS
#include "miramon_rel.h"                          // For MMDataType
#include "miramon_palettes.h"

class MMRDataset;

/* ==================================================================== */
/*                            MMRRasterBand                             */
/* ==================================================================== */
class MMRRasterBand final : public GDALPamRasterBand
{
  public:
    MMRRasterBand(MMRDataset *, int);

    MMRRasterBand(const MMRRasterBand &) =
        delete;  // I don't want to construct a MMRRasterBand from another MMRRasterBand (effc++)
    MMRRasterBand &operator=(const MMRRasterBand &) =
        delete;  // I don't want to assign a MMRRasterBand to another MMRRasterBand (effc++)
    ~MMRRasterBand() override;

    CPLErr IReadBlock(int, int, void *) override;
    GDALColorInterp GetColorInterpretation() override;
    GDALColorTable *GetColorTable() override;
    double GetMinimum(int *pbSuccess = nullptr) override;
    double GetMaximum(int *pbSuccess = nullptr) override;
    const char *GetUnitType() override;
    CPLErr SetUnitType(const char *pszNewValue) override;
    double GetNoDataValue(int *pbSuccess = nullptr) override;
    GDALRasterAttributeTable *GetDefaultRAT() override;

    const std::vector<double> &GetPCT_Red() const
    {
        return m_aadfPCT[0];
    }

    const std::vector<double> &GetPCT_Green() const
    {
        return m_aadfPCT[1];
    }

    const std::vector<double> &GetPCT_Blue() const
    {
        return m_aadfPCT[2];
    }

    const std::vector<double> &GetPCT_Alpha() const
    {
        return m_aadfPCT[3];
    }

    bool IsValid() const
    {
        return m_bIsValid;
    }

    bool IsInteger() const
    {
        if (m_eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_BIT ||
            m_eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_BYTE ||
            m_eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_INTEGER ||
            m_eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_UINTEGER ||
            m_eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_LONG ||
            m_eMMRDataTypeMiraMon ==
                MMDataType::DATATYPE_AND_COMPR_INTEGER_ASCII ||
            m_eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_BYTE_RLE ||
            m_eMMRDataTypeMiraMon ==
                MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE ||
            m_eMMRDataTypeMiraMon ==
                MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE ||
            m_eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_LONG_RLE)
            return true;
        return false;
    }

  private:
    void AssignRGBColor(int nIndexDstPalette, int nIndexSrcPalette);
    void AssignRGBColorDirectly(int nIndexDstPalette, double dfValue);
    void UpdateDataType();
    CPLErr FillRATFromPalette();
    CPLErr FromPaletteToAttributeTable();
    CPLErr FromPaletteToAttributeTableConstant();
    CPLErr FromPaletteToAttributeTableDirectAssig();
    CPLErr FromPaletteToAttributeTableLinear();
    void ConvertColorsFromPaletteToColorTable();
    CPLErr GetRATName(CPLString &osRELName, CPLString &osDBFName,
                      CPLString &osAssociateREL);
    CPLErr UpdateAttributeColorsFromPalette();
    CPLErr CreateRATFromDBF(const CPLString &osRELName,
                            const CPLString &osDBFName,
                            const CPLString &osAssociateRel);

    CPLErr AssignUniformColorTable();
    CPLErr FromPaletteToColorTableCategoricalMode();
    CPLErr FromPaletteToColorTableContinuousMode();
    CPLErr UpdateTableColorsFromPalette();

    bool m_bTriedLoadColorTable = false;
    bool m_bIsValid =
        false;  // Determines if the created object is valid or not.

    RAT_OR_CT nRatOrCT = RAT_OR_CT::ALL;

    std::array<std::vector<double>, 4> m_aadfPCT{};

    // Name of the band
    CPLString m_osBandSection = "";

    CPLString m_osUnitType = "";

    MMDataType m_eMMRDataTypeMiraMon = MMDataType::DATATYPE_AND_COMPR_UNDEFINED;
    MMBytesPerPixel m_eMMBytesPerPixel =
        MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_UNDEFINED;

    MMRRel *m_pfRel = nullptr;  // Pointer to info from rel. Do not free.

    // Color table
    std::unique_ptr<GDALColorTable> m_poCT = nullptr;

    // Attributte table
    std::unique_ptr<GDALRasterAttributeTable> m_poDefaultRAT = nullptr;

    // Palettes
    std::unique_ptr<MMRPalettes> m_Palette = nullptr;
};

#endif  // MMRRASTERBAND_H_INCLUDED
