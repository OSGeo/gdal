/******************************************************************************
 * $Id$
 *
 * Project:  TIGER/Line Translator
 * Purpose:  Implements OGRTigerDataSource class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam <warmerdam@pobox.com>
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

#include "ogr_tiger.h"
#include "cpl_conv.h"
#include "cpl_string.h"
#include <ctype.h>

CPL_CVSID("$Id$");

/************************************************************************/
/*                        TigerClassifyVersion()                        */
/************************************************************************/

TigerVersion TigerClassifyVersion( int nVersionCode )

{
    TigerVersion        nVersion;
    int                 nYear, nMonth;

/*
** TIGER Versions
**
** 0000           TIGER/Line Precensus Files, 1990 
** 0002           TIGER/Line Initial Voting District Codes Files, 1990 
** 0003           TIGER/Line Files, 1990 
** 0005           TIGER/Line Files, 1992 
** 0021           TIGER/Line Files, 1994 
** 0024           TIGER/Line Files, 1995 
** 9706 to 9810   TIGER/Line Files, 1997 
** 9812 to 9904   TIGER/Line Files, 1998 
** 0006 to 0008   TIGER/Line Files, 1999 
** 0010 to 0011   TIGER/Line Files, Redistricting Census 2000
** 0103 to 0108   TIGER/Line Files, Census 2000
**
** 0203 to 0205   TIGER/Line Files, UA 2000
** ????    ????
**
** 0206 to 0299   TIGER/Line Files, 2002
** 0300 to 0399   TIGER/Line Files, 2003
** 0400+          TIGER/Line Files, 2004 - one sample is 0405
** ????
*/

    nVersion = TIGER_Unknown;
    if( nVersionCode == 0 )
        nVersion = TIGER_1990_Precensus;
    else if( nVersionCode == 2 )
        nVersion = TIGER_1990;
    else if( nVersionCode == 3 )
        nVersion = TIGER_1992;
    else if( nVersionCode == 5 )
        nVersion = TIGER_1994;
    else if( nVersionCode == 21 )
        nVersion = TIGER_1994;
    else if( nVersionCode == 24 )
        nVersion = TIGER_1995;

    else if( nVersionCode == 9999 )  /* special hack, fme bug 7625 */
        nVersion = TIGER_UA2000;

    nYear = nVersionCode % 100;
    nMonth = nVersionCode / 100;

    nVersionCode = nYear * 100 + nMonth;

    if( nVersion != TIGER_Unknown )
        /* do nothing */;
    else if( nVersionCode >= 9706 && nVersionCode <= 9810 )
        nVersion = TIGER_1997;
    else if( nVersionCode >= 9812 && nVersionCode <= 9904 )
        nVersion = TIGER_1998;
    else if( nVersionCode >=    6 /*0006*/ && nVersionCode <=    8 /*0008*/ )
        nVersion = TIGER_1999;
    else if( nVersionCode >=   10 /*0010*/ && nVersionCode <=   11 /*0011*/ )
        nVersion = TIGER_2000_Redistricting;
    else if( nVersionCode >=  103 /*0103*/ && nVersionCode <= 108 /*0108*/ )
        nVersion = TIGER_2000_Census;
    else if( nVersionCode >=  203 /*0302*/ && nVersionCode <= 205 /*0502*/ )
        nVersion = TIGER_UA2000;
    else if( nVersionCode >=  210 /*1002*/ && nVersionCode <= 306 /*0603*/)
        nVersion = TIGER_2002;
    else if( nVersionCode >=  312 /*1203*/ && nVersionCode <= 403 /*0304*/)
        nVersion = TIGER_2003;
    else if( nVersionCode >=  404 )
        nVersion = TIGER_2004;

    return nVersion;
}

/************************************************************************/
/*                         TigerVersionString()                         */
/************************************************************************/

