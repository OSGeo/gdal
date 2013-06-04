/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Read subdatasets of HDF5 file.
 * Author:   Denis Nadeau <denis.nadeau@gmail.com>
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
 ****************************************************************************/

#define H5_USE_16_API

#include "hdf5.h"

#include "gdal_pam.h"
#include "gdal_priv.h"
#include "cpl_string.h"
#include "hdf5dataset.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

CPL_C_START
void GDALRegister_HDF5Image(void);
CPL_C_END

/* release 1.6.3 or 1.6.4 changed the type of count in some api functions */

#if H5_VERS_MAJOR == 1 && H5_VERS_MINOR <= 6 \
       && (H5_VERS_MINOR < 6 || H5_VERS_RELEASE < 3)
#  define H5OFFSET_TYPE hssize_t
#else
#  define H5OFFSET_TYPE  hsize_t
#endif

class HDF5ImageDataset : public HDF5Dataset
{
    typedef enum
    {
        UNKNOWN_PRODUCT=0,
        CSK_PRODUCT
    } Hdf5ProductType;

    typedef enum
    {
        PROD_UNKNOWN=0,
        PROD_CSK_L0,
        PROD_CSK_L1A,
        PROD_CSK_L1B,
        PROD_CSK_L1C,
        PROD_CSK_L1D
    } HDF5CSKProductEnum;

    friend class HDF5ImageRasterBand;

    char        *pszProjection;
    char        *pszGCPProjection;
    GDAL_GCP    *pasGCPList;
    int         nGCPCount;
    OGRSpatialReference oSRS;

    hsize_t      *dims,*maxdims;
    HDF5GroupObjects *poH5Objects;
    int          ndims,dimensions;
    hid_t        dataset_id;
    hid_t        dataspace_id;
    hsize_t      size;
    haddr_t      address;
    hid_t        datatype;
    hid_t        native;
    H5T_class_t  clas;
    int          iSubdatasetType;
    double       adfGeoTransform[6];
    bool         bHasGeoTransform;

public:
    HDF5ImageDataset();
    ~HDF5ImageDataset();

    CPLErr CreateProjections( );
    static GDALDataset  *Open( GDALOpenInfo * );
    static int           Identify( GDALOpenInfo * );

    const char          *GetProjectionRef();
    virtual int         GetGCPCount( );
    virtual const char  *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs( ); 
    virtual CPLErr GetGeoTransform( double * padfTransform );

    /**
     * Identify if the subdataset has a known product format
     * It stores a product identifier in iSubdatasetType,
     * UNKNOWN_PRODUCT, if it isn't a recognizable format.
     */
    void IdentifyProductType();

    /**
     * Captures Geolocation information from a COSMO-SKYMED
     * file.
     * The geoid will allways be WGS84
     * The projection type may be UTM or UPS, depending on the
     * latitude from the center of the image.
     * @param iProductType type of HDF5 subproduct, see HDF5CSKProduct
     */
    void CaptureCSKGeolocation(int iProductType);

    /**
    * Get Geotransform information for COSMO-SKYMED files
    * In case of sucess it stores the transformation
    * in adfGeoTransform. In case of failure it doesn't
    * modify adfGeoTransform
    * @param iProductType type of HDF5 subproduct, see HDF5CSKProduct
    */
    void CaptureCSKGeoTransform(int iProductType);
    
    /**
     * @param iProductType type of HDF5 subproduct, see HDF5CSKProduct
     */
    void CaptureCSKGCPs(int iProductType);    
};

/************************************************************************/
/* ==================================================================== */
/*                              HDF5ImageDataset                        */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           HDF5ImageDataset()                         */
/************************************************************************/
HDF5ImageDataset::HDF5ImageDataset()
{
    nGCPCount       = 0;
    pszProjection   = NULL;
    pszGCPProjection= NULL;
    pasGCPList      = NULL;
    poH5Objects     = NULL;
    poH5RootGroup   = NULL;
    dims            = NULL;
    maxdims         = NULL;
    papszMetadata   = NULL;
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    iSubdatasetType    = UNKNOWN_PRODUCT;
    bHasGeoTransform   = false;
    dataset_id         = -1;
    dataspace_id       = -1;
    datatype           = -1;
    native             = -1;
}

