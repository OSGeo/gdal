/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRDGNLayer class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2000, Frank Warmerdam (warmerdam@pobox.com)
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

#include "ogr_dgn.h"
#include "cpl_conv.h"
#include "ogr_featurestyle.h"
#include "ogr_api.h"
#include <list>

CPL_CVSID("$Id$");

/************************************************************************/
/*                           OGRDGNLayer()                              */
/************************************************************************/

OGRDGNLayer::OGRDGNLayer( const char * pszName, DGNHandle hDGNIn,
                          int bUpdateIn ) :
    poFeatureDefn(new OGRFeatureDefn( pszName )),
    iNextShapeId(0),
    hDGN(hDGNIn),
    bUpdate(bUpdateIn)
    // Unused:
    // bHaveSimpleQuery(FALSE)
{

/* -------------------------------------------------------------------- */
/*      Work out what link format we are using.                         */
/* -------------------------------------------------------------------- */
    OGRFieldType eLinkFieldType;

    pszLinkFormat = const_cast<char *>(
        CPLGetConfigOption( "DGN_LINK_FORMAT", "FIRST" ) );

    if( EQUAL(pszLinkFormat,"FIRST") )
        eLinkFieldType = OFTInteger;
    else if( EQUAL(pszLinkFormat,"LIST") )
        eLinkFieldType = OFTIntegerList;
    else if( EQUAL(pszLinkFormat,"STRING") )
        eLinkFieldType = OFTString;
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "DGN_LINK_FORMAT=%s, but only FIRST, LIST or STRING "
                  "supported.",
                  pszLinkFormat );
        pszLinkFormat = const_cast<char *>( "FIRST" );
        eLinkFieldType = OFTInteger;
    }
    pszLinkFormat = CPLStrdup(pszLinkFormat);

/* -------------------------------------------------------------------- */
/*      Create the feature definition.                                  */
/* -------------------------------------------------------------------- */
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->Reference();

    OGRFieldDefn oField( "", OFTInteger );

/* -------------------------------------------------------------------- */
/*      Element type                                                    */
/* -------------------------------------------------------------------- */
    oField.SetName( "Type" );
    oField.SetType( OFTInteger );
    oField.SetWidth( 2 );
    oField.SetPrecision( 0 );
    poFeatureDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      Level number.                                                   */
/* -------------------------------------------------------------------- */
    oField.SetName( "Level" );
    oField.SetType( OFTInteger );
    oField.SetWidth( 2 );
    oField.SetPrecision( 0 );
    poFeatureDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      graphic group                                                   */
/* -------------------------------------------------------------------- */
    oField.SetName( "GraphicGroup" );
    oField.SetType( OFTInteger );
    oField.SetWidth( 4 );
    oField.SetPrecision( 0 );
    poFeatureDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      ColorIndex                                                      */
/* -------------------------------------------------------------------- */
    oField.SetName( "ColorIndex" );
    oField.SetType( OFTInteger );
    oField.SetWidth( 3 );
    oField.SetPrecision( 0 );
    poFeatureDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      Weight                                                          */
/* -------------------------------------------------------------------- */
    oField.SetName( "Weight" );
    oField.SetType( OFTInteger );
    oField.SetWidth( 2 );
    oField.SetPrecision( 0 );
    poFeatureDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      Style                                                           */
/* -------------------------------------------------------------------- */
    oField.SetName( "Style" );
    oField.SetType( OFTInteger );
    oField.SetWidth( 1 );
    oField.SetPrecision( 0 );
    poFeatureDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      EntityNum                                                       */
/* -------------------------------------------------------------------- */
    oField.SetName( "EntityNum" );
    oField.SetType( eLinkFieldType );
    oField.SetWidth( 0 );
    oField.SetPrecision( 0 );
    poFeatureDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      MSLink                                                          */
/* -------------------------------------------------------------------- */
    oField.SetName( "MSLink" );
    oField.SetType( eLinkFieldType );
    oField.SetWidth( 0 );
    oField.SetPrecision( 0 );
    poFeatureDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      Text                                                            */
/* -------------------------------------------------------------------- */
    oField.SetName( "Text" );
    oField.SetType( OFTString );
    oField.SetWidth( 0 );
    oField.SetPrecision( 0 );
    poFeatureDefn->AddFieldDefn( &oField );

/* -------------------------------------------------------------------- */
/*      Create template feature for evaluating simple expressions.      */
/* -------------------------------------------------------------------- */
    poEvalFeature = new OGRFeature( poFeatureDefn );

    /* TODO: I am intending to keep track of simple attribute queries (ones
       using only FID, Type and Level and short circuiting their operation
       based on the index.  However, there are some complexities with
       complex elements, and spatial queries that have caused me to put it
       off for now.
    */
}

/************************************************************************/
/*                           ~OGRDGNLayer()                             */
/************************************************************************/

OGRDGNLayer::~OGRDGNLayer()

{
    if( m_nFeaturesRead > 0 )
    {
        CPLDebug( "Mem", "%d features read on layer '%s'.",
                  static_cast<int>( m_nFeaturesRead ),
                  poFeatureDefn->GetName() );
    }

    delete poEvalFeature;

    poFeatureDefn->Release();

    CPLFree( pszLinkFormat );
}

