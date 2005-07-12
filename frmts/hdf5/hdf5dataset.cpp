/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  HDF5 Datasets. Open HDF5 file, fetch metadata and list of
 *           subdatasets.
 *           This driver initially based on code supplied by Markus Neteler
 * Author:  
 *
 ******************************************************************************
 * Copyright (c) 2005
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
 * Revision 1.2  2005/07/12 17:13:57  denad21
 * comment printf and pointer problem
 *
 *
 *
 */
#include "hdf5.h"

#include "gdal_priv.h"
#include "cpl_string.h"
#include "hdf5dataset.h"

CPL_CVSID("$Id$");

CPL_C_START
void	GDALRegister_HDF5(void);
CPL_C_END



/************************************************************************/
/* ==================================================================== */
/*				HDF5Dataset				*/
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        GDALRegister_HDF5()				*/
/************************************************************************/
void GDALRegister_HDF5()

{
    GDALDriver	*poDriver;
    if( GDALGetDriverByName("HDF5") == NULL )
    {
        poDriver = new GDALDriver();
        poDriver->SetDescription("HDF5");
        poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, 
                                  "Hierarchical Data Format Release 5");
        poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, 
                                  "frmt_hdf5.html");
        poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "hdf5");
        poDriver->pfnOpen = HDF5Dataset::Open;
        GetGDALDriverManager()->RegisterDriver(poDriver);
    }
}

/************************************************************************/
/*                           HDF5Dataset()                      	*/
/************************************************************************/
HDF5Dataset::HDF5Dataset()
{
  papszSubDatasets    = NULL;
  papszGlobalMetadata = NULL;
  nSubDataCount       = -1;
  hHDF5        = -1;
  hDatasetID   = -1;
  hGroupID     = -1;
  bIsHDFEOS    = FALSE;
  nDatasetType = -1;
  poH5RootGroup = NULL;
}

/************************************************************************/
/*                            ~HDF5Dataset()                            */
/************************************************************************/
HDF5Dataset::~HDF5Dataset()
{
  if (papszGlobalMetadata)
    CSLDestroy( papszGlobalMetadata);
  if (hHDF5 > 0)
    H5Fclose(hHDF5);
  if(hGroupID > 0)
    H5Gclose(hGroupID);
  if (papszSubDatasets)
    CSLDestroy(papszSubDatasets);
  // if (pszFilename != NULL)
  //es  CPLFree(pszFilename);
  //  if (poH5RootGroup != NULL)
  //    DestroyH5Objects(poH5RootGroup);
}

/************************************************************************/
/*                            GetDataType()                             */
/*                                                                      */
/*      Transform HDF5 datatype to GDAL datatype                        */
/************************************************************************/
GDALDataType HDF5Dataset::GetDataType(hid_t TypeID) 
{
  if(H5Tequal(H5T_NATIVE_CHAR,TypeID))
     return GDT_Byte;
  if(H5Tequal(H5T_NATIVE_UCHAR, TypeID)) 
    return GDT_Byte;

  if(H5Tequal(H5T_NATIVE_SHORT,TypeID))
    return GDT_Int16;
  if(H5Tequal(H5T_NATIVE_USHORT, TypeID)) 
    return GDT_UInt16;
  if(H5Tequal(H5T_NATIVE_INT,TypeID)) 
    return GDT_Int16;      
  if(H5Tequal(H5T_NATIVE_UINT, TypeID)) 
    return GDT_UInt16;

  if(H5Tequal(H5T_NATIVE_LONG, TypeID)) 
    return GDT_Int32;;      
  if(H5Tequal(H5T_NATIVE_ULONG, TypeID)) 
    return GDT_UInt32;

  if(H5Tequal(H5T_NATIVE_FLOAT, TypeID)) 
    return GDT_Float32;
  if(H5Tequal(H5T_NATIVE_DOUBLE, TypeID)) 
    return GDT_Float64;

  if(H5Tequal(H5T_NATIVE_LLONG, TypeID)) 
    return GDT_Unknown;
  if(H5Tequal(H5T_NATIVE_ULLONG, TypeID)) 
    return GDT_Unknown;
  if(H5Tequal(H5T_NATIVE_DOUBLE, TypeID)) 
    return GDT_Unknown;
  return GDT_Unknown;
}

