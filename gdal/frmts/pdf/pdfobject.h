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

#ifndef PDFOBJECT_H_INCLUDED
#define PDFOBJECT_H_INCLUDED

#include "cpl_string.h"
#include <map>
#include <vector>

#ifdef HAVE_POPPLER

/* begin of poppler xpdf includes */
#include <poppler/Object.h>

#define private public /* Ugly! Page::pageObj is private but we need it... */
#include <poppler/Page.h>
#undef private

#include <poppler/Dict.h>

#define private public /* Ugly! Catalog::optContent is private but we need it... */
#include <poppler/Catalog.h>
#undef private

#define private public  /* Ugly! PDFDoc::str is private but we need it... */
#include <poppler/PDFDoc.h>
#undef private

#include <poppler/splash/SplashBitmap.h>
#include <poppler/splash/Splash.h>
#include <poppler/SplashOutputDev.h>
#include <poppler/GlobalParams.h>
#include <poppler/ErrorCodes.h>
/* end of poppler xpdf includes */

#endif // HAVE_POPPLER

#ifdef HAVE_PODOFO
#include "podofo.h"
#endif // HAVE_PODOFO


double ROUND_TO_INT_IF_CLOSE(double x, double eps = 0);

typedef enum
{
    PDFObjectType_Unknown,
    PDFObjectType_Null,
    PDFObjectType_Bool,
    PDFObjectType_Int,
    PDFObjectType_Real,
    PDFObjectType_String,
    PDFObjectType_Name,
    PDFObjectType_Array,
    PDFObjectType_Dictionary
} GDALPDFObjectType;

class GDALPDFDictionary;
class GDALPDFArray;
class GDALPDFStream;

class GDALPDFObjectRW;
class GDALPDFDictionaryRW;
class GDALPDFArrayRW;

class GDALPDFObject
{
    protected:
        virtual const char*       GetTypeNameNative() = 0;

    public:
        virtual ~GDALPDFObject();

        virtual GDALPDFObjectType GetType() = 0;
        virtual const char*       GetTypeName();
        virtual int               GetBool() = 0;
        virtual int               GetInt() = 0;
        virtual double            GetReal() = 0;
        virtual int               CanRepresentRealAsString() { return FALSE; }
        virtual const CPLString&  GetString() = 0;
        virtual const CPLString&  GetName() = 0;
        virtual GDALPDFDictionary*  GetDictionary() = 0;
        virtual GDALPDFArray*       GetArray() = 0;
        virtual GDALPDFStream*      GetStream() = 0;
        virtual int                 GetRefNum() = 0;
        virtual int                 GetRefGen() = 0;

        void                        Serialize(CPLString& osStr);
        CPLString                   Serialize() { CPLString osStr; Serialize(osStr); return osStr; }
        GDALPDFObjectRW*            Clone();
};

class GDALPDFDictionary
{
    public:
        virtual ~GDALPDFDictionary();

        virtual GDALPDFObject* Get(const char* pszKey) = 0;
        virtual std::map<CPLString, GDALPDFObject*>& GetValues() = 0;

        void                        Serialize(CPLString& osStr);
        CPLString                   Serialize() { CPLString osStr; Serialize(osStr); return osStr; }
        GDALPDFDictionaryRW*        Clone();
};

class GDALPDFArray
{
    public:
        virtual ~GDALPDFArray();

        virtual int GetLength() = 0;
        virtual GDALPDFObject* Get(int nIndex) = 0;

        void                        Serialize(CPLString& osStr);
        CPLString                   Serialize() { CPLString osStr; Serialize(osStr); return osStr; }
        GDALPDFArrayRW*             Clone();
};

class GDALPDFStream
{
    public:
        virtual ~GDALPDFStream();

        virtual int GetLength() = 0;
        virtual char* GetBytes() = 0;
};

class GDALPDFObjectRW : public GDALPDFObject
{
    private:
        GDALPDFObjectType     m_eType;
        int                   m_nVal;
        double                m_dfVal;
        CPLString             m_osVal;
        GDALPDFDictionaryRW  *m_poDict;
        GDALPDFArrayRW       *m_poArray;
        int                   m_nNum;
        int                   m_nGen;
        int                   m_bCanRepresentRealAsString;

                              GDALPDFObjectRW(GDALPDFObjectType eType);

    protected:
        virtual const char*       GetTypeNameNative();

    public:

        static GDALPDFObjectRW* CreateIndirect(int nNum, int nGen);
        static GDALPDFObjectRW* CreateNull();
        static GDALPDFObjectRW* CreateBool(int bVal);
        static GDALPDFObjectRW* CreateInt(int nVal);
        static GDALPDFObjectRW* CreateReal(double dfVal, int bCanRepresentRealAsString = FALSE);
        static GDALPDFObjectRW* CreateString(const char* pszStr);
        static GDALPDFObjectRW* CreateName(const char* pszName);
        static GDALPDFObjectRW* CreateDictionary(GDALPDFDictionaryRW* poDict);
        static GDALPDFObjectRW* CreateArray(GDALPDFArrayRW* poArray);
        virtual ~GDALPDFObjectRW();

        virtual GDALPDFObjectType GetType();
        virtual int               GetBool();
        virtual int               GetInt();
        virtual double            GetReal();
        virtual int               CanRepresentRealAsString() { return m_bCanRepresentRealAsString; }
        virtual const CPLString&  GetString();
        virtual const CPLString&  GetName();
        virtual GDALPDFDictionary*  GetDictionary();
        virtual GDALPDFArray*       GetArray();
        virtual GDALPDFStream*      GetStream();
        virtual int                 GetRefNum();
        virtual int                 GetRefGen();
};

class GDALPDFDictionaryRW : public GDALPDFDictionary
{
    private:
        std::map<CPLString, GDALPDFObject*> m_map;

