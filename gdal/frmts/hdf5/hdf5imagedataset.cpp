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
#include "hdf5.h"

#include "gdal_pam.h"
#include "gdal_priv.h"
#include "cpl_string.h"
#include "hdf5dataset.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_HDF5Image(void);
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

    friend class HDF5ImageRasterBand;

    char        *pszProjection;
    char        *pszGCPProjection;
    GDAL_GCP    *pasGCPList;
    int         nGCPCount;
    OGRSpatialReference oSRS;

    hsize_t      *dims,*maxdims;
    char          **papszName;
    HDF5GroupObjects *poH5Objects;
    int          ndims,dimensions;
    hid_t        dataset_id;
    hid_t        dataspace_id;
    hsize_t      size;
    int          address;
    hid_t        datatype;
    hid_t        native;
    H5T_class_t  clas;

public:
    HDF5ImageDataset();
    ~HDF5ImageDataset();
    
    CPLErr CreateProjections( );
    static GDALDataset  *Open( GDALOpenInfo * );
    const char          *GetProjectionRef();
    virtual CPLErr      SetProjection( const char * );
    virtual int         GetGCPCount( );
    virtual const char  *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs( ); 
    
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

    fp=NULL;
    nGCPCount       = -1;
    pszProjection   = NULL;
    pasGCPList      = NULL;
    papszName       = NULL;
    pszFilename     = NULL;
    poH5Objects     = NULL;
    poH5RootGroup   = NULL;
    dims            = NULL;
    maxdims         = NULL;
    papszName       = NULL;
    papszMetadata   = NULL;

}

