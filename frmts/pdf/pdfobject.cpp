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
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include <vector>
#include "pdfobject.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                        ROUND_TO_INT_IF_CLOSE()                       */
/************************************************************************/

double ROUND_TO_INT_IF_CLOSE(double x, double eps)
{
    if( eps == 0.0 )
        eps = fabs(x) < 1 ? 1e-10 : 1e-8;
    int nClosestInt = (int)floor(x + 0.5);
    if ( fabs(x - nClosestInt) < eps )
        return nClosestInt;
    else
        return x;
}

/************************************************************************/
/*                         GDALPDFGetPDFString()                        */
/************************************************************************/

static CPLString GDALPDFGetPDFString(const char* pszStr)
{
    GByte* pabyData = (GByte*)pszStr;
    int i;
    GByte ch;
    for(i=0;(ch = pabyData[i]) != '\0';i++)
    {
        if (ch < 32 || ch > 127 ||
            ch == '(' || ch == ')' ||
            ch == '\\' || ch == '%' || ch == '#')
            break;
    }
    CPLString osStr;
    if (ch == 0)
    {
        osStr = "(";
        osStr += pszStr;
        osStr += ")";
        return osStr;
    }

    wchar_t* pwszDest = CPLRecodeToWChar( pszStr, CPL_ENC_UTF8, CPL_ENC_UCS2 );
    osStr = "<FEFF";
    for(i=0;pwszDest[i] != 0;i++)
    {
#ifndef _WIN32
        if (pwszDest[i] >= 0x10000 /* && pwszDest[i] <= 0x10FFFF */)
        {
            /* Generate UTF-16 surrogate pairs (on Windows, CPLRecodeToWChar does it for us)  */
            int nHeadSurrogate = ((pwszDest[i] - 0x10000) >> 10) | 0xd800;
            int nTrailSurrogate = ((pwszDest[i] - 0x10000) & 0x3ff) | 0xdc00;
            osStr += CPLSPrintf("%02X", (nHeadSurrogate >> 8) & 0xff);
            osStr += CPLSPrintf("%02X", (nHeadSurrogate) & 0xff);
            osStr += CPLSPrintf("%02X", (nTrailSurrogate >> 8) & 0xff);
            osStr += CPLSPrintf("%02X", (nTrailSurrogate) & 0xff);
        }
        else
#endif
        {
            osStr += CPLSPrintf("%02X", (int)(pwszDest[i] >> 8) & 0xff);
            osStr += CPLSPrintf("%02X", (int)(pwszDest[i]) & 0xff);
        }
    }
    osStr += ">";
    CPLFree(pwszDest);
    return osStr;
}

#if defined(HAVE_POPPLER) || defined(HAVE_PDFIUM)

/************************************************************************/
/*                     GDALPDFGetUTF8StringFromBytes()                  */
/************************************************************************/

static CPLString GDALPDFGetUTF8StringFromBytes(const GByte* pabySrc, int nLen)
{
    int bLEUnicodeMarker = nLen > 2 && pabySrc[0] == 0xFE && pabySrc[1] == 0xFF;
    int bBEUnicodeMarker = nLen > 2 && pabySrc[0] == 0xFF && pabySrc[1] == 0xFE;
    if (!bLEUnicodeMarker && !bBEUnicodeMarker)
    {
        CPLString osStr;
        osStr.resize( nLen + 1 );
        osStr.assign( (const char*)pabySrc, (size_t)nLen );
        osStr[nLen] = 0;
        const char* pszStr = osStr.c_str();
        if (CPLIsUTF8(pszStr, -1))
            return osStr;
        else
        {
            char* pszUTF8 = CPLRecode( pszStr, CPL_ENC_ISO8859_1, CPL_ENC_UTF8 );
            CPLString osRet = pszUTF8;
            CPLFree(pszUTF8);
            return osRet;
        }
    }

    /* This is UTF-16 content */
    pabySrc += 2;
    nLen = (nLen - 2) / 2;
    wchar_t *pwszSource = new wchar_t[nLen + 1];
    int j = 0;
    for(int i=0; i<nLen; i++, j++)
    {
        if (!bBEUnicodeMarker)
            pwszSource[j] = (pabySrc[2 * i] << 8) + pabySrc[2 * i + 1];
        else
            pwszSource[j] = (pabySrc[2 * i + 1] << 8) + pabySrc[2 * i];
#ifndef _WIN32
        /* Is there a surrogate pair ? See http://en.wikipedia.org/wiki/UTF-16 */
        /* On Windows, CPLRecodeFromWChar does this for us, because wchar_t is only */
        /* 2 bytes wide, whereas on Unix it is 32bits */
        if (pwszSource[j] >= 0xD800 && pwszSource[j] <= 0xDBFF && i + 1 < nLen)
        {
            /* should be in the range 0xDC00... 0xDFFF */
            wchar_t nTrailSurrogate;
            if (!bBEUnicodeMarker)
                nTrailSurrogate = (pabySrc[2 * (i+1)] << 8) + pabySrc[2 * (i+1) + 1];
            else
                nTrailSurrogate = (pabySrc[2 * (i+1) + 1] << 8) + pabySrc[2 * (i+1)];
            if (nTrailSurrogate >= 0xDC00 && nTrailSurrogate <= 0xDFFF)
            {
                pwszSource[j] = ((pwszSource[j] - 0xD800) << 10) + (nTrailSurrogate - 0xDC00) + 0x10000;
                i++;
            }
        }
#endif
    }
    pwszSource[j] = 0;

    char* pszUTF8 = CPLRecodeFromWChar( pwszSource, CPL_ENC_UCS2, CPL_ENC_UTF8 );
    delete[] pwszSource;
    CPLString osStrUTF8(pszUTF8);
    CPLFree(pszUTF8);
    return osStrUTF8;
}

#endif // defined(HAVE_POPPLER) || defined(HAVE_PDFIUM)

/************************************************************************/
/*                          GDALPDFGetPDFName()                         */
/************************************************************************/

