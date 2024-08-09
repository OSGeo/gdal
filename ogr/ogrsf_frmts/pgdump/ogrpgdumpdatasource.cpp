/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGDumpDataSource class.
 * Author:   Even Rouault, <even dot rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at spatialys.com>
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

#include <algorithm>
#include <cstring>
#include "ogr_pgdump.h"
#include "cpl_conv.h"
#include "cpl_md5.h"
#include "cpl_string.h"

/************************************************************************/
/*                      OGRPGDumpDataSource()                           */
/************************************************************************/

OGRPGDumpDataSource::OGRPGDumpDataSource(const char *pszNameIn,
                                         char **papszOptions)
{
    SetDescription(pszNameIn);

    const char *pszCRLFFormat = CSLFetchNameValue(papszOptions, "LINEFORMAT");

    bool bUseCRLF = false;
    if (pszCRLFFormat == nullptr)
    {
#ifdef _WIN32
        bUseCRLF = true;
#endif
    }
    else if (EQUAL(pszCRLFFormat, "CRLF"))
    {
        bUseCRLF = true;
    }
    else if (EQUAL(pszCRLFFormat, "LF"))
    {
        bUseCRLF = false;
    }
    else
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "LINEFORMAT=%s not understood, use one of CRLF or LF.",
                 pszCRLFFormat);
#ifdef _WIN32
        bUseCRLF = true;
#endif
    }

    if (bUseCRLF)
        m_pszEOL = "\r\n";

    m_fp = VSIFOpenL(pszNameIn, "wb");
    if (m_fp == nullptr)
    {
        CPLError(CE_Failure, CPLE_FileIO, "Cannot create %s", pszNameIn);
        return;
    }
}

/************************************************************************/
/*                          ~OGRPGDumpDataSource()                          */
/************************************************************************/

OGRPGDumpDataSource::~OGRPGDumpDataSource()

{
    EndCopy();
    m_apoLayers.clear();

    if (m_fp)
    {
        LogCommit();
        VSIFCloseL(m_fp);
        m_fp = nullptr;
    }
}

/************************************************************************/
/*                         LogStartTransaction()                        */
/************************************************************************/

void OGRPGDumpDataSource::LogStartTransaction()
{
    if (m_bInTransaction)
        return;
    m_bInTransaction = true;
    Log("BEGIN");
}

/************************************************************************/
/*                             LogCommit()                              */
/************************************************************************/

void OGRPGDumpDataSource::LogCommit()
{
    EndCopy();

    if (!m_bInTransaction)
        return;
    m_bInTransaction = false;
    Log("COMMIT");
}

/************************************************************************/
/*                         OGRPGCommonLaunderName()                     */
/************************************************************************/

char *OGRPGCommonLaunderName(const char *pszSrcName, const char *pszDebugPrefix,
                             bool bUTF8ToASCII)

