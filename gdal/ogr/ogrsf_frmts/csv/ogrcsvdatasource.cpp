/******************************************************************************
 *
 * Project:  CSV Translator
 * Purpose:  Implements OGRCSVDataSource class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"
#include "cpl_vsi_virtual.h"

#include "ogr_csv.h"

#include "ogreditablelayer.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                     OGRCSVEditableLayerSynchronizer                  */
/************************************************************************/

class OGRCSVEditableLayerSynchronizer: public IOGREditableLayerSynchronizer
{
            OGRCSVLayer *m_poCSVLayer;
            char        **m_papszOpenOptions;

    public:
                            OGRCSVEditableLayerSynchronizer(OGRCSVLayer* poCSVLayer,
                                                            char** papszOpenOptions) :
                                                m_poCSVLayer(poCSVLayer),
                                                m_papszOpenOptions(CSLDuplicate(papszOpenOptions)) {}
                   virtual ~OGRCSVEditableLayerSynchronizer();

            virtual OGRErr EditableSyncToDisk(OGRLayer* poEditableLayer,
                                              OGRLayer** ppoDecoratedLayer) override;
};

/************************************************************************/
/*                     ~OGRCSVEditableLayerSynchronizer()               */
/************************************************************************/

OGRCSVEditableLayerSynchronizer::~OGRCSVEditableLayerSynchronizer()
{
    CSLDestroy(m_papszOpenOptions);
}

/************************************************************************/
/*                       EditableSyncToDisk()                           */
/************************************************************************/

