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

#include "ogr_feature.h"

// std
#include <map>
#include <string>

class KMLVector final : public KML
{
  public:
    // Container - FeatureContainer - Feature
    bool isFeature(std::string const &sIn) const override;
    bool isFeatureContainer(std::string const &sIn) const override;
    bool isContainer(std::string const &sIn) const override;
    bool isLeaf(std::string const &sIn) const override;
    bool isRest(std::string const &sIn) const override;
    void findLayers(KMLNode *poNode, int bKeepEmptyContainers);
    void findSchemas(KMLNode *poNode = nullptr);

    const std::map<std::string, std::vector<std::unique_ptr<OGRFieldDefn>>> &
    GetSchemas() const
    {
        return oMapSchemas_;
    }

  private:
    std::map<std::string, std::vector<std::unique_ptr<OGRFieldDefn>>>
        oMapSchemas_{};
};

#endif  // HAVE_EXPAT

#endif /* OGR_KMLVECTOR_H_INCLUDED */
