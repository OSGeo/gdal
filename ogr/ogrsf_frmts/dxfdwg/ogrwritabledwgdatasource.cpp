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
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 *
 * $Log$
 * Revision 1.3  2005/11/15 14:57:20  fwarmerdam
 * various test updates
 *
 * Revision 1.2  2005/11/10 21:31:48  fwarmerdam
 * preliminary version
 *
 * Revision 1.1  2005/11/07 04:43:24  fwarmerdam
 * New
 *
 */

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
    
    try 
    {
        pDb->writeFile(&fb, fileType, outVer, true);
    }
    catch( ... )
    {
        printf( "Catch exception from writeFile\n" );
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
//    pDb->setEXTMIN(OdGePoint3d(-10000000.0, -10000000.0, -10000.0));
//    pDb->setEXTMAX(OdGePoint3d(10000000.0, 10000000.0, 10000.0));
    pDb->setEXTMIN(OdGePoint3d(1296000, 228000, 0));
    pDb->setEXTMAX(OdGePoint3d(1302700, 238000, 0));

    // Set Creation and last update times
    OdDbDate date;
    date.setDate(1, 20, 2001);
    date.setTime(13, 0, 0, 0);
    odDbSetTDUCREATE(*pDb, date);

    date.setTime(18, 30, 0, 0);
    odDbSetTDUUPDATE(*pDb, date);

    pDb->setTILEMODE(1);  // 0 for paperspace, 1 for modelspace
    pDb->newRegApp("ODA");

    OdDbBlockTableRecordPtr pPs = pDb->getPaperSpaceId().safeOpenObject(OdDb::kForWrite);

    pVp = OdDbViewport::createObject();
  
//    pVp->setCenterPoint(OdGePoint3d(5.375, 4.125, 0));
//    pVp->setWidth(14.63);
//    pVp->setHeight(9.0);

    pVp->setCenterPoint(OdGePoint3d(1300000, 233000, 0));
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

    // Add a new layer to the drawing
    OdDbLayerTablePtr pLayers;
    OdDbLayerTableRecordPtr pLayer;
    OdDbObjectId newLayerId;

    pLayers = pDb->getLayerTableId().safeOpenObject(OdDb::kForWrite);
    pLayer = OdDbLayerTableRecord::createObject();
    
    // Name must be set before a table object is added to a table.
    pLayer->setName("Layer1");
    
    // Add the object to the table.
    newLayerId = pLayers->add(pLayer);
#ifdef notdef    
    OdDbViewportTablePtr pVpTable = pDb->getViewportTableId().openObject(OdDb::kForWrite);
    OdDbObjectId vpID = pVpTable->getActiveViewportId();
    OdDbViewportTableRecordPtr vPortRec = vpID.openObject(OdDb::kForWrite);
    vPortRec->setCenterPoint(OdGePoint2d(11.26, 4.5));
    vPortRec->setWidth(22.53);
    vPortRec->setHeight(9.);
#endif
  
    OdDbCirclePtr pCircle = OdDbCircle::createObject();
    pCircle->setCenter( OdGePoint3d(1300000, 233000, 0) );
    pCircle->setRadius(3000);
    pCircle->setLayer(newLayerId, false);
    pPs->appendOdDbEntity(pCircle);

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





