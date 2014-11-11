/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  HDF5 Datasets. Open HDF5 file, fetch metadata and list of
 *           subdatasets.
 *           This driver initially based on code supplied by Markus Neteler
 * Author:  Denis Nadeau <denis.nadeau@gmail.com>
 *
 ******************************************************************************
 * Copyright (c) 2005, Frank Warmerdam <warmerdam@pobox.com>
 * Copyright (c) 2008-2013, Even Rouault <even dot rouault at mines-paris dot org>
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

#define MAX_METADATA_LEN 32768

#include "hdf5.h"

#include "gdal_priv.h"
#include "cpl_string.h"
#include "hdf5dataset.h"

CPL_CVSID("$Id$");

CPL_C_START
void GDALRegister_HDF5(void);
CPL_C_END



/************************************************************************/
/* ==================================================================== */
/*                              HDF5Dataset                             */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                        GDALRegister_HDF5()                           */
/************************************************************************/
void GDALRegister_HDF5()

{
    GDALDriver	*poDriver;
    if( GDALGetDriverByName("HDF5") == NULL )
    {
        poDriver = new GDALDriver();
        poDriver->SetDescription("HDF5");
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                                  "Hierarchical Data Format Release 5");
        poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                                  "frmt_hdf5.html");
        poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "hdf5");
        poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
        poDriver->pfnOpen = HDF5Dataset::Open;
        poDriver->pfnIdentify = HDF5Dataset::Identify;
        GetGDALDriverManager()->RegisterDriver(poDriver);
    }
}

/************************************************************************/
/*                           HDF5Dataset()                      	*/
/************************************************************************/
HDF5Dataset::HDF5Dataset()
{
    papszSubDatasets    = NULL;
    papszMetadata       = NULL;
    poH5RootGroup       = NULL;
    nSubDataCount       = 0;
    hHDF5               = -1;
    hGroupID            = -1;
    bIsHDFEOS           = FALSE;
    nDatasetType        = -1;
}

/************************************************************************/
/*                            ~HDF5Dataset()                            */
/************************************************************************/
HDF5Dataset::~HDF5Dataset()
{
    CSLDestroy( papszMetadata );
    if( hGroupID > 0 )
        H5Gclose( hGroupID );
    if( hHDF5 > 0 )
        H5Fclose( hHDF5 );
    CSLDestroy( papszSubDatasets );
    if( poH5RootGroup != NULL ) {
        DestroyH5Objects( poH5RootGroup );
        CPLFree( poH5RootGroup->pszName );
        CPLFree( poH5RootGroup->pszPath );
        CPLFree( poH5RootGroup->pszUnderscorePath );
        CPLFree( poH5RootGroup->poHchild );
        CPLFree( poH5RootGroup );
    }
}

