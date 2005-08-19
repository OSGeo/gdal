/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 4 (HDF4)
 * Purpose:  Read subdatasets of HDF4 file.
 *           This driver initially based on code supplied by Markus Neteler
 * Author:   Andrey Kiselev, dron@remotesensing.org
 *
 ******************************************************************************
 * Copyright (c) 2002
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
 * Revision 1.8  2005/08/19 15:03:20  dron
 * Fixed type of start offset array in IReadBlock().
 *
 * Revision 1.7  2005/08/13 02:22:01  dnadeau
 * check for windows drive letter in filename
 *
 * Revision 1.6  2005/08/12 23:39:50  dnadeau
 * add GCPs and projection class members
 *
 * Revision 1.5  2005/07/29 15:44:01  fwarmerdam
 * minor formatting change
 *
 * Revision 1.4  2005/07/27 16:41:46  dnadeau
 * take care of memory leak
 *
 * Revision 1.3  2005/07/20 21:16:10  dnadeau
 * fix commit problem with file
 *
 */
#include "hdf5.h"

#include "gdal_priv.h"
#include "cpl_string.h"
#include "hdf5dataset.h"
#include "ogr_spatialref.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_HDF5Image(void);
CPL_C_END



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
    
    CPLErr CreateProjections();
    static GDALDataset  *Open( GDALOpenInfo * );
    const char          *GetProjectionRef();
    virtual CPLErr      SetProjection( const char * );
    virtual int         GetGCPCount();
    virtual const char  *GetGCPProjection();
    virtual const GDAL_GCP *GetGCPs(); 
    
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
    papszName       = NULL;
    pszFilename     = NULL;
    poH5Objects     = NULL;
    poH5RootGroup   = NULL;
    dims            = NULL;
    maxdims         = NULL;

}

/************************************************************************/
/*                            ~HDF5ImageDataset()                       */
/************************************************************************/
HDF5ImageDataset::~HDF5ImageDataset()
{


    if (papszName != NULL)
	CSLDestroy(papszName);

    if(dims)
	CPLFree(dims);

    if(maxdims)
	CPLFree(maxdims);

    if( nGCPCount > 0 )
    {
        for( int i = 0; i < nGCPCount; i++ )
        {
            if ( pasGCPList[i].pszId )
                CPLFree( pasGCPList[i].pszId );
            if ( pasGCPList[i].pszInfo )
                CPLFree( pasGCPList[i].pszInfo );
        }

        CPLFree( pasGCPList );
    }



}

/************************************************************************/
/* ==================================================================== */
/*                            HDF5ImageRasterBand                       */
/* ==================================================================== */
/************************************************************************/
class HDF5ImageRasterBand : public GDALRasterBand
{
    friend class HDF5ImageDataset;

    int         bNoDataSet;
    double      dfNoDataValue;
    char        *pszFilename;
    

public:
  
    HDF5ImageRasterBand( HDF5ImageDataset *, int, GDALDataType );
    
    virtual CPLErr          IReadBlock( int, int, void * );
    virtual double	    GetNoDataValue( int * ); 
    virtual CPLErr	    SetNoDataValue( double );
    /*  virtual CPLErr          IWriteBlock( int, int, void * ); */
};

/************************************************************************/
/*                           HDF5ImageRasterBand()                      */
/************************************************************************/
HDF5ImageRasterBand::HDF5ImageRasterBand( HDF5ImageDataset *poDS, int nBand,
                                          GDALDataType eType )

{
    this->poDS    = poDS;
    this->nBand   = nBand;
    eDataType     = eType;
    bNoDataSet    = FALSE;
    dfNoDataValue = -9999;
    nBlockXSize   = poDS->GetRasterXSize();
    nBlockYSize   = 1;

}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/
double HDF5ImageRasterBand::GetNoDataValue( int * pbSuccess )

