/******************************************************************************
 *
 * Project:  PDF driver
 * Purpose:  GDALDataset driver for PDF dataset.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 *
 * Support for open-source PDFium library
 *
 * Copyright (C) 2015 Klokan Technologies GmbH (http://www.klokantech.com/)
 * Author: Martin Mikita <martin.mikita@klokantech.com>, xmikit00 @ FIT VUT Brno
 *
 ******************************************************************************
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "gdal_pdf.h"

#include "cpl_vsi_virtual.h"
#include "cpl_spawn.h"
#include "cpl_string.h"
#include "gdal_frmts.h"
#include "ogr_spatialref.h"
#include "ogr_geometry.h"

#ifdef HAVE_POPPLER
#include "cpl_multiproc.h"
#include "pdfio.h"
#endif  // HAVE_POPPLER

#include "pdfcreatecopy.h"

#include "pdfdrivercore.h"

#include <algorithm>
#include <cassert>
#include <set>

#ifdef HAVE_PDFIUM
// To be able to use
// https://github.com/rouault/pdfium_build_gdal_3_5/releases/download/v1_pdfium_5106/install-win10-vs2019-x64-rev5106.zip
// with newer Visual Studio versions.
// Trick from https://github.com/conan-io/conan-center-index/issues/4826
#if _MSC_VER >= 1932  // Visual Studio 2022 version 17.2+
#pragma comment(                                                               \
    linker,                                                                    \
    "/alternatename:__imp___std_init_once_complete=__imp_InitOnceComplete")
#pragma comment(                                                               \
    linker,                                                                    \
    "/alternatename:__imp___std_init_once_begin_initialize=__imp_InitOnceBeginInitialize")
#endif
#endif

/* g++ -fPIC -g -Wall frmts/pdf/pdfdataset.cpp -shared -o gdal_PDF.so -Iport
 * -Igcore -Iogr -L. -lgdal -lpoppler -I/usr/include/poppler */

#ifdef HAVE_PDF_READ_SUPPORT

static double Get(GDALPDFObject *poObj, int nIndice = -1);

#ifdef HAVE_POPPLER

static CPLMutex *hGlobalParamsMutex = nullptr;

/************************************************************************/
/*                         GDALPDFOutputDev                             */
/************************************************************************/

class GDALPDFOutputDev : public SplashOutputDev
{
  private:
    int bEnableVector;
    int bEnableText;
    int bEnableBitmap;

    void skipBytes(Stream *str, int width, int height, int nComps, int nBits)
    {
        int nVals = width * nComps;
        int nLineSize = (nVals * nBits + 7) >> 3;
        int nBytes = nLineSize * height;
        for (int i = 0; i < nBytes; i++)
        {
            if (str->getChar() == EOF)
                break;
        }
    }

  public:
    GDALPDFOutputDev(SplashColorMode colorModeA, int bitmapRowPadA,
                     bool reverseVideoA, SplashColorPtr paperColorA)
        : SplashOutputDev(colorModeA, bitmapRowPadA, reverseVideoA,
                          paperColorA),
          bEnableVector(TRUE), bEnableText(TRUE), bEnableBitmap(TRUE)
    {
    }

    void SetEnableVector(int bFlag)
    {
        bEnableVector = bFlag;
    }

    void SetEnableText(int bFlag)
    {
        bEnableText = bFlag;
    }

    void SetEnableBitmap(int bFlag)
    {
        bEnableBitmap = bFlag;
    }

    virtual void startPage(int pageNum, GfxState *state, XRef *xrefIn) override
    {
        SplashOutputDev::startPage(pageNum, state, xrefIn);
        SplashBitmap *poBitmap = getBitmap();
        memset(poBitmap->getDataPtr(), 255,
               static_cast<size_t>(poBitmap->getRowSize()) *
                   poBitmap->getHeight());
    }

    virtual void stroke(GfxState *state) override
    {
        if (bEnableVector)
            SplashOutputDev::stroke(state);
    }

    virtual void fill(GfxState *state) override
    {
        if (bEnableVector)
            SplashOutputDev::fill(state);
    }

    virtual void eoFill(GfxState *state) override
    {
        if (bEnableVector)
            SplashOutputDev::eoFill(state);
    }

    virtual void drawChar(GfxState *state, double x, double y, double dx,
                          double dy, double originX, double originY,
                          CharCode code, int nBytes, const Unicode *u,
                          int uLen) override
    {
        if (bEnableText)
            SplashOutputDev::drawChar(state, x, y, dx, dy, originX, originY,
                                      code, nBytes, u, uLen);
    }

    virtual void beginTextObject(GfxState *state) override
    {
        if (bEnableText)
            SplashOutputDev::beginTextObject(state);
    }

    virtual void endTextObject(GfxState *state) override
    {
        if (bEnableText)
            SplashOutputDev::endTextObject(state);
    }

    virtual void drawImageMask(GfxState *state, Object *ref, Stream *str,
                               int width, int height, bool invert,
                               bool interpolate, bool inlineImg) override
    {
        if (bEnableBitmap)
            SplashOutputDev::drawImageMask(state, ref, str, width, height,
                                           invert, interpolate, inlineImg);
        else
        {
            str->reset();
            if (inlineImg)
            {
                skipBytes(str, width, height, 1, 1);
            }
            str->close();
        }
    }

    virtual void setSoftMaskFromImageMask(GfxState *state, Object *ref,
                                          Stream *str, int width, int height,
                                          bool invert, bool inlineImg,
                                          double *baseMatrix) override
    {
        if (bEnableBitmap)
            SplashOutputDev::setSoftMaskFromImageMask(
                state, ref, str, width, height, invert, inlineImg, baseMatrix);
        else
            str->close();
    }

    virtual void unsetSoftMaskFromImageMask(GfxState *state,
                                            double *baseMatrix) override
    {
        if (bEnableBitmap)
            SplashOutputDev::unsetSoftMaskFromImageMask(state, baseMatrix);
    }

    virtual void drawImage(GfxState *state, Object *ref, Stream *str, int width,
                           int height, GfxImageColorMap *colorMap,
                           bool interpolate, const int *maskColors,
                           bool inlineImg) override
    {
        if (bEnableBitmap)
            SplashOutputDev::drawImage(state, ref, str, width, height, colorMap,
                                       interpolate, maskColors, inlineImg);
        else
        {
            str->reset();
            if (inlineImg)
            {
                skipBytes(str, width, height, colorMap->getNumPixelComps(),
                          colorMap->getBits());
            }
            str->close();
        }
    }

    virtual void drawMaskedImage(GfxState *state, Object *ref, Stream *str,
                                 int width, int height,
                                 GfxImageColorMap *colorMap, bool interpolate,
                                 Stream *maskStr, int maskWidth, int maskHeight,
                                 bool maskInvert, bool maskInterpolate) override
    {
        if (bEnableBitmap)
            SplashOutputDev::drawMaskedImage(
                state, ref, str, width, height, colorMap, interpolate, maskStr,
                maskWidth, maskHeight, maskInvert, maskInterpolate);
        else
            str->close();
    }

    virtual void drawSoftMaskedImage(GfxState *state, Object *ref, Stream *str,
                                     int width, int height,
                                     GfxImageColorMap *colorMap,
                                     bool interpolate, Stream *maskStr,
                                     int maskWidth, int maskHeight,
                                     GfxImageColorMap *maskColorMap,
                                     bool maskInterpolate) override
    {
        if (bEnableBitmap)
        {
            if (maskColorMap->getBits() <=
                0) /* workaround poppler bug (robustness) */
            {
                str->close();
                return;
            }
            SplashOutputDev::drawSoftMaskedImage(
                state, ref, str, width, height, colorMap, interpolate, maskStr,
                maskWidth, maskHeight, maskColorMap, maskInterpolate);
        }
        else
            str->close();
    }
};

#endif  // ~ HAVE_POPPLER

/************************************************************************/
/*                         Dump routines                                */
/************************************************************************/

class GDALPDFDumper
{
  private:
    FILE *f;
    int nDepthLimit;
    std::set<int> aoSetObjectExplored;
    int bDumpParent;

    void DumpSimplified(GDALPDFObject *poObj);

  public:
    GDALPDFDumper(const char *pszFilename, const char *pszDumpFile,
                  int nDepthLimitIn = -1)
        : nDepthLimit(nDepthLimitIn)
    {
        bDumpParent =
            CPLTestBool(CPLGetConfigOption("PDF_DUMP_PARENT", "FALSE"));
        if (strcmp(pszDumpFile, "stderr") == 0)
            f = stderr;
        else if (EQUAL(pszDumpFile, "YES"))
            f = fopen(CPLSPrintf("dump_%s.txt", CPLGetFilename(pszFilename)),
                      "wt");
        else
            f = fopen(pszDumpFile, "wt");
        if (f == nullptr)
            f = stderr;
    }

    ~GDALPDFDumper()
    {
        if (f != stderr)
            fclose(f);
    }

    void Dump(GDALPDFObject *poObj, int nDepth = 0);
    void Dump(GDALPDFDictionary *poDict, int nDepth = 0);
    void Dump(GDALPDFArray *poArray, int nDepth = 0);
};

void GDALPDFDumper::Dump(GDALPDFArray *poArray, int nDepth)
{
    if (nDepthLimit >= 0 && nDepth > nDepthLimit)
        return;

    int nLength = poArray->GetLength();
    int i;
    CPLString osIndent;
    for (i = 0; i < nDepth; i++)
        osIndent += " ";
    for (i = 0; i < nLength; i++)
    {
        fprintf(f, "%sItem[%d]:", osIndent.c_str(), i);
        GDALPDFObject *poObj = nullptr;
        if ((poObj = poArray->Get(i)) != nullptr)
        {
            if (poObj->GetType() == PDFObjectType_String ||
                poObj->GetType() == PDFObjectType_Null ||
                poObj->GetType() == PDFObjectType_Bool ||
                poObj->GetType() == PDFObjectType_Int ||
                poObj->GetType() == PDFObjectType_Real ||
                poObj->GetType() == PDFObjectType_Name)
            {
                fprintf(f, " ");
                DumpSimplified(poObj);
                fprintf(f, "\n");
            }
            else
            {
                fprintf(f, "\n");
                Dump(poObj, nDepth + 1);
            }
        }
    }
}

void GDALPDFDumper::DumpSimplified(GDALPDFObject *poObj)
{
    switch (poObj->GetType())
    {
        case PDFObjectType_String:
            fprintf(f, "%s (string)", poObj->GetString().c_str());
            break;

        case PDFObjectType_Null:
            fprintf(f, "null");
            break;

        case PDFObjectType_Bool:
            fprintf(f, "%s (bool)", poObj->GetBool() ? "true" : "false");
            break;

        case PDFObjectType_Int:
            fprintf(f, "%d (int)", poObj->GetInt());
            break;

        case PDFObjectType_Real:
            fprintf(f, "%f (real)", poObj->GetReal());
            break;

        case PDFObjectType_Name:
            fprintf(f, "%s (name)", poObj->GetName().c_str());
            break;

        default:
            fprintf(f, "unknown !");
            break;
    }
}

void GDALPDFDumper::Dump(GDALPDFObject *poObj, int nDepth)
{
    if (nDepthLimit >= 0 && nDepth > nDepthLimit)
        return;

    int i;
    CPLString osIndent;
    for (i = 0; i < nDepth; i++)
        osIndent += " ";
    fprintf(f, "%sType = %s", osIndent.c_str(), poObj->GetTypeName());
    int nRefNum = poObj->GetRefNum().toInt();
    if (nRefNum != 0)
        fprintf(f, ", Num = %d, Gen = %d", nRefNum, poObj->GetRefGen());
    fprintf(f, "\n");

    if (nRefNum != 0)
    {
        if (aoSetObjectExplored.find(nRefNum) != aoSetObjectExplored.end())
            return;
        aoSetObjectExplored.insert(nRefNum);
    }

    switch (poObj->GetType())
    {
        case PDFObjectType_Array:
            Dump(poObj->GetArray(), nDepth + 1);
            break;

        case PDFObjectType_Dictionary:
            Dump(poObj->GetDictionary(), nDepth + 1);
            break;

        case PDFObjectType_String:
        case PDFObjectType_Null:
        case PDFObjectType_Bool:
        case PDFObjectType_Int:
        case PDFObjectType_Real:
        case PDFObjectType_Name:
            fprintf(f, "%s", osIndent.c_str());
            DumpSimplified(poObj);
            fprintf(f, "\n");
            break;

        default:
            fprintf(f, "%s", osIndent.c_str());
            fprintf(f, "unknown !\n");
            break;
    }

    GDALPDFStream *poStream = poObj->GetStream();
    if (poStream != nullptr)
    {
        fprintf(f,
                "%sHas stream (" CPL_FRMT_GIB
                " uncompressed bytes, " CPL_FRMT_GIB " raw bytes)\n",
                osIndent.c_str(), static_cast<GIntBig>(poStream->GetLength()),
                static_cast<GIntBig>(poStream->GetRawLength()));
    }
}

void GDALPDFDumper::Dump(GDALPDFDictionary *poDict, int nDepth)
{
    if (nDepthLimit >= 0 && nDepth > nDepthLimit)
        return;

    CPLString osIndent;
    for (int i = 0; i < nDepth; i++)
        osIndent += " ";
    int i = 0;
    const auto &oMap = poDict->GetValues();
    for (const auto &[osKey, poObj] : oMap)
    {
        fprintf(f, "%sItem[%d] : %s", osIndent.c_str(), i, osKey.c_str());
        ++i;
        if (osKey == "Parent" && !bDumpParent)
        {
            if (poObj->GetRefNum().toBool())
                fprintf(f, ", Num = %d, Gen = %d", poObj->GetRefNum().toInt(),
                        poObj->GetRefGen());
            fprintf(f, "\n");
            continue;
        }
        if (poObj != nullptr)
        {
            if (poObj->GetType() == PDFObjectType_String ||
                poObj->GetType() == PDFObjectType_Null ||
                poObj->GetType() == PDFObjectType_Bool ||
                poObj->GetType() == PDFObjectType_Int ||
                poObj->GetType() == PDFObjectType_Real ||
                poObj->GetType() == PDFObjectType_Name)
            {
                fprintf(f, " = ");
                DumpSimplified(poObj);
                fprintf(f, "\n");
            }
            else
            {
                fprintf(f, "\n");
                Dump(poObj, nDepth + 1);
            }
        }
    }
}

/************************************************************************/
/*                         PDFRasterBand()                              */
/************************************************************************/

PDFRasterBand::PDFRasterBand(PDFDataset *poDSIn, int nBandIn,
                             int nResolutionLevelIn)
    : nResolutionLevel(nResolutionLevelIn)
{
    poDS = poDSIn;
    nBand = nBandIn;

    eDataType = GDT_Byte;

    if (nResolutionLevel > 0)
    {
        nBlockXSize = 256;
        nBlockYSize = 256;
        poDSIn->SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    }
    else if (poDSIn->m_nBlockXSize)
    {
        nBlockXSize = poDSIn->m_nBlockXSize;
        nBlockYSize = poDSIn->m_nBlockYSize;
        poDSIn->SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    }
    else if (poDSIn->GetRasterXSize() <
             64 * 1024 * 1024 / poDSIn->GetRasterYSize())
    {
        nBlockXSize = poDSIn->GetRasterXSize();
        nBlockYSize = 1;
    }
    else
    {
        nBlockXSize = std::min(1024, poDSIn->GetRasterXSize());
        nBlockYSize = std::min(1024, poDSIn->GetRasterYSize());
        poDSIn->SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    }
}

/************************************************************************/
/*                         InitOverviews()                              */
/************************************************************************/

void PDFDataset::InitOverviews()
{
#ifdef HAVE_PDFIUM
    // Only if used pdfium, make "arbitrary overviews"
    // Blocks are 256x256
    if (m_bUseLib.test(PDFLIB_PDFIUM) && m_apoOvrDS.empty() &&
        m_apoOvrDSBackup.empty())
    {
        int nXSize = nRasterXSize;
        int nYSize = nRasterYSize;
        constexpr int minSize = 256;
        int nDiscard = 1;
        while (nXSize > minSize || nYSize > minSize)
        {
            nXSize = (nXSize + 1) / 2;
            nYSize = (nYSize + 1) / 2;

            auto poOvrDS = std::make_unique<PDFDataset>(this, nXSize, nYSize);

            for (int i = 0; i < nBands; i++)
                poOvrDS->SetBand(
                    i + 1, new PDFRasterBand(poOvrDS.get(), i + 1, nDiscard));

            m_apoOvrDS.emplace_back(std::move(poOvrDS));
            ++nDiscard;
        }
    }
#endif
#if defined(HAVE_POPPLER) || defined(HAVE_PODOFO)
    if (!m_bUseLib.test(PDFLIB_PDFIUM) && m_apoOvrDS.empty() &&
        m_apoOvrDSBackup.empty() && m_osUserPwd != "ASK_INTERACTIVE")
    {
        int nXSize = nRasterXSize;
        int nYSize = nRasterYSize;
        constexpr int minSize = 256;
        double dfDPI = m_dfDPI;
        while (nXSize > minSize || nYSize > minSize)
        {
            nXSize = (nXSize + 1) / 2;
            nYSize = (nYSize + 1) / 2;
            dfDPI /= 2;

            GDALOpenInfo oOpenInfo(GetDescription(), GA_ReadOnly);
            CPLStringList aosOpenOptions(CSLDuplicate(papszOpenOptions));
            aosOpenOptions.SetNameValue("DPI", CPLSPrintf("%g", dfDPI));
            aosOpenOptions.SetNameValue("BANDS", CPLSPrintf("%d", nBands));
            aosOpenOptions.SetNameValue("@OPEN_FOR_OVERVIEW", "YES");
            if (!m_osUserPwd.empty())
                aosOpenOptions.SetNameValue("USER_PWD", m_osUserPwd.c_str());
            oOpenInfo.papszOpenOptions = aosOpenOptions.List();
            auto poOvrDS = std::unique_ptr<PDFDataset>(Open(&oOpenInfo));
            if (!poOvrDS || poOvrDS->nBands != nBands)
                break;
            poOvrDS->m_bIsOvrDS = true;
            m_apoOvrDS.emplace_back(std::move(poOvrDS));
        }
    }
#endif
}

/************************************************************************/
/*                        GetColorInterpretation()                      */
/************************************************************************/

GDALColorInterp PDFRasterBand::GetColorInterpretation()
{
    PDFDataset *poGDS = (PDFDataset *)poDS;
    if (poGDS->nBands == 1)
        return GCI_GrayIndex;
    else
        return (GDALColorInterp)(GCI_RedBand + (nBand - 1));
}

/************************************************************************/
/*                          GetOverviewCount()                          */
/************************************************************************/

int PDFRasterBand::GetOverviewCount()
{
    PDFDataset *poGDS = cpl::down_cast<PDFDataset *>(poDS);
    if (poGDS->m_bIsOvrDS)
        return 0;
    if (GDALPamRasterBand::GetOverviewCount() > 0)
        return GDALPamRasterBand::GetOverviewCount();
    else
    {
        poGDS->InitOverviews();
        return static_cast<int>(poGDS->m_apoOvrDS.size());
    }
}

/************************************************************************/
/*                            GetOverview()                             */
/************************************************************************/

GDALRasterBand *PDFRasterBand::GetOverview(int iOverviewIndex)
{
    if (GDALPamRasterBand::GetOverviewCount() > 0)
        return GDALPamRasterBand::GetOverview(iOverviewIndex);

    else if (iOverviewIndex < 0 || iOverviewIndex >= GetOverviewCount())
        return nullptr;
    else
    {
        PDFDataset *poGDS = cpl::down_cast<PDFDataset *>(poDS);
        return poGDS->m_apoOvrDS[iOverviewIndex]->GetRasterBand(nBand);
    }
}

/************************************************************************/
/*                           ~PDFRasterBand()                           */
/************************************************************************/

PDFRasterBand::~PDFRasterBand()
{
}

/************************************************************************/
/*                         IReadBlockFromTile()                         */
/************************************************************************/

CPLErr PDFRasterBand::IReadBlockFromTile(int nBlockXOff, int nBlockYOff,
                                         void *pImage)

{
    PDFDataset *poGDS = (PDFDataset *)poDS;

    int nReqXSize = nBlockXSize;
    int nReqYSize = nBlockYSize;
    if ((nBlockXOff + 1) * nBlockXSize > nRasterXSize)
        nReqXSize = nRasterXSize - nBlockXOff * nBlockXSize;
    if ((nBlockYOff + 1) * nBlockYSize > nRasterYSize)
        nReqYSize = nRasterYSize - nBlockYOff * nBlockYSize;

    int nXBlocks = DIV_ROUND_UP(nRasterXSize, nBlockXSize);
    int iTile = poGDS->m_aiTiles[nBlockYOff * nXBlocks + nBlockXOff];
    if (iTile < 0)
    {
        memset(pImage, 0, static_cast<size_t>(nBlockXSize) * nBlockYSize);
        return CE_None;
    }

    GDALPDFTileDesc &sTile = poGDS->m_asTiles[iTile];
    GDALPDFObject *poImage = sTile.poImage;

    if (nBand == 4)
    {
        GDALPDFDictionary *poImageDict = poImage->GetDictionary();
        GDALPDFObject *poSMask = poImageDict->Get("SMask");
        if (poSMask != nullptr &&
            poSMask->GetType() == PDFObjectType_Dictionary)
        {
            GDALPDFDictionary *poSMaskDict = poSMask->GetDictionary();
            GDALPDFObject *poWidth = poSMaskDict->Get("Width");
            GDALPDFObject *poHeight = poSMaskDict->Get("Height");
            GDALPDFObject *poColorSpace = poSMaskDict->Get("ColorSpace");
            GDALPDFObject *poBitsPerComponent =
                poSMaskDict->Get("BitsPerComponent");
            int nBits = 0;
            if (poBitsPerComponent)
                nBits = (int)Get(poBitsPerComponent);
            if (poWidth && Get(poWidth) == nReqXSize && poHeight &&
                Get(poHeight) == nReqYSize && poColorSpace &&
                poColorSpace->GetType() == PDFObjectType_Name &&
                poColorSpace->GetName() == "DeviceGray" &&
                (nBits == 1 || nBits == 8))
            {
                GDALPDFStream *poStream = poSMask->GetStream();
                GByte *pabyStream = nullptr;

                if (poStream == nullptr)
                    return CE_Failure;

                pabyStream = (GByte *)poStream->GetBytes();
                if (pabyStream == nullptr)
                    return CE_Failure;

                const int nReqXSize1 = (nReqXSize + 7) / 8;
                if ((nBits == 8 &&
                     static_cast<size_t>(poStream->GetLength()) !=
                         static_cast<size_t>(nReqXSize) * nReqYSize) ||
                    (nBits == 1 &&
                     static_cast<size_t>(poStream->GetLength()) !=
                         static_cast<size_t>(nReqXSize1) * nReqYSize))
                {
                    VSIFree(pabyStream);
                    return CE_Failure;
                }

                GByte *pabyData = (GByte *)pImage;
                if (nReqXSize != nBlockXSize || nReqYSize != nBlockYSize)
                {
                    memset(pabyData, 0,
                           static_cast<size_t>(nBlockXSize) * nBlockYSize);
                }

                if (nBits == 8)
                {
                    for (int j = 0; j < nReqYSize; j++)
                    {
                        for (int i = 0; i < nReqXSize; i++)
                        {
                            pabyData[j * nBlockXSize + i] =
                                pabyStream[j * nReqXSize + i];
                        }
                    }
                }
                else
                {
                    for (int j = 0; j < nReqYSize; j++)
                    {
                        for (int i = 0; i < nReqXSize; i++)
                        {
                            if (pabyStream[j * nReqXSize1 + i / 8] &
                                (1 << (7 - (i % 8))))
                                pabyData[j * nBlockXSize + i] = 255;
                            else
                                pabyData[j * nBlockXSize + i] = 0;
                        }
                    }
                }

                VSIFree(pabyStream);
                return CE_None;
            }
        }

        memset(pImage, 255, static_cast<size_t>(nBlockXSize) * nBlockYSize);
        return CE_None;
    }

    if (poGDS->m_nLastBlockXOff == nBlockXOff &&
        poGDS->m_nLastBlockYOff == nBlockYOff &&
        poGDS->m_pabyCachedData != nullptr)
    {
#ifdef DEBUG
        CPLDebug("PDF", "Using cached block (%d, %d)", nBlockXOff, nBlockYOff);
#endif
        // do nothing
    }
    else
    {
        if (!poGDS->m_bTried)
        {
            poGDS->m_bTried = true;
            poGDS->m_pabyCachedData =
                (GByte *)VSIMalloc3(3, nBlockXSize, nBlockYSize);
        }
        if (poGDS->m_pabyCachedData == nullptr)
            return CE_Failure;

        GDALPDFStream *poStream = poImage->GetStream();
        GByte *pabyStream = nullptr;

        if (poStream == nullptr)
            return CE_Failure;

        pabyStream = (GByte *)poStream->GetBytes();
        if (pabyStream == nullptr)
            return CE_Failure;

        if (static_cast<size_t>(poStream->GetLength()) !=
            static_cast<size_t>(sTile.nBands) * nReqXSize * nReqYSize)
        {
            VSIFree(pabyStream);
            return CE_Failure;
        }

        memcpy(poGDS->m_pabyCachedData, pabyStream,
               static_cast<size_t>(poStream->GetLength()));
        VSIFree(pabyStream);
        poGDS->m_nLastBlockXOff = nBlockXOff;
        poGDS->m_nLastBlockYOff = nBlockYOff;
    }

    GByte *pabyData = (GByte *)pImage;
    if (nBand != 4 && (nReqXSize != nBlockXSize || nReqYSize != nBlockYSize))
    {
        memset(pabyData, 0, static_cast<size_t>(nBlockXSize) * nBlockYSize);
    }

    if (poGDS->nBands >= 3 && sTile.nBands == 3)
    {
        for (int j = 0; j < nReqYSize; j++)
        {
            for (int i = 0; i < nReqXSize; i++)
            {
                pabyData[j * nBlockXSize + i] =
                    poGDS
                        ->m_pabyCachedData[3 * (j * nReqXSize + i) + nBand - 1];
            }
        }
    }
    else if (sTile.nBands == 1)
    {
        for (int j = 0; j < nReqYSize; j++)
        {
            for (int i = 0; i < nReqXSize; i++)
            {
                pabyData[j * nBlockXSize + i] =
                    poGDS->m_pabyCachedData[j * nReqXSize + i];
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                     GetSuggestedBlockAccessPattern()                 */
/************************************************************************/

GDALSuggestedBlockAccessPattern
PDFRasterBand::GetSuggestedBlockAccessPattern() const
{
    PDFDataset *poGDS = cpl::down_cast<PDFDataset *>(poDS);
    if (!poGDS->m_aiTiles.empty())
        return GSBAP_RANDOM;
    return GSBAP_LARGEST_CHUNK_POSSIBLE;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr PDFRasterBand::IReadBlock(int nBlockXOff, int nBlockYOff, void *pImage)

{
    PDFDataset *poGDS = (PDFDataset *)poDS;

    if (!poGDS->m_aiTiles.empty())
    {
        if (IReadBlockFromTile(nBlockXOff, nBlockYOff, pImage) == CE_None)
        {
            return CE_None;
        }
        else
        {
            poGDS->m_aiTiles.resize(0);
            poGDS->m_bTried = false;
            CPLFree(poGDS->m_pabyCachedData);
            poGDS->m_pabyCachedData = nullptr;
            poGDS->m_nLastBlockXOff = -1;
            poGDS->m_nLastBlockYOff = -1;
        }
    }

    int nReqXSize = nBlockXSize;
    int nReqYSize = nBlockYSize;
    if ((nBlockXOff + 1) * nBlockXSize > nRasterXSize)
        nReqXSize = nRasterXSize - nBlockXOff * nBlockXSize;
    if (nBlockYSize == 1)
        nReqYSize = nRasterYSize;
    else if ((nBlockYOff + 1) * nBlockYSize > nRasterYSize)
        nReqYSize = nRasterYSize - nBlockYOff * nBlockYSize;

    if (!poGDS->m_bTried)
    {
        poGDS->m_bTried = true;
        if (nBlockYSize == 1)
            poGDS->m_pabyCachedData = static_cast<GByte *>(VSIMalloc3(
                std::max(3, poGDS->nBands), nRasterXSize, nRasterYSize));
        else
            poGDS->m_pabyCachedData = static_cast<GByte *>(VSIMalloc3(
                std::max(3, poGDS->nBands), nBlockXSize, nBlockYSize));
    }
    if (poGDS->m_pabyCachedData == nullptr)
        return CE_Failure;

    if (poGDS->m_nLastBlockXOff == nBlockXOff &&
        (nBlockYSize == 1 || poGDS->m_nLastBlockYOff == nBlockYOff) &&
        poGDS->m_pabyCachedData != nullptr)
    {
        /*CPLDebug("PDF", "Using cached block (%d, %d)",
                 nBlockXOff, nBlockYOff);*/
        // do nothing
    }
    else
    {
#ifdef HAVE_PODOFO
        if (poGDS->m_bUseLib.test(PDFLIB_PODOFO) && nBand == 4)
        {
            memset(pImage, 255, nBlockXSize * nBlockYSize);
            return CE_None;
        }
#endif

        const int nReqXOff = nBlockXOff * nBlockXSize;
        const int nReqYOff = (nBlockYSize == 1) ? 0 : nBlockYOff * nBlockYSize;
        const GSpacing nPixelSpace = 1;
        const GSpacing nLineSpace = nBlockXSize;
        const GSpacing nBandSpace =
            static_cast<GSpacing>(nBlockXSize) *
            ((nBlockYSize == 1) ? nRasterYSize : nBlockYSize);

        CPLErr eErr = poGDS->ReadPixels(nReqXOff, nReqYOff, nReqXSize,
                                        nReqYSize, nPixelSpace, nLineSpace,
                                        nBandSpace, poGDS->m_pabyCachedData);

        if (eErr == CE_None)
        {
            poGDS->m_nLastBlockXOff = nBlockXOff;
            poGDS->m_nLastBlockYOff = nBlockYOff;
        }
        else
        {
            CPLFree(poGDS->m_pabyCachedData);
            poGDS->m_pabyCachedData = nullptr;
        }
    }
    if (poGDS->m_pabyCachedData == nullptr)
        return CE_Failure;

    if (nBlockYSize == 1)
        memcpy(pImage,
               poGDS->m_pabyCachedData +
                   (nBand - 1) * nBlockXSize * nRasterYSize +
                   nBlockYOff * nBlockXSize,
               nBlockXSize);
    else
    {
        memcpy(pImage,
               poGDS->m_pabyCachedData +
                   static_cast<size_t>(nBand - 1) * nBlockXSize * nBlockYSize,
               static_cast<size_t>(nBlockXSize) * nBlockYSize);

        if (poGDS->m_bCacheBlocksForOtherBands && nBand == 1)
        {
            for (int iBand = 2; iBand <= poGDS->nBands; ++iBand)
            {
                auto poOtherBand = cpl::down_cast<PDFRasterBand *>(
                    poGDS->papoBands[iBand - 1]);
                GDALRasterBlock *poBlock =
                    poOtherBand->TryGetLockedBlockRef(nBlockXOff, nBlockYOff);
                if (poBlock)
                {
                    poBlock->DropLock();
                }
                else
                {
                    poBlock = poOtherBand->GetLockedBlockRef(nBlockXOff,
                                                             nBlockYOff, TRUE);
                    if (poBlock)
                    {
                        memcpy(poBlock->GetDataRef(),
                               poGDS->m_pabyCachedData +
                                   static_cast<size_t>(iBand - 1) *
                                       nBlockXSize * nBlockYSize,
                               static_cast<size_t>(nBlockXSize) * nBlockYSize);
                        poBlock->DropLock();
                    }
                }
            }
        }
    }

    return CE_None;
}

/************************************************************************/
/*                    PDFEnterPasswordFromConsoleIfNeeded()             */
/************************************************************************/

static const char *PDFEnterPasswordFromConsoleIfNeeded(const char *pszUserPwd)
{
    if (EQUAL(pszUserPwd, "ASK_INTERACTIVE"))
    {
        static char szPassword[81];
        printf("Enter password (will be echo'ed in the console): "); /*ok*/
        if (nullptr == fgets(szPassword, sizeof(szPassword), stdin))
        {
            fprintf(stderr, "WARNING: Error getting password.\n"); /*ok*/
        }
        szPassword[sizeof(szPassword) - 1] = 0;
        char *sz10 = strchr(szPassword, '\n');
        if (sz10)
            *sz10 = 0;
        return szPassword;
    }
    return pszUserPwd;
}

#ifdef HAVE_PDFIUM

/************************************************************************/
/*                         Pdfium Load/Unload                           */
/* Copyright (C) 2015 Klokan Technologies GmbH (http://www.klokantech.com/) */
/* Author: Martin Mikita <martin.mikita@klokantech.com>                 */
/************************************************************************/

// Flag for calling PDFium Init and Destroy methods
bool PDFDataset::g_bPdfiumInit = false;

// Pdfium global read mutex - Pdfium is not multi-thread
static CPLMutex *g_oPdfiumReadMutex = nullptr;
static CPLMutex *g_oPdfiumLoadDocMutex = nullptr;

// Comparison of char* for std::map
struct cmp_str
{
    bool operator()(char const *a, char const *b) const
    {
        return strcmp(a, b) < 0;
    }
};

static int GDALPdfiumGetBlock(void *param, unsigned long position,
                              unsigned char *pBuf, unsigned long size)
{
    VSILFILE *fp = (VSILFILE *)param;
    VSIFSeekL(fp, position, SEEK_SET);
    return VSIFReadL(pBuf, size, 1, fp) == 1;
}

// List of all PDF datasets
typedef std::map<char *, TPdfiumDocumentStruct *, cmp_str> TMapPdfiumDatasets;
static TMapPdfiumDatasets g_mPdfiumDatasets;

/**
 * Loading PDFIUM page
 * - multithreading requires "mutex"
 * - one page can require too much RAM
 * - we will have one document per filename and one object per page
 */

static int LoadPdfiumDocumentPage(const char *pszFilename,
                                  const char *pszUserPwd, int pageNum,
                                  TPdfiumDocumentStruct **doc,
                                  TPdfiumPageStruct **page, int *pnPageCount)
{
    // Prepare nullptr for error returning
    if (doc)
        *doc = nullptr;
    if (page)
        *page = nullptr;
    if (pnPageCount)
        *pnPageCount = 0;

    // Loading document and page must be only in one thread!
    CPLCreateOrAcquireMutex(&g_oPdfiumLoadDocMutex, PDFIUM_MUTEX_TIMEOUT);

    // Library can be destroyed if every PDF dataset was closed!
    if (!PDFDataset::g_bPdfiumInit)
    {
        FPDF_InitLibrary();
        PDFDataset::g_bPdfiumInit = TRUE;
    }

    TMapPdfiumDatasets::iterator it;
    it = g_mPdfiumDatasets.find((char *)pszFilename);
    TPdfiumDocumentStruct *poDoc = nullptr;
    // Load new document if missing
    if (it == g_mPdfiumDatasets.end())
    {
        // Try without password (if PDF not requires password it can fail)

        VSILFILE *fp = VSIFOpenL(pszFilename, "rb");
        if (fp == nullptr)
        {
            CPLReleaseMutex(g_oPdfiumLoadDocMutex);
            return FALSE;
        }
        VSIFSeekL(fp, 0, SEEK_END);
        unsigned long nFileLen = (unsigned long)VSIFTellL(fp);
        if (nFileLen != VSIFTellL(fp))
        {
            VSIFCloseL(fp);
            CPLReleaseMutex(g_oPdfiumLoadDocMutex);
            return FALSE;
        }

        FPDF_FILEACCESS *psFileAccess = new FPDF_FILEACCESS;
        psFileAccess->m_Param = fp;
        psFileAccess->m_FileLen = nFileLen;
        psFileAccess->m_GetBlock = GDALPdfiumGetBlock;
        CPDF_Document *docPdfium = CPDFDocumentFromFPDFDocument(
            FPDF_LoadCustomDocument(psFileAccess, nullptr));
        if (docPdfium == nullptr)
        {
            unsigned long err = FPDF_GetLastError();
            if (err == FPDF_ERR_PASSWORD)
            {
                if (pszUserPwd)
                {
                    pszUserPwd =
                        PDFEnterPasswordFromConsoleIfNeeded(pszUserPwd);
                    docPdfium = CPDFDocumentFromFPDFDocument(
                        FPDF_LoadCustomDocument(psFileAccess, pszUserPwd));
                    if (docPdfium == nullptr)
                        err = FPDF_GetLastError();
                    else
                        err = FPDF_ERR_SUCCESS;
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "A password is needed. You can specify it through "
                             "the PDF_USER_PWD "
                             "configuration option / USER_PWD open option "
                             "(that can be set to ASK_INTERACTIVE)");

                    VSIFCloseL(fp);
                    delete psFileAccess;
                    CPLReleaseMutex(g_oPdfiumLoadDocMutex);
                    return FALSE;
                }
            }  // First Error Password [null password given]
            if (err != FPDF_ERR_SUCCESS)
            {
                if (err == FPDF_ERR_PASSWORD)
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "PDFium Invalid password.");
                else if (err == FPDF_ERR_SECURITY)
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "PDFium Unsupported security scheme.");
                else if (err == FPDF_ERR_FORMAT)
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "PDFium File not in PDF format or corrupted.");
                else if (err == FPDF_ERR_FILE)
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "PDFium File not found or could not be opened.");
                else
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "PDFium Unknown PDF error or invalid PDF.");

                VSIFCloseL(fp);
                delete psFileAccess;
                CPLReleaseMutex(g_oPdfiumLoadDocMutex);
                return FALSE;
            }
        }  // ~ wrong PDF or password required

        // Create new poDoc
        poDoc = new TPdfiumDocumentStruct;
        if (!poDoc)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Not enough memory for Pdfium Document object");

            VSIFCloseL(fp);
            delete psFileAccess;
            CPLReleaseMutex(g_oPdfiumLoadDocMutex);
            return FALSE;
        }
        poDoc->filename = CPLStrdup(pszFilename);
        poDoc->doc = docPdfium;
        poDoc->psFileAccess = psFileAccess;

        g_mPdfiumDatasets[poDoc->filename] = poDoc;
    }
    // Document already loaded
    else
    {
        poDoc = it->second;
    }

    // Check page num in document
    int nPages = poDoc->doc->GetPageCount();
    if (pageNum < 1 || pageNum > nPages)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "PDFium Invalid page number (%d/%d) for document %s", pageNum,
                 nPages, pszFilename);

        CPLReleaseMutex(g_oPdfiumLoadDocMutex);
        return FALSE;
    }

    /* Sanity check to validate page count */
    if (pageNum != nPages)
    {
        if (poDoc->doc->GetPageDictionary(nPages - 1) == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid PDF : invalid page count");
            CPLReleaseMutex(g_oPdfiumLoadDocMutex);
            return FALSE;
        }
    }

    TMapPdfiumPages::iterator itPage;
    itPage = poDoc->pages.find(pageNum);
    TPdfiumPageStruct *poPage = nullptr;
    // Page not loaded
    if (itPage == poDoc->pages.end())
    {
        auto pDict = poDoc->doc->GetPageDictionary(pageNum - 1);
        if (pDict == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid PDFium : invalid page");

            CPLReleaseMutex(g_oPdfiumLoadDocMutex);
            return FALSE;
        }
        auto pPage = pdfium::MakeRetain<CPDF_Page>(
            poDoc->doc,
            // coverity is confused by WrapRetain(), believing that multiple
            // smart pointers manage the same raw pointer. Which is actually
            // true, but a RetainPtr holds a reference counted object. It is
            // thus safe to have several RetainPtr holding it.
            // coverity[multiple_init_smart_ptr]
            pdfium::WrapRetain(const_cast<CPDF_Dictionary *>(pDict.Get())));

        poPage = new TPdfiumPageStruct;
        if (!poPage)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Not enough memory for Pdfium Page object");

            CPLReleaseMutex(g_oPdfiumLoadDocMutex);
            return FALSE;
        }
        poPage->pageNum = pageNum;
        poPage->page = pPage.Leak();
        poPage->readMutex = nullptr;
        poPage->sharedNum = 0;

        poDoc->pages[pageNum] = poPage;
    }
    // Page already loaded
    else
    {
        poPage = itPage->second;
    }

    // Increase number of used
    ++poPage->sharedNum;

    if (doc)
        *doc = poDoc;
    if (page)
        *page = poPage;
    if (pnPageCount)
        *pnPageCount = nPages;

    CPLReleaseMutex(g_oPdfiumLoadDocMutex);

    return TRUE;
}

