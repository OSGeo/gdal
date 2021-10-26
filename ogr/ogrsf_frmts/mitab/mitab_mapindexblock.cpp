/**********************************************************************
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

#include <cmath>
#include <cstdlib>
#include <cstring>

#include <algorithm>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "mitab_priv.h"

CPL_CVSID("$Id$")

/*=====================================================================
 *                      class TABMAPIndexBlock
 *====================================================================*/

/**********************************************************************
 *                   TABMAPIndexBlock::TABMAPIndexBlock()
 *
 * Constructor.
 **********************************************************************/
TABMAPIndexBlock::TABMAPIndexBlock( TABAccess eAccessMode /*= TABRead*/ ) :
    TABRawBinBlock(eAccessMode, TRUE),
    m_numEntries(0),
    m_nMinX(1000000000),
    m_nMinY(1000000000),
    m_nMaxX(-1000000000),
    m_nMaxY(-1000000000),
    m_poBlockManagerRef(nullptr),
    m_poCurChild(nullptr),
    m_nCurChildIndex(-1),
    m_poParentRef(nullptr)
{
    memset(m_asEntries, 0, sizeof(m_asEntries));
}

/**********************************************************************
 *                   TABMAPIndexBlock::~TABMAPIndexBlock()
 *
 * Destructor.
 **********************************************************************/
TABMAPIndexBlock::~TABMAPIndexBlock()
{
    UnsetCurChild();
}

/**********************************************************************
 *                   TABMAPIndexBlock::UnsetCurChild()
 **********************************************************************/

void TABMAPIndexBlock::UnsetCurChild()
{
    if (m_poCurChild)
    {
        if (m_eAccess == TABWrite || m_eAccess == TABReadWrite)
            m_poCurChild->CommitToFile();
        delete m_poCurChild;
        m_poCurChild = nullptr;
    }
    m_nCurChildIndex = -1;
}

/**********************************************************************
 *                   TABMAPIndexBlock::InitBlockFromData()
 *
 * Perform some initialization on the block after its binary data has
 * been set or changed (or loaded from a file).
 *
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPIndexBlock::InitBlockFromData(GByte *pabyBuf,
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
    if (m_nBlockType != TABMAP_INDEX_BLOCK)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "InitBlockFromData(): Invalid Block Type: got %d expected %d",
                 m_nBlockType, TABMAP_INDEX_BLOCK);
        CPLFree(m_pabyBuf);
        m_pabyBuf = nullptr;
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
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPIndexBlock::CommitToFile()
{
    if ( m_pabyBuf == nullptr )
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
     * Nothing to do here if block has not been modified
     *----------------------------------------------------------------*/
    if (!m_bModified)
        return 0;

    /*-----------------------------------------------------------------
     * Make sure 4 bytes block header is up to date.
     *----------------------------------------------------------------*/
    GotoByteInBlock(0x000);

    WriteInt16(TABMAP_INDEX_BLOCK);    // Block type code
    WriteInt16(static_cast<GInt16>(m_numEntries));

    int nStatus = CPLGetLastErrorType() == CE_Failure ? -1 : 0;

    /*-----------------------------------------------------------------
     * Loop through all entries, writing each of them, and calling
     * CommitToFile() (recursively) on any child index entries we may
     * encounter.
     *----------------------------------------------------------------*/
    for(int i=0; nStatus == 0 && i<m_numEntries; i++)
    {
        nStatus = WriteNextEntry(&(m_asEntries[i]));
    }

    /*-----------------------------------------------------------------
     * OK, call the base class to write the block to disk.
     *----------------------------------------------------------------*/
    if (nStatus == 0)
    {
#ifdef DEBUG_VERBOSE
        CPLDebug("MITAB", "Committing INDEX block to offset %d", m_nFileOffset);
#endif
        nStatus = TABRawBinBlock::CommitToFile();
    }

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
 * Returns 0 if successful or -1 if an error happened, in which case
 * CPLError() will have been called.
 **********************************************************************/