const char * TigerVersionString( TigerVersion nVersion )
{

  if (nVersion == TIGER_1990_Precensus) { return "TIGER_1990_Precensus"; }
  if (nVersion == TIGER_1990) { return "TIGER_1990"; }
  if (nVersion == TIGER_1992) { return "TIGER_1992"; }
  if (nVersion == TIGER_1994) { return "TIGER_1994"; }
  if (nVersion == TIGER_1995) { return "TIGER_1995"; }
  if (nVersion == TIGER_1997) { return "TIGER_1997"; }
  if (nVersion == TIGER_1998) { return "TIGER_1998"; }
  if (nVersion == TIGER_1999) { return "TIGER_1999"; }
  if (nVersion == TIGER_2000_Redistricting) { return "TIGER_2000_Redistricting"; }
  if (nVersion == TIGER_UA2000) { return "TIGER_UA2000"; }
  if (nVersion == TIGER_2002) { return "TIGER_2002"; }
  if (nVersion == TIGER_2003) { return "TIGER_2003"; }
  if (nVersion == TIGER_2004) { return "TIGER_2004"; }
  if (nVersion == TIGER_Unknown) { return "TIGER_Unknown"; }
  return "???";
}

/************************************************************************/
/*                         TigerCheckVersion()                          */
/*                                                                      */
/*      Some tiger products seem to be generated with version info      */
/*      that doesn't match the tiger specs.  We can sometimes           */
/*      recognise the wrongness by checking the record length of        */
/*      some well known changing files and adjusting the version        */
/*      based on this.                                                  */
/************************************************************************/

TigerVersion OGRTigerDataSource::TigerCheckVersion( TigerVersion nOldVersion, 
                                                    const char *pszFilename )

{
    if( nOldVersion != TIGER_2002 )
        return nOldVersion;

    char *pszRTCFilename = BuildFilename( pszFilename, "C" );
    VSILFILE *fp = VSIFOpenL( pszRTCFilename, "rb" );
    CPLFree( pszRTCFilename );

    if( fp == NULL )
        return nOldVersion;
    
    char        szHeader[115];

    if( VSIFReadL( szHeader, sizeof(szHeader)-1, 1, fp ) < 1 )
    {
        VSIFCloseL( fp );
        return nOldVersion;
    }

    VSIFCloseL( fp );
    
/* -------------------------------------------------------------------- */
/*      Is the record length 112?  If so, it is an older version        */
/*      than 2002.                                                      */
/* -------------------------------------------------------------------- */
    if( szHeader[112] == 10 || szHeader[112] == 13 )
    {
        CPLDebug( "TIGER", "Forcing version back to UA2000 since RTC records are short." );
        return TIGER_UA2000;
    }
    else
        return nOldVersion;
}

/************************************************************************/
/*                         OGRTigerDataSource()                         */
/************************************************************************/

OGRTigerDataSource::OGRTigerDataSource()

{
    bWriteMode = FALSE;

    nLayers = 0;
    papoLayers = NULL;

    nModules = 0;
    papszModules = NULL;

    pszName = NULL;
    pszPath = NULL;

    papszOptions = NULL;

    poSpatialRef = new OGRSpatialReference( "GEOGCS[\"NAD83\",DATUM[\"North_American_Datum_1983\",SPHEROID[\"GRS 1980\",6378137,298.257222101]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]]" );
}

/************************************************************************/
/*                        ~OGRTigerDataSource()                         */
/************************************************************************/

OGRTigerDataSource::~OGRTigerDataSource()

