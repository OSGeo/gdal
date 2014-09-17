/**********************************************************************
 * $Id: mitab_indfile.cpp,v 1.14 2010-07-07 19:00:15 aboudreault Exp $
 *
 * Name:     mitab_indfile.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the TABINDFile class used to handle
 *           access to .IND file (table field indexes) attached to a .DAT file
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
 * $Log: mitab_indfile.cpp,v $
 * Revision 1.14  2010-07-07 19:00:15  aboudreault
 * Cleanup Win32 Compile Warnings (GDAL bug #2930)
 *
 * Revision 1.13  2008-01-29 20:46:32  dmorissette
 * Added support for v9 Time and DateTime fields (byg 1754)
 *
 * Revision 1.12  2007/12/11 03:43:03  dmorissette
 * Added reporting access mode to error message in TABINDFile::Open()
 * (GDAL changeset r12460, ticket 1620)
 *
 * Revision 1.11  2005/04/29 19:08:56  dmorissette
 * Produce an error if m_nSubtreeDepth > 255 when creating a .IND (OGR bug 839)
 *
 * Revision 1.10  2004/06/30 20:29:04  dmorissette
 * Fixed refs to old address danmo@videotron.ca
 *
 * Revision 1.9  2003/07/24 02:45:57  daniel
 * Fixed problem scanning node in TABINDNode::FindNext() - bug 2176, FW
 *
 * Revision 1.8  2001/05/01 03:38:23  daniel
 * Added update support (allows creating new index in existing IND files).
 *
 * Revision 1.7  2000/11/13 22:17:57  daniel
 * When a (child) node's first entry is replaced by InsertEntry() then make
 * sure that node's key is updated in its parent node.
 *
 * Revision 1.6  2000/03/01 00:32:00  daniel
 * Added support for float keys, and completed support for generating indexes
 *
 * Revision 1.5  2000/02/28 16:57:42  daniel
 * Added support for writing indexes
 *
 * Revision 1.4  2000/01/15 22:30:44  daniel
 * Switch to MIT/X-Consortium OpenSource license
 *
 * Revision 1.3  1999/12/14 05:52:05  daniel
 * Fixed compile error on Windows
 *
 * Revision 1.2  1999/12/14 02:19:42  daniel
 * Completed .IND support for simple TABViews
 *
 * Revision 1.1  1999/11/20 15:49:07  daniel
 * Initial version
 *
 **********************************************************************/

#include "mitab.h"
#include "mitab_utils.h"

#include <ctype.h>      /* toupper() */

/*=====================================================================
 *                      class TABINDFile
 *====================================================================*/

#define IND_MAGIC_COOKIE  24242424

/**********************************************************************
 *                   TABINDFile::TABINDFile()
 *
 * Constructor.
 **********************************************************************/
TABINDFile::TABINDFile()
{
    m_fp = NULL;
    m_pszFname = NULL;
    m_eAccessMode = TABRead;
    m_numIndexes = 0;
    m_papoIndexRootNodes = NULL;
    m_papbyKeyBuffers = NULL;
    m_oBlockManager.SetName("IND");
}

/**********************************************************************
 *                   TABINDFile::~TABINDFile()
 *
 * Destructor.
 **********************************************************************/
TABINDFile::~TABINDFile()
{
    Close();
}

/**********************************************************************
 *                   TABINDFile::Open()
 *
 * Open a .IND file, read the header and the root nodes for all the
 * field indexes, and be ready to search the indexes.
 *
 * If the filename that is passed in contains a .DAT extension then
 * the extension will be changed to .IND before trying to open the file.
 *
 * Note that we pass a pszAccess flag, but only read access is supported
 * for now (and there are no plans to support write.)
 *
 * Set bTestOpenNoError=TRUE to silently return -1 with no error message
 * if the file cannot be opened because it does not exist.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABINDFile::Open(const char *pszFname, const char *pszAccess,
                     GBool bTestOpenNoError /*=FALSE*/)
{
    int         nLen;

    if (m_fp)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed: object already contains an open file");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Validate access mode and make sure we use binary access.
     * Note that for write access, we actually need read/write access to
     * the file.
     *----------------------------------------------------------------*/
    if (EQUALN(pszAccess, "r", 1) && strchr(pszAccess, '+') != NULL)
    {
        m_eAccessMode = TABReadWrite;
        pszAccess = "rb+";
    }
    else if (EQUALN(pszAccess, "r", 1))
    {
        m_eAccessMode = TABRead;
        pszAccess = "rb";
    }
    else if (EQUALN(pszAccess, "w", 1))
    {
        m_eAccessMode = TABWrite;
        pszAccess = "wb+";
    }
    else
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed: access mode \"%s\" not supported", pszAccess);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Change .DAT (or .TAB) extension to .IND if necessary
     *----------------------------------------------------------------*/
    m_pszFname = CPLStrdup(pszFname);

    nLen = strlen(m_pszFname);
    if (nLen > 4 && !EQUAL(m_pszFname+nLen-4, ".IND") )
        strcpy(m_pszFname+nLen-4, ".ind");

#ifndef _WIN32
    TABAdjustFilenameExtension(m_pszFname);
#endif

    /*-----------------------------------------------------------------
     * Open file
     *----------------------------------------------------------------*/
    m_fp = VSIFOpenL(m_pszFname, pszAccess);

    if (m_fp == NULL)
    {
        if (!bTestOpenNoError)
            CPLError(CE_Failure, CPLE_FileIO,
                     "Open() failed for %s (%s)", m_pszFname, pszAccess);

        CPLFree(m_pszFname);
        m_pszFname = NULL;
        return -1;
    }

    /*-----------------------------------------------------------------
     * Reset block manager to allocate first block at byte 512, after header.
     *----------------------------------------------------------------*/
    m_oBlockManager.Reset();
    m_oBlockManager.AllocNewBlock();

    /*-----------------------------------------------------------------
     * Read access: Read the header block
     * This will also alloc and init the array of index root nodes.
     *----------------------------------------------------------------*/
    if ((m_eAccessMode == TABRead || m_eAccessMode == TABReadWrite) &&
        ReadHeader() != 0)
    {
        // Failed reading header... CPLError() has already been called
        Close();
        return -1;
    }

    /*-----------------------------------------------------------------
     * Write access: Init class members and write a dummy header block
     *----------------------------------------------------------------*/
    if (m_eAccessMode == TABWrite)
    {
        m_numIndexes = 0;

        if (WriteHeader() != 0)
        {
            // Failed writing header... CPLError() has already been called
            Close();
            return -1;
        }
    }

    return 0;
}

/**********************************************************************
 *                   TABINDFile::Close()
 *
 * Close current file, and release all memory used.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABINDFile::Close()
{
    if (m_fp == NULL)
        return 0;

    /*-----------------------------------------------------------------
     * In Write Mode, commit all indexes to the file
     *----------------------------------------------------------------*/
    if (m_eAccessMode == TABWrite || m_eAccessMode == TABReadWrite)
    {
        WriteHeader();

        for(int iIndex=0; iIndex<m_numIndexes; iIndex++)
        {
            if (m_papoIndexRootNodes &&
                m_papoIndexRootNodes[iIndex])
            {
                m_papoIndexRootNodes[iIndex]->CommitToFile();
            }
        }
    }

    /*-----------------------------------------------------------------
     * Free index nodes in memory
     *----------------------------------------------------------------*/
    for (int iIndex=0; iIndex<m_numIndexes; iIndex++)
    {
        if (m_papoIndexRootNodes && m_papoIndexRootNodes[iIndex])
            delete m_papoIndexRootNodes[iIndex];
        if (m_papbyKeyBuffers && m_papbyKeyBuffers[iIndex])
            CPLFree(m_papbyKeyBuffers[iIndex]);
    }
    CPLFree(m_papoIndexRootNodes);
    m_papoIndexRootNodes = NULL;
    CPLFree(m_papbyKeyBuffers);
    m_papbyKeyBuffers = NULL;
    m_numIndexes = 0;

    /*-----------------------------------------------------------------
     * Close file
     *----------------------------------------------------------------*/
    VSIFCloseL(m_fp);
    m_fp = NULL;

    CPLFree(m_pszFname);
    m_pszFname = NULL;

    return 0;
}