/************************************************************************/
/*                          GetDataTypeName()                           */
/*                                                                      */
/*      Return the human readable name of data type                     */
/************************************************************************/
const char *HDF5Dataset::GetDataTypeName(hid_t TypeID)
{
  if(H5Tequal(H5T_NATIVE_CHAR,TypeID))
    return "8-bit character";
  if(H5Tequal(H5T_NATIVE_UCHAR, TypeID)) 
    return "8-bit unsigned character";    
  if(H5Tequal(H5T_NATIVE_SHORT,TypeID))
    return "8-bit integer";
  if(H5Tequal(H5T_NATIVE_USHORT, TypeID)) 
    return "8-bit unsigned integer";
  if(H5Tequal(H5T_NATIVE_INT,TypeID)) 
    return "16-bit integer";
  if(H5Tequal(H5T_NATIVE_UINT, TypeID)) 
    return "16-bit unsigned integer";
  if(H5Tequal(H5T_NATIVE_LONG, TypeID)) 
    return "32-bit integer";
  if(H5Tequal(H5T_NATIVE_ULONG, TypeID)) 
    return "32-bit unsigned integer";
  if(H5Tequal(H5T_NATIVE_FLOAT, TypeID)) 
    return "32-bit floating-point";
  if(H5Tequal(H5T_NATIVE_DOUBLE, TypeID)) 
    return "64-bit floating-point";
  if(H5Tequal(H5T_NATIVE_LLONG, TypeID)) 
    return "64-bit integer";
  if(H5Tequal(H5T_NATIVE_ULLONG, TypeID)) 
    return "64-bit unsigned integer";
  if(H5Tequal(H5T_NATIVE_DOUBLE, TypeID)) 
    return "64-bit floating-point";

  return "Unknown";
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/
char **HDF5Dataset::GetMetadata( const char *pszDomain )
{
    if( pszDomain != NULL && EQUALN( pszDomain, "SUBDATASETS", 11 ) )
        return papszSubDatasets;
    else
        return GDALDataset::GetMetadata( pszDomain );
}

 
/************************************************************************/
/*                                Open()                                */
/************************************************************************/
GDALDataset *HDF5Dataset::Open(GDALOpenInfo * poOpenInfo)
{
    HDF5Dataset *poDS;
    CPLErr      Err;

    poDS = new HDF5Dataset();
    if( poOpenInfo->fp == NULL )
        return NULL;

    poDS->fp = poOpenInfo->fp;
    poOpenInfo->fp = NULL;

/* -------------------------------------------------------------------- */
/*  We have special routine in the HDF library for format checking!     */
/* -------------------------------------------------------------------- */
    poDS->pszFilename = strdup(poOpenInfo->pszFilename);
    if (!H5Fis_hdf5(poOpenInfo->pszFilename)) {
      return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    poDS->hHDF5 = H5Fopen(poOpenInfo->pszFilename, 
   			  H5F_ACC_RDONLY, 
   			  H5P_DEFAULT);
    if (poDS->hHDF5 < 0)  {
   	return NULL;
    }
   
    poDS->hGroupID = H5Gopen(poDS->hHDF5, "/"); 
    if (poDS->hGroupID < 0){
      poDS->bIsHDFEOS=false;
      return NULL;
    }
   
    poDS->bIsHDFEOS=true;
    Err = poDS->ReadGlobalAttributes(true);
    poDS->SetMetadata( poDS->papszGlobalMetadata, "" );

    return(poDS);
}

/************************************************************************/
/*                          DestroyH5Objects()                          */
/*                                                                      */
/*      Erase all objects                                               */
/************************************************************************/
void HDF5Dataset::DestroyH5Objects(HDF5GroupObjects *poH5Object)
{
  int i;
/* -------------------------------------------------------------------- */
/*      Visite all objects                                              */
/* -------------------------------------------------------------------- */

  if(poH5Object == NULL) return;

  for(i=0; i < poH5Object->nbObjs; i++)
    if(poH5Object->poHchild+i != NULL)
      DestroyH5Objects(poH5Object->poHchild+i);

/* -------------------------------------------------------------------- */
/*      Erase some data                                                 */
/* -------------------------------------------------------------------- */
  if(poH5Object->paDims != NULL)
    CPLFree(poH5Object->paDims);

/* -------------------------------------------------------------------- */
/*      All Children are visited and can be deleted.                    */
/* -------------------------------------------------------------------- */
  if ((i==poH5Object->nbObjs) && (poH5Object->nbObjs!=0)) 
    {
      /* printf("%d %s\n",poH5Object->nbObjs,poH5Object->pszName);*/
      CPLFree(poH5Object->poHchild);
    }
}

/************************************************************************/
/*                             CreatePath()                             */
/*                                                                      */
/*      Find Dataset path for HDopen                                    */
/************************************************************************/
char* CreatePath(HDF5GroupObjects *poH5Object)
{
  char pszPath[8192];

  pszPath[0]='\0';
  if(poH5Object->poHparent !=NULL) 
    {
      strcpy(pszPath,CreatePath(poH5Object->poHparent));
    }
  //  printf("aa%s\n",poH5Object->pszName);

  if(!EQUAL(poH5Object->pszName,"/")){
    strcat(pszPath,"/");
    strcat(pszPath,poH5Object->pszName);
  }
  poH5Object->pszPath  = (char *)strdup(pszPath);
  /*printf("poH5Object->pszPath %s\n",poH5Object->pszPath);*/
  return(poH5Object->pszPath);
}


/************************************************************************/
/*                             group_obs()                              */
/*                                                                      */
/*      Create HDF5 hierarchy into a linked list                        */
/************************************************************************/
herr_t HDF5CreateGroupObjs(hid_t hHDF5, const char *pszObjName, 
			   void *poHObjParent)
{
    herr_t	ret;			/* error return status */
    hid_t	hGroupID;		/* identifier of group */
    hid_t	hDatasetID;		/* identifier of dataset */
    hsize_t     nbObjs=0;		/* number of objects in a group */
    int         nbAttrs=0;		/* number of attributes in object */
    int		idx;
    int         n_dims;
    H5G_stat_t  oStatbuf;
    hsize_t     *dims=NULL;
    hsize_t     *maxdims=NULL;
    hid_t       datatype;
    hid_t       dataspace;
    hid_t       native;
    herr_t status;

    char *CreatePath(HDF5GroupObjects *poH5Object);

    HDF5GroupObjects *poHchild;
    HDF5GroupObjects *poHparent;

    poHparent = (HDF5GroupObjects *) poHObjParent;
    poHchild=poHparent->poHchild;

    if (H5Gget_objinfo( hHDF5, pszObjName, FALSE, &oStatbuf) < 0 ) return -1;


/* -------------------------------------------------------------------- */
/*      Initialize data for a child                                     */
/* -------------------------------------------------------------------- */
    for (idx=0; idx < poHparent->nbObjs; idx++)
    {
	if (poHchild->pszName == NULL) break;
	poHchild++;
    }
    if (idx == poHparent->nbObjs) return -1;  // all children parsed
  
/* -------------------------------------------------------------------- */
/*      Save child information                                          */
/* -------------------------------------------------------------------- */
    poHchild->pszName  = (char *)strdup(pszObjName);

    poHchild->nType  = oStatbuf.type;
    poHchild->nIndex = idx;
    poHchild->poHparent = poHparent;
    poHchild->nRank     = 0;
    poHchild->paDims    = 0;
    poHchild->HDatatype = 0;
    poHchild->pszPath  = CreatePath(poHchild);

    switch (oStatbuf.type) 
      {
      case H5G_LINK:
	poHchild->nbAttrs = 0;
	poHchild->nbObjs = 0;
	poHchild->poHchild = NULL;
	poHchild->nRank      = 0;
	poHchild->paDims    = 0;
	poHchild->HDatatype = 0;
	break;

      case H5G_GROUP:
	if ((hGroupID = H5Gopen(hHDF5, pszObjName)) == -1 ) {
	  printf("Error: unable to access \"%s\" group.\n", pszObjName);
	  return -1;
	}
	nbAttrs          = H5Aget_num_attrs(hGroupID);
	ret              = H5Gget_num_objs(hGroupID, &nbObjs);
        poHchild->nbAttrs= nbAttrs;
        poHchild->nbObjs = nbObjs;
	poHchild->nRank      = 0;
	poHchild->paDims    = 0;
	poHchild->HDatatype = 0;

	if (nbObjs > 0) poHchild->poHchild = 
			  (HDF5GroupObjects *)CPLCalloc(nbObjs, 
						     sizeof(HDF5GroupObjects));
	else poHchild->poHchild = NULL;

	ret = H5Giterate(hHDF5, pszObjName, NULL, 
			 HDF5CreateGroupObjs,  (void*) poHchild);
	ret = H5Gclose(hGroupID);
	break;

      case H5G_DATASET:

	if ((hDatasetID = H5Dopen(hHDF5, pszObjName)) == -1 ) {
	  printf("Error: unable to access \"%s\" dataset.\n", pszObjName);
	  return -1;
	}
	nbAttrs      = H5Aget_num_attrs(hDatasetID);
	datatype     = H5Dget_type(hDatasetID);
	dataspace    = H5Dget_space(hDatasetID);
	n_dims       = H5Sget_simple_extent_ndims(dataspace);
	native       = H5Tget_native_type(datatype, H5T_DIR_ASCEND);

	if(n_dims > 0) {
	  dims         = (hsize_t *) CPLCalloc(n_dims,sizeof(hsize_t));
	  maxdims      = (hsize_t *) CPLCalloc(n_dims,sizeof(hsize_t));
	}
	status       = H5Sget_simple_extent_dims(dataspace, dims, maxdims);
	if(maxdims != NULL)
	  CPLFree(maxdims);

	if (n_dims > 0) {
	  poHchild->nRank     = n_dims;   // rank of the array
	  poHchild->paDims    = dims;      // dimmension of the array.
	  poHchild->HDatatype = datatype;  // HDF5 datatype
	}
	else  {
	  poHchild->nRank     = -1;
	  poHchild->paDims    = NULL;
	  poHchild->HDatatype = 0;
	}
        poHchild->nbAttrs   = nbAttrs;
        poHchild->nbObjs    = 0;
	poHchild->poHchild  = NULL;
	poHchild->native    = native;
	ret                 = H5Dclose(hDatasetID);
	break;

      case H5G_TYPE:
        poHchild->nbAttrs = 0;
        poHchild->nbObjs = 0;
	poHchild->poHchild = NULL;
	poHchild->nRank      = 0;
	poHchild->paDims    = 0;
	poHchild->HDatatype = 0;
	break;
	
      default:
	break;
    }

    return 0;
}

/************************************************************************/
/*                       HDF5FindDatasetObjects()                       */
/*      Find object by name                                             */
/************************************************************************/

HDF5GroupObjects* HDF5Dataset::HDF5FindDatasetObjects
(HDF5GroupObjects *poH5Objects, char* pszDatasetName)
{
  int i;
  HDF5Dataset *poDS;
  HDF5GroupObjects *poObjectsFound;
  poDS=this;

  if(poH5Objects->nType == H5G_DATASET &&
     EQUAL(poH5Objects->pszName,pszDatasetName))
    {
      /*      printf("found it! %ld\n",(long) poH5Objects);*/
      return(poH5Objects);
    }

  if (poH5Objects->nbObjs >0 )
    for(i=0; i <poH5Objects->nbObjs; i++)
      {
	poObjectsFound=poDS->HDF5FindDatasetObjects(poH5Objects->poHchild+i, 
						    pszDatasetName);
/* -------------------------------------------------------------------- */
/*      Is this our dataset??                                           */
/* -------------------------------------------------------------------- */
	if(poObjectsFound != NULL) return(poObjectsFound);

      }
/* -------------------------------------------------------------------- */
/*      Dataset has not been found!                                     */
/* -------------------------------------------------------------------- */
  return(NULL);
  
  }

/************************************************************************/
/*                        HDF5ListGroupObjects()                        */
/*                                                                      */
/*      List all objects in HDF5                                        */
/************************************************************************/
CPLErr HDF5Dataset::HDF5ListGroupObjects(HDF5GroupObjects *poRootGroup)
{
  int i;
  char szTemp[8192];
  char szDim[8192];
  HDF5Dataset *poDS;
  poDS=this;
  
  if (poRootGroup->nbObjs >0 )
    for(i=0; i <poRootGroup->nbObjs; i++){
      poDS->HDF5ListGroupObjects(poRootGroup->poHchild+i);
    }
  
/* -------------------------------------------------------------------- */
/*      Create Sub dataset list                                         */
/* -------------------------------------------------------------------- */
  if(poRootGroup->nType == H5G_DATASET){
  
    switch(poRootGroup->nRank) 
      {
  	 case 3: {
  	   szDim[0]='\0';
  	   sprintf(szTemp,"%dx%dx%d",
  		   (int)poRootGroup->paDims[0],
  		   (int)poRootGroup->paDims[1],
  		   (int)poRootGroup->paDims[2]);
  	   strcat(szDim,szTemp);
  	   
  	   for (i=0; i<(int)poRootGroup->paDims[2]; i++){
  	     sprintf( szTemp, "SUBDATASET_%d_NAME", poDS->nSubDataCount );
  	     
  	     poDS->papszSubDatasets =
  	       CSLSetNameValue(poDS->papszSubDatasets, 
  			       szTemp,
  			       CPLSPrintf( "HDF5:\"%s\":%s:Band_%d",
  					   poDS->pszFilename,
  					   poRootGroup->pszName,i+1));
  	     
  	     sprintf( szTemp, "SUBDATASET_%d_DESC", poDS->nSubDataCount++ );
  	     
  	     
  	     poDS->papszSubDatasets =
  	       CSLSetNameValue(poDS->papszSubDatasets, 
  			       szTemp,
  			       CPLSPrintf("[%s] %s (%s)", 
  					  szDim,
  					  poRootGroup->pszName,
  					  poDS->GetDataTypeName
  					  (poRootGroup->native)));
  	     
  	   }
  	 }
  	
  	case 2: {
  	szDim[0]='\0';
  	sprintf(szTemp,"%dx%d",
  		(int)poRootGroup->paDims[0],
  		(int)poRootGroup->paDims[1]);
  	strcat(szDim,szTemp);
  	sprintf( szTemp, "SUBDATASET_%d_NAME", poDS->nSubDataCount );
  	
  	poDS->papszSubDatasets =
  	  CSLSetNameValue(poDS->papszSubDatasets, szTemp,
  			  CPLSPrintf( "HDF5:\"%s\":%s:Band_1 ",
  				      poDS->pszFilename,
  				      poRootGroup->pszName));
  	sprintf( szTemp, "SUBDATASET_%d_DESC", poDS->nSubDataCount++ );
  	
  	
  	poDS->papszSubDatasets =
  	  CSLSetNameValue(poDS->papszSubDatasets, szTemp,
  			  CPLSPrintf( "[%s] %s (%s)", 
  				      szDim,
  				      poRootGroup->pszName,
  				      poDS->GetDataTypeName
  				      (poRootGroup->native)));
  	}
      }
  }
  
  return CE_None;
}


/************************************************************************/
/*                       ReadGlobalAttributes()                         */
/************************************************************************/
CPLErr HDF5Dataset::ReadGlobalAttributes(int bSUBDATASET)
{

  HDF5Dataset *poDS;
  HDF5GroupObjects *poRootGroup;
  poDS = this;
  
  poRootGroup = new HDF5GroupObjects;
  
  poDS->poH5RootGroup=poRootGroup;
  poRootGroup->pszName   = strdup("/");
  poRootGroup->nType     = H5G_GROUP;
  poRootGroup->poHparent = NULL;
  
  if (hHDF5 < 0)  {
    printf("hHDF5 <0!!\n");
    return CE_None;
  }
  
  hGroupID = H5Gopen(hHDF5, "/"); 
  if (hGroupID < 0){
    printf("hGroupID <0!!\n");
    return CE_None;
  }
  
  poRootGroup->nbAttrs = H5Aget_num_attrs(hGroupID);
  
  H5Gget_num_objs(hGroupID, (hsize_t *) &(poRootGroup->nbObjs));
  
  if (poRootGroup->nbObjs > 0) {
    poRootGroup->poHchild = (HDF5GroupObjects *) calloc(poRootGroup->nbObjs, 
  				sizeof(HDF5GroupObjects));
    H5Giterate(hGroupID, "/", NULL, 
  	       HDF5CreateGroupObjs, (void *)poRootGroup);
  }
  else poRootGroup->poHchild = NULL;
  
  
  if(bSUBDATASET)
    poDS->HDF5ListGroupObjects(poRootGroup);
  return CE_None;
}