{
    int         i;

    for( i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    
    CPLFree( papoLayers );

    CPLFree( pszName );
    CPLFree( pszPath );

    CSLDestroy( papszOptions );

    CSLDestroy( papszModules );

    delete poSpatialRef;
}

/************************************************************************/
/*                              AddLayer()                              */
/************************************************************************/

void OGRTigerDataSource::AddLayer( OGRTigerLayer * poNewLayer )

{
    papoLayers = (OGRTigerLayer **)
        CPLRealloc( papoLayers, sizeof(void*) * ++nLayers );
    
    papoLayers[nLayers-1] = poNewLayer;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRTigerDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRTigerDataSource::GetLayer( const char *pszLayerName )

{
    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( EQUAL(papoLayers[iLayer]->GetLayerDefn()->GetName(),pszLayerName) )
            return papoLayers[iLayer];
    }

    return NULL;
}

/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

int OGRTigerDataSource::GetLayerCount()

{
    return nLayers;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRTigerDataSource::Open( const char * pszFilename, int bTestOpen,
                              char ** papszLimitedFileList )

{
    VSIStatBufL     stat;
    char            **papszFileList = NULL;
    int             i;

    pszName = CPLStrdup( pszFilename );

/* -------------------------------------------------------------------- */
/*      Is the given path a directory or a regular file?                */
/* -------------------------------------------------------------------- */
    if( VSIStatExL( pszFilename, &stat, VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG ) != 0
        || (!VSI_ISDIR(stat.st_mode) && !VSI_ISREG(stat.st_mode)) )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_AppDefined,
                   "%s is neither a file or directory, Tiger access failed.\n",
                      pszFilename );

        return FALSE;
    }
    
/* -------------------------------------------------------------------- */
/*      Build a list of filenames we figure are Tiger files.            */
/* -------------------------------------------------------------------- */
    if( VSI_ISREG(stat.st_mode) )
    {
        char       szModule[128];
        
        if( strlen(CPLGetFilename(pszFilename)) == 0 )
        {
            return FALSE;
        }

        pszPath = CPLStrdup( CPLGetPath(pszFilename) );

        strncpy( szModule, CPLGetFilename(pszFilename), sizeof(szModule)-1 );
        /* Make sure the buffer is 0 terminated */
        szModule[sizeof(szModule)-1] = '\0';

        /* And now remove last character of filename */
        szModule[strlen(szModule)-1] = '\0';

        papszFileList = CSLAddString( papszFileList, szModule );
    }
    else
    {
        char      **candidateFileList = CPLReadDir( pszFilename );
        int         i;

        pszPath = CPLStrdup( pszFilename );

        for( i = 0; 
             candidateFileList != NULL && candidateFileList[i] != NULL; 
             i++ ) 
        {
            int nCandidateLen = strlen(candidateFileList[i]);

            if( papszLimitedFileList != NULL 
                && CSLFindString(papszLimitedFileList,
                                 CPLGetBasename(candidateFileList[i])) == -1 )
            {
                continue;
            }

            if( nCandidateLen > 4 
                && candidateFileList[i][nCandidateLen-4] == '.'
                && candidateFileList[i][nCandidateLen-1] == '1')
            {
                char       szModule[128];

                strncpy( szModule, candidateFileList[i],
                         strlen(candidateFileList[i])-1 );

                szModule[strlen(candidateFileList[i])-1] = '\0';

                papszFileList = CSLAddString(papszFileList, szModule);
            }
        }

        CSLDestroy( candidateFileList );

        if( CSLCount(papszFileList) == 0 )
        {
            if( !bTestOpen )
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "No candidate Tiger files (TGR*.RT1) found in\n"
                          "directory: %s",
                          pszFilename );

            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Loop over all these files trying to open them.  In testopen     */
/*      mode we first read the first 80 characters, to verify that      */
/*      it looks like an Tiger file.  Note that we don't keep the file  */
/*      open ... we don't want to occupy alot of file handles when      */
/*      handling a whole directory.                                     */
/* -------------------------------------------------------------------- */
    papszModules = NULL;
    
    for( i = 0; papszFileList[i] != NULL; i++ )
    {
        if( bTestOpen || i == 0 )
        {
            char        szHeader[500];
            VSILFILE    *fp;
            char        *pszRecStart = NULL;
            int         bIsGDT = FALSE;
            char       *pszFilename;

            pszFilename = BuildFilename( papszFileList[i], "1" );

            fp = VSIFOpenL( pszFilename, "rb" );
            CPLFree( pszFilename );

            if( fp == NULL )
                continue;
            
            if( VSIFReadL( szHeader, sizeof(szHeader)-1, 1, fp ) < 1 )
            {
                VSIFCloseL( fp );
                continue;
            }

            VSIFCloseL( fp );

            pszRecStart = szHeader;
            szHeader[sizeof(szHeader)-1] = '\0';

            if( EQUALN(pszRecStart,"Copyright (C)",13) 
                && strstr(pszRecStart,"Geographic Data Tech") != NULL )
            {
                bIsGDT = TRUE;

                while( *pszRecStart != '\0' 
                       && *pszRecStart != 10 
                       && *pszRecStart != 13 )
                    pszRecStart++;

                while( *pszRecStart == 10 || *pszRecStart == 13 )
                    pszRecStart++;
            }
            
            if( pszRecStart[0] != '1' )
                continue;

            if( !isdigit(pszRecStart[1]) || !isdigit(pszRecStart[2])
                || !isdigit(pszRecStart[3]) || !isdigit(pszRecStart[4]) )
                continue;

            nVersionCode = atoi(TigerFileBase::GetField( pszRecStart, 2, 5 ));
            nVersion = TigerClassifyVersion( nVersionCode );
            nVersion = TigerCheckVersion( nVersion, papszFileList[i] );

            CPLDebug( "OGR", "Tiger Version Code=%d, Classified as %s ", 
                      nVersionCode, TigerVersionString(nVersion) );

            if(    nVersionCode !=  0
                && nVersionCode !=  2
                && nVersionCode !=  3
                && nVersionCode !=  5
                && nVersionCode != 21 
                && nVersionCode != 24
                && pszRecStart[3]  != '9'
                && pszRecStart[3]  != '0'
                && !bIsGDT )
                continue;

            // we could (and should) add a bunch more validation here.
        }

        papszModules = CSLAddString( papszModules, papszFileList[i] );
    }

    CSLDestroy( papszFileList );

    nModules = CSLCount( papszModules );

    if( nModules == 0 )
    {
        if( !bTestOpen )
        {
            if( VSI_ISREG(stat.st_mode) )
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "No TIGER/Line files (TGR*.RT1) found in\n"
                          "directory: %s",
                          pszFilename );
            else
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "File %s does not appear to be a TIGER/Line .RT1 file.",
                          pszFilename );
        }

        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Do we have a user provided version override?                    */
/* -------------------------------------------------------------------- */
    if( CPLGetConfigOption( "TIGER_VERSION", NULL ) != NULL )
    {
        const char *pszRequestedVersion = 
            CPLGetConfigOption( "TIGER_VERSION", NULL );

        if( EQUALN(pszRequestedVersion,"TIGER_",6) )
        {
            int iCode;

            for( iCode = 1; iCode < TIGER_Unknown; iCode++ )
            {
                if( EQUAL(TigerVersionString((TigerVersion)iCode),
                          pszRequestedVersion) )
                {
                    nVersion = (TigerVersion) iCode;
                    break;
                }
            }

            if( iCode == TIGER_Unknown )
            {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Failed to recognise TIGER_VERSION setting: %s", 
                          pszRequestedVersion );
                return FALSE;
            }                          

            CPLDebug( "OGR", "OVERRIDE Tiger Version %s ", 
                      TigerVersionString(nVersion) );
        }
        else
        {
            nVersionCode = atoi(pszRequestedVersion);
            nVersion = TigerClassifyVersion( nVersionCode );

            CPLDebug( "OGR", 
                      "OVERRIDE Tiger Version Code=%d, Classified as %s ", 
                      nVersionCode, TigerVersionString(nVersion) );
        }
    }

