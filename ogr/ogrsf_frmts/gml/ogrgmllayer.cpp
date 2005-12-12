/******************************************************************************
 * $Id$
 *
 * Project:  OGR
 * Purpose:  Implements OGRGMLLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.20  2005/12/12 21:33:07  fwarmerdam
 * remove ogr: prefix from field names: bug 1016
 *
 * Revision 1.19  2005/09/21 00:59:36  fwarmerdam
 * fixup OGRFeatureDefn and OGRSpatialReference refcount handling
 *
 * Revision 1.18  2005/06/30 02:16:41  fwarmerdam
 * more efforts to produce valid GML
 *
 * Revision 1.17  2005/05/04 19:34:07  fwarmerdam
 * delete reader in datasource destructor, not layer
 *
 * Revision 1.16  2005/02/22 12:56:28  fwarmerdam
 * use OGRLayer base spatial filter support
 *
 * Revision 1.15  2005/02/02 20:54:27  fwarmerdam
 * track m_nFeaturesRead
 *
 * Revision 1.14  2004/10/05 19:56:51  fwarmerdam
 * If a reader has been instantiated, delete it in ~OGRGMLLayer().
 *
 * Revision 1.13  2004/07/20 17:32:03  warmerda
 * restructure GetNextFeature() to apply spatial and attribute queries
 *
 * Revision 1.12  2004/01/29 15:30:40  warmerda
 * cleanup layer and field names
 *
 * Revision 1.11  2004/01/27 21:22:04  warmerda
 * Escape strings written to GML file.
 *
 * Revision 1.10  2003/12/02 18:43:03  warmerda
 * Removed unused variable.
 *
 * Revision 1.9  2003/05/21 03:48:35  warmerda
 * Expand tabs
 *
 * Revision 1.8  2003/03/07 14:53:21  warmerda
 * implement preliminary BXFS write support
 *
 * Revision 1.7  2003/03/06 20:30:28  warmerda
 * use GML/OGR geometry translations from ogr_geometry.h now
 *
 * Revision 1.6  2003/01/17 20:39:40  warmerda
 * added bounding rectangle support
 *
 * Revision 1.5  2003/01/10 16:23:11  warmerda
 * assign FIDs if not provided in CreateFeature()
 *
 * Revision 1.4  2002/04/24 19:32:07  warmerda
 * Make a copy of the passed OGRSpatialReference in the constructor.
 *
 * Revision 1.3  2002/03/06 20:08:24  warmerda
 * add accelerated GetFeatureCount and GetExtent
 *
 * Revision 1.2  2002/01/25 21:00:31  warmerda
 * fix some small bugs found by MS VC++
 *
 * Revision 1.1  2002/01/25 20:37:02  warmerda
 * New
 *
 */

#include "ogr_gml.h"
#include "cpl_conv.h"
#include "cpl_port.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRGMLLayer()                              */
/************************************************************************/

OGRGMLLayer::OGRGMLLayer( const char * pszName,
                          OGRSpatialReference *poSRSIn, int bWriterIn,
                          OGRwkbGeometryType eReqType,
                          OGRGMLDataSource *poDSIn )

{
    if( poSRSIn == NULL )
        poSRS = NULL;
    else
        poSRS = poSRSIn->Clone();
    
    iNextGMLId = 0;
    nTotalGMLCount = -1;
    
    poDS = poDSIn;

    if ( EQUALN(pszName, "ogr:", 4) )
      poFeatureDefn = new OGRFeatureDefn( pszName+4 );
    else
      poFeatureDefn = new OGRFeatureDefn( pszName );
    poFeatureDefn->Reference();
    poFeatureDefn->SetGeomType( eReqType );

    bWriter = bWriterIn;

/* -------------------------------------------------------------------- */
/*      Reader's should get the corresponding GMLFeatureClass and       */
/*      cache it.                                                       */
/* -------------------------------------------------------------------- */
    if( !bWriter )
        poFClass = poDS->GetReader()->GetClass( pszName );
    else
        poFClass = NULL;
}

/************************************************************************/
/*                           ~OGRGMLLayer()                           */
/************************************************************************/

OGRGMLLayer::~OGRGMLLayer()

