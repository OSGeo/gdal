/**********************************************************************
 *
 * Name:     mitab_rawbinblock.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the TABRawBinBlock class used to handle
 *           reading/writing blocks in the .MAP files
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999, 2000, Daniel Morissette
 * Copyright (c) 2014, Even Rouault <even.rouault at spatialys.com>
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
 **********************************************************************/

#include "cpl_port.h"
#include "mitab.h"

#include <cctype>
#include <cstddef>
#include <cstdio>
#include <cstring>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "mitab_priv.h"

CPL_CVSID("$Id$")

/*=====================================================================
 *                      class TABRawBinBlock
 *====================================================================*/

/**********************************************************************
 *                   TABRawBinBlock::TABRawBinBlock()
 *
 * Constructor.
 **********************************************************************/
TABRawBinBlock::TABRawBinBlock( TABAccess eAccessMode /*= TABRead*/,
                                GBool bHardBlockSize /*= TRUE*/ ) :
    m_fp(nullptr),
    m_eAccess(eAccessMode),
    m_nBlockType(0),
    m_pabyBuf(nullptr),
    m_nBlockSize(0),
    m_nSizeUsed(0),
    m_bHardBlockSize(bHardBlockSize),
    m_nFileOffset(0),
    m_nCurPos(0),
    m_nFirstBlockPtr(0),
    m_nFileSize(-1),
    m_bModified(FALSE)
{}

/**********************************************************************
 *                   TABRawBinBlock::~TABRawBinBlock()
 *
 * Destructor.
 **********************************************************************/
TABRawBinBlock::~TABRawBinBlock()
{
    if (m_pabyBuf)
        CPLFree(m_pabyBuf);
}

/**********************************************************************
 *                   TABRawBinBlock::ReadFromFile()
 *
 * Load data from the specified file location and initialize the block.
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABRawBinBlock::ReadFromFile(VSILFILE *fpSrc, int nOffset,
                                     int nSize)
{
    if (fpSrc == nullptr || nSize == 0)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABRawBinBlock::ReadFromFile(): Assertion Failed!");
        return -1;
    }

    m_fp = fpSrc;

    VSIFSeekL(fpSrc, 0, SEEK_END);
    m_nFileSize = static_cast<int>(VSIFTellL(m_fp));

    m_nFileOffset = nOffset;
    m_nCurPos = 0;
    m_bModified = FALSE;

    /*----------------------------------------------------------------
     * Alloc a buffer to contain the data
     *---------------------------------------------------------------*/
    GByte *pabyBuf = static_cast<GByte*>(CPLMalloc(nSize*sizeof(GByte)));

    /*----------------------------------------------------------------
     * Read from the file
     *---------------------------------------------------------------*/
    if (VSIFSeekL(fpSrc, nOffset, SEEK_SET) != 0 ||
        (m_nSizeUsed = static_cast<int>(VSIFReadL(pabyBuf, sizeof(GByte), nSize, fpSrc)) ) == 0 ||
        (m_bHardBlockSize && m_nSizeUsed != nSize ) )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "ReadFromFile() failed reading %d bytes at offset %d.",
                 nSize, nOffset);
        CPLFree(pabyBuf);
        return -1;
    }

    /*----------------------------------------------------------------
     * Init block with the data we just read
     *---------------------------------------------------------------*/
    return InitBlockFromData(pabyBuf, nSize, m_nSizeUsed,
                             FALSE, fpSrc, nOffset);
}

/**********************************************************************
 *                   TABRawBinBlock::CommitToFile()
 *
 * Commit the current state of the binary block to the file to which
 * it has been previously attached.
 *
 * Derived classes may want to (optionally) reimplement this method if
 * they need to do special processing before committing the block to disk.
 *
 * For files created with bHardBlockSize=TRUE, a complete block of
 * the specified size is always written, otherwise only the number of
 * used bytes in the block will be written to disk.
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABRawBinBlock::CommitToFile()
{
    int nStatus = 0;

    if (m_fp == nullptr || m_nBlockSize <= 0 || m_pabyBuf == nullptr ||
        m_nFileOffset < 0)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
        "TABRawBinBlock::CommitToFile(): Block has not been initialized yet!");
        return -1;
    }

    /*----------------------------------------------------------------
     * If block has not been modified, then just return... nothing to do.
     *---------------------------------------------------------------*/
    if (!m_bModified)
        return 0;

    /*----------------------------------------------------------------
     * Move the output file pointer to the right position...
     *---------------------------------------------------------------*/
    if (VSIFSeekL(m_fp, m_nFileOffset, SEEK_SET) != 0)
    {
        /*------------------------------------------------------------
         * Moving pointer failed... we may need to pad with zeros if
         * block destination is beyond current end of file.
         *-----------------------------------------------------------*/
        int nCurPos = static_cast<int>(VSIFTellL(m_fp));

        if (nCurPos < m_nFileOffset &&
            VSIFSeekL(m_fp, 0L, SEEK_END) == 0 &&
            (nCurPos = static_cast<int>(VSIFTellL(m_fp))) < m_nFileOffset)
        {
            const GByte cZero = 0;

            while(nCurPos < m_nFileOffset && nStatus == 0)
            {
                if (VSIFWriteL(&cZero, 1, 1, m_fp) != 1)
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                             "Failed writing 1 byte at offset %d.", nCurPos);
                    nStatus = -1;
                    break;
                }
                nCurPos++;
            }
        }

        if (nCurPos != m_nFileOffset)
            nStatus = -1; // Error message will follow below
    }

    /*----------------------------------------------------------------
     * At this point we are ready to write to the file.
     *
     * If m_bHardBlockSize==FALSE, then we do not write a complete block;
     * we write only the part of the block that was used.
     *---------------------------------------------------------------*/
    int numBytesToWrite = m_bHardBlockSize?m_nBlockSize:m_nSizeUsed;

    /*CPLDebug("MITAB", "Committing to offset %d", m_nFileOffset);*/

    if (nStatus != 0 ||
        VSIFWriteL(m_pabyBuf,sizeof(GByte),
                    numBytesToWrite, m_fp) != static_cast<size_t>(numBytesToWrite) )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed writing %d bytes at offset %d.",
                 numBytesToWrite, m_nFileOffset);
        return -1;
    }
    if( m_nFileOffset + numBytesToWrite > m_nFileSize )
    {
        m_nFileSize = m_nFileOffset + numBytesToWrite;
    }

    VSIFFlushL(m_fp);

    m_bModified = FALSE;

    return 0;
}