/* -------------------------------------------------------------------- */
/*      Create the layers which appear to exist.                        */
/* -------------------------------------------------------------------- */
    // RT1, RT2, RT3
    AddLayer( new OGRTigerLayer( this,
                                 new TigerCompleteChain( this,
                                                         papszModules[0]) ));

    /* should we have kept track of whether we encountered an RT4 file? */
    // RT4
    AddLayer( new OGRTigerLayer( this,
                                 new TigerAltName( this,
                                                   papszModules[0]) ));

    // RT5
    AddLayer( new OGRTigerLayer( this,
                                 new TigerFeatureIds( this,
                                                      papszModules[0]) ));

    // RT6
    AddLayer( new OGRTigerLayer( this,
                                 new TigerZipCodes( this,
                                                    papszModules[0]) ));
    // RT7
    AddLayer( new OGRTigerLayer( this,
                                 new TigerLandmarks( this,
                                                     papszModules[0]) ));
    
    // RT8
    AddLayer( new OGRTigerLayer( this,
                                 new TigerAreaLandmarks( this,
                                                     papszModules[0]) ));

    // RT9
    if (nVersion < TIGER_2002) {
      AddLayer( new OGRTigerLayer( this,
                                   new TigerKeyFeatures( this,
                                                         papszModules[0]) ));
    }
    
    // RTA, RTS
    AddLayer( new OGRTigerLayer( this,
                                 new TigerPolygon( this,
                                                   papszModules[0]) ));

    // RTB
    if (nVersion >= TIGER_2002) {
      AddLayer( new OGRTigerLayer( this,
                                   new TigerPolygonCorrections( this,
                                                                papszModules[0]) ));
    }
    
    // RTC
    AddLayer( new OGRTigerLayer( this,
                                 new TigerEntityNames( this,
                                                       papszModules[0]) ));

    // RTE
    if (nVersion >= TIGER_2002) {
      AddLayer( new OGRTigerLayer( this,
                                   new TigerPolygonEconomic( this,
                                                             papszModules[0]) ));
    }

    // RTH
    AddLayer( new OGRTigerLayer( this,
                                 new TigerIDHistory( this,
                                                     papszModules[0]) ));
    
    // RTI
    AddLayer( new OGRTigerLayer( this,
                                 new TigerPolyChainLink( this,
                                                       papszModules[0]) ));
    
    // RTM
    AddLayer( new OGRTigerLayer( this,
                                 new TigerSpatialMetadata( this,
                                                           papszModules[0] ) ) );
    
    // RTP
    AddLayer( new OGRTigerLayer( this,
                                 new TigerPIP( this,
                                               papszModules[0]) ));
    
    // RTR
    AddLayer( new OGRTigerLayer( this,
                                 new TigerTLIDRange( this,
                                                     papszModules[0]) ));
    
    // RTT
    if (nVersion >= TIGER_2002) {
      AddLayer( new OGRTigerLayer( this,
                                   new TigerZeroCellID( this,
                                                        papszModules[0]) ));
    }

    // RTU
    if (nVersion >= TIGER_2002) {
      AddLayer( new OGRTigerLayer( this,
                                   new TigerOverUnder( this,
                                                       papszModules[0]) ));
    }

    // RTZ
    AddLayer( new OGRTigerLayer( this,
                                 new TigerZipPlus4( this,
                                                     papszModules[0]) ));
    
    return TRUE;
}

