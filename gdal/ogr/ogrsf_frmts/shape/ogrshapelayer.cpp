/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRShapeLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogrshape.h"

#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_time.h"
#include "ogr_p.h"

#include <algorithm>

static const char UNSUPPORTED_OP_READ_ONLY[] =
    "%s : unsupported operation on a read-only datasource.";

CPL_CVSID("$Id$")

/************************************************************************/
/*                           OGRShapeLayer()                            */
/************************************************************************/

OGRShapeLayer::OGRShapeLayer( OGRShapeDataSource* poDSIn,
                              const char * pszFullNameIn,
                              SHPHandle hSHPIn, DBFHandle hDBFIn,
                              OGRSpatialReference *poSRSIn, bool bSRSSetIn,
                              bool bUpdate,
                              OGRwkbGeometryType eReqType,
                              char ** papszCreateOptions ) :
    OGRAbstractProxiedLayer(poDSIn->GetPool()),
    poDS(poDSIn),
    iNextShapeId(0),
    nTotalShapeCount(0),
    pszFullName(CPLStrdup(pszFullNameIn)),
    hSHP(hSHPIn),
    hDBF(hDBFIn),
    bUpdateAccess(bUpdate),
    eRequestedGeomType(eReqType),
    panMatchingFIDs(NULL),
    iMatchingFID(0),
    m_poFilterGeomLastValid(NULL),
    nSpatialFIDCount(0),
    panSpatialFIDs(NULL),
    bHeaderDirty(false),
    bSHPNeedsRepack(false),
    bCheckedForQIX(false),
    hQIX(NULL),
    bCheckedForSBN(false),
    hSBN(NULL),
    bSbnSbxDeleted(false),
    bTruncationWarningEmitted(false),
    bHSHPWasNonNULL(hSHPIn != NULL),
    bHDBFWasNonNULL(hDBFIn != NULL),
    eFileDescriptorsState(FD_OPENED),
    bResizeAtClose(false),
    bCreateSpatialIndexAtClose(false),
    bRewindOnWrite(false),
    m_bAutoRepack(false),
    m_eNeedRepack(MAYBE)
{
    if( hSHP != NULL )
    {
        nTotalShapeCount = hSHP->nRecords;
        if( hDBF != NULL && hDBF->nRecords != nTotalShapeCount )
        {
            CPLDebug(
                "Shape",
                "Inconsistent record number in .shp (%d) and in .dbf (%d)",
                hSHP->nRecords, hDBF->nRecords);
        }
    }
    else if( hDBF != NULL )
    {
        nTotalShapeCount = hDBF->nRecords;
    }
#ifdef DEBUG
    else
    {
        CPLError(CE_Fatal, CPLE_AssertionFailed,
                 "Should not happen: Both hSHP and hDBF are nullptrs");
    }
#endif

    if( !TouchLayer() )
    {
        CPLDebug("Shape", "TouchLayer in shape ctor failed. ");
    }

    if( hDBF != NULL && hDBF->pszCodePage != NULL )
    {
        CPLDebug( "Shape", "DBF Codepage = %s for %s",
                  hDBF->pszCodePage, pszFullName );

        // Not too sure about this, but it seems like better than nothing.
        osEncoding = ConvertCodePage( hDBF->pszCodePage );
    }

    if( hDBF != NULL )
    {
        if( !(hDBF->nUpdateYearSince1900 == 95 &&
              hDBF->nUpdateMonth == 7 &&
              hDBF->nUpdateDay == 26) )
        {
            SetMetadataItem(
                "DBF_DATE_LAST_UPDATE",
                CPLSPrintf("%04d-%02d-%02d",
                           hDBF->nUpdateYearSince1900 + 1900,
                           hDBF->nUpdateMonth, hDBF->nUpdateDay) );
        }
        struct tm tm;
        CPLUnixTimeToYMDHMS(time(NULL), &tm);
        DBFSetLastModifiedDate( hDBF, tm.tm_year,
                                tm.tm_mon + 1, tm.tm_mday );
    }

    const char* pszShapeEncoding = NULL;
    pszShapeEncoding = CSLFetchNameValue(poDS->GetOpenOptions(), "ENCODING");
    if( pszShapeEncoding == NULL && osEncoding == "")
        pszShapeEncoding = CSLFetchNameValue( papszCreateOptions, "ENCODING" );
    if( pszShapeEncoding == NULL )
        pszShapeEncoding = CPLGetConfigOption( "SHAPE_ENCODING", NULL );
    if( pszShapeEncoding != NULL )
        osEncoding = pszShapeEncoding;

    if( osEncoding != "" )
    {
        CPLDebug( "Shape", "Treating as encoding '%s'.", osEncoding.c_str() );

        if( !TestCapability(OLCStringsAsUTF8) )
        {
            CPLDebug( "Shape", "Cannot recode from '%s'. Disabling recoding",
                      osEncoding.c_str() );
            osEncoding = "";
        }
    }

    poFeatureDefn = SHPReadOGRFeatureDefn(
        CPLGetBasename(pszFullName),
        hSHP, hDBF, osEncoding,
        CPLFetchBool(poDS->GetOpenOptions(), "ADJUST_TYPE", false) );

    // To make sure that
    //  GetLayerDefn()->GetGeomFieldDefn(0)->GetSpatialRef() == GetSpatialRef()
    OGRwkbGeometryType eGeomType = poFeatureDefn->GetGeomType();
    if( eGeomType != wkbNone )
    {
        OGRwkbGeometryType eType = wkbUnknown;

        if( eRequestedGeomType == wkbNone )
        {
            eType = eGeomType;

            const char* pszAdjustGeomType = CSLFetchNameValueDef(
                poDS->GetOpenOptions(), "ADJUST_GEOM_TYPE", "FIRST_SHAPE");
            const bool bFirstShape = EQUAL(pszAdjustGeomType, "FIRST_SHAPE");
            const bool bAllShapes  = EQUAL(pszAdjustGeomType, "ALL_SHAPES");
            if( (hSHP != NULL) && (hSHP->nRecords > 0) && wkbHasM(eType) &&
                (bFirstShape || bAllShapes) )
            {
                bool bMIsUsed = false;
                for( int iShape=0; iShape < hSHP->nRecords; iShape++ )
                {
                    SHPObject *psShape = SHPReadObject( hSHP, iShape );
                    if( psShape )
                    {
                        if( psShape->bMeasureIsUsed &&
                            psShape->nVertices > 0 &&
                            psShape->padfM != NULL )
                        {
                            for( int i = 0; i < psShape->nVertices; i++ )
                            {
                                // Per the spec, if the M value is smaller than
                                // -1e38, it is a nodata value.
                                if( psShape->padfM[i] > -1e38 )
                                {
                                    bMIsUsed = true;
                                    break;
                                }
                            }
                        }

                        SHPDestroyObject(psShape);
                    }
                    if( bFirstShape || bMIsUsed )
                        break;
                }
                if( !bMIsUsed )
                    eType = OGR_GT_SetModifier(eType, wkbHasZ(eType), FALSE);
            }
        }
        else
        {
            eType = eRequestedGeomType;
        }

        OGRShapeGeomFieldDefn* poGeomFieldDefn =
            new OGRShapeGeomFieldDefn(pszFullName, eType, bSRSSetIn, poSRSIn);
        poFeatureDefn->SetGeomType(wkbNone);
        poFeatureDefn->AddGeomFieldDefn(poGeomFieldDefn, FALSE);
    }
    else if( bSRSSetIn && poSRSIn != NULL )
    {
        poSRSIn->Release();
    }
    SetDescription( poFeatureDefn->GetName() );
    bRewindOnWrite =
        CPLTestBool(CPLGetConfigOption( "SHAPE_REWIND_ON_WRITE", "YES" ));
}

/************************************************************************/
/*                           ~OGRShapeLayer()                           */
/************************************************************************/

OGRShapeLayer::~OGRShapeLayer()

{
    if( m_eNeedRepack == YES && m_bAutoRepack )
        Repack();

    if( bResizeAtClose && hDBF != NULL )
    {
        ResizeDBF();
    }
    if( bCreateSpatialIndexAtClose && hSHP != NULL )
    {
        CreateSpatialIndex(0);
    }

    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "Shape", "%d features read on layer '%s'.",
                  static_cast<int>(m_nFeaturesRead),
                  poFeatureDefn->GetName() );
    }

    ClearMatchingFIDs();
    ClearSpatialFIDs();

    CPLFree( pszFullName );

    if( poFeatureDefn != NULL )
        poFeatureDefn->Release();

    if( hDBF != NULL )
        DBFClose( hDBF );

    if( hSHP != NULL )
        SHPClose( hSHP );

    if( hQIX != NULL )
        SHPCloseDiskTree( hQIX );

    if( hSBN != NULL )
        SBNCloseDiskTree( hSBN );
}

/************************************************************************/
/*                       SetModificationDate()                          */
/************************************************************************/

void OGRShapeLayer::SetModificationDate( const char* pszStr )
{
    if( hDBF && pszStr )
    {
        int year = 0;
        int month = 0;
        int day = 0;
        if( (sscanf(pszStr, "%04d-%02d-%02d", &year, &month, &day) == 3 ||
             sscanf(pszStr, "%04d/%02d/%02d", &year, &month, &day) == 3) &&
            (year >= 1900 && year <= 1900 + 255 && month >= 1 && month <= 12 &&
             day >= 1 && day <= 31) )
        {
            DBFSetLastModifiedDate( hDBF, year - 1900, month, day );
        }
    }
}

/************************************************************************/
/*                       SetWriteDBFEOFChar()                           */
/************************************************************************/

void OGRShapeLayer::SetWriteDBFEOFChar( bool b )
{
    if( hDBF )
    {
        DBFSetWriteEndOfFileChar( hDBF, b );
    }
}

/************************************************************************/
/*                          ConvertCodePage()                           */
/************************************************************************/

CPLString OGRShapeLayer::ConvertCodePage( const char *pszCodePage )

{
    CPLString l_osEncoding;

    if( pszCodePage == NULL )
        return l_osEncoding;

    if( STARTS_WITH_CI(pszCodePage, "LDID/") )
    {
        int nCP = -1;  // Windows code page.

        // http://www.autopark.ru/ASBProgrammerGuide/DBFSTRUC.HTM
        switch( atoi(pszCodePage+5) )
        {
          case 1: nCP = 437;      break;
          case 2: nCP = 850;      break;
          case 3: nCP = 1252;     break;
          case 4: nCP = 10000;    break;
          case 8: nCP = 865;      break;
          case 10: nCP = 850;     break;
          case 11: nCP = 437;     break;
          case 13: nCP = 437;     break;
          case 14: nCP = 850;     break;
          case 15: nCP = 437;     break;
          case 16: nCP = 850;     break;
          case 17: nCP = 437;     break;
          case 18: nCP = 850;     break;
          case 19: nCP = 932;     break;
          case 20: nCP = 850;     break;
          case 21: nCP = 437;     break;
          case 22: nCP = 850;     break;
          case 23: nCP = 865;     break;
          case 24: nCP = 437;     break;
          case 25: nCP = 437;     break;
          case 26: nCP = 850;     break;
          case 27: nCP = 437;     break;
          case 28: nCP = 863;     break;
          case 29: nCP = 850;     break;
          case 31: nCP = 852;     break;
          case 34: nCP = 852;     break;
          case 35: nCP = 852;     break;
          case 36: nCP = 860;     break;
          case 37: nCP = 850;     break;
          case 38: nCP = 866;     break;
          case 55: nCP = 850;     break;
          case 64: nCP = 852;     break;
          case 77: nCP = 936;     break;
          case 78: nCP = 949;     break;
          case 79: nCP = 950;     break;
          case 80: nCP = 874;     break;
          case 87: return CPL_ENC_ISO8859_1;
          case 88: nCP = 1252;     break;
          case 89: nCP = 1252;     break;
          case 100: nCP = 852;     break;
          case 101: nCP = 866;     break;
          case 102: nCP = 865;     break;
          case 103: nCP = 861;     break;
          case 104: nCP = 895;     break;
          case 105: nCP = 620;     break;
          case 106: nCP = 737;     break;
          case 107: nCP = 857;     break;
          case 108: nCP = 863;     break;
          case 120: nCP = 950;     break;
          case 121: nCP = 949;     break;
          case 122: nCP = 936;     break;
          case 123: nCP = 932;     break;
          case 124: nCP = 874;     break;
          case 134: nCP = 737;     break;
          case 135: nCP = 852;     break;
          case 136: nCP = 857;     break;
          case 150: nCP = 10007;   break;
          case 151: nCP = 10029;   break;
          case 200: nCP = 1250;    break;
          case 201: nCP = 1251;    break;
          case 202: nCP = 1254;    break;
          case 203: nCP = 1253;    break;
          case 204: nCP = 1257;    break;
          default: break;
        }

        if( nCP != -1 )
        {
            l_osEncoding.Printf( "CP%d", nCP );
            return l_osEncoding;
        }
    }

    // From the CPG file
    // http://resources.arcgis.com/fr/content/kbase?fa=articleShow&d=21106

    if( (atoi(pszCodePage) >= 437 && atoi(pszCodePage) <= 950)
        || (atoi(pszCodePage) >= 1250 && atoi(pszCodePage) <= 1258) )
    {
        l_osEncoding.Printf( "CP%d", atoi(pszCodePage) );
        return l_osEncoding;
    }
    if( STARTS_WITH_CI(pszCodePage, "8859") )
    {
        if( pszCodePage[4] == '-' )
            l_osEncoding.Printf( "ISO-8859-%s", pszCodePage + 5 );
        else
            l_osEncoding.Printf( "ISO-8859-%s", pszCodePage + 4 );
        return l_osEncoding;
    }
    if( STARTS_WITH_CI(pszCodePage, "UTF-8") )
        return CPL_ENC_UTF8;

    // Try just using the CPG value directly.  Works for stuff like Big5.
    return pszCodePage;
}

