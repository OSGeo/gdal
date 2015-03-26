/******************************************************************************
 * $Id$
 *
 * Project:  GDAL Core
 * Purpose:  Implementation of GDALPamDataset, a dataset base class that
 *           knows how to persist auxiliary metadata into a support XML file.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "gdal_pam.h"
#include "cpl_string.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                           GDALPamDataset()                           */
/************************************************************************/

/**
 * \class GDALPamDataset "gdal_pam.h"
 * 
 * A subclass of GDALDataset which introduces the ability to save and
 * restore auxiliary information (coordinate system, gcps, metadata, 
 * etc) not supported by a file format via an "auxiliary metadata" file
 * with the .aux.xml extension.  
 * 
 * <h3>Enabling PAM</h3>
 * 
 * PAM support can be enabled (resp. disabled) in GDAL by setting the GDAL_PAM_ENABLED
 * configuration option (via CPLSetConfigOption(), or the environment) to 
 * the value of YES (resp. NO). Note: The default value is build dependant and defaults
 * to YES in Windows and Unix builds.
 *
 * <h3>PAM Proxy Files</h3>
 *
 * In order to be able to record auxiliary information about files on
 * read-only media such as CDROMs or in directories where the user does not
 * have write permissions, it is possible to enable the "PAM Proxy Database".
 * When enabled the .aux.xml files are kept in a different directory, writable
 * by the user. Overviews will also be stored in the PAM proxy directory.
 *
 * To enable this, set the GDAL_PAM_PROXY_DIR configuration option to be
 * the name of the directory where the proxies should be kept. The configuration
 * option must be set *before* the first access to PAM, because its value is cached
 * for later access.
 *
 * <h3>Adding PAM to Drivers</h3>
 *
 * Drivers for physical file formats that wish to support persistent auxiliary
 * metadata in addition to that for the format itself should derive their
 * dataset class from GDALPamDataset instead of directly from GDALDataset.
 * The raster band classes should also be derived from GDALPamRasterBand.
 *
 * They should also call something like this near the end of the Open() 
 * method:
 * 
 * \code
 *      poDS->SetDescription( poOpenInfo->pszFilename );
 *      poDS->TryLoadXML();
 * \endcode
 *
 * The SetDescription() is necessary so that the dataset will have a valid
 * filename set as the description before TryLoadXML() is called.  TryLoadXML()
 * will look for an .aux.xml file with the same basename as the dataset and
 * in the same directory.  If found the contents will be loaded and kept
 * track of in the GDALPamDataset and GDALPamRasterBand objects.  When a 
 * call like GetProjectionRef() is not implemented by the format specific
 * class, it will fall through to the PAM implementation which will return
 * information if it was in the .aux.xml file. 
 *
 * Drivers should also try to call the GDALPamDataset/GDALPamRasterBand
 * methods as a fallback if their implementation does not find information.
 * This allows using the .aux.xml for variations that can't be stored in
 * the format.  For instance, the GeoTIFF driver GetProjectionRef() looks
 * like this:
 *
 * \code
 *      if( EQUAL(pszProjection,"") )
 *          return GDALPamDataset::GetProjectionRef();
 *      else
 *          return( pszProjection );
 * \endcode
 *
 * So if the geotiff header is missing, the .aux.xml file will be 
 * consulted. 
 *
 * Similarly, if SetProjection() were called with a coordinate system
 * not supported by GeoTIFF, the SetProjection() method should pass it on
 * to the GDALPamDataset::SetProjection() method after issuing a warning
 * that the information can't be represented within the file itself. 
 * 
 * Drivers for subdataset based formats will also need to declare the
 * name of the physical file they are related to, and the name of their 
 * subdataset before calling TryLoadXML(). 
 *
 * \code
 *      poDS->SetDescription( poOpenInfo->pszFilename );
 *      poDS->SetPhysicalFilename( poDS->pszFilename );
 *      poDS->SetSubdatasetName( osSubdatasetName );
 * 
 *      poDS->TryLoadXML();
 * \endcode
 */

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

CPLXMLNode *GDALPamDataset::SerializeToXML( const char *pszUnused )

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
    if( psPam->bHasMetadata )
    {
        CPLXMLNode *psMD;

        psMD = oMDMD.Serialize();
        if( psMD != NULL )
        {
            if( psMD->psChild == NULL && psMD->psNext == NULL )
                CPLDestroyXMLNode( psMD );
            else
                CPLAddXMLChild( psDSTree, psMD );
        }
    }

