/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  GDALJP2Stucture - Dump structure of a JP2/J2K file
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 ******************************************************************************
 * Copyright (c) 2015, European Union (European Environment Agency)
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

#include "cpl_port.h"
#include "gdaljp2metadata.h"

#include <cmath>
#include <cstring>
#if HAVE_FCNTL_H
#include <fcntl.h>
#endif

#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "cpl_vsi.h"
#include "gdal.h"
#include "gdal_priv.h"

constexpr int knbMaxJPEG2000Components = 16384;  // per the JPEG2000 standard

namespace
{
struct DumpContext
{
    int nCurLineCount = 0;
    int nMaxLineCount = 0;
    const char *pszCodestreamMarkers = nullptr;
    bool bDumpAll = false;
    bool bDumpCodestream = false;
    bool bDumpBinaryContent = false;
    bool bDumpTextContent = false;
    bool bDumpJP2Boxes = false;
    bool bStopAtSOD = false;
    bool bSODEncountered = false;
    bool bAllowGetFileSize = true;
};
}  // namespace

static CPLXMLNode *GetLastChild(CPLXMLNode *psParent)
{
    CPLXMLNode *psChild = psParent->psChild;
    while (psChild && psChild->psNext)
        psChild = psChild->psNext;
    return psChild;
}

static CPLXMLNode *_AddError(CPLXMLNode *psParent, const char *pszErrorMsg,
                             GIntBig nOffset = 0)
{
    CPLXMLNode *psError = CPLCreateXMLNode(psParent, CXT_Element, "Error");
    CPLAddXMLAttributeAndValue(psError, "message", pszErrorMsg);
    if (nOffset)
    {
        CPLAddXMLAttributeAndValue(psError, "offset",
                                   CPLSPrintf(CPL_FRMT_GIB, nOffset));
    }
    return psError;
}

static CPLXMLNode *AddElement(CPLXMLNode *psParent, CPLXMLNode *&psLastChild,
                              DumpContext *psDumpContext, CPLXMLNode *psNewElt)
{
    if (psDumpContext->nCurLineCount > psDumpContext->nMaxLineCount)
    {
        CPLDestroyXMLNode(psNewElt);

        if (psDumpContext->nCurLineCount == psDumpContext->nMaxLineCount + 1)
        {
            _AddError(psParent, "Too many lines in dump");
            psDumpContext->nCurLineCount++;
        }
        return nullptr;
    }
    psDumpContext->nCurLineCount++;

    if (psLastChild == nullptr)
        psLastChild = GetLastChild(psParent);
    if (psLastChild == nullptr)
        psParent->psChild = psNewElt;
    else
        psLastChild->psNext = psNewElt;
    psLastChild = psNewElt;
    return psNewElt;
}

static void AddField(CPLXMLNode *psParent, CPLXMLNode *&psLastChild,
                     DumpContext *psDumpContext, const char *pszFieldName,
                     int nFieldSize, const char *pszValue,
                     const char *pszDescription = nullptr)
{
    if (psDumpContext->nCurLineCount > psDumpContext->nMaxLineCount + 1)
    {
        return;
    }

    CPLXMLNode *psField =
        CPLCreateXMLElementAndValue(nullptr, "Field", pszValue);
    CPLAddXMLAttributeAndValue(psField, "name", pszFieldName);
    CPLAddXMLAttributeAndValue(psField, "type", "string");
    CPLAddXMLAttributeAndValue(psField, "size", CPLSPrintf("%d", nFieldSize));
    if (pszDescription)
        CPLAddXMLAttributeAndValue(psField, "description", pszDescription);
    AddElement(psParent, psLastChild, psDumpContext, psField);
}

static void AddHexField(CPLXMLNode *psParent, CPLXMLNode *&psLastChild,
                        DumpContext *psDumpContext, const char *pszFieldName,
                        int nFieldSize, const char *pszValue,
                        const char *pszDescription = nullptr)
{
    if (psDumpContext->nCurLineCount > psDumpContext->nMaxLineCount + 1)
    {
        return;
    }

    CPLXMLNode *psField =
        CPLCreateXMLElementAndValue(nullptr, "Field", pszValue);
    CPLAddXMLAttributeAndValue(psField, "name", pszFieldName);
    CPLAddXMLAttributeAndValue(psField, "type", "hexint");
    CPLAddXMLAttributeAndValue(psField, "size", CPLSPrintf("%d", nFieldSize));
    if (pszDescription)
        CPLAddXMLAttributeAndValue(psField, "description", pszDescription);
    AddElement(psParent, psLastChild, psDumpContext, psField);
}

static void AddField(CPLXMLNode *psParent, CPLXMLNode *&psLastChild,
                     DumpContext *psDumpContext, const char *pszFieldName,
                     GByte nVal, const char *pszDescription = nullptr)
{
    if (psDumpContext->nCurLineCount > psDumpContext->nMaxLineCount + 1)
    {
        return;
    }

    CPLXMLNode *psField =
        CPLCreateXMLElementAndValue(nullptr, "Field", CPLSPrintf("%d", nVal));
    CPLAddXMLAttributeAndValue(psField, "name", pszFieldName);
    CPLAddXMLAttributeAndValue(psField, "type", "uint8");
    if (pszDescription)
        CPLAddXMLAttributeAndValue(psField, "description", pszDescription);
    AddElement(psParent, psLastChild, psDumpContext, psField);
}

static void AddField(CPLXMLNode *psParent, CPLXMLNode *&psLastChild,
                     DumpContext *psDumpContext, const char *pszFieldName,
                     GUInt16 nVal, const char *pszDescription = nullptr)
{
    if (psDumpContext->nCurLineCount > psDumpContext->nMaxLineCount + 1)
    {
        return;
    }

    CPLXMLNode *psField =
        CPLCreateXMLElementAndValue(nullptr, "Field", CPLSPrintf("%d", nVal));
    CPLAddXMLAttributeAndValue(psField, "name", pszFieldName);
    CPLAddXMLAttributeAndValue(psField, "type", "uint16");
    if (pszDescription)
        CPLAddXMLAttributeAndValue(psField, "description", pszDescription);
    AddElement(psParent, psLastChild, psDumpContext, psField);
}

static void AddField(CPLXMLNode *psParent, CPLXMLNode *&psLastChild,
                     DumpContext *psDumpContext, const char *pszFieldName,
                     GUInt32 nVal, const char *pszDescription = nullptr)
{
    if (psDumpContext->nCurLineCount - 1 >= psDumpContext->nMaxLineCount)
    {
        return;
    }

    CPLXMLNode *psField =
        CPLCreateXMLElementAndValue(nullptr, "Field", CPLSPrintf("%u", nVal));
    CPLAddXMLAttributeAndValue(psField, "name", pszFieldName);
    CPLAddXMLAttributeAndValue(psField, "type", "uint32");
    if (pszDescription)
        CPLAddXMLAttributeAndValue(psField, "description", pszDescription);
    AddElement(psParent, psLastChild, psDumpContext, psField);
}

static const char *GetInterpretationOfBPC(GByte bpc)
{
    if (bpc == 255)
        return nullptr;
    if ((bpc & 0x80))
        return CPLSPrintf("Signed %d bits", 1 + (bpc & 0x7F));
    else
        return CPLSPrintf("Unsigned %d bits", 1 + bpc);
}

static const char *GetStandardFieldString(GUInt16 nVal)
{
    switch (nVal)
    {
        case 1:
            return "Codestream contains no extensions";
        case 2:
            return "Contains multiple composition layers";
        case 3:
            return "Codestream is compressed using JPEG 2000 and requires at "
                   "least a Profile 0 decoder";
        case 4:
            return "Codestream is compressed using JPEG 2000 and requires at "
                   "least a Profile 1 decoder";
        case 5:
            return "Codestream is compressed using JPEG 2000 unrestricted";
        case 35:
            return "Contains IPR metadata";
        case 67:
            return "Contains GMLJP2 metadata";
        default:
            return nullptr;
    }
}

static void DumpGeoTIFFBox(CPLXMLNode *psBox, GDALJP2Box &oBox,
                           DumpContext *psDumpContext)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte *pabyBoxData = oBox.ReadBoxData();
    GDALDriver *poVRTDriver =
        static_cast<GDALDriver *>(GDALGetDriverByName("VRT"));
    if (pabyBoxData && poVRTDriver)
    {
        CPLString osTmpFilename(
            CPLSPrintf("/vsimem/tmp_%p.tif", oBox.GetFILE()));
        CPL_IGNORE_RET_VAL(VSIFCloseL(VSIFileFromMemBuffer(
            osTmpFilename, pabyBoxData, nBoxDataLength, FALSE)));
        CPLPushErrorHandler(CPLQuietErrorHandler);
        GDALDataset *poDS =
            GDALDataset::FromHandle(GDALOpen(osTmpFilename, GA_ReadOnly));
        CPLPopErrorHandler();
        // Reject GeoJP2 boxes with a TIFF with band_count > 1.
        if (poDS && poDS->GetRasterCount() > 1)
        {
            GDALClose(poDS);
            poDS = nullptr;
        }
        if (poDS)
        {
            CPLString osTmpVRTFilename(
                CPLSPrintf("/vsimem/tmp_%p.vrt", oBox.GetFILE()));
            GDALDataset *poVRTDS = poVRTDriver->CreateCopy(
                osTmpVRTFilename, poDS, FALSE, nullptr, nullptr, nullptr);
            GDALClose(poVRTDS);
            GByte *pabyXML =
                VSIGetMemFileBuffer(osTmpVRTFilename, nullptr, FALSE);
            CPLXMLNode *psXMLVRT =
                CPLParseXMLString(reinterpret_cast<const char *>(pabyXML));
            if (psXMLVRT)
            {
                ++psDumpContext->nCurLineCount;

                CPLXMLNode *psXMLContentNode =
                    CPLCreateXMLNode(psBox, CXT_Element, "DecodedGeoTIFF");
                psXMLContentNode->psChild = psXMLVRT;
                CPLXMLNode *psPrev = nullptr;
                for (CPLXMLNode *psIter = psXMLVRT->psChild; psIter;
                     psIter = psIter->psNext)
                {
                    if (psIter->eType == CXT_Element &&
                        strcmp(psIter->pszValue, "VRTRasterBand") == 0)
                    {
                        CPLXMLNode *psNext = psIter->psNext;
                        psIter->psNext = nullptr;
                        CPLDestroyXMLNode(psIter);
                        if (psPrev)
                            psPrev->psNext = psNext;
                        else
                            break;
                        psIter = psPrev;
                    }
                    psPrev = psIter;
                }
                CPLCreateXMLNode(psXMLVRT, CXT_Element, "VRTRasterBand");
            }

            VSIUnlink(osTmpVRTFilename);
            GDALClose(poDS);
        }
        VSIUnlink(osTmpFilename);
    }
    CPLFree(pabyBoxData);
}