int     TABMAPIndexBlock::InitNewBlock(VSILFILE *fpSrc, int nBlockSize,
                                        int nFileOffset /* = 0*/)
{
    /*-----------------------------------------------------------------
     * Start with the default initialization
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

    if (m_eAccess != TABRead && nFileOffset != 0)
    {
        GotoByteInBlock(0x000);

        WriteInt16(TABMAP_INDEX_BLOCK);     // Block type code
        WriteInt16(0);                      // num. index entries
    }

    if (CPLGetLastErrorType() == CE_Failure)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPIndexBlock::ReadNextEntry()
 *
 * Read the next index entry from the block and fill the sEntry
 * structure.
 *
 * Returns 0 if successful or -1 if we reached the end of the block.
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

    if (CPLGetLastErrorType() == CE_Failure)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPIndexBlock::ReadAllEntries()
 *
 * Init the block by reading all entries from the data block.
 *
 * Returns 0 if successful or -1 on error.
 **********************************************************************/
int     TABMAPIndexBlock::ReadAllEntries()
{
    CPLAssert(m_numEntries <= GetMaxEntries());
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
 * Returns 0 if successful or -1 if we reached the end of the block.
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

    if (CPLGetLastErrorType() == CE_Failure)
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
    return (m_nBlockSize-4)/20 - m_numEntries;
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
        return nullptr;

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
 *                   TABMAPIndexBlock::SetMBR()
 *
 **********************************************************************/
void TABMAPIndexBlock::SetMBR(GInt32 nXMin, GInt32 nYMin,
                              GInt32 nXMax, GInt32 nYMax)
{
    m_nMinX = nXMin;
    m_nMinY = nYMin;
    m_nMaxX = nXMax;
    m_nMaxY = nYMax;
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
    CPLAssert(m_numEntries <= GetMaxEntries());

    m_asEntries[m_numEntries-1].XMin = nXMin;
    m_asEntries[m_numEntries-1].YMin = nYMin;
    m_asEntries[m_numEntries-1].XMax = nXMax;
    m_asEntries[m_numEntries-1].YMax = nYMax;
    m_asEntries[m_numEntries-1].nBlockPtr = nBlockPtr;

    m_bModified = TRUE;

    return 0;
}

/**********************************************************************
 *                   TABMAPIndexBlock::ChooseSubEntryForInsert()
 *
 * Select the entry in this index block in which the new entry should
 * be inserted. The criteria used is to select the node whose MBR needs
 * the least enlargement to include the new entry. We resolve ties by
 * choosing the entry with the rectangle of smallest area.
 * (This is the ChooseSubtree part of Guttman's "ChooseLeaf" algorithm.)
 *
 * Returns the index of the best candidate or -1 of node is empty.
 **********************************************************************/
int     TABMAPIndexBlock::ChooseSubEntryForInsert(GInt32 nXMin, GInt32 nYMin,
                                                  GInt32 nXMax, GInt32 nYMax)
{
    GInt32 nBestCandidate = -1;

    double dOptimalAreaDiff = 0.0;

    const double dNewEntryArea = MITAB_AREA(nXMin, nYMin, nXMax, nYMax);

    for( GInt32 i = 0; i<m_numEntries; i++ )
    {
        double dAreaDiff = 0.0;
        const double dAreaBefore =
            MITAB_AREA(m_asEntries[i].XMin,
                       m_asEntries[i].YMin,
                       m_asEntries[i].XMax,
                       m_asEntries[i].YMax);

        /* Does this entry fully contain the new entry's MBR ?
         */
        const GBool bIsContained =
            nXMin >= m_asEntries[i].XMin &&
            nYMin >= m_asEntries[i].YMin &&
            nXMax <= m_asEntries[i].XMax &&
            nYMax <= m_asEntries[i].YMax;

        if( bIsContained )
        {
            /* If new entry is fully contained in this entry then
             * the area difference will be the difference between the area
             * of the entry to insert and the area of m_asEntries[i]
             *
             * The diff value is negative in this case.
             */
            dAreaDiff = dNewEntryArea - dAreaBefore;
        }
        else
        {
            /* Need to calculate the expanded MBR to calculate the area
             * difference.
             */
            GInt32 nXMin2 = std::min(m_asEntries[i].XMin, nXMin);
            GInt32 nYMin2 = std::min(m_asEntries[i].YMin, nYMin);
            GInt32 nXMax2 = std::max(m_asEntries[i].XMax, nXMax);
            GInt32 nYMax2 = std::max(m_asEntries[i].YMax, nYMax);

            dAreaDiff = MITAB_AREA(nXMin2,nYMin2,nXMax2,nYMax2) - dAreaBefore;
        }

        /* Is this a better candidate?
         * Note, possible Optimization: In case of tie, we could to pick the
         * candidate with the smallest area
         */

        if (/* No best candidate yet */
            (nBestCandidate == -1)
            /* or current candidate is contained and best candidate is not contained */
            || (dAreaDiff < 0 && dOptimalAreaDiff >= 0)
            /* or if both are either contained or not contained then use the one
             * with the smallest area diff, which means maximum coverage in the case
             * of contained rects, or minimum area increase when not contained
             */
            || (((dOptimalAreaDiff < 0 && dAreaDiff < 0) ||
                 (dOptimalAreaDiff > 0 && dAreaDiff > 0)) &&
                std::abs(dAreaDiff) < std::abs(dOptimalAreaDiff)) )
        {
            nBestCandidate = i;
            dOptimalAreaDiff = dAreaDiff;
        }
    }

    return nBestCandidate;
}

/**********************************************************************
 *                   TABMAPIndexBlock::ChooseLeafForInsert()
 *
 * Recursively search the tree until we find the best leaf to
 * contain the specified object MBR.
 *
 * Returns the nBlockPtr of the selected leaf node entry (should be a
 * ref to a TABMAPObjectBlock) or -1 on error.
 *
 * After this call, m_poCurChild will be pointing at the selected child
 * node, for use by later calls to UpdateLeafEntry()
 **********************************************************************/
GInt32  TABMAPIndexBlock::ChooseLeafForInsert(GInt32 nXMin, GInt32 nYMin,
                                              GInt32 nXMax, GInt32 nYMax)
{
    GBool bFound = FALSE;

    if (m_numEntries < 0)
        return -1;

    /*-----------------------------------------------------------------
     * Look for the best candidate to contain the new entry
     *----------------------------------------------------------------*/

    // Make sure blocks currently in memory are written to disk.
    // TODO: Could we avoid deleting m_poCurChild if it is already
    //       the best candidate for insert?
    if (m_poCurChild)
    {
        m_poCurChild->CommitToFile();
        delete m_poCurChild;
        m_poCurChild = nullptr;
        m_nCurChildIndex = -1;
    }

    int nBestCandidate = ChooseSubEntryForInsert(nXMin,nYMin,nXMax,nYMax);

    CPLAssert(nBestCandidate != -1);
    if (nBestCandidate == -1)
        return -1;  /* This should never happen! */

    // Try to load corresponding child... if it fails then we are
    // likely in a leaf node, so we'll add the new entry in the current
    // node.

    // Prevent error message if referred block not committed yet.
    CPLPushErrorHandler(CPLQuietErrorHandler);

    TABRawBinBlock* poBlock = TABCreateMAPBlockFromFile(m_fp,
                                    m_asEntries[nBestCandidate].nBlockPtr,
                                    m_nBlockSize, TRUE, TABReadWrite);
    if (poBlock != nullptr && poBlock->GetBlockClass() == TABMAP_INDEX_BLOCK)
    {
        m_poCurChild = cpl::down_cast<TABMAPIndexBlock*>(poBlock);
        poBlock = nullptr;
        m_nCurChildIndex = nBestCandidate;
        m_poCurChild->SetParentRef(this);
        m_poCurChild->SetMAPBlockManagerRef(m_poBlockManagerRef);
        bFound = TRUE;
    }

    if (poBlock)
        delete poBlock;

    CPLPopErrorHandler();
    CPLErrorReset();

    if (bFound)
    {
        /*-------------------------------------------------------------
         * Found a child leaf... pass the call to it.
         *------------------------------------------------------------*/
        return m_poCurChild->ChooseLeafForInsert(nXMin, nYMin, nXMax, nYMax);
    }

    /*-------------------------------------------------------------
     * Found no child index node... we must be at the leaf level
     * (leaf points at map object data blocks) so we return a ref
     * to the TABMAPObjBlock for insertion
     *------------------------------------------------------------*/
    return m_asEntries[nBestCandidate].nBlockPtr;
}

/**********************************************************************
 *                   TABMAPIndexBlock::GetCurLeafEntryMBR()
 *
 * Get the MBR for specified nBlockPtr in the leaf at the end of the
 * chain of m_poCurChild refs.
 *
 * This method requires that the chain of m_poCurChild refs already point
 * to a leaf that contains the specified nBlockPtr, it is usually called
 * right after ChooseLeafForInsert().
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int     TABMAPIndexBlock::GetCurLeafEntryMBR(GInt32 nBlockPtr,
                                             GInt32 &nXMin, GInt32 &nYMin,
                                             GInt32 &nXMax, GInt32 &nYMax)
{
    if (m_poCurChild)
    {
        /* Pass the call down to current child */
        return m_poCurChild->GetCurLeafEntryMBR(nBlockPtr,
                                                nXMin, nYMin, nXMax, nYMax);
    }

    /* We're at the leaf level, look for the entry */
    for(int i=0; i<m_numEntries; i++)
    {
        if (m_asEntries[i].nBlockPtr == nBlockPtr)
        {
            /* Found it. Return its MBR */
            nXMin = m_asEntries[i].XMin;
            nYMin = m_asEntries[i].YMin;
            nXMax = m_asEntries[i].XMax;
            nYMax = m_asEntries[i].YMax;

            return 0;
        }
    }

    /* Not found! This should not happen if method is used properly. */
    CPLError(CE_Failure, CPLE_AssertionFailed,
             "Entry to update not found in GetCurLeafEntryMBR()!");
    return -1;
}

