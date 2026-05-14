/******************************************************************************
 *
 * Project:  GDAL Core
 * Purpose:  Color table implementation.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 **********************************************************************
 * Copyright (c) 2000, Frank Warmerdam
 * Copyright (c) 2009, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "gdal_priv.h"

#include <algorithm>
#include <cstring>
#include <exception>
#include <memory>
#include <vector>

#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_vsi_virtual.h"
#include "gdal.h"

/************************************************************************/
/*                           GDALColorTable()                           */
/************************************************************************/

/**
 * \brief Construct a new color table.
 *
 * This constructor is the same as the C GDALCreateColorTable() function.
 *
 * @param eInterpIn the interpretation to be applied to GDALColorEntry
 * values.
 */

GDALColorTable::GDALColorTable(GDALPaletteInterp eInterpIn) : eInterp(eInterpIn)
{
}

/************************************************************************/
/*                        GDALCreateColorTable()                        */
/************************************************************************/

/**
 * \brief Construct a new color table.
 *
 * This function is the same as the C++ method GDALColorTable::GDALColorTable()
 */
GDALColorTableH CPL_STDCALL GDALCreateColorTable(GDALPaletteInterp eInterp)

{
    return GDALColorTable::ToHandle(new GDALColorTable(eInterp));
}

/************************************************************************/
/*                          ~GDALColorTable()                           */
/************************************************************************/

/**
 * \brief Destructor.
 *
 * This destructor is the same as the C GDALDestroyColorTable() function.
 */

GDALColorTable::~GDALColorTable() = default;

/************************************************************************/
/*                       GDALDestroyColorTable()                        */
/************************************************************************/

/**
 * \brief Destroys a color table.
 *
 * This function is the same as the C++ method GDALColorTable::~GDALColorTable()
 */
void CPL_STDCALL GDALDestroyColorTable(GDALColorTableH hTable)

{
    delete GDALColorTable::FromHandle(hTable);
}

/************************************************************************/
/*                           GetColorEntry()                            */
/************************************************************************/

/**
 * \brief Fetch a color entry from table.
 *
 * This method is the same as the C function GDALGetColorEntry().
 *
 * @param i entry offset from zero to GetColorEntryCount()-1.
 *
 * @return pointer to internal color entry, or NULL if index is out of range.
 */

const GDALColorEntry *GDALColorTable::GetColorEntry(int i) const

{
    if (i < 0 || i >= static_cast<int>(aoEntries.size()))
        return nullptr;

    return &aoEntries[i];
}

/************************************************************************/
/*                         GDALGetColorEntry()                          */
/************************************************************************/

/**
 * \brief Fetch a color entry from table.
 *
 * This function is the same as the C++ method GDALColorTable::GetColorEntry()
 */
const GDALColorEntry *CPL_STDCALL GDALGetColorEntry(GDALColorTableH hTable,
                                                    int i)

{
    VALIDATE_POINTER1(hTable, "GDALGetColorEntry", nullptr);

    return GDALColorTable::FromHandle(hTable)->GetColorEntry(i);
}

/************************************************************************/
/*                         GetColorEntryAsRGB()                         */
/************************************************************************/

/**
 * \brief Fetch a table entry in RGB format.
 *
 * In theory this method should support translation of color palettes in
 * non-RGB color spaces into RGB on the fly, but currently it only works
 * on RGB color tables.
 *
 * This method is the same as the C function GDALGetColorEntryAsRGB().
 *
 * @param i entry offset from zero to GetColorEntryCount()-1.
 *
 * @param poEntry the existing GDALColorEntry to be overwritten with the RGB
 * values.
 *
 * @return TRUE on success, or FALSE if the conversion isn't supported.
 */

int GDALColorTable::GetColorEntryAsRGB(int i, GDALColorEntry *poEntry) const

{
    if (eInterp != GPI_RGB || i < 0 || i >= static_cast<int>(aoEntries.size()))
        return FALSE;

    *poEntry = aoEntries[i];
    return TRUE;
}

