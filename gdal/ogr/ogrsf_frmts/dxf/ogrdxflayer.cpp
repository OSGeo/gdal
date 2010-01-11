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
        delete apoPendingFeatures.top();
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
        poFeature->SetField( "Layer", pszValue );
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
        poFeature->SetField( "Linetype", pszValue );
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

      default:
        break;
    }
}

/************************************************************************/
/*                          PrepareLineStyle()                          */
/*                                                                      */
/*      For now I don't bother trying to translate the dash/dot         */
/*      linetype into the style string since I'm not aware of any       */
/*      application that will honour it anyways.                        */
/************************************************************************/

void OGRDXFLayer::PrepareLineStyle( OGRFeature *poFeature )

{
    CPLString osLayer = poFeature->GetFieldAsString("Layer");

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

        dfWeight = atof(osWeight) / 100.0;
    }

/* -------------------------------------------------------------------- */
/*      Format the style string.                                        */
/* -------------------------------------------------------------------- */
    CPLString osStyle;
    const unsigned char *pabyDXFColors = OGRDXFDriver::GetDXFColorTable();

    osStyle.Printf( "PEN(c:#%02x%02x%02x", 
                    pabyDXFColors[nColor*3+0],
                    pabyDXFColors[nColor*3+1],
                    pabyDXFColors[nColor*3+2] );

    if( dfWeight > 0.0 )
        osStyle += CPLString().Printf( ",w:%.2gmm", dfWeight );

    osStyle += ")";
    
    poFeature->SetStyleString( osStyle );
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
    int nAttachmentPoint = -1;
    CPLString osText;

    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfX = atof(szLineBuf);
            break;

          case 20:
            dfY = atof(szLineBuf);
            break;

          case 30:
            dfZ = atof(szLineBuf);
            break;

          case 40:
            dfHeight = atof(szLineBuf);
            break;

          case 71:
            nAttachmentPoint = atoi(szLineBuf);
            break;

          case 11:
            dfXDirection = atof(szLineBuf);
            break;

          case 21:
            dfYDirection = atof(szLineBuf);
            dfAngle = atan2( dfXDirection, dfYDirection );
            break;

          case 1:
          case 3:
            osText += szLineBuf;
            break;

          case 50:
            dfAngle = atof(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

    if( nCode == 0 )
        poDS->UnreadValue();

    poFeature->SetField( "Text", osText );
    poFeature->SetGeometryDirectly( new OGRPoint( dfX, dfY, dfZ ) );

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

    osStyle.Printf("LABEL(f:\"Arial\",t:\"%s\"",osText.c_str());

    if( dfAngle != 0.0 )
        osStyle += CPLString().Printf(",a:%.3g", dfAngle);

    if( dfHeight != 0.0 )
        osStyle += CPLString().Printf(",s:%.3gg", dfHeight);

    if( nAttachmentPoint >= 0 && nAttachmentPoint <= 9 )
    {
        const static int anAttachmentMap[10] = 
            { -1, 7, 8, 9, 4, 5, 6, 1, 2, 3 };
        
        osStyle += 
            CPLString().Printf(",p:%d", anAttachmentMap[nAttachmentPoint]);
    }

    if( nColor > 0 && nColor < 256 )
    {
        const unsigned char *pabyDXFColors = OGRDXFDriver::GetDXFColorTable();
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

    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfX = atof(szLineBuf);
            break;

          case 20:
            dfY = atof(szLineBuf);
            break;

          case 30:
            dfZ = atof(szLineBuf);
            break;

          case 40:
            dfHeight = atof(szLineBuf);
            break;

          case 1:
          case 3:
            osText += szLineBuf;
            break;

          case 50:
            dfAngle = atof(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

    if( nCode == 0 )
        poDS->UnreadValue();

    poFeature->SetField( "Text", osText );
    poFeature->SetGeometryDirectly( new OGRPoint( dfX, dfY, dfZ ) );

/* -------------------------------------------------------------------- */
/*      Prepare style string.                                           */
/* -------------------------------------------------------------------- */
    CPLString osStyle;

    osStyle.Printf("LABEL(f:\"Arial\",t:\"%s\"",osText.c_str());

    if( dfAngle != 0.0 )
        osStyle += CPLString().Printf(",a:%.3g", dfAngle);

    if( dfHeight != 0.0 )
        osStyle += CPLString().Printf(",s:%.3gg", dfHeight);

    // add color!

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

    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfX = atof(szLineBuf);
            break;

          case 20:
            dfY = atof(szLineBuf);
            break;

          case 30:
            dfZ = atof(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

    poFeature->SetGeometryDirectly( new OGRPoint( dfX, dfY, dfZ ) );

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

/* -------------------------------------------------------------------- */
/*      Process values.                                                 */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfX1 = atof(szLineBuf);
            break;

          case 11:
            dfX2 = atof(szLineBuf);
            break;

          case 20:
            dfY1 = atof(szLineBuf);
            break;

          case 21:
            dfY2 = atof(szLineBuf);
            break;

          case 30:
            dfZ1 = atof(szLineBuf);
            break;

          case 31:
            dfZ2 = atof(szLineBuf);
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
    poLS->addPoint( dfX1, dfY1, dfZ1 );
    poLS->addPoint( dfX2, dfY2, dfZ2 );

    poFeature->SetGeometryDirectly( poLS );

    PrepareLineStyle( poFeature );

    return poFeature;
}

/************************************************************************/
/*                         TranslateLWPOLYLINE()                        */
/************************************************************************/

OGRFeature *OGRDXFLayer::TranslateLWPOLYLINE()

{
    char szLineBuf[257];
    int nCode;
    int nPolylineFlag = 0;
    OGRFeature *poFeature = new OGRFeature( poFeatureDefn );
    OGRLineString *poLS = new OGRLineString();
    double dfX = 0.0, dfY = 0.0, dfZ = 0.0;
    int    bHaveX = FALSE, bHaveY = FALSE;

/* -------------------------------------------------------------------- */
/*      Collect information from the LWPOLYLINE object itself.          */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 70:
            nPolylineFlag = atoi(szLineBuf);
            break;

          case 10:
            if( bHaveX && bHaveY )
            {
                poLS->addPoint( dfX, dfY, dfZ );
                bHaveY = FALSE;
            }
            dfX = atof(szLineBuf);
            bHaveX = TRUE;
            break;

          case 20:
            if( bHaveX && bHaveY )
            {
                poLS->addPoint( dfX, dfY, dfZ );
                bHaveX = FALSE;
            }
            dfY = atof(szLineBuf);
            bHaveY = TRUE;
            break;

          case 30:
            dfZ = atof(szLineBuf);
            break;

          default:
            TranslateGenericProperty( poFeature, nCode, szLineBuf );
            break;
        }
    }

    poDS->UnreadValue();

    if( bHaveX && bHaveY )
        poLS->addPoint( dfX, dfY, dfZ );

/* -------------------------------------------------------------------- */
/*      Close polyline as polygon if necessary.                         */
/* -------------------------------------------------------------------- */
    if( (nPolylineFlag & 0x01)
        && poLS->getNumPoints() > 0 
        && (poLS->getX(poLS->getNumPoints()-1) != poLS->getX(0)
            || poLS->getY(poLS->getNumPoints()-1) != poLS->getY(0)) )
    {
        poLS->addPoint( poLS->getX(0), poLS->getY(0), poLS->getZ(0) );
    }

    poFeature->SetGeometryDirectly( poLS );

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
/*      Collect VERTEXes as a linestring.                               */
/* -------------------------------------------------------------------- */
    OGRLineString *poLS = new OGRLineString();
    double dfX = 0.0, dfY = 0.0, dfZ = 0.0;

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
                dfX = atof(szLineBuf);
                break;
                
              case 20:
                dfY = atof(szLineBuf);
                break;
                
              case 30:
                dfZ = atof(szLineBuf);
                break;

              default:
                break;
            }
        }

        poLS->addPoint( dfX, dfY, dfZ );
    }

