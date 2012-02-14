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
    else if(type == MultiGeometry)
        return "MultiGeometry";
    else if(type == MultiPoint)
        return "MultiPoint";
    else if(type == MultiLineString)
        return "MultiLineString";
    else if(type == MultiPolygon)
        return "MultiPolygon";
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
    psTmp->dfLongitude = CPLAtof(text.substr(0, (pos - 1)).c_str());

    // Y coordinate
    if(text[pos - 1] != ',')
    {
        delete psTmp;
        return NULL;
    }
    std::string tmp(text.substr(pos, text.length() - pos));
    pos = 0;
    while(isNumberDigit(tmp[pos++]));
    psTmp->dfLatitude = CPLAtof(tmp.substr(0, (pos - 1)).c_str());
    
    // Z coordinate
    if(tmp[pos - 1] != ',')
    {
        psTmp->bHasZ = FALSE;
        psTmp->dfAltitude = 0;
        return psTmp;
    }
    tmp = tmp.substr(pos, tmp.length() - pos);
    pos = 0;
    while(isNumberDigit(tmp[pos++]));
    psTmp->bHasZ = TRUE;
    psTmp->dfAltitude = CPLAtof(tmp.substr(0, (pos - 1)).c_str());

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
    b25D_ = FALSE;
    nNumFeatures_ = -1;
}

KMLNode::~KMLNode()
{
    CPLAssert( NULL != pvpoChildren_ );
    CPLAssert( NULL != pvoAttributes_ );

    kml_nodes_t::iterator itChild;
    for( itChild = pvpoChildren_->begin();
         itChild != pvpoChildren_->end(); ++itChild)
    {
        delete (*itChild);
    }
    delete pvpoChildren_;

    kml_attributes_t::iterator itAttr;
    for( itAttr = pvoAttributes_->begin();
         itAttr != pvoAttributes_->end(); ++itAttr)
    {
        delete (*itAttr);
    }
    delete pvoAttributes_;

    delete pvsContent_;
}

void KMLNode::print(unsigned int what)
{
    std::string indent;
    for(std::size_t l = 0; l < nLevel_; l++)
        indent += " ";

    if(nLevel_ > 0)
    {
        if(nLayerNumber_ > -1)
        {
            CPLDebug("KML", "%s%s (nLevel: %d Type: %s poParent: %s pvpoChildren_: %d pvsContent_: %d pvoAttributes_: %d) <--- Layer #%d", 
                        indent.c_str(), sName_.c_str(), (int) nLevel_, Nodetype2String(eType_).c_str(), poParent_->sName_.c_str(), 
                        (int) pvpoChildren_->size(), (int) pvsContent_->size(), (int) pvoAttributes_->size(), nLayerNumber_);
        }
        else
        {
            CPLDebug("KML", "%s%s (nLevel: %d Type: %s poParent: %s pvpoChildren_: %d pvsContent_: %d pvoAttributes_: %d)", 
                        indent.c_str(), sName_.c_str(), (int) nLevel_, Nodetype2String(eType_).c_str(), poParent_->sName_.c_str(), 
                        (int) pvpoChildren_->size(), (int) pvsContent_->size(), (int) pvoAttributes_->size());
        }
    }
    else
    {
        CPLDebug("KML", "%s%s (nLevel: %d Type: %s pvpoChildren_: %d pvsContent_: %d pvoAttributes_: %d)", 
                 indent.c_str(), sName_.c_str(), (int) nLevel_, Nodetype2String(eType_).c_str(), (int) pvpoChildren_->size(), 
                 (int) pvsContent_->size(), (int) pvoAttributes_->size());
    }

    if(what == 1 || what == 3)
    {
        for(kml_content_t::size_type z = 0; z < pvsContent_->size(); z++)
            CPLDebug("KML", "%s|->pvsContent_: '%s'", indent.c_str(), (*pvsContent_)[z].c_str());
    }

    if(what == 2 || what == 3)
    {
        for(kml_attributes_t::size_type z = 0; z < pvoAttributes_->size(); z++)
            CPLDebug("KML", "%s|->pvoAttributes_: %s = '%s'", indent.c_str(), (*pvoAttributes_)[z]->sName.c_str(), (*pvoAttributes_)[z]->sValue.c_str());
    }

    for(kml_nodes_t::size_type z = 0; z < pvpoChildren_->size(); z++)
        (*pvpoChildren_)[z]->print(what);
}