OGRErr OGRCSVEditableLayerSynchronizer::EditableSyncToDisk(OGRLayer* poEditableLayer,
                                                           OGRLayer** ppoDecoratedLayer)
{
    CPLAssert( m_poCSVLayer == *ppoDecoratedLayer );

    CPLString osLayerName(m_poCSVLayer->GetName());
    CPLString osFilename(m_poCSVLayer->GetFilename());
    const bool bCreateCSVT = m_poCSVLayer->GetCreateCSVT();
    CPLString osCSVTFilename(CPLResetExtension(osFilename, "csvt"));
    VSIStatBufL sStatBuf;
    const bool bHasCSVT = VSIStatL(osCSVTFilename, &sStatBuf) == 0;
    CPLString osTmpFilename(osFilename);
    CPLString osTmpCSVTFilename(osFilename);
    if( VSIStatL(osFilename, &sStatBuf) == 0 )
    {
        osTmpFilename += "_ogr_tmp.csv";
        osTmpCSVTFilename += "_ogr_tmp.csvt";
    }
    const char chDelimiter = m_poCSVLayer->GetDelimiter();
    OGRCSVLayer* poCSVTmpLayer = new OGRCSVLayer( osLayerName, NULL,
                                                  osTmpFilename,
                                                  true, true, chDelimiter );
    poCSVTmpLayer->BuildFeatureDefn(NULL, NULL, m_papszOpenOptions);
    poCSVTmpLayer->SetCRLF( m_poCSVLayer->GetCRLF() );
    poCSVTmpLayer->SetCreateCSVT( bCreateCSVT || bHasCSVT );
    poCSVTmpLayer->SetWriteBOM( m_poCSVLayer->GetWriteBOM() );

    if( m_poCSVLayer->GetGeometryFormat() == OGR_CSV_GEOM_AS_WKT )
        poCSVTmpLayer->SetWriteGeometry( wkbNone, OGR_CSV_GEOM_AS_WKT, NULL );

    OGRErr eErr = OGRERR_NONE;
    OGRFeatureDefn* poEditableFDefn =  poEditableLayer->GetLayerDefn();
    for( int i=0; eErr == OGRERR_NONE &&
                  i < poEditableFDefn->GetFieldCount(); i++ )
    {
        OGRFieldDefn oFieldDefn(poEditableFDefn->GetFieldDefn(i));
        int iGeomFieldIdx = 0;
        if( (EQUAL(oFieldDefn.GetNameRef(), "WKT") &&
             (iGeomFieldIdx = poEditableFDefn->GetGeomFieldIndex("")) >= 0) ||
            (iGeomFieldIdx = poEditableFDefn->GetGeomFieldIndex(oFieldDefn.GetNameRef())) >= 0 )
        {
            OGRGeomFieldDefn oGeomFieldDefn(
                poEditableFDefn->GetGeomFieldDefn(iGeomFieldIdx) );
            eErr = poCSVTmpLayer->CreateGeomField( &oGeomFieldDefn );
        }
        else
        {
            eErr = poCSVTmpLayer->CreateField( &oFieldDefn );
        }
    }

    const bool bHasXY = ( m_poCSVLayer->GetXField().size() != 0 &&
                          m_poCSVLayer->GetYField().size() != 0 );
    const bool bHasZ = ( m_poCSVLayer->GetZField().size() != 0 );
    if( bHasXY && !CPLFetchBool(m_papszOpenOptions, "KEEP_GEOM_COLUMNS", true) )
    {
        if( poCSVTmpLayer->GetLayerDefn()->GetFieldIndex(m_poCSVLayer->GetXField()) < 0 )
        {
            OGRFieldDefn oFieldDefn(m_poCSVLayer->GetXField(), OFTReal);
            if( eErr == OGRERR_NONE )
                eErr = poCSVTmpLayer->CreateField( &oFieldDefn );
        }
        if( poCSVTmpLayer->GetLayerDefn()->GetFieldIndex(m_poCSVLayer->GetYField()) < 0 )
        {
            OGRFieldDefn oFieldDefn(m_poCSVLayer->GetYField(), OFTReal);
            if( eErr == OGRERR_NONE )
                eErr = poCSVTmpLayer->CreateField( &oFieldDefn );
        }
        if( bHasZ && poCSVTmpLayer->GetLayerDefn()->GetFieldIndex(m_poCSVLayer->GetZField()) < 0 )
        {
            OGRFieldDefn oFieldDefn(m_poCSVLayer->GetZField(), OFTReal);
            if( eErr == OGRERR_NONE )
                eErr = poCSVTmpLayer->CreateField( &oFieldDefn );
        }
    }

    int nFirstGeomColIdx = 0;
    if( m_poCSVLayer->HasHiddenWKTColumn() )
    {
        poCSVTmpLayer->SetWriteGeometry(
            poEditableFDefn->GetGeomFieldDefn(0)->GetType(),
            OGR_CSV_GEOM_AS_WKT,
            poEditableFDefn->GetGeomFieldDefn(0)->GetNameRef());
        nFirstGeomColIdx = 1;
    }

    if( !(poEditableFDefn->GetGeomFieldCount() == 1 && bHasXY) )
    {
        for( int i=nFirstGeomColIdx; eErr == OGRERR_NONE &&
                i < poEditableFDefn->GetGeomFieldCount(); i++ )
        {
            OGRGeomFieldDefn oGeomFieldDefn( poEditableFDefn->GetGeomFieldDefn(i) );
            if( poCSVTmpLayer->GetLayerDefn()->GetGeomFieldIndex(oGeomFieldDefn.GetNameRef()) >= 0 )
                continue;
            eErr = poCSVTmpLayer->CreateGeomField( &oGeomFieldDefn );
        }
    }

    OGRFeature* poFeature = NULL;
    poEditableLayer->ResetReading();
    while( eErr == OGRERR_NONE &&
           (poFeature = poEditableLayer->GetNextFeature()) != NULL )
    {
        OGRFeature* poNewFeature = new OGRFeature( poCSVTmpLayer->GetLayerDefn() );
        poNewFeature->SetFrom(poFeature);
        if( bHasXY )
        {
            OGRGeometry* poGeom = poFeature->GetGeometryRef();
            if( poGeom != NULL && wkbFlatten(poGeom->getGeometryType()) == wkbPoint )
            {
                poNewFeature->SetField( m_poCSVLayer->GetXField(),
                                        static_cast<OGRPoint*>(poGeom)->getX());
                poNewFeature->SetField( m_poCSVLayer->GetYField(),
                                        static_cast<OGRPoint*>(poGeom)->getY());
                if( bHasZ )
                {
                    poNewFeature->SetField( m_poCSVLayer->GetZField(),
                                        static_cast<OGRPoint*>(poGeom)->getZ());
                }
            }
        }
        eErr = poCSVTmpLayer->CreateFeature(poNewFeature);
        delete poFeature;
        delete poNewFeature;
    }
    delete poCSVTmpLayer;

    if( eErr != OGRERR_NONE )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Error while creating %s",
                 osTmpFilename.c_str());
        VSIUnlink( osTmpFilename );
        VSIUnlink( CPLResetExtension(osTmpFilename, "csvt") );
        return eErr;
    }

    delete m_poCSVLayer;

    if( osFilename != osTmpFilename )
    {
        CPLString osTmpOriFilename(osFilename + ".ogr_bak");
        CPLString osTmpOriCSVTFilename(osCSVTFilename + ".ogr_bak");
        if( VSIRename( osFilename, osTmpOriFilename ) != 0 ||
            (bHasCSVT && VSIRename( osCSVTFilename, osTmpOriCSVTFilename ) != 0 ) ||
            VSIRename( osTmpFilename, osFilename) != 0 ||
            (bHasCSVT && VSIRename( osTmpCSVTFilename, osCSVTFilename ) != 0) )
        {
            CPLError(CE_Failure, CPLE_AppDefined, "Cannot rename files");
            *ppoDecoratedLayer = NULL;
            m_poCSVLayer = NULL;
            return OGRERR_FAILURE;
        }
        VSIUnlink( osTmpOriFilename );
        if( bHasCSVT )
            VSIUnlink( osTmpOriCSVTFilename );
    }

    VSILFILE* fp = VSIFOpenL( osFilename, "rb+" );
    if( fp == NULL )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Cannot reopen updated %s",
                 osFilename.c_str());
        *ppoDecoratedLayer = NULL;
        m_poCSVLayer = NULL;
        return OGRERR_FAILURE;
    }

    m_poCSVLayer = new OGRCSVLayer( osLayerName, fp,
                                    osFilename,
                                    false, /* new */
                                    true, /* update */
                                    chDelimiter );
    m_poCSVLayer->BuildFeatureDefn(NULL, NULL, m_papszOpenOptions);
    *ppoDecoratedLayer = m_poCSVLayer;

    return OGRERR_NONE;
}

