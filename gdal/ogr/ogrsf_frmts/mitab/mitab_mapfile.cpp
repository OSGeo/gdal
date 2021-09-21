/**********************************************************************
 *
 * Name:     mitab_mapfile.cpp
 * Project:  MapInfo TAB Read/Write library
 * Language: C++
 * Purpose:  Implementation of the TABMAPFile class used to handle
 *           reading/writing of the .MAP files at the MapInfo object level
 * Author:   Daniel Morissette, dmorissette@dmsolutions.ca
 *
 **********************************************************************
 * Copyright (c) 1999-2002, Daniel Morissette
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

#include <cstddef>
#if HAVE_FCNTL_H
#  include <fcntl.h>
#endif
#include <algorithm>
#include <utility>

#include "cpl_conv.h"
#include "cpl_error.h"
#include "cpl_vsi.h"
#include "mitab_priv.h"
#include "ogr_feature.h"

CPL_CVSID("$Id$")

/*=====================================================================
 *                      class TABMAPFile
 *====================================================================*/

/**********************************************************************
 *                   TABMAPFile::TABMAPFile()
 *
 * Constructor.
 **********************************************************************/
TABMAPFile::TABMAPFile(const char* pszEncoding) :
    m_nMinTABVersion(300),
    m_pszFname(nullptr),
    m_fp(nullptr),
    m_eAccessMode(TABRead),
    m_poHeader(nullptr),
    m_poSpIndex(nullptr),
    // See bug 1732: Optimized spatial index produces broken files because of
    // the way CoordBlocks are split. For now we have to force using the quick
    // (old) spatial index mode by default until bug 1732 is fixed.
    m_bQuickSpatialIndexMode(TRUE),
    m_poIdIndex(nullptr),
    m_poCurObjBlock(nullptr),
    m_nCurObjPtr(-1),
    m_nCurObjType(TAB_GEOM_UNSET),
    m_nCurObjId(-1),
    m_poCurCoordBlock(nullptr),
    m_poToolDefTable(nullptr),
    m_XMinFilter(0),
    m_YMinFilter(0),
    m_XMaxFilter(0),
    m_YMaxFilter(0),
    m_bUpdated(FALSE),
    m_bLastOpWasRead(FALSE),
    m_bLastOpWasWrite(FALSE),
    m_poSpIndexLeaf(nullptr),
    m_osEncoding(pszEncoding)
{
    m_sMinFilter.x = 0;
    m_sMinFilter.y = 0;
    m_sMaxFilter.x = 0;
    m_sMaxFilter.y = 0;

    m_oBlockManager.SetName("MAP");
}

/**********************************************************************
 *                   TABMAPFile::~TABMAPFile()
 *
 * Destructor.
 **********************************************************************/
TABMAPFile::~TABMAPFile()
{
    Close();
}

/**********************************************************************
 *                   TABMAPFile::Open()
 *
 * Compatibility layer with new interface.
 * Return 0 on success, -1 in case of failure.
 **********************************************************************/

int TABMAPFile::Open(const char *pszFname, const char* pszAccess, GBool bNoErrorMsg,
                     int nBlockSizeForCreate)
{
    // cppcheck-suppress nullPointer
    if( STARTS_WITH_CI(pszAccess, "r") )
        return Open(pszFname, TABRead, bNoErrorMsg, nBlockSizeForCreate);
    else if( STARTS_WITH_CI(pszAccess, "w") )
        return Open(pszFname, TABWrite, bNoErrorMsg, nBlockSizeForCreate);
    else
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed: access mode \"%s\" not supported", pszAccess);
        return -1;
    }
}

/**********************************************************************
 *                   TABMAPFile::Open()
 *
 * Open a .MAP file, and initialize the structures to be ready to read
 * objects from it.
 *
 * Since .MAP and .ID files are optional, you can set bNoErrorMsg=TRUE to
 * disable the error message and receive an return value of 1 if file
 * cannot be opened.
 * In this case, only the methods MoveToObjId() and GetCurObjType() can
 * be used.  They will behave as if the .ID file contained only null
 * references, so all object will look like they have NONE geometries.
 *
 * Returns 0 on success, 1 when the .map file does not exist, -1 on error.
 **********************************************************************/
int TABMAPFile::Open(const char *pszFname, TABAccess eAccess,
                     GBool bNoErrorMsg /* = FALSE */,
                     int nBlockSizeForCreate /* = 512 */)
{
    CPLErrorReset();

    VSILFILE    *fp=nullptr;
    TABRawBinBlock *poBlock=nullptr;

    if (m_fp)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed: object already contains an open file");
        return -1;
    }

    m_nMinTABVersion = 300;
    m_fp = nullptr;
    m_poHeader = nullptr;
    m_poIdIndex = nullptr;
    m_poSpIndex = nullptr;
    m_poToolDefTable = nullptr;
    m_eAccessMode = eAccess;
    m_bUpdated = FALSE;
    m_bLastOpWasRead = FALSE;
    m_bLastOpWasWrite = FALSE;

    if( m_eAccessMode == TABWrite &&
            (nBlockSizeForCreate < TAB_MIN_BLOCK_SIZE ||
             nBlockSizeForCreate > TAB_MAX_BLOCK_SIZE ||
             (nBlockSizeForCreate % TAB_MIN_BLOCK_SIZE) != 0) )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Open() failed: invalid block size: %d", nBlockSizeForCreate);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Open file
     *----------------------------------------------------------------*/
    const char* pszAccess = ( eAccess == TABRead ) ? "rb" :
                            ( eAccess == TABWrite ) ? "wb+" :
                                                      "rb+";
    fp = VSIFOpenL(pszFname, pszAccess);

    m_oBlockManager.Reset();

    if (fp != nullptr && (m_eAccessMode == TABRead || m_eAccessMode == TABReadWrite))
    {
        /*-----------------------------------------------------------------
         * Read access: try to read header block
         * First try with a 512 bytes block to check the .map version.
         * If it is version 500 or more then read again a 1024 bytes block
         *----------------------------------------------------------------*/
        poBlock = TABCreateMAPBlockFromFile(fp, 0, 512, TRUE, m_eAccessMode);

        if (poBlock && poBlock->GetBlockClass() == TABMAP_HEADER_BLOCK &&
            cpl::down_cast<TABMAPHeaderBlock*>(poBlock)->m_nMAPVersionNumber >= 500)
        {
            // Version 500 or higher.  Read 1024 bytes block instead of 512
            delete poBlock;
            poBlock = TABCreateMAPBlockFromFile(fp, 0, 1024, TRUE, m_eAccessMode);
        }

        if (poBlock==nullptr || poBlock->GetBlockClass() != TABMAP_HEADER_BLOCK)
        {
            if (poBlock)
                delete poBlock;
            poBlock = nullptr;
            VSIFCloseL(fp);
            CPLError(CE_Failure, CPLE_FileIO,
                "Open() failed: %s does not appear to be a valid .MAP file",
                     pszFname);
            return -1;
        }
        m_oBlockManager.SetBlockSize(cpl::down_cast<TABMAPHeaderBlock*>(poBlock)->m_nRegularBlockSize);
    }
    else if (fp != nullptr && m_eAccessMode == TABWrite)
    {
        /*-----------------------------------------------------------------
         * Write access: create a new header block
         * .MAP files of Version 500 and up appear to have a 1024 bytes
         * header.  The last 512 bytes are usually all zeros.
         *----------------------------------------------------------------*/
        m_poHeader = new TABMAPHeaderBlock(m_eAccessMode);
        poBlock = m_poHeader;
        poBlock->InitNewBlock(fp, nBlockSizeForCreate, 0 );

        m_oBlockManager.SetBlockSize(m_poHeader->m_nRegularBlockSize);
        if( m_poHeader->m_nRegularBlockSize == 512 )
            m_oBlockManager.SetLastPtr( 512 );
        else
            m_oBlockManager.SetLastPtr( 0 );

        m_bUpdated = TRUE;
    }
    else if (bNoErrorMsg)
    {
        /*-----------------------------------------------------------------
         * .MAP does not exist... produce no error message, but set
         * the class members so that MoveToObjId() and GetCurObjType()
         * can be used to return only NONE geometries.
         *----------------------------------------------------------------*/
        m_fp = nullptr;
        m_nCurObjType = TAB_GEOM_NONE;

        /* Create a false header block that will return default
         * values for projection and coordsys conversion stuff...
         */
        m_poHeader = new TABMAPHeaderBlock(m_eAccessMode);
        m_poHeader->InitNewBlock(nullptr, 512, 0 );

        return 1;
    }
    else
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed for %s", pszFname);
        return -1;
    }

    /*-----------------------------------------------------------------
     * File appears to be valid... set the various class members
     *----------------------------------------------------------------*/
    m_fp = fp;
    m_poHeader = cpl::down_cast<TABMAPHeaderBlock*>(poBlock);
    m_pszFname = CPLStrdup(pszFname);

    /*-----------------------------------------------------------------
     * Create a TABMAPObjectBlock, in READ mode only or in UPDATE mode
     * if there's an object
     *
     * In WRITE mode, the object block will be created only when needed.
     * We do not create the object block in the open() call because
     * files that contained only "NONE" geometries ended up with empty
     * object and spatial index blocks.
     *----------------------------------------------------------------*/

    if (m_eAccessMode == TABRead ||
        (m_eAccessMode == TABReadWrite && m_poHeader->m_nFirstIndexBlock != 0 ))
    {
        m_poCurObjBlock = new TABMAPObjectBlock(m_eAccessMode);
        m_poCurObjBlock->InitNewBlock(m_fp, m_poHeader->m_nRegularBlockSize);
    }
    else
    {
        m_poCurObjBlock = nullptr;
    }

    /*-----------------------------------------------------------------
     * Open associated .ID (object id index) file
     *----------------------------------------------------------------*/
    m_poIdIndex = new TABIDFile;
    if (m_poIdIndex->Open(pszFname, m_eAccessMode) != 0)
    {
        // Failed... an error has already been reported
        Close();
        return -1;
    }

    /*-----------------------------------------------------------------
     * Default Coord filter is the MBR of the whole file
     * This is currently unused but could eventually be used to handle
     * spatial filters more efficiently.
     *----------------------------------------------------------------*/
    if (m_eAccessMode == TABRead || m_eAccessMode == TABReadWrite)
    {
        ResetCoordFilter();
    }

    /*-----------------------------------------------------------------
     * We could scan a file through its quad tree index... but we don't!
     *
     * In read mode, we just ignore the spatial index.
     *
     * In write mode the index is created and maintained as new object
     * blocks are added inside CommitObjBlock().
     *----------------------------------------------------------------*/
    m_poSpIndex = nullptr;

    if (m_eAccessMode == TABReadWrite)
    {
        /* We don't allow quick mode in read/write mode */
        m_bQuickSpatialIndexMode = FALSE;

        if( m_poHeader->m_nFirstIndexBlock != 0 )
        {
            poBlock = GetIndexObjectBlock( m_poHeader->m_nFirstIndexBlock );
            if( poBlock == nullptr || (poBlock->GetBlockType() != TABMAP_INDEX_BLOCK &&
                                    poBlock->GetBlockType() != TABMAP_OBJECT_BLOCK) )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot find first index block at offset %d",
                         m_poHeader->m_nFirstIndexBlock );
                delete poBlock;
            }
            else if( poBlock->GetBlockType() == TABMAP_INDEX_BLOCK )
            {
                m_poSpIndex = cpl::down_cast<TABMAPIndexBlock *>(poBlock);
                m_poSpIndex->SetMBR(m_poHeader->m_nXMin, m_poHeader->m_nYMin,
                                    m_poHeader->m_nXMax, m_poHeader->m_nYMax);
            }
            else /* if( poBlock->GetBlockType() == TABMAP_OBJECT_BLOCK ) */
            {
                /* This can happen if the file created by MapInfo contains just */
                /* a few objects */
                delete poBlock;
            }
        }
    }

    /*-----------------------------------------------------------------
     * Initialization of the Drawing Tools table will be done automatically
     * as Read/Write calls are done later.
     *----------------------------------------------------------------*/
    m_poToolDefTable = nullptr;

    if( m_eAccessMode == TABReadWrite )
    {
        InitDrawingTools();
    }

    if( m_eAccessMode == TABReadWrite )
    {
        VSIStatBufL sStatBuf;
        if( VSIStatL(m_pszFname, &sStatBuf) != 0 )
        {
            Close();
            return -1;
        }
        m_oBlockManager.SetLastPtr(
            static_cast<int>(((sStatBuf.st_size-1)/m_poHeader->m_nRegularBlockSize)*m_poHeader->m_nRegularBlockSize));

        /* Read chain of garbage blocks */
        if( m_poHeader->m_nFirstGarbageBlock != 0 )
        {
            int nCurGarbBlock = m_poHeader->m_nFirstGarbageBlock;
            m_oBlockManager.PushGarbageBlockAsLast(nCurGarbBlock);
            while( true )
            {
                GUInt16 nBlockType = 0;
                int nNextGarbBlockPtr = 0;
                if( VSIFSeekL(fp, nCurGarbBlock, SEEK_SET) != 0 ||
                    VSIFReadL(&nBlockType, sizeof(nBlockType), 1, fp) != 1 ||
                    VSIFReadL(&nNextGarbBlockPtr, sizeof(nNextGarbBlockPtr), 1, fp) != 1 )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Cannot read garbage block at offset %d",
                             nCurGarbBlock);
                    break;
                }
                if( nBlockType != TABMAP_GARB_BLOCK )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Got block type (%d) instead of %d at offset %d",
                             nBlockType, TABMAP_GARB_BLOCK, nCurGarbBlock);
                }
                if( nNextGarbBlockPtr == 0 )
                    break;
                nCurGarbBlock = nNextGarbBlockPtr;
                m_oBlockManager.PushGarbageBlockAsLast(nCurGarbBlock);
            }
        }
    }

    /*-----------------------------------------------------------------
     * Make sure all previous calls succeeded.
     *----------------------------------------------------------------*/
    if (CPLGetLastErrorType() == CE_Failure)
    {
        // Open Failed... an error has already been reported
        Close();
        return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABMAPFile::Close()
 *
 * Close current file, and release all memory used.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::Close()
{
    // Check if file is opened... it is possible to have a fake header
    // without an actual file attached to it.
    if (m_fp == nullptr && m_poHeader == nullptr)
        return 0;

    /*----------------------------------------------------------------
     * Write access: commit latest changes to the file.
     *---------------------------------------------------------------*/
    if (m_eAccessMode != TABRead)
    {
        SyncToDisk();
    }

    // Delete all structures
    if (m_poHeader)
        delete m_poHeader;
    m_poHeader = nullptr;

    if (m_poIdIndex)
    {
        m_poIdIndex->Close();
        delete m_poIdIndex;
        m_poIdIndex = nullptr;
    }

    if (m_poCurObjBlock)
    {
        delete m_poCurObjBlock;
        m_poCurObjBlock = nullptr;
        m_nCurObjPtr = -1;
        m_nCurObjType = TAB_GEOM_UNSET;
        m_nCurObjId = -1;
    }

    if (m_poCurCoordBlock)
    {
        delete m_poCurCoordBlock;
        m_poCurCoordBlock = nullptr;
    }

    if (m_poSpIndex)
    {
        delete m_poSpIndex;
        m_poSpIndex = nullptr;
        m_poSpIndexLeaf = nullptr;
    }

    if (m_poToolDefTable)
    {
        delete m_poToolDefTable;
        m_poToolDefTable = nullptr;
    }

    // Close file
    if (m_fp)
        VSIFCloseL(m_fp);
    m_fp = nullptr;

    CPLFree(m_pszFname);
    m_pszFname = nullptr;

    return 0;
}

/************************************************************************/
/*                         GetFileSize()                                */
/************************************************************************/

GUInt32 TABMAPFile::GetFileSize()
{
    if( !m_fp )
        return 0;
    vsi_l_offset nCurPos = VSIFTellL(m_fp);
    VSIFSeekL(m_fp, 0, SEEK_END);
    vsi_l_offset nSize = VSIFTellL(m_fp);
    VSIFSeekL(m_fp, nCurPos, SEEK_SET);
    return nSize > UINT_MAX ? UINT_MAX : static_cast<GUInt32>(nSize);
}

/************************************************************************/
/*                            SyncToDisk()                             */
/************************************************************************/

int TABMAPFile::SyncToDisk()
{
    if( m_eAccessMode == TABRead )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "SyncToDisk() can be used only with Write access.");
        return -1;
    }

    if( !m_bUpdated)
        return 0;

    // Start by committing current object and coord blocks
    // Nothing happens if none has been created yet.
    if( CommitObjAndCoordBlocks(FALSE) != 0 )
        return -1;

    // Write the drawing tools definitions now.
    if( CommitDrawingTools() != 0 )
        return -1;

    // Commit spatial index blocks
    if( CommitSpatialIndex() != 0 )
        return -1;

    // Update header fields and commit
    if (m_poHeader)
    {
        // OK, with V450 files, objects are not limited to 32k nodes
        // any more, and this means that m_nMaxCoordBufSize can become
        // huge, and actually more huge than can be held in memory.
        // MapInfo counts m_nMaxCoordBufSize=0 for V450 objects, but
        // until this is cleanly implemented, we will just prevent
        // m_nMaxCoordBufSizefrom going beyond 512k in V450 files.
        if (m_nMinTABVersion >= 450)
        {
            m_poHeader->m_nMaxCoordBufSize =
                std::min(m_poHeader->m_nMaxCoordBufSize, 512*1024);
        }

        // Write Ref to beginning of the chain of garbage blocks
        m_poHeader->m_nFirstGarbageBlock =
            m_oBlockManager.GetFirstGarbageBlock();

        if( m_poHeader->CommitToFile() != 0 )
            return -1;
    }

    // Check for overflow of internal coordinates and produce a warning
    // if that happened...
    if (m_poHeader && m_poHeader->m_bIntBoundsOverflow)
    {
        double dBoundsMinX = 0.0;
        double dBoundsMinY = 0.0;
        double dBoundsMaxX = 0.0;
        double dBoundsMaxY = 0.0;
        Int2Coordsys(-1000000000, -1000000000, dBoundsMinX, dBoundsMinY);
        Int2Coordsys(1000000000, 1000000000, dBoundsMaxX, dBoundsMaxY);

        CPLError(CE_Warning,
                 static_cast<CPLErrorNum>(TAB_WarningBoundsOverflow),
                 "Some objects were written outside of the file's "
                 "predefined bounds.\n"
                 "These objects may have invalid coordinates when the file "
                 "is reopened.\n"
                 "Predefined bounds: (%.15g,%.15g)-(%.15g,%.15g)\n",
                 dBoundsMinX, dBoundsMinY, dBoundsMaxX, dBoundsMaxY );
    }

    if( m_poIdIndex != nullptr && m_poIdIndex->SyncToDisk() != 0 )
        return -1;

    m_bUpdated = FALSE;

    return 0;
}

