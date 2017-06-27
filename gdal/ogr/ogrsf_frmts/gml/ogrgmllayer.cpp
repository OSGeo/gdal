/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  Implements OGRGMLLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_gml.h"
#include "gmlutils.h"
#include "cpl_conv.h"
#include "cpl_port.h"
#include "cpl_string.h"
#include "ogr_p.h"
#include "ogr_api.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                           OGRGMLLayer()                              */
/************************************************************************/

OGRGMLLayer::OGRGMLLayer( const char * pszName,
                          bool bWriterIn,
                          OGRGMLDataSource *poDSIn ) :
    poFeatureDefn(new OGRFeatureDefn(
        pszName + (STARTS_WITH_CI(pszName, "ogr:") ? 4 : 0))),
    iNextGMLId(0),
    bInvalidFIDFound(false),
    pszFIDPrefix(NULL),
    bWriter(bWriterIn),
    bSameSRS(false),
    poDS(poDSIn),
    poFClass(!bWriter ? poDS->GetReader()->GetClass(pszName) :  NULL),
    // Reader's should get the corresponding GMLFeatureClass and cache it.
    hCacheSRS(GML_BuildOGRGeometryFromList_CreateCache()),
    // Compatibility option. Not advertized, because hopefully won't be needed.
    // Just put here in case.
    bUseOldFIDFormat(CPLTestBool(
        CPLGetConfigOption("GML_USE_OLD_FID_FORMAT", "FALSE"))),
    // Must be in synced in OGR_G_CreateFromGML(), OGRGMLLayer::OGRGMLLayer()
    // and GMLReader::GMLReader().
    bFaceHoleNegative(CPLTestBool(
        CPLGetConfigOption("GML_FACE_HOLE_NEGATIVE", "NO")))
{
    SetDescription(poFeatureDefn->GetName());
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType(wkbNone);
}

/************************************************************************/
/*                           ~OGRGMLLayer()                           */
/************************************************************************/

OGRGMLLayer::~OGRGMLLayer()