/************************************************************************/
/*                        OGRCSVEditableLayer                           */
/************************************************************************/

class OGRCSVEditableLayer: public OGREditableLayer
{
    public:
        OGRCSVEditableLayer(OGRCSVLayer* poCSVLayer,
                            char** papszOpenOptions);

    virtual OGRErr      CreateField( OGRFieldDefn *poField,
                                     int bApproxOK = TRUE ) override;
    virtual GIntBig     GetFeatureCount( int bForce = TRUE ) override;
};

/************************************************************************/
/*                       GRCSVEditableLayer()                           */
/************************************************************************/

OGRCSVEditableLayer::OGRCSVEditableLayer(OGRCSVLayer* poCSVLayer,
                                         char** papszOpenOptions) :
    OGREditableLayer(poCSVLayer, true,
                     new OGRCSVEditableLayerSynchronizer(
                         poCSVLayer, papszOpenOptions),
                     true)
{
    SetSupportsCreateGeomField(true);
    SetSupportsCurveGeometries(true);
}

/************************************************************************/
/*                            CreateField()                             */
/************************************************************************/

OGRErr OGRCSVEditableLayer::CreateField( OGRFieldDefn *poNewField,
                                         int bApproxOK )

{
    OGRCSVCreateFieldAction eAction = OGRCSVLayer::PreCreateField(
                            m_poEditableFeatureDefn, poNewField, bApproxOK );
    if( eAction == CREATE_FIELD_DO_NOTHING )
        return OGRERR_NONE;
    if( eAction == CREATE_FIELD_ERROR )
        return OGRERR_FAILURE;
    return OGREditableLayer::CreateField(poNewField, bApproxOK);
}

/************************************************************************/
/*                        GetFeatureCount()                             */
/************************************************************************/

GIntBig OGRCSVEditableLayer::GetFeatureCount( int bForce )
{
    GIntBig nRet = OGREditableLayer::GetFeatureCount(bForce);
    if( m_poDecoratedLayer != NULL && m_nNextFID <= 0 )
    {
        GIntBig nTotalFeatureCount =
            static_cast<OGRCSVLayer*>(m_poDecoratedLayer)->GetTotalFeatureCount();
        if( nTotalFeatureCount >= 0 )
            SetNextFID(nTotalFeatureCount+1);
    }
    return nRet;
}

/************************************************************************/
/*                          OGRCSVDataSource()                          */
/************************************************************************/

OGRCSVDataSource::OGRCSVDataSource() :
    pszName(NULL),
    papoLayers(NULL),
    nLayers(0),
    bUpdate(false),
    bEnableGeometryFields(false)
{}

/************************************************************************/
/*                         ~OGRCSVDataSource()                          */
/************************************************************************/

OGRCSVDataSource::~OGRCSVDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

    if( bUpdate )
        OGRCSVDriverRemoveFromMap(pszName, this);

    CPLFree( pszName );
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRCSVDataSource::TestCapability( const char * pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return bUpdate;
    else if( EQUAL(pszCap,ODsCDeleteLayer) )
        return bUpdate;
    else if( EQUAL(pszCap,ODsCCreateGeomFieldAfterCreateLayer) )
        return bUpdate && bEnableGeometryFields;
    else if( EQUAL(pszCap,ODsCCurveGeometries) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCMeasuredGeometries) )
        return TRUE;
    else if( EQUAL(pszCap,ODsCRandomLayerWrite) )
        return bUpdate;
    else
        return FALSE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRCSVDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;

    return papoLayers[iLayer];
}

/************************************************************************/
/*                          GetRealExtension()                          */
/************************************************************************/