// ~ static int LoadPdfiumDocumentPage()

static int UnloadPdfiumDocumentPage(TPdfiumDocumentStruct **doc,
                                    TPdfiumPageStruct **page)
{
    if (!doc || !page)
        return FALSE;

    TPdfiumPageStruct *pPage = *page;
    TPdfiumDocumentStruct *pDoc = *doc;

    // Get mutex for loading pdfium
    CPLCreateOrAcquireMutex(&g_oPdfiumLoadDocMutex, PDFIUM_MUTEX_TIMEOUT);

    // Decrease page use
    --pPage->sharedNum;

#ifdef DEBUG
    CPLDebug("PDF", "PDFDataset::UnloadPdfiumDocumentPage: page shared num %d",
             pPage->sharedNum);
#endif
    // Page is used (also document)
    if (pPage->sharedNum != 0)
    {
        CPLReleaseMutex(g_oPdfiumLoadDocMutex);
        return TRUE;
    }

    // Get mutex, release and destroy it
    CPLCreateOrAcquireMutex(&(pPage->readMutex), PDFIUM_MUTEX_TIMEOUT);
    CPLReleaseMutex(pPage->readMutex);
    CPLDestroyMutex(pPage->readMutex);
    // Close page and remove from map
    FPDF_ClosePage(FPDFPageFromIPDFPage(pPage->page));

    pDoc->pages.erase(pPage->pageNum);
    delete pPage;
    pPage = nullptr;

#ifdef DEBUG
    CPLDebug("PDF", "PDFDataset::UnloadPdfiumDocumentPage: pages %lu",
             pDoc->pages.size());
#endif
    // Another page is used
    if (!pDoc->pages.empty())
    {
        CPLReleaseMutex(g_oPdfiumLoadDocMutex);
        return TRUE;
    }

    // Close document and remove from map
    FPDF_CloseDocument(FPDFDocumentFromCPDFDocument(pDoc->doc));
    g_mPdfiumDatasets.erase(pDoc->filename);
    CPLFree(pDoc->filename);
    VSIFCloseL((VSILFILE *)pDoc->psFileAccess->m_Param);
    delete pDoc->psFileAccess;
    delete pDoc;
    pDoc = nullptr;

#ifdef DEBUG
    CPLDebug("PDF", "PDFDataset::UnloadPdfiumDocumentPage: documents %lu",
             g_mPdfiumDatasets.size());
#endif
    // Another document is used
    if (!g_mPdfiumDatasets.empty())
    {
        CPLReleaseMutex(g_oPdfiumLoadDocMutex);
        return TRUE;
    }

#ifdef DEBUG
    CPLDebug("PDF", "PDFDataset::UnloadPdfiumDocumentPage: Nothing loaded, "
                    "destroy Library");
#endif
    // No document loaded, destroy pdfium
    FPDF_DestroyLibrary();
    PDFDataset::g_bPdfiumInit = FALSE;

    CPLReleaseMutex(g_oPdfiumLoadDocMutex);

    return TRUE;
}

// ~ static int UnloadPdfiumDocumentPage()

#endif  // ~ HAVE_PDFIUM

/************************************************************************/
/*                             GetOption()                              */
/************************************************************************/

const char *PDFDataset::GetOption(char **papszOpenOptionsIn,
                                  const char *pszOptionName,
                                  const char *pszDefaultVal)
{
    CPLErr eLastErrType = CPLGetLastErrorType();
    CPLErrorNum nLastErrno = CPLGetLastErrorNo();
    CPLString osLastErrorMsg(CPLGetLastErrorMsg());
    CPLXMLNode *psNode = CPLParseXMLString(PDFGetOpenOptionList());
    CPLErrorSetState(eLastErrType, nLastErrno, osLastErrorMsg);
    if (psNode == nullptr)
        return pszDefaultVal;
    CPLXMLNode *psIter = psNode->psChild;
    while (psIter != nullptr)
    {
        if (EQUAL(CPLGetXMLValue(psIter, "name", ""), pszOptionName))
        {
            const char *pszVal =
                CSLFetchNameValue(papszOpenOptionsIn, pszOptionName);
            if (pszVal != nullptr)
            {
                CPLDestroyXMLNode(psNode);
                return pszVal;
            }
            const char *pszAltConfigOption =
                CPLGetXMLValue(psIter, "alt_config_option", nullptr);
            if (pszAltConfigOption != nullptr)
            {
                pszVal = CPLGetConfigOption(pszAltConfigOption, pszDefaultVal);
                CPLDestroyXMLNode(psNode);
                return pszVal;
            }
            CPLDestroyXMLNode(psNode);
            return pszDefaultVal;
        }
        psIter = psIter->psNext;
    }
    CPLError(CE_Failure, CPLE_AppDefined,
             "Requesting an undocumented open option '%s'", pszOptionName);
    CPLDestroyXMLNode(psNode);
    return pszDefaultVal;
}

#ifdef HAVE_PDFIUM

/************************************************************************/
/*                         GDALPDFiumOCContext                          */
/************************************************************************/

class GDALPDFiumOCContext : public CPDF_OCContextInterface
{
    PDFDataset *m_poDS;
    RetainPtr<CPDF_OCContext> m_DefaultOCContext;

  public:
    GDALPDFiumOCContext(PDFDataset *poDS, CPDF_Document *pDoc,
                        CPDF_OCContext::UsageType usage)
        : m_poDS(poDS),
          m_DefaultOCContext(pdfium::MakeRetain<CPDF_OCContext>(pDoc, usage))
    {
    }

    virtual bool
    CheckOCGDictVisible(const CPDF_Dictionary *pOCGDict) const override
    {
        // CPLDebug("PDF", "CheckOCGDictVisible(%d,%d)",
        //          pOCGDict->GetObjNum(), pOCGDict->GetGenNum() );
        PDFDataset::VisibilityState eVisibility =
            m_poDS->GetVisibilityStateForOGCPdfium(pOCGDict->GetObjNum(),
                                                   pOCGDict->GetGenNum());
        if (eVisibility == PDFDataset::VISIBILITY_ON)
            return true;
        if (eVisibility == PDFDataset::VISIBILITY_OFF)
            return false;
        return m_DefaultOCContext->CheckOCGDictVisible(pOCGDict);
    }
};

/************************************************************************/
/*                      GDALPDFiumRenderDeviceDriver                    */
/************************************************************************/

class GDALPDFiumRenderDeviceDriver : public RenderDeviceDriverIface
{
    std::unique_ptr<RenderDeviceDriverIface> m_poParent;
    CFX_RenderDevice *m_pDevice;

    int bEnableVector;
    int bEnableText;
    int bEnableBitmap;
    int bTemporaryEnableVectorForTextStroking;

  public:
    GDALPDFiumRenderDeviceDriver(
        std::unique_ptr<RenderDeviceDriverIface> &&poParent,
        CFX_RenderDevice *pDevice)
        : m_poParent(std::move(poParent)), m_pDevice(pDevice),
          bEnableVector(TRUE), bEnableText(TRUE), bEnableBitmap(TRUE),
          bTemporaryEnableVectorForTextStroking(FALSE)
    {
    }

    virtual ~GDALPDFiumRenderDeviceDriver() = default;

    void SetEnableVector(int bFlag)
    {
        bEnableVector = bFlag;
    }

    void SetEnableText(int bFlag)
    {
        bEnableText = bFlag;
    }

    void SetEnableBitmap(int bFlag)
    {
        bEnableBitmap = bFlag;
    }

    virtual DeviceType GetDeviceType() const override
    {
        return m_poParent->GetDeviceType();
    }

    virtual int GetDeviceCaps(int caps_id) const override
    {
        return m_poParent->GetDeviceCaps(caps_id);
    }

    virtual void SaveState() override
    {
        m_poParent->SaveState();
    }

    virtual void RestoreState(bool bKeepSaved) override
    {
        m_poParent->RestoreState(bKeepSaved);
    }

    virtual void SetBaseClip(const FX_RECT &rect) override
    {
        m_poParent->SetBaseClip(rect);
    }

    virtual bool
    SetClip_PathFill(const CFX_Path &path, const CFX_Matrix *pObject2Device,
                     const CFX_FillRenderOptions &fill_options) override
    {
        if (!bEnableVector && !bTemporaryEnableVectorForTextStroking)
            return true;
        return m_poParent->SetClip_PathFill(path, pObject2Device, fill_options);
    }

    virtual bool
    SetClip_PathStroke(const CFX_Path &path, const CFX_Matrix *pObject2Device,
                       const CFX_GraphStateData *pGraphState) override
    {
        if (!bEnableVector && !bTemporaryEnableVectorForTextStroking)
            return true;
        return m_poParent->SetClip_PathStroke(path, pObject2Device,
                                              pGraphState);
    }

    virtual bool DrawPath(const CFX_Path &path,
                          const CFX_Matrix *pObject2Device,
                          const CFX_GraphStateData *pGraphState,
                          uint32_t fill_color, uint32_t stroke_color,
                          const CFX_FillRenderOptions &fill_options,
                          BlendMode blend_type) override
    {
        if (!bEnableVector && !bTemporaryEnableVectorForTextStroking)
            return true;
        return m_poParent->DrawPath(path, pObject2Device, pGraphState,
                                    fill_color, stroke_color, fill_options,
                                    blend_type);
    }

    virtual bool FillRectWithBlend(const FX_RECT &rect, uint32_t fill_color,
                                   BlendMode blend_type) override
    {
        return m_poParent->FillRectWithBlend(rect, fill_color, blend_type);
    }

    virtual bool DrawCosmeticLine(const CFX_PointF &ptMoveTo,
                                  const CFX_PointF &ptLineTo, uint32_t color,
                                  BlendMode blend_typeL) override
    {
        if (!bEnableVector && !bTemporaryEnableVectorForTextStroking)
            return TRUE;
        return m_poParent->DrawCosmeticLine(ptMoveTo, ptLineTo, color,
                                            blend_typeL);
    }

    virtual bool GetClipBox(FX_RECT *pRect) override
    {
        return m_poParent->GetClipBox(pRect);
    }

    virtual bool GetDIBits(const RetainPtr<CFX_DIBitmap> &pBitmap, int left,
                           int top) override
    {
        return m_poParent->GetDIBits(pBitmap, left, top);
    }

    virtual RetainPtr<CFX_DIBitmap> GetBackDrop() override
    {
        return m_poParent->GetBackDrop();
    }

    virtual bool SetDIBits(RetainPtr<const CFX_DIBBase> bitmap, uint32_t color,
                           const FX_RECT &src_rect, int dest_left, int dest_top,
                           BlendMode blend_type) override
    {
        if (!bEnableBitmap && !bTemporaryEnableVectorForTextStroking)
            return true;
        return m_poParent->SetDIBits(std::move(bitmap), color, src_rect,
                                     dest_left, dest_top, blend_type);
    }

    virtual bool StretchDIBits(RetainPtr<const CFX_DIBBase> bitmap,
                               uint32_t color, int dest_left, int dest_top,
                               int dest_width, int dest_height,
                               const FX_RECT *pClipRect,
                               const FXDIB_ResampleOptions &options,
                               BlendMode blend_type) override
    {
        if (!bEnableBitmap && !bTemporaryEnableVectorForTextStroking)
            return true;
        return m_poParent->StretchDIBits(std::move(bitmap), color, dest_left,
                                         dest_top, dest_width, dest_height,
                                         pClipRect, options, blend_type);
    }

    virtual bool StartDIBits(RetainPtr<const CFX_DIBBase> bitmap, float alpha,
                             uint32_t color, const CFX_Matrix &matrix,
                             const FXDIB_ResampleOptions &options,
                             std::unique_ptr<CFX_ImageRenderer> *handle,
                             BlendMode blend_type) override
    {
        if (!bEnableBitmap && !bTemporaryEnableVectorForTextStroking)
            return true;
        return m_poParent->StartDIBits(std::move(bitmap), alpha, color, matrix,
                                       options, handle, blend_type);
    }

    virtual bool ContinueDIBits(CFX_ImageRenderer *handle,
                                PauseIndicatorIface *pPause) override
    {
        return m_poParent->ContinueDIBits(handle, pPause);
    }

    virtual bool DrawDeviceText(const pdfium::span<const TextCharPos> &pCharPos,
                                CFX_Font *pFont,
                                const CFX_Matrix &mtObject2Device,
                                float font_size, uint32_t color,
                                const CFX_TextRenderOptions &options) override
    {
        if (bEnableText)
        {
            // This is quite tricky. We call again the guy who called us
            // (CFX_RenderDevice::DrawNormalText()) but we set a special flag to
            // allow vector&raster operations so that the rendering will happen
            // in the next phase
            if (bTemporaryEnableVectorForTextStroking)
                return FALSE;  // this is the default behavior of the parent
            bTemporaryEnableVectorForTextStroking = true;
            bool bRet = m_pDevice->DrawNormalText(
                pCharPos, pFont, font_size, mtObject2Device, color, options);
            bTemporaryEnableVectorForTextStroking = FALSE;
            return bRet;
        }
        else
            return true;  // pretend that we did the job
    }

    virtual int GetDriverType() const override
    {
        return m_poParent->GetDriverType();
    }

    virtual bool DrawShading(const CPDF_ShadingPattern *pPattern,
                             const CFX_Matrix *pMatrix,
                             const FX_RECT &clip_rect, int alpha,
                             bool bAlphaMode) override
    {
        if (!bEnableBitmap && !bTemporaryEnableVectorForTextStroking)
            return true;
        return m_poParent->DrawShading(pPattern, pMatrix, clip_rect, alpha,
                                       bAlphaMode);
    }

    bool MultiplyAlpha(float alpha) override
    {
        return m_poParent->MultiplyAlpha(alpha);
    }

    bool MultiplyAlphaMask(RetainPtr<const CFX_DIBitmap> mask) override
    {
        return m_poParent->MultiplyAlphaMask(std::move(mask));
    }

#if defined(_SKIA_SUPPORT_)
    virtual bool SetBitsWithMask(RetainPtr<const CFX_DIBBase> bitmap,
                                 RetainPtr<const CFX_DIBBase> mask, int left,
                                 int top, float alpha,
                                 BlendMode blend_type) override
    {
        if (!bEnableBitmap && !bTemporaryEnableVectorForTextStroking)
            return true;
        return m_poParent->SetBitsWithMask(bitmap, mask, left, top, alpha,
                                           blend_type);
    }

    virtual void SetGroupKnockout(bool group_knockout) override
    {
        m_poParent->SetGroupKnockout(group_knockout);
    }
#endif
#if defined _SKIA_SUPPORT_ || defined _SKIA_SUPPORT_PATHS_
    virtual void Flush() override
    {
        return m_poParent->Flush();
    }
#endif
};

/************************************************************************/
/*                         PDFiumRenderPageBitmap()                     */
/************************************************************************/

/* This method is a customization of FPDF_RenderPageBitmap()
   from pdfium/fpdfsdk/fpdf_view.cpp to allow selection of which OGC/layer are
   active. Thus it inherits the following license */
// Copyright 2014 PDFium Authors. All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//    * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//    * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//    * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

static void myRenderPageImpl(PDFDataset *poDS, CPDF_PageRenderContext *pContext,
                             CPDF_Page *pPage, const CFX_Matrix &matrix,
                             const FX_RECT &clipping_rect, int flags,
                             const FPDF_COLORSCHEME *color_scheme,
                             bool bNeedToRestore, CPDFSDK_PauseAdapter *pause)
{
    if (!pContext->m_pOptions)
        pContext->m_pOptions = std::make_unique<CPDF_RenderOptions>();

    auto &options = pContext->m_pOptions->GetOptions();
    options.bClearType = !!(flags & FPDF_LCD_TEXT);
    options.bNoNativeText = !!(flags & FPDF_NO_NATIVETEXT);
    options.bLimitedImageCache = !!(flags & FPDF_RENDER_LIMITEDIMAGECACHE);
    options.bForceHalftone = !!(flags & FPDF_RENDER_FORCEHALFTONE);
    options.bNoTextSmooth = !!(flags & FPDF_RENDER_NO_SMOOTHTEXT);
    options.bNoImageSmooth = !!(flags & FPDF_RENDER_NO_SMOOTHIMAGE);
    options.bNoPathSmooth = !!(flags & FPDF_RENDER_NO_SMOOTHPATH);

    // Grayscale output
    if (flags & FPDF_GRAYSCALE)
        pContext->m_pOptions->SetColorMode(CPDF_RenderOptions::kGray);

    if (color_scheme)
    {
        pContext->m_pOptions->SetColorMode(CPDF_RenderOptions::kForcedColor);
        SetColorFromScheme(color_scheme, pContext->m_pOptions.get());
        options.bConvertFillToStroke = !!(flags & FPDF_CONVERT_FILL_TO_STROKE);
    }

    const CPDF_OCContext::UsageType usage = (flags & FPDF_PRINTING)
                                                ? CPDF_OCContext::kPrint
                                                : CPDF_OCContext::kView;
    pContext->m_pOptions->SetOCContext(pdfium::MakeRetain<GDALPDFiumOCContext>(
        poDS, pPage->GetDocument(), usage));

    pContext->m_pDevice->SaveState();
    pContext->m_pDevice->SetBaseClip(clipping_rect);
    pContext->m_pDevice->SetClip_Rect(clipping_rect);
    pContext->m_pContext = std::make_unique<CPDF_RenderContext>(
        pPage->GetDocument(), pPage->GetMutablePageResources(),
        pPage->GetPageImageCache());

    pContext->m_pContext->AppendLayer(pPage, matrix);

    if (flags & FPDF_ANNOT)
    {
        auto pOwnedList = std::make_unique<CPDF_AnnotList>(pPage);
        CPDF_AnnotList *pList = pOwnedList.get();
        pContext->m_pAnnots = std::move(pOwnedList);
        bool bPrinting =
            pContext->m_pDevice->GetDeviceType() != DeviceType::kDisplay;

        // TODO(https://crbug.com/pdfium/993) - maybe pass true here.
        const bool bShowWidget = false;
        pList->DisplayAnnots(pPage, pContext->m_pDevice.get(),
                             pContext->m_pContext.get(), bPrinting, matrix,
                             bShowWidget);
    }

    pContext->m_pRenderer = std::make_unique<CPDF_ProgressiveRenderer>(
        pContext->m_pContext.get(), pContext->m_pDevice.get(),
        pContext->m_pOptions.get());
    pContext->m_pRenderer->Start(pause);
    if (bNeedToRestore)
        pContext->m_pDevice->RestoreState(false);
}

static void
myRenderPageWithContext(PDFDataset *poDS, CPDF_PageRenderContext *pContext,
                        FPDF_PAGE page, int start_x, int start_y, int size_x,
                        int size_y, int rotate, int flags,
                        const FPDF_COLORSCHEME *color_scheme,
                        bool bNeedToRestore, CPDFSDK_PauseAdapter *pause)
{
    CPDF_Page *pPage = CPDFPageFromFPDFPage(page);
    if (!pPage)
        return;

    const FX_RECT rect(start_x, start_y, start_x + size_x, start_y + size_y);
    myRenderPageImpl(poDS, pContext, pPage,
                     pPage->GetDisplayMatrix(rect, rotate), rect, flags,
                     color_scheme, bNeedToRestore, pause);
}

class MyRenderDevice final : public CFX_RenderDevice
{

  public:
    // Substitution for CFX_DefaultRenderDevice::Attach
    bool Attach(const RetainPtr<CFX_DIBitmap> &pBitmap, bool bRgbByteOrder,
                const RetainPtr<CFX_DIBitmap> &pBackdropBitmap,
                bool bGroupKnockout, const char *pszRenderingOptions);
};

bool MyRenderDevice::Attach(const RetainPtr<CFX_DIBitmap> &pBitmap,
                            bool bRgbByteOrder,
                            const RetainPtr<CFX_DIBitmap> &pBackdropBitmap,
                            bool bGroupKnockout,
                            const char *pszRenderingOptions)
{
    SetBitmap(pBitmap);

    std::unique_ptr<RenderDeviceDriverIface> driver =
        std::make_unique<pdfium::CFX_AggDeviceDriver>(
            pBitmap, bRgbByteOrder, pBackdropBitmap, bGroupKnockout);
    if (pszRenderingOptions != nullptr)
    {
        int bEnableVector = FALSE;
        int bEnableText = FALSE;
        int bEnableBitmap = FALSE;

        char **papszTokens = CSLTokenizeString2(pszRenderingOptions, " ,", 0);
        for (int i = 0; papszTokens[i] != nullptr; i++)
        {
            if (EQUAL(papszTokens[i], "VECTOR"))
                bEnableVector = TRUE;
            else if (EQUAL(papszTokens[i], "TEXT"))
                bEnableText = TRUE;
            else if (EQUAL(papszTokens[i], "RASTER") ||
                     EQUAL(papszTokens[i], "BITMAP"))
                bEnableBitmap = TRUE;
            else
            {
                CPLError(CE_Warning, CPLE_NotSupported,
                         "Value %s is not a valid value for "
                         "GDAL_PDF_RENDERING_OPTIONS",
                         papszTokens[i]);
            }
        }
        CSLDestroy(papszTokens);

        if (!bEnableVector || !bEnableText || !bEnableBitmap)
        {
            std::unique_ptr<GDALPDFiumRenderDeviceDriver> poGDALRDDriver =
                std::make_unique<GDALPDFiumRenderDeviceDriver>(
                    std::move(driver), this);
            poGDALRDDriver->SetEnableVector(bEnableVector);
            poGDALRDDriver->SetEnableText(bEnableText);
            poGDALRDDriver->SetEnableBitmap(bEnableBitmap);
            driver = std::move(poGDALRDDriver);
        }
    }

    SetDeviceDriver(std::move(driver));
    return true;
}

void PDFDataset::PDFiumRenderPageBitmap(FPDF_BITMAP bitmap, FPDF_PAGE page,
                                        int start_x, int start_y, int size_x,
                                        int size_y,
                                        const char *pszRenderingOptions)
{
    const int rotate = 0;
    const int flags = 0;

    if (!bitmap)
        return;

    CPDF_Page *pPage = CPDFPageFromFPDFPage(page);
    if (!pPage)
        return;

    auto pOwnedContext = std::make_unique<CPDF_PageRenderContext>();
    CPDF_PageRenderContext *pContext = pOwnedContext.get();
    CPDF_Page::RenderContextClearer clearer(pPage);
    pPage->SetRenderContext(std::move(pOwnedContext));

    auto pOwnedDevice = std::make_unique<MyRenderDevice>();
    auto pDevice = pOwnedDevice.get();
    pContext->m_pDevice = std::move(pOwnedDevice);

    RetainPtr<CFX_DIBitmap> pBitmap(CFXDIBitmapFromFPDFBitmap(bitmap));

    pDevice->Attach(pBitmap, !!(flags & FPDF_REVERSE_BYTE_ORDER), nullptr,
                    false, pszRenderingOptions);

    myRenderPageWithContext(this, pContext, page, start_x, start_y, size_x,
                            size_y, rotate, flags,
                            /*color_scheme=*/nullptr,
                            /*need_to_restore=*/true, /*pause=*/nullptr);

#ifdef _SKIA_SUPPORT_PATHS_
    pDevice->Flush(true);
    pBitmap->UnPreMultiply();
#endif
}

#endif /* HAVE_PDFIUM */

/************************************************************************/
/*                             ReadPixels()                             */
/************************************************************************/