/**********************************************************************
 *                   TABINDFile::ReadHeader()
 *
 * (private method)
 * Read the header block and init all class members for read access.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABINDFile::ReadHeader()
{

    CPLAssert(m_fp);
    CPLAssert(m_eAccessMode == TABRead || m_eAccessMode == TABReadWrite);

    /*-----------------------------------------------------------------
     * In ReadWrite mode, we need to init BlockManager with file size
     *----------------------------------------------------------------*/
    VSIStatBufL  sStatBuf;
    if (m_eAccessMode == TABReadWrite && VSIStatL(m_pszFname, &sStatBuf) != -1)
    {
        m_oBlockManager.SetLastPtr((int)(((sStatBuf.st_size-1)/512)*512));
    }

    /*-----------------------------------------------------------------
     * Read the header block
     *----------------------------------------------------------------*/
    TABRawBinBlock *poHeaderBlock;
    poHeaderBlock = new TABRawBinBlock(m_eAccessMode, TRUE);
    if (poHeaderBlock->ReadFromFile(m_fp, 0, 512) != 0)
    {
        // CPLError() has already been called.
        delete poHeaderBlock;
        return -1;
    }

    poHeaderBlock->GotoByteInBlock(0);
    GUInt32 nMagicCookie = poHeaderBlock->ReadInt32();
    if (nMagicCookie != IND_MAGIC_COOKIE)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "%s: Invalid Magic Cookie: got %d, expected %d",
                 m_pszFname, nMagicCookie, IND_MAGIC_COOKIE);
        delete poHeaderBlock;
        return -1;
    }

    poHeaderBlock->GotoByteInBlock(12);
    m_numIndexes = poHeaderBlock->ReadInt16();
    if (m_numIndexes < 1 || m_numIndexes > 29)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Invalid number of indexes (%d) in file %s",
                 m_numIndexes, m_pszFname);
        delete poHeaderBlock;
        return -1;
    }

    /*-----------------------------------------------------------------
     * Alloc and init the array of index root nodes.
     *----------------------------------------------------------------*/
    m_papoIndexRootNodes = (TABINDNode**)CPLCalloc(m_numIndexes,
                                                   sizeof(TABINDNode*));

    m_papbyKeyBuffers = (GByte **)CPLCalloc(m_numIndexes, sizeof(GByte*));

    /* First index def. starts at byte 48 */
    poHeaderBlock->GotoByteInBlock(48);

    for(int iIndex=0; iIndex<m_numIndexes; iIndex++)
    {
        /*-------------------------------------------------------------
         * Read next index definition
         *------------------------------------------------------------*/
        GInt32 nRootNodePtr = poHeaderBlock->ReadInt32();
        poHeaderBlock->ReadInt16();   // skip... max. num of entries per node
        int nTreeDepth = poHeaderBlock->ReadByte();
        int nKeyLength = poHeaderBlock->ReadByte();
        poHeaderBlock->GotoByteRel(8); // skip next 8 bytes;

        /*-------------------------------------------------------------
         * And init root node for this index.
         * Note that if nRootNodePtr==0 then this means that the 
         * corresponding index does not exist (i.e. has been deleted?)
         * so we simply do not allocate the root node in this case.
         * An error will be produced if the user tries to access this index
         * later during execution.
         *------------------------------------------------------------*/
        if (nRootNodePtr > 0)
        {
            m_papoIndexRootNodes[iIndex] = new TABINDNode(m_eAccessMode);
            if (m_papoIndexRootNodes[iIndex]->InitNode(m_fp, nRootNodePtr,
                                                       nKeyLength, nTreeDepth,
                                                       FALSE,
                                                       &m_oBlockManager)!= 0)
            {
                // CPLError has already been called
                delete poHeaderBlock;
                return -1;
            }

            // Alloc a temporary key buffer for this index.
            // This buffer will be used by the BuildKey() method
            m_papbyKeyBuffers[iIndex] = (GByte *)CPLCalloc(nKeyLength+1, 
                                                           sizeof(GByte));
        }
        else
        {
            m_papoIndexRootNodes[iIndex] = NULL;
            m_papbyKeyBuffers[iIndex] = NULL;
        }
    }

    /*-----------------------------------------------------------------
     * OK, we won't need the header block any more... free it.
     *----------------------------------------------------------------*/
    delete poHeaderBlock;

    return 0;
}


/**********************************************************************
 *                   TABINDFile::WriteHeader()
 *
 * (private method)
 * Write the header block based on current index information.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABINDFile::WriteHeader()
{
    CPLAssert(m_fp);
    CPLAssert(m_eAccessMode == TABWrite || m_eAccessMode == TABReadWrite);

    /*-----------------------------------------------------------------
     * Write the 48 bytes of file header
     *----------------------------------------------------------------*/
    TABRawBinBlock *poHeaderBlock;
    poHeaderBlock = new TABRawBinBlock(m_eAccessMode, TRUE);
    poHeaderBlock->InitNewBlock(m_fp, 512, 0);

    poHeaderBlock->WriteInt32( IND_MAGIC_COOKIE );

    poHeaderBlock->WriteInt16( 100 );   // ???
    poHeaderBlock->WriteInt16( 512 );   // ???
    poHeaderBlock->WriteInt32( 0 );     // ???

    poHeaderBlock->WriteInt16( (GInt16)m_numIndexes );

    poHeaderBlock->WriteInt16( 0x15e7); // ???

    poHeaderBlock->WriteInt16( 10 );    // ???
    poHeaderBlock->WriteInt16( 0x611d); // ???

    poHeaderBlock->WriteZeros( 28 );

    /*-----------------------------------------------------------------
     * The first index definition starts at byte 48
     *----------------------------------------------------------------*/
    for(int iIndex=0; iIndex<m_numIndexes; iIndex++)
    {
        TABINDNode *poRootNode = m_papoIndexRootNodes[iIndex];

        if (poRootNode)
        {
            /*---------------------------------------------------------
             * Write next index definition
             *--------------------------------------------------------*/
            poHeaderBlock->WriteInt32(poRootNode->GetNodeBlockPtr());
            poHeaderBlock->WriteInt16((GInt16)poRootNode->GetMaxNumEntries());
            poHeaderBlock->WriteByte( (GByte)poRootNode->GetSubTreeDepth());
            poHeaderBlock->WriteByte( (GByte)poRootNode->GetKeyLength());

            poHeaderBlock->WriteZeros( 8 );

            /*---------------------------------------------------------
             * Look for overflow of the SubTreeDepth field (byte)
             *--------------------------------------------------------*/
            if (poRootNode->GetSubTreeDepth() > 255)
            {
                CPLError(CE_Failure, CPLE_AssertionFailed,
                         "Index no %d is too large and will not be useable. "
                         "(SubTreeDepth = %d, cannot exceed 255).",
                         iIndex+1, poRootNode->GetSubTreeDepth());
                return -1;
            }
        }
        else
        {
            /*---------------------------------------------------------
             * NULL Root Node: This index has likely been deleted
             *--------------------------------------------------------*/
            poHeaderBlock->WriteZeros( 16 );
        }
    }

    /*-----------------------------------------------------------------
     * OK, we won't need the header block any more... write and free it.
     *----------------------------------------------------------------*/
    if (poHeaderBlock->CommitToFile() != 0)
        return -1;

    delete poHeaderBlock;

    return 0;
}

/**********************************************************************
 *                   TABINDFile::ValidateIndexNo()
 *
 * Private method to validate the index no parameter of some methods...
 * returns 0 if index no. is OK, or produces an error ands returns -1
 * if index no is not valid. 
 **********************************************************************/
int TABINDFile::ValidateIndexNo(int nIndexNumber)
{
    if (m_fp == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABINDFile: File has not been opened yet!");
        return -1;
    }

    if (nIndexNumber < 1 || nIndexNumber > m_numIndexes ||
        m_papoIndexRootNodes == NULL || 
        m_papoIndexRootNodes[nIndexNumber-1] == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "No field index number %d in %s: Valid range is [1..%d].",
                 nIndexNumber, m_pszFname, m_numIndexes);
        return -1;
    }

    return 0;  // Index seems valid
}

/**********************************************************************
 *                   TABINDFile::SetIndexFieldType()
 *
 * Sets the field type for the specified index.
 * This information will then be used in building the key values, etc.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABINDFile::SetIndexFieldType(int nIndexNumber, TABFieldType eType)
{
    if (ValidateIndexNo(nIndexNumber) != 0)
        return -1;

    return m_papoIndexRootNodes[nIndexNumber-1]->SetFieldType(eType);
}

/**********************************************************************
 *                   TABINDFile::SetIndexUnique()
 *
 * Indicate that an index's keys are unique.  This allows for some 
 * optimization with read access.  By default, an index is treated as if
 * its keys could have duplicates.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABINDFile::SetIndexUnique(int nIndexNumber, GBool bUnique/*=TRUE*/)
{
    if (ValidateIndexNo(nIndexNumber) != 0)
        return -1;

    m_papoIndexRootNodes[nIndexNumber-1]->SetUnique(bUnique);

    return 0;
}

/**********************************************************************
 *                   TABINDFile::BuildKey()
 *
 * Encode a field value in the form required to be compared with index
 * keys in the specified index.
 * 
 * Note that index numbers are positive values starting at 1.
 *
 * Returns a reference to an internal buffer that is valid only until the
 * next call to BuildKey().  (should not be freed by the caller).
 * Returns NULL if field index is invalid.
 *
 * The first flavour of the function handles integer type of values, this
 * corresponds to MapInfo types: integer, smallint, logical and date
 **********************************************************************/
