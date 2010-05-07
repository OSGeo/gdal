/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
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

#include <ogr_featurestyle.h>

#include <kml/dom.h>
#include <kml/base/color32.h>

using kmldom::KmlFactory;;
using kmldom::ElementPtr;
using kmldom::ObjectPtr;
using kmldom::FeaturePtr;
using kmldom::StylePtr;
using kmldom::StyleSelectorPtr;
using kmldom::LineStylePtr;
using kmldom::PolyStylePtr;
using kmldom::IconStylePtr;
using kmldom::IconStyleIconPtr;
using kmldom::LabelStylePtr;
using kmldom::HotSpotPtr;
using kmlbase::Color32;

#include "ogrlibkmlstyle.h"

/******************************************************************************
 generic function to parse a stylestring and add to a kml style

args:
            pszStyleString  the stylestring to parse
            poKmlStyle      the kml style to add to
            poKmlFactory    the kml dom factory

returns:
            nothing

******************************************************************************/

void addstylestring2kml (
    const char *pszStyleString,
    StylePtr poKmlStyle,
    KmlFactory * poKmlFactory,
    PlacemarkPtr poKmlPlacemark,
    OGRFeature * poOgrFeat )
{

    LineStylePtr poKmlLineStyle = NULL;
    PolyStylePtr poKmlPolyStyle = NULL;
    IconStylePtr poKmlIconStyle = NULL;
    LabelStylePtr poKmlLabelStyle = NULL;
    
    /***** just bail now if stylestring is empty *****/

    if ( !pszStyleString || !*pszStyleString ) {
        return;
    }

    /***** create and init a style mamager with the style string *****/

    OGRStyleMgr *poOgrSM = new OGRStyleMgr;

    poOgrSM->InitStyleString ( pszStyleString );

    /***** loop though the style parts *****/

    int i;

    for ( i = 0; i < poOgrSM->GetPartCount ( NULL ); i++ ) {
        OGRStyleTool *poOgrST = poOgrSM->GetPart ( i, NULL );

        if ( !poOgrST ) {
            continue;
        }

        switch ( poOgrST->GetType (  ) ) {
        case OGRSTCPen:
            {
                GBool nullcheck;

                poKmlLineStyle = poKmlFactory->CreateLineStyle (  );

                OGRStylePen *poStylePen = ( OGRStylePen * ) poOgrST;

                /***** pen color *****/

                int nR,
                    nG,
                    nB,
                    nA;

                const char *pszcolor = poStylePen->Color ( nullcheck );

                if ( !nullcheck
                     && poStylePen->GetRGBFromString ( pszcolor, nR, nG, nB, nA ) ) {
                    poKmlLineStyle->set_color ( Color32 ( nA, nB, nG, nR ) );
                }
                double dfWidth = poStylePen->Width ( nullcheck );

                if ( nullcheck )
                    dfWidth = 1.0;

                poKmlLineStyle->set_width ( dfWidth );
                
                break;
            }
        case OGRSTCBrush:
            {
                GBool nullcheck;

                poKmlPolyStyle = poKmlFactory->CreatePolyStyle (  );

                OGRStyleBrush *poStyleBrush = ( OGRStyleBrush * ) poOgrST;

                /***** brush color *****/

                int nR,
                    nG,
                    nB,
                    nA;

                const char *pszcolor = poStyleBrush->ForeColor ( nullcheck );

                if ( !nullcheck
                     && poStyleBrush->GetRGBFromString ( pszcolor, nR, nG, nB, nA ) ) {
                    poKmlPolyStyle->set_color ( Color32 ( nA, nB, nG, nR ) );
                }
                

                break;
            }
        case OGRSTCSymbol:
            {
                GBool nullcheck;
                GBool nullcheck2;

                OGRStyleSymbol *poStyleSymbol = ( OGRStyleSymbol * ) poOgrST;

                /***** id (kml icon) *****/

                const char *pszId = poStyleSymbol->Id ( nullcheck );

                if ( !nullcheck ) {
                    if ( !poKmlIconStyle)
                        poKmlIconStyle = poKmlFactory->CreateIconStyle (  );

                    /***** split it at the ,'s *****/

                    char **papszTokens =
                        CSLTokenizeString2 ( pszId, ",",
                                             CSLT_HONOURSTRINGS | CSLT_STRIPLEADSPACES |
                                             CSLT_STRIPENDSPACES );

                    if ( papszTokens ) {

                        /***** for lack of a better solution just take the first one *****/
                        //todo come up with a better idea

                        if ( papszTokens[0] ) {
                            IconStyleIconPtr poKmlIcon =
                                poKmlFactory->CreateIconStyleIcon (  );
                            poKmlIcon->set_href ( papszTokens[0] );
                            poKmlIconStyle->set_icon ( poKmlIcon );
                        }

                        CSLDestroy ( papszTokens );

                    }


                }

                /***** heading *****/

                double heading = poStyleSymbol->Angle ( nullcheck );

                if ( !nullcheck ) {
                    if ( !poKmlIconStyle)
                        poKmlIconStyle = poKmlFactory->CreateIconStyle (  );
                    poKmlIconStyle->set_heading ( heading );
                }

                /***** scale *****/

                double dfScale = poStyleSymbol->Size ( nullcheck );

                if ( !nullcheck ) {
                    if ( !poKmlIconStyle)
                        poKmlIconStyle = poKmlFactory->CreateIconStyle (  );

                    poKmlIconStyle->set_scale ( dfScale );
                }

                /***** color *****/

                int nR,
                    nG,
                    nB,
                    nA;

                const char *pszcolor = poStyleSymbol->Color ( nullcheck );

                if ( !nullcheck && poOgrST->GetRGBFromString ( pszcolor, nR, nG, nB, nA ) ) {
                    poKmlIconStyle->set_color ( Color32 ( nA, nB, nG, nR ) );
                }

                /***** hotspot *****/

                double dfDx = poStyleSymbol->SpacingX ( nullcheck );
                double dfDy = poStyleSymbol->SpacingY ( nullcheck2 );

                if ( !nullcheck && !nullcheck2 ) {
                    if ( !poKmlIconStyle)
                        poKmlIconStyle = poKmlFactory->CreateIconStyle (  );

                    HotSpotPtr poKmlHotSpot = poKmlFactory->CreateHotSpot (  );

                    poKmlHotSpot->set_x ( dfDx );
                    poKmlHotSpot->set_y ( dfDy );

                    poKmlIconStyle->set_hotspot ( poKmlHotSpot );
                }
                
                break;
            }
        case OGRSTCLabel:
            {
                GBool nullcheck;
                GBool nullcheck2;
                
                poKmlLabelStyle = poKmlFactory->CreateLabelStyle (  );

                OGRStyleLabel *poStyleLabel = ( OGRStyleLabel * ) poOgrST;

                /***** color *****/

                int nR,
                    nG,
                    nB,
                    nA;

                const char *pszcolor = poStyleLabel->ForeColor ( nullcheck );

                if ( !nullcheck
                     && poStyleLabel->GetRGBFromString ( pszcolor, nR, nG, nB, nA ) ) {
                    poKmlLabelStyle->set_color ( Color32 ( nA, nB, nG, nR ) );
                }

                /***** scale *****/

                double dfScale = poStyleLabel->Size ( nullcheck );

                if ( !nullcheck ) {
                    poKmlLabelStyle->set_scale ( dfScale );
                }
                
                /***** heading *****/

                double heading = poStyleLabel->Angle ( nullcheck );

                if ( !nullcheck ) {
                    if ( !poKmlIconStyle) {
                        poKmlIconStyle = poKmlFactory->CreateIconStyle (  );
                        IconStyleIconPtr poKmlIcon = poKmlFactory->CreateIconStyleIcon (  );
                        poKmlIconStyle->set_icon ( poKmlIcon );
                    }
                    
                    poKmlIconStyle->set_heading ( heading );
                }

                /***** hotspot *****/

                double dfDx = poStyleLabel->SpacingX ( nullcheck );
                double dfDy = poStyleLabel->SpacingY ( nullcheck2 );

                if ( !nullcheck && !nullcheck2 ) {
                    if ( !poKmlIconStyle) {
                        poKmlIconStyle = poKmlFactory->CreateIconStyle (  );
                        IconStyleIconPtr poKmlIcon = poKmlFactory->CreateIconStyleIcon (  );
                        poKmlIconStyle->set_icon ( poKmlIcon );
                    }
                    
                    HotSpotPtr poKmlHotSpot = poKmlFactory->CreateHotSpot (  );

                    poKmlHotSpot->set_x ( dfDx );
                    poKmlHotSpot->set_y ( dfDy );

                    poKmlIconStyle->set_hotspot ( poKmlHotSpot );
                }

                /***** label text *****/

                const char *pszText = poStyleLabel->TextString ( nullcheck );

                if ( !nullcheck ) {
                    if ( poKmlPlacemark ) {

                        poKmlPlacemark->set_name( pszText );
                    }
                }
                    
                break;
            }
        case OGRSTCNone:
        default:
            break;
        }

        delete poOgrST;
    }

    if ( poKmlLineStyle )
        poKmlStyle->set_linestyle ( poKmlLineStyle );

    if ( poKmlPolyStyle )
        poKmlStyle->set_polystyle ( poKmlPolyStyle );

    if ( poKmlIconStyle )
        poKmlStyle->set_iconstyle ( poKmlIconStyle );

    if ( poKmlLabelStyle )
        poKmlStyle->set_labelstyle ( poKmlLabelStyle );
    
    delete poOgrSM;
}