{
    CPLFree(pszFIDPrefix);

    if( poFeatureDefn )
        poFeatureDefn->Release();

    GML_BuildOGRGeometryFromList_DestroyCache(hCacheSRS);
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGMLLayer::ResetReading()

{
    if (bWriter)
        return;

    if (poDS->GetReadMode() == INTERLEAVED_LAYERS ||
        poDS->GetReadMode() == SEQUENTIAL_LAYERS)
    {
        // Does the last stored feature belong to our layer ? If so, no
        // need to reset the reader.
        if (iNextGMLId == 0 && poDS->PeekStoredGMLFeature() != NULL &&
            poDS->PeekStoredGMLFeature()->GetClass() == poFClass)
            return;

        delete poDS->PeekStoredGMLFeature();
        poDS->SetStoredGMLFeature(NULL);
    }

    iNextGMLId = 0;
    poDS->GetReader()->ResetReading();
    CPLDebug("GML", "ResetReading()");
    if ( poDS->GetLayerCount() > 1 && poDS->GetReadMode() == STANDARD )
    {
        const char *pszElementName = poFClass->GetElementName();
        const char *pszLastPipe = strrchr(pszElementName, '|');
        if ( pszLastPipe != NULL )
            pszElementName = pszLastPipe + 1;
        poDS->GetReader()->SetFilteredClassName(pszElementName);
    }
}

/************************************************************************/
/*                              Increment()                             */
/************************************************************************/

static GIntBig Increment(GIntBig nVal)
{
    if( nVal <= GINTBIG_MAX - 1)
        return nVal + 1;
    return nVal;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGMLLayer::GetNextFeature()

{
    if (bWriter)
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "Cannot read features when writing a GML file");
        return NULL;
    }

    if( poDS->GetLastReadLayer() != this )
    {
        if( poDS->GetReadMode() != INTERLEAVED_LAYERS )
            ResetReading();
        poDS->SetLastReadLayer(this);
    }

/* ==================================================================== */
/*      Loop till we find and translate a feature meeting all our       */
/*      requirements.                                                   */
/* ==================================================================== */
    while( true )
    {
        GMLFeature *poGMLFeature = poDS->PeekStoredGMLFeature();
        if (poGMLFeature != NULL)
        {
            poDS->SetStoredGMLFeature(NULL);
        }
        else
        {
            poGMLFeature = poDS->GetReader()->NextFeature();
            if( poGMLFeature == NULL )
                return NULL;

            // We count reading low level GML features as a feature read for
            // work checking purposes, though at least we didn't necessary
            // have to turn it into an OGRFeature.
            m_nFeaturesRead++;
        }

/* -------------------------------------------------------------------- */
/*      Is it of the proper feature class?                              */
/* -------------------------------------------------------------------- */

        if( poGMLFeature->GetClass() != poFClass )
        {
            if( poDS->GetReadMode() == INTERLEAVED_LAYERS ||
                (poDS->GetReadMode() == SEQUENTIAL_LAYERS && iNextGMLId != 0) )
            {
                CPLAssert(poDS->PeekStoredGMLFeature() == NULL);
                poDS->SetStoredGMLFeature(poGMLFeature);
                return NULL;
            }
            else
            {
                delete poGMLFeature;
                continue;
            }
        }

/* -------------------------------------------------------------------- */
/*      Extract the fid:                                                */
/*      -Assumes the fids are non-negative integers with an optional    */
/*       prefix                                                         */
/*      -If a prefix differs from the prefix of the first feature from  */
/*       the poDS then the fids from the poDS are ignored and are       */
/*       assigned serially thereafter                                   */
/* -------------------------------------------------------------------- */
        GIntBig nFID = -1;
        const char *pszGML_FID = poGMLFeature->GetFID();
        if( bInvalidFIDFound )
        {
            nFID = iNextGMLId;
            iNextGMLId = Increment(iNextGMLId);
        }
        else if( pszGML_FID == NULL )
        {
            bInvalidFIDFound = true;
            nFID = iNextGMLId;
            iNextGMLId = Increment(iNextGMLId);
        }
        else if( iNextGMLId == 0 )
        {
            int j = 0;
            int i = static_cast<int>(strlen(pszGML_FID)) - 1;
            while( i >= 0 && pszGML_FID[i] >= '0'
                          && pszGML_FID[i] <= '9' && j < 20)
            {
                i--;
                j++;
            }
            // i points the last character of the fid.
            if( i >= 0 && j < 20 && pszFIDPrefix == NULL)
            {
                pszFIDPrefix = static_cast<char *>(CPLMalloc(i + 2));
                pszFIDPrefix[i + 1] = '\0';
                strncpy(pszFIDPrefix, pszGML_FID, i + 1);
            }
            // pszFIDPrefix now contains the prefix or NULL if no prefix is
            // found.
            if( j < 20 && sscanf(pszGML_FID + i + 1, CPL_FRMT_GIB, &nFID) == 1)
            {
                if( iNextGMLId <= nFID )
                    iNextGMLId = Increment(nFID);
            }
            else
            {
                bInvalidFIDFound = true;
                nFID = iNextGMLId;
                iNextGMLId = Increment(iNextGMLId);
            }
        }
        else  // if( iNextGMLId != 0 ).
        {
            const char *pszFIDPrefix_notnull = pszFIDPrefix;
            if (pszFIDPrefix_notnull == NULL) pszFIDPrefix_notnull = "";
            int nLenPrefix = static_cast<int>(strlen(pszFIDPrefix_notnull));

            if( strncmp(pszGML_FID, pszFIDPrefix_notnull, nLenPrefix) == 0 &&
                strlen(pszGML_FID + nLenPrefix) < 20 &&
                sscanf(pszGML_FID + nLenPrefix, CPL_FRMT_GIB, &nFID) == 1 )
            {
                // fid with the prefix. Using its numerical part.
                if( iNextGMLId < nFID )
                    iNextGMLId = Increment(nFID);
            }
            else
            {
                // fid without the aforementioned prefix or a valid numerical
                // part.
                bInvalidFIDFound = true;
                nFID = iNextGMLId;
                iNextGMLId = Increment(iNextGMLId);
            }
        }

/* -------------------------------------------------------------------- */
/*      Does it satisfy the spatial query, if there is one?             */
/* -------------------------------------------------------------------- */

        OGRGeometry **papoGeometries = NULL;
        const CPLXMLNode *const *papsGeometry = poGMLFeature->GetGeometryList();

        OGRGeometry *poGeom = NULL;

        if( poFeatureDefn->GetGeomFieldCount() > 1 )
        {
            papoGeometries = static_cast<OGRGeometry **>(CPLCalloc(
                poFeatureDefn->GetGeomFieldCount(), sizeof(OGRGeometry *)));
            const char *pszSRSName = poDS->GetGlobalSRSName();
            for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
            {
                const CPLXMLNode *psGeom = poGMLFeature->GetGeometryRef(i);
                if( psGeom != NULL )
                {
                    const CPLXMLNode *myGeometryList[2] = {psGeom, NULL};
                    poGeom = GML_BuildOGRGeometryFromList(
                        myGeometryList, true,
                        poDS->GetInvertAxisOrderIfLatLong(), pszSRSName,
                        poDS->GetConsiderEPSGAsURN(),
                        poDS->GetSwapCoordinates(),
                        poDS->GetSecondaryGeometryOption(), hCacheSRS,
                        bFaceHoleNegative);

                    // Do geometry type changes if needed to match layer
                    // geometry type.
                    if (poGeom != NULL)
                    {
                        papoGeometries[i] = OGRGeometryFactory::forceTo(
                            poGeom,
                            poFeatureDefn->GetGeomFieldDefn(i)->GetType());
                        poGeom = NULL;
                    }
                    else
                    {
                        // We assume the createFromGML() function would have
                        // already reported the error.
                        for(int j = 0; j < poFeatureDefn->GetGeomFieldCount();
                            j++)
                        {
                            delete papoGeometries[j];
                        }
                        CPLFree(papoGeometries);
                        delete poGMLFeature;
                        return NULL;
                    }
                }
            }

            if( m_poFilterGeom != NULL &&
                m_iGeomFieldFilter >= 0 &&
                m_iGeomFieldFilter < poFeatureDefn->GetGeomFieldCount() &&
                papoGeometries[m_iGeomFieldFilter] &&
                !FilterGeometry( papoGeometries[m_iGeomFieldFilter] ) )
            {
                for( int j = 0; j < poFeatureDefn->GetGeomFieldCount(); j++ )
                {
                    delete papoGeometries[j];
                }
                CPLFree(papoGeometries);
                delete poGMLFeature;
                continue;
            }
        }
        else if (papsGeometry[0] != NULL)
        {
            const char *pszSRSName = poDS->GetGlobalSRSName();
            CPLPushErrorHandler(CPLQuietErrorHandler);
            poGeom = GML_BuildOGRGeometryFromList(
                papsGeometry, true,
                poDS->GetInvertAxisOrderIfLatLong(),
                pszSRSName,
                poDS->GetConsiderEPSGAsURN(),
                poDS->GetSwapCoordinates(),
                poDS->GetSecondaryGeometryOption(),
                hCacheSRS,
                bFaceHoleNegative);
            CPLPopErrorHandler();

            // Do geometry type changes if needed to match layer geometry type.
            if (poGeom != NULL)
            {
                poGeom = OGRGeometryFactory::forceTo(poGeom, GetGeomType());
            }
            else
            {
                const CPLString osLastErrorMsg(CPLGetLastErrorMsg());

                const bool bGoOn = CPLTestBool(
                    CPLGetConfigOption("GML_SKIP_CORRUPTED_FEATURES", "NO"));

                CPLError(bGoOn ? CE_Warning : CE_Failure, CPLE_AppDefined,
                         "Geometry of feature " CPL_FRMT_GIB
                         " %scannot be parsed: %s%s",
                         nFID, pszGML_FID ? CPLSPrintf("%s ", pszGML_FID) : "",
                         osLastErrorMsg.c_str(),
                         bGoOn ? ". Skipping to next feature.":
                         ". You may set the GML_SKIP_CORRUPTED_FEATURES "
                         "configuration option to YES to skip to the next "
                         "feature");
                delete poGMLFeature;
                if( bGoOn )
                    continue;
                return NULL;
            }

            if( m_poFilterGeom != NULL && !FilterGeometry(poGeom) )
            {
                delete poGMLFeature;
                delete poGeom;
                continue;
            }
        }

/* -------------------------------------------------------------------- */
/*      Convert the whole feature into an OGRFeature.                   */
/* -------------------------------------------------------------------- */
        int iDstField = 0;
        OGRFeature *poOGRFeature = new OGRFeature(poFeatureDefn);

        poOGRFeature->SetFID(nFID);
        if (poDS->ExposeId())
        {
            if (pszGML_FID)
                poOGRFeature->SetField(iDstField, pszGML_FID);
            iDstField++;
        }

        const int nPropertyCount = poFClass->GetPropertyCount();
        for( int iField = 0; iField < nPropertyCount; iField++, iDstField++ )
        {
            const GMLProperty *psGMLProperty =
                poGMLFeature->GetProperty(iField);
            if( psGMLProperty == NULL || psGMLProperty->nSubProperties == 0 )
                continue;

            if( EQUAL(psGMLProperty->papszSubProperties[0], OGR_GML_NULL) )
            {
                poOGRFeature->SetFieldNull( iDstField );
                continue;
            }

            switch( poFClass->GetProperty(iField)->GetType() )
            {
              case GMLPT_Real:
              {
                  poOGRFeature->SetField(
                      iDstField, CPLAtof(psGMLProperty->papszSubProperties[0]));
              }
              break;

              case GMLPT_IntegerList:
              {
                  const int nCount = psGMLProperty->nSubProperties;
                  int *panIntList =
                      static_cast<int *>(CPLMalloc(sizeof(int) * nCount));

                  for( int i = 0; i < nCount; i++ )
                      panIntList[i] =
                          atoi(psGMLProperty->papszSubProperties[i]);

                  poOGRFeature->SetField(iDstField, nCount, panIntList);
                  CPLFree(panIntList);
              }
              break;

              case GMLPT_Integer64List:
              {
                  const int nCount = psGMLProperty->nSubProperties;
                  GIntBig *panIntList = static_cast<GIntBig *>(
                      CPLMalloc(sizeof(GIntBig) * nCount));

                  for( int i = 0; i < nCount; i++ )
                      panIntList[i] =
                          CPLAtoGIntBig(psGMLProperty->papszSubProperties[i]);

                  poOGRFeature->SetField(iDstField, nCount, panIntList);
                  CPLFree(panIntList);
              }
              break;

              case GMLPT_RealList:
              {
                  const int nCount = psGMLProperty->nSubProperties;
                  double *padfList = static_cast<double *>(
                      CPLMalloc(sizeof(double) * nCount));

                  for( int i = 0; i < nCount; i++ )
                      padfList[i] =
                          CPLAtof(psGMLProperty->papszSubProperties[i]);

                  poOGRFeature->SetField(iDstField, nCount, padfList);
                  CPLFree(padfList);
              }
              break;

              case GMLPT_StringList:
              case GMLPT_FeaturePropertyList:
              {
                  poOGRFeature->SetField(iDstField,
                                         psGMLProperty->papszSubProperties);
              }
              break;

              case GMLPT_Boolean:
              {
                  if( strcmp(psGMLProperty->papszSubProperties[0],
                             "true") == 0 ||
                      strcmp(psGMLProperty->papszSubProperties[0], "1") == 0 )
                  {
                      poOGRFeature->SetField(iDstField, 1);
                  }
                  else if( strcmp(psGMLProperty->papszSubProperties[0],
                                  "false") == 0 ||
                           strcmp(psGMLProperty->papszSubProperties[0],
                                  "0") == 0 )
                  {
                      poOGRFeature->SetField(iDstField, 0);
                  }
                  else
                  {
                      poOGRFeature->SetField(
                          iDstField, psGMLProperty->papszSubProperties[0]);
                  }
                  break;
              }

              case GMLPT_BooleanList:
              {
                  const int nCount = psGMLProperty->nSubProperties;
                  int *panIntList =
                      static_cast<int *>(CPLMalloc(sizeof(int) * nCount));

                  for( int i = 0; i < nCount; i++ )
                  {
                      panIntList[i] = (
                          strcmp(psGMLProperty->papszSubProperties[i],
                                 "true") == 0 ||
                          strcmp(psGMLProperty->papszSubProperties[i],
                                 "1") == 0 );
                  }

                  poOGRFeature->SetField(iDstField, nCount, panIntList);
                  CPLFree(panIntList);
                  break;
              }

              default:
                  poOGRFeature->SetField(iDstField,
                                         psGMLProperty->papszSubProperties[0]);
                  break;
            }
        }

        delete poGMLFeature;
        poGMLFeature = NULL;

        // Assign the geometry before the attribute filter because
        // the attribute filter may use a special field like OGR_GEOMETRY.
        if( papoGeometries != NULL )
        {
            for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
            {
                poOGRFeature->SetGeomFieldDirectly(i, papoGeometries[i]);
            }
            CPLFree(papoGeometries);
            papoGeometries = NULL;
        }
        else
        {
            poOGRFeature->SetGeometryDirectly(poGeom);
        }

        // Assign SRS.
        for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
        {
            poGeom = poOGRFeature->GetGeomFieldRef(i);
            if( poGeom != NULL )
            {
                OGRSpatialReference *poSRS =
                    poFeatureDefn->GetGeomFieldDefn(i)->GetSpatialRef();
                if (poSRS != NULL)
                    poGeom->assignSpatialReference(poSRS);
            }
        }

/* -------------------------------------------------------------------- */
/*      Test against the attribute query.                               */
/* -------------------------------------------------------------------- */
        if( m_poAttrQuery != NULL && !m_poAttrQuery->Evaluate(poOGRFeature) )
        {
            delete poOGRFeature;
            continue;
        }

        // Got the desired feature.
        return poOGRFeature;
    }

    return NULL;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRGMLLayer::GetFeatureCount( int bForce )

{
    if( poFClass == NULL )
        return 0;

    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL )
        return OGRLayer::GetFeatureCount(bForce);

    // If the schema is read from a .xsd file, we haven't read
    // the feature count, so compute it now.
    GIntBig nFeatureCount = poFClass->GetFeatureCount();
    if (nFeatureCount < 0)
    {
        nFeatureCount = OGRLayer::GetFeatureCount(bForce);
        poFClass->SetFeatureCount(nFeatureCount);
    }

    return nFeatureCount;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRGMLLayer::GetExtent(OGREnvelope *psExtent, int bForce )

{
    if( GetGeomType() == wkbNone )
        return OGRERR_FAILURE;

    double dfXMin = 0.0;
    double dfXMax = 0.0;
    double dfYMin = 0.0;
    double dfYMax = 0.0;
    if( poFClass != NULL &&
        poFClass->GetExtents(&dfXMin, &dfXMax, &dfYMin, &dfYMax) )
    {
        psExtent->MinX = dfXMin;
        psExtent->MaxX = dfXMax;
        psExtent->MinY = dfYMin;
        psExtent->MaxY = dfYMax;

        return OGRERR_NONE;
    }

    return OGRLayer::GetExtent(psExtent, bForce);
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

static void GMLWriteField(OGRGMLDataSource *poDS,
                          VSILFILE *fp,
                          bool bWriteSpaceIndentation,
                          const char *pszPrefix,
                          bool bRemoveAppPrefix,
                          OGRFieldDefn *poFieldDefn,
                          const char *pszVal )

{
    const char *pszFieldName = poFieldDefn->GetNameRef();

    while( *pszVal == ' ' )
        pszVal++;

    if (bWriteSpaceIndentation)
        VSIFPrintfL(fp, "      ");

    if( bRemoveAppPrefix )
        poDS->PrintLine(fp, "<%s>%s</%s>", pszFieldName, pszVal, pszFieldName);
    else
        poDS->PrintLine(fp, "<%s:%s>%s</%s:%s>",
                        pszPrefix,
                        pszFieldName,
                        pszVal,
                        pszPrefix,
                        pszFieldName);
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRGMLLayer::ICreateFeature( OGRFeature *poFeature )

{
    const bool bIsGML3Output = poDS->IsGML3Output();
    VSILFILE *fp = poDS->GetOutputFP();
    const bool bWriteSpaceIndentation = poDS->WriteSpaceIndentation();
    const char *pszPrefix = poDS->GetAppPrefix();
    const bool bRemoveAppPrefix = poDS->RemoveAppPrefix();

    if( !bWriter )
        return OGRERR_FAILURE;

    poFeature->FillUnsetWithDefault(TRUE, NULL);
    if( !poFeature->Validate(OGR_F_VAL_ALL & ~OGR_F_VAL_GEOM_TYPE &
                                 ~OGR_F_VAL_ALLOW_NULL_WHEN_DEFAULT,
                             TRUE) )
        return OGRERR_FAILURE;

    if (bWriteSpaceIndentation)
        VSIFPrintfL(fp, "  ");
    if (bIsGML3Output)
    {
        if( bRemoveAppPrefix )
            poDS->PrintLine(fp, "<featureMember>");
        else
            poDS->PrintLine(fp, "<%s:featureMember>", pszPrefix);
    }
    else
    {
        poDS->PrintLine(fp, "<gml:featureMember>");
    }

    if( iNextGMLId == 0 )
    {
        bSameSRS = true;
        for( int iGeomField = 1;
             iGeomField < poFeatureDefn->GetGeomFieldCount();
             iGeomField++ )
        {
            OGRGeomFieldDefn *poFieldDefn0 = poFeatureDefn->GetGeomFieldDefn(0);
            OGRGeomFieldDefn *poFieldDefn =
                poFeatureDefn->GetGeomFieldDefn(iGeomField);
            OGRSpatialReference *poSRS0 = poFieldDefn0->GetSpatialRef();
            OGRSpatialReference *poSRS = poFieldDefn->GetSpatialRef();
            if( poSRS0 != NULL && poSRS == NULL )
            {
                bSameSRS = false;
            }
            else if( poSRS0 == NULL && poSRS != NULL )
            {
                bSameSRS = false;
            }
            else if( poSRS0 != NULL && poSRS != NULL &&
                     poSRS0 != poSRS && !poSRS0->IsSame(poSRS) )
            {
                bSameSRS = false;
            }
        }
    }

    if( poFeature->GetFID() == OGRNullFID )
        poFeature->SetFID(iNextGMLId++);

    if (bWriteSpaceIndentation)
        VSIFPrintfL(fp, "    ");
    VSIFPrintfL(fp, "<");
    if( !bRemoveAppPrefix )
        VSIFPrintfL(fp, "%s:", pszPrefix);

    int nGMLIdIndex = -1;
    if (bIsGML3Output)
    {
        nGMLIdIndex = poFeatureDefn->GetFieldIndex("gml_id");
        if (nGMLIdIndex >= 0 && poFeature->IsFieldSetAndNotNull(nGMLIdIndex) )
            poDS->PrintLine(fp, "%s gml:id=\"%s\">",
                            poFeatureDefn->GetName(),
                            poFeature->GetFieldAsString(nGMLIdIndex));
        else
            poDS->PrintLine(fp, "%s gml:id=\"%s." CPL_FRMT_GIB "\">",
                            poFeatureDefn->GetName(),
                            poFeatureDefn->GetName(),
                            poFeature->GetFID());
    }
    else
    {
        nGMLIdIndex = poFeatureDefn->GetFieldIndex("fid");
        if (bUseOldFIDFormat)
        {
            poDS->PrintLine(fp, "%s fid=\"F" CPL_FRMT_GIB "\">",
                            poFeatureDefn->GetName(),
                            poFeature->GetFID());
        }
        else if (nGMLIdIndex >= 0 && poFeature->IsFieldSetAndNotNull( nGMLIdIndex ) )
        {
            poDS->PrintLine(fp, "%s fid=\"%s\">",
                            poFeatureDefn->GetName(),
                            poFeature->GetFieldAsString(nGMLIdIndex));
        }
        else
        {
            poDS->PrintLine(fp, "%s fid=\"%s." CPL_FRMT_GIB "\">",
                            poFeatureDefn->GetName(),
                            poFeatureDefn->GetName(),
                            poFeature->GetFID());
        }
    }

    for( int iGeomField = 0; iGeomField < poFeatureDefn->GetGeomFieldCount();
         iGeomField++ )
    {
        OGRGeomFieldDefn *poFieldDefn =
            poFeatureDefn->GetGeomFieldDefn(iGeomField);

        // Write out Geometry - for now it isn't indented properly.
        // GML geometries don't like very much the concept of empty geometry.
        OGRGeometry *poGeom = poFeature->GetGeomFieldRef(iGeomField);
        if( poGeom != NULL && !poGeom->IsEmpty())
        {
            OGREnvelope3D sGeomBounds;

            const int nCoordDimension = poGeom->getCoordinateDimension();

            poGeom->getEnvelope(&sGeomBounds);
            if( bSameSRS )
                poDS->GrowExtents(&sGeomBounds, nCoordDimension);

            if (poGeom->getSpatialReference() == NULL &&
                poFieldDefn->GetSpatialRef() != NULL)
                poGeom->assignSpatialReference(poFieldDefn->GetSpatialRef());

            if (bIsGML3Output && poDS->WriteFeatureBoundedBy())
            {
                bool bCoordSwap = false;

                char *pszSRSName =
                    GML_GetSRSName(poGeom->getSpatialReference(),
                                   poDS->GetSRSNameFormat(), &bCoordSwap);
                char szLowerCorner[75] = {};
                char szUpperCorner[75] = {};
                if (bCoordSwap)
                {
                    OGRMakeWktCoordinate(szLowerCorner, sGeomBounds.MinY,
                                         sGeomBounds.MinX, sGeomBounds.MinZ,
                                         nCoordDimension);
                    OGRMakeWktCoordinate(szUpperCorner, sGeomBounds.MaxY,
                                         sGeomBounds.MaxX, sGeomBounds.MaxZ,
                                         nCoordDimension);
                }
                else
                {
                    OGRMakeWktCoordinate(szLowerCorner, sGeomBounds.MinX,
                                         sGeomBounds.MinY, sGeomBounds.MinZ,
                                         nCoordDimension);
                    OGRMakeWktCoordinate(szUpperCorner, sGeomBounds.MaxX,
                                         sGeomBounds.MaxY, sGeomBounds.MaxZ,
                                         nCoordDimension);
                }
                if (bWriteSpaceIndentation)
                    VSIFPrintfL(fp, "      ");
                poDS->PrintLine(
                    fp,
                    "<gml:boundedBy><gml:Envelope%s%s><gml:lowerCorner>%s"
                    "</gml:lowerCorner><gml:upperCorner>%s</gml:upperCorner>"
                    "</gml:Envelope></gml:boundedBy>",
                    (nCoordDimension == 3) ? " srsDimension=\"3\"" : "",
                    pszSRSName, szLowerCorner, szUpperCorner);
                CPLFree(pszSRSName);
            }

            char **papszOptions = NULL;
            if( bIsGML3Output )
            {
                papszOptions = CSLAddString(papszOptions, "FORMAT=GML3");
                if( poDS->GetSRSNameFormat() == SRSNAME_SHORT )
                    papszOptions =
                        CSLAddString(papszOptions, "SRSNAME_FORMAT=SHORT");
                else if( poDS->GetSRSNameFormat() == SRSNAME_OGC_URN )
                    papszOptions =
                        CSLAddString(papszOptions, "SRSNAME_FORMAT=OGC_URN");
                else if( poDS->GetSRSNameFormat() == SRSNAME_OGC_URL )
                    papszOptions =
                        CSLAddString(papszOptions, "SRSNAME_FORMAT=OGC_URL");
            }
            const char *pszSRSDimensionLoc = poDS->GetSRSDimensionLoc();
            if( pszSRSDimensionLoc != NULL )
                papszOptions = CSLSetNameValue(papszOptions, "SRSDIMENSION_LOC",
                                               pszSRSDimensionLoc);
            if (poDS->IsGML32Output())
            {
                if( poFeatureDefn->GetGeomFieldCount() > 1 )
                    papszOptions = CSLAddString(
                        papszOptions,
                        CPLSPrintf("GMLID=%s.%s." CPL_FRMT_GIB,
                                   poFeatureDefn->GetName(),
                                   poFieldDefn->GetNameRef(),
                                   poFeature->GetFID()));
                else
                    papszOptions = CSLAddString(
                        papszOptions,
                        CPLSPrintf("GMLID=%s.geom." CPL_FRMT_GIB,
                                   poFeatureDefn->GetName(),
                                   poFeature->GetFID()));
            }

            char *pszGeometry = NULL;
            if( !bIsGML3Output &&
                OGR_GT_IsNonLinear(poGeom->getGeometryType()) )
            {
                OGRGeometry *poGeomTmp = OGRGeometryFactory::forceTo(
                    poGeom->clone(),
                    OGR_GT_GetLinear(poGeom->getGeometryType()));
                pszGeometry = poGeomTmp->exportToGML(papszOptions);
                delete poGeomTmp;
            }
            else
            {
                if( wkbFlatten(poGeom->getGeometryType()) == wkbTriangle )
                {
                    pszGeometry = poGeom->exportToGML(papszOptions);

                    const char *pszGMLID =
                        poDS->IsGML32Output()
                            ? CPLSPrintf(
                                  " gml:id=\"%s\"",
                                  CSLFetchNameValue(papszOptions, "GMLID"))
                            : "";
                    char *pszNewGeom = CPLStrdup(
                        CPLSPrintf("<gml:TriangulatedSurface%s><gml:patches>%s<"
                                   "/gml:patches></gml:TriangulatedSurface>",
                                   pszGMLID, pszGeometry));
                    CPLFree(pszGeometry);
                    pszGeometry = pszNewGeom;
                }
                else
                {
                    pszGeometry = poGeom->exportToGML(papszOptions);
                }
            }
            CSLDestroy(papszOptions);
            if( pszGeometry )
            {
                if (bWriteSpaceIndentation)
                    VSIFPrintfL(fp, "      ");
                if( bRemoveAppPrefix )
                    poDS->PrintLine( fp, "<%s>%s</%s>",
                                    poFieldDefn->GetNameRef(),
                                    pszGeometry,
                                    poFieldDefn->GetNameRef() );
                else
                    poDS->PrintLine( fp, "<%s:%s>%s</%s:%s>",
                                     pszPrefix, poFieldDefn->GetNameRef(),
                                     pszGeometry,
                                     pszPrefix, poFieldDefn->GetNameRef() );
            }
            else
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Export of geometry to GML failed");
            }
            CPLFree(pszGeometry);
        }
    }

    // Write all "set" fields.
    for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        if( iField == nGMLIdIndex )
            continue;
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn( iField );

        if( poFeature->IsFieldNull( iField ) )
        {
            const char* pszFieldName = poFieldDefn->GetNameRef();

            if (bWriteSpaceIndentation)
                VSIFPrintfL(fp, "      ");

            if( bRemoveAppPrefix )
                poDS->PrintLine( fp, "<%s xsi:nil=\"true\"/>",
                                pszFieldName);
            else
                poDS->PrintLine( fp, "<%s:%s xsi:nil=\"true\"/>",
                                pszPrefix,
                                pszFieldName);
        }
        else if( poFeature->IsFieldSet( iField ) )
        {
            OGRFieldType eType = poFieldDefn->GetType();
            if (eType == OFTStringList )
            {
                char **papszIter = poFeature->GetFieldAsStringList(iField);
                while( papszIter != NULL && *papszIter != NULL )
                {
                    char *pszEscaped = OGRGetXML_UTF8_EscapedString(*papszIter);
                    GMLWriteField(poDS, fp, bWriteSpaceIndentation, pszPrefix,
                                  bRemoveAppPrefix, poFieldDefn, pszEscaped);
                    CPLFree(pszEscaped);

                    papszIter++;
                }
            }
            else if (eType == OFTIntegerList )
            {
                int nCount = 0;
                const int *panVals =
                    poFeature->GetFieldAsIntegerList(iField, &nCount);
                if( poFieldDefn->GetSubType() == OFSTBoolean )
                {
                    for(int i = 0; i < nCount; i++)
                    {
                        // 0 and 1 are OK, but the canonical representation is
                        // false and true.
                        GMLWriteField(poDS, fp, bWriteSpaceIndentation,
                                      pszPrefix, bRemoveAppPrefix, poFieldDefn,
                                      panVals[i] ? "true" : "false");
                    }
                }
                else
                {
                    for(int i = 0; i < nCount; i++)
                    {
                        GMLWriteField(poDS, fp, bWriteSpaceIndentation,
                                      pszPrefix, bRemoveAppPrefix, poFieldDefn,
                                      CPLSPrintf("%d", panVals[i]));
                    }
                }
            }
            else if (eType == OFTInteger64List )
            {
                int nCount = 0;
                const GIntBig *panVals =
                    poFeature->GetFieldAsInteger64List(iField, &nCount);
                if( poFieldDefn->GetSubType() == OFSTBoolean )
                {
                    for(int i = 0; i < nCount; i++)
                    {
                        // 0 and 1 are OK, but the canonical representation is
                        // false and true.
                        GMLWriteField(poDS, fp, bWriteSpaceIndentation,
                                      pszPrefix, bRemoveAppPrefix, poFieldDefn,
                                      panVals[i] ? "true" : "false");
                    }
                }
                else
                {
                    for(int i = 0; i < nCount; i++)
                    {
                        GMLWriteField(poDS, fp, bWriteSpaceIndentation,
                                      pszPrefix, bRemoveAppPrefix, poFieldDefn,
                                      CPLSPrintf(CPL_FRMT_GIB, panVals[i]));
                    }
                }
            }
            else if (eType == OFTRealList )
            {
                int nCount = 0;
                const double *padfVals =
                    poFeature->GetFieldAsDoubleList(iField, &nCount);
                for(int i = 0; i < nCount; i++)
                {
                    char szBuffer[80] = {};
                    CPLsnprintf(szBuffer, sizeof(szBuffer), "%.15g",
                                padfVals[i]);
                    GMLWriteField(poDS, fp, bWriteSpaceIndentation, pszPrefix,
                                  bRemoveAppPrefix, poFieldDefn, szBuffer);
                }
            }
            else if ((eType == OFTInteger || eType == OFTInteger64) &&
                     poFieldDefn->GetSubType() == OFSTBoolean )
            {
                // 0 and 1 are OK, but the canonical representation is false and
                // true.
                GMLWriteField(poDS, fp, bWriteSpaceIndentation, pszPrefix,
                              bRemoveAppPrefix, poFieldDefn,
                              (poFeature->GetFieldAsInteger(iField)) ? "true"
                                                                     : "false");
            }
            else
            {
                const char *pszRaw = poFeature->GetFieldAsString(iField);

                char *pszEscaped = OGRGetXML_UTF8_EscapedString(pszRaw);

                GMLWriteField(poDS, fp, bWriteSpaceIndentation, pszPrefix,
                              bRemoveAppPrefix, poFieldDefn, pszEscaped);
                CPLFree(pszEscaped);
            }
        }
    }

    if (bWriteSpaceIndentation)
        VSIFPrintfL(fp, "    ");
    if( bRemoveAppPrefix )
        poDS->PrintLine(fp, "</%s>", poFeatureDefn->GetName());
    else
        poDS->PrintLine(fp, "</%s:%s>", pszPrefix, poFeatureDefn->GetName());
    if (bWriteSpaceIndentation)
        VSIFPrintfL(fp, "  ");
    if (bIsGML3Output)
    {
        if( bRemoveAppPrefix )
            poDS->PrintLine(fp, "</featureMember>");
        else
            poDS->PrintLine(fp, "</%s:featureMember>", pszPrefix);
    }
    else
    {
        poDS->PrintLine(fp, "</gml:featureMember>");
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGMLLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap, OLCSequentialWrite) )
        return bWriter;

    else if( EQUAL(pszCap, OLCCreateField) )
        return bWriter && iNextGMLId == 0;

    else if( EQUAL(pszCap, OLCCreateGeomField) )
        return bWriter && iNextGMLId == 0;

    else if( EQUAL(pszCap, OLCFastGetExtent) )
    {
        if( poFClass == NULL )
            return FALSE;

        double dfXMin = 0.0;
        double dfXMax = 0.0;
        double dfYMin = 0.0;
        double dfYMax = 0.0;

        return poFClass->GetExtents(&dfXMin, &dfXMax, &dfYMin, &dfYMax);
    }

    else if( EQUAL(pszCap, OLCFastFeatureCount) )
    {
        if( poFClass == NULL ||
            m_poFilterGeom != NULL ||
            m_poAttrQuery != NULL )
            return FALSE;

        return poFClass->GetFeatureCount() != -1;
    }

    else if( EQUAL(pszCap, OLCStringsAsUTF8) )
        return TRUE;

    else if( EQUAL(pszCap, OLCCurveGeometries) )
        return poDS->IsGML3Output();

    else
        return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRGMLLayer::CreateField( OGRFieldDefn *poField, int bApproxOK )

{
    if( !bWriter || iNextGMLId != 0 )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Enforce XML naming semantics on element name.                   */
/* -------------------------------------------------------------------- */
    OGRFieldDefn oCleanCopy(poField);
    char *pszName = CPLStrdup(poField->GetNameRef());
    CPLCleanXMLElementName(pszName);

    if( strcmp(pszName, poField->GetNameRef()) != 0 )
    {
        if( !bApproxOK )
        {
            CPLFree(pszName);
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to create field with name '%s', it would not\n"
                     "be valid as an XML element name.",
                     poField->GetNameRef());
            return OGRERR_FAILURE;
        }

        oCleanCopy.SetName(pszName);
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Field name '%s' adjusted to '%s' to be a valid\n"
                 "XML element name.",
                 poField->GetNameRef(), pszName);
    }

    CPLFree(pszName);

    poFeatureDefn->AddFieldDefn(&oCleanCopy);

    return OGRERR_NONE;
}

/************************************************************************/
/*                          CreateGeomField()                           */
/************************************************************************/

OGRErr OGRGMLLayer::CreateGeomField( OGRGeomFieldDefn *poField, int bApproxOK )

{
    if( !bWriter || iNextGMLId != 0 )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      Enforce XML naming semantics on element name.                   */
/* -------------------------------------------------------------------- */
    OGRGeomFieldDefn oCleanCopy(poField);
    char *pszName = CPLStrdup(poField->GetNameRef());
    CPLCleanXMLElementName(pszName);

    if( strcmp(pszName, poField->GetNameRef()) != 0 )
    {
        if( !bApproxOK )
        {
            CPLFree(pszName);
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Unable to create field with name '%s', it would not\n"
                     "be valid as an XML element name.",
                     poField->GetNameRef());
            return OGRERR_FAILURE;
        }

        oCleanCopy.SetName(pszName);
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Field name '%s' adjusted to '%s' to be a valid\n"
                 "XML element name.",
                 poField->GetNameRef(), pszName);
    }

    CPLFree(pszName);

    poFeatureDefn->AddGeomFieldDefn(&oCleanCopy);

    return OGRERR_NONE;
}
