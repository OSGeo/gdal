/******************************************************************************
 * $Id$
 *
 * Project:  DXF Translator
 * Purpose:  Implements OGRDXFLayer class.
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
#include "ogrdxf_polyline_smooth.h"

CPL_CVSID("$Id$");

#ifndef PI
#define PI  3.14159265358979323846
#endif 

/************************************************************************/
/*                            OGRDXFLayer()                             */
/************************************************************************/

OGRDXFLayer::OGRDXFLayer( OGRDXFDataSource *poDS )

{
    this->poDS = poDS;

    iNextFID = 0;

    poFeatureDefn = new OGRFeatureDefn( "entities" );
    poFeatureDefn->Reference();

    poDS->AddStandardFields( poFeatureDefn );

    if( !poDS->InlineBlocks() )
    {
        OGRFieldDefn  oScaleField( "BlockScale", OFTRealList );
        poFeatureDefn->AddFieldDefn( &oScaleField );

        OGRFieldDefn  oBlockAngleField( "BlockAngle", OFTReal );
        poFeatureDefn->AddFieldDefn( &oBlockAngleField );
    }
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

void OGRDXFLayer::TranslateGenericProperty( OGRFeature *poFeature, 
                                            int nCode, char *pszValue )

{
    switch( nCode )
    {
      case 8: 
        poFeature->SetField( "Layer", TextUnescape(pszValue) );
        break;
            
      case 100: 
      {
          CPLString osSubClass = poFeature->GetFieldAsString("SubClasses");
          if( osSubClass.size() > 0 )
              osSubClass += ":";
          osSubClass += pszValue;
          poFeature->SetField( "SubClasses", osSubClass.c_str() );
      }
      break;

      case 62:
        oStyleProperties["Color"] = pszValue;
        break;

      case 6:
        poFeature->SetField( "Linetype", TextUnescape(pszValue) );
        break;

      case 370:
      case 39:
        oStyleProperties["LineWeight"] = pszValue;
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

          if( osAggregate.size() > 0 )
              osAggregate += " ";
          osAggregate += pszValue;
            
          poFeature->SetField( "ExtendedEntity", osAggregate );
      }
      break;

      // OCS vector.
      case 210:
        oStyleProperties["210_N.dX"] = pszValue;
        break;
        
      case 220:
        oStyleProperties["220_N.dY"] = pszValue;
        break;
        
      case 230:
        oStyleProperties["230_N.dZ"] = pszValue;
        break;


      default:
        break;
    }
}

/************************************************************************/
/*                          PrepareLineStyle()                          */
/************************************************************************/

void OGRDXFLayer::PrepareLineStyle( OGRFeature *poFeature )

