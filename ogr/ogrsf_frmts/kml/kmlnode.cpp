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
#include "kmlnode.h"
#include "cpl_conv.h"
// std
#include <string>
#include <vector>

/************************************************************************/
/*                           Help functions                             */
/************************************************************************/

std::string Nodetype2String(Nodetype t)
{
    if(t == Empty)
        return "Empty";
    else if(t == Rest)
        return "Rest";
    else if(t == Mixed)
        return "Mixed";
    else if(t == Point)
        return "Point";
    else if(t == LineString)
        return "LineString";
    else if(t == Polygon)
        return "Polygon";
    else
        return "Unknown";
}

bool isNumberDigit(const char cIn)
{
    return ( cIn == '-' || cIn == '+' || 
            (cIn >= '0' && cIn <= '9') ||
             cIn == '.' || cIn == 'e' || cIn == 'E' );
}

Coordinate* ParseCoordinate(std::string sIn)
{
    unsigned short nPos = 0;
    Coordinate *psTmp = new Coordinate;
    while(isNumberDigit(sIn[nPos++]));
    psTmp->dfLongitude = atof(sIn.substr(0, (nPos - 1)).c_str());
    if(sIn[nPos - 1] != ',')
    {
        delete psTmp;
        return NULL;
    }
    sIn = sIn.substr(nPos, sIn.length() - nPos);
    nPos = 0;
    while(isNumberDigit(sIn[nPos++]));
    psTmp->dfLatitude = atof(sIn.substr(0, (nPos - 1)).c_str());
    if(sIn[nPos - 1] != ',')
    {
        psTmp->dfAltitude = 0;
        return psTmp;
    }
    sIn = sIn.substr(nPos, sIn.length() - nPos);
    nPos = 0;
    while(isNumberDigit(sIn[nPos++]));
    psTmp->dfAltitude = atof(sIn.substr(0, (nPos - 1)).c_str());
    return psTmp;
}

/************************************************************************/
/*                         KMLnode methods                              */
/************************************************************************/

KMLnode::KMLnode()
{
    this->sName = "";
    this->poParent = NULL;
    this->pvpoChildren = new std::vector<KMLnode*>;
    this->pvsContent = new std::vector<std::string>;
    this->pvoAttributes = new std::vector<Attribute*>;
    this->eType = Unknown;
	this->nLayerNumber = -1;
	this->psExtent = NULL;
}

KMLnode::~KMLnode()
{
    kml_nodes_t::size_type nCount = 0;

    for( nCount = 0; nCount < pvpoChildren->size(); ++nCount )
    {
        delete (pvpoChildren->at( nCount ));
    }

    delete(this->pvpoChildren);
    delete(this->pvsContent);

    for( nCount = 0; nCount < pvoAttributes->size(); ++nCount)
    {
        delete pvoAttributes->at(nCount);
    }
    
    delete(pvoAttributes);

    if( psExtent != NULL )
        delete psExtent;
}

