/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRShapeLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999,  Les Technologies SoftMap Inc.
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
#include "ogr_p.h"

#if defined(_WIN32_WCE)
#  include <wce_errno.h>
#endif

#define FD_OPENED           0
#define FD_CLOSED           1
#define FD_CANNOT_REOPEN    2

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRShapeLayer()                            */
/************************************************************************/

OGRShapeLayer::OGRShapeLayer( OGRShapeDataSource* poDSIn,
                              const char * pszName,
                              SHPHandle hSHPIn, DBFHandle hDBFIn, 
                              OGRSpatialReference *poSRSIn, int bSRSSetIn,
                              int bUpdate,
                              OGRwkbGeometryType eReqType )

{
    poDS = poDSIn;
    poSRS = poSRSIn;
    bSRSSet = bSRSSetIn;

    pszFullName = CPLStrdup(pszName);
    
    hSHP = hSHPIn;
    hDBF = hDBFIn;
    bUpdateAccess = bUpdate;

    iNextShapeId = 0;
    panMatchingFIDs = NULL;

    bCheckedForQIX = FALSE;
    hQIX = NULL;

    bSbnSbxDeleted = FALSE;

    bHeaderDirty = FALSE;

    if( hSHP != NULL )
    {
        nTotalShapeCount = hSHP->nRecords;
        if( hDBF != NULL && hDBF->nRecords != nTotalShapeCount )
        {
            CPLDebug("Shape", "Inconsistant record number in .shp (%d) and in .dbf (%d)",
                     hSHP->nRecords, hDBF->nRecords);
        }
    }
    else 
        nTotalShapeCount = hDBF->nRecords;
    
    eRequestedGeomType = eReqType;

    bTruncationWarningEmitted = FALSE;

    
    if( hDBF != NULL && hDBF->pszCodePage != NULL )
    {
        CPLDebug( "Shape", "DBF Codepage = %s for %s", 
                  hDBF->pszCodePage, pszName );

        // Not too sure about this, but it seems like better than nothing.
        osEncoding = ConvertCodePage( hDBF->pszCodePage );
    }
    
    if( CPLGetConfigOption( "SHAPE_ENCODING", NULL ) != NULL )
        osEncoding = CPLGetConfigOption( "SHAPE_ENCODING", "" );

    if( osEncoding != "" )
        CPLDebug( "Shape", "Treating as encoding '%s'.", osEncoding.c_str() );

    poFeatureDefn = SHPReadOGRFeatureDefn( CPLGetBasename(pszName),
                                           hSHP, hDBF, osEncoding );

    /* Init info for the LRU layer mechanism */
    poPrevLayer = NULL;
    poNextLayer = NULL;
    bHSHPWasNonNULL = hSHPIn != NULL;
    bHDBFWasNonNULL = hDBFIn != NULL;
    eFileDescriptorsState = FD_OPENED;
    TouchLayer();
}

/************************************************************************/
/*                           ~OGRShapeLayer()                           */
/************************************************************************/

OGRShapeLayer::~OGRShapeLayer()

{
    /* Remove us from the list of LRU layers if necessary */
    poDS->UnchainLayer(this);

    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "Shape", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead, 
                  poFeatureDefn->GetName() );
    }

    CPLFree( panMatchingFIDs );
    panMatchingFIDs = NULL;

    CPLFree( pszFullName );

    if( poFeatureDefn != NULL )
        poFeatureDefn->Release();

    if( poSRS != NULL )
        poSRS->Release();

    if( hDBF != NULL )
        DBFClose( hDBF );

    if( hSHP != NULL )
        SHPClose( hSHP );

    if( hQIX != NULL )
        SHPCloseDiskTree( hQIX );
}

/************************************************************************/
/*                          ConvertCodePage()                           */
/************************************************************************/

CPLString OGRShapeLayer::ConvertCodePage( const char *pszCodePage )

{
    CPLString osEncoding;

    if( pszCodePage == NULL )
        return osEncoding;

    if( EQUALN(pszCodePage,"LDID/",5) )
    {
        int nCP = -1; // windows code page. 

        //http://www.autopark.ru/ASBProgrammerGuide/DBFSTRUC.HTM
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
            osEncoding.Printf( "CP%d", nCP );
            return osEncoding;
        }
    }

    // From the CPG file
    // http://resources.arcgis.com/fr/content/kbase?fa=articleShow&d=21106
    
    if( (atoi(pszCodePage) >= 437 && atoi(pszCodePage) <= 950)
        || (atoi(pszCodePage) >= 1250 && atoi(pszCodePage) <= 1258) )
    {
        osEncoding.Printf( "CP%d", atoi(pszCodePage) );
        return osEncoding;
    }
    if( EQUALN(pszCodePage,"8859",4) )
    {
        osEncoding.Printf( "ISO%s", pszCodePage );
        return osEncoding;
    }
    if( EQUALN(pszCodePage,"UTF-8",5) )
        return CPL_ENC_UTF8;

    // try just using the CPG value directly.  Works for stuff like Big5.
    return pszCodePage;
}

/************************************************************************/
/*                            CheckForQIX()                             */
/************************************************************************/

int OGRShapeLayer::CheckForQIX()

{
    const char *pszQIXFilename;

    if( bCheckedForQIX )
        return hQIX != NULL;

    pszQIXFilename = CPLResetExtension( pszFullName, "qix" );

    hQIX = SHPOpenDiskTree( pszQIXFilename, NULL ); 

    bCheckedForQIX = TRUE;

    return hQIX != NULL;
}

/************************************************************************/
/*                            ScanIndices()                             */
/*                                                                      */
/*      Utilize optional spatial and attribute indices if they are      */
/*      available.                                                      */
/************************************************************************/

int OGRShapeLayer::ScanIndices()

