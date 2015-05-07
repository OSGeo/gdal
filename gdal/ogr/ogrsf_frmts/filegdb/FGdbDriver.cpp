/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements FileGDB OGR driver.
 * Author:   Ragi Yaser Burhum, ragi@burhum.com
 *           Paul Ramsey, pramsey at cleverelephant.ca
 *
 ******************************************************************************
 * Copyright (c) 2010, Ragi Yaser Burhum
 * Copyright (c) 2011, Paul Ramsey <pramsey at cleverelephant.ca>
 * Copyright (c) 2011-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_fgdb.h"
#include "cpl_conv.h"
#include "FGdbUtils.h"
#include "cpl_multiproc.h"
#include "ogrmutexeddatasource.h"

CPL_CVSID("$Id$");

extern "C" void RegisterOGRFileGDB();

/************************************************************************/
/*                            FGdbDriver()                              */
/************************************************************************/
FGdbDriver::FGdbDriver(): OGRSFDriver(), hMutex(NULL)
{
}

/************************************************************************/
/*                            ~FGdbDriver()                             */
/************************************************************************/
FGdbDriver::~FGdbDriver()

{
    if( oMapConnections.size() != 0 )
        CPLDebug("FileGDB", "Remaining %d connections. Bug?",
                 (int)oMapConnections.size());
    if( hMutex != NULL )
        CPLDestroyMutex(hMutex);
    hMutex = NULL;
}


/************************************************************************/
/*                              GetName()                               */
/************************************************************************/

const char *FGdbDriver::GetName()

{
    return "FileGDB";
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

OGRDataSource *FGdbDriver::Open( const char* pszFilename, int bUpdate )

{
    // First check if we have to do any work.
    int nLen = strlen(pszFilename);
    if(! ((nLen >= 4 && EQUAL(pszFilename + nLen - 4, ".gdb")) ||
          (nLen >= 5 && EQUAL(pszFilename + nLen - 5, ".gdb/"))) )
        return NULL;

    long hr;

    /* Check that the filename is really a directory, to avoid confusion with */
    /* Garmin MapSource - gdb format which can be a problem when the FileGDB */
    /* driver is loaded as a plugin, and loaded before the GPSBabel driver */
    /* (http://trac.osgeo.org/osgeo4w/ticket/245) */
    VSIStatBuf stat;
    if( CPLStat( pszFilename, &stat ) != 0 || !VSI_ISDIR(stat.st_mode) )
    {
        return NULL;
    }

    CPLMutexHolderD(&hMutex);
    Geodatabase* pGeoDatabase = NULL;

    FGdbDatabaseConnection* pConnection = oMapConnections[pszFilename];
    if( pConnection != NULL )
    {
        pGeoDatabase = pConnection->m_pGeodatabase;
        pConnection->m_nRefCount ++;
        CPLDebug("FileGDB", "ref_count of %s = %d now", pszFilename,
                 pConnection->m_nRefCount);
    }
    else
    {
        pGeoDatabase = new Geodatabase;
        hr = ::OpenGeodatabase(StringToWString(pszFilename), *pGeoDatabase);

        if (FAILED(hr) || pGeoDatabase == NULL)
        {
            delete pGeoDatabase;
            
            if( OGRGetDriverByName("OpenFileGDB") != NULL && bUpdate == FALSE )
            {
                std::wstring fgdb_error_desc_w;
                std::string fgdb_error_desc("Unknown error");
                fgdbError er;
                er = FileGDBAPI::ErrorInfo::GetErrorDescription(hr, fgdb_error_desc_w);
                if ( er == S_OK )
                {
                    fgdb_error_desc = WStringToString(fgdb_error_desc_w);
                }
                CPLDebug("FileGDB", "Cannot open %s with FileGDB driver: %s. Failing silently so OpenFileGDB can be tried",
                         pszFilename,
                         fgdb_error_desc.c_str());
            }
            else
            {
                GDBErr(hr, "Failed to open Geodatabase");
            }
            oMapConnections.erase(pszFilename);
            return NULL;
        }

        CPLDebug("FileGDB", "Really opening %s", pszFilename);
        pConnection = new FGdbDatabaseConnection(pGeoDatabase);
        oMapConnections[pszFilename] = pConnection;
    }

    FGdbDataSource* pDS;

    pDS = new FGdbDataSource(this, pConnection);

    if(!pDS->Open( pszFilename, bUpdate ) )
    {
        delete pDS;
        return NULL;
    }
    else
    {
        OGRMutexedDataSource* poMutexedDS =
                new OGRMutexedDataSource(pDS, TRUE, hMutex, TRUE);
        if( bUpdate )
            return OGRCreateEmulatedTransactionDataSourceWrapper(poMutexedDS, this, TRUE, FALSE);
        else
            return poMutexedDS;
    }
}

/***********************************************************************/
/*                     CreateDataSource()                              */
/***********************************************************************/

OGRDataSource* FGdbDriver::CreateDataSource( const char * conn,
                                           char **papszOptions)
{
    long hr;
    Geodatabase *pGeodatabase;
    std::wstring wconn = StringToWString(conn);
    int bUpdate = TRUE; // If we're creating, we must be writing.
    VSIStatBuf stat;

    CPLMutexHolderD(&hMutex);

    /* We don't support options yet, so warn if they send us some */
    if ( papszOptions )
    {
        /* TODO: warning, ignoring options */
    }

    /* Only accept names of form "filename.gdb" and */
    /* also .gdb.zip to be able to return FGDB with MapServer OGR output (#4199) */
    const char* pszExt = CPLGetExtension(conn);
    if ( !(EQUAL(pszExt,"gdb") || EQUAL(pszExt, "zip")) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "FGDB data source name must use 'gdb' extension.\n" );
        return NULL;
    }

    /* Don't try to create on top of something already there */
    if( CPLStat( conn, &stat ) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "%s already exists.\n", conn );
        return NULL;
    }

    /* Try to create the geodatabase */
    pGeodatabase = new Geodatabase; // Create on heap so we can store it in the Datasource
    hr = CreateGeodatabase(wconn, *pGeodatabase);

    /* Handle creation errors */
    if ( S_OK != hr )
    {
        const char *errstr = "Error creating geodatabase (%s).\n";
        if ( hr == -2147220653 )
            errstr = "File already exists (%s).\n";
        delete pGeodatabase;
        CPLError( CE_Failure, CPLE_AppDefined, errstr, conn );
        return NULL;
    }

    FGdbDatabaseConnection* pConnection = new FGdbDatabaseConnection(pGeodatabase);
    oMapConnections[conn] = pConnection;

    /* Ready to embed the Geodatabase in an OGR Datasource */
    FGdbDataSource* pDS = new FGdbDataSource(this, pConnection);
    if ( ! pDS->Open(conn, bUpdate) )
    {
        delete pDS;
        return NULL;
    }
    else
        return OGRCreateEmulatedTransactionDataSourceWrapper(
            new OGRMutexedDataSource(pDS, TRUE, hMutex, TRUE), this,
            TRUE, FALSE);
}

