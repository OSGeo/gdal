/**********************************************************************
 * $Id: mitab_mapindexblock.cpp,v 1.5 2000/01/15 22:30:44 daniel Exp $
 *
 * Name:     mitab_mapindexblock.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the TABMAPIndexBlock class used to handle
 *           reading/writing of the .MAP files' index blocks
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
 * $Log: mitab_mapindexblock.cpp,v $
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
    m_papsEntries = NULL;
    m_numEntries = 0;

    m_nMinX = 1000000000;
    m_nMinY = 1000000000;
    m_nMaxX = -1000000000;
    m_nMaxY = -1000000000;
}

/**********************************************************************
 *                   TABMAPIndexBlock::~TABMAPIndexBlock()
 *
 * Destructor.
 **********************************************************************/
TABMAPIndexBlock::~TABMAPIndexBlock()
{
    int i;

    // Note that in read mode, m_numEntries is set but m_papsEntries is not
    // used... be careful about that!

    for(i=0; m_papsEntries && i<m_numEntries; i++)
    {
        if (m_papsEntries[i]->poBlock)
            delete m_papsEntries[i]->poBlock;
        CPLFree(m_papsEntries[i]);
    }
    CPLFree(m_papsEntries);

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
        if (m_papsEntries[i]->poBlock)
        {
            nStatus = m_papsEntries[i]->poBlock->CommitToFile();
        }

        if (nStatus == 0)
            nStatus = WriteNextEntry(m_papsEntries[i]);
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
    if (m_papsEntries)
        return -1;

    if (m_numEntries == 0)
        return 0;
    
    if (GotoByteInBlock( 0x004 ) != 0)
        return -1;

    /*-----------------------------------------------------------------
     * Alloc array of index entries... and read all entries
     *----------------------------------------------------------------*/
    m_papsEntries = (TABMAPIndexEntry**)CPLCalloc(m_numEntries,
                                                  sizeof(TABMAPIndexEntry*));

    for(int i=0; i<m_numEntries; i++)
    {
        m_papsEntries[i]=
                   (TABMAPIndexEntry*)CPLCalloc(1,sizeof(TABMAPIndexEntry));

        if ( ReadNextEntry(m_papsEntries[i]) != 0)
            return -1;

        m_papsEntries[i]->poBlock = NULL;
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
    int nMaxEntries = (m_nBlockSize-4)/20;

    return (nMaxEntries - m_numEntries);
}

/**********************************************************************
 *                   TABMAPIndexBlock::GetMaxDepth()
 *
 * Return maximum depth in the current index tree
 **********************************************************************/
int     TABMAPIndexBlock::GetMaxDepth()
{
    int nMaxDepth = 0, nDepth, i;

    for(i=0; i<m_numEntries; i++)
    {
        if (m_papsEntries[i]->poBlock)
        {
            nDepth = m_papsEntries[i]->poBlock->GetMaxDepth();
            nMaxDepth = MAX(nDepth, nMaxDepth);
        }
    }

    return nMaxDepth+1;
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
 *                   TABMAPIndexBlock::AddEntry()
 *
 * Add an entry to this index block.  
 *
 * For TABMAPObjectBlock entries, pass poBlock=NULL.
 *
 * For TABMAPIndexBlock entries, pass a non-NULL value for poBlock and this
 * index block will become a child of this object.  This means that it will
 * be maintained and later committed to disk, etc.
 *
 * __TODO__ This function could eventually be improved to add entries to
 *          children leaves as well.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int     TABMAPIndexBlock::AddEntry(GInt32 nXMin, GInt32 nYMin,
                                   GInt32 nXMax, GInt32 nYMax,
                                   GInt32 nBlockPtr, 
                                   TABMAPIndexBlock *poBlock /*=NULL*/)
{
    int i;

    if (m_eAccess != TABWrite)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
               "Failed adding index entry: File not opened for write access.");
        return -1;
    }

    if (GetNumFreeEntries() < 1)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Current Block Index is full, cannot add new entry.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Realloc array of index entries...
     *----------------------------------------------------------------*/
    m_numEntries++;
    i = m_numEntries-1;

    m_papsEntries = (TABMAPIndexEntry**)CPLRealloc(m_papsEntries,
                                       m_numEntries*sizeof(TABMAPIndexEntry*));
    m_papsEntries[i]=(TABMAPIndexEntry*)CPLCalloc(1,sizeof(TABMAPIndexEntry));

    /*-----------------------------------------------------------------
     * ... and store new entry.
     *----------------------------------------------------------------*/
    m_papsEntries[i]->XMin = nXMin;
    m_papsEntries[i]->YMin = nYMin;
    m_papsEntries[i]->XMax = nXMax;
    m_papsEntries[i]->YMax = nYMax;
    m_papsEntries[i]->nBlockPtr = nBlockPtr;
    m_papsEntries[i]->poBlock = poBlock;

    /*-----------------------------------------------------------------
     * Update MBR
     *----------------------------------------------------------------*/
    if (nXMin < m_nMinX)
        m_nMinX = nXMin;
    if (nXMax > m_nMaxX)
        m_nMaxX = nXMax;
    
    if (nYMin < m_nMinY)
        m_nMinY = nYMin;
    if (nYMax > m_nMaxY)
        m_nMaxY = nYMax;

    return 0;
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
        if (m_numEntries > 0 && m_papsEntries == NULL)
            ReadAllEntries();

        for(int i=0; m_papsEntries && i<m_numEntries; i++)
        {
            if (m_papsEntries[i])
            {
                fprintf(fpOut, "    %6d -> (%d, %d) - (%d, %d)\n",
                        m_papsEntries[i]->nBlockPtr,
                        m_papsEntries[i]->XMin, m_papsEntries[i]->YMin,
                        m_papsEntries[i]->XMax, m_papsEntries[i]->YMax );
            }
        }

    }

    fflush(fpOut);
}

#endif // DEBUG