/* -------------------------------------------------------------------- */
/*      Close polyline as polygon if necessary.                         */
/* -------------------------------------------------------------------- */
    if( (nPolylineFlag & 0x01)
        && poLS->getNumPoints() > 0 
        && (poLS->getX(poLS->getNumPoints()-1) != poLS->getX(0)
            || poLS->getY(poLS->getNumPoints()-1) != poLS->getY(0)) )
    {
        poLS->addPoint( poLS->getX(0), poLS->getY(0), poLS->getZ(0) );
    }

    poFeature->SetGeometryDirectly( poLS );

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

/* -------------------------------------------------------------------- */
/*      Process values.                                                 */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfX1 = atof(szLineBuf);
            break;

          case 20:
            dfY1 = atof(szLineBuf);
            break;

          case 30:
            dfZ1 = atof(szLineBuf);
            break;

          case 40:
            dfRadius = atof(szLineBuf);
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
    poFeature->SetGeometryDirectly( 
        OGRGeometryFactory::approximateArcAngles( dfX1, dfY1, dfZ1, 
                                                  dfRadius, dfRadius, 0.0,
                                                  0.0, 360.0, 
                                                  0.0 ) );

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

/* -------------------------------------------------------------------- */
/*      Process values.                                                 */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfX1 = atof(szLineBuf);
            break;

          case 20:
            dfY1 = atof(szLineBuf);
            break;

          case 30:
            dfZ1 = atof(szLineBuf);
            break;

          case 11:
            dfAxisX = atof(szLineBuf);
            break;

          case 21:
            dfAxisY = atof(szLineBuf);
            break;

          case 31:
            dfAxisZ = atof(szLineBuf);
            break;

          case 40:
            dfRatio = atof(szLineBuf);
            break;

          case 41:
            dfEndAngle = -1 * atof(szLineBuf) * 180.0 / PI;
            break;

          case 42:
            dfStartAngle = -1 * atof(szLineBuf) * 180.0 / PI;
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
    poFeature->SetGeometryDirectly( 
        OGRGeometryFactory::approximateArcAngles( dfX1, dfY1, dfZ1, 
                                                  dfPrimaryRadius, 
                                                  dfSecondaryRadius,
                                                  dfRotation, 
                                                  dfStartAngle, dfEndAngle,
                                                  0.0 ) );

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