CPLErr PDFDataset::ReadPixels(int nReqXOff, int nReqYOff, int nReqXSize,
                              int nReqYSize, GSpacing nPixelSpace,
                              GSpacing nLineSpace, GSpacing nBandSpace,
                              GByte *pabyData)
{
    CPLErr eErr = CE_None;
    const char *pszRenderingOptions =
        GetOption(papszOpenOptions, "RENDERING_OPTIONS", nullptr);

#ifdef HAVE_POPPLER
    if (m_bUseLib.test(PDFLIB_POPPLER))
    {
        SplashColor sColor;
        sColor[0] = 255;
        sColor[1] = 255;
        sColor[2] = 255;
        GDALPDFOutputDev *poSplashOut = new GDALPDFOutputDev(
            (nBands < 4) ? splashModeRGB8 : splashModeXBGR8, 4, false,
            (nBands < 4) ? sColor : nullptr);

        if (pszRenderingOptions != nullptr)
        {
            poSplashOut->SetEnableVector(FALSE);
            poSplashOut->SetEnableText(FALSE);
            poSplashOut->SetEnableBitmap(FALSE);

            char **papszTokens =
                CSLTokenizeString2(pszRenderingOptions, " ,", 0);
            for (int i = 0; papszTokens[i] != nullptr; i++)
            {
                if (EQUAL(papszTokens[i], "VECTOR"))
                    poSplashOut->SetEnableVector(TRUE);
                else if (EQUAL(papszTokens[i], "TEXT"))
                    poSplashOut->SetEnableText(TRUE);
                else if (EQUAL(papszTokens[i], "RASTER") ||
                         EQUAL(papszTokens[i], "BITMAP"))
                    poSplashOut->SetEnableBitmap(TRUE);
                else
                {
                    CPLError(CE_Warning, CPLE_NotSupported,
                             "Value %s is not a valid value for "
                             "GDAL_PDF_RENDERING_OPTIONS",
                             papszTokens[i]);
                }
            }
            CSLDestroy(papszTokens);
        }

        PDFDoc *poDoc = m_poDocPoppler;
        poSplashOut->startDoc(poDoc);

        /* EVIL: we modify a private member... */
        /* poppler (at least 0.12 and 0.14 versions) don't render correctly */
        /* some PDFs and display an error message 'Could not find a OCG with
         * Ref' */
        /* in those cases. This processing of optional content is an addition of
         */
        /* poppler in comparison to original xpdf, which hasn't the issue. All
         * in */
        /* all, nullifying optContent removes the error message and improves the
         * rendering */
        Catalog *poCatalog = poDoc->getCatalog();
        OCGs *poOldOCGs = poCatalog->optContent;
        if (!m_bUseOCG)
            poCatalog->optContent = nullptr;
        try
        {
            poDoc->displayPageSlice(poSplashOut, m_iPage, m_dfDPI, m_dfDPI, 0,
                                    TRUE, false, false, nReqXOff, nReqYOff,
                                    nReqXSize, nReqYSize);
        }
        catch (const std::exception &e)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "PDFDoc::displayPageSlice() failed with %s", e.what());
            /* Restore back */
            poCatalog->optContent = poOldOCGs;
            delete poSplashOut;
            return CE_Failure;
        }

        /* Restore back */
        poCatalog->optContent = poOldOCGs;

        SplashBitmap *poBitmap = poSplashOut->getBitmap();
        if (poBitmap->getWidth() != nReqXSize ||
            poBitmap->getHeight() != nReqYSize)
        {
            CPLError(
                CE_Failure, CPLE_AppDefined,
                "Bitmap decoded size (%dx%d) doesn't match raster size (%dx%d)",
                poBitmap->getWidth(), poBitmap->getHeight(), nReqXSize,
                nReqYSize);
            delete poSplashOut;
            return CE_Failure;
        }

        GByte *pabyDataR = pabyData;
        GByte *pabyDataG = pabyData + nBandSpace;
        GByte *pabyDataB = pabyData + 2 * nBandSpace;
        GByte *pabyDataA = pabyData + 3 * nBandSpace;
        GByte *pabySrc = poBitmap->getDataPtr();
        GByte *pabyAlphaSrc = (GByte *)poBitmap->getAlphaPtr();
        int i, j;
        for (j = 0; j < nReqYSize; j++)
        {
            for (i = 0; i < nReqXSize; i++)
            {
                if (nBands < 4)
                {
                    pabyDataR[i * nPixelSpace] = pabySrc[i * 3 + 0];
                    pabyDataG[i * nPixelSpace] = pabySrc[i * 3 + 1];
                    pabyDataB[i * nPixelSpace] = pabySrc[i * 3 + 2];
                }
                else
                {
                    pabyDataR[i * nPixelSpace] = pabySrc[i * 4 + 2];
                    pabyDataG[i * nPixelSpace] = pabySrc[i * 4 + 1];
                    pabyDataB[i * nPixelSpace] = pabySrc[i * 4 + 0];
                    pabyDataA[i * nPixelSpace] = pabyAlphaSrc[i];
                }
            }
            pabyDataR += nLineSpace;
            pabyDataG += nLineSpace;
            pabyDataB += nLineSpace;
            pabyDataA += nLineSpace;
            pabyAlphaSrc += poBitmap->getAlphaRowSize();
            pabySrc += poBitmap->getRowSize();
        }
        delete poSplashOut;
    }
#endif  // HAVE_POPPLER

#ifdef HAVE_PODOFO
    if (m_bUseLib.test(PDFLIB_PODOFO))
    {
        if (m_bPdfToPpmFailed)
            return CE_Failure;

        if (pszRenderingOptions != nullptr &&
            !EQUAL(pszRenderingOptions, "RASTER,VECTOR,TEXT"))
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "GDAL_PDF_RENDERING_OPTIONS only supported "
                     "when PDF lib is Poppler.");
        }

        CPLString osTmpFilename;
        int nRet;

#ifdef notdef
        int bUseSpawn =
            CPLTestBool(CPLGetConfigOption("GDAL_PDF_USE_SPAWN", "YES"));
        if (!bUseSpawn)
        {
            CPLString osCmd = CPLSPrintf(
                "pdftoppm -r %f -x %d -y %d -W %d -H %d -f %d -l %d \"%s\"",
                dfDPI, nReqXOff, nReqYOff, nReqXSize, nReqYSize, iPage, iPage,
                osFilename.c_str());

            if (!osUserPwd.empty())
            {
                osCmd += " -upw \"";
                osCmd += osUserPwd;
                osCmd += "\"";
            }

            CPLString osTmpFilenamePrefix = CPLGenerateTempFilename("pdf");
            osTmpFilename =
                CPLSPrintf("%s-%d.ppm", osTmpFilenamePrefix.c_str(), iPage);
            osCmd += CPLSPrintf(" \"%s\"", osTmpFilenamePrefix.c_str());

            CPLDebug("PDF", "Running '%s'", osCmd.c_str());
            nRet = CPLSystem(nullptr, osCmd.c_str());
        }
        else
#endif  // notdef
        {
            char **papszArgs = nullptr;
            papszArgs = CSLAddString(papszArgs, "pdftoppm");
            papszArgs = CSLAddString(papszArgs, "-r");
            papszArgs = CSLAddString(papszArgs, CPLSPrintf("%f", m_dfDPI));
            papszArgs = CSLAddString(papszArgs, "-x");
            papszArgs = CSLAddString(papszArgs, CPLSPrintf("%d", nReqXOff));
            papszArgs = CSLAddString(papszArgs, "-y");
            papszArgs = CSLAddString(papszArgs, CPLSPrintf("%d", nReqYOff));
            papszArgs = CSLAddString(papszArgs, "-W");
            papszArgs = CSLAddString(papszArgs, CPLSPrintf("%d", nReqXSize));
            papszArgs = CSLAddString(papszArgs, "-H");
            papszArgs = CSLAddString(papszArgs, CPLSPrintf("%d", nReqYSize));
            papszArgs = CSLAddString(papszArgs, "-f");
            papszArgs = CSLAddString(papszArgs, CPLSPrintf("%d", m_iPage));
            papszArgs = CSLAddString(papszArgs, "-l");
            papszArgs = CSLAddString(papszArgs, CPLSPrintf("%d", m_iPage));
            if (!m_osUserPwd.empty())
            {
                papszArgs = CSLAddString(papszArgs, "-upw");
                papszArgs = CSLAddString(papszArgs, m_osUserPwd.c_str());
            }
            papszArgs = CSLAddString(papszArgs, m_osFilename.c_str());

            osTmpFilename = CPLSPrintf("/vsimem/pdf/temp_%p.ppm", this);
            VSILFILE *fpOut = VSIFOpenL(osTmpFilename, "wb");
            if (fpOut != nullptr)
            {
                nRet = CPLSpawn(papszArgs, nullptr, fpOut, FALSE);
                VSIFCloseL(fpOut);
            }
            else
                nRet = -1;

            CSLDestroy(papszArgs);
        }

        if (nRet == 0)
        {
            GDALDataset *poDS =
                (GDALDataset *)GDALOpen(osTmpFilename, GA_ReadOnly);
            if (poDS)
            {
                if (poDS->GetRasterCount() == 3)
                {
                    eErr = poDS->RasterIO(GF_Read, 0, 0, nReqXSize, nReqYSize,
                                          pabyData, nReqXSize, nReqYSize,
                                          GDT_Byte, 3, nullptr, nPixelSpace,
                                          nLineSpace, nBandSpace, nullptr);
                }
                delete poDS;
            }
        }
        else
        {
            CPLDebug("PDF", "Ret code = %d", nRet);
            m_bPdfToPpmFailed = true;
            eErr = CE_Failure;
        }
        VSIUnlink(osTmpFilename);
    }
#endif  // HAVE_PODOFO
#ifdef HAVE_PDFIUM
    if (m_bUseLib.test(PDFLIB_PDFIUM))
    {
        if (!m_poPagePdfium)
        {
            return CE_Failure;
        }

        // Pdfium does not support multithreading
        CPLCreateOrAcquireMutex(&g_oPdfiumReadMutex, PDFIUM_MUTEX_TIMEOUT);

        CPLCreateOrAcquireMutex(&(m_poPagePdfium->readMutex),
                                PDFIUM_MUTEX_TIMEOUT);

        // Parsing content required before rastering
        // can takes too long for PDF with large number of objects/layers
        m_poPagePdfium->page->ParseContent();

        FPDF_BITMAP bitmap =
            FPDFBitmap_Create(nReqXSize, nReqYSize, nBands == 4 /*alpha*/);
        // As coded now, FPDFBitmap_Create cannot allocate more than 1 GB
        if (bitmap == nullptr)
        {
            // Release mutex - following code is thread-safe
            CPLReleaseMutex(m_poPagePdfium->readMutex);
            CPLReleaseMutex(g_oPdfiumReadMutex);

#ifdef notdef
            // If the requested area is not too small, then try subdividing
            if ((GIntBig)nReqXSize * nReqYSize * 4 > 1024 * 1024)
            {
#ifdef DEBUG
                CPLDebug(
                    "PDF",
                    "Subdividing PDFDataset::ReadPixels(%d, %d, %d, %d, "
                    "scaleFactor=%d)",
                    nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                    1 << ((PDFRasterBand *)GetRasterBand(1))->nResolutionLevel);
#endif
                if (nReqXSize >= nReqYSize)
                {
                    eErr = ReadPixels(nReqXOff, nReqYOff, nReqXSize / 2,
                                      nReqYSize, nPixelSpace, nLineSpace,
                                      nBandSpace, pabyData);
                    if (eErr == CE_None)
                    {
                        eErr = ReadPixels(
                            nReqXSize / 2, nReqYOff, nReqXSize - nReqXSize / 2,
                            nReqYSize, nPixelSpace, nLineSpace, nBandSpace,
                            pabyData + nPixelSpace * (nReqXSize / 2));
                    }
                }
                else
                {
                    eErr = ReadPixels(nReqXOff, nReqYOff, nReqXSize,
                                      nReqYSize - nReqYSize / 2, nPixelSpace,
                                      nLineSpace, nBandSpace, pabyData);
                    if (eErr == CE_None)
                    {
                        eErr =
                            ReadPixels(nReqXOff, nReqYSize / 2, nReqXSize,
                                       nReqYSize - nReqYSize / 2, nPixelSpace,
                                       nLineSpace, nBandSpace,
                                       pabyData + nLineSpace * (nReqYSize / 2));
                    }
                }
                return eErr;
            }
#endif

            CPLError(CE_Failure, CPLE_AppDefined,
                     "FPDFBitmap_Create(%d,%d) failed", nReqXSize, nReqYSize);

            return CE_Failure;
        }
        // alpha is 0% which is transported to FF if not alpha
        // Default background color is white
        FPDF_DWORD color = 0x00FFFFFF;  // A,R,G,B
        FPDFBitmap_FillRect(bitmap, 0, 0, nReqXSize, nReqYSize, color);

#ifdef DEBUG
        // start_x, start_y, size_x, size_y, rotate, flags
        CPLDebug("PDF",
                 "PDFDataset::ReadPixels(%d, %d, %d, %d, scaleFactor=%d)",
                 nReqXOff, nReqYOff, nReqXSize, nReqYSize,
                 1 << ((PDFRasterBand *)GetRasterBand(1))->nResolutionLevel);

        CPLDebug("PDF", "FPDF_RenderPageBitmap(%d, %d, %d, %d)", -nReqXOff,
                 -nReqYOff, nRasterXSize, nRasterYSize);
#endif

        // Part of PDF is render with -x, -y, page_width, page_height
        // (not requested size!)
        PDFiumRenderPageBitmap(
            bitmap, FPDFPageFromIPDFPage(m_poPagePdfium->page), -nReqXOff,
            -nReqYOff, nRasterXSize, nRasterYSize, pszRenderingOptions);

        int stride = FPDFBitmap_GetStride(bitmap);
        const GByte *buffer =
            reinterpret_cast<const GByte *>(FPDFBitmap_GetBuffer(bitmap));

        // Release mutex - following code is thread-safe
        CPLReleaseMutex(m_poPagePdfium->readMutex);
        CPLReleaseMutex(g_oPdfiumReadMutex);

        // Source data is B, G, R, unused.
        // Destination data is R, G, B (,A if is alpha)
        GByte *pabyDataR = pabyData;
        GByte *pabyDataG = pabyData + 1 * nBandSpace;
        GByte *pabyDataB = pabyData + 2 * nBandSpace;
        GByte *pabyDataA = pabyData + 3 * nBandSpace;
        // Copied from Poppler
        int i, j;
        for (j = 0; j < nReqYSize; j++)
        {
            for (i = 0; i < nReqXSize; i++)
            {
                pabyDataR[i * nPixelSpace] = buffer[(i * 4) + 2];
                pabyDataG[i * nPixelSpace] = buffer[(i * 4) + 1];
                pabyDataB[i * nPixelSpace] = buffer[(i * 4) + 0];
                if (nBands == 4)
                {
                    pabyDataA[i * nPixelSpace] = buffer[(i * 4) + 3];
                }
            }
            pabyDataR += nLineSpace;
            pabyDataG += nLineSpace;
            pabyDataB += nLineSpace;
            pabyDataA += nLineSpace;
            buffer += stride;
        }
        FPDFBitmap_Destroy(bitmap);
    }
#endif  // ~ HAVE_PDFIUM

    return eErr;
}

/************************************************************************/
/* ==================================================================== */
/*                        PDFImageRasterBand                            */
/* ==================================================================== */
/************************************************************************/

class PDFImageRasterBand final : public PDFRasterBand
{
    friend class PDFDataset;

  public:
    PDFImageRasterBand(PDFDataset *, int);

    virtual CPLErr IReadBlock(int, int, void *) override;
};

/************************************************************************/
/*                        PDFImageRasterBand()                          */
/************************************************************************/

