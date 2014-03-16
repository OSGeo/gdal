/******************************************************************************
 * $Id$
 *
 * Project:  Hierarchical Data Format Release 5 (HDF5)
 * Purpose:  Read BAG datasets.
 * Author:   Frank Warmerdam <warmerdam@pobox.com>
 *
 ******************************************************************************
 * Copyright (c) 2009, Frank Warmerdam <warmerdam@pobox.com>
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

#include "gh5_convenience.h"

#include "gdal_pam.h"
#include "gdal_priv.h"
#include "ogr_spatialref.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

CPL_C_START
void    GDALRegister_BAG(void);
CPL_C_END

OGRErr OGR_SRS_ImportFromISO19115( OGRSpatialReference *poThis, 
                                   const char *pszISOXML );

/************************************************************************/
/* ==================================================================== */
/*                               BAGDataset                             */
/* ==================================================================== */
/************************************************************************/
class BAGDataset : public GDALPamDataset
{

    friend class BAGRasterBand;

    hid_t        hHDF5;

    char        *pszProjection;
    double       adfGeoTransform[6];

    void         LoadMetadata();

    char        *pszXMLMetadata;
    char        *apszMDList[2];
    
public:
    BAGDataset();
    ~BAGDataset();
    
    virtual CPLErr GetGeoTransform( double * );
    virtual const char *GetProjectionRef(void);
    virtual char      **GetMetadataDomainList();
    virtual char      **GetMetadata( const char * pszDomain = "" );

    static GDALDataset  *Open( GDALOpenInfo * );
    static int          Identify( GDALOpenInfo * );

    OGRErr ParseWKTFromXML( const char *pszISOXML );
};

/************************************************************************/
/* ==================================================================== */
/*                               BAGRasterBand                          */
/* ==================================================================== */
/************************************************************************/
class BAGRasterBand : public GDALPamRasterBand
{
    friend class BAGDataset;

    hid_t       hDatasetID;
    hid_t       native;
    hid_t       dataspace;

    bool        bMinMaxSet;
    double      dfMinimum;
    double      dfMaximum;

public:
  
    BAGRasterBand( BAGDataset *, int );
    ~BAGRasterBand();

    bool                    Initialize( hid_t hDataset, const char *pszName );

    virtual CPLErr          IReadBlock( int, int, void * );
    virtual double	    GetNoDataValue( int * ); 

    virtual double GetMinimum( int *pbSuccess = NULL );
    virtual double GetMaximum(int *pbSuccess = NULL );
};

/************************************************************************/
/*                           BAGRasterBand()                            */
/************************************************************************/
BAGRasterBand::BAGRasterBand( BAGDataset *poDS, int nBand )

{
    this->poDS       = poDS;
    this->nBand      = nBand;
    
    hDatasetID = -1;
    dataspace = -1;
    native = -1;
    bMinMaxSet = false;
}

/************************************************************************/
/*                           ~BAGRasterBand()                           */
/************************************************************************/

BAGRasterBand::~BAGRasterBand()
{
  if( dataspace > 0 )
    H5Sclose(dataspace);

  if( native > 0 )
    H5Tclose( native );

  if( hDatasetID > 0 )
    H5Dclose( hDatasetID );
}

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

bool BAGRasterBand::Initialize( hid_t hDatasetID, const char *pszName )

{
    SetDescription( pszName );

    this->hDatasetID = hDatasetID;

    hid_t datatype     = H5Dget_type( hDatasetID );
    dataspace          = H5Dget_space( hDatasetID );
    int n_dims         = H5Sget_simple_extent_ndims( dataspace );
    native             = H5Tget_native_type( datatype, H5T_DIR_ASCEND );
    hsize_t dims[3], maxdims[3];

    eDataType = GH5_GetDataType( native );

    if( n_dims == 2 )
    {
        H5Sget_simple_extent_dims( dataspace, dims, maxdims );

        nRasterXSize = (int) dims[1];
        nRasterYSize = (int) dims[0];
    }
    else
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Dataset not of rank 2." );
        return false;
    }

    nBlockXSize   = nRasterXSize;
    nBlockYSize   = 1;

