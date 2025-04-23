/******************************************************************************
 *
 * Project:  DXF Translator
 * Purpose:  Implements OGRDXFBlocksWriterLayer used for capturing block
 *           definitions for writing to a DXF file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_dxf.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_featurestyle.h"

/************************************************************************/
/*                      OGRDXFBlocksWriterLayer()                       */
/************************************************************************/

OGRDXFBlocksWriterLayer::OGRDXFBlocksWriterLayer(OGRDXFWriterDS * /* poDS */)
    : poFeatureDefn(new OGRFeatureDefn("blocks"))
{
    poFeatureDefn->Reference();

    OGRDXFDataSource::AddStandardFields(poFeatureDefn, ODFM_IncludeBlockFields);
}

/************************************************************************/
/*                      ~OGRDXFBlocksWriterLayer()                      */
/************************************************************************/

OGRDXFBlocksWriterLayer::~OGRDXFBlocksWriterLayer()

{
    for (size_t i = 0; i < apoBlocks.size(); i++)
        delete apoBlocks[i];

    if (poFeatureDefn)
        poFeatureDefn->Release();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDXFBlocksWriterLayer::TestCapability(const char *pszCap)

{
    return EQUAL(pszCap, OLCSequentialWrite);
}

/************************************************************************/
/*                            CreateField()                             */
/*                                                                      */
/*      This is really a dummy as our fields are precreated.            */
/************************************************************************/

OGRErr OGRDXFBlocksWriterLayer::CreateField(const OGRFieldDefn *poField,
                                            int bApproxOK)

{
    if (poFeatureDefn->GetFieldIndex(poField->GetNameRef()) >= 0 && bApproxOK)
        return OGRERR_NONE;

    CPLError(CE_Failure, CPLE_AppDefined,
             "DXF layer does not support arbitrary field creation, field '%s' "
             "not created.",
             poField->GetNameRef());

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/*                                                                      */
/*      We just stash a copy of the features for later writing to       */
/*      the blocks section of the header.                               */
/************************************************************************/

OGRErr OGRDXFBlocksWriterLayer::ICreateFeature(OGRFeature *poFeature)

{
    apoBlocks.push_back(poFeature->Clone());

    return OGRERR_NONE;
}

/************************************************************************/
/*                             FindBlock()                              */
/************************************************************************/

OGRFeature *OGRDXFBlocksWriterLayer::FindBlock(const char *pszBlockName)

{
    for (size_t i = 0; i < apoBlocks.size(); i++)
    {
        const char *pszThisName = apoBlocks[i]->GetFieldAsString("Block");

        if (pszThisName != nullptr && strcmp(pszBlockName, pszThisName) == 0)
            return apoBlocks[i];
    }

    return nullptr;
}
