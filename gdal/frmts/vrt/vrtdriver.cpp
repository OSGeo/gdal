/******************************************************************************
 * $Id$
 *
 * Project:  Virtual GDAL Datasets
 * Purpose:  Implementation of VRTDriver
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2009-2013, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "cpl_minixml.h"
#include "cpl_string.h"
#include "gdal_alg_priv.h"

CPL_CVSID("$Id$");

/************************************************************************/
/*                             VRTDriver()                              */
/************************************************************************/

VRTDriver::VRTDriver()

{
    papszSourceParsers = NULL;
#if 0
    pDeserializerData = GDALRegisterTransformDeserializer("WarpedOverviewTransformer",
                                                          VRTWarpedOverviewTransform,
                                                          VRTDeserializeWarpedOverviewTransformer);
#else
    pDeserializerData = NULL;
#endif
}

/************************************************************************/
/*                             ~VRTDriver()                             */
/************************************************************************/

VRTDriver::~VRTDriver()

{
    CSLDestroy( papszSourceParsers );
#if 0
    if ( pDeserializerData )
    {
        GDALUnregisterTransformDeserializer( pDeserializerData );
    }
#endif
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **VRTDriver::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALDriver::GetMetadataDomainList(),
                                   TRUE,
                                   "SourceParsers", NULL);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **VRTDriver::GetMetadata( const char *pszDomain )

{
    if( pszDomain && EQUAL(pszDomain,"SourceParsers") )
        return papszSourceParsers;
    else
        return GDALDriver::GetMetadata( pszDomain );
}

/************************************************************************/
/*                            SetMetadata()                             */
/************************************************************************/

CPLErr VRTDriver::SetMetadata( char **papszMetadata, const char *pszDomain )

{
    if( pszDomain && EQUAL(pszDomain,"SourceParsers") )
    {
        CSLDestroy( papszSourceParsers );
        papszSourceParsers = CSLDuplicate( papszMetadata );
        return CE_None;
    }
    else
        return GDALDriver::SetMetadata( papszMetadata, pszDomain );
}

/************************************************************************/
/*                          AddSourceParser()                           */
/************************************************************************/

void VRTDriver::AddSourceParser( const char *pszElementName, 
                                 VRTSourceParser pfnParser )

{
    char szPtrValue[128];

    sprintf( szPtrValue, "%p", pfnParser );
    papszSourceParsers = CSLSetNameValue( papszSourceParsers, 
                                          pszElementName, szPtrValue );
}

/************************************************************************/
/*                            ParseSource()                             */
/************************************************************************/

VRTSource *VRTDriver::ParseSource( CPLXMLNode *psSrc, const char *pszVRTPath )

{
    const char *pszParserFunc;

    if( psSrc == NULL || psSrc->eType != CXT_Element )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Corrupt or empty VRT source XML document." );
        return NULL;
    }

    pszParserFunc = CSLFetchNameValue( papszSourceParsers, psSrc->pszValue );
    if( pszParserFunc == NULL )
        return NULL;

    VRTSourceParser pfnParser = NULL;

    sscanf( pszParserFunc, "%p", &pfnParser );

    if( pfnParser == NULL )
        return NULL;
    else
        return pfnParser( psSrc, pszVRTPath );
}

/************************************************************************/
/*                           VRTCreateCopy()                            */
/************************************************************************/

