/******************************************************************************
 *
 * Project:  DXF Translator
 * Purpose:  Implements BlockMap reading and management portion of
 *           OGRDXFDataSource class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "ogr_dxf.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"

#include <algorithm>

CPL_CVSID("$Id$")

/************************************************************************/
/*                          ReadBlockSection()                          */
/************************************************************************/

bool OGRDXFDataSource::ReadBlocksSection()

{
    // Force inlining of blocks to false, for when OGRDXFLayer processes
    // INSERT entities
    const bool bOldInlineBlocks = bInlineBlocks;
    bInlineBlocks = false;

    OGRDXFLayer *poReaderLayer = static_cast<OGRDXFLayer *>(
        GetLayerByName( "Entities" ));

    iEntitiesSectionOffset = oReader.iSrcBufferFileOffset + oReader.iSrcBufferOffset;

    char szLineBuf[257];
    int nCode = 0;
    while( (nCode = ReadValue( szLineBuf, sizeof(szLineBuf) )) > -1
           && !EQUAL(szLineBuf,"ENDSEC") )
    {
        // We are only interested in extracting blocks.
        if( nCode != 0 || !EQUAL(szLineBuf,"BLOCK") )
            continue;

        // Process contents of BLOCK definition till we find the
        // first entity.
        CPLString osBlockName;
        CPLString osBlockRecordHandle;

        while( (nCode = ReadValue( szLineBuf,sizeof(szLineBuf) )) > 0 )
        {
            if( nCode == 2 )
                osBlockName = szLineBuf;

            // get the block record handle as well, for arrowheads
            else if( nCode == 330 )
                osBlockRecordHandle = szLineBuf;

            // anything else we want?
        }
        if( nCode < 0 )
        {
            bInlineBlocks = bOldInlineBlocks;
            DXF_READER_ERROR();
            return false;
        }

        // store the block record handle mapping even if the block is empty
        oBlockRecordHandles[osBlockRecordHandle] = osBlockName;

        if( EQUAL(szLineBuf,"ENDBLK") )
            continue;

        if (nCode >= 0)
            UnreadValue();

        if( oBlockMap.find(osBlockName) != oBlockMap.end() )
        {
            bInlineBlocks = bOldInlineBlocks;
            DXF_READER_ERROR();
            return false;
        }

        // Now we will process entities till we run out at the ENDBLK code.

        PushBlockInsertion( osBlockName );

        OGRDXFFeature *poFeature = NULL;
        while( (poFeature = poReaderLayer->GetNextUnfilteredFeature()) != NULL )
            oBlockMap[osBlockName].apoFeatures.push_back( poFeature );

        PopBlockInsertion();
    }
    if( nCode < 0 )
    {
        bInlineBlocks = bOldInlineBlocks;
        DXF_READER_ERROR();
        return false;
    }

    CPLDebug( "DXF", "Read %d blocks with meaningful geometry.",
              (int) oBlockMap.size() );

    // Restore old inline blocks setting
    bInlineBlocks = bOldInlineBlocks;

    return true;
}

/************************************************************************/
/*                            LookupBlock()                             */
/*                                                                      */
/*      Find the geometry collection corresponding to a name if it      */
/*      exists.  Note that the returned geometry pointer is to a        */
/*      geometry that continues to be owned by the datasource.  It      */
/*      should be cloned for use.                                       */
/************************************************************************/

DXFBlockDefinition *OGRDXFDataSource::LookupBlock( const char *pszName )

{
    CPLString l_osName = pszName;

    if( oBlockMap.count( l_osName ) == 0 )
        return NULL;
    else
        return &(oBlockMap[l_osName]);
}

/************************************************************************/
/*                     GetBlockNameByRecordHandle()                     */
/*                                                                      */
/*      Find the name of the block with the given BLOCK_RECORD handle.  */
/*      If there is no such block, an empty string is returned.         */
/************************************************************************/

CPLString OGRDXFDataSource::GetBlockNameByRecordHandle( const char *pszID )

{
    CPLString l_osID = pszID;

    if( oBlockRecordHandles.count( l_osID ) == 0 )
        return "";
    else
        return oBlockRecordHandles[l_osID];
}

/************************************************************************/
/*                         PushBlockInsertion()                         */
/*                                                                      */
/*      Add a block name to the stack of blocks being inserted.         */
/*      Returns false if we are already inserting this block.           */
/************************************************************************/

bool OGRDXFDataSource::PushBlockInsertion( const CPLString& osBlockName )

{
    // Make sure we are not recursing too deeply (avoid stack overflows) or
    // inserting a block within itself (avoid billion-laughs type issues).
    // 128 is a totally arbitrary limit
    if( aosBlockInsertionStack.size() > 128 ||
        std::find( aosBlockInsertionStack.begin(),
            aosBlockInsertionStack.end(), osBlockName )
        != aosBlockInsertionStack.end() )
    {
        CPLDebug( "DXF", "Dangerous block recursion detected. "
            "Some blocks have not been inserted." );
        return false;
    }

    aosBlockInsertionStack.push_back( osBlockName );
    return true;
}

/************************************************************************/
/*                        ~DXFBlockDefinition()                         */
/*                                                                      */
/*      Safe cleanup of a block definition.                             */
/************************************************************************/

DXFBlockDefinition::~DXFBlockDefinition()
{
    while( !apoFeatures.empty() )
    {
        delete apoFeatures.back();
        apoFeatures.pop_back();
    }
}
