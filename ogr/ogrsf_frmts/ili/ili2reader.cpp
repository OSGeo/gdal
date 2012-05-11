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
 ****************************************************************************/

#include "ogr_ili2.h"
#include "cpl_conv.h"
#include "cpl_string.h"

#include "ilihelper.h"
#include "iomhelper.h"
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
// helper functions
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
  while (i < tmpstr.length() && (tmpstr[i] == ' ' || tmpstr[i] == '\t' || tmpstr[i] == '\r' || tmpstr[i] == '\n')) ++i;
  return i > 0 ? tmpstr.substr(i, tmpstr.length()-i) : tmpstr;
}

string rtrim(string tmpstr) {
  if (tmpstr.length() == 0) return tmpstr;
  unsigned int i = tmpstr.length() - 1;
  while (i >= 0 && (tmpstr[i] == ' ' || tmpstr[i] == '\t' || tmpstr[i] == '\r' || tmpstr[i] == '\n')) --i;
  return i < tmpstr.length() - 1 ? tmpstr.substr(0, i+1) : tmpstr;
}

string trim(string tmpstr) {
  tmpstr = ltrim(tmpstr);
  tmpstr = rtrim(tmpstr);
  return tmpstr;
}

int getGeometryTypeOfElem(DOMElement* elem) {
  int type = ILI2_STRING_TYPE;
  char* pszTagName = XMLString::transcode(elem->getTagName());

  if (elem && elem->getNodeType() == DOMNode::ELEMENT_NODE) {
    if (cmpStr(ILI2_COORD, pszTagName) == 0) {
      type = ILI2_COORD_TYPE;
    } else if (cmpStr(ILI2_ARC, pszTagName) == 0) {
      type = ILI2_ARC_TYPE;
    } else if (cmpStr(ILI2_POLYLINE, pszTagName) == 0) {
      type = ILI2_POLYLINE_TYPE;
    } else if (cmpStr(ILI2_BOUNDARY, pszTagName) == 0) {
      type = ILI2_BOUNDARY_TYPE;
    } else if (cmpStr(ILI2_AREA, pszTagName) == 0) {
      type = ILI2_AREA_TYPE;
    } else if (cmpStr(ILI2_SURFACE, pszTagName) == 0) {
      type = ILI2_AREA_TYPE;
    }
  }
  XMLString::release(&pszTagName);
  return type;
}

char *getObjValue(DOMElement *elem) {
  DOMElement *textElem = (DOMElement *)elem->getFirstChild();

  if ((textElem != NULL) && (textElem->getNodeType() == DOMNode::TEXT_NODE))
  {
    char* pszNodeValue = XMLString::transcode(textElem->getNodeValue());
    char* pszRet = CPLStrdup(pszNodeValue);
    XMLString::release(&pszNodeValue);
    return pszRet;
  }

  return NULL;
}

char *getREFValue(DOMElement *elem) {
  XMLCh* pszIli2_ref = XMLString::transcode(ILI2_REF);
  char* pszREFValue = XMLString::transcode(elem->getAttribute(pszIli2_ref));
  char* pszRet = CPLStrdup(pszREFValue);
  XMLString::release(&pszIli2_ref);
  XMLString::release(&pszREFValue);
  return pszRet;
}

OGRPoint *getPoint(DOMElement *elem) {
  // elem -> COORD (or ARC)
  OGRPoint *pt = new OGRPoint();

  DOMElement *coordElem = (DOMElement *)elem->getFirstChild();
  while (coordElem != NULL) {
    char* pszTagName = XMLString::transcode(coordElem->getTagName());
    char* pszObjValue = getObjValue(coordElem);
    if (cmpStr("C1", pszTagName) == 0)
      pt->setX(atof(pszObjValue));
    else if (cmpStr("C2", pszTagName) == 0)
      pt->setY(atof(pszObjValue));
    else if (cmpStr("C3", pszTagName) == 0)
      pt->setZ(atof(pszObjValue));
    CPLFree(pszObjValue);
    XMLString::release(&pszTagName);
    coordElem = (DOMElement *)coordElem->getNextSibling();
  }
  pt->flattenTo2D();
  return pt;
}