/************************************************************************/
/*                            ~HDF5ImageDataset()                       */
/************************************************************************/
HDF5ImageDataset::~HDF5ImageDataset( )
{
    FlushCache();
    
    if( dataset_id > 0 )
        H5Dclose(dataset_id);
    if( dataspace_id > 0 )
        H5Sclose(dataspace_id);
    if( datatype > 0 )
        H5Tclose(datatype);
    if( native > 0 )
        H5Tclose(native);

    CPLFree(pszProjection);
    CPLFree(pszGCPProjection);

    if( dims )
        CPLFree( dims );

    if( maxdims )
        CPLFree( maxdims );

    if( nGCPCount > 0 )
    {
        for( int i = 0; i < nGCPCount; i++ )
        {
            if( pasGCPList[i].pszId )
                CPLFree( pasGCPList[i].pszId );
            if( pasGCPList[i].pszInfo )
                CPLFree( pasGCPList[i].pszInfo );
        }

        CPLFree( pasGCPList );
    }
}

/************************************************************************/
/* ==================================================================== */
/*                            Hdf5imagerasterband                       */
/* ==================================================================== */
/************************************************************************/
class HDF5ImageRasterBand : public GDALPamRasterBand
{
    friend class HDF5ImageDataset;

    int         bNoDataSet;
    double      dfNoDataValue;

public:

    HDF5ImageRasterBand( HDF5ImageDataset *, int, GDALDataType );
    ~HDF5ImageRasterBand();

    virtual CPLErr      IReadBlock( int, int, void * );
    virtual double      GetNoDataValue( int * );
    virtual CPLErr      SetNoDataValue( double );
    /*  virtual CPLErr          IWriteBlock( int, int, void * ); */
};

/************************************************************************/
/*                        ~HDF5ImageRasterBand()                        */
/************************************************************************/

HDF5ImageRasterBand::~HDF5ImageRasterBand()
{

}
/************************************************************************/
/*                           HDF5ImageRasterBand()                      */
/************************************************************************/
HDF5ImageRasterBand::HDF5ImageRasterBand( HDF5ImageDataset *poDS, int nBand,
                                          GDALDataType eType )

{
    char          **papszMetaGlobal;
    this->poDS    = poDS;
    this->nBand   = nBand;
    eDataType     = eType;
    bNoDataSet    = FALSE;
    dfNoDataValue = -9999;
    nBlockXSize   = poDS->GetRasterXSize( );
    nBlockYSize   = 1;

/* -------------------------------------------------------------------- */
/*      Take a copy of Global Metadata since  I can't pass Raster       */
/*      variable to Iterate function.                                   */
/* -------------------------------------------------------------------- */
    papszMetaGlobal = CSLDuplicate( poDS->papszMetadata );
    CSLDestroy( poDS->papszMetadata );
    poDS->papszMetadata = NULL;

    if( poDS->poH5Objects->nType == H5G_DATASET ) {
        poDS->CreateMetadata( poDS->poH5Objects, H5G_DATASET );
    }

/* -------------------------------------------------------------------- */
/*      Recover Global Metadat and set Band Metadata                    */
/* -------------------------------------------------------------------- */

    SetMetadata( poDS->papszMetadata );

    CSLDestroy( poDS->papszMetadata );
    poDS->papszMetadata = CSLDuplicate( papszMetaGlobal );
    CSLDestroy( papszMetaGlobal );

    /* check for chunksize and set it as the blocksize (optimizes read) */
    hid_t listid = H5Dget_create_plist(((HDF5ImageDataset * )poDS)->dataset_id);
    if (listid>0)
    {
        if(H5Pget_layout(listid) == H5D_CHUNKED)
        {
            hsize_t panChunkDims[3];
            int nDimSize = H5Pget_chunk(listid, 3, panChunkDims);
            nBlockXSize   = (int) panChunkDims[nDimSize-1];
            nBlockYSize   = (int) panChunkDims[nDimSize-2];
        }
        H5Pclose(listid);
    }

}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/
double HDF5ImageRasterBand::GetNoDataValue( int * pbSuccess )

