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
#include "ili1reader.h"
#include "ili1readerp.h"

#include <vector>

#ifdef HAVE_GEOS
#  define POLYGONIZE_AREAS
#endif

#ifndef POLYGONIZE_AREAS
#  if defined(__GNUC_PREREQ)
#    warning Interlis 1 Area polygonizing disabled. Needs GEOS >= 3.1.0
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

const char* ILI1Reader::GetLayerNameString(const char* topicname, const char* tablename) {

    return CPLSPrintf("%s__%s", topicname, tablename);
}

int ILI1Reader::ReadModel(ImdReader *poImdReader, const char *pszModelFilename, OGRILI1DataSource *poDS) {

  poImdReader->ReadModel(pszModelFilename);
  for (FeatureDefnInfos::const_iterator it = poImdReader->featureDefnInfos.begin(); it != poImdReader->featureDefnInfos.end(); ++it)
  {
    //CPLDebug( "OGR_ILI", "Adding OGRILI1Layer with table '%s'", it->poTableDefn->GetName() );
    OGRILI1Layer* layer = new OGRILI1Layer(it->poTableDefn, it->poGeomFieldInfos, poDS);
    AddLayer(layer);
    //Create additional layers for surface and area geometries
    for (GeomFieldInfos::const_iterator it2 = it->poGeomFieldInfos.begin(); it2 != it->poGeomFieldInfos.end(); ++it2)
    {
      if (it2->second.geomTable)
      {
        GeomFieldInfos oGeomFieldInfos;
        //CPLDebug( "OGR_ILI", "Adding OGRILI1Layer with geometry table '%s'", it2->second.geomTable->GetName() );
        OGRILI1Layer* geomlayer = new OGRILI1Layer(it2->second.geomTable, oGeomFieldInfos, poDS);
        AddLayer(geomlayer);
      }
    }
  }

  codeBlank = poImdReader->codeBlank;
  CPLDebug( "OGR_ILI", "Ili1Format blankCode '%c'", poImdReader->codeBlank );
  codeUndefined = poImdReader->codeUndefined;
  CPLDebug( "OGR_ILI", "Ili1Format undefinedCode '%c'", poImdReader->codeUndefined );
  codeContinue = poImdReader->codeContinue;
  CPLDebug( "OGR_ILI", "Ili1Format continueCode '%c'", poImdReader->codeContinue );
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
        const char *layername = GetLayerNameString(topic, CSLGetField(tokens, 1));
        CPLDebug( "OGR_ILI", "Reading table '%s'", layername );
        curLayer = GetLayerByName(layername);

        if (curLayer == NULL) { //create one
          CPLError(CE_Warning, CPLE_AppDefined,
              "No model definition for table '%s' found, using default field names.", layername );
          OGRFeatureDefn* poFeatureDefn = new OGRFeatureDefn(GetLayerNameString(topic, CSLGetField(tokens, 1)));
          poFeatureDefn->SetGeomType( wkbUnknown );
          GeomFieldInfos oGeomFieldInfos;
          curLayer = new OGRILI1Layer(poFeatureDefn, oGeomFieldInfos, NULL);
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
        CPLError(CE_Warning, CPLE_AppDefined, "Unexpected token: %s", firsttok );
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
    int geomIdx = -1;

    OGRFeatureDefn *featureDef = curLayer->GetLayerDefn();
    OGRFeature *feature = NULL;

    long fpos = VSIFTell(fpItf);
    while (ret && (tokens = ReadParseLine()))
    {
      firsttok = CSLGetField(tokens, 0);
      if (EQUAL(firsttok, "OBJE"))
      {
        //Check for features spread over multiple objects
        if (featureDef->GetGeomType() == wkbPolygon) //FIXME: Multi-geom support
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
            CPLError(CE_Warning, CPLE_AppDefined,
                "No field definition found for table: %s", featureDef->GetName() );
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
              //CPLDebug( "READ TABLE OGR_ILI", "Setting Field %d (Type %d): %s", fieldno, featureDef->GetFieldDefn(fieldno)->GetType(), tokens[fIndex]);
              if (featureDef->GetFieldDefn(fieldno)->GetType() == OFTString) {
                  //Interlis 1 encoding is ISO 8859-1 (Latin1) -> Recode to UTF-8
                  char* pszRecoded = CPLRecode(tokens[fIndex], CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
                  //Replace space marks
                  for(char* pszString = pszRecoded; *pszString != '\0'; pszString++ ) {
                      if (*pszString == codeBlank) *pszString = ' ';
                  }
                  feature->SetField(fieldno, pszRecoded);
                  CPLFree(pszRecoded);
              } else {
                feature->SetField(fieldno, tokens[fIndex]);
              }
              if (featureDef->GetFieldDefn(fieldno)->GetType() == OFTReal
                  && fieldno > 0
                  && featureDef->GetFieldDefn(fieldno-1)->GetType() == OFTReal) {
                //check for Point geometry (Coord type)
                // if there is no ili model read,
                // we have no chance to detect the
                // geometry column!!
                CPLString geomfldname = featureDef->GetFieldDefn(fieldno)->GetNameRef();
                //Check if name ends with _1
                if (geomfldname.size() >= 2 && geomfldname[geomfldname.size()-2] == '_') {
                  geomfldname = geomfldname.substr(0, geomfldname.size()-2);
                  geomIdx = featureDef->GetGeomFieldIndex(geomfldname.c_str());
                  if (geomIdx == -1)
                  {
                    CPLError(CE_Warning, CPLE_AppDefined,
                        "No matching definition for field '%s' of table %s found", geomfldname.c_str(), featureDef->GetName());
                  }
                } else {
                  geomIdx = -1;
                }
                if (geomIdx >= 0) {
                  if (featureDef->GetGeomFieldDefn(geomIdx)->GetType() == wkbPoint) {
                    //add Point geometry
                    OGRPoint *ogrPoint = new OGRPoint(atof(tokens[fIndex-1]), atof(tokens[fIndex]));
                    feature->SetGeomFieldDirectly(geomIdx, ogrPoint);
                  } else if (featureDef->GetGeomFieldDefn(geomIdx)->GetType() == wkbPoint25D && fieldno > 1 && featureDef->GetFieldDefn(fieldno-2)->GetType() == OFTReal) {
                    //add 3D Point geometry
                    OGRPoint *ogrPoint = new OGRPoint(atof(tokens[fIndex-2]), atof(tokens[fIndex-1]), atof(tokens[fIndex]));
                    feature->SetGeomFieldDirectly(geomIdx, ogrPoint);
                  }
                }
              }
            }
          }
          if (!warned && featureDef->GetFieldCount() != CSLCount(tokens)-1 && !(featureDef->GetFieldCount() == CSLCount(tokens) && EQUAL(featureDef->GetFieldDefn(featureDef->GetFieldCount()-1)->GetNameRef(), "ILI_Geometry"))) {
            CPLError(CE_Warning, CPLE_AppDefined,
                "Field count doesn't match. %d declared, %d found", featureDef->GetFieldCount(), CSLCount(tokens)-1);
            warned = TRUE;
          }
          if (feature->GetFieldCount() > 0) {
            feature->SetFID(atol(feature->GetFieldAsString(0))); //TODO: use IDENT field from model instead of TID
          }
          curLayer->AddFeature(feature);
          geomIdx = -1; //Reset
        }
      }
      else if (EQUAL(firsttok, "STPT"))
      {
        //Find next non-Point geometry
        do { geomIdx++; } while (geomIdx < featureDef->GetGeomFieldCount() && featureDef->GetGeomFieldDefn(geomIdx)->GetType() == wkbPoint);
        OGRwkbGeometryType geomType = (geomIdx < featureDef->GetGeomFieldCount()) ? featureDef->GetGeomFieldDefn(geomIdx)->GetType() : wkbNone;
        ReadGeom(tokens, geomIdx, geomType, feature);
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
        //Find next non-Point geometry
        do { geomIdx++; } while (geomIdx < featureDef->GetGeomFieldCount() && featureDef->GetGeomFieldDefn(geomIdx)->GetType() == wkbPoint);
        ReadGeom(tokens, geomIdx, wkbMultiLineString, feature);
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
        CSLDestroy(tokens);
        return TRUE;
      }
      else
      {
        CPLError(CE_Warning, CPLE_AppDefined, "Unexpected token: %s", firsttok );
      }

      CSLDestroy(tokens);
      fpos = VSIFTell(fpItf);
    }

    return ret;
}

