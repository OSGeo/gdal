/******************************************************************************
 *
 * Project:  KML Driver
 * Purpose:  Specialization of the kml class, only for vectors in kml files.
 * Author:   Jens Oberender, j.obi@troja.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Jens Oberender
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_port.h"
#include "kmlvector.h"

#include <string>

#include "cpl_conv.h"
#include "cpl_error.h"
// #include "kmlnode.h"
#include "kmlutility.h"

KMLVector::~KMLVector()
{
}

bool KMLVector::isLeaf(std::string const &sIn) const
{
    return sIn.compare("name") == 0 || sIn.compare("coordinates") == 0 ||
           sIn.compare("altitudeMode") == 0 || sIn.compare("description") == 0;
}

// Container - FeatureContainer - Feature

bool KMLVector::isContainer(std::string const &sIn) const
{
    return sIn.compare("Folder") == 0 || sIn.compare("Document") == 0 ||
           sIn.compare("kml") == 0;
}

bool KMLVector::isFeatureContainer(std::string const &sIn) const
{
    return sIn.compare("MultiGeometry") == 0 ||
           sIn.compare("MultiPolygon") == 0        // non conformant
           || sIn.compare("MultiLineString") == 0  // non conformant
           || sIn.compare("MultiPoint") == 0       // non conformant
           || sIn.compare("Placemark") == 0;
}

bool KMLVector::isFeature(std::string const &sIn) const
{
    return sIn.compare("Polygon") == 0 || sIn.compare("LineString") == 0 ||
           sIn.compare("Point") == 0;
}

bool KMLVector::isRest(std::string const &sIn) const
{
    return sIn.compare("outerBoundaryIs") == 0 ||
           sIn.compare("innerBoundaryIs") == 0 ||
           sIn.compare("LinearRing") == 0;
}

void KMLVector::findLayers(KMLNode *poNode, int bKeepEmptyContainers)
{
    bool bEmpty = true;

    // Start with the trunk
    if (nullptr == poNode)
    {
        nNumLayers_ = 0;
        poNode = poTrunk_;
    }

    if (isFeature(poNode->getName()) || isFeatureContainer(poNode->getName()) ||
        (isRest(poNode->getName()) && poNode->getName().compare("kml") != 0))
    {
        return;
    }
    else if (isContainer(poNode->getName()))
    {
        for (int z = 0; z < (int)poNode->countChildren(); z++)
        {
            if (isContainer(poNode->getChild(z)->getName()))
            {
                findLayers(poNode->getChild(z), bKeepEmptyContainers);
            }
            else if (isFeatureContainer(poNode->getChild(z)->getName()))
            {
                bEmpty = false;
            }
        }

        if (bKeepEmptyContainers && poNode->getName() == "Folder")
        {
            if (!bEmpty)
                poNode->eliminateEmpty(this);
        }
        else if (bEmpty)
        {
            return;
        }

        Nodetype nodeType = poNode->getType();
        if (bKeepEmptyContainers || isFeature(Nodetype2String(nodeType)) ||
            nodeType == Mixed || nodeType == MultiGeometry ||
            nodeType == MultiPoint || nodeType == MultiLineString ||
            nodeType == MultiPolygon)
        {
            poNode->setLayerNumber(nNumLayers_++);
            papoLayers_ = static_cast<KMLNode **>(
                CPLRealloc(papoLayers_, nNumLayers_ * sizeof(KMLNode *)));
            papoLayers_[nNumLayers_ - 1] = poNode;
        }
        else
        {
            CPLDebug("KML", "We have a strange type here for node %s: %s",
                     poNode->getName().c_str(),
                     Nodetype2String(poNode->getType()).c_str());
        }
    }
    else
    {
        CPLDebug("KML",
                 "There is something wrong!  Define KML_DEBUG to see details");
        if (CPLGetConfigOption("KML_DEBUG", nullptr) != nullptr)
            print();
    }
}