/************************************************************************/
/*                       GDALGetColorEntryAsRGB()                       */
/************************************************************************/

/**
 * \brief Fetch a table entry in RGB format.
 *
 * This function is the same as the C++ method
 * GDALColorTable::GetColorEntryAsRGB().
 */
int CPL_STDCALL GDALGetColorEntryAsRGB(GDALColorTableH hTable, int i,
                                       GDALColorEntry *poEntry)

{
    VALIDATE_POINTER1(hTable, "GDALGetColorEntryAsRGB", 0);
    VALIDATE_POINTER1(poEntry, "GDALGetColorEntryAsRGB", 0);

    return GDALColorTable::FromHandle(hTable)->GetColorEntryAsRGB(i, poEntry);
}

/************************************************************************/
/*                           SetColorEntry()                            */
/************************************************************************/

/**
 * \brief Set entry in color table.
 *
 * Note that the passed in color entry is copied, and no internal reference
 * to it is maintained.  Also, the passed in entry must match the color
 * interpretation of the table to which it is being assigned.
 *
 * The table is grown as needed to hold the supplied offset.
 *
 * This function is the same as the C function GDALSetColorEntry().
 *
 * @param i entry offset from zero to GetColorEntryCount()-1.
 * @param poEntry value to assign to table.
 */

void GDALColorTable::SetColorEntry(int i, const GDALColorEntry *poEntry)

{
    if (i < 0)
        return;

    try
    {
        if (i >= static_cast<int>(aoEntries.size()))
        {
            GDALColorEntry oBlack = {0, 0, 0, 0};
            aoEntries.resize(i + 1, oBlack);
        }

        aoEntries[i] = *poEntry;
    }
    catch (std::exception &e)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", e.what());
    }
}

/************************************************************************/
/*                         GDALSetColorEntry()                          */
/************************************************************************/

/**
 * \brief Set entry in color table.
 *
 * This function is the same as the C++ method GDALColorTable::SetColorEntry()
 */
void CPL_STDCALL GDALSetColorEntry(GDALColorTableH hTable, int i,
                                   const GDALColorEntry *poEntry)

