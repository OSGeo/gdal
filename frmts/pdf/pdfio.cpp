/******************************************************************************
 *
 * Project:  PDF driver
 * Purpose:  GDALDataset driver for PDF dataset.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "gdal_pdf.h"

#ifdef HAVE_POPPLER

#include "pdfio.h"

#include "cpl_vsi.h"

static vsi_l_offset VSIPDFFileStreamGetSize(VSILFILE *f)
{
    VSIFSeekL(f, 0, SEEK_END);
    vsi_l_offset nSize = VSIFTellL(f);
    VSIFSeekL(f, 0, SEEK_SET);
    return nSize;
}

/************************************************************************/
/*                         VSIPDFFileStream()                           */
/************************************************************************/

VSIPDFFileStream::VSIPDFFileStream(VSILFILE *fIn, const char *pszFilename,
                                   Object &&dictA)
    : BaseStream(std::move(dictA),
                 static_cast<Goffset>(VSIPDFFileStreamGetSize(fIn))),
      poParent(nullptr), poFilename(new GooString(pszFilename)), f(fIn)
{
}

/************************************************************************/
/*                         VSIPDFFileStream()                           */
/************************************************************************/

VSIPDFFileStream::VSIPDFFileStream(VSIPDFFileStream *poParentIn,
                                   vsi_l_offset startA, bool limitedA,
                                   vsi_l_offset lengthA, Object &&dictA)
    : BaseStream(std::move(dictA), static_cast<Goffset>(lengthA)),
      poParent(poParentIn), poFilename(poParentIn->poFilename),
      f(poParentIn->f), nStart(startA), bLimited(limitedA), nLength(lengthA)
{
}

/************************************************************************/
/*                        ~VSIPDFFileStream()                           */
/************************************************************************/

VSIPDFFileStream::~VSIPDFFileStream()
{
    close();
    if (poParent == nullptr)
    {
        delete poFilename;
    }
}

/************************************************************************/
/*                                  copy()                              */
/************************************************************************/

BaseStream *VSIPDFFileStream::copy()
{
    return new VSIPDFFileStream(poParent, nStart, bLimited, nLength,
                                dict.copy());
}

/************************************************************************/
/*                             makeSubStream()                          */
/************************************************************************/
Stream *VSIPDFFileStream::makeSubStream(Goffset startA, bool limitedA,
                                        Goffset lengthA, Object &&dictA)
{
    return new VSIPDFFileStream(this, startA, limitedA, lengthA,
                                std::move(dictA));
}

/************************************************************************/
/*                                 getPos()                             */
/************************************************************************/

Goffset VSIPDFFileStream::getPos()
{
    return static_cast<Goffset>(nCurrentPos);
}

/************************************************************************/
/*                                getStart()                            */
/************************************************************************/

Goffset VSIPDFFileStream::getStart()
{
    return static_cast<Goffset>(nStart);
}

/************************************************************************/
/*                             getKind()                                */
/************************************************************************/

StreamKind VSIPDFFileStream::getKind() const
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
        nToRead = static_cast<int>(nStart + nLength - nCurrentPos);
    else
        nToRead = BUFFER_SIZE;
    if (nToRead < 0)
        return FALSE;
    nBufferLength = static_cast<int>(VSIFReadL(abyBuffer, 1, nToRead, f));
    if (nBufferLength == 0)
        return FALSE;

    // Since we now report a non-zero length (as BaseStream::length member),
    // PDFDoc::getPage() can go to the linearized mode if the file is
    // linearized, and thus create a pageCache. If so, in PDFDoc::~PDFDoc(), if
    // pageCache is not null, it would try to access the stream (str) through
    // getPageCount(), but we have just freed and nullify str before in
    // PDFFreeDoc(). So make as if the file is not linearized to avoid those
    // issues... All this is due to our attempt of avoiding cross-heap issues
    // with allocation and liberation of VSIPDFFileStream as PDFDoc::str member.
    if (nCurrentPos == 0 || nCurrentPos == VSI_L_OFFSET_MAX)
    {
        for (int i = 0;
             i < nBufferLength - static_cast<int>(strlen("/Linearized ")); i++)
        {
            if (memcmp(abyBuffer + i, "/Linearized ", strlen("/Linearized ")) ==
                0)
            {
                bFoundLinearizedHint = true;
                memcpy(abyBuffer + i, "/XXXXXXXXXX ", strlen("/Linearized "));
                break;
            }
        }
    }

    return TRUE;
}

