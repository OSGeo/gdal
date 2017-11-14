/******************************************************************************
 *
 * Project:  DXF Translator
 * Purpose:  Implements OGRDXFLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogrdxf_polyline_smooth.h"
#include "ogr_api.h"

#include <cmath>
#include <algorithm>
#include <stdexcept>

CPL_CVSID("$Id$")

/************************************************************************/
/*                            OGRDXFLayer()                             */
/************************************************************************/

OGRDXFLayer::OGRDXFLayer( OGRDXFDataSource *poDSIn ) :
    poDS(poDSIn),
    poFeatureDefn(new OGRFeatureDefn( "entities" )),
    iNextFID(0)
{
    poFeatureDefn->Reference();

    poDS->AddStandardFields( poFeatureDefn );

    SetDescription( poFeatureDefn->GetName() );
}

/************************************************************************/
/*                           ~OGRDXFLayer()                           */
/************************************************************************/

OGRDXFLayer::~OGRDXFLayer()

{
    ClearPendingFeatures();
    if( m_nFeaturesRead > 0 && poFeatureDefn != NULL )
    {
        CPLDebug( "DXF", "%d features read on layer '%s'.",
                  (int) m_nFeaturesRead,
                  poFeatureDefn->GetName() );
    }

    if( poFeatureDefn )
        poFeatureDefn->Release();
}

/************************************************************************/
/*                        ClearPendingFeatures()                        */
/************************************************************************/

void OGRDXFLayer::ClearPendingFeatures()