static GDALDataset *
VRTCreateCopy( const char * pszFilename,
               GDALDataset *poSrcDS,
               int bStrict,
               char ** papszOptions,
               CPL_UNUSED GDALProgressFunc pfnProgress,
               CPL_UNUSED void * pProgressData )
{
    VRTDataset *poVRTDS = NULL;

    (void) bStrict;
    (void) papszOptions;

    CPLAssert( NULL != poSrcDS );

/* -------------------------------------------------------------------- */
/*      If the source dataset is a virtual dataset then just write      */
/*      it to disk as a special case to avoid extra layers of           */
/*      indirection.                                                    */
/* -------------------------------------------------------------------- */
    if( poSrcDS->GetDriver() != NULL &&
        EQUAL(poSrcDS->GetDriver()->GetDescription(),"VRT") )
    {

    /* -------------------------------------------------------------------- */
    /*      Convert tree to a single block of XML text.                     */
    /* -------------------------------------------------------------------- */
        char *pszVRTPath = CPLStrdup(CPLGetPath(pszFilename));
        CPLXMLNode *psDSTree = ((VRTDataset *) poSrcDS)->SerializeToXML( pszVRTPath );

        char *pszXML = CPLSerializeXMLTree( psDSTree );
        
        CPLDestroyXMLNode( psDSTree );

        CPLFree( pszVRTPath );
        
    /* -------------------------------------------------------------------- */
    /*      Write to disk.                                                  */
    /* -------------------------------------------------------------------- */
        GDALDataset* pCopyDS = NULL;

        if( 0 != strlen( pszFilename ) )
        {
            VSILFILE *fpVRT = NULL;

            fpVRT = VSIFOpenL( pszFilename, "wb" );
            if (fpVRT == NULL)
            {
                CPLError(CE_Failure, CPLE_AppDefined,
                         "Cannot create %s", pszFilename);
                CPLFree( pszXML );
                return NULL;
            }

            VSIFWriteL( pszXML, 1, strlen(pszXML), fpVRT );
            VSIFCloseL( fpVRT );

            pCopyDS = (GDALDataset *) GDALOpen( pszFilename, GA_Update );
        }
        else
        {
            /* No destination file is given, so pass serialized XML directly. */

            pCopyDS = (GDALDataset *) GDALOpen( pszXML, GA_Update );
        }

        CPLFree( pszXML );

        return pCopyDS;
    }

/* -------------------------------------------------------------------- */
/*      Create the virtual dataset.                                     */
/* -------------------------------------------------------------------- */
    poVRTDS = (VRTDataset *) 
        VRTDataset::Create( pszFilename, 
                            poSrcDS->GetRasterXSize(),
                            poSrcDS->GetRasterYSize(),
                            0, GDT_Byte, NULL );
    if (poVRTDS == NULL)
        return NULL;

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
/*      Copy any special domains that should be transportable.          */
/* -------------------------------------------------------------------- */
    char **papszMD;

    papszMD = poSrcDS->GetMetadata( "RPC" );
    if( papszMD )
        poVRTDS->SetMetadata( papszMD, "RPC" );

    papszMD = poSrcDS->GetMetadata( "IMD" );
    if( papszMD )
        poVRTDS->SetMetadata( papszMD, "IMD" );

    papszMD = poSrcDS->GetMetadata( "GEOLOCATION" );
    if( papszMD )
        poVRTDS->SetMetadata( papszMD, "GEOLOCATION" );

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

        VRTSourcedRasterBand *poVRTBand = 
            (VRTSourcedRasterBand *) poVRTDS->GetRasterBand( iBand+1 );

/* -------------------------------------------------------------------- */
/*      Setup source mapping.                                           */
/* -------------------------------------------------------------------- */
        poVRTBand->AddSimpleSource( poSrcBand );

/* -------------------------------------------------------------------- */
/*      Emit various band level metadata.                               */
/* -------------------------------------------------------------------- */
        poVRTBand->CopyCommonInfoFrom( poSrcBand );

/* -------------------------------------------------------------------- */
/*      Add specific mask band.                                         */
/* -------------------------------------------------------------------- */
        if ( (poSrcBand->GetMaskFlags() & (GMF_PER_DATASET | GMF_ALL_VALID | GMF_NODATA)) == 0)
        {
            VRTSourcedRasterBand* poVRTMaskBand = new VRTSourcedRasterBand(poVRTDS, 0,
                poSrcBand->GetMaskBand()->GetRasterDataType(),
                poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize());
            poVRTMaskBand->AddMaskBandSource( poSrcBand );
            poVRTBand->SetMaskBand( poVRTMaskBand );
        }
    }

/* -------------------------------------------------------------------- */
/*      Add dataset mask band                                           */
/* -------------------------------------------------------------------- */
    if (poSrcDS->GetRasterCount() != 0 &&
        poSrcDS->GetRasterBand(1) != NULL &&
        poSrcDS->GetRasterBand(1)->GetMaskFlags() == GMF_PER_DATASET)
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand(1);
        VRTSourcedRasterBand* poVRTMaskBand = new VRTSourcedRasterBand(poVRTDS, 0,
            poSrcBand->GetMaskBand()->GetRasterDataType(),
            poSrcDS->GetRasterXSize(), poSrcDS->GetRasterYSize());
        poVRTMaskBand->AddMaskBandSource( poSrcBand );
        poVRTDS->SetMaskBand( poVRTMaskBand );
    }

    poVRTDS->FlushCache();

    return poVRTDS;
}

/************************************************************************/
/*                          GDALRegister_VRT()                          */
/************************************************************************/

void GDALRegister_VRT()

{
    VRTDriver	*poDriver;

    if( GDALGetDriverByName( "VRT" ) == NULL )
    {
        poDriver = new VRTDriver();
        
        poDriver->SetDescription( "VRT" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Virtual Raster" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "vrt" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "gdal_vrttut.html" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
                                   "Byte Int16 UInt16 Int32 UInt32 Float32 Float64 CInt16 CInt32 CFloat32 CFloat64" );
        
        poDriver->pfnOpen = VRTDataset::Open;
        poDriver->pfnCreateCopy = VRTCreateCopy;
        poDriver->pfnCreate = VRTDataset::Create;
        poDriver->pfnIdentify = VRTDataset::Identify;
        poDriver->pfnDelete = VRTDataset::Delete;

        poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='ROOT_PATH' type='string' description='Root path to evaluate relative paths inside the VRT. Mainly useful for inlined VRT, or in-memory VRT, where their own directory does not make sense'/>"
"</OpenOptionList>" );

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->AddSourceParser( "SimpleSource", 
                                   VRTParseCoreSources );
        poDriver->AddSourceParser( "ComplexSource", 
                                   VRTParseCoreSources );
        poDriver->AddSourceParser( "AveragedSource", 
                                   VRTParseCoreSources );

        poDriver->AddSourceParser( "KernelFilteredSource", 
                                   VRTParseFilterSources );

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