{
    if( bNoDataSet )
    {
        if( pbSuccess )
            *pbSuccess = bNoDataSet;

        return dfNoDataValue;
    }
    else
        return GDALPamRasterBand::GetNoDataValue( pbSuccess );
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/
CPLErr HDF5ImageRasterBand::SetNoDataValue( double dfNoData )

{
    bNoDataSet = TRUE;
    dfNoDataValue = dfNoData;

    return CE_None;
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/
CPLErr HDF5ImageRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                        void * pImage )
{
    herr_t      status;
    hsize_t     count[3];
    H5OFFSET_TYPE offset[3];
    int         nSizeOfData;
    hid_t       memspace;
    hsize_t     col_dims[3];
    hsize_t     rank;

    HDF5ImageDataset    *poGDS = ( HDF5ImageDataset * ) poDS;

    if( poGDS->eAccess == GA_Update ) {
        memset( pImage, 0,
            nBlockXSize * nBlockYSize *
            GDALGetDataTypeSize( eDataType )/8 );
        return CE_None;
    }

    rank=2;

    if( poGDS->ndims == 3 ){
        rank=3;
        offset[0]   = nBand-1;
        count[0]    = 1;
        col_dims[0] = 1;
    }

    offset[poGDS->ndims - 2] = nBlockYOff*nBlockYSize;
    offset[poGDS->ndims - 1] = nBlockXOff*nBlockXSize;
    count[poGDS->ndims - 2]  = nBlockYSize;
    count[poGDS->ndims - 1]  = nBlockXSize;

    nSizeOfData = H5Tget_size( poGDS->native );
    memset( pImage,0,nBlockXSize*nBlockYSize*nSizeOfData );

    /*  blocksize may not be a multiple of imagesize */
    count[poGDS->ndims - 2]  = MIN( size_t(nBlockYSize),
                                    poDS->GetRasterYSize() -
                                            offset[poGDS->ndims - 2]);
    count[poGDS->ndims - 1]  = MIN( size_t(nBlockXSize),
                                    poDS->GetRasterXSize()-
                                            offset[poGDS->ndims - 1]);

/* -------------------------------------------------------------------- */
/*      Select block from file space                                    */
/* -------------------------------------------------------------------- */
    status =  H5Sselect_hyperslab( poGDS->dataspace_id,
                                   H5S_SELECT_SET,
                                   offset, NULL,
                                   count, NULL );

/* -------------------------------------------------------------------- */
/*      Create memory space to receive the data                         */
/* -------------------------------------------------------------------- */
    col_dims[poGDS->ndims-2]=nBlockYSize;
    col_dims[poGDS->ndims-1]=nBlockXSize;
    memspace = H5Screate_simple( (int) rank, col_dims, NULL );
    H5OFFSET_TYPE mem_offset[3] = {0, 0, 0};
    status =  H5Sselect_hyperslab(memspace,
                                  H5S_SELECT_SET,
                                  mem_offset, NULL,
                                  count, NULL);

    status = H5Dread ( poGDS->dataset_id,
                       poGDS->native,
                       memspace,
                       poGDS->dataspace_id,
                       H5P_DEFAULT,
                       pImage );

    H5Sclose( memspace );

    if( status < 0 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "H5Dread() failed for block." );
        return CE_Failure;
    }
    else
        return CE_None;
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int HDF5ImageDataset::Identify( GDALOpenInfo *poOpenInfo )

{
    if(!EQUALN( poOpenInfo->pszFilename, "HDF5:", 5 ) )
        return FALSE;
    else
        return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
GDALDataset *HDF5ImageDataset::Open( GDALOpenInfo * poOpenInfo )
{
    int i;
    HDF5ImageDataset    *poDS;
    char szFilename[2048];

    if(!EQUALN( poOpenInfo->pszFilename, "HDF5:", 5 ) ||
        strlen(poOpenInfo->pszFilename) > sizeof(szFilename) - 3 )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The HDF5ImageDataset driver does not support update access to existing"
                  " datasets.\n" );
        return NULL;
    }

    poDS = new HDF5ImageDataset();

    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    /* printf("poOpenInfo->pszFilename %s\n",poOpenInfo->pszFilename); */
    char **papszName =
        CSLTokenizeString2(  poOpenInfo->pszFilename,
                             ":", CSLT_HONOURSTRINGS|CSLT_PRESERVEESCAPES );

    if( !((CSLCount(papszName) == 3) || (CSLCount(papszName) == 4)) )
    {
        CSLDestroy(papszName);
        delete poDS;
        return NULL;
    }

    poDS->SetDescription( poOpenInfo->pszFilename );

    /* -------------------------------------------------------------------- */
    /*    Check for drive name in windows HDF5:"D:\...                      */
    /* -------------------------------------------------------------------- */
    CPLString osSubdatasetName;

    strcpy(szFilename, papszName[1]);

    if( strlen(papszName[1]) == 1 && papszName[3] != NULL )
    {
        strcat(szFilename, ":");
        strcat(szFilename, papszName[2]);
        osSubdatasetName = papszName[3];
    }
    else
        osSubdatasetName = papszName[2];
    
    poDS->SetSubdatasetName( osSubdatasetName );

    CSLDestroy(papszName);
    papszName = NULL;

    if( !H5Fis_hdf5(szFilename) ) {
        delete poDS;
        return NULL;
    }

    poDS->SetPhysicalFilename( szFilename );

    /* -------------------------------------------------------------------- */
    /*      Try opening the dataset.                                        */
    /* -------------------------------------------------------------------- */
    poDS->hHDF5 = H5Fopen(szFilename,
                          H5F_ACC_RDONLY,
                          H5P_DEFAULT );

    if( poDS->hHDF5 < 0 )
    {
        delete poDS;
        return NULL;
    }

    poDS->hGroupID = H5Gopen( poDS->hHDF5, "/" );
    if( poDS->hGroupID < 0 )
    {
        poDS->bIsHDFEOS=false;
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      THIS IS AN HDF5 FILE                                            */
/* -------------------------------------------------------------------- */
    poDS->bIsHDFEOS=TRUE;
    poDS->ReadGlobalAttributes( FALSE );

/* -------------------------------------------------------------------- */
/*      Create HDF5 Data Hierarchy in a link list                       */
/* -------------------------------------------------------------------- */
    poDS->poH5Objects =
        poDS->HDF5FindDatasetObjectsbyPath( poDS->poH5RootGroup,
                                            osSubdatasetName );

    if( poDS->poH5Objects == NULL ) {
        delete poDS;
        return NULL;
    }
/* -------------------------------------------------------------------- */
/*      Retrieve HDF5 data information                                  */
/* -------------------------------------------------------------------- */
    poDS->dataset_id   = H5Dopen( poDS->hHDF5,poDS->poH5Objects->pszPath );
    poDS->dataspace_id = H5Dget_space( poDS->dataset_id );
    poDS->ndims        = H5Sget_simple_extent_ndims( poDS->dataspace_id );
    poDS->dims         = (hsize_t*)CPLCalloc( poDS->ndims, sizeof(hsize_t) );
    poDS->maxdims      = (hsize_t*)CPLCalloc( poDS->ndims, sizeof(hsize_t) );
    poDS->dimensions   = H5Sget_simple_extent_dims( poDS->dataspace_id,
                                                    poDS->dims,
                                                    poDS->maxdims );
    poDS->datatype = H5Dget_type( poDS->dataset_id );
    poDS->clas     = H5Tget_class( poDS->datatype );
    poDS->size     = H5Tget_size( poDS->datatype );
    poDS->address = H5Dget_offset( poDS->dataset_id );
    poDS->native  = H5Tget_native_type( poDS->datatype, H5T_DIR_ASCEND );

    poDS->nRasterYSize=(int)poDS->dims[poDS->ndims-2];   // Y
    poDS->nRasterXSize=(int)poDS->dims[poDS->ndims-1];   // X alway last

    poDS->nBands=1;

    if( poDS->ndims == 3 ) poDS->nBands=(int) poDS->dims[0];


    for(  i = 1; i <= poDS->nBands; i++ ) {
        HDF5ImageRasterBand *poBand =
            new HDF5ImageRasterBand( poDS, i,
                            poDS->GetDataType( poDS->native ) );

        poDS->SetBand( i, poBand );
        if( poBand->bNoDataSet )
            poBand->SetNoDataValue( 255 );
    }

    // CSK code in IdentifyProductType() and CreateProjections() 
    // uses dataset metadata.
    poDS->SetMetadata( poDS->papszMetadata );

    // Check if the hdf5 is a well known product type
    poDS->IdentifyProductType();

    poDS->CreateProjections( );

/* -------------------------------------------------------------------- */
/*      Setup/check for pam .aux.xml.                                   */
/* -------------------------------------------------------------------- */
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Setup overviews.                                                */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, ":::VIRTUAL:::" );

    return( poDS );
}