OGRLineString *ILI2Reader::getArc(DOMElement *elem) {
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
    char* pszTagName = XMLString::transcode(arcElem->getTagName());
    char* pszObjValue = getObjValue(arcElem);
    if (cmpStr("C1", pszTagName) == 0)
      ptEnd->setX(atof(pszObjValue));
    else if (cmpStr("C2", pszTagName) == 0)
      ptEnd->setY(atof(pszObjValue));
    else if (cmpStr("C3", pszTagName) == 0)
      ptEnd->setZ(atof(pszObjValue));
    else if (cmpStr("A1", pszTagName) == 0)
      ptOnArc->setX(atof(pszObjValue));
    else if (cmpStr("A2", pszTagName) == 0)
      ptOnArc->setY(atof(pszObjValue));
    else if (cmpStr("A3", pszTagName) == 0)
      ptOnArc->setZ(atof(pszObjValue));
    else if (cmpStr("R", pszTagName) == 0)
      radius = atof(pszObjValue);
    CPLFree(pszObjValue);
    XMLString::release(&pszTagName);
    arcElem = (DOMElement *)arcElem->getNextSibling();
  }
  ptEnd->flattenTo2D();
  ptOnArc->flattenTo2D();
  interpolateArc(ls, ptStart, ptOnArc, ptEnd, arcIncr);
  delete ptStart;
  delete ptOnArc;
  delete ptEnd;
  return ls;
}

OGRLineString *getLineString(DOMElement *elem, int bAsLinearRing) {
  // elem -> POLYLINE
  OGRLineString *ls;
  if (bAsLinearRing)
      ls = new OGRLinearRing();
  else
      ls = new OGRLineString();

  DOMElement *lineElem = (DOMElement *)elem->getFirstChild();
  while (lineElem != NULL) {
    char* pszTagName = XMLString::transcode(lineElem->getTagName());
    if (cmpStr(ILI2_COORD, pszTagName) == 0)
    {
      OGRPoint* poPoint = getPoint(lineElem);
      ls->addPoint(poPoint);
      delete poPoint;
    }
    else if (cmpStr(ILI2_ARC, pszTagName) == 0) {
      // end point
      OGRPoint *ptEnd = new OGRPoint();
      // point on the arc
      OGRPoint *ptOnArc = new OGRPoint();
      // radius
      double radius = 0;

      DOMElement *arcElem = (DOMElement *)lineElem->getFirstChild();
      while (arcElem != NULL) {
        char* pszTagName = XMLString::transcode(arcElem->getTagName());
        char* pszObjValue = getObjValue(arcElem);
        if (cmpStr("C1", pszTagName) == 0)
          ptEnd->setX(atof(pszObjValue));
        else if (cmpStr("C2", pszTagName) == 0)
          ptEnd->setY(atof(pszObjValue));
        else if (cmpStr("C3", pszTagName) == 0)
          ptEnd->setZ(atof(pszObjValue));
        else if (cmpStr("A1", pszTagName) == 0)
          ptOnArc->setX(atof(pszObjValue));
        else if (cmpStr("A2", pszTagName) == 0)
          ptOnArc->setY(atof(pszObjValue));
        else if (cmpStr("A3", pszTagName) == 0)
          ptOnArc->setZ(atof(pszObjValue));
        else if (cmpStr("R", pszTagName) == 0)
          radius = atof(pszObjValue);
        CPLFree(pszObjValue);
        XMLString::release(&pszTagName);

        arcElem = (DOMElement *)arcElem->getNextSibling();
      }

      ptEnd->flattenTo2D();
      ptOnArc->flattenTo2D();
      OGRPoint *ptStart = getPoint((DOMElement *)lineElem->getPreviousSibling()); // COORD or ARC
      interpolateArc(ls, ptStart, ptOnArc, ptEnd, PI/180);

      delete ptStart;
      delete ptEnd;
      delete ptOnArc;
    } /* else { // FIXME StructureValue in Polyline not yet supported
    } */
    XMLString::release(&pszTagName);

    lineElem = (DOMElement *)lineElem->getNextSibling();
  }

  return ls;
}