//static int nDepth = 0;
//static char* genSpaces()
//{
//    static char spaces[128];
//    int i;
//    for(i=0;i<nDepth;i++)
//        spaces[i] = ' ';
//    spaces[i] = '\0';
//    return spaces;
//}

int KMLNode::classify(KML* poKML, int nRecLevel)
{
    Nodetype curr = Unknown;
    Nodetype all = Empty;

    /* Arbitrary value, but certainly large enough for reasonable usages ! */
    if( nRecLevel == 32 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Too many recursiong level (%d) while parsing KML geometry.",
                    nRecLevel );
        return NULL;
    }

    //CPLDebug("KML", "%s<%s>", genSpaces(), sName_.c_str());
    //nDepth ++;
    
    if(sName_.compare("Point") == 0)
        eType_ = Point;
    else if(sName_.compare("LineString") == 0)
        eType_ = LineString;
    else if(sName_.compare("Polygon") == 0)
        eType_ = Polygon;
    else if(poKML->isRest(sName_))
        eType_ = Empty;
    else if(sName_.compare("coordinates") == 0)
    {
        unsigned int nCountP;
        for(nCountP = 0; nCountP < pvsContent_->size(); nCountP++)
        {
            const char* pszCoord = (*pvsContent_)[nCountP].c_str();
            int nComma = 0;
            while(TRUE)
            {
                pszCoord = strchr(pszCoord, ',');
                if (pszCoord)
                {
                    nComma ++;
                    pszCoord ++;
                }
                else
                    break;
            }
            if (nComma == 2)
                b25D_ = TRUE;
        }
    }

    const kml_nodes_t::size_type size = pvpoChildren_->size();
    for(kml_nodes_t::size_type z = 0; z < size; z++)
    {
        //CPLDebug("KML", "%s[%d] %s", genSpaces(), z, (*pvpoChildren_)[z]->sName_.c_str());

        // Classify pvpoChildren_
        if (!(*pvpoChildren_)[z]->classify(poKML, nRecLevel + 1))
            return FALSE;

        curr = (*pvpoChildren_)[z]->eType_;
        b25D_ |= (*pvpoChildren_)[z]->b25D_;

        // Compare and return if it is mixed
        if(curr != all && all != Empty && curr != Empty)
        {
            if (sName_.compare("MultiGeometry") == 0)
                eType_ = MultiGeometry;
            else
                eType_ = Mixed;
        }
        else if(curr != Empty)
        {
            all = curr;
        }
    }

    if(eType_ == Unknown)
    {
        if (sName_.compare("MultiGeometry") == 0)
        {
            if (all == Point)
                eType_ = MultiPoint;
            else if (all == LineString)
                eType_ = MultiLineString;
            else if (all == Polygon)
                eType_ = MultiPolygon;
            else
                eType_ = MultiGeometry;
        }
        else
            eType_ = all;
    }

    //nDepth --;
    //CPLDebug("KML", "%s</%s> --> eType=%s", genSpaces(), sName_.c_str(), Nodetype2String(eType_).c_str());

    return TRUE;
}

void KMLNode::eliminateEmpty(KML* poKML)
{
    for(kml_nodes_t::size_type z = 0; z < pvpoChildren_->size(); z++)
    {
        if((*pvpoChildren_)[z]->eType_ == Empty
           && (poKML->isContainer((*pvpoChildren_)[z]->sName_)
               || poKML->isFeatureContainer((*pvpoChildren_)[z]->sName_)))
        {
            delete (*pvpoChildren_)[z];
            pvpoChildren_->erase(pvpoChildren_->begin() + z);
            z--;
        }
        else
        {
            (*pvpoChildren_)[z]->eliminateEmpty(poKML);
        }
    }
}

