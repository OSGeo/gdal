/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 1 Reader
 * Purpose:  Implementation of ILI1Reader class.
 * Author:   Pirmin Kalberer, Sourcepole AG <pi@sourcepole.com>
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG <pi@sourcepole.com>
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
 * Revision 1.1  2005/07/08 22:10:57  pka
 * Initial import of OGR Interlis driver
 *
 */

#include "ogr_ili1.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_api.h"

#include "iomhelper.h"
#include "ili1reader.h"
#include "ili1readerp.h"

CPL_CVSID("$Id$");


//
// ILI1Reader
//
IILI1Reader::~IILI1Reader() {
}

ILI1Reader::ILI1Reader() {
  fpItf = NULL;
  nLayers = 0;
  papoLayers = NULL;
}

ILI1Reader::~ILI1Reader() {
 if (fpItf) VSIFClose( fpItf );
}

/* -------------------------------------------------------------------- */
/*      Open the source file.                                           */
/* -------------------------------------------------------------------- */
int ILI1Reader::OpenFile( const char *pszFilename ) {
    fpItf = VSIFOpen( pszFilename, "r" );
    if( fpItf == NULL )
    {
          CPLError( CE_Failure, CPLE_OpenFailed, 
                    "Failed to open ILI file `%s'.", 
                    pszFilename );

        return FALSE;
    }
    return TRUE;
}


const char* ILI1Reader::GetLayerNameString(const char* topicname, const char* tablename) {
    static char layername[512];
    layername[0] = '\0';
    strcat(layername, topicname);
    strcat(layername, "__");
    strcat(layername, tablename);
    return layername;
}

const char* ILI1Reader::GetLayerName(IOM_BASKET model, IOM_OBJECT table) {
    static char layername[512];
    IOM_OBJECT topic = GetAttrObj(model, table, "container");
    layername[0] = '\0';
    strcat(layername, iom_getattrvalue(topic, "name"));
    strcat(layername, "__");
    strcat(layername, iom_getattrvalue(table, "name"));
    return layername;
}

void ILI1Reader::AddCoord(OGRLayer* layer, IOM_BASKET model, IOM_OBJECT modelele, IOM_OBJECT typeobj) {
  unsigned int dim = ::GetCoordDim(model, typeobj);
  for (unsigned int i=0; i<dim; i++) {
    OGRFieldDefn fieldDef(CPLSPrintf("%s_%d", iom_getattrvalue(modelele, "name"), i), OFTReal);
    layer->GetLayerDefn()->AddFieldDefn(&fieldDef);
    CPLDebug( "OGR_ILI", "Field %s (Table %s): %s", fieldDef.GetNameRef(), layer->GetLayerDefn()->GetName(), iom_getobjecttag(typeobj));
  }
}

void ILI1Reader::AddPolyTable(OGRLayer* layer, IOM_OBJECT obj) {
  static char layername[512];
  layername[0] = '\0';
  strcat(layername, layer->GetLayerDefn()->GetName());
  strcat(layername, "_");
  strcat(layername, iom_getattrvalue(obj, "name"));

  OGRLayer* geomlayer = new OGRILI1Layer(layername, NULL, 0, wkbPolygon, NULL);
  AddLayer(geomlayer);
  OGRFieldDefn fieldDef(layer->GetLayerDefn()->GetName(), OFTString); //"__Ident"
  geomlayer->GetLayerDefn()->AddFieldDefn(&fieldDef);
  OGRFieldDefn fieldDef2("ILI_Geometry", OFTString);
  geomlayer->GetLayerDefn()->AddFieldDefn(&fieldDef2);
}