OGRLinearRing *getBoundary(DOMElement *elem) {

  DOMElement *lineElem = (DOMElement *)elem->getFirstChild();
  if (lineElem != NULL)
  {
    char* pszTagName = XMLString::transcode(lineElem->getTagName());
    if (cmpStr(ILI2_POLYLINE, pszTagName) == 0)
    {
      XMLString::release(&pszTagName);
      return (OGRLinearRing*) getLineString(lineElem, TRUE);
    }
    XMLString::release(&pszTagName);
  }

  return new OGRLinearRing();
}

OGRPolygon *getPolygon(DOMElement *elem) {
  OGRPolygon *pg = new OGRPolygon();

  DOMElement *boundaryElem = (DOMElement *)elem->getFirstChild(); // outer boundary
  while (boundaryElem != NULL) {
    char* pszTagName = XMLString::transcode(boundaryElem->getTagName());
    if (cmpStr(ILI2_BOUNDARY, pszTagName) == 0)
      pg->addRingDirectly(getBoundary(boundaryElem));
    XMLString::release(&pszTagName);
    boundaryElem = (DOMElement *)boundaryElem->getNextSibling(); // inner boundaries
  }

  return pg;
}

OGRGeometry *ILI2Reader::getGeometry(DOMElement *elem, int type) {
  OGRGeometryCollection *gm = new OGRGeometryCollection();

  DOMElement *childElem = elem;
  while (childElem != NULL) {
    char* pszTagName = XMLString::transcode(childElem->getTagName());
    switch (type) {
      case ILI2_COORD_TYPE :
        if (cmpStr(ILI2_COORD, pszTagName) == 0)
        {
          delete gm;
          XMLString::release(&pszTagName);
          return getPoint(childElem);
        }
        break;
      case ILI2_ARC_TYPE :
        // is it possible here? It have to be a ARC or COORD before (getPreviousSibling)
        if (cmpStr(ILI2_ARC, pszTagName) == 0)
        {
          delete gm;
          XMLString::release(&pszTagName);
          return getArc(childElem);
        }
        break;
      case ILI2_POLYLINE_TYPE :
        if (cmpStr(ILI2_POLYLINE, pszTagName) == 0)
        {
          delete gm;
          XMLString::release(&pszTagName);
          return getLineString(childElem, FALSE);
        }
        break;
      case ILI2_BOUNDARY_TYPE :
        if (cmpStr(ILI2_BOUNDARY, pszTagName) == 0)
        {
          delete gm;
          XMLString::release(&pszTagName);
          return getLineString(childElem, FALSE);
        }
        break;
      case ILI2_AREA_TYPE :
        if ((cmpStr(ILI2_AREA, pszTagName) == 0) ||
          (cmpStr(ILI2_SURFACE, pszTagName) == 0))
        {
          delete gm;
          XMLString::release(&pszTagName);
          return getPolygon(childElem);
        }
        break;
      default :
        if (type >= ILI2_GEOMCOLL_TYPE) {
          int subType = getGeometryTypeOfElem(childElem); //????
          gm->addGeometryDirectly(getGeometry(childElem, subType));
        }
        break;
    }
    XMLString::release(&pszTagName);

    // GEOMCOLL
    childElem = (DOMElement *)childElem->getNextSibling();
  }

  return gm;
}

const char* ILI2Reader::GetLayerName(IOM_BASKET model, IOM_OBJECT table) {
    static char layername[512];
    IOM_OBJECT topic = GetAttrObj(model, table, "container");
    layername[0] = '\0';
    strcat(layername, iom_getattrvalue(GetAttrObj(model, topic, "container"), "name"));
    strcat(layername, ".");
    strcat(layername, iom_getattrvalue(topic, "name"));
    strcat(layername, ".");
    strcat(layername, iom_getattrvalue(table, "name"));
    return layername;
}

