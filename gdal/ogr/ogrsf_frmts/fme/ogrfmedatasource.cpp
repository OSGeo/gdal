/******************************************************************************
 * $Id$
 *
 * Project:  FMEObjects Translator
 * Purpose:  Implementations of the OGRFMEDataSource class.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, 2001, 2002 Safe Software Inc.
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
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#include "fme2ogr.h"
//#include "ogr2fme.h"
#include "cpl_conv.h"
#include "cpl_string.h"
//#include "ogrfme_cdsys.h"
#include "cpl_multiproc.h"
#include <idialog.h>
#include <ilogfile.h>
#include <icrdsysmgr.h>

const char* kPROVIDERNAME = "FME_OLEDB";

CPL_CVSID("$Id$");

#ifdef WIN32
#define FMEDLL_NAME "fme.dll"
#define PATH_CHAR '\\'
#else
#define FMEDLL_NAME "libfmeobj.so"
#define PATH_CHAR '/'
#endif

static IFMESession      *poSharedSession = NULL;
static int               nSharedSessionRefCount = 0;
static void             *hSessionMutex = NULL;

typedef struct {
    IFMEUniversalReader      *poReader;
    char                     *pszReaderType;
    char                     *pszDefinition;
} CachedConnection;

static int              nCachedConnectionCount = 0;
static CachedConnection *pasCachedConnections = NULL;

typedef struct {
    OGREnvelope  sExtent;
    char         *pszIndFile;
    char         *pszCoordSys;
    IFMESpatialIndex *poIndex;
    OGRwkbGeometryType eBestGeomType;
} CacheLayerInfo;

/************************************************************************/
/*                             FME_Logger()                             */
/*                                                                      */
/*      Output that would normally go to the FME log file will          */
/*      instead be redirected through this function.                    */
/************************************************************************/

void FME_Logger( FME_MsgLevel severity, const char *message )

{
    char *pszMessageCopy = CPLStrdup(message);

    if( pszMessageCopy[strlen(pszMessageCopy)-1] == '\n' )
        pszMessageCopy[strlen(pszMessageCopy)-1] = '\0';
        
    CPLDebug( "FME_LOG", "%d:%s", severity, pszMessageCopy );

    CPLFree( pszMessageCopy );
}

/************************************************************************/
/*                             GetTmpDir()                              */
/************************************************************************/

static const char *GetTmpDir()

{
    const char     *pszTmpDir;

    pszTmpDir = getenv("OGRFME_TMPDIR");
    if( pszTmpDir == NULL )
        pszTmpDir = getenv("TMPDIR");
    if( pszTmpDir == NULL )
        pszTmpDir = getenv("TEMPDIR");
    if( pszTmpDir == NULL ) //20020419 - ryan
        pszTmpDir = getenv("TMP");
    if( pszTmpDir == NULL )
        pszTmpDir = getenv("TEMP");
    if( pszTmpDir == NULL )
    {
#ifdef WIN32
        pszTmpDir = "C:\\";
#else
        pszTmpDir = "/tmp";
#endif
    }

    return pszTmpDir;
}

/************************************************************************/
/*                            BuildTmpNam()                             */
/*                                                                      */
/*      Create a basename for the temporary file for a given layer      */
/*      on this dataset.                                                */
/************************************************************************/

static char *BuildTmpNam( const char *pszLayerName )

{
    int            i;
    char           szFilename[2048];
    VSIStatBuf     sStat;
    const char     *pszTmpDir = GetTmpDir();

/* -------------------------------------------------------------------- */
/*      Look for an unused name.                                        */
/* -------------------------------------------------------------------- */
    for( i = -1; TRUE; i++ )
    {
        if( i == -1 )
            sprintf( szFilename, "%s%c%s_%s", 
                     pszTmpDir, PATH_CHAR, kPROVIDERNAME, pszLayerName );
        else
            sprintf( szFilename, "%s%c%s_%s_%d", 
                     pszTmpDir, PATH_CHAR, kPROVIDERNAME, pszLayerName, i );
        
        if( VSIStat( szFilename, &sStat ) != 0 )
            break;
    }

    return CPLStrdup( szFilename );
}

/************************************************************************/
/*                          OGRFMEDataSource()                          */
/************************************************************************/

OGRFMEDataSource::OGRFMEDataSource()

{
    pszName = NULL;
    pszDataset = NULL;
    pszReaderName = NULL;
    poSession = NULL;
    poReader = NULL;
    poFMEFeature = NULL;

    nLayers = 0;
    papoLayers = NULL;

    poUserDirectives = NULL;

    bUseCaching = FALSE;
}

/************************************************************************/
/*                         ~OGRFMEDataSource()                          */
/************************************************************************/

OGRFMEDataSource::~OGRFMEDataSource()

{
    CPLDebug( kPROVIDERNAME, "~OGRFMEDataSource(): %p", this );

    if( poSharedSession == NULL )
        return;

    AcquireSession();

/* -------------------------------------------------------------------- */
/*      Destroy the layers, so we know we don't still have the          */
/*      caches open when we dereference them.                           */
/* -------------------------------------------------------------------- */
    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];
    
    CPLFree( papoLayers );

/* -------------------------------------------------------------------- */
/*      If we have a cached instances, decrement the reference count.   */
/* -------------------------------------------------------------------- */
#ifdef SUPPORT_PERSISTENT_CACHE
    {
        OGRFMECacheIndex   oCacheIndex( 
            CPLFormFilename(GetTmpDir(), "ogrfmeds", "ind" ) );

        if( pszReaderName != NULL && nLayers > 0 
            && bUseCaching && oCacheIndex.Lock() && oCacheIndex.Load() )
        {
            CPLXMLNode        *psMatchDS = NULL;

            psMatchDS = oCacheIndex.FindMatch( pszReaderName, pszDataset, 
                                               *poUserDirectives );

            if( psMatchDS != NULL )
                oCacheIndex.Dereference( psMatchDS );

            if( oCacheIndex.ExpireOldCaches( poSession ) || psMatchDS != NULL )
                oCacheIndex.Save();

            oCacheIndex.Unlock();
        }
    }
#endif /* def SUPPORT_PERSISTENT_CACHE */

