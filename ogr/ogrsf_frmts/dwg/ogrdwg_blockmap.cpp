/******************************************************************************
 *
 * Project:  DWG Translator
 * Purpose:  Implements BlockMap reading and management portion of
 *           OGRDWGDataSource class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_dwg.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"

/************************************************************************/
/*                          ReadBlockSection()                          */
/************************************************************************/

void OGRDWGDataSource::ReadBlocksSection()

{
    OGRDWGLayer *poReaderLayer = (OGRDWGLayer *)GetLayerByName("Entities");
    int bMergeBlockGeometries =
        CPLTestBool(CPLGetConfigOption("DWG_MERGE_BLOCK_GEOMETRIES", "TRUE"));

    /* -------------------------------------------------------------------- */
    /*      Loop over all the block tables, skipping *Model_Space which     */
    /*      we assume is primary entities.                                  */
    /* -------------------------------------------------------------------- */
    OdDbBlockTableRecordPtr poModelSpace, poBlock;
    OdDbBlockTablePtr pTable = GetDB()->getBlockTableId().safeOpenObject();
    OdDbSymbolTableIteratorPtr pBlkIter = pTable->newIterator();

    for (pBlkIter->start(); !pBlkIter->done(); pBlkIter->step())
    {
        poBlock = pBlkIter->getRecordId().safeOpenObject();
        CPLString osBlockName = (const char *)poBlock->getName();

        if (EQUAL(osBlockName, "*Model_Space"))
        {
            poModelSpace = poBlock;
            continue;
        }

        poReaderLayer->SetBlockTable(poBlock);

        // Now we will process entities till we run out.
        // We aggregate the geometries of the features into a multi-geometry,
        // but throw away other stuff attached to the features.

        OGRFeature *poFeature = nullptr;
        OGRGeometryCollection *poColl = new OGRGeometryCollection();
        std::vector<OGRFeature *> apoFeatures;

        while ((poFeature = poReaderLayer->GetNextUnfilteredFeature()) !=
               nullptr)
        {
            if ((poFeature->GetStyleString() != nullptr &&
                 strstr(poFeature->GetStyleString(), "LABEL") != nullptr) ||
                !bMergeBlockGeometries)
            {
                apoFeatures.push_back(poFeature);
            }
            else
            {
                poColl->addGeometryDirectly(poFeature->StealGeometry());
                delete poFeature;
            }
        }

        if (poColl->getNumGeometries() == 0)
            delete poColl;
        else
            oBlockMap[osBlockName].poGeometry = SimplifyBlockGeometry(poColl);

        if (!apoFeatures.empty())
            oBlockMap[osBlockName].apoFeatures = apoFeatures;
    }

    CPLDebug("DWG", "Read %d blocks with meaningful geometry.",
             (int)oBlockMap.size());

    poReaderLayer->SetBlockTable(poModelSpace);
}

/************************************************************************/
/*                       SimplifyBlockGeometry()                        */
/************************************************************************/

OGRGeometry *
OGRDWGDataSource::SimplifyBlockGeometry(OGRGeometryCollection *poCollection)

{
    /* -------------------------------------------------------------------- */
    /*      If there is only one geometry in the collection, just return    */
    /*      it.                                                             */
    /* -------------------------------------------------------------------- */
    if (poCollection->getNumGeometries() == 1)
    {
        OGRGeometry *poReturn = poCollection->getGeometryRef(0);
        poCollection->removeGeometry(0, FALSE);
        delete poCollection;
        return poReturn;
    }

    /* -------------------------------------------------------------------- */
    /*      Eventually we likely ought to have logic to convert to          */
    /*      polygon, multipolygon, multilinestring or multipoint but        */
    /*      I'll put that off till it would be meaningful.                  */
    /* -------------------------------------------------------------------- */

    return poCollection;
}

/************************************************************************/
/*                            LookupBlock()                             */
/*                                                                      */
/*      Find the geometry collection corresponding to a name if it      */
/*      exists.  Note that the returned geometry pointer is to a        */
/*      geometry that continues to be owned by the datasource.  It      */
/*      should be cloned for use.                                       */
/************************************************************************/

DWGBlockDefinition *OGRDWGDataSource::LookupBlock(const char *pszName)

{
    CPLString osName = pszName;

    if (oBlockMap.count(osName) == 0)
        return nullptr;
    else
        return &(oBlockMap[osName]);
}

/************************************************************************/
/*                        ~DWGBlockDefinition()                         */
/*                                                                      */
/*      Safe cleanup of a block definition.                             */
/************************************************************************/

DWGBlockDefinition::~DWGBlockDefinition()

{
    delete poGeometry;

    while (!apoFeatures.empty())
    {
        delete apoFeatures.back();
        apoFeatures.pop_back();
    }
}