{
    while( !apoPendingFeatures.empty() )
    {
        delete apoPendingFeatures.front();
        apoPendingFeatures.pop();
    }
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRDXFLayer::ResetReading()

{
    iNextFID = 0;
    ClearPendingFeatures();
    poDS->RestartEntities();
}

/************************************************************************/
/*                      TranslateGenericProperty()                      */
/*                                                                      */
/*      Try and convert entity properties handled similarly for most    */
/*      or all entity types.                                            */
/************************************************************************/

void OGRDXFLayer::TranslateGenericProperty( OGRDXFFeature *poFeature,
                                            int nCode, char *pszValue )

{
    switch( nCode )
    {
      case 8:
        poFeature->SetField( "Layer", TextRecode(pszValue) );
        break;

      case 100:
      {
          CPLString osSubClass = poFeature->GetFieldAsString("SubClasses");
          if( !osSubClass.empty() )
              osSubClass += ":";
          osSubClass += pszValue;
          poFeature->SetField( "SubClasses", osSubClass.c_str() );
      }
      break;

      case 62:
        poFeature->oStyleProperties["Color"] = pszValue;
        break;

      case 6:
        poFeature->SetField( "Linetype", TextRecode(pszValue) );
        break;

      case 48:
        poFeature->oStyleProperties["LinetypeScale"] = pszValue;
        break;

      case 370:
      case 39:
        poFeature->oStyleProperties["LineWeight"] = pszValue;
        break;

      case 5:
        poFeature->SetField( "EntityHandle", pszValue );
        break;

        // Extended entity data
      case 1000:
      case 1002:
      case 1004:
      case 1005:
      case 1040:
      case 1041:
      case 1070:
      case 1071:
      {
          CPLString osAggregate = poFeature->GetFieldAsString("ExtendedEntity");

          if( !osAggregate.empty() )
              osAggregate += " ";
          osAggregate += TextRecode(pszValue);

          poFeature->SetField( "ExtendedEntity", osAggregate );
      }
      break;

      // OCS vector.
      case 210:
        poFeature->oOCS.dfX = CPLAtof( pszValue );
        break;

      case 220:
        poFeature->oOCS.dfY = CPLAtof( pszValue );
        break;

      case 230:
        poFeature->oOCS.dfZ = CPLAtof( pszValue );
        break;

      default:
        break;
    }
}

/************************************************************************/
/*                        PrepareFeatureStyle()                         */
/*                                                                      */
/*     - poByBlockStyleProps: Style properties to use for               */
/*       attributes with ByBlock values set.                            */
/************************************************************************/

void OGRDXFLayer::PrepareFeatureStyle( OGRDXFFeature* const poFeature,
    OGRDXFFeature* const poBlockFeature /* = NULL */ )

{
    // Not sure of the best way of telling these apart.
    if( poFeature->oStyleProperties.count( "WantBrush" ) )
        PrepareHatchStyle( poFeature, poBlockFeature );
    else 
        PrepareLineStyle( poFeature, poBlockFeature );
}

/************************************************************************/
/*                          PrepareLineStyle()                          */
/*                                                                      */
/*     - poBlockFeature: If this is not NULL, style properties on       */
/*       poFeature with ByBlock values will be replaced with the        */
/*       corresponding property from poBlockFeature.  If this           */
/*       parameter is supplied it is assumed that poFeature is a        */
/*       clone, not an "original" feature object.                       */
/************************************************************************/

void OGRDXFLayer::PrepareLineStyle( OGRDXFFeature* const poFeature,
    OGRDXFFeature* const poBlockFeature /* = NULL */ )

{
    CPLString osLayer = poFeature->GetFieldAsString("Layer");

/* -------------------------------------------------------------------- */
/*      Is the layer disabled/hidden/frozen/off?                        */
/* -------------------------------------------------------------------- */
    const char* pszHidden = poDS->LookupLayerProperty( osLayer, "Hidden" );
    const bool bHidden = pszHidden && EQUAL(pszHidden, "1");

    // TODO put in object hidden here

/* -------------------------------------------------------------------- */
/*      Work out the color for this feature.                            */
/* -------------------------------------------------------------------- */
    int nColor = 256;

    if( poFeature->oStyleProperties.count("Color") > 0 )
        nColor = atoi(poFeature->oStyleProperties["Color"]);

    // Use ByBlock color?
    if( nColor < 1 )
    {
        if( poBlockFeature &&
            poBlockFeature->oStyleProperties.count("Color") > 0 )
        {
            // Inherit color from the owning block
            nColor = atoi(poBlockFeature->oStyleProperties["Color"]);

            // Use the inherited color if we regenerate the style string
            // again during block insertion
            poFeature->oStyleProperties["Color"] =
                poBlockFeature->oStyleProperties["Color"];
        }
        else
        {
            // Default to black/white
            nColor = 7;
        }
    }
    // Use layer color?
    else if( nColor > 255 )
    {
        const char *pszValue = poDS->LookupLayerProperty( osLayer, "Color" );
        if( pszValue != NULL )
            nColor = atoi(pszValue);
    }

    if( nColor < 1 || nColor > 255 )
        return;

/* -------------------------------------------------------------------- */
/*      Get line weight if available.                                   */
/* -------------------------------------------------------------------- */
    double dfWeight = 0.0;

    if( poFeature->oStyleProperties.count("LineWeight") > 0 )
    {
        CPLString osWeight = poFeature->oStyleProperties["LineWeight"];

        // Use ByBlock lineweight?
        if( osWeight == "-2" && poBlockFeature &&
            poBlockFeature->oStyleProperties.count("LineWeight") > 0 )
        {
            // Inherit lineweight from the owning block
            osWeight = poBlockFeature->oStyleProperties["LineWeight"];

            // Use the inherited lineweight if we regenerate the style string
            // again during block insertion
            poFeature->oStyleProperties["LineWeight"] = osWeight;
        }
        // Use layer lineweight?
        else if( osWeight == "-1" )
        {
            osWeight = poDS->LookupLayerProperty(osLayer,"LineWeight");
        }

        dfWeight = CPLAtof(osWeight) / 100.0;
    }

/* -------------------------------------------------------------------- */
/*      Do we have a dash/dot line style?                               */
/* -------------------------------------------------------------------- */
    const char *pszLinetype = poFeature->GetFieldAsString("Linetype");

    // Use ByBlock line style?
    if( pszLinetype && EQUAL( pszLinetype, "ByBlock" ) && poBlockFeature )
    {
        pszLinetype = poBlockFeature->GetFieldAsString("Linetype");

        // Use the inherited line style if we regenerate the style string
        // again during block insertion
        if( pszLinetype )
            poFeature->SetField( "Linetype", pszLinetype );
    }

    const std::vector<double> oLineType = poDS->LookupLineType( pszLinetype );

    // Linetype scale is not inherited from the block feature
    double dfLineTypeScale = CPLAtof( poDS->GetVariable( "$LTSCALE", "1.0" ) );
    if( poFeature->oStyleProperties.count( "LinetypeScale" ) > 0 )
        dfLineTypeScale *= CPLAtof( poFeature->oStyleProperties["LinetypeScale"] );

    CPLString osPattern;
    for( std::vector<double>::const_iterator oIt = oLineType.begin();
        oIt != oLineType.end(); ++oIt )
    {
        // this is the format specifier %g followed by a literal 'g'
        osPattern += CPLString().Printf( "%.11gg ",
            fabs( *oIt ) * dfLineTypeScale );
    }

    if( osPattern.length() > 0 )
        osPattern.erase( osPattern.end() - 1 );

/* -------------------------------------------------------------------- */
/*      Format the style string.                                        */
/* -------------------------------------------------------------------- */
    CPLString osStyle;
    const unsigned char *pabyDXFColors = ACGetColorTable();

    osStyle.Printf( "PEN(c:#%02x%02x%02x",
                    pabyDXFColors[nColor*3+0],
                    pabyDXFColors[nColor*3+1],
                    pabyDXFColors[nColor*3+2] );

    if( bHidden )
        osStyle += "00";

    if( dfWeight > 0.0 )
    {
        char szBuffer[64];
        CPLsnprintf(szBuffer, sizeof(szBuffer), "%.2g", dfWeight);
        osStyle += CPLString().Printf( ",w:%sg", szBuffer );
    }

    if( osPattern != "" )
    {
        osStyle += ",p:\"";
        osStyle += osPattern;
        osStyle += "\"";
    }

    osStyle += ")";

    poFeature->SetStyleString( osStyle );
}

/************************************************************************/
/*                            OCSTransformer                            */
/************************************************************************/

class OCSTransformer : public OGRCoordinateTransformation
{
private:
    double adfN[3];
    double adfAX[3];
    double adfAY[3];

    double dfDeterminant;
    double aadfInverse[4][4];

    static double Det2x2( double a, double b, double c, double d )
    {
        return a*d - b*c;
    }

public:
    OCSTransformer( double adfNIn[3], bool bInverse = false ) :
        aadfInverse()
    {
        static const double dSmall = 1.0 / 64.0;
        static const double adfWZ[3] = { 0.0, 0.0, 1.0 };
        static const double adfWY[3] = { 0.0, 1.0, 0.0 };

        dfDeterminant = 0.0;
        Scale2Unit( adfNIn );
        memcpy( adfN, adfNIn, sizeof(double)*3 );

    if ((std::abs(adfN[0]) < dSmall) && (std::abs(adfN[1]) < dSmall))
            CrossProduct(adfWY, adfN, adfAX);
    else
            CrossProduct(adfWZ, adfN, adfAX);

    Scale2Unit( adfAX );
    CrossProduct(adfN, adfAX, adfAY);
    Scale2Unit( adfAY );

    if( bInverse == true ) {
        const double a[4] = { 0.0, adfAX[0], adfAY[0], adfN[0] };
        const double b[4] = { 0.0, adfAX[1], adfAY[1], adfN[1] };
        const double c[4] = { 0.0, adfAX[2], adfAY[2], adfN[2] };

        dfDeterminant = a[1]*b[2]*c[3] - a[1]*b[3]*c[2]
                      + a[2]*b[3]*c[1] - a[2]*b[1]*c[3]
                      + a[3]*b[1]*c[2] - a[3]*b[2]*c[1];

        if( dfDeterminant != 0.0 ) {
            const double k = 1.0 / dfDeterminant;
            const double a11 = adfAX[0];
            const double a12 = adfAY[0];
            const double a13 = adfN[0];
            const double a21 = adfAX[1];
            const double a22 = adfAY[1];
            const double a23 = adfN[1];
            const double a31 = adfAX[2];
            const double a32 = adfAY[2];
            const double a33 = adfN[2];

            aadfInverse[1][1] = k * Det2x2( a22,a23,a32,a33 );
            aadfInverse[1][2] = k * Det2x2( a13,a12,a33,a32 );
            aadfInverse[1][3] = k * Det2x2( a12,a13,a22,a23 );

            aadfInverse[2][1] = k * Det2x2( a23,a21,a33,a31 );
            aadfInverse[2][2] = k * Det2x2( a11,a13,a31,a33 );
            aadfInverse[2][3] = k * Det2x2( a13,a11,a23,a21 );

            aadfInverse[3][1] = k * Det2x2( a21,a22,a31,a32 );
            aadfInverse[3][2] = k * Det2x2( a12,a11,a32,a31 );
            aadfInverse[3][3] = k * Det2x2( a11,a12,a21,a22 );
        }
    }
    }

    static void CrossProduct(const double *a, const double *b, double *vResult) {
        vResult[0] = a[1] * b[2] - a[2] * b[1];
        vResult[1] = a[2] * b[0] - a[0] * b[2];
        vResult[2] = a[0] * b[1] - a[1] * b[0];
    }

    static void Scale2Unit(double* adfV) {
        double dfLen=sqrt(adfV[0]*adfV[0] + adfV[1]*adfV[1] + adfV[2]*adfV[2]);
        if (dfLen != 0)
        {
                adfV[0] /= dfLen;
                adfV[1] /= dfLen;
                adfV[2] /= dfLen;
        }
    }
    OGRSpatialReference *GetSourceCS() override { return NULL; }
    OGRSpatialReference *GetTargetCS() override { return NULL; }
    int Transform( int nCount,
                   double *x, double *y, double *z ) override
        { return TransformEx( nCount, x, y, z, NULL ); }

    int TransformEx( int nCount,
                     double *adfX, double *adfY, double *adfZ,
                     int *pabSuccess = NULL ) override
        {
            for( int i = 0; i < nCount; i++ )
            {
                const double x = adfX[i];
                const double y = adfY[i];
                const double z = adfZ[i];

                adfX[i] = x * adfAX[0] + y * adfAY[0] + z * adfN[0];
                adfY[i] = x * adfAX[1] + y * adfAY[1] + z * adfN[1];
                adfZ[i] = x * adfAX[2] + y * adfAY[2] + z * adfN[2];

                if( pabSuccess )
                    pabSuccess[i] = TRUE;
            }
            return TRUE;
        }

    int InverseTransform( int nCount,
                          double *adfX, double *adfY, double *adfZ )
    {
        if( dfDeterminant == 0.0 )
            return FALSE;

        for( int i = 0; i < nCount; i++ )
        {
            const double x = adfX[i];
            const double y = adfY[i];
            const double z = adfZ[i];

            adfX[i] = x * aadfInverse[1][1] + y * aadfInverse[1][2]
                    + z * aadfInverse[1][3];
            adfY[i] = x * aadfInverse[2][1] + y * aadfInverse[2][2]
                    + z * aadfInverse[2][3];
            adfZ[i] = x * aadfInverse[3][1] + y * aadfInverse[3][2]
                    + z * aadfInverse[3][3];
        }
        return TRUE;
    }
};

/************************************************************************/
/*                         ApplyOCSTransformer()                        */
/*                                                                      */
/*      Apply a transformation from the given OCS to world              */
/*      coordinates.                                                    */
/************************************************************************/

void OGRDXFLayer::ApplyOCSTransformer( OGRGeometry *poGeometry,
    const DXFTriple& oOCS )

{
    if( poGeometry == NULL )
        return;

    double adfN[3];
    oOCS.ToArray( adfN );

    OCSTransformer oTransformer( adfN );

    // Promote to 3D, in case the OCS transformation introduces a
    // third dimension to the geometry.
    const bool bInitially2D = !poGeometry->Is3D();
    if( bInitially2D )
        poGeometry->set3D( TRUE );

    poGeometry->transform( &oTransformer );

    // If the geometry was 2D to begin with, and is still 2D after the
    // OCS transformation, flatten it back to 2D.
    if( bInitially2D )
    {
        OGREnvelope3D oEnvelope;
        poGeometry->getEnvelope( &oEnvelope );
        if( oEnvelope.MaxZ == 0.0 && oEnvelope.MinZ == 0.0 )
            poGeometry->flattenTo2D();
    }
}

/************************************************************************/
/*                             TextRecode()                             */
/************************************************************************/

CPLString OGRDXFLayer::TextRecode( const char *pszInput )

{
    return CPLString( pszInput ).Recode( poDS->GetEncoding(), CPL_ENC_UTF8 );
}

/************************************************************************/
/*                            TextUnescape()                            */
/*                                                                      */
/*      Unexcape DXF style escape sequences such as \P for newline      */
/*      and \~ for space, and do the recoding to UTF8.                  */
/************************************************************************/

CPLString OGRDXFLayer::TextUnescape( const char *pszInput, bool bIsMText )

{
    return ACTextUnescape( pszInput, poDS->GetEncoding(), bIsMText );
}

/************************************************************************/
/*                           TranslateMTEXT()                           */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::TranslateMTEXT()

{
    char szLineBuf[257];
    int nCode = 0;
    OGRDXFFeature *poFeature = new OGRDXFFeature( poFeatureDefn );
    double dfX = 0.0;
    double dfY = 0.0;
    double dfZ = 0.0;
    double dfAngle = 0.0;
    double dfHeight = 0.0;
    double dfXDirection = 0.0;
    double dfYDirection = 0.0;
    bool bHaveZ = false;
    int nAttachmentPoint = -1;
    CPLString osText;
    CPLString osStyleName = "Arial";

    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfX = CPLAtof(szLineBuf);
            break;

          case 20:
            dfY = CPLAtof(szLineBuf);
            break;

          case 30:
            dfZ = CPLAtof(szLineBuf);
            bHaveZ = true;
            break;

          case 40:
            dfHeight = CPLAtof(szLineBuf);
            break;

          case 71:
            nAttachmentPoint = atoi(szLineBuf);
            break;

          case 11:
            dfXDirection = CPLAtof(szLineBuf);
            break;

          case 21:
            dfYDirection = CPLAtof(szLineBuf);
            dfAngle = atan2( dfYDirection, dfXDirection ) * 180.0 / M_PI;
            break;

          case 1:
          case 3:
            if( osText != "" )
                osText += "\n";
            osText += TextUnescape(szLineBuf, true);
            break;

          case 50:
            dfAngle = CPLAtof(szLineBuf);
            break;

          case 7:
            osStyleName = TextRecode(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }
    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        delete poFeature;
        return NULL;
    }

    if( nCode == 0 )
        poDS->UnreadValue();

    OGRPoint* poGeom = NULL;
    if( bHaveZ )
        poGeom = new OGRPoint( dfX, dfY, dfZ );
    else
        poGeom = new OGRPoint( dfX, dfY );

    /* We do NOT apply the OCS for MTEXT. See https://trac.osgeo.org/gdal/ticket/7049 */
    /* ApplyOCSTransformer( poGeom ); */

    poFeature->SetGeometryDirectly( poGeom );

/* -------------------------------------------------------------------- */
/*      Apply text after stripping off any extra terminating newline.   */
/* -------------------------------------------------------------------- */
    if( !osText.empty() && osText.back() == '\n' )
        osText.resize( osText.size() - 1 );

    poFeature->SetField( "Text", osText );

/* -------------------------------------------------------------------- */
/*      We need to escape double quotes with backslashes before they    */
/*      can be inserted in the style string.                            */
/* -------------------------------------------------------------------- */
    if( strchr( osText, '"') != NULL )
    {
        CPLString osEscaped;

        for( size_t iC = 0; iC < osText.size(); iC++ )
        {
            if( osText[iC] == '"' )
                osEscaped += "\\\"";
            else
                osEscaped += osText[iC];
        }
        osText = osEscaped;
    }

/* -------------------------------------------------------------------- */
/*      Work out the color for this feature.                            */
/* -------------------------------------------------------------------- */
    int nColor = 256;

    if( poFeature->oStyleProperties.count("Color") > 0 )
        nColor = atoi(poFeature->oStyleProperties["Color"]);

    // Use layer color?
    if( nColor < 1 || nColor > 255 )
    {
        CPLString osLayer = poFeature->GetFieldAsString("Layer");
        const char *pszValue = poDS->LookupLayerProperty( osLayer, "Color" );
        if( pszValue != NULL )
            nColor = atoi(pszValue);
    }

/* -------------------------------------------------------------------- */
/*      Prepare style string.                                           */
/* -------------------------------------------------------------------- */
    CPLString osStyle;
    char szBuffer[64];

    osStyle.Printf("LABEL(f:\"%s\",t:\"%s\"", osStyleName.c_str(), osText.c_str());

    if( dfAngle != 0.0 )
    {
        CPLsnprintf(szBuffer, sizeof(szBuffer), "%.3g", dfAngle);
        osStyle += CPLString().Printf(",a:%s", szBuffer);
    }

    if( dfHeight != 0.0 )
    {
        CPLsnprintf(szBuffer, sizeof(szBuffer), "%.3g", dfHeight);
        osStyle += CPLString().Printf(",s:%sg", szBuffer);
    }

    if( dfXDirection != 0.0 )
    {
        CPLsnprintf(szBuffer, sizeof(szBuffer), "%.6g", dfXDirection);
        osStyle += CPLString().Printf(",dx:%s", szBuffer);
    }

    if( dfYDirection != 0.0 )
    {
        CPLsnprintf(szBuffer, sizeof(szBuffer), "%.6g", dfYDirection);
        osStyle += CPLString().Printf(",dy:%s", szBuffer);
    }

    if( nAttachmentPoint >= 0 && nAttachmentPoint <= 9 )
    {
        const static int anAttachmentMap[10] =
            { -1, 7, 8, 9, 4, 5, 6, 1, 2, 3 };

        osStyle +=
            CPLString().Printf(",p:%d", anAttachmentMap[nAttachmentPoint]);
    }

    if( nColor > 0 && nColor < 256 )
    {
        const unsigned char *pabyDXFColors = ACGetColorTable();
        osStyle +=
            CPLString().Printf( ",c:#%02x%02x%02x",
                                pabyDXFColors[nColor*3+0],
                                pabyDXFColors[nColor*3+1],
                                pabyDXFColors[nColor*3+2] );
    }

    osStyle += ")";

    poFeature->SetStyleString( osStyle );

    return poFeature;
}

