/******************************************************************************
 * $Id$
 *
 * Project:  OGR/DODS Interface
 * Purpose:  Implements OGRDODSLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.2  2004/01/22 21:15:50  warmerda
 * parse url into components
 *
 * Revision 1.1  2004/01/21 20:08:29  warmerda
 * New
 *
 */

#include "ogr_dods.h"
#include "cpl_conv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            OGRDODSLayer()                            */
/************************************************************************/

OGRDODSLayer::OGRDODSLayer( OGRDODSDataSource *poDSIn, 
                            const char *pszTargetIn,
                            AttrTable *poOGRLayerInfoIn )

{
    poDS = poDSIn;
    poFeatureDefn = NULL;
    poFilterGeom = NULL;
    pszQuery = NULL;
    pszFIDColumn = NULL;
    poSRS = NULL;
    iNextShapeId = 0;
    pszTarget = CPLStrdup( pszTargetIn );
    panFieldMapping = NULL;

    bDataLoaded = FALSE;
    poConnection = NULL;
    poTargetVar = NULL;
    poOGRLayerInfo = poOGRLayerInfoIn;
    bKnowExtent = FALSE;

/* ==================================================================== */
/*      Harvest some metadata if available.                             */
/* ==================================================================== */
    if( poOGRLayerInfo != NULL )
    {
        string oMValue;

/* -------------------------------------------------------------------- */
/*      spatial_ref                                                     */
/* -------------------------------------------------------------------- */
        oMValue = poOGRLayerInfo->get_attr( "spatial_ref" );
        if( oMValue.length() > 0 )
        {
            poSRS = new OGRSpatialReference();
            if( poSRS->SetFromUserInput( oMValue.c_str() ) != OGRERR_NONE )
            {
                CPLError( CE_Warning, CPLE_AppDefined, 
                          "Ignoring unreconised SRS '%s'", 
                          oMValue.c_str() );
                delete poSRS;
                poSRS = NULL;
            }
        }

/* -------------------------------------------------------------------- */
/*      Layer extents.                                                  */
/* -------------------------------------------------------------------- */
        AttrTable *poLayerExt=poOGRLayerInfo->find_container("layer_extents");
        if( poLayerExt != NULL )
        {
            bKnowExtent = TRUE;
            sExtent.MinX = atof(poLayerExt->get_attr("x_min").c_str());
            sExtent.MaxX = atof(poLayerExt->get_attr("x_max").c_str());
            sExtent.MinY = atof(poLayerExt->get_attr("y_min").c_str());
            sExtent.MaxY = atof(poLayerExt->get_attr("y_max").c_str());
        }

    }
}

/************************************************************************/
/*                           ~OGRDODSLayer()                            */
/************************************************************************/

OGRDODSLayer::~OGRDODSLayer()

{
    if( poFilterGeom != NULL )
        delete poFilterGeom;

    if( poSRS != NULL )
        poSRS->Dereference();

    if( poFeatureDefn != NULL )
        delete poFeatureDefn;

    CPLFree( pszFIDColumn );
    pszFIDColumn = NULL;

    CPLFree( panFieldMapping );

    CPLFree( pszTarget );
    pszTarget = NULL;

    if( poConnection != NULL )
        delete poConnection;
}

/************************************************************************/
/*                            BuildFields()                             */
/*                                                                      */
/*      Build the field definition or definitions corresponding to      */
/*      the passed variable and it's children (if it has them).         */
/************************************************************************/

int OGRDODSLayer::BuildFields( BaseType *poTargetVar, 
                               const char *pszPathToVar )

{
    OGRFieldDefn oField( poTargetVar->name().c_str(), OFTInteger );

/* -------------------------------------------------------------------- */
/*      Capture this field definition.                                  */
/* -------------------------------------------------------------------- */
    switch( poTargetVar->type() )
    {
      case dods_byte_c:
      case dods_int16_c:
      case dods_uint16_c:
      case dods_int32_c:
      case dods_uint32_c:
        oField.SetType( OFTInteger );
        break;

      case dods_float32_c:
      case dods_float64_c:
        oField.SetType( OFTReal );
        break;

      case dods_str_c:
      case dods_url_c:
        oField.SetType( OFTString );
        break;

      case dods_sequence_c:
        return TRUE;

      default:
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Add field to feature defn, and capture mapping.                 */
/* -------------------------------------------------------------------- */
    poFeatureDefn->AddFieldDefn( &oField );
    
    panFieldMapping = (int *) 
        CPLRealloc( panFieldMapping, 
                    sizeof(int) * poFeatureDefn->GetFieldCount() );
    panFieldMapping[poFeatureDefn->GetFieldCount()-1] = -1;

    return TRUE;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRDODSLayer::ResetReading()

{
    iNextShapeId = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRDODSLayer::GetNextFeature()

{
    return GetFeature( iNextShapeId++ );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDODSLayer::TestCapability( const char * pszCap )

{
    return FALSE;
}

/************************************************************************/
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRDODSLayer::GetSpatialRef()

{
    return poSRS;
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRDODSLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( poFilterGeom != NULL )
    {
        delete poFilterGeom;
        poFilterGeom = NULL;
    }

    if( poGeomIn != NULL )
        poFilterGeom = poGeomIn->clone();

    ResetReading();
}

/************************************************************************/
/*                           ProvideDataDDS()                           */
/************************************************************************/

int OGRDODSLayer::ProvideDataDDS()

{
    if( bDataLoaded )
        return poTargetVar != NULL;

    bDataLoaded = TRUE;
    try
    {
        poConnection = new AISConnect( poDS->oBaseURL );
        CPLDebug( "DODS", "request_data(%s,%s)",
                  poDS->oBaseURL.c_str(),
                  (poDS->oProjection + poDS->oConstraints).c_str() );

        // We may need to use custom constraints here. 
        poConnection->request_data( oDataDDS, 
                                    poDS->oProjection + poDS->oConstraints );
    }
    catch (Error &e)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "DataDDS request failed:\n%s", 
                  e.get_error_message().c_str() );
        return FALSE;
    }

    poTargetVar = oDataDDS.var( pszTarget );

    return poTargetVar != NULL;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRDODSLayer::GetExtent(OGREnvelope *psExtent, int bForce)

{
    OGRErr eErr;

    if( bKnowExtent )
    {
        *psExtent = this->sExtent;
        return OGRERR_NONE;
    }

    if( !bForce )
        return OGRERR_FAILURE;

    eErr = OGRLayer::GetExtent( &sExtent, bForce );
    if( eErr == OGRERR_NONE )
    {
        bKnowExtent = TRUE;
        *psExtent = sExtent;
    }

    return eErr;
}