PDFImageRasterBand::PDFImageRasterBand(PDFDataset *poDSIn, int nBandIn)
    : PDFRasterBand(poDSIn, nBandIn, 0)
{
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr PDFImageRasterBand::IReadBlock(int CPL_UNUSED nBlockXOff, int nBlockYOff,
                                      void *pImage)
{
    PDFDataset *poGDS = (PDFDataset *)poDS;
    CPLAssert(poGDS->m_poImageObj != nullptr);

    if (!poGDS->m_bTried)
    {
        int nBands = (poGDS->nBands == 1) ? 1 : 3;
        poGDS->m_bTried = true;
        if (nBands == 3)
        {
            poGDS->m_pabyCachedData =
                (GByte *)VSIMalloc3(nBands, nRasterXSize, nRasterYSize);
            if (poGDS->m_pabyCachedData == nullptr)
                return CE_Failure;
        }

        GDALPDFStream *poStream = poGDS->m_poImageObj->GetStream();
        GByte *pabyStream = nullptr;

        if (poStream == nullptr ||
            static_cast<size_t>(poStream->GetLength()) !=
                static_cast<size_t>(nBands) * nRasterXSize * nRasterYSize ||
            (pabyStream = (GByte *)poStream->GetBytes()) == nullptr)
        {
            VSIFree(poGDS->m_pabyCachedData);
            poGDS->m_pabyCachedData = nullptr;
            return CE_Failure;
        }

        if (nBands == 3)
        {
            /* pixel interleaved to band interleaved */
            for (size_t i = 0;
                 i < static_cast<size_t>(nRasterXSize) * nRasterYSize; i++)
            {
                poGDS->m_pabyCachedData[0 * static_cast<size_t>(nRasterXSize) *
                                            nRasterYSize +
                                        i] = pabyStream[3 * i + 0];
                poGDS->m_pabyCachedData[1 * static_cast<size_t>(nRasterXSize) *
                                            nRasterYSize +
                                        i] = pabyStream[3 * i + 1];
                poGDS->m_pabyCachedData[2 * static_cast<size_t>(nRasterXSize) *
                                            nRasterYSize +
                                        i] = pabyStream[3 * i + 2];
            }
            VSIFree(pabyStream);
        }
        else
            poGDS->m_pabyCachedData = pabyStream;
    }

    if (poGDS->m_pabyCachedData == nullptr)
        return CE_Failure;

    if (nBand == 4)
        memset(pImage, 255, nRasterXSize);
    else
        memcpy(pImage,
               poGDS->m_pabyCachedData +
                   static_cast<size_t>(nBand - 1) * nRasterXSize *
                       nRasterYSize +
                   static_cast<size_t>(nBlockYOff) * nRasterXSize,
               nRasterXSize);

    return CE_None;
}

/************************************************************************/
/*                            PDFDataset()                              */
/************************************************************************/

PDFDataset::PDFDataset(PDFDataset *poParentDSIn, int nXSize, int nYSize)
    : m_bIsOvrDS(poParentDSIn != nullptr),
#ifdef HAVE_PDFIUM
      m_poDocPdfium(poParentDSIn ? poParentDSIn->m_poDocPdfium : nullptr),
      m_poPagePdfium(poParentDSIn ? poParentDSIn->m_poPagePdfium : nullptr),
#endif
      m_bSetStyle(CPLTestBool(CPLGetConfigOption("OGR_PDF_SET_STYLE", "YES")))
{
    m_oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;
    if (poParentDSIn)
        m_bUseLib = poParentDSIn->m_bUseLib;

    InitMapOperators();
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr PDFDataset::IBuildOverviews(const char *pszResampling, int nOverviews,
                                   const int *panOverviewList, int nListBands,
                                   const int *panBandList,
                                   GDALProgressFunc pfnProgress,
                                   void *pProgressData,
                                   CSLConstList papszOptions)

{
    /* -------------------------------------------------------------------- */
    /*      In order for building external overviews to work properly we    */
    /*      discard any concept of internal overviews when the user         */
    /*      first requests to build external overviews.                     */
    /* -------------------------------------------------------------------- */
    if (!m_apoOvrDS.empty())
    {
        m_apoOvrDSBackup = std::move(m_apoOvrDS);
        m_apoOvrDS.clear();
    }

    // Prevents InitOverviews() to run
    m_apoOvrDSBackup.emplace_back(nullptr);
    const CPLErr eErr = GDALPamDataset::IBuildOverviews(
        pszResampling, nOverviews, panOverviewList, nListBands, panBandList,
        pfnProgress, pProgressData, papszOptions);
    m_apoOvrDSBackup.pop_back();
    return eErr;
}

/************************************************************************/
/*                           PDFFreeDoc()                               */
/************************************************************************/

#ifdef HAVE_POPPLER
static void PDFFreeDoc(PDFDoc *poDoc)
{
    if (poDoc)
    {
        /* hack to avoid potential cross heap issues on Win32 */
        /* str is the VSIPDFFileStream object passed in the constructor of
         * PDFDoc */
        // NOTE: This is potentially very dangerous. See comment in
        // VSIPDFFileStream::FillBuffer() */
        delete poDoc->str;
        poDoc->str = nullptr;

        delete poDoc;
    }
}
#endif

/************************************************************************/
/*                            GetCatalog()                              */
/************************************************************************/

GDALPDFObject *PDFDataset::GetCatalog()
{
    if (m_poCatalogObject)
        return m_poCatalogObject;

#ifdef HAVE_POPPLER
    if (m_bUseLib.test(PDFLIB_POPPLER))
    {
        m_poCatalogObjectPoppler =
            std::make_unique<Object>(m_poDocPoppler->getXRef()->getCatalog());
        if (!m_poCatalogObjectPoppler->isNull())
            m_poCatalogObject =
                new GDALPDFObjectPoppler(m_poCatalogObjectPoppler.get(), FALSE);
    }
#endif

#ifdef HAVE_PODOFO
    if (m_bUseLib.test(PDFLIB_PODOFO))
    {
        int nCatalogNum = 0;
        int nCatalogGen = 0;
        VSILFILE *fp = VSIFOpenL(m_osFilename.c_str(), "rb");
        if (fp != nullptr)
        {
            GDALPDFUpdateWriter oWriter(fp);
            if (oWriter.ParseTrailerAndXRef())
            {
                nCatalogNum = oWriter.GetCatalogNum().toInt();
                nCatalogGen = oWriter.GetCatalogGen();
            }
            oWriter.Close();
        }

        PoDoFo::PdfObject *poCatalogPodofo =
            m_poDocPodofo->GetObjects().GetObject(
                PoDoFo::PdfReference(nCatalogNum, nCatalogGen));
        if (poCatalogPodofo)
            m_poCatalogObject = new GDALPDFObjectPodofo(
                poCatalogPodofo, m_poDocPodofo->GetObjects());
    }
#endif

#ifdef HAVE_PDFIUM
    if (m_bUseLib.test(PDFLIB_PDFIUM))
    {
        const CPDF_Dictionary *catalog = m_poDocPdfium->doc->GetRoot();
        if (catalog)
            m_poCatalogObject =
                // coverity is confused by WrapRetain(), believing that multiple
                // smart pointers manage the same raw pointer. Which is actually
                // true, but a RetainPtr holds a reference counted object. It is
                // thus safe to have several RetainPtr holding it.
                // coverity[multiple_init_smart_ptr]
                GDALPDFObjectPdfium::Build(pdfium::WrapRetain(catalog));
    }
#endif  // ~ HAVE_PDFIUM

    return m_poCatalogObject;
}

/************************************************************************/
/*                            ~PDFDataset()                            */
/************************************************************************/

PDFDataset::~PDFDataset()
{
#ifdef HAVE_PDFIUM
    m_apoOvrDS.clear();
    m_apoOvrDSBackup.clear();
#endif

    CPLFree(m_pabyCachedData);
    m_pabyCachedData = nullptr;

    delete m_poNeatLine;
    m_poNeatLine = nullptr;

    /* Collect data necessary to update */
    int nNum = 0;
    int nGen = 0;
    GDALPDFDictionaryRW *poPageDictCopy = nullptr;
    GDALPDFDictionaryRW *poCatalogDictCopy = nullptr;
    if (m_poPageObj)
    {
        nNum = m_poPageObj->GetRefNum().toInt();
        nGen = m_poPageObj->GetRefGen();
        if (eAccess == GA_Update &&
            (m_bProjDirty || m_bNeatLineDirty || m_bInfoDirty || m_bXMPDirty) &&
            nNum != 0 && m_poPageObj != nullptr &&
            m_poPageObj->GetType() == PDFObjectType_Dictionary)
        {
            poPageDictCopy = m_poPageObj->GetDictionary()->Clone();

            if (m_bXMPDirty)
            {
                /* We need the catalog because it points to the XMP Metadata
                 * object */
                GetCatalog();
                if (m_poCatalogObject &&
                    m_poCatalogObject->GetType() == PDFObjectType_Dictionary)
                    poCatalogDictCopy =
                        m_poCatalogObject->GetDictionary()->Clone();
            }
        }
    }

    /* Close document (and file descriptor) to be able to open it */
    /* in read-write mode afterwards */
    delete m_poPageObj;
    m_poPageObj = nullptr;
    delete m_poCatalogObject;
    m_poCatalogObject = nullptr;
#ifdef HAVE_POPPLER
    if (m_bUseLib.test(PDFLIB_POPPLER))
    {
        m_poCatalogObjectPoppler.reset();
        PDFFreeDoc(m_poDocPoppler);
    }
    m_poDocPoppler = nullptr;
#endif
#ifdef HAVE_PODOFO
    if (m_bUseLib.test(PDFLIB_PODOFO))
    {
        delete m_poDocPodofo;
    }
    m_poDocPodofo = nullptr;
#endif
#ifdef HAVE_PDFIUM
    if (!m_bIsOvrDS)
    {
        if (m_bUseLib.test(PDFLIB_PDFIUM))
        {
            UnloadPdfiumDocumentPage(&m_poDocPdfium, &m_poPagePdfium);
        }
    }
    m_poDocPdfium = nullptr;
    m_poPagePdfium = nullptr;
#endif  // ~ HAVE_PDFIUM

    /* Now do the update */
    if (poPageDictCopy)
    {
        VSILFILE *fp = VSIFOpenL(m_osFilename, "rb+");
        if (fp != nullptr)
        {
            GDALPDFUpdateWriter oWriter(fp);
            if (oWriter.ParseTrailerAndXRef())
            {
                if ((m_bProjDirty || m_bNeatLineDirty) &&
                    poPageDictCopy != nullptr)
                    oWriter.UpdateProj(this, m_dfDPI, poPageDictCopy,
                                       GDALPDFObjectNum(nNum), nGen);

                if (m_bInfoDirty)
                    oWriter.UpdateInfo(this);

                if (m_bXMPDirty && poCatalogDictCopy != nullptr)
                    oWriter.UpdateXMP(this, poCatalogDictCopy);
            }
            oWriter.Close();
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot open %s in update mode", m_osFilename.c_str());
        }
    }
    delete poPageDictCopy;
    poPageDictCopy = nullptr;
    delete poCatalogDictCopy;
    poCatalogDictCopy = nullptr;

    if (m_nGCPCount > 0)
    {
        GDALDeinitGCPs(m_nGCPCount, m_pasGCPList);
        CPLFree(m_pasGCPList);
        m_pasGCPList = nullptr;
        m_nGCPCount = 0;
    }

    CleanupIntermediateResources();

    m_apoLayers.clear();

    // Do that only after having destroyed Poppler objects
    m_fp.reset();
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

CPLErr PDFDataset::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                             int nXSize, int nYSize, void *pData, int nBufXSize,
                             int nBufYSize, GDALDataType eBufType,
                             int nBandCount, int *panBandMap,
                             GSpacing nPixelSpace, GSpacing nLineSpace,
                             GSpacing nBandSpace,
                             GDALRasterIOExtraArg *psExtraArg)
{
    // Try to pass the request to the most appropriate overview dataset.
    if (nBufXSize < nXSize && nBufYSize < nYSize)
    {
        int bTried = FALSE;
        const CPLErr eErr = TryOverviewRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace,
            nBandSpace, psExtraArg, &bTried);
        if (bTried)
            return eErr;
    }

    int nBandBlockXSize, nBandBlockYSize;
    int bReadPixels = FALSE;
    GetRasterBand(1)->GetBlockSize(&nBandBlockXSize, &nBandBlockYSize);
    if (m_aiTiles.empty() && eRWFlag == GF_Read && nXSize == nBufXSize &&
        nYSize == nBufYSize &&
        (nBufXSize > nBandBlockXSize || nBufYSize > nBandBlockYSize) &&
        eBufType == GDT_Byte && nBandCount == nBands &&
        (nBands >= 3 && panBandMap[0] == 1 && panBandMap[1] == 2 &&
         panBandMap[2] == 3 && (nBands == 3 || panBandMap[3] == 4)))
    {
        bReadPixels = TRUE;
#ifdef HAVE_PODOFO
        if (m_bUseLib.test(PDFLIB_PODOFO) && nBands == 4)
        {
            bReadPixels = FALSE;
        }
#endif
    }

    if (bReadPixels)
        return ReadPixels(nXOff, nYOff, nXSize, nYSize, nPixelSpace, nLineSpace,
                          nBandSpace, (GByte *)pData);

    if (nBufXSize != nXSize || nBufYSize != nYSize || eBufType != GDT_Byte)
    {
        m_bCacheBlocksForOtherBands = true;
    }
    CPLErr eErr = GDALPamDataset::IRasterIO(
        eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
        eBufType, nBandCount, panBandMap, nPixelSpace, nLineSpace, nBandSpace,
        psExtraArg);
    m_bCacheBlocksForOtherBands = false;
    return eErr;
}

/************************************************************************/
/*                            IRasterIO()                               */
/************************************************************************/

CPLErr PDFRasterBand::IRasterIO(GDALRWFlag eRWFlag, int nXOff, int nYOff,
                                int nXSize, int nYSize, void *pData,
                                int nBufXSize, int nBufYSize,
                                GDALDataType eBufType, GSpacing nPixelSpace,
                                GSpacing nLineSpace,
                                GDALRasterIOExtraArg *psExtraArg)
{
    PDFDataset *poGDS = (PDFDataset *)poDS;

    // Try to pass the request to the most appropriate overview dataset.
    if (nBufXSize < nXSize && nBufYSize < nYSize)
    {
        int bTried = FALSE;
        const CPLErr eErr = TryOverviewRasterIO(
            eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
            eBufType, nPixelSpace, nLineSpace, psExtraArg, &bTried);
        if (bTried)
            return eErr;
    }

    if (nBufXSize != nXSize || nBufYSize != nYSize || eBufType != GDT_Byte)
    {
        poGDS->m_bCacheBlocksForOtherBands = true;
    }
    CPLErr eErr = GDALPamRasterBand::IRasterIO(
        eRWFlag, nXOff, nYOff, nXSize, nYSize, pData, nBufXSize, nBufYSize,
        eBufType, nPixelSpace, nLineSpace, psExtraArg);
    poGDS->m_bCacheBlocksForOtherBands = false;
    return eErr;
}

/************************************************************************/
/*                    PDFDatasetErrorFunction()                         */
/************************************************************************/

#ifdef HAVE_POPPLER

static void PDFDatasetErrorFunctionCommon(const CPLString &osError)
{
    if (strcmp(osError.c_str(), "Incorrect password") == 0)
        return;
    /* Reported on newer USGS GeoPDF */
    if (strcmp(osError.c_str(),
               "Couldn't find group for reference to set OFF") == 0)
    {
        CPLDebug("PDF", "%s", osError.c_str());
        return;
    }

    CPLError(CE_Failure, CPLE_AppDefined, "%s", osError.c_str());
}

static int g_nPopplerErrors = 0;
constexpr int MAX_POPPLER_ERRORS = 1000;

static void PDFDatasetErrorFunction(ErrorCategory /* eErrCategory */,
                                    Goffset nPos, const char *pszMsg)
{
    if (g_nPopplerErrors >= MAX_POPPLER_ERRORS)
    {
        // If there are too many errors, then unregister ourselves and turn
        // quiet error mode, as the error() function in poppler can spend
        // significant time formatting an error message we won't emit...
        setErrorCallback(nullptr);
        globalParams->setErrQuiet(true);
        return;
    }

    g_nPopplerErrors++;
    CPLString osError;

    if (nPos >= 0)
        osError.Printf("Pos = " CPL_FRMT_GUIB ", ",
                       static_cast<GUIntBig>(nPos));
    osError += pszMsg;
    PDFDatasetErrorFunctionCommon(osError);
}
#endif

/************************************************************************/
/*                GDALPDFParseStreamContentOnlyDrawForm()               */
/************************************************************************/

static CPLString GDALPDFParseStreamContentOnlyDrawForm(const char *pszContent)
{
    CPLString osToken;
    char ch;
    int nCurIdx = 0;
    CPLString osCurrentForm;

    // CPLDebug("PDF", "content = %s", pszContent);

    while ((ch = *pszContent) != '\0')
    {
        if (ch == '%')
        {
            /* Skip comments until end-of-line */
            while ((ch = *pszContent) != '\0')
            {
                if (ch == '\r' || ch == '\n')
                    break;
                pszContent++;
            }
            if (ch == 0)
                break;
        }
        else if (ch == ' ' || ch == '\r' || ch == '\n')
        {
            if (!osToken.empty())
            {
                if (nCurIdx == 0 && osToken[0] == '/')
                {
                    osCurrentForm = osToken.substr(1);
                    nCurIdx++;
                }
                else if (nCurIdx == 1 && osToken == "Do")
                {
                    nCurIdx++;
                }
                else
                {
                    return "";
                }
            }
            osToken = "";
        }
        else
            osToken += ch;
        pszContent++;
    }

    return osCurrentForm;
}

/************************************************************************/
/*                    GDALPDFParseStreamContent()                       */
/************************************************************************/

typedef enum
{
    STATE_INIT,
    STATE_AFTER_q,
    STATE_AFTER_cm,
    STATE_AFTER_Do
} PDFStreamState;

/* This parser is reduced to understanding sequences that draw rasters, such as
   :
   q
   scaleX 0 0 scaleY translateX translateY cm
   /ImXXX Do
   Q

   All other sequences will abort the parsing.

   Returns TRUE if the stream only contains images.
*/

static int GDALPDFParseStreamContent(const char *pszContent,
                                     GDALPDFDictionary *poXObjectDict,
                                     double *pdfDPI, int *pbDPISet,
                                     int *pnBands,
                                     std::vector<GDALPDFTileDesc> &asTiles,
                                     int bAcceptRotationTerms)
{
    CPLString osToken;
    char ch;
    PDFStreamState nState = STATE_INIT;
    int nCurIdx = 0;
    double adfVals[6];
    CPLString osCurrentImage;

    double dfDPI = DEFAULT_DPI;
    *pbDPISet = FALSE;

    while ((ch = *pszContent) != '\0')
    {
        if (ch == '%')
        {
            /* Skip comments until end-of-line */
            while ((ch = *pszContent) != '\0')
            {
                if (ch == '\r' || ch == '\n')
                    break;
                pszContent++;
            }
            if (ch == 0)
                break;
        }
        else if (ch == ' ' || ch == '\t' || ch == '\r' || ch == '\n')
        {
            if (!osToken.empty())
            {
                if (nState == STATE_INIT)
                {
                    if (osToken == "q")
                    {
                        nState = STATE_AFTER_q;
                        nCurIdx = 0;
                    }
                    else if (osToken != "Q")
                        return FALSE;
                }
                else if (nState == STATE_AFTER_q)
                {
                    if (osToken == "q")
                    {
                        // ignore
                    }
                    else if (nCurIdx < 6)
                    {
                        adfVals[nCurIdx++] = CPLAtof(osToken);
                    }
                    else if (nCurIdx == 6 && osToken == "cm")
                    {
                        nState = STATE_AFTER_cm;
                        nCurIdx = 0;
                    }
                    else
                        return FALSE;
                }
                else if (nState == STATE_AFTER_cm)
                {
                    if (nCurIdx == 0 && osToken[0] == '/')
                    {
                        osCurrentImage = osToken.substr(1);
                    }
                    else if (osToken == "Do")
                    {
                        nState = STATE_AFTER_Do;
                    }
                    else
                        return FALSE;
                }
                else if (nState == STATE_AFTER_Do)
                {
                    if (osToken == "Q")
                    {
                        GDALPDFObject *poImage =
                            poXObjectDict->Get(osCurrentImage);
                        if (poImage != nullptr &&
                            poImage->GetType() == PDFObjectType_Dictionary)
                        {
                            GDALPDFTileDesc sTile;
                            GDALPDFDictionary *poImageDict =
                                poImage->GetDictionary();
                            GDALPDFObject *poWidth = poImageDict->Get("Width");
                            GDALPDFObject *poHeight =
                                poImageDict->Get("Height");
                            GDALPDFObject *poColorSpace =
                                poImageDict->Get("ColorSpace");
                            GDALPDFObject *poSMask = poImageDict->Get("SMask");
                            if (poColorSpace &&
                                poColorSpace->GetType() == PDFObjectType_Name)
                            {
                                if (poColorSpace->GetName() == "DeviceRGB")
                                {
                                    sTile.nBands = 3;
                                    if (*pnBands < 3)
                                        *pnBands = 3;
                                }
                                else if (poColorSpace->GetName() ==
                                         "DeviceGray")
                                {
                                    sTile.nBands = 1;
                                    if (*pnBands < 1)
                                        *pnBands = 1;
                                }
                                else
                                    sTile.nBands = 0;
                            }
                            if (poSMask != nullptr)
                                *pnBands = 4;

                            if (poWidth && poHeight &&
                                ((bAcceptRotationTerms &&
                                  adfVals[1] == -adfVals[2]) ||
                                 (!bAcceptRotationTerms && adfVals[1] == 0.0 &&
                                  adfVals[2] == 0.0)))
                            {
                                double dfWidth = Get(poWidth);
                                double dfHeight = Get(poHeight);
                                double dfScaleX = adfVals[0];
                                double dfScaleY = adfVals[3];
                                if (dfWidth > 0 && dfHeight > 0 &&
                                    dfScaleX > 0 && dfScaleY > 0 &&
                                    dfWidth / dfScaleX * DEFAULT_DPI <
                                        INT_MAX &&
                                    dfHeight / dfScaleY * DEFAULT_DPI < INT_MAX)
                                {
                                    double dfDPI_X = ROUND_TO_INT_IF_CLOSE(
                                        dfWidth / dfScaleX * DEFAULT_DPI, 1e-3);
                                    double dfDPI_Y = ROUND_TO_INT_IF_CLOSE(
                                        dfHeight / dfScaleY * DEFAULT_DPI,
                                        1e-3);
                                    // CPLDebug("PDF", "Image %s, width = %.16g,
                                    // height = %.16g, scaleX = %.16g, scaleY =
                                    // %.16g --> DPI_X = %.16g, DPI_Y = %.16g",
                                    //                 osCurrentImage.c_str(),
                                    //                 dfWidth, dfHeight,
                                    //                 dfScaleX, dfScaleY,
                                    //                 dfDPI_X, dfDPI_Y);
                                    if (dfDPI_X > dfDPI)
                                        dfDPI = dfDPI_X;
                                    if (dfDPI_Y > dfDPI)
                                        dfDPI = dfDPI_Y;

                                    memcpy(&(sTile.adfCM), adfVals,
                                           6 * sizeof(double));
                                    sTile.poImage = poImage;
                                    sTile.dfWidth = dfWidth;
                                    sTile.dfHeight = dfHeight;
                                    asTiles.push_back(sTile);

                                    *pbDPISet = TRUE;
                                    *pdfDPI = dfDPI;
                                }
                            }
                        }
                        nState = STATE_INIT;
                    }
                    else
                        return FALSE;
                }
            }
            osToken = "";
        }
        else
            osToken += ch;
        pszContent++;
    }

    return TRUE;
}

/************************************************************************/
/*                         CheckTiledRaster()                           */
/************************************************************************/

int PDFDataset::CheckTiledRaster()
{
    size_t i;
    int l_nBlockXSize = 0;
    int l_nBlockYSize = 0;
    const double dfUserUnit = m_dfDPI * USER_UNIT_IN_INCH;

    /* First pass : check that all tiles have same DPI, */
    /* are contained entirely in the raster size, */
    /* and determine the block size */
    for (i = 0; i < m_asTiles.size(); i++)
    {
        double dfDrawWidth = m_asTiles[i].adfCM[0] * dfUserUnit;
        double dfDrawHeight = m_asTiles[i].adfCM[3] * dfUserUnit;
        double dfX = m_asTiles[i].adfCM[4] * dfUserUnit;
        double dfY = m_asTiles[i].adfCM[5] * dfUserUnit;
        int nX = (int)(dfX + 0.1);
        int nY = (int)(dfY + 0.1);
        int nWidth = (int)(m_asTiles[i].dfWidth + 1e-8);
        int nHeight = (int)(m_asTiles[i].dfHeight + 1e-8);

        GDALPDFDictionary *poImageDict = m_asTiles[i].poImage->GetDictionary();
        GDALPDFObject *poBitsPerComponent =
            poImageDict->Get("BitsPerComponent");
        GDALPDFObject *poColorSpace = poImageDict->Get("ColorSpace");
        GDALPDFObject *poFilter = poImageDict->Get("Filter");

        /* Podofo cannot uncompress JPEG2000 streams */
        if (m_bUseLib.test(PDFLIB_PODOFO) && poFilter != nullptr &&
            poFilter->GetType() == PDFObjectType_Name &&
            poFilter->GetName() == "JPXDecode")
        {
            CPLDebug("PDF", "Tile %d : Incompatible image for tiled reading",
                     (int)i);
            return FALSE;
        }

        if (poBitsPerComponent == nullptr || Get(poBitsPerComponent) != 8 ||
            poColorSpace == nullptr ||
            poColorSpace->GetType() != PDFObjectType_Name ||
            (poColorSpace->GetName() != "DeviceRGB" &&
             poColorSpace->GetName() != "DeviceGray"))
        {
            CPLDebug("PDF", "Tile %d : Incompatible image for tiled reading",
                     (int)i);
            return FALSE;
        }

        if (fabs(dfDrawWidth - m_asTiles[i].dfWidth) > 1e-2 ||
            fabs(dfDrawHeight - m_asTiles[i].dfHeight) > 1e-2 ||
            fabs(nWidth - m_asTiles[i].dfWidth) > 1e-8 ||
            fabs(nHeight - m_asTiles[i].dfHeight) > 1e-8 ||
            fabs(nX - dfX) > 1e-1 || fabs(nY - dfY) > 1e-1 || nX < 0 ||
            nY < 0 || nX + nWidth > nRasterXSize || nY >= nRasterYSize)
        {
            CPLDebug("PDF", "Tile %d : %f %f %f %f %f %f", (int)i, dfX, dfY,
                     dfDrawWidth, dfDrawHeight, m_asTiles[i].dfWidth,
                     m_asTiles[i].dfHeight);
            return FALSE;
        }
        if (l_nBlockXSize == 0 && l_nBlockYSize == 0 && nX == 0 && nY != 0)
        {
            l_nBlockXSize = nWidth;
            l_nBlockYSize = nHeight;
        }
    }
    if (l_nBlockXSize <= 0 || l_nBlockYSize <= 0 || l_nBlockXSize > 2048 ||
        l_nBlockYSize > 2048)
        return FALSE;

    int nXBlocks = DIV_ROUND_UP(nRasterXSize, l_nBlockXSize);
    int nYBlocks = DIV_ROUND_UP(nRasterYSize, l_nBlockYSize);

    /* Second pass to determine that all tiles are properly aligned on block
     * size */
    for (i = 0; i < m_asTiles.size(); i++)
    {
        double dfX = m_asTiles[i].adfCM[4] * dfUserUnit;
        double dfY = m_asTiles[i].adfCM[5] * dfUserUnit;
        int nX = (int)(dfX + 0.1);
        int nY = (int)(dfY + 0.1);
        int nWidth = (int)(m_asTiles[i].dfWidth + 1e-8);
        int nHeight = (int)(m_asTiles[i].dfHeight + 1e-8);
        int bOK = TRUE;
        int nBlockXOff = nX / l_nBlockXSize;
        if ((nX % l_nBlockXSize) != 0)
            bOK = FALSE;
        if (nBlockXOff < nXBlocks - 1 && nWidth != l_nBlockXSize)
            bOK = FALSE;
        if (nBlockXOff == nXBlocks - 1 && nX + nWidth != nRasterXSize)
            bOK = FALSE;

        if (nY > 0 && nHeight != l_nBlockYSize)
            bOK = FALSE;
        if (nY == 0 && nHeight != nRasterYSize - (nYBlocks - 1) * l_nBlockYSize)
            bOK = FALSE;

        if (!bOK)
        {
            CPLDebug("PDF", "Tile %d : %d %d %d %d", (int)i, nX, nY, nWidth,
                     nHeight);
            return FALSE;
        }
    }

    /* Third pass to set the aiTiles array */
    m_aiTiles.resize(static_cast<size_t>(nXBlocks) * nYBlocks, -1);
    for (i = 0; i < m_asTiles.size(); i++)
    {
        double dfX = m_asTiles[i].adfCM[4] * dfUserUnit;
        double dfY = m_asTiles[i].adfCM[5] * dfUserUnit;
        int nHeight = (int)(m_asTiles[i].dfHeight + 1e-8);
        int nX = (int)(dfX + 0.1);
        int nY = nRasterYSize - ((int)(dfY + 0.1) + nHeight);
        int nBlockXOff = nX / l_nBlockXSize;
        int nBlockYOff = nY / l_nBlockYSize;
        m_aiTiles[nBlockYOff * nXBlocks + nBlockXOff] = (int)i;
    }

    this->m_nBlockXSize = l_nBlockXSize;
    this->m_nBlockYSize = l_nBlockYSize;

    return TRUE;
}

/************************************************************************/
/*                              GuessDPI()                              */
/************************************************************************/

void PDFDataset::GuessDPI(GDALPDFDictionary *poPageDict, int *pnBands)
{
    const char *pszDPI = GetOption(papszOpenOptions, "DPI", nullptr);
    if (pszDPI != nullptr)
    {
        // coverity[tainted_data]
        m_dfDPI = CPLAtof(pszDPI);
    }
    else
    {
        /* Try to get a better value from the images that are drawn */
        /* Very simplistic logic. Will only work for raster only PDF */

        GDALPDFObject *poContents = poPageDict->Get("Contents");
        if (poContents != nullptr &&
            poContents->GetType() == PDFObjectType_Array)
        {
            GDALPDFArray *poContentsArray = poContents->GetArray();
            if (poContentsArray->GetLength() == 1)
            {
                poContents = poContentsArray->Get(0);
            }
        }

        GDALPDFObject *poXObject =
            poPageDict->LookupObject("Resources.XObject");
        if (poContents != nullptr &&
            poContents->GetType() == PDFObjectType_Dictionary &&
            poXObject != nullptr &&
            poXObject->GetType() == PDFObjectType_Dictionary)
        {
            GDALPDFDictionary *poXObjectDict = poXObject->GetDictionary();
            GDALPDFDictionary *poContentDict = poXObjectDict;
            GDALPDFStream *poPageStream = poContents->GetStream();
            if (poPageStream != nullptr)
            {
                char *pszContent = nullptr;
                const int64_t MAX_LENGTH = 10 * 1000 * 1000;
                int64_t nLength = poPageStream->GetLength(MAX_LENGTH);
                int bResetTiles = FALSE;
                double dfScaleDPI = 1.0;

                if (nLength < MAX_LENGTH)
                {
                    CPLString osForm;
                    pszContent = poPageStream->GetBytes();
                    if (pszContent != nullptr)
                    {
#ifdef DEBUG
                        const char *pszDumpStream =
                            CPLGetConfigOption("PDF_DUMP_STREAM", nullptr);
                        if (pszDumpStream != nullptr)
                        {
                            VSILFILE *fpDump = VSIFOpenL(pszDumpStream, "wb");
                            if (fpDump)
                            {
                                VSIFWriteL(pszContent, 1,
                                           static_cast<int>(nLength), fpDump);
                                VSIFCloseL(fpDump);
                            }
                        }
#endif  // DEBUG
                        osForm =
                            GDALPDFParseStreamContentOnlyDrawForm(pszContent);
                        if (osForm.empty())
                        {
                            /* Special case for USGS Topo PDF, like
                             * CA_Hollywood_20090811_OM_geo.pdf */
                            const char *pszOGCDo =
                                strstr(pszContent, " /XO1 Do");
                            if (pszOGCDo)
                            {
                                const char *pszcm = strstr(pszContent, " cm ");
                                if (pszcm != nullptr && pszcm < pszOGCDo)
                                {
                                    const char *pszNextcm =
                                        strstr(pszcm + 2, "cm");
                                    if (pszNextcm == nullptr ||
                                        pszNextcm > pszOGCDo)
                                    {
                                        const char *pszIter = pszcm;
                                        while (pszIter > pszContent)
                                        {
                                            if ((*pszIter >= '0' &&
                                                 *pszIter <= '9') ||
                                                *pszIter == '-' ||
                                                *pszIter == '.' ||
                                                *pszIter == ' ')
                                                pszIter--;
                                            else
                                            {
                                                pszIter++;
                                                break;
                                            }
                                        }
                                        CPLString oscm(pszIter);
                                        oscm.resize(pszcm - pszIter);
                                        char **papszTokens =
                                            CSLTokenizeString(oscm);
                                        double dfScaleX = -1.0;
                                        double dfScaleY = -2.0;
                                        if (CSLCount(papszTokens) == 6)
                                        {
                                            dfScaleX = CPLAtof(papszTokens[0]);
                                            dfScaleY = CPLAtof(papszTokens[3]);
                                        }
                                        CSLDestroy(papszTokens);
                                        if (dfScaleX == dfScaleY &&
                                            dfScaleX > 0.0)
                                        {
                                            osForm = "XO1";
                                            bResetTiles = TRUE;
                                            dfScaleDPI = 1.0 / dfScaleX;
                                        }
                                    }
                                }
                                else
                                {
                                    osForm = "XO1";
                                    bResetTiles = TRUE;
                                }
                            }
                            /* Special case for USGS Topo PDF, like
                             * CA_Sacramento_East_20120308_TM_geo.pdf */
                            else
                            {
                                CPLString osOCG =
                                    FindLayerOCG(poPageDict, "Orthoimage");
                                if (!osOCG.empty())
                                {
                                    const char *pszBDCLookup = CPLSPrintf(
                                        "/OC /%s BDC", osOCG.c_str());
                                    const char *pszBDC =
                                        strstr(pszContent, pszBDCLookup);
                                    if (pszBDC != nullptr)
                                    {
                                        const char *pszIter =
                                            pszBDC + strlen(pszBDCLookup);
                                        while (*pszIter != '\0')
                                        {
                                            if (*pszIter == 13 ||
                                                *pszIter == 10 ||
                                                *pszIter == ' ' ||
                                                *pszIter == 'q')
                                                pszIter++;
                                            else
                                                break;
                                        }
                                        if (STARTS_WITH(pszIter,
                                                        "1 0 0 1 0 0 cm\n"))
                                            pszIter +=
                                                strlen("1 0 0 1 0 0 cm\n");
                                        if (*pszIter == '/')
                                        {
                                            pszIter++;
                                            const char *pszDo =
                                                strstr(pszIter, " Do");
                                            if (pszDo != nullptr)
                                            {
                                                osForm = pszIter;
                                                osForm.resize(pszDo - pszIter);
                                                bResetTiles = TRUE;
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }

                    if (!osForm.empty())
                    {
                        CPLFree(pszContent);
                        pszContent = nullptr;

                        GDALPDFObject *poObjForm = poXObjectDict->Get(osForm);
                        if (poObjForm != nullptr &&
                            poObjForm->GetType() == PDFObjectType_Dictionary &&
                            (poPageStream = poObjForm->GetStream()) != nullptr)
                        {
                            GDALPDFDictionary *poObjFormDict =
                                poObjForm->GetDictionary();
                            GDALPDFObject *poSubtype =
                                poObjFormDict->Get("Subtype");
                            if (poSubtype != nullptr &&
                                poSubtype->GetType() == PDFObjectType_Name &&
                                poSubtype->GetName() == "Form")
                            {
                                nLength = poPageStream->GetLength(MAX_LENGTH);
                                if (nLength < MAX_LENGTH)
                                {
                                    pszContent = poPageStream->GetBytes();

                                    GDALPDFObject *poXObject2 =
                                        poObjFormDict->LookupObject(
                                            "Resources.XObject");
                                    if (poXObject2 != nullptr &&
                                        poXObject2->GetType() ==
                                            PDFObjectType_Dictionary)
                                        poContentDict =
                                            poXObject2->GetDictionary();
                                }
                            }
                        }
                    }
                }

                if (pszContent != nullptr)
                {
                    int bDPISet = FALSE;

                    const char *pszContentToParse = pszContent;
                    if (bResetTiles)
                    {
                        while (*pszContentToParse != '\0')
                        {
                            if (*pszContentToParse == 13 ||
                                *pszContentToParse == 10 ||
                                *pszContentToParse == ' ' ||
                                (*pszContentToParse >= '0' &&
                                 *pszContentToParse <= '9') ||
                                *pszContentToParse == '.' ||
                                *pszContentToParse == '-' ||
                                *pszContentToParse == 'l' ||
                                *pszContentToParse == 'm' ||
                                *pszContentToParse == 'n' ||
                                *pszContentToParse == 'W')
                                pszContentToParse++;
                            else
                                break;
                        }
                    }

                    GDALPDFParseStreamContent(pszContentToParse, poContentDict,
                                              &(m_dfDPI), &bDPISet, pnBands,
                                              m_asTiles, bResetTiles);
                    CPLFree(pszContent);
                    if (bDPISet)
                    {
                        m_dfDPI *= dfScaleDPI;

                        CPLDebug("PDF",
                                 "DPI guessed from contents stream = %.16g",
                                 m_dfDPI);
                        SetMetadataItem("DPI", CPLSPrintf("%.16g", m_dfDPI));
                        if (bResetTiles)
                            m_asTiles.resize(0);
                    }
                    else
                        m_asTiles.resize(0);
                }
            }
        }

        GDALPDFObject *poUserUnit = nullptr;
        if ((poUserUnit = poPageDict->Get("UserUnit")) != nullptr &&
            (poUserUnit->GetType() == PDFObjectType_Int ||
             poUserUnit->GetType() == PDFObjectType_Real))
        {
            m_dfDPI =
                ROUND_TO_INT_IF_CLOSE(Get(poUserUnit) * DEFAULT_DPI, 1e-5);
            CPLDebug("PDF", "Found UserUnit in Page --> DPI = %.16g", m_dfDPI);
            SetMetadataItem("DPI", CPLSPrintf("%.16g", m_dfDPI));
        }
    }

    if (m_dfDPI < 1e-2 || m_dfDPI > 7200)
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Invalid value for GDAL_PDF_DPI. Using default value instead");
        m_dfDPI = GDAL_DEFAULT_DPI;
    }
}

/************************************************************************/
/*                              FindXMP()                               */
/************************************************************************/

void PDFDataset::FindXMP(GDALPDFObject *poObj)
{
    if (poObj->GetType() != PDFObjectType_Dictionary)
        return;

    GDALPDFDictionary *poDict = poObj->GetDictionary();
    GDALPDFObject *poType = poDict->Get("Type");
    GDALPDFObject *poSubtype = poDict->Get("Subtype");
    if (poType == nullptr || poType->GetType() != PDFObjectType_Name ||
        poType->GetName() != "Metadata" || poSubtype == nullptr ||
        poSubtype->GetType() != PDFObjectType_Name ||
        poSubtype->GetName() != "XML")
    {
        return;
    }

    GDALPDFStream *poStream = poObj->GetStream();
    if (poStream == nullptr)
        return;

    char *pszContent = poStream->GetBytes();
    const auto nLength = poStream->GetLength();
    if (pszContent != nullptr && nLength > 15 &&
        STARTS_WITH(pszContent, "<?xpacket begin="))
    {
        char *apszMDList[2];
        apszMDList[0] = pszContent;
        apszMDList[1] = nullptr;
        SetMetadata(apszMDList, "xml:XMP");
    }
    CPLFree(pszContent);
}

/************************************************************************/
/*                             ParseInfo()                              */
/************************************************************************/

void PDFDataset::ParseInfo(GDALPDFObject *poInfoObj)
{
    if (poInfoObj->GetType() != PDFObjectType_Dictionary)
        return;

    GDALPDFDictionary *poInfoObjDict = poInfoObj->GetDictionary();
    GDALPDFObject *poItem = nullptr;
    int bOneMDISet = FALSE;
    if ((poItem = poInfoObjDict->Get("Author")) != nullptr &&
        poItem->GetType() == PDFObjectType_String)
    {
        SetMetadataItem("AUTHOR", poItem->GetString().c_str());
        bOneMDISet = TRUE;
    }
    if ((poItem = poInfoObjDict->Get("Creator")) != nullptr &&
        poItem->GetType() == PDFObjectType_String)
    {
        SetMetadataItem("CREATOR", poItem->GetString().c_str());
        bOneMDISet = TRUE;
    }
    if ((poItem = poInfoObjDict->Get("Keywords")) != nullptr &&
        poItem->GetType() == PDFObjectType_String)
    {
        SetMetadataItem("KEYWORDS", poItem->GetString().c_str());
        bOneMDISet = TRUE;
    }
    if ((poItem = poInfoObjDict->Get("Subject")) != nullptr &&
        poItem->GetType() == PDFObjectType_String)
    {
        SetMetadataItem("SUBJECT", poItem->GetString().c_str());
        bOneMDISet = TRUE;
    }
    if ((poItem = poInfoObjDict->Get("Title")) != nullptr &&
        poItem->GetType() == PDFObjectType_String)
    {
        SetMetadataItem("TITLE", poItem->GetString().c_str());
        bOneMDISet = TRUE;
    }
    if ((poItem = poInfoObjDict->Get("Producer")) != nullptr &&
        poItem->GetType() == PDFObjectType_String)
    {
        if (bOneMDISet ||
            poItem->GetString() != "PoDoFo - http://podofo.sf.net")
        {
            SetMetadataItem("PRODUCER", poItem->GetString().c_str());
            bOneMDISet = TRUE;
        }
    }
    if ((poItem = poInfoObjDict->Get("CreationDate")) != nullptr &&
        poItem->GetType() == PDFObjectType_String)
    {
        if (bOneMDISet)
            SetMetadataItem("CREATION_DATE", poItem->GetString().c_str());
    }
}

#if defined(HAVE_POPPLER) || defined(HAVE_PDFIUM)

/************************************************************************/
/*                             AddLayer()                               */
/************************************************************************/

void PDFDataset::AddLayer(const std::string &osName, int iPage)
{
    LayerStruct layerStruct;
    layerStruct.osName = osName;
    layerStruct.nInsertIdx = static_cast<int>(m_oLayerNameSet.size());
    layerStruct.iPage = iPage;
    m_oLayerNameSet.emplace_back(std::move(layerStruct));
}

/************************************************************************/
/*                           CreateLayerList()                          */
/************************************************************************/

void PDFDataset::CreateLayerList()
{
    // Sort layers by prioritizing page number and then insertion index
    std::sort(m_oLayerNameSet.begin(), m_oLayerNameSet.end(),
              [](const LayerStruct &a, const LayerStruct &b)
              {
                  if (a.iPage < b.iPage)
                      return true;
                  if (a.iPage > b.iPage)
                      return false;
                  return a.nInsertIdx < b.nInsertIdx;
              });

    if (m_oLayerNameSet.size() >= 100)
    {
        for (const auto &oLayerStruct : m_oLayerNameSet)
        {
            m_aosLayerNames.AddNameValue(
                CPLSPrintf("LAYER_%03d_NAME", m_aosLayerNames.size()),
                oLayerStruct.osName.c_str());
        }
    }
    else
    {
        for (const auto &oLayerStruct : m_oLayerNameSet)
        {
            m_aosLayerNames.AddNameValue(
                CPLSPrintf("LAYER_%02d_NAME", m_aosLayerNames.size()),
                oLayerStruct.osName.c_str());
        }
    }
}

/************************************************************************/
/*                  BuildPostfixedLayerNameAndAddLayer()                */
/************************************************************************/

/** Append a suffix with the page number(s) to the provided layer name, if
 * it makes sense (that is if it is a multiple page PDF and we haven't selected
 * a specific name). And also call AddLayer() on it if successful.
 * If may return an empty string if the layer isn't used by the page of interest
 */
std::string PDFDataset::BuildPostfixedLayerNameAndAddLayer(
    const std::string &osName, const std::pair<int, int> &oOCGRef,
    int iPageOfInterest, int nPageCount)
{
    std::string osPostfixedName = osName;
    int iLayerPage = 0;
    if (nPageCount > 1 && !m_oMapOCGNumGenToPages.empty())
    {
        const auto oIterToPages = m_oMapOCGNumGenToPages.find(oOCGRef);
        if (oIterToPages != m_oMapOCGNumGenToPages.end())
        {
            const auto &anPages = oIterToPages->second;
            if (iPageOfInterest > 0)
            {
                if (std::find(anPages.begin(), anPages.end(),
                              iPageOfInterest) == anPages.end())
                {
                    return std::string();
                }
            }
            else if (anPages.size() == 1)
            {
                iLayerPage = anPages.front();
                osPostfixedName += CPLSPrintf(" (page %d)", anPages.front());
            }
            else
            {
                osPostfixedName += " (pages ";
                for (size_t j = 0; j < anPages.size(); ++j)
                {
                    if (j > 0)
                        osPostfixedName += ", ";
                    osPostfixedName += CPLSPrintf("%d", anPages[j]);
                }
                osPostfixedName += ')';
            }
        }
    }

    AddLayer(osPostfixedName, iLayerPage);

    return osPostfixedName;
}

#endif  //  defined(HAVE_POPPLER) || defined(HAVE_PDFIUM)

#ifdef HAVE_POPPLER

/************************************************************************/
/*                       ExploreLayersPoppler()                         */
/************************************************************************/

void PDFDataset::ExploreLayersPoppler(GDALPDFArray *poArray,
                                      int iPageOfInterest, int nPageCount,
                                      CPLString osTopLayer, int nRecLevel,
                                      int &nVisited, bool &bStop)
{
    if (nRecLevel == 16 || nVisited == 1000)
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "ExploreLayersPoppler(): too deep exploration or too many items");
        bStop = true;
        return;
    }
    if (bStop)
        return;

    int nLength = poArray->GetLength();
    CPLString osCurLayer;
    for (int i = 0; i < nLength; i++)
    {
        nVisited++;
        GDALPDFObject *poObj = poArray->Get(i);
        if (poObj == nullptr)
            continue;
        if (i == 0 && poObj->GetType() == PDFObjectType_String)
        {
            std::string osName =
                PDFSanitizeLayerName(poObj->GetString().c_str());
            if (!osTopLayer.empty())
            {
                osTopLayer += '.';
                osTopLayer += osName;
            }
            else
                osTopLayer = std::move(osName);
            AddLayer(osTopLayer, 0);
            m_oLayerOCGListPoppler.push_back(std::pair(osTopLayer, nullptr));
        }
        else if (poObj->GetType() == PDFObjectType_Array)
        {
            ExploreLayersPoppler(poObj->GetArray(), iPageOfInterest, nPageCount,
                                 osCurLayer, nRecLevel + 1, nVisited, bStop);
            if (bStop)
                return;
            osCurLayer = "";
        }
        else if (poObj->GetType() == PDFObjectType_Dictionary)
        {
            GDALPDFDictionary *poDict = poObj->GetDictionary();
            GDALPDFObject *poName = poDict->Get("Name");
            if (poName != nullptr && poName->GetType() == PDFObjectType_String)
            {
                std::string osName =
                    PDFSanitizeLayerName(poName->GetString().c_str());
                /* coverity[copy_paste_error] */
                if (!osTopLayer.empty())
                {
                    osCurLayer = osTopLayer;
                    osCurLayer += '.';
                    osCurLayer += osName;
                }
                else
                    osCurLayer = std::move(osName);
                // CPLDebug("PDF", "Layer %s", osCurLayer.c_str());

                OCGs *optContentConfig = m_poDocPoppler->getOptContentConfig();
                struct Ref r;
                r.num = poObj->GetRefNum().toInt();
                r.gen = poObj->GetRefGen();
                OptionalContentGroup *ocg = optContentConfig->findOcgByRef(r);
                if (ocg)
                {
                    const auto oRefPair = std::pair(poObj->GetRefNum().toInt(),
                                                    poObj->GetRefGen());
                    const std::string osPostfixedName =
                        BuildPostfixedLayerNameAndAddLayer(
                            osCurLayer, oRefPair, iPageOfInterest, nPageCount);
                    if (osPostfixedName.empty())
                        continue;

                    m_oLayerOCGListPoppler.push_back(
                        std::make_pair(osPostfixedName, ocg));
                    m_aoLayerWithRef.emplace_back(osPostfixedName.c_str(),
                                                  poObj->GetRefNum(), r.gen);
                }
            }
        }
    }
}

/************************************************************************/
/*                         FindLayersPoppler()                          */
/************************************************************************/

void PDFDataset::FindLayersPoppler(int iPageOfInterest)
{
    int nPageCount = 0;
    const auto poPages = GetPagesKids();
    if (poPages)
        nPageCount = poPages->GetLength();

    OCGs *optContentConfig = m_poDocPoppler->getOptContentConfig();
    if (optContentConfig == nullptr || !optContentConfig->isOk())
        return;

    Array *array = optContentConfig->getOrderArray();
    if (array)
    {
        GDALPDFArray *poArray = GDALPDFCreateArray(array);
        int nVisited = 0;
        bool bStop = false;
        ExploreLayersPoppler(poArray, iPageOfInterest, nPageCount, CPLString(),
                             0, nVisited, bStop);
        delete poArray;
    }
    else
    {
        for (const auto &refOCGPair : optContentConfig->getOCGs())
        {
            auto ocg = refOCGPair.second.get();
            if (ocg != nullptr && ocg->getName() != nullptr)
            {
                const char *pszLayerName =
                    (const char *)ocg->getName()->c_str();
                AddLayer(pszLayerName, 0);
                m_oLayerOCGListPoppler.push_back(
                    std::make_pair(CPLString(pszLayerName), ocg));
            }
        }
    }

    CreateLayerList();
    m_oMDMD_PDF.SetMetadata(m_aosLayerNames.List(), "LAYERS");
}

/************************************************************************/
/*                       TurnLayersOnOffPoppler()                       */
/************************************************************************/

void PDFDataset::TurnLayersOnOffPoppler()
{
    OCGs *optContentConfig = m_poDocPoppler->getOptContentConfig();
    if (optContentConfig == nullptr || !optContentConfig->isOk())
        return;

    // Which layers to turn ON ?
    const char *pszLayers = GetOption(papszOpenOptions, "LAYERS", nullptr);
    if (pszLayers)
    {
        int i;
        int bAll = EQUAL(pszLayers, "ALL");
        for (const auto &refOCGPair : optContentConfig->getOCGs())
        {
            auto ocg = refOCGPair.second.get();
            ocg->setState((bAll) ? OptionalContentGroup::On
                                 : OptionalContentGroup::Off);
        }

        char **papszLayers = CSLTokenizeString2(pszLayers, ",", 0);
        for (i = 0; !bAll && papszLayers[i] != nullptr; i++)
        {
            bool isFound = false;
            for (auto oIter2 = m_oLayerOCGListPoppler.begin();
                 oIter2 != m_oLayerOCGListPoppler.end(); ++oIter2)
            {
                if (oIter2->first != papszLayers[i])
                    continue;

                isFound = true;
                auto oIter = oIter2;
                if (oIter->second)
                {
                    // CPLDebug("PDF", "Turn '%s' on", papszLayers[i]);
                    oIter->second->setState(OptionalContentGroup::On);
                }

                // Turn child layers on, unless there's one of them explicitly
                // listed in the list.
                size_t nLen = strlen(papszLayers[i]);
                int bFoundChildLayer = FALSE;
                oIter = m_oLayerOCGListPoppler.begin();
                for (;
                     oIter != m_oLayerOCGListPoppler.end() && !bFoundChildLayer;
                     ++oIter)
                {
                    if (oIter->first.size() > nLen &&
                        strncmp(oIter->first.c_str(), papszLayers[i], nLen) ==
                            0 &&
                        oIter->first[nLen] == '.')
                    {
                        for (int j = 0; papszLayers[j] != nullptr; j++)
                        {
                            if (strcmp(papszLayers[j], oIter->first.c_str()) ==
                                0)
                            {
                                bFoundChildLayer = TRUE;
                                break;
                            }
                        }
                    }
                }

                if (!bFoundChildLayer)
                {
                    oIter = m_oLayerOCGListPoppler.begin();
                    for (; oIter != m_oLayerOCGListPoppler.end() &&
                           !bFoundChildLayer;
                         ++oIter)
                    {
                        if (oIter->first.size() > nLen &&
                            strncmp(oIter->first.c_str(), papszLayers[i],
                                    nLen) == 0 &&
                            oIter->first[nLen] == '.')
                        {
                            if (oIter->second)
                            {
                                // CPLDebug("PDF", "Turn '%s' on too",
                                // oIter->first.c_str());
                                oIter->second->setState(
                                    OptionalContentGroup::On);
                            }
                        }
                    }
                }

                // Turn parent layers on too
                std::string layer(papszLayers[i]);
                std::string::size_type j;
                while ((j = layer.find_last_of('.')) != std::string::npos)
                {
                    layer.resize(j);
                    oIter = m_oLayerOCGListPoppler.begin();
                    for (; oIter != m_oLayerOCGListPoppler.end(); ++oIter)
                    {
                        if (oIter->first == layer && oIter->second)
                        {
                            // CPLDebug("PDF", "Turn '%s' on too",
                            // layer.c_str());
                            oIter->second->setState(OptionalContentGroup::On);
                        }
                    }
                }
            }
            if (!isFound)
            {
                CPLError(CE_Warning, CPLE_AppDefined, "Unknown layer '%s'",
                         papszLayers[i]);
            }
        }
        CSLDestroy(papszLayers);

        m_bUseOCG = true;
    }

    // Which layers to turn OFF ?
    const char *pszLayersOFF =
        GetOption(papszOpenOptions, "LAYERS_OFF", nullptr);
    if (pszLayersOFF)
    {
        char **papszLayersOFF = CSLTokenizeString2(pszLayersOFF, ",", 0);
        for (int i = 0; papszLayersOFF[i] != nullptr; i++)
        {
            bool isFound = false;
            for (auto oIter2 = m_oLayerOCGListPoppler.begin();
                 oIter2 != m_oLayerOCGListPoppler.end(); ++oIter2)
            {
                if (oIter2->first != papszLayersOFF[i])
                    continue;

                isFound = true;
                auto oIter = oIter2;
                if (oIter->second)
                {
                    // CPLDebug("PDF", "Turn '%s' off", papszLayersOFF[i]);
                    oIter->second->setState(OptionalContentGroup::Off);
                }

                // Turn child layers off too
                size_t nLen = strlen(papszLayersOFF[i]);
                oIter = m_oLayerOCGListPoppler.begin();
                for (; oIter != m_oLayerOCGListPoppler.end(); ++oIter)
                {
                    if (oIter->first.size() > nLen &&
                        strncmp(oIter->first.c_str(), papszLayersOFF[i],
                                nLen) == 0 &&
                        oIter->first[nLen] == '.')
                    {
                        if (oIter->second)
                        {
                            // CPLDebug("PDF", "Turn '%s' off too",
                            // oIter->first.c_str());
                            oIter->second->setState(OptionalContentGroup::Off);
                        }
                    }
                }
            }
            if (!isFound)
            {
                CPLError(CE_Warning, CPLE_AppDefined, "Unknown layer '%s'",
                         papszLayersOFF[i]);
            }
        }
        CSLDestroy(papszLayersOFF);

        m_bUseOCG = true;
    }
}

#endif

#ifdef HAVE_PDFIUM

/************************************************************************/
/*                       ExploreLayersPdfium()                          */
/************************************************************************/

void PDFDataset::ExploreLayersPdfium(GDALPDFArray *poArray, int iPageOfInterest,
                                     int nPageCount, int nRecLevel,
                                     CPLString osTopLayer)
{
    if (nRecLevel == 16)
        return;

    const int nLength = poArray->GetLength();
    std::string osCurLayer;
    for (int i = 0; i < nLength; i++)
    {
        GDALPDFObject *poObj = poArray->Get(i);
        if (poObj == nullptr)
            continue;
        if (i == 0 && poObj->GetType() == PDFObjectType_String)
        {
            const std::string osName =
                PDFSanitizeLayerName(poObj->GetString().c_str());
            if (!osTopLayer.empty())
                osTopLayer = std::string(osTopLayer).append(".").append(osName);
            else
                osTopLayer = osName;
            AddLayer(osTopLayer, 0);
            m_oMapLayerNameToOCGNumGenPdfium[osTopLayer] = std::pair(-1, -1);
        }
        else if (poObj->GetType() == PDFObjectType_Array)
        {
            ExploreLayersPdfium(poObj->GetArray(), iPageOfInterest, nPageCount,
                                nRecLevel + 1, osCurLayer);
            osCurLayer.clear();
        }
        else if (poObj->GetType() == PDFObjectType_Dictionary)
        {
            GDALPDFDictionary *poDict = poObj->GetDictionary();
            GDALPDFObject *poName = poDict->Get("Name");
            if (poName != nullptr && poName->GetType() == PDFObjectType_String)
            {
                const std::string osName =
                    PDFSanitizeLayerName(poName->GetString().c_str());
                // coverity[copy_paste_error]
                if (!osTopLayer.empty())
                {
                    osCurLayer =
                        std::string(osTopLayer).append(".").append(osName);
                }
                else
                    osCurLayer = osName;
                // CPLDebug("PDF", "Layer %s", osCurLayer.c_str());

                const auto oRefPair =
                    std::pair(poObj->GetRefNum().toInt(), poObj->GetRefGen());
                const std::string osPostfixedName =
                    BuildPostfixedLayerNameAndAddLayer(
                        osCurLayer, oRefPair, iPageOfInterest, nPageCount);
                if (osPostfixedName.empty())
                    continue;

                m_aoLayerWithRef.emplace_back(
                    osPostfixedName, poObj->GetRefNum(), poObj->GetRefGen());
                m_oMapLayerNameToOCGNumGenPdfium[osPostfixedName] = oRefPair;
            }
        }
    }
}

/************************************************************************/
/*                         FindLayersPdfium()                          */
/************************************************************************/

void PDFDataset::FindLayersPdfium(int iPageOfInterest)
{
    int nPageCount = 0;
    const auto poPages = GetPagesKids();
    if (poPages)
        nPageCount = poPages->GetLength();

    GDALPDFObject *poCatalog = GetCatalog();
    if (poCatalog == nullptr ||
        poCatalog->GetType() != PDFObjectType_Dictionary)
        return;
    GDALPDFObject *poOrder = poCatalog->LookupObject("OCProperties.D.Order");
    if (poOrder != nullptr && poOrder->GetType() == PDFObjectType_Array)
    {
        ExploreLayersPdfium(poOrder->GetArray(), iPageOfInterest, nPageCount,
                            0);
    }
#if 0
    else
    {
        GDALPDFObject* poOCGs = poD->GetDictionary()->Get("OCGs");
        if( poOCGs != nullptr && poOCGs->GetType() == PDFObjectType_Array )
        {
            GDALPDFArray* poArray = poOCGs->GetArray();
            int nLength = poArray->GetLength();
            for(int i=0;i<nLength;i++)
            {
                GDALPDFObject* poObj = poArray->Get(i);
                if( poObj != nullptr )
                {
                    // TODO ?
                }
            }
        }
    }
#endif

    CreateLayerList();
    m_oMDMD_PDF.SetMetadata(m_aosLayerNames.List(), "LAYERS");
}

/************************************************************************/
/*                       TurnLayersOnOffPdfium()                       */
/************************************************************************/

void PDFDataset::TurnLayersOnOffPdfium()
{
    GDALPDFObject *poCatalog = GetCatalog();
    if (poCatalog == nullptr ||
        poCatalog->GetType() != PDFObjectType_Dictionary)
        return;
    GDALPDFObject *poOCGs = poCatalog->LookupObject("OCProperties.OCGs");
    if (poOCGs == nullptr || poOCGs->GetType() != PDFObjectType_Array)
        return;

    // Which layers to turn ON ?
    const char *pszLayers = GetOption(papszOpenOptions, "LAYERS", nullptr);
    if (pszLayers)
    {
        int i;
        int bAll = EQUAL(pszLayers, "ALL");

        GDALPDFArray *poOCGsArray = poOCGs->GetArray();
        int nLength = poOCGsArray->GetLength();
        for (i = 0; i < nLength; i++)
        {
            GDALPDFObject *poOCG = poOCGsArray->Get(i);
            m_oMapOCGNumGenToVisibilityStatePdfium[std::pair(
                poOCG->GetRefNum().toInt(), poOCG->GetRefGen())] =
                (bAll) ? VISIBILITY_ON : VISIBILITY_OFF;
        }

        char **papszLayers = CSLTokenizeString2(pszLayers, ",", 0);
        for (i = 0; !bAll && papszLayers[i] != nullptr; i++)
        {
            auto oIter = m_oMapLayerNameToOCGNumGenPdfium.find(papszLayers[i]);
            if (oIter != m_oMapLayerNameToOCGNumGenPdfium.end())
            {
                if (oIter->second.first >= 0)
                {
                    // CPLDebug("PDF", "Turn '%s' on", papszLayers[i]);
                    m_oMapOCGNumGenToVisibilityStatePdfium[oIter->second] =
                        VISIBILITY_ON;
                }

                // Turn child layers on, unless there's one of them explicitly
                // listed in the list.
                size_t nLen = strlen(papszLayers[i]);
                int bFoundChildLayer = FALSE;
                oIter = m_oMapLayerNameToOCGNumGenPdfium.begin();
                for (; oIter != m_oMapLayerNameToOCGNumGenPdfium.end() &&
                       !bFoundChildLayer;
                     oIter++)
                {
                    if (oIter->first.size() > nLen &&
                        strncmp(oIter->first.c_str(), papszLayers[i], nLen) ==
                            0 &&
                        oIter->first[nLen] == '.')
                    {
                        for (int j = 0; papszLayers[j] != nullptr; j++)
                        {
                            if (strcmp(papszLayers[j], oIter->first.c_str()) ==
                                0)
                                bFoundChildLayer = TRUE;
                        }
                    }
                }

                if (!bFoundChildLayer)
                {
                    oIter = m_oMapLayerNameToOCGNumGenPdfium.begin();
                    for (; oIter != m_oMapLayerNameToOCGNumGenPdfium.end() &&
                           !bFoundChildLayer;
                         oIter++)
                    {
                        if (oIter->first.size() > nLen &&
                            strncmp(oIter->first.c_str(), papszLayers[i],
                                    nLen) == 0 &&
                            oIter->first[nLen] == '.')
                        {
                            if (oIter->second.first >= 0)
                            {
                                // CPLDebug("PDF", "Turn '%s' on too",
                                // oIter->first.c_str());
                                m_oMapOCGNumGenToVisibilityStatePdfium
                                    [oIter->second] = VISIBILITY_ON;
                            }
                        }
                    }
                }

                // Turn parent layers on too
                char *pszLastDot = nullptr;
                while ((pszLastDot = strrchr(papszLayers[i], '.')) != nullptr)
                {
                    *pszLastDot = '\0';
                    oIter =
                        m_oMapLayerNameToOCGNumGenPdfium.find(papszLayers[i]);
                    if (oIter != m_oMapLayerNameToOCGNumGenPdfium.end())
                    {
                        if (oIter->second.first >= 0)
                        {
                            // CPLDebug("PDF", "Turn '%s' on too",
                            // papszLayers[i]);
                            m_oMapOCGNumGenToVisibilityStatePdfium
                                [oIter->second] = VISIBILITY_ON;
                        }
                    }
                }
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined, "Unknown layer '%s'",
                         papszLayers[i]);
            }
        }
        CSLDestroy(papszLayers);

        m_bUseOCG = true;
    }

    // Which layers to turn OFF ?
    const char *pszLayersOFF =
        GetOption(papszOpenOptions, "LAYERS_OFF", nullptr);
    if (pszLayersOFF)
    {
        char **papszLayersOFF = CSLTokenizeString2(pszLayersOFF, ",", 0);
        for (int i = 0; papszLayersOFF[i] != nullptr; i++)
        {
            auto oIter =
                m_oMapLayerNameToOCGNumGenPdfium.find(papszLayersOFF[i]);
            if (oIter != m_oMapLayerNameToOCGNumGenPdfium.end())
            {
                if (oIter->second.first >= 0)
                {
                    // CPLDebug("PDF", "Turn '%s' (%d,%d) off",
                    // papszLayersOFF[i], oIter->second.first,
                    // oIter->second.second);
                    m_oMapOCGNumGenToVisibilityStatePdfium[oIter->second] =
                        VISIBILITY_OFF;
                }

                // Turn child layers off too
                size_t nLen = strlen(papszLayersOFF[i]);
                oIter = m_oMapLayerNameToOCGNumGenPdfium.begin();
                for (; oIter != m_oMapLayerNameToOCGNumGenPdfium.end(); oIter++)
                {
                    if (oIter->first.size() > nLen &&
                        strncmp(oIter->first.c_str(), papszLayersOFF[i],
                                nLen) == 0 &&
                        oIter->first[nLen] == '.')
                    {
                        if (oIter->second.first >= 0)
                        {
                            // CPLDebug("PDF", "Turn '%s' off too",
                            // oIter->first.c_str());
                            m_oMapOCGNumGenToVisibilityStatePdfium
                                [oIter->second] = VISIBILITY_OFF;
                        }
                    }
                }
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined, "Unknown layer '%s'",
                         papszLayersOFF[i]);
            }
        }
        CSLDestroy(papszLayersOFF);

        m_bUseOCG = true;
    }
}

/************************************************************************/
/*                    GetVisibilityStateForOGCPdfium()                  */
/************************************************************************/

PDFDataset::VisibilityState PDFDataset::GetVisibilityStateForOGCPdfium(int nNum,
                                                                       int nGen)
{
    auto oIter =
        m_oMapOCGNumGenToVisibilityStatePdfium.find(std::pair(nNum, nGen));
    if (oIter == m_oMapOCGNumGenToVisibilityStatePdfium.end())
        return VISIBILITY_DEFAULT;
    return oIter->second;
}

#endif /* HAVE_PDFIUM */

/************************************************************************/
/*                            GetPagesKids()                            */
/************************************************************************/

GDALPDFArray *PDFDataset::GetPagesKids()
{
    const auto poCatalog = GetCatalog();
    if (!poCatalog || poCatalog->GetType() != PDFObjectType_Dictionary)
    {
        return nullptr;
    }
    const auto poKids = poCatalog->LookupObject("Pages.Kids");
    if (!poKids || poKids->GetType() != PDFObjectType_Array)
    {
        return nullptr;
    }
    return poKids->GetArray();
}

/************************************************************************/
/*                           MapOCGsToPages()                           */
/************************************************************************/

void PDFDataset::MapOCGsToPages()
{
    const auto poKidsArray = GetPagesKids();
    if (!poKidsArray)
    {
        return;
    }
    const int nKidsArrayLength = poKidsArray->GetLength();
    for (int iPage = 0; iPage < nKidsArrayLength; ++iPage)
    {
        const auto poPage = poKidsArray->Get(iPage);
        if (poPage && poPage->GetType() == PDFObjectType_Dictionary)
        {
            const auto poXObject = poPage->LookupObject("Resources.XObject");
            if (poXObject && poXObject->GetType() == PDFObjectType_Dictionary)
            {
                for (const auto &oNameObjectPair :
                     poXObject->GetDictionary()->GetValues())
                {
                    const auto poProperties =
                        oNameObjectPair.second->LookupObject(
                            "Resources.Properties");
                    if (poProperties &&
                        poProperties->GetType() == PDFObjectType_Dictionary)
                    {
                        const auto &oMap =
                            poProperties->GetDictionary()->GetValues();
                        for (const auto &[osKey, poObj] : oMap)
                        {
                            if (poObj->GetRefNum().toBool() &&
                                poObj->GetType() == PDFObjectType_Dictionary)
                            {
                                GDALPDFObject *poType =
                                    poObj->GetDictionary()->Get("Type");
                                GDALPDFObject *poName =
                                    poObj->GetDictionary()->Get("Name");
                                if (poType &&
                                    poType->GetType() == PDFObjectType_Name &&
                                    poType->GetName() == "OCG" && poName &&
                                    poName->GetType() == PDFObjectType_String)
                                {
                                    m_oMapOCGNumGenToPages
                                        [std::pair(poObj->GetRefNum().toInt(),
                                                   poObj->GetRefGen())]
                                            .push_back(iPage + 1);
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}

/************************************************************************/
/*                           FindLayerOCG()                             */
/************************************************************************/

CPLString PDFDataset::FindLayerOCG(GDALPDFDictionary *poPageDict,
                                   const char *pszLayerName)
{
    GDALPDFObject *poProperties =
        poPageDict->LookupObject("Resources.Properties");
    if (poProperties != nullptr &&
        poProperties->GetType() == PDFObjectType_Dictionary)
    {
        const auto &oMap = poProperties->GetDictionary()->GetValues();
        for (const auto &[osKey, poObj] : oMap)
        {
            if (poObj->GetRefNum().toBool() &&
                poObj->GetType() == PDFObjectType_Dictionary)
            {
                GDALPDFObject *poType = poObj->GetDictionary()->Get("Type");
                GDALPDFObject *poName = poObj->GetDictionary()->Get("Name");
                if (poType != nullptr &&
                    poType->GetType() == PDFObjectType_Name &&
                    poType->GetName() == "OCG" && poName != nullptr &&
                    poName->GetType() == PDFObjectType_String)
                {
                    if (poName->GetString() == pszLayerName)
                        return osKey;
                }
            }
        }
    }
    return "";
}

/************************************************************************/
/*                         FindLayersGeneric()                          */
/************************************************************************/

void PDFDataset::FindLayersGeneric(GDALPDFDictionary *poPageDict)
{
    GDALPDFObject *poProperties =
        poPageDict->LookupObject("Resources.Properties");
    if (poProperties != nullptr &&
        poProperties->GetType() == PDFObjectType_Dictionary)
    {
        const auto &oMap = poProperties->GetDictionary()->GetValues();
        for (const auto &[osKey, poObj] : oMap)
        {
            if (poObj->GetRefNum().toBool() &&
                poObj->GetType() == PDFObjectType_Dictionary)
            {
                GDALPDFObject *poType = poObj->GetDictionary()->Get("Type");
                GDALPDFObject *poName = poObj->GetDictionary()->Get("Name");
                if (poType != nullptr &&
                    poType->GetType() == PDFObjectType_Name &&
                    poType->GetName() == "OCG" && poName != nullptr &&
                    poName->GetType() == PDFObjectType_String)
                {
                    m_aoLayerWithRef.emplace_back(
                        PDFSanitizeLayerName(poName->GetString().c_str())
                            .c_str(),
                        poObj->GetRefNum(), poObj->GetRefGen());
                }
            }
        }
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

PDFDataset *PDFDataset::Open(GDALOpenInfo *poOpenInfo)

{
    if (!PDFDatasetIdentify(poOpenInfo))
        return nullptr;

    const char *pszUserPwd =
        GetOption(poOpenInfo->papszOpenOptions, "USER_PWD", nullptr);

    const bool bOpenSubdataset = STARTS_WITH(poOpenInfo->pszFilename, "PDF:");
    const bool bOpenSubdatasetImage =
        STARTS_WITH(poOpenInfo->pszFilename, "PDF_IMAGE:");
    int iPage = -1;
    int nImageNum = -1;
    std::string osSubdatasetName;
    const char *pszFilename = poOpenInfo->pszFilename;

    if (bOpenSubdataset)
    {
        iPage = atoi(pszFilename + 4);
        if (iPage <= 0)
            return nullptr;
        pszFilename = strchr(pszFilename + 4, ':');
        if (pszFilename == nullptr)
            return nullptr;
        pszFilename++;
        osSubdatasetName = CPLSPrintf("Page %d", iPage);
    }
    else if (bOpenSubdatasetImage)
    {
        iPage = atoi(pszFilename + 10);
        if (iPage <= 0)
            return nullptr;
        const char *pszNext = strchr(pszFilename + 10, ':');
        if (pszNext == nullptr)
            return nullptr;
        nImageNum = atoi(pszNext + 1);
        if (nImageNum <= 0)
            return nullptr;
        pszFilename = strchr(pszNext + 1, ':');
        if (pszFilename == nullptr)
            return nullptr;
        pszFilename++;
        osSubdatasetName = CPLSPrintf("Image %d", nImageNum);
    }
    else
        iPage = 1;

    std::bitset<PDFLIB_COUNT> bHasLib;
    bHasLib.reset();
    // Each library set their flag
#if defined(HAVE_POPPLER)
    bHasLib.set(PDFLIB_POPPLER);
#endif  // HAVE_POPPLER
#if defined(HAVE_PODOFO)
    bHasLib.set(PDFLIB_PODOFO);
#endif  // HAVE_PODOFO
#if defined(HAVE_PDFIUM)
    bHasLib.set(PDFLIB_PDFIUM);
#endif  // HAVE_PDFIUM

    std::bitset<PDFLIB_COUNT> bUseLib;

    // More than one library available
    // Detect which one
    if (bHasLib.count() != 1)
    {
        const char *pszDefaultLib = bHasLib.test(PDFLIB_PDFIUM)    ? "PDFIUM"
                                    : bHasLib.test(PDFLIB_POPPLER) ? "POPPLER"
                                                                   : "PODOFO";
        const char *pszPDFLib =
            GetOption(poOpenInfo->papszOpenOptions, "PDF_LIB", pszDefaultLib);
        while (true)
        {
            if (EQUAL(pszPDFLib, "POPPLER"))
                bUseLib.set(PDFLIB_POPPLER);
            else if (EQUAL(pszPDFLib, "PODOFO"))
                bUseLib.set(PDFLIB_PODOFO);
            else if (EQUAL(pszPDFLib, "PDFIUM"))
                bUseLib.set(PDFLIB_PDFIUM);

            if (bUseLib.count() != 1 || (bHasLib & bUseLib) == 0)
            {
                CPLDebug("PDF",
                         "Invalid value for GDAL_PDF_LIB config option: %s. "
                         "Fallback to %s",
                         pszPDFLib, pszDefaultLib);
                pszPDFLib = pszDefaultLib;
                bUseLib.reset();
            }
            else
                break;
        }
    }
    else
        bUseLib = bHasLib;

    GDALPDFObject *poPageObj = nullptr;
#ifdef HAVE_POPPLER
    PDFDoc *poDocPoppler = nullptr;
    Page *poPagePoppler = nullptr;
    Catalog *poCatalogPoppler = nullptr;
#endif
#ifdef HAVE_PODOFO
    std::unique_ptr<PoDoFo::PdfMemDocument> poDocPodofo;
    PoDoFo::PdfPage *poPagePodofo = nullptr;
#endif
#ifdef HAVE_PDFIUM
    TPdfiumDocumentStruct *poDocPdfium = nullptr;
    TPdfiumPageStruct *poPagePdfium = nullptr;
#endif
    int nPages = 0;
    VSIVirtualHandleUniquePtr fp;

#ifdef HAVE_POPPLER
    if (bUseLib.test(PDFLIB_POPPLER))
    {
        static bool globalParamsCreatedByGDAL = false;
        {
            CPLMutexHolderD(&hGlobalParamsMutex);
            /* poppler global variable */
            if (globalParams == nullptr)
            {
                globalParamsCreatedByGDAL = true;
                globalParams.reset(new GlobalParams());
            }

            globalParams->setPrintCommands(CPLTestBool(
                CPLGetConfigOption("GDAL_PDF_PRINT_COMMANDS", "FALSE")));
        }

        const auto registerErrorCallback = []()
        {
            /* Set custom error handler for poppler errors */
            setErrorCallback(PDFDatasetErrorFunction);
            assert(globalParams);  // avoid CSA false positive
            globalParams->setErrQuiet(false);
        };

        fp.reset(VSIFOpenL(pszFilename, "rb"));
        if (!fp)
            return nullptr;

#ifdef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
        {
            // Workaround for ossfuzz only due to
            // https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=37584
            // https://gitlab.freedesktop.org/poppler/poppler/-/issues/1137
            GByte *pabyRet = nullptr;
            vsi_l_offset nSize = 0;
            if (VSIIngestFile(fp.get(), pszFilename, &pabyRet, &nSize,
                              10 * 1024 * 1024))
            {
                // Replace nul byte by something else so that strstr() works
                for (size_t i = 0; i < nSize; i++)
                {
                    if (pabyRet[i] == 0)
                        pabyRet[i] = ' ';
                }
                if (strstr(reinterpret_cast<const char *>(pabyRet),
                           "/JBIG2Decode"))
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "/JBIG2Decode found. Giving up due to potential "
                             "very long processing time.");
                    CPLFree(pabyRet);
                    return nullptr;
                }
            }
            CPLFree(pabyRet);
        }
#endif

        fp.reset(VSICreateBufferedReaderHandle(fp.release()));
        while (true)
        {
            fp->Seek(0, SEEK_SET);
            g_nPopplerErrors = 0;
            if (globalParamsCreatedByGDAL)
                registerErrorCallback();
            Object oObj;
            auto poStream =
                new VSIPDFFileStream(fp.get(), pszFilename, std::move(oObj));
#if POPPLER_MAJOR_VERSION > 22 ||                                              \
    (POPPLER_MAJOR_VERSION == 22 && POPPLER_MINOR_VERSION > 2)
            std::optional<GooString> osUserPwd;
            if (pszUserPwd)
                osUserPwd = std::optional<GooString>(pszUserPwd);
            try
            {
                poDocPoppler =
                    new PDFDoc(poStream, std::optional<GooString>(), osUserPwd);
            }
            catch (const std::exception &e)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "PDFDoc::PDFDoc() failed with %s", e.what());
                return nullptr;
            }
#else
            GooString *poUserPwd = nullptr;
            if (pszUserPwd)
                poUserPwd = new GooString(pszUserPwd);
            poDocPoppler = new PDFDoc(poStream, nullptr, poUserPwd);
            delete poUserPwd;
#endif
            if (globalParamsCreatedByGDAL)
                registerErrorCallback();
            if (g_nPopplerErrors >= MAX_POPPLER_ERRORS)
            {
                PDFFreeDoc(poDocPoppler);
                return nullptr;
            }

            if (!poDocPoppler->isOk() || poDocPoppler->getNumPages() == 0)
            {
                if (poDocPoppler->getErrorCode() == errEncrypted)
                {
                    if (pszUserPwd && EQUAL(pszUserPwd, "ASK_INTERACTIVE"))
                    {
                        pszUserPwd =
                            PDFEnterPasswordFromConsoleIfNeeded(pszUserPwd);
                        PDFFreeDoc(poDocPoppler);

                        /* Reset errors that could have been issued during
                         * opening and that */
                        /* did not result in an invalid document */
                        CPLErrorReset();

                        continue;
                    }
                    else if (pszUserPwd == nullptr)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "A password is needed. You can specify it "
                                 "through the PDF_USER_PWD "
                                 "configuration option / USER_PWD open option "
                                 "(that can be set to ASK_INTERACTIVE)");
                    }
                    else
                    {
                        CPLError(CE_Failure, CPLE_AppDefined,
                                 "Invalid password");
                    }
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF");
                }

                PDFFreeDoc(poDocPoppler);
                return nullptr;
            }
            else if (poDocPoppler->isLinearized() &&
                     !poStream->FoundLinearizedHint())
            {
                // This is a likely defect of poppler Linearization.cc file that
                // recognizes a file as linearized if the /Linearized hint is
                // missing, but the content of this dictionary are present. But
                // given the hacks of PDFFreeDoc() and
                // VSIPDFFileStream::FillBuffer() opening such a file will
                // result in a null-ptr deref at closing if we try to access a
                // page and build the page cache, so just exit now
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF");

                PDFFreeDoc(poDocPoppler);
                return nullptr;
            }
            else
            {
                break;
            }
        }

        poCatalogPoppler = poDocPoppler->getCatalog();
        if (poCatalogPoppler == nullptr || !poCatalogPoppler->isOk())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid PDF : invalid catalog");
            PDFFreeDoc(poDocPoppler);
            return nullptr;
        }

        nPages = poDocPoppler->getNumPages();

        if (iPage == 1 && nPages > 10000 &&
            CPLTestBool(CPLGetConfigOption("GDAL_PDF_LIMIT_PAGE_COUNT", "YES")))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "This PDF document reports %d pages. "
                     "Limiting count to 10000 for performance reasons. "
                     "You may remove this limit by setting the "
                     "GDAL_PDF_LIMIT_PAGE_COUNT configuration option to NO",
                     nPages);
            nPages = 10000;
        }

        if (iPage < 1 || iPage > nPages)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid page number (%d/%d)",
                     iPage, nPages);
            PDFFreeDoc(poDocPoppler);
            return nullptr;
        }

        /* Sanity check to validate page count */
        if (iPage > 1 && nPages <= 10000 && iPage != nPages)
        {
            poPagePoppler = poCatalogPoppler->getPage(nPages);
            if (poPagePoppler == nullptr || !poPagePoppler->isOk())
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Invalid PDF : invalid page count");
                PDFFreeDoc(poDocPoppler);
                return nullptr;
            }
        }

        poPagePoppler = poCatalogPoppler->getPage(iPage);
        if (poPagePoppler == nullptr || !poPagePoppler->isOk())
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : invalid page");
            PDFFreeDoc(poDocPoppler);
            return nullptr;
        }

        /* Here's the dirty part: this is a private member */
        /* so we had to #define private public to get it ! */
        Object &oPageObj = poPagePoppler->pageObj;
        if (!oPageObj.isDict())
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid PDF : !oPageObj.isDict()");
            PDFFreeDoc(poDocPoppler);
            return nullptr;
        }

        poPageObj = new GDALPDFObjectPoppler(&oPageObj, FALSE);
        Ref *poPageRef = poCatalogPoppler->getPageRef(iPage);
        if (poPageRef != nullptr)
        {
            ((GDALPDFObjectPoppler *)poPageObj)
                ->SetRefNumAndGen(GDALPDFObjectNum(poPageRef->num),
                                  poPageRef->gen);
        }
    }
