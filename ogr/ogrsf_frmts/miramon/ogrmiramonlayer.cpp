/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRMiraMonLayer class.
 * Author:   Abel Pau, a.pau@creaf.uab.cat
 *
 ******************************************************************************
 * Copyright (c) 2023,  MiraMon
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

#include "mm_wrlayr.h"
#include "ogrmiramon.h"
#include "cpl_conv.h"
#include "ogr_p.h"

#include <algorithm>

/************************************************************************/
/*                            OGRMiraMonLayer()                         */
/************************************************************************/

OGRMiraMonLayer::OGRMiraMonLayer(const char *pszFilename, VSILFILE *fp,
                         const OGRSpatialReference *poSRS, int bUpdateIn)
    : poFeatureDefn(nullptr), iNextFID(0), bUpdate(CPL_TO_BOOL(bUpdateIn)),
      // Assume header complete in readonly mode.
      bHeaderComplete(CPL_TO_BOOL(!bUpdate)), bRegionComplete(false),
      nRegionOffset(0),
      m_fp(fp ? fp : VSIFOpenL(pszFilename, (bUpdateIn ? "r+" : "r"))),
      papszKeyedValues(nullptr), bValidFile(false), hMMFeature(),
      hMiraMonLayer(), pMMHeader(), hLayerDB()
{
    if (m_fp == nullptr)
        return;

    /* -------------------------------------------------------------------- */
    /*      Create the feature definition                                   */
    /* -------------------------------------------------------------------- */
    poFeatureDefn = new OGRFeatureDefn(CPLGetBasename(pszFilename));
    SetDescription(poFeatureDefn->GetName());
    poFeatureDefn->Reference();

    /* -------------------------------------------------------------------- */
    /*      Read the header.                                                */
    /* -------------------------------------------------------------------- */
    if (!STARTS_WITH(pszFilename, "/vsistdout"))
    {
        CPLString osFieldNames;
        CPLString osFieldTypes;
        CPLString osGeometryType;
        CPLString osRegion;
        CPLString osWKT;
        CPLString osProj4;
        CPLString osEPSG;
        int nMMVersion;

        MMReadHeader(m_fp, &pMMHeader);
        MMInitFeature(&hMMFeature);

        nMMVersion=MMGetVectorVersion(&pMMHeader);
        if (nMMVersion == MM_UNKNOWN_VERSION)
            bValidFile = false;
        if(pMMHeader.aFileType[0]=='P' &&
                pMMHeader.aFileType[1]=='N' &&
                pMMHeader.aFileType[2]=='T')
        {
            if (pMMHeader.Flag & MM_LAYER_3D_INFO)
            {
                poFeatureDefn->SetGeomType(wkbPoint25D);
                if (MMInitLayer(&hMiraMonLayer, pszFilename, nMMVersion,
                        MM_LayerType_Point3d, 0, NULL))
                    bValidFile = false;
            }
            else
            {
                poFeatureDefn->SetGeomType(wkbPoint);
                if(MMInitLayer(&hMiraMonLayer, pszFilename, nMMVersion,
                        MM_LayerType_Point, 0, NULL))
                    bValidFile = false;
            }
            hMiraMonLayer.bIsPoint=1;
        }
        else if (pMMHeader.aFileType[0] == 'A' &&
            pMMHeader.aFileType[1] == 'R' &&
            pMMHeader.aFileType[2] == 'C')
        {
            if (pMMHeader.Flag & MM_LAYER_3D_INFO)
            {
                poFeatureDefn->SetGeomType(wkbLineString25D);
                if(MMInitLayer(&hMiraMonLayer, pszFilename, nMMVersion,
                        MM_LayerType_Arc3d, 0, NULL))
                    bValidFile = false;
            }
            else
            {
                poFeatureDefn->SetGeomType(wkbLineString);
                if(MMInitLayer(&hMiraMonLayer, pszFilename, nMMVersion,
                        MM_LayerType_Arc, 0, NULL))
                    bValidFile = false;
            }
            hMiraMonLayer.bIsArc=1;
        }
        else if(pMMHeader.aFileType[0]=='P' &&
                pMMHeader.aFileType[1]=='O' &&
                pMMHeader.aFileType[2]=='L')
        {
            // 3D
            if (pMMHeader.Flag & MM_LAYER_3D_INFO)
            {
                if (pMMHeader.Flag & MM_LAYER_MULTIPOLYGON)
                    poFeatureDefn->SetGeomType(wkbMultiPolygon25D);
                else
                    poFeatureDefn->SetGeomType(wkbPolygon25D);
                if(MMInitLayer(&hMiraMonLayer, pszFilename, nMMVersion,
                        MM_LayerType_Pol3d, 0, NULL))
                    bValidFile = false;
            }
            else
            {
                if (pMMHeader.Flag & MM_LAYER_MULTIPOLYGON)
                    poFeatureDefn->SetGeomType(wkbMultiPolygon);
                else
                    poFeatureDefn->SetGeomType(wkbPolygon);
                if(MMInitLayer(&hMiraMonLayer, pszFilename, nMMVersion,
                        MM_LayerType_Pol, 0, NULL))
                    bValidFile = false;
            }
            hMiraMonLayer.bIsPolygon=1;
        }

        /* --------------------------------------------------------------------
         */
        /*      Handle coordinate system. */
        /* --------------------------------------------------------------------
         */
        if (osWKT.length())
        {
            m_poSRS = new OGRSpatialReference();
            m_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (m_poSRS->importFromWkt(osWKT.c_str()) != OGRERR_NONE)
            {
                delete m_poSRS;
                m_poSRS = nullptr;
            }
        }
        else if (osEPSG.length())
        {
            m_poSRS = new OGRSpatialReference();
            m_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (m_poSRS->importFromEPSG(atoi(osEPSG)) != OGRERR_NONE)
            {
                delete m_poSRS;
                m_poSRS = nullptr;
            }
        }
        else if (osProj4.length())
        {
            m_poSRS = new OGRSpatialReference();
            m_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
            if (m_poSRS->importFromProj4(osProj4) != OGRERR_NONE)
            {
                delete m_poSRS;
                m_poSRS = nullptr;
            }
        }

        /* -----------------------------------------------------------------*/
        /*      Process fields.                                             */
        /* -----------------------------------------------------------------*/
        //MMReadBD_XP();
        if (osFieldNames.length() || osFieldTypes.length())
        {
            char **papszFN =
                CSLTokenizeStringComplex(osFieldNames, "|", TRUE, TRUE);
            char **papszFT =
                CSLTokenizeStringComplex(osFieldTypes, "|", TRUE, TRUE);
            const int nFNCount = CSLCount(papszFN);
            const int nFTCount = CSLCount(papszFT);
            const int nFieldCount = std::max(nFNCount, nFTCount);

            for (int iField = 0; iField < nFieldCount; iField++)
            {
                OGRFieldDefn oField("", OFTString);

                if (iField < nFNCount)
                    oField.SetName(papszFN[iField]);
                else
                    oField.SetName(CPLString().Printf("Field_%d", iField + 1));

                if (iField < nFTCount)
                {
                    if (EQUAL(papszFT[iField], "integer"))
                        oField.SetType(OFTInteger);
                    else if (EQUAL(papszFT[iField], "double"))
                        oField.SetType(OFTReal);
                    else if (EQUAL(papszFT[iField], "datetime"))
                        oField.SetType(OFTDateTime);
                }

                poFeatureDefn->AddFieldDefn(&oField);
            }

            CSLDestroy(papszFN);
            CSLDestroy(papszFT);
        }
    }
    else
    {
        if (poSRS)
        {
            m_poSRS = poSRS->Clone();
            m_poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
        }
    }

    poFeatureDefn->GetGeomFieldDefn(0)->SetSpatialRef(m_poSRS);

    bValidFile = true;
}