/* -------------------------------------------------------------------- */
/*      Check for chunksize, and use it as blocksize for optimized      */
/*      reading.                                                        */
/* -------------------------------------------------------------------- */
    hid_t listid = H5Dget_create_plist( hDatasetID );
    if (listid>0)
    {
        if(H5Pget_layout(listid) == H5D_CHUNKED)
        {
            hsize_t panChunkDims[3];
            int nDimSize = H5Pget_chunk(listid, 3, panChunkDims);
            nBlockXSize  = (int) panChunkDims[nDimSize-1];
            nBlockYSize  = (int) panChunkDims[nDimSize-2];
        }

        int nfilters = H5Pget_nfilters( listid );

        H5Z_filter_t filter;
        char         name[120];
        size_t       cd_nelmts = 20;
        unsigned int cd_values[20];
        unsigned int flags;
        for (int i = 0; i < nfilters; i++) 
        {
          filter = H5Pget_filter(listid, i, &flags, (size_t *)&cd_nelmts, cd_values, 120, name);
          if (filter == H5Z_FILTER_DEFLATE)
            poDS->SetMetadataItem( "COMPRESSION", "DEFLATE", "IMAGE_STRUCTURE" );
          else if (filter == H5Z_FILTER_NBIT)
            poDS->SetMetadataItem( "COMPRESSION", "NBIT", "IMAGE_STRUCTURE" );
          else if (filter == H5Z_FILTER_SCALEOFFSET)
            poDS->SetMetadataItem( "COMPRESSION", "SCALEOFFSET", "IMAGE_STRUCTURE" );
          else if (filter == H5Z_FILTER_SZIP)
            poDS->SetMetadataItem( "COMPRESSION", "SZIP", "IMAGE_STRUCTURE" );
        }

        H5Pclose(listid);
    }

/* -------------------------------------------------------------------- */
/*      Load min/max information.                                       */
/* -------------------------------------------------------------------- */
    if( EQUAL(pszName,"elevation") 
        && GH5_FetchAttribute( hDatasetID, "Maximum Elevation Value", 
                            dfMaximum ) 
        && GH5_FetchAttribute( hDatasetID, "Minimum Elevation Value", 
                               dfMinimum ) )
        bMinMaxSet = true;
    else if( EQUAL(pszName,"uncertainty") 
             && GH5_FetchAttribute( hDatasetID, "Maximum Uncertainty Value", 
                                    dfMaximum ) 
             && GH5_FetchAttribute( hDatasetID, "Minimum Uncertainty Value", 
                                    dfMinimum ) )
        bMinMaxSet = true;
    else if( EQUAL(pszName,"nominal_elevation") 
             && GH5_FetchAttribute( hDatasetID, "max_value", 
                                    dfMaximum ) 
             && GH5_FetchAttribute( hDatasetID, "min_value", 
                                    dfMinimum ) )
        bMinMaxSet = true;

    return true;
}

/************************************************************************/
/*                             GetMinimum()                             */
/************************************************************************/

double BAGRasterBand::GetMinimum( int * pbSuccess )

{
    if( bMinMaxSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return dfMinimum;
    }
    else
        return GDALRasterBand::GetMinimum( pbSuccess );
}

/************************************************************************/
/*                             GetMaximum()                             */
/************************************************************************/

double BAGRasterBand::GetMaximum( int * pbSuccess )