/* -------------------------------------------------------------------- */
/*      Cleanup up various resources.                                   */
/* -------------------------------------------------------------------- */
    if( poFMEFeature != NULL )
        poSession->destroyFeature( poFMEFeature );

    if( poUserDirectives != NULL )
        poSession->destroyStringArray( poUserDirectives );

    if( poReader != NULL )
    {
        if( !IsPartOfConnectionCache( poReader ) )
            poSession->destroyReader( poReader );
        else
            CPLDebug( kPROVIDERNAME, "Preserving cached reader on destructor");
    }
    
    if( poSession != NULL )
    {
        if( --nSharedSessionRefCount == 0 )
        {
#ifdef SUPPORT_CLEANUP_SESSION
#ifdef SUPPORT_INDIRECT_FMEDLL
            int (*pfnFME_destroySession)(void *);

            pfnFME_destroySession = (int (*)(void*)) 
                CPLGetSymbol(FMEDLL_NAME, "FME_DestroySession" );
            if( pfnFME_destroySession == NULL )
                CPLError( CE_Warning, CPLE_AppDefined, 
                          "Failed to fetch FME_DestroySession entry point." );
            else
                pfnFME_destroySession( (void *) (&poSession) );
#else
            FME_destroySession( poSession );
#endif // def SUPPORT_INDIRECT_FMEDLL
            poSharedSession = NULL;
#else // ndef SUPPORT_CLEANUP_SESSION
            CPLDebug( kPROVIDERNAME, "no active datasources left, but preserving session." );
#endif
        }
    }

    CPLFree( pszName );
    CPLFree( pszDataset );
    CPLFree( pszReaderName );

    ReleaseSession();
}

/************************************************************************/
/*                          PromptForSource()                           */
/************************************************************************/

char *OGRFMEDataSource::PromptForSource()

{
    IFMEDialog      *poDialog = NULL;
    IFMEString      *poSourceFormat, *poSourceDSName;
    char            *pszResult = NULL;

    poSourceFormat = poSession->createString();
    poSourceDSName = poSession->createString();

    if( poSession->createDialog( poDialog ) != 0 )
        return NULL;

    poUserDirectives->append( "SPATIAL_SETTINGS" );
    poUserDirectives->append( "no" );
    if( poDialog->sourcePrompt( NULL, NULL, *poSourceFormat, *poSourceDSName,
                                *poUserDirectives ) )
    {
        pszResult = CPLStrdup(CPLSPrintf("%s:%s", 
                                         poSourceFormat->data(), 
                                         poSourceDSName->data()));
    }

    poSession->destroyString( poSourceFormat );
    poSession->destroyString( poSourceDSName );
        
    return pszResult;
}

/************************************************************************/
/*                           ReadFileSource()                           */
/************************************************************************/

char *OGRFMEDataSource::ReadFileSource( const char *pszFilename )

{
    FILE            *fp;
    char            **papszLines = NULL;
    const char      *pszLine;

/* -------------------------------------------------------------------- */
/*      Read the definition file.                                       */
/* -------------------------------------------------------------------- */
    fp = VSIFOpen( pszFilename, "rt" );
    if( fp == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Failed to open file %s.", 
                  pszFilename );
        return NULL;
    }

    while( (pszLine = CPLReadLine(fp)) != NULL )
    {
        if( *pszLine != '#' )
            papszLines = CSLAddString( papszLines, pszLine );
    }

    VSIFClose( fp );

/* -------------------------------------------------------------------- */
/*      verify minimal requirements.                                    */
/* -------------------------------------------------------------------- */
    if( CSLCount(papszLines) < 2 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Insufficient lines in FME Data Definition file."
                  "At least a readername and data source name is required." );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Apply extra values to user directives.                          */
/* -------------------------------------------------------------------- */
    int            i;

    for( i = 2; papszLines[i] != NULL; i++ )
        poUserDirectives->append( papszLines[i] );

/* -------------------------------------------------------------------- */
/*      Prepare reader:dataset response string.                         */
/* -------------------------------------------------------------------- */
    char     *pszReturn;

    pszReturn = CPLStrdup(CPLSPrintf("%s:%s", papszLines[0], papszLines[1] ));

    CSLDestroy( papszLines );

    return pszReturn;
}

/************************************************************************/
/*                         SaveDefinitionFile()                         */
/************************************************************************/

static void SaveDefinitionFile( const char *pszFilename, 
                                const char *pszReader, 
                                const char *pszDatasource,
                                IFMEStringArray &oUserDirectives )

{
    FILE     *fp;
    int      i;

    fp = VSIFOpen( CPLResetExtension( pszFilename, "fdd" ), "wt" );
    if( fp == NULL )
        return;

    fprintf( fp, "%s\n", pszReader );
    fprintf( fp, "%s\n", pszDatasource );

    for( i = 0; i < (int) oUserDirectives.entries(); i++ )
    {
        fprintf( fp, "%s\n", oUserDirectives(i) );
    }

    VSIFClose( fp );
}

/************************************************************************/
/*                             ExtractSRS()                             */
/************************************************************************/

OGRSpatialReference *
OGRFMEDataSource::ExtractSRS()

{
/* -------------------------------------------------------------------- */
/*      Try to find the COORDSYS in the user directives.                */
/* -------------------------------------------------------------------- */
    const char *pszCoordSys = NULL;
    for( int i = 0; i < (int) poUserDirectives->entries(); i += 2 )
    {
        if( EQUAL((*poUserDirectives)(i),"COORDSYS") )
            pszCoordSys = (*poUserDirectives)(i+1);
    }

    if( pszCoordSys == NULL || strlen(pszCoordSys) == 0 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Translate FME name to an OGRSpatialReference.                   */
/* -------------------------------------------------------------------- */
    return FME2OGRSpatialRef( pszCoordSys );
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRFMEDataSource::Open( const char * pszCompositeName )

{
    FME_MsgNum          err;

    CPLAssert( poSession == NULL );  // only open once

/* -------------------------------------------------------------------- */
/*      Do some initial validation.  Does this even look like it        */
/*      could plausibly be an FME suitable name?  We accept PROMPT:,    */
/*      <reader>: or anything ending in .fdd as a reasonable candidate. */
/* -------------------------------------------------------------------- */
    int  i;

    for( i = 0; pszCompositeName[i] != ':' && pszCompositeName[i] != '\0'; i++)
    {
        if( pszCompositeName[i] == '/' || pszCompositeName[i] == '\\'
            || pszCompositeName[i] == '.' )
            break;
    }
         
    if( (i < 2 || pszCompositeName[i] != ':' 
         || EQUALN(pszCompositeName,"OCI:",4)
         || EQUALN(pszCompositeName,"gltp:",5)
         || EQUALN(pszCompositeName,"http",4)
         || EQUALN(pszCompositeName,"DODS:",5)
         || EQUALN(pszCompositeName,"ODBC:",5)
         || EQUALN(pszCompositeName,"MYSQL:",5))
        && !EQUAL(CPLGetExtension( pszCompositeName ), "fdd")
        && !EQUALN(pszCompositeName,"PROMPT",6) )
    {
        CPLDebug( kPROVIDERNAME, 
                  "OGRFMEDataSource::Open(%s) don't try to open via FME.", 
                  pszCompositeName );
        return FALSE;
    }

    CPLDebug( kPROVIDERNAME, "OGRFMEDataSource::Open(%s):%p/%ld", 
              pszCompositeName, this, (long) CPLGetPID() );

/* -------------------------------------------------------------------- */
/*      Create an FME Session.                                          */
/* -------------------------------------------------------------------- */
    poSession = AcquireSession();
    if( poSession == NULL )
        return FALSE;

    nSharedSessionRefCount++;

    CPLDebug( kPROVIDERNAME, "%p:acquired session", this );

    poUserDirectives = poSession->createStringArray();

/* -------------------------------------------------------------------- */
/*      Redirect FME log messages through CPLDebug().                   */
/* -------------------------------------------------------------------- */
    IFMELogFile   *poLogFile = poSession->logFile();

    poLogFile->setFileName( NULL, FME_FALSE );
    poLogFile->setCallBack( FME_Logger );

    CPLDebug( kPROVIDERNAME, "%p:reset logfile", this );

/* -------------------------------------------------------------------- */
/*      Prompt for a source, if none is provided.                       */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszCompositeName,"") || EQUALN(pszCompositeName,"PROMPT",6) )
    {
        pszName = PromptForSource();
        if( pszName == NULL )
        {
            ReleaseSession();
            return FALSE;
        }
    }
    else if( CPLGetExtension( pszCompositeName ) != NULL
             && EQUAL(CPLGetExtension( pszCompositeName ),"fdd") )
    {
        pszName = ReadFileSource(pszCompositeName);
        if( pszName == NULL )
        {
            ReleaseSession();
            return FALSE;
        }
    }
    else
    {
        pszName = CPLStrdup( pszCompositeName );
    }

/* -------------------------------------------------------------------- */
/*      Extract the reader name and password compontents.  The          */
/*      reader name will be followed by a single colon and then the     */
/*      FME DATASET name.                                               */
/* -------------------------------------------------------------------- */
    for( i = 0; pszName[i] != '\0' && pszName[i] != ':'; i++ ) {}

    if( pszName[i] == '\0' || i < 2 )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Failed to parse reader and data source from:\n%s", 
                  pszName );
        ReleaseSession();
        return FALSE;
    }

    pszReaderName = CPLStrdup( pszName );
    pszReaderName[i] = '\0';

    pszDataset = CPLStrdup(pszName + i + 1);

    CPLDebug( kPROVIDERNAME, "%s:parsed out dataset", pszDataset );