/**********************************************************************
 *                   TABRawBinBlock::CommitAsDeleted()
 *
 * Commit current block to file using block type 4 (garbage block)
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABRawBinBlock::CommitAsDeleted(GInt32 nNextBlockPtr)
{
    CPLErrorReset();

    if ( m_pabyBuf == nullptr )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "CommitAsDeleted(): Block has not been initialized yet!");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Create deleted block header
     *----------------------------------------------------------------*/
    GotoByteInBlock(0x000);
    WriteInt16(TABMAP_GARB_BLOCK);    // Block type code
    WriteInt32(nNextBlockPtr);

    int nStatus = CPLGetLastErrorType() == CE_Failure ? -1 : 0;

    /*-----------------------------------------------------------------
     * OK, call the base class to write the block to disk.
     *----------------------------------------------------------------*/
    if (nStatus == 0)
    {
#ifdef DEBUG_VERBOSE
        CPLDebug("MITAB", "Committing GARBAGE block to offset %d", m_nFileOffset);
#endif
        nStatus = TABRawBinBlock::CommitToFile();
        m_nSizeUsed = 0;
    }

    return nStatus;
}

/**********************************************************************
 *                   TABRawBinBlock::InitBlockFromData()
 *
 * Set the binary data buffer and initialize the block.
 *
 * Calling ReadFromFile() will automatically call InitBlockFromData() to
 * complete the initialization of the block after the data is read from the
 * file.  Derived classes should implement their own version of
 * InitBlockFromData() if they need specific initialization... in this
 * case the derived InitBlockFromData() should call
 * TABRawBinBlock::InitBlockFromData() before doing anything else.
 *
 * By default, the buffer will be copied, but if bMakeCopy = FALSE then
 * it won't be copied, and the object will keep a reference to the
 * user's buffer... and this object will eventually free the user's buffer.
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABRawBinBlock::InitBlockFromData(GByte *pabyBuf,
                                          int nBlockSize, int nSizeUsed,
                                          GBool bMakeCopy /* = TRUE */,
                                          VSILFILE *fpSrc /* = NULL */,
                                          int nOffset /* = 0 */)
{
    m_fp = fpSrc;
    m_nFileOffset = nOffset;
    m_nCurPos = 0;
    m_bModified = FALSE;

    /*----------------------------------------------------------------
     * Alloc or realloc the buffer to contain the data if necessary
     *---------------------------------------------------------------*/
    if (!bMakeCopy)
    {
        if (m_pabyBuf != nullptr)
            CPLFree(m_pabyBuf);
        m_pabyBuf = pabyBuf;
        m_nBlockSize = nBlockSize;
        m_nSizeUsed = nSizeUsed;
    }
    else if (m_pabyBuf == nullptr || nBlockSize != m_nBlockSize)
    {
        m_pabyBuf = static_cast<GByte*> (CPLRealloc(m_pabyBuf, nBlockSize*sizeof(GByte)));
        m_nBlockSize = nBlockSize;
        m_nSizeUsed = nSizeUsed;
        memcpy(m_pabyBuf, pabyBuf, m_nSizeUsed);
    }

    /*----------------------------------------------------------------
     * Extract block type... header block (first block in a file) has
     * no block type, so we assign one by default.
     *---------------------------------------------------------------*/
    if (m_nFileOffset == 0)
        m_nBlockType = TABMAP_HEADER_BLOCK;
    else
    {
        // Block type will be validated only if GetBlockType() is called
        m_nBlockType = static_cast<int>(m_pabyBuf[0]);
    }

    return 0;
}

