/******************************************************************************
 *
 * Project:  Interlis 1 Reader
 * Purpose:  Implementation of ILI1Reader class.
 * Author:   Pirmin Kalberer, Sourcepole AG
 *
 ******************************************************************************
 * Copyright (c) 2004, Pirmin Kalberer, Sourcepole AG
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_api.h"
#include "ogr_ili1.h"
#include "ogr_geos.h"

#include "ili1reader.h"
#include "ili1readerp.h"

#include <vector>

#ifdef HAVE_GEOS
#  define POLYGONIZE_AREAS
#endif

#ifndef POLYGONIZE_AREAS
#  if defined(__GNUC_PREREQ)
//#    warning Interlis 1 Area polygonizing disabled. Needs GEOS >= 3.1.0
#  endif
#endif

CPL_CVSID("$Id$");

//
// ILI1Reader
//
IILI1Reader::~IILI1Reader() {}

ILI1Reader::ILI1Reader() :
  fpItf(NULL),
  nLayers(0),
  papoLayers(NULL),
  curLayer(NULL),
  codeBlank('_'),
  codeUndefined('@'),
  codeContinue('\\')
{}

ILI1Reader::~ILI1Reader()
{
  if (fpItf) VSIFClose( fpItf );

  for( int i=0; i < nLayers; i++)
     delete papoLayers[i];
  CPLFree(papoLayers);
}

/* -------------------------------------------------------------------- */
/*      Open the source file.                                           */
/* -------------------------------------------------------------------- */
int ILI1Reader::OpenFile( const char *pszFilename )
{
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

const char* ILI1Reader::GetLayerNameString( const char* topicname,
                                            const char* tablename)
{

    return CPLSPrintf("%s__%s", topicname, tablename);
}

int ILI1Reader::ReadModel( ImdReader *poImdReader,
                           const char *pszModelFilename,
                           OGRILI1DataSource *poDS)
{

  poImdReader->ReadModel(pszModelFilename);
  for (FeatureDefnInfos::const_iterator it = poImdReader->featureDefnInfos.begin(); it != poImdReader->featureDefnInfos.end(); ++it)
  {
#if DEBUG_VERBOSE
    CPLDebug( "OGR_ILI", "Adding OGRILI1Layer with table '%s'",
              it->GetTableDefnRef()->GetName() );
#endif
    OGRILI1Layer* layer = new OGRILI1Layer( it->GetTableDefnRef(),
                                            it->poGeomFieldInfos, poDS);
    AddLayer(layer);
    // Create additional layers for surface and area geometries.
    for (GeomFieldInfos::const_iterator it2 = it->poGeomFieldInfos.begin();
         it2 != it->poGeomFieldInfos.end();
         ++it2)
    {
      if (it2->second.GetGeomTableDefnRef())
      {
        OGRFeatureDefn* poGeomTableDefn = it2->second.GetGeomTableDefnRef();
        OGRGeomFieldDefn* poOGRGeomFieldDefn
            = poGeomTableDefn->GetGeomFieldDefn(0);
        GeomFieldInfos oGeomFieldInfos;
        // We add iliGeomType to recognize Ili1 geom tables
        oGeomFieldInfos[poOGRGeomFieldDefn->GetNameRef()].iliGeomType
            = it2->second.iliGeomType;
#if DEBUG_VERBOSE
        CPLDebug( "OGR_ILI", "Adding OGRILI1Layer with geometry table '%s'",
                  poGeomTableDefn->GetName() );
#endif
        OGRILI1Layer* geomlayer
            = new OGRILI1Layer(poGeomTableDefn, oGeomFieldInfos, poDS);
        AddLayer(geomlayer);
      }
    }
  }

  codeBlank = poImdReader->codeBlank;
  CPLDebug( "OGR_ILI", "Ili1Format blankCode '%c'", poImdReader->codeBlank );
  codeUndefined = poImdReader->codeUndefined;
  CPLDebug( "OGR_ILI", "Ili1Format undefinedCode '%c'",
            poImdReader->codeUndefined );
  codeContinue = poImdReader->codeContinue;
  CPLDebug( "OGR_ILI", "Ili1Format continueCode '%c'",
            poImdReader->codeContinue );
  return 0;
}

int ILI1Reader::ReadFeatures() {
    char **tokens = NULL;
    const char *pszLine = NULL;
    char *topic = CPLStrdup("(null)");
    int ret = TRUE;

    while (ret && (tokens = ReadParseLine()) != NULL)
    {
      const char *firsttok = tokens[0];
      if (EQUAL(firsttok, "SCNT"))
      {
        //read description
        do
        {
          pszLine = CPLReadLine( fpItf );
        }
        while (pszLine && !STARTS_WITH_CI(pszLine, "////"));
        ret = (pszLine != NULL);
      }
      else if (EQUAL(firsttok, "MOTR"))
      {
        //read model
        do
        {
          pszLine = CPLReadLine( fpItf );
        }
        while (pszLine && !STARTS_WITH_CI(pszLine, "////"));
        ret = (pszLine != NULL);
      }
      else if (EQUAL(firsttok, "MTID"))
      {
      }
      else if (EQUAL(firsttok, "MODL"))
      {
      }
      else if (EQUAL(firsttok, "TOPI") && CSLCount(tokens) >= 2)
      {
        CPLFree(topic);
        topic = CPLStrdup(CSLGetField(tokens, 1));
      }
      else if (EQUAL(firsttok, "TABL") && CSLCount(tokens) >= 2)
      {
        const char *layername
            = GetLayerNameString(topic, CSLGetField(tokens, 1));
        CPLDebug( "OGR_ILI", "Reading table '%s'", layername );
        curLayer = GetLayerByName(layername);

        if (curLayer == NULL) { //create one
          CPLError( CE_Warning, CPLE_AppDefined,
                    "No model definition for table '%s' found, "
                    "using default field names.", layername );
          OGRFeatureDefn* poFeatureDefn
            = new OGRFeatureDefn(
                GetLayerNameString(topic, CSLGetField(tokens, 1)));
          poFeatureDefn->SetGeomType( wkbUnknown );
          GeomFieldInfos oGeomFieldInfos;
          curLayer = new OGRILI1Layer(poFeatureDefn, oGeomFieldInfos, NULL);
          AddLayer(curLayer);
        }
        if(curLayer != NULL) {
          for (int i=0; i < curLayer->GetLayerDefn()->GetFieldCount(); i++) {
            CPLDebug( "OGR_ILI", "Field %d: %s", i,
                      curLayer->GetLayerDefn()->GetFieldDefn(i)->GetNameRef());
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
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Unexpected token: %s", firsttok );
      }

      CSLDestroy(tokens);
      tokens = NULL;
    }

    CSLDestroy(tokens);
    CPLFree(topic);

    return ret;
}

int ILI1Reader::ReadTable(CPL_UNUSED const char *layername) {
    char **tokens = NULL;
    int ret = TRUE;
    int warned = FALSE;
    int geomIdx = -1;

    OGRFeatureDefn *featureDef = curLayer->GetLayerDefn();
    OGRFeature *feature = NULL;
    bool bFeatureAdded = false;

    while (ret && (tokens = ReadParseLine()) != NULL)
    {
      const char *firsttok = CSLGetField(tokens, 0);
      if (EQUAL(firsttok, "OBJE"))
      {
        if (featureDef->GetFieldCount() == 0)
        {
          CPLError( CE_Warning, CPLE_AppDefined,
                    "No field definition found for table: %s",
                    featureDef->GetName() );
          // Model not read - use heuristics.
          for( int fIndex=1; fIndex<CSLCount(tokens); fIndex++ )
          {
            char szFieldName[32];
            snprintf(szFieldName, sizeof(szFieldName), "Field%02d", fIndex);
            OGRFieldDefn oFieldDefn(szFieldName, OFTString);
            featureDef->AddFieldDefn(&oFieldDefn);
          }
        }
        //start new feature
        if( !bFeatureAdded )
            delete feature;
        feature = new OGRFeature(featureDef);

        for( int fIndex=1, fieldno = 0;
             fIndex<CSLCount(tokens) && fieldno < featureDef->GetFieldCount();
             fIndex++, fieldno++ )
        {
          if (!(tokens[fIndex][0] == codeUndefined && tokens[fIndex][1] == '\0')) {
#ifdef DEBUG_VERBOSE
            CPLDebug( "READ TABLE OGR_ILI", "Setting Field %d (Type %d): %s",
                      fieldno, featureDef->GetFieldDefn(fieldno)->GetType(),
                      tokens[fIndex] );
#endif
            if (featureDef->GetFieldDefn(fieldno)->GetType() == OFTString) {
                // Interlis 1 encoding is ISO 8859-1 (Latin1) -> Recode to UTF-8
                char* pszRecoded = CPLRecode(
                    tokens[fIndex], CPL_ENC_ISO8859_1, CPL_ENC_UTF8);
                // Replace space marks
                for( char* pszString = pszRecoded;
                     *pszString != '\0';
                     pszString++ ) {
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
              // Check for Point geometry (Coord type).
              // If there is no ili model read,
              // we have no chance to detect the
              // geometry column.
              CPLString geomfldname
                  = featureDef->GetFieldDefn(fieldno)->GetNameRef();
              // Check if name ends with _1.
              if (geomfldname.size() >= 2 && geomfldname[geomfldname.size()-2]
                  == '_') {
                geomfldname = geomfldname.substr(0, geomfldname.size()-2);
                geomIdx = featureDef->GetGeomFieldIndex(geomfldname.c_str());
                if (geomIdx == -1)
                {
                  CPLError( CE_Warning, CPLE_AppDefined,
                            "No matching definition for field '%s' of "
                            "table %s found",
                            geomfldname.c_str(), featureDef->GetName() );
                }
              } else {
                geomIdx = -1;
              }
              if (geomIdx >= 0) {
                if (featureDef->GetGeomFieldDefn(geomIdx)->GetType() ==
                    wkbPoint) {
                  // Add Point geometry.
                  OGRPoint *ogrPoint = new OGRPoint(
                      CPLAtof(tokens[fIndex-1]), CPLAtof(tokens[fIndex]));
                  feature->SetGeomFieldDirectly(geomIdx, ogrPoint);
                } else if (featureDef->GetGeomFieldDefn(geomIdx)->GetType() ==
                           wkbPoint25D && fieldno > 1 &&
                           featureDef->GetFieldDefn(fieldno-2)->GetType() ==
                           OFTReal) {
                  // Add 3D Point geometry.
                  OGRPoint *ogrPoint = new OGRPoint(
                      CPLAtof(tokens[fIndex-2]), CPLAtof(tokens[fIndex-1]),
                      CPLAtof(tokens[fIndex]) );
                  feature->SetGeomFieldDirectly(geomIdx, ogrPoint);
                }
              }
            }
          }
        }
        if (!warned && featureDef->GetFieldCount() != CSLCount(tokens)-1) {
          CPLError( CE_Warning, CPLE_AppDefined,
                    "Field count of table %s doesn't match. %d declared, "
                    "%d found (e.g. ignored LINEATTR)",
                    featureDef->GetName(), featureDef->GetFieldCount(),
                    CSLCount(tokens) - 1 );
          warned = TRUE;
        }
        if (feature->GetFieldCount() > 0) {
          // USE _TID as FID. TODO: respect IDENT field from model.
          feature->SetFID(feature->GetFieldAsInteger64(0));
        }
        curLayer->AddFeature(feature);
        bFeatureAdded = true;
        geomIdx = -1; //Reset
      }
      else if (EQUAL(firsttok, "STPT") && feature != NULL)
      {
        //Find next non-Point geometry
        if (geomIdx < 0) geomIdx = 0;
        while (geomIdx < featureDef->GetGeomFieldCount() &&
               featureDef->GetGeomFieldDefn(geomIdx)->GetType() == wkbPoint) {
            geomIdx++;
        }
        OGRwkbGeometryType geomType
            = (geomIdx < featureDef->GetGeomFieldCount()) ?
               featureDef->GetGeomFieldDefn(geomIdx)->GetType() : wkbNone;
        ReadGeom(tokens, geomIdx, geomType, feature);
      }
      else if (EQUAL(firsttok, "ELIN"))
      {
        // Empty geom.
      }
      else if (EQUAL(firsttok, "EDGE") && feature != NULL)
      {
        CSLDestroy(tokens);
        tokens = ReadParseLine(); //STPT
        //Find next non-Point geometry
        do {
            geomIdx++;
        } while (geomIdx < featureDef->GetGeomFieldCount() &&
                 featureDef->GetGeomFieldDefn(geomIdx)->GetType() == wkbPoint);
        ReadGeom(tokens, geomIdx, wkbMultiLineString, feature);
      }
      else if (EQUAL(firsttok, "PERI"))
      {
      }
      else if (EQUAL(firsttok, "ETAB"))
      {
        CPLDebug( "OGR_ILI", "Total features: " CPL_FRMT_GIB,
                  curLayer->GetFeatureCount() );
        CSLDestroy(tokens);
        if( !bFeatureAdded )
            delete feature;
        return TRUE;
      }
      else
      {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Unexpected token: %s", firsttok );
      }

      CSLDestroy(tokens);
    }

    if( !bFeatureAdded )
        delete feature;

    return ret;
}

void ILI1Reader::ReadGeom( char **stgeom, int geomIdx, OGRwkbGeometryType eType,
                           OGRFeature *feature ) {
#ifdef DEBUG_VERBOSE
    CPLDebug( "OGR_ILI",
              "ILI1Reader::ReadGeom geomIdx: %d OGRGeometryType: %s",
              geomIdx, OGRGeometryTypeToName(eType) );
#endif
    if (eType == wkbNone)
    {
      CPLError( CE_Warning, CPLE_AppDefined,
                "Calling ILI1Reader::ReadGeom with wkbNone" );
    }

    // Initialize geometry.

    OGRCompoundCurve *ogrCurve = new OGRCompoundCurve();
    OGRCurvePolygon *ogrPoly = NULL; //current polygon
    OGRMultiCurve *ogrMultiLine = NULL; //current multi line

    if (eType == wkbMultiCurve || eType == wkbMultiLineString)
    {
      ogrMultiLine = new OGRMultiCurve();
    }
    else if (eType == wkbPolygon || eType == wkbCurvePolygon)
    {
      ogrPoly = new OGRCurvePolygon();
    }

    OGRPoint ogrPoint; // Current point.
    ogrPoint.setX(CPLAtof(stgeom[1])); ogrPoint.setY(CPLAtof(stgeom[2]));

    OGRLineString *ogrLine = new OGRLineString();
    ogrLine->addPoint(&ogrPoint);

    // Parse geometry.

    char **tokens = NULL;
    bool end = false;
    OGRCircularString *arc = NULL; //current arc

    while (!end && (tokens = ReadParseLine()) != NULL)
    {
      const char *firsttok = CSLGetField(tokens, 0);
      if (EQUAL(firsttok, "LIPT"))
      {
        ogrPoint.setX(CPLAtof(tokens[1])); ogrPoint.setY(CPLAtof(tokens[2]));
        if (arc) {
          arc->addPoint(&ogrPoint);
          OGRErr error =  ogrCurve->addCurveDirectly(arc);
          if (error != OGRERR_NONE) {
            CPLError(CE_Warning, CPLE_AppDefined, "Added geometry: %s", arc->exportToJson() );
            delete arc;
          }
          arc = NULL;
        }
        ogrLine->addPoint(&ogrPoint);
      }
      else if (EQUAL(firsttok, "ARCP"))
      {
        //Finish line and start arc
        if (ogrLine->getNumPoints() > 1) {
          OGRErr error = ogrCurve->addCurveDirectly(ogrLine);
          if (error != OGRERR_NONE) {
            CPLError(CE_Warning, CPLE_AppDefined, "Added geometry: %s", ogrLine->exportToJson() );
          }
          ogrLine = new OGRLineString();
        } else {
          ogrLine->empty();
        }
        delete arc;
        arc = new OGRCircularString();
        arc->addPoint(&ogrPoint);
        ogrPoint.setX(CPLAtof(tokens[1])); ogrPoint.setY(CPLAtof(tokens[2]));
        arc->addPoint(&ogrPoint);
      }
      else if (EQUAL(firsttok, "ELIN"))
      {
        if (ogrLine->getNumPoints() > 1) { // Ignore single LIPT after ARCP
          OGRErr error = ogrCurve->addCurveDirectly(ogrLine);
          if (error != OGRERR_NONE) {
            CPLError(CE_Warning, CPLE_AppDefined, "Added geometry: %s", ogrLine->exportToJson() );
          }
          ogrLine = NULL;
        }
        if (!ogrCurve->IsEmpty()) {
          if (ogrMultiLine)
          {
            OGRErr error = ogrMultiLine->addGeometryDirectly(ogrCurve);
            if (error != OGRERR_NONE) {
              CPLError(CE_Warning, CPLE_AppDefined, "Added geometry: %s", ogrCurve->exportToJson() );
            }
            ogrCurve = NULL;
          }
          if (ogrPoly)
          {
            OGRErr error = ogrPoly->addRingDirectly(ogrCurve);
            if (error != OGRERR_NONE) {
              CPLError(CE_Warning, CPLE_AppDefined, "Added geometry: %s", ogrCurve->exportToJson() );
            }
            ogrCurve = NULL;
          }
        }
        end = true;
      }
      else if (EQUAL(firsttok, "EEDG"))
      {
        end = true;
      }
      else if (EQUAL(firsttok, "LATT"))
      {
        //Line Attributes (ignored)
      }
      else if (EQUAL(firsttok, "EFLA"))
      {
        end = true;
      }
      else if (EQUAL(firsttok, "ETAB"))
      {
        end = true;
      }
      else
      {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Unexpected token: %s", firsttok );
      }

      CSLDestroy(tokens);
    }
    delete arc;

    delete ogrLine;

    //Set feature geometry
    if (eType == wkbMultiCurve)
    {
      feature->SetGeomFieldDirectly(geomIdx, ogrMultiLine);
      delete ogrCurve;
    }
    else if (eType == wkbMultiLineString)
    {
      feature->SetGeomFieldDirectly(geomIdx, ogrMultiLine->getLinearGeometry());
      delete ogrMultiLine;
      delete ogrCurve;
    }
    else if (eType == wkbCurvePolygon)
    {
      feature->SetGeomFieldDirectly(geomIdx, ogrPoly);
      delete ogrCurve;
    }
    else if (eType == wkbPolygon)
    {
      feature->SetGeomFieldDirectly(geomIdx, ogrPoly->getLinearGeometry());
      delete ogrPoly;
      delete ogrCurve;
    }
    else
    {
      feature->SetGeomFieldDirectly(geomIdx, ogrCurve);
    }
}

/************************************************************************/
/*                              AddLayer()                              */
/************************************************************************/

void ILI1Reader::AddLayer( OGRILI1Layer * poNewLayer )

{
    nLayers++;

    papoLayers = static_cast<OGRILI1Layer **>(
        CPLRealloc( papoLayers, sizeof(void*) * nLayers ) );

    papoLayers[nLayers-1] = poNewLayer;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRILI1Layer *ILI1Reader::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;

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
    CPLAssert( fpItf != NULL );
    if( fpItf == NULL )
        return NULL;

    const char  *pszLine = CPLReadLine( fpItf );
    if( pszLine == NULL )
        return NULL;

    if (strlen(pszLine) == 0) return NULL;

    char **tokens = CSLTokenizeString2( pszLine, " ", CSLT_PRESERVEESCAPES );
    char *token = tokens[CSLCount(tokens)-1];

    //Append CONT lines
    while (strlen(pszLine) && token[0] == codeContinue && token[1] == '\0')
    {
       //remove last token
      CPLFree(tokens[CSLCount(tokens)-1]);
      tokens[CSLCount(tokens)-1] = NULL;

      pszLine = CPLReadLine( fpItf );
      if( pszLine == NULL )
      {
          break;
      }
      char **conttok = CSLTokenizeString2( pszLine, " ", CSLT_PRESERVEESCAPES );
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