void ILI1Reader::ReadGeom(char **stgeom, int geomIdx, OGRwkbGeometryType eType, OGRFeature *feature) {
    char **tokens = NULL;
    const char *firsttok = NULL;
    int end = FALSE;
    int isArc = FALSE;
    OGRLineString *ogrLine = NULL; //current line
    OGRLinearRing *ogrRing = NULL; //current ring
    OGRPolygon *ogrPoly = NULL; //current polygon
    OGRPoint ogrPoint, arcPoint, endPoint; //points for arc interpolation
    OGRMultiLineString *ogrMultiLine = NULL; //current multi line

    //CPLDebug( "OGR_ILI", "ILI1Reader::ReadGeom geomIdx: %d", geomIdx);
    //tokens = ["STPT", "1111", "22222"]
    ogrPoint.setX(atof(stgeom[1])); ogrPoint.setY(atof(stgeom[2]));
    ogrLine = (eType == wkbPolygon) ? new OGRLinearRing() : new OGRLineString();
    ogrLine->addPoint(&ogrPoint);

    //Set feature geometry
    if (eType == wkbMultiLineString)
    {
      ogrMultiLine = new OGRMultiLineString();
      feature->SetGeomFieldDirectly(geomIdx, ogrMultiLine);
    }
    else if (eType == wkbGeometryCollection) //AREA
    {
      if (feature->GetGeometryRef())
        ogrMultiLine = (OGRMultiLineString *)feature->GetGeometryRef();
      else
      {
        ogrMultiLine = new OGRMultiLineString();
        feature->SetGeomFieldDirectly(geomIdx, ogrMultiLine);
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
        feature->SetGeomFieldDirectly(geomIdx, ogrPoly);
      }
    }
    else
    {
      feature->SetGeomFieldDirectly(geomIdx, ogrLine);
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
        CPLError(CE_Warning, CPLE_AppDefined, "Unexpected token: %s", firsttok );
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
