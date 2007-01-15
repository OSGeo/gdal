/**********************************************************************
 * $Id: mitab_mapindexblock.cpp,v 1.9 2004/06/30 20:29:04 dmorissette Exp $
 *
 * Name:     mitab_mapindexblock.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the TABMAPIndexBlock class used to handle
 *           reading/writing of the .MAP files' index blocks
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
 * $Log: mitab_mapindexblock.cpp,v $
 * Revision 1.9  2004/06/30 20:29:04  dmorissette
 * Fixed refs to old address danmo@videotron.ca
 *
 * Revision 1.8  2001/09/14 03:23:55  warmerda
 * Substantial upgrade to support spatial queries using spatial indexes
 *
 * Revision 1.7  2000/05/23 17:02:54  daniel
 * Removed unused variables
 *
 * Revision 1.6  2000/05/19 06:45:10  daniel
 * Modified generation of spatial index to split index nodes and produce a
 * more balanced tree.
 *
 * Revision 1.5  2000/01/15 22:30:44  daniel
 * Switch to MIT/X-Consortium OpenSource license
 *
 * Revision 1.4  1999/10/01 03:46:31  daniel
 * Added ReadAllEntries() and more complete Dump() for debugging files
 *
 * Revision 1.3  1999/09/29 04:23:51  daniel
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
 *                      class TABMAPIndexBlock
 *====================================================================*/


/**********************************************************************
 *                   TABMAPIndexBlock::TABMAPIndexBlock()
 *
 * Constructor.
 **********************************************************************/
TABMAPIndexBlock::TABMAPIndexBlock(TABAccess eAccessMode /*= TABRead*/):
    TABRawBinBlock(eAccessMode, TRUE)
{
    m_numEntries = 0;

    m_nMinX = 1000000000;
    m_nMinY = 1000000000;
    m_nMaxX = -1000000000;
    m_nMaxY = -1000000000;

    m_poCurChild = NULL;
    m_nCurChildIndex = -1;
    m_poParentRef = NULL;
    m_poBlockManagerRef = NULL;
}

/**********************************************************************
 *                   TABMAPIndexBlock::~TABMAPIndexBlock()
 *
 * Destructor.
 **********************************************************************/
TABMAPIndexBlock::~TABMAPIndexBlock()
{
    if (m_poCurChild)
    {
        if (m_eAccess == TABWrite || m_eAccess == TABReadWrite)
            m_poCurChild->CommitToFile();
        delete m_poCurChild;
    }
}

/**********************************************************************
 *                   TABMAPIndexBlock::InitBlockFromData()
 *
 * Perform some initialization on the block after its binary data has
 * been set or changed (or loaded from a file).
 *
 * Returns 0 if succesful or -1 if an error happened, in which case 
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPIndexBlock::InitBlockFromData(GByte *pabyBuf, int nSize, 
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
    if (m_nBlockType != TABMAP_INDEX_BLOCK)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "InitBlockFromData(): Invalid Block Type: got %d expected %d",
                 m_nBlockType, TABMAP_INDEX_BLOCK);
        CPLFree(m_pabyBuf);
        m_pabyBuf = NULL;
        return -1;
    }

    /*-----------------------------------------------------------------
     * Init member variables
     *----------------------------------------------------------------*/
    GotoByteInBlock(0x002);
    m_numEntries = ReadInt16();

    if (m_numEntries > 0)
        ReadAllEntries();

    return 0;
}