/************************************************************************/
/*                           TranslateTEXT()                            */
/*                                                                      */
/*      This function translates TEXT and ATTRIB entities, as well as   */
/*      ATTDEF entities when we are not inlining blocks.                */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::TranslateTEXT()

{
    char szLineBuf[257];
    int nCode = 0;
    OGRDXFFeature *poFeature = new OGRDXFFeature( poFeatureDefn );

    double dfX = 0.0;
    double dfY = 0.0;
    double dfZ = 0.0;
    bool bHaveZ = false;

    double dfAngle = 0.0;
    double dfHeight = 0.0;
    double dfXDirection = 0.0;
    double dfYDirection = 0.0;

    CPLString osText;
    CPLString osStyleName = "Arial";

    int nAnchorPosition = 1;
    int nHorizontalAlignment = 0;
    int nVerticalAlignment = 0;

    bool bIsAttribOrAttdef = false;

    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfX = CPLAtof(szLineBuf);
            break;

          case 20:
            dfY = CPLAtof(szLineBuf);
            break;

          case 11:
            dfXDirection = CPLAtof(szLineBuf);
            break;

          case 21:
            dfYDirection = CPLAtof(szLineBuf);
            break;

          case 30:
            dfZ = CPLAtof(szLineBuf);
            bHaveZ = true;
            break;

          case 40:
            dfHeight = CPLAtof(szLineBuf);
            break;

          case 1:
            osText += TextUnescape(szLineBuf, false);
            break;

          case 50:
            dfAngle = CPLAtof(szLineBuf);
            break;

          case 72:
            nHorizontalAlignment = atoi(szLineBuf);
            break;

          case 73:
            if( !bIsAttribOrAttdef )
                nVerticalAlignment = atoi(szLineBuf);
            break;

          case 74:
            if( bIsAttribOrAttdef )
                nVerticalAlignment = atoi(szLineBuf);
            break;

          case 7:
            osStyleName = TextRecode(szLineBuf);
            break;
          
          case 100:
            if( STARTS_WITH_CI( szLineBuf, "AcDbAttribute" ) )
                bIsAttribOrAttdef = true;

            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;

          // 2 and 70 are for ATTRIB entities only
          case 2:
            if( bIsAttribOrAttdef )
            {
                if( strchr( szLineBuf, ' ' ) )
                {
                    CPLDebug( "DXF", "Attribute tags may not contain spaces" );
                    DXF_LAYER_READER_ERROR();
                    delete poFeature;
                    return NULL;
                }
                poFeature->osAttributeTag = szLineBuf;
            }
            break;

          case 70:
            // TODO when the LSB is set, this ATTRIB is "invisible"
            // and should be disregarded
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }
    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        delete poFeature;
        return NULL;
    }

    if( nCode == 0 )
        poDS->UnreadValue();

    OGRPoint* poGeom = NULL;
    if( bHaveZ )
        poGeom = new OGRPoint( dfX, dfY, dfZ );
    else
        poGeom = new OGRPoint( dfX, dfY );
    poFeature->ApplyOCSTransformer( poGeom );
    poFeature->SetGeometryDirectly( poGeom );

/* -------------------------------------------------------------------- */
/*      Determine anchor position.                                      */
/* -------------------------------------------------------------------- */
    if( nHorizontalAlignment > 0 || nVerticalAlignment > 0 )
    {
        switch( nVerticalAlignment )
        {
          case 1: // bottom
            nAnchorPosition = 10;
            break;

          case 2: // middle
            nAnchorPosition = 4;
            break;

          case 3: // top
            nAnchorPosition = 7;
            break;

          default:
            break;
        }
        if( nHorizontalAlignment < 3 )
            nAnchorPosition += nHorizontalAlignment;
        // TODO other alignment options
    }

/* -------------------------------------------------------------------- */
/*      Translate text from Win-1252 to UTF8.  We approximate this      */
/*      by treating Win-1252 as Latin-1.                                */
/* -------------------------------------------------------------------- */
    osText.Recode( poDS->GetEncoding(), CPL_ENC_UTF8 );

    poFeature->SetField( "Text", osText );

/* -------------------------------------------------------------------- */
/*      We need to escape double quotes with backslashes before they    */
/*      can be inserted in the style string.                            */
/* -------------------------------------------------------------------- */
    if( strchr( osText, '"' ) != NULL )
    {
        CPLString osEscaped;

        for( size_t iC = 0; iC < osText.size(); iC++ )
        {
            if( osText[iC] == '"' )
                osEscaped += "\\\"";
            else
                osEscaped += osText[iC];
        }
        osText = osEscaped;
    }

/* -------------------------------------------------------------------- */
/*      Is the layer disabled/hidden/frozen/off?                        */
/* -------------------------------------------------------------------- */
    CPLString osLayer = poFeature->GetFieldAsString("Layer");

    const char* pszHidden = poDS->LookupLayerProperty( osLayer, "Hidden" );
    const bool bHidden = pszHidden && EQUAL(pszHidden, "1");

/* -------------------------------------------------------------------- */
/*      Work out the color for this feature.                            */
/* -------------------------------------------------------------------- */
    int nColor = 256;

    if( poFeature->oStyleProperties.count("Color") > 0 )
        nColor = atoi(poFeature->oStyleProperties["Color"]);

    // Use layer color?
    if( nColor < 1 || nColor > 255 )
    {
        const char *pszValue = poDS->LookupLayerProperty( osLayer, "Color" );
        if( pszValue != NULL )
            nColor = atoi(pszValue);
    }

    if( nColor < 1 || nColor > 255 )
        nColor = 8;

/* -------------------------------------------------------------------- */
/*      Prepare style string.                                           */
/* -------------------------------------------------------------------- */
    CPLString osStyle;
    char szBuffer[64];

    osStyle.Printf("LABEL(f:\"%s\",t:\"%s\"", osStyleName.c_str(), osText.c_str());
    
    osStyle += CPLString().Printf(",p:%d", nAnchorPosition);

    if( dfAngle != 0.0 )
    {
        CPLsnprintf(szBuffer, sizeof(szBuffer), "%.3g", dfAngle);
        osStyle += CPLString().Printf(",a:%s", szBuffer);
    }

    if( dfHeight != 0.0 )
    {
        CPLsnprintf(szBuffer, sizeof(szBuffer), "%.3g", dfHeight);
        osStyle += CPLString().Printf(",s:%sg", szBuffer);
    }

    if( dfXDirection != 0.0 )
    {
        CPLsnprintf(szBuffer, sizeof(szBuffer), "%.6g", dfXDirection - dfX);
        osStyle += CPLString().Printf(",dx:%s", szBuffer);
    }

    if( dfYDirection != 0.0 )
    {
        CPLsnprintf(szBuffer, sizeof(szBuffer), "%.6g", dfYDirection - dfY);
        osStyle += CPLString().Printf(",dy:%s", szBuffer);
    }

    const unsigned char *pabyDWGColors = ACGetColorTable();

    snprintf( szBuffer, sizeof(szBuffer), ",c:#%02x%02x%02x",
              pabyDWGColors[nColor*3+0],
              pabyDWGColors[nColor*3+1],
              pabyDWGColors[nColor*3+2] );
    osStyle += szBuffer;

    if( bHidden )
        osStyle += "00";

    osStyle += ")";

    poFeature->SetStyleString( osStyle );

    return poFeature;
}

/************************************************************************/
/*                           TranslatePOINT()                           */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::TranslatePOINT()

{
    char szLineBuf[257];
    int nCode = 0;
    OGRDXFFeature *poFeature = new OGRDXFFeature( poFeatureDefn );
    double dfX = 0.0;
    double dfY = 0.0;
    double dfZ = 0.0;
    bool bHaveZ = false;

    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfX = CPLAtof(szLineBuf);
            break;

          case 20:
            dfY = CPLAtof(szLineBuf);
            break;

          case 30:
            dfZ = CPLAtof(szLineBuf);
            bHaveZ = true;
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }
    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        delete poFeature;
        return NULL;
    }

    if( nCode == 0 )
        poDS->UnreadValue();

    OGRPoint* poGeom = NULL;
    if( bHaveZ )
        poGeom = new OGRPoint( dfX, dfY, dfZ );
    else
        poGeom = new OGRPoint( dfX, dfY );

    poFeature->SetGeometryDirectly( poGeom );

    // Set style pen color
    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                           TranslateLINE()                            */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::TranslateLINE()

{
    char szLineBuf[257];
    int nCode = 0;
    OGRDXFFeature *poFeature = new OGRDXFFeature( poFeatureDefn );
    double dfX1 = 0.0;
    double dfY1 = 0.0;
    double dfZ1 = 0.0;
    double dfX2 = 0.0;
    double dfY2 = 0.0;
    double dfZ2 = 0.0;
    bool bHaveZ = false;

/* -------------------------------------------------------------------- */
/*      Process values.                                                 */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfX1 = CPLAtof(szLineBuf);
            break;

          case 11:
            dfX2 = CPLAtof(szLineBuf);
            break;

          case 20:
            dfY1 = CPLAtof(szLineBuf);
            break;

          case 21:
            dfY2 = CPLAtof(szLineBuf);
            break;

          case 30:
            dfZ1 = CPLAtof(szLineBuf);
            bHaveZ = true;
            break;

          case 31:
            dfZ2 = CPLAtof(szLineBuf);
            bHaveZ = true;
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }
    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        delete poFeature;
        return NULL;
    }

    if( nCode == 0 )
        poDS->UnreadValue();

