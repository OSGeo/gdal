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
 ****************************************************************************/

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
    SetMOFlags( GetMOFlags() | GMO_PAM_CLASS );
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
    CPLString oFmt;

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
                        oFmt.Printf( "%24.16e,%24.16e,%24.16e,%24.16e,%24.16e,%24.16e",
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

    psMD = oMDMD.Serialize();
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
                            oFmt.Printf( "%.4f", psGCP->dfGCPPixel ) );

            CPLSetXMLValue( psXMLGCP, "#Line", 
                            oFmt.Printf( "%.4f", psGCP->dfGCPLine ) );

            CPLSetXMLValue( psXMLGCP, "#X", 
                            oFmt.Printf( "%.12E", psGCP->dfGCPX ) );

            CPLSetXMLValue( psXMLGCP, "#Y", 
                            oFmt.Printf( "%.12E", psGCP->dfGCPY ) );

            if( psGCP->dfGCPZ != 0.0 )
                CPLSetXMLValue( psXMLGCP, "#GCPZ", 
                                oFmt.Printf( "%.12E", psGCP->dfGCPZ ) );
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

    if( !CSLTestBoolean( CPLGetConfigOption( "GDAL_PAM_ENABLED", "NO" ) ) )
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
    oMDMD.XMLInit( psTree, TRUE );

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

    psPam->pszPamFilename = (char *) CPLMalloc(strlen(GetDescription()) + 10 );
    strcpy( psPam->pszPamFilename, GetDescription() );
    strcat( psPam->pszPamFilename, ".aux.xml" );

    return psPam->pszPamFilename;
}

/************************************************************************/
/*                             TryLoadXML()                             */
/************************************************************************/

CPLErr GDALPamDataset::TryLoadXML()

{
    char *pszVRTPath = NULL;
    CPLXMLNode *psTree = NULL;

    PamInitialize();

/* -------------------------------------------------------------------- */
/*      Clear dirty flag.  Generally when we get to this point is       */
/*      from a call at the end of the Open() method, and some calls     */
/*      may have already marked the PAM info as dirty (for instance     */
/*      setting metadata), but really everything to this point is       */
/*      reproducable, and so the PAM info shouldn't really be           */
/*      thought of as dirty.                                            */
/* -------------------------------------------------------------------- */
    nPamFlags &= ~GPF_DIRTY;

/* -------------------------------------------------------------------- */
/*      Try reading the file.                                           */
/* -------------------------------------------------------------------- */
    if( !BuildPamFilename() )
        return CE_None;

    VSIStatBufL sStatBuf;

    if( VSIStatL( psPam->pszPamFilename, &sStatBuf ) == 0 
        && VSI_ISREG( sStatBuf.st_mode ) )
    {
        CPLErrorReset();
        CPLPushErrorHandler( CPLQuietErrorHandler );
        psTree = CPLParseXMLFile( psPam->pszPamFilename );
        CPLPopErrorHandler();
    }

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
    int nSavedMOFlags = GetMOFlags();

    PamInitialize();

/* -------------------------------------------------------------------- */
/*      Supress NotImplemented error messages - mainly needed if PAM    */
/*      disabled.                                                       */
/* -------------------------------------------------------------------- */
    SetMOFlags( nSavedMOFlags | GMO_IGNORE_UNIMPLEMENTED );

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

            if( !(poBand->GetMOFlags() | GMO_PAM_CLASS) )
                continue;

            if( poSrcDS->GetRasterCount() >= iBand+1 )
                poBand->CloneInfo( poSrcDS->GetRasterBand(iBand+1), 
                                   nCloneFlags );
            else
                CPLDebug( "GDALPamDataset", "Skipping CloneInfo for band not in source, this is a bit unusual!" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Restore MO flags.                                               */
/* -------------------------------------------------------------------- */
    SetMOFlags( nSavedMOFlags );

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
    PamInitialize();

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
    PamInitialize();

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
    PamInitialize();

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
    PamInitialize();

    if( psPam )
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
    PamInitialize();

    if( psPam )
        MarkPamDirty();

    return GDALDataset::SetMetadataItem( pszName, pszValue, pszDomain );
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
/*      Try to open .aux file.                                          */
/* -------------------------------------------------------------------- */
    if( GetDescription() == NULL || strlen(GetDescription()) == 0 )
        return CE_None;

    GDALDataset *poAuxDS = GDALFindAssociatedAuxFile( GetDescription(), 
                                                      GA_ReadOnly );

    if( poAuxDS == NULL )
        return CE_None;

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
    {
        char **papszMerged = 
            CSLMerge( CSLDuplicate(GetMetadata()), papszMD );
        GDALPamDataset::SetMetadata( papszMerged );
        CSLDestroy( papszMerged );
    }

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
        {
            char **papszMerged = 
                CSLMerge( CSLDuplicate(poBand->GetMetadata()), papszMD );
            poBand->SetMetadata( papszMerged );
            CSLDestroy( papszMerged );
        }

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

        // RAT 
        if( poAuxBand->GetDefaultRAT() != NULL )
            poBand->SetDefaultRAT( poAuxBand->GetDefaultRAT() );
    }

    GDALClose( poAuxDS );
    
/* -------------------------------------------------------------------- */
/*      Mark PAM info as clean.                                         */
/* -------------------------------------------------------------------- */
    nPamFlags &= ~GPF_DIRTY;

    return CE_Failure;
}
