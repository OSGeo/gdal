/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Implements OGRPGDumpLayer class
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2010-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "ogr_pgdump.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "ogr_p.h"

CPL_CVSID("$Id$");

static const int USE_COPY_UNSET = -1;

static CPLString OGRPGDumpEscapeStringList(
    char** papszItems, bool bForInsertOrUpdate,
    OGRPGCommonEscapeStringCbk pfnEscapeString,
    void* userdata );

static CPLString OGRPGDumpEscapeStringWithUserData(
    CPL_UNUSED void* user_data,
    const char* pszStrValue, int nMaxLength,
    CPL_UNUSED const char* pszLayerName,
    const char* pszFieldName )
{
    return OGRPGDumpEscapeString(pszStrValue, nMaxLength, pszFieldName);
}

/************************************************************************/
/*                        OGRPGDumpLayer()                              */
/************************************************************************/

OGRPGDumpLayer::OGRPGDumpLayer( OGRPGDumpDataSource* poDSIn,
                                const char* pszSchemaNameIn,
                                const char* pszTableName,
                                const char *pszFIDColumnIn,
                                int bWriteAsHexIn,
                                int bCreateTableIn ) :
    pszSchemaName(CPLStrdup(pszSchemaNameIn)),
    pszSqlTableName(CPLStrdup(
        CPLString().Printf("%s.%s",
                           OGRPGDumpEscapeColumnName(pszSchemaName).c_str(),
                           OGRPGDumpEscapeColumnName(pszTableName).c_str()))),
    pszFIDColumn(CPLStrdup(pszFIDColumnIn)),
    poFeatureDefn(new OGRFeatureDefn(pszTableName)),
    poDS(poDSIn),
    bLaunderColumnNames(true),
    bPreservePrecision(true),
    bUseCopy(USE_COPY_UNSET),
    bWriteAsHex(CPL_TO_BOOL(bWriteAsHexIn)),
    bCopyActive(false),
    bFIDColumnInCopyFields(false),
    bCreateTable(bCreateTableIn),
    nUnknownSRSId(-1),
    nForcedSRSId(-2),
    nForcedGeometryTypeFlags(-1),
    bCreateSpatialIndexFlag(true),
    nPostGISMajor(1),
    nPostGISMinor(2),
    iNextShapeId(0),
    iFIDAsRegularColumnIndex(-1),
    bAutoFIDOnCreateViaCopy(true),
    bCopyStatementWithFID(false),
    papszOverrideColumnTypes(NULL)
{
    SetDescription( poFeatureDefn->GetName() );
    poFeatureDefn->SetGeomType(wkbNone);
    poFeatureDefn->Reference();
}

/************************************************************************/
/*                          ~OGRPGDumpLayer()                           */
/************************************************************************/

OGRPGDumpLayer::~OGRPGDumpLayer()
{
    EndCopy();

    poFeatureDefn->Release();
    CPLFree(pszSchemaName);
    CPLFree(pszSqlTableName);
    CPLFree(pszFIDColumn);
    CSLDestroy(papszOverrideColumnTypes);
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRPGDumpLayer::GetNextFeature()
{
    CPLError(CE_Failure, CPLE_NotSupported, "PGDump driver is write only");
    return NULL;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

int OGRPGDumpLayer::TestCapability( const char * pszCap )
{
    if( EQUAL(pszCap,OLCSequentialWrite) ||
        EQUAL(pszCap,OLCCreateField) ||
        EQUAL(pszCap,OLCCreateGeomField) ||
        EQUAL(pszCap,OLCCurveGeometries) ||
        EQUAL(pszCap,OLCMeasuredGeometries) )
        return TRUE;
    else
        return FALSE;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRErr OGRPGDumpLayer::ICreateFeature( OGRFeature *poFeature )
{
    if( NULL == poFeature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to CreateFeature()." );
        return OGRERR_FAILURE;
    }

    /* In case the FID column has also been created as a regular field */
    if( iFIDAsRegularColumnIndex >= 0 )
    {
        if( poFeature->GetFID() == OGRNullFID )
        {
            if( poFeature->IsFieldSetAndNotNull( iFIDAsRegularColumnIndex ) )
            {
                poFeature->SetFID(
                    poFeature->GetFieldAsInteger64(iFIDAsRegularColumnIndex));
            }
        }
        else
        {
            if( !poFeature->IsFieldSetAndNotNull( iFIDAsRegularColumnIndex ) ||
                poFeature->GetFieldAsInteger64(iFIDAsRegularColumnIndex) != poFeature->GetFID() )
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Inconsistent values of FID and field of same name");
                return OGRERR_FAILURE;
            }
        }
    }

    if( !poFeature->Validate((OGR_F_VAL_ALL & ~OGR_F_VAL_WIDTH) |
                             OGR_F_VAL_ALLOW_DIFFERENT_GEOM_DIM, TRUE ) )
        return OGRERR_FAILURE;

    // We avoid testing the config option too often.
    if( bUseCopy == USE_COPY_UNSET )
        bUseCopy = CPLTestBool( CPLGetConfigOption( "PG_USE_COPY", "NO") );

    OGRErr eErr;
    if( !bUseCopy )
    {
        eErr = CreateFeatureViaInsert( poFeature );
    }
    else
    {
        // If there's a unset field with a default value, then we must use a
        // specific INSERT statement to avoid unset fields to be bound to NULL.
        bool bHasDefaultValue = false;
        const int nFieldCount = poFeatureDefn->GetFieldCount();
        for( int iField = 0; iField < nFieldCount; iField++ )
        {
            if( !poFeature->IsFieldSetAndNotNull( iField ) &&
                poFeature->GetFieldDefnRef(iField)->GetDefault() != NULL )
            {
                bHasDefaultValue = true;
                break;
            }
        }
        if( bHasDefaultValue )
        {
            EndCopy();
            eErr = CreateFeatureViaInsert( poFeature );
        }
        else
        {
            const bool bFIDSet = poFeature->GetFID() != OGRNullFID;
            if( bCopyActive && bFIDSet != bCopyStatementWithFID )
            {
                EndCopy();
                eErr = CreateFeatureViaInsert( poFeature );
            }
            else
            {
                if ( !bCopyActive )
                {
                    // This is a heuristics. If the first feature to be copied
                    // has a FID set (and that a FID column has been
                    // identified), then we will try to copy FID values from
                    // features. Otherwise, we will not do and assume that the
                    // FID column is an autoincremented column.
                    StartCopy(bFIDSet);
                    bCopyStatementWithFID = bFIDSet;
                }

                eErr = CreateFeatureViaCopy( poFeature );
                if( bFIDSet )
                    bAutoFIDOnCreateViaCopy = false;
                if( eErr == OGRERR_NONE && bAutoFIDOnCreateViaCopy )
                {
                    poFeature->SetFID( ++iNextShapeId );
                }
            }
        }
    }

    if( eErr == OGRERR_NONE && iFIDAsRegularColumnIndex >= 0 )
    {
        poFeature->SetField(iFIDAsRegularColumnIndex, poFeature->GetFID());
    }
    return eErr;
}

/************************************************************************/
/*                       CreateFeatureViaInsert()                       */
/************************************************************************/

OGRErr OGRPGDumpLayer::CreateFeatureViaInsert( OGRFeature *poFeature )

{
    OGRErr eErr = OGRERR_FAILURE;

    if( NULL == poFeature )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "NULL pointer to OGRFeature passed to CreateFeatureViaInsert()." );
        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      Form the INSERT command.                                        */
/* -------------------------------------------------------------------- */
    CPLString osCommand;
    osCommand.Printf( "INSERT INTO %s (", pszSqlTableName );

    bool bNeedComma = false;

    for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        OGRGeometry *poGeom = poFeature->GetGeomFieldRef(i);
        if( poGeom != NULL )
        {
            if( bNeedComma )
                osCommand += ", ";

            OGRGeomFieldDefn* poGFldDefn = poFeature->GetGeomFieldDefnRef(i);
            osCommand = osCommand + OGRPGDumpEscapeColumnName(poGFldDefn->GetNameRef()) + " ";
            bNeedComma = true;
        }
    }

    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL )
    {
        if( bNeedComma )
            osCommand += ", ";

        osCommand = osCommand + OGRPGDumpEscapeColumnName(pszFIDColumn) + " ";
        bNeedComma = true;
    }

    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( i == iFIDAsRegularColumnIndex )
            continue;
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( !bNeedComma )
            bNeedComma = true;
        else
            osCommand += ", ";

        osCommand = osCommand
            + OGRPGDumpEscapeColumnName(poFeatureDefn->GetFieldDefn(i)->GetNameRef());
    }

    const bool bEmptyInsert = !bNeedComma;

    osCommand += ") VALUES (";

    /* Set the geometry */
    bNeedComma = false;
    for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        OGRGeometry *poGeom = poFeature->GetGeomFieldRef(i);
        if( poGeom != NULL )
        {
            char *pszWKT = NULL;

            OGRPGDumpGeomFieldDefn* poGFldDefn =
                (OGRPGDumpGeomFieldDefn*) poFeature->GetGeomFieldDefnRef(i);

            poGeom->closeRings();
            poGeom->set3D(poGFldDefn->GeometryTypeFlags & OGRGeometry::OGR_G_3D);
            poGeom->setMeasured(poGFldDefn->GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED);

            if( bNeedComma )
                osCommand += ", ";

            if( bWriteAsHex )
            {
                char* pszHex = OGRGeometryToHexEWKB( poGeom, poGFldDefn->nSRSId,
                                                     nPostGISMajor,
                                                     nPostGISMinor );
                osCommand += "'";
                if (pszHex)
                    osCommand += pszHex;
                osCommand += "'";
                CPLFree(pszHex);
            }
            else
            {
                poGeom->exportToWkt( &pszWKT );

                if( pszWKT != NULL )
                {
                    osCommand +=
                        CPLString().Printf(
                            "GeomFromEWKT('SRID=%d;%s'::TEXT) ", poGFldDefn->nSRSId, pszWKT );
                    CPLFree( pszWKT );
                }
                else
                    osCommand += "''";
            }

            bNeedComma = true;
        }
    }

    /* Set the FID */
    if( poFeature->GetFID() != OGRNullFID && pszFIDColumn != NULL )
    {
        if( bNeedComma )
            osCommand += ", ";
        osCommand += CPLString().Printf( CPL_FRMT_GIB, poFeature->GetFID() );
        bNeedComma = true;
    }

    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if( i == iFIDAsRegularColumnIndex )
            continue;
        if( !poFeature->IsFieldSet( i ) )
            continue;

        if( bNeedComma )
            osCommand += ", ";
        else
            bNeedComma = true;

        OGRPGCommonAppendFieldValue(osCommand, poFeature, i,
                                    OGRPGDumpEscapeStringWithUserData, NULL);
    }

    osCommand += ")";

    if( bEmptyInsert )
        osCommand.Printf( "INSERT INTO %s DEFAULT VALUES", pszSqlTableName );

