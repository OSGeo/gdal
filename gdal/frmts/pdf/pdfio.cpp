/******************************************************************************
 * $Id $
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

#ifdef HAVE_POPPLER

/* hack for PDF driver and poppler >= 0.15.0 that defines incompatible "typedef bool GBool" */
/* in include/poppler/goo/gtypes.h with the one defined in cpl_port.h */
#define CPL_GBOOL_DEFINED

#include "pdfio.h"

#include "cpl_vsi.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         VSIPDFFileStream()                           */
/************************************************************************/

VSIPDFFileStream::VSIPDFFileStream(VSILFILE* f, const char* pszFilename, Object *dictA):
#ifdef POPPLER_BASE_STREAM_HAS_TWO_ARGS
                                                        BaseStream(dictA, 0)
#else
                                                        BaseStream(dictA)
#endif
{
    poParent = NULL;
    poFilename = new GooString(pszFilename);
    this->f = f;
    nStart = 0;
    bLimited = gFalse;
    nLength = 0;
    nCurrentPos = -1;
    bHasSavedPos = FALSE;
    nSavedPos = 0;
    nPosInBuffer = nBufferLength = -1;
}

/************************************************************************/
/*                         VSIPDFFileStream()                           */
/************************************************************************/

VSIPDFFileStream::VSIPDFFileStream(VSIPDFFileStream* poParent,
                                   vsi_l_offset startA, GBool limitedA,
                                   vsi_l_offset lengthA, Object *dictA):
#ifdef POPPLER_BASE_STREAM_HAS_TWO_ARGS
                                                        BaseStream(dictA, lengthA)
#else
                                                        BaseStream(dictA)
#endif
{
    this->poParent = poParent;
    poFilename = poParent->poFilename;
    f = poParent->f;
    nStart = startA;
    bLimited = limitedA;
    nLength = lengthA;
    nCurrentPos = -1;
    bHasSavedPos = FALSE;
    nSavedPos = 0;
    nPosInBuffer = nBufferLength = -1;
}

/************************************************************************/
/*                        ~VSIPDFFileStream()                           */
/************************************************************************/

VSIPDFFileStream::~VSIPDFFileStream()
{
    close();
    if (poParent == NULL)
    {
        delete poFilename;
        if (f)
            VSIFCloseL(f);
    }
}

/************************************************************************/
/*                                  copy()                              */
/************************************************************************/

#ifdef POPPLER_0_23_OR_LATER
BaseStream* VSIPDFFileStream::copy()
{
    return new VSIPDFFileStream(poParent, nStart, bLimited,
                                nLength, &dict);
}
#endif

/************************************************************************/
/*                             makeSubStream()                          */
/************************************************************************/

Stream *VSIPDFFileStream::makeSubStream(makeSubStream_offset_type startA, GBool limitedA,
                                        makeSubStream_offset_type lengthA, Object *dictA)
{
    return new VSIPDFFileStream(this,
                                startA, limitedA,
                                lengthA, dictA);
}

/************************************************************************/
/*                                 getPos()                             */
/************************************************************************/

getPos_ret_type VSIPDFFileStream::getPos()
{
    return (getPos_ret_type) nCurrentPos;
}

/************************************************************************/
/*                                getStart()                            */
/************************************************************************/


getStart_ret_type VSIPDFFileStream::getStart()
{
    return (getStart_ret_type) nStart;
}

/************************************************************************/
/*                             getKind()                                */
/************************************************************************/

StreamKind VSIPDFFileStream::getKind()
{
    return strFile;
}

/************************************************************************/
/*                           getFileName()                               */
/************************************************************************/

GooString *VSIPDFFileStream::getFileName()
{
    return poFilename;
}

/************************************************************************/
/*                             FillBuffer()                             */
/************************************************************************/