/**********************************************************************
 *                   TABMAPFile::ReOpenReadWrite()
 **********************************************************************/
int TABMAPFile::ReOpenReadWrite()
{
    char* pszFname = m_pszFname;
    m_pszFname = nullptr;
    Close();
    if( Open(pszFname, TABReadWrite) < 0 )
    {
        CPLFree(pszFname);
        return -1;
    }
    CPLFree(pszFname);
    return 0;
}

/**********************************************************************
 *                   TABMAPFile::SetQuickSpatialIndexMode()
 *
 * Select "quick spatial index mode".
 *
 * The default behavior of MITAB is to generate an optimized spatial index,
 * but this results in slower write speed.
 *
 * Applications that want faster write speed and do not care
 * about the performance of spatial queries on the resulting file can
 * use SetQuickSpatialIndexMode() to require the creation of a non-optimal
 * spatial index (actually emulating the type of spatial index produced
 * by MITAB before version 1.6.0). In this mode writing files can be
 * about 5 times faster, but spatial queries can be up to 30 times slower.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::SetQuickSpatialIndexMode(GBool bQuickSpatialIndexMode/*=TRUE*/)
{
    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "SetQuickSpatialIndexMode() failed: file not opened for write access.");
        return -1;
    }

    if (m_poCurObjBlock != nullptr || m_poSpIndex != nullptr)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "SetQuickSpatialIndexMode() must be called before writing the first object.");
        return -1;
    }

    m_bQuickSpatialIndexMode = bQuickSpatialIndexMode;

    return 0;
}

/************************************************************************/
/*                             PushBlock()                              */
/*                                                                      */
/*      Install a new block (object or spatial) as being current -      */
/*      whatever that means.  This method is only intended to ever      */
/*      be called from LoadNextMatchingObjectBlock().                   */
/************************************************************************/

TABRawBinBlock *TABMAPFile::PushBlock( int nFileOffset )

{
    TABRawBinBlock *poBlock = GetIndexObjectBlock( nFileOffset );
    if( poBlock == nullptr )
        return nullptr;

    if( poBlock->GetBlockType() == TABMAP_INDEX_BLOCK )
    {
        TABMAPIndexBlock *poIndex = cpl::down_cast<TABMAPIndexBlock *>(poBlock);

        if( m_poSpIndexLeaf == nullptr )
        {
            delete m_poSpIndex;
            m_poSpIndexLeaf = poIndex;
            m_poSpIndex = poIndex;
        }
        else
        {
            CPLAssert(
                m_poSpIndexLeaf->GetEntry(
                    m_poSpIndexLeaf->GetCurChildIndex())->nBlockPtr
                == nFileOffset );

            m_poSpIndexLeaf->SetCurChildRef( poIndex,
                                         m_poSpIndexLeaf->GetCurChildIndex() );
            poIndex->SetParentRef( m_poSpIndexLeaf );
            m_poSpIndexLeaf = poIndex;
        }
    }
    else
    {
        CPLAssert( poBlock->GetBlockType() == TABMAP_OBJECT_BLOCK );

        if( m_poCurObjBlock != nullptr )
            delete m_poCurObjBlock;

        m_poCurObjBlock = cpl::down_cast<TABMAPObjectBlock *>(poBlock);

        m_nCurObjPtr = nFileOffset;
        m_nCurObjType = TAB_GEOM_NONE;
        m_nCurObjId   = -1;
    }

    return poBlock;
}

/************************************************************************/
/*                    LoadNextMatchingObjectBlock()                     */
/*                                                                      */
/*      Advance through the spatial indices till the next object        */
/*      block is loaded that matching the spatial query extents.        */
/************************************************************************/

int TABMAPFile::LoadNextMatchingObjectBlock( int bFirstObject )

{
    // If we are just starting, verify the stack is empty.
    if( bFirstObject )
    {
        CPLAssert( m_poSpIndexLeaf == nullptr );

        /* m_nFirstIndexBlock set to 0 means that there is no feature */
        if ( m_poHeader->m_nFirstIndexBlock == 0 )
            return FALSE;

        if( m_poSpIndex != nullptr )
        {
            m_poSpIndex->UnsetCurChild();
            m_poSpIndexLeaf = m_poSpIndex;
        }
        else
        {
            if( PushBlock( m_poHeader->m_nFirstIndexBlock ) == nullptr )
                return FALSE;

            if( m_poSpIndex == nullptr )
            {
                CPLAssert( m_poCurObjBlock != nullptr );
                return TRUE;
            }
        }
    }

    while( m_poSpIndexLeaf != nullptr )
    {
        int     iEntry = m_poSpIndexLeaf->GetCurChildIndex();

        if( iEntry >= m_poSpIndexLeaf->GetNumEntries()-1 )
        {
            TABMAPIndexBlock *poParent = m_poSpIndexLeaf->GetParentRef();
            if( m_poSpIndexLeaf == m_poSpIndex )
                m_poSpIndex->UnsetCurChild();
            else
                delete m_poSpIndexLeaf;
            m_poSpIndexLeaf = poParent;

            if( poParent != nullptr )
            {
                poParent->SetCurChildRef( nullptr, poParent->GetCurChildIndex() );
            }
            continue;
        }

        m_poSpIndexLeaf->SetCurChildRef( nullptr, ++iEntry );

        TABMAPIndexEntry *psEntry = m_poSpIndexLeaf->GetEntry( iEntry );

        if( psEntry->XMax < m_XMinFilter
            || psEntry->YMax < m_YMinFilter
            || psEntry->XMin > m_XMaxFilter
            || psEntry->YMin > m_YMaxFilter )
            continue;

        TABRawBinBlock *poBlock = PushBlock( psEntry->nBlockPtr );
        if( poBlock == nullptr )
            return FALSE;
        else if( poBlock->GetBlockType() == TABMAP_OBJECT_BLOCK )
            return TRUE;
        else {
            /* continue processing new index block */
        }
    }

    return false;
}

/************************************************************************/
/*                            ResetReading()                            */
/*                                                                      */
/*      Ensure that any resources related to a spatial traversal of     */
/*      the file are recovered, and the state reinitialized to the      */
/*      initial conditions.                                             */
/************************************************************************/

void TABMAPFile::ResetReading()

{
    if( m_bLastOpWasWrite )
        CommitObjAndCoordBlocks( FALSE );

    if (m_poSpIndex)
    {
        m_poSpIndex->UnsetCurChild();
    }
    m_poSpIndexLeaf = nullptr;

    m_bLastOpWasWrite = FALSE;
    m_bLastOpWasRead = FALSE;
}

/************************************************************************/
/*                          GetNextFeatureId()                          */
/*                                                                      */
/*      Fetch the next feature id based on a traversal of the           */
/*      spatial index.                                                  */
/************************************************************************/

int TABMAPFile::GetNextFeatureId( int nPrevId )

{
    if( m_bLastOpWasWrite )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "GetNextFeatureId() cannot be called after write operation");
        return -1;
    }
    if( m_eAccessMode == TABWrite )
    {
        if( ReOpenReadWrite() < 0 )
            return -1;
    }
    m_bLastOpWasRead = TRUE;

/* -------------------------------------------------------------------- */
/*      m_fp is NULL when all geometry are NONE and/or there's          */
/*          no .map file and/or there's no spatial indexes              */
/* -------------------------------------------------------------------- */
    if( m_fp == nullptr )
        return -1;

    if( nPrevId == 0 )
        nPrevId = -1;

/* -------------------------------------------------------------------- */
/*      This should always be true if we are being called properly.     */
/* -------------------------------------------------------------------- */
    if( nPrevId != -1 && m_nCurObjId != nPrevId )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "TABMAPFile::GetNextFeatureId(%d) called out of sequence.",
                  nPrevId );
        return -1;
    }

    CPLAssert( nPrevId == -1 || m_poCurObjBlock != nullptr );

/* -------------------------------------------------------------------- */
/*      Ensure things are initialized properly if this is a request     */
/*      for the first feature.                                          */
/* -------------------------------------------------------------------- */
    if( nPrevId == -1 )
    {
        m_nCurObjId = -1;
    }

/* -------------------------------------------------------------------- */
/*      Try to advance to the next object in the current object         */
/*      block.                                                          */
/* -------------------------------------------------------------------- */
    if( nPrevId == -1
        || m_poCurObjBlock->AdvanceToNextObject(m_poHeader) == -1 )
    {
        // If not, try to advance to the next object block, and get
        // first object from it.  Note that some object blocks actually
        // have no objects, so we may have to advance to additional
        // object blocks till we find a non-empty one.
        GBool bFirstCall = (nPrevId == -1);
        do
        {
            if( !LoadNextMatchingObjectBlock( bFirstCall ) )
                return -1;

            bFirstCall = FALSE;
        } while( m_poCurObjBlock->AdvanceToNextObject(m_poHeader) == -1 );
    }

    m_nCurObjType = m_poCurObjBlock->GetCurObjectType();
    m_nCurObjId = m_poCurObjBlock->GetCurObjectId();
    m_nCurObjPtr = m_poCurObjBlock->GetStartAddress()
        + m_poCurObjBlock->GetCurObjectOffset();

    CPLAssert( m_nCurObjId != -1 );

    return m_nCurObjId;
}

