/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
 * Copyright (c) 2011-2014, Even Rouault <even dot rouault at mines-paris dot org>
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
 *****************************************************************************/

#include "libkml_headers.h"

#include <set>
#include <string>

#include "ogr_featurestyle.h"
#include "ogrlibkmlstyle.h"
#include "ogr_libkml.h"

CPL_CVSID("$Id$")

using kmlbase::Color32;
using kmldom::BalloonStylePtr;
using kmldom::ContainerPtr;
using kmldom::DocumentPtr;
using kmldom::ElementPtr;
using kmldom::FeaturePtr;
using kmldom::HotSpotPtr;
using kmldom::ItemIconPtr;
using kmldom::IconStyleIconPtr;
using kmldom::IconStylePtr;
using kmldom::KmlFactory;
using kmldom::KmlPtr;
using kmldom::LabelStylePtr;
using kmldom::LineStylePtr;
using kmldom::ListStylePtr;
using kmldom::ObjectPtr;
using kmldom::PairPtr;
using kmldom::PolyStylePtr;
using kmldom::StyleMapPtr;
using kmldom::StylePtr;
using kmldom::StyleSelectorPtr;
using kmldom::STYLESTATE_HIGHLIGHT;
using kmldom::STYLESTATE_NORMAL;

/******************************************************************************
 Generic function to parse a stylestring and add to a kml style.

Args:
            pszStyleString  the stylestring to parse
            poKmlStyle      the kml style to add to (or NULL)
            poKmlFactory    the kml dom factory

Returns:
            the kml style

******************************************************************************/