/******************************************************************************
 kml2pen
******************************************************************************/

OGRStylePen *kml2pen (
    LineStylePtr poKmlLineStyle );

/******************************************************************************
 kml2brush
******************************************************************************/

OGRStyleBrush *kml2brush (
    PolyStylePtr poKmlPolyStyle );

/******************************************************************************
 kml2brush
******************************************************************************/

OGRStyleSymbol *kml2symbol (
    IconStylePtr poKmlIconStyle );

/******************************************************************************
 kml2label
******************************************************************************/

OGRStyleLabel *kml2label (
    LabelStylePtr poKmlLabelStyle );

/******************************************************************************
 kml2stylemgr
******************************************************************************/

void kml2stylestring (
    StylePtr poKmlStyle,
    OGRStyleMgr * poOgrSM )
{

    /***** linestyle / pen *****/

    if ( poKmlStyle->has_linestyle (  ) ) {
        LineStylePtr poKmlLineStyle = poKmlStyle->get_linestyle (  );

        OGRStylePen *poOgrStylePen = kml2pen ( poKmlLineStyle );

        poOgrSM->AddPart ( poOgrStylePen );

        delete poOgrStylePen;
    }

    /***** polystyle / brush *****/

    if ( poKmlStyle->has_polystyle (  ) ) {
        PolyStylePtr poKmlPolyStyle = poKmlStyle->get_polystyle (  );

        OGRStyleBrush *poOgrStyleBrush = kml2brush ( poKmlPolyStyle );

        poOgrSM->AddPart ( poOgrStyleBrush );

        delete poOgrStyleBrush;
    }

    /***** iconstyle / symbol *****/

    if ( poKmlStyle->has_iconstyle (  ) ) {
        IconStylePtr poKmlIconStyle = poKmlStyle->get_iconstyle (  );

        OGRStyleSymbol *poOgrStyleSymbol = kml2symbol ( poKmlIconStyle );

        poOgrSM->AddPart ( poOgrStyleSymbol );

        delete poOgrStyleSymbol;
    }

    /***** labelstyle / label *****/

    if ( poKmlStyle->has_labelstyle (  ) ) {
        LabelStylePtr poKmlLabelStyle = poKmlStyle->get_labelstyle (  );

        OGRStyleLabel *poOgrStyleLabel = kml2label ( poKmlLabelStyle );

        poOgrSM->AddPart ( poOgrStyleLabel );

        delete poOgrStyleLabel;
    }

}



