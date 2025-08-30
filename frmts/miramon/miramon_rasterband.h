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
        delete;  // I don't want to assing a MMRRasterBand to another MMRRasterBand (effc++)
    ~MMRRasterBand();

    CPLErr IReadBlock(int, int, void *) override;
    GDALColorInterp GetColorInterpretation() override;
    GDALColorTable *GetColorTable() override;
    double GetMinimum(int *pbSuccess = nullptr) override;
    double GetMaximum(int *pbSuccess = nullptr) override;
    double GetNoDataValue(int *pbSuccess = nullptr) override;
    GDALRasterAttributeTable *GetDefaultRAT() override;

    const std::vector<double> &GetPCT_Red() const
    {
        return aadfPCT[0];
    }

    const std::vector<double> &GetPCT_Green() const
    {
        return aadfPCT[1];
    }

    const std::vector<double> &GetPCT_Blue() const
    {
        return aadfPCT[2];
    }

    const std::vector<double> &GetPCT_Alpha() const
    {
        return aadfPCT[3];
    }

    bool IsValid() const
    {
        return bIsValid;
    }

    bool IsInteger() const
    {
        if (eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_BIT ||
            eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_BYTE ||
            eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_INTEGER ||
            eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_UINTEGER ||
            eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_LONG ||
            eMMRDataTypeMiraMon ==
                MMDataType::DATATYPE_AND_COMPR_INTEGER_ASCII ||
            eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_BYTE_RLE ||
            eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_INTEGER_RLE ||
            eMMRDataTypeMiraMon ==
                MMDataType::DATATYPE_AND_COMPR_UINTEGER_RLE ||
            eMMRDataTypeMiraMon == MMDataType::DATATYPE_AND_COMPR_LONG_RLE)
            return true;
        return false;
    }

  private:
    void AssignRGBColor(int nIndexDstPalete, int nIndexSrcPalete);
    void AssignRGBColorDirectly(int nIndexDstPalete, double dfValue);
    void UpdateDataType();
    CPLErr FillRATFromPalette();
    CPLErr FromPaletteToAttributeTable();
    CPLErr FromPaletteToAttributeTableConstant();
    CPLErr FromPaletteToAttributeTableDirectAssig();
    CPLErr FromPaletteToAttributeTableLinear();
    void ConvertColorsFromPaletteToColorTable();
    CPLErr GetRATName(char *papszToken, CPLString &osRELName,
                      CPLString &osDBFName, CPLString &osAssociateREL);
    CPLErr UpdateAttributeColorsFromPalette();
    CPLErr CreateRATFromDBF(CPLString osRELName, CPLString osDBFName,
                            CPLString osAssociateRel);

    CPLErr AssignUniformColorTable();
    CPLErr FromPaletteToColorTableCategoricalMode();
    CPLErr FromPaletteToColorTableContinousMode();
    CPLErr UpdateTableColorsFromPalette();

    bool bTriedLoadColorTable = false;
    bool bIsValid = false;  // Determines if the created object is valid or not.

    std::array<std::vector<double>, 4> aadfPCT{};

    CPLString osBandSection = "";  // Name of the band

    MMDataType eMMRDataTypeMiraMon = MMDataType::DATATYPE_AND_COMPR_UNDEFINED;
    MMBytesPerPixel eMMBytesPerPixel =
        MMBytesPerPixel::TYPE_BYTES_PER_PIXEL_UNDEFINED;

    MMRRel *pfRel = nullptr;  // Pointer to info from rel. Do not free.

    // Color table
    GDALColorTable *poCT = nullptr;

    // Attributte table
    GDALRasterAttributeTable *poDefaultRAT = nullptr;

    // Palettes
    MMRPalettes *Palette = nullptr;
};

#endif  // MMRRASTERBAND_H_INCLUDED
