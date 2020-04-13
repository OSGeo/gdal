/******************************************************************************
 *
 * Project:  DXF Translator
 * Purpose:  Implements OGRDXFWriterLayer - the OGRLayer class used for
 *           writing a DXF file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include <cstdlib>

CPL_CVSID("$Id$")

/************************************************************************/
/*                         OGRDXFWriterLayer()                          */
/************************************************************************/

OGRDXFWriterLayer::OGRDXFWriterLayer( OGRDXFWriterDS *poDSIn, VSILFILE *fpIn ) :
    fp(fpIn),
    poFeatureDefn(nullptr),  // TODO(schwehr): Can I move the new here?
    poDS(poDSIn)
{
    nNextAutoID = 1;
    bWriteHatch = CPLTestBool(CPLGetConfigOption("DXF_WRITE_HATCH", "YES"));

    poFeatureDefn = new OGRFeatureDefn( "entities" );
    poFeatureDefn->Reference();

    OGRDXFDataSource::AddStandardFields( poFeatureDefn,
        ODFM_IncludeBlockFields );
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
    if( EQUAL(poField->GetNameRef(), "OGR_STYLE") )
    {
        poFeatureDefn->AddFieldDefn(poField);
        return OGRERR_NONE;
    }

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

    CPLsnprintf(szLinePair, sizeof(szLinePair), "%3d\n%.15g\n", nCode, dfValue );
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
    poFeature->SetFID( poDS->WriteEntityID(fp,(int)poFeature->GetFID()) );

    WriteValue( 100, "AcDbEntity" );

/* -------------------------------------------------------------------- */
/*      For now we assign everything to the default layer - layer       */
/*      "0" - if there is no layer property on the source features.     */
/* -------------------------------------------------------------------- */
    const char *pszLayer = poFeature->GetFieldAsString( "Layer" );
    if( pszLayer == nullptr || strlen(pszLayer) == 0 )
    {
        WriteValue( 8, "0" );
    }
    else
    {
        CPLString osSanitizedLayer(pszLayer);
        // Replaced restricted characters with underscore
        // See http://docs.autodesk.com/ACD/2010/ENU/AutoCAD%202010%20User%20Documentation/index.html?url=WS1a9193826455f5ffa23ce210c4a30acaf-7345.htm,topicNumber=d0e41665
        const char achForbiddenChars[] = {
          '<', '>', '/', '\\', '"', ':', ';', '?', '*', '|', '=', '\'' };
        for( size_t i = 0; i < CPL_ARRAYSIZE(achForbiddenChars); ++i )
        {
            osSanitizedLayer.replaceAll( achForbiddenChars[i], '_' );
        }

        // also remove newline characters (#15067)
        osSanitizedLayer.replaceAll( "\r\n", "_" );
        osSanitizedLayer.replaceAll( '\r', '_' );
        osSanitizedLayer.replaceAll( '\n', '_' );

        const char *pszExists =
            poDS->oHeaderDS.LookupLayerProperty( osSanitizedLayer, "Exists" );
        if( (pszExists == nullptr || strlen(pszExists) == 0)
            && CSLFindString( poDS->papszLayersToCreate, osSanitizedLayer ) == -1 )
        {
            poDS->papszLayersToCreate =
                CSLAddString( poDS->papszLayersToCreate, osSanitizedLayer );
        }

        WriteValue( 8, osSanitizedLayer );
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
    WriteValue( 100, "AcDbBlockReference" );
    WriteValue( 2, poFeature->GetFieldAsString("BlockName") );

    // Write style symbol color
    OGRStyleTool *poTool = nullptr;
    OGRStyleMgr oSM;
    if( poFeature->GetStyleString() != nullptr )
    {
        oSM.InitFromFeature( poFeature );

        if( oSM.GetPartCount() > 0 )
            poTool = oSM.GetPart(0);
    }
    if( poTool && poTool->GetType() == OGRSTCSymbol )
    {
        OGRStyleSymbol *poSymbol = (OGRStyleSymbol *) poTool;
        GBool bDefault;

        if( poSymbol->Color(bDefault) != nullptr && !bDefault )
            WriteValue( 62, ColorStringToDXFColor( poSymbol->Color(bDefault) ) );
    }
    delete poTool;

/* -------------------------------------------------------------------- */
/*      Write location in OCS.                                          */
/* -------------------------------------------------------------------- */
    int nCoordCount = 0;
    const double *padfCoords =
        poFeature->GetFieldAsDoubleList( "BlockOCSCoords", &nCoordCount );

    if( nCoordCount == 3 )
    {
        WriteValue( 10, padfCoords[0] );
        WriteValue( 20, padfCoords[1] );
        if( !WriteValue( 30, padfCoords[2] ) )
            return OGRERR_FAILURE;
    }
    else
    {
        // We don't have an OCS; we will just assume that the location of
        // the geometry (in WCS) is the correct insertion point.
        OGRPoint *poPoint = poFeature->GetGeometryRef()->toPoint();

        WriteValue( 10, poPoint->getX() );
        if( !WriteValue( 20, poPoint->getY() ) )
            return OGRERR_FAILURE;

        if( poPoint->getGeometryType() == wkbPoint25D )
        {
            if( !WriteValue( 30, poPoint->getZ() ) )
                return OGRERR_FAILURE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Write scaling.                                                  */
/* -------------------------------------------------------------------- */
    int nScaleCount = 0;
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
    const double dfAngle = poFeature->GetFieldAsDouble( "BlockAngle" );

    if( dfAngle != 0.0 )
    {
        WriteValue( 50, dfAngle ); // degrees
    }

/* -------------------------------------------------------------------- */
/*      Write OCS normal vector.                                        */
/* -------------------------------------------------------------------- */
    int nOCSCount = 0;
    const double *padfOCS =
        poFeature->GetFieldAsDoubleList( "BlockOCSNormal", &nOCSCount );

    if( nOCSCount == 3 )
    {
        WriteValue( 210, padfOCS[0] );
        WriteValue( 220, padfOCS[1] );
        WriteValue( 230, padfOCS[2] );
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
    WriteValue( 100, "AcDbPoint" );

    // Write style pen color
    OGRStyleTool *poTool = nullptr;
    OGRStyleMgr oSM;
    if( poFeature->GetStyleString() != nullptr )
    {
        oSM.InitFromFeature( poFeature );

        if( oSM.GetPartCount() > 0 )
            poTool = oSM.GetPart(0);
    }
    if( poTool && poTool->GetType() == OGRSTCPen )
    {
        OGRStylePen *poPen = (OGRStylePen *) poTool;
        GBool  bDefault;

        if( poPen->Color(bDefault) != nullptr && !bDefault )
            WriteValue( 62, ColorStringToDXFColor( poPen->Color(bDefault) ) );
    }
    delete poTool;

    OGRPoint *poPoint = poFeature->GetGeometryRef()->toPoint();

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
    for( int i = 0; panInput[i] != 0; i++ )
    {
        if( panInput[i] == '\n' )
        {
            osResult += "\\P";
        }
        else if( panInput[i] == ' ' )
        {
            osResult += "\\~";
        }
        else if( panInput[i] == '\\' )
        {
            osResult += "\\\\";
        }
        else if( panInput[i] == '^' )
        {
            osResult += "^ ";
        }
        else if( panInput[i] < ' ' )
        {
            osResult += '^';
            osResult += static_cast<char>( panInput[i] + '@' );
        }
        else if( panInput[i] > 255 )
        {
            CPLString osUnicode;
            osUnicode.Printf( "\\U+%04x", (int) panInput[i] );
            osResult += osUnicode;
        }
        else
        {
            osResult += (char) panInput[i];
        }
    }

    CPLFree(panInput);

    return osResult;
}

/************************************************************************/
/*                     PrepareTextStyleDefinition()                     */
/************************************************************************/
std::map<CPLString, CPLString>
OGRDXFWriterLayer::PrepareTextStyleDefinition( OGRStyleLabel *poLabelTool )
{
    GBool bDefault;

    std::map<CPLString, CPLString> oTextStyleDef;

/* -------------------------------------------------------------------- */
/*      Fetch the data for this text style.                             */
/* -------------------------------------------------------------------- */
    const char *pszFontName = poLabelTool->FontName(bDefault);
    if( !bDefault )
        oTextStyleDef["Font"] = pszFontName;

    const GBool bBold = poLabelTool->Bold(bDefault);
    if( !bDefault )
        oTextStyleDef["Bold"] = bBold ? "1" : "0";

    const GBool bItalic = poLabelTool->Italic(bDefault);
    if( !bDefault )
        oTextStyleDef["Italic"] = bItalic ? "1" : "0";

    const double dfStretch = poLabelTool->Stretch(bDefault);
    if( !bDefault )
    {
        oTextStyleDef["Width"] = CPLString().Printf( "%f",
            dfStretch / 100.0 );
    }

    return oTextStyleDef;
}

/************************************************************************/
/*                             WriteTEXT()                              */
/************************************************************************/

OGRErr OGRDXFWriterLayer::WriteTEXT( OGRFeature *poFeature )

{
    WriteValue( 0, "MTEXT" );
    WriteCore( poFeature );
    WriteValue( 100, "AcDbMText" );

/* -------------------------------------------------------------------- */
/*      Do we have styling information?                                 */
/* -------------------------------------------------------------------- */
    OGRStyleTool *poTool = nullptr;
    OGRStyleMgr oSM;

    if( poFeature->GetStyleString() != nullptr )
    {
        oSM.InitFromFeature( poFeature );

        if( oSM.GetPartCount() > 0 )
            poTool = oSM.GetPart(0);
    }

/* ==================================================================== */
/*      Process the LABEL tool.                                         */
/* ==================================================================== */
    double dfDx = 0.0;
    double dfDy = 0.0;

    if( poTool && poTool->GetType() == OGRSTCLabel )
    {
        OGRStyleLabel *poLabel = (OGRStyleLabel *) poTool;
        GBool  bDefault;

/* -------------------------------------------------------------------- */
/*      Color                                                           */
/* -------------------------------------------------------------------- */
        if( poLabel->ForeColor(bDefault) != nullptr && !bDefault )
            WriteValue( 62, ColorStringToDXFColor(
                            poLabel->ForeColor(bDefault) ) );

/* -------------------------------------------------------------------- */
/*      Angle                                                           */
/* -------------------------------------------------------------------- */
        const double dfAngle = poLabel->Angle(bDefault);

        if( !bDefault )
            WriteValue( 50, dfAngle );

/* -------------------------------------------------------------------- */
/*      Height - We need to fetch this in georeferenced units - I'm     */
/*      doubt the default translation mechanism will be much good.      */
/* -------------------------------------------------------------------- */
        poTool->SetUnit( OGRSTUGround );
        const double dfHeight = poLabel->Size(bDefault);

        if( !bDefault )
            WriteValue( 40, dfHeight );

/* -------------------------------------------------------------------- */
/*      Anchor / Attachment Point                                       */
/* -------------------------------------------------------------------- */
        const int nAnchor = poLabel->Anchor(bDefault);

        if( !bDefault )
        {
            const static int anAnchorMap[] =
                { -1, 7, 8, 9, 4, 5, 6, 1, 2, 3, 7, 8, 9 };

            if( nAnchor > 0 && nAnchor < 13 )
                WriteValue( 71, anAnchorMap[nAnchor] );
        }

/* -------------------------------------------------------------------- */
/*      Offset                                                          */
/* -------------------------------------------------------------------- */
        dfDx = poLabel->SpacingX(bDefault);
        dfDy = poLabel->SpacingY(bDefault);

/* -------------------------------------------------------------------- */
/*      Escape the text, and convert to ISO8859.                        */
/* -------------------------------------------------------------------- */
        const char *pszText = poLabel->TextString( bDefault );

        if( pszText != nullptr && !bDefault )
        {
            CPLString osEscaped = TextEscape( pszText );
            while( osEscaped.size() > 250 )
            {
                WriteValue( 3, osEscaped.substr( 0, 250 ).c_str() );
                osEscaped.erase( 0, 250 );
            }
            WriteValue( 1, osEscaped );
        }

/* -------------------------------------------------------------------- */
/*      Store the text style in the map.                                */
/* -------------------------------------------------------------------- */
        std::map<CPLString, CPLString> oTextStyleDef =
            PrepareTextStyleDefinition( poLabel );
        CPLString osStyleName;

        for( const auto& oPair: oNewTextStyles )
        {
            if( oPair.second == oTextStyleDef )
            {
                osStyleName = oPair.first;
                break;
            }
        }

        if( osStyleName == "" )
        {

            do
            {
                osStyleName.Printf( "AutoTextStyle-%d", nNextAutoID++ );
            }
            while( poDS->oHeaderDS.TextStyleExists( osStyleName ) );

            oNewTextStyles[osStyleName] = oTextStyleDef;
        }

        WriteValue( 7, osStyleName );
    }

    delete poTool;

/* -------------------------------------------------------------------- */
/*      Write the location.                                             */
/* -------------------------------------------------------------------- */
    OGRPoint *poPoint = poFeature->GetGeometryRef()->toPoint();

    WriteValue( 10, poPoint->getX() + dfDx );
    if( !WriteValue( 20, poPoint->getY() + dfDy ) )
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
std::vector<double>
OGRDXFWriterLayer::PrepareLineTypeDefinition( OGRStylePen *poPen )
{

/* -------------------------------------------------------------------- */
/*      Fetch pattern.                                                  */
/* -------------------------------------------------------------------- */
    GBool bDefault;
    const char *pszPattern = poPen->Pattern( bDefault );

    if( bDefault || strlen(pszPattern) == 0 )
        return std::vector<double>();

/* -------------------------------------------------------------------- */
/*      Split into pen up / pen down bits.                              */
/* -------------------------------------------------------------------- */
    char **papszTokens = CSLTokenizeString(pszPattern);
    std::vector<double> adfWeightTokens;

    for( int i = 0; papszTokens != nullptr && papszTokens[i] != nullptr; i++ )
    {
        const char *pszToken = papszTokens[i];
        CPLString osAmount;
        CPLString osDXFEntry;

        // Split amount and unit.
        const char *pszUnit = pszToken;  // Used after for.
        for( ;
             strchr( "0123456789.", *pszUnit) != nullptr;
             pszUnit++ ) {}

        osAmount.assign(pszToken,(int) (pszUnit-pszToken));

        // If the unit is other than 'g' we really should be trying to
        // do some type of transformation - but what to do?  Pretty hard.

        // Even entries are "pen down" represented as positive in DXF.
        // "Pen up" entries (gaps) are represented as negative.
        if( i%2 == 0 )
            adfWeightTokens.push_back( CPLAtof( osAmount ) );
        else
            adfWeightTokens.push_back( -CPLAtof( osAmount ) );
    }

    CSLDestroy( papszTokens );

    return adfWeightTokens;
}

/************************************************************************/
/*                       IsLineTypeProportional()                       */
/************************************************************************/

static double IsLineTypeProportional( const std::vector<double>& adfA,
    const std::vector<double>& adfB )
{
    // If they are not the same length, they are not the same linetype
    if( adfA.size() != adfB.size() )
        return 0.0;

    // Determine the proportion of the first elements
    const double dfRatio = ( adfA[0] != 0.0 ) ? ( adfB[0] / adfA[0] ) : 0.0;

    // Check if all elements follow this proportionality
    for( size_t iIndex = 1; iIndex < adfA.size(); iIndex++ )
        if( fabs( adfB[iIndex] - ( adfA[iIndex] * dfRatio ) ) > 1e-6 )
            return 0.0;

    return dfRatio;
}

/************************************************************************/
/*                           WritePOLYLINE()                            */
/************************************************************************/

OGRErr OGRDXFWriterLayer::WritePOLYLINE( OGRFeature *poFeature,
                                         const OGRGeometry *poGeom )

{
/* -------------------------------------------------------------------- */
/*      For now we handle multilinestrings by writing a series of       */
/*      entities.                                                       */
/* -------------------------------------------------------------------- */
    if( poGeom == nullptr )
        poGeom = poFeature->GetGeometryRef();

    if ( poGeom->IsEmpty() )
    {
        return OGRERR_NONE;
    }

    if( wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon
        || wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString )
    {
        const OGRGeometryCollection *poGC = poGeom->toGeometryCollection();
        OGRErr eErr = OGRERR_NONE;
        for( auto&& poMember: *poGC )
        {
            eErr = WritePOLYLINE( poFeature, poMember );
            if( eErr != OGRERR_NONE )
                break;
        }

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Polygons are written with on entity per ring.                   */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(poGeom->getGeometryType()) == wkbPolygon
        || wkbFlatten(poGeom->getGeometryType()) == wkbTriangle)
    {
        const OGRPolygon *poPoly = poGeom->toPolygon();
        OGRErr eErr = OGRERR_NONE;
        for( auto&& poRing: *poPoly )
        {
            eErr = WritePOLYLINE( poFeature, poRing );
            if( eErr != OGRERR_NONE )
                break;
        }

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Do we now have a geometry we can work with?                     */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(poGeom->getGeometryType()) != wkbLineString )
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

    const OGRLineString *poLS = poGeom->toLineString();

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
    OGRStyleTool *poTool = nullptr;
    OGRStyleMgr oSM;

    if( poFeature->GetStyleString() != nullptr )
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
        GBool bDefault;

        if( poPen->Color(bDefault) != nullptr && !bDefault )
            WriteValue( 62, ColorStringToDXFColor( poPen->Color(bDefault) ) );

        // we want to fetch the width in ground units.
        poPen->SetUnit( OGRSTUGround, 1.0 );
        const double dfWidth = poPen->Width(bDefault);

        if( !bDefault )
            WriteValue( 370, (int) floor(dfWidth * 100 + 0.5) );
    }

/* -------------------------------------------------------------------- */
/*      Do we have a Linetype for the feature?                          */
/* -------------------------------------------------------------------- */
    CPLString osLineType = poFeature->GetFieldAsString( "Linetype" );
    double dfLineTypeScale = 0.0;

    bool bGotLinetype = false;

    if( !osLineType.empty() )
    {
        std::vector<double> adfLineType =
            poDS->oHeaderDS.LookupLineType( osLineType );

        if( adfLineType.empty() && oNewLineTypes.count(osLineType) > 0 )
            adfLineType = oNewLineTypes[osLineType];

        if( !adfLineType.empty() )
        {
            bGotLinetype = true;
            WriteValue( 6, osLineType );

            // If the given linetype is proportional to the linetype data
            // in the style string, then apply a linetype scale
            if( poTool != nullptr && poTool->GetType() == OGRSTCPen )
            {
                std::vector<double> adfDefinition = PrepareLineTypeDefinition(
                    static_cast<OGRStylePen *>( poTool ) );
                
                if( !adfDefinition.empty() )
                {
                    dfLineTypeScale = IsLineTypeProportional( adfLineType,
                        adfDefinition );

                    if( dfLineTypeScale != 0.0 && 
                        fabs( dfLineTypeScale - 1.0 ) > 1e-4 )
                    {
                        WriteValue( 48, dfLineTypeScale );
                    }
                }
            }
        }
    }

    if( !bGotLinetype && poTool != nullptr && poTool->GetType() == OGRSTCPen )
    {
        std::vector<double> adfDefinition = PrepareLineTypeDefinition(
            static_cast<OGRStylePen *>( poTool ) );

        if( !adfDefinition.empty() )
        {
            // Is this definition already created and named?
            for( const auto& oPair : poDS->oHeaderDS.GetLineTypeTable() )
            {
                dfLineTypeScale = IsLineTypeProportional( oPair.second,
                    adfDefinition );
                if( dfLineTypeScale != 0.0 )
                {
                    osLineType = oPair.first;
                    break;
                }
            }

            if( dfLineTypeScale == 0.0 )
            {
                for( const auto& oPair : oNewLineTypes )
                {
                    dfLineTypeScale = IsLineTypeProportional( oPair.second,
                        adfDefinition );
                    if( dfLineTypeScale != 0.0 )
                    {
                        osLineType = oPair.first;
                        break;
                    }
                }
            }

            // If not, create an automatic name for it.
            if( osLineType == "" )
            {
                dfLineTypeScale = 1.0;
                do
                {
                    osLineType.Printf( "AutoLineType-%d", nNextAutoID++ );
                }
                while( poDS->oHeaderDS.LookupLineType(osLineType).size() > 0 );
            }

            // If it isn't already defined, add it now.
            if( poDS->oHeaderDS.LookupLineType( osLineType ).empty() &&
                oNewLineTypes.count( osLineType ) == 0 )
            {
                oNewLineTypes[osLineType] = adfDefinition;
            }

            WriteValue( 6, osLineType );

            if( dfLineTypeScale != 0.0 && fabs( dfLineTypeScale - 1.0 ) > 1e-4 )
            {
                WriteValue( 48, dfLineTypeScale );
            }
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

    for( int iVert = 0; iVert < poLS->getNumPoints(); iVert++ )
    {
        if( bHasDifferentZ )
        {
            WriteValue( 0, "VERTEX" );
            WriteCore( poFeature );
            WriteValue( 100, "AcDbVertex" );
            WriteValue( 100, "AcDb3dPolylineVertex" );
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
    }

    delete poTool;

    return OGRERR_NONE;

#ifdef notdef
/* -------------------------------------------------------------------- */
/*      Alternate unmaintained implementation as a polyline entity.     */
/* -------------------------------------------------------------------- */
    WriteValue( 0, "POLYLINE" );
    WriteCore( poFeature );
    WriteValue( 100, "AcDbPolyline" );
    if( EQUAL( poGeom->getGeometryName(), "LINEARRING" ) )
        WriteValue( 70, 1 );
    else
        WriteValue( 70, 0 );
    WriteValue( 66, "1" );

    for( int iVert = 0; iVert < poLS->getNumPoints(); iVert++ )
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
    if( poGeom == nullptr )
        poGeom = poFeature->GetGeometryRef();

    if ( poGeom->IsEmpty() )
    {
        return OGRERR_NONE;
    }

    if( wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon )
    {
        OGRErr eErr = OGRERR_NONE;
        for( auto&& poMember: poGeom->toMultiPolygon() )
        {
            eErr = WriteHATCH( poFeature, poMember );
            if( eErr != OGRERR_NONE )
                break;
        }

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Do we now have a geometry we can work with?                     */
/* -------------------------------------------------------------------- */
    if( wkbFlatten(poGeom->getGeometryType()) != wkbPolygon &&
        wkbFlatten(poGeom->getGeometryType()) != wkbTriangle )
    {
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
    }

/* -------------------------------------------------------------------- */
/*      Write as a hatch.                                               */
/* -------------------------------------------------------------------- */
    WriteValue( 0, "HATCH" );
    WriteCore( poFeature );
    WriteValue( 100, "AcDbHatch" );

    // Figure out "average" elevation
    OGREnvelope3D oEnv;
    poGeom->getEnvelope( &oEnv );
    WriteValue( 10, 0 ); // elevation point X = 0
    WriteValue( 20, 0 ); // elevation point Y = 0
    // elevation point Z = constant elevation
    WriteValue( 30, oEnv.MinZ + ( oEnv.MaxZ - oEnv.MinZ ) / 2 );

    WriteValue(210, 0 ); // extrusion direction X
    WriteValue(220, 0 ); // extrusion direction Y
    WriteValue(230,1.0); // extrusion direction Z

    WriteValue( 2, "SOLID" ); // fill pattern
    WriteValue( 70, 1 ); // solid fill
    WriteValue( 71, 0 ); // associativity

/* -------------------------------------------------------------------- */
/*      Do we have styling information?                                 */
/* -------------------------------------------------------------------- */
    OGRStyleTool *poTool = nullptr;
    OGRStyleMgr oSM;

    if( poFeature->GetStyleString() != nullptr )
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

        if( poBrush->ForeColor(bDefault) != nullptr && !bDefault )
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

    if( !osLineType.empty()
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
    OGRPolygon *poPoly = poGeom->toPolygon();

    WriteValue( 91, poPoly->getNumInteriorRings() + 1 );

    for( auto&& poLR: *poPoly )
    {
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
    WriteValue( 100, "AcDbPolyline" );
    if( EQUAL( poGeom->getGeometryName(), "LINEARRING" ) )
        WriteValue( 70, 1 );
    else
        WriteValue( 70, 0 );
    WriteValue( 66, "1" );

    for( int iVert = 0; iVert < poLS->getNumPoints(); iVert++ )
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
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRDXFWriterLayer::ICreateFeature( OGRFeature *poFeature )

{
    OGRGeometry *poGeom = poFeature->GetGeometryRef();
    OGRwkbGeometryType eGType = wkbNone;

    if( poGeom != nullptr )
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

        // We don't want to treat as a blocks ref if the block is not defined
        if( pszBlockName
            && poDS->oHeaderDS.LookupBlock(pszBlockName) == nullptr )
        {
            if( poDS->poBlocksLayer == nullptr
                || poDS->poBlocksLayer->FindBlock(pszBlockName) == nullptr )
                pszBlockName = nullptr;
        }

        if( pszBlockName != nullptr )
            return WriteINSERT( poFeature );

        else if( poFeature->GetStyleString() != nullptr
            && STARTS_WITH_CI(poFeature->GetStyleString(), "LABEL") )
            return WriteTEXT( poFeature );
        else
            return WritePOINT( poFeature );
    }
    else if( eGType == wkbLineString
             || eGType == wkbMultiLineString )
        return WritePOLYLINE( poFeature );

    else if( eGType == wkbPolygon
             || eGType == wkbTriangle
             || eGType == wkbMultiPolygon)
    {
        if( bWriteHatch )
            return WriteHATCH( poFeature );
        else
            return WritePOLYLINE( poFeature );
    }

    // Explode geometry collections into multiple entities.
    else if( eGType == wkbGeometryCollection )
    {
        OGRGeometryCollection *poGC =
            poFeature->StealGeometry()->toGeometryCollection();
        for( auto&& poMember: poGC )
        {
            poFeature->SetGeometry( poMember );

            OGRErr eErr = CreateFeature( poFeature );

            if( eErr != OGRERR_NONE )
            {
                delete poGC;
                return eErr;
            }
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
    if( pszRGB == nullptr )
        return -1;

    unsigned int nRed = 0;
    unsigned int nGreen = 0;
    unsigned int nBlue = 0;
    unsigned int nTransparency = 255;

    const int nCount =
        sscanf(pszRGB, "#%2x%2x%2x%2x", &nRed, &nGreen, &nBlue, &nTransparency);

    if (nCount < 3 )
        return -1;

/* -------------------------------------------------------------------- */
/*      Find near color in DXF palette.                                 */
/* -------------------------------------------------------------------- */
    const unsigned char *pabyDXFColors = ACGetColorTable();
    int nMinDist = 768;
    int nBestColor = -1;

    for( int i = 1; i < 256; i++ )
    {
        const int nDist =
            std::abs(static_cast<int>(nRed) - pabyDXFColors[i*3+0])
            + std::abs(static_cast<int>(nGreen) - pabyDXFColors[i*3+1])
            + std::abs(static_cast<int>(nBlue)  - pabyDXFColors[i*3+2]);

        if( nDist < nMinDist )
        {
            nBestColor = i;
            nMinDist = nDist;
        }
    }

    return nBestColor;
}