void ILI2Reader::AddField(OGRLayer* layer, IOM_BASKET model, IOM_OBJECT obj) {
  const char* typenam = "Reference";
  if (EQUAL(iom_getobjecttag(obj),"iom04.metamodel.LocalAttribute")) typenam = GetTypeName(model, obj);
  if (EQUAL(typenam, "iom04.metamodel.SurfaceType")) {
  } else if (EQUAL(typenam, "iom04.metamodel.AreaType")) {
  } else if (EQUAL(typenam, "iom04.metamodel.PolylineType") ) {
  } else if (EQUAL(typenam, "iom04.metamodel.CoordType")) {
  } else {
    OGRFieldDefn fieldDef(iom_getattrvalue(obj, "name"), OFTString);
    layer->GetLayerDefn()->AddFieldDefn(&fieldDef);
    CPLDebug( "OGR_ILI", "Field %s: %s", fieldDef.GetNameRef(), typenam);
  }
}

int ILI2Reader::ReadModel(char **modelFilenames) {

  IOM_BASKET model;
  IOM_ITERATOR modelelei;
  IOM_OBJECT modelele;

  iom_init();

  // set error listener to a iom provided one, that just
  // dumps all errors to stderr
  iom_seterrlistener(iom_stderrlistener);

  // compile ili models
  model=iom_compileIli(CSLCount(modelFilenames), modelFilenames);
  if(!model){
    CPLError( CE_Failure, CPLE_FileIO, "iom_compileIli failed." );
    iom_end();
    return FALSE;
  }

  // read tables
  modelelei=iom_iteratorobject(model);
  modelele=iom_nextobject(modelelei);
  while(modelele){
    const char *tag=iom_getobjecttag(modelele);
    if (tag && EQUAL(tag,"iom04.metamodel.Table")) {
      const char* topic = iom_getattrvalue(GetAttrObj(model, modelele, "container"), "name");
      if (!EQUAL(topic, "INTERLIS")) {
        const char* layername = GetLayerName(model, modelele);
        OGRLayer* layer = new OGRILI2Layer(layername, NULL, 0, wkbUnknown, NULL);
        m_listLayer.push_back(layer);
        CPLDebug( "OGR_ILI", "Reading table model '%s'", layername );

        // read fields
        IOM_OBJECT fields[255];
        IOM_OBJECT roledefs[255];
        memset(fields, 0, 255);
        memset(roledefs, 0, 255);
        int maxIdx = -1;
        IOM_ITERATOR fieldit=iom_iteratorobject(model);
        for (IOM_OBJECT fieldele=iom_nextobject(fieldit); fieldele; fieldele=iom_nextobject(fieldit)){
          const char *etag=iom_getobjecttag(fieldele);
          if (etag && (EQUAL(etag,"iom04.metamodel.ViewableAttributesAndRoles"))) {
            IOM_OBJECT table = GetAttrObj(model, fieldele, "viewable");
            if (table == modelele) {
              IOM_OBJECT obj = GetAttrObj(model, fieldele, "attributesAndRoles");
              int ili1AttrIdx = GetAttrObjPos(fieldele, "attributesAndRoles")-1;
              if (EQUAL(iom_getobjecttag(obj),"iom04.metamodel.RoleDef")) {
                //??ili1AttrIdx = atoi(iom_getattrvalue(GetAttrObj(model, obj, "oppend"), "ili1AttrIdx"));
                roledefs[ili1AttrIdx] = obj;
              } else {
                fields[ili1AttrIdx] = obj;
              }
              if (ili1AttrIdx > maxIdx) maxIdx = ili1AttrIdx;
              //CPLDebug( "OGR_ILI", "Field %s Pos: %d", iom_getattrvalue(obj, "name"), ili1AttrIdx);
            }
          }
          iom_releaseobject(fieldele);
        }
        iom_releaseiterator(fieldit);

        for (int i=0; i<=maxIdx; i++) {
          IOM_OBJECT obj = fields[i];
          IOM_OBJECT roleobj = roledefs[i];
          if (roleobj) AddField(layer, model, roleobj);
          if (obj) AddField(layer, model, obj);
        }
      }
    }
    iom_releaseobject(modelele);

    modelele=iom_nextobject(modelelei);
  }

  iom_releaseiterator(modelelei);

  iom_releasebasket(model);

  iom_end();

  return 0;
}