/* -------------------------------------------------------------------- */
/*      Execute the insert.                                             */
/* -------------------------------------------------------------------- */
    poDS->Log(osCommand);

    if( poFeature->GetFID() == OGRNullFID )
        poFeature->SetFID( ++iNextShapeId );

    return OGRERR_NONE;
}

/************************************************************************/
/*                        CreateFeatureViaCopy()                        */
/************************************************************************/

OGRErr OGRPGDumpLayer::CreateFeatureViaCopy( OGRFeature *poFeature )
{
    CPLString            osCommand;

    /* First process geometry */
    for( int i = 0; i < poFeature->GetGeomFieldCount(); i++ )
    {
        OGRGeometry *poGeometry = poFeature->GetGeomFieldRef(i);
        char *pszGeom = NULL;
        if ( NULL != poGeometry /* && (bHasWkb || bHasPostGISGeometry || bHasPostGISGeography) */)
        {
            OGRPGDumpGeomFieldDefn* poGFldDefn =
                (OGRPGDumpGeomFieldDefn*) poFeature->GetGeomFieldDefnRef(i);

            poGeometry->closeRings();
            poGeometry->set3D(poGFldDefn->GeometryTypeFlags & OGRGeometry::OGR_G_3D);
            poGeometry->setMeasured(poGFldDefn->GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED);

            //CheckGeomTypeCompatibility(poGeometry);

            /*if (bHasWkb)
                pszGeom = GeometryToBYTEA( poGeometry );
            else*/
                pszGeom = OGRGeometryToHexEWKB( poGeometry, poGFldDefn->nSRSId,
                                                nPostGISMajor,
                                                nPostGISMinor );
        }

        if (!osCommand.empty())
            osCommand += "\t";
        if ( pszGeom )
        {
            osCommand += pszGeom;
            CPLFree( pszGeom );
        }
        else
        {
            osCommand += "\\N";
        }
    }

    OGRPGCommonAppendCopyFieldsExceptGeom(osCommand,
                                          poFeature,
                                          pszFIDColumn,
                                          bFIDColumnInCopyFields,
                                          OGRPGDumpEscapeStringWithUserData,
                                          NULL);

    /* Add end of line marker */
    // osCommand += "\n";

    /* ------------------------------------------------------------ */
    /*      Execute the copy.                                       */
    /* ------------------------------------------------------------ */

    OGRErr result = OGRERR_NONE;

    poDS->Log(osCommand, false);

    return result;
}

/************************************************************************/
/*                OGRPGCommonAppendCopyFieldsExceptGeom()               */
/************************************************************************/

