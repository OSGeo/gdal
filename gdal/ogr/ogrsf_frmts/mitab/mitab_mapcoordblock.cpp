/**********************************************************************
 * $Id: mitab_mapcoordblock.cpp,v 1.18 2010-07-07 19:00:15 aboudreault Exp $
 *
 * Name:     mitab_mapcoordblock.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the TABMAPCoordBlock class used to handle
 *           reading/writing of the .MAP files' coordinate blocks
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2001, Daniel Morissette
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
 **********************************************************************
 *
 * $Log: mitab_mapcoordblock.cpp,v $
 * Revision 1.18  2010-07-07 19:00:15  aboudreault
 * Cleanup Win32 Compile Warnings (GDAL bug #2930)
 *
 * Revision 1.17  2008-02-01 19:36:31  dmorissette
 * Initial support for V800 REGION and MULTIPLINE (bug 1496)
 *
 * Revision 1.16  2007/02/23 18:56:44  dmorissette
 * Fixed another problem writing collections when the header of objects
 * part of a collection were split on multiple blocks. Fix WriteBytes()
 * to reload next coord block in TABReadWrite mode if there is one (bug 1663)
 *
 * Revision 1.15  2006/11/28 18:49:08  dmorissette
 * Completed changes to split TABMAPObjectBlocks properly and produce an
 * optimal spatial index (bug 1585)
 *
 * Revision 1.14  2005/10/06 19:15:31  dmorissette
 * Collections: added support for reading/writing pen/brush/symbol ids and
 * for writing collection objects to .TAB/.MAP (bug 1126)
 *
 * Revision 1.13  2004/06/30 20:29:04  dmorissette
 * Fixed refs to old address danmo@videotron.ca
 *
 * Revision 1.12  2002/08/27 17:18:23  warmerda
 * improved CPL error testing
 *
 * Revision 1.11  2001/11/17 21:54:06  daniel
 * Made several changes in order to support writing objects in 16 bits coordinate format.
 * New TABMAPObjHdr-derived classes are used to hold object info in mem until block is full.
 *
 * Revision 1.10  2001/05/09 17:45:12  daniel
 * Support reading and writing data blocks > 512 bytes (for text objects).
 *
 * Revision 1.9  2000/10/10 19:05:12  daniel
 * Fixed ReadBytes() to allow strings overlapping on 2 blocks
 *
 * Revision 1.8  2000/02/28 16:58:55  daniel
 * Added V450 object types with num_points > 32767 and pen width in points
 *
 * Revision 1.7  2000/01/15 22:30:44  daniel
 * Switch to MIT/X-Consortium OpenSource license
 *
 * Revision 1.6  1999/11/08 04:29:31  daniel
 * Fixed problem with compressed coord. offset for regions and multiplines
 *
 * Revision 1.5  1999/10/06 15:19:11  daniel
 * Do not automatically init. curr. feature MBR when block is initialized
 *
 * Revision 1.4  1999/10/06 13:18:55  daniel
 * Fixed uninitialized class members
 *
 * Revision 1.3  1999/09/29 04:25:42  daniel
 * Fixed typo in GetFeatureMBR()
 *
 * Revision 1.2  1999/09/26 14:59:36  daniel
 * Implemented write support
 *
 * Revision 1.1  1999/07/12 04:18:24  daniel
 * Initial checkin
 *
 **********************************************************************/

#include "mitab.h"
#include "mitab_utils.h"

/*=====================================================================
 *                      class TABMAPCoordBlock
 *====================================================================*/

#define MAP_COORD_HEADER_SIZE   8

/**********************************************************************
 *                   TABMAPCoordBlock::TABMAPCoordBlock()
 *
 * Constructor.
 **********************************************************************/