/************************************************************************/
/*                            GetDataType()                             */
/*                                                                      */
/*      Transform HDF5 datatype to GDAL datatype                        */
/************************************************************************/
GDALDataType HDF5Dataset::GetDataType(hid_t TypeID)
{
    if( H5Tequal( H5T_NATIVE_CHAR,        TypeID ) )
        return GDT_Byte;
    else if( H5Tequal( H5T_NATIVE_SCHAR,  TypeID ) )
        return GDT_Byte;
    else if( H5Tequal( H5T_NATIVE_UCHAR,  TypeID ) )
        return GDT_Byte;
    else if( H5Tequal( H5T_NATIVE_SHORT,  TypeID ) )
        return GDT_Int16;
    else if( H5Tequal( H5T_NATIVE_USHORT, TypeID ) )
        return GDT_UInt16;
    else if( H5Tequal( H5T_NATIVE_INT,    TypeID ) )
        return GDT_Int32;
    else if( H5Tequal( H5T_NATIVE_UINT,   TypeID ) )
        return GDT_UInt32;
    else if( H5Tequal( H5T_NATIVE_LONG,   TypeID ) )
    {
        if( sizeof(long) == 4 )
            return GDT_Int32;
        else
            return GDT_Unknown;
    }
    else if( H5Tequal( H5T_NATIVE_ULONG,  TypeID ) )
    {
        if( sizeof(unsigned long) == 4 )
            return GDT_UInt32;
        else
            return GDT_Unknown;
    }
    else if( H5Tequal( H5T_NATIVE_FLOAT,  TypeID ) )
        return GDT_Float32;
    else if( H5Tequal( H5T_NATIVE_DOUBLE, TypeID ) )
        return GDT_Float64;
    else if( H5Tequal( H5T_NATIVE_LLONG,  TypeID ) )
        return GDT_Unknown;
    else if( H5Tequal( H5T_NATIVE_ULLONG, TypeID ) )
        return GDT_Unknown;
    else if( H5Tequal( H5T_NATIVE_DOUBLE, TypeID ) )
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
    if( H5Tequal( H5T_NATIVE_CHAR,        TypeID ) )
        return "8-bit character";
    else if( H5Tequal( H5T_NATIVE_SCHAR,  TypeID ) )
        return "8-bit signed character";
    else if( H5Tequal( H5T_NATIVE_UCHAR,  TypeID ) )
        return "8-bit unsigned character";
    else if( H5Tequal( H5T_NATIVE_SHORT,  TypeID ) )
        return "16-bit integer";
    else if( H5Tequal( H5T_NATIVE_USHORT, TypeID ) )
        return "16-bit unsigned integer";
    else if( H5Tequal( H5T_NATIVE_INT,    TypeID ) )
        return "32-bit integer";
    else if( H5Tequal( H5T_NATIVE_UINT,   TypeID ) )
        return "32-bit unsigned integer";
    else if( H5Tequal( H5T_NATIVE_LONG,   TypeID ) )
        return "32/64-bit integer";
    else if( H5Tequal( H5T_NATIVE_ULONG,  TypeID ) )
        return "32/64-bit unsigned integer";
    else if( H5Tequal( H5T_NATIVE_FLOAT,  TypeID ) )
        return "32-bit floating-point";
    else if( H5Tequal( H5T_NATIVE_DOUBLE, TypeID ) )
        return "64-bit floating-point";
    else if( H5Tequal( H5T_NATIVE_LLONG,  TypeID ) )
        return "64-bit integer";
    else if( H5Tequal( H5T_NATIVE_ULLONG, TypeID ) )
        return "64-bit unsigned integer";
    else if( H5Tequal( H5T_NATIVE_DOUBLE, TypeID ) )
        return "64-bit floating-point";

    return "Unknown";
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int HDF5Dataset::Identify( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Is it an HDF5 file?                                             */
/* -------------------------------------------------------------------- */
    static const char achSignature[] = "\211HDF\r\n\032\n";

    if( poOpenInfo->pabyHeader )
    {
        if( memcmp(poOpenInfo->pabyHeader,achSignature,8) == 0 )
            return TRUE;

        if( memcmp(poOpenInfo->pabyHeader,"<HDF_UserBlock>",15) == 0)
        {
            if( H5Fis_hdf5(poOpenInfo->pszFilename) )
              return TRUE;
        }
    }

    return FALSE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/
GDALDataset *HDF5Dataset::Open( GDALOpenInfo * poOpenInfo )
{
    HDF5Dataset *poDS;

    if( !Identify( poOpenInfo ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Create datasource.                                              */
/* -------------------------------------------------------------------- */
    poDS = new HDF5Dataset();

    poDS->SetDescription( poOpenInfo->pszFilename );

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    poDS->hHDF5 = H5Fopen( poOpenInfo->pszFilename,
                           H5F_ACC_RDONLY,
                           H5P_DEFAULT );
    if( poDS->hHDF5 < 0 )  {
        delete poDS;
        return NULL;
    }

    poDS->hGroupID = H5Gopen( poDS->hHDF5, "/" );
    if( poDS->hGroupID < 0 ) {
        poDS->bIsHDFEOS=false;
        delete poDS;
        return NULL;
    }

    poDS->bIsHDFEOS=true;
    poDS->ReadGlobalAttributes( true );

    poDS->SetMetadata( poDS->papszMetadata  );

    if ( CSLCount( poDS->papszSubDatasets ) / 2 >= 1 )
        poDS->SetMetadata( poDS->papszSubDatasets, "SUBDATASETS" );

    // Make sure we don't try to do any pam stuff with this dataset.
    poDS->nPamFlags |= GPF_NOSAVE;

/* -------------------------------------------------------------------- */
/*      If we have single subdataset only, open it immediately          */
/* -------------------------------------------------------------------- */
    int nSubDatasets = CSLCount( poDS->papszSubDatasets ) / 2;
    if( nSubDatasets == 1 )
    {
        CPLString osDSName = CSLFetchNameValue( poDS->papszSubDatasets,
                                                "SUBDATASET_1_NAME" );
        delete poDS;
        return (GDALDataset *) GDALOpen( osDSName, poOpenInfo->eAccess );
    }
    else
    {
/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
        if( poOpenInfo->eAccess == GA_Update )
        {
            delete poDS;
            CPLError( CE_Failure, CPLE_NotSupported,
                      "The HDF5 driver does not support update access to existing"
                      " datasets.\n" );
            return NULL;
        }
    }
    return( poDS );
}

/************************************************************************/
/*                          DestroyH5Objects()                          */
/*                                                                      */
/*      Erase all objects                                               */
/************************************************************************/
void HDF5Dataset::DestroyH5Objects( HDF5GroupObjects *poH5Object )
{
    unsigned i;

/* -------------------------------------------------------------------- */
/*      Visit all objects                                               */
/* -------------------------------------------------------------------- */

    for( i=0; i < poH5Object->nbObjs; i++ )
        if( poH5Object->poHchild+i != NULL )
            DestroyH5Objects( poH5Object->poHchild+i );

    if( poH5Object->poHparent ==NULL )
        return;

/* -------------------------------------------------------------------- */
/*      Erase some data                                                 */
/* -------------------------------------------------------------------- */
    CPLFree( poH5Object->paDims );
    poH5Object->paDims = NULL;

    CPLFree( poH5Object->pszPath );
    poH5Object->pszPath = NULL;

    CPLFree( poH5Object->pszName );
    poH5Object->pszName = NULL;

    CPLFree( poH5Object->pszUnderscorePath );
    poH5Object->pszUnderscorePath = NULL;
    
    if( poH5Object->native > 0 )
        H5Tclose( poH5Object->native );
    poH5Object->native = 0;

/* -------------------------------------------------------------------- */
/*      All Children are visited and can be deleted.                    */
/* -------------------------------------------------------------------- */
    if( ( i==poH5Object->nbObjs ) && ( poH5Object->nbObjs!=0 ) ) {
        CPLFree( poH5Object->poHchild );
        poH5Object->poHchild = NULL;
    }

}

/************************************************************************/
/*                             CreatePath()                             */
/*                                                                      */
/*      Find Dataset path for HDopen                                    */
/************************************************************************/
char* CreatePath( HDF5GroupObjects *poH5Object )
{
    char pszPath[8192];
    char pszUnderscoreSpaceInName[8192];
    char *popszPath;
    int  i;
    char **papszPath;

/* -------------------------------------------------------------------- */
/*      Recurse to the root path                                        */
/* -------------------------------------------------------------------- */
    pszPath[0]='\0';
    if( poH5Object->poHparent !=NULL ) {
        popszPath=CreatePath( poH5Object->poHparent );
        strcpy( pszPath,popszPath );
    }

/* -------------------------------------------------------------------- */
/*      add name to the path                                            */
/* -------------------------------------------------------------------- */
    if( !EQUAL( poH5Object->pszName,"/" ) ){
        strcat( pszPath,"/" );
        strcat( pszPath,poH5Object->pszName );
    }

/* -------------------------------------------------------------------- */
/*      fill up path for each object                                    */
/* -------------------------------------------------------------------- */
    if( poH5Object->pszPath == NULL ) {

        if( strlen( poH5Object->pszName ) == 1 ) {
            strcat(pszPath, poH5Object->pszName );
            strcpy(pszUnderscoreSpaceInName, poH5Object->pszName);
        }
        else {
/* -------------------------------------------------------------------- */
/*      Change space for underscore                                     */
/* -------------------------------------------------------------------- */
            papszPath = CSLTokenizeString2( pszPath,
                            " ", CSLT_HONOURSTRINGS );

            strcpy(pszUnderscoreSpaceInName,papszPath[0]);
            for( i=1; i < CSLCount( papszPath ); i++ ) {
                strcat( pszUnderscoreSpaceInName, "_" );
                strcat( pszUnderscoreSpaceInName, papszPath[ i ] );
            }
            CSLDestroy(papszPath);

        }
        poH5Object->pszUnderscorePath  =
            CPLStrdup( pszUnderscoreSpaceInName );
        poH5Object->pszPath  = CPLStrdup( pszPath );
    }

    return( poH5Object->pszPath );
}

/************************************************************************/
/*                      HDF5GroupCheckDuplicate()                       */
/*                                                                      */
/*      Returns TRUE if an ancestor has the same objno[] as passed      */
/*      in - used to avoid looping in files with "links up" #(3218).    */
/************************************************************************/

static int HDF5GroupCheckDuplicate( HDF5GroupObjects *poHparent,
                                    unsigned long *objno )

{
    while( poHparent != NULL )
    {
        if( poHparent->objno[0] == objno[0]
            && poHparent->objno[1] == objno[1] )
            return TRUE;

        poHparent = poHparent->poHparent;
    }

    return FALSE;
}

/************************************************************************/
/*                      HDF5CreateGroupObjs()                           */
/*                                                                      */
/*      Create HDF5 hierarchy into a linked list                        */
/************************************************************************/
herr_t HDF5CreateGroupObjs(hid_t hHDF5, const char *pszObjName,
                           void *poHObjParent)
{
    hid_t       hGroupID;       /* identifier of group */
    hid_t       hDatasetID;     /* identifier of dataset */
    hsize_t     nbObjs=0;       /* number of objects in a group */
    int         nbAttrs=0;      /* number of attributes in object */
    unsigned    idx;
    int         n_dims;
    H5G_stat_t  oStatbuf;
    hsize_t     *dims=NULL;
    hsize_t     *maxdims=NULL;
    hid_t       datatype;
    hid_t       dataspace;
    hid_t       native;

    HDF5GroupObjects *poHchild;
    HDF5GroupObjects *poHparent;

    poHparent = ( HDF5GroupObjects * ) poHObjParent;
    poHchild=poHparent->poHchild;

    if( H5Gget_objinfo( hHDF5, pszObjName, FALSE, &oStatbuf ) < 0  )
        return -1;


/* -------------------------------------------------------------------- */
/*      Look for next child                                             */
/* -------------------------------------------------------------------- */
    for( idx=0; idx < poHparent->nbObjs; idx++ ) {
        if( poHchild->pszName == NULL ) break;
        poHchild++;
    }

    if( idx == poHparent->nbObjs )
        return -1;  // all children parsed

/* -------------------------------------------------------------------- */
/*      Save child information                                          */
/* -------------------------------------------------------------------- */
    poHchild->pszName  = CPLStrdup( pszObjName );

    poHchild->nType  = oStatbuf.type;
    poHchild->nIndex = idx;
    poHchild->poHparent = poHparent;
    poHchild->nRank     = 0;
    poHchild->paDims    = 0;
    poHchild->HDatatype = 0;
    poHchild->objno[0]  = oStatbuf.objno[0];
    poHchild->objno[1]  = oStatbuf.objno[1];
    if( poHchild->pszPath == NULL ) {
        poHchild->pszPath  = CreatePath( poHchild );
    }
    if( poHparent->pszPath == NULL ) {
        poHparent->pszPath = CreatePath( poHparent );
    }


    switch ( oStatbuf.type )
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
            if( ( hGroupID = H5Gopen( hHDF5, pszObjName ) ) == -1  ) {
                printf( "Error: unable to access \"%s\" group.\n",
                        pszObjName );
                return -1;
            }
            nbAttrs          = H5Aget_num_attrs( hGroupID );
            H5Gget_num_objs( hGroupID, &nbObjs );
            poHchild->nbAttrs= nbAttrs;
            poHchild->nbObjs = (int) nbObjs;
            poHchild->nRank      = 0;
            poHchild->paDims    = 0;
            poHchild->HDatatype = 0;

            if( nbObjs > 0 ) {
                poHchild->poHchild =( HDF5GroupObjects * )
                CPLCalloc( (int)nbObjs, sizeof( HDF5GroupObjects ) );
                memset( poHchild->poHchild, 0,
                        (size_t) (sizeof( HDF5GroupObjects ) * nbObjs) );
            }
            else
                poHchild->poHchild = NULL;

            if( !HDF5GroupCheckDuplicate( poHparent, oStatbuf.objno ) )
                H5Giterate( hHDF5, pszObjName, NULL,
                            HDF5CreateGroupObjs,  (void*) poHchild );
            else
                CPLDebug( "HDF5", "avoiding link looping on node '%s'.",
                        pszObjName );

            H5Gclose( hGroupID );
            break;

        case H5G_DATASET:

            if( ( hDatasetID = H5Dopen( hHDF5, pszObjName ) ) == -1  ) {
                printf( "Error: unable to access \"%s\" dataset.\n",
                        pszObjName );
                return -1;
            }
            nbAttrs      = H5Aget_num_attrs( hDatasetID );
            datatype     = H5Dget_type( hDatasetID );
            dataspace    = H5Dget_space( hDatasetID );
            n_dims       = H5Sget_simple_extent_ndims( dataspace );
            native       = H5Tget_native_type( datatype, H5T_DIR_ASCEND );

            if( n_dims > 0 ) {
                dims     = (hsize_t *) CPLCalloc( n_dims,sizeof( hsize_t ) );
                maxdims  = (hsize_t *) CPLCalloc( n_dims,sizeof( hsize_t ) );
            }
            H5Sget_simple_extent_dims( dataspace, dims, maxdims );
            if( maxdims != NULL )
                CPLFree( maxdims );

            if( n_dims > 0 ) {
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
            H5Tclose( datatype );
            H5Sclose( dataspace );
            H5Dclose( hDatasetID );
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
/*                          HDF5AttrIterate()                           */
/************************************************************************/

herr_t HDF5AttrIterate( hid_t hH5ObjID,
                        const char *pszAttrName,
                        void *pDS )
{
    hid_t           hAttrID;
    hid_t           hAttrTypeID;
    hid_t           hAttrNativeType;
    hid_t           hAttrSpace;

    char           *szData = NULL;
    hsize_t        nSize[64];
    unsigned int   nAttrElmts;
    hsize_t        nAttrSize;
    hsize_t        i;
    void           *buf = NULL;
    unsigned int   nAttrDims;

    char          **papszTokens;

    HDF5Dataset    *poDS;
    CPLString       osKey;
    char           *szValue = NULL;

    poDS = (HDF5Dataset *) pDS;

    // Convert "/" into "_" for the path component
    const char* pszPath = poDS->poH5CurrentObject->pszUnderscorePath;
    if(pszPath != NULL && strlen(pszPath) > 0)
    {
        papszTokens = CSLTokenizeString2( pszPath, "/", CSLT_HONOURSTRINGS );

        for( i = 0; papszTokens != NULL && papszTokens[i] != NULL; ++i )
        {
            if( i != 0)
                osKey += '_';
            osKey += papszTokens[i];
        }
        CSLDestroy( papszTokens );
    }

    // Convert whitespaces into "_" for the attribute name component
    papszTokens = CSLTokenizeString2( pszAttrName, " ",
                            CSLT_STRIPLEADSPACES | CSLT_STRIPENDSPACES );
    for( i = 0; papszTokens != NULL && papszTokens[i] != NULL; ++i )
    {
        if(!osKey.empty())
            osKey += '_';
        osKey += papszTokens[i];
    }
    CSLDestroy( papszTokens );

    hAttrID          = H5Aopen_name( hH5ObjID, pszAttrName );
    hAttrTypeID      = H5Aget_type( hAttrID );
    hAttrNativeType  = H5Tget_native_type( hAttrTypeID, H5T_DIR_DEFAULT );
    hAttrSpace       = H5Aget_space( hAttrID );
    nAttrDims        = H5Sget_simple_extent_dims( hAttrSpace, nSize, NULL );

    nAttrElmts = 1;
    for( i=0; i < nAttrDims; i++ ) {
        nAttrElmts *= (int) nSize[i];
    }

    if( H5Tget_class( hAttrNativeType ) == H5T_STRING )
    {
        if ( H5Tis_variable_str(hAttrNativeType) )
        {
            char** papszStrings;
            papszStrings = (char**) CPLMalloc( nAttrElmts * sizeof(char*) );

            // Read the values
            H5Aread( hAttrID, hAttrNativeType, papszStrings );

            // Concatenate all values as one string (separated by a space)
            CPLString osVal = papszStrings[0];
            for( i=1; i < nAttrElmts; i++ ) {
                osVal += " ";
                osVal += papszStrings[i];
            }

            szValue = (char*) CPLMalloc(osVal.length() + 1);
            strcpy( szValue, osVal.c_str() );

            H5Dvlen_reclaim( hAttrNativeType, hAttrSpace, H5P_DEFAULT,
                             papszStrings );
            CPLFree( papszStrings );
        }
        else
        {
            nAttrSize = H5Aget_storage_size( hAttrID );
            szValue = (char*) CPLMalloc((size_t) (nAttrSize+1));
            H5Aread( hAttrID, hAttrNativeType, szValue );
            szValue[nAttrSize] = '\0';
        }
    }
    else {
        if( nAttrElmts > 0 ) {
            buf = (void *) CPLMalloc( nAttrElmts*
                          H5Tget_size( hAttrNativeType ));
            szData = (char*) CPLMalloc( 8192 );
            szValue = (char*) CPLMalloc( MAX_METADATA_LEN );
            szData[0] = '\0';
            szValue[0] ='\0';
            H5Aread( hAttrID, hAttrNativeType, buf );
        }
        if( H5Tequal( H5T_NATIVE_CHAR, hAttrNativeType ) 
            || H5Tequal( H5T_NATIVE_SCHAR,  hAttrNativeType ) ) {
            for( i=0; i < nAttrElmts; i++ ) {
                sprintf( szData, "%c ", ((char *) buf)[i]);
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                                                            MAX_METADATA_LEN )
                    CPLError( CE_Warning, CPLE_OutOfMemory,
                              "Header data too long. Truncated\n");
            }
        }
        else if( H5Tequal( H5T_NATIVE_UCHAR,  hAttrNativeType ) ) {
            for( i=0; i < nAttrElmts; i++ ) {
                sprintf( szData, "%c", ((char *) buf)[i] );
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                                                            MAX_METADATA_LEN )
                    CPLError( CE_Warning, CPLE_OutOfMemory,
                              "Header data too long. Truncated\n");
            }
        }
        else if( H5Tequal( H5T_NATIVE_SHORT,  hAttrNativeType ) ) {
            for( i=0; i < nAttrElmts; i++ ) {
                sprintf( szData, "%d ", ((short *) buf)[i] );
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                                                            MAX_METADATA_LEN )
                    CPLError( CE_Warning, CPLE_OutOfMemory,
                              "Header data too long. Truncated\n");
            }
        }
        else if( H5Tequal( H5T_NATIVE_USHORT, hAttrNativeType ) ) {
            for( i=0; i < nAttrElmts; i++ ) {
                sprintf( szData, "%ud ", ((unsigned short *) buf)[i] );
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                                                            MAX_METADATA_LEN )
                    CPLError( CE_Warning, CPLE_OutOfMemory,
                              "Header data too long. Truncated\n");
            }
        }
        else if( H5Tequal( H5T_NATIVE_INT,    hAttrNativeType ) ) {
            for( i=0; i < nAttrElmts; i++ ) {
                sprintf( szData, "%d ", ((int *) buf)[i] );
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                                                            MAX_METADATA_LEN )
                    CPLError( CE_Warning, CPLE_OutOfMemory,
                              "Header data too long. Truncated\n");
            }
        }
        else if( H5Tequal( H5T_NATIVE_UINT,   hAttrNativeType ) ) {
            for( i=0; i < nAttrElmts; i++ ) {
                sprintf( szData, "%ud ", ((unsigned int *) buf)[i] );
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                                                            MAX_METADATA_LEN )
                    CPLError( CE_Warning, CPLE_OutOfMemory,
                              "Header data too long. Truncated\n");
            }
        }
        else if( H5Tequal( H5T_NATIVE_LONG,   hAttrNativeType ) ) {
            for( i=0; i < nAttrElmts; i++ ) {
                sprintf( szData, "%ld ", ((long *)buf)[i] );
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                                                            MAX_METADATA_LEN )
                    CPLError( CE_Warning, CPLE_OutOfMemory,
                              "Header data too long. Truncated\n");
            }
        }
        else if( H5Tequal( H5T_NATIVE_ULONG,  hAttrNativeType ) ) {
            for( i=0; i < nAttrElmts; i++ ) {
                sprintf( szData, "%ld ", ((unsigned long *)buf)[i] );
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                                                            MAX_METADATA_LEN )
                    CPLError( CE_Warning, CPLE_OutOfMemory,
                              "Header data too long. Truncated\n");
            }
        }
        else if( H5Tequal( H5T_NATIVE_FLOAT,  hAttrNativeType ) ) {
            for( i=0; i < nAttrElmts; i++ ) {
                CPLsprintf( szData, "%.8g ",  ((float *)buf)[i] );
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                                                            MAX_METADATA_LEN )
                    CPLError( CE_Warning, CPLE_OutOfMemory,
                              "Header data too long. Truncated\n");
            }
        }
        else if( H5Tequal( H5T_NATIVE_DOUBLE, hAttrNativeType ) ) {
            for( i=0; i < nAttrElmts; i++ ) {
                CPLsprintf( szData, "%.15g ",  ((double *)buf)[i] );
                if( CPLStrlcat(szValue, szData, MAX_METADATA_LEN) >=
                                                            MAX_METADATA_LEN )
                    CPLError( CE_Warning, CPLE_OutOfMemory,
                              "Header data too long. Truncated\n");
            }
        }
        CPLFree( buf );

    }
    H5Sclose(hAttrSpace);
    H5Tclose(hAttrNativeType);
    H5Tclose(hAttrTypeID);
    H5Aclose( hAttrID );
    poDS->papszMetadata = CSLSetNameValue( poDS->papszMetadata, osKey, szValue);

    CPLFree( szData );
    CPLFree( szValue );

    return 0;
}

