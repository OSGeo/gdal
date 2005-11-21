/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 2 Reader
 * Purpose:  Implementation of ILI2Reader class.
 * Author:   Markus Schnider, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.3  2005/11/21 17:06:24  fwarmerdam
 * avoid const iterator for VC6 compatibility
 *
 * Revision 1.2  2005/08/06 22:21:53  pka
 * Area polygonizer added
 *
 * Revision 1.1  2005/07/08 22:10:57  pka
 * Initial import of OGR Interlis driver
 *
 */

#include "ogr_ili2.h"
#include "cpl_conv.h"
#include "cpl_string.h"

#include "ili2reader.h"
#include "ili2readerp.h"

using namespace std;

CPL_CVSID("$Id$");

//
// constants
//
static const char *ILI2_TID = "TID";
static const char *ILI2_REF = "REF";

static const int ILI2_STRING_TYPE = 0;
static const int ILI2_COORD_TYPE = 1;
static const int ILI2_ARC_TYPE = 2;
static const int ILI2_POLYLINE_TYPE = 4;
static const int ILI2_BOUNDARY_TYPE = 8;
static const int ILI2_AREA_TYPE = 16; // also SURFACE
static const int ILI2_GEOMCOLL_TYPE = 32;

static const char *ILI2_COORD = "COORD";
static const char *ILI2_ARC = "ARC";
static const char *ILI2_POLYLINE = "POLYLINE";
static const char *ILI2_BOUNDARY = "BOUNDARY";
static const char *ILI2_AREA = "AREA";
static const char *ILI2_SURFACE = "SURFACE";


// 
// utili2ties
// 
int cmpStr(string s1, string s2) {
  
  string::const_iterator p1 = s1.begin();
  string::const_iterator p2 = s2.begin();

  while (p1 != s1.end() && p2 != s2.end()) {
    if (toupper(*p1) != toupper(*p2))
      return (toupper(*p1) < toupper(*p2)) ? -1 : 1;
    ++p1;
    ++p2;
  }

  return (s2.size() == s1.size()) ? 0 :
         (s1.size() < s2.size()) ? -1 : 1;
}

string ltrim(string tmpstr) {
  unsigned int i = 0;
  for (i = 0; i < tmpstr.length() - 1; i++)
    if ((tmpstr[i] != ' ') && (tmpstr[i] != '\t') && (tmpstr[i] != '\r') && (tmpstr[i] != '\n'))
      return tmpstr.substr(i, tmpstr.length());
  return tmpstr.substr(i, tmpstr.length());
}

string rtrim(string tmpstr) {
  unsigned int i = tmpstr.length() - 1;
  for (i = tmpstr.length() - 1; i > 0; i--)
    if ((tmpstr[i] != ' ') && (tmpstr[i] != '\t') && (tmpstr[i] != '\r') && (tmpstr[i] != '\n'))
      return tmpstr.substr(i, tmpstr.length());
  return tmpstr.substr(0, i);
}

string trim(string tmpstr) {
  tmpstr = ltrim(tmpstr);
  tmpstr = rtrim(tmpstr);
  return tmpstr;
}