void ILI1Reader::AddField(OGRLayer* layer, IOM_BASKET model, IOM_OBJECT obj) {
  const char* typenam = "Reference";
  if (EQUAL(iom_getobjecttag(obj),"iom04.metamodel.LocalAttribute")) typenam = GetTypeName(model, obj);
  if (EQUAL(typenam, "iom04.metamodel.SurfaceType")) {
    AddPolyTable(layer, obj);
  } else if (EQUAL(typenam, "iom04.metamodel.AreaType")) {
    IOM_OBJECT controlPointDomain = GetAttrObj(model, GetTypeObj(model, obj), "controlPointDomain");
    if (controlPointDomain) {
      AddCoord(layer, model, obj, GetTypeObj(model, controlPointDomain));
    }
    layer->GetLayerDefn()->SetGeomType(wkbMultiLineString);
    //polygonize: AddPolyTable(layer, obj);
  } else if (EQUAL(typenam, "iom04.metamodel.PolylineType") ) {
    layer->GetLayerDefn()->SetGeomType(wkbMultiLineString);
  } else if (EQUAL(typenam, "iom04.metamodel.CoordType")) {
    AddCoord(layer, model, obj, GetTypeObj(model, obj));
  } else {
    OGRFieldDefn fieldDef(iom_getattrvalue(obj, "name"), OFTString);
    layer->GetLayerDefn()->AddFieldDefn(&fieldDef);
    CPLDebug( "OGR_ILI", "Field %s (Table %s): %s", fieldDef.GetNameRef(), layer->GetLayerDefn()->GetName(), typenam);
  }
}