bool KMLNode::hasOnlyEmpty() const
{
    for(kml_nodes_t::size_type z = 0; z < pvpoChildren_->size(); z++)
    {
        if((*pvpoChildren_)[z]->eType_ != Empty)
        {
            return false;
        }
        else
        {
            if (!(*pvpoChildren_)[z]->hasOnlyEmpty())
                return false;
        }
    }

    return true;
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

const std::string& KMLNode::getName() const
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
    return (*pvpoChildren_)[index];
}

void KMLNode::addContent(std::string const& text)
{
    pvsContent_->push_back(text);
}

void KMLNode::appendContent(std::string const& text)
{
    std::string& tmp = (*pvsContent_)[pvsContent_->size() - 1];
    tmp += text;
}

std::string KMLNode::getContent(std::size_t index) const
{
    return (*pvsContent_)[index];
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

std::string KMLNode::getNameElement() const
{
    kml_nodes_t::size_type subsize = 0;
    kml_nodes_t::size_type size = pvpoChildren_->size();

    for( kml_nodes_t::size_type i = 0; i < size; ++i )
    {
        if( (*pvpoChildren_)[i]->sName_.compare("name") == 0 )
        {
            subsize = (*pvpoChildren_)[i]->pvsContent_->size();
            if( subsize > 0 )
            {
                return (*(*pvpoChildren_)[i]->pvsContent_)[0];
            }
            break;
        }
    }
    return "";
}

std::string KMLNode::getDescriptionElement() const
{
    kml_nodes_t::size_type subsize = 0;
    kml_nodes_t::size_type size = pvpoChildren_->size();
    for( kml_nodes_t::size_type i = 0; i < size; ++i )
    {
        if( (*pvpoChildren_)[i]->sName_.compare("description") == 0 )
        {
            subsize = (*pvpoChildren_)[i]->pvsContent_->size();
            if ( subsize > 0 )
            {
                return (*(*pvpoChildren_)[i]->pvsContent_)[0];
            }
            break;
        }
    }
    return "";
}

std::size_t KMLNode::getNumFeatures()
{
    if (nNumFeatures_ < 0)
    {
        std::size_t nNum = 0;
        kml_nodes_t::size_type size = pvpoChildren_->size();
        
        for( kml_nodes_t::size_type i = 0; i < size; ++i )
        {
            if( (*pvpoChildren_)[i]->sName_ == "Placemark" )
                nNum++;
        }
        nNumFeatures_ = (int)nNum;
    }
    return nNumFeatures_;
}

OGRGeometry* KMLNode::getGeometry(Nodetype eType)
{
    unsigned int nCount, nCount2, nCountP;
    OGRGeometry* poGeom = NULL;
    KMLNode* poCoor = NULL;
    Coordinate* psCoord = NULL;

    if (sName_.compare("Point") == 0)
    {
        // Search coordinate Element
        for(nCount = 0; nCount < pvpoChildren_->size(); nCount++)
        {
            if((*pvpoChildren_)[nCount]->sName_.compare("coordinates") == 0)
            {
                poCoor = (*pvpoChildren_)[nCount];
                for(nCountP = 0; nCountP < poCoor->pvsContent_->size(); nCountP++)
                {
                    psCoord = ParseCoordinate((*poCoor->pvsContent_)[nCountP]);
                    if(psCoord != NULL)
                    {
                        if (psCoord->bHasZ)
                            poGeom = new OGRPoint(psCoord->dfLongitude,
                                                  psCoord->dfLatitude,
                                                  psCoord->dfAltitude);
                        else
                            poGeom = new OGRPoint(psCoord->dfLongitude,
                                                  psCoord->dfLatitude);
                        delete psCoord;
                        return poGeom;
                    }
                }
            }
        }
        poGeom = new OGRPoint();
    }
    else if (sName_.compare("LineString") == 0)
    {
        // Search coordinate Element
        poGeom = new OGRLineString();
        for(nCount = 0; nCount < pvpoChildren_->size(); nCount++)
        {
            if((*pvpoChildren_)[nCount]->sName_.compare("coordinates") == 0)
            {
                poCoor = (*pvpoChildren_)[nCount];
                for(nCountP = 0; nCountP < poCoor->pvsContent_->size(); nCountP++)
                {
                    psCoord = ParseCoordinate((*poCoor->pvsContent_)[nCountP]);
                    if(psCoord != NULL)
                    {
                        if (psCoord->bHasZ)
                            ((OGRLineString*)poGeom)->addPoint(psCoord->dfLongitude,
                                                               psCoord->dfLatitude,
                                                               psCoord->dfAltitude);
                        else
                            ((OGRLineString*)poGeom)->addPoint(psCoord->dfLongitude,
                                                               psCoord->dfLatitude);
                        delete psCoord;
                    }
                }
            }
        }
    }
    else if (sName_.compare("Polygon") == 0)
    {
        //*********************************
        // Search outerBoundaryIs Element
        //*********************************
        poGeom = new OGRPolygon();
        for(nCount = 0; nCount < pvpoChildren_->size(); nCount++)
        {
            if((*pvpoChildren_)[nCount]->sName_.compare("outerBoundaryIs") == 0 &&
               (*pvpoChildren_)[nCount]->pvpoChildren_->size() > 0)
            {
                poCoor = (*(*pvpoChildren_)[nCount]->pvpoChildren_)[0];
            }
        }
        // No outer boundary found
        if(poCoor == NULL)
        {
            return poGeom;
        }
        // Search coordinate Element
        OGRLinearRing* poLinearRing = NULL;
        for(nCount = 0; nCount < poCoor->pvpoChildren_->size(); nCount++)
        {
            if((*poCoor->pvpoChildren_)[nCount]->sName_.compare("coordinates") == 0)
            {
                for(nCountP = 0; nCountP < (*poCoor->pvpoChildren_)[nCount]->pvsContent_->size(); nCountP++)
                {
                    psCoord = ParseCoordinate((*(*poCoor->pvpoChildren_)[nCount]->pvsContent_)[nCountP]);
                    if(psCoord != NULL)
                    {
                        if (poLinearRing == NULL)
                        {
                            poLinearRing = new OGRLinearRing();
                        }
                        if (psCoord->bHasZ)
                            poLinearRing->addPoint(psCoord->dfLongitude,
                                                   psCoord->dfLatitude,
                                                   psCoord->dfAltitude);
                        else
                            poLinearRing->addPoint(psCoord->dfLongitude,
                                                   psCoord->dfLatitude);
                        delete psCoord;
                    }
                }
            }
        }
        // No outer boundary coordinates found
        if(poLinearRing == NULL)
        {
            return poGeom;
        }

        ((OGRPolygon*)poGeom)->addRingDirectly(poLinearRing);
        poLinearRing = NULL;

        //*********************************
        // Search innerBoundaryIs Elements
        //*********************************

        for(nCount2 = 0; nCount2 < pvpoChildren_->size(); nCount2++)
        {
            if((*pvpoChildren_)[nCount2]->sName_.compare("innerBoundaryIs") == 0)
            {
                if (poLinearRing)
                    ((OGRPolygon*)poGeom)->addRingDirectly(poLinearRing);
                poLinearRing = NULL;

                if ((*pvpoChildren_)[nCount2]->pvpoChildren_->size() == 0)
                    continue;

                poLinearRing = new OGRLinearRing();

                poCoor = (*(*pvpoChildren_)[nCount2]->pvpoChildren_)[0];
                // Search coordinate Element
                for(nCount = 0; nCount < poCoor->pvpoChildren_->size(); nCount++)
                {
                    if((*poCoor->pvpoChildren_)[nCount]->sName_.compare("coordinates") == 0)
                    {
                        for(nCountP = 0; nCountP < (*poCoor->pvpoChildren_)[nCount]->pvsContent_->size(); nCountP++)
                        {
                            psCoord = ParseCoordinate((*(*poCoor->pvpoChildren_)[nCount]->pvsContent_)[nCountP]);
                            if(psCoord != NULL)
                            {
                                if (psCoord->bHasZ)
                                    poLinearRing->addPoint(psCoord->dfLongitude,
                                                        psCoord->dfLatitude,
                                                        psCoord->dfAltitude);
                                else
                                    poLinearRing->addPoint(psCoord->dfLongitude,
                                                        psCoord->dfLatitude);
                                delete psCoord;
                            }
                        }
                    }
                }
            }
        }

        if (poLinearRing)
            ((OGRPolygon*)poGeom)->addRingDirectly(poLinearRing);
    }
    else if (sName_.compare("MultiGeometry") == 0)
    {
        if (eType == MultiPoint)
            poGeom = new OGRMultiPoint();
        else if (eType == MultiLineString)
            poGeom = new OGRMultiLineString();
        else if (eType == MultiPolygon)
            poGeom = new OGRMultiPolygon();
        else
            poGeom = new OGRGeometryCollection();
        for(nCount = 0; nCount < pvpoChildren_->size(); nCount++)
        {
            OGRGeometry* poSubGeom = (*pvpoChildren_)[nCount]->getGeometry();
            if (poSubGeom)
                ((OGRGeometryCollection*)poGeom)->addGeometryDirectly(poSubGeom);
        }
    }

    return poGeom;
}

Feature* KMLNode::getFeature(std::size_t nNum, int& nLastAsked, int &nLastCount)
{
    unsigned int nCount, nCountP = 0;
    KMLNode* poFeat = NULL;
    KMLNode* poTemp = NULL;

    if(nNum >= this->getNumFeatures())
        return NULL;

    if (nLastAsked + 1 != (int)nNum)
    {
        nCount = 0;
        nCountP = 0;
    }
    else
    {
        nCount = nLastCount + 1;
        nCountP = nLastAsked + 1;
    }

    for(; nCount < pvpoChildren_->size(); nCount++)
    {
        if((*pvpoChildren_)[nCount]->sName_.compare("Placemark") == 0)
        {
            if(nCountP == nNum)
            {
                poFeat = (*pvpoChildren_)[nCount];
                break;
            }
            nCountP++;
        }
    }

    nLastAsked = nNum;
    nLastCount = nCount;

    if(poFeat == NULL)
        return NULL;
        
    // Create a feature structure
    Feature *psReturn = new Feature;
    // Build up the name
    psReturn->sName = poFeat->getNameElement();
    // Build up the description
    psReturn->sDescription = poFeat->getDescriptionElement();
    // the type
    psReturn->eType = poFeat->eType_;

    std::string sElementName;
    if(poFeat->eType_ == Point ||
       poFeat->eType_ == LineString ||
       poFeat->eType_ == Polygon)
        sElementName = Nodetype2String(poFeat->eType_);
    else if (poFeat->eType_ == MultiGeometry || 
             poFeat->eType_ == MultiPoint || 
             poFeat->eType_ == MultiLineString || 
             poFeat->eType_ == MultiPolygon)
        sElementName = "MultiGeometry";
    else
    {
        delete psReturn;
        return NULL;
    }

    for(nCount = 0; nCount < poFeat->pvpoChildren_->size(); nCount++)
    {
        if((*poFeat->pvpoChildren_)[nCount]->sName_.compare(sElementName) == 0)
        {
            poTemp = (*poFeat->pvpoChildren_)[nCount];
            psReturn->poGeom = poTemp->getGeometry(poFeat->eType_);
            if(psReturn->poGeom)
                return psReturn;
            else
            {
                delete psReturn;
                return NULL;
            }
        }
    }

    delete psReturn;
    return NULL;
}