TABMAPCoordBlock::TABMAPCoordBlock(TABAccess eAccessMode /*= TABRead*/):
    TABRawBinBlock(eAccessMode, TRUE)
{
    m_nComprOrgX = m_nComprOrgY = m_nNextCoordBlock = m_numDataBytes = 0;

    m_numBlocksInChain = 1;  // Current block counts as 1

    m_poBlockManagerRef = NULL;

    m_nTotalDataSize = 0;
    m_nFeatureDataSize = 0;

    m_nFeatureXMin = m_nMinX = 1000000000;
    m_nFeatureYMin = m_nMinY = 1000000000;
    m_nFeatureXMax = m_nMaxX = -1000000000;
    m_nFeatureYMax = m_nMaxY = -1000000000;

}

/**********************************************************************
 *                   TABMAPCoordBlock::~TABMAPCoordBlock()
 *
 * Destructor.
 **********************************************************************/
TABMAPCoordBlock::~TABMAPCoordBlock()
{

}


/**********************************************************************
 *                   TABMAPCoordBlock::InitBlockFromData()
 *
 * Perform some initialization on the block after its binary data has
 * been set or changed (or loaded from a file).
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPCoordBlock::InitBlockFromData(GByte *pabyBuf,
                                            int nBlockSize, int nSizeUsed,
                                            GBool bMakeCopy /* = TRUE */,
                                            VSILFILE *fpSrc /* = NULL */,
                                            int nOffset /* = 0 */)
{
    int nStatus;
#ifdef DEBUG_VERBOSE
    CPLDebug("MITAB", "Instantiating COORD block to/from offset %d", nOffset);
#endif
    /*-----------------------------------------------------------------
     * First of all, we must call the base class' InitBlockFromData()
     *----------------------------------------------------------------*/
    nStatus = TABRawBinBlock::InitBlockFromData(pabyBuf, nBlockSize, nSizeUsed,
                                                bMakeCopy, fpSrc, nOffset);
    if (nStatus != 0)
        return nStatus;

    /*-----------------------------------------------------------------
     * Validate block type
     *----------------------------------------------------------------*/
    if (m_nBlockType != TABMAP_COORD_BLOCK)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "InitBlockFromData(): Invalid Block Type: got %d expected %d",
                 m_nBlockType, TABMAP_COORD_BLOCK);
        CPLFree(m_pabyBuf);
        m_pabyBuf = NULL;
        return -1;
    }

    /*-----------------------------------------------------------------
     * Init member variables
     *----------------------------------------------------------------*/
    GotoByteInBlock(0x002);
    m_numDataBytes = ReadInt16();       /* Excluding 8 bytes header */
    if( m_numDataBytes < 0 || m_numDataBytes + MAP_COORD_HEADER_SIZE > nBlockSize )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "TABMAPCoordBlock::InitBlockFromData(): m_numDataBytes=%d incompatible with block size %d",
                 m_numDataBytes, nBlockSize);
        CPLFree(m_pabyBuf);
        m_pabyBuf = NULL;
        return -1;
    }

    m_nNextCoordBlock = ReadInt32();

    // Set the real SizeUsed based on numDataBytes
    m_nSizeUsed = m_numDataBytes + MAP_COORD_HEADER_SIZE;

    /*-----------------------------------------------------------------
     * The read ptr is now located at the beginning of the data part.
     *----------------------------------------------------------------*/
    GotoByteInBlock(MAP_COORD_HEADER_SIZE);

    return 0;
}

