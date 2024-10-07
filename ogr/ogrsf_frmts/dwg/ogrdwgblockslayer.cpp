/******************************************************************************
 *
 * Project:  DWG Translator
 * Purpose:  Implements OGRDWGBlocksLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2010, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_dwg.h"
#include "cpl_conv.h"

/************************************************************************/
/*                         OGRDWGBlocksLayer()                          */
/************************************************************************/

OGRDWGBlocksLayer::OGRDWGBlocksLayer(OGRDWGDataSource *poDSIn)
    : poDS(poDSIn), poFeatureDefn(new OGRFeatureDefn("blocks"))
{
    OGRDWGBlocksLayer::ResetReading();

    poFeatureDefn->Reference();

    poDS->AddStandardFields(poFeatureDefn);
}

/************************************************************************/
/*                         ~OGRDWGBlocksLayer()                         */
/************************************************************************/

OGRDWGBlocksLayer::~OGRDWGBlocksLayer()

{
    if (m_nFeaturesRead > 0 && poFeatureDefn != nullptr)
    {
        CPLDebug("DWG", "%d features read on layer '%s'.", (int)m_nFeaturesRead,
                 poFeatureDefn->GetName());
    }

    if (poFeatureDefn)
        poFeatureDefn->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRDWGBlocksLayer::ResetReading()

{
    iNextFID = 0;
    iNextSubFeature = 0;
    oIt = poDS->GetBlockMap().begin();
}

/************************************************************************/
/*                      GetNextUnfilteredFeature()                      */
/************************************************************************/

OGRFeature *OGRDWGBlocksLayer::GetNextUnfilteredFeature()

{
    OGRFeature *poFeature = nullptr;

    /* -------------------------------------------------------------------- */
    /*      Are we out of features?                                         */
    /* -------------------------------------------------------------------- */
    if (oIt == poDS->GetBlockMap().end())
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Are we done reading the current blocks features?                */
    /* -------------------------------------------------------------------- */
    DWGBlockDefinition *psBlock = &(oIt->second);
    unsigned int nSubFeatureCount =
        static_cast<unsigned int>(psBlock->apoFeatures.size());

    if (psBlock->poGeometry != nullptr)
        nSubFeatureCount++;

    if (iNextSubFeature >= nSubFeatureCount)
    {
        ++oIt;

        iNextSubFeature = 0;

        if (oIt == poDS->GetBlockMap().end())
            return nullptr;

        psBlock = &(oIt->second);
    }

    /* -------------------------------------------------------------------- */
    /*      Is this a geometry based block?                                 */
    /* -------------------------------------------------------------------- */
    if (psBlock->poGeometry != nullptr &&
        iNextSubFeature == psBlock->apoFeatures.size())
    {
        poFeature = new OGRFeature(poFeatureDefn);
        poFeature->SetGeometry(psBlock->poGeometry);
        iNextSubFeature++;
    }

    /* -------------------------------------------------------------------- */
    /*      Otherwise duplicate the next sub-feature.                       */
    /* -------------------------------------------------------------------- */
    else
    {
        poFeature = new OGRFeature(poFeatureDefn);
        poFeature->SetFrom(psBlock->apoFeatures[iNextSubFeature]);
        iNextSubFeature++;
    }

    /* -------------------------------------------------------------------- */
    /*      Set FID and block name.                                         */
    /* -------------------------------------------------------------------- */
    poFeature->SetFID(iNextFID++);

    poFeature->SetField("BlockName", oIt->first.c_str());

    m_nFeaturesRead++;

    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRDWGBlocksLayer::GetNextFeature()

{
    while (true)
    {
        OGRFeature *poFeature = GetNextUnfilteredFeature();

        if (poFeature == nullptr)
            return nullptr;

        if ((m_poFilterGeom == nullptr ||
             FilterGeometry(poFeature->GetGeometryRef())) &&
            (m_poAttrQuery == nullptr || m_poAttrQuery->Evaluate(poFeature)))
        {
            return poFeature;
        }

        delete poFeature;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDWGBlocksLayer::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                             GetDataset()                             */
/************************************************************************/

GDALDataset *OGRDWGBlocksLayer::GetDataset()
{
    return poDS;
}