/************************************************************************/
/*                           StartTransaction()                         */
/************************************************************************/

OGRErr FGdbDriver::StartTransaction(OGRDataSource*& poDSInOut, int& bOutHasReopenedDS)
{
    CPLMutexHolderOptionalLockD(hMutex);

    bOutHasReopenedDS = FALSE;

    OGRMutexedDataSource* poMutexedDS = (OGRMutexedDataSource*)poDSInOut;
    FGdbDataSource* poDS = (FGdbDataSource* )poMutexedDS->GetBaseDataSource();
    if( !poDS->GetUpdate() )
        return OGRERR_FAILURE;
    FGdbDatabaseConnection* pConnection = poDS->GetConnection();
    if( pConnection->GetRefCount() != 1 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot start transaction as database is opened in another connection");
        return OGRERR_FAILURE;
    }
    if( pConnection->IsLocked() )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Transaction is already in progress");
        return OGRERR_FAILURE;
    }

    bOutHasReopenedDS = TRUE;

    CPLString osName(poMutexedDS->GetName());
    if( osName[osName.size()-1] == '/' || osName[osName.size()-1] == '\\' )
        osName.resize(osName.size()-1);

    pConnection->m_nRefCount ++;
    delete poDSInOut;
    poDSInOut = NULL;
    poMutexedDS = NULL;
    poDS = NULL;

    ::CloseGeodatabase(*(pConnection->m_pGeodatabase));
    delete pConnection->m_pGeodatabase;
    pConnection->m_pGeodatabase = NULL;

    CPLString osEditedName(osName);
    osEditedName += ".ogredited";

    CPLPushErrorHandler(CPLQuietErrorHandler);
    CPLUnlinkTree(osEditedName);
    CPLPopErrorHandler();

    OGRErr eErr = OGRERR_NONE;
    CPLString osDatabaseToReopen;
    if( EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE1") ||
        CPLCopyTree( osEditedName, osName ) != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot backup geodatabase");
        eErr = OGRERR_FAILURE;
        osDatabaseToReopen = osName;
    }
    else
        osDatabaseToReopen = osEditedName;

    pConnection->m_pGeodatabase = new Geodatabase;
    long hr = ::OpenGeodatabase(StringToWString(osDatabaseToReopen), *(pConnection->m_pGeodatabase));
    if( EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE2") || FAILED(hr))
    {
        delete pConnection->m_pGeodatabase;
        pConnection->m_pGeodatabase = NULL;
        Release(osName);
        GDBErr(hr, CPLSPrintf("Failed to open %s. Dataset should be closed",
                              osDatabaseToReopen.c_str()));

        return OGRERR_FAILURE;
    }

    FGdbDataSource* pDS = new FGdbDataSource(this, pConnection);
    pDS->Open(osName, TRUE);
    poDSInOut = new OGRMutexedDataSource(pDS, TRUE, hMutex, TRUE);

    if( eErr == OGRERR_NONE )
        pConnection->SetLocked(TRUE);
    return eErr;
}

