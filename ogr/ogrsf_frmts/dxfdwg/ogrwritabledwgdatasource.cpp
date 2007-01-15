/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRWritableDWGDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
 ****************************************************************************/

#include "ogr_dwg.h"
#include "cpl_conv.h"
#include "cpl_string.h"

#include "Ge/GeCircArc2d.h"
#include "Ge/GeScale3d.h"
#include "Ge/GeExtents3d.h"

#include "DbViewportTable.h"
#include "DbViewportTableRecord.h"
#include "DbViewport.h"
#include "DbBlockTable.h"
#include "DbBlockTableRecord.h"
#include "DbCircle.h"
#include "DbLayerTable.h"
#include "DbLayerTableRecord.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                      OGRWritableDWGDataSource()                      */
/************************************************************************/

OGRWritableDWGDataSource::OGRWritableDWGDataSource( const char *pszOutClass )

{
    osOutClass = pszOutClass;
    papszOptions = NULL;

    nLayers = 0;
    papoLayers = NULL;
}

/************************************************************************/
/*                     ~OGRWritableDWGDataSource()                      */
/************************************************************************/

OGRWritableDWGDataSource::~OGRWritableDWGDataSource()

{
    OdDb::SaveType fileType = OdDb::kDwg;
    OdDb::DwgVersion outVer = OdDb::vAC12;
    OdWrFileBuf fb(osFilename);

    if( osOutClass == "DWG" ) 
        fileType = OdDb::kDwg;
    else if( osOutClass == "DXF" )
        fileType = OdDb::kDxf;

    const char *pszVersion = CSLFetchNameValue( papszOptions, "VERSION" );
    if( pszVersion == NULL )
        outVer = OdDb::vAC12;
    else if( EQUAL(pszVersion,"12" ) )
        outVer = OdDb::vAC12;
    else if( EQUAL(pszVersion,"13" ) )
        outVer = OdDb::vAC13;
    else if( EQUAL(pszVersion,"14" ) )
        outVer = OdDb::vAC14;
    else if( EQUAL(pszVersion,"15" ) )
        outVer = OdDb::vAC15;
    else if( EQUAL(pszVersion,"18" ) )
        outVer = OdDb::vAC18;
    
/* -------------------------------------------------------------------- */
/*      Reset the viewports based on the available data extents.        */
/* -------------------------------------------------------------------- */
    try 
    {
        pDb->setEXTMIN(OdGePoint3d(sExtent.MinX, sExtent.MinY, 0));
        pDb->setEXTMAX(OdGePoint3d(sExtent.MaxX, sExtent.MaxY, 0));

        pVp->setCenterPoint(OdGePoint3d((sExtent.MinX + sExtent.MaxX) * 0.5,
                                        (sExtent.MinY + sExtent.MaxY) * 0.5,
                                        0 ) );
        pVp->setWidth( sExtent.MaxX - sExtent.MinX );
        pVp->setHeight( sExtent.MaxY - sExtent.MinY );
        
        pVp->setViewCenter(OdGePoint2d((sExtent.MinX + sExtent.MaxX) * 0.5,
                                        (sExtent.MinY + sExtent.MaxY) * 0.5));
        pVp->setViewTarget(OdGePoint3d((sExtent.MinX + sExtent.MaxX) * 0.5,
                                        (sExtent.MinY + sExtent.MaxY) * 0.5,
                                        0 ) );
        pVp->setViewDirection(OdGeVector3d(0, 0, 1));
        pVp->setViewHeight(sExtent.MaxY - sExtent.MinY);

        pVp->setCenterPoint(OdGePoint3d((sExtent.MinX + sExtent.MaxX) * 0.5,
                                        (sExtent.MinY + sExtent.MaxY) * 0.5,
                                        0 ) );
        pVm->setWidth( sExtent.MaxX - sExtent.MinX );
        pVm->setHeight( sExtent.MaxY - sExtent.MinY );
        pVm->zoomExtents();

        pVm->setViewCenter(OdGePoint2d((sExtent.MinX + sExtent.MaxX) * 0.5,
                                        (sExtent.MinY + sExtent.MaxY) * 0.5));
        pVm->setViewTarget(OdGePoint3d((sExtent.MinX + sExtent.MaxX) * 0.5,
                                        (sExtent.MinY + sExtent.MaxY) * 0.5,
                                        0 ) );
        pVm->setViewDirection(OdGeVector3d(0, 0, 1));
        pVm->setViewHeight(sExtent.MaxY - sExtent.MinY);
    }
    catch( OdError& e )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "resetting extents:%s", 
                  (const char *) e.description() );
    }