/************************************************************************/
/*                           CreateMetadata()                           */
/************************************************************************/
CPLErr HDF5Dataset::CreateMetadata( HDF5GroupObjects *poH5Object, int nType)
{
    hid_t       hGroupID;       /* identifier of group */
    hid_t       hDatasetID;
    int         nbAttrs;

    HDF5Dataset *poDS;

    if( !poH5Object->pszPath )
        return CE_None;

    poDS = this;

    poH5CurrentObject = poH5Object;
    nbAttrs = poH5Object->nbAttrs;

    if( poH5Object->pszPath == NULL || EQUAL(poH5Object->pszPath, "" ) )
        return CE_None;

    switch( nType ) {

    case H5G_GROUP:

        if( nbAttrs > 0 ) {
            hGroupID = H5Gopen( hHDF5, poH5Object->pszPath );
            H5Aiterate( hGroupID, NULL, HDF5AttrIterate, (void *)poDS  );
            H5Gclose( hGroupID );
        }

        break;

    case H5G_DATASET:

        if( nbAttrs > 0 ) {
            hDatasetID =  H5Dopen(hHDF5, poH5Object->pszPath );
            H5Aiterate( hDatasetID, NULL, HDF5AttrIterate, (void *)poDS );
            H5Dclose( hDatasetID );
        }
        break;

    default:
        break;
    }

    return CE_None;
}