/**********************************************************************
 *                   TABMAPFile::Int2Coordsys()
 *
 * Convert from long integer (internal) to coordinates system units
 * as defined in the file's coordsys clause.
 *
 * Note that the false easting/northing and the conversion factor from
 * datum to coordsys units are not included in the calculation.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::Int2Coordsys( GInt32 nX, GInt32 nY, double &dX, double &dY )
{
    if( m_poHeader == nullptr )
        return -1;

    return m_poHeader->Int2Coordsys(nX, nY, dX, dY);
}

/**********************************************************************
 *                   TABMAPFile::Coordsys2Int()
 *
 * Convert from coordinates system units as defined in the file's
 * coordsys clause to long integer (internal) coordinates.
 *
 * Note that the false easting/northing and the conversion factor from
 * datum to coordsys units are not included in the calculation.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::Coordsys2Int( double dX, double dY, GInt32 &nX, GInt32 &nY,
                              GBool bIgnoreOverflow/*=FALSE*/ )
{
    if( m_poHeader == nullptr )
        return -1;

    return m_poHeader->Coordsys2Int(dX, dY, nX, nY, bIgnoreOverflow);
}

/**********************************************************************
 *                   TABMAPFile::Int2CoordsysDist()
 *
 * Convert a pair of X,Y size (or distance) values from long integer
 * (internal) to coordinates system units as defined in the file's coordsys
 * clause.
 *
 * The difference with Int2Coordsys() is that this function only applies
 * the scaling factor: it does not apply the displacement.
 *
 * Since the calculations on the X and Y values are independent, either
 * one can be omitted (i.e. passed as 0)
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::Int2CoordsysDist( GInt32 nX, GInt32 nY, double &dX, double &dY )
{
    if( m_poHeader == nullptr )
        return -1;

    return m_poHeader->Int2CoordsysDist(nX, nY, dX, dY);
}

/**********************************************************************
 *                   TABMAPFile::Coordsys2IntDist()
 *
 * Convert a pair of X,Y size (or distance) values from coordinates
 * system units as defined in the file's coordsys clause to long
 * integer (internal) coordinate units.
 *
 * The difference with Int2Coordsys() is that this function only applies
 * the scaling factor: it does not apply the displacement.
 *
 * Since the calculations on the X and Y values are independent, either
 * one can be omitted (i.e. passed as 0)
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::Coordsys2IntDist( double dX, double dY, GInt32 &nX, GInt32 &nY )
{
    if (m_poHeader == nullptr)
        return -1;

    return m_poHeader->Coordsys2IntDist(dX, dY, nX, nY);
}

/**********************************************************************
 *                   TABMAPFile::SetCoordsysBounds()
 *
 * Set projection coordinates bounds of the newly created dataset.
 *
 * This function must be called after creating a new dataset and before any
 * feature can be written to it.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::SetCoordsysBounds( double dXMin, double dYMin,
                                   double dXMax, double dYMax )
{
    if (m_poHeader == nullptr)
        return -1;

    const int nStatus =
        m_poHeader->SetCoordsysBounds(dXMin, dYMin, dXMax, dYMax);

    if (nStatus == 0)
        ResetCoordFilter();

    return nStatus;
}

/**********************************************************************
 *                   TABMAPFile::GetMaxObjId()
 *
 * Return the value of the biggest valid object id.
 *
 * Note that object ids are positive and start at 1.
 *
 * Returns a value >= 0 on success, -1 on error.
 **********************************************************************/
GInt32 TABMAPFile::GetMaxObjId()
{
    if (m_poIdIndex)
        return m_poIdIndex->GetMaxObjId();

    return -1;
}

/**********************************************************************
 *                   TABMAPFile::MoveToObjId()
 *
 * Get ready to work with the object with the specified id.  The object
 * data pointer (inside m_poCurObjBlock) will be moved to the first byte
 * of data for this map object.
 *
 * The object type and id (i.e. table row number) will be accessible
 * using GetCurObjType() and GetCurObjId().
 *
 * Note that object ids are positive and start at 1.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::MoveToObjId(int nObjId)
{
    if( m_bLastOpWasWrite )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "MoveToObjId() cannot be called after write operation");
        return -1;
    }
    if( m_eAccessMode == TABWrite )
    {
        if( ReOpenReadWrite() < 0 )
            return -1;
    }
    m_bLastOpWasRead = TRUE;

    /*-----------------------------------------------------------------
     * In non creation mode, since the .MAP/.ID are optional, if the
     * file is not opened then we can still act as if one existed and
     * make any object id look like a TAB_GEOM_NONE
     *----------------------------------------------------------------*/
    if (m_fp == nullptr && m_eAccessMode != TABWrite)
    {
        CPLAssert(m_poIdIndex == nullptr && m_poCurObjBlock == nullptr);
        m_nCurObjPtr = 0;
        m_nCurObjId = nObjId;
        m_nCurObjType = TAB_GEOM_NONE;

        return 0;
    }

    if (m_poIdIndex == nullptr)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "MoveToObjId(): file not opened!");
        m_nCurObjPtr = -1;
        m_nCurObjId = -1;
        m_nCurObjType = TAB_GEOM_UNSET;
        return -1;
    }

    /*-----------------------------------------------------------------
     * Move map object pointer to the right location.  Fetch location
     * from the index file, unless we are already pointing at it.
     *----------------------------------------------------------------*/
    int nFileOffset = m_nCurObjId == nObjId
        ? m_nCurObjPtr
        : m_poIdIndex->GetObjPtr(nObjId);

    if (nFileOffset != 0 && m_poCurObjBlock == nullptr)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "MoveToObjId(): no current object block!");
        m_nCurObjPtr = -1;
        m_nCurObjId = -1;
        m_nCurObjType = TAB_GEOM_UNSET;
        return -1;
    }

    if (nFileOffset == 0)
    {
        /*---------------------------------------------------------
         * Object with no geometry... this is a valid case.
         *--------------------------------------------------------*/
        m_nCurObjPtr = 0;
        m_nCurObjId = nObjId;
        m_nCurObjType = TAB_GEOM_NONE;
    }
    else if ( m_poCurObjBlock->GotoByteInFile(nFileOffset, TRUE) == 0)
    {
        /*-------------------------------------------------------------
         * OK, it worked, read the object type and row id.
         *------------------------------------------------------------*/
        m_nCurObjPtr = nFileOffset;

        const GByte byVal = m_poCurObjBlock->ReadByte();
        if( IsValidObjType(byVal) )
        {
            m_nCurObjType = static_cast<TABGeomType>(byVal);
        }
        else
        {
            CPLError(CE_Warning,
                static_cast<CPLErrorNum>(TAB_WarningFeatureTypeNotSupported),
                "Unsupported object type %d (0x%2.2x).  Feature will be "
                "returned with NONE geometry.",
                byVal, byVal);
            m_nCurObjType = TAB_GEOM_NONE;
        }
        m_nCurObjId   = m_poCurObjBlock->ReadInt32();

        // Do a consistency check...
        if (m_nCurObjId != nObjId)
        {
            if( m_nCurObjId == (nObjId | 0x40000000) )
            {
                CPLError(CE_Failure, CPLE_FileIO,
                    "Object %d is marked as deleted in the .MAP file but not in the .ID file."
                    "File may be corrupt.",
                    nObjId);
            }
            else
            {
                CPLError(CE_Failure, CPLE_FileIO,
                    "Object ID from the .ID file (%d) differs from the value "
                    "in the .MAP file (%d).  File may be corrupt.",
                    nObjId, m_nCurObjId);
            }
            m_nCurObjPtr = -1;
            m_nCurObjId = -1;
            m_nCurObjType = TAB_GEOM_UNSET;
            return -1;
        }
    }
    else
    {
        /*---------------------------------------------------------
         * Failed positioning input file... CPLError has been called.
         *--------------------------------------------------------*/
        m_nCurObjPtr = -1;
        m_nCurObjId = -1;
        m_nCurObjType = TAB_GEOM_UNSET;
        return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABMAPFile::MarkAsDeleted()
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::MarkAsDeleted()
{
    if (m_eAccessMode == TABRead)
        return -1;

    if ( m_nCurObjPtr <= 0 )
        return 0;

    int ret = 0;
    if( m_nCurObjType != TAB_GEOM_NONE  )
    {
        /* Goto offset for object id */
        if ( m_poCurObjBlock == nullptr ||
            m_poCurObjBlock->GotoByteInFile(m_nCurObjPtr + 1, TRUE) != 0)
            return -1;

        /* Mark object as deleted */
        m_poCurObjBlock->WriteInt32(m_nCurObjId | 0x40000000);

        if( m_poCurObjBlock->CommitToFile() != 0 )
            ret = -1;
    }

    /* Update index entry to reflect delete state as well */
    if( m_poIdIndex->SetObjPtr(m_nCurObjId, 0) != 0 )
        ret = -1;

    m_nCurObjPtr = -1;
    m_nCurObjId = -1;
    m_nCurObjType = TAB_GEOM_UNSET;
    m_bUpdated = TRUE;

    return ret;
}

/**********************************************************************
 *                   TABMAPFile::UpdateMapHeaderInfo()
 *
 * Update .map header information (counter of objects by type and minimum
 * required version) in light of a new object to be written to the file.
 *
 * Called only by PrepareNewObj() and by the TABCollection class.
 **********************************************************************/
void  TABMAPFile::UpdateMapHeaderInfo(TABGeomType nObjType)
{
    /*-----------------------------------------------------------------
     * Update count of objects by type in the header block
     *----------------------------------------------------------------*/
    if (nObjType == TAB_GEOM_SYMBOL ||
        nObjType == TAB_GEOM_FONTSYMBOL ||
        nObjType == TAB_GEOM_CUSTOMSYMBOL ||
        nObjType == TAB_GEOM_MULTIPOINT ||
        nObjType == TAB_GEOM_V800_MULTIPOINT ||
        nObjType == TAB_GEOM_SYMBOL_C ||
        nObjType == TAB_GEOM_FONTSYMBOL_C ||
        nObjType == TAB_GEOM_CUSTOMSYMBOL_C ||
        nObjType == TAB_GEOM_MULTIPOINT_C ||
        nObjType == TAB_GEOM_V800_MULTIPOINT_C )
    {
        m_poHeader->m_numPointObjects++;
    }
    else if (nObjType == TAB_GEOM_LINE ||
             nObjType == TAB_GEOM_PLINE ||
             nObjType == TAB_GEOM_MULTIPLINE ||
             nObjType == TAB_GEOM_V450_MULTIPLINE ||
             nObjType == TAB_GEOM_V800_MULTIPLINE ||
             nObjType == TAB_GEOM_ARC ||
             nObjType == TAB_GEOM_LINE_C ||
             nObjType == TAB_GEOM_PLINE_C ||
             nObjType == TAB_GEOM_MULTIPLINE_C ||
             nObjType == TAB_GEOM_V450_MULTIPLINE_C ||
             nObjType == TAB_GEOM_V800_MULTIPLINE_C ||
             nObjType == TAB_GEOM_ARC_C)
    {
        m_poHeader->m_numLineObjects++;
    }
    else if (nObjType == TAB_GEOM_REGION ||
             nObjType == TAB_GEOM_V450_REGION ||
             nObjType == TAB_GEOM_V800_REGION ||
             nObjType == TAB_GEOM_RECT ||
             nObjType == TAB_GEOM_ROUNDRECT ||
             nObjType == TAB_GEOM_ELLIPSE ||
             nObjType == TAB_GEOM_REGION_C ||
             nObjType == TAB_GEOM_V450_REGION_C ||
             nObjType == TAB_GEOM_V800_REGION_C ||
             nObjType == TAB_GEOM_RECT_C ||
             nObjType == TAB_GEOM_ROUNDRECT_C ||
             nObjType == TAB_GEOM_ELLIPSE_C)
    {
        m_poHeader->m_numRegionObjects++;
    }
    else if (nObjType == TAB_GEOM_TEXT ||
             nObjType == TAB_GEOM_TEXT_C)
    {
        m_poHeader->m_numTextObjects++;
    }

    /*-----------------------------------------------------------------
     * Check for minimum TAB file version number
     *----------------------------------------------------------------*/
    int nVersion = TAB_GEOM_GET_VERSION(nObjType);

    if (nVersion > m_nMinTABVersion )
    {
        m_nMinTABVersion = nVersion;
    }
}

/**********************************************************************
 *                   TABMAPFile::PrepareNewObj()
 *
 * Get ready to write a new object described by poObjHdr (using the
 * poObjHdr's m_nId (featureId), m_nType and IntMBR members which must
 * have been set by the caller).
 *
 * Depending on whether "quick spatial index mode" is selected, we either:
 *
 * 1- Walk through the spatial index to find the best place to insert the
 * new object, update the spatial index references, and prepare the object
 * data block to be ready to write the object to it.
 * ... or ...
 * 2- prepare the current object data block to be ready to write the
 * object to it. If the object block is full then it is inserted in the
 * spatial index and committed to disk, and a new obj block is created.
 *
 * m_poCurObjBlock will be set to be ready to receive the new object, and
 * a new block will be created if necessary (in which case the current
 * block contents will be committed to disk, etc.)  The actual ObjHdr
 * data won't be written to m_poCurObjBlock until CommitNewObj() is called.
 *
 * If this object type uses coordinate blocks, then the coordinate block
 * will be prepared to receive coordinates.
 *
 * This function will also take care of updating the .ID index entry for
 * the new object.
 *
 * Note that object ids are positive and start at 1.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::PrepareNewObj( TABMAPObjHdr *poObjHdr )
{
    m_nCurObjPtr = -1;
    m_nCurObjId = -1;
    m_nCurObjType = TAB_GEOM_UNSET;

    if (m_eAccessMode == TABRead ||
        m_poIdIndex == nullptr || m_poHeader == nullptr)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "PrepareNewObj() failed: file not opened for write access.");
        return -1;
    }

    if (m_bLastOpWasRead )
    {
        m_bLastOpWasRead = FALSE;
        if( m_poSpIndex)
        {
            m_poSpIndex->UnsetCurChild();
        }
    }

    /*-----------------------------------------------------------------
     * For objects with no geometry, we just update the .ID file and return
     *----------------------------------------------------------------*/
    if (poObjHdr->m_nType == TAB_GEOM_NONE)
    {
        m_nCurObjType = poObjHdr->m_nType;
        m_nCurObjId   = poObjHdr->m_nId;
        m_nCurObjPtr  = 0;
        m_poIdIndex->SetObjPtr(m_nCurObjId, 0);

        return 0;
    }

    /*-----------------------------------------------------------------
     * Update count of objects by type in the header block and minimum
     * required version.
     *----------------------------------------------------------------*/
    UpdateMapHeaderInfo(poObjHdr->m_nType);

    /*-----------------------------------------------------------------
     * Depending on the selected spatial index mode, we will either insert
     * new objects via the spatial index (slower write but results in optimal
     * spatial index) or directly in the current ObjBlock (faster write
     * but non-optimal spatial index)
     *----------------------------------------------------------------*/
    if ( !m_bQuickSpatialIndexMode )
    {
        if (PrepareNewObjViaSpatialIndex(poObjHdr) != 0)
            return -1;  /* Error already reported */
    }
    else
    {
        if (PrepareNewObjViaObjBlock(poObjHdr) != 0)
            return -1;  /* Error already reported */
    }

    /*-----------------------------------------------------------------
     * Prepare ObjBlock for this new object.
     * Real data won't be written to the object block until CommitNewObj()
     * is called.
     *----------------------------------------------------------------*/
    m_nCurObjPtr = m_poCurObjBlock->PrepareNewObject(poObjHdr);
    if (m_nCurObjPtr < 0 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed writing object header for feature id %d",
                 poObjHdr->m_nId);
        return -1;
    }

    m_nCurObjType = poObjHdr->m_nType;
    m_nCurObjId   = poObjHdr->m_nId;

    /*-----------------------------------------------------------------
     * Update .ID Index
     *----------------------------------------------------------------*/
    m_poIdIndex->SetObjPtr(m_nCurObjId, m_nCurObjPtr);

    /*-----------------------------------------------------------------
     * Prepare Coords block...
     * create a new TABMAPCoordBlock if it was not done yet.
     *----------------------------------------------------------------*/
    PrepareCoordBlock(m_nCurObjType, m_poCurObjBlock, &m_poCurCoordBlock);

    if (CPLGetLastErrorType() == CE_Failure)
        return -1;

    m_bUpdated = TRUE;
    m_bLastOpWasWrite = TRUE;

    return 0;
}

