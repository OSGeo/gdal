/**********************************************************************
 * $Id: mitab_mapobjectblock.cpp,v 1.15 2005/10/06 19:15:31 dmorissette Exp $
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
 * Revision 1.15  2005/10/06 19:15:31  dmorissette
 * Collections: added support for reading/writing pen/brush/symbol ids and
 * for writing collection objects to .TAB/.MAP (bug 1126)
 *
 * Revision 1.14  2005/10/04 15:44:31  dmorissette
 * First round of support for Collection objects. Currently supports reading
 * from .TAB/.MAP and writing to .MIF. Still lacks symbol support and write
 * support. (Based in part on patch and docs from Jim Hope, bug 1126)
 *
 * Revision 1.13  2004/06/30 20:29:04  dmorissette
 * Fixed refs to old address danmo@videotron.ca
 *
 * Revision 1.12  2002/03/26 01:48:40  daniel
 * Added Multipoint object type (V650)
 *
 * Revision 1.11  2001/12/05 22:40:27  daniel
 * Init MBR to 0 in TABMAPObjHdr and modif. SetMBR() to validate min/max
 *
 * Revision 1.10  2001/11/19 15:07:06  daniel
 * Handle the case of TAB_GEOM_NONE with the new TABMAPObjHdr classes.
 *
 * Revision 1.9  2001/11/17 21:54:06  daniel
 * Made several changes in order to support writing objects in 16 bits 
 * coordinate format. New TABMAPObjHdr-derived classes are used to hold 
 * object info in mem until block is full.
 *
 * Revision 1.8  2001/09/19 19:19:11  warmerda
 * modified AdvanceToNextObject() to skip deleted objects
 *
 * Revision 1.7  2001/09/14 03:23:55  warmerda
 * Substantial upgrade to support spatial queries using spatial indexes
 *
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
    m_papoObjHdr = NULL;
    m_numObjects = 0;
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

    FreeObjectArray();
}


/**********************************************************************
 *                   TABMAPObjectBlock::FreeObjectArray()
 **********************************************************************/