/**********************************************************************
 *                   TABRawBinBlock::InitNewBlock()
 *
 * Initialize the block so that it knows to which file is attached,
 * its block size, etc.
 *
 * This is an alternative to calling ReadFromFile() or InitBlockFromData()
 * that puts the block in a stable state without loading any initial
 * data in it.
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABRawBinBlock::InitNewBlock(VSILFILE *fpSrc, int nBlockSize,
                                     int nFileOffset /* = 0*/)
{
    m_fp = fpSrc;
    m_nBlockSize = nBlockSize;
    m_nSizeUsed = 0;
    m_nCurPos = 0;
    m_bModified = FALSE;

    if (nFileOffset > 0)
        m_nFileOffset = nFileOffset;
    else
        m_nFileOffset = 0;

    if( m_fp != nullptr && m_nFileSize < 0 && m_eAccess == TABReadWrite )
    {
        int nCurPos = static_cast<int>(VSIFTellL(m_fp));
        VSIFSeekL(fpSrc, 0, SEEK_END);
        m_nFileSize = static_cast<int>(VSIFTellL(m_fp));
        VSIFSeekL(fpSrc, nCurPos, SEEK_SET);
    }

    m_nBlockType = -1;

    m_pabyBuf = static_cast<GByte*>(CPLRealloc(m_pabyBuf, m_nBlockSize*sizeof(GByte)));
    if( m_nBlockSize )
        memset(m_pabyBuf, 0, m_nBlockSize);

    return 0;
}

/**********************************************************************
 *                   TABRawBinBlock::GetBlockType()
 *
 * Return the block type for the current object.
 *
 * Returns a block type >= 0 if successful or -1 if an error happened, in
 * which case  CPLError() will have been called.
 **********************************************************************/
int     TABRawBinBlock::GetBlockType()
{
    if (m_pabyBuf == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GetBlockType(): Block has not been initialized.");
        return -1;
    }

    if (m_nBlockType > TABMAP_LAST_VALID_BLOCK_TYPE)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "GetBlockType(): Unsupported block type %d.",
                 m_nBlockType);
        return -1;
    }

    return m_nBlockType;
}