int ILI1Reader::ReadModel(const char *pszModelFilename) {

  IOM_BASKET model;
  IOM_ITERATOR modelelei;
  IOM_OBJECT modelele;

  iom_init();

  // set error listener to a iom provided one, that just 
  // dumps all errors to stderr
  iom_seterrlistener(iom_stderrlistener);

  // compile ili model
  char *iomarr[1] = {(char *)pszModelFilename};
  model=iom_compileIli(1, iomarr);
  if(!model){
    fprintf(stderr,"iom_compileIli() failed\n");
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
        OGRSpatialReference *poSRSIn = NULL;
        int bWriterIn = 0;
        OGRwkbGeometryType eReqType = wkbUnknown;
        OGRILI1DataSource *poDSIn = NULL;
        OGRLayer* layer = new OGRILI1Layer(layername, poSRSIn, bWriterIn, eReqType, poDSIn);
        AddLayer(layer);
        CPLDebug( "OGR_ILI", "Reading table model (%s).", layername );

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
                int ili1AttrIdx = atoi(iom_getattrvalue(GetAttrObj(model, obj, "oppend"), "ili1AttrIdx"));
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

        OGRFieldDefn fieldDef("__Ident", OFTString);
        layer->GetLayerDefn()->AddFieldDefn(&fieldDef);
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

int ILI1Reader::ReadFeatures() {
    char **tokens = NULL;
    const char *firsttok = NULL;
    const char *pszLine;
    const char *topic;
    int ret = TRUE;
    
    while (ret && (tokens = ReadParseLine()))
    {
      firsttok = tokens[0];
      if (EQUAL(firsttok, "SCNT"))
      {
        //read description
        do 
        {
          pszLine = CPLReadLine( fpItf );
        }
        while (pszLine && !EQUALN(pszLine, "////", 4));
        ret = (pszLine != NULL);
      }
      else if (EQUAL(firsttok, "MOTR"))
      {
        //read model
        do 
        {
          pszLine = CPLReadLine( fpItf );
        }
        while (pszLine && !EQUALN(pszLine, "////", 4));
        ret = (pszLine != NULL);
      }
      else if (EQUAL(firsttok, "MTID"))
      {
      }
      else if (EQUAL(firsttok, "MODL"))
      {
      }
      else if (EQUAL(firsttok, "TOPI"))
      {
        topic = CPLStrdup(CSLGetField(tokens, 1));
      }
      else if (EQUAL(firsttok, "TABL"))
      {
        CPLDebug( "OGR_ILI", "Reading table (%s).", CSLGetField(tokens, 1) );
        curLayer = (OGRILI1Layer*)GetLayerByName(GetLayerNameString(topic, CSLGetField(tokens, 1)));
        if (curLayer == NULL) { //create one
          OGRSpatialReference *poSRSIn = NULL;
          int bWriterIn = 0;
          OGRwkbGeometryType eReqType = wkbUnknown;
          OGRILI1DataSource *poDSIn = NULL;
          curLayer = new OGRILI1Layer(GetLayerNameString(topic, CSLGetField(tokens, 1)), poSRSIn, bWriterIn, eReqType, poDSIn);
          AddLayer(curLayer);
        }
        for (int i=0; i < curLayer->GetLayerDefn()->GetFieldCount(); i++) {
          CPLDebug( "OGR_ILI", "Field %d: %s", i,  curLayer->GetLayerDefn()->GetFieldDefn(i)->GetNameRef());
        }
        ret = ReadTable();
      }
      else if (EQUAL(firsttok, "ETOP"))
      {
      }
      else if (EQUAL(firsttok, "EMOD"))
      {
      }
      else if (EQUAL(firsttok, "ENDE"))
      {
        return TRUE;
      }
      else
      {
        CPLDebug( "OGR_ILI", "Unexpected token: %s", firsttok );
      }
      
      CSLDestroy(tokens);
    }
    return ret;
}

int ILI1Reader::AddIliGeom(OGRFeature *feature, int iField, long fpos)
{
    long nBlockLen = VSIFTell( fpItf )-fpos;
    VSIFSeek( fpItf, fpos, SEEK_SET );

    char *pszRawData = (char *) CPLMalloc(nBlockLen+1);
    if( (int) VSIFRead( pszRawData, 1, nBlockLen, fpItf ) != nBlockLen )
    {
        CPLFree( pszRawData );

        CPLError( CE_Failure, CPLE_FileIO, "Read of transfer file failed." );
        return FALSE;
    }
    pszRawData[nBlockLen]= '\0';
    feature->SetField(iField, pszRawData);
    return TRUE;
}


int ILI1Reader::ReadTable() {
    
    char **tokens = NULL;
    const char *firsttok = NULL;
    int ret = TRUE;
    int warned = FALSE;
    int fIndex;

    OGRFeatureDefn *featureDef = curLayer->GetLayerDefn();
    OGRFieldDefn *fieldDef = NULL;
    OGRFeature *feature = NULL;
    OGRGeometry *ogrGeom = NULL;

    long fpos = VSIFTell(fpItf);

    if (featureDef->GetGeomType() == wkbUnknown)
    {
      //Read all features as line collection
      //ogrGeom = new OGRGeometryCollection();
    }
    while (ret && (tokens = ReadParseLine()))
    {
      firsttok = CSLGetField(tokens, 0);
      if (EQUAL(firsttok, "OBJE"))
      {
        if (featureDef->GetFieldCount() == 0)
        {
          CPLDebug( "OGR_ILI", "No field definition found for table: %s", featureDef->GetName() );
          //Model not read - use heuristics
          for (fIndex=1; fIndex<CSLCount(tokens); fIndex++)
          {
            fieldDef = new OGRFieldDefn(CPLStrdup("Field0"), OFTString);
            *(char *)(fieldDef->GetNameRef()+strlen(fieldDef->GetNameRef())-1) = '0'+fIndex;
            featureDef->AddFieldDefn(fieldDef);
          }
        }
        feature = new OGRFeature(featureDef);
        int fieldno = 0;
        for (fIndex=1; fIndex<CSLCount(tokens) && fieldno < featureDef->GetFieldCount(); fIndex++, fieldno++)
        {
          if (!EQUAL(tokens[fIndex], "@")) {
            //CPLDebug( "OGR_ILI", "Adding Field %d: %s", fieldno, tokens[fIndex]);
            feature->SetField(fieldno, CPLStrdup(tokens[fIndex]));
            if (featureDef->GetFieldDefn(fieldno)->GetType() == OFTReal
                && fieldno > 0
                && featureDef->GetFieldDefn(fieldno-1)->GetType() == OFTReal
                && featureDef->GetGeomType() == wkbUnknown) {
              //add Point geometry
              OGRPoint *ogrPoint = new OGRPoint(atof(tokens[fIndex-1]), atof(tokens[fIndex]));
              feature->SetGeometry(ogrPoint);
            }
          }
        }
        if (!warned && featureDef->GetFieldCount() != CSLCount(tokens)-1) {
          CPLDebug( "OGR_ILI", "Field count doesn't match. %d declared, %d found", featureDef->GetFieldCount(), CSLCount(tokens)-1);
          warned = TRUE;
        }
        curLayer->AddFeature(feature);
      }
      else if (EQUAL(firsttok, "STPT"))
      {
        OGRGeometry *geom = ReadGeom(tokens);
        //CPLDebug( "OGR_ILI", "Geometry: %s", ogrGeom->exportToGML() );

        if (ogrGeom && ogrGeom->getGeometryType() == wkbGeometryCollection)
        {
          ((OGRGeometryCollection *)ogrGeom)->addGeometry(geom);
        }
        else if (featureDef->GetGeomType() == wkbPolygon)
        {
          OGRPolygon *gc = new OGRPolygon();
          gc->addRing((OGRLinearRing *)geom);
          feature->SetGeometry(gc);
        }
        else
        {
          feature->SetGeometry(geom);
        }
        if (EQUAL(featureDef->GetFieldDefn(featureDef->GetFieldCount()-1)->GetNameRef(), "ILI_Geometry"))
        {
          AddIliGeom(feature, featureDef->GetFieldCount()-1, fpos);
        }

      }
      else if (EQUAL(firsttok, "ELIN"))
      {
        //empty geom
      }
      else if (EQUAL(firsttok, "EDGE"))
      {
        OGRGeometry *geom = ReadGeom(tokens);
        feature->SetGeometry(geom);
        if (EQUAL(featureDef->GetFieldDefn(featureDef->GetFieldCount()-1)->GetNameRef(), "ILI_Geometry"))
        {
          AddIliGeom(feature, featureDef->GetFieldCount()-1, fpos);
        }
      }
      else if (EQUAL(firsttok, "PERI"))
      {
      }
      else if (EQUAL(firsttok, "ETAB"))
      {
        if (ogrGeom && ogrGeom->getGeometryType() == wkbGeometryCollection)
        { //polygonze AREA lines
          if (((OGRGeometryCollection *)ogrGeom)->getNumGeometries() > 0)
          {
            OGRErr eErr;
            ogrGeom = (OGRPolygon *)OGRBuildPolygonFromEdges( ogrGeom,
                                    TRUE, FALSE, 0.0, &eErr );
            if( eErr != OGRERR_NONE )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Polygon assembly has failed for feature %s.\n"
                          "Geometry may be missing or incomplete.", 
                          featureDef->GetName() );
            }
            else
            {
              feature->SetGeometry(ogrGeom);
            }
          }
        }
        return TRUE;
      }
      else
      {
        CPLDebug( "OGR_ILI", "Unexpected token: %s", firsttok );
      }
      
      CSLDestroy(tokens);
      fpos = VSIFTell(fpItf);
    }
    
    return ret;
}