static void DumpFTYPBox(CPLXMLNode *psBox, GDALJP2Box &oBox,
                        DumpContext *psDumpContext)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte *pabyBoxData = oBox.ReadBoxData();
    if (pabyBoxData)
    {
        CPLXMLNode *psDecodedContent =
            CPLCreateXMLNode(psBox, CXT_Element, "DecodedContent");
        GIntBig nRemainingLength = nBoxDataLength;
        GByte *pabyIter = pabyBoxData;
        CPLXMLNode *psLastChild = nullptr;
        if (nRemainingLength >= 4)
        {
            char szBranding[5];
            memcpy(szBranding, pabyIter, 4);
            szBranding[4] = 0;
            AddField(psDecodedContent, psLastChild, psDumpContext, "BR", 4,
                     szBranding);
            pabyIter += 4;
            nRemainingLength -= 4;
        }
        if (nRemainingLength >= 4)
        {
            GUInt32 nVal;
            memcpy(&nVal, pabyIter, 4);
            CPL_MSBPTR32(&nVal);
            AddField(psDecodedContent, psLastChild, psDumpContext, "MinV",
                     nVal);
            pabyIter += 4;
            nRemainingLength -= 4;
        }
        int nCLIndex = 0;
        while (nRemainingLength >= 4)
        {
            char szBranding[5];
            memcpy(szBranding, pabyIter, 4);
            szBranding[4] = 0;
            AddField(psDecodedContent, psLastChild, psDumpContext,
                     CPLSPrintf("CL%d", nCLIndex), 4, szBranding);
            pabyIter += 4;
            nRemainingLength -= 4;
            nCLIndex++;
        }
        if (nRemainingLength > 0)
            AddElement(
                psDecodedContent, psLastChild, psDumpContext,
                CPLCreateXMLElementAndValue(
                    nullptr, "RemainingBytes",
                    CPLSPrintf("%d", static_cast<int>(nRemainingLength))));
    }
    CPLFree(pabyBoxData);
}