{
    if(pbSuccess)
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
    hssize_t    offset[3];
    int         nSizeOfData;
    hid_t       memspace;
    hsize_t     col_dims[3];
    hsize_t     rank;

    HDF5ImageDataset    *poGDS = (HDF5ImageDataset *) poDS;
   
    if( poGDS->eAccess == GA_Update ) {
	memset( pImage, 0,
		nBlockXSize * nBlockYSize * 
		GDALGetDataTypeSize(eDataType)/8);
	return CE_None;
    }

    rank=2;

    if(poGDS->ndims == 3){
	rank=3;
	offset[2] = nBand-1;
	count[2]  = 1;
	col_dims[2] = 1;
    }

    offset[0] = nBlockYOff;
    offset[1] = nBlockXOff;
    count[0]  = 1;
    count[1]  = poGDS->GetRasterXSize();

    nSizeOfData = H5Tget_size(poGDS->native);
    memset(pImage,0,count[1]-offset[1]*nSizeOfData);

/* -------------------------------------------------------------------- */
/*      Select 1 line                                                   */
/* -------------------------------------------------------------------- */
    status =  H5Sselect_hyperslab(poGDS->dataspace_id, 
				  H5S_SELECT_SET, 
				  offset, NULL, 
				  count, NULL);
   

/* -------------------------------------------------------------------- */
/*      Create memory space to receive the data                         */
/* -------------------------------------------------------------------- */
    col_dims[0]=count[1];
    col_dims[1]=1;
    memspace = H5Screate_simple(rank,col_dims, NULL);

    status = H5Dread (poGDS->dataset_id,
		      poGDS->native, 
		      memspace,
		      poGDS->dataspace_id,
		      H5P_DEFAULT, 
		      pImage);

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
    poDS = new HDF5ImageDataset();
    poDS->fp = poOpenInfo->fp;
    poOpenInfo->fp = NULL;
  
    if(!EQUALN(poOpenInfo->pszFilename, "HDF5:", 5))
	return NULL;
  
    /* -------------------------------------------------------------------- */
    /*      Create a corresponding GDALDataset.                             */
    /* -------------------------------------------------------------------- */
    /* printf("poOpenInfo->pszFilename %s\n",poOpenInfo->pszFilename); */
    poDS->papszName = CSLTokenizeString2( poOpenInfo->pszFilename,
				    ":", CSLT_HONOURSTRINGS );

    if( !((CSLCount(poDS->papszName) == 3) || 
          (CSLCount(poDS->papszName) == 4)) ){
        CSLDestroy(poDS->papszName);
        return NULL;
    }
     	  
    poDS->pszFilename = CPLStrdup(poOpenInfo->pszFilename);
  
    if(!EQUAL(poDS->papszName[0], "HDF5")) {
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
	    return NULL;
        }

    /* -------------------------------------------------------------------- */
    /*      Try opening the dataset.                                        */
    /* -------------------------------------------------------------------- */
    poDS->hHDF5 = H5Fopen(szFilename,
			  H5F_ACC_RDONLY, 
			  H5P_DEFAULT);
  
    if (poDS->hHDF5 < 0)  {
	return NULL;
    }
    printf("Open!!\n");
  
    poDS->hGroupID = H5Gopen(poDS->hHDF5, "/"); 
    if (poDS->hGroupID < 0){
	poDS->bIsHDFEOS=false;
	return NULL;
    }

/* -------------------------------------------------------------------- */
/*      THIS IS AN HDF5 FILE                                            */
/* -------------------------------------------------------------------- */
    poDS->bIsHDFEOS=TRUE;
    poDS->ReadGlobalAttributes(FALSE);
  
/* -------------------------------------------------------------------- */
/*      Create HDF5 Data Hierarchy in a link list                       */
/* -------------------------------------------------------------------- */
    poDS->poH5Objects=
	poDS->HDF5FindDatasetObjects(poDS->poH5RootGroup, 
				     poDS->papszName[nDatasetPos]);
	printf("%s\n",poDS->poH5Objects->pszPath);
/* -------------------------------------------------------------------- */
/*      Retrieve HDF5 data information                                  */
/* -------------------------------------------------------------------- */
    poDS->dataset_id   = H5Dopen(poDS->hHDF5,poDS->poH5Objects->pszPath); 
    poDS->dataspace_id = H5Dget_space(poDS->dataset_id);                       
    poDS->ndims        = H5Sget_simple_extent_ndims(poDS->dataspace_id);
    poDS->dims         = (hsize_t *)CPLCalloc(poDS->ndims, sizeof(hsize_t));
    poDS->maxdims      = (hsize_t *)CPLCalloc(poDS->ndims, sizeof(hsize_t));
    poDS->dimensions   = H5Sget_simple_extent_dims(poDS->dataspace_id,
						   poDS->dims,
						   poDS->maxdims);
    poDS->datatype = H5Dget_type(poDS->dataset_id);
    poDS->clas     = H5Tget_class(poDS->datatype);
    poDS->size     = H5Tget_size(poDS->datatype);
    poDS->address = H5Dget_offset(poDS->dataset_id);
    poDS->native  = H5Tget_native_type(poDS->datatype, H5T_DIR_ASCEND);

    poDS->nRasterYSize=poDS->dims[0];
    poDS->nRasterXSize=poDS->dims[1];

    poDS->nBands=1;

    if(poDS->ndims == 3) poDS->nBands=poDS->dims[poDS->ndims-1];


    for( i = 1; i <= poDS->nBands; i++ ) {
	HDF5ImageRasterBand *poBand = 
	    new HDF5ImageRasterBand(poDS, i, poDS->GetDataType(poDS->native));
	
	poDS->SetBand( i, poBand );
	if ( poBand->bNoDataSet )
		poBand->SetNoDataValue( 255 );
    }

    
    poDS->CreateProjections();


    return(poDS);
}


/************************************************************************/
/*                        GDALRegister_HDF5Image()                      */
/************************************************************************/
void GDALRegister_HDF5Image()