/**********************************************************************
 *                   TABMAPIndexBlock::CommitToFile()
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
int     TABMAPIndexBlock::CommitToFile()
{
    int nStatus = 0;

    if ( m_pabyBuf == NULL )
    {
        CPLError(CE_Failure, CPLE_AssertionFailed, 
                 "CommitToFile(): Block has not been initialized yet!");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Commit child first
     *----------------------------------------------------------------*/
    if (m_poCurChild)
    {
        if (m_poCurChild->CommitToFile() != 0)
            return -1;
    }

    /*-----------------------------------------------------------------
     * Make sure 4 bytes block header is up to date.
     *----------------------------------------------------------------*/
    GotoByteInBlock(0x000);

    WriteInt16(TABMAP_INDEX_BLOCK);    // Block type code
    WriteInt16(m_numEntries);

    nStatus = CPLGetLastErrorNo();

    /*-----------------------------------------------------------------
     * Loop through all entries, writing each of them, and calling
     * CommitToFile() (recursively) on any child index entries we may
     * encounter.
     *----------------------------------------------------------------*/
    for(int i=0; nStatus == 0 && i<m_numEntries; i++)
    {
        if (nStatus == 0)
            nStatus = WriteNextEntry(&(m_asEntries[i]));
    }


    /*-----------------------------------------------------------------
     * OK, call the base class to write the block to disk.
     *----------------------------------------------------------------*/
    if (nStatus == 0)
        nStatus = TABRawBinBlock::CommitToFile();

    return nStatus;
}


/**********************************************************************
 *                   TABMAPIndexBlock::InitNewBlock()
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
int     TABMAPIndexBlock::InitNewBlock(FILE *fpSrc, int nBlockSize, 
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
    m_numEntries = 0;

    m_nMinX = 1000000000;
    m_nMinY = 1000000000;
    m_nMaxX = -1000000000;
    m_nMaxY = -1000000000;

    if (m_eAccess != TABRead)
    {
        GotoByteInBlock(0x000);

        WriteInt16(TABMAP_INDEX_BLOCK);     // Block type code
        WriteInt16(0);                      // num. index entries
    }

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}



/**********************************************************************
 *                   TABMAPIndexBlock::ReadNextEntry()
 *
 * Read the next index entry from the block and fill the sEntry
 * structure. 
 *
 * Returns 0 if succesful or -1 if we reached the end of the block.
 **********************************************************************/