static void DumpIHDRBox(CPLXMLNode *psBox, GDALJP2Box &oBox,
                        DumpContext *psDumpContext)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte *pabyBoxData = oBox.ReadBoxData();
    if (pabyBoxData)
    {
        CPLXMLNode *psDecodedContent =
            CPLCreateXMLNode(psBox, CXT_Element, "DecodedContent");
        GIntBig nRemainingLength = nBoxDataLength;
        GByte *pabyIter = pabyBoxData;
        CPLXMLNode *psLastChild = nullptr;
        if (nRemainingLength >= 4)
        {
            GUInt32 nVal;
            memcpy(&nVal, pabyIter, 4);
            CPL_MSBPTR32(&nVal);
            AddField(psDecodedContent, psLastChild, psDumpContext, "HEIGHT",
                     nVal);
            pabyIter += 4;
            nRemainingLength -= 4;
        }
        if (nRemainingLength >= 4)
        {
            GUInt32 nVal;
            memcpy(&nVal, pabyIter, 4);
            CPL_MSBPTR32(&nVal);
            AddField(psDecodedContent, psLastChild, psDumpContext, "WIDTH",
                     nVal);
            pabyIter += 4;
            nRemainingLength -= 4;
        }
        if (nRemainingLength >= 2)
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            AddField(psDecodedContent, psLastChild, psDumpContext, "NC", nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        if (nRemainingLength >= 1)
        {
            AddField(psDecodedContent, psLastChild, psDumpContext, "BPC",
                     *pabyIter, GetInterpretationOfBPC(*pabyIter));
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if (nRemainingLength >= 1)
        {
            AddField(psDecodedContent, psLastChild, psDumpContext, "C",
                     *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if (nRemainingLength >= 1)
        {
            AddField(psDecodedContent, psLastChild, psDumpContext, "UnkC",
                     *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if (nRemainingLength >= 1)
        {
            AddField(psDecodedContent, psLastChild, psDumpContext, "IPR",
                     *pabyIter);
            /*pabyIter += 1;*/
            nRemainingLength -= 1;
        }
        if (nRemainingLength > 0)
            AddElement(
                psDecodedContent, psLastChild, psDumpContext,
                CPLCreateXMLElementAndValue(
                    nullptr, "RemainingBytes",
                    CPLSPrintf("%d", static_cast<int>(nRemainingLength))));
    }
    CPLFree(pabyBoxData);
}

static void DumpBPCCBox(CPLXMLNode *psBox, GDALJP2Box &oBox,
                        DumpContext *psDumpContext)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte *pabyBoxData = oBox.ReadBoxData();
    if (pabyBoxData)
    {
        CPLXMLNode *psDecodedContent =
            CPLCreateXMLNode(psBox, CXT_Element, "DecodedContent");
        GIntBig nRemainingLength = nBoxDataLength;
        GByte *pabyIter = pabyBoxData;
        int nBPCIndex = 0;
        CPLXMLNode *psLastChild = nullptr;
        while (nRemainingLength >= 1 && nBPCIndex < knbMaxJPEG2000Components)
        {
            AddField(psDecodedContent, psLastChild, psDumpContext,
                     CPLSPrintf("BPC%d", nBPCIndex), *pabyIter,
                     GetInterpretationOfBPC(*pabyIter));
            nBPCIndex++;
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if (nRemainingLength > 0)
            AddElement(
                psDecodedContent, psLastChild, psDumpContext,
                CPLCreateXMLElementAndValue(
                    nullptr, "RemainingBytes",
                    CPLSPrintf("%d", static_cast<int>(nRemainingLength))));
    }
    CPLFree(pabyBoxData);
}

static void DumpCOLRBox(CPLXMLNode *psBox, GDALJP2Box &oBox,
                        DumpContext *psDumpContext)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte *pabyBoxData = oBox.ReadBoxData();
    if (pabyBoxData)
    {
        CPLXMLNode *psDecodedContent =
            CPLCreateXMLNode(psBox, CXT_Element, "DecodedContent");
        GIntBig nRemainingLength = nBoxDataLength;
        GByte *pabyIter = pabyBoxData;
        GByte nMeth;
        CPLXMLNode *psLastChild = nullptr;
        if (nRemainingLength >= 1)
        {
            nMeth = *pabyIter;
            AddField(psDecodedContent, psLastChild, psDumpContext, "METH",
                     nMeth,
                     (nMeth == 0)   ? "Enumerated Colourspace"
                     : (nMeth == 1) ? "Restricted ICC profile"
                                    : nullptr);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if (nRemainingLength >= 1)
        {
            AddField(psDecodedContent, psLastChild, psDumpContext, "PREC",
                     *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if (nRemainingLength >= 1)
        {
            AddField(psDecodedContent, psLastChild, psDumpContext, "APPROX",
                     *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if (nRemainingLength >= 4)
        {
            GUInt32 nVal;
            memcpy(&nVal, pabyIter, 4);
            CPL_MSBPTR32(&nVal);
            AddField(psDecodedContent, psLastChild, psDumpContext, "EnumCS",
                     nVal,
                     (nVal == 16)   ? "sRGB"
                     : (nVal == 17) ? "greyscale"
                     : (nVal == 18) ? "sYCC"
                                    : nullptr);
            /*pabyIter += 4;*/
            nRemainingLength -= 4;
        }
        if (nRemainingLength > 0)
            AddElement(
                psDecodedContent, psLastChild, psDumpContext,
                CPLCreateXMLElementAndValue(
                    nullptr, "RemainingBytes",
                    CPLSPrintf("%d", static_cast<int>(nRemainingLength))));
    }
    CPLFree(pabyBoxData);
}

static void DumpPCLRBox(CPLXMLNode *psBox, GDALJP2Box &oBox,
                        DumpContext *psDumpContext)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte *pabyBoxData = oBox.ReadBoxData();
    if (pabyBoxData)
    {
        CPLXMLNode *psDecodedContent =
            CPLCreateXMLNode(psBox, CXT_Element, "DecodedContent");
        GIntBig nRemainingLength = nBoxDataLength;
        GByte *pabyIter = pabyBoxData;
        GUInt16 NE = 0;
        CPLXMLNode *psLastChild = nullptr;
        if (nRemainingLength >= 2)
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            NE = nVal;
            AddField(psDecodedContent, psLastChild, psDumpContext, "NE", nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        GByte NPC = 0;
        if (nRemainingLength >= 1)
        {
            NPC = *pabyIter;
            AddField(psDecodedContent, psLastChild, psDumpContext, "NPC", NPC);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        int b8BitOnly = TRUE;
        for (int i = 0; i < NPC; i++)
        {
            if (nRemainingLength >= 1)
            {
                b8BitOnly &= (*pabyIter <= 7);
                AddField(psDecodedContent, psLastChild, psDumpContext,
                         CPLSPrintf("B%d", i), *pabyIter,
                         GetInterpretationOfBPC(*pabyIter));
                pabyIter += 1;
                nRemainingLength -= 1;
            }
        }
        if (b8BitOnly)
        {
            for (int j = 0; j < NE; j++)
            {
                for (int i = 0; i < NPC; i++)
                {
                    if (nRemainingLength >= 1)
                    {
                        AddField(psDecodedContent, psLastChild, psDumpContext,
                                 CPLSPrintf("C_%d_%d", j, i), *pabyIter);
                        pabyIter += 1;
                        nRemainingLength -= 1;
                    }
                }
            }
        }
        if (nRemainingLength > 0)
            AddElement(
                psDecodedContent, psLastChild, psDumpContext,
                CPLCreateXMLElementAndValue(
                    nullptr, "RemainingBytes",
                    CPLSPrintf("%d", static_cast<int>(nRemainingLength))));
    }
    CPLFree(pabyBoxData);
}

static void DumpCMAPBox(CPLXMLNode *psBox, GDALJP2Box &oBox,
                        DumpContext *psDumpContext)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte *pabyBoxData = oBox.ReadBoxData();
    if (pabyBoxData)
    {
        CPLXMLNode *psDecodedContent =
            CPLCreateXMLNode(psBox, CXT_Element, "DecodedContent");
        GIntBig nRemainingLength = nBoxDataLength;
        GByte *pabyIter = pabyBoxData;
        int nIndex = 0;
        CPLXMLNode *psLastChild = nullptr;
        while (nRemainingLength >= 2 + 1 + 1 &&
               nIndex < knbMaxJPEG2000Components)
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            AddField(psDecodedContent, psLastChild, psDumpContext,
                     CPLSPrintf("CMP%d", nIndex), nVal);
            pabyIter += 2;
            nRemainingLength -= 2;

            AddField(psDecodedContent, psLastChild, psDumpContext,
                     CPLSPrintf("MTYP%d", nIndex), *pabyIter,
                     (*pabyIter == 0)   ? "Direct use"
                     : (*pabyIter == 1) ? "Palette mapping"
                                        : nullptr);
            pabyIter += 1;
            nRemainingLength -= 1;

            AddField(psDecodedContent, psLastChild, psDumpContext,
                     CPLSPrintf("PCOL%d", nIndex), *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;

            nIndex++;
        }
        if (nRemainingLength > 0)
            AddElement(
                psDecodedContent, psLastChild, psDumpContext,
                CPLCreateXMLElementAndValue(
                    nullptr, "RemainingBytes",
                    CPLSPrintf("%d", static_cast<int>(nRemainingLength))));
    }
    CPLFree(pabyBoxData);
}

static void DumpCDEFBox(CPLXMLNode *psBox, GDALJP2Box &oBox,
                        DumpContext *psDumpContext)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte *pabyBoxData = oBox.ReadBoxData();
    if (pabyBoxData)
    {
        CPLXMLNode *psDecodedContent =
            CPLCreateXMLNode(psBox, CXT_Element, "DecodedContent");
        GIntBig nRemainingLength = nBoxDataLength;
        GByte *pabyIter = pabyBoxData;
        GUInt16 nChannels = 0;
        CPLXMLNode *psLastChild = nullptr;
        if (nRemainingLength >= 2)
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            nChannels = nVal;
            CPL_MSBPTR16(&nVal);
            AddField(psDecodedContent, psLastChild, psDumpContext, "N", nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        for (int i = 0; i < nChannels; i++)
        {
            if (nRemainingLength >= 2)
            {
                GUInt16 nVal;
                memcpy(&nVal, pabyIter, 2);
                CPL_MSBPTR16(&nVal);
                AddField(psDecodedContent, psLastChild, psDumpContext,
                         CPLSPrintf("Cn%d", i), nVal);
                pabyIter += 2;
                nRemainingLength -= 2;
            }
            if (nRemainingLength >= 2)
            {
                GUInt16 nVal;
                memcpy(&nVal, pabyIter, 2);
                CPL_MSBPTR16(&nVal);
                AddField(psDecodedContent, psLastChild, psDumpContext,
                         CPLSPrintf("Typ%d", i), nVal,
                         (nVal == 0)       ? "Colour channel"
                         : (nVal == 1)     ? "Opacity channel"
                         : (nVal == 2)     ? "Premultiplied opacity"
                         : (nVal == 65535) ? "Not specified"
                                           : nullptr);
                pabyIter += 2;
                nRemainingLength -= 2;
            }
            if (nRemainingLength >= 2)
            {
                GUInt16 nVal;
                memcpy(&nVal, pabyIter, 2);
                CPL_MSBPTR16(&nVal);
                AddField(psDecodedContent, psLastChild, psDumpContext,
                         CPLSPrintf("Asoc%d", i), nVal,
                         (nVal == 0) ? "Associated to the whole image"
                         : (nVal == 65535)
                             ? "Not associated with a particular colour"
                             : "Associated with a particular colour");
                pabyIter += 2;
                nRemainingLength -= 2;
            }
        }
        if (nRemainingLength > 0)
            AddElement(
                psDecodedContent, psLastChild, psDumpContext,
                CPLCreateXMLElementAndValue(
                    nullptr, "RemainingBytes",
                    CPLSPrintf("%d", static_cast<int>(nRemainingLength))));
    }
    CPLFree(pabyBoxData);
}

static void DumpRESxBox(CPLXMLNode *psBox, GDALJP2Box &oBox,
                        DumpContext *psDumpContext)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte *pabyBoxData = oBox.ReadBoxData();
    char chC = oBox.GetType()[3];
    if (pabyBoxData)
    {
        CPLXMLNode *psDecodedContent =
            CPLCreateXMLNode(psBox, CXT_Element, "DecodedContent");
        GIntBig nRemainingLength = nBoxDataLength;
        GByte *pabyIter = pabyBoxData;
        GUInt16 nNumV = 0;
        GUInt16 nNumH = 0;
        GUInt16 nDenomV = 1;
        GUInt16 nDenomH = 1;
        GUInt16 nExpV = 0;
        GUInt16 nExpH = 0;
        CPLXMLNode *psLastChild = nullptr;
        if (nRemainingLength >= 2)
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            nNumV = nVal;
            AddField(psDecodedContent, psLastChild, psDumpContext,
                     CPLSPrintf("VR%cN", chC), nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        if (nRemainingLength >= 2)
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            nDenomV = nVal;
            AddField(psDecodedContent, psLastChild, psDumpContext,
                     CPLSPrintf("VR%cD", chC), nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        if (nRemainingLength >= 2)
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            nNumH = nVal;
            AddField(psDecodedContent, psLastChild, psDumpContext,
                     CPLSPrintf("HR%cN", chC), nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        if (nRemainingLength >= 2)
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            nDenomH = nVal;
            AddField(psDecodedContent, psLastChild, psDumpContext,
                     CPLSPrintf("HR%cD", chC), nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        if (nRemainingLength >= 1)
        {
            AddField(psDecodedContent, psLastChild, psDumpContext,
                     CPLSPrintf("VR%cE", chC), *pabyIter);
            nExpV = *pabyIter;
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if (nRemainingLength >= 1)
        {
            AddField(psDecodedContent, psLastChild, psDumpContext,
                     CPLSPrintf("HR%cE", chC), *pabyIter);
            nExpH = *pabyIter;
            /*pabyIter += 1;*/
            nRemainingLength -= 1;
        }
        if (nRemainingLength == 0)
        {
            const char *pszVRes =
                (nDenomV == 0) ? "invalid"
                               : CPLSPrintf("%.03f", 1.0 * nNumV / nDenomV *
                                                         pow(10.0, nExpV));
            AddElement(psDecodedContent, psLastChild, psDumpContext,
                       CPLCreateXMLElementAndValue(nullptr, "VRes", pszVRes));
            const char *pszHRes =
                (nDenomH == 0) ? "invalid"
                               : CPLSPrintf("%.03f", 1.0 * nNumH / nDenomH *
                                                         pow(10.0, nExpH));
            AddElement(psDecodedContent, psLastChild, psDumpContext,
                       CPLCreateXMLElementAndValue(nullptr, "HRes", pszHRes));
        }
        else if (nRemainingLength > 0)
            AddElement(
                psDecodedContent, psLastChild, psDumpContext,
                CPLCreateXMLElementAndValue(
                    nullptr, "RemainingBytes",
                    CPLSPrintf("%d", static_cast<int>(nRemainingLength))));
    }
    CPLFree(pabyBoxData);
}

static void DumpRREQBox(CPLXMLNode *psBox, GDALJP2Box &oBox,
                        DumpContext *psDumpContext)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte *pabyBoxData = oBox.ReadBoxData();
    if (pabyBoxData)
    {
        CPLXMLNode *psDecodedContent =
            CPLCreateXMLNode(psBox, CXT_Element, "DecodedContent");
        GIntBig nRemainingLength = nBoxDataLength;
        GByte *pabyIter = pabyBoxData;
        GByte ML = 0;
        CPLXMLNode *psLastChild = nullptr;
        if (nRemainingLength >= 1)
        {
            ML = *pabyIter;
            AddField(psDecodedContent, psLastChild, psDumpContext, "ML",
                     *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if (nRemainingLength >= ML)
        {
            CPLString osHex("0x");
            for (int i = 0; i < ML; i++)
            {
                osHex += CPLSPrintf("%02X", *pabyIter);
                pabyIter += 1;
                nRemainingLength -= 1;
            }
            AddHexField(psDecodedContent, psLastChild, psDumpContext, "FUAM",
                        static_cast<int>(ML), osHex.c_str());
        }
        if (nRemainingLength >= ML)
        {
            CPLString osHex("0x");
            for (int i = 0; i < ML; i++)
            {
                osHex += CPLSPrintf("%02X", *pabyIter);
                pabyIter += 1;
                nRemainingLength -= 1;
            }
            AddHexField(psDecodedContent, psLastChild, psDumpContext, "DCM",
                        static_cast<int>(ML), osHex.c_str());
        }
        GUInt16 NSF = 0;
        if (nRemainingLength >= 2)
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            NSF = nVal;
            AddField(psDecodedContent, psLastChild, psDumpContext, "NSF", nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        for (int iNSF = 0; iNSF < NSF; iNSF++)
        {
            if (nRemainingLength >= 2)
            {
                GUInt16 nVal;
                memcpy(&nVal, pabyIter, 2);
                CPL_MSBPTR16(&nVal);
                AddField(psDecodedContent, psLastChild, psDumpContext,
                         CPLSPrintf("SF%d", iNSF), nVal,
                         GetStandardFieldString(nVal));
                pabyIter += 2;
                nRemainingLength -= 2;
            }
            else
                break;
            if (nRemainingLength >= ML)
            {
                CPLString osHex("0x");
                for (int i = 0; i < ML; i++)
                {
                    osHex += CPLSPrintf("%02X", *pabyIter);
                    pabyIter += 1;
                    nRemainingLength -= 1;
                }
                AddHexField(psDecodedContent, psLastChild, psDumpContext,
                            CPLSPrintf("SM%d", iNSF), static_cast<int>(ML),
                            osHex.c_str());
            }
            else
                break;
        }
        GUInt16 NVF = 0;
        if (nRemainingLength >= 2)
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            NVF = nVal;
            AddField(psDecodedContent, psLastChild, psDumpContext, "NVF", nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        for (int iNVF = 0; iNVF < NVF; iNVF++)
        {
            if (nRemainingLength >= 16)
            {
                CPLString osHex("0x");
                for (int i = 0; i < 16; i++)
                {
                    osHex += CPLSPrintf("%02X", *pabyIter);
                    pabyIter += 1;
                    nRemainingLength -= 1;
                }
                AddHexField(psDecodedContent, psLastChild, psDumpContext,
                            CPLSPrintf("VF%d", iNVF), static_cast<int>(ML),
                            osHex.c_str());
            }
            else
                break;
            if (nRemainingLength >= ML)
            {
                CPLString osHex("0x");
                for (int i = 0; i < ML; i++)
                {
                    osHex += CPLSPrintf("%02X", *pabyIter);
                    pabyIter += 1;
                    nRemainingLength -= 1;
                }
                AddHexField(psDecodedContent, psLastChild, psDumpContext,
                            CPLSPrintf("VM%d", iNVF), static_cast<int>(ML),
                            osHex.c_str());
            }
            else
                break;
        }
        if (nRemainingLength > 0)
            AddElement(
                psDecodedContent, psLastChild, psDumpContext,
                CPLCreateXMLElementAndValue(
                    nullptr, "RemainingBytes",
                    CPLSPrintf("%d", static_cast<int>(nRemainingLength))));
    }
    CPLFree(pabyBoxData);
}

static CPLXMLNode *CreateMarker(CPLXMLNode *psCSBox,
                                CPLXMLNode *&psLastChildCSBox,
                                DumpContext *psDumpContext, const char *pszName,
                                GIntBig nOffset, GIntBig nLength)
{
    CPLXMLNode *psMarker = CPLCreateXMLNode(nullptr, CXT_Element, "Marker");
    CPLAddXMLAttributeAndValue(psMarker, "name", pszName);
    CPLAddXMLAttributeAndValue(psMarker, "offset",
                               CPLSPrintf(CPL_FRMT_GIB, nOffset));
    CPLAddXMLAttributeAndValue(psMarker, "length",
                               CPLSPrintf(CPL_FRMT_GIB, 2 + nLength));
    return AddElement(psCSBox, psLastChildCSBox, psDumpContext, psMarker);
}

static void AddError(CPLXMLNode *psParent, CPLXMLNode *&psLastChild,
                     DumpContext *psDumpContext, const char *pszErrorMsg,
                     GIntBig nOffset = 0)
{
    if (psDumpContext->nCurLineCount > psDumpContext->nMaxLineCount + 1)
    {
        return;
    }

    AddElement(psParent, psLastChild, psDumpContext,
               _AddError(nullptr, pszErrorMsg, nOffset));
}

static const char *GetMarkerName(GByte byVal)
{
    switch (byVal)
    {
        case 0x90:
            return "SOT";
        case 0x50:
            return "CAP";
        case 0x51:
            return "SIZ";
        case 0x52:
            return "COD";
        case 0x53:
            return "COC";
        case 0x55:
            return "TLM";
        case 0x57:
            return "PLM";
        case 0x58:
            return "PLT";
        case 0x5C:
            return "QCD";
        case 0x5D:
            return "QCC";
        case 0x5E:
            return "RGN";
        case 0x5F:
            return "POC";
        case 0x59:
            return "CPF";  // HTJ2K
        case 0x60:
            return "PPM";
        case 0x61:
            return "PPT";
        case 0x63:
            return "CRG";
        case 0x64:
            return "COM";
        default:
            return CPLSPrintf("Unknown 0xFF%02X", byVal);
    }
}

/************************************************************************/
/*                       DumpJPK2CodeStream()                           */
/************************************************************************/

static CPLXMLNode *DumpJPK2CodeStream(CPLXMLNode *psBox, VSILFILE *fp,
                                      GIntBig nBoxDataOffset,
                                      GIntBig nBoxDataLength,
                                      DumpContext *psDumpContext)
{
    GByte abyMarker[2];
    CPLXMLNode *psCSBox =
        CPLCreateXMLNode(psBox, CXT_Element, "JP2KCodeStream");
    CPLXMLNode *psLastChildCSBox = nullptr;
    if (VSIFSeekL(fp, nBoxDataOffset, SEEK_SET) != 0)
    {
        AddError(psCSBox, psLastChildCSBox, psDumpContext,
                 "Cannot read codestream", 0);
        return psCSBox;
    }
    GByte *pabyMarkerData = static_cast<GByte *>(CPLMalloc(65535 + 1));
    GIntBig nNextTileOffset = 0;
    int Csiz = -1;
    const auto lambdaPOCType = [](GByte v)
    {
        return std::string((v == 0)   ? "LRCP"
                           : (v == 1) ? "RLCP"
                           : (v == 2) ? "RPCL"
                           : (v == 3) ? "PCRL"
                           : (v == 4) ? "CPRL"
                                      : "");
    };

    while (psDumpContext->nCurLineCount <= psDumpContext->nMaxLineCount + 1)
    {
        GIntBig nOffset = static_cast<GIntBig>(VSIFTellL(fp));
        if (nBoxDataLength > 0 && nOffset == nBoxDataOffset + nBoxDataLength)
            break;
        if (VSIFReadL(abyMarker, 2, 1, fp) != 1)
        {
            AddError(psCSBox, psLastChildCSBox, psDumpContext,
                     "Cannot read marker", nOffset);
            break;
        }
        if (abyMarker[0] != 0xFF)
        {
            AddError(psCSBox, psLastChildCSBox, psDumpContext, "Not a marker",
                     nOffset);
            break;
        }
        if (abyMarker[1] == 0x4F)  // SOC
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "SOC"))
            {
                CreateMarker(psCSBox, psLastChildCSBox, psDumpContext, "SOC",
                             nOffset, 0);
            }
            continue;
        }
        if (abyMarker[1] == 0x93)  // SOD
        {
            const bool bIncludeSOD =
                (psDumpContext->pszCodestreamMarkers == nullptr ||
                 strstr(psDumpContext->pszCodestreamMarkers, "SOD"));
            if (psDumpContext->bStopAtSOD && !bIncludeSOD)
            {
                psDumpContext->bSODEncountered = true;
                break;
            }

            GIntBig nMarkerSize = 0;
            bool bBreak = false;
            if (nNextTileOffset == 0)
            {
                nMarkerSize =
                    (nBoxDataOffset + nBoxDataLength - 2) - nOffset - 2;
                if (VSIFSeekL(fp, nBoxDataOffset + nBoxDataLength - 2,
                              SEEK_SET) != 0 ||
                    VSIFReadL(abyMarker, 2, 1, fp) != 1 ||
                    abyMarker[0] != 0xFF || abyMarker[1] != 0xD9)
                {
                    /* autotest/gdrivers/data/rgb16_ecwsdk.jp2 does not end */
                    /* with a EOC... */
                    nMarkerSize += 2;
                    bBreak = true;
                }
            }
            else if (nNextTileOffset >= nOffset + 2)
                nMarkerSize = nNextTileOffset - nOffset - 2;

            if (bIncludeSOD)
            {
                CreateMarker(psCSBox, psLastChildCSBox, psDumpContext, "SOD",
                             nOffset, nMarkerSize);
            }
            if (bBreak || psDumpContext->bStopAtSOD)
            {
                psDumpContext->bSODEncountered = true;
                break;
            }

            if (nNextTileOffset && nNextTileOffset == nOffset)
            {
                /* Found with Pleiades images. openjpeg doesn't like it either
                 */
                nNextTileOffset = 0;
            }
            else if (nNextTileOffset && nNextTileOffset >= nOffset + 2)
            {
                if (VSIFSeekL(fp, nNextTileOffset, SEEK_SET) != 0)
                    AddError(psCSBox, psLastChildCSBox, psDumpContext,
                             "Cannot seek to", nNextTileOffset);
                nNextTileOffset = 0;
            }
            else
            {
                /* We have seek and check before we hit a EOC */
                nOffset = nBoxDataOffset + nBoxDataLength - 2;
                if (psDumpContext->pszCodestreamMarkers == nullptr ||
                    strstr(psDumpContext->pszCodestreamMarkers, "EOC"))
                {
                    CreateMarker(psCSBox, psLastChildCSBox, psDumpContext,
                                 "EOC", nOffset, 0);
                }
            }
            continue;
        }
        if (abyMarker[1] == 0xD9)
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "EOC"))
            {
                CreateMarker(psCSBox, psLastChildCSBox, psDumpContext, "EOC",
                             nOffset, 0);
            }
            continue;
        }
        /* Reserved markers */
        if (abyMarker[1] >= 0x30 && abyMarker[1] <= 0x3F)
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr)
            {
                CreateMarker(psCSBox, psLastChildCSBox, psDumpContext,
                             CPLSPrintf("Unknown 0xFF%02X", abyMarker[1]),
                             nOffset, 0);
            }
            continue;
        }

        GUInt16 nMarkerSize;
        if (VSIFReadL(&nMarkerSize, 2, 1, fp) != 1)
        {
            AddError(psCSBox, psLastChildCSBox, psDumpContext,
                     CPLSPrintf("Cannot read marker size of %s",
                                GetMarkerName(abyMarker[1])),
                     nOffset);
            break;
        }
        CPL_MSBPTR16(&nMarkerSize);
        if (nMarkerSize < 2)
        {
            AddError(psCSBox, psLastChildCSBox, psDumpContext,
                     CPLSPrintf("Invalid marker size of %s",
                                GetMarkerName(abyMarker[1])),
                     nOffset);
            break;
        }

        const auto CreateCurrentMarker = [&]()
        {
            return CreateMarker(psCSBox, psLastChildCSBox, psDumpContext,
                                GetMarkerName(abyMarker[1]), nOffset,
                                nMarkerSize);
        };
        CPLXMLNode *psMarker = nullptr;
        CPLXMLNode *psLastChild = nullptr;
        if (VSIFReadL(pabyMarkerData, nMarkerSize - 2, 1, fp) != 1)
        {
            psMarker = CreateCurrentMarker();
            AddError(psMarker, psLastChild, psDumpContext,
                     "Cannot read marker data", nOffset);
            break;
        }
        GByte *pabyMarkerDataIter = pabyMarkerData;
        GUInt16 nRemainingMarkerSize = nMarkerSize - 2;
        bool bError = false;

        auto READ_MARKER_FIELD_UINT8 =
            [&](const char *name, std::string (*commentFunc)(GByte) = nullptr)
        {
            GByte v;
            if (nRemainingMarkerSize >= 1)
            {
                v = *pabyMarkerDataIter;
                const auto comment =
                    commentFunc ? commentFunc(v) : std::string();
                AddField(psMarker, psLastChild, psDumpContext, name,
                         *pabyMarkerDataIter,
                         comment.empty() ? nullptr : comment.c_str());
                pabyMarkerDataIter += 1;
                nRemainingMarkerSize -= 1;
            }
            else
            {
                AddError(psMarker, psLastChild, psDumpContext,
                         CPLSPrintf("Cannot read field %s", name));
                v = 0;
                bError = true;
            }
            return v;
        };

        auto READ_MARKER_FIELD_UINT16 =
            [&](const char *name, std::string (*commentFunc)(GUInt16) = nullptr)
        {
            GUInt16 v;
            if (nRemainingMarkerSize >= 2)
            {
                memcpy(&v, pabyMarkerDataIter, 2);
                CPL_MSBPTR16(&v);
                const auto comment =
                    commentFunc ? commentFunc(v) : std::string();
                AddField(psMarker, psLastChild, psDumpContext, name, v,
                         comment.empty() ? nullptr : comment.c_str());
                pabyMarkerDataIter += 2;
                nRemainingMarkerSize -= 2;
            }
            else
            {
                AddError(psMarker, psLastChild, psDumpContext,
                         CPLSPrintf("Cannot read field %s", name));
                v = 0;
                bError = true;
            }
            return v;
        };

        auto READ_MARKER_FIELD_UINT32 =
            [&](const char *name, std::string (*commentFunc)(GUInt32) = nullptr)
        {
            GUInt32 v;
            if (nRemainingMarkerSize >= 4)
            {
                memcpy(&v, pabyMarkerDataIter, 4);
                CPL_MSBPTR32(&v);
                const auto comment =
                    commentFunc ? commentFunc(v) : std::string();
                AddField(psMarker, psLastChild, psDumpContext, name, v,
                         comment.empty() ? nullptr : comment.c_str());
                pabyMarkerDataIter += 4;
                nRemainingMarkerSize -= 4;
            }
            else
            {
                AddError(psMarker, psLastChild, psDumpContext,
                         CPLSPrintf("Cannot read field %s", name));
                v = 0;
                bError = true;
            }
            return v;
        };

        const auto cblkstyleLamba = [](GByte v)
        {
            std::string osInterp;
            if (v & 0x1)
                osInterp += "Selective arithmetic coding bypass";
            else
                osInterp += "No selective arithmetic coding bypass";
            osInterp += ", ";
            if (v & 0x2)
                osInterp +=
                    "Reset context probabilities on coding pass boundaries";
            else
                osInterp += "No reset of context probabilities on coding pass "
                            "boundaries";
            osInterp += ", ";
            if (v & 0x4)
                osInterp += "Termination on each coding pass";
            else
                osInterp += "No termination on each coding pass";
            osInterp += ", ";
            if (v & 0x8)
                osInterp += "Vertically causal context";
            else
                osInterp += "No vertically causal context";
            osInterp += ", ";
            if (v & 0x10)
                osInterp += "Predictable termination";
            else
                osInterp += "No predictable termination";
            osInterp += ", ";
            if (v & 0x20)
                osInterp += "Segmentation symbols are used";
            else
                osInterp += "No segmentation symbols are used";
            if (v & 0x40)
                osInterp += ", High Throughput algorithm";
            if (v & 0x80)
                osInterp += ", Mixed HT and Part1 code-block style";
            return osInterp;
        };

        if (abyMarker[1] == 0x90) /* SOT */
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "SOT"))
            {
                psMarker = CreateCurrentMarker();
                if (!psMarker)
                    break;
                READ_MARKER_FIELD_UINT16("Isot");
                GUInt32 PSOT = READ_MARKER_FIELD_UINT32("Psot");
                READ_MARKER_FIELD_UINT8("TPsot");
                READ_MARKER_FIELD_UINT8("TNsot");
                if (nRemainingMarkerSize > 0)
                    AddElement(
                        psMarker, psLastChild, psDumpContext,
                        CPLCreateXMLElementAndValue(
                            nullptr, "RemainingBytes",
                            CPLSPrintf(
                                "%d", static_cast<int>(nRemainingMarkerSize))));

                if (PSOT)
                    nNextTileOffset = nOffset + PSOT;
            }
        }
        else if (abyMarker[1] == 0x50) /* CAP (HTJ2K) */
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "CAP"))
            {
                psMarker = CreateCurrentMarker();
                if (!psMarker)
                    break;
                const GUInt32 Pcap = READ_MARKER_FIELD_UINT32("Pcap");
                for (int i = 0; i < 32; i++)
                {
                    if ((Pcap >> (31 - i)) & 1)
                    {
                        if (i + 1 == 15)
                        {
                            READ_MARKER_FIELD_UINT16(
                                CPLSPrintf("Scap_P%d", i + 1),
                                [](GUInt16 v)
                                {
                                    std::string ret;
                                    if ((v >> 14) == 0)
                                        ret = "All code-blocks are HT "
                                              "code-blocks";
                                    else if ((v >> 14) == 2)
                                        ret = "Either all HT or all Part1 "
                                              "code-blocks per tile component";
                                    else if ((v >> 14) == 3)
                                        ret = "Mixed HT or all Part1 "
                                              "code-blocks per tile component";
                                    else
                                        ret =
                                            "Reserved value for bit 14 and 15";
                                    ret += ", ";
                                    if ((v >> 13) & 1)
                                        ret += "More than one HT set per "
                                               "code-block";
                                    else
                                        ret +=
                                            "Zero or one HT set per code-block";
                                    ret += ", ";
                                    if ((v >> 12) & 1)
                                        ret += "ROI marker can be present";
                                    else
                                        ret += "No ROI marker";
                                    ret += ", ";
                                    if ((v >> 11) & 1)
                                        ret += "Heterogeneous codestream";
                                    else
                                        ret += "Homogeneous codestream";
                                    ret += ", ";
                                    if ((v >> 5) & 1)
                                        ret += "HT code-blocks can be used "
                                               "with irreversible transforms";
                                    else
                                        ret += "HT code-blocks only used with "
                                               "reversible transforms";
                                    ret += ", ";
                                    ret += "P=";
                                    ret += CPLSPrintf("%d", v & 0x31);
                                    return ret;
                                });
                        }
                        else
                        {
                            READ_MARKER_FIELD_UINT16(
                                CPLSPrintf("Scap_P%d", i + 1));
                        }
                    }
                }
                if (nRemainingMarkerSize > 0)
                    AddElement(
                        psMarker, psLastChild, psDumpContext,
                        CPLCreateXMLElementAndValue(
                            nullptr, "RemainingBytes",
                            CPLSPrintf(
                                "%d", static_cast<int>(nRemainingMarkerSize))));
            }
        }
        else if (abyMarker[1] == 0x51) /* SIZ */
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "SIZ"))
            {
                psMarker = CreateCurrentMarker();
                if (!psMarker)
                    break;
                READ_MARKER_FIELD_UINT16(
                    "Rsiz",
                    [](GUInt16 v)
                    {
                        return std::string((v == 0)   ? "Unrestricted profile"
                                           : (v == 1) ? "Profile 0"
                                           : (v == 2) ? "Profile 1"
                                           : (v == 16384) ? "HTJ2K"
                                                          : "");
                    });
                READ_MARKER_FIELD_UINT32("Xsiz");
                READ_MARKER_FIELD_UINT32("Ysiz");
                READ_MARKER_FIELD_UINT32("XOsiz");
                READ_MARKER_FIELD_UINT32("YOsiz");
                READ_MARKER_FIELD_UINT32("XTsiz");
                READ_MARKER_FIELD_UINT32("YTsiz");
                READ_MARKER_FIELD_UINT32("XTOSiz");
                READ_MARKER_FIELD_UINT32("YTOSiz");
                Csiz = READ_MARKER_FIELD_UINT16("Csiz");
                bError = false;
                // cppcheck-suppress knownConditionTrueFalse
                for (int i = 0; i < Csiz && !bError; i++)
                {
                    READ_MARKER_FIELD_UINT8(
                        CPLSPrintf("Ssiz%d", i),
                        [](GByte v)
                        {
                            const char *psz = GetInterpretationOfBPC(v);
                            return std::string(psz ? psz : "");
                        });
                    READ_MARKER_FIELD_UINT8(CPLSPrintf("XRsiz%d", i));
                    READ_MARKER_FIELD_UINT8(CPLSPrintf("YRsiz%d", i));
                }
                if (nRemainingMarkerSize > 0)
                    AddElement(
                        psMarker, psLastChild, psDumpContext,
                        CPLCreateXMLElementAndValue(
                            nullptr, "RemainingBytes",
                            CPLSPrintf(
                                "%d", static_cast<int>(nRemainingMarkerSize))));
            }
        }
        else if (abyMarker[1] == 0x52) /* COD */
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "COD"))
            {
                psMarker = CreateCurrentMarker();
                if (!psMarker)
                    break;
                bool bHasPrecincts = false;
                if (nRemainingMarkerSize >= 1)
                {
                    auto nLastVal = *pabyMarkerDataIter;
                    CPLString osInterp;
                    if (nLastVal & 0x1)
                    {
                        bHasPrecincts = true;
                        osInterp += "User defined precincts";
                    }
                    else
                        osInterp += "Standard precincts";
                    osInterp += ", ";
                    if (nLastVal & 0x2)
                        osInterp += "SOP marker segments may be used";
                    else
                        osInterp += "No SOP marker segments";
                    osInterp += ", ";
                    if (nLastVal & 0x4)
                        osInterp += "EPH marker segments may be used";
                    else
                        osInterp += "No EPH marker segments";
                    AddField(psMarker, psLastChild, psDumpContext, "Scod",
                             nLastVal, osInterp.c_str());
                    pabyMarkerDataIter += 1;
                    nRemainingMarkerSize -= 1;
                }
                else
                {
                    AddError(psMarker, psLastChild, psDumpContext,
                             CPLSPrintf("Cannot read field %s", "Scod"));
                }
                READ_MARKER_FIELD_UINT8("SGcod_Progress", lambdaPOCType);
                READ_MARKER_FIELD_UINT16("SGcod_NumLayers");
                READ_MARKER_FIELD_UINT8("SGcod_MCT");
                READ_MARKER_FIELD_UINT8("SPcod_NumDecompositions");
                READ_MARKER_FIELD_UINT8(
                    "SPcod_xcb_minus_2",
                    [](GByte v) {
                        return std::string(v <= 8
                                               ? CPLSPrintf("%d", 1 << (2 + v))
                                               : "invalid");
                    });
                READ_MARKER_FIELD_UINT8(
                    "SPcod_ycb_minus_2",
                    [](GByte v) {
                        return std::string(v <= 8
                                               ? CPLSPrintf("%d", 1 << (2 + v))
                                               : "invalid");
                    });
                READ_MARKER_FIELD_UINT8("SPcod_cbstyle", cblkstyleLamba);
                READ_MARKER_FIELD_UINT8("SPcod_transformation",
                                        [](GByte v)
                                        {
                                            return std::string(
                                                (v == 0)   ? "9-7 irreversible"
                                                : (v == 1) ? "5-3 reversible"
                                                           : "");
                                        });
                if (bHasPrecincts)
                {
                    int i = 0;
                    while (nRemainingMarkerSize >= 1)
                    {
                        auto nLastVal = *pabyMarkerDataIter;
                        AddField(psMarker, psLastChild, psDumpContext,
                                 CPLSPrintf("SPcod_Precincts%d", i),
                                 *pabyMarkerDataIter,
                                 CPLSPrintf("PPx=%d PPy=%d: %dx%d",
                                            nLastVal & 0xf, nLastVal >> 4,
                                            1 << (nLastVal & 0xf),
                                            1 << (nLastVal >> 4)));
                        pabyMarkerDataIter += 1;
                        nRemainingMarkerSize -= 1;
                        i++;
                    }
                }
                if (nRemainingMarkerSize > 0)
                    AddElement(
                        psMarker, psLastChild, psDumpContext,
                        CPLCreateXMLElementAndValue(
                            nullptr, "RemainingBytes",
                            CPLSPrintf(
                                "%d", static_cast<int>(nRemainingMarkerSize))));
            }
        }
        else if (abyMarker[1] == 0x53) /* COC */
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "COC"))
            {
                psMarker = CreateCurrentMarker();
                if (!psMarker)
                    break;
                if (Csiz < 257)
                    READ_MARKER_FIELD_UINT8("Ccoc");
                else
                    READ_MARKER_FIELD_UINT16("Ccoc");

                bool bHasPrecincts = false;
                if (nRemainingMarkerSize >= 1)
                {
                    auto nLastVal = *pabyMarkerDataIter;
                    CPLString osInterp;
                    if (nLastVal & 0x1)
                    {
                        bHasPrecincts = true;
                        osInterp += "User defined precincts";
                    }
                    else
                        osInterp += "Standard precincts";
                    AddField(psMarker, psLastChild, psDumpContext, "Scoc",
                             nLastVal, osInterp.c_str());
                    pabyMarkerDataIter += 1;
                    nRemainingMarkerSize -= 1;
                }
                else
                {
                    AddError(psMarker, psLastChild, psDumpContext,
                             CPLSPrintf("Cannot read field %s", "Scoc"));
                }
                READ_MARKER_FIELD_UINT8("SPcoc_NumDecompositions");
                READ_MARKER_FIELD_UINT8(
                    "SPcoc_xcb_minus_2",
                    [](GByte v) {
                        return std::string(v <= 8
                                               ? CPLSPrintf("%d", 1 << (2 + v))
                                               : "invalid");
                    });
                READ_MARKER_FIELD_UINT8(
                    "SPcoc_ycb_minus_2",
                    [](GByte v) {
                        return std::string(v <= 8
                                               ? CPLSPrintf("%d", 1 << (2 + v))
                                               : "invalid");
                    });
                READ_MARKER_FIELD_UINT8("SPcoc_cbstyle", cblkstyleLamba);
                READ_MARKER_FIELD_UINT8("SPcoc_transformation",
                                        [](GByte v)
                                        {
                                            return std::string(
                                                (v == 0)   ? "9-7 irreversible"
                                                : (v == 1) ? "5-3 reversible"
                                                           : "");
                                        });
                if (bHasPrecincts)
                {
                    int i = 0;
                    while (nRemainingMarkerSize >= 1)
                    {
                        auto nLastVal = *pabyMarkerDataIter;
                        AddField(psMarker, psLastChild, psDumpContext,
                                 CPLSPrintf("SPcoc_Precincts%d", i),
                                 *pabyMarkerDataIter,
                                 CPLSPrintf("PPx=%d PPy=%d: %dx%d",
                                            nLastVal & 0xf, nLastVal >> 4,
                                            1 << (nLastVal & 0xf),
                                            1 << (nLastVal >> 4)));
                        pabyMarkerDataIter += 1;
                        nRemainingMarkerSize -= 1;
                        i++;
                    }
                }
                if (nRemainingMarkerSize > 0)
                    AddElement(
                        psMarker, psLastChild, psDumpContext,
                        CPLCreateXMLElementAndValue(
                            nullptr, "RemainingBytes",
                            CPLSPrintf(
                                "%d", static_cast<int>(nRemainingMarkerSize))));
            }
        }
        else if (abyMarker[1] == 0x55) /* TLM */
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "TLM"))
            {
                psMarker = CreateCurrentMarker();
                if (!psMarker)
                    break;
                READ_MARKER_FIELD_UINT8("Ztlm");
                auto Stlm = READ_MARKER_FIELD_UINT8(
                    "Stlm",
                    [](GByte v) {
                        return std::string(CPLSPrintf(
                            "ST=%d SP=%d", (v >> 4) & 3, (v >> 6) & 1));
                    });
                int ST = (Stlm >> 4) & 3;
                int SP = (Stlm >> 6) & 1;
                int nTilePartDescLength = ST + ((SP == 0) ? 2 : 4);
                int i = 0;
                while (nRemainingMarkerSize >= nTilePartDescLength)
                {
                    if (ST == 1)
                        READ_MARKER_FIELD_UINT8(CPLSPrintf("Ttlm%d", i));
                    else if (ST == 2)
                        READ_MARKER_FIELD_UINT16(CPLSPrintf("Ttlm%d", i));
                    if (SP == 0)
                        READ_MARKER_FIELD_UINT16(CPLSPrintf("Ptlm%d", i));
                    else
                        READ_MARKER_FIELD_UINT32(CPLSPrintf("Ptlm%d", i));
                    i++;
                }
                if (nRemainingMarkerSize > 0)
                    AddElement(
                        psMarker, psLastChild, psDumpContext,
                        CPLCreateXMLElementAndValue(
                            nullptr, "RemainingBytes",
                            CPLSPrintf(
                                "%d", static_cast<int>(nRemainingMarkerSize))));
            }
        }
        else if (abyMarker[1] == 0x57) /* PLM */
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "PLM"))
            {
                psMarker = CreateCurrentMarker();
                if (!psMarker)
                    break;
            }
        }
        else if (abyMarker[1] == 0x58) /* PLT */
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "PLT"))
            {
                psMarker = CreateCurrentMarker();
                if (!psMarker)
                    break;
                READ_MARKER_FIELD_UINT8("Zplt");
                int i = 0;
                unsigned nPacketLength = 0;
                while (nRemainingMarkerSize >= 1)
                {
                    auto nLastVal = *pabyMarkerDataIter;
                    nPacketLength |= (nLastVal & 0x7f);
                    if (nLastVal & 0x80)
                    {
                        nPacketLength <<= 7;
                    }
                    else
                    {
                        AddField(psMarker, psLastChild, psDumpContext,
                                 CPLSPrintf("Iplt%d", i), nPacketLength);
                        nPacketLength = 0;
                        i++;
                    }
                    pabyMarkerDataIter += 1;
                    nRemainingMarkerSize -= 1;
                }
                if (nPacketLength != 0)
                {
                    AddError(psMarker, psLastChild, psDumpContext,
                             "Incorrect PLT marker");
                }
            }
        }
        else if (abyMarker[1] == 0x59) /* CPF (HTJ2K) */
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "CPF"))
            {
                psMarker = CreateCurrentMarker();
                if (!psMarker)
                    break;
                const GUInt16 Lcpf = nMarkerSize;
                if (Lcpf > 2 && (Lcpf % 2) == 0)
                {
                    for (GUInt16 i = 0; i < (Lcpf - 2) / 2; i++)
                    {
                        READ_MARKER_FIELD_UINT16(CPLSPrintf("Pcpf%d", i + 1));
                    }
                }
                if (nRemainingMarkerSize > 0)
                    AddElement(
                        psMarker, psLastChild, psDumpContext,
                        CPLCreateXMLElementAndValue(
                            nullptr, "RemainingBytes",
                            CPLSPrintf(
                                "%d", static_cast<int>(nRemainingMarkerSize))));
            }
        }
        else if (abyMarker[1] == 0x5C) /* QCD */
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "QCD"))
            {
                psMarker = CreateCurrentMarker();
                if (!psMarker)
                    break;
                const int Sqcd = READ_MARKER_FIELD_UINT8(
                    "Sqcd",
                    [](GByte v)
                    {
                        std::string ret;
                        if ((v & 31) == 0)
                            ret = "No quantization";
                        else if ((v & 31) == 1)
                            ret = "Scalar derived";
                        else if ((v & 31) == 2)
                            ret = "Scalar expounded";
                        ret += ", ";
                        ret += CPLSPrintf("guard bits = %d", v >> 5);
                        return ret;
                    });
                if ((Sqcd & 31) == 0)
                {
                    // Reversible
                    int i = 0;
                    while (nRemainingMarkerSize >= 1)
                    {
                        READ_MARKER_FIELD_UINT8(
                            CPLSPrintf("SPqcd%d", i),
                            [](GByte v) {
                                return std::string(
                                    CPLSPrintf("epsilon_b = %d", v >> 3));
                            });
                        ++i;
                    }
                }
                else
                {
                    int i = 0;
                    while (nRemainingMarkerSize >= 2)
                    {
                        READ_MARKER_FIELD_UINT16(
                            CPLSPrintf("SPqcd%d", i),
                            [](GUInt16 v)
                            {
                                return std::string(CPLSPrintf(
                                    "mantissa_b = %d, epsilon_b = %d",
                                    v & ((1 << 11) - 1), v >> 11));
                            });
                        ++i;
                    }
                }
            }
        }
        else if (abyMarker[1] == 0x5D) /* QCC */
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "QCC"))
            {
                psMarker = CreateCurrentMarker();
                if (!psMarker)
                    break;
                if (Csiz < 257)
                    READ_MARKER_FIELD_UINT8("Cqcc");
                else
                    READ_MARKER_FIELD_UINT16("Cqcc");

                const int Sqcc = READ_MARKER_FIELD_UINT8(
                    "Sqcc",
                    [](GByte v)
                    {
                        std::string ret;
                        if ((v & 31) == 0)
                            ret = "No quantization";
                        else if ((v & 31) == 1)
                            ret = "Scalar derived";
                        else if ((v & 31) == 2)
                            ret = "Scalar expounded";
                        ret += ", ";
                        ret += CPLSPrintf("guard bits = %d", v >> 5);
                        return ret;
                    });
                if ((Sqcc & 31) == 0)
                {
                    // Reversible
                    int i = 0;
                    while (nRemainingMarkerSize >= 1)
                    {
                        READ_MARKER_FIELD_UINT8(
                            CPLSPrintf("SPqcc%d", i),
                            [](GByte v) {
                                return std::string(
                                    CPLSPrintf("epsilon_b = %d", v >> 3));
                            });
                        ++i;
                    }
                }
                else
                {
                    int i = 0;
                    while (nRemainingMarkerSize >= 2)
                    {
                        READ_MARKER_FIELD_UINT16(
                            CPLSPrintf("SPqcc%d", i),
                            [](GUInt16 v)
                            {
                                return std::string(CPLSPrintf(
                                    "mantissa_b = %d, epsilon_b = %d",
                                    v & ((1 << 11) - 1), v >> 11));
                            });
                        ++i;
                    }
                }
            }
        }
        else if (abyMarker[1] == 0x5E) /* RGN */
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "RGN"))
            {
                psMarker = CreateCurrentMarker();
                if (!psMarker)
                    break;
            }
        }
        else if (abyMarker[1] == 0x5F) /* POC */
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "POC"))
            {
                psMarker = CreateCurrentMarker();
                if (!psMarker)
                    break;
                const int nPOCEntrySize = Csiz < 257 ? 7 : 9;
                int i = 0;
                while (nRemainingMarkerSize >= nPOCEntrySize)
                {
                    READ_MARKER_FIELD_UINT8(CPLSPrintf("RSpoc%d", i));
                    if (nPOCEntrySize == 7)
                    {
                        READ_MARKER_FIELD_UINT8(CPLSPrintf("CSpoc%d", i));
                    }
                    else
                    {
                        READ_MARKER_FIELD_UINT16(CPLSPrintf("CSpoc%d", i));
                    }
                    READ_MARKER_FIELD_UINT16(CPLSPrintf("LYEpoc%d", i));
                    READ_MARKER_FIELD_UINT8(CPLSPrintf("REpoc%d", i));
                    if (nPOCEntrySize == 7)
                    {
                        READ_MARKER_FIELD_UINT8(CPLSPrintf("CEpoc%d", i));
                    }
                    else
                    {
                        READ_MARKER_FIELD_UINT16(CPLSPrintf("CEpoc%d", i));
                    }
                    READ_MARKER_FIELD_UINT8(CPLSPrintf("Ppoc%d", i),
                                            lambdaPOCType);
                    i++;
                }
                if (nRemainingMarkerSize > 0)
                {
                    AddElement(
                        psMarker, psLastChild, psDumpContext,
                        CPLCreateXMLElementAndValue(
                            nullptr, "RemainingBytes",
                            CPLSPrintf(
                                "%d", static_cast<int>(nRemainingMarkerSize))));
                }
            }
        }
        else if (abyMarker[1] == 0x60) /* PPM */
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "PPM"))
            {
                psMarker = CreateCurrentMarker();
                if (!psMarker)
                    break;
            }
        }
        else if (abyMarker[1] == 0x61) /* PPT */
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "PPT"))
            {
                psMarker = CreateCurrentMarker();
                if (!psMarker)
                    break;
            }
        }
        else if (abyMarker[1] == 0x63) /* CRG */
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "CRG"))
            {
                psMarker = CreateCurrentMarker();
                if (!psMarker)
                    break;
            }
        }
        else if (abyMarker[1] == 0x64) /* COM */
        {
            if (psDumpContext->pszCodestreamMarkers == nullptr ||
                strstr(psDumpContext->pszCodestreamMarkers, "COM"))
            {
                psMarker = CreateCurrentMarker();
                if (!psMarker)
                    break;
                auto RCom = READ_MARKER_FIELD_UINT16(
                    "Rcom",
                    [](GUInt16 v) {
                        return std::string((v == 0)   ? "Binary"
                                           : (v == 1) ? "LATIN1"
                                                      : "");
                    });
                if (RCom == 1)
                {
                    GByte abyBackup = pabyMarkerDataIter[nRemainingMarkerSize];
                    pabyMarkerDataIter[nRemainingMarkerSize] = 0;
                    AddField(
                        psMarker, psLastChild, psDumpContext, "COM",
                        static_cast<int>(nRemainingMarkerSize),
                        reinterpret_cast<const char *>(pabyMarkerDataIter));
                    pabyMarkerDataIter[nRemainingMarkerSize] = abyBackup;
                }
            }
        }

        if (VSIFSeekL(fp, nOffset + 2 + nMarkerSize, SEEK_SET) != 0)
        {
            AddError(psCSBox, psLastChildCSBox, psDumpContext,
                     "Cannot seek to next marker", nOffset + 2 + nMarkerSize);
            break;
        }

        CPL_IGNORE_RET_VAL(bError);
    }
    CPLFree(pabyMarkerData);
    return psCSBox;
}