/**********************************************************************
 *                   TABMAPFile::PrepareNewObjViaSpatialIndex()
 *
 * Used by TABMAPFile::PrepareNewObj() to walk through the spatial index
 * to find the best place to insert the new object, update the spatial
 * index references, and prepare the object data block to be ready to
 * write the object to it.
 *
 * This method is used when "quick spatial index mode" is NOT selected,
 * i.e. when we want to produce a file with an optimal spatial index
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int   TABMAPFile::PrepareNewObjViaSpatialIndex(TABMAPObjHdr *poObjHdr)
{
    GInt32 nObjBlockForInsert = -1;

    /*-----------------------------------------------------------------
     * Create spatial index if we don't have one yet.
     * We do not create the index and object data blocks in the open()
     * call because files that contained only "NONE" geometries ended up
     * with empty object and spatial index blocks.
     *----------------------------------------------------------------*/
    if (m_poSpIndex == nullptr)
    {
        // Spatial Index not created yet...
        m_poSpIndex = new TABMAPIndexBlock(m_eAccessMode);

        m_poSpIndex->InitNewBlock(m_fp, m_poHeader->m_nRegularBlockSize,
                                  m_oBlockManager.AllocNewBlock("INDEX"));
        m_poSpIndex->SetMAPBlockManagerRef(&m_oBlockManager);

        if( m_eAccessMode == TABReadWrite && m_poHeader->m_nFirstIndexBlock != 0 )
        {
            /* This can happen if the file created by MapInfo contains just */
            /* a few objects */
            TABRawBinBlock *poBlock
                = GetIndexObjectBlock( m_poHeader->m_nFirstIndexBlock );
            CPLAssert( poBlock != nullptr && poBlock->GetBlockType() == TABMAP_OBJECT_BLOCK);
            delete poBlock;

            if (m_poSpIndex->AddEntry(m_poHeader->m_nXMin, m_poHeader->m_nYMin,
                                      m_poHeader->m_nXMax, m_poHeader->m_nYMax,
                                      m_poHeader->m_nFirstIndexBlock) != 0)
                return -1;

            delete m_poCurObjBlock;
            m_poCurObjBlock = nullptr;
            delete m_poCurCoordBlock;
            m_poCurCoordBlock = nullptr;
        }

        m_poHeader->m_nFirstIndexBlock = m_poSpIndex->GetNodeBlockPtr();

        /* We'll also need to create an object data block (later) */
        // nObjBlockForInsert = -1;

        CPLAssert(m_poCurObjBlock == nullptr);
    }
    else
    /*-----------------------------------------------------------------
     * Search the spatial index to find the best place to insert this
     * new object.
     *----------------------------------------------------------------*/
    {
        nObjBlockForInsert=m_poSpIndex->ChooseLeafForInsert(poObjHdr->m_nMinX,
                                                            poObjHdr->m_nMinY,
                                                            poObjHdr->m_nMaxX,
                                                            poObjHdr->m_nMaxY);
        if( nObjBlockForInsert == -1 )
        {
            /* ChooseLeafForInsert() should not fail unless file is corrupt*/
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "ChooseLeafForInsert() Failed?!?!");
            return -1;
        }
    }

    if( nObjBlockForInsert == -1 )
    {
        /*-------------------------------------------------------------
         * Create a new object data block from scratch
         *------------------------------------------------------------*/
        m_poCurObjBlock = new TABMAPObjectBlock(TABReadWrite);

        int nBlockOffset = m_oBlockManager.AllocNewBlock("OBJECT");

        m_poCurObjBlock->InitNewBlock(m_fp, m_poHeader->m_nRegularBlockSize, nBlockOffset);

        /*-------------------------------------------------------------
         * Insert new object block in index, based on MBR of poObjHdr
         *------------------------------------------------------------*/
        if (m_poSpIndex->AddEntry(poObjHdr->m_nMinX,
                                  poObjHdr->m_nMinY,
                                  poObjHdr->m_nMaxX,
                                  poObjHdr->m_nMaxY,
                                  m_poCurObjBlock->GetStartAddress()) != 0)
            return -1;

        m_poCurObjBlock->SetMBR(poObjHdr->m_nMinX, poObjHdr->m_nMinY,
                                poObjHdr->m_nMaxX, poObjHdr->m_nMaxY);

        const int nNextDepth = m_poSpIndex->GetCurMaxDepth() + 1;
        m_poHeader->m_nMaxSpIndexDepth = static_cast<GByte>(
            std::max(static_cast<int>(m_poHeader->m_nMaxSpIndexDepth), nNextDepth));
    }
    else
    {
        /*-------------------------------------------------------------
         * Load existing object and Coord blocks, unless we've already
         * got the right object block in memory
         *------------------------------------------------------------*/
        if (m_poCurObjBlock &&
            m_poCurObjBlock->GetStartAddress() != nObjBlockForInsert)
        {
            /* Got a block in memory but it is not the right one, flush it */
            if (CommitObjAndCoordBlocks(TRUE) != 0 )
                return -1;
        }

        if (m_poCurObjBlock == nullptr)
        {
            if (LoadObjAndCoordBlocks(nObjBlockForInsert) != 0)
                return -1;
        }

        /* If we have compressed objects, we don't want to change the center  */
        m_poCurObjBlock->LockCenter();

        // Check if the ObjBlock know its MBR. If not (new block, or the current
        // block was the good one but retrieved without the index), get the value
        // from the index and set it.
        GInt32 nMinX, nMinY, nMaxX, nMaxY;
        m_poCurObjBlock->GetMBR(nMinX, nMinY, nMaxX, nMaxY);
        if( nMinX > nMaxX )
        {
            m_poSpIndex->GetCurLeafEntryMBR(m_poCurObjBlock->GetStartAddress(),
                                            nMinX, nMinY, nMaxX, nMaxY);
            m_poCurObjBlock->SetMBR(nMinX, nMinY, nMaxX, nMaxY);
        }
    }

    /*-----------------------------------------------------------------
     * Fetch new object size, make sure there is enough room in obj.
     * block for new object, update spatial index and split if necessary.
     *----------------------------------------------------------------*/
    int nObjSize = m_poHeader->GetMapObjectSize(poObjHdr->m_nType);

    /*-----------------------------------------------------------------
     * But first check if we can recover space from this block in case
     * there are deleted objects in it.
     *----------------------------------------------------------------*/
    if (m_poCurObjBlock->GetNumUnusedBytes() < nObjSize )
    {
        std::vector<std::unique_ptr<TABMAPObjHdr>> apoSrcObjHdrs;
        int nObjectSpace = 0;

        /* First pass to enumerate valid objects and compute their accumulated
           required size. */
        m_poCurObjBlock->Rewind();
        while (auto poExistingObjHdr = TABMAPObjHdr::ReadNextObj(m_poCurObjBlock,
                                                    m_poHeader))
        {
            nObjectSpace += m_poHeader->GetMapObjectSize(poExistingObjHdr->m_nType);
            apoSrcObjHdrs.emplace_back(poExistingObjHdr);
        }

        /* Check that there's really some place that can be recovered */
        if( nObjectSpace < m_poHeader->m_nRegularBlockSize - 20 - m_poCurObjBlock->GetNumUnusedBytes() )
        {
#ifdef DEBUG_VERBOSE
            CPLDebug("MITAB", "Compacting block at offset %d, %d objects valid, recovering %d bytes",
                     m_poCurObjBlock->GetStartAddress(), static_cast<int>(apoSrcObjHdrs.size()),
                     (m_poHeader->m_nRegularBlockSize - 20 - m_poCurObjBlock->GetNumUnusedBytes()) - nObjectSpace);
#endif
            m_poCurObjBlock->ClearObjects();

            for(auto& poSrcObjHdrs: apoSrcObjHdrs)
            {
                /*-----------------------------------------------------------------
                * Prepare and Write ObjHdr to this ObjBlock
                *----------------------------------------------------------------*/
                int nObjPtr = m_poCurObjBlock->PrepareNewObject(poSrcObjHdrs.get());
                if (nObjPtr < 0 ||
                    m_poCurObjBlock->CommitNewObject(poSrcObjHdrs.get()) != 0)
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                            "Failed writing object header for feature id %d",
                            poSrcObjHdrs->m_nId);
                    return -1;
                }

                /*-----------------------------------------------------------------
                * Update .ID Index
                *----------------------------------------------------------------*/
                m_poIdIndex->SetObjPtr(poSrcObjHdrs->m_nId, nObjPtr);
            }
        }
    }

    if (m_poCurObjBlock->GetNumUnusedBytes() >= nObjSize )
    {
        /*-------------------------------------------------------------
         * New object fits in current block, just update the spatial index
         *------------------------------------------------------------*/
        GInt32 nMinX, nMinY, nMaxX, nMaxY;
        m_poCurObjBlock->GetMBR(nMinX, nMinY, nMaxX, nMaxY);

        /* Need to calculate the enlarged MBR that includes new object */
        nMinX = std::min(nMinX, poObjHdr->m_nMinX);
        nMinY = std::min(nMinY, poObjHdr->m_nMinY);
        nMaxX = std::max(nMaxX, poObjHdr->m_nMaxX);
        nMaxY = std::max(nMaxY, poObjHdr->m_nMaxY);

        m_poCurObjBlock->SetMBR(nMinX, nMinY, nMaxX, nMaxY);

        if (m_poSpIndex->UpdateLeafEntry(m_poCurObjBlock->GetStartAddress(),
                                         nMinX, nMinY, nMaxX, nMaxY) != 0)
            return -1;
    }
    else
    {
        /*-------------------------------------------------------------
         * OK, the new object won't fit in the current block, need to split
         * and update index.
         * Split() does its job so that the current obj block will remain
         * the best candidate to receive the new object. It also flushes
         * everything to disk and will update m_poCurCoordBlock to point to
         * the last coord block in the chain, ready to accept new data
         *------------------------------------------------------------*/
        TABMAPObjectBlock *poNewObjBlock
            = SplitObjBlock(poObjHdr, nObjSize);

        if (poNewObjBlock == nullptr)
            return -1;  /* Split failed, error already reported. */

        /*-------------------------------------------------------------
         * Update index with info about m_poCurObjectBlock *first*
         * This is important since UpdateLeafEntry() needs the chain of
         * index nodes preloaded by ChooseLeafEntry() in order to do its job
         *------------------------------------------------------------*/
        GInt32 nMinX = 0;
        GInt32 nMinY = 0;
        GInt32 nMaxX = 0;
        GInt32 nMaxY = 0;
        m_poCurObjBlock->GetMBR(nMinX, nMinY, nMaxX, nMaxY);
        CPLAssert(nMinX <= nMaxX);

        /* Need to calculate the enlarged MBR that includes new object */
        nMinX = std::min(nMinX, poObjHdr->m_nMinX);
        nMinY = std::min(nMinY, poObjHdr->m_nMinY);
        nMaxX = std::max(nMaxX, poObjHdr->m_nMaxX);
        nMaxY = std::max(nMaxY, poObjHdr->m_nMaxY);

        m_poCurObjBlock->SetMBR(nMinX, nMinY, nMaxX, nMaxY);

        if (m_poSpIndex->UpdateLeafEntry(m_poCurObjBlock->GetStartAddress(),
                                         nMinX, nMinY, nMaxX, nMaxY) != 0)
            return -1;

        /*-------------------------------------------------------------
         * Add new obj block to index
         *------------------------------------------------------------*/
        poNewObjBlock->GetMBR(nMinX, nMinY, nMaxX, nMaxY);
        CPLAssert(nMinX <= nMaxX);

        if (m_poSpIndex->AddEntry(nMinX, nMinY, nMaxX, nMaxY,
                                  poNewObjBlock->GetStartAddress()) != 0)
            return -1;
        const int nNextDepth = m_poSpIndex->GetCurMaxDepth() + 1;
        m_poHeader->m_nMaxSpIndexDepth = static_cast<GByte>(
            std::max(static_cast<int>(m_poHeader->m_nMaxSpIndexDepth), nNextDepth));

        /*-------------------------------------------------------------
         * Delete second object block, no need to commit to file first since
         * it is already been committed to disk by Split()
         *------------------------------------------------------------*/
        delete poNewObjBlock;
    }

    return 0;
}