/************************************************************************/
/*                           ~OGRMiraMonLayer()                           */
/************************************************************************/

OGRMiraMonLayer::~OGRMiraMonLayer()

{
    if (m_nFeaturesRead > 0 && poFeatureDefn != nullptr)
    {
        CPLDebug("MiraMon", "%d features read on layer '%s'.",
                 static_cast<int>(m_nFeaturesRead), poFeatureDefn->GetName());
    }

    /* -------------------------------------------------------------------- */
    /*      Write out the region bounds if we know where they go, and we    */
    /*      are in update mode.                                             */
    /* -------------------------------------------------------------------- */
    MMCloseLayer(&hMiraMonLayer);
	MMFreeLayer(&hMiraMonLayer);

    /* -------------------------------------------------------------------- */
    /*      Clean up.                                                       */
    /* -------------------------------------------------------------------- */
    CSLDestroy(papszKeyedValues);

    if (poFeatureDefn)
        poFeatureDefn->Release();

    if (m_poSRS)
        m_poSRS->Release();

    if (m_fp != nullptr)
        VSIFCloseL(m_fp);
}

/************************************************************************/
/*                              ReadLine()                              */
/*                                                                      */
/*      Read a line into osLine.  If it is a comment line with @        */
/*      keyed values, parse out the keyed values into                   */
/*      papszKeyedValues.                                               */
/************************************************************************/

bool OGRMiraMonLayer::ReadLine()