/**********************************************************************
 *                   TABMAPCoordBlock::CommitToFile()
 *
 * Commit the current state of the binary block to the file to which
 * it has been previously attached.
 *
 * This method makes sure all values are properly set in the map object
 * block header and then calls TABRawBinBlock::CommitToFile() to do
 * the actual writing to disk.
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPCoordBlock::CommitToFile()
{
    int nStatus = 0;

    CPLErrorReset();

    if ( m_pabyBuf == NULL )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "CommitToFile(): Block has not been initialized yet!");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Nothing to do here if block has not been modified
     *----------------------------------------------------------------*/
    if (!m_bModified)
        return 0;

    /*-----------------------------------------------------------------
     * Make sure 8 bytes block header is up to date.
     *----------------------------------------------------------------*/
    GotoByteInBlock(0x000);

    WriteInt16(TABMAP_COORD_BLOCK);    // Block type code
    CPLAssert(m_nSizeUsed >= MAP_COORD_HEADER_SIZE && m_nSizeUsed < MAP_COORD_HEADER_SIZE + 32768);
    WriteInt16((GInt16)(m_nSizeUsed - MAP_COORD_HEADER_SIZE)); // num. bytes used
    WriteInt32(m_nNextCoordBlock);

    if( CPLGetLastErrorType() == CE_Failure )
        nStatus = CPLGetLastErrorNo();

    /*-----------------------------------------------------------------
     * OK, call the base class to write the block to disk.
     *----------------------------------------------------------------*/
    if (nStatus == 0)
    {
#ifdef DEBUG_VERBOSE
        CPLDebug("MITAB", "Committing COORD block to offset %d", m_nFileOffset);
#endif
        nStatus = TABRawBinBlock::CommitToFile();
    }

    return nStatus;
}

