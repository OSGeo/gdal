/******************************************************************************
 *
 * Project:  OGR
 * Purpose:  Implements OGRNASDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_nas.h"

static const char *const apszURNNames[] = {
    "DE_DHDN_3GK2_*", "EPSG:31466", "DE_DHDN_3GK3_*", "EPSG:31467",
    "ETRS89_UTM32",   "EPSG:25832", "ETRS89_UTM33",   "EPSG:25833",
    nullptr,          nullptr};

/************************************************************************/
/*                         OGRNASDataSource()                           */
/************************************************************************/

OGRNASDataSource::OGRNASDataSource()
    : papoLayers(nullptr), nLayers(0), poReader(nullptr)
{
}

/************************************************************************/
/*                        ~OGRNASDataSource()                         */
/************************************************************************/

OGRNASDataSource::~OGRNASDataSource()

{
    for (int i = 0; i < nLayers; i++)
        delete papoLayers[i];

    CPLFree(papoLayers);

    if (poReader)
        delete poReader;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRNASDataSource::Open(const char *pszNewName)

{
    poReader = CreateNASReader();
    if (poReader == nullptr)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "File %s appears to be NAS but the NAS reader cannot\n"
                 "be instantiated, likely because Xerces support was not\n"
                 "configured in.",
                 pszNewName);
        return FALSE;
    }

    poReader->SetSourceFile(pszNewName);

    bool bHaveSchema = false;
    bool bHaveTemplate = false;

    // Is some NAS Feature Schema (.gfs) TEMPLATE required?
    const char *pszNASTemplateName = CPLGetConfigOption("NAS_GFS_TEMPLATE", "");
    if (!EQUAL(pszNASTemplateName, ""))
    {
        // Load the TEMPLATE.
        if (!poReader->LoadClasses(pszNASTemplateName))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "NAS schema %s could not be loaded\n", pszNASTemplateName);
            return FALSE;
        }

        bHaveTemplate = true;

        CPLDebug("NAS", "Schema loaded.");
    }
    else
    {
        /* --------------------------------------------------------------------
         */
        /*      Can we find a NAS Feature Schema (.gfs) for the input file? */
        /* --------------------------------------------------------------------
         */
        const std::string osGFSFilename =
            CPLResetExtensionSafe(pszNewName, "gfs");
        VSIStatBufL sGFSStatBuf;
        if (VSIStatL(osGFSFilename.c_str(), &sGFSStatBuf) == 0)
        {
            VSIStatBufL sNASStatBuf;
            if (VSIStatL(pszNewName, &sNASStatBuf) == 0 &&
                sNASStatBuf.st_mtime > sGFSStatBuf.st_mtime)
            {
                CPLDebug("NAS",
                         "Found %s but ignoring because it appears "
                         "be older than the associated NAS file.",
                         osGFSFilename.c_str());
            }
            else
            {
                bHaveSchema = poReader->LoadClasses(osGFSFilename.c_str());
            }
        }

        if (!bHaveSchema)
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "No schema information loaded");
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Force a first pass to establish the schema.  The loaded schema  */
    /*      if any will be cleaned from any unavailable classes.            */
    /* -------------------------------------------------------------------- */
    CPLErrorReset();
    if (!bHaveSchema && !poReader->PrescanForSchema(TRUE) &&
        CPLGetLastErrorType() == CE_Failure)
    {
        // Assume an error has been reported.
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Save the schema file if possible.  Do not make a fuss if we     */
    /*      cannot.  It could be read-only directory or something.          */
    /* -------------------------------------------------------------------- */
    if (!bHaveTemplate && !bHaveSchema && poReader->GetClassCount() > 0 &&
        !STARTS_WITH_CI(pszNewName, "/vsitar/") &&
        !STARTS_WITH_CI(pszNewName, "/vsizip/") &&
        !STARTS_WITH_CI(pszNewName, "/vsigzip/vsi") &&
        !STARTS_WITH_CI(pszNewName, "/vsigzip//vsi") &&
        !STARTS_WITH_CI(pszNewName, "/vsicurl/") &&
        !STARTS_WITH_CI(pszNewName, "/vsicurl_streaming/"))
    {
        VSILFILE *fp = nullptr;

        const std::string osGFSFilename =
            CPLResetExtensionSafe(pszNewName, "gfs");
        VSIStatBufL sGFSStatBuf;
        if (VSIStatL(osGFSFilename.c_str(), &sGFSStatBuf) != 0 &&
            (fp = VSIFOpenL(osGFSFilename.c_str(), "wt")) != nullptr)
        {
            VSIFCloseL(fp);
            poReader->SaveClasses(osGFSFilename.c_str());
        }
        else
        {
            CPLDebug("NAS",
                     "Not saving %s. File already exists or can't be created.",
                     osGFSFilename.c_str());
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Translate the GMLFeatureClasses into layers.                    */
    /* -------------------------------------------------------------------- */
    papoLayers = (OGRLayer **)CPLCalloc(sizeof(OGRNASLayer *),
                                        poReader->GetClassCount() + 1);
    nLayers = 0;

    while (nLayers < poReader->GetClassCount())
    {
        papoLayers[nLayers] = TranslateNASSchema(poReader->GetClass(nLayers));
        nLayers++;
    }

    return TRUE;
}

/************************************************************************/
/*                         TranslateNASSchema()                         */
/************************************************************************/

OGRNASLayer *OGRNASDataSource::TranslateNASSchema(GMLFeatureClass *poClass)

{
    /* -------------------------------------------------------------------- */
    /*      Translate SRS.                                                  */
    /* -------------------------------------------------------------------- */
    const char *pszSRSName = poClass->GetSRSName();
    OGRSpatialReference *poSRS = nullptr;
    if (pszSRSName)
    {
        const char *pszHandle = strrchr(pszSRSName, ':');
        if (pszHandle != nullptr)
        {
            pszHandle += 1;

            poSRS = new OGRSpatialReference();
            poSRS->SetAxisMappingStrategy(OAMS_TRADITIONAL_GIS_ORDER);

            for (int i = 0; apszURNNames[i * 2 + 0] != nullptr; i++)
            {
                const char *pszTarget = apszURNNames[i * 2 + 0];
                const int nTLen = static_cast<int>(strlen(pszTarget));

                // Are we just looking for a prefix match?
                if (pszTarget[nTLen - 1] == '*')
                {
                    if (EQUALN(pszTarget, pszHandle, nTLen - 1))
                        pszSRSName = apszURNNames[i * 2 + 1];
                }
                else
                {
                    if (EQUAL(pszTarget, pszHandle))
                        pszSRSName = apszURNNames[i * 2 + 1];
                }
            }

            if (poSRS->SetFromUserInput(
                    pszSRSName,
                    OGRSpatialReference::
                        SET_FROM_USER_INPUT_LIMITATIONS_get()) != OGRERR_NONE)
            {
                CPLDebug("NAS", "Failed to translate srsName='%s'", pszSRSName);
                delete poSRS;
                poSRS = nullptr;
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Create an empty layer.                                          */
    /* -------------------------------------------------------------------- */
    OGRNASLayer *poLayer = new OGRNASLayer(poClass->GetName(), this);

    /* -------------------------------------------------------------------- */
    /*      Added attributes (properties).                                  */
    /* -------------------------------------------------------------------- */
    for (int iField = 0; iField < poClass->GetPropertyCount(); iField++)
    {
        GMLPropertyDefn *poProperty = poClass->GetProperty(iField);
        OGRFieldType eFType;

        if (poProperty->GetType() == GMLPT_Untyped)
            eFType = OFTString;
        else if (poProperty->GetType() == GMLPT_String)
            eFType = OFTString;
        else if (poProperty->GetType() == GMLPT_Integer)
            eFType = OFTInteger;
        else if (poProperty->GetType() == GMLPT_Real)
            eFType = OFTReal;
        else if (poProperty->GetType() == GMLPT_StringList)
            eFType = OFTStringList;
        else if (poProperty->GetType() == GMLPT_IntegerList)
            eFType = OFTIntegerList;
        else if (poProperty->GetType() == GMLPT_RealList)
            eFType = OFTRealList;
        else
            eFType = OFTString;

        OGRFieldDefn oField(poProperty->GetName(), eFType);
        if (STARTS_WITH_CI(oField.GetNameRef(), "ogr:"))
            oField.SetName(poProperty->GetName() + 4);
        if (poProperty->GetWidth() > 0)
            oField.SetWidth(poProperty->GetWidth());

        poLayer->GetLayerDefn()->AddFieldDefn(&oField);
    }

    for (int iField = 0; iField < poClass->GetGeometryPropertyCount(); iField++)
    {
        GMLGeometryPropertyDefn *poProperty =
            poClass->GetGeometryProperty(iField);
        OGRGeomFieldDefn oField(poProperty->GetName(),
                                (OGRwkbGeometryType)poProperty->GetType());
        if (poClass->GetGeometryPropertyCount() == 1 &&
            poClass->GetFeatureCount() == 0)
        {
            oField.SetType(wkbUnknown);
        }

        oField.SetSpatialRef(poSRS);
        oField.SetNullable(poProperty->IsNullable());
        poLayer->GetLayerDefn()->AddGeomFieldDefn(&oField);
    }

    if (poSRS)
        poSRS->Dereference();

    return poLayer;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRNASDataSource::GetLayer(int iLayer)

{
    if (iLayer < 0 || iLayer >= nLayers)
        return nullptr;

    return papoLayers[iLayer];
}