void OGRPGCommonAppendCopyFieldsExceptGeom(
    CPLString& osCommand,
    OGRFeature* poFeature,
    const char* pszFIDColumn,
    bool bFIDColumnInCopyFields,
    OGRPGCommonEscapeStringCbk pfnEscapeString,
    void* userdata )
{
    OGRFeatureDefn* poFeatureDefn = poFeature->GetDefnRef();

    /* Next process the field id column */
    int nFIDIndex = -1;
    if( bFIDColumnInCopyFields )
    {
        if (!osCommand.empty())
            osCommand += "\t";

        nFIDIndex = poFeatureDefn->GetFieldIndex( pszFIDColumn );

        /* Set the FID */
        if( poFeature->GetFID() != OGRNullFID )
        {
            osCommand += CPLString().Printf( CPL_FRMT_GIB, poFeature->GetFID());
        }
        else
        {
            osCommand += "\\N" ;
        }
    }

    /* Now process the remaining fields */

    int nFieldCount = poFeatureDefn->GetFieldCount();
    bool bAddTab = !osCommand.empty();

    for( int i = 0; i < nFieldCount;  i++ )
    {
        if (i == nFIDIndex)
            continue;

        const char *pszStrValue = poFeature->GetFieldAsString(i);
        char *pszNeedToFree = NULL;

        if( bAddTab )
            osCommand += "\t";
        bAddTab = true;

        if( !poFeature->IsFieldSetAndNotNull( i ) )
        {
            osCommand += "\\N" ;

            continue;
        }

        const int nOGRFieldType = poFeatureDefn->GetFieldDefn(i)->GetType();

        // We need special formatting for integer list values.
        if( nOGRFieldType == OFTIntegerList )
        {
            int nCount, nOff = 0;
            const int *panItems = poFeature->GetFieldAsIntegerList(i,&nCount);

            const size_t nLen = nCount * 13 + 10;
            pszNeedToFree = (char *) CPLMalloc(nLen);
            strcpy( pszNeedToFree, "{" );
            for( int j = 0; j < nCount; j++ )
            {
                if( j != 0 )
                    strcat( pszNeedToFree+nOff, "," );

                nOff += static_cast<int>(strlen(pszNeedToFree+nOff));
                snprintf( pszNeedToFree+nOff, nLen-nOff, "%d", panItems[j] );
            }
            strcat( pszNeedToFree+nOff, "}" );
            pszStrValue = pszNeedToFree;
        }

        else if( nOGRFieldType == OFTInteger64List )
        {
            int nCount, nOff = 0;
            const GIntBig *panItems = poFeature->GetFieldAsInteger64List(i,&nCount);

            const size_t nLen = nCount * 26 + 10;
            pszNeedToFree = (char *) CPLMalloc(nLen);
            strcpy( pszNeedToFree, "{" );
            for( int j = 0; j < nCount; j++ )
            {
                if( j != 0 )
                    strcat( pszNeedToFree+nOff, "," );

                nOff += static_cast<int>(strlen(pszNeedToFree+nOff));
                snprintf( pszNeedToFree+nOff, nLen-nOff, CPL_FRMT_GIB, panItems[j] );
            }
            strcat( pszNeedToFree+nOff, "}" );
            pszStrValue = pszNeedToFree;
        }

        // We need special formatting for real list values.
        else if( nOGRFieldType == OFTRealList )
        {
            int nOff = 0;
            int nCount = 0;
            const double *padfItems =
                poFeature->GetFieldAsDoubleList(i,&nCount);

            const size_t nLen = nCount * 40 + 10;
            pszNeedToFree = (char *) CPLMalloc(nLen);
            strcpy( pszNeedToFree, "{" );
            for( int j = 0; j < nCount; j++ )
            {
                if( j != 0 )
                    strcat( pszNeedToFree+nOff, "," );

                nOff += static_cast<int>(strlen(pszNeedToFree+nOff));
                //Check for special values. They need to be quoted.
                if( CPLIsNan(padfItems[j]) )
                    snprintf( pszNeedToFree+nOff, nLen-nOff, "NaN" );
                else if( CPLIsInf(padfItems[j]) )
                    snprintf( pszNeedToFree+nOff, nLen-nOff, (padfItems[j] > 0) ? "Infinity" : "-Infinity" );
                else
                    CPLsnprintf( pszNeedToFree+nOff, nLen-nOff, "%.16g", padfItems[j] );
            }
            strcat( pszNeedToFree+nOff, "}" );
            pszStrValue = pszNeedToFree;
        }

        // We need special formatting for string list values.
        else if( nOGRFieldType == OFTStringList )
        {
            CPLString osStr;
            char **papszItems = poFeature->GetFieldAsStringList(i);

            pszStrValue = pszNeedToFree = CPLStrdup(
                OGRPGDumpEscapeStringList(papszItems, false,
                                          pfnEscapeString, userdata));
        }

        // Binary formatting
        else if( nOGRFieldType == OFTBinary )
        {
            int nLen = 0;
            GByte* pabyData = poFeature->GetFieldAsBinary( i, &nLen );
            char* pszBytea = OGRPGDumpLayer::GByteArrayToBYTEA( pabyData, nLen);

            pszStrValue = pszNeedToFree = pszBytea;
        }

        else if( nOGRFieldType == OFTReal )
        {
            //Check for special values. They need to be quoted.
            double dfVal = poFeature->GetFieldAsDouble(i);
            if( CPLIsNan(dfVal) )
                pszStrValue = "NaN";
            else if( CPLIsInf(dfVal) )
                pszStrValue = (dfVal > 0) ? "Infinity" : "-Infinity";
        }

        if( nOGRFieldType != OFTIntegerList &&
            nOGRFieldType != OFTInteger64List &&
            nOGRFieldType != OFTRealList &&
            nOGRFieldType != OFTInteger &&
            nOGRFieldType != OFTInteger64 &&
            nOGRFieldType != OFTReal &&
            nOGRFieldType != OFTBinary )
        {
            int iUTFChar = 0;
            const int nMaxWidth = poFeatureDefn->GetFieldDefn(i)->GetWidth();

            for( int iChar = 0; pszStrValue[iChar] != '\0'; iChar++ )
            {
                //count of utf chars
                if (nOGRFieldType != OFTStringList && (pszStrValue[iChar] & 0xc0) != 0x80)
                {
                    if( nMaxWidth > 0 && iUTFChar == nMaxWidth )
                    {
                        CPLDebug( "PG",
                                "Truncated %s field value, it was too long.",
                                poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
                        break;
                    }
                    iUTFChar++;
                }

                /* Escape embedded \, \t, \n, \r since they will cause COPY
                   to misinterpret a line of text and thus abort */
                if( pszStrValue[iChar] == '\\' ||
                    pszStrValue[iChar] == '\t' ||
                    pszStrValue[iChar] == '\r' ||
                    pszStrValue[iChar] == '\n'   )
                {
                    osCommand += '\\';
                }

                osCommand += pszStrValue[iChar];
            }
        }
        else
        {
            osCommand += pszStrValue;
        }

        if( pszNeedToFree )
            CPLFree( pszNeedToFree );
    }
}

/************************************************************************/
/*                             StartCopy()                              */
/************************************************************************/

OGRErr OGRPGDumpLayer::StartCopy( int bSetFID )

{
    /* Tell the datasource we are now planning to copy data */
    poDS->StartCopy( this );

    CPLString osFields = BuildCopyFields(bSetFID);

    size_t size = osFields.size() +  strlen(pszSqlTableName) + 100;
    char *pszCommand = (char *) CPLMalloc(size);

    snprintf( pszCommand, size,
             "COPY %s (%s) FROM STDIN",
             pszSqlTableName, osFields.c_str() );

    poDS->Log(pszCommand);
    bCopyActive = true;

    CPLFree( pszCommand );

    return OGRERR_NONE;
}

/************************************************************************/
/*                              EndCopy()                               */
/************************************************************************/

OGRErr OGRPGDumpLayer::EndCopy()

{
    if( !bCopyActive )
        return OGRERR_NONE;

    bCopyActive = false;

    poDS->Log("\\.", false);
    poDS->Log("END");

    bUseCopy = USE_COPY_UNSET;

    return OGRERR_NONE;
}

/************************************************************************/
/*                          BuildCopyFields()                           */
/************************************************************************/

CPLString OGRPGDumpLayer::BuildCopyFields( int bSetFID )
{
    CPLString osFieldList;

    for( int i = 0; i < poFeatureDefn->GetGeomFieldCount(); i++ )
    {
        if( !osFieldList.empty() )
            osFieldList += ", ";

        OGRGeomFieldDefn* poGFldDefn = poFeatureDefn->GetGeomFieldDefn(i);

        osFieldList += OGRPGDumpEscapeColumnName(poGFldDefn->GetNameRef());
    }

    int nFIDIndex = -1;
    bFIDColumnInCopyFields = pszFIDColumn != NULL && bSetFID;
    if( bFIDColumnInCopyFields )
    {
        if( !osFieldList.empty() )
            osFieldList += ", ";

        nFIDIndex = poFeatureDefn->GetFieldIndex( pszFIDColumn );

        osFieldList += OGRPGDumpEscapeColumnName(pszFIDColumn);
    }

    for( int i = 0; i < poFeatureDefn->GetFieldCount(); i++ )
    {
        if (i == nFIDIndex)
            continue;

        const char *pszName = poFeatureDefn->GetFieldDefn(i)->GetNameRef();

       if( !osFieldList.empty() )
            osFieldList += ", ";

        osFieldList += OGRPGDumpEscapeColumnName(pszName);
    }

    return osFieldList;
}

/************************************************************************/
/*                       OGRPGDumpEscapeColumnName( )                   */
/************************************************************************/

CPLString OGRPGDumpEscapeColumnName(const char* pszColumnName)
{
    CPLString osStr = "\"";

    char ch = '\0';
    for( int i = 0; (ch = pszColumnName[i]) != '\0'; i++ )
    {
        if (ch == '"')
            osStr.append(1, ch);
        osStr.append(1, ch);
    }

    osStr += "\"";

    return osStr;
}

/************************************************************************/
/*                             EscapeString( )                          */
/************************************************************************/

CPLString OGRPGDumpEscapeString( const char* pszStrValue, int nMaxLength,
                                 const char* pszFieldName )
{
    CPLString osCommand;

    /* We need to quote and escape string fields. */
    osCommand += "'";

    int nSrcLen = static_cast<int>(strlen(pszStrValue));
    const int nSrcLenUTF = CPLStrlenUTF8(pszStrValue);

    if (nMaxLength > 0 && nSrcLenUTF > nMaxLength)
    {
        CPLDebug( "PG",
                  "Truncated %s field value, it was too long.",
                  pszFieldName );

        int iUTF8Char = 0;
        for(int iChar = 0; iChar < nSrcLen; iChar++ )
        {
            if( (((unsigned char *) pszStrValue)[iChar] & 0xc0) != 0x80 )
            {
                if( iUTF8Char == nMaxLength )
                {
                    nSrcLen = iChar;
                    break;
                }
                iUTF8Char ++;
            }
        }
    }

    char* pszDestStr = (char*)CPLMalloc(2 * nSrcLen + 1);

    /* -------------------------------------------------------------------- */
    /*  PQescapeStringConn was introduced in PostgreSQL security releases   */
    /*  8.1.4, 8.0.8, 7.4.13, 7.3.15                                        */
    /*  PG_HAS_PQESCAPESTRINGCONN is added by a test in 'configure'         */
    /*  so it is not set by default when building OGR for Win32             */
    /* -------------------------------------------------------------------- */
#if defined(PG_HAS_PQESCAPESTRINGCONN)
    int nError = 0;
    PQescapeStringConn (hPGConn, pszDestStr, pszStrValue, nSrcLen, &nError);
    if (nError == 0)
        osCommand += pszDestStr;
    else
        CPLError(CE_Warning, CPLE_AppDefined,
                 "PQescapeString(): %s\n"
                 "  input: '%s'\n"
                 "    got: '%s'\n",
                 PQerrorMessage( hPGConn ),
                 pszStrValue, pszDestStr );
#else
    //PQescapeString(pszDestStr, pszStrValue, nSrcLen);

    int j = 0;  // Used after for.
    for( int i = 0; i < nSrcLen; i++)
    {
        if (pszStrValue[i] == '\'')
        {
            pszDestStr[j++] = '\'';
            pszDestStr[j++] = '\'';
        }
        // FIXME: at some point (when we drop PostgreSQL < 9.1 support, remove
        // the escaping of backslash and remove
        //   'SET standard_conforming_strings = OFF'
        //  in ICreateLayer().
        else if (pszStrValue[i] == '\\')
        {
            pszDestStr[j++] = '\\';
            pszDestStr[j++] = '\\';
        }
        else
        {
            pszDestStr[j++] = pszStrValue[i];
        }
    }
    pszDestStr[j] = 0;

    osCommand += pszDestStr;
#endif
    CPLFree(pszDestStr);

    osCommand += "'";

    return osCommand;
}

/************************************************************************/
/*                    OGRPGDumpEscapeStringList( )                      */
/************************************************************************/

static CPLString OGRPGDumpEscapeStringList(
    char** papszItems, bool bForInsertOrUpdate,
    OGRPGCommonEscapeStringCbk pfnEscapeString,
    void* userdata)
{
    bool bFirstItem = true;
    CPLString osStr;
    if (bForInsertOrUpdate)
        osStr += "ARRAY[";
    else
        osStr += "{";
    while(papszItems && *papszItems)
    {
        if( !bFirstItem )
        {
            osStr += ',';
        }

        char* pszStr = *papszItems;
        if (*pszStr != '\0')
        {
            if (bForInsertOrUpdate)
                osStr += pfnEscapeString(userdata, pszStr, 0, "", "");
            else
            {
                osStr += '"';

                while(*pszStr)
                {
                    if (*pszStr == '"' )
                        osStr += "\\";
                    osStr += *pszStr;
                    pszStr++;
                }

                osStr += '"';
            }
        }
        else
            osStr += "NULL";

        bFirstItem = false;

        papszItems++;
    }
    if (bForInsertOrUpdate)
    {
        osStr += "]";
        if( papszItems == NULL )
            osStr += "::varchar[]";
    }
    else
        osStr += "}";
    return osStr;
}

/************************************************************************/
/*                          AppendFieldValue()                          */
/*                                                                      */
/* Used by CreateFeatureViaInsert() and SetFeature() to format a        */
/* non-empty field value                                                */
/************************************************************************/

void OGRPGCommonAppendFieldValue(CPLString& osCommand,
                                 OGRFeature* poFeature, int i,
                                 OGRPGCommonEscapeStringCbk pfnEscapeString,
                                 void* userdata)
{
    if( poFeature->IsFieldNull(i) )
    {
        osCommand += "NULL";
        return;
    }

    OGRFeatureDefn* poFeatureDefn = poFeature->GetDefnRef();
    OGRFieldType nOGRFieldType = poFeatureDefn->GetFieldDefn(i)->GetType();
    OGRFieldSubType eSubType = poFeatureDefn->GetFieldDefn(i)->GetSubType();

    // We need special formatting for integer list values.
    if(  nOGRFieldType == OFTIntegerList )
    {
        int nCount, nOff = 0, j;
        const int *panItems = poFeature->GetFieldAsIntegerList(i,&nCount);
        char *pszNeedToFree = NULL;

        const size_t nLen = nCount * 13 + 10;
        pszNeedToFree = (char *) CPLMalloc(nLen);
        strcpy( pszNeedToFree, "'{" );
        for( j = 0; j < nCount; j++ )
        {
            if( j != 0 )
                strcat( pszNeedToFree+nOff, "," );

            nOff += static_cast<int>(strlen(pszNeedToFree+nOff));
            snprintf( pszNeedToFree+nOff, nLen-nOff, "%d", panItems[j] );
        }
        strcat( pszNeedToFree+nOff, "}'" );

        osCommand += pszNeedToFree;
        CPLFree(pszNeedToFree);

        return;
    }

    else if(  nOGRFieldType == OFTInteger64List )
    {
        int nCount, nOff = 0, j;
        const GIntBig *panItems = poFeature->GetFieldAsInteger64List(i,&nCount);
        char *pszNeedToFree = NULL;

        const size_t nLen = nCount * 26 + 10;
        pszNeedToFree = (char *) CPLMalloc(nLen);
        strcpy( pszNeedToFree, "'{" );
        for( j = 0; j < nCount; j++ )
        {
            if( j != 0 )
                strcat( pszNeedToFree+nOff, "," );

            nOff += static_cast<int>(strlen(pszNeedToFree+nOff));
            snprintf( pszNeedToFree+nOff, nLen-nOff, CPL_FRMT_GIB, panItems[j] );
        }
        strcat( pszNeedToFree+nOff, "}'" );

        osCommand += pszNeedToFree;
        CPLFree(pszNeedToFree);

        return;
    }

    // We need special formatting for real list values.
    else if( nOGRFieldType == OFTRealList )
    {
        int nCount = 0;
        int nOff = 0;
        const double *padfItems = poFeature->GetFieldAsDoubleList(i,&nCount);
        char *pszNeedToFree = NULL;

        const size_t nLen = nCount * 40 + 10;
        pszNeedToFree = (char *) CPLMalloc(nLen);
        strcpy( pszNeedToFree, "'{" );
        for( int j = 0; j < nCount; j++ )
        {
            if( j != 0 )
                strcat( pszNeedToFree+nOff, "," );

            nOff += static_cast<int>(strlen(pszNeedToFree+nOff));
            //Check for special values. They need to be quoted.
            if( CPLIsNan(padfItems[j]) )
                snprintf( pszNeedToFree+nOff, nLen-nOff, "NaN" );
            else if( CPLIsInf(padfItems[j]) )
                snprintf( pszNeedToFree+nOff, nLen-nOff, (padfItems[j] > 0) ? "Infinity" : "-Infinity" );
            else
                CPLsnprintf( pszNeedToFree+nOff, nLen-nOff, "%.16g", padfItems[j] );
        }
        strcat( pszNeedToFree+nOff, "}'" );

        osCommand += pszNeedToFree;
        CPLFree(pszNeedToFree);

        return;
    }

    // We need special formatting for string list values.
    else if( nOGRFieldType == OFTStringList )
    {
        char **papszItems = poFeature->GetFieldAsStringList(i);

        osCommand += OGRPGDumpEscapeStringList(papszItems, true,
                                               pfnEscapeString, userdata);

        return;
    }

    // Binary formatting
    else if( nOGRFieldType == OFTBinary )
    {
        osCommand += "E'";

        int nLen = 0;
        GByte* pabyData = poFeature->GetFieldAsBinary( i, &nLen );
        char* pszBytea = OGRPGDumpLayer::GByteArrayToBYTEA( pabyData, nLen);

        osCommand += pszBytea;

        CPLFree(pszBytea);
        osCommand += "'";

        return;
    }

    // Flag indicating NULL or not-a-date date value
    // e.g. 0000-00-00 - there is no year 0
    bool bIsDateNull = false;

    const char *pszStrValue = poFeature->GetFieldAsString(i);

    // Check if date is NULL: 0000-00-00
    if( nOGRFieldType == OFTDate )
    {
        if( STARTS_WITH_CI(pszStrValue, "0000") )
        {
            pszStrValue = "NULL";
            bIsDateNull = true;
        }
    }
    else if ( nOGRFieldType == OFTReal )
    {
        //Check for special values. They need to be quoted.
        double dfVal = poFeature->GetFieldAsDouble(i);
        if( CPLIsNan(dfVal) )
            pszStrValue = "'NaN'";
        else if( CPLIsInf(dfVal) )
            pszStrValue = (dfVal > 0) ? "'Infinity'" : "'-Infinity'";
    }
    else if ( (nOGRFieldType == OFTInteger ||
               nOGRFieldType == OFTInteger64) && eSubType == OFSTBoolean )
        pszStrValue = poFeature->GetFieldAsInteger(i) ? "'t'" : "'f'";

    if( nOGRFieldType != OFTInteger && nOGRFieldType != OFTInteger64 &&
        nOGRFieldType != OFTReal && nOGRFieldType != OFTStringList
        && !bIsDateNull )
    {
        osCommand += pfnEscapeString( userdata, pszStrValue,
                                      poFeatureDefn->GetFieldDefn(i)->GetWidth(),
                                      poFeatureDefn->GetName(),
                                      poFeatureDefn->GetFieldDefn(i)->GetNameRef() );
    }
    else
    {
        osCommand += pszStrValue;
    }
}

/************************************************************************/
/*                        GByteArrayToBYTEA()                           */
/************************************************************************/

char* OGRPGDumpLayer::GByteArrayToBYTEA( const GByte* pabyData, int nLen)
{
    const size_t nTextBufLen = nLen * 5 + 1;
    char* pszTextBuf;
    pszTextBuf = (char *) CPLMalloc(nTextBufLen);

    int iDst = 0;

    for( int iSrc = 0; iSrc < nLen; iSrc++ )
    {
        if( pabyData[iSrc] < 40 || pabyData[iSrc] > 126
            || pabyData[iSrc] == '\\' )
        {
            snprintf( pszTextBuf+iDst, nTextBufLen - iDst, "\\\\%03o", pabyData[iSrc] );
            iDst += 5;
        }
        else
            pszTextBuf[iDst++] = pabyData[iSrc];
    }
    pszTextBuf[iDst] = '\0';

    return pszTextBuf;
}

/************************************************************************/
/*                       OGRPGCommonLayerGetType()                      */
/************************************************************************/

CPLString OGRPGCommonLayerGetType( OGRFieldDefn& oField,
                                   bool bPreservePrecision,
                                   bool bApproxOK )
{
    char szFieldType[256];

/* -------------------------------------------------------------------- */
/*      Work out the PostgreSQL type.                                   */
/* -------------------------------------------------------------------- */
    if( oField.GetType() == OFTInteger )
    {
        if( oField.GetSubType() == OFSTBoolean )
            strcpy( szFieldType, "BOOLEAN" );
        else if( oField.GetSubType() == OFSTInt16 )
            strcpy( szFieldType, "SMALLINT" );
        else if( oField.GetWidth() > 0 && bPreservePrecision )
            snprintf( szFieldType, sizeof(szFieldType), "NUMERIC(%d,0)", oField.GetWidth() );
        else
            strcpy( szFieldType, "INTEGER" );
    }
    else if( oField.GetType() == OFTInteger64 )
    {
        if( oField.GetWidth() > 0 && bPreservePrecision )
            snprintf( szFieldType, sizeof(szFieldType), "NUMERIC(%d,0)", oField.GetWidth() );
        else
            strcpy( szFieldType, "INT8" );
    }
    else if( oField.GetType() == OFTReal )
    {
        if( oField.GetSubType() == OFSTFloat32 )
            strcpy( szFieldType, "REAL" );
        else if( oField.GetWidth() > 0 &&
                 oField.GetPrecision() > 0 &&
                 bPreservePrecision )
            snprintf( szFieldType, sizeof(szFieldType), "NUMERIC(%d,%d)",
                     oField.GetWidth(), oField.GetPrecision() );
        else
            strcpy( szFieldType, "FLOAT8" );
    }
    else if( oField.GetType() == OFTString )
    {
        if (oField.GetWidth() > 0 &&  bPreservePrecision )
            snprintf( szFieldType, sizeof(szFieldType), "VARCHAR(%d)",  oField.GetWidth() );
        else
            strcpy( szFieldType, "VARCHAR");
    }
    else if( oField.GetType() == OFTIntegerList )
    {
        if( oField.GetSubType() == OFSTBoolean )
            strcpy( szFieldType, "BOOLEAN[]" );
        else if( oField.GetSubType() == OFSTInt16 )
            strcpy( szFieldType, "INT2[]" );
        else
            strcpy( szFieldType, "INTEGER[]" );
    }
    else if( oField.GetType() == OFTInteger64List )
    {
        strcpy( szFieldType, "INT8[]" );
    }
    else if( oField.GetType() == OFTRealList )
    {
        if( oField.GetSubType() == OFSTFloat32 )
            strcpy( szFieldType, "REAL[]" );
        else
            strcpy( szFieldType, "FLOAT8[]" );
    }
    else if( oField.GetType() == OFTStringList )
    {
        strcpy( szFieldType, "varchar[]" );
    }
    else if( oField.GetType() == OFTDate )
    {
        strcpy( szFieldType, "date" );
    }
    else if( oField.GetType() == OFTTime )
    {
        strcpy( szFieldType, "time" );
    }
    else if( oField.GetType() == OFTDateTime )
    {
        strcpy( szFieldType, "timestamp with time zone" );
    }
    else if( oField.GetType() == OFTBinary )
    {
        strcpy( szFieldType, "bytea" );
    }
    else if( bApproxOK )
    {
        CPLError( CE_Warning, CPLE_NotSupported,
                  "Can't create field %s with type %s on PostgreSQL layers.  Creating as VARCHAR.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );
        strcpy( szFieldType, "VARCHAR" );
    }
    else
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "Can't create field %s with type %s on PostgreSQL layers.",
                  oField.GetNameRef(),
                  OGRFieldDefn::GetFieldTypeName(oField.GetType()) );
        strcpy( szFieldType, "");
    }

    return szFieldType;
}