#endif  // ~ HAVE_POPPLER

#ifdef HAVE_PODOFO
    if (bUseLib.test(PDFLIB_PODOFO) && poPageObj == nullptr)
    {
#if !(PODOFO_VERSION_MAJOR > 0 ||                                              \
      (PODOFO_VERSION_MAJOR == 0 && PODOFO_VERSION_MINOR >= 10))
        PoDoFo::PdfError::EnableDebug(false);
        PoDoFo::PdfError::EnableLogging(false);
#endif

        poDocPodofo = std::make_unique<PoDoFo::PdfMemDocument>();
        try
        {
            poDocPodofo->Load(pszFilename);
        }
        catch (PoDoFo::PdfError &oError)
        {
#if PODOFO_VERSION_MAJOR > 0 ||                                                \
    (PODOFO_VERSION_MAJOR == 0 && PODOFO_VERSION_MINOR >= 10)
            if (oError.GetCode() == PoDoFo::PdfErrorCode::InvalidPassword)
#else
            if (oError.GetError() == PoDoFo::ePdfError_InvalidPassword)
#endif
            {
                if (pszUserPwd)
                {
                    pszUserPwd =
                        PDFEnterPasswordFromConsoleIfNeeded(pszUserPwd);

                    try
                    {
#if PODOFO_VERSION_MAJOR > 0 ||                                                \
    (PODOFO_VERSION_MAJOR == 0 && PODOFO_VERSION_MINOR >= 10)
                        poDocPodofo =
                            std::make_unique<PoDoFo::PdfMemDocument>();
                        poDocPodofo->Load(pszFilename, pszUserPwd);
#else
                        poDocPodofo->SetPassword(pszUserPwd);
#endif
                    }
                    catch (PoDoFo::PdfError &oError2)
                    {
#if PODOFO_VERSION_MAJOR > 0 ||                                                \
    (PODOFO_VERSION_MAJOR == 0 && PODOFO_VERSION_MINOR >= 10)
                        if (oError2.GetCode() ==
                            PoDoFo::PdfErrorCode::InvalidPassword)
#else
                        if (oError2.GetError() ==
                            PoDoFo::ePdfError_InvalidPassword)
#endif
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Invalid password");
                        }
                        else
                        {
                            CPLError(CE_Failure, CPLE_AppDefined,
                                     "Invalid PDF : %s", oError2.what());
                        }
                        return nullptr;
                    }
                    catch (...)
                    {
                        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF");
                        return nullptr;
                    }
                }
                else
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "A password is needed. You can specify it through "
                             "the PDF_USER_PWD "
                             "configuration option / USER_PWD open option "
                             "(that can be set to ASK_INTERACTIVE)");
                    return nullptr;
                }
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : %s",
                         oError.what());
                return nullptr;
            }
        }
        catch (...)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF");
            return nullptr;
        }