void KMLnode::print(unsigned int what)
{
    std::string indent("");

    for(unsigned int z = 0; z < this->nLevel; z++)
        indent += " ";

    if(this->nLevel > 0)
    {
        if(this->nLayerNumber > -1)
            if(this->psExtent != NULL)
                CPLDebug("KML", "%s%s (nLevel: %d Type: %s poParent: %s pvpoChildren: %d pvsContent: %d pvoAttributes: %d) (%f|%f|%f|%f) <--- Layer #%d", 
                    indent.c_str(), this->sName.c_str(), this->nLevel, Nodetype2String(this->eType).c_str(), this->poParent->sName.c_str(), 
                    this->pvpoChildren->size(), this->pvsContent->size(), this->pvoAttributes->size(), 
                    this->psExtent->dfX1, this->psExtent->dfX2, this->psExtent->dfY1, this->psExtent->dfY2, this->nLayerNumber);
            else
                CPLDebug("KML", "%s%s (nLevel: %d Type: %s poParent: %s pvpoChildren: %d pvsContent: %d pvoAttributes: %d) <--- Layer #%d", 
                    indent.c_str(), this->sName.c_str(), this->nLevel, Nodetype2String(this->eType).c_str(), this->poParent->sName.c_str(), 
                    this->pvpoChildren->size(), this->pvsContent->size(), this->pvoAttributes->size(), this->nLayerNumber);
        else
            if(this->psExtent != NULL)
                CPLDebug("KML", "%s%s (nLevel: %d Type: %s poParent: %s pvpoChildren: %d pvsContent: %d pvoAttributes: %d) (%f|%f|%f|%f)", 
                    indent.c_str(), this->sName.c_str(), this->nLevel, Nodetype2String(this->eType).c_str(), this->poParent->sName.c_str(), 
                    this->pvpoChildren->size(), this->pvsContent->size(), this->pvoAttributes->size(),
                    this->psExtent->dfX1, this->psExtent->dfX2, this->psExtent->dfY1, this->psExtent->dfY2);
            else
                CPLDebug("KML", "%s%s (nLevel: %d Type: %s poParent: %s pvpoChildren: %d pvsContent: %d pvoAttributes: %d)", 
                    indent.c_str(), this->sName.c_str(), this->nLevel, Nodetype2String(this->eType).c_str(), this->poParent->sName.c_str(), 
                    this->pvpoChildren->size(), this->pvsContent->size(), this->pvoAttributes->size());
    } else
        CPLDebug("KML", "%s%s (nLevel: %d Type: %s pvpoChildren: %d pvsContent: %d pvoAttributes: %d)", 
            indent.c_str(), this->sName.c_str(), Nodetype2String(this->eType).c_str(), this->nLevel, this->pvpoChildren->size(), 
            this->pvsContent->size(), this->pvoAttributes->size());
    if(what == 1 || what == 3)
        for(unsigned int z = 0; z < this->pvsContent->size(); z++)
            CPLDebug("KML", "%s|->pvsContent: '%s'", indent.c_str(), this->pvsContent->at(z).c_str());
    if(what == 2 || what == 3)
        for(unsigned int z = 0; z < this->pvoAttributes->size(); z++)
            CPLDebug("KML", "%s|->pvoAttributes: %s = '%s'", indent.c_str(), this->pvoAttributes->at(z)->sName.c_str(), this->pvoAttributes->at(z)->sValue.c_str());
    for(unsigned int z = 0; z < this->pvpoChildren->size(); z++)
        this->pvpoChildren->at(z)->print(what);
}

void KMLnode::classify(KML *kmlclass)
{
    Nodetype curr = Unknown, all = Empty;
    
    CPLDebug("KML", "Start -- sName: %s\tnLevel: %d\t", this->sName.c_str(), this->nLevel);

    for(unsigned int z = 0; z < this->pvpoChildren->size(); z++) {
        // Leafs are ignored
        if(kmlclass->isLeaf(this->pvpoChildren->at(z)->sName))
            continue;
        // Classify pvpoChildren
        this->pvpoChildren->at(z)->classify(kmlclass);

        if(kmlclass->isContainer(this->sName))
            curr = this->pvpoChildren->at(z)->eType;
        else if(kmlclass->isFeatureContainer(this->sName)) {
            if(kmlclass->isFeature(this->pvpoChildren->at(z)->sName)) {
                if(this->pvpoChildren->at(z)->sName.compare("Point") == 0)
                    curr = Point;
                else if(this->pvpoChildren->at(z)->sName.compare("LineString") == 0)
                    curr = LineString;
                else if(this->pvpoChildren->at(z)->sName.compare("Polygon") == 0)
                    curr = Polygon;
            } else if(kmlclass->isContainer(this->sName))
                curr = this->pvpoChildren->at(z)->eType;
        } else if(kmlclass->isFeature(this->sName) || kmlclass->isRest(this->sName))
            curr = Empty;
            
        // Compare and return if it is mixed
        if(curr != all && all != Empty && curr != Empty) {
            this->eType = Mixed;
            CPLDebug("KML", "Mixed --> sName: %s\tClassify sName: %s\tnLevel: %d\tpoParent: %s (%s/%s)", this->sName.c_str(), Nodetype2String(curr).c_str(), this->nLevel, this->poParent->sName.c_str(), Nodetype2String(curr).c_str(), Nodetype2String(all).c_str());
            if((kmlclass->isFeature(Nodetype2String(curr)) && kmlclass->isFeatureContainer(Nodetype2String(all))) || 
            (kmlclass->isFeature(Nodetype2String(all)) && kmlclass->isFeatureContainer(Nodetype2String(curr))))
                CPLDebug("KML", "FeatureContainer and Feature");
            continue;
        } else if(curr != Empty)
            all = curr;
        if(this->poParent != NULL)
            CPLDebug("KML", "sName: %s\tClassify sName: %s\tnLevel: %d\tpoParent: %s (%s/%s)", this->sName.c_str(), Nodetype2String(curr).c_str(), this->nLevel, this->poParent->sName.c_str(), Nodetype2String(curr).c_str(), Nodetype2String(all).c_str());
    }
    if(this->eType == Unknown)
        this->eType = all;
}

