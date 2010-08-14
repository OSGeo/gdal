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
#include "ogr_featurestyle.h"

CPL_CVSID("$Id$");

#ifndef PI
#define PI  3.14159265358979323846
#endif 

/************************************************************************/
/*                         OGRDXFWriterLayer()                          */
/************************************************************************/

OGRDXFWriterLayer::OGRDXFWriterLayer( OGRDXFWriterDS *poDS, FILE *fp )

{
    this->fp = fp;
    this->poDS = poDS;

    nNextFID = 80;

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
    if( poFeatureDefn )
        poFeatureDefn->Release();
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
    char szLinePair[64];

    snprintf(szLinePair, sizeof(szLinePair), "%3d\n%.15g\n", nCode, dfValue );
    char* pszComma = strchr(szLinePair, ',');
    if (pszComma)
        *pszComma = '.';
    size_t nLen = strlen(szLinePair);

    return VSIFWriteL( szLinePair, 
                       1, nLen, fp ) == nLen;
}

/************************************************************************/
/*                             WriteCore()                              */
/*                                                                      */
/*      Write core fields common to all sorts of elements.              */
/************************************************************************/

OGRErr OGRDXFWriterLayer::WriteCore( OGRFeature *poFeature )

{
/* -------------------------------------------------------------------- */
/*      Write out an entity id.  I'm not sure why this is critical,     */
/*      but it seems that VoloView will just quietly fail to open       */
/*      dxf files without entity ids set on most/all entities.          */
/*      Also, for reasons I don't understand these ids seem to have     */
/*      to start somewhere around 0x50 hex (80 decimal).                */
/* -------------------------------------------------------------------- */
    char szEntityID[16];

    sprintf( szEntityID, "%X", nNextFID++ );
    WriteValue( 5, szEntityID );

/* -------------------------------------------------------------------- */
/*      For now we assign everything to the default layer - layer       */
/*      "0".                                                            */
/* -------------------------------------------------------------------- */
    const char *pszLayer = poFeature->GetFieldAsString( "Layer" );
    if( pszLayer == NULL )
    {
        WriteValue( 8, "0" );
    }
    else
    {
        const char *pszExists = 
            poDS->oHeaderDS.LookupLayerProperty( pszLayer, "Exists" );
        if( (pszExists == NULL || strlen(pszExists) == 0)
            && CSLFindString( poDS->papszLayersToCreate, pszLayer ) == -1 )
        {
            poDS->papszLayersToCreate = 
                CSLAddString( poDS->papszLayersToCreate, pszLayer );
        }

        WriteValue( 8, pszLayer );
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                             WritePOINT()                             */
/************************************************************************/

OGRErr OGRDXFWriterLayer::WritePOINT( OGRFeature *poFeature )

{
    WriteValue( 0, "POINT" );
    WriteCore( poFeature );
    WriteValue( 100, "AcDbEntity" );
    WriteValue( 100, "AcDbPoint" );

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
/*                             WriteTEXT()                              */
/************************************************************************/

OGRErr OGRDXFWriterLayer::WriteTEXT( OGRFeature *poFeature )

{
    WriteValue( 0, "MTEXT" );
    WriteCore( poFeature );
    WriteValue( 100, "AcDbEntity" );
    WriteValue( 100, "AcDbMText" );

/* -------------------------------------------------------------------- */
/*      Do we have styling information?                                 */
/* -------------------------------------------------------------------- */
    OGRStyleTool *poTool = NULL;
    OGRStyleMgr oSM;

    if( poFeature->GetStyleString() != NULL )
    {
        oSM.InitFromFeature( poFeature );

        if( oSM.GetPartCount() > 0 )
            poTool = oSM.GetPart(0);
    }

/* ==================================================================== */
/*      Process the LABEL tool.                                         */
/* ==================================================================== */
    if( poTool && poTool->GetType() == OGRSTCLabel )
    {
        OGRStyleLabel *poLabel = (OGRStyleLabel *) poTool;
        GBool  bDefault;

/* -------------------------------------------------------------------- */
/*      Color                                                           */
/* -------------------------------------------------------------------- */
        if( poLabel->ForeColor(bDefault) != NULL && !bDefault )
            WriteValue( 62, ColorStringToDXFColor( 
                            poLabel->ForeColor(bDefault) ) );

/* -------------------------------------------------------------------- */
/*      Angle                                                           */
/* -------------------------------------------------------------------- */
        double dfAngle = poLabel->Angle(bDefault);

        if( !bDefault )
            WriteValue( 50, dfAngle * (PI/180.0) );

/* -------------------------------------------------------------------- */
/*      Height - We need to fetch this in georeferenced units - I'm     */
/*      doubt the default translation mechanism will be much good.      */
/* -------------------------------------------------------------------- */
        poTool->SetUnit( OGRSTUGround );
        double dfHeight = poLabel->Size(bDefault);

        if( !bDefault )
            WriteValue( 40, dfHeight );

/* -------------------------------------------------------------------- */
/*      Anchor / Attachment Point                                       */
/* -------------------------------------------------------------------- */
        int nAnchor = poLabel->Anchor(bDefault);
        
        if( !bDefault )
        {
            const static int anAnchorMap[] = 
                { -1, 7, 8, 9, 4, 5, 6, 1, 2, 3, 7, 8, 9 };

            if( nAnchor > 0 && nAnchor < 13 )
                WriteValue( 71, anAnchorMap[nAnchor] );
        }

/* -------------------------------------------------------------------- */
/*      Text - split into distinct lines.                               */
/* -------------------------------------------------------------------- */
        const char *pszText = poLabel->TextString( bDefault );

        if( pszText != NULL && !bDefault )
        {
            int iLine;
            char **papszLines = CSLTokenizeStringComplex(
                pszText, "\n", FALSE, TRUE );

            for( iLine = 0; 
                 papszLines != NULL && papszLines[iLine] != NULL;
                 iLine++ )
            {
                if( iLine == 0 )
                    WriteValue( 1, papszLines[iLine] );
                else
                    WriteValue( 3, papszLines[iLine] );
            }
            CSLDestroy( papszLines );
        }
    }

    delete poTool;

/* -------------------------------------------------------------------- */
/*      Write the location.                                             */
/* -------------------------------------------------------------------- */
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
/*                           WritePOLYLINE()                            */
/************************************************************************/

OGRErr OGRDXFWriterLayer::WritePOLYLINE( OGRFeature *poFeature,
                                         OGRGeometry *poGeom )

{
/* -------------------------------------------------------------------- */
/*      For now we handle multilinestrings by writing a series of       */
/*      entities.                                                       */
/* -------------------------------------------------------------------- */
    if( poGeom == NULL )
        poGeom = poFeature->GetGeometryRef();

    if ( poGeom->IsEmpty() )
    {
        return OGRERR_NONE;
    }
            
    if( wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon 
        || wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString )
    {
        OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeom;
        int iGeom;
        OGRErr eErr = OGRERR_NONE;

        for( iGeom = 0; 
             eErr == OGRERR_NONE && iGeom < poGC->getNumGeometries(); 
             iGeom++ )
        {
            eErr = WritePOLYLINE( poFeature, poGC->getGeometryRef( iGeom ) );
        }

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Polygons are written with on entity per ring.                   */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(poGeom->getGeometryType()) == wkbPolygon )
    {
        OGRPolygon *poPoly = (OGRPolygon *) poGeom;
        int iGeom;
        OGRErr eErr;

        eErr = WritePOLYLINE( poFeature, poPoly->getExteriorRing() );
        for( iGeom = 0; 
             eErr == OGRERR_NONE && iGeom < poPoly->getNumInteriorRings(); 
             iGeom++ )
        {
            eErr = WritePOLYLINE( poFeature, poPoly->getInteriorRing(iGeom) );
        }

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Do we now have a geometry we can work with?                     */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(poGeom->getGeometryType()) != wkbLineString )
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

    OGRLineString *poLS = (OGRLineString *) poGeom;

/* -------------------------------------------------------------------- */
/*      Write as a lightweight polygon.                                 */
/* -------------------------------------------------------------------- */
    WriteValue( 0, "LWPOLYLINE" );
    WriteCore( poFeature );
    WriteValue( 100, "AcDbEntity" );
    WriteValue( 100, "AcDbPolyline" );
    if( EQUAL( poGeom->getGeometryName(), "LINEARRING" ) )
        WriteValue( 70, 1 );
    else
        WriteValue( 70, 0 );
    WriteValue( 90, poLS->getNumPoints() );

/* -------------------------------------------------------------------- */
/*      Do we have styling information?                                 */
/* -------------------------------------------------------------------- */
    OGRStyleTool *poTool = NULL;
    OGRStyleMgr oSM;

    if( poFeature->GetStyleString() != NULL )
    {
        oSM.InitFromFeature( poFeature );

        if( oSM.GetPartCount() > 0 )
            poTool = oSM.GetPart(0);
    }

/* -------------------------------------------------------------------- */
/*      Handle a PEN tool to control drawing color and width.           */
/*      Perhaps one day also dottedness, etc.                           */
/* -------------------------------------------------------------------- */
    if( poTool && poTool->GetType() == OGRSTCPen )
    {
        OGRStylePen *poPen = (OGRStylePen *) poTool;
        GBool  bDefault;

        if( poPen->Color(bDefault) != NULL && !bDefault )
            WriteValue( 62, ColorStringToDXFColor( poPen->Color(bDefault) ) );
        
        double dfWidthInMM = poPen->Width(bDefault);

        if( !bDefault )
            WriteValue( 370, (int) floor(dfWidthInMM * 100 + 0.5) );
    }

    delete poTool;

/* -------------------------------------------------------------------- */
/*      Write the vertices                                              */
/* -------------------------------------------------------------------- */
    int iVert;

    for( iVert = 0; iVert < poLS->getNumPoints(); iVert++ )
    {
        WriteValue( 10, poLS->getX(iVert) );
        if( !WriteValue( 20, poLS->getY(iVert) ) ) 
            return OGRERR_FAILURE;

        if( poLS->getGeometryType() == wkbLineString25D )
        {
            if( !WriteValue( 38, poLS->getZ(iVert) ) )
                return OGRERR_FAILURE;
        }
    }
    
    return OGRERR_NONE;

#ifdef notdef
/* -------------------------------------------------------------------- */
/*      Alternate unmaintained implementation as a polyline entity.     */
/* -------------------------------------------------------------------- */
    WriteValue( 0, "POLYLINE" );
    WriteCore( poFeature );
    WriteValue( 100, "AcDbEntity" );
    WriteValue( 100, "AcDbPolyline" );
    if( EQUAL( poGeom->getGeometryName(), "LINEARRING" ) )
        WriteValue( 70, 1 );
    else
        WriteValue( 70, 0 );
    WriteValue( 66, "1" );

    int iVert;

    for( iVert = 0; iVert < poLS->getNumPoints(); iVert++ )
    {
        WriteValue( 0, "VERTEX" );
        WriteValue( 8, "0" );
        WriteValue( 10, poLS->getX(iVert) );
        if( !WriteValue( 20, poLS->getY(iVert) ) ) 
            return OGRERR_FAILURE;

        if( poLS->getGeometryType() == wkbLineString25D )
        {
            if( !WriteValue( 30, poLS->getZ(iVert) ) )
                return OGRERR_FAILURE;
        }
    }

    WriteValue( 0, "SEQEND" );
    WriteValue( 8, "0" );
    
    return OGRERR_NONE;
#endif
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
    {
        if( poFeature->GetStyleString() != NULL
            && EQUALN(poFeature->GetStyleString(),"LABEL",5) )
            return WriteTEXT( poFeature );
        else
            return WritePOINT( poFeature );
    }
    else if( eGType == wkbLineString 
             || eGType == wkbMultiLineString 
             || eGType == wkbPolygon 
             || eGType == wkbMultiPolygon )
        return WritePOLYLINE( poFeature );
    else 
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No known way to write feature with geometry '%s'.",
                  OGRGeometryTypeToName(eGType) );
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                       ColorStringToDXFColor()                        */
/************************************************************************/

int OGRDXFWriterLayer::ColorStringToDXFColor( const char *pszRGB )

{
/* -------------------------------------------------------------------- */
/*      Parse the RGB string.                                           */
/* -------------------------------------------------------------------- */
    if( pszRGB == NULL )
        return -1;

    int nRed, nGreen, nBlue, nTransparency = 255;

    int nCount  = sscanf(pszRGB,"#%2x%2x%2x%2x",&nRed,&nGreen,&nBlue, 
                         &nTransparency);
   
    if (nCount < 3 )
        return -1;

/* -------------------------------------------------------------------- */
/*      Find near color in DXF palette.                                 */
/* -------------------------------------------------------------------- */
    const unsigned char *pabyDXFColors = OGRDXFDriver::GetDXFColorTable();
    int i;
    int nMinDist = 768;
    int nBestColor = -1;

    for( i = 1; i < 256; i++ )
    {
        int nDist = ABS(nRed - pabyDXFColors[i*3+0])
            + ABS(nGreen - pabyDXFColors[i*3+1])
            + ABS(nBlue  - pabyDXFColors[i*3+2]);

        if( nDist < nMinDist )
        {
            nBestColor = i;
            nMinDist = nDist;
        }
    }
    
    return nBestColor;
}
