/******************************************************************************
 * $Id$
 *
 * Project:  KML Driver
 * Purpose:  Class for building up the node structure of the kml file.
 * Author:   Jens Oberender, j.obi@troja.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Jens Oberender
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
#ifndef OGR_KMLNODE_H_INCLUDED
#define OGR_KMLNODE_H_INCLUDED

#include "kml.h"
#include "kmlutility.h"
// std
#include <iostream>
#include <string>
#include <vector>

std::string Nodetype2String(Nodetype const& type);

class KMLNode
{
    CPL_DISALLOW_COPY_ASSIGN( KMLNode )
public:

    KMLNode();
    ~KMLNode();

    void print(unsigned int what = 3);
    int classify(KML* poKML, int nRecLevel = 0);
    void eliminateEmpty(KML* poKML);
    bool hasOnlyEmpty() const;

    void setType(Nodetype type);
    Nodetype getType() const;

    void setName(std::string const& name);
    const std::string& getName() const;

    void setLevel(std::size_t level);
    std::size_t getLevel() const;

    void addAttribute(Attribute* poAttr);

    void setParent(KMLNode* poNode);
    KMLNode* getParent() const;

    void addChildren(KMLNode* poNode);
    std::size_t countChildren() const;

    KMLNode* getChild(std::size_t index) const;

    void addContent(std::string const& text);
    void appendContent(std::string const& text);
    std::string getContent(std::size_t index) const;
    void deleteContent(std::size_t index);
    std::size_t numContent() const;

    void setLayerNumber(int nNum);
    int getLayerNumber() const;

    std::string getNameElement() const;
    std::string getDescriptionElement() const;

    std::size_t getNumFeatures();
    Feature* getFeature(std::size_t nNum, int& nLastAsked, int &nLastCount);

    OGRGeometry* getGeometry(Nodetype eType = Unknown);

    bool is25D() const { return b25D_; }

private:

    typedef std::vector<KMLNode*> kml_nodes_t;
    kml_nodes_t* pvpoChildren_;

    typedef std::vector<std::string> kml_content_t;
    kml_content_t* pvsContent_;

    typedef std::vector<Attribute*> kml_attributes_t;
    kml_attributes_t* pvoAttributes_;

    KMLNode *poParent_;
    std::size_t nLevel_;
    std::string sName_;

    Nodetype eType_;
    bool b25D_;

    int nLayerNumber_;
    int nNumFeatures_;

    void unregisterLayerIfMatchingThisNode(KML* poKML);
};

#endif /* KMLNODE_H_INCLUDED */
