/******************************************************************************
 * $Id$
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTDataset
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2007-2014, Even Rouault <even dot rouault at mines-paris dot org>
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

#include "vrtdataset.h"
#include "cpl_string.h"
#include "cpl_minixml.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                            VRTDataset()                             */
/************************************************************************/

VRTDataset::VRTDataset( int nXSize, int nYSize )

{
    nRasterXSize = nXSize;
    nRasterYSize = nYSize;
    pszProjection = NULL;

    bNeedsFlush = FALSE;
    bWritable = TRUE;

    bGeoTransformSet = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    nGCPCount = 0;
    pasGCPList = NULL;
    pszGCPProjection = NULL;

    pszVRTPath = NULL;

    poMaskBand = NULL;

    GDALRegister_VRT();
    poDriver = (GDALDriver *) GDALGetDriverByName( "VRT" );

    bCompatibleForDatasetIO = -1;
}

/************************************************************************/
/*                              VRTCreate()                             */
/************************************************************************/

/**
 * @see VRTDataset::VRTDataset()
 */

VRTDatasetH CPL_STDCALL VRTCreate(int nXSize, int nYSize)

{
    return ( new VRTDataset(nXSize, nYSize) );
}

/************************************************************************/
/*                            ~VRTDataset()                            */
/************************************************************************/

VRTDataset::~VRTDataset()

{
    FlushCache();
    CPLFree( pszProjection );

    CPLFree( pszGCPProjection );
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }
    CPLFree( pszVRTPath );

    delete poMaskBand;

    for(size_t i=0;i<apoOverviews.size();i++)
        delete apoOverviews[i];
    for(size_t i=0;i<apoOverviewsBak.size();i++)
        delete apoOverviewsBak[i];
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void VRTDataset::FlushCache()

{
    GDALDataset::FlushCache();

    if( !bNeedsFlush || bWritable == FALSE)
        return;

    bNeedsFlush = FALSE;

    // We don't write to disk if there is no filename.  This is a 
    // memory only dataset.
    if( strlen(GetDescription()) == 0 
        || EQUALN(GetDescription(),"<VRTDataset",11) )
        return;

    /* -------------------------------------------------------------------- */
    /*      Create the output file.                                         */
    /* -------------------------------------------------------------------- */
    VSILFILE *fpVRT;

    fpVRT = VSIFOpenL( GetDescription(), "w" );
    if( fpVRT == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Failed to write .vrt file in FlushCache()." );
        return;
    }

    /* -------------------------------------------------------------------- */
    /*      Convert tree to a single block of XML text.                     */
    /* -------------------------------------------------------------------- */
    char *pszVRTPath = CPLStrdup(CPLGetPath(GetDescription()));
    CPLXMLNode *psDSTree = SerializeToXML( pszVRTPath );
    char *pszXML;

    pszXML = CPLSerializeXMLTree( psDSTree );

    CPLDestroyXMLNode( psDSTree );

    CPLFree( pszVRTPath );

    /* -------------------------------------------------------------------- */
    /*      Write to disk.                                                  */
    /* -------------------------------------------------------------------- */
    VSIFWriteL( pszXML, 1, strlen(pszXML), fpVRT );
    VSIFCloseL( fpVRT );

    CPLFree( pszXML );
}

/************************************************************************/
/*                            VRTFlushCache()                           */
/************************************************************************/

/**
 * @see VRTDataset::FlushCache()
 */

