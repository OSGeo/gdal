/******************************************************************************
 * $Id$
 *
 * Project:     TerraSAR-X XML Product Support
 * Purpose:     Support for TerraSAR-X XML Metadata files
 * Author:      Philippe Vachon <philippe@cowpig.ca>
 * Description: This driver adds support for reading metadata and georef data
 *              associated with TerraSAR-X products.
 *
 ******************************************************************************
 * Copyright (c) 2007, Philippe Vachon <philippe@cowpig.ca>
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
void GDALRegister_TSX(void);
CPL_C_END


enum ePolarization {
	HH=0,
	HV,
	VH,
	VV
};

enum eProductType {
    eSSC = 0,
    eMGD,
    eEEC,
    eGEC,
    eUnknown
};

/************************************************************************/
/* Helper Functions                                                     */
/************************************************************************/

/* GetFilePath: return a relative path to a file within an XML node.
 * Returns Null on failure
 */
const char *GetFilePath(CPLXMLNode *psXMLNode, char **pszNodeType) {
	const char *pszDirectory, *pszFilename;

	pszDirectory = CPLGetXMLValue( psXMLNode, "file.location.path", "" );
	pszFilename = CPLGetXMLValue( psXMLNode, "file.location.filename", "" );
	*pszNodeType = strdup(CPLGetXMLValue (psXMLNode, "type", " " ));

	if (pszDirectory == NULL || pszFilename == NULL) {
		return NULL;
	}

	return strdup( CPLFormFilename( pszDirectory, pszFilename, "" ) );
}

/************************************************************************/
/* ==================================================================== */
/*	                            TSXDataset	                 			*/
/* ==================================================================== */
/************************************************************************/

class TSXDataset : public GDALPamDataset {
    int nGCPCount;
    GDAL_GCP *pasGCPList;

    char *pszGCPProjection;

	char *pszGeorefFile;
	FILE *fp;

    eProductType nProduct;
public:
	TSXDataset();
	~TSXDataset();
    
    virtual int GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    static GDALDataset *Open( GDALOpenInfo *poOpenInfo );
	static int Identify( GDALOpenInfo *poOpenInfo );
};

/************************************************************************/
/* ==================================================================== */
/*			                    TSXRasterBand                           */
/* ==================================================================== */
/************************************************************************/

class TSXRasterBand : public GDALPamRasterBand {
	GDALDataset *poBand;
	ePolarization ePol;
public:
	TSXRasterBand( TSXDataset *poDSIn, GDALDataType eDataType, 
		ePolarization ePol, GDALDataset *poBand );
	virtual ~TSXRasterBand();

	virtual CPLErr IReadBlock( int nBlockXOff, int nBlockYOff, void *pImage );

	static GDALDataset *Open( GDALOpenInfo *poOpenInfo );
};

/************************************************************************/
/*                            TSXRasterBand                             */
/************************************************************************/

TSXRasterBand::TSXRasterBand( TSXDataset *poDS, GDALDataType eDataType,
                              ePolarization ePol, GDALDataset *poBand )
{
    this->poDS = poDS;
    this->eDataType = eDataType;
    this->ePol = ePol;

	switch (ePol) {
		case HH:
	        SetMetadataItem( "POLARIMETRIC_INTERP", "HH" );
			break;
		case HV:
	        SetMetadataItem( "POLARIMETRIC_INTERP", "HV" );
			break;
		case VH:
	        SetMetadataItem( "POLARIMETRIC_INTERP", "VH" );
			break;
		case VV:
	        SetMetadataItem( "POLARIMETRIC_INTERP", "VV" );
			break;
	}


	/* now setup the actual raster reader */
	this->poBand = poBand;

	GDALRasterBand *poSrcBand = poBand->GetRasterBand( 1 );
	poSrcBand->GetBlockSize( &nBlockXSize, &nBlockYSize );
}

/************************************************************************/
/*                            TSXRasterBand()                           */
/************************************************************************/

