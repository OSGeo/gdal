/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRODBCDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#include "ogr_odbc.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogrodbcdrivercore.h"

/************************************************************************/
/*                         OGRODBCDataSource()                          */
/************************************************************************/

OGRODBCDataSource::OGRODBCDataSource() : papoLayers(nullptr), nLayers(0)
{
}

/************************************************************************/
/*                         ~OGRODBCDataSource()                         */
/************************************************************************/

OGRODBCDataSource::~OGRODBCDataSource()

{
    for (int i = 0; i < nLayers; i++)
        delete papoLayers[i];

    CPLFree(papoLayers);
}

/************************************************************************/
/*                  CheckDSNStringTemplate()                            */
/* The string will be used as the formatting argument of sprintf with   */
/* a string in vararg. So let's check there's only one '%s', and nothing*/
/* else                                                                 */
/************************************************************************/

static int CheckDSNStringTemplate(const char *pszStr)
{
    int nPercentSFound = FALSE;
    while (*pszStr)
    {
        if (*pszStr == '%')
        {
            if (pszStr[1] != 's')
            {
                return FALSE;
            }
            else
            {
                if (nPercentSFound)
                    return FALSE;
                nPercentSFound = TRUE;
            }
        }
        pszStr++;
    }
    return TRUE;
}

/************************************************************************/
/*                              OpenMDB()                               */
/************************************************************************/