void CPL_STDCALL VRTFlushCache( VRTDatasetH hDataset )
{
    VALIDATE_POINTER0( hDataset, "VRTFlushCache" );

    ((VRTDataset *)hDataset)->FlushCache();
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTDataset::SerializeToXML( const char *pszVRTPath )

{
    /* -------------------------------------------------------------------- */
    /*      Setup root node and attributes.                                 */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psDSTree = NULL;
    CPLXMLNode *psMD = NULL;
    char       szNumber[128];

    psDSTree = CPLCreateXMLNode( NULL, CXT_Element, "VRTDataset" );

    sprintf( szNumber, "%d", GetRasterXSize() );
    CPLSetXMLValue( psDSTree, "#rasterXSize", szNumber );

    sprintf( szNumber, "%d", GetRasterYSize() );
    CPLSetXMLValue( psDSTree, "#rasterYSize", szNumber );

 /* -------------------------------------------------------------------- */
 /*      SRS                                                             */
 /* -------------------------------------------------------------------- */
    if( pszProjection != NULL && strlen(pszProjection) > 0 )
        CPLSetXMLValue( psDSTree, "SRS", pszProjection );

 /* -------------------------------------------------------------------- */
 /*      Geotransform.                                                   */
 /* -------------------------------------------------------------------- */
    if( bGeoTransformSet )
    {
        CPLSetXMLValue( psDSTree, "GeoTransform", 
                        CPLSPrintf( "%24.16e,%24.16e,%24.16e,%24.16e,%24.16e,%24.16e",
                                    adfGeoTransform[0],
                                    adfGeoTransform[1],
                                    adfGeoTransform[2],
                                    adfGeoTransform[3],
                                    adfGeoTransform[4],
                                    adfGeoTransform[5] ) );
    }

/* -------------------------------------------------------------------- */
/*      Metadata                                                        */
/* -------------------------------------------------------------------- */
    psMD = oMDMD.Serialize();
    if( psMD != NULL )
    {
        CPLAddXMLChild( psDSTree, psMD );
    }

 /* -------------------------------------------------------------------- */
 /*      GCPs                                                            */
 /* -------------------------------------------------------------------- */
    if( nGCPCount > 0 )
    {
        GDALSerializeGCPListToXML( psDSTree,
                                   pasGCPList,
                                   nGCPCount,
                                   pszGCPProjection );
    }

    /* -------------------------------------------------------------------- */
    /*      Serialize bands.                                                */
    /* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        CPLXMLNode *psBandTree = 
            ((VRTRasterBand *) papoBands[iBand])->SerializeToXML(pszVRTPath);

        if( psBandTree != NULL )
            CPLAddXMLChild( psDSTree, psBandTree );
    }

    /* -------------------------------------------------------------------- */
    /*      Serialize dataset mask band.                                    */
    /* -------------------------------------------------------------------- */
    if (poMaskBand)
    {
        CPLXMLNode *psBandTree =
            poMaskBand->SerializeToXML(pszVRTPath);

        if( psBandTree != NULL )
        {
            CPLXMLNode *psMaskBandElement = CPLCreateXMLNode( psDSTree, CXT_Element, 
                                                              "MaskBand" );
            CPLAddXMLChild( psMaskBandElement, psBandTree );
        }
    }

    return psDSTree;
}

/************************************************************************/
/*                          VRTSerializeToXML()                         */
/************************************************************************/

/**
 * @see VRTDataset::SerializeToXML()
 */

CPLXMLNode * CPL_STDCALL VRTSerializeToXML( VRTDatasetH hDataset,
                                            const char *pszVRTPath )
{
    VALIDATE_POINTER1( hDataset, "VRTSerializeToXML", NULL );

    return ((VRTDataset *)hDataset)->SerializeToXML(pszVRTPath);
}

/************************************************************************/
/*                              XMLInit()                               */
/************************************************************************/

CPLErr VRTDataset::XMLInit( CPLXMLNode *psTree, const char *pszVRTPath )

{
    if( pszVRTPath != NULL )
        this->pszVRTPath = CPLStrdup(pszVRTPath);

/* -------------------------------------------------------------------- */
/*      Check for an SRS node.                                          */
/* -------------------------------------------------------------------- */
    if( strlen(CPLGetXMLValue(psTree, "SRS", "")) > 0 )
    {
        OGRSpatialReference oSRS;

        CPLFree( pszProjection );
        pszProjection = NULL;

        if( oSRS.SetFromUserInput( CPLGetXMLValue(psTree, "SRS", "") )
            == OGRERR_NONE )
            oSRS.exportToWkt( &pszProjection );
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
                adfGeoTransform[iTA] = CPLAtof(papszTokens[iTA]);
            bGeoTransformSet = TRUE;
        }

        CSLDestroy( papszTokens );
    }

/* -------------------------------------------------------------------- */
/*      Check for GCPs.                                                 */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psGCPList = CPLGetXMLNode( psTree, "GCPList" );

    if( psGCPList != NULL )
    {
        GDALDeserializeGCPListFromXML( psGCPList,
                                       &pasGCPList,
                                       &nGCPCount,
                                       &pszGCPProjection );
    }

/* -------------------------------------------------------------------- */
/*      Apply any dataset level metadata.                               */
/* -------------------------------------------------------------------- */
    oMDMD.XMLInit( psTree, TRUE );

/* -------------------------------------------------------------------- */
/*      Create dataset mask band.                                       */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psChild;

    /* Parse dataset mask band first */
    CPLXMLNode* psMaskBandNode = CPLGetXMLNode(psTree, "MaskBand");
    if (psMaskBandNode)
        psChild = psMaskBandNode->psChild;
    else
        psChild = NULL;
    for( ; psChild != NULL; psChild=psChild->psNext )
    {
        if( psChild->eType == CXT_Element
            && EQUAL(psChild->pszValue,"VRTRasterBand") )
        {
            VRTRasterBand  *poBand = NULL;
            const char *pszSubclass = CPLGetXMLValue( psChild, "subclass",
                                                      "VRTSourcedRasterBand" );

            if( EQUAL(pszSubclass,"VRTSourcedRasterBand") )
                poBand = new VRTSourcedRasterBand( this, 0 );
            else if( EQUAL(pszSubclass, "VRTDerivedRasterBand") )
                poBand = new VRTDerivedRasterBand( this, 0 );
            else if( EQUAL(pszSubclass, "VRTRawRasterBand") )
                poBand = new VRTRawRasterBand( this, 0 );
            else if( EQUAL(pszSubclass, "VRTWarpedRasterBand") )
                poBand = new VRTWarpedRasterBand( this, 0 );
            //else if( EQUAL(pszSubclass, "VRTPansharpenedRasterBand") )
            //    poBand = new VRTPansharpenedRasterBand( this, 0 );
            else
                CPLError( CE_Failure, CPLE_AppDefined,
                          "VRTRasterBand of unrecognised subclass '%s'.",
                          pszSubclass );

            if( poBand != NULL
                && poBand->XMLInit( psChild, pszVRTPath ) == CE_None )
            {
                SetMaskBand(poBand);
                break;
            }
            else
            {
                if( poBand )
                    delete poBand;
                return CE_Failure;
            }
        }
    }

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int		nBands = 0;
    for( psChild=psTree->psChild; psChild != NULL; psChild=psChild->psNext )
    {
        if( psChild->eType == CXT_Element
            && EQUAL(psChild->pszValue,"VRTRasterBand") )
        {
            VRTRasterBand  *poBand = NULL;
            const char *pszSubclass = CPLGetXMLValue( psChild, "subclass", 
                                                      "VRTSourcedRasterBand" );

            if( EQUAL(pszSubclass,"VRTSourcedRasterBand") )
                poBand = new VRTSourcedRasterBand( this, nBands+1 );
            else if( EQUAL(pszSubclass, "VRTDerivedRasterBand") )
                poBand = new VRTDerivedRasterBand( this, nBands+1 );
            else if( EQUAL(pszSubclass, "VRTRawRasterBand") )
                poBand = new VRTRawRasterBand( this, nBands+1 );
            else if( EQUAL(pszSubclass, "VRTWarpedRasterBand") )
                poBand = new VRTWarpedRasterBand( this, nBands+1 );
            else if( EQUAL(pszSubclass, "VRTPansharpenedRasterBand") )
                poBand = new VRTPansharpenedRasterBand( this, nBands+1 );
            else
                CPLError( CE_Failure, CPLE_AppDefined,
                          "VRTRasterBand of unrecognised subclass '%s'.",
                          pszSubclass );

            if( poBand != NULL 
                && poBand->XMLInit( psChild, pszVRTPath ) == CE_None )
            {
                SetBand( ++nBands, poBand );
            }
            else
            {
                if( poBand )
                    delete poBand; 
                return CE_Failure;
            }
        }
    }
    
    return CE_None;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int VRTDataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *VRTDataset::GetGCPProjection()

{
    if( pszGCPProjection == NULL )
        return "";
    else
        return pszGCPProjection;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *VRTDataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                              SetGCPs()                               */
/************************************************************************/

CPLErr VRTDataset::SetGCPs( int nGCPCount, const GDAL_GCP *pasGCPList,
                            const char *pszGCPProjection )

{
    CPLFree( this->pszGCPProjection );
    if( this->nGCPCount > 0 )
    {
        GDALDeinitGCPs( this->nGCPCount, this->pasGCPList );
        CPLFree( this->pasGCPList );
    }

    this->pszGCPProjection = CPLStrdup(pszGCPProjection);

    this->nGCPCount = nGCPCount;

    this->pasGCPList = GDALDuplicateGCPs( nGCPCount, pasGCPList );

    this->bNeedsFlush = TRUE;

    return CE_None;
}

/************************************************************************/
/*                           SetProjection()                            */
/************************************************************************/

CPLErr VRTDataset::SetProjection( const char *pszWKT )

{
    CPLFree( pszProjection );
    pszProjection = NULL;

    if( pszWKT != NULL )
        pszProjection = CPLStrdup(pszWKT);

    bNeedsFlush = TRUE;

    return CE_None;
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *VRTDataset::GetProjectionRef()

{
    if( pszProjection == NULL )
        return "";
    else
        return pszProjection;
}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr VRTDataset::SetGeoTransform( double *padfGeoTransformIn )

{
    memcpy( adfGeoTransform, padfGeoTransformIn, sizeof(double) * 6 );
    bGeoTransformSet = TRUE;

    bNeedsFlush = TRUE;

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr VRTDataset::GetGeoTransform( double * padfGeoTransform )

{
    memcpy( padfGeoTransform, adfGeoTransform, sizeof(double) * 6 );

    if( bGeoTransformSet )
        return CE_None;
    else
        return CE_Failure;
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr VRTDataset::SetMetadata( char **papszMetadata, 
                                   const char *pszDomain )

{
    SetNeedsFlush();

    return GDALDataset::SetMetadata( papszMetadata, pszDomain );
}

/************************************************************************/
/*                          SetMetadataItem()                           */
/************************************************************************/

CPLErr VRTDataset::SetMetadataItem( const char *pszName, 
                                    const char *pszValue, 
                                    const char *pszDomain )

{
    SetNeedsFlush();

    return GDALDataset::SetMetadataItem( pszName, pszValue, pszDomain );
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int VRTDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( poOpenInfo->nHeaderBytes > 20
         && strstr((const char *)poOpenInfo->pabyHeader,"<VRTDataset") != NULL )
        return TRUE;

    if( strstr(poOpenInfo->pszFilename,"<VRTDataset") != NULL )
        return TRUE;

    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *VRTDataset::Open( GDALOpenInfo * poOpenInfo )

{
    char *pszVRTPath = NULL;

/* -------------------------------------------------------------------- */
/*      Does this appear to be a virtual dataset definition XML         */
/*      file?                                                           */
/* -------------------------------------------------------------------- */
    if( !Identify( poOpenInfo ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*	Try to read the whole file into memory.				*/
/* -------------------------------------------------------------------- */
    char        *pszXML;

    VSILFILE        *fp = poOpenInfo->fpL;
    if( fp != NULL )
    {
        unsigned int nLength;

        poOpenInfo->fpL = NULL;
        
        if( strcmp(poOpenInfo->pszFilename, "/vsistdin/") == 0 )
        {
            nLength = 0;
            pszXML = (char *) VSIMalloc(1024+1);
            while( TRUE )
            {
                int nRead = (int) VSIFReadL( pszXML + nLength, 1, 1024, fp);
                nLength += nRead;
                if( nRead < 1024 )
                    break;
                char* pszXMLNew = (char*) VSIRealloc( pszXML, nLength + 1024 + 1);
                if( pszXMLNew == NULL )
                {
                    VSIFree(pszXML);
                    return NULL;
                }
                pszXML = pszXMLNew;
            }
        }
        else
        {
            VSIFSeekL( fp, 0, SEEK_END );
            nLength = (int) VSIFTellL( fp );
            VSIFSeekL( fp, 0, SEEK_SET );

            pszXML = (char *) VSIMalloc(nLength+1);

            if( pszXML == NULL )
            {
                VSIFCloseL(fp);
                CPLError( CE_Failure, CPLE_OutOfMemory,
                          "Failed to allocate %d byte buffer to hold VRT xml file.",
                          nLength );
                return NULL;
            }
            
            if( VSIFReadL( pszXML, 1, nLength, fp ) != nLength )
            {
                VSIFCloseL(fp);
                CPLFree( pszXML );
                CPLError( CE_Failure, CPLE_FileIO,
                          "Failed to read %d bytes from VRT xml file.",
                          nLength );
                return NULL;
            }
        }

        pszXML[nLength] = '\0';
        
        char* pszCurDir = CPLGetCurrentDir();
        const char *currentVrtFilename = CPLProjectRelativeFilename(pszCurDir, poOpenInfo->pszFilename);
        CPLFree(pszCurDir);
#if defined(HAVE_READLINK) && defined(HAVE_LSTAT)
        VSIStatBuf statBuffer;
        char filenameBuffer[2048];

        while( true ) {
            int lstatCode = lstat( currentVrtFilename, &statBuffer );
            if ( lstatCode == -1 ) {
                if (errno == ENOENT) {
                    // The file could be a virtual file, let later checks handle it.
                    break;
                } else {
                    VSIFCloseL(fp);
                    CPLFree( pszXML );
                    CPLError( CE_Failure, CPLE_FileIO,
                              "Failed to lstat %s: %s",
                              currentVrtFilename,
                              VSIStrerror(errno) );
                    return NULL;
                }
            }

            if ( !VSI_ISLNK(statBuffer.st_mode) ) {
                break;
            }

            int bufferSize = readlink(currentVrtFilename, filenameBuffer, sizeof(filenameBuffer));
            if (bufferSize != -1) {
                filenameBuffer[MIN(bufferSize, (int) sizeof(filenameBuffer) - 1)] = 0;
                // The filename in filenameBuffer might be a relative path from the linkfile resolve it before looping
                currentVrtFilename = CPLProjectRelativeFilename(CPLGetDirname(currentVrtFilename), filenameBuffer);
            } else {
                VSIFCloseL(fp);
                CPLFree( pszXML );
                CPLError( CE_Failure, CPLE_FileIO,
                          "Failed to read filename from symlink %s: %s",
                          currentVrtFilename,
                          VSIStrerror(errno) );
                return NULL;
            }
        }
#endif

        pszVRTPath = CPLStrdup(CPLGetPath(currentVrtFilename));

        VSIFCloseL(fp);
    }
/* -------------------------------------------------------------------- */
/*      Or use the filename as the XML input.                           */
/* -------------------------------------------------------------------- */
    else
    {
        pszXML = CPLStrdup( poOpenInfo->pszFilename );
    }
    
    if( CSLFetchNameValue(poOpenInfo->papszOpenOptions, "ROOT_PATH") != NULL )
    {
        CPLFree(pszVRTPath);
        pszVRTPath = CPLStrdup(CSLFetchNameValue(poOpenInfo->papszOpenOptions, "ROOT_PATH"));
    }

/* -------------------------------------------------------------------- */
/*      Turn the XML representation into a VRTDataset.                  */
/* -------------------------------------------------------------------- */
    VRTDataset *poDS = (VRTDataset *) OpenXML( pszXML, pszVRTPath, poOpenInfo->eAccess );

    if( poDS != NULL )
        poDS->bNeedsFlush = FALSE;

    CPLFree( pszXML );
    CPLFree( pszVRTPath );

/* -------------------------------------------------------------------- */
/*      Open overviews.                                                 */
/* -------------------------------------------------------------------- */
    if( fp != NULL && poDS != NULL )
        poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename,
                                     poOpenInfo->GetSiblingFiles() );

    return poDS;
}

/************************************************************************/
/*                              OpenXML()                               */
/*                                                                      */
/*      Create an open VRTDataset from a supplied XML representation    */
/*      of the dataset.                                                 */
/************************************************************************/

GDALDataset *VRTDataset::OpenXML( const char *pszXML, const char *pszVRTPath,
                                  GDALAccess eAccess)

{
 /* -------------------------------------------------------------------- */
 /*      Parse the XML.                                                  */
 /* -------------------------------------------------------------------- */
    CPLXMLNode	*psTree;

    psTree = CPLParseXMLString( pszXML );

    if( psTree == NULL )
        return NULL;

    CPLXMLNode *psRoot = CPLGetXMLNode( psTree, "=VRTDataset" );
    if (psRoot == NULL)
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Missing VRTDataset element." );
        CPLDestroyXMLNode( psTree );
        return NULL;
    }

    int bIsPansharpened = strstr(pszXML,"VRTPansharpenedDataset") != NULL;
    if( !bIsPansharpened &&
        (CPLGetXMLNode( psRoot, "rasterXSize" ) == NULL
        || CPLGetXMLNode( psRoot, "rasterYSize" ) == NULL
        || CPLGetXMLNode( psRoot, "VRTRasterBand" ) == NULL) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Missing one of rasterXSize, rasterYSize or bands on"
                  " VRTDataset." );
        CPLDestroyXMLNode( psTree );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the new virtual dataset object.                          */
/* -------------------------------------------------------------------- */
    VRTDataset *poDS;
    int nXSize = atoi(CPLGetXMLValue(psRoot,"rasterXSize","0"));
    int nYSize = atoi(CPLGetXMLValue(psRoot,"rasterYSize","0"));
    
    if ( !bIsPansharpened &&
        !GDALCheckDatasetDimensions(nXSize, nYSize) )
    {
        CPLDestroyXMLNode( psTree );
        return NULL;
    }

    if( strstr(pszXML,"VRTWarpedDataset") != NULL )
        poDS = new VRTWarpedDataset( nXSize, nYSize );
    else if( bIsPansharpened )
        poDS = new VRTPansharpenedDataset( nXSize, nYSize );
    else
    {
        poDS = new VRTDataset( nXSize, nYSize );
        poDS->eAccess = eAccess;
    }

    if( poDS->XMLInit( psRoot, pszVRTPath ) != CE_None )
    {
        delete poDS;
        poDS = NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try to return a regular handle on the file.                     */
/* -------------------------------------------------------------------- */
    CPLDestroyXMLNode( psTree );

    return poDS;
}

/************************************************************************/
/*                              AddBand()                               */
/************************************************************************/

CPLErr VRTDataset::AddBand( GDALDataType eType, char **papszOptions )

{
    int i;

    const char *pszSubClass = CSLFetchNameValue(papszOptions, "subclass");

    bNeedsFlush = 1;

/* ==================================================================== */
/*      Handle a new raw band.                                          */
/* ==================================================================== */
    if( pszSubClass != NULL && EQUAL(pszSubClass,"VRTRawRasterBand") )
    {
        int nWordDataSize = GDALGetDataTypeSize( eType ) / 8;
        vsi_l_offset nImageOffset = 0;
        int nPixelOffset = nWordDataSize;
        int nLineOffset = nWordDataSize * GetRasterXSize();
        const char *pszFilename;
        const char *pszByteOrder = NULL;
        int bRelativeToVRT = FALSE;

/* -------------------------------------------------------------------- */
/*      Collect required information.                                   */
/* -------------------------------------------------------------------- */
        if( CSLFetchNameValue(papszOptions, "ImageOffset") != NULL )
            nImageOffset = CPLScanUIntBig(
                CSLFetchNameValue(papszOptions, "ImageOffset"), 20);

        if( CSLFetchNameValue(papszOptions, "PixelOffset") != NULL )
            nPixelOffset = atoi(CSLFetchNameValue(papszOptions,"PixelOffset"));

        if( CSLFetchNameValue(papszOptions, "LineOffset") != NULL )
            nLineOffset = atoi(CSLFetchNameValue(papszOptions, "LineOffset"));

        if( CSLFetchNameValue(papszOptions, "ByteOrder") != NULL )
            pszByteOrder = CSLFetchNameValue(papszOptions, "ByteOrder");

        if( CSLFetchNameValue(papszOptions, "SourceFilename") != NULL )
            pszFilename = CSLFetchNameValue(papszOptions, "SourceFilename");
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "AddBand() requires a SourceFilename option for VRTRawRasterBands." );
            return CE_Failure;
        }
        
        bRelativeToVRT = 
            CSLFetchBoolean( papszOptions, "relativeToVRT", FALSE );

/* -------------------------------------------------------------------- */
/*      Create and initialize the band.                                 */
/* -------------------------------------------------------------------- */
        CPLErr eErr;

        VRTRawRasterBand *poBand = 
            new VRTRawRasterBand( this, GetRasterCount() + 1, eType );

        eErr = 
            poBand->SetRawLink( pszFilename, NULL, bRelativeToVRT,
                                nImageOffset, nPixelOffset, nLineOffset, 
                                pszByteOrder );
        if( eErr != CE_None )
        {
            delete poBand;
            return eErr;
        }

        SetBand( GetRasterCount() + 1, poBand );

        return CE_None;
    }

/* ==================================================================== */
/*      Handle a new "sourced" band.                                    */
/* ==================================================================== */
    else
    {
        VRTSourcedRasterBand *poBand;

	/* ---- Check for our sourced band 'derived' subclass ---- */
        if(pszSubClass != NULL && EQUAL(pszSubClass,"VRTDerivedRasterBand")) {

            /* We'll need a pointer to the subclass in case we need */
            /* to set the new band's pixel function below. */
            VRTDerivedRasterBand* poDerivedBand;

            poDerivedBand = new VRTDerivedRasterBand
                (this, GetRasterCount() + 1, eType,
                 GetRasterXSize(), GetRasterYSize());

            /* Set the pixel function options it provided. */
            const char* pszFuncName =
                CSLFetchNameValue(papszOptions, "PixelFunctionType");
            if (pszFuncName != NULL)
                poDerivedBand->SetPixelFunctionName(pszFuncName);

            const char* pszTransferTypeName =
                CSLFetchNameValue(papszOptions, "SourceTransferType");
            if (pszTransferTypeName != NULL) {
                GDALDataType eTransferType =
                    GDALGetDataTypeByName(pszTransferTypeName);
                if (eTransferType == GDT_Unknown) {
                    CPLError( CE_Failure, CPLE_AppDefined,
                              "invalid SourceTransferType: \"%s\".",
                              pszTransferTypeName);
                    delete poDerivedBand;
                    return CE_Failure;
                }
                poDerivedBand->SetSourceTransferType(eTransferType);
            }

            /* We're done with the derived band specific stuff, so */
            /* we can assigned the base class pointer now. */
            poBand = poDerivedBand;
        }
	else {

	    /* ---- Standard sourced band ---- */
	    poBand = new VRTSourcedRasterBand
		(this, GetRasterCount() + 1, eType, 
		 GetRasterXSize(), GetRasterYSize());
	}

        SetBand( GetRasterCount() + 1, poBand );

        for( i=0; papszOptions != NULL && papszOptions[i] != NULL; i++ )
        {
            if( EQUALN(papszOptions[i],"AddFuncSource=", 14) )
            {
                VRTImageReadFunc pfnReadFunc = NULL;
                void             *pCBData = NULL;
                double           dfNoDataValue = VRT_NODATA_UNSET;

                char **papszTokens = CSLTokenizeStringComplex( papszOptions[i]+14,
                                                               ",", TRUE, FALSE );

                if( CSLCount(papszTokens) < 1 )
                {
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "AddFuncSource() ... required argument missing." );
                }

                sscanf( papszTokens[0], "%p", &pfnReadFunc );
                if( CSLCount(papszTokens) > 1 )
                    sscanf( papszTokens[1], "%p", &pCBData );
                if( CSLCount(papszTokens) > 2 )
                    dfNoDataValue = CPLAtof( papszTokens[2] );

                poBand->AddFuncSource( pfnReadFunc, pCBData, dfNoDataValue );
            }
        }

        return CE_None;
    }
}

/************************************************************************/
/*                              VRTAddBand()                            */
/************************************************************************/

/**
 * @see VRTDataset::VRTAddBand().
 */

int CPL_STDCALL VRTAddBand( VRTDatasetH hDataset, GDALDataType eType, 
                            char **papszOptions )

{
    VALIDATE_POINTER1( hDataset, "VRTAddBand", 0 );

    return ((VRTDataset *) hDataset)->AddBand(eType, papszOptions);
}

/************************************************************************/
/*                               Create()                               */
/************************************************************************/

GDALDataset *
VRTDataset::Create( const char * pszName,
                    int nXSize, int nYSize, int nBands,
                    GDALDataType eType, char ** papszOptions )

{
    VRTDataset *poDS = NULL;
    int        iBand = 0;

    (void) papszOptions;

    if( EQUALN(pszName,"<VRTDataset",11) )
    {
        GDALDataset *poDS = OpenXML( pszName, NULL, GA_Update );
        if (poDS)
            poDS->SetDescription( "<FromXML>" );
        return poDS;
    }
    else
    {
        const char *pszSubclass = CSLFetchNameValue( papszOptions,
                                                     "SUBCLASS" );

        if( pszSubclass == NULL || EQUAL(pszSubclass,"VRTDataset") )
            poDS = new VRTDataset( nXSize, nYSize );
        else if( EQUAL(pszSubclass,"VRTWarpedDataset") )
        {
            poDS = new VRTWarpedDataset( nXSize, nYSize );
        }
        else
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                      "SUBCLASS=%s not recognised.", 
                      pszSubclass );
            return NULL;
        }
        poDS->eAccess = GA_Update;

        poDS->SetDescription( pszName );
        
        for( iBand = 0; iBand < nBands; iBand++ )
            poDS->AddBand( eType, NULL );
        
        poDS->bNeedsFlush = 1;

        poDS->oOvManager.Initialize( poDS, pszName );
        
        return poDS;
    }
}

/************************************************************************/
/*                            GetFileList()                             */
/************************************************************************/

char** VRTDataset::GetFileList()
{
    char** papszFileList = GDALDataset::GetFileList();
    
    int nSize = CSLCount(papszFileList);
    int nMaxSize = nSize;
    
    /* Don't need an element desallocator as each string points to an */
    /* element of the papszFileList */
    CPLHashSet* hSetFiles = CPLHashSetNew(CPLHashSetHashStr,
                                          CPLHashSetEqualStr,
                                          NULL);
                                          
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
       ((VRTRasterBand *) papoBands[iBand])->GetFileList(
                                &papszFileList, &nSize, &nMaxSize, hSetFiles);
    }
                                          
    CPLHashSetDestroy(hSetFiles);
    
    return papszFileList;
}

/************************************************************************/
/*                              Delete()                                */
/************************************************************************/

/* We implement Delete() to avoid that the default implementation */
/* in GDALDriver::Delete() destroys the source files listed by GetFileList(),*/
/* which would be an undesired effect... */
CPLErr VRTDataset::Delete( const char * pszFilename )
{
    GDALDriverH hDriver = GDALIdentifyDriver(pszFilename, NULL);
    if (hDriver && EQUAL(GDALGetDriverShortName(hDriver), "VRT"))
    {
        if( strstr(pszFilename, "<VRTDataset") == NULL &&
            VSIUnlink( pszFilename ) != 0 )
        {
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Deleting %s failed:\n%s",
                      pszFilename,
                      VSIStrerror(errno) );
            return CE_Failure;
        }
        
        return CE_None;
    }
    else
        return CE_Failure;
}

