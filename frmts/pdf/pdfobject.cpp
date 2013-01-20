/******************************************************************************
 * $Id$
 *
 * Project:  PDF driver
 * Purpose:  GDALDataset driver for PDF dataset.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011, Even Rouault
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

/* hack for PDF driver and poppler >= 0.15.0 that defines incompatible "typedef bool GBool" */
/* in include/poppler/goo/gtypes.h with the one defined in cpl_port.h */
#define CPL_GBOOL_DEFINED

#include <vector>
#include "pdfobject.h"

CPL_CVSID("$Id$");

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
            osStr += CPLSPrintf("%02X", (pwszDest[i] >> 8) & 0xff);
            osStr += CPLSPrintf("%02X", (pwszDest[i]) & 0xff);
        }
    }
    osStr += ">";
    CPLFree(pwszDest);
    return osStr;
}

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
        return NULL;
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

void GDALPDFObject::Serialize(CPLString& osStr)
{
    int nRefNum = GetRefNum();
    if( nRefNum )
    {
        int nRefGen = GetRefGen();
        osStr.append(CPLSPrintf("%d %d R", nRefNum, nRefGen));
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
            if (dfReal == (double)(int)dfReal)
                sprintf(szReal, "%d", (int)dfReal);
            else if (CanRepresentRealAsString())
            {
                /* Used for OGC BP numeric values */
                sprintf(szReal, "(%.16g)", dfReal);
            }
            else
            {
                sprintf(szReal, "%.16f", dfReal);

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
        default: fprintf(stderr, "Serializing unknown object !\n"); return;
    }
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

GDALPDFObjectRW* GDALPDFObject::Clone()
{
    int nRefNum = GetRefNum();
    if( nRefNum )
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
        default: fprintf(stderr, "Cloning unknown object !\n"); return NULL;
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
    GDALPDFObject* poCurObj = NULL;
    char** papszTokens = CSLTokenizeString2(pszPath, ".", 0);
    for(int i=0; papszTokens[i] != NULL; i++)
    {
        int iElt = -1;
        char* pszBracket = strchr(papszTokens[i], '[');
        if( pszBracket != NULL )
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
                poCurObj = NULL;
                break;
            }
            poCurObj = poCurObj->GetDictionary()->Get(papszTokens[i]);
        }

        if( poCurObj == NULL )
        {
            poCurObj = NULL;
            break;
        }

        if( iElt >= 0 )
        {
            if( poCurObj->GetType() != PDFObjectType_Array )
            {
                poCurObj = NULL;
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

GDALPDFObjectRW::GDALPDFObjectRW(GDALPDFObjectType eType)
{
    m_eType = eType;
    m_nVal = 0;
    m_dfVal = 0.0;
    //m_osVal;
    m_poDict = NULL;
    m_poArray = NULL;
    m_nNum = 0;
    m_nGen = 0;
    m_bCanRepresentRealAsString = FALSE;
}

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

GDALPDFObjectRW* GDALPDFObjectRW::CreateIndirect(int nNum, int nGen)
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
    fprintf(stderr, "Should not go here");
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
    return NULL;
}

/************************************************************************/
/*                              GetRefNum()                             */
/************************************************************************/

int GDALPDFObjectRW::GetRefNum()
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

GDALPDFDictionaryRW::GDALPDFDictionaryRW()
{
}

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
    return NULL;
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

GDALPDFArrayRW::GDALPDFArrayRW()
{
}

/************************************************************************/
/*                            ~GDALPDFArrayRW()                         */
/************************************************************************/

GDALPDFArrayRW::~GDALPDFArrayRW()
{
    for(int i=0; i < (int)m_array.size(); i++)
        delete m_array[i];
}

/************************************************************************/
/*                               GetLength()                             */
/************************************************************************/

int GDALPDFArrayRW::GetLength()
{
    return (int)m_array.size();
}

/************************************************************************/
/*                                  Get()                               */
/************************************************************************/

GDALPDFObject* GDALPDFArrayRW::Get(int nIndex)
{
    if (nIndex < 0 || nIndex >= GetLength())
        return NULL;
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

        virtual GDALPDFObject* Get(const char* pszKey);
        virtual std::map<CPLString, GDALPDFObject*>& GetValues();
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

        virtual int GetLength();
        virtual GDALPDFObject* Get(int nIndex);
};

/************************************************************************/
/* ==================================================================== */
/*                           GDALPDFStreamPoppler                       */
/* ==================================================================== */
/************************************************************************/

class GDALPDFStreamPoppler : public GDALPDFStream
{
    private:
        int     m_nLength;
        Stream* m_poStream;

    public:
        GDALPDFStreamPoppler(Stream* poStream) : m_nLength(-1), m_poStream(poStream) {}
        virtual ~GDALPDFStreamPoppler() {}

        virtual int GetLength();
        virtual char* GetBytes();
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
    m_po->free();
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
/*                         GDALPDFPopplerGetUTF8()                      */
/************************************************************************/

static CPLString GDALPDFPopplerGetUTF8(GooString* poStr)
{
    GByte* pabySrc = (GByte*)poStr->getCString();
    int nLen = poStr->getLength();
    int bBEUnicodeMarker = nLen > 2 && pabySrc[0] == 0xFF && pabySrc[1] == 0xFE;
    if (!poStr->hasUnicodeMarker() && !bBEUnicodeMarker)
    {
        const char* pszStr = poStr->getCString();
        if (CPLIsUTF8(pszStr, -1))
            return pszStr;
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

/************************************************************************/
/*                              GetString()                             */
/************************************************************************/

const CPLString& GDALPDFObjectPoppler::GetString()
{
    if (GetType() == PDFObjectType_String)
        return (osStr = GDALPDFPopplerGetUTF8(m_po->getString()));
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
        return NULL;

    if (m_poDict)
        return m_poDict;

    Dict* poDict = (m_po->getType() == objStream) ? m_po->getStream()->getDict() : m_po->getDict();
    if (poDict == NULL)
        return NULL;
    m_poDict = new GDALPDFDictionaryPoppler(poDict);
    return m_poDict;
}

/************************************************************************/
/*                              GetArray()                              */
/************************************************************************/

GDALPDFArray* GDALPDFObjectPoppler::GetArray()
{
    if (GetType() != PDFObjectType_Array)
        return NULL;

    if (m_poArray)
        return m_poArray;

    Array* poArray = m_po->getArray();
    if (poArray == NULL)
        return NULL;
    m_poArray = new GDALPDFArrayPoppler(poArray);
    return m_poArray;
}

/************************************************************************/
/*                             GetStream()                              */
/************************************************************************/

GDALPDFStream* GDALPDFObjectPoppler::GetStream()
{
    if (m_po->getType() != objStream)
        return NULL;

    if (m_poStream)
        return m_poStream;
    m_poStream = new GDALPDFStreamPoppler(m_po->getStream());
    return m_poStream;
}

/************************************************************************/
/*                           SetRefNumAndGen()                          */
/************************************************************************/

void GDALPDFObjectPoppler::SetRefNumAndGen(int nNum, int nGen)
{
    m_nRefNum = nNum;
    m_nRefGen = nGen;
}

/************************************************************************/
/*                               GetRefNum()                            */
/************************************************************************/

int GDALPDFObjectPoppler::GetRefNum()
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

    Object* po = new Object;
    if (m_poDict->lookupNF((char*)pszKey, po) && !po->isNull())
    {
        int nRefNum = 0, nRefGen = 0;
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
            return NULL;
        }
    }
    else
    {
        delete po;
        return NULL;
    }
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
    for(int i=0;i<(int)m_v.size();i++)
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
        return NULL;

    int nOldSize = (int)m_v.size();
    if (nIndex >= nOldSize)
    {
        m_v.resize(nIndex+1);
        for(int i=nOldSize;i<=nIndex;i++)
        {
            m_v[i] = NULL;
        }
    }

    if (m_v[nIndex] != NULL)
        return m_v[nIndex];

    Object* po = new Object;
    if (m_poArray->getNF(nIndex, po))
    {
        int nRefNum = 0, nRefGen = 0;
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
            return NULL;
        }
    }
    else
    {
        delete po;
        return NULL;
    }
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
/*                               GetBytes()                             */
/************************************************************************/

char* GDALPDFStreamPoppler::GetBytes()
{
    /* fillGooString() available in poppler >= 0.16.0 */
#ifdef POPPLER_BASE_STREAM_HAS_TWO_ARGS
    GooString* gstr = new GooString();
    m_poStream->fillGooString(gstr);

    if( gstr->getLength() )
    {
        m_nLength = gstr->getLength();
        char* pszContent = (char*) VSIMalloc(m_nLength + 1);
        if (!pszContent)
            return NULL;
        memcpy(pszContent, gstr->getCString(), m_nLength);
        pszContent[m_nLength] = '\0';
        delete gstr;
        return pszContent;
    }
    else
    {
        delete gstr;
        return NULL;
    }
#else
    int i;
    int nLengthAlloc = 0;
    char* pszContent = NULL;
    if( m_nLength >= 0 )
    {
        pszContent = (char*) VSIMalloc(m_nLength + 1);
        if (!pszContent)
            return NULL;
        nLengthAlloc = m_nLength;
    }
    m_poStream->reset();
    for(i = 0; ; ++i )
    {
        int nVal = m_poStream->getChar();
        if (nVal == EOF)
            break;
        if( i >= nLengthAlloc )
        {
            nLengthAlloc = 32 + nLengthAlloc + nLengthAlloc / 3;
            char* pszContentNew = (char*) VSIRealloc(pszContent, nLengthAlloc + 1);
            if( pszContentNew == NULL )
            {
                CPLFree(pszContent);
                m_nLength = 0;
                return NULL;
            }
            pszContent = pszContentNew;
        }
        pszContent[i] = (GByte)nVal;
    }
    m_nLength = i;
    pszContent[i] = '\0';
    return pszContent;
#endif
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

        virtual GDALPDFObject* Get(const char* pszKey);
        virtual std::map<CPLString, GDALPDFObject*>& GetValues();
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

        virtual int GetLength();
        virtual GDALPDFObject* Get(int nIndex);
};

/************************************************************************/
/* ==================================================================== */
/*                          GDALPDFStreamPodofo                         */
/* ==================================================================== */
/************************************************************************/

class GDALPDFStreamPodofo : public GDALPDFStream
{
    private:
        int     m_nLength;
        PoDoFo::PdfMemStream* m_pStream;

    public:
        GDALPDFStreamPodofo(PoDoFo::PdfMemStream* pStream) : m_nLength(-1), m_pStream(pStream) { }
        virtual ~GDALPDFStreamPodofo() {}

        virtual int GetLength();
        virtual char* GetBytes();
};

/************************************************************************/
/* ==================================================================== */
/*                          GDALPDFObjectPodofo                         */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                          GDALPDFObjectPodofo()                       */
/************************************************************************/

GDALPDFObjectPodofo::GDALPDFObjectPodofo(PoDoFo::PdfObject* po, PoDoFo::PdfVecObjects& poObjects) :
        m_po(po), m_poObjects(poObjects), m_poDict(NULL), m_poArray(NULL), m_poStream(NULL)
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
        return (int)m_po->GetNumber();
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
        return NULL;

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
        return NULL;

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
            return NULL;
    }
    catch(PoDoFo::PdfError& oError)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid object : %s", oError.what());
        return NULL;
    }
    catch(...)
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Invalid object");
        return NULL;
    }

    if (m_poStream)
        return m_poStream;
    PoDoFo::PdfMemStream* pStream = NULL;
    try
    {
        pStream = dynamic_cast<PoDoFo::PdfMemStream*>(m_po->GetStream());
        pStream->Uncompress();
    }
    catch( const PoDoFo::PdfError & e )
    {
        e.PrintErrorMsg();
        pStream = NULL;
    }
    if (pStream)
    {
        m_poStream = new GDALPDFStreamPodofo(pStream);
        return m_poStream;
    }
    else
        return NULL;
}