{
    VALIDATE_POINTER0(hTable, "GDALSetColorEntry");
    VALIDATE_POINTER0(poEntry, "GDALSetColorEntry");

    GDALColorTable::FromHandle(hTable)->SetColorEntry(i, poEntry);
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

/**
 * \brief Make a copy of a color table.
 *
 * This method is the same as the C function GDALCloneColorTable().
 */

GDALColorTable *GDALColorTable::Clone() const

{
    return new GDALColorTable(*this);
}

/************************************************************************/
/*                        GDALCloneColorTable()                         */
/************************************************************************/

/**
 * \brief Make a copy of a color table.
 *
 * This function is the same as the C++ method GDALColorTable::Clone()
 */
GDALColorTableH CPL_STDCALL GDALCloneColorTable(GDALColorTableH hTable)

{
    VALIDATE_POINTER1(hTable, "GDALCloneColorTable", nullptr);

    return GDALColorTable::ToHandle(
        GDALColorTable::FromHandle(hTable)->Clone());
}

/************************************************************************/
/*                         GetColorEntryCount()                         */
/************************************************************************/

/**
 * \brief Get number of color entries in table.
 *
 * This method is the same as the function GDALGetColorEntryCount().
 *
 * @return the number of color entries.
 */

int GDALColorTable::GetColorEntryCount() const

{
    return static_cast<int>(aoEntries.size());
}

/************************************************************************/
/*                       GDALGetColorEntryCount()                       */
/************************************************************************/

/**
 * \brief Get number of color entries in table.
 *
 * This function is the same as the C++ method
 * GDALColorTable::GetColorEntryCount()
 */
int CPL_STDCALL GDALGetColorEntryCount(GDALColorTableH hTable)

{
    VALIDATE_POINTER1(hTable, "GDALGetColorEntryCount", 0);

    return GDALColorTable::FromHandle(hTable)->GetColorEntryCount();
}

/************************************************************************/
/*                      GetPaletteInterpretation()                      */
/************************************************************************/

/**
 * \brief Fetch palette interpretation.
 *
 * The returned value is used to interpret the values in the GDALColorEntry.
 *
 * This method is the same as the C function GDALGetPaletteInterpretation().
 *
 * @return palette interpretation enumeration value, usually GPI_RGB.
 */

GDALPaletteInterp GDALColorTable::GetPaletteInterpretation() const

{
    return eInterp;
}

/************************************************************************/
/*                    GDALGetPaletteInterpretation()                    */
/************************************************************************/

/**
 * \brief Fetch palette interpretation.
 *
 * This function is the same as the C++ method
 * GDALColorTable::GetPaletteInterpretation()
 */
GDALPaletteInterp CPL_STDCALL
GDALGetPaletteInterpretation(GDALColorTableH hTable)

{
    VALIDATE_POINTER1(hTable, "GDALGetPaletteInterpretation", GPI_Gray);

    return GDALColorTable::FromHandle(hTable)->GetPaletteInterpretation();
}

/**
 * \brief Create color ramp
 *
 * Automatically creates a color ramp from one color entry to
 * another. It can be called several times to create multiples ramps
 * in the same color table.
 *
 * This function is the same as the C function GDALCreateColorRamp().
 *
 * @param nStartIndex index to start the ramp on the color table [0..255]
 * @param psStartColor a color entry value to start the ramp
 * @param nEndIndex index to end the ramp on the color table [0..255]
 * @param psEndColor a color entry value to end the ramp
 * @return total number of entries, -1 to report error
 */

int GDALColorTable::CreateColorRamp(int nStartIndex,
                                    const GDALColorEntry *psStartColor,
                                    int nEndIndex,
                                    const GDALColorEntry *psEndColor)
{
    // Validate indexes.
    if (nStartIndex < 0 || nStartIndex > 255 || nEndIndex < 0 ||
        nEndIndex > 255 || nStartIndex > nEndIndex)
    {
        return -1;
    }

    // Validate color entries.
    if (psStartColor == nullptr || psEndColor == nullptr)
    {
        return -1;
    }

    // Calculate number of colors in-between + 1.
    const int nColors = nEndIndex - nStartIndex;

    // Set starting color.
    SetColorEntry(nStartIndex, psStartColor);

    if (nColors == 0)
    {
        return GetColorEntryCount();  // Only one color.  No ramp to create.
    }

    // Set ending color.
    SetColorEntry(nEndIndex, psEndColor);

    // Calculate the slope of the linear transformation.
    const double dfColors = static_cast<double>(nColors);
    const double dfSlope1 = (psEndColor->c1 - psStartColor->c1) / dfColors;
    const double dfSlope2 = (psEndColor->c2 - psStartColor->c2) / dfColors;
    const double dfSlope3 = (psEndColor->c3 - psStartColor->c3) / dfColors;
    const double dfSlope4 = (psEndColor->c4 - psStartColor->c4) / dfColors;

    // Loop through the new colors.
    GDALColorEntry sColor = *psStartColor;

    for (int i = 1; i < nColors; i++)
    {
        sColor.c1 = static_cast<short>(i * dfSlope1 + psStartColor->c1);
        sColor.c2 = static_cast<short>(i * dfSlope2 + psStartColor->c2);
        sColor.c3 = static_cast<short>(i * dfSlope3 + psStartColor->c3);
        sColor.c4 = static_cast<short>(i * dfSlope4 + psStartColor->c4);

        SetColorEntry(nStartIndex + i, &sColor);
    }

    // Return the total number of colors.
    return GetColorEntryCount();
}

/************************************************************************/
/*                        GDALCreateColorRamp()                         */
/************************************************************************/

/**
 * \brief Create color ramp
 *
 * This function is the same as the C++ method GDALColorTable::CreateColorRamp()
 */
void CPL_STDCALL GDALCreateColorRamp(GDALColorTableH hTable, int nStartIndex,
                                     const GDALColorEntry *psStartColor,
                                     int nEndIndex,
                                     const GDALColorEntry *psEndColor)
{
    VALIDATE_POINTER0(hTable, "GDALCreateColorRamp");

    GDALColorTable::FromHandle(hTable)->CreateColorRamp(
        nStartIndex, psStartColor, nEndIndex, psEndColor);
}

/************************************************************************/
/*                               IsSame()                               */
/************************************************************************/

/**
 * \brief Returns if the current color table is the same as another one.
 *
 * @param poOtherCT other color table to be compared to.
 * @return TRUE if both color tables are identical.
 */

int GDALColorTable::IsSame(const GDALColorTable *poOtherCT) const
{
    return aoEntries.size() == poOtherCT->aoEntries.size() &&
           (aoEntries.empty() ||
            memcmp(&aoEntries[0], &poOtherCT->aoEntries[0],
                   aoEntries.size() * sizeof(GDALColorEntry)) == 0);
}

/************************************************************************/
/*                             IsIdentity()                             */
/************************************************************************/

/**
 * \brief Returns if the current color table is the identity, that is
 * for each index i, colortable[i].c1 = .c2 = .c3 = i and .c4 = 255
 *
 * @since GDAL 3.4.1
 */

bool GDALColorTable::IsIdentity() const
{
    for (int i = 0; i < static_cast<int>(aoEntries.size()); ++i)
    {
        if (aoEntries[i].c1 != i || aoEntries[i].c2 != i ||
            aoEntries[i].c3 != i || aoEntries[i].c4 != 255)
        {
            return false;
        }
    }
    return true;
}

/************************************************************************/
/*                         FindRasterRenderer()                         */
/************************************************************************/

static bool FindRasterRenderer(const CPLXMLNode *const psNode,
                               bool bVisitSibblings, const CPLXMLNode *&psRet)
{
    bool bRet = true;

    if (psNode->eType == CXT_Element &&
        strcmp(psNode->pszValue, "rasterrenderer") == 0)
    {
        const char *pszType = CPLGetXMLValue(psNode, "type", "");
        if (strcmp(pszType, "paletted") == 0 ||
            strcmp(pszType, "singlebandpseudocolor") == 0)
        {
            bRet = psRet == nullptr;
            if (bRet)
            {
                psRet = psNode;
            }
        }
    }

    for (const CPLXMLNode *psIter = psNode->psChild; bRet && psIter;
         psIter = psIter->psNext)
    {
        if (psIter->eType == CXT_Element)
            bRet = FindRasterRenderer(psIter, false, psRet);
    }

    if (bVisitSibblings)
    {
        for (const CPLXMLNode *psIter = psNode->psNext; bRet && psIter;
             psIter = psIter->psNext)
        {
            if (psIter->eType == CXT_Element)
                bRet = FindRasterRenderer(psIter, false, psRet);
        }
    }

    return bRet;
}

static const CPLXMLNode *FindRasterRenderer(const CPLXMLNode *psNode)
{
    const CPLXMLNode *psRet = nullptr;
    if (!FindRasterRenderer(psNode, true, psRet))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Several raster renderers with color tables found");
        return nullptr;
    }
    if (!psRet)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "No color table found");
        return nullptr;
    }
    return psRet;
}