/************************************************************************/
/*                          SetSpatialFilter()                          */
/************************************************************************/

void OGRDGNLayer::SetSpatialFilter( OGRGeometry * poGeomIn )

{
    if( !InstallFilter(poGeomIn) )
        return;

    if( m_poFilterGeom != NULL )
    {
        DGNSetSpatialFilter( hDGN,
                             m_sFilterEnvelope.MinX,
                             m_sFilterEnvelope.MinY,
                             m_sFilterEnvelope.MaxX,
                             m_sFilterEnvelope.MaxY );
    }
    else
    {
        DGNSetSpatialFilter( hDGN, 0.0, 0.0, 0.0, 0.0 );
    }

    ResetReading();
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRDGNLayer::ResetReading()

{
    iNextShapeId = 0;
    DGNRewind( hDGN );
}

/************************************************************************/
/*                             GetFeature()                             */
/************************************************************************/

OGRFeature *OGRDGNLayer::GetFeature( GIntBig nFeatureId )

{
    if( nFeatureId > INT_MAX || !DGNGotoElement( hDGN, (int)nFeatureId ) )
        return NULL;

    // We should likely clear the spatial search region as it affects
    // DGNReadElement(), but I will defer that for now.

    DGNElemCore *psElement = DGNReadElement( hDGN );
    OGRFeature *poFeature = ElementToFeature( psElement );
    DGNFreeElement( hDGN, psElement );

    if( poFeature == NULL )
        return NULL;

    if( poFeature->GetFID() != nFeatureId )
    {
        delete poFeature;
        return NULL;
    }

    return poFeature;
}

/************************************************************************/
/*                           ConsiderBrush()                            */
/*                                                                      */
/*      Method to set the style for a polygon, including a brush if     */
/*      appropriate.                                                    */
/************************************************************************/

void OGRDGNLayer::ConsiderBrush( DGNElemCore *psElement, const char *pszPen,
                                 OGRFeature *poFeature )

{
    int nFillColor = 0;
    int gv_red = 0;
    int gv_green = 0;
    int gv_blue = 0;

    if( DGNGetShapeFillInfo( hDGN, psElement, &nFillColor )
        && DGNLookupColor( hDGN, nFillColor,
                           &gv_red, &gv_green, &gv_blue ) )
    {
        char szFullStyle[256];
        snprintf( szFullStyle, sizeof(szFullStyle),
                 "BRUSH(fc:#%02x%02x%02x,id:\"ogr-brush-0\")",
                 gv_red, gv_green, gv_blue );

        if( nFillColor != psElement->color )
        {
            strcat( szFullStyle, ";" );
            strcat( szFullStyle, pszPen );
        }
        poFeature->SetStyleString( szFullStyle );
    }
    else
        poFeature->SetStyleString( pszPen );
}

/************************************************************************/
/*                          ElementToFeature()                          */
/************************************************************************/

OGRFeature *OGRDGNLayer::ElementToFeature( DGNElemCore *psElement )

{
    OGRFeature  *poFeature = new OGRFeature( poFeatureDefn );

    poFeature->SetFID( psElement->element_id );
    poFeature->SetField( "Type", psElement->type );
    poFeature->SetField( "Level", psElement->level );
    poFeature->SetField( "GraphicGroup", psElement->graphic_group );
    poFeature->SetField( "ColorIndex", psElement->color );
    poFeature->SetField( "Weight", psElement->weight );
    poFeature->SetField( "Style", psElement->style );

    m_nFeaturesRead++;

/* -------------------------------------------------------------------- */
/*      Collect linkage information                                     */
/* -------------------------------------------------------------------- */
    const int MAX_LINK = 100;

    int anEntityNum[MAX_LINK];
    anEntityNum[0] = 0;

    int anMSLink[MAX_LINK];
    anMSLink[0] = 0;

    unsigned char *pabyData = NULL;
    int iLink = 0;
    int nLinkCount = 0;


    pabyData = DGNGetLinkage( hDGN, psElement, iLink, NULL,
                              anEntityNum + iLink, anMSLink + iLink, NULL );
    while( pabyData && nLinkCount < MAX_LINK )
    {
        iLink++;

        if( anEntityNum[nLinkCount] != 0 || anMSLink[nLinkCount] != 0 )
            nLinkCount++;

        anEntityNum[nLinkCount] = 0;
        anMSLink[nLinkCount] = 0;

        pabyData = DGNGetLinkage( hDGN, psElement, iLink, NULL,
                                  anEntityNum+nLinkCount, anMSLink+nLinkCount,
                                  NULL );
    }

/* -------------------------------------------------------------------- */
/*      Apply attribute linkage to feature.                             */
/* -------------------------------------------------------------------- */
    if( nLinkCount > 0 )
    {
        if( EQUAL(pszLinkFormat,"FIRST") )
        {
            poFeature->SetField( "EntityNum", anEntityNum[0] );
            poFeature->SetField( "MSLink", anMSLink[0] );
        }
        else if( EQUAL(pszLinkFormat,"LIST") )
        {
            poFeature->SetField( "EntityNum", nLinkCount, anEntityNum );
            poFeature->SetField( "MSLink", nLinkCount, anMSLink );
        }
        else if( EQUAL(pszLinkFormat,"STRING") )
        {
            char szEntityList[MAX_LINK * 9];
            char szMSLinkList[MAX_LINK * 9];
            int nEntityLen = 0;
            int nMSLinkLen = 0;

            for( iLink = 0; iLink < nLinkCount; iLink++ )
            {
                if( iLink != 0 )
                {
                    szEntityList[nEntityLen++] = ',';
                    szMSLinkList[nMSLinkLen++] = ',';
                }

                snprintf( szEntityList + nEntityLen,
                          sizeof(szEntityList) - nEntityLen,
                          "%d", anEntityNum[iLink]);
                snprintf( szMSLinkList + nMSLinkLen,
                          sizeof(szMSLinkList) - nMSLinkLen,
                          "%d", anMSLink[iLink] );

                nEntityLen +=
                    static_cast<int>(strlen(szEntityList + nEntityLen ));
                nMSLinkLen +=
                    static_cast<int>(strlen(szMSLinkList + nMSLinkLen ));
            }

            poFeature->SetField( "EntityNum", szEntityList );
            poFeature->SetField( "MSLink", szMSLinkList );
        }
    }

/* -------------------------------------------------------------------- */
/*      Lookup color.                                                   */
/* -------------------------------------------------------------------- */
    int gv_red;
    int gv_green;
    int gv_blue;

    char szFSColor[128];
    szFSColor[0] = '\0';
    if( DGNLookupColor( hDGN, psElement->color,
                        &gv_red, &gv_green, &gv_blue ) )
    {
        char gv_color[128];
        CPLsnprintf( gv_color, sizeof(gv_color), "%f %f %f 1.0",
                 gv_red / 255.0, gv_green / 255.0, gv_blue / 255.0 );

        snprintf( szFSColor, sizeof(szFSColor), "c:#%02x%02x%02x",
                 gv_red, gv_green, gv_blue );
    }

/* -------------------------------------------------------------------- */
/*      Generate corresponding PEN style.                               */
/* -------------------------------------------------------------------- */
    char szPen[256];
    szPen[0] = '\0';

    if( psElement->style == DGNS_SOLID )
        snprintf( szPen, sizeof(szPen), "PEN(id:\"ogr-pen-0\"" );
    else if( psElement->style == DGNS_DOTTED )
        snprintf( szPen, sizeof(szPen), "PEN(id:\"ogr-pen-5\"" );
    else if( psElement->style == DGNS_MEDIUM_DASH )
        snprintf( szPen, sizeof(szPen), "PEN(id:\"ogr-pen-2\"" );
    else if( psElement->style == DGNS_LONG_DASH )
        snprintf( szPen, sizeof(szPen), "PEN(id:\"ogr-pen-4\"" );
    else if( psElement->style == DGNS_DOT_DASH )
        snprintf( szPen, sizeof(szPen), "PEN(id:\"ogr-pen-6\"" );
    else if( psElement->style == DGNS_SHORT_DASH )
        snprintf( szPen, sizeof(szPen), "PEN(id:\"ogr-pen-3\"" );
    else if( psElement->style == DGNS_DASH_DOUBLE_DOT )
        snprintf( szPen, sizeof(szPen), "PEN(id:\"ogr-pen-7\"" );
    else if( psElement->style == DGNS_LONG_DASH_SHORT_DASH )
        snprintf( szPen, sizeof(szPen), "PEN(p:\"10px 5px 4px 5px\"" );
    else
        snprintf( szPen, sizeof(szPen), "PEN(id:\"ogr-pen-0\"" );

    if( strlen(szFSColor) > 0 )
        snprintf( szPen+strlen(szPen), sizeof(szPen)-strlen(szPen),
                  ",%s", szFSColor );

    if( psElement->weight > 1 )
        snprintf( szPen+strlen(szPen), sizeof(szPen)-strlen(szPen),
                  ",w:%dpx", psElement->weight );

    strcat( szPen, ")" );

    switch( psElement->stype )
    {
      case DGNST_MULTIPOINT:
        if( psElement->type == DGNT_SHAPE )
        {
            OGRLinearRing *poLine = new OGRLinearRing();
            DGNElemMultiPoint *psEMP
                = reinterpret_cast<DGNElemMultiPoint *>( psElement );

            poLine->setNumPoints( psEMP->num_vertices );
            for( int i = 0; i < psEMP->num_vertices; i++ )
            {
                poLine->setPoint( i,
                                  psEMP->vertices[i].x,
                                  psEMP->vertices[i].y,
                                  psEMP->vertices[i].z );
            }

            OGRPolygon *poPolygon = new OGRPolygon();
            poPolygon->addRingDirectly( poLine );

            poFeature->SetGeometryDirectly( poPolygon );

            ConsiderBrush( psElement, szPen, poFeature );
        }
        else if( psElement->type == DGNT_CURVE )
        {
            OGRLineString *poLine = new OGRLineString();
            DGNElemMultiPoint *psEMP
                = reinterpret_cast<DGNElemMultiPoint *>( psElement );
            const int nPoints = 5 * psEMP->num_vertices;
            DGNPoint *pasPoints = static_cast<DGNPoint *>(
                CPLMalloc( sizeof(DGNPoint) * nPoints ) );

            DGNStrokeCurve( hDGN, psEMP, nPoints, pasPoints );

            poLine->setNumPoints( nPoints );
            for( int i = 0; i < nPoints; i++ )
            {
                poLine->setPoint( i,
                                  pasPoints[i].x,
                                  pasPoints[i].y,
                                  pasPoints[i].z );
            }

            poFeature->SetGeometryDirectly( poLine );
            CPLFree( pasPoints );

            poFeature->SetStyleString( szPen );
        }
        else
        {
            DGNElemMultiPoint *psEMP = (DGNElemMultiPoint *) psElement;

            if( psEMP->num_vertices > 0 )
            {
                OGRLineString       *poLine = new OGRLineString();
                poLine->setNumPoints( psEMP->num_vertices );
                for( int i = 0; i < psEMP->num_vertices; i++ )
                {
                    poLine->setPoint( i,
                                      psEMP->vertices[i].x,
                                      psEMP->vertices[i].y,
                                      psEMP->vertices[i].z );
                }

                poFeature->SetGeometryDirectly( poLine );
            }

            poFeature->SetStyleString( szPen );
        }
        break;

      case DGNST_ARC:
      {
          OGRLineString *poLine = new OGRLineString();
          DGNElemArc    *psArc = (DGNElemArc *) psElement;
          DGNPoint      asPoints[90];
          // TODO: std::abs abd std::max
          int nPoints = static_cast<int>(MAX(1,ABS(psArc->sweepang) / 5) + 1);
          if( nPoints > 90 )
              nPoints = 90;
          DGNStrokeArc( hDGN, psArc, nPoints, asPoints );

          poLine->setNumPoints( nPoints );
          for( int i = 0; i < nPoints; i++ )
          {
              poLine->setPoint( i,
                                asPoints[i].x,
                                asPoints[i].y,
                                asPoints[i].z );
          }

          poFeature->SetGeometryDirectly( poLine );
          poFeature->SetStyleString( szPen );
      }
      break;

      case DGNST_TEXT:
      {
          OGRPoint *poPoint = new OGRPoint();
          DGNElemText *psText = reinterpret_cast<DGNElemText *>( psElement );

          poPoint->setX( psText->origin.x );
          poPoint->setY( psText->origin.y );
          poPoint->setZ( psText->origin.z );

          poFeature->SetGeometryDirectly( poPoint );

          const size_t nOgrFSLen = strlen(psText->string) + 150;
          char *pszOgrFS = static_cast<char *>( CPLMalloc(nOgrFSLen) );

          // setup the basic label.
          snprintf( pszOgrFS, nOgrFSLen, "LABEL(t:\"%s\"",  psText->string );

          // set the color if we have it.
          if( strlen(szFSColor) > 0 )
              snprintf( pszOgrFS+strlen(pszOgrFS), nOgrFSLen-strlen(pszOgrFS),
                        ",%s", szFSColor );

          // Add the size info in ground units.
          // TODO: std::abs
          if( ABS(psText->height_mult) >= 6.0 )
              CPLsnprintf( pszOgrFS+strlen(pszOgrFS),
                           nOgrFSLen-strlen(pszOgrFS), ",s:%dg",
                           static_cast<int>( psText->height_mult ) );
          else if( ABS(psText->height_mult) > 0.1 )
              CPLsnprintf( pszOgrFS+strlen(pszOgrFS),
                          nOgrFSLen-strlen(pszOgrFS), ",s:%.3fg",
                           psText->height_mult );
          else
              CPLsnprintf( pszOgrFS+strlen(pszOgrFS),
                          nOgrFSLen-strlen(pszOgrFS), ",s:%.12fg",
                           psText->height_mult );

          // Add the font name. Name it MstnFont<FONTNUMBER> if not available
          // in the font list. #3392
          static const char * const papszFontList[] =
          { "STANDARD", "WORKING", "FANCY", "ENGINEERING", "NEWZERO",
            "STENCEL", // 0-5
            "USTN_FANCY", "COMPRESSED", "STENCEQ", NULL, "hand", "ARCH", //6-11
            "ARCHB", NULL, NULL, "IGES1001", "IGES1002", "IGES1003", //12-17
            "CENTB", "MICROS", NULL, NULL, "ISOFRACTIONS", "ITALICS", //18-23
            "ISO30", NULL, "GREEK", "ISOREC", "Isoeq", NULL, //24-29
            "ISO_FONTLEFT", "ISO_FONTRIGHT", "INTL_ENGINEERING", "INTL_WORKING",
            "ISOITEQ", NULL, // 30-35
            "USTN FONT 26", NULL, NULL, NULL, NULL, "ARCHITECTURAL", //36-41
            "BLOCK_OUTLINE", "LOW_RES_FILLED", NULL, NULL, NULL, NULL, //42-47
            NULL, NULL, "UPPERCASE", NULL, NULL, NULL, //48-53
            NULL, NULL, NULL, NULL, NULL, NULL, //54-49
            "FONT060", "din", "dinit", "helvl", "HELVLIT", "helv", //60-65
            "HELVIT", "cent", "CENTIT", "SCRIPT", NULL, NULL, //66-71
            NULL, NULL, NULL, NULL, "MICROQ", "dotfont", //72-77
            "DOTIT", NULL, NULL, NULL, NULL, NULL, //78-83
            NULL, NULL, NULL, NULL, NULL, NULL, //84-89
            NULL, NULL, "FONT092", NULL, "FONT094", NULL, //90-95
            NULL, NULL, NULL, NULL, "ANSI_SYMBOLS",
            "FEATURE_CONTROL_SYSMBOLS", // 96-101
            "SYMB_FAST", NULL, NULL, "INTL_ISO", "INTL_ISO_EQUAL",
            "INTL_ISO_ITALIC", // 102-107
            "INTL_ISO_ITALIC_EQUAL" }; //108

          if(psText->font_id <= 108 && papszFontList[psText->font_id] != NULL )
          {
              snprintf( pszOgrFS+strlen(pszOgrFS),
                        nOgrFSLen-strlen(pszOgrFS), ",f:%s",
                       papszFontList[psText->font_id] );
          }
          else
          {
              snprintf( pszOgrFS+strlen(pszOgrFS),
                        nOgrFSLen-strlen(pszOgrFS), ",f:MstnFont%d",
                       psText->font_id );
          }

          // Add the angle, if not horizontal
          if( psText->rotation != 0.0 )
              snprintf( pszOgrFS+strlen(pszOgrFS),
                        nOgrFSLen-strlen(pszOgrFS), ",a:%d",
                       (int) (psText->rotation+0.5) );

          snprintf( pszOgrFS+strlen(pszOgrFS),
                        nOgrFSLen-strlen(pszOgrFS), ")" );

          poFeature->SetStyleString( pszOgrFS );
          CPLFree( pszOgrFS );

          poFeature->SetField( "Text", psText->string );
      }
      break;

      case DGNST_COMPLEX_HEADER:
      {
          DGNElemComplexHeader *psHdr = (DGNElemComplexHeader *) psElement;
          OGRMultiLineString  oChildren;

          /* collect subsequent child geometries. */
          // we should disable the spatial filter ... add later.
          for( int iChild = 0; iChild < psHdr->numelems; iChild++ )
          {
              OGRFeature *poChildFeature = NULL;
              DGNElemCore *psChildElement;

              psChildElement = DGNReadElement( hDGN );
              // should verify complex bit set, not another header.

              if( psChildElement != NULL )
              {
                  poChildFeature = ElementToFeature( psChildElement );
                  DGNFreeElement( hDGN, psChildElement );
              }

              if( poChildFeature != NULL
                  && poChildFeature->GetGeometryRef() != NULL )
              {
                  OGRGeometry *poGeom;

                  poGeom = poChildFeature->GetGeometryRef();
                  if( wkbFlatten(poGeom->getGeometryType()) == wkbLineString )
                      oChildren.addGeometry( poGeom );
              }

              if( poChildFeature != NULL )
                  delete poChildFeature;
          }

          // Try to assemble into polygon geometry.
          OGRGeometry *poGeom = NULL;;

          if( psElement->type == DGNT_COMPLEX_SHAPE_HEADER )
              poGeom = reinterpret_cast<OGRPolygon *>(
                  OGRBuildPolygonFromEdges(
                      reinterpret_cast<OGRGeometryH>( &oChildren ),
                      TRUE, TRUE, 100000, NULL ) );
          else
              poGeom = oChildren.clone();

          if( poGeom != NULL )
              poFeature->SetGeometryDirectly( poGeom );

          ConsiderBrush( psElement, szPen, poFeature );
      }
      break;

      default:
        break;
    }

/* -------------------------------------------------------------------- */
/*      Fixup geometry dimension.                                       */
/* -------------------------------------------------------------------- */
    if( poFeature->GetGeometryRef() != NULL )
        poFeature->GetGeometryRef()->setCoordinateDimension(
            DGNGetDimension( hDGN ) );

    return poFeature;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRDGNLayer::GetNextFeature()

{
    DGNGetElementIndex( hDGN, NULL );

    DGNElemCore *psElement = NULL;
    while( (psElement = DGNReadElement( hDGN )) != NULL )
    {
        if( psElement->deleted )
        {
            DGNFreeElement( hDGN, psElement );
            continue;
        }

        OGRFeature *poFeature = ElementToFeature( psElement );
        DGNFreeElement( hDGN, psElement );

        if( poFeature == NULL )
            continue;

        if( poFeature->GetGeometryRef() == NULL )
        {
            delete poFeature;
            continue;
        }

        if( (m_poAttrQuery == NULL
             || m_poAttrQuery->Evaluate( poFeature ))
            && FilterGeometry( poFeature->GetGeometryRef() ) )
            return poFeature;

        delete poFeature;
    }

    return NULL;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDGNLayer::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,OLCRandomRead) )
        return TRUE;

    else if( EQUAL(pszCap,OLCSequentialWrite) )
        return bUpdate;
    else if( EQUAL(pszCap,OLCRandomWrite) )
        return FALSE; /* maybe later? */

    else if( EQUAL(pszCap,OLCFastFeatureCount) )
        return m_poFilterGeom == NULL || m_poAttrQuery == NULL;

    else if( EQUAL(pszCap,OLCFastSpatialFilter) )
        return FALSE;

    else if( EQUAL(pszCap,OLCFastGetExtent) )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                          GetFeatureCount()                           */
/************************************************************************/

GIntBig OGRDGNLayer::GetFeatureCount( int bForce )

{
/* -------------------------------------------------------------------- */
/*      If any odd conditions are in effect collect the information     */
/*      normally.                                                       */
/* -------------------------------------------------------------------- */
    if( m_poFilterGeom != NULL || m_poAttrQuery != NULL )
        return OGRLayer::GetFeatureCount( bForce );

/* -------------------------------------------------------------------- */
/*      Otherwise scan the index.                                       */
/* -------------------------------------------------------------------- */
    int nElementCount;
    const DGNElementInfo *pasIndex = DGNGetElementIndex(hDGN,&nElementCount);

    int nFeatureCount = 0;
    bool bInComplexShape = false;

    for( int i = 0; i < nElementCount; i++ )
    {
        if( pasIndex[i].flags & DGNEIF_DELETED )
            continue;

        switch( pasIndex[i].stype )
        {
          case DGNST_MULTIPOINT:
          case DGNST_ARC:
          case DGNST_TEXT:
            if( !(pasIndex[i].flags & DGNEIF_COMPLEX) || !bInComplexShape )
            {
                nFeatureCount++;
                bInComplexShape = FALSE;
            }
            break;

          case DGNST_COMPLEX_HEADER:
            nFeatureCount++;
            bInComplexShape = TRUE;
            break;

          default:
            break;
        }
    }

    return nFeatureCount;
}

/************************************************************************/
/*                             GetExtent()                              */
/************************************************************************/

OGRErr OGRDGNLayer::GetExtent( OGREnvelope *psExtent, int /* bForce */ )
{
    double adfExtents[6];

    if( !DGNGetExtents( hDGN, adfExtents ) )
        return OGRERR_FAILURE;

    psExtent->MinX = adfExtents[0];
    psExtent->MinY = adfExtents[1];
    psExtent->MaxX = adfExtents[3];
    psExtent->MaxY = adfExtents[4];

    return OGRERR_NONE;
}

/************************************************************************/
/*                      LineStringToElementGroup()                      */
/*                                                                      */
/*      Convert an OGR line string to one or more DGN elements.  If     */
/*      the input is too long for a single element (more than 38        */
/*      points) we split it into multiple LINE_STRING elements, and     */
/*      prefix with a complex group header element.                     */
/*                                                                      */
/*      This method can create handle creating shapes, or line          */
/*      strings for the aggregate object, but the components of a       */
/*      complex shape group are always line strings.                    */
/************************************************************************/

static const int MAX_ELEM_POINTS = 38;

DGNElemCore **OGRDGNLayer::LineStringToElementGroup( OGRLineString *poLS,
                                                     int nGroupType )

{
    int nTotalPoints = poLS->getNumPoints();
    int iNextPoint = 0;
    int iGeom = 0;
    DGNElemCore **papsGroup = static_cast<DGNElemCore **>(
        CPLCalloc( sizeof(void*), (nTotalPoints/(MAX_ELEM_POINTS-1))+3 ) );

    for( iNextPoint = 0; iNextPoint < nTotalPoints;  )
    {
        DGNPoint asPoints[MAX_ELEM_POINTS];
        int nThisCount = 0;

        // we need to repeat end points of elements.
        if( iNextPoint != 0 )
            iNextPoint--;

        for( ; iNextPoint < nTotalPoints && nThisCount < MAX_ELEM_POINTS;
             iNextPoint++, nThisCount++ )
        {
            asPoints[nThisCount].x = poLS->getX( iNextPoint );
            asPoints[nThisCount].y = poLS->getY( iNextPoint );
            asPoints[nThisCount].z = poLS->getZ( iNextPoint );
        }

        if( nTotalPoints <= MAX_ELEM_POINTS )
            papsGroup[0] = DGNCreateMultiPointElem( hDGN, nGroupType,
                                                 nThisCount, asPoints);
        else
            papsGroup[++iGeom] =
                DGNCreateMultiPointElem( hDGN, DGNT_LINE_STRING,
                                         nThisCount, asPoints);
    }

/* -------------------------------------------------------------------- */
/*      We needed to make into a group.  Create the complex header      */
/*      from the rest of the group.                                     */
/* -------------------------------------------------------------------- */
    if( papsGroup[0] == NULL )
    {
        if( nGroupType == DGNT_SHAPE )
            nGroupType = DGNT_COMPLEX_SHAPE_HEADER;
        else
            nGroupType = DGNT_COMPLEX_CHAIN_HEADER;

        papsGroup[0] =
            DGNCreateComplexHeaderFromGroup( hDGN, nGroupType,
                                             iGeom, papsGroup + 1 );
    }

    return papsGroup;
}

/************************************************************************/
/*                           TranslateLabel()                           */
/*                                                                      */
/*      Translate LABEL feature.                                        */
/************************************************************************/

DGNElemCore **OGRDGNLayer::TranslateLabel( OGRFeature *poFeature )

{
    OGRPoint *poPoint
        = reinterpret_cast<OGRPoint *>( poFeature->GetGeometryRef() );
    const char *pszText = poFeature->GetFieldAsString( "Text" );

    OGRStyleMgr oMgr;
    oMgr.InitFromFeature( poFeature );
    OGRStyleLabel *poLabel
        = reinterpret_cast<OGRStyleLabel *>( oMgr.GetPart( 0 ) );
    if( poLabel != NULL && poLabel->GetType() != OGRSTCLabel )
    {
        delete poLabel;
        poLabel = NULL;
    }

    double dfRotation = 0.0;
    double dfCharHeight = 100.0;
    int nFontID = 1; // 1 is the default font for DGN. Not 0.

    if( poLabel != NULL )
    {
        GBool bDefault;

        if( poLabel->TextString(bDefault) != NULL && !bDefault )
            pszText = poLabel->TextString(bDefault);
        dfRotation = poLabel->Angle(bDefault);

        poLabel->Size( bDefault );
        if( !bDefault && poLabel->GetUnit() == OGRSTUGround )
            dfCharHeight = poLabel->Size(bDefault);
        // this part is really kind of bogus.
        if( !bDefault && poLabel->GetUnit() == OGRSTUMM )
            dfCharHeight = poLabel->Size(bDefault)/1000.0;

        /* get font id */
        static const char * const papszFontNumbers[] =
          { "STANDARD=0", "WORKING=1", "FANCY=2", "ENGINEERING=3", "NEWZERO=4",
            "STENCEL=5", "USTN_FANCY=7", "COMPRESSED=8", "STENCEQ=9", "hand=10",
            "ARCH=11", "ARCHB=12", "IGES1001=15", "IGES1002=16", "IGES1003=17",
            "CENTB=18", "MICROS=19", "ISOFRACTIONS=22", "ITALICS=23",
            "ISO30=24", "GREEK=25", "ISOREC=26", "Isoeq=27", "ISO_FONTLEFT=30",
            "ISO_FONTRIGHT=31", "INTL_ENGINEERING=32", "INTL_WORKING=33",
            "ISOITEQ=34", "USTN FONT 26=36", "ARCHITECTURAL=41",
            "BLOCK_OUTLINE=42", "LOW_RES_FILLED=43", "UPPERCASE50",
            "FONT060=60", "din=61", "dinit=62", "helvl=63", "HELVLIT=64",
            "helv=65", "HELVIT=66", "cent=67", "CENTIT=68", "SCRIPT=69",
            "MICROQ=76", "dotfont=77", "DOTIT=78", "FONT092=92", "FONT094=94",
            "ANSI_SYMBOLS=100", "FEATURE_CONTROL_SYSMBOLS=101", "SYMB_FAST=102",
            "INTL_ISO=105", "INTL_ISO_EQUAL=106", "INTL_ISO_ITALIC=107",
            "INTL_ISO_ITALIC_EQUAL=108", NULL };

        const char *pszFontName = poLabel->FontName( bDefault );
        if( !bDefault && pszFontName != NULL )
        {
            const char *pszFontNumber =
              CSLFetchNameValue( const_cast<char**>( papszFontNumbers ),
                                 pszFontName );

            if( pszFontNumber != NULL )
            {
                nFontID = atoi( pszFontNumber );
            }
        }
    }

    DGNElemCore **papsGroup
        = static_cast<DGNElemCore **>( CPLCalloc(sizeof(void*), 2) );
    papsGroup[0] =
        DGNCreateTextElem( hDGN, pszText, nFontID, DGNJ_LEFT_BOTTOM,
                           dfCharHeight, dfCharHeight, dfRotation, NULL,
                           poPoint->getX(),
                           poPoint->getY(),
                           poPoint->getZ() );

    if( poLabel )
        delete poLabel;

    return papsGroup;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/*                                                                      */
/*      Create a new feature and write to file.                         */
/************************************************************************/

OGRErr OGRDGNLayer::ICreateFeature( OGRFeature *poFeature )

{
    if( !bUpdate )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create feature on read-only DGN file." );
        return OGRERR_FAILURE;
    }

    if( poFeature->GetGeometryRef() == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Features with empty, geometry collection geometries not\n"
                  "supported in DGN format." );
        return OGRERR_FAILURE;
    }

    return CreateFeatureWithGeom( poFeature, poFeature->GetGeometryRef() );
}