/************************************************************************/
/*                         OGRPGCommonLayerSetType()                    */
/************************************************************************/

bool OGRPGCommonLayerSetType( OGRFieldDefn& oField,
                              const char* pszType,
                              const char* pszFormatType,
                              int nWidth )
{
    if( EQUAL(pszType,"text") )
    {
        oField.SetType( OFTString );
    }
    else if( EQUAL(pszType,"_bpchar") ||
            EQUAL(pszType,"_varchar") ||
            EQUAL(pszType,"_text"))
    {
        oField.SetType( OFTStringList );
    }
    else if( EQUAL(pszType,"bpchar") || EQUAL(pszType,"varchar") )
    {
        if( nWidth == -1 )
        {
            if( STARTS_WITH_CI(pszFormatType, "character(") )
                nWidth = atoi(pszFormatType+10);
            else if( STARTS_WITH_CI(pszFormatType, "character varying(") )
                nWidth = atoi(pszFormatType+18);
            else
                nWidth = 0;
        }
        oField.SetType( OFTString );
        oField.SetWidth( nWidth );
    }
    else if( EQUAL(pszType,"bool") )
    {
        oField.SetType( OFTInteger );
        oField.SetSubType( OFSTBoolean );
        oField.SetWidth( 1 );
    }
    else if( EQUAL(pszType,"_numeric") )
    {
        if( EQUAL(pszFormatType, "numeric[]") )
            oField.SetType( OFTRealList );
        else
        {
            const char *pszPrecision = strstr(pszFormatType,",");
            int    nPrecision = 0;

            nWidth = atoi(pszFormatType + 8);
            if( pszPrecision != NULL )
                nPrecision = atoi(pszPrecision+1);

            if( nPrecision == 0 )
            {
                if( nWidth >= 10 )
                    oField.SetType( OFTInteger64List );
                else
                    oField.SetType( OFTIntegerList );
            }
            else
                oField.SetType( OFTRealList );

            oField.SetWidth( nWidth );
            oField.SetPrecision( nPrecision );
        }
    }
    else if( EQUAL(pszType,"numeric") )
    {
        if( EQUAL(pszFormatType, "numeric") )
            oField.SetType( OFTReal );
        else
        {
            const char *pszPrecision = strstr(pszFormatType,",");
            int    nPrecision = 0;

            nWidth = atoi(pszFormatType + 8);
            if( pszPrecision != NULL )
                nPrecision = atoi(pszPrecision+1);

            if( nPrecision == 0 )
            {
                if( nWidth >= 10 )
                    oField.SetType( OFTInteger64 );
                else
                    oField.SetType( OFTInteger );
            }
            else
                oField.SetType( OFTReal );

            oField.SetWidth( nWidth );
            oField.SetPrecision( nPrecision );
        }
    }
    else if( EQUAL(pszFormatType,"integer[]") )
    {
        oField.SetType( OFTIntegerList );
    }
    else if( EQUAL(pszFormatType,"smallint[]") )
    {
        oField.SetType( OFTIntegerList );
        oField.SetSubType( OFSTInt16 );
    }
    else if( EQUAL(pszFormatType,"boolean[]") )
    {
        oField.SetType( OFTIntegerList );
        oField.SetSubType( OFSTBoolean );
    }
    else if( EQUAL(pszFormatType, "float[]") ||
            EQUAL(pszFormatType, "real[]") )
    {
        oField.SetType( OFTRealList );
        oField.SetSubType( OFSTFloat32 );
    }
    else if( EQUAL(pszFormatType, "double precision[]") )
    {
        oField.SetType( OFTRealList );
    }
    else if( EQUAL(pszType,"int2") )
    {
        oField.SetType( OFTInteger );
        oField.SetSubType( OFSTInt16 );
        oField.SetWidth( 5 );
    }
    else if( EQUAL(pszType,"int8") )
    {
        oField.SetType( OFTInteger64 );
    }
    else if( EQUAL(pszFormatType,"bigint[]") )
    {
        oField.SetType( OFTInteger64List );
    }
    else if( STARTS_WITH_CI(pszType, "int") )
    {
        oField.SetType( OFTInteger );
    }
    else if( EQUAL(pszType,"float4")  )
    {
        oField.SetType( OFTReal );
        oField.SetSubType( OFSTFloat32 );
    }
    else if( STARTS_WITH_CI(pszType, "float") ||
            STARTS_WITH_CI(pszType, "double") ||
            EQUAL(pszType,"real") )
    {
        oField.SetType( OFTReal );
    }
    else if( STARTS_WITH_CI(pszType, "timestamp") )
    {
        oField.SetType( OFTDateTime );
    }
    else if( STARTS_WITH_CI(pszType, "date") )
    {
        oField.SetType( OFTDate );
    }
    else if( STARTS_WITH_CI(pszType, "time") )
    {
        oField.SetType( OFTTime );
    }
    else if( EQUAL(pszType,"bytea") )
    {
        oField.SetType( OFTBinary );
    }
    else
    {
        CPLDebug( "PGCommon", "Field %s is of unknown format type %s (type=%s).",
                oField.GetNameRef(), pszFormatType, pszType );
        return false;
    }
    return true;
}