int     TABMAPIndexBlock::ReadNextEntry(TABMAPIndexEntry *psEntry)
{
    if (m_nCurPos < 4)
        GotoByteInBlock( 0x004 );

    if (m_nCurPos > 4+(20*m_numEntries) )
    {
        // End of BLock
        return -1;
    }

    psEntry->XMin = ReadInt32();
    psEntry->YMin = ReadInt32();
    psEntry->XMax = ReadInt32();
    psEntry->YMax = ReadInt32();
    psEntry->nBlockPtr = ReadInt32();

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPIndexBlock::ReadAllEntries()
 *
 * Init the block by reading all entries from the data block.
 *
 * Returns 0 if succesful or -1 on error.
 **********************************************************************/
int     TABMAPIndexBlock::ReadAllEntries()
{
    CPLAssert(m_numEntries <= TAB_MAX_ENTRIES_INDEX_BLOCK);
    if (m_numEntries == 0)
        return 0;
    
    if (GotoByteInBlock( 0x004 ) != 0)
        return -1;

    for(int i=0; i<m_numEntries; i++)
    {
        if ( ReadNextEntry(&(m_asEntries[i])) != 0)
            return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABMAPIndexBlock::WriteNextEntry()
 *
 * Write the sEntry index entry at current position in the block.
 *
 * Returns 0 if succesful or -1 if we reached the end of the block.
 **********************************************************************/
int     TABMAPIndexBlock::WriteNextEntry(TABMAPIndexEntry *psEntry)
{
    if (m_nCurPos < 4)
        GotoByteInBlock( 0x004 );

    WriteInt32(psEntry->XMin);
    WriteInt32(psEntry->YMin);
    WriteInt32(psEntry->XMax);
    WriteInt32(psEntry->YMax);
    WriteInt32(psEntry->nBlockPtr);

    if (CPLGetLastErrorNo() != 0)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPIndexBlock::GetNumFreeEntries()
 *
 * Return the number of available entries in this block.
 *
 * __TODO__ This function could eventually be improved to search
 *          children leaves as well.
 **********************************************************************/
int     TABMAPIndexBlock::GetNumFreeEntries()
{
    /* nMaxEntries = (m_nBlockSize-4)/20;*/

    return (TAB_MAX_ENTRIES_INDEX_BLOCK - m_numEntries);
}

/**********************************************************************
 *                   TABMAPIndexBlock::GetEntry()
 *
 * Fetch a reference to the requested entry.
 *
 * @param iIndex index of entry, must be from 0 to GetNumEntries()-1. 
 *
 * @return a reference to the internal copy of the entry, or NULL if out
 * of range. 
 **********************************************************************/
TABMAPIndexEntry *TABMAPIndexBlock::GetEntry( int iIndex )
{
    if( iIndex < 0 || iIndex >= m_numEntries )
        return NULL;

    return m_asEntries + iIndex;
}

/**********************************************************************
 *                   TABMAPIndexBlock::GetCurMaxDepth()
 *
 * Return maximum depth in the currently loaded part of the index tree
 **********************************************************************/
int     TABMAPIndexBlock::GetCurMaxDepth()
{
    if (m_poCurChild)
        return m_poCurChild->GetCurMaxDepth() + 1;

    return 1;  /* No current child... this node counts for one. */
}

/**********************************************************************
 *                   TABMAPIndexBlock::GetMBR()
 *
 * Return the MBR for the current block.
 **********************************************************************/
void TABMAPIndexBlock::GetMBR(GInt32 &nXMin, GInt32 &nYMin, 
                                     GInt32 &nXMax, GInt32 &nYMax)
{
    nXMin = m_nMinX;
    nYMin = m_nMinY;
    nXMax = m_nMaxX;
    nYMax = m_nMaxY; 
}

/**********************************************************************
 *                   TABMAPIndexBlock::InsertEntry()
 *
 * Add a new entry to this index block.  It is assumed that there is at
 * least one free slot available, so if the block has to be split then it
 * should have been done prior to calling this function.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int     TABMAPIndexBlock::InsertEntry(GInt32 nXMin, GInt32 nYMin,
                                      GInt32 nXMax, GInt32 nYMax,
                                      GInt32 nBlockPtr)
{
    if (m_eAccess != TABWrite && m_eAccess != TABReadWrite)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
               "Failed adding index entry: File not opened for write access.");
        return -1;
    }

    if (GetNumFreeEntries() < 1)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Current Block Index is full, cannot add new entry.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Update count of entries and store new entry.
     *----------------------------------------------------------------*/
    m_numEntries++;
    CPLAssert(m_numEntries <= TAB_MAX_ENTRIES_INDEX_BLOCK);

    m_asEntries[m_numEntries-1].XMin = nXMin;
    m_asEntries[m_numEntries-1].YMin = nYMin;
    m_asEntries[m_numEntries-1].XMax = nXMax;
    m_asEntries[m_numEntries-1].YMax = nYMax;
    m_asEntries[m_numEntries-1].nBlockPtr = nBlockPtr;

    return 0;
}

/**********************************************************************
 *                   TABMAPIndexBlock::AddEntry()
 *
 * Recursively search the tree until we encounter the best leaf to
 * contain the specified object MBR and add the new entry to it.
 *
 * In the even that the selected leaf node would be full, then it will be
 * split and this split can propagate up to its parent, etc.
 *
 * If bAddInThisNodeOnly=TRUE, then the entry is added only locally and
 * we do not try to update the child node.  This is used when the parent 
 * of a node that is being splitted has to be updated.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int     TABMAPIndexBlock::AddEntry(GInt32 nXMin, GInt32 nYMin,
                                   GInt32 nXMax, GInt32 nYMax,
                                   GInt32 nBlockPtr,
                                   GBool bAddInThisNodeOnly /*=FALSE*/)
{
    int i;
    GBool bFound = FALSE;

    if (m_eAccess != TABWrite && m_eAccess != TABReadWrite)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
               "Failed adding index entry: File not opened for write access.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Update MBR now... even if we're going to split current node later.
     *----------------------------------------------------------------*/
    if (nXMin < m_nMinX)
        m_nMinX = nXMin;
    if (nXMax > m_nMaxX)
        m_nMaxX = nXMax;
    
    if (nYMin < m_nMinY)
        m_nMinY = nYMin;
    if (nYMax > m_nMaxY)
        m_nMaxY = nYMax;

    /*-----------------------------------------------------------------
     * Look for the best candidate to contain the new entry
     * __TODO__ For now we'll just look for the first entry that can 
     *          contain the MBR, but we could probably have a better
     *          search criteria to optimize the resulting tree
     *----------------------------------------------------------------*/

    /*-----------------------------------------------------------------
     * If bAddInThisNodeOnly=TRUE then we add the entry only locally
     * and do not need to look for the proper leaf to insert it.
     *----------------------------------------------------------------*/
    if (bAddInThisNodeOnly)
        bFound = TRUE;

    /*-----------------------------------------------------------------
     * First check if current child could be a valid candidate.
     *----------------------------------------------------------------*/
    if (!bFound &&
        m_poCurChild && (m_asEntries[m_nCurChildIndex].XMin <= nXMin &&
                         m_asEntries[m_nCurChildIndex].XMax >= nXMax &&
                         m_asEntries[m_nCurChildIndex].YMin <= nYMin &&
                         m_asEntries[m_nCurChildIndex].YMax >= nYMax ) )
    {

        bFound = TRUE;
    }

    /*-----------------------------------------------------------------
     * Scan all entries to find a valid candidate
     * We look for the entry whose center is the closest to the center
     * of the object to add.
     *----------------------------------------------------------------*/
    if (!bFound)
    {
        int nObjCenterX = (nXMin + nXMax)/2;
        int nObjCenterY = (nYMin + nYMax)/2;

        // Make sure blocks currently in memory are written to disk.
        if (m_poCurChild)
        {
            m_poCurChild->CommitToFile();
            delete m_poCurChild;
            m_poCurChild = NULL;
            m_nCurChildIndex = -1;
        }

        // Look for entry whose center is closest to center of new object
        int nBestCandidate = -1;
        int nMinDist = 2000000000;

        for(i=0; i<m_numEntries; i++)
        {
            int nX = (m_asEntries[i].XMin + m_asEntries[i].XMax)/2;
            int nY = (m_asEntries[i].YMin + m_asEntries[i].YMax)/2;

            int nDist = (nX-nObjCenterX)*(nX-nObjCenterX) +
                             (nY-nObjCenterY)*(nY-nObjCenterY);

            if (nBestCandidate==-1 || nDist < nMinDist)
            {
                nBestCandidate = i;
                nMinDist = nDist;
            }
        }
        
        if (nBestCandidate != -1)
        {
            // Try to load corresponding child... if it fails then we are
            // likely in a leaf node, so we'll add the new entry in the current
            // node.
            TABRawBinBlock *poBlock = NULL;

            // Prevent error message if referred block not committed yet.
            CPLPushErrorHandler(CPLQuietErrorHandler);

            if ((poBlock = TABCreateMAPBlockFromFile(m_fp, 
                                       m_asEntries[nBestCandidate].nBlockPtr,
                                       512, TRUE, TABReadWrite)) &&
                poBlock->GetBlockClass() == TABMAP_INDEX_BLOCK)
            {
                m_poCurChild = (TABMAPIndexBlock*)poBlock;
                poBlock = NULL;
                m_nCurChildIndex = nBestCandidate;
                m_poCurChild->SetParentRef(this);
                m_poCurChild->SetMAPBlockManagerRef(m_poBlockManagerRef);
                bFound = TRUE;
            }
                
            if (poBlock)
                delete poBlock;
            
            CPLPopErrorHandler();
            CPLErrorReset();
        }
    }

    if (bFound && !bAddInThisNodeOnly)
    {
        /*-------------------------------------------------------------
         * Found a child leaf... pass the call to it.
         *------------------------------------------------------------*/
        if (m_poCurChild->AddEntry(nXMin, nYMin, nXMax, nYMax, nBlockPtr) != 0)
            return -1;
    }
    else
    {
        /*-------------------------------------------------------------
         * Found no child to store new object... we're likely at the leaf
         * level so we'll store new object in current node
         *------------------------------------------------------------*/

        /*-------------------------------------------------------------
         * First thing to do is make sure that there is room for a new
         * entry in this node, and to split it if necessary.
         *------------------------------------------------------------*/
        if (GetNumFreeEntries() < 1)
        {
            if (m_poParentRef == NULL)
            {
                /*-----------------------------------------------------
                 * Splitting the root node adds one level to the tree, so
                 * after splitting we just redirect the call to the new
                 * child that's just been created.
                 *----------------------------------------------------*/
                if (SplitRootNode((nXMin+nXMax)/2, (nYMin+nYMax)/2) != 0)
                    return -1;  // Error happened and has already been reported

                CPLAssert(m_poCurChild);
                return m_poCurChild->AddEntry(nXMin, nYMin, nXMax, nYMax,
                                              nBlockPtr, TRUE);
            }
            else
            {
                /*-----------------------------------------------------
                 * Splitting a regular node
                 *----------------------------------------------------*/
                if (SplitNode((nXMin+nXMax)/2, (nYMin+nYMax)/2) != 0)
                    return -1; 
            }
        }

        if (InsertEntry(nXMin, nYMin, nXMax, nYMax, nBlockPtr) != 0)
            return -1;
    }

    /*-----------------------------------------------------------------
     * Update current node MBR and the reference to it in our parent.
     *----------------------------------------------------------------*/
    RecomputeMBR();

    return 0;
}

/**********************************************************************
 *                   TABMAPIndexBlock::SplitNode()
 *
 * Split current Node, update the references in the parent node, etc.
 * Note that Root Nodes cannot be split using this method... SplitRootNode()
 * should be used instead.
 *
 * nNewEntryX, nNewEntryY are the coord. of the center of the new entry that 
 * will be added after the split.  The split is done so that the current
 * node will be the one in which the new object should be stored.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int     TABMAPIndexBlock::SplitNode(int nNewEntryX, int nNewEntryY)
{
    int nSrcEntries = m_numEntries;
    int nWidth, nHeight, nCenterX1, nCenterY1, nCenterX2, nCenterY2;

    CPLAssert(m_poBlockManagerRef);

    /*-----------------------------------------------------------------
     * Create a 2nd node, and assign both nodes a MBR that is half
     * of the biggest dimension (width or height) of the current node's MBR
     *
     * We also want to keep this node's current child in here.
     * Since splitting happens only during an addentry() operation and 
     * then both the current child and nNewEntryX/Y should fit in the same
     * area.
     *----------------------------------------------------------------*/
    TABMAPIndexBlock *poNewNode = new TABMAPIndexBlock(m_eAccess);
    if (poNewNode->InitNewBlock(m_fp, 512, 
                                m_poBlockManagerRef->AllocNewBlock()) != 0)
    {
        return -1;
    }
    poNewNode->SetMAPBlockManagerRef(m_poBlockManagerRef);


    nWidth = ABS(m_nMaxX - m_nMinX);
    nHeight = ABS(m_nMaxY - m_nMinY);

    if (nWidth > nHeight)
    {
        // Split node horizontally
        nCenterY1 = nCenterY2 = m_nMinY + nHeight/2;

        if (nNewEntryX < (m_nMinX + m_nMaxX)/2)
        {
            nCenterX1 = m_nMinX + nWidth/4;
            nCenterX2 = m_nMaxX - nWidth/4;
        }
        else
        {
            nCenterX2 = m_nMinX + nWidth/4;
            nCenterX1 = m_nMaxX - nWidth/4;
        }
    }
    else
    {
        // Split node vertically
        nCenterX1 = nCenterX2 = m_nMinX + nWidth/2;

        if (nNewEntryY < (m_nMinY + m_nMaxY)/2)
        {
            nCenterY1 = m_nMinY + nHeight/4;
            nCenterY2 = m_nMaxY - nHeight/4;
        }
        else
        {
            nCenterY2 = m_nMinY + nHeight/4;
            nCenterY1 = m_nMaxY - nHeight/4;
        }
    }

    /*-----------------------------------------------------------------
     * Go through all entries and assign them to one of the 2 nodes.
     *
     * Criteria is that entries are assigned to the node in which their
     * center falls.
     *
     * Hummm... this does not prevent the possibility that one of the
     * 2 nodes might end up empty at the end.
     *----------------------------------------------------------------*/
    m_numEntries = 0;
    for(int iEntry=0; iEntry<nSrcEntries; iEntry++)
    {
        int nEntryCenterX = (m_asEntries[iEntry].XMax +
                             m_asEntries[iEntry].XMin) / 2;
        int nEntryCenterY = (m_asEntries[iEntry].YMax +
                             m_asEntries[iEntry].YMin) / 2;

        if (iEntry == m_nCurChildIndex ||
            (nWidth > nHeight && 
             ABS(nEntryCenterX-nCenterX1) < ABS(nEntryCenterX-nCenterX2)) ||
            (nWidth <= nHeight &&
             ABS(nEntryCenterY-nCenterY1) < ABS(nEntryCenterY-nCenterY2) ) )
        {
            // This entry stays in current node.
            InsertEntry(m_asEntries[iEntry].XMin, m_asEntries[iEntry].YMin,
                        m_asEntries[iEntry].XMax, m_asEntries[iEntry].YMax,
                        m_asEntries[iEntry].nBlockPtr);

            // We have to keep track of new m_nCurChildIndex value
            if (iEntry == m_nCurChildIndex) 
            {
                m_nCurChildIndex = m_numEntries-1;
            }
        }
        else
        {
            // This entry goes in the new node.
            poNewNode->InsertEntry(m_asEntries[iEntry].XMin, 
                                   m_asEntries[iEntry].YMin,
                                   m_asEntries[iEntry].XMax, 
                                   m_asEntries[iEntry].YMax,
                                   m_asEntries[iEntry].nBlockPtr);
        }
    }

    /*-----------------------------------------------------------------
     * If no entry was moved to second node, then move ALL entries except
     * the current child to the second node... this way current node will
     * have room for a new entry when this function exits.
     *----------------------------------------------------------------*/
    if (poNewNode->GetNumEntries() == 0)
    {
        nSrcEntries = m_numEntries;
        m_numEntries = 0;
        for(int iEntry=0; iEntry<nSrcEntries; iEntry++)
        {
            if (iEntry == m_nCurChildIndex)
            {
                // Keep current child in current node
                InsertEntry(m_asEntries[iEntry].XMin, 
                            m_asEntries[iEntry].YMin,
                            m_asEntries[iEntry].XMax,
                            m_asEntries[iEntry].YMax,
                            m_asEntries[iEntry].nBlockPtr);
                m_nCurChildIndex = m_numEntries-1;
            }
            else
            {
                // All other entries go to second node
                poNewNode->InsertEntry(m_asEntries[iEntry].XMin, 
                                       m_asEntries[iEntry].YMin,
                                       m_asEntries[iEntry].XMax, 
                                       m_asEntries[iEntry].YMax,
                                       m_asEntries[iEntry].nBlockPtr);
            }
        }
    }

    /*-----------------------------------------------------------------
     * Recompute MBR and update current node info in parent
     *----------------------------------------------------------------*/
    RecomputeMBR();
    poNewNode->RecomputeMBR();

    /*-----------------------------------------------------------------
     * Add second node info to parent and then flush it to disk.
     * This may trigger splitting of parent
     *----------------------------------------------------------------*/
    CPLAssert(m_poParentRef);
    int nMinX, nMinY, nMaxX, nMaxY;
    poNewNode->GetMBR(nMinX, nMinY, nMaxX, nMaxY);
    m_poParentRef->AddEntry(nMinX, nMinY, nMaxX, nMaxY,
                            poNewNode->GetNodeBlockPtr(), TRUE);
    poNewNode->CommitToFile();
    delete poNewNode;

    return 0;
}

/**********************************************************************
 *                   TABMAPIndexBlock::SplitRootNode()
 *
 * (private method)
 *
 * Split a Root Node.
 * First, a level of nodes must be added to the tree, then the contents
 * of what used to be the root node is moved 1 level down and then that
 * node is split like a regular node.
 *
 * Returns 0 on success, -1 on error
 **********************************************************************/
int TABMAPIndexBlock::SplitRootNode(int nNewEntryX, int nNewEntryY)
{
    CPLAssert(m_poBlockManagerRef);
    CPLAssert(m_poParentRef == NULL);

    /*-----------------------------------------------------------------
     * Since a root note cannot be split, we add a level of nodes
     * under it and we'll do the split at that level.
     *----------------------------------------------------------------*/
    TABMAPIndexBlock *poNewNode = new TABMAPIndexBlock(m_eAccess);

    if (poNewNode->InitNewBlock(m_fp, 512, 
                                m_poBlockManagerRef->AllocNewBlock()) != 0)
    {
        return -1;
    }
    poNewNode->SetMAPBlockManagerRef(m_poBlockManagerRef);

    // Move all entries to the new child
    int nSrcEntries = m_numEntries;
    m_numEntries = 0;
    for(int iEntry=0; iEntry<nSrcEntries; iEntry++)
    {
        poNewNode->InsertEntry(m_asEntries[iEntry].XMin, 
                               m_asEntries[iEntry].YMin,
                               m_asEntries[iEntry].XMax, 
                               m_asEntries[iEntry].YMax,
                               m_asEntries[iEntry].nBlockPtr);
    }
    
    /*-----------------------------------------------------------------
     * Transfer current child object to new node.
     *----------------------------------------------------------------*/
    if (m_poCurChild)
    {
        poNewNode->SetCurChildRef(m_poCurChild, m_nCurChildIndex);
        m_poCurChild->SetParentRef(poNewNode);
        m_poCurChild = NULL;
        m_nCurChildIndex = -1;
    }

    /*-----------------------------------------------------------------
     * Place info about new child in current node.
     *----------------------------------------------------------------*/
    poNewNode->RecomputeMBR();
    int nMinX, nMinY, nMaxX, nMaxY;
    poNewNode->GetMBR(nMinX, nMinY, nMaxX, nMaxY);
    InsertEntry(nMinX, nMinY, nMaxX, nMaxY, poNewNode->GetNodeBlockPtr());

    /*-----------------------------------------------------------------
     * Keep a reference to the new child
     *----------------------------------------------------------------*/
    poNewNode->SetParentRef(this);
    m_poCurChild = poNewNode;
    m_nCurChildIndex = m_numEntries -1;

    /*-----------------------------------------------------------------
     * And finally force the child to split itself
     *----------------------------------------------------------------*/
    return m_poCurChild->SplitNode(nNewEntryX, nNewEntryY);
}


/**********************************************************************
 *                   TABMAPIndexBlock::RecomputeMBR()
 *
 * Recompute current block MBR, and update info in parent.
 **********************************************************************/
void TABMAPIndexBlock::RecomputeMBR()
{
    m_nMinX = 1000000000;
    m_nMinY = 1000000000;
    m_nMaxX = -1000000000;
    m_nMaxY = -1000000000;

    for(int i=0; i<m_numEntries; i++)
    {
        if (m_asEntries[i].XMin < m_nMinX)
            m_nMinX = m_asEntries[i].XMin;
        if (m_asEntries[i].XMax > m_nMaxX)
            m_nMaxX = m_asEntries[i].XMax;
    
        if (m_asEntries[i].YMin < m_nMinY)
            m_nMinY = m_asEntries[i].YMin;
        if (m_asEntries[i].YMax > m_nMaxY)
            m_nMaxY = m_asEntries[i].YMax;
    }

    if (m_poParentRef)
        m_poParentRef->UpdateCurChildMBR(m_nMinX, m_nMinY, m_nMaxX, m_nMaxY,
                                         GetNodeBlockPtr());

}

/**********************************************************************
 *                   TABMAPIndexBlock::UpateCurChildMBR()
 *
 * Update current child MBR info, and propagate info in parent.
 *
 * nBlockPtr is passed only to validate the consistency of the tree.
 **********************************************************************/
void TABMAPIndexBlock::UpdateCurChildMBR(GInt32 nXMin, GInt32 nYMin,
                                         GInt32 nXMax, GInt32 nYMax,
                                         GInt32 nBlockPtr)
{
    CPLAssert(m_poCurChild);
    CPLAssert(m_asEntries[m_nCurChildIndex].nBlockPtr == nBlockPtr);

    m_asEntries[m_nCurChildIndex].XMin = nXMin;
    m_asEntries[m_nCurChildIndex].YMin = nYMin;
    m_asEntries[m_nCurChildIndex].XMax = nXMax;
    m_asEntries[m_nCurChildIndex].YMax = nYMax;

    m_nMinX = 1000000000;
    m_nMinY = 1000000000;
    m_nMaxX = -1000000000;
    m_nMaxY = -1000000000;

    for(int i=0; i<m_numEntries; i++)
    {
        if (m_asEntries[i].XMin < m_nMinX)
            m_nMinX = m_asEntries[i].XMin;
        if (m_asEntries[i].XMax > m_nMaxX)
            m_nMaxX = m_asEntries[i].XMax;
    
        if (m_asEntries[i].YMin < m_nMinY)
            m_nMinY = m_asEntries[i].YMin;
        if (m_asEntries[i].YMax > m_nMaxY)
            m_nMaxY = m_asEntries[i].YMax;
    }

    if (m_poParentRef)
        m_poParentRef->UpdateCurChildMBR(m_nMinX, m_nMinY, m_nMaxX, m_nMaxY,
                                         GetNodeBlockPtr());

}


/**********************************************************************
 *                   TABMAPIndexBlock::SetMAPBlockManagerRef()
 *
 * Pass a reference to the block manager object for the file this 
 * block belongs to.  The block manager will be used by this object
 * when it needs to automatically allocate a new block.
 **********************************************************************/
void TABMAPIndexBlock::SetMAPBlockManagerRef(TABBinBlockManager *poBlockMgr)
{
    m_poBlockManagerRef = poBlockMgr;
};

/**********************************************************************
 *                   TABMAPIndexBlock::SetParentRef()
 *
 * Used to pass a reference to this node's parent.
 **********************************************************************/
void    TABMAPIndexBlock::SetParentRef(TABMAPIndexBlock *poParent)
{
    m_poParentRef = poParent;
}

/**********************************************************************
 *                   TABMAPIndexBlock::SetCurChildRef()
 *
 * Used to transfer a child object from one node to another
 **********************************************************************/
void    TABMAPIndexBlock::SetCurChildRef(TABMAPIndexBlock *poChild,
                                         int nChildIndex)
{
    m_poCurChild = poChild;
    m_nCurChildIndex = nChildIndex;
}

/**********************************************************************
 *                   TABMAPIndexBlock::Dump()
 *
 * Dump block contents... available only in DEBUG mode.
 **********************************************************************/
#ifdef DEBUG

void TABMAPIndexBlock::Dump(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "----- TABMAPIndexBlock::Dump() -----\n");
    if (m_pabyBuf == NULL)
    {
        fprintf(fpOut, "Block has not been initialized yet.");
    }
    else
    {
        fprintf(fpOut,"Index Block (type %d) at offset %d.\n", 
                                                m_nBlockType, m_nFileOffset);
        fprintf(fpOut,"  m_numEntries          = %d\n", m_numEntries);

        /*-------------------------------------------------------------
         * Loop through all entries, dumping each of them
         *------------------------------------------------------------*/
        if (m_numEntries > 0)
            ReadAllEntries();

        for(int i=0; i<m_numEntries; i++)
        {
            fprintf(fpOut, "    %6d -> (%d, %d) - (%d, %d)\n",
                    m_asEntries[i].nBlockPtr,
                    m_asEntries[i].XMin, m_asEntries[i].YMin,
                    m_asEntries[i].XMax, m_asEntries[i].YMax );
        }

    }

    fflush(fpOut);
}

#endif // DEBUG