int getTypeOfGeometry(DOMElement* elem, bool parseDown) {
  int type = ILI2_STRING_TYPE;
  
  DOMElement *childElem = (DOMElement *)elem->getFirstChild();
  if ((childElem != NULL) && (childElem->getNodeType() == DOMNode::ELEMENT_NODE)) {

    if (cmpStr(ILI2_COORD, XMLString::transcode(childElem->getTagName())) == 0) {
      type |= ILI2_COORD_TYPE;
    } else if (cmpStr(ILI2_ARC, XMLString::transcode(childElem->getTagName())) == 0) {
      type |= ILI2_ARC_TYPE;
    } else if (cmpStr(ILI2_POLYLINE, XMLString::transcode(childElem->getTagName())) == 0) {
      type |= ILI2_POLYLINE_TYPE;
    } else if (cmpStr(ILI2_BOUNDARY, XMLString::transcode(childElem->getTagName())) == 0) {
      type |= ILI2_BOUNDARY_TYPE;
    } else if (cmpStr(ILI2_AREA, XMLString::transcode(childElem->getTagName())) == 0) {
      type |= ILI2_AREA_TYPE;
    } else if (cmpStr(ILI2_SURFACE, XMLString::transcode(childElem->getTagName())) == 0) {
      type |= ILI2_AREA_TYPE;
    }  
    
    if ((type == ILI2_STRING_TYPE) || parseDown) {
      DOMElement* subChildElem = (DOMElement*)childElem->getFirstChild();
      if ((subChildElem != NULL) && (childElem->getNodeType() == DOMNode::ELEMENT_NODE)) {
        type |= getTypeOfGeometry(subChildElem, parseDown);
      }
    }
    
    DOMElement* nextElem = (DOMElement*)childElem->getNextSibling();
    if ((nextElem != NULL) && (childElem->getNodeType() == DOMNode::ELEMENT_NODE)) {
      if (nextElem->getNodeType() == DOMNode::ELEMENT_NODE) {
        int subType = getTypeOfGeometry(nextElem, parseDown);
        if (type == subType) {
          switch (type) {
            case ILI2_STRING_TYPE :
            case ILI2_COORD_TYPE :
            case ILI2_ARC_TYPE :
            case ILI2_BOUNDARY_TYPE : 
              break;
            default :
              type |= subType;
              if (parseDown)
                type |= ILI2_GEOMCOLL_TYPE;
              else
                type = ILI2_GEOMCOLL_TYPE;
          }
        } else {
          if (type < subType) {
            type |= subType;
          } else {
            if (parseDown)
              type |= ILI2_GEOMCOLL_TYPE;
            else
              type = ILI2_GEOMCOLL_TYPE;
          }
        }
      }
    }
  }
  
  return type;
}

char *getObjValue(DOMElement *elem) {
  DOMElement *textElem = (DOMElement *)elem->getFirstChild();
  
  if ((textElem != NULL) && (textElem->getNodeType() == DOMNode::TEXT_NODE))
    return CPLStrdup(XMLString::transcode(textElem->getNodeValue()));
  
  return NULL;
}

char *getREFValue(DOMElement *elem) {  
  return CPLStrdup(XMLString::transcode(elem->getAttribute(XMLString::transcode(ILI2_REF))));
}

OGRPoint *getPoint(DOMElement *elem) {
  // elem -> COORD (or ARC)
  OGRPoint *pt = new OGRPoint();
  
  DOMElement *coordElem = (DOMElement *)elem->getFirstChild();
  while (coordElem != NULL) {
    if (cmpStr("C1", XMLString::transcode(coordElem->getTagName())) == 0)
      pt->setX(atof(getObjValue(coordElem)));
    else if (cmpStr("C2", XMLString::transcode(coordElem->getTagName())) == 0)
      pt->setY(atof(getObjValue(coordElem)));
    else if (cmpStr("C3", XMLString::transcode(coordElem->getTagName())) == 0)
      pt->setZ(atof(getObjValue(coordElem)));
    coordElem = (DOMElement *)coordElem->getNextSibling();
  }
  
  return pt;
}

OGRPoint *getARCCenter(OGRPoint *ptStart, OGRPoint *ptArc, OGRPoint *ptEnd) {  
  // FIXME precision
  double bx = ptStart->getX(); double by = ptStart->getY();
  double cx = ptArc->getX(); double cy = ptArc->getY();
  double dx = ptEnd->getX(); double dy = ptEnd->getY();
  double temp, bc, cd, det, x, y;
  
  temp = cx*cx+cy*cy;
  bc = (bx*bx + by*by - temp)/2.0;
  cd = (temp - dx*dx - dy*dy)/2.0;
  det = (bx-cx)*(cy-dy)-(cx-dx)*(by-cy);
  
  OGRPoint *center = new OGRPoint();
  
  if (fabs(det) < 1.0e-6) { // could not determin the determinante: too small
    return center;
  }    
  det = 1/det;
  x = (bc*(cy-dy)-cd*(by-cy))*det;
  y = ((bx-cx)*cd-(cx-dx)*bc)*det;
  
  center->setX(x);
  center->setY(y);
  
  return center;
  //r = sqrt((x-bx)*(x-bx)+(y-by)*(y-by));
}