#if PODOFO_VERSION_MAJOR > 0 ||                                                \
    (PODOFO_VERSION_MAJOR == 0 && PODOFO_VERSION_MINOR >= 10)
        auto &oPageCollections = poDocPodofo->GetPages();
        nPages = static_cast<int>(oPageCollections.GetCount());
#else
        nPages = poDocPodofo->GetPageCount();
#endif
        if (iPage < 1 || iPage > nPages)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid page number (%d/%d)",
                     iPage, nPages);
            return nullptr;
        }

        try
        {
#if PODOFO_VERSION_MAJOR > 0 ||                                                \
    (PODOFO_VERSION_MAJOR == 0 && PODOFO_VERSION_MINOR >= 10)
            /* Sanity check to validate page count */
            if (iPage != nPages)
                CPL_IGNORE_RET_VAL(oPageCollections.GetPageAt(nPages - 1));

            poPagePodofo = &oPageCollections.GetPageAt(iPage - 1);
#else
            /* Sanity check to validate page count */
            if (iPage != nPages)
                CPL_IGNORE_RET_VAL(poDocPodofo->GetPage(nPages - 1));

            poPagePodofo = poDocPodofo->GetPage(iPage - 1);
#endif
        }
        catch (PoDoFo::PdfError &oError)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : %s",
                     oError.what());
            return nullptr;
        }
        catch (...)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF");
            return nullptr;
        }

        if (poPagePodofo == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : invalid page");
            return nullptr;
        }

#if PODOFO_VERSION_MAJOR > 0 ||                                                \
    (PODOFO_VERSION_MAJOR == 0 && PODOFO_VERSION_MINOR >= 10)
        const PoDoFo::PdfObject *pObj = &poPagePodofo->GetObject();
#else
        const PoDoFo::PdfObject *pObj = poPagePodofo->GetObject();
#endif
        poPageObj = new GDALPDFObjectPodofo(pObj, poDocPodofo->GetObjects());
    }
#endif  // ~ HAVE_PODOFO

#ifdef HAVE_PDFIUM
    if (bUseLib.test(PDFLIB_PDFIUM) && poPageObj == nullptr)
    {
        if (!LoadPdfiumDocumentPage(pszFilename, pszUserPwd, iPage,
                                    &poDocPdfium, &poPagePdfium, &nPages))
        {
            // CPLError is called inside function
            return nullptr;
        }

        const auto pageObj = poPagePdfium->page->GetDict();
        if (pageObj == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid PDF : invalid page object");
            UnloadPdfiumDocumentPage(&poDocPdfium, &poPagePdfium);
            return nullptr;
        }
        poPageObj = GDALPDFObjectPdfium::Build(pageObj);
        if (poPageObj == nullptr)
            return nullptr;
    }
#endif  // ~ HAVE_PDFIUM

    GDALPDFDictionary *poPageDict = poPageObj->GetDictionary();
    if (poPageDict == nullptr)
    {
        delete poPageObj;

        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid PDF : poPageDict == nullptr");
#ifdef HAVE_POPPLER
        if (bUseLib.test(PDFLIB_POPPLER))
            PDFFreeDoc(poDocPoppler);
#endif
#ifdef HAVE_PDFIUM
        if (bUseLib.test(PDFLIB_PDFIUM))
        {
            UnloadPdfiumDocumentPage(&poDocPdfium, &poPagePdfium);
        }
#endif
        return nullptr;
    }

    const char *pszDumpObject = CPLGetConfigOption("PDF_DUMP_OBJECT", nullptr);
    if (pszDumpObject != nullptr)
    {
        GDALPDFDumper oDumper(pszFilename, pszDumpObject);
        oDumper.Dump(poPageObj);
    }

    PDFDataset *poDS = new PDFDataset();
    poDS->m_fp = std::move(fp);
    poDS->papszOpenOptions = CSLDuplicate(poOpenInfo->papszOpenOptions);
    poDS->m_bUseLib = bUseLib;
    poDS->m_osFilename = pszFilename;
    poDS->eAccess = poOpenInfo->eAccess;

    if (nPages > 1 && !bOpenSubdataset)
    {
        int i;
        CPLStringList aosList;
        for (i = 0; i < nPages; i++)
        {
            char szKey[32];
            snprintf(szKey, sizeof(szKey), "SUBDATASET_%d_NAME", i + 1);
            aosList.AddNameValue(
                szKey, CPLSPrintf("PDF:%d:%s", i + 1, poOpenInfo->pszFilename));
            snprintf(szKey, sizeof(szKey), "SUBDATASET_%d_DESC", i + 1);
            aosList.AddNameValue(szKey, CPLSPrintf("Page %d of %s", i + 1,
                                                   poOpenInfo->pszFilename));
        }
        poDS->SetMetadata(aosList.List(), "SUBDATASETS");
    }

#ifdef HAVE_POPPLER
    poDS->m_poDocPoppler = poDocPoppler;
#endif
#ifdef HAVE_PODOFO
    poDS->m_poDocPodofo = poDocPodofo.release();
#endif
#ifdef HAVE_PDFIUM
    poDS->m_poDocPdfium = poDocPdfium;
    poDS->m_poPagePdfium = poPagePdfium;
#endif
    poDS->m_poPageObj = poPageObj;
    poDS->m_osUserPwd = pszUserPwd ? pszUserPwd : "";
    poDS->m_iPage = iPage;

    const char *pszDumpCatalog =
        CPLGetConfigOption("PDF_DUMP_CATALOG", nullptr);
    if (pszDumpCatalog != nullptr)
    {
        GDALPDFDumper oDumper(pszFilename, pszDumpCatalog);
        oDumper.Dump(poDS->GetCatalog());
    }

    int nBandsGuessed = 0;
    if (nImageNum < 0)
    {
        poDS->GuessDPI(poPageDict, &nBandsGuessed);
        if (nBandsGuessed < 4)
            nBandsGuessed = 0;
    }
    else
    {
        const char *pszDPI =
            GetOption(poOpenInfo->papszOpenOptions, "DPI", nullptr);
        if (pszDPI != nullptr)
        {
            // coverity[tainted_data]
            poDS->m_dfDPI = CPLAtof(pszDPI);
        }
    }

    double dfX1 = 0.0;
    double dfY1 = 0.0;
    double dfX2 = 0.0;
    double dfY2 = 0.0;

#ifdef HAVE_POPPLER
    if (bUseLib.test(PDFLIB_POPPLER))
    {
        const auto *psMediaBox = poPagePoppler->getMediaBox();
        dfX1 = psMediaBox->x1;
        dfY1 = psMediaBox->y1;
        dfX2 = psMediaBox->x2;
        dfY2 = psMediaBox->y2;
    }
#endif

#ifdef HAVE_PODOFO
    if (bUseLib.test(PDFLIB_PODOFO))
    {
        CPLAssert(poPagePodofo);
        auto oMediaBox = poPagePodofo->GetMediaBox();
        dfX1 = oMediaBox.GetLeft();
        dfY1 = oMediaBox.GetBottom();
#if PODOFO_VERSION_MAJOR > 0 ||                                                \
    (PODOFO_VERSION_MAJOR == 0 && PODOFO_VERSION_MINOR >= 10)
        dfX2 = dfX1 + oMediaBox.Width;
        dfY2 = dfY1 + oMediaBox.Height;
#else
        dfX2 = dfX1 + oMediaBox.GetWidth();
        dfY2 = dfY1 + oMediaBox.GetHeight();
#endif
    }
#endif

#ifdef HAVE_PDFIUM
    if (bUseLib.test(PDFLIB_PDFIUM))
    {
        CFX_FloatRect rect = poPagePdfium->page->GetBBox();
        dfX1 = rect.left;
        dfX2 = rect.right;
        dfY1 = rect.bottom;
        dfY2 = rect.top;
    }
#endif  // ~ HAVE_PDFIUM

    double dfUserUnit = poDS->m_dfDPI * USER_UNIT_IN_INCH;
    poDS->m_dfPageWidth = dfX2 - dfX1;
    poDS->m_dfPageHeight = dfY2 - dfY1;
    // CPLDebug("PDF", "left=%f right=%f bottom=%f top=%f", dfX1, dfX2, dfY1,
    // dfY2);
    poDS->nRasterXSize = (int)floor((dfX2 - dfX1) * dfUserUnit + 0.5);
    poDS->nRasterYSize = (int)floor((dfY2 - dfY1) * dfUserUnit + 0.5);

    if (!GDALCheckDatasetDimensions(poDS->nRasterXSize, poDS->nRasterYSize))
    {
        delete poDS;
        return nullptr;
    }

    double dfRotation = 0;
#ifdef HAVE_POPPLER
    if (bUseLib.test(PDFLIB_POPPLER))
        dfRotation = poDocPoppler->getPageRotate(iPage);
#endif

#ifdef HAVE_PODOFO
    if (bUseLib.test(PDFLIB_PODOFO))
    {
        CPLAssert(poPagePodofo);
#if PODOFO_VERSION_MAJOR > 0 ||                                                \
    (PODOFO_VERSION_MAJOR == 0 && PODOFO_VERSION_MINOR >= 10)
        dfRotation = poPagePodofo->GetRotationRaw();
#else
        dfRotation = poPagePodofo->GetRotation();
#endif
    }
#endif

#ifdef HAVE_PDFIUM
    if (bUseLib.test(PDFLIB_PDFIUM))
    {
        dfRotation = poPagePdfium->page->GetPageRotation() * 90;
    }
#endif

    if (dfRotation == 90 || dfRotation == -90 || dfRotation == 270)
    {
/* FIXME: the podofo case should be implemented. This needs to rotate */
/* the output of pdftoppm */
#if defined(HAVE_POPPLER) || defined(HAVE_PDFIUM)
        if (bUseLib.test(PDFLIB_POPPLER) || bUseLib.test(PDFLIB_PDFIUM))
        {
            int nTmp = poDS->nRasterXSize;
            poDS->nRasterXSize = poDS->nRasterYSize;
            poDS->nRasterYSize = nTmp;
        }
#endif
    }

    if (CSLFetchNameValue(poOpenInfo->papszOpenOptions, "@OPEN_FOR_OVERVIEW"))
    {
        poDS->m_nBlockXSize = 512;
        poDS->m_nBlockYSize = 512;
    }
    /* Check if the PDF is only made of regularly tiled images */
    /* (like some USGS GeoPDF production) */
    else if (dfRotation == 0.0 && !poDS->m_asTiles.empty() &&
             EQUAL(GetOption(poOpenInfo->papszOpenOptions, "LAYERS", "ALL"),
                   "ALL"))
    {
        poDS->CheckTiledRaster();
        if (!poDS->m_aiTiles.empty())
            poDS->SetMetadataItem("INTERLEAVE", "PIXEL", "IMAGE_STRUCTURE");
    }

    GDALPDFObject *poLGIDict = nullptr;
    GDALPDFObject *poVP = nullptr;
    int bIsOGCBP = FALSE;
    if ((poLGIDict = poPageDict->Get("LGIDict")) != nullptr && nImageNum < 0)
    {
        /* Cf 08-139r3_GeoPDF_Encoding_Best_Practice_Version_2.2.pdf */
        CPLDebug("PDF", "OGC Encoding Best Practice style detected");
        if (poDS->ParseLGIDictObject(poLGIDict))
        {
            if (poDS->m_bHasCTM)
            {
                if (dfRotation == 90)
                {
                    poDS->m_adfGeoTransform[0] = poDS->m_adfCTM[4];
                    poDS->m_adfGeoTransform[1] = poDS->m_adfCTM[2] / dfUserUnit;
                    poDS->m_adfGeoTransform[2] = poDS->m_adfCTM[0] / dfUserUnit;
                    poDS->m_adfGeoTransform[3] = poDS->m_adfCTM[5];
                    poDS->m_adfGeoTransform[4] = poDS->m_adfCTM[3] / dfUserUnit;
                    poDS->m_adfGeoTransform[5] = poDS->m_adfCTM[1] / dfUserUnit;
                }
                else if (dfRotation == -90 || dfRotation == 270)
                {
                    poDS->m_adfGeoTransform[0] =
                        poDS->m_adfCTM[4] +
                        poDS->m_adfCTM[2] * poDS->m_dfPageHeight +
                        poDS->m_adfCTM[0] * poDS->m_dfPageWidth;
                    poDS->m_adfGeoTransform[1] =
                        -poDS->m_adfCTM[2] / dfUserUnit;
                    poDS->m_adfGeoTransform[2] =
                        -poDS->m_adfCTM[0] / dfUserUnit;
                    poDS->m_adfGeoTransform[3] =
                        poDS->m_adfCTM[5] +
                        poDS->m_adfCTM[3] * poDS->m_dfPageHeight +
                        poDS->m_adfCTM[1] * poDS->m_dfPageWidth;
                    poDS->m_adfGeoTransform[4] =
                        -poDS->m_adfCTM[3] / dfUserUnit;
                    poDS->m_adfGeoTransform[5] =
                        -poDS->m_adfCTM[1] / dfUserUnit;
                }
                else
                {
                    poDS->m_adfGeoTransform[0] = poDS->m_adfCTM[4] +
                                                 poDS->m_adfCTM[2] * dfY2 +
                                                 poDS->m_adfCTM[0] * dfX1;
                    poDS->m_adfGeoTransform[1] = poDS->m_adfCTM[0] / dfUserUnit;
                    poDS->m_adfGeoTransform[2] =
                        -poDS->m_adfCTM[2] / dfUserUnit;
                    poDS->m_adfGeoTransform[3] = poDS->m_adfCTM[5] +
                                                 poDS->m_adfCTM[3] * dfY2 +
                                                 poDS->m_adfCTM[1] * dfX1;
                    poDS->m_adfGeoTransform[4] = poDS->m_adfCTM[1] / dfUserUnit;
                    poDS->m_adfGeoTransform[5] =
                        -poDS->m_adfCTM[3] / dfUserUnit;
                }

                poDS->m_bGeoTransformValid = true;
            }

            bIsOGCBP = TRUE;

            int i;
            for (i = 0; i < poDS->m_nGCPCount; i++)
            {
                if (dfRotation == 90)
                {
                    double dfPixel =
                        poDS->m_pasGCPList[i].dfGCPPixel * dfUserUnit;
                    double dfLine =
                        poDS->m_pasGCPList[i].dfGCPLine * dfUserUnit;
                    poDS->m_pasGCPList[i].dfGCPPixel = dfLine;
                    poDS->m_pasGCPList[i].dfGCPLine = dfPixel;
                }
                else if (dfRotation == -90 || dfRotation == 270)
                {
                    double dfPixel =
                        poDS->m_pasGCPList[i].dfGCPPixel * dfUserUnit;
                    double dfLine =
                        poDS->m_pasGCPList[i].dfGCPLine * dfUserUnit;
                    poDS->m_pasGCPList[i].dfGCPPixel =
                        poDS->nRasterXSize - dfLine;
                    poDS->m_pasGCPList[i].dfGCPLine =
                        poDS->nRasterYSize - dfPixel;
                }
                else
                {
                    poDS->m_pasGCPList[i].dfGCPPixel =
                        (-dfX1 + poDS->m_pasGCPList[i].dfGCPPixel) * dfUserUnit;
                    poDS->m_pasGCPList[i].dfGCPLine =
                        (dfY2 - poDS->m_pasGCPList[i].dfGCPLine) * dfUserUnit;
                }
            }
        }
    }
    else if ((poVP = poPageDict->Get("VP")) != nullptr && nImageNum < 0)
    {
        /* Cf adobe_supplement_iso32000.pdf */
        CPLDebug("PDF", "Adobe ISO32000 style Geospatial PDF perhaps ?");
        if (dfX1 != 0 || dfY1 != 0)
        {
            CPLDebug("PDF", "non null dfX1 or dfY1 values. untested case...");
        }
        poDS->ParseVP(poVP, dfX2 - dfX1, dfY2 - dfY1);
    }
    else
    {
        GDALPDFObject *poXObject =
            poPageDict->LookupObject("Resources.XObject");

        if (poXObject != nullptr &&
            poXObject->GetType() == PDFObjectType_Dictionary)
        {
            GDALPDFDictionary *poXObjectDict = poXObject->GetDictionary();
            const auto &oMap = poXObjectDict->GetValues();
            int nSubDataset = 0;
            for (const auto &[osKey, poObj] : oMap)
            {
                if (poObj->GetType() == PDFObjectType_Dictionary)
                {
                    GDALPDFDictionary *poDict = poObj->GetDictionary();
                    GDALPDFObject *poSubtype = nullptr;
                    GDALPDFObject *poMeasure = nullptr;
                    GDALPDFObject *poWidth = nullptr;
                    GDALPDFObject *poHeight = nullptr;
                    int nW = 0;
                    int nH = 0;
                    if ((poSubtype = poDict->Get("Subtype")) != nullptr &&
                        poSubtype->GetType() == PDFObjectType_Name &&
                        poSubtype->GetName() == "Image" &&
                        (poMeasure = poDict->Get("Measure")) != nullptr &&
                        poMeasure->GetType() == PDFObjectType_Dictionary &&
                        (poWidth = poDict->Get("Width")) != nullptr &&
                        poWidth->GetType() == PDFObjectType_Int &&
                        (nW = poWidth->GetInt()) > 0 &&
                        (poHeight = poDict->Get("Height")) != nullptr &&
                        poHeight->GetType() == PDFObjectType_Int &&
                        (nH = poHeight->GetInt()) > 0)
                    {
                        if (nImageNum < 0)
                            CPLDebug("PDF",
                                     "Measure found on Image object (%d)",
                                     poObj->GetRefNum().toInt());

                        GDALPDFObject *poColorSpace = poDict->Get("ColorSpace");
                        GDALPDFObject *poBitsPerComponent =
                            poDict->Get("BitsPerComponent");
                        if (poObj->GetRefNum().toBool() &&
                            poObj->GetRefGen() == 0 &&
                            poColorSpace != nullptr &&
                            poColorSpace->GetType() == PDFObjectType_Name &&
                            (poColorSpace->GetName() == "DeviceGray" ||
                             poColorSpace->GetName() == "DeviceRGB") &&
                            (poBitsPerComponent == nullptr ||
                             (poBitsPerComponent->GetType() ==
                                  PDFObjectType_Int &&
                              poBitsPerComponent->GetInt() == 8)))
                        {
                            if (nImageNum < 0)
                            {
                                nSubDataset++;
                                poDS->SetMetadataItem(
                                    CPLSPrintf("SUBDATASET_%d_NAME",
                                               nSubDataset),
                                    CPLSPrintf("PDF_IMAGE:%d:%d:%s", iPage,
                                               poObj->GetRefNum().toInt(),
                                               pszFilename),
                                    "SUBDATASETS");
                                poDS->SetMetadataItem(
                                    CPLSPrintf("SUBDATASET_%d_DESC",
                                               nSubDataset),
                                    CPLSPrintf("Georeferenced image of size "
                                               "%dx%d of page %d of %s",
                                               nW, nH, iPage, pszFilename),
                                    "SUBDATASETS");
                            }
                            else if (poObj->GetRefNum().toInt() == nImageNum)
                            {
                                poDS->nRasterXSize = nW;
                                poDS->nRasterYSize = nH;
                                poDS->ParseMeasure(poMeasure, nW, nH, 0, nH, nW,
                                                   0);
                                poDS->m_poImageObj = poObj;
                                if (poColorSpace->GetName() == "DeviceGray")
                                    nBandsGuessed = 1;
                                break;
                            }
                        }
                    }
                }
            }
        }

        if (nImageNum >= 0 && poDS->m_poImageObj == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find image %d",
                     nImageNum);
            delete poDS;
            return nullptr;
        }

        /* Not a geospatial PDF doc */
    }

    /* If pixel size or top left coordinates are very close to an int, round
     * them to the int */
    double dfEps = (fabs(poDS->m_adfGeoTransform[0]) > 1e5 &&
                    fabs(poDS->m_adfGeoTransform[3]) > 1e5)
                       ? 1e-5
                       : 1e-8;
    poDS->m_adfGeoTransform[0] =
        ROUND_TO_INT_IF_CLOSE(poDS->m_adfGeoTransform[0], dfEps);
    poDS->m_adfGeoTransform[1] =
        ROUND_TO_INT_IF_CLOSE(poDS->m_adfGeoTransform[1]);
    poDS->m_adfGeoTransform[3] =
        ROUND_TO_INT_IF_CLOSE(poDS->m_adfGeoTransform[3], dfEps);
    poDS->m_adfGeoTransform[5] =
        ROUND_TO_INT_IF_CLOSE(poDS->m_adfGeoTransform[5]);

    if (bUseLib.test(PDFLIB_PDFIUM))
    {
        // Attempt to "fix" the loss of precision due to the use of float32 for
        // numbers by pdfium
        if ((fabs(poDS->m_adfGeoTransform[0]) > 1e5 ||
             fabs(poDS->m_adfGeoTransform[3]) > 1e5) &&
            fabs(poDS->m_adfGeoTransform[0] -
                 (int)floor(poDS->m_adfGeoTransform[0] + 0.5)) <
                1e-6 * fabs(poDS->m_adfGeoTransform[0]) &&
            fabs(poDS->m_adfGeoTransform[1] -
                 (int)floor(poDS->m_adfGeoTransform[1] + 0.5)) <
                1e-3 * fabs(poDS->m_adfGeoTransform[1]) &&
            fabs(poDS->m_adfGeoTransform[3] -
                 (int)floor(poDS->m_adfGeoTransform[3] + 0.5)) <
                1e-6 * fabs(poDS->m_adfGeoTransform[3]) &&
            fabs(poDS->m_adfGeoTransform[5] -
                 (int)floor(poDS->m_adfGeoTransform[5] + 0.5)) <
                1e-3 * fabs(poDS->m_adfGeoTransform[5]))
        {
            for (int i = 0; i < 6; i++)
            {
                poDS->m_adfGeoTransform[i] =
                    (int)floor(poDS->m_adfGeoTransform[i] + 0.5);
            }
        }
    }

    if (poDS->m_poNeatLine)
    {
        char *pszNeatLineWkt = nullptr;
        OGRLinearRing *poRing = poDS->m_poNeatLine->getExteriorRing();
        /* Adobe style is already in target SRS units */
        if (bIsOGCBP)
        {
            int nPoints = poRing->getNumPoints();
            int i;

            for (i = 0; i < nPoints; i++)
            {
                double x, y;
                if (dfRotation == 90.0)
                {
                    x = poRing->getY(i) * dfUserUnit;
                    y = poRing->getX(i) * dfUserUnit;
                }
                else if (dfRotation == -90.0 || dfRotation == 270.0)
                {
                    x = poDS->nRasterXSize - poRing->getY(i) * dfUserUnit;
                    y = poDS->nRasterYSize - poRing->getX(i) * dfUserUnit;
                }
                else
                {
                    x = (-dfX1 + poRing->getX(i)) * dfUserUnit;
                    y = (dfY2 - poRing->getY(i)) * dfUserUnit;
                }
                double X = poDS->m_adfGeoTransform[0] +
                           x * poDS->m_adfGeoTransform[1] +
                           y * poDS->m_adfGeoTransform[2];
                double Y = poDS->m_adfGeoTransform[3] +
                           x * poDS->m_adfGeoTransform[4] +
                           y * poDS->m_adfGeoTransform[5];
                poRing->setPoint(i, X, Y);
            }
        }
        poRing->closeRings();

        poDS->m_poNeatLine->exportToWkt(&pszNeatLineWkt);
        if (nImageNum < 0)
            poDS->SetMetadataItem("NEATLINE", pszNeatLineWkt);
        CPLFree(pszNeatLineWkt);
    }

    poDS->MapOCGsToPages();

#ifdef HAVE_POPPLER
    if (bUseLib.test(PDFLIB_POPPLER))
    {
        auto poMetadata = poCatalogPoppler->readMetadata();
        if (poMetadata)
        {
            const char *pszContent = poMetadata->c_str();
            if (pszContent != nullptr &&
                STARTS_WITH(pszContent, "<?xpacket begin="))
            {
                const char *const apszMDList[2] = {pszContent, nullptr};
                poDS->SetMetadata(const_cast<char **>(apszMDList), "xml:XMP");
            }
#if (POPPLER_MAJOR_VERSION < 21 ||                                             \
     (POPPLER_MAJOR_VERSION == 21 && POPPLER_MINOR_VERSION < 10))
            delete poMetadata;
#endif
        }

        /* Read Info object */
        /* The test is necessary since with some corrupted PDFs
         * poDocPoppler->getDocInfo() */
        /* might abort() */
        if (poDocPoppler->getXRef()->isOk())
        {
            Object oInfo = poDocPoppler->getDocInfo();
            GDALPDFObjectPoppler oInfoObjPoppler(&oInfo, FALSE);
            poDS->ParseInfo(&oInfoObjPoppler);
        }

        /* Find layers */
        poDS->FindLayersPoppler(
            (bOpenSubdataset || bOpenSubdatasetImage) ? iPage : 0);

        /* Turn user specified layers on or off */
        poDS->TurnLayersOnOffPoppler();
    }
#endif

#ifdef HAVE_PODOFO
    if (bUseLib.test(PDFLIB_PODOFO))
    {
        for (const auto &obj : poDS->m_poDocPodofo->GetObjects())
        {
            GDALPDFObjectPodofo oObjPodofo(obj,
                                           poDS->m_poDocPodofo->GetObjects());
            poDS->FindXMP(&oObjPodofo);
        }

        /* Find layers */
        poDS->FindLayersGeneric(poPageDict);

        /* Read Info object */
        const PoDoFo::PdfInfo *poInfo = poDS->m_poDocPodofo->GetInfo();
        if (poInfo != nullptr)
        {
            GDALPDFObjectPodofo oInfoObjPodofo(
#if PODOFO_VERSION_MAJOR > 0 ||                                                \
    (PODOFO_VERSION_MAJOR == 0 && PODOFO_VERSION_MINOR >= 10)
                &(poInfo->GetObject()),
#else
                poInfo->GetObject(),
#endif
                poDS->m_poDocPodofo->GetObjects());
            poDS->ParseInfo(&oInfoObjPodofo);
        }
    }
#endif
#ifdef HAVE_PDFIUM
    if (bUseLib.test(PDFLIB_PDFIUM))
    {
        // coverity is confused by WrapRetain(), believing that multiple
        // smart pointers manage the same raw pointer. Which is actually
        // true, but a RetainPtr holds a reference counted object. It is
        // thus safe to have several RetainPtr holding it.
        // coverity[multiple_init_smart_ptr]
        GDALPDFObjectPdfium *poRoot = GDALPDFObjectPdfium::Build(
            pdfium::WrapRetain(poDocPdfium->doc->GetRoot()));
        if (poRoot->GetType() == PDFObjectType_Dictionary)
        {
            GDALPDFDictionary *poDict = poRoot->GetDictionary();
            GDALPDFObject *poMetadata(poDict->Get("Metadata"));
            if (poMetadata != nullptr)
            {
                GDALPDFStream *poStream = poMetadata->GetStream();
                if (poStream != nullptr)
                {
                    char *pszContent = poStream->GetBytes();
                    const auto nLength = poStream->GetLength();
                    if (pszContent != nullptr && nLength > 15 &&
                        STARTS_WITH(pszContent, "<?xpacket begin="))
                    {
                        char *apszMDList[2];
                        apszMDList[0] = pszContent;
                        apszMDList[1] = nullptr;
                        poDS->SetMetadata(apszMDList, "xml:XMP");
                    }
                    CPLFree(pszContent);
                }
            }
        }
        delete poRoot;

        /* Find layers */
        poDS->FindLayersPdfium((bOpenSubdataset || bOpenSubdatasetImage) ? iPage
                                                                         : 0);

        /* Turn user specified layers on or off */
        poDS->TurnLayersOnOffPdfium();

        GDALPDFObjectPdfium *poInfo =
            GDALPDFObjectPdfium::Build(poDocPdfium->doc->GetInfo());
        if (poInfo)
        {
            /* Read Info object */
            poDS->ParseInfo(poInfo);
            delete poInfo;
        }
    }
#endif  // ~ HAVE_PDFIUM

    int nBands = 3;
#ifdef HAVE_PDFIUM
    // Use Alpha channel for PDFIUM as default format RGBA
    if (bUseLib.test(PDFLIB_PDFIUM))
        nBands = 4;
#endif
    if (nBandsGuessed)
        nBands = nBandsGuessed;
    const char *pszPDFBands =
        GetOption(poOpenInfo->papszOpenOptions, "BANDS", nullptr);
    if (pszPDFBands)
    {
        nBands = atoi(pszPDFBands);
        if (nBands != 3 && nBands != 4)
        {
            CPLError(CE_Warning, CPLE_NotSupported,
                     "Invalid value for GDAL_PDF_BANDS. Using 3 as a fallback");
            nBands = 3;
        }
    }
#ifdef HAVE_PODOFO
    if (bUseLib.test(PDFLIB_PODOFO) && nBands == 4 && poDS->m_aiTiles.empty())
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "GDAL_PDF_BANDS=4 not supported when PDF driver is compiled "
                 "against Podofo. "
                 "Using 3 as a fallback");
        nBands = 3;
    }
#endif

    int iBand;
    for (iBand = 1; iBand <= nBands; iBand++)
    {
        if (poDS->m_poImageObj != nullptr)
            poDS->SetBand(iBand, new PDFImageRasterBand(poDS, iBand));
        else
            poDS->SetBand(iBand, new PDFRasterBand(poDS, iBand, 0));
    }

    /* Check if this is a raster-only PDF file and that we are */
    /* opened in vector-only mode */
    if ((poOpenInfo->nOpenFlags & GDAL_OF_RASTER) == 0 &&
        (poOpenInfo->nOpenFlags & GDAL_OF_VECTOR) != 0 &&
        !poDS->OpenVectorLayers(poPageDict))
    {
        CPLDebug("PDF", "This is a raster-only PDF dataset, "
                        "but it has been opened in vector-only mode");
        /* Clear dirty flag */
        poDS->m_bProjDirty = false;
        poDS->m_bNeatLineDirty = false;
        poDS->m_bInfoDirty = false;
        poDS->m_bXMPDirty = false;
        delete poDS;
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Initialize any PAM information.                                 */
    /* -------------------------------------------------------------------- */
    if (bOpenSubdataset || bOpenSubdatasetImage)
    {
        poDS->SetPhysicalFilename(pszFilename);
        poDS->SetSubdatasetName(osSubdatasetName.c_str());
    }
    else
    {
        poDS->SetDescription(poOpenInfo->pszFilename);
    }

    poDS->TryLoadXML();

    /* -------------------------------------------------------------------- */
    /*      Support overviews.                                              */
    /* -------------------------------------------------------------------- */
    if (!CSLFetchNameValue(poOpenInfo->papszOpenOptions, "@OPEN_FOR_OVERVIEW"))
    {
        poDS->oOvManager.Initialize(poDS, poOpenInfo->pszFilename);
    }

    /* Clear dirty flag */
    poDS->m_bProjDirty = false;
    poDS->m_bNeatLineDirty = false;
    poDS->m_bInfoDirty = false;
    poDS->m_bXMPDirty = false;

    return (poDS);
}

/************************************************************************/
/*                       ParseLGIDictObject()                           */
/************************************************************************/

