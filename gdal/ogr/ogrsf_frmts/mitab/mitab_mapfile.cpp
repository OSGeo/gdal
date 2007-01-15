/**********************************************************************
 * $Id: mitab_mapfile.cpp,v 1.32 2005/10/06 19:15:31 dmorissette Exp $
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

    m_poCurObjBlock = NULL;
    m_nCurObjPtr = -1;
    m_nCurObjType = -1;
    m_nCurObjId = -1;
    m_poCurCoordBlock = NULL;
    m_poToolDefTable = NULL;
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
int TABMAPFile::Open(const char *pszFname, const char *pszAccess,
                     GBool bNoErrorMsg /* = FALSE */)
{
    FILE        *fp=NULL;
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

    /*-----------------------------------------------------------------
     * Validate access mode and make sure we use binary access.
     *----------------------------------------------------------------*/
    if (EQUALN(pszAccess, "r", 1))
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
     * Open file
     *----------------------------------------------------------------*/
    fp = VSIFOpen(pszFname, pszAccess);

    m_oBlockManager.Reset();

    if (fp != NULL && m_eAccessMode == TABRead)
    {
        /*-----------------------------------------------------------------
         * Read access: try to read header block
         * First try with a 512 bytes block to check the .map version.
         * If it's version 500 or more then read again a 1024 bytes block
         *----------------------------------------------------------------*/
        poBlock = TABCreateMAPBlockFromFile(fp, 0, 512);

        if (poBlock && poBlock->GetBlockClass() == TABMAP_HEADER_BLOCK &&
            ((TABMAPHeaderBlock*)poBlock)->m_nMAPVersionNumber >= 500)
        {
            // Version 500 or higher.  Read 1024 bytes block instead of 512
            delete poBlock;
            poBlock = TABCreateMAPBlockFromFile(fp, 0, 1024);
        }

        if (poBlock==NULL || poBlock->GetBlockClass() != TABMAP_HEADER_BLOCK)
        {
            if (poBlock)
                delete poBlock;
            poBlock = NULL;
            VSIFClose(fp);
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
        poBlock->InitNewBlock(fp, 1024, m_oBlockManager.AllocNewBlock() );

        // Alloc a second 512 bytes of space since oBlockManager deals 
        // with 512 bytes blocks.
        m_oBlockManager.AllocNewBlock(); 
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
     * Create a TABMAPObjectBlock, in READ mode only.
     *
     * In WRITE mode, the object block will be created only when needed.
     * We do not create the object block in the open() call because
     * files that contained only "NONE" geometries ended up with empty
     * object and spatial index blocks.
     *----------------------------------------------------------------*/

    if (m_eAccessMode == TABRead)
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
    if (m_poIdIndex->Open(pszFname, pszAccess) != 0)
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
    if (m_eAccessMode == TABRead)
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

    /*-----------------------------------------------------------------
     * Initialization of the Drawing Tools table will be done automatically
     * as Read/Write calls are done later.
     *----------------------------------------------------------------*/
    m_poToolDefTable = NULL;

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
    if (m_eAccessMode == TABWrite)
    {
        // Start by committing current object and coord blocks
        // Nothing happens if none has been created yet.
        CommitObjBlock(FALSE);

        // Write the drawing tools definitions now.
        CommitDrawingTools();

        // Commit spatial index blocks
        CommitSpatialIndex();

        // __TODO__ We probably need to update some header fields first.
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
        m_nCurObjType = -1;
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
        VSIFClose(m_fp);
    m_fp = NULL;

    CPLFree(m_pszFname);
    m_pszFname = NULL;

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
        m_nCurObjType = 0;
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
        CPLAssert( m_poSpIndex == NULL && m_poSpIndexLeaf == NULL );

        if( PushBlock( m_poHeader->m_nFirstIndexBlock ) == NULL )
            return FALSE;

        if( m_poSpIndex == NULL )
        {
            CPLAssert( m_poCurObjBlock != NULL );
            return TRUE;
        }
    }

    while( m_poSpIndexLeaf != NULL )
    {
        int     iEntry = m_poSpIndexLeaf->GetCurChildIndex();

        if( iEntry >= m_poSpIndexLeaf->GetNumEntries()-1 )
        {
            TABMAPIndexBlock *poParent = m_poSpIndexLeaf->GetParentRef();
            delete m_poSpIndexLeaf;
            m_poSpIndexLeaf = poParent;
            
            if( poParent != NULL )
            {
                poParent->SetCurChildRef( NULL, poParent->GetCurChildIndex() );
            }
            else
            {
                m_poSpIndex = NULL;
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
    if (m_poSpIndex && m_eAccessMode == TABRead )
    {
        delete m_poSpIndex;
        m_poSpIndex = NULL;
        m_poSpIndexLeaf = NULL;
    }
}

/************************************************************************/
/*                          GetNextFeatureId()                          */
/*                                                                      */
/*      Fetch the next feature id based on a traversal of the           */
/*      spatial index.                                                  */
/************************************************************************/

int TABMAPFile::GetNextFeatureId( int nPrevId )

{
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
        m_nCurObjPtr = m_nCurObjId = m_nCurObjType = -1;
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
    else if ( m_poCurObjBlock->GotoByteInFile(nFileOffset) == 0)
    {
        /*-------------------------------------------------------------
         * OK, it worked, read the object type and row id.
         *------------------------------------------------------------*/
        m_nCurObjPtr = nFileOffset;
        m_nCurObjType = m_poCurObjBlock->ReadByte();
        m_nCurObjId   = m_poCurObjBlock->ReadInt32();

        // Do a consistency check...
        if (m_nCurObjId != nObjId)
        {
            CPLError(CE_Failure, CPLE_FileIO,
                 "Object ID from the .ID file (%d) differs from the value "
                 "in the .MAP file (%d).  File may be corrupt.",
                 nObjId, m_nCurObjId);
            m_nCurObjPtr = m_nCurObjId = m_nCurObjType = -1;
            return -1;
        }
    }
    else
    {
        /*---------------------------------------------------------
         * Failed positioning input file... CPLError has been called.
         *--------------------------------------------------------*/
        m_nCurObjPtr = m_nCurObjId = m_nCurObjType = -1;
        return -1;
    }

    return 0;
}

/**********************************************************************
 *                   TABMAPFile::UpdateMapHeaderInfo()
 *
 * Update .map header information (counter of objects by type and minimum
 * required version) in light of a new object to be written to the file.
 *
 * Called only by PrepareNewObj() and by the TABCollection class.
 **********************************************************************/
void  TABMAPFile::UpdateMapHeaderInfo(GByte nObjType)
{
    /*-----------------------------------------------------------------
     * Update count of objects by type in the header block
     *----------------------------------------------------------------*/
    if (nObjType == TAB_GEOM_SYMBOL ||
        nObjType == TAB_GEOM_FONTSYMBOL ||
        nObjType == TAB_GEOM_CUSTOMSYMBOL ||
        nObjType == TAB_GEOM_MULTIPOINT ||
        nObjType == TAB_GEOM_SYMBOL_C ||
        nObjType == TAB_GEOM_FONTSYMBOL_C ||
        nObjType == TAB_GEOM_CUSTOMSYMBOL_C ||
        nObjType == TAB_GEOM_MULTIPOINT_C )
    {
        m_poHeader->m_numPointObjects++;
    }
    else if (nObjType == TAB_GEOM_LINE ||
             nObjType == TAB_GEOM_PLINE ||
             nObjType == TAB_GEOM_MULTIPLINE ||
             nObjType == TAB_GEOM_V450_MULTIPLINE ||
             nObjType == TAB_GEOM_ARC ||
             nObjType == TAB_GEOM_LINE_C ||
             nObjType == TAB_GEOM_PLINE_C ||
             nObjType == TAB_GEOM_MULTIPLINE_C ||
             nObjType == TAB_GEOM_V450_MULTIPLINE_C ||
             nObjType == TAB_GEOM_ARC_C)
    {
        m_poHeader->m_numLineObjects++;
    }
    else if (nObjType == TAB_GEOM_REGION ||
             nObjType == TAB_GEOM_V450_REGION ||
             nObjType == TAB_GEOM_RECT ||
             nObjType == TAB_GEOM_ROUNDRECT ||
             nObjType == TAB_GEOM_ELLIPSE ||
             nObjType == TAB_GEOM_REGION_C ||
             nObjType == TAB_GEOM_V450_REGION_C ||
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
     * Check for V450-specific object types and minimum TAB file version number
     *----------------------------------------------------------------*/
    if (m_nMinTABVersion < 450 &&
        (nObjType == TAB_GEOM_V450_REGION ||
         nObjType == TAB_GEOM_V450_MULTIPLINE ||
         nObjType == TAB_GEOM_V450_REGION_C ||
         nObjType == TAB_GEOM_V450_MULTIPLINE_C) )
    {
        m_nMinTABVersion = 450;
    }
    /*-----------------------------------------------------------------
     * Check for V460-specific object types (multipoint and collection)
     *----------------------------------------------------------------*/
    if (m_nMinTABVersion < 650 &&
        (nObjType == TAB_GEOM_MULTIPOINT   ||
         nObjType == TAB_GEOM_MULTIPOINT_C ||
         nObjType == TAB_GEOM_COLLECTION   ||
         nObjType == TAB_GEOM_COLLECTION_C    ) )
    {
        m_nMinTABVersion = 650;
    }

}

/**********************************************************************
 *                   TABMAPFile::PrepareNewObj()
 *
 * Get ready to write a new object of the specified type and with the 
 * specified id.  
 *
 * m_poCurObjBlock will be set to be ready to receive the new object, and
 * a new block will be created if necessary (in which case the current 
 * block contents will be committed to disk, etc.)  The object type and 
 * row ID will be written to the m_poCurObjBlock, so it will be ready to
 * receive the first byte of data for this map object.  
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
int   TABMAPFile::PrepareNewObj(int nObjId, GByte nObjType)
{
    int nObjSize;

    m_nCurObjPtr = m_nCurObjId = m_nCurObjType = -1;

    if (m_eAccessMode != TABWrite || 
        m_poIdIndex == NULL || m_poHeader == NULL)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "PrepareNewObj() failed: file not opened for write access.");
        return -1;
    }

    /*-----------------------------------------------------------------
     * For objects with no geometry, we just update the .ID file and return
     *----------------------------------------------------------------*/
    if (nObjType == TAB_GEOM_NONE)
    {
        m_nCurObjType = nObjType;
        m_nCurObjId   = nObjId;
        m_nCurObjPtr  = 0;
        m_poIdIndex->SetObjPtr(m_nCurObjId, 0);

        return 0;
    }

    /*-----------------------------------------------------------------
     * Update count of objects by type in the header block and minimum
     * required version.
     *----------------------------------------------------------------*/
    UpdateMapHeaderInfo(nObjType);

    /*-----------------------------------------------------------------
     * OK, looks like we will need object block... check if it exists and
     * create it if it has not been created yet (first time for this file).
     * We do not create the object block in the open() call because
     * files that contained only "NONE" geometries ended up with empty
     * object and spatial index blocks.
     * Note: A coord block will be created only if needed later.
     *----------------------------------------------------------------*/
    if (m_poCurObjBlock == NULL)
    {
        m_poCurObjBlock = new TABMAPObjectBlock(m_eAccessMode);

        int nBlockOffset = m_oBlockManager.AllocNewBlock();

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
    nObjSize = m_poHeader->GetMapObjectSize(nObjType);
    if (m_poCurObjBlock->GetNumUnusedBytes() < nObjSize )
    {
        /*-------------------------------------------------------------
         * OK, the new object won't fit in the current block.  Commit
         * the current block to disk, and init a new one.
         *
         * __TODO__ To create an optimum file, we should split the object
         * block at this point and update the index blocks... however the
         * file should still be usable even if we don't do it.
         *------------------------------------------------------------*/
        CommitObjBlock(TRUE);
    }

    /*-----------------------------------------------------------------
     * Init member vars and write new object type/id
     *----------------------------------------------------------------*/
    m_nCurObjType = nObjType;
    m_nCurObjId   = nObjId;
    m_nCurObjPtr = m_poCurObjBlock->GetFirstUnusedByteOffset();

    m_poCurObjBlock->GotoByteInFile(m_nCurObjPtr);

    m_poCurObjBlock->WriteByte(m_nCurObjType);
    m_poCurObjBlock->WriteInt32(m_nCurObjId);

    // Move write pointer to start location of next object (padding with zeros)
    // Nothing will get written to the object block until CommitToFile()
    // is called.
    m_poCurObjBlock->WriteZeros(m_poHeader->GetMapObjectSize(nObjType) - 5);

    /*-----------------------------------------------------------------
     * Update .ID Index
     *----------------------------------------------------------------*/
    m_poIdIndex->SetObjPtr(m_nCurObjId, m_nCurObjPtr);

    /*-----------------------------------------------------------------
     * Prepare Coords block... 
     * create a new TABMAPCoordBlock if it was not done yet.
     * Note that in write mode, TABCollections require read/write access
     * to the coord block.
     *----------------------------------------------------------------*/
    if (m_poHeader->MapObjectUsesCoordBlock(m_nCurObjType))
    {
        if (m_poCurCoordBlock == NULL)
        {
            m_poCurCoordBlock = new TABMAPCoordBlock(m_eAccessMode==TABWrite?
                                                     TABReadWrite: 
                                                     m_eAccessMode);
            m_poCurCoordBlock->InitNewBlock(m_fp, 512, 
                                            m_oBlockManager.AllocNewBlock());
            m_poCurCoordBlock->SetMAPBlockManagerRef(&m_oBlockManager);

            // Set the references to this coord block in the MAPObjBlock
            m_poCurObjBlock->AddCoordBlockRef(m_poCurCoordBlock->
                                                           GetStartAddress());

        }

        if (m_poCurCoordBlock->GetNumUnusedBytes() < 4)
        {
            int nNewBlockOffset = m_oBlockManager.AllocNewBlock();
            m_poCurCoordBlock->SetNextCoordBlock(nNewBlockOffset);
            m_poCurCoordBlock->CommitToFile();
            m_poCurCoordBlock->InitNewBlock(m_fp, 512, nNewBlockOffset);
        }
    }

    if (CPLGetLastErrorNo() != 0 && CPLGetLastErrorType() == CE_Failure)
        return -1;

    return 0;
}

/**********************************************************************
 *                   TABMAPFile::CommitObjBlock()
 *
 * Commit the TABMAPObjBlock and TABMAPCoordBlock to disk after updating
 * the references to each other and in the TABMAPIndex block.
 *
 * After the Commit() call, the ObjBlock and others will be reinitialized
 * and ready to receive new objects. (only if bInitNewBlock==TRUE)
 *
 * Returns 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::CommitObjBlock(GBool bInitNewBlock /*=TRUE*/)
{
    int nStatus = 0;

    /*-----------------------------------------------------------------
     * First check that a objBlock has been created.  It is possible to have
     * no object block in files that contain only "NONE" geometries.
     *----------------------------------------------------------------*/
    if (m_poCurObjBlock == NULL)
        return 0; 

    if (m_eAccessMode != TABWrite)
    {
        CPLError(CE_Failure, CPLE_AssertionFailed,
                 "CommitObjBlock() failed: file not opened for write access.");
        return -1;
    }

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
        delete m_poCurCoordBlock;
        m_poCurCoordBlock = NULL;
    }

    /*-----------------------------------------------------------------
     * Commit the obj block.
     *----------------------------------------------------------------*/
    if (nStatus == 0)
        nStatus = m_poCurObjBlock->CommitToFile();

    /*-----------------------------------------------------------------
     * Update the spatial index
     *
     * Spatial index will be created here if it was not done yet.
     *----------------------------------------------------------------*/
    if (nStatus == 0)
    {
        GInt32 nXMin, nYMin, nXMax, nYMax;

        if (m_poSpIndex == NULL)
        {
            // Spatial Index not created yet...
            m_poSpIndex = new TABMAPIndexBlock(m_eAccessMode);

            m_poSpIndex->InitNewBlock(m_fp, 512, 
                                      m_oBlockManager.AllocNewBlock());
            m_poSpIndex->SetMAPBlockManagerRef(&m_oBlockManager);

            m_poHeader->m_nFirstIndexBlock = m_poSpIndex->GetNodeBlockPtr();
        }

        m_poCurObjBlock->GetMBR(nXMin, nYMin, nXMax, nYMax);
        nStatus = m_poSpIndex->AddEntry(nXMin, nYMin, nXMax, nYMax,
                                        m_poCurObjBlock->GetStartAddress());

        m_poHeader->m_nMaxSpIndexDepth = MAX(m_poHeader->m_nMaxSpIndexDepth,
                                             m_poSpIndex->GetCurMaxDepth()+1);
    }

    /*-----------------------------------------------------------------
     * Reinitialize the obj block only if requested
     *----------------------------------------------------------------*/
    if (bInitNewBlock && nStatus == 0)
    {
        nStatus = m_poCurObjBlock->InitNewBlock(m_fp,512,
                                            m_oBlockManager.AllocNewBlock());
    }


    return nStatus;
}

/**********************************************************************
 *                   TABMAPFile::GetCurObjType()
 *
 * Return the MapInfo object type of the object that the m_poCurObjBlock
 * is pointing to.  This value is set after a call to MoveToObjId().
 *
 * Returns a value >= 0 on success, -1 on error.
 **********************************************************************/
int TABMAPFile::GetCurObjType()
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
    if (m_eAccessMode != TABRead)
        return NULL;

    if (m_poCurCoordBlock == NULL)
    {
        m_poCurCoordBlock = new TABMAPCoordBlock(m_eAccessMode);
        m_poCurCoordBlock->InitNewBlock(m_fp, 512);
    }

    /*-----------------------------------------------------------------
     * Use GotoByteInFile() to go to the requested location.  This will
     * force loading the block if necessary and reading its header.
     * If nFileOffset is at the beginning of the requested block, then
     * we make sure to move the read pointer past the 8 bytes header
     * to be ready to read coordinates data
     *----------------------------------------------------------------*/
    if ( m_poCurCoordBlock->GotoByteInFile(nFileOffset) != 0)
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

    if (VSIFSeek(m_fp, nFileOffset, SEEK_SET) != 0 
        || VSIFRead(abyData, sizeof(GByte), 512, m_fp) != 512 )
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
        poBlock = new TABMAPIndexBlock();
    else
        poBlock = new TABMAPObjectBlock();
    
    if( poBlock->InitBlockFromData(abyData,512,TRUE,m_fp,nFileOffset) == -1 )
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

    if (m_eAccessMode == TABRead && m_poHeader->m_nFirstToolBlock != 0)
    {
        TABMAPToolBlock *poBlock;

        poBlock = new TABMAPToolBlock(m_eAccessMode);
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

    if (m_eAccessMode != TABWrite || m_poHeader == NULL)
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
    poBlock->InitNewBlock(m_fp, 512, m_oBlockManager.AllocNewBlock());
    poBlock->SetMAPBlockManagerRef(&m_oBlockManager);

    m_poHeader->m_nFirstToolBlock = poBlock->GetStartAddress();

    m_poHeader->m_numPenDefs = m_poToolDefTable->GetNumPen();
    m_poHeader->m_numBrushDefs = m_poToolDefTable->GetNumBrushes();
    m_poHeader->m_numFontDefs = m_poToolDefTable->GetNumFonts();
    m_poHeader->m_numSymbolDefs = m_poToolDefTable->GetNumSymbols();

    /*-------------------------------------------------------------
     * Do the actual work and delete poBlock
     * (Note that poBlock will have already been committed to the file
     * by WriteAllToolDefs() )
     *------------------------------------------------------------*/
    nStatus = m_poToolDefTable->WriteAllToolDefs(poBlock);
    
    m_poHeader->m_numMapToolBlocks = poBlock->GetNumBlocksInChain();

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
    if (m_eAccessMode != TABWrite || m_poHeader == NULL)
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
                                         m_poSpIndex->GetCurMaxDepth()+1);

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