StylePtr addstylestring2kml(
    const char *pszStyleString,
    StylePtr poKmlStyle,
    KmlFactory * poKmlFactory,
    FeaturePtr poKmlFeature )
{
    /***** just bail now if stylestring is empty *****/
    if( !pszStyleString || !*pszStyleString )
    {
        return poKmlStyle;
    }

    LineStylePtr poKmlLineStyle = NULL;
    PolyStylePtr poKmlPolyStyle = NULL;
    IconStylePtr poKmlIconStyle = NULL;
    LabelStylePtr poKmlLabelStyle = NULL;

    /***** create and init a style mamager with the style string *****/
    OGRStyleMgr * const poOgrSM = new OGRStyleMgr;

    poOgrSM->InitStyleString( pszStyleString );

    /***** loop though the style parts *****/
    for( int i = 0; i < poOgrSM->GetPartCount( NULL ); i++ )
    {
        OGRStyleTool *poOgrST = poOgrSM->GetPart( i, NULL );

        if( !poOgrST )
        {
            continue;
        }

        switch( poOgrST->GetType() )
        {
            case OGRSTCPen:
            {
                poKmlLineStyle = poKmlFactory->CreateLineStyle();

                OGRStylePen *poStylePen = dynamic_cast<OGRStylePen *>(poOgrST);
                if( poStylePen == NULL )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "dynamic_cast failed.");
                    delete poOgrSM;
                    return NULL;
                }

                /***** pen color *****/
                GBool nullcheck = FALSE;
                const char * const pszColor = poStylePen->Color( nullcheck );

                int nR = 0;
                int nG = 0;
                int nB = 0;
                int nA = 0;
                if( !nullcheck &&
                    poStylePen->GetRGBFromString( pszColor, nR, nG, nB, nA ) )
                {
                    poKmlLineStyle->set_color(
                        Color32( static_cast<GByte>(nA),
                                 static_cast<GByte>(nB),
                                 static_cast<GByte>(nG),
                                 static_cast<GByte>(nR) ) );
                }
                poStylePen->SetUnit(OGRSTUPixel);
                double dfWidth = poStylePen->Width( nullcheck );

                if( nullcheck )
                    dfWidth = 1.0;

                poKmlLineStyle->set_width( dfWidth );

                break;
            }
            case OGRSTCBrush:
            {
                OGRStyleBrush * const poStyleBrush =
                    dynamic_cast<OGRStyleBrush *>(poOgrST);
                if( poStyleBrush == NULL )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "dynamic_cast failed.");
                    delete poOgrSM;
                    return NULL;
                }

                /***** brush color *****/
                GBool nullcheck = FALSE;
                const char *pszColor = poStyleBrush->ForeColor( nullcheck );

                int nR = 0;
                int nG = 0;
                int nB = 0;
                int nA = 0;
                if( !nullcheck &&
                    poStyleBrush->GetRGBFromString( pszColor, nR, nG, nB, nA ) )
                {
                    poKmlPolyStyle = poKmlFactory->CreatePolyStyle();
                    poKmlPolyStyle->set_color(
                        Color32( static_cast<GByte>(nA),
                                 static_cast<GByte>(nB),
                                 static_cast<GByte>(nG),
                                 static_cast<GByte>(nR) ) );
                }
                break;
            }
            case OGRSTCSymbol:
            {
                OGRStyleSymbol * const poStyleSymbol =
                    dynamic_cast<OGRStyleSymbol *>(poOgrST);
                if( poStyleSymbol == NULL )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "dynamic_cast failed.");
                    delete poOgrSM;
                    return NULL;
                }

                /***** id (kml icon) *****/
                GBool nullcheck = FALSE;
                const char *pszId = poStyleSymbol->Id( nullcheck );

                if( !nullcheck )
                {
                    if( !poKmlIconStyle)
                        poKmlIconStyle = poKmlFactory->CreateIconStyle();

                    /***** split it at the ,'s *****/
                    char **papszTokens =
                        CSLTokenizeString2(
                            pszId, ",",
                            CSLT_HONOURSTRINGS | CSLT_STRIPLEADSPACES |
                            CSLT_STRIPENDSPACES );

                    if( papszTokens )
                    {
                        // Just take the first one.
                        // TODO: Come up with a better idea.
                        if( papszTokens[0] )
                        {
                            IconStyleIconPtr poKmlIcon =
                                poKmlFactory->CreateIconStyleIcon();
                            poKmlIcon->set_href( papszTokens[0] );
                            poKmlIconStyle->set_icon( poKmlIcon );
                        }

                        CSLDestroy( papszTokens );
                    }
                }

                /***** heading *****/
                double heading = poStyleSymbol->Angle( nullcheck );

                if( !nullcheck )
                {
                    if( !poKmlIconStyle)
                        poKmlIconStyle = poKmlFactory->CreateIconStyle();
                    poKmlIconStyle->set_heading( heading );
                }

                /***** scale *****/
                double dfScale = poStyleSymbol->Size( nullcheck );

                if( !nullcheck )
                {
                    if( !poKmlIconStyle )
                        poKmlIconStyle = poKmlFactory->CreateIconStyle();

                    poKmlIconStyle->set_scale( dfScale );
                }

                /***** color *****/
                const char * const pszcolor =
                    poStyleSymbol->Color( nullcheck );

                int nR = 0;
                int nG = 0;
                int nB = 0;
                int nA = 0;
                if( !nullcheck &&
                    poOgrST->GetRGBFromString( pszcolor, nR, nG, nB, nA ) )
                {
                    poKmlIconStyle->set_color(
                        Color32 ( static_cast<GByte>(nA),
                                  static_cast<GByte>(nB),
                                  static_cast<GByte>(nG),
                                  static_cast<GByte>(nR) ) );
                }

                /***** hotspot *****/

                double dfDx = poStyleSymbol->SpacingX( nullcheck );
                GBool nullcheck2 = FALSE;
                double dfDy = poStyleSymbol->SpacingY( nullcheck2 );

                if( !nullcheck && !nullcheck2 )
                {
                    if( !poKmlIconStyle)
                        poKmlIconStyle = poKmlFactory->CreateIconStyle();

                    HotSpotPtr poKmlHotSpot = poKmlFactory->CreateHotSpot();

                    poKmlHotSpot->set_x( dfDx );
                    poKmlHotSpot->set_y( dfDy );

                    poKmlIconStyle->set_hotspot( poKmlHotSpot );
                }

                break;
            }
        case OGRSTCLabel:
            {
                GBool nullcheck;
                GBool nullcheck2;

                OGRStyleLabel *poStyleLabel =
                    dynamic_cast<OGRStyleLabel *>(poOgrST);
                if( poStyleLabel == NULL )
                {
                    CPLError(CE_Failure, CPLE_AppDefined,
                             "dynamic_cast failed.");
                    delete poOgrSM;
                    return NULL;
                }

                /***** color *****/
                const char *pszcolor = poStyleLabel->ForeColor( nullcheck );

                int nR = 0;
                int nG = 0;
                int nB = 0;
                int nA = 0;
                if( !nullcheck &&
                    poStyleLabel->GetRGBFromString( pszcolor, nR, nG, nB, nA ) )
                {
                    if( poKmlLabelStyle == NULL )
                        poKmlLabelStyle = poKmlFactory->CreateLabelStyle();
                    poKmlLabelStyle->set_color(
                        Color32 ( static_cast<GByte>(nA),
                                  static_cast<GByte>(nB),
                                  static_cast<GByte>(nG),
                                  static_cast<GByte>(nR) ) );
                }

                /***** scale *****/
                double dfScale = poStyleLabel->Stretch( nullcheck );

                if( !nullcheck )
                {
                    dfScale /= 100.0;
                    if( poKmlLabelStyle == NULL )
                        poKmlLabelStyle = poKmlFactory->CreateLabelStyle();
                    poKmlLabelStyle->set_scale( dfScale );
                }

                /***** heading *****/
                const double heading = poStyleLabel->Angle( nullcheck );

                if( !nullcheck )
                {
                    if( !poKmlIconStyle)
                    {
                        poKmlIconStyle = poKmlFactory->CreateIconStyle();
                        const IconStyleIconPtr poKmlIcon =
                            poKmlFactory->CreateIconStyleIcon();
                        poKmlIconStyle->set_icon( poKmlIcon );
                    }

                    poKmlIconStyle->set_heading( heading );
                }

                /***** hotspot *****/
                const double dfDx = poStyleLabel->SpacingX( nullcheck );
                const double dfDy = poStyleLabel->SpacingY( nullcheck2 );

                if( !nullcheck && !nullcheck2 )
                {
                    if( !poKmlIconStyle)
                    {
                        poKmlIconStyle = poKmlFactory->CreateIconStyle();
                        const IconStyleIconPtr poKmlIcon =
                            poKmlFactory->CreateIconStyleIcon();
                        poKmlIconStyle->set_icon( poKmlIcon );
                    }

                    HotSpotPtr poKmlHotSpot = poKmlFactory->CreateHotSpot();

                    poKmlHotSpot->set_x( dfDx );
                    poKmlHotSpot->set_y( dfDy );

                    poKmlIconStyle->set_hotspot( poKmlHotSpot );
                }

                /***** label text *****/
                const char * const pszText =
                    poStyleLabel->TextString( nullcheck );

                if( !nullcheck && poKmlFeature != NULL )
                {
                        poKmlFeature->set_name( pszText );
                }

                break;
            }
            case OGRSTCNone:
            default:
            {
                break;
            }
        }

        delete poOgrST;
    }

    if( poKmlLineStyle || poKmlPolyStyle || poKmlIconStyle || poKmlLabelStyle )
    {
        if( poKmlStyle == NULL )
            poKmlStyle = poKmlFactory->CreateStyle();

        if( poKmlLineStyle )
            poKmlStyle->set_linestyle( poKmlLineStyle );

        if( poKmlPolyStyle )
            poKmlStyle->set_polystyle( poKmlPolyStyle );

        if( poKmlIconStyle )
            poKmlStyle->set_iconstyle( poKmlIconStyle );

        if( poKmlLabelStyle )
            poKmlStyle->set_labelstyle( poKmlLabelStyle );
    }

    delete poOgrSM;

    return poKmlStyle;
}