/************************************************************************/
/*                             SetOptions()                             */
/************************************************************************/

void OGRTigerDataSource::SetOptionList( char ** papszNewOptions )

{
    CSLDestroy( papszOptions );
    papszOptions = CSLDuplicate( papszNewOptions );
}

/************************************************************************/
/*                             GetOption()                              */
/************************************************************************/

const char *OGRTigerDataSource::GetOption( const char * pszOption )

{
    return CSLFetchNameValue( papszOptions, pszOption );
}

/************************************************************************/
/*                             GetModule()                              */
/************************************************************************/

const char *OGRTigerDataSource::GetModule( int iModule )

{
    if( iModule < 0 || iModule >= nModules )
        return NULL;
    else
        return papszModules[iModule];
}

/************************************************************************/
/*                            CheckModule()                             */
/*                                                                      */
/*      This is used by the writer to check if this module has been     */
/*      written to before.                                              */
/************************************************************************/

int OGRTigerDataSource::CheckModule( const char *pszModule )

{
    int         i;

    for( i = 0; i < nModules; i++ )
    {
        if( EQUAL(pszModule,papszModules[i]) )
            return TRUE;
    }
    return FALSE;
}

/************************************************************************/
/*                             AddModule()                              */
/************************************************************************/

void OGRTigerDataSource::AddModule( const char *pszModule )