TSXRasterBand::~TSXRasterBand() {
    if( poBand != NULL )
        GDALClose( (GDALRasterBandH) poBand );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr TSXRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )
{
	int nRequestYSize;

	/* Check if the last strip is partial so we can avoid over-requesting */
	if ( (nBlockYOff + 1) * nBlockYSize > nRasterYSize ) {
		nRequestYSize = nRasterYSize - nBlockYOff * nBlockYSize;
		memset( pImage, 0, (GDALGetDataTypeSize( eDataType ) / 8) *
			nBlockXSize * nBlockYSize);
	}
	else {
		nRequestYSize = nBlockYSize;
	}

	/* Read Complex Data */
	if ( eDataType == GDT_CInt16 ) {
		return poBand->RasterIO( GF_Read, nBlockXOff * nBlockXSize,
			nBlockYOff * nBlockYSize, nBlockXSize, nRequestYSize,
			pImage, nBlockXSize, nRequestYSize, GDT_CInt16, 1, NULL, 4,
			nBlockXSize * 4, 0 );
	}
	else { /* Detected Product */
		return poBand->RasterIO( GF_Read, nBlockXOff * nBlockXSize,
			nBlockYOff * nBlockYSize, nBlockXSize, nRequestYSize,
			pImage, nBlockXSize, nRequestYSize, GDT_UInt16, 1, NULL, 2,
			nBlockXSize * 2, 0 );
	}
}

/************************************************************************/
/* ==================================================================== */
/*			                	TSXDataset				                */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             TSXDataset()                             */
/************************************************************************/

TSXDataset::TSXDataset() {
    nGCPCount = 0;
    pasGCPList = NULL;
    pszGCPProjection = CPLStrdup("");
}

/************************************************************************/
/*                            ~TSXDataset()                             */
/************************************************************************/