/******************************************************************************
 kml2pen
******************************************************************************/

static
OGRStylePen *kml2pen( LineStylePtr poKmlLineStyle, OGRStylePen *poOgrStylePen )
{
    if( !poOgrStylePen )
        poOgrStylePen = new OGRStylePen();

    /***** <LineStyle> should always have a width in pixels *****/
    poOgrStylePen->SetUnit(OGRSTUPixel);

    /***** width *****/
    if( poKmlLineStyle->has_width() )
        poOgrStylePen->SetWidth( poKmlLineStyle->get_width() );

    /***** color *****/
    if( poKmlLineStyle->has_color() )
    {
        Color32 poKmlColor = poKmlLineStyle->get_color();
        char szColor[10] = {};
        snprintf( szColor, sizeof( szColor ), "#%02X%02X%02X%02X",
                  poKmlColor.get_red(),
                  poKmlColor.get_green(),
                  poKmlColor.get_blue(),
                  poKmlColor.get_alpha() );
        poOgrStylePen->SetColor( szColor );
    }

    return poOgrStylePen;
}

/******************************************************************************
 kml2brush
******************************************************************************/

static
OGRStyleBrush *kml2brush(
    PolyStylePtr poKmlPolyStyle,
    OGRStyleBrush *poOgrStyleBrush )
{
    if( !poOgrStyleBrush )
        poOgrStyleBrush = new OGRStyleBrush();

    /***** color *****/
    if( poKmlPolyStyle->has_color() )
    {
        Color32 poKmlColor = poKmlPolyStyle->get_color();
        char szColor[10] = {};
        snprintf( szColor, sizeof( szColor ), "#%02X%02X%02X%02X",
                  poKmlColor.get_red(),
                  poKmlColor.get_green(),
                  poKmlColor.get_blue(),
                  poKmlColor.get_alpha() );
        poOgrStyleBrush->SetForeColor( szColor );
    }

    return poOgrStyleBrush;
}

/******************************************************************************
 kml2symbol
******************************************************************************/

static
OGRStyleSymbol *kml2symbol(
    IconStylePtr poKmlIconStyle,
    OGRStyleSymbol *poOgrStyleSymbol )
{
    if( !poOgrStyleSymbol )
        poOgrStyleSymbol = new OGRStyleSymbol();

    /***** id (kml icon) *****/
    if( poKmlIconStyle->has_icon() )
    {
        IconStyleIconPtr poKmlIcon = poKmlIconStyle->get_icon();

        if( poKmlIcon->has_href() )
        {
            std::string oIcon = "\"";
            oIcon.append( poKmlIcon->get_href().c_str() );
            oIcon.append( "\"" );
            poOgrStyleSymbol->SetId( oIcon.c_str() );
        }
    }

    /***** heading *****/
    if( poKmlIconStyle->has_heading() )
        poOgrStyleSymbol->SetAngle( poKmlIconStyle->get_heading() );

    /***** scale *****/
    if( poKmlIconStyle->has_scale() )
        poOgrStyleSymbol->SetSize( poKmlIconStyle->get_scale() );

    /***** color *****/
    if( poKmlIconStyle->has_color() )
    {
        Color32 poKmlColor = poKmlIconStyle->get_color();
        char szColor[10] = {};
        snprintf( szColor, sizeof( szColor ), "#%02X%02X%02X%02X",
                  poKmlColor.get_red(),
                  poKmlColor.get_green(),
                  poKmlColor.get_blue(),
                  poKmlColor.get_alpha() );
        poOgrStyleSymbol->SetColor( szColor );
    }

    /***** hotspot *****/
    if( poKmlIconStyle->has_hotspot() )
    {
        const HotSpotPtr poKmlHotSpot = poKmlIconStyle->get_hotspot();

        if( poKmlHotSpot->has_x() )
            poOgrStyleSymbol->SetSpacingX( poKmlHotSpot->get_x() );
        if( poKmlHotSpot->has_y() )
            poOgrStyleSymbol->SetSpacingY( poKmlHotSpot->get_y() );
    }

    return poOgrStyleSymbol;
}

