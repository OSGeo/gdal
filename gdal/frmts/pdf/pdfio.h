/******************************************************************************
 * $Id$
 *
 * Project:  PDF driver
 * Purpose:  GDALDataset driver for PDF dataset.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010, Even Rouault
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

#ifndef PDFIO_H_INCLUDED
#define PDFIO_H_INCLUDED

#include "cpl_vsi_virtual.h"


/* begin of poppler xpdf includes */
#include <poppler/Object.h>
#include <poppler/Stream.h>
/* end of poppler xpdf includes */

/************************************************************************/
/*                         VSIPDFFileStream                             */
/************************************************************************/

#define BUFFER_SIZE 1024

class VSIPDFFileStream: public BaseStream
{
    public:
        VSIPDFFileStream(VSILFILE* f, const char* pszFilename,
                         Guint startA, GBool limitedA,
                         Guint lengthA, Object *dictA);
        VSIPDFFileStream(VSIPDFFileStream* poParent,
                         Guint startA, GBool limitedA,
                         Guint lengthA, Object *dictA);
        virtual ~VSIPDFFileStream();

#ifdef POPPLER_0_23_OR_LATER
        virtual BaseStream* copy();
#endif

        virtual Stream *   makeSubStream(Guint startA, GBool limitedA,
                                         Guint lengthA, Object *dictA);
        virtual int        getPos();
        virtual Guint      getStart();
        virtual StreamKind getKind();
        virtual GooString *getFileName();

        virtual int        getChar();
        virtual int        getUnfilteredChar ();
        virtual int        lookChar();

        virtual void       reset();
        virtual void       unfilteredReset ();
        virtual void       close();
        virtual void       setPos(Guint pos, int dir = 0);
        virtual void       moveStart(int delta);

    private:
        VSIPDFFileStream  *poParent;
        GooString         *poFilename;
        VSILFILE          *f;
        int                nStart;
        int                bLimited;
        int                nLength;

        int                nCurrentPos;
        int                nSavedPos;

        GByte              abyBuffer[BUFFER_SIZE];
        int                nPosInBuffer;
        int                nBufferLength;

        int                FillBuffer();
};

#endif // PDFIO_H_INCLUDED
