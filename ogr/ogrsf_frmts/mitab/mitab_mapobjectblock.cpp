/**********************************************************************
 * $Id: mitab_mapobjectblock.cpp,v 1.6 2000/01/15 22:30:44 daniel Exp $
 *
 * Name:     mitab_mapobjectblock.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the TABMAPObjectBlock class used to handle
 *           reading/writing of the .MAP files' object data blocks
 * Author:   Daniel Morissette, danmo@videotron.ca
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
 * $Log: mitab_mapobjectblock.cpp,v $
 * Revision 1.6  2000/01/15 22:30:44  daniel
 * Switch to MIT/X-Consortium OpenSource license
 *
 * Revision 1.5  1999/10/19 06:07:29  daniel
 * Removed obsolete comment.
 *
 * Revision 1.4  1999/10/18 15:41:00  daniel
 * Added WriteIntMBRCoord()
 *
 * Revision 1.3  1999/09/29 04:23:06  daniel
 * Fixed typo in GetMBR()
 *
 * Revision 1.2  1999/09/26 14:59:37  daniel
 * Implemented write support
 *
 * Revision 1.1  1999/07/12 04:18:25  daniel
 * Initial checkin
 *
 **********************************************************************/

#include "mitab.h"

/*=====================================================================
 *                      class TABMAPObjectBlock
 *====================================================================*/

#define MAP_OBJECT_HEADER_SIZE   20

/**********************************************************************
 *                   TABMAPObjectBlock::TABMAPObjectBlock()
 *
 * Constructor.
 **********************************************************************/
TABMAPObjectBlock::TABMAPObjectBlock(TABAccess eAccessMode /*= TABRead*/):
    TABRawBinBlock(eAccessMode, TRUE)
{

}

/**********************************************************************
 *                   TABMAPObjectBlock::~TABMAPObjectBlock()
 *
 * Destructor.
 **********************************************************************/
TABMAPObjectBlock::~TABMAPObjectBlock()
{

    m_nMinX = 1000000000;
    m_nMinY = 1000000000;
    m_nMaxX = -1000000000;
    m_nMaxY = -1000000000;

}


/**********************************************************************
 *                   TABMAPObjectBlock::InitBlockFromData()
 *
 * Perform some initialization on the block after its binary data has
 * been set or changed (or loaded from a file).
 *
 * Returns 0 if succesful or -1 if an error happened, in which case 
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPObjectBlock::InitBlockFromData(GByte *pabyBuf, int nSize, 
                                         GBool bMakeCopy /* = TRUE */,
                                         FILE *fpSrc /* = NULL */, 
                                         int nOffset /* = 0 */)
{
    int nStatus;

    /*-----------------------------------------------------------------
     * First of all, we must call the base class' InitBlockFromData()
     *----------------------------------------------------------------*/
    nStatus = TABRawBinBlock::InitBlockFromData(pabyBuf, nSize, bMakeCopy,
                                            fpSrc, nOffset);
    if (nStatus != 0)   
        return nStatus;

    /*-----------------------------------------------------------------
     * Validate block type
     *----------------------------------------------------------------*/
    if (m_nBlockType != TABMAP_OBJECT_BLOCK)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "InitBlockFromData(): Invalid Block Type: got %d expected %d",
                 m_nBlockType, TABMAP_OBJECT_BLOCK);
        CPLFree(m_pabyBuf);
        m_pabyBuf = NULL;
        return -1;
    }

    /*-----------------------------------------------------------------
     * Init member variables
     *----------------------------------------------------------------*/
    GotoByteInBlock(0x002);
    m_numDataBytes = ReadInt16();       /* Excluding 4 bytes header */

    m_nCenterX = ReadInt32();
    m_nCenterY = ReadInt32();

    m_nFirstCoordBlock = ReadInt32();
    m_nLastCoordBlock = ReadInt32();

    return 0;
}

