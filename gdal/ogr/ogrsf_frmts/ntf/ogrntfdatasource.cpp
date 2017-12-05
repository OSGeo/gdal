/******************************************************************************
 *
 * Project:  UK NTF Reader
 * Purpose:  Implements OGRNTFDataSource class
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 1999, Frank Warmerdam
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

#include "ntf.h"
#include "cpl_conv.h"
#include "cpl_string.h"

CPL_CVSID("$Id$")

/************************************************************************/
/*                          OGRNTFDataSource()                          */
/************************************************************************/

OGRNTFDataSource::OGRNTFDataSource() :
    pszName(NULL),
    nLayers(0),
    papoLayers(NULL),
    poFCLayer(NULL),
    iCurrentFC(0),
    iCurrentReader(-1),
    nCurrentPos(0),
    nCurrentFID(0),
    nNTFFileCount(0),
    papoNTFFileReader(NULL),
    nFCCount(0),
    papszFCNum(NULL),
    papszFCName(NULL),
    poSpatialRef(new OGRSpatialReference(
        "PROJCS[\"OSGB 1936 / British National Grid\",GEOGCS[\"OSGB 1936\","
        "DATUM[\"OSGB_1936\",SPHEROID[\"Airy 1830\",6377563.396,299.3249646,"
        "AUTHORITY[\"EPSG\",\"7001\"]],AUTHORITY[\"EPSG\",\"6277\"]],"
        "PRIMEM[\"Greenwich\",0,AUTHORITY[\"EPSG\",\"8901\"]],"
        "UNIT[\"degree\",0.0174532925199433],AUTHORITY[\"EPSG\",\"4277\"]],"
        "PROJECTION[\"Transverse_Mercator\"],"
        "PARAMETER[\"latitude_of_origin\",49],"
        "PARAMETER[\"central_meridian\",-2],"
        "PARAMETER[\"scale_factor\",0.999601272],"
        "PARAMETER[\"false_easting\",400000],"
        "PARAMETER[\"false_northing\",-100000],"
        "UNIT[\"metre\",1,AUTHORITY[\"EPSG\",\"9001\"]],"
        "AUTHORITY[\"EPSG\",\"27700\"]]")),
    papszOptions(NULL)
{
/* -------------------------------------------------------------------- */
/*      Allow initialization of options from the environment.           */
/* -------------------------------------------------------------------- */
    if( getenv("OGR_NTF_OPTIONS") != NULL )
    {
        papszOptions =
            CSLTokenizeStringComplex( getenv("OGR_NTF_OPTIONS"), ",",
                                      FALSE, FALSE );
    }
}

/************************************************************************/
/*                         ~OGRNTFDataSource()                          */
/************************************************************************/

OGRNTFDataSource::~OGRNTFDataSource()