OGRPoint *getPointBetween(OGRPoint *center, OGRPoint *pt1, OGRPoint *pt2) {
  // FIXME precision
  OGRPoint *middlePoint = new OGRPoint();
  
  double cx = center->getX(); double cy = center->getY();
  double px = pt1->getX(); double py = pt1->getY();
  
  double mx = ((px - cx)+(pt2->getX() - cx));
  double my = ((py - cy)+(pt2->getY() - cy));
  double mlen = sqrt(mx*mx+my*my);
  
  double r = sqrt((cx-px)*(cx-px)+(cy-py)*(cy-py));
  
  middlePoint->setX(cx + mx / mlen * r);
  middlePoint->setY(cy + my / mlen * r);
  middlePoint->setZ(pt1->getZ());
  
  return middlePoint;
}

OGRLineString *getArc(DOMElement *elem) { // FIXME only 4 points on arc 
  // elem -> ARC
  OGRLineString *ls = new OGRLineString();
  // previous point -> start point
  OGRPoint *ptStart = getPoint((DOMElement *)elem->getPreviousSibling()); // COORD or ARC
  // end point
  OGRPoint *ptEnd = new OGRPoint();
  // point on the arc 
  OGRPoint *ptOnArc = new OGRPoint();
  double radius = 0; // radius
  
  DOMElement *arcElem = (DOMElement *)elem->getFirstChild();
  while (arcElem != NULL) {
    if (cmpStr("C1", XMLString::transcode(arcElem->getTagName())) == 0)
      ptEnd->setX(atof(getObjValue(arcElem)));
    else if (cmpStr("C2", XMLString::transcode(arcElem->getTagName())) == 0)
      ptEnd->setY(atof(getObjValue(arcElem)));
    else if (cmpStr("C3", XMLString::transcode(arcElem->getTagName())) == 0)
      ptEnd->setZ(atof(getObjValue(arcElem)));
    else if (cmpStr("A1", XMLString::transcode(arcElem->getTagName())) == 0)
      ptOnArc->setX(atof(getObjValue(arcElem)));
    else if (cmpStr("A2", XMLString::transcode(arcElem->getTagName())) == 0)
      ptOnArc->setY(atof(getObjValue(arcElem)));
    else if (cmpStr("A3", XMLString::transcode(arcElem->getTagName())) == 0)
      ptOnArc->setZ(atof(getObjValue(arcElem)));
    else if (cmpStr("R", XMLString::transcode(arcElem->getTagName())) == 0)
      radius = atof(getObjValue(arcElem));
      
    arcElem = (DOMElement *)arcElem->getNextSibling();
  }
  //ls->addPoint(ptStart);// is set before
  if (fabs(radius) > 0) {
    //r = sqrt((cx-ptx)*(cx-ptx)+(cy-pty)*(cy-pty));
    radius = fabs(radius);
    OGRPoint *center = getARCCenter(ptStart, ptOnArc, ptEnd);  
    ls->addPoint(getPointBetween(center, ptStart, ptOnArc));
    ls->addPoint(ptOnArc);
    ls->addPoint(getPointBetween(center, ptOnArc, ptEnd));
    ls->addPoint(ptEnd);
  } else {
    ls->addPoint(ptOnArc);
    ls->addPoint(ptEnd);
  }
  
  return ls;
}

