/******************************************************************************
 * $Id$
 *
 * Project:  DXF Translator
 * Purpose:  Implements OGRDXFWriterLayer - the OGRLayer class used for
 *           writing a DXF file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_dxf.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                         OGRDXFWriterLayer()                          */
/************************************************************************/

OGRDXFWriterLayer::OGRDXFWriterLayer( FILE *fp )

{
    this->fp = fp;

    poFeatureDefn = new OGRFeatureDefn( "entities" );
    poFeatureDefn->Reference();

    OGRFieldDefn  oLayerField( "Layer", OFTString );
    poFeatureDefn->AddFieldDefn( &oLayerField );

    OGRFieldDefn  oClassField( "SubClasses", OFTString );
    poFeatureDefn->AddFieldDefn( &oClassField );

    OGRFieldDefn  oExtendedField( "ExtendedEntity", OFTString );
    poFeatureDefn->AddFieldDefn( &oExtendedField );

    OGRFieldDefn  oLinetypeField( "Linetype", OFTString );
    poFeatureDefn->AddFieldDefn( &oLinetypeField );

    OGRFieldDefn  oEntityHandleField( "EntityHandle", OFTString );
    poFeatureDefn->AddFieldDefn( &oEntityHandleField );

    OGRFieldDefn  oTextField( "Text", OFTString );
    poFeatureDefn->AddFieldDefn( &oTextField );
}

/************************************************************************/
/*                         ~OGRDXFWriterLayer()                         */
/************************************************************************/

OGRDXFWriterLayer::~OGRDXFWriterLayer()

{
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDXFWriterLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCSequentialWrite) )
        return TRUE;
    else 
        return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/*                                                                      */
/*      This is really a dummy as our fields are precreated.            */
/************************************************************************/

OGRErr OGRDXFWriterLayer::CreateField( OGRFieldDefn *poField,
                                       int bApproxOK )

{
    if( poFeatureDefn->GetFieldIndex(poField->GetNameRef()) >= 0
        && bApproxOK )
        return OGRERR_NONE;

    CPLError( CE_Failure, CPLE_AppDefined,
              "DXF layer does not support arbitrary field creation." );

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                             WriteValue()                             */
/************************************************************************/

int OGRDXFWriterLayer::WriteValue( int nCode, const char *pszValue )

{
    CPLString osLinePair;

    osLinePair.Printf( "%3d\n", nCode );

    if( strlen(pszValue) < 255 )
        osLinePair += pszValue;
    else
        osLinePair.append( pszValue, 255 );

    osLinePair += "\n";

    return VSIFWriteL( osLinePair.c_str(), 
                       1, osLinePair.size(), fp ) == osLinePair.size();
}

/************************************************************************/
/*                             WriteValue()                             */
/************************************************************************/

int OGRDXFWriterLayer::WriteValue( int nCode, int nValue )

{
    CPLString osLinePair;

    osLinePair.Printf( "%3d\n%d\n", nCode, nValue );

    return VSIFWriteL( osLinePair.c_str(), 
                       1, osLinePair.size(), fp ) == osLinePair.size();
}

/************************************************************************/
/*                             WriteValue()                             */
/************************************************************************/

int OGRDXFWriterLayer::WriteValue( int nCode, double dfValue )

{
    CPLString osLinePair;

    osLinePair.Printf( "%3d\n%.15g\n", nCode, dfValue );

    return VSIFWriteL( osLinePair.c_str(), 
                       1, osLinePair.size(), fp ) == osLinePair.size();
}

/************************************************************************/
/*                             WriteCore()                              */
/*                                                                      */
/*      Write core fields common to all sorts of elements.              */
/************************************************************************/

OGRErr OGRDXFWriterLayer::WriteCore( OGRFeature *poFeature )

{
    return OGRERR_NONE;
}

/************************************************************************/
/*                             WritePOINT()                             */
/************************************************************************/

OGRErr OGRDXFWriterLayer::WritePOINT( OGRFeature *poFeature )

{
    WriteValue( 0, "POINT" );
    WriteCore( poFeature );

    OGRPoint *poPoint = (OGRPoint *) poFeature->GetGeometryRef();

    WriteValue( 10, poPoint->getX() );
    if( !WriteValue( 20, poPoint->getY() ) ) 
        return OGRERR_FAILURE;

    if( poPoint->getGeometryType() == wkbPoint25D )
    {
        if( !WriteValue( 30, poPoint->getZ() ) )
            return OGRERR_FAILURE;
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                          WriteLWPOLYLINE()                           */
/************************************************************************/

OGRErr OGRDXFWriterLayer::WriteLWPOLYLINE( OGRFeature *poFeature )

{
    WriteValue( 0, "LWPOLYLINE" );
    WriteCore( poFeature );

    OGRLineString *poLS = (OGRLineString *) poFeature->GetGeometryRef();

    int iVert;

    for( iVert = 0; iVert < poLS->getNumPoints(); iVert++ )
    {
        WriteValue( 10, poLS->getX(iVert) );
        if( !WriteValue( 20, poLS->getY(iVert) ) ) 
            return OGRERR_FAILURE;

#ifdef notdef
        if( poLS->getGeometryType() == wkbLineString25D )
        {
            if( !WriteValue( 30, poLS->getZ(iVert) ) )
                return OGRERR_FAILURE;
        }
#endif
    }
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateFeature()                            */
/************************************************************************/

OGRErr OGRDXFWriterLayer::CreateFeature( OGRFeature *poFeature )

{
    OGRGeometry *poGeom = poFeature->GetGeometryRef();
    OGRwkbGeometryType eGType = wkbNone;
    
    if( poGeom != NULL )
        eGType = wkbFlatten(poGeom->getGeometryType());

    if( eGType == wkbPoint )
        return WritePOINT( poFeature );
    else if( eGType == wkbLineString )
        return WriteLWPOLYLINE( poFeature );
    else 
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No known way to write feature with geometry '%s'.",
                  OGRGeometryTypeToName(eGType) );
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