CPLString OGRCSVDataSource::GetRealExtension(CPLString osFilename)
{
    CPLString osExt = CPLGetExtension(osFilename);
    if( STARTS_WITH(osFilename, "/vsigzip/") && EQUAL(osExt, "gz") )
    {
        if( strlen(osFilename) > 7
            && EQUAL(osFilename + strlen(osFilename) - 7, ".csv.gz") )
            osExt = "csv";
        else if( strlen(osFilename) > 7
                 && EQUAL(osFilename + strlen(osFilename) - 7, ".tsv.gz") )
            osExt = "tsv";
    }
    return osExt;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRCSVDataSource::Open( const char * pszFilename, int bUpdateIn,
                            int bForceOpen, char** papszOpenOptionsIn )

{
    pszName = CPLStrdup( pszFilename );
    bUpdate = CPL_TO_BOOL(bUpdateIn);

    if( bUpdate && bForceOpen && EQUAL(pszFilename, "/vsistdout/") )
        return TRUE;

    /* For writable /vsizip/, do nothing more */
    if( bUpdate && bForceOpen && STARTS_WITH(pszFilename, "/vsizip/") )
        return TRUE;

    CPLString osFilename(pszFilename);
    CPLString osBaseFilename = CPLGetFilename(pszFilename);
    CPLString osExt = GetRealExtension(osFilename);
    // pszFilename = NULL;

    bool bIgnoreExtension = STARTS_WITH_CI(osFilename, "CSV:");
    bool bUSGeonamesFile = false;
    /* bool bGeonamesOrgFile = false; */
    if (bIgnoreExtension)
    {
        osFilename = osFilename + 4;
    }

    // Those are *not* real .XLS files, but text file with tab as column
    // separator.
    if (EQUAL(osBaseFilename, "NfdcFacilities.xls") ||
        EQUAL(osBaseFilename, "NfdcRunways.xls") ||
        EQUAL(osBaseFilename, "NfdcRemarks.xls") ||
        EQUAL(osBaseFilename, "NfdcSchedules.xls"))
    {
        if( bUpdate )
            return FALSE;
        bIgnoreExtension = true;
    }
    else if ((STARTS_WITH_CI(osBaseFilename, "NationalFile_") ||
              STARTS_WITH_CI(osBaseFilename, "POP_PLACES_") ||
              STARTS_WITH_CI(osBaseFilename, "HIST_FEATURES_") ||
              STARTS_WITH_CI(osBaseFilename, "US_CONCISE_") ||
              STARTS_WITH_CI(osBaseFilename, "AllNames_") ||
              STARTS_WITH_CI(osBaseFilename, "Feature_Description_History_") ||
              STARTS_WITH_CI(osBaseFilename, "ANTARCTICA_") ||
              STARTS_WITH_CI(osBaseFilename, "GOVT_UNITS_") ||
              STARTS_WITH_CI(osBaseFilename, "NationalFedCodes_") ||
              STARTS_WITH_CI(osBaseFilename, "AllStates_") ||
              STARTS_WITH_CI(osBaseFilename, "AllStatesFedCodes_") ||
              ( strlen(osBaseFilename) > 2 &&
                STARTS_WITH_CI(osBaseFilename+2, "_Features_")) ||
              ( strlen(osBaseFilename) > 2 &&
                STARTS_WITH_CI(osBaseFilename+2, "_FedCodes_"))) &&
             (EQUAL(osExt, "txt") || EQUAL(osExt, "zip")) )
    {
        if( bUpdate )
            return FALSE;
        bIgnoreExtension = true;
        bUSGeonamesFile = true;

        if (EQUAL(osExt, "zip") &&
            strstr(osFilename, "/vsizip/") == NULL )
        {
            osFilename = "/vsizip/" + osFilename;
        }
    }
    else if (EQUAL(osBaseFilename, "allCountries.txt") ||
             EQUAL(osBaseFilename, "allCountries.zip"))
    {
        if( bUpdate )
            return FALSE;
        bIgnoreExtension = true;
        /* bGeonamesOrgFile = true; */

        if (EQUAL(osExt, "zip") &&
            strstr(osFilename, "/vsizip/") == NULL )
        {
            osFilename = "/vsizip/" + osFilename;
        }
    }

/* -------------------------------------------------------------------- */
/*      Determine what sort of object this is.                          */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if( VSIStatExL( osFilename, &sStatBuf, VSI_STAT_NATURE_FLAG ) != 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Is this a single CSV file?                                      */
/* -------------------------------------------------------------------- */
    if( VSI_ISREG(sStatBuf.st_mode)
        && (bIgnoreExtension || EQUAL(osExt,"csv") || EQUAL(osExt,"tsv")) )
    {
        if (EQUAL(CPLGetFilename(osFilename), "NfdcFacilities.xls"))
        {
            return OpenTable( osFilename, papszOpenOptionsIn, "ARP");
        }
        else if (EQUAL(CPLGetFilename(osFilename), "NfdcRunways.xls"))
        {
            OpenTable( osFilename, papszOpenOptionsIn, "BaseEndPhysical");
            OpenTable( osFilename, papszOpenOptionsIn, "BaseEndDisplaced");
            OpenTable( osFilename, papszOpenOptionsIn, "ReciprocalEndPhysical");
            OpenTable( osFilename, papszOpenOptionsIn,
                       "ReciprocalEndDisplaced");
            return nLayers != 0;
        }
        else if (bUSGeonamesFile)
        {
            /* GNIS specific */
            if (STARTS_WITH_CI(osBaseFilename, "NationalFedCodes_") ||
                STARTS_WITH_CI(osBaseFilename, "AllStatesFedCodes_") ||
                STARTS_WITH_CI(osBaseFilename, "ANTARCTICA_") ||
                ( strlen(osBaseFilename) > 2 &&
                  STARTS_WITH_CI(osBaseFilename+2, "_FedCodes_")))
            {
                OpenTable( osFilename, papszOpenOptionsIn, NULL, "PRIMARY");
            }
            else if (STARTS_WITH_CI(osBaseFilename, "GOVT_UNITS_") ||
                     STARTS_WITH_CI(osBaseFilename,
                                    "Feature_Description_History_"))
            {
                OpenTable( osFilename, papszOpenOptionsIn, NULL, "");
            }
            else
            {
                OpenTable( osFilename, papszOpenOptionsIn, NULL, "PRIM");
                OpenTable( osFilename, papszOpenOptionsIn, NULL, "SOURCE");
            }
            return nLayers != 0;
        }

        return OpenTable( osFilename, papszOpenOptionsIn );
    }

/* -------------------------------------------------------------------- */
/*      Is this a single a ZIP file with only a CSV file inside ?       */
/* -------------------------------------------------------------------- */
    if( STARTS_WITH(osFilename, "/vsizip/") &&
        EQUAL(osExt, "zip") &&
        VSI_ISREG(sStatBuf.st_mode) )
    {
        char** papszFiles = VSIReadDir(osFilename);
        if (CSLCount(papszFiles) != 1 ||
            !EQUAL(CPLGetExtension(papszFiles[0]), "CSV"))
        {
            CSLDestroy(papszFiles);
            return FALSE;
        }
        osFilename = CPLFormFilename(osFilename, papszFiles[0], NULL);
        CSLDestroy(papszFiles);
        return OpenTable( osFilename, papszOpenOptionsIn );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise it has to be a directory.                             */
/* -------------------------------------------------------------------- */
    if( !VSI_ISDIR(sStatBuf.st_mode) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Scan through for entries ending in .csv.                        */
/* -------------------------------------------------------------------- */
    int nNotCSVCount = 0;
    char **papszNames = VSIReadDir( osFilename );

    for( int i = 0; papszNames != NULL && papszNames[i] != NULL; i++ )
    {
        CPLString oSubFilename =
            CPLFormFilename( osFilename, papszNames[i], NULL );

        if( EQUAL(papszNames[i],".") || EQUAL(papszNames[i],"..") )
            continue;

        if (EQUAL(CPLGetExtension(oSubFilename),"csvt"))
            continue;

        if( VSIStatL( oSubFilename, &sStatBuf ) != 0
            || !VSI_ISREG(sStatBuf.st_mode) )
        {
            nNotCSVCount++;
            continue;
        }

        if (EQUAL(CPLGetExtension(oSubFilename),"csv"))
        {
            if( !OpenTable( oSubFilename, papszOpenOptionsIn ) )
            {
                CPLDebug("CSV", "Cannot open %s", oSubFilename.c_str());
                nNotCSVCount++;
                continue;
            }
        }

        /* GNIS specific */
        else if ( strlen(papszNames[i]) > 2 &&
                  STARTS_WITH_CI(papszNames[i]+2, "_Features_") &&
                  EQUAL(CPLGetExtension(papszNames[i]), "txt") )
        {
            bool bRet =
                OpenTable( oSubFilename, papszOpenOptionsIn, NULL, "PRIM");
            bRet |=
                OpenTable( oSubFilename, papszOpenOptionsIn, NULL, "SOURCE");
            if ( !bRet )
            {
                CPLDebug("CSV", "Cannot open %s", oSubFilename.c_str());
                nNotCSVCount++;
                continue;
            }
        }
        /* GNIS specific */
        else if ( strlen(papszNames[i]) > 2 &&
                  STARTS_WITH_CI(papszNames[i]+2, "_FedCodes_") &&
                  EQUAL(CPLGetExtension(papszNames[i]), "txt") )
        {
            if ( !OpenTable( oSubFilename, papszOpenOptionsIn,
                             NULL, "PRIMARY") )
            {
                CPLDebug("CSV", "Cannot open %s", oSubFilename.c_str());
                nNotCSVCount++;
                continue;
            }
        }
        else
        {
            nNotCSVCount++;
            continue;
        }
    }

    CSLDestroy( papszNames );

/* -------------------------------------------------------------------- */
/*      We presume that this is indeed intended to be a CSV             */
/*      datasource if over half the files were .csv files.              */
/* -------------------------------------------------------------------- */
    return bForceOpen || nNotCSVCount < nLayers;
}

/************************************************************************/
/*                              OpenTable()                             */
/************************************************************************/

bool OGRCSVDataSource::OpenTable( const char * pszFilename,
                                  char** papszOpenOptionsIn,
                                  const char* pszNfdcRunwaysGeomField,
                                  const char* pszGeonamesGeomFieldPrefix)

{
/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    VSILFILE *fp = NULL;

    if( bUpdate )
        fp = VSIFOpenL( pszFilename, "rb+" );
    else
        fp = VSIFOpenL( pszFilename, "rb" );
    if( fp == NULL )
    {
        CPLError( CE_Warning, CPLE_OpenFailed,
                  "Failed to open %s, %s.",
                  pszFilename, VSIStrerror( errno ) );
        return false;
    }

    if( !bUpdate && strstr(pszFilename, "/vsigzip/") == NULL &&
        strstr(pszFilename, "/vsizip/") == NULL )
        fp = reinterpret_cast<VSILFILE *>(
            VSICreateBufferedReaderHandle(
                reinterpret_cast<VSIVirtualHandle *>( fp ) ) );

    CPLString osLayerName = CPLGetBasename(pszFilename);
    CPLString osExt = CPLGetExtension(pszFilename);
    if( STARTS_WITH(pszFilename, "/vsigzip/") && EQUAL(osExt, "gz") )
    {
        if( strlen(pszFilename) > 7
            && EQUAL(pszFilename + strlen(pszFilename) - 7, ".csv.gz") )
        {
            osLayerName = osLayerName.substr(0, osLayerName.size() - 4);
            osExt = "csv";
        }
        else if( strlen(pszFilename) > 7
                 && EQUAL(pszFilename + strlen(pszFilename) - 7, ".tsv.gz") )
        {
            osLayerName = osLayerName.substr(0, osLayerName.size() - 4);
            osExt = "tsv";
        }
    }

/* -------------------------------------------------------------------- */
/*      Read and parse a line.  Did we get multiple fields?             */
/* -------------------------------------------------------------------- */

    const char* pszLine = CPLReadLineL( fp );
    if (pszLine == NULL)
    {
        VSIFCloseL( fp );
        return false;
    }
    char chDelimiter = CSVDetectSeperator(pszLine);
    if( chDelimiter != '\t' && strchr(pszLine, '\t') != NULL )
    {
        // Force the delimiter to be TAB for a .tsv file that has a tabulation
        // in its first line */
        if( EQUAL(osExt, "tsv") )
        {
            chDelimiter = '\t';
        }
        else
        {
            for( int nDontHonourStrings = 0;
                 nDontHonourStrings <= 1;
                 nDontHonourStrings++ )
            {
                const bool bDontHonourStrings = CPL_TO_BOOL(nDontHonourStrings);
                // Read the first 2 lines to see if they have the same number
                // of fields, if using tabulation.
                VSIRewindL( fp );
                char** papszTokens = OGRCSVReadParseLineL( fp, '\t',
                                                           bDontHonourStrings );
                int nTokens1 = CSLCount(papszTokens);
                CSLDestroy(papszTokens);
                papszTokens = OGRCSVReadParseLineL( fp, '\t',
                                                    bDontHonourStrings );
                int nTokens2 = CSLCount(papszTokens);
                CSLDestroy(papszTokens);
                if( nTokens1 >= 2 && nTokens1 == nTokens2 )
                {
                    chDelimiter = '\t';
                    break;
                }
            }
        }
    }

    VSIRewindL( fp );

#if 0
    const char *pszDelimiter = CSLFetchNameValueDef( papszOpenOptionsIn,
                                                     "SEPARATOR", "AUTO");
    if( !EQUAL(pszDelimiter, "AUTO") )
    {
        if (EQUAL(pszDelimiter, "COMMA"))
            chDelimiter = ',';
        else if (EQUAL(pszDelimiter, "SEMICOLON"))
            chDelimiter = ';';
        else if (EQUAL(pszDelimiter, "TAB"))
            chDelimiter = '\t';
        else if (EQUAL(pszDelimiter, "SPACE"))
            chDelimiter = ' ';
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "SEPARATOR=%s not understood, use one of COMMA, "
                      "SEMICOLON, SPACE or TAB.",
                      pszDelimiter );
        }
    }
#endif

    /* GNIS specific */
    if (pszGeonamesGeomFieldPrefix != NULL &&
        strchr(pszLine, '|') != NULL)
        chDelimiter = '|';

    char **papszFields = OGRCSVReadParseLineL( fp, chDelimiter, false );

    if( CSLCount(papszFields) < 2 )
    {
        VSIFCloseL( fp );
        CSLDestroy( papszFields );
        return false;
    }

    VSIRewindL( fp );
    CSLDestroy( papszFields );

/* -------------------------------------------------------------------- */
/*      Create a layer.                                                 */
/* -------------------------------------------------------------------- */
    nLayers++;
    papoLayers = static_cast<OGRLayer **>(
        CPLRealloc( papoLayers, sizeof(void*) * nLayers ) );

    if (pszNfdcRunwaysGeomField != NULL)
    {
        osLayerName += "_";
        osLayerName += pszNfdcRunwaysGeomField;
    }
    else if (pszGeonamesGeomFieldPrefix != NULL &&
             !EQUAL(pszGeonamesGeomFieldPrefix, ""))
    {
        osLayerName += "_";
        osLayerName += pszGeonamesGeomFieldPrefix;
    }
    if (EQUAL(pszFilename, "/vsistdin/"))
        osLayerName = "layer";

    OGRCSVLayer* poCSVLayer =
        new OGRCSVLayer( osLayerName, fp, pszFilename, FALSE, bUpdate,
                         chDelimiter  );
    poCSVLayer->BuildFeatureDefn( pszNfdcRunwaysGeomField,
                                             pszGeonamesGeomFieldPrefix,
                                             papszOpenOptionsIn );
    OGRLayer* poLayer = poCSVLayer;
    if( bUpdate )
    {
        poLayer = new OGRCSVEditableLayer(poCSVLayer, papszOpenOptionsIn);
    }
    papoLayers[nLayers-1] = poLayer;

    return true;
}

/************************************************************************/
/*                           ICreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRCSVDataSource::ICreateLayer( const char *pszLayerName,
                                OGRSpatialReference *poSpatialRef,
                                OGRwkbGeometryType eGType,
                                char ** papszOptions  )
{
/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( !bUpdate )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.\n"
                  "New layer %s cannot be created.",
                  pszName, pszLayerName );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Verify that the datasource is a directory.                      */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if( STARTS_WITH(pszName, "/vsizip/"))
    {
        /* Do nothing */
    }
    else if( !EQUAL(pszName, "/vsistdout/") &&
        (VSIStatL( pszName, &sStatBuf ) != 0
        || !VSI_ISDIR( sStatBuf.st_mode )) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create csv layer (file) against a "
                  "non-directory datasource." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      What filename would we use?                                     */
/* -------------------------------------------------------------------- */
    CPLString osFilename;

    if( osDefaultCSVName != "" )
    {
        osFilename = CPLFormFilename( pszName, osDefaultCSVName, NULL );
        osDefaultCSVName = "";
    }
    else
    {
        osFilename = CPLFormFilename( pszName, pszLayerName, "csv" );
    }

/* -------------------------------------------------------------------- */
/*      Does this directory/file already exist?                         */
/* -------------------------------------------------------------------- */
    if( VSIStatL( osFilename, &sStatBuf ) == 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Attempt to create layer %s, but %s already exists.",
                  pszLayerName, osFilename.c_str() );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the empty file.                                          */
/* -------------------------------------------------------------------- */

    const char *pszDelimiter = CSLFetchNameValue( papszOptions, "SEPARATOR");
    char chDelimiter = ',';
    if (pszDelimiter != NULL)
    {
        if (EQUAL(pszDelimiter, "COMMA"))
            chDelimiter = ',';
        else if (EQUAL(pszDelimiter, "SEMICOLON"))
            chDelimiter = ';';
        else if (EQUAL(pszDelimiter, "TAB"))
            chDelimiter = '\t';
        else if (EQUAL(pszDelimiter, "SPACE"))
            chDelimiter = ' ';
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "SEPARATOR=%s not understood, use one of "
                      "COMMA, SEMICOLON, SPACE or TAB.",
                      pszDelimiter );
        }
    }

/* -------------------------------------------------------------------- */
/*      Create a layer.                                                 */
/* -------------------------------------------------------------------- */

    OGRCSVLayer* poCSVLayer = new OGRCSVLayer( pszLayerName, NULL, osFilename,
                                               true, true, chDelimiter );

    poCSVLayer->BuildFeatureDefn();

/* -------------------------------------------------------------------- */
/*      Was a particular CRLF order requested?                          */
/* -------------------------------------------------------------------- */
    const char *pszCRLFFormat = CSLFetchNameValue( papszOptions, "LINEFORMAT");
    bool bUseCRLF = false;

    if( pszCRLFFormat == NULL )
    {
#ifdef WIN32
        bUseCRLF = true;
#endif
    }
    else if( EQUAL(pszCRLFFormat, "CRLF") )
    {
        bUseCRLF = true;
    }
    else if( EQUAL(pszCRLFFormat, "LF") )
    {
    }
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "LINEFORMAT=%s not understood, use one of CRLF or LF.",
                  pszCRLFFormat );
