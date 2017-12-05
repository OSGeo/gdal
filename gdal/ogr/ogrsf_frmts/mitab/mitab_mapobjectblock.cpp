/**********************************************************************
 *
 * Name:     mitab_mapobjectblock.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the TABMAPObjectBlock class used to handle
 *           reading/writing of the .MAP files' object data blocks
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
 **********************************************************************/

#include "cpl_port.h"
#include "mitab.h"

#include <algorithm>
#include <limits.h>
#include <stddef.h>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "mitab_priv.h"
#include "mitab_utils.h"

CPL_CVSID("$Id$")

/*=====================================================================
 *                      class TABMAPObjectBlock
 *====================================================================*/

static const int MAP_OBJECT_HEADER_SIZE = 20;

/**********************************************************************
 *                   TABMAPObjectBlock::TABMAPObjectBlock()
 *
 * Constructor.
 **********************************************************************/
TABMAPObjectBlock::TABMAPObjectBlock( TABAccess eAccessMode /*= TABRead*/ ) :
    TABRawBinBlock(eAccessMode, TRUE),
    m_numDataBytes(0),
    m_nFirstCoordBlock(0),
    m_nLastCoordBlock(0),
    m_nCenterX(0),
    m_nCenterY(0),
    m_nMinX(0),
    m_nMinY(0),
    m_nMaxX(0),
    m_nMaxY(0),
    m_nCurObjectOffset(0),
    m_nCurObjectId(0),
    m_nCurObjectType(TAB_GEOM_UNSET),
    m_bLockCenter(FALSE)
{}

/**********************************************************************
 *                   TABMAPObjectBlock::~TABMAPObjectBlock()
 *
 * Destructor.
 **********************************************************************/
TABMAPObjectBlock::~TABMAPObjectBlock()
{
    // TODO(schwehr): Why set these?  Should remove.
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
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPObjectBlock::InitBlockFromData(GByte *pabyBuf,
                                             int nBlockSize, int nSizeUsed,
                                             GBool bMakeCopy /* = TRUE */,
                                             VSILFILE *fpSrc /* = NULL */,
                                             int nOffset /* = 0 */)
{
    /*-----------------------------------------------------------------
     * First of all, we must call the base class' InitBlockFromData()
     *----------------------------------------------------------------*/
    const int nStatus =
        TABRawBinBlock::InitBlockFromData(pabyBuf,
                                          nBlockSize, nSizeUsed,
                                          bMakeCopy,
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
    if( m_numDataBytes < 0 || m_numDataBytes + MAP_OBJECT_HEADER_SIZE > nBlockSize )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "TABMAPObjectBlock::InitBlockFromData(): m_numDataBytes=%d incompatible with block size %d",
                 m_numDataBytes, nBlockSize);
        CPLFree(m_pabyBuf);
        m_pabyBuf = NULL;
        return -1;
    }

    m_nCenterX = ReadInt32();
    m_nCenterY = ReadInt32();

    m_nFirstCoordBlock = ReadInt32();
    m_nLastCoordBlock = ReadInt32();

    m_nCurObjectOffset = -1;
    m_nCurObjectId = -1;
    m_nCurObjectType = TAB_GEOM_UNSET;

    m_nMinX = 1000000000;
    m_nMinY = 1000000000;
    m_nMaxX = -1000000000;
    m_nMaxY = -1000000000;
    m_bLockCenter = FALSE;

    /*-----------------------------------------------------------------
     * Set real value for m_nSizeUsed to allow random update
     * (By default TABRawBinBlock thinks all bytes are used)
     *----------------------------------------------------------------*/
    m_nSizeUsed = m_numDataBytes + MAP_OBJECT_HEADER_SIZE;

    return 0;
}

/************************************************************************
 *                       ClearObjects()
 *
 * Cleans existing objects from the block. This method is used when
 * compacting a page that has deleted records.
 ************************************************************************/
void TABMAPObjectBlock::ClearObjects()
{
    GotoByteInBlock(MAP_OBJECT_HEADER_SIZE);
    WriteZeros(m_nBlockSize - MAP_OBJECT_HEADER_SIZE);
    GotoByteInBlock(MAP_OBJECT_HEADER_SIZE);
    m_nSizeUsed = MAP_OBJECT_HEADER_SIZE;
    m_bModified = TRUE;
}

/************************************************************************
 *                         LockCenter()
 *
 * Prevents the m_nCenterX and m_nCenterY to be adjusted by other methods.
 * Useful when editing pages that have compressed geometries.
 * This is a bit band-aid. Proper support of compressed geometries should
 * handle center moves.
 ************************************************************************/
void TABMAPObjectBlock::LockCenter()
{
    m_bLockCenter = TRUE;
}

/************************************************************************
 *                       SetCenterFromOtherBlock()
 *
 * Sets the m_nCenterX and m_nCenterY from the one of another block and
 * lock them. See LockCenter() as well.
 * Used when splitting a page.
 ************************************************************************/
void TABMAPObjectBlock::SetCenterFromOtherBlock(TABMAPObjectBlock* poOtherObjBlock)
{
    m_nCenterX = poOtherObjBlock->m_nCenterX;
    m_nCenterY = poOtherObjBlock->m_nCenterY;
    LockCenter();
}

/************************************************************************/
/*                        Rewind()                                      */
/************************************************************************/
void TABMAPObjectBlock::Rewind( )
{
    m_nCurObjectId = -1;
    m_nCurObjectOffset = -1;
    m_nCurObjectType = TAB_GEOM_UNSET;
}

/************************************************************************/
/*                        AdvanceToNextObject()                         */
/************************************************************************/

int TABMAPObjectBlock::AdvanceToNextObject( TABMAPHeaderBlock *poHeader )

