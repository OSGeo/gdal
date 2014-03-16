/******************************************************************************
 * $Id$
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

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRGMLLayer()                              */
/************************************************************************/

OGRGMLLayer::OGRGMLLayer( const char * pszName,
                          int bWriterIn,
                          OGRGMLDataSource *poDSIn )

{
    iNextGMLId = 0;
    nTotalGMLCount = -1;
    bInvalidFIDFound = FALSE;
    pszFIDPrefix = NULL;
    bFaceHoleNegative = FALSE;
        
    poDS = poDSIn;

    if ( EQUALN(pszName, "ogr:", 4) )
      poFeatureDefn = new OGRFeatureDefn( pszName+4 );
    else
      poFeatureDefn = new OGRFeatureDefn( pszName );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( wkbNone );

    bWriter = bWriterIn;
    bSameSRS = FALSE;

/* -------------------------------------------------------------------- */
/*      Reader's should get the corresponding GMLFeatureClass and       */
/*      cache it.                                                       */
/* -------------------------------------------------------------------- */
    if( !bWriter )
        poFClass = poDS->GetReader()->GetClass( pszName );
    else
        poFClass = NULL;

    hCacheSRS = GML_BuildOGRGeometryFromList_CreateCache();

    /* Compatibility option. Not advertized, because hopefully won't be needed */
    /* Just put here in provision... */
    bUseOldFIDFormat = CSLTestBoolean(CPLGetConfigOption("GML_USE_OLD_FID_FORMAT", "FALSE"));

    /* Must be in synced in OGR_G_CreateFromGML(), OGRGMLLayer::OGRGMLLayer() and GMLReader::GMLReader() */
    bFaceHoleNegative = CSLTestBoolean(CPLGetConfigOption("GML_FACE_HOLE_NEGATIVE", "NO"));

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
        /* Does the last stored feature belong to our layer ? If so, no */
        /* need to reset the reader */
        if (iNextGMLId == 0 && poDS->PeekStoredGMLFeature() != NULL &&
            poDS->PeekStoredGMLFeature()->GetClass() == poFClass)
            return;

        delete poDS->PeekStoredGMLFeature();
        poDS->SetStoredGMLFeature(NULL);
    }

    iNextGMLId = 0;
    poDS->GetReader()->ResetReading();
    CPLDebug("GML", "ResetReading()");
    if (poDS->GetLayerCount() > 1 && poDS->GetReadMode() == STANDARD)
        poDS->GetReader()->SetFilteredClassName(poFClass->GetName());
}

/************************************************************************/
/*                     ConvertGeomToMultiIfNecessary()                  */
/************************************************************************/