/* -------------------------------------------------------------------- */
/*      If we prompted for a defintion that includes a file to save     */
/*      it to, do the save now.                                         */
/* -------------------------------------------------------------------- */
    if( EQUALN(pszCompositeName,"PROMPT:",7) 
        && strlen(pszCompositeName) > 7 )
    {
        SaveDefinitionFile( pszCompositeName+7, 
                            pszReaderName, pszDataset, 
                            *poUserDirectives );
    }

/* -------------------------------------------------------------------- */
/*      Is there a Coordsys statement in the user directives?           */
/* -------------------------------------------------------------------- */
    OGRSpatialReference *poSRS = ExtractSRS();

    CPLDebug( kPROVIDERNAME, "got the SRS parsed");

    bCoordSysOverride = poSRS != NULL;

/* -------------------------------------------------------------------- */
/*      Allocate an FME string, and feature for use here and            */
/*      elsewhere.                                                      */
/* -------------------------------------------------------------------- */
    poFMEFeature = poSession->createFeature();
    poFMEString = poSession->createString();

/* -------------------------------------------------------------------- */
/*      Are we going to use the direct access DB mechanism, or the      */
/*      spatiallly cached (dumb reader) mechanism.                      */
/* -------------------------------------------------------------------- */
    bUseCaching = !EQUALN(pszReaderName,"SDE",3) 
               && !EQUALN(pszReaderName,"ORACLE",6);

/* -------------------------------------------------------------------- */
/*      Is there already a cache for this dataset?  If so, we will      */
/*      use it.                                                         */
/* -------------------------------------------------------------------- */
#ifdef SUPPORT_PERSISTENT_CACHE
    OGRFMECacheIndex   oCacheIndex( 
                           CPLFormFilename(GetTmpDir(), "ogrfmeds", "ind" ) );
    CPLXMLNode        *psMatchDS = NULL;

    if( bUseCaching && oCacheIndex.Lock() && oCacheIndex.Load() )
    {
        int bNeedSave = oCacheIndex.ExpireOldCaches( poSession );

        psMatchDS = oCacheIndex.FindMatch( pszReaderName, pszDataset, 
                                           *poUserDirectives );

        if( psMatchDS != NULL )
        {
            oCacheIndex.Reference( psMatchDS );
            oCacheIndex.Save();

            psMatchDS = CPLCloneXMLTree( psMatchDS );
            oCacheIndex.Unlock();

        }
        else
        {
            if( bNeedSave )
                oCacheIndex.Save();

            oCacheIndex.Unlock();
        }

        if( psMatchDS != NULL )
        {
            if( InitializeFromXML( psMatchDS ) )
            {
                CPLDestroyXMLNode( psMatchDS );
                ReleaseSession();

                return TRUE;
            }

            CPLDestroyXMLNode( psMatchDS );
        }
    }
#endif /* def SUPPORT_PERSISTENT_CACHE */

/* -------------------------------------------------------------------- */
/*      Create a reader.                                                */
/* -------------------------------------------------------------------- */
    IFMEStringArray     *poParms = poSession->createStringArray();

    for( i = 0; i < (int) poUserDirectives->entries(); i++ )
        CPLDebug( kPROVIDERNAME, "oUserDirectives(%d) = '%s'", 
                  i, (*poUserDirectives)(i) );

    poReader = poSession->createReader(pszReaderName, FME_FALSE, 
                                       poUserDirectives);
    if( poReader == NULL )
    {
        CPLFMEError( poSession,
                     "Failed to create reader of type `%s'.\n",
                     pszReaderName );
        ReleaseSession();
        return FALSE;
    }

    CPLDebug( kPROVIDERNAME, "%p:reader created.", this );

/* -------------------------------------------------------------------- */
/*      Now try to open the dataset.                                    */
/* -------------------------------------------------------------------- */
    err = poReader->open( pszDataset, *poParms );
    if( err )
    {
        CPLFMEError( poSession,
                     "Failed to open dataset `%s' with reader of type `%s'.\n",
                     pszDataset, pszReaderName );
        ReleaseSession();
        return FALSE;
    }

    CPLDebug( kPROVIDERNAME, "%p:reader opened.", this );

/* -------------------------------------------------------------------- */
/*      There are some circumstances where we want to keep a            */
/*      "connection" open for a data source.  Offer this reader for     */
/*      connection caching.                                             */
/* -------------------------------------------------------------------- */
    OfferForConnectionCaching( poReader, pszReaderName, 
                               pszDataset );

