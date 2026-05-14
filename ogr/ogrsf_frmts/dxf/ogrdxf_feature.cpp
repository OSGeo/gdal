/******************************************************************************
 *
 * Project:  DXF Translator
 * Purpose:  Provides additional functionality for DXF features
 * Author:   Alan Thomas, alant@outlook.com.au
 *
 ******************************************************************************
 * Copyright (c) 2017, Alan Thomas <alant@outlook.com.au>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_dxf.h"
#include "cpl_string.h"

/************************************************************************/
/*                           OGRDXFFeature()                            */
/************************************************************************/

OGRDXFFeature::OGRDXFFeature(const OGRFeatureDefn *poFeatureDefn)
    : OGRFeature(poFeatureDefn), oOCS(0.0, 0.0, 1.0), bIsBlockReference(false),
      dfBlockAngle(0.0), oBlockScale(1.0, 1.0, 1.0),
      oOriginalCoords(0.0, 0.0, 0.0)
{
}

OGRDXFFeature::~OGRDXFFeature() = default;

/************************************************************************/
/*                          CloneDXFFeature()                           */
/*                                                                      */
/*      Replacement for OGRFeature::Clone() for DXF features.           */
/************************************************************************/

OGRDXFFeature *OGRDXFFeature::CloneDXFFeature()
{
    OGRDXFFeature *poNew = new OGRDXFFeature(GetDefnRef());
    if (poNew == nullptr)
        return nullptr;

    if (!CopySelfTo(poNew))
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

    if (poASMTransform)
    {
        poNew->poASMTransform = std::unique_ptr<OGRDXFAffineTransform>(
            new OGRDXFAffineTransform(*poASMTransform));
    }

    for (const std::unique_ptr<OGRDXFFeature> &poAttribFeature :
         apoAttribFeatures)
    {
        poNew->apoAttribFeatures.emplace_back(
            poAttribFeature->CloneDXFFeature());
    }

    return poNew;
}

/************************************************************************/
/*                        ApplyOCSTransformer()                         */
/*                                                                      */
/*      Applies the OCS transformation stored in this feature to        */
/*      the specified geometry.                                         */
/************************************************************************/

void OGRDXFFeature::ApplyOCSTransformer(OGRGeometry *const poGeometry) const
{
    if (poGeometry == nullptr)
        return;

    double adfN[3];
    oOCS.ToArray(adfN);

    OGRDXFOCSTransformer oTransformer(adfN);

    // Promote to 3D, in case the OCS transformation introduces a
    // third dimension to the geometry.
    const bool bInitially2D = !poGeometry->Is3D();
    if (bInitially2D)
        poGeometry->set3D(TRUE);

    poGeometry->transform(&oTransformer);

    // If the geometry was 2D to begin with, and is still 2D after the
    // OCS transformation, flatten it back to 2D.
    if (bInitially2D)
    {
        OGREnvelope3D oEnvelope;
        poGeometry->getEnvelope(&oEnvelope);
        if (oEnvelope.MaxZ == 0.0 && oEnvelope.MinZ == 0.0)
            poGeometry->flattenTo2D();
    }
}

/************************************************************************/
/*                        ApplyOCSTransformer()                         */
/************************************************************************/