/************************************************************************/
/*                          CreateMaskBand()                            */
/************************************************************************/

CPLErr VRTDataset::CreateMaskBand( CPL_UNUSED int nFlags )
{
    if (poMaskBand != NULL)
    {
        CPLError(CE_Failure, CPLE_AppDefined,
                 "This VRT dataset has already a mask band");
        return CE_Failure;
    }

    SetMaskBand(new VRTSourcedRasterBand( this, 0 ));

    return CE_None;
}

/************************************************************************/
/*                           SetMaskBand()                              */
/************************************************************************/

void VRTDataset::SetMaskBand(VRTRasterBand* poMaskBand)
{
    delete this->poMaskBand;
    this->poMaskBand = poMaskBand;
    poMaskBand->SetIsMaskBand();
}

/************************************************************************/
/*                        CloseDependentDatasets()                      */
/************************************************************************/

int VRTDataset::CloseDependentDatasets()
{
    /* We need to call it before removing the sources, otherwise */
    /* we would remove them from the serizalized VRT */
    FlushCache();

    int bHasDroppedRef = GDALDataset::CloseDependentDatasets();

    for( int iBand = 0; iBand < nBands; iBand++ )
    {
       bHasDroppedRef |= ((VRTRasterBand *) papoBands[iBand])->
                                                CloseDependentDatasets();
    }
    return bHasDroppedRef;
}

