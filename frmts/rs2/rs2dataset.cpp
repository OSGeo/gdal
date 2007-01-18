/******************************************************************************
 * $Id$
 *
 * Project:  Polarimetric Workstation
 * Purpose:  Radarsat 2 - XML Products (product.xml) driver
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam <warmerdam@pobox.com>
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
#include "cpl_minixml.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_RS2(void);
CPL_C_END

/************************************************************************/
/* ==================================================================== */
/*				RS2Dataset				*/
/* ==================================================================== */
/************************************************************************/

class RS2Dataset : public GDALPamDataset
{
    CPLXMLNode *psProduct;

    int           nGCPCount;
    GDAL_GCP      *pasGCPList;
    char          *pszGCPProjection;

  public:
    		RS2Dataset();
    	        ~RS2Dataset();
    
    virtual int    GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    static GDALDataset *Open( GDALOpenInfo * );

    CPLXMLNode *GetProduct() { return psProduct; }
};

/************************************************************************/
/* ==================================================================== */
/*			        RS2RasterBand                           */
/* ==================================================================== */
/************************************************************************/

class RS2RasterBand : public GDALPamRasterBand
{
    GDALDataset     *poBandFile;

  public:
    		RS2RasterBand( RS2Dataset *poDSIn, 
                               GDALDataType eDataTypeIn,
                               const char *pszPole, 
                               GDALDataset *poBandFile );
    virtual     ~RS2RasterBand();
    
    virtual CPLErr IReadBlock( int, int, void * );

    static GDALDataset *Open( GDALOpenInfo * );
};

/************************************************************************/
/*                            RS2RasterBand                             */
/************************************************************************/

RS2RasterBand::RS2RasterBand( RS2Dataset *poDSIn,
                              GDALDataType eDataTypeIn,
                              const char *pszPole, 
                              GDALDataset *poBandFileIn )

{
    GDALRasterBand *poSrcBand;

    poDS = poDSIn;
    poBandFile = poBandFileIn;

    poSrcBand = poBandFile->GetRasterBand( 1 );

    poSrcBand->GetBlockSize( &nBlockXSize, &nBlockYSize );
    
    eDataType = eDataTypeIn;

    if( *pszPole != '\0' )
        SetMetadataItem( "POLARIMETRIC_INTERP", pszPole );
}

/************************************************************************/
/*                            RSRasterBand()                            */
/************************************************************************/

RS2RasterBand::~RS2RasterBand()

{
    if( poBandFile != NULL )
        GDALClose( (GDALRasterBandH) poBandFile );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr RS2RasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )

{
    int nRequestYSize;

/* -------------------------------------------------------------------- */
/*      If the last strip is partial, we need to avoid                  */
/*      over-requesting.  We also need to initialize the extra part     */
/*      of the block to zero.                                           */
/* -------------------------------------------------------------------- */
    if( (nBlockYOff + 1) * nBlockYSize > nRasterYSize )
    {
        nRequestYSize = nRasterYSize - nBlockYOff * nBlockYSize;
        memset( pImage, 0, (GDALGetDataTypeSize( eDataType ) / 8) * nBlockXSize * nBlockYSize );
    }
    else
    {
        nRequestYSize = nBlockYSize;
    }

/* -------------------------------------------------------------------- */
/*      Each complex component is a seperate sample in the TIFF file    */
/*      (old way)                                                       */
/* -------------------------------------------------------------------- */
    if( eDataType == GDT_CInt16 && poBandFile->GetRasterCount() == 2 )
        return 
            poBandFile->RasterIO( GF_Read, 
                                  nBlockXOff * nBlockXSize, 
                                  nBlockYOff * nBlockYSize,
                                  nBlockXSize, nRequestYSize,
                                  pImage, nBlockXSize, nRequestYSize, 
                                  GDT_Int16,
                                  2, NULL, 4, nBlockXSize * 4, 2 );

/* -------------------------------------------------------------------- */
/*      File has one sample marked as sample format void, a 32bits.     */
/* -------------------------------------------------------------------- */
    else if( eDataType == GDT_CInt16 && poBandFile->GetRasterCount() == 1 )
    {
        CPLErr eErr;

        eErr = 
            poBandFile->RasterIO( GF_Read, 
                                  nBlockXOff * nBlockXSize, 
                                  nBlockYOff * nBlockYSize,
                                  nBlockXSize, nRequestYSize, 
                                  pImage, nBlockXSize, nRequestYSize, 
                                  GDT_UInt32,
                                  1, NULL, 4, nBlockXSize * 4, 0 );

#ifdef CPL_LSB
        // First, undo the 32bit swap. 
        GDALSwapWords( pImage, 4, nBlockXSize * nBlockYSize, 4 );

        // Then apply 16 bit swap. 
        GDALSwapWords( pImage, 2, nBlockXSize * nBlockYSize * 2, 2 );
#endif        

        return eErr;
    }

/* -------------------------------------------------------------------- */
/*      The 16bit case is straight forward.  The underlying file        */
/*      looks like a 16bit unsigned data too.                           */
/* -------------------------------------------------------------------- */
    else if( eDataType == GDT_UInt16 )
        return 
            poBandFile->RasterIO( GF_Read, 
                                  nBlockXOff * nBlockXSize, 
                                  nBlockYOff * nBlockYSize,
                                  nBlockXSize, nRequestYSize, 
                                  pImage, nBlockXSize, nRequestYSize,
                                  GDT_UInt16,
                                  1, NULL, 2, nBlockXSize * 2, 0 );
    else
    {
        CPLAssert( FALSE );
        return CE_Failure;
    }
}

