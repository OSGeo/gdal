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
#include "cpl_string.h"

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
    if( strlen(GetDescription()) == 0 )
        return;

    /* -------------------------------------------------------------------- */
    /*      Create the output file.                                         */
    /* -------------------------------------------------------------------- */
    FILE *fpVRT;

    fpVRT = VSIFOpen( GetDescription(), "w" );


    /* -------------------------------------------------------------------- */
    /*      Convert tree to a single block of XML text.                     */
    /* -------------------------------------------------------------------- */
    CPLXMLNode *psDSTree = SerializeToXML();
    char *pszXML;

    pszXML = CPLSerializeXMLTree( psDSTree );

    CPLDestroyXMLNode( psDSTree );

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

CPLXMLNode *VRTDataset::SerializeToXML()

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
    psMD = VRTSerializeMetadata( this );
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
        CPLXMLNode *psBandTree;

        psBandTree = ((VRTRasterBand *) papoBands[iBand])->SerializeToXML();
        if( psBandTree != NULL )
            CPLAddXMLChild( psDSTree, psBandTree );
    }

    return psDSTree;
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
/*                                Open()                                */
/************************************************************************/

GDALDataset *VRTDataset::Open( GDALOpenInfo * poOpenInfo )

{
    /* -------------------------------------------------------------------- */
    /*      Does this appear to be a virtual dataset definition XML         */
    /*      file?                                                           */
    /* -------------------------------------------------------------------- */
    if( poOpenInfo->nHeaderBytes < 20 
        || !EQUALN((const char *)poOpenInfo->pabyHeader,"<VRTDataset",11) )
        return NULL;

 /* -------------------------------------------------------------------- */
 /*	Try to read the whole file into memory.				*/
 /* -------------------------------------------------------------------- */
    unsigned int nLength;
    char        *pszXML;
     
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

 /* -------------------------------------------------------------------- */
 /*      Parse the XML.                                                  */
 /* -------------------------------------------------------------------- */
    CPLXMLNode	*psTree;

    psTree = CPLParseXMLString( pszXML );
    CPLFree( pszXML );

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

    poDS = new VRTDataset(atoi(CPLGetXMLValue(psTree,"rasterXSize","0")),
                          atoi(CPLGetXMLValue(psTree,"rasterYSize","0")));

    poDS->eAccess = GA_ReadOnly;

    /* -------------------------------------------------------------------- */
    /*	Check for an SRS node.						*/
    /* -------------------------------------------------------------------- */
    if( strlen(CPLGetXMLValue(psTree, "SRS", "")) > 0 )
        poDS->pszProjection = CPLStrdup(CPLGetXMLValue(psTree, "SRS", ""));

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
                poDS->adfGeoTransform[iTA] = atof(papszTokens[iTA]);
            poDS->bGeoTransformSet = TRUE;
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

        CPLFree( poDS->pszGCPProjection );
        poDS->pszGCPProjection =
            CPLStrdup(CPLGetXMLValue(psGCPList,"Projection",""));
         
        // Count GCPs.
        int  nGCPMax = 0;
         
        for( psXMLGCP = psGCPList->psChild; psXMLGCP != NULL; 
             psXMLGCP = psXMLGCP->psNext )
            nGCPMax++;
         
        poDS->pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),nGCPMax);
         
        for( psXMLGCP = psGCPList->psChild; psXMLGCP != NULL; 
             psXMLGCP = psXMLGCP->psNext )
        {
            GDAL_GCP *psGCP = poDS->pasGCPList + poDS->nGCPCount;

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

            poDS->nGCPCount++;
        }
    }
     
/* -------------------------------------------------------------------- */
/*      Apply any dataset level metadata.                               */
/* -------------------------------------------------------------------- */
    VRTApplyMetadata( psTree, poDS );

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
            VRTRasterBand  *poBand;

            poBand = new VRTRasterBand( poDS, nBands+1 );
            if( poBand->XMLInit( psChild ) == CE_None )
            {
                poDS->SetBand( ++nBands, poBand );
            }
            else
            {
                delete poBand; 
                break;
            }
        }
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
    SetBand( GetRasterCount() + 1, 
             new VRTRasterBand( this, GetRasterCount() + 1, eType, 
                                GetRasterXSize(), GetRasterYSize() ) );

    return CE_None;
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

    poDS = new VRTDataset( nXSize, nYSize );
    poDS->SetDescription( pszName );

    for( iBand = 0; iBand < nBands; iBand++ )
        poDS->AddBand( eType, NULL );

    poDS->bNeedsFlush = 1;
    
    return poDS;
}