{
    if( m_nCurObjectId == -1 )
    {
        m_nCurObjectOffset = 20;
    }
    else
    {
        m_nCurObjectOffset += poHeader->GetMapObjectSize( m_nCurObjectType );
    }

    if( m_nCurObjectOffset + 5 < m_numDataBytes + 20 )
    {
        GotoByteInBlock( m_nCurObjectOffset );
        m_nCurObjectType = (TABGeomType)ReadByte();
    }
    else
    {
        m_nCurObjectType = TAB_GEOM_UNSET;
    }

    if( m_nCurObjectType <= 0 || m_nCurObjectType >= TAB_GEOM_MAX_TYPE )
    {
        m_nCurObjectType = TAB_GEOM_UNSET;
        m_nCurObjectId = -1;
        m_nCurObjectOffset = -1;
    }
    else
    {
        m_nCurObjectId = ReadInt32();

        // Is this object marked as deleted?  If so, skip it.
        // I check both the top bits but I have only seen this occur
        // with the second highest bit set (i.e. in usa/states.tab). NFW.

        if( (((GUInt32)m_nCurObjectId) & (GUInt32) 0xC0000000) != 0 )
        {
            m_nCurObjectId = AdvanceToNextObject( poHeader );
        }
    }

    return m_nCurObjectId;
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
 * Returns 0 if successful or -1 if an error happened, in which case
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
     * Nothing to do here if block has not been modified
     *----------------------------------------------------------------*/
    if (!m_bModified)
        return 0;

    /*-----------------------------------------------------------------
     * Make sure 20 bytes block header is up to date.
     *----------------------------------------------------------------*/
    GotoByteInBlock(0x000);

    WriteInt16(TABMAP_OBJECT_BLOCK);    // Block type code
    m_numDataBytes = m_nSizeUsed - MAP_OBJECT_HEADER_SIZE;
    CPLAssert(m_numDataBytes >= 0 && m_numDataBytes < 32768);
    WriteInt16((GInt16)m_numDataBytes);         // num. bytes used

    WriteInt32(m_nCenterX);
    WriteInt32(m_nCenterY);

    WriteInt32(m_nFirstCoordBlock);
    WriteInt32(m_nLastCoordBlock);

    nStatus = CPLGetLastErrorNo();

    /*-----------------------------------------------------------------
     * OK, all object data has already been written in the block.
     * Call the base class to write the block to disk.
     *----------------------------------------------------------------*/
    if (nStatus == 0)
    {
#ifdef DEBUG_VERBOSE
        CPLDebug("MITAB", "Committing OBJECT block to offset %d", m_nFileOffset);
#endif
        nStatus = TABRawBinBlock::CommitToFile();
    }

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
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int TABMAPObjectBlock::InitNewBlock( VSILFILE *fpSrc, int nBlockSize,
                                     int nFileOffset /* = 0*/ )
{
    /*-----------------------------------------------------------------
     * Start with the default initialization
     *----------------------------------------------------------------*/
    if ( TABRawBinBlock::InitNewBlock(fpSrc, nBlockSize, nFileOffset) != 0)
        return -1;

    /*-----------------------------------------------------------------
     * And then set default values for the block header.
     *----------------------------------------------------------------*/
    // Set block MBR to extreme values to force an update on the first
    // UpdateMBR() call.
    m_nMinX = 1000000000;
    m_nMaxX = -1000000000;
    m_nMinY = 1000000000;
    m_nMaxY = -1000000000;

    // Reset current object refs
    m_nCurObjectId = -1;
    m_nCurObjectOffset = -1;
    m_nCurObjectType = TAB_GEOM_UNSET;

    m_numDataBytes = 0;       /* Data size excluding header */
    m_nCenterX = 0;
    m_nCenterY = 0;
    m_nFirstCoordBlock = 0;
    m_nLastCoordBlock = 0;

    if (m_eAccess != TABRead && nFileOffset != 0)
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
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPObjectBlock::ReadIntCoord(GBool bCompressed,
                                        GInt32 &nX, GInt32 &nY)
{
    if (bCompressed)
    {
        nX = ReadInt16();
        nY = ReadInt16();
        TABSaturatedAdd(nX, m_nCenterX);
        TABSaturatedAdd(nY, m_nCenterY);
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
 * the block.  If bCompr=TRUE then the coordinates are written relative to
 * the object block center... otherwise they're written as 32 bits int.
 *
 * This function does not maintain the block's MBR and center... it is
 * assumed to have been set before the first call to WriteIntCoord()
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPObjectBlock::WriteIntCoord(GInt32 nX, GInt32 nY,
                                         GBool bCompressed /*=FALSE*/)
{

    /*-----------------------------------------------------------------
     * Write coords to the file.
     *----------------------------------------------------------------*/
    if ((!bCompressed && (WriteInt32(nX) != 0 || WriteInt32(nY) != 0 ) ) ||
        (bCompressed && (WriteInt16((GInt16)(nX - m_nCenterX)) != 0 ||
                         WriteInt16((GInt16)(nY - m_nCenterY)) != 0) ) )
    {
        return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABMAPObjectBlock::WriteIntMBRCoord()
 *
 * Write 2 pairs of integer coordinates values to the current position
 * in the block after making sure that min values are smaller than
 * max values.  Use this function to write MBR coordinates for an object.
 *
 * If bCompr=TRUE then the coordinates are written relative to
 * the object block center... otherwise they're written as 32 bits int.
 *
 * This function does not maintain the block's MBR and center... it is
 * assumed to have been set before the first call to WriteIntCoord()
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPObjectBlock::WriteIntMBRCoord(GInt32 nXMin, GInt32 nYMin,
                                            GInt32 nXMax, GInt32 nYMax,
                                            GBool bCompressed /*=FALSE*/)
{
    if (WriteIntCoord(std::min(nXMin, nXMax), std::min(nYMin, nYMax),
                      bCompressed) != 0 ||
        WriteIntCoord(std::max(nXMin, nXMax), std::max(nYMin, nYMax),
                      bCompressed) != 0 )
    {
        return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABMAPObjectBlock::UpdateMBR()
 *
 * Update the block's MBR and center.
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPObjectBlock::UpdateMBR(GInt32 nX, GInt32 nY)
{

    if (nX < m_nMinX)
        m_nMinX = nX;
    if (nX > m_nMaxX)
        m_nMaxX = nX;

    if (nY < m_nMinY)
        m_nMinY = nY;
    if (nY > m_nMaxY)
        m_nMaxY = nY;

    if( !m_bLockCenter )
    {
        m_nCenterX = (m_nMinX + m_nMaxX) /2;
        m_nCenterY = (m_nMinY + m_nMaxY) /2;
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
    m_bModified = TRUE;
}

/**********************************************************************
 *                   TABMAPObjectBlock::SetMBR()
 *
 * Set the MBR for the current block.
 **********************************************************************/
void TABMAPObjectBlock::SetMBR(GInt32 nXMin, GInt32 nYMin,
                               GInt32 nXMax, GInt32 nYMax)
{
    m_nMinX = nXMin;
    m_nMinY = nYMin;
    m_nMaxX = nXMax;
    m_nMaxY = nYMax;

    if( !m_bLockCenter )
    {
        m_nCenterX = (m_nMinX + m_nMaxX) /2;
        m_nCenterY = (m_nMinY + m_nMaxY) /2;
    }
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
 *                   TABMAPObjectBlock::PrepareNewObject()
 *
 * Prepare this block to receive this new object. We only reserve space for
 * it in this call. Actual data will be written only when CommitNewObject()
 * is called.
 *
 * Returns the position at which the new object starts
 **********************************************************************/
int     TABMAPObjectBlock::PrepareNewObject(TABMAPObjHdr *poObjHdr)
{
    int nStartAddress = 0;

    // Nothing to do for NONE objects
    if (poObjHdr->m_nType == TAB_GEOM_NONE)
    {
        return 0;
    }

    // Maintain MBR of this object block.
    UpdateMBR(poObjHdr->m_nMinX, poObjHdr->m_nMinY);
    UpdateMBR(poObjHdr->m_nMaxX, poObjHdr->m_nMaxY);

    /*-----------------------------------------------------------------
     * Keep track of object type, ID and start address for use by
     * CommitNewObject()
     *----------------------------------------------------------------*/
    nStartAddress = GetFirstUnusedByteOffset();

    // Backup MBR and bLockCenter as they will be reset by GotoByteInFile()
    // that will call InitBlockFromData()
    GInt32 nXMin, nYMin, nXMax, nYMax;
    GetMBR(nXMin, nYMin, nXMax, nYMax);
    int bLockCenter = m_bLockCenter;
    GotoByteInFile(nStartAddress);
    m_bLockCenter = bLockCenter;
    SetMBR(nXMin, nYMin, nXMax, nYMax);
    m_nCurObjectOffset = nStartAddress - GetStartAddress();

    m_nCurObjectType = poObjHdr->m_nType;
    m_nCurObjectId   = poObjHdr->m_nId;

    return nStartAddress;
}

/**********************************************************************
 *                   TABMAPObjectBlock::CommitCurObjData()
 *
 * Write the ObjHdr to this block. This is usually called after
 * PrepareNewObject() once all members of the ObjHdr have
 * been set.
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPObjectBlock::CommitNewObject(TABMAPObjHdr *poObjHdr)
{
    int nStatus = 0;

    CPLAssert (poObjHdr->m_nType != TAB_GEOM_NONE);

    // Nothing to do for NONE objects
    if (poObjHdr->m_nType == TAB_GEOM_NONE)
    {
        return 0;
    }

    CPLAssert(m_nCurObjectId == poObjHdr->m_nId);
    GotoByteInBlock(m_nCurObjectOffset);

    nStatus = poObjHdr->WriteObj(this);

    if (nStatus == 0)
        m_numDataBytes = m_nSizeUsed - MAP_OBJECT_HEADER_SIZE;

    return nStatus;
}

/**********************************************************************
 *                   TABMAPObjectBlock::Dump()
 *
 * Dump block contents... available only in DEBUG mode.
 **********************************************************************/
#ifdef DEBUG

void TABMAPObjectBlock::Dump(FILE *fpOut, GBool bDetails)
{
    CPLErrorReset();

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

    if (bDetails)
    {
        /* We need the mapfile's header block */
        TABRawBinBlock *poBlock =
            TABCreateMAPBlockFromFile(m_fp, 0, m_nBlockSize);
        if (poBlock==NULL || poBlock->GetBlockClass() != TABMAP_HEADER_BLOCK)
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "Failed reading header block.");
            return;
        }
        TABMAPHeaderBlock *poHeader = (TABMAPHeaderBlock *)poBlock;

        Rewind();
        TABMAPObjHdr *poObjHdr = NULL;
        while((poObjHdr = TABMAPObjHdr::ReadNextObj(this, poHeader)) != NULL)
        {
            fprintf(fpOut,
                    "   object id=%d, type=%d, offset=%d (%d), size=%d\n"
                    "          MBR=(%d, %d, %d, %d)\n",
                    m_nCurObjectId, m_nCurObjectType, m_nCurObjectOffset,
                    m_nFileOffset + m_nCurObjectOffset,
                    poHeader->GetMapObjectSize( m_nCurObjectType ),
                    poObjHdr->m_nMinX, poObjHdr->m_nMinY,
                    poObjHdr->m_nMaxX,poObjHdr->m_nMaxY);
            delete poObjHdr;
        }

        delete poHeader;
    }

    fflush(fpOut);
}

#endif // DEBUG

/*=====================================================================
 *                      class TABMAPObjHdr and family
 *====================================================================*/

/**********************************************************************
 *                   class TABMAPObjHdr
 *
 * Virtual base class... contains static methods used to allocate instance
 * of the derived classes.
 *
 **********************************************************************/

/**********************************************************************
 *                    TABMAPObjHdr::NewObj()
 *
 * Alloc a new object of specified type or NULL for NONE types or if type
 * is not supported.
 **********************************************************************/
TABMAPObjHdr *TABMAPObjHdr::NewObj(TABGeomType nNewObjType, GInt32 nId /*=0*/)
{
    TABMAPObjHdr *poObj = NULL;

    switch(nNewObjType)
    {
      case TAB_GEOM_NONE:
        poObj = new TABMAPObjNone;
        break;
      case TAB_GEOM_SYMBOL_C:
      case TAB_GEOM_SYMBOL:
        poObj = new TABMAPObjPoint;
        break;
      case TAB_GEOM_FONTSYMBOL_C:
      case TAB_GEOM_FONTSYMBOL:
        poObj = new TABMAPObjFontPoint;
        break;
      case TAB_GEOM_CUSTOMSYMBOL_C:
      case TAB_GEOM_CUSTOMSYMBOL:
        poObj = new TABMAPObjCustomPoint;
        break;
      case TAB_GEOM_LINE_C:
      case TAB_GEOM_LINE:
        poObj = new TABMAPObjLine;
        break;
      case TAB_GEOM_PLINE_C:
      case TAB_GEOM_PLINE:
      case TAB_GEOM_REGION_C:
      case TAB_GEOM_REGION:
      case TAB_GEOM_MULTIPLINE_C:
      case TAB_GEOM_MULTIPLINE:
      case TAB_GEOM_V450_REGION_C:
      case TAB_GEOM_V450_REGION:
      case TAB_GEOM_V450_MULTIPLINE_C:
      case TAB_GEOM_V450_MULTIPLINE:
      case TAB_GEOM_V800_REGION_C:
      case TAB_GEOM_V800_REGION:
      case TAB_GEOM_V800_MULTIPLINE_C:
      case TAB_GEOM_V800_MULTIPLINE:
        poObj = new TABMAPObjPLine;
        break;
      case TAB_GEOM_ARC_C:
      case TAB_GEOM_ARC:
        poObj = new TABMAPObjArc;
        break;
      case TAB_GEOM_RECT_C:
      case TAB_GEOM_RECT:
      case TAB_GEOM_ROUNDRECT_C:
      case TAB_GEOM_ROUNDRECT:
      case TAB_GEOM_ELLIPSE_C:
      case TAB_GEOM_ELLIPSE:
        poObj = new TABMAPObjRectEllipse;
        break;
      case TAB_GEOM_TEXT_C:
      case TAB_GEOM_TEXT:
        poObj = new TABMAPObjText;
        break;
      case TAB_GEOM_MULTIPOINT_C:
      case TAB_GEOM_MULTIPOINT:
      case TAB_GEOM_V800_MULTIPOINT_C:
      case TAB_GEOM_V800_MULTIPOINT:
        poObj = new TABMAPObjMultiPoint;
        break;
      case TAB_GEOM_COLLECTION_C:
      case TAB_GEOM_COLLECTION:
      case TAB_GEOM_V800_COLLECTION_C:
      case TAB_GEOM_V800_COLLECTION:
        poObj = new TABMAPObjCollection();
    break;
      default:
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABMAPObjHdr::NewObj(): Unsupported object type %d",
                 nNewObjType);
    }

    if (poObj)
    {
        poObj->m_nType = nNewObjType;
        poObj->m_nId = nId;
        poObj->m_nMinX = poObj->m_nMinY = poObj->m_nMaxX = poObj->m_nMaxY = 0;
    }

    return poObj;
}

/**********************************************************************
 *                    TABMAPObjHdr::ReadNextObj()
 *
 * Read next object in this block and allocate/init a new object for it
 * if successful.
 * Returns NULL in case of error or if we reached end of block.
 **********************************************************************/
TABMAPObjHdr *TABMAPObjHdr::ReadNextObj(TABMAPObjectBlock *poObjBlock,
                                        TABMAPHeaderBlock *poHeader)
{
    TABMAPObjHdr *poObjHdr = NULL;

    if (poObjBlock->AdvanceToNextObject(poHeader) != -1)
    {
        poObjHdr=TABMAPObjHdr::NewObj(poObjBlock->GetCurObjectType());
        if (poObjHdr &&
            ((poObjHdr->m_nId = poObjBlock->GetCurObjectId()) == -1 ||
             poObjHdr->ReadObj(poObjBlock) != 0 ) )
        {
            // Failed reading object in block... an error was already produced
            delete poObjHdr;
            return NULL;
        }
    }

    return poObjHdr;
}

/**********************************************************************
 *                    TABMAPObjHdr::IsCompressedType()
 *
 * Returns TRUE if the current object type uses compressed coordinates
 * or FALSE otherwise.
 **********************************************************************/
GBool TABMAPObjHdr::IsCompressedType()
{
    // Compressed types are 1, 4, 7, etc.
    return (m_nType % 3) == 1 ? TRUE : FALSE;
}

/**********************************************************************
 *                   TABMAPObjHdr::WriteObjTypeAndId()
 *
 * Writetype+object id information... should be called only by the derived
 * classes' WriteObj() methods.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPObjHdr::WriteObjTypeAndId(TABMAPObjectBlock *poObjBlock)
{
    poObjBlock->WriteByte((GByte)m_nType);
    return poObjBlock->WriteInt32(m_nId);
}

/**********************************************************************
 *                   TABMAPObjHdr::SetMBR()
 *
 **********************************************************************/
void TABMAPObjHdr::SetMBR(GInt32 nMinX, GInt32 nMinY,
                          GInt32 nMaxX, GInt32 nMaxY)
{
    m_nMinX = std::min(nMinX, nMaxX);
    m_nMinY = std::min(nMinY, nMaxY);
    m_nMaxX = std::max(nMinX, nMaxX);
    m_nMaxY = std::max(nMinY, nMaxY);
}

/**********************************************************************
 *                   class TABMAPObjLine
 *
 * Applies to 2-points LINEs only
 **********************************************************************/

/**********************************************************************
 *                   TABMAPObjLine::ReadObj()
 *
 * Read Object information starting after the object id which should
 * have been read by TABMAPObjHdr::ReadNextObj() already.
 * This function should be called only by TABMAPObjHdr::ReadNextObj().
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPObjLine::ReadObj(TABMAPObjectBlock *poObjBlock)
{
    poObjBlock->ReadIntCoord(IsCompressedType(), m_nX1, m_nY1);
    poObjBlock->ReadIntCoord(IsCompressedType(), m_nX2, m_nY2);

    m_nPenId = poObjBlock->ReadByte();      // Pen index

    SetMBR(m_nX1, m_nY1, m_nX2, m_nY2);

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPObjLine::WriteObj()
 *
 * Write Object information with the type+object id
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPObjLine::WriteObj(TABMAPObjectBlock *poObjBlock)
{
    // Write object type and id
    TABMAPObjHdr::WriteObjTypeAndId(poObjBlock);

    poObjBlock->WriteIntCoord(m_nX1, m_nY1, IsCompressedType());
    poObjBlock->WriteIntCoord(m_nX2, m_nY2, IsCompressedType());

    poObjBlock->WriteByte(m_nPenId);      // Pen index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   class TABMAPObjPLine
 *
 * Applies to PLINE, MULTIPLINE and REGION object types
 **********************************************************************/

/**********************************************************************
 *                   TABMAPObjPLine::ReadObj()
 *
 * Read Object information starting after the object id which should
 * have been read by TABMAPObjHdr::ReadNextObj() already.
 * This function should be called only by TABMAPObjHdr::ReadNextObj().
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPObjPLine::ReadObj(TABMAPObjectBlock *poObjBlock)
{
    m_nCoordBlockPtr = poObjBlock->ReadInt32();
    m_nCoordDataSize = poObjBlock->ReadInt32();

    if (m_nCoordDataSize & 0x80000000)
    {
        m_bSmooth = TRUE;
        m_nCoordDataSize &= 0x7FFFFFFF; //Take smooth flag out of the value
    }
    else
    {
        m_bSmooth = FALSE;
    }

#ifdef TABDUMP
    printf("TABMAPObjPLine::ReadObj: m_nCoordDataSize = %d @ %d\n",/*ok*/
           m_nCoordDataSize, m_nCoordBlockPtr);
#endif

    // Number of line segments applies only to MULTIPLINE/REGION but not PLINE
    if (m_nType == TAB_GEOM_PLINE_C ||
        m_nType == TAB_GEOM_PLINE )
    {
        m_numLineSections = 1;
    }
    else if (m_nType == TAB_GEOM_V800_REGION ||
             m_nType == TAB_GEOM_V800_REGION_C ||
             m_nType == TAB_GEOM_V800_MULTIPLINE ||
             m_nType == TAB_GEOM_V800_MULTIPLINE_C )
    {
        /* V800 REGIONS/MULTIPLINES use an int32 */
        m_numLineSections = poObjBlock->ReadInt32();
        /* ... followed by 33 unknown bytes */
        poObjBlock->ReadInt32();
        poObjBlock->ReadInt32();
        poObjBlock->ReadInt32();
        poObjBlock->ReadInt32();
        poObjBlock->ReadInt32();
        poObjBlock->ReadInt32();
        poObjBlock->ReadInt32();
        poObjBlock->ReadInt32();
        poObjBlock->ReadByte();
    }
    else
    {
        /* V300 and V450 REGIONS/MULTIPLINES use an int16 */
        m_numLineSections = poObjBlock->ReadInt16();
    }

    if( m_numLineSections < 0 )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                    "Invalid numLineSections");
        return -1;
    }

#ifdef TABDUMP
    printf("PLINE/REGION: id=%d, type=%d, "/*ok*/
           "CoordBlockPtr=%d, CoordDataSize=%d, numLineSect=%d, bSmooth=%d\n",
           m_nId, m_nType, m_nCoordBlockPtr, m_nCoordDataSize,
           m_numLineSections, m_bSmooth);
#endif

    if (IsCompressedType())
    {
        // Region center/label point, relative to compr. coord. origin
        // No it's not relative to the Object block center
        m_nLabelX = poObjBlock->ReadInt16();
        m_nLabelY = poObjBlock->ReadInt16();

        // Compressed coordinate origin (present only in compressed case!)
        m_nComprOrgX = poObjBlock->ReadInt32();
        m_nComprOrgY = poObjBlock->ReadInt32();

        TABSaturatedAdd(m_nLabelX, m_nComprOrgX);
        TABSaturatedAdd(m_nLabelY, m_nComprOrgY);

        m_nMinX = poObjBlock->ReadInt16();  // Read MBR
        m_nMinY = poObjBlock->ReadInt16();
        m_nMaxX = poObjBlock->ReadInt16();
        m_nMaxY = poObjBlock->ReadInt16();
        TABSaturatedAdd(m_nMinX, m_nComprOrgX);
        TABSaturatedAdd(m_nMinY, m_nComprOrgY);
        TABSaturatedAdd(m_nMaxX, m_nComprOrgX);
        TABSaturatedAdd(m_nMaxY, m_nComprOrgY);
    }
    else
    {
        // Region center/label point, relative to compr. coord. origin
        // No it's not relative to the Object block center
        m_nLabelX = poObjBlock->ReadInt32();
        m_nLabelY = poObjBlock->ReadInt32();

        m_nMinX = poObjBlock->ReadInt32();    // Read MBR
        m_nMinY = poObjBlock->ReadInt32();
        m_nMaxX = poObjBlock->ReadInt32();
        m_nMaxY = poObjBlock->ReadInt32();
    }

    if ( ! IsCompressedType() )
    {
        // Init. Compr. Origin to a default value in case type is ever changed
        m_nComprOrgX = static_cast<GInt32>((static_cast<GIntBig>(m_nMinX) + m_nMaxX) / 2);
        m_nComprOrgY = static_cast<GInt32>((static_cast<GIntBig>(m_nMinY) + m_nMaxY) / 2);
    }

    m_nPenId = poObjBlock->ReadByte();      // Pen index

    if (m_nType == TAB_GEOM_REGION ||
        m_nType == TAB_GEOM_REGION_C ||
        m_nType == TAB_GEOM_V450_REGION ||
        m_nType == TAB_GEOM_V450_REGION_C ||
        m_nType == TAB_GEOM_V800_REGION ||
        m_nType == TAB_GEOM_V800_REGION_C )
    {
        m_nBrushId = poObjBlock->ReadByte();    // Brush index... REGION only
    }
    else
    {
        m_nBrushId = 0;
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPObjPLine::WriteObj()
 *
 * Write Object information with the type+object id
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPObjPLine::WriteObj(TABMAPObjectBlock *poObjBlock)
{
    // Write object type and id
    TABMAPObjHdr::WriteObjTypeAndId(poObjBlock);

    poObjBlock->WriteInt32(m_nCoordBlockPtr);

    // Combine smooth flag in the coord data size.
    if (m_bSmooth)
        poObjBlock->WriteInt32( m_nCoordDataSize | 0x80000000 );
    else
        poObjBlock->WriteInt32( m_nCoordDataSize );

    // Number of line segments applies only to MULTIPLINE/REGION but not PLINE
    if (m_nType == TAB_GEOM_V800_REGION ||
        m_nType == TAB_GEOM_V800_REGION_C ||
        m_nType == TAB_GEOM_V800_MULTIPLINE ||
        m_nType == TAB_GEOM_V800_MULTIPLINE_C )
    {
        /* V800 REGIONS/MULTIPLINES use an int32 */
        poObjBlock->WriteInt32(m_numLineSections);
        /* ... followed by 33 unknown bytes */
        poObjBlock->WriteZeros(33);
    }
    else if (m_nType != TAB_GEOM_PLINE_C &&
             m_nType != TAB_GEOM_PLINE )
    {
        /* V300 and V450 REGIONS/MULTIPLINES use an int16 */
        poObjBlock->WriteInt16((GInt16)m_numLineSections);
    }

    if (IsCompressedType())
    {
        // Region center/label point, relative to compr. coord. origin
        // No it's not relative to the Object block center
        poObjBlock->WriteInt16((GInt16)(m_nLabelX - m_nComprOrgX));
        poObjBlock->WriteInt16((GInt16)(m_nLabelY - m_nComprOrgY));

        // Compressed coordinate origin (present only in compressed case!)
        poObjBlock->WriteInt32(m_nComprOrgX);
        poObjBlock->WriteInt32(m_nComprOrgY);
    }
    else
    {
        // Region center/label point
        poObjBlock->WriteInt32(m_nLabelX);
        poObjBlock->WriteInt32(m_nLabelY);
    }

    // MBR
    if (IsCompressedType())
    {
        // MBR relative to PLINE origin (and not object block center)
        poObjBlock->WriteInt16((GInt16)(m_nMinX - m_nComprOrgX));
        poObjBlock->WriteInt16((GInt16)(m_nMinY - m_nComprOrgY));
        poObjBlock->WriteInt16((GInt16)(m_nMaxX - m_nComprOrgX));
        poObjBlock->WriteInt16((GInt16)(m_nMaxY - m_nComprOrgY));
    }
    else
    {
        poObjBlock->WriteInt32(m_nMinX);
        poObjBlock->WriteInt32(m_nMinY);
        poObjBlock->WriteInt32(m_nMaxX);
        poObjBlock->WriteInt32(m_nMaxY);
    }

    poObjBlock->WriteByte(m_nPenId);      // Pen index

    if (m_nType == TAB_GEOM_REGION ||
        m_nType == TAB_GEOM_REGION_C ||
        m_nType == TAB_GEOM_V450_REGION ||
        m_nType == TAB_GEOM_V450_REGION_C ||
        m_nType == TAB_GEOM_V800_REGION ||
        m_nType == TAB_GEOM_V800_REGION_C )
    {
        poObjBlock->WriteByte(m_nBrushId);    // Brush index... REGION only
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   class TABMAPObjPoint
 *
 **********************************************************************/

/**********************************************************************
 *                   TABMAPObjPoint::ReadObj()
 *
 * Read Object information starting after the object id
 **********************************************************************/
int TABMAPObjPoint::ReadObj(TABMAPObjectBlock *poObjBlock)
{
    poObjBlock->ReadIntCoord(IsCompressedType(), m_nX, m_nY);

    m_nSymbolId = poObjBlock->ReadByte();      // Symbol index

    SetMBR(m_nX, m_nY, m_nX, m_nY);

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPObjPoint::WriteObj()
 *
 * Write Object information with the type+object id
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPObjPoint::WriteObj(TABMAPObjectBlock *poObjBlock)
{
    // Write object type and id
    TABMAPObjHdr::WriteObjTypeAndId(poObjBlock);

    poObjBlock->WriteIntCoord(m_nX, m_nY, IsCompressedType());

    poObjBlock->WriteByte(m_nSymbolId);      // Symbol index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   class TABMAPObjFontPoint
 *
 **********************************************************************/

/**********************************************************************
 *                   TABMAPObjFontPoint::ReadObj()
 *
 * Read Object information starting after the object id
 **********************************************************************/
int TABMAPObjFontPoint::ReadObj(TABMAPObjectBlock *poObjBlock)
{
    m_nSymbolId  = poObjBlock->ReadByte();      // Symbol index
    m_nPointSize = poObjBlock->ReadByte();
    m_nFontStyle = poObjBlock->ReadInt16();     // font style

    m_nR = poObjBlock->ReadByte();
    m_nG = poObjBlock->ReadByte();
    m_nB = poObjBlock->ReadByte();

    poObjBlock->ReadByte();         // ??? BG Color ???
    poObjBlock->ReadByte();         // ???
    poObjBlock->ReadByte();         // ???

    m_nAngle = poObjBlock->ReadInt16();

    poObjBlock->ReadIntCoord(IsCompressedType(), m_nX, m_nY);

    m_nFontId  = poObjBlock->ReadByte();      // Font name index

    SetMBR(m_nX, m_nY, m_nX, m_nY);

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPObjFontPoint::WriteObj()
 *
 * Write Object information with the type+object id
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPObjFontPoint::WriteObj(TABMAPObjectBlock *poObjBlock)
{
    // Write object type and id
    TABMAPObjHdr::WriteObjTypeAndId(poObjBlock);

    poObjBlock->WriteByte(m_nSymbolId);   // symbol shape
    poObjBlock->WriteByte(m_nPointSize);
    poObjBlock->WriteInt16(m_nFontStyle);            // font style

    poObjBlock->WriteByte( m_nR );
    poObjBlock->WriteByte( m_nG );
    poObjBlock->WriteByte( m_nB );

    poObjBlock->WriteByte( 0 );
    poObjBlock->WriteByte( 0 );
    poObjBlock->WriteByte( 0 );

    poObjBlock->WriteInt16(m_nAngle);

    poObjBlock->WriteIntCoord(m_nX, m_nY, IsCompressedType());

    poObjBlock->WriteByte(m_nFontId);      // Font name index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   class TABMAPObjCustomPoint
 *
 **********************************************************************/

/**********************************************************************
 *                   TABMAPObjCustomPoint::ReadObj()
 *
 * Read Object information starting after the object id
 **********************************************************************/
int TABMAPObjCustomPoint::ReadObj(TABMAPObjectBlock *poObjBlock)
{
    m_nUnknown_ = poObjBlock->ReadByte();       // ???
    m_nCustomStyle = poObjBlock->ReadByte(); // 0x01=Show BG, 0x02=Apply Color

    poObjBlock->ReadIntCoord(IsCompressedType(), m_nX, m_nY);

    m_nSymbolId = poObjBlock->ReadByte();      // Symbol index
    m_nFontId   = poObjBlock->ReadByte();      // Font index

    SetMBR(m_nX, m_nY, m_nX, m_nY);

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPObjCustomPoint::WriteObj()
 *
 * Write Object information with the type+object id
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPObjCustomPoint::WriteObj(TABMAPObjectBlock *poObjBlock)
{
    // Write object type and id
    TABMAPObjHdr::WriteObjTypeAndId(poObjBlock);

    poObjBlock->WriteByte(m_nUnknown_);  // ???
    poObjBlock->WriteByte(m_nCustomStyle); // 0x01=Show BG, 0x02=Apply Color
    poObjBlock->WriteIntCoord(m_nX, m_nY, IsCompressedType());

    poObjBlock->WriteByte(m_nSymbolId);      // Symbol index
    poObjBlock->WriteByte(m_nFontId);      // Font index

  if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   class TABMAPObjRectEllipse
 *
 **********************************************************************/

/**********************************************************************
 *                   TABMAPObjRectEllipse::ReadObj()
 *
 * Read Object information starting after the object id
 **********************************************************************/
int TABMAPObjRectEllipse::ReadObj(TABMAPObjectBlock *poObjBlock)
{
    if (m_nType == TAB_GEOM_ROUNDRECT ||
        m_nType == TAB_GEOM_ROUNDRECT_C)
    {
        if (IsCompressedType())
        {
            m_nCornerWidth  = poObjBlock->ReadInt16();
            m_nCornerHeight = poObjBlock->ReadInt16();
        }
        else
        {
            m_nCornerWidth  = poObjBlock->ReadInt32();
            m_nCornerHeight = poObjBlock->ReadInt32();
        }
    }

    poObjBlock->ReadIntCoord(IsCompressedType(), m_nMinX, m_nMinY);
    poObjBlock->ReadIntCoord(IsCompressedType(), m_nMaxX, m_nMaxY);

    m_nPenId    = poObjBlock->ReadByte();      // Pen index
    m_nBrushId  = poObjBlock->ReadByte();      // Brush index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPObjRectEllipse::WriteObj()
 *
 * Write Object information with the type+object id
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPObjRectEllipse::WriteObj(TABMAPObjectBlock *poObjBlock)
{
    // Write object type and id
    TABMAPObjHdr::WriteObjTypeAndId(poObjBlock);

    if (m_nType == TAB_GEOM_ROUNDRECT ||
        m_nType == TAB_GEOM_ROUNDRECT_C)
    {
        if (IsCompressedType())
        {
            poObjBlock->WriteInt16((GInt16)m_nCornerWidth);
            poObjBlock->WriteInt16((GInt16)m_nCornerHeight);
        }
        else
        {
            poObjBlock->WriteInt32(m_nCornerWidth);
            poObjBlock->WriteInt32(m_nCornerHeight);
        }
    }

    poObjBlock->WriteIntMBRCoord(m_nMinX, m_nMinY, m_nMaxX, m_nMaxY,
                                 IsCompressedType());

    poObjBlock->WriteByte(m_nPenId);      // Pen index
    poObjBlock->WriteByte(m_nBrushId);      // Brush index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   class TABMAPObjArc
 *
 **********************************************************************/

/**********************************************************************
 *                   TABMAPObjArc::ReadObj()
 *
 * Read Object information starting after the object id
 **********************************************************************/
int TABMAPObjArc::ReadObj(TABMAPObjectBlock *poObjBlock)
{
    m_nStartAngle = poObjBlock->ReadInt16();
    m_nEndAngle   = poObjBlock->ReadInt16();

    // An arc is defined by its defining ellipse's MBR:
    poObjBlock->ReadIntCoord(IsCompressedType(),
                             m_nArcEllipseMinX, m_nArcEllipseMinY);
    poObjBlock->ReadIntCoord(IsCompressedType(),
                             m_nArcEllipseMaxX, m_nArcEllipseMaxY);

    // Read the Arc's actual MBR
    poObjBlock->ReadIntCoord(IsCompressedType(), m_nMinX, m_nMinY);
    poObjBlock->ReadIntCoord(IsCompressedType(), m_nMaxX, m_nMaxY);

    m_nPenId = poObjBlock->ReadByte();      // Pen index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPObjArc::WriteObj()
 *
 * Write Object information with the type+object id
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPObjArc::WriteObj(TABMAPObjectBlock *poObjBlock)
{
    // Write object type and id
    TABMAPObjHdr::WriteObjTypeAndId(poObjBlock);

    poObjBlock->WriteInt16((GInt16)m_nStartAngle);
    poObjBlock->WriteInt16((GInt16)m_nEndAngle);

    // An arc is defined by its defining ellipse's MBR:
    poObjBlock->WriteIntMBRCoord(m_nArcEllipseMinX, m_nArcEllipseMinY,
                                 m_nArcEllipseMaxX, m_nArcEllipseMaxY,
                                 IsCompressedType());

    // Write the Arc's actual MBR
    poObjBlock->WriteIntMBRCoord(m_nMinX, m_nMinY, m_nMaxX, m_nMaxY,
                                 IsCompressedType());

    poObjBlock->WriteByte(m_nPenId);      // Pen index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   class TABMAPObjText
 *
 **********************************************************************/

/**********************************************************************
 *                   TABMAPObjText::ReadObj()
 *
 * Read Object information starting after the object id
 **********************************************************************/
int TABMAPObjText::ReadObj(TABMAPObjectBlock *poObjBlock)
{
    m_nCoordBlockPtr  = poObjBlock->ReadInt32();    // String position
    m_nCoordDataSize  = poObjBlock->ReadInt16();    // String length
    if( m_nCoordDataSize < 0 )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed, "m_nCoordDataSize < 0");
        return -1;
    }
    m_nTextAlignment  = poObjBlock->ReadInt16();    // just./spacing/arrow

    m_nAngle     = poObjBlock->ReadInt16();         // Tenths of degree

    m_nFontStyle = poObjBlock->ReadInt16();         // Font style/effect

    m_nFGColorR  = poObjBlock->ReadByte();
    m_nFGColorG  = poObjBlock->ReadByte();
    m_nFGColorB  = poObjBlock->ReadByte();

    m_nBGColorR  = poObjBlock->ReadByte();
    m_nBGColorG  = poObjBlock->ReadByte();
    m_nBGColorB  = poObjBlock->ReadByte();

    // Label line end point
    poObjBlock->ReadIntCoord(IsCompressedType(), m_nLineEndX, m_nLineEndY);

    // Text Height
    if (IsCompressedType())
        m_nHeight = poObjBlock->ReadInt16();
    else
        m_nHeight = poObjBlock->ReadInt32();

    // Font name
    m_nFontId = poObjBlock->ReadByte();      // Font name index

    // MBR after rotation
    poObjBlock->ReadIntCoord(IsCompressedType(), m_nMinX, m_nMinY);
    poObjBlock->ReadIntCoord(IsCompressedType(), m_nMaxX, m_nMaxY);

    m_nPenId = poObjBlock->ReadByte();      // Pen index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPObjText::WriteObj()
 *
 * Write Object information with the type+object id
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPObjText::WriteObj(TABMAPObjectBlock *poObjBlock)
{
    // Write object type and id
    TABMAPObjHdr::WriteObjTypeAndId(poObjBlock);

    poObjBlock->WriteInt32(m_nCoordBlockPtr);     // String position
    poObjBlock->WriteInt16((GInt16)m_nCoordDataSize);     // String length
    poObjBlock->WriteInt16((GInt16)m_nTextAlignment);     // just./spacing/arrow

    poObjBlock->WriteInt16((GInt16)m_nAngle);             // Tenths of degree

    poObjBlock->WriteInt16(m_nFontStyle);         // Font style/effect

    poObjBlock->WriteByte(m_nFGColorR );
    poObjBlock->WriteByte(m_nFGColorG );
    poObjBlock->WriteByte(m_nFGColorB );

    poObjBlock->WriteByte(m_nBGColorR );
    poObjBlock->WriteByte(m_nBGColorG );
    poObjBlock->WriteByte(m_nBGColorB );

    // Label line end point
    poObjBlock->WriteIntCoord(m_nLineEndX, m_nLineEndY, IsCompressedType());

    // Text Height
    if (IsCompressedType())
        poObjBlock->WriteInt16((GInt16)m_nHeight);
    else
        poObjBlock->WriteInt32(m_nHeight);

    // Font name
    poObjBlock->WriteByte(m_nFontId);      // Font name index

    // MBR after rotation
    poObjBlock->WriteIntMBRCoord(m_nMinX, m_nMinY, m_nMaxX, m_nMaxY,
                                 IsCompressedType());

    poObjBlock->WriteByte(m_nPenId);      // Pen index

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   class TABMAPObjMultiPoint
 *
 * Applies to PLINE, MULTIPLINE and REGION object types
 **********************************************************************/

/**********************************************************************
 *                   TABMAPObjMultiPoint::ReadObj()
 *
 * Read Object information starting after the object id which should
 * have been read by TABMAPObjHdr::ReadNextObj() already.
 * This function should be called only by TABMAPObjHdr::ReadNextObj().
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPObjMultiPoint::ReadObj(TABMAPObjectBlock *poObjBlock)
{
    m_nCoordBlockPtr = poObjBlock->ReadInt32();
    m_nNumPoints = poObjBlock->ReadInt32();

    const int nPointSize = (IsCompressedType()) ? 2 * 2 : 2 * 4;
    if( m_nNumPoints < 0 || m_nNumPoints > INT_MAX / nPointSize )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Invalid m_nNumPoints = %d", m_nNumPoints);
        return -1;
    }
    m_nCoordDataSize = m_nNumPoints * nPointSize;

#ifdef TABDUMP
    printf("MULTIPOINT: id=%d, type=%d, "/*ok*/
           "CoordBlockPtr=%d, CoordDataSize=%d, numPoints=%d\n",
           m_nId, m_nType, m_nCoordBlockPtr, m_nCoordDataSize, m_nNumPoints);
#endif

    // ?????
    poObjBlock->ReadInt32();
    poObjBlock->ReadInt32();
    poObjBlock->ReadInt32();
    poObjBlock->ReadByte();
    poObjBlock->ReadByte();
    poObjBlock->ReadByte();

    if (m_nType == TAB_GEOM_V800_MULTIPOINT ||
        m_nType == TAB_GEOM_V800_MULTIPOINT_C )
    {
        /* V800 MULTIPOINTS have another 33 unknown bytes... all zeros */
        poObjBlock->ReadInt32();
        poObjBlock->ReadInt32();
        poObjBlock->ReadInt32();
        poObjBlock->ReadInt32();
        poObjBlock->ReadInt32();
        poObjBlock->ReadInt32();
        poObjBlock->ReadInt32();
        poObjBlock->ReadInt32();
        poObjBlock->ReadByte();
    }

    m_nSymbolId = poObjBlock->ReadByte();

    // ?????
    poObjBlock->ReadByte();

    if (IsCompressedType())
    {
        // Region center/label point, relative to compr. coord. origin
        // No it's not relative to the Object block center
        m_nLabelX = poObjBlock->ReadInt16();
        m_nLabelY = poObjBlock->ReadInt16();

        // Compressed coordinate origin
        m_nComprOrgX = poObjBlock->ReadInt32();
        m_nComprOrgY = poObjBlock->ReadInt32();

        TABSaturatedAdd(m_nLabelX, m_nComprOrgX);
        TABSaturatedAdd(m_nLabelY, m_nComprOrgY);

        m_nMinX = poObjBlock->ReadInt16();  // Read MBR
        m_nMinY = poObjBlock->ReadInt16();
        m_nMaxX = poObjBlock->ReadInt16();
        m_nMaxY = poObjBlock->ReadInt16();
        TABSaturatedAdd(m_nMinX, m_nComprOrgX);
        TABSaturatedAdd(m_nMinY, m_nComprOrgY);
        TABSaturatedAdd(m_nMaxX, m_nComprOrgX);
        TABSaturatedAdd(m_nMaxY, m_nComprOrgY);
    }
    else
    {
        // Region center/label point
        m_nLabelX = poObjBlock->ReadInt32();
        m_nLabelY = poObjBlock->ReadInt32();

        m_nMinX = poObjBlock->ReadInt32();    // Read MBR
        m_nMinY = poObjBlock->ReadInt32();
        m_nMaxX = poObjBlock->ReadInt32();
        m_nMaxY = poObjBlock->ReadInt32();

        // Init. Compr. Origin to a default value in case type is ever changed
        m_nComprOrgX = static_cast<GInt32>((static_cast<GIntBig>(m_nMinX) + m_nMaxX) / 2);
        m_nComprOrgY = static_cast<GInt32>((static_cast<GIntBig>(m_nMinY) + m_nMaxY) / 2);
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPObjMultiPoint::WriteObj()
 *
 * Write Object information with the type+object id
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPObjMultiPoint::WriteObj(TABMAPObjectBlock *poObjBlock)
{
    // Write object type and id
    TABMAPObjHdr::WriteObjTypeAndId(poObjBlock);

    poObjBlock->WriteInt32(m_nCoordBlockPtr);

    // Number of points
    poObjBlock->WriteInt32(m_nNumPoints);

    //  unknown bytes
    poObjBlock->WriteZeros(15);

    if (m_nType == TAB_GEOM_V800_MULTIPOINT ||
        m_nType == TAB_GEOM_V800_MULTIPOINT_C )
    {
        /* V800 MULTIPOINTS have another 33 unknown bytes... all zeros */
        poObjBlock->WriteZeros(33);
    }

    // Symbol Id
    poObjBlock->WriteByte(m_nSymbolId);

    // ????
    poObjBlock->WriteByte(0);

    // MBR
    if (IsCompressedType())
    {
        // Region center/label point, relative to compr. coord. origin
        // No it's not relative to the Object block center
        poObjBlock->WriteInt16((GInt16)(m_nLabelX - m_nComprOrgX));
        poObjBlock->WriteInt16((GInt16)(m_nLabelY - m_nComprOrgY));

        poObjBlock->WriteInt32(m_nComprOrgX);
        poObjBlock->WriteInt32(m_nComprOrgY);

        // MBR relative to object origin (and not object block center)
        poObjBlock->WriteInt16((GInt16)(m_nMinX - m_nComprOrgX));
        poObjBlock->WriteInt16((GInt16)(m_nMinY - m_nComprOrgY));
        poObjBlock->WriteInt16((GInt16)(m_nMaxX - m_nComprOrgX));
        poObjBlock->WriteInt16((GInt16)(m_nMaxY - m_nComprOrgY));
    }
    else
    {
        // Region center/label point
        poObjBlock->WriteInt32(m_nLabelX);
        poObjBlock->WriteInt32(m_nLabelY);

        poObjBlock->WriteInt32(m_nMinX);
        poObjBlock->WriteInt32(m_nMinY);
        poObjBlock->WriteInt32(m_nMaxX);
        poObjBlock->WriteInt32(m_nMaxY);
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   class TABMAPObjCollection
 *
 **********************************************************************/

/**********************************************************************
 *                   TABMAPObjCollection::ReadObj()
 *
 * Read Object information starting after the object id which should
 * have been read by TABMAPObjHdr::ReadNextObj() already.
 * This function should be called only by TABMAPObjHdr::ReadNextObj().
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPObjCollection::ReadObj(TABMAPObjectBlock *poObjBlock)
{
    int SIZE_OF_REGION_PLINE_MINI_HDR = 24, SIZE_OF_MPOINT_MINI_HDR = 24;
    int nVersion = TAB_GEOM_GET_VERSION(m_nType);

    /* Figure the size of the mini-header that we find for each of the
     * 3 optional components (center x,y and mbr)
     */
    if (IsCompressedType())
    {
        /* 6 * int16 */
        SIZE_OF_REGION_PLINE_MINI_HDR = SIZE_OF_MPOINT_MINI_HDR = 12;
    }
    else
    {
        /* 6 * int32 */
        SIZE_OF_REGION_PLINE_MINI_HDR = SIZE_OF_MPOINT_MINI_HDR = 24;
    }

    if (nVersion >= 800)
    {
        /* extra 4 bytes for num_segments in Region/Pline mini-headers */
        SIZE_OF_REGION_PLINE_MINI_HDR += 4;
    }

    m_nCoordBlockPtr = poObjBlock->ReadInt32();    // pointer into coord block
    m_nNumMultiPoints = poObjBlock->ReadInt32();   // no. points in multi point
    m_nRegionDataSize = poObjBlock->ReadInt32();   // size of region data inc. section hdrs
    m_nPolylineDataSize = poObjBlock->ReadInt32(); // size of multipline data inc. section hdrs

    if( m_nRegionDataSize < 0 )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                    "Invalid m_nRegionDataSize");
        return -1;
    }

    if( m_nPolylineDataSize < 0 )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                    "Invalid m_nRegionDataSize");
        return -1;
    }

    if (nVersion < 800)
    {
        // Num Region/Pline section headers (int16 in V650)
        m_nNumRegSections = poObjBlock->ReadInt16();
        m_nNumPLineSections = poObjBlock->ReadInt16();
    }
    else
    {
        // Num Region/Pline section headers (int32 in V800)
        m_nNumRegSections = poObjBlock->ReadInt32();
        m_nNumPLineSections = poObjBlock->ReadInt32();
    }

    const int nPointSize = (IsCompressedType()) ? 2 * 2 : 2 * 4;
    if( m_nNumMultiPoints < 0 || m_nNumMultiPoints > INT_MAX / nPointSize )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                    "Invalid m_nNumMultiPoints");
        return -1;
    }

    m_nMPointDataSize = m_nNumMultiPoints * nPointSize;

    /* NB. MapInfo counts 2 extra bytes per Region and Pline section header
     * in the RegionDataSize and PolylineDataSize values but those 2 extra
     * bytes are not present in the section hdr (possibly due to an alignment
     * to a 4 byte boundary in memory in MapInfo?). The real data size in
     * the CoordBlock is actually 2 bytes shorter per section header than
     * what is written in RegionDataSize and PolylineDataSize values.
     *
     * We'll adjust the values in memory to be the corrected values.
     */
    if( m_nNumRegSections < 0 || m_nNumRegSections > INT_MAX / 2 ||
        m_nRegionDataSize < 2 * m_nNumRegSections )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                    "Invalid m_nNumRegSections / m_nRegionDataSize");
        return -1;
    }
    m_nRegionDataSize   = m_nRegionDataSize - (2 * m_nNumRegSections);

    if( m_nNumPLineSections < 0 || m_nNumPLineSections > INT_MAX / 2 ||
        m_nPolylineDataSize < 2 * m_nNumPLineSections )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                    "Invalid m_nNumPLineSections / m_nPolylineDataSize");
        return -1;
    }
    m_nPolylineDataSize = m_nPolylineDataSize - (2 * m_nNumPLineSections);

    /* Compute total coord block data size, required when splitting blocks */
    m_nCoordDataSize = 0;

    if(m_nNumRegSections > 0)
    {
        if( m_nRegionDataSize > INT_MAX - SIZE_OF_REGION_PLINE_MINI_HDR ||
            m_nCoordDataSize > INT_MAX - (SIZE_OF_REGION_PLINE_MINI_HDR + m_nRegionDataSize) )
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                        "Invalid m_nCoordDataSize / m_nRegionDataSize");
            return -1;
        }
        m_nCoordDataSize += SIZE_OF_REGION_PLINE_MINI_HDR + m_nRegionDataSize;
    }
    if(m_nNumPLineSections > 0)
    {
        if( m_nPolylineDataSize > INT_MAX - SIZE_OF_REGION_PLINE_MINI_HDR ||
            m_nCoordDataSize > INT_MAX - (SIZE_OF_REGION_PLINE_MINI_HDR + m_nPolylineDataSize) )
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                        "Invalid m_nCoordDataSize / m_nPolylineDataSize");
            return -1;
        }
        m_nCoordDataSize += SIZE_OF_REGION_PLINE_MINI_HDR + m_nPolylineDataSize;
    }
    if(m_nNumMultiPoints > 0)
    {
        if( m_nMPointDataSize > INT_MAX - SIZE_OF_MPOINT_MINI_HDR ||
            m_nCoordDataSize > INT_MAX - (SIZE_OF_MPOINT_MINI_HDR + m_nMPointDataSize) )
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                        "Invalid m_nCoordDataSize / m_nMPointDataSize");
            return -1;
        }
        m_nCoordDataSize += SIZE_OF_MPOINT_MINI_HDR + m_nMPointDataSize;
    }

#ifdef TABDUMP
    printf("COLLECTION: id=%d, type=%d (0x%x), "/*ok*/
           "CoordBlockPtr=%d, numRegionSections=%d (size=%d+%d), "
           "numPlineSections=%d (size=%d+%d), numPoints=%d (size=%d+%d)\n",
           m_nId, m_nType, m_nType, m_nCoordBlockPtr,
           m_nNumRegSections, m_nRegionDataSize, SIZE_OF_REGION_PLINE_MINI_HDR,
           m_nNumPLineSections, m_nPolylineDataSize, SIZE_OF_REGION_PLINE_MINI_HDR,
           m_nNumMultiPoints, m_nMPointDataSize, SIZE_OF_MPOINT_MINI_HDR);
#endif

    if (nVersion >= 800)
    {
        // Extra byte in V800 files... value always 4???
        int nValue = poObjBlock->ReadByte();
        if (nValue != 4)
        {
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "TABMAPObjCollection::ReadObj(): Byte 29 in Collection "
                     "object header not equal to 4 as expected. Value is %d. "
                     "Please report this error to the MITAB list so that "
                     "MITAB can be extended to support this case.",
                     nValue);
            // We don't return right away, the error should be caught at the
            // end of this function.
        }
    }

    // ??? All zeros ???
    poObjBlock->ReadInt32();
    poObjBlock->ReadInt32();
    poObjBlock->ReadInt32();
    poObjBlock->ReadByte();
    poObjBlock->ReadByte();
    poObjBlock->ReadByte();

    m_nMultiPointSymbolId = poObjBlock->ReadByte();

    poObjBlock->ReadByte();  // ???
    m_nRegionPenId = poObjBlock->ReadByte();
    m_nPolylinePenId = poObjBlock->ReadByte();
    m_nRegionBrushId = poObjBlock->ReadByte();

    if (IsCompressedType())
    {
#ifdef TABDUMP
    printf("COLLECTION: READING ComprOrg @ %d\n",/*ok*/
           poObjBlock->GetCurAddress());
#endif
        // Compressed coordinate origin
        m_nComprOrgX = poObjBlock->ReadInt32();
        m_nComprOrgY = poObjBlock->ReadInt32();

        m_nMinX = poObjBlock->ReadInt16();  // Read MBR
        m_nMinY = poObjBlock->ReadInt16();
        m_nMaxX = poObjBlock->ReadInt16();
        m_nMaxY = poObjBlock->ReadInt16();
        TABSaturatedAdd(m_nMinX, m_nComprOrgX);
        TABSaturatedAdd(m_nMinY, m_nComprOrgY);
        TABSaturatedAdd(m_nMaxX, m_nComprOrgX);
        TABSaturatedAdd(m_nMaxY, m_nComprOrgY);
#ifdef TABDUMP
    printf("COLLECTION: ComprOrgX,Y= (%d,%d)\n",/*ok*/
           m_nComprOrgX, m_nComprOrgY);
#endif
    }
    else
    {
        m_nMinX = poObjBlock->ReadInt32();    // Read MBR
        m_nMinY = poObjBlock->ReadInt32();
        m_nMaxX = poObjBlock->ReadInt32();
        m_nMaxY = poObjBlock->ReadInt32();

        // Init. Compr. Origin to a default value in case type is ever changed
        m_nComprOrgX = static_cast<GInt32>((static_cast<GIntBig>(m_nMinX) + m_nMaxX) / 2);
        m_nComprOrgY = static_cast<GInt32>((static_cast<GIntBig>(m_nMinY) + m_nMaxY) / 2);
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPObjCollection::WriteObj()
 *
 * Write Object information with the type+object id
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPObjCollection::WriteObj(TABMAPObjectBlock *poObjBlock)
{
    // Write object type and id
    TABMAPObjHdr::WriteObjTypeAndId(poObjBlock);

    int nVersion = TAB_GEOM_GET_VERSION(m_nType);

    /* NB. MapInfo counts 2 extra bytes per Region and Pline section header
     * in the RegionDataSize and PolylineDataSize values but those 2 extra
     * bytes are not present in the section hdr (possibly due to an alignment
     * to a 4 byte boundary in memory in MapInfo?). The real data size in
     * the CoordBlock is actually 2 bytes shorter per section header than
     * what is written in RegionDataSize and PolylineDataSize values.
     *
     * The values in memory are the corrected values so we need to add 2 bytes
     * per section header in the values that we write on disk to emulate
     * MapInfo's behavior.
     */
    GInt32 nRegionDataSizeMI = m_nRegionDataSize + (2*m_nNumRegSections);
    GInt32 nPolylineDataSizeMI = m_nPolylineDataSize+(2*m_nNumPLineSections);

    poObjBlock->WriteInt32(m_nCoordBlockPtr);    // pointer into coord block
    poObjBlock->WriteInt32(m_nNumMultiPoints);   // no. points in multi point
    poObjBlock->WriteInt32(nRegionDataSizeMI);   // size of region data inc. section hdrs
    poObjBlock->WriteInt32(nPolylineDataSizeMI); // size of Mpolyline data inc. section hdrs

    if (nVersion < 800)
    {
        // Num Region/Pline section headers (int16 in V650)
        poObjBlock->WriteInt16((GInt16)m_nNumRegSections);
        poObjBlock->WriteInt16((GInt16)m_nNumPLineSections);
    }
    else
    {
        // Num Region/Pline section headers (int32 in V800)
        poObjBlock->WriteInt32(m_nNumRegSections);
        poObjBlock->WriteInt32(m_nNumPLineSections);
    }

    if (nVersion >= 800)
    {
        // Extra byte in V800 files... value always 4???
        poObjBlock->WriteByte(4);
    }

    // Unknown data ?????
    poObjBlock->WriteInt32(0);
    poObjBlock->WriteInt32(0);
    poObjBlock->WriteInt32(0);
    poObjBlock->WriteByte(0);
    poObjBlock->WriteByte(0);
    poObjBlock->WriteByte(0);

    poObjBlock->WriteByte(m_nMultiPointSymbolId);

    poObjBlock->WriteByte(0);
    poObjBlock->WriteByte(m_nRegionPenId);
    poObjBlock->WriteByte(m_nPolylinePenId);
    poObjBlock->WriteByte(m_nRegionBrushId);

    if (IsCompressedType())
    {
#ifdef TABDUMP
    printf("COLLECTION: WRITING ComprOrgX,Y= (%d,%d) @ %d\n",/*ok*/
           m_nComprOrgX, m_nComprOrgY, poObjBlock->GetCurAddress());
#endif
        // Compressed coordinate origin
        poObjBlock->WriteInt32(m_nComprOrgX);
        poObjBlock->WriteInt32(m_nComprOrgY);

        poObjBlock->WriteInt16((GInt16)(m_nMinX - m_nComprOrgX));  // MBR
        poObjBlock->WriteInt16((GInt16)(m_nMinY - m_nComprOrgY));
        poObjBlock->WriteInt16((GInt16)(m_nMaxX - m_nComprOrgX));
        poObjBlock->WriteInt16((GInt16)(m_nMaxY - m_nComprOrgY));
    }
    else
    {
        poObjBlock->WriteInt32(m_nMinX);    // MBR
        poObjBlock->WriteInt32(m_nMinY);
        poObjBlock->WriteInt32(m_nMaxX);
        poObjBlock->WriteInt32(m_nMaxY);
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}