GByte *TABINDFile::BuildKey(int nIndexNumber, GInt32 nValue)
{
    if (ValidateIndexNo(nIndexNumber) != 0)
        return NULL;

    int nKeyLength = m_papoIndexRootNodes[nIndexNumber-1]->GetKeyLength();
    
    /*-----------------------------------------------------------------
     * Convert all int values to MSB using the right number of bytes
     * Note:
     * The most significant bit has to be unset for negative values,
     * and to be set for positive ones... that's the reverse of what it
     * should usually be.  Adding 0x80 to the MSB byte will do the job.
     *----------------------------------------------------------------*/
    switch(nKeyLength)
    {
      case 1:
        m_papbyKeyBuffers[nIndexNumber-1][0] = (GByte)(nValue & 0xff)+0x80;
        break;
      case 2:
        m_papbyKeyBuffers[nIndexNumber-1][0] = 
                                       (GByte)(nValue/0x100 & 0xff)+0x80;
        m_papbyKeyBuffers[nIndexNumber-1][1] = (GByte)(nValue & 0xff);
        break;
      case 4:
        m_papbyKeyBuffers[nIndexNumber-1][0] = 
                                       (GByte)(nValue/0x1000000 &0xff)+0x80;
        m_papbyKeyBuffers[nIndexNumber-1][1] = (GByte)(nValue/0x10000 & 0xff);
        m_papbyKeyBuffers[nIndexNumber-1][2] = (GByte)(nValue/0x100 &0xff);
        m_papbyKeyBuffers[nIndexNumber-1][3] = (GByte)(nValue & 0xff);
        break;
      default:
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "BuildKey(): %d bytes integer key length not supported",
                 nKeyLength);
        break;
    }

    return m_papbyKeyBuffers[nIndexNumber-1];
}

/**********************************************************************
 *                   TABINDFile::BuildKey()
 *
 * BuildKey() for string fields
 **********************************************************************/
GByte *TABINDFile::BuildKey(int nIndexNumber, const char *pszStr)
{
    if (ValidateIndexNo(nIndexNumber) != 0 || pszStr == NULL)
        return NULL;

    int nKeyLength = m_papoIndexRootNodes[nIndexNumber-1]->GetKeyLength();

    /*-----------------------------------------------------------------
     * Strings keys are all in uppercase, and padded with '\0'
     *----------------------------------------------------------------*/
    int i=0;
    for (i=0; i<nKeyLength && pszStr[i] != '\0'; i++)
    {
        m_papbyKeyBuffers[nIndexNumber-1][i] = (GByte)toupper(pszStr[i]);
    }

    /* Pad the end of the buffer with '\0' */
    for( ; i<nKeyLength; i++)
    {   
        m_papbyKeyBuffers[nIndexNumber-1][i] = '\0';
    }
        
    return m_papbyKeyBuffers[nIndexNumber-1];
}

/**********************************************************************
 *                   TABINDFile::BuildKey()
 *
 * BuildKey() for float and decimal fields
 **********************************************************************/
GByte *TABINDFile::BuildKey(int nIndexNumber, double dValue)
{
    if (ValidateIndexNo(nIndexNumber) != 0)
        return NULL;

    int nKeyLength = m_papoIndexRootNodes[nIndexNumber-1]->GetKeyLength();
    CPLAssert(nKeyLength == 8 && sizeof(double) == 8);

    /*-----------------------------------------------------------------
     * Convert double and decimal values... 
     * Reverse the sign of the value, and convert to MSB
     *----------------------------------------------------------------*/
    dValue = -dValue;

#ifndef CPL_MSB
    CPL_SWAPDOUBLE(&dValue);
#endif

    memcpy(m_papbyKeyBuffers[nIndexNumber-1], (GByte*)(&dValue), nKeyLength);

    return m_papbyKeyBuffers[nIndexNumber-1];
}


/**********************************************************************
 *                   TABINDFile::FindFirst()
 *
 * Search one of the indexes for a key value.  
 *
 * Note that index numbers are positive values starting at 1.
 *
 * Return value:
 *  - the key's corresponding record number in the .DAT file (greater than 0)
 *  - 0 if the key was not found
 *  - or -1 if an error happened
 **********************************************************************/
GInt32 TABINDFile::FindFirst(int nIndexNumber, GByte *pKeyValue)
{
    if (ValidateIndexNo(nIndexNumber) != 0)
        return -1;

    return m_papoIndexRootNodes[nIndexNumber-1]->FindFirst(pKeyValue);
}

/**********************************************************************
 *                   TABINDFile::FindNext()
 *
 * Continue the Search for pKeyValue previously initiated by FindFirst().  
 * NOTE: FindFirst() MUST have been previously called for this call to
 *       work...
 *
 * Note that index numbers are positive values starting at 1.
 *
 * Return value:
 *  - the key's corresponding record number in the .DAT file (greater than 0)
 *  - 0 if the key was not found
 *  - or -1 if an error happened
 **********************************************************************/
GInt32 TABINDFile::FindNext(int nIndexNumber, GByte *pKeyValue)
{
    if (ValidateIndexNo(nIndexNumber) != 0)
        return -1;

    return m_papoIndexRootNodes[nIndexNumber-1]->FindNext(pKeyValue);
}


/**********************************************************************
 *                   TABINDFile::CreateIndex()
 *
 * Create a new index with the specified field type and size.
 * Field size applies only to char field type... the other types have a
 * predefined key length.
 *
 * Key length is limited to 128 chars. char fields longer than 128 chars
 * will have their key truncated to 128 bytes.
 *
 * Note that a .IND file can contain only a maximum of 29 indexes.
 *
 * Returns the new field index on success (greater than 0), or -1 on error.
 **********************************************************************/
int TABINDFile::CreateIndex(TABFieldType eType, int nFieldSize)
{
    int i, nNewIndexNo = -1;

    if (m_fp == NULL || 
        (m_eAccessMode != TABWrite && m_eAccessMode != TABReadWrite))
        return -1;

    // __TODO__
    // We'll need more work in TABDATFile::WriteDateTimeField() before
    // we can support indexes on fields of type DateTime (see bug #1844)
    if (eType == TABFDateTime)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Index on fields of type DateTime not supported yet.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Look for an empty slot in the current array, if there is none
     * then extend the array.
     *----------------------------------------------------------------*/
    for(i=0; m_papoIndexRootNodes && i<m_numIndexes; i++)
    {
        if (m_papoIndexRootNodes[i] == NULL)
        {
            nNewIndexNo = i;
            break;
        }
    }

    if (nNewIndexNo == -1 && m_numIndexes >= 29)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot add new index to %s.  A dataset can contain only a "
                 "maximum of 29 indexes.", m_pszFname);
        return -1;
    }

    if (nNewIndexNo == -1)
    {
        /*-------------------------------------------------------------
         * Add a slot for new index at the end of the nodes array.
         *------------------------------------------------------------*/
        m_numIndexes++;
        m_papoIndexRootNodes = (TABINDNode**)CPLRealloc( m_papoIndexRootNodes,
                                                         m_numIndexes*
                                                         sizeof(TABINDNode*));

        m_papbyKeyBuffers = (GByte **)CPLRealloc(m_papbyKeyBuffers,
                                                 m_numIndexes*sizeof(GByte*));

        nNewIndexNo = m_numIndexes-1;
    }

    /*-----------------------------------------------------------------
     * Alloc and init new node
     * The call to InitNode() automatically allocates storage space for
     * the node in the file.
     * New nodes are created with a subtree_depth=1 since they start as
     * leaf nodes, i.e. their entries point directly to .DAT records
     *----------------------------------------------------------------*/
    int nKeyLength = ((eType == TABFInteger)  ? 4:
                      (eType == TABFSmallInt) ? 2:
                      (eType == TABFFloat)    ? 8:
                      (eType == TABFDecimal)  ? 8:
                      (eType == TABFDate)     ? 4:
                      (eType == TABFTime)     ? 4:
                      (eType == TABFDateTime) ? 8:
                      (eType == TABFLogical)  ? 4: MIN(128,nFieldSize));

    m_papoIndexRootNodes[nNewIndexNo] = new TABINDNode(m_eAccessMode);
    if (m_papoIndexRootNodes[nNewIndexNo]->InitNode(m_fp, 0, nKeyLength, 
                                                    1,  // subtree depth=1
                                                    FALSE, // not unique
                                                    &m_oBlockManager, 
                                                    NULL, 0, 0)!= 0)
    {
        // CPLError has already been called
        return -1;
    }

    // Alloc a temporary key buffer for this index.
    // This buffer will be used by the BuildKey() method
    m_papbyKeyBuffers[nNewIndexNo] = (GByte *)CPLCalloc(nKeyLength+1,
                                                        sizeof(GByte));

    // Return 1-based index number
    return nNewIndexNo+1;
}