/******************************************************************************
 kml2label
******************************************************************************/

static
OGRStyleLabel *kml2label(
    LabelStylePtr poKmlLabelStyle,
    OGRStyleLabel *poOgrStyleLabel )
{
    if( !poOgrStyleLabel )
        poOgrStyleLabel = new OGRStyleLabel();

    /***** color *****/
    if( poKmlLabelStyle->has_color() )
    {
        Color32 poKmlColor = poKmlLabelStyle->get_color();
        char szColor[10] = {};
        snprintf( szColor, sizeof( szColor ), "#%02X%02X%02X%02X",
                  poKmlColor.get_red(),
                  poKmlColor.get_green(),
                  poKmlColor.get_blue(),
                  poKmlColor.get_alpha() );
        poOgrStyleLabel->SetForColor( szColor );
    }

    if( poKmlLabelStyle->has_scale() )
    {
        double dfScale = poKmlLabelStyle->get_scale();
        dfScale *= 100.0;

        poOgrStyleLabel->SetStretch(dfScale);
    }

    return poOgrStyleLabel;
}

/******************************************************************************
 Function to add a kml style to a style table.
******************************************************************************/

static void kml2styletable(
    OGRStyleTable * poOgrStyleTable,
    StylePtr poKmlStyle )
{
    /***** No reason to add it if it don't have an id. *****/
    if( !poKmlStyle->has_id() )
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "ERROR parsing kml Style: No id" );
        return;
    }

    OGRStyleMgr *poOgrSM = new OGRStyleMgr( poOgrStyleTable );

    poOgrSM->InitStyleString( NULL );

    /***** read the style *****/
    kml2stylestring( poKmlStyle, poOgrSM );

    /***** add the style to the style table *****/
    const std::string oName = poKmlStyle->get_id();

    poOgrSM->AddStyle(
        CPLString().Printf( "%s", oName.c_str() ), NULL );

    /***** Cleanup the style manager. *****/
    delete poOgrSM;
}

/******************************************************************************
 Function to follow the kml stylemap if one exists.
******************************************************************************/

StyleSelectorPtr StyleFromStyleSelector(
    const StyleSelectorPtr& poKmlStyleSelector,
    OGRStyleTable * poStyleTable )
{
    /***** Is it a style? *****/
    if( poKmlStyleSelector->IsA( kmldom::Type_Style) )
        return poKmlStyleSelector;

    /***** Is it a style map? *****/

    else if( poKmlStyleSelector->IsA( kmldom::Type_StyleMap ) )
        return StyleFromStyleMap(
            kmldom::AsStyleMap(poKmlStyleSelector), poStyleTable);

    /***** Not a style or a style map. *****/
    return NULL;
}

/******************************************************************************
 kml2stylemgr
******************************************************************************/

void kml2stylestring( StylePtr poKmlStyle, OGRStyleMgr * poOgrSM )

