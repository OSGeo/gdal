/**********************************************************************
 * $Id: mitab_indfile.cpp,v 1.4 2000/01/15 22:30:44 daniel Exp $
 *
 * Name:     mitab_indfile.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the TABINDFile class used to handle
 *           read-only access to .IND file (table field indexes) 
 *           attached to a .DAT file
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
 * $Log: mitab_indfile.cpp,v $
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

#define IND_MAGIC_COOKIE  0xe8f8

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
     * Note that we support only read access.
     *----------------------------------------------------------------*/
    if (EQUALN(pszAccess, "r", 1))
    {
        m_eAccessMode = TABRead;
        pszAccess = "rb";
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
        strcpy(m_pszFname+nLen-4, ".IND");

#ifndef _WIN32
    TABAdjustFilenameExtension(m_pszFname);
#endif

    /*-----------------------------------------------------------------
     * Open file
     *----------------------------------------------------------------*/
    m_fp = VSIFOpen(m_pszFname, pszAccess);

    if (m_fp == NULL)
    {
        if (!bTestOpenNoError)
            CPLError(CE_Failure, CPLE_FileIO,
                     "Open() failed for %s", m_pszFname);

        CPLFree(m_pszFname);
        m_pszFname = NULL;
        return -1;
    }

    /*-----------------------------------------------------------------
     * Read the header block
     *----------------------------------------------------------------*/
    TABRawBinBlock *poHeaderBlock;
    poHeaderBlock = new TABRawBinBlock(m_eAccessMode, TRUE);
    if (poHeaderBlock->ReadFromFile(m_fp, 0, 512) != 0)
    {
        // CPLError() has already been called.
        Close();
        return -1;
    }

    poHeaderBlock->GotoByteInBlock(0);
    GUInt16 nMagicCookie = poHeaderBlock->ReadInt16();
    if (nMagicCookie != IND_MAGIC_COOKIE)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "%s: Invalid Magic Cookie: got 0x%04.4x expected 0x%04.4x",
                 m_pszFname, nMagicCookie, IND_MAGIC_COOKIE);
        delete poHeaderBlock;
        Close();
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
        Close();
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
            m_papoIndexRootNodes[iIndex] = new TABINDNode;
            if (m_papoIndexRootNodes[iIndex]->InitNode(m_fp, nRootNodePtr,
                                                   nKeyLength, nTreeDepth)!= 0)
            {
                // CPLError has already been called
                delete poHeaderBlock;
                Close();
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

    // Delete array of indexes
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

    // Close file
    VSIFClose(m_fp);
    m_fp = NULL;

    CPLFree(m_pszFname);
    m_pszFname = NULL;

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
     * Convert all int values to MSB usingthe right number of bytes
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

    /*-----------------------------------------------------------------
     * Convert double and decimal values... not clear yet!!!!
     *----------------------------------------------------------------*/
    // __TODO__
    // Still need to get some sample files to find out the way floating
    // point keys are encoded.

    CPLError(CE_Failure, CPLE_NotSupported,
             "BuildKey(): index access for fields of type FLOAT and DECIMAL "
             "is not supported yet.");

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
TABINDNode::TABINDNode()
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

    m_eAccessMode = TABRead;
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
 * This call will read the data from the file at the specified location
 * if necessary, and leave the object ready to be searched.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABINDNode::InitNode(FILE *fp, int nBlockPtr, 
                         int nKeyLength, int nSubTreeDepth)
{
    /*-----------------------------------------------------------------
     * If the block already points to the right block, then don't do 
     * anything here.
     *----------------------------------------------------------------*/
    if (m_fp == fp && m_nCurDataBlockPtr == nBlockPtr)
        return 0;

    // Keep track of some info
    m_fp = fp;
    m_nKeyLength = nKeyLength;
    m_nSubTreeDepth = nSubTreeDepth;
    m_nCurDataBlockPtr = nBlockPtr;

    m_nCurIndexEntry = 0;

    /*-----------------------------------------------------------------
     * Read the data block from the file
     *----------------------------------------------------------------*/
    if (m_poDataBlock == NULL)
        m_poDataBlock = new TABRawBinBlock(m_eAccessMode, TRUE);

    if (m_poDataBlock->ReadFromFile(m_fp, m_nCurDataBlockPtr, 512) != 0)
    {
        // CPLError() has already been called.
        return -1;
    }

    m_poDataBlock->GotoByteInBlock(0);
    m_numEntriesInNode = m_poDataBlock->ReadInt32();
    m_nPrevNodePtr = m_poDataBlock->ReadInt32();
    m_nNextNodePtr = m_poDataBlock->ReadInt32();

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
    return InitNode(m_fp, nNewNodePtr, m_nKeyLength, m_nSubTreeDepth);
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
         * that the pKeyValue does not exist in our children... but this
         * should never happen since this method is always called from 
         * a parent node that should have checked that we contain the key
         * before calling us!
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
                     *-------------------------------------------------*/
                    return 0;
                }

                /*-----------------------------------------------------
                 * If we found an node for which pKeyValue <= indexkey 
                 * then we access the preceding child node.
                 * Note that this implies that for indexkey == pKeyValue
                 * we access the node corresponding to that indexkey
                 * by default.
                 *----------------------------------------------------*/
                if (nCmpStatus < 0)
                    m_nCurIndexEntry--;

                /*-----------------------------------------------------
                 * OK, now it's time to load/access that child node.
                 *----------------------------------------------------*/
                int nChildNodePtr = ReadIndexEntry(m_nCurIndexEntry, NULL);
                if (nChildNodePtr == 0)
                {
                    /* Invalid child node??? */
                    return 0;
                }
                else if (m_poCurChildNode == NULL)
                {
                    /* Child node has never been initialized... do it now! */

                    m_poCurChildNode = new TABINDNode;
                    if ( m_poCurChildNode->InitNode(m_fp, nChildNodePtr, 
                                                    m_nKeyLength, 
                                                    m_nSubTreeDepth-1) != 0 ||
                         m_poCurChildNode->SetFieldType(m_eFieldType) != 0)
                    {
                        // An error happened... and has already been reported
                        return -1;
                    }
                }

                if (m_poCurChildNode->GotoNodePtr(nChildNodePtr) != 0)
                {
                    // An error happened and has already been reported
                    return -1;
                }

                return m_poCurChildNode->FindFirst(pKeyValue);
            }
        }

        // No node was found that contains the key value.
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

        if (m_nCurIndexEntry >= m_numEntriesInNode &&
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
        fprintf(fpOut, "   m_nFieldtype         = %s\n", 
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

              if (m_nKeyLength != 4)
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
                                    m_nSubTreeDepth - 1);
                oChildNode.Dump(fpOut);
              }
            }
        }
    }

    fflush(fpOut);
}

#endif // DEBUG