/************************************************************************/
/*                      GDALGetJPEG2000StructureInternal()              */
/************************************************************************/

static void GDALGetJPEG2000StructureInternal(CPLXMLNode *psParent, VSILFILE *fp,
                                             GDALJP2Box *poParentBox,
                                             int nRecLevel,
                                             vsi_l_offset nFileOrParentBoxSize,
                                             DumpContext *psDumpContext)
{
    // Limit recursion to a reasonable level. I believe that in practice 2
    // should be sufficient, but just in case someone creates deeply
    // nested "super-boxes", allow up to 5.
    if (nRecLevel == 5)
        return;

    static const char *const szHex = "0123456789ABCDEF";
    GDALJP2Box oBox(fp);
    oBox.SetAllowGetFileSize(psDumpContext->bAllowGetFileSize);
    CPLXMLNode *psLastChild = nullptr;
    if (oBox.ReadFirstChild(poParentBox))
    {
        while (strlen(oBox.GetType()) > 0 &&
               psDumpContext->nCurLineCount <= psDumpContext->nMaxLineCount + 1)
        {
            GIntBig nBoxDataLength = oBox.GetDataLength();
            const char *pszBoxType = oBox.GetType();
            CPLXMLNode *psBox = nullptr;
            const auto CreateBox = [&]()
            {
                if (psBox != nullptr)
                    return true;
                psBox = CPLCreateXMLNode(nullptr, CXT_Element, "JP2Box");
                psBox = AddElement(psParent, psLastChild, psDumpContext, psBox);
                if (!psBox)
                    return false;
                CPLAddXMLAttributeAndValue(psBox, "name", pszBoxType);
                CPLAddXMLAttributeAndValue(
                    psBox, "box_offset",
                    CPLSPrintf(CPL_FRMT_GIB, oBox.GetBoxOffset()));
                const auto nBoxLength = oBox.GetBoxLength();
                CPLAddXMLAttributeAndValue(
                    psBox, "box_length",
                    nBoxLength > 0 ? CPLSPrintf(CPL_FRMT_GIB, nBoxLength)
                                   : "unknown");
                CPLAddXMLAttributeAndValue(
                    psBox, "data_offset",
                    CPLSPrintf(CPL_FRMT_GIB, oBox.GetDataOffset()));
                CPLAddXMLAttributeAndValue(
                    psBox, "data_length",
                    nBoxDataLength > 0
                        ? CPLSPrintf(CPL_FRMT_GIB, nBoxDataLength)
                        : "unknown");

                if (nBoxDataLength > GINTBIG_MAX - oBox.GetDataOffset())
                {
                    CPLXMLNode *psLastChildBox = nullptr;
                    AddError(psBox, psLastChildBox, psDumpContext,
                             "Invalid box_length");
                    return false;
                }
                return true;
            };

            // Check large non-jp2c boxes against filesize
            if (strcmp(pszBoxType, "jp2c") != 0 && nBoxDataLength > 100 * 1024)
            {
                if (nFileOrParentBoxSize == 0)
                {
                    CPL_IGNORE_RET_VAL(VSIFSeekL(fp, 0, SEEK_END));
                    nFileOrParentBoxSize = VSIFTellL(fp);
                }
            }
            if (nFileOrParentBoxSize > 0 && nBoxDataLength > 0 &&
                (static_cast<vsi_l_offset>(oBox.GetDataOffset()) >
                     nFileOrParentBoxSize ||
                 static_cast<vsi_l_offset>(nBoxDataLength) >
                     nFileOrParentBoxSize - oBox.GetDataOffset()))
            {
                CPLXMLNode *psLastChildBox = nullptr;
                if (!CreateBox())
                    break;
                AddError(psBox, psLastChildBox, psDumpContext,
                         "Invalid box_length");
                break;
            }

            if (oBox.IsSuperBox())
            {
                if (!CreateBox())
                    break;
                if (nBoxDataLength <= 0)
                    break;
                GDALGetJPEG2000StructureInternal(
                    psBox, fp, &oBox, nRecLevel + 1,
                    oBox.GetDataOffset() +
                        static_cast<vsi_l_offset>(nBoxDataLength),
                    psDumpContext);
            }
            else
            {
                if (strcmp(pszBoxType, "uuid") == 0 &&
                    psDumpContext->bDumpJP2Boxes)
                {
                    if (!CreateBox())
                        break;
                    char *pszBinaryContent =
                        static_cast<char *>(VSIMalloc(2 * 16 + 1));
                    const GByte *pabyUUID = oBox.GetUUID();
                    for (int i = 0; i < 16; i++)
                    {
                        pszBinaryContent[2 * i] = szHex[pabyUUID[i] >> 4];
                        pszBinaryContent[2 * i + 1] = szHex[pabyUUID[i] & 0xf];
                    }
                    pszBinaryContent[2 * 16] = '\0';
                    CPLXMLNode *psUUIDNode =
                        CPLCreateXMLNode(nullptr, CXT_Element, "UUID");
                    if (GDALJP2Metadata::IsUUID_MSI(pabyUUID))
                        CPLAddXMLAttributeAndValue(psUUIDNode, "description",
                                                   "GeoTIFF");
                    else if (GDALJP2Metadata::IsUUID_XMP(pabyUUID))
                        CPLAddXMLAttributeAndValue(psUUIDNode, "description",
                                                   "XMP");
                    CPLCreateXMLNode(psUUIDNode, CXT_Text, pszBinaryContent);
                    VSIFree(pszBinaryContent);

                    CPLXMLNode *psLastChildBox = nullptr;
                    AddElement(psBox, psLastChildBox, psDumpContext,
                               psUUIDNode);
                }

                if (psDumpContext->bDumpBinaryContent &&
                    strcmp(pszBoxType, "jp2c") != 0 &&
                    nBoxDataLength < 100 * 1024)
                {
                    if (!CreateBox())
                        break;
                    CPLXMLNode *psBinaryContent =
                        CPLCreateXMLNode(nullptr, CXT_Element, "BinaryContent");
                    GByte *pabyBoxData = oBox.ReadBoxData();
                    int nBoxLength = static_cast<int>(nBoxDataLength);
                    char *pszBinaryContent =
                        static_cast<char *>(VSIMalloc(2 * nBoxLength + 1));
                    if (pabyBoxData && pszBinaryContent)
                    {
                        for (int i = 0; i < nBoxLength; i++)
                        {
                            pszBinaryContent[2 * i] =
                                szHex[pabyBoxData[i] >> 4];
                            pszBinaryContent[2 * i + 1] =
                                szHex[pabyBoxData[i] & 0xf];
                        }
                        pszBinaryContent[2 * nBoxLength] = '\0';
                        CPLCreateXMLNode(psBinaryContent, CXT_Text,
                                         pszBinaryContent);
                    }
                    CPLFree(pabyBoxData);
                    VSIFree(pszBinaryContent);

                    CPLXMLNode *psLastChildBox = nullptr;
                    AddElement(psBox, psLastChildBox, psDumpContext,
                               psBinaryContent);
                }

                if (psDumpContext->bDumpTextContent &&
                    strcmp(pszBoxType, "jp2c") != 0 &&
                    nBoxDataLength < 100 * 1024)
                {
                    if (!CreateBox())
                        break;
                    GByte *pabyBoxData = oBox.ReadBoxData();
                    if (pabyBoxData)
                    {
                        const char *pszBoxData =
                            reinterpret_cast<const char *>(pabyBoxData);
                        if (CPLIsUTF8(pszBoxData, -1) &&
                            static_cast<int>(strlen(pszBoxData)) + 2 >=
                                nBoxDataLength)
                        {
                            CPLXMLNode *psXMLContentBox = nullptr;
                            if (pszBoxData[0] == '<')
                            {
                                CPLPushErrorHandler(CPLQuietErrorHandler);
                                psXMLContentBox = CPLParseXMLString(pszBoxData);
                                CPLPopErrorHandler();
                            }
                            if (psXMLContentBox)
                            {
                                CPLXMLNode *psXMLContentNode = CPLCreateXMLNode(
                                    nullptr, CXT_Element, "XMLContent");
                                psXMLContentNode->psChild = psXMLContentBox;

                                CPLXMLNode *psLastChildBox = nullptr;
                                AddElement(psBox, psLastChildBox, psDumpContext,
                                           psXMLContentNode);
                            }
                            else
                            {
                                auto psTextElement = CPLCreateXMLNode(
                                    nullptr, CXT_Element, "TextContent");
                                CPLCreateXMLNode(psTextElement, CXT_Text,
                                                 pszBoxData);

                                CPLXMLNode *psLastChildBox = nullptr;
                                AddElement(psBox, psLastChildBox, psDumpContext,
                                           psTextElement);
                            }
                        }
                    }
                    CPLFree(pabyBoxData);
                }

                if (strcmp(pszBoxType, "jp2c") == 0)
                {
                    if (psDumpContext->bDumpCodestream ||
                        psDumpContext->pszCodestreamMarkers)
                    {
                        if (!CreateBox())
                            break;
                        DumpJPK2CodeStream(psBox, fp, oBox.GetDataOffset(),
                                           nBoxDataLength, psDumpContext);
                        if (psDumpContext->bStopAtSOD &&
                            psDumpContext->bSODEncountered)
                        {
                            break;
                        }
                    }
                }
                else if (!psDumpContext->bDumpJP2Boxes)
                {
                    // do nothing
                }
                else if (strcmp(pszBoxType, "uuid") == 0 &&
                         GDALJP2Metadata::IsUUID_MSI(oBox.GetUUID()))
                {
                    if (!CreateBox())
                        break;
                    DumpGeoTIFFBox(psBox, oBox, psDumpContext);
                }
                else if (strcmp(pszBoxType, "ftyp") == 0)
                {
                    if (!CreateBox())
                        break;
                    DumpFTYPBox(psBox, oBox, psDumpContext);
                }
                else if (strcmp(pszBoxType, "ihdr") == 0)
                {
                    if (!CreateBox())
                        break;
                    DumpIHDRBox(psBox, oBox, psDumpContext);
                }
                else if (strcmp(pszBoxType, "bpcc") == 0)
                {
                    if (!CreateBox())
                        break;
                    DumpBPCCBox(psBox, oBox, psDumpContext);
                }
                else if (strcmp(pszBoxType, "colr") == 0)
                {
                    if (!CreateBox())
                        break;
                    DumpCOLRBox(psBox, oBox, psDumpContext);
                }
                else if (strcmp(pszBoxType, "pclr") == 0)
                {
                    if (!CreateBox())
                        break;
                    DumpPCLRBox(psBox, oBox, psDumpContext);
                }
                else if (strcmp(pszBoxType, "cmap") == 0)
                {
                    if (!CreateBox())
                        break;
                    DumpCMAPBox(psBox, oBox, psDumpContext);
                }
                else if (strcmp(pszBoxType, "cdef") == 0)
                {
                    if (!CreateBox())
                        break;
                    DumpCDEFBox(psBox, oBox, psDumpContext);
                }
                else if (strcmp(pszBoxType, "resc") == 0 ||
                         strcmp(pszBoxType, "resd") == 0)
                {
                    if (!CreateBox())
                        break;
                    DumpRESxBox(psBox, oBox, psDumpContext);
                }
                else if (strcmp(pszBoxType, "rreq") == 0)
                {
                    if (!CreateBox())
                        break;
                    DumpRREQBox(psBox, oBox, psDumpContext);
                }
            }

            if (!oBox.ReadNextChild(poParentBox))
                break;
        }
    }
}

