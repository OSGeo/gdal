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


#ifdef POPPLER_0_23_OR_LATER
#define getPos_ret_type Goffset
#define getStart_ret_type Goffset
#define makeSubStream_offset_type Goffset
#define setPos_offset_type Goffset
#define moveStart_delta_type Goffset
#else
#define getPos_ret_type int
#define getStart_ret_type Guint
#define makeSubStream_offset_type Guint
#define setPos_offset_type Guint
#define moveStart_delta_type int
#endif


class VSIPDFFileStream: public BaseStream
{
    public:
        VSIPDFFileStream(VSILFILE* f, const char* pszFilename, Object *dictA);
        VSIPDFFileStream(VSIPDFFileStream* poParent,
                         vsi_l_offset startA, GBool limitedA,
                         vsi_l_offset lengthA, Object *dictA);
        virtual ~VSIPDFFileStream();

#ifdef POPPLER_0_23_OR_LATER
        virtual BaseStream* copy();
#endif

        virtual Stream *   makeSubStream(makeSubStream_offset_type startA, GBool limitedA,
                                         makeSubStream_offset_type lengthA, Object *dictA);
        virtual getPos_ret_type      getPos();
        virtual getStart_ret_type    getStart();

        virtual void       setPos(setPos_offset_type pos, int dir = 0);
        virtual void       moveStart(moveStart_delta_type delta);

        virtual StreamKind getKind();
        virtual GooString *getFileName();

        virtual int        getChar();
        virtual int        getUnfilteredChar ();
        virtual int        lookChar();

        virtual void       reset();
        virtual void       unfilteredReset ();
        virtual void       close();

    private:
        VSIPDFFileStream  *poParent;
        GooString         *poFilename;
        VSILFILE          *f;
        vsi_l_offset       nStart;
        int                bLimited;
        vsi_l_offset       nLength;

        vsi_l_offset       nCurrentPos;
        int                bHasSavedPos;
        vsi_l_offset       nSavedPos;

        GByte              abyBuffer[BUFFER_SIZE];
        int                nPosInBuffer;
        int                nBufferLength;

        int                FillBuffer();
};

#endif // PDFIO_H_INCLUDED