/**********************************************************************
 *                   TABINDFile::AddEntry()
 *
 * Add an .DAT record entry for pKeyValue in the specified index.
 *
 * Note that index numbers are positive values starting at 1.
 * nRecordNo is the .DAT record number, record numbers start at 1.
 *
 * Returns 0 on success, -1 on error
 **********************************************************************/
int TABINDFile::AddEntry(int nIndexNumber, GByte *pKeyValue, GInt32 nRecordNo)
{
    if ((m_eAccessMode != TABWrite && m_eAccessMode != TABReadWrite) || 
        ValidateIndexNo(nIndexNumber) != 0)
        return -1;

    return m_papoIndexRootNodes[nIndexNumber-1]->AddEntry(pKeyValue,nRecordNo);
}


/**********************************************************************
 *                   TABINDFile::Dump()
 *
 * Dump block contents... available only in DEBUG mode.
 **********************************************************************/
#ifdef DEBUG

void TABINDFile::Dump(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "----- TABINDFile::Dump() -----\n");

    if (m_fp == NULL)
    {
        fprintf(fpOut, "File is not opened.\n");
    }
    else
    {
        fprintf(fpOut, "File is opened: %s\n", m_pszFname);
        fprintf(fpOut, "   m_numIndexes   = %d\n", m_numIndexes);
        for(int i=0; i<m_numIndexes && m_papoIndexRootNodes; i++)
        {
            if (m_papoIndexRootNodes[i])
            {
                fprintf(fpOut, "  ----- Index # %d -----\n", i+1);
                m_papoIndexRootNodes[i]->Dump(fpOut);
            }
        }

    }

    fflush(fpOut);
}

#endif // DEBUG





/*=====================================================================
 *                      class TABINDNode
 *====================================================================*/

/**********************************************************************
 *                   TABINDNode::TABINDNode()
 *
 * Constructor.
 **********************************************************************/
TABINDNode::TABINDNode(TABAccess eAccessMode /*=TABRead*/)
{
    m_fp = NULL;
    m_poCurChildNode = NULL;
    m_nSubTreeDepth = 0;
    m_nKeyLength = 0;
    m_eFieldType = TABFUnknown;
    m_poDataBlock = NULL;
    m_numEntriesInNode = 0;
    m_nCurIndexEntry = 0;
    m_nPrevNodePtr = 0;
    m_nNextNodePtr = 0;
    m_poBlockManagerRef = NULL;
    m_poParentNodeRef = NULL;
    m_bUnique = FALSE;

    m_eAccessMode = eAccessMode;
}

/**********************************************************************
 *                   TABINDNode::~TABINDNode()
 *
 * Destructor.
 **********************************************************************/
TABINDNode::~TABINDNode()
{
    if (m_poCurChildNode)
        delete m_poCurChildNode;

    if (m_poDataBlock)
        delete m_poDataBlock;
}

/**********************************************************************
 *                   TABINDNode::InitNode()
 *
 * Init a node... this function can be used either to initialize a new
 * node, or to make it point to a new data block in the file.
 *
 * By default, this call will read the data from the file at the
 * specified location if necessary, and leave the object ready to be searched.
 *
 * In write access, if the block does not exist (i.e. nBlockPtr=0) then a
 * new one is created and initialized.
 *
 * poParentNode is used in write access in order to update the parent node
 * when this node becomes full and has to be split.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABINDNode::InitNode(VSILFILE *fp, int nBlockPtr, 
                         int nKeyLength, int nSubTreeDepth, 
                         GBool bUnique,
                         TABBinBlockManager *poBlockMgr /*=NULL*/,
                         TABINDNode *poParentNode /*=NULL*/,
                         int nPrevNodePtr /*=0*/, int nNextNodePtr /*=0*/)
{
    /*-----------------------------------------------------------------
     * If the block already points to the right block, then don't do 
     * anything here.
     *----------------------------------------------------------------*/
    if (m_fp == fp && nBlockPtr> 0 && m_nCurDataBlockPtr == nBlockPtr)
        return 0;

    // Keep track of some info
    m_fp = fp;
    m_nKeyLength = nKeyLength;
    m_nSubTreeDepth = nSubTreeDepth;
    m_nCurDataBlockPtr = nBlockPtr;
    m_bUnique = bUnique;

    // Do not overwrite the following values if we receive NULL (the defaults)
    if (poBlockMgr)
        m_poBlockManagerRef = poBlockMgr;
    if (poParentNode)
        m_poParentNodeRef = poParentNode;

    // Set some defaults
    m_numEntriesInNode = 0;
    m_nPrevNodePtr = nPrevNodePtr;
    m_nNextNodePtr = nNextNodePtr;

    m_nCurIndexEntry = 0;

    /*-----------------------------------------------------------------
     * Init RawBinBlock
     * The node's buffer has to be created with read/write access since
     * the index is a very dynamic structure!
     *----------------------------------------------------------------*/
    if (m_poDataBlock == NULL)
        m_poDataBlock = new TABRawBinBlock(TABReadWrite, TRUE);

    if ((m_eAccessMode == TABWrite || m_eAccessMode == TABReadWrite) && 
        nBlockPtr == 0 && m_poBlockManagerRef)
    {
        /*-------------------------------------------------------------
         * Write access: Create and init a new block
         *------------------------------------------------------------*/
        m_nCurDataBlockPtr = m_poBlockManagerRef->AllocNewBlock();
        m_poDataBlock->InitNewBlock(m_fp, 512, m_nCurDataBlockPtr);

        m_poDataBlock->WriteInt32( m_numEntriesInNode );
        m_poDataBlock->WriteInt32( m_nPrevNodePtr );
        m_poDataBlock->WriteInt32( m_nNextNodePtr );
    }
    else
    {
        CPLAssert(m_nCurDataBlockPtr > 0);
        /*-------------------------------------------------------------
         * Read the data block from the file, applies to read access, or
         * to write access (to modify an existing block)
         *------------------------------------------------------------*/
        if (m_poDataBlock->ReadFromFile(m_fp, m_nCurDataBlockPtr, 512) != 0)
        {
            // CPLError() has already been called.
            return -1;
        }

        m_poDataBlock->GotoByteInBlock(0);
        m_numEntriesInNode = m_poDataBlock->ReadInt32();
        m_nPrevNodePtr = m_poDataBlock->ReadInt32();
        m_nNextNodePtr = m_poDataBlock->ReadInt32();
    }

    // m_poDataBlock is now positioned at the beginning of the key entries

    return 0;
}


/**********************************************************************
 *                   TABINDNode::GotoNodePtr()
 *
 * Move to the specified node ptr, and read the new node data from the file.
 *
 * This is just a cover funtion on top of InitNode()
 **********************************************************************/
int TABINDNode::GotoNodePtr(GInt32 nNewNodePtr)
{
    // First flush current changes if any.
    if ((m_eAccessMode == TABWrite || m_eAccessMode == TABReadWrite) && 
        m_poDataBlock && m_poDataBlock->CommitToFile() != 0)
        return -1;

    CPLAssert(nNewNodePtr % 512 == 0);

    // Then move to the requested location.
    return InitNode(m_fp, nNewNodePtr, m_nKeyLength, m_nSubTreeDepth, 
                    m_bUnique);
}

/**********************************************************************
 *                   TABINDNode::ReadIndexEntry()
 *
 * Read the key value and record/node ptr for the specified index entry
 * inside the current node data.
 *
 * nEntryNo is the 0-based index of the index entry that we are interested
 * in inside the current node.
 *
 * Returns the record/node ptr, and copies the key value inside the
 * buffer pointed to by *pKeyValue... this assumes that *pKeyValue points
 * to a buffer big enough to hold the key value (m_nKeyLength bytes).
 * If pKeyValue == NULL, then this parameter is ignored and the key value
 * is not copied.
 **********************************************************************/
GInt32 TABINDNode::ReadIndexEntry(int nEntryNo, GByte *pKeyValue)
{
    GInt32 nRecordPtr = 0;
    if (nEntryNo >= 0 && nEntryNo < m_numEntriesInNode)
    {
        if (pKeyValue)
        {
            m_poDataBlock->GotoByteInBlock(12 + nEntryNo*(m_nKeyLength+4));
            m_poDataBlock->ReadBytes(m_nKeyLength, pKeyValue);
        }
        else
        {
            m_poDataBlock->GotoByteInBlock(12 + nEntryNo*(m_nKeyLength+4)+
                                                                 m_nKeyLength);
        }

        nRecordPtr = m_poDataBlock->ReadInt32();
    }

    return nRecordPtr;
}

/**********************************************************************
 *                   TABINDNode::IndexKeyCmp()
 *
 * Compare the specified index entry with the key value, and 
 * return 0 if equal, an integer less than 0 if key is smaller than 
 * index entry, and an integer greater than 0 if key is bigger than 
 * index entry.
 *
 * nEntryNo is the 0-based index of the index entry that we are interested
 * in inside the current node.
 **********************************************************************/