/**********************************************************************
 *                   TABMAPObjectBlock::CommitToFile()
 *
 * Commit the current state of the binary block to the file to which 
 * it has been previously attached.
 *
 * This method makes sure all values are properly set in the map object
 * block header and then calls TABRawBinBlock::CommitToFile() to do
 * the actual writing to disk.
 *
 * Returns 0 if succesful or -1 if an error happened, in which case 
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPObjectBlock::CommitToFile()
{
    int nStatus = 0;

    if ( m_pabyBuf == NULL )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed, 
     "TABMAPObjectBlock::CommitToFile(): Block has not been initialized yet!");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Make sure 20 bytes block header is up to date.
     *----------------------------------------------------------------*/
    GotoByteInBlock(0x000);

    WriteInt16(TABMAP_OBJECT_BLOCK);    // Block type code
    WriteInt16(m_nSizeUsed - MAP_OBJECT_HEADER_SIZE); // num. bytes used
    
    WriteInt32(m_nCenterX);
    WriteInt32(m_nCenterY);

    WriteInt32(m_nFirstCoordBlock);
    WriteInt32(m_nLastCoordBlock);

    nStatus = CPLGetLastErrorNo();

    /*-----------------------------------------------------------------
     * OK, call the base class to write the block to disk.
     *----------------------------------------------------------------*/
    if (nStatus == 0)
        nStatus = TABRawBinBlock::CommitToFile();

    return nStatus;
}