/************************************************************************/
/*                            CheckForQIX()                             */
/************************************************************************/

bool OGRShapeLayer::CheckForQIX()

{
    if( bCheckedForQIX )
        return hQIX != NULL;

    const char *pszQIXFilename = CPLResetExtension( pszFullName, "qix" );

    hQIX = SHPOpenDiskTree( pszQIXFilename, NULL );

    bCheckedForQIX = true;

    return hQIX != NULL;
}

/************************************************************************/
/*                            CheckForSBN()                             */
/************************************************************************/

bool OGRShapeLayer::CheckForSBN()

{
    if( bCheckedForSBN )
        return hSBN != NULL;

    const char *pszSBNFilename = CPLResetExtension( pszFullName, "sbn" );

    hSBN = SBNOpenDiskTree( pszSBNFilename, NULL );

    bCheckedForSBN = true;

    return hSBN != NULL;
}

/************************************************************************/
/*                            ScanIndices()                             */
/*                                                                      */
/*      Utilize optional spatial and attribute indices if they are      */
/*      available.                                                      */
/************************************************************************/

bool OGRShapeLayer::ScanIndices()

{
    iMatchingFID = 0;

/* -------------------------------------------------------------------- */
/*      Utilize attribute index if appropriate.                         */
/* -------------------------------------------------------------------- */
    if( m_poAttrQuery != NULL )
    {
        CPLAssert( panMatchingFIDs == NULL );

        InitializeIndexSupport( pszFullName );

        panMatchingFIDs =
            m_poAttrQuery->EvaluateAgainstIndices( this, NULL );
    }

/* -------------------------------------------------------------------- */
/*      Check for spatial index if we have a spatial query.             */
/* -------------------------------------------------------------------- */

    if( m_poFilterGeom == NULL || hSHP == NULL )
        return true;

    OGREnvelope oSpatialFilterEnvelope;
    bool bTryQIXorSBN = true;

    m_poFilterGeom->getEnvelope( &oSpatialFilterEnvelope );

    OGREnvelope oLayerExtent;
    if( GetExtent(&oLayerExtent, TRUE) == OGRERR_NONE )
    {
        if( oSpatialFilterEnvelope.Contains(oLayerExtent) )
        {
            // The spatial filter is larger than the layer extent. No use of
            // .qix file for now.
            return true;
        }
        else if( !oSpatialFilterEnvelope.Intersects(oLayerExtent) )
        {
            // No intersection : no need to check for .qix or .sbn.
            bTryQIXorSBN = false;

            // Set an empty result for spatial FIDs.
            free(panSpatialFIDs);
            panSpatialFIDs = static_cast<int *>(calloc(1, sizeof(int)));
            nSpatialFIDCount = 0;

            delete m_poFilterGeomLastValid;
            m_poFilterGeomLastValid = m_poFilterGeom->clone();
        }
    }

    if( bTryQIXorSBN )
    {
        if( !bCheckedForQIX )
            CPL_IGNORE_RET_VAL(CheckForQIX());
        if( hQIX == NULL && !bCheckedForSBN )
            CPL_IGNORE_RET_VAL(CheckForSBN());
    }

/* -------------------------------------------------------------------- */
/*      Compute spatial index if appropriate.                           */
/* -------------------------------------------------------------------- */
    if( bTryQIXorSBN && (hQIX != NULL || hSBN != NULL) &&
        panSpatialFIDs == NULL )
    {
        double adfBoundsMin[4] = {
            oSpatialFilterEnvelope.MinX,
            oSpatialFilterEnvelope.MinY,
            0.0,
            0.0 };
        double adfBoundsMax[4] = {
            oSpatialFilterEnvelope.MaxX,
            oSpatialFilterEnvelope.MaxY,
            0.0,
            0.0 };

        if( hQIX != NULL )
            panSpatialFIDs = SHPSearchDiskTreeEx( hQIX,
                                                  adfBoundsMin, adfBoundsMax,
                                                  &nSpatialFIDCount );
        else
            panSpatialFIDs = SBNSearchDiskTree( hSBN,
                                                adfBoundsMin, adfBoundsMax,
                                                &nSpatialFIDCount );

        CPLDebug( "SHAPE", "Used spatial index, got %d matches.",
                  nSpatialFIDCount );

        delete m_poFilterGeomLastValid;
        m_poFilterGeomLastValid = m_poFilterGeom->clone();
    }

/* -------------------------------------------------------------------- */
/*      Use spatial index if appropriate.                               */
/* -------------------------------------------------------------------- */
    if( panSpatialFIDs != NULL )
    {
        // Use resulting list as matching FID list (but reallocate and
        // terminate with OGRNullFID).
        if( panMatchingFIDs == NULL )
        {
            panMatchingFIDs = static_cast<GIntBig *>(
                CPLMalloc(sizeof(GIntBig) * (nSpatialFIDCount+1) ));
            for( int i = 0; i < nSpatialFIDCount; i++ )
              panMatchingFIDs[i] = static_cast<GIntBig>( panSpatialFIDs[i] );
            panMatchingFIDs[nSpatialFIDCount] = OGRNullFID;
        }
        // Cull attribute index matches based on those in the spatial index
        // result set.  We assume that the attribute results are in sorted
        // order.
        else
        {
            int iWrite = 0;
            int iSpatial = 0;

            for( int iRead = 0; panMatchingFIDs[iRead] != OGRNullFID; iRead++ )
            {
                while( iSpatial < nSpatialFIDCount
                       && panSpatialFIDs[iSpatial] < panMatchingFIDs[iRead] )
                    iSpatial++;

                if( iSpatial == nSpatialFIDCount )
                    continue;

                if( panSpatialFIDs[iSpatial] == panMatchingFIDs[iRead] )
                    panMatchingFIDs[iWrite++] = panMatchingFIDs[iRead];
            }
            panMatchingFIDs[iWrite] = OGRNullFID;
        }

        if( nSpatialFIDCount > 100000 )
        {
            ClearSpatialFIDs();
        }
    }

    return true;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRShapeLayer::ResetReading()

{
    if( !TouchLayer() )
        return;

    iMatchingFID = 0;

    iNextShapeId = 0;

    if( bHeaderDirty && bUpdateAccess )
        SyncToDisk();
}

/************************************************************************/
/*                        ClearMatchingFIDs()                           */
/************************************************************************/

void OGRShapeLayer::ClearMatchingFIDs()
{
/* -------------------------------------------------------------------- */
/*      Clear previous index search result, if any.                     */
/* -------------------------------------------------------------------- */
    CPLFree( panMatchingFIDs );
    panMatchingFIDs = NULL;
}

/************************************************************************/
/*                        ClearSpatialFIDs()                           */
/************************************************************************/

void OGRShapeLayer::ClearSpatialFIDs()
{
    if( panSpatialFIDs != NULL )
    {
        CPLDebug("SHAPE", "Clear panSpatialFIDs");
        free( panSpatialFIDs );
    }
    panSpatialFIDs = NULL;
    nSpatialFIDCount = 0;

    delete m_poFilterGeomLastValid;
    m_poFilterGeomLastValid = NULL;
}

/************************************************************************/
/*                         SetSpatialFilter()                           */
/************************************************************************/

void OGRShapeLayer::SetSpatialFilter( OGRGeometry * poGeomIn )
{
    ClearMatchingFIDs();

    if( poGeomIn == NULL )
    {
        // Do nothing.
    }
    else if( m_poFilterGeomLastValid != NULL &&
             m_poFilterGeomLastValid->Equals(poGeomIn) )
    {
        // Do nothing.
    }
    else if( panSpatialFIDs != NULL )
    {
        // We clear the spatialFIDs only if we have a new non-NULL spatial
        // filter, otherwise we keep the previous result cached. This can be
        // useful when several SQL layers rely on the same table layer, and use
        // the same spatial filters. But as there is in the destructor of
        // OGRGenSQLResultsLayer a clearing of the spatial filter of the table
        // layer, we need this trick.
        ClearSpatialFIDs();
    }

    return OGRLayer::SetSpatialFilter(poGeomIn);
}

/************************************************************************/
/*                         SetAttributeFilter()                         */
/************************************************************************/

OGRErr OGRShapeLayer::SetAttributeFilter( const char * pszAttributeFilter )
{
    ClearMatchingFIDs();

    return OGRLayer::SetAttributeFilter(pszAttributeFilter);
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/*                                                                      */
/*      If we already have an FID list, we can easily reposition        */
/*      ourselves in it.                                                */
/************************************************************************/

OGRErr OGRShapeLayer::SetNextByIndex( GIntBig nIndex )

{
    if( !TouchLayer() )
        return OGRERR_FAILURE;

    if( nIndex < 0 || nIndex > INT_MAX )
        return OGRERR_FAILURE;

    // Eventually we should try to use panMatchingFIDs list
    // if available and appropriate.
    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL )
        return OGRLayer::SetNextByIndex( nIndex );

    iNextShapeId = static_cast<int>(nIndex);

    return OGRERR_NONE;
}

/************************************************************************/
/*                             FetchShape()                             */
/*                                                                      */
/*      Take a shape id, a geometry, and a feature, and set the feature */
/*      if the shapeid bbox intersects the geometry.                    */
/************************************************************************/

OGRFeature *OGRShapeLayer::FetchShape( int iShapeId )

{
    OGRFeature *poFeature = NULL;

    if( m_poFilterGeom != NULL && hSHP != NULL )
    {
        SHPObject *psShape = SHPReadObject( hSHP, iShapeId );

        // do not trust degenerate bounds on non-point geometries
        // or bounds on null shapes.
        if( psShape == NULL
            || (psShape->nSHPType != SHPT_POINT
                && psShape->nSHPType != SHPT_POINTZ
                && psShape->nSHPType != SHPT_POINTM
                && (psShape->dfXMin == psShape->dfXMax
                    || psShape->dfYMin == psShape->dfYMax))
            || psShape->nSHPType == SHPT_NULL )
        {
            poFeature = SHPReadOGRFeature( hSHP, hDBF, poFeatureDefn,
                                           iShapeId, psShape, osEncoding );
        }
        else if( m_sFilterEnvelope.MaxX < psShape->dfXMin
                 || m_sFilterEnvelope.MaxY < psShape->dfYMin
                 || psShape->dfXMax  < m_sFilterEnvelope.MinX
                 || psShape->dfYMax < m_sFilterEnvelope.MinY )
        {
            SHPDestroyObject(psShape);
            poFeature = NULL;
        }
        else
        {
            poFeature = SHPReadOGRFeature( hSHP, hDBF, poFeatureDefn,
                                           iShapeId, psShape, osEncoding );
        }
    }
    else
    {
        poFeature = SHPReadOGRFeature( hSHP, hDBF, poFeatureDefn,
                                       iShapeId, NULL, osEncoding );
    }

    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRShapeLayer::GetNextFeature()

{
    if( !TouchLayer() )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Collect a matching list if we have attribute or spatial         */
/*      indices.  Only do this on the first request for a given pass    */
/*      of course.                                                      */
/* -------------------------------------------------------------------- */
    if( (m_poAttrQuery != NULL || m_poFilterGeom != NULL)
        && iNextShapeId == 0 && panMatchingFIDs == NULL )
    {
        ScanIndices();
    }

/* -------------------------------------------------------------------- */
/*      Loop till we find a feature matching our criteria.              */
/* -------------------------------------------------------------------- */
    OGRFeature *poFeature = NULL;

    while( true )
    {
        if( panMatchingFIDs != NULL )
        {
            if( panMatchingFIDs[iMatchingFID] == OGRNullFID )
            {
                return NULL;
            }

            // Check the shape object's geometry, and if it matches
            // any spatial filter, return it.
            poFeature =
                FetchShape(static_cast<int>(panMatchingFIDs[iMatchingFID]));

            iMatchingFID++;
        }
        else
        {
            if( iNextShapeId >= nTotalShapeCount )
            {
                return NULL;
            }

            if( hDBF )
            {
                if( DBFIsRecordDeleted( hDBF, iNextShapeId ) )
                    poFeature = NULL;
                else if( VSIFEofL(VSI_SHP_GetVSIL(hDBF->fp)) )
                    return NULL;  //* I/O error.
                else
                    poFeature = FetchShape(iNextShapeId);
            }
            else
                poFeature = FetchShape(iNextShapeId);

            iNextShapeId++;
        }

        if( poFeature != NULL )
        {
            OGRGeometry* poGeom = poFeature->GetGeometryRef();
            if( poGeom != NULL )
            {
                poGeom->assignSpatialReference( GetSpatialRef() );
            }

            m_nFeaturesRead++;

            if( (m_poFilterGeom == NULL || FilterGeometry( poGeom ) )
                && (m_poAttrQuery == NULL ||
                    m_poAttrQuery->Evaluate( poFeature )) )
            {
                return poFeature;
            }

            delete poFeature;
        }
    }
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRShapeLayer::GetFeature( GIntBig nFeatureId )

{
    if( !TouchLayer() || nFeatureId > INT_MAX )
        return NULL;

    OGRFeature *poFeature =
        SHPReadOGRFeature( hSHP, hDBF, poFeatureDefn,
                           static_cast<int>(nFeatureId), NULL,
                           osEncoding );

    if( poFeature == NULL ) {
        // Reading shape feature failed.
        return NULL;
    }

    if( poFeature->GetGeometryRef() != NULL )
    {
        poFeature->GetGeometryRef()->assignSpatialReference( GetSpatialRef() );
    }

    m_nFeaturesRead++;

    return poFeature;
}

/************************************************************************/
/*                             ISetFeature()                             */
/************************************************************************/

OGRErr OGRShapeLayer::ISetFeature( OGRFeature *poFeature )

{
    if( !TouchLayer() )
        return OGRERR_FAILURE;

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "SetFeature");
        return OGRERR_FAILURE;
    }

    GIntBig nFID = poFeature->GetFID();
    if( nFID < 0
        || (hSHP != NULL && nFID >= hSHP->nRecords)
        || (hDBF != NULL && nFID >= hDBF->nRecords) )
    {
        return OGRERR_NON_EXISTING_FEATURE;
    }

    bHeaderDirty = true;
    if( CheckForQIX() || CheckForSBN() )
        DropSpatialIndex();

    unsigned int nOffset = 0;
    unsigned int nSize = 0;
    if( hSHP != NULL )
    {
        nOffset = hSHP->panRecOffset[nFID];
        nSize = hSHP->panRecSize[nFID];
    }

    OGRErr eErr = SHPWriteOGRFeature( hSHP, hDBF, poFeatureDefn, poFeature,
                                      osEncoding, &bTruncationWarningEmitted,
                                      bRewindOnWrite );

    if( hSHP != NULL )
    {
        if( nOffset != hSHP->panRecOffset[nFID] ||
            nSize != hSHP->panRecSize[nFID] )
        {
            bSHPNeedsRepack = true;
            m_eNeedRepack = YES;
        }
    }

    return eErr;
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGRShapeLayer::DeleteFeature( GIntBig nFID )

{
    if( !TouchLayer() || nFID > INT_MAX )
        return OGRERR_FAILURE;

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "DeleteFeature");
        return OGRERR_FAILURE;
    }

    if( nFID < 0
        || (hSHP != NULL && nFID >= hSHP->nRecords)
        || (hDBF != NULL && nFID >= hDBF->nRecords) )
    {
        return OGRERR_NON_EXISTING_FEATURE;
    }

    if( !hDBF )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to delete shape in shapefile with no .dbf file.  "
                  "Deletion is done by marking record deleted in dbf "
                  "and is not supported without a .dbf file." );
        return OGRERR_FAILURE;
    }

    if( DBFIsRecordDeleted( hDBF, static_cast<int>(nFID) ) )
    {
        return OGRERR_NON_EXISTING_FEATURE;
    }

    if( !DBFMarkRecordDeleted( hDBF, static_cast<int>(nFID), TRUE ) )
        return OGRERR_FAILURE;

    bHeaderDirty = true;
    if( CheckForQIX() || CheckForSBN() )
        DropSpatialIndex();
    m_eNeedRepack = YES;

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRShapeLayer::ICreateFeature( OGRFeature *poFeature )