/* -------------------------------------------------------------------- */
/*      Create a layer for each schema feature.                         */
/* -------------------------------------------------------------------- */
    FME_Boolean         eEndOfSchema;

    while( TRUE )
    {
        err = poReader->readSchema( *poFMEFeature, eEndOfSchema );
        if( err )
        {
            CPLFMEError( poSession, "IFMEReader::readSchema() failed." );
            ReleaseSession();
            return FALSE;
        }

        if( eEndOfSchema == FME_TRUE )
            break;

        CPLDebug( kPROVIDERNAME, "%p:readSchema() got %s.", 
                  this, poFMEFeature->getFeatureType() );

        OGRFMELayer     *poNewLayer = NULL;

        if( bUseCaching )
            poNewLayer = new OGRFMELayerCached( this );
        else
            poNewLayer = new OGRFMELayerDB( this, pszReaderName, pszDataset,
                                            poUserDirectives );

        if( !poNewLayer->Initialize( poFMEFeature, poSRS ) )
        {
            CPLDebug( kPROVIDERNAME, "%p:Initialize() failed.", this );
            delete poNewLayer;
            ReleaseSession();
            return FALSE;
        }

        papoLayers = (OGRFMELayer **)
            CPLRealloc(papoLayers, sizeof(void*) * ++nLayers );
        papoLayers[nLayers-1] = poNewLayer;
    }

    poSession->destroyStringArray( poParms );

    if( poSRS != NULL )
        poSRS->Release();

    CPLDebug( kPROVIDERNAME, "%p:schema read.", this );

/* -------------------------------------------------------------------- */
/*      Do we want to build our own index/caches for each layer?        */
/* -------------------------------------------------------------------- */
    if( bUseCaching )
        BuildSpatialIndexes();

    CPLDebug( kPROVIDERNAME, "%p:Open() successful.", this );

    ReleaseSession();

/* -------------------------------------------------------------------- */
/*      If we are caching, add this cache to the cache index.           */
/* -------------------------------------------------------------------- */
#ifdef SUPPORT_PERSISTENT_CACHE
    if( bUseCaching && oCacheIndex.Lock() && oCacheIndex.Load() )
    {
        CPLXMLNode *psXML = SerializeToXML();

        oCacheIndex.Add( psXML  ); // cache index takes ownership of tree
        oCacheIndex.Reference( psXML );
        oCacheIndex.Save();
        oCacheIndex.Unlock();
    }
#endif

    return TRUE;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRFMEDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer >= nLayers )
        return NULL;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                        BuildSpatialIndexes()                         */
/*                                                                      */
/*      Import all the features, building per-layer spatial             */
/*      caches with indexing.                                           */
/************************************************************************/

void OGRFMEDataSource::BuildSpatialIndexes()

{
    CacheLayerInfo     *pasCLI;
    int                iLayer;

    pasCLI = (CacheLayerInfo *) CPLCalloc(sizeof(CacheLayerInfo),nLayers);

/* -------------------------------------------------------------------- */
/*      Create index files with "temp file" names.                      */
/* -------------------------------------------------------------------- */
    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        CacheLayerInfo *psCLI = pasCLI + iLayer;

        psCLI->pszCoordSys = NULL;

        psCLI->pszIndFile = 
            BuildTmpNam( papoLayers[iLayer]->GetLayerDefn()->GetName() );

        psCLI->poIndex = 
            poSession->createSpatialIndex( psCLI->pszIndFile, "WRITE", NULL );
        
        if( psCLI->poIndex == NULL || psCLI->poIndex->open() != 0 )
        {
            CPLDebug( kPROVIDERNAME, 
                      "Serious error creating or opening spatial index ... bailing." );
            return;
        }

        // our special marker meaning unset.
        psCLI->eBestGeomType = (OGRwkbGeometryType) 500; 
    }