OGRGeometry *ILI1Reader::ReadGeom(char **stgeom) {
    
    char **tokens = NULL;
    const char *firsttok = NULL;
    int end = FALSE;
    OGRGeometry *ogrGeom = NULL;
    OGRLineString *ogrLine = NULL; //current line
    OGRMultiLineString *ogrMultiLine = NULL; //current multi line
    OGRPoint *ogrPoint = NULL;
    
    if (EQUAL(stgeom[0], "STPT"))
    {
      ogrLine = new OGRLineString();
      ogrPoint = new OGRPoint(atof(stgeom[1]), atof(stgeom[2]));
      ogrLine->addPoint(ogrPoint);
      ogrGeom = ogrLine;
    }
    else
    {
      ogrMultiLine = new OGRMultiLineString();
      ogrGeom = ogrMultiLine;
    }
    
    while (!end && (tokens = ReadParseLine()))
    {
      firsttok = CSLGetField(tokens, 0);
      if (EQUAL(firsttok, "LIPT"))
      {
        ogrPoint = new OGRPoint(atof(tokens[1]), atof(tokens[2]));
        ogrLine->addPoint(ogrPoint);
      }
      else if (EQUAL(firsttok, "ARCP"))
      {
        ogrPoint = new OGRPoint(atof(tokens[1]), atof(tokens[2]));
        ogrLine->addPoint(ogrPoint);
      }
      else if (EQUAL(firsttok, "ELIN"))
      {
        end = TRUE;
      }
      else if (EQUAL(firsttok, "STPT"))
      {
        if (ogrMultiLine) //within EDGE
        {
          ogrLine = (OGRLineString*)ReadGeom(tokens);
          ogrMultiLine->addGeometry(ogrLine);
        }
        else if (ogrLine) //AREA lines spread into mutltiple objects
        {
        } //else ignore error
      }
      else if (EQUAL(firsttok, "EEDG"))
      {
        end = TRUE;
      }
      else if (EQUAL(firsttok, "LATT"))
      {
        //Line Attributes (ignored)
      }
      else if (EQUAL(firsttok, "EFLA"))
      {
        end = TRUE;
      }
      else if (EQUAL(firsttok, "OBJE"))
      {
        //AREA lines spread into mutltiple objects
      }
      else
      {
        CPLDebug( "OGR_ILI", "Unexpected token: %s", firsttok );
      }
    
      CSLDestroy(tokens);
    }
    
    return ogrGeom;
}

