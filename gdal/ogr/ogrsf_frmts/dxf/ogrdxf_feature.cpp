/******************************************************************************
 *
 * Project:  DXF Translator
 * Purpose:  Provides additional functionality for DXF features
 * Author:   Alan Thomas, alant@outlook.com.au
 *
 ******************************************************************************
 * Copyright (c) 2017, Alan Thomas <alant@outlook.com.au>
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
#include "cpl_string.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                            OGRDXFFeature()                           */
/************************************************************************/

OGRDXFFeature::OGRDXFFeature( OGRFeatureDefn * poFeatureDefn ):
    OGRFeature( poFeatureDefn ),
    oOCS(0.0, 0.0, 1.0),
    bIsBlockReference(false),
    dfBlockAngle(0.0),
    oBlockScale(1.0, 1.0, 1.0),
    oOriginalCoords(0.0, 0.0, 0.0)
{
}

/************************************************************************/
/*                          CloneDXFFeature()                           */
/*                                                                      */
/*      Replacement for OGRFeature::Clone() for DXF features.           */
/************************************************************************/

OGRDXFFeature *OGRDXFFeature::CloneDXFFeature()
{
    OGRDXFFeature *poNew = new OGRDXFFeature( GetDefnRef() );
    if( poNew == nullptr )
        return nullptr;

    if( !CopySelfTo( poNew ) )
    {
        delete poNew;
        return nullptr;
    }

    poNew->oOCS = oOCS;
    poNew->bIsBlockReference = bIsBlockReference;
    poNew->osBlockName = osBlockName;
    poNew->dfBlockAngle = dfBlockAngle;
    poNew->oBlockScale = oBlockScale;
    poNew->oOriginalCoords = oOriginalCoords;
    poNew->osAttributeTag = osAttributeTag;
    poNew->oStyleProperties = oStyleProperties;

    return poNew;
}

/************************************************************************/
/*                        ApplyOCSTransformer()                         */
/*                                                                      */
/*      Applies the OCS transformation stored in this feature to        */
/*      the specified geometry.                                         */
/************************************************************************/

void OGRDXFFeature::ApplyOCSTransformer( OGRGeometry* const poGeometry ) const

{
    OGRDXFLayer::ApplyOCSTransformer( poGeometry, oOCS );
}

/************************************************************************/
/*                              GetColor()                              */
/*                                                                      */
/*      Gets the hex color string for this feature, using the given     */
/*      data source to fetch layer properties.                          */
/*                                                                      */
/*      For usage info about poBlockFeature, see                        */
/*      OGRDXFLayer::PrepareFeatureStyle.                               */
/************************************************************************/

const CPLString OGRDXFFeature::GetColor( OGRDXFDataSource* const poDS,
    OGRDXFFeature* const poBlockFeature /* = NULL */ )
{
    const CPLString osLayer = GetFieldAsString("Layer");

/* -------------------------------------------------------------------- */
/*      Is the layer or object disabled/hidden/frozen/off?              */
/* -------------------------------------------------------------------- */

    bool bHidden = false;

    if( oStyleProperties.count("Hidden") > 0 &&
        atoi(oStyleProperties["Hidden"]) == 1 )
    {
        bHidden = true;
    }
    else
    {
        const char* pszHidden = poDS->LookupLayerProperty( osLayer, "Hidden" );
        bHidden = pszHidden && EQUAL(pszHidden, "1");
    }

/* -------------------------------------------------------------------- */
/*      MULTILEADER entities store colors by directly outputting        */
/*      the AcCmEntityColor struct as a 32-bit integer.                 */
/* -------------------------------------------------------------------- */

    int nColor = 256;

    if( oStyleProperties.count("Color") > 0 )
        nColor = atoi(oStyleProperties["Color"]);

    const unsigned char byColorMethod = ( nColor & 0xFF000000 ) >> 24;
    switch( byColorMethod )
    {
      // ByLayer
      case 0xC0:
        nColor = 256;
        break;

      // ByBlock
      case 0xC1:
        nColor = 0;
        break;

      // RGB true color
      case 0xC2:
        return CPLString().Printf( "#%06x", nColor & 0xFFFFFF );

      // Indexed color
      case 0xC3:
        nColor &= 0xFF;
        break;
    }

/* -------------------------------------------------------------------- */
/*      Work out the indexed color for this feature.                    */
/* -------------------------------------------------------------------- */

    // Use ByBlock color?
    if( nColor < 1 && poBlockFeature )
    {
        if( poBlockFeature->oStyleProperties.count("Color") > 0 )
        {
            // Inherit color from the owning block
            nColor = atoi(poBlockFeature->oStyleProperties["Color"]);

            // Use the inherited color if we regenerate the style string
            // again during block insertion
            oStyleProperties["Color"] =
                poBlockFeature->oStyleProperties["Color"];
        }
        else
        {
            // If the owning block has no explicit color, assume ByLayer
            nColor = 256;
        }
    }

    // Use layer color?
    if( nColor > 255 )
    {
        const char *pszValue = poDS->LookupLayerProperty( osLayer, "Color" );
        if( pszValue != nullptr )
            nColor = atoi(pszValue);
    }

    // If no color is available, use the default black/white color
    if( nColor < 1 || nColor > 255 )
        nColor = 7;

/* -------------------------------------------------------------------- */
/*      Translate the DWG/DXF color index to a hex color string.        */
/* -------------------------------------------------------------------- */

    const unsigned char *pabyDXFColors = ACGetColorTable();

    CPLString osResult;
    osResult.Printf( "#%02x%02x%02x",
        pabyDXFColors[nColor*3+0],
        pabyDXFColors[nColor*3+1],
        pabyDXFColors[nColor*3+2] );

    if( bHidden )
        osResult += "00";

    return osResult;
}