/**********************************************************************
 *                   TABMAPCoordBlock::InitNewBlock()
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
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPCoordBlock::InitNewBlock(VSILFILE *fpSrc, int nBlockSize,
                                        int nFileOffset /* = 0*/)
{
    CPLErrorReset();
#ifdef DEBUG_VERBOSE
    CPLDebug( "MITAB", "Instantiating new COORD block at offset %d",
              nFileOffset);
#endif
    /*-----------------------------------------------------------------
     * Start with the default initialization
     *----------------------------------------------------------------*/
    if ( TABRawBinBlock::InitNewBlock(fpSrc, nBlockSize, nFileOffset) != 0)
        return -1;

    /*-----------------------------------------------------------------
     * And then set default values for the block header.
     *
     * IMPORTANT: Do not reset m_nComprOrg here because its value needs to be
     * maintained between blocks in the same chain.
     *----------------------------------------------------------------*/
    m_nNextCoordBlock = 0;

    m_numDataBytes = 0;

    // m_nMin/Max are used to keep track of current block MBR
    // FeatureMin/Max should not be reset here since feature coords can
    // be split on several blocks
    m_nMinX = 1000000000;
    m_nMinY = 1000000000;
    m_nMaxX = -1000000000;
    m_nMaxY = -1000000000;

    if (m_eAccess != TABRead && nFileOffset != 0)
    {
        GotoByteInBlock(0x000);

        WriteInt16(TABMAP_COORD_BLOCK); // Block type code
        WriteInt16(0);                  // num. bytes used, excluding header
        WriteInt32(0);                  // Pointer to next coord block
    }

    if (CPLGetLastErrorType() == CE_Failure )
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPObjectBlock::SetNextCoordBlock()
 *
 * Set the address (offset from beginning of file) of the coord. block
 * that follows the current one.
 **********************************************************************/
void     TABMAPCoordBlock::SetNextCoordBlock(GInt32 nNextCoordBlockAddress)
{
    m_nNextCoordBlock = nNextCoordBlockAddress;
    m_bModified = TRUE;
}


/**********************************************************************
 *                   TABMAPObjectBlock::SetComprCoordOrigin()
 *
 * Set the Compressed integer coordinates space origin to be used when
 * reading compressed coordinates using ReadIntCoord().
 **********************************************************************/
void     TABMAPCoordBlock::SetComprCoordOrigin(GInt32 nX, GInt32 nY)
{
    m_nComprOrgX = nX;
    m_nComprOrgY = nY;
}

/**********************************************************************
 *                   TABMAPObjectBlock::ReadIntCoord()
 *
 * Read the next pair of integer coordinates value from the block, and
 * apply the translation relative to the origin of the coord. space
 * previously set using SetComprCoordOrigin() if bCompressed=TRUE.
 *
 * This means that the returned coordinates are always absolute integer
 * coordinates, even when the source coords are in compressed form.
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPCoordBlock::ReadIntCoord(GBool bCompressed,
                                        GInt32 &nX, GInt32 &nY)
{
    if (bCompressed)
    {
        nX = ReadInt16();
        nY = ReadInt16();
        TABSaturatedAdd(nX, m_nComprOrgX);
        TABSaturatedAdd(nY, m_nComprOrgY);
    }
    else
    {
        nX = ReadInt32();
        nY = ReadInt32();
    }

    if (CPLGetLastErrorType() == CE_Failure)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPObjectBlock::ReadIntCoords()
 *
 * Read the specified number of pairs of X,Y integer coordinates values
 * from the block, and apply the translation relative to the origin of
 * the coord. space previously set using SetComprCoordOrigin() if
 * bCompressed=TRUE.
 *
 * This means that the returned coordinates are always absolute integer
 * coordinates, even when the source coords are in compressed form.
 *
 * panXY should point to an array big enough to receive the specified
 * number of coordinates.
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPCoordBlock::ReadIntCoords(GBool bCompressed, int numCoordPairs,
                                        GInt32 *panXY)
{
    int i, numValues = numCoordPairs*2;

    if (bCompressed)
    {
        for(i=0; i<numValues; i+=2)
        {
            panXY[i]   = ReadInt16();
            panXY[i+1] = ReadInt16();
            TABSaturatedAdd(panXY[i], m_nComprOrgX);
            TABSaturatedAdd(panXY[i+1], m_nComprOrgY);
            if (CPLGetLastErrorType() != 0)
                return -1;
        }
    }
    else
    {
        for(i=0; i<numValues; i+=2)
        {
            panXY[i]   = ReadInt32();
            panXY[i+1] = ReadInt32();
            if (CPLGetLastErrorType() != 0)
                return -1;
        }
    }

    return 0;
}

/**********************************************************************
 *                   TABMAPObjectBlock::ReadCoordSecHdrs()
 *
 * Read a set of coordinate section headers for PLINE MULTIPLE or REGIONs
 * and store the result in the array of structures pasHdrs[].  It is assumed
 * that pasHdrs points to an allocated array of at least numSections
 * TABMAPCoordSecHdr structures.
 *
 * The function will also set the values of numVerticesTotal to the
 * total number of coordinates in the object (the sum of all sections
 * headers read).
 *
 * At the end of the call, this TABMAPCoordBlock object will be located
 * at the beginning of the coordinate data.
 *
 * In V450 the numVertices is stored on an int32 instead of an int16
 *
 * In V800 the numHoles is stored on an int32 instead of an int16
 *
 * IMPORTANT: This function makes the assumption that coordinates for all
 *            the sections are grouped together immediately after the
 *            last section header block (i.e. that the coord. data is not
 *            located all over the place).  If it is not the case then
 *            an error will be produced and the code to read region and
 *            multipline objects will have to be updated.
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPCoordBlock::ReadCoordSecHdrs(GBool bCompressed,
                                           int nVersion,
                                           int numSections,
                                           TABMAPCoordSecHdr *pasHdrs,
                                           GInt32    &numVerticesTotal)
{
    int i, nTotalHdrSizeUncompressed;

    CPLErrorReset();

    /*-------------------------------------------------------------
     * Note about header+vertices size vs compressed coordinates:
     * The uncompressed header sections are actually 16 bytes, but the
     * offset calculations are based on prior decompression of the
     * coordinates.  Our coordinate offset calculations have
     * to take this fact into account.
     * Also, V450 header section uses int32 instead of int16 for numVertices
     * and we add another 2 bytes to align with a 4 bytes boundary.
     * V800 header section uses int32 for numHoles but there is no need
     * for the 2 alignment bytes so the size is the same as V450
     *------------------------------------------------------------*/
    const int nSectionSize = (nVersion >= 450) ? 28 : 24;
    if( numSections > INT_MAX / nSectionSize )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Invalid numSections");
        return -1;
    }
    nTotalHdrSizeUncompressed = nSectionSize * numSections;

    numVerticesTotal = 0;

    for(i=0; i<numSections; i++)
    {
        /*-------------------------------------------------------------
         * Read the coord. section header blocks
         *------------------------------------------------------------*/
#ifdef TABDUMP
        int nHdrAddress = GetCurAddress();
#endif
        if (nVersion >= 450)
            pasHdrs[i].numVertices = ReadInt32();
        else
            pasHdrs[i].numVertices = ReadInt16();
        if( pasHdrs[i].numVertices < 0 )
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "Invalid number of vertices for section %d", i);
            return -1;
        }
        if (nVersion >= 800)
            pasHdrs[i].numHoles = ReadInt32();
        else
            pasHdrs[i].numHoles = ReadInt16();
        if( pasHdrs[i].numHoles < 0 )
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "Invalid number of holes for section %d", i);
            return -1;
        }
        ReadIntCoord(bCompressed, pasHdrs[i].nXMin, pasHdrs[i].nYMin);
        ReadIntCoord(bCompressed, pasHdrs[i].nXMax, pasHdrs[i].nYMax);
        pasHdrs[i].nDataOffset = ReadInt32();
        if( pasHdrs[i].nDataOffset < nTotalHdrSizeUncompressed )
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "Invalid data offset for section %d", i);
            return -1;
        }

        if (CPLGetLastErrorType() != 0)
            return -1;

        if( numVerticesTotal > INT_MAX - pasHdrs[i].numVertices )
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "Invalid number of vertices for section %d", i);
            return -1;
        }
        numVerticesTotal += pasHdrs[i].numVertices;


        pasHdrs[i].nVertexOffset = (pasHdrs[i].nDataOffset -
                                    nTotalHdrSizeUncompressed ) / 8;