/************************************************************************/
/*                        GDALGetJPEG2000Structure()                    */
/************************************************************************/

constexpr unsigned char jpc_header[] = {0xff, 0x4f};
constexpr unsigned char jp2_box_jp[] = {0x6a, 0x50, 0x20, 0x20}; /* 'jP  ' */

/** Dump the structure of a JPEG2000 file as a XML tree.
 *
 * @param pszFilename filename.
 * @param papszOptions NULL terminated list of options, or NULL.
 *                     Allowed options are BINARY_CONTENT=YES, TEXT_CONTENT=YES,
 *                     CODESTREAM=YES, ALL=YES, JP2_BOXES=YES,
 *                     CODESTREAM_MARKERS=list_of_marker_names_comma_separated,
 *                     STOP_AT_SOD=YES, ALLOW_GET_FILE_SIZE=NO.
 * @return XML tree (to be freed with CPLDestroyXMLNode()) or NULL in case
 *         of error
 * @since GDAL 2.0
 */

CPLXMLNode *GDALGetJPEG2000Structure(const char *pszFilename,
                                     CSLConstList papszOptions)
{
    VSILFILE *fp = VSIFOpenL(pszFilename, "rb");
    if (fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s", pszFilename);
        return nullptr;
    }
    auto psRet = GDALGetJPEG2000Structure(pszFilename, fp, papszOptions);
    CPL_IGNORE_RET_VAL(VSIFCloseL(fp));
    return psRet;
}