OGRLineString *getLineString(DOMElement *elem) {
  // elem -> POLYLINE
  OGRLineString *ls = new OGRLineString();
  
  DOMElement *lineElem = (DOMElement *)elem->getFirstChild();
  while (lineElem != NULL) {
    if (cmpStr(ILI2_COORD, XMLString::transcode(lineElem->getTagName())) == 0)
      ls->addPoint(getPoint(lineElem));
    else if (cmpStr(ILI2_ARC, XMLString::transcode(lineElem->getTagName())) == 0) { // FIXME see getArc
      // end point
      OGRPoint *ptEnd = new OGRPoint();
      // point on the arc 
      OGRPoint *ptOnArc = new OGRPoint();
      // radius
      double radius = 0;
      
      DOMElement *arcElem = (DOMElement *)lineElem->getFirstChild();
      while (arcElem != NULL) {        
        if (cmpStr("C1", XMLString::transcode(arcElem->getTagName())) == 0)
          ptEnd->setX(atof(getObjValue(arcElem)));
        else if (cmpStr("C2", XMLString::transcode(arcElem->getTagName())) == 0)
          ptEnd->setY(atof(getObjValue(arcElem)));
        else if (cmpStr("C3", XMLString::transcode(arcElem->getTagName())) == 0)
          ptEnd->setZ(atof(getObjValue(arcElem)));
        else if (cmpStr("A1", XMLString::transcode(arcElem->getTagName())) == 0)
          ptOnArc->setX(atof(getObjValue(arcElem)));
        else if (cmpStr("A2", XMLString::transcode(arcElem->getTagName())) == 0)
          ptOnArc->setY(atof(getObjValue(arcElem)));
        else if (cmpStr("A3", XMLString::transcode(arcElem->getTagName())) == 0)
          ptOnArc->setZ(atof(getObjValue(arcElem)));
        else if (cmpStr("R", XMLString::transcode(arcElem->getTagName())) == 0)
          radius = atof(getObjValue(arcElem));
        
        arcElem = (DOMElement *)arcElem->getNextSibling();
      }
      
      if (fabs(radius) > 0) {
        OGRPoint *ptStart = getPoint((DOMElement *)lineElem->getPreviousSibling()); // COORD or ARC
        //r = sqrt((cx-ptx)*(cx-ptx)+(cy-pty)*(cy-pty));
        radius = fabs(radius);
        OGRPoint *center = getARCCenter(ptStart, ptOnArc, ptEnd);  
        ls->addPoint(getPointBetween(center, ptStart, ptOnArc));
        ls->addPoint(ptOnArc);
        ls->addPoint(getPointBetween(center, ptOnArc, ptEnd));
        ls->addPoint(ptEnd);
      } else {
        ls->addPoint(ptOnArc);
        ls->addPoint(ptEnd);
      }
    } /* else { // FIXME StructureValue in Polyline not yet supported
    } */
        
    lineElem = (DOMElement *)lineElem->getNextSibling();
  }
  
  return ls;
}

OGRLineString *getBoundary(DOMElement *elem) {
  
  DOMElement *lineElem = (DOMElement *)elem->getFirstChild();
  if (lineElem != NULL)
    if (cmpStr(ILI2_POLYLINE, XMLString::transcode(lineElem->getTagName())) == 0)
      return getLineString(lineElem);
  
  return new OGRLineString;
}

OGRPolygon *getPolygon(DOMElement *elem) {
  OGRPolygon *pg = new OGRPolygon();
  
  DOMElement *boundaryElem = (DOMElement *)elem->getFirstChild(); // outer boundary
  while (boundaryElem != NULL) {
    if (cmpStr(ILI2_BOUNDARY, XMLString::transcode(boundaryElem->getTagName())) == 0)
      pg->addRing((OGRLinearRing *)getBoundary(boundaryElem));
        
    boundaryElem = (DOMElement *)boundaryElem->getNextSibling(); // inner boundaries
  }
  
  return pg;
}

OGRGeometry *getGeometry(DOMElement *elem, int type) {
  OGRGeometryCollection *gm = new OGRGeometryCollection();
  
  DOMElement *childElem = (DOMElement *)elem->getFirstChild();
  while (childElem != NULL) {
    switch (type) {
      case ILI2_COORD_TYPE : 
        if (cmpStr(ILI2_COORD, XMLString::transcode(childElem->getTagName())) == 0)
          return getPoint(childElem);
        break;
      case ILI2_ARC_TYPE :
        // is it possible here? It have to be a ARC or COORD before (getPreviousSibling)
        if (cmpStr(ILI2_ARC, XMLString::transcode(childElem->getTagName())) == 0)
          return getArc(childElem);
        break;
      case ILI2_POLYLINE_TYPE :
        if (cmpStr(ILI2_POLYLINE, XMLString::transcode(childElem->getTagName())) == 0)
          return getLineString(childElem);
        break;
      case ILI2_BOUNDARY_TYPE :
        if (cmpStr(ILI2_BOUNDARY, XMLString::transcode(childElem->getTagName())) == 0)
          return getLineString(childElem);
        break;
      case ILI2_AREA_TYPE :
        if ((cmpStr(ILI2_AREA, XMLString::transcode(childElem->getTagName())) == 0) ||
          (cmpStr(ILI2_SURFACE, XMLString::transcode(childElem->getTagName())) == 0))
          return getPolygon(childElem);
        break;
      default : 
        if (type >= ILI2_GEOMCOLL_TYPE) {
          int subType = getTypeOfGeometry(childElem, false);
          gm->addGeometry(getGeometry(childElem, subType));
        }
        break;
    }
    
    // GEOMCOLL
    childElem = (DOMElement *)childElem->getNextSibling();
  }
  
  return gm;
}

