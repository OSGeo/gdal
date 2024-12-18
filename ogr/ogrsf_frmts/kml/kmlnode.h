/******************************************************************************
 *
 * Project:  KML Driver
 * Purpose:  Class for building up the node structure of the kml file.
 * Author:   Jens Oberender, j.obi@troja.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Jens Oberender
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/
#ifndef OGR_KMLNODE_H_INCLUDED
#define OGR_KMLNODE_H_INCLUDED

#ifdef HAVE_EXPAT

#include "kml.h"
#include "kmlutility.h"
// std
#include <iostream>
#include <string>
#include <vector>

std::string Nodetype2String(Nodetype const &type);

class KMLNode
{
    CPL_DISALLOW_COPY_ASSIGN(KMLNode)
  public:
    KMLNode();
    ~KMLNode();

    void print(unsigned int what = 3);
    int classify(KML *poKML, int nRecLevel = 0);
    void eliminateEmpty(KML *poKML);
    bool hasOnlyEmpty() const;

    void setType(Nodetype type);
    Nodetype getType() const;

    void setName(std::string const &name);
    const std::string &getName() const;

    void setLevel(std::size_t level);
    std::size_t getLevel() const;

    void addAttribute(Attribute *poAttr);

    void setParent(KMLNode *poNode);
    KMLNode *getParent() const;

    void addChildren(KMLNode *poNode);
    std::size_t countChildren() const;

    KMLNode *getChild(std::size_t index) const;

    void addContent(std::string const &text);
    void appendContent(std::string const &text);
    std::string getContent(std::size_t index) const;
    void deleteContent(std::size_t index);
    std::size_t numContent() const;

    void setLayerNumber(int nNum);
    int getLayerNumber() const;

    std::string getNameElement() const;
    std::string getDescriptionElement() const;

    std::size_t getNumFeatures();
    Feature *getFeature(std::size_t nNum, int &nLastAsked, int &nLastCount);

    OGRGeometry *getGeometry(Nodetype eType = Unknown);

    bool is25D() const
    {
        return b25D_;
    }

  private:
    typedef std::vector<KMLNode *> kml_nodes_t;
    kml_nodes_t *pvpoChildren_;

    typedef std::vector<std::string> kml_content_t;
    kml_content_t *pvsContent_;

    typedef std::vector<Attribute *> kml_attributes_t;
    kml_attributes_t *pvoAttributes_;

    KMLNode *poParent_;
    std::size_t nLevel_;
    std::string sName_;

    Nodetype eType_;
    bool b25D_;

    int nLayerNumber_;
    int nNumFeatures_;

    void unregisterLayerIfMatchingThisNode(KML *poKML);
};

#endif  // HAVE_EXPAT

#endif /* KMLNODE_H_INCLUDED */