OGRGeometry* OGRGMLLayer::ConvertGeomToMultiIfNecessary(OGRGeometry* poGeom)
{
    OGRwkbGeometryType eType = poGeom->getGeometryType();
    OGRwkbGeometryType eLayerType = GetGeomType();
    OGRGeometryCollection* poNewGeom = NULL;
    if (eType == wkbPoint && eLayerType == wkbMultiPoint)
    {
        poNewGeom = new OGRMultiPoint();
    }
    else if (eType == wkbLineString && eLayerType == wkbMultiLineString)
    {
        poNewGeom = new OGRMultiLineString();
    }
    else if (eType == wkbPolygon && eLayerType == wkbMultiPolygon)
    {
        poNewGeom = new OGRMultiPolygon();
    }

    if( poNewGeom != NULL )
    {
        OGRSpatialReference* poGeomSRS = poGeom->getSpatialReference();
        poNewGeom->addGeometryDirectly(poGeom);
        if( poGeomSRS != NULL )
            poNewGeom->assignSpatialReference(poGeomSRS);
        poGeom = poNewGeom;
    }

    return poGeom;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGMLLayer::GetNextFeature()

{
    int i;

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
    while( TRUE )
    {
        GMLFeature  *poGMLFeature = NULL;
        OGRGeometry *poGeom = NULL;

        poGMLFeature = poDS->PeekStoredGMLFeature();
        if (poGMLFeature != NULL)
            poDS->SetStoredGMLFeature(NULL);
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
        int nFID = -1;
        const char * pszGML_FID = poGMLFeature->GetFID();
        if( bInvalidFIDFound )
        {
            nFID = iNextGMLId++;
        }
        else if( pszGML_FID == NULL )
        {
            bInvalidFIDFound = TRUE;
            nFID = iNextGMLId++;
        }
        else if( iNextGMLId == 0 )
        {
            int j = 0;
            i = strlen( pszGML_FID )-1;
            while( i >= 0 && pszGML_FID[i] >= '0'
                          && pszGML_FID[i] <= '9' && j<8)
                i--, j++;
            /* i points the last character of the fid */
            if( i >= 0 && j < 8 && pszFIDPrefix == NULL)
            {
                pszFIDPrefix = (char *) CPLMalloc(i+2);
                pszFIDPrefix[i+1] = '\0';
                strncpy(pszFIDPrefix, pszGML_FID, i+1);
            }
            /* pszFIDPrefix now contains the prefix or NULL if no prefix is found */
            if( j < 8 && sscanf(pszGML_FID+i+1, "%d", &nFID)==1)
            {
                if( iNextGMLId <= nFID )
                    iNextGMLId = nFID + 1;
            }
            else
            {
                bInvalidFIDFound = TRUE;
                nFID = iNextGMLId++;
            }
        }
        else if( iNextGMLId != 0 )
        {
            const char* pszFIDPrefix_notnull = pszFIDPrefix;
            if (pszFIDPrefix_notnull == NULL) pszFIDPrefix_notnull = "";
            int nLenPrefix = strlen(pszFIDPrefix_notnull);

            if(  strncmp(pszGML_FID, pszFIDPrefix_notnull, nLenPrefix) == 0 &&
                 strlen(pszGML_FID+nLenPrefix) <= 9 &&
                 sscanf(pszGML_FID+nLenPrefix, "%d", &nFID) == 1 )
            { /* fid with the prefix. Using its numerical part */
                if( iNextGMLId < nFID )
                    iNextGMLId = nFID + 1;
            }
            else
            { /* fid without the aforementioned prefix or a valid numerical part */
                bInvalidFIDFound = TRUE;
                nFID = iNextGMLId++;
            }
        }

/* -------------------------------------------------------------------- */
/*      Does it satisfy the spatial query, if there is one?             */
/* -------------------------------------------------------------------- */

        OGRGeometry** papoGeometries = NULL;
        const CPLXMLNode* const * papsGeometry = poGMLFeature->GetGeometryList();

        if( poFeatureDefn->GetGeomFieldCount() > 1 )
        {
            papoGeometries = (OGRGeometry**)
                CPLCalloc( poFeatureDefn->GetGeomFieldCount(), sizeof(OGRGeometry*) );
            const char* pszSRSName = poDS->GetGlobalSRSName();
            for( i=0; i < poFeatureDefn->GetGeomFieldCount(); i++)
            {
                const CPLXMLNode* psGeom = poGMLFeature->GetGeometryRef(i);
                if( psGeom != NULL )
                {
                    const CPLXMLNode* myGeometryList[2];
                    myGeometryList[0] = psGeom;
                    myGeometryList[1] = NULL;
                    poGeom = GML_BuildOGRGeometryFromList(myGeometryList, TRUE,
                                                  poDS->GetInvertAxisOrderIfLatLong(),
                                                  pszSRSName,
                                                  poDS->GetConsiderEPSGAsURN(),
                                                  poDS->GetSecondaryGeometryOption(),
                                                  hCacheSRS,
                                                  bFaceHoleNegative );

                    /* Force single geometry to multigeometry if needed to match layer geometry type */
                    if (poGeom != NULL)
                    {
                        papoGeometries[i] = ConvertGeomToMultiIfNecessary(poGeom);
                        poGeom = NULL;
                    }
                    else
                    // We assume the createFromGML() function would have already
                    // reported the error.
                    {
                        for( i=0; i < poFeatureDefn->GetGeomFieldCount(); i++)
                        {
                            delete papoGeometries[i];
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
                for( i=0; i < poFeatureDefn->GetGeomFieldCount(); i++)
                {
                    delete papoGeometries[i];
                }
                CPLFree(papoGeometries);
                delete poGMLFeature;
                continue;
            }
        }
        else if (papsGeometry[0] != NULL)
        {
            const char* pszSRSName = poDS->GetGlobalSRSName();
            poGeom = GML_BuildOGRGeometryFromList(papsGeometry, TRUE,
                                                  poDS->GetInvertAxisOrderIfLatLong(),
                                                  pszSRSName,
                                                  poDS->GetConsiderEPSGAsURN(),
                                                  poDS->GetSecondaryGeometryOption(),
                                                  hCacheSRS,
                                                  bFaceHoleNegative );

            /* Force single geometry to multigeometry if needed to match layer geometry type */
            if (poGeom != NULL)
            {
                poGeom = ConvertGeomToMultiIfNecessary(poGeom);
            }
            else
            // We assume the createFromGML() function would have already
            // reported the error.
            {
                delete poGMLFeature;
                return NULL;
            }

            if( m_poFilterGeom != NULL && !FilterGeometry( poGeom ) )
            {
                delete poGMLFeature;
                delete poGeom;
                continue;
            }
        }

        
/* -------------------------------------------------------------------- */
/*      Convert the whole feature into an OGRFeature.                   */
/* -------------------------------------------------------------------- */
        int iField;
        int iDstField = 0;
        OGRFeature *poOGRFeature = new OGRFeature( poFeatureDefn );

        poOGRFeature->SetFID( nFID );
        if (poDS->ExposeId())
        {
            if (pszGML_FID)
                poOGRFeature->SetField( iDstField, pszGML_FID );
            iDstField ++;
        }

        int nPropertyCount = poFClass->GetPropertyCount();
        for( iField = 0; iField < nPropertyCount; iField++, iDstField ++ )
        {
            const GMLProperty *psGMLProperty = poGMLFeature->GetProperty( iField );
            if( psGMLProperty == NULL || psGMLProperty->nSubProperties == 0 )
                continue;

            switch( poFClass->GetProperty(iField)->GetType()  )
            {
              case GMLPT_Real:
              {
                  poOGRFeature->SetField( iDstField, CPLAtof(psGMLProperty->papszSubProperties[0]) );
              }
              break;

              case GMLPT_IntegerList:
              {
                  int nCount = psGMLProperty->nSubProperties;
                  int *panIntList = (int *) CPLMalloc(sizeof(int) * nCount );

                  for( i = 0; i < nCount; i++ )
                      panIntList[i] = atoi(psGMLProperty->papszSubProperties[i]);

                  poOGRFeature->SetField( iDstField, nCount, panIntList );
                  CPLFree( panIntList );
              }
              break;

              case GMLPT_RealList:
              {
                  int nCount = psGMLProperty->nSubProperties;
                  double *padfList = (double *)CPLMalloc(sizeof(double)*nCount);

                  for( i = 0; i < nCount; i++ )
                      padfList[i] = CPLAtof(psGMLProperty->papszSubProperties[i]);

                  poOGRFeature->SetField( iDstField, nCount, padfList );
                  CPLFree( padfList );
              }
              break;

              case GMLPT_StringList:
              case GMLPT_FeaturePropertyList:
              {
                  poOGRFeature->SetField( iDstField, psGMLProperty->papszSubProperties );
              }
              break;

              default:
                poOGRFeature->SetField( iDstField, psGMLProperty->papszSubProperties[0] );
                break;
            }
        }

        delete poGMLFeature;
        poGMLFeature = NULL;

        /* Assign the geometry before the attribute filter because */
        /* the attribute filter may use a special field like OGR_GEOMETRY */
        if( papoGeometries != NULL )
        {
            for( i=0; i < poFeatureDefn->GetGeomFieldCount(); i++)
            {
                poOGRFeature->SetGeomFieldDirectly( i, papoGeometries[i] );
            }
            CPLFree(papoGeometries);
            papoGeometries = NULL;
        }
        else
            poOGRFeature->SetGeometryDirectly( poGeom );

        /* Assign SRS */
        for( i=0; i < poFeatureDefn->GetGeomFieldCount(); i++)
        {
            poGeom = poOGRFeature->GetGeomFieldRef(i);
            if( poGeom != NULL )
            {
                OGRSpatialReference* poSRS = poFeatureDefn->GetGeomFieldDefn(i)->GetSpatialRef();
                if (poSRS != NULL)
                    poGeom->assignSpatialReference(poSRS);
            }
        }
        
/* -------------------------------------------------------------------- */
/*      Test against the attribute query.                               */
/* -------------------------------------------------------------------- */
        if( m_poAttrQuery != NULL
            && !m_poAttrQuery->Evaluate( poOGRFeature ) )
        {
            delete poOGRFeature;
            continue;
        }

/* -------------------------------------------------------------------- */
/*      Wow, we got our desired feature. Return it.                     */
/* -------------------------------------------------------------------- */

        return poOGRFeature;
    }

    return NULL;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRGMLLayer::GetFeatureCount( int bForce )

{
    if( poFClass == NULL )
        return 0;

    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL )
        return OGRLayer::GetFeatureCount( bForce );
    else
    {
        /* If the schema is read from a .xsd file, we haven't read */
        /* the feature count, so compute it now */
        int nFeatureCount = poFClass->GetFeatureCount();
        if (nFeatureCount < 0)
        {
            nFeatureCount = OGRLayer::GetFeatureCount(bForce);
            poFClass->SetFeatureCount(nFeatureCount);
        }
        return nFeatureCount;
    }
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRGMLLayer::GetExtent(OGREnvelope *psExtent, int bForce )

{
    double dfXMin, dfXMax, dfYMin, dfYMax;

    if( GetGeomType() == wkbNone )
        return OGRERR_FAILURE;

    if( poFClass != NULL && 
        poFClass->GetExtents( &dfXMin, &dfXMax, &dfYMin, &dfYMax ) )
    {
        psExtent->MinX = dfXMin;
        psExtent->MaxX = dfXMax;
        psExtent->MinY = dfYMin;
        psExtent->MaxY = dfYMax;

        return OGRERR_NONE;
    }
    else 
        return OGRLayer::GetExtent( psExtent, bForce );
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

static void GMLWriteField(OGRGMLDataSource* poDS,
                          VSILFILE *fp,
                          int bWriteSpaceIndentation,
                          const char* pszPrefix,
                          int bRemoveAppPrefix,
                          OGRFieldDefn* poFieldDefn,
                          const char* pszVal )

{
    const char* pszFieldName = poFieldDefn->GetNameRef();

    while( *pszVal == ' ' )
        pszVal++;

    if (bWriteSpaceIndentation)
        VSIFPrintfL(fp, "      ");

    if( bRemoveAppPrefix )
        poDS->PrintLine( fp, "<%s>%s</%s>",
                        pszFieldName,
                        pszVal,
                        pszFieldName);
    else
        poDS->PrintLine( fp, "<%s:%s>%s</%s:%s>",
                        pszPrefix,
                        pszFieldName,
                        pszVal,
                        pszPrefix,
                        pszFieldName);
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRGMLLayer::CreateFeature( OGRFeature *poFeature )

{
    int bIsGML3Output = poDS->IsGML3Output();
    VSILFILE *fp = poDS->GetOutputFP();
    int bWriteSpaceIndentation = poDS->WriteSpaceIndentation();
    const char* pszPrefix = poDS->GetAppPrefix();
    int bRemoveAppPrefix = poDS->RemoveAppPrefix();

    if( !bWriter )
        return OGRERR_FAILURE;

    if (bWriteSpaceIndentation)
        VSIFPrintfL(fp, "  ");
    if (bIsGML3Output)
    {
        if( bRemoveAppPrefix ) 
            poDS->PrintLine( fp, "<featureMember>" );
        else
            poDS->PrintLine( fp, "<%s:featureMember>", pszPrefix );
    }
    else
        poDS->PrintLine( fp, "<gml:featureMember>" );

    if( iNextGMLId == 0 )
    {
        bSameSRS = TRUE;
        for( int iGeomField = 1; iGeomField < poFeatureDefn->GetGeomFieldCount(); iGeomField++ )
        {
            OGRGeomFieldDefn *poFieldDefn0 = poFeatureDefn->GetGeomFieldDefn(0);
            OGRGeomFieldDefn *poFieldDefn = poFeatureDefn->GetGeomFieldDefn(iGeomField);
            OGRSpatialReference* poSRS0 = poFieldDefn0->GetSpatialRef();
            OGRSpatialReference* poSRS = poFieldDefn->GetSpatialRef();
            if( poSRS0 != NULL && poSRS == NULL )
                bSameSRS = FALSE;
            else if( poSRS0 == NULL && poSRS != NULL )
                bSameSRS = FALSE;
            else if( poSRS0 != NULL && poSRS != NULL &&
                     poSRS0 != poSRS && !poSRS0->IsSame(poSRS) )
            {
                bSameSRS = FALSE;
            }
        }
    }

    if( poFeature->GetFID() == OGRNullFID )
        poFeature->SetFID( iNextGMLId++ );

    int nGMLIdIndex = -1;
    if (bWriteSpaceIndentation)
        VSIFPrintfL(fp, "    ");
    VSIFPrintfL(fp, "<");
    if( !bRemoveAppPrefix ) 
        VSIFPrintfL(fp, "%s:", pszPrefix);
    if (bIsGML3Output)
    {
        nGMLIdIndex = poFeatureDefn->GetFieldIndex("gml_id");
        if (nGMLIdIndex >= 0 && poFeature->IsFieldSet( nGMLIdIndex ) )
            poDS->PrintLine( fp, "%s gml:id=\"%s\">",
                             poFeatureDefn->GetName(),
                             poFeature->GetFieldAsString(nGMLIdIndex) );
        else
            poDS->PrintLine( fp, "%s gml:id=\"%s.%ld\">",
                             poFeatureDefn->GetName(),
                             poFeatureDefn->GetName(),
                             poFeature->GetFID() );
    }
    else
    {
        nGMLIdIndex = poFeatureDefn->GetFieldIndex("fid");
        if (bUseOldFIDFormat)
        {
            poDS->PrintLine( fp, "%s fid=\"F%ld\">",
                             poFeatureDefn->GetName(),
                             poFeature->GetFID() );
        }
        else if (nGMLIdIndex >= 0 && poFeature->IsFieldSet( nGMLIdIndex ) )
        {
            poDS->PrintLine( fp, "%s fid=\"%s\">",
                             poFeatureDefn->GetName(),
                             poFeature->GetFieldAsString(nGMLIdIndex) );
        }
        else
        {
            poDS->PrintLine( fp, "%s fid=\"%s.%ld\">",
                             poFeatureDefn->GetName(),
                             poFeatureDefn->GetName(),
                             poFeature->GetFID() );
        }
    }


    for( int iGeomField = 0; iGeomField < poFeatureDefn->GetGeomFieldCount(); iGeomField++ )
    {
        OGRGeomFieldDefn *poFieldDefn = poFeatureDefn->GetGeomFieldDefn(iGeomField);

        // Write out Geometry - for now it isn't indented properly.
        /* GML geometries don't like very much the concept of empty geometry */
        OGRGeometry* poGeom = poFeature->GetGeomFieldRef(iGeomField);
        if( poGeom != NULL && !poGeom->IsEmpty())
        {
            char    *pszGeometry;
            OGREnvelope3D sGeomBounds;

            int nCoordDimension = poGeom->getCoordinateDimension();

            poGeom->getEnvelope( &sGeomBounds );
            if( bSameSRS )
                poDS->GrowExtents( &sGeomBounds, nCoordDimension );

            if (poGeom->getSpatialReference() == NULL && poFieldDefn->GetSpatialRef() != NULL)
                poGeom->assignSpatialReference(poFieldDefn->GetSpatialRef());

            if (bIsGML3Output && poDS->WriteFeatureBoundedBy())
            {
                int bCoordSwap;

                char* pszSRSName = GML_GetSRSName(poGeom->getSpatialReference(), poDS->IsLongSRSRequired(), &bCoordSwap);
                char szLowerCorner[75], szUpperCorner[75];
                if (bCoordSwap)
                {
                    OGRMakeWktCoordinate(szLowerCorner, sGeomBounds.MinY, sGeomBounds.MinX, sGeomBounds.MinZ, nCoordDimension);
                    OGRMakeWktCoordinate(szUpperCorner, sGeomBounds.MaxY, sGeomBounds.MaxX, sGeomBounds.MaxZ, nCoordDimension);
                }
                else
                {
                    OGRMakeWktCoordinate(szLowerCorner, sGeomBounds.MinX, sGeomBounds.MinY, sGeomBounds.MinZ, nCoordDimension);
                    OGRMakeWktCoordinate(szUpperCorner, sGeomBounds.MaxX, sGeomBounds.MaxY, sGeomBounds.MaxZ, nCoordDimension);
                }
                if (bWriteSpaceIndentation)
                    VSIFPrintfL(fp, "      ");
                poDS->PrintLine( fp, "<gml:boundedBy><gml:Envelope%s%s><gml:lowerCorner>%s</gml:lowerCorner><gml:upperCorner>%s</gml:upperCorner></gml:Envelope></gml:boundedBy>",
                                (nCoordDimension == 3) ? " srsDimension=\"3\"" : "",pszSRSName, szLowerCorner, szUpperCorner);
                CPLFree(pszSRSName);
            }

            char** papszOptions = (bIsGML3Output) ? CSLAddString(NULL, "FORMAT=GML3") : NULL;
            if (bIsGML3Output && !poDS->IsLongSRSRequired())
                papszOptions = CSLAddString(papszOptions, "GML3_LONGSRS=NO");
            if (poDS->IsGML32Output())
            {
                if( poFeatureDefn->GetGeomFieldCount() > 1 )
                    papszOptions = CSLAddString(papszOptions,
                        CPLSPrintf("GMLID=%s.%s.%ld",
                                   poFeatureDefn->GetName(),
                                   poFieldDefn->GetNameRef(),
                                   poFeature->GetFID()));
                else
                    papszOptions = CSLAddString(papszOptions,
                        CPLSPrintf("GMLID=%s.geom.%ld",
                                   poFeatureDefn->GetName(), poFeature->GetFID()));
            }
            pszGeometry = poGeom->exportToGML(papszOptions);
            CSLDestroy(papszOptions);
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
            CPLFree( pszGeometry );
        }
    }

    // Write all "set" fields. 
    for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        
        OGRFieldDefn *poFieldDefn = poFeatureDefn->GetFieldDefn( iField );

        if( poFeature->IsFieldSet( iField ) && iField != nGMLIdIndex )
        {
            if (poFieldDefn->GetType() == OFTStringList )
            {
                char ** papszIter =  poFeature->GetFieldAsStringList( iField );
                while( papszIter != NULL && *papszIter != NULL )
                {
                    char *pszEscaped = OGRGetXML_UTF8_EscapedString( *papszIter );
                    GMLWriteField(poDS, fp, bWriteSpaceIndentation, pszPrefix,
                                  bRemoveAppPrefix, poFieldDefn, pszEscaped);
                    CPLFree( pszEscaped );

                    papszIter ++;
                }
            }
            else if (poFieldDefn->GetType() == OFTIntegerList )
            {
                int nCount = 0;
                const int* panVals = poFeature->GetFieldAsIntegerList( iField, &nCount );
                for(int i = 0; i < nCount; i++)
                {
                    GMLWriteField(poDS, fp, bWriteSpaceIndentation, pszPrefix,
                                  bRemoveAppPrefix, poFieldDefn, CPLSPrintf("%d", panVals[i]));
                }
            }
            else if (poFieldDefn->GetType() == OFTRealList )
            {
                int nCount = 0;
                const double* padfVals = poFeature->GetFieldAsDoubleList( iField, &nCount );
                for(int i = 0; i < nCount; i++)
                {
                    char szBuffer[80];
                    snprintf( szBuffer, sizeof(szBuffer), "%.15g", padfVals[i]);
                    /* Use point as decimal separator */
                    char* pszComma = strchr(szBuffer, ',');
                    if (pszComma)
                        *pszComma = '.';
                    GMLWriteField(poDS, fp, bWriteSpaceIndentation, pszPrefix,
                                  bRemoveAppPrefix, poFieldDefn, szBuffer);
                }
            }
            else
            {
                const char *pszRaw = poFeature->GetFieldAsString( iField );

                char *pszEscaped = OGRGetXML_UTF8_EscapedString( pszRaw );

                if (poFieldDefn->GetType() == OFTReal)
                {
                    /* Use point as decimal separator */
                    char* pszComma = strchr(pszEscaped, ',');
                    if (pszComma)
                        *pszComma = '.';
                }

                GMLWriteField(poDS, fp, bWriteSpaceIndentation, pszPrefix,
                              bRemoveAppPrefix, poFieldDefn, pszEscaped);
                CPLFree( pszEscaped );
            }
        }
    }

    if (bWriteSpaceIndentation)
        VSIFPrintfL(fp, "    ");
    if( bRemoveAppPrefix )
        poDS->PrintLine( fp, "</%s>", poFeatureDefn->GetName() );
    else
        poDS->PrintLine( fp, "</%s:%s>", pszPrefix, poFeatureDefn->GetName() );
    if (bWriteSpaceIndentation)
        VSIFPrintfL(fp, "  ");
    if (bIsGML3Output)
    {
        if( bRemoveAppPrefix )
            poDS->PrintLine( fp, "</featureMember>" );
        else
            poDS->PrintLine( fp, "</%s:featureMember>", pszPrefix );
    }
    else
        poDS->PrintLine( fp, "</gml:featureMember>" );

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRGMLLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCSequentialWrite) )
        return bWriter;

    else if( EQUAL(pszCap,OLCCreateField) )
        return bWriter && iNextGMLId == 0;

    else if( EQUAL(pszCap,OLCCreateGeomField) )
        return bWriter && iNextGMLId == 0;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
    {
        double  dfXMin, dfXMax, dfYMin, dfYMax;

        if( poFClass == NULL )
            return FALSE;

        return poFClass->GetExtents( &dfXMin, &dfXMax, &dfYMin, &dfYMax );
    }

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
    {
        if( poFClass == NULL 
            || m_poFilterGeom != NULL 
            || m_poAttrQuery != NULL )
            return FALSE;

        return poFClass->GetFeatureCount() != -1;
    }

    else if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;

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
    OGRFieldDefn oCleanCopy( poField );
    char *pszName = CPLStrdup( poField->GetNameRef() );
    CPLCleanXMLElementName( pszName );
    
    if( strcmp(pszName,poField->GetNameRef()) != 0 )
    {
        if( !bApproxOK )
        {
            CPLFree( pszName );
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to create field with name '%s', it would not\n"
                      "be valid as an XML element name.",
                      poField->GetNameRef() );
            return OGRERR_FAILURE;
        }

        oCleanCopy.SetName( pszName );
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Field name '%s' adjusted to '%s' to be a valid\n"
                  "XML element name.",
                  poField->GetNameRef(), pszName );
    }

    CPLFree( pszName );

    
    poFeatureDefn->AddFieldDefn( &oCleanCopy );

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
    OGRGeomFieldDefn oCleanCopy( poField );
    char *pszName = CPLStrdup( poField->GetNameRef() );
    CPLCleanXMLElementName( pszName );
    
    if( strcmp(pszName,poField->GetNameRef()) != 0 )
    {
        if( !bApproxOK )
        {
            CPLFree( pszName );
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "Unable to create field with name '%s', it would not\n"
                      "be valid as an XML element name.",
                      poField->GetNameRef() );
            return OGRERR_FAILURE;
        }

        oCleanCopy.SetName( pszName );
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Field name '%s' adjusted to '%s' to be a valid\n"
                  "XML element name.",
                  poField->GetNameRef(), pszName );
    }

    CPLFree( pszName );

    
    poFeatureDefn->AddGeomFieldDefn( &oCleanCopy );

    return OGRERR_NONE;
}
