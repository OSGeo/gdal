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

std::string Nodetype2String(Nodetype const& type)
{
    if(type == Empty)
        return "Empty";
    else if(type == Rest)
        return "Rest";
    else if(type == Mixed)
        return "Mixed";
    else if(type == Point)
        return "Point";
    else if(type == LineString)
        return "LineString";
    else if(type == Polygon)
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

Coordinate* ParseCoordinate(std::string const& text)
{
    std::string::size_type pos = 0;
    Coordinate *psTmp = new Coordinate();

    // X coordinate
    while(isNumberDigit(text[pos++]));
    psTmp->dfLongitude = atof(text.substr(0, (pos - 1)).c_str());

    // Y coordinate
    if(text[pos - 1] != ',')
    {
        delete psTmp;
        return NULL;
    }
    std::string tmp(text.substr(pos, text.length() - pos));
    pos = 0;
    while(isNumberDigit(tmp[pos++]));
    psTmp->dfLatitude = atof(tmp.substr(0, (pos - 1)).c_str());
    
    // Z coordinate
    if(tmp[pos - 1] != ',')
    {
        psTmp->dfAltitude = 0;
        return psTmp;
    }
    tmp = tmp.substr(pos, tmp.length() - pos);
    pos = 0;
    while(isNumberDigit(tmp[pos++]));
    psTmp->dfAltitude = atof(tmp.substr(0, (pos - 1)).c_str());

    return psTmp;
}

/************************************************************************/
/*                         KMLNode methods                              */
/************************************************************************/

KMLNode::KMLNode()
{
    poParent_ = NULL;
    pvpoChildren_ = new std::vector<KMLNode*>;
    pvsContent_ = new std::vector<std::string>;
    pvoAttributes_ = new std::vector<Attribute*>;
    eType_ = Unknown;
	nLayerNumber_ = -1;
	psExtent_ = NULL;
}

KMLNode::~KMLNode()
{
    CPLAssert( NULL != pvpoChildren_ );
    CPLAssert( NULL != pvoAttributes_ );

    for( kml_nodes_t::iterator it = pvpoChildren_->begin();
         it != pvpoChildren_->end(); ++it)
    {
        delete (*it);
    }
    delete pvpoChildren_;

    for( kml_attributes_t::iterator it = pvoAttributes_->begin();
         it != pvoAttributes_->end(); ++it)
    {
        delete (*it);
    }
    delete pvoAttributes_;

    delete pvsContent_;
    delete psExtent_;
}

void KMLNode::print(unsigned int what)
{
    std::string indent;
    for(std::size_t z = 0; z < nLevel_; z++)
        indent += " ";

    if(nLevel_ > 0)
    {
        if(nLayerNumber_ > -1)
        {
            if(psExtent_ != NULL)
                CPLDebug("KML", "%s%s (nLevel: %d Type: %s poParent: %s pvpoChildren_: %d pvsContent_: %d pvoAttributes_: %d) (%f|%f|%f|%f) <--- Layer #%d", 
                    indent.c_str(), sName_.c_str(), nLevel_, Nodetype2String(eType_).c_str(), poParent_->sName_.c_str(), 
                    pvpoChildren_->size(), pvsContent_->size(), pvoAttributes_->size(), 
                    psExtent_->dfX1, psExtent_->dfX2, psExtent_->dfY1, psExtent_->dfY2, nLayerNumber_);
            else
                CPLDebug("KML", "%s%s (nLevel: %d Type: %s poParent: %s pvpoChildren_: %d pvsContent_: %d pvoAttributes_: %d) <--- Layer #%d", 
                    indent.c_str(), sName_.c_str(), nLevel_, Nodetype2String(eType_).c_str(), poParent_->sName_.c_str(), 
                    pvpoChildren_->size(), pvsContent_->size(), pvoAttributes_->size(), nLayerNumber_);
        }
        else
        {
            if(psExtent_ != NULL)
                CPLDebug("KML", "%s%s (nLevel: %d Type: %s poParent: %s pvpoChildren_: %d pvsContent_: %d pvoAttributes_: %d) (%f|%f|%f|%f)", 
                    indent.c_str(), sName_.c_str(), nLevel_, Nodetype2String(eType_).c_str(), poParent_->sName_.c_str(), 
                    pvpoChildren_->size(), pvsContent_->size(), pvoAttributes_->size(),
                    psExtent_->dfX1, psExtent_->dfX2, psExtent_->dfY1, psExtent_->dfY2);
            else
                CPLDebug("KML", "%s%s (nLevel: %d Type: %s poParent: %s pvpoChildren_: %d pvsContent_: %d pvoAttributes_: %d)", 
                    indent.c_str(), sName_.c_str(), nLevel_, Nodetype2String(eType_).c_str(), poParent_->sName_.c_str(), 
                    pvpoChildren_->size(), pvsContent_->size(), pvoAttributes_->size());
        }
    }
    else
    {
        CPLDebug("KML", "%s%s (nLevel: %d Type: %s pvpoChildren_: %d pvsContent_: %d pvoAttributes_: %d)", 
            indent.c_str(), sName_.c_str(), Nodetype2String(eType_).c_str(), nLevel_, pvpoChildren_->size(), 
            pvsContent_->size(), pvoAttributes_->size());
    }

    if(what == 1 || what == 3)
    {
        for(kml_content_t::size_type z = 0; z < pvsContent_->size(); z++)
            CPLDebug("KML", "%s|->pvsContent_: '%s'", indent.c_str(), pvsContent_->at(z).c_str());
    }

    if(what == 2 || what == 3)
    {
        for(kml_attributes_t::size_type z = 0; z < pvoAttributes_->size(); z++)
            CPLDebug("KML", "%s|->pvoAttributes_: %s = '%s'", indent.c_str(), pvoAttributes_->at(z)->sName.c_str(), pvoAttributes_->at(z)->sValue.c_str());
    }

    for(kml_nodes_t::size_type z = 0; z < pvpoChildren_->size(); z++)
        pvpoChildren_->at(z)->print(what);
}

void KMLNode::classify(KML* poKML)
{
    Nodetype curr = Unknown;
    Nodetype all = Empty;
    
    const kml_nodes_t::size_type size = pvpoChildren_->size();
    for(kml_nodes_t::size_type z = 0; z < size; z++)
    {
        // Leafs are ignored
        if(poKML->isLeaf(pvpoChildren_->at(z)->sName_))
            continue;

        // Classify pvpoChildren_
        pvpoChildren_->at(z)->classify(poKML);

        if(poKML->isContainer(sName_))
        {
            curr = pvpoChildren_->at(z)->eType_;
        }
        else if(poKML->isFeatureContainer(sName_))
        {
            if(poKML->isFeature(pvpoChildren_->at(z)->sName_))
            {
                if(pvpoChildren_->at(z)->sName_.compare("Point") == 0)
                    curr = Point;
                else if(pvpoChildren_->at(z)->sName_.compare("LineString") == 0)
                    curr = LineString;
                else if(pvpoChildren_->at(z)->sName_.compare("Polygon") == 0)
                    curr = Polygon;
            }
            else if(poKML->isContainer(sName_))
            {
                curr = pvpoChildren_->at(z)->eType_;
            }
        }
        else if(poKML->isFeature(sName_) || poKML->isRest(sName_))
        {
            curr = Empty;
        }

        // Compare and return if it is mixed
        if(curr != all && all != Empty && curr != Empty)
        {
            eType_ = Mixed;
            
            if((poKML->isFeature(Nodetype2String(curr))
                && poKML->isFeatureContainer(Nodetype2String(all)))
                || (poKML->isFeature(Nodetype2String(all))
                    && poKML->isFeatureContainer(Nodetype2String(curr))))
            {
                CPLDebug("KML", "FeatureContainer and Feature");
                continue;
            }
        }
        else if(curr != Empty)
        {
            all = curr;
        }
    }

    if(eType_ == Unknown)
        eType_ = all;
}

void KMLNode::eliminateEmpty(KML* poKML)
{
    for(kml_nodes_t::size_type z = 0; z < pvpoChildren_->size(); z++)
    {
        if(pvpoChildren_->at(z)->eType_ == Empty
           && (poKML->isContainer(pvpoChildren_->at(z)->sName_)
               || poKML->isFeatureContainer(pvpoChildren_->at(z)->sName_)))
        {
            delete pvpoChildren_->at(z);
            pvpoChildren_->erase(pvpoChildren_->begin() + z);
            z--;
        }
        else
        {
            pvpoChildren_->at(z)->eliminateEmpty(poKML);
        }
    }
    calcExtent(poKML);
}

void KMLNode::setType(Nodetype oNotet)
{
    eType_ = oNotet;
}

Nodetype KMLNode::getType() const
{
    return eType_;
}

void KMLNode::setName(std::string const& sIn)
{
    sName_ = sIn;
}

std::string KMLNode::getName() const
{
    return sName_;
}

void KMLNode::setLevel(std::size_t nLev)
{
    nLevel_ = nLev;
}

std::size_t KMLNode::getLevel() const
{
    return nLevel_;
}

void KMLNode::addAttribute(Attribute *poAttr)
{
    pvoAttributes_->push_back(poAttr);
}

void KMLNode::setParent(KMLNode* poPar)
{
    poParent_ = poPar;
}

KMLNode* KMLNode::getParent() const
{
    return poParent_;
}

void KMLNode::addChildren(KMLNode *poChil)
{
    pvpoChildren_->push_back(poChil);
}

std::size_t KMLNode::countChildren()
{
    return pvpoChildren_->size();
}

KMLNode* KMLNode::getChild(std::size_t index) const
{
    return pvpoChildren_->at(index);
}

void KMLNode::addContent(std::string const& text)
{
    pvsContent_->push_back(text);
}

void KMLNode::appendContent(std::string const& text)
{
    std::string& tmp = pvsContent_->at(pvsContent_->size() - 1);
    tmp += text;
}

std::string KMLNode::getContent(std::size_t index) const
{
    std::string tmp;
    if( index < pvsContent_->size() )
    {
        tmp = pvsContent_->at(index);
    }
    return tmp;
}

void KMLNode::deleteContent(std::size_t index)
{
    if( index < pvsContent_->size() )
    {
        pvsContent_->erase(pvsContent_->begin() + index);
    }
}

std::size_t KMLNode::numContent()
{
    return pvsContent_->size();
}

void KMLNode::setLayerNumber(int nNum)
{
    nLayerNumber_ = nNum;
}

int KMLNode::getLayerNumber() const
{
    return nLayerNumber_;
}

KMLNode* KMLNode::getLayer(int nNum)
{
    KMLNode* poTmp = NULL;
    if(nLayerNumber_ == nNum)
        return this;

    kml_nodes_t::size_type size = pvpoChildren_->size();
    for( kml_nodes_t::size_type i = 0; i < size; ++i )
    {
        if((poTmp = pvpoChildren_->at(i)->getLayer(nNum)) != NULL)
            return poTmp;
    }

    return NULL;
}

std::string KMLNode::getNameElement() const
{
    std::string sElem;
    kml_nodes_t::size_type subsize = 0;
    kml_nodes_t::size_type size = pvpoChildren_->size();

    for( kml_nodes_t::size_type i = 0; i < size; ++i )
    {
        if( pvpoChildren_->at(i)->sName_.compare("name") == 0 )
        {
            subsize = pvpoChildren_->at(i)->pvsContent_->size();
            if( subsize > 0 )
            {
                sElem = pvpoChildren_->at(i)->pvsContent_->at(0);
                for( kml_nodes_t::size_type j = 1; j < subsize; ++j )
                {
                    sElem += " " + pvpoChildren_->at(i)->pvsContent_->at(j);
                }
                return sElem;
            }
            break;
        }
    }
    return "";
}

std::string KMLNode::getDescriptionElement() const
{
    std::string sElem;
    kml_nodes_t::size_type subsize = 0;
    kml_nodes_t::size_type size = pvpoChildren_->size();
    for( kml_nodes_t::size_type i = 0; i < size; ++i )
    {
        if( pvpoChildren_->at(i)->sName_.compare("description") == 0 )
        {
            subsize = pvpoChildren_->at(i)->pvsContent_->size();
            if ( subsize > 0 )
            {
                sElem = pvpoChildren_->at(i)->pvsContent_->at(0);
                for( kml_nodes_t::size_type j = 1; j < subsize; ++j )
                {
                    sElem += " " + pvpoChildren_->at(i)->pvsContent_->at(j);
                }
                return sElem;
            }
            break;
        }
    }
    return "";
}

std::size_t KMLNode::getNumFeatures() const
{
    std::size_t nNum = 0;
    kml_nodes_t::size_type size = pvpoChildren_->size();
    
    for( kml_nodes_t::size_type i = 0; i < size; ++i )
    {
        if(pvpoChildren_->at(i)->sName_ == "Placemark" )
            nNum++;
    }
    return nNum;
}

Feature* KMLNode::getFeature(std::size_t nNum)
{
    unsigned short nCount, nCount2, nCountP = 0;
    KMLNode* poFeat = NULL;
    KMLNode* poTemp = NULL;
    KMLNode* poCoor = NULL;
    std::string sContent;
    Coordinate* psCoord = NULL;
    std::vector<Coordinate*>* pvpsTmp = NULL;

    if(nNum >= this->getNumFeatures())
        return NULL;

    for(nCount = 0; nCount < pvpoChildren_->size(); nCount++)
    {
        if(pvpoChildren_->at(nCount)->sName_.compare("Placemark") == 0)
        {
            if(nCountP == nNum)
            {
                poFeat = pvpoChildren_->at(nCount);
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
    psReturn->eType = poFeat->eType_;
    // the coordinates (for a Point)
    if(poFeat->eType_ == Point)
    {
        psReturn->pvpsCoordinates = new std::vector<Coordinate*>;
        // Search Point Element
        for(nCount = 0; nCount < poFeat->pvpoChildren_->size(); nCount++)
        {
            if(poFeat->pvpoChildren_->at(nCount)->sName_.compare("Point") == 0)
            {
                poTemp = poFeat->pvpoChildren_->at(nCount);
                break;
            }
        }
        // poTemp must be a Point
        if(poTemp->sName_.compare("Point") != 0)
        {
            delete psReturn->pvpsCoordinates;
            delete psReturn;
            return NULL;
        }

        // Search coordinate Element
        for(nCount = 0; nCount < poTemp->pvpoChildren_->size(); nCount++)
        {
            if(poTemp->pvpoChildren_->at(nCount)->sName_.compare("coordinates") == 0)
            {
                poCoor = poTemp->pvpoChildren_->at(nCount);
                for(nCountP = 0; nCountP < poCoor->pvsContent_->size(); nCountP++)
                {
                    psCoord = ParseCoordinate(poCoor->pvsContent_->at(nCountP));
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
    else if(poFeat->eType_ == LineString)
    {
        psReturn->pvpsCoordinates = new std::vector<Coordinate*>;
        // Search LineString Element
        for(nCount = 0; nCount < poFeat->pvpoChildren_->size(); nCount++)
        {
            if(poFeat->pvpoChildren_->at(nCount)->sName_.compare("LineString") == 0)
            {
                poTemp = poFeat->pvpoChildren_->at(nCount);
                break;
            }
        }
        // poTemp must be a LineString
        if(poTemp->sName_.compare("LineString") != 0)
        {
            delete psReturn->pvpsCoordinates;
            delete psReturn;
            return NULL;
        }

        // Search coordinate Element
        for(nCount = 0; nCount < poTemp->pvpoChildren_->size(); nCount++)
        {
            if(poTemp->pvpoChildren_->at(nCount)->sName_.compare("coordinates") == 0)
            {
                poCoor = poTemp->pvpoChildren_->at(nCount);
                for(nCountP = 0; nCountP < poCoor->pvsContent_->size(); nCountP++)
                {
                    psCoord = ParseCoordinate(poCoor->pvsContent_->at(nCountP));
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
    else if(poFeat->eType_ == Polygon)
    {
        psReturn->pvpsCoordinates = new std::vector<Coordinate*>;
        // Search Polygon Element
        for(nCount = 0; nCount < poFeat->pvpoChildren_->size(); nCount++)
        {
            if(poFeat->pvpoChildren_->at(nCount)->sName_.compare("Polygon") == 0)
            {
                poTemp = poFeat->pvpoChildren_->at(nCount);
                break;
            }
        }
        // poTemp must be a Polygon
        if(poTemp->sName_.compare("Polygon") != 0)
        {
            delete psReturn->pvpsCoordinates;
            delete psReturn;
            return NULL;
        }

        //*********************************
        // Search outerBoundaryIs Element
        //*********************************
        for(nCount = 0; nCount < poTemp->pvpoChildren_->size(); nCount++)
        {
            if(poTemp->pvpoChildren_->at(nCount)->sName_.compare("outerBoundaryIs") == 0)
            {
                poCoor = poTemp->pvpoChildren_->at(nCount)->pvpoChildren_->at(0);
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
        for(nCount = 0; nCount < poCoor->pvpoChildren_->size(); nCount++)
        {
            if(poCoor->pvpoChildren_->at(nCount)->sName_.compare("coordinates") == 0)
            {
                for(nCountP = 0; nCountP < poCoor->pvpoChildren_->at(nCount)->pvsContent_->size(); nCountP++)
                {
                    psCoord = ParseCoordinate(poCoor->pvpoChildren_->at(nCount)->pvsContent_->at(nCountP));
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

        for(nCount2 = 0; nCount2 < poTemp->pvpoChildren_->size(); nCount2++)
        {
            if(poTemp->pvpoChildren_->at(nCount2)->sName_.compare("innerBoundaryIs") == 0)
            {
                pvpsTmp = new std::vector<Coordinate*>; 
                poCoor = poTemp->pvpoChildren_->at(nCount2)->pvpoChildren_->at(0);
                // Search coordinate Element
                for(nCount = 0; nCount < poCoor->pvpoChildren_->size(); nCount++)
                {
                    if(poCoor->pvpoChildren_->at(nCount)->sName_.compare("coordinates") == 0)
                    {
                        for(nCountP = 0; nCountP < poCoor->pvpoChildren_->at(nCount)->pvsContent_->size(); nCountP++)
                        {
                            psCoord = ParseCoordinate(poCoor->pvpoChildren_->at(nCount)->pvsContent_->at(nCountP));
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

void KMLNode::calcExtent(KML *poKML)
{
    CPLAssert( NULL != poKML );

    KMLNode* poTmp = NULL;
    Coordinate* psCoords = NULL;
    
    if( psExtent_ != NULL )
        return;

    // Handle Features
    if(poKML->isFeature(sName_))
    {
        psExtent_ = new Extent();

        // Special for Polygons
        if(sName_.compare("Polygon") == 0)
        {
            for(unsigned short nCount = 0; nCount < pvpoChildren_->size(); nCount++)
            {
                if(pvpoChildren_->at(nCount)->sName_.compare("outerBoundaryIs") == 0
                   || pvpoChildren_->at(nCount)->sName_.compare("innerBoundaryIs") == 0)
                {
                    if(pvpoChildren_->at(nCount)->pvpoChildren_->size() == 1)
                    {
                        poTmp = pvpoChildren_->at(nCount)->pvpoChildren_->at(0);
                        for(unsigned short nCount3 = 0; nCount3 < poTmp->pvpoChildren_->size(); nCount3++)
                        {
                            if(poTmp->pvpoChildren_->at(nCount3)->sName_.compare("coordinates") == 0)
                            {
                                for(unsigned short nCount2 = 0;
                                    nCount2 < poTmp->pvpoChildren_->at(nCount3)->pvsContent_->size(); nCount2++)
                                {
                                    psCoords = ParseCoordinate(poTmp->pvpoChildren_->at(nCount3)->pvsContent_->at(nCount2));
                                    if(psCoords != NULL)
                                    {
                                        if(psCoords->dfLongitude < psExtent_->dfX1 || psExtent_->dfX1 == 0)
                                            psExtent_->dfX1 = psCoords->dfLongitude;
                                        if(psCoords->dfLongitude > psExtent_->dfX2 || psExtent_->dfX2 == 0)
                                            psExtent_->dfX2 = psCoords->dfLongitude;
                                        if(psCoords->dfLatitude < psExtent_->dfY1 || psExtent_->dfY1 == 0)
                                            psExtent_->dfY1 = psCoords->dfLatitude;
                                        if(psCoords->dfLatitude > psExtent_->dfY2 || psExtent_->dfY2 == 0)
                                            psExtent_->dfY2 = psCoords->dfLatitude;
                                        delete psCoords;
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
            for(unsigned short nCount = 0; nCount < pvpoChildren_->size(); nCount++)
            {
                if(pvpoChildren_->at(nCount)->sName_.compare("coordinates") == 0)
                {
                    poTmp = pvpoChildren_->at(nCount);
                    for(unsigned short nCount2 = 0; nCount2 < poTmp->pvsContent_->size(); nCount2++)
                    {
                        psCoords = ParseCoordinate(poTmp->pvsContent_->at(nCount2));
                        if(psCoords != NULL)
                        {
                            if(psCoords->dfLongitude < psExtent_->dfX1 || psExtent_->dfX1 == 0.0)
                                psExtent_->dfX1 = psCoords->dfLongitude;
                            if(psCoords->dfLongitude > psExtent_->dfX2 || psExtent_->dfX2 == 0.0)
                                psExtent_->dfX2 = psCoords->dfLongitude;
                            if(psCoords->dfLatitude < psExtent_->dfY1 || psExtent_->dfY1 == 0.0)
                                psExtent_->dfY1 = psCoords->dfLatitude;
                            if(psCoords->dfLatitude > psExtent_->dfY2 || psExtent_->dfY2 == 0.0)
                                psExtent_->dfY2 = psCoords->dfLatitude;
                            delete psCoords;
                        }
                    }
                }
            }
        }
    // Summarize Containers
    }
    else if( poKML->isFeatureContainer(sName_)
             || poKML->isContainer(sName_))
    {
        psExtent_ = new Extent;
        psExtent_->dfX1 = psExtent_->dfX2 = psExtent_->dfY1 = psExtent_->dfY2 = 0.0;
        for(unsigned short nCount = 0; nCount < pvpoChildren_->size(); nCount++)
        {
            pvpoChildren_->at(nCount)->calcExtent(poKML);
            if(pvpoChildren_->at(nCount)->psExtent_ != NULL)
            {
                if(pvpoChildren_->at(nCount)->psExtent_->dfX1 < psExtent_->dfX1
                   || psExtent_->dfX1 == 0)
                    psExtent_->dfX1 = pvpoChildren_->at(nCount)->psExtent_->dfX1;
                if(pvpoChildren_->at(nCount)->psExtent_->dfX2 > psExtent_->dfX2
                   || psExtent_->dfX2 == 0)
                    psExtent_->dfX2 = pvpoChildren_->at(nCount)->psExtent_->dfX2;
                if(pvpoChildren_->at(nCount)->psExtent_->dfY1 < psExtent_->dfY1
                   || psExtent_->dfY1 == 0)
                    psExtent_->dfY1 = pvpoChildren_->at(nCount)->psExtent_->dfY1;
                if(pvpoChildren_->at(nCount)->psExtent_->dfY2 > psExtent_->dfY2
                   || psExtent_->dfY2 == 0)
                    psExtent_->dfY2 = pvpoChildren_->at(nCount)->psExtent_->dfY2;
            }
        }
    }
}

Extent const* KMLNode::getExtents() const
{
    return psExtent_;
}