{
    OGRStyleMgr * const poOgrNewSM = new OGRStyleMgr( NULL );

    /***** linestyle / pen *****/
    if( poKmlStyle->has_linestyle() )
    {
        poOgrNewSM->InitStyleString( NULL );

        LineStylePtr poKmlLineStyle = poKmlStyle->get_linestyle();

        OGRStyleTool *poOgrTmpST = NULL;
        for( int i = 0; i < poOgrSM->GetPartCount( NULL ); i++ )
        {
            OGRStyleTool *poOgrST = poOgrSM->GetPart( i, NULL );

            if( !poOgrST )
                continue;

            if( poOgrST->GetType() == OGRSTCPen )
            {
                poOgrTmpST = poOgrST;
            }
            else
            {
                poOgrNewSM->AddPart( poOgrST );
                delete poOgrST;
            }
        }

        OGRStylePen *poOgrStylePen =
            kml2pen( poKmlLineStyle,
                     ( OGRStylePen *) poOgrTmpST);

        poOgrNewSM->AddPart( poOgrStylePen );

        delete poOgrStylePen;
        poOgrSM->InitStyleString( poOgrNewSM->GetStyleString(NULL) );
    }

    /***** polystyle / brush *****/
    if( poKmlStyle->has_polystyle() )
    {
        poOgrNewSM->InitStyleString( NULL );

        PolyStylePtr poKmlPolyStyle = poKmlStyle->get_polystyle();

        OGRStyleTool *poOgrTmpST = NULL;
        for( int i = 0; i < poOgrSM->GetPartCount( NULL ); i++ )
        {
            OGRStyleTool *poOgrST = poOgrSM->GetPart( i, NULL );

            if( !poOgrST )
                continue;

            if( poOgrST->GetType() == OGRSTCBrush )
            {
                poOgrTmpST = poOgrST;
            }
            else
            {
                poOgrNewSM->AddPart( poOgrST );
                delete poOgrST;
            }
        }

        OGRStyleBrush *poOgrStyleBrush =
            kml2brush( poKmlPolyStyle,
                       ( OGRStyleBrush *) poOgrTmpST );

        poOgrNewSM->AddPart( poOgrStyleBrush );

        delete poOgrStyleBrush;
        poOgrSM->InitStyleString( poOgrNewSM->GetStyleString(NULL) );
    }

    /***** iconstyle / symbol *****/
    if( poKmlStyle->has_iconstyle() )
    {
        poOgrNewSM->InitStyleString( NULL );

        IconStylePtr poKmlIconStyle = poKmlStyle->get_iconstyle();

        OGRStyleTool *poOgrTmpST = NULL;
        for( int i = 0; i < poOgrSM->GetPartCount( NULL ); i++ )
        {
            OGRStyleTool *poOgrST = poOgrSM->GetPart( i, NULL );

            if( !poOgrST )
                continue;

            if( poOgrST->GetType() == OGRSTCSymbol )
            {
                poOgrTmpST = poOgrST;
            }
            else
            {
                poOgrNewSM->AddPart( poOgrST );
                delete poOgrST;
            }
        }

        OGRStyleSymbol *poOgrStyleSymbol =
            kml2symbol( poKmlIconStyle,
                        ( OGRStyleSymbol *) poOgrTmpST );

        poOgrNewSM->AddPart( poOgrStyleSymbol );

        delete poOgrStyleSymbol;
        poOgrSM->InitStyleString( poOgrNewSM->GetStyleString(NULL) );
    }

    /***** labelstyle / label *****/
    if( poKmlStyle->has_labelstyle() )
    {
        poOgrNewSM->InitStyleString( NULL );

        LabelStylePtr poKmlLabelStyle = poKmlStyle->get_labelstyle();

        OGRStyleTool *poOgrTmpST = NULL;
        for( int i = 0; i < poOgrSM->GetPartCount( NULL ); i++ )
        {
            OGRStyleTool *poOgrST = poOgrSM->GetPart( i, NULL );

            if( !poOgrST )
                continue;

            if( poOgrST->GetType() == OGRSTCLabel )
            {
                poOgrTmpST = poOgrST;
            }
            else
            {
                poOgrNewSM->AddPart( poOgrST );
                delete poOgrST;
            }
        }

        OGRStyleLabel *poOgrStyleLabel =
            kml2label( poKmlLabelStyle,
                       ( OGRStyleLabel *) poOgrTmpST );

        poOgrNewSM->AddPart( poOgrStyleLabel );

        delete poOgrStyleLabel;
        poOgrSM->InitStyleString( poOgrNewSM->GetStyleString(NULL) );
    }

    delete poOgrNewSM;
}

/******************************************************************************
 Function to get the container from the kmlroot.

 Args:          poKmlRoot   the root element

 Returns:       root if its a container, if its a kml the container it
                contains, or NULL

******************************************************************************/

static ContainerPtr MyGetContainerFromRoot(
    KmlFactory *m_poKmlFactory, ElementPtr poKmlRoot )
{
    ContainerPtr poKmlContainer = NULL;

    if( poKmlRoot )
    {
        /***** skip over the <kml> we want the container *****/
        if( poKmlRoot->IsA( kmldom::Type_kml ) )
        {
            KmlPtr poKmlKml = AsKml( poKmlRoot );

            if( poKmlKml->has_feature() )
            {
                FeaturePtr poKmlFeat = poKmlKml->get_feature();

                if( poKmlFeat->IsA( kmldom::Type_Container ) )
                {
                    poKmlContainer = AsContainer( poKmlFeat );
                }
                else if( poKmlFeat->IsA( kmldom::Type_Placemark ) )
                {
                    poKmlContainer = m_poKmlFactory->CreateDocument();
                    poKmlContainer->add_feature(
                        kmldom::AsFeature(kmlengine::Clone(poKmlFeat)) );
                }
            }
        }
        else if( poKmlRoot->IsA( kmldom::Type_Container ) )
        {
            poKmlContainer = AsContainer( poKmlRoot );
        }
    }

    return poKmlContainer;
}

