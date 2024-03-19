/******************************************************************************
 *
 * Project:  ZMap driver
 * Purpose:  GDALDataset driver for ZMap dataset.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "cpl_string.h"
#include "cpl_vsi_virtual.h"
#include "gdal_frmts.h"
#include "gdal_pam.h"

#include <array>
#include <cmath>
#include <deque>

/************************************************************************/
/* ==================================================================== */
/*                              ZMapDataset                             */
/* ==================================================================== */
/************************************************************************/

class ZMapRasterBand;

class ZMapDataset final : public GDALPamDataset
{
    friend class ZMapRasterBand;

    VSIVirtualHandleUniquePtr m_fp{};
    int m_nValuesPerLine = 0;
    int m_nFieldSize = 0;
    int m_nDecimalCount = 0;
    int m_nColNum = -1;
    double m_dfNoDataValue = 0;
    vsi_l_offset m_nDataStartOff = 0;
    std::array<double, 6> m_adfGeoTransform = {{0, 1, 0, 0, 0, 1}};
    int m_nFirstDataLine = 0;
    int m_nCurLine = 0;
    std::deque<double> m_odfQueue{};

  public:
    ZMapDataset();
    virtual ~ZMapDataset();

    virtual CPLErr GetGeoTransform(double *) override;

    static GDALDataset *Open(GDALOpenInfo *);
    static int Identify(GDALOpenInfo *);
    static GDALDataset *CreateCopy(const char *pszFilename,
                                   GDALDataset *poSrcDS, int bStrict,
                                   char **papszOptions,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData);
};

/************************************************************************/
/* ==================================================================== */
/*                            ZMapRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class ZMapRasterBand final : public GDALPamRasterBand
{
    friend class ZMapDataset;

  public:
    explicit ZMapRasterBand(ZMapDataset *);

    virtual CPLErr IReadBlock(int, int, void *) override;
    virtual double GetNoDataValue(int *pbSuccess = nullptr) override;
};

/************************************************************************/
/*                           ZMapRasterBand()                           */
/************************************************************************/

ZMapRasterBand::ZMapRasterBand(ZMapDataset *poDSIn)