int   TABINDNode::IndexKeyCmp(GByte *pKeyValue, int nEntryNo)
{
    CPLAssert(pKeyValue);
    CPLAssert(nEntryNo >= 0 && nEntryNo < m_numEntriesInNode);

    m_poDataBlock->GotoByteInBlock(12 + nEntryNo*(m_nKeyLength+4));

    return memcmp(pKeyValue, m_poDataBlock->GetCurDataPtr(), m_nKeyLength);
}

/**********************************************************************
 *                   TABINDNode::SetFieldType()
 *
 * Sets the field type for the current index and recursively set all 
 * children as well.
 * This information will then be used in building the key values, etc.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABINDNode::SetFieldType(TABFieldType eType)
{
    if (m_fp == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABINDNode::SetFieldType(): File has not been opened yet!");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Validate field type with key length
     *----------------------------------------------------------------*/
    if ((eType == TABFInteger && m_nKeyLength != 4) ||
        (eType == TABFSmallInt && m_nKeyLength != 2) ||
        (eType == TABFFloat && m_nKeyLength != 8) ||
        (eType == TABFDecimal && m_nKeyLength != 8) ||
        (eType == TABFDate && m_nKeyLength != 4) ||
        (eType == TABFTime && m_nKeyLength != 4) ||
        (eType == TABFDateTime && m_nKeyLength != 8) ||
        (eType == TABFLogical && m_nKeyLength != 4) )
    {
        CPLError(CE_Failure, CPLE_IllegalArg,
                 "Index key length (%d) does not match field type (%s).",
                 m_nKeyLength, TABFIELDTYPE_2_STRING(eType) );
        return -1;
    }           
    
    m_eFieldType = eType;

    /*-----------------------------------------------------------------
     * Pass the field type info to child nodes
     *----------------------------------------------------------------*/
    if (m_poCurChildNode)
        return m_poCurChildNode->SetFieldType(eType);

    return 0;
}

/**********************************************************************
 *                   TABINDNode::FindFirst()
 *
 * Start a new search in this node and its children for a key value.
 * If the index is not unique, then FindNext() can be used to return
 * the other values that correspond to the key.
 *
 * Return value:
 *  - the key's corresponding record number in the .DAT file (greater than 0)
 *  - 0 if the key was not found
 *  - or -1 if an error happened
 **********************************************************************/
GInt32 TABINDNode::FindFirst(GByte *pKeyValue)
{
    if (m_poDataBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABINDNode::Search(): Node has not been initialized yet!");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Unless something has been broken, this method will be called by our
     * parent node after it has established that we are the best candidate
     * to contain the first instance of the key value.  So there is no
     * need to look in the previous or next nodes in the chain... if the
     * value is not found in the current node block then it is not present
     * in the index at all.
     *
     * m_nCurIndexEntry will be used to keep track of the search pointer
     * when FindNext() will be used.
     *----------------------------------------------------------------*/
    m_nCurIndexEntry = 0;

    if (m_nSubTreeDepth == 1)
    {
        /*-------------------------------------------------------------
         * Leaf node level... we look for an exact match
         *------------------------------------------------------------*/
        while(m_nCurIndexEntry < m_numEntriesInNode)
        {
            int nCmpStatus = IndexKeyCmp(pKeyValue, m_nCurIndexEntry);
            if (nCmpStatus > 0)
            {
                /* Not there yet... (pKey > IndexEntry) */
                m_nCurIndexEntry++;
            }
            else if (nCmpStatus == 0)
            {
                /* Found it!  Return the record number */
                return ReadIndexEntry(m_nCurIndexEntry, NULL);
            }
            else
            {
                /* Item does not exist... return 0 */
                return 0;
            }
        }
    }
    else
    {
        /*-------------------------------------------------------------
         * Index Node: Find the child node that is the best candidate to
         * contain the value
         *
         * In the index tree at the node level, for each node entry inside
         * the parent node, the key value (in the parent) corresponds to 
         * the value of the first key that you will find when you access
         * the corresponding child node.
         *
         * This means that to find the child that contains the searched
         * key, we look for the first index key >= pKeyValue and the child
         * node that we are looking for is the one that precedes it.
         *
         * If the first key in the list is >= pKeyValue then this means
         * that the pKeyValue does not exist in our children and we just
         * return 0.  We do not bother searching the previous node at the
         * same level since this is the responsibility of our parent.
         *
         * The same way if the last indexkey in this node is < pKeyValue
         * we won't bother searching the next node since this should also
         * be taken care of by our parent.
         *------------------------------------------------------------*/
        while(m_nCurIndexEntry < m_numEntriesInNode)
        {
            int nCmpStatus = IndexKeyCmp(pKeyValue, m_nCurIndexEntry);

            if (nCmpStatus > 0 && m_nCurIndexEntry+1 < m_numEntriesInNode)
            {
                /* Not there yet... (pKey > IndexEntry) */
                m_nCurIndexEntry++;
            }
            else
            {
                /*-----------------------------------------------------
                 * We either found an indexkey >= pKeyValue or reached 
                 * the last entry in this node... still have to decide 
                 * what we're going to do... 
                 *----------------------------------------------------*/
                if (nCmpStatus < 0 && m_nCurIndexEntry == 0)
                {
                    /*-------------------------------------------------
                     * First indexkey in block is > pKeyValue...
                     * the key definitely does not exist in our children.
                     * However, we still want to drill down the rest of the
                     * tree because this function is also used when looking
                     * for a node to insert a new value.
                     *-------------------------------------------------*/
                    // Nothing special to do... just continue processing.
                }

                /*-----------------------------------------------------
                 * If we found an node for which pKeyValue < indexkey 
                 * (or pKeyValue <= indexkey for non-unique indexes) then 
                 * we access the preceding child node.
                 *
                 * Note that for indexkey == pKeyValue in non-unique indexes
                 * we also check in the preceding node because when keys
                 * are not unique then there are chances that the requested
                 * key could also be found at the end of the preceding node.
                 * In this case, if we don't find the key in the preceding
                 * node then we'll do a second search in the current node.
                 *----------------------------------------------------*/
                int numChildrenToVisit=1;
                if (m_nCurIndexEntry > 0 &&
                    (nCmpStatus < 0 || (nCmpStatus==0 && !m_bUnique)) )
                {
                    m_nCurIndexEntry--;
                    if (nCmpStatus == 0)
                        numChildrenToVisit = 2;
                }

                /*-----------------------------------------------------
                 * OK, now it's time to load/access the candidate child nodes.
                 *----------------------------------------------------*/
                int nRetValue = 0;
                for(int iChild=0; nRetValue==0 && 
                                  iChild<numChildrenToVisit; iChild++)
                {
                    // If we're doing a second pass then jump to next entry
                    if (iChild > 0)
                        m_nCurIndexEntry++;

                    int nChildNodePtr = ReadIndexEntry(m_nCurIndexEntry, NULL);
                    if (nChildNodePtr == 0)
                    {
                        /* Invalid child node??? */
                        nRetValue = 0;
                        continue;
                    }
                    else if (m_poCurChildNode == NULL)
                    {
                        /* Child node has never been initialized...do it now!*/

                        m_poCurChildNode = new TABINDNode(m_eAccessMode);
                        if ( m_poCurChildNode->InitNode(m_fp, nChildNodePtr, 
                                                        m_nKeyLength, 
                                                        m_nSubTreeDepth-1,
                                                        m_bUnique,
                                                        m_poBlockManagerRef, 
                                                        this) != 0 ||
                             m_poCurChildNode->SetFieldType(m_eFieldType)!=0)
                        {
                            // An error happened... and was already reported
                            return -1;
                        }
                    }

                    if (m_poCurChildNode->GotoNodePtr(nChildNodePtr) != 0)
                    {
                        // An error happened and has already been reported
                        return -1;
                    }

                    nRetValue = m_poCurChildNode->FindFirst(pKeyValue);
                }/*for iChild*/

                return nRetValue;

            }/*else*/

        }/*while numEntries*/

        // No node was found that contains the key value.
        // We should never get here... only leaf nodes should return 0
        CPLAssert(FALSE);
        return 0;
    }

    return 0;  // Not found
}

/**********************************************************************
 *                   TABINDNode::FindNext()
 *
 * Continue the search previously started by FindFirst() in this node 
 * and its children for a key value.
 *
 * Return value:
 *  - the key's corresponding record number in the .DAT file (greater than 0)
 *  - 0 if the key was not found
 *  - or -1 if an error happened
 **********************************************************************/
GInt32 TABINDNode::FindNext(GByte *pKeyValue)
{
    if (m_poDataBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "TABINDNode::Search(): Node has not been initialized yet!");
        return -1;
    }

    /*-----------------------------------------------------------------
     * m_nCurIndexEntry is the index of the last item that has been 
     * returned by FindFirst()/FindNext().
     *----------------------------------------------------------------*/

    if (m_nSubTreeDepth == 1)
    {
        /*-------------------------------------------------------------
         * Leaf node level... check if the next entry is an exact match
         *------------------------------------------------------------*/
        m_nCurIndexEntry++;
        if (m_nCurIndexEntry >= m_numEntriesInNode && m_nNextNodePtr > 0)
        {
            // We're at the end of a node ... continue with next node
            GotoNodePtr(m_nNextNodePtr);
            m_nCurIndexEntry = 0;
        }

        if (m_nCurIndexEntry < m_numEntriesInNode &&
            IndexKeyCmp(pKeyValue, m_nCurIndexEntry) == 0)
        {
           /* Found it!  Return the record number */
            return ReadIndexEntry(m_nCurIndexEntry, NULL);
        }
        else
        {
            /* No more items with that key... return 0 */
            return 0;
        }
    }
    else
    {
        /*-------------------------------------------------------------
         * Index Node: just pass the search to this child node.
         *------------------------------------------------------------*/
        while(m_nCurIndexEntry < m_numEntriesInNode)
        {
            if (m_poCurChildNode != NULL)
                return m_poCurChildNode->FindNext(pKeyValue);
        }
    }

    // No more nodes were found that contain the key value.
    return 0;
}