/************************************************************************/
/*                              AddLayer()                              */
/************************************************************************/

void ILI1Reader::AddLayer( OGRLayer * poNewLayer )

{
    papoLayers = (OGRLayer **)
        CPLRealloc( papoLayers, sizeof(void*) * ++nLayers );
    
    papoLayers[nLayers-1] = poNewLayer;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *ILI1Reader::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

OGRLayer *ILI1Reader::GetLayerByName( const char* pszLayerName )

{
    for(int iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(pszLayerName,
                  papoLayers[iLayer]->GetLayerDefn()->GetName()) )
            return papoLayers[iLayer];
    }
    return NULL;
}


/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

int ILI1Reader::GetLayerCount()

{
    return nLayers;
}

/************************************************************************/
/*     Read one logical line, and return split into fields.  The return */
/*     result is a stringlist, in the sense of the CSL functions.       */
/************************************************************************/

char ** ILI1Reader::ReadParseLine()
{
    const char  *pszLine;
    char **tokens;
    char **conttok;
    char *token;

    CPLAssert( fpItf != NULL );
    if( fpItf == NULL )
        return( NULL );
    
    pszLine = CPLReadLine( fpItf );
    if( pszLine == NULL )
        return( NULL );
    
    if (strlen(pszLine) == 0) return NULL;
      
    tokens = CSLTokenizeString2( pszLine, " ", CSLT_PRESERVEESCAPES );
    token = tokens[CSLCount(tokens)-1];
    
    //Append CONT lines
    while (strlen(pszLine) && EQUALN(token, "\\", 2))
    {
       //remove last token
      CPLFree(tokens[CSLCount(tokens)-1]);
      tokens[CSLCount(tokens)-1] = NULL;
      
      pszLine = CPLReadLine( fpItf );
      conttok = CSLTokenizeString2( pszLine, " ", CSLT_PRESERVEESCAPES );
      if (!conttok || !EQUAL(conttok[0], "CONT")) break;
      
      //append
      tokens = CSLInsertStrings(tokens, -1, &conttok[1]);
      token = tokens[CSLCount(tokens)-1];
      
      CSLDestroy(conttok);
    }
    return tokens;
}



IILI1Reader *CreateILI1Reader() {
    return new ILI1Reader();
}
