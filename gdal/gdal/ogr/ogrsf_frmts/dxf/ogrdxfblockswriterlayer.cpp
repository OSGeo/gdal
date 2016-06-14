/******************************************************************************
 * $Id: ogrdxfwriterlayer.cpp 20670 2010-09-22 00:21:17Z warmerdam $
 *
 * Project:  DXF Translator
 * Purpose:  Implements OGRDXFBlocksWriterLayer used for capturing block
 *           definitions for writing to a DXF file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Frank Warmerdam <warmerdam@pobox.com>
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
#include "ogr_featurestyle.h"

CPL_CVSID("$Id: ogrdxfwriterlayer.cpp 20670 2010-09-22 00:21:17Z warmerdam $");

/************************************************************************/
/*                      OGRDXFBlocksWriterLayer()                       */
/************************************************************************/

OGRDXFBlocksWriterLayer::OGRDXFBlocksWriterLayer(
    OGRDXFWriterDS * /* poDS */ ) :
    poFeatureDefn(new OGRFeatureDefn( "blocks" ))
{
    poFeatureDefn->Reference();

    OGRFieldDefn  oLayerField( "Layer", OFTString );
    poFeatureDefn->AddFieldDefn( &oLayerField );

    OGRFieldDefn  oClassField( "SubClasses", OFTString );
    poFeatureDefn->AddFieldDefn( &oClassField );

    OGRFieldDefn  oExtendedField( "ExtendedEntity", OFTString );
    poFeatureDefn->AddFieldDefn( &oExtendedField );

    OGRFieldDefn  oLinetypeField( "Linetype", OFTString );
    poFeatureDefn->AddFieldDefn( &oLinetypeField );

    OGRFieldDefn  oEntityHandleField( "EntityHandle", OFTString );
    poFeatureDefn->AddFieldDefn( &oEntityHandleField );

    OGRFieldDefn  oTextField( "Text", OFTString );
    poFeatureDefn->AddFieldDefn( &oTextField );

    OGRFieldDefn  oBlockField( "BlockName", OFTString );
    poFeatureDefn->AddFieldDefn( &oBlockField );
}

/************************************************************************/
/*                      ~OGRDXFBlocksWriterLayer()                      */
/************************************************************************/

OGRDXFBlocksWriterLayer::~OGRDXFBlocksWriterLayer()

{
    for( size_t i=0; i < apoBlocks.size(); i++ )
        delete apoBlocks[i];

    if( poFeatureDefn )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDXFBlocksWriterLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCSequentialWrite) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/*                                                                      */
/*      This is really a dummy as our fields are precreated.            */
/************************************************************************/

OGRErr OGRDXFBlocksWriterLayer::CreateField( OGRFieldDefn *poField,
                                             int bApproxOK )

{
    if( poFeatureDefn->GetFieldIndex(poField->GetNameRef()) >= 0
        && bApproxOK )
        return OGRERR_NONE;

    CPLError( CE_Failure, CPLE_AppDefined,
              "DXF layer does not support arbitrary field creation, field '%s' not created.",
              poField->GetNameRef() );

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/*                                                                      */
/*      We just stash a copy of the features for later writing to       */
/*      the blocks section of the header.                               */
/************************************************************************/

OGRErr OGRDXFBlocksWriterLayer::ICreateFeature( OGRFeature *poFeature )

{
    apoBlocks.push_back( poFeature->Clone() );

    return OGRERR_NONE;
}

/************************************************************************/
/*                             FindBlock()                              */
/************************************************************************/

OGRFeature *OGRDXFBlocksWriterLayer::FindBlock( const char *pszBlockName )

{
    for( size_t i=0; i < apoBlocks.size(); i++ )
    {
        const char *pszThisName = apoBlocks[i]->GetFieldAsString("BlockName");

        if( pszThisName != NULL && strcmp(pszBlockName,pszThisName) == 0 )
            return apoBlocks[i];
    }

    return NULL;
}