int OGRODBCDataSource::OpenMDB(GDALOpenInfo *poOpenInfo)
{
#ifndef _WIN32
    // Try to register MDB Tools driver
    CPLODBCDriverInstaller::InstallMdbToolsDriver();
#endif /* ndef WIN32 */

    const char *pszOptionName = "PGEO_DRIVER_TEMPLATE";
    const char *pszDSNStringTemplate =
        CPLGetConfigOption(pszOptionName, nullptr);
    if (pszDSNStringTemplate == nullptr)
    {
        pszOptionName = "MDB_DRIVER_TEMPLATE";
        pszDSNStringTemplate = CPLGetConfigOption(pszOptionName, nullptr);
        if (pszDSNStringTemplate == nullptr)
        {
            pszOptionName = "";
        }
    }
    if (pszDSNStringTemplate && !CheckDSNStringTemplate(pszDSNStringTemplate))
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Illegal value for %s option",
                 pszOptionName);
        return FALSE;
    }

    const char *pszNewName = poOpenInfo->pszFilename;
    if (!oSession.ConnectToMsAccess(pszNewName, pszDSNStringTemplate))
    {
        return FALSE;
    }

    // Retrieve numeric values from MS Access files using ODBC numeric types, to
    // avoid loss of precision and missing values on Windows (see
    // https://github.com/OSGeo/gdal/issues/3885)
    m_nStatementFlags |= CPLODBCStatement::Flag::RetrieveNumericColumnsAsDouble;

    // Collate a list of all tables in the data source
    CPLODBCStatement oTableList(&oSession);
    std::vector<CPLString> aosTableNames;
    if (oTableList.GetTables())
    {
        while (oTableList.Fetch())
        {
            const char *pszSchema = oTableList.GetColData(1);
            const char *pszTableName = oTableList.GetColData(2);
            if (pszTableName != nullptr)
            {
                CPLString osLayerName;

                if (pszSchema != nullptr && strlen(pszSchema) > 0)
                {
                    osLayerName = pszSchema;
                    osLayerName += ".";
                }

                osLayerName += pszTableName;

                const CPLString osLCTableName(CPLString(osLayerName).tolower());
                m_aosAllLCTableNames.insert(osLCTableName);

                aosTableNames.emplace_back(osLayerName);
            }
        }
    }
    else
    {
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Check if it is a PGeo MDB.                    */
    /* -------------------------------------------------------------------- */
    for (const CPLString &osTableName : aosTableNames)
    {
        const CPLString osLCTableName(CPLString(osTableName).tolower());
        if (osLCTableName == "gdb_geomcolumns" /* PGeo */)
            return FALSE;
    }

    const bool bListAllTables = CPLTestBool(CSLFetchNameValueDef(
        poOpenInfo->papszOpenOptions, "LIST_ALL_TABLES", "NO"));

    /* -------------------------------------------------------------------- */
    /*      Return all tables as non-spatial tables.                       */
    /* -------------------------------------------------------------------- */
    for (const CPLString &osTableName : aosTableNames)
    {
        const CPLString osLCTableName(CPLString(osTableName).tolower());
        if (bListAllTables || !(osLCTableName.size() >= 4 &&
                                osLCTableName.substr(0, 4) ==
                                    "msys")  // MS Access internal tables
        )
        {
            OpenTable(osTableName, nullptr);
        }
    }

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRODBCDataSource::Open(GDALOpenInfo *poOpenInfo)
{
    CPLAssert(nLayers == 0);

    const char *pszNewName = poOpenInfo->pszFilename;

    constexpr const char *ODBC_PREFIX = "ODBC:";
    if (!STARTS_WITH_CI(pszNewName, ODBC_PREFIX) &&
        OGRODBCDriverIsSupportedMsAccessFileExtension(
            CPLGetExtension(pszNewName)))
        return OpenMDB(poOpenInfo);

    /* -------------------------------------------------------------------- */
    /*      Start parsing dataset name from the end of string, fetching     */
    /*      the name of spatial reference table and names for SRID and      */
    /*      SRTEXT columns first.                                           */
    /* -------------------------------------------------------------------- */
    char *pszWrkName = CPLStrdup(pszNewName + strlen(ODBC_PREFIX));
    char **papszTables = nullptr;
    char **papszGeomCol = nullptr;
    char *pszSRSTableName = nullptr;
    char *pszSRIDCol = nullptr;
    char *pszSRTextCol = nullptr;
    char *pszDelimiter = nullptr;

    if ((pszDelimiter = strrchr(pszWrkName, ':')) != nullptr)
    {
        char *pszOBracket = strchr(pszDelimiter + 1, '(');

        if (strchr(pszDelimiter, '\\') != nullptr ||
            strchr(pszDelimiter, '/') != nullptr)
        {
            /*
            ** if there are special tokens then this isn't really
            ** the srs table name, so avoid further processing.
            */
        }
        else if (pszOBracket == nullptr)
        {
            pszSRSTableName = CPLStrdup(pszDelimiter + 1);
            *pszDelimiter = '\0';
        }
        else
        {
            char *pszCBracket = strchr(pszOBracket, ')');
            if (pszCBracket != nullptr)
                *pszCBracket = '\0';

            char *pszComma = strchr(pszOBracket, ',');
            if (pszComma != nullptr)
            {
                *pszComma = '\0';
                pszSRIDCol = CPLStrdup(pszComma + 1);
            }

            *pszOBracket = '\0';
            pszSRSTableName = CPLStrdup(pszDelimiter + 1);
            pszSRTextCol = CPLStrdup(pszOBracket + 1);

            *pszDelimiter = '\0';
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Strip off any comma delimited set of tables names to access     */
    /*      from the end of the string first.  Also allow an optional       */
    /*      bracketed geometry column name after the table name.            */
    /* -------------------------------------------------------------------- */
    while ((pszDelimiter = strrchr(pszWrkName, ',')) != nullptr)
    {
        char *pszOBracket = strstr(pszDelimiter + 1, "(");
        if (pszOBracket == nullptr)
        {
            papszTables = CSLAddString(papszTables, pszDelimiter + 1);
            papszGeomCol = CSLAddString(papszGeomCol, "");
        }
        else
        {
            char *pszCBracket = strstr(pszOBracket, ")");

            if (pszCBracket != nullptr)
                *pszCBracket = '\0';

            *pszOBracket = '\0';
            papszTables = CSLAddString(papszTables, pszDelimiter + 1);
            papszGeomCol = CSLAddString(papszGeomCol, pszOBracket + 1);
        }
        *pszDelimiter = '\0';
    }

    /* -------------------------------------------------------------------- */
    /*      Split out userid, password and DSN.  The general form is        */
    /*      user/password@dsn.  But if there are no @ characters the        */
    /*      whole thing is assumed to be a DSN.                             */
    /* -------------------------------------------------------------------- */
    std::string osUserId;
    std::string osPassword;
    std::string osDSN;

    const char *pszAt = strchr(pszWrkName, '@');
    if (pszAt == nullptr)
    {
        osDSN = pszWrkName;
    }
    else
    {
        osDSN = pszAt + 1;
        osUserId.assign(pszWrkName, pszAt - pszWrkName);
        const auto nSlashPos = osUserId.find('/');
        if (nSlashPos != std::string::npos)
        {
            osPassword = osUserId.substr(nSlashPos + 1);
            osUserId.resize(nSlashPos);
        }
    }

    CPLFree(pszWrkName);

    /* -------------------------------------------------------------------- */
    /*      Initialize based on the DSN.                                    */
    /* -------------------------------------------------------------------- */
    CPLDebug("OGR_ODBC",
             "EstablishSession(DSN:\"%s\", userid:\"%s\", password:\"%s\")",
             osDSN.c_str(), osUserId.c_str(), osPassword.c_str());

    if (!oSession.EstablishSession(
            osDSN.c_str(), osUserId.empty() ? nullptr : osUserId.c_str(),
            osPassword.empty() ? nullptr : osPassword.c_str()))
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Unable to initialize ODBC connection to DSN for %s,\n"
                 "%s",
                 pszNewName + strlen(ODBC_PREFIX), oSession.GetLastError());
        CSLDestroy(papszTables);
        CSLDestroy(papszGeomCol);
        CPLFree(pszSRIDCol);
        CPLFree(pszSRTextCol);
        CPLFree(pszSRSTableName);
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      If no explicit list of tables was given, check for a list in    */
    /*      a geometry_columns table.                                       */
    /* -------------------------------------------------------------------- */
    if (papszTables == nullptr)
    {
        CPLODBCStatement oStmt(&oSession);

        oStmt.Append("SELECT f_table_name, f_geometry_column, geometry_type"
                     " FROM geometry_columns");
        if (oStmt.ExecuteSQL())
        {
            while (oStmt.Fetch())
            {
                papszTables = CSLAddString(papszTables, oStmt.GetColData(0));
                papszGeomCol = CSLAddString(papszGeomCol, oStmt.GetColData(1));
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      Otherwise our final resort is to return all tables as           */
    /*      non-spatial tables.                                             */
    /* -------------------------------------------------------------------- */
    if (papszTables == nullptr)
    {
        CPLODBCStatement oTableList(&oSession);

        if (oTableList.GetTables())
        {
            while (oTableList.Fetch())
            {
                const char *pszSchema = oTableList.GetColData(1);
                CPLString osLayerName;

                if (pszSchema != nullptr && strlen(pszSchema) > 0)
                {
                    osLayerName = pszSchema;
                    osLayerName += ".";
                }

                osLayerName += oTableList.GetColData(2);

                papszTables = CSLAddString(papszTables, osLayerName);

                papszGeomCol = CSLAddString(papszGeomCol, "");
            }
        }
    }

    /* -------------------------------------------------------------------- */
    /*      If we have an explicit list of requested tables, use them       */
    /*      (non-spatial).                                                  */
    /* -------------------------------------------------------------------- */
    for (int iTable = 0;
         papszTables != nullptr && papszTables[iTable] != nullptr; iTable++)
    {
        if (strlen(papszGeomCol[iTable]) > 0)
            OpenTable(papszTables[iTable], papszGeomCol[iTable]);
        else
            OpenTable(papszTables[iTable], nullptr);
    }

    CSLDestroy(papszTables);
    CSLDestroy(papszGeomCol);

#if 0
    // NOTE: nothing uses the SRS cache currently. Hence disabled.

    /* -------------------------------------------------------------------- */
    /*      If no explicit list of tables was given, check for a list in    */
    /*      a geometry_columns table.                                       */
    /* -------------------------------------------------------------------- */
    if (pszSRSTableName)
    {
        CPLODBCStatement oSRSList(&oSession);

        if (!pszSRTextCol)
            pszSRTextCol = CPLStrdup("srtext");
        if (!pszSRIDCol)
            pszSRIDCol = CPLStrdup("srid");

        oSRSList.Append("SELECT ");
        oSRSList.Append(pszSRIDCol);
        oSRSList.Append(",");
        oSRSList.Append(pszSRTextCol);
        oSRSList.Append(" FROM ");
        oSRSList.Append(pszSRSTableName);

        CPLDebug("OGR_ODBC", "ExecuteSQL(%s) to read SRS table",
                 oSRSList.GetCommand());
        if (oSRSList.ExecuteSQL())
        {
            while (oSRSList.Fetch())
            {
                const char *pszSRID = oSRSList.GetColData(pszSRIDCol);
                if (!pszSRID)
                    continue;

                const char *pszSRText = oSRSList.GetColData(pszSRTextCol);

                if (pszSRText)
                {
                    std::unique_ptr<OGRSpatialReference, OGRSpatialReferenceReleaser> poSRS(new OGRSpatialReference());
                    poSRS->SetAxisMappingStrategy(
                        OAMS_TRADITIONAL_GIS_ORDER);
                    if (poSRS->importFromWkt(pszSRText) == OGRERR_NONE )
                    {
                        m_oSRSCache[atoi(pszSRID)] = std::move(poSRS);
                    }
                }
            }
        }
    }
#endif

    if (pszSRIDCol)
        CPLFree(pszSRIDCol);
    if (pszSRTextCol)
        CPLFree(pszSRTextCol);
    if (pszSRSTableName)
        CPLFree(pszSRSTableName);

    return TRUE;
}

/************************************************************************/
/*                             OpenTable()                              */
/************************************************************************/

int OGRODBCDataSource::OpenTable(const char *pszNewName, const char *pszGeomCol)
{
    /* -------------------------------------------------------------------- */
    /*      Create the layer object.                                        */
    /* -------------------------------------------------------------------- */
    OGRODBCTableLayer *poLayer = new OGRODBCTableLayer(this, m_nStatementFlags);

    if (poLayer->Initialize(pszNewName, pszGeomCol))
    {
        delete poLayer;
        return FALSE;
    }

    /* -------------------------------------------------------------------- */
    /*      Add layer to data source layer list.                            */
    /* -------------------------------------------------------------------- */
    papoLayers = (OGRODBCLayer **)CPLRealloc(
        papoLayers, sizeof(OGRODBCLayer *) * (nLayers + 1));
    papoLayers[nLayers++] = poLayer;

    return TRUE;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRODBCDataSource::TestCapability(CPL_UNUSED const char *pszCap)
{
    return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRODBCDataSource::GetLayer(int iLayer)

{
    if (iLayer < 0 || iLayer >= nLayers)
        return nullptr;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                              GetLayerByName()                        */
/************************************************************************/

OGRLayer *OGRODBCDataSource::GetLayerByName(const char *pszLayerName)
{
    OGRLayer *poLayer = GDALDataset::GetLayerByName(pszLayerName);
    if (poLayer != nullptr)
        return poLayer;

    // if table name doesn't exist in database, don't try any further
    const CPLString osLCTableName(CPLString(pszLayerName).tolower());
    if (m_aosAllLCTableNames.find(osLCTableName) == m_aosAllLCTableNames.end())
        return nullptr;

    // try to open the table -- if successful the table will be added to
    // papoLayers as the last item
    if (OpenTable(pszLayerName, nullptr))
        return papoLayers[nLayers - 1];
    else
        return nullptr;
}

/************************************************************************/
/*                    IsPrivateLayerName()                              */
/************************************************************************/

bool OGRODBCDataSource::IsPrivateLayerName(const CPLString &osName)
{
    const CPLString osLCTableName(CPLString(osName).tolower());

    return osLCTableName.size() >= 4 &&
           osLCTableName.substr(0, 4) == "msys";  // MS Access internal tables
}

/************************************************************************/
/*                    IsLayerPrivate()                                  */
/************************************************************************/

bool OGRODBCDataSource::IsLayerPrivate(int iLayer) const
{
    if (iLayer < 0 || iLayer >= nLayers)
        return false;

    const std::string osName(papoLayers[iLayer]->GetName());
    return IsPrivateLayerName(osName);
}

/************************************************************************/
/*                             ExecuteSQL()                             */
/************************************************************************/

OGRLayer *OGRODBCDataSource::ExecuteSQL(const char *pszSQLCommand,
                                        OGRGeometry *poSpatialFilter,
                                        const char *pszDialect)

{
    /* -------------------------------------------------------------------- */
    /*      Use generic implementation for recognized dialects              */
    /* -------------------------------------------------------------------- */
    if (IsGenericSQLDialect(pszDialect))
        return GDALDataset::ExecuteSQL(pszSQLCommand, poSpatialFilter,
                                       pszDialect);

    /* -------------------------------------------------------------------- */
    /*      Execute statement.                                              */
    /* -------------------------------------------------------------------- */
    CPLODBCStatement *poStmt =
        new CPLODBCStatement(&oSession, m_nStatementFlags);

    CPLDebug("ODBC", "ExecuteSQL(%s) called.", pszSQLCommand);
    poStmt->Append(pszSQLCommand);
    if (!poStmt->ExecuteSQL())
    {
        CPLError(CE_Failure, CPLE_AppDefined, "%s", oSession.GetLastError());
        delete poStmt;
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Are there result columns for this statement?                    */
    /* -------------------------------------------------------------------- */
    if (poStmt->GetColCount() == 0)
    {
        delete poStmt;
        CPLErrorReset();
        return nullptr;
    }

    /* -------------------------------------------------------------------- */
    /*      Create a results layer.  It will take ownership of the          */
    /*      statement.                                                      */
    /* -------------------------------------------------------------------- */

    OGRODBCSelectLayer *poLayer = new OGRODBCSelectLayer(this, poStmt);

    if (poSpatialFilter != nullptr)
        poLayer->SetSpatialFilter(poSpatialFilter);

    return poLayer;
}

/************************************************************************/
/*                          ReleaseResultSet()                          */
/************************************************************************/

void OGRODBCDataSource::ReleaseResultSet(OGRLayer *poLayer)

{
    delete poLayer;
}