{
    /* -------------------------------------------------------------------- */
    /*      Clear last line.                                                */
    /* -------------------------------------------------------------------- */
    osLine.erase();
    if (papszKeyedValues)
    {
        CSLDestroy(papszKeyedValues);
        papszKeyedValues = nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Read newline.                                                   */
    /* -------------------------------------------------------------------- */
    const char *pszLine = CPLReadLineL(m_fp);
    if (pszLine == nullptr)
        return false;  // end of file.

    osLine = pszLine;

    /* -------------------------------------------------------------------- */
    /*      If this is a comment line with keyed values, parse them.        */
    /* -------------------------------------------------------------------- */

    if (osLine[0] != '#' || osLine.find_first_of('@') == std::string::npos)
        return true;

    CPLStringList aosKeyedValues;
    for (size_t i = 0; i < osLine.length(); i++)
    {
        if (osLine[i] == '@' && i + 2 <= osLine.size())
        {
            bool bInQuotes = false;

            size_t iValEnd = i + 2;  // Used after for.
            for (; iValEnd < osLine.length(); iValEnd++)
            {
                if (!bInQuotes && isspace((unsigned char)osLine[iValEnd]))
                    break;

                if (bInQuotes && iValEnd < osLine.length() - 1 &&
                    osLine[iValEnd] == '\\')
                {
                    iValEnd++;
                }
                else if (osLine[iValEnd] == '"')
                    bInQuotes = !bInQuotes;
            }

            const CPLString osValue = osLine.substr(i + 2, iValEnd - i - 2);

            // Unecape contents
            char *pszUEValue =
                CPLUnescapeString(osValue, nullptr, CPLES_BackslashQuotable);

            CPLString osKeyValue = osLine.substr(i + 1, 1);
            osKeyValue += pszUEValue;
            CPLFree(pszUEValue);
            aosKeyedValues.AddString(osKeyValue);

            i = iValEnd;
        }
    }
    papszKeyedValues = aosKeyedValues.StealList();

    return true;
}

/************************************************************************/
/*                            ResetReading()                            */
/************************************************************************/

void OGRMiraMonLayer::ResetReading()

{
    if (iNextFID == 0)
        return;

    iNextFID = 0;
    VSIFSeekL(m_fp, 0, SEEK_SET);
    ReadLine();
}

/************************************************************************/
/*                          ScanAheadForHole()                          */
/*                                                                      */
/*      Scan ahead to see if the next geometry is a hole.  If so        */
/*      return true, otherwise seek back to where we were and return    */
/*      false.                                                          */
/************************************************************************/

bool OGRMiraMonLayer::ScanAheadForHole()

{
    const CPLString osSavedLine = osLine;
    const vsi_l_offset nSavedLocation = VSIFTellL(m_fp);

    while (ReadLine() && osLine[0] == '#')
    {
        if (papszKeyedValues != nullptr && papszKeyedValues[0][0] == 'H')
            return true;
    }

    VSIFSeekL(m_fp, nSavedLocation, SEEK_SET);
    osLine = osSavedLine;

    // We do not actually restore papszKeyedValues, but we
    // assume it does not matter since this method is only called
    // when processing the '>' line.

    return false;
}

/************************************************************************/
/*                           NextIsFeature()                            */
/*                                                                      */
/*      Returns true if the next line is a feature attribute line.      */
/*      This generally indicates the end of a multilinestring or        */
/*      multipolygon feature.                                           */
/************************************************************************/

bool OGRMiraMonLayer::NextIsFeature()

{
    const CPLString osSavedLine = osLine;
    const vsi_l_offset nSavedLocation = VSIFTellL(m_fp);
    bool bReturn = false;

    ReadLine();

    if (osLine[0] == '#' && strstr(osLine, "@D") != nullptr)
        bReturn = true;

    VSIFSeekL(m_fp, nSavedLocation, SEEK_SET);
    osLine = osSavedLine;

    // We do not actually restore papszKeyedValues, but we
    // assume it does not matter since this method is only called
    // when processing the '>' line.

    return bReturn;
}

/************************************************************************/
/*                         GetNextRawFeature()                          */
/************************************************************************/

OGRFeature *OGRMiraMonLayer::GetNextRawFeature()

{
#if 0
    bool bMultiVertex =
        poFeatureDefn->GetGeomType() != wkbPoint
        && poFeatureDefn->GetGeomType() != wkbUnknown;
#endif
    CPLString osFieldData;
    OGRGeometry *poGeom = nullptr;

    /* -------------------------------------------------------------------- */
    /*      Read lines associated with this feature.                        */
    /* -------------------------------------------------------------------- */
    for (; true; ReadLine())
    {
        if (osLine.length() == 0)
            break;

        if (osLine[0] == '>')
        {
            OGRwkbGeometryType eType = wkbUnknown;
            if (poGeom)
                eType = wkbFlatten(poGeom->getGeometryType());
            if (eType == wkbMultiPolygon)
            {
                OGRMultiPolygon *poMP = poGeom->toMultiPolygon();
                if (ScanAheadForHole())
                {
                    // Add a hole to the current polygon.
                    poMP->getGeometryRef(poMP->getNumGeometries() - 1)
                        ->addRingDirectly(new OGRLinearRing());
                }
                else if (!NextIsFeature())
                {
                    OGRPolygon *poPoly = new OGRPolygon();

                    poPoly->addRingDirectly(new OGRLinearRing());

                    poMP->addGeometryDirectly(poPoly);
                }
                else
                    break; /* done geometry */
            }
            else if (eType == wkbPolygon)
            {
                if (ScanAheadForHole())
                    poGeom->toPolygon()->addRingDirectly(new OGRLinearRing());
                else
                    break; /* done geometry */
            }
            else if (eType == wkbMultiLineString && !NextIsFeature())
            {
                poGeom->toMultiLineString()->addGeometryDirectly(
                    new OGRLineString());
            }
            else if (poGeom != nullptr)
            {
                break;
            }
            else if (poFeatureDefn->GetGeomType() == wkbUnknown)
            {
                poFeatureDefn->SetGeomType(wkbLineString);
                // bMultiVertex = true;
            }
        }
        else if (osLine[0] == '#')
        {
            for (int i = 0;
                 papszKeyedValues != nullptr && papszKeyedValues[i] != nullptr;
                 i++)
            {
                if (papszKeyedValues[i][0] == 'D')
                    osFieldData = papszKeyedValues[i] + 1;
            }
        }
        else
        {
            // Parse point line.
            double dfX = 0.0;
            double dfY = 0.0;
            double dfZ = 0.0;
            const int nDim = CPLsscanf(osLine, "%lf %lf %lf", &dfX, &dfY, &dfZ);

            if (nDim >= 2)
            {
                if (poGeom == nullptr)
                {
                    switch (poFeatureDefn->GetGeomType())
                    {
                        case wkbLineString:
                            poGeom = new OGRLineString();
                            break;

                        case wkbPolygon:
                        {
                            OGRPolygon *poPoly = new OGRPolygon();
                            poGeom = poPoly;
                            poPoly->addRingDirectly(new OGRLinearRing());
                            break;
                        }

                        case wkbMultiPolygon:
                        {
                            OGRPolygon *poPoly = new OGRPolygon();
                            poPoly->addRingDirectly(new OGRLinearRing());

                            OGRMultiPolygon *poMP = new OGRMultiPolygon();
                            poGeom = poMP;
                            poMP->addGeometryDirectly(poPoly);
                        }
                        break;

                        case wkbMultiPoint:
                            poGeom = new OGRMultiPoint();
                            break;

                        case wkbMultiLineString:
                        {
                            OGRMultiLineString *poMLS =
                                new OGRMultiLineString();
                            poGeom = poMLS;
                            poMLS->addGeometryDirectly(new OGRLineString());
                            break;
                        }

                        case wkbPoint:
                        case wkbUnknown:
                        default:
                            poGeom = new OGRPoint();
                            break;
                    }
                }

                CPLAssert(poGeom != nullptr);
                // cppcheck-suppress nullPointerRedundantCheck
                switch (wkbFlatten(poGeom->getGeometryType()))
                {
                    case wkbPoint:
                    {
                        OGRPoint *poPoint = poGeom->toPoint();
                        poPoint->setX(dfX);
                        poPoint->setY(dfY);
                        if (nDim == 3)
                            poPoint->setZ(dfZ);
                        break;
                    }

                    case wkbLineString:
                    {
                        OGRLineString *poLS = poGeom->toLineString();
                        if (nDim == 3)
                            poLS->addPoint(dfX, dfY, dfZ);
                        else
                            poLS->addPoint(dfX, dfY);
                        break;
                    }

                    case wkbPolygon:
                    case wkbMultiPolygon:
                    {
                        OGRPolygon *poPoly = nullptr;

                        if (wkbFlatten(poGeom->getGeometryType()) ==
                            wkbMultiPolygon)
                        {
                            OGRMultiPolygon *poMP = poGeom->toMultiPolygon();
                            poPoly = poMP->getGeometryRef(
                                poMP->getNumGeometries() - 1);
                        }
                        else
                            poPoly = poGeom->toPolygon();

                        OGRLinearRing *poRing = nullptr;
                        if (poPoly->getNumInteriorRings() == 0)
                            poRing = poPoly->getExteriorRing();
                        else
                            poRing = poPoly->getInteriorRing(
                                poPoly->getNumInteriorRings() - 1);

                        if (nDim == 3)
                            poRing->addPoint(dfX, dfY, dfZ);
                        else
                            poRing->addPoint(dfX, dfY);
                    }
                    break;

                    case wkbMultiLineString:
                    {
                        OGRMultiLineString *poML = poGeom->toMultiLineString();
                        OGRLineString *poLine =
                            poML->getGeometryRef(poML->getNumGeometries() - 1);

                        if (nDim == 3)
                            poLine->addPoint(dfX, dfY, dfZ);
                        else
                            poLine->addPoint(dfX, dfY);
                    }
                    break;

                    default:
                        CPLAssert(false);
                }
            }
        }

        if (poGeom && wkbFlatten(poGeom->getGeometryType()) == wkbPoint)
        {
            ReadLine();
            break;
        }
    }

    if (poGeom == nullptr)
        return nullptr;

    /* -------------------------------------------------------------------- */
    /*      Create feature.                                                 */
    /* -------------------------------------------------------------------- */
    OGRFeature *poFeature = new OGRFeature(poFeatureDefn);
    poGeom->assignSpatialReference(m_poSRS);
    poFeature->SetGeometryDirectly(poGeom);
    poFeature->SetFID(iNextFID++);

    /* -------------------------------------------------------------------- */
    /*      Process field values.                                           */
    /* -------------------------------------------------------------------- */
    char **papszFD = CSLTokenizeStringComplex(osFieldData, "|", TRUE, TRUE);

    for (int iField = 0; papszFD != nullptr && papszFD[iField] != nullptr;
         iField++)
    {
        if (iField >= poFeatureDefn->GetFieldCount())
            break;

        poFeature->SetField(iField, papszFD[iField]);
    }

    CSLDestroy(papszFD);

    m_nFeaturesRead++;

    return poFeature;
}

/************************************************************************/
/*                         GetFeatureCount()                            */
/************************************************************************/
GIntBig OGRMiraMonLayer::GetFeatureCount(int bForce)
{
    return (GIntBig)hMiraMonLayer.TopHeader.nElemCount;
}

/************************************************************************/
/*                           CompleteHeader()                           */
/*                                                                      */
/*      Finish writing out the header with field definitions and the    */
/*      layer geometry type.                                            */
/************************************************************************/

OGRErr OGRMiraMonLayer::CompleteHeader(OGRGeometry *poThisGeom)

{
    /* -------------------------------------------------------------------- */
    /*      If we do not already have a geometry type, try to work one      */
    /*      out and write it now.                                           */
    /* -------------------------------------------------------------------- */
    if (poFeatureDefn->GetGeomType() == wkbUnknown && poThisGeom != nullptr)
    {
        poFeatureDefn->SetGeomType(wkbFlatten(poThisGeom->getGeometryType()));

        const char *pszGeom = nullptr;
        switch (wkbFlatten(poFeatureDefn->GetGeomType()))
        {
            case wkbPoint:
                pszGeom = " @GPOINT";
                break;
            case wkbLineString:
                pszGeom = " @GLINESTRING";
                break;
            case wkbPolygon:
                pszGeom = " @GPOLYGON";
                break;
            case wkbMultiPoint:
                pszGeom = " @GMULTIPOINT";
                break;
            case wkbMultiLineString:
                pszGeom = " @GMULTILINESTRING";
                break;
            case wkbMultiPolygon:
                pszGeom = " @GMULTIPOLYGON";
                break;
            default:
                pszGeom = "";
                break;
        }

        VSIFPrintfL(m_fp, "#%s\n", pszGeom);
    }

    /* -------------------------------------------------------------------- */
    /*      Prepare and write the field names and types.                    */
    /* -------------------------------------------------------------------- */
    CPLString osFieldNames;
    CPLString osFieldTypes;

    for (int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++)
    {
        if (iField > 0)
        {
            osFieldNames += "|";
            osFieldTypes += "|";
        }

        osFieldNames += poFeatureDefn->GetFieldDefn(iField)->GetNameRef();
        switch (poFeatureDefn->GetFieldDefn(iField)->GetType())
        {
            case OFTInteger:
                osFieldTypes += "integer";
                break;

            case OFTReal:
                osFieldTypes += "double";
                break;

            case OFTDateTime:
                osFieldTypes += "datetime";
                break;

            default:
                osFieldTypes += "string";
                break;
        }
    }

    if (poFeatureDefn->GetFieldCount() > 0)
    {
        VSIFPrintfL(m_fp, "# @N%s\n", osFieldNames.c_str());
        VSIFPrintfL(m_fp, "# @T%s\n", osFieldTypes.c_str());
    }

    /* -------------------------------------------------------------------- */
    /*      Mark the end of the header, and start of feature data.          */
    /* -------------------------------------------------------------------- */
    VSIFPrintfL(m_fp, "# FEATURE_DATA\n");

    bHeaderComplete = true;
    bRegionComplete = true;  // no feature written, so we know them all!

    return OGRERR_NONE;
}

/************************************************************************/
/*                           ICreateFeature()                            */
/************************************************************************/

OGRErr OGRMiraMonLayer::ICreateFeature(OGRFeature *poFeature)

{
    OGRErr eErr = OGRERR_NONE;

    if (!bUpdate)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Cannot create features on read-only dataset.");
        return OGRERR_FAILURE;
    }

    /* -------------------------------------------------------------------- */
    /*      Write out the feature                                           */
    /* -------------------------------------------------------------------- */
    OGRGeometry *poGeom = poFeature->GetGeometryRef();

    if (poGeom == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Features without geometry not supported by MM writer.");
        return OGRERR_FAILURE;
    }

    if (poFeatureDefn->GetGeomType() == wkbUnknown)
        poFeatureDefn->SetGeomType(wkbFlatten(poGeom->getGeometryType()));

    /* -------------------------------------------------------------------- */
    /*      Write Geometry                                                  */
    /* -------------------------------------------------------------------- */
    MMResetFeature(&hMMFeature);
    // Reads all coordinates
    eErr = WriteGeometry(OGRGeometry::ToHandle(poGeom), true, false, poFeature);

    // Writes them to the disk
    if(eErr == OGRERR_NONE)
        return WriteGeometry(OGRGeometry::ToHandle(poGeom), true, true, poFeature);
    else
        return eErr;

}