/************************************************************************/
/*                                getChar()                             */
/************************************************************************/

/* The unoptimized version performs a bit less since we must go through */
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
    nPosInBuffer++;
#endif
    nCurrentPos++;
    return chRead;
}

/************************************************************************/
/*                       getUnfilteredChar()                            */
/************************************************************************/

int VSIPDFFileStream::getUnfilteredChar()
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
    nPosInBuffer--;
    nCurrentPos--;
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
    nPosInBuffer = -1;
    nBufferLength = -1;
}

/************************************************************************/
/*                         unfilteredReset()                            */
/************************************************************************/

void VSIPDFFileStream::unfilteredReset()
{
    reset();
}

/************************************************************************/
/*                                close()                               */
/************************************************************************/

void VSIPDFFileStream::close()
{
    if (bHasSavedPos)
    {
        nCurrentPos = nSavedPos;
        VSIFSeekL(f, nCurrentPos, SEEK_SET);
    }
    bHasSavedPos = FALSE;
    nSavedPos = 0;
}

/************************************************************************/
/*                               setPos()                               */
/************************************************************************/

void VSIPDFFileStream::setPos(Goffset pos, int dir)
{
    if (dir >= 0)
    {
        VSIFSeekL(f, nCurrentPos = pos, SEEK_SET);
    }
    else
    {
        if (bLimited == false)
        {
            VSIFSeekL(f, 0, SEEK_END);
        }
        else
        {
            VSIFSeekL(f, nStart + nLength, SEEK_SET);
        }
        vsi_l_offset size = VSIFTellL(f);
        vsi_l_offset newpos = static_cast<vsi_l_offset>(pos);
        if (newpos > size)
            newpos = size;
        VSIFSeekL(f, nCurrentPos = size - newpos, SEEK_SET);
    }
    nPosInBuffer = -1;
    nBufferLength = -1;
}

/************************************************************************/
/*                            moveStart()                               */
/************************************************************************/

void VSIPDFFileStream::moveStart(Goffset delta)
{
    nStart += delta;
    nCurrentPos = nStart;
    VSIFSeekL(f, nCurrentPos, SEEK_SET);
    nPosInBuffer = -1;
    nBufferLength = -1;
}

/************************************************************************/
/*                          hasGetChars()                               */
/************************************************************************/

bool VSIPDFFileStream::hasGetChars()
{
    return true;
}

/************************************************************************/
/*                            getChars()                                */
/************************************************************************/

int VSIPDFFileStream::getChars(int nChars, unsigned char *buffer)
{
    int nRead = 0;
    while (nRead < nChars)
    {
        int nToRead = nChars - nRead;
        if (nPosInBuffer == nBufferLength)
        {
            if (!bLimited && nToRead > BUFFER_SIZE)
            {
                int nJustRead =
                    static_cast<int>(VSIFReadL(buffer + nRead, 1, nToRead, f));
                nPosInBuffer = -1;
                nBufferLength = -1;
                nCurrentPos += nJustRead;
                nRead += nJustRead;
                break;
            }
            else if (!FillBuffer() || nPosInBuffer >= nBufferLength)
                break;
        }
        if (nToRead > nBufferLength - nPosInBuffer)
            nToRead = nBufferLength - nPosInBuffer;

        memcpy(buffer + nRead, abyBuffer + nPosInBuffer, nToRead);
        nPosInBuffer += nToRead;
        nCurrentPos += nToRead;
        nRead += nToRead;
    }
    return nRead;
}

#endif