#ifndef DOXYGEN_SKIP

/************************************************************************/
/*                        GDALGetJPEG2000Structure()                    */
/************************************************************************/

CPLXMLNode *GDALGetJPEG2000Structure(const char *pszFilename, VSILFILE *fp,
                                     CSLConstList papszOptions)
{
    if (fp == nullptr)
        return GDALGetJPEG2000Structure(pszFilename, papszOptions);

    GByte abyHeader[16];
    if (VSIFSeekL(fp, 0, SEEK_SET) != 0 ||
        VSIFReadL(abyHeader, 16, 1, fp) != 1 ||
        (memcmp(abyHeader, jpc_header, sizeof(jpc_header)) != 0 &&
         memcmp(abyHeader + 4, jp2_box_jp, sizeof(jp2_box_jp)) != 0))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s is not a JPEG2000 file",
                 pszFilename);
        return nullptr;
    }

    CPLXMLNode *psParent = nullptr;
    DumpContext dc;
    dc.nCurLineCount = 0;
    dc.nMaxLineCount = atoi(CSLFetchNameValueDef(
        papszOptions, "MAX_LINES",
        CPLGetConfigOption("GDAL_JPEG2000_STRUCTURE_MAX_LINES", "500000")));
    if (dc.nMaxLineCount > INT_MAX - 1)
        dc.nMaxLineCount = INT_MAX - 1;
    dc.bDumpAll = CPLFetchBool(papszOptions, "ALL", false);
    dc.bDumpCodestream =
        dc.bDumpAll || CPLFetchBool(papszOptions, "CODESTREAM", false);
    dc.bDumpBinaryContent =
        dc.bDumpAll || CPLFetchBool(papszOptions, "BINARY_CONTENT", false);
    dc.bDumpTextContent =
        dc.bDumpAll || CPLFetchBool(papszOptions, "TEXT_CONTENT", false);
    dc.pszCodestreamMarkers =
        CSLFetchNameValue(papszOptions, "CODESTREAM_MARKERS");
    dc.bDumpJP2Boxes = dc.bDumpAll ||
                       CPLFetchBool(papszOptions, "JP2_BOXES", false) ||
                       dc.pszCodestreamMarkers == nullptr;
    dc.bStopAtSOD = CPLFetchBool(papszOptions, "STOP_AT_SOD", false);
    dc.bAllowGetFileSize =
        CPLFetchBool(papszOptions, "ALLOW_GET_FILE_SIZE", true);

    if (memcmp(abyHeader, jpc_header, sizeof(jpc_header)) == 0)
    {
        if (dc.bDumpCodestream || dc.pszCodestreamMarkers != nullptr)
        {
            GIntBig nBoxDataLength = -1;
            if (dc.bAllowGetFileSize && VSIFSeekL(fp, 0, SEEK_END) == 0)
            {
                nBoxDataLength = static_cast<GIntBig>(VSIFTellL(fp));
            }
            psParent = DumpJPK2CodeStream(nullptr, fp, 0, nBoxDataLength, &dc);
            CPLAddXMLAttributeAndValue(psParent, "filename", pszFilename);
        }
    }
    else
    {
        psParent = CPLCreateXMLNode(nullptr, CXT_Element, "JP2File");
        CPLAddXMLAttributeAndValue(psParent, "filename", pszFilename);
        vsi_l_offset nFileSize = 0;
        GDALGetJPEG2000StructureInternal(psParent, fp, nullptr, 0, nFileSize,
                                         &dc);
    }

    if (dc.nCurLineCount > dc.nMaxLineCount)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Maximum number of lines in JPEG2000 structure dump reached. "
                 "Increase GDAL_JPEG2000_STRUCTURE_MAX_LINES beyond %d.",
                 dc.nMaxLineCount);
    }

    return psParent;
}