/**********************************************************************
 *                   TABRawBinBlock::GotoByteInBlock()
 *
 * Move the block pointer to the specified position relative to the
 * beginning of the block.
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABRawBinBlock::GotoByteInBlock(int nOffset)
{
    if ( (m_eAccess == TABRead && nOffset > m_nSizeUsed) ||
         (m_eAccess != TABRead && nOffset > m_nBlockSize) )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "GotoByteInBlock(): Attempt to go past end of data block.");
        return -1;
    }

    if (nOffset < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
               "GotoByteInBlock(): Attempt to go before start of data block.");
        return -1;
    }

    m_nCurPos = nOffset;

    m_nSizeUsed = std::max(m_nSizeUsed, m_nCurPos);

    return 0;
}

/**********************************************************************
 *                   TABRawBinBlock::GotoByteRel()
 *
 * Move the block pointer by the specified number of bytes relative
 * to its current position.
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABRawBinBlock::GotoByteRel(int nOffset)
{
    return GotoByteInBlock(m_nCurPos + nOffset);
}

/**********************************************************************
 *                   TABRawBinBlock::GotoByteInFile()
 *
 * Move the block pointer to the specified position relative to the
 * beginning of the file.
 *
 * In read access, the current block may be reloaded to contain a right
 * block of binary data if necessary.
 *
 * In write mode, the current block may automagically be committed to
 * disk and a new block initialized if necessary.
 *
 * bForceReadFromFile is used in write mode to read the new block data from
 * file instead of creating an empty block. (Useful for TABCollection
 * or other cases that need to do random access in the file in write mode.)
 *
 * bOffsetIsEndOfData is set to TRUE to indicate that the nOffset
 * to which we are attempting to go is the end of the used data in this
 * block (we are positioning ourselves to append data), so if the nOffset
 * corresponds to the beginning of a block then we should really
 * be positioning ourselves at the end of the block that ends at this
 * address instead of at the beginning of the blocks that starts at this
 * address. This case can happen when going back and forth to write collection
 * objects to a Coordblock and is documented in bug 1657.
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABRawBinBlock::GotoByteInFile(int nOffset,
                                       GBool bForceReadFromFile /*=FALSE*/,
                                       GBool bOffsetIsEndOfData /*=FALSE*/)
{
    if (nOffset < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
               "GotoByteInFile(): Attempt to go before start of file.");
        return -1;
    }

    int nNewBlockPtr =
        ( (nOffset-m_nFirstBlockPtr)/m_nBlockSize)*m_nBlockSize +
        m_nFirstBlockPtr;

    if (m_eAccess == TABRead)
    {
        if ( (nOffset<m_nFileOffset || nOffset>=m_nFileOffset+m_nSizeUsed) &&
             ReadFromFile(m_fp, nNewBlockPtr, m_nBlockSize) != 0)
        {
            // Failed reading new block... error has already been reported.
            return -1;
        }
    }
    else if (m_eAccess == TABWrite)
    {
        if ( (nOffset<m_nFileOffset || nOffset>=m_nFileOffset+m_nBlockSize) &&
             (CommitToFile() != 0 ||
              InitNewBlock(m_fp, m_nBlockSize, nNewBlockPtr) != 0 ) )
        {
            // Failed reading new block... error has already been reported.
            return -1;
        }
    }
    else if (m_eAccess == TABReadWrite)
    {
        // TODO: THIS IS NOT REAL read/write access (it is more extended write)
        // Currently we try to read from file only if explicitly requested.
        // If we ever want true read/write mode we should implement
        // more smarts to detect whether the caller wants an existing block to
        // be read, or a new one to be created from scratch.
        // CommitToFile() should only be called only if something changed.
        //
        if (bOffsetIsEndOfData &&  nOffset%m_nBlockSize == 0)
        {
            /* We're trying to go byte m_nBlockSize of a block that's full of data.
             * In this case it is okay to place the m_nCurPos at byte m_nBlockSize
             * which is past the end of the block.
             */

            /* Make sure we request the block that ends with requested
             * address and not the following block that doesn't exist
             * yet on disk */
            nNewBlockPtr -= m_nBlockSize;

            if ( (nOffset < m_nFileOffset ||
                  nOffset > m_nFileOffset+m_nBlockSize) &&
                 (CommitToFile() != 0 ||
                  (!bForceReadFromFile &&
                   InitNewBlock(m_fp, m_nBlockSize, nNewBlockPtr) != 0) ||
                  (bForceReadFromFile &&
                   ReadFromFile(m_fp, nNewBlockPtr, m_nBlockSize) != 0) )  )
            {
                // Failed reading new block... error has already been reported.
                return -1;
            }
        }
        else
        {
            if( !bForceReadFromFile && m_nFileSize > 0 &&
                nOffset < m_nFileSize )
            {
                bForceReadFromFile = TRUE;
                if ( !(nOffset < m_nFileOffset ||
                       nOffset >= m_nFileOffset+m_nBlockSize) )
                {
                    if ( (nOffset<m_nFileOffset || nOffset>=m_nFileOffset+m_nSizeUsed) &&
                         (CommitToFile() != 0 ||
                          ReadFromFile(m_fp, nNewBlockPtr, m_nBlockSize) != 0) )
                    {
                        // Failed reading new block... error has already been reported.
                        return -1;
                    }
                }
            }

            if ( (nOffset < m_nFileOffset ||
                  nOffset >= m_nFileOffset+m_nBlockSize) &&
                 (CommitToFile() != 0 ||
                  (!bForceReadFromFile &&
                   InitNewBlock(m_fp, m_nBlockSize, nNewBlockPtr) != 0) ||
                  (bForceReadFromFile &&
                   ReadFromFile(m_fp, nNewBlockPtr, m_nBlockSize) != 0) )  )
            {
                // Failed reading new block... error has already been reported.
                return -1;
            }
        }
    }
    else
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Access mode not supported yet!");
        return -1;
    }

    m_nCurPos = nOffset-m_nFileOffset;

    m_nSizeUsed = std::max(m_nSizeUsed, m_nCurPos);

    return 0;
}

/**********************************************************************
 *                   TABRawBinBlock::SetFirstBlockPtr()
 *
 * Set the position in the file at which the first block starts.
 * This value will usually be the header size and needs to be specified
 * only if the header size is different from the other blocks size.
 *
 * This value will be used by GotoByteInFile() to properly align the data
 * blocks that it loads automatically when a requested position is outside
 * of the block currently in memory.
 **********************************************************************/
void  TABRawBinBlock::SetFirstBlockPtr(int nOffset)
{
    m_nFirstBlockPtr = nOffset;
}

/**********************************************************************
 *                   TABRawBinBlock::GetNumUnusedBytes()
 *
 * Return the number of unused bytes in this block.
 **********************************************************************/
int     TABRawBinBlock::GetNumUnusedBytes()
{
    return m_nBlockSize - m_nSizeUsed;
}

/**********************************************************************
 *                   TABRawBinBlock::GetFirstUnusedByteOffset()
 *
 * Return the position of the first unused byte in this block relative
 * to the beginning of the file, or -1 if the block is full.
 **********************************************************************/
int     TABRawBinBlock::GetFirstUnusedByteOffset()
{
    if (m_nSizeUsed < m_nBlockSize)
        return m_nFileOffset + m_nSizeUsed;
    else
        return -1;
}

/**********************************************************************
 *                   TABRawBinBlock::GetCurAddress()
 *
 * Return the current pointer position, relative to beginning of file.
 **********************************************************************/
int     TABRawBinBlock::GetCurAddress()
{
    return m_nFileOffset + m_nCurPos;
}

