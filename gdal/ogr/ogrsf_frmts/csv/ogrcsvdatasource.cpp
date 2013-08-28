/******************************************************************************
 * $Id$
 *
 * Project:  CSV Translator
 * Purpose:  Implements OGRCSVDataSource class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_csv.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include "cpl_csv.h"
#include "cpl_vsi_virtual.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                          OGRCSVDataSource()                          */
/************************************************************************/

OGRCSVDataSource::OGRCSVDataSource()

{
    papoLayers = NULL;
    nLayers = 0;

    pszName = NULL;

    bUpdate = FALSE;
    bEnableGeometryFields = FALSE;
}

/************************************************************************/
/*                         ~OGRCSVDataSource()                          */
/************************************************************************/

OGRCSVDataSource::~OGRCSVDataSource()

{
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    CPLFree( papoLayers );

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
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                          GetRealExtension()                          */
/************************************************************************/

CPLString OGRCSVDataSource::GetRealExtension(CPLString osFilename)
{
    CPLString osExt = CPLGetExtension(osFilename);
    if( strncmp(osFilename, "/vsigzip/", 9) == 0 && EQUAL(osExt, "gz") )
    {
        if( strlen(osFilename) > 7 && EQUAL(osFilename + strlen(osFilename) - 7, ".csv.gz") )
            osExt = "csv";
        else if( strlen(osFilename) > 7 && EQUAL(osFilename + strlen(osFilename) - 7, ".tsv.gz") )
            osExt = "tsv";
    }
    return osExt;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRCSVDataSource::Open( const char * pszFilename, int bUpdateIn,
                            int bForceOpen )

{
    pszName = CPLStrdup( pszFilename );
    bUpdate = bUpdateIn;

    if (bUpdateIn && bForceOpen && EQUAL(pszFilename, "/vsistdout/"))
        return TRUE;

    /* For writable /vsizip/, do nothing more */
    if (bUpdateIn && bForceOpen && strncmp(pszFilename, "/vsizip/", 8) == 0)
        return TRUE;

    CPLString osFilename(pszFilename);
    CPLString osBaseFilename = CPLGetFilename(pszFilename);
    CPLString osExt = GetRealExtension(osFilename);
    pszFilename = NULL;

    int bIgnoreExtension = EQUALN(osFilename, "CSV:", 4);
    int bUSGeonamesFile = FALSE;
    int bGeonamesOrgFile = FALSE;
    if (bIgnoreExtension)
    {
        osFilename = osFilename + 4;
    }

    /* Those are *not* real .XLS files, but text file with tab as column separator */
    if (EQUAL(osBaseFilename, "NfdcFacilities.xls") ||
        EQUAL(osBaseFilename, "NfdcRunways.xls") ||
        EQUAL(osBaseFilename, "NfdcRemarks.xls") ||
        EQUAL(osBaseFilename, "NfdcSchedules.xls"))
    {
        if (bUpdateIn)
            return FALSE;
        bIgnoreExtension = TRUE;
    }
    else if ((EQUALN(osBaseFilename, "NationalFile_", 13) ||
              EQUALN(osBaseFilename, "POP_PLACES_", 11) ||
              EQUALN(osBaseFilename, "HIST_FEATURES_", 14) ||
              EQUALN(osBaseFilename, "US_CONCISE_", 11) ||
              EQUALN(osBaseFilename, "AllNames_", 9) ||
              EQUALN(osBaseFilename, "Feature_Description_History_", 28) ||
              EQUALN(osBaseFilename, "ANTARCTICA_", 11) ||
              EQUALN(osBaseFilename, "GOVT_UNITS_", 11) ||
              EQUALN(osBaseFilename, "NationalFedCodes_", 17) ||
              EQUALN(osBaseFilename, "AllStates_", 10) ||
              EQUALN(osBaseFilename, "AllStatesFedCodes_", 18) ||
              (strlen(osBaseFilename) > 2 && EQUALN(osBaseFilename+2, "_Features_", 10)) ||
              (strlen(osBaseFilename) > 2 && EQUALN(osBaseFilename+2, "_FedCodes_", 10))) &&
             (EQUAL(osExt, "txt") || EQUAL(osExt, "zip")) )
    {
        if (bUpdateIn)
            return FALSE;
        bIgnoreExtension = TRUE;
        bUSGeonamesFile = TRUE;

        if (EQUAL(osExt, "zip") &&
            strstr(osFilename, "/vsizip/") == NULL )
        {
            osFilename = "/vsizip/" + osFilename;
        }
    }
    else if (EQUAL(osBaseFilename, "allCountries.txt") ||
             EQUAL(osBaseFilename, "allCountries.zip"))
    {
        if (bUpdateIn)
            return FALSE;
        bIgnoreExtension = TRUE;
        bGeonamesOrgFile = TRUE;

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
            return OpenTable( osFilename, "ARP");
        }
        else if (EQUAL(CPLGetFilename(osFilename), "NfdcRunways.xls"))
        {
            OpenTable( osFilename, "BaseEndPhysical");
            OpenTable( osFilename, "BaseEndDisplaced");
            OpenTable( osFilename, "ReciprocalEndPhysical");
            OpenTable( osFilename, "ReciprocalEndDisplaced");
            return nLayers != 0;
        }
        else if (bUSGeonamesFile)
        {
            /* GNIS specific */
            if (EQUALN(osBaseFilename, "NationalFedCodes_", 17) ||
                EQUALN(osBaseFilename, "AllStatesFedCodes_", 18) ||
                EQUALN(osBaseFilename, "ANTARCTICA_", 11) ||
                (strlen(osBaseFilename) > 2 && EQUALN(osBaseFilename+2, "_FedCodes_", 10)))
            {
                OpenTable( osFilename, NULL, "PRIMARY");
            }
            else if (EQUALN(osBaseFilename, "GOVT_UNITS_", 11) ||
                     EQUALN(osBaseFilename, "Feature_Description_History_", 28))
            {
                OpenTable( osFilename, NULL, "");
            }
            else
            {
                OpenTable( osFilename, NULL, "PRIM");
                OpenTable( osFilename, NULL, "SOURCE");
            }
            return nLayers != 0;
        }

        return OpenTable( osFilename );
    }

/* -------------------------------------------------------------------- */
/*      Is this a single a ZIP file with only a CSV file inside ?       */
/* -------------------------------------------------------------------- */
    if( strncmp(osFilename, "/vsizip/", 8) == 0 &&
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
        return OpenTable( osFilename );
    }

/* -------------------------------------------------------------------- */
/*      Otherwise it has to be a directory.                             */
/* -------------------------------------------------------------------- */
    if( !VSI_ISDIR(sStatBuf.st_mode) )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Scan through for entries ending in .csv.                        */
/* -------------------------------------------------------------------- */
    int nNotCSVCount = 0, i;
    char **papszNames = CPLReadDir( osFilename );

    for( i = 0; papszNames != NULL && papszNames[i] != NULL; i++ )
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
            if( !OpenTable( oSubFilename ) )
            {
                CPLDebug("CSV", "Cannot open %s", oSubFilename.c_str());
                nNotCSVCount++;
                continue;
            }
        }

        /* GNIS specific */
        else if ( strlen(papszNames[i]) > 2 &&
                  EQUALN(papszNames[i]+2, "_Features_", 10) &&
                  EQUAL(CPLGetExtension(papszNames[i]), "txt") )
        {
            int bRet = OpenTable( oSubFilename, NULL, "PRIM");
            bRet |= OpenTable( oSubFilename, NULL, "SOURCE");
            if ( !bRet )
            {
                CPLDebug("CSV", "Cannot open %s", oSubFilename.c_str());
                nNotCSVCount++;
                continue;
            }
        }
        /* GNIS specific */
        else if ( strlen(papszNames[i]) > 2 &&
                  EQUALN(papszNames[i]+2, "_FedCodes_", 10) &&
                  EQUAL(CPLGetExtension(papszNames[i]), "txt") )
        {
            if ( !OpenTable( oSubFilename, NULL, "PRIMARY") )
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

int OGRCSVDataSource::OpenTable( const char * pszFilename,
                                 const char* pszNfdcRunwaysGeomField,
                                 const char* pszGeonamesGeomFieldPrefix)

{
/* -------------------------------------------------------------------- */
/*      Open the file.                                                  */
/* -------------------------------------------------------------------- */
    VSILFILE       * fp;

    if( bUpdate )
        fp = VSIFOpenL( pszFilename, "rb+" );
    else
        fp = VSIFOpenL( pszFilename, "rb" );
    if( fp == NULL )
    {
        CPLError( CE_Warning, CPLE_OpenFailed, 
                  "Failed to open %s, %s.", 
                  pszFilename, VSIStrerror( errno ) );
        return FALSE;
    }

    if( !bUpdate && strstr(pszFilename, "/vsigzip/") == NULL &&
        strstr(pszFilename, "/vsizip/") == NULL )
        fp = (VSILFILE*) VSICreateBufferedReaderHandle((VSIVirtualHandle*)fp);

    CPLString osLayerName = CPLGetBasename(pszFilename);
    CPLString osExt = CPLGetExtension(pszFilename);
    if( strncmp(pszFilename, "/vsigzip/", 9) == 0 && EQUAL(osExt, "gz") )
    {
        if( strlen(pszFilename) > 7 && EQUAL(pszFilename + strlen(pszFilename) - 7, ".csv.gz") )
        {
            osLayerName = osLayerName.substr(0, osLayerName.size() - 4);
            osExt = "csv";
        }
        else if( strlen(pszFilename) > 7 && EQUAL(pszFilename + strlen(pszFilename) - 7, ".tsv.gz") )
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
        return FALSE;
    }
    char chDelimiter = CSVDetectSeperator(pszLine);

    /* Force the delimiter to be TAB for a .tsv file that has a tabulation */
    /* in its first line */
    if( EQUAL(osExt, "tsv") && chDelimiter != '\t' &&
        strchr(pszLine, '\t') != NULL )
    {
        chDelimiter = '\t';
    }

    VSIRewindL( fp );

    /* GNIS specific */
    if (pszGeonamesGeomFieldPrefix != NULL &&
        strchr(pszLine, '|') != NULL)
        chDelimiter = '|';

    char **papszFields = OGRCSVReadParseLineL( fp, chDelimiter, FALSE );
						
    if( CSLCount(papszFields) < 2 )
    {
        VSIFCloseL( fp );
        CSLDestroy( papszFields );
        return FALSE;
    }

    VSIRewindL( fp );
    CSLDestroy( papszFields );

/* -------------------------------------------------------------------- */
/*      Create a layer.                                                 */
/* -------------------------------------------------------------------- */
    nLayers++;
    papoLayers = (OGRCSVLayer **) CPLRealloc(papoLayers, 
                                             sizeof(void*) * nLayers);

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
    papoLayers[nLayers-1] = 
        new OGRCSVLayer( osLayerName, fp, pszFilename, FALSE, bUpdate,
                         chDelimiter, pszNfdcRunwaysGeomField, pszGeonamesGeomFieldPrefix );

    return TRUE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *
OGRCSVDataSource::CreateLayer( const char *pszLayerName, 
                               OGRSpatialReference *poSpatialRef,
                               OGRwkbGeometryType eGType,
                               char ** papszOptions  )

{
/* -------------------------------------------------------------------- */
/*      Verify we are in update mode.                                   */
/* -------------------------------------------------------------------- */
    if (!bUpdate)
    {
        CPLError( CE_Failure, CPLE_NoWriteAccess,
                  "Data source %s opened read-only.\n"
                  "New layer %s cannot be created.\n",
                  pszName, pszLayerName );

        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Verify that the datasource is a directory.                      */
/* -------------------------------------------------------------------- */
    VSIStatBufL sStatBuf;

    if( strncmp(pszName, "/vsizip/", 8) == 0)
    {
        /* Do nothing */
    }
    else if( !EQUAL(pszName, "/vsistdout/") &&
        (VSIStatL( pszName, &sStatBuf ) != 0
        || !VSI_ISDIR( sStatBuf.st_mode )) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Attempt to create csv layer (file) against a non-directory datasource." );
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
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                  "SEPARATOR=%s not understood, use one of COMMA, SEMICOLON or TAB.",
                  pszDelimiter );
        }
    }

/* -------------------------------------------------------------------- */
/*      Create a layer.                                                 */
/* -------------------------------------------------------------------- */
    nLayers++;
    papoLayers = (OGRCSVLayer **) CPLRealloc(papoLayers, 
                                             sizeof(void*) * nLayers);
    
    papoLayers[nLayers-1] = new OGRCSVLayer( pszLayerName, NULL, osFilename,
                                             TRUE, TRUE, chDelimiter, NULL, NULL );

/* -------------------------------------------------------------------- */
/*      Was a partiuclar CRLF order requested?                          */
/* -------------------------------------------------------------------- */
    const char *pszCRLFFormat = CSLFetchNameValue( papszOptions, "LINEFORMAT");
    int bUseCRLF;

    if( pszCRLFFormat == NULL )
    {
#ifdef WIN32
        bUseCRLF = TRUE;
#else
        bUseCRLF = FALSE;
#endif
    }
    else if( EQUAL(pszCRLFFormat,"CRLF") )
        bUseCRLF = TRUE;
    else if( EQUAL(pszCRLFFormat,"LF") )
        bUseCRLF = FALSE;
    else
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "LINEFORMAT=%s not understood, use one of CRLF or LF.",
                  pszCRLFFormat );
#ifdef WIN32
        bUseCRLF = TRUE;
#else
        bUseCRLF = FALSE;
#endif
    }
    
    papoLayers[nLayers-1]->SetCRLF( bUseCRLF );

/* -------------------------------------------------------------------- */
/*      Should we write the geometry ?                                  */
/* -------------------------------------------------------------------- */
    const char *pszGeometry = CSLFetchNameValue( papszOptions, "GEOMETRY");
    if( bEnableGeometryFields )
    {
        papoLayers[nLayers-1]->SetWriteGeometry(eGType, OGR_CSV_GEOM_AS_WKT);
    }
    else if (pszGeometry != NULL)
    {
        if (EQUAL(pszGeometry, "AS_WKT"))
        {
            papoLayers[nLayers-1]->SetWriteGeometry(eGType, OGR_CSV_GEOM_AS_WKT);
        }
        else if (EQUAL(pszGeometry, "AS_XYZ") ||
                 EQUAL(pszGeometry, "AS_XY") ||
                 EQUAL(pszGeometry, "AS_YX"))
        {
            if (eGType == wkbUnknown || wkbFlatten(eGType) == wkbPoint)
            {
                papoLayers[nLayers-1]->SetWriteGeometry(eGType,
                                                        EQUAL(pszGeometry, "AS_XYZ") ? OGR_CSV_GEOM_AS_XYZ :
                                                        EQUAL(pszGeometry, "AS_XY") ?  OGR_CSV_GEOM_AS_XY :
                                                                                       OGR_CSV_GEOM_AS_YX);
            }
            else
            {
                CPLError( CE_Warning, CPLE_AppDefined, 
                          "Geometry type %s is not compatible with GEOMETRY=AS_XYZ.",
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
    if (pszCreateCSVT)
        papoLayers[nLayers-1]->SetCreateCSVT(CSLTestBoolean(pszCreateCSVT));

/* -------------------------------------------------------------------- */
/*      Should we write a UTF8 BOM ?                                    */
/* -------------------------------------------------------------------- */

    const char *pszWriteBOM = CSLFetchNameValue( papszOptions, "WRITE_BOM");
    if (pszWriteBOM)
        papoLayers[nLayers-1]->SetWriteBOM(CSLTestBoolean(pszWriteBOM));

    return papoLayers[nLayers-1];
}

/************************************************************************/
/*                            DeleteLayer()                             */
/************************************************************************/

OGRErr OGRCSVDataSource::DeleteLayer( int iLayer )

{
    char *pszFilename;
    char *pszFilenameCSVT;

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

    pszFilename = 
        CPLStrdup(CPLFormFilename(pszName,papoLayers[iLayer]->GetLayerDefn()->GetName(),"csv"));
    pszFilenameCSVT = 
        CPLStrdup(CPLFormFilename(pszName,papoLayers[iLayer]->GetLayerDefn()->GetName(),"csvt"));

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