/************************************************************************/
/*                       CreateFeatureWithGeom()                        */
/*                                                                      */
/*      Create an element or element group from a given geometry and    */
/*      the given feature.  This method recurses to handle              */
/*      collections as essentially independent features.                */
/************************************************************************/

OGRErr OGRDGNLayer::CreateFeatureWithGeom( OGRFeature *poFeature,
                                           OGRGeometry *poGeom)

{
/* -------------------------------------------------------------------- */
/*      Translate the geometry.                                         */
/* -------------------------------------------------------------------- */
    DGNElemCore **papsGroup = NULL;
    const char *pszStyle = poFeature->GetStyleString();

    if( wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
    {
        OGRPoint *poPoint = reinterpret_cast<OGRPoint *>( poGeom );
        const char *pszText = poFeature->GetFieldAsString("Text");

        if( (pszText == NULL || strlen(pszText) == 0)
            && (pszStyle == NULL || strstr(pszStyle,"LABEL") == NULL) )
        {
            // Treat a non text point as a degenerate line.
            DGNPoint asPoints[2];
            asPoints[0].x = poPoint->getX();
            asPoints[0].y = poPoint->getY();
            asPoints[0].z = poPoint->getZ();
            asPoints[1] = asPoints[0];

            papsGroup
                = static_cast<DGNElemCore **>( CPLCalloc(sizeof(void*), 2) );
            papsGroup[0] = DGNCreateMultiPointElem( hDGN, DGNT_LINE,
                                                    2, asPoints );
        }
        else
        {
            papsGroup = TranslateLabel( poFeature );
        }
    }
    else if( wkbFlatten(poGeom->getGeometryType()) == wkbLineString )
    {
        papsGroup = LineStringToElementGroup(
            reinterpret_cast<OGRLineString *>( poGeom ),
            DGNT_LINE_STRING );
    }
    else if( wkbFlatten(poGeom->getGeometryType()) == wkbPolygon )
    {
        OGRPolygon *poPoly = reinterpret_cast<OGRPolygon *>( poGeom );

        DGNElemCore **papsGroupExt = LineStringToElementGroup(
                poPoly->getExteriorRing(), DGNT_SHAPE);

        const int innerRingsCnt = poPoly->getNumInteriorRings();

        if (innerRingsCnt > 0) {
            CPLDebug("InnerRings", "there are %d inner rings", innerRingsCnt);
            std::list<DGNElemCore*> dgnElements;

            for( int i = 0; papsGroupExt[i] != NULL; i++) {
                dgnElements.push_back(papsGroupExt[i]);
            }
            CPLFree(papsGroupExt);

            // get all interior rings and create complex group shape
            for (int iRing = 0; iRing < innerRingsCnt; iRing++) {
                DGNElemCore **papsGroupInner = LineStringToElementGroup(
                        poPoly->getInteriorRing(iRing), DGNT_SHAPE);
                papsGroupInner[0]->properties |= DGNPF_HOLE;
                DGNUpdateElemCoreExtended(hDGN, papsGroupInner[0]);
                for( int i = 0; papsGroupInner[i] != NULL; i++ ) {
                    dgnElements.push_back(papsGroupInner[i]);
                }
                CPLFree(papsGroupInner);
            }
            int index = 1;
            papsGroup = (DGNElemCore **) CPLCalloc(sizeof(void*),
                    dgnElements.size() + 2);
            for( std::list<DGNElemCore*>::iterator list_iter
                     = dgnElements.begin();
                 list_iter != dgnElements.end();
                 ++list_iter) {
                papsGroup[index++] = *list_iter;
            }

            // papsGroup[0] = DGNCreateComplexHeaderFromGroup(
            //     hDGN, DGNT_COMPLEX_SHAPE_HEADER, dgnElements.size(),
            //     papsGroup+1 );
            DGNPoint asPoints[1];
            asPoints[0].x = 0;
            asPoints[0].y = 0;
            asPoints[0].z = 0;
            papsGroup[0] = DGNCreateCellHeaderFromGroup(
                hDGN, "", 1, NULL,
                static_cast<int>(dgnElements.size()), papsGroup + 1,
                asPoints + 0, 1.0, 1.0, 0.0) ;
            DGNAddShapeFillInfo(hDGN, papsGroup[0], 6);
        } else {
            papsGroup = papsGroupExt;
        }
    }
    else if( wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon
             || wkbFlatten(poGeom->getGeometryType()) == wkbMultiPoint
             || wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString
             || wkbFlatten(poGeom->getGeometryType()) == wkbGeometryCollection)
    {
        OGRGeometryCollection *poGC = (OGRGeometryCollection *) poGeom;

        for( int iGeom = 0; iGeom < poGC->getNumGeometries(); iGeom++ )
        {
            OGRErr eErr = CreateFeatureWithGeom( poFeature,
                                                 poGC->getGeometryRef(iGeom) );
            if( eErr != OGRERR_NONE )
                return eErr;
        }

        return OGRERR_NONE;
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Unsupported geometry type (%s) for DGN.",
                  OGRGeometryTypeToName( poGeom->getGeometryType() ) );
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Add other attributes.                                           */
/* -------------------------------------------------------------------- */
    int nLevel = poFeature->GetFieldAsInteger( "Level" );
    int nGraphicGroup = poFeature->GetFieldAsInteger( "GraphicGroup" );
    int nColor = poFeature->GetFieldAsInteger( "ColorIndex" );
    int nWeight = poFeature->GetFieldAsInteger( "Weight" );
    int nStyle = poFeature->GetFieldAsInteger( "Style" );
    int nMSLink = poFeature->GetFieldAsInteger( "MSLink" );

    // TODO: Use std::max and std::min.
    nLevel = MAX(0,MIN(63,nLevel));
    nColor = MAX(0,MIN(255,nColor));
    nWeight = MAX(0,MIN(31,nWeight));
    nStyle = MAX(0,MIN(7,nStyle));
    nMSLink = MAX(0,nMSLink);

    DGNUpdateElemCore( hDGN, papsGroup[0], nLevel, nGraphicGroup, nColor,
                       nWeight, nStyle );
    DGNAddMSLink( hDGN, papsGroup[0], DGNLT_ODBC, 0, nMSLink );
/* -------------------------------------------------------------------- */
/*      Write to file.                                                  */
/* -------------------------------------------------------------------- */
    for( int i = 0; papsGroup[i] != NULL; i++ )
    {
        DGNWriteElement( hDGN, papsGroup[i] );

        if( i == 0 )
            poFeature->SetFID( papsGroup[i]->element_id );

        DGNFreeElement( hDGN, papsGroup[i] );
    }

    CPLFree( papsGroup );

    return OGRERR_NONE;
}
