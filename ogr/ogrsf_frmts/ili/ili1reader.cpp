/******************************************************************************
 * $Id$
 *
 * Project:  Interlis 1 Reader
 * Purpose:  Implementation of ILI1Reader class.
 * Author:   Pirmin Kalberer, Sourcepole AG
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

#include "ogr_ili1.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_api.h"
#include "ogr_geos.h"

#include "ilihelper.h"
#include "iomhelper.h"
#include "ili1reader.h"
#include "ili1readerp.h"

#include <vector>

#ifdef HAVE_GEOS
#  define POLYGONIZE_AREAS
#endif

#ifndef POLYGONIZE_AREAS
#  if defined(__GNUC_PREREQ)
#    warning Interlis 1 Area polygonizing disabled. Needs GEOS >= 2.1.0
#  endif
#endif

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
  curLayer = NULL;
  metaLayer = NULL;
  codeBlank = '_';
  codeUndefined = '@';
  codeContinue = '\\';
  SetArcDegrees(1);

}

ILI1Reader::~ILI1Reader() {
 int i;
 if (fpItf) VSIFClose( fpItf );

 for(i=0;i<nLayers;i++)
     delete papoLayers[i];
 CPLFree(papoLayers);

 delete metaLayer;
}

void ILI1Reader::SetArcDegrees(double arcDegrees) {
  arcIncr = arcDegrees*PI/180;
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

int ILI1Reader::HasMultiplePointGeom(const char* layername) {
    if (metaLayer != NULL) {
        OGRFeature *metaFeature = NULL;
        metaLayer->ResetReading();
        int i = -1;
        while((metaFeature = metaLayer->GetNextFeature()) != NULL ) {
            if(EQUAL(layername, metaFeature->GetFieldAsString(0))) {
              i++;
            }
            delete metaFeature;
        }
        return i;
    } else {
        return -1;
    }
}

char* ILI1Reader::GetPointLayerName(const char* layername, char* newlayername) {
    static char geomlayername[512];
    geomlayername[0] = '\0';
    strcat(geomlayername, layername);
    strcat(geomlayername, "__");
    strcat(geomlayername, newlayername);
    return geomlayername;
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

void ILI1Reader::AddCoord(OGRILI1Layer* layer, IOM_BASKET model, IOM_OBJECT modelele, IOM_OBJECT typeobj) {
  unsigned int dim = ::GetCoordDim(model, typeobj);
  for (unsigned int i=0; i<dim; i++) {
    OGRFieldDefn fieldDef(CPLSPrintf("%s_%d", iom_getattrvalue(modelele, "name"), i), OFTReal);
    layer->GetLayerDefn()->AddFieldDefn(&fieldDef);
    //CPLDebug( "AddCoord   OGR_ILI", "Field %s: OFTReal", fieldDef.GetNameRef());
  }
}

OGRILI1Layer* ILI1Reader::AddGeomTable(const char* datalayername, const char* geomname, OGRwkbGeometryType eType) {
  static char layername[512];
  layername[0] = '\0';
  strcat(layername, datalayername);
  strcat(layername, "_");
  strcat(layername, geomname);

  OGRILI1Layer* geomlayer = new OGRILI1Layer(layername, NULL, 0, eType, NULL);
  AddLayer(geomlayer);
  OGRFieldDefn fieldDef("_TID", OFTString);
  geomlayer->GetLayerDefn()->AddFieldDefn(&fieldDef);
  if (eType == wkbPolygon)
  {
     OGRFieldDefn fieldDefRef("_RefTID", OFTString);
     geomlayer->GetLayerDefn()->AddFieldDefn(&fieldDefRef);
  }
  OGRFieldDefn fieldDef2("ILI_Geometry", OFTString); //in write mode only?
  geomlayer->GetLayerDefn()->AddFieldDefn(&fieldDef2);
  return geomlayer;
}

void ILI1Reader::AddField(OGRILI1Layer* layer, IOM_BASKET model, IOM_OBJECT obj) {
  const char* typenam = "Reference";
  if (EQUAL(iom_getobjecttag(obj),"iom04.metamodel.LocalAttribute")) typenam = GetTypeName(model, obj);
  //CPLDebug( "OGR_ILI", "Field %s: %s", iom_getattrvalue(obj, "name"), typenam);
  if (EQUAL(typenam, "iom04.metamodel.SurfaceType")) {
    OGRILI1Layer* polyLayer = AddGeomTable(layer->GetLayerDefn()->GetName(), iom_getattrvalue(obj, "name"), wkbPolygon);
    layer->SetSurfacePolyLayer(polyLayer);
    //TODO: add line attributes to geometry
  } else if (EQUAL(typenam, "iom04.metamodel.AreaType")) {
    IOM_OBJECT controlPointDomain = GetAttrObj(model, GetTypeObj(model, obj), "controlPointDomain");
    if (controlPointDomain) {
      AddCoord(layer, model, obj, GetTypeObj(model, controlPointDomain));
      layer->GetLayerDefn()->SetGeomType(wkbPoint);
    }
    OGRILI1Layer* areaLineLayer = AddGeomTable(layer->GetLayerDefn()->GetName(), iom_getattrvalue(obj, "name"), wkbMultiLineString);
#ifdef POLYGONIZE_AREAS
    OGRILI1Layer* areaLayer = new OGRILI1Layer(CPLSPrintf("%s__Areas",layer->GetLayerDefn()->GetName()), NULL, 0, wkbPolygon, NULL);
    AddLayer(areaLayer);
    areaLayer->SetAreaLayers(layer, areaLineLayer);
#endif
  } else if (EQUAL(typenam, "iom04.metamodel.PolylineType") ) {
    layer->GetLayerDefn()->SetGeomType(wkbMultiLineString);
  } else if (EQUAL(typenam, "iom04.metamodel.CoordType")) {
    AddCoord(layer, model, obj, GetTypeObj(model, obj));
    if (layer->GetLayerDefn()->GetGeomType() == wkbUnknown) layer->GetLayerDefn()->SetGeomType(wkbPoint);
  } else if (EQUAL(typenam, "iom04.metamodel.NumericType") ) {
     OGRFieldDefn fieldDef(iom_getattrvalue(obj, "name"), OFTReal);
     layer->GetLayerDefn()->AddFieldDefn(&fieldDef);
  } else if (EQUAL(typenam, "iom04.metamodel.EnumerationType") ) {
     OGRFieldDefn fieldDef(iom_getattrvalue(obj, "name"), OFTInteger);
     layer->GetLayerDefn()->AddFieldDefn(&fieldDef);
  } else {
    OGRFieldDefn fieldDef(iom_getattrvalue(obj, "name"), OFTString);
    layer->GetLayerDefn()->AddFieldDefn(&fieldDef);
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
    CPLError( CE_Failure, CPLE_FileIO, "iom_compileIli failed." );
    iom_end();
    return FALSE;
  }

  // create new layer with meta information (ILI table name and geometry column index)
  // while reading the features from the ITF we have to know which column is the geometry column
  metaLayer = new OGRILI1Layer("Metatable", NULL, 0, wkbUnknown, NULL);
  OGRFieldDefn fieldDef1("layername", OFTString);
  metaLayer->GetLayerDefn()->AddFieldDefn(&fieldDef1);
  OGRFieldDefn fieldDef2("geomIdx", OFTInteger);
  metaLayer->GetLayerDefn()->AddFieldDefn(&fieldDef2);
  OGRFieldDefn fieldDef3("geomlayername", OFTString);
  metaLayer->GetLayerDefn()->AddFieldDefn(&fieldDef3);


  // read tables
  int j = 0;
  modelelei=iom_iteratorobject(model);
  modelele=iom_nextobject(modelelei);
  while(modelele){
    const char *tag=iom_getobjecttag(modelele);

    if (tag) {
      if (EQUAL(tag,"iom04.metamodel.Table")) {

        const char* topic = iom_getattrvalue(GetAttrObj(model, modelele, "container"), "name");

        if (!EQUAL(topic, "INTERLIS")) {

          const char* layername = GetLayerName(model, modelele);
          OGRSpatialReference *poSRSIn = NULL;
          int bWriterIn = 0;
          OGRwkbGeometryType eReqType = wkbUnknown;
          OGRILI1DataSource *poDSIn = NULL;

          CPLDebug( "OGR_ILI", "Reading table model '%s'", layername );

          // read fields
          IOM_OBJECT fields[255];
          IOM_OBJECT roledefs[255];
          memset(fields, 0, 255);
          memset(roledefs, 0, 255);
          int maxIdx = -1;
          IOM_ITERATOR fieldit=iom_iteratorobject(model);
          std::vector<IOM_OBJECT> attributes;

          for (IOM_OBJECT fieldele=iom_nextobject(fieldit); fieldele; fieldele=iom_nextobject(fieldit)){
            const char *etag=iom_getobjecttag(fieldele);

            if (etag && (EQUAL(etag,"iom04.metamodel.ViewableAttributesAndRoles"))) {
              IOM_OBJECT table = GetAttrObj(model, fieldele, "viewable");

              if (table == modelele) {

                IOM_OBJECT obj = GetAttrObj(model, fieldele, "attributesAndRoles");
                int ili1AttrIdx = GetAttrObjPos(fieldele, "attributesAndRoles")-1;

                if (EQUAL(iom_getobjecttag(obj),"iom04.metamodel.RoleDef")) {
                  int ili1AttrIdxOppend = atoi(iom_getattrvalue(GetAttrObj(model, obj, "oppend"), "ili1AttrIdx"));

                  if (ili1AttrIdxOppend>=0) {
                    roledefs[ili1AttrIdxOppend] = obj;
                    if (ili1AttrIdxOppend > maxIdx) maxIdx = ili1AttrIdxOppend;
                  }
                } else {
                  fields[ili1AttrIdx] = obj;
                  if (ili1AttrIdx > maxIdx) maxIdx = ili1AttrIdx;
                }
              }
            }
            iom_releaseobject(fieldele);
          }
          iom_releaseiterator(fieldit);

          // if multiple gets positive we have more than one geometry column (only points)
          int multiple = -1;

          for (int i=0; i<=maxIdx; i++) {
            IOM_OBJECT obj = fields[i];
            if (obj) {
             attributes.push_back(obj);
             if (EQUAL(GetTypeName(model, obj), "iom04.metamodel.CoordType")) multiple++;
            }
          }

          std::vector<IOM_OBJECT>::iterator it = attributes.begin();
          for (int i=0; i<=maxIdx; i++) {
            IOM_OBJECT obj = roledefs[i];
            if (obj) attributes.insert(attributes.begin() + i, obj);
          }

          OGRFeature *feature = NULL;
          char* geomlayername = '\0';
          OGRILI1Layer* layer = NULL;

          for(size_t i=0; i<attributes.size(); i++) {
            IOM_OBJECT obj = attributes.at(i);
            const char* typenam = GetTypeName(model, obj);
            if (EQUAL(typenam, "iom04.metamodel.CoordType")  || EQUAL(typenam, "iom04.metamodel.AreaType")) {
              feature = OGRFeature::CreateFeature(metaLayer->GetLayerDefn());
              feature->SetFID(j+1);
              feature->SetField("layername", layername);
              feature->SetField("geomIdx", (int)i);

              if(multiple > 0) {
                geomlayername = GetPointLayerName(layername, iom_getattrvalue(obj, "name"));
                feature->SetField("geomlayername", geomlayername);
                layer = new OGRILI1Layer(geomlayername, poSRSIn, bWriterIn, eReqType, poDSIn);
                AddLayer(layer);

              } else {
                feature->SetField("geomlayername", layername);
                layer = new OGRILI1Layer(layername, poSRSIn, bWriterIn, eReqType, poDSIn);
                AddLayer(layer);
              }
              metaLayer->AddFeature(feature);
            }
          }

          if(layer == NULL) {
            layer = new OGRILI1Layer(layername, poSRSIn, bWriterIn, eReqType, poDSIn);
            AddLayer(layer);
          }

          OGRFieldDefn fieldDef("_TID", OFTString);
          layer->GetLayerDefn()->AddFieldDefn(&fieldDef);

          for(size_t i=0; i<attributes.size(); i++) {
            IOM_OBJECT obj = attributes.at(i);
            AddField(layer, model, obj);
          }

          // additional point layer added
          if(multiple > 0) {
            for(int i = 1; i <= multiple; i++) {
               OGRILI1Layer* pointLayer = papoLayers[nLayers-(i+1)];
               for (int j=0; j < layer->GetLayerDefn()->GetFieldCount(); j++) {
                 pointLayer->CreateField(layer->GetLayerDefn()->GetFieldDefn(j));
               }
            if (pointLayer->GetLayerDefn()->GetGeomType() == wkbUnknown) pointLayer->GetLayerDefn()->SetGeomType(wkbPoint);
            }
          }

          if (papoLayers[nLayers-1]->GetLayerDefn()->GetFieldCount() == 0) {
              //Area layer added
              OGRILI1Layer* areaLayer = papoLayers[nLayers-1];
              for (int i=0; i < layer->GetLayerDefn()->GetFieldCount(); i++) {
                areaLayer->CreateField(layer->GetLayerDefn()->GetFieldDefn(i));
              }
          }
        }
      } else if (EQUAL(tag,"iom04.metamodel.Ili1Format")) {
        codeBlank = atoi(iom_getattrvalue(modelele, "blankCode"));
        CPLDebug( "OGR_ILI", "Reading Ili1Format blankCode '%c'", codeBlank );
        codeUndefined = atoi(iom_getattrvalue(modelele, "undefinedCode"));
        CPLDebug( "OGR_ILI", "Reading Ili1Format undefinedCode '%c'", codeUndefined );
        codeContinue = atoi(iom_getattrvalue(modelele, "continueCode"));
        CPLDebug( "OGR_ILI", "Reading Ili1Format continueCode '%c'", codeContinue );
      }
      iom_releaseobject(modelele);

      modelele=iom_nextobject(modelelei);
      j++;
    }
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
    char *topic = NULL;
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
        CPLFree(topic);
        topic = CPLStrdup(CSLGetField(tokens, 1));
      }
      else if (EQUAL(firsttok, "TABL"))
      {
        CPLDebug( "OGR_ILI", "Reading table '%s'", GetLayerNameString(topic, CSLGetField(tokens, 1)) );
        const char *layername = GetLayerNameString(topic, CSLGetField(tokens, 1));
        curLayer = GetLayerByName(layername);

        int multiple = HasMultiplePointGeom(layername);

        // create only a new layer if there is no curLayer AND
        // if there are more than one point geometry columns
        if (curLayer == NULL && multiple < 1) { //create one
          CPLDebug( "OGR_ILI", "No model found, using default field names." );
          OGRSpatialReference *poSRSIn = NULL;
          int bWriterIn = 0;
          OGRwkbGeometryType eReqType = wkbUnknown;
          OGRILI1DataSource *poDSIn = NULL;
          curLayer = new OGRILI1Layer(GetLayerNameString(topic, CSLGetField(tokens, 1)), poSRSIn, bWriterIn, eReqType, poDSIn);
          AddLayer(curLayer);
        }
        if(curLayer != NULL) {
          for (int i=0; i < curLayer->GetLayerDefn()->GetFieldCount(); i++) {
            CPLDebug( "OGR_ILI", "Field %d: %s", i,  curLayer->GetLayerDefn()->GetFieldDefn(i)->GetNameRef());
          }
        }
        ret = ReadTable(layername);
      }
      else if (EQUAL(firsttok, "ETOP"))
      {
      }
      else if (EQUAL(firsttok, "EMOD"))
      {
      }
      else if (EQUAL(firsttok, "ENDE"))
      {
        CSLDestroy(tokens);
        CPLFree(topic);
        return TRUE;
      }
      else
      {
        CPLDebug( "OGR_ILI", "Unexpected token: %s", firsttok );
      }

      CSLDestroy(tokens);
      tokens = NULL;
    }

    CSLDestroy(tokens);
    CPLFree(topic);

    return ret;
}

int ILI1Reader::AddIliGeom(OGRFeature *feature, int iField, long fpos)
{
#if defined(_WIN32) || defined(__WIN32__)
    //Other positions on Windows !?
#else
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
    CPLFree( pszRawData );
#endif
    return TRUE;
}


int ILI1Reader::ReadTable(const char *layername) {
    char **tokens = NULL;
    const char *firsttok = NULL;
    int ret = TRUE;
    int warned = FALSE;
    int fIndex;
    int geomIdx = 0;

    // curLayer is NULL if we have more than one
    // point geometry column
    if(curLayer == NULL) {
      OGRFeature *metaFeature = NULL;
      metaLayer->ResetReading();
      while((metaFeature = metaLayer->GetNextFeature()) != NULL ) {
        if(EQUAL(layername, metaFeature->GetFieldAsString(0))) {
          const char *geomlayername = metaFeature->GetFieldAsString(2);
          curLayer = GetLayerByName(geomlayername);
          delete metaFeature;
          break;
        }
        delete metaFeature;
      }
    }

    OGRFeatureDefn *featureDef = curLayer->GetLayerDefn();
    OGRFeature *feature = NULL;

    // get the geometry index of the current layer
    // only if the model is read
    if(featureDef->GetFieldCount() != 0) {
      OGRFeature *metaFeature = NULL;
      metaLayer->ResetReading();
      while((metaFeature = metaLayer->GetNextFeature()) != NULL ) {
        if(EQUAL(curLayer->GetLayerDefn()->GetName(), metaFeature->GetFieldAsString(2))) {
          geomIdx = metaFeature->GetFieldAsInteger(1);
        }
        delete metaFeature;
      }
    }

    long fpos = VSIFTell(fpItf);
    while (ret && (tokens = ReadParseLine()))
    {
      firsttok = CSLGetField(tokens, 0);
      if (EQUAL(firsttok, "OBJE"))
      {
        //Check for features spread over multiple objects
        if (featureDef->GetGeomType() == wkbPolygon)
        {
          //Multiple polygon rings
          feature = curLayer->GetFeatureRef(atol(CSLGetField(tokens, 2)));
        }
        else if (featureDef->GetGeomType() == wkbGeometryCollection)
        {
          //AREA lines spread over mutltiple objects
        }
        else
        {
          feature = NULL;
        }

        if (feature == NULL)
        {
          if (featureDef->GetFieldCount() == 0)
          {
            CPLDebug( "OGR_ILI", "No field definition found for table: %s", featureDef->GetName() );
            //Model not read - use heuristics
            for (fIndex=1; fIndex<CSLCount(tokens); fIndex++)
            {
              char szFieldName[32];
              sprintf(szFieldName, "Field%02d", fIndex);
              OGRFieldDefn oFieldDefn(szFieldName, OFTString);
              featureDef->AddFieldDefn(&oFieldDefn);
            }
          }
          //start new feature
          feature = new OGRFeature(featureDef);

          int fieldno = 0;
          for (fIndex=1; fIndex<CSLCount(tokens) && fieldno < featureDef->GetFieldCount(); fIndex++, fieldno++)
          {
            if (!(tokens[fIndex][0] == codeUndefined && tokens[fIndex][1] == '\0')) {
              //CPLDebug( "READ TABLE OGR_ILI", "Setting Field %d: %s", fieldno, tokens[fIndex]);
              if (featureDef->GetFieldDefn(fieldno)->GetType() == OFTString) {
                  //Replace space marks
                  for(char* pszString = tokens[fIndex] ; *pszString != '\0'; pszString++ ) {
                      if (*pszString == codeBlank) *pszString = ' ';
                  }
              }
              CPLDebug( "READ TABLE OGR_ILI", "Setting Field %d (Type %d): %s", fieldno, featureDef->GetFieldDefn(fieldno)->GetType(), tokens[fIndex]);
              feature->SetField(fieldno, tokens[fIndex]);
              if (featureDef->GetFieldDefn(fieldno)->GetType() == OFTReal
                  && fieldno > 0
                  && featureDef->GetFieldDefn(fieldno-1)->GetType() == OFTReal
                  && featureDef->GetGeomType() == wkbPoint

                  /*
                  // if there is no ili model read,
                  // we have no chance to detect the
                  // geometry column!!
                  */

                  && (fieldno-2) == geomIdx) {
                //add Point geometry
                OGRPoint *ogrPoint = new OGRPoint(atof(tokens[fIndex-1]), atof(tokens[fIndex]));
                feature->SetGeometryDirectly(ogrPoint);
              }
            }
          }
          if (!warned && featureDef->GetFieldCount() != CSLCount(tokens)-1 && !(featureDef->GetFieldCount() == CSLCount(tokens) && EQUAL(featureDef->GetFieldDefn(featureDef->GetFieldCount()-1)->GetNameRef(), "ILI_Geometry"))) {
            CPLDebug( "OGR_ILI", "Field count doesn't match. %d declared, %d found", featureDef->GetFieldCount(), CSLCount(tokens)-1);
            warned = TRUE;
          }
          if (featureDef->GetGeomType() == wkbPolygon)
            feature->SetFID(atol(feature->GetFieldAsString(1)));
          else if (feature->GetFieldCount() > 0)
            feature->SetFID(atol(feature->GetFieldAsString(0)));
          curLayer->AddFeature(feature);
        }
      }
      else if (EQUAL(firsttok, "STPT"))
      {
        ReadGeom(tokens, featureDef->GetGeomType(), feature);
        if (EQUAL(featureDef->GetFieldDefn(featureDef->GetFieldCount()-1)->GetNameRef(), "ILI_Geometry"))
        {
          AddIliGeom(feature, featureDef->GetFieldCount()-1, fpos); //TODO: append multi-OBJECT geometries
        }
      }
      else if (EQUAL(firsttok, "ELIN"))
      {
        //empty geom
      }
      else if (EQUAL(firsttok, "EDGE"))
      {
        tokens = ReadParseLine(); //STPT
        ReadGeom(tokens, wkbMultiLineString, feature);
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
        if(HasMultiplePointGeom(layername) > 0) {
          OGRFeature *metaFeature = NULL;
          metaLayer->ResetReading();
          while((metaFeature = metaLayer->GetNextFeature()) != NULL ) {
            int pntCln = 1;
            if(EQUAL(layername, metaFeature->GetFieldAsString(0)) && !EQUAL(curLayer->GetLayerDefn()->GetName(), metaFeature->GetFieldAsString(2))) {
              pntCln++;
              OGRILI1Layer *curLayerTmp = GetLayerByName(metaFeature->GetFieldAsString(2));
              OGRFeature *tmpFeature = NULL;
              int geomIdxTmp = metaFeature->GetFieldAsInteger(1);
              curLayer->ResetReading();
              while((tmpFeature = curLayer->GetNextFeature()) != NULL ) {
                OGRPoint *ogrPoint = new OGRPoint(atof(tmpFeature->GetFieldAsString(geomIdxTmp + pntCln)), atof(tmpFeature->GetFieldAsString(geomIdxTmp + pntCln + 1)));
                tmpFeature->SetGeometryDirectly(ogrPoint);
                curLayerTmp->AddFeature(tmpFeature);
              }
            }
            delete metaFeature;
          }
        }
        CSLDestroy(tokens);
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

void ILI1Reader::ReadGeom(char **stgeom, OGRwkbGeometryType eType, OGRFeature *feature) {

    char **tokens = NULL;
    const char *firsttok = NULL;
    int end = FALSE;
    int isArc = FALSE;
    OGRLineString *ogrLine = NULL; //current line
    OGRLinearRing *ogrRing = NULL; //current ring
    OGRPolygon *ogrPoly = NULL; //current polygon
    OGRPoint ogrPoint, arcPoint, endPoint; //points for arc interpolation
    OGRMultiLineString *ogrMultiLine = NULL; //current multi line

    //tokens = ["STPT", "1111", "22222"]
    ogrPoint.setX(atof(stgeom[1])); ogrPoint.setY(atof(stgeom[2]));
    ogrLine = (eType == wkbPolygon) ? new OGRLinearRing() : new OGRLineString();
    ogrLine->addPoint(&ogrPoint);

    //Set feature geometry
    if (eType == wkbMultiLineString)
    {
      ogrMultiLine = new OGRMultiLineString();
      feature->SetGeometryDirectly(ogrMultiLine);
    }
    else if (eType == wkbGeometryCollection) //AREA
    {
      if (feature->GetGeometryRef())
        ogrMultiLine = (OGRMultiLineString *)feature->GetGeometryRef();
      else
      {
        ogrMultiLine = new OGRMultiLineString();
        feature->SetGeometryDirectly(ogrMultiLine);
      }
    }
    else if (eType == wkbPolygon)
    {
      if (feature->GetGeometryRef())
      {
        ogrPoly = (OGRPolygon *)feature->GetGeometryRef();
        if (ogrPoly->getNumInteriorRings() > 0)
          ogrRing = ogrPoly->getInteriorRing(ogrPoly->getNumInteriorRings()-1);
        else
          ogrRing = ogrPoly->getExteriorRing();
        if (ogrRing && !ogrRing->get_IsClosed()) ogrLine = ogrRing; //SURFACE polygon spread over multiple OBJECTs
      }
      else
      {
        ogrPoly = new OGRPolygon();
        feature->SetGeometryDirectly(ogrPoly);
      }
    }
    else
    {
      feature->SetGeometryDirectly(ogrLine);
    }

    //Parse geometry
    while (!end && (tokens = ReadParseLine()))
    {
      firsttok = CSLGetField(tokens, 0);
      if (EQUAL(firsttok, "LIPT"))
      {
        if (isArc) {
          endPoint.setX(atof(tokens[1])); endPoint.setY(atof(tokens[2]));
          interpolateArc(ogrLine, &ogrPoint, &arcPoint, &endPoint, arcIncr);
        }
        ogrPoint.setX(atof(tokens[1])); ogrPoint.setY(atof(tokens[2])); isArc = FALSE;
        ogrLine->addPoint(&ogrPoint);
      }
      else if (EQUAL(firsttok, "ARCP"))
      {
        isArc = TRUE;
        arcPoint.setX(atof(tokens[1])); arcPoint.setY(atof(tokens[2]));
      }
      else if (EQUAL(firsttok, "ELIN"))
      {
        if (ogrMultiLine)
        {
          ogrMultiLine->addGeometryDirectly(ogrLine);
        }
        if (ogrPoly && ogrLine != ogrRing)
        {
          ogrPoly->addRingDirectly((OGRLinearRing *)ogrLine);
        }
        end = TRUE;
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
      else if (EQUAL(firsttok, "ETAB"))
      {
        end = TRUE;
      }
      else
      {
        CPLDebug( "OGR_ILI", "Unexpected token: %s", firsttok );
      }

      CSLDestroy(tokens);
    }
}

/************************************************************************/
/*                              AddLayer()                              */
/************************************************************************/

void ILI1Reader::AddLayer( OGRILI1Layer * poNewLayer )

{
    nLayers++;

    papoLayers = (OGRILI1Layer **)
        CPLRealloc( papoLayers, sizeof(void*) * nLayers );

    papoLayers[nLayers-1] = poNewLayer;
}

/************************************************************************/
/*                              AddAreaLayer()                              */
/************************************************************************/

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRILI1Layer *ILI1Reader::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

OGRILI1Layer *ILI1Reader::GetLayerByName( const char* pszLayerName )

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
    while (strlen(pszLine) && token[0] == codeContinue && token[1] == '\0')
    {
       //remove last token
      CPLFree(tokens[CSLCount(tokens)-1]);
      tokens[CSLCount(tokens)-1] = NULL;

      pszLine = CPLReadLine( fpItf );
      conttok = CSLTokenizeString2( pszLine, " ", CSLT_PRESERVEESCAPES );
      if (!conttok || !EQUAL(conttok[0], "CONT"))
      {
          CSLDestroy(conttok);
          break;
      }

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

void DestroyILI1Reader(IILI1Reader* reader)
{
    if (reader)
        delete reader;
}