void KMLnode::eliminateEmpty(KML *kmlclass)
{
    for(unsigned int z = 0; z < this->pvpoChildren->size(); z++) {
        if(this->pvpoChildren->at(z)->eType == Empty && 
                (kmlclass->isContainer(this->pvpoChildren->at(z)->sName) || 
                kmlclass->isFeatureContainer(this->pvpoChildren->at(z)->sName))) {
            CPLDebug("KML", "Deleting sName: %s\tClassify sName: %s\tnLevel: %d\tpoParent: %s", this->pvpoChildren->at(z)->sName.c_str(), Nodetype2String(this->pvpoChildren->at(z)->eType).c_str(), this->pvpoChildren->at(z)->nLevel, this->sName.c_str());
            delete this->pvpoChildren->at(z);
            this->pvpoChildren->erase(this->pvpoChildren->begin() + z);
            z--;
        } else
            this->pvpoChildren->at(z)->eliminateEmpty(kmlclass);
    }
    this->calcExtent(kmlclass);
}

void KMLnode::setType(Nodetype oNotet)
{
    this->eType = oNotet;
}

Nodetype KMLnode::getType()
{
    return this->eType;
}

void KMLnode::setName(std::string const& sIn)
{
    this->sName = sIn;
}

std::string KMLnode::getName()
{
    return this->sName;
}

void KMLnode::setLevel(unsigned int nLev)
{
    this->nLevel = nLev;
}

unsigned int KMLnode::getLevel()
{
    return this->nLevel;
}

void KMLnode::addAttribute(Attribute *poAttr)
{
    this->pvoAttributes->push_back(poAttr);
}

void KMLnode::setParent(KMLnode* poPar)
{
    this->poParent = poPar;
}

KMLnode* KMLnode::getParent()
{
    return this->poParent;
}

void KMLnode::addChildren(KMLnode *poChil)
{
    this->pvpoChildren->push_back(poChil);
}

unsigned short KMLnode::countChildren()
{
    return this->pvpoChildren->size();
}

KMLnode* KMLnode::getChild(unsigned short nNum)
{
    return this->pvpoChildren->at(nNum);
}

void KMLnode::addContent(std::string const& sCon)
{
    this->pvsContent->push_back(sCon);
}

void KMLnode::appendContent(std::string sCon)
{
    this->pvsContent->at(this->pvsContent->size()-1) += sCon;
}

std::string KMLnode::getContent(unsigned short nNum)
{
    if(nNum >= this->pvsContent->size())
        return NULL;
    return this->pvsContent->at(nNum);
}

void KMLnode::deleteContent(unsigned short nNum)
{
    if(nNum >= this->pvsContent->size())
        return;
    this->pvsContent->erase(this->pvsContent->begin() + nNum);
}

unsigned short KMLnode::numContent()
{
    return this->pvsContent->size();
}

void KMLnode::setLayerNumber(short nNum)
{
    this->nLayerNumber = nNum;
}

short KMLnode::getLayerNumber()
{
    return this->nLayerNumber;
}

KMLnode* KMLnode::getLayer(unsigned short nNum)
{
    KMLnode *poTmp;
    if(this->nLayerNumber == nNum)
        return this;

    for(unsigned short nCount = 0; nCount < this->pvpoChildren->size(); nCount++)
    {
        if((poTmp = this->pvpoChildren->at(nCount)->getLayer(nNum)) != NULL)
            return poTmp;
    }

    return NULL;
}