/************************************************************************/
/*                  OGRPGCommonLayerNormalizeDefault()                  */
/************************************************************************/

void OGRPGCommonLayerNormalizeDefault(OGRFieldDefn* poFieldDefn,
                                      const char* pszDefault)
{
    if(pszDefault==NULL)
        return;
    CPLString osDefault(pszDefault);
    size_t nPos = osDefault.find("::character varying");
    if( nPos != std::string::npos &&
        nPos + strlen("::character varying") == osDefault.size() )
    {
        osDefault.resize(nPos);
    }
    else if( (nPos = osDefault.find("::text")) != std::string::npos &&
             nPos + strlen("::text") == osDefault.size() )
    {
        osDefault.resize(nPos);
    }
    else if( strcmp(osDefault, "now()") == 0 )
        osDefault = "CURRENT_TIMESTAMP";
    else if( strcmp(osDefault, "('now'::text)::date") == 0 )
        osDefault = "CURRENT_DATE";
    else if( strcmp(osDefault, "('now'::text)::time with time zone") == 0 )
        osDefault = "CURRENT_TIME";
    else
    {
        nPos = osDefault.find("::timestamp with time zone");
        if( poFieldDefn->GetType() == OFTDateTime && nPos != std::string::npos )
        {
            osDefault.resize(nPos);
            nPos = osDefault.find("'+");
            if( nPos != std::string::npos )
            {
                osDefault.resize(nPos);
                osDefault += "'";
            }
            int nYear = 0;
            int nMonth = 0;
            int nDay = 0;
            int nHour = 0;
            int nMinute = 0;
            float fSecond = 0.0f;
            if( sscanf(osDefault, "'%d-%d-%d %d:%d:%f'", &nYear, &nMonth, &nDay,
                                &nHour, &nMinute, &fSecond) == 6 ||
                sscanf(osDefault, "'%d-%d-%d %d:%d:%f+00'", &nYear, &nMonth, &nDay,
                                &nHour, &nMinute, &fSecond) == 6)
            {
                if( osDefault.find('.') == std::string::npos )
                    osDefault = CPLSPrintf("'%04d/%02d/%02d %02d:%02d:%02d'",
                                            nYear, nMonth, nDay, nHour, nMinute, (int)(fSecond+0.5));
                else
                    osDefault = CPLSPrintf("'%04d/%02d/%02d %02d:%02d:%06.3f'",
                                                    nYear, nMonth, nDay, nHour, nMinute, fSecond);
            }
        }
    }
    poFieldDefn->SetDefault(osDefault);
}