/**********************************************************************
 *                   TABRawBinBlock::ReadBytes()
 *
 * Copy the number of bytes from the data block's internal buffer to
 * the user's buffer pointed by pabyDstBuf.
 *
 * Passing pabyDstBuf = NULL will only move the read pointer by the
 * specified number of bytes as if the copy had happened... but it
 * won't crash.
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABRawBinBlock::ReadBytes(int numBytes, GByte *pabyDstBuf)
{
    /*----------------------------------------------------------------
     * Make sure block is initialized with Read access and that the
     * operation won't go beyond the buffer's size.
     *---------------------------------------------------------------*/
    if (m_pabyBuf == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ReadBytes(): Block has not been initialized.");
        return -1;
    }

    if (m_nCurPos + numBytes > m_nSizeUsed)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ReadBytes(): Attempt to read past end of data block.");
        return -1;
    }

    if (pabyDstBuf)
    {
        memcpy(pabyDstBuf, m_pabyBuf + m_nCurPos, numBytes);
    }

    m_nCurPos += numBytes;

    return 0;
}

/**********************************************************************
 *                   TABRawBinBlock::Read<datatype>()
 *
 * MapInfo files are binary files with LSB first (Intel) byte
 * ordering.  The following functions will read from the input file
 * and return a value with the bytes ordered properly for the current
 * platform.
 **********************************************************************/
GByte  TABRawBinBlock::ReadByte()
{
    GByte byValue = 0;

    ReadBytes(1, &byValue);

    return byValue;
}

GInt16  TABRawBinBlock::ReadInt16()
{
    GInt16 n16Value = 0;

    ReadBytes(2, reinterpret_cast<GByte*>(&n16Value));

#ifdef CPL_MSB
    return static_cast<GInt16>(CPL_SWAP16(n16Value));
#else
    return n16Value;
#endif
}

GInt32  TABRawBinBlock::ReadInt32()
{
    GInt32 n32Value = 0;

    ReadBytes(4, reinterpret_cast<GByte*>(&n32Value));

#ifdef CPL_MSB
    return static_cast<GInt32>(CPL_SWAP32(n32Value));
#else
    return n32Value;
#endif
}

float   TABRawBinBlock::ReadFloat()
{
    float fValue = 0.0f;

    ReadBytes(4, reinterpret_cast<GByte*>(&fValue));

#ifdef CPL_MSB
    CPL_LSBPTR32(&fValue);
#endif
    return fValue;
}

double  TABRawBinBlock::ReadDouble()
{
    double dValue = 0.0;

    ReadBytes(8, reinterpret_cast<GByte*>(&dValue));

#ifdef CPL_MSB
    CPL_SWAPDOUBLE(&dValue);
#endif

    return dValue;
}

/**********************************************************************
 *                   TABRawBinBlock::WriteBytes()
 *
 * Copy the number of bytes from the user's buffer pointed by pabySrcBuf
 * to the data block's internal buffer.
 * Note that this call only writes to the memory buffer... nothing is
 * written to the file until WriteToFile() is called.
 *
 * Passing pabySrcBuf = NULL will only move the write pointer by the
 * specified number of bytes as if the copy had happened... but it
 * won't crash.
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int  TABRawBinBlock::WriteBytes(int nBytesToWrite, const GByte *pabySrcBuf)
{
    /*----------------------------------------------------------------
     * Make sure block is initialized with Write access and that the
     * operation won't go beyond the buffer's size.
     *---------------------------------------------------------------*/
    if (m_pabyBuf == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "WriteBytes(): Block has not been initialized.");
        return -1;
    }

    if (m_eAccess == TABRead )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "WriteBytes(): Block does not support write operations.");
        return -1;
    }

    if (m_nCurPos + nBytesToWrite > m_nBlockSize)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "WriteBytes(): Attempt to write past end of data block.");
        return -1;
    }

    /*----------------------------------------------------------------
     * Everything is OK... copy the data
     *---------------------------------------------------------------*/
    if (pabySrcBuf)
    {
        memcpy(m_pabyBuf + m_nCurPos, pabySrcBuf, nBytesToWrite);
    }

    m_nCurPos += nBytesToWrite;

    m_nSizeUsed = std::max(m_nSizeUsed, m_nCurPos);

    m_bModified = TRUE;

    return 0;
}

/**********************************************************************
 *                    TABRawBinBlock::Write<datatype>()
 *
 * Arc/Info files are binary files with MSB first (Motorola) byte
 * ordering.  The following functions will reorder the byte for the
 * value properly and write that to the output file.
 *
 * If a problem happens, then CPLError() will be called and
 * CPLGetLastErrNo() can be used to test if a write operation was
 * successful.
 **********************************************************************/
int  TABRawBinBlock::WriteByte(GByte byValue)
{
    return WriteBytes(1, &byValue);
}

int  TABRawBinBlock::WriteInt16(GInt16 n16Value)
{
#ifdef CPL_MSB
    n16Value = static_cast<GInt16>(CPL_SWAP16(n16Value));
#endif

    return WriteBytes(2, reinterpret_cast<GByte*>(&n16Value));
}

