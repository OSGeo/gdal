/******************************************************************************
 *
 * Project:  Idrisi Translator
 * Purpose:  Implements OGRIdrisiLayer class.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at spatialys.com>
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
#include "ogr_idrisi.h"
#include "ogr_p.h"
#include "ogr_srs_api.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRIdrisiLayer()                             */
/************************************************************************/

OGRIdrisiLayer::OGRIdrisiLayer( const char* pszFilename,
                                const char* pszLayerName,
                                VSILFILE* fpIn,
                                OGRwkbGeometryType eGeomTypeIn,
                                const char* pszWTKString ) :
    poFeatureDefn(new OGRFeatureDefn( pszLayerName )),
    poSRS(nullptr),
    eGeomType(eGeomTypeIn),
    fp(fpIn),
    fpAVL(nullptr),
    bEOF(false),
    nNextFID(1),
    bExtentValid(false),
    dfMinX(0.0),
    dfMinY(0.0),
    dfMaxX(0.0),
    dfMaxY(0.0),
    nTotalFeatures(0)
{
    if (pszWTKString)
    {
        poSRS = new OGRSpatialReference();
        poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        poSRS->importFromWkt(pszWTKString);
    }

    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();
    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(poSRS);
    poFeatureDefn->SetGeomType( eGeomType );

    OGRFieldDefn oFieldDefn("id", OFTReal);
    poFeatureDefn->AddFieldDefn( &oFieldDefn );

    VSIFSeekL( fp, 1, SEEK_SET );
    if( VSIFReadL( &nTotalFeatures, sizeof(unsigned int), 1, fp ) != 1 )
        nTotalFeatures = 0;
    CPL_LSBPTR32(&nTotalFeatures);

    if( nTotalFeatures != 0 )
    {
        if( !Detect_AVL_ADC(pszFilename) )
        {
            if( fpAVL != nullptr )
                VSIFCloseL( fpAVL );
            fpAVL = nullptr;
        }
    }

    OGRIdrisiLayer::ResetReading();
}

/************************************************************************/
/*                          ~OGRIdrisiLayer()                           */
/************************************************************************/

OGRIdrisiLayer::~OGRIdrisiLayer()

{
    if( poSRS != nullptr )
        poSRS->Release();

    poFeatureDefn->Release();

    VSIFCloseL( fp );

    if( fpAVL != nullptr )
        VSIFCloseL( fpAVL );
}

/************************************************************************/
/*                           Detect_AVL_ADC()                           */
/************************************************************************/

