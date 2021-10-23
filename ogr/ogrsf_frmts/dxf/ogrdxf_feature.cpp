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

    if( poASMTransform )
    {
        poNew->poASMTransform = std::unique_ptr<OGRDXFAffineTransform>(
            new OGRDXFAffineTransform( *poASMTransform ) );
    }

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
    if( poGeometry == nullptr )
        return;

    double adfN[3];
    oOCS.ToArray( adfN );

    OGRDXFOCSTransformer oTransformer( adfN );

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
/*                        ApplyOCSTransformer()                         */
/************************************************************************/

void OGRDXFFeature::ApplyOCSTransformer( OGRDXFAffineTransform* const poCT ) const
{
    if( !poCT )
        return;

    double adfN[3];
    oOCS.ToArray( adfN );

    OGRDXFOCSTransformer oTransformer( adfN );

    oTransformer.ComposeOnto( *poCT );
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
    CPLString osLayer = GetFieldAsString("Layer");

/* -------------------------------------------------------------------- */
/*      Is the layer or object hidden/off (1) or frozen (2)?            */
/* -------------------------------------------------------------------- */

    int iHidden = 0;

    if( oStyleProperties.count("Hidden") > 0 ||
        ( poBlockFeature &&
        poBlockFeature->oStyleProperties.count("Hidden") > 0 ) )
    {
        // Hidden objects should never be shown no matter what happens,
        // so they can be treated as if they are on a frozen layer
        iHidden = 2;
    }
    else
    {
        const char* pszHidden = poDS->LookupLayerProperty( osLayer, "Hidden" );
        if( pszHidden )
            iHidden = atoi(pszHidden);

        // Is the block feature on a frozen layer? If so, hide this feature
        if( !iHidden && poBlockFeature )
        {
            const CPLString osBlockLayer =
                poBlockFeature->GetFieldAsString("Layer");
            const char* pszBlockHidden =
                poDS->LookupLayerProperty( osBlockLayer, "Hidden" );
            if( pszBlockHidden && atoi(pszBlockHidden) == 2 )
                iHidden = 2;
        }
    }

    // If this feature is on a frozen layer, make the object totally
    // hidden so it won't reappear if we regenerate the style string again
    // during block insertion
    if( iHidden == 2 )
        oStyleProperties["Hidden"] = "1";

    // Helpful constants
    const int C_BYLAYER = 256;
    const int C_BYBLOCK = 0;
    const int C_TRUECOLOR = -100; // not used in DXF - for our purposes only

/* -------------------------------------------------------------------- */
/*      MULTILEADER entities store colors by directly outputting        */
/*      the AcCmEntityColor struct as a 32-bit integer.                 */
/* -------------------------------------------------------------------- */

    int nColor = C_BYLAYER;
    unsigned int nTrueColor = 0;

    if( oStyleProperties.count("TrueColor") > 0 )
    {
        nTrueColor = atoi(oStyleProperties["TrueColor"]);
        nColor = C_TRUECOLOR;
    }
    else if( oStyleProperties.count("Color") > 0 )
    {
        nColor = atoi(oStyleProperties["Color"]);
    }

    const unsigned char byColorMethod = ( nColor & 0xFF000000 ) >> 24;
    switch( byColorMethod )
    {
      // ByLayer
      case 0xC0:
        nColor = C_BYLAYER;
        break;

      // ByBlock
      case 0xC1:
        nColor = C_BYBLOCK;
        break;

      // RGB true color
      case 0xC2:
        nTrueColor = nColor & 0xFFFFFF;
        nColor = C_TRUECOLOR;
        break;

      // Indexed color
      case 0xC3:
        nColor &= 0xFF;
        break;
    }

/* -------------------------------------------------------------------- */
/*      Work out the indexed color for this feature.                    */
/* -------------------------------------------------------------------- */

    // Use ByBlock color?
    if( nColor == C_BYBLOCK && poBlockFeature )
    {
        if( poBlockFeature->oStyleProperties.count("TrueColor") > 0 )
        {
            // Inherit true color from the owning block
            nTrueColor = atoi(poBlockFeature->oStyleProperties["TrueColor"]);
            nColor = C_TRUECOLOR;

            // Use the inherited color if we regenerate the style string
            // again during block insertion
            oStyleProperties["TrueColor"] =
                poBlockFeature->oStyleProperties["TrueColor"];
        }
        else if( poBlockFeature->oStyleProperties.count("Color") > 0 )
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
            nColor = C_BYLAYER;
        }
    }

    // Use layer color?
    if( nColor == C_BYLAYER )
    {
        if( poBlockFeature )
        {
            // Use the block feature's layer instead
            osLayer = poBlockFeature->GetFieldAsString( "Layer" );
        }

        const char *pszTrueColor =
            poDS->LookupLayerProperty( osLayer, "TrueColor" );
        if( pszTrueColor != nullptr && *pszTrueColor )
        {
            nTrueColor = atoi(pszTrueColor);
            nColor = C_TRUECOLOR;

            if( poBlockFeature )
            {
                // Use the inherited color if we regenerate the style string
                // again during block insertion
                oStyleProperties["TrueColor"] = pszTrueColor;
            }
        }
        else
        {
            const char *pszColor =
                poDS->LookupLayerProperty( osLayer, "Color" );
            if( pszColor != nullptr )
            {
                nColor = atoi(pszColor);

                if( poBlockFeature )
                {
                    // Use the inherited color if we regenerate the style string
                    // again during block insertion
                    oStyleProperties["Color"] = pszColor;
                }
            }
        }
    }

    // If no color is available, use the default black/white color
    if( nColor != C_TRUECOLOR && ( nColor < 1 || nColor > 255 ) )
        nColor = 7;

/* -------------------------------------------------------------------- */
/*      Translate the DWG/DXF color index to a hex color string.        */
/* -------------------------------------------------------------------- */

    CPLString osResult;

    if( nColor == C_TRUECOLOR )
    {
        osResult.Printf( "#%06x", nTrueColor );
    }
    else
    {
        const unsigned char *pabyDXFColors = ACGetColorTable();

        osResult.Printf( "#%02x%02x%02x",
            pabyDXFColors[nColor*3+0],
            pabyDXFColors[nColor*3+1],
            pabyDXFColors[nColor*3+2] );
    }
    
    if( iHidden )
        osResult += "00";

    return osResult;
}
