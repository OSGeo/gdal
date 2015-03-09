/******************************************************************************
 * $Id$
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

#include "gdaljp2metadata.h"

static void AddField(CPLXMLNode* psParent, const char* pszFieldName,
                     int nFieldSize, const char* pszValue,
                     const char* pszDescription = NULL)
{
    CPLXMLNode* psField = CPLCreateXMLElementAndValue(
                                    psParent, "Field", pszValue );
    CPLAddXMLAttributeAndValue(psField, "name", pszFieldName );
    CPLAddXMLAttributeAndValue(psField, "type", "string" );
    CPLAddXMLAttributeAndValue(psField, "size", CPLSPrintf("%d", nFieldSize )  );
    if( pszDescription )
        CPLAddXMLAttributeAndValue(psField, "description", pszDescription );
}

static void AddHexField(CPLXMLNode* psParent, const char* pszFieldName,
                        int nFieldSize, const char* pszValue,
                        const char* pszDescription = NULL)
{
    CPLXMLNode* psField = CPLCreateXMLElementAndValue(
                                    psParent, "Field", pszValue );
    CPLAddXMLAttributeAndValue(psField, "name", pszFieldName );
    CPLAddXMLAttributeAndValue(psField, "type", "hexint" );
    CPLAddXMLAttributeAndValue(psField, "size", CPLSPrintf("%d", nFieldSize )  );
    if( pszDescription )
        CPLAddXMLAttributeAndValue(psField, "description", pszDescription );
}

static void AddField(CPLXMLNode* psParent, const char* pszFieldName, GByte nVal,
                     const char* pszDescription = NULL)
{
    CPLXMLNode* psField = CPLCreateXMLElementAndValue(
                                psParent, "Field", CPLSPrintf("%d", nVal) );
    CPLAddXMLAttributeAndValue(psField, "name", pszFieldName );
    CPLAddXMLAttributeAndValue(psField, "type", "uint8" );
    if( pszDescription )
        CPLAddXMLAttributeAndValue(psField, "description", pszDescription );
}

static void AddField(CPLXMLNode* psParent, const char* pszFieldName, GUInt16 nVal,
                     const char* pszDescription = NULL)
{
    CPLXMLNode* psField = CPLCreateXMLElementAndValue(
                                psParent, "Field", CPLSPrintf("%d", nVal) );
    CPLAddXMLAttributeAndValue(psField, "name", pszFieldName );
    CPLAddXMLAttributeAndValue(psField, "type", "uint16" );
    if( pszDescription )
        CPLAddXMLAttributeAndValue(psField, "description", pszDescription );
}

static void AddField(CPLXMLNode* psParent, const char* pszFieldName, GUInt32 nVal,
                     const char* pszDescription = NULL)
{
    CPLXMLNode* psField = CPLCreateXMLElementAndValue(
                                psParent, "Field", CPLSPrintf("%u", nVal) );
    CPLAddXMLAttributeAndValue(psField, "name", pszFieldName );
    CPLAddXMLAttributeAndValue(psField, "type", "uint32" );
    if( pszDescription )
        CPLAddXMLAttributeAndValue(psField, "description", pszDescription );
}

static const char* GetInterpretationOfBPC(GByte bpc)
{
    if( bpc == 255 )
        return NULL;
    if( (bpc & 0x80) )
        return CPLSPrintf("Signed %d bits", 1 + (bpc & 0x7F));
    else
        return CPLSPrintf("Unsigned %d bits", 1 + bpc);
}

static const char* GetStandardFieldString(GUInt16 nVal)
{
    switch(nVal)
    {
        case 1: return "Codestream contains no extensions";
        case 2: return "Contains multiple composition layers";
        case 3: return "Codestream is compressed using JPEG 2000 and requires at least a Profile 0 decoder";
        case 4: return "Codestream is compressed using JPEG 2000 and requires at least a Profile 1 decoder";
        case 5: return "Codestream is compressed using JPEG 2000 unrestricted";
        case 35: return "Contains IPR metadata";
        case 67: return "Contains GMLJP2 metadata";
        default: return NULL;
    }
}

static void DumpGeoTIFFBox(CPLXMLNode* psBox,
                           GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    GDALDriver* poVRTDriver = (GDALDriver*) GDALGetDriverByName("VRT");
    if( pabyBoxData && poVRTDriver)
    {
        CPLString osTmpFilename(CPLSPrintf("/vsimem/tmp_%p.tif", oBox.GetFILE()));
        VSIFCloseL(VSIFileFromMemBuffer(
            osTmpFilename, pabyBoxData, nBoxDataLength, TRUE) );
        CPLPushErrorHandler(CPLQuietErrorHandler);
        GDALDataset* poDS = (GDALDataset*) GDALOpen(osTmpFilename, GA_ReadOnly);
        CPLPopErrorHandler();
        if( poDS )
        {
            CPLString osTmpVRTFilename(CPLSPrintf("/vsimem/tmp_%p.vrt", oBox.GetFILE()));
            GDALDataset* poVRTDS = poVRTDriver->CreateCopy(osTmpVRTFilename, poDS, FALSE, NULL, NULL, NULL);
            GDALClose(poVRTDS);
            GByte* pabyXML = VSIGetMemFileBuffer( osTmpVRTFilename, NULL, FALSE );
            CPLXMLNode* psXMLVRT = CPLParseXMLString((const char*)pabyXML);
            if( psXMLVRT )
            {
                CPLXMLNode* psXMLContentNode = 
                    CPLCreateXMLNode( psBox, CXT_Element, "DecodedGeoTIFF" );
                psXMLContentNode->psChild = psXMLVRT;
                CPLXMLNode* psPrev = NULL;
                for(CPLXMLNode* psIter = psXMLVRT->psChild; psIter; psIter = psIter->psNext)
                {
                    if( psIter->eType == CXT_Element &&
                        strcmp(psIter->pszValue, "VRTRasterBand") == 0 )
                    {
                        CPLXMLNode* psNext = psIter->psNext;
                        psIter->psNext = NULL;
                        CPLDestroyXMLNode(psIter);
                        if( psPrev )
                            psPrev->psNext = psNext;
                        else
                            break;
                        psIter = psPrev;
                    }
                    psPrev = psIter;
                }
            }

            VSIUnlink(osTmpVRTFilename);
            GDALClose(poDS);
        }
        VSIUnlink(osTmpFilename);
    }
}

static void DumpFTYPBox(CPLXMLNode* psBox, GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    if( pabyBoxData )
    {
        CPLXMLNode* psDecodedContent = 
            CPLCreateXMLNode( psBox, CXT_Element, "DecodedContent" );
        GIntBig nRemainingLength = nBoxDataLength;
        GByte* pabyIter = pabyBoxData;
        if( nRemainingLength >= 4 )
        {
            char szBranding[5];
            memcpy(szBranding, pabyIter, 4);
            szBranding[4] = 0;
            AddField(psDecodedContent, "BR", 4, szBranding);
            pabyIter += 4;
            nRemainingLength -= 4;
        }
        if( nRemainingLength >= 4 )
        {
            GUInt32 nVal;
            memcpy(&nVal, pabyIter, 4);
            CPL_MSBPTR32(&nVal);
            AddField(psDecodedContent, "MinV", nVal);
            pabyIter += 4;
            nRemainingLength -= 4;
        }
        int nCLIndex = 0;
        while( nRemainingLength >= 4 )
        {
            char szBranding[5];
            memcpy(szBranding, pabyIter, 4);
            szBranding[4] = 0;
            AddField(psDecodedContent,
                        CPLSPrintf("CL%d", nCLIndex),
                        4, szBranding);
            pabyIter += 4;
            nRemainingLength -= 4;
            nCLIndex ++;
        }
        if( nRemainingLength > 0 )
            CPLCreateXMLElementAndValue(
                    psDecodedContent, "RemainingBytes",
                    CPLSPrintf("%d", (int)nRemainingLength ));
    }
    CPLFree(pabyBoxData);
}

static void DumpIHDRBox(CPLXMLNode* psBox, GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    if( pabyBoxData )
    {
        CPLXMLNode* psDecodedContent = 
            CPLCreateXMLNode( psBox, CXT_Element, "DecodedContent" );
        GIntBig nRemainingLength = nBoxDataLength;
        GByte* pabyIter = pabyBoxData;
        if( nRemainingLength >= 4 )
        {
            GUInt32 nVal;
            memcpy(&nVal, pabyIter, 4);
            CPL_MSBPTR32(&nVal);
            AddField(psDecodedContent, "HEIGHT", nVal);
            pabyIter += 4;
            nRemainingLength -= 4;
        }
        if( nRemainingLength >= 4 )
        {
            GUInt32 nVal;
            memcpy(&nVal, pabyIter, 4);
            CPL_MSBPTR32(&nVal);
            AddField(psDecodedContent, "WIDTH", nVal);
            pabyIter += 4;
            nRemainingLength -= 4;
        }
        if( nRemainingLength >= 2 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            AddField(psDecodedContent, "NC", nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        if( nRemainingLength >= 1 )
        {
            AddField(psDecodedContent, "BPC", *pabyIter,
                        GetInterpretationOfBPC(*pabyIter));
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength >= 1 )
        {
            AddField(psDecodedContent, "C", *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength >= 1 )
        {
            AddField(psDecodedContent, "UnkC", *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength >= 1 )
        {
            AddField(psDecodedContent, "IPR", *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength > 0 )
            CPLCreateXMLElementAndValue(
                    psDecodedContent, "RemainingBytes",
                    CPLSPrintf("%d", (int)nRemainingLength ));
    }
    CPLFree(pabyBoxData);
}

static void DumpBPCCBox(CPLXMLNode* psBox, GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    if( pabyBoxData )
    {
        CPLXMLNode* psDecodedContent = 
            CPLCreateXMLNode( psBox, CXT_Element, "DecodedContent" );
        GIntBig nRemainingLength = nBoxDataLength;
        GByte* pabyIter = pabyBoxData;
        int nBPCIndex = 0;
        while( nRemainingLength >= 1 )
        {
            AddField(psDecodedContent,
                        CPLSPrintf("BPC%d", nBPCIndex),
                        *pabyIter,
                        GetInterpretationOfBPC(*pabyIter));
            nBPCIndex ++;
            pabyIter += 1;
            nRemainingLength -= 1;
        }
    }
    CPLFree(pabyBoxData);
}

static void DumpCOLRBox(CPLXMLNode* psBox, GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    if( pabyBoxData )
    {
        CPLXMLNode* psDecodedContent = 
            CPLCreateXMLNode( psBox, CXT_Element, "DecodedContent" );
        GIntBig nRemainingLength = nBoxDataLength;
        GByte* pabyIter = pabyBoxData;
        GByte nMeth;
        if( nRemainingLength >= 1 )
        {
            nMeth = *pabyIter;
            AddField(psDecodedContent, "METH", nMeth,
                        (nMeth == 0) ? "Enumerated Colourspace":
                        (nMeth == 0) ? "Restricted ICC profile": NULL);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength >= 1 )
        {
            AddField(psDecodedContent, "PREC", *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength >= 1 )
        {
            AddField(psDecodedContent, "APPROX", *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength >= 4 )
        {
            GUInt32 nVal;
            memcpy(&nVal, pabyIter, 4);
            CPL_MSBPTR32(&nVal);
            AddField(psDecodedContent, "EnumCS", nVal,
                        (nVal == 16) ? "sRGB" :
                        (nVal == 17) ? "greyscale":
                        (nVal == 18) ? "sYCC" : NULL);
            pabyIter += 4;
            nRemainingLength -= 4;
        }
        if( nRemainingLength > 0 )
            CPLCreateXMLElementAndValue(
                    psDecodedContent, "RemainingBytes",
                    CPLSPrintf("%d", (int)nRemainingLength ));
    }
    CPLFree(pabyBoxData);
}

static void DumpPCLRBox(CPLXMLNode* psBox, GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    if( pabyBoxData )
    {
        CPLXMLNode* psDecodedContent = 
            CPLCreateXMLNode( psBox, CXT_Element, "DecodedContent" );
        GIntBig nRemainingLength = nBoxDataLength;
        GByte* pabyIter = pabyBoxData;
        GUInt16 NE;
        if( nRemainingLength >= 2 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            NE = nVal;
            AddField(psDecodedContent, "NE", nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        GByte NPC;
        if( nRemainingLength >= 1 )
        {
            NPC = *pabyIter;
            AddField(psDecodedContent, "NPC", NPC);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        int b8BitOnly = TRUE;
        for(int i=0;i<NPC;i++)
        {
            if( nRemainingLength >= 1 )
            {
                b8BitOnly &= (*pabyIter == 7);
                AddField(psDecodedContent,
                            CPLSPrintf("B%d", i),
                            *pabyIter,
                            GetInterpretationOfBPC(*pabyIter));
                pabyIter += 1;
                nRemainingLength -= 1;
            }
        }
        if( b8BitOnly )
        {
            for(int j=0;j<NE;j++)
            {
                for(int i=0;i<NPC;i++)
                {
                    if( nRemainingLength >= 1 )
                    {
                        AddField(psDecodedContent,
                                CPLSPrintf("C_%d_%d", j, i),
                                *pabyIter);
                        pabyIter += 1;
                        nRemainingLength -= 1;
                    }
                }
            }
        }
        if( nRemainingLength > 0 )
            CPLCreateXMLElementAndValue(
                    psDecodedContent, "RemainingBytes",
                    CPLSPrintf("%d", (int)nRemainingLength ));
    }
    CPLFree(pabyBoxData);
}

static void DumpCMAPBox(CPLXMLNode* psBox, GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    if( pabyBoxData )
    {
        CPLXMLNode* psDecodedContent = 
            CPLCreateXMLNode( psBox, CXT_Element, "DecodedContent" );
        GIntBig nRemainingLength = nBoxDataLength;
        GByte* pabyIter = pabyBoxData;
        int nIndex = 0;
        while( nRemainingLength >= 2 + 1 + 1 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            AddField(psDecodedContent,
                        CPLSPrintf("CMP%d", nIndex),
                        nVal);
            pabyIter += 2;
            nRemainingLength -= 2;

            AddField(psDecodedContent,
                        CPLSPrintf("MTYP%d", nIndex),
                        *pabyIter,
                        (*pabyIter == 0) ? "Direct use":
                        (*pabyIter == 1) ? "Palette mapping": NULL);
            pabyIter += 1;
            nRemainingLength -= 1;

            AddField(psDecodedContent,
                        CPLSPrintf("PCOL%d", nIndex),
                        *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;

            nIndex ++;
        }
        if( nRemainingLength > 0 )
            CPLCreateXMLElementAndValue(
                    psDecodedContent, "RemainingBytes",
                    CPLSPrintf("%d", (int)nRemainingLength ));
    }
    CPLFree(pabyBoxData);
}

static void DumpCDEFBox(CPLXMLNode* psBox, GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    if( pabyBoxData )
    {
        CPLXMLNode* psDecodedContent = 
            CPLCreateXMLNode( psBox, CXT_Element, "DecodedContent" );
        GIntBig nRemainingLength = nBoxDataLength;
        GByte* pabyIter = pabyBoxData;
        GUInt16 nChannels;
        if( nRemainingLength >= 2 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            nChannels = nVal;
            CPL_MSBPTR16(&nVal);
            AddField(psDecodedContent, "N", nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        for( int i=0; i < nChannels; i++ )
        {
            if( nRemainingLength >= 2 )
            {
                GUInt16 nVal;
                memcpy(&nVal, pabyIter, 2);
                CPL_MSBPTR16(&nVal);
                AddField(psDecodedContent,
                            CPLSPrintf("Cn%d", i),
                            nVal);
                pabyIter += 2;
                nRemainingLength -= 2;
            }
            if( nRemainingLength >= 2 )
            {
                GUInt16 nVal;
                memcpy(&nVal, pabyIter, 2);
                CPL_MSBPTR16(&nVal);
                AddField(psDecodedContent,
                            CPLSPrintf("Typ%d", i),
                            nVal,
                            (nVal == 0) ? "Colour channel":
                            (nVal == 1) ? "Opacity channel":
                            (nVal == 2) ? "Premultiplied opacity":
                            (nVal == 65535) ? "Not specified" : NULL);
                pabyIter += 2;
                nRemainingLength -= 2;
            }
            if( nRemainingLength >= 2 )
            {
                GUInt16 nVal;
                memcpy(&nVal, pabyIter, 2);
                CPL_MSBPTR16(&nVal);
                AddField(psDecodedContent,
                            CPLSPrintf("Assoc%d", i),
                            nVal,
                            (nVal == 0) ? "Associated to the whole image":
                            (nVal == 65535) ? "Not associated with a particular colour":
                            "Associated with a particular colour");
                pabyIter += 2;
                nRemainingLength -= 2;
            }
        }
        if( nRemainingLength > 0 )
            CPLCreateXMLElementAndValue(
                    psDecodedContent, "RemainingBytes",
                    CPLSPrintf("%d", (int)nRemainingLength ));
    }
    CPLFree(pabyBoxData);
}

static void DumpRESxBox(CPLXMLNode* psBox, GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    char chC = oBox.GetType()[3];
    if( pabyBoxData )
    {
        CPLXMLNode* psDecodedContent = 
            CPLCreateXMLNode( psBox, CXT_Element, "DecodedContent" );
        GIntBig nRemainingLength = nBoxDataLength;
        GByte* pabyIter = pabyBoxData;
        GUInt16 nNumV = 0, nNumH = 0, nDenomV = 1, nDenomH = 1, nExpV = 0, nExpH = 0;
        if( nRemainingLength >= 2 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            nNumV = nVal;
            AddField(psDecodedContent, CPLSPrintf("VR%cN", chC), nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        if( nRemainingLength >= 2 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            nDenomV = nVal;
            AddField(psDecodedContent, CPLSPrintf("VR%cD", chC), nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        if( nRemainingLength >= 2 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            nNumH = nVal;
            AddField(psDecodedContent, CPLSPrintf("HR%cN", chC), nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        if( nRemainingLength >= 2 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            nDenomH = nVal;
            AddField(psDecodedContent, CPLSPrintf("HR%cD", chC), nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        if( nRemainingLength >= 1 )
        {
            AddField(psDecodedContent, CPLSPrintf("VR%cE", chC), *pabyIter);
            nExpV = *pabyIter;
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength >= 1 )
        {
            AddField(psDecodedContent, CPLSPrintf("HR%cE", chC), *pabyIter);
            nExpH = *pabyIter;
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength == 0 )
        {
            CPLCreateXMLElementAndValue(psDecodedContent, "VRes",
                CPLSPrintf("%.03f", 1.0 * nNumV / nDenomV * pow(10.0, nExpV)));
            CPLCreateXMLElementAndValue(psDecodedContent, "HRes",
                CPLSPrintf("%.03f", 1.0 * nNumH / nDenomH * pow(10.0, nExpH)));
        }
        else if( nRemainingLength > 0 )
            CPLCreateXMLElementAndValue(
                    psDecodedContent, "RemainingBytes",
                    CPLSPrintf("%d", (int)nRemainingLength ));
    }
    CPLFree(pabyBoxData);
}

static void DumpRREQBox(CPLXMLNode* psBox, GDALJP2Box& oBox)
{
    GIntBig nBoxDataLength = oBox.GetDataLength();
    GByte* pabyBoxData = oBox.ReadBoxData();
    if( pabyBoxData )
    {
        CPLXMLNode* psDecodedContent = 
            CPLCreateXMLNode( psBox, CXT_Element, "DecodedContent" );
        GIntBig nRemainingLength = nBoxDataLength;
        GByte* pabyIter = pabyBoxData;
        GByte ML = 0;
        if( nRemainingLength >= 1 )
        {
            ML = *pabyIter;
            AddField(psDecodedContent, "ML", *pabyIter);
            pabyIter += 1;
            nRemainingLength -= 1;
        }
        if( nRemainingLength >= ML )
        {
            CPLString osHex("0x");
            for(int i=0;i<ML;i++)
            {
                osHex += CPLSPrintf("%02X", *pabyIter);
                pabyIter += 1;
                nRemainingLength -= 1;
            }
            AddHexField(psDecodedContent, "FUAM", (int)ML, osHex.c_str());
        }
        if( nRemainingLength >= ML )
        {
            CPLString osHex("0x");
            for(int i=0;i<ML;i++)
            {
                osHex += CPLSPrintf("%02X", *pabyIter);
                pabyIter += 1;
                nRemainingLength -= 1;
            }
            AddHexField(psDecodedContent, "DCM", (int)ML, osHex.c_str());
        }
        GUInt16 NSF = 0;
        if( nRemainingLength >= 2 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            NSF = nVal;
            AddField(psDecodedContent, "NSF", nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        for(int iNSF=0;iNSF<NSF;iNSF++)
        {
            if( nRemainingLength >= 2 )
            {
                GUInt16 nVal;
                memcpy(&nVal, pabyIter, 2);
                CPL_MSBPTR16(&nVal);
                AddField(psDecodedContent,
                            CPLSPrintf("SF%d", iNSF), nVal,
                            GetStandardFieldString(nVal));
                pabyIter += 2;
                nRemainingLength -= 2;
            }
            if( nRemainingLength >= ML )
            {
                CPLString osHex("0x");
                for(int i=0;i<ML;i++)
                {
                    osHex += CPLSPrintf("%02X", *pabyIter);
                    pabyIter += 1;
                    nRemainingLength -= 1;
                }
                AddHexField(psDecodedContent,
                            CPLSPrintf("SM%d", iNSF),
                            (int)ML, osHex.c_str());
            }
        }
        GUInt16 NVF = 0;
        if( nRemainingLength >= 2 )
        {
            GUInt16 nVal;
            memcpy(&nVal, pabyIter, 2);
            CPL_MSBPTR16(&nVal);
            NVF = nVal;
            AddField(psDecodedContent, "NVF", nVal);
            pabyIter += 2;
            nRemainingLength -= 2;
        }
        for(int iNVF=0;iNVF<NVF;iNVF++)
        {
            if( nRemainingLength >= 16 )
            {
                CPLString osHex("0x");
                for(int i=0;i<16;i++)
                {
                    osHex += CPLSPrintf("%02X", *pabyIter);
                    pabyIter += 1;
                    nRemainingLength -= 1;
                }
                AddHexField(psDecodedContent,
                            CPLSPrintf("VF%d", iNVF),
                            (int)ML, osHex.c_str());
            }
            if( nRemainingLength >= ML )
            {
                CPLString osHex("0x");
                for(int i=0;i<ML;i++)
                {
                    osHex += CPLSPrintf("%02X", *pabyIter);
                    pabyIter += 1;
                    nRemainingLength -= 1;
                }
                AddHexField(psDecodedContent,
                            CPLSPrintf("VM%d", iNVF),
                            (int)ML, osHex.c_str());
            }
        }
        if( nRemainingLength > 0 )
            CPLCreateXMLElementAndValue(
                    psDecodedContent, "RemainingBytes",
                    CPLSPrintf("%d", (int)nRemainingLength ));
    }
    CPLFree(pabyBoxData);
}

static CPLXMLNode* CreateMarker(CPLXMLNode* psCSBox, const char* pszName,
                                GIntBig nOffset, GIntBig nLength)
{
    CPLXMLNode* psMarker = CPLCreateXMLNode( psCSBox, CXT_Element, "Marker" );
    CPLAddXMLAttributeAndValue(psMarker, "name", pszName );
    CPLAddXMLAttributeAndValue(psMarker, "offset",
                               CPLSPrintf(CPL_FRMT_GIB, nOffset )  );
    CPLAddXMLAttributeAndValue(psMarker, "length",
                               CPLSPrintf(CPL_FRMT_GIB, 2 + nLength ) );
    return psMarker;
}

static void AddError(CPLXMLNode* psParent, const char* pszErrorMsg,
                     GIntBig nOffset = 0)
{
    CPLXMLNode* psError = CPLCreateXMLNode( psParent, CXT_Element, "Error" );
    CPLAddXMLAttributeAndValue(psError, "message", pszErrorMsg );
    if( nOffset )
    {
        CPLAddXMLAttributeAndValue(psError, "offset",
                                CPLSPrintf(CPL_FRMT_GIB, nOffset )  );
    }
}

/************************************************************************/
/*                       DumpJPK2CodeStream()                           */
/************************************************************************/