/************************************************************************/
/*                           WriteGeometry()                            */
/*                                                                      */
/*      Write a geometry to the file.  If bExternalRing is true it      */
/*      means the ring is being processed is external.                  */
/*      If bWriteNow is true it means all readed coorinates are in the  */
/*      MiraMon Feature Object and can be written.                      */
/*                                                                      */
/************************************************************************/

OGRErr OGRMiraMonLayer::WriteGeometry(OGRGeometryH hGeom,
                                        bool bExternalRing,
                                        bool bWriteNow,
                                        OGRFeature *poFeature)

{
    if (!bWriteNow)
    {
        /* -------------------------------------------------------------------- */
        /*      This is a geometry with sub-geometries.                         */
        /* -------------------------------------------------------------------- */
        if (OGR_G_GetGeometryCount(hGeom) > 0)
        {
            OGRErr eErr = OGRERR_NONE;
            int nGeom=OGR_G_GetGeometryCount(hGeom);

            for (int iGeom = 0;
                 iGeom < nGeom && eErr == OGRERR_NONE;
                 iGeom++)
            {
                // We need to inform ig the ring is external or not
                // (only in polygons)
                if (wkbFlatten(OGR_G_GetGeometryType(hGeom)) == wkbPolygon)
                {
                    if (iGeom == 0)
                        bExternalRing = true;
                    else
                        bExternalRing = false;
                }

                eErr =
                    WriteGeometry(OGR_G_GetGeometryRef(hGeom, iGeom),
                                bExternalRing, false, poFeature);
            }
            return eErr;
        }

        /* -------------------------------------------------------------------- */
        /*      Dump vertices.                                                  */
        /* -------------------------------------------------------------------- */
    
        if (wkbFlatten(OGR_G_GetGeometryType(hGeom)) == wkbPoint ||
            wkbFlatten(OGR_G_GetGeometryType(hGeom)) == wkbLineString ||
            wkbFlatten(OGR_G_GetGeometryType(hGeom)) == wkbPolygon)
        {
            if (MMResizeUI64Pointer(&hMMFeature.pNCoord, &hMMFeature.nMaxpNCoord,
                hMMFeature.nNRings + 1, MM_MEAN_NUMBER_OF_RINGS, 0))
            {
                CPLError(CE_Failure, CPLE_FileIO, "MiraMon write failure: %s",
                    VSIStrerror(errno));
                return OGRERR_FAILURE;
            }

            if (MMResizeIntPointer(&hMMFeature.pbArcInfo, &hMMFeature.nMaxpbArcInfo,
                hMMFeature.nNRings + 1, MM_MEAN_NUMBER_OF_RINGS, 0))
            {
                CPLError(CE_Failure, CPLE_FileIO, "MiraMon write failure: %s",
                    VSIStrerror(errno));
                return OGRERR_FAILURE;
            }
            if (bExternalRing)
                hMMFeature.pbArcInfo[hMMFeature.nIRing] = 1;
            else
                hMMFeature.pbArcInfo[hMMFeature.nIRing] = 0;

            hMMFeature.pNCoord[hMMFeature.nIRing] = OGR_G_GetPointCount(hGeom);

            if (MMResizeMM_POINT2DPointer(&hMMFeature.pCoord, &hMMFeature.nMaxpCoord,
                hMMFeature.nICoord + hMMFeature.pNCoord[hMMFeature.nIRing],
                MM_MEAN_NUMBER_OF_COORDS, 0))
            {
                CPLError(CE_Failure, CPLE_FileIO, "MiraMon write failure: %s",
                    VSIStrerror(errno));
                return OGRERR_FAILURE;
            }
            if (hMiraMonLayer.TopHeader.bIs3d)
            {
                if (MMResizeDoublePointer(&hMMFeature.pZCoord, &hMMFeature.nMaxpZCoord,
                    hMMFeature.nICoord + hMMFeature.pNCoord[hMMFeature.nIRing],
                    MM_MEAN_NUMBER_OF_COORDS, 0))
                {
                    CPLError(CE_Failure, CPLE_FileIO, "MiraMon write failure: %s",
                        VSIStrerror(errno));
                    return OGRERR_FAILURE;
                }
            }

            for (int iPoint = 0; iPoint < hMMFeature.pNCoord[hMMFeature.nIRing]; iPoint++)
            {
                hMMFeature.pCoord[hMMFeature.nICoord].dfX = OGR_G_GetX(hGeom, iPoint);
                hMMFeature.pCoord[hMMFeature.nICoord].dfY = OGR_G_GetY(hGeom, iPoint);
                if (hMiraMonLayer.TopHeader.bIs3d && OGR_G_GetCoordinateDimension(hGeom) != 3)
                {
                    CPLError(CE_Failure, CPLE_FileIO, "MiraMon write failure: %s",
                        VSIStrerror(errno));
                    return OGRERR_FAILURE;
                }
                if (hMiraMonLayer.TopHeader.bIs3d)
                {
                    if (OGR_G_GetCoordinateDimension(hGeom) == 2)
                        hMMFeature.pZCoord[hMMFeature.nICoord] = 0;  // Possible rare case
                    else
                        hMMFeature.pZCoord[hMMFeature.nICoord] = OGR_G_GetZ(hGeom, iPoint);
                }
                hMMFeature.nICoord++;
            }
            hMMFeature.nIRing++;
            hMMFeature.nNRings++;
        }
    }
    else
    {
        // Field translation from GDAL to MiraMon
        if(!hMiraMonLayer.pLayerDB)
            TranslateFieldsToMM();

        // All coordinates can be written to the disk
        int result = TranslateFieldsValuesToMM(poFeature);
        if(result!=OGRERR_NONE)
            return result;

        result = AddMMFeature(&hMiraMonLayer, &hMMFeature);
        
        if(result==MM_FATAL_ERROR_WRITING_FEATURES)
        {
            CPLError(CE_Failure, CPLE_FileIO, "MiraMon write failure: %s",
                         VSIStrerror(errno));
            return OGRERR_FAILURE;
        }
        if(result==MM_STOP_WRITING_FEATURES)
        {
            CPLError(CE_Failure, CPLE_FileIO, "MiraMon format limitations.");
            CPLError(CE_Failure, CPLE_FileIO, "Try V2.0 option.");
            return OGRERR_FAILURE;
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                       TranslateFieldsToMM()                          */
/*                                                                      */
/*      Translase ogr Fields to a structure that MiraMon can understand */
/*                                                                      */
/*      Returns OGRERR_NONE/OGRRERR_FAILURE.                            */
/************************************************************************/

OGRErr OGRMiraMonLayer::TranslateFieldsToMM()

{
    if (poFeatureDefn->GetFieldCount() == 0)
        return OGRERR_NONE;

    // If the structure i fill we do anything
    if(hMiraMonLayer.pLayerDB)
        return OGRERR_NONE;

    hMiraMonLayer.pLayerDB=
        static_cast<struct MiraMonDataBase *>(CPLCalloc(
            sizeof(*hMiraMonLayer.pLayerDB), 1));
    if(!hMiraMonLayer.pLayerDB)
        return OGRERR_NOT_ENOUGH_MEMORY;

    hMiraMonLayer.pLayerDB->pFields=
        static_cast<struct MiraMonDataBaseField *>(CPLCalloc(
            poFeatureDefn->GetFieldCount(),
            sizeof(*(hMiraMonLayer.pLayerDB->pFields))));
    if(!hMiraMonLayer.pLayerDB->pFields)
    {
	    CPLFree(hMiraMonLayer.pLayerDB);
        return OGRERR_NOT_ENOUGH_MEMORY;
    }

    hMiraMonLayer.pLayerDB->nNFields=0;
    memset(hMiraMonLayer.pLayerDB->pFields, 0, sizeof(*hMiraMonLayer.pLayerDB->pFields));
    for (int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++)
    {
        switch (poFeatureDefn->GetFieldDefn(iField)->GetType())
        {
            case OFTInteger:
            case OFTIntegerList:
                hMiraMonLayer.pLayerDB->pFields[iField].eFieldType=MM_Numeric;
                hMiraMonLayer.pLayerDB->pFields[iField].nNumberOfDecimals = 0;
                break;

            case OFTInteger64:
            case OFTInteger64List:
                hMiraMonLayer.pLayerDB->pFields[iField].bIs64BitInteger=TRUE;
                hMiraMonLayer.pLayerDB->pFields[iField].eFieldType=MM_Numeric;
                hMiraMonLayer.pLayerDB->pFields[iField].nNumberOfDecimals = 0;
                break;
                
            case OFTReal:
            case OFTRealList:
                hMiraMonLayer.pLayerDB->pFields[iField].eFieldType=MM_Numeric;
                hMiraMonLayer.pLayerDB->pFields[iField].nNumberOfDecimals =
                    poFeatureDefn->GetFieldDefn(iField)->GetPrecision();
                break;

            case OFTBinary:
                 hMiraMonLayer.pLayerDB->pFields[iField].eFieldType = MM_Logic;
                 break;
            case OFTDate:
            case OFTTime:
            case OFTDateTime:
                hMiraMonLayer.pLayerDB->pFields[iField].eFieldType = MM_Data;
                break;

            case OFTString:
            case OFTStringList:
            default:
                hMiraMonLayer.pLayerDB->pFields[iField].eFieldType = MM_Character;
                break;
        }
        if (poFeatureDefn->GetFieldDefn(iField)->GetPrecision() == 0)
        {
            hMiraMonLayer.pLayerDB->pFields[iField].nFieldSize =
                poFeatureDefn->GetFieldDefn(iField)->GetWidth();
        }
        else
        {
            // One more space for the "."
            hMiraMonLayer.pLayerDB->pFields[iField].nFieldSize =
                poFeatureDefn->GetFieldDefn(iField)->GetWidth()+1;
        }

        strncpy(hMiraMonLayer.pLayerDB->pFields[iField].pszFieldName,
            poFeatureDefn->GetFieldDefn(iField)->GetNameRef(),
            MM_MAX_LON_FIELD_NAME_DBF);

        strncpy(hMiraMonLayer.pLayerDB->pFields[iField].pszFieldDescription,
            poFeatureDefn->GetFieldDefn(iField)->GetAlternativeNameRef(),
            MM_MAX_BYTES_FIELD_DESC);

        hMiraMonLayer.pLayerDB->nNFields++;
    }
        
    return OGRERR_NONE;
}

/************************************************************************/
/*                       TranslateFieldsValuesToMM()                    */
/*                                                                      */
/*      Translase ogr Fields to a structure that MiraMon can understand */
/*                                                                      */
/*      Returns OGRERR_NONE/OGRRERR_FAILURE/OGRERR_NOT_ENOUGH_MEMORY    */
/************************************************************************/

OGRErr OGRMiraMonLayer::TranslateFieldsValuesToMM(OGRFeature *poFeature)

{
    if (poFeatureDefn->GetFieldCount() == 0)
        return OGRERR_NONE;

    CPLString osFieldData;
    unsigned __int32 nIRecord;

    for (int iField = 0; iField < poFeatureDefn->GetFieldCount(); iField++)
    {
        OGRFieldType eFType =
            poFeatureDefn->GetFieldDefn(iField)->GetType();
        const char* pszRawValue = poFeature->GetFieldAsString(iField);

        if (eFType == OFTStringList)
        {
            char **panValues =
                poFeature->GetFieldAsStringList(iField);
            hMMFeature.nNumRecords = CSLCount(panValues);
            if(MMResizeMiraMonRecord(&hMMFeature.pRecords, &hMMFeature.nMaxRecords,
                    hMMFeature.nNumRecords, hMMFeature.nNumRecords, 0))
                return OGRERR_NOT_ENOUGH_MEMORY;

            for (nIRecord = 0; nIRecord < hMMFeature.nNumRecords; nIRecord++)
            {
                hMMFeature.pRecords[nIRecord].nNumField=poFeatureDefn->GetFieldCount();

                if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[nIRecord].pField),
                    &hMMFeature.pRecords[nIRecord].nMaxField,
                    hMMFeature.pRecords[nIRecord].nNumField,
                    hMMFeature.pRecords[nIRecord].nNumField, 0))
                        return OGRERR_NOT_ENOUGH_MEMORY;

                if (strlen(panValues[nIRecord]) < MM_MAX_STRING_FIELD_VALUE)
                    strcpy(hMMFeature.pRecords[nIRecord].pField[iField].pStaticValue, panValues[nIRecord]);
                else
                {
                    if (hMMFeature.pRecords[nIRecord].pField[iField].pDinValue)
                    {
                        CPLFree(hMMFeature.pRecords[nIRecord].pField[iField].pDinValue);
                        hMMFeature.pRecords[nIRecord].pField[iField].pDinValue = NULL;
                    }
                    else
                        hMMFeature.pRecords[nIRecord].pField[iField].pDinValue = CPLStrdup(panValues[nIRecord]);
                }
            }
        }
        else if (eFType == OFTIntegerList)
        {
            int nCount = 0;
            const int *panValues =
                poFeature->GetFieldAsIntegerList(iField, &nCount);
            hMMFeature.nNumRecords = nCount;
            if(MMResizeMiraMonRecord(&hMMFeature.pRecords, &hMMFeature.nMaxRecords,
                    hMMFeature.nNumRecords, hMMFeature.nNumRecords, 0))
                return OGRERR_NOT_ENOUGH_MEMORY;

            for (nIRecord = 0; nIRecord < hMMFeature.nNumRecords; nIRecord++)
            {
                hMMFeature.pRecords[nIRecord].nNumField=poFeatureDefn->GetFieldCount();

                if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[nIRecord].pField),
                    &hMMFeature.pRecords[nIRecord].nMaxField,
                    hMMFeature.pRecords[nIRecord].nNumField,
                    hMMFeature.pRecords[nIRecord].nNumField, 0))
                        return OGRERR_NOT_ENOUGH_MEMORY;

                hMMFeature.pRecords[nIRecord].nNumField = iField;
                hMMFeature.pRecords[nIRecord].pField[iField].iValue = panValues[iField];
            }

        }
        else if (eFType == OFTInteger64List)
        {
            int nCount = 0;
            const GIntBig *panValues =
                poFeature->GetFieldAsInteger64List(iField, &nCount);
            hMMFeature.nNumRecords = nCount;
            if(MMResizeMiraMonRecord(&hMMFeature.pRecords, &hMMFeature.nMaxRecords,
                    hMMFeature.nNumRecords, hMMFeature.nNumRecords, 0))
                return OGRERR_NOT_ENOUGH_MEMORY;

            for (nIRecord = 0; nIRecord < hMMFeature.nNumRecords; nIRecord++)
            {
                hMMFeature.pRecords[nIRecord].nNumField=poFeatureDefn->GetFieldCount();

                if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[nIRecord].pField),
                    &hMMFeature.pRecords[nIRecord].nMaxField,
                    hMMFeature.pRecords[nIRecord].nNumField,
                    hMMFeature.pRecords[nIRecord].nNumField, 0))
                        return OGRERR_NOT_ENOUGH_MEMORY;

                hMMFeature.pRecords[nIRecord].nNumField = iField;
                hMMFeature.pRecords[nIRecord].pField[iField].iValue = panValues[iField];
            }
        }
        else if (eFType == OFTRealList)
        {
            int nCount = 0;
            const double *panValues =
                poFeature->GetFieldAsDoubleList(iField, &nCount);
            hMMFeature.nNumRecords = nCount;
            if(MMResizeMiraMonRecord(&hMMFeature.pRecords, &hMMFeature.nMaxRecords,
                    hMMFeature.nNumRecords, hMMFeature.nNumRecords, 0))
                return OGRERR_NOT_ENOUGH_MEMORY;

            for (nIRecord = 0; nIRecord < hMMFeature.nNumRecords; nIRecord++)
            {
                hMMFeature.pRecords[nIRecord].nNumField=poFeatureDefn->GetFieldCount();

                if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[nIRecord].pField),
                    &hMMFeature.pRecords[nIRecord].nMaxField,
                    hMMFeature.pRecords[nIRecord].nNumField,
                    hMMFeature.pRecords[nIRecord].nNumField, 0))
                        return OGRERR_NOT_ENOUGH_MEMORY;

                hMMFeature.pRecords[nIRecord].nNumField = iField;
                hMMFeature.pRecords[nIRecord].pField[iField].dValue = panValues[iField];
            }
        }
        else if (eFType == OFTString)
        {
            hMMFeature.nNumRecords=1;
            hMMFeature.pRecords[0].nNumField=poFeatureDefn->GetFieldCount();
            if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[0].pField),
                    &hMMFeature.pRecords[0].nMaxField,
                    hMMFeature.pRecords[0].nNumField,
                    hMMFeature.pRecords[0].nNumField, 0))
                        return OGRERR_NOT_ENOUGH_MEMORY;

            if (strlen(pszRawValue) < MM_MAX_STRING_FIELD_VALUE)
                strcpy(hMMFeature.pRecords[0].pField[iField].pStaticValue, pszRawValue);
            else
            {
                if (hMMFeature.pRecords[0].pField[iField].pDinValue)
                {
                    CPLFree(hMMFeature.pRecords[0].pField[iField].pDinValue);
                    hMMFeature.pRecords[0].pField[iField].pDinValue = NULL;
                }
                else
                    hMMFeature.pRecords[0].pField[iField].pDinValue = CPLStrdup(pszRawValue);
            }
        }
        else if (eFType == OFTDate)
        {
            hMMFeature.nNumRecords=1;
            hMMFeature.pRecords[0].nNumField=poFeatureDefn->GetFieldCount();
             if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[0].pField),
                    &hMMFeature.pRecords[0].nMaxField,
                    hMMFeature.pRecords[0].nNumField,
                    hMMFeature.pRecords[0].nNumField, 0))
                        return OGRERR_NOT_ENOUGH_MEMORY;


            const OGRField *poField = poFeature->GetRawFieldRef(iField);
            sprintf(hMMFeature.pRecords[0].pField[iField].pStaticValue,
                        "%04d%02d%02d", poField->Date.Year,
                        poField->Date.Month, poField->Date.Day);
        }
        else if (eFType == OFTInteger)
        {
            hMMFeature.nNumRecords=1;
            hMMFeature.pRecords[0].nNumField=poFeatureDefn->GetFieldCount();
             if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[0].pField),
                    &hMMFeature.pRecords[0].nMaxField,
                    hMMFeature.pRecords[0].nNumField,
                    hMMFeature.pRecords[0].nNumField, 0))
                        return OGRERR_NOT_ENOUGH_MEMORY;

            hMMFeature.pRecords[0].pField[iField].dValue =
                poFeature->GetFieldAsInteger(iField);
        }
        else if (eFType == OFTInteger64)
        {
            hMMFeature.nNumRecords=1;
            hMMFeature.pRecords[0].nNumField=poFeatureDefn->GetFieldCount();
             if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[0].pField),
                    &hMMFeature.pRecords[0].nMaxField,
                    hMMFeature.pRecords[0].nNumField,
                    hMMFeature.pRecords[0].nNumField, 0))
                        return OGRERR_NOT_ENOUGH_MEMORY;

            hMMFeature.pRecords[0].pField[iField].iValue =
                poFeature->GetFieldAsInteger64(iField);
        }
        else if (eFType == OFTReal)
        {
            hMMFeature.nNumRecords=1;
            hMMFeature.pRecords[0].nNumField=poFeatureDefn->GetFieldCount();
             if (MMResizeMiraMonFieldValue(&(hMMFeature.pRecords[0].pField),
                    &hMMFeature.pRecords[0].nMaxField,
                    hMMFeature.pRecords[0].nNumField,
                    hMMFeature.pRecords[0].nNumField, 0))
                        return OGRERR_NOT_ENOUGH_MEMORY;

            hMMFeature.pRecords[0].pField[iField].dValue =
                poFeature->GetFieldAsDouble(iField);
        }
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                             GetExtent()                              */
/*                                                                      */
/*      Fetch extent of the data currently stored in the dataset.       */
/*      The bForce flag has no effect on SHO files since that value     */
/*      is always in the header.                                        */
/*                                                                      */
/*      Returns OGRERR_NONE/OGRRERR_FAILURE.                            */
/************************************************************************/