std::string KMLnode::getNameElement()
{
    std::string sContent;

    for(unsigned short nCount = 0; nCount < this->pvpoChildren->size(); nCount++)
    {
        if(this->pvpoChildren->at(nCount)->sName.compare("name") == 0)
        {
            unsigned int nSize = this->pvpoChildren->at(nCount)->pvsContent->size();
            if (nSize > 0)
            {
                sContent = this->pvpoChildren->at(nCount)->pvsContent->at(0);
                for(unsigned int nCount2 = 1; nCount2 < nSize; nCount2++)
                {
                    sContent += " " + this->pvpoChildren->at(nCount)->pvsContent->at(nCount2);
                }
                return sContent;
            }
            break;
        }
    }

    return "";
}

std::string KMLnode::getDescriptionElement()
{
    std::string sContent;
    for(unsigned short nCount = 0; nCount < this->pvpoChildren->size(); nCount++)
    {
        if(this->pvpoChildren->at(nCount)->sName.compare("description") == 0)
        {
            unsigned int nSize = this->pvpoChildren->at(nCount)->pvsContent->size();
            if (nSize > 0)
            {
                sContent = this->pvpoChildren->at(nCount)->pvsContent->at(0);
                for(unsigned int nCount2 = 1; nCount2 < nSize; nCount2++)
                {
                    sContent += " " + this->pvpoChildren->at(nCount)->pvsContent->at(nCount2);
                }
                return sContent;
            }
            break;
        }
    }
    return "";
}

short KMLnode::getNumFeatures()
{
    short nNum = 0;
    for(unsigned short z = 0; z < this->pvpoChildren->size();z++)
    {
        if(this->pvpoChildren->at(z)->sName.compare("Placemark") == 0)
            nNum++;
    }
    return nNum;
}