/**********************************************************************
 *                   TABMAPFile::PrepareNewObjViaObjBlock()
 *
 * Used by TABMAPFile::PrepareNewObj() to prepare the current object
 * data block to be ready to write the object to it. If the object block
 * is full then it is inserted in the spatial index and committed to disk,
 * and a new obj block is created.
 *
 * This method is used when "quick spatial index mode" is selected,
 * i.e. faster write, but non-optimal spatial index.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int   TABMAPFile::PrepareNewObjViaObjBlock(TABMAPObjHdr *poObjHdr)
{
    /*-------------------------------------------------------------
     * We will need an object block... check if it exists and
     * create it if it has not been created yet (first time for this file).
     * We do not create the object block in the open() call because
     * files that contained only "NONE" geometries ended up with empty
     * object and spatial index blocks.
     * Note: A coord block will be created only if needed later.
     *------------------------------------------------------------*/
    if (m_poCurObjBlock == nullptr)
    {
        m_poCurObjBlock = new TABMAPObjectBlock(m_eAccessMode);

        int nBlockOffset = m_oBlockManager.AllocNewBlock("OBJECT");

        m_poCurObjBlock->InitNewBlock(m_fp, m_poHeader->m_nRegularBlockSize, nBlockOffset);

        // The reference to the first object block should
        // actually go through the index blocks... this will be
        // updated when file is closed.
        m_poHeader->m_nFirstIndexBlock = nBlockOffset;
    }

    /*-----------------------------------------------------------------
     * Fetch new object size, make sure there is enough room in obj.
     * block for new object, and save/create a new one if necessary.
     *----------------------------------------------------------------*/
    const int nObjSize = m_poHeader->GetMapObjectSize(poObjHdr->m_nType);
    if (m_poCurObjBlock->GetNumUnusedBytes() < nObjSize )
    {
        /*-------------------------------------------------------------
         * OK, the new object won't fit in the current block. Add the
         * current block to the spatial index, commit it to disk and init
         * a new block
         *------------------------------------------------------------*/
        CommitObjAndCoordBlocks(FALSE);

        if (m_poCurObjBlock->InitNewBlock(m_fp,m_poHeader->m_nRegularBlockSize,
                                  m_oBlockManager.AllocNewBlock("OBJECT"))!=0)
            return -1; /* Error already reported */

        /*-------------------------------------------------------------
         * Coord block has been committed to disk but not deleted.
         * Delete it to require the creation of a new coord block chain
         * as needed.
         *-------------------------------------------------------------*/
        if (m_poCurCoordBlock)
        {
            delete m_poCurCoordBlock;
            m_poCurCoordBlock = nullptr;
        }
    }

    return 0;
}

/**********************************************************************
 *                   TABMAPFile::CommitNewObj()
 *
 * Commit object header data to the ObjBlock. Should be called after
 * PrepareNewObj, once all members of the ObjHdr have been set.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int   TABMAPFile::CommitNewObj(TABMAPObjHdr *poObjHdr)
{
    // Nothing to do for NONE objects
    if (poObjHdr->m_nType == TAB_GEOM_NONE)
    {
        return 0;
    }

    /* Update this now so that PrepareCoordBlock() doesn't try to old an older */
    /* block */
    if( m_poCurCoordBlock != nullptr )
        m_poCurObjBlock->AddCoordBlockRef(m_poCurCoordBlock->GetStartAddress());

    /* So that GetExtent() is up-to-date */
    if( m_poSpIndex != nullptr )
    {
        m_poSpIndex->GetMBR(m_poHeader->m_nXMin, m_poHeader->m_nYMin,
                            m_poHeader->m_nXMax, m_poHeader->m_nYMax);
    }

    return m_poCurObjBlock->CommitNewObject(poObjHdr);
}

/**********************************************************************
 *                   TABMAPFile::CommitObjAndCoordBlocks()
 *
 * Commit the TABMAPObjBlock and TABMAPCoordBlock to disk.
 *
 * The objects are deleted from memory if bDeleteObjects==TRUE.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::CommitObjAndCoordBlocks(GBool bDeleteObjects /*=FALSE*/)
{
    int nStatus = 0;

    /*-----------------------------------------------------------------
     * First check that a objBlock has been created.  It is possible to have
     * no object block in files that contain only "NONE" geometries.
     *----------------------------------------------------------------*/
    if (m_poCurObjBlock == nullptr)
        return 0;

    if (m_eAccessMode == TABRead)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "CommitObjAndCoordBlocks() failed: file not opened for write access.");
        return -1;
    }

    if (!m_bLastOpWasWrite)
    {
        if (bDeleteObjects)
        {
            delete m_poCurCoordBlock;
            m_poCurCoordBlock = nullptr;
            delete m_poCurObjBlock;
            m_poCurObjBlock = nullptr;
        }
        return 0;
    }
    m_bLastOpWasWrite = FALSE;

    /*-----------------------------------------------------------------
     * We need to flush the coord block if there was one
     * since a list of coord blocks can belong to only one obj. block
     *----------------------------------------------------------------*/
    if (m_poCurCoordBlock)
    {
        // Update the m_nMaxCoordBufSize member in the header block
        //
        int nTotalCoordSize = m_poCurCoordBlock->GetNumBlocksInChain()*m_poHeader->m_nRegularBlockSize;
        if (nTotalCoordSize > m_poHeader->m_nMaxCoordBufSize)
            m_poHeader->m_nMaxCoordBufSize = nTotalCoordSize;

        // Update the references to this coord block in the MAPObjBlock
        //
        m_poCurObjBlock->AddCoordBlockRef(m_poCurCoordBlock->
                                                         GetStartAddress());
        nStatus = m_poCurCoordBlock->CommitToFile();

        if (bDeleteObjects)
        {
            delete m_poCurCoordBlock;
            m_poCurCoordBlock = nullptr;
        }
    }

    /*-----------------------------------------------------------------
     * Commit the obj block
     *----------------------------------------------------------------*/
    if (nStatus == 0)
    {
        nStatus = m_poCurObjBlock->CommitToFile();
    }

    /*-----------------------------------------------------------------
     * Update the spatial index ** only in "quick spatial index" mode **
     * In the (default) optimized spatial index mode, the spatial index
     * is already maintained up to date as part of inserting the objects in
     * PrepareNewObj().
     *
     * Spatial index will be created here if it was not done yet.
     *----------------------------------------------------------------*/
    if (nStatus == 0 && m_bQuickSpatialIndexMode)
    {
        if (m_poSpIndex == nullptr)
        {
            // Spatial Index not created yet...
            m_poSpIndex = new TABMAPIndexBlock(m_eAccessMode);

            m_poSpIndex->InitNewBlock(m_fp, m_poHeader->m_nRegularBlockSize,
                                      m_oBlockManager.AllocNewBlock("INDEX"));
            m_poSpIndex->SetMAPBlockManagerRef(&m_oBlockManager);

            m_poHeader->m_nFirstIndexBlock = m_poSpIndex->GetNodeBlockPtr();
        }

        GInt32 nXMin, nYMin, nXMax, nYMax;
        m_poCurObjBlock->GetMBR(nXMin, nYMin, nXMax, nYMax);
        nStatus = m_poSpIndex->AddEntry(nXMin, nYMin, nXMax, nYMax,
                                        m_poCurObjBlock->GetStartAddress());

        const int nNextDepth = m_poSpIndex->GetCurMaxDepth() + 1;
        m_poHeader->m_nMaxSpIndexDepth = static_cast<GByte>(
            std::max(static_cast<int>(m_poHeader->m_nMaxSpIndexDepth), nNextDepth));
    }

    /*-----------------------------------------------------------------
     * Delete obj block only if requested
     *----------------------------------------------------------------*/
    if (bDeleteObjects)
    {
        delete m_poCurObjBlock;
        m_poCurObjBlock = nullptr;
    }

    return nStatus;
}

/**********************************************************************
 *                   TABMAPFile::LoadObjAndCoordBlocks()
 *
 * Load the TABMAPObjBlock at specified address and corresponding
 * TABMAPCoordBlock, ready to write new objects to them.
 *
 * It is assumed that pre-existing m_poCurObjBlock and m_poCurCoordBlock
 * have been flushed to disk already using CommitObjAndCoordBlocks()
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::LoadObjAndCoordBlocks(GInt32 nBlockPtr)
{
    /*-----------------------------------------------------------------
     * In Write mode, if an object block is already in memory then flush it
     *----------------------------------------------------------------*/
    if (m_eAccessMode != TABRead && m_poCurObjBlock != nullptr)
    {
        int nStatus = CommitObjAndCoordBlocks(TRUE);
        if (nStatus != 0)
            return nStatus;
    }

    /*-----------------------------------------------------------------
     * Load Obj Block
     *----------------------------------------------------------------*/
    TABRawBinBlock *poBlock =
        TABCreateMAPBlockFromFile(m_fp,
                                  nBlockPtr,
                                  m_poHeader->m_nRegularBlockSize, TRUE, TABReadWrite);
    if (poBlock != nullptr &&
        poBlock->GetBlockClass() == TABMAP_OBJECT_BLOCK)
    {
        m_poCurObjBlock = cpl::down_cast<TABMAPObjectBlock*>(poBlock);
        poBlock = nullptr;
    }
    else
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "LoadObjAndCoordBlocks() failed for object block at %d.",
                 nBlockPtr);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Load the last coord block in the chain
     *----------------------------------------------------------------*/
    if (m_poCurObjBlock->GetLastCoordBlockAddress() == 0)
    {
        m_poCurCoordBlock = nullptr;
        return 0;
    }

    poBlock = TABCreateMAPBlockFromFile(m_fp,
                                   m_poCurObjBlock->GetLastCoordBlockAddress(),
                                   m_poHeader->m_nRegularBlockSize, TRUE, TABReadWrite);
    if (poBlock != nullptr && poBlock->GetBlockClass() == TABMAP_COORD_BLOCK)
    {
        m_poCurCoordBlock = cpl::down_cast<TABMAPCoordBlock *>(poBlock);
        m_poCurCoordBlock->SetMAPBlockManagerRef(&m_oBlockManager);
        poBlock = nullptr;
    }
    else
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "LoadObjAndCoordBlocks() failed for coord block at %d.",
                 m_poCurObjBlock->GetLastCoordBlockAddress());
        return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABMAPFile::SplitObjBlock()
 *
 * Split m_poCurObjBlock using Guttman algorithm.
 *
 * SplitObjBlock() doe its job so that the current obj block will remain
 * the best candidate to receive the new object to add. It also flushes
 * everything to disk and will update m_poCurCoordBlock to point to the
 * last coord block in the chain, ready to accept new data
 *
 * Updates to the spatial index are left to the caller.
 *
 * Returns the TABMAPObjBlock of the second block for use by the caller
 * in updating the spatial index, or NULL in case of error.
 **********************************************************************/
