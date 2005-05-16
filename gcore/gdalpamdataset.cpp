/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALPamDataset, a dataset base class that 
 *           knows how to persist auxilary metadata into a support XML file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.3  2005/05/16 21:36:23  fwarmerdam
 * prototype support for reading aux file
 *
 * Revision 1.2  2005/05/11 14:03:08  fwarmerdam
 * added option to disable pam, and quiet failed .pam opens
 *
 * Revision 1.1  2005/04/27 16:27:44  fwarmerdam
 * New
 *
 */

#include "gdal_pam.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           GDALPamDataset()                           */
/************************************************************************/

GDALPamDataset::GDALPamDataset()

{
    nPamFlags = 0;
    psPam = NULL;
}

/************************************************************************/
/*                          ~GDALPamDataset()                           */
/************************************************************************/

GDALPamDataset::~GDALPamDataset()

{
    if( nPamFlags & GPF_DIRTY )
    {
        CPLDebug( "GDALPamDataset", "In destructor with dirty metadata." );
        FlushCache();
    }

    PamClear();
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void GDALPamDataset::FlushCache()

{
    GDALDataset::FlushCache();
    if( nPamFlags & GPF_DIRTY )
        TrySaveXML();
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *GDALPamDataset::SerializeToXML( const char *pszVRTPath )

{
    if( psPam == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Setup root node and attributes.                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psDSTree;

    psDSTree = CPLCreateXMLNode( NULL, CXT_Element, "PAMDataset" );

/* -------------------------------------------------------------------- */
/*      SRS                                                             */
/* -------------------------------------------------------------------- */
    if( psPam->pszProjection != NULL && strlen(psPam->pszProjection) > 0 )
        CPLSetXMLValue( psDSTree, "SRS", psPam->pszProjection );

/* -------------------------------------------------------------------- */
/*      GeoTransform.                                                   */
/* -------------------------------------------------------------------- */
    if( psPam->bHaveGeoTransform )
    {
        CPLSetXMLValue( psDSTree, "GeoTransform", 
                        CPLSPrintf( "%24.16e,%24.16e,%24.16e,%24.16e,%24.16e,%24.16e",
                                    psPam->adfGeoTransform[0],
                                    psPam->adfGeoTransform[1],
                                    psPam->adfGeoTransform[2],
                                    psPam->adfGeoTransform[3],
                                    psPam->adfGeoTransform[4],
                                    psPam->adfGeoTransform[5] ) );
    }

/* -------------------------------------------------------------------- */
/*      Metadata.                                                       */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psMD;

    psMD = PamSerializeMetadata( this );
    if( psMD != NULL )
        CPLAddXMLChild( psDSTree, psMD );

/* -------------------------------------------------------------------- */
/*      GCPs                                                            */
/* -------------------------------------------------------------------- */
    if( psPam->nGCPCount > 0 )
    {
        CPLXMLNode *psPamGCPList = CPLCreateXMLNode( psDSTree, CXT_Element, 
                                                     "GCPList" );

        if( psPam->pszGCPProjection != NULL 
            && strlen(psPam->pszGCPProjection) > 0 )
            CPLSetXMLValue( psPamGCPList, "#Projection", 
                            psPam->pszGCPProjection );

        for( int iGCP = 0; iGCP < psPam->nGCPCount; iGCP++ )
        {
            CPLXMLNode *psXMLGCP;
            GDAL_GCP *psGCP = psPam->pasGCPList + iGCP;

            psXMLGCP = CPLCreateXMLNode( psPamGCPList, CXT_Element, "GCP" );

            CPLSetXMLValue( psXMLGCP, "#Id", psGCP->pszId );

            if( psGCP->pszInfo != NULL && strlen(psGCP->pszInfo) > 0 )
                CPLSetXMLValue( psXMLGCP, "Info", psGCP->pszInfo );

            CPLSetXMLValue( psXMLGCP, "#Pixel", 
                            CPLSPrintf( "%.4f", psGCP->dfGCPPixel ) );

            CPLSetXMLValue( psXMLGCP, "#Line", 
                            CPLSPrintf( "%.4f", psGCP->dfGCPLine ) );

            CPLSetXMLValue( psXMLGCP, "#X", 
                            CPLSPrintf( "%.12E", psGCP->dfGCPX ) );

            CPLSetXMLValue( psXMLGCP, "#Y", 
                            CPLSPrintf( "%.12E", psGCP->dfGCPY ) );

            if( psGCP->dfGCPZ != 0.0 )
                CPLSetXMLValue( psXMLGCP, "#GCPZ", 
                                CPLSPrintf( "%.12E", psGCP->dfGCPZ ) );
        }
    }

/* -------------------------------------------------------------------- */
/*      Process bands.                                                  */
/* -------------------------------------------------------------------- */
    int iBand;

    for( iBand = 0; iBand < GetRasterCount(); iBand++ )
    {
        CPLXMLNode *psBandTree;

        GDALPamRasterBand *poBand = (GDALPamRasterBand *)
            GetRasterBand(iBand+1);

        // we really should be testing if poBand is a really derived
        // from PamRasterBand or not. 

        psBandTree = poBand->SerializeToXML( pszVRTPath );

        if( psBandTree != NULL )
            CPLAddXMLChild( psDSTree, psBandTree );
    }

/* -------------------------------------------------------------------- */
/*      We don't want to return anything if we had no metadata to       */
/*      attach.                                                         */
/* -------------------------------------------------------------------- */
    if( psDSTree->psChild == NULL )
    {
        CPLDestroyXMLNode( psDSTree );
        psDSTree = NULL;
    }

    return psDSTree;
}

/************************************************************************/
/*                           PamInitialize()                            */
/************************************************************************/

void GDALPamDataset::PamInitialize()

{
    if( psPam || (nPamFlags & GPF_DISABLED) )
        return;

    if( !CSLTestBoolean( CPLGetConfigOption( "GDAL_PAM_ENABLED", "YES" ) ) )
    {
        nPamFlags |= GPF_DISABLED;
        return;
    }

    if( EQUAL( CPLGetConfigOption( "GDAL_PAM_MODE", "PAM" ), "AUX") )
        nPamFlags |= GPF_AUXMODE;

    psPam = (GDALDatasetPamInfo *) CPLCalloc(sizeof(GDALDatasetPamInfo),1);

    int iBand;
    
    for( iBand = 0; iBand < GetRasterCount(); iBand++ )
    {
        GDALPamRasterBand *poBand = (GDALPamRasterBand *)
            GetRasterBand(iBand+1);
        
        // we really should be testing if poBand is a really derived
        // from PamRasterBand or not. 
        
        poBand->PamInitialize();
    }
}

/************************************************************************/
/*                              PamClear()                              */
/************************************************************************/

void GDALPamDataset::PamClear()

{
    if( psPam )
    {
        CPLFree( psPam->pszPamFilename );

        CPLFree( psPam->pszProjection );

        CPLFree( psPam->pszGCPProjection );
        if( psPam->nGCPCount > 0 )
        {
            GDALDeinitGCPs( psPam->nGCPCount, psPam->pasGCPList );
            CPLFree( psPam->pasGCPList );
        }

        CPLFree( psPam );
        psPam = NULL;
    }
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr GDALPamDataset::XMLInit( CPLXMLNode *psTree, const char *pszVRTPath )

{
/* -------------------------------------------------------------------- */
/*      Check for an SRS node.                                          */
/* -------------------------------------------------------------------- */
    if( strlen(CPLGetXMLValue(psTree, "SRS", "")) > 0 )
    {
        OGRSpatialReference oSRS;

        CPLFree( psPam->pszProjection );
        psPam->pszProjection = NULL;

        if( oSRS.SetFromUserInput( CPLGetXMLValue(psTree, "SRS", "") )
            == OGRERR_NONE )
            oSRS.exportToWkt( &(psPam->pszProjection) );
    }

/* -------------------------------------------------------------------- */
/*      Check for a GeoTransform node.                                  */
/* -------------------------------------------------------------------- */
    if( strlen(CPLGetXMLValue(psTree, "GeoTransform", "")) > 0 )
    {
        const char *pszGT = CPLGetXMLValue(psTree, "GeoTransform", "");
        char	**papszTokens;

        papszTokens = CSLTokenizeStringComplex( pszGT, ",", FALSE, FALSE );
        if( CSLCount(papszTokens) != 6 )
        {
            CPLError( CE_Warning, CPLE_AppDefined,
                      "GeoTransform node does not have expected six values.");
        }
        else
        {
            for( int iTA = 0; iTA < 6; iTA++ )
                psPam->adfGeoTransform[iTA] = atof(papszTokens[iTA]);
            psPam->bHaveGeoTransform = TRUE;
        }

        CSLDestroy( papszTokens );
    }

/* -------------------------------------------------------------------- */
/*      Check for GCPs.                                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psGCPList = CPLGetXMLNode( psTree, "GCPList" );

    if( psGCPList != NULL )
    {
        CPLXMLNode *psXMLGCP;
        OGRSpatialReference oSRS;
        const char *pszRawProj = CPLGetXMLValue(psGCPList, "Projection", "");

        CPLFree( psPam->pszGCPProjection );

        if( strlen(pszRawProj) > 0 
            && oSRS.SetFromUserInput( pszRawProj ) == OGRERR_NONE )
            oSRS.exportToWkt( &(psPam->pszGCPProjection) );
        else
            psPam->pszGCPProjection = CPLStrdup("");

        // Count GCPs.
        int  nGCPMax = 0;
         
        for( psXMLGCP = psGCPList->psChild; psXMLGCP != NULL; 
             psXMLGCP = psXMLGCP->psNext )
            nGCPMax++;
         
        psPam->pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),nGCPMax);
         
        for( psXMLGCP = psGCPList->psChild; psXMLGCP != NULL; 
             psXMLGCP = psXMLGCP->psNext )
        {
            GDAL_GCP *psGCP = psPam->pasGCPList + psPam->nGCPCount;

            if( !EQUAL(psXMLGCP->pszValue,"GCP") || 
                psXMLGCP->eType != CXT_Element )
                continue;
             
            GDALInitGCPs( 1, psGCP );
             
            CPLFree( psGCP->pszId );
            psGCP->pszId = CPLStrdup(CPLGetXMLValue(psXMLGCP,"Id",""));
             
            CPLFree( psGCP->pszInfo );
            psGCP->pszInfo = CPLStrdup(CPLGetXMLValue(psXMLGCP,"Info",""));
             
            psGCP->dfGCPPixel = atof(CPLGetXMLValue(psXMLGCP,"Pixel","0.0"));
            psGCP->dfGCPLine = atof(CPLGetXMLValue(psXMLGCP,"Line","0.0"));
             
            psGCP->dfGCPX = atof(CPLGetXMLValue(psXMLGCP,"X","0.0"));
            psGCP->dfGCPY = atof(CPLGetXMLValue(psXMLGCP,"Y","0.0"));
            psGCP->dfGCPZ = atof(CPLGetXMLValue(psXMLGCP,"Z","0.0"));

            psPam->nGCPCount++;
        }
    }

/* -------------------------------------------------------------------- */
/*      Apply any dataset level metadata.                               */
/* -------------------------------------------------------------------- */
    PamApplyMetadata( psTree, this );

/* -------------------------------------------------------------------- */
/*      Process bands.                                                  */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psBandTree;

    for( psBandTree = psTree->psChild; 
         psBandTree != NULL; psBandTree = psBandTree->psNext )
    {
        if( psBandTree->eType != CXT_Element
            || !EQUAL(psBandTree->pszValue,"PAMRasterBand") )
            continue;

        int nBand = atoi(CPLGetXMLValue( psBandTree, "band", "0"));

        if( nBand < 1 || nBand > GetRasterCount() )
            continue;

        GDALPamRasterBand *poBand = (GDALPamRasterBand *)
            GetRasterBand(nBand);

        if( poBand == NULL )
            continue;

        // we really should be testing if poBand is a really derived
        // from PamRasterBand or not. 

        poBand->XMLInit( psBandTree, pszVRTPath );
    }

/* -------------------------------------------------------------------- */
/*      Clear dirty flag.                                               */
/* -------------------------------------------------------------------- */
    nPamFlags &= ~GPF_DIRTY;

    return CE_None;
}

/************************************************************************/
/*                          BuildPamFilename()                          */
/************************************************************************/

const char *GDALPamDataset::BuildPamFilename()

{
    if( psPam == NULL )
        return NULL;

    if( psPam->pszPamFilename != NULL )
        return psPam->pszPamFilename;
    
    if( GetDescription() == NULL || strlen(GetDescription()) == 0 )
        return NULL;

    psPam->pszPamFilename = (char *) CPLMalloc(strlen(GetDescription()) + 5 );
    strcpy( psPam->pszPamFilename, GetDescription() );
    strcat( psPam->pszPamFilename, ".pam" );

    return psPam->pszPamFilename;
}

/************************************************************************/
/*                             TryLoadXML()                             */
/************************************************************************/

CPLErr GDALPamDataset::TryLoadXML()

{
    char *pszVRTPath = NULL;
    CPLXMLNode *psTree;

    PamInitialize();

/* -------------------------------------------------------------------- */
/*      Try reading the file.                                           */
/* -------------------------------------------------------------------- */
    if( !BuildPamFilename() )
        return CE_None;

    // we should really check for the files existance before trying
    // to open it. 
    CPLErrorReset();
    CPLPushErrorHandler( CPLQuietErrorHandler );
    psTree = CPLParseXMLFile( psPam->pszPamFilename );
    CPLPopErrorHandler();

/* -------------------------------------------------------------------- */
/*      If we fail, try .aux.                                           */
/* -------------------------------------------------------------------- */
    if( psTree == NULL )
        return TryLoadAux();

/* -------------------------------------------------------------------- */
/*      Initialize ourselves from this XML tree.                        */
/* -------------------------------------------------------------------- */
    CPLErr eErr;

    pszVRTPath = CPLStrdup(CPLGetPath(psPam->pszPamFilename));
    eErr = XMLInit( psTree, pszVRTPath );
    CPLFree( pszVRTPath );

    CPLDestroyXMLNode( psTree );

    if( eErr != CE_None )
        PamClear();

    return eErr;
}

/************************************************************************/
/*                             TrySaveXML()                             */
/************************************************************************/

CPLErr GDALPamDataset::TrySaveXML()

{
    CPLXMLNode *psTree;
    char *pszVRTPath;
    CPLErr eErr = CE_None;

    nPamFlags &= ~GPF_DIRTY;

    if( psPam == NULL )
        return CE_None;

    if( !BuildPamFilename() )
        return CE_None;

    pszVRTPath = CPLStrdup(CPLGetPath(psPam->pszPamFilename));
    psTree = SerializeToXML( pszVRTPath );
    CPLFree( pszVRTPath );

    if( psTree != NULL )
    {
        if( CPLSerializeXMLTreeToFile( psTree, psPam->pszPamFilename ) )
            eErr = CE_None;
        else
            eErr = CE_Failure;
    }
    
    CPLDestroyXMLNode( psTree );

    return eErr;
}

/************************************************************************/
/*                             CloneInfo()                              */
/************************************************************************/

CPLErr GDALPamDataset::CloneInfo( GDALDataset *poSrcDS, int nCloneFlags )

{
    int bOnlyIfMissing = nCloneFlags & GCIF_ONLY_IF_MISSING;

/* -------------------------------------------------------------------- */
/*      GeoTransform                                                    */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_GEOTRANSFORM )
    {
        double adfGeoTransform[6];

        if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None )
        {
            double adfOldGT[6];

            if( !bOnlyIfMissing || GetGeoTransform( adfOldGT ) != CE_None )
                SetGeoTransform( adfGeoTransform );
        }
    }

/* -------------------------------------------------------------------- */
/*      Projection                                                      */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_PROJECTION )
    {
        const char *pszWKT = poSrcDS->GetProjectionRef();

        if( pszWKT != NULL && strlen(pszWKT) > 0 )
        {
            if( !bOnlyIfMissing 
                || GetProjectionRef() == NULL
                || strlen(GetProjectionRef()) == 0 )
                SetProjection( pszWKT );
        }
    }

/* -------------------------------------------------------------------- */
/*      GCPs                                                            */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_GCPS )
    {
        if( poSrcDS->GetGCPCount() > 0 )
        {
            if( !bOnlyIfMissing || GetGCPCount() == 0 )
            {
                SetGCPs( poSrcDS->GetGCPCount(), 
                         poSrcDS->GetGCPs(), 
                         poSrcDS->GetGCPProjection() );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Metadata                                                        */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_METADATA )
    {
        if( poSrcDS->GetMetadata() != NULL )
        {
            if( !bOnlyIfMissing 
                || CSLCount(GetMetadata()) != CSLCount(poSrcDS->GetMetadata()) )
            {
                SetMetadata( poSrcDS->GetMetadata() );
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Process bands.                                                  */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_PROCESS_BANDS )
    {
        int iBand;

        for( iBand = 0; iBand < GetRasterCount(); iBand++ )
        {
            GDALPamRasterBand *poBand = (GDALPamRasterBand *)
                GetRasterBand(iBand+1);

            // we really should be testing if poBand is a really derived
            // from PamRasterBand or not. 

            poBand->CloneInfo( poSrcDS->GetRasterBand(iBand+1), nCloneFlags );
        }
    }

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *GDALPamDataset::GetProjectionRef()

{
    if( psPam && psPam->pszProjection )
        return psPam->pszProjection;
    else
        return GDALDataset::GetProjectionRef();
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr GDALPamDataset::SetProjection( const char *pszProjectionIn )

{
    if( psPam == NULL )
        return GDALDataset::SetProjection( pszProjectionIn );
    else
    {
        CPLFree( psPam->pszProjection );
        psPam->pszProjection = CPLStrdup( pszProjectionIn );
        MarkPamDirty();

        return CE_None;
    }
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr GDALPamDataset::GetGeoTransform( double * padfTransform )

{
    if( psPam && psPam->bHaveGeoTransform )
    {
        memcpy( padfTransform, psPam->adfGeoTransform, sizeof(double) * 6 );
        return CE_None;
    }
    else
        return GDALDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr GDALPamDataset::SetGeoTransform( double * padfTransform )

{
    if( psPam )
    {
        MarkPamDirty();
        psPam->bHaveGeoTransform = TRUE;
        memcpy( psPam->adfGeoTransform, padfTransform, sizeof(double) * 6 );
        return( CE_None );
    }
    else
    {
        return GDALDataset::SetGeoTransform( padfTransform );
    }
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int GDALPamDataset::GetGCPCount()

{
    if( psPam && psPam->nGCPCount > 0 )
        return psPam->nGCPCount;
    else
        return GDALDataset::GetGCPCount();
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *GDALPamDataset::GetGCPProjection()

{
    if( psPam && psPam->pszGCPProjection != NULL )
        return psPam->pszGCPProjection;
    else
        return GDALDataset::GetGCPProjection();
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *GDALPamDataset::GetGCPs()

{
    if( psPam && psPam->nGCPCount > 0 )
        return psPam->pasGCPList;
    else
        return GDALDataset::GetGCPs();
}

/************************************************************************/
/*                              SetGCPs()                               */
/************************************************************************/

CPLErr GDALPamDataset::SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                                const char *pszGCPProjection )

{
    if( psPam )
    {
        CPLFree( psPam->pszGCPProjection );
        if( psPam->nGCPCount > 0 )
        {
            GDALDeinitGCPs( psPam->nGCPCount, psPam->pasGCPList );
            CPLFree( psPam->pasGCPList );
        }

        psPam->pszGCPProjection = CPLStrdup(pszGCPProjection);
        psPam->nGCPCount = nGCPCount;
        psPam->pasGCPList = GDALDuplicateGCPs( nGCPCount, pasGCPList );

        MarkPamDirty();

        return CE_None;
    }
    else
    {
        return GDALDataset::SetGCPs( nGCPCount, pasGCPList, pszGCPProjection );
    }
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr GDALPamDataset::SetMetadata( char **papszMetadata, 
                                    const char *pszDomain )

{
    if( pszDomain == NULL || strlen(pszDomain) == 0 && psPam )
        MarkPamDirty();

    return GDALDataset::SetMetadata( papszMetadata, pszDomain );
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr GDALPamDataset::SetMetadataItem( const char *pszName, 
                                        const char *pszValue, 
                                        const char *pszDomain )

{
    if( pszDomain == NULL || strlen(pszDomain) == 0 && psPam )
        MarkPamDirty();

    return GDALDataset::SetMetadataItem( pszName, pszValue, pszDomain );
}

/************************************************************************/
/*                          PAMApplyMetadata()                          */
/************************************************************************/
int PamApplyMetadata( CPLXMLNode *psTree, GDALMajorObject *poMO )

{
    char **papszMD = NULL;
    CPLXMLNode *psMetadata = CPLGetXMLNode( psTree, "Metadata" );
    CPLXMLNode *psMDI;

    if( psMetadata == NULL )
        return FALSE;

    /* Format is <MDI key="...">value_Text</MDI> */

    for( psMDI = psMetadata->psChild; psMDI != NULL; psMDI = psMDI->psNext )
    {
        if( !EQUAL(psMDI->pszValue,"MDI") || psMDI->eType != CXT_Element 
            || psMDI->psChild == NULL || psMDI->psChild->psNext == NULL 
            || psMDI->psChild->eType != CXT_Attribute
            || psMDI->psChild->psChild == NULL )
            continue;

        papszMD = 
            CSLSetNameValue( papszMD, psMDI->psChild->psChild->pszValue, 
                             psMDI->psChild->psNext->pszValue );
    }

    poMO->SetMetadata( papszMD );
    CSLDestroy( papszMD );

    return papszMD != NULL;
}

/************************************************************************/
/*                        PAMSerializeMetadata()                        */
/************************************************************************/

CPLXMLNode *PamSerializeMetadata( GDALMajorObject *poMO )

{
    char **papszMD = poMO->GetMetadata();

    if( papszMD == NULL || CSLCount(papszMD) == 0 )
        return NULL;

    CPLXMLNode *psMD;

    psMD = CPLCreateXMLNode( NULL, CXT_Element, "Metadata" );

    for( int i = 0; papszMD[i] != NULL; i++ )
    {
        const char *pszRawValue;
        char *pszKey;
        CPLXMLNode *psMDI;

        pszRawValue = CPLParseNameValue( papszMD[i], &pszKey );

        psMDI = CPLCreateXMLNode( psMD, CXT_Element, "MDI" );
        CPLSetXMLValue( psMDI, "#key", pszKey );
        CPLCreateXMLNode( psMDI, CXT_Text, pszRawValue );

        CPLFree( pszKey );
    }

    return psMD;
}

/************************************************************************/
/*                             TryLoadAux()                             */
/************************************************************************/

CPLErr GDALPamDataset::TryLoadAux()

{
/* -------------------------------------------------------------------- */
/*      Initialize PAM.                                                 */
/* -------------------------------------------------------------------- */
    PamInitialize();
    if( psPam == NULL )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Try to build the .aux filename.                                 */
/* -------------------------------------------------------------------- */
    char *pszAuxFilename;

    if( GetDescription() == NULL || strlen(GetDescription()) == 0 )
        return CE_None;

    if( EQUAL(CPLGetExtension(GetDescription()),"aux") )
        return CE_None;

    pszAuxFilename =
        CPLStrdup( CPLResetExtension(GetDescription(),"aux") );

/* -------------------------------------------------------------------- */
/*      Does this file exist?  Does it have the right signature?        */
/* -------------------------------------------------------------------- */
    FILE *fpAux;
    char szSignature[16];

    memset( szSignature, 0, 16 );

    fpAux = VSIFOpenL( pszAuxFilename, "rb" );

    if( fpAux != NULL )
    {
        VSIFReadL( szSignature, 16, 1, fpAux );
        VSIFCloseL( fpAux );
    }

    if( !EQUALN(szSignature,"EHFA_HEADER_TAG",15) )
    {
        CPLFree( pszAuxFilename );
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      We have a HFA file.  Now try opening it via GDAL.               */
/* -------------------------------------------------------------------- */
    GDALDataset *poAuxDS = (GDALDataset *)
        GDALOpen( pszAuxFilename, GA_ReadOnly );

    CPLFree( pszAuxFilename );
    if( poAuxDS == NULL )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Check dependent file to ensure it matches our target file.      */
/* -------------------------------------------------------------------- */
    // TODO 

/* -------------------------------------------------------------------- */
/*      Do we have an SRS on the aux file?                              */
/* -------------------------------------------------------------------- */
    if( strlen(poAuxDS->GetProjectionRef()) > 0 )
        GDALPamDataset::SetProjection( poAuxDS->GetProjectionRef() );

/* -------------------------------------------------------------------- */
/*      Geotransform.                                                   */
/* -------------------------------------------------------------------- */
    if( poAuxDS->GetGeoTransform( psPam->adfGeoTransform ) == CE_None )
        psPam->bHaveGeoTransform = TRUE;

/* -------------------------------------------------------------------- */
/*      GCPs                                                            */
/* -------------------------------------------------------------------- */
    if( poAuxDS->GetGCPCount() > 0 )
    {
        psPam->nGCPCount = poAuxDS->GetGCPCount();
        psPam->pasGCPList = GDALDuplicateGCPs( psPam->nGCPCount, 
                                               poAuxDS->GetGCPs() );
    }

/* -------------------------------------------------------------------- */
/*      Apply metadata. We likely ought to be merging this in rather    */
/*      than overwriting everything that was there.                     */
/* -------------------------------------------------------------------- */
    char **papszMD = poAuxDS->GetMetadata();
    if( CSLCount(papszMD) > 0 )
        GDALPamDataset::SetMetadata( papszMD );

/* ==================================================================== */
/*      Process bands.                                                  */
/* ==================================================================== */
    int iBand;

    for( iBand = 0; iBand < poAuxDS->GetRasterCount(); iBand++ )
    {
        if( iBand >= GetRasterCount() )
            break;

        GDALRasterBand *poAuxBand = poAuxDS->GetRasterBand( iBand+1 );
        GDALRasterBand *poBand = GetRasterBand( iBand+1 );

        papszMD = poAuxBand->GetMetadata();
        if( CSLCount(papszMD) > 0 )
            GDALPamDataset::SetMetadata( papszMD );

        if( poAuxBand->GetCategoryNames() != NULL )
            poBand->SetCategoryNames( poAuxBand->GetCategoryNames() );

        if( poAuxBand->GetColorTable() != NULL 
            && poBand->GetColorTable() == NULL )
            poBand->SetColorTable( poAuxBand->GetColorTable() );

        // histograms?
        double dfMin, dfMax;
        int nBuckets, *panHistogram=NULL;

        if( poAuxBand->GetDefaultHistogram( &dfMin, &dfMax, 
                                            &nBuckets, &panHistogram,
                                            FALSE, NULL, NULL ) == CE_None )
        {
            poBand->SetDefaultHistogram( dfMin, dfMax, nBuckets, 
                                         panHistogram );
            CPLFree( panHistogram );
        }
    }

    GDALClose( poAuxDS );
    
    return CE_Failure;
}
