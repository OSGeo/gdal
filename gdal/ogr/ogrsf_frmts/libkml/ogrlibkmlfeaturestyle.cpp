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
#include <iostream>
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

    if ( pszStyleString && pszStyleString[0] != '\0' ) {

        /***** does it ref a style table? *****/

        if ( *pszStyleString == '@' ) {

            const char* pszStyleName = pszStyleString + 1;

            /***** is the name in the layer style table *****/

            OGRStyleTable *hSTBLLayer;
            const char *pszTest = NULL;

            if ( ( hSTBLLayer = poOgrLayer->GetStyleTable (  ) ) )
                pszTest = hSTBLLayer->Find ( pszStyleName );

            if ( pszTest ) {
                string oTmp = "#";

                oTmp.append ( pszStyleName );

                poKmlPlacemark->set_styleurl ( oTmp );
            }


            /***** assume its a dataset style, mayby the user will add it later *****/

            else {
                string oTmp;

                if ( poOgrDS->GetStylePath (  ) )
                    oTmp.append ( poOgrDS->GetStylePath (  ) );
                oTmp.append ( "#" );
                oTmp.append ( pszStyleName );

                poKmlPlacemark->set_styleurl ( oTmp );
            }
        }

        /***** no style table ref *****/

        else {
            /***** parse the style string *****/

            StylePtr poKmlStyle = addstylestring2kml ( pszStyleString, NULL, poKmlFactory,
                                 poKmlPlacemark, poOgrFeat );

            /***** add the style to the placemark *****/
            if( poKmlStyle != NULL )
                poKmlPlacemark->set_styleselector ( poKmlStyle );

        }
    }

    /***** get the style table *****/

    else if ( ( poOgrSTBL = poOgrFeat->GetStyleTable (  ) ) ) {


        StylePtr poKmlStyle = NULL;

        /***** parse the style table *****/

        poOgrSTBL->ResetStyleStringReading (  );
        const char *pszStyleString;

        while ( ( pszStyleString = poOgrSTBL->GetNextStyle (  ) ) ) {

            if ( *pszStyleString == '@' ) {

                const char* pszStyleName = pszStyleString + 1;

                /***** is the name in the layer style table *****/

                OGRStyleTable *poOgrSTBLLayer;
                const char *pszTest = NULL;

                if ( ( poOgrSTBLLayer = poOgrLayer->GetStyleTable (  ) ) )
                    poOgrSTBLLayer->Find ( pszStyleName );

                if ( pszTest ) {
                    string oTmp = "#";

                    oTmp.append ( pszStyleName );

                    poKmlPlacemark->set_styleurl ( oTmp );
                }

                /***** assume its a dataset style,      *****/
                /***** mayby the user will add it later *****/

                else {
                    string oTmp;

                    if ( poOgrDS->GetStylePath (  ) )
                        oTmp.append ( poOgrDS->GetStylePath (  ) );
                    oTmp.append ( "#" );
                    oTmp.append ( pszStyleName );

                    poKmlPlacemark->set_styleurl ( oTmp );
                }
            }

            else {

                /***** parse the style string *****/

                poKmlStyle = addstylestring2kml ( pszStyleString, poKmlStyle,
                                     poKmlFactory, poKmlPlacemark, poOgrFeat );
                if( poKmlStyle != NULL )
                {
                    /***** add the style to the placemark *****/

                    poKmlPlacemark->set_styleselector ( poKmlStyle );
                }

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

    /***** does the placemark have a style url? *****/

    if (    poKmlPlacemark->has_styleurl (  ) ) {

        const string poKmlStyleUrl = poKmlPlacemark->get_styleurl (  );

        /***** is the name in the layer style table *****/

        char *pszUrl = CPLStrdup ( poKmlStyleUrl.c_str (  ) );

        OGRStyleTable *poOgrSTBLLayer;
        const char *pszTest = NULL;

        /***** is it a layer style ? *****/

        if ( *pszUrl == '#'
             && ( poOgrSTBLLayer = poOgrLayer->GetStyleTable (  ) ) )
        {
             pszTest = poOgrSTBLLayer->Find ( pszUrl + 1 );
        }

        if ( pszTest ) {

            /***** should we resolve the style *****/

            const char *pszResolve = CPLGetConfigOption ( "LIBKML_RESOLVE_STYLE", "no" );

            if (CSLTestBoolean(pszResolve)) {

                poOgrFeat->SetStyleString ( pszTest );
            }

            else {

                *pszUrl = '@';

                poOgrFeat->SetStyleString( pszUrl );

            }

        }

        /***** is it a dataset style? *****/
        
        else {

            int nPathLen = strlen ( poOgrDS->GetStylePath (  ) );

            if (    nPathLen == 0
                 || EQUALN ( pszUrl, poOgrDS->GetStylePath (  ), nPathLen ))
            {

                /***** should we resolve the style *****/

                const char *pszResolve = CPLGetConfigOption ( "LIBKML_RESOLVE_STYLE", "no" );

                if (    CSLTestBoolean(pszResolve)
                     && ( poOgrSTBLLayer = poOgrDS->GetStyleTable (  ) )
                     && ( pszTest = poOgrSTBLLayer->Find ( pszUrl + nPathLen + 1) )
                   )
                {

                    poOgrFeat->SetStyleString ( pszTest );
                }

                else {

                    pszUrl[nPathLen] = '@';
                    poOgrFeat->SetStyleString ( pszUrl + nPathLen );
                }
       
            }
            
            /**** its someplace else *****/

            else {

                const char *pszFetch = CPLGetConfigOption ( "LIBKML_EXTERNAL_STYLE", "no" );

                if ( CSLTestBoolean(pszFetch) ) {

                    /***** load up the style table *****/

                    char *pszUrlTmp = CPLStrdup(pszUrl);
                    char *pszPound;
                    if ((pszPound = strchr(pszUrlTmp, '#'))) {
                        
                        *pszPound = '\0';
                    }

                    /***** try it as a url then a file *****/

                    VSILFILE *fp = NULL;

                    if (    (fp = VSIFOpenL( CPLFormFilename( "/vsicurl/",
                                                              pszUrlTmp,
                                                              NULL),
                                             "r" ))
                        ||  (fp = VSIFOpenL( pszUrlTmp, "r" )))
                    {

                        char szbuf[1025];
                        std::string oStyle = "";

                        /***** loop, read and copy to a string *****/

                        size_t nRead;
                    
                        do {
                            
                            nRead = VSIFReadL(szbuf, 1, sizeof(szbuf) - 1, fp);
                            
                            if (nRead == 0)
                                break;

                            /***** copy buf to the string *****/

                            szbuf[nRead] = '\0';
                            oStyle.append( szbuf );

                        } while (!VSIFEofL(fp));

                        VSIFCloseL(fp);

                        /***** parse the kml into the ds style table *****/

                        if ( poOgrDS->ParseIntoStyleTable (&oStyle, pszUrlTmp)) {

                            kml2featurestyle (poKmlPlacemark,
                                              poOgrDS,
                                              poOgrLayer,
                                              poOgrFeat );
                        }

                        else {

                            /***** if failed just store the url *****/

                            poOgrFeat->SetStyleString ( pszUrl );
                        }
                    }
                    CPLFree(pszUrlTmp);
                }

                else {

                    poOgrFeat->SetStyleString ( pszUrl );
                }
            }

        }
        CPLFree ( pszUrl );

    }

    /***** does the placemark have a style selector *****/

   if ( poKmlPlacemark->has_styleselector (  ) ) {

        StyleSelectorPtr poKmlStyleSelector =
            poKmlPlacemark->get_styleselector (  );

        /***** is the style a style? *****/

        if ( poKmlStyleSelector->IsA ( kmldom::Type_Style ) ) {
            StylePtr poKmlStyle = AsStyle ( poKmlStyleSelector );

            OGRStyleMgr *poOgrSM = new OGRStyleMgr;

            /***** if were resolveing style the feature  *****/
            /***** might already have styling to add too *****/
            
            const char *pszResolve = CPLGetConfigOption ( "LIBKML_RESOLVE_STYLE", "no" );
            if (CSLTestBoolean(pszResolve)) {
                 poOgrSM->InitFromFeature ( poOgrFeat );
            }
            else {

                /***** if featyurestyle gets a name tool this needs changed to the above *****/
                
                poOgrSM->InitStyleString ( NULL );
            }

            /***** read the style *****/

            kml2stylestring ( poKmlStyle, poOgrSM );

            /***** add the style to the feature *****/
            
            poOgrFeat->SetStyleString(poOgrSM->GetStyleString(NULL));

            delete poOgrSM;
        }

        /***** is the style a stylemap? *****/

        else if ( poKmlStyleSelector->IsA ( kmldom::Type_StyleMap ) ) {
            /* todo need to figure out what to do with a style map */
        }


    }
}