#ifdef TABDUMP
        printf("READING pasHdrs[%d] @ %d = \n"
               "              { numVertices = %d, numHoles = %d, \n"
               "                nXMin=%d, nYMin=%d, nXMax=%d, nYMax=%d,\n"
               "                nDataOffset=%d, nVertexOffset=%d }\n",
               i, nHdrAddress, pasHdrs[i].numVertices, pasHdrs[i].numHoles,
               pasHdrs[i].nXMin, pasHdrs[i].nYMin, pasHdrs[i].nXMax,
               pasHdrs[i].nYMax, pasHdrs[i].nDataOffset,
               pasHdrs[i].nVertexOffset);
        printf("                dX = %d, dY = %d  (center = %d , %d)\n",
               pasHdrs[i].nXMax - pasHdrs[i].nXMin,
               pasHdrs[i].nYMax - pasHdrs[i].nYMin,
               m_nComprOrgX, m_nComprOrgY);
#endif
    }

    for(i=0; i<numSections; i++)
    {
        /*-------------------------------------------------------------
         * Make sure all coordinates are grouped together
         * (Well... at least check that all the vertex indices are enclosed
         * inside the [0..numVerticesTotal] range.)
         *------------------------------------------------------------*/
        if ( pasHdrs[i].nVertexOffset < 0 ||
             pasHdrs[i].nVertexOffset > INT_MAX - pasHdrs[i].numVertices ||
             (pasHdrs[i].nVertexOffset +
                           pasHdrs[i].numVertices ) > numVerticesTotal)
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "Unsupported case or corrupt file: MULTIPLINE/REGION "
                     "object vertices do not appear to be grouped together.");
            return -1;
        }
    }

    return 0;
}