{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "HDF5Image" ) == NULL )
	{
	    poDriver = new GDALDriver();
        
	    poDriver->SetDescription("HDF5Image");
	    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, 
				      "HDF5 Dataset");
	    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, 
				      "frmt_hdf5.html");
	    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, 
				      "Byte Int16 UInt16 Int32 UInt32 Float32 Float64" );
	    poDriver->SetMetadataItem(GDAL_DMD_CREATIONOPTIONLIST, 
				      "<CreationOptionList>"
				      "   <Option name='RANK' type='int' description='Rank of output file'/>"
				      "</CreationOptionList>");
	    poDriver->pfnOpen = HDF5ImageDataset::Open;

	    GetGDALDriverManager()->RegisterDriver(poDriver);
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
/* -------------------------------------------------------------------- */
/*      Create HDF5 Data Hierarchy in a link list                       */
/* -------------------------------------------------------------------- */
    poH5Objects=HDF5FindDatasetObjects(poH5RootGroup,  "Latitude");
/* -------------------------------------------------------------------- */
/*      Retrieve HDF5 data information                                  */
/* -------------------------------------------------------------------- */
    LatitudeDatasetID   = H5Dopen(hHDF5,poH5Objects->pszPath); 
    LatitudeDataspaceID = H5Dget_space(dataset_id);                       

    poH5Objects=HDF5FindDatasetObjects(poH5RootGroup, "Longitude");
    LongitudeDatasetID   = H5Dopen(hHDF5,poH5Objects->pszPath); 
    LongitudeDataspaceID = H5Dget_space(dataset_id);                       

    if( ( LatitudeDatasetID > 0 ) && ( LongitudeDatasetID > 0) ) {
	
	Latitude         = (float *) CPLCalloc( nRasterYSize*nRasterXSize, 
						sizeof(float) );
	Longitude         = (float *) CPLCalloc( nRasterYSize*nRasterXSize, 
						 sizeof(float) );
	memset( Latitude, 0, nRasterXSize*nRasterYSize*sizeof( float ) );
	memset( Longitude, 0, nRasterXSize*nRasterYSize*sizeof( float ) );
	
	H5Dread (LatitudeDatasetID,
		 H5T_NATIVE_FLOAT, 
		 H5S_ALL,
		 H5S_ALL,
		 H5P_DEFAULT, 
		 Latitude);
	
	H5Dread (LongitudeDatasetID,
		 H5T_NATIVE_FLOAT, 
		 H5S_ALL,
		 H5S_ALL,
		 H5P_DEFAULT, 
		 Longitude);
	
	oSRS.SetWellKnownGeogCS( "WGS84" );
	CPLFree( pszProjection );
	oSRS.exportToWkt( &pszProjection );
	oSRS.exportToWkt( &pszGCPProjection );
	
/* -------------------------------------------------------------------- */
/*  Fill the GCPs list.                                                 */
/* -------------------------------------------------------------------- */
	
	int nDeltaLat = (nRasterYSize / NBGCPLAT);
	int nDeltaLon = (nRasterXSize / NBGCPLON);
	nGCPCount =  (nRasterYSize / nDeltaLat); 
	nGCPCount *= (nRasterXSize / nDeltaLon);
	pasGCPList = (GDAL_GCP *)
	    CPLCalloc( nGCPCount, sizeof( GDAL_GCP ) );
	
	GDALInitGCPs( nGCPCount, pasGCPList );
	int k=0;

	printf("nGCPCount = %d \n",nGCPCount);
	for ( j = 0; j <= nRasterYSize-nDeltaLat; j+=nDeltaLat ) {
	    for ( i = 0; i <= nRasterXSize-nDeltaLon; i+=nDeltaLon ) {
		int iGCP =  j * nRasterXSize + i;
		
		pasGCPList[k].dfGCPX = (double) Longitude[iGCP]+180.0;
		pasGCPList[k].dfGCPY = (double) Latitude[iGCP];
		
		pasGCPList[k].dfGCPPixel = i + 0.5;
		pasGCPList[k++].dfGCPLine =  j + 0.5;
		
	    }
	}
printf("k=%d\n",k);
	
	CPLFree(Latitude);
	CPLFree(Longitude);
    }
    return CE_None;

}
/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

  const char *HDF5ImageDataset::GetProjectionRef()

  {
  return pszProjection;
  }

/************************************************************************/
/*                          SetProjection()                             */
/************************************************************************/

CPLErr HDF5ImageDataset::SetProjection( const char *pszNewProjection )

{
    if ( pszProjection )
	CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszNewProjection );
    
    return CE_None;
}

/************************************************************************/
/*                            GetGCPCount()                             */
/************************************************************************/

int HDF5ImageDataset::GetGCPCount()
    
{
    return nGCPCount;
}

/************************************************************************/
/*                          GetGCPProjection()                          */
/************************************************************************/

const char *HDF5ImageDataset::GetGCPProjection()

{
    if( nGCPCount > 0 )
	return pszGCPProjection;
    else
	return "";
}

/************************************************************************/
/*                               GetGCPs()                              */
/************************************************************************/

const GDAL_GCP *HDF5ImageDataset::GetGCPs()
{
    return pasGCPList;
}





