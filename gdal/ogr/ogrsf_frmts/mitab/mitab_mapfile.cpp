/**********************************************************************
 * $Id: mitab_mapfile.cpp,v 1.46 2010-07-07 19:00:15 aboudreault Exp $
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
 **********************************************************************
 *
 * $Log: mitab_mapfile.cpp,v $
 * Revision 1.46  2010-07-07 19:00:15  aboudreault
 * Cleanup Win32 Compile Warnings (GDAL bug #2930)
 *
 * Revision 1.45  2010-01-08 22:02:51  aboudreault
 * Fixed error issued when reading empty TAB with spatial index active (bug 2136)
 *
 * Revision 1.44  2009-03-03 20:44:23  dmorissette
 * Use transparent brush in DumpSpatialIndexToMIF()
 *
 * Revision 1.43  2008/02/20 21:35:30  dmorissette
 * Added support for V800 COLLECTION of large objects (bug 1496)
 *
 * Revision 1.42  2008/02/01 19:36:31  dmorissette
 * Initial support for V800 REGION and MULTIPLINE (bug 1496)
 *
 * Revision 1.41  2007/11/08 18:57:56  dmorissette
 * Upgrade of OGR and CPL libs to the version from GDAL/OGR 1.4.3
 *
 * Revision 1.40  2007/09/14 18:30:19  dmorissette
 * Fixed the splitting of object blocks with the optimized spatial
 * index mode that was producing files with misaligned bytes that
 * confused MapInfo (bug 1732)
 *
 * Revision 1.39  2007/07/11 15:51:52  dmorissette
 * Fixed duplicate 'int i' definition build errors in SplitObjBlock()
 *
 * Revision 1.38  2007/06/12 12:50:39  dmorissette
 * Use Quick Spatial Index by default until bug 1732 is fixed (broken files
 * produced by current coord block splitting technique).
 *
 * Revision 1.37  2007/06/05 13:23:57  dmorissette
 * Fixed memory leak when writing .TAB with new (optimized) spatial index
 * introduced in v1.6.0 (bug 1725)
 *
 * Revision 1.36  2007/03/21 21:15:56  dmorissette
 * Added SetQuickSpatialIndexMode() which generates a non-optimal spatial
 * index but results in faster write time (bug 1669)
 *
 * Revision 1.35  2006/11/28 18:49:08  dmorissette
 * Completed changes to split TABMAPObjectBlocks properly and produce an
 * optimal spatial index (bug 1585)
 *
 * Revision 1.34  2006/11/20 20:05:58  dmorissette
 * First pass at improving generation of spatial index in .map file (bug 1585)
 * New methods for insertion and splittung in the spatial index are done.
 * Also implemented a method to dump the spatial index to .mif/.mid
 * Still need to implement splitting of TABMapObjectBlock to get optimal
 * results.
 *
 * Revision 1.33  2006/09/05 23:05:08  dmorissette
 * Added TABMAPFile::DumpSpatialIndex() (bug 1585)
 *
 * Revision 1.32  2005/10/06 19:15:31  dmorissette
 * Collections: added support for reading/writing pen/brush/symbol ids and
 * for writing collection objects to .TAB/.MAP (bug 1126)
 *
 * Revision 1.31  2004/09/22 13:07:58  fwarmerdam
 * fixed return value in LoadNextMatchingObjectBlock() per rso bug 615
 *
 * Revision 1.30  2004/06/30 20:29:04  dmorissette
 * Fixed refs to old address danmo@videotron.ca
 *
 * Revision 1.29  2003/08/12 23:17:21  dmorissette
 * Added reading of v500+ coordsys affine params (Anthony D. - Encom)
 *
 * Revision 1.28  2002/08/27 17:18:43  warmerda
 * improved CPL error testing
 *
 * Revision 1.27  2002/07/30 13:54:12  julien
 * TABMAPFile::GetFeatureId() now return -1 when there's no geometry. (bug 169)
 *
 * Revision 1.26  2002/04/25 16:05:24  julien
 * Disabled the overflow warning in SetCoordFilter() by adding bIgnoreOverflow
 * variable in Coordsys2Int of the TABMAPFile class and TABMAPHeaderBlock class
 *
 * Revision 1.25  2002/03/26 19:27:43  daniel
 * Got rid of tabs in source
 *
 * Revision 1.24  2002/03/26 01:48:40  daniel
 * Added Multipoint object type (V650)
 *
 * Revision 1.23  2002/02/20 13:53:40  daniel
 * Prevent an infinite loop of calls to LoadNextMatchingObjectBlock() in
 * GetNextFeatureId() if no objects found in spatial index.
 *
 * Revision 1.22  2001/11/19 15:04:41  daniel
 * Prevent writing of coordinates outside of the +/-1e9 integer bounds.
 *
 * Revision 1.21  2001/11/17 21:54:06  daniel
 * Made several changes in order to support writing objects in 16 bits 
 * coordinate format. New TABMAPObjHdr-derived classes are used to hold 
 * object info in mem until block is full.
 *
 * Revision 1.20  2001/09/18 20:33:52  warmerda
 * fixed case of spatial search on file with just one object block
 *
 * Revision 1.19  2001/09/14 03:23:55  warmerda
 * Substantial upgrade to support spatial queries using spatial indexes
 *
 * Revision 1.18  2001/03/15 03:57:51  daniel
 * Added implementation for new OGRLayer::GetExtent(), returning data MBR.
 *
 * Revision 1.17  2000/11/23 21:11:07  daniel
 * OOpps... VC++ didn't like the way TABPenDef, etc. were initialized
 *
 * Revision 1.16  2000/11/23 20:47:46  daniel
 * Use MI defaults for Pen, Brush, Font, Symbol instead of all zeros
 *
 * Revision 1.15  2000/11/22 04:03:10  daniel
 * Added warning when objects written outside of the +/-1e9 int. coord. range
 *
 * Revision 1.14  2000/11/15 04:13:49  daniel
 * Fixed writing of TABMAPToolBlock to allocate a new block when full
 *
 * Revision 1.13  2000/05/19 06:44:55  daniel
 * Modified generation of spatial index to split index nodes and produce a
 * more balanced tree.
 *
 * Revision 1.12  2000/03/13 05:58:01  daniel
 * Create 1024 bytes V500 .MAP header + limit m_nMaxCoordBufSize for V450 obj.
 *
 * Revision 1.11  2000/02/28 17:00:00  daniel
 * Added V450 object types
 *
 * Revision 1.10  2000/01/15 22:30:44  daniel
 * Switch to MIT/X-Consortium OpenSource license
 *
 * Revision 1.9  1999/12/19 17:37:52  daniel
 * Fixed memory leaks
 *
 * Revision 1.8  1999/11/14 04:43:31  daniel
 * Support dataset with no .MAP/.ID files
 *
 * Revision 1.7  1999/10/19 22:57:17  daniel
 * Create m_poCurObjBlock only when needed to avoid empty blocks in files
 * and problems with MBR in header block of files with only "NONE" geometries
 *
 * Revision 1.6  1999/10/06 13:17:46  daniel
 * Update m_nMaxCoordBufSize in header block
 *
 * Revision 1.5  1999/10/01 03:52:22  daniel
 * Avoid producing an unused block in the file when closing it.
 *
 * Revision 1.4  1999/09/26 14:59:36  daniel
 * Implemented write support
 *
 * Revision 1.3  1999/09/20 18:42:42  daniel
 * Use binary access to open file.
 *
 * Revision 1.2  1999/09/16 02:39:16  daniel
 * Completed read support for most feature types
 *
 * Revision 1.1  1999/07/12 04:18:24  daniel
 * Initial checkin
 *
 **********************************************************************/