/************************************************************************/
/*                            ~HDF5ImageDataset()                       */
/************************************************************************/
HDF5ImageDataset::~HDF5ImageDataset( )
{


    if( papszName != NULL )
    	CSLDestroy( papszName );

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
    char        *pszFilename;
    

public:
  
    HDF5ImageRasterBand( HDF5ImageDataset *, int, GDALDataType );
    ~HDF5ImageRasterBand();

    virtual CPLErr          IReadBlock( int, int, void * );
    virtual double	    GetNoDataValue( int * ); 
    virtual CPLErr	    SetNoDataValue( double );
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
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/
double HDF5ImageRasterBand::GetNoDataValue( int * pbSuccess )

{
    if( pbSuccess )
	*pbSuccess = bNoDataSet;

    return dfNoDataValue;
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

    offset[poGDS->ndims - 2] = nBlockYOff;
    offset[poGDS->ndims - 1] = nBlockXOff;
    count[poGDS->ndims - 2]  = 1;
    count[poGDS->ndims - 1]  = poGDS->GetRasterXSize( );

    nSizeOfData = H5Tget_size( poGDS->native );
    memset( pImage,0,count[poGDS->ndims-1]-offset[poGDS->ndims-1]*nSizeOfData );

/* -------------------------------------------------------------------- */
/*      Select 1 line                                                   */
/* -------------------------------------------------------------------- */
    status =  H5Sselect_hyperslab( poGDS->dataspace_id, 
				  H5S_SELECT_SET, 
				  offset, NULL, 
				  count, NULL );
   
/* -------------------------------------------------------------------- */
/*      Create memory space to receive the data                         */
/* -------------------------------------------------------------------- */
    col_dims[poGDS->ndims-2]=1;
    col_dims[poGDS->ndims-1]=count[poGDS->ndims-1];
    memspace = H5Screate_simple( rank, col_dims, NULL );

    status = H5Dread ( poGDS->dataset_id,
		      poGDS->native, 
		      memspace,
		      poGDS->dataspace_id,
		      H5P_DEFAULT, 
		      pImage );

    return CE_None;
}


/************************************************************************/
/*                                Open()                                */
/************************************************************************/
GDALDataset *HDF5ImageDataset::Open( GDALOpenInfo * poOpenInfo )
{
    int i;
    HDF5ImageDataset    *poDS;
    int nDatasetPos = 2;
    char szFilename[2048];

    if(!EQUALN( poOpenInfo->pszFilename, "HDF5:", 5 ) )
	return NULL;
  
    poDS = new HDF5ImageDataset();

    poDS->fp = poOpenInfo->fp;
    poOpenInfo->fp = NULL;
  
    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    /* printf("poOpenInfo->pszFilename %s\n",poOpenInfo->pszFilename); */
    poDS->papszName = CSLTokenizeString2(  poOpenInfo->pszFilename,
				    ":", CSLT_HONOURSTRINGS );

    if( !((CSLCount(poDS->papszName) == 3) || 
          (CSLCount(poDS->papszName) == 4)) ){
        CSLDestroy(poDS->papszName);
        delete poDS;
        return NULL;
    }

    poDS->pszFilename = CPLStrdup( poOpenInfo->pszFilename );
  
    if( !EQUAL( poDS->papszName[0], "HDF5" ) ) {
        delete poDS;
	return NULL;
    }
  
    /* -------------------------------------------------------------------- */
    /*    Check for drive name in windows HDF5:"D:\...                      */
    /* -------------------------------------------------------------------- */
    strcpy(szFilename, poDS->papszName[1]);

    if( strlen(poDS->papszName[1]) == 1 ) {
	strcat(szFilename, ":");
	strcat(szFilename, poDS->papszName[2]);
        nDatasetPos = 3;
    }
    printf("szFilenname %s\n",szFilename);
    if( !H5Fis_hdf5(szFilename) ) {
        delete poDS;
        return NULL;
    }

    /* -------------------------------------------------------------------- */
    /*      Try opening the dataset.                                        */
    /* -------------------------------------------------------------------- */
    poDS->hHDF5 = H5Fopen(szFilename,
			  H5F_ACC_RDONLY, 
			  H5P_DEFAULT );
  
    if( poDS->hHDF5 < 0 )  {
        delete poDS;
	return NULL;
    }
  
    poDS->hGroupID = H5Gopen( poDS->hHDF5, "/" ); 
    if( poDS->hGroupID < 0 ){
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
				      poDS->papszName[nDatasetPos] );

    if( poDS->poH5Objects == NULL ) {
	return NULL;
    }
/* -------------------------------------------------------------------- */
/*      Retrieve HDF5 data information                                  */
/* -------------------------------------------------------------------- */
    poDS->dataset_id   = H5Dopen( poDS->hHDF5,poDS->poH5Objects->pszPath ); 
    poDS->dataspace_id = H5Dget_space( poDS->dataset_id );                       
    poDS->ndims        = H5Sget_simple_extent_ndims( poDS->dataspace_id );
    poDS->dims         = ( hsize_t * )CPLCalloc( poDS->ndims, 
						 sizeof( hsize_t ) );
    poDS->maxdims      = ( hsize_t * )CPLCalloc( poDS->ndims, 
						 sizeof( hsize_t ) );
    poDS->dimensions   = H5Sget_simple_extent_dims( poDS->dataspace_id,
						   poDS->dims,
						   poDS->maxdims );
    poDS->datatype = H5Dget_type( poDS->dataset_id );
    poDS->clas     = H5Tget_class( poDS->datatype );
    poDS->size     = H5Tget_size( poDS->datatype );
    poDS->address = H5Dget_offset( poDS->dataset_id );
    poDS->native  = H5Tget_native_type( poDS->datatype, H5T_DIR_ASCEND );

    poDS->nRasterYSize=poDS->dims[poDS->ndims-2];   // Y
    poDS->nRasterXSize=poDS->dims[poDS->ndims-1];   // X alway last

    poDS->nBands=1;

    if( poDS->ndims == 3 ) poDS->nBands=poDS->dims[0];


    for(  i = 1; i <= poDS->nBands; i++ ) {
	HDF5ImageRasterBand *poBand = 
	    new HDF5ImageRasterBand( poDS, i, 
				     poDS->GetDataType( poDS->native ) );
	
	poDS->SetBand( i, poBand );
	if( poBand->bNoDataSet )
		poBand->SetNoDataValue( 255 );
    }

    
    poDS->oSRS.SetWellKnownGeogCS( "WGS84" );
    poDS->oSRS.exportToWkt( &poDS->pszProjection );
    poDS->CreateProjections( );

    poDS->SetMetadata( poDS->papszMetadata );
    return( poDS );
}


/************************************************************************/
/*                        GDALRegister_HDF5Image()                      */
/************************************************************************/
void GDALRegister_HDF5Image( )

{
    GDALDriver  *poDriver;

    if(  GDALGetDriverByName( "HDF5Image" ) == NULL )
	{
	    poDriver = new GDALDriver( );
        
	    poDriver->SetDescription( "HDF5Image" );
	    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
				      "HDF5 Dataset" );
	    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
				      "frmt_hdf5.html" );
	    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
				      "Byte Int16 UInt16 Int32 UInt32 Float32 Float64"  );
	    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
				      "<CreationOptionList>"
				      "   <Option name='RANK' type='int' description='Rank of output file'/>"
				      "</CreationOptionList>" );
	    poDriver->pfnOpen = HDF5ImageDataset::Open;

	    GetGDALDriverManager( )->RegisterDriver( poDriver );
	}
}

/************************************************************************/
/*                         CreateProjections()                          */
/************************************************************************/
CPLErr HDF5ImageDataset::CreateProjections()
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
	for( j = 0; j < nYLimit; j+=nDeltaLat ) {
	    for( i = 0; i < nXLimit; i+=nDeltaLon ) {
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
    return CE_None;

}
/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *HDF5ImageDataset::GetProjectionRef( )
    
{
    return pszProjection;
}

/************************************************************************/
/*                          SetProjection()                             */
/************************************************************************/

CPLErr HDF5ImageDataset::SetProjection( const char *pszNewProjection )

{
    if( pszProjection )
	CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszNewProjection );
    
    return CE_None;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int HDF5ImageDataset::GetGCPCount( )
    
{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *HDF5ImageDataset::GetGCPProjection( )

{
    if( nGCPCount > 0 )
	return pszGCPProjection;
    else
	return "";
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *HDF5ImageDataset::GetGCPs( )
{
    return pasGCPList;
}





