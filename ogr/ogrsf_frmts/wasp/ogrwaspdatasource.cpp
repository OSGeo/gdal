/******************************************************************************
 *
 * Project:  WAsP Translator
 * Purpose:  Implements OGRWAsPDataSource class
 * Author:   Vincent Mora, vincent dot mora at oslandia dot com
 *
 ******************************************************************************
 * Copyright (c) 2014, Oslandia <info at oslandia dot com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogrwasp.h"
#include "cpl_conv.h"
#include "cpl_string.h"

#include <cassert>
#include <sstream>

/************************************************************************/
/*                          OGRWAsPDataSource()                          */
/************************************************************************/

OGRWAsPDataSource::OGRWAsPDataSource(const char *pszName, VSILFILE *hFileHandle)
    : sFilename(pszName), hFile(hFileHandle)
{
}

/************************************************************************/
/*                         ~OGRWAsPDataSource()                          */
/************************************************************************/

OGRWAsPDataSource::~OGRWAsPDataSource()

{
    oLayer.reset();    /* we write to file int layer dtor */
    VSIFCloseL(hFile); /* nothing smart can be done here in case of error */
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRWAsPDataSource::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, ODsCCreateLayer) && oLayer.get() == nullptr)
        return true;
    else if (EQUAL(pszCap, ODsCZGeometries))
        return true;

    return false;
}

/************************************************************************/
/*                              GetLayerByName()                        */
/************************************************************************/

OGRLayer *OGRWAsPDataSource::GetLayerByName(const char *pszName)

{
    return (oLayer.get() && EQUAL(pszName, oLayer->GetName())) ? oLayer.get()
                                                               : nullptr;
}

/************************************************************************/
/*                              Load()                                  */
/************************************************************************/

OGRErr OGRWAsPDataSource::Load(bool bSilent)