/**********************************************************************
 *                   TABINDNode::CommitToFile()
 *
 * For write access, write current block and its children to file.
 *
 * note: TABRawBinBlock::CommitToFile() does nothing unless the block has
 *       been modified.  (it has an internal bModified flag)
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABINDNode::CommitToFile()
{
    if ((m_eAccessMode != TABWrite && m_eAccessMode != TABReadWrite) || 
        m_poDataBlock == NULL)
        return -1;

    if (m_poCurChildNode)
    {
        if (m_poCurChildNode->CommitToFile() != 0)
            return -1;

        m_nSubTreeDepth = m_poCurChildNode->GetSubTreeDepth() + 1;
    }

    return m_poDataBlock->CommitToFile();
}

/**********************************************************************
 *                   TABINDNode::AddEntry()
 *
 * Add an .DAT record entry for pKeyValue in this index
 *
 * nRecordNo is the .DAT record number, record numbers start at 1.
 *
 * In order to insert a new value, the root node first does a FindFirst()
 * that will load the whole tree branch up to the insertion point.
 * Then AddEntry() is recursively called up to the leaf node level for
 * the insertion of the actual value.
 * If the leaf node is full then it will be split and if necessary the 
 * split will propagate up in the tree through the pointer that each node
 * has on its parent.
 *
 * If bAddInThisNodeOnly=TRUE, then the entry is added only locally and
 * we do not try to update the child node.  This is used when the parent 
 * of a node that is being splitted has to be updated.
 *
 * bInsertAfterCurChild forces the insertion to happen immediately after
 * the m_nCurIndexEntry.  This works only when bAddInThisNodeOnly=TRUE.
 * The default is to search the node for a an insertion point.
 *
 * Returns 0 on success, -1 on error
 **********************************************************************/