{
    if( bMinMaxSet )
    {
        if( pbSuccess )
            *pbSuccess = TRUE;
        return dfMaximum;
    }
    else
        return GDALRasterBand::GetMaximum( pbSuccess );
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/
double BAGRasterBand::GetNoDataValue( int * pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = TRUE;

    if( EQUAL(GetDescription(),"elevation") )
        return  1000000.0;
    else if( EQUAL(GetDescription(),"uncertainty") )
        return 0.0;
    else if( EQUAL(GetDescription(),"nominal_elevation") )
        return 1000000.0;
    else
        return GDALPamRasterBand::GetNoDataValue( pbSuccess );
}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/
CPLErr BAGRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                  void * pImage )
{
    herr_t      status;
    hsize_t     count[3];
    H5OFFSET_TYPE offset[3];
    int         nSizeOfData;
    hid_t       memspace;
    hsize_t     col_dims[3];
    hsize_t     rank;

    rank=2;

    offset[0] = MAX(0,nRasterYSize - (nBlockYOff+1)*nBlockYSize);
    offset[1] = nBlockXOff*nBlockXSize;
    count[0]  = nBlockYSize;
    count[1]  = nBlockXSize;

    nSizeOfData = H5Tget_size( native );
    memset( pImage,0,nBlockXSize*nBlockYSize*nSizeOfData );

/*  blocksize may not be a multiple of imagesize */
    count[0]  = MIN( size_t(nBlockYSize), GetYSize() - offset[0]);
    count[1]  = MIN( size_t(nBlockXSize), GetXSize() - offset[1]);

    if( nRasterYSize - (nBlockYOff+1)*nBlockYSize < 0 )
    {
        count[0] += (nRasterYSize - (nBlockYOff+1)*nBlockYSize);
    }

/* -------------------------------------------------------------------- */
/*      Select block from file space                                    */
/* -------------------------------------------------------------------- */
    status =  H5Sselect_hyperslab( dataspace,
                                   H5S_SELECT_SET,
                                   offset, NULL,
                                   count, NULL );

/* -------------------------------------------------------------------- */
/*      Create memory space to receive the data                         */
/* -------------------------------------------------------------------- */
    col_dims[0]=nBlockYSize;
    col_dims[1]=nBlockXSize;
    memspace = H5Screate_simple( (int) rank, col_dims, NULL );
    H5OFFSET_TYPE mem_offset[3] = {0, 0, 0};
    status =  H5Sselect_hyperslab(memspace,
                                  H5S_SELECT_SET,
                                  mem_offset, NULL,
                                  count, NULL);

    status = H5Dread ( hDatasetID,
                       native,
                       memspace,
                       dataspace,
                       H5P_DEFAULT,
                       pImage );

    H5Sclose( memspace );

/* -------------------------------------------------------------------- */
/*      Y flip the data.                                                */
/* -------------------------------------------------------------------- */
    int nLinesToFlip = count[0];
    int nLineSize = nSizeOfData * nBlockXSize;
    GByte *pabyTemp = (GByte *) CPLMalloc(nLineSize);

    for( int iY = 0; iY < nLinesToFlip/2; iY++ )
    {
        memcpy( pabyTemp, 
                ((GByte *)pImage) + iY * nLineSize,
                nLineSize );
        memcpy( ((GByte *)pImage) + iY * nLineSize,
                ((GByte *)pImage) + (nLinesToFlip-iY-1) * nLineSize,
                nLineSize );
        memcpy( ((GByte *)pImage) + (nLinesToFlip-iY-1) * nLineSize,
                pabyTemp,
                nLineSize );
    }

    CPLFree( pabyTemp );

/* -------------------------------------------------------------------- */
/*      Return success or failure.                                      */
/* -------------------------------------------------------------------- */
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
/* ==================================================================== */
/*                              BAGDataset                              */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                             BAGDataset()                             */
/************************************************************************/

BAGDataset::BAGDataset()
{
    hHDF5 = -1;
    pszXMLMetadata = NULL;
    pszProjection = NULL;

    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
}

/************************************************************************/
/*                            ~BAGDataset()                             */
/************************************************************************/
BAGDataset::~BAGDataset( )
{
    FlushCache();

    if( hHDF5 >= 0 )
        H5Fclose( hHDF5 );

    CPLFree( pszXMLMetadata );
    CPLFree( pszProjection );
}

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int BAGDataset::Identify( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Is it an HDF5 file?                                             */
/* -------------------------------------------------------------------- */
    static const char achSignature[] = "\211HDF\r\n\032\n";

    if( poOpenInfo->pabyHeader == NULL
        || memcmp(poOpenInfo->pabyHeader,achSignature,8) != 0 )
        return FALSE;

/* -------------------------------------------------------------------- */
/*      Does it have the extension .bag?                                */
/* -------------------------------------------------------------------- */
    if( !EQUAL(CPLGetExtension(poOpenInfo->pszFilename),"bag") )
        return FALSE;

    return TRUE;
}

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *BAGDataset::Open( GDALOpenInfo * poOpenInfo )

{
/* -------------------------------------------------------------------- */
/*      Confirm that this appears to be a BAG file.                     */
/* -------------------------------------------------------------------- */
    if( !Identify( poOpenInfo ) )
        return NULL;

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The BAG driver does not support update access." );
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Open the file as an HDF5 file.                                  */
/* -------------------------------------------------------------------- */
    hid_t hHDF5 = H5Fopen( poOpenInfo->pszFilename, 
                           H5F_ACC_RDONLY, H5P_DEFAULT );

    if( hHDF5 < 0 )  
        return NULL;

/* -------------------------------------------------------------------- */
/*      Confirm it is a BAG dataset by checking for the                 */
/*      BAG_Root/Bag Version attribute.                                 */
/* -------------------------------------------------------------------- */
    hid_t hBagRoot = H5Gopen( hHDF5, "/BAG_root" );
    hid_t hVersion = -1;

    if( hBagRoot >= 0 )
        hVersion = H5Aopen_name( hBagRoot, "Bag Version" );

    if( hVersion < 0 )
    {
        if( hBagRoot >= 0 )
            H5Gclose( hBagRoot );
        H5Fclose( hHDF5 );
        return NULL;
    }
    H5Aclose( hVersion );

/* -------------------------------------------------------------------- */
/*      Create a corresponding dataset.                                 */
/* -------------------------------------------------------------------- */
    BAGDataset *poDS = new BAGDataset();

    poDS->hHDF5 = hHDF5;

/* -------------------------------------------------------------------- */
/*      Extract version as metadata.                                    */
/* -------------------------------------------------------------------- */
    CPLString osVersion;

    if( GH5_FetchAttribute( hBagRoot, "Bag Version", osVersion ) )
        poDS->SetMetadataItem( "BagVersion", osVersion );

    H5Gclose( hBagRoot );

/* -------------------------------------------------------------------- */
/*      Fetch the elevation dataset and attach as a band.               */
/* -------------------------------------------------------------------- */
    int nNextBand = 1;
    hid_t hElevation = H5Dopen( hHDF5, "/BAG_root/elevation" );
    if( hElevation < 0 )
    {
        delete poDS;
        return NULL;
    }

    BAGRasterBand *poElevBand = new BAGRasterBand( poDS, nNextBand );

    if( !poElevBand->Initialize( hElevation, "elevation" ) )
    {
        delete poElevBand;
        delete poDS;
        return NULL;
    }

    poDS->nRasterXSize = poElevBand->nRasterXSize;
    poDS->nRasterYSize = poElevBand->nRasterYSize;

    poDS->SetBand( nNextBand++, poElevBand );

/* -------------------------------------------------------------------- */
/*      Try to do the same for the uncertainty band.                    */
/* -------------------------------------------------------------------- */
    hid_t hUncertainty = H5Dopen( hHDF5, "/BAG_root/uncertainty" );
    BAGRasterBand *poUBand = new BAGRasterBand( poDS, nNextBand );

    if( hUncertainty >= 0 && poUBand->Initialize( hUncertainty, "uncertainty") )
    {
        poDS->SetBand( nNextBand++, poUBand );
    }
    else
        delete poUBand;

/* -------------------------------------------------------------------- */
/*      Try to do the same for the uncertainty band.                    */
/* -------------------------------------------------------------------- */
    hid_t hNominal = -1;

    H5E_BEGIN_TRY {
        hNominal = H5Dopen( hHDF5, "/BAG_root/nominal_elevation" );
    } H5E_END_TRY;

    BAGRasterBand *poNBand = new BAGRasterBand( poDS, nNextBand );
    if( hNominal >= 0 && poNBand->Initialize( hNominal,
                                              "nominal_elevation" ) )
    {
        poDS->SetBand( nNextBand++, poNBand );
    }
    else
        delete poNBand;
        
/* -------------------------------------------------------------------- */
/*      Load the XML metadata.                                          */
/* -------------------------------------------------------------------- */
    poDS->LoadMetadata();

/* -------------------------------------------------------------------- */
/*      Setup/check for pam .aux.xml.                                   */
/* -------------------------------------------------------------------- */
    poDS->SetDescription( poOpenInfo->pszFilename );
    poDS->TryLoadXML();

/* -------------------------------------------------------------------- */
/*      Setup overviews.                                                */
/* -------------------------------------------------------------------- */
    poDS->oOvManager.Initialize( poDS, poOpenInfo->pszFilename );

    return( poDS );
}

/************************************************************************/
/*                            LoadMetadata()                            */
/************************************************************************/

void BAGDataset::LoadMetadata()

{
/* -------------------------------------------------------------------- */
/*      Load the metadata from the file.                                */
/* -------------------------------------------------------------------- */
    hid_t hMDDS = H5Dopen( hHDF5, "/BAG_root/metadata" );
    hid_t datatype     = H5Dget_type( hMDDS );
    hid_t dataspace    = H5Dget_space( hMDDS );
    hid_t native       = H5Tget_native_type( datatype, H5T_DIR_ASCEND );
    hsize_t dims[3], maxdims[3];

    H5Sget_simple_extent_dims( dataspace, dims, maxdims );

    pszXMLMetadata = (char *) CPLCalloc((int) (dims[0]+1),1);

    H5Dread( hMDDS, native, H5S_ALL, dataspace, H5P_DEFAULT, pszXMLMetadata );

    H5Tclose( native );
    H5Sclose( dataspace );
    H5Tclose( datatype );
    H5Dclose( hMDDS );

    if( strlen(pszXMLMetadata) == 0 )
        return;

/* -------------------------------------------------------------------- */
/*      Try to get the geotransform.                                    */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psRoot = CPLParseXMLString( pszXMLMetadata );

    if( psRoot == NULL )
        return;

    CPLStripXMLNamespace( psRoot, NULL, TRUE );

    CPLXMLNode *psGeo = CPLSearchXMLNode( psRoot, "=MD_Georectified" );

    if( psGeo != NULL )
    {
        char **papszCornerTokens = 
            CSLTokenizeStringComplex( 
                CPLGetXMLValue( psGeo, "cornerPoints.Point.coordinates", "" ),
                " ,", FALSE, FALSE );

        if( CSLCount(papszCornerTokens ) == 4 )
        {
            double dfLLX = atof( papszCornerTokens[0] );
            double dfLLY = atof( papszCornerTokens[1] );
            double dfURX = atof( papszCornerTokens[2] );
            double dfURY = atof( papszCornerTokens[3] );

            adfGeoTransform[0] = dfLLX;
            adfGeoTransform[1] = (dfURX - dfLLX) / (GetRasterXSize()-1);
            adfGeoTransform[3] = dfURY;
            adfGeoTransform[5] = (dfLLY - dfURY) / (GetRasterYSize()-1);

            adfGeoTransform[0] -= adfGeoTransform[1] * 0.5;
            adfGeoTransform[3] -= adfGeoTransform[5] * 0.5;
        }
        CSLDestroy( papszCornerTokens );
    }

/* -------------------------------------------------------------------- */
/*      Try to get the coordinate system.                               */
/* -------------------------------------------------------------------- */
    OGRSpatialReference oSRS;

    if( OGR_SRS_ImportFromISO19115( &oSRS, pszXMLMetadata )
        == OGRERR_NONE )
    {
        oSRS.exportToWkt( &pszProjection );
    } 
    else
    {
        ParseWKTFromXML( pszXMLMetadata );
    }

/* -------------------------------------------------------------------- */
/*      Fetch acquisition date.                                         */
/* -------------------------------------------------------------------- */
    CPLXMLNode *psDateTime = CPLSearchXMLNode( psRoot, "=dateTime" );
    if( psDateTime != NULL )
    {
        const char *pszDateTimeValue = CPLGetXMLValue( psDateTime, NULL, "" );
        if( pszDateTimeValue )
            SetMetadataItem( "BAG_DATETIME", pszDateTimeValue );
    }

    CPLDestroyXMLNode( psRoot );
}

/************************************************************************/
/*                          ParseWKTFromXML()                           */
/************************************************************************/
OGRErr BAGDataset::ParseWKTFromXML( const char *pszISOXML )
{
    OGRSpatialReference oSRS;
    CPLXMLNode *psRoot = CPLParseXMLString( pszISOXML );
    OGRErr eOGRErr = OGRERR_FAILURE;

    if( psRoot == NULL )
        return eOGRErr;

    CPLStripXMLNamespace( psRoot, NULL, TRUE ); 

    CPLXMLNode *psRSI = CPLSearchXMLNode( psRoot, "=referenceSystemInfo" );
    if( psRSI == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
          "Unable to find <referenceSystemInfo> in metadata." );
        CPLDestroyXMLNode( psRoot );
        return eOGRErr;
    }

    oSRS.Clear();

    const char *pszSRCodeString = 
        CPLGetXMLValue( psRSI, "MD_ReferenceSystem.referenceSystemIdentifier.RS_Identifier.code.CharacterString", NULL );
    if( pszSRCodeString == NULL )
    {
        CPLDebug("BAG",
          "Unable to find /MI_Metadata/referenceSystemInfo[1]/MD_ReferenceSystem[1]/referenceSystemIdentifier[1]/RS_Identifier[1]/code[1]/CharacterString[1] in metadata." );
        CPLDestroyXMLNode( psRoot );
        return eOGRErr;
    }
    
    const char *pszSRCodeSpace = 
        CPLGetXMLValue( psRSI, "MD_ReferenceSystem.referenceSystemIdentifier.RS_Identifier.codeSpace.CharacterString", "" );
    if( !EQUAL( pszSRCodeSpace, "WKT" ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Spatial reference string is not in WKT." );
        CPLDestroyXMLNode( psRoot );
        return eOGRErr;
    }

    char* pszWKT = const_cast< char* >( pszSRCodeString );
    if( oSRS.importFromWkt( &pszWKT ) != OGRERR_NONE )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
          "Failed parsing WKT string \"%s\".", pszSRCodeString );
        CPLDestroyXMLNode( psRoot );
        return eOGRErr;
    }

    oSRS.exportToWkt( &pszProjection );
    eOGRErr = OGRERR_NONE;

    psRSI = CPLSearchXMLNode( psRSI->psNext, "=referenceSystemInfo" );
    if( psRSI == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Unable to find second instance of <referenceSystemInfo> in metadata." );
        CPLDestroyXMLNode( psRoot );
        return eOGRErr;
    }

    pszSRCodeString = 
      CPLGetXMLValue( psRSI, "MD_ReferenceSystem.referenceSystemIdentifier.RS_Identifier.code.CharacterString", NULL );
    if( pszSRCodeString == NULL )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Unable to find /MI_Metadata/referenceSystemInfo[2]/MD_ReferenceSystem[1]/referenceSystemIdentifier[1]/RS_Identifier[1]/code[1]/CharacterString[1] in metadata." );
        CPLDestroyXMLNode( psRoot );
        return eOGRErr;
    }

    pszSRCodeSpace = 
        CPLGetXMLValue( psRSI, "MD_ReferenceSystem.referenceSystemIdentifier.RS_Identifier.codeSpace.CharacterString", "" );
    if( !EQUAL( pszSRCodeSpace, "WKT" ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Spatial reference string is not in WKT." );
        CPLDestroyXMLNode( psRoot );
        return eOGRErr;
    }

    if( EQUALN(pszSRCodeString, "VERTCS", 6 ) )
    {
        CPLString oString( pszProjection );
        oString += ",";
        oString += pszSRCodeString;
        if ( pszProjection )
            CPLFree( pszProjection );
        pszProjection = CPLStrdup( oString );
    }

    CPLDestroyXMLNode( psRoot );
    
    return eOGRErr;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr BAGDataset::GetGeoTransform( double *padfGeoTransform )

{
    if( adfGeoTransform[0] != 0.0 || adfGeoTransform[3] != 0.0 )
    {
        memcpy( padfGeoTransform, adfGeoTransform, sizeof(double)*6 );
        return CE_None;
    }
    else
        return GDALPamDataset::GetGeoTransform( padfGeoTransform );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char *BAGDataset::GetProjectionRef()

{
    if( pszProjection )
        return pszProjection;
    else 
        return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                      GetMetadataDomainList()                         */
/************************************************************************/

char **BAGDataset::GetMetadataDomainList()
{
    return BuildMetadataDomainList(GDALPamDataset::GetMetadataDomainList(),
                                   TRUE,
                                   "xml:BAG", NULL);
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/

char **BAGDataset::GetMetadata( const char *pszDomain )

{
    if( pszDomain != NULL && EQUAL(pszDomain,"xml:BAG") )
    {
        apszMDList[0] = pszXMLMetadata;
        apszMDList[1] = NULL;

        return apszMDList;
    }
    else
        return GDALPamDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                          GDALRegister_BAG()                          */
/************************************************************************/
void GDALRegister_BAG( )

{
    GDALDriver  *poDriver;
    
    if (! GDAL_CHECK_VERSION("BAG"))
        return;

    if(  GDALGetDriverByName( "BAG" ) == NULL )
    {
        poDriver = new GDALDriver();
        
        poDriver->SetDescription( "BAG" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Bathymetry Attributed Grid" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_bag.html" );
        poDriver->pfnOpen = BAGDataset::Open;
        poDriver->pfnIdentify = BAGDataset::Identify;

        GetGDALDriverManager( )->RegisterDriver( poDriver );
    }
}