/* -------------------------------------------------------------------- */
/*      GCPs                                                            */
/* -------------------------------------------------------------------- */
    if( psPam->nGCPCount > 0 )
    {
        GDALSerializeGCPListToXML( psDSTree,
                                   psPam->pasGCPList,
                                   psPam->nGCPCount,
                                   psPam->pszGCPProjection );
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

        if( poBand == NULL || !(poBand->GetMOFlags() & GMO_PAM_CLASS) )
            continue;

        psBandTree = poBand->SerializeToXML( pszUnused );

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
#ifdef PAM_ENABLED
    static const char *pszPamDefault = "YES";
#else
    static const char *pszPamDefault = "NO";
#endif
    
    if( psPam || (nPamFlags & GPF_DISABLED) )
        return;

    if( !CSLTestBoolean( CPLGetConfigOption( "GDAL_PAM_ENABLED", 
                                             pszPamDefault ) ) )
    {
        nPamFlags |= GPF_DISABLED;
        return;
    }

    /* ERO 2011/04/13 : GPF_AUXMODE seems to be unimplemented */
    if( EQUAL( CPLGetConfigOption( "GDAL_PAM_MODE", "PAM" ), "AUX") )
        nPamFlags |= GPF_AUXMODE;

    psPam = new GDALDatasetPamInfo;
    psPam->pszPamFilename = NULL;
    psPam->pszProjection = NULL;
    psPam->bHaveGeoTransform = FALSE;
    psPam->nGCPCount = 0;
    psPam->pasGCPList = NULL;
    psPam->pszGCPProjection = NULL;
    psPam->bHasMetadata = FALSE;

    int iBand;
    
    for( iBand = 0; iBand < GetRasterCount(); iBand++ )
    {
        GDALPamRasterBand *poBand = (GDALPamRasterBand *)
            GetRasterBand(iBand+1);
        
        if( poBand == NULL || !(poBand->GetMOFlags() & GMO_PAM_CLASS) )
            continue;

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

        delete psPam;
        psPam = NULL;
    }
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr GDALPamDataset::XMLInit( CPLXMLNode *psTree, const char *pszUnused )

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
                psPam->adfGeoTransform[iTA] = CPLAtof(papszTokens[iTA]);
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
        CPLFree( psPam->pszGCPProjection );
        psPam->pszGCPProjection = NULL;

        // Make sure any previous GCPs, perhaps from an .aux file, are cleared
        // if we have new ones.
        if( psPam->nGCPCount > 0 )
        {
            GDALDeinitGCPs( psPam->nGCPCount, psPam->pasGCPList );
            CPLFree( psPam->pasGCPList );
            psPam->nGCPCount = 0;
            psPam->pasGCPList = 0;
        }

        GDALDeserializeGCPListFromXML( psGCPList,
                                       &(psPam->pasGCPList),
                                       &(psPam->nGCPCount),
                                       &(psPam->pszGCPProjection) );
    }

/* -------------------------------------------------------------------- */
/*      Apply any dataset level metadata.                               */
/* -------------------------------------------------------------------- */
    oMDMD.XMLInit( psTree, TRUE );

/* -------------------------------------------------------------------- */
/*      Try loading ESRI xml encoded projection                         */
/* -------------------------------------------------------------------- */
    if (psPam->pszProjection == NULL)
    {
        char** papszXML = oMDMD.GetMetadata( "xml:ESRI" );
        if (CSLCount(papszXML) == 1)
        {
            CPLXMLNode *psValueAsXML = CPLParseXMLString( papszXML[0] );
            if (psValueAsXML)
            {
                const char* pszESRI_WKT = CPLGetXMLValue(psValueAsXML,
                                  "=GeodataXform.SpatialReference.WKT", NULL);
                if (pszESRI_WKT)
                {
                    OGRSpatialReference* poSRS = new OGRSpatialReference(NULL);
                    char* pszTmp = (char*)pszESRI_WKT;
                    if (poSRS->importFromWkt(&pszTmp) == OGRERR_NONE &&
                        poSRS->morphFromESRI() == OGRERR_NONE)
                    {
                        char* pszWKT = NULL;
                        if (poSRS->exportToWkt(&pszWKT) == OGRERR_NONE)
                        {
                            psPam->pszProjection = CPLStrdup(pszWKT);
                        }
                        CPLFree(pszWKT);
                    }
                    delete poSRS;
                }
                CPLDestroyXMLNode(psValueAsXML);
            }
        }
    }

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

        if( poBand == NULL || !(poBand->GetMOFlags() & GMO_PAM_CLASS) )
            continue;

        poBand->XMLInit( psBandTree, pszUnused );
    }