{
    iMatchingFID = 0;

/* -------------------------------------------------------------------- */
/*      Utilize attribute index if appropriate.                         */
/* -------------------------------------------------------------------- */
    if( m_poAttrQuery != NULL )
    {
        CPLAssert( panMatchingFIDs == NULL );

        InitializeIndexSupport( pszFullName );

        panMatchingFIDs = m_poAttrQuery->EvaluateAgainstIndices( this,
                                                                 NULL );
    }

/* -------------------------------------------------------------------- */
/*      Check for spatial index if we have a spatial query.             */
/* -------------------------------------------------------------------- */

    OGREnvelope oEnvelope;
    if( m_poFilterGeom != NULL )
    {
        m_poFilterGeom->getEnvelope( &oEnvelope );

        OGREnvelope oLayerExtent;
        if (GetExtent(&oLayerExtent, TRUE) == OGRERR_NONE &&
            oEnvelope.Contains(oLayerExtent))
        {
            // The spatial filter is larger than the layer extent. No use of .qix file for now
            return TRUE;
        }
    }

    if( m_poFilterGeom != NULL && !bCheckedForQIX )
        CheckForQIX();

/* -------------------------------------------------------------------- */
/*      Utilize spatial index if appropriate.                           */
/* -------------------------------------------------------------------- */
    if( m_poFilterGeom && hQIX )
    {
        int nSpatialFIDCount, *panSpatialFIDs;
        double adfBoundsMin[4], adfBoundsMax[4];

        adfBoundsMin[0] = oEnvelope.MinX;
        adfBoundsMin[1] = oEnvelope.MinY;
        adfBoundsMin[2] = 0.0;
        adfBoundsMin[3] = 0.0;
        adfBoundsMax[0] = oEnvelope.MaxX;
        adfBoundsMax[1] = oEnvelope.MaxY;
        adfBoundsMax[2] = 0.0;
        adfBoundsMax[3] = 0.0;

        panSpatialFIDs = SHPSearchDiskTreeEx( hQIX,
                                            adfBoundsMin, adfBoundsMax, 
                                            &nSpatialFIDCount );
        CPLDebug( "SHAPE", "Used spatial index, got %d matches.", 
                  nSpatialFIDCount );

        // Use resulting list as matching FID list (but reallocate and
        // terminate with OGRNullFID).

        if( panMatchingFIDs == NULL )
        {
            int i;

            panMatchingFIDs = (long *) 
                CPLMalloc(sizeof(long) * (nSpatialFIDCount+1) );
            for( i = 0; i < nSpatialFIDCount; i++ )
                panMatchingFIDs[i] = (long) panSpatialFIDs[i];
            panMatchingFIDs[nSpatialFIDCount] = OGRNullFID;
        }

        // Cull attribute index matches based on those in the spatial index
        // result set.  We assume that the attribute results are in sorted
        // order.
        else
        {
            int iRead, iWrite=0, iSpatial=0;

            for( iRead = 0; panMatchingFIDs[iRead] != OGRNullFID; iRead++ )
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

        if ( panSpatialFIDs )
            free( panSpatialFIDs );
    }

    return TRUE;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRShapeLayer::ResetReading()

{
    if (!TouchLayer())
        return;

/* -------------------------------------------------------------------- */
/*      Clear previous index search result, if any.                     */
/* -------------------------------------------------------------------- */
    CPLFree( panMatchingFIDs );
    panMatchingFIDs = NULL;
    iMatchingFID = 0;

    iNextShapeId = 0;

    if( bHeaderDirty && bUpdateAccess )
        SyncToDisk();
}

/************************************************************************/
/*                           SetNextByIndex()                           */
/*                                                                      */
/*      If we already have an FID list, we can easily resposition       */
/*      ourselves in it.                                                */
/************************************************************************/

OGRErr OGRShapeLayer::SetNextByIndex( long nIndex )

{
    if (!TouchLayer())
        return OGRERR_FAILURE;

    // Eventually we should try to use panMatchingFIDs list 
    // if available and appropriate. 
    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL )
        return OGRLayer::SetNextByIndex( nIndex );

    iNextShapeId = nIndex;

    return OGRERR_NONE;
}

/************************************************************************/
/*                             FetchShape()                             */
/*                                                                      */
/*      Take a shape id, a geometry, and a feature, and set the feature */
/*      if the shapeid bbox intersects the geometry.                    */
/************************************************************************/

OGRFeature *OGRShapeLayer::FetchShape(int iShapeId)

{
    OGRFeature *poFeature;

    if (m_poFilterGeom != NULL && hSHP != NULL ) 
    {
        SHPObject   *psShape;
        
        psShape = SHPReadObject( hSHP, iShapeId );

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
    if (!TouchLayer())
        return NULL;

    OGRFeature  *poFeature = NULL;

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
    while( TRUE )
    {
        if( panMatchingFIDs != NULL )
        {
            if( panMatchingFIDs[iMatchingFID] == OGRNullFID )
            {
                return NULL;
            }
            
            // Check the shape object's geometry, and if it matches
            // any spatial filter, return it.  
            poFeature = FetchShape(panMatchingFIDs[iMatchingFID]);
            
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
                if (DBFIsRecordDeleted( hDBF, iNextShapeId ))
                    poFeature = NULL;
                else if( VSIFEofL((VSILFILE*)hDBF->fp) )
                    return NULL; /* There's an I/O error */
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
                && (m_poAttrQuery == NULL || m_poAttrQuery->Evaluate( poFeature )) )
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

OGRFeature *OGRShapeLayer::GetFeature( long nFeatureId )

{
    if (!TouchLayer())
        return NULL;

    OGRFeature *poFeature = NULL;
    poFeature = SHPReadOGRFeature( hSHP, hDBF, poFeatureDefn, nFeatureId, NULL,
                                   osEncoding );

    if( poFeature != NULL )
    {
        if( poFeature->GetGeometryRef() != NULL )
        {
            poFeature->GetGeometryRef()->assignSpatialReference( GetSpatialRef() );
        }

        m_nFeaturesRead++;
    
        return poFeature;
    }

    /*
     * Reading shape feature failed.
     */
    return NULL;
}

/************************************************************************/
/*                             SetFeature()                             */
/************************************************************************/

OGRErr OGRShapeLayer::SetFeature( OGRFeature *poFeature )

{
    if (!TouchLayer())
        return OGRERR_FAILURE;

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                    "The SetFeature() operation is not permitted on a read-only shapefile." );
        return OGRERR_FAILURE;
    }

    bHeaderDirty = TRUE;
    if( CheckForQIX() )
        DropSpatialIndex();

    return SHPWriteOGRFeature( hSHP, hDBF, poFeatureDefn, poFeature,
                               osEncoding, &bTruncationWarningEmitted );
}

/************************************************************************/
/*                           DeleteFeature()                            */
/************************************************************************/

OGRErr OGRShapeLayer::DeleteFeature( long nFID )

{
    if (!TouchLayer())
        return OGRERR_FAILURE;

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The DeleteFeature() operation is not permitted on a read-only shapefile." );
        return OGRERR_FAILURE;
    }

    if( nFID < 0 
        || (hSHP != NULL && nFID >= hSHP->nRecords)
        || (hDBF != NULL && nFID >= hDBF->nRecords) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to delete shape with feature id (%ld) which does "
                  "not exist.", nFID );
        return OGRERR_FAILURE;
    }

    if( !hDBF )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to delete shape in shapefile with no .dbf file.\n"
                  "Deletion is done by marking record deleted in dbf\n"
                  "and is not supported without a .dbf file." );
        return OGRERR_FAILURE;
    }

    if( DBFIsRecordDeleted( hDBF, nFID ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to delete shape with feature id (%ld), but it is marked deleted already.",
                  nFID );
        return OGRERR_FAILURE;
    }

    if( !DBFMarkRecordDeleted( hDBF, nFID, TRUE ) )
        return OGRERR_FAILURE;

    bHeaderDirty = TRUE;
    if( CheckForQIX() )
        DropSpatialIndex();

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRShapeLayer::CreateFeature( OGRFeature *poFeature )

