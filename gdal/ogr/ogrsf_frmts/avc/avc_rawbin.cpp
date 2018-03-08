/**********************************************************************
 * $Id$
 *
 * Name:     avc_rawbin.c
 * Project:  Arc/Info vector coverage (AVC)  BIN->E00 conversion library
 * Language: ANSI C
 * Purpose:  Raw Binary file access functions.
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2005, Daniel Morissette
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 **********************************************************************
 *
 * $Log: avc_rawbin.c,v $
 * Revision 1.14  2008/07/23 20:51:38  dmorissette
 * Fixed GCC 4.1.x compile warnings related to use of char vs unsigned char
 * (GDAL/OGR ticket http://trac.osgeo.org/gdal/ticket/2495)
 *
 * Revision 1.13  2005/06/03 03:49:59  daniel
 * Update email address, website url, and copyright dates
 *
 * Revision 1.12  2004/08/19 23:41:04  warmerda
 * fixed pointer aliasing optimization bug
 *
 * Revision 1.11  2000/09/22 19:45:21  daniel
 * Switch to MIT-style license
 *
 * Revision 1.10  2000/05/29 15:36:07  daniel
 * Fixed compile warning
 *
 * Revision 1.9  2000/05/29 15:31:31  daniel
 * Added Japanese DBCS support
 *
 * Revision 1.8  2000/01/10 02:59:11  daniel
 * Fixed problem in AVCRawBinOpen() when file not found
 *
 * Revision 1.7  1999/12/24 07:18:34  daniel
 * Added PC Arc/Info coverages support
 *
 * Revision 1.6  1999/08/29 15:05:43  daniel
 * Added source filename in "Attempt to read past EOF" error message
 *
 * Revision 1.5  1999/06/08 22:09:03  daniel
 * Allow opening file with "r+" (but no real random access support yet)
 *
 * Revision 1.4  1999/05/11 02:10:51  daniel
 * Added write support
 *
 * Revision 1.3  1999/03/03 19:55:21  daniel
 * Fixed syntax error in the CPL_MSB version of AVCRawBinReadInt32()
 *
 * Revision 1.2  1999/02/25 04:20:08  daniel
 * Modified AVCRawBinEOF() to detect EOF even if AVCRawBinFSeek() was used.
 *
 * Revision 1.1  1999/01/29 16:28:52  daniel
 * Initial revision
 *
 **********************************************************************/

#include "avc.h"
#include "avc_mbyte.h"

/*---------------------------------------------------------------------
 * Define a static flag and set it with the byte ordering on this machine
 * we will then compare with this value to decide if we need to swap
 * bytes or not.
 *
 * CPL_MSB or CPL_LSB should be set in the makefile... the default is
 * CPL_LSB.
 *--------------------------------------------------------------------*/
#ifndef CPL_LSB
static AVCByteOrder geSystemByteOrder = AVCBigEndian;
#else
static AVCByteOrder geSystemByteOrder = AVCLittleEndian;
#endif

/*=====================================================================
 * Stuff related to buffered reading of raw binary files
 *====================================================================*/

/**********************************************************************
 *                          AVCRawBinOpen()
 *
 * Open a binary file for reading with buffering, or writing.
 *
 * Returns a valid AVCRawBinFile structure, or nullptr if the file could
 * not be opened or created.
 *
 * AVCRawBinClose() will eventually have to be called to release the
 * resources used by the AVCRawBinFile structure.
 **********************************************************************/