/* -------------------------------------------------------------------- */
/*      Create geometry                                                 */
/* -------------------------------------------------------------------- */
    OGRLineString *poLS = new OGRLineString();
    if( bHaveZ )
    {
        poLS->addPoint( dfX1, dfY1, dfZ1 );
        poLS->addPoint( dfX2, dfY2, dfZ2 );
    }
    else
    {
        poLS->addPoint( dfX1, dfY1 );
        poLS->addPoint( dfX2, dfY2 );
    }

    poFeature->SetGeometryDirectly( poLS );

    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                         TranslateLWPOLYLINE()                        */
/************************************************************************/
OGRDXFFeature *OGRDXFLayer::TranslateLWPOLYLINE()

{
    // Collect vertices and attributes into a smooth polyline.
    // If there are no bulges, then we are a straight-line polyline.
    // Single-vertex polylines become points.
    // Group code 30 (vertex Z) is not part of this entity.
    char szLineBuf[257];
    int nCode = 0;
    int nPolylineFlag = 0;

    OGRDXFFeature *poFeature = new OGRDXFFeature( poFeatureDefn );
    double dfX = 0.0;
    double dfY = 0.0;
    double dfZ = 0.0;
    bool bHaveX = false;
    bool bHaveY = false;

    int nNumVertices = 1;   // use 1 based index
    int npolyarcVertexCount = 1;
    double dfBulge = 0.0;
    DXFSmoothPolyline smoothPolyline;

    smoothPolyline.setCoordinateDimension(2);

/* -------------------------------------------------------------------- */
/*      Collect information from the LWPOLYLINE object itself.          */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        if(npolyarcVertexCount > nNumVertices)
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Too many vertices found in LWPOLYLINE." );
            delete poFeature;
            return NULL;
        }

        switch( nCode )
        {
          case 38:
            // Constant elevation.
            dfZ = CPLAtof(szLineBuf);
            smoothPolyline.setCoordinateDimension(3);
            break;

          case 90:
            nNumVertices = atoi(szLineBuf);
            break;

          case 70:
            nPolylineFlag = atoi(szLineBuf);
            break;

          case 10:
            if( bHaveX && bHaveY )
            {
                smoothPolyline.AddPoint(dfX, dfY, dfZ, dfBulge);
                npolyarcVertexCount++;
                dfBulge = 0.0;
                bHaveY = false;
            }
            dfX = CPLAtof(szLineBuf);
            bHaveX = true;
            break;

          case 20:
            if( bHaveX && bHaveY )
            {
                smoothPolyline.AddPoint( dfX, dfY, dfZ, dfBulge );
                npolyarcVertexCount++;
                dfBulge = 0.0;
                bHaveX = false;
            }
            dfY = CPLAtof(szLineBuf);
            bHaveY = true;
            break;

          case 42:
            dfBulge = CPLAtof(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }
    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        delete poFeature;
        return NULL;
    }

    if( nCode == 0 )
        poDS->UnreadValue();

    if( bHaveX && bHaveY )
        smoothPolyline.AddPoint(dfX, dfY, dfZ, dfBulge);

    if(smoothPolyline.IsEmpty())
    {
        delete poFeature;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Close polyline if necessary.                                    */
/* -------------------------------------------------------------------- */
    if(nPolylineFlag & 0x01)
        smoothPolyline.Close();

    OGRGeometry* poGeom = smoothPolyline.Tesselate();
    poFeature->ApplyOCSTransformer( poGeom );
    poFeature->SetGeometryDirectly( poGeom );

    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                         TranslatePOLYLINE()                          */
/*                                                                      */
/*      We also capture the following VERTEXes.                         */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::TranslatePOLYLINE()

{
    char szLineBuf[257];
    int nCode = 0;
    int nPolylineFlag = 0;
    OGRDXFFeature *poFeature = new OGRDXFFeature( poFeatureDefn );

/* -------------------------------------------------------------------- */
/*      Collect information from the POLYLINE object itself.            */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 70:
            nPolylineFlag = atoi(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }
    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        delete poFeature;
        return NULL;
    }

    if( (nPolylineFlag & 16) != 0 )
    {
        CPLDebug( "DXF", "Polygon mesh not supported." );
        delete poFeature;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Collect VERTEXes as a smooth polyline.                          */
/* -------------------------------------------------------------------- */
    double dfX = 0.0;
    double dfY = 0.0;
    double dfZ = 0.0;
    double dfBulge = 0.0;
    int nVertexFlag = 0;
    DXFSmoothPolyline   smoothPolyline;
    int                 vertexIndex71 = 0;
    int                 vertexIndex72 = 0;
    int                 vertexIndex73 = 0;
    int                 vertexIndex74 = 0;
    OGRPoint **papoPoints = NULL;
    int nPoints = 0;
    OGRPolyhedralSurface *poPS = new OGRPolyhedralSurface();

    smoothPolyline.setCoordinateDimension(2);

    while( nCode == 0 && !EQUAL(szLineBuf,"SEQEND") )
    {
        // Eat non-vertex objects.
        if( !EQUAL(szLineBuf,"VERTEX") )
        {
            while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf)))>0 ) {}
            if( nCode < 0 )
            {
                DXF_LAYER_READER_ERROR();
                delete poFeature;
                delete poPS;
                // delete the list of points
                for (int i = 0; i < nPoints; i++)
                    delete papoPoints[i];
                CPLFree(papoPoints);

                return NULL;
            }

            continue;
        }

        // process a Vertex
        while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
        {
            switch( nCode )
            {
              case 10:
                dfX = CPLAtof(szLineBuf);
                break;

              case 20:
                dfY = CPLAtof(szLineBuf);
                break;

              case 30:
                dfZ = CPLAtof(szLineBuf);
                smoothPolyline.setCoordinateDimension(3);
                break;

              case 42:
                dfBulge = CPLAtof(szLineBuf);
                break;

              case 70:
                nVertexFlag = atoi(szLineBuf);
                break;

              case 71:
                vertexIndex71 = atoi(szLineBuf);
                break;

              case 72:
                vertexIndex72 = atoi(szLineBuf);
                break;

              case 73:
                vertexIndex73 = atoi(szLineBuf);
                break;

              case 74:
                vertexIndex74 = atoi(szLineBuf);
                break;

              default:
                break;
            }
        }

        if (((nVertexFlag & 64) != 0) && ((nVertexFlag & 128) != 0))
        {
            // add the point to the list of points
            OGRPoint *poPoint = new OGRPoint(dfX, dfY, dfZ);
            OGRPoint** papoNewPoints = (OGRPoint **) VSI_REALLOC_VERBOSE( papoPoints,
                                                     sizeof(void*) * (nPoints+1) );

            papoPoints = papoNewPoints;
            papoPoints[nPoints] = poPoint;
            nPoints++;
        }

        // Note - If any index out of vertexIndex71, vertexIndex72, vertexIndex73 or vertexIndex74
        // is negative, it means that the line starting from that vertex is invisible

        if (nVertexFlag == 128 && papoPoints != NULL)
        {
            // create a polygon and add it to the Polyhedral Surface
            OGRLinearRing *poLR = new OGRLinearRing();
            int iPoint = 0;
            int startPoint = -1;
            poLR->set3D(TRUE);
            if (vertexIndex71 > 0 && vertexIndex71 <= nPoints)
            {
                if (startPoint == -1)
                    startPoint = vertexIndex71-1;
                poLR->setPoint(iPoint,papoPoints[vertexIndex71-1]);
                iPoint++;
                vertexIndex71 = 0;
            }
            if (vertexIndex72 > 0 && vertexIndex72 <= nPoints)
            {
                if (startPoint == -1)
                    startPoint = vertexIndex72-1;
                poLR->setPoint(iPoint,papoPoints[vertexIndex72-1]);
                iPoint++;
                vertexIndex72 = 0;
            }
            if (vertexIndex73 > 0 && vertexIndex73 <= nPoints)
            {
                if (startPoint == -1)
                    startPoint = vertexIndex73-1;
                poLR->setPoint(iPoint,papoPoints[vertexIndex73-1]);
                iPoint++;
                vertexIndex73 = 0;
            }
            if (vertexIndex74 > 0 && vertexIndex74 <= nPoints)
            {
                if (startPoint == -1)
                    startPoint = vertexIndex74-1;
                poLR->setPoint(iPoint,papoPoints[vertexIndex74-1]);
                iPoint++;
                vertexIndex74 = 0;
            }
            if( startPoint >= 0 )
            {
                // complete the ring
                poLR->setPoint(iPoint,papoPoints[startPoint]);

                OGRPolygon *poPolygon = new OGRPolygon();
                poPolygon->addRing((OGRCurve *)poLR);

                poPS->addGeometryDirectly(poPolygon);
            }

            // delete the ring to prevent leakage
            delete poLR;
        }

        if( nCode < 0 )
        {
            DXF_LAYER_READER_ERROR();
            delete poFeature;
            delete poPS;
            // delete the list of points
            for (int i = 0; i < nPoints; i++)
                delete papoPoints[i];
            CPLFree(papoPoints);
            return NULL;
        }

        // Ignore Spline frame control points ( see #4683 )
        if ((nVertexFlag & 16) == 0)
            smoothPolyline.AddPoint( dfX, dfY, dfZ, dfBulge );
        dfBulge = 0.0;
    }

    // delete the list of points
    for (int i = 0; i < nPoints; i++)
        delete papoPoints[i];
    CPLFree(papoPoints);

    if(smoothPolyline.IsEmpty())
    {
        delete poFeature;
        delete poPS;
        return NULL;
    }

    if (poPS->getNumGeometries() > 0)
    {
        poFeature->SetGeometryDirectly((OGRGeometry *)poPS);
        return poFeature;
    }

    else
        delete poPS;

    /* -------------------------------------------------------------------- */
    /*      Close polyline if necessary.                                    */
    /* -------------------------------------------------------------------- */
    if(nPolylineFlag & 0x01)
        smoothPolyline.Close();

    OGRGeometry* poGeom = smoothPolyline.Tesselate();

    if( (nPolylineFlag & 8) == 0 )
        poFeature->ApplyOCSTransformer( poGeom );
    poFeature->SetGeometryDirectly( poGeom );

    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                          TranslateCIRCLE()                           */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::TranslateCIRCLE()

{
    char szLineBuf[257];
    int nCode = 0;
    OGRDXFFeature *poFeature = new OGRDXFFeature( poFeatureDefn );
    double dfX1 = 0.0;
    double dfY1 = 0.0;
    double dfZ1 = 0.0;
    double dfRadius = 0.0;
    bool bHaveZ = false;

/* -------------------------------------------------------------------- */
/*      Process values.                                                 */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfX1 = CPLAtof(szLineBuf);
            break;

          case 20:
            dfY1 = CPLAtof(szLineBuf);
            break;

          case 30:
            dfZ1 = CPLAtof(szLineBuf);
            bHaveZ = true;
            break;

          case 40:
            dfRadius = CPLAtof(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }
    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        delete poFeature;
        return NULL;
    }

    if( nCode == 0 )
        poDS->UnreadValue();