{
    OGRErr eErr;

    if (!TouchLayer())
        return OGRERR_FAILURE;

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The CreateFeature() operation is not permitted on a read-only shapefile." );
        return OGRERR_FAILURE;
    }

    bHeaderDirty = TRUE;
    if( CheckForQIX() )
        DropSpatialIndex();

    poFeature->SetFID( OGRNullFID );

    if( nTotalShapeCount == 0 
        && eRequestedGeomType == wkbUnknown 
        && poFeature->GetGeometryRef() != NULL )
    {
        OGRGeometry     *poGeom = poFeature->GetGeometryRef();
        int             nShapeType;
        
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

          case wkbMultiPoint:
            nShapeType = SHPT_MULTIPOINT;
            eRequestedGeomType = wkbMultiPoint;
            break;

          case wkbMultiPoint25D:
            nShapeType = SHPT_MULTIPOINTZ;
            eRequestedGeomType = wkbMultiPoint25D;
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

          case wkbPolygon:
          case wkbMultiPolygon:
            nShapeType = SHPT_POLYGON;
            eRequestedGeomType = wkbPolygon;
            break;

          case wkbPolygon25D:
          case wkbMultiPolygon25D:
            nShapeType = SHPT_POLYGONZ;
            eRequestedGeomType = wkbPolygon25D;
            break;

          default:
            nShapeType = -1;
            break;
        }

        if( nShapeType != -1 )
        {
            ResetGeomType( nShapeType );
        }
    }
    
    eErr = SHPWriteOGRFeature( hSHP, hDBF, poFeatureDefn, poFeature, 
                               osEncoding, &bTruncationWarningEmitted );

    if( hSHP != NULL )
        nTotalShapeCount = hSHP->nRecords;
    else 
        nTotalShapeCount = hDBF->nRecords;
    
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
    int bExpectPoints = FALSE;

    if (wkbFlatten(poFeatureDefn->GetGeomType()) == wkbPoint)
        bExpectPoints = TRUE;