/* -------------------------------------------------------------------- */
/*      Clear dirty flag.                                               */
/* -------------------------------------------------------------------- */
    nPamFlags &= ~GPF_DIRTY;

    return CE_None;
}

/************************************************************************/
/*                        SetPhysicalFilename()                         */
/************************************************************************/

void GDALPamDataset::SetPhysicalFilename( const char *pszFilename )

{
    PamInitialize();

    if( psPam )
        psPam->osPhysicalFilename = pszFilename;
}

/************************************************************************/
/*                        GetPhysicalFilename()                         */
/************************************************************************/

const char *GDALPamDataset::GetPhysicalFilename()

{
    PamInitialize();

    if( psPam )
        return psPam->osPhysicalFilename;
    else
        return "";
}

/************************************************************************/
/*                         SetSubdatasetName()                          */
/************************************************************************/

void GDALPamDataset::SetSubdatasetName( const char *pszSubdataset )

{
    PamInitialize();

    if( psPam )
        psPam->osSubdatasetName = pszSubdataset;
}

/************************************************************************/
/*                         GetSubdatasetName()                          */
/************************************************************************/

const char *GDALPamDataset::GetSubdatasetName()

{
    PamInitialize();

    if( psPam )
        return psPam->osSubdatasetName;
    else
        return "";
}

/************************************************************************/
/*                          BuildPamFilename()                          */
/************************************************************************/

const char *GDALPamDataset::BuildPamFilename()