TABMAPObjectBlock *TABMAPFile::SplitObjBlock(TABMAPObjHdr *poObjHdrToAdd,
                                             int nSizeOfObjToAdd)
{
    std::vector<std::unique_ptr<TABMAPObjHdr>> apoSrcObjHdrs;

    /*-----------------------------------------------------------------
     * Read all object headers
     *----------------------------------------------------------------*/
    m_poCurObjBlock->Rewind();
    while (auto poObjHdr = TABMAPObjHdr::ReadNextObj(m_poCurObjBlock,
                                                     m_poHeader))
    {
        apoSrcObjHdrs.emplace_back(poObjHdr);
    }
    /* PickSeedsForSplit (reasonably) assumes at least 2 nodes */
    CPLAssert(apoSrcObjHdrs.size() > 1);

    /*-----------------------------------------------------------------
     * Reset current obj and coord block
     *----------------------------------------------------------------*/
    GInt32 nFirstSrcCoordBlock = m_poCurObjBlock->GetFirstCoordBlockAddress();

    m_poCurObjBlock->InitNewBlock(m_fp, m_poHeader->m_nRegularBlockSize,
                                  m_poCurObjBlock->GetStartAddress());

    std::unique_ptr<TABMAPCoordBlock> poSrcCoordBlock(m_poCurCoordBlock);
    m_poCurCoordBlock = nullptr;

    /*-----------------------------------------------------------------
     * Create new obj and coord block
     *----------------------------------------------------------------*/
    auto poNewObjBlock = cpl::make_unique<TABMAPObjectBlock>(m_eAccessMode);
    poNewObjBlock->InitNewBlock(m_fp, m_poHeader->m_nRegularBlockSize, m_oBlockManager.AllocNewBlock("OBJECT"));

    /* Use existing center of other block in case we have compressed objects
       and freeze it */
    poNewObjBlock->SetCenterFromOtherBlock(m_poCurObjBlock);

    /* Coord block will be alloc'd automatically*/
    TABMAPCoordBlock *poNewCoordBlock = nullptr;

    /*-----------------------------------------------------------------
     * Pick Seeds for each block
     *----------------------------------------------------------------*/
    std::vector<TABMAPIndexEntry> asSrcEntries;
    asSrcEntries.reserve(apoSrcObjHdrs.size());
    for (const auto& poSrcObjHdrs: apoSrcObjHdrs)
    {
        TABMAPIndexEntry sEntry;
        sEntry.nBlockPtr = 0;
        sEntry.XMin = poSrcObjHdrs->m_nMinX;
        sEntry.YMin = poSrcObjHdrs->m_nMinY;
        sEntry.XMax = poSrcObjHdrs->m_nMaxX;
        sEntry.YMax = poSrcObjHdrs->m_nMaxY;
        asSrcEntries.emplace_back(sEntry);
    }

    int nSeed1, nSeed2;
    TABMAPIndexBlock::PickSeedsForSplit(asSrcEntries.data(),
                                        static_cast<int>(asSrcEntries.size()),
                                        -1,
                                        poObjHdrToAdd->m_nMinX,
                                        poObjHdrToAdd->m_nMinY,
                                        poObjHdrToAdd->m_nMaxX,
                                        poObjHdrToAdd->m_nMaxY,
                                        nSeed1, nSeed2);

    /*-----------------------------------------------------------------
     * Assign the seeds to their respective block
     *----------------------------------------------------------------*/
    // Insert nSeed1 in this block
    if (MoveObjToBlock(apoSrcObjHdrs[nSeed1].get(), poSrcCoordBlock.get(),
                       m_poCurObjBlock, &m_poCurCoordBlock) <= 0)
    {
        return nullptr;
    }

    // Move nSeed2 to 2nd block
    if (MoveObjToBlock(apoSrcObjHdrs[nSeed2].get(), poSrcCoordBlock.get(),
                       poNewObjBlock.get(), &poNewCoordBlock) <= 0)
    {
        return nullptr;
    }

    /*-----------------------------------------------------------------
     * Go through the rest of the entries and assign them to one
     * of the 2 blocks
     *
     * Criteria is minimal area difference.
     * Resolve ties by adding the entry to the block with smaller total
     * area, then to the one with fewer entries, then to either.
     *----------------------------------------------------------------*/
    for(int iEntry=0; iEntry<static_cast<int>(apoSrcObjHdrs.size()); iEntry++)
    {
        if (iEntry == nSeed1 || iEntry == nSeed2)
            continue;

        TABMAPObjHdr* poObjHdr = apoSrcObjHdrs[iEntry].get();

        int nObjSize = m_poHeader->GetMapObjectSize(poObjHdr->m_nType);

        // If one of the two blocks is almost full then all remaining
        // entries should go to the other block
        if (m_poCurObjBlock->GetNumUnusedBytes() < nObjSize+nSizeOfObjToAdd )
        {
            if (MoveObjToBlock(poObjHdr, poSrcCoordBlock.get(),
                               poNewObjBlock.get(), &poNewCoordBlock) <= 0)
                return nullptr;
            continue;
        }
        else if (poNewObjBlock->GetNumUnusedBytes() < nObjSize+nSizeOfObjToAdd)
        {
            if (MoveObjToBlock(poObjHdr, poSrcCoordBlock.get(),
                               m_poCurObjBlock, &m_poCurCoordBlock) <= 0)
                return nullptr;
            continue;
        }

        // Decide which of the two blocks to put this entry in
        GInt32 nXMin, nYMin, nXMax, nYMax;
        m_poCurObjBlock->GetMBR(nXMin, nYMin, nXMax, nYMax);
        CPLAssert( nXMin <= nXMax );
        double dAreaDiff1 =
            TABMAPIndexBlock::ComputeAreaDiff(nXMin, nYMin,
                                              nXMax, nYMax,
                                              poObjHdr->m_nMinX,
                                              poObjHdr->m_nMinY,
                                              poObjHdr->m_nMaxX,
                                              poObjHdr->m_nMaxY);

        poNewObjBlock->GetMBR(nXMin, nYMin, nXMax, nYMax);
        CPLAssert( nXMin <= nXMax );
        double dAreaDiff2 =
            TABMAPIndexBlock::ComputeAreaDiff(nXMin, nYMin, nXMax, nYMax,
                                              poObjHdr->m_nMinX,
                                              poObjHdr->m_nMinY,
                                              poObjHdr->m_nMaxX,
                                              poObjHdr->m_nMaxY);

        if (dAreaDiff1 < dAreaDiff2)
        {
            // This entry stays in this block
            if (MoveObjToBlock(poObjHdr, poSrcCoordBlock.get(),
                               m_poCurObjBlock, &m_poCurCoordBlock) <= 0)
                return nullptr;
        }
        else
        {
            // This entry goes to new block
            if (MoveObjToBlock(poObjHdr, poSrcCoordBlock.get(),
                               poNewObjBlock.get(), &poNewCoordBlock) <= 0)
                return nullptr;
        }
    }

    /*-----------------------------------------------------------------
     * Delete second coord block if one was created
     * Refs to coord block were kept up to date by MoveObjToBlock()
     * We just need to commit to file and delete the object now.
     *----------------------------------------------------------------*/
    if (poNewCoordBlock)
    {
        if (poNewCoordBlock->CommitToFile() != 0)
        {
            return nullptr;
        }
        delete poNewCoordBlock;
    }

    /*-----------------------------------------------------------------
     * Release unused coord. data blocks
     *----------------------------------------------------------------*/
    if (poSrcCoordBlock)
    {
        if (poSrcCoordBlock->GetStartAddress() != nFirstSrcCoordBlock)
        {
            if (poSrcCoordBlock->GotoByteInFile(nFirstSrcCoordBlock, TRUE) != 0)
            {
                return nullptr;
            }
        }

        int nNextCoordBlock = poSrcCoordBlock->GetNextCoordBlock();
        while(poSrcCoordBlock != nullptr)
        {
            // Mark this block as deleted
            if (poSrcCoordBlock->CommitAsDeleted(m_oBlockManager.
                                                 GetFirstGarbageBlock()) != 0)
            {
                return nullptr;
            }
            m_oBlockManager.PushGarbageBlockAsFirst(poSrcCoordBlock->GetStartAddress());

            // Advance to next
            if (nNextCoordBlock > 0)
            {
                if (poSrcCoordBlock->GotoByteInFile(nNextCoordBlock, TRUE) != 0)
                    return nullptr;

                nNextCoordBlock = poSrcCoordBlock->GetNextCoordBlock();
            }
            else
            {
                // end of chain
                poSrcCoordBlock.reset();
            }
        }
    }

    if (poNewObjBlock->CommitToFile() != 0)
        return nullptr;

    return poNewObjBlock.release();
}

/**********************************************************************
 *                   TABMAPFile::MoveObjToBlock()
 *
 * Moves an object and its coord data to a new ObjBlock. Used when
 * splitting Obj Blocks.
 *
 * May update the value of ppoCoordBlock if a new coord block had to
 * be created.
 *
 * Returns the address where new object is stored on success, -1 on error.
 **********************************************************************/
int TABMAPFile::MoveObjToBlock(TABMAPObjHdr       *poObjHdr,
                               TABMAPCoordBlock   *poSrcCoordBlock,
                               TABMAPObjectBlock  *poDstObjBlock,
                               TABMAPCoordBlock   **ppoDstCoordBlock)
{
    /*-----------------------------------------------------------------
     * Copy Coord data if applicable
     * We use a temporary TABFeature object to handle the reading/writing
     * of coord block data.
     *----------------------------------------------------------------*/
    if (m_poHeader->MapObjectUsesCoordBlock(poObjHdr->m_nType))
    {
        TABMAPObjHdrWithCoord *poObjHdrCoord =cpl::down_cast<TABMAPObjHdrWithCoord*>(poObjHdr);
        OGRFeatureDefn * poDummyDefn = new OGRFeatureDefn;
        // Ref count defaults to 0... set it to 1
        poDummyDefn->Reference();

        TABFeature *poFeature =
            TABFeature::CreateFromMapInfoType(poObjHdr->m_nType, poDummyDefn);

        if (PrepareCoordBlock(poObjHdrCoord->m_nType,
                              poDstObjBlock, ppoDstCoordBlock) != 0)
            return -1;

        GInt32 nSrcCoordPtr = poObjHdrCoord->m_nCoordBlockPtr;

        /* Copy Coord data
         * poObjHdrCoord->m_nCoordBlockPtr will be set by WriteGeometry...
         * We pass second arg to GotoByteInFile() to force reading from file
         * if nSrcCoordPtr is not in current block
         */
        if (poSrcCoordBlock->GotoByteInFile(nSrcCoordPtr, TRUE) != 0 ||
            poFeature->ReadGeometryFromMAPFile(this, poObjHdr,
                                               TRUE /* bCoordDataOnly */,
                                               &poSrcCoordBlock) != 0 ||
            poFeature->WriteGeometryToMAPFile(this, poObjHdr,
                                              TRUE /* bCoordDataOnly */,
                                              ppoDstCoordBlock) != 0)
        {
            delete poFeature;
            delete poDummyDefn;
            return -1;
        }

        // Update the references to dest coord block in the MAPObjBlock
        // in case new block has been alloc'd since PrepareCoordBlock()
        //
        poDstObjBlock->AddCoordBlockRef((*ppoDstCoordBlock)->GetStartAddress());
        /* Cleanup */
        delete poFeature;
        poDummyDefn->Release();
    }

    /*-----------------------------------------------------------------
     * Prepare and Write ObjHdr to this ObjBlock
     *----------------------------------------------------------------*/
    int nObjPtr = poDstObjBlock->PrepareNewObject(poObjHdr);
    if (nObjPtr < 0 ||
        poDstObjBlock->CommitNewObject(poObjHdr) != 0)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Failed writing object header for feature id %d",
                 poObjHdr->m_nId);
        return -1;
    }

    /*-----------------------------------------------------------------
     * Update .ID Index
     *----------------------------------------------------------------*/
    m_poIdIndex->SetObjPtr(poObjHdr->m_nId, nObjPtr);

    return nObjPtr;
}

/**********************************************************************
 *                   TABMAPFile::PrepareCoordBlock()
 *
 * Prepare the coord block to receive an object of specified type if one
 * is needed, and update corresponding members in ObjBlock.
 *
 * May update the value of ppoCoordBlock and Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::PrepareCoordBlock(int nObjType,
                                  TABMAPObjectBlock *poObjBlock,
                                  TABMAPCoordBlock  **ppoCoordBlock)
{

    /*-----------------------------------------------------------------
     * Prepare Coords block...
     * create a new TABMAPCoordBlock if it was not done yet.
     * Note that in write mode, TABCollections require read/write access
     * to the coord block.
     *----------------------------------------------------------------*/
    if (m_poHeader->MapObjectUsesCoordBlock(nObjType))
    {
        if (*ppoCoordBlock == nullptr)
        {
            *ppoCoordBlock = new TABMAPCoordBlock(m_eAccessMode==TABWrite?
                                                  TABReadWrite:
                                                  m_eAccessMode);
            (*ppoCoordBlock)->InitNewBlock(m_fp, m_poHeader->m_nRegularBlockSize,
                                           m_oBlockManager.AllocNewBlock("COORD"));
            (*ppoCoordBlock)->SetMAPBlockManagerRef(&m_oBlockManager);

            // Set the references to this coord block in the MAPObjBlock
            poObjBlock->AddCoordBlockRef((*ppoCoordBlock)->GetStartAddress());
        }
        /* If we are not at the end of the chain of coordinate blocks, then */
        /* reload us */
        else if( (*ppoCoordBlock)->GetStartAddress() != poObjBlock->GetLastCoordBlockAddress() )
        {
            TABRawBinBlock* poBlock = TABCreateMAPBlockFromFile(m_fp,
                                    poObjBlock->GetLastCoordBlockAddress(),
                                    m_poHeader->m_nRegularBlockSize, TRUE, TABReadWrite);
            if (poBlock != nullptr && poBlock->GetBlockClass() == TABMAP_COORD_BLOCK)
            {
                delete *ppoCoordBlock;
                *ppoCoordBlock = cpl::down_cast<TABMAPCoordBlock *>(poBlock);
                (*ppoCoordBlock)->SetMAPBlockManagerRef(&m_oBlockManager);
            }
            else
            {
                delete poBlock;
                CPLError(CE_Failure, CPLE_FileIO,
                            "LoadObjAndCoordBlocks() failed for coord block at %d.",
                            poObjBlock->GetLastCoordBlockAddress());
                return -1;
            }
        }

        if ((*ppoCoordBlock)->GetNumUnusedBytes() < 4)
        {
            int nNewBlockOffset = m_oBlockManager.AllocNewBlock("COORD");
            (*ppoCoordBlock)->SetNextCoordBlock(nNewBlockOffset);
            CPL_IGNORE_RET_VAL((*ppoCoordBlock)->CommitToFile());
            (*ppoCoordBlock)->InitNewBlock(m_fp, m_poHeader->m_nRegularBlockSize, nNewBlockOffset);
            poObjBlock->AddCoordBlockRef((*ppoCoordBlock)->GetStartAddress());
        }

        // Make sure read/write pointer is at the end of the block
        (*ppoCoordBlock)->SeekEnd();

        if (CPLGetLastErrorType() == CE_Failure)
            return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABMAPFile::GetCurObjType()
 *
 * Return the MapInfo object type of the object that the m_poCurObjBlock
 * is pointing to.  This value is set after a call to MoveToObjId().
 *
 * Returns a value >= 0 on success, -1 on error.
 **********************************************************************/
TABGeomType TABMAPFile::GetCurObjType()
{
    return m_nCurObjType;
}

/**********************************************************************
 *                   TABMAPFile::GetCurObjId()
 *
 * Return the MapInfo object id of the object that the m_poCurObjBlock
 * is pointing to.  This value is set after a call to MoveToObjId().
 *
 * Returns a value >= 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::GetCurObjId()
{
    return m_nCurObjId;
}

/**********************************************************************
 *                   TABMAPFile::GetCurObjBlock()
 *
 * Return the m_poCurObjBlock.  If MoveToObjId() has previously been
 * called then m_poCurObjBlock points to the beginning of the current
 * object data.
 *
 * Returns a reference to an object owned by this TABMAPFile object, or
 * NULL on error.
 **********************************************************************/
TABMAPObjectBlock *TABMAPFile::GetCurObjBlock()
{
    return m_poCurObjBlock;
}

/**********************************************************************
 *                   TABMAPFile::GetCurCoordBlock()
 *
 * Return the m_poCurCoordBlock.  This function should be used after
 * PrepareNewObj() to get the reference to the coord block that has
 * just been initialized.
 *
 * Returns a reference to an object owned by this TABMAPFile object, or
 * NULL on error.
 **********************************************************************/
TABMAPCoordBlock *TABMAPFile::GetCurCoordBlock()
{
    return m_poCurCoordBlock;
}

/**********************************************************************
 *                   TABMAPFile::GetCoordBlock()
 *
 * Return a TABMAPCoordBlock object ready to read coordinates from it.
 * The block that contains nFileOffset will automatically be
 * loaded, and if nFileOffset is the beginning of a new block then the
 * pointer will be moved to the beginning of the data.
 *
 * The contents of the returned object is only valid until the next call
 * to GetCoordBlock().
 *
 * Returns a reference to an object owned by this TABMAPFile object, or
 * NULL on error.
 **********************************************************************/
TABMAPCoordBlock *TABMAPFile::GetCoordBlock(int nFileOffset)
{
    if (m_poCurCoordBlock == nullptr)
    {
        m_poCurCoordBlock = new TABMAPCoordBlock(m_eAccessMode);
        m_poCurCoordBlock->InitNewBlock(m_fp, m_poHeader->m_nRegularBlockSize);
        m_poCurCoordBlock->SetMAPBlockManagerRef(&m_oBlockManager);
    }

    /*-----------------------------------------------------------------
     * Use GotoByteInFile() to go to the requested location.  This will
     * force loading the block if necessary and reading its header.
     * If nFileOffset is at the beginning of the requested block, then
     * we make sure to move the read pointer past the 8 bytes header
     * to be ready to read coordinates data
     *----------------------------------------------------------------*/
    if ( m_poCurCoordBlock->GotoByteInFile(nFileOffset, TRUE) != 0)
    {
        // Failed... an error has already been reported.
        return nullptr;
    }

    if (nFileOffset % m_poHeader->m_nRegularBlockSize == 0)
        m_poCurCoordBlock->GotoByteInBlock(8);      // Skip Header

    return m_poCurCoordBlock;
}

/**********************************************************************
 *                   TABMAPFile::GetHeaderBlock()
 *
 * Return a reference to the MAP file's header block.
 *
 * The returned pointer is a reference to an object owned by this TABMAPFile
 * object and should not be deleted by the caller.
 *
 * Return NULL if file has not been opened yet.
 **********************************************************************/
TABMAPHeaderBlock *TABMAPFile::GetHeaderBlock()
{
    return m_poHeader;
}

/**********************************************************************
 *                   TABMAPFile::GetIDFileRef()
 *
 * Return a reference to the .ID file attached to this .MAP file
 *
 * The returned pointer is a reference to an object owned by this TABMAPFile
 * object and should not be deleted by the caller.
 *
 * Return NULL if file has not been opened yet.
 **********************************************************************/
TABIDFile *TABMAPFile::GetIDFileRef()
{
    return m_poIdIndex;
}

/**********************************************************************
 *                   TABMAPFile::GetIndexBlock()
 *
 * Return a reference to the requested index or object block..
 *
 * Ownership of the returned block is turned over to the caller, who should
 * delete it when no longer needed.  The type of the block can be determined
 * with the GetBlockType() method.
 *
 * @param nFileOffset the offset in the map file of the spatial index
 * block or object block to load.
 *
 * @return The requested TABMAPIndexBlock, TABMAPObjectBlock or NULL if the
 * read fails for some reason.
 **********************************************************************/
TABRawBinBlock *TABMAPFile::GetIndexObjectBlock( int nFileOffset )
{
    /*----------------------------------------------------------------
     * Read from the file
     *---------------------------------------------------------------*/
    GByte* pabyData = static_cast<GByte*>(CPLMalloc(m_poHeader->m_nRegularBlockSize));

    if (VSIFSeekL(m_fp, nFileOffset, SEEK_SET) != 0
        || static_cast<int>(VSIFReadL(pabyData, sizeof(GByte), m_poHeader->m_nRegularBlockSize, m_fp)) !=
                        m_poHeader->m_nRegularBlockSize )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "GetIndexBlock() failed reading %d bytes at offset %d.",
                 m_poHeader->m_nRegularBlockSize, nFileOffset);
        CPLFree(pabyData);
        return nullptr;
    }