{
    if( CheckModule( pszModule ) )
        return;

    papszModules = CSLAddString( papszModules, pszModule );
    nModules++;
}

/************************************************************************/
/*                           BuildFilename()                            */
/************************************************************************/

char *OGRTigerDataSource::BuildFilename( const char *pszModuleName,
                                    const char *pszExtension )

{
    char        *pszFilename;
    char        szLCExtension[3];

/* -------------------------------------------------------------------- */
/*      Force the record type to lower case if the filename appears     */
/*      to be in lower case.                                            */
/* -------------------------------------------------------------------- */
    if( *pszExtension >= 'A' && *pszExtension <= 'Z' && *pszModuleName == 't' )
    {
        szLCExtension[0] = (*pszExtension) + 'a' - 'A';
        szLCExtension[1] = '\0';
        pszExtension = szLCExtension;
    }

/* -------------------------------------------------------------------- */
/*      Build the filename.                                             */
/* -------------------------------------------------------------------- */
    pszFilename = (char *) CPLMalloc(strlen(GetDirPath())
                                     + strlen(pszModuleName)
                                     + strlen(pszExtension) + 10);

    if( strlen(GetDirPath()) == 0 )
        sprintf( pszFilename, "%s%s",
                 pszModuleName, pszExtension );
    else
        sprintf( pszFilename, "%s/%s%s",
                 GetDirPath(), pszModuleName, pszExtension );

    return pszFilename;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRTigerDataSource::TestCapability( const char *pszCap )

{
    if( EQUAL(pszCap,ODsCCreateLayer) )
        return GetWriteMode();
    else
        return FALSE;
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

int OGRTigerDataSource::Create( const char *pszNameIn, char **papszOptions )

{
    VSIStatBufL      stat;
    
/* -------------------------------------------------------------------- */
/*      Try to create directory if it doesn't already exist.            */
/* -------------------------------------------------------------------- */
    if( VSIStatL( pszNameIn, &stat ) != 0 )
    {
        VSIMkdir( pszNameIn, 0755 );
    }

    if( VSIStatL( pszNameIn, &stat ) != 0 || !VSI_ISDIR(stat.st_mode) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "%s is not a directory, nor can be directly created as one.",
                  pszName );
        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Store various information.                                      */
/* -------------------------------------------------------------------- */
    pszPath = CPLStrdup( pszNameIn );
    pszName = CPLStrdup( pszNameIn );
    bWriteMode = TRUE;

    SetOptionList( papszOptions );

/* -------------------------------------------------------------------- */
/*      Work out the version.                                           */
/* -------------------------------------------------------------------- */
//    nVersionCode = 1000; /* census 2000 */

    nVersionCode = 1002; /* census 2002 */
    if( GetOption("VERSION") != NULL )
    {
        nVersionCode = atoi(GetOption("VERSION"));
        nVersionCode = MAX(0,MIN(9999,nVersionCode));
    }
    nVersion = TigerClassifyVersion(nVersionCode);

    return TRUE;
}

/************************************************************************/
/*                            CreateLayer()                             */
/************************************************************************/

OGRLayer *OGRTigerDataSource::CreateLayer( const char *pszLayerName, 
                                           OGRSpatialReference *poSpatRef, 
                                           OGRwkbGeometryType eGType, 
                                           char **papszOptions )

{
    OGRTigerLayer       *poLayer = NULL;

    if( GetLayer( pszLayerName ) != NULL )
        return GetLayer( pszLayerName );

    if( poSpatRef != NULL && 
        (!poSpatRef->IsGeographic() 
         || !EQUAL(poSpatRef->GetAttrValue("DATUM"),
                   "North_American_Datum_1983")) )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "Requested coordinate system wrong for Tiger, " 
                  "forcing to GEOGCS NAD83." );
    }

    if( EQUAL(pszLayerName,"PIP") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerPIP( this, NULL ) );
    }
    else if( EQUAL(pszLayerName,"ZipPlus4") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerZipPlus4( this, NULL ) );
    }
    else if( EQUAL(pszLayerName,"TLIDRange") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerTLIDRange( this, NULL ) );
    }
    else if( EQUAL(pszLayerName,"PolyChainLink") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerPolyChainLink( this, NULL ) );
    }
    else if( EQUAL(pszLayerName,"CompleteChain") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerCompleteChain( this, NULL ) );
    }
    else if( EQUAL(pszLayerName,"AltName") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerAltName( this, NULL ) );
    }
    else if( EQUAL(pszLayerName,"FeatureIds") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerFeatureIds( this, NULL ) );
    }
    else if( EQUAL(pszLayerName,"ZipCodes") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerZipCodes( this, NULL ) );
    }
    else if( EQUAL(pszLayerName,"Landmarks") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerLandmarks( this, NULL ) );
    }
    else if( EQUAL(pszLayerName,"AreaLandmarks") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerAreaLandmarks( this, NULL ) );
    }
    else if( EQUAL(pszLayerName,"KeyFeatures") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerKeyFeatures( this, NULL ) );
    }
    else if( EQUAL(pszLayerName,"EntityNames") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerEntityNames( this, NULL ) );
    }
    else if( EQUAL(pszLayerName,"IDHistory") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerIDHistory( this, NULL ) );
    }
    else if( EQUAL(pszLayerName,"Polygon") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerPolygon( this, NULL ) );
    }

    else if( EQUAL(pszLayerName,"PolygonCorrections") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerPolygonCorrections( this, NULL ) );
    }

    else if( EQUAL(pszLayerName,"PolygonEconomic") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerPolygonEconomic( this, NULL ) );
    }

    else if( EQUAL(pszLayerName,"SpatialMetadata") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerSpatialMetadata( this, NULL ) );
    }

    else if( EQUAL(pszLayerName,"ZeroCellID") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerZeroCellID( this, NULL ) );
    }

    else if( EQUAL(pszLayerName,"OverUnder") )
    {
        poLayer = new OGRTigerLayer( this,
                                     new TigerOverUnder( this, NULL ) );
    }

    if( poLayer == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Unable to create layer %s, not a known TIGER/Line layer.",
                  pszLayerName );
    }
    else
        AddLayer( poLayer );

    return poLayer;
}

/************************************************************************/
/*                         DeleteModuleFiles()                          */
/************************************************************************/

void OGRTigerDataSource::DeleteModuleFiles( const char *pszModule )

{
    char        **papszDirFiles = CPLReadDir( GetDirPath() );
    int         i, nCount = CSLCount(papszDirFiles);
    
    for( i = 0; i < nCount; i++ )
    {
        if( EQUALN(pszModule,papszDirFiles[i],strlen(pszModule)) )
        {
            const char  *pszFilename;

            pszFilename = CPLFormFilename( GetDirPath(), 
                                           papszDirFiles[i], 
                                           NULL );
            if( VSIUnlink( pszFilename ) != 0 )
            {
                CPLDebug( "OGR_TIGER", "Failed to unlink %s", pszFilename );
            }
        }
    }

    CSLDestroy( papszDirFiles );
}