{
    /* if we don't have a layer, we read from file */
    if (oLayer.get())
    {
        if (!bSilent)
            CPLError(CE_Failure, CPLE_NotSupported, "layer already loaded");
        return OGRERR_FAILURE;
    }
    /* Parse the first line of the file in case it is a spatial ref*/
    const char *pszLine = CPLReadLine2L(hFile, 1024, nullptr);
    if (!pszLine)
    {
        if (!bSilent)
            CPLError(CE_Failure, CPLE_FileIO, "empty file");
        return OGRERR_FAILURE;
    }
    CPLString sLine(pszLine);
    sLine = sLine.substr(0, sLine.find("|"));
    OGRSpatialReference *poSpatialRef = new OGRSpatialReference;
    poSpatialRef->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    if (poSpatialRef->importFromProj4(sLine.c_str()) != OGRERR_NONE)
    {
        if (!bSilent)
            CPLError(CE_Warning, CPLE_FileIO, "cannot find spatial reference");
        delete poSpatialRef;
        poSpatialRef = nullptr;
    }

    /* TODO Parse those line since they define a coordinate transformation */
    CPLReadLineL(hFile);
    CPLReadLineL(hFile);
    CPLReadLineL(hFile);

    oLayer.reset(new OGRWAsPLayer(this, CPLGetBasename(sFilename.c_str()),
                                  hFile, poSpatialRef));
    if (poSpatialRef)
        poSpatialRef->Release();

    const vsi_l_offset iOffset = VSIFTellL(hFile);
    pszLine = CPLReadLineL(hFile);
    if (!pszLine)
    {
        if (!bSilent)
            CPLError(CE_Failure, CPLE_FileIO, "no feature in file");
        oLayer.reset();
        return OGRERR_FAILURE;
    }

    double dfValues[4] = {0};
    int iNumValues = 0;
    {
        std::istringstream iss(pszLine);
        while (iNumValues < 4 && (iss >> dfValues[iNumValues]))
        {
            ++iNumValues;
        }

        if (iNumValues < 2)
        {
            if (!bSilent && iNumValues)
                CPLError(CE_Failure, CPLE_FileIO, "no enough values");
            else if (!bSilent)
                CPLError(CE_Failure, CPLE_FileIO, "no feature in file");

            oLayer.reset();
            return OGRERR_FAILURE;
        }
    }

    if (iNumValues == 3 || iNumValues == 4)
    {
        OGRFieldDefn left("z_left", OFTReal);
        OGRFieldDefn right("z_right", OFTReal);
        oLayer->CreateField(&left);
        oLayer->CreateField(&right);
    }
    if (iNumValues == 2 || iNumValues == 4)
    {
        OGRFieldDefn height("elevation", OFTReal);
        oLayer->CreateField(&height);
    }

    VSIFSeekL(hFile, iOffset, SEEK_SET);
    return OGRERR_NONE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRWAsPDataSource::GetLayer(int iLayer)

{
    return (iLayer == 0) ? oLayer.get() : nullptr;
}

/************************************************************************/
/*                             ICreateLayer()                           */
/************************************************************************/

OGRLayer *
OGRWAsPDataSource::ICreateLayer(const char *pszName,
                                const OGRGeomFieldDefn *poGeomFieldDefn,
                                CSLConstList papszOptions)

{
    const auto eGType = poGeomFieldDefn ? poGeomFieldDefn->GetType() : wkbNone;
    const auto poSpatialRef =
        poGeomFieldDefn ? poGeomFieldDefn->GetSpatialRef() : nullptr;

    if (eGType != wkbLineString && eGType != wkbLineString25D &&
        eGType != wkbMultiLineString && eGType != wkbMultiLineString25D &&
        eGType != wkbPolygon && eGType != wkbPolygon25D &&
        eGType != wkbMultiPolygon && eGType != wkbMultiPolygon25D)
    {
        CPLError(CE_Failure, CPLE_NotSupported, "unsupported geometry type %s",
                 OGRGeometryTypeToName(eGType));
        return nullptr;
    }

    if (!OGRGeometryFactory::haveGEOS() &&
        (eGType == wkbPolygon || eGType == wkbPolygon25D ||
         eGType == wkbMultiPolygon || eGType == wkbMultiPolygon25D))
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "unsupported geometry type %s without GEOS support",
                 OGRGeometryTypeToName(eGType));
        return nullptr;
    }

    if (oLayer.get())
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "this data source does not support more than one layer");
        return nullptr;
    }

    CPLString sFirstField, sSecondField, sGeomField;

    const char *pszFields = CSLFetchNameValue(papszOptions, "WASP_FIELDS");
    const CPLString sFields(pszFields ? pszFields : "");
    if (!sFields.empty())
    {
        /* parse the comma separated list of fields */
        const size_t iComma = sFields.find(',');
        if (std::string::npos != iComma)
        {
            sFirstField = sFields.substr(0, iComma);
            sSecondField = sFields.substr(iComma + 1);
        }
        else
        {
            sFirstField = sFields;
        }
    }

    const char *pszGeomField =
        CSLFetchNameValue(papszOptions, "WASP_GEOM_FIELD");
    sGeomField = CPLString(pszGeomField ? pszGeomField : "");

    const bool bMerge =
        CPLTestBool(CSLFetchNameValueDef(papszOptions, "WASP_MERGE", "YES"));

    std::unique_ptr<double> pdfTolerance;
    {
        const char *pszToler =
            CSLFetchNameValue(papszOptions, "WASP_TOLERANCE");

        if (pszToler)
        {
            if (!OGRGeometryFactory::haveGEOS())
            {
                CPLError(
                    CE_Warning, CPLE_IllegalArg,
                    "GEOS support not enabled, ignoring option WASP_TOLERANCE");
            }
            else
            {
                pdfTolerance.reset(new double);
                if (!(std::istringstream(pszToler) >> *pdfTolerance))
                {
                    CPLError(CE_Failure, CPLE_IllegalArg,
                             "cannot set tolerance from %s", pszToler);
                    return nullptr;
                }
            }
        }
    }

    std::unique_ptr<double> pdfAdjacentPointTolerance;
    {
        const char *pszAdjToler =
            CSLFetchNameValue(papszOptions, "WASP_ADJ_TOLER");
        if (pszAdjToler)
        {
            pdfAdjacentPointTolerance.reset(new double);
            if (!(std::istringstream(pszAdjToler) >>
                  *pdfAdjacentPointTolerance))
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "cannot set tolerance from %s", pszAdjToler);
                return nullptr;
            }
        }
    }

    std::unique_ptr<double> pdfPointToCircleRadius;
    {
        const char *pszPtToCircRad =
            CSLFetchNameValue(papszOptions, "WASP_POINT_TO_CIRCLE_RADIUS");
        if (pszPtToCircRad)
        {
            pdfPointToCircleRadius.reset(new double);
            if (!(std::istringstream(pszPtToCircRad) >>
                  *pdfPointToCircleRadius))
            {
                CPLError(CE_Failure, CPLE_IllegalArg,
                         "cannot set tolerance from %s", pszPtToCircRad);
                return nullptr;
            }
        }
    }

    OGRSpatialReference *poSRSClone = nullptr;
    if (poSpatialRef)
    {
        poSRSClone = poSpatialRef->Clone();
        poSRSClone->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);
    }
    oLayer.reset(new OGRWAsPLayer(
        this, CPLGetBasename(pszName), hFile, poSRSClone, sFirstField,
        sSecondField, sGeomField, bMerge, pdfTolerance.release(),
        pdfAdjacentPointTolerance.release(), pdfPointToCircleRadius.release()));
    if (poSRSClone)
        poSRSClone->Release();

    char *ppszWktSpatialRef = nullptr;
    if (poSpatialRef &&
        poSpatialRef->exportToProj4(&ppszWktSpatialRef) == OGRERR_NONE)
    {
        VSIFPrintfL(hFile, "%s\n", ppszWktSpatialRef);
    }
    else
    {
        VSIFPrintfL(hFile, "no spatial ref sys\n");
    }
    CPLFree(ppszWktSpatialRef);

    VSIFPrintfL(hFile, "  0.0 0.0 0.0 0.0\n");
    VSIFPrintfL(hFile, "  1.0 0.0 1.0 0.0\n");
    VSIFPrintfL(hFile, "  1.0 0.0\n");
    return oLayer.get();
}