bool OGRIdrisiLayer::Detect_AVL_ADC( const char* pszFilename )
{
// --------------------------------------------------------------------
//      Look for .adc file
// --------------------------------------------------------------------
    const char* pszADCFilename = CPLResetExtension(pszFilename, "adc");
    VSILFILE* fpADC = VSIFOpenL(pszADCFilename, "rb");
    if( fpADC == nullptr )
    {
        pszADCFilename = CPLResetExtension(pszFilename, "ADC");
        fpADC = VSIFOpenL(pszADCFilename, "rb");
    }

    char** papszADC = nullptr;
    if( fpADC != nullptr )
    {
        VSIFCloseL(fpADC);
        fpADC = nullptr;

        CPLPushErrorHandler(CPLQuietErrorHandler);
        papszADC = CSLLoad2(pszADCFilename, 1024, 256, nullptr);
        CPLPopErrorHandler();
        CPLErrorReset();
    }

    if( papszADC == nullptr )
        return false;

    CSLSetNameValueSeparator( papszADC, ":" );

    const char *pszVersion = CSLFetchNameValue( papszADC, "file format" );
    if( pszVersion == nullptr || !EQUAL( pszVersion, "IDRISI Values A.1" ) )
    {
        CSLDestroy( papszADC );
        return false;
    }

    const char *pszFileType = CSLFetchNameValue( papszADC, "file type" );
    if( pszFileType == nullptr || !EQUAL( pszFileType, "ascii" ) )
    {
        CPLDebug("IDRISI", ".adc file found, but file type != ascii");
        CSLDestroy( papszADC );
        return false;
    }

    const char* pszRecords = CSLFetchNameValue( papszADC, "records" );
    if( pszRecords == nullptr || atoi(pszRecords) != (int)nTotalFeatures )
    {
        CPLDebug("IDRISI", ".adc file found, but 'records' not found or not "
                 "consistent with feature number declared in .vdc");
        CSLDestroy( papszADC );
        return false;
    }

    const char* pszFields = CSLFetchNameValue( papszADC, "fields" );
    if( pszFields == nullptr || atoi(pszFields) <= 1 )
    {
        CPLDebug( "IDRISI",
                  ".adc file found, but 'fields' not found or invalid" );
        CSLDestroy( papszADC );
        return false;
    }

// --------------------------------------------------------------------
//      Look for .avl file
// --------------------------------------------------------------------
    const char* pszAVLFilename = CPLResetExtension(pszFilename, "avl");
    fpAVL = VSIFOpenL(pszAVLFilename, "rb");
    if (fpAVL == nullptr)
    {
        pszAVLFilename = CPLResetExtension(pszFilename, "AVL");
        fpAVL = VSIFOpenL(pszAVLFilename, "rb");
    }
    if (fpAVL == nullptr)
    {
        CSLDestroy( papszADC );
        return false;
    }

// --------------------------------------------------------------------
//      Build layer definition
// --------------------------------------------------------------------

    char szKey[32];
    int iCurField = 0;
    snprintf(szKey, sizeof(szKey), "field %d", iCurField);

    char** papszIter = papszADC;
    const char* pszLine = nullptr;
    bool bFieldFound = false;
    CPLString osFieldName;
    while((pszLine = *papszIter) != nullptr)
    {
        //CPLDebug("IDRISI", "%s", pszLine);
        if (strncmp(pszLine, szKey, strlen(szKey)) == 0)
        {
            const char* pszColon = strchr(pszLine, ':');
            if (pszColon)
            {
                osFieldName = pszColon + 1;
                bFieldFound = true;
            }
        }
        else if (bFieldFound &&
                 STARTS_WITH(pszLine, "data type:"))
        {
            const char* pszFieldType = pszLine + strlen("data type:");

            OGRFieldDefn oFieldDefn(osFieldName.c_str(),
                                    EQUAL(pszFieldType, "integer") ? OFTInteger :
                                    EQUAL(pszFieldType, "real") ? OFTReal : OFTString);

            if( iCurField == 0 && oFieldDefn.GetType() != OFTInteger )
            {
                CSLDestroy( papszADC );
                return false;
            }

            if( iCurField != 0 )
                poFeatureDefn->AddFieldDefn( &oFieldDefn );

            iCurField ++;
            snprintf(szKey, sizeof(szKey), "field %d", iCurField);
        }

        papszIter++;
    }

    CSLDestroy(papszADC);

    return true;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRIdrisiLayer::ResetReading()

{
    nNextFID = 1;
    bEOF = false;
    VSIFSeekL( fp, 0x105, SEEK_SET );
    if( fpAVL != nullptr )
        VSIFSeekL( fpAVL, 0, SEEK_SET );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRIdrisiLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap, OLCFastFeatureCount) )
        return m_poFilterGeom == nullptr && m_poAttrQuery == nullptr;

    if( EQUAL(pszCap, OLCFastGetExtent) )
        return bExtentValid;

    return FALSE;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRIdrisiLayer::GetNextRawFeature()
{
    if( bEOF )
        return nullptr;

    while( true )
    {
        if (eGeomType == wkbPoint)
        {
            double dfId = 0.0;
            double dfX = 0.0;
            double dfY = 0.0;
            if (VSIFReadL(&dfId, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfX, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfY, sizeof(double), 1, fp) != 1)
            {
                return nullptr;
            }
            CPL_LSBPTR64(&dfId);
            CPL_LSBPTR64(&dfX);
            CPL_LSBPTR64(&dfY);

            if (m_poFilterGeom != nullptr &&
                (dfX < m_sFilterEnvelope.MinX ||
                 dfX > m_sFilterEnvelope.MaxX ||
                 dfY < m_sFilterEnvelope.MinY ||
                 dfY > m_sFilterEnvelope.MaxY))
            {
                nNextFID++;
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
            double dfId = 0.0;
            double dfMinXShape = 0.0;
            double dfMaxXShape = 0.0;
            double dfMinYShape = 0.0;
            double dfMaxYShape = 0.0;

            if( VSIFReadL(&dfId, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMinXShape, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMaxXShape, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMinYShape, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMaxYShape, sizeof(double), 1, fp) != 1 )
            {
                bEOF = true;
                return nullptr;
            }
            CPL_LSBPTR64(&dfId);
            CPL_LSBPTR64(&dfMinXShape);
            CPL_LSBPTR64(&dfMaxXShape);
            CPL_LSBPTR64(&dfMinYShape);
            CPL_LSBPTR64(&dfMaxYShape);

            unsigned int nNodes = 0;
            if( VSIFReadL(&nNodes, sizeof(unsigned int), 1, fp) != 1 )
            {
                bEOF = true;
                return nullptr;
            }
            CPL_LSBPTR32(&nNodes);

            if( nNodes > 100 * 1000 * 1000 )
            {
                bEOF = true;
                return nullptr;
            }

            if( m_poFilterGeom != nullptr &&
                (dfMaxXShape < m_sFilterEnvelope.MinX ||
                 dfMinXShape > m_sFilterEnvelope.MaxX ||
                 dfMaxYShape < m_sFilterEnvelope.MinY ||
                 dfMinYShape > m_sFilterEnvelope.MaxY) )
            {
                nNextFID++;
                VSIFSeekL(fp, sizeof(OGRRawPoint) * nNodes, SEEK_CUR);
                continue;
            }

            OGRRawPoint* poRawPoints = static_cast<OGRRawPoint *>(
                VSI_MALLOC2_VERBOSE(sizeof(OGRRawPoint), nNodes) );
            if (poRawPoints == nullptr)
            {
                bEOF = true;
                return nullptr;
            }

            if( static_cast<unsigned int>(VSIFReadL(
                    poRawPoints, sizeof(OGRRawPoint), nNodes, fp)) != nNodes )
            {
                VSIFree(poRawPoints);
                bEOF = true;
                return nullptr;
            }

#if defined(CPL_MSB)
            for( unsigned int iNode=0; iNode<nNodes; iNode++ )
            {
                CPL_LSBPTR64(&poRawPoints[iNode].x);
                CPL_LSBPTR64(&poRawPoints[iNode].y);
            }
#endif

            OGRLineString* poGeom = new OGRLineString();
            poGeom->setPoints(nNodes, poRawPoints, nullptr);

            VSIFree(poRawPoints);

            if( poSRS )
                poGeom->assignSpatialReference(poSRS);
            OGRFeature* poFeature = new OGRFeature(poFeatureDefn);
            poFeature->SetField(0, dfId);
            poFeature->SetFID(nNextFID ++);
            poFeature->SetGeometryDirectly(poGeom);
            ReadAVLLine(poFeature);
            return poFeature;
        }
        else  // if (eGeomType == wkbPolygon)
        {
            double dfId = 0.0;
            double dfMinXShape = 0.0;
            double dfMaxXShape = 0.0;
            double dfMinYShape = 0.0;
            double dfMaxYShape = 0.0;

            if (VSIFReadL(&dfId, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMinXShape, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMaxXShape, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMinYShape, sizeof(double), 1, fp) != 1 ||
                VSIFReadL(&dfMaxYShape, sizeof(double), 1, fp) != 1)
            {
                bEOF = true;
                return nullptr;
            }
            CPL_LSBPTR64(&dfId);
            CPL_LSBPTR64(&dfMinXShape);
            CPL_LSBPTR64(&dfMaxXShape);
            CPL_LSBPTR64(&dfMinYShape);
            CPL_LSBPTR64(&dfMaxYShape);
            unsigned int nParts = 0;
            unsigned int nTotalNodes = 0;
            if (VSIFReadL(&nParts, sizeof(unsigned int), 1, fp) != 1 ||
                VSIFReadL(&nTotalNodes, sizeof(unsigned int), 1, fp) != 1)
            {
                bEOF = true;
                return nullptr;
            }
            CPL_LSBPTR32(&nParts);
            CPL_LSBPTR32(&nTotalNodes);

            if (nParts > 100000 || nTotalNodes > 100 * 1000 * 1000)
            {
                bEOF = true;
                return nullptr;
            }

            if (m_poFilterGeom != nullptr &&
                (dfMaxXShape < m_sFilterEnvelope.MinX ||
                 dfMinXShape > m_sFilterEnvelope.MaxX ||
                 dfMaxYShape < m_sFilterEnvelope.MinY ||
                 dfMinYShape > m_sFilterEnvelope.MaxY))
            {
                VSIFSeekL(fp, sizeof(unsigned int) * nParts
                          + sizeof(OGRRawPoint) * nTotalNodes, SEEK_CUR);
                nNextFID ++;
                continue;
            }

            OGRRawPoint* poRawPoints = static_cast<OGRRawPoint *>(
                VSI_MALLOC2_VERBOSE(sizeof(OGRRawPoint), nTotalNodes) );
            if (poRawPoints == nullptr)
            {
                bEOF = true;
                return nullptr;
            }
            unsigned int* panNodesCount = nullptr;
            if( nParts > 1 )
            {
                panNodesCount = static_cast<unsigned int *>(
                    CPLMalloc( sizeof(unsigned int) * nParts ) );
                if (VSIFReadL(panNodesCount, sizeof(unsigned int) * nParts, 1,
                              fp) != 1)
                {
                    VSIFree(poRawPoints);
                    VSIFree(panNodesCount);
                    bEOF = true;
                    return nullptr;
                }
#if defined(CPL_MSB)
                for(unsigned int iPart=0; iPart < nParts; iPart ++)
                {
                    CPL_LSBPTR32(&panNodesCount[iPart]);
                }
#endif
            }
            else
            {
                unsigned int nNodes = 0;
                if (VSIFReadL(&nNodes, sizeof(unsigned int) * nParts, 1, fp) != 1)
                {
                    VSIFree(poRawPoints);
                    bEOF = true;
                    return nullptr;
                }
                CPL_LSBPTR32(&nNodes);
                if( nNodes != nTotalNodes )
                {
                    VSIFree(poRawPoints);
                    bEOF = true;
                    return nullptr;
                }
            }

            OGRPolygon* poGeom = new OGRPolygon();
            for( unsigned int iPart = 0; iPart < nParts; iPart++ )
            {
                unsigned int nNodes
                    = (nParts > 1) ? panNodesCount[iPart] : nTotalNodes;
                if( nNodes > nTotalNodes ||
                    static_cast<unsigned int>(
                        VSIFReadL(poRawPoints, sizeof(OGRRawPoint), nNodes, fp))
                    != nNodes )
                {
                    VSIFree(poRawPoints);
                    VSIFree(panNodesCount);
                    delete poGeom;
                    bEOF = true;
                    return nullptr;
                }

#if defined(CPL_MSB)
                for( unsigned int iNode=0; iNode<nNodes; iNode++ )
                {
                    CPL_LSBPTR64(&poRawPoints[iNode].x);
                    CPL_LSBPTR64(&poRawPoints[iNode].y);
                }
#endif

                OGRLinearRing* poLR = new OGRLinearRing();
                poGeom->addRingDirectly(poLR);
                poLR->setPoints(nNodes, poRawPoints, nullptr);
            }

            VSIFree(poRawPoints);
            VSIFree(panNodesCount);

            if( poSRS )
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
    if( fpAVL == nullptr )
        return;

    const char* pszLine = CPLReadLineL(fpAVL);
    if( pszLine == nullptr )
        return;

    char** papszTokens = CSLTokenizeStringComplex(pszLine, "\t", TRUE, TRUE);
    if( CSLCount(papszTokens) == poFeatureDefn->GetFieldCount() )
    {
        const int nID = atoi(papszTokens[0]);
        if( nID == poFeature->GetFID() )
        {
            for( int i = 1; i < poFeatureDefn->GetFieldCount(); i++ )
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

void OGRIdrisiLayer::SetExtent( double dfMinXIn, double dfMinYIn,
                                double dfMaxXIn, double dfMaxYIn )
{
    bExtentValid = true;
    dfMinX = dfMinXIn;
    dfMinY = dfMinYIn;
    dfMaxX = dfMaxXIn;
    dfMaxY = dfMaxYIn;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRIdrisiLayer::GetExtent(OGREnvelope *psExtent, int bForce)
{
    if( !bExtentValid )
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

GIntBig OGRIdrisiLayer::GetFeatureCount( int bForce )
{
    if( nTotalFeatures > 0 && m_poFilterGeom == nullptr && m_poAttrQuery == nullptr )
        return nTotalFeatures;

    return OGRLayer::GetFeatureCount(bForce);
}