/**********************************************************************
 *                   TABMAPIndexBlock::UpdateLeafEntry()
 *
 * Update the MBR for specified nBlockPtr in the leaf at the end of the
 * chain of m_poCurChild refs and update MBR of parents if required.
 *
 * This method requires that the chain of m_poCurChild refs already point
 * to a leaf that contains the specified nBlockPtr, it is usually called
 * right after ChooseLeafForInsert().
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int     TABMAPIndexBlock::UpdateLeafEntry(GInt32 nBlockPtr,
                                          GInt32 nXMin, GInt32 nYMin,
                                          GInt32 nXMax, GInt32 nYMax )
{
    if (m_poCurChild)
    {
        /* Pass the call down to current child */
        return m_poCurChild->UpdateLeafEntry(nBlockPtr,
                                             nXMin, nYMin, nXMax, nYMax);
    }

    /* We're at the leaf level, look for the entry to update */
    for(int i=0; i<m_numEntries; i++)
    {
        if (m_asEntries[i].nBlockPtr == nBlockPtr)
        {
            /* Found it. */
            TABMAPIndexEntry *psEntry = &m_asEntries[i];

            if (psEntry->XMin != nXMin ||
                psEntry->YMin != nYMin ||
                psEntry->XMax != nXMax ||
                psEntry->YMax != nYMax )
            {
                /* MBR changed. Update MBR of entry */
                psEntry->XMin = nXMin;
                psEntry->YMin = nYMin;
                psEntry->XMax = nXMax;
                psEntry->YMax = nYMax;

                m_bModified = TRUE;

                /* Update MBR of this node and all parents */
                RecomputeMBR();
            }

            return 0;
        }
    }

    /* Not found! This should not happen if method is used properly. */
    CPLError(CE_Failure, CPLE_AssertionFailed,
             "Entry to update not found in UpdateLeafEntry()!");
    return -1;
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
 * of a node that is being split has to be updated.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int     TABMAPIndexBlock::AddEntry(GInt32 nXMin, GInt32 nYMin,
                                   GInt32 nXMax, GInt32 nYMax,
                                   GInt32 nBlockPtr,
                                   GBool bAddInThisNodeOnly /*=FALSE*/)
{
    GBool bFound = FALSE;

    if (m_eAccess != TABWrite && m_eAccess != TABReadWrite)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
               "Failed adding index entry: File not opened for write access.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Look for the best candidate to contain the new entry
     *----------------------------------------------------------------*/

    /*-----------------------------------------------------------------
     * If bAddInThisNodeOnly=TRUE then we add the entry only locally
     * and do not need to look for the proper leaf to insert it.
     *----------------------------------------------------------------*/
    if (bAddInThisNodeOnly)
        bFound = TRUE;

    if (!bFound && m_numEntries > 0)
    {
        // Make sure blocks currently in memory are written to disk.
        if (m_poCurChild)
        {
            m_poCurChild->CommitToFile();
            delete m_poCurChild;
            m_poCurChild = nullptr;
            m_nCurChildIndex = -1;
        }

        int nBestCandidate = ChooseSubEntryForInsert(nXMin,nYMin,nXMax,nYMax);

        CPLAssert(nBestCandidate != -1);

        if (nBestCandidate != -1)
        {
            // Try to load corresponding child... if it fails then we are
            // likely in a leaf node, so we'll add the new entry in the current
            // node.

            // Prevent error message if referred block not committed yet.
            CPLPushErrorHandler(CPLQuietErrorHandler);

            TABRawBinBlock* poBlock = TABCreateMAPBlockFromFile(m_fp,
                                       m_asEntries[nBestCandidate].nBlockPtr,
                                       m_nBlockSize, TRUE, TABReadWrite);
            if (poBlock != nullptr && poBlock->GetBlockClass() == TABMAP_INDEX_BLOCK)
            {
                m_poCurChild = cpl::down_cast<TABMAPIndexBlock*>(poBlock);
                poBlock = nullptr;
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
            if (m_poParentRef == nullptr)
            {
                /*-----------------------------------------------------
                 * Splitting the root node adds one level to the tree, so
                 * after splitting we just redirect the call to the new
                 * child that's just been created.
                 *----------------------------------------------------*/
                if (SplitRootNode(nXMin, nYMin, nXMax, nYMax) != 0)
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
                if (SplitNode(nXMin, nYMin, nXMax, nYMax) != 0)
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
 *                   TABMAPIndexBlock::ComputeAreaDiff()
 *
 * (static method, also used by the TABMAPObjBlock class)
 *
 * Compute the area difference between two MBRs. Used in the SplitNode
 * algorithm to decide to which of the two nodes an entry should be added.
 *
 * The returned AreaDiff value is positive if NodeMBR has to be enlarged
 * and negative if new Entry is fully contained in the NodeMBR.
 **********************************************************************/
double  TABMAPIndexBlock::ComputeAreaDiff( GInt32 nNodeXMin, GInt32 nNodeYMin,
                                           GInt32 nNodeXMax, GInt32 nNodeYMax,
                                           GInt32 nEntryXMin, GInt32 nEntryYMin,
                                           GInt32 nEntryXMax,
                                           GInt32 nEntryYMax )
{
    double dAreaDiff = 0.0;

    const double dNodeAreaBefore =
        MITAB_AREA(nNodeXMin,
                   nNodeYMin,
                   nNodeXMax,
                   nNodeYMax);

    // Does the node fully contain the new entry's MBR?
    const GBool bIsContained =
        nEntryXMin >= nNodeXMin &&
        nEntryYMin >= nNodeYMin &&
        nEntryXMax <= nNodeXMax &&
        nEntryYMax <= nNodeYMax;

    if( bIsContained )
    {
        /* If new entry is fully contained in this entry then
         * the area difference will be the difference between the area
         * of the entry to insert and the area of the node
         */
        dAreaDiff = MITAB_AREA(nEntryXMin, nEntryYMin,
                               nEntryXMax, nEntryYMax) - dNodeAreaBefore;
    }
    else
    {
        /* Need to calculate the expanded MBR to calculate the area
         * difference.
         */
        nNodeXMin = std::min(nNodeXMin, nEntryXMin);
        nNodeYMin = std::min(nNodeYMin, nEntryYMin);
        nNodeXMax = std::max(nNodeXMax, nEntryXMax);
        nNodeYMax = std::max(nNodeYMax, nEntryYMax);

        dAreaDiff = MITAB_AREA(nNodeXMin,nNodeYMin,
                               nNodeXMax,nNodeYMax) - dNodeAreaBefore;
    }

    return dAreaDiff;
}

/**********************************************************************
 *                   TABMAPIndexBlock::PickSeedsForSplit()
 *
 * (static method, also used by the TABMAPObjBlock class)
 *
 * Pick two seeds to use to start splitting this node.
 *
 * Guttman's LinearPickSeed:
 * - Along each dimension find the entry whose rectangle has the
 *   highest low side, and the one with the lowest high side
 * - Calculate the separation for each pair
 * - Normalize the separation by dividing by the extents of the
 *   corresponding dimension
 * - Choose the pair with the greatest normalized separation along
 *   any dimension
 **********************************************************************/
int TABMAPIndexBlock::PickSeedsForSplit( TABMAPIndexEntry *pasEntries,
                                         int numEntries,
                                         int nSrcCurChildIndex,
                                         GInt32 nNewEntryXMin,
                                         GInt32 nNewEntryYMin,
                                         GInt32 nNewEntryXMax,
                                         GInt32 nNewEntryYMax,
                                         int &nSeed1, int &nSeed2 )
{
    GInt32 nSrcMinX = 0;
    GInt32 nSrcMinY = 0;
    GInt32 nSrcMaxX = 0;
    GInt32 nSrcMaxY = 0;
    int nLowestMaxX = -1;
    int nHighestMinX = -1;
    int nLowestMaxY = -1;
    int nHighestMinY = -1;
    GInt32 nLowestMaxXId=-1;
    GInt32 nHighestMinXId=-1;
    GInt32 nLowestMaxYId=-1;
    GInt32 nHighestMinYId = -1;

    nSeed1 = -1;
    nSeed2 = -1;

    // Along each dimension find the entry whose rectangle has the
    // highest low side, and the one with the lowest high side
    for(int iEntry=0; iEntry<numEntries; iEntry++)
    {
        if (nLowestMaxXId == -1 ||
            pasEntries[iEntry].XMax < nLowestMaxX)
        {
            nLowestMaxX = pasEntries[iEntry].XMax;
            nLowestMaxXId = iEntry;
        }

        if (nHighestMinXId == -1 ||
            pasEntries[iEntry].XMin > nHighestMinX)
        {
            nHighestMinX = pasEntries[iEntry].XMin;
            nHighestMinXId = iEntry;
        }

        if (nLowestMaxYId == -1 ||
            pasEntries[iEntry].YMax < nLowestMaxY)
        {
            nLowestMaxY = pasEntries[iEntry].YMax;
            nLowestMaxYId = iEntry;
        }

        if (nHighestMinYId == -1 ||
            pasEntries[iEntry].YMin > nHighestMinY)
        {
            nHighestMinY = pasEntries[iEntry].YMin;
            nHighestMinYId = iEntry;
        }

        // Also keep track of MBR of all entries
        if (iEntry == 0)
        {
            nSrcMinX = pasEntries[iEntry].XMin;
            nSrcMinY = pasEntries[iEntry].YMin;
            nSrcMaxX = pasEntries[iEntry].XMax;
            nSrcMaxY = pasEntries[iEntry].YMax;
        }
        else
        {
            nSrcMinX = std::min(nSrcMinX, pasEntries[iEntry].XMin);
            nSrcMinY = std::min(nSrcMinY, pasEntries[iEntry].YMin);
            nSrcMaxX = std::max(nSrcMaxX, pasEntries[iEntry].XMax);
            nSrcMaxY = std::max(nSrcMaxY, pasEntries[iEntry].YMax);
        }
    }

    const double dfSrcWidth = std::abs(static_cast<double>(nSrcMaxX) - nSrcMinX);
    const double dfSrcHeight = std::abs(static_cast<double>(nSrcMaxY) - nSrcMinY);

    // Calculate the separation for each pair (note that it may be negative
    // in case of overlap)
    // Normalize the separation by dividing by the extents of the
    // corresponding dimension
    const double dX =
        dfSrcWidth == 0.0 ? 0.0 : (static_cast<double>(nHighestMinX) - nLowestMaxX) / dfSrcWidth;
    const double dY =
        dfSrcHeight == 0.0 ? 0.0 : (static_cast<double>(nHighestMinY) - nLowestMaxY) / dfSrcHeight;

    // Choose the pair with the greatest normalized separation along
    // any dimension
    if (dX > dY)
    {
        nSeed1 = nHighestMinXId;
        nSeed2 = nLowestMaxXId;
    }
    else
    {
        nSeed1 = nHighestMinYId;
        nSeed2 = nLowestMaxYId;
    }

    // If nSeed1==nSeed2 then just pick any two (giving pref to current child)
    if (nSeed1 == nSeed2)
    {
        if (nSeed1 != nSrcCurChildIndex && nSrcCurChildIndex != -1)
            nSeed1 = nSrcCurChildIndex;
        else if (nSeed1 != 0)
            nSeed1 = 0;
        else
            nSeed1 = 1;
    }

    // Decide which of the two seeds best matches the new entry. That seed and
    // the new entry will stay in current node (new entry will be added by the
    // caller later). The other seed will go in the 2nd node
    const double dAreaDiff1 =
        ComputeAreaDiff(pasEntries[nSeed1].XMin,
                        pasEntries[nSeed1].YMin,
                        pasEntries[nSeed1].XMax,
                        pasEntries[nSeed1].YMax,
                        nNewEntryXMin, nNewEntryYMin,
                        nNewEntryXMax, nNewEntryYMax);

    const double dAreaDiff2 =
        ComputeAreaDiff(pasEntries[nSeed2].XMin,
                        pasEntries[nSeed2].YMin,
                        pasEntries[nSeed2].XMax,
                        pasEntries[nSeed2].YMax,
                        nNewEntryXMin, nNewEntryYMin,
                        nNewEntryXMax, nNewEntryYMax);

    /* Note that we want to keep this node's current child in here.
     * Since splitting happens only during an addentry() operation and
     * then both the current child and the New Entry should fit in the same
     * area.
     */
    if (nSeed1 != nSrcCurChildIndex &&
        (dAreaDiff1 > dAreaDiff2 || nSeed2 == nSrcCurChildIndex))
    {
        // Seed2 stays in this node, Seed1 moves to new node
        // ... swap Seed1 and Seed2 indices
        int nTmp = nSeed1;
        nSeed1 = nSeed2;
        nSeed2 = nTmp;
    }

    return 0;
}

/**********************************************************************
 *                   TABMAPIndexBlock::SplitNode()
 *
 * Split current Node, update the references in the parent node, etc.
 * Note that Root Nodes cannot be split using this method... SplitRootNode()
 * should be used instead.
 *
 * nNewEntry* are the coord. of the new entry that
 * will be added after the split.  The split is done so that the current
 * node will be the one in which the new object should be stored.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int     TABMAPIndexBlock::SplitNode(GInt32 nNewEntryXMin, GInt32 nNewEntryYMin,
                                    GInt32 nNewEntryXMax, GInt32 nNewEntryYMax)
{
    CPLAssert(m_poBlockManagerRef);

    /*-----------------------------------------------------------------
     * Create a 2nd node
     *----------------------------------------------------------------*/
    TABMAPIndexBlock *poNewNode = new TABMAPIndexBlock(m_eAccess);
    if (poNewNode->InitNewBlock(m_fp, m_nBlockSize,
                                m_poBlockManagerRef->AllocNewBlock("INDEX")) != 0)
    {
        return -1;
    }
    poNewNode->SetMAPBlockManagerRef(m_poBlockManagerRef);

    /*-----------------------------------------------------------------
     * Make a temporary copy of the entries in current node
     *----------------------------------------------------------------*/
    int nSrcEntries = m_numEntries;
    TABMAPIndexEntry *pasSrcEntries = static_cast<TABMAPIndexEntry*>(CPLMalloc(m_numEntries*sizeof(TABMAPIndexEntry)));
    memcpy(pasSrcEntries, &m_asEntries, m_numEntries*sizeof(TABMAPIndexEntry));

    int nSrcCurChildIndex = m_nCurChildIndex;

    /*-----------------------------------------------------------------
     * Pick Seeds for each node
     *----------------------------------------------------------------*/
    int nSeed1, nSeed2;
    PickSeedsForSplit(pasSrcEntries, nSrcEntries, nSrcCurChildIndex,
                      nNewEntryXMin, nNewEntryYMin,
                      nNewEntryXMax, nNewEntryYMax,
                      nSeed1, nSeed2);

    /*-----------------------------------------------------------------
     * Reset number of entries in this node and start moving new entries
     *----------------------------------------------------------------*/
    m_numEntries = 0;

    // Insert nSeed1 in this node
    InsertEntry(pasSrcEntries[nSeed1].XMin,
                pasSrcEntries[nSeed1].YMin,
                pasSrcEntries[nSeed1].XMax,
                pasSrcEntries[nSeed1].YMax,
                pasSrcEntries[nSeed1].nBlockPtr);

    // Move nSeed2 to 2nd node
    poNewNode->InsertEntry(pasSrcEntries[nSeed2].XMin,
                           pasSrcEntries[nSeed2].YMin,
                           pasSrcEntries[nSeed2].XMax,
                           pasSrcEntries[nSeed2].YMax,
                           pasSrcEntries[nSeed2].nBlockPtr);

    // Update cur child index if necessary
    if (nSeed1 == nSrcCurChildIndex)
        m_nCurChildIndex = m_numEntries-1;

    /*-----------------------------------------------------------------
     * Go through the rest of the entries and assign them to one
     * of the 2 nodes.
     *
     * Criteria is minimal area difference.
     * Resolve ties by adding the entry to the node with smaller total
     * area, then to the one with fewer entries, then to either.
     *----------------------------------------------------------------*/
    for(int iEntry=0; iEntry<nSrcEntries; iEntry++)
    {
        if (iEntry == nSeed1 || iEntry == nSeed2)
            continue;

        // If one of the two nodes is almost full then all remaining
        // entries should go to the other node
        // The entry corresponding to the current child also automatically
        // stays in this node.
        if (iEntry == nSrcCurChildIndex)
        {
            InsertEntry(pasSrcEntries[iEntry].XMin,
                        pasSrcEntries[iEntry].YMin,
                        pasSrcEntries[iEntry].XMax,
                        pasSrcEntries[iEntry].YMax,
                        pasSrcEntries[iEntry].nBlockPtr);

            // Update current child index
            m_nCurChildIndex = m_numEntries-1;

            continue;
        }
        else if (m_numEntries >= GetMaxEntries()-1)
        {
            poNewNode->InsertEntry(pasSrcEntries[iEntry].XMin,
                                   pasSrcEntries[iEntry].YMin,
                                   pasSrcEntries[iEntry].XMax,
                                   pasSrcEntries[iEntry].YMax,
                                   pasSrcEntries[iEntry].nBlockPtr);
            continue;
        }
        else if (poNewNode->GetNumEntries() >= GetMaxEntries()-1)
        {
            InsertEntry(pasSrcEntries[iEntry].XMin,
                        pasSrcEntries[iEntry].YMin,
                        pasSrcEntries[iEntry].XMax,
                        pasSrcEntries[iEntry].YMax,
                        pasSrcEntries[iEntry].nBlockPtr);
            continue;
        }

        // Decide which of the two nodes to put this entry in
        RecomputeMBR();
        const double dAreaDiff1 =
            ComputeAreaDiff(m_nMinX, m_nMinY, m_nMaxX, m_nMaxY,
                            pasSrcEntries[iEntry].XMin,
                            pasSrcEntries[iEntry].YMin,
                            pasSrcEntries[iEntry].XMax,
                            pasSrcEntries[iEntry].YMax);

        GInt32 nXMin2 = 0;
        GInt32 nYMin2 = 0;
        GInt32 nXMax2 = 0;
        GInt32 nYMax2 = 0;
        poNewNode->RecomputeMBR();
        poNewNode->GetMBR(nXMin2, nYMin2, nXMax2, nYMax2);
        const double dAreaDiff2 =
            ComputeAreaDiff(nXMin2, nYMin2, nXMax2, nYMax2,
                            pasSrcEntries[iEntry].XMin,
                            pasSrcEntries[iEntry].YMin,
                            pasSrcEntries[iEntry].XMax,
                            pasSrcEntries[iEntry].YMax);
        if( dAreaDiff1 < dAreaDiff2 )
        {
            // This entry stays in this node.
            InsertEntry(pasSrcEntries[iEntry].XMin,
                        pasSrcEntries[iEntry].YMin,
                        pasSrcEntries[iEntry].XMax,
                        pasSrcEntries[iEntry].YMax,
                        pasSrcEntries[iEntry].nBlockPtr);
        }
        else
        {
            // This entry goes to new node
            poNewNode->InsertEntry(pasSrcEntries[iEntry].XMin,
                                   pasSrcEntries[iEntry].YMin,
                                   pasSrcEntries[iEntry].XMax,
                                   pasSrcEntries[iEntry].YMax,
                                   pasSrcEntries[iEntry].nBlockPtr);
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

    CPLFree(pasSrcEntries);

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
int TABMAPIndexBlock::SplitRootNode(GInt32 nNewEntryXMin, GInt32 nNewEntryYMin,
                                    GInt32 nNewEntryXMax, GInt32 nNewEntryYMax)
{
    CPLAssert(m_poBlockManagerRef);
    CPLAssert(m_poParentRef == nullptr);

    /*-----------------------------------------------------------------
     * Since a root note cannot be split, we add a level of nodes
     * under it and we'll do the split at that level.
     *----------------------------------------------------------------*/
    TABMAPIndexBlock *poNewNode = new TABMAPIndexBlock(m_eAccess);

    if (poNewNode->InitNewBlock(m_fp, m_nBlockSize,
                                m_poBlockManagerRef->AllocNewBlock("INDEX")) != 0)
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
        m_poCurChild = nullptr;
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
    return m_poCurChild->SplitNode(nNewEntryXMin, nNewEntryYMin,
                                   nNewEntryXMax, nNewEntryYMax);
}

/**********************************************************************
 *                   TABMAPIndexBlock::RecomputeMBR()
 *
 * Recompute current block MBR, and update info in parent.
 **********************************************************************/
void TABMAPIndexBlock::RecomputeMBR()
{
    GInt32 nMinX, nMinY, nMaxX, nMaxY;

    nMinX = 1000000000;
    nMinY = 1000000000;
    nMaxX = -1000000000;
    nMaxY = -1000000000;

    for(int i=0; i<m_numEntries; i++)
    {
        if (m_asEntries[i].XMin < nMinX)
            nMinX = m_asEntries[i].XMin;
        if (m_asEntries[i].XMax > nMaxX)
            nMaxX = m_asEntries[i].XMax;

        if (m_asEntries[i].YMin < nMinY)
            nMinY = m_asEntries[i].YMin;
        if (m_asEntries[i].YMax > nMaxY)
            nMaxY = m_asEntries[i].YMax;
    }

    if (m_nMinX != nMinX ||
        m_nMinY != nMinY ||
        m_nMaxX != nMaxX ||
        m_nMaxY != nMaxY )
    {
        m_nMinX = nMinX;
        m_nMinY = nMinY;
        m_nMaxX = nMaxX;
        m_nMaxY = nMaxY;

        m_bModified = TRUE;

        if (m_poParentRef)
            m_poParentRef->UpdateCurChildMBR(m_nMinX, m_nMinY,
                                             m_nMaxX, m_nMaxY,
                                             GetNodeBlockPtr());
    }
}

/**********************************************************************
 *                   TABMAPIndexBlock::UpdateCurChildMBR()
 *
 * Update current child MBR info, and propagate info in parent.
 *
 * nBlockPtr is passed only to validate the consistency of the tree.
 **********************************************************************/
void TABMAPIndexBlock::UpdateCurChildMBR(GInt32 nXMin, GInt32 nYMin,
                                         GInt32 nXMax, GInt32 nYMax,
                                         CPL_UNUSED GInt32 nBlockPtr)
{
    CPLAssert(m_poCurChild);
    CPLAssert(m_asEntries[m_nCurChildIndex].nBlockPtr == nBlockPtr);

    if (m_asEntries[m_nCurChildIndex].XMin == nXMin &&
        m_asEntries[m_nCurChildIndex].YMin == nYMin &&
        m_asEntries[m_nCurChildIndex].XMax == nXMax &&
        m_asEntries[m_nCurChildIndex].YMax == nYMax)
    {
        return;  /* Nothing changed... nothing to do */
    }

    m_bModified = TRUE;

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
}

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
    if (fpOut == nullptr)
        fpOut = stdout;

    fprintf(fpOut, "----- TABMAPIndexBlock::Dump() -----\n");
    if (m_pabyBuf == nullptr)
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