/* -------------------------------------------------------------------- */
/*      Create geometry                                                 */
/* -------------------------------------------------------------------- */
    OGRGeometry *poCircle =
        OGRGeometryFactory::approximateArcAngles( dfX1, dfY1, dfZ1,
                                                  dfRadius, dfRadius, 0.0,
                                                  0.0, 360.0,
                                                  0.0 );

    if( !bHaveZ )
        poCircle->flattenTo2D();

    poFeature->ApplyOCSTransformer( poCircle );
    poFeature->SetGeometryDirectly( poCircle );
    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                          TranslateELLIPSE()                          */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::TranslateELLIPSE()

{
    char szLineBuf[257];
    int nCode = 0;
    OGRDXFFeature *poFeature = new OGRDXFFeature( poFeatureDefn );
    double dfX1 = 0.0;
    double dfY1 = 0.0;
    double dfZ1 = 0.0;
    double dfRatio = 0.0;
    double dfStartAngle = 0.0;
    double dfEndAngle = 360.0;
    double dfAxisX = 0.0;
    double dfAxisY = 0.0;
    double dfAxisZ=0.0;
    bool bHaveZ = false;
    bool bApplyOCSTransform = false;

/* -------------------------------------------------------------------- */
/*      Process values.                                                 */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfX1 = CPLAtof(szLineBuf);
            break;

          case 20:
            dfY1 = CPLAtof(szLineBuf);
            break;

          case 30:
            dfZ1 = CPLAtof(szLineBuf);
            bHaveZ = true;
            break;

          case 11:
            dfAxisX = CPLAtof(szLineBuf);
            break;

          case 21:
            dfAxisY = CPLAtof(szLineBuf);
            break;

          case 31:
            dfAxisZ = CPLAtof(szLineBuf);
            break;

          case 40:
            dfRatio = CPLAtof(szLineBuf);
            break;

          case 41:
            // These *seem* to always be in radians regardless of $AUNITS
            dfEndAngle = -1 * CPLAtof(szLineBuf) * 180.0 / M_PI;
            break;

          case 42:
            // These *seem* to always be in radians regardless of $AUNITS
            dfStartAngle = -1 * CPLAtof(szLineBuf) * 180.0 / M_PI;
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }
    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        delete poFeature;
        return NULL;
    }

    if( nCode == 0 )
        poDS->UnreadValue();

/* -------------------------------------------------------------------- */
/*      Setup coordinate system                                         */
/* -------------------------------------------------------------------- */
    double adfN[3];
    poFeature->oOCS.ToArray( adfN );

    if( (adfN[0] == 0.0 && adfN[1] == 0.0 && adfN[2] == 1.0) == false )
    {
        OCSTransformer oTransformer( adfN, true );

        bApplyOCSTransform = true;

        double *x = &dfX1;
        double *y = &dfY1;
        double *z = &dfZ1;
        oTransformer.InverseTransform( 1, x, y, z );

        x = &dfAxisX;
        y = &dfAxisY;
        z = &dfAxisZ;
        oTransformer.InverseTransform( 1, x, y, z );
    }

/* -------------------------------------------------------------------- */
/*      Compute primary and secondary axis lengths, and the angle of    */
/*      rotation for the ellipse.                                       */
/* -------------------------------------------------------------------- */
    double dfPrimaryRadius = sqrt( dfAxisX * dfAxisX
                            + dfAxisY * dfAxisY
                            + dfAxisZ * dfAxisZ );

    double dfSecondaryRadius = dfRatio * dfPrimaryRadius;

    double dfRotation = -1 * atan2( dfAxisY, dfAxisX ) * 180 / M_PI;

/* -------------------------------------------------------------------- */
/*      Create geometry                                                 */
/* -------------------------------------------------------------------- */
    if( dfStartAngle > dfEndAngle )
        dfEndAngle += 360.0;

    if( fabs(dfEndAngle - dfStartAngle) <= 361.0 )
    {
        OGRGeometry *poEllipse =
            OGRGeometryFactory::approximateArcAngles( dfX1, dfY1, dfZ1,
                                                    dfPrimaryRadius,
                                                    dfSecondaryRadius,
                                                    dfRotation,
                                                    dfStartAngle, dfEndAngle,
                                                    0.0 );

        if( !bHaveZ )
            poEllipse->flattenTo2D();

        if( bApplyOCSTransform == true )
            poFeature->ApplyOCSTransformer( poEllipse );
        poFeature->SetGeometryDirectly( poEllipse );
    }
    else
    {
        // TODO: emit error ?
    }

    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                            TranslateARC()                            */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::TranslateARC()

{
    char szLineBuf[257];
    int nCode = 0;
    OGRDXFFeature *poFeature = new OGRDXFFeature( poFeatureDefn );
    double dfX1 = 0.0;
    double dfY1 = 0.0;
    double dfZ1 = 0.0;
    double dfRadius = 0.0;
    double dfStartAngle = 0.0;
    double dfEndAngle = 360.0;
    bool bHaveZ = false;

/* -------------------------------------------------------------------- */
/*      Process values.                                                 */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfX1 = CPLAtof(szLineBuf);
            break;

          case 20:
            dfY1 = CPLAtof(szLineBuf);
            break;

          case 30:
            dfZ1 = CPLAtof(szLineBuf);
            bHaveZ = true;
            break;

          case 40:
            dfRadius = CPLAtof(szLineBuf);
            break;

          case 50:
            // This is apparently always degrees regardless of AUNITS
            dfEndAngle = -1 * CPLAtof(szLineBuf);
            break;

          case 51:
            // This is apparently always degrees regardless of AUNITS
            dfStartAngle = -1 * CPLAtof(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }
    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        delete poFeature;
        return NULL;
    }

    if( nCode == 0 )
        poDS->UnreadValue();

/* -------------------------------------------------------------------- */
/*      Create geometry                                                 */
/* -------------------------------------------------------------------- */
    if( dfStartAngle > dfEndAngle )
        dfEndAngle += 360.0;

    if( fabs(dfEndAngle - dfStartAngle) <= 361.0 )
    {
        OGRGeometry *poArc =
            OGRGeometryFactory::approximateArcAngles( dfX1, dfY1, dfZ1,
                                                    dfRadius, dfRadius, 0.0,
                                                    dfStartAngle, dfEndAngle,
                                                    0.0 );
        if( !bHaveZ )
            poArc->flattenTo2D();

        poFeature->ApplyOCSTransformer( poArc );
        poFeature->SetGeometryDirectly( poArc );
    }
    else
    {
        // TODO: emit error ?
    }

    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                          TranslateSPLINE()                           */
/************************************************************************/

void rbspline2(int npts,int k,int p1,double b[],double h[],
        bool bCalculateKnots, double knots[], double p[]);

OGRDXFFeature *OGRDXFLayer::TranslateSPLINE()

{
    char szLineBuf[257];
    int nCode;
    int nDegree = -1;
    int nOrder = -1;
    int i;
    int nControlPoints = -1;
    int nKnots = -1;
    bool bResult = false;
    bool bCalculateKnots = false;
    OGRDXFFeature *poFeature = new OGRDXFFeature( poFeatureDefn );
    std::vector<double> adfControlPoints;
    std::vector<double> adfKnots;
    std::vector<double> adfWeights;

    adfControlPoints.push_back(0.0);
    adfKnots.push_back(0.0);
    adfWeights.push_back(0.0);

/* -------------------------------------------------------------------- */
/*      Process values.                                                 */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            adfControlPoints.push_back( CPLAtof(szLineBuf) );
            break;

          case 20:
            adfControlPoints.push_back( CPLAtof(szLineBuf) );
            adfControlPoints.push_back( 0.0 );
            break;

          case 40:
            adfKnots.push_back( CPLAtof(szLineBuf) );
            break;

          case 41:
            adfWeights.push_back( CPLAtof(szLineBuf) );
            break;

          case 70:
            break;

          case 71:
            nDegree = atoi(szLineBuf);
            // Arbitrary threshold
            if( nDegree < 0 || nDegree > 100)
            {
                DXF_LAYER_READER_ERROR();
                delete poFeature;
                return NULL;
            }
            break;

          case 72:
            nKnots = atoi(szLineBuf);
            // Arbitrary threshold
            if( nKnots < 0 || nKnots > 10000000)
            {
                DXF_LAYER_READER_ERROR();
                delete poFeature;
                return NULL;
            }
            break;

          case 73:
            nControlPoints = atoi(szLineBuf);
            // Arbitrary threshold
            if( nControlPoints < 0 || nControlPoints > 10000000)
            {
                DXF_LAYER_READER_ERROR();
                delete poFeature;
                return NULL;
            }
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }
    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        delete poFeature;
        return NULL;
    }

    if( nCode == 0 )
        poDS->UnreadValue();