/******************************************************************************
 kml2pen
******************************************************************************/

OGRStylePen *kml2pen (
    LineStylePtr poKmlLineStyle )
{

    OGRStylePen *poOgrStylePen = new OGRStylePen (  );

    /***** width *****/

    if ( poKmlLineStyle->has_width (  ) )
        poOgrStylePen->SetWidth ( poKmlLineStyle->get_width (  ) );

    /***** color *****/

    if ( poKmlLineStyle->has_color (  ) ) {
        Color32 poKmlColor = poKmlLineStyle->get_color (  );
        char szColor[10] = { };
        snprintf ( szColor, sizeof ( szColor ), "#%02X%02X%02X%02X",
                   poKmlColor.get_red (  ),
                   poKmlColor.get_green (  ),
                   poKmlColor.get_blue (  ), poKmlColor.get_alpha (  ) );
        poOgrStylePen->SetColor ( szColor );
    }

    return poOgrStylePen;
}

/******************************************************************************
 kml2brush
******************************************************************************/

OGRStyleBrush *kml2brush (
    PolyStylePtr poKmlPolyStyle )
{

    OGRStyleBrush *poOgrStyleBrush = new OGRStyleBrush (  );

    /***** color *****/

    if ( poKmlPolyStyle->has_color (  ) ) {
        Color32 poKmlColor = poKmlPolyStyle->get_color (  );
        char szColor[10] = { };
        snprintf ( szColor, sizeof ( szColor ), "#%02X%02X%02X%02X",
                   poKmlColor.get_red (  ),
                   poKmlColor.get_green (  ),
                   poKmlColor.get_blue (  ), poKmlColor.get_alpha (  ) );
        poOgrStyleBrush->SetForeColor ( szColor );
    }

    return poOgrStyleBrush;
}

