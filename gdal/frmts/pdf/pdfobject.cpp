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

GDALPDFObject::~GDALPDFObject()
{
}

GDALPDFDictionary::~GDALPDFDictionary()
{
}

GDALPDFArray::~GDALPDFArray()
{
}

GDALPDFStream::~GDALPDFStream()
{
}

#ifdef USE_POPPLER

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

GDALPDFObjectPoppler::~GDALPDFObjectPoppler()
{
    m_po->free();
    if (m_bDestroy)
        delete m_po;
    delete m_poDict;
    delete m_poArray;
    delete m_poStream;
}

GDALPDFObjectType GDALPDFObjectPoppler::GetType()
{
    switch(m_po->getType())
    {
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

const char* GDALPDFObjectPoppler::GetTypeName()
{
    return m_po->getTypeName();
}

int GDALPDFObjectPoppler::GetInt()
{
    if (GetType() == PDFObjectType_Int)
        return m_po->getInt();
    else
        return 0;
}

double GDALPDFObjectPoppler::GetReal()
{
    if (GetType() == PDFObjectType_Real)
        return m_po->getReal();
    else
        return 0.0;
}

static CPLString GDALPDFPopplerGetUTF8(GooString* poStr)
{
    if (!poStr->hasUnicodeMarker())
        return poStr->getCString();

    GByte* pabySrc = ((GByte*)poStr->getCString()) + 2;
    int nLen = (poStr->getLength() - 2) / 2;
    wchar_t *pwszSource = new wchar_t[nLen + 1];
    for(int i=0; i<nLen; i++)
    {
        pwszSource[i] = (pabySrc[2 * i] << 8) + pabySrc[2 * i + 1];
    }
    pwszSource[nLen] = 0;

    char* pszUTF8 = CPLRecodeFromWChar( pwszSource, CPL_ENC_UCS2, CPL_ENC_UTF8 );
    delete[] pwszSource;
    CPLString osStrUTF8(pszUTF8);
    CPLFree(pszUTF8);
    return osStrUTF8;
}

const CPLString& GDALPDFObjectPoppler::GetString()
{
    if (GetType() == PDFObjectType_String)
        return (osStr = GDALPDFPopplerGetUTF8(m_po->getString()));
    else
        return (osStr = "");
}

const CPLString& GDALPDFObjectPoppler::GetName()
{
    if (GetType() == PDFObjectType_Name)
        return (osStr = m_po->getName());
    else
        return (osStr = "");
}

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

GDALPDFStream* GDALPDFObjectPoppler::GetStream()
{
    if (m_po->getType() != objStream)
        return NULL;

    if (m_poStream)
        return m_poStream;
    m_poStream = new GDALPDFStreamPoppler(m_po->getStream());
    return m_poStream;
}

GDALPDFDictionaryPoppler::~GDALPDFDictionaryPoppler()
{
    std::map<CPLString, GDALPDFObject*>::iterator oIter = m_map.begin();
    std::map<CPLString, GDALPDFObject*>::iterator oEnd = m_map.end();
    for(; oIter != oEnd; ++oIter)
        delete oIter->second;
}

GDALPDFObject* GDALPDFDictionaryPoppler::Get(const char* pszKey)
{
    std::map<CPLString, GDALPDFObject*>::iterator oIter = m_map.find(pszKey);
    if (oIter != m_map.end())
        return oIter->second;

    Object* po = new Object;
    if (m_poDict->lookup((char*)pszKey, po) && !po->isNull())
    {
         GDALPDFObjectPoppler* poObj = new GDALPDFObjectPoppler(po, TRUE);
         m_map[pszKey] = poObj;
         return poObj;
    }
    else
    {
        delete po;
        return NULL;
    }
}

std::map<CPLString, GDALPDFObject*>& GDALPDFDictionaryPoppler::GetValues()
{
    int i = 0;
    int nLength = m_poDict->getLength();
    for(i=0;i<nLength;i++)
    {
        Get((const char*)m_poDict->getKey(i));
    }
    return m_map;
}

GDALPDFArrayPoppler::~GDALPDFArrayPoppler()
{
    for(int i=0;i<(int)m_v.size();i++)
    {
        delete m_v[i];
    }
}

int GDALPDFArrayPoppler::GetLength()
{
    return m_poArray->getLength();
}

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
    if (m_poArray->get(nIndex, po) && !po->isNull())
    {
        GDALPDFObjectPoppler* poObj = new GDALPDFObjectPoppler(po, TRUE);
        m_v[nIndex] = poObj;
        return poObj;
    }
    else
    {
        delete po;
        return NULL;
    }
}

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

char* GDALPDFStreamPoppler::GetBytes()
{
    int i;
    int nLength = GetLength();
    char* pszContent = (char*) VSIMalloc(nLength + 1);
    if (!pszContent)
        return NULL;
    m_poStream->reset();
    for(i=0;i<nLength;i++)
    {
        int nVal = m_poStream->getChar();
        if (nVal == EOF)
            break;
        pszContent[i] = (GByte)nVal;
    }
    pszContent[i] = '\0';
    return pszContent;
}

#else

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

GDALPDFObjectPodofo::GDALPDFObjectPodofo(PoDoFo::PdfObject* po, PoDoFo::PdfVecObjects& poObjects) :
        m_po(po), m_poObjects(poObjects), m_poDict(NULL), m_poArray(NULL), m_poStream(NULL)
{
    if (m_po->GetDataType() == PoDoFo::ePdfDataType_Reference)
    {
        PoDoFo::PdfObject* poObj = m_poObjects.GetObject(m_po->GetReference());
        if (poObj)
            m_po = poObj;
    }
}

GDALPDFObjectPodofo::~GDALPDFObjectPodofo()
{
    delete m_poDict;
    delete m_poArray;
    delete m_poStream;
}

GDALPDFObjectType GDALPDFObjectPodofo::GetType()
{
    switch(m_po->GetDataType())
    {
        case PoDoFo::ePdfDataType_Bool:       return PDFObjectType_Int;
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

const char* GDALPDFObjectPodofo::GetTypeName()
{
    return m_po->GetDataTypeString();
}

int GDALPDFObjectPodofo::GetInt()
{
    if (m_po->GetDataType() == PoDoFo::ePdfDataType_Bool)
        return m_po->GetBool();
    else if (m_po->GetDataType() == PoDoFo::ePdfDataType_Number)
        return (int)m_po->GetNumber();
    else
        return 0;
}

double GDALPDFObjectPodofo::GetReal()
{
    if (GetType() == PDFObjectType_Real)
        return m_po->GetReal();
    else
        return 0.0;
}

const CPLString& GDALPDFObjectPodofo::GetString()
{
    if (GetType() == PDFObjectType_String)
        return (osStr = m_po->GetString().GetStringUtf8());
    else
        return (osStr = "");
}

const CPLString&  GDALPDFObjectPodofo::GetName()
{
    if (GetType() == PDFObjectType_Name)
        return (osStr = m_po->GetName().GetName());
    else
        return (osStr = "");
}

GDALPDFDictionary* GDALPDFObjectPodofo::GetDictionary()
{
    if (GetType() != PDFObjectType_Dictionary)
        return NULL;

    if (m_poDict)
        return m_poDict;

    m_poDict = new GDALPDFDictionaryPodofo(&m_po->GetDictionary(), m_poObjects);
    return m_poDict;
}

GDALPDFArray* GDALPDFObjectPodofo::GetArray()
{
    if (GetType() != PDFObjectType_Array)
        return NULL;

    if (m_poArray)
        return m_poArray;

    m_poArray = new GDALPDFArrayPodofo(&m_po->GetArray(), m_poObjects);
    return m_poArray;
}

GDALPDFStream* GDALPDFObjectPodofo::GetStream()
{
    if (!m_po->HasStream())
        return NULL;

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

GDALPDFDictionaryPodofo::~GDALPDFDictionaryPodofo()
{
    std::map<CPLString, GDALPDFObject*>::iterator oIter = m_map.begin();
    std::map<CPLString, GDALPDFObject*>::iterator oEnd = m_map.end();
    for(; oIter != oEnd; ++oIter)
        delete oIter->second;
}

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

GDALPDFArrayPodofo::~GDALPDFArrayPodofo()
{
    for(int i=0;i<(int)m_v.size();i++)
    {
        delete m_v[i];
    }
}

int GDALPDFArrayPodofo::GetLength()
{
    return (int)m_poArray->GetSize();
}

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

int GDALPDFStreamPodofo::GetLength()
{
    return (int)m_pStream->GetLength();
}

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

#endif