/************************************************************************/
/*                            LoadFromFile()                            */
/************************************************************************/

/**
 * \brief Load a color table from a (text) file.
 *
 * Supported formats are:
 * - QGIS Layer Style File (.qml) or QGIS Layer Definition File (.qlr) using
 *   "Palette/unique values" raster renderer or "Single band pseudocolor" renderer
 * - GMT or GRASS text files, when entry index are integers
 *
 * @return a new color table, or NULL in case of error.
 * @since GDAL 3.12
 */

/* static */
std::unique_ptr<GDALColorTable>
GDALColorTable::LoadFromFile(const char *pszFilename)
{
    const std::string osExt = CPLGetExtensionSafe(pszFilename);
    auto poCT = std::make_unique<GDALColorTable>();
    if (EQUAL(osExt.c_str(), "qlr") || EQUAL(osExt.c_str(), "qml"))
    {
        GByte *pabyData = nullptr;
        if (!VSIIngestFile(nullptr, pszFilename, &pabyData, nullptr,
                           10 * 1024 * 1024))
            return nullptr;
        CPLXMLTreeCloser oTree(
            CPLParseXMLString(reinterpret_cast<const char *>(pabyData)));
        CPLFree(pabyData);
        if (!oTree)
            return nullptr;
        const CPLXMLNode *psRasterRenderer = FindRasterRenderer(oTree.get());
        if (!psRasterRenderer)
        {
            return nullptr;
        }
        const char *pszType = CPLGetXMLValue(psRasterRenderer, "type", "");
        const char *pszColorEntryNodeName =
            strcmp(pszType, "paletted") == 0 ? "paletteEntry" : "item";
        const CPLXMLNode *psEntry =
            CPLSearchXMLNode(psRasterRenderer, pszColorEntryNodeName);
        if (!psEntry)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find color entry");
            return nullptr;
        }
        for (; psEntry; psEntry = psEntry->psNext)
        {
            if (psEntry->eType == CXT_Element &&
                strcmp(psEntry->pszValue, pszColorEntryNodeName) == 0)
            {
                // <paletteEntry value="74" label="74" alpha="255" color="#431be1"/>
                // <item value="74" label="74" alpha="255" color="#ffffcc"/>
                char *pszEndPtr = nullptr;
                const char *pszValue = CPLGetXMLValue(psEntry, "value", "");
                const auto nVal = std::strtol(pszValue, &pszEndPtr, 10);
                if (pszEndPtr != pszValue + strlen(pszValue) || nVal < 0 ||
                    nVal > 65536)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Unsupported value '%s' for color entry. Only "
                             "integer value in [0, 65535] range supported",
                             pszValue);
                    return nullptr;
                }

                long nAlpha = 255;
                const char *pszAlpha =
                    CPLGetXMLValue(psEntry, "alpha", nullptr);
                if (pszAlpha && pszEndPtr == pszAlpha + strlen(pszAlpha))
                {
                    nAlpha = std::clamp<long>(
                        std::strtol(pszAlpha, &pszEndPtr, 10), 0, 255);
                }

                long nColor = 0;
                const char *pszColor = CPLGetXMLValue(psEntry, "color", "");
                if (strlen(pszColor) == 7 && pszColor[0] == '#' &&
                    (nColor = std::strtol(pszColor + 1, &pszEndPtr, 16)) >= 0 &&
                    nColor <= 0xFFFFFF &&
                    pszEndPtr == pszColor + strlen(pszColor))
                {
                    GDALColorEntry sColor;
                    sColor.c1 = static_cast<GByte>((nColor >> 16) & 0xFF);
                    sColor.c2 = static_cast<GByte>((nColor >> 8) & 0xFF);
                    sColor.c3 = static_cast<GByte>(nColor & 0xFF);
                    sColor.c4 = static_cast<GByte>(nAlpha);
                    poCT->SetColorEntry(static_cast<int>(nVal), &sColor);
                }
                else
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Unsupported color '%s' for color entry.",
                             pszColor);
                    return nullptr;
                }
            }
        }
    }
    else
    {
        const auto asEntries = GDALLoadTextColorMap(pszFilename, nullptr);
        if (asEntries.empty())
            return nullptr;
        for (const auto &sEntry : asEntries)
        {
            if (!(sEntry.dfVal >= 0 && sEntry.dfVal <= 65536 &&
                  static_cast<int>(sEntry.dfVal) == sEntry.dfVal))
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Unsupported value '%f' for color entry. Only integer "
                         "value in [0, 65535] range supported",
                         sEntry.dfVal);
                return nullptr;
            }
            GDALColorEntry sColor;
            sColor.c1 = static_cast<GByte>(sEntry.nR);
            sColor.c2 = static_cast<GByte>(sEntry.nG);
            sColor.c3 = static_cast<GByte>(sEntry.nB);
            sColor.c4 = static_cast<GByte>(sEntry.nA);
            poCT->SetColorEntry(static_cast<int>(sEntry.dfVal), &sColor);
        }
    }
    return poCT;
}

