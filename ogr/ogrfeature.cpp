/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  The OGRFeature class implementation.
 * Author:   Frank Warmerdam, warmerda@home.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.1  1999/06/11 19:21:02  warmerda
 * New
 *
 */

#include "ogr_feature.h"
#include "ogr_p.h"

/************************************************************************/
/*                             OGRFeature()                             */
/************************************************************************/

OGRFeature::OGRFeature( OGRFeatureDefn * poDefnIn )

{
    poDefnIn->Reference();
    poDefn = poDefnIn;

    poGeometry = NULL;

    // we should likely be initializing from the defaults, but this will
    // usually be a waste. 
    pauFields = (OGRField *) CPLCalloc( poDefn->GetFieldCount(),
                                        sizeof(OGRField) );
}

/************************************************************************/
/*                            ~OGRFeature()                             */
/************************************************************************/

OGRFeature::~OGRFeature()

{
    poDefn->Dereference();

    if( poGeometry != NULL )
        delete poGeometry;

    for( int i = 0; i < poDefn->GetFieldCount(); i++ )
    {
        OGRFieldDefn	*poFDefn = poDefn->GetFieldDefn(i);

        switch( poFDefn->GetType() )
        {
          case OFTString:
            if( pauFields[i].String != NULL )
                CPLFree( pauFields[i].String );
            break;

          default:
            // should add support for list types and wide string.
            break;
        }
    }
    
    CPLFree( pauFields );
}

/************************************************************************/
/*                        SetGeometryDirectly()                         */
/************************************************************************/

OGRErr OGRFeature::SetGeometryDirectly( OGRGeometry * poGeomIn )