/************************************************************************/
/*                       HDF5FindDatasetObjectsbyPath()                 */
/*      Find object by name                                             */
/************************************************************************/
HDF5GroupObjects* HDF5Dataset::HDF5FindDatasetObjectsbyPath
    ( HDF5GroupObjects *poH5Objects, const char* pszDatasetPath )
{
    unsigned i;
    HDF5Dataset *poDS;
    HDF5GroupObjects *poObjectsFound;
    poDS=this;

    if( poH5Objects->nType == H5G_DATASET &&
        EQUAL( poH5Objects->pszUnderscorePath,pszDatasetPath ) ) {

        /*      printf("found it! %ld\n",(long) poH5Objects);*/
        return( poH5Objects );
    }

    if( poH5Objects->nbObjs >0 )
        for( i=0; i <poH5Objects->nbObjs; i++ )   {
            poObjectsFound=
            poDS->HDF5FindDatasetObjectsbyPath( poH5Objects->poHchild+i,
                                                pszDatasetPath );
/* -------------------------------------------------------------------- */
/*      Is this our dataset??                                           */
/* -------------------------------------------------------------------- */
            if( poObjectsFound != NULL ) return( poObjectsFound );
        }
/* -------------------------------------------------------------------- */
/*      Dataset has not been found!                                     */
/* -------------------------------------------------------------------- */
    return( NULL );

}