/************************************************************************/
/*                     OGRPGCommonLayerGetPGDefault()                   */
/************************************************************************/

CPLString OGRPGCommonLayerGetPGDefault(OGRFieldDefn* poFieldDefn)
{
    CPLString osRet = poFieldDefn->GetDefault();
    int nYear = 0;
    int nMonth = 0;
    int nDay = 0;
    int nHour = 0;
    int nMinute = 0;
    float fSecond = 0.0f;
    if( sscanf(osRet, "'%d/%d/%d %d:%d:%f'",
                &nYear, &nMonth, &nDay,
                &nHour, &nMinute, &fSecond) == 6 )
    {
        osRet.resize(osRet.size()-1);
        osRet += "+00'::timestamp with time zone";
    }
    return osRet;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRErr OGRPGDumpLayer::CreateField( OGRFieldDefn *poFieldIn,
                                    int bApproxOK )
{
    CPLString osFieldType;
    OGRFieldDefn oField( poFieldIn );

    // Can be set to NO to test ogr2ogr default behaviour
    const bool bAllowCreationOfFieldWithFIDName =
        CPLTestBool(CPLGetConfigOption(
            "PGDUMP_DEBUG_ALLOW_CREATION_FIELD_WITH_FID_NAME", "YES"));

    if( bAllowCreationOfFieldWithFIDName && pszFIDColumn != NULL &&
        EQUAL( oField.GetNameRef(), pszFIDColumn ) &&
        oField.GetType() != OFTInteger &&
        oField.GetType() != OFTInteger64 )
    {
        CPLError(CE_Failure, CPLE_AppDefined, "Wrong field type for %s",
                 oField.GetNameRef());
        return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Do we want to "launder" the column names into Postgres          */
/*      friendly format?                                                */
/* -------------------------------------------------------------------- */
    if( bLaunderColumnNames )
    {
        char *pszSafeName =
            OGRPGCommonLaunderName( oField.GetNameRef(), "PGDump" );

        oField.SetName( pszSafeName );
        CPLFree( pszSafeName );

        if( EQUAL(oField.GetNameRef(),"oid") )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "Renaming field 'oid' to 'oid_' to avoid conflict with "
                      "internal oid field." );
            oField.SetName( "oid_" );
        }
    }

    const char* pszOverrideType =
        CSLFetchNameValue(papszOverrideColumnTypes, oField.GetNameRef());
    if( pszOverrideType != NULL )
    {
        osFieldType = pszOverrideType;
    }
    else
    {
        osFieldType =
            OGRPGCommonLayerGetType(oField, bPreservePrecision,
                                    CPL_TO_BOOL(bApproxOK));
        if (osFieldType.empty())
            return OGRERR_FAILURE;
    }

/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */
    CPLString osCommand;
    osCommand.Printf( "ALTER TABLE %s ADD COLUMN %s %s",
                      pszSqlTableName,
                      OGRPGDumpEscapeColumnName(oField.GetNameRef()).c_str(),
                      osFieldType.c_str() );
    if( !oField.IsNullable() )
        osCommand += " NOT NULL";
    if( oField.GetDefault() != NULL && !oField.IsDefaultDriverSpecific() )
    {
        osCommand += " DEFAULT ";
        osCommand += OGRPGCommonLayerGetPGDefault(&oField);
    }

    poFeatureDefn->AddFieldDefn( &oField );

    if( bAllowCreationOfFieldWithFIDName && pszFIDColumn != NULL &&
        EQUAL( oField.GetNameRef(), pszFIDColumn ) )
    {
        iFIDAsRegularColumnIndex = poFeatureDefn->GetFieldCount() - 1;
    }
    else
    {
        if( bCreateTable )
            poDS->Log(osCommand);
    }

    return OGRERR_NONE;
}