/************************************************************************/
/*                      CheckCompatibleForDatasetIO()                   */
/************************************************************************/

/* We will return TRUE only if all the bands are VRTSourcedRasterBands */
/* made of identical sources, that are strictly VRTSimpleSource, and that */
/* the band number of each source is the band number of the VRTSouredRasterBand */

int VRTDataset::CheckCompatibleForDatasetIO()
{
    int iBand;
    int nSources = 0;
    VRTSource    **papoSources = NULL;
    CPLString osResampling;
    for(iBand = 0; iBand < nBands; iBand++)
    {
        if (!((VRTRasterBand *) papoBands[iBand])->IsSourcedRasterBand())
            return FALSE;

        VRTSourcedRasterBand* poBand = (VRTSourcedRasterBand* )papoBands[iBand];

        if (iBand == 0)
        {
            nSources = poBand->nSources;
            papoSources = poBand->papoSources;
            for(int iSource = 0; iSource < nSources; iSource++)
            {
                if (!papoSources[iSource]->IsSimpleSource())
                    return FALSE;

                VRTSimpleSource* poSource = (VRTSimpleSource* )papoSources[iSource];
                if (!EQUAL(poSource->GetType(), "SimpleSource"))
                    return FALSE;

                GDALRasterBand *srcband = poSource->GetBand();
                if (srcband == NULL)
                    return FALSE;
                if (srcband->GetDataset() == NULL)
                    return FALSE;
                if (srcband->GetDataset()->GetRasterCount() <= iBand)
                    return FALSE;
                if (srcband->GetDataset()->GetRasterBand(iBand + 1) != srcband)
                    return FALSE;
                osResampling = poSource->GetResampling();
            }
        }
        else if (nSources != poBand->nSources)
        {
            return FALSE;
        }
        else
        {
            for(int iSource = 0; iSource < nSources; iSource++)
            {
                if (!poBand->papoSources[iSource]->IsSimpleSource())
                    return FALSE;
                VRTSimpleSource* poRefSource = (VRTSimpleSource* )papoSources[iSource];
                VRTSimpleSource* poSource = (VRTSimpleSource* )poBand->papoSources[iSource];
                if (!EQUAL(poSource->GetType(), "SimpleSource"))
                    return FALSE;
                if (!poSource->IsSameExceptBandNumber(poRefSource))
                    return FALSE;

                GDALRasterBand *srcband = poSource->GetBand();
                if (srcband == NULL)
                    return FALSE;
                if (srcband->GetDataset() == NULL)
                    return FALSE;
                if (srcband->GetDataset()->GetRasterCount() <= iBand)
                    return FALSE;
                if (srcband->GetDataset()->GetRasterBand(iBand + 1) != srcband)
                    return FALSE;
                if (osResampling.compare(poSource->GetResampling()) != 0)
                    return FALSE;
            }
        }
    }

    return nSources != 0;
}