void addFeature(OGRFeature *feature, DOMElement *elem, int type) {
  char *fName = XMLString::transcode(elem->getTagName());
  int fIndex = feature->GetFieldIndex(fName);
  
  if (type == 0) {  
    char * objVal = getObjValue(elem);
    if (objVal == NULL)
      objVal = getREFValue(elem); // only to try
    feature->SetField(fIndex, objVal);
  } else {
    feature->SetGeometry(getGeometry(elem, type));
  }
}

//
// ILI2Reader
//
IILI2Reader::~IILI2Reader() {
}

ILI2Reader::ILI2Reader() {
    m_poILI2Handler = NULL;
    m_poSAXReader = NULL;
    m_bReadStarted = FALSE;

    m_pszFilename = NULL;
    
    SetupParser();
}

ILI2Reader::~ILI2Reader() {
    CPLFree( m_pszFilename );

    CleanupParser();
}

void ILI2Reader::SetSourceFile( const char *pszFilename ) {
    CPLFree( m_pszFilename );
    m_pszFilename = CPLStrdup( pszFilename );
}

int ILI2Reader::SetupParser() {

    static int bXercesInitialized = FALSE;

    if( !bXercesInitialized )
    {
        try
        {
            XMLPlatformUtils::Initialize();
        }
        
        catch (const XMLException& toCatch)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
              "Unable to initalize Xerces C++ based ILI2 reader. Error message:\n%s\n", 
              toCatch.getMessage() );
            return FALSE;
        }
        bXercesInitialized = TRUE;
    }

    // Cleanup any old parser.
    if( m_poSAXReader != NULL )
        CleanupParser();

    // Create and initialize parser.
    m_poSAXReader = XMLReaderFactory::createXMLReader();
    
    m_poILI2Handler = new ILI2Handler( this );

    m_poSAXReader->setContentHandler( m_poILI2Handler );
    m_poSAXReader->setErrorHandler( m_poILI2Handler );
    m_poSAXReader->setLexicalHandler( m_poILI2Handler );
    m_poSAXReader->setEntityResolver( m_poILI2Handler );
    m_poSAXReader->setDTDHandler( m_poILI2Handler );

/* No Validation
#if (OGR_ILI2_VALIDATION)
    m_poSAXReader->setFeature(
        XMLString::transcode("http://xml.org/sax/features/validation"), true);
    m_poSAXReader->setFeature(
        XMLString::transcode("http://xml.org/sax/features/namespaces"), true);

    m_poSAXReader->setFeature( XMLUni::fgSAX2CoreNameSpaces, true );
    m_poSAXReader->setFeature( XMLUni::fgXercesSchema, true );

//    m_poSAXReader->setDoSchema(true);
//    m_poSAXReader->setValidationSchemaFullChecking(true);
#else
*/
    m_poSAXReader->setFeature(
        XMLString::transcode("http://xml.org/sax/features/validation"), false);
    m_poSAXReader->setFeature(
        XMLString::transcode("http://xml.org/sax/features/namespaces"), false);
//#endif

    m_bReadStarted = FALSE;

    return TRUE;
}

void ILI2Reader::CleanupParser() {
    if( m_poSAXReader == NULL )
        return;

    delete m_poSAXReader;
    m_poSAXReader = NULL;

    delete m_poILI2Handler;
    m_poILI2Handler = NULL;

    m_bReadStarted = FALSE;
}

int ILI2Reader::SaveClasses( const char *pszFile = NULL ) {

    // Add logic later to determine reasonable default schema file. 
    if( pszFile == NULL )
        return FALSE;
        
    // parse and create layers and features
    m_poSAXReader->parse(pszFile);

  if (m_missAttrs.size() != 0) {
    m_missAttrs.sort();
    m_missAttrs.unique();
    string attrs = "";  
    list<string>::const_iterator it = m_missAttrs.begin();
    for (it = m_missAttrs.begin(); it != m_missAttrs.end(); ++it)
      attrs += *it + ", ";
   
    CPLError( CE_Warning, CPLE_NotSupported, 
              "Failed to add new definition to existing layers, attributes not saved: %s", attrs.c_str() );
  }

    return TRUE;
}

list<OGRLayer *> ILI2Reader::GetLayers() {
  return m_listLayer;
}

int ILI2Reader::GetLayerCount() {
  return m_listLayer.size();
}

