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

#ifdef USE_POPPLER

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

#else

#include "podofo.h"

#endif

typedef enum
{
    PDFObjectType_Unknown,
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

class GDALPDFObject
{
    public:
        virtual ~GDALPDFObject();

        virtual GDALPDFObjectType GetType() = 0;
        virtual const char*       GetTypeName() = 0;
        virtual int               GetInt() = 0;
        virtual double            GetReal() = 0;
        virtual const CPLString&  GetString() = 0;
        virtual const CPLString&  GetName() = 0;
        virtual GDALPDFDictionary*  GetDictionary() = 0;
        virtual GDALPDFArray*       GetArray() = 0;
        virtual GDALPDFStream*      GetStream() = 0;
};

class GDALPDFDictionary
{
    public:
        virtual ~GDALPDFDictionary();

        virtual GDALPDFObject* Get(const char* pszKey) = 0;
        virtual std::map<CPLString, GDALPDFObject*>& GetValues() = 0;

};

class GDALPDFArray
{
    public:
        virtual ~GDALPDFArray();

        virtual int GetLength() = 0;
        virtual GDALPDFObject* Get(int nIndex) = 0;
};

class GDALPDFStream
{
    public:
        virtual ~GDALPDFStream();

        virtual int GetLength() = 0;
        virtual char* GetBytes() = 0;
};

#ifdef USE_POPPLER

class GDALPDFObjectPoppler : public GDALPDFObject
{
    private:
        Object* m_po;
        int     m_bDestroy;
        GDALPDFDictionary* m_poDict;
        GDALPDFArray* m_poArray;
        GDALPDFStream* m_poStream;
        CPLString osStr;

    public:
        GDALPDFObjectPoppler(Object* po, int bDestroy) : m_po(po), m_bDestroy(bDestroy), m_poDict(NULL), m_poArray(NULL), m_poStream(NULL) {}

        virtual ~GDALPDFObjectPoppler();

        virtual GDALPDFObjectType GetType();
        virtual const char*       GetTypeName();
        virtual int               GetInt();
        virtual double            GetReal();
        virtual const CPLString&  GetString();
        virtual const CPLString&  GetName();
        virtual GDALPDFDictionary*  GetDictionary();
        virtual GDALPDFArray*       GetArray();
        virtual GDALPDFStream*      GetStream();
};

#else

class GDALPDFObjectPodofo : public GDALPDFObject
{
    private:
        PoDoFo::PdfObject* m_po;
        PoDoFo::PdfVecObjects& m_poObjects;
        GDALPDFDictionary* m_poDict;
        GDALPDFArray* m_poArray;
        GDALPDFStream* m_poStream;
        CPLString osStr;

    public:
        GDALPDFObjectPodofo(PoDoFo::PdfObject* po, PoDoFo::PdfVecObjects& poObjects);

        virtual ~GDALPDFObjectPodofo();

        virtual GDALPDFObjectType GetType();
        virtual const char*       GetTypeName();
        virtual int               GetInt();
        virtual double            GetReal();
        virtual const CPLString&  GetString();
        virtual const CPLString&  GetName();
        virtual GDALPDFDictionary*  GetDictionary();
        virtual GDALPDFArray*       GetArray();
        virtual GDALPDFStream*      GetStream();
};

#endif

#endif // PDFOBJECT_H_INCLUDED