AVCRawBinFile *AVCRawBinOpen(const char *pszFname, const char *pszAccess,
                             AVCByteOrder eFileByteOrder,
                             AVCDBCSInfo *psDBCSInfo)
{
    AVCRawBinFile *psFile;

    psFile = (AVCRawBinFile*)CPLCalloc(1, sizeof(AVCRawBinFile));

    /*-----------------------------------------------------------------
     * Validate access mode and open/create file.
     * For now we support only: "r" for read-only or "w" for write-only
     * or "a" for append.
     *
     * A case for "r+" is included here, but random access is not
     * properly supported yet... so this option should be used with care.
     *----------------------------------------------------------------*/
    if (STARTS_WITH_CI(pszAccess, "r+"))
    {
        psFile->eAccess = AVCReadWrite;
        psFile->fp = VSIFOpenL(pszFname, "r+b");
    }
    else if (STARTS_WITH_CI(pszAccess, "r"))
    {
        psFile->eAccess = AVCRead;
        psFile->fp = VSIFOpenL(pszFname, "rb");
    }
    else if (STARTS_WITH_CI(pszAccess, "w"))
    {
        psFile->eAccess = AVCWrite;
        psFile->fp = VSIFOpenL(pszFname, "wb");
    }
    else if (STARTS_WITH_CI(pszAccess, "a"))
    {
        psFile->eAccess = AVCWrite;
        psFile->fp = VSIFOpenL(pszFname, "ab");
    }
    else
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Access mode \"%s\" not supported.", pszAccess);
        CPLFree(psFile);
        return nullptr;
    }

    /*-----------------------------------------------------------------
     * Check that file was opened successfully, and init struct.
     *----------------------------------------------------------------*/
    if (psFile->fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_OpenFailed,
                 "Failed to open file %s", pszFname);
        CPLFree(psFile);
        return nullptr;
    }

    /*-----------------------------------------------------------------
     * OK... Init psFile struct
     *----------------------------------------------------------------*/
    psFile->pszFname = CPLStrdup(pszFname);

    psFile->eByteOrder = eFileByteOrder;
    psFile->psDBCSInfo = psDBCSInfo; /* Handle on dataset DBCS info */

    /*-----------------------------------------------------------------
     * One can set nFileDataSize based on some header fields to force
     * EOF beyond a given point in the file.  Useful for cases like
     * PC Arc/Info where the physical file size is always a multiple of
     * 256 bytes padded with some junk at the end.
     *----------------------------------------------------------------*/
    psFile->nFileDataSize = -1;

    return psFile;
}

/**********************************************************************
 *                          AVCRawBinClose()
 *
 * Close a binary file previously opened with AVCRawBinOpen() and release
 * any memory used by the handle.
 **********************************************************************/
void AVCRawBinClose(AVCRawBinFile *psFile)
{
    if (psFile)
    {
        if (psFile->fp)
            VSIFCloseL(psFile->fp);
        CPLFree(psFile->pszFname);
        CPLFree(psFile);
    }
}

/**********************************************************************
 *                          AVCRawBinSetFileDataSize()
 *
 * One can set nFileDataSize based on some header fields to force
 * EOF beyond a given point in the file.  Useful for cases like
 * PC Arc/Info where the physical file size is always a multiple of
 * 256 bytes padded with some junk at the end.
 *
 * The default value is -1 which just looks for the real EOF.
 **********************************************************************/
void AVCRawBinSetFileDataSize(AVCRawBinFile *psFile, int nFileDataSize)
{
    if (psFile)
    {
        psFile->nFileDataSize = nFileDataSize;
    }
}

/**********************************************************************
 *                      AVCRawBinIsFileGreaterThan()
 *
 **********************************************************************/
int AVCRawBinIsFileGreaterThan(AVCRawBinFile *psFile, vsi_l_offset nSize)
{
    vsi_l_offset nCurPos = VSIFTellL(psFile->fp);
    VSIFSeekL(psFile->fp, 0, SEEK_END);
    bool bRet = VSIFTellL(psFile->fp) >= nSize;
    VSIFSeekL(psFile->fp, nCurPos, SEEK_SET);
    return bRet;
}

/**********************************************************************
 *                          AVCRawBinReadBytes()
 *
 * Copy the number of bytes from the input file to the specified
 * memory location.
 **********************************************************************/
static GBool bDisableReadBytesEOFError = FALSE;