/************************************************************************/
/*                           CommitTransaction()                        */
/************************************************************************/

OGRErr FGdbDriver::CommitTransaction(OGRDataSource*& poDSInOut, int& bOutHasReopenedDS)
{
    CPLMutexHolderOptionalLockD(hMutex);

    bOutHasReopenedDS = FALSE;


    OGRMutexedDataSource* poMutexedDS = (OGRMutexedDataSource*)poDSInOut;
    FGdbDataSource* poDS = (FGdbDataSource* )poMutexedDS->GetBaseDataSource();
    FGdbDatabaseConnection* pConnection = poDS->GetConnection();
    if( !pConnection->IsLocked() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "No transaction in progress");
        return OGRERR_FAILURE;
    }

    bOutHasReopenedDS = TRUE;

    CPLString osName(poMutexedDS->GetName());
    if( osName[osName.size()-1] == '/' || osName[osName.size()-1] == '\\' )
        osName.resize(osName.size()-1);

    pConnection->m_nRefCount ++;
    delete poDSInOut;
    poDSInOut = NULL;
    poMutexedDS = NULL;
    poDS = NULL;

    ::CloseGeodatabase(*(pConnection->m_pGeodatabase));
    delete pConnection->m_pGeodatabase;
    pConnection->m_pGeodatabase = NULL;

    CPLString osEditedName(osName);
    osEditedName += ".ogredited";
    CPLString osTmpName(osName);
    osTmpName += ".ogrtmp";
    
    /* Install the backup copy as the main database in 3 steps : */
    /* first rename the main directory  in .tmp */
    /* then rename the edited copy under regular name */
    /* and finally dispose the .tmp directory */
    /* That way there's no risk definitely losing data */
    if( EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE1") || 
        VSIRename(osName, osTmpName) != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot rename %s to %s. Edited database during transaction is in %s"
                 "Dataset should be closed",
                 osName.c_str(), osTmpName.c_str(), osEditedName.c_str());
        pConnection->SetLocked(FALSE);
        Release(osName);
        return OGRERR_FAILURE;
    }
    
    if( EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE2") || 
        VSIRename(osEditedName, osName) != 0 )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot rename %s to %s. The original geodatabase is in '%s'. "
                 "Dataset should be closed",
                 osEditedName.c_str(), osName.c_str(), osTmpName.c_str());
        pConnection->SetLocked(FALSE);
        Release(osName);
        return OGRERR_FAILURE;
    }

    if( EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE3") || 
        CPLUnlinkTree(osTmpName) != 0 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Cannot remove %s. Manual cleanup required", osTmpName.c_str());
    }

    pConnection->m_pGeodatabase = new Geodatabase;
    long hr = ::OpenGeodatabase(StringToWString(osName), *(pConnection->m_pGeodatabase));
    if( EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE4") || FAILED(hr))
    {
        delete pConnection->m_pGeodatabase;
        pConnection->m_pGeodatabase = NULL;
        pConnection->SetLocked(FALSE);
        Release(osName);
        GDBErr(hr, "Failed to re-open Geodatabase. Dataset should be closed");
        return OGRERR_FAILURE;
    }

    FGdbDataSource* pDS = new FGdbDataSource(this, pConnection);
    pDS->Open(osName, TRUE);
    poDSInOut = new OGRMutexedDataSource(pDS, TRUE, hMutex, TRUE);

    pConnection->SetLocked(FALSE);

    return OGRERR_NONE;
}

