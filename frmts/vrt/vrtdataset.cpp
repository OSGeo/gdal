/******************************************************************************
 * $Id$
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTDataset
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2001, Frank Warmerdam <warmerdam@pobox.com>
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
 * Revision 1.24  2006/02/08 06:12:07  fwarmerdam
 * Override SetMetadata methods so that metadata can be preserved.
 * Support saving histograms in VRT per bug 1060.
 *
 * Revision 1.23  2005/10/28 16:59:50  pnagy
 * Added VRTDerivedBand support
 *
 * Revision 1.22  2005/06/28 14:50:44  fwarmerdam
 * avoid destroy psTree in case of failure - done at higher level
 *
 * Revision 1.21  2005/05/05 13:56:34  fwarmerdam
 * moved metadata handling to PAM
 *
 * Revision 1.20  2005/04/11 17:08:36  fwarmerdam
 * Fixed leak of xml tree when XMLInit() fails.
 *
 * Revision 1.19  2004/10/03 16:52:45  fwarmerdam
 * ensure overviews initialized in Create()
 *
 * Revision 1.18  2004/08/11 20:34:52  warmerda
 * Allow "user input" for projection information in .vrt file.
 *
 * Revision 1.17  2004/08/11 18:43:50  warmerda
 * restructure init to support derived class, add pszVRTPath to serialize
 *
 * Revision 1.16  2004/07/30 21:51:29  warmerda
 * added support for VRTRawRasterBand
 *
 * Revision 1.15  2004/07/27 21:59:48  warmerda
 * Enable .ovr support.
 *
 * Revision 1.14  2004/03/16 18:34:35  warmerda
 * added support for relativeToVRT attribute on SourceFilename
 *
 * Revision 1.13  2003/07/17 20:31:12  warmerda
 * moved out driver and VRTCreateCopy() code
 *
 * Revision 1.12  2003/06/10 19:58:35  warmerda
 * added support for AddFuncSource in AddBand() method for Imagine
 *
 * Revision 1.11  2002/12/05 22:17:19  warmerda
 * added support for opening directly from XML
 *
 * Revision 1.10  2002/11/30 16:55:49  warmerda
 * added OpenXML method
 *
 * Revision 1.9  2002/11/24 04:27:52  warmerda
 * CopyCreate() nows saves source image directly if it is a VRTDataset
 *
 * Revision 1.8  2002/11/23 18:54:17  warmerda
 * added CREATIONDATATYPES metadata for drivers
 *
 * Revision 1.7  2002/09/04 06:50:37  warmerda
 * avoid static driver pointers
 *
 * Revision 1.6  2002/06/12 21:12:25  warmerda
 * update to metadata based driver info
 *
 * Revision 1.5  2002/05/29 16:40:18  warmerda
 * dont provide default value for method argumentsvrtdataset.cpp
 *
 * Revision 1.4  2002/05/29 16:06:05  warmerda
 * complete detailed band metadata
 *
 * Revision 1.3  2002/04/19 19:43:38  warmerda
 * added CreateCopy method
 *
 * Revision 1.2  2001/11/18 15:46:45  warmerda
 * added SRS and GeoTransform
 *
 * Revision 1.1  2001/11/16 21:14:31  warmerda
 * New
 *
 */

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

    bGeoTransformSet = FALSE;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;

    nGCPCount = 0;
    pasGCPList = NULL;
    pszGCPProjection = CPLStrdup("");

    pszVRTPath = NULL;
    
    GDALRegister_VRT();
    poDriver = (GDALDriver *) GDALGetDriverByName( "VRT" );
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
}

/************************************************************************/
/*                             FlushCache()                             */
/************************************************************************/

void VRTDataset::FlushCache()

{
    GDALDataset::FlushCache();

    if( !bNeedsFlush )
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
    FILE *fpVRT;

    fpVRT = VSIFOpen( GetDescription(), "w" );
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
    VSIFWrite( pszXML, 1, strlen(pszXML), fpVRT );
    VSIFClose( fpVRT );

    CPLFree( pszXML );
}

/************************************************************************/
/*                           SerializeToXML()                           */
/************************************************************************/

CPLXMLNode *VRTDataset::SerializeToXML( const char *pszVRTPath )