/* -------------------------------------------------------------------- */
/*      Sanity checks                                                   */
/* -------------------------------------------------------------------- */
    nOrder = nDegree + 1;

    bResult = ( nOrder >= 2 );
    if( bResult == true )
    {
        // Check whether nctrlpts value matches number of vertices read
        int nCheck = (static_cast<int>(adfControlPoints.size()) - 1) / 3;

        if( nControlPoints == -1 )
            nControlPoints =
                (static_cast<int>(adfControlPoints.size()) - 1) / 3;

        // min( num(ctrlpts) ) = order
        bResult = ( nControlPoints >= nOrder && nControlPoints == nCheck);
    }

    if( bResult == true )
    {
        int nCheck = static_cast<int>(adfKnots.size()) - 1;

        // Recalculate knots when:
        // - no knots data present, nknots is -1 and ncheck is 0
        // - nknots value present, no knot vertices
        //   nknots is (nctrlpts + order), ncheck is 0
        if( nCheck == 0 )
        {
            bCalculateKnots = true;
            for( i = 0; i < (nControlPoints + nOrder); i++ )
                adfKnots.push_back( 0.0 );

            nCheck = static_cast<int>(adfKnots.size()) - 1;
        }
        // Adjust nknots value when:
        // - nknots value not present, knot vertices present
        //   nknots is -1, ncheck is (nctrlpts + order)
        if( nKnots == -1 )
            nKnots = static_cast<int>(adfKnots.size()) - 1;

        // num(knots) = num(ctrlpts) + order
        bResult = ( nKnots == (nControlPoints + nOrder) && nKnots == nCheck );
    }

    if( bResult == true )
    {
        int nWeights = static_cast<int>(adfWeights.size()) - 1;

        if( nWeights == 0 )
        {
            for( i = 0; i < nControlPoints; i++ )
                adfWeights.push_back( 1.0 );

            nWeights = static_cast<int>(adfWeights.size()) - 1;
        }

        // num(weights) = num(ctrlpts)
        bResult = ( nWeights == nControlPoints );
    }

    if( bResult == false )
    {
        DXF_LAYER_READER_ERROR();
        delete poFeature;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Interpolate spline                                              */
/* -------------------------------------------------------------------- */
    int p1 = nControlPoints * 8;
    std::vector<double> p;

    p.push_back( 0.0 );
    for( i = 0; i < 3*p1; i++ )
        p.push_back( 0.0 );

    rbspline2( nControlPoints, nOrder, p1, &(adfControlPoints[0]),
            &(adfWeights[0]), bCalculateKnots, &(adfKnots[0]), &(p[0]) );

/* -------------------------------------------------------------------- */
/*      Turn into OGR geometry.                                         */
/* -------------------------------------------------------------------- */
    OGRLineString *poLS = new OGRLineString();

    poLS->setNumPoints( p1 );
    for( i = 0; i < p1; i++ )
        poLS->setPoint( i, p[i*3+1], p[i*3+2] );

    poFeature->SetGeometryDirectly( poLS );

    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                          Translate3DFACE()                           */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::Translate3DFACE()

{
    char szLineBuf[257];
    int nCode = 0;
    OGRDXFFeature *poFeature = new OGRDXFFeature( poFeatureDefn );
    double dfX1 = 0.0;
    double dfY1 = 0.0;
    double dfZ1 = 0.0;
    double dfX2 = 0.0;
    double dfY2 = 0.0;
    double dfZ2 = 0.0;
    double dfX3 = 0.0;
    double dfY3 = 0.0;
    double dfZ3 = 0.0;
    double dfX4 = 0.0;
    double dfY4 = 0.0;
    double dfZ4 = 0.0;

/* -------------------------------------------------------------------- */
/*      Process values.                                                 */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfX1 = CPLAtof(szLineBuf);
            break;

          case 11:
            dfX2 = CPLAtof(szLineBuf);
            break;

          case 12:
            dfX3 = CPLAtof(szLineBuf);
            break;

          case 13:
            dfX4 = CPLAtof(szLineBuf);
            break;

          case 20:
            dfY1 = CPLAtof(szLineBuf);
            break;

          case 21:
            dfY2 = CPLAtof(szLineBuf);
            break;

          case 22:
            dfY3 = CPLAtof(szLineBuf);
            break;

          case 23:
            dfY4 = CPLAtof(szLineBuf);
            break;

          case 30:
            dfZ1 = CPLAtof(szLineBuf);
            break;

          case 31:
            dfZ2 = CPLAtof(szLineBuf);
            break;

          case 32:
            dfZ3 = CPLAtof(szLineBuf);
            break;

          case 33:
            dfZ4 = CPLAtof(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }
    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        delete poFeature;
        return NULL;
    }

    if( nCode == 0 )
        poDS->UnreadValue();

/* -------------------------------------------------------------------- */
/*      Create geometry                                                 */
/* -------------------------------------------------------------------- */
    OGRPolygon *poPoly = new OGRPolygon();
    OGRLinearRing* poLR = new OGRLinearRing();
    poLR->addPoint( dfX1, dfY1, dfZ1 );
    poLR->addPoint( dfX2, dfY2, dfZ2 );
    poLR->addPoint( dfX3, dfY3, dfZ3 );
    if( dfX4 != dfX3 || dfY4 != dfY3 || dfZ4 != dfZ3 )
        poLR->addPoint( dfX4, dfY4, dfZ4 );
    poPoly->addRingDirectly(poLR);
    poPoly->closeRings();

    poFeature->ApplyOCSTransformer( poLR );
    poFeature->SetGeometryDirectly( poPoly );

    /* PrepareLineStyle( poFeature ); */

    return poFeature;
}

/* -------------------------------------------------------------------- */
/*      PointXAxisComparer                                              */
/*                                                                      */
/*      Returns true if oP1 is to the left of oP2, or they have the     */
/*      same x-coordinate and oP1 is below oP2.                         */
/* -------------------------------------------------------------------- */

static bool PointXAxisComparer(const OGRPoint& oP1, const OGRPoint& oP2)
{
    return oP1.getX() == oP2.getX() ?
        oP1.getY() < oP2.getY() :
        oP1.getX() < oP2.getX();
}

/* -------------------------------------------------------------------- */
/*      PointXYZEqualityComparer                                        */
/*                                                                      */
/*      Returns true if oP1 is equal to oP2 in the X, Y and Z axes.     */
/* -------------------------------------------------------------------- */

static bool PointXYZEqualityComparer(const OGRPoint& oP1, const OGRPoint& oP2)
{
    return oP1.getX() == oP2.getX() && oP1.getY() == oP2.getY() &&
        oP1.getZ() == oP2.getZ();
}

/************************************************************************/
/*                           TranslateSOLID()                           */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::TranslateSOLID()

{
    CPLDebug("SOLID", "translating solid");
    char szLineBuf[257];
    int nCode = 0;
    OGRDXFFeature *poFeature = new OGRDXFFeature(poFeatureDefn);
    double dfX1 = 0.0;
    double dfY1 = 0.0;
    double dfZ1 = 0.0;
    double dfX2 = 0.0;
    double dfY2 = 0.0;
    double dfZ2 = 0.0;
    double dfX3 = 0.0;
    double dfY3 = 0.0;
    double dfZ3 = 0.0;
    double dfX4 = 0.0;
    double dfY4 = 0.0;
    double dfZ4 = 0.0;

    while ((nCode = poDS->ReadValue(szLineBuf, sizeof(szLineBuf))) > 0) {
        switch (nCode) {
        case 10:
            dfX1 = CPLAtof(szLineBuf);
            break;

        case 20:
            dfY1 = CPLAtof(szLineBuf);
            break;

        case 30:
            dfZ1 = CPLAtof(szLineBuf);
            break;

        case 11:
            dfX2 = CPLAtof(szLineBuf);
            break;

        case 21:
            dfY2 = CPLAtof(szLineBuf);
            break;

        case 31:
            dfZ2 = CPLAtof(szLineBuf);
            break;

        case 12:
            dfX3 = CPLAtof(szLineBuf);
            break;

        case 22:
            dfY3 = CPLAtof(szLineBuf);
            break;

        case 32:
            dfZ3 = CPLAtof(szLineBuf);
            break;

        case 13:
            dfX4 = CPLAtof(szLineBuf);
            break;

        case 23:
            dfY4 = CPLAtof(szLineBuf);
            break;

        case 33:
            dfZ4 = CPLAtof(szLineBuf);
            break;

        default:
            TranslateGenericProperty(poFeature, nCode, szLineBuf);
            break;
        }
    }
    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        delete poFeature;
        return NULL;
    }

    // do we want Z-coordinates?
    const bool bWantZ = dfZ1 != 0.0 || dfZ2 != 0.0 ||
                        dfZ3 != 0.0 || dfZ4 != 0.0;

    // check how many unique corners we have
    OGRPoint* poCorners = new OGRPoint[4];
    poCorners[0].setX(dfX1);
    poCorners[0].setY(dfY1);
    if( bWantZ )
        poCorners[0].setZ(dfZ1);
    poCorners[1].setX(dfX2);
    poCorners[1].setY(dfY2);
    if( bWantZ )
        poCorners[1].setZ(dfZ2);
    poCorners[2].setX(dfX3);
    poCorners[2].setY(dfY3);
    if( bWantZ )
        poCorners[2].setZ(dfZ3);
    poCorners[3].setX(dfX4);
    poCorners[3].setY(dfY4);
    if( bWantZ )
        poCorners[3].setZ(dfZ4);

    std::sort(poCorners, poCorners + 4, PointXAxisComparer);
    int nCornerCount = static_cast<int>(std::unique(poCorners, poCorners + 4,
        PointXYZEqualityComparer) - poCorners);
    if( nCornerCount < 1 )
    {
        DXF_LAYER_READER_ERROR();
        delete poFeature;
        delete[] poCorners;
        return NULL;
    }

    OGRGeometry* poFinalGeom;

    // what kind of object do we need?
    if( nCornerCount == 1 )
    {
        poFinalGeom = poCorners[0].clone();
    }
    else if( nCornerCount == 2 )
    {
        OGRLineString* poLS = new OGRLineString();
        poLS->setPoint( 0, &poCorners[0] );
        poLS->setPoint( 1, &poCorners[1] );
        poFinalGeom = poLS;
    }
    else
    {
        // SOLID vertices seem to be joined in the order 1-2-4-3-1.
        // See trac ticket #7089
        OGRLinearRing* poLinearRing = new OGRLinearRing();
        int iIndex = 0;
        poLinearRing->setPoint( iIndex++, dfX1, dfY1, dfZ1 );
        if( dfX1 != dfX2 || dfY1 != dfY2 || dfZ1 != dfZ2 )
            poLinearRing->setPoint( iIndex++, dfX2, dfY2, dfZ2 );
        if( dfX2 != dfX4 || dfY2 != dfY4 || dfZ2 != dfZ4 )
            poLinearRing->setPoint( iIndex++, dfX4, dfY4, dfZ4 );
        if( dfX4 != dfX3 || dfY4 != dfY3 || dfZ4 != dfZ3 )
            poLinearRing->setPoint( iIndex++, dfX3, dfY3, dfZ3 );
        poLinearRing->closeRings();

        if( !bWantZ )
            poLinearRing->flattenTo2D();

        OGRPolygon* poPoly = new OGRPolygon();
        poPoly->addRingDirectly( poLinearRing );
        poFinalGeom = poPoly;
    }

    delete[] poCorners;

    poFeature->ApplyOCSTransformer( poFinalGeom );

    poFeature->SetGeometryDirectly( poFinalGeom );

    if (nCode == 0)
        poDS->UnreadValue();

    // Set style pen color
    PrepareLineStyle(poFeature);

    return poFeature;
}

/************************************************************************/
/*                       SimplifyBlockGeometry()                        */
/************************************************************************/