Feature* KMLnode::getFeature(unsigned short nNum)
{
    CPLDebug("KML", "GetFeature(#%d)", nNum);

    unsigned short nCount, nCount2, nCountP = 0;
    KMLnode *poFeat = NULL, *poTemp = NULL, *poCoor = NULL;
    std::string sContent;
    Coordinate *psCoord;
    std::vector<Coordinate*> *pvpsTmp;

    if(nNum >= this->getNumFeatures())
        return NULL;
    for(nCount = 0; nCount < this->pvpoChildren->size(); nCount++)
    {
        if(this->pvpoChildren->at(nCount)->sName.compare("Placemark") == 0)
        {
            CPLDebug("KML", "GetFeature(#%d) - %s", nNum, this->pvpoChildren->at(nCount)->sName.c_str());
            if(nCountP == nNum)
            {
                poFeat = this->pvpoChildren->at(nCount);
                break;
            }
            nCountP++;
        }
    }
    if(poFeat == NULL)
        return NULL;
        
    // Create a feature structure
    Feature *psReturn = new Feature;
    psReturn->pvpsCoordinatesExtra = NULL;
    // Build up the name
    psReturn->sName = poFeat->getNameElement();
    // Build up the description
    psReturn->sDescription = poFeat->getDescriptionElement();
    // the type
    psReturn->eType = poFeat->eType;
    CPLDebug("KML", "GetFeature(#%d) --> %s", nNum, Nodetype2String(poFeat->eType).c_str());
    // the coordinates (for a Point)
    if(poFeat->eType == Point)
    {
        psReturn->pvpsCoordinates = new std::vector<Coordinate*>;
        // Search Point Element
        for(nCount = 0; nCount < poFeat->pvpoChildren->size(); nCount++)
        {
            if(poFeat->pvpoChildren->at(nCount)->sName.compare("Point") == 0)
            {
                poTemp = poFeat->pvpoChildren->at(nCount);
                break;
            }
        }
        // poTemp must be a Point
        if(poTemp->sName.compare("Point") != 0)
        {
            delete psReturn->pvpsCoordinates;
            delete psReturn;
            return NULL;
        }

        // Search coordinate Element
        for(nCount = 0; nCount < poTemp->pvpoChildren->size(); nCount++)
        {
            CPLDebug("KML", "GetFeature(#%d) ---> %s", nNum, poTemp->pvpoChildren->at(nCount)->sName.c_str());
            if(poTemp->pvpoChildren->at(nCount)->sName.compare("coordinates") == 0)
            {
                poCoor = poTemp->pvpoChildren->at(nCount);
                CPLDebug("KML", "GetFeature(#%d) ---> #%d", nNum, poCoor->pvsContent->size());
                for(nCountP = 0; nCountP < poCoor->pvsContent->size(); nCountP++)
                {
                    CPLDebug("KML", "GetFeature(#%d) ----> %s", nNum, poCoor->pvsContent->at(nCountP).c_str());
                    psCoord = ParseCoordinate(poCoor->pvsContent->at(nCountP));
                    if(psCoord != NULL)
                        psReturn->pvpsCoordinates->push_back(psCoord);
                }
            }
        }
        if(psReturn->pvpsCoordinates->size() == 1)
            return psReturn;
        else
        {
            for(unsigned short nNum = 0; nNum < psReturn->pvpsCoordinates->size(); nNum++)
                delete psReturn->pvpsCoordinates->at(nNum);
            delete psReturn->pvpsCoordinates;
            delete psReturn;
            return NULL;
        }
    }
    // the coordinates (for a LineString)
    else if(poFeat->eType == LineString)
    {
        psReturn->pvpsCoordinates = new std::vector<Coordinate*>;
        // Search LineString Element
        for(nCount = 0; nCount < poFeat->pvpoChildren->size(); nCount++)
        {
            if(poFeat->pvpoChildren->at(nCount)->sName.compare("LineString") == 0)
            {
                poTemp = poFeat->pvpoChildren->at(nCount);
                break;
            }
        }
        // poTemp must be a LineString
        if(poTemp->sName.compare("LineString") != 0)
        {
            delete psReturn->pvpsCoordinates;
            delete psReturn;
            return NULL;
        }

        // Search coordinate Element
        for(nCount = 0; nCount < poTemp->pvpoChildren->size(); nCount++)
        {
            CPLDebug("KML", "GetFeature(#%d) ---> %s", nNum, poTemp->pvpoChildren->at(nCount)->sName.c_str());
            if(poTemp->pvpoChildren->at(nCount)->sName.compare("coordinates") == 0)
            {
                poCoor = poTemp->pvpoChildren->at(nCount);
                CPLDebug("KML", "GetFeature(#%d) ---> #%d", nNum, poCoor->pvsContent->size());
                for(nCountP = 0; nCountP < poCoor->pvsContent->size(); nCountP++)
                {
                    CPLDebug("KML", "GetFeature(#%d) ----> %s", nNum, poCoor->pvsContent->at(nCountP).c_str());
                    psCoord = ParseCoordinate(poCoor->pvsContent->at(nCountP));
                    if(psCoord != NULL)
                        psReturn->pvpsCoordinates->push_back(psCoord);
                }
            }
        }
        if(psReturn->pvpsCoordinates->size() > 0)
            return psReturn;
        else
        {
            delete psReturn->pvpsCoordinates;
            delete psReturn;
            return NULL;
        }    
    }
    // the coordinates (for a Polygon)
    else if(poFeat->eType == Polygon)
    {
        psReturn->pvpsCoordinates = new std::vector<Coordinate*>;
        // Search Polygon Element
        for(nCount = 0; nCount < poFeat->pvpoChildren->size(); nCount++)
        {
            if(poFeat->pvpoChildren->at(nCount)->sName.compare("Polygon") == 0)
            {
                poTemp = poFeat->pvpoChildren->at(nCount);
                break;
            }
        }
        // poTemp must be a Polygon
        if(poTemp->sName.compare("Polygon") != 0)
        {
            delete psReturn->pvpsCoordinates;
            delete psReturn;
            return NULL;
        }

        //*********************************
        // Search outerBoundaryIs Element
        //*********************************
        for(nCount = 0; nCount < poTemp->pvpoChildren->size(); nCount++)
        {
            CPLDebug("KML", "GetFeature(#%d) ---> %s", nNum, poTemp->pvpoChildren->at(nCount)->sName.c_str());
            if(poTemp->pvpoChildren->at(nCount)->sName.compare("outerBoundaryIs") == 0)
            {
                poCoor = poTemp->pvpoChildren->at(nCount)->pvpoChildren->at(0);
            }
        }
        // No outer boundary found
        if(poCoor == NULL)
        {
            delete psReturn->pvpsCoordinates;
            delete psReturn;
            return NULL;
        }
        // Search coordinate Element
        CPLDebug("KML", "GetFeature(#%d) ---> #%d", nNum, poCoor->pvsContent->size());
        for(nCount = 0; nCount < poCoor->pvpoChildren->size(); nCount++)
        {
            if(poCoor->pvpoChildren->at(nCount)->sName.compare("coordinates") == 0)
            {
                for(nCountP = 0; nCountP < poCoor->pvpoChildren->at(nCount)->pvsContent->size(); nCountP++)
                {
                    CPLDebug("KML", "GetFeature(#%d) ----> %s", nNum, poCoor->pvpoChildren->at(nCount)->pvsContent->at(nCountP).c_str());
                    psCoord = ParseCoordinate(poCoor->pvpoChildren->at(nCount)->pvsContent->at(nCountP));
                    if(psCoord != NULL)
                        psReturn->pvpsCoordinates->push_back(psCoord);
                }
            }
        }
        // No outer boundary coordinates found
        if(psReturn->pvpsCoordinates->size() == 0)
        {
            delete psReturn->pvpsCoordinates;
            delete psReturn;
            return NULL;
        }
        //*********************************
        // Search innerBoundaryIs Elements
        //*********************************
        psReturn->pvpsCoordinatesExtra = new std::vector< std::vector<Coordinate*>* >;

        for(nCount2 = 0; nCount2 < poTemp->pvpoChildren->size(); nCount2++)
        {
            CPLDebug("KML", "GetFeature(#%d) ---> %s", nNum, poTemp->pvpoChildren->at(nCount2)->sName.c_str());
            if(poTemp->pvpoChildren->at(nCount2)->sName.compare("innerBoundaryIs") == 0)
            {
                pvpsTmp = new std::vector<Coordinate*>; 
                poCoor = poTemp->pvpoChildren->at(nCount2)->pvpoChildren->at(0);
                // Search coordinate Element
                CPLDebug("KML", "GetFeature(#%d) ----> #%d", nNum, poCoor->pvsContent->size());
                for(nCount = 0; nCount < poCoor->pvpoChildren->size(); nCount++)
                {
                    if(poCoor->pvpoChildren->at(nCount)->sName.compare("coordinates") == 0)
                    {
                        for(nCountP = 0; nCountP < poCoor->pvpoChildren->at(nCount)->pvsContent->size(); nCountP++)
                        {
                            CPLDebug("KML", "GetFeature(#%d) -----> %s", nNum, poCoor->pvpoChildren->at(nCount)->pvsContent->at(nCountP).c_str());
                            psCoord = ParseCoordinate(poCoor->pvpoChildren->at(nCount)->pvsContent->at(nCountP));
                            if(psCoord != NULL)
                                pvpsTmp->push_back(psCoord);
                        }
                    }
                }
                // Outer boundary coordinates found?
                if(psReturn->pvpsCoordinates->size() > 0)
                    psReturn->pvpsCoordinatesExtra->push_back(pvpsTmp);
                else
                    delete pvpsTmp;
            }
        }
        // No inner boundaries
        if(psReturn->pvpsCoordinates->size() == 0)
            delete psReturn->pvpsCoordinates;
        // everything OK
        return psReturn;
    }

    // Nothing found
    delete psReturn;
    return NULL;
}

