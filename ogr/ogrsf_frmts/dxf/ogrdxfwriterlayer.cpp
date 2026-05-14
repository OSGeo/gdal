/******************************************************************************
 *
 * Project:  DXF Translator
 * Purpose:  Implements OGRDXFWriterLayer - the OGRLayer class used for
 *           writing a DXF file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_dxf.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_featurestyle.h"

#include <cstdlib>

/************************************************************************/
/*                         OGRDXFWriterLayer()                          */
/************************************************************************/

OGRDXFWriterLayer::OGRDXFWriterLayer(OGRDXFWriterDS *poDSIn, VSILFILE *fpIn)
    : fp(fpIn),
      poFeatureDefn(nullptr),  // TODO(schwehr): Can I move the new here?
      poDS(poDSIn)
{
    nNextAutoID = 1;
    bWriteHatch = CPLTestBool(CPLGetConfigOption("DXF_WRITE_HATCH", "YES"));

    poFeatureDefn = new OGRFeatureDefn("entities");
    poFeatureDefn->Reference();

    OGRDXFDataSource::AddStandardFields(poFeatureDefn, ODFM_IncludeBlockFields);
}

/************************************************************************/
/*                         ~OGRDXFWriterLayer()                         */
/************************************************************************/

OGRDXFWriterLayer::~OGRDXFWriterLayer()

{
    if (poFeatureDefn)
        poFeatureDefn->Release();
}

/************************************************************************/
/*                              ResetFP()                               */
/*                                                                      */
/*      Redirect output.  Mostly used for writing block definitions.    */
/************************************************************************/

void OGRDXFWriterLayer::ResetFP(VSILFILE *fpNew)