static StyleSelectorPtr StyleFromStyleURL(
    const StyleMapPtr& stylemap,
    const string& styleurl,
    OGRStyleTable * poStyleTable )
{
    // TODO:: Parse the styleURL.
    char *pszUrl = CPLStrdup( styleurl.c_str() );
    char *pszStyleMapId = CPLStrdup( stylemap->get_id().c_str() );

    /***** Is it an internal style ref that starts with a #? *****/
    if( *pszUrl == '#' && poStyleTable )
    {
        /***** Search the style table for the style we *****/
        /***** want and copy it back into the table.   *****/
        const char *pszTest = poStyleTable->Find( pszUrl + 1 );
        if( pszTest )
        {
            poStyleTable->AddStyle(pszStyleMapId, pszTest);
        }
    }

    /***** We have a real URL and need to go out and fetch it *****/
    /***** FIXME this could be a relative path in a kmz *****/
    else if( strchr(pszUrl, '#') )
    {
        const char *pszFetch =
            CPLGetConfigOption( "LIBKML_EXTERNAL_STYLE", "no" );
        if( CPLTestBool(pszFetch) )
        {
            /***** Lets go out and fetch the style from the external URL *****/
            char *pszUrlTmp = CPLStrdup(pszUrl);
            char *pszPound = strchr(pszUrlTmp, '#');
            char *pszRemoteStyleName = NULL;
            // Chop off the stuff (style id) after the URL
            if( pszPound != NULL )
            {
                *pszPound = '\0';
                pszRemoteStyleName = pszPound + 1;
            }

            /***** try it as a url then a file *****/
            VSILFILE *fp = NULL;
            if( (fp = VSIFOpenL( CPLFormFilename( "/vsicurl/",
                                                   pszUrlTmp,
                                                   NULL), "r" )) != NULL
                 ||  (fp = VSIFOpenL( pszUrlTmp, "r" )) != NULL )
            {
                char szbuf[1025] = {};
                std::string oStyle = "";

                /***** loop, read and copy to a string *****/
                do {
                    const size_t nRead =
                        VSIFReadL(szbuf, 1, sizeof(szbuf) - 1, fp);
                    if(nRead == 0)
                        break;

                    /***** copy buf to the string *****/
                    szbuf[nRead] = '\0';
                    oStyle.append( szbuf );
                } while (!VSIFEofL(fp));

                VSIFCloseL(fp);

                /***** parse the kml into the dom *****/
                std::string oKmlErrors;
                ElementPtr poKmlRoot = kmldom::Parse( oStyle, &oKmlErrors );

                if( !poKmlRoot )
                {
                    CPLError( CE_Warning, CPLE_OpenFailed,
                              "ERROR parsing style kml %s :%s",
                              pszUrlTmp, oKmlErrors.c_str() );
                    CPLFree(pszUrlTmp);
                    CPLFree(pszUrl);
                    CPLFree(pszStyleMapId);

                    return NULL;
                }

                /***** get the root container *****/
                kmldom::KmlFactory* poKmlFactory =
                    kmldom::KmlFactory::GetFactory();
                ContainerPtr poKmlContainer;
                if( !( poKmlContainer =
                      MyGetContainerFromRoot( poKmlFactory, poKmlRoot ) ) )
                {
                    CPLFree(pszUrlTmp);
                    CPLFree(pszUrl);
                    CPLFree(pszStyleMapId);

                    return NULL;
                }

                /**** parse the styles into the table *****/
                ParseStyles( AsDocument( poKmlContainer ), &poStyleTable );

                /***** look for the style we need to map to in the table *****/
                const char *pszTest = NULL;
                pszTest = poStyleTable->Find(pszRemoteStyleName);

                /***** if found copy it to the table as a new style *****/
                if( pszTest )
                    poStyleTable->AddStyle(pszStyleMapId, pszTest);
            }
            CPLFree(pszUrlTmp);
        }
    }

    /***** FIXME Add support here for relative links inside KML. *****/
    CPLFree( pszUrl );
    CPLFree( pszStyleMapId );

    return NULL;
}

StyleSelectorPtr StyleFromStyleMap(
    const StyleMapPtr& poKmlStyleMap,
    OGRStyleTable * poStyleTable )
{
    /***** check the config option to see if the    *****/
    /***** user wants normal or highlighted mapping *****/
    const char *pszStyleMapKey =
        CPLGetConfigOption( "LIBKML_STYLEMAP_KEY", "normal" );
    const int nStyleMapKey =
        EQUAL(pszStyleMapKey, "highlight") ?
            STYLESTATE_HIGHLIGHT: STYLESTATE_NORMAL;

    /*****  Loop through the stylemap pairs and look for the "normal" one *****/
    for( size_t i = 0; i < poKmlStyleMap->get_pair_array_size(); ++i )
    {
        PairPtr myPair = poKmlStyleMap->get_pair_array_at(i);

        /***** is it the right one of the pair? *****/
        if( myPair->get_key() == nStyleMapKey )
        {
            if( myPair->has_styleselector() )
                return StyleFromStyleSelector(myPair->get_styleselector(),
                                              poStyleTable);
            else if(myPair->has_styleurl() )
                return StyleFromStyleURL(poKmlStyleMap, myPair->get_styleurl(),
                                         poStyleTable);
        }
    }

    return NULL;
}

/******************************************************************************
 Function to parse a style table out of a document.
******************************************************************************/