{
    poDS = poDSIn;
    nBand = 1;

    eDataType = GDT_Float64;

    // The format is column oriented! That is we have first the value of
    // pixel (col=0, line=0), then the one of (col=0, line=1), etc.
    nBlockXSize = 1;
    nBlockYSize = poDSIn->GetRasterYSize();
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr ZMapRasterBand::IReadBlock(int nBlockXOff, CPL_UNUSED int nBlockYOff,
                                  void *pImage)
{
    ZMapDataset *poGDS = cpl::down_cast<ZMapDataset *>(poDS);

    // If seeking backwards in term of columns, reset reading to the first
    // column
    if (nBlockXOff < poGDS->m_nColNum + 1)
    {
        poGDS->m_fp->Seek(poGDS->m_nDataStartOff, SEEK_SET);
        poGDS->m_nColNum = -1;
        poGDS->m_nCurLine = poGDS->m_nFirstDataLine;
        poGDS->m_odfQueue.clear();
    }

    if (nBlockXOff > poGDS->m_nColNum + 1)
    {
        for (int i = poGDS->m_nColNum + 1; i < nBlockXOff; i++)
        {
            if (IReadBlock(i, 0, nullptr) != CE_None)
                return CE_Failure;
        }
    }

    int iRow = 0;
    const double dfExp = std::pow(10.0, poGDS->m_nDecimalCount);
    double *padfImage = reinterpret_cast<double *>(pImage);

    // If we have previously read too many values, start by consuming the
    // queue
    while (iRow < nRasterYSize && !poGDS->m_odfQueue.empty())
    {
        if (padfImage)
            padfImage[iRow] = poGDS->m_odfQueue.front();
        ++iRow;
        poGDS->m_odfQueue.pop_front();
    }

    // Now read as many lines as needed to finish filling the column buffer
    while (iRow < nRasterYSize)
    {
        constexpr int MARGIN = 16;  // Should be at least 2 for \r\n
        char *pszLine = const_cast<char *>(CPLReadLine2L(
            poGDS->m_fp.get(),
            poGDS->m_nValuesPerLine * poGDS->m_nFieldSize + MARGIN, nullptr));
        ++poGDS->m_nCurLine;
        if (pszLine == nullptr)
            return CE_Failure;

        // Each line should have at most m_nValuesPerLine values of size
        // m_nFieldSize
        const int nLineLen = static_cast<int>(strlen(pszLine));
        if ((nLineLen % poGDS->m_nFieldSize) != 0)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Line %d has length %d, which is not a multiple of %d",
                     poGDS->m_nCurLine, nLineLen, poGDS->m_nFieldSize);
            return CE_Failure;
        }

        const int nValuesThisLine = nLineLen / poGDS->m_nFieldSize;
        if (nValuesThisLine > poGDS->m_nValuesPerLine)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Line %d has %d values, whereas the maximum expected is %d",
                poGDS->m_nCurLine, nValuesThisLine, poGDS->m_nValuesPerLine);
            return CE_Failure;
        }

        for (int iValueThisLine = 0; iValueThisLine < nValuesThisLine;
             iValueThisLine++)
        {
            char *pszValue = pszLine + iValueThisLine * poGDS->m_nFieldSize;
            const char chSaved = pszValue[poGDS->m_nFieldSize];
            pszValue[poGDS->m_nFieldSize] = 0;
            const double dfVal = strchr(pszValue, '.') != nullptr
                                     ? CPLAtofM(pszValue)
                                     : atoi(pszValue) * dfExp;
            pszValue[poGDS->m_nFieldSize] = chSaved;
            if (iRow < nRasterYSize)
            {
                if (padfImage)
                    padfImage[iRow] = dfVal;
                ++iRow;
            }
            else
            {
                poGDS->m_odfQueue.push_back(dfVal);
            }
        }
    }

    poGDS->m_nColNum++;

    return CE_None;
}

/************************************************************************/
/*                          GetNoDataValue()                            */
/************************************************************************/

double ZMapRasterBand::GetNoDataValue(int *pbSuccess)
{
    ZMapDataset *poGDS = cpl::down_cast<ZMapDataset *>(poDS);

    if (pbSuccess)
        *pbSuccess = TRUE;

    return poGDS->m_dfNoDataValue;
}

/************************************************************************/
/*                            ~ZMapDataset()                            */
/************************************************************************/

ZMapDataset::ZMapDataset() = default;

/************************************************************************/
/*                            ~ZMapDataset()                            */
/************************************************************************/

ZMapDataset::~ZMapDataset()

{
    FlushCache(true);
}

/************************************************************************/
/*                             Identify()                               */
/************************************************************************/

int ZMapDataset::Identify(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->nHeaderBytes == 0)
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Check that it looks roughly as a ZMap dataset                   */
    /* -------------------------------------------------------------------- */
    const char *pszData =
        reinterpret_cast<const char *>(poOpenInfo->pabyHeader);

    /* Skip comments line at the beginning */
    int i = 0;
    if (pszData[i] == '!')
    {
        i++;
        for (; i < poOpenInfo->nHeaderBytes; i++)
        {
            char ch = pszData[i];
            if (ch == 13 || ch == 10)
            {
                i++;
                if (ch == 13 && pszData[i] == 10)
                    i++;
                if (pszData[i] != '!')
                    break;
            }
        }
    }

    if (pszData[i] != '@')
        return FALSE;
    i++;

    const CPLStringList aosTokens(CSLTokenizeString2(pszData + i, ",", 0));
    if (aosTokens.size() < 3)
    {
        return FALSE;
    }

    const char *pszToken = aosTokens[1];
    while (*pszToken == ' ')
        pszToken++;

    return STARTS_WITH(pszToken, "GRID");
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *ZMapDataset::Open(GDALOpenInfo *poOpenInfo)