/************************************************************************/
/*                         GetSingleSimpleSource()                      */
/*                                                                      */
/* Returns a non-NULL dataset if the VRT is made of a single source     */
/* that is a simple source, in its full extent, and with all of its     */
/* bands. Basically something produced by :                             */
/*   gdal_translate src dst.vrt -of VRT (-a_srs / -a_ullr)              */
/************************************************************************/

GDALDataset* VRTDataset::GetSingleSimpleSource()
{
    if (!CheckCompatibleForDatasetIO())
        return NULL;

    VRTSourcedRasterBand* poVRTBand = (VRTSourcedRasterBand* )papoBands[0];
    if( poVRTBand->nSources != 1 )
        return NULL;
    VRTSimpleSource* poSource = (VRTSimpleSource* )poVRTBand->papoSources[0];
    GDALRasterBand* poBand = poSource->GetBand();
    if (poBand == NULL)
        return NULL;
    GDALDataset* poSrcDS = poBand->GetDataset();
    if (poSrcDS == NULL)
        return NULL;

    /* Check that it uses the full source dataset */
    double dfReqXOff, dfReqYOff, dfReqXSize, dfReqYSize;
    int nReqXOff, nReqYOff, nReqXSize, nReqYSize;
    int nOutXOff, nOutYOff, nOutXSize, nOutYSize;
    poSource->GetSrcDstWindow( 0, 0,
                               poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(),
                               poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize(),
                               &dfReqXOff, &dfReqYOff, &dfReqXSize, &dfReqYSize,
                               &nReqXOff, &nReqYOff,
                               &nReqXSize, &nReqYSize,
                               &nOutXOff, &nOutYOff,
                               &nOutXSize, &nOutYSize );

    if (nReqXOff != 0 || nReqYOff != 0 ||
        nReqXSize != poSrcDS->GetRasterXSize() ||
        nReqYSize != poSrcDS->GetRasterYSize())
        return NULL;

    if (nOutXOff != 0 || nOutYOff != 0 ||
        nOutXSize != poSrcDS->GetRasterXSize() ||
        nOutYSize != poSrcDS->GetRasterYSize())
        return NULL;

    return poSrcDS;
}