OGRGeometry *OGRDXFLayer::SimplifyBlockGeometry(
    OGRGeometryCollection *poCollection )
{
/* -------------------------------------------------------------------- */
/*      If there is only one geometry in the collection, just return    */
/*      it.                                                             */
/* -------------------------------------------------------------------- */
    if( poCollection->getNumGeometries() == 1 )
    {
        OGRGeometry *poReturn = poCollection->getGeometryRef(0);
        poCollection->removeGeometry(0, FALSE);
        delete poCollection;
        return poReturn;
    }

/* -------------------------------------------------------------------- */
/*      Convert to polygon, multipolygon, multilinestring or multipoint */
/* -------------------------------------------------------------------- */

    OGRwkbGeometryType eType =
                wkbFlatten(poCollection->getGeometryRef(0)->getGeometryType());
    int i;
    for(i=1;i<poCollection->getNumGeometries();i++)
    {
        if (wkbFlatten(poCollection->getGeometryRef(i)->getGeometryType())
            != eType)
        {
            eType = wkbUnknown;
            break;
        }
    }
    if (eType == wkbPoint || eType == wkbLineString)
    {
        OGRGeometryCollection* poNewColl;
        if (eType == wkbPoint)
            poNewColl = new OGRMultiPoint();
        else
            poNewColl = new OGRMultiLineString();
        while(poCollection->getNumGeometries() > 0)
        {
            OGRGeometry *poGeom = poCollection->getGeometryRef(0);
            poCollection->removeGeometry(0,FALSE);
            poNewColl->addGeometryDirectly(poGeom);
        }
        delete poCollection;
        return poNewColl;
    }
    else if (eType == wkbPolygon)
    {
        std::vector<OGRGeometry*> aosPolygons;
        while(poCollection->getNumGeometries() > 0)
        {
            OGRGeometry *poGeom = poCollection->getGeometryRef(0);
            poCollection->removeGeometry(0,FALSE);
            aosPolygons.push_back(poGeom);
        }
        delete poCollection;
        int bIsValidGeometry;
        return OGRGeometryFactory::organizePolygons(
            &aosPolygons[0], (int)aosPolygons.size(),
            &bIsValidGeometry, NULL);
    }

    return poCollection;
}

/************************************************************************/
/*                       InsertBlockReference()                         */
/*                                                                      */
/*     Returns a point geometry located at the block's insertion        */
/*     point.                                                           */
/************************************************************************/
OGRDXFFeature *OGRDXFLayer::InsertBlockReference(
    const CPLString& osBlockName,
    const OGRDXFInsertTransformer& oTransformer,
    OGRDXFFeature* const poFeature )
{
    // Store the block's properties in the special DXF-specific members
    // on the feature object
    poFeature->bIsBlockReference = true;
    poFeature->osBlockName = osBlockName;
    poFeature->dfBlockAngle = oTransformer.dfAngle * 180 / M_PI;
    poFeature->oBlockScale = DXFTriple( oTransformer.dfXScale, 
        oTransformer.dfYScale, oTransformer.dfZScale );
    poFeature->oOriginalCoords = DXFTriple( oTransformer.dfXOffset,
        oTransformer.dfYOffset, oTransformer.dfZOffset );

    // Only if DXF_INLINE_BLOCKS is false should we ever need to expose these
    // to the end user as fields.
    if( poFeature->GetFieldIndex( "BlockName" ) != -1 )
    {
        poFeature->SetField( "BlockName", poFeature->osBlockName );
        poFeature->SetField( "BlockAngle", poFeature->dfBlockAngle );
        poFeature->SetField( "BlockScale", 3, &(poFeature->oBlockScale.dfX) );
        poFeature->SetField( "BlockOCSNormal", 3, &(poFeature->oOCS.dfX) );
        poFeature->SetField( "BlockOCSCoords", 3,
            &(poFeature->oOriginalCoords.dfX) );
    }

    // For convenience to the end user, the point geometry will be located
    // at the WCS coordinates of the insertion point.
    OGRPoint* poInsertionPoint = new OGRPoint( oTransformer.dfXOffset,
        oTransformer.dfYOffset, oTransformer.dfZOffset );

    poFeature->ApplyOCSTransformer( poInsertionPoint );
    poFeature->SetGeometryDirectly( poInsertionPoint );

    return poFeature;
}

/************************************************************************/
/*                         InsertBlockInline()                          */
/*                                                                      */
/*     Inserts the given block at the location specified by the given   */
/*     transformer.  Returns poFeature, or NULL if all features on      */
/*     the block have been pushed to the extra feature queue.           */
/*     If poFeature is not returned, it is deleted.                     */
/*     Throws std::invalid_argument if the requested block              */
/*     doesn't exist.                                                   */
/*                                                                      */
/*     - poFeature: The feature to use as a template. This feature's    */
/*       OCS will be applied to the block.                              */
/*     - bInlineRecursively: If true, INSERTs within this block         */
/*       will be recursively inserted.  Otherwise, they will be         */
/*       represented as a point geometry using InsertBlockReference.    */
/*     - bMergeGeometry: If true, all features in the block,            */
/*       apart from text features, are merged into a                    */
/*       GeometryCollection which is returned by the function.          */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::InsertBlockInline( const CPLString& osBlockName,
    OGRDXFInsertTransformer oTransformer,
    OGRDXFFeature* const poFeature,
    std::queue<OGRDXFFeature *>& apoExtraFeatures,
    const bool bInlineRecursively,
    const bool bMergeGeometry )
{
/* -------------------------------------------------------------------- */
/*      Set up protection against excessive recursion on this layer.    */
/* -------------------------------------------------------------------- */
    if( !poDS->PushBlockInsertion( osBlockName ) )
    {
        delete poFeature;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Transform the insertion point from OCS into                     */
/*      world coordinates.                                              */
/* -------------------------------------------------------------------- */
    OGRPoint oInsertionPoint( oTransformer.dfXOffset, oTransformer.dfYOffset,
        oTransformer.dfZOffset );

    poFeature->ApplyOCSTransformer( &oInsertionPoint );

    oTransformer.dfXOffset = oInsertionPoint.getX();
    oTransformer.dfYOffset = oInsertionPoint.getY();
    oTransformer.dfZOffset = oInsertionPoint.getZ();

/* -------------------------------------------------------------------- */
/*      Lookup the block.                                               */
/* -------------------------------------------------------------------- */
    DXFBlockDefinition *poBlock = poDS->LookupBlock( osBlockName );

    if( poBlock == NULL )
    {
        //CPLDebug( "DXF", "Attempt to insert missing block %s", osBlockName );
        poDS->PopBlockInsertion();
        throw std::invalid_argument("osBlockName");
    }

/* -------------------------------------------------------------------- */
/*      If we have complete features associated with the block, push    */
/*      them on the pending feature stack copying over key override     */
/*      information.                                                    */
/*                                                                      */
/*      If bMergeGeometry is true, we merge the features                */
/*      (except text) into a single GeometryCollection.                 */
/* -------------------------------------------------------------------- */
    OGRGeometryCollection *poMergedGeometry = NULL;
    if( bMergeGeometry )
        poMergedGeometry = new OGRGeometryCollection();

    std::queue<OGRDXFFeature *> apoInnerExtraFeatures;

    for( unsigned int iSubFeat = 0;
        iSubFeat < poBlock->apoFeatures.size();
        iSubFeat++ )
    {
        OGRDXFFeature *poSubFeature =
            poBlock->apoFeatures[iSubFeat]->CloneDXFFeature();

        // Does this feature represent a block reference? If so,
        // insert that block
        if( bInlineRecursively && poSubFeature->IsBlockReference() )
        {
            // Unpack the transformation data stored in fields of this
            // feature
            OGRDXFInsertTransformer oInnerTransformer;
            oInnerTransformer.dfXOffset = poSubFeature->oOriginalCoords.dfX;
            oInnerTransformer.dfYOffset = poSubFeature->oOriginalCoords.dfY;
            oInnerTransformer.dfZOffset = poSubFeature->oOriginalCoords.dfZ;
            oInnerTransformer.dfAngle = poSubFeature->dfBlockAngle * M_PI / 180;
            oInnerTransformer.dfXScale = poSubFeature->oBlockScale.dfX;
            oInnerTransformer.dfYScale = poSubFeature->oBlockScale.dfY;
            oInnerTransformer.dfZScale = poSubFeature->oBlockScale.dfZ;

            poSubFeature->bIsBlockReference = false;

            // Insert this block recursively
            try
            {
                poSubFeature = InsertBlockInline( poSubFeature->osBlockName,
                    oInnerTransformer, poSubFeature, apoInnerExtraFeatures,
                    true, bMergeGeometry );
            }
            catch( const std::invalid_argument& )
            {
                // Block doesn't exist. Skip it and keep going
                delete poSubFeature;
                continue;
            }

            if( !poSubFeature )
            {
                if ( apoInnerExtraFeatures.empty() )
                {
                    // Block is empty. Skip it and keep going
                    continue;
                }
                else
                {
                    // Load up the first extra feature ready for
                    // transformation
                    poSubFeature = apoInnerExtraFeatures.front();
                    apoInnerExtraFeatures.pop();
                }
            }
        }

        // Go through the current feature and any extra features generated
        // by the recursive insert, and apply transformations
        while( true )
        {
            OGRGeometry *poSubFeatGeom = poSubFeature->GetGeometryRef();
            if( poSubFeatGeom != NULL )
            {
                // Rotation and scaling first
                OGRDXFInsertTransformer oInnerTrans =
                    oTransformer.GetRotateScaleTransformer();
                poSubFeatGeom->transform( &oInnerTrans );

                // Then the OCS to WCS transformation
                poFeature->ApplyOCSTransformer( poSubFeatGeom );

                // Offset translation last
                oInnerTrans = oTransformer.GetOffsetTransformer();
                poSubFeatGeom->transform( &oInnerTrans );
            }

            // If we are merging features, and this is not text or a block
            // reference, merge it into the GeometryCollection
            if( bMergeGeometry && 
                (poSubFeature->GetStyleString() == NULL ||
                    strstr(poSubFeature->GetStyleString(),"LABEL") == NULL) &&
                !poSubFeature->IsBlockReference() &&
                poSubFeature->GetGeometryRef() )
            {
                poMergedGeometry->addGeometryDirectly( poSubFeature->StealGeometry() );
                delete poSubFeature;
            }
            // Import all other features, except ATTDEFs when inlining
            // recursively
            else if( !bInlineRecursively || poSubFeature->osAttributeTag == "" )
            {
                // If the subfeature is on layer 0, this is a special case: the
                // subfeature should take on the style properties of the layer
                // the block is being inserted onto.
                // But don't do this if we are inserting onto a Blocks layer
                // (that is, the owning feature has no layer).
                if( EQUAL( poSubFeature->GetFieldAsString( "Layer" ), "0" ) &&
                    !EQUAL( poFeature->GetFieldAsString( "Layer" ), "" ) )
                {
                    poSubFeature->SetField( "Layer",
                        poFeature->GetFieldAsString( "Layer" ) );
                }

                // If the feature is something other than text, update the
                // style string to replace ByBlock and ByLayer values.
                const char* pszSubFeatureStyle = poSubFeature->GetStyleString();
                if( pszSubFeatureStyle == NULL ||
                    !STARTS_WITH_CI(pszSubFeatureStyle, "LABEL") )
                {
                    PrepareFeatureStyle( poSubFeature, poFeature );
                }
                // TODO Do this for text as well (trac ticket #7099)

                ACAdjustText( oTransformer.dfAngle * 180 / M_PI,
                    oTransformer.dfXScale, poSubFeature );

                if ( !EQUAL( poFeature->GetFieldAsString( "EntityHandle" ), "" ) )
                {
                    poSubFeature->SetField( "EntityHandle",
                        poFeature->GetFieldAsString( "EntityHandle" ) );
                }

                apoExtraFeatures.push( poSubFeature );
            }
            else
            {
                delete poSubFeature;
            }

            if( apoInnerExtraFeatures.empty() )
            {
                break;
            }
            else
            {
                poSubFeature = apoInnerExtraFeatures.front();
                apoInnerExtraFeatures.pop();
            }
        }
    }

    poDS->PopBlockInsertion();

/* -------------------------------------------------------------------- */
/*      Return the merged geometry if applicable.  Otherwise            */
/*      return NULL and let the machinery find the rest of the          */
/*      features in the pending feature stack.                          */
/* -------------------------------------------------------------------- */
    if( bMergeGeometry )
    {
        if( poMergedGeometry->getNumGeometries() == 0 )
        {
            delete poMergedGeometry;
        }
        else
        {
            poFeature->SetGeometryDirectly(
                SimplifyBlockGeometry( poMergedGeometry ) );

            PrepareLineStyle( poFeature );
            return poFeature;
        }
    }

    delete poFeature;
    return NULL;
}

/************************************************************************/
/*                          TranslateINSERT()                           */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::TranslateINSERT()

{
    char szLineBuf[257];
    int nCode = 0;

    OGRDXFFeature* poFeature = new OGRDXFFeature( poFeatureDefn );
    OGRDXFInsertTransformer oTransformer;
    CPLString osBlockName;

    bool bHasAttribs = false;
    std::queue<OGRDXFFeature *> apoAttribs;

/* -------------------------------------------------------------------- */
/*      Process values.                                                 */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            oTransformer.dfXOffset = CPLAtof(szLineBuf);
            break;

          case 20:
            oTransformer.dfYOffset = CPLAtof(szLineBuf);
            break;

          case 30:
            oTransformer.dfZOffset = CPLAtof(szLineBuf);
            break;

          case 41:
            oTransformer.dfXScale = CPLAtof(szLineBuf);
            break;

          case 42:
            oTransformer.dfYScale = CPLAtof(szLineBuf);
            break;

          case 43:
            oTransformer.dfZScale = CPLAtof(szLineBuf);
            break;

          case 50:
            // We want to transform this to radians.
            // It is apparently always in degrees regardless of $AUNITS
            oTransformer.dfAngle = CPLAtof(szLineBuf) * M_PI / 180.0;
            break;

          case 66:
            bHasAttribs = atoi(szLineBuf) == 1;
            break;

          case 2:
            osBlockName = szLineBuf;
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }
    if( nCode < 0 )
    {
        DXF_LAYER_READER_ERROR();
        delete poFeature;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Process any attribute entities.                                 */
/* -------------------------------------------------------------------- */

    if ( bHasAttribs )
    {
        while( nCode == 0 && !EQUAL( szLineBuf, "SEQEND" ) )
        {
            if( !EQUAL( szLineBuf, "ATTRIB" ) )
            {
                DXF_LAYER_READER_ERROR();

                delete poFeature;
                while( !apoAttribs.empty() )
                {
                    delete apoAttribs.front();
                    apoAttribs.pop();
                }

                return NULL;
            }

            OGRDXFFeature *poAttribFeature = TranslateTEXT();

            if( poAttribFeature && poAttribFeature->osAttributeTag != "" )
                apoAttribs.push( poAttribFeature );
            else
                delete poAttribFeature;

            nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf));
        }
    }
    else if( nCode == 0 )
        poDS->UnreadValue();