/************************************************************************/
/*                       HDF5FindDatasetObjects()                       */
/*      Find object by name                                             */
/************************************************************************/
HDF5GroupObjects* HDF5Dataset::HDF5FindDatasetObjects
    ( HDF5GroupObjects *poH5Objects, const char* pszDatasetName )
{
    unsigned i;
    HDF5Dataset *poDS;
    HDF5GroupObjects *poObjectsFound;
    poDS=this;

    if( poH5Objects->nType == H5G_DATASET &&
        EQUAL( poH5Objects->pszName,pszDatasetName ) ) {

        /*      printf("found it! %ld\n",(long) poH5Objects);*/
        return( poH5Objects );
    }

    if( poH5Objects->nbObjs >0 )
        for( i=0; i <poH5Objects->nbObjs; i++ )   {
            poObjectsFound=
            poDS->HDF5FindDatasetObjects( poH5Objects->poHchild+i,
                                          pszDatasetName );
/* -------------------------------------------------------------------- */
/*      Is this our dataset??                                           */
/* -------------------------------------------------------------------- */
            if( poObjectsFound != NULL ) return( poObjectsFound );

        }
/* -------------------------------------------------------------------- */
/*      Dataset has not been found!                                     */
/* -------------------------------------------------------------------- */
    return( NULL );

}