{
    CPLString osLayer = poFeature->GetFieldAsString("Layer");

/* -------------------------------------------------------------------- */
/*      Is the layer disabled/hidden/frozen/off?                        */
/* -------------------------------------------------------------------- */
    int bHidden = 
        EQUAL(poDS->LookupLayerProperty( osLayer, "Hidden" ), "1");

/* -------------------------------------------------------------------- */
/*      Work out the color for this feature.                            */
/* -------------------------------------------------------------------- */
    int nColor = 256;

    if( oStyleProperties.count("Color") > 0 )
        nColor = atoi(oStyleProperties["Color"]);

    // Use layer color? 
    if( nColor < 1 || nColor > 255 )
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

    if( oStyleProperties.count("LineWeight") > 0 )
    {
        CPLString osWeight = oStyleProperties["LineWeight"];

        if( osWeight == "-1" )
            osWeight = poDS->LookupLayerProperty(osLayer,"LineWeight");

        dfWeight = CPLAtof(osWeight) / 100.0;
    }

/* -------------------------------------------------------------------- */
/*      Do we have a dash/dot line style?                               */
/* -------------------------------------------------------------------- */
    const char *pszPattern = poDS->LookupLineType(
        poFeature->GetFieldAsString("Linetype") );

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
        snprintf(szBuffer, sizeof(szBuffer), "%.2g", dfWeight);
        char* pszComma = strchr(szBuffer, ',');
        if (pszComma)
            *pszComma = '.';
        osStyle += CPLString().Printf( ",w:%sg", szBuffer );
    }

    if( pszPattern )
    {
        osStyle += ",p:\"";
        osStyle += pszPattern;
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
    
public:
    OCSTransformer( double adfN[3] ) {
        static const double dSmall = 1.0 / 64.0;
        static const double adfWZ[3] = {0, 0, 1};
        static const double adfWY[3] = {0, 1, 0};

        memcpy( this->adfN, adfN, sizeof(double)*3 );

    if ((ABS(adfN[0]) < dSmall) && (ABS(adfN[1]) < dSmall))
            CrossProduct(adfWY, adfN, adfAX);
    else
            CrossProduct(adfWZ, adfN, adfAX);

    Scale2Unit( adfAX );
    CrossProduct(adfN, adfAX, adfAY);
    Scale2Unit( adfAY );
    }

    void CrossProduct(const double *a, const double *b, double *vResult) {
        vResult[0] = a[1] * b[2] - a[2] * b[1];
        vResult[1] = a[2] * b[0] - a[0] * b[2];
        vResult[2] = a[0] * b[1] - a[1] * b[0];
    }

    void Scale2Unit(double* adfV) {
    double dfLen=sqrt(adfV[0]*adfV[0] + adfV[1]*adfV[1] + adfV[2]*adfV[2]);
    if (dfLen != 0)
    {
            adfV[0] /= dfLen;
            adfV[1] /= dfLen;
            adfV[2] /= dfLen;
    }
    }
    OGRSpatialReference *GetSourceCS() { return NULL; }
    OGRSpatialReference *GetTargetCS() { return NULL; }
    int Transform( int nCount, 
                   double *x, double *y, double *z )
        { return TransformEx( nCount, x, y, z, NULL ); }
    
    int TransformEx( int nCount, 
                     double *adfX, double *adfY, double *adfZ = NULL,
                     int *pabSuccess = NULL )
        {
            int i;
            for( i = 0; i < nCount; i++ )
            {
                double x = adfX[i], y = adfY[i], z = adfZ[i];
                
                adfX[i] = x * adfAX[0] + y * adfAY[0] + z * adfN[0];
                adfY[i] = x * adfAX[1] + y * adfAY[1] + z * adfN[1];
                adfZ[i] = x * adfAX[2] + y * adfAY[2] + z * adfN[2];

                if( pabSuccess )
                    pabSuccess[i] = TRUE;
            }
            return TRUE;
        }
};

/************************************************************************/
/*                        ApplyOCSTransformer()                         */
/*                                                                      */
/*      Apply a transformation from OCS to world coordinates if an      */
/*      OCS vector was found in the object.                             */
/************************************************************************/

void OGRDXFLayer::ApplyOCSTransformer( OGRGeometry *poGeometry )

{
    if( oStyleProperties.count("210_N.dX") == 0
        || oStyleProperties.count("220_N.dY") == 0
        || oStyleProperties.count("230_N.dZ") == 0 )
        return;

    if( poGeometry == NULL )
        return;

    double adfN[3];

    adfN[0] = CPLAtof(oStyleProperties["210_N.dX"]);
    adfN[1] = CPLAtof(oStyleProperties["220_N.dY"]);
    adfN[2] = CPLAtof(oStyleProperties["230_N.dZ"]);

    OCSTransformer oTransformer( adfN );

    poGeometry->transform( &oTransformer );
}

/************************************************************************/
/*                            TextUnescape()                            */
/*                                                                      */
/*      Unexcape DXF style escape sequences such as \P for newline      */
/*      and \~ for space, and do the recoding to UTF8.                  */
/************************************************************************/

CPLString OGRDXFLayer::TextUnescape( const char *pszInput )

{
    return ACTextUnescape( pszInput, poDS->GetEncoding() );
}

/************************************************************************/
/*                           TranslateMTEXT()                           */
/************************************************************************/

OGRFeature *OGRDXFLayer::TranslateMTEXT()

