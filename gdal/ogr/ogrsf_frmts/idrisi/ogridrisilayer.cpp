/******************************************************************************
 * $Id$
 *
 * Project:  Idrisi Translator
 * Purpose:  Implements OGRIdrisiLayer class.
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_idrisi.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_p.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRIdrisiLayer()                             */
/************************************************************************/

OGRIdrisiLayer::OGRIdrisiLayer( const char* pszFilename,
                                const char* pszLayerName,
                                VSILFILE* fp,
                                OGRwkbGeometryType eGeomType,
                                const char* pszWTKString )

{
    this->fp = fp;
    this->eGeomType = eGeomType;
    nNextFID = 1;
    bEOF = FALSE;
    fpAVL = NULL;

    if (pszWTKString)
    {
        poSRS = new OGRSpatialReference();
        char* pszTmp = (char*)pszWTKString;
        poSRS->importFromWkt(&pszTmp);
    }
    else
        poSRS = NULL;

    poFeatureDefn = new OGRFeatureDefn( pszLayerName );
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
    poFeatureDefn->SetGeomType( eGeomType );

    OGRFieldDefn oFieldDefn("id", OFTReal);
    poFeatureDefn->AddFieldDefn( &oFieldDefn );

    bExtentValid = FALSE;
    dfMinX = dfMinY = dfMaxX = dfMaxY = 0.0;

    VSIFSeekL( fp, 1, SEEK_SET );
    if (VSIFReadL( &nTotalFeatures, sizeof(unsigned int), 1, fp ) != 1)
        nTotalFeatures = 0;
    CPL_LSBPTR32(&nTotalFeatures);

    if (nTotalFeatures != 0)
    {
        if (!Detect_AVL_ADC(pszFilename))
        {
            if( fpAVL != NULL )
                VSIFCloseL( fpAVL );
            fpAVL = NULL;
        }
    }

    ResetReading();
}

/************************************************************************/
/*                          ~OGRIdrisiLayer()                           */
/************************************************************************/

OGRIdrisiLayer::~OGRIdrisiLayer()

{
    if( poSRS != NULL )
        poSRS->Release();

    poFeatureDefn->Release();

    VSIFCloseL( fp );

    if( fpAVL != NULL )
        VSIFCloseL( fpAVL );
}

/************************************************************************/
/*                           Detect_AVL_ADC()                           */
/************************************************************************/