TSXDataset::~TSXDataset() {
    FlushCache();
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int TSXDataset::Identify( GDALOpenInfo *poOpenInfo ) {
	if (poOpenInfo->fp == NULL || poOpenInfo->nHeaderBytes < 260) 
		return 0;

	/* Check if the filename contains TSX1_SAR */
	if (!EQUALN(CPLGetBasename( poOpenInfo->pszFilename ), "TSX1_SAR", 8))
		return 0;

	/* finally look for the <level1Product tag */
	if (!EQUALN((char *)poOpenInfo->pabyHeader, "<level1Product", 14)) 
		return 0;

	return 1;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *TSXDataset::Open( GDALOpenInfo *poOpenInfo ) {
/* -------------------------------------------------------------------- */
/*      Is this a TerraSAR-X product file?                              */
/* -------------------------------------------------------------------- */
	if (!TSXDataset::Identify( poOpenInfo )) {
		return NULL; /* nope */
	}
    
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The TSX driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }
    
	/* Ingest the XML */
	CPLXMLNode *psData, *psComponents, *psProductInfo;
	psData = CPLParseXMLFile( poOpenInfo->pszFilename );

	/* find the product components */
	psComponents = CPLGetXMLNode( psData, "=level1Product.productComponents" );
	if (psComponents == NULL) {
		CPLError( CE_Failure, CPLE_OpenFailed, 
			"Unable to find <productComponents> tag in file.\n" );
		return NULL;
	}

	/* find the product info tag */
	psProductInfo = CPLGetXMLNode( psData, "=level1Product.productInfo" );
	if (psComponents == NULL) {
		CPLError( CE_Failure, CPLE_OpenFailed,
			"Unable to find <productInfo> tag in file.\n" );
		return NULL;
	}

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
	
    TSXDataset *poDS = new TSXDataset();
	poDS->fp = poOpenInfo->fp;
	poOpenInfo->fp = NULL;

/* -------------------------------------------------------------------- */
/*      Read in product info.                                           */
/* -------------------------------------------------------------------- */

    poDS->SetMetadataItem( "SCENE_CENTRE_TIME", CPLGetXMLValue( psProductInfo,
        "sceneInfo.sceneCenterCoord.azimuthTimeUTC", "unknown" ) );
	poDS->SetMetadataItem( "OPERATIONAL_MODE", CPLGetXMLValue( psProductInfo, 
		"generationInfo.groundOperationsType", "unknown" ) );
	poDS->SetMetadataItem( "ORBIT_CYCLE", CPLGetXMLValue( psProductInfo,
		"missionInfo.orbitCycle", "unknown" ) );
	poDS->SetMetadataItem( "ABSOLUTE_ORBIT", CPLGetXMLValue( psProductInfo,
		"missionInfo.absOrbit", "unknown" ) );
	poDS->SetMetadataItem( "ORBIT_DIRECTION", CPLGetXMLValue( psProductInfo,
		"missionInfo.orbitDirection", "unknown" ) );
	poDS->SetMetadataItem( "IMAGING_MODE", CPLGetXMLValue( psProductInfo,
		"acquisitionInfo.imagingMode", "unknown" ) );
	poDS->SetMetadataItem( "PRODUCT_VARIANT", CPLGetXMLValue( psProductInfo,
		"productVariantInfo.productVariant", "unknown" ) ); 
	char *pszDataType = strdup( CPLGetXMLValue( psProductInfo,
		"imageDataInfo.imageDataType", "unknown" ) );
	poDS->SetMetadataItem( "IMAGE_TYPE", pszDataType ); 
	
	/* Get raster information */
	int nRows = atoi( CPLGetXMLValue( psProductInfo,
		"imageDataInfo.imageRaster.numberOfRows", "" ) );
	int nCols = atoi( CPLGetXMLValue( psProductInfo,
		"imageDataInfo.imageRaster.numberOfColumns", "" ) );

	poDS->nRasterXSize = nCols;
	poDS->nRasterYSize = nRows;

	poDS->SetMetadataItem( "ROW_SPACING", CPLGetXMLValue( psProductInfo,
		"imageDataInfo.imageRaster.rowSpacing", "unknown" ) );
	poDS->SetMetadataItem( "COL_SPACING", CPLGetXMLValue( psProductInfo,
		"imageDataInfo.imageRaster.columnSpacing", "unknown" ) );
    poDS->SetMetadataItem( "COL_SPACING_UNITS", CPLGetXMLValue( psProductInfo,
        "imageDataInfo.imageRaster.columnSpacing.units", "unknown" ) );

	/* Get equivalent number of looks */
	poDS->SetMetadataItem( "AZIMUTH_LOOKS", CPLGetXMLValue( psProductInfo,
		"imageDataInfo.imageRaster.azimuthLooks", "unknown" ) );
	poDS->SetMetadataItem( "RANGE_LOOKS", CPLGetXMLValue( psProductInfo,
		"imageDataInfo.imageRaster.rangeLooks", "unknown" ) );

    const char *pszProductVariant;
    pszProductVariant = CPLGetXMLValue( psProductInfo, 
        "productVariantInfo.productVariant", "unknown" );

    poDS->SetMetadataItem( "PRODUCT_VARIANT", pszProductVariant );

    /* Determine what product variant this is */
    if (EQUALN(pszProductVariant,"SSC",3))
        poDS->nProduct = eSSC;
    else if (EQUALN(pszProductVariant,"MGD",3))
        poDS->nProduct = eMGD;
    else if (EQUALN(pszProductVariant,"EEC",3))
        poDS->nProduct = eEEC;
    else if (EQUALN(pszProductVariant,"GEC",3))
        poDS->nProduct = eGEC;
    else
        poDS->nProduct = eUnknown;

	/* Start reading in the product components */
	const char *pszPath;
	char *pszGeorefFile = NULL;
	CPLXMLNode *psComponent;
	for (psComponent = psComponents->psChild; psComponent != NULL;
		 psComponent = psComponent->psNext)
	{
		char *pszType;
		pszPath = CPLFormFilename( 
				CPLGetDirname( poOpenInfo->pszFilename ),
				GetFilePath(psComponent, &pszType), 
				"" );
		const char *pszPolLayer = CPLGetXMLValue(psComponent, "polLayer", " ");

		if ( !EQUALN(pszType," ",1) ) {
			if (EQUALN(pszType, "MAPPING_GRID", 12) ) {
				/* the mapping grid... save as a metadata item this path */
				poDS->SetMetadataItem( "MAPPING_GRID", pszPath );	
			}
			else if (EQUALN(pszType, "GEOREF", 6)) {
				/* save the path to the georef data for later use */
				pszGeorefFile = strdup( pszPath );
			}
			CPLFree(pszType);
		}
		else if( !EQUALN(pszPolLayer, " ", 1) && 
			EQUALN(psComponent->pszValue, "imageData", 9) ) {
			/* determine the polarization of this band */
			ePolarization ePol;
			if ( EQUALN(pszPolLayer, "HH", 2) ) {
				ePol = HH;
			}
			else if ( EQUALN(pszPolLayer, "HV" , 2) ) {
				ePol = HV;
			}
			else if ( EQUALN(pszPolLayer, "VH", 2) ) {
				ePol = VH;
			}
			else {
				ePol = VV;
			}

			GDALDataType eDataType = EQUALN(pszDataType, "COMPLEX", 7) ?
				GDT_CInt16 : GDT_UInt16;

			/* try opening the file that represents that band */
			TSXRasterBand *poBand;
			GDALDataset *poBandData;

			poBandData = (GDALDataset *) GDALOpen( pszPath, GA_ReadOnly );
			if ( poBandData != NULL ) {
				poBand = new TSXRasterBand( poDS, eDataType, ePol, 
					poBandData );
				poDS->SetBand( poDS->GetRasterCount() + 1, poBand );
			}
		}
	}

	CPLFree(pszDataType);


/* -------------------------------------------------------------------- */
/*      Check and set matrix representation.                            */
/* -------------------------------------------------------------------- */

	if (poDS->GetRasterCount() == 4) {
		poDS->SetMetadataItem( "MATRIX_REPRESENTATION", "SCATTERING" );
	}

/* -------------------------------------------------------------------- */
/*      Read the four corners and centre GCPs in                        */
/* -------------------------------------------------------------------- */

    CPLXMLNode *psSceneInfo = CPLGetXMLNode( psData, 
        "=level1Product.productInfo.sceneInfo" );
    /* for SSC products */
    if (poDS->nProduct == eSSC && psSceneInfo != NULL) {
        CPLXMLNode *psNode;
        int nGCP = 0;
        double dfAvgHeight = atof(CPLGetXMLValue(psSceneInfo, 
            "sceneAverageHeight", "0.0"));
        char szID[3];

        poDS->nGCPCount = 5; /* 5 GCPs provided */
        poDS->pasGCPList = (GDAL_GCP *)CPLCalloc(sizeof(GDAL_GCP), 
            poDS->nGCPCount);

        /* iterate over GCPs */
        for (psNode = psSceneInfo->psChild; psNode != NULL; 
             psNode = psNode->psNext )
        {
            GDAL_GCP *psGCP = poDS->pasGCPList + nGCP;

            if (!EQUAL(psNode->pszValue, "sceneCenterCoord") && 
                !EQUAL(psNode->pszValue, "sceneCornerCoord"))
                continue;

            CPLSPrintf( szID, "%d", nGCP );
            
            psGCP->dfGCPPixel = atof(CPLGetXMLValue(psNode, "refColumn", 
                "0.0"));
            psGCP->dfGCPLine = atof(CPLGetXMLValue(psNode, "refRow", "0.0"));
            psGCP->dfGCPX = atof(CPLGetXMLValue(psNode, "lon", "0.0"));
            psGCP->dfGCPY = atof(CPLGetXMLValue(psNode, "lat", "0.0"));
            psGCP->dfGCPZ = dfAvgHeight;
            psGCP->pszId = CPLStrdup( szID );
            psGCP->pszInfo = CPLStrdup("");

            nGCP++;
        }
    }
    else if (psSceneInfo != NULL) {
        /* extract the GCPs from the provided file */

        /* TODO */
    }
    else {
        CPLError(CE_Warning, CPLE_AppDefined, 
            "Unable to find sceneInfo tag in XML document. " 
            "Proceeding with caution.");
    }

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Check for overviews.                                            */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    CPLDestroyXMLNode(psData);

    return poDS;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int TSXDataset::GetGCPCount() {
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *TSXDataset::GetGCPProjection() {
    return pszGCPProjection;
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *TSXDataset::GetGCPs() {
    return pasGCPList;
}

/************************************************************************/
/*                         GDALRegister_TSX()                          */
/************************************************************************/

void GDALRegister_TSX() {
    GDALDriver	*poDriver;

    if( GDALGetDriverByName( "TSX" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "TSX" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "TerraSAR-X Product" );
/*        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_tsx.html" ); */

        poDriver->pfnOpen = TSXDataset::Open;
		poDriver->pfnIdentify = TSXDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

