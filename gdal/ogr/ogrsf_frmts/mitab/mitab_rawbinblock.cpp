/**********************************************************************
 * $Id: mitab_rawbinblock.cpp,v 1.11 2007-06-11 14:40:03 dmorissette Exp $
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
 * $Log: mitab_rawbinblock.cpp,v $
 * Revision 1.11  2007-06-11 14:40:03  dmorissette
 * Fixed another issue related to attempting to read past EOF while writing
 * collections (bug 1657)
 *
 * Revision 1.10  2007/02/22 18:35:53  dmorissette
 * Fixed problem writing collections where MITAB was sometimes trying to
 * read past EOF in write mode (bug 1657).
 *
 * Revision 1.9  2006/11/28 18:49:08  dmorissette
 * Completed changes to split TABMAPObjectBlocks properly and produce an
 * optimal spatial index (bug 1585)
 *
 * Revision 1.8  2005/10/06 19:15:31  dmorissette
 * Collections: added support for reading/writing pen/brush/symbol ids and
 * for writing collection objects to .TAB/.MAP (bug 1126)
 *
 * Revision 1.7  2004/12/01 18:25:03  dmorissette
 * Fixed potential memory leaks in error conditions (bug 881)
 *
 * Revision 1.6  2004/06/30 20:29:04  dmorissette
 * Fixed refs to old address danmo@videotron.ca
 *
 * Revision 1.5  2000/02/28 17:06:06  daniel
 * Added m_bModified flag
 *
 * Revision 1.4  2000/01/15 22:30:45  daniel
 * Switch to MIT/X-Consortium OpenSource license
 *
 * Revision 1.3  1999/09/26 14:59:37  daniel
 * Implemented write support
 *
 * Revision 1.2  1999/09/16 02:39:17  daniel
 * Completed read support for most feature types
 *
 * Revision 1.1  1999/07/12 04:18:25  daniel
 * Initial checkin
 *
 **********************************************************************/

#include "mitab.h"

/*=====================================================================
 *                      class TABRawBinBlock
 *====================================================================*/


/**********************************************************************
 *                   TABRawBinBlock::TABRawBinBlock()
 *
 * Constructor.
 **********************************************************************/
TABRawBinBlock::TABRawBinBlock(TABAccess eAccessMode /*= TABRead*/,
                               GBool bHardBlockSize /*= TRUE*/)
{
    m_fp = NULL;
    m_pabyBuf = NULL;
    m_nFirstBlockPtr = 0;
    m_nBlockSize = m_nSizeUsed = m_nFileOffset = m_nCurPos = 0;
    m_bHardBlockSize = bHardBlockSize;

    m_bModified = FALSE;

    m_eAccess = eAccessMode; 

}

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
 * Returns 0 if succesful or -1 if an error happened, in which case 
 * CPLError() will have been called.
 **********************************************************************/