char* fieldName(DOMElement* elem) {
  string fullname;
  int depth = 0;
  DOMNode *node;
  for (node = elem; node; node = node->getParentNode()) ++depth;
  depth-=3; //ignore root elements

// We cannot do this sort of dynamic stack alloc on MSVC6.
//  DOMNode* elements[depth];
  DOMNode* elements[1000];
  CPLAssert( depth < (int)(sizeof(elements) / sizeof(DOMNode*)) );

  int d=0;
  for (node = elem; d<depth; node = node->getParentNode()) elements[d++] = node;
  for (d=depth-1; d>=0; --d) {
    if (d < depth-1) fullname += "_";
    char* pszNodeName = XMLString::transcode(elements[d]->getNodeName());
    fullname += pszNodeName;
    XMLString::release(&pszNodeName);
  }
  return CPLStrdup(fullname.c_str());
}

void ILI2Reader::setFieldDefn(OGRFeatureDefn *featureDef, DOMElement* elem) {
  int type = 0;
  //recursively search children
  for (DOMElement *childElem = (DOMElement *)elem->getFirstChild();
        type == 0 && childElem && childElem->getNodeType() == DOMNode::ELEMENT_NODE;
        childElem = (DOMElement*)childElem->getNextSibling()) {
    type = getGeometryTypeOfElem(childElem);
    if (type == 0) {
      if (childElem->getFirstChild() && childElem->getFirstChild()->getNodeType() == DOMNode::ELEMENT_NODE) {
        setFieldDefn(featureDef, childElem);
      } else {
        char *fName = fieldName(childElem);
        if (featureDef->GetFieldIndex(fName) == -1) {
          CPLDebug( "OGR_ILI", "AddFieldDefn: %s",fName );
          OGRFieldDefn oFieldDefn(fName, OFTString);
          featureDef->AddFieldDefn(&oFieldDefn);
        }
        CPLFree(fName);
      }
    }
  }
}