/* -------------------------------------------------------------------- */
/*      Release all pointer references.                                 */
/* -------------------------------------------------------------------- */
    pVp->release();
    pVm->release();
    pPs->release();
    pMs->release();
        
/* -------------------------------------------------------------------- */
/*      Write out file.                                                 */
/* -------------------------------------------------------------------- */
    try 
    {
        pDb->writeFile(&fb, fileType, outVer, true);
    }
    catch( OdError& e )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "writeFile:%s", 
                  (const char *) e.description() );
    }

    CSLDestroy( papszOptions );
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

int OGRWritableDWGDataSource::Create( const char *pszFilename, 
                                      char **papszOptionsIn )

{
    osFilename = pszFilename;
    papszOptions = CSLDuplicate( papszOptionsIn );

//    svcs.disableOutput(false);
    odInitialize(&svcs);

    pDb = svcs.createDatabase();

    // Set the drawing extents
    pDb->setEXTMIN(OdGePoint3d(-10000000.0, -10000000.0, 0));
    pDb->setEXTMAX(OdGePoint3d(10000000.0, 10000000.0, 0));

    // Set Creation and last update times
    OdDbDate date;
    date.setDate(1, 20, 2001);
    date.setTime(13, 0, 0, 0);
    odDbSetTDUCREATE(*pDb, date);

    date.setTime(18, 30, 0, 0);
    odDbSetTDUUPDATE(*pDb, date);

    pDb->setTILEMODE(1);  // 0 for paperspace, 1 for modelspace
//    pDb->newRegApp("ODA");

/* -------------------------------------------------------------------- */
/*      paper space viewport.                                           */
/* -------------------------------------------------------------------- */
    pPs = pDb->getPaperSpaceId().safeOpenObject(OdDb::kForWrite);

    pVp = OdDbViewport::createObject();
  
    pVp->setCenterPoint(OdGePoint3d(0, 0, 0));
    pVp->setWidth(10000);
    pVp->setHeight(10000);

    pVp->setViewTarget(OdGePoint3d(0, 0, 0));
    pVp->setViewDirection(OdGeVector3d(0, 0, 1));
    pVp->setViewHeight(9.0);

    pVp->setLensLength(50.0);
    pVp->setViewCenter(OdGePoint2d(5.375, 4.125));
    pVp->setSnapIncrement(OdGeVector2d(0.5, 0.5));
    pVp->setGridIncrement(OdGeVector2d(0.5, 0.5));
    pVp->setCircleSides(OdUInt16(100));

    pPs->appendOdDbEntity(pVp);

/* -------------------------------------------------------------------- */
/*      model space viewport.                                           */
/* -------------------------------------------------------------------- */
    pMs = pDb->getModelSpaceId().safeOpenObject(OdDb::kForWrite);

    pVm = OdDbViewport::createObject();
  
    pVm->setCenterPoint(OdGePoint3d(0,0,0));
    pVm->setWidth(10000);
    pVm->setHeight(10000);

    pVm->setViewTarget(OdGePoint3d(0, 0, 0));
    pVm->setViewDirection(OdGeVector3d(0, 0, 1));
    pVm->setViewHeight(9.0);

    pVm->setLensLength(50.0);
    pVm->setViewCenter(OdGePoint2d(5.375, 4.125));
    pVm->setSnapIncrement(OdGeVector2d(0.5, 0.5));
    pVm->setGridIncrement(OdGeVector2d(0.5, 0.5));
    pVm->setCircleSides(OdUInt16(100));

    pMs->appendOdDbEntity(pVm);

    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRWritableDWGDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *OGRWritableDWGDataSource::GetName()
    
{
    return osFilename;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRWritableDWGDataSource::GetLayer( int iLayer )
    
{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *OGRWritableDWGDataSource::CreateLayer( const char *pszLayerName, 
                                                 OGRSpatialReference *, 
                                                 OGRwkbGeometryType, 
                                                 char ** papszLayerOptions )
    
{
    OGRWritableDWGLayer *poLayer;

    poLayer = new OGRWritableDWGLayer( pszLayerName, papszLayerOptions, 
                                       this );

    papoLayers = (OGRWritableDWGLayer **) 
        CPLRealloc(papoLayers, sizeof(void*) * ++nLayers );
    papoLayers[nLayers-1] = poLayer;

    return poLayer;
}

/************************************************************************/
/*                            ExtendExtent()                            */
/************************************************************************/

void OGRWritableDWGDataSource::ExtendExtent( OGRGeometry * poGeometry )

{
    if( poGeometry == NULL )
        return;

    OGREnvelope sThisEnvelope;

    poGeometry->getEnvelope( &sThisEnvelope );

    sExtent.Merge( sThisEnvelope );
}