/************************************************************************/
/*                        HDF5ListGroupObjects()                        */
/*                                                                      */
/*      List all objects in HDF5                                        */
/************************************************************************/
CPLErr HDF5Dataset::HDF5ListGroupObjects( HDF5GroupObjects *poRootGroup,
					  int bSUBDATASET )
{
    char szTemp[8192];
    char szDim[8192];
    HDF5Dataset *poDS;
    poDS=this;

    if( poRootGroup->nbObjs >0 )
        for( hsize_t i=0; i < poRootGroup->nbObjs; i++ ) {
            poDS->HDF5ListGroupObjects( poRootGroup->poHchild+i, bSUBDATASET );
        }


    if( poRootGroup->nType == H5G_GROUP ) {
        CreateMetadata( poRootGroup, H5G_GROUP );
    }

/* -------------------------------------------------------------------- */
/*      Create Sub dataset list                                         */
/* -------------------------------------------------------------------- */
    if( (poRootGroup->nType == H5G_DATASET ) && bSUBDATASET
        && poDS->GetDataType( poRootGroup->native ) == GDT_Unknown )
    {
        CPLDebug( "HDF5", "Skipping unsupported %s of type %s",
                  poRootGroup->pszUnderscorePath,
                  poDS->GetDataTypeName( poRootGroup->native ) );
    }
    else if( (poRootGroup->nType == H5G_DATASET ) && bSUBDATASET )
    {
        CreateMetadata( poRootGroup, H5G_DATASET );

        szDim[0]='\0';
        switch( poRootGroup->nRank ) {
        case 3:
            sprintf( szTemp,"%dx%dx%d",
                (int)poRootGroup->paDims[0],
                (int)poRootGroup->paDims[1],
                (int)poRootGroup->paDims[2] );
            break;

        case 2:
            sprintf( szTemp,"%dx%d",
                (int)poRootGroup->paDims[0],
                (int)poRootGroup->paDims[1] );
            break;

        default:
            return CE_None;

        }
        strcat( szDim,szTemp );

        sprintf( szTemp, "SUBDATASET_%d_NAME", ++(poDS->nSubDataCount) );

        poDS->papszSubDatasets =
            CSLSetNameValue( poDS->papszSubDatasets, szTemp,
                    CPLSPrintf( "HDF5:\"%s\":%s",
                        poDS->GetDescription(),
                        poRootGroup->pszUnderscorePath ) );

        sprintf(  szTemp, "SUBDATASET_%d_DESC", poDS->nSubDataCount );

        poDS->papszSubDatasets =
            CSLSetNameValue( poDS->papszSubDatasets, szTemp,
                    CPLSPrintf( "[%s] %s (%s)",
                        szDim,
                        poRootGroup->pszUnderscorePath,
                        poDS->GetDataTypeName
                        ( poRootGroup->native ) ) );

    }

    return CE_None;
}