/************************************************************************/
/*                               GetRefNum()                            */
/************************************************************************/

int GDALPDFObjectPodofo::GetRefNum()
{
    return m_po->Reference().ObjectNumber();
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
        return NULL;
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
    for(int i=0;i<(int)m_v.size();i++)
    {
        delete m_v[i];
    }
}

/************************************************************************/
/*                              GetLength()                             */
/************************************************************************/

int GDALPDFArrayPodofo::GetLength()
{
    return (int)m_poArray->GetSize();
}

/************************************************************************/
/*                                Get()                                 */
/************************************************************************/

GDALPDFObject* GDALPDFArrayPodofo::Get(int nIndex)
{
    if (nIndex < 0 || nIndex >= GetLength())
        return NULL;

    int nOldSize = (int)m_v.size();
    if (nIndex >= nOldSize)
    {
        m_v.resize(nIndex+1);
        for(int i=nOldSize;i<=nIndex;i++)
        {
            m_v[i] = NULL;
        }
    }

    if (m_v[nIndex] != NULL)
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
    return (int)m_pStream->GetLength();
}

/************************************************************************/
/*                               GetBytes()                             */
/************************************************************************/

char* GDALPDFStreamPodofo::GetBytes()
{
    int nLength = GetLength();
    char* pszContent = (char*) VSIMalloc(nLength + 1);
    if (!pszContent)
        return NULL;
    memcpy(pszContent, m_pStream->Get(), nLength);
    pszContent[nLength] = '\0';
    return pszContent;
}

#endif // HAVE_PODOFO
