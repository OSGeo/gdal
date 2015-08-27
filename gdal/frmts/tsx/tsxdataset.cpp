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
 * Copyright (c) 2009-2012, Even Rouault <even dot rouault at mines-paris dot org>
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
#include "ogr_spatialref.h"

#define MAX_GCPS 5000    //this should be more than enough ground control points

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
const char *GetFilePath(CPLXMLNode *psXMLNode, const char **pszNodeType) {
    const char *pszDirectory, *pszFilename;

    pszDirectory = CPLGetXMLValue( psXMLNode, "file.location.path", "" );
    pszFilename = CPLGetXMLValue( psXMLNode, "file.location.filename", "" );
    *pszNodeType = CPLGetXMLValue (psXMLNode, "type", " " );

    if (pszDirectory == NULL || pszFilename == NULL) {
        return NULL;
    }

    return CPLFormFilename( pszDirectory, pszFilename, "" );
}

/************************************************************************/
/* ==================================================================== */
/*                                TSXDataset                                 */
/* ==================================================================== */
/************************************************************************/

class TSXDataset : public GDALPamDataset {
    int nGCPCount;
    GDAL_GCP *pasGCPList;

    char *pszGCPProjection;

    char *pszProjection;
    double adfGeoTransform[6];
    bool bHaveGeoTransform;

    eProductType nProduct;
public:
    TSXDataset();
    ~TSXDataset();

    virtual int GetGCPCount();
    virtual const char *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs();

    CPLErr GetGeoTransform( double* padfTransform);
    const char* GetProjectionRef();

    static GDALDataset *Open( GDALOpenInfo *poOpenInfo );
    static int Identify( GDALOpenInfo *poOpenInfo );
private:
    bool getGCPsFromGEOREF_XML(char *pszGeorefFilename);
};

/************************************************************************/
/* ==================================================================== */
/*                                TSXRasterBand                           */
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
            nBlockXSize * 4, 0, NULL );
    }
    else { /* Detected Product */
        return poBand->RasterIO( GF_Read, nBlockXOff * nBlockXSize,
            nBlockYOff * nBlockYSize, nBlockXSize, nRequestYSize,
            pImage, nBlockXSize, nRequestYSize, GDT_UInt16, 1, NULL, 2,
            nBlockXSize * 2, 0, NULL );
    }
}

/************************************************************************/
/* ==================================================================== */
/*                                TSXDataset                                */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             TSXDataset()                             */
/************************************************************************/

