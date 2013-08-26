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

OGRDXFWriterLayer::OGRDXFWriterLayer( OGRDXFWriterDS *poDS, VSILFILE *fp )

{
    this->fp = fp;
    this->poDS = poDS;

    nNextAutoID = 1;
    bWriteHatch = CSLTestBoolean(CPLGetConfigOption("DXF_WRITE_HATCH", "YES"));

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

    OGRFieldDefn  oBlockField( "BlockName", OFTString );
    poFeatureDefn->AddFieldDefn( &oBlockField );
    
    OGRFieldDefn  oScaleField( "BlockScale", OFTRealList );
    poFeatureDefn->AddFieldDefn( &oScaleField );
    
    OGRFieldDefn  oBlockAngleField( "BlockAngle", OFTReal );
    poFeatureDefn->AddFieldDefn( &oBlockAngleField );
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
/*                              ResetFP()                               */
/*                                                                      */
/*      Redirect output.  Mostly used for writing block definitions.    */
/************************************************************************/

void OGRDXFWriterLayer::ResetFP( VSILFILE *fpNew )

{
    fp = fpNew;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDXFWriterLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;
    else if( EQUAL(pszCap,OLCSequentialWrite) )
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
              "DXF layer does not support arbitrary field creation, field '%s' not created.",
              poField->GetNameRef() );

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
    poFeature->SetFID( poDS->WriteEntityID(fp,poFeature->GetFID()) );

/* -------------------------------------------------------------------- */
/*      For now we assign everything to the default layer - layer       */
/*      "0" - if there is no layer property on the source features.     */
/* -------------------------------------------------------------------- */
    const char *pszLayer = poFeature->GetFieldAsString( "Layer" );
    if( pszLayer == NULL || strlen(pszLayer) == 0 )
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
/*                            WriteINSERT()                             */
/************************************************************************/

OGRErr OGRDXFWriterLayer::WriteINSERT( OGRFeature *poFeature )

{
    WriteValue( 0, "INSERT" );
    WriteCore( poFeature );
    WriteValue( 100, "AcDbEntity" );
    WriteValue( 100, "AcDbBlockReference" );
    WriteValue( 2, poFeature->GetFieldAsString("BlockName") );

    // Write style symbol color
    OGRStyleTool *poTool = NULL;
    OGRStyleMgr oSM;
    if( poFeature->GetStyleString() != NULL )
    {
        oSM.InitFromFeature( poFeature );

        if( oSM.GetPartCount() > 0 )
            poTool = oSM.GetPart(0);
    }
    if( poTool && poTool->GetType() == OGRSTCSymbol )
    {
        OGRStyleSymbol *poSymbol = (OGRStyleSymbol *) poTool;
        GBool  bDefault;

        if( poSymbol->Color(bDefault) != NULL && !bDefault )
            WriteValue( 62, ColorStringToDXFColor( poSymbol->Color(bDefault) ) );
    }
    delete poTool;
    
/* -------------------------------------------------------------------- */
/*      Write location.                                                 */
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
    
/* -------------------------------------------------------------------- */
/*      Write scaling.                                                  */
/* -------------------------------------------------------------------- */
    int nScaleCount;
    const double *padfScale = 
        poFeature->GetFieldAsDoubleList( "BlockScale", &nScaleCount );

    if( nScaleCount == 3 )
    {
        WriteValue( 41, padfScale[0] );
        WriteValue( 42, padfScale[1] );
        WriteValue( 43, padfScale[2] );
    }

/* -------------------------------------------------------------------- */
/*      Write rotation.                                                 */
/* -------------------------------------------------------------------- */
    double dfAngle = poFeature->GetFieldAsDouble( "BlockAngle" );

    if( dfAngle != 0.0 )
    {
        WriteValue( 50, dfAngle ); // degrees
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

    // Write style pen color
    OGRStyleTool *poTool = NULL;
    OGRStyleMgr oSM;
    if( poFeature->GetStyleString() != NULL )
    {
        oSM.InitFromFeature( poFeature );

        if( oSM.GetPartCount() > 0 )
            poTool = oSM.GetPart(0);
    }
    if( poTool && poTool->GetType() == OGRSTCPen )
    {
        OGRStylePen *poPen = (OGRStylePen *) poTool;
        GBool  bDefault;

        if( poPen->Color(bDefault) != NULL && !bDefault )
            WriteValue( 62, ColorStringToDXFColor( poPen->Color(bDefault) ) );
    }
    delete poTool;

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
/*                             TextEscape()                             */
/*                                                                      */
/*      Translate UTF8 to Win1252 and escape special characters like    */
/*      newline and space with DXF style escapes.  Note that            */
/*      non-win1252 unicode characters are translated using the         */
/*      unicode escape sequence.                                        */
/************************************************************************/

CPLString OGRDXFWriterLayer::TextEscape( const char *pszInput )

{
    CPLString osResult;
    wchar_t *panInput = CPLRecodeToWChar( pszInput, 
                                          CPL_ENC_UTF8, 
                                          CPL_ENC_UCS2 );
    int i;


    for( i = 0; panInput[i] != 0; i++ )
    {
        if( panInput[i] == '\n' )
            osResult += "\\P";
        else if( panInput[i] == ' ' )
            osResult += "\\~";
        else if( panInput[i] == '\\' )
            osResult += "\\\\";
        else if( panInput[i] > 255 )
        {
            CPLString osUnicode;
            osUnicode.Printf( "\\U+%04x", (int) panInput[i] );
            osResult += osUnicode;
        }
        else
            osResult += (char) panInput[i];
    }

    CPLFree(panInput);
    
    return osResult;
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

        // The DXF2000 reference says this is in radians, but in files
        // I see it seems to be in degrees. Perhaps this is version dependent?
        if( !bDefault )
            WriteValue( 50, dfAngle );

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
/*      Escape the text, and convert to ISO8859.                        */
/* -------------------------------------------------------------------- */
        const char *pszText = poLabel->TextString( bDefault );

        if( pszText != NULL && !bDefault )
        {
            CPLString osEscaped = TextEscape( pszText );
            WriteValue( 1, osEscaped );
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
/*                     PrepareLineTypeDefinition()                      */
/************************************************************************/
CPLString 
OGRDXFWriterLayer::PrepareLineTypeDefinition( OGRFeature *poFeature, 
                                              OGRStyleTool *poTool )

{
    CPLString osDef;
    OGRStylePen *poPen = (OGRStylePen *) poTool;
    GBool  bDefault;
    const char *pszPattern;
    
/* -------------------------------------------------------------------- */
/*      Fetch pattern.                                                  */
/* -------------------------------------------------------------------- */
    pszPattern = poPen->Pattern( bDefault );
    if( bDefault || strlen(pszPattern) == 0 ) 
        return "";

/* -------------------------------------------------------------------- */
/*      Split into pen up / pen down bits.                              */
/* -------------------------------------------------------------------- */
    char **papszTokens = CSLTokenizeString(pszPattern);
    int i;
    double dfTotalLength = 0;

    for( i = 0; papszTokens != NULL && papszTokens[i] != NULL; i++ )
    {
        const char *pszToken = papszTokens[i];
        const char *pszUnit;
        CPLString osAmount;
        CPLString osDXFEntry;

        // Split amount and unit.
        for( pszUnit = pszToken; 
             strchr( "0123456789.", *pszUnit) != NULL;
             pszUnit++ ) {}

        osAmount.assign(pszToken,(int) (pszUnit-pszToken));
        
        // If the unit is other than 'g' we really should be trying to 
        // do some type of transformation - but what to do?  Pretty hard.
        
        // 

        // Even entries are "pen down" represented as negative in DXF.
        if( i%2 == 0 )
            osDXFEntry.Printf( " 49\n-%s\n 74\n0\n", osAmount.c_str() );
        else
            osDXFEntry.Printf( " 49\n%s\n 74\n0\n", osAmount.c_str() );
        
        osDef += osDXFEntry;

        dfTotalLength += atof(osAmount);
    }

/* -------------------------------------------------------------------- */
/*      Prefix 73 and 40 items to the definition.                       */
/* -------------------------------------------------------------------- */
    CPLString osPrefix;
    
    osPrefix.Printf( " 73\n%d\n 40\n%.6g\n", 
                     CSLCount(papszTokens), 
                     dfTotalLength );
    osDef = osPrefix + osDef;

    CSLDestroy( papszTokens );

    return osDef;
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
/*      Write as a lightweight polygon,                                 */
/*       or as POLYLINE if the line contains different heights          */
/* -------------------------------------------------------------------- */
    int bHasDifferentZ = FALSE;
    if( poLS->getGeometryType() == wkbLineString25D )
    {
        double z0 = poLS->getZ(0);
        for( int iVert = 0; iVert < poLS->getNumPoints(); iVert++ )
        {
            if (z0 != poLS->getZ(iVert))
            {
                bHasDifferentZ = TRUE;
                break;
            }
        }
    }

    WriteValue( 0, bHasDifferentZ ? "POLYLINE" : "LWPOLYLINE" );
    WriteCore( poFeature );
    WriteValue( 100, "AcDbEntity" );
    if( bHasDifferentZ )
    {
        WriteValue( 100, "AcDb3dPolyline" );
        WriteValue( 10, 0.0 );
        WriteValue( 20, 0.0 );
        WriteValue( 30, 0.0 );
    }
    else
        WriteValue( 100, "AcDbPolyline" );
    if( EQUAL( poGeom->getGeometryName(), "LINEARRING" ) )
        WriteValue( 70, 1 + (bHasDifferentZ ? 8 : 0) );
    else
        WriteValue( 70, 0 + (bHasDifferentZ ? 8 : 0) );
    if( !bHasDifferentZ )
        WriteValue( 90, poLS->getNumPoints() );
    else
        WriteValue( 66, "1" );  // Vertex Flag

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

        // we want to fetch the width in ground units. 
        poPen->SetUnit( OGRSTUGround, 1.0 );
        double dfWidth = poPen->Width(bDefault);

        if( !bDefault )
            WriteValue( 370, (int) floor(dfWidth * 100 + 0.5) );
    }

/* -------------------------------------------------------------------- */
/*      Do we have a Linetype for the feature?                          */
/* -------------------------------------------------------------------- */
    CPLString osLineType = poFeature->GetFieldAsString( "Linetype" );

    if( osLineType.size() > 0 
        && (poDS->oHeaderDS.LookupLineType( osLineType ) != NULL 
            || oNewLineTypes.count(osLineType) > 0 ) )
    {
        // Already define -> just reference it.
        WriteValue( 6, osLineType );
    }
    else if( poTool != NULL && poTool->GetType() == OGRSTCPen )
    {
        CPLString osDefinition = PrepareLineTypeDefinition( poFeature, 
                                                            poTool );

        if( osDefinition != "" && osLineType == "" )
        {
            // Is this definition already created and named?
            std::map<CPLString,CPLString>::iterator it;

            for( it = oNewLineTypes.begin();
                 it != oNewLineTypes.end();
                 it++ )
            {
                if( (*it).second == osDefinition )
                {
                    osLineType = (*it).first;
                    break;
                }
            }

            // create an automatic name for it.
            if( osLineType == "" )
            {
                do 
                { 
                    osLineType.Printf( "AutoLineType-%d", nNextAutoID++ );
                }
                while( poDS->oHeaderDS.LookupLineType(osLineType) != NULL );
            }
        }

        // If it isn't already defined, add it now.
        if( osDefinition != "" && oNewLineTypes.count(osLineType) == 0 )
        {
            oNewLineTypes[osLineType] = osDefinition;
            WriteValue( 6, osLineType );
        }
    }

/* -------------------------------------------------------------------- */
/*      Write the vertices                                              */
/* -------------------------------------------------------------------- */

    if( !bHasDifferentZ && poLS->getGeometryType() == wkbLineString25D )
    {
     // if LWPOLYLINE with Z write it only once
        if( !WriteValue( 38, poLS->getZ(0) ) )
            return OGRERR_FAILURE;
    }

    int iVert;

    for( iVert = 0; iVert < poLS->getNumPoints(); iVert++ )
    {
        if( bHasDifferentZ ) 
        {
            WriteValue( 0, "VERTEX" );
            WriteValue( 100, "AcDbEntity" );
            WriteValue( 100, "AcDbVertex" );
            WriteValue( 100, "AcDb3dPolylineVertex" );
            WriteCore( poFeature );
        }
        WriteValue( 10, poLS->getX(iVert) );
        if( !WriteValue( 20, poLS->getY(iVert) ) ) 
            return OGRERR_FAILURE;

        if( bHasDifferentZ )
        {
            if( !WriteValue( 30 , poLS->getZ(iVert) ) )
                return OGRERR_FAILURE;
            WriteValue( 70, 32 );
        }
    }

    if( bHasDifferentZ )
    {
        WriteValue( 0, "SEQEND" );
        WriteCore( poFeature );
        WriteValue( 100, "AcDbEntity" );
    }
    
    delete poTool;

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
/*                             WriteHATCH()                             */
/************************************************************************/

OGRErr OGRDXFWriterLayer::WriteHATCH( OGRFeature *poFeature,
                                      OGRGeometry *poGeom )

{
/* -------------------------------------------------------------------- */
/*      For now we handle multipolygons by writing a series of          */
/*      entities.                                                       */
/* -------------------------------------------------------------------- */
    if( poGeom == NULL )
        poGeom = poFeature->GetGeometryRef();

    if ( poGeom->IsEmpty() )
    {
        return OGRERR_NONE;
    }
            
    if( wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon )
    {
        OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeom;
        int iGeom;
        OGRErr eErr = OGRERR_NONE;

        for( iGeom = 0; 
             eErr == OGRERR_NONE && iGeom < poGC->getNumGeometries(); 
             iGeom++ )
        {
            eErr = WriteHATCH( poFeature, poGC->getGeometryRef( iGeom ) );
        }

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Do we now have a geometry we can work with?                     */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(poGeom->getGeometryType()) != wkbPolygon )
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

/* -------------------------------------------------------------------- */
/*      Write as a hatch.                                               */
/* -------------------------------------------------------------------- */
    WriteValue( 0, "HATCH" );
    WriteCore( poFeature );
    WriteValue( 100, "AcDbEntity" );
    WriteValue( 100, "AcDbHatch" );

    WriteValue( 10, 0 ); // elevation point X. 0 for DXF
    WriteValue( 20, 0 ); // elevation point Y
    WriteValue( 30, 0 ); // elevation point Z
    WriteValue(210, 0 ); // extrusion direction X
    WriteValue(220, 0 ); // extrusion direction Y
    WriteValue(230,1.0); // extrusion direction Z

    WriteValue( 2, "SOLID" ); // fill pattern
    WriteValue( 70, 1 ); // solid fill
    WriteValue( 71, 0 ); // associativity 

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
    // Write style brush fore color
    if( poTool && poTool->GetType() == OGRSTCBrush )
    {
        OGRStyleBrush *poBrush = (OGRStyleBrush *) poTool;
        GBool  bDefault;

        if( poBrush->ForeColor(bDefault) != NULL && !bDefault )
            WriteValue( 62, ColorStringToDXFColor( poBrush->ForeColor(bDefault) ) );
    }
    delete poTool;

/* -------------------------------------------------------------------- */
/*      Handle a PEN tool to control drawing color and width.           */
/*      Perhaps one day also dottedness, etc.                           */
/* -------------------------------------------------------------------- */
#ifdef notdef
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

/* -------------------------------------------------------------------- */
/*      Do we have a Linetype for the feature?                          */
/* -------------------------------------------------------------------- */
    CPLString osLineType = poFeature->GetFieldAsString( "Linetype" );

    if( osLineType.size() > 0 
        && (poDS->oHeaderDS.LookupLineType( osLineType ) != NULL 
            || oNewLineTypes.count(osLineType) > 0 ) )
    {
        // Already define -> just reference it.
        WriteValue( 6, osLineType );
    }
    else if( poTool != NULL && poTool->GetType() == OGRSTCPen )
    {
        CPLString osDefinition = PrepareLineTypeDefinition( poFeature, 
                                                            poTool );

        if( osDefinition != "" && osLineType == "" )
        {
            // Is this definition already created and named?
            std::map<CPLString,CPLString>::iterator it;

            for( it = oNewLineTypes.begin();
                 it != oNewLineTypes.end();
                 it++ )
            {
                if( (*it).second == osDefinition )
                {
                    osLineType = (*it).first;
                    break;
                }
            }

            // create an automatic name for it.
            if( osLineType == "" )
            {
                do 
                { 
                    osLineType.Printf( "AutoLineType-%d", nNextAutoID++ );
                }
                while( poDS->oHeaderDS.LookupLineType(osLineType) != NULL );
            }
        }

        // If it isn't already defined, add it now.
        if( osDefinition != "" && oNewLineTypes.count(osLineType) == 0 )
        {
            oNewLineTypes[osLineType] = osDefinition;
            WriteValue( 6, osLineType );
        }
    }
    delete poTool;
#endif

/* -------------------------------------------------------------------- */
/*      Process the loops (rings).                                      */
/* -------------------------------------------------------------------- */
    OGRPolygon *poPoly = (OGRPolygon *) poGeom;

    WriteValue( 91, poPoly->getNumInteriorRings() + 1 );

    for( int iRing = -1; iRing < poPoly->getNumInteriorRings(); iRing++ )
    {
        OGRLinearRing *poLR;

        if( iRing == -1 )
            poLR = poPoly->getExteriorRing();
        else
            poLR = poPoly->getInteriorRing( iRing );

        WriteValue( 92, 2 ); // Polyline
        WriteValue( 72, 0 ); // has bulge
        WriteValue( 73, 1 ); // is closed
        WriteValue( 93, poLR->getNumPoints() );
        
        for( int iVert = 0; iVert < poLR->getNumPoints(); iVert++ )
        {
            WriteValue( 10, poLR->getX(iVert) );
            WriteValue( 20, poLR->getY(iVert) );
        }

        WriteValue( 97, 0 ); // 0 source boundary objects
    }

    WriteValue( 75, 0 ); // hatch style = Hatch "odd parity" area (Normal style)
    WriteValue( 76, 1 ); // hatch pattern type = predefined
    WriteValue( 98, 0 ); // 0 seed points
    
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
    {
        if( !poGeom->IsEmpty() )
        {
            OGREnvelope sEnvelope;
            poGeom->getEnvelope(&sEnvelope);
            poDS->UpdateExtent(&sEnvelope);
        }
        eGType = wkbFlatten(poGeom->getGeometryType());
    }

    if( eGType == wkbPoint )
    {
        const char *pszBlockName = poFeature->GetFieldAsString("BlockName");

        // we don't want to treat as a block ref if we are writing blocks layer
        if( pszBlockName != NULL
            && poDS->poBlocksLayer != NULL 
            && poFeature->GetDefnRef() == poDS->poBlocksLayer->GetLayerDefn())
            pszBlockName = NULL;

        // We don't want to treat as a blocks ref if the block is not defined
        if( pszBlockName 
            && poDS->oHeaderDS.LookupBlock(pszBlockName) == NULL )
        {
            if( poDS->poBlocksLayer == NULL
                || poDS->poBlocksLayer->FindBlock(pszBlockName) == NULL )
                pszBlockName = NULL;
        }
                                  
        if( pszBlockName != NULL )
            return WriteINSERT( poFeature );
            
        else if( poFeature->GetStyleString() != NULL
            && EQUALN(poFeature->GetStyleString(),"LABEL",5) )
            return WriteTEXT( poFeature );
        else
            return WritePOINT( poFeature );
    }
    else if( eGType == wkbLineString 
             || eGType == wkbMultiLineString )
        return WritePOLYLINE( poFeature );

    else if( eGType == wkbPolygon 
             || eGType == wkbMultiPolygon )
    {
        if( bWriteHatch )
            return WriteHATCH( poFeature );
        else
            return WritePOLYLINE( poFeature );
    }

    // Explode geometry collections into multiple entities.
    else if( eGType == wkbGeometryCollection )
    {
        OGRGeometryCollection *poGC = (OGRGeometryCollection *)
            poFeature->StealGeometry();
        int iGeom;

        for( iGeom = 0; iGeom < poGC->getNumGeometries(); iGeom++ )
        {
            poFeature->SetGeometry( poGC->getGeometryRef(iGeom) );
                                    
            OGRErr eErr = CreateFeature( poFeature );
            
            if( eErr != OGRERR_NONE )
                return eErr;

        }
        
        poFeature->SetGeometryDirectly( poGC );
        return OGRERR_NONE;
    }
    else 
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "No known way to write feature with geometry '%s'.",
                  OGRGeometryTypeToName(eGType) );
        return OGRERR_FAILURE;
    }
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
    const unsigned char *pabyDXFColors = ACGetColorTable();
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