void AVCRawBinReadBytes(AVCRawBinFile *psFile, int nBytesToRead, GByte *pBuf)
{
    int nTotalBytesToRead = nBytesToRead;

    /* Make sure file is opened with Read access
     */
    if (psFile == nullptr ||
        (psFile->eAccess != AVCRead && psFile->eAccess != AVCReadWrite))
    {
        CPLError(CE_Failure, CPLE_FileIO,
                "AVCRawBinReadBytes(): call not compatible with access mode.");
        return;
    }

    /* Quick method: check to see if we can satisfy the request with a
     * simple memcpy... most calls should take this path.
     */
    if (psFile->nCurPos + nBytesToRead <= psFile->nCurSize)
    {
        memcpy(pBuf, psFile->abyBuf+psFile->nCurPos, nBytesToRead);
        psFile->nCurPos += nBytesToRead;
        return;
    }

    /* This is the long method... it supports reading data that
     * overlaps the input buffer boundaries.
     */
    while(nBytesToRead > 0)
    {
        /* If we reached the end of our memory buffer then read another
         * chunk from the file
         */
        CPLAssert(psFile->nCurPos <= psFile->nCurSize);
        if (psFile->nCurPos == psFile->nCurSize)
        {
            psFile->nOffset += psFile->nCurSize;
            psFile->nCurSize = (int)VSIFReadL(psFile->abyBuf, sizeof(GByte),
                                        AVCRAWBIN_READBUFSIZE, psFile->fp);
            psFile->nCurPos = 0;
        }

        if (psFile->nCurSize == 0)
        {
            /* Attempt to read past EOF... generate an error.
             *
             * Note: AVCRawBinEOF() can set bDisableReadBytesEOFError=TRUE
             *       to disable the error message while it is testing
             *       for EOF.
             *
             * TODO: We are not resetting the buffer. Also, there is no easy
             *       way to recover from the situation.
             */
            if (bDisableReadBytesEOFError == FALSE)
                CPLError(CE_Failure, CPLE_FileIO,
                         "EOF encountered in %s after reading %d bytes while "
                         "trying to read %d bytes. File may be corrupt.",
                         psFile->pszFname, nTotalBytesToRead-nBytesToRead,
                         nTotalBytesToRead);
            return;
        }

        /* If the requested bytes are not all in the current buffer then
         * just read the part that's in memory for now... the loop will
         * take care of the rest.
         */
        if (psFile->nCurPos + nBytesToRead > psFile->nCurSize)
        {
            int nBytes;
            nBytes = psFile->nCurSize-psFile->nCurPos;
            memcpy(pBuf, psFile->abyBuf+psFile->nCurPos, nBytes);
            psFile->nCurPos += nBytes;
            pBuf += nBytes;
            nBytesToRead -= nBytes;
        }
        else
        {
            /* All the requested bytes are now in the buffer...
             * simply copy them and return.
             */
            memcpy(pBuf, psFile->abyBuf+psFile->nCurPos, nBytesToRead);
            psFile->nCurPos += nBytesToRead;

            nBytesToRead = 0;   /* Terminate the loop */
        }
    }
}

/**********************************************************************
 *                          AVCRawBinReadString()
 *
 * Same as AVCRawBinReadBytes() except that the string is run through
 * the DBCS conversion function.
 *
 * pBuf should be allocated with a size of at least nBytesToRead+1 bytes.
 **********************************************************************/
void AVCRawBinReadString(AVCRawBinFile *psFile, int nBytesToRead, GByte *pBuf)
{
    const GByte *pszConvBuf;

    memset(pBuf, 0, nBytesToRead);
    AVCRawBinReadBytes(psFile, nBytesToRead, pBuf);

    pBuf[nBytesToRead] = '\0';

    pszConvBuf = AVCE00ConvertFromArcDBCS(psFile->psDBCSInfo,
                                          pBuf,
                                          nBytesToRead);

    if (pszConvBuf != pBuf)
    {
        memcpy(pBuf, pszConvBuf, nBytesToRead);
    }
}

/**********************************************************************
 *                          AVCRawBinFSeek()
 *
 * Move the read pointer to the specified location.
 *
 * As with fseek(), the specified position can be relative to the
 * beginning of the file (SEEK_SET), or the current position (SEEK_CUR).
 * SEEK_END is not supported.
 **********************************************************************/
void AVCRawBinFSeek(AVCRawBinFile *psFile, int nOffset, int nFrom)
{
    int  nTarget = 0;

    CPLAssert(nFrom == SEEK_SET || nFrom == SEEK_CUR);

    /* Supported only with read access for now
     */
    CPLAssert(psFile && psFile->eAccess != AVCWrite);
    if (psFile == nullptr || psFile->eAccess == AVCWrite)
        return;

    /* Compute destination relative to current memory buffer
     */
    GIntBig nTargetBig;
    if (nFrom == SEEK_SET)
        nTargetBig = static_cast<GIntBig>(nOffset) - psFile->nOffset;
    else /* if (nFrom == SEEK_CUR) */
        nTargetBig = static_cast<GIntBig>(nOffset) + psFile->nCurPos;
    if( nTargetBig > INT_MAX )
        return;
    nTarget = static_cast<int>(nTargetBig);

    /* Is the destination located inside the current buffer?
     */
    if (nTarget > 0 && nTarget <= psFile->nCurSize)
    {
        /* Requested location is already in memory... just move the
         * read pointer
         */
        psFile->nCurPos = nTarget;
    }
    else
    {
        if( (nTarget > 0 && psFile->nOffset > INT_MAX - nTarget) ||
            psFile->nOffset+nTarget < 0 )
        {
            return;
        }

        /* Requested location is not part of the memory buffer...
         * move the FILE * to the right location and be ready to
         * read from there.
         */
        psFile->nCurPos = 0;
        psFile->nCurSize = 0;
        psFile->nOffset = psFile->nOffset+nTarget;
        if( VSIFSeekL(psFile->fp, psFile->nOffset, SEEK_SET) < 0 )
            return;
    }

}