/* -------------------------------------------------------------------- */
/*      Loop till we find a feature matching our criteria.              */
/* -------------------------------------------------------------------- */

    SHPObject sShape;
    memset(&sShape, 0, sizeof(sShape));

    VSILFILE* fpSHP = (VSILFILE*) hSHP->fpSHP;

    while( TRUE )
    {
        SHPObject* psShape = NULL;
        int iShape = -1;

        if( panMatchingFIDs != NULL )
        {
            iShape = panMatchingFIDs[iLocalMatchingFID];
            if( iShape == OGRNullFID )
                break;
            iLocalMatchingFID++;
        }
        else
        {
            if( iLocalNextShapeId >= nTotalShapeCount )
                break;
            iShape = iLocalNextShapeId ++;

            if( hDBF )
            {
                if (DBFIsRecordDeleted( hDBF, iShape ))
                    continue;

                if (VSIFEofL((VSILFILE*)hDBF->fp))
                    break;
            }
        }

        /* Read full shape for point layers */
        if (bExpectPoints)
            psShape = SHPReadObject( hSHP, iShape);

/* -------------------------------------------------------------------- */
/*      Only read feature type and bounding box for now. In case of     */
/*      inconclusive tests on bounding box only, we will read the full  */
/*      shape later.                                                    */
/* -------------------------------------------------------------------- */
        else if (iShape >= 0 && iShape < hSHP->nRecords &&
                    hSHP->panRecSize[iShape] > 4 + 8 * 4 )
        {
            GByte abyBuf[4 + 8 * 4];
            if( VSIFSeekL( fpSHP, hSHP->panRecOffset[iShape] + 8, 0 ) == 0 &&
                VSIFReadL( abyBuf, sizeof(abyBuf), 1, fpSHP ) == 1 )
            {
                memcpy(&(sShape.nSHPType), abyBuf, 4);
                CPL_LSBPTR32(&(sShape.nSHPType));
                if ( sShape.nSHPType != SHPT_NULL &&
                        sShape.nSHPType != SHPT_POINT &&
                        sShape.nSHPType != SHPT_POINTM &&
                        sShape.nSHPType != SHPT_POINTZ)
                {
                    psShape = &sShape;
                    memcpy(&(sShape.dfXMin), abyBuf + 4, 8);
                    memcpy(&(sShape.dfYMin), abyBuf + 12, 8);
                    memcpy(&(sShape.dfXMax), abyBuf + 20, 8);
                    memcpy(&(sShape.dfYMax), abyBuf + 28, 8);
                    CPL_MSBPTR32(&(sShape.dfXMin));
                    CPL_MSBPTR32(&(sShape.dfYMin));
                    CPL_MSBPTR32(&(sShape.dfXMax));
                    CPL_MSBPTR32(&(sShape.dfYMax));
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
            /* Test if we have a degenerated bounding box */
            if (psShape->nSHPType != SHPT_POINT
                && psShape->nSHPType != SHPT_POINTZ
                && psShape->nSHPType != SHPT_POINTM
                && (psShape->dfXMin == psShape->dfXMax
                    || psShape->dfYMin == psShape->dfYMax))
            {
                /* We need to read the full geometry */
                /* to compute the envelope */
                if (psShape == &sShape)
                    psShape = SHPReadObject( hSHP, iShape);
                if (psShape)
                {
                    poGeometry = SHPReadOGRObject( hSHP, iShape, psShape );
                    poGeometry->getEnvelope( &sGeomEnv );
                    psShape = NULL;
                }
            }
            else
            {
                /* Trust the shape bounding box as the shape envelope */
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
            {
            }
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
                nFeatureCount ++;
            }
            else
            {
/* -------------------------------------------------------------------- */
/*      Fallback to full intersect test (using GEOS) if we still        */
/*      don't know for sure.                                            */
/* -------------------------------------------------------------------- */
                if( OGRGeometryFactory::haveGEOS() )
                {
                    /* We need to read the full geometry */
                    if (poGeometry == NULL)
                    {
                        if (psShape == &sShape)
                            psShape = SHPReadObject( hSHP, iShape);
                        if (psShape)
                        {
                            poGeometry =
                                SHPReadOGRObject( hSHP, iShape, psShape );
                            psShape = NULL;
                        }
                    }
                    if( poGeometry == NULL ||
                        m_poFilterGeom->Intersects( poGeometry ) )
                        nFeatureCount ++;
                }
                else
                    nFeatureCount ++;
            }

            delete poGeometry;
        }
        else
            nFeatureCount ++;

        if (psShape && psShape != &sShape)
            SHPDestroyObject( psShape );
    }

    return nFeatureCount;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

int OGRShapeLayer::GetFeatureCount( int bForce )

{
    /* Check if the spatial filter is non-trivial */
    int bHasTrivialSpatialFilter;
    if (m_poFilterGeom != NULL)
    {
        OGREnvelope oEnvelope;
        m_poFilterGeom->getEnvelope( &oEnvelope );

        OGREnvelope oLayerExtent;
        if (GetExtent(&oLayerExtent, TRUE) == OGRERR_NONE &&
            oEnvelope.Contains(oLayerExtent))
        {
            bHasTrivialSpatialFilter = TRUE;
        }
        else
            bHasTrivialSpatialFilter = FALSE;
    }
    else
        bHasTrivialSpatialFilter = TRUE;


    if( bHasTrivialSpatialFilter && m_poAttrQuery == NULL )
        return nTotalShapeCount;

    if (!TouchLayer())
        return 0;

    /* Spatial filter only */
    if( m_poAttrQuery == NULL && hSHP != NULL )
    {
        return GetFeatureCountWithSpatialFilterOnly();
    }

    /* Attribute filter only */
    if( m_poAttrQuery != NULL )
    {
        /* Let's see if we can ignore reading geometries */
        int bSaveGeometryIgnored = poFeatureDefn->IsGeometryIgnored();
        if (!AttributeFilterEvaluationNeedsGeometry())
            poFeatureDefn->SetGeometryIgnored(TRUE);

        int nRet = OGRLayer::GetFeatureCount( bForce );

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

OGRErr OGRShapeLayer::GetExtent (OGREnvelope *psExtent, int bForce)

{
    UNREFERENCED_PARAM( bForce );

    if (!TouchLayer())
        return OGRERR_FAILURE;

    double adMin[4], adMax[4];

    if( hSHP == NULL )
        return OGRERR_FAILURE;

    SHPGetInfo(hSHP, NULL, NULL, adMin, adMax);

    psExtent->MinX = adMin[0];
    psExtent->MinY = adMin[1];
    psExtent->MaxX = adMax[0];
    psExtent->MaxY = adMax[1];

    return OGRERR_NONE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRShapeLayer::TestCapability( const char * pszCap )

{
    if (!TouchLayer())
        return FALSE;

    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite) 
             || EQUAL(pszCap,OLCRandomWrite) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_poFilterGeom == NULL || CheckForQIX();

    else if( EQUAL(pszCap,OLCDeleteFeature) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return CheckForQIX();

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return TRUE;

    else if( EQUAL(pszCap,OLCFastSetNextByIndex) )
        return m_poFilterGeom == NULL && m_poAttrQuery == NULL;

    else if( EQUAL(pszCap,OLCCreateField) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCDeleteField) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCReorderFields) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCAlterFieldDefn) )
        return bUpdateAccess;

    else if( EQUAL(pszCap,OLCIgnoreFields) )
        return TRUE;

    else if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return strlen(osEncoding) > 0; /* if encoding is defined, we are able to convert to UTF-8 */

    else 
        return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRShapeLayer::CreateField( OGRFieldDefn *poFieldDefn, int bApproxOK )

{
    if (!TouchLayer())
        return OGRERR_FAILURE;

    CPLAssert( NULL != poFieldDefn );
    
    int         iNewField;

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create fields on a read-only shapefile layer.\n");
        return OGRERR_FAILURE;

    }

    int bDBFJustCreated = FALSE;
    if( hDBF == NULL )
    {
        CPLString osFilename = CPLResetExtension( pszFullName, "dbf" );
        hDBF = DBFCreate( osFilename );

        if( hDBF == NULL )
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Failed to create DBF file `%s'.\n",
                      osFilename.c_str() );
            return OGRERR_FAILURE;
        }

        bDBFJustCreated = TRUE;
    }

    if ( poFeatureDefn->GetFieldCount() == 255 )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Creating a 256th field, but some DBF readers might only support 255 fields" );
    }
    if ( hDBF->nHeaderLength + 32 > 65535 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Cannot add more fields in DBF file.");
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Normalize field name                                            */
/* -------------------------------------------------------------------- */
        
    char szNewFieldName[10 + 1];
    char * pszTmp = NULL;
    int nRenameNum = 1;

    size_t nNameSize = strlen( poFieldDefn->GetNameRef() );
    pszTmp = CPLScanString( poFieldDefn->GetNameRef(),
                            MIN( nNameSize, 10) , TRUE, TRUE);
    strncpy(szNewFieldName, pszTmp, 10);
    szNewFieldName[10] = '\0';

    if( !bApproxOK &&
        ( DBFGetFieldIndex( hDBF, szNewFieldName ) >= 0 ||
          !EQUAL(poFieldDefn->GetNameRef(),szNewFieldName) ) )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Failed to add field named '%s'",
                  poFieldDefn->GetNameRef() );
                  
        CPLFree( pszTmp );
        return OGRERR_FAILURE;
    }

    while( DBFGetFieldIndex( hDBF, szNewFieldName ) >= 0 && nRenameNum < 10 )
        sprintf( szNewFieldName, "%.8s_%.1d", pszTmp, nRenameNum++ );
    while( DBFGetFieldIndex( hDBF, szNewFieldName ) >= 0 && nRenameNum < 100 )
        sprintf( szNewFieldName, "%.8s%.2d", pszTmp, nRenameNum++ );

    CPLFree( pszTmp );
    pszTmp = NULL;
    
    if( DBFGetFieldIndex( hDBF, szNewFieldName ) >= 0 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Too many field names like '%s' when truncated to 10 letters "
                  "for Shapefile format.",
                  poFieldDefn->GetNameRef() );//One hundred similar field names!!?
    }

    if( !EQUAL(poFieldDefn->GetNameRef(),szNewFieldName) )
        CPLError( CE_Warning, CPLE_NotSupported,
                  "Normalized/laundered field name: '%s' to '%s'", 
                  poFieldDefn->GetNameRef(),
                  szNewFieldName );
                  
    // Set field name with normalized value
    OGRFieldDefn oModFieldDefn(poFieldDefn);
    oModFieldDefn.SetName(szNewFieldName);

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
            if (nWidth == 0) nWidth = 10;
            break;

        case OFTReal:
            chType = 'N';
            nWidth = oModFieldDefn.GetWidth();
            nDecimals = oModFieldDefn.GetPrecision();
            if (nWidth == 0)
            {
                nWidth = 24;
                nDecimals = 15;
            }
            break;

        case OFTString:
            chType = 'C';
            nWidth = oModFieldDefn.GetWidth();
            if (nWidth == 0) nWidth = 80;
            else if (nWidth > 255)
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                        "Field %s of width %d truncated to %d.",
                        oModFieldDefn.GetNameRef(), nWidth, 255 );
                nWidth = 255;
            }
            break;

        case OFTDate:
            chType = 'D';
            nWidth = 8;
            break;

        case OFTDateTime:
            CPLError( CE_Warning, CPLE_NotSupported,
                    "Field %s create as date field, though DateTime requested.",
                    oModFieldDefn.GetNameRef() );
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

    if ( hDBF->nRecordLength + nWidth > 65535 )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create field %s in Shape DBF file. "
                  "Maximum record length reached.",
                  oModFieldDefn.GetNameRef() );
        return OGRERR_FAILURE;
    }

    iNewField =
        DBFAddNativeFieldType( hDBF, oModFieldDefn.GetNameRef(),
                               chType, nWidth, nDecimals );

    if( iNewField != -1 )
    {
        poFeatureDefn->AddFieldDefn( &oModFieldDefn );

        if( bDBFJustCreated )
        {
            for(int i=0;i<nTotalShapeCount;i++)
            {
                DBFWriteNULLAttribute( hDBF, i, 0 );
            }
        }

        return OGRERR_NONE;
    }
    else        
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Can't create field %s in Shape DBF file, reason unknown.",
                  oModFieldDefn.GetNameRef() );

        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                            DeleteField()                             */
/************************************************************************/

OGRErr OGRShapeLayer::DeleteField( int iField )
{
    if (!TouchLayer())
        return OGRERR_FAILURE;

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't delete fields on a read-only shapefile layer.");
        return OGRERR_FAILURE;
    }

    if (iField < 0 || iField >= poFeatureDefn->GetFieldCount())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

    if ( DBFDeleteField( hDBF, iField ) )
    {
        return poFeatureDefn->DeleteFieldDefn( iField );
    }
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                           ReorderFields()                            */
/************************************************************************/

OGRErr OGRShapeLayer::ReorderFields( int* panMap )
{
    if (!TouchLayer())
        return OGRERR_FAILURE;

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't reorder fields on a read-only shapefile layer.");
        return OGRERR_FAILURE;
    }

    if (poFeatureDefn->GetFieldCount() == 0)
        return OGRERR_NONE;

    OGRErr eErr = OGRCheckPermutation(panMap, poFeatureDefn->GetFieldCount());
    if (eErr != OGRERR_NONE)
        return eErr;

    if ( DBFReorderFields( hDBF, panMap ) )
    {
        return poFeatureDefn->ReorderFieldDefns( panMap );
    }
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                           AlterFieldDefn()                           */
/************************************************************************/

OGRErr OGRShapeLayer::AlterFieldDefn( int iField, OGRFieldDefn* poNewFieldDefn, int nFlags )
{
    if (!TouchLayer())
        return OGRERR_FAILURE;

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't alter field definition on a read-only shapefile layer.");
        return OGRERR_FAILURE;
    }

    if (iField < 0 || iField >= poFeatureDefn->GetFieldCount())
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Invalid field index");
        return OGRERR_FAILURE;
    }

    OGRFieldDefn* poFieldDefn = poFeatureDefn->GetFieldDefn(iField);

    char chNativeType;
    char            szFieldName[20];
    int             nWidth, nPrecision;
    OGRFieldType    eType = poFieldDefn->GetType();
    DBFFieldType    eDBFType;

    chNativeType = DBFGetNativeFieldType( hDBF, iField );
    eDBFType = DBFGetFieldInfo( hDBF, iField, szFieldName,
                                &nWidth, &nPrecision );

    if ((nFlags & ALTER_TYPE_FLAG) &&
        poNewFieldDefn->GetType() != poFieldDefn->GetType())
    {
        if (poNewFieldDefn->GetType() != OFTString)
        {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "Can only convert to OFTString");
            return OGRERR_FAILURE;
        }
        else
        {
            chNativeType = 'C';
            eType = poNewFieldDefn->GetType();
        }
    }

    if (nFlags & ALTER_NAME_FLAG)
    {
        strncpy(szFieldName, poNewFieldDefn->GetNameRef(), 10);
        szFieldName[10] = '\0';
    }
    if (nFlags & ALTER_WIDTH_PRECISION_FLAG)
    {
        nWidth = poNewFieldDefn->GetWidth();
        nPrecision = poNewFieldDefn->GetPrecision();
    }

    if ( DBFAlterFieldDefn( hDBF, iField, szFieldName,
                            chNativeType, nWidth, nPrecision) )
    {
        if (nFlags & ALTER_TYPE_FLAG)
            poFieldDefn->SetType(eType);
        if (nFlags & ALTER_NAME_FLAG)
            poFieldDefn->SetName(szFieldName);
        if (nFlags & ALTER_WIDTH_PRECISION_FLAG)
        {
            poFieldDefn->SetWidth(nWidth);
            poFieldDefn->SetPrecision(nPrecision);
        }
        return OGRERR_NONE;
    }
    else
        return OGRERR_FAILURE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRShapeLayer::GetSpatialRef()

{
    if (bSRSSet)
        return poSRS;

    bSRSSet = TRUE;

/* -------------------------------------------------------------------- */
/*      Is there an associated .prj file we can read?                   */
/* -------------------------------------------------------------------- */
    const char  *pszPrjFile = CPLResetExtension( pszFullName, "prj" );
    char    **papszLines;

    char* apszOptions[] = { (char*)"EMIT_ERROR_IF_CANNOT_OPEN_FILE=FALSE", NULL };
    papszLines = CSLLoad2( pszPrjFile, -1, -1, apszOptions );
    if (papszLines == NULL)
    {
        pszPrjFile = CPLResetExtension( pszFullName, "PRJ" );
        papszLines = CSLLoad2( pszPrjFile, -1, -1, apszOptions );
    }

    if( papszLines != NULL )
    {
        poSRS = new OGRSpatialReference();
        if( poSRS->importFromESRI( papszLines ) != OGRERR_NONE )
        {
            delete poSRS;
            poSRS = NULL;
        }
        CSLDestroy( papszLines );
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
    char        abyHeader[100];
    int         nStartPos;

    if( nTotalShapeCount > 0 )
        return FALSE;

    if( hSHP->fpSHX == NULL)
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  " OGRShapeLayer::ResetGeomType failed : SHX file is closed");
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Update .shp header.                                             */
/* -------------------------------------------------------------------- */
    nStartPos = (int)( hSHP->sHooks.FTell( hSHP->fpSHP ) );

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
    nStartPos = (int)( hSHP->sHooks.FTell( hSHP->fpSHX ) );
    
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
    if (!TouchLayer())
        return OGRERR_FAILURE;

    if( bHeaderDirty )
    {
        if( hSHP != NULL )
            SHPWriteHeader( hSHP );

        if( hDBF != NULL )
            DBFUpdateHeader( hDBF );

        bHeaderDirty = FALSE;
    }

    if( hSHP != NULL )
    {
        hSHP->sHooks.FFlush( hSHP->fpSHP );
        if( hSHP->fpSHX != NULL )
            hSHP->sHooks.FFlush( hSHP->fpSHX );
    }

    if( hDBF != NULL )
        hDBF->sHooks.FFlush( hDBF->fp );

    return OGRERR_NONE;
}

/************************************************************************/
/*                          DropSpatialIndex()                          */
/************************************************************************/

OGRErr OGRShapeLayer::DropSpatialIndex()

{
    if (!TouchLayer())
        return OGRERR_FAILURE;

    if( !CheckForQIX() )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Layer %s has no spatial index, DROP SPATIAL INDEX failed.",
                  poFeatureDefn->GetName() );
        return OGRERR_FAILURE;
    }

    SHPCloseDiskTree( hQIX );
    hQIX = NULL;
    bCheckedForQIX = FALSE;
    
    const char *pszQIXFilename;

    pszQIXFilename = CPLResetExtension( pszFullName, "qix" );
    CPLDebug( "SHAPE", "Unlinking index file %s", pszQIXFilename );

    if( VSIUnlink( pszQIXFilename ) != 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Failed to delete file %s.\n%s", 
                  pszQIXFilename, VSIStrerror( errno ) );
        return OGRERR_FAILURE;
    }

    if( !bSbnSbxDeleted )
    {
        const char *pszIndexFilename;
        const char papszExt[2][4] = { "sbn", "sbx" };
        int i;
        for( i = 0; i < 2; i++ )
        {
            pszIndexFilename = CPLResetExtension( pszFullName, papszExt[i] );
            CPLDebug( "SHAPE", "Trying to unlink index file %s", pszIndexFilename );

            if( VSIUnlink( pszIndexFilename ) != 0 )
            {
                CPLDebug( "SHAPE",
                          "Failed to delete file %s.\n%s", 
                          pszIndexFilename, VSIStrerror( errno ) );
            }
        }
    }
    bSbnSbxDeleted = TRUE;

    return OGRERR_NONE;
}

/************************************************************************/
/*                         CreateSpatialIndex()                         */
/************************************************************************/

OGRErr OGRShapeLayer::CreateSpatialIndex( int nMaxDepth )

{
    if (!TouchLayer())
        return OGRERR_FAILURE;

/* -------------------------------------------------------------------- */
/*      If we have an existing spatial index, blow it away first.       */
/* -------------------------------------------------------------------- */
    if( CheckForQIX() )
        DropSpatialIndex();

    bCheckedForQIX = FALSE;

/* -------------------------------------------------------------------- */
/*      Build a quadtree structure for this file.                       */
/* -------------------------------------------------------------------- */
    SHPTree	*psTree;

    SyncToDisk();
    psTree = SHPCreateTree( hSHP, 2, nMaxDepth, NULL, NULL );

    if( NULL == psTree )
    {
        // TODO - mloskot: Is it better to return OGRERR_NOT_ENOUGH_MEMORY?

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
    char *pszQIXFilename;

    pszQIXFilename = CPLStrdup(CPLResetExtension( pszFullName, "qix" ));

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
/*                               Repack()                               */
/*                                                                      */
/*      Repack the shape and dbf file, dropping deleted records.        */
/*      FIDs may change.                                                */
/************************************************************************/

OGRErr OGRShapeLayer::Repack()

{
    if (!TouchLayer())
        return OGRERR_FAILURE;

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The REPACK operation is not permitted on a read-only shapefile." );
        return OGRERR_FAILURE;
    }
    
    if( hDBF == NULL )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Attempt to repack a shapefile with no .dbf file not supported.");
        return OGRERR_FAILURE;
    }
    
/* -------------------------------------------------------------------- */
/*      Build a list of records to be dropped.                          */
/* -------------------------------------------------------------------- */
    int *panRecordsToDelete = (int *) 
        CPLMalloc(sizeof(int)*(nTotalShapeCount+1));
    int nDeleteCount = 0;
    int iShape = 0;
    OGRErr eErr = OGRERR_NONE;

    for( iShape = 0; iShape < nTotalShapeCount; iShape++ )
    {
        if( DBFIsRecordDeleted( hDBF, iShape ) )
            panRecordsToDelete[nDeleteCount++] = iShape;
    }
    panRecordsToDelete[nDeleteCount] = -1;

/* -------------------------------------------------------------------- */
/*      If there are no records marked for deletion, we take no         */
/*      action.                                                         */
/* -------------------------------------------------------------------- */
    if( nDeleteCount == 0 )
    {
        CPLFree( panRecordsToDelete );
        return OGRERR_NONE;
    }

/* -------------------------------------------------------------------- */
/*      Find existing filenames with exact case (see #3293).            */
/* -------------------------------------------------------------------- */
    CPLString osDirname(CPLGetPath(pszFullName));
    CPLString osBasename(CPLGetBasename(pszFullName));
    
    CPLString osDBFName, osSHPName, osSHXName;
    char **papszCandidates = CPLReadDir( osDirname );
    int i = 0;
    while(papszCandidates != NULL && papszCandidates[i] != NULL)
    {
        CPLString osCandidateBasename = CPLGetBasename(papszCandidates[i]);
        CPLString osCandidateExtension = CPLGetExtension(papszCandidates[i]);
        if (osCandidateBasename.compare(osBasename) == 0)
        {
            if (EQUAL(osCandidateExtension, "dbf"))
                osDBFName = CPLFormFilename(osDirname, papszCandidates[i], NULL);
            else if (EQUAL(osCandidateExtension, "shp"))
                osSHPName = CPLFormFilename(osDirname, papszCandidates[i], NULL);
            else if (EQUAL(osCandidateExtension, "shx"))
                osSHXName = CPLFormFilename(osDirname, papszCandidates[i], NULL);
        }
        
        i++;
    }
    CSLDestroy(papszCandidates);
    papszCandidates = NULL;
    
    if (osDBFName.size() == 0)
    {
        /* Should not happen, really */
        CPLFree( panRecordsToDelete );
        return OGRERR_FAILURE;
    }
    
/* -------------------------------------------------------------------- */
/*      Cleanup any existing spatial index.  It will become             */
/*      meaningless when the fids change.                               */
/* -------------------------------------------------------------------- */
    if( CheckForQIX() )
        DropSpatialIndex();

/* -------------------------------------------------------------------- */
/*      Create a new dbf file, matching the old.                        */
/* -------------------------------------------------------------------- */
    DBFHandle hNewDBF = NULL;
    
    CPLString oTempFile(CPLFormFilename(osDirname, osBasename, NULL));
    oTempFile += "_packed.dbf";

    hNewDBF = DBFCloneEmpty( hDBF, oTempFile );
    if( hNewDBF == NULL )
    {
        CPLFree( panRecordsToDelete );

        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to create temp file %s.", 
                  oTempFile.c_str() );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Copy over all records that are not deleted.                     */
/* -------------------------------------------------------------------- */
    int iDestShape = 0;
    int iNextDeletedShape = 0;

    for( iShape = 0; 
         iShape < nTotalShapeCount && eErr == OGRERR_NONE; 
         iShape++ )
    {
        if( panRecordsToDelete[iNextDeletedShape] == iShape )
            iNextDeletedShape++;
        else
        {
            void *pTuple = (void *) DBFReadTuple( hDBF, iShape );
            if( pTuple == NULL )
                eErr = OGRERR_FAILURE;
            else if( !DBFWriteTuple( hNewDBF, iDestShape++, pTuple ) )
                eErr = OGRERR_FAILURE;
        }                           
    }

    if( eErr != OGRERR_NONE )
    {
        CPLFree( panRecordsToDelete );
        VSIUnlink( oTempFile );
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Cleanup the old .dbf and rename the new one.                    */
/* -------------------------------------------------------------------- */
    DBFClose( hDBF );
    DBFClose( hNewDBF );
    hDBF = hNewDBF = NULL;
    
    VSIUnlink( osDBFName );
        
    if( VSIRename( oTempFile, osDBFName ) != 0 )
    {
        CPLDebug( "Shape", "Can not rename DBF file: %s", VSIStrerror( errno ) );
        CPLFree( panRecordsToDelete );
        return OGRERR_FAILURE;
    }
    
/* -------------------------------------------------------------------- */
/*      Now create a shapefile matching the old one.                    */
/* -------------------------------------------------------------------- */
    if( hSHP != NULL )
    {
        SHPHandle hNewSHP = NULL;
        
        if (osSHPName.size() == 0 || osSHXName.size() == 0)
        {
            /* Should not happen, really */
            CPLFree( panRecordsToDelete );
            return OGRERR_FAILURE;
        }

        oTempFile = CPLFormFilename(osDirname, osBasename, NULL);
        oTempFile += "_packed.shp";

        hNewSHP = SHPCreate( oTempFile, hSHP->nShapeType );
        if( hNewSHP == NULL )
        {
            CPLFree( panRecordsToDelete );
            return OGRERR_FAILURE;
        }

/* -------------------------------------------------------------------- */
/*      Copy over all records that are not deleted.                     */
/* -------------------------------------------------------------------- */
        iNextDeletedShape = 0;

        for( iShape = 0; 
             iShape < nTotalShapeCount && eErr == OGRERR_NONE; 
             iShape++ )
        {
            if( panRecordsToDelete[iNextDeletedShape] == iShape )
                iNextDeletedShape++;
            else
            {
                SHPObject *hObject;

                hObject = SHPReadObject( hSHP, iShape );
                if( hObject == NULL )
                    eErr = OGRERR_FAILURE;
                else if( SHPWriteObject( hNewSHP, -1, hObject ) == -1 )
                    eErr = OGRERR_FAILURE;

                if( hObject )
                    SHPDestroyObject( hObject );
            }
        }

        if( eErr != OGRERR_NONE )
        {
            CPLFree( panRecordsToDelete );
            VSIUnlink( CPLResetExtension( oTempFile, "shp" ) );
            VSIUnlink( CPLResetExtension( oTempFile, "shx" ) );
            return eErr;
        }

/* -------------------------------------------------------------------- */
/*      Cleanup the old .shp/.shx and rename the new one.               */
/* -------------------------------------------------------------------- */
        SHPClose( hSHP );
        SHPClose( hNewSHP );
        hSHP = hNewSHP = NULL;

        VSIUnlink( osSHPName );
        VSIUnlink( osSHXName );

        oTempFile = CPLResetExtension( oTempFile, "shp" );
        if( VSIRename( oTempFile, osSHPName ) != 0 )
        {
            CPLDebug( "Shape", "Can not rename SHP file: %s", VSIStrerror( errno ) );
            CPLFree( panRecordsToDelete );
            return OGRERR_FAILURE;
        }
    
        oTempFile = CPLResetExtension( oTempFile, "shx" );
        if( VSIRename( oTempFile, osSHXName ) != 0 )
        {
            CPLDebug( "Shape", "Can not rename SHX file: %s", VSIStrerror( errno ) );
            CPLFree( panRecordsToDelete );
            return OGRERR_FAILURE;
        }
    }
    
    CPLFree( panRecordsToDelete );
    panRecordsToDelete = NULL;

/* -------------------------------------------------------------------- */
/*      Reopen the shapefile                                            */
/*                                                                      */
/* We do not need to reimplement OGRShapeDataSource::OpenFile() here    */  
/* with the fully featured error checking.                              */
/* If all operations above succeeded, then all necessery files are      */
/* in the right place and accessible.                                   */
/* -------------------------------------------------------------------- */
    CPLAssert( NULL == hSHP );
    CPLAssert( NULL == hDBF && NULL == hNewDBF );
    
    CPLPushErrorHandler( CPLQuietErrorHandler );
    
    const char* pszAccess = NULL;
    if( bUpdateAccess )
        pszAccess = "r+";
    else
        pszAccess = "r";
    
    hSHP = SHPOpen ( CPLResetExtension( pszFullName, "shp" ) , pszAccess );
    hDBF = DBFOpen ( CPLResetExtension( pszFullName, "dbf" ) , pszAccess );
    
    CPLPopErrorHandler();
    
    if( NULL == hSHP || NULL == hDBF )
    {
        CPLString osMsg(CPLGetLastErrorMsg());
        CPLError( CE_Failure, CPLE_OpenFailed, "%s", osMsg.c_str() );

        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Update total shape count.                                       */
/* -------------------------------------------------------------------- */
    nTotalShapeCount = hDBF->nRecords;

    return OGRERR_NONE;
}

/************************************************************************/
/*                        RecomputeExtent()                             */
/*                                                                      */
/*      Force recomputation of the extent of the .SHP file              */
/************************************************************************/

OGRErr OGRShapeLayer::RecomputeExtent()
{
    if (!TouchLayer())
        return OGRERR_FAILURE;

    if( !bUpdateAccess )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The RECOMPUTE EXTENT operation is not permitted on a read-only shapefile." );
        return OGRERR_FAILURE;
    }
    
    if( hSHP == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "The RECOMPUTE EXTENT operation is not permitted on a layer without .SHP file." );
        return OGRERR_FAILURE;
    }
    
    double adBoundsMin[4] = { 0.0, 0.0, 0.0, 0.0 };
    double adBoundsMax[4] = { 0.0, 0.0, 0.0, 0.0 };

    int bHasBeenInit = FALSE;

    for( int iShape = 0; 
         iShape < nTotalShapeCount; 
         iShape++ )
    {
        if( hDBF == NULL || !DBFIsRecordDeleted( hDBF, iShape ) )
        {
            SHPObject *psObject = SHPReadObject( hSHP, iShape );
            if ( psObject != NULL &&
                 psObject->nSHPType != SHPT_NULL &&
                 psObject->nVertices != 0 )
            {
                if( !bHasBeenInit )
                {
                    bHasBeenInit = TRUE;
                    adBoundsMin[0] = adBoundsMax[0] = psObject->padfX[0];
                    adBoundsMin[1] = adBoundsMax[1] = psObject->padfY[0];
                    adBoundsMin[2] = adBoundsMax[2] = psObject->padfZ[0];
                    adBoundsMin[3] = adBoundsMax[3] = psObject->padfM[0];
                }

                for( int i = 0; i < psObject->nVertices; i++ )
                {
                    adBoundsMin[0] = MIN(adBoundsMin[0],psObject->padfX[i]);
                    adBoundsMin[1] = MIN(adBoundsMin[1],psObject->padfY[i]);
                    adBoundsMin[2] = MIN(adBoundsMin[2],psObject->padfZ[i]);
                    adBoundsMin[3] = MIN(adBoundsMin[3],psObject->padfM[i]);
                    adBoundsMax[0] = MAX(adBoundsMax[0],psObject->padfX[i]);
                    adBoundsMax[1] = MAX(adBoundsMax[1],psObject->padfY[i]);
                    adBoundsMax[2] = MAX(adBoundsMax[2],psObject->padfZ[i]);
                    adBoundsMax[3] = MAX(adBoundsMax[3],psObject->padfM[i]);
                }
            }
            SHPDestroyObject(psObject);
        }
    }

    if( memcmp(hSHP->adBoundsMin, adBoundsMin, 4*sizeof(double)) != 0 ||
        memcmp(hSHP->adBoundsMax, adBoundsMax, 4*sizeof(double)) != 0 )
    {
        bHeaderDirty = TRUE;
        hSHP->bUpdated = TRUE;
        memcpy(hSHP->adBoundsMin, adBoundsMin, 4*sizeof(double));
        memcpy(hSHP->adBoundsMax, adBoundsMax, 4*sizeof(double));
    }
    
    return OGRERR_NONE;
}


/************************************************************************/
/*                              TouchLayer()                            */
/************************************************************************/

int OGRShapeLayer::TouchLayer()
{
    poDS->SetLastUsedLayer(this);

    if (eFileDescriptorsState == FD_OPENED)
        return TRUE;
    else if (eFileDescriptorsState == FD_CANNOT_REOPEN)
        return FALSE;
    else
        return ReopenFileDescriptors();
}

/************************************************************************/
/*                        ReopenFileDescriptors()                       */
/************************************************************************/

int OGRShapeLayer::ReopenFileDescriptors()
{
    CPLDebug("SHAPE", "ReopenFileDescriptors(%s)", pszFullName);

    if( bHSHPWasNonNULL )
    {
        if( bUpdateAccess )
            hSHP = SHPOpen( pszFullName, "r+" );
        else
            hSHP = SHPOpen( pszFullName, "r" );

        if (hSHP == NULL)
        {
            eFileDescriptorsState = FD_CANNOT_REOPEN;
            return FALSE;
        }
    }

    if( bHDBFWasNonNULL )
    {
        if( bUpdateAccess )
            hDBF = DBFOpen( pszFullName, "r+" );
        else
            hDBF = DBFOpen( pszFullName, "r" );

        if (hDBF == NULL)
        {
            CPLError(CE_Failure, CPLE_OpenFailed,
                     "Cannot reopen %s", CPLResetExtension(pszFullName, "dbf"));
            eFileDescriptorsState = FD_CANNOT_REOPEN;
            return FALSE;
        }
    }

    eFileDescriptorsState = FD_OPENED;

    return TRUE;
}

/************************************************************************/
/*                        CloseFileDescriptors()                        */
/************************************************************************/

void OGRShapeLayer::CloseFileDescriptors()
{
    CPLDebug("SHAPE", "CloseFileDescriptors(%s)", pszFullName);

    if( hDBF != NULL )
        DBFClose( hDBF );
    hDBF = NULL;

    if( hSHP != NULL )
        SHPClose( hSHP );
    hSHP = NULL;

    /* We close QIX and reset the check flag, so that CheckForQIX() */
    /* will retry opening it if necessary when the layer is active again */
    if( hQIX != NULL )
        SHPCloseDiskTree( hQIX ); 
    hQIX = NULL;
    bCheckedForQIX = FALSE;

    eFileDescriptorsState = FD_CLOSED;
}