{
    /* -------------------------------------------------------------------- */
    /*      Setup root node and attributes.                                 */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psDSTree, *psMD;
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
    psMD = PamSerializeMetadata( this );
    if( psMD != NULL )
        CPLAddXMLChild( psDSTree, psMD );

 /* -------------------------------------------------------------------- */
 /*      GCPs                                                            */
 /* -------------------------------------------------------------------- */
    if( nGCPCount > 0 )
    {
        CPLXMLNode *psGCPList = CPLCreateXMLNode( psDSTree, CXT_Element, 
                                                  "GCPList" );

        if( pszGCPProjection != NULL && strlen(pszGCPProjection) > 0 )
            CPLSetXMLValue( psGCPList, "#Projection", pszGCPProjection );

        for( int iGCP = 0; iGCP < nGCPCount; iGCP++ )
        {
            CPLXMLNode *psXMLGCP;
            GDAL_GCP *psGCP = pasGCPList + iGCP;

            psXMLGCP = CPLCreateXMLNode( psGCPList, CXT_Element, "GCP" );

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
    /*      Serialize bands.                                                */
    /* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < nBands; iBand++ )
    {
        CPLXMLNode *psBandTree = 
            ((VRTRasterBand *) papoBands[iBand])->SerializeToXML(pszVRTPath);

        if( psBandTree != NULL )
            CPLAddXMLChild( psDSTree, psBandTree );
    }

    return psDSTree;
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
                adfGeoTransform[iTA] = atof(papszTokens[iTA]);
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
        CPLXMLNode *psXMLGCP;
        OGRSpatialReference oSRS;
        const char *pszRawProj = CPLGetXMLValue(psGCPList, "Projection", "");

        CPLFree( pszGCPProjection );

        if( strlen(pszRawProj) > 0 
            && oSRS.SetFromUserInput( pszRawProj ) == OGRERR_NONE )
            oSRS.exportToWkt( &pszGCPProjection );
        else
            pszGCPProjection = CPLStrdup("");

        // Count GCPs.
        int  nGCPMax = 0;
         
        for( psXMLGCP = psGCPList->psChild; psXMLGCP != NULL; 
             psXMLGCP = psXMLGCP->psNext )
            nGCPMax++;
         
        pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),nGCPMax);
         
        for( psXMLGCP = psGCPList->psChild; psXMLGCP != NULL; 
             psXMLGCP = psXMLGCP->psNext )
        {
            GDAL_GCP *psGCP = pasGCPList + nGCPCount;

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

            nGCPCount++;
        }
    }
     
/* -------------------------------------------------------------------- */
/*      Apply any dataset level metadata.                               */
/* -------------------------------------------------------------------- */
    PamApplyMetadata( psTree, this );

/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    int		nBands = 0;
    CPLXMLNode *psChild;

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
/*                                Open()                                */
/************************************************************************/

GDALDataset *VRTDataset::Open( GDALOpenInfo * poOpenInfo )

{
    char *pszVRTPath = NULL;

/* -------------------------------------------------------------------- */
/*      Does this appear to be a virtual dataset definition XML         */
/*      file?                                                           */
/* -------------------------------------------------------------------- */
    if( (poOpenInfo->nHeaderBytes < 20 
         || !EQUALN((const char *)poOpenInfo->pabyHeader,"<VRTDataset",11))
        && !EQUALN(poOpenInfo->pszFilename,"<VRTDataset",11) )
        return NULL;

/* -------------------------------------------------------------------- */
/*	Try to read the whole file into memory.				*/
/* -------------------------------------------------------------------- */
    char        *pszXML;

    if( poOpenInfo->fp != NULL )
    {
        unsigned int nLength;
     
        VSIFSeek( poOpenInfo->fp, 0, SEEK_END );
        nLength = VSIFTell( poOpenInfo->fp );
        VSIFSeek( poOpenInfo->fp, 0, SEEK_SET );
        
        nLength = MAX(0,nLength);
        pszXML = (char *) VSIMalloc(nLength+1);
        
        if( pszXML == NULL )
        {
            CPLError( CE_Failure, CPLE_OutOfMemory, 
                      "Failed to allocate %d byte buffer to hold VRT xml file.",
                      nLength );
            return NULL;
        }
        
        if( VSIFRead( pszXML, 1, nLength, poOpenInfo->fp ) != nLength )
        {
            CPLFree( pszXML );
            CPLError( CE_Failure, CPLE_FileIO,
                      "Failed to read %d bytes from VRT xml file.",
                      nLength );
            return NULL;
        }
        
        pszXML[nLength] = '\0';
        pszVRTPath = CPLStrdup(CPLGetPath(poOpenInfo->pszFilename));
    }
/* -------------------------------------------------------------------- */
/*      Or use the filename as the XML input.                           */
/* -------------------------------------------------------------------- */
    else
    {
        pszXML = CPLStrdup( poOpenInfo->pszFilename );
    }

/* -------------------------------------------------------------------- */
/*      Turn the XML representation into a VRTDataset.                  */
/* -------------------------------------------------------------------- */
    VRTDataset *poDS = (VRTDataset *) OpenXML( pszXML, pszVRTPath );

    if( poDS != NULL )
        poDS->bNeedsFlush = FALSE;

    CPLFree( pszXML );
    CPLFree( pszVRTPath );

/* -------------------------------------------------------------------- */
/*      Open overviews.                                                 */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->fp != NULL && poDS != NULL )
        poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return poDS;
}

/************************************************************************/
/*                              OpenXML()                               */
/*                                                                      */
/*      Create an open VRTDataset from a supplied XML representation    */
/*      of the dataset.                                                 */
/************************************************************************/

GDALDataset *VRTDataset::OpenXML( const char *pszXML, const char *pszVRTPath )

{
 /* -------------------------------------------------------------------- */
 /*      Parse the XML.                                                  */
 /* -------------------------------------------------------------------- */
    CPLXMLNode	*psTree;

    psTree = CPLParseXMLString( pszXML );

    if( psTree == NULL )
        return NULL;

    if( CPLGetXMLNode( psTree, "rasterXSize" ) == NULL
        || CPLGetXMLNode( psTree, "rasterYSize" ) == NULL
        || CPLGetXMLNode( psTree, "VRTRasterBand" ) == NULL )
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
    int nXSize = atoi(CPLGetXMLValue(psTree,"rasterXSize","0"));
    int nYSize = atoi(CPLGetXMLValue(psTree,"rasterYSize","0"));

    if( strstr(pszXML,"VRTWarpedDataset") != NULL )
        poDS = new VRTWarpedDataset( nXSize, nYSize );
    else
        poDS = new VRTDataset( nXSize, nYSize );

    if( poDS->XMLInit( psTree, pszVRTPath ) != CE_None )
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
            nImageOffset = atoi(CSLFetchNameValue(papszOptions, "ImageOffset"));

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
            CSLFetchBoolean( papszOptions, "RelativeToVRT", FALSE );

/* -------------------------------------------------------------------- */
/*      Create and initialize the band.                                 */
/* -------------------------------------------------------------------- */
        CPLErr eErr;

        VRTRawRasterBand *poBand = 
            new VRTRawRasterBand( this, GetRasterCount() + 1, eType );

        eErr = 
            poBand->SetRawLink( pszFilename, NULL, FALSE, 
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
	    poBand = new VRTDerivedRasterBand
		(this, GetRasterCount() + 1, eType, 
		 GetRasterXSize(), GetRasterYSize());
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
                    dfNoDataValue = atof( papszTokens[2] );

                poBand->AddFuncSource( pfnReadFunc, pCBData, dfNoDataValue );
            }
        }

        return CE_None;
    }
}

/************************************************************************/
/*                             VRTCreate()                              */
/************************************************************************/

GDALDataset *
VRTDataset::Create( const char * pszName,
                    int nXSize, int nYSize, int nBands,
                    GDALDataType eType, char ** papszOptions )

{
    VRTDataset *poDS;
    int        iBand;

    (void) papszOptions;

    if( EQUALN(pszName,"<VRTDataset",11) )
    {
        GDALDataset *poDS = OpenXML( pszName, NULL );
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

        poDS->SetDescription( pszName );
        
        for( iBand = 0; iBand < nBands; iBand++ )
            poDS->AddBand( eType, NULL );
        
        poDS->bNeedsFlush = 1;

        poDS->oOvManager.Initialize( poDS, pszName );
        
        return poDS;
    }
}