{
    char szLineBuf[257];
    int nCode;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    double dfX = 0.0, dfY = 0.0, dfZ = 0.0;
    double dfAngle = 0.0;
    double dfHeight = 0.0;
    double dfXDirection = 0.0, dfYDirection = 0.0;
    int bHaveZ = FALSE;
    int nAttachmentPoint = -1;
    CPLString osText;

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
            bHaveZ = TRUE;
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
            dfAngle = atan2( dfYDirection, dfXDirection ) * 180.0 / PI;
            break;

          case 1:
          case 3:
            if( osText != "" )
                osText += "\n";
            osText += TextUnescape(szLineBuf);
            break;

          case 50:
            dfAngle = CPLAtof(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

    if( nCode == 0 )
        poDS->UnreadValue();

    if( bHaveZ )
        poFeature->SetGeometryDirectly( new OGRPoint( dfX, dfY, dfZ ) );
    else
        poFeature->SetGeometryDirectly( new OGRPoint( dfX, dfY ) );

/* -------------------------------------------------------------------- */
/*      Apply text after stripping off any extra terminating newline.   */
/* -------------------------------------------------------------------- */
    if( osText != "" && osText[osText.size()-1] == '\n' )
        osText.resize( osText.size() - 1 );

    poFeature->SetField( "Text", osText );

    
/* -------------------------------------------------------------------- */
/*      We need to escape double quotes with backslashes before they    */
/*      can be inserted in the style string.                            */
/* -------------------------------------------------------------------- */
    if( strchr( osText, '"') != NULL )
    {
        CPLString osEscaped;
        size_t iC;

        for( iC = 0; iC < osText.size(); iC++ )
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

    if( oStyleProperties.count("Color") > 0 )
        nColor = atoi(oStyleProperties["Color"]);

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
    char* pszComma;

    osStyle.Printf("LABEL(f:\"Arial\",t:\"%s\"",osText.c_str());

    if( dfAngle != 0.0 )
    {
        snprintf(szBuffer, sizeof(szBuffer), "%.3g", dfAngle);
        pszComma = strchr(szBuffer, ',');
        if (pszComma)
            *pszComma = '.';
        osStyle += CPLString().Printf(",a:%s", szBuffer);
    }

    if( dfHeight != 0.0 )
    {
        snprintf(szBuffer, sizeof(szBuffer), "%.3g", dfHeight);
        pszComma = strchr(szBuffer, ',');
        if (pszComma)
            *pszComma = '.';
        osStyle += CPLString().Printf(",s:%sg", szBuffer);
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
/************************************************************************/

OGRFeature *OGRDXFLayer::TranslateTEXT()

{
    char szLineBuf[257];
    int nCode;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    double dfX = 0.0, dfY = 0.0, dfZ = 0.0;
    double dfAngle = 0.0;
    double dfHeight = 0.0;
    CPLString osText;
    int bHaveZ = FALSE;

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
            bHaveZ = TRUE;
            break;

          case 40:
            dfHeight = CPLAtof(szLineBuf);
            break;

          case 1:
          // case 3:  // we used to capture prompt, but it should not be displayed as text.
            osText += szLineBuf;
            break;

          case 50:
            dfAngle = CPLAtof(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

    if( nCode == 0 )
        poDS->UnreadValue();

    if( bHaveZ )
        poFeature->SetGeometryDirectly( new OGRPoint( dfX, dfY, dfZ ) );
    else
        poFeature->SetGeometryDirectly( new OGRPoint( dfX, dfY ) );

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
    if( strchr( osText, '"') != NULL )
    {
        CPLString osEscaped;
        size_t iC;

        for( iC = 0; iC < osText.size(); iC++ )
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

    int bHidden = 
        EQUAL(poDS->LookupLayerProperty( osLayer, "Hidden" ), "1");

/* -------------------------------------------------------------------- */
/*      Work out the color for this feature.                            */
/* -------------------------------------------------------------------- */
    int nColor = 256;

    if( oStyleProperties.count("Color") > 0 )
        nColor = atoi(oStyleProperties["Color"]);

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
    char* pszComma;

    osStyle.Printf("LABEL(f:\"Arial\",t:\"%s\"",osText.c_str());

    if( dfAngle != 0.0 )
    {
        snprintf(szBuffer, sizeof(szBuffer), "%.3g", dfAngle);
        pszComma = strchr(szBuffer, ',');
        if (pszComma)
            *pszComma = '.';
        osStyle += CPLString().Printf(",a:%s", szBuffer);
    }

    if( dfHeight != 0.0 )
    {
        snprintf(szBuffer, sizeof(szBuffer), "%.3g", dfHeight);
        pszComma = strchr(szBuffer, ',');
        if (pszComma)
            *pszComma = '.';
        osStyle += CPLString().Printf(",s:%sg", szBuffer);
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

OGRFeature *OGRDXFLayer::TranslatePOINT()

{
    char szLineBuf[257];
    int nCode;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    double dfX = 0.0, dfY = 0.0, dfZ = 0.0;
    int bHaveZ = FALSE;

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
            bHaveZ = TRUE;
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

    if( bHaveZ )
        poFeature->SetGeometryDirectly( new OGRPoint( dfX, dfY, dfZ ) );
    else
        poFeature->SetGeometryDirectly( new OGRPoint( dfX, dfY ) );

    if( nCode == 0 )
        poDS->UnreadValue();

    return poFeature;
}

/************************************************************************/
/*                           TranslateLINE()                            */
/************************************************************************/

OGRFeature *OGRDXFLayer::TranslateLINE()

{
    char szLineBuf[257];
    int nCode;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    double dfX1 = 0.0, dfY1 = 0.0, dfZ1 = 0.0;
    double dfX2 = 0.0, dfY2 = 0.0, dfZ2 = 0.0;
    int bHaveZ = FALSE;

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
            bHaveZ = TRUE;
            break;

          case 31:
            dfZ2 = CPLAtof(szLineBuf);
            bHaveZ = TRUE;
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
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
OGRFeature *OGRDXFLayer::TranslateLWPOLYLINE()

{
    // Collect vertices and attributes into a smooth polyline.
    // If there are no bulges, then we are a straight-line polyline.
    // Single-vertex polylines become points.
    // Group code 30 (vertex Z) is not part of this entity.

    char                szLineBuf[257];
    int                 nCode;
    int                 nPolylineFlag = 0;


    OGRFeature          *poFeature = new OGRFeature( poFeatureDefn );
    double              dfX = 0.0, dfY = 0.0, dfZ = 0.0;
    int                 bHaveX = FALSE;
    int                 bHaveY = FALSE;

    int                 nNumVertices = 1;   // use 1 based index
    int                 npolyarcVertexCount = 1;
    double              dfBulge = 0.0;
    DXFSmoothPolyline   smoothPolyline;

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
                bHaveY = FALSE;
            }
            dfX = CPLAtof(szLineBuf);
            bHaveX = TRUE;
            break;

          case 20:
            if( bHaveX && bHaveY )
            {
                smoothPolyline.AddPoint( dfX, dfY, dfZ, dfBulge );
                npolyarcVertexCount++;
                dfBulge = 0.0;
                bHaveX = FALSE;
            }
            dfY = CPLAtof(szLineBuf);
            bHaveY = TRUE;
            break;

          case 42:
            dfBulge = CPLAtof(szLineBuf);
            break;


          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

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
    ApplyOCSTransformer( poGeom );
    poFeature->SetGeometryDirectly( poGeom );

    PrepareLineStyle( poFeature );

    return poFeature;
}


/************************************************************************/
/*                         TranslatePOLYLINE()                          */
/*                                                                      */
/*      We also capture the following VERTEXes.                         */
/************************************************************************/

OGRFeature *OGRDXFLayer::TranslatePOLYLINE()

{
    char szLineBuf[257];
    int nCode;
    int nPolylineFlag = 0;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );

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

/* -------------------------------------------------------------------- */
/*      Collect VERTEXes as a smooth polyline.                          */
/* -------------------------------------------------------------------- */
    double              dfX = 0.0, dfY = 0.0, dfZ = 0.0;
    double              dfBulge = 0.0;
    DXFSmoothPolyline   smoothPolyline;

    smoothPolyline.setCoordinateDimension(2);

    while( nCode == 0 && !EQUAL(szLineBuf,"SEQEND") )
    {
        // Eat non-vertex objects.
        if( !EQUAL(szLineBuf,"VERTEX") )
        {
            while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf)))>0 ) {}
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

              default:
                break;
            }
        }

        smoothPolyline.AddPoint( dfX, dfY, dfZ, dfBulge );
        dfBulge = 0.0;
    }

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
    ApplyOCSTransformer( poGeom );
    poFeature->SetGeometryDirectly( poGeom );

    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                          TranslateCIRCLE()                           */
/************************************************************************/

OGRFeature *OGRDXFLayer::TranslateCIRCLE()

{
    char szLineBuf[257];
    int nCode;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    double dfX1 = 0.0, dfY1 = 0.0, dfZ1 = 0.0, dfRadius = 0.0;
    int bHaveZ = FALSE;

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
            bHaveZ = TRUE;
            break;

          case 40:
            dfRadius = CPLAtof(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
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

    poFeature->SetGeometryDirectly( poCircle );
    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                          TranslateELLIPSE()                          */
/************************************************************************/

OGRFeature *OGRDXFLayer::TranslateELLIPSE()

{
    char szLineBuf[257];
    int nCode;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    double dfX1 = 0.0, dfY1 = 0.0, dfZ1 = 0.0, dfRatio = 0.0;
    double dfStartAngle = 0.0, dfEndAngle = 360.0;
    double dfAxisX=0.0, dfAxisY=0.0, dfAxisZ=0.0;
    int bHaveZ = FALSE;

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
            bHaveZ = TRUE;
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
            dfEndAngle = -1 * CPLAtof(szLineBuf) * 180.0 / PI;
            break;

          case 42:
            // These *seem* to always be in radians regardless of $AUNITS
            dfStartAngle = -1 * CPLAtof(szLineBuf) * 180.0 / PI;
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

    if( nCode == 0 )
        poDS->UnreadValue();

/* -------------------------------------------------------------------- */
/*      Compute primary and secondary axis lengths, and the angle of    */
/*      rotation for the ellipse.                                       */
/* -------------------------------------------------------------------- */
    double dfPrimaryRadius, dfSecondaryRadius;
    double dfRotation;

    if( dfStartAngle > dfEndAngle )
        dfEndAngle += 360.0;

    dfPrimaryRadius = sqrt( dfAxisX * dfAxisX 
                            + dfAxisY * dfAxisY
                            + dfAxisZ * dfAxisZ );

    dfSecondaryRadius = dfRatio * dfPrimaryRadius;

    dfRotation = -1 * atan2( dfAxisY, dfAxisX ) * 180 / PI;

/* -------------------------------------------------------------------- */
/*      Create geometry                                                 */
/* -------------------------------------------------------------------- */
    OGRGeometry *poEllipse = 
        OGRGeometryFactory::approximateArcAngles( dfX1, dfY1, dfZ1, 
                                                  dfPrimaryRadius, 
                                                  dfSecondaryRadius,
                                                  dfRotation, 
                                                  dfStartAngle, dfEndAngle,
                                                  0.0 );

    if( !bHaveZ )
        poEllipse->flattenTo2D();

    poFeature->SetGeometryDirectly( poEllipse );

    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                            TranslateARC()                            */
/************************************************************************/

OGRFeature *OGRDXFLayer::TranslateARC()

{
    char szLineBuf[257];
    int nCode;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    double dfX1 = 0.0, dfY1 = 0.0, dfZ1 = 0.0, dfRadius = 0.0;
    double dfStartAngle = 0.0, dfEndAngle = 360.0;
    int bHaveZ = FALSE;

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
            bHaveZ = TRUE;
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

    if( nCode == 0 )
        poDS->UnreadValue();

/* -------------------------------------------------------------------- */
/*      Create geometry                                                 */
/* -------------------------------------------------------------------- */
    if( dfStartAngle > dfEndAngle )
        dfEndAngle += 360.0;

    OGRGeometry *poArc = 
        OGRGeometryFactory::approximateArcAngles( dfX1, dfY1, dfZ1, 
                                                  dfRadius, dfRadius, 0.0,
                                                  dfStartAngle, dfEndAngle,
                                                  0.0 );
    if( !bHaveZ )
        poArc->flattenTo2D();

    poFeature->SetGeometryDirectly( poArc );

    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                          TranslateSPLINE()                           */
/************************************************************************/

void rbspline(int npts,int k,int p1,double b[],double h[], double p[]);
void rbsplinu(int npts,int k,int p1,double b[],double h[], double p[]);

OGRFeature *OGRDXFLayer::TranslateSPLINE()

{
    char szLineBuf[257];
    int nCode, nDegree = -1, nFlags = -1, bClosed = FALSE, i;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    std::vector<double> adfControlPoints;

    adfControlPoints.push_back(0.0);

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

          case 70:
            nFlags = atoi(szLineBuf);
            if( nFlags & 1 )
                bClosed = TRUE;
            break;

          case 71:
            nDegree = atoi(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

    if( nCode == 0 )
        poDS->UnreadValue();

    if( bClosed )
    {
        for( i = 0; i < nDegree; i++ )
        {
            adfControlPoints.push_back( adfControlPoints[i*3+1] );
            adfControlPoints.push_back( adfControlPoints[i*3+2] );
            adfControlPoints.push_back( adfControlPoints[i*3+3] );
        }
    }

/* -------------------------------------------------------------------- */
/*      Interpolate spline                                              */
/* -------------------------------------------------------------------- */
    int nControlPoints = adfControlPoints.size() / 3;
    std::vector<double> h, p;

    h.push_back(1.0);
    for( i = 0; i < nControlPoints; i++ )
        h.push_back( 1.0 );
    
    // resolution:
    //int p1 = getGraphicVariableInt("$SPLINESEGS", 8) * npts;
    int p1 = nControlPoints * 8;

    p.push_back( 0.0 );
    for( i = 0; i < 3*p1; i++ )
        p.push_back( 0.0 );

    if( bClosed )
        rbsplinu( nControlPoints, nDegree+1, p1, &(adfControlPoints[0]), 
                  &(h[0]), &(p[0]) );
    else
        rbspline( nControlPoints, nDegree+1, p1, &(adfControlPoints[0]), 
                  &(h[0]), &(p[0]) );
    
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
/*                      GeometryInsertTransformer                       */
/************************************************************************/

class GeometryInsertTransformer : public OGRCoordinateTransformation
{
public:
    GeometryInsertTransformer() : 
            dfXOffset(0),dfYOffset(0),dfZOffset(0),
            dfXScale(1.0),dfYScale(1.0),dfZScale(1.0),
            dfAngle(0.0) {}

    double dfXOffset;
    double dfYOffset;
    double dfZOffset;
    double dfXScale;
    double dfYScale;
    double dfZScale;
    double dfAngle;

    OGRSpatialReference *GetSourceCS() { return NULL; }
    OGRSpatialReference *GetTargetCS() { return NULL; }
    int Transform( int nCount, 
                   double *x, double *y, double *z )
        { return TransformEx( nCount, x, y, z, NULL ); }
    
    int TransformEx( int nCount, 
                     double *x, double *y, double *z = NULL,
                     int *pabSuccess = NULL )
        {
            int i;
            for( i = 0; i < nCount; i++ )
            {
                double dfXNew, dfYNew;

                x[i] *= dfXScale;
                y[i] *= dfYScale;
                z[i] *= dfZScale;

                dfXNew = x[i] * cos(dfAngle) - y[i] * sin(dfAngle);
                dfYNew = x[i] * sin(dfAngle) + y[i] * cos(dfAngle);

                x[i] = dfXNew;
                y[i] = dfYNew;

                x[i] += dfXOffset;
                y[i] += dfYOffset;
                z[i] += dfZOffset;

                if( pabSuccess )
                    pabSuccess[i] = TRUE;
            }
            return TRUE;
        }
};

/************************************************************************/
/*                          TranslateINSERT()                           */
/************************************************************************/

OGRFeature *OGRDXFLayer::TranslateINSERT()

{
    char szLineBuf[257];
    int nCode;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    GeometryInsertTransformer oTransformer;
    CPLString osBlockName;
    double dfAngle = 0.0;

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
            dfAngle = CPLAtof(szLineBuf);
            // We want to transform this to radians. 
            // It is apparently always in degrees regardless of $AUNITS
            oTransformer.dfAngle = dfAngle * PI / 180.0;
            break;

          case 2: 
            osBlockName = szLineBuf;
            break;
            
          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

    if( nCode == 0 )
        poDS->UnreadValue();

/* -------------------------------------------------------------------- */
/*      In the case where we do not inlined blocks we just capture      */
/*      info on a point feature.                                        */
/* -------------------------------------------------------------------- */
    if( !poDS->InlineBlocks() )
    {
        poFeature->SetGeometryDirectly(
            new OGRPoint( oTransformer.dfXOffset, 
                          oTransformer.dfYOffset,
                          oTransformer.dfZOffset ) );

        poFeature->SetField( "BlockName", osBlockName );

        poFeature->SetField( "BlockAngle", dfAngle );
        poFeature->SetField( "BlockScale", 3, &(oTransformer.dfXScale) );

        return poFeature;
    }

/* -------------------------------------------------------------------- */
/*      Lookup the block.                                               */
/* -------------------------------------------------------------------- */
    DXFBlockDefinition *poBlock = poDS->LookupBlock( osBlockName );
    
    if( poBlock == NULL )
    {
        delete poFeature;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Transform the geometry.                                         */
/* -------------------------------------------------------------------- */
    if( poBlock->poGeometry != NULL )
    {
        OGRGeometry *poGeometry = poBlock->poGeometry->clone();

        poGeometry->transform( &oTransformer );

        poFeature->SetGeometryDirectly( poGeometry );
    }

/* -------------------------------------------------------------------- */
/*      If we have complete features associated with the block, push    */
/*      them on the pending feature stack copying over key override     */
/*      information.                                                    */
/*                                                                      */
/*      Note that while we transform the geometry of the features we    */
/*      don't adjust subtle things like text angle.                     */
/* -------------------------------------------------------------------- */
    unsigned int iSubFeat;

    for( iSubFeat = 0; iSubFeat < poBlock->apoFeatures.size(); iSubFeat++ )
    {
        OGRFeature *poSubFeature = poBlock->apoFeatures[iSubFeat]->Clone();
        CPLString osCompEntityId;

        if( poSubFeature->GetGeometryRef() != NULL )
            poSubFeature->GetGeometryRef()->transform( &oTransformer );

        ACAdjustText( dfAngle, oTransformer.dfXScale, poSubFeature );

#ifdef notdef
        osCompEntityId = poSubFeature->GetFieldAsString( "EntityHandle" );
        osCompEntityId += ":";
#endif
        osCompEntityId += poFeature->GetFieldAsString( "EntityHandle" );

        poSubFeature->SetField( "EntityHandle", osCompEntityId );

        apoPendingFeatures.push( poSubFeature );
    }

/* -------------------------------------------------------------------- */
/*      Return the working feature if we had geometry, otherwise        */
/*      return NULL and let the machinery find the rest of the          */
/*      features in the pending feature stack.                          */
/* -------------------------------------------------------------------- */
    if( poBlock->poGeometry == NULL )
    {
        delete poFeature;
        return NULL;
    }
    else
    {
        return poFeature;
    }
}

/************************************************************************/
/*                      GetNextUnfilteredFeature()                      */
/************************************************************************/

OGRFeature *OGRDXFLayer::GetNextUnfilteredFeature()

{
    OGRFeature *poFeature = NULL;

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
    int nCode;

    while( poFeature == NULL )
    {
        // read ahead to an entity.
        while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 ) {}

        if( nCode == -1 )
        {
            CPLDebug( "DXF", "Unexpected end of data without ENDSEC." );
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
        oStyleProperties.clear();

        if( EQUAL(szLineBuf,"POINT") )
        {
            poFeature = TranslatePOINT();
        }
        else if( EQUAL(szLineBuf,"MTEXT") )
        {
            poFeature = TranslateMTEXT();
        }
        else if( EQUAL(szLineBuf,"TEXT") 
                 || EQUAL(szLineBuf,"ATTDEF") )
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
        else
        {
            if( oIgnoredEntities.count(szLineBuf) == 0 )
            {
                oIgnoredEntities.insert( szLineBuf );
                CPLDebug( "DWG", "Ignoring one or more of entity '%s'.", 
                          szLineBuf );
            }
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
    while( TRUE )
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
    if( EQUAL(pszCap,OLCStringsAsUTF8) )
        return TRUE;
    else
        return FALSE;
}