int  TABRawBinBlock::WriteInt32(GInt32 n32Value)
{
#ifdef CPL_MSB
    n32Value = static_cast<GInt32>(CPL_SWAP32(n32Value));
#endif

    return WriteBytes(4, reinterpret_cast<GByte*>(&n32Value));
}

int  TABRawBinBlock::WriteFloat(float fValue)
{
#ifdef CPL_MSB
    CPL_LSBPTR32(&fValue);
#endif

    return WriteBytes(4, reinterpret_cast<GByte*>(&fValue));
}

int  TABRawBinBlock::WriteDouble(double dValue)
{
#ifdef CPL_MSB
    CPL_SWAPDOUBLE(&dValue);
#endif

    return WriteBytes(8, reinterpret_cast<GByte*>(&dValue));
}

/**********************************************************************
 *                    TABRawBinBlock::WriteZeros()
 *
 * Write a number of zeros (specified in bytes) at the current position
 * in the file.
 *
 * If a problem happens, then CPLError() will be called and
 * CPLGetLastErrNo() can be used to test if a write operation was
 * successful.
 **********************************************************************/
int  TABRawBinBlock::WriteZeros(int nBytesToWrite)
{
    const GByte acZeros[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int nStatus = 0;

    // Write by 8 bytes chunks.  The last chunk may be less than 8 bytes.
    for( int i = 0; nStatus == 0 && i< nBytesToWrite; i += 8 )
    {
        nStatus = WriteBytes(std::min(8, nBytesToWrite - i), acZeros);
    }

    return nStatus;
}

/**********************************************************************
 *                   TABRawBinBlock::WritePaddedString()
 *
 * Write a string and pad the end of the field (up to nFieldSize) with
 * spaces number of spaces at the current position in the file.
 *
 * If a problem happens, then CPLError() will be called and
 * CPLGetLastErrNo() can be used to test if a write operation was
 * successful.
 **********************************************************************/
int  TABRawBinBlock::WritePaddedString(int nFieldSize, const char *pszString)
{
    char acSpaces[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    int i, nLen, numSpaces;
    int nStatus = 0;

    nLen = static_cast<int>(strlen(pszString));
    nLen = std::min(nLen, nFieldSize);
    numSpaces = nFieldSize - nLen;

    if (nLen > 0)
        nStatus = WriteBytes(nLen, reinterpret_cast<const GByte*>(pszString));

    /* Write spaces by 8 bytes chunks.  The last chunk may be less than 8 bytes
     */
    for(i=0; nStatus == 0 && i< numSpaces; i+=8)
    {
        nStatus = WriteBytes(std::min(8, numSpaces - i), reinterpret_cast<GByte*>(acSpaces));
    }

    return nStatus;
}

/**********************************************************************
 *                   TABRawBinBlock::Dump()
 *
 * Dump block contents... available only in DEBUG mode.
 **********************************************************************/
#ifdef DEBUG

void TABRawBinBlock::Dump(FILE *fpOut /*=NULL*/)
{
    if (fpOut == nullptr)
        fpOut = stdout;

    fprintf(fpOut, "----- TABRawBinBlock::Dump() -----\n");
    if (m_pabyBuf == nullptr)
    {
        fprintf(fpOut, "Block has not been initialized yet.");
    }
    else
    {
        if( m_nBlockType == TABMAP_GARB_BLOCK )
        {
            fprintf(fpOut,"Garbage Block (type %d) at offset %d.\n",
                                                    m_nBlockType, m_nFileOffset);
            int nNextGarbageBlock = 0;
            memcpy(&nNextGarbageBlock, m_pabyBuf + 2, 4);
            CPL_LSBPTR32(&nNextGarbageBlock);
            fprintf(fpOut,"  m_nNextGarbageBlock     = %d\n", nNextGarbageBlock);
        }
        else
        {
            fprintf(fpOut, "Block (type %d) size=%d bytes at offset %d in file.\n",
                    m_nBlockType, m_nBlockSize, m_nFileOffset);
            fprintf(fpOut, "Current pointer at byte %d\n", m_nCurPos);
        }
    }

    fflush(fpOut);
}

#endif // DEBUG

/**********************************************************************
 *                          DumpBytes()
 *
 * Read and dump the contents of an Binary file.
 **********************************************************************/
void TABRawBinBlock::DumpBytes(GInt32 nValue, int nOffset /*=0*/,
                               FILE *fpOut /*=NULL*/)
{
    float fValue = 0.0f;
    memcpy(&fValue, &nValue, 4);

    char achValue[4];
    memcpy(achValue, &nValue, 4);

    GInt16 n16Val1 = 0;
    memcpy(&n16Val1, achValue + 2, sizeof(GInt16));
    GInt16 n16Val2 = 0;
    memcpy(&n16Val2, achValue, sizeof(GInt16));

    /* For double precision values, we only use the first half
     * of the height bytes... and leave the other 4 bytes as zeros!
     * It's a bit of a hack, but it seems to be enough for the
     * precision of the values we print!
     */
#ifdef CPL_MSB
    const GInt32 anVal[2] = { nValue, 0 };
#else
    const GInt32 anVal[2] = { 0, nValue };
#endif
    double dValue = 0.0;
    memcpy(&dValue, anVal, 8);

    if (fpOut == nullptr)
        fpOut = stdout;

    fprintf(fpOut, "%d\t0x%8.8x  %-5d\t%-6d %-6d %5.3e  d=%5.3e",
                    nOffset, nValue, nValue,
                    n16Val1, n16Val2, fValue, dValue);

    fprintf(fpOut, "\t[%c%c%c%c]\n", isprint(achValue[0])?achValue[0]:'.',
                             isprint(achValue[1])?achValue[1]:'.',
                             isprint(achValue[2])?achValue[2]:'.',
                             isprint(achValue[3])?achValue[3]:'.');
}

/**********************************************************************
 *                   TABCreateMAPBlockFromFile()
 *
 * Load data from the specified file location and create and initialize
 * a TABMAP*Block of the right type to handle it.
 *
 * Returns the new object if successful or NULL if an error happened, in
 * which case CPLError() will have been called.
 **********************************************************************/
TABRawBinBlock *TABCreateMAPBlockFromFile(VSILFILE *fpSrc, int nOffset,
                                          int nSize,
                                          GBool bHardBlockSize /*= TRUE */,
                                          TABAccess eAccessMode /*= TABRead*/)
{
    if (fpSrc == nullptr || nSize == 0)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABCreateMAPBlockFromFile(): Assertion Failed!");
        return nullptr;
    }

    /*----------------------------------------------------------------
     * Alloc a buffer to contain the data
     *---------------------------------------------------------------*/
    GByte *pabyBuf = static_cast<GByte*>(CPLMalloc(nSize*sizeof(GByte)));

    /*----------------------------------------------------------------
     * Read from the file
     *---------------------------------------------------------------*/
    if (VSIFSeekL(fpSrc, nOffset, SEEK_SET) != 0 ||
        VSIFReadL(pabyBuf, sizeof(GByte), nSize, fpSrc)!=static_cast<unsigned int>(nSize) )
    {
        CPLError(CE_Failure, CPLE_FileIO,
         "TABCreateMAPBlockFromFile() failed reading %d bytes at offset %d.",
                 nSize, nOffset);
        CPLFree(pabyBuf);
        return nullptr;
    }

    /*----------------------------------------------------------------
     * Create an object of the right type
     * Header block is different: it does not start with the object
     * type byte but it is always the first block in a file
     *---------------------------------------------------------------*/
    TABRawBinBlock *poBlock = nullptr;

    if (nOffset == 0)
    {
        poBlock = new TABMAPHeaderBlock(eAccessMode);
    }
    else
    {
        switch(pabyBuf[0])
        {
          case TABMAP_INDEX_BLOCK:
            poBlock = new TABMAPIndexBlock(eAccessMode);
            break;
          case TABMAP_OBJECT_BLOCK:
            poBlock = new TABMAPObjectBlock(eAccessMode);
            break;
          case TABMAP_COORD_BLOCK:
            poBlock = new TABMAPCoordBlock(eAccessMode);
            break;
          case TABMAP_TOOL_BLOCK:
            poBlock = new TABMAPToolBlock(eAccessMode);
            break;
          case TABMAP_GARB_BLOCK:
          default:
            poBlock = new TABRawBinBlock(eAccessMode, bHardBlockSize);
            break;
        }
    }

    /*----------------------------------------------------------------
     * Init new object with the data we just read
     *---------------------------------------------------------------*/
    if (poBlock->InitBlockFromData(pabyBuf, nSize, nSize,
                                   FALSE, fpSrc, nOffset) != 0)
    {
        // Some error happened... and CPLError() has been called
        delete poBlock;
        poBlock = nullptr;
    }

    return poBlock;
}

/*=====================================================================
 *                      class TABBinBlockManager
 *====================================================================*/

/**********************************************************************
 *                   TABBinBlockManager::TABBinBlockManager()
 *
 * Constructor.
 **********************************************************************/
TABBinBlockManager::TABBinBlockManager() :
    m_nBlockSize(0),
    m_nLastAllocatedBlock(-1),
    m_psGarbageBlocksFirst(nullptr),
    m_psGarbageBlocksLast(nullptr)
{
    m_szName[0] = '\0';
}

/**********************************************************************
 *                   TABBinBlockManager::~TABBinBlockManager()
 *
 * Destructor.
 **********************************************************************/
TABBinBlockManager::~TABBinBlockManager()
{
    Reset();
}

/**********************************************************************
 *                   TABBinBlockManager::SetBlockSize()
 **********************************************************************/
void TABBinBlockManager::SetBlockSize(int nBlockSize)
{
    m_nBlockSize = nBlockSize;
}

/**********************************************************************
 *                   TABBinBlockManager::SetName()
 **********************************************************************/
void TABBinBlockManager::SetName(const char* pszName)
{
    strncpy(m_szName, pszName, sizeof(m_szName));
    m_szName[sizeof(m_szName)-1] = '\0';
}

/**********************************************************************
 *                   TABBinBlockManager::AllocNewBlock()
 *
 * Returns and reserves the address of the next available block, either a
 * brand new block at end of file, or recycle a garbage block if one is
 * available.
 **********************************************************************/
GInt32  TABBinBlockManager::AllocNewBlock(CPL_UNUSED const char* pszReason)
{
    // Try to reuse garbage blocks first
    if (GetFirstGarbageBlock() > 0)
    {
        int nRetValue = PopGarbageBlock();
#ifdef DEBUG_VERBOSE
        CPLDebug("MITAB", "AllocNewBlock(%s, %s) = %d (recycling garbage block)", m_szName, pszReason, nRetValue);
#endif
        return nRetValue;
    }

    // ... or alloc a new block at EOF
    if (m_nLastAllocatedBlock==-1)
        m_nLastAllocatedBlock = 0;
    else
    {
        CPLAssert(m_nBlockSize);
        m_nLastAllocatedBlock+=m_nBlockSize;
    }

#ifdef DEBUG_VERBOSE
    CPLDebug("MITAB", "AllocNewBlock(%s, %s) = %d", m_szName, pszReason, m_nLastAllocatedBlock);
#endif
    return m_nLastAllocatedBlock;
}

/**********************************************************************
 *                   TABBinBlockManager::Reset()
 *
 **********************************************************************/
void TABBinBlockManager::Reset()
{
    m_nLastAllocatedBlock = -1;

    // Flush list of garbage blocks
    while (m_psGarbageBlocksFirst != nullptr)
    {
        TABBlockRef *psNext = m_psGarbageBlocksFirst->psNext;
        CPLFree(m_psGarbageBlocksFirst);
        m_psGarbageBlocksFirst = psNext;
    }
    m_psGarbageBlocksLast = nullptr;
}

/**********************************************************************
 *                   TABBinBlockManager::PushGarbageBlockAsFirst()
 *
 * Insert a garbage block at the head of the list of garbage blocks.
 **********************************************************************/
void TABBinBlockManager::PushGarbageBlockAsFirst(GInt32 nBlockPtr)
{
    TABBlockRef *psNewBlockRef = static_cast<TABBlockRef *>(CPLMalloc(sizeof(TABBlockRef)));

    psNewBlockRef->nBlockPtr = nBlockPtr;
    psNewBlockRef->psPrev = nullptr;
    psNewBlockRef->psNext = m_psGarbageBlocksFirst;

    if( m_psGarbageBlocksFirst != nullptr )
        m_psGarbageBlocksFirst->psPrev = psNewBlockRef;
    m_psGarbageBlocksFirst = psNewBlockRef;
    if( m_psGarbageBlocksLast == nullptr )
        m_psGarbageBlocksLast = m_psGarbageBlocksFirst;
}

/**********************************************************************
 *                   TABBinBlockManager::PushGarbageBlockAsLast()
 *
 * Insert a garbage block at the tail of the list of garbage blocks.
 **********************************************************************/
void TABBinBlockManager::PushGarbageBlockAsLast(GInt32 nBlockPtr)
{
    TABBlockRef *psNewBlockRef = static_cast<TABBlockRef *>(CPLMalloc(sizeof(TABBlockRef)));

    psNewBlockRef->nBlockPtr = nBlockPtr;
    psNewBlockRef->psPrev = m_psGarbageBlocksLast;
    psNewBlockRef->psNext = nullptr;

    if( m_psGarbageBlocksLast != nullptr )
        m_psGarbageBlocksLast->psNext = psNewBlockRef;
    m_psGarbageBlocksLast = psNewBlockRef;
    if( m_psGarbageBlocksFirst == nullptr )
        m_psGarbageBlocksFirst = m_psGarbageBlocksLast;
}

/**********************************************************************
 *                   TABBinBlockManager::GetFirstGarbageBlock()
 *
 * Return address of the block at the head of the list of garbage blocks
 * or 0 if the list is empty.
 **********************************************************************/
GInt32 TABBinBlockManager::GetFirstGarbageBlock()
{
    if (m_psGarbageBlocksFirst)
        return m_psGarbageBlocksFirst->nBlockPtr;

    return 0;
}

/**********************************************************************
 *                   TABBinBlockManager::PopGarbageBlock()
 *
 * Return address of the block at the head of the list of garbage blocks
 * and remove that block from the list.
 * Returns 0 if the list is empty.
 **********************************************************************/
GInt32 TABBinBlockManager::PopGarbageBlock()
{
    GInt32 nBlockPtr = 0;

    if (m_psGarbageBlocksFirst)
    {
        nBlockPtr = m_psGarbageBlocksFirst->nBlockPtr;
        TABBlockRef *psNext = m_psGarbageBlocksFirst->psNext;
        CPLFree(m_psGarbageBlocksFirst);
        if( psNext != nullptr )
            psNext->psPrev = nullptr;
        else
            m_psGarbageBlocksLast = nullptr;
        m_psGarbageBlocksFirst = psNext;
    }

    return nBlockPtr;
}