{
    if( poGeometry != NULL )
        delete poGeometry;

    poGeometry = poGeomIn;

    // I should be verifying that the geometry matches the defn's type.
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                            SetGeometry()                             */
/************************************************************************/

OGRErr OGRFeature::SetGeometry( OGRGeometry * poGeomIn )

{
    if( poGeometry != NULL )
        delete poGeometry;

    poGeometry = poGeomIn->clone();

    // I should be verifying that the geometry matches the defn's type.
    
    return OGRERR_NONE;
}

/************************************************************************/
/*                               Clone()                                */
/************************************************************************/

OGRFeature *OGRFeature::Clone()

{
    OGRFeature	*poNew = new OGRFeature( poDefn );

    poNew->SetGeometry( poGeometry );

    for( int i = 0; i < poDefn->GetFieldCount(); i++ )
    {
        poNew->SetField( i, pauFields + i );
    }

    return poNew;
}

/************************************************************************/
/*                         GetFieldAsInteger()                          */
/************************************************************************/

int OGRFeature::GetFieldAsInteger( int iField )

{
    OGRFieldDefn	*poFDefn = poDefn->GetFieldDefn( iField );
    
    CPLAssert( poFDefn != NULL );
    
    if( poFDefn->GetType() == OFTInteger )
        return pauFields[iField].Integer;
    else if( poFDefn->GetType() == OFTReal )
        return (int) pauFields[iField].Real;
    else if( poFDefn->GetType() == OFTString )
    {
        if( pauFields[iField].String == NULL )
            return 0;
        else
            return atoi(pauFields[iField].String);
    }
    else
        return 0;
}

/************************************************************************/
/*                          GetFieldAsDouble()                          */
/************************************************************************/

double OGRFeature::GetFieldAsDouble( int iField )

{
    OGRFieldDefn	*poFDefn = poDefn->GetFieldDefn( iField );
    
    CPLAssert( poFDefn != NULL );
    
    if( poFDefn->GetType() == OFTReal )
        return pauFields[iField].Real;
    else if( poFDefn->GetType() == OFTInteger )
        return pauFields[iField].Integer;
    else if( poFDefn->GetType() == OFTString )
    {
        if( pauFields[iField].String == NULL )
            return 0;
        else
            return atof(pauFields[iField].String);
    }
    else
        return 0.0;
}

/************************************************************************/
/*                          GetFieldAsString()                          */
/************************************************************************/

const char *OGRFeature::GetFieldAsString( int iField )

{
    OGRFieldDefn	*poFDefn = poDefn->GetFieldDefn( iField );
    static char		szTempBuffer[64];

    CPLAssert( poFDefn != NULL );
    
    if( poFDefn->GetType() == OFTString )
    {
        if( pauFields[iField].String == NULL )
            return "";
        else
            return pauFields[iField].String;
    }
    else if( poFDefn->GetType() == OFTInteger )
    {
        sprintf( szTempBuffer, "%d", pauFields[iField].Integer );
        return szTempBuffer;
    }
    else if( poFDefn->GetType() == OFTReal )
    {
        sprintf( szTempBuffer, "%g", pauFields[iField].Real );
        return szTempBuffer;
    }
    else
        return "";
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

void OGRFeature::SetField( int iField, int nValue )

{
    OGRFieldDefn	*poFDefn = poDefn->GetFieldDefn( iField );

    CPLAssert( poFDefn != NULL );
    
    if( poFDefn->GetType() == OFTInteger )
    {
        pauFields[iField].Integer = nValue;
    }
    else if( poFDefn->GetType() == OFTReal )
    {
        pauFields[iField].Real = nValue;
    }
    else if( poFDefn->GetType() == OFTString )
    {
        char	szTempBuffer[64];

        sprintf( szTempBuffer, "%d", nValue );

        CPLFree( pauFields[iField].String );
        pauFields[iField].String = CPLStrdup( szTempBuffer );
    }
    else
        /* do nothing for other field types */;
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

void OGRFeature::SetField( int iField, double dfValue )

{
    OGRFieldDefn	*poFDefn = poDefn->GetFieldDefn( iField );

    CPLAssert( poFDefn != NULL );
    
    if( poFDefn->GetType() == OFTReal )
    {
        pauFields[iField].Real = dfValue;
    }
    else if( poFDefn->GetType() == OFTInteger )
    {
        pauFields[iField].Integer = (int) dfValue;
    }
    else if( poFDefn->GetType() == OFTString )
    {
        char	szTempBuffer[128];

        sprintf( szTempBuffer, "%g", dfValue );

        CPLFree( pauFields[iField].String );
        pauFields[iField].String = CPLStrdup( szTempBuffer );
    }
    else
        /* do nothing for other field types */;
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

void OGRFeature::SetField( int iField, const char * pszValue )

{
    OGRFieldDefn	*poFDefn = poDefn->GetFieldDefn( iField );

    CPLAssert( poFDefn != NULL );
    
    if( poFDefn->GetType() == OFTString )
    {
        CPLFree( pauFields[iField].String );
        pauFields[iField].String = CPLStrdup( pszValue );
    }
    else if( poFDefn->GetType() == OFTInteger )
    {
        pauFields[iField].Integer = atoi(pszValue);
    }
    else if( poFDefn->GetType() == OFTReal )
    {
        pauFields[iField].Real = atof(pszValue);
    }
    else
        /* do nothing for other field types */;
}

/************************************************************************/
/*                              SetField()                              */
/************************************************************************/

void OGRFeature::SetField( int iField, OGRField * puValue )

{
    OGRFieldDefn	*poFDefn = poDefn->GetFieldDefn( iField );

    CPLAssert( poFDefn != NULL );
    
    if( poFDefn->GetType() == OFTInteger )
    {
        pauFields[iField].Integer = puValue->Integer;
    }
    else if( poFDefn->GetType() == OFTReal )
    {
        pauFields[iField].Real = puValue->Real;
    }
    else if( poFDefn->GetType() == OFTString )
    {
        CPLFree( pauFields[iField].String );
        pauFields[iField].String = CPLStrdup( puValue->String );
    }
    else
        /* do nothing for other field types */;
}

/************************************************************************/
/*                            DumpReadable()                            */
/************************************************************************/

void OGRFeature::DumpReadable( FILE * fpOut )

{
    fprintf( fpOut, "OGRFeature(%s)\n", poDefn->GetName() );
    for( int iField = 0; iField < GetFieldCount(); iField++ )
    {
        OGRFieldDefn	*poFDefn = poDefn->GetFieldDefn(iField);
        
        fprintf( fpOut, "  %s (%s) = %s\n",
                 poFDefn->GetNameRef(),
                 OGRFieldDefn::GetFieldTypeName(poFDefn->GetType()),
                 GetFieldAsString( iField ) );
    }
    
    if( poGeometry != NULL )
        poGeometry->dumpReadable( fpOut, "  " );

    fprintf( fpOut, "\n" );
}