/************************************************************************/
/* ==================================================================== */
/*				RS2Dataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             RS2Dataset()                             */
/************************************************************************/

RS2Dataset::RS2Dataset()
{
    psProduct = NULL;

    nGCPCount = 0;
    pasGCPList = NULL;
    pszGCPProjection = CPLStrdup("");

}

/************************************************************************/
/*                            ~RS2Dataset()                             */
/************************************************************************/

RS2Dataset::~RS2Dataset()

{
    FlushCache();

    CPLDestroyXMLNode( psProduct );

    CPLFree( pszGCPProjection );
    if( nGCPCount > 0 )
    {
        GDALDeinitGCPs( nGCPCount, pasGCPList );
        CPLFree( pasGCPList );
    }
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *RS2Dataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Is this a RadardSat 2 Product.xml definition?                   */
/* -------------------------------------------------------------------- */
    if( strlen(poOpenInfo->pszFilename) < 11 
        || !EQUAL(poOpenInfo->pszFilename + strlen(poOpenInfo->pszFilename)-11,
                  "product.xml") )
        return NULL;

    if( poOpenInfo->nHeaderBytes < 100 )
        return NULL;

    if( strstr((const char *) poOpenInfo->pabyHeader, "/rs2" ) == NULL
        || strstr((const char *) poOpenInfo->pabyHeader, "<product" ) == NULL)
        return NULL;

/* -------------------------------------------------------------------- */
/*      Ingest the Product.xml file.                                    */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psProduct, *psImageAttributes;

    psProduct = CPLParseXMLFile( poOpenInfo->pszFilename );
    if( psProduct == NULL )
        return NULL;

    psImageAttributes = CPLGetXMLNode(psProduct, "=product.imageAttributes" );
    if( psImageAttributes == NULL )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Failed to find <imageAttributes> in document." );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    RS2Dataset *poDS = new RS2Dataset();

    poDS->psProduct = psProduct;

/* -------------------------------------------------------------------- */
/*      Get overall image information.                                  */
/* -------------------------------------------------------------------- */
    poDS->nRasterXSize = 
        atoi(CPLGetXMLValue( psImageAttributes, 
                             "rasterAttributes.numberOfSamplesPerLine", 
                             "-1" ));
    poDS->nRasterYSize = 
        atoi(CPLGetXMLValue( psImageAttributes, 
                             "rasterAttributes.numberofLines", 
                             "-1" ));

/* -------------------------------------------------------------------- */
/*      Get dataType (so we can recognise complex data), and the        */
/*      bitsPerSample.                                                  */
/* -------------------------------------------------------------------- */
    GDALDataType eDataType;

    const char *pszDataType = 
        CPLGetXMLValue( psImageAttributes, "rasterAttributes.dataType", "" );
    int nBitsPerSample = 
        atoi( CPLGetXMLValue( psImageAttributes, 
                              "rasterAttributes.bitsPerSample", "" ) );

    if( nBitsPerSample == 16 && EQUAL(pszDataType,"Complex") )
        eDataType = GDT_CInt16;
    else if( nBitsPerSample == 16 && EQUALN(pszDataType,"Mag",3) )
        eDataType = GDT_UInt16;
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "dataType=%s, bitsPerSample=%d: not a supported configuration.",
                  pszDataType, nBitsPerSample );
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Open each of the data files as a complex band.                  */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psNode;
    char *pszPath = CPLStrdup(CPLGetPath( poOpenInfo->pszFilename ));

    for( psNode = psImageAttributes->psChild;
         psNode != NULL;
         psNode = psNode->psNext )
    {
        RS2RasterBand *poBand;
        const char *pszBasename;

        if( psNode->eType != CXT_Element 
            || !EQUAL(psNode->pszValue,"fullResolutionImageData") )
            continue;

/* -------------------------------------------------------------------- */
/*      Fetch filename.                                                 */
/* -------------------------------------------------------------------- */
        pszBasename = CPLGetXMLValue( psNode, "", "" );
        if( *pszBasename == '\0' )
            continue;

/* -------------------------------------------------------------------- */
/*      Form full filename (path of product.xml + basename).            */
/* -------------------------------------------------------------------- */
        char *pszFullname = 
            CPLStrdup(CPLFormFilename( pszPath, pszBasename, NULL ));

/* -------------------------------------------------------------------- */
/*      Try and open the file.                                          */
/* -------------------------------------------------------------------- */
        GDALDataset *poBandFile;

        poBandFile = (GDALDataset *) GDALOpen( pszFullname, GA_ReadOnly );
        if( poBandFile == NULL )
            continue;
        
/* -------------------------------------------------------------------- */
/*      Create the band.                                                */
/* -------------------------------------------------------------------- */
        poBand = new RS2RasterBand( poDS, eDataType,
                                    CPLGetXMLValue( psNode, "pole", "" ), 
                                    poBandFile ); 

        poDS->SetBand( poDS->GetRasterCount() + 1, poBand );

        CPLFree( pszFullname );
    }