/************************************************************************/
/*                           RollbackTransaction()                      */
/************************************************************************/

OGRErr FGdbDriver::RollbackTransaction(OGRDataSource*& poDSInOut, int& bOutHasReopenedDS)
{
    CPLMutexHolderOptionalLockD(hMutex);

    bOutHasReopenedDS = FALSE;

    OGRMutexedDataSource* poMutexedDS = (OGRMutexedDataSource*)poDSInOut;
    FGdbDataSource* poDS = (FGdbDataSource* )poMutexedDS->GetBaseDataSource();
    FGdbDatabaseConnection* pConnection = poDS->GetConnection();
    if( !pConnection->IsLocked() )
    {
        CPLError(CE_Failure, CPLE_NotSupported,
                 "No transaction in progress");
        return OGRERR_FAILURE;
    }

    bOutHasReopenedDS = TRUE;

    CPLString osName(poMutexedDS->GetName());
    if( osName[osName.size()-1] == '/' || osName[osName.size()-1] == '\\' )
        osName.resize(osName.size()-1);

    pConnection->m_nRefCount ++;
    delete poDSInOut;
    poDSInOut = NULL;
    poMutexedDS = NULL;
    poDS = NULL;

    ::CloseGeodatabase(*(pConnection->m_pGeodatabase));
    delete pConnection->m_pGeodatabase;
    pConnection->m_pGeodatabase = NULL;

    CPLString osEditedName(osName);
    osEditedName += ".ogredited";

    OGRErr eErr = OGRERR_NONE;
    if( EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE1") || 
        CPLUnlinkTree(osEditedName) != 0 )
    {
        CPLError(CE_Warning, CPLE_AppDefined,
                 "Cannot remove %s. Manual cleanup required", osEditedName.c_str());
        eErr = OGRERR_FAILURE;
    }

    pConnection->m_pGeodatabase = new Geodatabase;
    long hr = ::OpenGeodatabase(StringToWString(osName), *(pConnection->m_pGeodatabase));
    if (EQUAL(CPLGetConfigOption("FGDB_SIMUL_FAIL", ""), "CASE2") ||
        FAILED(hr))
    {
        delete pConnection->m_pGeodatabase;
        pConnection->m_pGeodatabase = NULL;
        pConnection->SetLocked(FALSE);
        Release(osName);
        GDBErr(hr, "Failed to re-open Geodatabase. Dataset should be closed");
        return OGRERR_FAILURE;
    }

    FGdbDataSource* pDS = new FGdbDataSource(this, pConnection);
    pDS->Open(osName, TRUE);
    poDSInOut = new OGRMutexedDataSource(pDS, TRUE, hMutex, TRUE);

    pConnection->SetLocked(FALSE);

    return eErr;
}

/***********************************************************************/
/*                       ReadoptOldFeatureDefn()                       */
/***********************************************************************/

void FGdbDriver::ReadoptOldFeatureDefn(CPL_UNUSED OGRDataSource* poDS,
                                       OGRLayer* poLayer,
                                       OGRFeatureDefn* poFeatureDefn)
{
    FGdbLayer* poFGdbLayer = (FGdbLayer* )((OGRMutexedLayer*)poLayer)->GetBaseLayer();
    poFGdbLayer->ReadoptOldFeatureDefn(poFeatureDefn);
}

/***********************************************************************/
/*                            Release()                                */
/***********************************************************************/

void FGdbDriver::Release(const char* pszName)
{
    CPLMutexHolderOptionalLockD(hMutex);

    FGdbDatabaseConnection* pConnection = oMapConnections[pszName];
    if( pConnection != NULL )
    {
        pConnection->m_nRefCount --;
        CPLDebug("FileGDB", "ref_count of %s = %d now", pszName,
                 pConnection->m_nRefCount);
        if( pConnection->m_nRefCount == 0 )
        {
            if( pConnection->m_pGeodatabase != NULL )
            {
                CPLDebug("FileGDB", "Really closing %s now", pszName);
                ::CloseGeodatabase(*(pConnection->m_pGeodatabase));
                delete pConnection->m_pGeodatabase;
                pConnection->m_pGeodatabase = NULL;
            }
            delete pConnection;
            oMapConnections.erase(pszName);
        }
    }
}