/************************************************************************/
/*                       ReadGlobalAttributes()                         */
/************************************************************************/
CPLErr HDF5Dataset::ReadGlobalAttributes(int bSUBDATASET)
{

    HDF5GroupObjects *poRootGroup;

    poRootGroup = (HDF5GroupObjects*) CPLCalloc(sizeof(HDF5GroupObjects), 1);

    poH5RootGroup=poRootGroup;
    poRootGroup->pszName   = CPLStrdup( "/" );
    poRootGroup->nType     = H5G_GROUP;
    poRootGroup->poHparent = NULL;
    poRootGroup->pszPath = NULL;
    poRootGroup->pszUnderscorePath = NULL;

    if( hHDF5 < 0 )  {
        printf( "hHDF5 <0!!\n" );
        return CE_None;
    }

    H5G_stat_t  oStatbuf;
    if( H5Gget_objinfo( hHDF5, "/", FALSE, &oStatbuf ) < 0  )
        return CE_Failure;
    poRootGroup->objno[0] = oStatbuf.objno[0];
    poRootGroup->objno[1] = oStatbuf.objno[1];

    if( hGroupID > 0 )
        H5Gclose( hGroupID );
    hGroupID = H5Gopen( hHDF5, "/" );
    if( hGroupID < 0 ){
        printf( "hGroupID <0!!\n" );
        return CE_None;
    }

    poRootGroup->nbAttrs = H5Aget_num_attrs( hGroupID );

    H5Gget_num_objs( hGroupID, &( poRootGroup->nbObjs ) );

    if( poRootGroup->nbObjs > 0 ) {
        poRootGroup->poHchild = ( HDF5GroupObjects * )
            CPLCalloc( poRootGroup->nbObjs,
            sizeof( HDF5GroupObjects ) );
        H5Giterate( hGroupID, "/", NULL,
               HDF5CreateGroupObjs, (void *)poRootGroup );
    }
    else poRootGroup->poHchild = NULL;

    HDF5ListGroupObjects( poRootGroup, bSUBDATASET );
    return CE_None;
}