int PDFDataset::ParseLGIDictObject(GDALPDFObject *poLGIDict)
{
    bool bOK = false;
    if (poLGIDict->GetType() == PDFObjectType_Array)
    {
        GDALPDFArray *poArray = poLGIDict->GetArray();
        int nArrayLength = poArray->GetLength();
        int iMax = -1;
        GDALPDFObject *poArrayElt = nullptr;
        for (int i = 0; i < nArrayLength; i++)
        {
            if ((poArrayElt = poArray->Get(i)) == nullptr ||
                poArrayElt->GetType() != PDFObjectType_Dictionary)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "LGIDict[%d] is not a dictionary", i);
                return FALSE;
            }

            int bIsBestCandidate = FALSE;
            if (ParseLGIDictDictFirstPass(poArrayElt->GetDictionary(),
                                          &bIsBestCandidate))
            {
                if (bIsBestCandidate || iMax < 0)
                    iMax = i;
            }
        }

        if (iMax < 0)
            return FALSE;

        poArrayElt = poArray->Get(iMax);
        bOK = CPL_TO_BOOL(
            ParseLGIDictDictSecondPass(poArrayElt->GetDictionary()));
    }
    else if (poLGIDict->GetType() == PDFObjectType_Dictionary)
    {
        bOK = ParseLGIDictDictFirstPass(poLGIDict->GetDictionary()) &&
              ParseLGIDictDictSecondPass(poLGIDict->GetDictionary());
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined, "LGIDict is of type %s",
                 poLGIDict->GetTypeName());
    }

    return bOK;
}

/************************************************************************/
/*                            Get()                                     */
/************************************************************************/

static double Get(GDALPDFObject *poObj, int nIndice)
{
    if (poObj->GetType() == PDFObjectType_Array && nIndice >= 0)
    {
        poObj = poObj->GetArray()->Get(nIndice);
        if (poObj == nullptr)
            return 0;
        return Get(poObj);
    }
    else if (poObj->GetType() == PDFObjectType_Int)
        return poObj->GetInt();
    else if (poObj->GetType() == PDFObjectType_Real)
        return poObj->GetReal();
    else if (poObj->GetType() == PDFObjectType_String)
    {
        const char *pszStr = poObj->GetString().c_str();
        size_t nLen = strlen(pszStr);
        if (nLen == 0)
            return 0;
        /* cf Military_Installations_2008.pdf that has values like "96 0 0.0W"
         */
        char chLast = pszStr[nLen - 1];
        if (chLast == 'W' || chLast == 'E' || chLast == 'N' || chLast == 'S')
        {
            double dfDeg = CPLAtof(pszStr);
            double dfMin = 0.0;
            double dfSec = 0.0;
            const char *pszNext = strchr(pszStr, ' ');
            if (pszNext)
                pszNext++;
            if (pszNext)
                dfMin = CPLAtof(pszNext);
            if (pszNext)
                pszNext = strchr(pszNext, ' ');
            if (pszNext)
                pszNext++;
            if (pszNext)
                dfSec = CPLAtof(pszNext);
            double dfVal = dfDeg + dfMin / 60 + dfSec / 3600;
            if (chLast == 'W' || chLast == 'S')
                return -dfVal;
            else
                return dfVal;
        }
        return CPLAtof(pszStr);
    }
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined, "Unexpected type : %s",
                 poObj->GetTypeName());
        return 0;
    }
}

/************************************************************************/
/*                            Get()                                */
/************************************************************************/

static double Get(GDALPDFDictionary *poDict, const char *pszName)
{
    GDALPDFObject *poObj = poDict->Get(pszName);
    if (poObj != nullptr)
        return Get(poObj);
    CPLError(CE_Failure, CPLE_AppDefined, "Cannot find parameter %s", pszName);
    return 0;
}

/************************************************************************/
/*                   ParseLGIDictDictFirstPass()                        */
/************************************************************************/

int PDFDataset::ParseLGIDictDictFirstPass(GDALPDFDictionary *poLGIDict,
                                          int *pbIsBestCandidate)
{
    if (pbIsBestCandidate)
        *pbIsBestCandidate = FALSE;

    if (poLGIDict == nullptr)
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Extract Type attribute                                          */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poType = poLGIDict->Get("Type");
    if (poType == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Type of LGIDict object");
        return FALSE;
    }

    if (poType->GetType() != PDFObjectType_Name)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid type for Type of LGIDict object");
        return FALSE;
    }

    if (strcmp(poType->GetName().c_str(), "LGIDict") != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid value for Type of LGIDict object : %s",
                 poType->GetName().c_str());
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Extract Version attribute                                       */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poVersion = poLGIDict->Get("Version");
    if (poVersion == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Version of LGIDict object");
        return FALSE;
    }

    if (poVersion->GetType() == PDFObjectType_String)
    {
        /* OGC best practice is 2.1 */
        CPLDebug("PDF", "LGIDict Version : %s", poVersion->GetString().c_str());
    }
    else if (poVersion->GetType() == PDFObjectType_Int)
    {
        /* Old TerraGo is 2 */
        CPLDebug("PDF", "LGIDict Version : %d", poVersion->GetInt());
    }

    /* USGS PDF maps have several LGIDict. Keep the one whose description */
    /* is "Map Layers" by default */
    const char *pszNeatlineToSelect =
        GetOption(papszOpenOptions, "NEATLINE", "Map Layers");

    /* -------------------------------------------------------------------- */
    /*      Extract Neatline attribute                                      */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poNeatline = poLGIDict->Get("Neatline");
    if (poNeatline != nullptr && poNeatline->GetType() == PDFObjectType_Array)
    {
        int nLength = poNeatline->GetArray()->GetLength();
        if ((nLength % 2) != 0 || nLength < 4)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid length for Neatline");
            return FALSE;
        }

        GDALPDFObject *poDescription = poLGIDict->Get("Description");
        bool bIsAskedNeatline = false;
        if (poDescription != nullptr &&
            poDescription->GetType() == PDFObjectType_String)
        {
            CPLDebug("PDF", "Description = %s",
                     poDescription->GetString().c_str());

            if (EQUAL(poDescription->GetString().c_str(), pszNeatlineToSelect))
            {
                m_dfMaxArea = 1e300;
                bIsAskedNeatline = true;
            }
        }

        if (!bIsAskedNeatline)
        {
            double dfMinX = 0.0;
            double dfMinY = 0.0;
            double dfMaxX = 0.0;
            double dfMaxY = 0.0;
            for (int i = 0; i < nLength; i += 2)
            {
                double dfX = Get(poNeatline, i);
                double dfY = Get(poNeatline, i + 1);
                if (i == 0 || dfX < dfMinX)
                    dfMinX = dfX;
                if (i == 0 || dfY < dfMinY)
                    dfMinY = dfY;
                if (i == 0 || dfX > dfMaxX)
                    dfMaxX = dfX;
                if (i == 0 || dfY > dfMaxY)
                    dfMaxY = dfY;
            }
            double dfArea = (dfMaxX - dfMinX) * (dfMaxY - dfMinY);
            if (dfArea < m_dfMaxArea)
            {
                CPLDebug("PDF", "Not the largest neatline. Skipping it");
                return TRUE;
            }

            CPLDebug("PDF", "This is the largest neatline for now");
            m_dfMaxArea = dfArea;
        }
        else
            CPLDebug("PDF", "The \"%s\" registration will be selected",
                     pszNeatlineToSelect);

        if (pbIsBestCandidate)
            *pbIsBestCandidate = TRUE;

        delete m_poNeatLine;
        m_poNeatLine = new OGRPolygon();
        OGRLinearRing *poRing = new OGRLinearRing();
        if (nLength == 4)
        {
            /* 2 points only ? They are the bounding box */
            double dfX1 = Get(poNeatline, 0);
            double dfY1 = Get(poNeatline, 1);
            double dfX2 = Get(poNeatline, 2);
            double dfY2 = Get(poNeatline, 3);
            poRing->addPoint(dfX1, dfY1);
            poRing->addPoint(dfX2, dfY1);
            poRing->addPoint(dfX2, dfY2);
            poRing->addPoint(dfX1, dfY2);
        }
        else
        {
            for (int i = 0; i < nLength; i += 2)
            {
                double dfX = Get(poNeatline, i);
                double dfY = Get(poNeatline, i + 1);
                poRing->addPoint(dfX, dfY);
            }
        }
        m_poNeatLine->addRingDirectly(poRing);
    }

    return TRUE;
}

/************************************************************************/
/*                  ParseLGIDictDictSecondPass()                        */
/************************************************************************/

int PDFDataset::ParseLGIDictDictSecondPass(GDALPDFDictionary *poLGIDict)
{
    int i;

    /* -------------------------------------------------------------------- */
    /*      Extract Description attribute                                   */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poDescription = poLGIDict->Get("Description");
    if (poDescription != nullptr &&
        poDescription->GetType() == PDFObjectType_String)
    {
        CPLDebug("PDF", "Description = %s", poDescription->GetString().c_str());
    }

    /* -------------------------------------------------------------------- */
    /*      Extract CTM attribute                                           */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poCTM = poLGIDict->Get("CTM");
    m_bHasCTM = false;
    if (poCTM != nullptr && poCTM->GetType() == PDFObjectType_Array &&
        CPLTestBool(CPLGetConfigOption("PDF_USE_CTM", "YES")))
    {
        int nLength = poCTM->GetArray()->GetLength();
        if (nLength != 6)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid length for CTM");
            return FALSE;
        }

        m_bHasCTM = true;
        for (i = 0; i < nLength; i++)
        {
            m_adfCTM[i] = Get(poCTM, i);
            /* Nullify rotation terms that are significantly smaller than */
            /* scaling terms. */
            if ((i == 1 || i == 2) &&
                fabs(m_adfCTM[i]) < fabs(m_adfCTM[0]) * 1e-10)
                m_adfCTM[i] = 0;
            CPLDebug("PDF", "CTM[%d] = %.16g", i, m_adfCTM[i]);
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Extract Registration attribute                                  */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poRegistration = poLGIDict->Get("Registration");
    if (poRegistration != nullptr &&
        poRegistration->GetType() == PDFObjectType_Array)
    {
        GDALPDFArray *poRegistrationArray = poRegistration->GetArray();
        int nLength = poRegistrationArray->GetLength();
        if (nLength > 4 || (!m_bHasCTM && nLength >= 2) ||
            CPLTestBool(CPLGetConfigOption("PDF_REPORT_GCPS", "NO")))
        {
            m_nGCPCount = 0;
            m_pasGCPList = (GDAL_GCP *)CPLCalloc(sizeof(GDAL_GCP), nLength);

            for (i = 0; i < nLength; i++)
            {
                GDALPDFObject *poGCP = poRegistrationArray->Get(i);
                if (poGCP != nullptr &&
                    poGCP->GetType() == PDFObjectType_Array &&
                    poGCP->GetArray()->GetLength() == 4)
                {
                    double dfUserX = Get(poGCP, 0);
                    double dfUserY = Get(poGCP, 1);
                    double dfX = Get(poGCP, 2);
                    double dfY = Get(poGCP, 3);
                    CPLDebug("PDF", "GCP[%d].userX = %.16g", i, dfUserX);
                    CPLDebug("PDF", "GCP[%d].userY = %.16g", i, dfUserY);
                    CPLDebug("PDF", "GCP[%d].x = %.16g", i, dfX);
                    CPLDebug("PDF", "GCP[%d].y = %.16g", i, dfY);

                    char szID[32];
                    snprintf(szID, sizeof(szID), "%d", m_nGCPCount + 1);
                    m_pasGCPList[m_nGCPCount].pszId = CPLStrdup(szID);
                    m_pasGCPList[m_nGCPCount].pszInfo = CPLStrdup("");
                    m_pasGCPList[m_nGCPCount].dfGCPPixel = dfUserX;
                    m_pasGCPList[m_nGCPCount].dfGCPLine = dfUserY;
                    m_pasGCPList[m_nGCPCount].dfGCPX = dfX;
                    m_pasGCPList[m_nGCPCount].dfGCPY = dfY;
                    m_nGCPCount++;
                }
            }

            if (m_nGCPCount == 0)
            {
                CPLFree(m_pasGCPList);
                m_pasGCPList = nullptr;
            }
        }
    }

    if (!m_bHasCTM && m_nGCPCount == 0)
    {
        CPLDebug("PDF", "Neither CTM nor Registration found");
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Extract Projection attribute                                    */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poProjection = poLGIDict->Get("Projection");
    if (poProjection == nullptr ||
        poProjection->GetType() != PDFObjectType_Dictionary)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Could not find Projection");
        return FALSE;
    }

    return ParseProjDict(poProjection->GetDictionary());
}

/************************************************************************/
/*                         ParseProjDict()                               */
/************************************************************************/

int PDFDataset::ParseProjDict(GDALPDFDictionary *poProjDict)
{
    if (poProjDict == nullptr)
        return FALSE;
    OGRSpatialReference oSRS;
    oSRS.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

    /* -------------------------------------------------------------------- */
    /*      Extract WKT attribute (GDAL extension)                          */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poWKT = poProjDict->Get("WKT");
    if (poWKT != nullptr && poWKT->GetType() == PDFObjectType_String &&
        CPLTestBool(CPLGetConfigOption("GDAL_PDF_OGC_BP_READ_WKT", "TRUE")))
    {
        CPLDebug("PDF", "Found WKT attribute (GDAL extension). Using it");
        const char *pszWKTRead = poWKT->GetString().c_str();
        if (pszWKTRead[0] != 0)
            m_oSRS.importFromWkt(pszWKTRead);
        return TRUE;
    }

    /* -------------------------------------------------------------------- */
    /*      Extract Type attribute                                          */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poType = poProjDict->Get("Type");
    if (poType == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find Type of Projection object");
        return FALSE;
    }

    if (poType->GetType() != PDFObjectType_Name)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid type for Type of Projection object");
        return FALSE;
    }

    if (strcmp(poType->GetName().c_str(), "Projection") != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid value for Type of Projection object : %s",
                 poType->GetName().c_str());
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Extract Datum attribute                                         */
    /* -------------------------------------------------------------------- */
    int bIsWGS84 = FALSE;
    int bIsNAD83 = FALSE;
    /* int bIsNAD27 = FALSE; */

    GDALPDFObject *poDatum = poProjDict->Get("Datum");
    if (poDatum != nullptr)
    {
        if (poDatum->GetType() == PDFObjectType_String)
        {
            /* Using Annex A of
             * http://portal.opengeospatial.org/files/?artifact_id=40537 */
            const char *pszDatum = poDatum->GetString().c_str();
            CPLDebug("PDF", "Datum = %s", pszDatum);
            if (EQUAL(pszDatum, "WE") || EQUAL(pszDatum, "WGE"))
            {
                bIsWGS84 = TRUE;
                oSRS.SetWellKnownGeogCS("WGS84");
            }
            else if (EQUAL(pszDatum, "NAR") || STARTS_WITH_CI(pszDatum, "NAR-"))
            {
                bIsNAD83 = TRUE;
                oSRS.SetWellKnownGeogCS("NAD83");
            }
            else if (EQUAL(pszDatum, "NAS") || STARTS_WITH_CI(pszDatum, "NAS-"))
            {
                /* bIsNAD27 = TRUE; */
                oSRS.SetWellKnownGeogCS("NAD27");
            }
            else if (EQUAL(pszDatum, "HEN")) /* HERAT North, Afghanistan */
            {
                oSRS.SetGeogCS("unknown" /*const char * pszGeogName*/,
                               "unknown" /*const char * pszDatumName */,
                               "International 1924", 6378388, 297);
                oSRS.SetTOWGS84(-333, -222, 114);
            }
            else if (EQUAL(pszDatum, "ING-A")) /* INDIAN 1960, Vietnam 16N */
            {
                oSRS.importFromEPSG(4131);
            }
            else if (EQUAL(pszDatum, "GDS")) /* Geocentric Datum of Australia */
            {
                oSRS.importFromEPSG(4283);
            }
            else if (STARTS_WITH_CI(pszDatum, "OHA-")) /* Old Hawaiian */
            {
                oSRS.importFromEPSG(4135); /* matches OHA-M (Mean) */
                if (!EQUAL(pszDatum, "OHA-M"))
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Using OHA-M (Old Hawaiian Mean) definition for "
                             "%s. Potential issue with datum shift parameters",
                             pszDatum);
                    OGR_SRSNode *poNode = oSRS.GetRoot();
                    int iChild = poNode->FindChild("AUTHORITY");
                    if (iChild != -1)
                        poNode->DestroyChild(iChild);
                    iChild = poNode->FindChild("DATUM");
                    if (iChild != -1)
                    {
                        poNode = poNode->GetChild(iChild);
                        iChild = poNode->FindChild("AUTHORITY");
                        if (iChild != -1)
                            poNode->DestroyChild(iChild);
                    }
                }
            }
            else
            {
                CPLError(CE_Warning, CPLE_AppDefined,
                         "Unhandled (yet) value for Datum : %s. Defaulting to "
                         "WGS84...",
                         pszDatum);
                oSRS.SetGeogCS("unknown" /*const char * pszGeogName*/,
                               "unknown" /*const char * pszDatumName */,
                               "unknown", 6378137, 298.257223563);
            }
        }
        else if (poDatum->GetType() == PDFObjectType_Dictionary)
        {
            GDALPDFDictionary *poDatumDict = poDatum->GetDictionary();

            GDALPDFObject *poDatumDescription = poDatumDict->Get("Description");
            const char *pszDatumDescription = "unknown";
            if (poDatumDescription != nullptr &&
                poDatumDescription->GetType() == PDFObjectType_String)
                pszDatumDescription = poDatumDescription->GetString().c_str();
            CPLDebug("PDF", "Datum.Description = %s", pszDatumDescription);

            GDALPDFObject *poEllipsoid = poDatumDict->Get("Ellipsoid");
            if (poEllipsoid == nullptr ||
                !(poEllipsoid->GetType() == PDFObjectType_String ||
                  poEllipsoid->GetType() == PDFObjectType_Dictionary))
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Cannot find Ellipsoid in Datum. Defaulting to WGS84...");
                oSRS.SetGeogCS("unknown", pszDatumDescription, "unknown",
                               6378137, 298.257223563);
            }
            else if (poEllipsoid->GetType() == PDFObjectType_String)
            {
                const char *pszEllipsoid = poEllipsoid->GetString().c_str();
                CPLDebug("PDF", "Datum.Ellipsoid = %s", pszEllipsoid);
                if (EQUAL(pszEllipsoid, "WE"))
                {
                    oSRS.SetGeogCS("unknown", pszDatumDescription, "WGS 84",
                                   6378137, 298.257223563);
                }
                else
                {
                    CPLError(CE_Warning, CPLE_AppDefined,
                             "Unhandled (yet) value for Ellipsoid : %s. "
                             "Defaulting to WGS84...",
                             pszEllipsoid);
                    oSRS.SetGeogCS("unknown", pszDatumDescription, pszEllipsoid,
                                   6378137, 298.257223563);
                }
            }
            else  // if (poEllipsoid->GetType() == PDFObjectType_Dictionary)
            {
                GDALPDFDictionary *poEllipsoidDict =
                    poEllipsoid->GetDictionary();

                GDALPDFObject *poEllipsoidDescription =
                    poEllipsoidDict->Get("Description");
                const char *pszEllipsoidDescription = "unknown";
                if (poEllipsoidDescription != nullptr &&
                    poEllipsoidDescription->GetType() == PDFObjectType_String)
                    pszEllipsoidDescription =
                        poEllipsoidDescription->GetString().c_str();
                CPLDebug("PDF", "Datum.Ellipsoid.Description = %s",
                         pszEllipsoidDescription);

                double dfSemiMajor = Get(poEllipsoidDict, "SemiMajorAxis");
                CPLDebug("PDF", "Datum.Ellipsoid.SemiMajorAxis = %.16g",
                         dfSemiMajor);
                double dfInvFlattening = -1.0;

                if (poEllipsoidDict->Get("InvFlattening"))
                {
                    dfInvFlattening = Get(poEllipsoidDict, "InvFlattening");
                    CPLDebug("PDF", "Datum.Ellipsoid.InvFlattening = %.16g",
                             dfInvFlattening);
                }
                else if (poEllipsoidDict->Get("SemiMinorAxis"))
                {
                    double dfSemiMinor = Get(poEllipsoidDict, "SemiMinorAxis");
                    CPLDebug("PDF", "Datum.Ellipsoid.SemiMinorAxis = %.16g",
                             dfSemiMinor);
                    dfInvFlattening =
                        OSRCalcInvFlattening(dfSemiMajor, dfSemiMinor);
                }

                if (dfSemiMajor != 0.0 && dfInvFlattening != -1.0)
                {
                    oSRS.SetGeogCS("unknown", pszDatumDescription,
                                   pszEllipsoidDescription, dfSemiMajor,
                                   dfInvFlattening);
                }
                else
                {
                    CPLError(
                        CE_Warning, CPLE_AppDefined,
                        "Invalid Ellipsoid object. Defaulting to WGS84...");
                    oSRS.SetGeogCS("unknown", pszDatumDescription,
                                   pszEllipsoidDescription, 6378137,
                                   298.257223563);
                }
            }

            GDALPDFObject *poTOWGS84 = poDatumDict->Get("ToWGS84");
            if (poTOWGS84 != nullptr &&
                poTOWGS84->GetType() == PDFObjectType_Dictionary)
            {
                GDALPDFDictionary *poTOWGS84Dict = poTOWGS84->GetDictionary();
                double dx = Get(poTOWGS84Dict, "dx");
                double dy = Get(poTOWGS84Dict, "dy");
                double dz = Get(poTOWGS84Dict, "dz");
                if (poTOWGS84Dict->Get("rx") && poTOWGS84Dict->Get("ry") &&
                    poTOWGS84Dict->Get("rz") && poTOWGS84Dict->Get("sf"))
                {
                    double rx = Get(poTOWGS84Dict, "rx");
                    double ry = Get(poTOWGS84Dict, "ry");
                    double rz = Get(poTOWGS84Dict, "rz");
                    double sf = Get(poTOWGS84Dict, "sf");
                    oSRS.SetTOWGS84(dx, dy, dz, rx, ry, rz, sf);
                }
                else
                {
                    oSRS.SetTOWGS84(dx, dy, dz);
                }
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Extract Hemisphere attribute                                    */
    /* -------------------------------------------------------------------- */
    CPLString osHemisphere;
    GDALPDFObject *poHemisphere = poProjDict->Get("Hemisphere");
    if (poHemisphere != nullptr &&
        poHemisphere->GetType() == PDFObjectType_String)
    {
        osHemisphere = poHemisphere->GetString();
    }

    /* -------------------------------------------------------------------- */
    /*      Extract ProjectionType attribute                                */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poProjectionType = poProjDict->Get("ProjectionType");
    if (poProjectionType == nullptr ||
        poProjectionType->GetType() != PDFObjectType_String)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find ProjectionType of Projection object");
        return FALSE;
    }
    CPLString osProjectionType(poProjectionType->GetString());
    CPLDebug("PDF", "Projection.ProjectionType = %s", osProjectionType.c_str());

    /* Unhandled: NONE, GEODETIC */

    if (EQUAL(osProjectionType, "GEOGRAPHIC"))
    {
        /* Nothing to do */
    }

    /* Unhandled: LOCAL CARTESIAN, MG (MGRS) */

    else if (EQUAL(osProjectionType, "UT")) /* UTM */
    {
        int nZone = (int)Get(poProjDict, "Zone");
        int bNorth = EQUAL(osHemisphere, "N");
        if (bIsWGS84)
            oSRS.importFromEPSG(((bNorth) ? 32600 : 32700) + nZone);
        else
            oSRS.SetUTM(nZone, bNorth);
    }

    else if (EQUAL(osProjectionType,
                   "UP")) /* Universal Polar Stereographic (UPS) */
    {
        int bNorth = EQUAL(osHemisphere, "N");
        if (bIsWGS84)
            oSRS.importFromEPSG((bNorth) ? 32661 : 32761);
        else
            oSRS.SetPS((bNorth) ? 90 : -90, 0, 0.994, 200000, 200000);
    }

    else if (EQUAL(osProjectionType, "SPCS")) /* State Plane */
    {
        int nZone = (int)Get(poProjDict, "Zone");
        oSRS.SetStatePlane(nZone, bIsNAD83);
    }

    else if (EQUAL(osProjectionType, "AC")) /* Albers Equal Area Conic */
    {
        double dfStdP1 = Get(poProjDict, "StandardParallelOne");
        double dfStdP2 = Get(poProjDict, "StandardParallelTwo");
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetACEA(dfStdP1, dfStdP2, dfCenterLat, dfCenterLong,
                     dfFalseEasting, dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "AL")) /* Azimuthal Equidistant */
    {
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetAE(dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "BF")) /* Bonne */
    {
        double dfStdP1 = Get(poProjDict, "OriginLatitude");
        double dfCentralMeridian = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetBonne(dfStdP1, dfCentralMeridian, dfFalseEasting,
                      dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "CS")) /* Cassini */
    {
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetCS(dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "LI")) /* Cylindrical Equal Area */
    {
        double dfStdP1 = Get(poProjDict, "OriginLatitude");
        double dfCentralMeridian = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetCEA(dfStdP1, dfCentralMeridian, dfFalseEasting,
                    dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "EF")) /* Eckert IV */
    {
        double dfCentralMeridian = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetEckertIV(dfCentralMeridian, dfFalseEasting, dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "ED")) /* Eckert VI */
    {
        double dfCentralMeridian = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetEckertVI(dfCentralMeridian, dfFalseEasting, dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "CP")) /* Equidistant Cylindrical */
    {
        double dfCenterLat = Get(poProjDict, "StandardParallel");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetEquirectangular(dfCenterLat, dfCenterLong, dfFalseEasting,
                                dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "GN")) /* Gnomonic */
    {
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetGnomonic(dfCenterLat, dfCenterLong, dfFalseEasting,
                         dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "LE")) /* Lambert Conformal Conic */
    {
        double dfStdP1 = Get(poProjDict, "StandardParallelOne");
        double dfStdP2 = Get(poProjDict, "StandardParallelTwo");
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetLCC(dfStdP1, dfStdP2, dfCenterLat, dfCenterLong, dfFalseEasting,
                    dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "MC")) /* Mercator */
    {
#ifdef not_supported
        if (poProjDict->Get("StandardParallelOne") == nullptr)
#endif
        {
            double dfCenterLat = Get(poProjDict, "OriginLatitude");
            double dfCenterLong = Get(poProjDict, "CentralMeridian");
            double dfScale = Get(poProjDict, "ScaleFactor");
            double dfFalseEasting = Get(poProjDict, "FalseEasting");
            double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
            oSRS.SetMercator(dfCenterLat, dfCenterLong, dfScale, dfFalseEasting,
                             dfFalseNorthing);
        }
#ifdef not_supported
        else
        {
            double dfStdP1 = Get(poProjDict, "StandardParallelOne");
            double dfCenterLat = poProjDict->Get("OriginLatitude")
                                     ? Get(poProjDict, "OriginLatitude")
                                     : 0;
            double dfCenterLong = Get(poProjDict, "CentralMeridian");
            double dfFalseEasting = Get(poProjDict, "FalseEasting");
            double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
            oSRS.SetMercator2SP(dfStdP1, dfCenterLat, dfCenterLong,
                                dfFalseEasting, dfFalseNorthing);
        }
#endif
    }

    else if (EQUAL(osProjectionType, "MH")) /* Miller Cylindrical */
    {
        double dfCenterLat = 0 /* ? */;
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetMC(dfCenterLat, dfCenterLong, dfFalseEasting, dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "MP")) /* Mollweide */
    {
        double dfCentralMeridian = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetMollweide(dfCentralMeridian, dfFalseEasting, dfFalseNorthing);
    }

    /* Unhandled:  "NY" : Ney's (Modified Lambert Conformal Conic) */

    else if (EQUAL(osProjectionType, "NT")) /* New Zealand Map Grid */
    {
        /* No parameter specified in the PDF, so let's take the ones of
         * EPSG:27200 */
        double dfCenterLat = -41;
        double dfCenterLong = 173;
        double dfFalseEasting = 2510000;
        double dfFalseNorthing = 6023150;
        oSRS.SetNZMG(dfCenterLat, dfCenterLong, dfFalseEasting,
                     dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "OC")) /* Oblique Mercator */
    {
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfLat1 = Get(poProjDict, "LatitudeOne");
        double dfLong1 = Get(poProjDict, "LongitudeOne");
        double dfLat2 = Get(poProjDict, "LatitudeTwo");
        double dfLong2 = Get(poProjDict, "LongitudeTwo");
        double dfScale = Get(poProjDict, "ScaleFactor");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetHOM2PNO(dfCenterLat, dfLat1, dfLong1, dfLat2, dfLong2, dfScale,
                        dfFalseEasting, dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "OD")) /* Orthographic */
    {
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetOrthographic(dfCenterLat, dfCenterLong, dfFalseEasting,
                             dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "PG")) /* Polar Stereographic */
    {
        double dfCenterLat = Get(poProjDict, "LatitudeTrueScale");
        double dfCenterLong = Get(poProjDict, "LongitudeDownFromPole");
        double dfScale = 1.0;
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetPS(dfCenterLat, dfCenterLong, dfScale, dfFalseEasting,
                   dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "PH")) /* Polyconic */
    {
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetPolyconic(dfCenterLat, dfCenterLong, dfFalseEasting,
                          dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "SA")) /* Sinusoidal */
    {
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetSinusoidal(dfCenterLong, dfFalseEasting, dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "SD")) /* Stereographic */
    {
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfScale = 1.0;
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetStereographic(dfCenterLat, dfCenterLong, dfScale,
                              dfFalseEasting, dfFalseNorthing);
    }

    else if (EQUAL(osProjectionType, "TC")) /* Transverse Mercator */
    {
        double dfCenterLat = Get(poProjDict, "OriginLatitude");
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfScale = Get(poProjDict, "ScaleFactor");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        if (dfCenterLat == 0.0 && dfScale == 0.9996 &&
            dfFalseEasting == 500000 &&
            (dfFalseNorthing == 0.0 || dfFalseNorthing == 10000000.0))
        {
            int nZone = (int)floor((dfCenterLong + 180.0) / 6.0) + 1;
            int bNorth = dfFalseNorthing == 0;
            if (bIsWGS84)
                oSRS.importFromEPSG(((bNorth) ? 32600 : 32700) + nZone);
            else if (bIsNAD83 && bNorth)
                oSRS.importFromEPSG(26900 + nZone);
            else
                oSRS.SetUTM(nZone, bNorth);
        }
        else
        {
            oSRS.SetTM(dfCenterLat, dfCenterLong, dfScale, dfFalseEasting,
                       dfFalseNorthing);
        }
    }

    /* Unhandled TX : Transverse Cylindrical Equal Area */

    else if (EQUAL(osProjectionType, "VA")) /* Van der Grinten */
    {
        double dfCenterLong = Get(poProjDict, "CentralMeridian");
        double dfFalseEasting = Get(poProjDict, "FalseEasting");
        double dfFalseNorthing = Get(poProjDict, "FalseNorthing");
        oSRS.SetVDG(dfCenterLong, dfFalseEasting, dfFalseNorthing);
    }

    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unhandled (yet) value for ProjectionType : %s",
                 osProjectionType.c_str());
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Extract Units attribute                                         */
    /* -------------------------------------------------------------------- */
    CPLString osUnits;
    GDALPDFObject *poUnits = poProjDict->Get("Units");
    if (poUnits != nullptr && poUnits->GetType() == PDFObjectType_String &&
        !EQUAL(osProjectionType, "GEOGRAPHIC"))
    {
        osUnits = poUnits->GetString();
        CPLDebug("PDF", "Projection.Units = %s", osUnits.c_str());

        // This is super weird. The false easting/northing of the SRS
        // are expressed in the unit, but the geotransform is expressed in
        // meters. Hence this hack to have an equivalent SRS definition, but
        // with linear units converted in meters.
        if (EQUAL(osUnits, "M"))
            oSRS.SetLinearUnits("Meter", 1.0);
        else if (EQUAL(osUnits, "FT"))
        {
            oSRS.SetLinearUnits("foot", 0.3048);
            oSRS.SetLinearUnitsAndUpdateParameters("Meter", 1.0);
        }
        else if (EQUAL(osUnits, "USSF"))
        {
            oSRS.SetLinearUnits(SRS_UL_US_FOOT, CPLAtof(SRS_UL_US_FOOT_CONV));
            oSRS.SetLinearUnitsAndUpdateParameters("Meter", 1.0);
        }
        else
            CPLError(CE_Warning, CPLE_AppDefined, "Unhandled unit: %s",
                     osUnits.c_str());
    }

    /* -------------------------------------------------------------------- */
    /*      Export SpatialRef                                               */
    /* -------------------------------------------------------------------- */
    m_oSRS = std::move(oSRS);

    return TRUE;
}

