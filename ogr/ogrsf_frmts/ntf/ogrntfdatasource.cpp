/******************************************************************************
 * $Id$
 *
 * Project:  UK NTF Reader
 * Purpose:  Implements OGRNTFDataSource class
 * Author:   Frank Warmerdam, warmerda@home.com
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
 ******************************************************************************
 *
 * $Log$
 * Revision 1.7  1999/11/04 21:11:37  warmerda
 * Added TestCapability().
 *
 * Revision 1.6  1999/10/04 03:08:52  warmerda
 * added raster support
 *
 * Revision 1.5  1999/10/01 14:47:51  warmerda
 * major upgrade: generic, string feature codes, etc
 *
 * Revision 1.4  1999/09/29 16:43:43  warmerda
 * added spatial ref, improved test open for non-os files
 *
 * Revision 1.3  1999/09/08 00:58:40  warmerda
 * Added limiting list of files for FME.
 *
 * Revision 1.2  1999/08/30 16:49:26  warmerda
 * added feature class layer support
 *
 * Revision 1.1  1999/08/28 03:13:35  warmerda
 * New
 */

#include "ntf.h"
#include "cpl_conv.h"
#include "cpl_string.h"

/************************************************************************/
/*                          OGRNTFDataSource()                          */
/************************************************************************/

OGRNTFDataSource::OGRNTFDataSource()

{
    nLayers = 0;
    papoLayers = NULL;

    nNTFFileCount = 0;
    papoNTFFileReader = NULL;

    pszName = NULL;

    iCurrentReader = -1;
    iCurrentFC = 0;

    nFCCount = 0;
    papszFCNum = NULL;
    papszFCName = NULL;

    poFCLayer = NULL;

    papszOptions = NULL;

    poSpatialRef = new OGRSpatialReference( "PROJCS[\"OSGB 1936 / British National Grid\",GEOGCS[\"OSGB 1936\",DATUM[\"OSGB_1936\",SPHEROID[\"Airy 1830\",6377563.396,299.3249646]],PRIMEM[\"Greenwich\",0],UNIT[\"degree\",0.0174532925199433]],PROJECTION[\"Transverse_Mercator\"],PARAMETER[\"latitude_of_origin\",49],PARAMETER[\"central_meridian\",-2],PARAMETER[\"scale_factor\",0.999601272],PARAMETER[\"false_easting\",400000],PARAMETER[\"false_northing\",-100000],UNIT[\"metre\",1]]" );
}

/************************************************************************/
/*                         ~OGRNTFDataSource()                          */
/************************************************************************/

OGRNTFDataSource::~OGRNTFDataSource()