#include "mitab.h"

/*=====================================================================
 *                      class TABMAPFile
 *====================================================================*/


/**********************************************************************
 *                   TABMAPFile::TABMAPFile()
 *
 * Constructor.
 **********************************************************************/
TABMAPFile::TABMAPFile()
{
    m_nMinTABVersion = 300;
    m_fp = NULL;
    m_pszFname = NULL;
    m_poHeader = NULL;
    m_poSpIndex = NULL;
    m_poSpIndexLeaf = NULL;
/* See bug 1732: Optimized spatial index produces broken files because
 * of the way CoordBlocks are split. For now we have to force using the
 * Quick (old) spatial index mode by default until bug 1732 is fixed.
 */
    m_bQuickSpatialIndexMode = TRUE;
//  m_bQuickSpatialIndexMode = FALSE;

    m_poCurObjBlock = NULL;
    m_nCurObjPtr = -1;
    m_nCurObjType = TAB_GEOM_UNSET;
    m_nCurObjId = -1;
    m_poCurCoordBlock = NULL;
    m_poToolDefTable = NULL;
    
    m_bUpdated = FALSE;
    m_bLastOpWasRead = FALSE;
    m_bLastOpWasWrite = FALSE;
    
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

int TABMAPFile::Open(const char *pszFname, const char* pszAccess, GBool bNoErrorMsg)
{
    if( EQUALN(pszAccess, "r", 1) )
        return Open(pszFname, TABRead, bNoErrorMsg);
    else if( EQUALN(pszAccess, "w", 1) )
        return Open(pszFname, TABWrite, bNoErrorMsg);
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
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::Open(const char *pszFname, TABAccess eAccess,
                     GBool bNoErrorMsg /* = FALSE */)
{
    VSILFILE    *fp=NULL;
    TABRawBinBlock *poBlock=NULL;

    if (m_fp)
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Open() failed: object already contains an open file");
        return -1;
    }

    m_nMinTABVersion = 300;
    m_fp = NULL;
    m_poHeader = NULL;
    m_poIdIndex = NULL;
    m_poSpIndex = NULL;
    m_poToolDefTable = NULL;
    m_eAccessMode = eAccess;
    m_bUpdated = FALSE;
    m_bLastOpWasRead = FALSE;
    m_bLastOpWasWrite = FALSE;

    /*-----------------------------------------------------------------
     * Open file
     *----------------------------------------------------------------*/
    const char* pszAccess = ( eAccess == TABRead ) ? "rb" :
                            ( eAccess == TABWrite ) ? "wb+" :
                                                      "rb+";
    fp = VSIFOpenL(pszFname, pszAccess);

    m_oBlockManager.Reset();

    if (fp != NULL && (m_eAccessMode == TABRead || m_eAccessMode == TABReadWrite))
    {
        /*-----------------------------------------------------------------
         * Read access: try to read header block
         * First try with a 512 bytes block to check the .map version.
         * If it's version 500 or more then read again a 1024 bytes block
         *----------------------------------------------------------------*/
        poBlock = TABCreateMAPBlockFromFile(fp, 0, 512, TRUE, m_eAccessMode);

        if (poBlock && poBlock->GetBlockClass() == TABMAP_HEADER_BLOCK &&
            ((TABMAPHeaderBlock*)poBlock)->m_nMAPVersionNumber >= 500)
        {
            // Version 500 or higher.  Read 1024 bytes block instead of 512
            delete poBlock;
            poBlock = TABCreateMAPBlockFromFile(fp, 0, 1024, TRUE, m_eAccessMode);
        }

        if (poBlock==NULL || poBlock->GetBlockClass() != TABMAP_HEADER_BLOCK)
        {
            if (poBlock)
                delete poBlock;
            poBlock = NULL;
            VSIFCloseL(fp);
            CPLError(CE_Failure, CPLE_FileIO,
                "Open() failed: %s does not appear to be a valid .MAP file",
                     pszFname);
            return -1;
        }
    }
    else if (fp != NULL && m_eAccessMode == TABWrite)
    {
        /*-----------------------------------------------------------------
         * Write access: create a new header block
         * .MAP files of Version 500 and up appear to have a 1024 bytes
         * header.  The last 512 bytes are usually all zeros.
         *----------------------------------------------------------------*/
        poBlock = new TABMAPHeaderBlock(m_eAccessMode);
        poBlock->InitNewBlock(fp, 1024, m_oBlockManager.AllocNewBlock("HEADER") );

        // Alloc a second 512 bytes of space since oBlockManager deals 
        // with 512 bytes blocks.
        m_oBlockManager.AllocNewBlock("HEADER"); 
    }
    else if (bNoErrorMsg)
    {
        /*-----------------------------------------------------------------
         * .MAP does not exist... produce no error message, but set
         * the class members so that MoveToObjId() and GetCurObjType()
         * can be used to return only NONE geometries.
         *----------------------------------------------------------------*/
        m_fp = NULL;
        m_nCurObjType = TAB_GEOM_NONE;

        /* Create a false header block that will return default
         * values for projection and coordsys conversion stuff...
         */
        m_poHeader = new TABMAPHeaderBlock(m_eAccessMode);
        m_poHeader->InitNewBlock(NULL, 512, 0 );

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
    m_poHeader = (TABMAPHeaderBlock*)poBlock;
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
        m_poCurObjBlock->InitNewBlock(m_fp, 512);
    }
    else
    {
        m_poCurObjBlock = NULL;
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
    m_poSpIndex = NULL;

    if (m_eAccessMode == TABReadWrite)
    {
        /* We don't allow quick mode in read/write mode */
        m_bQuickSpatialIndexMode = FALSE;

        if( m_poHeader->m_nFirstIndexBlock != 0 )
        {
            TABRawBinBlock *poBlock;
            poBlock = GetIndexObjectBlock( m_poHeader->m_nFirstIndexBlock );
            if( poBlock == NULL || poBlock->GetBlockType() != TABMAP_INDEX_BLOCK )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot find first index block at offset %d",
                         m_poHeader->m_nFirstIndexBlock );
                delete poBlock;
            }
            else
            {
                m_poSpIndex = (TABMAPIndexBlock *)poBlock;
                m_poSpIndex->SetMBR(m_poHeader->m_nXMin, m_poHeader->m_nYMin,
                                    m_poHeader->m_nXMax, m_poHeader->m_nYMax);
            }
        }
    }

    /*-----------------------------------------------------------------
     * Initialization of the Drawing Tools table will be done automatically
     * as Read/Write calls are done later.
     *----------------------------------------------------------------*/
    m_poToolDefTable = NULL;
    
    if( m_eAccessMode == TABReadWrite )
    {
        InitDrawingTools();
    }

    if( m_eAccessMode == TABReadWrite )
    {
        VSIStatBufL sStatBuf;
        VSIStatL(m_pszFname, &sStatBuf);
        m_oBlockManager.SetLastPtr((int)(((sStatBuf.st_size-1)/512)*512));

        /* Read chain of garbage blocks */
        if( m_poHeader->m_nFirstGarbageBlock != 0 )
        {
            int nCurGarbBlock = m_poHeader->m_nFirstGarbageBlock;
            m_oBlockManager.PushGarbageBlockAsLast(nCurGarbBlock);
            while(TRUE)
            {
                GUInt16 nBlockType;
                int     nNextGarbBlockPtr;
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
     * Make sure all previous calls succeded.
     *----------------------------------------------------------------*/
    if (CPLGetLastErrorNo() != 0)
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
    if (m_fp == NULL && m_poHeader == NULL)
        return 0;

    /*----------------------------------------------------------------
     * Write access: commit latest changes to the file.
     *---------------------------------------------------------------*/
    if (m_eAccessMode == TABWrite || (m_eAccessMode == TABReadWrite && m_bUpdated))
    {
        // Start by committing current object and coord blocks
        // Nothing happens if none has been created yet.
        CommitObjAndCoordBlocks(FALSE);

        // Write the drawing tools definitions now.
        CommitDrawingTools();

        // Commit spatial index blocks
        CommitSpatialIndex();

        // Update header fields and commit
        if (m_poHeader)
        {
            // OK, with V450 files, objects are not limited to 32k nodes
            // any more, and this means that m_nMaxCoordBufSize can become
            // huge, and actually more huge than can be held in memory.
            // MapInfo counts m_nMaxCoordBufSize=0 for V450 objects, but 
            // until this is cleanly implented, we will just prevent 
            // m_nMaxCoordBufSizefrom going beyond 512k in V450 files.
            if (m_nMinTABVersion >= 450)
            {
                m_poHeader->m_nMaxCoordBufSize = 
                                 MIN(m_poHeader->m_nMaxCoordBufSize, 512*1024);
            }

            // Write Ref to beginning of the chain of garbage blocks
            m_poHeader->m_nFirstGarbageBlock = 
                m_oBlockManager.GetFirstGarbageBlock();

            m_poHeader->CommitToFile();
        }
    }
    
    // Check for overflow of internal coordinates and produce a warning
    // if that happened...
    if (m_poHeader && m_poHeader->m_bIntBoundsOverflow)
    {
        double dBoundsMinX, dBoundsMinY, dBoundsMaxX, dBoundsMaxY;
        Int2Coordsys(-1000000000, -1000000000, dBoundsMinX, dBoundsMinY);
        Int2Coordsys(1000000000, 1000000000, dBoundsMaxX, dBoundsMaxY);

        CPLError(CE_Warning, TAB_WarningBoundsOverflow,
                 "Some objects were written outside of the file's "
                 "predefined bounds.\n"
                 "These objects may have invalid coordinates when the file "
                 "is reopened.\n"
                 "Predefined bounds: (%.15g,%.15g)-(%.15g,%.15g)\n",
                 dBoundsMinX, dBoundsMinY, dBoundsMaxX, dBoundsMaxY );
    }

    // Delete all structures 
    if (m_poHeader)
        delete m_poHeader;
    m_poHeader = NULL;

    if (m_poIdIndex)
    {
        m_poIdIndex->Close();
        delete m_poIdIndex;
        m_poIdIndex = NULL;
    }

    if (m_poCurObjBlock)
    {
        delete m_poCurObjBlock;
        m_poCurObjBlock = NULL;
        m_nCurObjPtr = -1;
        m_nCurObjType = TAB_GEOM_UNSET;
        m_nCurObjId = -1;
    }

    if (m_poCurCoordBlock)
    {
        delete m_poCurCoordBlock;
        m_poCurCoordBlock = NULL;
    }

    if (m_poSpIndex)
    {
        delete m_poSpIndex;
        m_poSpIndex = NULL;
        m_poSpIndexLeaf = NULL;
    }

    if (m_poToolDefTable)
    {
        delete m_poToolDefTable;
        m_poToolDefTable = NULL;
    }

    // Close file
    if (m_fp)
        VSIFCloseL(m_fp);
    m_fp = NULL;

    CPLFree(m_pszFname);
    m_pszFname = NULL;

    return 0;
}

/**********************************************************************
 *                   TABMAPFile::ReOpenReadWrite()
 **********************************************************************/
int TABMAPFile::ReOpenReadWrite()
{
    char* pszFname = m_pszFname;
    m_pszFname = NULL;
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

    if (m_poCurObjBlock != NULL || m_poSpIndex != NULL)
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
    TABRawBinBlock *poBlock;

    poBlock = GetIndexObjectBlock( nFileOffset );
    if( poBlock == NULL )
        return NULL;

    if( poBlock->GetBlockType() == TABMAP_INDEX_BLOCK )
    {
        TABMAPIndexBlock *poIndex = (TABMAPIndexBlock *) poBlock;

        if( m_poSpIndexLeaf == NULL )
        {
            delete m_poSpIndex;
            m_poSpIndexLeaf = m_poSpIndex = poIndex;
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
        
        if( m_poCurObjBlock != NULL )
            delete m_poCurObjBlock;

        m_poCurObjBlock = (TABMAPObjectBlock *) poBlock;

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
        CPLAssert( m_poSpIndexLeaf == NULL );

        /* m_nFirstIndexBlock set to 0 means that there is no feature */
        if ( m_poHeader->m_nFirstIndexBlock == 0 )
            return FALSE;

        if( m_poSpIndex != NULL )
        {
            m_poSpIndex->UnsetCurChild();
            m_poSpIndexLeaf = m_poSpIndex;
        }
        else
        {
            if( PushBlock( m_poHeader->m_nFirstIndexBlock ) == NULL )
                return FALSE;

            if( m_poSpIndex == NULL )
            {
                CPLAssert( m_poCurObjBlock != NULL );
                return TRUE;
            }
        }
    }

    while( m_poSpIndexLeaf != NULL )
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
            
            if( poParent != NULL )
            {
                poParent->SetCurChildRef( NULL, poParent->GetCurChildIndex() );
            }
            continue;
        }

        m_poSpIndexLeaf->SetCurChildRef( NULL, ++iEntry );

        TABMAPIndexEntry *psEntry = m_poSpIndexLeaf->GetEntry( iEntry );
        TABRawBinBlock *poBlock;
        
        if( psEntry->XMax < m_XMinFilter
            || psEntry->YMax < m_YMinFilter
            || psEntry->XMin > m_XMaxFilter
            || psEntry->YMin > m_YMaxFilter )
            continue;

        poBlock = PushBlock( psEntry->nBlockPtr );
        if( poBlock == NULL )
            return FALSE;
        else if( poBlock->GetBlockType() == TABMAP_OBJECT_BLOCK )
            return TRUE;
        else
            /* continue processing new index block */;
    }

    return m_poSpIndexLeaf != NULL;
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
    m_poSpIndexLeaf = NULL;
    
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
    if( m_fp == NULL )
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

    CPLAssert( nPrevId == -1 || m_poCurObjBlock != NULL );

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
int TABMAPFile::Int2Coordsys(GInt32 nX, GInt32 nY, double &dX, double &dY)
{
    if (m_poHeader == NULL)
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
int TABMAPFile::Coordsys2Int(double dX, double dY, GInt32 &nX, GInt32 &nY, 
                             GBool bIgnoreOverflow/*=FALSE*/)
{
    if (m_poHeader == NULL)
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
int TABMAPFile::Int2CoordsysDist(GInt32 nX, GInt32 nY, double &dX, double &dY)
{
    if (m_poHeader == NULL)
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
int TABMAPFile::Coordsys2IntDist(double dX, double dY, GInt32 &nX, GInt32 &nY)
{
    if (m_poHeader == NULL)
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
int TABMAPFile::SetCoordsysBounds(double dXMin, double dYMin, 
                                  double dXMax, double dYMax)
{
    int nStatus = 0;

    if (m_poHeader == NULL)
        return -1;

    nStatus = m_poHeader->SetCoordsysBounds(dXMin, dYMin, dXMax, dYMax);

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
int   TABMAPFile::MoveToObjId(int nObjId)
{
    int nFileOffset;
    
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
     * In read access mode, since the .MAP/.ID are optional, if the 
     * file is not opened then we can still act as if one existed and
     * make any object id look like a TAB_GEOM_NONE
     *----------------------------------------------------------------*/
    if (m_fp == NULL && m_eAccessMode == TABRead)
    {
        CPLAssert(m_poIdIndex == NULL && m_poCurObjBlock == NULL);
        m_nCurObjPtr = 0;
        m_nCurObjId = nObjId;
        m_nCurObjType = TAB_GEOM_NONE;

        return 0;
    }

    if (m_poIdIndex == NULL || m_poCurObjBlock == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "MoveToObjId(): file not opened!");
        m_nCurObjPtr = m_nCurObjId = -1;
        m_nCurObjType = TAB_GEOM_UNSET;
        return -1;
    }

    /*-----------------------------------------------------------------
     * Move map object pointer to the right location.  Fetch location
     * from the index file, unless we are already pointing at it.
     *----------------------------------------------------------------*/
    if( m_nCurObjId == nObjId )
        nFileOffset = m_nCurObjPtr;
    else
        nFileOffset = m_poIdIndex->GetObjPtr(nObjId);

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
        m_nCurObjType = (TABGeomType)m_poCurObjBlock->ReadByte();
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
            m_nCurObjPtr = m_nCurObjId = -1;
            m_nCurObjType = TAB_GEOM_UNSET;
            return -1;
        }
    }
    else
    {
        /*---------------------------------------------------------
         * Failed positioning input file... CPLError has been called.
         *--------------------------------------------------------*/
        m_nCurObjPtr = m_nCurObjId = -1;
        m_nCurObjType = TAB_GEOM_UNSET;
        return -1;
    }

    return 0;
}


/**********************************************************************
 *                   TABMAPFile::MarkAsDeleted()
 
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::MarkAsDeleted()
{
    if (m_eAccessMode == TABRead || m_poCurObjBlock == NULL)
        return -1;
    
    if ( m_nCurObjPtr <= 0 )
        return 0;
    
    /* Goto offset for object id */
    if ( m_poCurObjBlock->GotoByteInFile(m_nCurObjPtr + 1, TRUE) != 0)
        return -1;

    /* Mark object as deleted */
    m_poCurObjBlock->WriteInt32(m_nCurObjId | 0x40000000);

    int ret = 0;
    if( m_poCurObjBlock->CommitToFile() != 0 )
        ret = -1;

    /* Update index entry to reflect delete state as well */
    if( m_poIdIndex->SetObjPtr(m_nCurObjId, 0) != 0 )
        ret = -1;

    m_nCurObjPtr = m_nCurObjId = -1;
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
     * Check forminimum TAB file version number
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
int   TABMAPFile::PrepareNewObj(TABMAPObjHdr *poObjHdr)
{
    m_nCurObjPtr = m_nCurObjId = -1;
    m_nCurObjType = TAB_GEOM_UNSET;

    if (m_eAccessMode == TABRead || 
        m_poIdIndex == NULL || m_poHeader == NULL)
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

    if (CPLGetLastErrorNo() != 0 && CPLGetLastErrorType() == CE_Failure)
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
    int nObjSize;
    GInt32 nObjBlockForInsert = -1;

    /*-----------------------------------------------------------------
     * Create spatial index if we don't have one yet.
     * We do not create the index and object data blocks in the open() 
     * call because files that contained only "NONE" geometries ended up 
     * with empty object and spatial index blocks.
     *----------------------------------------------------------------*/
    if (m_poSpIndex == NULL)
    {
        // Spatial Index not created yet...
        m_poSpIndex = new TABMAPIndexBlock(m_eAccessMode);

        m_poSpIndex->InitNewBlock(m_fp, 512, 
                                  m_oBlockManager.AllocNewBlock("INDEX"));
        m_poSpIndex->SetMAPBlockManagerRef(&m_oBlockManager);

        m_poHeader->m_nFirstIndexBlock = m_poSpIndex->GetNodeBlockPtr();

        /* We'll also need to create an object data block (later) */
        nObjBlockForInsert = -1;

        CPLAssert(m_poCurObjBlock == NULL);
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
        if (nObjBlockForInsert == -1)
        {
            /* ChooseLeafForInsert() should not fail unless file is corrupt*/
            CPLError(CE_Failure, CPLE_AssertionFailed,
                     "ChooseLeafForInsert() Failed?!?!");
            return -1;
        }
    }


    if (nObjBlockForInsert == -1)
    {
        /*-------------------------------------------------------------
         * Create a new object data block from scratch
         *------------------------------------------------------------*/
        m_poCurObjBlock = new TABMAPObjectBlock(TABReadWrite);

        int nBlockOffset = m_oBlockManager.AllocNewBlock("OBJECT");

        m_poCurObjBlock->InitNewBlock(m_fp, 512, nBlockOffset);

        /*-------------------------------------------------------------
         * Insert new object block in index, based on MBR of poObjHdr
         *------------------------------------------------------------*/
        if (m_poSpIndex->AddEntry(poObjHdr->m_nMinX, 
                                  poObjHdr->m_nMinY,
                                  poObjHdr->m_nMaxX,
                                  poObjHdr->m_nMaxY,
                                  m_poCurObjBlock->GetStartAddress()) != 0)
            return -1;

        m_poHeader->m_nMaxSpIndexDepth = MAX(m_poHeader->m_nMaxSpIndexDepth,
                                      (GByte)m_poSpIndex->GetCurMaxDepth()+1);
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
            /* Got a block in memory but it's not the right one, flush it */
            if (CommitObjAndCoordBlocks(TRUE) != 0 )
                return -1;
        }

        if (m_poCurObjBlock == NULL)
        {
            if (LoadObjAndCoordBlocks(nObjBlockForInsert) != 0)
                return -1;

            /* If we have compressed objects, we don't want to change the center  */
            m_poCurObjBlock->LockCenter();

            // The ObjBlock doesn't know its MBR. Get the value from the 
            // index and set it
            GInt32 nMinX, nMinY, nMaxX, nMaxY;
            m_poSpIndex->GetCurLeafEntryMBR(m_poCurObjBlock->GetStartAddress(),
                                            nMinX, nMinY, nMaxX, nMaxY);
            m_poCurObjBlock->SetMBR(nMinX, nMinY, nMaxX, nMaxY);
        }
        else
        {
            /* If we have compressed objects, we don't want to change the center */
            m_poCurObjBlock->LockCenter();
        }
    }

    /*-----------------------------------------------------------------
     * Fetch new object size, make sure there is enough room in obj. 
     * block for new object, update spatial index and split if necessary.
     *----------------------------------------------------------------*/
    nObjSize = m_poHeader->GetMapObjectSize(poObjHdr->m_nType);


    /*-----------------------------------------------------------------
     * But first check if we can recover space from this block in case
     * there are deleted objects in it.
     *----------------------------------------------------------------*/
    if (m_poCurObjBlock->GetNumUnusedBytes() < nObjSize )
    {
        TABMAPObjHdr *poExistingObjHdr=NULL;
        TABMAPObjHdr **papoSrcObjHdrs = NULL;
        int i, numSrcObj = 0;
        int nObjectSpace = 0;

        /* First pass to enumerate valid objects and compute their accumulated
           required size. */
        m_poCurObjBlock->Rewind();
        while ((poExistingObjHdr = TABMAPObjHdr::ReadNextObj(m_poCurObjBlock, 
                                                    m_poHeader)) != NULL)
        {
            if (papoSrcObjHdrs == NULL || numSrcObj%10 == 0)
            {
                // Realloc the array... by steps of 10
                papoSrcObjHdrs = (TABMAPObjHdr**)CPLRealloc(papoSrcObjHdrs, 
                                                            (numSrcObj+10)*
                                                            sizeof(TABMAPObjHdr*));
            }
            papoSrcObjHdrs[numSrcObj++] = poExistingObjHdr;

            nObjectSpace += m_poHeader->GetMapObjectSize(poExistingObjHdr->m_nType);
        }

        /* Check that there's really some place that can be recovered */
        if( nObjectSpace < 512 - 20 - m_poCurObjBlock->GetNumUnusedBytes() )
        {
#ifdef DEBUG_VERBOSE
            CPLDebug("MITAB", "Compacting block at offset %d, %d objects valid, recovering %d bytes",
                     m_poCurObjBlock->GetStartAddress(), numSrcObj,
                     (512 - 20 - m_poCurObjBlock->GetNumUnusedBytes()) - nObjectSpace);
#endif
            m_poCurObjBlock->ClearObjects();

            for(i=0; i<numSrcObj; i++)
            {
                /*-----------------------------------------------------------------
                * Prepare and Write ObjHdr to this ObjBlock
                *----------------------------------------------------------------*/
                int nObjPtr = m_poCurObjBlock->PrepareNewObject(papoSrcObjHdrs[i]);
                if (nObjPtr < 0 ||
                    m_poCurObjBlock->CommitNewObject(papoSrcObjHdrs[i]) != 0)
                {
                    CPLError(CE_Failure, CPLE_FileIO,
                            "Failed writing object header for feature id %d",
                            papoSrcObjHdrs[i]->m_nId);
                    return -1;
                }

                /*-----------------------------------------------------------------
                * Update .ID Index
                *----------------------------------------------------------------*/
                m_poIdIndex->SetObjPtr(papoSrcObjHdrs[i]->m_nId, nObjPtr);
            }
        }

        /* Cleanup papoSrcObjHdrs[] */
        for(i=0; i<numSrcObj; i++)
        {
            delete papoSrcObjHdrs[i];
        }
        CPLFree(papoSrcObjHdrs);
        papoSrcObjHdrs = NULL;
    }
    
    if (m_poCurObjBlock->GetNumUnusedBytes() >= nObjSize )
    {
        /*-------------------------------------------------------------
         * New object fits in current block, just update the spatial index
         *------------------------------------------------------------*/
        GInt32 nMinX, nMinY, nMaxX, nMaxY;
        m_poCurObjBlock->GetMBR(nMinX, nMinY, nMaxX, nMaxY);

        /* Need to calculate the enlarged MBR that includes new object */
        nMinX = MIN(nMinX, poObjHdr->m_nMinX);
        nMinY = MIN(nMinY, poObjHdr->m_nMinY);
        nMaxX = MAX(nMaxX, poObjHdr->m_nMaxX);
        nMaxY = MAX(nMaxY, poObjHdr->m_nMaxY);

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
        TABMAPObjectBlock *poNewObjBlock;
        poNewObjBlock= SplitObjBlock(poObjHdr, nObjSize);

        if (poNewObjBlock == NULL)
            return -1;  /* Split failed, error already reported. */

        /*-------------------------------------------------------------
         * Update index with info about m_poCurObjectBlock *first*
         * This is important since UpdateLeafEntry() needs the chain of
         * index nodes preloaded by ChooseLeafEntry() in order to do its job
         *------------------------------------------------------------*/
        GInt32 nMinX, nMinY, nMaxX, nMaxY;
        m_poCurObjBlock->GetMBR(nMinX, nMinY, nMaxX, nMaxY);

        /* Need to calculate the enlarged MBR that includes new object */
        nMinX = MIN(nMinX, poObjHdr->m_nMinX);
        nMinY = MIN(nMinY, poObjHdr->m_nMinY);
        nMaxX = MAX(nMaxX, poObjHdr->m_nMaxX);
        nMaxY = MAX(nMaxY, poObjHdr->m_nMaxY);

        if (m_poSpIndex->UpdateLeafEntry(m_poCurObjBlock->GetStartAddress(),
                                         nMinX, nMinY, nMaxX, nMaxY) != 0)
            return -1;

        /*-------------------------------------------------------------
         * Add new obj block to index
         *------------------------------------------------------------*/
        poNewObjBlock->GetMBR(nMinX, nMinY, nMaxX, nMaxY);

        if (m_poSpIndex->AddEntry(nMinX, nMinY, nMaxX, nMaxY,
                                  poNewObjBlock->GetStartAddress()) != 0)
            return -1;
        m_poHeader->m_nMaxSpIndexDepth = MAX(m_poHeader->m_nMaxSpIndexDepth,
                                      (GByte)m_poSpIndex->GetCurMaxDepth()+1);

        /*-------------------------------------------------------------
         * Delete second object block, no need to commit to file first since
         * it's already been committed to disk by Split()
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
    int nObjSize;

    /*-------------------------------------------------------------
     * We will need an object block... check if it exists and
     * create it if it has not been created yet (first time for this file).
     * We do not create the object block in the open() call because
     * files that contained only "NONE" geometries ended up with empty
     * object and spatial index blocks.
     * Note: A coord block will be created only if needed later.
     *------------------------------------------------------------*/
    if (m_poCurObjBlock == NULL)
    {
        m_poCurObjBlock = new TABMAPObjectBlock(m_eAccessMode);

        int nBlockOffset = m_oBlockManager.AllocNewBlock("OBJECT");

        m_poCurObjBlock->InitNewBlock(m_fp, 512, nBlockOffset);

        // The reference to the first object block should 
        // actually go through the index blocks... this will be 
        // updated when file is closed.
        m_poHeader->m_nFirstIndexBlock = nBlockOffset;
    }

    /*-----------------------------------------------------------------
     * Fetch new object size, make sure there is enough room in obj. 
     * block for new object, and save/create a new one if necessary.
     *----------------------------------------------------------------*/
    nObjSize = m_poHeader->GetMapObjectSize(poObjHdr->m_nType);
    if (m_poCurObjBlock->GetNumUnusedBytes() < nObjSize )
    {
        /*-------------------------------------------------------------
         * OK, the new object won't fit in the current block. Add the
         * current block to the spatial index, commit it to disk and init
         * a new block
         *------------------------------------------------------------*/
        CommitObjAndCoordBlocks(FALSE);

        if (m_poCurObjBlock->InitNewBlock(m_fp,512,
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
            m_poCurCoordBlock = NULL;
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
    if (m_poCurObjBlock == NULL)
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
            m_poCurCoordBlock = NULL;
            delete m_poCurObjBlock;
            m_poCurObjBlock = NULL;
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
        int nTotalCoordSize = m_poCurCoordBlock->GetNumBlocksInChain()*512;
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
            m_poCurCoordBlock = NULL;
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
        GInt32 nXMin, nYMin, nXMax, nYMax;

        if (m_poSpIndex == NULL)
        {
            // Spatial Index not created yet...
            m_poSpIndex = new TABMAPIndexBlock(m_eAccessMode);

            m_poSpIndex->InitNewBlock(m_fp, 512, 
                                      m_oBlockManager.AllocNewBlock("INDEX"));
            m_poSpIndex->SetMAPBlockManagerRef(&m_oBlockManager);

            m_poHeader->m_nFirstIndexBlock = m_poSpIndex->GetNodeBlockPtr();
        }

        m_poCurObjBlock->GetMBR(nXMin, nYMin, nXMax, nYMax);
        nStatus = m_poSpIndex->AddEntry(nXMin, nYMin, nXMax, nYMax,
                                        m_poCurObjBlock->GetStartAddress());

        m_poHeader->m_nMaxSpIndexDepth = MAX(m_poHeader->m_nMaxSpIndexDepth,
                                      (GByte)m_poSpIndex->GetCurMaxDepth()+1);
    }

    /*-----------------------------------------------------------------
     * Delete obj block only if requested
     *----------------------------------------------------------------*/
    if (bDeleteObjects)
    {
        delete m_poCurObjBlock;
        m_poCurObjBlock = NULL;
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
    TABRawBinBlock *poBlock = NULL;

    /*-----------------------------------------------------------------
     * In Write mode, if an object block is already in memory then flush it
     *----------------------------------------------------------------*/
    if (m_eAccessMode != TABRead && m_poCurObjBlock != NULL)
    {
        int nStatus = CommitObjAndCoordBlocks(TRUE);
        if (nStatus != 0)
            return nStatus;
    }

    /*-----------------------------------------------------------------
     * Load Obj Block
     *----------------------------------------------------------------*/
    poBlock = TABCreateMAPBlockFromFile(m_fp, 
                                             nBlockPtr,
                                             512, TRUE, TABReadWrite);
    if (poBlock != NULL &&
        poBlock->GetBlockClass() == TABMAP_OBJECT_BLOCK)
    {
        m_poCurObjBlock = (TABMAPObjectBlock*)poBlock;
        poBlock = NULL;
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
        m_poCurCoordBlock = NULL;
        return 0;
    }

    poBlock = TABCreateMAPBlockFromFile(m_fp, 
                                   m_poCurObjBlock->GetLastCoordBlockAddress(),
                                                  512, TRUE, TABReadWrite);
    if (poBlock != NULL && poBlock->GetBlockClass() == TABMAP_COORD_BLOCK)
    {
        m_poCurCoordBlock = (TABMAPCoordBlock*)poBlock;
        m_poCurCoordBlock->SetMAPBlockManagerRef(&m_oBlockManager);
        poBlock = NULL;
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
    TABMAPObjHdr **papoSrcObjHdrs = NULL, *poObjHdr=NULL;
    int i, numSrcObj = 0;

    /*-----------------------------------------------------------------
     * Read all object headers
     *----------------------------------------------------------------*/
    m_poCurObjBlock->Rewind();
    while ((poObjHdr = TABMAPObjHdr::ReadNextObj(m_poCurObjBlock, 
                                                 m_poHeader)) != NULL)
    {
        if (papoSrcObjHdrs == NULL || numSrcObj%10 == 0)
        {
            // Realloc the array... by steps of 10
            papoSrcObjHdrs = (TABMAPObjHdr**)CPLRealloc(papoSrcObjHdrs, 
                                                        (numSrcObj+10)*
                                                        sizeof(TABMAPObjHdr*));
        }
        papoSrcObjHdrs[numSrcObj++] = poObjHdr;
    }
    /* PickSeedsForSplit (reasonably) assumes at least 2 nodes */
    CPLAssert(numSrcObj > 1);

    /*-----------------------------------------------------------------
     * Reset current obj and coord block 
     *----------------------------------------------------------------*/
    GInt32 nFirstSrcCoordBlock = m_poCurObjBlock->GetFirstCoordBlockAddress();

    m_poCurObjBlock->InitNewBlock(m_fp, 512, 
                                  m_poCurObjBlock->GetStartAddress());

    TABMAPCoordBlock *poSrcCoordBlock = m_poCurCoordBlock;
    m_poCurCoordBlock = NULL;

    /*-----------------------------------------------------------------
     * Create new obj and coord block
     *----------------------------------------------------------------*/
    TABMAPObjectBlock *poNewObjBlock = new TABMAPObjectBlock(m_eAccessMode);
    poNewObjBlock->InitNewBlock(m_fp, 512, m_oBlockManager.AllocNewBlock("OBJECT"));

    /* Use existing center of other block in case we have compressed objects
       and freeze it */
    poNewObjBlock->SetCenterFromOtherBlock(m_poCurObjBlock);

    /* Coord block will be alloc'd automatically*/
    TABMAPCoordBlock *poNewCoordBlock = NULL;  

    /*-----------------------------------------------------------------
     * Pick Seeds for each block
     *----------------------------------------------------------------*/
    TABMAPIndexEntry *pasSrcEntries = 
        (TABMAPIndexEntry*)CPLMalloc(numSrcObj*sizeof(TABMAPIndexEntry));
    for (i=0; i<numSrcObj; i++)
    {
        pasSrcEntries[i].XMin = papoSrcObjHdrs[i]->m_nMinX;
        pasSrcEntries[i].YMin = papoSrcObjHdrs[i]->m_nMinY;
        pasSrcEntries[i].XMax = papoSrcObjHdrs[i]->m_nMaxX;
        pasSrcEntries[i].YMax = papoSrcObjHdrs[i]->m_nMaxY;
    }

    int nSeed1, nSeed2;
    TABMAPIndexBlock::PickSeedsForSplit(pasSrcEntries, numSrcObj, -1,
                                        poObjHdrToAdd->m_nMinX,
                                        poObjHdrToAdd->m_nMinY,
                                        poObjHdrToAdd->m_nMaxX,
                                        poObjHdrToAdd->m_nMaxY,
                                        nSeed1, nSeed2);
    CPLFree(pasSrcEntries);
    pasSrcEntries = NULL;

    /*-----------------------------------------------------------------
     * Assign the seeds to their respective block
     *----------------------------------------------------------------*/
    // Insert nSeed1 in this block
    poObjHdr = papoSrcObjHdrs[nSeed1];
    if (MoveObjToBlock(poObjHdr, poSrcCoordBlock,
                       m_poCurObjBlock, &m_poCurCoordBlock) <= 0)
        return NULL;

    // Move nSeed2 to 2nd block
    poObjHdr = papoSrcObjHdrs[nSeed2];
    if (MoveObjToBlock(poObjHdr, poSrcCoordBlock,
                       poNewObjBlock, &poNewCoordBlock) <= 0)
        return NULL;

    /*-----------------------------------------------------------------
     * Go through the rest of the entries and assign them to one 
     * of the 2 blocks
     *
     * Criteria is minimal area difference.
     * Resolve ties by adding the entry to the block with smaller total
     * area, then to the one with fewer entries, then to either.
     *----------------------------------------------------------------*/
    for(int iEntry=0; iEntry<numSrcObj; iEntry++)
    {
        if (iEntry == nSeed1 || iEntry == nSeed2)
            continue;

        poObjHdr = papoSrcObjHdrs[iEntry];

        int nObjSize = m_poHeader->GetMapObjectSize(poObjHdr->m_nType);

        // If one of the two blocks is almost full then all remaining
        // entries should go to the other block
        if (m_poCurObjBlock->GetNumUnusedBytes() < nObjSize+nSizeOfObjToAdd )
        {
            if (MoveObjToBlock(poObjHdr, poSrcCoordBlock,
                               poNewObjBlock, &poNewCoordBlock) <= 0)
                return NULL;
            continue;
        }
        else if (poNewObjBlock->GetNumUnusedBytes() < nObjSize+nSizeOfObjToAdd)
        {
            if (MoveObjToBlock(poObjHdr, poSrcCoordBlock,
                               m_poCurObjBlock, &m_poCurCoordBlock) <= 0)
                return NULL;
            continue;
        }


        // Decide which of the two blocks to put this entry in
        GInt32 nXMin, nYMin, nXMax, nYMax;
        m_poCurObjBlock->GetMBR(nXMin, nYMin, nXMax, nYMax);
        double dAreaDiff1 = 
            TABMAPIndexBlock::ComputeAreaDiff(nXMin, nYMin, 
                                              nXMax, nYMax,
                                              poObjHdr->m_nMinX, 
                                              poObjHdr->m_nMinY,
                                              poObjHdr->m_nMaxX,
                                              poObjHdr->m_nMaxY);

        poNewObjBlock->GetMBR(nXMin, nYMin, nXMax, nYMax);
        double dAreaDiff2 = 
            TABMAPIndexBlock::ComputeAreaDiff(nXMin, nYMin, nXMax, nYMax,
                                              poObjHdr->m_nMinX, 
                                              poObjHdr->m_nMinY,
                                              poObjHdr->m_nMaxX,
                                              poObjHdr->m_nMaxY);

        if (dAreaDiff1 < dAreaDiff2)
        {
            // This entry stays in this block
            if (MoveObjToBlock(poObjHdr, poSrcCoordBlock,
                               m_poCurObjBlock, &m_poCurCoordBlock) <= 0)
                return NULL;
        }
        else
        {
            // This entry goes to new block
            if (MoveObjToBlock(poObjHdr, poSrcCoordBlock,
                               poNewObjBlock, &poNewCoordBlock) <= 0)
                return NULL;
        }
    }

    /* Cleanup papoSrcObjHdrs[] */
    for(i=0; i<numSrcObj; i++)
    {
        delete papoSrcObjHdrs[i];
    }
    CPLFree(papoSrcObjHdrs);
    papoSrcObjHdrs = NULL;

    /*-----------------------------------------------------------------
     * Delete second coord block if one was created
     * Refs to coord block were kept up to date by MoveObjToBlock()
     * We just need to commit to file and delete the object now.
     *----------------------------------------------------------------*/
    if (poNewCoordBlock)
    {
        if (poNewCoordBlock->CommitToFile() != 0)
            return NULL;
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
                return NULL;
        }

        int nNextCoordBlock = poSrcCoordBlock->GetNextCoordBlock();
        while(poSrcCoordBlock != NULL)
        {
            // Mark this block as deleted
            if (poSrcCoordBlock->CommitAsDeleted(m_oBlockManager.
                                                 GetFirstGarbageBlock()) != 0)
                return NULL;
            m_oBlockManager.PushGarbageBlockAsFirst(poSrcCoordBlock->GetStartAddress());

            // Advance to next
            if (nNextCoordBlock > 0)
            {
                if (poSrcCoordBlock->GotoByteInFile(nNextCoordBlock, TRUE) != 0)
                    return NULL;
                nNextCoordBlock = poSrcCoordBlock->GetNextCoordBlock();
            }
            else
            {
                // end of chain
                delete poSrcCoordBlock;
                poSrcCoordBlock = NULL;
            }
        }
    }
            

    if (poNewObjBlock->CommitToFile() != 0)
        return NULL;

    return poNewObjBlock;
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
        TABMAPObjHdrWithCoord *poObjHdrCoord =(TABMAPObjHdrWithCoord*)poObjHdr;
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
        if (*ppoCoordBlock == NULL)
        {
            *ppoCoordBlock = new TABMAPCoordBlock(m_eAccessMode==TABWrite?
                                                  TABReadWrite: 
                                                  m_eAccessMode);
            (*ppoCoordBlock)->InitNewBlock(m_fp, 512, 
                                           m_oBlockManager.AllocNewBlock("COORD"));
            (*ppoCoordBlock)->SetMAPBlockManagerRef(&m_oBlockManager);

            // Set the references to this coord block in the MAPObjBlock
            poObjBlock->AddCoordBlockRef((*ppoCoordBlock)->GetStartAddress());

        }

        if ((*ppoCoordBlock)->GetNumUnusedBytes() < 4)
        {
            int nNewBlockOffset = m_oBlockManager.AllocNewBlock("COORD");
            (*ppoCoordBlock)->SetNextCoordBlock(nNewBlockOffset);
            (*ppoCoordBlock)->CommitToFile();
            (*ppoCoordBlock)->InitNewBlock(m_fp, 512, nNewBlockOffset);
        }

        // Make sure read/write pointer is at the end of the block
        (*ppoCoordBlock)->SeekEnd();

        if (CPLGetLastErrorNo() != 0 && CPLGetLastErrorType() == CE_Failure)
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
    if (m_poCurCoordBlock == NULL)
    {
        m_poCurCoordBlock = new TABMAPCoordBlock(m_eAccessMode);
        m_poCurCoordBlock->InitNewBlock(m_fp, 512);
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
        return NULL;
    }

    if (nFileOffset % 512 == 0)
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
    GByte abyData[512];

    if (VSIFSeekL(m_fp, nFileOffset, SEEK_SET) != 0 
        || VSIFReadL(abyData, sizeof(GByte), 512, m_fp) != 512 )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "GetIndexBlock() failed reading %d bytes at offset %d.",
                 512, nFileOffset);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create and initialize depending on the block type.              */
/* -------------------------------------------------------------------- */
    int nBlockType = abyData[0];
    TABRawBinBlock *poBlock;

    if( nBlockType == TABMAP_INDEX_BLOCK )
    {
        TABMAPIndexBlock* poIndexBlock = new TABMAPIndexBlock(m_eAccessMode);
        poBlock = poIndexBlock;
        poIndexBlock->SetMAPBlockManagerRef(&m_oBlockManager);
    }
    else
        poBlock = new TABMAPObjectBlock(m_eAccessMode);
    
    if( poBlock->InitBlockFromData(abyData, 512, 512,
                                   TRUE, m_fp, nFileOffset) == -1 )
    {
        delete poBlock;
        poBlock = NULL;
    }

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
 * Reutrns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::InitDrawingTools()
{
    int nStatus = 0;

    if (m_poHeader == NULL)
        return -1;    // File not opened yet!

    /*-------------------------------------------------------------
     * We want to perform this initialisation only ONCE
     *------------------------------------------------------------*/
    if (m_poToolDefTable != NULL)
        return 0;

    /*-------------------------------------------------------------
     * Create a new ToolDefTable... no more initialization is required 
     * unless we want to read tool blocks from file.
     *------------------------------------------------------------*/
    m_poToolDefTable = new TABToolDefTable;

    if ((m_eAccessMode == TABRead || m_eAccessMode == TABReadWrite) &&
        m_poHeader->m_nFirstToolBlock != 0)
    {
        TABMAPToolBlock *poBlock;

        poBlock = new TABMAPToolBlock(TABRead);
        poBlock->InitNewBlock(m_fp, 512);
    
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

    if (m_eAccessMode == TABRead || m_poHeader == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
            "CommitDrawingTools() failed: file not opened for write access.");
        return -1;
    }

    if (m_poToolDefTable == NULL ||
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
    TABMAPToolBlock *poBlock;
    
    poBlock = new TABMAPToolBlock(m_eAccessMode);
    if( m_poHeader->m_nFirstToolBlock != 0 )
        poBlock->InitNewBlock(m_fp, 512, m_poHeader->m_nFirstToolBlock);
    else
        poBlock->InitNewBlock(m_fp, 512, m_oBlockManager.AllocNewBlock("TOOL"));
    poBlock->SetMAPBlockManagerRef(&m_oBlockManager);

    m_poHeader->m_nFirstToolBlock = poBlock->GetStartAddress();

    m_poHeader->m_numPenDefs = (GByte)m_poToolDefTable->GetNumPen();
    m_poHeader->m_numBrushDefs = (GByte)m_poToolDefTable->GetNumBrushes();
    m_poHeader->m_numFontDefs = (GByte)m_poToolDefTable->GetNumFonts();
    m_poHeader->m_numSymbolDefs = (GByte)m_poToolDefTable->GetNumSymbols();

    /*-------------------------------------------------------------
     * Do the actual work and delete poBlock
     * (Note that poBlock will have already been committed to the file
     * by WriteAllToolDefs() )
     *------------------------------------------------------------*/
    nStatus = m_poToolDefTable->WriteAllToolDefs(poBlock);
    
    m_poHeader->m_numMapToolBlocks = (GInt16)poBlock->GetNumBlocksInChain();

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
    TABPenDef *psTmp;

    if (m_poToolDefTable == NULL && InitDrawingTools() != 0)
        return -1;

    if (psDef && m_poToolDefTable &&
        (psTmp = m_poToolDefTable->GetPenDefRef(nPenIndex)) != NULL)
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
    if (psDef == NULL || 
        (m_poToolDefTable == NULL && InitDrawingTools() != 0) ||
        m_poToolDefTable==NULL )
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
    TABBrushDef *psTmp;

    if (m_poToolDefTable == NULL && InitDrawingTools() != 0)
        return -1;

    if (psDef && m_poToolDefTable &&
        (psTmp = m_poToolDefTable->GetBrushDefRef(nBrushIndex)) != NULL)
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
    if (psDef == NULL || 
        (m_poToolDefTable == NULL && InitDrawingTools() != 0) ||
        m_poToolDefTable==NULL )
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
    TABFontDef *psTmp;

    if (m_poToolDefTable == NULL && InitDrawingTools() != 0)
        return -1;

    if (psDef && m_poToolDefTable &&
        (psTmp = m_poToolDefTable->GetFontDefRef(nFontIndex)) != NULL)
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
    if (psDef == NULL || 
        (m_poToolDefTable == NULL && InitDrawingTools() != 0) ||
        m_poToolDefTable==NULL )
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
    TABSymbolDef *psTmp;

    if (m_poToolDefTable == NULL && InitDrawingTools() != 0)
        return -1;

    if (psDef && m_poToolDefTable &&
        (psTmp = m_poToolDefTable->GetSymbolDefRef(nSymbolIndex)) != NULL)
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
    if (psDef == NULL || 
        (m_poToolDefTable == NULL && InitDrawingTools() != 0) ||
        m_poToolDefTable==NULL )
    {
        return -1;
    }

    return m_poToolDefTable->AddSymbolDefRef(psDef);
}

#define ORDER_MIN_MAX(type,min,max)                                    \
    {   if( (max) < (min) )                                            \
          { type temp = (max); (max) = (min); (min) = temp; } }

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

    ORDER_MIN_MAX(int,m_XMinFilter,m_XMaxFilter);
    ORDER_MIN_MAX(int,m_YMinFilter,m_YMaxFilter);
    ORDER_MIN_MAX(double,m_sMinFilter.x,m_sMaxFilter.x);
    ORDER_MIN_MAX(double,m_sMinFilter.y,m_sMaxFilter.y);
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

    ORDER_MIN_MAX(int,m_XMinFilter,m_XMaxFilter);
    ORDER_MIN_MAX(int,m_YMinFilter,m_YMaxFilter);
    ORDER_MIN_MAX(double,m_sMinFilter.x,m_sMaxFilter.x);
    ORDER_MIN_MAX(double,m_sMinFilter.y,m_sMaxFilter.y);
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
void TABMAPFile::GetCoordFilter(TABVertex &sMin, TABVertex &sMax)
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
    if (m_eAccessMode == TABRead || m_poHeader == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
            "CommitSpatialIndex() failed: file not opened for write access.");
        return -1;
    }

    if (m_poSpIndex == NULL)
    {
        return 0;       // Nothing to do!
    }

    /*-------------------------------------------------------------
     * Update header fields and commit index block
     * (it's children will be recursively committed as well)
     *------------------------------------------------------------*/
    // Add 1 to Spatial Index Depth to account to the MapObjectBlocks
    m_poHeader->m_nMaxSpIndexDepth = MAX(m_poHeader->m_nMaxSpIndexDepth,
                                  (GByte)m_poSpIndex->GetCurMaxDepth()+1);

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

    return MAX(nToolVersion, m_nMinTABVersion);
}


/**********************************************************************
 *                   TABMAPFile::Dump()
 *
 * Dump block contents... available only in DEBUG mode.
 **********************************************************************/
#ifdef DEBUG

void TABMAPFile::Dump(FILE *fpOut /*=NULL*/)
{
    if (fpOut == NULL)
        fpOut = stdout;

    fprintf(fpOut, "----- TABMAPFile::Dump() -----\n");

    if (m_fp == NULL)
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
    if (poNode == NULL)
    {
        if (m_poHeader && m_poHeader->m_nFirstIndexBlock != 0)
        {
            TABRawBinBlock *poBlock;

            poBlock = GetIndexObjectBlock(m_poHeader->m_nFirstIndexBlock);
            if (poBlock && poBlock->GetBlockType() == TABMAP_INDEX_BLOCK)
                poNode = (TABMAPIndexBlock *)poBlock;
        }

        if (poNode == NULL)
            return;
    }


    /*-------------------------------------------------------------
     * Report info on current tree node
     *------------------------------------------------------------*/
    int numEntries = poNode->GetNumEntries();
    GInt32 nXMin, nYMin, nXMax, nYMax;
    double dXMin, dYMin, dXMax, dYMax;

    poNode->RecomputeMBR();
    poNode->GetMBR(nXMin, nYMin, nXMax, nYMax);

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

            TABRawBinBlock *poBlock;
            poBlock = GetIndexObjectBlock( psEntry->nBlockPtr );
            if( poBlock == NULL )
                continue;

            if( poBlock->GetBlockType() == TABMAP_INDEX_BLOCK )
            {
                /* Index block, dump recursively */
                DumpSpatialIndexToMIF((TABMAPIndexBlock *)poBlock, 
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