#ifdef WIN32
        bUseCRLF = true;
#endif
    }

    poCSVLayer->SetCRLF( bUseCRLF );

/* -------------------------------------------------------------------- */
/*      Should we write the geometry ?                                  */
/* -------------------------------------------------------------------- */
    const char *pszGeometry = CSLFetchNameValue( papszOptions, "GEOMETRY");
    if( bEnableGeometryFields )
    {
        poCSVLayer->SetWriteGeometry(eGType, OGR_CSV_GEOM_AS_WKT,
            CSLFetchNameValueDef(papszOptions, "GEOMETRY_NAME", "WKT"));
    }
    else if (pszGeometry != NULL)
    {
        if (EQUAL(pszGeometry, "AS_WKT"))
        {
            poCSVLayer->SetWriteGeometry(eGType, OGR_CSV_GEOM_AS_WKT,
                CSLFetchNameValueDef(papszOptions, "GEOMETRY_NAME", "WKT"));
        }
        else if (EQUAL(pszGeometry, "AS_XYZ") ||
                 EQUAL(pszGeometry, "AS_XY") ||
                 EQUAL(pszGeometry, "AS_YX"))
        {
            if (eGType == wkbUnknown || wkbFlatten(eGType) == wkbPoint)
            {
                poCSVLayer->SetWriteGeometry(
                    eGType,
                    EQUAL(pszGeometry, "AS_XYZ") ? OGR_CSV_GEOM_AS_XYZ :
                    EQUAL(pszGeometry, "AS_XY") ?  OGR_CSV_GEOM_AS_XY :
                    OGR_CSV_GEOM_AS_YX);
            }
            else
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Geometry type %s is not compatible with "
                          "GEOMETRY=AS_XYZ.",
                          OGRGeometryTypeToName(eGType) );
            }
        }
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Unsupported value %s for creation option GEOMETRY",
                       pszGeometry );
        }
    }

