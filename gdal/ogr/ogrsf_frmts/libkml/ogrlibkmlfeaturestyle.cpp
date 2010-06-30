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

#include <ogrsf_frmts.h>
#include <ogr_featurestyle.h>
#include <string>
using namespace std;

#include <kml/dom.h>

using kmldom::KmlFactory;;
using kmldom::IconStylePtr;
using kmldom::PolyStylePtr;
using kmldom::LineStylePtr;
using kmldom::LabelStylePtr;
using kmldom::StylePtr;
using kmldom::Style;
using kmldom::StyleMapPtr;
using kmldom::StyleSelectorPtr;


#include "ogrlibkmlfeaturestyle.h"
#include "ogrlibkmlstyle.h"

/******************************************************************************
 function to write out a features style to kml

args:
            poOgrLayer      the layer the feature is in
            poOgrFeat       the feature
            poKmlFactory    the kml dom factory
            poKmlPlacemark  the placemark to add it to

returns:
            nothing
******************************************************************************/

void featurestyle2kml (
    OGRLIBKMLDataSource * poOgrDS,
    OGRLayer * poOgrLayer,
    OGRFeature * poOgrFeat,
    KmlFactory * poKmlFactory,
    PlacemarkPtr poKmlPlacemark )
{

    /***** get the style table *****/

    OGRStyleTable *poOgrSTBL;

    const char *pszStyleString = poOgrFeat->GetStyleString (  );

    /***** does the feature have style? *****/

    if ( pszStyleString ) {

        /***** does it ref a style table? *****/

        if ( *pszStyleString == '@' ) {

            /***** is the name in the layer style table *****/

            OGRStyleTable *hSTBLLayer;
            const char *pszTest = NULL;

            if ( ( hSTBLLayer = poOgrLayer->GetStyleTable (  ) ) )
                pszTest = hSTBLLayer->Find ( pszStyleString );

            if ( pszTest ) {
                string oTmp = "#";

                oTmp.append ( pszStyleString + 1 );

                poKmlPlacemark->set_styleurl ( oTmp );
            }


            /***** assume its a dataset style, mayby the user will add it later *****/

            else {
                string oTmp;

                if ( poOgrDS->GetStylePath (  ) )
                    oTmp.append ( poOgrDS->GetStylePath (  ) );
                oTmp.append ( "#" );
                oTmp.append ( pszStyleString + 1 );

                poKmlPlacemark->set_styleurl ( oTmp );
            }
        }

        /***** no style table ref *****/

        else {
            StylePtr poKmlStyle = poKmlFactory->CreateStyle (  );

            /***** parse the style string *****/

            addstylestring2kml ( pszStyleString, poKmlStyle, poKmlFactory,
                                 poKmlPlacemark, poOgrFeat );

            /***** add the style to the placemark *****/

            poKmlPlacemark->set_styleselector ( poKmlStyle );

        }
    }

    /***** get the style table *****/

    else if ( ( poOgrSTBL = poOgrFeat->GetStyleTable (  ) ) ) {


        StylePtr poKmlStyle = poKmlFactory->CreateStyle (  );

        /***** parse the style table *****/

        poOgrSTBL->ResetStyleStringReading (  );
        const char *pszStyleString;

        while ( ( pszStyleString = poOgrSTBL->GetNextStyle (  ) ) ) {

            if ( *pszStyleString == '@' ) {

                /***** is the name in the layer style table *****/

                OGRStyleTable *poOgrSTBLLayer;
                const char *pszTest = NULL;

                if ( ( poOgrSTBLLayer = poOgrLayer->GetStyleTable (  ) ) )
                    poOgrSTBLLayer->Find ( pszStyleString );

                if ( pszTest ) {
                    string oTmp = "#";

                    oTmp.append ( pszStyleString + 1 );

                    poKmlPlacemark->set_styleurl ( oTmp );
                }

                /***** assume its a dataset style,      *****/
                /***** mayby the user will add it later *****/

                else {
                    string oTmp;

                    if ( poOgrDS->GetStylePath (  ) )
                        oTmp.append ( poOgrDS->GetStylePath (  ) );
                    oTmp.append ( "#" );
                    oTmp.append ( pszStyleString + 1 );

                    poKmlPlacemark->set_styleurl ( oTmp );
                }
            }

            else {

                /***** parse the style string *****/

                addstylestring2kml ( pszStyleString, poKmlStyle,
                                     poKmlFactory, poKmlPlacemark, poOgrFeat );

                /***** add the style to the placemark *****/

                poKmlPlacemark->set_styleselector ( poKmlStyle );

            }
        }
    }
}