/**********************************************************************
 *                   TABMAPObjectBlock::InitNewBlock()
 *
 * Initialize a newly created block so that it knows to which file it
 * is attached, its block size, etc . and then perform any specific 
 * initialization for this block type, including writing a default 
 * block header, etc. and leave the block ready to receive data.
 *
 * This is an alternative to calling ReadFromFile() or InitBlockFromData()
 * that puts the block in a stable state without loading any initial
 * data in it.
 *
 * Returns 0 if succesful or -1 if an error happened, in which case 
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPObjectBlock::InitNewBlock(FILE *fpSrc, int nBlockSize, 
                                        int nFileOffset /* = 0*/)
{
    /*-----------------------------------------------------------------
     * Start with the default initialisation
     *----------------------------------------------------------------*/
    if ( TABRawBinBlock::InitNewBlock(fpSrc, nBlockSize, nFileOffset) != 0)
        return -1;

    /*-----------------------------------------------------------------
     * And then set default values for the block header.
     *----------------------------------------------------------------*/
    // Set block MBR to extreme values to force an update on the first
    // WriteIntCoord() call.
    m_nMinX = 1000000000;
    m_nMaxX = -1000000000;
    m_nMinY = 1000000000;
    m_nMaxY = -1000000000;


    m_numDataBytes = 0;       /* Data size excluding header */
    m_nCenterX = m_nCenterY = 0;
    m_nFirstCoordBlock = 0;
    m_nLastCoordBlock = 0;

    if (m_eAccess != TABRead)
    {
        GotoByteInBlock(0x000);

        WriteInt16(TABMAP_OBJECT_BLOCK);// Block type code
        WriteInt16(0);                  // num. bytes used, excluding header
    
        // MBR center here... will be written in CommitToFile()
        WriteInt32(0);
        WriteInt32(0);

        // First/last coord block ref... will be written in CommitToFile()
        WriteInt32(0);
        WriteInt32(0);
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPObjectBlock::ReadCoord()
 *
 * Read the next pair of integer coordinates value from the block, and
 * apply the translation relative to to the center of the data block
 * if bCompressed=TRUE.
 *
 * This means that the returned coordinates are always absolute integer
 * coordinates, even when the source coords are in compressed form.
 *
 * Returns 0 if succesful or -1 if an error happened, in which case 
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPObjectBlock::ReadIntCoord(GBool bCompressed, 
                                        GInt32 &nX, GInt32 &nY)
{
    if (bCompressed)
    {   
        nX = m_nCenterX + ReadInt16();
        nY = m_nCenterY + ReadInt16();
    }
    else
    {
        nX = ReadInt32();
        nY = ReadInt32();
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPObjectBlock::WriteIntCoord()
 *
 * Write a pair of integer coordinates values to the current position in the
 * the block.  For now, compressed integer coordinates are NOT supported.
 *
 * This function also keeps track of the block's MBR and center.
 *
 * Returns 0 if succesful or -1 if an error happened, in which case 
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPObjectBlock::WriteIntCoord(GInt32 nX, GInt32 nY,
                                         GBool bUpdateMBR /*=TRUE*/)
{
    /*-----------------------------------------------------------------
     * Write coords to the file.
     *----------------------------------------------------------------*/
    if (WriteInt32(nX) != 0 ||
        WriteInt32(nY) != 0 )
    {
        return -1;
    }

    /*-----------------------------------------------------------------
     * Update MBR and block center unless explicitly requested not to do so.
     *----------------------------------------------------------------*/
    if (bUpdateMBR)
    {
        if (nX < m_nMinX)
            m_nMinX = nX;
        if (nX > m_nMaxX)
            m_nMaxX = nX;

        if (nY < m_nMinY)
            m_nMinY = nY;
        if (nY > m_nMaxY)
            m_nMaxY = nY;
    
        m_nCenterX = (m_nMinX + m_nMaxX) /2;
        m_nCenterY = (m_nMinY + m_nMaxY) /2;
    }

    return 0;
}

/**********************************************************************
 *                   TABMAPObjectBlock::WriteIntMBRCoord()
 *
 * Write 2 pairs of integer coordinates values to the current position 
 * in the the block after making sure that min values are smaller than
 * max values.  Use this function to write MBR coordinates for an object.
 *
 * This function also updates the block's MBR and center if requested.
 *
 * Returns 0 if succesful or -1 if an error happened, in which case 
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPObjectBlock::WriteIntMBRCoord(GInt32 nXMin, GInt32 nYMin,
                                            GInt32 nXMax, GInt32 nYMax,
                                            GBool bUpdateMBR /*=TRUE*/)
{
    if (WriteIntCoord(MIN(nXMin, nXMax), MIN(nYMin, nYMax), bUpdateMBR) != 0 ||
        WriteIntCoord(MAX(nXMin, nXMax), MAX(nYMin, nYMax), bUpdateMBR) != 0 )
    {
        return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABMAPObjectBlock::AddCoordBlockRef()
 *
 * Update the first/last coord block fields in this object to contain
 * the specified block address.
 **********************************************************************/
void     TABMAPObjectBlock::AddCoordBlockRef(GInt32 nNewBlockAddress)
{
    /*-----------------------------------------------------------------
     * Normally, new blocks are added to the end of the list, except
     * the first one which is the beginning and the end of the list at 
     * the same time.
     *----------------------------------------------------------------*/
    if (m_nFirstCoordBlock == 0)
        m_nFirstCoordBlock = nNewBlockAddress;

    m_nLastCoordBlock = nNewBlockAddress;
}

/**********************************************************************
 *                   TABMAPObjectBlock::GetMBR()
 *
 * Return the MBR for the current block.
 **********************************************************************/
void TABMAPObjectBlock::GetMBR(GInt32 &nXMin, GInt32 &nYMin, 
                               GInt32 &nXMax, GInt32 &nYMax)
{
    nXMin = m_nMinX;
    nYMin = m_nMinY;
    nXMax = m_nMaxX;
    nYMax = m_nMaxY; 
}


/**********************************************************************
 *                   TABMAPObjectBlock::Dump()
 *
 * Dump block contents... available only in DEBUG mode.
 **********************************************************************/
#ifdef DEBUG

void TABMAPObjectBlock::Dump(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "----- TABMAPObjectBlock::Dump() -----\n");
    if (m_pabyBuf == NULL)
    {
        fprintf(fpOut, "Block has not been initialized yet.");
    }
    else
    {
        fprintf(fpOut,"Object Data Block (type %d) at offset %d.\n", 
                                                m_nBlockType, m_nFileOffset);
        fprintf(fpOut,"  m_numDataBytes        = %d\n", m_numDataBytes);
        fprintf(fpOut,"  m_nCenterX            = %d\n", m_nCenterX);
        fprintf(fpOut,"  m_nCenterY            = %d\n", m_nCenterY);
        fprintf(fpOut,"  m_nFirstCoordBlock    = %d\n", m_nFirstCoordBlock);
        fprintf(fpOut,"  m_nLastCoordBlock     = %d\n", m_nLastCoordBlock);
    }

    fflush(fpOut);
}

#endif // DEBUG