/************************************************************************/
/*                     GDALGetJPEG2000Reversibility()                   */
/************************************************************************/

const char *GDALGetJPEG2000Reversibility(const char *pszFilename, VSILFILE *fp)
{
    const char *const apszOptions[] = {"ALLOW_GET_FILE_SIZE=NO",
                                       "STOP_AT_SOD=YES",
                                       "CODESTREAM_MARKERS=COD,COM", nullptr};
    CPLXMLNode *psRes = GDALGetJPEG2000Structure(pszFilename, fp, apszOptions);
    if (psRes == nullptr)
        return nullptr;
    const char *pszReversibility = nullptr;
    const CPLXMLNode *psJP2C = CPLSearchXMLNode(psRes, "JP2KCodeStream");
    if (psJP2C)
    {
        const char *pszTransformation = nullptr;
        const char *pszCOM = nullptr;
        for (const CPLXMLNode *psMarker = psJP2C->psChild; psMarker;
             psMarker = psMarker->psNext)
        {
            if (psMarker->eType == CXT_Element &&
                strcmp(psMarker->pszValue, "Marker") == 0 &&
                strcmp(CPLGetXMLValue(psMarker, "name", ""), "COD") == 0)
            {
                for (const CPLXMLNode *psField = psMarker->psChild; psField;
                     psField = psField->psNext)
                {
                    if (psField->eType == CXT_Element &&
                        strcmp(psField->pszValue, "Field") == 0 &&
                        strcmp(CPLGetXMLValue(psField, "name", ""),
                               "SPcod_transformation") == 0)
                    {
                        pszTransformation =
                            CPLGetXMLValue(psField, nullptr, nullptr);
                        break;
                    }
                }
            }
            else if (psMarker->eType == CXT_Element &&
                     strcmp(psMarker->pszValue, "Marker") == 0 &&
                     strcmp(CPLGetXMLValue(psMarker, "name", ""), "COM") == 0)
            {
                for (const CPLXMLNode *psField = psMarker->psChild; psField;
                     psField = psField->psNext)
                {
                    if (psField->eType == CXT_Element &&
                        strcmp(psField->pszValue, "Field") == 0 &&
                        strcmp(CPLGetXMLValue(psField, "name", ""), "COM") == 0)
                    {
                        pszCOM = CPLGetXMLValue(psField, nullptr, nullptr);
                        break;
                    }
                }
            }
        }

        if (pszTransformation != nullptr &&
            strcmp(pszTransformation, "0") ==
                0)  // 0 = 9x7 irreversible wavelet
        {
            pszReversibility = "LOSSY";
        }
        else if (pszTransformation != nullptr &&
                 strcmp(pszTransformation, "1") ==
                     0)  // 1 = 5x3 reversible wavelet
        {
            // 5x3 wavelet by itself doesn't guarantee full lossless mode
            // if quality layers are discarded. hence the "possibly"
            pszReversibility = "LOSSLESS (possibly)";

            if (pszCOM &&
                STARTS_WITH(
                    pszCOM,
                    "Kdu-Layer-Info: "
                    "log_2{Delta-D(squared-error)/Delta-L(bytes)}, L(bytes)"))
            {
                if (strstr(pszCOM, "-192.0,") != nullptr)
                {
                    // Not really sure to understand this fully, but
                    // experimentaly I've found that if the last row in the
                    // Kdu-Layer-Info includes a line starting with "-192.0", it
                    // means that the last layer includes everything to be
                    // lossless.
                    pszReversibility = "LOSSLESS";
                }
                else
                {
                    pszReversibility = "LOSSY";
                }
            }
            // Kakadu < 6.4
            else if (pszCOM &&
                     STARTS_WITH(
                         pszCOM,
                         "Kdu-Layer-Info: "
                         "log_2{Delta-D(MSE)/[2^16*Delta-L(bytes)]}, L(bytes)"))
            {
                if (strstr(pszCOM, "-256.0,") != nullptr)
                {
                    // Not really sure to understand this fully, but
                    // experimentaly I've found that if the last row in the
                    // Kdu-Layer-Info includes a line starting with "-256.0", it
                    // means that the last layer includes everything to be
                    // lossless.
                    pszReversibility = "LOSSLESS";
                }
                else
                {
                    pszReversibility = "LOSSY";
                }
            }
            else if (pszCOM && STARTS_WITH(pszCOM, "Created by OpenJPEG"))
            {
                // Starting with GDAL 3.6, the JP2OpenJPEG driver will write
                // if the encoding parameters are lossless/lossy (for 5x3
                // wavelets)
                if (strstr(pszCOM, "LOSSLESS settings used"))
                {
                    pszReversibility = "LOSSLESS";
                }
                else if (strstr(pszCOM, "LOSSY settings used"))
                {
                    pszReversibility = "LOSSY";
                }
            }
        }
    }
    CPLDestroyXMLNode(psRes);
    return pszReversibility;
}

#endif /* #ifndef DOXYGEN_SKIP */