/******************************************************************************
 function to read a kml style into ogr's featurestyle
******************************************************************************/

void kml2featurestyle (
    PlacemarkPtr poKmlPlacemark,
    OGRLIBKMLDataSource * poOgrDS,
    OGRLayer * poOgrLayer,
    OGRFeature * poOgrFeat )
{

    /***** does the placemark have a styleselector and a style url? *****/

    if ( poKmlPlacemark->has_styleselector (  )
         && poKmlPlacemark->has_styleurl (  ) ) {

        /* todo do the style and styleurl part */

    }

    /***** is the style a style url *****/

    else if ( poKmlPlacemark->has_styleurl (  ) ) {

        const string poKmlStyleUrl = poKmlPlacemark->get_styleurl (  );

        /***** is the name in the layer style table *****/

        char *pszTmp = CPLStrdup ( poKmlStyleUrl.c_str (  ) );

        OGRStyleTable *poOgrSTBLLayer;
        const char *pszTest = NULL;

        if ( *pszTmp == '#'
             && ( poOgrSTBLLayer = poOgrLayer->GetStyleTable (  ) ) )
            pszTest = poOgrSTBLLayer->Find ( pszTmp + 1 );
        
        if ( pszTest ) {

            /***** should we resolve the style *****/
            
            const char *pszResolve = CPLGetConfigOption ( "LIBKML_RESOLVE_STYLE", "no" );

            if (EQUAL(pszResolve, "yes")) {
                poOgrFeat->SetStyleString ( pszTest );
            }

            else {
                *pszTmp = '@';

                poOgrFeat->SetStyleStringDirectly ( pszTmp );

            }

        }

        /***** not a layer style *****/
        

        else {

            /***** is it a dataset style? *****/

            int nPathLen = strlen ( poOgrDS->GetStylePath (  ) );
             
            if ( !strncmp ( pszTmp, poOgrDS->GetStylePath (  ), nPathLen )) {
                

                /***** should we resolve the style *****/
            
                const char *pszResolve = CPLGetConfigOption ( "LIBKML_RESOLVE_STYLE", "no" );

                if ( EQUAL(pszResolve, "yes") &&
                     ( poOgrSTBLLayer = poOgrDS->GetStyleTable (  ) ) &&
                     ( pszTest = poOgrSTBLLayer->Find ( pszTmp + nPathLen + 1) )
                    ) {
                    
                    poOgrFeat->SetStyleString ( pszTest );
                }

                else {

                    pszTmp[nPathLen] = '@';
                    poOgrFeat->SetStyleString ( pszTmp + nPathLen );
                }

                CPLFree ( pszTmp );
            }
        
            /**** its someplace else *****/

            else {

//todo Handle out of DS style tables 

            }
        }

    }

    /***** does the placemark have a style selector *****/

    else if ( poKmlPlacemark->has_styleselector (  ) ) {

        StyleSelectorPtr poKmlStyleSelector =
            poKmlPlacemark->get_styleselector (  );

        /***** is the style a style? *****/

        if ( poKmlStyleSelector->IsA ( kmldom::Type_Style ) ) {
            StylePtr poKmlStyle = AsStyle ( poKmlStyleSelector );

            OGRStyleMgr *poOgrSM = new OGRStyleMgr;

            poOgrSM->InitStyleString ( NULL );

            /***** read the style *****/

            kml2stylestring ( poKmlStyle, poOgrSM );

            /***** add the style to the feature *****/

            poOgrSM->SetFeatureStyleString ( poOgrFeat );

            delete poOgrSM;
        }

        /***** is the style a stylemap? *****/

        else if ( poKmlStyleSelector->IsA ( kmldom::Type_StyleMap ) ) {
            /* todo need to figure out what to do with a style map */
        }


    }
}