{
    if( !TouchLayer() )
        return OGRERR_FAILURE;

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "CreateFeature");
        return OGRERR_FAILURE;
    }

    if( hDBF != NULL &&
        !VSI_SHP_WriteMoreDataOK(hDBF->fp, hDBF->nRecordLength) )
    {
        return OGRERR_FAILURE;
    }

    bHeaderDirty = true;
    if( CheckForQIX() || CheckForSBN() )
        DropSpatialIndex();

    poFeature->SetFID( OGRNullFID );

    if( nTotalShapeCount == 0
        && wkbFlatten(eRequestedGeomType) == wkbUnknown
        && hSHP != NULL
        && hSHP->nShapeType != SHPT_MULTIPATCH
        && poFeature->GetGeometryRef() != NULL )
    {
        OGRGeometry *poGeom = poFeature->GetGeometryRef();
        int nShapeType = -1;

        switch( poGeom->getGeometryType() )
        {
          case wkbPoint:
            nShapeType = SHPT_POINT;
            eRequestedGeomType = wkbPoint;
            break;

          case wkbPoint25D:
            nShapeType = SHPT_POINTZ;
            eRequestedGeomType = wkbPoint25D;
            break;

          case wkbPointM:
            nShapeType = SHPT_POINTM;
            eRequestedGeomType = wkbPointM;
            break;

          case wkbPointZM:
            nShapeType = SHPT_POINTZ;
            eRequestedGeomType = wkbPointZM;
            break;

          case wkbMultiPoint:
            nShapeType = SHPT_MULTIPOINT;
            eRequestedGeomType = wkbMultiPoint;
            break;

          case wkbMultiPoint25D:
            nShapeType = SHPT_MULTIPOINTZ;
            eRequestedGeomType = wkbMultiPoint25D;
            break;

          case wkbMultiPointM:
            nShapeType = SHPT_MULTIPOINTM;
            eRequestedGeomType = wkbMultiPointM;
            break;

          case wkbMultiPointZM:
            nShapeType = SHPT_MULTIPOINTZ;
            eRequestedGeomType = wkbMultiPointM;
            break;

          case wkbLineString:
          case wkbMultiLineString:
            nShapeType = SHPT_ARC;
            eRequestedGeomType = wkbLineString;
            break;

          case wkbLineString25D:
          case wkbMultiLineString25D:
            nShapeType = SHPT_ARCZ;
            eRequestedGeomType = wkbLineString25D;
            break;

          case wkbLineStringM:
          case wkbMultiLineStringM:
            nShapeType = SHPT_ARCM;
            eRequestedGeomType = wkbLineStringM;
            break;

          case wkbLineStringZM:
          case wkbMultiLineStringZM:
            nShapeType = SHPT_ARCZ;
            eRequestedGeomType = wkbLineStringZM;
            break;

          case wkbPolygon:
          case wkbMultiPolygon:
          case wkbTriangle:
            nShapeType = SHPT_POLYGON;
            eRequestedGeomType = wkbPolygon;
            break;

          case wkbPolygon25D:
          case wkbMultiPolygon25D:
          case wkbTriangleZ:
            nShapeType = SHPT_POLYGONZ;
            eRequestedGeomType = wkbPolygon25D;
            break;

          case wkbPolygonM:
          case wkbMultiPolygonM:
          case wkbTriangleM:
            nShapeType = SHPT_POLYGONM;
            eRequestedGeomType = wkbPolygonM;
            break;

          case wkbPolygonZM:
          case wkbMultiPolygonZM:
          case wkbTriangleZM:
            nShapeType = SHPT_POLYGONZ;
            eRequestedGeomType = wkbPolygonZM;
            break;

          default:
            nShapeType = -1;
            break;
        }

        if( wkbFlatten(poGeom->getGeometryType()) == wkbTIN ||
            wkbFlatten(poGeom->getGeometryType()) == wkbPolyhedralSurface )
        {
            nShapeType = SHPT_MULTIPATCH;
            eRequestedGeomType = wkbUnknown;
        }

        if( wkbFlatten(poGeom->getGeometryType()) == wkbGeometryCollection )
        {
            OGRGeometryCollection *poGC =
                            dynamic_cast<OGRGeometryCollection *>(poGeom);
            bool bIsMultiPatchCompatible = false;
            for( int iGeom = 0; poGC != NULL &&
                                iGeom < poGC->getNumGeometries(); iGeom++ )
            {
                OGRwkbGeometryType eSubGeomType =
                    wkbFlatten(poGC->getGeometryRef(iGeom)->getGeometryType());
                if( eSubGeomType == wkbTIN ||
                    eSubGeomType == wkbPolyhedralSurface )
                {
                    bIsMultiPatchCompatible = true;
                }
                else if( eSubGeomType != wkbMultiPolygon )
                {
                    bIsMultiPatchCompatible = false;
                    break;
                }
            }
            if( bIsMultiPatchCompatible )
            {
                nShapeType = SHPT_MULTIPATCH;
                eRequestedGeomType = wkbUnknown;
            }
        }

        if( nShapeType != -1 )
        {
            poFeatureDefn->SetGeomType(eRequestedGeomType);
            ResetGeomType( nShapeType );
        }
    }

    const OGRErr eErr =
        SHPWriteOGRFeature( hSHP, hDBF, poFeatureDefn, poFeature,
                            osEncoding, &bTruncationWarningEmitted,
                            bRewindOnWrite );

    if( hSHP != NULL )
        nTotalShapeCount = hSHP->nRecords;
    else if( hDBF != NULL )
        nTotalShapeCount = hDBF->nRecords;
#ifdef DEBUG
    else  // Silence coverity.
        CPLError(CE_Fatal, CPLE_AssertionFailed,
                 "Should not happen: Both hSHP and hDBF are nullptrs");
#endif

    return eErr;
}

/************************************************************************/
/*               GetFeatureCountWithSpatialFilterOnly()                 */
/*                                                                      */
/* Specialized implementation of GetFeatureCount() when there is *only* */
/* a spatial filter and no attribute filter.                            */
/************************************************************************/

int OGRShapeLayer::GetFeatureCountWithSpatialFilterOnly()