{
    int		i;

    for( i = 0; i < nNTFFileCount; i++ )
        delete papoNTFFileReader[i];

    CPLFree( papoNTFFileReader );

    for( i = 0; i < nLayers; i++ )
        delete papoLayers[i];

    if( poFCLayer != NULL )
        delete poFCLayer;
    
    CPLFree( papoLayers );

    CPLFree( pszName );

    CSLDestroy( papszOptions );

    delete poSpatialRef;
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

OGRNTFLayer * OGRNTFDataSource::GetNamedLayer( const char * pszName )

{
    for( int i = 0; i < nLayers; i++ )
    {
        if( EQUAL(papoLayers[i]->GetLayerDefn()->GetName(),pszName) )
            return (OGRNTFLayer *) papoLayers[i];
    }

    return NULL;
}

/************************************************************************/
/*                              AddLayer()                              */
/************************************************************************/

void OGRNTFDataSource::AddLayer( OGRLayer * poNewLayer )

{
    papoLayers = (OGRLayer **)
        CPLRealloc( papoLayers, sizeof(void*) * ++nLayers );
    
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
    VSIStatBuf      stat;
    char	    **papszFileList = NULL;

    pszName = CPLStrdup( pszFilename );

/* -------------------------------------------------------------------- */
/*      Is the given path a directory or a regular file?                */
/* -------------------------------------------------------------------- */
    if( VSIStat( pszFilename, &stat ) != 0 
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
        char      **candidateFileList = CPLReadDir( pszFilename );
        int         i;

        for( i = 0; 
             candidateFileList != NULL && candidateFileList[i] != NULL; 
             i++ ) 
        {
            if( papszLimitedFileList != NULL
                && CSLFindString(papszLimitedFileList,
                                 candidateFileList[i]) == -1 )
            {
                continue;
            }
            
            if( EQUALN(candidateFileList[i] + strlen(candidateFileList[i])-4,
                       ".ntf",4) )
            {
                char       fullFilename[2048];

                sprintf( fullFilename, "%s%c%s", 
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

        if( CSLCount(papszFileList) == 0 )
        {
            if( !bTestOpen )
                CPLError( CE_Failure, CPLE_OpenFailed,
                          "No candidate NTF files (.ntf) found in\n"
                          "directory: %s",
                          pszFilename );

            return FALSE;
        }
    }

/* -------------------------------------------------------------------- */
/*      Loop over all these files trying to open them.  In testopen     */
/*      mode we first read the first 80 characters, to verify that      */
/*      it looks like an NTF file.  Note that we don't keep the file    */
/*      open ... we don't want to occupy alot of file handles when      */
/*      handling a whole directory.                                     */
/* -------------------------------------------------------------------- */
    int		i;

    papoNTFFileReader = (NTFFileReader **)
        CPLCalloc(sizeof(void*), CSLCount(papszFileList));
    
    for( i = 0; papszFileList[i] != NULL; i++ )
    {
        if( bTestOpen )
        {
            char	szHeader[80];
            FILE	*fp;
            int		j;

            fp = VSIFOpen( papszFileList[i], "rb" );
            if( fp == NULL )
                continue;
            
            if( VSIFRead( szHeader, 80, 1, fp ) < 1 )
            {
                VSIFClose( fp );
                continue;
            }

            VSIFClose( fp );
            
            if( !EQUALN(szHeader,"01",2) )
                continue;

            for( j = 0; j < 80; j++ )
            {
                if( szHeader[j] == 10 || szHeader[j] == 13 )
                    break;
            }

            if( j == 80 || szHeader[j-1] != '%' )
                continue;

        }

        NTFFileReader	*poFR;

        poFR = new NTFFileReader( this );

        if( !poFR->Open( papszFileList[i] ) )
        {
            delete poFR;
            CSLDestroy( papszFileList );
            
            return FALSE;
        }

        poFR->SetBaseFID( nNTFFileCount * 1000000 + 1 );
        poFR->Close();
        
        papoNTFFileReader[nNTFFileCount++] = poFR;
    }

    CSLDestroy( papszFileList );

    if( nNTFFileCount == 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Establish generic layers.					*/
/* -------------------------------------------------------------------- */
    EstablishGenericLayers();
    
/* -------------------------------------------------------------------- */
/*      Loop over all the files, collecting a unique feature class      */
/*      listing.                                                        */
/* -------------------------------------------------------------------- */
    for( int iSrcFile = 0; iSrcFile < nNTFFileCount; iSrcFile++ )
    {
        NTFFileReader	*poSrcReader = papoNTFFileReader[iSrcFile];
        
        for( int iSrcFC = 0; iSrcFC < poSrcReader->GetFCCount(); iSrcFC++ )
        {
            int	        iDstFC;
            char       *pszSrcFCName, *pszSrcFCNum;

            poSrcReader->GetFeatureClass( iSrcFC, &pszSrcFCNum, &pszSrcFCName);
            
            for( iDstFC = 0; iDstFC < nFCCount; iDstFC++ )
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
    nCurrentPos = -1;
    nCurrentFID = 1;
    iCurrentFC = 0;
}

/************************************************************************/
/*                           GetNextFeature()                           */
/************************************************************************/

OGRFeature *OGRNTFDataSource::GetNextFeature()

{
    OGRFeature	*poFeature = NULL;

/* -------------------------------------------------------------------- */
/*	If we have already read all the conventional features, we 	*/
/*	should try and return feature class features.			*/    
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
        nCurrentPos = -1;
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
    if( nCurrentPos != -1 )
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
        iCurrentReader++;
        nCurrentPos = -1;
        nCurrentFID = 1;

        poFeature = GetNextFeature();
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