int ILI2Reader::AddFeature(DOMElement *elem) {

  // test if this is the first layer
  bool newLayer = (m_listLayer.size() == 0);
  bool newFDefn = false;
//  list<OGRLayer *>::const_reverse_iterator layerIt = m_listLayer.rbegin();
  list<OGRLayer *>::reverse_iterator layerIt = m_listLayer.rbegin();
  OGRLayer *curLayer;
  
  // new layer data
  char *pszName = XMLString::transcode(elem->getTagName());
  OGRSpatialReference *poSRSIn = NULL; // FIXME fix values for initial layer
  int bWriterIn = 0;
  OGRwkbGeometryType eReqType = wkbUnknown;
  OGRILI2DataSource *poDSIn = NULL;
  
  // test if this layer exist
  if (m_listLayer.size() > 0) { // FIXME can the layer be before the current layer ???
    curLayer = *layerIt;
    OGRFeatureDefn *fDef = curLayer->GetLayerDefn();
    newLayer = newLayer || (cmpStr(fDef->GetName(), XMLString::transcode(elem->getTagName())));
  }
  
  // add a layer
  if (newLayer) { // FIXME in Layer: SRS Writer Type datasource    
    OGRILI2Layer *poLayer = new OGRILI2Layer(CPLStrdup(pszName), poSRSIn, bWriterIn, eReqType, poDSIn);
    m_listLayer.push_back(poLayer);
  }
  
  //
  // define the features
  //
  
  // the feature and field definition
  curLayer = *layerIt;
  OGRFeatureDefn *featureDef = curLayer->GetLayerDefn();
  //OGRFeatureDefn *newFeatureDef = new OGRFeatureDefn(CPLStrdup(featureDef->GetName()));
  OGRFieldDefn *fieldDef;
  
  // type of the geometry
  int type = 0;
  
  // the tid
  if (featureDef->GetFieldIndex(ILI2_TID) == -1) {      
    // should only be true if the layer is not new
    newFDefn = true;
    
    fieldDef = new OGRFieldDefn(CPLStrdup(ILI2_TID), OFTString);
    
    if (newLayer)
      featureDef->AddFieldDefn(fieldDef);
    else
      m_missAttrs.push_back(ILI2_TID);
      //newFeatureDef->AddFieldDefn(fieldDef);
  } 
  
  // parse the element and create a feature
  DOMElement *childElem = (DOMElement*)elem->getFirstChild();
  while (childElem != NULL) {
    // attributes
    char *attr = XMLString::transcode(childElem->getTagName());
    type = getTypeOfGeometry(childElem, false); 
    
    if (featureDef->GetFieldIndex(attr) == -1) {     
      // should only be true if the layer is not new
      newFDefn = true;
      
      fieldDef = new OGRFieldDefn(CPLStrdup(attr), OFTString);
      
      if (newLayer)
        featureDef->AddFieldDefn(fieldDef);
      else
        m_missAttrs.push_back(attr);
        //newFeatureDef->AddFieldDefn(fieldDef);
    }
    
    childElem = (DOMElement *)childElem->getNextSibling();
  } 
  
  //
  // add the features
  //
  
  // the feature
  OGRFeature *feature;
  feature = new OGRFeature(featureDef);
  int fIndex = feature->GetFieldIndex(ILI2_TID);
  char *fChVal = XMLString::transcode(elem->getAttribute(XMLString::transcode(ILI2_TID)));
  
  /*
  if (!newLayer && newFDefn) {
    for (int i = 0; i < newFeatureDef->GetFieldCount(); i++) {
    CPLError( CE_Warning, CPLE_NotSupported, 
              "Failed to add new definition to an existing layer (TID: '%s'), attributes not saved: %s", 
              fChVal, newFeatureDef->GetFieldDefn(i)->GetNameRef() );
    }
  }
  */
  
  feature->SetField(fIndex, CPLStrdup(fChVal));
  
  // parse the element and create a feature
  childElem = (DOMElement*)elem->getFirstChild();
  while (childElem != NULL) {   
    
    type = getTypeOfGeometry(childElem, false); 
    addFeature(feature, childElem, type);
    
    childElem = (DOMElement*)childElem->getNextSibling();
  } 
  
  curLayer = *layerIt;
  curLayer->SetFeature(feature);
    
  return 0;
}

IILI2Reader *CreateILI2Reader() {
    return new ILI2Reader();
}