/* -------------------------------------------------------------------- */
/*      Process values.                                                 */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            dfX1 = atof(szLineBuf);
            break;

          case 20:
            dfY1 = atof(szLineBuf);
            break;

          case 30:
            dfZ1 = atof(szLineBuf);
            break;

          case 40:
            dfRadius = atof(szLineBuf);
            break;

          case 50:
            dfEndAngle = -1 * atof(szLineBuf);
            break;

          case 51:
            dfStartAngle = -1 * atof(szLineBuf);
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

    poFeature->SetGeometryDirectly( 
        OGRGeometryFactory::approximateArcAngles( dfX1, dfY1, dfZ1, 
                                                  dfRadius, dfRadius, 0.0,
                                                  dfStartAngle, dfEndAngle,
                                                  0.0 ) );

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
            adfControlPoints.push_back( atof(szLineBuf) );
            break;

          case 20:
            adfControlPoints.push_back( atof(szLineBuf) );
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

/* -------------------------------------------------------------------- */
/*      Process values.                                                 */
/* -------------------------------------------------------------------- */
    while( (nCode = poDS->ReadValue(szLineBuf,sizeof(szLineBuf))) > 0 )
    {
        switch( nCode )
        {
          case 10:
            oTransformer.dfXOffset = atof(szLineBuf);
            break;

          case 20:
            oTransformer.dfYOffset = atof(szLineBuf);
            break;

          case 30:
            oTransformer.dfZOffset = atof(szLineBuf);
            break;

          case 41:
            oTransformer.dfXScale = atof(szLineBuf);
            break;

          case 42:
            oTransformer.dfYScale = atof(szLineBuf);
            break;

          case 43:
            oTransformer.dfZScale = atof(szLineBuf);
            break;

          case 50:
            oTransformer.dfAngle = atof(szLineBuf) * PI / 180.0;
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

        if( poSubFeature->GetGeometryRef() != NULL )
            poSubFeature->GetGeometryRef()->transform( &oTransformer );

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
        poFeature = apoPendingFeatures.top();
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
        else if( EQUAL(szLineBuf,"TEXT") )
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
        else
        {
            CPLDebug( "DXF", "Ignoring entity '%s'.", szLineBuf );
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
    return FALSE;
}

