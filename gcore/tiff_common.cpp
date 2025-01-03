/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Common code shared between the GTiff and libertiff drivers
 * Author:   Even Rouault <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2024, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "tiff_common.h"

#include <algorithm>

#include "gdal_mdreader.h"

namespace gdal
{
namespace tiff_common
{

/************************************************************************/
/*                       PrepareTIFFErrorFormat()                       */
/*                                                                      */
/*      sometimes the "module" has stuff in it that has special         */
/*      meaning in a printf() style format, so we try to escape it.     */
/*      For now we hope the only thing we have to escape is %'s.        */
/************************************************************************/

char *PrepareTIFFErrorFormat(const char *module, const char *fmt)

{
    const size_t nModuleSize = strlen(module);
    const size_t nModFmtSize = nModuleSize * 2 + strlen(fmt) + 2;
    char *pszModFmt = static_cast<char *>(CPLMalloc(nModFmtSize));

    size_t iOut = 0;  // Used after for.

    for (size_t iIn = 0; iIn < nModuleSize; ++iIn)
    {
        if (module[iIn] == '%')
        {
            CPLAssert(iOut < nModFmtSize - 2);
            pszModFmt[iOut++] = '%';
            pszModFmt[iOut++] = '%';
        }
        else
        {
            CPLAssert(iOut < nModFmtSize - 1);
            pszModFmt[iOut++] = module[iIn];
        }
    }
    CPLAssert(iOut < nModFmtSize);
    pszModFmt[iOut] = '\0';
    strcat(pszModFmt, ":");
    strcat(pszModFmt, fmt);

    return pszModFmt;
}

/************************************************************************/
/*                     TIFFColorMapTagToColorTable()                    */
/************************************************************************/

std::unique_ptr<GDALColorTable> TIFFColorMapTagToColorTable(
    const unsigned short *panRed, const unsigned short *panGreen,
    const unsigned short *panBlue, int nColorCount, int &nColorTableMultiplier,
    int nDefaultColorTableMultiplier, bool bNoDataSet, double dfNoDataValue)
{
    auto poColorTable = std::make_unique<GDALColorTable>();

    if (nColorTableMultiplier == 0)
    {
        // TIFF color maps are in the [0, 65535] range, so some remapping must
        // be done to get values in the [0, 255] range, but it is not clear
        // how to do that exactly. Since GDAL 2.3.0 we have standardized on
        // using a 257 multiplication factor (https://github.com/OSGeo/gdal/commit/eeec5b62e385d53e7f2edaba7b73c7c74bc2af39)
        // but other software uses 256 (cf https://github.com/OSGeo/gdal/issues/10310)
        // Do a first pass to check if all values are multiples of 256 or 257.
        bool bFoundNonZeroEntry = false;
        bool bAllValuesMultipleOf256 = true;
        bool bAllValuesMultipleOf257 = true;
        unsigned short nMaxColor = 0;
        for (int iColor = 0; iColor < nColorCount; ++iColor)
        {
            if (panRed[iColor] > 0 || panGreen[iColor] > 0 ||
                panBlue[iColor] > 0)
            {
                bFoundNonZeroEntry = true;
            }
            if ((panRed[iColor] % 256) != 0 || (panGreen[iColor] % 256) != 0 ||
                (panBlue[iColor] % 256) != 0)
            {
                bAllValuesMultipleOf256 = false;
            }
            if ((panRed[iColor] % 257) != 0 || (panGreen[iColor] % 257) != 0 ||
                (panBlue[iColor] % 257) != 0)
            {
                bAllValuesMultipleOf257 = false;
            }

            nMaxColor = std::max(nMaxColor, panRed[iColor]);
            nMaxColor = std::max(nMaxColor, panGreen[iColor]);
            nMaxColor = std::max(nMaxColor, panBlue[iColor]);
        }

        if (nMaxColor > 0 && nMaxColor < 256)
        {
            // Bug 1384 - Some TIFF files are generated with color map entry
            // values in range 0-255 instead of 0-65535 - try to handle these
            // gracefully.
            nColorTableMultiplier = 1;
            CPLDebug("GTiff",
                     "TIFF ColorTable seems to be improperly scaled with "
                     "values all in [0,255] range, fixing up.");
        }
        else
        {
            if (!bAllValuesMultipleOf256 && !bAllValuesMultipleOf257)
            {
                CPLDebug("GTiff",
                         "The color map contains entries which are not "
                         "multiple of 256 or 257, so we don't know for "
                         "sure how to remap them to [0, 255]. Default to "
                         "using a 257 multiplication factor");
            }
            nColorTableMultiplier =
                (bFoundNonZeroEntry && bAllValuesMultipleOf256)
                    ? 256
                    : nDefaultColorTableMultiplier;
        }
    }
    CPLAssert(nColorTableMultiplier > 0);
    CPLAssert(nColorTableMultiplier <= 257);
    for (int iColor = nColorCount - 1; iColor >= 0; iColor--)
    {
        const GDALColorEntry oEntry = {
            static_cast<short>(panRed[iColor] / nColorTableMultiplier),
            static_cast<short>(panGreen[iColor] / nColorTableMultiplier),
            static_cast<short>(panBlue[iColor] / nColorTableMultiplier),
            static_cast<short>(
                bNoDataSet && static_cast<int>(dfNoDataValue) == iColor ? 0
                                                                        : 255)};

        poColorTable->SetColorEntry(iColor, &oEntry);
    }

    return poColorTable;
}

/************************************************************************/
/*                      TIFFRPCTagToRPCMetadata()                       */
/************************************************************************/

CPLStringList TIFFRPCTagToRPCMetadata(const double adfRPC[92])
{
    CPLStringList asMD;
    asMD.SetNameValue(RPC_ERR_BIAS, CPLOPrintf("%.15g", adfRPC[0]));
    asMD.SetNameValue(RPC_ERR_RAND, CPLOPrintf("%.15g", adfRPC[1]));
    asMD.SetNameValue(RPC_LINE_OFF, CPLOPrintf("%.15g", adfRPC[2]));
    asMD.SetNameValue(RPC_SAMP_OFF, CPLOPrintf("%.15g", adfRPC[3]));
    asMD.SetNameValue(RPC_LAT_OFF, CPLOPrintf("%.15g", adfRPC[4]));
    asMD.SetNameValue(RPC_LONG_OFF, CPLOPrintf("%.15g", adfRPC[5]));
    asMD.SetNameValue(RPC_HEIGHT_OFF, CPLOPrintf("%.15g", adfRPC[6]));
    asMD.SetNameValue(RPC_LINE_SCALE, CPLOPrintf("%.15g", adfRPC[7]));
    asMD.SetNameValue(RPC_SAMP_SCALE, CPLOPrintf("%.15g", adfRPC[8]));
    asMD.SetNameValue(RPC_LAT_SCALE, CPLOPrintf("%.15g", adfRPC[9]));
    asMD.SetNameValue(RPC_LONG_SCALE, CPLOPrintf("%.15g", adfRPC[10]));
    asMD.SetNameValue(RPC_HEIGHT_SCALE, CPLOPrintf("%.15g", adfRPC[11]));

    CPLString osField;
    CPLString osMultiField;

    for (int i = 0; i < 20; ++i)
    {
        osField.Printf("%.15g", adfRPC[12 + i]);
        if (i > 0)
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    asMD.SetNameValue(RPC_LINE_NUM_COEFF, osMultiField);

    for (int i = 0; i < 20; ++i)
    {
        osField.Printf("%.15g", adfRPC[32 + i]);
        if (i > 0)
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    asMD.SetNameValue(RPC_LINE_DEN_COEFF, osMultiField);

    for (int i = 0; i < 20; ++i)
    {
        osField.Printf("%.15g", adfRPC[52 + i]);
        if (i > 0)
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    asMD.SetNameValue(RPC_SAMP_NUM_COEFF, osMultiField);

    for (int i = 0; i < 20; ++i)
    {
        osField.Printf("%.15g", adfRPC[72 + i]);
        if (i > 0)
            osMultiField += " ";
        else
            osMultiField = "";
        osMultiField += osField;
    }
    asMD.SetNameValue(RPC_SAMP_DEN_COEFF, osMultiField);
    return asMD;
}

}  // namespace tiff_common
}  // namespace gdal