static CPLString GDALPDFGetPDFName(const char* pszStr)
{
    GByte* pabyData = (GByte*)pszStr;
    int i;
    GByte ch;
    CPLString osStr;
    for(i=0;(ch = pabyData[i]) != '\0';i++)
    {
        if (!((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') || ch == '-'))
            osStr += '_';
        else
            osStr += ch;
    }
    return osStr;
}

/************************************************************************/
/* ==================================================================== */
/*                            GDALPDFObject                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                            ~GDALPDFObject()                          */
/************************************************************************/

GDALPDFObject::~GDALPDFObject()
{
}

/************************************************************************/
/*                            LookupObject()                            */
/************************************************************************/

GDALPDFObject* GDALPDFObject::LookupObject(const char* pszPath)
{
    if( GetType() != PDFObjectType_Dictionary )
        return nullptr;
    return GetDictionary()->LookupObject(pszPath);
}

/************************************************************************/
/*                             GetTypeName()                            */
/************************************************************************/

const char* GDALPDFObject::GetTypeName()
{
    switch(GetType())
    {
        case PDFObjectType_Unknown: return GetTypeNameNative();
        case PDFObjectType_Null: return "null";
        case PDFObjectType_Bool: return "bool";
        case PDFObjectType_Int: return "int";
        case PDFObjectType_Real: return "real";
        case PDFObjectType_String: return "string";
        case PDFObjectType_Name: return "name";
        case PDFObjectType_Array: return "array";
        case PDFObjectType_Dictionary: return "dictionary";
        default: return GetTypeNameNative();
    }
}

/************************************************************************/
/*                             Serialize()                              */
/************************************************************************/

void GDALPDFObject::Serialize(CPLString& osStr, bool bEmitRef)
{
    auto nRefNum = GetRefNum();
    if( bEmitRef && nRefNum.toBool() )
    {
        int nRefGen = GetRefGen();
        osStr.append(CPLSPrintf("%d %d R", nRefNum.toInt(), nRefGen));
        return;
    }

    switch(GetType())
    {
        case PDFObjectType_Null: osStr.append("null"); return;
        case PDFObjectType_Bool: osStr.append(GetBool() ? "true": "false"); return;
        case PDFObjectType_Int: osStr.append(CPLSPrintf("%d", GetInt())); return;
        case PDFObjectType_Real:
        {
            char szReal[512];
            double dfRealNonRounded = GetReal();
            double dfReal = ROUND_TO_INT_IF_CLOSE(dfRealNonRounded);
            if (dfReal == (double)(GIntBig)dfReal)
                snprintf(szReal, sizeof(szReal), CPL_FRMT_GIB, (GIntBig)dfReal);
            else if (CanRepresentRealAsString())
            {
                /* Used for OGC BP numeric values */
                CPLsnprintf(szReal, sizeof(szReal), "(%.*g)", GetPrecision(), dfReal);
            }
            else
            {
                CPLsnprintf(szReal, sizeof(szReal), "%.*f", GetPrecision(), dfReal);

                /* Remove non significant trailing zeroes */
                char* pszDot = strchr(szReal, '.');
                if (pszDot)
                {
                    int iDot = (int)(pszDot - szReal);
                    int nLen = (int)strlen(szReal);
                    for(int i=nLen-1; i > iDot; i --)
                    {
                        if (szReal[i] == '0')
                            szReal[i] = '\0';
                        else
                            break;
                    }
                }
            }
            osStr.append(szReal);
            return;
        }
        case PDFObjectType_String: osStr.append(GDALPDFGetPDFString(GetString())); return;
        case PDFObjectType_Name: osStr.append("/"); osStr.append(GDALPDFGetPDFName(GetName())); return;
        case PDFObjectType_Array: GetArray()->Serialize(osStr); return;
        case PDFObjectType_Dictionary: GetDictionary()->Serialize(osStr); return;
        case PDFObjectType_Unknown:
        default: CPLError(CE_Warning, CPLE_AppDefined,
                          "Serializing unknown object !"); return;
    }
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

GDALPDFObjectRW* GDALPDFObject::Clone()
{
    auto nRefNum = GetRefNum();
    if( nRefNum.toBool() )
    {
        int nRefGen = GetRefGen();
        return GDALPDFObjectRW::CreateIndirect(nRefNum, nRefGen);
    }

    switch(GetType())
    {
        case PDFObjectType_Null: return GDALPDFObjectRW::CreateNull();
        case PDFObjectType_Bool: return GDALPDFObjectRW::CreateBool(GetBool());
        case PDFObjectType_Int: return GDALPDFObjectRW::CreateInt(GetInt());
        case PDFObjectType_Real: return GDALPDFObjectRW::CreateReal(GetReal());
        case PDFObjectType_String: return GDALPDFObjectRW::CreateString(GetString());
        case PDFObjectType_Name: return GDALPDFObjectRW::CreateName(GetName());
        case PDFObjectType_Array: return GDALPDFObjectRW::CreateArray(GetArray()->Clone());
        case PDFObjectType_Dictionary: return GDALPDFObjectRW::CreateDictionary(GetDictionary()->Clone());
        case PDFObjectType_Unknown:
        default: CPLError(CE_Warning, CPLE_AppDefined,
                          "Cloning unknown object !"); return nullptr;
    }
}

/************************************************************************/
/* ==================================================================== */
/*                         GDALPDFDictionary                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        ~GDALPDFDictionary()                          */
/************************************************************************/

GDALPDFDictionary::~GDALPDFDictionary()
{
}

/************************************************************************/
/*                            LookupObject()                            */
/************************************************************************/

GDALPDFObject* GDALPDFDictionary::LookupObject(const char* pszPath)
{
    GDALPDFObject* poCurObj = nullptr;
    char** papszTokens = CSLTokenizeString2(pszPath, ".", 0);
    for(int i=0; papszTokens[i] != nullptr; i++)
    {
        int iElt = -1;
        char* pszBracket = strchr(papszTokens[i], '[');
        if( pszBracket != nullptr )
        {
            iElt = atoi(pszBracket + 1);
            *pszBracket = '\0';
        }

        if( i == 0 )
        {
            poCurObj = Get(papszTokens[i]);
        }
        else
        {
            if( poCurObj->GetType() != PDFObjectType_Dictionary )
            {
                poCurObj = nullptr;
                break;
            }
            poCurObj = poCurObj->GetDictionary()->Get(papszTokens[i]);
        }

        if( poCurObj == nullptr )
        {
            poCurObj = nullptr;
            break;
        }

        if( iElt >= 0 )
        {
            if( poCurObj->GetType() != PDFObjectType_Array )
            {
                poCurObj = nullptr;
                break;
            }
            poCurObj = poCurObj->GetArray()->Get(iElt);
        }
    }
    CSLDestroy(papszTokens);
    return poCurObj;
}

/************************************************************************/
/*                             Serialize()                              */
/************************************************************************/

void GDALPDFDictionary::Serialize(CPLString& osStr)
{
    osStr.append("<< ");
    std::map<CPLString, GDALPDFObject*>& oMap = GetValues();
    std::map<CPLString, GDALPDFObject*>::iterator oIter = oMap.begin();
    std::map<CPLString, GDALPDFObject*>::iterator oEnd = oMap.end();
    for(;oIter != oEnd;++oIter)
    {
        const char* pszKey = oIter->first.c_str();
        GDALPDFObject* poObj = oIter->second;
        osStr.append("/");
        osStr.append(pszKey);
        osStr.append(" ");
        poObj->Serialize(osStr);
        osStr.append(" ");
    }
    osStr.append(">>");
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

GDALPDFDictionaryRW* GDALPDFDictionary::Clone()
{
    GDALPDFDictionaryRW* poDict = new GDALPDFDictionaryRW();
    std::map<CPLString, GDALPDFObject*>& oMap = GetValues();
    std::map<CPLString, GDALPDFObject*>::iterator oIter = oMap.begin();
    std::map<CPLString, GDALPDFObject*>::iterator oEnd = oMap.end();
    for(;oIter != oEnd;++oIter)
    {
        const char* pszKey = oIter->first.c_str();
        GDALPDFObject* poObj = oIter->second;
        poDict->Add(pszKey, poObj->Clone());
    }
    return poDict;
}

/************************************************************************/
/* ==================================================================== */
/*                           GDALPDFArray                               */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           ~GDALPDFArray()                            */
/************************************************************************/

GDALPDFArray::~GDALPDFArray()
{
}

/************************************************************************/
/*                             Serialize()                              */
/************************************************************************/

void GDALPDFArray::Serialize(CPLString& osStr)
{
    int nLength = GetLength();
    int i;

    osStr.append("[ ");
    for(i=0;i<nLength;i++)
    {
        Get(i)->Serialize(osStr);
        osStr.append(" ");
    }
    osStr.append("]");
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

GDALPDFArrayRW* GDALPDFArray::Clone()
{
    GDALPDFArrayRW* poArray = new GDALPDFArrayRW();
    int nLength = GetLength();
    int i;
    for(i=0;i<nLength;i++)
    {
        poArray->Add(Get(i)->Clone());
    }
    return poArray;
}

/************************************************************************/
/* ==================================================================== */
/*                           GDALPDFStream                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           ~GDALPDFStream()                           */
/************************************************************************/

GDALPDFStream::~GDALPDFStream()
{
}

/************************************************************************/
/* ==================================================================== */
/*                           GDALPDFObjectRW                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           GDALPDFObjectRW()                          */
/************************************************************************/

GDALPDFObjectRW::GDALPDFObjectRW(GDALPDFObjectType eType) :
    m_eType(eType),
    m_nVal(0),
    m_dfVal(0.0),
    // m_osVal
    m_poDict(nullptr),
    m_poArray(nullptr),
    m_nNum(0),
    m_nGen(0),
    m_bCanRepresentRealAsString(FALSE)
{}

/************************************************************************/
/*                             ~GDALPDFObjectRW()                       */
/************************************************************************/

GDALPDFObjectRW::~GDALPDFObjectRW()
{
    delete m_poDict;
    delete m_poArray;
}

/************************************************************************/
/*                            CreateIndirect()                          */
/************************************************************************/

GDALPDFObjectRW* GDALPDFObjectRW::CreateIndirect(const GDALPDFObjectNum& nNum, int nGen)
{
    GDALPDFObjectRW* poObj = new GDALPDFObjectRW(PDFObjectType_Unknown);
    poObj->m_nNum = nNum;
    poObj->m_nGen = nGen;
    return poObj;
}

/************************************************************************/
/*                             CreateNull()                             */
/************************************************************************/

GDALPDFObjectRW* GDALPDFObjectRW::CreateNull()
{
    return new GDALPDFObjectRW(PDFObjectType_Null);
}

/************************************************************************/
/*                             CreateBool()                             */
/************************************************************************/

GDALPDFObjectRW* GDALPDFObjectRW::CreateBool(int bVal)
{
    GDALPDFObjectRW* poObj = new GDALPDFObjectRW(PDFObjectType_Bool);
    poObj->m_nVal = bVal;
    return poObj;
}

/************************************************************************/
/*                             CreateInt()                              */
/************************************************************************/

GDALPDFObjectRW* GDALPDFObjectRW::CreateInt(int nVal)
{
    GDALPDFObjectRW* poObj = new GDALPDFObjectRW(PDFObjectType_Int);
    poObj->m_nVal = nVal;
    return poObj;
}

/************************************************************************/
/*                            CreateReal()                              */
/************************************************************************/

GDALPDFObjectRW* GDALPDFObjectRW::CreateReal(double dfVal,
                                             int bCanRepresentRealAsString)
{
    GDALPDFObjectRW* poObj = new GDALPDFObjectRW(PDFObjectType_Real);
    poObj->m_dfVal = dfVal;
    poObj->m_bCanRepresentRealAsString = bCanRepresentRealAsString;
    return poObj;
}

/************************************************************************/
/*                       CreateRealWithPrecision()                      */
/************************************************************************/

GDALPDFObjectRW* GDALPDFObjectRW::CreateRealWithPrecision(double dfVal,
                                             int nPrecision)
{
    GDALPDFObjectRW* poObj = new GDALPDFObjectRW(PDFObjectType_Real);
    poObj->m_dfVal = dfVal;
    poObj->m_nPrecision = nPrecision;
    return poObj;
}

/************************************************************************/
/*                           CreateString()                             */
/************************************************************************/

GDALPDFObjectRW* GDALPDFObjectRW::CreateString(const char* pszStr)
{
    GDALPDFObjectRW* poObj = new GDALPDFObjectRW(PDFObjectType_String);
    poObj->m_osVal = pszStr;
    return poObj;
}

/************************************************************************/
/*                            CreateName()                              */
/************************************************************************/

GDALPDFObjectRW* GDALPDFObjectRW::CreateName(const char* pszName)
{
    GDALPDFObjectRW* poObj = new GDALPDFObjectRW(PDFObjectType_Name);
    poObj->m_osVal = pszName;
    return poObj;
}

/************************************************************************/
/*                          CreateDictionary()                          */
/************************************************************************/

GDALPDFObjectRW* GDALPDFObjectRW::CreateDictionary(GDALPDFDictionaryRW* poDict)
{
    CPLAssert(poDict);
    GDALPDFObjectRW* poObj = new GDALPDFObjectRW(PDFObjectType_Dictionary);
    poObj->m_poDict = poDict;
    return poObj;
}

/************************************************************************/
/*                            CreateArray()                             */
/************************************************************************/

GDALPDFObjectRW* GDALPDFObjectRW::CreateArray(GDALPDFArrayRW* poArray)
{
    CPLAssert(poArray);
    GDALPDFObjectRW* poObj = new GDALPDFObjectRW(PDFObjectType_Array);
    poObj->m_poArray = poArray;
    return poObj;
}

/************************************************************************/
/*                          GetTypeNameNative()                         */
/************************************************************************/

const char* GDALPDFObjectRW::GetTypeNameNative()
{
    CPLError(CE_Failure, CPLE_AppDefined, "Should not go here");
    return "";
}

/************************************************************************/
/*                             GetType()                                */
/************************************************************************/

GDALPDFObjectType GDALPDFObjectRW::GetType()
{
    return m_eType;
}

/************************************************************************/
/*                             GetBool()                                */
/************************************************************************/

int GDALPDFObjectRW::GetBool()
{
    if (m_eType == PDFObjectType_Bool)
        return m_nVal;

    return FALSE;
}

/************************************************************************/
/*                               GetInt()                               */
/************************************************************************/

int GDALPDFObjectRW::GetInt()
{
    if (m_eType == PDFObjectType_Int)
        return m_nVal;

    return 0;
}

/************************************************************************/
/*                              GetReal()                               */
/************************************************************************/

double GDALPDFObjectRW::GetReal()
{
   return m_dfVal;
}

/************************************************************************/
/*                             GetString()                              */
/************************************************************************/

const CPLString& GDALPDFObjectRW::GetString()
{
    return m_osVal;
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const CPLString& GDALPDFObjectRW::GetName()
{
    return m_osVal;
}

/************************************************************************/
/*                            GetDictionary()                           */
/************************************************************************/

GDALPDFDictionary* GDALPDFObjectRW::GetDictionary()
{
    return m_poDict;
}

/************************************************************************/
/*                              GetArray()                              */
/************************************************************************/

GDALPDFArray* GDALPDFObjectRW::GetArray()
{
    return m_poArray;
}

/************************************************************************/
/*                              GetStream()                             */
/************************************************************************/

GDALPDFStream* GDALPDFObjectRW::GetStream()
{
    return nullptr;
}

/************************************************************************/
/*                              GetRefNum()                             */
/************************************************************************/

GDALPDFObjectNum GDALPDFObjectRW::GetRefNum()
{
    return m_nNum;
}

/************************************************************************/
/*                              GetRefGen()                             */
/************************************************************************/

int GDALPDFObjectRW::GetRefGen()
{
    return m_nGen;
}

/************************************************************************/
/* ==================================================================== */
/*                          GDALPDFDictionaryRW                         */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           GDALPDFDictionaryRW()                      */
/************************************************************************/

GDALPDFDictionaryRW::GDALPDFDictionaryRW() {}

/************************************************************************/
/*                          ~GDALPDFDictionaryRW()                      */
/************************************************************************/

GDALPDFDictionaryRW::~GDALPDFDictionaryRW()
{
    std::map<CPLString, GDALPDFObject*>::iterator oIter = m_map.begin();
    std::map<CPLString, GDALPDFObject*>::iterator oEnd = m_map.end();
    for(; oIter != oEnd; ++oIter)
        delete oIter->second;
}

/************************************************************************/
/*                                   Get()                              */
/************************************************************************/

GDALPDFObject* GDALPDFDictionaryRW::Get(const char* pszKey)
{
    std::map<CPLString, GDALPDFObject*>::iterator oIter = m_map.find(pszKey);
    if (oIter != m_map.end())
        return oIter->second;
    return nullptr;
}

/************************************************************************/
/*                               GetValues()                            */
/************************************************************************/

std::map<CPLString, GDALPDFObject*>& GDALPDFDictionaryRW::GetValues()
{
    return m_map;
}

/************************************************************************/
/*                                 Add()                                */
/************************************************************************/

GDALPDFDictionaryRW& GDALPDFDictionaryRW::Add(const char* pszKey, GDALPDFObject* poVal)
{
    std::map<CPLString, GDALPDFObject*>::iterator oIter = m_map.find(pszKey);
    if (oIter != m_map.end())
    {
        delete oIter->second;
        oIter->second = poVal;
    }
    else
        m_map[pszKey] = poVal;

    return *this;
}

/************************************************************************/
/*                                Remove()                              */
/************************************************************************/

GDALPDFDictionaryRW& GDALPDFDictionaryRW::Remove(const char* pszKey)
{
    std::map<CPLString, GDALPDFObject*>::iterator oIter = m_map.find(pszKey);
    if (oIter != m_map.end())
    {
        delete oIter->second;
        m_map.erase(pszKey);
    }

    return *this;
}

/************************************************************************/
/* ==================================================================== */
/*                            GDALPDFArrayRW                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             GDALPDFArrayRW()                         */
/************************************************************************/

GDALPDFArrayRW::GDALPDFArrayRW() {}

/************************************************************************/
/*                            ~GDALPDFArrayRW()                         */
/************************************************************************/

GDALPDFArrayRW::~GDALPDFArrayRW()
{
    for( size_t i = 0; i < m_array.size(); i++ )
        delete m_array[i];
}

/************************************************************************/
/*                               GetLength()                             */
/************************************************************************/

int GDALPDFArrayRW::GetLength()
{
    return static_cast<int>(m_array.size());
}

/************************************************************************/
/*                                  Get()                               */
/************************************************************************/

GDALPDFObject* GDALPDFArrayRW::Get(int nIndex)
{
    if (nIndex < 0 || nIndex >= GetLength())
        return nullptr;
    return m_array[nIndex];
}

/************************************************************************/
/*                                  Add()                               */
/************************************************************************/

GDALPDFArrayRW& GDALPDFArrayRW::Add(GDALPDFObject* poObj)
{
    m_array.push_back(poObj);
    return *this;
}

/************************************************************************/
/*                                  Add()                               */
/************************************************************************/

GDALPDFArrayRW& GDALPDFArrayRW::Add(double* padfVal, int nCount,
                                    int bCanRepresentRealAsString)
{
    for(int i=0;i<nCount;i++)
        m_array.push_back(GDALPDFObjectRW::CreateReal(padfVal[i], bCanRepresentRealAsString));
    return *this;
}

#ifdef HAVE_POPPLER

/************************************************************************/
/* ==================================================================== */
/*                         GDALPDFDictionaryPoppler                     */
/* ==================================================================== */
/************************************************************************/

class GDALPDFDictionaryPoppler: public GDALPDFDictionary
{
    private:
        Dict* m_poDict;
        std::map<CPLString, GDALPDFObject*> m_map;

    public:
        GDALPDFDictionaryPoppler(Dict* poDict) : m_poDict(poDict) {}
        virtual ~GDALPDFDictionaryPoppler();

        virtual GDALPDFObject* Get(const char* pszKey) override;
        virtual std::map<CPLString, GDALPDFObject*>& GetValues() override;
};

/************************************************************************/
/* ==================================================================== */
/*                           GDALPDFArrayPoppler                        */
/* ==================================================================== */
/************************************************************************/

class GDALPDFArrayPoppler : public GDALPDFArray
{
    private:
        Array* m_poArray;
        std::vector<GDALPDFObject*> m_v;

    public:
        GDALPDFArrayPoppler(Array* poArray) : m_poArray(poArray) {}
        virtual ~GDALPDFArrayPoppler();

        virtual int GetLength() override;
        virtual GDALPDFObject* Get(int nIndex) override;
};

/************************************************************************/
/* ==================================================================== */
/*                           GDALPDFStreamPoppler                       */
/* ==================================================================== */
/************************************************************************/

class GDALPDFStreamPoppler : public GDALPDFStream
{
    private:
        int     m_nLength = -1;
        Stream* m_poStream;
        int     m_nRawLength = -1;

    public:
        GDALPDFStreamPoppler(Stream* poStream) : m_poStream(poStream) {}
        virtual ~GDALPDFStreamPoppler() {}

        virtual int GetLength() override;
        virtual char* GetBytes() override;

        virtual int GetRawLength() override;
        virtual char* GetRawBytes() override;
};

/************************************************************************/
/* ==================================================================== */
/*                         GDALPDFObjectPoppler                         */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          ~GDALPDFObjectPoppler()                     */
/************************************************************************/

GDALPDFObjectPoppler::~GDALPDFObjectPoppler()
{
#if !(POPPLER_MAJOR_VERSION >= 1 || POPPLER_MINOR_VERSION >= 58)
    m_po->free();
#endif
    if (m_bDestroy)
        delete m_po;
    delete m_poDict;
    delete m_poArray;
    delete m_poStream;
}

/************************************************************************/
/*                              GetType()                               */
/************************************************************************/

GDALPDFObjectType GDALPDFObjectPoppler::GetType()
{
    switch(m_po->getType())
    {
        case objNull:       return PDFObjectType_Null;
        case objBool:       return PDFObjectType_Bool;
        case objInt:        return PDFObjectType_Int;
        case objReal:       return PDFObjectType_Real;
        case objString:     return PDFObjectType_String;
        case objName:       return PDFObjectType_Name;
        case objArray:      return PDFObjectType_Array;
        case objDict:       return PDFObjectType_Dictionary;
        case objStream:     return PDFObjectType_Dictionary;
        default:            return PDFObjectType_Unknown;
    }
}

/************************************************************************/
/*                          GetTypeNameNative()                         */
/************************************************************************/

const char* GDALPDFObjectPoppler::GetTypeNameNative()
{
    return m_po->getTypeName();
}

/************************************************************************/
/*                               GetBool()                              */
/************************************************************************/

int GDALPDFObjectPoppler::GetBool()
{
    if (GetType() == PDFObjectType_Bool)
        return m_po->getBool();
    else
        return 0;
}

/************************************************************************/
/*                               GetInt()                               */
/************************************************************************/

int GDALPDFObjectPoppler::GetInt()
{
    if (GetType() == PDFObjectType_Int)
        return m_po->getInt();
    else
        return 0;
}

/************************************************************************/
/*                               GetReal()                              */
/************************************************************************/

double GDALPDFObjectPoppler::GetReal()
{
    if (GetType() == PDFObjectType_Real)
        return m_po->getReal();
    else
        return 0.0;
}

/************************************************************************/
/*                              GetString()                             */
/************************************************************************/

const CPLString& GDALPDFObjectPoppler::GetString()
{
    if (GetType() == PDFObjectType_String)
    {
#if POPPLER_MAJOR_VERSION >= 1 || POPPLER_MINOR_VERSION >= 58
        // At least available since poppler 0.41
        const GooString* gooString = m_po->getString();
#else
        GooString* gooString = m_po->getString();
#endif
#if (POPPLER_MAJOR_VERSION >= 1 || POPPLER_MINOR_VERSION >= 72)
        return (osStr = GDALPDFGetUTF8StringFromBytes(reinterpret_cast<const GByte*>(gooString->c_str()),
                                                      static_cast<int>(gooString->getLength())));
#else
        return (osStr = GDALPDFGetUTF8StringFromBytes(reinterpret_cast<const GByte*>(gooString->getCString()),
                                                      static_cast<int>(gooString->getLength())));
#endif
    }
    else
        return (osStr = "");
}

/************************************************************************/
/*                               GetName()                              */
/************************************************************************/

const CPLString& GDALPDFObjectPoppler::GetName()
{
    if (GetType() == PDFObjectType_Name)
        return (osStr = m_po->getName());
    else
        return (osStr = "");
}

/************************************************************************/
/*                            GetDictionary()                           */
/************************************************************************/

GDALPDFDictionary* GDALPDFObjectPoppler::GetDictionary()
{
    if (GetType() != PDFObjectType_Dictionary)
        return nullptr;

    if (m_poDict)
        return m_poDict;

    Dict* poDict = (m_po->getType() == objStream) ? m_po->getStream()->getDict() : m_po->getDict();
    if (poDict == nullptr)
        return nullptr;
    m_poDict = new GDALPDFDictionaryPoppler(poDict);
    return m_poDict;
}

/************************************************************************/
/*                              GetArray()                              */
/************************************************************************/

GDALPDFArray* GDALPDFObjectPoppler::GetArray()
{
    if (GetType() != PDFObjectType_Array)
        return nullptr;

    if (m_poArray)
        return m_poArray;

    Array* poArray = m_po->getArray();
    if (poArray == nullptr)
        return nullptr;
    m_poArray = new GDALPDFArrayPoppler(poArray);
    return m_poArray;
}

/************************************************************************/
/*                             GetStream()                              */
/************************************************************************/

GDALPDFStream* GDALPDFObjectPoppler::GetStream()
{
    if (m_po->getType() != objStream)
        return nullptr;

    if (m_poStream)
        return m_poStream;
    m_poStream = new GDALPDFStreamPoppler(m_po->getStream());
    return m_poStream;
}

/************************************************************************/
/*                           SetRefNumAndGen()                          */
/************************************************************************/

void GDALPDFObjectPoppler::SetRefNumAndGen(const GDALPDFObjectNum& nNum, int nGen)
{
    m_nRefNum = nNum;
    m_nRefGen = nGen;
}

/************************************************************************/
/*                               GetRefNum()                            */
/************************************************************************/

GDALPDFObjectNum GDALPDFObjectPoppler::GetRefNum()
{
    return m_nRefNum;
}

/************************************************************************/
/*                               GetRefGen()                            */
/************************************************************************/

int GDALPDFObjectPoppler::GetRefGen()
{
    return m_nRefGen;
}

/************************************************************************/
/* ==================================================================== */
/*                        GDALPDFDictionaryPoppler                      */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                       ~GDALPDFDictionaryPoppler()                    */
/************************************************************************/

GDALPDFDictionaryPoppler::~GDALPDFDictionaryPoppler()
{
    std::map<CPLString, GDALPDFObject*>::iterator oIter = m_map.begin();
    std::map<CPLString, GDALPDFObject*>::iterator oEnd = m_map.end();
    for(; oIter != oEnd; ++oIter)
        delete oIter->second;
}

/************************************************************************/
/*                                  Get()                               */
/************************************************************************/

GDALPDFObject* GDALPDFDictionaryPoppler::Get(const char* pszKey)
{
    std::map<CPLString, GDALPDFObject*>::iterator oIter = m_map.find(pszKey);
    if (oIter != m_map.end())
        return oIter->second;

#if POPPLER_MAJOR_VERSION >= 1 || POPPLER_MINOR_VERSION >= 58
    auto&& o(m_poDict->lookupNF(((char*)pszKey)));
    if (!o.isNull())
    {
        GDALPDFObjectNum nRefNum;
        int nRefGen = 0;
        if( o.isRef())
        {
            nRefNum = o.getRefNum();
            nRefGen = o.getRefGen();
            Object o2(m_poDict->lookup((char*)pszKey));
            if( !o2.isNull() )
            {
                GDALPDFObjectPoppler* poObj = new GDALPDFObjectPoppler(new Object(std::move(o2)), TRUE);
                poObj->SetRefNumAndGen(nRefNum, nRefGen);
                m_map[pszKey] = poObj;
                return poObj;
            }
        }
        else
        {
            GDALPDFObjectPoppler* poObj = new GDALPDFObjectPoppler(new Object(o.copy()), TRUE);
            poObj->SetRefNumAndGen(nRefNum, nRefGen);
            m_map[pszKey] = poObj;
            return poObj;
        }
    }
    return nullptr;
#else
    Object* po = new Object;
    if (m_poDict->lookupNF((char*)pszKey, po) && !po->isNull())
    {
        GDALPDFObjectNum nRefNum;
        int nRefGen = 0;
        if( po->isRef())
        {
            nRefNum = po->getRefNum();
            nRefGen = po->getRefGen();
        }
        if( !po->isRef() || (m_poDict->lookup((char*)pszKey, po) && !po->isNull()) )
        {
            GDALPDFObjectPoppler* poObj = new GDALPDFObjectPoppler(po, TRUE);
            poObj->SetRefNumAndGen(nRefNum, nRefGen);
            m_map[pszKey] = poObj;
            return poObj;
        }
        else
        {
            delete po;
            return nullptr;
        }
    }
    else
    {
        delete po;
        return nullptr;
    }
#endif
}

/************************************************************************/
/*                                GetValues()                           */
/************************************************************************/

std::map<CPLString, GDALPDFObject*>& GDALPDFDictionaryPoppler::GetValues()
{
    int i = 0;
    int nLength = m_poDict->getLength();
    for(i=0;i<nLength;i++)
    {
        const char* pszKey = (const char*)m_poDict->getKey(i);
        Get(pszKey);
    }
    return m_map;
}

/************************************************************************/
/* ==================================================================== */
/*                          GDALPDFArrayPoppler                         */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           GDALPDFCreateArray()                       */
/************************************************************************/

GDALPDFArray* GDALPDFCreateArray(Array* array)
{
    return new GDALPDFArrayPoppler(array);
}

/************************************************************************/
/*                           ~GDALPDFArrayPoppler()                     */
/************************************************************************/

GDALPDFArrayPoppler::~GDALPDFArrayPoppler()
{
    for(size_t i=0;i<m_v.size();i++)
    {
        delete m_v[i];
    }
}

/************************************************************************/
/*                               GetLength()                            */
/************************************************************************/

int GDALPDFArrayPoppler::GetLength()
{
    return m_poArray->getLength();
}

/************************************************************************/
/*                                 Get()                                */
/************************************************************************/

GDALPDFObject* GDALPDFArrayPoppler::Get(int nIndex)
{
    if (nIndex < 0 || nIndex >= GetLength())
        return nullptr;

    int nOldSize = static_cast<int>(m_v.size());
    if (nIndex >= nOldSize)
    {
        m_v.resize(nIndex+1);
        for(int i=nOldSize;i<=nIndex;i++)
        {
            m_v[i] = nullptr;
        }
    }

    if (m_v[nIndex] != nullptr)
        return m_v[nIndex];

#if POPPLER_MAJOR_VERSION >= 1 || POPPLER_MINOR_VERSION >= 58
    auto&& o(m_poArray->getNF(nIndex));
    if( !o.isNull() )
    {
        GDALPDFObjectNum nRefNum;
        int nRefGen = 0;
        if( o.isRef())
        {
            nRefNum = o.getRefNum();
            nRefGen = o.getRefGen();
            Object o2(m_poArray->get(nIndex));
            if( !o2.isNull() )
            {
                GDALPDFObjectPoppler* poObj = new GDALPDFObjectPoppler(new Object(std::move(o2)), TRUE);
                poObj->SetRefNumAndGen(nRefNum, nRefGen);
                m_v[nIndex] = poObj;
                return poObj;
            }
        }
        else
        {
            GDALPDFObjectPoppler* poObj = new GDALPDFObjectPoppler(new Object(o.copy()), TRUE);
            poObj->SetRefNumAndGen(nRefNum, nRefGen);
            m_v[nIndex] = poObj;
            return poObj;
        }
    }
    return nullptr;
#else
    Object* po = new Object;
    if (m_poArray->getNF(nIndex, po))
    {
        GDALPDFObjectNum nRefNum;
        int nRefGen = 0;
        if( po->isRef())
        {
            nRefNum = po->getRefNum();
            nRefGen = po->getRefGen();
        }
        if( !po->isRef() || (m_poArray->get(nIndex, po)) )
        {
            GDALPDFObjectPoppler* poObj = new GDALPDFObjectPoppler(po, TRUE);
            poObj->SetRefNumAndGen(nRefNum, nRefGen);
            m_v[nIndex] = poObj;
            return poObj;
        }
        else
        {
            delete po;
            return nullptr;
        }
    }
    else
    {
        delete po;
        return nullptr;
    }
#endif
}

/************************************************************************/
/* ==================================================================== */
/*                          GDALPDFStreamPoppler                        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                               GetLength()                            */
/************************************************************************/

int GDALPDFStreamPoppler::GetLength()
{
    if (m_nLength >= 0)
        return m_nLength;

    m_poStream->reset();
    m_nLength = 0;
    while(m_poStream->getChar() != EOF)
        m_nLength ++;
    return m_nLength;
}

/************************************************************************/
/*                         GooStringToCharStart()                       */
/************************************************************************/

static char* GooStringToCharStart(GooString& gstr)
{
    auto nLength = gstr.getLength();
    if( nLength )
    {
        char* pszContent = (char*) VSIMalloc(nLength + 1);
        if (pszContent)
        {
#if (POPPLER_MAJOR_VERSION >= 1 || POPPLER_MINOR_VERSION >= 72)
            const char* srcStr = gstr.c_str();
#else
            const char* srcStr = gstr.getCString();
#endif
            memcpy(pszContent, srcStr, nLength);
            pszContent[nLength] = '\0';
        }
        return pszContent;
    }
    return nullptr;
}

/************************************************************************/
/*                               GetBytes()                             */
/************************************************************************/

char* GDALPDFStreamPoppler::GetBytes()
{
    GooString gstr;
    m_poStream->fillGooString(&gstr);
    m_nLength = gstr.getLength();
    return GooStringToCharStart(gstr);
}

/************************************************************************/
/*                            GetRawLength()                            */
/************************************************************************/

int GDALPDFStreamPoppler::GetRawLength()
{
    if (m_nRawLength >= 0)
        return m_nRawLength;

    auto undecodeStream = m_poStream->getUndecodedStream();
    undecodeStream->reset();
    m_nRawLength = 0;
    while(undecodeStream->getChar() != EOF)
        m_nRawLength ++;
    return m_nRawLength;
}

/************************************************************************/
/*                             GetRawBytes()                            */
/************************************************************************/

char* GDALPDFStreamPoppler::GetRawBytes()
{
    GooString gstr;
    auto undecodeStream = m_poStream->getUndecodedStream();
    undecodeStream->fillGooString(&gstr);
    m_nRawLength = gstr.getLength();
    return GooStringToCharStart(gstr);
}


#endif // HAVE_POPPLER

#ifdef HAVE_PODOFO

/************************************************************************/
/* ==================================================================== */
/*                         GDALPDFDictionaryPodofo                      */
/* ==================================================================== */
/************************************************************************/

class GDALPDFDictionaryPodofo: public GDALPDFDictionary
{
    private:
        PoDoFo::PdfDictionary* m_poDict;
        PoDoFo::PdfVecObjects& m_poObjects;
        std::map<CPLString, GDALPDFObject*> m_map;

    public:
        GDALPDFDictionaryPodofo(PoDoFo::PdfDictionary* poDict, PoDoFo::PdfVecObjects& poObjects) : m_poDict(poDict), m_poObjects(poObjects) {}
        virtual ~GDALPDFDictionaryPodofo();

        virtual GDALPDFObject* Get(const char* pszKey) override;
        virtual std::map<CPLString, GDALPDFObject*>& GetValues() override;
};

/************************************************************************/
/* ==================================================================== */
/*                           GDALPDFArrayPodofo                         */
/* ==================================================================== */
/************************************************************************/

class GDALPDFArrayPodofo : public GDALPDFArray
{
    private:
        PoDoFo::PdfArray* m_poArray;
        PoDoFo::PdfVecObjects& m_poObjects;
        std::vector<GDALPDFObject*> m_v;

    public:
        GDALPDFArrayPodofo(PoDoFo::PdfArray* poArray, PoDoFo::PdfVecObjects& poObjects) : m_poArray(poArray), m_poObjects(poObjects) {}
        virtual ~GDALPDFArrayPodofo();

        virtual int GetLength() override;
        virtual GDALPDFObject* Get(int nIndex) override;
};

/************************************************************************/
/* ==================================================================== */
/*                          GDALPDFStreamPodofo                         */
/* ==================================================================== */
/************************************************************************/

class GDALPDFStreamPodofo : public GDALPDFStream
{
    private:
        PoDoFo::PdfStream* m_pStream;

    public:
        GDALPDFStreamPodofo(PoDoFo::PdfStream* pStream) : m_pStream(pStream) { }
        virtual ~GDALPDFStreamPodofo() { }

        virtual int GetLength() override;
        virtual char* GetBytes() override;

        virtual int GetRawLength() override;
        virtual char* GetRawBytes() override;
};

/************************************************************************/
/* ==================================================================== */
/*                          GDALPDFObjectPodofo                         */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          GDALPDFObjectPodofo()                       */
/************************************************************************/

GDALPDFObjectPodofo::GDALPDFObjectPodofo(PoDoFo::PdfObject* po,
                                         PoDoFo::PdfVecObjects& poObjects) :
    m_po(po),
    m_poObjects(poObjects),
    m_poDict(nullptr),
    m_poArray(nullptr),
    m_poStream(nullptr)
{
    try
    {
        if (m_po->GetDataType() == PoDoFo::ePdfDataType_Reference)
        {
            PoDoFo::PdfObject* poObj = m_poObjects.GetObject(m_po->GetReference());
            if (poObj)
                m_po = poObj;
        }
    }
    catch(PoDoFo::PdfError& oError)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : %s", oError.what());
    }
}

/************************************************************************/
/*                         ~GDALPDFObjectPodofo()                       */
/************************************************************************/

GDALPDFObjectPodofo::~GDALPDFObjectPodofo()
{
    delete m_poDict;
    delete m_poArray;
    delete m_poStream;
}

/************************************************************************/
/*                               GetType()                              */
/************************************************************************/

GDALPDFObjectType GDALPDFObjectPodofo::GetType()
{
    try
    {
        switch(m_po->GetDataType())
        {
            case PoDoFo::ePdfDataType_Null:       return PDFObjectType_Null;
            case PoDoFo::ePdfDataType_Bool:       return PDFObjectType_Bool;
            case PoDoFo::ePdfDataType_Number:     return PDFObjectType_Int;
            case PoDoFo::ePdfDataType_Real:       return PDFObjectType_Real;
            case PoDoFo::ePdfDataType_HexString:  return PDFObjectType_String;
            case PoDoFo::ePdfDataType_String:     return PDFObjectType_String;
            case PoDoFo::ePdfDataType_Name:       return PDFObjectType_Name;
            case PoDoFo::ePdfDataType_Array:      return PDFObjectType_Array;
            case PoDoFo::ePdfDataType_Dictionary: return PDFObjectType_Dictionary;
            default:                              return PDFObjectType_Unknown;
        }
    }
    catch(PoDoFo::PdfError& oError)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : %s", oError.what());
        return PDFObjectType_Unknown;
    }
}

/************************************************************************/
/*                          GetTypeNameNative()                         */
/************************************************************************/

const char* GDALPDFObjectPodofo::GetTypeNameNative()
{
    try
    {
        return m_po->GetDataTypeString();
    }
    catch(PoDoFo::PdfError& oError)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid PDF : %s", oError.what());
        return "unknown";
    }
}

/************************************************************************/
/*                              GetBool()                               */
/************************************************************************/

int GDALPDFObjectPodofo::GetBool()
{
    if (m_po->GetDataType() == PoDoFo::ePdfDataType_Bool)
        return m_po->GetBool();
    else
        return 0;
}

/************************************************************************/
/*                              GetInt()                                */
/************************************************************************/

int GDALPDFObjectPodofo::GetInt()
{
    if (m_po->GetDataType() == PoDoFo::ePdfDataType_Number)
        return static_cast<int>(m_po->GetNumber());
    else
        return 0;
}

/************************************************************************/
/*                              GetReal()                               */
/************************************************************************/

double GDALPDFObjectPodofo::GetReal()
{
    if (GetType() == PDFObjectType_Real)
        return m_po->GetReal();
    else
        return 0.0;
}

/************************************************************************/
/*                              GetString()                             */
/************************************************************************/

const CPLString& GDALPDFObjectPodofo::GetString()
{
    if (GetType() == PDFObjectType_String)
        return (osStr = m_po->GetString().GetStringUtf8());
    else
        return (osStr = "");
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const CPLString&  GDALPDFObjectPodofo::GetName()
{
    if (GetType() == PDFObjectType_Name)
        return (osStr = m_po->GetName().GetName());
    else
        return (osStr = "");
}

/************************************************************************/
/*                             GetDictionary()                          */
/************************************************************************/

GDALPDFDictionary* GDALPDFObjectPodofo::GetDictionary()
{
    if (GetType() != PDFObjectType_Dictionary)
        return nullptr;

    if (m_poDict)
        return m_poDict;

    m_poDict = new GDALPDFDictionaryPodofo(&m_po->GetDictionary(), m_poObjects);
    return m_poDict;
}

/************************************************************************/
/*                                GetArray()                            */
/************************************************************************/

GDALPDFArray* GDALPDFObjectPodofo::GetArray()
{
    if (GetType() != PDFObjectType_Array)
        return nullptr;

    if (m_poArray)
        return m_poArray;

    m_poArray = new GDALPDFArrayPodofo(&m_po->GetArray(), m_poObjects);
    return m_poArray;
}

/************************************************************************/
/*                               GetStream()                            */
/************************************************************************/

GDALPDFStream* GDALPDFObjectPodofo::GetStream()
{
    try
    {
        if (!m_po->HasStream())
            return nullptr;
    }
    catch(PoDoFo::PdfError& oError)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid object : %s", oError.what());
        return nullptr;
    }
    catch(...)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid object");
        return nullptr;
    }

    if (m_poStream == nullptr)
        m_poStream = new GDALPDFStreamPodofo(m_po->GetStream());
    return m_poStream;
}

/************************************************************************/
/*                               GetRefNum()                            */
/************************************************************************/

GDALPDFObjectNum GDALPDFObjectPodofo::GetRefNum()
{
    return GDALPDFObjectNum(m_po->Reference().ObjectNumber());
}

/************************************************************************/
/*                               GetRefGen()                            */
/************************************************************************/

int GDALPDFObjectPodofo::GetRefGen()
{
    return m_po->Reference().GenerationNumber();
}

/************************************************************************/
/* ==================================================================== */
/*                         GDALPDFDictionaryPodofo                      */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         ~GDALPDFDictionaryPodofo()                   */
/************************************************************************/

GDALPDFDictionaryPodofo::~GDALPDFDictionaryPodofo()
{
    std::map<CPLString, GDALPDFObject*>::iterator oIter = m_map.begin();
    std::map<CPLString, GDALPDFObject*>::iterator oEnd = m_map.end();
    for(; oIter != oEnd; ++oIter)
        delete oIter->second;
}

/************************************************************************/
/*                                  Get()                               */
/************************************************************************/

GDALPDFObject* GDALPDFDictionaryPodofo::Get(const char* pszKey)
{
    std::map<CPLString, GDALPDFObject*>::iterator oIter = m_map.find(pszKey);
    if (oIter != m_map.end())
        return oIter->second;

    PoDoFo::PdfObject* poVal = m_poDict->GetKey(PoDoFo::PdfName(pszKey));
    if (poVal)
    {
         GDALPDFObjectPodofo* poObj = new GDALPDFObjectPodofo(poVal, m_poObjects);
         m_map[pszKey] = poObj;
         return poObj;
    }
    else
    {
        return nullptr;
    }
}

/************************************************************************/
/*                              GetValues()                             */
/************************************************************************/

std::map<CPLString, GDALPDFObject*>& GDALPDFDictionaryPodofo::GetValues()
{
    PoDoFo::TKeyMap::iterator oIter = m_poDict->GetKeys().begin();
    PoDoFo::TKeyMap::iterator oEnd = m_poDict->GetKeys().end();
    for( ; oIter != oEnd; ++oIter)
    {
        const char* pszKey = oIter->first.GetName().c_str();
        Get(pszKey);
    }

    return m_map;
}

/************************************************************************/
/* ==================================================================== */
/*                           GDALPDFArrayPodofo                         */
/* ==================================================================== */
/************************************************************************/

GDALPDFArrayPodofo::~GDALPDFArrayPodofo()
{
    for(size_t i=0;i<m_v.size();i++)
    {
        delete m_v[i];
    }
}

/************************************************************************/
/*                              GetLength()                             */
/************************************************************************/

int GDALPDFArrayPodofo::GetLength()
{
    return static_cast<int>(m_poArray->GetSize());
}

/************************************************************************/
/*                                Get()                                 */
/************************************************************************/

GDALPDFObject* GDALPDFArrayPodofo::Get(int nIndex)
{
    if (nIndex < 0 || nIndex >= GetLength())
        return nullptr;

    int nOldSize = static_cast<int>(m_v.size());
    if (nIndex >= nOldSize)
    {
        m_v.resize(nIndex+1);
        for(int i=nOldSize;i<=nIndex;i++)
        {
            m_v[i] = nullptr;
        }
    }

    if (m_v[nIndex] != nullptr)
        return m_v[nIndex];

    PoDoFo::PdfObject& oVal = (*m_poArray)[nIndex];
    GDALPDFObjectPodofo* poObj = new GDALPDFObjectPodofo(&oVal, m_poObjects);
    m_v[nIndex] = poObj;
    return poObj;
}

/************************************************************************/
/* ==================================================================== */
/*                           GDALPDFStreamPodofo                        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                              GetLength()                             */
/************************************************************************/

int GDALPDFStreamPodofo::GetLength()
{
    char* pBuffer = nullptr;
    PoDoFo::pdf_long nLen = 0;
    try
    {
        m_pStream->GetFilteredCopy( &pBuffer, &nLen );
        PoDoFo::podofo_free(pBuffer);
        return (int)nLen;
    }
    catch( PoDoFo::PdfError & e )
    {
    }
    return 0;
}

/************************************************************************/
/*                               GetBytes()                             */
/************************************************************************/

char* GDALPDFStreamPodofo::GetBytes()
{
    char* pBuffer = nullptr;
    PoDoFo::pdf_long nLen = 0;
    try
    {
        m_pStream->GetFilteredCopy( &pBuffer, &nLen );
    }
    catch( PoDoFo::PdfError & e )
    {
        return nullptr;
    }
    char* pszContent = (char*) VSIMalloc(nLen + 1);
    if (!pszContent)
    {
        PoDoFo::podofo_free(pBuffer);
        return nullptr;
    }
    memcpy(pszContent, pBuffer, nLen);
    PoDoFo::podofo_free(pBuffer);
    pszContent[nLen] = '\0';
    return pszContent;
}

/************************************************************************/
/*                             GetRawLength()                           */
/************************************************************************/

int GDALPDFStreamPodofo::GetRawLength()
{
    try
    {
        auto nLen = m_pStream->GetLength();
        return (int)nLen;
    }
    catch( PoDoFo::PdfError & e )
    {
    }
    return 0;
}

/************************************************************************/
/*                              GetRawBytes()                           */
/************************************************************************/

char* GDALPDFStreamPodofo::GetRawBytes()
{
    char* pBuffer = nullptr;
    PoDoFo::pdf_long nLen = 0;
    try
    {
        m_pStream->GetCopy( &pBuffer, &nLen );
    }
    catch( PoDoFo::PdfError & e )
    {
        return nullptr;
    }
    char* pszContent = (char*) VSIMalloc(nLen + 1);
    if (!pszContent)
    {
        PoDoFo::podofo_free(pBuffer);
        return nullptr;
    }
    memcpy(pszContent, pBuffer, nLen);
    PoDoFo::podofo_free(pBuffer);
    pszContent[nLen] = '\0';
    return pszContent;
}

#endif // HAVE_PODOFO

#ifdef HAVE_PDFIUM

/************************************************************************/
/* ==================================================================== */
/*                         GDALPDFDictionaryPdfium                      */
/* ==================================================================== */
/************************************************************************/

class GDALPDFDictionaryPdfium: public GDALPDFDictionary
{
    private:
        CPDF_Dictionary* m_poDict;
        std::map<CPLString, GDALPDFObject*> m_map;

    public:
        GDALPDFDictionaryPdfium(CPDF_Dictionary* poDict) : m_poDict(poDict) {}
        virtual ~GDALPDFDictionaryPdfium();

        virtual GDALPDFObject* Get(const char* pszKey) override;
        virtual std::map<CPLString, GDALPDFObject*>& GetValues() override;
};

/************************************************************************/
/* ==================================================================== */
/*                           GDALPDFArrayPdfium                         */
/* ==================================================================== */
/************************************************************************/

class GDALPDFArrayPdfium : public GDALPDFArray
{
    private:
        CPDF_Array* m_poArray;
        std::vector<GDALPDFObject*> m_v;

    public:
        GDALPDFArrayPdfium(CPDF_Array* poArray) : m_poArray(poArray) {}
        virtual ~GDALPDFArrayPdfium();

        virtual int GetLength() override;
        virtual GDALPDFObject* Get(int nIndex) override;
};

/************************************************************************/
/* ==================================================================== */
/*                          GDALPDFStreamPdfium                         */
/* ==================================================================== */
/************************************************************************/

class GDALPDFStreamPdfium : public GDALPDFStream
{
    private:
        CPDF_Stream* m_pStream;
        int m_nSize = 0;
        std::unique_ptr<uint8_t, CPLFreeReleaser> m_pData = nullptr;
        int m_nRawSize = 0;
        std::unique_ptr<uint8_t, CPLFreeReleaser> m_pRawData = nullptr;

        void Decompress();
        void FillRaw();

    public:
        GDALPDFStreamPdfium( CPDF_Stream* pStream ) :
            m_pStream(pStream) {}
        virtual ~GDALPDFStreamPdfium() {}

        virtual int GetLength() override;
        virtual char* GetBytes() override;

        virtual int GetRawLength() override;
        virtual char* GetRawBytes() override;
};

/************************************************************************/
/* ==================================================================== */
/*                          GDALPDFObjectPdfium                         */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          GDALPDFObjectPdfium()                       */
/************************************************************************/

GDALPDFObjectPdfium::GDALPDFObjectPdfium( CPDF_Object *po ) :
    m_po(po),
    m_poDict(nullptr),
    m_poArray(nullptr),
    m_poStream(nullptr)
{
    CPLAssert(m_po != nullptr);
}

/************************************************************************/
/*                         ~GDALPDFObjectPdfium()                       */
/************************************************************************/

GDALPDFObjectPdfium::~GDALPDFObjectPdfium()
{
    delete m_poDict;
    delete m_poArray;
    delete m_poStream;
}

/************************************************************************/
/*                               Build()                                */
/************************************************************************/

GDALPDFObjectPdfium* GDALPDFObjectPdfium::Build(CPDF_Object *poVal)
{
    if( poVal == nullptr )
        return nullptr;
    if( poVal->GetType() == CPDF_Object::Type::kReference )
    {
        poVal = poVal->GetDirect();
        if( poVal == nullptr )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot resolve indirect object");
            return nullptr;
        }
    }
    return new GDALPDFObjectPdfium(poVal);
}

/************************************************************************/
/*                               GetType()                              */
/************************************************************************/

GDALPDFObjectType GDALPDFObjectPdfium::GetType()
{
    switch(m_po->GetType())
    {
        case CPDF_Object::Type::kNullobj:                  return PDFObjectType_Null;
        case CPDF_Object::Type::kBoolean:                  return PDFObjectType_Bool;
        case CPDF_Object::Type::kNumber:
          return (reinterpret_cast<CPDF_Number*>(m_po))->IsInteger()
              ? PDFObjectType_Int
              : PDFObjectType_Real;
        case CPDF_Object::Type::kString:                   return PDFObjectType_String;
        case CPDF_Object::Type::kName:                     return PDFObjectType_Name;
        case CPDF_Object::Type::kArray:                    return PDFObjectType_Array;
        case CPDF_Object::Type::kDictionary:               return PDFObjectType_Dictionary;
        case CPDF_Object::Type::kStream:                   return PDFObjectType_Dictionary;
        case CPDF_Object::Type::kReference:
            // unresolved reference
            return PDFObjectType_Unknown;
        default:
          CPLAssert(false);
          return PDFObjectType_Unknown;
    }
}

/************************************************************************/
/*                          GetTypeNameNative()                         */
/************************************************************************/

const char* GDALPDFObjectPdfium::GetTypeNameNative()
{
    if(m_po->GetType() == CPDF_Object::Type::kStream)
      return "stream";
    else
      return "";
}

/************************************************************************/
/*                              GetBool()                               */
/************************************************************************/

int GDALPDFObjectPdfium::GetBool()
{
    return m_po->GetInteger();
}

/************************************************************************/
/*                              GetInt()                                */
/************************************************************************/

int GDALPDFObjectPdfium::GetInt()
{
    return m_po->GetInteger();
}

/************************************************************************/
/*                       CPLRoundToMoreLikelyDouble()                   */
/************************************************************************/

// We try to compensate for rounding errors when converting the number
// in the PDF expressed as a string (e.g 297.84) to float32 by pdfium : 297.8399963378906
// Which is technically correct per the PDF spec, but in practice poppler or podofo use double
// and Geospatial PDF are often encoded with double precision.

static double CPLRoundToMoreLikelyDouble(float f)
{
    if( (float)(int)f == f )
        return f;

    char szBuffer[80];
    CPLsnprintf(szBuffer, 80, "%f\n", f);
    double d = f;
    char* pszDot = strchr(szBuffer, '.');
    if( pszDot == nullptr )
        return d;
    pszDot ++;
    if( pszDot[0] == 0 || pszDot[1] == 0 )
        return d;
    if( STARTS_WITH(pszDot + 2, "99") )
    {
        pszDot[2] = 0;
        double d2 = CPLAtof(szBuffer) + 0.01;
        float f2 = (float)d2;
        if( f == f2 || nextafterf(f,f+1.0f) == f2 || nextafterf(f,f-1.0f) == f2 )
            d = d2;
    }
    else if( STARTS_WITH(pszDot + 2, "00") )
    {
        pszDot[2] = 0;
        double d2 = CPLAtof(szBuffer);
        float f2 = (float)d2;
        if( f == f2 || nextafterf(f,f+1.0f) == f2 || nextafterf(f,f-1.0f) == f2 )
            d = d2;
    }
    return d;
}

/************************************************************************/
/*                              GetReal()                               */
/************************************************************************/

double GDALPDFObjectPdfium::GetReal()
{
    return CPLRoundToMoreLikelyDouble(m_po->GetNumber());
}

/************************************************************************/
/*                              GetString()                             */
/************************************************************************/

const CPLString& GDALPDFObjectPdfium::GetString()
{
    if (GetType() == PDFObjectType_String) {
        const auto bs = m_po->GetString();
        // If empty string, code crashes
        if(bs.IsEmpty())
          return (osStr = "");
        return (osStr = GDALPDFGetUTF8StringFromBytes(static_cast<const GByte*>(bs.raw_str()),
                                                      static_cast<int>(bs.GetLength())));
    }
    else
        return (osStr = "");
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const CPLString&  GDALPDFObjectPdfium::GetName()
{
    if (GetType() == PDFObjectType_Name)
        return (osStr = m_po->GetString().c_str());
    else
        return (osStr = "");
}

/************************************************************************/
/*                             GetDictionary()                          */
/************************************************************************/

GDALPDFDictionary* GDALPDFObjectPdfium::GetDictionary()
{
    if (GetType() != PDFObjectType_Dictionary)
        return nullptr;

    if (m_poDict)
        return m_poDict;

    m_poDict = new GDALPDFDictionaryPdfium(m_po->GetDict());
    return m_poDict;
}

/************************************************************************/
/*                                GetArray()                            */
/************************************************************************/

GDALPDFArray* GDALPDFObjectPdfium::GetArray()
{
    if (GetType() != PDFObjectType_Array)
        return nullptr;

    if (m_poArray)
        return m_poArray;

    m_poArray = new GDALPDFArrayPdfium(reinterpret_cast<CPDF_Array*>(m_po));
    return m_poArray;
}

/************************************************************************/
/*                               GetStream()                            */
/************************************************************************/

GDALPDFStream* GDALPDFObjectPdfium::GetStream()
{
    if (m_po->GetType() != CPDF_Object::Type::kStream)
        return nullptr;

    if (m_poStream)
        return m_poStream;
    CPDF_Stream* pStream = reinterpret_cast<CPDF_Stream*>(m_po);
    if (pStream)
    {
        m_poStream = new GDALPDFStreamPdfium(pStream);
        return m_poStream;
    }
    else
        return nullptr;
}

/************************************************************************/
/*                               GetRefNum()                            */
/************************************************************************/

GDALPDFObjectNum GDALPDFObjectPdfium::GetRefNum()
{
    return GDALPDFObjectNum(m_po->GetObjNum());
}

/************************************************************************/
/*                               GetRefGen()                            */
/************************************************************************/

int GDALPDFObjectPdfium::GetRefGen()
{
    return m_po->GetGenNum();
}

/************************************************************************/
/* ==================================================================== */
/*                         GDALPDFDictionaryPdfium                      */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                         ~GDALPDFDictionaryPdfium()                   */
/************************************************************************/

GDALPDFDictionaryPdfium::~GDALPDFDictionaryPdfium()
{
    std::map<CPLString, GDALPDFObject*>::iterator oIter = m_map.begin();
    std::map<CPLString, GDALPDFObject*>::iterator oEnd = m_map.end();
    for(; oIter != oEnd; ++oIter)
        delete oIter->second;
}

/************************************************************************/
/*                                  Get()                               */
/************************************************************************/

GDALPDFObject* GDALPDFDictionaryPdfium::Get(const char* pszKey)
{
    std::map<CPLString, GDALPDFObject*>::iterator oIter = m_map.find(pszKey);
    if (oIter != m_map.end())
        return oIter->second;

    ByteString pdfiumKey(pszKey);
    CPDF_Object* poVal = m_poDict->GetObjectFor(pdfiumKey);
    GDALPDFObjectPdfium* poObj = GDALPDFObjectPdfium::Build(poVal);
    if (poObj)
    {
        m_map[pszKey] = poObj;
        return poObj;
    }
    else
    {
        return nullptr;
    }
}

/************************************************************************/
/*                              GetValues()                             */
/************************************************************************/

std::map<CPLString, GDALPDFObject*>& GDALPDFDictionaryPdfium::GetValues()
{
    CPDF_DictionaryLocker dictIterator(m_poDict);
    for( const auto iter: dictIterator )
    {
        // No object for this key
        if( !iter.second )
            continue;

        const char* pszKey = iter.first.c_str();
        // Objects exists in the map
        if(m_map.find(pszKey) != m_map.end())
          continue;
        GDALPDFObjectPdfium* poObj = GDALPDFObjectPdfium::Build(iter.second.Get());
        if( poObj == nullptr )
            continue;
        m_map[pszKey] = poObj;
    }

    return m_map;
}

/************************************************************************/
/* ==================================================================== */
/*                           GDALPDFArrayPdfium                         */
/* ==================================================================== */
/************************************************************************/

GDALPDFArrayPdfium::~GDALPDFArrayPdfium()
{
    for(size_t i=0;i<m_v.size();i++)
    {
        delete m_v[i];
    }
}

/************************************************************************/
/*                              GetLength()                             */
/************************************************************************/

int GDALPDFArrayPdfium::GetLength()
{
    return static_cast<int>(m_poArray->size());
}

/************************************************************************/
/*                                Get()                                 */
/************************************************************************/

GDALPDFObject* GDALPDFArrayPdfium::Get(int nIndex)
{
    if (nIndex < 0 || nIndex >= GetLength())
        return nullptr;

    int nOldSize = static_cast<int>(m_v.size());
    if (nIndex >= nOldSize)
    {
        m_v.resize(nIndex+1);
        for(int i=nOldSize;i<=nIndex;i++)
        {
            m_v[i] = nullptr;
        }
    }

    if (m_v[nIndex] != nullptr)
        return m_v[nIndex];

    CPDF_Object* poVal = m_poArray->GetObjectAt(nIndex);
    GDALPDFObjectPdfium* poObj = GDALPDFObjectPdfium::Build(poVal);
    if( poObj == nullptr )
        return nullptr;
    m_v[nIndex] = poObj;
    return poObj;
}

/************************************************************************/
/* ==================================================================== */
/*                           GDALPDFStreamPdfium                        */
/* ==================================================================== */
/************************************************************************/

void GDALPDFStreamPdfium::Decompress()
{
    if( m_pData != nullptr )
        return;
    auto acc(pdfium::MakeRetain<CPDF_StreamAcc>(m_pStream));
    acc->LoadAllDataFiltered();
    m_nSize = static_cast<int>(acc->GetSize());
    m_pData.reset();
    // We don't use m_pData->Detach() as we don't want to deal with
    // std::unique_ptr<uint8_t, FxFreeDeleter>, and FxFreeDeleter behavior
    // depends on whether GDAL and pdfium are compiled with the same
    // NDEBUG and DCHECK_ALWAYS_ON settings
    if( m_nSize )
    {
        m_pData.reset(static_cast<uint8_t*>(CPLMalloc(m_nSize)));
        memcpy(&m_pData.get()[0], acc->GetData(), m_nSize);
    }
}

/************************************************************************/
/*                              GetLength()                             */
/************************************************************************/

int GDALPDFStreamPdfium::GetLength()
{
    Decompress();
    return m_nSize;
}

/************************************************************************/
/*                               GetBytes()                             */
/************************************************************************/

char* GDALPDFStreamPdfium::GetBytes()
{
    int nLength = GetLength();
    if(nLength == 0)
      return nullptr;
    char* pszContent = (char*) VSIMalloc(sizeof(char)*(nLength + 1));
    if (!pszContent)
        return nullptr;
    memcpy( pszContent, m_pData.get(), nLength);
    pszContent[nLength] = '\0';
    return pszContent;
}

/************************************************************************/
/*                                FillRaw()                             */
/************************************************************************/

void GDALPDFStreamPdfium::FillRaw()
{
    if( m_pRawData != nullptr )
        return;
    auto acc(pdfium::MakeRetain<CPDF_StreamAcc>(m_pStream));
    acc->LoadAllDataRaw();
    m_nRawSize = static_cast<int>(acc->GetSize());
    m_pRawData.reset();
    // We don't use m_pData->Detach() as we don't want to deal with
    // std::unique_ptr<uint8_t, FxFreeDeleter>, and FxFreeDeleter behavior
    // depends on whether GDAL and pdfium are compiled with the same
    // NDEBUG and DCHECK_ALWAYS_ON settings
    if( m_nRawSize )
    {
        m_pRawData.reset(static_cast<uint8_t*>(CPLMalloc(m_nRawSize)));
        memcpy(&m_pRawData.get()[0], acc->GetData(), m_nRawSize);
    }
}

/************************************************************************/
/*                            GetRawLength()                            */
/************************************************************************/

int GDALPDFStreamPdfium::GetRawLength()
{
    FillRaw();
    return m_nRawSize;
}

/************************************************************************/
/*                             GetRawBytes()                            */
/************************************************************************/

char* GDALPDFStreamPdfium::GetRawBytes()
{
    int nLength = GetRawLength();
    if(nLength == 0)
      return nullptr;
    char* pszContent = (char*) VSIMalloc(sizeof(char)*(nLength + 1));
    if (!pszContent)
        return nullptr;
    memcpy( pszContent, m_pRawData.get(), nLength);
    pszContent[nLength] = '\0';
    return pszContent;
}


#endif // HAVE_PDFIUM