{
    char *pszSafeName = bUTF8ToASCII ? CPLUTF8ForceToASCII(pszSrcName, '_')
                                     : CPLStrdup(pszSrcName);

    int i = 0;  // needed after loop
    for (; i < OGR_PG_NAMEDATALEN - 1 && pszSafeName[i] != '\0'; i++)
    {
        if (static_cast<unsigned char>(pszSafeName[i]) <= 127)
        {
            pszSafeName[i] =
                (char)CPLTolower(static_cast<unsigned char>(pszSafeName[i]));
            if (pszSafeName[i] == '\'' || pszSafeName[i] == '-' ||
                pszSafeName[i] == '#')
            {
                pszSafeName[i] = '_';
            }
        }
    }

    if (i == OGR_PG_NAMEDATALEN - 1 && pszSafeName[i] != '\0' &&
        pszSafeName[i + 1] != '\0')
    {
        constexpr int FIRST_8_CHARS_OF_MD5 = 8;
        pszSafeName[i - FIRST_8_CHARS_OF_MD5 - 1] = '_';
        memcpy(pszSafeName + i - FIRST_8_CHARS_OF_MD5, CPLMD5String(pszSrcName),
               FIRST_8_CHARS_OF_MD5);
    }

    pszSafeName[i] = '\0';

    if (strcmp(pszSrcName, pszSafeName) != 0)
    {
        if (CPLStrlenUTF8(pszSafeName) < CPLStrlenUTF8(pszSrcName))
        {
            CPLError(CE_Warning, CPLE_AppDefined,
                     "%s identifier truncated to %s", pszSrcName, pszSafeName);
        }
        else
        {
            CPLDebug(pszDebugPrefix, "LaunderName('%s') -> '%s'", pszSrcName,
                     pszSafeName);
        }
    }

    return pszSafeName;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRPGDumpDataSource::ICreateLayer(const char *pszLayerName,
                                  const OGRGeomFieldDefn *poGeomFieldDefn,
                                  CSLConstList papszOptions)

{
    if (STARTS_WITH(pszLayerName, "pg"))
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "The layer name should not begin by 'pg' as it is a reserved "
                 "prefix");
    }

    auto eType = poGeomFieldDefn ? poGeomFieldDefn->GetType() : wkbNone;
    const auto poSRS =
        poGeomFieldDefn ? poGeomFieldDefn->GetSpatialRef() : nullptr;

    const bool bCreateTable = CPLFetchBool(papszOptions, "CREATE_TABLE", true);
    const bool bCreateSchema =
        CPLFetchBool(papszOptions, "CREATE_SCHEMA", true);
    const char *pszDropTable =
        CSLFetchNameValueDef(papszOptions, "DROP_TABLE", "IF_EXISTS");
    int nGeometryTypeFlags = 0;

    if (OGR_GT_HasZ(eType))
        nGeometryTypeFlags |= OGRGeometry::OGR_G_3D;
    if (OGR_GT_HasM(eType))
        nGeometryTypeFlags |= OGRGeometry::OGR_G_MEASURED;

    int nForcedGeometryTypeFlags = -1;
    const char *pszDim = CSLFetchNameValue(papszOptions, "DIM");
    if (pszDim != nullptr)
    {
        if (EQUAL(pszDim, "XY") || EQUAL(pszDim, "2"))
        {
            nGeometryTypeFlags = 0;
            nForcedGeometryTypeFlags = nGeometryTypeFlags;
        }
        else if (EQUAL(pszDim, "XYZ") || EQUAL(pszDim, "3"))
        {
            nGeometryTypeFlags = OGRGeometry::OGR_G_3D;
            nForcedGeometryTypeFlags = nGeometryTypeFlags;
        }
        else if (EQUAL(pszDim, "XYM"))
        {
            nGeometryTypeFlags = OGRGeometry::OGR_G_MEASURED;
            nForcedGeometryTypeFlags = nGeometryTypeFlags;
        }
        else if (EQUAL(pszDim, "XYZM") || EQUAL(pszDim, "4"))
        {
            nGeometryTypeFlags =
                OGRGeometry::OGR_G_3D | OGRGeometry::OGR_G_MEASURED;
            nForcedGeometryTypeFlags = nGeometryTypeFlags;
        }
        else
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Invalid value for DIM");
        }
    }

    const int nDimension =
        2 + ((nGeometryTypeFlags & OGRGeometry::OGR_G_3D) ? 1 : 0) +
        ((nGeometryTypeFlags & OGRGeometry::OGR_G_MEASURED) ? 1 : 0);

    /* Should we turn layers with None geometry type as Unknown/GEOMETRY */
    /* so they are still recorded in geometry_columns table ? (#4012) */
    const bool bNoneAsUnknown = CPLTestBool(
        CSLFetchNameValueDef(papszOptions, "NONE_AS_UNKNOWN", "NO"));

    if (bNoneAsUnknown && eType == wkbNone)
        eType = wkbUnknown;

    const bool bExtractSchemaFromLayerName = CPLTestBool(CSLFetchNameValueDef(
        papszOptions, "EXTRACT_SCHEMA_FROM_LAYER_NAME", "YES"));

    // Postgres Schema handling:

    // Extract schema name from input layer name or passed with -lco SCHEMA.
    // Set layer name to "schema.table" or to "table" if schema ==
    // current_schema() Usage without schema name is backwards compatible

    const char *pszDotPos = strstr(pszLayerName, ".");
    std::string osTable;
    std::string osSchema;
    const bool bUTF8ToASCII =
        CPLFetchBool(papszOptions, "LAUNDER_ASCII", false);
    const bool bLaunder =
        bUTF8ToASCII || CPLFetchBool(papszOptions, "LAUNDER", true);

    if (pszDotPos != nullptr && bExtractSchemaFromLayerName)
    {
        const size_t length = static_cast<size_t>(pszDotPos - pszLayerName);
        osSchema = pszLayerName;
        osSchema.resize(length);

        if (bLaunder)
        {
            char *pszTmp = OGRPGCommonLaunderName(pszDotPos + 1, "PGDump",
                                                  bUTF8ToASCII);  // skip "."
            osTable = pszTmp;
            CPLFree(pszTmp);
        }
        else
            osTable = OGRPGCommonGenerateShortEnoughIdentifier(pszDotPos +
                                                               1);  // skip "."
    }
    else
    {
        if (bLaunder)
        {
            char *pszTmp =
                OGRPGCommonLaunderName(pszLayerName, "PGDump", bUTF8ToASCII);
            osTable = pszTmp;
            CPLFree(pszTmp);
        }
        else
            osTable = OGRPGCommonGenerateShortEnoughIdentifier(pszLayerName);
    }

    const std::string osTableEscaped =
        OGRPGDumpEscapeColumnName(osTable.c_str());
    const char *pszTableEscaped = osTableEscaped.c_str();

    LogCommit();

    /* -------------------------------------------------------------------- */
    /*      Set the default schema for the layers.                          */
    /* -------------------------------------------------------------------- */
    CPLString osCommand;

    const char *pszSchemaOption = CSLFetchNameValue(papszOptions, "SCHEMA");
    if (pszSchemaOption)
    {
        osSchema = pszSchemaOption;
        if (bCreateSchema)
        {
            osCommand.Printf(
                "CREATE SCHEMA %s",
                OGRPGDumpEscapeColumnName(osSchema.c_str()).c_str());
            Log(osCommand);
        }
    }

    const bool bTemporary = CPLFetchBool(papszOptions, "TEMPORARY", false);
    if (bTemporary)
    {
        osSchema = "pg_temp";
    }

    if (osSchema.empty())
    {
        osSchema = "public";
    }
    const std::string osSchemaEscaped =
        OGRPGDumpEscapeColumnName(osSchema.c_str());
    const char *pszSchemaEscaped = osSchemaEscaped.c_str();

    /* -------------------------------------------------------------------- */
    /*      Do we already have this layer?                                  */
    /* -------------------------------------------------------------------- */
    for (const auto &poLayer : m_apoLayers)
    {
        if (EQUAL(pszLayerName, poLayer->GetDescription()))
        {
            CPLError(CE_Failure, CPLE_AppDefined,
                     "Layer %s already exists, CreateLayer failed.\n",
                     pszLayerName);
            return nullptr;
        }
    }

    if (bCreateTable &&
        (EQUAL(pszDropTable, "YES") || EQUAL(pszDropTable, "ON") ||
         EQUAL(pszDropTable, "TRUE") || EQUAL(pszDropTable, "IF_EXISTS")))
    {
        if (EQUAL(pszDropTable, "IF_EXISTS"))
            osCommand.Printf("DROP TABLE IF EXISTS %s.%s CASCADE",
                             pszSchemaEscaped, pszTableEscaped);
        else
            osCommand.Printf("DROP TABLE %s.%s CASCADE", pszSchemaEscaped,
                             pszTableEscaped);
        Log(osCommand);
    }

    /* -------------------------------------------------------------------- */
    /*      Handle the GEOM_TYPE option.                                    */
    /* -------------------------------------------------------------------- */
    const char *pszGeomType = CSLFetchNameValue(papszOptions, "GEOM_TYPE");
    if (pszGeomType == nullptr)
    {
        pszGeomType = "geometry";
    }

    if (!EQUAL(pszGeomType, "geometry") && !EQUAL(pszGeomType, "geography"))
    {
        CPLError(
            CE_Failure, CPLE_AppDefined,
            "GEOM_TYPE in PostGIS enabled databases must be 'geometry' or "
            "'geography'.  Creation of layer %s with GEOM_TYPE %s has failed.",
            pszLayerName, pszGeomType);
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Try to get the SRS Id of this spatial reference system,         */
    /*      adding tot the srs table if needed.                             */
    /* -------------------------------------------------------------------- */
    const char *pszPostgisVersion =
        CSLFetchNameValueDef(papszOptions, "POSTGIS_VERSION", "2.2");
    const int nPostGISMajor = atoi(pszPostgisVersion);
    const char *pszPostgisVersionDot = strchr(pszPostgisVersion, '.');
    const int nPostGISMinor =
        pszPostgisVersionDot ? atoi(pszPostgisVersionDot + 1) : 0;
    const int nUnknownSRSId = nPostGISMajor >= 2 ? 0 : -1;

    int nSRSId = nUnknownSRSId;
    int nForcedSRSId = -2;
    const char *pszSRID = CSLFetchNameValue(papszOptions, "SRID");
    if (pszSRID)
    {
        nSRSId = atoi(pszSRID);
        nForcedSRSId = nSRSId;
    }
    else
    {
        if (poSRS)
        {
            const char *pszAuthorityName = poSRS->GetAuthorityName(nullptr);
            if (pszAuthorityName != nullptr && EQUAL(pszAuthorityName, "EPSG"))
            {
                /* Assume the EPSG Id is the SRS ID. Might be a wrong guess ! */
                nSRSId = atoi(poSRS->GetAuthorityCode(nullptr));
            }
            else
            {
                const char *pszGeogCSName = poSRS->GetAttrValue("GEOGCS");
                if (pszGeogCSName != nullptr &&
                    EQUAL(pszGeogCSName, "GCS_WGS_1984"))
                {
                    nSRSId = 4326;
                }
            }
        }
    }

    const std::string osEscapedTableNameSingleQuote =
        OGRPGDumpEscapeString(osTable.c_str());
    const char *pszEscapedTableNameSingleQuote =
        osEscapedTableNameSingleQuote.c_str();

    const char *pszGeometryType = OGRToOGCGeomType(eType);

    const char *pszGFldName = CSLFetchNameValue(papszOptions, "GEOMETRY_NAME");
    if (eType != wkbNone && !EQUAL(pszGeomType, "geography"))
    {
        if (pszGFldName == nullptr)
            pszGFldName = "wkb_geometry";

        if (nPostGISMajor < 2)
        {
            // Sometimes there is an old cruft entry in the geometry_columns
            // table if things were not properly cleaned up before.  We make
            // an effort to clean out such cruft.
            //
            // Note: PostGIS 2.0 defines geometry_columns as a view (no clean up
            // is needed).

            osCommand.Printf("DELETE FROM geometry_columns "
                             "WHERE f_table_name = %s AND f_table_schema = %s",
                             pszEscapedTableNameSingleQuote,
                             OGRPGDumpEscapeString(osSchema.c_str()).c_str());
            if (bCreateTable)
                Log(osCommand);
        }
    }

    LogStartTransaction();

    /* -------------------------------------------------------------------- */
    /*      Create an empty table first.                                    */
    /* -------------------------------------------------------------------- */
    if (bCreateTable)
    {
        if (bTemporary)
        {
            osCommand.Printf("CREATE TEMPORARY TABLE %s()", pszTableEscaped);
        }
        else
        {
            osCommand.Printf("CREATE%s TABLE %s.%s()",
                             CPLFetchBool(papszOptions, "UNLOGGED", false)
                                 ? " UNLOGGED"
                                 : "",
                             pszSchemaEscaped, pszTableEscaped);
        }
        Log(osCommand);
    }

    /* -------------------------------------------------------------------- */
    /*      Add FID if needed.                                              */
    /* -------------------------------------------------------------------- */
    const char *pszFIDColumnNameIn = CSLFetchNameValue(papszOptions, "FID");
    CPLString osFIDColumnName;
    if (pszFIDColumnNameIn == nullptr)
        osFIDColumnName = "ogc_fid";
    else
    {
        if (bLaunder)
        {
            char *pszLaunderedFid = OGRPGCommonLaunderName(
                pszFIDColumnNameIn, "PGDump", bUTF8ToASCII);
            osFIDColumnName = pszLaunderedFid;
            CPLFree(pszLaunderedFid);
        }
        else
        {
            osFIDColumnName = pszFIDColumnNameIn;
        }
    }
    const CPLString osFIDColumnNameEscaped =
        OGRPGDumpEscapeColumnName(osFIDColumnName);

    const bool bFID64 = CPLFetchBool(papszOptions, "FID64", false);
    const char *pszSerialType = bFID64 ? "BIGSERIAL" : "SERIAL";

    if (bCreateTable && !osFIDColumnName.empty())
    {
        std::string osConstraintName(osTable);
        if (osConstraintName.size() + strlen("_pk") >
            static_cast<size_t>(OGR_PG_NAMEDATALEN - 1))
        {
            osConstraintName.resize(OGR_PG_NAMEDATALEN - 1 - strlen("_pk"));
        }
        osConstraintName += "_pk";
        osCommand.Printf(
            "ALTER TABLE %s.%s ADD COLUMN %s %s "
            "CONSTRAINT %s PRIMARY KEY",
            pszSchemaEscaped, pszTableEscaped, osFIDColumnNameEscaped.c_str(),
            pszSerialType,
            OGRPGDumpEscapeColumnName(osConstraintName.c_str()).c_str());
        Log(osCommand);
    }

    /* -------------------------------------------------------------------- */
    /*      Create geometry/geography column (actual creation possibly      */
    /*      deferred).                                                      */
    /* -------------------------------------------------------------------- */
    std::vector<std::string> aosGeomCommands;
    if (bCreateTable && eType != wkbNone && EQUAL(pszGeomType, "geography"))
    {
        if (CSLFetchNameValue(papszOptions, "GEOMETRY_NAME") != nullptr)
            pszGFldName = CSLFetchNameValue(papszOptions, "GEOMETRY_NAME");
        else
            pszGFldName = "the_geog";

        const char *suffix = "";
        if ((nGeometryTypeFlags & OGRGeometry::OGR_G_MEASURED) &&
            (nGeometryTypeFlags & OGRGeometry::OGR_G_3D))
        {
            suffix = "ZM";
        }
        else if ((nGeometryTypeFlags & OGRGeometry::OGR_G_MEASURED))
        {
            suffix = "M";
        }
        else if ((nGeometryTypeFlags & OGRGeometry::OGR_G_3D))
        {
            suffix = "Z";
        }

        if (nSRSId)
            osCommand.Printf("ALTER TABLE %s.%s "
                             "ADD COLUMN %s geography(%s%s,%d)",
                             pszSchemaEscaped, pszTableEscaped,
                             OGRPGDumpEscapeColumnName(pszGFldName).c_str(),
                             pszGeometryType, suffix, nSRSId);
        else
            osCommand.Printf("ALTER TABLE %s.%s "
                             "ADD COLUMN %s geography(%s%s)",
                             pszSchemaEscaped, pszTableEscaped,
                             OGRPGDumpEscapeColumnName(pszGFldName).c_str(),
                             pszGeometryType, suffix);
        aosGeomCommands.push_back(osCommand);
    }
    else if (bCreateTable && eType != wkbNone)
    {
        const char *suffix = "";
        if (nGeometryTypeFlags ==
                static_cast<int>(OGRGeometry::OGR_G_MEASURED) &&
            wkbFlatten(eType) != wkbUnknown)
        {
            suffix = "M";
        }

        osCommand.Printf(
            "SELECT AddGeometryColumn(%s,%s,%s,%d,'%s%s',%d)",
            OGRPGDumpEscapeString(bTemporary ? "" : osSchema.c_str()).c_str(),
            pszEscapedTableNameSingleQuote,
            OGRPGDumpEscapeString(pszGFldName).c_str(), nSRSId, pszGeometryType,
            suffix, nDimension);
        aosGeomCommands.push_back(osCommand);
    }

    const char *pszSI =
        CSLFetchNameValueDef(papszOptions, "SPATIAL_INDEX", "GIST");
    const bool bCreateSpatialIndex =
        (EQUAL(pszSI, "GIST") || EQUAL(pszSI, "SPGIST") ||
         EQUAL(pszSI, "BRIN") || EQUAL(pszSI, "YES") || EQUAL(pszSI, "ON") ||
         EQUAL(pszSI, "TRUE"));
    if (!bCreateSpatialIndex && !EQUAL(pszSI, "NO") && !EQUAL(pszSI, "OFF") &&
        !EQUAL(pszSI, "FALSE") && !EQUAL(pszSI, "NONE"))
    {
        CPLError(CE_Warning, CPLE_NotSupported,
                 "SPATIAL_INDEX=%s not supported", pszSI);
    }
    const char *pszSpatialIndexType = EQUAL(pszSI, "SPGIST") ? "SPGIST"
                                      : EQUAL(pszSI, "BRIN") ? "BRIN"
                                                             : "GIST";

    std::vector<std::string> aosSpatialIndexCreationCommands;
    if (bCreateTable && bCreateSpatialIndex && pszGFldName && eType != wkbNone)
    {
        const std::string osIndexName(OGRPGCommonGenerateSpatialIndexName(
            osTable.c_str(), pszGFldName, 0));

        /* --------------------------------------------------------------- */
        /*      Create the spatial index.                                  */
        /* --------------------------------------------------------------- */
        osCommand.Printf("CREATE INDEX %s "
                         "ON %s.%s "
                         "USING %s (%s)",
                         OGRPGDumpEscapeColumnName(osIndexName.c_str()).c_str(),
                         pszSchemaEscaped, pszTableEscaped, pszSpatialIndexType,
                         OGRPGDumpEscapeColumnName(pszGFldName).c_str());
        aosSpatialIndexCreationCommands.push_back(osCommand);
    }

    /* -------------------------------------------------------------------- */
    /*      Create the layer object.                                        */
    /* -------------------------------------------------------------------- */
    const bool bWriteAsHex =
        !CPLFetchBool(papszOptions, "WRITE_EWKT_GEOM", false);

    auto poLayer = std::make_unique<OGRPGDumpLayer>(
        this, osSchema.c_str(), osTable.c_str(),
        !osFIDColumnName.empty() ? osFIDColumnName.c_str() : nullptr,
        bWriteAsHex, bCreateTable);
    poLayer->SetLaunderFlag(bLaunder);
    poLayer->SetUTF8ToASCIIFlag(bUTF8ToASCII);
    poLayer->SetPrecisionFlag(CPLFetchBool(papszOptions, "PRECISION", true));

    const char *pszOverrideColumnTypes =
        CSLFetchNameValue(papszOptions, "COLUMN_TYPES");
    poLayer->SetOverrideColumnTypes(pszOverrideColumnTypes);
    poLayer->SetUnknownSRSId(nUnknownSRSId);
    poLayer->SetForcedSRSId(nForcedSRSId);
    poLayer->SetCreateSpatialIndex(bCreateSpatialIndex, pszSpatialIndexType);
    poLayer->SetPostGISVersion(nPostGISMajor, nPostGISMinor);
    poLayer->SetForcedGeometryTypeFlags(nForcedGeometryTypeFlags);

    // Log geometry field creation immediately or defer it, according to
    // GEOM_COLUMN_POSITION
    const bool bGeomColumnPositionImmediate = EQUAL(
        CSLFetchNameValueDef(papszOptions, "GEOM_COLUMN_POSITION", "IMMEDIATE"),
        "IMMEDIATE");
    poLayer->SetGeomColumnPositionImmediate(bGeomColumnPositionImmediate);
    if (bGeomColumnPositionImmediate)
    {
        for (const auto &osSQL : aosGeomCommands)
            Log(osSQL.c_str());
    }
    else
    {
        poLayer->SetDeferredGeomFieldCreationCommands(aosGeomCommands);
    }
    poLayer->SetSpatialIndexCreationCommands(aosSpatialIndexCreationCommands);

    const char *pszDescription = CSLFetchNameValue(papszOptions, "DESCRIPTION");
    if (pszDescription != nullptr)
        poLayer->SetForcedDescription(pszDescription);

    if (eType != wkbNone)
    {
        OGRGeomFieldDefn oTmp(pszGFldName, eType);
        auto poGeomField = std::make_unique<OGRPGDumpGeomFieldDefn>(&oTmp);
        poGeomField->m_nSRSId = nSRSId;
        poGeomField->m_nGeometryTypeFlags = nGeometryTypeFlags;
        poLayer->GetLayerDefn()->AddGeomFieldDefn(std::move(poGeomField));
    }
    else if (pszGFldName)
        poLayer->SetGeometryFieldName(pszGFldName);

    /* -------------------------------------------------------------------- */
    /*      Add layer to data source layer list.                            */
    /* -------------------------------------------------------------------- */
    m_apoLayers.emplace_back(std::move(poLayer));

    return m_apoLayers.back().get();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRPGDumpDataSource::TestCapability(const char *pszCap)

{
    if (EQUAL(pszCap, ODsCCreateLayer))
        return TRUE;
    else if (EQUAL(pszCap, ODsCCreateGeomFieldAfterCreateLayer))
        return TRUE;
    else if (EQUAL(pszCap, ODsCCurveGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCMeasuredGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCZGeometries))
        return TRUE;
    else if (EQUAL(pszCap, ODsCRandomLayerWrite))
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRPGDumpDataSource::GetLayer(int iLayer)

{
    if (iLayer < 0 || iLayer >= static_cast<int>(m_apoLayers.size()))
        return nullptr;
    else
        return m_apoLayers[iLayer].get();
}

/************************************************************************/
/*                                  Log()                               */
/************************************************************************/

bool OGRPGDumpDataSource::Log(const char *pszStr, bool bAddSemiColumn)
{
    if (m_fp == nullptr)
    {
        return false;
    }

    VSIFWriteL(pszStr, strlen(pszStr), 1, m_fp);
    if (bAddSemiColumn)
    {
        const char chSemiColumn = ';';
        VSIFWriteL(&chSemiColumn, 1, 1, m_fp);
    }
    VSIFWriteL(m_pszEOL, strlen(m_pszEOL), 1, m_fp);
    return true;
}

/************************************************************************/
/*                             StartCopy()                              */
/************************************************************************/
void OGRPGDumpDataSource::StartCopy(OGRPGDumpLayer *poPGLayer)
{
    EndCopy();
    m_poLayerInCopyMode = poPGLayer;
}

/************************************************************************/
/*                              EndCopy()                               */
/************************************************************************/
OGRErr OGRPGDumpDataSource::EndCopy()
{
    if (m_poLayerInCopyMode != nullptr)
    {
        OGRErr result = m_poLayerInCopyMode->EndCopy();
        m_poLayerInCopyMode = nullptr;

        return result;
    }

    return OGRERR_NONE;
}