void TABMAPObjectBlock::FreeObjectArray()
{
    if (m_numObjects > 0 && m_papoObjHdr)
    {
        for(int i=0; i<m_numObjects; i++)
            delete m_papoObjHdr[i];
        CPLFree(m_papoObjHdr);
    }
    m_papoObjHdr = NULL;
    m_numObjects = 0;
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
    FreeObjectArray();

    GotoByteInBlock(0x002);
    m_numDataBytes = ReadInt16();       /* Excluding 4 bytes header */

    m_nCenterX = ReadInt32();
    m_nCenterY = ReadInt32();

    m_nFirstCoordBlock = ReadInt32();
    m_nLastCoordBlock = ReadInt32();

    m_nCurObjectOffset = -1;
    m_nCurObjectId = -1;

    return 0;
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
        m_nCurObjectType = ReadByte();
    }
    else
    {
        m_nCurObjectType = -1;
    }

    if( m_nCurObjectType <= 0 || m_nCurObjectType >= 0x80 )
    {
        m_nCurObjectType = -1;
        m_nCurObjectId = -1;
        m_nCurObjectOffset = -1;
    }
    else
    {
        m_nCurObjectId = ReadInt32();

        // Is this object marked as deleted?  If so, skip it.
        // I check both the top bits but I have only seen this occur
        // with the second highest bit set (ie. in usa/states.tab). NFW.

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
     * Write all the individual objects
     *----------------------------------------------------------------*/
    for(int i=0; i<m_numObjects; i++)
    {
        m_papoObjHdr[i]->WriteObj(this);
    }

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
    // UpdateMBR() call.
    m_nMinX = 1000000000;
    m_nMaxX = -1000000000;
    m_nMinY = 1000000000;
    m_nMaxY = -1000000000;

    // Make sure there is no object header left from last time.
    FreeObjectArray();

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
 * the block.  If bCompr=TRUE then the coordinates are written relative to
 * the object block center... otherwise they're written as 32 bits int.
 *
 * This function does not maintain the block's MBR and center... it is 
 * assumed to have been set before the first call to WriteIntCoord()
 *
 * Returns 0 if succesful or -1 if an error happened, in which case 
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPObjectBlock::WriteIntCoord(GInt32 nX, GInt32 nY,
                                         GBool bCompressed /*=FALSE*/)
{

    /*-----------------------------------------------------------------
     * Write coords to the file.
     *----------------------------------------------------------------*/
    if ((!bCompressed && (WriteInt32(nX) != 0 || WriteInt32(nY) != 0 ) ) ||
        (bCompressed && (WriteInt16(nX - m_nCenterX) != 0 ||
                         WriteInt16(nY - m_nCenterY) != 0) ) )
    {
        return -1;
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
 * If bCompr=TRUE then the coordinates are written relative to
 * the object block center... otherwise they're written as 32 bits int.
 *
 * This function does not maintain the block's MBR and center... it is 
 * assumed to have been set before the first call to WriteIntCoord()
 *
 * Returns 0 if succesful or -1 if an error happened, in which case 
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPObjectBlock::WriteIntMBRCoord(GInt32 nXMin, GInt32 nYMin,
                                            GInt32 nXMax, GInt32 nYMax,
                                            GBool bCompressed /*=FALSE*/)
{
    if (WriteIntCoord(MIN(nXMin, nXMax), MIN(nYMin, nYMax),
                      bCompressed) != 0 ||
        WriteIntCoord(MAX(nXMin, nXMax), MAX(nYMin, nYMax), 
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
 * Returns 0 if succesful or -1 if an error happened, in which case 
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
    
    m_nCenterX = (m_nMinX + m_nMaxX) /2;
    m_nCenterY = (m_nMinY + m_nMaxY) /2;
    
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
 *                   TABMAPObjectBlock::AddObject()
 *
 * Add an object to be eventually writtent to this object.  The poObjHdr
 * becomes owned by the TABMAPObjectBlock object and will be destroyed
 * once we're done with it.
 *
 * In write mode, an array of TABMAPObjHdr objects is maintained in memory
 * until the object is full, at which time the block center is recomputed
 * and all objects are written to the block.
 **********************************************************************/
int     TABMAPObjectBlock::AddObject(TABMAPObjHdr *poObjHdr)
{
    // We do not store NONE objects
    if (poObjHdr->m_nType == TAB_GEOM_NONE)
    {
        delete poObjHdr;
        return 0;
    }

    if (m_papoObjHdr == NULL || m_numObjects%10 == 0)
    {
        // Realloc the array... by steps of 10
        m_papoObjHdr = (TABMAPObjHdr**)CPLRealloc(m_papoObjHdr, 
                                                  (m_numObjects+10)*
                                                    sizeof(TABMAPObjHdr*));
    }

    m_papoObjHdr[m_numObjects++] = poObjHdr;

    // Maintain MBR of this object block.
    UpdateMBR(poObjHdr->m_nMinX, poObjHdr->m_nMinY);
    UpdateMBR(poObjHdr->m_nMaxX, poObjHdr->m_nMaxY);
   
    return 0;
}

/**********************************************************************
 *                   TABMAPObjectBlock::Dump()
 *
 * Dump block contents... available only in DEBUG mode.
 **********************************************************************/
#ifdef DEBUG

void TABMAPObjectBlock::Dump(FILE *fpOut, GBool bDetails)
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

    if (bDetails)
    {
        /* We need the mapfile's header block */
        TABRawBinBlock *poBlock;
        TABMAPHeaderBlock *poHeader;

        poBlock = TABCreateMAPBlockFromFile(m_fp, 0, 512);
        if (poBlock==NULL || poBlock->GetBlockClass() != TABMAP_HEADER_BLOCK)
        {
            CPLError(CE_Failure, CPLE_AssertionFailed, 
                     "Failed reading header block.");
            return;
        }
        poHeader = (TABMAPHeaderBlock *)poBlock;

        while(AdvanceToNextObject(poHeader) != -1)
        {
            fprintf(fpOut, 
                    "   object id=%d, type=%d, offset=%d (%d), size=%d \n",
                    m_nCurObjectId, m_nCurObjectType, m_nCurObjectOffset,
                    m_nFileOffset + m_nCurObjectOffset,
                    poHeader->GetMapObjectSize( m_nCurObjectType ) );
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
 * __TODO__
 * - Why are most of the ReadObj() methods not implemented???
 *    -> Because it's faster to not parse the whole object block all the time 
 *       but instead parse one object at a time in read mode... so there is
 *       no real use for this class in read mode UNTIL we support random
 *       update.
 *       At this point, only TABRegion and TABPolyline make use of the 
 *       ReadObj() methods.
 **********************************************************************/


/**********************************************************************
 *                    TABMAPObjHdr::NewObj()
 *
 * Alloc a new object of specified type or NULL for NONE types or if type 
 * is not supported.
 **********************************************************************/
TABMAPObjHdr *TABMAPObjHdr::NewObj(GByte nNewObjType, GInt32 nId /*=0*/)
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
        poObj = new TABMAPObjMultiPoint;
        break;
      case TAB_GEOM_COLLECTION_C:
      case TAB_GEOM_COLLECTION:
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
 * if succesful.  
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
    return ((m_nType % 3) == 1 ? TRUE : FALSE);
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
    poObjBlock->WriteByte(m_nType);
    return poObjBlock->WriteInt32(m_nId);
}

/**********************************************************************
 *                   TABMAPObjHdr::SetMBR()
 *
 **********************************************************************/
void TABMAPObjHdr::SetMBR(GInt32 nMinX, GInt32 nMinY, 
                          GInt32 nMaxX, GInt32 nMaxY)
{
    m_nMinX = MIN(nMinX, nMaxX);
    m_nMinY = MIN(nMinY, nMaxY);
    m_nMaxX = MAX(nMinX, nMaxX);
    m_nMaxY = MAX(nMinY, nMaxY);
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


    // Number of line segments applies only to MULTIPLINE/REGION but not PLINE
    if (m_nType == TAB_GEOM_PLINE_C ||
        m_nType == TAB_GEOM_PLINE )
    {
        m_numLineSections = 1;
    }
    else
    {
        m_numLineSections = poObjBlock->ReadInt16();
    }

#ifdef TABDUMP
    printf("PLINE/REGION: id=%d, type=%d, "
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

        m_nLabelX += m_nComprOrgX;
        m_nLabelY += m_nComprOrgY;

        m_nMinX = m_nComprOrgX + poObjBlock->ReadInt16();  // Read MBR
        m_nMinY = m_nComprOrgY + poObjBlock->ReadInt16();
        m_nMaxX = m_nComprOrgX + poObjBlock->ReadInt16();
        m_nMaxY = m_nComprOrgY + poObjBlock->ReadInt16();
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
        m_nComprOrgX = (m_nMinX + m_nMaxX) / 2;
        m_nComprOrgY = (m_nMinY + m_nMaxY) / 2;
    }

    m_nPenId = poObjBlock->ReadByte();      // Pen index

    if (m_nType == TAB_GEOM_REGION ||
        m_nType == TAB_GEOM_REGION_C ||
        m_nType == TAB_GEOM_V450_REGION ||
        m_nType == TAB_GEOM_V450_REGION_C )
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
    if (m_nType != TAB_GEOM_PLINE_C &&
        m_nType != TAB_GEOM_PLINE )
    {
        poObjBlock->WriteInt16(m_numLineSections);
    }

    if (IsCompressedType())
    {
        // Region center/label point, relative to compr. coord. origin
        // No it's not relative to the Object block center
        poObjBlock->WriteInt16(m_nLabelX - m_nComprOrgX);
        poObjBlock->WriteInt16(m_nLabelY - m_nComprOrgY);

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
        poObjBlock->WriteInt16(m_nMinX - m_nComprOrgX);
        poObjBlock->WriteInt16(m_nMinY - m_nComprOrgY);
        poObjBlock->WriteInt16(m_nMaxX - m_nComprOrgX);
        poObjBlock->WriteInt16(m_nMaxY - m_nComprOrgY);
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
        m_nType == TAB_GEOM_V450_REGION_C )
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
//__TODO__  For now this is read directly in ReadGeometryFromMAPFile()
//          This will be implemented the day we support random update.
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
//__TODO__  For now this is read directly in ReadGeometryFromMAPFile()
//          This will be implemented the day we support random update.
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
//__TODO__  For now this is read directly in ReadGeometryFromMAPFile()
//          This will be implemented the day we support random update.
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
//__TODO__  For now this is read directly in ReadGeometryFromMAPFile()
//          This will be implemented the day we support random update.
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
            poObjBlock->WriteInt16(m_nCornerWidth);
            poObjBlock->WriteInt16(m_nCornerHeight);
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
//__TODO__  For now this is read directly in ReadGeometryFromMAPFile()
//          This will be implemented the day we support random update.
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

    poObjBlock->WriteInt16(m_nStartAngle);
    poObjBlock->WriteInt16(m_nEndAngle);
    
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
//__TODO__  For now this is read directly in ReadGeometryFromMAPFile()
//          This will be implemented the day we support random update.
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
    poObjBlock->WriteInt16(m_nCoordDataSize);     // String length
    poObjBlock->WriteInt16(m_nTextAlignment);     // just./spacing/arrow

    poObjBlock->WriteInt16(m_nAngle);             // Tenths of degree

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

    if (IsCompressedType())
    {
        m_nCoordDataSize = m_nNumPoints * 2 * 2;
    }
    else
    {
        m_nCoordDataSize = m_nNumPoints * 2 * 4;
    }


#ifdef TABDUMP
    printf("MULTIPOINT: id=%d, type=%d, "
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

        m_nLabelX += m_nComprOrgX;
        m_nLabelY += m_nComprOrgY;

        m_nMinX = m_nComprOrgX + poObjBlock->ReadInt16();  // Read MBR
        m_nMinY = m_nComprOrgY + poObjBlock->ReadInt16();
        m_nMaxX = m_nComprOrgX + poObjBlock->ReadInt16();
        m_nMaxY = m_nComprOrgY + poObjBlock->ReadInt16();
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
        m_nComprOrgX = (m_nMinX + m_nMaxX) / 2;
        m_nComprOrgY = (m_nMinY + m_nMaxY) / 2;
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
    poObjBlock->WriteInt32(0);
    poObjBlock->WriteInt32(0);
    poObjBlock->WriteInt32(0);
    poObjBlock->WriteByte(0);
    poObjBlock->WriteByte(0);
    poObjBlock->WriteByte(0);

    // Symbol Id
    poObjBlock->WriteByte(m_nSymbolId);

    // ????
    poObjBlock->WriteByte(0);

    // MBR
    if (IsCompressedType())
    {
        // Region center/label point, relative to compr. coord. origin
        // No it's not relative to the Object block center
        poObjBlock->WriteInt16(m_nLabelX - m_nComprOrgX);
        poObjBlock->WriteInt16(m_nLabelY - m_nComprOrgY);

        poObjBlock->WriteInt32(m_nComprOrgX);
        poObjBlock->WriteInt32(m_nComprOrgY);

        // MBR relative to object origin (and not object block center)
        poObjBlock->WriteInt16(m_nMinX - m_nComprOrgX);
        poObjBlock->WriteInt16(m_nMinY - m_nComprOrgY);
        poObjBlock->WriteInt16(m_nMaxX - m_nComprOrgX);
        poObjBlock->WriteInt16(m_nMaxY - m_nComprOrgY);
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
    const int SIZE_OF_PLINE_HDR = 24;
    const int SIZE_OF_REGION_HDR = 24;
//    const int SIZE_OF_MULTI_PT_HDR = 24;
  
    m_nCoordBlockPtr = poObjBlock->ReadInt32();    // pointer into coord block
    m_nNumMultiPoints = poObjBlock->ReadInt32();   // no. points in multi point
    m_nRegionDataSize = poObjBlock->ReadInt32();   // size of region data inc. section hdrs
    m_nPolylineDataSize = poObjBlock->ReadInt32(); // size of multipline data inc. section hdrs
    m_nNumRegSections = poObjBlock->ReadInt16();   // Num Region section headers
    m_nNumPLineSections = poObjBlock->ReadInt16(); // Num Pline section headers

    if (IsCompressedType())
    {
        m_nMPointDataSize = m_nNumMultiPoints * 2 * 2;
    }
    else
    {
        m_nMPointDataSize = m_nNumMultiPoints * 2 * 4;
    }
    /* NB. The Region and Pline section headers are supposed to be extended
     * by 2 bytes to align with a 4 byte boundary.  This extension is included
     * in the Region and Polyline data sizes read above. In reality the 
     * extension is nowhere to be found so the actual data sizes are
     * two bytes shorter per section header.
     */
    m_nTotalRegDataSize = 0;
    if(m_nNumRegSections > 0)
    {
        m_nTotalRegDataSize = SIZE_OF_REGION_HDR + m_nRegionDataSize - 
                           (2 * m_nNumRegSections);
    }
    m_nTotalPolyDataSize = 0;
    if(m_nNumPLineSections > 0)
    {
        m_nTotalPolyDataSize = SIZE_OF_PLINE_HDR + m_nPolylineDataSize - 
                            (2 * m_nNumPLineSections);
    }

#ifdef TABDUMP
    printf("COLLECTION: id=%d, type=%d (0x%x), "
           "CoordBlockPtr=%d, numRegionSections=%d, "
           "numPlineSections=%d, numPoints=%d\n",
           m_nId, m_nType, m_nType, m_nCoordBlockPtr, 
           m_nNumRegSections, m_nNumPLineSections, m_nNumMultiPoints);
#endif

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
    printf("COLLECTION: READING ComprOrg @ %d\n",
           poObjBlock->GetCurAddress());
#endif
        // Compressed coordinate origin
        m_nComprOrgX = poObjBlock->ReadInt32();
        m_nComprOrgY = poObjBlock->ReadInt32();

        m_nMinX = m_nComprOrgX + poObjBlock->ReadInt16();  // Read MBR
        m_nMinY = m_nComprOrgY + poObjBlock->ReadInt16();
        m_nMaxX = m_nComprOrgX + poObjBlock->ReadInt16();
        m_nMaxY = m_nComprOrgY + poObjBlock->ReadInt16();
#ifdef TABDUMP
    printf("COLLECTION: ComprOrgX,Y= (%d,%d)\n",
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
        m_nComprOrgX = (m_nMinX + m_nMaxX) / 2;
        m_nComprOrgY = (m_nMinY + m_nMaxY) / 2;
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

    poObjBlock->WriteInt32(m_nCoordBlockPtr);    // pointer into coord block
    poObjBlock->WriteInt32(m_nNumMultiPoints);   // no. points in multi point
    poObjBlock->WriteInt32(m_nRegionDataSize);   // size of region data inc. section hdrs
    poObjBlock->WriteInt32(m_nPolylineDataSize); // size of Mpolyline data inc. sction hdrs
    poObjBlock->WriteInt16(m_nNumRegSections);   // Num Region section headers
    poObjBlock->WriteInt16(m_nNumPLineSections); // Num Pline section headers

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
    printf("COLLECTION: WRITING ComprOrgX,Y= (%d,%d) @ %d\n",
           m_nComprOrgX, m_nComprOrgY, poObjBlock->GetCurAddress());
#endif
        // Compressed coordinate origin
        poObjBlock->WriteInt32(m_nComprOrgX);
        poObjBlock->WriteInt32(m_nComprOrgY);

        poObjBlock->WriteInt16(m_nMinX - m_nComprOrgX);  // MBR
        poObjBlock->WriteInt16(m_nMinY - m_nComprOrgY);
        poObjBlock->WriteInt16(m_nMaxX - m_nComprOrgX);
        poObjBlock->WriteInt16(m_nMaxY - m_nComprOrgY);
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