void ParseStyles(
    DocumentPtr poKmlDocument,
    OGRStyleTable ** poStyleTable )
{
    /***** if document is null just bail now *****/
    if( !poKmlDocument )
        return;

    /***** loop over the Styles *****/
    const size_t nKmlStyles = poKmlDocument->get_styleselector_array_size();

    /***** Lets first build the style table.    *****/
    /***** to begin this is just proper styles. *****/
    for( size_t iKmlStyle = 0; iKmlStyle < nKmlStyles; iKmlStyle++ )
    {
        StyleSelectorPtr poKmlStyle =
            poKmlDocument->get_styleselector_array_at( iKmlStyle );

        /***** Everything that is not a style you skip *****/
        if( !poKmlStyle->IsA( kmldom::Type_Style ) )
            continue;

        /***** We need to check to see if this is the first style. if it *****/
        /***** is we will not have a style table and need to create one  *****/

        if( !*poStyleTable )
            *poStyleTable = new OGRStyleTable();

        /***** TODO:: Not sure we need to do this as we seem *****/
        /***** to cast to element and then back to style.    *****/
        ElementPtr poKmlElement = AsElement( poKmlStyle );
        kml2styletable( *poStyleTable, AsStyle( poKmlElement ) );
    }

    /***** Now we have to loop back around and get the style maps. We    *****/
    /***** have to do this a second time since the stylemap might matter *****/
    /***** and we are just looping reference styles that are farther     *****/
    /***** down in the file. Order through the XML as it is parsed.      *****/

    for( size_t iKmlStyle = 0; iKmlStyle < nKmlStyles; iKmlStyle++ )
    {
        StyleSelectorPtr poKmlStyle =
            poKmlDocument->get_styleselector_array_at( iKmlStyle );

        /***** Everything that is not a stylemap you skip *****/
        if( !poKmlStyle->IsA( kmldom::Type_StyleMap ) )
            continue;

        /***** We need to check to see if this is the first style. if it *****/
        /***** is we will not have a style table and need to create one  *****/
        if( !*poStyleTable )
            *poStyleTable = new OGRStyleTable();

        /***** copy the style the style map points to since *****/

        char *pszStyleMapId = CPLStrdup( poKmlStyle->get_id().c_str() );
        poKmlStyle =
            StyleFromStyleMap(kmldom::AsStyleMap(poKmlStyle), *poStyleTable);
        if( poKmlStyle == NULL )
        {
            CPLFree(pszStyleMapId);
            continue;
        }
        char *pszStyleId = CPLStrdup( poKmlStyle->get_id().c_str() );

        /***** TODO:: Not sure we need to do this as we seem *****/
        /***** to cast to element and then back to style.    *****/
        ElementPtr poKmlElement = AsElement( poKmlStyle );
        kml2styletable( *poStyleTable, AsStyle( poKmlElement ) );

        // Change the name of the new style in the style table

        const char *pszTest = NULL;
        pszTest = (*poStyleTable)->Find(pszStyleId);
        // If we found the style we want in the style table we...
        if( pszTest )
        {
            (*poStyleTable)->AddStyle(pszStyleMapId, pszTest);
            (*poStyleTable)->RemoveStyle( pszStyleId );
        }
        CPLFree( pszStyleId );
        CPLFree( pszStyleMapId );
    }
}

/******************************************************************************
 Function to add a style table to a kml container.
******************************************************************************/