/************************************************************************/
/*                        GDALRegister_HDF5Image()                      */
/************************************************************************/
void GDALRegister_HDF5Image( )

{
    GDALDriver  *poDriver;

    if (! GDAL_CHECK_VERSION("HDF5Image driver"))
        return;

    if(  GDALGetDriverByName( "HDF5Image" ) == NULL )
    {
        poDriver = new GDALDriver( );

        poDriver->SetDescription( "HDF5Image" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                                   "HDF5 Dataset" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC,
                                   "frmt_hdf5.html" );
        poDriver->pfnOpen = HDF5ImageDataset::Open;
        poDriver->pfnIdentify = HDF5ImageDataset::Identify;

        GetGDALDriverManager( )->RegisterDriver( poDriver );
    }
}

/************************************************************************/
/*                         CreateProjections()                          */
/************************************************************************/
CPLErr HDF5ImageDataset::CreateProjections()
{
    switch(iSubdatasetType)
    {
    case CSK_PRODUCT:
    {
        const char *osMissionLevel = NULL;
        int productType = PROD_UNKNOWN;

        if(GetMetadataItem("Product_Type")!=NULL)
        {
            //Get the format's level
            osMissionLevel = HDF5Dataset::GetMetadataItem("Product_Type");

            if(EQUALN(osMissionLevel,"RAW",3))
                productType  = PROD_CSK_L0;

            if(EQUALN(osMissionLevel,"SSC",3))
                productType  = PROD_CSK_L1A;

            if(EQUALN(osMissionLevel,"DGM",3))
                productType  = PROD_CSK_L1B;

            if(EQUALN(osMissionLevel,"GEC",3))
                productType  = PROD_CSK_L1C;

            if(EQUALN(osMissionLevel,"GTC",3))
                productType  = PROD_CSK_L1D;
        }
            
        CaptureCSKGeoTransform(productType);
        CaptureCSKGeolocation(productType);
        CaptureCSKGCPs(productType);
        
        break;
    }
    case UNKNOWN_PRODUCT:
    {
#define NBGCPLAT 100
#define NBGCPLON 30

    hid_t LatitudeDatasetID  = -1;
    hid_t LongitudeDatasetID = -1;
    hid_t LatitudeDataspaceID;
    hid_t LongitudeDataspaceID;
    float* Latitude;
    float* Longitude;
    int    i,j;
    int   nDeltaLat;
    int   nDeltaLon;

    nDeltaLat = nRasterYSize / NBGCPLAT;
    nDeltaLon = nRasterXSize / NBGCPLON;

    if( nDeltaLat == 0 || nDeltaLon == 0 )
        return CE_None;

    /* -------------------------------------------------------------------- */
    /*      Create HDF5 Data Hierarchy in a link list                       */
    /* -------------------------------------------------------------------- */
    poH5Objects=HDF5FindDatasetObjects( poH5RootGroup,  "Latitude" );
    if( !poH5Objects ) {
        return CE_None;
    }
    /* -------------------------------------------------------------------- */
    /*      The Lattitude and Longitude arrays must have a rank of 2 to     */
    /*      retrieve GCPs.                                                  */
    /* -------------------------------------------------------------------- */
    if( poH5Objects->nRank != 2 ) {
        return CE_None;
    }

    /* -------------------------------------------------------------------- */
    /*      Retrieve HDF5 data information                                  */
    /* -------------------------------------------------------------------- */
    LatitudeDatasetID   = H5Dopen( hHDF5,poH5Objects->pszPath );
    LatitudeDataspaceID = H5Dget_space( dataset_id );

    poH5Objects=HDF5FindDatasetObjects( poH5RootGroup, "Longitude" );
    LongitudeDatasetID   = H5Dopen( hHDF5,poH5Objects->pszPath );
    LongitudeDataspaceID = H5Dget_space( dataset_id );

    if( ( LatitudeDatasetID > 0 ) && ( LongitudeDatasetID > 0) ) {

        Latitude         = ( float * ) CPLCalloc(  nRasterYSize*nRasterXSize,
                            sizeof( float ) );
        Longitude         = ( float * ) CPLCalloc( nRasterYSize*nRasterXSize,
                            sizeof( float ) );
        memset( Latitude, 0, nRasterXSize*nRasterYSize*sizeof(  float ) );
        memset( Longitude, 0, nRasterXSize*nRasterYSize*sizeof( float ) );

        H5Dread ( LatitudeDatasetID,
            H5T_NATIVE_FLOAT,
            H5S_ALL,
            H5S_ALL,
            H5P_DEFAULT,
            Latitude );

        H5Dread ( LongitudeDatasetID,
            H5T_NATIVE_FLOAT,
            H5S_ALL,
            H5S_ALL,
            H5P_DEFAULT,
            Longitude );

        oSRS.SetWellKnownGeogCS( "WGS84" );
        CPLFree(pszProjection);
        CPLFree(pszGCPProjection);
        oSRS.exportToWkt( &pszProjection );
        oSRS.exportToWkt( &pszGCPProjection );

    /* -------------------------------------------------------------------- */
    /*  Fill the GCPs list.                                                 */
    /* -------------------------------------------------------------------- */
        nGCPCount = nRasterYSize/nDeltaLat * nRasterXSize/nDeltaLon;

        pasGCPList = ( GDAL_GCP * )
            CPLCalloc( nGCPCount, sizeof( GDAL_GCP ) );

        GDALInitGCPs( nGCPCount, pasGCPList );
        int k=0;

        int nYLimit = ((int)nRasterYSize/nDeltaLat) * nDeltaLat;
        int nXLimit = ((int)nRasterXSize/nDeltaLon) * nDeltaLon;
        for( j = 0; j < nYLimit; j+=nDeltaLat )
        {
            for( i = 0; i < nXLimit; i+=nDeltaLon )
            {
                int iGCP =  j * nRasterXSize + i;
                pasGCPList[k].dfGCPX = ( double ) Longitude[iGCP]+180.0;
                pasGCPList[k].dfGCPY = ( double ) Latitude[iGCP];

                pasGCPList[k].dfGCPPixel = i + 0.5;
                pasGCPList[k++].dfGCPLine =  j + 0.5;

            }
        }

        CPLFree( Latitude );
        CPLFree( Longitude );
    }
    
    if( LatitudeDatasetID > 0 )
        H5Dclose(LatitudeDatasetID);
    if( LongitudeDatasetID > 0 )
        H5Dclose(LongitudeDatasetID);

        break;
    }
    }
    return CE_None;

}
/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *HDF5ImageDataset::GetProjectionRef( )