/**********************************************************************
 *                   TABMAPObjectBlock::WriteCoordSecHdrs()
 *
 * Write a set of coordinate section headers for PLINE MULTIPLE or REGIONs.
 * pasHdrs should point to an array of numSections TABMAPCoordSecHdr
 * structures that have been properly initialized.
 *
 * In V450 the numVertices is stored on an int32 instead of an int16
 *
 * In V800 the numHoles is stored on an int32 instead of an int16
 *
 * At the end of the call, this TABMAPCoordBlock object will be ready to
 * receive the coordinate data.
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPCoordBlock::WriteCoordSecHdrs(int nVersion,
                                            int numSections,
                                            TABMAPCoordSecHdr *pasHdrs,
                                            GBool bCompressed /*=FALSE*/)
{
    int i;

    CPLErrorReset();

    for(i=0; i<numSections; i++)
    {
        /*-------------------------------------------------------------
         * Write the coord. section header blocks
         *------------------------------------------------------------*/
#ifdef TABDUMP
        printf("WRITING pasHdrs[%d] @ %d = \n"
               "              { numVertices = %d, numHoles = %d, \n"
               "                nXMin=%d, nYMin=%d, nXMax=%d, nYMax=%d,\n"
               "                nDataOffset=%d, nVertexOffset=%d }\n",
               i, GetCurAddress(), pasHdrs[i].numVertices, pasHdrs[i].numHoles,
               pasHdrs[i].nXMin, pasHdrs[i].nYMin, pasHdrs[i].nXMax,
               pasHdrs[i].nYMax, pasHdrs[i].nDataOffset,
               pasHdrs[i].nVertexOffset);
        printf("                dX = %d, dY = %d  (center = %d , %d)\n",
               pasHdrs[i].nXMax - pasHdrs[i].nXMin,
               pasHdrs[i].nYMax - pasHdrs[i].nYMin,
               m_nComprOrgX, m_nComprOrgY);
#endif

        if (nVersion >= 450)
            WriteInt32(pasHdrs[i].numVertices);
        else
            WriteInt16((GInt16)pasHdrs[i].numVertices);
        if (nVersion >= 800)
            WriteInt32(pasHdrs[i].numHoles);
        else
            WriteInt16((GInt16)pasHdrs[i].numHoles);
        WriteIntCoord(pasHdrs[i].nXMin, pasHdrs[i].nYMin, bCompressed);
        WriteIntCoord(pasHdrs[i].nXMax, pasHdrs[i].nYMax, bCompressed);
        WriteInt32(pasHdrs[i].nDataOffset);

        if (CPLGetLastErrorType() == CE_Failure )
            return -1;
    }

    return 0;
}


/**********************************************************************
 *                   TABMAPCoordBlock::WriteIntCoord()
 *
 * Write a pair of integer coordinates values to the current position in the
 * the block.
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/

int     TABMAPCoordBlock::WriteIntCoord(GInt32 nX, GInt32 nY,
                                        GBool bCompressed /*=FALSE*/)
{

    if ((!bCompressed && (WriteInt32(nX) != 0 || WriteInt32(nY) != 0 ) ) ||
        (bCompressed && (WriteInt16((GInt16)(nX - m_nComprOrgX)) != 0 ||
                         WriteInt16((GInt16)(nY - m_nComprOrgY)) != 0) ) )
    {
        return -1;
    }

    /*-----------------------------------------------------------------
     * Update block MBR
     *----------------------------------------------------------------*/
    //__TODO__ Do we still need to track the block MBR???
    if (nX < m_nMinX)
        m_nMinX = nX;
    if (nX > m_nMaxX)
        m_nMaxX = nX;

    if (nY < m_nMinY)
        m_nMinY = nY;
    if (nY > m_nMaxY)
        m_nMaxY = nY;

    /*-------------------------------------------------------------
     * Also keep track of current feature MBR.
     *------------------------------------------------------------*/
    if (nX < m_nFeatureXMin)
        m_nFeatureXMin = nX;
    if (nX > m_nFeatureXMax)
        m_nFeatureXMax = nX;

    if (nY < m_nFeatureYMin)
        m_nFeatureYMin = nY;
    if (nY > m_nFeatureYMax)
        m_nFeatureYMax = nY;

    return 0;
}

/**********************************************************************
 *                   TABMAPCoordBlock::SetMAPBlockManagerRef()
 *
 * Pass a reference to the block manager object for the file this
 * block belongs to.  The block manager will be used by this object
 * when it needs to automatically allocate a new block.
 **********************************************************************/
void TABMAPCoordBlock::SetMAPBlockManagerRef(TABBinBlockManager *poBlockMgr)
{
    m_poBlockManagerRef = poBlockMgr;
};