/************************************************************************/
/*                           CreateGeomField()                          */
/************************************************************************/

OGRErr OGRPGDumpLayer::CreateGeomField( OGRGeomFieldDefn *poGeomFieldIn,
                                        int /* bApproxOK */ )
{
    OGRwkbGeometryType eType = poGeomFieldIn->GetType();
    if( eType == wkbNone )
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "Cannot create geometry field of type wkbNone");
        return OGRERR_FAILURE;
    }

    // Check if GEOMETRY_NAME layer creation option was set, but no initial
    // column was created in ICreateLayer()
    const CPLString osGeomFieldName =
        !m_osFirstGeometryFieldName.empty()
        ? m_osFirstGeometryFieldName
        : CPLString(poGeomFieldIn->GetNameRef());

    m_osFirstGeometryFieldName = ""; // reset for potential next geom columns

    OGRGeomFieldDefn oTmpGeomFieldDefn( poGeomFieldIn );
    oTmpGeomFieldDefn.SetName(osGeomFieldName);

    CPLString               osCommand;
    OGRPGDumpGeomFieldDefn *poGeomField =
        new OGRPGDumpGeomFieldDefn( &oTmpGeomFieldDefn );

/* -------------------------------------------------------------------- */
/*      Do we want to "launder" the column names into Postgres          */
/*      friendly format?                                                */
/* -------------------------------------------------------------------- */
    if( bLaunderColumnNames )
    {
        char *pszSafeName =
            OGRPGCommonLaunderName( poGeomField->GetNameRef(), "PGDump" );

        poGeomField->SetName( pszSafeName );
        CPLFree( pszSafeName );
    }

    OGRSpatialReference* poSRS = poGeomField->GetSpatialRef();
    int nSRSId = nUnknownSRSId;
    if( nForcedSRSId != -2 )
        nSRSId = nForcedSRSId;
    else if( poSRS != NULL )
    {
        const char* pszAuthorityName = poSRS->GetAuthorityName(NULL);
        if( pszAuthorityName != NULL && EQUAL( pszAuthorityName, "EPSG" ) )
        {
            /* Assume the EPSG Id is the SRS ID. Might be a wrong guess ! */
            nSRSId = atoi( poSRS->GetAuthorityCode(NULL) );
        }
        else
        {
            const char* pszGeogCSName = poSRS->GetAttrValue("GEOGCS");
            if (pszGeogCSName != NULL && EQUAL(pszGeogCSName, "GCS_WGS_1984"))
                nSRSId = 4326;
        }
    }

    poGeomField->nSRSId = nSRSId;

    int GeometryTypeFlags = 0;
    if( OGR_GT_HasZ((OGRwkbGeometryType)eType) )
        GeometryTypeFlags |= OGRGeometry::OGR_G_3D;
    if( OGR_GT_HasM((OGRwkbGeometryType)eType) )
        GeometryTypeFlags |= OGRGeometry::OGR_G_MEASURED;
    if( nForcedGeometryTypeFlags >= 0 )
    {
        GeometryTypeFlags = nForcedGeometryTypeFlags;
        eType = OGR_GT_SetModifier(eType,
                                   GeometryTypeFlags & OGRGeometry::OGR_G_3D,
                                   GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED);
    }
    poGeomField->SetType(eType);
    poGeomField->GeometryTypeFlags = GeometryTypeFlags;