int OGRIdrisiLayer::Detect_AVL_ADC(const char* pszFilename)
{
// --------------------------------------------------------------------
//      Look for .adc file
// --------------------------------------------------------------------
    const char* pszADCFilename = CPLResetExtension(pszFilename, "adc");
    VSILFILE* fpADC = VSIFOpenL(pszADCFilename, "rb");
    if (fpADC == NULL)
    {
        pszADCFilename = CPLResetExtension(pszFilename, "ADC");
        fpADC = VSIFOpenL(pszADCFilename, "rb");
    }

    char** papszADC = NULL;
    if (fpADC != NULL)
    {
        VSIFCloseL(fpADC);
        fpADC = NULL;

        CPLPushErrorHandler(CPLQuietErrorHandler);
        papszADC = CSLLoad2(pszADCFilename, 1024, 256, NULL);
        CPLPopErrorHandler();
        CPLErrorReset();
    }

    if (papszADC == NULL)
        return FALSE;

    CSLSetNameValueSeparator( papszADC, ":" );

    const char *pszVersion = CSLFetchNameValue( papszADC, "file format " );
    if( pszVersion == NULL || !EQUAL( pszVersion, "IDRISI Values A.1" ) )
    {
        CSLDestroy( papszADC );
        return FALSE;
    }

    const char *pszFileType = CSLFetchNameValue( papszADC, "file type   " );
    if( pszFileType == NULL || !EQUAL( pszFileType, "ascii" ) )
    {
        CPLDebug("IDRISI", ".adc file found, but file type != ascii");
        CSLDestroy( papszADC );
        return FALSE;
    }

    const char* pszRecords = CSLFetchNameValue( papszADC, "records     " );
    if( pszRecords == NULL || atoi(pszRecords) != (int)nTotalFeatures )
    {
        CPLDebug("IDRISI", ".adc file found, but 'records' not found or not "
                 "consistant with feature number declared in .vdc");
        CSLDestroy( papszADC );
        return FALSE;
    }

    const char* pszFields = CSLFetchNameValue( papszADC, "fields      " );
    if( pszFields == NULL || atoi(pszFields) <= 1 )
    {
        CPLDebug("IDRISI", ".adc file found, but 'fields' not found or invalid");
        CSLDestroy( papszADC );
        return FALSE;
    }

// --------------------------------------------------------------------
//      Look for .avl file
// --------------------------------------------------------------------
    const char* pszAVLFilename = CPLResetExtension(pszFilename, "avl");
    fpAVL = VSIFOpenL(pszAVLFilename, "rb");
    if (fpAVL == NULL)
    {
        pszAVLFilename = CPLResetExtension(pszFilename, "AVL");
        fpAVL = VSIFOpenL(pszAVLFilename, "rb");
    }
    if (fpAVL == NULL)
    {
        CSLDestroy( papszADC );
        return FALSE;
    }

// --------------------------------------------------------------------
//      Build layer definition
// --------------------------------------------------------------------

    int iCurField;
    char szKey[32];

    iCurField = 0;
    sprintf(szKey, "field %d ", iCurField);

    char** papszIter = papszADC;
    const char* pszLine;
    int bFieldFound = FALSE;
    CPLString osFieldName;
    while((pszLine = *papszIter) != NULL)
    {
        //CPLDebug("IDRISI", "%s", pszLine);
        if (strncmp(pszLine, szKey, strlen(szKey)) == 0)
        {
            const char* pszColon = strchr(pszLine, ':');
            if (pszColon)
            {
                osFieldName = pszColon + 1;
                bFieldFound = TRUE;
            }
        }
        else if (bFieldFound &&
                 strncmp(pszLine, "data type   :", strlen("data type   :")) == 0)
        {
            const char* pszFieldType = pszLine + strlen("data type   :");

            OGRFieldDefn oFieldDefn(osFieldName.c_str(),
                                    EQUAL(pszFieldType, "integer") ? OFTInteger :
                                    EQUAL(pszFieldType, "real") ? OFTReal : OFTString);

            if( iCurField == 0 && oFieldDefn.GetType() != OFTInteger )
            {
                CSLDestroy( papszADC );
                return FALSE;
            }

            if( iCurField != 0 )
                poFeatureDefn->AddFieldDefn( &oFieldDefn );

            iCurField ++;
            sprintf(szKey, "field %d ", iCurField);
        }

        papszIter++;
    }

    CSLDestroy(papszADC);

    return TRUE;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRIdrisiLayer::ResetReading()

{
    nNextFID = 1;
    bEOF = FALSE;
    VSIFSeekL( fp, 0x105, SEEK_SET );
    if( fpAVL != NULL )
        VSIFSeekL( fpAVL, 0, SEEK_SET );
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRIdrisiLayer::GetNextFeature()
{
    OGRFeature  *poFeature;

    while(TRUE)
    {
        if (bEOF)
            return NULL;

        poFeature = GetNextRawFeature();
        if (poFeature == NULL)
        {
            bEOF = TRUE;
            return NULL;
        }

        if((m_poFilterGeom == NULL
            || FilterGeometry( poFeature->GetGeometryRef() ) )
        && (m_poAttrQuery == NULL
            || m_poAttrQuery->Evaluate( poFeature )) )
        {
            return poFeature;
        }
        else
            delete poFeature;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRIdrisiLayer::TestCapability( const char * pszCap )

{
    if (EQUAL(pszCap, OLCFastFeatureCount))
        return m_poFilterGeom == NULL && m_poAttrQuery == NULL;

    if (EQUAL(pszCap, OLCFastGetExtent))
        return bExtentValid;

    return FALSE;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRIdrisiLayer::GetNextRawFeature()
{
    while(TRUE)
    {
        if (eGeomType == wkbPoint)
        {
            double dfId;
            double dfX, dfY;
            if (VSIFReadL(&dfId, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfX, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfY, sizeof(double), 1, fp) != 1)
            {
                return NULL;
            }
            CPL_LSBPTR64(&dfId);
            CPL_LSBPTR64(&dfX);
            CPL_LSBPTR64(&dfY);

            if (m_poFilterGeom != NULL &&
                (dfX < m_sFilterEnvelope.MinX ||
                 dfX > m_sFilterEnvelope.MaxX ||
                 dfY < m_sFilterEnvelope.MinY ||
                 dfY > m_sFilterEnvelope.MaxY))
            {
                nNextFID ++;
                continue;
            }

            OGRPoint* poGeom = new OGRPoint(dfX, dfY);
            if (poSRS)
                poGeom->assignSpatialReference(poSRS);
            OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
            poFeature->SetField(0, dfId);
            poFeature->SetFID(nNextFID ++);
            poFeature->SetGeometryDirectly(poGeom);
            ReadAVLLine(poFeature);
            return poFeature;
        }
        else if (eGeomType == wkbLineString)
        {
            double dfId;
            double dfMinXShape, dfMaxXShape, dfMinYShape, dfMaxYShape;
            unsigned int nNodes;

            if (VSIFReadL(&dfId, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMinXShape, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMaxXShape, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMinYShape, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMaxYShape, sizeof(double), 1, fp) != 1)
            {
                return NULL;
            }
            CPL_LSBPTR64(&dfId);
            CPL_LSBPTR64(&dfMinXShape);
            CPL_LSBPTR64(&dfMaxXShape);
            CPL_LSBPTR64(&dfMinYShape);
            CPL_LSBPTR64(&dfMaxYShape);

            if (VSIFReadL(&nNodes, sizeof(unsigned int), 1, fp) != 1)
            {
                return NULL;
            }
            CPL_LSBPTR32(&nNodes);

            if (nNodes > 100 * 1000 * 1000)
                return NULL;

            if (m_poFilterGeom != NULL &&
                (dfMaxXShape < m_sFilterEnvelope.MinX ||
                 dfMinXShape > m_sFilterEnvelope.MaxX ||
                 dfMaxYShape < m_sFilterEnvelope.MinY ||
                 dfMinYShape > m_sFilterEnvelope.MaxY))
            {
                nNextFID ++;
                VSIFSeekL(fp, sizeof(OGRRawPoint) * nNodes, SEEK_CUR);
                continue;
            }

            OGRRawPoint* poRawPoints = (OGRRawPoint*)VSIMalloc2(sizeof(OGRRawPoint), nNodes);
            if (poRawPoints == NULL)
            {
                return NULL;
            }

            if ((unsigned int)VSIFReadL(poRawPoints, sizeof(OGRRawPoint), nNodes, fp) != nNodes)
            {
                VSIFree(poRawPoints);
                return NULL;
            }

#if defined(CPL_MSB)
            for(unsigned int iNode=0; iNode<nNodes; iNode++)
            {
                CPL_LSBPTR64(&poRawPoints[iNode].x);
                CPL_LSBPTR64(&poRawPoints[iNode].y);
            }
#endif

            OGRLineString* poGeom = new OGRLineString();
            poGeom->setPoints(nNodes, poRawPoints, NULL);

            VSIFree(poRawPoints);

            if (poSRS)
                poGeom->assignSpatialReference(poSRS);
            OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
            poFeature->SetField(0, dfId);
            poFeature->SetFID(nNextFID ++);
            poFeature->SetGeometryDirectly(poGeom);
            ReadAVLLine(poFeature);
            return poFeature;
        }
        else /* if (eGeomType == wkbPolygon) */
        {
            double dfId;
            double dfMinXShape, dfMaxXShape, dfMinYShape, dfMaxYShape;
            unsigned int nParts;
            unsigned int nTotalNodes;

            if (VSIFReadL(&dfId, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMinXShape, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMaxXShape, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMinYShape, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMaxYShape, sizeof(double), 1, fp) != 1)
            {
                return NULL;
            }
            CPL_LSBPTR64(&dfId);
            CPL_LSBPTR64(&dfMinXShape);
            CPL_LSBPTR64(&dfMaxXShape);
            CPL_LSBPTR64(&dfMinYShape);
            CPL_LSBPTR64(&dfMaxYShape);
            if (VSIFReadL(&nParts, sizeof(unsigned int), 1, fp) != 1 ||
                VSIFReadL(&nTotalNodes, sizeof(unsigned int), 1, fp) != 1)
            {
                return NULL;
            }
            CPL_LSBPTR32(&nParts);
            CPL_LSBPTR32(&nTotalNodes);

            if (nParts > 100000 || nTotalNodes > 100 * 1000 * 1000)
                return NULL;

            if (m_poFilterGeom != NULL &&
                (dfMaxXShape < m_sFilterEnvelope.MinX ||
                 dfMinXShape > m_sFilterEnvelope.MaxX ||
                 dfMaxYShape < m_sFilterEnvelope.MinY ||
                 dfMinYShape > m_sFilterEnvelope.MaxY))
            {
                unsigned int iPart;
                for(iPart = 0; iPart < nParts; iPart ++)
                {
                    unsigned int nNodes;
                    if (VSIFReadL(&nNodes, sizeof(unsigned int), 1, fp) != 1)
                        return NULL;
                    CPL_LSBPTR32(&nNodes);
                    if (nNodes > nTotalNodes)
                        return NULL;
                    VSIFSeekL(fp, sizeof(OGRRawPoint) * nNodes, SEEK_CUR);
                }
                nNextFID ++;
                continue;
            }

            OGRRawPoint* poRawPoints = (OGRRawPoint*)VSIMalloc2(sizeof(OGRRawPoint), nTotalNodes);
            if (poRawPoints == NULL)
            {
                return NULL;
            }

            unsigned int iPart;
            OGRPolygon* poGeom = new OGRPolygon();
            for(iPart = 0; iPart < nParts; iPart ++)
            {
                unsigned int nNodes;
                if (VSIFReadL(&nNodes, sizeof(unsigned int), 1, fp) != 1)
                {
                    VSIFree(poRawPoints);
                    delete poGeom;
                    return NULL;
                }
                CPL_LSBPTR32(&nNodes);

                if (nNodes > nTotalNodes ||
                    (unsigned int)VSIFReadL(poRawPoints, sizeof(OGRRawPoint), nNodes, fp) != nNodes)
                {
                    VSIFree(poRawPoints);
                    delete poGeom;
                    return NULL;
                }

#if defined(CPL_MSB)
                for(unsigned int iNode=0; iNode<nNodes; iNode++)
                {
                    CPL_LSBPTR64(&poRawPoints[iNode].x);
                    CPL_LSBPTR64(&poRawPoints[iNode].y);
                }
#endif

                OGRLinearRing* poLR = new OGRLinearRing();
                poGeom->addRingDirectly(poLR);
                poLR->setPoints(nNodes, poRawPoints, NULL);
            }

            VSIFree(poRawPoints);

            if (poSRS)
                poGeom->assignSpatialReference(poSRS);
            OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
            poFeature->SetField(0, dfId);
            poFeature->SetFID(nNextFID ++);
            poFeature->SetGeometryDirectly(poGeom);
            ReadAVLLine(poFeature);
            return poFeature;
        }
    }
}

/************************************************************************/
/*                            ReadAVLLine()                             */
/************************************************************************/

void OGRIdrisiLayer::ReadAVLLine(OGRFeature* poFeature)
{
    if (fpAVL == NULL)
        return;

    const char* pszLine = CPLReadLineL(fpAVL);
    if (pszLine == NULL)
        return;

    char** papszTokens = CSLTokenizeStringComplex(pszLine, "\t", TRUE, TRUE);
    if (CSLCount(papszTokens) == poFeatureDefn->GetFieldCount())
    {
        int nID = atoi(papszTokens[0]);
        if (nID == poFeature->GetFID())
        {
            int i;
            for(i=1;i<poFeatureDefn->GetFieldCount();i++)
            {
                poFeature->SetField(i, papszTokens[i]);
            }
        }
    }
    CSLDestroy(papszTokens);
}

/************************************************************************/
/*                             SetExtent()                              */
/************************************************************************/

void OGRIdrisiLayer::SetExtent(double dfMinX, double dfMinY, double dfMaxX, double dfMaxY)
{
    bExtentValid = TRUE;
    this->dfMinX = dfMinX;
    this->dfMinY = dfMinY;
    this->dfMaxX = dfMaxX;
    this->dfMaxY = dfMaxY;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRIdrisiLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    if (!bExtentValid)
        return OGRLayer::GetExtent(psExtent, bForce);

    psExtent->MinX = dfMinX;
    psExtent->MinY = dfMinY;
    psExtent->MaxX = dfMaxX;
    psExtent->MaxY = dfMaxY;
    return OGRERR_NONE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRIdrisiLayer::GetFeatureCount( int bForce )
{
    if (nTotalFeatures > 0 && m_poFilterGeom == NULL && m_poAttrQuery == NULL)
        return nTotalFeatures;

    return OGRLayer::GetFeatureCount(bForce);
}