{
    fp = fpNew;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRDXFWriterLayer::TestCapability(const char *pszCap) const

{
    if (EQUAL(pszCap, OLCStringsAsUTF8))
        return TRUE;
    else if (EQUAL(pszCap, OLCSequentialWrite))
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/*                                                                      */
/*      This is really a dummy as our fields are precreated.            */
/************************************************************************/

OGRErr OGRDXFWriterLayer::CreateField(const OGRFieldDefn *poField,
                                      int bApproxOK)

{
    if (poFeatureDefn->GetFieldIndex(poField->GetNameRef()) >= 0 && bApproxOK)
        return OGRERR_NONE;
    if (EQUAL(poField->GetNameRef(), "OGR_STYLE"))
    {
        poFeatureDefn->AddFieldDefn(poField);
        return OGRERR_NONE;
    }

    CPLError(CE_Failure, CPLE_AppDefined,
             "DXF layer does not support arbitrary field creation, field '%s' "
             "not created.",
             poField->GetNameRef());

    return OGRERR_FAILURE;
}

/************************************************************************/
/*                             WriteValue()                             */
/************************************************************************/

int OGRDXFWriterLayer::WriteValue(int nCode, const char *pszValue)

{
    CPLString osLinePair;

    osLinePair.Printf("%3d\n", nCode);

    if (strlen(pszValue) < 255)
        osLinePair += pszValue;
    else
        osLinePair.append(pszValue, 255);

    osLinePair += "\n";

    return VSIFWriteL(osLinePair.c_str(), 1, osLinePair.size(), fp) ==
           osLinePair.size();
}

/************************************************************************/
/*                             WriteValue()                             */
/************************************************************************/

int OGRDXFWriterLayer::WriteValue(int nCode, int nValue)

{
    CPLString osLinePair;

    osLinePair.Printf("%3d\n%d\n", nCode, nValue);

    return VSIFWriteL(osLinePair.c_str(), 1, osLinePair.size(), fp) ==
           osLinePair.size();
}

/************************************************************************/
/*                             WriteValue()                             */
/************************************************************************/

int OGRDXFWriterLayer::WriteValue(int nCode, double dfValue)

{
    char szLinePair[64];

    CPLsnprintf(szLinePair, sizeof(szLinePair), "%3d\n%.15g\n", nCode, dfValue);
    size_t nLen = strlen(szLinePair);

    return VSIFWriteL(szLinePair, 1, nLen, fp) == nLen;
}

/************************************************************************/
/*                             WriteCore()                              */
/*                                                                      */
/*      Write core fields common to all sorts of elements.              */
/************************************************************************/

OGRErr OGRDXFWriterLayer::WriteCore(OGRFeature *poFeature,
                                    const CorePropertiesType &oCoreProperties)

{
    /* -------------------------------------------------------------------- */
    /*      Write out an entity id.  I'm not sure why this is critical,     */
    /*      but it seems that VoloView will just quietly fail to open       */
    /*      dxf files without entity ids set on most/all entities.          */
    /*      Also, for reasons I don't understand these ids seem to have     */
    /*      to start somewhere around 0x50 hex (80 decimal).                */
    /* -------------------------------------------------------------------- */
    unsigned int nGotFID = 0;
    poDS->WriteEntityID(fp, nGotFID, poFeature->GetFID());
    poFeature->SetFID(nGotFID);

    WriteValue(100, "AcDbEntity");

    /* -------------------------------------------------------------------- */
    /*      For now we assign everything to the default layer - layer       */
    /*      "0" - if there is no layer property on the source features.     */
    /* -------------------------------------------------------------------- */
    const char *pszLayer = poFeature->GetFieldAsString("Layer");
    if (pszLayer == nullptr || strlen(pszLayer) == 0)
    {
        WriteValue(8, "0");
    }
    else
    {
        CPLString osSanitizedLayer(pszLayer);
        // Replaced restricted characters with underscore
        // See
        // http://docs.autodesk.com/ACD/2010/ENU/AutoCAD%202010%20User%20Documentation/index.html?url=WS1a9193826455f5ffa23ce210c4a30acaf-7345.htm,topicNumber=d0e41665
        const char achForbiddenChars[] = {'<', '>', '/', '\\', '"', ':',
                                          ';', '?', '*', '|',  '=', '\''};
        for (size_t i = 0; i < CPL_ARRAYSIZE(achForbiddenChars); ++i)
        {
            osSanitizedLayer.replaceAll(achForbiddenChars[i], '_');
        }

        // also remove newline characters (#15067)
        osSanitizedLayer.replaceAll("\r\n", "_");
        osSanitizedLayer.replaceAll('\r', '_');
        osSanitizedLayer.replaceAll('\n', '_');

        auto osExists =
            poDS->oHeaderDS.LookupLayerProperty(osSanitizedLayer, "Exists");
        if (!osExists &&
            CSLFindString(poDS->papszLayersToCreate, osSanitizedLayer) == -1)
        {
            poDS->papszLayersToCreate =
                CSLAddString(poDS->papszLayersToCreate, osSanitizedLayer);
        }

        WriteValue(8, osSanitizedLayer);
    }

    for (const auto &oProp : oCoreProperties)
    {
        if (oProp.first == PROP_RGBA_COLOR)
        {
            bool bPerfectMatch = false;
            const char *pszColor = oProp.second.c_str();
            const int nColor = ColorStringToDXFColor(pszColor, bPerfectMatch);
            if (nColor >= 0)
            {
                WriteValue(62, nColor);

                unsigned int nRed = 0;
                unsigned int nGreen = 0;
                unsigned int nBlue = 0;
                unsigned int nOpacity = 255;

                const int nCount = sscanf(pszColor, "#%2x%2x%2x%2x", &nRed,
                                          &nGreen, &nBlue, &nOpacity);
                if (nCount >= 3 && !bPerfectMatch)
                {
                    WriteValue(420, static_cast<int>(nBlue | (nGreen << 8) |
                                                     (nRed << 16)));
                }
                if (nCount == 4)
                {
                    WriteValue(440, static_cast<int>(nOpacity | (2 << 24)));
                }
            }
        }
        else
        {
            // If this happens, this is a coding error
            CPLError(CE_Failure, CPLE_AppDefined,
                     "BUG! Unhandled core property %d", oProp.first);
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                            WriteINSERT()                             */
/************************************************************************/

OGRErr OGRDXFWriterLayer::WriteINSERT(OGRFeature *poFeature)

{
    CorePropertiesType oCoreProperties;

    // Write style symbol color
    OGRStyleTool *poTool = nullptr;
    OGRStyleMgr oSM;
    if (poFeature->GetStyleString() != nullptr)
    {
        oSM.InitFromFeature(poFeature);

        if (oSM.GetPartCount() > 0)
            poTool = oSM.GetPart(0);
    }
    if (poTool && poTool->GetType() == OGRSTCSymbol)
    {
        OGRStyleSymbol *poSymbol = cpl::down_cast<OGRStyleSymbol *>(poTool);
        GBool bDefault;
        const char *pszColor = poSymbol->Color(bDefault);
        if (pszColor && !bDefault)
            oCoreProperties.emplace_back(PROP_RGBA_COLOR, pszColor);
    }
    delete poTool;

    WriteValue(0, "INSERT");
    WriteCore(poFeature, oCoreProperties);
    WriteValue(100, "AcDbBlockReference");
    WriteValue(2, poFeature->GetFieldAsString("BlockName"));

    /* -------------------------------------------------------------------- */
    /*      Write location in OCS.                                          */
    /* -------------------------------------------------------------------- */
    int nCoordCount = 0;
    const double *padfCoords =
        poFeature->GetFieldAsDoubleList("BlockOCSCoords", &nCoordCount);

    if (nCoordCount == 3)
    {
        WriteValue(10, padfCoords[0]);
        WriteValue(20, padfCoords[1]);
        if (!WriteValue(30, padfCoords[2]))
            return OGRERR_FAILURE;
    }
    else
    {
        // We don't have an OCS; we will just assume that the location of
        // the geometry (in WCS) is the correct insertion point.
        OGRPoint *poPoint = poFeature->GetGeometryRef()->toPoint();

        WriteValue(10, poPoint->getX());
        if (!WriteValue(20, poPoint->getY()))
            return OGRERR_FAILURE;

        if (poPoint->getGeometryType() == wkbPoint25D)
        {
            if (!WriteValue(30, poPoint->getZ()))
                return OGRERR_FAILURE;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Write scaling.                                                  */
    /* -------------------------------------------------------------------- */
    int nScaleCount = 0;
    const double *padfScale =
        poFeature->GetFieldAsDoubleList("BlockScale", &nScaleCount);

    if (nScaleCount == 3)
    {
        WriteValue(41, padfScale[0]);
        WriteValue(42, padfScale[1]);
        WriteValue(43, padfScale[2]);
    }

    /* -------------------------------------------------------------------- */
    /*      Write rotation.                                                 */
    /* -------------------------------------------------------------------- */
    const double dfAngle = poFeature->GetFieldAsDouble("BlockAngle");

    if (dfAngle != 0.0)
    {
        WriteValue(50, dfAngle);  // degrees
    }

    /* -------------------------------------------------------------------- */
    /*      Write OCS normal vector.                                        */
    /* -------------------------------------------------------------------- */
    int nOCSCount = 0;
    const double *padfOCS =
        poFeature->GetFieldAsDoubleList("BlockOCSNormal", &nOCSCount);

    if (nOCSCount == 3)
    {
        WriteValue(210, padfOCS[0]);
        WriteValue(220, padfOCS[1]);
        WriteValue(230, padfOCS[2]);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                             WritePOINT()                             */
/************************************************************************/

OGRErr OGRDXFWriterLayer::WritePOINT(OGRFeature *poFeature)

{
    CorePropertiesType oCoreProperties;

    // Write style pen color
    OGRStyleTool *poTool = nullptr;
    OGRStyleMgr oSM;
    if (poFeature->GetStyleString() != nullptr)
    {
        oSM.InitFromFeature(poFeature);

        if (oSM.GetPartCount() > 0)
            poTool = oSM.GetPart(0);
    }
    if (poTool && poTool->GetType() == OGRSTCPen)
    {
        OGRStylePen *poPen = cpl::down_cast<OGRStylePen *>(poTool);
        GBool bDefault;
        const char *pszColor = poPen->Color(bDefault);
        if (pszColor && !bDefault)
            oCoreProperties.emplace_back(PROP_RGBA_COLOR, pszColor);
    }
    delete poTool;

    WriteValue(0, "POINT");
    WriteCore(poFeature, oCoreProperties);
    WriteValue(100, "AcDbPoint");

    OGRPoint *poPoint = poFeature->GetGeometryRef()->toPoint();

    WriteValue(10, poPoint->getX());
    if (!WriteValue(20, poPoint->getY()))
        return OGRERR_FAILURE;

    if (poPoint->getGeometryType() == wkbPoint25D)
    {
        if (!WriteValue(30, poPoint->getZ()))
            return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                             TextEscape()                             */
/*                                                                      */
/*      Translate UTF8 to Win1252 and escape special characters like    */
/*      newline and space with DXF style escapes.  Note that            */
/*      non-win1252 unicode characters are translated using the         */
/*      unicode escape sequence.                                        */
/************************************************************************/

CPLString OGRDXFWriterLayer::TextEscape(const char *pszInput)

{
    CPLString osResult;
    wchar_t *panInput = CPLRecodeToWChar(pszInput, CPL_ENC_UTF8, CPL_ENC_UCS2);
    for (int i = 0; panInput[i] != 0; i++)
    {
        if (panInput[i] == '\n')
        {
            osResult += "\\P";
        }
        else if (panInput[i] == ' ')
        {
            osResult += "\\~";
        }
        else if (panInput[i] == '\\')
        {
            osResult += "\\\\";
        }
        else if (panInput[i] == '^')
        {
            osResult += "^ ";
        }
        else if (panInput[i] < ' ')
        {
            osResult += '^';
            osResult += static_cast<char>(panInput[i] + '@');
        }
        else if (panInput[i] > 255)
        {
            CPLString osUnicode;
            osUnicode.Printf("\\U+%04x", (int)panInput[i]);
            osResult += osUnicode;
        }
        else
        {
            osResult += (char)panInput[i];
        }
    }

    CPLFree(panInput);

    return osResult;
}

/************************************************************************/
/*                     PrepareTextStyleDefinition()                     */
/************************************************************************/
std::map<CPLString, CPLString>
OGRDXFWriterLayer::PrepareTextStyleDefinition(OGRStyleLabel *poLabelTool)
{
    GBool bDefault;

    std::map<CPLString, CPLString> oTextStyleDef;

    /* -------------------------------------------------------------------- */
    /*      Fetch the data for this text style.                             */
    /* -------------------------------------------------------------------- */
    const char *pszFontName = poLabelTool->FontName(bDefault);
    if (!bDefault)
        oTextStyleDef["Font"] = pszFontName;

    const GBool bBold = poLabelTool->Bold(bDefault);
    if (!bDefault)
        oTextStyleDef["Bold"] = bBold ? "1" : "0";

    const GBool bItalic = poLabelTool->Italic(bDefault);
    if (!bDefault)
        oTextStyleDef["Italic"] = bItalic ? "1" : "0";

    const double dfStretch = poLabelTool->Stretch(bDefault);
    if (!bDefault)
    {
        oTextStyleDef["Width"] = CPLString().Printf("%f", dfStretch / 100.0);
    }

    return oTextStyleDef;
}

/************************************************************************/
/*                             WriteTEXT()                              */
/************************************************************************/

OGRErr OGRDXFWriterLayer::WriteTEXT(OGRFeature *poFeature)

{
    CorePropertiesType oCoreProperties;

    /* -------------------------------------------------------------------- */
    /*      Do we have styling information?                                 */
    /* -------------------------------------------------------------------- */
    OGRStyleTool *poTool = nullptr;
    OGRStyleMgr oSM;

    if (poFeature->GetStyleString() != nullptr)
    {
        oSM.InitFromFeature(poFeature);

        if (oSM.GetPartCount() > 0)
            poTool = oSM.GetPart(0);
    }

    if (poTool && poTool->GetType() == OGRSTCLabel)
    {
        OGRStyleLabel *poLabel = cpl::down_cast<OGRStyleLabel *>(poTool);
        GBool bDefault;
        const char *pszColor = poLabel->ForeColor(bDefault);
        if (pszColor && !bDefault)
            oCoreProperties.emplace_back(PROP_RGBA_COLOR, pszColor);
    }

    WriteValue(0, "MTEXT");
    WriteCore(poFeature, oCoreProperties);
    WriteValue(100, "AcDbMText");

    /* ==================================================================== */
    /*      Process the LABEL tool.                                         */
    /* ==================================================================== */
    double dfDx = 0.0;
    double dfDy = 0.0;

    if (poTool && poTool->GetType() == OGRSTCLabel)
    {
        OGRStyleLabel *poLabel = cpl::down_cast<OGRStyleLabel *>(poTool);
        GBool bDefault;

        /* --------------------------------------------------------------------
         */
        /*      Angle */
        /* --------------------------------------------------------------------
         */
        const double dfAngle = poLabel->Angle(bDefault);

        if (!bDefault)
            WriteValue(50, dfAngle);

        /* --------------------------------------------------------------------
         */
        /*      Height - We need to fetch this in georeferenced units - I'm */
        /*      doubt the default translation mechanism will be much good. */
        /* --------------------------------------------------------------------
         */
        poTool->SetUnit(OGRSTUGround);
        const double dfHeight = poLabel->Size(bDefault);

        if (!bDefault)
            WriteValue(40, dfHeight);

        /* --------------------------------------------------------------------
         */
        /*      Anchor / Attachment Point */
        /* --------------------------------------------------------------------
         */
        const int nAnchor = poLabel->Anchor(bDefault);

        if (!bDefault)
        {
            const static int anAnchorMap[] = {-1, 7, 8, 9, 4, 5, 6,
                                              1,  2, 3, 7, 8, 9};

            if (nAnchor > 0 && nAnchor < 13)
                WriteValue(71, anAnchorMap[nAnchor]);
        }

        /* --------------------------------------------------------------------
         */
        /*      Offset */
        /* --------------------------------------------------------------------
         */
        dfDx = poLabel->SpacingX(bDefault);
        dfDy = poLabel->SpacingY(bDefault);

        /* --------------------------------------------------------------------
         */
        /*      Escape the text, and convert to ISO8859. */
        /* --------------------------------------------------------------------
         */
        const char *pszText = poLabel->TextString(bDefault);

        if (pszText != nullptr && !bDefault)
        {
            CPLString osEscaped = TextEscape(pszText);
            while (osEscaped.size() > 250)
            {
                WriteValue(3, osEscaped.substr(0, 250).c_str());
                osEscaped.erase(0, 250);
            }
            WriteValue(1, osEscaped);
        }

        /* --------------------------------------------------------------------
         */
        /*      Store the text style in the map. */
        /* --------------------------------------------------------------------
         */
        std::map<CPLString, CPLString> oTextStyleDef =
            PrepareTextStyleDefinition(poLabel);
        CPLString osStyleName;

        for (const auto &oPair : oNewTextStyles)
        {
            if (oPair.second == oTextStyleDef)
            {
                osStyleName = oPair.first;
                break;
            }
        }

        if (osStyleName == "")
        {

            do
            {
                osStyleName.Printf("AutoTextStyle-%d", nNextAutoID++);
            } while (poDS->oHeaderDS.TextStyleExists(osStyleName));

            oNewTextStyles[osStyleName] = std::move(oTextStyleDef);
        }

        WriteValue(7, osStyleName);
    }

    delete poTool;

    /* -------------------------------------------------------------------- */
    /*      Write the location.                                             */
    /* -------------------------------------------------------------------- */
    OGRPoint *poPoint = poFeature->GetGeometryRef()->toPoint();

    WriteValue(10, poPoint->getX() + dfDx);
    if (!WriteValue(20, poPoint->getY() + dfDy))
        return OGRERR_FAILURE;

    if (poPoint->getGeometryType() == wkbPoint25D)
    {
        if (!WriteValue(30, poPoint->getZ()))
            return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                     PrepareLineTypeDefinition()                      */
/************************************************************************/
std::vector<double>
OGRDXFWriterLayer::PrepareLineTypeDefinition(OGRStylePen *poPen)
{

    /* -------------------------------------------------------------------- */
    /*      Fetch pattern.                                                  */
    /* -------------------------------------------------------------------- */
    GBool bDefault;
    const char *pszPattern = poPen->Pattern(bDefault);

    if (bDefault || strlen(pszPattern) == 0)
        return std::vector<double>();

    /* -------------------------------------------------------------------- */
    /*      Split into pen up / pen down bits.                              */
    /* -------------------------------------------------------------------- */
    char **papszTokens = CSLTokenizeString(pszPattern);
    std::vector<double> adfWeightTokens;

    for (int i = 0; papszTokens != nullptr && papszTokens[i] != nullptr; i++)
    {
        const char *pszToken = papszTokens[i];
        CPLString osAmount;
        CPLString osDXFEntry;

        // Split amount and unit.
        const char *pszUnit = pszToken;  // Used after for.
        for (; strchr("0123456789.", *pszUnit) != nullptr; pszUnit++)
        {
        }

        osAmount.assign(pszToken, (int)(pszUnit - pszToken));

        // If the unit is other than 'g' we really should be trying to
        // do some type of transformation - but what to do?  Pretty hard.

        // Even entries are "pen down" represented as positive in DXF.
        // "Pen up" entries (gaps) are represented as negative.
        if (i % 2 == 0)
            adfWeightTokens.push_back(CPLAtof(osAmount));
        else
            adfWeightTokens.push_back(-CPLAtof(osAmount));
    }

    CSLDestroy(papszTokens);

    return adfWeightTokens;
}

/************************************************************************/
/*                       IsLineTypeProportional()                       */
/************************************************************************/

static double IsLineTypeProportional(const std::vector<double> &adfA,
                                     const std::vector<double> &adfB)
{
    // If they are not the same length, they are not the same linetype
    if (adfA.size() != adfB.size())
        return 0.0;

    // Determine the proportion of the first elements
    const double dfRatio = (adfA[0] != 0.0) ? (adfB[0] / adfA[0]) : 0.0;

    // Check if all elements follow this proportionality
    for (size_t iIndex = 1; iIndex < adfA.size(); iIndex++)
        if (fabs(adfB[iIndex] - (adfA[iIndex] * dfRatio)) > 1e-6)
            return 0.0;

    return dfRatio;
}

/************************************************************************/
/*                           WritePOLYLINE()                            */
/************************************************************************/

OGRErr OGRDXFWriterLayer::WritePOLYLINE(OGRFeature *poFeature,
                                        const OGRGeometry *poGeom)

{
    /* -------------------------------------------------------------------- */
    /*      For now we handle multilinestrings by writing a series of       */
    /*      entities.                                                       */
    /* -------------------------------------------------------------------- */
    if (poGeom == nullptr)
        poGeom = poFeature->GetGeometryRef();

    if (poGeom->IsEmpty())
    {
        return OGRERR_NONE;
    }

    if (wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon ||
        wkbFlatten(poGeom->getGeometryType()) == wkbMultiLineString)
    {
        const OGRGeometryCollection *poGC = poGeom->toGeometryCollection();
        OGRErr eErr = OGRERR_NONE;
        for (auto &&poMember : *poGC)
        {
            eErr = WritePOLYLINE(poFeature, poMember);
            if (eErr != OGRERR_NONE)
                break;
        }

        return eErr;
    }

    /* -------------------------------------------------------------------- */
    /*      Polygons are written with on entity per ring.                   */
    /* -------------------------------------------------------------------- */
    if (wkbFlatten(poGeom->getGeometryType()) == wkbPolygon ||
        wkbFlatten(poGeom->getGeometryType()) == wkbTriangle)
    {
        const OGRPolygon *poPoly = poGeom->toPolygon();
        OGRErr eErr = OGRERR_NONE;
        for (auto &&poRing : *poPoly)
        {
            eErr = WritePOLYLINE(poFeature, poRing);
            if (eErr != OGRERR_NONE)
                break;
        }

        return eErr;
    }

    /* -------------------------------------------------------------------- */
    /*      Do we now have a geometry we can work with?                     */
    /* -------------------------------------------------------------------- */
    if (wkbFlatten(poGeom->getGeometryType()) != wkbLineString)
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;

    const OGRLineString *poLS = poGeom->toLineString();

    /* -------------------------------------------------------------------- */
    /*      Write as a lightweight polygon,                                 */
    /*       or as POLYLINE if the line contains different heights          */
    /* -------------------------------------------------------------------- */
    int bHasDifferentZ = FALSE;
    if (poLS->getGeometryType() == wkbLineString25D)
    {
        double z0 = poLS->getZ(0);
        for (int iVert = 0; iVert < poLS->getNumPoints(); iVert++)
        {
            if (z0 != poLS->getZ(iVert))
            {
                bHasDifferentZ = TRUE;
                break;
            }
        }
    }

    CorePropertiesType oCoreProperties;

    /* -------------------------------------------------------------------- */
    /*      Do we have styling information?                                 */
    /* -------------------------------------------------------------------- */
    OGRStyleTool *poTool = nullptr;
    OGRStyleMgr oSM;

    if (poFeature->GetStyleString() != nullptr)
    {
        oSM.InitFromFeature(poFeature);

        if (oSM.GetPartCount() > 0)
            poTool = oSM.GetPart(0);
    }

    /* -------------------------------------------------------------------- */
    /*      Handle a PEN tool to control drawing color and width.           */
    /* -------------------------------------------------------------------- */
    if (poTool && poTool->GetType() == OGRSTCPen)
    {
        OGRStylePen *poPen = cpl::down_cast<OGRStylePen *>(poTool);
        GBool bDefault;
        const char *pszColor = poPen->Color(bDefault);
        if (pszColor && !bDefault)
            oCoreProperties.emplace_back(PROP_RGBA_COLOR, pszColor);
    }

    WriteValue(0, bHasDifferentZ ? "POLYLINE" : "LWPOLYLINE");
    WriteCore(poFeature, oCoreProperties);
    if (bHasDifferentZ)
    {
        WriteValue(100, "AcDb3dPolyline");
        WriteValue(10, 0.0);
        WriteValue(20, 0.0);
        WriteValue(30, 0.0);
    }
    else
        WriteValue(100, "AcDbPolyline");
    if (EQUAL(poGeom->getGeometryName(), "LINEARRING"))
        WriteValue(70, 1 + (bHasDifferentZ ? 8 : 0));
    else
        WriteValue(70, 0 + (bHasDifferentZ ? 8 : 0));
    if (!bHasDifferentZ)
        WriteValue(90, poLS->getNumPoints());
    else
        WriteValue(66, "1");  // Vertex Flag

    /* -------------------------------------------------------------------- */
    /*      Handle a PEN tool to control drawing color and width.           */
    /*      Perhaps one day also dottedness, etc.                           */
    /* -------------------------------------------------------------------- */
    if (poTool && poTool->GetType() == OGRSTCPen)
    {
        OGRStylePen *poPen = cpl::down_cast<OGRStylePen *>(poTool);
        GBool bDefault;

        // we want to fetch the width in ground units.
        poPen->SetUnit(OGRSTUGround, 1.0);
        const double dfWidth = poPen->Width(bDefault);

        if (!bDefault)
            WriteValue(370, (int)floor(dfWidth * 100 + 0.5));
    }

    /* -------------------------------------------------------------------- */
    /*      Do we have a Linetype for the feature?                          */
    /* -------------------------------------------------------------------- */
    CPLString osLineType = poFeature->GetFieldAsString("Linetype");
    double dfLineTypeScale = 0.0;

    bool bGotLinetype = false;

    if (!osLineType.empty())
    {
        std::vector<double> adfLineType =
            poDS->oHeaderDS.LookupLineType(osLineType);

        if (adfLineType.empty() && oNewLineTypes.count(osLineType) > 0)
            adfLineType = oNewLineTypes[osLineType];

        if (!adfLineType.empty())
        {
            bGotLinetype = true;
            WriteValue(6, osLineType);

            // If the given linetype is proportional to the linetype data
            // in the style string, then apply a linetype scale
            if (poTool != nullptr && poTool->GetType() == OGRSTCPen)
            {
                std::vector<double> adfDefinition = PrepareLineTypeDefinition(
                    static_cast<OGRStylePen *>(poTool));

                if (!adfDefinition.empty())
                {
                    dfLineTypeScale =
                        IsLineTypeProportional(adfLineType, adfDefinition);

                    if (dfLineTypeScale != 0.0 &&
                        fabs(dfLineTypeScale - 1.0) > 1e-4)
                    {
                        WriteValue(48, dfLineTypeScale);
                    }
                }
            }
        }
    }

    if (!bGotLinetype && poTool != nullptr && poTool->GetType() == OGRSTCPen)
    {
        std::vector<double> adfDefinition =
            PrepareLineTypeDefinition(static_cast<OGRStylePen *>(poTool));

        if (!adfDefinition.empty())
        {
            // Is this definition already created and named?
            for (const auto &oPair : poDS->oHeaderDS.GetLineTypeTable())
            {
                dfLineTypeScale =
                    IsLineTypeProportional(oPair.second, adfDefinition);
                if (dfLineTypeScale != 0.0)
                {
                    osLineType = oPair.first;
                    break;
                }
            }

            if (dfLineTypeScale == 0.0)
            {
                for (const auto &oPair : oNewLineTypes)
                {
                    dfLineTypeScale =
                        IsLineTypeProportional(oPair.second, adfDefinition);
                    if (dfLineTypeScale != 0.0)
                    {
                        osLineType = oPair.first;
                        break;
                    }
                }
            }

            // If not, create an automatic name for it.
            if (osLineType == "")
            {
                dfLineTypeScale = 1.0;
                do
                {
                    osLineType.Printf("AutoLineType-%d", nNextAutoID++);
                } while (poDS->oHeaderDS.LookupLineType(osLineType).size() > 0);
            }

            // If it isn't already defined, add it now.
            if (poDS->oHeaderDS.LookupLineType(osLineType).empty() &&
                oNewLineTypes.count(osLineType) == 0)
            {
                oNewLineTypes[osLineType] = std::move(adfDefinition);
            }

            WriteValue(6, osLineType);

            if (dfLineTypeScale != 0.0 && fabs(dfLineTypeScale - 1.0) > 1e-4)
            {
                WriteValue(48, dfLineTypeScale);
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Write the vertices                                              */
    /* -------------------------------------------------------------------- */

    if (!bHasDifferentZ && poLS->getGeometryType() == wkbLineString25D)
    {
        // if LWPOLYLINE with Z write it only once
        if (!WriteValue(38, poLS->getZ(0)))
            return OGRERR_FAILURE;
    }

    for (int iVert = 0; iVert < poLS->getNumPoints(); iVert++)
    {
        if (bHasDifferentZ)
        {
            WriteValue(0, "VERTEX");
            WriteCore(poFeature, CorePropertiesType());
            WriteValue(100, "AcDbVertex");
            WriteValue(100, "AcDb3dPolylineVertex");
        }
        WriteValue(10, poLS->getX(iVert));
        if (!WriteValue(20, poLS->getY(iVert)))
            return OGRERR_FAILURE;

        if (bHasDifferentZ)
        {
            if (!WriteValue(30, poLS->getZ(iVert)))
                return OGRERR_FAILURE;
            WriteValue(70, 32);
        }
    }

    if (bHasDifferentZ)
    {
        WriteValue(0, "SEQEND");
        WriteCore(poFeature, CorePropertiesType());
    }

    delete poTool;

    return OGRERR_NONE;

#ifdef notdef
    /* -------------------------------------------------------------------- */
    /*      Alternate unmaintained implementation as a polyline entity.     */
    /* -------------------------------------------------------------------- */
    WriteValue(0, "POLYLINE");
    WriteCore(poFeature);
    WriteValue(100, "AcDbPolyline");
    if (EQUAL(poGeom->getGeometryName(), "LINEARRING"))
        WriteValue(70, 1);
    else
        WriteValue(70, 0);
    WriteValue(66, "1");

    for (int iVert = 0; iVert < poLS->getNumPoints(); iVert++)
    {
        WriteValue(0, "VERTEX");
        WriteValue(8, "0");
        WriteValue(10, poLS->getX(iVert));
        if (!WriteValue(20, poLS->getY(iVert)))
            return OGRERR_FAILURE;

        if (poLS->getGeometryType() == wkbLineString25D)
        {
            if (!WriteValue(30, poLS->getZ(iVert)))
                return OGRERR_FAILURE;
        }
    }

    WriteValue(0, "SEQEND");
    WriteValue(8, "0");

    return OGRERR_NONE;
#endif
}

/************************************************************************/
/*                             WriteHATCH()                             */
/************************************************************************/

OGRErr OGRDXFWriterLayer::WriteHATCH(OGRFeature *poFeature, OGRGeometry *poGeom)

{
    /* -------------------------------------------------------------------- */
    /*      For now we handle multipolygons by writing a series of          */
    /*      entities.                                                       */
    /* -------------------------------------------------------------------- */
    if (poGeom == nullptr)
        poGeom = poFeature->GetGeometryRef();

    if (poGeom->IsEmpty())
    {
        return OGRERR_NONE;
    }

    if (wkbFlatten(poGeom->getGeometryType()) == wkbMultiPolygon)
    {
        OGRErr eErr = OGRERR_NONE;
        for (auto &&poMember : poGeom->toMultiPolygon())
        {
            eErr = WriteHATCH(poFeature, poMember);
            if (eErr != OGRERR_NONE)
                break;
        }

        return eErr;
    }

    /* -------------------------------------------------------------------- */
    /*      Do we now have a geometry we can work with?                     */
    /* -------------------------------------------------------------------- */
    if (wkbFlatten(poGeom->getGeometryType()) != wkbPolygon &&
        wkbFlatten(poGeom->getGeometryType()) != wkbTriangle)
    {
        return OGRERR_UNSUPPORTED_GEOMETRY_TYPE;
    }

    CorePropertiesType oCoreProperties;

    /* -------------------------------------------------------------------- */
    /*      Do we have styling information?                                 */
    /* -------------------------------------------------------------------- */
    OGRStyleTool *poTool = nullptr;
    OGRStyleMgr oSM;

    if (poFeature->GetStyleString() != nullptr)
    {
        oSM.InitFromFeature(poFeature);

        if (oSM.GetPartCount() > 0)
            poTool = oSM.GetPart(0);
    }
    // Write style brush fore color
    std::string osBrushId;
    std::string osBackgroundColor;
    double dfSize = 1.0;
    double dfAngle = 0.0;
    if (poTool && poTool->GetType() == OGRSTCBrush)
    {
        OGRStyleBrush *poBrush = cpl::down_cast<OGRStyleBrush *>(poTool);
        GBool bDefault;

        const char *pszBrushId = poBrush->Id(bDefault);
        if (pszBrushId && !bDefault)
            osBrushId = pszBrushId;

        // null brush (transparent - no fill, irrespective of fc or bc values
        if (osBrushId == "ogr-brush-1")
        {
            oCoreProperties.emplace_back(PROP_RGBA_COLOR, "#00000000");
        }
        else
        {
            const char *pszColor = poBrush->ForeColor(bDefault);
            if (pszColor != nullptr && !bDefault)
                oCoreProperties.emplace_back(PROP_RGBA_COLOR, pszColor);

            const char *pszBGColor = poBrush->BackColor(bDefault);
            if (pszBGColor != nullptr && !bDefault)
                osBackgroundColor = pszBGColor;
        }

        double dfStyleSize = poBrush->Size(bDefault);
        if (!bDefault)
            dfSize = dfStyleSize;

        double dfStyleAngle = poBrush->Angle(bDefault);
        if (!bDefault)
            dfAngle = dfStyleAngle;
    }
    delete poTool;

    /* -------------------------------------------------------------------- */
    /*      Write as a hatch.                                               */
    /* -------------------------------------------------------------------- */
    WriteValue(0, "HATCH");
    WriteCore(poFeature, oCoreProperties);
    WriteValue(100, "AcDbHatch");

    // Figure out "average" elevation
    OGREnvelope3D oEnv;
    poGeom->getEnvelope(&oEnv);
    WriteValue(10, 0);  // elevation point X = 0
    WriteValue(20, 0);  // elevation point Y = 0
    // elevation point Z = constant elevation
    WriteValue(30, oEnv.MinZ + (oEnv.MaxZ - oEnv.MinZ) / 2);

    WriteValue(210, 0);    // extrusion direction X
    WriteValue(220, 0);    // extrusion direction Y
    WriteValue(230, 1.0);  // extrusion direction Z

    const char *pszPatternName = "SOLID";
    double dfPatternRotation = 0;

    // Cf https://ezdxf.readthedocs.io/en/stable/tutorials/hatch.html#predefined-hatch-pattern
    // for DXF standard hatch pattern names

    if (osBrushId.empty() || osBrushId == "ogr-brush-0")
    {
        // solid fill pattern
    }
    else if (osBrushId == "ogr-brush-2")
    {
        // horizontal line.
        pszPatternName = "ANSI31";
        dfPatternRotation = -45;
    }
    else if (osBrushId == "ogr-brush-3")
    {
        // vertical line.
        pszPatternName = "ANSI31";
        dfPatternRotation = 45;
    }
    else if (osBrushId == "ogr-brush-4")
    {
        // top-left to bottom-right diagonal hatch.
        pszPatternName = "ANSI31";
        dfPatternRotation = 90;
    }
    else if (osBrushId == "ogr-brush-5")
    {
        // bottom-left to top-right diagonal hatch
        pszPatternName = "ANSI31";
        dfPatternRotation = 0;
    }
    else if (osBrushId == "ogr-brush-6")
    {
        // cross hatch
        pszPatternName = "ANSI37";
        dfPatternRotation = 45;
    }
    else if (osBrushId == "ogr-brush-7")
    {
        // diagonal cross hatch
        pszPatternName = "ANSI37";
        dfPatternRotation = 0;
    }
    else
    {
        // solid fill pattern as a fallback
    }

    dfPatternRotation += dfAngle;

    WriteValue(2, pszPatternName);

    if (EQUAL(pszPatternName, "ANSI31") || EQUAL(pszPatternName, "ANSI37"))
    {
        WriteValue(70, 0);  // pattern fill
    }
    else
    {
        WriteValue(70, 1);  // solid fill
    }
    WriteValue(71, 0);  // associativity

/* -------------------------------------------------------------------- */
/*      Handle a PEN tool to control drawing color and width.           */
/*      Perhaps one day also dottedness, etc.                           */
/* -------------------------------------------------------------------- */
#ifdef notdef
    if (poTool && poTool->GetType() == OGRSTCPen)
    {
        OGRStylePen *poPen = (OGRStylePen *)poTool;
        GBool bDefault;

        if (poPen->Color(bDefault) != NULL && !bDefault)
            WriteValue(62, ColorStringToDXFColor(poPen->Color(bDefault)));

        double dfWidthInMM = poPen->Width(bDefault);

        if (!bDefault)
            WriteValue(370, (int)floor(dfWidthInMM * 100 + 0.5));
    }

    /* -------------------------------------------------------------------- */
    /*      Do we have a Linetype for the feature?                          */
    /* -------------------------------------------------------------------- */
    CPLString osLineType = poFeature->GetFieldAsString("Linetype");

    if (!osLineType.empty() &&
        (poDS->oHeaderDS.LookupLineType(osLineType) != NULL ||
         oNewLineTypes.count(osLineType) > 0))
    {
        // Already define -> just reference it.
        WriteValue(6, osLineType);
    }
    else if (poTool != NULL && poTool->GetType() == OGRSTCPen)
    {
        CPLString osDefinition = PrepareLineTypeDefinition(poFeature, poTool);

        if (osDefinition != "" && osLineType == "")
        {
            // Is this definition already created and named?
            std::map<CPLString, CPLString>::iterator it;

            for (it = oNewLineTypes.begin(); it != oNewLineTypes.end(); it++)
            {
                if ((*it).second == osDefinition)
                {
                    osLineType = (*it).first;
                    break;
                }
            }

            // create an automatic name for it.
            if (osLineType == "")
            {
                do
                {
                    osLineType.Printf("AutoLineType-%d", nNextAutoID++);
                } while (poDS->oHeaderDS.LookupLineType(osLineType) != NULL);
            }
        }

        // If it isn't already defined, add it now.
        if (osDefinition != "" && oNewLineTypes.count(osLineType) == 0)
        {
            oNewLineTypes[osLineType] = osDefinition;
            WriteValue(6, osLineType);
        }
    }
    delete poTool;
#endif

    /* -------------------------------------------------------------------- */
    /*      Process the loops (rings).                                      */
    /* -------------------------------------------------------------------- */
    const OGRPolygon *poPoly = poGeom->toPolygon();

    WriteValue(91, poPoly->getNumInteriorRings() + 1);

    for (auto &&poLR : *poPoly)
    {
        WriteValue(92, 2);  // Polyline
        WriteValue(72, 0);  // has bulge
        WriteValue(73, 1);  // is closed
        WriteValue(93, poLR->getNumPoints());

        for (int iVert = 0; iVert < poLR->getNumPoints(); iVert++)
        {
            WriteValue(10, poLR->getX(iVert));
            WriteValue(20, poLR->getY(iVert));
        }

        WriteValue(97, 0);  // 0 source boundary objects
    }

    WriteValue(75, 0);  // hatch style = Hatch "odd parity" area (Normal style)
    WriteValue(76, 1);  // hatch pattern type = predefined

    const auto roundIfClose = [](double x)
    {
        if (std::fabs(x - std::round(x)) < 1e-12)
            x = std::round(x);
        return x == 0 ? 0 : x;  // make sure we return positive zero
    };

    if (EQUAL(pszPatternName, "ANSI31"))
    {
        // Single line. With dfPatternRotation=0, this is a bottom-left to top-right diagonal hatch

        WriteValue(52, dfPatternRotation);  // Hatch pattern angle
        WriteValue(41, dfSize);             // Hatch pattern scale or spacing
        WriteValue(77, 0);  // Hatch pattern double flag : 0 = not double

        WriteValue(78, 1);  // Number of pattern definition lines

        const double angle = dfPatternRotation + 45.0;
        WriteValue(53, angle);  // Pattern line angle
        WriteValue(43, 0.0);    // Pattern line base point, X component
        WriteValue(44, 0.0);    // Pattern line base point, Y component
        WriteValue(45, dfSize * 3.175 *
                           roundIfClose(
                               cos((angle + 90.0) / 180 *
                                   M_PI)));  // Pattern line offset, X component
        WriteValue(46, dfSize * 3.175 *
                           roundIfClose(
                               sin((angle + 90.0) / 180 *
                                   M_PI)));  // Pattern line offset, Y component
        WriteValue(79, 0);                   // Number of dash items
    }
    else if (EQUAL(pszPatternName, "ANSI37"))
    {
        // cross hatch. With dfPatternRotation=0, lines are diagonals

        WriteValue(52, dfPatternRotation);  // Hatch pattern angle
        WriteValue(41, dfSize);             // Hatch pattern scale or spacing
        WriteValue(77, 0);  // Hatch pattern double flag : 0 = not double

        WriteValue(78, 2);  // Number of pattern definition lines

        const double angle1 = dfPatternRotation + 45;
        WriteValue(53, angle1);  // Pattern line angle
        WriteValue(43, 0.0);     // Pattern line base point, X component
        WriteValue(44, 0.0);     // Pattern line base point, Y component
        WriteValue(45, dfSize * 3.175 *
                           roundIfClose(
                               cos((angle1 + 90.0) / 180 *
                                   M_PI)));  // Pattern line offset, X component
        WriteValue(46, dfSize * 3.175 *
                           roundIfClose(
                               sin((angle1 + 90.0) / 180 *
                                   M_PI)));  // Pattern line offset, Y component
        WriteValue(79, 0);                   // Number of dash items

        const double angle2 = dfPatternRotation + 135;
        WriteValue(53, angle2);  // Pattern line angle
        WriteValue(43, 0.0);     // Pattern line base point, X component
        WriteValue(44, 0.0);     // Pattern line base point, Y component
        WriteValue(45, dfSize * 3.175 *
                           roundIfClose(
                               cos((angle2 + 90.0) / 180 *
                                   M_PI)));  // Pattern line offset, X component
        WriteValue(46, dfSize * 3.175 *
                           roundIfClose(
                               sin((angle2 + 90.0) / 180 *
                                   M_PI)));  // Pattern line offset, Y component
        WriteValue(79, 0);                   // Number of dash items
    }

    WriteValue(98, 0);  // 0 seed points

    // Deal with brush background color
    if (!osBackgroundColor.empty())
    {
        bool bPerfectMatch = false;
        int nColor =
            ColorStringToDXFColor(osBackgroundColor.c_str(), bPerfectMatch);
        if (nColor >= 0)
        {
            WriteValue(1001, "HATCHBACKGROUNDCOLOR");
            if (bPerfectMatch)
            {
                // C3 is top 8 bit means an indexed color
                unsigned nRGBColorUnsigned =
                    (static_cast<unsigned>(0xC3) << 24) |
                    ((nColor & 0xff) << 0);
                // Convert to signed (negative) value
                int nRGBColorSigned;
                memcpy(&nRGBColorSigned, &nRGBColorUnsigned,
                       sizeof(nRGBColorSigned));
                WriteValue(1071, nRGBColorSigned);
            }
            else
            {
                unsigned int nRed = 0;
                unsigned int nGreen = 0;
                unsigned int nBlue = 0;
                unsigned int nOpacity = 255;

                const int nCount =
                    sscanf(osBackgroundColor.c_str(), "#%2x%2x%2x%2x", &nRed,
                           &nGreen, &nBlue, &nOpacity);
                if (nCount >= 3)
                {
                    // C2 is top 8 bit means a true color
                    unsigned nRGBColorUnsigned =
                        (static_cast<unsigned>(0xC2) << 24) |
                        ((nRed & 0xff) << 16) | ((nGreen & 0xff) << 8) |
                        ((nBlue & 0xff) << 0);
                    // Convert to signed (negative) value
                    int nRGBColorSigned;
                    memcpy(&nRGBColorSigned, &nRGBColorUnsigned,
                           sizeof(nRGBColorSigned));
                    WriteValue(1071, nRGBColorSigned);
                }
            }
        }
    }

    return OGRERR_NONE;

#ifdef notdef
    /* -------------------------------------------------------------------- */
    /*      Alternate unmaintained implementation as a polyline entity.     */
    /* -------------------------------------------------------------------- */
    WriteValue(0, "POLYLINE");
    WriteCore(poFeature);
    WriteValue(100, "AcDbPolyline");
    if (EQUAL(poGeom->getGeometryName(), "LINEARRING"))
        WriteValue(70, 1);
    else
        WriteValue(70, 0);
    WriteValue(66, "1");

    for (int iVert = 0; iVert < poLS->getNumPoints(); iVert++)
    {
        WriteValue(0, "VERTEX");
        WriteValue(8, "0");
        WriteValue(10, poLS->getX(iVert));
        if (!WriteValue(20, poLS->getY(iVert)))
            return OGRERR_FAILURE;

        if (poLS->getGeometryType() == wkbLineString25D)
        {
            if (!WriteValue(30, poLS->getZ(iVert)))
                return OGRERR_FAILURE;
        }
    }

    WriteValue(0, "SEQEND");
    WriteValue(8, "0");

    return OGRERR_NONE;
#endif
}

/************************************************************************/
/*                           ICreateFeature()                           */
/************************************************************************/

OGRErr OGRDXFWriterLayer::ICreateFeature(OGRFeature *poFeature)

{
    OGRGeometry *poGeom = poFeature->GetGeometryRef();
    OGRwkbGeometryType eGType = wkbNone;

    if (poGeom != nullptr)
    {
        if (!poGeom->IsEmpty())
        {
            OGREnvelope sEnvelope;
            poGeom->getEnvelope(&sEnvelope);
            poDS->UpdateExtent(&sEnvelope);
        }
        eGType = wkbFlatten(poGeom->getGeometryType());
    }

    if (eGType == wkbPoint)
    {
        const char *pszBlockName = poFeature->GetFieldAsString("BlockName");

        // We don't want to treat as a blocks ref if the block is not defined
        if (pszBlockName &&
            poDS->oHeaderDS.LookupBlock(pszBlockName) == nullptr)
        {
            if (poDS->poBlocksLayer == nullptr ||
                poDS->poBlocksLayer->FindBlock(pszBlockName) == nullptr)
                pszBlockName = nullptr;
        }

        if (pszBlockName != nullptr)
            return WriteINSERT(poFeature);

        else if (poFeature->GetStyleString() != nullptr &&
                 STARTS_WITH_CI(poFeature->GetStyleString(), "LABEL"))
            return WriteTEXT(poFeature);
        else
            return WritePOINT(poFeature);
    }
    else if (eGType == wkbLineString || eGType == wkbMultiLineString)
        return WritePOLYLINE(poFeature);

    else if (eGType == wkbPolygon || eGType == wkbTriangle ||
             eGType == wkbMultiPolygon)
    {
        if (bWriteHatch)
            return WriteHATCH(poFeature);
        else
            return WritePOLYLINE(poFeature);
    }

    // Explode geometry collections into multiple entities.
    else if (eGType == wkbGeometryCollection || eGType == wkbMultiPoint)
    {
        OGRGeometryCollection *poGC =
            poFeature->StealGeometry()->toGeometryCollection();
        for (auto &&poMember : poGC)
        {
            poFeature->SetGeometry(poMember);

            OGRErr eErr = CreateFeature(poFeature);

            if (eErr != OGRERR_NONE)
            {
                delete poGC;
                return eErr;
            }
        }

        poFeature->SetGeometryDirectly(poGC);
        return OGRERR_NONE;
    }
    else
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "No known way to write feature with geometry '%s'.",
                 OGRGeometryTypeToName(eGType));
        return OGRERR_FAILURE;
    }
}

/************************************************************************/
/*                       ColorStringToDXFColor()                        */
/************************************************************************/

int OGRDXFWriterLayer::ColorStringToDXFColor(const char *pszRGB,
                                             bool &bPerfectMatch)

{
    bPerfectMatch = false;

    /* -------------------------------------------------------------------- */
    /*      Parse the RGB string.                                           */
    /* -------------------------------------------------------------------- */
    if (pszRGB == nullptr)
        return -1;

    unsigned int nRed = 0;
    unsigned int nGreen = 0;
    unsigned int nBlue = 0;
    unsigned int nOpacity = 255;

    const int nCount =
        sscanf(pszRGB, "#%2x%2x%2x%2x", &nRed, &nGreen, &nBlue, &nOpacity);

    if (nCount < 3)
        return -1;

    /* -------------------------------------------------------------------- */
    /*      Find near color in DXF palette.                                 */
    /* -------------------------------------------------------------------- */
    const unsigned char *pabyDXFColors = ACGetColorTable();
    int nMinDist = 768;
    int nBestColor = -1;

    for (int i = 1; i < 256; i++)
    {
        const int nDist =
            std::abs(static_cast<int>(nRed) - pabyDXFColors[i * 3 + 0]) +
            std::abs(static_cast<int>(nGreen) - pabyDXFColors[i * 3 + 1]) +
            std::abs(static_cast<int>(nBlue) - pabyDXFColors[i * 3 + 2]);

        if (nDist < nMinDist)
        {
            nBestColor = i;
            nMinDist = nDist;
            if (nMinDist == 0)
            {
                bPerfectMatch = true;
                break;
            }
        }
    }

    return nBestColor;
}

/************************************************************************/
/*                             GetDataset()                             */
/************************************************************************/

GDALDataset *OGRDXFWriterLayer::GetDataset()
{
    return poDS;
}