/* -------------------------------------------------------------------- */
/*      Create and initialize depending on the block type.              */
/* -------------------------------------------------------------------- */
    int nBlockType = pabyData[0];
    TABRawBinBlock *poBlock = nullptr;

    if( nBlockType == TABMAP_INDEX_BLOCK )
    {
        TABMAPIndexBlock* poIndexBlock = new TABMAPIndexBlock(m_eAccessMode);
        poBlock = poIndexBlock;
        poIndexBlock->SetMAPBlockManagerRef(&m_oBlockManager);
    }
    else
        poBlock = new TABMAPObjectBlock(m_eAccessMode);

    poBlock->InitBlockFromData(pabyData,
                                   m_poHeader->m_nRegularBlockSize,
                                   m_poHeader->m_nRegularBlockSize,
                                   FALSE, m_fp, nFileOffset);

    return poBlock;
}

/**********************************************************************
 *                   TABMAPFile::InitDrawingTools()
 *
 * Init the drawing tools for this file.
 *
 * In Read mode, this will load the drawing tools from the file.
 *
 * In Write mode, this function will init an empty the tool def table.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::InitDrawingTools()
{
    int nStatus = 0;

    if (m_poHeader == nullptr)
        return -1;    // File not opened yet!

    /*-------------------------------------------------------------
     * We want to perform this initialization only once
     *------------------------------------------------------------*/
    if (m_poToolDefTable != nullptr)
        return 0;

    /*-------------------------------------------------------------
     * Create a new ToolDefTable... no more initialization is required
     * unless we want to read tool blocks from file.
     *------------------------------------------------------------*/
    m_poToolDefTable = new TABToolDefTable;

    if ((m_eAccessMode == TABRead || m_eAccessMode == TABReadWrite) &&
        m_poHeader->m_nFirstToolBlock != 0)
    {
        TABMAPToolBlock *poBlock = new TABMAPToolBlock(TABRead);
        poBlock->InitNewBlock(m_fp, m_poHeader->m_nRegularBlockSize);

        /*-------------------------------------------------------------
         * Use GotoByteInFile() to go to the first block's location.  This will
         * force loading the block if necessary and reading its header.
         * Also make sure to move the read pointer past the 8 bytes header
         * to be ready to read drawing tools data
         *------------------------------------------------------------*/
        if ( poBlock->GotoByteInFile(m_poHeader->m_nFirstToolBlock)!= 0)
        {
            // Failed... an error has already been reported.
            delete poBlock;
            return -1;
        }

        poBlock->GotoByteInBlock(8);

        nStatus = m_poToolDefTable->ReadAllToolDefs(poBlock);
        delete poBlock;
    }

    return nStatus;
}

/**********************************************************************
 *                   TABMAPFile::CommitDrawingTools()
 *
 * Write the drawing tools for this file.
 *
 * This function applies only to write access mode.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::CommitDrawingTools()
{
    int nStatus = 0;

    if (m_eAccessMode == TABRead || m_poHeader == nullptr)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
            "CommitDrawingTools() failed: file not opened for write access.");
        return -1;
    }

    if (m_poToolDefTable == nullptr ||
        (m_poToolDefTable->GetNumPen() +
         m_poToolDefTable->GetNumBrushes() +
         m_poToolDefTable->GetNumFonts() +
         m_poToolDefTable->GetNumSymbols()) == 0)
    {
        return 0;       // Nothing to do!
    }

    /*-------------------------------------------------------------
     * Create a new TABMAPToolBlock and update header fields
     *------------------------------------------------------------*/
    TABMAPToolBlock *poBlock = new TABMAPToolBlock(m_eAccessMode);
    if( m_poHeader->m_nFirstToolBlock != 0 )
        poBlock->InitNewBlock(m_fp, m_poHeader->m_nRegularBlockSize, m_poHeader->m_nFirstToolBlock);
    else
        poBlock->InitNewBlock(m_fp, m_poHeader->m_nRegularBlockSize, m_oBlockManager.AllocNewBlock("TOOL"));
    poBlock->SetMAPBlockManagerRef(&m_oBlockManager);

    m_poHeader->m_nFirstToolBlock = poBlock->GetStartAddress();

    m_poHeader->m_numPenDefs = static_cast<GByte>(m_poToolDefTable->GetNumPen());
    m_poHeader->m_numBrushDefs = static_cast<GByte>(m_poToolDefTable->GetNumBrushes());
    m_poHeader->m_numFontDefs = static_cast<GByte>(m_poToolDefTable->GetNumFonts());
    m_poHeader->m_numSymbolDefs = static_cast<GByte>(m_poToolDefTable->GetNumSymbols());

    /*-------------------------------------------------------------
     * Do the actual work and delete poBlock
     * (Note that poBlock will have already been committed to the file
     * by WriteAllToolDefs() )
     *------------------------------------------------------------*/
    nStatus = m_poToolDefTable->WriteAllToolDefs(poBlock);

    m_poHeader->m_numMapToolBlocks = static_cast<GByte>(poBlock->GetNumBlocksInChain());

    delete poBlock;

    return nStatus;
}

/**********************************************************************
 *                   TABMAPFile::ReadPenDef()
 *
 * Fill the TABPenDef structure with the definition of the specified pen
 * index... (1-based pen index)
 *
 * If nPenIndex==0 or is invalid, then the structure is cleared.
 *
 * Returns 0 on success, -1 on error (i.e. Pen not found).
 **********************************************************************/
int   TABMAPFile::ReadPenDef(int nPenIndex, TABPenDef *psDef)
{
    if (m_poToolDefTable == nullptr && InitDrawingTools() != 0)
        return -1;

    TABPenDef *psTmp = nullptr;
    if (psDef && m_poToolDefTable &&
        (psTmp = m_poToolDefTable->GetPenDefRef(nPenIndex)) != nullptr)
    {
        *psDef = *psTmp;
    }
    else if (psDef)
    {
        /* Init to MapInfo default */
        static const TABPenDef csDefaultPen = MITAB_PEN_DEFAULT;
        *psDef = csDefaultPen;
        return -1;
    }
    return 0;
}

/**********************************************************************
 *                   TABMAPFile::WritePenDef()
 *
 * Write a Pen Tool to the map file and return the pen index that has
 * been attributed to this Pen tool definition, or -1 if something went
 * wrong
 *
 * Note that the returned index is a 1-based index.  A value of 0
 * indicates "none" in MapInfo.

 * Returns a value >= 0 on success, -1 on error
 **********************************************************************/
int   TABMAPFile::WritePenDef(TABPenDef *psDef)
{
    if (psDef == nullptr ||
        (m_poToolDefTable == nullptr && InitDrawingTools() != 0) ||
        m_poToolDefTable==nullptr )
    {
        return -1;
    }

    return m_poToolDefTable->AddPenDefRef(psDef);
}

/**********************************************************************
 *                   TABMAPFile::ReadBrushDef()
 *
 * Fill the TABBrushDef structure with the definition of the specified Brush
 * index... (1-based Brush index)
 *
 * If nBrushIndex==0 or is invalid, then the structure is cleared.
 *
 * Returns 0 on success, -1 on error (i.e. Brush not found).
 **********************************************************************/
int   TABMAPFile::ReadBrushDef(int nBrushIndex, TABBrushDef *psDef)
{
    if (m_poToolDefTable == nullptr && InitDrawingTools() != 0)
        return -1;

    TABBrushDef *psTmp = nullptr;
    if (psDef && m_poToolDefTable &&
        (psTmp = m_poToolDefTable->GetBrushDefRef(nBrushIndex)) != nullptr)
    {
        *psDef = *psTmp;
    }
    else if (psDef)
    {
        /* Init to MapInfo default */
        static const TABBrushDef csDefaultBrush = MITAB_BRUSH_DEFAULT;
        *psDef = csDefaultBrush;
        return -1;
    }
    return 0;
}

/**********************************************************************
 *                   TABMAPFile::WriteBrushDef()
 *
 * Write a Brush Tool to the map file and return the Brush index that has
 * been attributed to this Brush tool definition, or -1 if something went
 * wrong
 *
 * Note that the returned index is a 1-based index.  A value of 0
 * indicates "none" in MapInfo.

 * Returns a value >= 0 on success, -1 on error
 **********************************************************************/
int   TABMAPFile::WriteBrushDef(TABBrushDef *psDef)
{
    if (psDef == nullptr ||
        (m_poToolDefTable == nullptr && InitDrawingTools() != 0) ||
        m_poToolDefTable==nullptr )
    {
        return -1;
    }

    return m_poToolDefTable->AddBrushDefRef(psDef);
}

/**********************************************************************
 *                   TABMAPFile::ReadFontDef()
 *
 * Fill the TABFontDef structure with the definition of the specified Font
 * index... (1-based Font index)
 *
 * If nFontIndex==0 or is invalid, then the structure is cleared.
 *
 * Returns 0 on success, -1 on error (i.e. Font not found).
 **********************************************************************/
int   TABMAPFile::ReadFontDef(int nFontIndex, TABFontDef *psDef)
{
    if (m_poToolDefTable == nullptr && InitDrawingTools() != 0)
        return -1;

    TABFontDef *psTmp = nullptr;
    if (psDef && m_poToolDefTable &&
        (psTmp = m_poToolDefTable->GetFontDefRef(nFontIndex)) != nullptr)
    {
        *psDef = *psTmp;
    }
    else if (psDef)
    {
        /* Init to MapInfo default */
        static const TABFontDef csDefaultFont = MITAB_FONT_DEFAULT;
        *psDef = csDefaultFont;
        return -1;
    }
    return 0;
}

/**********************************************************************
 *                   TABMAPFile::WriteFontDef()
 *
 * Write a Font Tool to the map file and return the Font index that has
 * been attributed to this Font tool definition, or -1 if something went
 * wrong
 *
 * Note that the returned index is a 1-based index.  A value of 0
 * indicates "none" in MapInfo.

 * Returns a value >= 0 on success, -1 on error
 **********************************************************************/
int   TABMAPFile::WriteFontDef(TABFontDef *psDef)
{
    if (psDef == nullptr ||
        (m_poToolDefTable == nullptr && InitDrawingTools() != 0) ||
        m_poToolDefTable==nullptr )
    {
        return -1;
    }

    return m_poToolDefTable->AddFontDefRef(psDef);
}

/**********************************************************************
 *                   TABMAPFile::ReadSymbolDef()
 *
 * Fill the TABSymbolDef structure with the definition of the specified Symbol
 * index... (1-based Symbol index)
 *
 * If nSymbolIndex==0 or is invalid, then the structure is cleared.
 *
 * Returns 0 on success, -1 on error (i.e. Symbol not found).
 **********************************************************************/