/**********************************************************************
 *                   TABMAPCoordBlock::ReadBytes()
 *
 * Cover function for TABRawBinBlock::ReadBytes() that will automagically
 * load the next coordinate block in the chain before reading the
 * requested bytes if we are at the end of the current block and if
 * m_nNextCoordBlock is a valid block.
 *
 * Then the control is passed to TABRawBinBlock::ReadBytes() to finish the
 * work:
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
int     TABMAPCoordBlock::ReadBytes(int numBytes, GByte *pabyDstBuf)
{
    int nStatus;

    if (m_pabyBuf &&
        m_nCurPos >= (m_numDataBytes+MAP_COORD_HEADER_SIZE) &&
        m_nNextCoordBlock > 0)
    {
        // We're at end of current block... advance to next block.

        if ( (nStatus=GotoByteInFile(m_nNextCoordBlock, TRUE)) != 0)
        {
            // Failed.... an error has already been reported.
            return nStatus;
        }

        GotoByteInBlock(MAP_COORD_HEADER_SIZE); // Move pointer past header
        m_numBlocksInChain++;
    }

    if (m_pabyBuf &&
        m_nCurPos < (m_numDataBytes+MAP_COORD_HEADER_SIZE) &&
        m_nCurPos+numBytes > (m_numDataBytes+MAP_COORD_HEADER_SIZE) &&
        m_nNextCoordBlock > 0)
    {
        // Data overlaps on more than one block
        // Read until end of this block and then recursively call ReadBytes()
        // for the rest.
        int numBytesInThisBlock =
                      (m_numDataBytes+MAP_COORD_HEADER_SIZE)-m_nCurPos;
        nStatus = TABRawBinBlock::ReadBytes(numBytesInThisBlock, pabyDstBuf);
        if (nStatus == 0)
            nStatus = TABMAPCoordBlock::ReadBytes(numBytes-numBytesInThisBlock,
                                               pabyDstBuf+numBytesInThisBlock);
        return nStatus;
    }


    return TABRawBinBlock::ReadBytes(numBytes, pabyDstBuf);
}


/**********************************************************************
 *                   TABMAPCoordBlock::WriteBytes()
 *
 * Cover function for TABRawBinBlock::WriteBytes() that will automagically
 * CommitToFile() the current block and create a new one if we are at
 * the end of the current block.
 *
 * Then the control is passed to TABRawBinBlock::WriteBytes() to finish the
 * work.
 *
 * Passing pabySrcBuf = NULL will only move the write pointer by the
 * specified number of bytes as if the copy had happened... but it
 * won't crash.
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int  TABMAPCoordBlock::WriteBytes(int nBytesToWrite, const GByte *pabySrcBuf)
{
    if (m_eAccess != TABWrite && m_eAccess != TABReadWrite )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "WriteBytes(): Block does not support write operations.");
        return -1;
    }

    if (m_poBlockManagerRef && (m_nBlockSize - m_nCurPos) < nBytesToWrite)
    {
        if (nBytesToWrite <= (m_nBlockSize-MAP_COORD_HEADER_SIZE))
        {
            // Data won't fit in this block but can fit inside a single
            // block, so we'll allocate a new block for it.  This will
            // prevent us from overlapping coordinate values on 2 blocks, but
            // still allows strings longer than one block (see 'else' below).
            //

            if ( m_nNextCoordBlock != 0 )
            {
                // We're in read/write mode and there is already an allocated
                // block following this one in the chain ... just reload it
                // and continue writing to it

                CPLAssert( m_eAccess == TABReadWrite );

                if (CommitToFile() != 0 ||
                     ReadFromFile(m_fp, m_nNextCoordBlock, m_nBlockSize) != 0)
                {
                    // An error message should have already been reported.
                    return -1;
                }
            }
            else
            {
                // Need to alloc a new block.

                int nNewBlockOffset = m_poBlockManagerRef->AllocNewBlock("COORD");
                SetNextCoordBlock(nNewBlockOffset);

                if (CommitToFile() != 0 ||
                    InitNewBlock(m_fp, m_nBlockSize, nNewBlockOffset) != 0)
                {
                    // An error message should have already been reported.
                    return -1;
                }

                m_numBlocksInChain++;
            }
        }
        else
        {
            // Data to write is longer than one block... so we'll have to
            // split it over multiple block through multiple calls.
            //
            int nStatus = 0;
            while(nStatus == 0 && nBytesToWrite > 0)
            {
                int nBytes = m_nBlockSize-MAP_COORD_HEADER_SIZE;
                if ( (m_nBlockSize - m_nCurPos) > 0 )
                {
                    // Use free room in current block
                    nBytes = (m_nBlockSize - m_nCurPos);
                }

                nBytes = MIN(nBytes, nBytesToWrite);

                // The following call will result in a new block being
                // allocated in the if() block above.
                nStatus = TABMAPCoordBlock::WriteBytes(nBytes,
                                                       pabySrcBuf);

                nBytesToWrite -= nBytes;
                pabySrcBuf += nBytes;
            }
            return nStatus;
        }
    }

    if (m_nCurPos >= MAP_COORD_HEADER_SIZE)
    {
        // Keep track of Coordinate data... this means ignore header bytes
        // that could be written.
        m_nTotalDataSize += nBytesToWrite;
        m_nFeatureDataSize += nBytesToWrite;
    }

    return TABRawBinBlock::WriteBytes(nBytesToWrite, pabySrcBuf);
}

/**********************************************************************
 *                   TABMAPObjectBlock::SeekEnd()
 *
 * Move read/write pointer to end of used part of the block
 **********************************************************************/
