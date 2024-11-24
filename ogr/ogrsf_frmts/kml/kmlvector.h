/******************************************************************************
 *
 * Project:  KML Driver
 * Purpose:  Specialization of the kml class, only for vectors in kml files.
 * Author:   Jens Oberender, j.obi@troja.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Jens Oberender
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef OGR_KMLVECTOR_H_INCLUDED
#define OGR_KMLVECTOR_H_INCLUDED

#ifdef HAVE_EXPAT

#include "kml.h"
#include "kmlnode.h"
// std
#include <string>

class KMLVector : public KML
{
  public:
    ~KMLVector();

    // Container - FeatureContainer - Feature
    bool isFeature(std::string const &sIn) const override;
    bool isFeatureContainer(std::string const &sIn) const override;
    bool isContainer(std::string const &sIn) const override;
    bool isLeaf(std::string const &sIn) const override;
    bool isRest(std::string const &sIn) const override;
    void findLayers(KMLNode *poNode, int bKeepEmptyContainers) override;
};

#endif  // HAVE_EXPAT

#endif /* OGR_KMLVECTOR_H_INCLUDED */