static CPLXMLNode* DumpJPK2CodeStream(CPLXMLNode* psBox,
                                      VSILFILE* fp,
                                      GIntBig nBoxDataOffset,
                                      GIntBig nBoxDataLength)
{
    VSIFSeekL(fp, nBoxDataOffset, SEEK_SET);
    GByte abyMarker[2];
    CPLXMLNode* psCSBox = CPLCreateXMLNode( psBox, CXT_Element, "JP2KCodeStream" );
    GByte* pabyMarkerData = (GByte*)CPLMalloc(65535+1);
    GIntBig nNextTileOffset = 0;
    while( TRUE )
    {
        GIntBig nOffset = (GIntBig)VSIFTellL(fp);
        if( nOffset == nBoxDataOffset + nBoxDataLength )
            break;
        if( VSIFReadL(abyMarker, 2, 1, fp) != 1 )
        {
            AddError(psCSBox, "Cannot read marker", nOffset);
            break;
        }
        if( abyMarker[0] != 0xFF )
        {
            AddError(psCSBox, "Not a marker", nOffset);
            break;
        }
        if( abyMarker[1] == 0x4F )
        {
            CreateMarker( psCSBox, "SOC", nOffset, 0 );
            continue;
        }
        if( abyMarker[1] == 0x93 )
        {
            GIntBig nMarkerSize = 0;
            if( nNextTileOffset == 0 )
                nMarkerSize = (nBoxDataOffset + nBoxDataLength - 2) - nOffset - 2;
            else if( nNextTileOffset >= nOffset + 2 )
                nMarkerSize = nNextTileOffset - nOffset - 2;

            CreateMarker( psCSBox, "SOD", nOffset, nMarkerSize );

            if( nNextTileOffset && nNextTileOffset == nOffset )
            {
                /* Found with Pleiades images. openjpeg doesn't like it either */
                nNextTileOffset = 0;
            }
            else if( nNextTileOffset && nNextTileOffset >= nOffset + 2 )
            {
                VSIFSeekL(fp, nNextTileOffset, SEEK_SET);
                nNextTileOffset = 0;
            }
            else
            {
                /* Skip to (hopefully) EOC */
                VSIFSeekL(fp, nBoxDataOffset + nBoxDataLength - 2, SEEK_SET);
            }
            continue;
        }
        if( abyMarker[1] == 0xD9 )
        {
            CreateMarker( psCSBox, "EOC", nOffset, 0 );
            continue;
        }
        /* Reserved markers */
        if( abyMarker[1] >= 0x30 && abyMarker[1] <= 0x3F )
        {
            CreateMarker( psCSBox, CPLSPrintf("Unknown 0xFF%02X", abyMarker[1]), nOffset, 0 );
            continue;
        }
        GUInt16 nMarkerSize;
        if( VSIFReadL(&nMarkerSize, 2, 1, fp) != 1 )
        {
            AddError(psCSBox, "Cannot read marker size", nOffset);
            break;
        }
        CPL_MSBPTR16(&nMarkerSize);
        if( nMarkerSize < 2 )
        {
            AddError(psCSBox, "Invalid marker size", nOffset);
            break;
        }
        if( VSIFReadL(pabyMarkerData, nMarkerSize - 2, 1, fp) != 1 )
        {
            AddError(psCSBox, "Cannot read marker data", nOffset);
            break;
        }
        GByte* pabyMarkerDataIter = pabyMarkerData;
        GUInt16 nRemainingMarkerSize = nMarkerSize - 2;
        GUInt32 nLastVal = 0;


#define READ_MARKER_FIELD_UINT8_COMMENT(name, comment) \
        do { if( nRemainingMarkerSize >= 1 ) { \
            nLastVal = *pabyMarkerDataIter; \
            AddField(psMarker, name, *pabyMarkerDataIter, comment); \
            pabyMarkerDataIter += 1; \
            nRemainingMarkerSize -= 1; \
            } \
            else { \
                AddError(psMarker, CPLSPrintf("Cannot read field %s", name)); \
                nLastVal = 0; \
            } \
        } while(0)

#define READ_MARKER_FIELD_UINT8(name) \
        READ_MARKER_FIELD_UINT8_COMMENT(name, NULL)

#define READ_MARKER_FIELD_UINT16_COMMENT(name, comment) \
        do { if( nRemainingMarkerSize >= 2 ) { \
            GUInt16 nVal; \
            memcpy(&nVal, pabyMarkerDataIter, 2); \
            CPL_MSBPTR16(&nVal); \
            nLastVal = nVal; \
            AddField(psMarker, name, nVal, comment); \
            pabyMarkerDataIter += 2; \
            nRemainingMarkerSize -= 2; \
            } \
            else { \
                AddError(psMarker, CPLSPrintf("Cannot read field %s", name)); \
                nLastVal = 0; \
            } \
        } while(0)

#define READ_MARKER_FIELD_UINT16(name) \
        READ_MARKER_FIELD_UINT16_COMMENT(name, NULL)

#define READ_MARKER_FIELD_UINT32_COMMENT(name, comment) \
        do { if( nRemainingMarkerSize >= 4 ) { \
            GUInt32 nVal; \
            memcpy(&nVal, pabyMarkerDataIter, 4); \
            CPL_MSBPTR32(&nVal); \
            AddField(psMarker, name, nVal, comment); \
            nLastVal = nVal; \
            pabyMarkerDataIter += 4; \
            nRemainingMarkerSize -= 4; \
            } \
            else { \
                AddError(psMarker, CPLSPrintf("Cannot read field %s", name)); \
                nLastVal = 0; \
            } \
        } while(0)

#define READ_MARKER_FIELD_UINT32(name) \
        READ_MARKER_FIELD_UINT32_COMMENT(name, NULL)

        if( abyMarker[1] == 0x90 )
        {
            CPLXMLNode* psMarker = CreateMarker( psCSBox, "SOT", nOffset, nMarkerSize );
            READ_MARKER_FIELD_UINT16("Isot");
            READ_MARKER_FIELD_UINT32("Psot");
            GUInt32 PSOT = nLastVal;
            READ_MARKER_FIELD_UINT8("TPsot");
            READ_MARKER_FIELD_UINT8("TNsot");
            if( nRemainingMarkerSize > 0 )
                CPLCreateXMLElementAndValue(
                        psMarker, "RemainingBytes",
                        CPLSPrintf("%d", (int)nRemainingMarkerSize ));

            if( PSOT )
                nNextTileOffset = nOffset + PSOT;
        }
        else if( abyMarker[1] == 0x51 )
        {
            CPLXMLNode* psMarker = CreateMarker( psCSBox, "SIZ", nOffset, nMarkerSize );
            READ_MARKER_FIELD_UINT16_COMMENT("Rsiz",
                                            (nLastVal == 0) ? "Unrestricted profile":
                                            (nLastVal == 1) ? "Profile 0":
                                            (nLastVal == 2) ? "Profile 1": NULL);
            READ_MARKER_FIELD_UINT32("Xsiz");
            READ_MARKER_FIELD_UINT32("Ysiz");
            READ_MARKER_FIELD_UINT32("XOsiz");
            READ_MARKER_FIELD_UINT32("YOsiz");
            READ_MARKER_FIELD_UINT32("XTsiz");
            READ_MARKER_FIELD_UINT32("YTsiz");
            READ_MARKER_FIELD_UINT32("XTOsiz");
            READ_MARKER_FIELD_UINT32("YTOsiz");
            READ_MARKER_FIELD_UINT16("Csiz");
            int CSiz = nLastVal;
            for(int i=0;i<CSiz;i++)
            {
                READ_MARKER_FIELD_UINT8_COMMENT(CPLSPrintf("Ssiz%d", i),
                                                GetInterpretationOfBPC(nLastVal));
                READ_MARKER_FIELD_UINT8(CPLSPrintf("XRsiz%d", i));
                READ_MARKER_FIELD_UINT8(CPLSPrintf("YRsiz%d", i));
            }
            if( nRemainingMarkerSize > 0 )
                CPLCreateXMLElementAndValue(
                        psMarker, "RemainingBytes",
                        CPLSPrintf("%d", (int)nRemainingMarkerSize ));
        }
        else if( abyMarker[1] == 0x52 )
        {
            CPLXMLNode* psMarker = CreateMarker( psCSBox, "COD", nOffset, nMarkerSize );
            int bHasPrecincts = TRUE;
            if( nRemainingMarkerSize >= 1 ) {
                nLastVal = *pabyMarkerDataIter;
                CPLString osInterp;
                if( nLastVal & 0x1 )
                {
                    bHasPrecincts = TRUE;
                    osInterp += "User defined precincts";
                }
                else
                    osInterp += "Standard precincts";
                osInterp += ", ";
                if( nLastVal & 0x2 )
                    osInterp += "SOP marker segments may be used";
                else
                    osInterp += "No SOP marker segments";
                osInterp += ", ";
                if( nLastVal & 0x4 )
                    osInterp += "EPH marker segments may be used";
                else
                    osInterp += "No EPH marker segments";
                AddField(psMarker, "Scod", nLastVal, osInterp.c_str());
                pabyMarkerDataIter += 1;
                nRemainingMarkerSize -= 1;
            }
            else {
                AddError(psMarker, CPLSPrintf("Cannot read field %s", "Scod"));
                nLastVal = 0;
            }
            READ_MARKER_FIELD_UINT8_COMMENT("SGcod_Progress",
                                            (nLastVal == 0) ? "LRCP" :
                                            (nLastVal == 1) ? "RLCP" :
                                            (nLastVal == 2) ? "RPCL" :
                                            (nLastVal == 3) ? "PCRL" :
                                            (nLastVal == 4) ? "CPRL" : NULL);
            READ_MARKER_FIELD_UINT16("SGcod_NumLayers");
            READ_MARKER_FIELD_UINT8("SGcod_MCT");
            READ_MARKER_FIELD_UINT8("SPcod_NumDecompositions");
            READ_MARKER_FIELD_UINT8_COMMENT("SPcod_xcb", CPLSPrintf("%d", 1 << nLastVal));
            READ_MARKER_FIELD_UINT8_COMMENT("SPcod_ycb", CPLSPrintf("%d", 1 << nLastVal));
            if( nRemainingMarkerSize >= 1 ) {
                nLastVal = *pabyMarkerDataIter;
                CPLString osInterp;
                if( nLastVal & 0x1 )
                    osInterp += "Selective arithmetic coding bypass";
                else
                    osInterp += "No selective arithmetic coding bypass";
                osInterp += ", ";
                if( nLastVal & 0x2 )
                    osInterp += "Reset context probabilities on coding pass boundaries";
                else
                    osInterp += "No reset of context probabilities on coding pass boundaries";
                osInterp += ", ";
                if( nLastVal & 0x4 )
                    osInterp += "Termination on each coding pass";
                else
                    osInterp += "No termination on each coding pass";
                osInterp += ", ";
                if( nLastVal & 0x8 )
                    osInterp += "Vertically causal context";
                else
                    osInterp += "No vertically causal context";
                osInterp += ", ";
                if( nLastVal & 0x10 )
                    osInterp += "Predictable termination";
                else
                    osInterp += "No predictable termination";
                osInterp += ", ";
                if( nLastVal & 0x20 )
                    osInterp += "Segmentation symbols are used";
                else
                    osInterp += "No segmentation symbols are used";
                AddField(psMarker, "SPcod_cbstyle", nLastVal, osInterp.c_str());
                pabyMarkerDataIter += 1;
                nRemainingMarkerSize -= 1;
            }
            else {
                AddError(psMarker, CPLSPrintf("Cannot read field %s", "SPcod_cbstyle"));
                nLastVal = 0;
            }
            READ_MARKER_FIELD_UINT8_COMMENT("SPcod_transformation",
                                            (nLastVal == 0) ? "9-7 irreversible":
                                            (nLastVal == 1) ? "5-3 reversible": NULL);
            if( bHasPrecincts )
            {
                while( nRemainingMarkerSize >= 1 )
                {
                    nLastVal = *pabyMarkerDataIter;
                    AddField(psMarker, "SPcod_Precincts", *pabyMarkerDataIter,
                             CPLSPrintf("PPx=%d PPy=%d: %dx%d",
                                        nLastVal & 0xf, nLastVal >> 4,
                                        1 << (nLastVal & 0xf), 1 << (nLastVal >> 4)));
                    pabyMarkerDataIter += 1;
                    nRemainingMarkerSize -= 1;
                }
            }
            if( nRemainingMarkerSize > 0 )
                CPLCreateXMLElementAndValue(
                        psMarker, "RemainingBytes",
                        CPLSPrintf("%d", (int)nRemainingMarkerSize ));
        }
        else if( abyMarker[1] == 0x53 )
        {
            CreateMarker( psCSBox, "COC", nOffset, nMarkerSize );
        }
        else if( abyMarker[1] == 0x55 )
        {
            CPLXMLNode* psMarker = CreateMarker( psCSBox, "TLM", nOffset, nMarkerSize );
            READ_MARKER_FIELD_UINT8("Ztlm");
            int ST, SP;
            READ_MARKER_FIELD_UINT8_COMMENT("Stlm",
                    CPLSPrintf("ST=%d SP=%d",
                               (ST = (nLastVal >> 4) & 3),
                               (SP = ((nLastVal >> 6) & 1))));
            int nTilePartDescLength = ST + ((SP == 0) ? 2 : 4);
            int i = 0;
            while( nRemainingMarkerSize >= nTilePartDescLength )
            {
                if( ST == 1 )
                    READ_MARKER_FIELD_UINT8(CPLSPrintf("Ttlm%d", i));
                else if( ST == 2 )
                    READ_MARKER_FIELD_UINT16(CPLSPrintf("Ttlm%d", i));
                if( SP == 0 )
                    READ_MARKER_FIELD_UINT16(CPLSPrintf("Ptlm%d", i));
                else
                    READ_MARKER_FIELD_UINT32(CPLSPrintf("Ptlm%d", i));
                i ++;
            }
            if( nRemainingMarkerSize > 0 )
                CPLCreateXMLElementAndValue(
                        psMarker, "RemainingBytes",
                        CPLSPrintf("%d", (int)nRemainingMarkerSize ));
        }
        else if( abyMarker[1] == 0x57 )
        {
            CreateMarker( psCSBox, "PLM", nOffset, nMarkerSize );
        }
        else if( abyMarker[1] == 0x58 )
        {
            CreateMarker( psCSBox, "PLT", nOffset, nMarkerSize );
        }
        else if( abyMarker[1] == 0x5C )
        {
            CreateMarker( psCSBox, "QCD", nOffset, nMarkerSize );
        }
        else if( abyMarker[1] == 0x5D )
        {
            CreateMarker( psCSBox, "QCC", nOffset, nMarkerSize );
        }
        else if( abyMarker[1] == 0x5E )
        {
            CreateMarker( psCSBox, "RGN", nOffset, nMarkerSize );
        }
        else if( abyMarker[1] == 0x5F )
        {
            CreateMarker( psCSBox, "POC", nOffset, nMarkerSize );
        }
        else if( abyMarker[1] == 0x60 )
        {
            CreateMarker( psCSBox, "PPM", nOffset, nMarkerSize );
        }
        else if( abyMarker[1] == 0x61 )
        {
            CreateMarker( psCSBox, "PPT", nOffset, nMarkerSize );
        }
        else if( abyMarker[1] == 0x63 )
        {
            CreateMarker( psCSBox, "CRG", nOffset, nMarkerSize );
        }
        else if( abyMarker[1] == 0x64 )
        {
            CPLXMLNode* psMarker = CreateMarker( psCSBox, "COM", nOffset, nMarkerSize );
            READ_MARKER_FIELD_UINT16_COMMENT("Rcom", (nLastVal == 0 ) ? "Binary" : (nLastVal == 1) ? "LATIN1" : NULL);
            if( nLastVal == 1 )
            {
                GByte abyBackup = pabyMarkerDataIter[nRemainingMarkerSize];
                pabyMarkerDataIter[nRemainingMarkerSize] = 0;
                AddField(psMarker, "COM", (int)nRemainingMarkerSize, (const char*)pabyMarkerDataIter);
                pabyMarkerDataIter[nRemainingMarkerSize] = abyBackup;
            }
        }
        else
        {
            CreateMarker( psCSBox, CPLSPrintf("Unknown 0xFF%02X", abyMarker[1]), nOffset, nMarkerSize );
        }
        VSIFSeekL(fp, nOffset + 2 + nMarkerSize, SEEK_SET);
    }
    CPLFree(pabyMarkerData);
    return psCSBox;
}