/* -------------------------------------------------------------------- */
/*      Collect GCPs.                                                   */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psGeoGrid = 
        CPLGetXMLNode( psImageAttributes, 
                       "geographicInformation.geolocationGrid" );

    if( psGeoGrid != NULL )
    {
        // count gcps.
        poDS->nGCPCount = 0;
        
        for( psNode = psGeoGrid->psChild; psNode != NULL;
             psNode = psNode->psNext )
        {
            if( EQUAL(psNode->pszValue,"imageTiePoint") )
                poDS->nGCPCount++ ;
        }

        poDS->pasGCPList = (GDAL_GCP *) 
            CPLCalloc(sizeof(GDAL_GCP),poDS->nGCPCount);
        
        poDS->nGCPCount = 0;
        
        for( psNode = psGeoGrid->psChild; psNode != NULL;
             psNode = psNode->psNext )
        {
            char	szID[32];
            GDAL_GCP   *psGCP = poDS->pasGCPList + poDS->nGCPCount;
            
            if( !EQUAL(psNode->pszValue,"imageTiePoint") )
                continue;

            poDS->nGCPCount++ ;

            sprintf( szID, "%d", poDS->nGCPCount );
            psGCP->pszId = CPLStrdup( szID );
            psGCP->pszInfo = CPLStrdup("");
            psGCP->dfGCPPixel = 
                atof(CPLGetXMLValue(psNode,"imageCoordinate.pixel","0"));
            psGCP->dfGCPLine = 
                atof(CPLGetXMLValue(psNode,"imageCoordinate.line","0"));
            psGCP->dfGCPX = 
                atof(CPLGetXMLValue(psNode,"geodeticCoordinate.longitude",""));
            psGCP->dfGCPY = 
                atof(CPLGetXMLValue(psNode,"geodeticCoordinate.latitude",""));
            psGCP->dfGCPZ = 
                atof(CPLGetXMLValue(psNode,"geodeticCoordinate.height",""));
        }
    }

    CPLFree( pszPath );

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

    return( poDS );
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int RS2Dataset::GetGCPCount()

{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *RS2Dataset::GetGCPProjection()

{
    return pszGCPProjection;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *RS2Dataset::GetGCPs()

{
    return pasGCPList;
}

/************************************************************************/
/*                         GDALRegister_RS2()                          */
/************************************************************************/

void GDALRegister_RS2()

{
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "RS2" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "RS2" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "RadarSat 2 XML Product" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_rs2.html" );

        poDriver->pfnOpen = RS2Dataset::Open;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