/* -------------------------------------------------------------------- */
/*      Should we create a CSVT file ?                                  */
/* -------------------------------------------------------------------- */

    const char *pszCreateCSVT = CSLFetchNameValue( papszOptions, "CREATE_CSVT");
    if (pszCreateCSVT && CPLTestBool(pszCreateCSVT))
    {
        poCSVLayer->SetCreateCSVT(true);

/* -------------------------------------------------------------------- */
/*      Create .prj file                                                */
/* -------------------------------------------------------------------- */

        if( poSpatialRef != NULL && osFilename != "/vsistdout/" )
        {
            char* pszWKT = NULL;
            poSpatialRef->exportToWkt(&pszWKT);
            if( pszWKT )
            {
                VSILFILE* fpPRJ
                    = VSIFOpenL(CPLResetExtension(osFilename, "prj"), "wb");
                if( fpPRJ )
                {
                    CPL_IGNORE_RET_VAL(VSIFPrintfL(fpPRJ, "%s\n", pszWKT));
                    VSIFCloseL(fpPRJ);
                }
                CPLFree(pszWKT);
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Should we write a UTF8 BOM ?                                    */
/* -------------------------------------------------------------------- */

    const char *pszWriteBOM = CSLFetchNameValue( papszOptions, "WRITE_BOM");
    if( pszWriteBOM )
        poCSVLayer->SetWriteBOM(CPLTestBool(pszWriteBOM));

    nLayers++;
    papoLayers = static_cast<OGRLayer **>(
        CPLRealloc( papoLayers, sizeof(void*) * nLayers ) );
    OGRLayer* poLayer = poCSVLayer;
    if( osFilename != "/vsistdout/" )
        poLayer = new OGRCSVEditableLayer(poCSVLayer, NULL);
    papoLayers[nLayers-1] = poLayer;
    return poLayer;
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRCSVDataSource::DeleteLayer( int iLayer )

{
/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if( !bUpdate )
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.\n"
                  "Layer %d cannot be deleted.\n",
                  pszName, iLayer );

        return OGRERR_FAILURE;
    }

    if( iLayer < 0 || iLayer >= nLayers )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Layer %d not in legal range of 0 to %d.",
                  iLayer, nLayers-1 );
        return OGRERR_FAILURE;
    }

    char *pszFilename =
        CPLStrdup(
            CPLFormFilename( pszName,
                             papoLayers[iLayer]->GetLayerDefn()->GetName(),
                             "csv" ) );
    char *pszFilenameCSVT =
        CPLStrdup(
            CPLFormFilename( pszName,
                             papoLayers[iLayer]->GetLayerDefn()->GetName(),
                             "csvt" ) );

    delete papoLayers[iLayer];

    while( iLayer < nLayers - 1 )
    {
        papoLayers[iLayer] = papoLayers[iLayer+1];
        iLayer++;
    }

    nLayers--;

    VSIUnlink( pszFilename );
    CPLFree( pszFilename );
    VSIUnlink( pszFilenameCSVT );
    CPLFree( pszFilenameCSVT );

    return OGRERR_NONE;
}

/************************************************************************/
/*                       CreateForSingleFile()                          */
/************************************************************************/

void OGRCSVDataSource::CreateForSingleFile( const char* pszDirname,
                                            const char *pszFilename )
{
    pszName = CPLStrdup( pszDirname );
    bUpdate = true;
    osDefaultCSVName = CPLGetFilename(pszFilename);
}