TSXDataset::TSXDataset() :
    nGCPCount(0), pasGCPList(NULL), pszGCPProjection(NULL), pszProjection(NULL),
    bHaveGeoTransform(false), nProduct(eUnknown)
{
    pszGCPProjection = CPLStrdup("");
    pszProjection = CPLStrdup("");
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~TSXDataset()                             */
/************************************************************************/

TSXDataset::~TSXDataset() {
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
/*                              Identify()                              */
/************************************************************************/

int TSXDataset::Identify( GDALOpenInfo *poOpenInfo )
{
    if (poOpenInfo->fpL == NULL || poOpenInfo->nHeaderBytes < 260)
    {
        if( poOpenInfo->bIsDirectory )
        {
            CPLString osFilename =
                CPLFormCIFilename( poOpenInfo->pszFilename, CPLGetFilename( poOpenInfo->pszFilename ), "xml" );

            /* Check if the filename contains TSX1_SAR (TerraSAR-X) or TDX1_SAR (TanDEM-X) */
            if (!(EQUALN(CPLGetBasename( osFilename ), "TSX1_SAR", 8) ||
                  EQUALN(CPLGetBasename( osFilename ), "TDX1_SAR", 8)))
                return 0;

            VSIStatBufL sStat;
            if( VSIStatL( osFilename, &sStat ) == 0 )
                return 1;
        }

        return 0;
    }

    /* Check if the filename contains TSX1_SAR (TerraSAR-X) or TDX1_SAR (TanDEM-X) */
    if (!(EQUALN(CPLGetBasename( poOpenInfo->pszFilename ), "TSX1_SAR", 8) ||
          EQUALN(CPLGetBasename( poOpenInfo->pszFilename ), "TDX1_SAR", 8)))
        return 0;

    /* finally look for the <level1Product tag */
    if (!EQUALN((char *)poOpenInfo->pabyHeader, "<level1Product", 14))
        return 0;

    return 1;
}

/************************************************************************/
/*                                getGCPsFromGEOREF_XML()               */
/*Reads georeferencing information from the TerraSAR-X GEOREF.XML file    */
/*and writes the information to the dataset's gcp list and projection     */
/*string.                                                                */
/*Returns true on success.                                                */
/************************************************************************/
bool TSXDataset::getGCPsFromGEOREF_XML(char *pszGeorefFilename)
{
    //open GEOREF.xml
    CPLXMLNode *psGeorefData;
    psGeorefData = CPLParseXMLFile( pszGeorefFilename );
    if (psGeorefData==NULL)
        return false;

    //get the ellipsoid and semi-major, semi-minor axes
    CPLXMLNode *psSphere;
    const char *pszEllipsoidName;
    double minor_axis, major_axis, inv_flattening;
    OGRSpatialReference osr;
    psSphere = CPLGetXMLNode( psGeorefData, "=geoReference.referenceFrames.sphere" );
    if (psSphere!=NULL)
    {
        pszEllipsoidName = CPLGetXMLValue( psSphere, "ellipsoidID", "" );
        minor_axis = CPLAtof(CPLGetXMLValue( psSphere, "semiMinorAxis", "0.0" ));
        major_axis = CPLAtof(CPLGetXMLValue( psSphere, "semiMajorAxis", "0.0" ));
        //save datum parameters to the spatial reference
        if ( EQUAL(pszEllipsoidName, "") || minor_axis==0.0 || major_axis==0.0 )
        {
            CPLError(CE_Warning,CPLE_AppDefined,"Warning- incomplete"
                " ellipsoid information.  Using wgs-84 parameters.\n");
            osr.SetWellKnownGeogCS( "WGS84" );
        }
        else if ( EQUAL( pszEllipsoidName, "WGS84" ) ) {
            osr.SetWellKnownGeogCS( "WGS84" );
        }
        else {
            inv_flattening = major_axis/(major_axis - minor_axis);
            osr.SetGeogCS( "","",pszEllipsoidName, major_axis, inv_flattening);
        }
    }

    //get gcps
    CPLXMLNode *psNode;
    CPLXMLNode *psGeolocationGrid = CPLGetXMLNode( psGeorefData, "=geoReference.geolocationGrid" );
    if (psGeolocationGrid==NULL)
    {
        CPLDestroyXMLNode( psGeorefData );
        return false;
    }
    nGCPCount = atoi(CPLGetXMLValue( psGeolocationGrid, "numberOfGridPoints.total", "0" ));
    //count the gcps if the given count value is invalid
    if (nGCPCount<=0)
    {
        for( psNode = psGeolocationGrid->psChild; psNode != NULL; psNode = psNode->psNext )
            if( EQUAL(psNode->pszValue,"gridPoint") )
                nGCPCount++ ;
    }
    //if there are no gcps, fail
    if(nGCPCount<=0)
    {
        CPLDestroyXMLNode( psGeorefData );
        return false;
    }

    //put some reasonable limits of the number of gcps
    if (nGCPCount>MAX_GCPS )
        nGCPCount=MAX_GCPS;
    //allocate memory for the gcps
    pasGCPList = (GDAL_GCP *)CPLCalloc(sizeof(GDAL_GCP),nGCPCount);
    //loop through all gcps and set info
    int gcps_allocated = nGCPCount;    //save the number allocated to ensure it does not run off the end of the array
    nGCPCount=0;    //reset to zero and count
    //do a check on the grid point to make sure it has lat,long row, and column
    //it seems that only SSC products contain row, col - how to map lat long otherwise??
    //for now fail if row and col are not present - just check the first and assume the rest are the same
    for( psNode = psGeolocationGrid->psChild; psNode != NULL; psNode = psNode->psNext )
    {
         if( !EQUAL(psNode->pszValue,"gridPoint") )
             continue;

         if (    !strcmp(CPLGetXMLValue(psNode,"col","error"), "error") ||
                 !strcmp(CPLGetXMLValue(psNode,"row","error"), "error") ||
                 !strcmp(CPLGetXMLValue(psNode,"lon","error"), "error") ||
                 !strcmp(CPLGetXMLValue(psNode,"lat","error"), "error"))
        {
            CPLDestroyXMLNode( psGeorefData );
            return false;
        }
    }
    for( psNode = psGeolocationGrid->psChild; psNode != NULL; psNode = psNode->psNext )
    {
        //break out if the end of the array has been reached
        if (nGCPCount >= gcps_allocated)
        {
            CPLError(CE_Warning, CPLE_AppDefined, "GDAL TSX driver: Truncating the number of GCPs.");
            break;
        }

         char    szID[32];
         GDAL_GCP   *psGCP = pasGCPList + nGCPCount;

         if( !EQUAL(psNode->pszValue,"gridPoint") )
             continue;

         nGCPCount++ ;

         sprintf( szID, "%d", nGCPCount );
         psGCP->pszId = CPLStrdup( szID );
         psGCP->pszInfo = CPLStrdup("");
         psGCP->dfGCPPixel =
             CPLAtof(CPLGetXMLValue(psNode,"col","0"));
         psGCP->dfGCPLine =
             CPLAtof(CPLGetXMLValue(psNode,"row","0"));
         psGCP->dfGCPX =
             CPLAtof(CPLGetXMLValue(psNode,"lon",""));
         psGCP->dfGCPY =
             CPLAtof(CPLGetXMLValue(psNode,"lat",""));
         //looks like height is in meters - should it be converted so xyz are all on the same scale??
         psGCP->dfGCPZ = 0;
             //CPLAtof(CPLGetXMLValue(psNode,"height",""));
    }

    CPLFree(pszGCPProjection);
    osr.exportToWkt( &(pszGCPProjection) );

    CPLDestroyXMLNode( psGeorefData );

    return true;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *TSXDataset::Open( GDALOpenInfo *poOpenInfo ) {
/* -------------------------------------------------------------------- */
/*      Is this a TerraSAR-X product file?                              */
/* -------------------------------------------------------------------- */
    if (!TSXDataset::Identify( poOpenInfo ))
    {
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

    CPLString osFilename;

    if( poOpenInfo->bIsDirectory )
    {
        osFilename =
                CPLFormCIFilename( poOpenInfo->pszFilename, CPLGetFilename( poOpenInfo->pszFilename ), "xml" );
    }
    else
        osFilename = poOpenInfo->pszFilename;

    /* Ingest the XML */
    CPLXMLNode *psData, *psComponents, *psProductInfo;
    psData = CPLParseXMLFile( osFilename );
    if (psData == NULL)
        return NULL;

    /* find the product components */
    psComponents = CPLGetXMLNode( psData, "=level1Product.productComponents" );
    if (psComponents == NULL) {
        CPLError( CE_Failure, CPLE_OpenFailed,
            "Unable to find <productComponents> tag in file.\n" );
        CPLDestroyXMLNode(psData);
        return NULL;
    }

    /* find the product info tag */
    psProductInfo = CPLGetXMLNode( psData, "=level1Product.productInfo" );
    if (psProductInfo == NULL) {
        CPLError( CE_Failure, CPLE_OpenFailed,
            "Unable to find <productInfo> tag in file.\n" );
        CPLDestroyXMLNode(psData);
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */

    TSXDataset *poDS = new TSXDataset();

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
    char *pszDataType = CPLStrdup( CPLGetXMLValue( psProductInfo,
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
    CPLErr geoTransformErr=CE_Failure;
    for (psComponent = psComponents->psChild; psComponent != NULL;
         psComponent = psComponent->psNext)
    {
        const char *pszType = NULL;
        pszPath = CPLFormFilename(
                CPLGetDirname( osFilename ),
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
                pszGeorefFile = CPLStrdup( pszPath );
            }
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

                //copy georeferencing info from the band
                //need error checking??
                //it will just save the info from the last band
                CPLFree( poDS->pszProjection );
                poDS->pszProjection = CPLStrdup(poBandData->GetProjectionRef());
                geoTransformErr = poBandData->GetGeoTransform(poDS->adfGeoTransform);
            }
        }
    }

    //now check if there is a geotransform
    if ( strcmp(poDS->pszProjection, "") && geoTransformErr==CE_None)
    {
        poDS->bHaveGeoTransform = TRUE;
    }
    else
    {
        poDS->bHaveGeoTransform = FALSE;
        CPLFree( poDS->pszProjection );
        poDS->pszProjection = CPLStrdup("");
        poDS->adfGeoTransform[0] = 0.0;
        poDS->adfGeoTransform[1] = 1.0;
        poDS->adfGeoTransform[2] = 0.0;
        poDS->adfGeoTransform[3] = 0.0;
        poDS->adfGeoTransform[4] = 0.0;
        poDS->adfGeoTransform[5] = 1.0;
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
    if (psSceneInfo != NULL)
    {
        /* extract the GCPs from the provided file */
        bool success = false;
        if (pszGeorefFile != NULL)
            success = poDS->getGCPsFromGEOREF_XML(pszGeorefFile);

        //if the gcp's cannot be extracted from the georef file, try to get the corner coordinates
        //for now just SSC because the others don't have refColumn and refRow
        if (!success && poDS->nProduct == eSSC)
        {
            CPLXMLNode *psNode;
            int nGCP = 0;
            double dfAvgHeight = CPLAtof(CPLGetXMLValue(psSceneInfo,
                "sceneAverageHeight", "0.0"));

            //count and allocate gcps - there should be five - 4 corners and a centre
            poDS->nGCPCount = 0;
            for (psNode = psSceneInfo->psChild; psNode != NULL; psNode = psNode->psNext )
            {
                if (!EQUAL(psNode->pszValue, "sceneCenterCoord") &&
                    !EQUAL(psNode->pszValue, "sceneCornerCoord"))
                    continue;

                poDS->nGCPCount++;
            }
            if (poDS->nGCPCount > 0)
            {
                poDS->pasGCPList = (GDAL_GCP *)CPLCalloc(sizeof(GDAL_GCP), poDS->nGCPCount);

                /* iterate over GCPs */
                for (psNode = psSceneInfo->psChild; psNode != NULL; psNode = psNode->psNext )
                {
                    GDAL_GCP *psGCP = poDS->pasGCPList + nGCP;

                    if (!EQUAL(psNode->pszValue, "sceneCenterCoord") &&
                        !EQUAL(psNode->pszValue, "sceneCornerCoord"))
                        continue;

                    psGCP->dfGCPPixel = CPLAtof(CPLGetXMLValue(psNode, "refColumn",
                        "0.0"));
                    psGCP->dfGCPLine = CPLAtof(CPLGetXMLValue(psNode, "refRow", "0.0"));
                    psGCP->dfGCPX = CPLAtof(CPLGetXMLValue(psNode, "lon", "0.0"));
                    psGCP->dfGCPY = CPLAtof(CPLGetXMLValue(psNode, "lat", "0.0"));
                    psGCP->dfGCPZ = dfAvgHeight;
                    psGCP->pszId = CPLStrdup( CPLSPrintf( "%d", nGCP ) );
                    psGCP->pszInfo = CPLStrdup("");

                    nGCP++;
                }

                //set the projection string - the fields are lat/long - seems to be WGS84 datum
                OGRSpatialReference osr;
                osr.SetWellKnownGeogCS( "WGS84" );
                CPLFree(poDS->pszGCPProjection);
                osr.exportToWkt( &(poDS->pszGCPProjection) );
            }
        }

        //gcps override geotransform - does it make sense to have both??
        if (poDS->nGCPCount>0)
        {
            poDS->bHaveGeoTransform = FALSE;
            CPLFree( poDS->pszProjection );
            poDS->pszProjection = CPLStrdup("");
            poDS->adfGeoTransform[0] = 0.0;
            poDS->adfGeoTransform[1] = 1.0;
            poDS->adfGeoTransform[2] = 0.0;
            poDS->adfGeoTransform[3] = 0.0;
            poDS->adfGeoTransform[4] = 0.0;
            poDS->adfGeoTransform[5] = 1.0;
        }

    }
    else {
        CPLError(CE_Warning, CPLE_AppDefined,
            "Unable to find sceneInfo tag in XML document. "
            "Proceeding with caution.");
    }

    CPLFree(pszGeorefFile);

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
/*                          GetProjectionRef()                          */
/************************************************************************/
const char *TSXDataset::GetProjectionRef()
{
    return pszProjection;
}

/************************************************************************/
/*                               GetGeotransform()                      */
/************************************************************************/
CPLErr TSXDataset::GetGeoTransform(double* padfTransform)
{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );

    if (bHaveGeoTransform)
        return( CE_None );

    return( CE_Failure );
}

/************************************************************************/
/*                         GDALRegister_TSX()                           */
/************************************************************************/

void GDALRegister_TSX() {
    GDALDriver    *poDriver;

    if( GDALGetDriverByName( "TSX" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "TSX" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "TerraSAR-X Product" );
/*        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_tsx.html" ); */

        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

        poDriver->pfnOpen = TSXDataset::Open;
        poDriver->pfnIdentify = TSXDataset::Identify;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