/************************************************************************/
/*                      GDALGetJPEG2000StructureInternal()              */
/************************************************************************/

static
void GDALGetJPEG2000StructureInternal(CPLXMLNode* psParent,
                                      VSILFILE* fp,
                                      GDALJP2Box* poParentBox,
                                      char** papszOptions)
{
    static const char* szHex = "0123456789ABCDEF";
    GDALJP2Box oBox( fp );
    if( oBox.ReadFirstChild(poParentBox) )
    {
        while( strlen(oBox.GetType()) > 0 )
        {
            GIntBig nBoxDataLength = oBox.GetDataLength();
            const char* pszBoxType = oBox.GetType();

            CPLXMLNode* psBox = CPLCreateXMLNode( psParent, CXT_Element, "JP2Box" );
            CPLAddXMLAttributeAndValue(psBox, "name", pszBoxType );
            CPLAddXMLAttributeAndValue(psBox, "box_offset",
                                       CPLSPrintf(CPL_FRMT_GIB, oBox.GetBoxOffset() )  );
            CPLAddXMLAttributeAndValue(psBox, "box_length",
                                       CPLSPrintf(CPL_FRMT_GIB, oBox.GetBoxLength() ) );
            CPLAddXMLAttributeAndValue(psBox, "data_offset",
                                       CPLSPrintf(CPL_FRMT_GIB, oBox.GetDataOffset() ) );
            CPLAddXMLAttributeAndValue(psBox, "data_length",
                                       CPLSPrintf(CPL_FRMT_GIB, nBoxDataLength ) );

            if( oBox.IsSuperBox() )
            {
                GDALGetJPEG2000StructureInternal(psBox, fp, &oBox, papszOptions);
            }
            else
            {
                if( strcmp(pszBoxType, "uuid") == 0 )
                {
                    char* pszBinaryContent = (char*)VSIMalloc( 2 * 16 + 1 );
                    const GByte* pabyUUID = oBox.GetUUID();
                    for(int i=0;i<16;i++)
                    {
                        pszBinaryContent[2*i] = szHex[pabyUUID[i] >> 4];
                        pszBinaryContent[2*i+1] = szHex[pabyUUID[i] & 0xf];
                    }
                    pszBinaryContent[2*16] = '\0';
                    CPLXMLNode* psUUIDNode =
                                CPLCreateXMLNode( psBox, CXT_Element, "UUID" );
                    if( GDALJP2Metadata::IsUUID_MSI(pabyUUID) )
                        CPLAddXMLAttributeAndValue(psUUIDNode, "description", "GeoTIFF" );
                    else if( GDALJP2Metadata::IsUUID_XMP(pabyUUID) )
                        CPLAddXMLAttributeAndValue(psUUIDNode, "description", "XMP" );
                    CPLCreateXMLNode( psUUIDNode, CXT_Text, pszBinaryContent);
                    VSIFree(pszBinaryContent);
                }

                if( (CSLFetchBoolean(papszOptions, "BINARY_CONTENT", FALSE) ||
                     CSLFetchBoolean(papszOptions, "ALL", FALSE) ) &&
                    strcmp(pszBoxType, "jp2c") != 0 &&
                    nBoxDataLength < 100 * 1024 )
                {
                    CPLXMLNode* psBinaryContent = CPLCreateXMLNode( psBox, CXT_Element, "BinaryContent" );
                    GByte* pabyBoxData = oBox.ReadBoxData();
                    int nBoxLength = (int)nBoxDataLength;
                    char* pszBinaryContent = (char*)VSIMalloc( 2 * nBoxLength + 1 );
                    if( pabyBoxData && pszBinaryContent )
                    {
                        for(int i=0;i<nBoxLength;i++)
                        {
                            pszBinaryContent[2*i] = szHex[pabyBoxData[i] >> 4];
                            pszBinaryContent[2*i+1] = szHex[pabyBoxData[i] & 0xf];
                        }
                        pszBinaryContent[2*nBoxLength] = '\0';
                        CPLCreateXMLNode( psBinaryContent, CXT_Text, pszBinaryContent );
                    }
                    CPLFree(pabyBoxData);
                    VSIFree(pszBinaryContent);
                }

                if( (CSLFetchBoolean(papszOptions, "TEXT_CONTENT", FALSE) ||
                     CSLFetchBoolean(papszOptions, "ALL", FALSE) ) &&
                    strcmp(pszBoxType, "jp2c") != 0 &&
                    nBoxDataLength < 100 * 1024 )
                {
                    GByte* pabyBoxData = oBox.ReadBoxData();
                    if( pabyBoxData )
                    {
                        if( CPLIsUTF8((const char*)pabyBoxData, -1) &&
                            (int)strlen((const char*)pabyBoxData) + 2 >= nBoxDataLength  )
                        {
                            CPLXMLNode* psXMLContentBox = NULL;
                            if( ((const char*)pabyBoxData)[0] ==  '<' )
                            {
                                CPLPushErrorHandler(CPLQuietErrorHandler);
                                psXMLContentBox = CPLParseXMLString((const char*)pabyBoxData);
                                CPLPopErrorHandler();
                            }
                            if( psXMLContentBox )
                            {
                                CPLXMLNode* psXMLContentNode = 
                                    CPLCreateXMLNode( psBox, CXT_Element, "XMLContent" );
                                psXMLContentNode->psChild = psXMLContentBox;
                            }
                            else
                            {
                                CPLCreateXMLNode( 
                                    CPLCreateXMLNode( psBox, CXT_Element, "TextContent" ),
                                        CXT_Text, (const char*)pabyBoxData);
                            }
                        }
                    }
                    CPLFree(pabyBoxData);
                }

                if( strcmp(pszBoxType, "jp2c") == 0 )
                {
                    if( CSLFetchBoolean(papszOptions, "CODESTREAM", FALSE) ||
                        CSLFetchBoolean(papszOptions, "ALL", FALSE) )
                    {
                        DumpJPK2CodeStream(psBox, fp,
                                           oBox.GetDataOffset(), nBoxDataLength);
                    }
                }
                else if( strcmp(pszBoxType, "uuid") == 0 &&
                         GDALJP2Metadata::IsUUID_MSI(oBox.GetUUID()) )
                {
                    DumpGeoTIFFBox(psBox, oBox);
                }
                else if( strcmp(pszBoxType, "ftyp") == 0 )
                {
                    DumpFTYPBox(psBox, oBox);
                }
                else if( strcmp(pszBoxType, "ihdr") == 0 )
                {
                    DumpIHDRBox(psBox, oBox);
                }
                else if( strcmp(pszBoxType, "bpcc") == 0 )
                {
                    DumpBPCCBox(psBox, oBox);
                }
                else if( strcmp(pszBoxType, "colr") == 0 )
                {
                    DumpCOLRBox(psBox, oBox);
                }
                else if( strcmp(pszBoxType, "pclr") == 0 )
                {
                    DumpPCLRBox(psBox, oBox);
                }
                else if( strcmp(pszBoxType, "cmap") == 0 )
                {
                    DumpCMAPBox(psBox, oBox);
                }
                else if( strcmp(pszBoxType, "cdef") == 0 )
                {
                    DumpCDEFBox(psBox, oBox);
                }
                else if( strcmp(pszBoxType, "resc") == 0 ||
                         strcmp(pszBoxType, "resd") == 0)
                {
                    DumpRESxBox(psBox, oBox);
                }
                else if( strcmp(pszBoxType, "rreq") == 0 )
                {
                    DumpRREQBox(psBox, oBox);
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

static const unsigned char jpc_header[] = {0xff,0x4f};
static const unsigned char jp2_box_jp[] = {0x6a,0x50,0x20,0x20}; /* 'jP  ' */

/** Dump the structure of a JPEG2000 file as a XML tree.
 *
 * @param pszFilename filename.
 * @param papszOptions NULL terminated list of options, or NULL.
 *                     Allowed options are BINARY_CONTENT=YES, TEXT_CONTENT=YES,
 *                     CODESTREAM=YES, ALL=YES.
 * @return XML tree (to be freed with CPLDestroyXMLNode()) or NULL in case
 *         of error
 * @since GDAL 2.0
 */

CPLXMLNode* GDALGetJPEG2000Structure(const char* pszFilename,
                                     char** papszOptions)
{
    VSILFILE* fp = VSIFOpenL(pszFilename, "rb");
    if( fp == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot open %s", pszFilename);
        return NULL;
    }
    GByte abyHeader[16];
    if( VSIFReadL(abyHeader, 16, 1, fp) != 1 ||
        (memcmp(abyHeader, jpc_header, sizeof(jpc_header)) != 0 &&
         memcmp(abyHeader + 4, jp2_box_jp, sizeof(jp2_box_jp)) != 0) )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s is not a JPEG2000 file", pszFilename);
        VSIFCloseL(fp);
        return NULL;
    }
    
    CPLXMLNode* psParent;
    if( memcmp(abyHeader, jpc_header, sizeof(jpc_header)) == 0 )
    {
        if( CSLFetchBoolean(papszOptions, "CODESTREAM", FALSE) ||
            CSLFetchBoolean(papszOptions, "ALL", FALSE) )
        {
            VSIFSeekL(fp, 0, SEEK_END);
            GIntBig nBoxDataLength = (GIntBig)VSIFTellL(fp);
            psParent = DumpJPK2CodeStream(NULL, fp, 0, nBoxDataLength);
        }
    }
    else
    {
        psParent = CPLCreateXMLNode( NULL, CXT_Element, "JP2File" );
        GDALGetJPEG2000StructureInternal(psParent, fp, NULL, papszOptions );
    }

    VSIFCloseL(fp);
    return psParent;
}