/************************************************************************/
/*                           VRTCreateCopy()                            */
/************************************************************************/

static GDALDataset *
VRTCreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
               int bStrict, char ** papszOptions, 
               GDALProgressFunc pfnProgress, void * pProgressData )

{
    VRTDataset *poVRTDS;

/* -------------------------------------------------------------------- */
/*      Create the virtual dataset.                                     */
/* -------------------------------------------------------------------- */
    poVRTDS = (VRTDataset *) 
        VRTDataset::Create( pszFilename, 
                            poSrcDS->GetRasterXSize(),
                            poSrcDS->GetRasterYSize(),
                            0, GDT_Byte, NULL );

/* -------------------------------------------------------------------- */
/*      Do we have a geotransform?                                      */
/* -------------------------------------------------------------------- */
    double adfGeoTransform[6];

    if( poSrcDS->GetGeoTransform( adfGeoTransform ) == CE_None )
    {
        poVRTDS->SetGeoTransform( adfGeoTransform );
    }

/* -------------------------------------------------------------------- */
/*      Copy projection                                                 */
/* -------------------------------------------------------------------- */
    poVRTDS->SetProjection( poSrcDS->GetProjectionRef() );

/* -------------------------------------------------------------------- */
/*      Emit dataset level metadata.                                    */
/* -------------------------------------------------------------------- */
    poVRTDS->SetMetadata( poSrcDS->GetMetadata() );

/* -------------------------------------------------------------------- */
/*      GCPs                                                            */
/* -------------------------------------------------------------------- */
    if( poSrcDS->GetGCPCount() > 0 )
    {
        poVRTDS->SetGCPs( poSrcDS->GetGCPCount(), 
                          poSrcDS->GetGCPs(),
                          poSrcDS->GetGCPProjection() );
    }

/* -------------------------------------------------------------------- */
/*      Loop over all the bands.					*/
/* -------------------------------------------------------------------- */
    for( int iBand = 0; iBand < poSrcDS->GetRasterCount(); iBand++ )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand+1 );

/* -------------------------------------------------------------------- */
/*      Create the band with the appropriate band type.                 */
/* -------------------------------------------------------------------- */
        poVRTDS->AddBand( poSrcBand->GetRasterDataType(), NULL );

        VRTRasterBand *poVRTBand = 
			(VRTRasterBand *) poVRTDS->GetRasterBand( iBand+1 );

/* -------------------------------------------------------------------- */
/*      Setup source mapping.                                           */
/* -------------------------------------------------------------------- */
        poVRTBand->AddSimpleSource( poSrcBand );

/* -------------------------------------------------------------------- */
/*      Emit various band level metadata.                               */
/* -------------------------------------------------------------------- */
        poVRTBand->SetMetadata( poSrcBand->GetMetadata() );

        poVRTBand->SetColorTable( poSrcBand->GetColorTable() );
        poVRTBand->SetColorInterpretation(poSrcBand->GetColorInterpretation());

        int bSuccess;

        poSrcBand->GetNoDataValue( &bSuccess );
        if( bSuccess )
            poVRTBand->SetNoDataValue( poSrcBand->GetNoDataValue(NULL) );
    }

    poVRTDS->FlushCache();

    return poVRTDS;
}

/************************************************************************/
/*                          VRTApplyMetadata()                          */
/************************************************************************/
int VRTApplyMetadata( CPLXMLNode *psTree, GDALMajorObject *poMO )

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
/*                        VRTSerializeMetadata()                        */
/************************************************************************/

CPLXMLNode *VRTSerializeMetadata( GDALMajorObject *poMO )

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
/*                          GDALRegister_VRT()                          */
/************************************************************************/

void GDALRegister_VRT()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "VRT" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "VRT" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Virtual Raster" );
        
        poDriver->pfnOpen = VRTDataset::Open;
        poDriver->pfnCreateCopy = VRTCreateCopy;
        poDriver->pfnCreate = VRTDataset::Create;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