/******************************************************************************
 kml2symbol
******************************************************************************/

OGRStyleSymbol *kml2symbol (
    IconStylePtr poKmlIconStyle )
{

    OGRStyleSymbol *poOgrStyleSymbol = new OGRStyleSymbol (  );

    /***** id (kml icon) *****/

    if ( poKmlIconStyle->has_icon (  ) ) {
        IconStyleIconPtr poKmlIcon = poKmlIconStyle->get_icon (  );

        if ( poKmlIcon->has_href (  ) ) {
            std::string oIcon = "\"";
            oIcon.append ( poKmlIcon->get_href (  ).c_str (  ) );
            oIcon.append ( "\"" );
            poOgrStyleSymbol->SetId ( oIcon.c_str (  ) );

        }
    }

    /***** heading *****/

    if ( poKmlIconStyle->has_heading (  ) )
        poOgrStyleSymbol->SetAngle ( poKmlIconStyle->get_heading (  ) );

    /***** scale *****/

    if ( poKmlIconStyle->has_scale (  ) )
        poOgrStyleSymbol->SetSize ( poKmlIconStyle->get_scale (  ) );

    /***** color *****/

    if ( poKmlIconStyle->has_color (  ) ) {
        Color32 poKmlColor = poKmlIconStyle->get_color (  );
        char szColor[10] = { };
        snprintf ( szColor, sizeof ( szColor ), "#%02X%02X%02X%02X",
                   poKmlColor.get_red (  ),
                   poKmlColor.get_green (  ),
                   poKmlColor.get_blue (  ), poKmlColor.get_alpha (  ) );
        poOgrStyleSymbol->SetColor ( szColor );
    }

    /***** hotspot *****/

    if ( poKmlIconStyle->has_hotspot (  ) ) {
        HotSpotPtr poKmlHotSpot = poKmlIconStyle->get_hotspot (  );

        if ( poKmlHotSpot->has_x (  ) )
            poOgrStyleSymbol->SetSpacingX ( poKmlHotSpot->get_x (  ) );
        if ( poKmlHotSpot->has_y (  ) )
            poOgrStyleSymbol->SetSpacingY ( poKmlHotSpot->get_y (  ) );

    }

    return poOgrStyleSymbol;
}

/******************************************************************************
 kml2label
******************************************************************************/