int     TABRawBinBlock::ReadFromFile(FILE *fpSrc, int nOffset, 
                                     int nSize /*= 512*/)
{
    GByte *pabyBuf;

    if (fpSrc == NULL || nSize == 0)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed, 
                 "TABRawBinBlock::ReadFromFile(): Assertion Failed!");
        return -1;
    }

    m_fp = fpSrc;
    m_nFileOffset = nOffset;
    m_nCurPos = 0;
    m_bModified = FALSE;
    
    /*----------------------------------------------------------------
     * Alloc a buffer to contain the data
     *---------------------------------------------------------------*/
    pabyBuf = (GByte*)CPLMalloc(nSize*sizeof(GByte));

    /*----------------------------------------------------------------
     * Read from the file
     *---------------------------------------------------------------*/
    if (VSIFSeek(fpSrc, nOffset, SEEK_SET) != 0 ||
        (m_nSizeUsed = VSIFRead(pabyBuf, sizeof(GByte), nSize, fpSrc) ) == 0 ||
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
 * Returns 0 if succesful or -1 if an error happened, in which case 
 * CPLError() will have been called.
 **********************************************************************/
int     TABRawBinBlock::CommitToFile()
{
    int nStatus = 0;

    if (m_fp == NULL || m_nBlockSize <= 0 || m_pabyBuf == NULL ||
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
    if (VSIFSeek(m_fp, m_nFileOffset, SEEK_SET) != 0)
    {
        /*------------------------------------------------------------
         * Moving pointer failed... we may need to pad with zeros if 
         * block destination is beyond current end of file.
         *-----------------------------------------------------------*/
        int nCurPos;
        nCurPos = VSIFTell(m_fp);

        if (nCurPos < m_nFileOffset &&
            VSIFSeek(m_fp, 0L, SEEK_END) == 0 &&
            (nCurPos = VSIFTell(m_fp)) < m_nFileOffset)
        {
            GByte cZero = 0;

            while(nCurPos < m_nFileOffset && nStatus == 0)
            {
                if (VSIFWrite(&cZero, 1, 1, m_fp) != 1)
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

    if (nStatus != 0 ||
        VSIFWrite(m_pabyBuf,sizeof(GByte),
                    numBytesToWrite, m_fp) != (size_t)numBytesToWrite )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed writing %d bytes at offset %d.",
                 numBytesToWrite, m_nFileOffset);
        return -1;
    }

    fflush(m_fp);

    m_bModified = FALSE;

    return 0;
}

/**********************************************************************
 *                   TABRawBinBlock::CommitAsDeleted()
 *
 * Commit current block to file using block type 4 (garbage block)
 *
 * Returns 0 if succesful or -1 if an error happened, in which case 
 * CPLError() will have been called.
 **********************************************************************/
int     TABRawBinBlock::CommitAsDeleted(GInt32 nNextBlockPtr)
{
    int nStatus = 0;

    CPLErrorReset();

    if ( m_pabyBuf == NULL )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed, 
                 "CommitAsDeleted(): Block has not been initialized yet!");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Create deleted block header
     *----------------------------------------------------------------*/
    GotoByteInBlock(0x000);
    WriteInt32(nNextBlockPtr);

    if( CPLGetLastErrorType() == CE_Failure )
        nStatus = CPLGetLastErrorNo();

    /*-----------------------------------------------------------------
     * OK, call the base class to write the block to disk.
     *----------------------------------------------------------------*/
    if (nStatus == 0)
        nStatus = TABRawBinBlock::CommitToFile();

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
 * Returns 0 if succesful or -1 if an error happened, in which case 
 * CPLError() will have been called.
 **********************************************************************/
int     TABRawBinBlock::InitBlockFromData(GByte *pabyBuf, 
                                          int nBlockSize, int nSizeUsed, 
                                          GBool bMakeCopy /* = TRUE */,
                                          FILE *fpSrc /* = NULL */, 
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
        if (m_pabyBuf != NULL)
            CPLFree(m_pabyBuf);
        m_pabyBuf = pabyBuf;
        m_nBlockSize = nBlockSize;
        m_nSizeUsed = nSizeUsed;
    }
    else if (m_pabyBuf == NULL || nBlockSize != m_nBlockSize)
    {
        m_pabyBuf = (GByte*)CPLRealloc(m_pabyBuf, nBlockSize*sizeof(GByte));
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
        m_nBlockType = (int)m_pabyBuf[0];
    }

    return 0;
}

/**********************************************************************
 *                   TABRawBinBlock::InitNewBlock()
 *
 * Initialize the block so that it knows to which file is is attached,
 * its block size, etc.
 *
 * This is an alternative to calling ReadFromFile() or InitBlockFromData()
 * that puts the block in a stable state without loading any initial
 * data in it.
 *
 * Returns 0 if succesful or -1 if an error happened, in which case 
 * CPLError() will have been called.
 **********************************************************************/
int     TABRawBinBlock::InitNewBlock(FILE *fpSrc, int nBlockSize, 
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

    m_nBlockType = -1;

    m_pabyBuf = (GByte*)CPLRealloc(m_pabyBuf, m_nBlockSize*sizeof(GByte));
    memset(m_pabyBuf, 0, m_nBlockSize);

    return 0;
}


/**********************************************************************
 *                   TABRawBinBlock::GetBlockType()
 *
 * Return the block type for the current object.
 *
 * Returns a block type >= 0 if succesful or -1 if an error happened, in 
 * which case  CPLError() will have been called.
 **********************************************************************/
int     TABRawBinBlock::GetBlockType()
{
    if (m_pabyBuf == NULL)
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
 * Returns 0 if succesful or -1 if an error happened, in which case 
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
    
    m_nSizeUsed = MAX(m_nSizeUsed, m_nCurPos);

    return 0;
}

/**********************************************************************
 *                   TABRawBinBlock::GotoByteRel()
 *
 * Move the block pointer by the specified number of bytes relative
 * to its current position.
 *
 * Returns 0 if succesful or -1 if an error happened, in which case 
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
 * block (we are positioninig ourselves to append data), so if the nOffset 
 * corresponds to the beginning of a 512 bytes block then we should really 
 * be positioning ourselves at the end of the block that ends at this 
 * address instead of at the beginning of the blocks that starts at this 
 * address. This case can happen when going back and forth to write collection
 * objects to a Coordblock and is documented in bug 1657.
 *
 * Returns 0 if succesful or -1 if an error happened, in which case 
 * CPLError() will have been called.
 **********************************************************************/
int     TABRawBinBlock::GotoByteInFile(int nOffset, 
                                       GBool bForceReadFromFile /*=FALSE*/,
                                       GBool bOffsetIsEndOfData /*=FALSE*/)
{
    int nNewBlockPtr;

    if (nOffset < 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
               "GotoByteInFile(): Attempt to go before start of file.");
        return -1;
    }

    nNewBlockPtr = ( (nOffset-m_nFirstBlockPtr)/m_nBlockSize)*m_nBlockSize +
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
        // TODO: THIS IS NOT REAL read/write access (it's more extended write)
        // Currently we try to read from file only if explicitly requested.
        // If we ever want true read/write mode we should implement
        // more smarts to detect whether the caller wants an existing block to
        // be read, or a new one to be created from scratch.
        // CommitToFile() should only be called only if something changed.
        //
        if (bOffsetIsEndOfData &&  nOffset%m_nBlockSize == 0)
        {
            /* We're trying to go byte 512 of a block that's full of data.
             * In this case it's okay to place the m_nCurPos at byte 512
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

    m_nSizeUsed = MAX(m_nSizeUsed, m_nCurPos);

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
    return (m_nBlockSize - m_nSizeUsed);
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
    return (m_nFileOffset + m_nCurPos);
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
 * Returns 0 if succesful or -1 if an error happened, in which case 
 * CPLError() will have been called.
 **********************************************************************/
int     TABRawBinBlock::ReadBytes(int numBytes, GByte *pabyDstBuf)
{
    /*----------------------------------------------------------------
     * Make sure block is initialized with Read access and that the
     * operation won't go beyond the buffer's size.
     *---------------------------------------------------------------*/
    if (m_pabyBuf == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ReadBytes(): Block has not been initialized.");
        return -1;
    }

    if (m_eAccess != TABRead && m_eAccess != TABReadWrite )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "ReadBytes(): Block does not support read operations.");
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
    GByte byValue;

    ReadBytes(1, (GByte*)(&byValue));

    return byValue;
}

GInt16  TABRawBinBlock::ReadInt16()
{
    GInt16 n16Value;

    ReadBytes(2, (GByte*)(&n16Value));

#ifdef CPL_MSB
    return (GInt16)CPL_SWAP16(n16Value);
#else
    return n16Value;
#endif
}

GInt32  TABRawBinBlock::ReadInt32()
{
    GInt32 n32Value;

    ReadBytes(4, (GByte*)(&n32Value));

#ifdef CPL_MSB
    return (GInt32)CPL_SWAP32(n32Value);
#else
    return n32Value;
#endif
}

float   TABRawBinBlock::ReadFloat()
{
    float fValue;

    ReadBytes(4, (GByte*)(&fValue));

#ifdef CPL_MSB
    *(GUInt32*)(&fValue) = CPL_SWAP32(*(GUInt32*)(&fValue));
#endif
    return fValue;
}

double  TABRawBinBlock::ReadDouble()
{
    double dValue;

    ReadBytes(8, (GByte*)(&dValue));

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
 * Returns 0 if succesful or -1 if an error happened, in which case 
 * CPLError() will have been called.
 **********************************************************************/
int  TABRawBinBlock::WriteBytes(int nBytesToWrite, GByte *pabySrcBuf)
{
    /*----------------------------------------------------------------
     * Make sure block is initialized with Write access and that the
     * operation won't go beyond the buffer's size.
     *---------------------------------------------------------------*/
    if (m_pabyBuf == NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "WriteBytes(): Block has not been initialized.");
        return -1;
    }

    if (m_eAccess != TABWrite && m_eAccess != TABReadWrite )
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

    m_nSizeUsed = MAX(m_nSizeUsed, m_nCurPos);

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
 * succesful.
 **********************************************************************/
int  TABRawBinBlock::WriteByte(GByte byValue)
{
    return WriteBytes(1, (GByte*)&byValue);
}

int  TABRawBinBlock::WriteInt16(GInt16 n16Value)
{
#ifdef CPL_MSB
    n16Value = (GInt16)CPL_SWAP16(n16Value);
#endif

    return WriteBytes(2, (GByte*)&n16Value);
}

int  TABRawBinBlock::WriteInt32(GInt32 n32Value)
{
#ifdef CPL_MSB
    n32Value = (GInt32)CPL_SWAP32(n32Value);
#endif

    return WriteBytes(4, (GByte*)&n32Value);
}

int  TABRawBinBlock::WriteFloat(float fValue)
{
#ifdef CPL_MSB
    *(GUInt32*)(&fValue) = CPL_SWAP32(*(GUInt32*)(&fValue));
#endif

    return WriteBytes(4, (GByte*)&fValue);
}

int  TABRawBinBlock::WriteDouble(double dValue)
{
#ifdef CPL_MSB
    CPL_SWAPDOUBLE(&dValue);
#endif

    return WriteBytes(8, (GByte*)&dValue);
}


/**********************************************************************
 *                    TABRawBinBlock::WriteZeros()
 *
 * Write a number of zeros (sepcified in bytes) at the current position 
 * in the file.
 *
 * If a problem happens, then CPLError() will be called and 
 * CPLGetLastErrNo() can be used to test if a write operation was 
 * succesful.
 **********************************************************************/
int  TABRawBinBlock::WriteZeros(int nBytesToWrite)
{
    char acZeros[8] = {0, 0, 0, 0, 0, 0, 0, 0};
    int i;
    int nStatus = 0;

    /* Write by 8 bytes chunks.  The last chunk may be less than 8 bytes 
     */
    for(i=0; nStatus == 0 && i< nBytesToWrite; i+=8)
    {
        nStatus = WriteBytes(MIN(8,(nBytesToWrite-i)), (GByte*)acZeros);
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
 * succesful.
 **********************************************************************/
int  TABRawBinBlock::WritePaddedString(int nFieldSize, const char *pszString)
{
    char acSpaces[8] = {' ', ' ', ' ', ' ', ' ', ' ', ' ', ' '};
    int i, nLen, numSpaces;
    int nStatus = 0;

    nLen = strlen(pszString);
    nLen = MIN(nLen, nFieldSize);
    numSpaces = nFieldSize - nLen;

    if (nLen > 0)
        nStatus = WriteBytes(nLen, (GByte*)pszString);

    /* Write spaces by 8 bytes chunks.  The last chunk may be less than 8 bytes
     */
    for(i=0; nStatus == 0 && i< numSpaces; i+=8)
    {
        nStatus = WriteBytes(MIN(8,(numSpaces-i)), (GByte*)acSpaces);
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
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "----- TABRawBinBlock::Dump() -----\n");
    if (m_pabyBuf == NULL)
    {
        fprintf(fpOut, "Block has not been initialized yet.");
    }
    else
    {
        fprintf(fpOut, "Block (type %d) size=%d bytes at offset %d in file.\n",
                m_nBlockType, m_nBlockSize, m_nFileOffset);
        fprintf(fpOut, "Current pointer at byte %d\n", m_nCurPos);
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
    GInt32      anVal[2];
    GInt16      *pn16Val1, *pn16Val2;
    float       *pfValue;
    char        *pcValue;
    double      *pdValue;


    pfValue = (float*)(&nValue);
    pcValue = (char*)(&nValue);
    pdValue = (double*)anVal;

    pn16Val1 = (GInt16*)(pcValue+2);
    pn16Val2 = (GInt16*)(pcValue);

    anVal[0] = anVal[1] = 0;

    /* For double precision values, we only use the first half 
     * of the height bytes... and leave the other 4 bytes as zeros!
     * It's a bit of a hack, but it seems to be enough for the 
     * precision of the values we print!
     */
#ifdef CPL_MSB
    anVal[0] = nValue;
#else
    anVal[1] = nValue;
#endif

    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "%d\t0x%8.8x  %-5d\t%-6d %-6d %5.3e  d=%5.3e",
                    nOffset, nValue, nValue,
                    *pn16Val1, *pn16Val2, *pfValue, *pdValue);

    printf("\t[%c%c%c%c]\n", isprint(pcValue[0])?pcValue[0]:'.',
                             isprint(pcValue[1])?pcValue[1]:'.',
                             isprint(pcValue[2])?pcValue[2]:'.',
                             isprint(pcValue[3])?pcValue[3]:'.');
}



/**********************************************************************
 *                   TABCreateMAPBlockFromFile()
 *
 * Load data from the specified file location and create and initialize 
 * a TABMAP*Block of the right type to handle it.
 *
 * Returns the new object if succesful or NULL if an error happened, in 
 * which case CPLError() will have been called.
 **********************************************************************/
TABRawBinBlock *TABCreateMAPBlockFromFile(FILE *fpSrc, int nOffset, 
                                          int nSize /*= 512*/, 
                                          GBool bHardBlockSize /*= TRUE */,
                                          TABAccess eAccessMode /*= TABRead*/)
{
    TABRawBinBlock *poBlock = NULL;
    GByte *pabyBuf;

    if (fpSrc == NULL || nSize == 0)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed, 
                 "TABCreateMAPBlockFromFile(): Assertion Failed!");
        return NULL;
    }

    /*----------------------------------------------------------------
     * Alloc a buffer to contain the data
     *---------------------------------------------------------------*/
    pabyBuf = (GByte*)CPLMalloc(nSize*sizeof(GByte));

    /*----------------------------------------------------------------
     * Read from the file
     *---------------------------------------------------------------*/
    if (VSIFSeek(fpSrc, nOffset, SEEK_SET) != 0 ||
        VSIFRead(pabyBuf, sizeof(GByte), nSize, fpSrc)!=(unsigned int)nSize )
    {
        CPLError(CE_Failure, CPLE_FileIO,
         "TABCreateMAPBlockFromFile() failed reading %d bytes at offset %d.",
                 nSize, nOffset);
        CPLFree(pabyBuf);
        return NULL;
    }

    /*----------------------------------------------------------------
     * Create an object of the right type
     * Header block is different: it does not start with the object 
     * type byte but it is always the first block in a file
     *---------------------------------------------------------------*/
    if (nOffset == 0)
    {
        poBlock = new TABMAPHeaderBlock;
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
        poBlock = NULL;
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
TABBinBlockManager::TABBinBlockManager(int nBlockSize /*=512*/)
{

    m_nBlockSize=nBlockSize;
    m_nLastAllocatedBlock = -1;
    m_psGarbageBlocks = NULL;
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
 *                   TABBinBlockManager::AllocNewBlock()
 *
 * Returns and reserves the address of the next available block, either a 
 * brand new block at end of file, or recycle a garbage block if one is 
 * available.
 **********************************************************************/
GInt32  TABBinBlockManager::AllocNewBlock()
{
    // Try to reuse garbage blocks first
    if (GetFirstGarbageBlock() > 0)
        return PopGarbageBlock();

    // ... or alloc a new block at EOF
    if (m_nLastAllocatedBlock==-1)
        m_nLastAllocatedBlock = 0;
    else
        m_nLastAllocatedBlock+=m_nBlockSize;

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
    while (m_psGarbageBlocks != NULL)
    {
        TABBlockRef *psNext = m_psGarbageBlocks->psNext;
        CPLFree(m_psGarbageBlocks);
        m_psGarbageBlocks = psNext;
    }
}

/**********************************************************************
 *                   TABBinBlockManager::PushGarbageBlock()
 *
 * Insert a garbage block at the head of the list of garbage blocks.
 **********************************************************************/
void TABBinBlockManager::PushGarbageBlock(GInt32 nBlockPtr)
{
    TABBlockRef *psNewBlockRef = (TABBlockRef *)CPLMalloc(sizeof(TABBlockRef));

    if (psNewBlockRef)
    {
        psNewBlockRef->nBlockPtr = nBlockPtr;
        psNewBlockRef->psNext = m_psGarbageBlocks;
        m_psGarbageBlocks = psNewBlockRef;
    }
}

/**********************************************************************
 *                   TABBinBlockManager::GetFirstGarbageBlock()
 *
 * Return address of the block at the head of the list of garbage blocks
 * or 0 if the list is empty.
 **********************************************************************/
GInt32 TABBinBlockManager::GetFirstGarbageBlock()
{
    if (m_psGarbageBlocks)
        return m_psGarbageBlocks->nBlockPtr;

    return 0;
}

/**********************************************************************
 *                   TABBinBlockManager::PopGarbageBlock()
 *
 * Return address of the block at the head of the list of garbage blocks
 * and remove that block from the list.
 * Retuns 0 if the list is empty.
 **********************************************************************/
GInt32 TABBinBlockManager::PopGarbageBlock()
{
    GInt32 nBlockPtr = 0;

    if (m_psGarbageBlocks)
    {
        nBlockPtr = m_psGarbageBlocks->nBlockPtr;
        TABBlockRef *psNext = m_psGarbageBlocks->psNext;
        CPLFree(m_psGarbageBlocks);
        m_psGarbageBlocks = psNext;
    }

    return nBlockPtr;
}