/************************************************************************/
/*                              IRasterIO()                             */
/************************************************************************/

CPLErr VRTDataset::IRasterIO( GDALRWFlag eRWFlag,
                               int nXOff, int nYOff, int nXSize, int nYSize,
                               void * pData, int nBufXSize, int nBufYSize,
                               GDALDataType eBufType,
                               int nBandCount, int *panBandMap,
                               GSpacing nPixelSpace, GSpacing nLineSpace,
                               GSpacing nBandSpace,
                               GDALRasterIOExtraArg* psExtraArg)
{
    if (bCompatibleForDatasetIO < 0)
    {
        bCompatibleForDatasetIO = CheckCompatibleForDatasetIO();
    }

    int bLocalCompatibleForDatasetIO = bCompatibleForDatasetIO;
    if (bLocalCompatibleForDatasetIO && eRWFlag == GF_Read &&
        (nBufXSize < nXSize || nBufYSize < nYSize))
    {
        int bTried;
        CPLErr eErr = TryOverviewRasterIO( eRWFlag,
                                    nXOff, nYOff, nXSize, nYSize,
                                    pData, nBufXSize, nBufYSize,
                                    eBufType,
                                    nBandCount, panBandMap,
                                    nPixelSpace, nLineSpace,
                                    nBandSpace,
                                    psExtraArg,
                                    &bTried );
        if( bTried )
            return eErr;

        for(int iBand = 0; iBand < nBands; iBand++)
        {
            VRTSourcedRasterBand* poBand = (VRTSourcedRasterBand* )papoBands[iBand];

            /* If there are overviews, let's VRTSourcedRasterBand::IRasterIO() */
            /* do the job */
            if( poBand->GetOverviewCount() != 0 )
            {
                bLocalCompatibleForDatasetIO = FALSE;
                break;
            }
        }
    }

    // If resampling with non-nearest neighbour, we need to be carefull
    // if the VRT band exposes a nodata value, but the sources do not have it
    if (bLocalCompatibleForDatasetIO && eRWFlag == GF_Read &&
        (nXSize != nBufXSize || nYSize != nBufYSize) &&
        psExtraArg->eResampleAlg != GRIORA_NearestNeighbour )
    {
        for(int iBandIndex=0; iBandIndex<nBandCount; iBandIndex++)
        {
            VRTSourcedRasterBand* poBand =
                    (VRTSourcedRasterBand*)GetRasterBand(panBandMap[iBandIndex]);
            int bHasNoData = FALSE;
            double dfNoDataValue = poBand->GetNoDataValue(&bHasNoData);
            if( bHasNoData )
            {
                for( int i = 0; i < poBand->nSources; i++ )
                {
                    VRTSimpleSource* poSource = (VRTSimpleSource*)poBand->papoSources[i];
                    int bSrcHasNoData = FALSE;
                    double dfSrcNoData = poSource->GetBand()->GetNoDataValue(&bSrcHasNoData);
                    if( !bSrcHasNoData || dfSrcNoData != dfNoDataValue )
                    {
                        bLocalCompatibleForDatasetIO = FALSE;
                        break;
                    }
                }
                if( !bLocalCompatibleForDatasetIO )
                    break;
            }
        }
    }


    if (bLocalCompatibleForDatasetIO && eRWFlag == GF_Read)
    {
        for(int iBandIndex=0; iBandIndex<nBandCount; iBandIndex++)
        {
            VRTSourcedRasterBand* poBand =
                    (VRTSourcedRasterBand*)GetRasterBand(panBandMap[iBandIndex]);

            /* Dirty little trick to initialize the buffer without doing */
            /* any real I/O */
            int nSavedSources = poBand->nSources;
            poBand->nSources = 0;

            GByte *pabyBandData = ((GByte *) pData) + iBandIndex * nBandSpace;
            poBand->IRasterIO(GF_Read, nXOff, nYOff, nXSize, nYSize,
                                pabyBandData, nBufXSize, nBufYSize,
                                eBufType,
                                nPixelSpace, nLineSpace, psExtraArg);

            poBand->nSources = nSavedSources;
        }

        CPLErr eErr = CE_None;

        GDALProgressFunc  pfnProgressGlobal = psExtraArg->pfnProgress;
        void             *pProgressDataGlobal = psExtraArg->pProgressData;

        /* Use the last band, because when sources reference a GDALProxyDataset, they */
        /* don't necessary instanciate all underlying rasterbands */
        VRTSourcedRasterBand* poBand = (VRTSourcedRasterBand* )papoBands[nBands - 1];
        for(int iSource = 0; eErr == CE_None && iSource < poBand->nSources; iSource++)
        {
            psExtraArg->pfnProgress = GDALScaledProgress;
            psExtraArg->pProgressData = 
                GDALCreateScaledProgress( 1.0 * iSource / poBand->nSources,
                                        1.0 * (iSource + 1) / poBand->nSources,
                                        pfnProgressGlobal,
                                        pProgressDataGlobal );

            VRTSimpleSource* poSource = (VRTSimpleSource* )poBand->papoSources[iSource];
            eErr = poSource->DatasetRasterIO( nXOff, nYOff, nXSize, nYSize,
                                              pData, nBufXSize, nBufYSize,
                                              eBufType,
                                              nBandCount, panBandMap,
                                              nPixelSpace, nLineSpace, nBandSpace,
                                              psExtraArg);

            GDALDestroyScaledProgress( psExtraArg->pProgressData );
        }

        psExtraArg->pfnProgress = pfnProgressGlobal;
        psExtraArg->pProgressData = pProgressDataGlobal;

        return eErr;
    }

    return GDALDataset::IRasterIO(eRWFlag, nXOff, nYOff, nXSize, nYSize,
                                  pData, nBufXSize, nBufYSize,
                                  eBufType,
                                  nBandCount, panBandMap,
                                  nPixelSpace, nLineSpace, nBandSpace, psExtraArg);
}