int TABINDNode::AddEntry(GByte *pKeyValue, GInt32 nRecordNo,
                         GBool bAddInThisNodeOnly /*=FALSE*/,
                         GBool bInsertAfterCurChild /*=FALSE*/,
                         GBool bMakeNewEntryCurChild /*=FALSE*/)
{
    if ((m_eAccessMode != TABWrite && m_eAccessMode != TABReadWrite) || 
        m_poDataBlock == NULL)
        return -1;

    /*-----------------------------------------------------------------
     * If I'm the root node, then do a FindFirst() to init all the nodes
     * and to make all of them point ot the insertion point.
     *----------------------------------------------------------------*/
    if (m_poParentNodeRef == NULL && !bAddInThisNodeOnly)
    {
        if (FindFirst(pKeyValue) < 0)
            return -1;  // Error happened and has already been reported.
    }

    if (m_poCurChildNode && !bAddInThisNodeOnly)
    {
        CPLAssert(m_nSubTreeDepth > 1);
        /*-------------------------------------------------------------
         * Propagate the call down to our children
         * Note: this recursive call could result in new levels of nodes
         * being added under our feet by SplitRootnode() so it is very 
         * important to return right after this call or we might not be 
         * able to recognize this node at the end of the call!
         *------------------------------------------------------------*/
        return m_poCurChildNode->AddEntry(pKeyValue, nRecordNo);
    }
    else
    {
        /*-------------------------------------------------------------
         * OK, we're a leaf node... this is where the real work happens!!!
         *------------------------------------------------------------*/
        CPLAssert(m_nSubTreeDepth == 1 || bAddInThisNodeOnly);

        /*-------------------------------------------------------------
         * First thing to do is make sure that there is room for a new
         * entry in this node, and to split it if necessary.
         *------------------------------------------------------------*/
        if (GetNumEntries() == GetMaxNumEntries())
        {
            if (m_poParentNodeRef == NULL)
            {
                /*-----------------------------------------------------
                 * Splitting the root node adds one level to the tree, so
                 * after splitting we just redirect the call to our child.
                 *----------------------------------------------------*/
                if (SplitRootNode() != 0)
                    return -1;  // Error happened and has already been reported

                CPLAssert(m_poCurChildNode);
                CPLAssert(m_nSubTreeDepth > 1);
                return m_poCurChildNode->AddEntry(pKeyValue, nRecordNo,
                                                  bAddInThisNodeOnly,
                                                  bInsertAfterCurChild,
                                                  bMakeNewEntryCurChild);
            }
            else
            {
                /*-----------------------------------------------------
                 * Splitting a regular node will leave it 50% full.
                 *----------------------------------------------------*/
                if (SplitNode() != 0)
                    return -1; 
            }
        }

        /*-------------------------------------------------------------
         * Insert new key/value at the right position in node.
         *------------------------------------------------------------*/
        if (InsertEntry(pKeyValue, nRecordNo, 
                        bInsertAfterCurChild, bMakeNewEntryCurChild) != 0)
            return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABINDNode::InsertEntry()
 *
 * (private method)
 *
 * Insert a key/value pair in the current node buffer.
 *
 * Returns 0 on success, -1 on error
 **********************************************************************/
int TABINDNode::InsertEntry(GByte *pKeyValue, GInt32 nRecordNo,
                            GBool bInsertAfterCurChild /*=FALSE*/,
                            GBool bMakeNewEntryCurChild /*=FALSE*/)
{
    int iInsertAt=0;

    if (GetNumEntries() >= GetMaxNumEntries())
    {   
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "Node is full!  Cannot insert key!");
        return -1;
    }

    /*-----------------------------------------------------------------
     * Find the spot where the key belongs
     *----------------------------------------------------------------*/
    if (bInsertAfterCurChild)
    {
        iInsertAt = m_nCurIndexEntry+1;
    }
    else
    {
        while(iInsertAt < m_numEntriesInNode)
        {
            int nCmpStatus = IndexKeyCmp(pKeyValue, iInsertAt);
            if (nCmpStatus <= 0)
            {
                break;
            }
            iInsertAt++;
        }
    }

    m_poDataBlock->GotoByteInBlock(12 + iInsertAt*(m_nKeyLength+4));

    /*-----------------------------------------------------------------
     * Shift all entries that follow in the array
     *----------------------------------------------------------------*/
    if (iInsertAt < m_numEntriesInNode)
    {
        // Since we use memmove() directly, we need to inform 
        // m_poDataBlock that the upper limit of the buffer will move
        m_poDataBlock->GotoByteInBlock(12 + (m_numEntriesInNode+1)*
                                                        (m_nKeyLength+4));
        m_poDataBlock->GotoByteInBlock(12 + iInsertAt*(m_nKeyLength+4));

        memmove(m_poDataBlock->GetCurDataPtr()+(m_nKeyLength+4),
                m_poDataBlock->GetCurDataPtr(),
                (m_numEntriesInNode-iInsertAt)*(m_nKeyLength+4));

    }

    /*-----------------------------------------------------------------
     * Write new entry
     *----------------------------------------------------------------*/
    m_poDataBlock->WriteBytes(m_nKeyLength, pKeyValue);
    m_poDataBlock->WriteInt32(nRecordNo);

    m_numEntriesInNode++;
    m_poDataBlock->GotoByteInBlock(0);
    m_poDataBlock->WriteInt32(m_numEntriesInNode);

    if (bMakeNewEntryCurChild)
        m_nCurIndexEntry = iInsertAt;
    else if (m_nCurIndexEntry >= iInsertAt)
        m_nCurIndexEntry++;

    /*-----------------------------------------------------------------
     * If we replaced the first entry in the node, then this node's key
     * changes and we have to update the reference in the parent node.
     *----------------------------------------------------------------*/
    if (iInsertAt == 0 && m_poParentNodeRef)
    {
        if (m_poParentNodeRef->UpdateCurChildEntry(GetNodeKey(),
                                                   GetNodeBlockPtr()) != 0)
            return -1;
    }

    return 0;
}


/**********************************************************************
 *                   TABINDNode::UpdateCurChildEntry()
 *
 * Update the key for the current child node.  This method is called by
 * the child when its first entry (defining its node key) is changed.
 *
 * Returns 0 on success, -1 on error
 **********************************************************************/
int TABINDNode::UpdateCurChildEntry(GByte *pKeyValue, GInt32 nRecordNo)
{

    /*-----------------------------------------------------------------
     * Update current child entry with the info for the first node.
     *
     * For some reason, the key for first entry of the first node of each
     * level has to be set to 0 except for the leaf level.
     *----------------------------------------------------------------*/
    m_poDataBlock->GotoByteInBlock(12 + m_nCurIndexEntry*(m_nKeyLength+4));

    if (m_nCurIndexEntry == 0 && m_nSubTreeDepth > 1 && m_nPrevNodePtr == 0)
    {
        m_poDataBlock->WriteZeros(m_nKeyLength);
    }
    else
    {
        m_poDataBlock->WriteBytes(m_nKeyLength, pKeyValue);
    }
    m_poDataBlock->WriteInt32(nRecordNo);

    return 0;
}



/**********************************************************************
 *                   TABINDNode::UpdateSplitChild()
 *
 * Update the key and/or record ptr information corresponding to the 
 * current child node.
 *
 * Returns 0 on success, -1 on error
 **********************************************************************/
int TABINDNode::UpdateSplitChild(GByte *pKeyValue1, GInt32 nRecordNo1,
                                 GByte *pKeyValue2, GInt32 nRecordNo2,
                                 int nNewCurChildNo /* 1 or 2 */)
{

    /*-----------------------------------------------------------------
     * Update current child entry with the info for the first node.
     *
     * For some reason, the key for first entry of the first node of each
     * level has to be set to 0 except for the leaf level.
     *----------------------------------------------------------------*/
    m_poDataBlock->GotoByteInBlock(12 + m_nCurIndexEntry*(m_nKeyLength+4));

    if (m_nCurIndexEntry == 0 && m_nSubTreeDepth > 1 && m_nPrevNodePtr == 0)
    {
        m_poDataBlock->WriteZeros(m_nKeyLength);
    }
    else
    {
        m_poDataBlock->WriteBytes(m_nKeyLength, pKeyValue1);
    }
    m_poDataBlock->WriteInt32(nRecordNo1);

    /*-----------------------------------------------------------------
     * Add an entry for the second node after the current one and ask 
     * AddEntry() to update m_nCurIndexEntry if the new node should 
     * become the new current child.
     *----------------------------------------------------------------*/
    if (AddEntry(pKeyValue2, nRecordNo2, 
                 TRUE, /* bInThisNodeOnly */
                 TRUE, /* bInsertAfterCurChild */
                 (nNewCurChildNo==2)) != 0)
    {
            return -1;
    }

    return 0;
}


/**********************************************************************
 *                   TABINDNode::SplitNode()
 *
 * (private method)
 *
 * Split a node, update the references in the parent node, etc.
 * Note that Root Nodes cannot be split using this method... SplitRootNode()
 * should be used instead.
 *
 * The node is split in a way that the current child stays inside this
 * node object, and a new node is created for the other half of the
 * entries.  This way, the object references in this node's parent and in its 
 * current child all remain valid.  The new node is not kept in memory, 
 * it is written to disk right away.
 *
 * Returns 0 on success, -1 on error
 **********************************************************************/
int TABINDNode::SplitNode()
{
    TABINDNode *poNewNode=NULL;
    int numInNode1, numInNode2;

    CPLAssert(m_numEntriesInNode >= 2);
    CPLAssert(m_poParentNodeRef);  // This func. does not work for root nodes

    /*-----------------------------------------------------------------
     * Prepare new node
     *----------------------------------------------------------------*/
    numInNode1 = (m_numEntriesInNode+1)/2;
    numInNode2 = m_numEntriesInNode - numInNode1;

    poNewNode = new TABINDNode(m_eAccessMode);

    if (m_nCurIndexEntry < numInNode1)
    {
        /*-------------------------------------------------------------
         * We will move the second half of the array to a new node.
         *------------------------------------------------------------*/
        if (poNewNode->InitNode(m_fp, 0, m_nKeyLength, 
                                m_nSubTreeDepth, m_bUnique, 
                                m_poBlockManagerRef, m_poParentNodeRef, 
                                GetNodeBlockPtr(), m_nNextNodePtr)!= 0 ||
            poNewNode->SetFieldType(m_eFieldType) != 0 )
        {
            return -1;
        }

        // We have to update m_nPrevNodePtr in the node that used to follow
        // the current node and will now follow the new node.
        if (m_nNextNodePtr)
        {
            TABINDNode *poTmpNode = new TABINDNode(m_eAccessMode);
            if (poTmpNode->InitNode(m_fp, m_nNextNodePtr, 
                                    m_nKeyLength, m_nSubTreeDepth,
                                    m_bUnique, m_poBlockManagerRef, 
                                    m_poParentNodeRef) != 0 ||
                poTmpNode->SetPrevNodePtr(poNewNode->GetNodeBlockPtr()) != 0 ||
                poTmpNode->CommitToFile() != 0)
            {
                return -1;
            }
            delete poTmpNode;
        }

        m_nNextNodePtr = poNewNode->GetNodeBlockPtr();

        // Move half the entries to the new block
        m_poDataBlock->GotoByteInBlock(12 + numInNode1*(m_nKeyLength+4));

        if (poNewNode->SetNodeBufferDirectly(numInNode2, 
                                        m_poDataBlock->GetCurDataPtr()) != 0)
            return -1;

#ifdef DEBUG
        // Just in case, reset space previously used by moved entries
        memset(m_poDataBlock->GetCurDataPtr(), 0, numInNode2*(m_nKeyLength+4));
#endif
        // And update current node members
        m_numEntriesInNode = numInNode1;

        // Update parent node with new children info
        if (m_poParentNodeRef)
        {
            if (m_poParentNodeRef->UpdateSplitChild(GetNodeKey(),
                                                    GetNodeBlockPtr(),
                                                    poNewNode->GetNodeKey(),
                                        poNewNode->GetNodeBlockPtr(), 1) != 0)
                return -1;
        }

    }
    else
    {
        /*-------------------------------------------------------------
         * We will move the first half of the array to a new node.
         *------------------------------------------------------------*/
        if (poNewNode->InitNode(m_fp, 0, m_nKeyLength, 
                                m_nSubTreeDepth, m_bUnique, 
                                m_poBlockManagerRef, m_poParentNodeRef, 
                                m_nPrevNodePtr, GetNodeBlockPtr())!= 0 ||
            poNewNode->SetFieldType(m_eFieldType) != 0 )
        {
            return -1;
        }

        // We have to update m_nNextNodePtr in the node that used to precede
        // the current node and will now precede the new node.
        if (m_nPrevNodePtr)
        {
            TABINDNode *poTmpNode = new TABINDNode(m_eAccessMode);
            if (poTmpNode->InitNode(m_fp, m_nPrevNodePtr, 
                                    m_nKeyLength, m_nSubTreeDepth,
                                    m_bUnique, m_poBlockManagerRef, 
                                    m_poParentNodeRef) != 0 ||
                poTmpNode->SetNextNodePtr(poNewNode->GetNodeBlockPtr()) != 0 ||
                poTmpNode->CommitToFile() != 0)
            {
                return -1;
            }
            delete poTmpNode;
        }

        m_nPrevNodePtr = poNewNode->GetNodeBlockPtr();

        // Move half the entries to the new block
        m_poDataBlock->GotoByteInBlock(12 + 0);

        if (poNewNode->SetNodeBufferDirectly(numInNode1, 
                                        m_poDataBlock->GetCurDataPtr()) != 0)
            return -1;

        // Shift the second half of the entries to beginning of buffer
        memmove (m_poDataBlock->GetCurDataPtr(),
                 m_poDataBlock->GetCurDataPtr()+numInNode1*(m_nKeyLength+4),
                 numInNode2*(m_nKeyLength+4));

#ifdef DEBUG
        // Just in case, reset space previously used by moved entries
        memset(m_poDataBlock->GetCurDataPtr()+numInNode2*(m_nKeyLength+4),
               0, numInNode1*(m_nKeyLength+4));
#endif

        // And update current node members
        m_numEntriesInNode = numInNode2;
        m_nCurIndexEntry -= numInNode1;

        // Update parent node with new children info
        if (m_poParentNodeRef)
        {
            if (m_poParentNodeRef->UpdateSplitChild(poNewNode->GetNodeKey(),
                                                  poNewNode->GetNodeBlockPtr(),
                                                    GetNodeKey(),
                                                    GetNodeBlockPtr(), 2) != 0)
                return -1;
        }

    }

    /*-----------------------------------------------------------------
     * Update current node header
     *----------------------------------------------------------------*/
    m_poDataBlock->GotoByteInBlock(0);
    m_poDataBlock->WriteInt32(m_numEntriesInNode);
    m_poDataBlock->WriteInt32(m_nPrevNodePtr);
    m_poDataBlock->WriteInt32(m_nNextNodePtr);

    /*-----------------------------------------------------------------
     * Flush and destroy temporary node
     *----------------------------------------------------------------*/
    if (poNewNode->CommitToFile() != 0)
        return -1;

    delete poNewNode;

    return 0;
}

/**********************************************************************
 *                   TABINDNode::SplitRootNode()
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
int TABINDNode::SplitRootNode()
{
    /*-----------------------------------------------------------------
     * Since a root note cannot be split, we add a level of nodes
     * under it and we'll do the split at that level.
     *----------------------------------------------------------------*/
    TABINDNode *poNewNode = new TABINDNode(m_eAccessMode);

    if (poNewNode->InitNode(m_fp, 0, m_nKeyLength, 
                            m_nSubTreeDepth, m_bUnique, m_poBlockManagerRef, 
                            this, 0, 0)!= 0 ||
        poNewNode->SetFieldType(m_eFieldType) != 0)
    {
        return -1;
    }

    // Move all entries to the new child
    m_poDataBlock->GotoByteInBlock(12 + 0);
    if (poNewNode->SetNodeBufferDirectly(m_numEntriesInNode, 
                                         m_poDataBlock->GetCurDataPtr(),
                                         m_nCurIndexEntry,
                                         m_poCurChildNode) != 0)
    {
        return -1;
    }

#ifdef DEBUG
    // Just in case, reset space previously used by moved entries
    memset(m_poDataBlock->GetCurDataPtr(), 0,
           m_numEntriesInNode*(m_nKeyLength+4));
#endif

    /*-----------------------------------------------------------------
     * Rewrite current node. (the new root node)
     *----------------------------------------------------------------*/
    m_numEntriesInNode = 0;
    m_nSubTreeDepth++;

    m_poDataBlock->GotoByteInBlock(0);
    m_poDataBlock->WriteInt32(m_numEntriesInNode);

    InsertEntry(poNewNode->GetNodeKey(), poNewNode->GetNodeBlockPtr());

    /*-----------------------------------------------------------------
     * Keep a reference to the new child
     *----------------------------------------------------------------*/
    m_poCurChildNode = poNewNode;
    m_nCurIndexEntry = 0;

    /*-----------------------------------------------------------------
     * And finally force the child to split itself
     *----------------------------------------------------------------*/
    return m_poCurChildNode->SplitNode();
}