{
    for( int i = 0; i < nNTFFileCount; i++ )
        delete papoNTFFileReader[i];

    CPLFree( papoNTFFileReader );

    for( int i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    if( poFCLayer != NULL )
        delete poFCLayer;

    CPLFree( papoLayers );

    CPLFree( pszName );

    CSLDestroy( papszOptions );

    CSLDestroy( papszFCNum );
    CSLDestroy( papszFCName );

    if( poSpatialRef )
        poSpatialRef->Release();
}

/************************************************************************/
/*                           TestCapability()                           */
/************************************************************************/

int OGRNTFDataSource::TestCapability( const char * )

{
    return FALSE;
}

/************************************************************************/
/*                           GetNamedLayer()                            */
/************************************************************************/

OGRNTFLayer * OGRNTFDataSource::GetNamedLayer( const char * pszNameIn )

{
    for( int i = 0; i < nLayers; i++ )
    {
        if( EQUAL(papoLayers[i]->GetLayerDefn()->GetName(),pszNameIn) )
            return static_cast<OGRNTFLayer *>(papoLayers[i]);
    }

    return NULL;
}

/************************************************************************/
/*                              AddLayer()                              */
/************************************************************************/

void OGRNTFDataSource::AddLayer( OGRLayer * poNewLayer )

{
    papoLayers = static_cast<OGRLayer **>(
        CPLRealloc( papoLayers, sizeof(void*) * ++nLayers ) );

    papoLayers[nLayers-1] = poNewLayer;
}

/************************************************************************/
/*                              GetLayer()                              */
/************************************************************************/

OGRLayer *OGRNTFDataSource::GetLayer( int iLayer )

{
    if( iLayer < 0 || iLayer > nLayers )
        return NULL;
    else if( iLayer == nLayers )
        return poFCLayer;
    else
        return papoLayers[iLayer];
}

/************************************************************************/
/*                           GetLayerCount()                            */
/************************************************************************/

int OGRNTFDataSource::GetLayerCount()

{
    if( poFCLayer == NULL )
        return nLayers;
    else
        return nLayers + 1;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

int OGRNTFDataSource::Open( const char * pszFilename, int bTestOpen,
                            char ** papszLimitedFileList )

{
    VSIStatBufL      stat;
    char            **papszFileList = NULL;

    pszName = CPLStrdup( pszFilename );

/* -------------------------------------------------------------------- */
/*      Is the given path a directory or a regular file?                */
/* -------------------------------------------------------------------- */
    if( VSIStatL( pszFilename, &stat ) != 0
        || (!VSI_ISDIR(stat.st_mode) && !VSI_ISREG(stat.st_mode)) )
    {
        if( !bTestOpen )
            CPLError( CE_Failure, CPLE_AppDefined,
                   "%s is neither a file or directory, NTF access failed.\n",
                      pszFilename );

        return FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Build a list of filenames we figure are NTF files.              */
/* -------------------------------------------------------------------- */
    if( VSI_ISREG(stat.st_mode) )
    {
        papszFileList = CSLAddString( NULL, pszFilename );
    }
    else
    {
        char **candidateFileList = VSIReadDir( pszFilename );

        for( int i = 0;
             candidateFileList != NULL && candidateFileList[i] != NULL;
             i++ )
        {
            if( papszLimitedFileList != NULL
                && CSLFindString(papszLimitedFileList,
                                 candidateFileList[i]) == -1 )
            {
                continue;
            }

            if( strlen(candidateFileList[i]) > 4
              && STARTS_WITH_CI(candidateFileList[i] + strlen(candidateFileList[i])-4, ".ntf") )
            {
                char fullFilename[2048];

                snprintf( fullFilename, sizeof(fullFilename), "%s%c%s",
                         pszFilename,
#ifdef WIN32
                         '\\',
#else
                         '/',
#endif
                         candidateFileList[i] );

                papszFileList = CSLAddString( papszFileList, fullFilename );
            }
        }

        CSLDestroy( candidateFileList );

        if( CSLCount(papszFileList) == 0 )
        {
            if( !bTestOpen )
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "No candidate NTF files (.ntf) found in\n"
                          "directory: %s",
                          pszFilename );
            CSLDestroy(papszFileList);
            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Loop over all these files trying to open them.  In testopen     */
/*      mode we first read the first 80 characters, to verify that      */
/*      it looks like an NTF file.  Note that we don't keep the file    */
/*      open ... we don't want to occupy a lot of file handles when      */
/*      handling a whole directory.                                     */
/* -------------------------------------------------------------------- */
    papoNTFFileReader = static_cast<NTFFileReader **>(
        CPLCalloc(sizeof(void*), CSLCount(papszFileList)));

    for( int i = 0; papszFileList != NULL && papszFileList[i] != NULL; i++ )
    {
        if( bTestOpen )
        {
            VSILFILE *fp = VSIFOpenL( papszFileList[i], "rb" );
            if( fp == NULL )
                continue;

            char szHeader[80] = {};
            if( VSIFReadL( szHeader, 80, 1, fp ) < 1 )
            {
                VSIFCloseL( fp );
                continue;
            }

            VSIFCloseL( fp );

            if( !STARTS_WITH_CI(szHeader, "01") )
                continue;

            int j = 0;  // Used after for.
            for( ; j < 80; j++ )
            {
                if( szHeader[j] == 10 || szHeader[j] == 13 )
                    break;
            }

            if( j == 80 || (j > 0 && szHeader[j-1] != '%') )
                continue;
        }

        NTFFileReader *poFR = new NTFFileReader( this );

        if( !poFR->Open( papszFileList[i] ) )
        {
            delete poFR;
            CSLDestroy( papszFileList );

            return FALSE;
        }

        poFR->SetBaseFID( nNTFFileCount * 1000000 + 1 );
        poFR->Close();

        EnsureTileNameUnique( poFR );

        papoNTFFileReader[nNTFFileCount++] = poFR;
    }

    CSLDestroy( papszFileList );

    if( nNTFFileCount == 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Establish generic layers.                                       */
/* -------------------------------------------------------------------- */
    EstablishGenericLayers();

/* -------------------------------------------------------------------- */
/*      Loop over all the files, collecting a unique feature class      */
/*      listing.                                                        */
/* -------------------------------------------------------------------- */
    for( int iSrcFile = 0; iSrcFile < nNTFFileCount; iSrcFile++ )
    {
        NTFFileReader *poSrcReader = papoNTFFileReader[iSrcFile];

        for( int iSrcFC = 0; iSrcFC < poSrcReader->GetFCCount(); iSrcFC++ )
        {
            char *pszSrcFCName = NULL;
            char *pszSrcFCNum = NULL;

            poSrcReader->GetFeatureClass( iSrcFC, &pszSrcFCNum, &pszSrcFCName);

            int iDstFC = 0;
            for( ; iDstFC < nFCCount; iDstFC++ )
            {
                if( EQUAL(pszSrcFCNum,papszFCNum[iDstFC]) )
                    break;
            }

            if( iDstFC >= nFCCount )
            {
                nFCCount++;
                papszFCNum = CSLAddString(papszFCNum,pszSrcFCNum);
                papszFCName = CSLAddString(papszFCName,pszSrcFCName);
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create a new layer specifically for feature classes.            */
/* -------------------------------------------------------------------- */
    if( nFCCount > 0 )
        poFCLayer = new OGRNTFFeatureClassLayer( this );
    else
        poFCLayer = NULL;

    return TRUE;
}

/************************************************************************/
/*                            ResetReading()                            */
/*                                                                      */
/*      Cleanup, and start over.                                        */
/************************************************************************/

void OGRNTFDataSource::ResetReading()

{
    for( int i = 0; i < nNTFFileCount; i++ )
        papoNTFFileReader[i]->Close();

    iCurrentReader = -1;
    nCurrentPos = (vsi_l_offset)-1;
    nCurrentFID = 1;
    iCurrentFC = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRNTFDataSource::GetNextFeature( OGRLayer** ppoBelongingLayer,
                                              double* pdfProgressPct,
                                              GDALProgressFunc /* pfnProgress */,
                                              void* /* pProgressData */ )

{
    if( pdfProgressPct != NULL )
        *pdfProgressPct = 0.0;
    if( ppoBelongingLayer != NULL )
        *ppoBelongingLayer = NULL;

    OGRFeature  *poFeature = NULL;

/* -------------------------------------------------------------------- */
/*      If we have already read all the conventional features, we       */
/*      should try and return feature class features.                   */
/* -------------------------------------------------------------------- */
    if( iCurrentReader == nNTFFileCount )
    {
        if( iCurrentFC < nFCCount )
            return poFCLayer->GetFeature( iCurrentFC++ );
        else
            return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Do we need to open a file?                                      */
/* -------------------------------------------------------------------- */
    if( iCurrentReader == -1 )
    {
        iCurrentReader++;
        nCurrentPos = (vsi_l_offset)-1;
    }

    if( papoNTFFileReader[iCurrentReader]->GetFP() == NULL )
    {
        papoNTFFileReader[iCurrentReader]->Open();
    }

/* -------------------------------------------------------------------- */
/*      Ensure we are reading on from the same point we were reading    */
/*      from for the last feature, even if some other access            */
/*      mechanism has moved the file pointer.                           */
/* -------------------------------------------------------------------- */
    if( nCurrentPos != (vsi_l_offset)-1 )
        papoNTFFileReader[iCurrentReader]->SetFPPos( nCurrentPos,
                                                     nCurrentFID );

/* -------------------------------------------------------------------- */
/*      Read a feature.  If we get NULL the file must be all            */
/*      consumed, advance to the next file.                             */
/* -------------------------------------------------------------------- */
    poFeature = papoNTFFileReader[iCurrentReader]->ReadOGRFeature();
    if( poFeature == NULL )
    {
        papoNTFFileReader[iCurrentReader]->Close();
        if( GetOption("CACHING") != NULL
            && EQUAL(GetOption("CACHING"),"OFF") )
            papoNTFFileReader[iCurrentReader]->DestroyIndex();

        iCurrentReader++;
        nCurrentPos = (vsi_l_offset)-1;
        nCurrentFID = 1;

        poFeature = GetNextFeature(NULL, NULL, NULL, NULL);
    }
    else
    {
        papoNTFFileReader[iCurrentReader]->GetFPPos(&nCurrentPos,
                                                    &nCurrentFID);
    }

    return poFeature;
}

/************************************************************************/
/*                          GetFeatureClass()                           */
/************************************************************************/

int OGRNTFDataSource::GetFeatureClass( int iFCIndex,
                                       char ** ppszFCId,
                                       char ** ppszFCName )

{
    if( iFCIndex < 0 || iFCIndex >= nFCCount )
    {
        *ppszFCId = NULL;
        *ppszFCName = NULL;
        return FALSE;
    }
    else
    {
        *ppszFCId = papszFCNum[iFCIndex];
        *ppszFCName = papszFCName[iFCIndex];
        return TRUE;
    }
}

/************************************************************************/
/*                             SetOptions()                             */
/************************************************************************/

void OGRNTFDataSource::SetOptionList( char ** papszNewOptions )

{
    CSLDestroy( papszOptions );
    papszOptions = CSLDuplicate( papszNewOptions );
}

/************************************************************************/
/*                             GetOption()                              */
/************************************************************************/

const char *OGRNTFDataSource::GetOption( const char * pszOption )

{
    return CSLFetchNameValue( papszOptions, pszOption );
}

/************************************************************************/
/*                        EnsureTileNameUnique()                        */
/*                                                                      */
/*      This method is called with an NTFFileReader to ensure that      */
/*      it's tilename is unique relative to all the readers already     */
/*      assigned to this data source.  If not, a unique name is         */
/*      selected for it and assigned.  This method should not be        */
/*      called with readers that are already attached to the data      */
/*      source.                                                         */
/************************************************************************/

void OGRNTFDataSource::EnsureTileNameUnique( NTFFileReader *poNewReader )

{
    int iSequenceNumber = -1;
    bool bIsUnique = false;
    char szCandidateName[11] = {};

    do
    {
        bIsUnique = TRUE;
        if( iSequenceNumber++ == -1 )
            strncpy( szCandidateName, poNewReader->GetTileName(),
                     sizeof(szCandidateName) - 1 );
        else
            snprintf( szCandidateName, sizeof(szCandidateName), "%010d", iSequenceNumber );

        for( int iReader = 0; iReader < nNTFFileCount && bIsUnique; iReader++ )
        {
            const char* pszTileName = GetFileReader( iReader )->GetTileName();
            if( pszTileName != NULL &&
                strcmp( szCandidateName, pszTileName ) == 0 )
            {
                bIsUnique = FALSE;
            }
        }
    } while( !bIsUnique );

    if( iSequenceNumber > 0 )
    {
        poNewReader->OverrideTileName( szCandidateName );
        CPLError( CE_Warning, CPLE_AppDefined,
                  "Forcing TILE_REF to `%s' on file %s\n"
                  "to avoid conflict with other tiles in this data source.",
                  szCandidateName, poNewReader->GetFilename() );
    }
}