{
    if( psPam == NULL )
        return NULL;

/* -------------------------------------------------------------------- */
/*      What is the name of the physical file we are referencing?       */
/*      We allow an override via the psPam->pszPhysicalFile item.       */
/* -------------------------------------------------------------------- */
    if( psPam->pszPamFilename != NULL )
        return psPam->pszPamFilename;

    const char *pszPhysicalFile = psPam->osPhysicalFilename;

    if( strlen(pszPhysicalFile) == 0 && GetDescription() != NULL )
        pszPhysicalFile = GetDescription();

    if( strlen(pszPhysicalFile) == 0 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Try a proxy lookup, otherwise just add .aux.xml.                */
/* -------------------------------------------------------------------- */
    const char *pszProxyPam = PamGetProxy( pszPhysicalFile );
    if( pszProxyPam != NULL )
        psPam->pszPamFilename = CPLStrdup(pszProxyPam);
    else
    {
        psPam->pszPamFilename = (char*) CPLMalloc(strlen(pszPhysicalFile)+10);
        strcpy( psPam->pszPamFilename, pszPhysicalFile );
        strcat( psPam->pszPamFilename, ".aux.xml" );
    }

    return psPam->pszPamFilename;
}

/************************************************************************/
/*                   IsPamFilenameAPotentialSiblingFile()               */
/************************************************************************/

int GDALPamDataset::IsPamFilenameAPotentialSiblingFile()
{
    if (psPam == NULL)
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Determine if the PAM filename is a .aux.xml file next to the    */
/*      physical file, or if it comes from the ProxyDB                  */
/* -------------------------------------------------------------------- */
    const char *pszPhysicalFile = psPam->osPhysicalFilename;

    if( strlen(pszPhysicalFile) == 0 && GetDescription() != NULL )
        pszPhysicalFile = GetDescription();

    int nLenPhysicalFile = strlen(pszPhysicalFile);
    int bIsSiblingPamFile = strncmp(psPam->pszPamFilename, pszPhysicalFile,
                                    nLenPhysicalFile) == 0 &&
                            strcmp(psPam->pszPamFilename + nLenPhysicalFile,
                                   ".aux.xml") == 0;

    return bIsSiblingPamFile;
}

/************************************************************************/
/*                             TryLoadXML()                             */
/************************************************************************/

CPLErr GDALPamDataset::TryLoadXML(char **papszSiblingFiles)

{
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

/* -------------------------------------------------------------------- */
/*      In case the PAM filename is a .aux.xml file next to the         */
/*      physical file and we have a siblings list, then we can skip     */
/*      stat'ing the filesystem.                                        */
/* -------------------------------------------------------------------- */
    if (papszSiblingFiles != NULL && IsPamFilenameAPotentialSiblingFile())
    {
        int iSibling = CSLFindString( papszSiblingFiles,
                                      CPLGetFilename(psPam->pszPamFilename) );
        if( iSibling >= 0 )
        {
            CPLErrorReset();
            CPLPushErrorHandler( CPLQuietErrorHandler );
            psTree = CPLParseXMLFile( psPam->pszPamFilename );
            CPLPopErrorHandler();
        }
    }
    else
    if( VSIStatExL( psPam->pszPamFilename, &sStatBuf,
                    VSI_STAT_EXISTS_FLAG | VSI_STAT_NATURE_FLAG ) == 0
        && VSI_ISREG( sStatBuf.st_mode ) )
    {
        CPLErrorReset();
        CPLPushErrorHandler( CPLQuietErrorHandler );
        psTree = CPLParseXMLFile( psPam->pszPamFilename );
        CPLPopErrorHandler();
    }

/* -------------------------------------------------------------------- */
/*      If we are looking for a subdataset, search for it's subtree     */
/*      now.                                                            */
/* -------------------------------------------------------------------- */
    if( psTree && psPam->osSubdatasetName.size() )
    {
        CPLXMLNode *psSubTree;
        
        for( psSubTree = psTree->psChild; 
             psSubTree != NULL;
             psSubTree = psSubTree->psNext )
        {
            if( psSubTree->eType != CXT_Element
                || !EQUAL(psSubTree->pszValue,"Subdataset") )
                continue;

            if( !EQUAL(CPLGetXMLValue( psSubTree, "name", "" ),
                       psPam->osSubdatasetName) )
                continue;

            psSubTree = CPLGetXMLNode( psSubTree, "PAMDataset" );
            break;
        }
        
        if( psSubTree != NULL )
            psSubTree = CPLCloneXMLTree( psSubTree );

        CPLDestroyXMLNode( psTree );
        psTree = psSubTree;
    }

/* -------------------------------------------------------------------- */
/*      If we fail, try .aux.                                           */
/* -------------------------------------------------------------------- */
    if( psTree == NULL )
        return TryLoadAux(papszSiblingFiles);

/* -------------------------------------------------------------------- */
/*      Initialize ourselves from this XML tree.                        */
/* -------------------------------------------------------------------- */
    CPLErr eErr;

    CPLString osVRTPath(CPLGetPath(psPam->pszPamFilename));
    eErr = XMLInit( psTree, osVRTPath );

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
    CPLErr eErr = CE_None;

    nPamFlags &= ~GPF_DIRTY;

    if( psPam == NULL || (nPamFlags & GPF_NOSAVE) )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Make sure we know the filename we want to store in.             */
/* -------------------------------------------------------------------- */
    if( !BuildPamFilename() )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Build the XML representation of the auxiliary metadata.          */
/* -------------------------------------------------------------------- */
    psTree = SerializeToXML( NULL );

    if( psTree == NULL )
    {
        /* If we have unset all metadata, we have to delete the PAM file */
        CPLPushErrorHandler( CPLQuietErrorHandler );
        VSIUnlink(psPam->pszPamFilename);
        CPLPopErrorHandler();
        return CE_None;
    }

/* -------------------------------------------------------------------- */
/*      If we are working with a subdataset, we need to integrate       */
/*      the subdataset tree within the whole existing pam tree,         */
/*      after removing any old version of the same subdataset.          */
/* -------------------------------------------------------------------- */
    if( psPam->osSubdatasetName.size() != 0 )
    {
        CPLXMLNode *psOldTree, *psSubTree;

        CPLErrorReset();
        CPLPushErrorHandler( CPLQuietErrorHandler );
        psOldTree = CPLParseXMLFile( psPam->pszPamFilename );
        CPLPopErrorHandler();

        if( psOldTree == NULL )
            psOldTree = CPLCreateXMLNode( NULL, CXT_Element, "PAMDataset" );

        for( psSubTree = psOldTree->psChild; 
             psSubTree != NULL;
             psSubTree = psSubTree->psNext )
        {
            if( psSubTree->eType != CXT_Element
                || !EQUAL(psSubTree->pszValue,"Subdataset") )
                continue;

            if( !EQUAL(CPLGetXMLValue( psSubTree, "name", "" ),
                       psPam->osSubdatasetName) )
                continue;

            break;
        }

        if( psSubTree == NULL )
        {
            psSubTree = CPLCreateXMLNode( psOldTree, CXT_Element, 
                                          "Subdataset" );
            CPLCreateXMLNode( 
                CPLCreateXMLNode( psSubTree, CXT_Attribute, "name" ),
                CXT_Text, psPam->osSubdatasetName );
        }
        
        CPLXMLNode *psOldPamDataset = CPLGetXMLNode( psSubTree, "PAMDataset");
        if( psOldPamDataset != NULL )
        {
            CPLRemoveXMLChild( psSubTree, psOldPamDataset );
            CPLDestroyXMLNode( psOldPamDataset );
        }

        CPLAddXMLChild( psSubTree, psTree );
        psTree = psOldTree;
    }

/* -------------------------------------------------------------------- */
/*      Try saving the auxiliary metadata.                               */
/* -------------------------------------------------------------------- */
    int bSaved;
    
    CPLPushErrorHandler( CPLQuietErrorHandler );
    bSaved = CPLSerializeXMLTreeToFile( psTree, psPam->pszPamFilename );
    CPLPopErrorHandler();

/* -------------------------------------------------------------------- */
/*      If it fails, check if we have a proxy directory for auxiliary    */
/*      metadata to be stored in, and try to save there.                */
/* -------------------------------------------------------------------- */
    if( bSaved )
        eErr = CE_None;
    else
    {
        const char *pszNewPam;
        const char *pszBasename = GetDescription();

        if( psPam && psPam->osPhysicalFilename.length() > 0 )
            pszBasename = psPam->osPhysicalFilename;
            
        if( PamGetProxy(pszBasename) == NULL 
            && ((pszNewPam = PamAllocateProxy(pszBasename)) != NULL))
        {
            CPLErrorReset();
            CPLFree( psPam->pszPamFilename );
            psPam->pszPamFilename = CPLStrdup(pszNewPam);
            eErr = TrySaveXML();
        }
        else
        {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Unable to save auxiliary information in %s.",
                      psPam->pszPamFilename );
            eErr = CE_Warning;
        }
    }
    
/* -------------------------------------------------------------------- */
/*      Cleanup                                                         */
/* -------------------------------------------------------------------- */
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
/*      Suppress NotImplemented error messages - mainly needed if PAM   */
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
        if( poSrcDS->GetMetadata("RPC") != NULL )
        {
            if( !bOnlyIfMissing 
                || CSLCount(GetMetadata("RPC")) 
                   != CSLCount(poSrcDS->GetMetadata("RPC")) )
            {
                SetMetadata( poSrcDS->GetMetadata("RPC"), "RPC" );
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

            if( poBand == NULL || !(poBand->GetMOFlags() & GMO_PAM_CLASS) )
                continue;

            if( poSrcDS->GetRasterCount() >= iBand+1 )
                poBand->CloneInfo( poSrcDS->GetRasterBand(iBand+1), 
                                   nCloneFlags );
            else
                CPLDebug( "GDALPamDataset", "Skipping CloneInfo for band not in source, this is a bit unusual!" );
        }
    }

/* -------------------------------------------------------------------- */
/*      Copy masks.  These are really copied at a lower level using     */
/*      GDALDefaultOverviews, for formats with no native mask           */
/*      support but this is a convenient central point to put this      */
/*      for most drivers.                                               */
/* -------------------------------------------------------------------- */
    if( nCloneFlags & GCIF_MASK )
    {
        GDALDriver::DefaultCopyMasks( poSrcDS, this, FALSE );
    }

/* -------------------------------------------------------------------- */
/*      Restore MO flags.                                               */
/* -------------------------------------------------------------------- */
    SetMOFlags( nSavedMOFlags );

    return CE_None;
}