/**********************************************************************
 *                   TABINDNode::SetNodeBufferDirectly()
 *
 * (private method)
 *
 * Set the key/value part of the nodes buffer and the pointers to the
 * current child direclty.  This is used when copying info to a new node
 * in SplitNode() and SplitRootNode()
 *
 * Returns 0 on success, -1 on error
 **********************************************************************/
int TABINDNode::SetNodeBufferDirectly(int numEntries, GByte *pBuf,
                                      int nCurIndexEntry/*=0*/, 
                                      TABINDNode *poCurChild/*=NULL*/)
{
    m_poDataBlock->GotoByteInBlock(0);
    m_poDataBlock->WriteInt32(numEntries);

    m_numEntriesInNode = numEntries;

    m_poDataBlock->GotoByteInBlock(12);
    if ( m_poDataBlock->WriteBytes(numEntries*(m_nKeyLength+4), pBuf) != 0)
    {
        return -1; // An error msg should have been reported already
    }

    m_nCurIndexEntry = nCurIndexEntry;
    m_poCurChildNode = poCurChild;
    if (m_poCurChildNode)
        m_poCurChildNode->m_poParentNodeRef = this;

    return 0;
}

/**********************************************************************
 *                   TABINDNode::GetNodeKey()
 *
 * Returns a reference to the key for the first entry in the node, which
 * is also the key for this node at the level above it in the tree.
 *
 * Returns NULL if node is empty.
 **********************************************************************/
GByte* TABINDNode::GetNodeKey()
{
    if (m_poDataBlock == NULL || m_numEntriesInNode == 0)
        return NULL;

    m_poDataBlock->GotoByteInBlock(12);

    return m_poDataBlock->GetCurDataPtr();
}

/**********************************************************************
 *                   TABINDNode::SetPrevNodePtr()
 *
 * Update the m_nPrevNodePtr member.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABINDNode::SetPrevNodePtr(GInt32 nPrevNodePtr)
{
    if ((m_eAccessMode != TABWrite && m_eAccessMode != TABReadWrite) ||
        m_poDataBlock == NULL)
        return -1;

    if (m_nPrevNodePtr == nPrevNodePtr)
        return 0;  // Nothing to do.

    m_poDataBlock->GotoByteInBlock(4);
    return m_poDataBlock->WriteInt32(nPrevNodePtr);
}

/**********************************************************************
 *                   TABINDNode::SetNextNodePtr()
 *
 * Update the m_nNextNodePtr member.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABINDNode::SetNextNodePtr(GInt32 nNextNodePtr)
{
    if ((m_eAccessMode != TABWrite && m_eAccessMode != TABReadWrite) ||
        m_poDataBlock == NULL)
        return -1;

    if (m_nNextNodePtr == nNextNodePtr)
        return 0;  // Nothing to do.

    m_poDataBlock->GotoByteInBlock(8);
    return m_poDataBlock->WriteInt32(nNextNodePtr);
}



/**********************************************************************
 *                   TABINDNode::Dump()
 *
 * Dump block contents... available only in DEBUG mode.
 **********************************************************************/
#ifdef DEBUG

void TABINDNode::Dump(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "----- TABINDNode::Dump() -----\n");

    if (m_fp == NULL)
    {
        fprintf(fpOut, "Node is not initialized.\n");
    }
    else
    {
        fprintf(fpOut, "   m_numEntriesInNode   = %d\n", m_numEntriesInNode);
        fprintf(fpOut, "   m_nCurDataBlockPtr   = %d\n", m_nCurDataBlockPtr);
        fprintf(fpOut, "   m_nPrevNodePtr       = %d\n", m_nPrevNodePtr);
        fprintf(fpOut, "   m_nNextNodePtr       = %d\n", m_nNextNodePtr);
        fprintf(fpOut, "   m_nSubTreeDepth      = %d\n", m_nSubTreeDepth);
        fprintf(fpOut, "   m_nKeyLength         = %d\n", m_nKeyLength);
        fprintf(fpOut, "   m_eFieldtype         = %s\n", 
                                        TABFIELDTYPE_2_STRING(m_eFieldType) );
        if (m_nSubTreeDepth > 0)
        {
            GByte  aKeyValBuf[255];
            GInt32 nRecordPtr, nValue;
            TABINDNode oChildNode;

            if (m_nKeyLength > 254)
            {
                CPLError(CE_Failure, CPLE_NotSupported,
                         "Dump() cannot handle keys longer than 254 chars.");
                return;
            }

            fprintf(fpOut, "\n");
            for (int i=0; i<m_numEntriesInNode; i++)
            {
              if (m_nSubTreeDepth > 1)
              {
                fprintf(fpOut, "   >>>> Child %d of %d <<<<<\n", i, 
                                                         m_numEntriesInNode);
              }
              else
              {
                fprintf(fpOut, "   >>>> Record (leaf) %d of %d <<<<<\n", i, 
                                                         m_numEntriesInNode);
              }

              if (m_eFieldType == TABFChar)
              {
                  nRecordPtr = ReadIndexEntry(i, aKeyValBuf);
                  fprintf(fpOut, "   nRecordPtr = %d\n", nRecordPtr);
                  fprintf(fpOut, "   Char Val= \"%s\"\n", (char*)aKeyValBuf);
              }
              else if (m_nKeyLength != 4)
              {
                nRecordPtr = ReadIndexEntry(i, aKeyValBuf);
                fprintf(fpOut, "   nRecordPtr = %d\n", nRecordPtr);
                fprintf(fpOut, "   Int Value = %d\n", *(GInt32*)aKeyValBuf);
                fprintf(fpOut, "   Int16 Val= %d\n",*(GInt16*)(aKeyValBuf+2));
                fprintf(fpOut, "   Hex Val= 0x%8.8x\n",*(GUInt32*)aKeyValBuf);
              }
              else
              {
                nRecordPtr = ReadIndexEntry(i, (GByte*)&nValue);
                fprintf(fpOut, "   nRecordPtr = %d\n", nRecordPtr);
                fprintf(fpOut, "   Int Value = %d\n", nValue);
                fprintf(fpOut, "   Hex Value = 0x%8.8x\n",nValue);
              }

              if (m_nSubTreeDepth > 1)
              {
                oChildNode.InitNode(m_fp, nRecordPtr, m_nKeyLength, 
                                    m_nSubTreeDepth - 1, FALSE);
                oChildNode.SetFieldType(m_eFieldType);
                oChildNode.Dump(fpOut);
              }
            }
        }
    }

    fflush(fpOut);
}

#endif // DEBUG