/* -------------------------------------------------------------------- */
/*      Create the new field.                                           */
/* -------------------------------------------------------------------- */
    if (bCreateTable)
    {
        const char *suffix = "";
        int dim = 2;
        if( (poGeomField->GeometryTypeFlags & OGRGeometry::OGR_G_3D) && (poGeomField->GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED) )
            dim = 4;
        else if( poGeomField->GeometryTypeFlags & OGRGeometry::OGR_G_MEASURED )
        {
            if( wkbFlatten(poGeomField->GetType()) != wkbUnknown )
                suffix = "M";
            dim = 3;
        }
        else if( poGeomField->GeometryTypeFlags & OGRGeometry::OGR_G_3D )
            dim = 3;

        const char *pszGeometryType = OGRToOGCGeomType(poGeomField->GetType());
        osCommand.Printf(
                "SELECT AddGeometryColumn(%s,%s,%s,%d,'%s%s',%d)",
                OGRPGDumpEscapeString(pszSchemaName).c_str(),
                OGRPGDumpEscapeString(poFeatureDefn->GetName()).c_str(),
                OGRPGDumpEscapeString(poGeomField->GetNameRef()).c_str(),
                nSRSId, pszGeometryType, suffix, dim );

        poDS->Log(osCommand);

        if( !poGeomField->IsNullable() )
        {
            osCommand.Printf( "ALTER TABLE %s ALTER COLUMN %s SET NOT NULL",
                              OGRPGDumpEscapeColumnName(poFeatureDefn->GetName()).c_str(),
                              OGRPGDumpEscapeColumnName(poGeomField->GetNameRef()).c_str() );

            poDS->Log(osCommand);
        }

        if( bCreateSpatialIndexFlag )
        {
            osCommand.Printf("CREATE INDEX %s ON %s USING GIST (%s)",
                            OGRPGDumpEscapeColumnName(
                                CPLSPrintf("%s_%s_geom_idx", GetName(), poGeomField->GetNameRef())).c_str(),
                            pszSqlTableName,
                            OGRPGDumpEscapeColumnName(poGeomField->GetNameRef()).c_str());

            poDS->Log(osCommand);
        }
    }

    poFeatureDefn->AddGeomFieldDefn( poGeomField, FALSE );

    return OGRERR_NONE;
}

/************************************************************************/
/*                        SetOverrideColumnTypes()                      */
/************************************************************************/

void OGRPGDumpLayer::SetOverrideColumnTypes( const char* pszOverrideColumnTypes )
{
    if( pszOverrideColumnTypes == NULL )
        return;

    const char* pszIter = pszOverrideColumnTypes;
    CPLString osCur;
    while(*pszIter != '\0')
    {
        if( *pszIter == '(' )
        {
            /* Ignore commas inside ( ) pair */
            while(*pszIter != '\0')
            {
                if( *pszIter == ')' )
                {
                    osCur += *pszIter;
                    pszIter ++;
                    break;
                }
                osCur += *pszIter;
                pszIter ++;
            }
            if( *pszIter == '\0')
                break;
        }

        if( *pszIter == ',' )
        {
            papszOverrideColumnTypes = CSLAddString(papszOverrideColumnTypes, osCur);
            osCur = "";
        }
        else
            osCur += *pszIter;
        pszIter ++;
    }
    if( !osCur.empty() )
        papszOverrideColumnTypes = CSLAddString(papszOverrideColumnTypes, osCur);
}

/************************************************************************/
/*                              SetMetadata()                           */
/************************************************************************/

CPLErr OGRPGDumpLayer::SetMetadata(char** papszMD, const char* pszDomain)
{
    OGRLayer::SetMetadata(papszMD, pszDomain);
    if( !osForcedDescription.empty() &&
        (pszDomain == NULL || EQUAL(pszDomain, "")) )
    {
        OGRLayer::SetMetadataItem("DESCRIPTION", osForcedDescription);
    }

    if( (pszDomain == NULL || EQUAL(pszDomain, "")) &&
        osForcedDescription.empty() )
    {
        const char* l_pszDescription = OGRLayer::GetMetadataItem("DESCRIPTION");
        CPLString osCommand;

        osCommand.Printf( "COMMENT ON TABLE %s IS %s",
                           pszSqlTableName,
                           l_pszDescription && l_pszDescription[0] != '\0' ?
                              OGRPGDumpEscapeString(l_pszDescription).c_str() : "NULL" );
        poDS->Log( osCommand );
    }

    return CE_None;
}

/************************************************************************/
/*                            SetMetadataItem()                         */
/************************************************************************/

CPLErr OGRPGDumpLayer::SetMetadataItem(const char* pszName, const char* pszValue,
                                       const char* pszDomain)
{
    if( (pszDomain == NULL || EQUAL(pszDomain, "")) && pszName != NULL &&
        EQUAL(pszName, "DESCRIPTION") && !osForcedDescription.empty() )
    {
        return CE_None;
    }
    OGRLayer::SetMetadataItem(pszName, pszValue, pszDomain);
    if( (pszDomain == NULL || EQUAL(pszDomain, "")) && pszName != NULL &&
        EQUAL(pszName, "DESCRIPTION") )
    {
        SetMetadata( GetMetadata() );
    }
    return CE_None;
}

/************************************************************************/
/*                      SetForcedDescription()                          */
/************************************************************************/

void OGRPGDumpLayer::SetForcedDescription( const char* pszDescriptionIn )
{
    osForcedDescription = pszDescriptionIn;
    OGRLayer::SetMetadataItem("DESCRIPTION", osForcedDescription);

    if( pszDescriptionIn[0] != '\0' )
    {
        CPLString osCommand;
        osCommand.Printf( "COMMENT ON TABLE %s IS %s",
                            pszSqlTableName,
                            OGRPGDumpEscapeString(pszDescriptionIn).c_str() );
        poDS->Log( osCommand );
    }
}