/************************************************************************/
/*                     GDALGetAbsoluteValFromPct()                      */
/************************************************************************/

/* dfPct : percentage between 0 and 1 */
static double GDALGetAbsoluteValFromPct(GDALRasterBand *poBand, double dfPct)
{
    int bSuccessMin = FALSE;
    int bSuccessMax = FALSE;
    double dfMin = poBand->GetMinimum(&bSuccessMin);
    double dfMax = poBand->GetMaximum(&bSuccessMax);
    if (!bSuccessMin || !bSuccessMax)
    {
        double dfMean = 0.0;
        double dfStdDev = 0.0;
        CPLDebug("GDAL", "Computing source raster statistics...");
        poBand->ComputeStatistics(false, &dfMin, &dfMax, &dfMean, &dfStdDev,
                                  nullptr, nullptr);
    }
    return dfMin + dfPct * (dfMax - dfMin);
}

/************************************************************************/
/*                         GDALFindNamedColor()                         */
/************************************************************************/

static bool GDALFindNamedColor(const char *pszColorName, int *pnR, int *pnG,
                               int *pnB)
{

    typedef struct
    {
        const char *name;
        float r, g, b;
    } NamedColor;

    static const NamedColor namedColors[] = {
        {"white", 1.00, 1.00, 1.00},   {"black", 0.00, 0.00, 0.00},
        {"red", 1.00, 0.00, 0.00},     {"green", 0.00, 1.00, 0.00},
        {"blue", 0.00, 0.00, 1.00},    {"yellow", 1.00, 1.00, 0.00},
        {"magenta", 1.00, 0.00, 1.00}, {"cyan", 0.00, 1.00, 1.00},
        {"aqua", 0.00, 0.75, 0.75},    {"grey", 0.75, 0.75, 0.75},
        {"gray", 0.75, 0.75, 0.75},    {"orange", 1.00, 0.50, 0.00},
        {"brown", 0.75, 0.50, 0.25},   {"purple", 0.50, 0.00, 1.00},
        {"violet", 0.50, 0.00, 1.00},  {"indigo", 0.00, 0.50, 1.00},
    };

    *pnR = 0;
    *pnG = 0;
    *pnB = 0;
    for (const auto &namedColor : namedColors)
    {
        if (EQUAL(pszColorName, namedColor.name))
        {
            *pnR = static_cast<int>(255.0f * namedColor.r);
            *pnG = static_cast<int>(255.0f * namedColor.g);
            *pnB = static_cast<int>(255.0f * namedColor.b);
            return true;
        }
    }
    return false;
}