/**********************************************************************
 *                          AVCRawBinEOF()
 *
 * Return TRUE if there is no more data to read from the file or
 * FALSE otherwise.
 **********************************************************************/
GBool AVCRawBinEOF(AVCRawBinFile *psFile)
{
    if (psFile == nullptr || psFile->fp == nullptr)
        return TRUE;

    /* In write access mode, always return TRUE, since we always write
     * at EOF for now.
     */
    if (psFile->eAccess != AVCRead && psFile->eAccess != AVCReadWrite)
        return TRUE;

    /* If file data size was specified, then check that we have not
     * passed that point yet...
     */
    if (psFile->nFileDataSize > 0 &&
        (psFile->nOffset+psFile->nCurPos) >= psFile->nFileDataSize)
        return TRUE;

    /* If the file pointer has been moved by AVCRawBinFSeek(), then
     * we may be at a position past EOF, but VSIFeof() would still
     * return FALSE. It also returns false if we have read just up to
     * the end of the file. EOF marker would not have been set unless
     * we try to read past that.
     *
     * To prevent this situation, if the memory buffer is empty,
     * we will try to read 1 byte from the file to force the next
     * chunk of data to be loaded (and we'll move the read pointer
     * back by 1 char after of course!).
     * If we are at the end of the file, this will trigger the EOF flag.
     */
    if ((psFile->nCurPos == 0 && psFile->nCurSize == 0) ||
        (psFile->nCurPos == AVCRAWBIN_READBUFSIZE &&
         psFile->nCurSize == AVCRAWBIN_READBUFSIZE))
    {
        GByte c;
        /* Set bDisableReadBytesEOFError=TRUE to temporarily disable
         * the EOF error message from AVCRawBinReadBytes().
         */
        bDisableReadBytesEOFError = TRUE;
        AVCRawBinReadBytes(psFile, 1, &c);
        bDisableReadBytesEOFError = FALSE;

        if (psFile->nCurPos > 0)
            AVCRawBinFSeek(psFile, -1, SEEK_CUR);
    }

    return (psFile->nCurPos == psFile->nCurSize &&
            VSIFEofL(psFile->fp));
}


/**********************************************************************
 *                          AVCRawBinRead<datatype>()
 *
 * Arc/Info files are binary files with MSB first (Motorola) byte
 * ordering.  The following functions will read from the input file
 * and return a value with the bytes ordered properly for the current
 * platform.
 **********************************************************************/
GInt16  AVCRawBinReadInt16(AVCRawBinFile *psFile)
{
    GInt16 n16Value = 0;

    AVCRawBinReadBytes(psFile, 2, (GByte*)(&n16Value));

    if (psFile->eByteOrder != geSystemByteOrder)
    {
        return (GInt16)CPL_SWAP16(n16Value);
    }

    return n16Value;
}

GInt32  AVCRawBinReadInt32(AVCRawBinFile *psFile)
{
    GInt32 n32Value = 0;

    AVCRawBinReadBytes(psFile, 4, (GByte*)(&n32Value));

    if (psFile->eByteOrder != geSystemByteOrder)
    {
        return (GInt32)CPL_SWAP32(n32Value);
    }

    return n32Value;
}

float   AVCRawBinReadFloat(AVCRawBinFile *psFile)
{
    float fValue = 0.0f;

    AVCRawBinReadBytes(psFile, 4, (GByte*)(&fValue));

    if (psFile->eByteOrder != geSystemByteOrder)
    {
        CPL_SWAP32PTR( &fValue );
    }

    return fValue;
}

double  AVCRawBinReadDouble(AVCRawBinFile *psFile)
{
    double dValue = 0.0;

    AVCRawBinReadBytes(psFile, 8, (GByte*)(&dValue));

    if (psFile->eByteOrder != geSystemByteOrder)
    {
        CPL_SWAPDOUBLE(&dValue);
    }

    return dValue;
}



/**********************************************************************
 *                          AVCRawBinWriteBytes()
 *
 * Write the number of bytes from the buffer to the file.
 *
 * If a problem happens, then CPLError() will be called and
 * CPLGetLastErrNo() can be used to test if a write operation was
 * successful.
 **********************************************************************/