void styletable2kml(
    OGRStyleTable * poOgrStyleTable,
    KmlFactory * poKmlFactory,
    ContainerPtr poKmlContainer,
    char** papszOptions )
{
    /***** just return if the styletable is null *****/
    if( !poOgrStyleTable )
        return;

    std::set<CPLString> aoSetNormalStyles;
    std::set<CPLString> aoSetHighlightStyles;
    poOgrStyleTable->ResetStyleStringReading();

    // Collect styles that end with _normal or _highlight.
    while( poOgrStyleTable->GetNextStyle() != NULL )
    {
        const char *pszStyleName = poOgrStyleTable->GetLastStyleName();

        if( strlen(pszStyleName) > strlen("_normal") &&
            EQUAL(pszStyleName + strlen(pszStyleName) -
                  strlen("_normal"), "_normal") )
        {
            CPLString osName(pszStyleName);
            osName.resize(strlen(pszStyleName) - strlen("_normal"));
            aoSetNormalStyles.insert(osName);
        }
        else if( strlen(pszStyleName) > strlen("_highlight") &&
                 EQUAL(pszStyleName + strlen(pszStyleName) -
                       strlen("_highlight"), "_highlight") )
        {
            CPLString osName(pszStyleName);
            osName.resize(strlen(pszStyleName) - strlen("_highlight"));
            aoSetHighlightStyles.insert(osName);
        }
    }

    /***** parse the style table *****/
    poOgrStyleTable->ResetStyleStringReading();

    const char *pszStyleString = NULL;
    while( ( pszStyleString = poOgrStyleTable->GetNextStyle() ) != NULL )
    {
        const char *pszStyleName = poOgrStyleTable->GetLastStyleName();

        if( aoSetNormalStyles.find(pszStyleName) != aoSetNormalStyles.end() &&
            aoSetHighlightStyles.find(pszStyleName) !=
                aoSetHighlightStyles.end() )
        {
            continue;
        }

        /***** add the style header to the kml *****/
        StylePtr poKmlStyle = poKmlFactory->CreateStyle();

        poKmlStyle->set_id( pszStyleName );

        /***** parse the style string *****/
        addstylestring2kml( pszStyleString, poKmlStyle, poKmlFactory, NULL );

        /***** add balloon style *****/
        const char* pszBalloonStyleBgColor =
            CSLFetchNameValue(papszOptions,
                              CPLSPrintf("%s_balloonstyle_bgcolor",
                                         pszStyleName));
        const char* pszBalloonStyleText =
            CSLFetchNameValue(papszOptions,
                              CPLSPrintf("%s_balloonstyle_text", pszStyleName));
        int nR = 0;
        int nG = 0;
        int nB = 0;
        int nA = 0;
        OGRStylePen oStyleTool;
        if( (pszBalloonStyleBgColor != NULL &&
             oStyleTool.GetRGBFromString( pszBalloonStyleBgColor,
                                          nR, nG, nB, nA ) ) ||
            pszBalloonStyleText != NULL )
        {
            const BalloonStylePtr poKmlBalloonStyle =
                poKmlFactory->CreateBalloonStyle();
            if( pszBalloonStyleBgColor != NULL &&
                oStyleTool.GetRGBFromString( pszBalloonStyleBgColor,
                                             nR, nG, nB, nA ) )
                poKmlBalloonStyle->set_bgcolor(
                    Color32( static_cast<GByte>(nA),
                             static_cast<GByte>(nB),
                             static_cast<GByte>(nG),
                             static_cast<GByte>(nR) ) );
            if( pszBalloonStyleText != NULL )
                poKmlBalloonStyle->set_text(pszBalloonStyleText);
            poKmlStyle->set_balloonstyle( poKmlBalloonStyle );
        }

        /***** add the style to the container *****/
        const DocumentPtr poKmlDocument = AsDocument( poKmlContainer );
        poKmlDocument->add_styleselector( poKmlStyle );
    }

    // Find style name that end with _normal and _highlight to create
    // a StyleMap from both.
    std::set<CPLString>::iterator aoSetNormalStylesIter =
        aoSetNormalStyles.begin();
    for( ;
         aoSetNormalStylesIter != aoSetNormalStyles.end();
         ++aoSetNormalStylesIter )
    {
        CPLString osStyleName(*aoSetNormalStylesIter);
        if( aoSetHighlightStyles.find(osStyleName) !=
                aoSetHighlightStyles.end() )
        {
            StyleMapPtr poKmlStyleMap = poKmlFactory->CreateStyleMap();
            poKmlStyleMap->set_id( osStyleName );

            PairPtr poKmlPairNormal = poKmlFactory->CreatePair();
            poKmlPairNormal->set_key(STYLESTATE_NORMAL);
            poKmlPairNormal->set_styleurl(
                CPLSPrintf("#%s_normal", osStyleName.c_str()));
            poKmlStyleMap->add_pair(poKmlPairNormal);

            PairPtr poKmlPairHighlight = poKmlFactory->CreatePair();
            poKmlPairHighlight->set_key(STYLESTATE_HIGHLIGHT);
            poKmlPairHighlight->set_styleurl(
                CPLSPrintf("#%s_highlight", osStyleName.c_str()));
            poKmlStyleMap->add_pair(poKmlPairHighlight);

            /***** add the style to the container *****/
            DocumentPtr poKmlDocument = AsDocument( poKmlContainer );
            poKmlDocument->add_styleselector( poKmlStyleMap );
        }
    }
}

/******************************************************************************
 Function to add a ListStyle and select it to a container.
******************************************************************************/

void createkmlliststyle(
    KmlFactory * poKmlFactory,
    const char* pszBaseName,
    ContainerPtr poKmlLayerContainer,
    DocumentPtr poKmlDocument,
    const CPLString& osListStyleType,
    const CPLString& osListStyleIconHref)
{
    if( !osListStyleType.empty() || !osListStyleIconHref.empty() )
    {
        StylePtr poKmlStyle = poKmlFactory->CreateStyle();

        const char* pszStyleName =
            CPLSPrintf("%s_liststyle",
                       OGRLIBKMLGetSanitizedNCName(pszBaseName).c_str());
        poKmlStyle->set_id( pszStyleName );

        ListStylePtr poKmlListStyle = poKmlFactory->CreateListStyle();
        poKmlStyle->set_liststyle( poKmlListStyle );
        if( !osListStyleType.empty() )
        {
            if( EQUAL(osListStyleType, "check") )
                poKmlListStyle->set_listitemtype( kmldom::LISTITEMTYPE_CHECK );
            else if( EQUAL(osListStyleType, "radioFolder") )
                poKmlListStyle->set_listitemtype(
                    kmldom::LISTITEMTYPE_RADIOFOLDER );
            else if( EQUAL(osListStyleType, "checkOffOnly") )
                poKmlListStyle->set_listitemtype(
                    kmldom::LISTITEMTYPE_CHECKOFFONLY );
            else if( EQUAL(osListStyleType, "checkHideChildren") )
                poKmlListStyle->set_listitemtype(
                    kmldom::LISTITEMTYPE_CHECKHIDECHILDREN );
            else
            {
                CPLError(
                    CE_Warning, CPLE_AppDefined,
                    "Invalid value for list style type: %s. "
                    "Defaulting to Check",
                    osListStyleType.c_str());
                poKmlListStyle->set_listitemtype( kmldom::LISTITEMTYPE_CHECK );
            }
        }

        if( !osListStyleIconHref.empty() )
        {
            ItemIconPtr poItemIcon = poKmlFactory->CreateItemIcon();
            poItemIcon->set_href( osListStyleIconHref.c_str() );
            poKmlListStyle->add_itemicon(poItemIcon);
        }

        poKmlDocument->add_styleselector( poKmlStyle );
        poKmlLayerContainer->set_styleurl( CPLSPrintf("#%s", pszStyleName) );
    }
}