void KMLnode::calcExtent(KML *poKMLClass)
{
    KMLnode *poTmp;
    Coordinate *psCoors;
    
    if(this->psExtent != NULL)
        return;
    // Handle Features
    if(poKMLClass->isFeature(this->sName))
    {
        this->psExtent = new Extent;
        this->psExtent->dfX1 = this->psExtent->dfX2 = this->psExtent->dfY1 = this->psExtent->dfY2 = 0.0;
        // Special for Polygons
        if(this->sName.compare("Polygon") == 0)
        {
            for(unsigned short nCount = 0; nCount < this->pvpoChildren->size(); nCount++)
            {
                if(this->pvpoChildren->at(nCount)->sName.compare("outerBoundaryIs") == 0
                   || this->pvpoChildren->at(nCount)->sName.compare("innerBoundaryIs") == 0)
                {
                    if(this->pvpoChildren->at(nCount)->pvpoChildren->size() == 1)
                    {
                        poTmp = this->pvpoChildren->at(nCount)->pvpoChildren->at(0);
                        for(unsigned short nCount3 = 0; nCount3 < poTmp->pvpoChildren->size(); nCount3++)
                        {
                            if(poTmp->pvpoChildren->at(nCount3)->sName.compare("coordinates") == 0)
                            {
                                for(unsigned short nCount2 = 0;
                                    nCount2 < poTmp->pvpoChildren->at(nCount3)->pvsContent->size(); nCount2++)
                                {
                                    psCoors = ParseCoordinate(poTmp->pvpoChildren->at(nCount3)->pvsContent->at(nCount2));
                                    if(psCoors != NULL)
                                    {
                                        if(psCoors->dfLongitude < this->psExtent->dfX1 || this->psExtent->dfX1 == 0)
                                            this->psExtent->dfX1 = psCoors->dfLongitude;
                                        if(psCoors->dfLongitude > this->psExtent->dfX2 || this->psExtent->dfX2 == 0)
                                            this->psExtent->dfX2 = psCoors->dfLongitude;
                                        if(psCoors->dfLatitude < this->psExtent->dfY1 || this->psExtent->dfY1 == 0)
                                            this->psExtent->dfY1 = psCoors->dfLatitude;
                                        if(psCoors->dfLatitude > this->psExtent->dfY2 || this->psExtent->dfY2 == 0)
                                            this->psExtent->dfY2 = psCoors->dfLatitude;
                                        delete psCoors;
                                    }
                                }
                            }
                        }
                    }
                }
            }
        // General for LineStrings and Points
        }
        else
        {
            for(unsigned short nCount = 0; nCount < this->pvpoChildren->size(); nCount++)
            {
                if(this->pvpoChildren->at(nCount)->sName.compare("coordinates") == 0)
                {
                    poTmp = this->pvpoChildren->at(nCount);
                    for(unsigned short nCount2 = 0; nCount2 < poTmp->pvsContent->size(); nCount2++)
                    {
                        psCoors = ParseCoordinate(poTmp->pvsContent->at(nCount2));
                        if(psCoors != NULL)
                        {
                            if(psCoors->dfLongitude < this->psExtent->dfX1 || this->psExtent->dfX1 == 0.0)
                                this->psExtent->dfX1 = psCoors->dfLongitude;
                            if(psCoors->dfLongitude > this->psExtent->dfX2 || this->psExtent->dfX2 == 0.0)
                                this->psExtent->dfX2 = psCoors->dfLongitude;
                            if(psCoors->dfLatitude < this->psExtent->dfY1 || this->psExtent->dfY1 == 0.0)
                                this->psExtent->dfY1 = psCoors->dfLatitude;
                            if(psCoors->dfLatitude > this->psExtent->dfY2 || this->psExtent->dfY2 == 0.0)
                                this->psExtent->dfY2 = psCoors->dfLatitude;
                            delete psCoors;
                        }
                    }
                }
            }
        }
    // Summarize Containers
    }
    else if( poKMLClass->isFeatureContainer(this->sName)
             || poKMLClass->isContainer(this->sName))
    {
        this->psExtent = new Extent;
        this->psExtent->dfX1 = this->psExtent->dfX2 = this->psExtent->dfY1 = this->psExtent->dfY2 = 0.0;
        for(unsigned short nCount = 0; nCount < this->pvpoChildren->size(); nCount++)
        {
            this->pvpoChildren->at(nCount)->calcExtent(poKMLClass);
            if(this->pvpoChildren->at(nCount)->psExtent != NULL)
            {
                if(this->pvpoChildren->at(nCount)->psExtent->dfX1 < this->psExtent->dfX1 || 
                        this->psExtent->dfX1 == 0)
                    this->psExtent->dfX1 = this->pvpoChildren->at(nCount)->psExtent->dfX1;
                if(this->pvpoChildren->at(nCount)->psExtent->dfX2 > this->psExtent->dfX2 || 
                        this->psExtent->dfX2 == 0)
                    this->psExtent->dfX2 = this->pvpoChildren->at(nCount)->psExtent->dfX2;
                if(this->pvpoChildren->at(nCount)->psExtent->dfY1 < this->psExtent->dfY1 || 
                        this->psExtent->dfY1 == 0)
                    this->psExtent->dfY1 = this->pvpoChildren->at(nCount)->psExtent->dfY1;
                if(this->pvpoChildren->at(nCount)->psExtent->dfY2 > this->psExtent->dfY2 || 
                        this->psExtent->dfY2 == 0)
                    this->psExtent->dfY2 = this->pvpoChildren->at(nCount)->psExtent->dfY2;
            }
        }
    }
}

Extent* KMLnode::getExtents()
{
    return this->psExtent;
}