/**
 * Reads an array of double attributes from the HDF5 metadata.
 * It reads the attributes directly on it's binary form directly,
 * thus avoiding string conversions.
 *
 * Important: It allocates the memory for the attributes internally,
 * so the caller must free the returned array after using it.
 * @param pszAttrName Name of the attribute to be read.
 *        the attribute name must be the form:
 *            root attribute name
 *            SUBDATASET/subdataset attribute name
 * @param pdfValues pointer wich will store the array of doubles read.
 * @param nLen it stores the length of the array read. If NULL it doesn't 
 *        inform the lenght of the array.
 * @return CPLErr CE_None in case of success, CE_Failure in case of failure
 */
CPLErr HDF5Dataset::HDF5ReadDoubleAttr(const char* pszAttrFullPath,
                                       double **pdfValues,int *nLen)
{
    CPLErr          retVal = CE_Failure;
    hid_t           hAttrID=-1;
    hid_t           hAttrTypeID=-1;
    hid_t           hAttrNativeType=-1;
    hid_t           hAttrSpace=-1;
    hid_t           hObjAttrID=-1;

    hsize_t         nSize[64];
    unsigned int    nAttrElmts;
    hsize_t         i;
    unsigned int    nAttrDims;

    size_t nSlashPos;

    CPLString osAttrFullPath(pszAttrFullPath);
    CPLString osObjName;
    CPLString osAttrName;

    //Search for the last "/" in order to get the
    //Path to the attribute
    nSlashPos = osAttrFullPath.find_last_of("/");

    //If objects name have been found
    if(nSlashPos != CPLString::npos )
    {
        //Split Object name (dataset, group)
        osObjName = osAttrFullPath.substr(0,nSlashPos);
        //Split attribute name
        osAttrName = osAttrFullPath.substr(nSlashPos+1);
    }
    else
    {
        //By default the group is root, and
        //the attribute is the full path
        osObjName = "/";
        osAttrName = pszAttrFullPath;
    }

    hObjAttrID = H5Oopen( hHDF5, osObjName.c_str(),H5P_DEFAULT);

    if(hObjAttrID < 0)
    {
        CPLError( CE_Failure, CPLE_OpenFailed,
                  "Object %s could not be opened\n", pszAttrFullPath);
        retVal = CE_Failure;
    }
    else
    {
        //Open attribute handler by name, from the object handler opened
        //earlier
        hAttrID = H5Aopen_name( hObjAttrID, osAttrName.c_str());

        //Check for errors opening the attribute
        if(hAttrID <0)
        {
            CPLError( CE_Failure, CPLE_OpenFailed,
                      "Attribute %s could not be opened\n", pszAttrFullPath);
            retVal = CE_Failure;
        }
        else
        {
            hAttrTypeID      = H5Aget_type( hAttrID );
            hAttrNativeType  = H5Tget_native_type( hAttrTypeID, H5T_DIR_DEFAULT );
            hAttrSpace       = H5Aget_space( hAttrID );
            nAttrDims        = H5Sget_simple_extent_dims( hAttrSpace, nSize, NULL );

            if( !H5Tequal( H5T_NATIVE_DOUBLE, hAttrNativeType ) )
            {
                 CPLError( CE_Failure, CPLE_OpenFailed,
                                 "Attribute %s is not of type double\n", pszAttrFullPath);
                 retVal = CE_Failure;
            }
            else
            {
                //Get the ammount of elements
                nAttrElmts = 1;
                for( i=0; i < nAttrDims; i++ )
                {
                    //For multidimensional attributes
                     nAttrElmts *= nSize[i];
                }

                if(nLen != NULL)
                    *nLen = nAttrElmts;

                (*pdfValues) = (double *) CPLMalloc(nAttrElmts*sizeof(double));

                //Read the attribute contents
                if(H5Aread( hAttrID, hAttrNativeType, *pdfValues )<0)
                {
                     CPLError( CE_Failure, CPLE_OpenFailed,
                               "Attribute %s could not be opened\n", 
                               pszAttrFullPath);
                     retVal = CE_Failure;
                }
                else
                {
                    retVal = CE_None;
                }
            }

            H5Tclose(hAttrNativeType);
            H5Tclose(hAttrTypeID);
            H5Sclose(hAttrSpace);
            H5Aclose(hAttrID);
        }
        H5Oclose(hObjAttrID);
    }

    return retVal;
}