    public:
                               GDALPDFDictionaryRW();
        virtual               ~GDALPDFDictionaryRW();

        virtual GDALPDFObject*                       Get(const char* pszKey);
        virtual std::map<CPLString, GDALPDFObject*>& GetValues();

        GDALPDFDictionaryRW&   Add(const char* pszKey, GDALPDFObject* poVal);
        GDALPDFDictionaryRW&   Remove(const char* pszKey);

        GDALPDFDictionaryRW&   Add(const char* pszKey, GDALPDFArrayRW* poArray) { return Add(pszKey, GDALPDFObjectRW::CreateArray(poArray)); }
        GDALPDFDictionaryRW&   Add(const char* pszKey, GDALPDFDictionaryRW* poDict) { return Add(pszKey, GDALPDFObjectRW::CreateDictionary(poDict)); }
        GDALPDFDictionaryRW&   Add(const char* pszKey, const char* pszVal) { return Add(pszKey, GDALPDFObjectRW::CreateString(pszVal)); }
        GDALPDFDictionaryRW&   Add(const char* pszKey, int nVal) { return Add(pszKey, GDALPDFObjectRW::CreateInt(nVal)); }
        GDALPDFDictionaryRW&   Add(const char* pszKey, double dfVal, int bCanRepresentRealAsString = FALSE) { return Add(pszKey, GDALPDFObjectRW::CreateReal(dfVal, bCanRepresentRealAsString)); }
        GDALPDFDictionaryRW&   Add(const char* pszKey, int nNum, int nGen) { return Add(pszKey, GDALPDFObjectRW::CreateIndirect(nNum, nGen)); }
};

class GDALPDFArrayRW : public GDALPDFArray
{
    private:
        std::vector<GDALPDFObject*> m_array;

    public:
                               GDALPDFArrayRW();
        virtual               ~GDALPDFArrayRW();

        virtual int            GetLength();
        virtual GDALPDFObject* Get(int nIndex);

        GDALPDFArrayRW&        Add(GDALPDFObject* poObj);

        GDALPDFArrayRW&        Add(GDALPDFArrayRW* poArray) { return Add(GDALPDFObjectRW::CreateArray(poArray)); }
        GDALPDFArrayRW&        Add(GDALPDFDictionaryRW* poDict) { return Add(GDALPDFObjectRW::CreateDictionary(poDict)); }
        GDALPDFArrayRW&        Add(const char* pszVal) { return Add(GDALPDFObjectRW::CreateString(pszVal)); }
        GDALPDFArrayRW&        Add(int nVal) { return Add(GDALPDFObjectRW::CreateInt(nVal)); }
        GDALPDFArrayRW&        Add(double dfVal, int bCanRepresentRealAsString = FALSE) { return Add(GDALPDFObjectRW::CreateReal(dfVal, bCanRepresentRealAsString)); }
        GDALPDFArrayRW&        Add(double* padfVal, int nCount, int bCanRepresentRealAsString = FALSE);
        GDALPDFArrayRW&        Add(int nNum, int nGen) { return Add(GDALPDFObjectRW::CreateIndirect(nNum, nGen)); }
};

#ifdef HAVE_POPPLER

class GDALPDFObjectPoppler : public GDALPDFObject
{
    private:
        Object* m_po;
        int     m_bDestroy;
        GDALPDFDictionary* m_poDict;
        GDALPDFArray* m_poArray;
        GDALPDFStream* m_poStream;
        CPLString osStr;
        int m_nRefNum;
        int m_nRefGen;

    protected:
        virtual const char*       GetTypeNameNative();

    public:
        GDALPDFObjectPoppler(Object* po, int bDestroy) :
                m_po(po), m_bDestroy(bDestroy),
                m_poDict(NULL), m_poArray(NULL), m_poStream(NULL),
                m_nRefNum(0), m_nRefGen(0) {}

        void SetRefNumAndGen(int nNum, int nGen);

        virtual ~GDALPDFObjectPoppler();

        virtual GDALPDFObjectType GetType();
        virtual int               GetBool();
        virtual int               GetInt();
        virtual double            GetReal();
        virtual const CPLString&  GetString();
        virtual const CPLString&  GetName();
        virtual GDALPDFDictionary*  GetDictionary();
        virtual GDALPDFArray*       GetArray();
        virtual GDALPDFStream*      GetStream();
        virtual int                 GetRefNum();
        virtual int                 GetRefGen();
};

GDALPDFArray* GDALPDFCreateArray(Array* array);

#endif // HAVE_POPPLER

#ifdef HAVE_PODOFO

class GDALPDFObjectPodofo : public GDALPDFObject
{
    private:
        PoDoFo::PdfObject* m_po;
        PoDoFo::PdfVecObjects& m_poObjects;
        GDALPDFDictionary* m_poDict;
        GDALPDFArray* m_poArray;
        GDALPDFStream* m_poStream;
        CPLString osStr;

    protected:
        virtual const char*       GetTypeNameNative();

    public:
        GDALPDFObjectPodofo(PoDoFo::PdfObject* po, PoDoFo::PdfVecObjects& poObjects);

        virtual ~GDALPDFObjectPodofo();

        virtual GDALPDFObjectType GetType();
        virtual int               GetBool();
        virtual int               GetInt();
        virtual double            GetReal();
        virtual const CPLString&  GetString();
        virtual const CPLString&  GetName();
        virtual GDALPDFDictionary*  GetDictionary();
        virtual GDALPDFArray*       GetArray();
        virtual GDALPDFStream*      GetStream();
        virtual int                 GetRefNum();
        virtual int                 GetRefGen();
};

#endif // HAVE_PODOFO

#endif // PDFOBJECT_H_INCLUDED