{
    if( pszProjection )
        return pszProjection;
    else
        return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int HDF5ImageDataset::GetGCPCount( )

{
    if( nGCPCount > 0 )
        return nGCPCount;
    else
        return GDALPamDataset::GetGCPCount();
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *HDF5ImageDataset::GetGCPProjection( )

{
    if( nGCPCount > 0 )
        return pszGCPProjection;
    else
        return GDALPamDataset::GetGCPProjection();
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *HDF5ImageDataset::GetGCPs( )
{
    if( nGCPCount > 0 )
        return pasGCPList;
    else
        return GDALPamDataset::GetGCPs();
}

/************************************************************************/
/*                         GetGeoTransform()                            */
/************************************************************************/

CPLErr HDF5ImageDataset::GetGeoTransform( double * padfTransform )
{
    if ( bHasGeoTransform )
    {
        memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
        return CE_None;
    }
    else
        return GDALPamDataset::GetGeoTransform(padfTransform);
}

/************************************************************************/
/*                       IdentifyProductType()                          */
/************************************************************************/

/**
 * Identify if the subdataset has a known product format
 * It stores a product identifier in iSubdatasetType,
 * UNKNOWN_PRODUCT, if it isn't a recognizable format.
 */
void HDF5ImageDataset::IdentifyProductType()
{
    iSubdatasetType = UNKNOWN_PRODUCT;

/************************************************************************/
/*                               COSMO-SKYMED                           */
/************************************************************************/
   const char *pszMissionId;

    //Get the Mission Id as a char *, because the
    //field may not exist
    pszMissionId = HDF5Dataset::GetMetadataItem("Mission_ID");

    //If there is a Mission_ID field
     if(pszMissionId != NULL && strstr(GetDescription(), "QLK") == NULL)
         //Check if the mission type is CSK
         if(EQUAL(pszMissionId,"CSK"))
             iSubdatasetType = CSK_PRODUCT;
}

/************************************************************************/
/*                       CaptureCSKGeolocation()                        */
/************************************************************************/

/**
 * Captures Geolocation information from a COSMO-SKYMED
 * file.
 * The geoid will allways be WGS84
 * The projection type may be UTM or UPS, depending on the
 * latitude from the center of the image.
 * @param iProductType type of CSK subproduct, see HDF5CSKProduct
 */
void HDF5ImageDataset::CaptureCSKGeolocation(int iProductType)
{
    double *dfProjFalseEastNorth;
    double *dfProjScaleFactor;
    double *dfCenterCoord;

    //Set the ellipsoid to WGS84
    oSRS.SetWellKnownGeogCS( "WGS84" );

    if(iProductType == PROD_CSK_L1C||iProductType == PROD_CSK_L1D)
    {
        //Check if all the metadata attributes are present
        if(HDF5ReadDoubleAttr("Map Projection False East-North", &dfProjFalseEastNorth) == CE_Failure||
           HDF5ReadDoubleAttr("Map Projection Scale Factor", &dfProjScaleFactor) == CE_Failure||
           HDF5ReadDoubleAttr("Map Projection Centre", &dfCenterCoord) == CE_Failure||
           GetMetadataItem("Projection_ID") == NULL)
        {

            pszProjection = CPLStrdup("");
            pszGCPProjection = CPLStrdup("");
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "The CSK hdf5 file geolocation information is malformed\n" );
        }
        else
        {
            //Fetch projection Type
            CPLString osProjectionID = GetMetadataItem("Projection_ID");

            //If the projection is UTM
            if(EQUAL(osProjectionID,"UTM"))
            {
                // @TODO: use SetUTM
                oSRS.SetProjCS(SRS_PT_TRANSVERSE_MERCATOR);
                oSRS.SetTM(dfCenterCoord[0],
                           dfCenterCoord[1],
                           dfProjScaleFactor[0],
                           dfProjFalseEastNorth[0], 
                           dfProjFalseEastNorth[1]);
            }
            else
            {
                //TODO Test! I didn't had any UPS projected files to test!
                //If the projection is UPS
                if(EQUAL(osProjectionID,"UPS"))
                {
                    oSRS.SetProjCS(SRS_PT_POLAR_STEREOGRAPHIC);
                    oSRS.SetPS(dfCenterCoord[0], 
                               dfCenterCoord[1],
                               dfProjScaleFactor[0],
                               dfProjFalseEastNorth[0],
                               dfProjFalseEastNorth[1]);
                }
            }

            //Export Projection to Wkt.
            //In case of error then clean the projection
            if (oSRS.exportToWkt(&pszProjection) != OGRERR_NONE)
                pszProjection = CPLStrdup("");

            CPLFree(dfCenterCoord);
            CPLFree(dfProjScaleFactor);
            CPLFree(dfProjFalseEastNorth);
        }
    }
    else
    {
        //Export GCPProjection to Wkt.
        //In case of error then clean the projection
        if(oSRS.exportToWkt(&pszGCPProjection) != OGRERR_NONE)
            pszGCPProjection = CPLStrdup("");
    }
}

/************************************************************************/
/*                       CaptureCSKGeoTransform()                       */
/************************************************************************/

/**
* Get Geotransform information for COSMO-SKYMED files
* In case of sucess it stores the transformation
* in adfGeoTransform. In case of failure it doesn't
* modify adfGeoTransform
* @param iProductType type of CSK subproduct, see HDF5CSKProduct
*/
void HDF5ImageDataset::CaptureCSKGeoTransform(int iProductType)
{
    double *pdOutUL;
    double *pdLineSpacing;
    double *pdColumnSpacing;

    CPLString osULCoord;
    CPLString osULPath;
    CPLString osLineSpacingPath, osColumnSpacingPath;

    const char *pszSubdatasetName = GetSubdatasetName();

    bHasGeoTransform = FALSE;
    //If the product level is not L1C or L1D then
    //it doesn't have a valid projection
    if(iProductType == PROD_CSK_L1C||iProductType == PROD_CSK_L1D)
    {
        //If there is a subdataset
        if(pszSubdatasetName != NULL)
        {

            osULPath = pszSubdatasetName ;
            osULPath += "/Top Left East-North";

            osLineSpacingPath = pszSubdatasetName;
            osLineSpacingPath += "/Line Spacing";

            osColumnSpacingPath = pszSubdatasetName;
            osColumnSpacingPath += "/Column Spacing";


            //If it could find the attributes on the metadata
            if(HDF5ReadDoubleAttr(osULPath.c_str(), &pdOutUL) == CE_Failure ||
               HDF5ReadDoubleAttr(osLineSpacingPath.c_str(), &pdLineSpacing) == CE_Failure ||
               HDF5ReadDoubleAttr(osColumnSpacingPath.c_str(), &pdColumnSpacing) == CE_Failure)
            {
                bHasGeoTransform = FALSE;
            }
            else
            {
//            	geotransform[1] : width of pixel
//            	geotransform[4] : rotational coefficient, zero for north up images.
//            	geotransform[2] : rotational coefficient, zero for north up images.
//            	geotransform[5] : height of pixel (but negative)

                adfGeoTransform[0] = pdOutUL[0];
                adfGeoTransform[1] = pdLineSpacing[0];
                adfGeoTransform[2] = 0;
                adfGeoTransform[3] = pdOutUL[1];
                adfGeoTransform[4] = 0;
                adfGeoTransform[5] = -pdColumnSpacing[0];


                CPLFree(pdOutUL);
                CPLFree(pdLineSpacing);
                CPLFree(pdColumnSpacing);

                bHasGeoTransform = TRUE;
            }
        }
    }
}


/************************************************************************/
/*                          CaptureCSKGCPs()                            */
/************************************************************************/

/**
 * Retrieves and stores the GCPs from a COSMO-SKYMED dataset
 * It only retrieves the GCPs for L0, L1A and L1B products
 * for L1C and L1D products, geotransform is provided.
 * The GCPs provided will be the Image's corners.
 * @param iProductType type of CSK product @see HDF5CSKProductEnum
 */
void HDF5ImageDataset::CaptureCSKGCPs(int iProductType)
{
    //Only retrieve GCPs for L0,L1A and L1B products
    if(iProductType == PROD_CSK_L0||iProductType == PROD_CSK_L1A||
       iProductType == PROD_CSK_L1B)
    {
        int i;
        double *pdCornerCoordinates;

        nGCPCount=4;
        pasGCPList = (GDAL_GCP *) CPLCalloc(sizeof(GDAL_GCP),4);
        CPLString osCornerName[4];
        double pdCornerPixel[4];
        double pdCornerLine[4];

        const char *pszSubdatasetName = GetSubdatasetName();

        //Load the subdataset name first
        for(i=0;i <4;i++)
            osCornerName[i] = pszSubdatasetName;

        //Load the attribute name, and raster coordinates for
        //all the corners
        osCornerName[0] += "/Top Left Geodetic Coordinates";
        pdCornerPixel[0] = 0;
        pdCornerLine[0] = 0;

        osCornerName[1] += "/Top Right Geodetic Coordinates";
        pdCornerPixel[1] = GetRasterXSize();
        pdCornerLine[1] = 0;

        osCornerName[2] += "/Bottom Left Geodetic Coordinates";
        pdCornerPixel[2] = 0;
        pdCornerLine[2] = GetRasterYSize();

        osCornerName[3] += "/Bottom Right Geodetic Coordinates";
        pdCornerPixel[3] = GetRasterXSize();
        pdCornerLine[3] = GetRasterYSize();

        //For all the image's corners
        for(i=0;i<4;i++)
        {
            GDALInitGCPs( 1, pasGCPList + i );

            CPLFree( pasGCPList[i].pszId );
            pasGCPList[i].pszId = NULL;

            //Retrieve the attributes
            if(HDF5ReadDoubleAttr(osCornerName[i].c_str(), 
                                  &pdCornerCoordinates) == CE_Failure)
            {
                CPLError( CE_Failure, CPLE_OpenFailed,
                             "Error retrieving CSK GCPs\n" );
                // Free on failure, e.g. in case of QLK subdataset.
                for( int i = 0; i < 4; i++ )
                {
                    if( pasGCPList[i].pszId )
                        CPLFree( pasGCPList[i].pszId );
                    if( pasGCPList[i].pszInfo )
                        CPLFree( pasGCPList[i].pszInfo );
	            }
                CPLFree( pasGCPList );
                pasGCPList = NULL;
                nGCPCount = 0;
                break;
            }

            //Fill the GCPs name
            pasGCPList[i].pszId = CPLStrdup( osCornerName[i].c_str() );

            //Fill the coordinates
            pasGCPList[i].dfGCPX = pdCornerCoordinates[1];
            pasGCPList[i].dfGCPY = pdCornerCoordinates[0];
            pasGCPList[i].dfGCPZ = pdCornerCoordinates[2];
            pasGCPList[i].dfGCPPixel = pdCornerPixel[i];
            pasGCPList[i].dfGCPLine = pdCornerLine[i];

            //Free the returned coordinates
            CPLFree(pdCornerCoordinates);
        }
    }
}