int VSIPDFFileStream::FillBuffer()
{
    if (nBufferLength == 0)
        return FALSE;
    if (nBufferLength != -1 && nBufferLength < BUFFER_SIZE)
        return FALSE;

    nPosInBuffer = 0;
    int nToRead;
    if (!bLimited)
        nToRead = BUFFER_SIZE;
    else if (nCurrentPos + BUFFER_SIZE > nStart + nLength)
        nToRead = (int)(nStart + nLength - nCurrentPos);
    else
        nToRead = BUFFER_SIZE;
    if( nToRead < 0 )
        return FALSE;
    nBufferLength = (int) VSIFReadL(abyBuffer, 1, nToRead, f);
    if (nBufferLength == 0)
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                getChar()                             */
/************************************************************************/

/* The unoptimized version performs a bit well since we must go through */
/* the whole virtual I/O chain for each character reading. We save a few */
/* percent with this extra internal caching */

int VSIPDFFileStream::getChar()
{
#ifdef unoptimized_version
    GByte chRead;
    if (bLimited && nCurrentPos >= nStart + nLength)
        return EOF;
    if (VSIFReadL(&chRead, 1, 1, f) == 0)
        return EOF;
#else
    if (nPosInBuffer == nBufferLength)
    {
        if (!FillBuffer() || nPosInBuffer >= nBufferLength)
            return EOF;
    }

    GByte chRead = abyBuffer[nPosInBuffer];
    nPosInBuffer ++;
#endif
    nCurrentPos ++;
    return chRead;
}

/************************************************************************/
/*                       getUnfilteredChar()                            */
/************************************************************************/

int VSIPDFFileStream::getUnfilteredChar ()
{
    return getChar();
}

/************************************************************************/
/*                               lookChar()                             */
/************************************************************************/

int VSIPDFFileStream::lookChar()
{
#ifdef unoptimized_version
    int nPosBefore = nCurrentPos;
    int chRead = getChar();
    if (chRead == EOF)
        return EOF;
    VSIFSeekL(f, nCurrentPos = nPosBefore, SEEK_SET);
    return chRead;
#else
    int chRead = getChar();
    if (chRead == EOF)
        return EOF;
    nPosInBuffer --;
    nCurrentPos --;
    return chRead;
#endif
}

/************************************************************************/
/*                                reset()                               */
/************************************************************************/

void VSIPDFFileStream::reset()
{
    nSavedPos = VSIFTellL(f);
    bHasSavedPos = TRUE;
    VSIFSeekL(f, nCurrentPos = nStart, SEEK_SET);
    nPosInBuffer = nBufferLength = -1;
}

/************************************************************************/
/*                         unfilteredReset()                            */
/************************************************************************/

void VSIPDFFileStream::unfilteredReset ()
{
    reset();
}

/************************************************************************/
/*                                close()                               */
/************************************************************************/

void VSIPDFFileStream::close()
{
    if (bHasSavedPos)
        VSIFSeekL(f, nCurrentPos = nSavedPos, SEEK_SET);
    bHasSavedPos = FALSE;
    nSavedPos = 0;
}

/************************************************************************/
/*                               setPos()                               */
/************************************************************************/

void VSIPDFFileStream::setPos(setPos_offset_type pos, int dir)
{
    if (dir >= 0)
    {
        VSIFSeekL(f, nCurrentPos = pos, SEEK_SET);
    }
    else
    {
        if (bLimited == gFalse)
        {
            VSIFSeekL(f, 0, SEEK_END);
        }
        else
        {
            VSIFSeekL(f, nStart + nLength, SEEK_SET);
        }
        vsi_l_offset size = VSIFTellL(f);
        vsi_l_offset newpos = (vsi_l_offset) pos;
        if (newpos > size)
            newpos = size;
        VSIFSeekL(f, nCurrentPos = size - newpos, SEEK_SET);
    }
    nPosInBuffer = nBufferLength = -1;
}

/************************************************************************/
/*                            moveStart()                               */
/************************************************************************/

void VSIPDFFileStream::moveStart(moveStart_delta_type delta)
{
    nStart += delta;
    VSIFSeekL(f, nCurrentPos = nStart, SEEK_SET);
    nPosInBuffer = nBufferLength = -1;
}

#endif