OGRStyleLabel *kml2label (
    LabelStylePtr poKmlLabelStyle )
{

    OGRStyleLabel *poOgrStyleLabel = new OGRStyleLabel (  );

    /***** color *****/

    if ( poKmlLabelStyle->has_color (  ) ) {
        Color32 poKmlColor = poKmlLabelStyle->get_color (  );
        char szColor[10] = { };
        snprintf ( szColor, sizeof ( szColor ), "#%02X%02X%02X%02X",
                   poKmlColor.get_red (  ),
                   poKmlColor.get_green (  ),
                   poKmlColor.get_blue (  ), poKmlColor.get_alpha (  ) );
        poOgrStyleLabel->SetForColor ( szColor );
    }

    return poOgrStyleLabel;
}

/******************************************************************************
 function to add a kml style to a style table
******************************************************************************/

void kml2styletable (
    OGRStyleTable * poOgrStyleTable,
    StylePtr poKmlStyle )
{


    /***** no reason to add it if it don't have an id *****/

    if ( poKmlStyle->has_id (  ) ) {

        OGRStyleMgr *poOgrSM = new OGRStyleMgr ( poOgrStyleTable );

        poOgrSM->InitStyleString ( NULL );

        /***** read the style *****/

        kml2stylestring ( poKmlStyle, poOgrSM );

        /***** add the style to the style table *****/

        const std::string oName = poKmlStyle->get_id (  );


        poOgrSM->AddStyle ( CPLString (  ).Printf ( "@%s",
                                                    oName.c_str (  ) ), NULL );

        /***** cleanup the style manager *****/

        delete poOgrSM;
    }

    else {
        CPLError ( CE_Failure, CPLE_AppDefined,
                   "ERROR Parseing kml Style: No id" );
    }

    return;
}

/******************************************************************************
 function to parse a style table out of a document
******************************************************************************/

void ParseStyles (
    DocumentPtr poKmlDocument,
    OGRStyleTable ** poStyleTable )
{

    /***** if document is null just bail now *****/

    if ( !poKmlDocument )
        return;

    /***** loop over the Styles *****/

    size_t nKmlStyles = poKmlDocument->get_styleselector_array_size (  );
    size_t iKmlStyle;

    for ( iKmlStyle = 0; iKmlStyle < nKmlStyles; iKmlStyle++ ) {

        StyleSelectorPtr poKmlStyle =
            poKmlDocument->get_styleselector_array_at ( iKmlStyle );

        if ( !poKmlStyle->IsA ( kmldom::Type_Style ) )
            continue;

        if ( !*poStyleTable )
            *poStyleTable = new OGRStyleTable (  );

        ElementPtr poKmlElement = AsElement ( poKmlStyle );

        kml2styletable ( *poStyleTable, AsStyle ( poKmlElement ) );
    }

    return;
}

/******************************************************************************
 function to add a style table to a kml container
******************************************************************************/

void styletable2kml (
    OGRStyleTable * poOgrStyleTable,
    KmlFactory * poKmlFactory,
    ContainerPtr poKmlContainer )
{

    /***** just return if the styletable is null *****/

    if ( !poOgrStyleTable )
        return;

    /***** parse the style table *****/

    poOgrStyleTable->ResetStyleStringReading (  );
    const char *pszStyleString;

    while ( ( pszStyleString = poOgrStyleTable->GetNextStyle (  ) ) ) {
        const char *pszStyleName = poOgrStyleTable->GetLastStyleName (  );

        /***** add the style header to the kml *****/

        StylePtr poKmlStyle = poKmlFactory->CreateStyle (  );

        poKmlStyle->set_id ( pszStyleName + 1 );

        /***** parse the style string *****/

        addstylestring2kml ( pszStyleString, poKmlStyle, poKmlFactory, NULL, NULL );

        /***** add the style to the container *****/

        DocumentPtr poKmlDocument = AsDocument ( poKmlContainer );

        //ObjectPtr pokmlObject = boost::static_pointer_cast <kmldom::Object> () ;
        //poKmlContainer->add_feature ( AsFeature( poKmlStyle) );
        poKmlDocument->add_styleselector ( poKmlStyle );

    }

    return;
}