/* -------------------------------------------------------------------- */
/*      Read all features, and store them into appropriate spatial      */
/*      indexes.                                                        */
/* -------------------------------------------------------------------- */
    while( ReadFMEFeature() )
    {
        CacheLayerInfo *psCLI = NULL;

        poFMEFeature->getFeatureType( *poFMEString );
        
        for( iLayer = 0; iLayer < nLayers; iLayer++ )
        {
            if( EQUAL(papoLayers[iLayer]->GetLayerDefn()->GetName(),
                      poFMEString->data()) )
            {
                psCLI = pasCLI + iLayer;
                break;
            }
        }
        
        if( psCLI == NULL )
        {
            CPLDebug( "FME_LOG", 
                      "Skipping %s feature, doesn't match a layer.", 
                      poFMEString->data() );
            continue;
        }

        psCLI->poIndex->store( *poFMEFeature );

        // Aggregate to extents.
        FME_Real64  dfMinX, dfMaxX, dfMinY, dfMaxY;

        poFMEFeature->boundingBox( dfMinX, dfMaxX, dfMinY, dfMaxY );
        
        if( psCLI->poIndex->entries() == 1 )
        {
            psCLI->sExtent.MinX = dfMinX;
            psCLI->sExtent.MaxX = dfMaxX;
            psCLI->sExtent.MinY = dfMinY;
            psCLI->sExtent.MaxY = dfMaxY;
        }
        else
        {
            psCLI->sExtent.MinX = MIN(psCLI->sExtent.MinX,dfMinX);
            psCLI->sExtent.MaxX = MAX(psCLI->sExtent.MaxX,dfMaxX);
            psCLI->sExtent.MinY = MIN(psCLI->sExtent.MinY,dfMinY);
            psCLI->sExtent.MaxY = MAX(psCLI->sExtent.MaxY,dfMaxY);
        }

        // Update best geometry type to use based on this geometry.
        ClarifyGeometryClass( poFMEFeature, psCLI->eBestGeomType );

        // Check on coordsys.
        if( poFMEFeature->getCoordSys() != NULL
            && strlen(poFMEFeature->getCoordSys()) > 0 )
        {
            if( psCLI->pszCoordSys == NULL )
                psCLI->pszCoordSys = CPLStrdup(poFMEFeature->getCoordSys());
            else
            {
                if( !EQUAL(psCLI->pszCoordSys,poFMEFeature->getCoordSys()) )
                    CPLDebug( "FME_OLEDB", 
                              "Conflicting coordsys %s (vs. %s) on layer %s.",
                              poFMEFeature->getCoordSys(), 
                              psCLI->pszCoordSys, 
                              papoLayers[iLayer]->GetLayerDefn()->GetName() );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Close indexes and assign to layers.                             */
/* -------------------------------------------------------------------- */
    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        OGRFMELayerCached * poLayer = (OGRFMELayerCached *) papoLayers[iLayer];
        CacheLayerInfo *psCLI = pasCLI + iLayer;

        // If there are no features, we destroy the layer.
        if( psCLI->poIndex->entries() == 0 )
        {
            CPLDebug( "FME_LOG", "Drop layer %s, there are not features.",
                      poLayer->GetLayerDefn()->GetName() );
            psCLI->poIndex->close(FME_TRUE);

            delete poLayer;
            papoLayers[iLayer] = NULL;
        }
        else
        {
            OGRSpatialReference *poSpatialRef = NULL;

            psCLI->poIndex->close(FME_FALSE);
            poSession->destroySpatialIndex( psCLI->poIndex );
            
            if( psCLI->pszCoordSys != NULL && !bCoordSysOverride )
            {
                CPLDebug("FME_OLEDB", 
                         "Applying COORDSYS=%s to layer %s from feature scan.",
                         psCLI->pszCoordSys, 
                         papoLayers[iLayer]->GetLayerDefn()->GetName() );
                       
                poSpatialRef = FME2OGRSpatialRef( psCLI->pszCoordSys );
            }

            poLayer->AssignIndex( psCLI->pszIndFile, &(psCLI->sExtent),
                                  poSpatialRef );
            if( psCLI->eBestGeomType != 500 
                && psCLI->eBestGeomType 
                         != poLayer->GetLayerDefn()->GetGeomType() )
            {
                CPLDebug( "FME_LOG", "Setting geom type from %d to %d", 
                          poLayer->GetLayerDefn()->GetGeomType(),
                          psCLI->eBestGeomType );
                          
                poLayer->GetLayerDefn()->SetGeomType( psCLI->eBestGeomType );
            }
        }
            
        CPLFree( psCLI->pszIndFile );
        CPLFree( psCLI->pszCoordSys );
    }

    CPLFree( pasCLI );

/* -------------------------------------------------------------------- */
/*      Compress missing layers from the layer list.                    */
/* -------------------------------------------------------------------- */
    for( iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        if( papoLayers[iLayer] == NULL )
        {
            papoLayers[iLayer] = papoLayers[nLayers-1];
            nLayers--;
            iLayer--;
        }
    }
}

/************************************************************************/
/*                        ClarifyGeometryClass()                        */
/*                                                                      */
/*      Examine an FME features geometry, and ensure the wkb            */
/*      geometry type we are using will include it.  That is, we        */
/*      generally make the geometry type sufficiently general to        */
/*      include the type of geometry of this feature.                   */
/*                                                                      */
/*      Exceptions are when the existing type is unknown, in which      */
/*      case we assign it the type of the geometry we find, and if      */
/*      it is a mixture of polygon and multipolygon in which case we    */
/*      use multipolygon with the understanding that we will force      */
/*      the polygons to multipolygon.                                   */
/************************************************************************/

void OGRFMEDataSource::ClarifyGeometryClass( 
    IFMEFeature *poFeature,
    OGRwkbGeometryType &eBestGeomType )

{
    FME_GeometryType      eFMEType = poFeature->getGeometryType();
    OGRwkbGeometryType    eThisType;

/* -------------------------------------------------------------------- */
/*      Classify this FME geometry.  The hard case is aggregate.        */
/* -------------------------------------------------------------------- */
    if( eFMEType == FME_GEOM_POINT )
        eThisType = wkbPoint;
    else if( eFMEType == FME_GEOM_LINE )
        eThisType = wkbLineString;
    else if( eFMEType == FME_GEOM_POLYGON || eFMEType == FME_GEOM_DONUT )
        eThisType = wkbPolygon;
    else if( eFMEType == FME_GEOM_AGGREGATE )
    {
        // This is the hard case!  Split the aggregate to see if we can
        // categorize it more specifically.
        OGRwkbGeometryType eComponentType = (OGRwkbGeometryType) 500;
        IFMEFeatureVector *poFeatVector;

        poFeatVector = poSession->createFeatureVector();
        
        poFeature->splitAggregate( *poFeatVector );
        
        for( int iPart = 0; iPart < (int)poFeatVector->entries(); iPart++ )
        {
            IFMEFeature      *poFMEPart = (*poFeatVector)(iPart);

            if( poFMEPart != NULL )
                ClarifyGeometryClass( poFMEPart, eComponentType );
        }

        poSession->destroyFeatureVector( poFeatVector );

        if( wkbFlatten(eComponentType) == wkbPolygon )
            eThisType = wkbMultiPolygon;
        else if( wkbFlatten(eComponentType) == wkbPoint )
            eThisType = wkbMultiPoint;
        else if( wkbFlatten(eComponentType) == wkbLineString )
            eThisType = wkbMultiLineString;
        else
            eThisType = wkbGeometryCollection;
    }
    else 
        eThisType = wkbUnknown;

    // Is this 3D?
    if( poFeature->getDimension() == FME_THREE_D )
        eThisType = (OGRwkbGeometryType) (eThisType | wkb25DBit);
    
/* -------------------------------------------------------------------- */
/*      Now adjust the working type.                                    */
/* -------------------------------------------------------------------- */
    OGRwkbGeometryType eNewBestGeomType = eBestGeomType;

    if( eBestGeomType == 500 )
        eNewBestGeomType = eThisType;
    else if( eThisType == wkbNone )
        /* do nothing */;
    else if( wkbFlatten(eThisType) == wkbFlatten(eBestGeomType) )
        /* no change */;
    else if( wkbFlatten(eThisType) == wkbPolygon 
             && wkbFlatten(eBestGeomType) == wkbMultiPolygon )
        /* do nothing */;
    else if( wkbFlatten(eThisType) == wkbMultiPolygon 
             && wkbFlatten(eBestGeomType) == wkbPolygon )
        eNewBestGeomType = wkbMultiPolygon;
    else if( wkbFlatten(eThisType) >= 4 && wkbFlatten(eThisType) <= 7 
          && wkbFlatten(eBestGeomType) >= 4 && wkbFlatten(eBestGeomType) <= 7 )
        /* they are both collections, but not the same ... go to generic coll*/
        eNewBestGeomType = wkbGeometryCollection;
    else
        eNewBestGeomType = wkbUnknown;

    if( ((eBestGeomType & wkb25DBit) || (eThisType & wkb25DBit)) 
        && (int) eNewBestGeomType != 500 )
    {
        eNewBestGeomType = (OGRwkbGeometryType)(((int) eBestGeomType) | wkb25DBit);
    } 

    eBestGeomType = eNewBestGeomType;
}

/************************************************************************/
/*                           ReadFMEFeature()                           */
/*                                                                      */
/*      Internal working function to read an FME feature into the       */
/*      poFMEFeature object, and increment the nPreviousFeature         */
/*      counter.  Returns FALSE on end of input, or on error.           */
/************************************************************************/

int OGRFMEDataSource::ReadFMEFeature()

{
    FME_Boolean     eEndOfSchema;
    FME_MsgNum      err;

    poFMEFeature->resetFeature();
    err = poReader->read( *poFMEFeature, eEndOfSchema );

    if( err )
    {
        CPLFMEError( poSession, "Error while reading feature." );
        return FALSE;
    }

    if( eEndOfSchema == FME_TRUE )
    {
        return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                          ProcessGeometry()                           */
/*                                                                      */
/*      Translate an FME geometry into an OGR geometry.                 */
/************************************************************************/

OGRGeometry *
OGRFMEDataSource::ProcessGeometry( OGRFMELayer * poLayer, 
                                   IFMEFeature * poGeomFeat,
                                   OGRwkbGeometryType eDesiredType  )
{
    
    FME_GeometryType      eGeomType = poGeomFeat->getGeometryType();
    int                   bForceToMulti = FALSE;
    
    if( wkbFlatten(eDesiredType) == wkbGeometryCollection
        || wkbFlatten(eDesiredType) == wkbMultiPolygon )
        bForceToMulti = TRUE;

/* -------------------------------------------------------------------- */
/*      Point                                                           */
/* -------------------------------------------------------------------- */
    if( eGeomType == FME_GEOM_POINT )
    {
        return
            new OGRPoint(poGeomFeat->getXCoordinate(0),
                         poGeomFeat->getYCoordinate(0),
                         poGeomFeat->getZCoordinate(0));
    }

/* -------------------------------------------------------------------- */
/*      Line                                                            */
/* -------------------------------------------------------------------- */
    else if( eGeomType == FME_GEOM_LINE )
    {
        OGRLineString *poLine = new OGRLineString();

        poLine->setNumPoints( poGeomFeat->numCoords() );
        
        for( int iPoint = 0; iPoint < (int) poGeomFeat->numCoords(); iPoint++ )
        {
            poLine->setPoint( iPoint, 
                              poGeomFeat->getXCoordinate(iPoint),
                              poGeomFeat->getYCoordinate(iPoint),
                              poGeomFeat->getZCoordinate(iPoint) );
        }

        return poLine;
    }

/* -------------------------------------------------------------------- */
/*      Polygon                                                         */
/* -------------------------------------------------------------------- */
    else if( eGeomType == FME_GEOM_POLYGON )
    {
        OGRLinearRing *poLine = new OGRLinearRing();
        OGRPolygon *poPolygon = new OGRPolygon();

        poLine->setNumPoints( poGeomFeat->numCoords() );
        
        for( int iPoint = 0; iPoint < (int)poGeomFeat->numCoords(); iPoint++ )
        {
            poLine->setPoint( iPoint, 
                              poGeomFeat->getXCoordinate(iPoint),
                              poGeomFeat->getYCoordinate(iPoint),
                              poGeomFeat->getZCoordinate(iPoint) );
        }
        poPolygon->addRingDirectly( poLine );

        if( !bForceToMulti )
            return poPolygon;

        OGRMultiPolygon *poMP = new OGRMultiPolygon();

        poMP->addGeometryDirectly( poPolygon );

        return poMP;
    }

/* -------------------------------------------------------------------- */
/*      Donut                                                           */
/* -------------------------------------------------------------------- */
    else if( eGeomType == FME_GEOM_DONUT )
    {
        OGRPolygon      *poPolygon = new OGRPolygon();
        IFMEFeatureVector *poFeatVector;
        IFMEFeature      *poFMERing = NULL;

        poFeatVector = poSession->createFeatureVector();
        
        poGeomFeat->getDonutParts( *poFeatVector );
        
        for( int iPart = 0; iPart < (int)poFeatVector->entries(); iPart++ )
        {
            OGRLinearRing      *poRing;

            poFMERing = (*poFeatVector)(iPart);
            if( poFMERing == NULL )
                continue;

            poRing = new OGRLinearRing();
            
            poRing->setNumPoints( poFMERing->numCoords() );
        
            for( int iPoint=0; iPoint < (int)poFMERing->numCoords(); iPoint++ )
            {
                poRing->setPoint( iPoint, 
                                  poFMERing->getXCoordinate(iPoint),
                                  poFMERing->getYCoordinate(iPoint),
                                  poFMERing->getZCoordinate(iPoint) );
            }

            poPolygon->addRingDirectly( poRing );
        }

        poFeatVector->clearAndDestroy();
        poSession->destroyFeatureVector( poFeatVector );

        if( !bForceToMulti )
            return poPolygon;

        OGRMultiPolygon *poMP = new OGRMultiPolygon();

        poMP->addGeometryDirectly( poPolygon );

        return poMP;
    }

/* -------------------------------------------------------------------- */
/*      Aggregate                                                       */
/* -------------------------------------------------------------------- */
    else if( eGeomType == FME_GEOM_AGGREGATE )
    {
        OGRGeometryCollection *poCollection;
        IFMEFeatureVector *poFeatVector;
        OGRwkbGeometryType eSubType = wkbUnknown;

        if( bForceToMulti && eDesiredType == wkbMultiPolygon )
        {
            poCollection = new OGRMultiPolygon();
            eSubType = wkbPolygon;
        }
        else
            poCollection = new OGRGeometryCollection();

        poFeatVector = poSession->createFeatureVector();
        
        poGeomFeat->splitAggregate( *poFeatVector );
        
        for( int iPart = 0; iPart < (int)poFeatVector->entries(); iPart++ )
        {
            OGRGeometry      *poOGRPart;
            IFMEFeature      *poFMEPart;

            poFMEPart = (*poFeatVector)(iPart);
            if( poFMEPart == NULL )
                continue;

            poOGRPart = ProcessGeometry( poLayer, poFMEPart, eSubType );
            if( poOGRPart == NULL )
                continue;

            poCollection->addGeometryDirectly( poOGRPart );
        }

        poSession->destroyFeatureVector( poFeatVector );

        return poCollection;
    }

    else if( eGeomType == FME_GEOM_UNDEFINED )
    {
        return NULL;
    }
    else
    {
        CPLDebug( kPROVIDERNAME, 
                  "unable to translate unsupported geometry type: %d\n",
                  eGeomType  );

        return NULL;
    }
}
 
/************************************************************************/
/*                           ProcessFeature()                           */
/*                                                                      */
/*      Process the current fme feature into an OGR feature of the      */
/*      passed layer types.                                             */
/************************************************************************/

OGRFeature *OGRFMEDataSource::ProcessFeature( OGRFMELayer *poLayer,
                                              IFMEFeature *poSrcFeature )

{
    OGRFeatureDefn  *poDefn = poLayer->GetLayerDefn();
    OGRFeature      *poFeature = new OGRFeature( poDefn );
    int             iAttr;

/* -------------------------------------------------------------------- */
/*      Transfer attributes ... for numeric values assume the string    */
/*      representation is appropriate, and automatically                */
/*      translatable.  Eventually we will need special handling for     */
/*      array style fields.                                             */
/* -------------------------------------------------------------------- */
    for( iAttr = 0; iAttr < poDefn->GetFieldCount(); iAttr++ )
    {
        OGRFieldDefn      *poField = poDefn->GetFieldDefn(iAttr);

        if( poSrcFeature->getAttribute( poField->GetNameRef(), 
                                        *poFMEString ) == FME_TRUE )
        {
            poFeature->SetField( iAttr, poFMEString->data() );
        }
    }

/* -------------------------------------------------------------------- */
/*      Translate the geometry.                                         */
/* -------------------------------------------------------------------- */
    OGRGeometry      *poOGRGeom = NULL;
    
    poOGRGeom = ProcessGeometry( poLayer, poSrcFeature,
                                 poLayer->GetLayerDefn()->GetGeomType() );
    if( poOGRGeom != NULL )
        poFeature->SetGeometryDirectly( poOGRGeom );

    return poFeature;
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRFMEDataSource::TestCapability( const char * )

{
    return FALSE;
}

/************************************************************************/
/*                     OfferForConnectionCaching()                      */
/*                                                                      */
/*      Sometimes we want to keep a prototype reader open to            */
/*      maintain a connection, for instance to SDE where creating       */
/*      the connection is pretty expensive.                             */
/************************************************************************/

void OGRFMEDataSource::OfferForConnectionCaching(IFMEUniversalReader *poReader,
                                                 const char *pszReaderType,
                                                 const char *pszDataset)

{
/* -------------------------------------------------------------------- */
/*      For now we only cache SDE readers.                              */
/* -------------------------------------------------------------------- */
    if( !EQUALN(pszReaderType,"SDE",3) 
        && !EQUALN(pszReaderType,"ORACLE",6) )
        return;

/* -------------------------------------------------------------------- */
/*      We want to build a definition of this connection that           */
/*      indicates a unique connection.  For now we base it on the       */
/*      Server, UserName, Password, and Instance values.  We will       */
/*      pick these all out of the RUNTIME_MACROS if present.            */
/*                                                                      */
/*      First find the runtime macros.                                  */
/* -------------------------------------------------------------------- */
    const char *pszRuntimeMacros = NULL;
    int i;

    for( i = 0; i < (int) poUserDirectives->entries()-1; i += 2 )
    {
        if( EQUALN((const char *) (*poUserDirectives)(i),"RUNTIME_MACROS",14) )
            pszRuntimeMacros = (*poUserDirectives)(i+1);
    }
    
/* -------------------------------------------------------------------- */
/*      Break into name/value pairs.                                    */
/* -------------------------------------------------------------------- */
    char **papszTokens = NULL;

    if( pszRuntimeMacros != NULL )
        papszTokens = CSLTokenizeStringComplex( pszRuntimeMacros, ",", 
                                                TRUE, TRUE);

/* -------------------------------------------------------------------- */
/*      Look for Name values we want, and append them to the            */
/*      definition string.                                              */
/* -------------------------------------------------------------------- */
    char      szDefinition[5000];

    sprintf( szDefinition, "%s::", pszDataset );

    for( i = 0; i < CSLCount(papszTokens)-1; i += 2 )
    {
        if( strstr(papszTokens[i],"Server") != NULL
            || strstr(papszTokens[i],"Service") != NULL
            || strstr(papszTokens[i],"UserName") != NULL
            || strstr(papszTokens[i],"Password") != NULL
            || strstr(papszTokens[i],"Instance") != NULL )
        {
            if( strlen(papszTokens[i+1]) + strlen(papszTokens[i]) + 20
                < sizeof(szDefinition) - strlen(szDefinition) )
            {
                sprintf( szDefinition + strlen(szDefinition), "%s=%s;", 
                         papszTokens[i], papszTokens[i+1] );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Do we already have a reader cached for this definition?         */
/* -------------------------------------------------------------------- */
    for( i = 0; i < nCachedConnectionCount; i++ )
    {
        if( strcmp(szDefinition, pasCachedConnections[i].pszDefinition) == 0 )
            return;
    }
        
/* -------------------------------------------------------------------- */
/*      Added this reader to the cache.                                 */
/* -------------------------------------------------------------------- */
    CPLDebug( kPROVIDERNAME, 
              "Caching IFMEUniversalReader to maintain connection.\n"
              "ReaderType=%s, Definition=%s", 
              pszReaderType, szDefinition );

    nCachedConnectionCount++;
    pasCachedConnections = (CachedConnection *) 
        CPLRealloc(pasCachedConnections, 
                   sizeof(CachedConnection) * nCachedConnectionCount);
    
    pasCachedConnections[nCachedConnectionCount-1].poReader = poReader;
    pasCachedConnections[nCachedConnectionCount-1].pszReaderType = 
        CPLStrdup(pszReaderType);
    pasCachedConnections[nCachedConnectionCount-1].pszDefinition = 
        CPLStrdup(szDefinition);
}

/************************************************************************/
/*                      IsPartOfConnectionCache()                       */
/*                                                                      */
/*      I this reader being used to maintain a connection cache?        */
/************************************************************************/
int OGRFMEDataSource::IsPartOfConnectionCache( IFMEUniversalReader *poReader )

{
    int            i;
    
    for( i = 0; i < nCachedConnectionCount; i++ )
        if( poReader == pasCachedConnections[i].poReader )
            return TRUE;

    return FALSE;
}

/************************************************************************/
/*                           AcquireSession()                           */
/*                                                                      */
/*      Get unique ownership of the FME session for this thread.        */
/************************************************************************/

IFMESession *OGRFMEDataSource::AcquireSession()

{
    FME_MsgNum          err;

/* -------------------------------------------------------------------- */
/*      Create session mutex if we don't already have one.              */
/* -------------------------------------------------------------------- */
    if( hSessionMutex == NULL )
    {
        hSessionMutex = CPLCreateMutex();

        CPLDebug( kPROVIDERNAME, "%p:Creating FME session, mutex=%d.", 
                  this, hSessionMutex );
    }

/* -------------------------------------------------------------------- */
/*      Try to acquire ownership of the session, even if the session    */
/*      doesn't yet exist.                                              */
/* -------------------------------------------------------------------- */
    else
    {
#ifdef DEBUG_MUTEX
        CPLDebug( kPROVIDERNAME, "%p:Wait for session mutex.", this );
#endif

        if( !CPLAcquireMutex( hSessionMutex, 5.0 ) )
        {
            CPLDebug( kPROVIDERNAME, "%p:Failed to acquire session mutex in 5s.", 
                      this );
        }

#ifdef DEBUG_MUTEX
        else
            CPLDebug( kPROVIDERNAME, "%p:Got session mutex.", this );
#endif
    }

/* -------------------------------------------------------------------- */
/*      If the session doesn't exist, create it now.                    */
/* -------------------------------------------------------------------- */
    if( poSharedSession == NULL )
    {
#ifdef SUPPORT_INDIRECT_FMEDLL
        FME_MsgNum (*pfnFME_CreateSession)( void * );
        pfnFME_CreateSession = (FME_MsgNum (*)(void*)) 
            CPLGetSymbol( FMEDLL_NAME, "FME_CreateSession" );
        if( pfnFME_CreateSession == NULL )
        {
            CPLReleaseMutex( hSessionMutex );
            CPLDebug( kPROVIDERNAME, "Unable to load FME_CreateSession from %s, skipping FME Driver.", FMEDLL_NAME );
            return NULL;
        }

        err = pfnFME_CreateSession( (void *) (&poSharedSession) );
#else
        err = FME_createSession(poSharedSession);
#endif
        if( err )
        {
            poSharedSession = NULL;
            CPLReleaseMutex( hSessionMutex );
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to create FMESession." );
            return NULL;
        }

        // Dale Nov 26 '01 -- Set up to log "badnews" from FME
        // to help track down problems

        IFMEStringArray *poSessionDirectives = 
            poSharedSession->createStringArray();

        if( poSessionDirectives == NULL )
        {
            err = 1;
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Something has gone wonky with createStringArray() on the IFMESession.\n"
                      "Is it possible you built with gcc 3.2 on Linux?  This seems problematic." );

        }
        else
        {
            poSessionDirectives->append("FME_DEBUG");
            poSessionDirectives->append("BADNEWS");
            
            err = poSharedSession->init( poSessionDirectives );

            poSharedSession->destroyStringArray( poSessionDirectives );

            if( err )
            {
                CPLError( CE_Warning, CPLE_AppDefined,
                          "Failed to initialize FMESession.\n%s",
                          poSharedSession->getLastErrorMsg());
            }
        }

        if( err )
        {
#ifdef SUPPORT_INDIRECT_FMEDLL
            int (*pfnFME_destroySession)(void *);

            pfnFME_destroySession = (int (*)(void*)) 
                CPLGetSymbol(FMEDLL_NAME, "FME_DestroySession" );
            if( pfnFME_destroySession != NULL )
                pfnFME_destroySession( (void *) (&poSharedSession) );
#else
            FME_destroySession( poSharedSession );
#endif // def SUPPORT_INDIRECT_FMEDLL

            poSharedSession = NULL;
            CPLReleaseMutex( hSessionMutex );
            return NULL;
        }
    }

    return poSharedSession;
}

/************************************************************************/
/*                           ReleaseSession()                           */
/*                                                                      */
/*      Release the lock on the FME session.                            */
/************************************************************************/

void OGRFMEDataSource::ReleaseSession()

{
#ifdef DEBUG_MUTEX
    CPLDebug( kPROVIDERNAME, "%p:Release session mutex.", this );
#endif

    CPLReleaseMutex( hSessionMutex );
}

/************************************************************************/
/*                           SerializeToXML()                           */
/*                                                                      */
/*      Convert the information about this datasource, and it's         */
/*      layers into an XML format that can be stored in the             */
/*      persistent feature cache index.                                 */
/************************************************************************/

CPLXMLNode *OGRFMEDataSource::SerializeToXML()

{
    CPLXMLNode      *psDS;

    CPLAssert( bUseCaching );

/* -------------------------------------------------------------------- */
/*      Setup data source information.                                  */
/* -------------------------------------------------------------------- */
    psDS = CPLCreateXMLNode( NULL, CXT_Element, "DataSource" );

    CPLCreateXMLElementAndValue( psDS, "Driver", pszReaderName );
    CPLCreateXMLElementAndValue( psDS, "DSName", pszDataset );
    CPLCreateXMLElementAndValue( psDS, "RefCount", "0" );
    CPLCreateXMLElementAndValue( psDS, "CreationTime", "0" );
    CPLCreateXMLElementAndValue( psDS, "LastUseTime", "0" );

/* -------------------------------------------------------------------- */
/*      Append all the FME user directives in force.                    */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psUD;

    psUD = CPLCreateXMLNode( psDS, CXT_Element, "UserDirectives" );
    for( int i = 0; i < (int) poUserDirectives->entries(); i++ )
        CPLCreateXMLElementAndValue( psUD, "Directive", 
                                     (*poUserDirectives)(i) );

/* -------------------------------------------------------------------- */
/*      Now append all the layer information.                           */
/* -------------------------------------------------------------------- */
    for( int iLayer = 0; iLayer < nLayers; iLayer++ )
    {
        OGRFMELayerCached * poLayer = (OGRFMELayerCached *) papoLayers[iLayer];
        CPLXMLNode *psLayer;

        psLayer = poLayer->SerializeToXML();
        CPLAddXMLChild( psDS, psLayer );
    }

    return psDS;
}

/************************************************************************/
/*                         InitializeFromXML()                          */
/************************************************************************/

int OGRFMEDataSource::InitializeFromXML( CPLXMLNode *psDS )

{
    CPLAssert( bUseCaching );

/* -------------------------------------------------------------------- */
/*      Loop over layers, instantiating from the cached data.           */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psLayerN;

    for( psLayerN = psDS->psChild; psLayerN != NULL; 
         psLayerN = psLayerN->psNext )
    {
        OGRFMELayerCached *poNewLayer;

        if( !EQUAL(psLayerN->pszValue,"OGRLayer") )
            continue;

        poNewLayer = new OGRFMELayerCached( this );

/* -------------------------------------------------------------------- */
/*      Initialize the layer from the XML.                              */
/* -------------------------------------------------------------------- */
        if( !poNewLayer->InitializeFromXML( psLayerN ) )
        {
            // this is *not* proper cleanup
            CPLAssert( FALSE );
            nLayers = 0;

            return FALSE;
        }

/* -------------------------------------------------------------------- */
/*      Assign the spatial index.  We should really change this to      */
/*      check if it succeeds!                                           */
/* -------------------------------------------------------------------- */
        poNewLayer->AssignIndex( 
            CPLGetXMLValue( psLayerN, "SpatialCacheName", 
                            "<missing cachename>" ),
            NULL, NULL );

/* -------------------------------------------------------------------- */
/*      Add the layer to the layer list.                                */
/* -------------------------------------------------------------------- */
        papoLayers = (OGRFMELayer **)
            CPLRealloc(papoLayers, sizeof(void*) * ++nLayers );
        papoLayers[nLayers-1] = poNewLayer;
    }

    return TRUE;
}

/************************************************************************/
/*                         FME2OGRSpatialRef()                          */
/*                                                                      */
/*      Translate an FME coordinate system into an                      */
/*      OGRSpatialReference using the coordinate system manager         */
/*      getCoordSysAsOGCDef() method.  We assume the session has        */
/*      already been acquired.                                          */
/************************************************************************/

OGRSpatialReference *
OGRFMEDataSource::FME2OGRSpatialRef( const char *pszCoordsys )

{
    IFMEString *poOGCDef;

    poOGCDef = poSession->createString();

    poSession->coordSysManager()->getCoordSysAsOGCDef( 
        pszCoordsys, *poOGCDef );

    char *pszWKT = (char *) poOGCDef->data();
    OGRSpatialReference oSRS;

    if( oSRS.importFromWkt( &pszWKT ) == OGRERR_NONE )
    {
        poSession->destroyString( poOGCDef );
        return oSRS.Clone();
    }
    else
    {
        poSession->destroyString( poOGCDef );
        return NULL;
    }
}