{
    if( poFeatureDefn )
        poFeatureDefn->Release();

    if( poSRS != NULL )
        poSRS->Release();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRGMLLayer::ResetReading()

{
    iNextGMLId = 0;
    poDS->GetReader()->ResetReading();
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRGMLLayer::GetNextFeature()

{
    GMLFeature  *poGMLFeature = NULL;
    OGRGeometry *poGeom = NULL;

    if( iNextGMLId == 0 )
        ResetReading();

/* ==================================================================== */
/*      Loop till we find and translate a feature meeting all our       */
/*      requirements.                                                   */
/* ==================================================================== */
    while( TRUE )
    {
/* -------------------------------------------------------------------- */
/*      Cleanup last feature, and get a new raw gml feature.            */
/* -------------------------------------------------------------------- */
        if( poGMLFeature != NULL )
            delete poGMLFeature;

        if( poGeom != NULL )
            delete poGeom;

        poGMLFeature = poDS->GetReader()->NextFeature();
        if( poGMLFeature == NULL )
            return NULL;

/* -------------------------------------------------------------------- */
/*      Is it of the proper feature class?                              */
/* -------------------------------------------------------------------- */

        // We count reading low level GML features as a feature read for
        // work checking purposes, though at least we didn't necessary
        // have to turn it into an OGRFeature.
        m_nFeaturesRead++;

        if( poGMLFeature->GetClass() != poFClass )
            continue;

        iNextGMLId++;

/* -------------------------------------------------------------------- */
/*      Does it satisfy the spatial query, if there is one?             */
/* -------------------------------------------------------------------- */

        if( poGMLFeature->GetGeometry() != NULL )
        {
            poGeom = OGRGeometryFactory::createFromGML( poGMLFeature->GetGeometry() );
            // We assume the createFromGML() function would have already
            // reported the error. 
            if( poGeom == NULL )
            {
                delete poGMLFeature;
                return NULL;
            }
            
            if( m_poFilterGeom != NULL && !FilterGeometry( poGeom ) )
                continue;
        }
        
/* -------------------------------------------------------------------- */
/*      Convert the whole feature into an OGRFeature.                   */
/* -------------------------------------------------------------------- */
        int iField;
        OGRFeature *poOGRFeature = new OGRFeature( GetLayerDefn() );

        poOGRFeature->SetFID( iNextGMLId );

        for( iField = 0; iField < poFClass->GetPropertyCount(); iField++ )
        {
            const char *pszProperty = poGMLFeature->GetProperty( iField );
            
            if( pszProperty != NULL )
                poOGRFeature->SetField( iField, pszProperty );
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
        delete poGMLFeature;

        poOGRFeature->SetGeometryDirectly( poGeom );

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
        return poFClass->GetFeatureCount();
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRGMLLayer::GetExtent(OGREnvelope *psExtent, int bForce )

{
    double dfXMin, dfXMax, dfYMin, dfYMax;

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
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRGMLLayer::CreateFeature( OGRFeature *poFeature )

{
    FILE        *fp = poDS->GetOutputFP();

    if( !bWriter )
        return OGRERR_FAILURE;

    VSIFPrintf( fp, "  <gml:featureMember>\n" );

    if( poFeature->GetFID() == OGRNullFID )
        poFeature->SetFID( iNextGMLId++ );

    VSIFPrintf( fp, "    <ogr:%s fid=\"F%d\">\n", 
                poFeatureDefn->GetName(),
                poFeature->GetFID() );

    // Write out Geometry - for now it isn't indented properly.
    if( poFeature->GetGeometryRef() != NULL )
    {
        char    *pszGeometry;
        OGREnvelope sGeomBounds;

        pszGeometry = poFeature->GetGeometryRef()->exportToGML();
        VSIFPrintf( fp, "      <ogr:geometryProperty>%s</ogr:geometryProperty>\n",
                    pszGeometry );
        CPLFree( pszGeometry );

        poFeature->GetGeometryRef()->getEnvelope( &sGeomBounds );
        poDS->GrowExtents( &sGeomBounds );
    }

    // Write all "set" fields. 
    for( int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++ )
    {
        
        OGRFieldDefn *poField = poFeatureDefn->GetFieldDefn( iField );

        if( poFeature->IsFieldSet( iField ) )
        {
            const char *pszRaw = poFeature->GetFieldAsString( iField );

            while( *pszRaw == ' ' )
                pszRaw++;

            char *pszEscaped = CPLEscapeString( pszRaw, -1, CPLES_XML );

            VSIFPrintf( fp, "      <ogr:%s>%s</ogr:%s>\n", 
                        poField->GetNameRef(), pszEscaped, 
                        poField->GetNameRef() );
            CPLFree( pszEscaped );
        }
    }

    VSIFPrintf( fp, "    </ogr:%s>\n", poFeatureDefn->GetName() );
    VSIFPrintf( fp, "  </gml:featureMember>\n" );

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
/*                           GetSpatialRef()                            */
/************************************************************************/

OGRSpatialReference *OGRGMLLayer::GetSpatialRef()

{
    return poSRS;
}