/************************************************************************/
/*                              ParseVP()                               */
/************************************************************************/

int PDFDataset::ParseVP(GDALPDFObject *poVP, double dfMediaBoxWidth,
                        double dfMediaBoxHeight)
{
    int i;

    if (poVP->GetType() != PDFObjectType_Array)
        return FALSE;

    GDALPDFArray *poVPArray = poVP->GetArray();

    int nLength = poVPArray->GetLength();
    CPLDebug("PDF", "VP length = %d", nLength);
    if (nLength < 1)
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Find the largest BBox                                           */
    /* -------------------------------------------------------------------- */
    const char *pszNeatlineToSelect =
        GetOption(papszOpenOptions, "NEATLINE", "Map Layers");

    int iLargest = 0;
    int iRequestedVP = -1;
    double dfLargestArea = 0;

    for (i = 0; i < nLength; i++)
    {
        GDALPDFObject *poVPElt = poVPArray->Get(i);
        if (poVPElt == nullptr ||
            poVPElt->GetType() != PDFObjectType_Dictionary)
        {
            return FALSE;
        }

        GDALPDFDictionary *poVPEltDict = poVPElt->GetDictionary();

        GDALPDFObject *poMeasure = poVPEltDict->Get("Measure");
        if (poMeasure == nullptr ||
            poMeasure->GetType() != PDFObjectType_Dictionary)
        {
            continue;
        }
        /* --------------------------------------------------------------------
         */
        /*      Extract Subtype attribute */
        /* --------------------------------------------------------------------
         */
        GDALPDFDictionary *poMeasureDict = poMeasure->GetDictionary();
        GDALPDFObject *poSubtype = poMeasureDict->Get("Subtype");
        if (poSubtype == nullptr || poSubtype->GetType() != PDFObjectType_Name)
        {
            continue;
        }

        CPLDebug("PDF", "Subtype = %s", poSubtype->GetName().c_str());
        if (!EQUAL(poSubtype->GetName().c_str(), "GEO"))
        {
            continue;
        }

        GDALPDFObject *poName = poVPEltDict->Get("Name");
        if (poName != nullptr && poName->GetType() == PDFObjectType_String)
        {
            CPLDebug("PDF", "Name = %s", poName->GetString().c_str());
            if (EQUAL(poName->GetString().c_str(), pszNeatlineToSelect))
            {
                iRequestedVP = i;
            }
        }

        GDALPDFObject *poBBox = poVPEltDict->Get("BBox");
        if (poBBox == nullptr || poBBox->GetType() != PDFObjectType_Array)
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot find Bbox object");
            return FALSE;
        }

        int nBboxLength = poBBox->GetArray()->GetLength();
        if (nBboxLength != 4)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Invalid length for Bbox object");
            return FALSE;
        }

        double adfBBox[4];
        adfBBox[0] = Get(poBBox, 0);
        adfBBox[1] = Get(poBBox, 1);
        adfBBox[2] = Get(poBBox, 2);
        adfBBox[3] = Get(poBBox, 3);
        double dfArea =
            fabs(adfBBox[2] - adfBBox[0]) * fabs(adfBBox[3] - adfBBox[1]);
        if (dfArea > dfLargestArea)
        {
            iLargest = i;
            dfLargestArea = dfArea;
        }
    }

    if (nLength > 1)
    {
        CPLDebug("PDF", "Largest BBox in VP array is element %d", iLargest);
    }

    GDALPDFObject *poVPElt = nullptr;

    if (iRequestedVP > -1)
    {
        CPLDebug("PDF", "Requested NEATLINE BBox in VP array is element %d",
                 iRequestedVP);
        poVPElt = poVPArray->Get(iRequestedVP);
    }
    else
    {
        poVPElt = poVPArray->Get(iLargest);
    }

    if (poVPElt == nullptr || poVPElt->GetType() != PDFObjectType_Dictionary)
    {
        return FALSE;
    }

    GDALPDFDictionary *poVPEltDict = poVPElt->GetDictionary();

    GDALPDFObject *poBBox = poVPEltDict->Get("BBox");
    if (poBBox == nullptr || poBBox->GetType() != PDFObjectType_Array)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find Bbox object");
        return FALSE;
    }

    int nBboxLength = poBBox->GetArray()->GetLength();
    if (nBboxLength != 4)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid length for Bbox object");
        return FALSE;
    }

    double dfULX = Get(poBBox, 0);
    double dfULY = dfMediaBoxHeight - Get(poBBox, 1);
    double dfLRX = Get(poBBox, 2);
    double dfLRY = dfMediaBoxHeight - Get(poBBox, 3);

    /* -------------------------------------------------------------------- */
    /*      Extract Measure attribute                                       */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poMeasure = poVPEltDict->Get("Measure");
    if (poMeasure == nullptr ||
        poMeasure->GetType() != PDFObjectType_Dictionary)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find Measure object");
        return FALSE;
    }

    int bRet = ParseMeasure(poMeasure, dfMediaBoxWidth, dfMediaBoxHeight, dfULX,
                            dfULY, dfLRX, dfLRY);

    /* -------------------------------------------------------------------- */
    /*      Extract PointData attribute                                     */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poPointData = poVPEltDict->Get("PtData");
    if (poPointData != nullptr &&
        poPointData->GetType() == PDFObjectType_Dictionary)
    {
        CPLDebug("PDF", "Found PointData");
    }

    return bRet;
}

/************************************************************************/
/*                           ParseMeasure()                             */
/************************************************************************/

int PDFDataset::ParseMeasure(GDALPDFObject *poMeasure, double dfMediaBoxWidth,
                             double dfMediaBoxHeight, double dfULX,
                             double dfULY, double dfLRX, double dfLRY)
{
    GDALPDFDictionary *poMeasureDict = poMeasure->GetDictionary();

    /* -------------------------------------------------------------------- */
    /*      Extract Subtype attribute                                       */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poSubtype = poMeasureDict->Get("Subtype");
    if (poSubtype == nullptr || poSubtype->GetType() != PDFObjectType_Name)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find Subtype object");
        return FALSE;
    }

    CPLDebug("PDF", "Subtype = %s", poSubtype->GetName().c_str());
    if (!EQUAL(poSubtype->GetName().c_str(), "GEO"))
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      Extract Bounds attribute (optional)                             */
    /* -------------------------------------------------------------------- */

    /* http://acrobatusers.com/sites/default/files/gallery_pictures/SEVERODVINSK.pdf
     */
    /* has lgit:LPTS, lgit:GPTS and lgit:Bounds that have more precision than */
    /* LPTS, GPTS and Bounds. Use those ones */

    GDALPDFObject *poBounds = poMeasureDict->Get("lgit:Bounds");
    if (poBounds != nullptr && poBounds->GetType() == PDFObjectType_Array)
    {
        CPLDebug("PDF", "Using lgit:Bounds");
    }
    else if ((poBounds = poMeasureDict->Get("Bounds")) == nullptr ||
             poBounds->GetType() != PDFObjectType_Array)
    {
        poBounds = nullptr;
    }

    if (poBounds != nullptr)
    {
        int nBoundsLength = poBounds->GetArray()->GetLength();
        if (nBoundsLength == 8)
        {
            double adfBounds[8];
            for (int i = 0; i < 8; i++)
            {
                adfBounds[i] = Get(poBounds, i);
                CPLDebug("PDF", "Bounds[%d] = %f", i, adfBounds[i]);
            }

            // TODO we should use it to restrict the neatline but
            // I have yet to set a sample where bounds are not the four
            // corners of the unit square.
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Extract GPTS attribute                                          */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poGPTS = poMeasureDict->Get("lgit:GPTS");
    if (poGPTS != nullptr && poGPTS->GetType() == PDFObjectType_Array)
    {
        CPLDebug("PDF", "Using lgit:GPTS");
    }
    else if ((poGPTS = poMeasureDict->Get("GPTS")) == nullptr ||
             poGPTS->GetType() != PDFObjectType_Array)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find GPTS object");
        return FALSE;
    }

    int nGPTSLength = poGPTS->GetArray()->GetLength();
    if ((nGPTSLength % 2) != 0 || nGPTSLength < 6)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid length for GPTS object");
        return FALSE;
    }

    std::vector<double> adfGPTS(nGPTSLength);
    for (int i = 0; i < nGPTSLength; i++)
    {
        adfGPTS[i] = Get(poGPTS, i);
        CPLDebug("PDF", "GPTS[%d] = %.18f", i, adfGPTS[i]);
    }

    /* -------------------------------------------------------------------- */
    /*      Extract LPTS attribute                                          */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poLPTS = poMeasureDict->Get("lgit:LPTS");
    if (poLPTS != nullptr && poLPTS->GetType() == PDFObjectType_Array)
    {
        CPLDebug("PDF", "Using lgit:LPTS");
    }
    else if ((poLPTS = poMeasureDict->Get("LPTS")) == nullptr ||
             poLPTS->GetType() != PDFObjectType_Array)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find LPTS object");
        return FALSE;
    }

    int nLPTSLength = poLPTS->GetArray()->GetLength();
    if (nLPTSLength != nGPTSLength)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid length for LPTS object");
        return FALSE;
    }

    std::vector<double> adfLPTS(nLPTSLength);
    for (int i = 0; i < nLPTSLength; i++)
    {
        adfLPTS[i] = Get(poLPTS, i);
        CPLDebug("PDF", "LPTS[%d] = %f", i, adfLPTS[i]);
    }

    /* -------------------------------------------------------------------- */
    /*      Extract GCS attribute                                           */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poGCS = poMeasureDict->Get("GCS");
    if (poGCS == nullptr || poGCS->GetType() != PDFObjectType_Dictionary)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find GCS object");
        return FALSE;
    }

    GDALPDFDictionary *poGCSDict = poGCS->GetDictionary();

    /* -------------------------------------------------------------------- */
    /*      Extract GCS.Type attribute                                      */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poGCSType = poGCSDict->Get("Type");
    if (poGCSType == nullptr || poGCSType->GetType() != PDFObjectType_Name)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot find GCS.Type object");
        return FALSE;
    }

    CPLDebug("PDF", "GCS.Type = %s", poGCSType->GetName().c_str());

    /* -------------------------------------------------------------------- */
    /*      Extract EPSG attribute                                          */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poEPSG = poGCSDict->Get("EPSG");
    int nEPSGCode = 0;
    if (poEPSG != nullptr && poEPSG->GetType() == PDFObjectType_Int)
    {
        nEPSGCode = poEPSG->GetInt();
        CPLDebug("PDF", "GCS.EPSG = %d", nEPSGCode);
    }

    /* -------------------------------------------------------------------- */
    /*      Extract GCS.WKT attribute                                       */
    /* -------------------------------------------------------------------- */
    GDALPDFObject *poGCSWKT = poGCSDict->Get("WKT");
    if (poGCSWKT != nullptr && poGCSWKT->GetType() != PDFObjectType_String)
    {
        poGCSWKT = nullptr;
    }

    if (poGCSWKT != nullptr)
        CPLDebug("PDF", "GCS.WKT = %s", poGCSWKT->GetString().c_str());

    if (nEPSGCode <= 0 && poGCSWKT == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find GCS.WKT or GCS.EPSG objects");
        return FALSE;
    }

    if (poGCSWKT != nullptr)
    {
        m_oSRS.importFromWkt(poGCSWKT->GetString().c_str());
    }

    bool bSRSOK = false;
    if (nEPSGCode != 0)
    {
        // At time of writing EPSG CRS codes are <= 32767.
        // The usual practice is that codes >= 100000 are in the ESRI namespace
        // instead
        if (nEPSGCode >= 100000)
        {
            CPLErrorHandlerPusher oHandler(CPLQuietErrorHandler);
            OGRSpatialReference oSRS_ESRI;
            oSRS_ESRI.SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (oSRS_ESRI.SetFromUserInput(CPLSPrintf("ESRI:%d", nEPSGCode)) ==
                OGRERR_NONE)
            {
                bSRSOK = true;

                // Check consistency of ESRI:xxxx and WKT definitions
                if (poGCSWKT != nullptr)
                {
                    if (!EQUAL(oSRS_ESRI.GetName(), m_oSRS.GetName()) &&
                        !oSRS_ESRI.IsSame(&m_oSRS))
                    {
                        CPLDebug("PDF",
                                 "Definition from ESRI:%d and WKT=%s do not "
                                 "match. Using WKT string",
                                 nEPSGCode, poGCSWKT->GetString().c_str());
                        bSRSOK = false;
                    }
                }
                if (bSRSOK)
                {
                    m_oSRS = std::move(oSRS_ESRI);
                }
            }
        }
        else if (m_oSRS.importFromEPSG(nEPSGCode) == OGRERR_NONE)
        {
            bSRSOK = true;
        }
    }

    if (!bSRSOK)
    {
        if (poGCSWKT == nullptr)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Cannot resolve EPSG object, and GCS.WKT not found");
            return FALSE;
        }

        if (m_oSRS.importFromWkt(poGCSWKT->GetString().c_str()) != OGRERR_NONE)
        {
            m_oSRS.Clear();
            return FALSE;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Compute geotransform                                            */
    /* -------------------------------------------------------------------- */
    OGRSpatialReference *poSRSGeog = m_oSRS.CloneGeogCS();

    /* Files found at
     * http://carto.iict.ch/blog/publications-cartographiques-au-format-geospatial-pdf/
     */
    /* are in a PROJCS. However the coordinates in GPTS array are not in (lat,
     * long) as required by the */
    /* ISO 32000 supplement spec, but in (northing, easting). Adobe reader is
     * able to understand that, */
    /* so let's also try to do it with a heuristics. */

    bool bReproject = true;
    if (m_oSRS.IsProjected())
    {
        for (int i = 0; i < nGPTSLength / 2; i++)
        {
            if (fabs(adfGPTS[2 * i]) > 91 || fabs(adfGPTS[2 * i + 1]) > 361)
            {
                CPLDebug("PDF", "GPTS coordinates seems to be in (northing, "
                                "easting), which is non-standard");
                bReproject = false;
                break;
            }
        }
    }

    OGRCoordinateTransformation *poCT = nullptr;
    if (bReproject)
    {
        poCT = OGRCreateCoordinateTransformation(poSRSGeog, &m_oSRS);
        if (poCT == nullptr)
        {
            delete poSRSGeog;
            m_oSRS.Clear();
            return FALSE;
        }
    }

    std::vector<GDAL_GCP> asGCPS(nGPTSLength / 2);

    /* Create NEATLINE */
    OGRLinearRing *poRing = nullptr;
    if (nGPTSLength == 8)
    {
        m_poNeatLine = new OGRPolygon();
        poRing = new OGRLinearRing();
        m_poNeatLine->addRingDirectly(poRing);
    }

    for (int i = 0; i < nGPTSLength / 2; i++)
    {
        /* We probably assume LPTS is 0 or 1 */
        asGCPS[i].dfGCPPixel =
            (dfULX * (1 - adfLPTS[2 * i + 0]) + dfLRX * adfLPTS[2 * i + 0]) /
            dfMediaBoxWidth * nRasterXSize;
        asGCPS[i].dfGCPLine =
            (dfULY * (1 - adfLPTS[2 * i + 1]) + dfLRY * adfLPTS[2 * i + 1]) /
            dfMediaBoxHeight * nRasterYSize;

        double lat = adfGPTS[2 * i];
        double lon = adfGPTS[2 * i + 1];
        double x = lon;
        double y = lat;
        if (bReproject)
        {
            if (!poCT->Transform(1, &x, &y, nullptr))
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot reproject (%f, %f)", lon, lat);
                delete poSRSGeog;
                delete poCT;
                m_oSRS.Clear();
                return FALSE;
            }
        }

        x = ROUND_TO_INT_IF_CLOSE(x);
        y = ROUND_TO_INT_IF_CLOSE(y);

        asGCPS[i].dfGCPX = x;
        asGCPS[i].dfGCPY = y;

        if (poRing)
            poRing->addPoint(x, y);
    }

    delete poSRSGeog;
    delete poCT;

    if (!GDALGCPsToGeoTransform(nGPTSLength / 2, asGCPS.data(),
                                m_adfGeoTransform.data(), FALSE))
    {
        CPLDebug("PDF",
                 "Could not compute GT with exact match. Try with approximate");
        if (!GDALGCPsToGeoTransform(nGPTSLength / 2, asGCPS.data(),
                                    m_adfGeoTransform.data(), TRUE))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Could not compute GT with approximate match.");
            return FALSE;
        }
    }
    m_bGeoTransformValid = true;

    // If the non scaling terms of the geotransform are significantly smaller
    // than the pixel size, then nullify them as being just artifacts of
    //  reprojection and GDALGCPsToGeoTransform() numerical imprecisions.
    const double dfPixelSize =
        std::min(fabs(m_adfGeoTransform[1]), fabs(m_adfGeoTransform[5]));
    const double dfRotationShearTerm =
        std::max(fabs(m_adfGeoTransform[2]), fabs(m_adfGeoTransform[4]));
    if (dfRotationShearTerm < 1e-5 * dfPixelSize ||
        (m_bUseLib.test(PDFLIB_PDFIUM) &&
         std::min(fabs(m_adfGeoTransform[2]), fabs(m_adfGeoTransform[4])) <
             1e-5 * dfPixelSize))
    {
        dfLRX = m_adfGeoTransform[0] + nRasterXSize * m_adfGeoTransform[1] +
                nRasterYSize * m_adfGeoTransform[2];
        dfLRY = m_adfGeoTransform[3] + nRasterXSize * m_adfGeoTransform[4] +
                nRasterYSize * m_adfGeoTransform[5];
        m_adfGeoTransform[1] = (dfLRX - m_adfGeoTransform[0]) / nRasterXSize;
        m_adfGeoTransform[5] = (dfLRY - m_adfGeoTransform[3]) / nRasterYSize;
        m_adfGeoTransform[2] = m_adfGeoTransform[4] = 0;
    }

    return TRUE;
}

/************************************************************************/
/*                          GetSpatialRef()                            */
/************************************************************************/

const OGRSpatialReference *PDFDataset::GetSpatialRef() const
{
    const auto poSRS = GDALPamDataset::GetSpatialRef();
    if (poSRS)
        return poSRS;

    if (!m_oSRS.IsEmpty() && m_bGeoTransformValid)
        return &m_oSRS;
    return nullptr;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr PDFDataset::GetGeoTransform(double *padfTransform)

{
    if (GDALPamDataset::GetGeoTransform(padfTransform) == CE_None)
    {
        return CE_None;
    }

    std::copy(std::begin(m_adfGeoTransform), std::end(m_adfGeoTransform),
              padfTransform);

    return ((m_bGeoTransformValid) ? CE_None : CE_Failure);
}

/************************************************************************/
/*                            SetSpatialRef()                           */
/************************************************************************/

CPLErr PDFDataset::SetSpatialRef(const OGRSpatialReference *poSRS)
{
    if (eAccess == GA_ReadOnly)
        GDALPamDataset::SetSpatialRef(poSRS);

    m_oSRS.Clear();
    if (poSRS)
        m_oSRS = *poSRS;
    m_bProjDirty = true;
    return CE_None;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr PDFDataset::SetGeoTransform(double *padfGeoTransform)
{
    if (eAccess == GA_ReadOnly)
        GDALPamDataset::SetGeoTransform(padfGeoTransform);

    std::copy(padfGeoTransform, padfGeoTransform + 6,
              std::begin(m_adfGeoTransform));
    m_bGeoTransformValid = true;
    m_bProjDirty = true;

    /* Reset NEATLINE if not explicitly set by the user */
    if (!m_bNeatLineDirty)
        SetMetadataItem("NEATLINE", nullptr);
    return CE_None;
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **PDFDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE, "xml:XMP", "LAYERS",
                                   "EMBEDDED_METADATA", nullptr);
}

/************************************************************************/
/*                           GetMetadata()                              */
/************************************************************************/

char **PDFDataset::GetMetadata(const char *pszDomain)
{
    if (pszDomain != nullptr && EQUAL(pszDomain, "EMBEDDED_METADATA"))
    {
        char **papszRet = m_oMDMD_PDF.GetMetadata(pszDomain);
        if (papszRet)
            return papszRet;

        GDALPDFObject *poCatalog = GetCatalog();
        if (poCatalog == nullptr)
            return nullptr;
        GDALPDFObject *poFirstElt =
            poCatalog->LookupObject("Names.EmbeddedFiles.Names[0]");
        GDALPDFObject *poF =
            poCatalog->LookupObject("Names.EmbeddedFiles.Names[1].EF.F");

        if (poFirstElt == nullptr ||
            poFirstElt->GetType() != PDFObjectType_String ||
            poFirstElt->GetString() != "Metadata")
            return nullptr;
        if (poF == nullptr || poF->GetType() != PDFObjectType_Dictionary)
            return nullptr;
        GDALPDFStream *poStream = poF->GetStream();
        if (poStream == nullptr)
            return nullptr;

        char *apszMetadata[2] = {nullptr, nullptr};
        apszMetadata[0] = poStream->GetBytes();
        m_oMDMD_PDF.SetMetadata(apszMetadata, pszDomain);
        VSIFree(apszMetadata[0]);
        return m_oMDMD_PDF.GetMetadata(pszDomain);
    }
    if (pszDomain == nullptr || EQUAL(pszDomain, ""))
    {
        char **papszPAMMD = GDALPamDataset::GetMetadata(pszDomain);
        for (char **papszIter = papszPAMMD; papszIter && *papszIter;
             ++papszIter)
        {
            char *pszKey = nullptr;
            const char *pszValue = CPLParseNameValue(*papszIter, &pszKey);
            if (pszKey && pszValue)
            {
                if (m_oMDMD_PDF.GetMetadataItem(pszKey, pszDomain) == nullptr)
                    m_oMDMD_PDF.SetMetadataItem(pszKey, pszValue, pszDomain);
            }
            CPLFree(pszKey);
        }
        return m_oMDMD_PDF.GetMetadata(pszDomain);
    }
    if (EQUAL(pszDomain, "LAYERS") || EQUAL(pszDomain, "xml:XMP") ||
        EQUAL(pszDomain, "SUBDATASETS"))
    {
        return m_oMDMD_PDF.GetMetadata(pszDomain);
    }
    return GDALPamDataset::GetMetadata(pszDomain);
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr PDFDataset::SetMetadata(char **papszMetadata, const char *pszDomain)
{
    if (pszDomain == nullptr || EQUAL(pszDomain, ""))
    {
        char **papszMetadataDup = CSLDuplicate(papszMetadata);
        m_oMDMD_PDF.SetMetadata(nullptr, pszDomain);

        for (char **papszIter = papszMetadataDup; papszIter && *papszIter;
             ++papszIter)
        {
            char *pszKey = nullptr;
            const char *pszValue = CPLParseNameValue(*papszIter, &pszKey);
            if (pszKey && pszValue)
            {
                SetMetadataItem(pszKey, pszValue, pszDomain);
            }
            CPLFree(pszKey);
        }
        CSLDestroy(papszMetadataDup);
        return CE_None;
    }
    else if (EQUAL(pszDomain, "xml:XMP"))
    {
        m_bXMPDirty = true;
        return m_oMDMD_PDF.SetMetadata(papszMetadata, pszDomain);
    }
    else if (EQUAL(pszDomain, "SUBDATASETS"))
    {
        return m_oMDMD_PDF.SetMetadata(papszMetadata, pszDomain);
    }
    else
    {
        return GDALPamDataset::SetMetadata(papszMetadata, pszDomain);
    }
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *PDFDataset::GetMetadataItem(const char *pszName,
                                        const char *pszDomain)
{
    if (pszDomain != nullptr && EQUAL(pszDomain, "_INTERNAL_") &&
        pszName != nullptr && EQUAL(pszName, "PDF_LIB"))
    {
        if (m_bUseLib.test(PDFLIB_POPPLER))
            return "POPPLER";
        if (m_bUseLib.test(PDFLIB_PODOFO))
            return "PODOFO";
        if (m_bUseLib.test(PDFLIB_PDFIUM))
            return "PDFIUM";
    }
    return CSLFetchNameValue(GetMetadata(pszDomain), pszName);
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr PDFDataset::SetMetadataItem(const char *pszName, const char *pszValue,
                                   const char *pszDomain)
{
    if (pszDomain == nullptr || EQUAL(pszDomain, ""))
    {
        if (EQUAL(pszName, "NEATLINE"))
        {
            const char *pszOldValue =
                m_oMDMD_PDF.GetMetadataItem(pszName, pszDomain);
            if ((pszValue == nullptr && pszOldValue != nullptr) ||
                (pszValue != nullptr && pszOldValue == nullptr) ||
                (pszValue != nullptr && pszOldValue != nullptr &&
                 strcmp(pszValue, pszOldValue) != 0))
            {
                m_bProjDirty = true;
                m_bNeatLineDirty = true;
            }
            return m_oMDMD_PDF.SetMetadataItem(pszName, pszValue, pszDomain);
        }
        else
        {
            if (EQUAL(pszName, "AUTHOR") || EQUAL(pszName, "PRODUCER") ||
                EQUAL(pszName, "CREATOR") || EQUAL(pszName, "CREATION_DATE") ||
                EQUAL(pszName, "SUBJECT") || EQUAL(pszName, "TITLE") ||
                EQUAL(pszName, "KEYWORDS"))
            {
                if (pszValue == nullptr)
                    pszValue = "";
                const char *pszOldValue =
                    m_oMDMD_PDF.GetMetadataItem(pszName, pszDomain);
                if (pszOldValue == nullptr ||
                    strcmp(pszValue, pszOldValue) != 0)
                {
                    m_bInfoDirty = true;
                }
                return m_oMDMD_PDF.SetMetadataItem(pszName, pszValue,
                                                   pszDomain);
            }
            else if (EQUAL(pszName, "DPI"))
            {
                return m_oMDMD_PDF.SetMetadataItem(pszName, pszValue,
                                                   pszDomain);
            }
            else
            {
                m_oMDMD_PDF.SetMetadataItem(pszName, pszValue, pszDomain);
                return GDALPamDataset::SetMetadataItem(pszName, pszValue,
                                                       pszDomain);
            }
        }
    }
    else if (EQUAL(pszDomain, "xml:XMP"))
    {
        m_bXMPDirty = true;
        return m_oMDMD_PDF.SetMetadataItem(pszName, pszValue, pszDomain);
    }
    else if (EQUAL(pszDomain, "SUBDATASETS"))
    {
        return m_oMDMD_PDF.SetMetadataItem(pszName, pszValue, pszDomain);
    }
    else
    {
        return GDALPamDataset::SetMetadataItem(pszName, pszValue, pszDomain);
    }
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int PDFDataset::GetGCPCount()
{
    return m_nGCPCount;
}

/************************************************************************/
/*                          GetGCPSpatialRef()                          */
/************************************************************************/

const OGRSpatialReference *PDFDataset::GetGCPSpatialRef() const
{
    if (!m_oSRS.IsEmpty() && m_nGCPCount != 0)
        return &m_oSRS;
    return nullptr;
}

/************************************************************************/
/*                              GetGCPs()                               */
/************************************************************************/

const GDAL_GCP *PDFDataset::GetGCPs()
{
    return m_pasGCPList;
}

/************************************************************************/
/*                               SetGCPs()                              */
/************************************************************************/

CPLErr PDFDataset::SetGCPs(int nGCPCountIn, const GDAL_GCP *pasGCPListIn,
                           const OGRSpatialReference *poSRS)
{
    const char *pszGEO_ENCODING =
        CPLGetConfigOption("GDAL_PDF_GEO_ENCODING", "ISO32000");
    if (nGCPCountIn != 4 && EQUAL(pszGEO_ENCODING, "ISO32000"))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "PDF driver only supports writing 4 GCPs when "
                 "GDAL_PDF_GEO_ENCODING=ISO32000.");
        return CE_Failure;
    }

    /* Free previous GCPs */
    GDALDeinitGCPs(m_nGCPCount, m_pasGCPList);
    CPLFree(m_pasGCPList);

    /* Duplicate in GCPs */
    m_nGCPCount = nGCPCountIn;
    m_pasGCPList = GDALDuplicateGCPs(m_nGCPCount, pasGCPListIn);

    m_oSRS.Clear();
    if (poSRS)
        m_oSRS = *poSRS;

    m_bProjDirty = true;

    /* Reset NEATLINE if not explicitly set by the user */
    if (!m_bNeatLineDirty)
        SetMetadataItem("NEATLINE", nullptr);

    return CE_None;
}

#endif  // #ifdef HAVE_PDF_READ_SUPPORT

/************************************************************************/
/*                          GDALPDFOpen()                               */
/************************************************************************/

GDALDataset *GDALPDFOpen(
#ifdef HAVE_PDF_READ_SUPPORT
    const char *pszFilename, GDALAccess eAccess
#else
    CPL_UNUSED const char *pszFilename, CPL_UNUSED GDALAccess eAccess
#endif
)
{
#ifdef HAVE_PDF_READ_SUPPORT
    GDALOpenInfo oOpenInfo(pszFilename, eAccess);
    return PDFDataset::Open(&oOpenInfo);
#else
    return nullptr;
#endif
}

/************************************************************************/
/*                       GDALPDFUnloadDriver()                          */
/************************************************************************/

static void GDALPDFUnloadDriver(CPL_UNUSED GDALDriver *poDriver)
{
#ifdef HAVE_POPPLER
    if (hGlobalParamsMutex != nullptr)
        CPLDestroyMutex(hGlobalParamsMutex);
#endif
#ifdef HAVE_PDFIUM
    if (PDFDataset::g_bPdfiumInit)
    {
        CPLCreateOrAcquireMutex(&g_oPdfiumLoadDocMutex, PDFIUM_MUTEX_TIMEOUT);
        // Destroy every loaded document or page
        TMapPdfiumDatasets::iterator itDoc;
        TMapPdfiumPages::iterator itPage;
        for (itDoc = g_mPdfiumDatasets.begin();
             itDoc != g_mPdfiumDatasets.end(); ++itDoc)
        {
            TPdfiumDocumentStruct *pDoc = itDoc->second;
            for (itPage = pDoc->pages.begin(); itPage != pDoc->pages.end();
                 ++itPage)
            {
                TPdfiumPageStruct *pPage = itPage->second;

                CPLCreateOrAcquireMutex(&g_oPdfiumReadMutex,
                                        PDFIUM_MUTEX_TIMEOUT);
                CPLCreateOrAcquireMutex(&(pPage->readMutex),
                                        PDFIUM_MUTEX_TIMEOUT);
                CPLReleaseMutex(pPage->readMutex);
                CPLDestroyMutex(pPage->readMutex);
                FPDF_ClosePage(FPDFPageFromIPDFPage(pPage->page));
                delete pPage;
                CPLReleaseMutex(g_oPdfiumReadMutex);
            }  // ~ foreach page

            FPDF_CloseDocument(FPDFDocumentFromCPDFDocument(pDoc->doc));
            CPLFree(pDoc->filename);
            VSIFCloseL((VSILFILE *)pDoc->psFileAccess->m_Param);
            delete pDoc->psFileAccess;
            pDoc->pages.clear();

            delete pDoc;
        }  // ~ foreach document
        g_mPdfiumDatasets.clear();
        FPDF_DestroyLibrary();
        PDFDataset::g_bPdfiumInit = FALSE;

        CPLReleaseMutex(g_oPdfiumLoadDocMutex);

        if (g_oPdfiumReadMutex)
            CPLDestroyMutex(g_oPdfiumReadMutex);
        CPLDestroyMutex(g_oPdfiumLoadDocMutex);
    }
#endif
}

/************************************************************************/
/*                           PDFSanitizeLayerName()                     */
/************************************************************************/

CPLString PDFSanitizeLayerName(const char *pszName)
{
    if (!CPLTestBool(CPLGetConfigOption("GDAL_PDF_LAUNDER_LAYER_NAMES", "YES")))
        return pszName;

    CPLString osName;
    for (int i = 0; pszName[i] != '\0'; i++)
    {
        if (pszName[i] == ' ' || pszName[i] == '.' || pszName[i] == ',')
            osName += "_";
        else if (pszName[i] != '"')
            osName += pszName[i];
    }
    return osName;
}

/************************************************************************/
/*                         GDALRegister_PDF()                           */
/************************************************************************/

void GDALRegister_PDF()

{
    if (!GDAL_CHECK_VERSION("PDF driver"))
        return;

    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
        return;

    GDALDriver *poDriver = new GDALDriver();
    PDFDriverSetCommonMetadata(poDriver);

#ifdef HAVE_PDF_READ_SUPPORT
    poDriver->pfnOpen = PDFDataset::OpenWrapper;
#endif  // HAVE_PDF_READ_SUPPORT

    poDriver->pfnCreateCopy = GDALPDFCreateCopy;
    poDriver->pfnCreate = PDFWritableVectorDataset::Create;
    poDriver->pfnUnloadDriver = GDALPDFUnloadDriver;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