/************************************************************************/
/*                        GDALLoadTextColorMap()                        */
/************************************************************************/

/**
 * \brief Load a color map from a GMT or GRASS text file.
 * @since GDAL 3.12
 */

std::vector<GDALColorAssociation> GDALLoadTextColorMap(const char *pszFilename,
                                                       GDALRasterBand *poBand)
{
    auto fpColorFile = VSIVirtualHandleUniquePtr(VSIFOpenL(pszFilename, "rb"));
    if (fpColorFile == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find %s", pszFilename);
        return {};
    }

    // Detect UTF-16 BOM (common on Windows)
    unsigned char bom[2] = {0, 0};
    if (fpColorFile->Read(bom, 1, 2) == 2)
    {
        if ((bom[0] == 0xFF && bom[1] == 0xFE) ||
            (bom[0] == 0xFE && bom[1] == 0xFF))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Color map file must be UTF-8 encoded, not UTF-16: %s",
                     pszFilename);
            return {};
        }
        fpColorFile->Seek(0, SEEK_SET);
    }

    std::vector<GDALColorAssociation> asColorAssociation;

    int bHasNoData = FALSE;
    const double dfNoDataValue =
        poBand ? poBand->GetNoDataValue(&bHasNoData) : 0.0;

    bool bIsGMT_CPT = false;
    GDALColorAssociation sColor;
    while (const char *pszLine =
               CPLReadLine2L(fpColorFile.get(), 10 * 1024, nullptr))
    {
        if (pszLine[0] == '#' && strstr(pszLine, "COLOR_MODEL"))
        {
            if (strstr(pszLine, "COLOR_MODEL = RGB") == nullptr)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Only COLOR_MODEL = RGB is supported");
                return {};
            }
            bIsGMT_CPT = true;
        }

        if (pszLine[0] == '#' || pszLine[0] == '/' || pszLine[0] == 0)
        {
            /* Skip comment and blank lines */
            continue;
        }

        const CPLStringList aosFields(
            CSLTokenizeStringComplex(pszLine, " ,\t:", FALSE, FALSE));
        const int nTokens = aosFields.size();

        if (bIsGMT_CPT && nTokens == 8)
        {
            sColor.dfVal = CPLAtof(aosFields[0]);
            sColor.nR = atoi(aosFields[1]);
            sColor.nG = atoi(aosFields[2]);
            sColor.nB = atoi(aosFields[3]);
            sColor.nA = 255;
            asColorAssociation.push_back(sColor);

            sColor.dfVal = CPLAtof(aosFields[4]);
            sColor.nR = atoi(aosFields[5]);
            sColor.nG = atoi(aosFields[6]);
            sColor.nB = atoi(aosFields[7]);
            sColor.nA = 255;
            asColorAssociation.push_back(sColor);
        }
        else if (bIsGMT_CPT && nTokens == 4)
        {
            // The first token might be B (background), F (foreground) or N
            // (nodata) Just interested in N.
            if (EQUAL(aosFields[0], "N") && bHasNoData)
            {
                sColor.dfVal = dfNoDataValue;
                sColor.nR = atoi(aosFields[1]);
                sColor.nG = atoi(aosFields[2]);
                sColor.nB = atoi(aosFields[3]);
                sColor.nA = 255;
                asColorAssociation.push_back(sColor);
            }
        }
        else if (!bIsGMT_CPT && nTokens >= 2)
        {
            if (EQUAL(aosFields[0], "nv"))
            {
                if (!bHasNoData)
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Input dataset has no nodata value. "
                             "Ignoring 'nv' entry in color palette");
                    continue;
                }
                sColor.dfVal = dfNoDataValue;
            }
            else if (strlen(aosFields[0]) > 1 &&
                     aosFields[0][strlen(aosFields[0]) - 1] == '%')
            {
                const double dfPct = CPLAtof(aosFields[0]) / 100.0;
                if (dfPct < 0.0 || dfPct > 1.0)
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Wrong value for a percentage : %s", aosFields[0]);
                    return {};
                }
                if (!poBand)
                {
                    CPLError(CE_Failure, CPLE_NotSupported,
                             "Percentage value not supported");
                    return {};
                }
                sColor.dfVal = GDALGetAbsoluteValFromPct(poBand, dfPct);
            }
            else
            {
                sColor.dfVal = CPLAtof(aosFields[0]);
            }

            if (nTokens >= 4)
            {
                sColor.nR = atoi(aosFields[1]);
                sColor.nG = atoi(aosFields[2]);
                sColor.nB = atoi(aosFields[3]);
                sColor.nA =
                    (CSLCount(aosFields) >= 5) ? atoi(aosFields[4]) : 255;
            }
            else
            {
                int nR = 0;
                int nG = 0;
                int nB = 0;
                if (!GDALFindNamedColor(aosFields[1], &nR, &nG, &nB))
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Unknown color : %s",
                             aosFields[1]);
                    return {};
                }
                sColor.nR = nR;
                sColor.nG = nG;
                sColor.nB = nB;
                sColor.nA =
                    (CSLCount(aosFields) >= 3) ? atoi(aosFields[2]) : 255;
            }
            asColorAssociation.push_back(sColor);
        }
    }

    if (asColorAssociation.empty())
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No color association found in %s", pszFilename);
    }
    return asColorAssociation;
}
