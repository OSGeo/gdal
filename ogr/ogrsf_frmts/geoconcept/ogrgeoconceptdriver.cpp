/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRGeoconceptDriver class.
 * Author:   Didier Richard, didier.richard@ign.fr
 * Language: C++
 *
 ******************************************************************************
 * Copyright (c) 2007,  Geoconcept and IGN
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogrgeoconceptdatasource.h"

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

static GDALDataset *OGRGeoconceptDriverOpen(GDALOpenInfo *poOpenInfo)

{
    const char *pszFilename = poOpenInfo->pszFilename;
#ifndef FUZZING_BUILD_MODE_UNSAFE_FOR_PRODUCTION
    /* -------------------------------------------------------------------- */
    /*      We will only consider .gxt and .txt files.                      */
    /* -------------------------------------------------------------------- */
    const char *pszExtension = CPLGetExtension(pszFilename);
    if (!EQUAL(pszExtension, "gxt") && !EQUAL(pszExtension, "txt"))
    {
        return nullptr;
    }
#endif

    auto poDS = new OGRGeoconceptDataSource();

    if (!poDS->Open(pszFilename, true, poOpenInfo->eAccess == GA_Update))
    {
        delete poDS;
        return nullptr;
    }
    return poDS;
}

/************************************************************************/
/*                          CreateDataSource()                          */
/*                                                                      */
/* Options (-dsco) :                                                    */
/*   EXTENSION=GXT|TXT (default GXT)                                    */
/************************************************************************/

static GDALDataset *OGRGeoconceptDriverCreate(const char *pszName,
                                              int /* nXSize */,
                                              int /* nYSize */,
                                              int /* nBandCount */,
                                              GDALDataType, char **papszOptions)

{
    VSIStatBufL sStat;
    /* int bSingleNewFile = FALSE; */

    if (pszName == nullptr || strlen(pszName) == 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Invalid datasource name (null or empty)");
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Is the target a valid existing directory?                       */
    /* -------------------------------------------------------------------- */
    if (VSIStatL(pszName, &sStat) == 0)
    {
        if (!VSI_ISDIR(sStat.st_mode))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "%s is not a valid existing directory.", pszName);
            return nullptr;
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Does it end with the extension .gxt indicating the user likely  */
    /*      wants to create a single file set?                              */
    /* -------------------------------------------------------------------- */
    else if (EQUAL(CPLGetExtension(pszName), "gxt") ||
             EQUAL(CPLGetExtension(pszName), "txt"))
    {
        /* bSingleNewFile = TRUE; */
    }

    /* -------------------------------------------------------------------- */
    /*      Return a new OGRDataSource()                                    */
    /* -------------------------------------------------------------------- */
    OGRGeoconceptDataSource *poDS = new OGRGeoconceptDataSource();
    if (!poDS->Create(pszName, papszOptions))
    {
        delete poDS;
        return nullptr;
    }
    return poDS;
}

/************************************************************************/
/*                      OGRGeoconceptDriverDelete()                     */
/************************************************************************/

static CPLErr OGRGeoconceptDriverDelete(const char *pszDataSource)

{
    VSIStatBufL sStatBuf;
    static const char *const apszExtensions[] = {"gxt", "txt", "gct",
                                                 "gcm", "gcr", nullptr};

    if (VSIStatL(pszDataSource, &sStatBuf) != 0)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "%s does not appear to be a file or directory.",
                 pszDataSource);

        return CE_Failure;
    }

    if (VSI_ISREG(sStatBuf.st_mode) &&
        (EQUAL(CPLGetExtension(pszDataSource), "gxt") ||
         EQUAL(CPLGetExtension(pszDataSource), "txt")))
    {
        for (int iExt = 0; apszExtensions[iExt] != nullptr; iExt++)
        {
            const char *pszFile =
                CPLResetExtension(pszDataSource, apszExtensions[iExt]);
            if (VSIStatL(pszFile, &sStatBuf) == 0)
                VSIUnlink(pszFile);
        }
    }
    else if (VSI_ISDIR(sStatBuf.st_mode))
    {
        char **papszDirEntries = VSIReadDir(pszDataSource);

        for (int iFile = 0;
             papszDirEntries != nullptr && papszDirEntries[iFile] != nullptr;
             iFile++)
        {
            if (CSLFindString(const_cast<char **>(apszExtensions),
                              CPLGetExtension(papszDirEntries[iFile])) != -1)
            {
                VSIUnlink(CPLFormFilename(pszDataSource, papszDirEntries[iFile],
                                          nullptr));
            }
        }

        CSLDestroy(papszDirEntries);

        VSIRmdir(pszDataSource);
    }

    return CE_None;
}

/************************************************************************/
/*                          RegisterOGRGeoconcept()                     */
/************************************************************************/

void RegisterOGRGeoconcept()

{
    if (GDALGetDriverByName("Geoconcept"))
        return;

    GDALDriver *poDriver = new GDALDriver();
    poDriver->SetDescription("Geoconcept");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Geoconcept");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_FIELD, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSIONS, "gxt txt");
    poDriver->SetMetadataItem(GDAL_DCAP_Z_GEOMETRIES, "YES");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "  <Option name='EXTENSION' type='string-select' "
        "description='indicates the "
        "GeoConcept export file extension. TXT was used by earlier releases of "
        "GeoConcept. GXT is currently used.' default='GXT'>"
        "    <Value>GXT</Value>"
        "    <Value>TXT</Value>"
        "  </Option>"
        "  <Option name='CONFIG' type='string' description='path to the GCT "
        "file that "
        "describes the GeoConcept types definitions.'/>"
        "</CreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST,
                              "<LayerCreationOptionList>"
                              "  <Option name='FEATURETYPE' type='string' "
                              "description='TYPE.SUBTYPE : "
                              "defines the feature to be created. The TYPE "
                              "corresponds to one of the Name "
                              "found in the GCT file for a type section. The "
                              "SUBTYPE corresponds to one of "
                              "the Name found in the GCT file for a sub-type "
                              "section within the previous "
                              "type section'/>"
                              "</LayerCreationOptionList>");
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

    poDriver->pfnOpen = OGRGeoconceptDriverOpen;
    poDriver->pfnCreate = OGRGeoconceptDriverCreate;
    poDriver->pfnDelete = OGRGeoconceptDriverDelete;

    GetGDALDriverManager()->RegisterDriver(poDriver);
}