{
    if (!Identify(poOpenInfo) || poOpenInfo->fpL == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Confirm the requested access is supported.                      */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->eAccess == GA_Update)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "The ZMAP driver does not support update access to existing"
                 " datasets.");
        return nullptr;
    }

    auto poDS = std::make_unique<ZMapDataset>();
    poDS->m_fp.reset(poOpenInfo->fpL);
    poOpenInfo->fpL = nullptr;

    /* -------------------------------------------------------------------- */
    /*      Find dataset characteristics                                    */
    /* -------------------------------------------------------------------- */

    const char *pszLine;
    int nLine = 0;
    constexpr int MAX_HEADER_LINE = 1024;
    while ((pszLine = CPLReadLine2L(poDS->m_fp.get(), MAX_HEADER_LINE,
                                    nullptr)) != nullptr)
    {
        ++nLine;
        if (*pszLine == '!')
        {
            continue;
        }
        else
            break;
    }
    // cppcheck-suppress knownConditionTrueFalse
    if (pszLine == nullptr)
    {
        return nullptr;
    }

    /* Parse first header line */
    CPLStringList aosTokensFirstLine(CSLTokenizeString2(pszLine, ",", 0));
    if (aosTokensFirstLine.size() != 3)
    {
        return nullptr;
    }

    const int nValuesPerLine = atoi(aosTokensFirstLine[2]);
    if (nValuesPerLine <= 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid/unsupported value for nValuesPerLine = %d",
                 nValuesPerLine);
        return nullptr;
    }

    /* Parse second header line */
    pszLine = CPLReadLine2L(poDS->m_fp.get(), MAX_HEADER_LINE, nullptr);
    ++nLine;
    if (pszLine == nullptr)
    {
        return nullptr;
    }
    const CPLStringList aosTokensSecondLine(
        CSLTokenizeString2(pszLine, ",", 0));
    if (aosTokensSecondLine.size() != 5)
    {
        return nullptr;
    }

    const int nFieldSize = atoi(aosTokensSecondLine[0]);
    const double dfNoDataValue = CPLAtofM(aosTokensSecondLine[1]);
    const int nDecimalCount = atoi(aosTokensSecondLine[3]);
    const int nColumnNumber = atoi(aosTokensSecondLine[4]);

    if (nFieldSize <= 0 || nFieldSize >= 40)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid/unsupported value for nFieldSize = %d", nFieldSize);
        return nullptr;
    }

    if (nDecimalCount <= 0 || nDecimalCount >= nFieldSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid/unsupported value for nDecimalCount = %d",
                 nDecimalCount);
        return nullptr;
    }

    if (nColumnNumber != 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid/unsupported value for nColumnNumber = %d",
                 nColumnNumber);
        return nullptr;
    }

    if (nValuesPerLine <= 0 || nFieldSize > 1024 * 1024 / nValuesPerLine)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid/unsupported value for nFieldSize = %d x "
                 "nValuesPerLine = %d",
                 nFieldSize, nValuesPerLine);
        return nullptr;
    }

    /* Parse third header line */
    pszLine = CPLReadLine2L(poDS->m_fp.get(), MAX_HEADER_LINE, nullptr);
    ++nLine;
    if (pszLine == nullptr)
    {
        return nullptr;
    }
    const CPLStringList aosTokensThirdLine(CSLTokenizeString2(pszLine, ",", 0));
    if (aosTokensThirdLine.size() != 6)
    {
        return nullptr;
    }

    const int nRows = atoi(aosTokensThirdLine[0]);
    const int nCols = atoi(aosTokensThirdLine[1]);
    const double dfMinX = CPLAtofM(aosTokensThirdLine[2]);
    const double dfMaxX = CPLAtofM(aosTokensThirdLine[3]);
    const double dfMinY = CPLAtofM(aosTokensThirdLine[4]);
    const double dfMaxY = CPLAtofM(aosTokensThirdLine[5]);

    if (!GDALCheckDatasetDimensions(nCols, nRows) || nCols == 1 || nRows == 1)
    {
        return nullptr;
    }

    /* Ignore fourth header line */
    pszLine = CPLReadLine2L(poDS->m_fp.get(), MAX_HEADER_LINE, nullptr);
    ++nLine;
    if (pszLine == nullptr)
    {
        return nullptr;
    }

    /* Check fifth header line */
    pszLine = CPLReadLine2L(poDS->m_fp.get(), MAX_HEADER_LINE, nullptr);
    ++nLine;
    if (pszLine == nullptr || pszLine[0] != '@')
    {
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    poDS->m_nDataStartOff = VSIFTellL(poDS->m_fp.get());
    poDS->m_nValuesPerLine = nValuesPerLine;
    poDS->m_nFieldSize = nFieldSize;
    poDS->m_nDecimalCount = nDecimalCount;
    poDS->nRasterXSize = nCols;
    poDS->nRasterYSize = nRows;
    poDS->m_dfNoDataValue = dfNoDataValue;
    poDS->m_nFirstDataLine = nLine;

    if (CPLTestBool(CPLGetConfigOption("ZMAP_PIXEL_IS_POINT", "FALSE")))
    {
        const double dfStepX = (dfMaxX - dfMinX) / (nCols - 1);
        const double dfStepY = (dfMaxY - dfMinY) / (nRows - 1);

        poDS->m_adfGeoTransform[0] = dfMinX - dfStepX / 2;
        poDS->m_adfGeoTransform[1] = dfStepX;
        poDS->m_adfGeoTransform[3] = dfMaxY + dfStepY / 2;
        poDS->m_adfGeoTransform[5] = -dfStepY;
    }
    else
    {
        const double dfStepX = (dfMaxX - dfMinX) / nCols;
        const double dfStepY = (dfMaxY - dfMinY) / nRows;

        poDS->m_adfGeoTransform[0] = dfMinX;
        poDS->m_adfGeoTransform[1] = dfStepX;
        poDS->m_adfGeoTransform[3] = dfMaxY;
        poDS->m_adfGeoTransform[5] = -dfStepY;
    }

    /* -------------------------------------------------------------------- */
    /*      Create band information objects.                                */
    /* -------------------------------------------------------------------- */
    poDS->nBands = 1;
    poDS->SetBand(1, std::make_unique<ZMapRasterBand>(poDS.get()));

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    poDS->SetDescription(poOpenInfo->pszFilename);
    poDS->TryLoadXML();

    /* -------------------------------------------------------------------- */
    /*      Support overviews.                                              */
    /* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize(poDS.get(), poOpenInfo->pszFilename);
    return poDS.release();
}

/************************************************************************/
/*                       WriteRightJustified()                          */
/************************************************************************/

static void WriteRightJustified(VSIVirtualHandleUniquePtr &fp,
                                const char *pszValue, int nWidth)
{
    int nLen = (int)strlen(pszValue);
    CPLAssert(nLen <= nWidth);
    for (int i = 0; i < nWidth - nLen; i++)
        fp->Write(" ", 1, 1);
    fp->Write(pszValue, 1, nLen);
}

static void WriteRightJustified(VSIVirtualHandleUniquePtr &fp, int nValue,
                                int nWidth)
{
    CPLString osValue(CPLSPrintf("%d", nValue));
    WriteRightJustified(fp, osValue.c_str(), nWidth);
}

static void WriteRightJustified(VSIVirtualHandleUniquePtr &fp, double dfValue,
                                int nWidth, int nDecimals = -1)
{
    char szFormat[32];
    if (nDecimals >= 0)
        snprintf(szFormat, sizeof(szFormat), "%%.%df", nDecimals);
    else
        snprintf(szFormat, sizeof(szFormat), "%%g");
    char *pszValue = const_cast<char *>(CPLSPrintf(szFormat, dfValue));
    char *pszE = strchr(pszValue, 'e');
    if (pszE)
        *pszE = 'E';

    if (static_cast<int>(strlen(pszValue)) > nWidth)
    {
        CPLAssert(nDecimals >= 0);
        snprintf(szFormat, sizeof(szFormat), "%%.%dg", nDecimals);
        pszValue = const_cast<char *>(CPLSPrintf(szFormat, dfValue));
        pszE = strchr(pszValue, 'e');
        if (pszE)
            *pszE = 'E';
    }

    CPLString osValue(pszValue);
    WriteRightJustified(fp, osValue.c_str(), nWidth);
}

/************************************************************************/
/*                             CreateCopy()                             */
/************************************************************************/

GDALDataset *ZMapDataset::CreateCopy(const char *pszFilename,
                                     GDALDataset *poSrcDS, int bStrict,
                                     CPL_UNUSED char **papszOptions,
                                     GDALProgressFunc pfnProgress,
                                     void *pProgressData)
{
    /* -------------------------------------------------------------------- */
    /*      Some some rudimentary checks                                    */
    /* -------------------------------------------------------------------- */
    int nBands = poSrcDS->GetRasterCount();
    if (nBands == 0)
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "ZMap driver does not support source dataset with zero band.\n");
        return nullptr;
    }

    if (nBands != 1)
    {
        CPLError((bStrict) ? CE_Failure : CE_Warning, CPLE_NotSupported,
                 "ZMap driver only uses the first band of the dataset.\n");
        if (bStrict)
            return nullptr;
    }

    if (pfnProgress && !pfnProgress(0.0, nullptr, pProgressData))
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Get source dataset info                                         */
    /* -------------------------------------------------------------------- */

    const int nXSize = poSrcDS->GetRasterXSize();
    const int nYSize = poSrcDS->GetRasterYSize();
    if (nXSize == 1 || nYSize == 1)
    {
        return nullptr;
    }

    double adfGeoTransform[6];
    poSrcDS->GetGeoTransform(adfGeoTransform);
    if (adfGeoTransform[2] != 0 || adfGeoTransform[4] != 0)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "ZMap driver does not support CreateCopy() from skewed or "
                 "rotated dataset.\n");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create target file                                              */
    /* -------------------------------------------------------------------- */

    auto fp = VSIVirtualHandleUniquePtr(VSIFOpenL(pszFilename, "wb"));
    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot create %s", pszFilename);
        return nullptr;
    }

    const int nFieldSize = 20;
    const int nValuesPerLine = 4;
    const int nDecimalCount = 7;

    int bHasNoDataValue = FALSE;
    double dfNoDataValue =
        poSrcDS->GetRasterBand(1)->GetNoDataValue(&bHasNoDataValue);
    if (!bHasNoDataValue)
        dfNoDataValue = 1.e30;

    fp->Printf("!\n");
    fp->Printf("! Created by GDAL.\n");
    fp->Printf("!\n");
    fp->Printf("@GRID FILE, GRID, %d\n", nValuesPerLine);

    WriteRightJustified(fp, nFieldSize, 10);
    fp->Printf(",");
    WriteRightJustified(fp, dfNoDataValue, nFieldSize, nDecimalCount);
    fp->Printf(",");
    WriteRightJustified(fp, "", 10);
    fp->Printf(",");
    WriteRightJustified(fp, nDecimalCount, 10);
    fp->Printf(",");
    WriteRightJustified(fp, 1, 10);
    fp->Printf("\n");

    WriteRightJustified(fp, nYSize, 10);
    fp->Printf(",");
    WriteRightJustified(fp, nXSize, 10);
    fp->Printf(",");

    if (CPLTestBool(CPLGetConfigOption("ZMAP_PIXEL_IS_POINT", "FALSE")))
    {
        WriteRightJustified(fp, adfGeoTransform[0] + adfGeoTransform[1] / 2, 14,
                            7);
        fp->Printf(",");
        WriteRightJustified(fp,
                            adfGeoTransform[0] + adfGeoTransform[1] * nXSize -
                                adfGeoTransform[1] / 2,
                            14, 7);
        fp->Printf(",");
        WriteRightJustified(fp,
                            adfGeoTransform[3] + adfGeoTransform[5] * nYSize -
                                adfGeoTransform[5] / 2,
                            14, 7);
        fp->Printf(",");
        WriteRightJustified(fp, adfGeoTransform[3] + adfGeoTransform[5] / 2, 14,
                            7);
    }
    else
    {
        WriteRightJustified(fp, adfGeoTransform[0], 14, 7);
        fp->Printf(",");
        WriteRightJustified(
            fp, adfGeoTransform[0] + adfGeoTransform[1] * nXSize, 14, 7);
        fp->Printf(",");
        WriteRightJustified(
            fp, adfGeoTransform[3] + adfGeoTransform[5] * nYSize, 14, 7);
        fp->Printf(",");
        WriteRightJustified(fp, adfGeoTransform[3], 14, 7);
    }

    fp->Printf("\n");

    fp->Printf("0.0, 0.0, 0.0\n");
    fp->Printf("@\n");

    /* -------------------------------------------------------------------- */
    /*      Copy imagery                                                    */
    /* -------------------------------------------------------------------- */
    std::vector<double> adfLineBuffer(nYSize);

    CPLErr eErr = CE_None;
    const bool bEmitEOLAtEndOfColumn = CPLTestBool(
        CPLGetConfigOption("ZMAP_EMIT_EOL_AT_END_OF_COLUMN", "YES"));
    bool bEOLPrinted = false;
    int nValuesThisLine = 0;
    for (int i = 0; i < nXSize && eErr == CE_None; i++)
    {
        eErr = poSrcDS->GetRasterBand(1)->RasterIO(
            GF_Read, i, 0, 1, nYSize, adfLineBuffer.data(), 1, nYSize,
            GDT_Float64, 0, 0, nullptr);
        if (eErr != CE_None)
            break;
        for (int j = 0; j < nYSize; j++)
        {
            WriteRightJustified(fp, adfLineBuffer[j], nFieldSize,
                                nDecimalCount);
            ++nValuesThisLine;
            if (nValuesThisLine == nValuesPerLine)
            {
                bEOLPrinted = true;
                nValuesThisLine = 0;
                fp->Printf("\n");
            }
            else
                bEOLPrinted = false;
        }
        if (bEmitEOLAtEndOfColumn && !bEOLPrinted)
        {
            bEOLPrinted = true;
            nValuesThisLine = 0;
            fp->Printf("\n");
        }

        if (pfnProgress != nullptr &&
            !pfnProgress((i + 1) * 1.0 / nXSize, nullptr, pProgressData))
        {
            eErr = CE_Failure;
            break;
        }
    }
    if (!bEOLPrinted)
        fp->Printf("\n");

    if (eErr != CE_None || fp->Close() != 0)
        return nullptr;

    fp.reset();
    GDALOpenInfo oOpenInfo(pszFilename, GA_ReadOnly);
    return ZMapDataset::Open(&oOpenInfo);
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr ZMapDataset::GetGeoTransform(double *padfTransform)

{
    memcpy(padfTransform, m_adfGeoTransform.data(), 6 * sizeof(double));

    return CE_None;
}

/************************************************************************/
/*                         GDALRegister_ZMap()                          */
/************************************************************************/

void GDALRegister_ZMap()

{
    if (GDALGetDriverByName("ZMap") != nullptr)
        return;

    auto poDriver = std::make_unique<GDALDriver>();

    poDriver->SetDescription("ZMap");
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "ZMap Plus Grid");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/zmap.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "dat");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnOpen = ZMapDataset::Open;
    poDriver->pfnIdentify = ZMapDataset::Identify;
    poDriver->pfnCreateCopy = ZMapDataset::CreateCopy;

    GetGDALDriverManager()->RegisterDriver(poDriver.release());
}