void     TABMAPCoordBlock::SeekEnd()
{
    m_nCurPos = m_nSizeUsed;
}

/**********************************************************************
 *                   TABMAPCoordBlock::StartNewFeature()
 *
 * Reset all member vars that are used to keep track of data size
 * and MBR for the current feature.  This is info is not needed by
 * the coord blocks themselves, but it helps a lot the callers to
 * have this class take care of that for them.
 *
 * See Also: GetFeatureDataSize() and GetFeatureMBR()
 **********************************************************************/
void TABMAPCoordBlock::StartNewFeature()
{
    m_nFeatureDataSize = 0;

    m_nFeatureXMin = 1000000000;
    m_nFeatureYMin = 1000000000;
    m_nFeatureXMax = -1000000000;
    m_nFeatureYMax = -1000000000;
}

/**********************************************************************
 *                   TABMAPCoordBlock::GetFeatureMBR()
 *
 * Return the MBR of all the coords written using WriteIntCoord() since
 * the last call to StartNewFeature().
 **********************************************************************/
void TABMAPCoordBlock::GetFeatureMBR(GInt32 &nXMin, GInt32 &nYMin,
                                     GInt32 &nXMax, GInt32 &nYMax)
{
    nXMin = m_nFeatureXMin;
    nYMin = m_nFeatureYMin;
    nXMax = m_nFeatureXMax;
    nYMax = m_nFeatureYMax;
}


/**********************************************************************
 *                   TABMAPCoordBlock::Dump()
 *
 * Dump block contents... available only in DEBUG mode.
 **********************************************************************/
#ifdef DEBUG

void TABMAPCoordBlock::Dump(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "----- TABMAPCoordBlock::Dump() -----\n");
    if (m_pabyBuf == NULL)
    {
        fprintf(fpOut, "Block has not been initialized yet.");
    }
    else
    {
        fprintf(fpOut,"Coordinate Block (type %d) at offset %d.\n",
                                                 m_nBlockType, m_nFileOffset);
        fprintf(fpOut,"  m_numDataBytes        = %d\n", m_numDataBytes);
        fprintf(fpOut,"  m_nNextCoordBlock     = %d\n", m_nNextCoordBlock);
    }

    fflush(fpOut);
}

#endif // DEBUG