/***********************************************************************/
/*                         TestCapability()                            */
/***********************************************************************/

int FGdbDriver::TestCapability( const char * pszCap )
{
    if (EQUAL(pszCap, ODrCCreateDataSource) )
        return TRUE;

    else if (EQUAL(pszCap, ODrCDeleteDataSource) )
        return TRUE;

    return FALSE;
}
/************************************************************************/
/*                          DeleteDataSource()                          */
/************************************************************************/

OGRErr FGdbDriver::DeleteDataSource( const char *pszDataSource )
{
    CPLMutexHolderD(&hMutex);

    std::wstring wstr = StringToWString(pszDataSource);

    long hr;

    if (S_OK != (hr = ::DeleteGeodatabase(wstr)))
    {
        GDBErr(hr, "Failed to delete Geodatabase");
        return OGRERR_FAILURE;
    }

    return OGRERR_NONE;
}

/***********************************************************************/
/*                       RegisterOGRFileGDB()                          */
/***********************************************************************/

void RegisterOGRFileGDB()

{
    if (! GDAL_CHECK_VERSION("OGR FGDB"))
        return;
    OGRSFDriver* poDriver = new FGdbDriver;
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                "ESRI FileGDB" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "gdb" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                "drv_filegdb.html" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, "<CreationOptionList/>" );

    poDriver->SetMetadataItem( GDAL_DS_LAYER_CREATIONOPTIONLIST,
"<LayerCreationOptionList>"
"  <Option name='FEATURE_DATASET' type='string' description='FeatureDataset folder into to put the new layer'/>"
"  <Option name='GEOMETRY_NAME' type='string' description='Name of geometry column' default='SHAPE'/>"
"  <Option name='GEOMETRY_NULLABLE' type='boolean' description='Whether the values of the geometry column can be NULL' default='YES'/>"
"  <Option name='FID' type='string' description='Name of OID column' default='OBJECTID' deprecated_alias='OID_NAME'/>"
"  <Option name='XYTOLERANCE' type='float' description='Snapping tolerance, used for advanced ArcGIS features like network and topology rules, on 2D coordinates, in the units of the CRS'/>"
"  <Option name='ZTOLERANCE' type='float' description='Snapping tolerance, used for advanced ArcGIS features like network and topology rules, on Z coordinates, in the units of the CRS'/>"
"  <Option name='XORIGIN' type='float' description='X origin of the coordinate precision grid'/>"
"  <Option name='YORIGIN' type='float' description='Y origin of the coordinate precision grid'/>"
"  <Option name='ZORIGIN' type='float' description='Z origin of the coordinate precision grid'/>"
"  <Option name='XYSCALE' type='float' description='X,Y scale of the coordinate precision grid'/>"
"  <Option name='ZSCALE' type='float' description='Z scale of the coordinate precision grid'/>"
"  <Option name='XML_DEFINITION' type='string' description='XML definition to create the new table. The root node of such a XML definition must be a <esri:DataElement> element conformant to FileGDBAPI.xsd'/>"
"  <Option name='CREATE_MULTIPATCH' type='boolean' description='Whether to write geometries of layers of type MultiPolygon as MultiPatch' default='NO'/>"
"  <Option name='COLUMN_TYPES' type='string' description='A list of strings of format field_name=fgdb_filed_type (separated by comma) to force the FileGDB column type of fields to be created'/>"
"  <Option name='CONFIGURATION_KEYWORD' type='string-select' description='Customize how data is stored. By default text in UTF-8 and data up to 1TB'>"
"    <Value>DEFAULTS</Value>"
"    <Value>TEXT_UTF16</Value>"
"    <Value>MAX_FILE_SIZE_4GB</Value>"
"    <Value>MAX_FILE_SIZE_256TB</Value>"
"    <Value>GEOMETRY_OUTOFLINE</Value>"
"    <Value>BLOB_OUTOFLINE</Value>"
"    <Value>GEOMETRY_AND_BLOB_OUTOFLINE</Value>"
"  </Option>"
"</LayerCreationOptionList>");
    
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONFIELDDATATYPES, "Integer Real String Date DateTime Binary" );
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_DEFAULT_FIELDS, "YES" );
    poDriver->SetMetadataItem( GDAL_DCAP_NOTNULL_GEOMFIELDS, "YES" );

    OGRSFDriverRegistrar::GetRegistrar()->RegisterDriver(poDriver);
}