/* -------------------------------------------------------------------- */
/*      Perform the actual block insertion.                             */
/* -------------------------------------------------------------------- */

    // If we are not inlining blocks, just insert a point that refers
    // to this block
    if( !poDS->InlineBlocks() )
    {
        poFeature = InsertBlockReference( osBlockName, oTransformer,
            poFeature );

        if( bHasAttribs && 
            poFeature->GetFieldIndex( "BlockAttributes" ) != -1 )
        {
            // Store the attributes and their text values as space-separated
            // entries in the BlockAttributes field
            char** papszAttribs = new char*[apoAttribs.size() + 1];
            int iIndex = 0;

            while( !apoAttribs.empty() )
            {
                CPLString osAttribString = apoAttribs.front()->osAttributeTag;
                osAttribString += " ";
                osAttribString += apoAttribs.front()->GetFieldAsString( "Text" );

                papszAttribs[iIndex] = new char[osAttribString.length() + 1];
                CPLStrlcpy( papszAttribs[iIndex], osAttribString.c_str(),
                    osAttribString.length() + 1 );

                delete apoAttribs.front();
                apoAttribs.pop();
                iIndex++;
            }
            papszAttribs[iIndex] = NULL;

            poFeature->SetField( "BlockAttributes", papszAttribs );
        }
    }
    // Otherwise, try inlining the contents of this block
    else
    {
        try
        {
            poFeature = InsertBlockInline( osBlockName, oTransformer,
                poFeature, apoPendingFeatures,
                true, poDS->ShouldMergeBlockGeometries() );
        }
        catch( const std::invalid_argument& )
        {
            // Block doesn't exist
            delete poFeature;
            while( !apoAttribs.empty() )
            {
                delete apoAttribs.front();
                apoAttribs.pop();
            }
            return NULL;
        }

        // Append the attribute features to the pending feature stack
        while( !apoAttribs.empty() )
        {
            apoPendingFeatures.push( apoAttribs.front() );
            apoAttribs.pop();
        }
    }

    if( !poFeature )
    {
        // The block geometries were appended to apoPendingFeatures
        delete poFeature;
        return NULL;
    }
    return poFeature;
}

/************************************************************************/
/*                      GetNextUnfilteredFeature()                      */
/************************************************************************/

OGRDXFFeature *OGRDXFLayer::GetNextUnfilteredFeature()

{
    OGRDXFFeature *poFeature = NULL;

/* -------------------------------------------------------------------- */
/*      If we have pending features, return one of them.                */
/* -------------------------------------------------------------------- */
    if( !apoPendingFeatures.empty() )
    {
        poFeature = apoPendingFeatures.front();
        apoPendingFeatures.pop();

        poFeature->SetFID( iNextFID++ );
        return poFeature;
    }

/* -------------------------------------------------------------------- */
/*      Read the entity type.                                           */
/* -------------------------------------------------------------------- */
    char szLineBuf[257];

    while( poFeature == NULL )
    {
        // read ahead to an entity.
        int nCode = 0;
        while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 ) {}
        if( nCode < 0 )
        {
            DXF_LAYER_READER_ERROR();
            return NULL;
        }

        if( EQUAL(szLineBuf,"ENDSEC") )
        {
            //CPLDebug( "DXF", "Clean end of features at ENDSEC." );
            poDS->UnreadValue();
            return NULL;
        }

        if( EQUAL(szLineBuf,"ENDBLK") )
        {
            //CPLDebug( "DXF", "Clean end of block at ENDBLK." );
            poDS->UnreadValue();
            return NULL;
        }

/* -------------------------------------------------------------------- */
/*      Handle the entity.                                              */
/* -------------------------------------------------------------------- */
        if( EQUAL(szLineBuf,"POINT") )
        {
            poFeature = TranslatePOINT();
        }
        else if( EQUAL(szLineBuf,"MTEXT") )
        {
            poFeature = TranslateMTEXT();
        }
        else if( EQUAL(szLineBuf,"TEXT") ||
            EQUAL(szLineBuf,"ATTDEF") )
        {
            poFeature = TranslateTEXT();
        }
        else if( EQUAL(szLineBuf,"LINE") )
        {
            poFeature = TranslateLINE();
        }
        else if( EQUAL(szLineBuf,"POLYLINE") )
        {
            poFeature = TranslatePOLYLINE();
        }
        else if( EQUAL(szLineBuf,"LWPOLYLINE") )
        {
            poFeature = TranslateLWPOLYLINE();
        }
        else if( EQUAL(szLineBuf,"CIRCLE") )
        {
            poFeature = TranslateCIRCLE();
        }
        else if( EQUAL(szLineBuf,"ELLIPSE") )
        {
            poFeature = TranslateELLIPSE();
        }
        else if( EQUAL(szLineBuf,"ARC") )
        {
            poFeature = TranslateARC();
        }
        else if( EQUAL(szLineBuf,"SPLINE") )
        {
            poFeature = TranslateSPLINE();
        }
        else if( EQUAL(szLineBuf,"3DFACE") )
        {
            poFeature = Translate3DFACE();
        }
        else if( EQUAL(szLineBuf,"INSERT") )
        {
            poFeature = TranslateINSERT();
        }
        else if( EQUAL(szLineBuf,"DIMENSION") )
        {
            poFeature = TranslateDIMENSION();
        }
        else if( EQUAL(szLineBuf,"HATCH") )
        {
            poFeature = TranslateHATCH();
        }
        else if( EQUAL(szLineBuf,"SOLID") )
        {
            poFeature = TranslateSOLID();
        }
        else if( EQUAL(szLineBuf,"LEADER") )
        {
            poFeature = TranslateLEADER();
        }
        else if( EQUAL(szLineBuf,"MLEADER")
            || EQUAL(szLineBuf,"MULTILEADER") )
        {
            poFeature = TranslateMLEADER();
        }
        else
        {
            if( oIgnoredEntities.count(szLineBuf) == 0 )
            {
                oIgnoredEntities.insert( szLineBuf );
                CPLDebug( "DXF", "Ignoring one or more of entity '%s'.",
                            szLineBuf );
            }
        }

        // If there are no more features, but we do still have pending features
        // (for example, after an INSERT), return the first pending feature.
        if ( poFeature == NULL && !apoPendingFeatures.empty() )
        {
            poFeature = apoPendingFeatures.front();
            apoPendingFeatures.pop();

            poFeature->SetFID( iNextFID++ );
            return poFeature;
        }
    }

/* -------------------------------------------------------------------- */
/*      Set FID.                                                        */
/* -------------------------------------------------------------------- */
    poFeature->SetFID( iNextFID++ );
    m_nFeaturesRead++;

    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRDXFLayer::GetNextFeature()

{
    while( true )
    {
        OGRFeature *poFeature = GetNextUnfilteredFeature();

        if( poFeature == NULL )
            return NULL;

        if( (m_poFilterGeom == NULL
             || FilterGeometry( poFeature->GetGeometryRef() ) )
            && (m_poAttrQuery == NULL
                || m_poAttrQuery->Evaluate( poFeature ) ) )
        {
            return poFeature;
        }

        delete poFeature;
    }
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDXFLayer::TestCapability( const char * pszCap )

{
    return EQUAL(pszCap, OLCStringsAsUTF8);
}
