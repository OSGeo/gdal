/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
 * Copyright (c) 2010-2014, Even Rouault <even dot rouault at spatialys.com>
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

#include "ogrsf_frmts.h"
#include "ogr_featurestyle.h"
#include <string>
#include "ogrlibkmlfeaturestyle.h"
#include "ogrlibkmlstyle.h"

CPL_CVSID("$Id$")

using kmldom::FeaturePtr;
using kmldom::IconStylePtr;
using kmldom::KmlFactory;
using kmldom::LabelStylePtr;
using kmldom::LineStylePtr;
using kmldom::PolyStylePtr;
using kmldom::StyleMapPtr;
using kmldom::StylePtr;
using kmldom::StyleSelectorPtr;

/******************************************************************************
 function to write out a features style to kml

Args:
            poOgrLayer      the layer the feature is in
            poOgrFeat       the feature
            poKmlFactory    the kml dom factory
            poKmlFeature    the placemark to add it to

Returns:
            nothing
******************************************************************************/

void featurestyle2kml(
    OGRLIBKMLDataSource * poOgrDS,
    OGRLayer * poOgrLayer,
    OGRFeature * poOgrFeat,
    KmlFactory * poKmlFactory,
    FeaturePtr poKmlFeature )
{
    /***** get the style table *****/
    OGRStyleTable *poOgrSTBL = nullptr;

    const char *pszStyleString = poOgrFeat->GetStyleString();

    /***** does the feature have style? *****/
    if( pszStyleString && pszStyleString[0] != '\0' )
    {
        /***** does it ref a style table? *****/
        if( *pszStyleString == '@' )
        {
            const char* pszStyleName = pszStyleString + 1;

            /***** Is the name in the layer style table *****/
            OGRStyleTable *hSTBLLayer = poOgrLayer->GetStyleTable();
            const char *pszTest = (hSTBLLayer != nullptr) ?
                hSTBLLayer->Find( pszStyleName ) : nullptr;

            if( pszTest )
            {
                string oTmp = "#";
                oTmp.append( pszStyleName );
                poKmlFeature->set_styleurl( oTmp );
            }
            /***** assume its a dataset style,
                   maybe the user will add it later *****/
            else
            {
                string oTmp;

                if( poOgrDS->GetStylePath() )
                    oTmp.append( poOgrDS->GetStylePath() );
                oTmp.append( "#" );
                oTmp.append( pszStyleName );

                poKmlFeature->set_styleurl( oTmp );
            }
        }
        /***** no style table ref *****/
        else
        {
            /***** parse the style string *****/
            const StylePtr poKmlStyle =
                addstylestring2kml( pszStyleString, nullptr, poKmlFactory,
                                    poKmlFeature );

            /***** add the style to the placemark *****/
            if( poKmlStyle )
                poKmlFeature->set_styleselector( poKmlStyle );
        }
    }
    /***** get the style table *****/
    else if( ( poOgrSTBL = poOgrFeat->GetStyleTable() ) != nullptr )
    {
        /***** parse the style table *****/
        poOgrSTBL->ResetStyleStringReading();

        while( ( pszStyleString = poOgrSTBL->GetNextStyle() ) != nullptr )
        {
            if( *pszStyleString == '@' )
            {
                const char* pszStyleName = pszStyleString + 1;

                /***** is the name in the layer style table *****/
                OGRStyleTable *poOgrSTBLLayer = nullptr;

                if( ( poOgrSTBLLayer = poOgrLayer->GetStyleTable() )
                    != nullptr )
                {
                    poOgrSTBLLayer->Find( pszStyleName );
                }

                /***** Assume its a dataset style,       *****/
                /***** mayby the user will add it later. *****/

                string oTmp;
                if( poOgrDS->GetStylePath() )
                    oTmp.append( poOgrDS->GetStylePath() );
                oTmp.append( "#" );
                oTmp.append( pszStyleName );

                poKmlFeature->set_styleurl( oTmp );
            }
            else
            {
                /***** parse the style string *****/
                const StylePtr poKmlStyle =
                    addstylestring2kml( pszStyleString, nullptr,
                                     poKmlFactory, poKmlFeature );
                if( poKmlStyle )
                {
                    /***** Add the style to the placemark. *****/
                    poKmlFeature->set_styleselector( poKmlStyle );
                }
            }
        }
    }
}

/******************************************************************************
 function to read a kml style into ogr's featurestyle
******************************************************************************/

