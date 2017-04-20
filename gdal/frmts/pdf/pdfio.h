/******************************************************************************
 * $Id$
 *
 * Project:  PDF driver
 * Purpose:  GDALDataset driver for PDF dataset.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
        virtual BaseStream* copy() override;
#endif

        virtual Stream *   makeSubStream(makeSubStream_offset_type startA, GBool limitedA,
                                         makeSubStream_offset_type lengthA, Object *dictA) override;
        virtual getPos_ret_type      getPos() override;
        virtual getStart_ret_type    getStart() override;

        virtual void       setPos(setPos_offset_type pos, int dir = 0) override;
        virtual void       moveStart(moveStart_delta_type delta) override;

        virtual StreamKind getKind() override;
        virtual GooString *getFileName() override;

        virtual int        getChar() override;
        virtual int        getUnfilteredChar () override;
        virtual int        lookChar() override;

        virtual void       reset() override;
        virtual void       unfilteredReset () override;
        virtual void       close() override;

    private:
#ifdef POPPLER_BASE_STREAM_HAS_TWO_ARGS
        /* getChars/hasGetChars added in poppler 0.15.0
         * POPPLER_BASE_STREAM_HAS_TWO_ARGS true from poppler 0.16,
         * This test will be wrong for poppler 0.15 or 0.16,
         * but will still compile correctly.
         */
        virtual GBool hasGetChars() override;
        virtual int getChars(int nChars, Guchar *buffer) override;
#else
        virtual GBool hasGetChars() ;
        virtual int getChars(int nChars, Guchar *buffer) ;
#endif

        VSIPDFFileStream  *poParent;
        GooString         *poFilename;
        VSILFILE          *f;
        vsi_l_offset       nStart;
        GBool              bLimited;
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
