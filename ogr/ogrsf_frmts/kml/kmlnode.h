/******************************************************************************
 * $Id$
 *
 * Project:  KML Driver
 * Purpose:  Class for building up the node structure of the kml file.
 * Author:   Jens Oberender, j.obi@troja.net
 *
 ******************************************************************************
 * Copyright (c) 2007, Jens Oberender
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

std::string Nodetype2String(Nodetype);
bool isNumberDigit(const char cIn);

class KMLnode
{
private:

    typedef std::vector<KMLnode*> kml_nodes_t;
    kml_nodes_t* pvpoChildren;

    typedef std::vector<std::string> kml_content_t;
    kml_content_t* pvsContent;

    typedef std::vector<Attribute*> kml_attributes_t;
    kml_attributes_t* pvoAttributes;

    KMLnode *poParent;
    unsigned int nLevel;
    std::string sName;

    Nodetype eType;
	// Layer number
	short nLayerNumber;
	// Extent
	Extent *psExtent;
	void calcExtent(KML*);
    
public:
    KMLnode();
    ~KMLnode();
    void print(unsigned int what = 3);
    void classify(KML*);
    void eliminateEmpty(KML*);
    void setType(Nodetype);
    Nodetype getType();
    void setName(std::string const&);
    std::string getName();
    void setLevel(unsigned int);
    unsigned int getLevel();
    void addAttribute(Attribute*);
    void setParent(KMLnode*);
    KMLnode* getParent();
    void addChildren(KMLnode*);
    unsigned short countChildren();
    KMLnode* getChild(unsigned short);
    void addContent(std::string const&);
    void appendContent(std::string);
    std::string getContent(unsigned short);
    void deleteContent(unsigned short);
    unsigned short numContent();
    void setLayerNumber(short);
    short getLayerNumber();
    KMLnode* getLayer(unsigned short);
    std::string getNameElement();
    std::string getDescriptionElement();
    short getNumFeatures();
    Feature* getFeature(unsigned short);
    Extent* getExtents();
};

#endif /* KMLNODE_H_INCLUDED */