void AVCRawBinWriteBytes(AVCRawBinFile *psFile, int nBytesToWrite,
                         const GByte *pBuf)
{
    /*----------------------------------------------------------------
     * Make sure file is opened with Write access
     *---------------------------------------------------------------*/
    if (psFile == nullptr ||
        (psFile->eAccess != AVCWrite && psFile->eAccess != AVCReadWrite))
    {
        CPLError(CE_Failure, CPLE_FileIO,
              "AVCRawBinWriteBytes(): call not compatible with access mode.");
        return;
    }

    if (VSIFWriteL((void*)pBuf, nBytesToWrite, 1, psFile->fp) != 1)
        CPLError(CE_Failure, CPLE_FileIO,
                 "Writing to %s failed.", psFile->pszFname);

    /*----------------------------------------------------------------
     * In write mode, we keep track of current file position ( =nbr of
     * bytes written) through psFile->nCurPos
     *---------------------------------------------------------------*/
    psFile->nCurPos += nBytesToWrite;
}


/**********************************************************************
 *                          AVCRawBinWrite<datatype>()
 *
 * Arc/Info files are binary files with MSB first (Motorola) byte
 * ordering.  The following functions will reorder the byte for the
 * value properly and write that to the output file.
 *
 * If a problem happens, then CPLError() will be called and
 * CPLGetLastErrNo() can be used to test if a write operation was
 * successful.
 **********************************************************************/
void  AVCRawBinWriteInt16(AVCRawBinFile *psFile, GInt16 n16Value)
{
    if (psFile->eByteOrder != geSystemByteOrder)
    {
        n16Value = (GInt16)CPL_SWAP16(n16Value);
    }

    AVCRawBinWriteBytes(psFile, 2, (GByte*)&n16Value);
}

void  AVCRawBinWriteInt32(AVCRawBinFile *psFile, GInt32 n32Value)
{
    if (psFile->eByteOrder != geSystemByteOrder)
    {
        n32Value = (GInt32)CPL_SWAP32(n32Value);
    }

    AVCRawBinWriteBytes(psFile, 4, (GByte*)&n32Value);
}

void  AVCRawBinWriteFloat(AVCRawBinFile *psFile, float fValue)
{
    if (psFile->eByteOrder != geSystemByteOrder)
    {
        CPL_SWAP32PTR( &fValue );
    }

    AVCRawBinWriteBytes(psFile, 4, (GByte*)&fValue);
}

void  AVCRawBinWriteDouble(AVCRawBinFile *psFile, double dValue)
{
    if (psFile->eByteOrder != geSystemByteOrder)
    {
        CPL_SWAPDOUBLE(&dValue);
    }

    AVCRawBinWriteBytes(psFile, 8, (GByte*)&dValue);
}


/**********************************************************************
 *                          AVCRawBinWriteZeros()
 *
 * Write a number of zeros (specified in bytes) at the current position
 * in the file.
 *
 * If a problem happens, then CPLError() will be called and
 * CPLGetLastErrNo() can be used to test if a write operation was
 * successful.
 **********************************************************************/
void AVCRawBinWriteZeros(AVCRawBinFile *psFile, int nBytesToWrite)
{
    char acZeros[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int i;

    /* Write by 8 bytes chunks.  The last chunk may be less than 8 bytes
     */
    for(i=0; i< nBytesToWrite; i+=8)
    {
        AVCRawBinWriteBytes(psFile, MIN(8,(nBytesToWrite-i)),
                            (GByte*)acZeros);
    }
}

/**********************************************************************
 *                          AVCRawBinWritePaddedString()
 *
 * Write a string and pad the end of the field (up to nFieldSize) with
 * spaces number of spaces at the current position in the file.
 *
 * If a problem happens, then CPLError() will be called and
 * CPLGetLastErrNo() can be used to test if a write operation was
 * successful.
 **********************************************************************/
void AVCRawBinWritePaddedString(AVCRawBinFile *psFile, int nFieldSize,
                                const GByte *pszString)
{
    char acSpaces[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    int i, nLen, numSpaces;

    /* If we're on a system with a multibyte codepage then we have to
     * convert strings to the proper multibyte encoding.
     */
    pszString = AVCE00Convert2ArcDBCS(psFile->psDBCSInfo,
                                      pszString, nFieldSize);

    nLen = (int)strlen((const char *)pszString);
    nLen = MIN(nLen, nFieldSize);
    numSpaces = nFieldSize - nLen;

    if (nLen > 0)
        AVCRawBinWriteBytes(psFile, nLen, pszString);

    /* Write spaces by 8 bytes chunks.  The last chunk may be less than 8 bytes
     */
    for(i=0; i< numSpaces; i+=8)
    {
        AVCRawBinWriteBytes(psFile, MIN(8,(numSpaces-i)),
                            (GByte*)acSpaces);
    }
}