int   TABMAPFile::ReadSymbolDef(int nSymbolIndex, TABSymbolDef *psDef)
{
    if (m_poToolDefTable == nullptr && InitDrawingTools() != 0)
        return -1;

    TABSymbolDef *psTmp = nullptr;
    if (psDef && m_poToolDefTable &&
        (psTmp = m_poToolDefTable->GetSymbolDefRef(nSymbolIndex)) != nullptr)
    {
        *psDef = *psTmp;
    }
    else if (psDef)
    {
        /* Init to MapInfo default */
        static const TABSymbolDef csDefaultSymbol = MITAB_SYMBOL_DEFAULT;
        *psDef = csDefaultSymbol;
        return -1;
    }
    return 0;
}

/**********************************************************************
 *                   TABMAPFile::WriteSymbolDef()
 *
 * Write a Symbol Tool to the map file and return the Symbol index that has
 * been attributed to this Symbol tool definition, or -1 if something went
 * wrong
 *
 * Note that the returned index is a 1-based index.  A value of 0
 * indicates "none" in MapInfo.

 * Returns a value >= 0 on success, -1 on error
 **********************************************************************/
int   TABMAPFile::WriteSymbolDef(TABSymbolDef *psDef)
{
    if (psDef == nullptr ||
        (m_poToolDefTable == nullptr && InitDrawingTools() != 0) ||
        m_poToolDefTable==nullptr )
    {
        return -1;
    }

    return m_poToolDefTable->AddSymbolDefRef(psDef);
}

static void ORDER_MIN_MAX( double &min, double &max )
{
    if( max < min )
      std::swap(min, max);
}

static void ORDER_MIN_MAX( int &min, int &max )
{
    if( max < min )
      std::swap(min, max);
}

/**********************************************************************
 *                   TABMAPFile::SetCoordFilter()
 *
 * Set the MBR of the area of interest... only objects that at least
 * overlap with that area will be returned.
 *
 * @param sMin minimum x/y the file's projection coord.
 * @param sMax maximum x/y the file's projection coord.
 **********************************************************************/
void TABMAPFile::SetCoordFilter(TABVertex sMin, TABVertex sMax)
{
    m_sMinFilter = sMin;
    m_sMaxFilter = sMax;

    Coordsys2Int(sMin.x, sMin.y, m_XMinFilter, m_YMinFilter, TRUE);
    Coordsys2Int(sMax.x, sMax.y, m_XMaxFilter, m_YMaxFilter, TRUE);

    ORDER_MIN_MAX(m_XMinFilter, m_XMaxFilter);
    ORDER_MIN_MAX(m_YMinFilter, m_YMaxFilter);
    ORDER_MIN_MAX(m_sMinFilter.x, m_sMaxFilter.x);
    ORDER_MIN_MAX(m_sMinFilter.y, m_sMaxFilter.y);
}

/**********************************************************************
 *                   TABMAPFile::ResetCoordFilter()
 *
 * Reset the MBR of the area of interest to be the extents as defined
 * in the header.
 **********************************************************************/

void TABMAPFile::ResetCoordFilter()

{
    m_XMinFilter = m_poHeader->m_nXMin;
    m_YMinFilter = m_poHeader->m_nYMin;
    m_XMaxFilter = m_poHeader->m_nXMax;
    m_YMaxFilter = m_poHeader->m_nYMax;
    Int2Coordsys(m_XMinFilter, m_YMinFilter,
                 m_sMinFilter.x, m_sMinFilter.y);
    Int2Coordsys(m_XMaxFilter, m_YMaxFilter,
                 m_sMaxFilter.x, m_sMaxFilter.y);

    ORDER_MIN_MAX(m_XMinFilter, m_XMaxFilter);
    ORDER_MIN_MAX(m_YMinFilter, m_YMaxFilter);
    ORDER_MIN_MAX(m_sMinFilter.x, m_sMaxFilter.x);
    ORDER_MIN_MAX(m_sMinFilter.y, m_sMaxFilter.y);
}

/**********************************************************************
 *                   TABMAPFile::GetCoordFilter()
 *
 * Get the MBR of the area of interest, as previously set by
 * SetCoordFilter().
 *
 * @param sMin vertex into which the minimum x/y values put in coordsys space.
 * @param sMax vertex into which the maximum x/y values put in coordsys space.
 **********************************************************************/
void TABMAPFile::GetCoordFilter(TABVertex &sMin, TABVertex &sMax) const
{
    sMin = m_sMinFilter;
    sMax = m_sMaxFilter;
}

/**********************************************************************
 *                   TABMAPFile::CommitSpatialIndex()
 *
 * Write the spatial index blocks tree for this file.
 *
 * This function applies only to write access mode.
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::CommitSpatialIndex()
{
    if (m_eAccessMode == TABRead || m_poHeader == nullptr)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
            "CommitSpatialIndex() failed: file not opened for write access.");
        return -1;
    }

    if (m_poSpIndex == nullptr)
    {
        return 0;       // Nothing to do!
    }

    /*-------------------------------------------------------------
     * Update header fields and commit index block
     * (its children will be recursively committed as well)
     *------------------------------------------------------------*/
    // Add 1 to Spatial Index Depth to account to the MapObjectBlocks
    const int nNextDepth = m_poSpIndex->GetCurMaxDepth() + 1;
    m_poHeader->m_nMaxSpIndexDepth = static_cast<GByte>(
        std::max(static_cast<int>(m_poHeader->m_nMaxSpIndexDepth), nNextDepth));

    m_poSpIndex->GetMBR(m_poHeader->m_nXMin, m_poHeader->m_nYMin,
                        m_poHeader->m_nXMax, m_poHeader->m_nYMax);

    return m_poSpIndex->CommitToFile();
}

/**********************************************************************
 *                   TABMAPFile::GetMinTABFileVersion()
 *
 * Returns the minimum TAB file version number that can contain all the
 * objects stored in this file.
 **********************************************************************/
int   TABMAPFile::GetMinTABFileVersion()
{
    int nToolVersion = 0;

    if (m_poToolDefTable)
        nToolVersion = m_poToolDefTable->GetMinVersionNumber();

    return std::max(nToolVersion, m_nMinTABVersion);
}

const CPLString& TABMAPFile::GetEncoding() const
{
    return m_osEncoding;
}

void TABMAPFile::SetEncoding( const CPLString& osEncoding )
{
    m_osEncoding = osEncoding;
}

bool TABMAPFile::IsValidObjType(int nObjType)
{
    switch( nObjType )
    {
        case TAB_GEOM_NONE:
        case TAB_GEOM_SYMBOL_C:
        case TAB_GEOM_SYMBOL:
        case TAB_GEOM_LINE_C:
        case TAB_GEOM_LINE:
        case TAB_GEOM_PLINE_C:
        case TAB_GEOM_PLINE:
        case TAB_GEOM_ARC_C:
        case TAB_GEOM_ARC:
        case TAB_GEOM_REGION_C:
        case TAB_GEOM_REGION:
        case TAB_GEOM_TEXT_C:
        case TAB_GEOM_TEXT:
        case TAB_GEOM_RECT_C:
        case TAB_GEOM_RECT:
        case TAB_GEOM_ROUNDRECT_C:
        case TAB_GEOM_ROUNDRECT:
        case TAB_GEOM_ELLIPSE_C:
        case TAB_GEOM_ELLIPSE:
        case TAB_GEOM_MULTIPLINE_C:
        case TAB_GEOM_MULTIPLINE:
        case TAB_GEOM_FONTSYMBOL_C:
        case TAB_GEOM_FONTSYMBOL:
        case TAB_GEOM_CUSTOMSYMBOL_C:
        case TAB_GEOM_CUSTOMSYMBOL:
        case TAB_GEOM_V450_REGION_C:
        case TAB_GEOM_V450_REGION:
        case TAB_GEOM_V450_MULTIPLINE_C:
        case TAB_GEOM_V450_MULTIPLINE:
        case TAB_GEOM_MULTIPOINT_C:
        case TAB_GEOM_MULTIPOINT:
        case TAB_GEOM_COLLECTION_C:
        case TAB_GEOM_COLLECTION:
        case TAB_GEOM_UNKNOWN1_C:
        case TAB_GEOM_UNKNOWN1:
        case TAB_GEOM_V800_REGION_C:
        case TAB_GEOM_V800_REGION:
        case TAB_GEOM_V800_MULTIPLINE_C:
        case TAB_GEOM_V800_MULTIPLINE:
        case TAB_GEOM_V800_MULTIPOINT_C:
        case TAB_GEOM_V800_MULTIPOINT:
        case TAB_GEOM_V800_COLLECTION_C:
        case TAB_GEOM_V800_COLLECTION:
            return true;

        default:
            return false;
    }
}

/**********************************************************************
 *                   TABMAPFile::Dump()
 *
 * Dump block contents... available only in DEBUG mode.
 **********************************************************************/
#ifdef DEBUG

void TABMAPFile::Dump(FILE *fpOut /*=NULL*/)
{
    if (fpOut == nullptr)
        fpOut = stdout;

    fprintf(fpOut, "----- TABMAPFile::Dump() -----\n");

    if (m_fp == nullptr)
    {
        fprintf(fpOut, "File is not opened.\n");
    }
    else
    {
        fprintf(fpOut, "File is opened: %s\n", m_pszFname);
        fprintf(fpOut, "Coordsys filter  = (%g,%g)-(%g,%g)\n",
                m_sMinFilter.x, m_sMinFilter.y, m_sMaxFilter.x,m_sMaxFilter.y);
        fprintf(fpOut, "Int coord filter = (%d,%d)-(%d,%d)\n",
                m_XMinFilter, m_YMinFilter, m_XMaxFilter,m_YMaxFilter);

        fprintf(fpOut, "\nFile Header follows ...\n\n");
        m_poHeader->Dump(fpOut);
        fprintf(fpOut, "... end of file header.\n\n");

        fprintf(fpOut, "Associated .ID file ...\n\n");
        m_poIdIndex->Dump(fpOut);
        fprintf(fpOut, "... end of ID file dump.\n\n");
    }

    fflush(fpOut);
}

#endif // DEBUG

/**********************************************************************
 *                   TABMAPFile::DumpSpatialIndexToMIF()
 *
 * Dump the spatial index tree... available only in DEBUG mode.
 **********************************************************************/
#ifdef DEBUG

void TABMAPFile::DumpSpatialIndexToMIF(TABMAPIndexBlock *poNode,
                                       FILE *fpMIF, FILE *fpMID,
                                       int nParentId /*=-1*/,
                                       int nIndexInNode /*=-1*/,
                                       int nCurDepth /*=0*/,
                                       int nMaxDepth /*=-1*/)
{
    if (poNode == nullptr)
    {
        if (m_poHeader && m_poHeader->m_nFirstIndexBlock != 0)
        {
            TABRawBinBlock *poBlock =
                GetIndexObjectBlock(m_poHeader->m_nFirstIndexBlock);
            if (poBlock && poBlock->GetBlockType() == TABMAP_INDEX_BLOCK)
                poNode = cpl::down_cast<TABMAPIndexBlock *>(poBlock);
        }

        if (poNode == nullptr)
            return;
    }

    /*-------------------------------------------------------------
     * Report info on current tree node
     *------------------------------------------------------------*/
    const int numEntries = poNode->GetNumEntries();
    GInt32 nXMin = 0;
    GInt32 nYMin = 0;
    GInt32 nXMax = 0;
    GInt32 nYMax = 0;

    poNode->RecomputeMBR();
    poNode->GetMBR(nXMin, nYMin, nXMax, nYMax);

    double dXMin = 0.0;
    double dYMin = 0.0;
    double dXMax = 0.0;
    double dYMax = 0.0;
    Int2Coordsys(nXMin, nYMin, dXMin, dYMin);
    Int2Coordsys(nXMax, nYMax, dXMax, dYMax);

    VSIFPrintf(fpMIF, "RECT %g %g %g %g\n", dXMin, dYMin, dXMax, dYMax);
    VSIFPrintf(fpMIF, "  Brush(1, 0)\n");  /* No fill */

    VSIFPrintf(fpMID, "%d,%d,%d,%d,%g,%d,%d,%d,%d\n",
               poNode->GetStartAddress(),
               nParentId,
               nIndexInNode,
               nCurDepth,
               MITAB_AREA(nXMin, nYMin, nXMax, nYMax),
               nXMin, nYMin, nXMax, nYMax);

    if (nMaxDepth != 0)
    {
        /*-------------------------------------------------------------
         * Loop through all entries, dumping each of them
         *------------------------------------------------------------*/
        for(int i=0; i<numEntries; i++)
        {
            TABMAPIndexEntry *psEntry = poNode->GetEntry(i);

            TABRawBinBlock *poBlock = GetIndexObjectBlock( psEntry->nBlockPtr );
            if( poBlock == nullptr )
                continue;

            if( poBlock->GetBlockType() == TABMAP_INDEX_BLOCK )
            {
                /* Index block, dump recursively */
                DumpSpatialIndexToMIF(cpl::down_cast<TABMAPIndexBlock *>(poBlock),
                                      fpMIF, fpMID,
                                      poNode->GetStartAddress(),
                                      i, nCurDepth+1, nMaxDepth-1);
            }
            else
            {
                /* Object block, dump directly */
                CPLAssert( poBlock->GetBlockType() == TABMAP_OBJECT_BLOCK );

                Int2Coordsys(psEntry->XMin, psEntry->YMin, dXMin, dYMin);
                Int2Coordsys(psEntry->XMax, psEntry->YMax, dXMax, dYMax);

                VSIFPrintf(fpMIF, "RECT %g %g %g %g\n", dXMin, dYMin, dXMax, dYMax);
                VSIFPrintf(fpMIF, "  Brush(1, 0)\n");  /* No fill */

                VSIFPrintf(fpMID, "%d,%d,%d,%d,%g,%d,%d,%d,%d\n",
                           psEntry->nBlockPtr,
                           poNode->GetStartAddress(),
                           i,
                           nCurDepth+1,
                           MITAB_AREA(psEntry->XMin, psEntry->YMin,
                                      psEntry->XMax, psEntry->YMax),
                           psEntry->XMin, psEntry->YMin,
                           psEntry->XMax, psEntry->YMax);
            }

            delete poBlock;
        }
    }
}

#endif // DEBUG