void OGRDXFFeature::ApplyOCSTransformer(OGRDXFAffineTransform *const poCT) const
{
    if (!poCT)
        return;

    double adfN[3];
    oOCS.ToArray(adfN);

    OGRDXFOCSTransformer oTransformer(adfN);

    oTransformer.ComposeOnto(*poCT);
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

const CPLString
OGRDXFFeature::GetColor(OGRDXFDataSource *const poDS,
                        OGRDXFFeature *const poBlockFeature /* = NULL */)
{
    CPLString osLayer = GetFieldAsString("Layer");

    /* -------------------------------------------------------------------- */
    /*      Is the layer or object hidden/off (1) or frozen (2)?            */
    /* -------------------------------------------------------------------- */

    int iHidden = 0;

    if (oStyleProperties.count("Hidden") > 0 ||
        (poBlockFeature &&
         poBlockFeature->oStyleProperties.count("Hidden") > 0))
    {
        // Hidden objects should never be shown no matter what happens
        iHidden = 1;
        oStyleProperties["Hidden"] = "1";
    }
    else
    {
        auto osHidden = poDS->LookupLayerProperty(osLayer, "Hidden");
        if (osHidden)
            iHidden = atoi(osHidden->c_str());

        // Is the block feature on a frozen layer? If so, hide this feature
        if (!iHidden && poBlockFeature)
        {
            const CPLString osBlockLayer =
                poBlockFeature->GetFieldAsString("Layer");
            auto osBlockHidden =
                poDS->LookupLayerProperty(osBlockLayer, "Hidden");
            if (osBlockHidden && atoi(osBlockHidden->c_str()) == 2)
                iHidden = 2;
        }

        // If this feature is on a frozen layer (other than layer 0), make the
        // object totally hidden so it won't reappear if we regenerate the style
        // string again during block insertion
        if (iHidden == 2 && !EQUAL(GetFieldAsString("Layer"), "0"))
            oStyleProperties["Hidden"] = "1";
    }

    // Helpful constants
    const int C_BYLAYER = 256;
    const int C_BYBLOCK = 0;
    const int C_TRUECOLOR = -100;  // not used in DXF - for our purposes only
    const int C_BYLAYER_FORCE0 =
        -101;  // not used in DXF - for our purposes only

    /* -------------------------------------------------------------------- */
    /*      MULTILEADER entities store colors by directly outputting        */
    /*      the AcCmEntityColor struct as a 32-bit integer.                 */
    /* -------------------------------------------------------------------- */

    int nColor = C_BYLAYER;
    unsigned int nTrueColor = 0;

    if (oStyleProperties.count("TrueColor") > 0)
    {
        nTrueColor = atoi(oStyleProperties["TrueColor"]);
        nColor = C_TRUECOLOR;
    }
    else if (oStyleProperties.count("Color") > 0)
    {
        nColor = atoi(oStyleProperties["Color"]);
    }

    const unsigned char byColorMethod = (nColor & 0xFF000000) >> 24;
    switch (byColorMethod)
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
    if (nColor == C_BYBLOCK && poBlockFeature)
    {
        if (poBlockFeature->oStyleProperties.count("TrueColor") > 0)
        {
            // Inherit true color from the owning block
            nTrueColor = atoi(poBlockFeature->oStyleProperties["TrueColor"]);
            nColor = C_TRUECOLOR;

            // Use the inherited color if we regenerate the style string
            // again during block insertion
            oStyleProperties["TrueColor"] =
                poBlockFeature->oStyleProperties["TrueColor"];
        }
        else if (poBlockFeature->oStyleProperties.count("Color") > 0)
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
            // If the owning block has no explicit color, assume ByLayer,
            // but take the color from the owning block's layer
            nColor = C_BYLAYER;
            osLayer = poBlockFeature->GetFieldAsString("Layer");

            // If we regenerate the style string again during
            // block insertion, treat as ByLayer, but when
            // not in block insertion, treat as layer 0
            oStyleProperties["Color"] = std::to_string(C_BYLAYER_FORCE0);
        }
    }

    // Strange special case: consider the following scenario:
    //
    //                             Block  Color    Layer
    //                             -----  -------  -------
    //  Drawing contains:  INSERT  BLK1   ByBlock  MYLAYER
    //     BLK1 contains:  INSERT  BLK2   ByLayer  0
    //     BLK2 contains:  LINE           ByBlock  0
    //
    // When viewing the drawing, the line is displayed in
    // MYLAYER's layer colour, not black as might be expected.
    if (nColor == C_BYLAYER_FORCE0)
    {
        if (poBlockFeature)
            osLayer = poBlockFeature->GetFieldAsString("Layer");
        else
            osLayer = "0";

        nColor = C_BYLAYER;
    }

    // Use layer color?
    if (nColor == C_BYLAYER)
    {
        auto osTrueColor = poDS->LookupLayerProperty(osLayer, "TrueColor");
        if (osTrueColor)
        {
            nTrueColor = atoi(osTrueColor->c_str());
            nColor = C_TRUECOLOR;

            if (poBlockFeature && osLayer != "0")
            {
                // Use the inherited color if we regenerate the style string
                // again during block insertion (except when the entity is
                // on layer 0)
                oStyleProperties["TrueColor"] = *osTrueColor;
            }
        }
        else
        {
            auto osColor = poDS->LookupLayerProperty(osLayer, "Color");
            if (osColor)
            {
                nColor = atoi(osColor->c_str());

                if (poBlockFeature && osLayer != "0")
                {
                    // Use the inherited color if we regenerate the style string
                    // again during block insertion (except when the entity is
                    // on layer 0)
                    oStyleProperties["Color"] = *osColor;
                }
            }
        }
    }

    // If no color is available, use the default black/white color
    if (nColor != C_TRUECOLOR && (nColor < 1 || nColor > 255))
        nColor = 7;

    /* -------------------------------------------------------------------- */
    /*      Translate the DWG/DXF color index to a hex color string.        */
    /* -------------------------------------------------------------------- */

    CPLString osResult;

    if (nColor == C_TRUECOLOR)
    {
        osResult.Printf("#%06x", nTrueColor);
    }
    else
    {
        const unsigned char *pabyDXFColors = ACGetColorTable();

        osResult.Printf("#%02x%02x%02x", pabyDXFColors[nColor * 3 + 0],
                        pabyDXFColors[nColor * 3 + 1],
                        pabyDXFColors[nColor * 3 + 2]);
    }

    if (iHidden)
        osResult += "00";
    else
    {
        int nOpacity = -1;

        if (oStyleProperties.count("Transparency") > 0)
        {
            int nTransparency = atoi(oStyleProperties["Transparency"]);
            if ((nTransparency & 0x02000000) != 0)
            {
                nOpacity = nTransparency & 0xFF;
            }
            else if ((nTransparency & 0x01000000) != 0)  // By block ?
            {
                if (poBlockFeature &&
                    poBlockFeature->oStyleProperties.count("Transparency") > 0)
                {
                    nOpacity =
                        atoi(poBlockFeature->oStyleProperties["Transparency"]) &
                        0xFF;

                    // Use the inherited transparency if we regenerate the style string
                    // again during block insertion
                    oStyleProperties["Transparency"] =
                        poBlockFeature->oStyleProperties["Transparency"];
                }
            }
        }
        else
        {
            auto osTransparency =
                poDS->LookupLayerProperty(osLayer, "Transparency");
            if (osTransparency)
            {
                nOpacity = atoi(osTransparency->c_str()) & 0xFF;

                if (poBlockFeature && osLayer != "0")
                {
                    // Use the inherited transparency if we regenerate the style string
                    // again during block insertion (except when the entity is
                    // on layer 0)
                    oStyleProperties["Transparency"] = *osTransparency;
                }
            }
        }

        if (nOpacity >= 0)
            osResult += CPLSPrintf("%02x", nOpacity & 0xFF);
    }

    return osResult;
}