OGRErr OGRMiraMonLayer::GetExtent(OGREnvelope *psExtent, int bForce)

{
    if (bRegionComplete && sRegion.IsInit())
    {
        *psExtent = sRegion;
        return OGRERR_NONE;
    }

    return OGRLayer::GetExtent(psExtent, bForce);
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRMiraMonLayer::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, OLCRandomRead))
        return FALSE;

    if (EQUAL(pszCap, OLCSequentialWrite))
        return TRUE;

    if (EQUAL(pszCap, OLCFastSpatialFilter))
        return FALSE;

    if (EQUAL(pszCap, OLCFastGetExtent))
        return bRegionComplete;

    if (EQUAL(pszCap, OLCCreateField))
        return TRUE;

    if (EQUAL(pszCap, OLCZGeometries))
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRMiraMonLayer::CreateField(OGRFieldDefn *poField, int bApproxOK)

{
    if (!bUpdate)
    {
        CPLError(CE_Failure, CPLE_NoWriteAccess,
                 "Cannot create fields on read-only dataset.");
        return OGRERR_FAILURE;
    }

    if (bHeaderComplete)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to create fields after features have been created.");
        return OGRERR_FAILURE;
    }

    switch (poField->GetType())
    {
        case OFTInteger:
        case OFTReal:
        case OFTString:
        case OFTDateTime:
            poFeatureDefn->AddFieldDefn(poField);
            return OGRERR_NONE;
            break;
        default:
            if (!bApproxOK)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Field %s is of unsupported type %s.",
                         poField->GetNameRef(),
                         poField->GetFieldTypeName(poField->GetType()));
                return OGRERR_FAILURE;
            }
            else if (poField->GetType() == OFTDate ||
                     poField->GetType() == OFTTime)
            {
                OGRFieldDefn oModDef(poField);
                oModDef.SetType(OFTDateTime);
                poFeatureDefn->AddFieldDefn(poField);
                return OGRERR_NONE;
            }
            else
            {
                OGRFieldDefn oModDef(poField);
                oModDef.SetType(OFTString);
                poFeatureDefn->AddFieldDefn(poField);
                return OGRERR_NONE;
            }
    }
}