/************************************************************************/
/*                            GetFileList()                             */
/*                                                                      */
/*      Add .aux.xml or .aux file into file list as appropriate.        */
/************************************************************************/

char **GDALPamDataset::GetFileList()

{
    VSIStatBufL sStatBuf;
    char **papszFileList = GDALDataset::GetFileList();

    if( psPam && psPam->osPhysicalFilename.size() > 0
        && CSLFindString( papszFileList, psPam->osPhysicalFilename ) == -1 )
    {
        papszFileList = CSLInsertString( papszFileList, 0, 
                                         psPam->osPhysicalFilename );
    }

    if( psPam && psPam->pszPamFilename )
    {
        int bAddPamFile = (nPamFlags & GPF_DIRTY);
        if (!bAddPamFile)
        {
            if (oOvManager.GetSiblingFiles() != NULL && IsPamFilenameAPotentialSiblingFile())
                bAddPamFile = CSLFindString(oOvManager.GetSiblingFiles(),
                                  CPLGetFilename(psPam->pszPamFilename)) >= 0;
            else
                bAddPamFile = VSIStatExL( psPam->pszPamFilename, &sStatBuf,
                                          VSI_STAT_EXISTS_FLAG ) == 0;
        }
        if (bAddPamFile)
        {
            papszFileList = CSLAddString( papszFileList, psPam->pszPamFilename );
        }
    }

    if( psPam && psPam->osAuxFilename.size() > 0 &&
        CSLFindString( papszFileList, psPam->osAuxFilename ) == -1 )
    {
        papszFileList = CSLAddString( papszFileList, psPam->osAuxFilename );
    }
    return papszFileList;
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr GDALPamDataset::IBuildOverviews( const char *pszResampling, 
                                        int nOverviews, int *panOverviewList, 
                                        int nListBands, int *panBandList,
                                        GDALProgressFunc pfnProgress, 
                                        void * pProgressData )
    
{
/* -------------------------------------------------------------------- */
/*      Initialize PAM.                                                 */
/* -------------------------------------------------------------------- */
    PamInitialize();
    if( psPam == NULL )
        return GDALDataset::IBuildOverviews( pszResampling,
                                             nOverviews, panOverviewList,
                                             nListBands, panBandList,
                                             pfnProgress, pProgressData );

/* -------------------------------------------------------------------- */
/*      If we appear to have subdatasets and to have a physical         */
/*      filename, use that physical filename to derive a name for a     */
/*      new overview file.                                              */
/* -------------------------------------------------------------------- */
    if( oOvManager.IsInitialized() && psPam->osPhysicalFilename.length() != 0 )
        return oOvManager.BuildOverviewsSubDataset( 
            psPam->osPhysicalFilename, pszResampling, 
            nOverviews, panOverviewList,
            nListBands, panBandList,
            pfnProgress, pProgressData );
    else 
        return GDALDataset::IBuildOverviews( pszResampling, 
                                             nOverviews, panOverviewList, 
                                             nListBands, panBandList, 
                                             pfnProgress, pProgressData );
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
    {
        psPam->bHasMetadata = TRUE;
        MarkPamDirty();
    }

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
    {
        psPam->bHasMetadata = TRUE;
        MarkPamDirty();
    }

    return GDALDataset::SetMetadataItem( pszName, pszValue, pszDomain );
}

/************************************************************************/
/*                          GetMetadataItem()                           */
/************************************************************************/

const char *GDALPamDataset::GetMetadataItem( const char *pszName, 
                                             const char *pszDomain )

{
/* -------------------------------------------------------------------- */
/*      A request against the ProxyOverviewRequest is a special         */
/*      mechanism to request an overview filename be allocated in       */
/*      the proxy pool location.  The allocated name is saved as        */
/*      metadata as well as being returned.                             */
/* -------------------------------------------------------------------- */
    if( pszDomain != NULL && EQUAL(pszDomain,"ProxyOverviewRequest") )
    {
        CPLString osPrelimOvr = GetDescription();
        osPrelimOvr += ":::OVR";
        
        const char *pszProxyOvrFilename = PamAllocateProxy( osPrelimOvr );
        if( pszProxyOvrFilename == NULL )
            return NULL;
        
        SetMetadataItem( "OVERVIEW_FILE", pszProxyOvrFilename, "OVERVIEWS" );
        
        return pszProxyOvrFilename;
    }

/* -------------------------------------------------------------------- */
/*      If the OVERVIEW_FILE metadata is requested, we intercept the    */
/*      request in order to replace ":::BASE:::" with the path to       */
/*      the physical file - if available.  This is primarily for the    */
/*      purpose of managing subdataset overview filenames as being      */
/*      relative to the physical file the subdataset comes              */
/*      from. (#3287).                                                  */
/* -------------------------------------------------------------------- */
    else if( pszDomain != NULL 
             && EQUAL(pszDomain,"OVERVIEWS") 
             && EQUAL(pszName,"OVERVIEW_FILE") )
    {
        const char *pszOverviewFile = 
            GDALDataset::GetMetadataItem( pszName, pszDomain );

        if( pszOverviewFile == NULL 
            || !EQUALN(pszOverviewFile,":::BASE:::",10) )
            return pszOverviewFile;
        
        CPLString osPath;

        if( strlen(GetPhysicalFilename()) > 0 )
            osPath = CPLGetPath(GetPhysicalFilename());
        else
            osPath = CPLGetPath(GetDescription());

        return CPLFormFilename( osPath, pszOverviewFile + 10, NULL );
    }

/* -------------------------------------------------------------------- */
/*      Everything else is a pass through.                              */
/* -------------------------------------------------------------------- */
    else
        return GDALDataset::GetMetadataItem( pszName, pszDomain );

}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **GDALPamDataset::GetMetadata( const char *pszDomain )

{
//    if( pszDomain == NULL || !EQUAL(pszDomain,"ProxyOverviewRequest") )
        return GDALDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                             TryLoadAux()                             */
/************************************************************************/

CPLErr GDALPamDataset::TryLoadAux(char **papszSiblingFiles)

{
/* -------------------------------------------------------------------- */
/*      Initialize PAM.                                                 */
/* -------------------------------------------------------------------- */
    PamInitialize();
    if( psPam == NULL )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      What is the name of the physical file we are referencing?       */
/*      We allow an override via the psPam->pszPhysicalFile item.       */
/* -------------------------------------------------------------------- */
    const char *pszPhysicalFile = psPam->osPhysicalFilename;

    if( strlen(pszPhysicalFile) == 0 && GetDescription() != NULL )
        pszPhysicalFile = GetDescription();

    if( strlen(pszPhysicalFile) == 0 )
        return CE_None;

    if( papszSiblingFiles )
    {
        CPLString osAuxFilename = CPLResetExtension( pszPhysicalFile, "aux");
        int iSibling = CSLFindString( papszSiblingFiles,
                                      CPLGetFilename(osAuxFilename) );
        if( iSibling < 0 )
        {
            osAuxFilename = pszPhysicalFile;
            osAuxFilename += ".aux";
            iSibling = CSLFindString( papszSiblingFiles,
                                      CPLGetFilename(osAuxFilename) );
            if( iSibling < 0 )
                return CE_None;
        }
    }

/* -------------------------------------------------------------------- */
/*      Try to open .aux file.                                          */
/* -------------------------------------------------------------------- */
    GDALDataset *poAuxDS = GDALFindAssociatedAuxFile( pszPhysicalFile,
                                                      GA_ReadOnly, this );

    if( poAuxDS == NULL )
        return CE_None;

    psPam->osAuxFilename = poAuxDS->GetDescription();

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

    papszMD = poAuxDS->GetMetadata("XFORMS");
    if( CSLCount(papszMD) > 0 )
    {
        char **papszMerged = 
            CSLMerge( CSLDuplicate(GetMetadata("XFORMS")), papszMD );
        GDALPamDataset::SetMetadata( papszMerged, "XFORMS" );
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

        if( strlen(poAuxBand->GetDescription()) > 0 )
            poBand->SetDescription( poAuxBand->GetDescription() );

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

        // NoData
        int bSuccess = FALSE;
        double dfNoDataValue = poAuxBand->GetNoDataValue( &bSuccess );
        if( bSuccess )
            poBand->SetNoDataValue( dfNoDataValue );
    }

    GDALClose( poAuxDS );
    
/* -------------------------------------------------------------------- */
/*      Mark PAM info as clean.                                         */
/* -------------------------------------------------------------------- */
    nPamFlags &= ~GPF_DIRTY;

    return CE_Failure;
}