void kml2featurestyle(
    FeaturePtr poKmlFeature,
    OGRLIBKMLDataSource * poOgrDS,
    OGRLayer * poOgrLayer,
    OGRFeature * poOgrFeat )
{
    /***** does the placemark have a style url? *****/
    if( poKmlFeature->has_styleurl() )
    {
        const string poKmlStyleUrl = poKmlFeature->get_styleurl();

        /***** is the name in the layer style table *****/
        char *pszUrl = CPLStrdup( poKmlStyleUrl.c_str() );

        OGRStyleTable *poOgrSTBLLayer = nullptr;
        const char *pszTest = nullptr;

        /***** is it a layer style ? *****/
        if( *pszUrl == '#' &&
            ( poOgrSTBLLayer = poOgrLayer->GetStyleTable() ) != nullptr )
        {
             pszTest = poOgrSTBLLayer->Find( pszUrl + 1 );
        }

        if( pszTest )
        {
            /***** should we resolve the style *****/
            const char *pszResolve =
                CPLGetConfigOption( "LIBKML_RESOLVE_STYLE", "no" );

            if( CPLTestBool(pszResolve) )
            {
                poOgrFeat->SetStyleString( pszTest );
            }
            else
            {
                *pszUrl = '@';
                poOgrFeat->SetStyleString( pszUrl );
            }
        }
        /***** is it a dataset style? *****/
        else
        {
            const int nPathLen =
                static_cast<int>(strlen( poOgrDS->GetStylePath() ));

            if( nPathLen == 0 ||
                EQUALN( pszUrl, poOgrDS->GetStylePath(), nPathLen ) )
            {
                /***** should we resolve the style *****/
                const char *pszResolve =
                    CPLGetConfigOption( "LIBKML_RESOLVE_STYLE", "no" );

                if( CPLTestBool(pszResolve) &&
                    ( poOgrSTBLLayer = poOgrDS->GetStyleTable() ) != nullptr &&
                    ( pszTest =
                          poOgrSTBLLayer->Find(pszUrl + nPathLen + 1) ) != nullptr
                    )
                {
                    poOgrFeat->SetStyleString( pszTest );
                }
                else
                {
                    pszUrl[nPathLen] = '@';
                    poOgrFeat->SetStyleString( pszUrl + nPathLen );
                }
            }
            /**** its someplace else *****/
            else
            {
                const char *pszFetch =
                    CPLGetConfigOption( "LIBKML_EXTERNAL_STYLE", "no" );

                if( CPLTestBool(pszFetch) )
                {
                    /***** load up the style table *****/
                    char *pszUrlTmp = CPLStrdup(pszUrl);
                    char *pszPound = strchr(pszUrlTmp, '#');
                    if( pszPound != nullptr )
                    {
                        *pszPound = '\0';
                    }

                    /***** try it as a url then a file *****/
                    VSILFILE *fp = nullptr;
                    if( (fp = VSIFOpenL(
                             CPLFormFilename( "/vsicurl/", pszUrlTmp, nullptr ),
                              "r" )) != nullptr ||
                        (fp = VSIFOpenL( pszUrlTmp, "r" )) != nullptr )
                    {
                        char szbuf[1025] = { '\0' };
                        std::string oStyle = "";

                        /***** loop, read and copy to a string *****/
                        do {
                            const size_t nRead =
                                VSIFReadL(szbuf, 1, sizeof(szbuf) - 1, fp);

                            if( nRead == 0 )
                                break;

                            /***** copy buf to the string *****/

                            szbuf[nRead] = '\0';
                            oStyle.append( szbuf );
                        } while( !VSIFEofL(fp) );

                        VSIFCloseL(fp);

                        /***** parse the kml into the ds style table *****/

                        if( poOgrDS->ParseIntoStyleTable(&oStyle, pszUrlTmp) )
                        {
                            kml2featurestyle( poKmlFeature,
                                              poOgrDS,
                                              poOgrLayer,
                                              poOgrFeat );
                        }
                        else
                        {
                            /***** if failed just store the url *****/
                            poOgrFeat->SetStyleString( pszUrl );
                        }
                    }
                    CPLFree(pszUrlTmp);
                }
                else
                {
                    poOgrFeat->SetStyleString( pszUrl );
                }
            }
        }
        CPLFree( pszUrl );
    }

    /***** does the placemark have a style selector *****/
    if( poKmlFeature->has_styleselector() )
    {
        StyleSelectorPtr poKmlStyleSelector =
            poKmlFeature->get_styleselector();

        /***** is the style a style? *****/
        if( poKmlStyleSelector->IsA( kmldom::Type_Style ) )
        {
            StylePtr poKmlStyle = AsStyle( poKmlStyleSelector );

            OGRStyleMgr *poOgrSM = new OGRStyleMgr;

            /***** if were resolving style the feature *****/
            /***** might already have styling to add too *****/
            const char *pszResolve =
                CPLGetConfigOption( "LIBKML_RESOLVE_STYLE", "no" );
            if( CPLTestBool(pszResolve) ) {
                 poOgrSM->InitFromFeature( poOgrFeat );
            }
            else
            {
                /***** if featyurestyle gets a name tool this needs
                       changed to the above *****/
                poOgrSM->InitStyleString( nullptr );
            }

            /***** read the style *****/
            kml2stylestring( poKmlStyle, poOgrSM );

            /***** add the style to the feature *****/
            poOgrFeat->SetStyleString(poOgrSM->GetStyleString(nullptr));

            delete poOgrSM;
        }
        /***** is the style a stylemap? *****/
        else if( poKmlStyleSelector->IsA( kmldom::Type_StyleMap ) )
        {
            // TODO: Need to figure out what to do with a style map.
        }
    }
}