/************************************************************************/
/*                  UnsetPreservedRelativeFilenames()                   */
/************************************************************************/

void VRTDataset::UnsetPreservedRelativeFilenames()
{
    for(int iBand = 0; iBand < nBands; iBand++)
    {
        if (!((VRTRasterBand *) papoBands[iBand])->IsSourcedRasterBand())
            continue;

        VRTSourcedRasterBand* poBand = (VRTSourcedRasterBand* )papoBands[iBand];
        int nSources = poBand->nSources;
        VRTSource** papoSources = poBand->papoSources;
        for(int iSource = 0; iSource < nSources; iSource++)
        {
            if (!papoSources[iSource]->IsSimpleSource())
                continue;

            VRTSimpleSource* poSource = (VRTSimpleSource* )papoSources[iSource];
            poSource->UnsetPreservedRelativeFilenames();
        }
    }
}

/************************************************************************/
/*                        BuildVirtualOverviews()                       */
/************************************************************************/

void VRTDataset::BuildVirtualOverviews()
{
    // Currently we expose virtual overviews only if the dataset is made of
    // a single SimpleSource/ComplexSource, in each band.
    // And if the underlying sources have overviews of course
    if( apoOverviews.size() || apoOverviewsBak.size() )
        return;
    
    int nOverviews = 0;
    GDALRasterBand* poFirstBand = NULL;
    for(int iBand = 0; iBand < nBands; iBand++)
    {
        if (!((VRTRasterBand *) papoBands[iBand])->IsSourcedRasterBand())
            return;

        VRTSourcedRasterBand* poVRTBand = (VRTSourcedRasterBand* )papoBands[iBand];
        if( poVRTBand->nSources != 1 )
            return;
        if (!poVRTBand->papoSources[0]->IsSimpleSource())
            return;

        VRTSimpleSource* poSource = (VRTSimpleSource* )poVRTBand->papoSources[0];
        if (!EQUAL(poSource->GetType(), "SimpleSource") &&
            !EQUAL(poSource->GetType(), "ComplexSource"))
            return;
        GDALRasterBand* poSrcBand = poSource->GetBand();
        if (poSrcBand == NULL)
            return;
        if( poSrcBand->GetOverviewCount() == 0 )
            return;
        if( iBand == 0 )
        {
            poFirstBand = poSrcBand;
            nOverviews = poSrcBand->GetOverviewCount();
        }
        else if( poSrcBand->GetOverviewCount() < nOverviews )
            nOverviews = poSrcBand->GetOverviewCount();
    }

    for(int j=0;j<nOverviews;j++)
    {
        double dfXRatio = (double)poFirstBand->GetOverview(j)->GetXSize() / poFirstBand->GetXSize();
        double dfYRatio = (double)poFirstBand->GetOverview(j)->GetYSize() / poFirstBand->GetYSize();
        int nOvrXSize = (int)(0.5 + nRasterXSize * dfXRatio);
        int nOvrYSize = (int)(0.5 + nRasterYSize * dfYRatio);
        if( nOvrXSize < 128 || nOvrYSize < 128 )
            break;
        VRTDataset* poOvrVDS = new VRTDataset(nOvrXSize, nOvrYSize);
        apoOverviews.push_back(poOvrVDS);

        for(int i=0;i<nBands;i++)
        {
            VRTSourcedRasterBand* poVRTBand = (VRTSourcedRasterBand* )GetRasterBand(i+1);
            VRTSourcedRasterBand* poOvrVRTBand = new VRTSourcedRasterBand(
                poOvrVDS, poOvrVDS->GetRasterCount() + 1, poVRTBand->GetRasterDataType(), 
                nOvrXSize, nOvrYSize);
            poOvrVDS->SetBand( poOvrVDS->GetRasterCount() + 1, poOvrVRTBand );

            VRTSimpleSource* poSrcSource = (VRTSimpleSource* )poVRTBand->papoSources[0];
            VRTSimpleSource* poNewSource = NULL;
            if( EQUAL(poSrcSource->GetType(), "SimpleSource") )
                poNewSource = new VRTSimpleSource(poSrcSource, dfXRatio, dfYRatio);
            else if( EQUAL(poSrcSource->GetType(), "ComplexSource") )
                poNewSource = new VRTComplexSource((VRTComplexSource*)poSrcSource, dfXRatio, dfYRatio);
            else
                CPLAssert(FALSE);
            if( poNewSource->GetBand()->GetDataset() )
                poNewSource->GetBand()->GetDataset()->Reference();
            poOvrVRTBand->AddSource(poNewSource);
        }
    }
}

/************************************************************************/
/*                          IBuildOverviews()                           */
/************************************************************************/

CPLErr
VRTDataset::IBuildOverviews( const char *pszResampling,
                             int nOverviews,
                             int *panOverviewList,
                             int nListBands,
                             int *panBandList,
                             GDALProgressFunc pfnProgress,
                             void * pProgressData )
{
    /* Make implicit overviews invisible, but do not destroy */
    /* them in case they are already used (not sure that the client */
    /* has the right to do that. behaviour undefined in GDAL API I think) */
    if( apoOverviews.size() )
    {
        apoOverviewsBak = apoOverviews;
        apoOverviews.resize(0);
    }
    else
    {
        // Add a dummy overview so that GDALDataset::IBuildOverviews()
        // doesn't manage to get a virtual implicit overview
        apoOverviews.push_back(NULL);
    }

    return GDALDataset::IBuildOverviews(pszResampling,
                                               nOverviews,
                                               panOverviewList,
                                               nListBands,
                                               panBandList,
                                               pfnProgress,
                                               pProgressData);
}