{
/* -------------------------------------------------------------------- */
/*      Collect a matching list if we have attribute or spatial         */
/*      indices.  Only do this on the first request for a given pass    */
/*      of course.                                                      */
/* -------------------------------------------------------------------- */
    if( panMatchingFIDs == NULL )
    {
        ScanIndices();
    }

    int nFeatureCount = 0;
    int iLocalMatchingFID = 0;
    int iLocalNextShapeId = 0;
    bool bExpectPoints = false;

    if( wkbFlatten(poFeatureDefn->GetGeomType()) == wkbPoint )
        bExpectPoints = true;

/* -------------------------------------------------------------------- */
/*      Loop till we find a feature matching our criteria.              */
/* -------------------------------------------------------------------- */

    SHPObject sShape;
    memset(&sShape, 0, sizeof(sShape));

    while( true )
    {
        int iShape = -1;

        if( panMatchingFIDs != NULL )
        {
            iShape = static_cast<int>(panMatchingFIDs[iLocalMatchingFID]);
            if( iShape == OGRNullFID )
                break;
            iLocalMatchingFID++;
        }
        else
        {
            if( iLocalNextShapeId >= nTotalShapeCount )
                break;
            iShape = iLocalNextShapeId++;

            if( hDBF )
            {
                if( DBFIsRecordDeleted( hDBF, iShape ) )
                    continue;

                if( VSIFEofL(VSI_SHP_GetVSIL(hDBF->fp)) )
                    break;
            }
        }

        // Read full shape for point layers.
        SHPObject* psShape = NULL;
        if( bExpectPoints ||
            hSHP->panRecOffset[iShape] == 0 /* lazy shx loading case */ )
            psShape = SHPReadObject( hSHP, iShape);

/* -------------------------------------------------------------------- */
/*      Only read feature type and bounding box for now. In case of     */
/*      inconclusive tests on bounding box only, we will read the full  */
/*      shape later.                                                    */
/* -------------------------------------------------------------------- */
        else if( iShape >= 0 && iShape < hSHP->nRecords &&
                 hSHP->panRecSize[iShape] > 4 + 8 * 4 )
        {
            GByte abyBuf[4 + 8 * 4] = {};
            if( hSHP->sHooks.FSeek( hSHP->fpSHP,
                                    hSHP->panRecOffset[iShape] + 8, 0 ) == 0 &&
                hSHP->sHooks.FRead( abyBuf, sizeof(abyBuf),
                                    1, hSHP->fpSHP ) == 1 )
            {
                memcpy(&(sShape.nSHPType), abyBuf, 4);
                CPL_LSBPTR32(&(sShape.nSHPType));
                if( sShape.nSHPType != SHPT_NULL &&
                    sShape.nSHPType != SHPT_POINT &&
                    sShape.nSHPType != SHPT_POINTM &&
                    sShape.nSHPType != SHPT_POINTZ )
                {
                    psShape = &sShape;
                    memcpy(&(sShape.dfXMin), abyBuf + 4, 8);
                    memcpy(&(sShape.dfYMin), abyBuf + 12, 8);
                    memcpy(&(sShape.dfXMax), abyBuf + 20, 8);
                    memcpy(&(sShape.dfYMax), abyBuf + 28, 8);
                    CPL_LSBPTR64(&(sShape.dfXMin));
                    CPL_LSBPTR64(&(sShape.dfYMin));
                    CPL_LSBPTR64(&(sShape.dfXMax));
                    CPL_LSBPTR64(&(sShape.dfYMax));
                }
            }
            else
            {
                break;
            }
        }

        if( psShape != NULL && psShape->nSHPType != SHPT_NULL )
        {
            OGRGeometry* poGeometry = NULL;
            OGREnvelope sGeomEnv;
            // Test if we have a degenerated bounding box.
            if( psShape->nSHPType != SHPT_POINT
                && psShape->nSHPType != SHPT_POINTZ
                && psShape->nSHPType != SHPT_POINTM
                && (psShape->dfXMin == psShape->dfXMax
                    || psShape->dfYMin == psShape->dfYMax) )
            {
                // Need to read the full geometry to compute the envelope.
                if( psShape == &sShape )
                    psShape = SHPReadObject( hSHP, iShape);

                if( psShape )
                {
                    poGeometry = SHPReadOGRObject( hSHP, iShape, psShape );
                    poGeometry->getEnvelope( &sGeomEnv );
                    psShape = NULL;
                }
            }
            else
            {
                // Trust the shape bounding box as the shape envelope.
                sGeomEnv.MinX = psShape->dfXMin;
                sGeomEnv.MinY = psShape->dfYMin;
                sGeomEnv.MaxX = psShape->dfXMax;
                sGeomEnv.MaxY = psShape->dfYMax;
            }

/* -------------------------------------------------------------------- */
/*      If there is no                                                  */
/*      intersection between the envelopes we are sure not to have      */
/*      any intersection.                                               */
/* -------------------------------------------------------------------- */
            if( sGeomEnv.MaxX < m_sFilterEnvelope.MinX
                || sGeomEnv.MaxY < m_sFilterEnvelope.MinY
                || m_sFilterEnvelope.MaxX < sGeomEnv.MinX
                || m_sFilterEnvelope.MaxY < sGeomEnv.MinY )
            {}
/* -------------------------------------------------------------------- */
/*      If the filter geometry is its own envelope and if the           */
/*      envelope of the geometry is inside the filter geometry,         */
/*      the geometry itself is inside the filter geometry               */
/* -------------------------------------------------------------------- */
            else if( m_bFilterIsEnvelope &&
                sGeomEnv.MinX >= m_sFilterEnvelope.MinX &&
                sGeomEnv.MinY >= m_sFilterEnvelope.MinY &&
                sGeomEnv.MaxX <= m_sFilterEnvelope.MaxX &&
                sGeomEnv.MaxY <= m_sFilterEnvelope.MaxY)
            {
                nFeatureCount++;
            }
            else
            {
/* -------------------------------------------------------------------- */
/*      Fallback to full intersect test (using GEOS) if we still        */
/*      don't know for sure.                                            */
/* -------------------------------------------------------------------- */
                if( OGRGeometryFactory::haveGEOS() )
                {
                    // Read the full geometry.
                    if( poGeometry == NULL )
                    {
                        if( psShape == &sShape )
                            psShape = SHPReadObject( hSHP, iShape);
                        if( psShape )
                        {
                            poGeometry =
                                SHPReadOGRObject( hSHP, iShape, psShape );
                            psShape = NULL;
                        }
                    }
                    if( poGeometry == NULL )
                    {
                        nFeatureCount++;
                    }
                    else if( m_pPreparedFilterGeom != NULL )
                    {
                        if( OGRPreparedGeometryIntersects(m_pPreparedFilterGeom,
                                                          poGeometry) )
                        {
                            nFeatureCount++;
                        }
                    }
                    else if( m_poFilterGeom->Intersects( poGeometry ) )
                        nFeatureCount++;
                }
                else
                {
                    nFeatureCount++;
                }
            }

            delete poGeometry;
        }
        else
        {
            nFeatureCount++;
        }

        if( psShape && psShape != &sShape )
            SHPDestroyObject( psShape );
    }

    return nFeatureCount;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRShapeLayer::GetFeatureCount( int bForce )

{
    // Check if the spatial filter is non-trivial.
    bool bHasTrivialSpatialFilter = false;
    if( m_poFilterGeom != NULL )
    {
        OGREnvelope oSpatialFilterEnvelope;
        m_poFilterGeom->getEnvelope( &oSpatialFilterEnvelope );

        OGREnvelope oLayerExtent;
        if( GetExtent(&oLayerExtent, TRUE) == OGRERR_NONE )
        {
            if( oSpatialFilterEnvelope.Contains(oLayerExtent) )
            {
                bHasTrivialSpatialFilter = true;
            }
            else
            {
                bHasTrivialSpatialFilter = false;
            }
        }
        else
        {
            bHasTrivialSpatialFilter = false;
        }
    }
    else
    {
        bHasTrivialSpatialFilter = true;
    }

    if( bHasTrivialSpatialFilter && m_poAttrQuery == NULL )
        return nTotalShapeCount;

    if( !TouchLayer() )
        return 0;

    // Spatial filter only.
    if( m_poAttrQuery == NULL && hSHP != NULL )
    {
        return GetFeatureCountWithSpatialFilterOnly();
    }

    // Attribute filter only.
    if( m_poAttrQuery != NULL )
    {
        // See if we can ignore reading geometries.
        const bool bSaveGeometryIgnored =
            CPL_TO_BOOL(poFeatureDefn->IsGeometryIgnored());
        if( !AttributeFilterEvaluationNeedsGeometry() )
            poFeatureDefn->SetGeometryIgnored(TRUE);

        GIntBig nRet = OGRLayer::GetFeatureCount( bForce );

        poFeatureDefn->SetGeometryIgnored(bSaveGeometryIgnored);
        return nRet;
    }

    return OGRLayer::GetFeatureCount( bForce );
}

/************************************************************************/
/*                             GetExtent()                              */
/*                                                                      */
/*      Fetch extent of the data currently stored in the dataset.       */
/*      The bForce flag has no effect on SHP files since that value     */
/*      is always in the header.                                        */
/*                                                                      */
/*      Returns OGRERR_NONE/OGRRERR_FAILURE.                            */
/************************************************************************/

OGRErr OGRShapeLayer::GetExtent( OGREnvelope *psExtent, int bForce )

{
    if( !TouchLayer() )
        return OGRERR_FAILURE;

    if( hSHP == NULL )
        return OGRERR_FAILURE;

    double adMin[4] = { 0.0, 0.0, 0.0, 0.0 };
    double adMax[4] = { 0.0, 0.0, 0.0, 0.0 };

    SHPGetInfo(hSHP, NULL, NULL, adMin, adMax);

    psExtent->MinX = adMin[0];
    psExtent->MinY = adMin[1];
    psExtent->MaxX = adMax[0];
    psExtent->MaxY = adMax[1];

    if( CPLIsNan(adMin[0]) || CPLIsNan(adMin[1]) ||
        CPLIsNan(adMax[0]) || CPLIsNan(adMax[1]) )
    {
        CPLDebug("SHAPE", "Invalid extent in shape header");

        // Disable filters to avoid infinite recursion in GetNextFeature()
        // that calls ScanIndices() that call GetExtent.
        OGRFeatureQuery* poAttrQuery = m_poAttrQuery;
        m_poAttrQuery = NULL;
        OGRGeometry* poFilterGeom = m_poFilterGeom;
        m_poFilterGeom = NULL;

        const OGRErr eErr = OGRLayer::GetExtent(psExtent, bForce);

        m_poAttrQuery = poAttrQuery;
        m_poFilterGeom = poFilterGeom;
        return eErr;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRShapeLayer::TestCapability( const char * pszCap )

{
    if( !TouchLayer() )
        return FALSE;

    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    if( EQUAL(pszCap,OLCSequentialWrite)
             || EQUAL(pszCap,OLCRandomWrite) )
        return bUpdateAccess;

    if( EQUAL(pszCap,OLCFastFeatureCount) )
    {
        if( !(m_poFilterGeom == NULL || CheckForQIX() || CheckForSBN()) )
            return FALSE;

        if( m_poAttrQuery != NULL )
        {
            InitializeIndexSupport( pszFullName );
            return m_poAttrQuery->CanUseIndex(this);
        }
        return TRUE;
    }

    if( EQUAL(pszCap,OLCDeleteFeature) )
        return bUpdateAccess;

    if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return CheckForQIX() || CheckForSBN();

    if( EQUAL(pszCap,OLCFastGetExtent) )
        return TRUE;

    if( EQUAL(pszCap,OLCFastSetNextByIndex) )
        return m_poFilterGeom == NULL && m_poAttrQuery == NULL;

    if( EQUAL(pszCap,OLCCreateField) )
        return bUpdateAccess;

    if( EQUAL(pszCap,OLCDeleteField) )
        return bUpdateAccess;

    if( EQUAL(pszCap,OLCReorderFields) )
        return bUpdateAccess;

    if( EQUAL(pszCap,OLCAlterFieldDefn) )
        return bUpdateAccess;

    if( EQUAL(pszCap,OLCIgnoreFields) )
        return TRUE;

    if( EQUAL(pszCap,OLCStringsAsUTF8) )
    {
        // No encoding defined: we don't know.
        if( osEncoding.empty())
            return FALSE;

        if( hDBF == NULL || DBFGetFieldCount( hDBF ) == 0 )
            return TRUE;

        CPLClearRecodeWarningFlags();

        // Otherwise test that we can re-encode field names to UTF-8.
        const int nFieldCount = DBFGetFieldCount( hDBF );
        for( int i = 0; i < nFieldCount; i++ )
        {
            char szFieldName[XBASE_FLDNAME_LEN_READ+1] = {};
            int nWidth = 0;
            int nPrecision = 0;

            DBFGetFieldInfo( hDBF, i, szFieldName, &nWidth, &nPrecision );

            CPLErrorReset();
            CPLPushErrorHandler(CPLQuietErrorHandler);
            char * const pszUTF8Field =
                CPLRecode( szFieldName, osEncoding, CPL_ENC_UTF8);
            CPLPopErrorHandler();
            CPLFree( pszUTF8Field );

            if( CPLGetLastErrorType() != 0 )
            {
                return FALSE;
            }
        }

        return TRUE;
    }

    if( EQUAL(pszCap,OLCMeasuredGeometries) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRShapeLayer::CreateField( OGRFieldDefn *poFieldDefn, int bApproxOK )

{
    if( !TouchLayer() )
        return OGRERR_FAILURE;

    CPLAssert( NULL != poFieldDefn );

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY, "CreateField" );
        return OGRERR_FAILURE;
    }

    bool bDBFJustCreated = false;
    if( hDBF == NULL )
    {
        const CPLString osFilename = CPLResetExtension( pszFullName, "dbf" );
        hDBF = DBFCreate( osFilename );

        if( hDBF == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to create DBF file `%s'.",
                      osFilename.c_str() );
            return OGRERR_FAILURE;
        }

        bDBFJustCreated = true;
    }

    CPLErrorReset();

    if( poFeatureDefn->GetFieldCount() == 255 )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Creating a 256th field, "
                  "but some DBF readers might only support 255 fields" );
    }

/* -------------------------------------------------------------------- */
/*      Normalize field name                                            */
/* -------------------------------------------------------------------- */
    CPLString osFieldName;
    if( !osEncoding.empty() )
    {
        CPLClearRecodeWarningFlags();
        CPLPushErrorHandler(CPLQuietErrorHandler);
        CPLErr eLastErr = CPLGetLastErrorType();
        char* const pszRecoded =
            CPLRecode( poFieldDefn->GetNameRef(), CPL_ENC_UTF8, osEncoding);
        CPLPopErrorHandler();
        osFieldName = pszRecoded;
        CPLFree(pszRecoded);
        if( CPLGetLastErrorType() != eLastErr )
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Failed to create field name '%s': cannot convert to %s",
                     poFieldDefn->GetNameRef(), osEncoding.c_str());
            return OGRERR_FAILURE;
        }
    }
    else
    {
        osFieldName = poFieldDefn->GetNameRef();
    }

    const int nNameSize = static_cast<int>(osFieldName.size());
    char * pszTmp =
        CPLScanString( osFieldName, std::min( nNameSize, XBASE_FLDNAME_LEN_WRITE) , TRUE, TRUE);
    char szNewFieldName[XBASE_FLDNAME_LEN_WRITE + 1];
    strncpy(szNewFieldName, pszTmp, sizeof(szNewFieldName)-1);
    szNewFieldName[sizeof(szNewFieldName)-1] = '\0';

    if( !bApproxOK &&
        ( DBFGetFieldIndex( hDBF, szNewFieldName ) >= 0 ||
          !EQUAL(osFieldName,szNewFieldName) ) )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Failed to add field named '%s'",
                  poFieldDefn->GetNameRef() );

        CPLFree( pszTmp );
        return OGRERR_FAILURE;
    }

    int nRenameNum = 1;
    while( DBFGetFieldIndex( hDBF, szNewFieldName ) >= 0 && nRenameNum < 10 )
    {
        CPLsnprintf( szNewFieldName, sizeof(szNewFieldName),
                  "%.8s_%.1d", pszTmp, nRenameNum );
        nRenameNum ++;
    }
    while( DBFGetFieldIndex( hDBF, szNewFieldName ) >= 0 && nRenameNum < 100 )
        CPLsnprintf( szNewFieldName, sizeof(szNewFieldName),
                  "%.8s%.2d", pszTmp, nRenameNum++ );

    CPLFree( pszTmp );
    pszTmp = NULL;

    if( DBFGetFieldIndex( hDBF, szNewFieldName ) >= 0 )
    {
        // One hundred similar field names!!?
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Too many field names like '%s' when truncated to %d letters "
                  "for Shapefile format.",
                  poFieldDefn->GetNameRef(),
                  XBASE_FLDNAME_LEN_WRITE );
    }

    OGRFieldDefn oModFieldDefn(poFieldDefn);

    if( !EQUAL(osFieldName,szNewFieldName) )
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "Normalized/laundered field name: '%s' to '%s'",
                  poFieldDefn->GetNameRef(),
                  szNewFieldName );

        // Set field name with normalized value.
        oModFieldDefn.SetName(szNewFieldName);
    }

/* -------------------------------------------------------------------- */
/*      Add field to layer                                              */
/* -------------------------------------------------------------------- */
    char chType = 'C';
    int nWidth = 0;
    int nDecimals = 0;

    switch( oModFieldDefn.GetType() )
    {
        case OFTInteger:
            chType = 'N';
            nWidth = oModFieldDefn.GetWidth();
            if( nWidth == 0 ) nWidth = 9;
            break;

        case OFTInteger64:
            chType = 'N';
            nWidth = oModFieldDefn.GetWidth();
            if( nWidth == 0 ) nWidth = 18;
            break;

        case OFTReal:
            chType = 'N';
            nWidth = oModFieldDefn.GetWidth();
            nDecimals = oModFieldDefn.GetPrecision();
            if( nWidth == 0 )
            {
                nWidth = 24;
                nDecimals = 15;
            }
            break;

        case OFTString:
            chType = 'C';
            nWidth = oModFieldDefn.GetWidth();
            if( nWidth == 0 ) nWidth = 80;
            else if( nWidth > OGR_DBF_MAX_FIELD_WIDTH )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Field %s of width %d truncated to %d.",
                          szNewFieldName, nWidth, OGR_DBF_MAX_FIELD_WIDTH );
                nWidth = OGR_DBF_MAX_FIELD_WIDTH;
            }
            break;

        case OFTDate:
            chType = 'D';
            nWidth = 8;
            break;

        case OFTDateTime:
            CPLError(
                CE_Warning, CPLE_NotSupported,
                "Field %s create as date field, though DateTime requested.",
                szNewFieldName );
            chType = 'D';
            nWidth = 8;
            oModFieldDefn.SetType( OFTDate );
            break;

        default:
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Can't create fields of type %s on shapefile layers.",
                      OGRFieldDefn::GetFieldTypeName(oModFieldDefn.GetType()) );

            return OGRERR_FAILURE;
            break;
    }

    oModFieldDefn.SetWidth( nWidth );
    oModFieldDefn.SetPrecision( nDecimals );

    // Suppress the dummy FID field if we have created it just before.
    if( DBFGetFieldCount( hDBF ) == 1 && poFeatureDefn->GetFieldCount() == 0 )
    {
        DBFDeleteField( hDBF, 0 );
    }

    const int iNewField =
        DBFAddNativeFieldType( hDBF, szNewFieldName,
                               chType, nWidth, nDecimals );

    if( iNewField != -1 )
    {
        poFeatureDefn->AddFieldDefn( &oModFieldDefn );

        if( bDBFJustCreated )
        {
            for( int i = 0; i < nTotalShapeCount; i++ )
            {
                DBFWriteNULLAttribute( hDBF, i, 0 );
            }
        }

        return OGRERR_NONE;
    }

    CPLError( CE_Failure, CPLE_AppDefined,
              "Can't create field %s in Shape DBF file, reason unknown.",
              szNewFieldName );

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                            DeleteField()                             */
/************************************************************************/

OGRErr OGRShapeLayer::DeleteField( int iField )
{
    if( !TouchLayer() )
        return OGRERR_FAILURE;

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "DeleteField");
        return OGRERR_FAILURE;
    }

    if( iField < 0 || iField >= poFeatureDefn->GetFieldCount() )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

    if( DBFDeleteField( hDBF, iField ) )
    {
        TruncateDBF();

        return poFeatureDefn->DeleteFieldDefn( iField );
    }

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           ReorderFields()                            */
/************************************************************************/

OGRErr OGRShapeLayer::ReorderFields( int* panMap )
{
    if( !TouchLayer() )
        return OGRERR_FAILURE;

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "ReorderFields");
        return OGRERR_FAILURE;
    }

    if( poFeatureDefn->GetFieldCount() == 0 )
        return OGRERR_NONE;

    OGRErr eErr = OGRCheckPermutation(panMap, poFeatureDefn->GetFieldCount());
    if( eErr != OGRERR_NONE )
        return eErr;

    if( DBFReorderFields( hDBF, panMap ) )
    {
        return poFeatureDefn->ReorderFieldDefns( panMap );
    }

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

OGRErr OGRShapeLayer::AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn,
                                      int nFlagsIn )
{
    if( !TouchLayer() )
        return OGRERR_FAILURE;

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "AlterFieldDefn");
        return OGRERR_FAILURE;
    }

    if( iField < 0 || iField >= poFeatureDefn->GetFieldCount() )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

    OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(iField);
    OGRFieldType eType = poFieldDefn->GetType();

    // On reading we support up to 11 characters
    char szFieldName[XBASE_FLDNAME_LEN_READ+1] = {};
    int nWidth = 0;
    int nPrecision = 0;
    DBFGetFieldInfo( hDBF, iField, szFieldName, &nWidth, &nPrecision );
    char chNativeType = DBFGetNativeFieldType( hDBF, iField );

    if( (nFlagsIn & ALTER_TYPE_FLAG) &&
        poNewFieldDefn->GetType() != poFieldDefn->GetType() )
    {
        if( poNewFieldDefn->GetType() == OFTInteger64 &&
            poFieldDefn->GetType() == OFTInteger )
        {
            eType = poNewFieldDefn->GetType();
        }
        else if( poNewFieldDefn->GetType() != OFTString )
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Can only convert to OFTString" );
            return OGRERR_FAILURE;
        }
        else
        {
            chNativeType = 'C';
            eType = poNewFieldDefn->GetType();
        }
    }

    if( nFlagsIn & ALTER_NAME_FLAG )
    {
        CPLString osFieldName;
        if( !osEncoding.empty() )
        {
            CPLClearRecodeWarningFlags();
            CPLErrorReset();
            CPLPushErrorHandler(CPLQuietErrorHandler);
            char* pszRecoded =
                CPLRecode( poNewFieldDefn->GetNameRef(),
                           CPL_ENC_UTF8, osEncoding);
            CPLPopErrorHandler();
            osFieldName = pszRecoded;
            CPLFree(pszRecoded);
            if( CPLGetLastErrorType() != 0 )
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Failed to rename field name to '%s': "
                    "cannot convert to %s",
                    poNewFieldDefn->GetNameRef(), osEncoding.c_str());
                return OGRERR_FAILURE;
            }
        }
        else
        {
            osFieldName = poNewFieldDefn->GetNameRef();
        }

        strncpy(szFieldName, osFieldName, sizeof(szFieldName)-1);
        szFieldName[sizeof(szFieldName)-1] = '\0';
    }
    if( nFlagsIn & ALTER_WIDTH_PRECISION_FLAG )
    {
        nWidth = poNewFieldDefn->GetWidth();
        nPrecision = poNewFieldDefn->GetPrecision();
    }

    if( DBFAlterFieldDefn( hDBF, iField, szFieldName,
                           chNativeType, nWidth, nPrecision) )
    {
        if( nFlagsIn & ALTER_TYPE_FLAG )
            poFieldDefn->SetType(eType);
        if( nFlagsIn & ALTER_NAME_FLAG )
            poFieldDefn->SetName(poNewFieldDefn->GetNameRef());
        if( nFlagsIn & ALTER_WIDTH_PRECISION_FLAG )
        {
            poFieldDefn->SetWidth(nWidth);
            poFieldDefn->SetPrecision(nPrecision);

            TruncateDBF();
        }
        return OGRERR_NONE;
    }

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRShapeGeomFieldDefn::GetSpatialRef()

{
    if( bSRSSet )
        return poSRS;

    bSRSSet = true;

/* -------------------------------------------------------------------- */
/*      Is there an associated .prj file we can read?                   */
/* -------------------------------------------------------------------- */
    const char  *pszPrjFile = CPLResetExtension( pszFullName, "prj" );

    char *apszOptions[] = {
        const_cast<char *>("EMIT_ERROR_IF_CANNOT_OPEN_FILE=FALSE"), NULL };
    char **papszLines = CSLLoad2( pszPrjFile, -1, -1, apszOptions );
    if( papszLines == NULL )
    {
        pszPrjFile = CPLResetExtension( pszFullName, "PRJ" );
        papszLines = CSLLoad2( pszPrjFile, -1, -1, apszOptions );
    }

    if( papszLines != NULL )
    {
        osPrjFile = pszPrjFile;

        poSRS = new OGRSpatialReference();
        // Remove UTF-8 BOM if found
        // http://lists.osgeo.org/pipermail/gdal-dev/2014-July/039527.html
        if( static_cast<unsigned char>(papszLines[0][0]) == 0xEF &&
            static_cast<unsigned char>(papszLines[0][1]) == 0xBB &&
            static_cast<unsigned char>(papszLines[0][2]) == 0xBF )
        {
            memmove(papszLines[0],
                    papszLines[0] + 3,
                    strlen(papszLines[0] + 3) + 1);
        }
        if( poSRS->importFromESRI( papszLines ) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = NULL;
        }
        CSLDestroy( papszLines );

        // Some new? shapefiles have  EPSG authority nodes (#6485) Use
        // them  to  'import'  TOWGS84  from EPSG  definition,  if  no
        // TOWGS84 is present in the  .prj (which should be the case).
        // We  could  potentially import  more,  or  just replace  the
        // entire definition
        const char* pszAuthorityName = NULL;
        const char* pszAuthorityCode = NULL;
        double adfTOWGS84[7] = { 0.0, 0.0, 0.0, 0.0, 0.0, 0.0, 0.0 };
        if( poSRS != NULL &&
            poSRS->GetTOWGS84(adfTOWGS84, 7) == OGRERR_FAILURE &&
            (pszAuthorityName = poSRS->GetAuthorityName(NULL)) != NULL &&
            EQUAL(pszAuthorityName, "EPSG") &&
            (pszAuthorityCode = poSRS->GetAuthorityCode(NULL)) != NULL )
        {
            const int nEPSGCode = atoi(pszAuthorityCode);
            OGRSpatialReference oSRS;
            if( oSRS.importFromEPSG(nEPSGCode) == OGRERR_NONE &&
                oSRS.GetTOWGS84(adfTOWGS84, 7) == OGRERR_NONE )
            {
                CPLDebug(
                    "Shape", "Importing TOWGS84 node from EPSG definition");
                poSRS->SetTOWGS84(adfTOWGS84[0], adfTOWGS84[1], adfTOWGS84[2],
                                  adfTOWGS84[3], adfTOWGS84[4], adfTOWGS84[5],
                                  adfTOWGS84[6]);
            }
        }

        if( poSRS )
            poSRS->AutoIdentifyEPSG();
    }

    return poSRS;
}

/************************************************************************/
/*                           ResetGeomType()                            */
/*                                                                      */
/*      Modify the geometry type for this file.  Used to convert to     */
/*      a different geometry type when a layer was created with a       */
/*      type of unknown, and we get to the first feature to             */
/*      establish the type.                                             */
/************************************************************************/

int OGRShapeLayer::ResetGeomType( int nNewGeomType )

{
    if( nTotalShapeCount > 0 )
        return FALSE;

    if( hSHP->fpSHX == NULL)
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "OGRShapeLayer::ResetGeomType failed: SHX file is closed");
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Update .shp header.                                             */
/* -------------------------------------------------------------------- */
    int nStartPos = static_cast<int>( hSHP->sHooks.FTell( hSHP->fpSHP ) );

    char abyHeader[100] = {};
    if( hSHP->sHooks.FSeek( hSHP->fpSHP, 0, SEEK_SET ) != 0
        || hSHP->sHooks.FRead( abyHeader, 100, 1, hSHP->fpSHP ) != 1 )
        return FALSE;

    *((GInt32 *) (abyHeader + 32)) = CPL_LSBWORD32( nNewGeomType );

    if( hSHP->sHooks.FSeek( hSHP->fpSHP, 0, SEEK_SET ) != 0
        || hSHP->sHooks.FWrite( abyHeader, 100, 1, hSHP->fpSHP ) != 1 )
        return FALSE;

    if( hSHP->sHooks.FSeek( hSHP->fpSHP, nStartPos, SEEK_SET ) != 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Update .shx header.                                             */
/* -------------------------------------------------------------------- */
    nStartPos = static_cast<int>( hSHP->sHooks.FTell( hSHP->fpSHX ) );

    if( hSHP->sHooks.FSeek( hSHP->fpSHX, 0, SEEK_SET ) != 0
        || hSHP->sHooks.FRead( abyHeader, 100, 1, hSHP->fpSHX ) != 1 )
        return FALSE;

    *((GInt32 *) (abyHeader + 32)) = CPL_LSBWORD32( nNewGeomType );

    if( hSHP->sHooks.FSeek( hSHP->fpSHX, 0, SEEK_SET ) != 0
        || hSHP->sHooks.FWrite( abyHeader, 100, 1, hSHP->fpSHX ) != 1 )
        return FALSE;

    if( hSHP->sHooks.FSeek( hSHP->fpSHX, nStartPos, SEEK_SET ) != 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Update other information.                                       */
/* -------------------------------------------------------------------- */
    hSHP->nShapeType = nNewGeomType;

    return TRUE;
}

/************************************************************************/
/*                             SyncToDisk()                             */
/************************************************************************/

OGRErr OGRShapeLayer::SyncToDisk()

{
    if( !TouchLayer() )
        return OGRERR_FAILURE;

    if( bHeaderDirty )
    {
        if( hSHP != NULL )
            SHPWriteHeader( hSHP );

        if( hDBF != NULL )
            DBFUpdateHeader( hDBF );

        bHeaderDirty = false;
    }

    if( hSHP != NULL )
    {
        hSHP->sHooks.FFlush( hSHP->fpSHP );
        if( hSHP->fpSHX != NULL )
            hSHP->sHooks.FFlush( hSHP->fpSHX );
    }

    if( hDBF != NULL )
    {
        hDBF->sHooks.FFlush( hDBF->fp );
    }

    if( m_eNeedRepack == YES && m_bAutoRepack )
        Repack();

    return OGRERR_NONE;
}

/************************************************************************/
/*                          DropSpatialIndex()                          */
/************************************************************************/

OGRErr OGRShapeLayer::DropSpatialIndex()

{
    if( !TouchLayer() )
        return OGRERR_FAILURE;

    if( !CheckForQIX() && !CheckForSBN() )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Layer %s has no spatial index, DROP SPATIAL INDEX failed.",
                  poFeatureDefn->GetName() );
        return OGRERR_FAILURE;
    }

    const bool bHadQIX = hQIX != NULL;

    SHPCloseDiskTree( hQIX );
    hQIX = NULL;
    bCheckedForQIX = false;

    SBNCloseDiskTree( hSBN );
    hSBN = NULL;
    bCheckedForSBN = false;

    if( bHadQIX )
    {
        const char *pszQIXFilename =
            CPLResetExtension( pszFullName, "qix" );
        CPLDebug( "SHAPE", "Unlinking index file %s", pszQIXFilename );

        if( VSIUnlink( pszQIXFilename ) != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to delete file %s.\n%s",
                      pszQIXFilename, VSIStrerror( errno ) );
            return OGRERR_FAILURE;
        }
    }

    if( !bSbnSbxDeleted )
    {
        const char papszExt[2][4] = { "sbn", "sbx" };
        for( int i = 0; i < 2; i++ )
        {
            const char *pszIndexFilename =
                CPLResetExtension( pszFullName, papszExt[i] );
            CPLDebug(
                "SHAPE", "Trying to unlink index file %s", pszIndexFilename );

            if( VSIUnlink( pszIndexFilename ) != 0 )
            {
                CPLDebug( "SHAPE",
                          "Failed to delete file %s.\n%s",
                          pszIndexFilename, VSIStrerror( errno ) );
            }
        }
    }
    bSbnSbxDeleted = true;

    ClearSpatialFIDs();

    return OGRERR_NONE;
}

/************************************************************************/
/*                         CreateSpatialIndex()                         */
/************************************************************************/

OGRErr OGRShapeLayer::CreateSpatialIndex( int nMaxDepth )

{
    if( !TouchLayer() )
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      If we have an existing spatial index, blow it away first.       */
/* -------------------------------------------------------------------- */
    if( CheckForQIX() )
        DropSpatialIndex();

    bCheckedForQIX = false;

/* -------------------------------------------------------------------- */
/*      Build a quadtree structure for this file.                       */
/* -------------------------------------------------------------------- */
    SyncToDisk();
    SHPTree *psTree = SHPCreateTree( hSHP, 2, nMaxDepth, NULL, NULL );

    if( NULL == psTree )
    {
        // TODO(mloskot): Is it better to return OGRERR_NOT_ENOUGH_MEMORY?
        CPLDebug( "SHAPE",
                  "Index creation failure. Likely, memory allocation error." );

        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Trim unused nodes from the tree.                                */
/* -------------------------------------------------------------------- */
    SHPTreeTrimExtraNodes( psTree );

/* -------------------------------------------------------------------- */
/*      Dump tree to .qix file.                                         */
/* -------------------------------------------------------------------- */
    char *pszQIXFilename = CPLStrdup(CPLResetExtension( pszFullName, "qix" ));

    CPLDebug( "SHAPE", "Creating index file %s", pszQIXFilename );

    SHPWriteTree( psTree, pszQIXFilename );
    CPLFree( pszQIXFilename );

/* -------------------------------------------------------------------- */
/*      cleanup                                                         */
/* -------------------------------------------------------------------- */
    SHPDestroyTree( psTree );

    CheckForQIX();

    return OGRERR_NONE;
}

/************************************************************************/
/*                            CopyInPlace()                             */
/************************************************************************/

static bool CopyInPlace( VSILFILE* fpTarget, const CPLString& osSourceFilename )
{
    VSILFILE* fpSource = VSIFOpenL(osSourceFilename, "rb");
    if( fpSource == NULL )
    {
        CPLError(CE_Failure, CPLE_FileIO,
                 "Cannot open %s", osSourceFilename.c_str());
        return false;
    }

    const size_t nBufferSize = 4096;
    void* pBuffer = CPLMalloc(nBufferSize);
    VSIFSeekL( fpTarget, 0, SEEK_SET );
    bool bRet = true;
    while( true )
    {
        size_t nRead = VSIFReadL( pBuffer, 1, nBufferSize, fpSource );
        size_t nWritten = VSIFWriteL( pBuffer, 1, nRead, fpTarget );
        if( nWritten != nRead )
        {
            bRet = false;
            break;
        }
        if( nRead < nBufferSize )
            break;
    }

    if( bRet )
    {
        bRet = VSIFTruncateL( fpTarget, VSIFTellL(fpTarget) ) == 0;
        if( !bRet )
        {
            CPLError(CE_Failure, CPLE_FileIO, "Truncation failed");
        }
    }

    CPLFree(pBuffer);
    VSIFCloseL(fpSource);
    return bRet;
}

/************************************************************************/
/*                       CheckFileDeletion()                            */
/************************************************************************/

static void CheckFileDeletion( const CPLString& osFilename )
{
    // On Windows, sometimes the file is still triansiently reported
    // as existing although being deleted, which makes QGIS things that
    // an issue arose. The following helps to reduce that risk.
    VSIStatBufL sStat;
    if( VSIStatL( osFilename, &sStat) == 0 &&
        VSIStatL( osFilename, &sStat) == 0 )
    {
        CPLDebug( "Shape",
                  "File %s is still reported as existing whereas "
                  "it should have been deleted",
                  osFilename.c_str() );
    }
}

/************************************************************************/
/*                         ForceDeleteFile()                            */
/************************************************************************/

static void ForceDeleteFile( const CPLString& osFilename )
{
    if( VSIUnlink( osFilename ) != 0 )
    {
        // In case of failure retry with a small delay (Windows specific)
        CPLSleep(0.1);
        if( VSIUnlink( osFilename ) != 0 )
        {
            CPLDebug( "Shape", "Cannot delete %s : %s",
                      osFilename.c_str(), VSIStrerror( errno ) );
        }
    }
    CheckFileDeletion( osFilename );
}

/************************************************************************/
/*                               Repack()                               */
/*                                                                      */
/*      Repack the shape and dbf file, dropping deleted records.        */
/*      FIDs may change.                                                */
/************************************************************************/

OGRErr OGRShapeLayer::Repack()

{
    if( m_eNeedRepack == NO )
    {
        CPLDebug("Shape", "REPACK: nothing to do. Was done previously");
        return OGRERR_NONE;
    }

    if( !TouchLayer() )
        return OGRERR_FAILURE;

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "Repack");
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Build a list of records to be dropped.                          */
/* -------------------------------------------------------------------- */
    int *panRecordsToDelete = static_cast<int *>( CPLMalloc(sizeof(int)*128) );
    int nDeleteCount = 0;
    int nDeleteCountAlloc = 128;
    OGRErr eErr = OGRERR_NONE;

    CPLDebug("Shape", "REPACK: Checking if features have been deleted");

    if( hDBF != NULL )
    {
        for( int iShape = 0; iShape < nTotalShapeCount; iShape++ )
        {
            if( DBFIsRecordDeleted( hDBF, iShape ) )
            {
                if( nDeleteCount == nDeleteCountAlloc )
                {
                    const int nDeleteCountAllocNew =
                        nDeleteCountAlloc + nDeleteCountAlloc / 3 + 32;
                    if( nDeleteCountAlloc >= (INT_MAX - 32) / 4 * 3 ||
                        nDeleteCountAllocNew >
                        INT_MAX / static_cast<int>(sizeof(int)) )
                    {
                        CPLError(
                            CE_Failure, CPLE_AppDefined,
                            "Too many features to delete : %d", nDeleteCount );
                        CPLFree( panRecordsToDelete );
                        return OGRERR_FAILURE;
                    }
                    nDeleteCountAlloc = nDeleteCountAllocNew;
                    int* panRecordsToDeleteNew =
                        static_cast<int*>( VSI_REALLOC_VERBOSE(
                            panRecordsToDelete,
                            nDeleteCountAlloc * sizeof(int) ));
                    if( panRecordsToDeleteNew == NULL )
                    {
                        CPLFree( panRecordsToDelete );
                        return OGRERR_FAILURE;
                    }
                    panRecordsToDelete = panRecordsToDeleteNew;
                }
                panRecordsToDelete[nDeleteCount++] = iShape;
            }
            if( VSIFEofL(VSI_SHP_GetVSIL(hDBF->fp)) )
            {
                CPLFree( panRecordsToDelete );
                return OGRERR_FAILURE;  //I/O error.
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      If there are no records marked for deletion, we take no         */
/*      action.                                                         */
/* -------------------------------------------------------------------- */
    if( nDeleteCount == 0 && !bSHPNeedsRepack )
    {
        CPLDebug("Shape", "REPACK: nothing to do");
        CPLFree( panRecordsToDelete );
        return OGRERR_NONE;
    }
    panRecordsToDelete[nDeleteCount] = -1;

/* -------------------------------------------------------------------- */
/*      Find existing filenames with exact case (see #3293).            */
/* -------------------------------------------------------------------- */
    const CPLString osDirname(CPLGetPath(pszFullName));
    const CPLString osBasename(CPLGetBasename(pszFullName));

    CPLString osDBFName;
    CPLString osSHPName;
    CPLString osSHXName;
    CPLString osCPGName;
    char **papszCandidates = VSIReadDir( osDirname );
    int i = 0;
    while( papszCandidates != NULL && papszCandidates[i] != NULL )
    {
        const CPLString osCandidateBasename =
            CPLGetBasename(papszCandidates[i]);
        const CPLString osCandidateExtension =
            CPLGetExtension(papszCandidates[i]);
#ifdef WIN32
        // On Windows, as filenames are case insensitive, a shapefile layer can
        // be made of foo.shp and FOO.DBF, so use case insensitive comparison.
        if( EQUAL(osCandidateBasename, osBasename) )
#else
        if( osCandidateBasename.compare(osBasename) == 0 )
#endif
        {
            if( EQUAL(osCandidateExtension, "dbf") )
                osDBFName =
                    CPLFormFilename(osDirname, papszCandidates[i], NULL);
            else if( EQUAL(osCandidateExtension, "shp") )
                osSHPName =
                    CPLFormFilename(osDirname, papszCandidates[i], NULL);
            else if( EQUAL(osCandidateExtension, "shx") )
                osSHXName =
                    CPLFormFilename(osDirname, papszCandidates[i], NULL);
            else if( EQUAL(osCandidateExtension, "cpg") )
                osCPGName =
                    CPLFormFilename(osDirname, papszCandidates[i], NULL);
        }

        i++;
    }
    CSLDestroy(papszCandidates);
    papszCandidates = NULL;

    if( hDBF != NULL && osDBFName.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find the filename of the DBF file, but we managed to "
                 "open it before !");
        // Should not happen, really.
        CPLFree( panRecordsToDelete );
        return OGRERR_FAILURE;
    }

    if( hSHP != NULL && osSHPName.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find the filename of the SHP file, but we managed to "
                 "open it before !");
        // Should not happen, really.
        CPLFree( panRecordsToDelete );
        return OGRERR_FAILURE;
    }

    if( hSHP != NULL && osSHXName.empty() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot find the filename of the SHX file, but we managed to "
                 "open it before !");
        // Should not happen, really.
        CPLFree( panRecordsToDelete );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup any existing spatial index.  It will become             */
/*      meaningless when the fids change.                               */
/* -------------------------------------------------------------------- */
    if( CheckForQIX() || CheckForSBN() )
        DropSpatialIndex();

/* -------------------------------------------------------------------- */
/*      Create a new dbf file, matching the old.                        */
/* -------------------------------------------------------------------- */
    bool bMustReopenDBF = false;
    CPLString oTempFileDBF;
    const int nNewRecords = nTotalShapeCount - nDeleteCount;

    if( hDBF != NULL && nDeleteCount > 0 )
    {
        CPLDebug("Shape", "REPACK: repacking .dbf");
        bMustReopenDBF = true;

        oTempFileDBF = CPLFormFilename(osDirname, osBasename, NULL);
        oTempFileDBF += "_packed.dbf";

        DBFHandle hNewDBF = DBFCloneEmpty( hDBF, oTempFileDBF );
        if( hNewDBF == NULL )
        {
            CPLFree( panRecordsToDelete );

            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to create temp file %s.",
                      oTempFileDBF.c_str() );
            return OGRERR_FAILURE;
        }

        // Delete temporary .cpg file if existing.
        if( !osCPGName.empty() )
        {
            CPLString oCPGTempFile =
                CPLFormFilename(osDirname, osBasename, NULL);
            oCPGTempFile += "_packed.cpg";
            ForceDeleteFile( oCPGTempFile );
        }

/* -------------------------------------------------------------------- */
/*      Copy over all records that are not deleted.                     */
/* -------------------------------------------------------------------- */
        int iDestShape = 0;
        int iNextDeletedShape = 0;

        for( int iShape = 0;
             iShape < nTotalShapeCount && eErr == OGRERR_NONE;
             iShape++ )
        {
            if( panRecordsToDelete[iNextDeletedShape] == iShape )
            {
                iNextDeletedShape++;
            }
            else
            {
                void *pTuple =
                    const_cast<char *>( DBFReadTuple( hDBF, iShape ) );
                if( pTuple == NULL ||
                    !DBFWriteTuple( hNewDBF, iDestShape++, pTuple ) )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Error writing record %d in .dbf", iShape);
                    eErr = OGRERR_FAILURE;
                }
            }
        }

        DBFClose( hNewDBF );

        if( eErr != OGRERR_NONE )
        {
            CPLFree( panRecordsToDelete );
            VSIUnlink( oTempFileDBF );
            return eErr;
        }
    }

/* -------------------------------------------------------------------- */
/*      Now create a shapefile matching the old one.                    */
/* -------------------------------------------------------------------- */
    bool bMustReopenSHP = hSHP != NULL;
    CPLString oTempFileSHP;
    CPLString oTempFileSHX;

    SHPInfo sSHPInfo;
    memset(&sSHPInfo, 0, sizeof(sSHPInfo));
    unsigned int *panRecOffsetNew = NULL;
    unsigned int *panRecSizeNew = NULL;

    // On Windows, use the pack-in-place approach, ie copy the content of
    // the _packed files on top of the existing opened files. This avoids
    // many issues with files being locked, at the expense of more I/O
    const bool bPackInPlace =
        CPLTestBool(CPLGetConfigOption("OGR_SHAPE_PACK_IN_PLACE",
#ifdef WIN32
                                        "YES"
#else
                                        "NO"
#endif
                                        ));

    if( hSHP != NULL )
    {
        CPLDebug("Shape", "REPACK: repacking .shp + .shx");

        oTempFileSHP = CPLFormFilename(osDirname, osBasename, NULL);
        oTempFileSHP += "_packed.shp";
        oTempFileSHX = CPLFormFilename(osDirname, osBasename, NULL);
        oTempFileSHX += "_packed.shx";

        SHPHandle hNewSHP = SHPCreate( oTempFileSHP, hSHP->nShapeType );
        if( hNewSHP == NULL )
        {
            CPLFree( panRecordsToDelete );
            if( !oTempFileDBF.empty() )
                VSIUnlink( oTempFileDBF );
            return OGRERR_FAILURE;
        }

/* -------------------------------------------------------------------- */
/*      Copy over all records that are not deleted.                     */
/* -------------------------------------------------------------------- */
        int iNextDeletedShape = 0;

        for( int iShape = 0;
             iShape < nTotalShapeCount && eErr == OGRERR_NONE;
             iShape++ )
        {
            if( panRecordsToDelete[iNextDeletedShape] == iShape )
            {
                iNextDeletedShape++;
            }
            else
            {
                SHPObject *hObject = SHPReadObject( hSHP, iShape );
                if( hObject == NULL ||
                    SHPWriteObject( hNewSHP, -1, hObject ) == -1 )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "Error writing record %d in .shp", iShape);
                    eErr = OGRERR_FAILURE;
                }

                if( hObject )
                    SHPDestroyObject( hObject );
            }
        }

        if( bPackInPlace )
        {
            // Backup information of the updated shape context so as to
            // restore it later in the current shape context
            memcpy(&sSHPInfo, hNewSHP, sizeof(sSHPInfo));

            // Use malloc like shapelib does
            panRecOffsetNew = reinterpret_cast<unsigned int*>(
                malloc(sizeof(unsigned int) * hNewSHP->nMaxRecords));
            panRecSizeNew = reinterpret_cast<unsigned int*>(
                malloc(sizeof(unsigned int) * hNewSHP->nMaxRecords));
            if( panRecOffsetNew == NULL || panRecSizeNew == NULL )
            {
                CPLError(CE_Failure, CPLE_OutOfMemory,
                         "Cannot allocate panRecOffsetNew/panRecSizeNew");
                eErr = OGRERR_FAILURE;
            }
            else
            {
                memcpy(panRecOffsetNew, hNewSHP->panRecOffset,
                       sizeof(unsigned int) * hNewSHP->nRecords);
                memcpy(panRecSizeNew, hNewSHP->panRecSize,
                       sizeof(unsigned int) * hNewSHP->nRecords);
            }
        }

        SHPClose( hNewSHP );

        if( eErr != OGRERR_NONE )
        {
            CPLFree( panRecordsToDelete );
            VSIUnlink( oTempFileSHP );
            VSIUnlink( oTempFileSHX );
            if( !oTempFileDBF.empty() )
                VSIUnlink( oTempFileDBF );
            free(panRecOffsetNew);
            free(panRecSizeNew);
            return eErr;
        }
    }

    CPLFree( panRecordsToDelete );
    panRecordsToDelete = NULL;

    // We could also use pack in place for Unix but this involves extra I/O
    // w.r.t to the delete and rename approach

    if( bPackInPlace )
    {
        if( hDBF != NULL && !oTempFileDBF.empty() )
        {
            if( !CopyInPlace( VSI_SHP_GetVSIL(hDBF->fp), oTempFileDBF ) )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                        "An error occurred while copying the content of %s on top of %s. "
                        "The non corrupted version is in the _packed.dbf, "
                        "_packed.shp and _packed.shx files that you should rename "
                        "on top of the main ones.",
                        oTempFileDBF.c_str(),
                        VSI_SHP_GetFilename( hDBF->fp ) );
                free(panRecOffsetNew);
                free(panRecSizeNew);

                DBFClose( hDBF );
                hDBF = NULL;
                if( hSHP != NULL )
                {
                    SHPClose( hSHP );
                    hSHP = NULL;
                }

                return OGRERR_FAILURE;
            }

            // Refresh current handle
            hDBF->nRecords = nNewRecords;
        }

        if( hSHP != NULL && !oTempFileSHP.empty() )
        {
            if( !CopyInPlace( VSI_SHP_GetVSIL(hSHP->fpSHP), oTempFileSHP ) )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                        "An error occurred while copying the content of %s on top of %s. "
                        "The non corrupted version is in the _packed.dbf, "
                        "_packed.shp and _packed.shx files that you should rename "
                        "on top of the main ones.",
                        oTempFileSHP.c_str(),
                        VSI_SHP_GetFilename( hSHP->fpSHP ) );
                free(panRecOffsetNew);
                free(panRecSizeNew);

                if( hDBF != NULL )
                {
                    DBFClose( hDBF );
                    hDBF = NULL;
                }
                SHPClose( hSHP );
                hSHP = NULL;

                return OGRERR_FAILURE;
            }
            if( !CopyInPlace( VSI_SHP_GetVSIL(hSHP->fpSHX), oTempFileSHX ) )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                        "An error occurred while copying the content of %s on top of %s. "
                        "The non corrupted version is in the _packed.dbf, "
                        "_packed.shp and _packed.shx files that you should rename "
                        "on top of the main ones.",
                        oTempFileSHX.c_str(),
                        VSI_SHP_GetFilename( hSHP->fpSHX ) );
                free(panRecOffsetNew);
                free(panRecSizeNew);

                if( hDBF != NULL )
                {
                    DBFClose( hDBF );
                    hDBF = NULL;
                }
                SHPClose( hSHP );
                hSHP = NULL;

                return OGRERR_FAILURE;
            }

            // Refresh current handle
            hSHP->nRecords = sSHPInfo.nRecords;
            hSHP->nMaxRecords = sSHPInfo.nMaxRecords;
            hSHP->nFileSize = sSHPInfo.nFileSize;
            CPLAssert(sizeof(sSHPInfo.adBoundsMin) == 4 * sizeof(double));
            memcpy(hSHP->adBoundsMin, sSHPInfo.adBoundsMin,
                   sizeof(sSHPInfo.adBoundsMin));
            memcpy(hSHP->adBoundsMax, sSHPInfo.adBoundsMax,
                   sizeof(sSHPInfo.adBoundsMax));
            free(hSHP->panRecOffset);
            free(hSHP->panRecSize);
            hSHP->panRecOffset = panRecOffsetNew;
            hSHP->panRecSize = panRecSizeNew;
        }
        else
        {
            // The free() are not really necessary but CSA doesn't realize it
            free(panRecOffsetNew);
            free(panRecSizeNew);
        }

        // Now that everything is successful, we can delete the temp files
        if( !oTempFileDBF.empty() )
        {
            ForceDeleteFile( oTempFileDBF );
        }
        if( !oTempFileSHP.empty() )
        {
            ForceDeleteFile( oTempFileSHP );
            ForceDeleteFile( oTempFileSHX );
        }
    }
    else
    {
/* -------------------------------------------------------------------- */
/*      Cleanup the old .dbf, .shp, .shx and rename the new ones.       */
/* -------------------------------------------------------------------- */
        if( !oTempFileDBF.empty() )
        {
            DBFClose( hDBF );
            hDBF = NULL;

            if( VSIUnlink( osDBFName ) != 0 )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                        "Failed to delete old DBF file: %s",
                        VSIStrerror( errno ) );

                hDBF = poDS->DS_DBFOpen( osDBFName, bUpdateAccess ? "r+" : "r" );

                VSIUnlink( oTempFileDBF );

                return OGRERR_FAILURE;
            }

            if( VSIRename( oTempFileDBF, osDBFName ) != 0 )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                        "Can not rename new DBF file: %s",
                        VSIStrerror( errno ) );
                return OGRERR_FAILURE;
            }

            CheckFileDeletion ( oTempFileDBF );
        }

        if( !oTempFileSHP.empty() )
        {
            SHPClose( hSHP );
            hSHP = NULL;

            if( VSIUnlink( osSHPName ) != 0 )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                        "Can not delete old SHP file: %s",
                        VSIStrerror( errno ) );
                return OGRERR_FAILURE;
            }

            if( VSIUnlink( osSHXName ) != 0 )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                        "Can not delete old SHX file: %s",
                        VSIStrerror( errno ) );
                return OGRERR_FAILURE;
            }

            if( VSIRename( oTempFileSHP, osSHPName ) != 0 )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                        "Can not rename new SHP file: %s",
                        VSIStrerror( errno ) );
                return OGRERR_FAILURE;
            }

            if( VSIRename( oTempFileSHX, osSHXName ) != 0 )
            {
                CPLError( CE_Failure, CPLE_FileIO,
                        "Can not rename new SHX file: %s",
                        VSIStrerror( errno ) );
                return OGRERR_FAILURE;
            }

            CheckFileDeletion( oTempFileSHP );
            CheckFileDeletion( oTempFileSHX );
        }

/* -------------------------------------------------------------------- */
/*      Reopen the shapefile                                            */
/*                                                                      */
/* We do not need to reimplement OGRShapeDataSource::OpenFile() here    */
/* with the fully featured error checking.                              */
/* If all operations above succeeded, then all necessary files are      */
/* in the right place and accessible.                                   */
/* -------------------------------------------------------------------- */

        const char * const pszAccess = bUpdateAccess ? "r+" :  "r";

        if( bMustReopenSHP )
            hSHP = poDS->DS_SHPOpen ( osSHPName , pszAccess );
        if( bMustReopenDBF )
            hDBF = poDS->DS_DBFOpen ( osDBFName , pszAccess );

        if( (bMustReopenSHP && NULL == hSHP) || (bMustReopenDBF && NULL == hDBF) )
            return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Update total shape count.                                       */
/* -------------------------------------------------------------------- */
    if( hDBF != NULL )
        nTotalShapeCount = hDBF->nRecords;
    bSHPNeedsRepack = false;
    m_eNeedRepack = NO;

    return OGRERR_NONE;
}

/************************************************************************/
/*                               ResizeDBF()                            */
/*                                                                      */
/*      Autoshrink columns of the DBF file to their minimum             */
/*      size, according to the existing data.                           */
/************************************************************************/

OGRErr OGRShapeLayer::ResizeDBF()

{
    if( !TouchLayer() )
        return OGRERR_FAILURE;

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "ResizeDBF");
        return OGRERR_FAILURE;
    }

    if( hDBF == NULL )
    {
        CPLError(
            CE_Failure, CPLE_NotSupported,
            "Attempt to RESIZE a shapefile with no .dbf file not supported.");
        return OGRERR_FAILURE;
    }

    /* Look which columns must be examined */
    int *panColMap = static_cast<int *>(
        CPLMalloc(poFeatureDefn->GetFieldCount() * sizeof(int)));
    int *panBestWidth = static_cast<int *>(
        CPLMalloc(poFeatureDefn->GetFieldCount() * sizeof(int)));
    int nStringCols = 0;
    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( poFeatureDefn->GetFieldDefn(i)->GetType() == OFTString ||
            poFeatureDefn->GetFieldDefn(i)->GetType() == OFTInteger ||
            poFeatureDefn->GetFieldDefn(i)->GetType() == OFTInteger64 )
        {
            panColMap[nStringCols] = i;
            panBestWidth[nStringCols] = 1;
            nStringCols++;
        }
    }

    if( nStringCols == 0 )
    {
        // Nothing to do.
        CPLFree(panColMap);
        CPLFree(panBestWidth);
        return OGRERR_NONE;
    }

    CPLDebug("SHAPE", "Computing optimal column size...");

    bool bAlreadyWarned = false;
    for( int i = 0; i < hDBF->nRecords; i++ )
    {
        if( !DBFIsRecordDeleted( hDBF, i ) )
        {
            for( int j = 0; j < nStringCols; j++ )
            {
                if( DBFIsAttributeNULL(hDBF, i, panColMap[j]) )
                    continue;

                const char *pszVal =
                    DBFReadStringAttribute(hDBF, i, panColMap[j]);
                const int nLen =  static_cast<int>(strlen(pszVal));
                if( nLen > panBestWidth[j] )
                    panBestWidth[j] = nLen;
            }
        }
        else if( !bAlreadyWarned )
        {
            bAlreadyWarned = true;
            CPLDebug(
                "SHAPE",
                "DBF file would also need a REPACK due to deleted records");
        }
    }

    for( int j = 0; j < nStringCols; j++ )
    {
        const int iField = panColMap[j];
        OGRFieldDefn* const poFieldDefn = poFeatureDefn->GetFieldDefn(iField);

        const char chNativeType = DBFGetNativeFieldType( hDBF, iField );
        char szFieldName[XBASE_FLDNAME_LEN_READ+1] = {};
        int nOriWidth = 0;
        int nPrecision = 0;
        DBFGetFieldInfo( hDBF, iField, szFieldName,
                         &nOriWidth, &nPrecision );

        if( panBestWidth[j] < nOriWidth )
        {
            CPLDebug(
                "SHAPE", "Shrinking field %d (%s) from %d to %d characters",
                iField, poFieldDefn->GetNameRef(), nOriWidth, panBestWidth[j]);

            if( !DBFAlterFieldDefn( hDBF, iField, szFieldName,
                                    chNativeType, panBestWidth[j],
                                    nPrecision ) )
            {
                CPLError(
                    CE_Failure, CPLE_AppDefined,
                    "Shrinking field %d (%s) from %d to %d characters failed",
                    iField, poFieldDefn->GetNameRef(), nOriWidth,
                    panBestWidth[j]);

                CPLFree(panColMap);
                CPLFree(panBestWidth);

                return OGRERR_FAILURE;
            }
            else
            {
                poFieldDefn->SetWidth(panBestWidth[j]);
            }
        }
    }

    TruncateDBF();

    CPLFree(panColMap);
    CPLFree(panBestWidth);

    return OGRERR_NONE;
}

/************************************************************************/
/*                          TruncateDBF()                               */
/************************************************************************/

void OGRShapeLayer::TruncateDBF()
{
    if( hDBF == NULL )
        return;

    hDBF->sHooks.FSeek(hDBF->fp, 0, SEEK_END);
    vsi_l_offset nOldSize = hDBF->sHooks.FTell(hDBF->fp);
    vsi_l_offset nNewSize =
        hDBF->nRecordLength * static_cast<SAOffset>(hDBF->nRecords)
        + hDBF->nHeaderLength;
    if( hDBF->bWriteEndOfFileChar )
        nNewSize ++;
    if( nNewSize < nOldSize )
    {
        CPLDebug(
            "SHAPE",
            "Truncating DBF file from " CPL_FRMT_GUIB " to " CPL_FRMT_GUIB
            " bytes",
            nOldSize, nNewSize);
        VSIFTruncateL(VSI_SHP_GetVSIL(hDBF->fp), nNewSize);
    }
    hDBF->sHooks.FSeek(hDBF->fp, 0, SEEK_SET);
}

/************************************************************************/
/*                        RecomputeExtent()                             */
/*                                                                      */
/*      Force recomputation of the extent of the .SHP file              */
/************************************************************************/

OGRErr OGRShapeLayer::RecomputeExtent()
{
    if( !TouchLayer() )
        return OGRERR_FAILURE;

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  UNSUPPORTED_OP_READ_ONLY,
                  "RecomputeExtent");
        return OGRERR_FAILURE;
    }

    if( hSHP == NULL )
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "The RECOMPUTE EXTENT operation is not permitted on a layer "
            "without .SHP file." );
        return OGRERR_FAILURE;
    }

    double adBoundsMin[4] = { 0.0, 0.0, 0.0, 0.0 };
    double adBoundsMax[4] = { 0.0, 0.0, 0.0, 0.0 };

    bool bHasBeenInit = false;

    for( int iShape = 0;
         iShape < nTotalShapeCount;
         iShape++ )
    {
        if( hDBF == NULL || !DBFIsRecordDeleted( hDBF, iShape ) )
        {
            SHPObject *psObject = SHPReadObject( hSHP, iShape );
            if( psObject != NULL &&
                psObject->nSHPType != SHPT_NULL &&
                psObject->nVertices != 0 )
            {
                if( !bHasBeenInit )
                {
                    bHasBeenInit = true;
                    adBoundsMin[0] = psObject->padfX[0];
                    adBoundsMax[0] = psObject->padfX[0];
                    adBoundsMin[1] = psObject->padfY[0];
                    adBoundsMax[1] = psObject->padfY[0];
                    if( psObject->padfZ )
                    {
                        adBoundsMin[2] = psObject->padfZ[0];
                        adBoundsMax[2] = psObject->padfZ[0];
                    }
                    if( psObject->padfM )
                    {
                        adBoundsMin[3] = psObject->padfM[0];
                        adBoundsMax[3] = psObject->padfM[0];
                    }
                }

                for( int i = 0; i < psObject->nVertices; i++ )
                {
                    adBoundsMin[0] = std::min(adBoundsMin[0], psObject->padfX[i]);
                    adBoundsMin[1] = std::min(adBoundsMin[1], psObject->padfY[i]);
                    adBoundsMax[0] = std::max(adBoundsMax[0], psObject->padfX[i]);
                    adBoundsMax[1] = std::max(adBoundsMax[1], psObject->padfY[i]);
                    if( psObject->padfZ )
                    {
                        adBoundsMin[2] = std::min(adBoundsMin[2],
                                                  psObject->padfZ[i]);
                        adBoundsMax[2] = std::max(adBoundsMax[2], psObject->padfZ[i]);
                    }
                    if( psObject->padfM )
                    {
                        adBoundsMax[3] = std::max(adBoundsMax[3], psObject->padfM[i]);
                        adBoundsMin[3] = std::min(adBoundsMin[3],
                                                  psObject->padfM[i]);
                    }
                }
            }
            SHPDestroyObject(psObject);
        }
    }

    if( memcmp(hSHP->adBoundsMin, adBoundsMin, 4*sizeof(double)) != 0 ||
        memcmp(hSHP->adBoundsMax, adBoundsMax, 4*sizeof(double)) != 0 )
    {
        bHeaderDirty = true;
        hSHP->bUpdated = TRUE;
        memcpy(hSHP->adBoundsMin, adBoundsMin, 4*sizeof(double));
        memcpy(hSHP->adBoundsMax, adBoundsMax, 4*sizeof(double));
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                              TouchLayer()                            */
/************************************************************************/

bool OGRShapeLayer::TouchLayer()
{
    poDS->SetLastUsedLayer(this);

    if( eFileDescriptorsState == FD_OPENED )
        return true;
    if( eFileDescriptorsState == FD_CANNOT_REOPEN )
        return false;

    return ReopenFileDescriptors();
}

/************************************************************************/
/*                        ReopenFileDescriptors()                       */
/************************************************************************/

bool OGRShapeLayer::ReopenFileDescriptors()
{
    CPLDebug("SHAPE", "ReopenFileDescriptors(%s)", pszFullName);

    if( bHSHPWasNonNULL )
    {
        hSHP = poDS->DS_SHPOpen( pszFullName, bUpdateAccess ? "r+" : "r" );

        if( hSHP == NULL )
        {
            eFileDescriptorsState = FD_CANNOT_REOPEN;
            return false;
        }
    }

    if( bHDBFWasNonNULL )
    {
        hDBF = poDS->DS_DBFOpen( pszFullName, bUpdateAccess ? "r+" : "r" );

        if( hDBF == NULL )
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Cannot reopen %s", CPLResetExtension(pszFullName, "dbf"));
            eFileDescriptorsState = FD_CANNOT_REOPEN;
            return false;
        }
    }

    eFileDescriptorsState = FD_OPENED;

    return true;
}

/************************************************************************/
/*                        CloseUnderlyingLayer()                        */
/************************************************************************/

void OGRShapeLayer::CloseUnderlyingLayer()
{
    CPLDebug("SHAPE", "CloseUnderlyingLayer(%s)", pszFullName);

    if( hDBF != NULL )
        DBFClose( hDBF );
    hDBF = NULL;

    if( hSHP != NULL )
        SHPClose( hSHP );
    hSHP = NULL;

    // We close QIX and reset the check flag, so that CheckForQIX()
    // will retry opening it if necessary when the layer is active again.
    if( hQIX != NULL )
        SHPCloseDiskTree( hQIX );
    hQIX = NULL;
    bCheckedForQIX = false;

    if( hSBN != NULL )
        SBNCloseDiskTree( hSBN );
    hSBN = NULL;
    bCheckedForSBN = false;

    eFileDescriptorsState = FD_CLOSED;
}

/************************************************************************/
/*                            AddToFileList()                           */
/************************************************************************/

void OGRShapeLayer::AddToFileList( CPLStringList& oFileList )
{
    if( !TouchLayer() )
        return;

    if( hSHP )
    {
        const char* pszSHPFilename = VSI_SHP_GetFilename( hSHP->fpSHP );
        oFileList.AddString(pszSHPFilename);
        const char* pszSHPExt = CPLGetExtension(pszSHPFilename);
        const char* pszSHXFilename = CPLResetExtension(
            pszSHPFilename,
            (pszSHPExt[0] == 's') ? "shx" : "SHX" );
        oFileList.AddString(pszSHXFilename);
    }

    if( hDBF )
    {
        const char* pszDBFFilename = VSI_SHP_GetFilename( hDBF->fp );
        oFileList.AddString(pszDBFFilename);
        if( hDBF->pszCodePage != NULL && hDBF->iLanguageDriver == 0 )
        {
            const char* pszDBFExt = CPLGetExtension(pszDBFFilename);
            const char* pszCPGFilename = CPLResetExtension(
                pszDBFFilename,
                (pszDBFExt[0] == 'd') ? "cpg" : "CPG" );
            oFileList.AddString(pszCPGFilename);
        }
    }

    if( hSHP )
    {
        if( GetSpatialRef() != NULL )
        {
            OGRShapeGeomFieldDefn* poGeomFieldDefn =
                (OGRShapeGeomFieldDefn*)GetLayerDefn()->GetGeomFieldDefn(0);
            oFileList.AddString(poGeomFieldDefn->GetPrjFilename());
        }
        if( CheckForQIX() )
        {
            const char* pszQIXFilename =
                CPLResetExtension( pszFullName, "qix" );
            oFileList.AddString(pszQIXFilename);
        }
        else if( CheckForSBN() )
        {
            const char* pszSBNFilename =
                CPLResetExtension( pszFullName, "sbn" );
            oFileList.AddString(pszSBNFilename);
            const char* pszSBXFilename =
                CPLResetExtension( pszFullName, "sbx" );
            oFileList.AddString(pszSBXFilename);
        }
    }
}