void ILI2Reader::SetFieldValues(OGRFeature *feature, DOMElement* elem) {
  int type = 0;
  //recursively search children
  for (DOMElement *childElem = (DOMElement *)elem->getFirstChild();
        type == 0 && childElem && childElem->getNodeType() == DOMNode::ELEMENT_NODE;
        childElem = (DOMElement*)childElem->getNextSibling()) {
    type = getGeometryTypeOfElem(childElem);
    if (type == 0) {
      if (childElem->getFirstChild() && childElem->getFirstChild()->getNodeType() == DOMNode::ELEMENT_NODE) {
        SetFieldValues(feature, childElem);
      } else {
        char *fName = fieldName(childElem);
        int fIndex = feature->GetFieldIndex(fName);
        if (fIndex != -1) {
          char * objVal = getObjValue(childElem);
          if (objVal == NULL)
            objVal = getREFValue(childElem); // only to try
          feature->SetField(fIndex, objVal);
          CPLFree(objVal);
        } else {
          CPLDebug( "OGR_ILI","Attribute '%s' not found", fName);
          m_missAttrs.push_back(fName);
        }
        CPLFree(fName);
      }
    } else {
      feature->SetGeometryDirectly(getGeometry(childElem, type));
    }
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

    list<OGRLayer *>::const_iterator layerIt = m_listLayer.begin();
    while (layerIt != m_listLayer.end()) {
        OGRILI2Layer *tmpLayer = (OGRILI2Layer *)*layerIt;
        delete tmpLayer;
        layerIt++;
    }
}

void ILI2Reader::SetArcDegrees(double arcDegrees) {
  arcIncr = arcDegrees*PI/180;
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
            char* msg = XMLString::transcode(toCatch.getMessage());
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Unable to initalize Xerces C++ based ILI2 reader. "
                      "Error message:\n%s\n", msg );
            XMLString::release(&msg);

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
    XMLCh *tmpCh = XMLString::transcode("http://xml.org/sax/features/validation");
    m_poSAXReader->setFeature(tmpCh, false);
    XMLString::release(&tmpCh);
    tmpCh = XMLString::transcode("http://xml.org/sax/features/namespaces");
    m_poSAXReader->setFeature(tmpCh, false);
    XMLString::release(&tmpCh);
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
    try
    {
        CPLDebug( "OGR_ILI", "Parsing %s", pszFile);
        m_poSAXReader->parse(pszFile);
    }
    catch (const SAXException& toCatch)
    {
        char* msg = XMLString::transcode(toCatch.getMessage());
        CPLError( CE_Failure, CPLE_AppDefined,
                    "Parsing failed: %s\n", msg );
        XMLString::release(&msg);

        return FALSE;
    }

  if (m_missAttrs.size() != 0) {
    m_missAttrs.sort();
    m_missAttrs.unique();
    string attrs = "";
    list<string>::const_iterator it;
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
  bool newLayer = true;
  OGRLayer *curLayer = 0;
  char *pszName = XMLString::transcode(elem->getTagName());

  // test if this layer exist
  for (list<OGRLayer *>::reverse_iterator layerIt = m_listLayer.rbegin();
       layerIt != m_listLayer.rend();
       ++layerIt) {
    OGRFeatureDefn *fDef = (*layerIt)->GetLayerDefn();
    if (cmpStr(fDef->GetName(), pszName) == 0) {
      newLayer = false;
      curLayer = *layerIt;
      break;
    }
  }

  // add a layer
  if (newLayer) { // FIXME in Layer: SRS Writer Type datasource
    CPLDebug( "OGR_ILI", "Adding layer: %s", pszName );
    // new layer data
    OGRSpatialReference *poSRSIn = NULL; // FIXME fix values for initial layer
    int bWriterIn = 0;
    OGRwkbGeometryType eReqType = wkbUnknown;
    OGRILI2DataSource *poDSIn = NULL;
    curLayer = new OGRILI2Layer(pszName, poSRSIn, bWriterIn, eReqType, poDSIn);
    m_listLayer.push_back(curLayer);
  }

  // the feature and field definition
  OGRFeatureDefn *featureDef = curLayer->GetLayerDefn();
  if (newLayer) {
    // add TID field
    OGRFieldDefn ofieldDefn (ILI2_TID, OFTString);
    featureDef->AddFieldDefn(&ofieldDefn);

    setFieldDefn(featureDef, elem);
  }

  // add the features
  OGRFeature *feature = new OGRFeature(featureDef);

  // assign TID
  int fIndex = feature->GetFieldIndex(ILI2_TID);
  if (fIndex != -1) {
      XMLCh *pszIli2_tid = XMLString::transcode(ILI2_TID);
      char *fChVal = XMLString::transcode(elem->getAttribute(pszIli2_tid));
      feature->SetField(fIndex, fChVal);
      XMLString::release (&pszIli2_tid);
      XMLString::release (&fChVal);
  } else {
      CPLDebug( "OGR_ILI","'%s' not found", ILI2_TID);
  }

  SetFieldValues(feature, elem);
  curLayer->SetFeature(feature);
  
  XMLString::release (&pszName);

  return 0;
}

IILI2Reader *CreateILI2Reader() {
    return new ILI2Reader();
}

void DestroyILI2Reader(IILI2Reader* reader)
{
    if (reader)
        delete reader;
}
