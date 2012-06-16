/******************************************************************************
 * $Id$
 *
 * Project:  netCDF read/write Driver
 * Purpose:  GDAL bindings over netCDF library.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2004, Frank Warmerdam
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

#include "netcdfdataset.h"
#include "cpl_error.h"
CPL_CVSID("$Id$");

#include <map> //for NCDFWriteProjAttribs()

/* Internal function declarations */

int NCDFIsGDALVersionGTE(const char* pszVersion, int nTarget);

void NCDFAddGDALHistory( int fpImage, 
                         const char * pszFilename, const char *pszOldHist,
                         const char * pszFunctionName );

void NCDFAddHistory(int fpImage, const char *pszAddHist, const char *pszOldHist);

int NCDFIsCfProjection( const char* pszProjection );

void NCDFWriteProjAttribs(const OGR_SRSNode *poPROJCS,
                            const char* pszProjection,
                            const int fpImage, const int NCDFVarID);

CPLErr NCDFSafeStrcat(char** ppszDest, char* pszSrc, size_t* nDestSize);

CPLErr NCDFGetAttr( int nCdfId, int nVarId, const char *pszAttrName, 
                    double *pdfValue );

CPLErr NCDFGetAttr( int nCdfId, int nVarId, const char *pszAttrName, 
                    char **pszValue );

CPLErr NCDFPutAttr( int nCdfId, int nVarId, 
                    const char *pszAttrName, const char *pszValue );
 

/************************************************************************/
/* ==================================================================== */
/*                         netCDFRasterBand                             */
/* ==================================================================== */
/************************************************************************/

class netCDFRasterBand : public GDALPamRasterBand
{
    friend class netCDFDataset;

    nc_type     nc_datatype;
    int         cdfid;
    int         nZId;
    int         nZDim;
    int         nLevel;
    int         nBandXPos;
    int         nBandYPos;
    int         *panBandZPos;
    int         *panBandZLev;
    int         bNoDataSet;
    double      dfNoDataValue;
    double      adfValidRange[2];
    double      dfScale;
    double      dfOffset;
    int         bSignedData;
    int         status;

    CPLErr	    CreateBandMetadata( int *paDimIds ); 
    template <class T> void  CheckValidData ( void * pImage, 
                                              int bCheckIsNan=FALSE ) ;
    
  public:

    netCDFRasterBand( netCDFDataset *poDS, 
                      int nZId, 
                      int nZDim,
                      int nLevel, 
                      int *panBandZLen,
                      int *panBandPos,
                      int *paDimIds,
                      int nBand );
    netCDFRasterBand( netCDFDataset *poDS, 
                      GDALDataType eType,
                      int nBand,
                      int bSigned=TRUE,
                      char *pszBandName=NULL,
                      char *pszLongName=NULL );
    ~netCDFRasterBand( );

    virtual double GetNoDataValue( int * );
    virtual CPLErr SetNoDataValue( double );
    virtual double GetOffset( int * );
    virtual CPLErr SetOffset( double );
    virtual double GetScale( int * );
    virtual CPLErr SetScale( double );
    virtual CPLErr IReadBlock( int, int, void * );
    virtual CPLErr IWriteBlock( int, int, void * );

};

/************************************************************************/
/*                          netCDFRasterBand()                          */
/************************************************************************/

netCDFRasterBand::netCDFRasterBand( netCDFDataset *poNCDFDS, 
                                    int nZId, 
                                    int nZDim,
                                    int nLevel, 
                                    int *panBandZLev, 
                                    int *panBandDimPos, 
                                    int *paDimIds,
                                    int nBand)

{
    double   dfNoData = 0.0;
    int      bNoDataSet = FALSE;
    nc_type  vartype=NC_NAT;
    nc_type  atttype=NC_NAT;
    size_t   attlen;
    char     szNoValueName[NCDF_MAX_STR_LEN];

    this->poDS = poNCDFDS;
    this->panBandZPos = NULL;
    this->panBandZLev = NULL;
    this->nBand = nBand;
    this->nZId = nZId;
    this->nZDim = nZDim;
    this->nLevel = nLevel;
    this->nBandXPos = panBandDimPos[0];
    this->nBandYPos = panBandDimPos[1];
    this->bSignedData = TRUE; //default signed, except for Byte 
    this->cdfid = poNCDFDS->GetCDFID();
    this->status = NC_NOERR;

/* -------------------------------------------------------------------- */
/*      Take care of all other dimmensions                              */
/* ------------------------------------------------------------------ */
    if( nZDim > 2 ) {
        this->panBandZPos = 
            (int *) CPLCalloc( nZDim-1, sizeof( int ) );
        this->panBandZLev = 
            (int *) CPLCalloc( nZDim-1, sizeof( int ) );

        for ( int i=0; i < nZDim - 2; i++ ){
            this->panBandZPos[i] = panBandDimPos[i+2];
            this->panBandZLev[i] = panBandZLev[i];
        }
    }
    bNoDataSet    = FALSE;
    dfNoDataValue = 0.0;

    nRasterXSize  = poDS->GetRasterXSize( );
    nRasterYSize  = poDS->GetRasterYSize( );
    nBlockXSize   = poDS->GetRasterXSize( );
    nBlockYSize   = 1;

/* -------------------------------------------------------------------- */
/*      Get the type of the "z" variable, our target raster array.      */
/* -------------------------------------------------------------------- */
    if( nc_inq_var( cdfid, nZId, NULL, &nc_datatype, NULL, NULL,
                    NULL ) != NC_NOERR ){
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "Error in nc_var_inq() on 'z'." );
        return;
    }

    if( nc_datatype == NC_BYTE )
        eDataType = GDT_Byte;
#ifdef NETCDF_HAS_NC4
    /* NC_UBYTE (unsigned byte) is only available for NC4 */
    else if( nc_datatype == NC_UBYTE )
        eDataType = GDT_Byte;
#endif    
    else if( nc_datatype == NC_CHAR )
        eDataType = GDT_Byte;        
    else if( nc_datatype == NC_SHORT )
        eDataType = GDT_Int16;
    else if( nc_datatype == NC_INT )
        eDataType = GDT_Int32;
    else if( nc_datatype == NC_FLOAT )
        eDataType = GDT_Float32;
    else if( nc_datatype == NC_DOUBLE )
        eDataType = GDT_Float64;
    else
    {
        if( nBand == 1 )
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "Unsupported netCDF datatype (%d), treat as Float32.", 
                      (int) nc_datatype );
        eDataType = GDT_Float32;
    }

/* -------------------------------------------------------------------- */
/*      Find out what is No Data for this variable                      */
/* -------------------------------------------------------------------- */

    status = nc_inq_att( cdfid, nZId, 
                         _FillValue, &atttype, &attlen);

/* -------------------------------------------------------------------- */
/*      Look for either Missing_Value or _FillValue attributes          */
/* -------------------------------------------------------------------- */

    if( status == NC_NOERR ) {
        strcpy(szNoValueName, _FillValue );
    }
    else {
        status = nc_inq_att( cdfid, nZId, 
                             "missing_value", &atttype, &attlen );
        if( status == NC_NOERR ) {
            strcpy( szNoValueName, "missing_value" );
        }
    }

    if( status == NC_NOERR ) {
        if ( NCDFGetAttr( cdfid, nZId, szNoValueName, 
                          &dfNoData ) == CE_None )
            bNoDataSet = TRUE;
    }

    nc_inq_vartype( cdfid, nZId, &vartype );

    /* if not found NoData, set the default one */
    if ( ! bNoDataSet ) { 
        switch( vartype ) {
            case NC_BYTE:
#ifdef NETCDF_HAS_NC4
            case NC_UBYTE:
#endif    
                /* don't do default fill-values for bytes, too risky */
                dfNoData = 0.0;
                /* should print a warning as users might not be expecting this */
                /* CPLError(CE_Warning, 1,"GDAL netCDF driver is setting default NoData value to 0.0 for NC_BYTE data\n"); */
               break;
            case NC_CHAR:
                dfNoData = NC_FILL_CHAR;
                break;
            case NC_SHORT:
                dfNoData = NC_FILL_SHORT;
                break;
            case NC_INT:
                dfNoData = NC_FILL_INT;
                break;
            case NC_FLOAT:
                dfNoData = NC_FILL_FLOAT;
                break;
            case NC_DOUBLE:
                dfNoData = NC_FILL_DOUBLE;
                break;
            default:
                dfNoData = 0.0;
                break;
        }
	    bNoDataSet = TRUE;
    } 
    
    if ( bNoDataSet ) 
        SetNoDataValue( dfNoData );
    else 
        CPLDebug( "GDAL_netCDF", "did not get nodata value for variable #%d", nZId );

/* -------------------------------------------------------------------- */
/*  Look for valid_range or valid_min/valid_max                         */
/* -------------------------------------------------------------------- */
    /* set valid_range to nodata, then check for actual values */
    adfValidRange[0] = dfNoData;
    adfValidRange[1] = dfNoData;
    /* first look for valid_range */
    int bGotValidRange = FALSE;
    status = nc_inq_att( cdfid, nZId, 
                         "valid_range", &atttype, &attlen);
    if( (status == NC_NOERR) && (attlen == 2)) {
        int vrange[2];
        int vmin, vmax;
        status = nc_get_att_int( cdfid, nZId,
                                 "valid_range", vrange ); 
        if( status == NC_NOERR ) {
            bGotValidRange = TRUE;
            adfValidRange[0] = vrange[0];
            adfValidRange[1] = vrange[1];
        }
        /* if not found look for valid_min and valid_max */
        else {
            status = nc_get_att_int( cdfid, nZId,
                                     "valid_min", &vmin );
            if( status == NC_NOERR ) {
                adfValidRange[0] = vmin;
                status = nc_get_att_int( cdfid, nZId,
                                         "valid_max", &vmax );
                if( status == NC_NOERR ) {
                    adfValidRange[1] = vmax;
                    bGotValidRange = TRUE;
                }
            }
        }
    }

    CPLDebug( "GDAL_netCDF", "valid_range={%f,%f}",
              adfValidRange[0], adfValidRange[1] );

/* -------------------------------------------------------------------- */
/*  Special For Byte Bands: check for signed/unsigned byte              */
/* -------------------------------------------------------------------- */
    if ( nc_datatype == NC_BYTE ) {

        /* netcdf uses signed byte by default, but GDAL uses unsigned by default */
        /* This may cause unexpected results, but is needed for back-compat */
        if ( poNCDFDS->bIsGdalFile )
            this->bSignedData = FALSE;
        else 
            this->bSignedData = TRUE;

        /* For NC4 format NC_BYTE is signed, NC_UBYTE is unsigned */
        if ( poNCDFDS->nFormat == NCDF_FORMAT_NC4 ) {
            this->bSignedData = TRUE;
        }   
        else  {
            /* if we got valid_range, test for signed/unsigned range */
            /* http://www.unidata.ucar.edu/software/netcdf/docs/netcdf/Attribute-Conventions.html */
            if ( bGotValidRange == TRUE ) {
                /* If we got valid_range={0,255}, treat as unsigned */
                if ( (adfValidRange[0] == 0) && (adfValidRange[1] == 255) ) {
                    bSignedData = FALSE;
                    /* reset valid_range */
                    adfValidRange[0] = dfNoData;
                    adfValidRange[1] = dfNoData;
                }
                /* If we got valid_range={-128,127}, treat as signed */
                else if ( (adfValidRange[0] == -128) && (adfValidRange[1] == 127) ) {
                    bSignedData = TRUE;
                    /* reset valid_range */
                    adfValidRange[0] = dfNoData;
                    adfValidRange[1] = dfNoData;
                }
            }
            /* else test for _Unsigned */
            /* http://www.unidata.ucar.edu/software/netcdf/docs/BestPractices.html */
            else {
                char *pszTemp = NULL;
                if ( NCDFGetAttr( cdfid, nZId, "_Unsigned", &pszTemp ) 

                     == CE_None ) {
                    if ( EQUAL(pszTemp,"true"))
                        bSignedData = FALSE;
                    else if ( EQUAL(pszTemp,"false"))
                        bSignedData = TRUE;
                    CPLFree( pszTemp );
                }
            }
        }

        if ( bSignedData )
        {
            /* set PIXELTYPE=SIGNEDBYTE */
            /* See http://trac.osgeo.org/gdal/wiki/rfc14_imagestructure */
            SetMetadataItem( "PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE" );    
            CPLDebug( "GDAL_netCDF", "got signed Byte" );        
        }
        else 
            CPLDebug( "GDAL_netCDF", "got unsigned Byte" );

    }

/* -------------------------------------------------------------------- */
/*      Create Band Metadata                                            */
/* -------------------------------------------------------------------- */
    CreateBandMetadata( paDimIds );

/* -------------------------------------------------------------------- */
/* Attempt to fetch the scale_factor and add_offset attributes for the  */
/* variable and set them.  If these values are not available, set       */
/* offset to 0 and scale to 1                                           */
/* -------------------------------------------------------------------- */
    double dfOff = 0.0; 
    double dfScale = 1.0; 
    
    if ( nc_inq_attid ( cdfid, nZId, CF_ADD_OFFSET, NULL) == NC_NOERR ) { 
        status = nc_get_att_double( cdfid, nZId, CF_ADD_OFFSET, &dfOff );
        CPLDebug( "GDAL_netCDF", "got add_offset=%.16g, status=%d", dfOff, status );
    }
    if ( nc_inq_attid ( cdfid, nZId, 
                        CF_SCALE_FACTOR, NULL) == NC_NOERR ) { 
        status = nc_get_att_double( cdfid, nZId, CF_SCALE_FACTOR, &dfScale ); 
        CPLDebug( "GDAL_netCDF", "got scale_factor=%.16g, status=%d", dfScale, status );
    }
    SetOffset( dfOff ); 
    SetScale( dfScale ); 
}

/* constructor in create mode, assume just 2 dimensions for now */
netCDFRasterBand::netCDFRasterBand( netCDFDataset *poNCDFDS, 
                                    GDALDataType eType,
                                    int nBand,
                                    int bSigned,
                                    char *pszBandName,
                                    char *pszLongName )

{
    int  anBandDims[ NC_MAX_DIMS ]; 
    int      status;
   
    double   dfNoData = 0.0;
    char szTemp[NCDF_MAX_STR_LEN];

    this->poDS = poNCDFDS;
    this->nBand = nBand;
    this->nZId = -1;
    this->nZDim = 2;
    this->nLevel = 0;
    this->panBandZPos = NULL;
    this->panBandZLev = NULL;
    this->nBandXPos = 1;
    this->nBandYPos = 0; 
    this->bSignedData = bSigned;  

    this->status = NC_NOERR;
    this->cdfid = poNCDFDS->GetCDFID();

    bNoDataSet    = FALSE;
    dfNoDataValue = 0.0;

    nRasterXSize   = poDS->GetRasterXSize( );
    nRasterYSize   = poDS->GetRasterYSize( );
    nBlockXSize   = poDS->GetRasterXSize( );
    nBlockYSize   = 1;

    if ( poDS->GetAccess() != GA_Update ) {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "Dataset is not in update mode, wrong netCDFRasterBand constructor" );
        return;
    }
    
/* -------------------------------------------------------------------- */
/*      Take care of all other dimmensions                              */
/* ------------------------------------------------------------------ */
    // if( nZDim > 2 ) {
    //     this->panBandZPos = 
    //         (int *) CPLCalloc( nZDim-1, sizeof( int ) );
    //     this->panBandZLev = 
    //         (int *) CPLCalloc( nZDim-1, sizeof( int ) );

    //     for ( int i=0; i < nZDim - 2; i++ ){
    //         this->panBandZPos[i] = panBandDimPos[i+2];
    //         this->panBandZLev[i] = panBandZLev[i];
    //     }
    // }

/* -------------------------------------------------------------------- */
/*      Get the type of the "z" variable, our target raster array.      */
/* -------------------------------------------------------------------- */
    eDataType = eType;

    switch ( eDataType ) 
    {
        case GDT_Byte:
            nc_datatype = NC_BYTE;
#ifdef NETCDF_HAS_NC4
            /* NC_UBYTE (unsigned byte) is only available for NC4 */
            if ( ! bSignedData && (poNCDFDS->nFormat == NCDF_FORMAT_NC4) )
                nc_datatype = NC_UBYTE;
#endif    
            break;
        case GDT_Int16:
            nc_datatype = NC_SHORT;
            break;
        case GDT_Int32:
            nc_datatype = NC_INT;
            break;
        case GDT_Float32:
            nc_datatype = NC_FLOAT;
            break;
        case GDT_Float64:
            nc_datatype = NC_DOUBLE;
            break;
        default:
            if( nBand == 1 )
                CPLError( CE_Warning, CPLE_AppDefined, 
                          "Unsupported GDAL datatype (%d), treat as NC_FLOAT.", 
                          (int) eDataType );
            nc_datatype = NC_FLOAT;
            break;
    }

/* -------------------------------------------------------------------- */
/*      Define the variable                                             */
/* -------------------------------------------------------------------- */
    anBandDims[0] = poNCDFDS->nYDimID;
    anBandDims[1] = poNCDFDS->nXDimID;

    if ( !pszBandName || EQUAL(pszBandName,"")  )
        sprintf( szTemp, "Band%d", nBand );
    else 
        strcpy( szTemp, pszBandName );
            CPLDebug( "GDAL_netCDF", "nc_def_var(%d,%s,%d)",
                      cdfid, szTemp, nc_datatype );
            status = nc_def_var( cdfid, szTemp, nc_datatype, 
                         NCDF_NBDIM, anBandDims, &nZId );
    NCDF_ERR(status);

    if ( !pszLongName || EQUAL(pszLongName,"")  )
        sprintf( szTemp, "GDAL Band Number %d", nBand );
    else 
        strcpy( szTemp, pszLongName );
    status =  nc_put_att_text( cdfid, nZId, CF_LNG_NAME, 
                               strlen( szTemp ), szTemp );
    NCDF_ERR(status);


    poNCDFDS->DefVarDeflate(nZId, TRUE);

    /* for Byte data add signed/unsigned info */
    if ( eDataType == GDT_Byte ) {

        CPLDebug( "GDAL_netCDF", "adding valid_range attributes for Byte Band" );
        /* For unsigned NC_BYTE (except NC4 format) */
        /* add valid_range and _Unsigned ( defined in CF-1 and NUG ) */
        if ( (nc_datatype == NC_BYTE) && (poNCDFDS->nFormat != NCDF_FORMAT_NC4) ) {
            short int adfValidRange[2]; 
            if  ( bSignedData ) {
                adfValidRange[0] = -128;
                adfValidRange[1] = 127;
                status = nc_put_att_text( cdfid,nZId, 
                                          "_Unsigned", 5, "false" );
            }
            else {
                adfValidRange[0] = 0;
                adfValidRange[1] = 255;
                status = nc_put_att_text( cdfid,nZId, 
                                          "_Unsigned", 4, "true" );
            }
            status=nc_put_att_short( cdfid,nZId, "valid_range",
                                     NC_SHORT, 2, adfValidRange );
        }         
        /* for unsigned byte set PIXELTYPE=SIGNEDBYTE */
        /* See http://trac.osgeo.org/gdal/wiki/rfc14_imagestructure */
        if  ( bSignedData ) 
            SetMetadataItem( "PIXELTYPE", "SIGNEDBYTE", "IMAGE_STRUCTURE" );    

    }

    /* set default nodata */
    switch( nc_datatype ) {
        case NC_BYTE:
#ifdef NETCDF_HAS_NC4
        case NC_UBYTE:
#endif    
            /* don't do default fill-values for bytes, too risky */
            dfNoData = 0.0;
            /* should print a warning as users might not be expecting this */
            /* CPLError(CE_Warning, 1,"GDAL netCDF driver is setting default NoData value to 0.0 for NC_BYTE data\n"); */
            break;
        case NC_CHAR:
            dfNoData = NC_FILL_CHAR;
            break;
        case NC_SHORT:
            dfNoData = NC_FILL_SHORT;
            break;
        case NC_INT:
            dfNoData = NC_FILL_INT;
            break;
        case NC_FLOAT:
            dfNoData = NC_FILL_FLOAT;
            break;
        case NC_DOUBLE:
            dfNoData = NC_FILL_DOUBLE;
            break;
        default:
            dfNoData = 0.0;
            break;
    }

    SetNoDataValue( dfNoData );

}

/************************************************************************/
/*                         ~netCDFRasterBand()                          */
/************************************************************************/

netCDFRasterBand::~netCDFRasterBand()
{
    FlushCache();
    if( panBandZPos ) 
        CPLFree( panBandZPos );
    if( panBandZLev )
        CPLFree( panBandZLev );
}

/************************************************************************/ 
/*                             GetOffset()                              */ 
/************************************************************************/ 
double netCDFRasterBand::GetOffset( int *pbSuccess ) 
{ 
    if( pbSuccess != NULL ) 
        *pbSuccess = TRUE; 
	 
    return dfOffset; 
}

/************************************************************************/ 
/*                             SetOffset()                              */ 
/************************************************************************/ 
CPLErr netCDFRasterBand::SetOffset( double dfNewOffset ) 
{ 
    dfOffset = dfNewOffset; 

    /* write value if in update mode */
    if ( poDS->GetAccess() == GA_Update ) {

        /* make sure we are in define mode */
        ( ( netCDFDataset * ) poDS )->SetDefineMode( TRUE );

        status = nc_put_att_double( cdfid, nZId, CF_ADD_OFFSET,
                                    NC_DOUBLE, 1, &dfOffset );

        NCDF_ERR(status);
        if ( status == NC_NOERR )
            return CE_None;
        else
            return CE_Failure;

    }

    return CE_None; 
}

/************************************************************************/ 
/*                              GetScale()                              */ 
/************************************************************************/ 
double netCDFRasterBand::GetScale( int *pbSuccess ) 
{ 
    if( pbSuccess != NULL ) 
        *pbSuccess = TRUE; 
    return dfScale; 
}

/************************************************************************/ 
/*                              SetScale()                              */ 
/************************************************************************/ 
CPLErr netCDFRasterBand::SetScale( double dfNewScale )  
{ 
    dfScale = dfNewScale; 

    /* write value if in update mode */
    if ( poDS->GetAccess() == GA_Update ) {

        /* make sure we are in define mode */
        ( ( netCDFDataset * ) poDS )->SetDefineMode( TRUE );

        status = nc_put_att_double( cdfid, nZId, CF_SCALE_FACTOR,
                                    NC_DOUBLE, 1, &dfScale );

        NCDF_ERR(status);
        if ( status == NC_NOERR )
            return CE_None;
        else
            return CE_Failure;

    }

    return CE_None;
}

/************************************************************************/
/*                           GetNoDataValue()                           */
/************************************************************************/

double netCDFRasterBand::GetNoDataValue( int * pbSuccess )

{
    if( pbSuccess )
        *pbSuccess = bNoDataSet;

    if( bNoDataSet )
        return dfNoDataValue;
    else
        return GDALPamRasterBand::GetNoDataValue( pbSuccess );
}

/************************************************************************/
/*                           SetNoDataValue()                           */
/************************************************************************/

CPLErr netCDFRasterBand::SetNoDataValue( double dfNoData )

{
    bNoDataSet = TRUE;
    dfNoDataValue = dfNoData;

    CPLDebug( "GDAL_netCDF", "SetNoDataValue( %.18g )", dfNoData );

    /* write value if in update mode */
    if ( poDS->GetAccess() == GA_Update ) {

        /* make sure we are in define mode */
        ( ( netCDFDataset * ) poDS )->SetDefineMode( TRUE );

        if ( eDataType == GDT_Byte) {
            if ( bSignedData ) {
                signed char cNoDataValue = (signed char) dfNoData;
                status = nc_put_att_schar( cdfid, nZId, _FillValue,
                                           nc_datatype, 1, &cNoDataValue );            
            }
            else {
                unsigned char ucNoDataValue = (unsigned char) dfNoDataValue;
                status = nc_put_att_uchar( cdfid, nZId, _FillValue,
                                           nc_datatype, 1, &ucNoDataValue );            
            }
        }
        else if ( eDataType == GDT_Int16 ) {
            short int nsNoDataValue = (short int) dfNoDataValue;
            status = nc_put_att_short( cdfid, nZId, _FillValue,
                                       nc_datatype, 1, &nsNoDataValue );
        }
        else if ( eDataType == GDT_Int32) {
            int nNoDataValue = (int) dfNoDataValue;
            status = nc_put_att_int( cdfid, nZId, _FillValue,
                                     nc_datatype, 1, &nNoDataValue );
        }
        else if ( eDataType == GDT_Float32) {
            float fNoDataValue = (float) dfNoDataValue;
            status = nc_put_att_float( cdfid, nZId, _FillValue,
                                       nc_datatype, 1, &fNoDataValue );
        }
        else 
            status = nc_put_att_double( cdfid, nZId, _FillValue,
                                        nc_datatype, 1, &dfNoDataValue );

        NCDF_ERR(status);
        if ( status == NC_NOERR )
            return CE_None;
        else
            return CE_Failure;

    }
    return CE_None;
}

/************************************************************************/
/*                         CreateBandMetadata()                         */
/************************************************************************/

CPLErr netCDFRasterBand::CreateBandMetadata( int *paDimIds ) 

{
    char     szVarName[NC_MAX_NAME];
    char     szMetaName[NC_MAX_NAME];
    char     szMetaTemp[NCDF_MAX_STR_LEN];
    char     *pszMetaValue = NULL;
    char     szTemp[NC_MAX_NAME];
    const char *pszValue;

    int      nd;
    int      i,j;
    int      Sum  = 1;
    int      Taken = 0;
    int      result = 0;
    int      status;
    int      nVarID = -1;
    int      nDims;
    size_t   start[1];
    size_t   count[1];
    nc_type  nVarType = NC_NAT;
    int      nAtt=0;

    netCDFDataset *poDS = (netCDFDataset *) this->poDS;
  
/* -------------------------------------------------------------------- */
/*      Compute all dimensions from Band number and save in Metadata    */
/* -------------------------------------------------------------------- */
    nc_inq_varname( cdfid, nZId, szVarName );
    nc_inq_varndims( cdfid, nZId, &nd );
/* -------------------------------------------------------------------- */
/*      Compute multidimention band position                            */
/*                                                                      */
/* BandPosition = (Total - sum(PastBandLevels) - 1)/sum(remainingLevels)*/
/* if Data[2,3,4,x,y]                                                   */
/*                                                                      */
/*  BandPos0 = (nBand ) / (3*4)                                         */
/*  BandPos1 = (nBand - BandPos0*(3*4) ) / (4)                          */
/*  BandPos2 = (nBand - BandPos0*(3*4) ) % (4)                          */
/* -------------------------------------------------------------------- */

    sprintf( szMetaName,"NETCDF_VARNAME");
    sprintf( szMetaTemp,"%s",szVarName);
    SetMetadataItem( szMetaName, szMetaTemp );
    if( nd == 3 ) {
        Sum *= panBandZLev[0];
    }

    for( i=0; i < nd-2 ; i++ ) {
        if( i != nd - 2 -1 ) {
            Sum = 1;
            for( j=i+1; j < nd-2; j++ ) {
                Sum *= panBandZLev[j];
            }
            result = (int) ( ( nLevel-Taken) / Sum );
        }
        else {
            result = (int) ( ( nLevel-Taken) % Sum );
        }
        
        strcpy(szVarName, 
               poDS->papszDimName[paDimIds[panBandZPos[i]]] );

        sprintf( szMetaName,"NETCDF_DIMENSION_%s",  szVarName );

        status=nc_inq_varid( cdfid, szVarName, &nVarID );

/* -------------------------------------------------------------------- */
/*      Try to uppercase the first letter of the variable               */
/* -------------------------------------------------------------------- */

        if( status != NC_NOERR ) {
            szVarName[0]=(char) toupper(szVarName[0]);
            status=nc_inq_varid( cdfid, szVarName, &nVarID );
        }

        status = nc_inq_vartype( cdfid, nVarID, &nVarType );

        nDims = 0;
        status = nc_inq_varndims( cdfid, nVarID, &nDims );

        if( nDims == 1 ) {
            count[0]=1;
            start[0]=result;
            switch( nVarType ) {
                case NC_SHORT:
                    short sData;
                    status =  nc_get_vara_short( cdfid, nVarID, 
                                                 start,
                                                 count, &sData );
                    sprintf( szMetaTemp,"%d", sData );
                    break;
                case NC_INT:
                    int nData;
                    status =  nc_get_vara_int( cdfid, nVarID, 
                                               start,
                                               count, &nData );
                    sprintf( szMetaTemp,"%d", nData );
                    break;
                case NC_FLOAT:
                    float fData;
                    status =  nc_get_vara_float( cdfid, nVarID, 
                                                 start,
                                                 count, &fData );
                    sprintf( szMetaTemp,"%.8g", fData );
                    break;
                case NC_DOUBLE:
                    double dfData;
                    status =  nc_get_vara_double( cdfid, nVarID, 
                                                  start,
                                                  count, &dfData);
                    sprintf( szMetaTemp,"%.16g", dfData );
                    break;
                default: 
                    CPLDebug( "GDAL_netCDF", "invalid dim %s, type=%d", 
                              szMetaTemp, nVarType);
                    break;
            }
        }
        else
            sprintf( szMetaTemp,"%d", result+1);
	
        CPLDebug( "GDAL_netCDF", "setting dimension metadata %s=%s", 
                  szMetaName, szMetaTemp );

        SetMetadataItem( szMetaName, szMetaTemp );

/* -------------------------------------------------------------------- */
/*      Fetch dimension units                                           */
/* -------------------------------------------------------------------- */

        strcpy( szTemp, szVarName );
        strcat( szTemp, "#units" );
        pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
        if( pszValue != NULL ) {
            if( EQUAL( pszValue, "T") ) { 
                strcpy( szTemp, szVarName );
                strcat( szTemp, "#original_units" );
                pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
                strcpy( szTemp, "NETCDF_");
                strcat( szTemp, szVarName );
                strcat( szTemp, "_original_units" );
                SetMetadataItem( szTemp, pszValue );
            }
            else {
                strcpy( szTemp, "NETCDF_");
                strcat( szTemp, szVarName  );
                strcat( szTemp, "_units" );
                SetMetadataItem( szTemp, pszValue );
            }
        }
        Taken += result * Sum;
    }

/* -------------------------------------------------------------------- */
/*      Get all other metadata                                          */
/* -------------------------------------------------------------------- */
    nc_inq_varnatts( cdfid, nZId, &nAtt );

    for( i=0; i < nAtt ; i++ ) {

    	status = nc_inq_attname( cdfid, nZId, i, szTemp);
    	// if(strcmp(szTemp,_FillValue) ==0) continue;
    	sprintf( szMetaName,"%s",szTemp);       

        if ( NCDFGetAttr( cdfid, nZId, szMetaName, &pszMetaValue) 
             == CE_None ) {
            SetMetadataItem( szMetaName, pszMetaValue );
        }
        else {
            CPLDebug( "GDAL_netCDF", "invalid Band metadata %s", szMetaName );
        }

        if ( pszMetaValue )  {
            CPLFree( pszMetaValue );
            pszMetaValue = NULL;
        }

    }

    return CE_None;
}

/************************************************************************/
/*                             CheckValidData()                         */
/************************************************************************/
template <class T>
void  netCDFRasterBand::CheckValidData ( void * pImage, int bCheckIsNan ) 
{
    int i;
    CPLAssert( pImage != NULL );

    /* check if needed or requested */
    if (  (adfValidRange[0] != dfNoDataValue) || 
          (adfValidRange[1] != dfNoDataValue) ||
          bCheckIsNan ) {

        for( i=0; i<nBlockXSize; i++ ) {         
            /* check for nodata and nan */
            if ( CPLIsEqual( (double) ((T *)pImage)[i], dfNoDataValue ) )
                continue;
            if( bCheckIsNan && CPLIsNan( (double) (( (T *) pImage))[i] ) ) { 
                ( (T *)pImage )[i] = (T)dfNoDataValue;
                continue;
            }
            /* check for valid_range */
            if ( ( ( adfValidRange[0] != dfNoDataValue ) && 
                   ( ((T *)pImage)[i] < (T)adfValidRange[0] ) ) 
                 || 
                 ( ( adfValidRange[1] != dfNoDataValue ) && 
                   ( ((T *)pImage)[i] > (T)adfValidRange[1] ) ) ) {
                ( (T *)pImage )[i] = (T)dfNoDataValue;
            }
        }
        
    }

}

/************************************************************************/
/*                             IReadBlock()                             */
/************************************************************************/

CPLErr netCDFRasterBand::IReadBlock( int nBlockXOff, int nBlockYOff,
                                     void * pImage )

{
    size_t start[ MAX_NC_DIMS ];
    size_t edge[ MAX_NC_DIMS ];
    char   pszName[ NCDF_MAX_STR_LEN ];
    int    i,j;
    int    Sum=-1;
    int    Taken=-1;
    int    nd;

    if ( (nBlockYOff == 0) || (nBlockYOff == nRasterYSize-1) )
        CPLDebug( "GDAL_netCDF", "netCDFRasterBand::IReadBlock( %d, %d, ... )",
                  nBlockXOff, nBlockYOff );

    *pszName='\0';
    memset( start, 0, sizeof( start ) );
    memset( edge,  0, sizeof( edge )  );
    nc_inq_varndims ( cdfid, nZId, &nd );

/* -------------------------------------------------------------------- */
/*      Locate X, Y and Z position in the array                         */
/* -------------------------------------------------------------------- */
	
    start[nBandXPos] = 0;          // x dim can move arround in array
    // check y order
    if( ( ( netCDFDataset *) poDS )->bBottomUp ) {
        start[nBandYPos] = nRasterYSize - 1 - nBlockYOff;
    } else {
        start[nBandYPos] = nBlockYOff; // y
    }
        
    edge[nBandXPos] = nBlockXSize; 
    edge[nBandYPos] = 1;

    if( nd == 3 ) {
        start[panBandZPos[0]]  = nLevel;     // z
        edge [panBandZPos[0]]  = 1;
    }

/* -------------------------------------------------------------------- */
/*      Compute multidimention band position                            */
/*                                                                      */
/* BandPosition = (Total - sum(PastBandLevels) - 1)/sum(remainingLevels)*/
/* if Data[2,3,4,x,y]                                                   */
/*                                                                      */
/*  BandPos0 = (nBand ) / (3*4)                                         */
/*  BandPos1 = (nBand - (3*4) ) / (4)                                   */
/*  BandPos2 = (nBand - (3*4) ) % (4)                                   */
/* -------------------------------------------------------------------- */
    if (nd > 3) 
    {
        Taken = 0;
        for( i=0; i < nd-2 ; i++ ) 
        {
            if( i != nd - 2 -1 ) {
                Sum = 1;
                for( j=i+1; j < nd-2; j++ ) {
                    Sum *= panBandZLev[j];
                }
                start[panBandZPos[i]] = (int) ( ( nLevel-Taken) / Sum );
                edge[panBandZPos[i]] = 1;
            } else {
                start[panBandZPos[i]] = (int) ( ( nLevel-Taken) % Sum );
                edge[panBandZPos[i]] = 1;
            }
            Taken += start[panBandZPos[i]] * Sum;
        }
    }

    /* make sure we are in data mode */
    ( ( netCDFDataset * ) poDS )->SetDefineMode( FALSE );

    /* read data according to type */
    if( eDataType == GDT_Byte ) 
    {
        if (this->bSignedData) 
        {
            status = nc_get_vara_schar( cdfid, nZId, start, edge, 
                                      (signed char *) pImage );
            if ( status == NC_NOERR ) CheckValidData<signed char>( pImage );
        }
        else {
            status = nc_get_vara_uchar( cdfid, nZId, start, edge, 
                                      (unsigned char *) pImage );
            if ( status == NC_NOERR ) CheckValidData<unsigned char>( pImage );
        }
    }
    else if( eDataType == GDT_Int16 )
    {
        status = nc_get_vara_short( cdfid, nZId, start, edge, 
                                  (short int *) pImage );
        if ( status == NC_NOERR ) CheckValidData<short int>( pImage );
    }
    else if( eDataType == GDT_Int32 )
    {
        if( sizeof(long) == 4 )
        {
            status = nc_get_vara_long( cdfid, nZId, start, edge, 
                                     (long *) pImage );
            if ( status == NC_NOERR ) CheckValidData<long>( pImage );
        }
        else
        {
            status = nc_get_vara_int( cdfid, nZId, start, edge, 
                                    (int *) pImage );
            if ( status == NC_NOERR ) CheckValidData<int>( pImage );
        }
    }
    else if( eDataType == GDT_Float32 )
    {
        status = nc_get_vara_float( cdfid, nZId, start, edge, 
                                  (float *) pImage );
        if ( status == NC_NOERR ) CheckValidData<float>( pImage, TRUE );
    }
    else if( eDataType == GDT_Float64 )
    {
        status = nc_get_vara_double( cdfid, nZId, start, edge, 
                                   (double *) pImage ); 
        if ( status == NC_NOERR ) CheckValidData<double>( pImage, TRUE );
    } 
    else
        status = NC_EBADTYPE;

    if( status != NC_NOERR )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "netCDF scanline fetch failed: %s", 
                  nc_strerror( status ) );
        return CE_Failure;
    }
    else
        return CE_None;
}

/************************************************************************/
/*                             IWriteBlock()                            */
/************************************************************************/

CPLErr netCDFRasterBand::IWriteBlock( int nBlockXOff, int nBlockYOff,
                                      void * pImage )

{
    int    status=-1;//, status2=-1;
    // int    cdfid = ( ( netCDFDataset * ) poDS )->cdfid;
    size_t start[ NCDF_NBDIM ];
    size_t edge[ MAX_NC_DIMS ];
    char   pszName[ NCDF_MAX_STR_LEN ];
    int    i,j;
    int    Sum=-1;
    int    Taken=-1;
    int    nd;

    if ( (nBlockYOff == 0) || (nBlockYOff == nRasterYSize-1) )
        CPLDebug( "GDAL_netCDF", "netCDFRasterBand::IReadBlock( %d, %d, ... )",
                  nBlockXOff, nBlockYOff );

    *pszName='\0';
    memset( start, 0, sizeof( start ) );
    memset( edge,  0, sizeof( edge )  );
    // nc_inq_varndims ( cdfid, nZId, &nd );
    nd=2;
  
/* -------------------------------------------------------------------- */
/*      Locate X, Y and Z position in the array                         */
/* -------------------------------------------------------------------- */
	
    start[nBandXPos] = 0;          // x dim can move arround in array
    // check y order
    if( ( ( netCDFDataset *) poDS )->bBottomUp ) {
        start[nBandYPos] = nRasterYSize - 1 - nBlockYOff;
    } else {
        start[nBandYPos] = nBlockYOff; // y
    }
        
    edge[nBandXPos] = nBlockXSize; 
    edge[nBandYPos] = 1;

    if( nd == 3 ) {
        start[panBandZPos[0]]  = nLevel;     // z
        edge [panBandZPos[0]]  = 1;
    }
    
/* -------------------------------------------------------------------- */
/*      Compute multidimention band position                            */
/*                                                                      */
/* BandPosition = (Total - sum(PastBandLevels) - 1)/sum(remainingLevels)*/
/* if Data[2,3,4,x,y]                                                   */
/*                                                                      */
/*  BandPos0 = (nBand ) / (3*4)                                         */
/*  BandPos1 = (nBand - (3*4) ) / (4)                                   */
/*  BandPos2 = (nBand - (3*4) ) % (4)                                   */
/* -------------------------------------------------------------------- */
    if (nd > 3) 
    {
        Taken = 0;
        for( i=0; i < nd-2 ; i++ ) 
        {
            if( i != nd - 2 -1 ) {
                Sum = 1;
                for( j=i+1; j < nd-2; j++ ) {
                    Sum *= panBandZLev[j];
                }
                start[panBandZPos[i]] = (int) ( ( nLevel-Taken) / Sum );
                edge[panBandZPos[i]] = 1;
            } else {
                start[panBandZPos[i]] = (int) ( ( nLevel-Taken) % Sum );
                edge[panBandZPos[i]] = 1;
            }
            Taken += start[panBandZPos[i]] * Sum;
        }
    }
    
    /* make sure we are in data mode */
    ( ( netCDFDataset * ) poDS )->SetDefineMode( FALSE );

    /* copy data according to type */
    if( eDataType == GDT_Byte ) {
        if ( this->bSignedData ) 
            status = nc_put_vara_schar( cdfid, nZId, start, edge, 
                                         (signed char*) pImage);
        else
            status = nc_put_vara_uchar( cdfid, nZId, start, edge, 
                                         (unsigned char*) pImage);
    }
    else if( ( eDataType == GDT_UInt16 ) || ( eDataType == GDT_Int16 ) ) {
        status = nc_put_vara_short( cdfid, nZId, start, edge, 
                                     (short int *) pImage);
    }
    else if( eDataType == GDT_Int32 ) {
        status = nc_put_vara_int( cdfid, nZId, start, edge, 
                                   (int *) pImage);
    }
    else if( eDataType == GDT_Float32 ) {
        status = nc_put_vara_float( cdfid, nZId, start, edge, 
                                    (float *) pImage);
    }
    else if( eDataType == GDT_Float64 ) {
        status = nc_put_vara_double( cdfid, nZId, start, edge, 
                                     (double *) pImage);
    }
    else {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The NetCDF driver does not support GDAL data type %d",
                  eDataType );
        status = NC_EBADTYPE;
    }
    NCDF_ERR(status);

    if( status != NC_NOERR )
    {
        CPLError( CE_Failure, CPLE_AppDefined, 
                  "netCDF scanline write failed: %s", 
                  nc_strerror( status ) );
        return CE_Failure;
    }
    else 
        return CE_None;

}

/************************************************************************/
/* ==================================================================== */
/*				netCDFDataset				                            */
/* ==================================================================== */
/************************************************************************/

/************************************************************************/
/*                           netCDFDataset()                            */
/************************************************************************/

netCDFDataset::netCDFDataset()

{
    /* basic dataset vars */
    cdfid            = -1;
    papszSubDatasets = NULL;
    papszMetadata    = NULL;	
    bBottomUp        = TRUE;
    nFormat          = NCDF_FORMAT_NONE;
    bIsGdalFile      = FALSE;
    bIsGdalCfFile    = FALSE;

    /* projection/GT */
    adfGeoTransform[0] = 0.0;
    adfGeoTransform[1] = 1.0;
    adfGeoTransform[2] = 0.0;
    adfGeoTransform[3] = 0.0;
    adfGeoTransform[4] = 0.0;
    adfGeoTransform[5] = 1.0;
    pszProjection    = NULL;
    nXDimID = -1;
    nYDimID = -1;
    bIsProjected = FALSE;
    bIsGeographic = FALSE; /* can be not projected, and also not geographic */

    /* state vars */
    status = NC_NOERR;
    bDefineMode = TRUE;    
    bSetProjection = FALSE;
    bSetGeoTransform = FALSE;    
    bAddedProjectionVars = FALSE;

    /* create vars */
    papszCreationOptions = NULL;
    nCompress = NCDF_COMPRESS_NONE;
    nZLevel = NCDF_DEFLATE_LEVEL;
    nCreateMode = NC_CLOBBER;
    bSignedData = TRUE;
}


/************************************************************************/
/*                           ~netCDFDataset()                           */
/************************************************************************/

netCDFDataset::~netCDFDataset()

{
    /* make sure projection is written if GeoTransform OR Projection are missing */
    if( (GetAccess() == GA_Update) && (! bAddedProjectionVars) ) {
        if ( bSetProjection && ! bSetGeoTransform )
            AddProjectionVars();
        else if ( bSetGeoTransform && ! bSetProjection )
            AddProjectionVars();
            // CPLError( CE_Warning, CPLE_AppDefined, 
            //           "netCDFDataset::~netCDFDataset() Projection was not defined, projection will be missing" );
    }
    FlushCache();

    CSLDestroy( papszMetadata );
    CSLDestroy( papszSubDatasets );
    CSLDestroy( papszCreationOptions );

    if( pszProjection )
        CPLFree( pszProjection );

    if( cdfid ) 
        nc_close( cdfid );
}

/************************************************************************/
/*                            SetDefineMode()                           */
/************************************************************************/
int netCDFDataset::SetDefineMode( int bNewDefineMode )
{
    /* do nothing if already in new define mode
       or if dataset is in read-only mode */
    if ( ( bDefineMode == bNewDefineMode ) || 
         ( GetAccess() == GA_ReadOnly ) ) 
        return CE_None;

    CPLDebug( "GDAL_netCDF", "SetDefineMode(%d) old=%d",
              bNewDefineMode, bDefineMode );

    bDefineMode = bNewDefineMode;
    
    if ( bDefineMode == TRUE ) 
        status = nc_redef( cdfid );
    else
        status = nc_enddef( cdfid );

    NCDF_ERR(status);
    return status;
}

/************************************************************************/
/*                            GetMetadata()                             */
/************************************************************************/
char **netCDFDataset::GetMetadata( const char *pszDomain )
{
    if( pszDomain != NULL && EQUALN( pszDomain, "SUBDATASETS", 11 ) )
        return papszSubDatasets;
    else
        return GDALDataset::GetMetadata( pszDomain );
}

/************************************************************************/
/*                          GetProjectionRef()                          */
/************************************************************************/

const char * netCDFDataset::GetProjectionRef()
{
    if( bSetProjection )
        return pszProjection;
    else
        return GDALPamDataset::GetProjectionRef();
}

/************************************************************************/
/*                           FetchCopyParm()                            */
/************************************************************************/

double netCDFDataset::FetchCopyParm( const char *pszGridMappingValue, 
                                     const char *pszParm, double dfDefault )

{
    char         szTemp[ MAX_NC_NAME ];
    const char  *pszValue;

    strcpy(szTemp,pszGridMappingValue);
    strcat( szTemp, "#" );
    strcat( szTemp, pszParm );
    pszValue = CSLFetchNameValue(papszMetadata, szTemp);

    if( pszValue )
    {
        return CPLAtofM(pszValue);
    }
    else
        return dfDefault;
}

/************************************************************************/
/*                           FetchStandardParallels()                   */
/************************************************************************/

char** netCDFDataset::FetchStandardParallels( const char *pszGridMappingValue )
{
    char         szTemp[ MAX_NC_NAME ];
    const char   *pszValue;
    char         **papszValues = NULL;
    //cf-1.0 tags
    strcpy( szTemp,pszGridMappingValue );
    strcat( szTemp, "#" );
    strcat( szTemp, CF_PP_STD_PARALLEL );
    pszValue = CSLFetchNameValue( papszMetadata, szTemp );
    if( pszValue != NULL ) {
        // papszValues = CSLTokenizeString2( pszValue, ",", CSLT_STRIPLEADSPACES |
        //                                   CSLT_STRIPENDSPACES );
        /* format has changed to { std1, std2 }, must remove { and } */
        strcpy( szTemp,pszValue );
        int last_char = strlen(pszValue) - 1;
        if ( szTemp[0] == '{' ) szTemp[0] = ' ';
        if ( szTemp[last_char] == '}' ) szTemp[last_char] = ' ';
        papszValues = CSLTokenizeString2( szTemp, ",", CSLT_STRIPLEADSPACES |
                                          CSLT_STRIPENDSPACES );
    }
    //try gdal tags
    else
    {
        strcpy( szTemp, pszGridMappingValue );
        strcat( szTemp, "#" );
        strcat( szTemp, CF_PP_STD_PARALLEL_1 );

        pszValue = CSLFetchNameValue( papszMetadata, szTemp );
	
        if ( pszValue != NULL )
            papszValues = CSLAddString( papszValues, pszValue );
				    
        strcpy( szTemp,pszGridMappingValue );
        strcat( szTemp, "#" );
        strcat( szTemp, CF_PP_STD_PARALLEL_2 );

        pszValue = CSLFetchNameValue( papszMetadata, szTemp );
	
        if( pszValue != NULL )	
            papszValues = CSLAddString( papszValues, pszValue );
    }
    
    return papszValues;
}

/************************************************************************/
/*                      SetProjectionFromVar()                          */
/************************************************************************/
void netCDFDataset::SetProjectionFromVar( int var )
{
    size_t       start[2], edge[2];
    unsigned int i;
    const char   *pszValue;
    int          nVarProjectionID;
    char         szVarName[ MAX_NC_NAME ];
    char         szTemp[ MAX_NC_NAME ];
    char         szGridMappingName[ MAX_NC_NAME ];
    char         szGridMappingValue[ MAX_NC_NAME ];

    double       dfStdP1=0.0;
    double       dfStdP2=0.0;
    double       dfCenterLat;
    double       dfCenterLon;
    double       dfScale;
    double       dfFalseEasting;
    double       dfFalseNorthing;
    double       dfCentralMeridian;
    double       dfEarthRadius;
    double       dfInverseFlattening;
    double       dfLonPrimeMeridian;
    double       dfSemiMajorAxis;
    double       dfSemiMinorAxis;
    
    int          bGotGeogCS = FALSE;
    int          bGotCfSRS = FALSE;
    int          bGotGdalSRS = FALSE;
    int          bGotCfGT = FALSE;
    int          bGotGdalGT = FALSE;
    int          bLookForWellKnownGCS = FALSE;  //this could be a Config Option

    /* These values from CF metadata */
    OGRSpatialReference oSRS;
    int          nVarDimXID = -1;
    int          nVarDimYID = -1;
    double       *pdfXCoord;
    double       *pdfYCoord;
    char         szDimNameX[ MAX_NC_NAME ];
    char         szDimNameY[ MAX_NC_NAME ];
    int          nSpacingBegin;
    int          nSpacingMiddle;
    int          nSpacingLast;
    size_t       xdim = nRasterXSize;
    size_t       ydim = nRasterYSize;

    const char  *pszUnits = NULL;

    /* These values from GDAL metadata */
    const char *pszWKT = NULL;
    const char *pszGeoTransform = NULL;
    char **papszGeoTransform = NULL;

    netCDFDataset * poDS = this; /* perhaps this should be removed for clarity */

    /* temp variables to use in SetGeoTransform() and SetProjection() */
    double      adfTempGeoTransform[6];
    char        *pszTempProjection;

    CPLDebug( "GDAL_netCDF", "\n=====\nSetProjectionFromVar( %d )\n", var );

/* -------------------------------------------------------------------- */
/*      Get x/y range information.                                      */
/* -------------------------------------------------------------------- */

    adfTempGeoTransform[0] = 0.0;
    adfTempGeoTransform[1] = 1.0;
    adfTempGeoTransform[2] = 0.0;
    adfTempGeoTransform[3] = 0.0;
    adfTempGeoTransform[4] = 0.0;
    adfTempGeoTransform[5] = 1.0;
    pszTempProjection = NULL;

/* -------------------------------------------------------------------- */
/*      Look for grid_mapping metadata                                  */
/* -------------------------------------------------------------------- */

    strcpy( szGridMappingValue, "" );
    strcpy( szGridMappingName, "" );

    nc_inq_varname( cdfid, var, szVarName );
    strcpy(szTemp,szVarName);
    strcat(szTemp,"#");
    strcat(szTemp,CF_GRD_MAPPING);

    pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);
    if( pszValue ) {
        strcpy(szGridMappingName,szTemp);
        strcpy(szGridMappingValue,pszValue);
    }

    if( !EQUAL( szGridMappingValue, "" )  ) {

        /*  Read grid_mapping metadata */
        nc_inq_varid( cdfid, szGridMappingValue, &nVarProjectionID );
        poDS->ReadAttributes( cdfid, nVarProjectionID );
        
/* -------------------------------------------------------------------- */
/*      Look for GDAL spatial_ref and GeoTransform within grid_mapping  */
/* -------------------------------------------------------------------- */
        CPLDebug( "GDAL_netCDF", "got grid_mapping %s", szGridMappingValue );
        strcpy( szTemp,szGridMappingValue);
        strcat( szTemp, "#" );
        strcat( szTemp, NCDF_SPATIAL_REF);

        pszWKT = CSLFetchNameValue(poDS->papszMetadata, szTemp);
	
        if( pszWKT != NULL ) {
            strcpy( szTemp,szGridMappingValue);
            strcat( szTemp, "#" );
            strcat( szTemp, NCDF_GEOTRANSFORM);
            pszGeoTransform = CSLFetchNameValue(poDS->papszMetadata, szTemp);	    
        }
    }

/* -------------------------------------------------------------------- */
/*      Get information about the file.                                 */
/* -------------------------------------------------------------------- */
/*      Was this file created by the GDAL netcdf driver?                */
/*      Was this file created by the newer (CF-conformant) driver?      */
/* -------------------------------------------------------------------- */
/* 1) If GDAL netcdf metadata is set, and version >= 1.9,               */
/*    it was created with the new driver                                */
/* 2) Else, if spatial_ref and GeoTransform are present in the          */
/*    grid_mapping variable, it was created by the old driver           */
/* -------------------------------------------------------------------- */
    pszValue = CSLFetchNameValue(poDS->papszMetadata, "NC_GLOBAL#GDAL");

   if( pszValue && NCDFIsGDALVersionGTE(pszValue, 1900)) {
        bIsGdalFile = TRUE;
        bIsGdalCfFile = TRUE;
    }
    else  if( pszWKT != NULL && pszGeoTransform != NULL ) {
        bIsGdalFile = TRUE;
        bIsGdalCfFile = FALSE;
    }

/* -------------------------------------------------------------------- */
/*      Set default bottom-up default value                             */
/*      Y axis dimension and absence of GT can modify this value        */
/*      Override with Config option GDAL_NETCDF_BOTTOMUP                */ 
/* -------------------------------------------------------------------- */
   /* new driver is bottom-up by default */
   if ( bIsGdalFile && ! bIsGdalCfFile )
       poDS->bBottomUp = FALSE;
   else
       poDS->bBottomUp = TRUE;

    CPLDebug( "GDAL_netCDF", 
              "bIsGdalFile=%d bIsGdalCfFile=%d bBottomUp=%d", 
              bIsGdalFile, bIsGdalCfFile, bBottomUp );
 
/* -------------------------------------------------------------------- */
/*      Look for dimension: lon                                         */
/* -------------------------------------------------------------------- */

    memset( szDimNameX, '\0', sizeof( char ) * MAX_NC_NAME );
    memset( szDimNameY, '\0', sizeof( char ) * MAX_NC_NAME );

    for( i = 0; (i < strlen( poDS->papszDimName[ poDS->nXDimID ] )  && 
                 i < 3 ); i++ ) {
        szDimNameX[i]=(char)tolower( ( poDS->papszDimName[poDS->nXDimID] )[i] );
    }
    szDimNameX[3] = '\0';
    for( i = 0; (i < strlen( poDS->papszDimName[ poDS->nYDimID ] )  && 
                 i < 3 ); i++ ) {
        szDimNameY[i]=(char)tolower( ( poDS->papszDimName[poDS->nYDimID] )[i] );
    }
    szDimNameY[3] = '\0';

/* -------------------------------------------------------------------- */
/*      Read grid_mapping information and set projections               */
/* -------------------------------------------------------------------- */

    if( !( EQUAL(szGridMappingName,"" ) ) ) {
     
        strcpy( szTemp, szGridMappingValue );
        strcat( szTemp, "#" );
        strcat( szTemp, CF_GRD_MAPPING_NAME );
        pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);

        if( pszValue != NULL ) {

/* -------------------------------------------------------------------- */
/*      Check for datum/spheroid information                            */
/* -------------------------------------------------------------------- */
            dfEarthRadius = 
                poDS->FetchCopyParm( szGridMappingValue, 
                                     CF_PP_EARTH_RADIUS, 
                                     -1.0 );

            dfLonPrimeMeridian = 
                poDS->FetchCopyParm( szGridMappingValue,
                                     CF_PP_LONG_PRIME_MERIDIAN, 
                                     0.0 );

            dfInverseFlattening = 
                poDS->FetchCopyParm( szGridMappingValue, 
                                     CF_PP_INVERSE_FLATTENING, 
                                     -1.0 );
	    
            dfSemiMajorAxis = 
                poDS->FetchCopyParm( szGridMappingValue, 
                                     CF_PP_SEMI_MAJOR_AXIS, 
                                     -1.0 );
	    
            dfSemiMinorAxis = 
                poDS->FetchCopyParm( szGridMappingValue, 
                                     CF_PP_SEMI_MINOR_AXIS, 
                                     -1.0 );

            //see if semi-major exists if radius doesn't
            if( dfEarthRadius < 0.0 )
                dfEarthRadius = dfSemiMajorAxis;
	    
            //if still no radius, check old tag
            if( dfEarthRadius < 0.0 )
                dfEarthRadius = poDS->FetchCopyParm( szGridMappingValue, 
                                                     CF_PP_EARTH_RADIUS_OLD,
                                                     -1.0 );

            //has radius value
            if( dfEarthRadius > 0.0 ) {
                //check for inv_flat tag
                if( dfInverseFlattening < 0.0 ) {
                    //no inv_flat tag, check for semi_minor
                    if( dfSemiMinorAxis < 0.0 ) {
                        //no way to get inv_flat, use sphere
                        oSRS.SetGeogCS( "unknown", 
                                        NULL, 
                                        "Sphere", 
                                        dfEarthRadius, 0.0 );
                        bGotGeogCS = TRUE;
                    }
                    else {
                        if( dfSemiMajorAxis < 0.0 )
                            dfSemiMajorAxis = dfEarthRadius;
                        //set inv_flat using semi_minor/major
                        dfInverseFlattening = 
                            1.0 / ( dfSemiMajorAxis - dfSemiMinorAxis ) / dfSemiMajorAxis;
                        oSRS.SetGeogCS( "unknown", 
                                        NULL, 
                                        "Spheroid", 
                                        dfEarthRadius, dfInverseFlattening );
                        bGotGeogCS = TRUE;
                    }
                }
                else {
                    oSRS.SetGeogCS( "unknown", 
                                    NULL, 
                                    "Spheroid", 
                                    dfEarthRadius, dfInverseFlattening );
                    bGotGeogCS = TRUE;
                }  

                if ( bGotGeogCS )
                    CPLDebug( "GDAL_netCDF", "got spheroid from CF: (%f , %f)", dfEarthRadius, dfInverseFlattening );

            }
            //no radius, set as wgs84 as default?
            else {
                // This would be too indiscrimant.  But we should set
                // it if we know the data is geographic.
                //oSRS.SetWellKnownGeogCS( "WGS84" );
            }
	    		
/* -------------------------------------------------------------------- */
/*      Transverse Mercator                                             */
/* -------------------------------------------------------------------- */

            if( EQUAL( pszValue, CF_PT_TM ) ) {

                dfScale = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_SCALE_FACTOR_MERIDIAN, 1.0 );

                dfCenterLon = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LONG_CENTRAL_MERIDIAN, 0.0 );

                dfCenterLat = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LAT_PROJ_ORIGIN, 0.0 );

                dfFalseEasting = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_EASTING, 0.0 );

                dfFalseNorthing = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_NORTHING, 0.0 );
 
                bGotCfSRS = TRUE;
                oSRS.SetTM( dfCenterLat, 
                            dfCenterLon,
                            dfScale,
                            dfFalseEasting,
                            dfFalseNorthing );

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );
            }

/* -------------------------------------------------------------------- */
/*      Albers Equal Area                                               */
/* -------------------------------------------------------------------- */

            if( EQUAL( pszValue, CF_PT_AEA ) ) {

                char **papszStdParallels = NULL;
		
                dfCenterLon = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LONG_CENTRAL_MERIDIAN, 0.0 );

                dfCenterLat = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LAT_PROJ_ORIGIN, 0.0 );

                dfFalseEasting = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_EASTING, 0.0 );

                dfFalseNorthing = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_NORTHING, 0.0 );
		
                papszStdParallels = 
                    FetchStandardParallels( szGridMappingValue );

                if( papszStdParallels != NULL ) {
		  
                    if ( CSLCount( papszStdParallels ) == 1 ) {
                        /* TODO CF-1 standard says it allows AEA to be encoded with only 1 standard parallel */
                        /* how should this actually map to a 2StdP OGC WKT version? */
                        CPLError( CE_Warning, CPLE_NotSupported, 
                                  "NetCDF driver import of AEA-1SP is not tested, using identical std. parallels\n" );
                        dfStdP1 = CPLAtofM( papszStdParallels[0] );
                        dfStdP2 = dfStdP1;

                    }
		
                    else if( CSLCount( papszStdParallels ) == 2 ) {
                        dfStdP1 = CPLAtofM( papszStdParallels[0] );
                        dfStdP2 = CPLAtofM( papszStdParallels[1] );
                    }
                }
                //old default
                else {
                    dfStdP1 = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_STD_PARALLEL_1, 0.0 );

                    dfStdP2 = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_STD_PARALLEL_2, 0.0 );
                }

                dfCenterLat = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LAT_PROJ_ORIGIN, 0.0 );

                bGotCfSRS = TRUE;
                oSRS.SetACEA( dfStdP1, dfStdP2, dfCenterLat, dfCenterLon,
                              dfFalseEasting, dfFalseNorthing );

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );

                CSLDestroy( papszStdParallels );
            }

/* -------------------------------------------------------------------- */
/*      Cylindrical Equal Area                                          */
/* -------------------------------------------------------------------- */

            else if( EQUAL( pszValue, CF_PT_CEA ) || EQUAL( pszValue, CF_PT_LCEA ) ) {

                char **papszStdParallels = NULL;

                papszStdParallels = 
                    FetchStandardParallels( szGridMappingValue );

                if( papszStdParallels != NULL ) {
                    dfStdP1 = CPLAtofM( papszStdParallels[0] );
                }
                else {
                    //TODO: add support for 'scale_factor_at_projection_origin' variant to standard parallel
                    //Probably then need to calc a std parallel equivalent
                    CPLError( CE_Failure, CPLE_NotSupported, 
                              "NetCDF driver does not support import of CF-1 LCEA "
                              "'scale_factor_at_projection_origin' variant yet.\n" );
                }

                dfCentralMeridian = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LONG_CENTRAL_MERIDIAN, 0.0 );

                dfFalseEasting = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_EASTING, 0.0 );

                dfFalseNorthing = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_NORTHING, 0.0 );
		
                bGotCfSRS = TRUE;
                oSRS.SetCEA( dfStdP1, dfCentralMeridian,
                             dfFalseEasting, dfFalseNorthing );

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );
		
                CSLDestroy( papszStdParallels );
            }

/* -------------------------------------------------------------------- */
/*      lambert_azimuthal_equal_area                                    */
/* -------------------------------------------------------------------- */
            else if( EQUAL( pszValue, CF_PT_LAEA ) ) {
                dfCenterLon = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LON_PROJ_ORIGIN, 0.0 );

                dfCenterLat = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LAT_PROJ_ORIGIN, 0.0 );

                dfFalseEasting = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_EASTING, 0.0 );

                dfFalseNorthing = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_NORTHING, 0.0 );

                oSRS.SetProjCS( "LAEA (WGS84) " );
		
                bGotCfSRS = TRUE;
                oSRS.SetLAEA( dfCenterLat, dfCenterLon,
                              dfFalseEasting, dfFalseNorthing );

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );
		
            }

/* -------------------------------------------------------------------- */
/*      Azimuthal Equidistant                                           */
/* -------------------------------------------------------------------- */
            else if( EQUAL( pszValue, CF_PT_AE ) ) {
                dfCenterLon = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LON_PROJ_ORIGIN, 0.0 );

                dfCenterLat = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LAT_PROJ_ORIGIN, 0.0 );

                dfFalseEasting = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_EASTING, 0.0 );

                dfFalseNorthing = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_NORTHING, 0.0 );

                bGotCfSRS = TRUE;
                oSRS.SetAE( dfCenterLat, dfCenterLon,
                            dfFalseEasting, dfFalseNorthing );

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );
		
            }

/* -------------------------------------------------------------------- */
/*      Lambert conformal conic                                         */
/* -------------------------------------------------------------------- */
            else if( EQUAL( pszValue, CF_PT_LCC ) ) {
		
                char **papszStdParallels = NULL;
		
                dfCenterLon = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LONG_CENTRAL_MERIDIAN, 0.0 );

                dfCenterLat = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LAT_PROJ_ORIGIN, 0.0 );

                dfFalseEasting = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_EASTING, 0.0 );

                dfFalseNorthing = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_NORTHING, 0.0 );
		
                papszStdParallels = 
                    FetchStandardParallels( szGridMappingValue );

                /* 2SP variant */
                if( CSLCount( papszStdParallels ) == 2 ) {
                    dfStdP1 = CPLAtofM( papszStdParallels[0] );
                    dfStdP2 = CPLAtofM( papszStdParallels[1] );
                    oSRS.SetLCC( dfStdP1, dfStdP2, dfCenterLat, dfCenterLon,
                                 dfFalseEasting, dfFalseNorthing );
                }
                /* 1SP variant (with standard_parallel or center lon) */
                /* See comments in netcdfdataset.h for this projection. */
                else {

                    dfScale = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_SCALE_FACTOR_ORIGIN, -1.0 );
                    
                    /* CF definition, without scale factor */
                    if( CPLIsEqual(dfScale, -1.0) ) {

                        /* with standard_parallel */
                        if( CSLCount( papszStdParallels ) == 1 )
                            dfStdP1 = CPLAtofM( papszStdParallels[0] );
                        /* with center lon instead */
                        else 
                            dfStdP1 = dfCenterLat;
                        dfStdP2 = dfStdP1;
                        
                        /* test if we should actually compute scale factor */
                        if ( ! CPLIsEqual( dfStdP1, dfCenterLat ) ) {
                            CPLError( CE_Warning, CPLE_NotSupported, 
                                      "NetCDF driver import of LCC-1SP with standard_parallel1 != latitude_of_projection_origin\n"
                                      "(which forces a computation of scale_factor) is experimental (bug #3324)\n" );
                            /* use Snyder eq. 15-4 to compute dfScale from dfStdP1 and dfCenterLat */
                            /* only tested for dfStdP1=dfCenterLat and (25,26), needs more data for testing */
                            /* other option: use the 2SP variant - how to compute new standard parallels? */
                            dfScale = ( cos(dfStdP1) * pow( tan(NCDF_PI/4 + dfStdP1/2), sin(dfStdP1) ) ) /
                                ( cos(dfCenterLat) * pow( tan(NCDF_PI/4 + dfCenterLat/2), sin(dfCenterLat) ) );
                        }
                        /* default is 1.0 */
                        else                    
                            dfScale = 1.0;
                        
                        oSRS.SetLCC1SP( dfCenterLat, dfCenterLon, dfScale, 
                                        dfFalseEasting, dfFalseNorthing );
                        /* store dfStdP1 so we can output it to CF later */
                        oSRS.SetNormProjParm( SRS_PP_STANDARD_PARALLEL_1, dfStdP1 );
                    }
                    /* OGC/PROJ.4 definition with scale factor */
                    else {
                        oSRS.SetLCC1SP( dfCenterLat, dfCenterLon, dfScale, 
                                        dfFalseEasting, dfFalseNorthing );
                    }
                }
		

                bGotCfSRS = TRUE;
                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );

                CSLDestroy( papszStdParallels );
            }
		
/* -------------------------------------------------------------------- */
/*      Is this Latitude/Longitude Grid explicitly                      */
/* -------------------------------------------------------------------- */
	    
            else if ( EQUAL ( pszValue, CF_PT_LATITUDE_LONGITUDE ) ) {
                bGotCfSRS = TRUE;
                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );
            }
/* -------------------------------------------------------------------- */
/*      Mercator                                                        */
/* -------------------------------------------------------------------- */
		  
            else if ( EQUAL ( pszValue, CF_PT_MERCATOR ) ) {

                char **papszStdParallels = NULL;

                /* If there is a standard_parallel, know it is Mercator 2SP */
                papszStdParallels = 
                    FetchStandardParallels( szGridMappingValue );
                
                if (NULL != papszStdParallels) {
                    /* CF-1 Mercator 2SP always has lat centered at equator */
                    dfStdP1 = CPLAtofM( papszStdParallels[0] );

                    dfCenterLat = 0.0;

                    dfCenterLon = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_LON_PROJ_ORIGIN, 0.0 );
              
                    dfFalseEasting = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_FALSE_EASTING, 0.0 );

                    dfFalseNorthing = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_FALSE_NORTHING, 0.0 );

                    oSRS.SetMercator2SP( dfStdP1, dfCenterLat, dfCenterLon, 
                                      dfFalseEasting, dfFalseNorthing );
                }
                else {
                    dfCenterLon = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_LON_PROJ_ORIGIN, 0.0 );
              
                    dfCenterLat = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_LAT_PROJ_ORIGIN, 0.0 );

                    dfScale = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_SCALE_FACTOR_ORIGIN,
                                             1.0 );

                    dfFalseEasting = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_FALSE_EASTING, 0.0 );

                    dfFalseNorthing = 
                        poDS->FetchCopyParm( szGridMappingValue, 
                                             CF_PP_FALSE_NORTHING, 0.0 );

                    oSRS.SetMercator( dfCenterLat, dfCenterLon, dfScale, 
                                      dfFalseEasting, dfFalseNorthing );
                }                      

                bGotCfSRS = TRUE;

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );

                CSLDestroy( papszStdParallels );
            }

/* -------------------------------------------------------------------- */
/*      Orthographic                                                    */
/* -------------------------------------------------------------------- */
		  

            else if ( EQUAL ( pszValue, CF_PT_ORTHOGRAPHIC ) ) {
                dfCenterLon = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LON_PROJ_ORIGIN, 0.0 );
	      
                dfCenterLat = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LAT_PROJ_ORIGIN, 0.0 );

                dfFalseEasting = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_EASTING, 0.0 );

                dfFalseNorthing = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_NORTHING, 0.0 );

                bGotCfSRS = TRUE;

                oSRS.SetOrthographic( dfCenterLat, dfCenterLon, 
                                      dfFalseEasting, dfFalseNorthing );

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );
            }

/* -------------------------------------------------------------------- */
/*      Polar Stereographic                                             */
/* -------------------------------------------------------------------- */
		  
            else if ( EQUAL ( pszValue, CF_PT_POLAR_STEREO ) ) {

                char **papszStdParallels = NULL;

                dfScale = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_SCALE_FACTOR_ORIGIN, 
                                         -1.0 );
                
                papszStdParallels = 
                    FetchStandardParallels( szGridMappingValue );
                
                /* CF allows the use of standard_parallel (lat_ts) OR scale_factor (k0),
                   make sure we have standard_parallel, using Snyder eq. 22-7
                   with k=1 and lat=standard_parallel */
                if ( papszStdParallels != NULL ) {
                    dfStdP1 = CPLAtofM( papszStdParallels[0] );
                    /* compute scale_factor from standard_parallel */
                    /* this creates WKT that is inconsistent, don't write for now 
                       also proj4 does not seem to use this parameter */                    
                    // dfScale = ( 1.0 + fabs( sin( dfStdP1 * NCDF_PI / 180.0 ) ) ) / 2.0;
                }
                else {
                    if ( ! CPLIsEqual(dfScale,-1.0) ) {
                        /* compute standard_parallel from scale_factor */
                        dfStdP1 = asin( 2*dfScale - 1 ) * 180.0 / NCDF_PI;

                        /* fetch latitude_of_projection_origin (+90/-90) 
                           used here for the sign of standard_parallel */
                        double dfLatProjOrigin = 
                            poDS->FetchCopyParm( szGridMappingValue, 
                                                 CF_PP_LAT_PROJ_ORIGIN, 
                                                 0.0 );
                        if ( ! CPLIsEqual(dfLatProjOrigin,90.0)  &&
                             ! CPLIsEqual(dfLatProjOrigin,-90.0) ) {
                            CPLError( CE_Failure, CPLE_NotSupported, 
                                      "Polar Stereographic must have a %s parameter equal to +90 or -90\n.",
                                      CF_PP_LAT_PROJ_ORIGIN );
                            dfLatProjOrigin = 90.0;
                        }
                        if ( CPLIsEqual(dfLatProjOrigin,-90.0) )
                            dfStdP1 = - dfStdP1;
                    }
                    else {
                        dfStdP1 = 0.0; //just to avoid warning at compilation
                        CPLError( CE_Failure, CPLE_NotSupported, 
                                  "The NetCDF driver does not support import of CF-1 Polar stereographic "
                                  "without standard_parallel and scale_factor_at_projection_origin parameters.\n" );
                    }
                }

                /* set scale to default value 1.0 if it was not set */
                if ( CPLIsEqual(dfScale,-1.0) )
                    dfScale = 1.0;

                dfCenterLon = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_VERT_LONG_FROM_POLE, 0.0 );

                dfFalseEasting = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_EASTING, 0.0 );

                dfFalseNorthing = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_NORTHING, 0.0 );

                bGotCfSRS = TRUE;
                /* map CF CF_PP_STD_PARALLEL_1 to WKT SRS_PP_LATITUDE_OF_ORIGIN */
                oSRS.SetPS( dfStdP1, dfCenterLon, dfScale, 
                            dfFalseEasting, dfFalseNorthing );

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );

                CSLDestroy( papszStdParallels );
            }

/* -------------------------------------------------------------------- */
/*      Stereographic                                                   */
/* -------------------------------------------------------------------- */
		  
            else if ( EQUAL ( pszValue, CF_PT_STEREO ) ) {
	        
                dfCenterLon = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LON_PROJ_ORIGIN, 0.0 );
	      
                dfCenterLat = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_LAT_PROJ_ORIGIN, 0.0 );

                dfScale = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_SCALE_FACTOR_ORIGIN,
                                         1.0 );

                dfFalseEasting = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_EASTING, 0.0 );

                dfFalseNorthing = 
                    poDS->FetchCopyParm( szGridMappingValue, 
                                         CF_PP_FALSE_NORTHING, 0.0 );

                bGotCfSRS = TRUE;
                oSRS.SetStereographic( dfCenterLat, dfCenterLon, dfScale, 
                                       dfFalseEasting, dfFalseNorthing );

                if( !bGotGeogCS )
                    oSRS.SetWellKnownGeogCS( "WGS84" );
            }
  
/* -------------------------------------------------------------------- */
/*      Is this Latitude/Longitude Grid, default                        */
/* -------------------------------------------------------------------- */
	    
        } else if( EQUAL( szDimNameX,"lon" ) ) {
            oSRS.SetWellKnownGeogCS( "WGS84" );

        } else {
            // This would be too indiscrimant.  But we should set
            // it if we know the data is geographic.
            //oSRS.SetWellKnownGeogCS( "WGS84" );
        }
    }
/* -------------------------------------------------------------------- */
/*      Read projection coordinates                                     */
/* -------------------------------------------------------------------- */

    nc_inq_varid( cdfid, poDS->papszDimName[nXDimID], &nVarDimXID );
    nc_inq_varid( cdfid, poDS->papszDimName[nYDimID], &nVarDimYID );
    
    if( ( nVarDimXID != -1 ) && ( nVarDimYID != -1 ) ) {
        pdfXCoord = (double *) CPLCalloc( xdim, sizeof(double) );
        pdfYCoord = (double *) CPLCalloc( ydim, sizeof(double) );

        start[0] = 0;
        edge[0]  = xdim;
        status = nc_get_vara_double( cdfid, nVarDimXID, 
                                     start, edge, pdfXCoord);
        
        edge[0]  = ydim;
        status = nc_get_vara_double( cdfid, nVarDimYID, 
                                     start, edge, pdfYCoord);

/* -------------------------------------------------------------------- */
/*      Check for bottom-up from the Y-axis order                       */
/*      see bugs #4284 and #4251                                        */
/* -------------------------------------------------------------------- */

        if ( pdfYCoord[0] > pdfYCoord[1] )
            poDS->bBottomUp = FALSE;
        else
            poDS->bBottomUp = TRUE;

        CPLDebug( "GDAL_netCDF", "set bBottomUp = %d from Y axis", poDS->bBottomUp );

/* -------------------------------------------------------------------- */
/*      Is pixel spacing is uniform accross the map?                    */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*      Check Longitude                                                 */
/* -------------------------------------------------------------------- */

        nSpacingBegin   = (int) poDS->rint((pdfXCoord[1]-pdfXCoord[0]) * 1000);
	
        nSpacingMiddle  = (int) poDS->rint((pdfXCoord[xdim / 2] - 
                                            pdfXCoord[(xdim / 2) + 1]) * 1000);
	
        nSpacingLast    = (int) poDS->rint((pdfXCoord[xdim - 2] - 
                                            pdfXCoord[xdim-1]) * 1000);
	
        if( ( abs( nSpacingBegin )  ==  abs( nSpacingLast )     )  &&
            ( abs( nSpacingBegin )  ==  abs( nSpacingMiddle )   ) &&
            ( abs( nSpacingMiddle ) ==  abs( nSpacingLast )     ) ) {

/* -------------------------------------------------------------------- */
/*      Longitude is equally spaced, check latitude                     */
/* -------------------------------------------------------------------- */
            nSpacingBegin   = (int) poDS->rint((pdfYCoord[1]-pdfYCoord[0]) * 
                                               1000); 
	    
            nSpacingMiddle  = (int) poDS->rint((pdfYCoord[ydim / 2] - 
                                                pdfYCoord[(ydim / 2) + 1]) * 
                                               1000);

            nSpacingLast    = (int) poDS->rint((pdfYCoord[ydim - 2] - 
                                                pdfYCoord[ydim-1]) * 
                                               1000);

		    
/* -------------------------------------------------------------------- */
/*   For Latitude  we allow an error of 0.1 degrees for gaussian        */
/*   gridding                                                           */
/* -------------------------------------------------------------------- */

            if((( abs( abs(nSpacingBegin) - abs(nSpacingLast) ) )   < 100 ) &&
               (( abs( abs(nSpacingBegin) -  abs(nSpacingMiddle) ) ) < 100 ) &&
               (( abs( abs(nSpacingMiddle) - abs(nSpacingLast) ) )   < 100) ) {

                if( ( abs( nSpacingBegin )  !=  abs( nSpacingLast )     )  ||
                    ( abs( nSpacingBegin )  !=  abs( nSpacingMiddle )   ) ||
                    ( abs( nSpacingMiddle ) !=  abs( nSpacingLast )     ) ) {
		    
                    CPLError(CE_Warning, 1,"Latitude grid not spaced evenly.\nSeting projection for grid spacing is within 0.1 degrees threshold.\n");

                }
/* -------------------------------------------------------------------- */
/*      We have gridded data s we can set the Gereferencing info.       */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*      Enable GeoTransform                                             */
/* -------------------------------------------------------------------- */
                /* ----------------------------------------------------------*/
                /*    In the following "actual_range" and "node_offset"      */
                /*    are attributes used by netCDF files created by GMT.    */
                /*    If we find them we know how to proceed. Else, use      */
                /*    the original algorithm.                                */
                /* --------------------------------------------------------- */
                double	dummy[2], xMinMax[2], yMinMax[2];
                int	node_offset = 0;

                bGotCfGT = TRUE;
                
                nc_get_att_int (cdfid, NC_GLOBAL, "node_offset", &node_offset);
                
                if (!nc_get_att_double (cdfid, nVarDimXID, "actual_range", dummy)) {
                    xMinMax[0] = dummy[0];		
                    xMinMax[1] = dummy[1];
                }
                else {
                    xMinMax[0] = pdfXCoord[0];
                    xMinMax[1] = pdfXCoord[xdim-1];
                    node_offset = 0;
                }

                if (!nc_get_att_double (cdfid, nVarDimYID, "actual_range", dummy)) {
                    yMinMax[0] = dummy[0];		
                    yMinMax[1] = dummy[1];
                }
                else {
                    yMinMax[0] = pdfYCoord[0];	
                    yMinMax[1] = pdfYCoord[ydim-1];
                    node_offset = 0;
                }

                /* Check for reverse order of y-coordinate */
                if ( yMinMax[0] > yMinMax[1] ) {
                    dummy[0] = yMinMax[1];
                    dummy[1] = yMinMax[0];
                    yMinMax[0] = dummy[0];
                    yMinMax[1] = dummy[1];
                }
                
                adfTempGeoTransform[0] = xMinMax[0];
                adfTempGeoTransform[2] = 0;
                adfTempGeoTransform[3] = yMinMax[1];
                adfTempGeoTransform[4] = 0;
                adfTempGeoTransform[1] = ( xMinMax[1] - xMinMax[0] ) / 
                    ( poDS->nRasterXSize + (node_offset - 1) );
                adfTempGeoTransform[5] = ( yMinMax[0] - yMinMax[1] ) / 
                    ( poDS->nRasterYSize + (node_offset - 1) );

/* -------------------------------------------------------------------- */
/*     Compute the center of the pixel                                  */
/* -------------------------------------------------------------------- */
                if ( !node_offset ) {	// Otherwise its already the pixel center
                    adfTempGeoTransform[0] -= (adfTempGeoTransform[1] / 2);
                    adfTempGeoTransform[3] -= (adfTempGeoTransform[5] / 2);
                }
            }// end if (Latitude is equally spaced, within 0.1 degrees)
            else {
                CPLDebug( "GDAL_netCDF", 
                          "Latitude is not equally spaced." );
            }
        }// end if (Longitude is equally spaced)
        else {
            CPLDebug( "GDAL_netCDF", 
                      "Longitude is not equally spaced." );
        }

        CPLFree( pdfXCoord );
        CPLFree( pdfYCoord );
    }// end if (has dims)

/* -------------------------------------------------------------------- */
/*     Set Projection from CF                                           */
/* -------------------------------------------------------------------- */
    if ( bGotGeogCS || bGotCfSRS ) {
        /* Set SRS Units */

        /* check units for x and y */
        if( oSRS.IsProjected( ) ) {
            const char *pszUnitsX = NULL;
            const char *pszUnitsY = NULL;

            strcpy( szTemp, "x" );
            strcat( szTemp, "#units" );
            pszValue = CSLFetchNameValue( poDS->papszMetadata, 
                                          szTemp );
            if( pszValue != NULL ) 
                pszUnitsX = pszValue;

            strcpy( szTemp, "y" );
            strcat( szTemp, "#units" );
            pszValue = CSLFetchNameValue( poDS->papszMetadata, 
                                          szTemp );
            if( pszValue != NULL )
                pszUnitsY = pszValue;

            /* TODO: what to do if units are not equal in X and Y */
            if ( (pszUnitsX != NULL) && (pszUnitsY != NULL) && 
                 EQUAL(pszUnitsX,pszUnitsY) )
                pszUnits = pszUnitsX;

            /* add units to PROJCS */
            if ( pszUnits != NULL && ! EQUAL(pszUnits,"") ) {
                if ( EQUAL(pszUnits,"m") ) {
                    oSRS.SetLinearUnits( CF_UNITS_M, 1.0 );
                    oSRS.SetAuthority( "PROJCS|UNIT", "EPSG", 9001 );
                }
                else if ( EQUAL(pszUnits,"km") ) {
                    oSRS.SetLinearUnits( CF_UNITS_M, 1000.0 );
                    oSRS.SetAuthority( "PROJCS|UNIT", "EPSG", 9001 );
                }
                /* TODO check for other values */
                // else 
                //     oSRS.SetLinearUnits(pszUnits, 1.0);
            }
        }
        else if ( oSRS.IsGeographic() ) {
            oSRS.SetAngularUnits( CF_UNITS_D, CPLAtof(SRS_UA_DEGREE_CONV) );
            oSRS.SetAuthority( "GEOGCS|UNIT", "EPSG", 9122 );
        }
        
        /* Set Projection */
        oSRS.exportToWkt( &(pszTempProjection) );
        CPLDebug( "GDAL_netCDF", "setting WKT from CF" );
        SetProjection( pszTempProjection );
        CPLFree( pszTempProjection );

        if ( !bGotCfGT )
            CPLDebug( "GDAL_netCDF", "got SRS but no geotransform from CF!");
    }

/* -------------------------------------------------------------------- */
/*      Process custom GDAL values (spatial_ref, GeoTransform)          */
/* -------------------------------------------------------------------- */
    if( !EQUAL( szGridMappingValue, "" )  ) {
        
        if( pszWKT != NULL ) {
            
/* -------------------------------------------------------------------- */
/*      Compare SRS obtained from CF attributes and GDAL WKT            */
/*      If possible use the more complete GDAL WKT                      */
/* -------------------------------------------------------------------- */
            /* Set the SRS to the one written by GDAL */
            if ( ! bGotCfSRS || poDS->pszProjection == NULL || ! bIsGdalCfFile ) {   
                bGotGdalSRS = TRUE;
                CPLDebug( "GDAL_netCDF", "setting WKT from GDAL" );
                SetProjection( pszWKT );
            }
            else { /* use the SRS from GDAL if it doesn't conflict with the one from CF */
                char *pszProjectionGDAL = (char*) pszWKT ;
                OGRSpatialReference oSRSGDAL;
                oSRSGDAL.importFromWkt( &pszProjectionGDAL );
                /* set datum to unknown or else datums will not match, see bug #4281 */
                if ( oSRSGDAL.GetAttrNode( "DATUM" ) )
                    oSRSGDAL.GetAttrNode( "DATUM" )->GetChild(0)->SetValue( "unknown" );
                /* need this for setprojection autotest */ 
                if ( oSRSGDAL.GetAttrNode( "PROJCS" ) )
                    oSRSGDAL.GetAttrNode( "PROJCS" )->GetChild(0)->SetValue( "unnamed" );
                if ( oSRSGDAL.GetAttrNode( "GEOGCS" ) )
                    oSRSGDAL.GetAttrNode( "GEOGCS" )->GetChild(0)->SetValue( "unknown" );   
                oSRSGDAL.GetRoot()->StripNodes( "UNIT" );
                if ( oSRS.IsSame(&oSRSGDAL) ) {
                    // printf("ARE SAME, using GDAL WKT\n");
                    bGotGdalSRS = TRUE;
                    CPLDebug( "GDAL_netCDF", "setting WKT from GDAL" );
                    SetProjection( pszWKT );
                }
                else {
                    CPLDebug( "GDAL_netCDF", 
                              "got WKT from GDAL \n[%s]\nbut not using it because conflicts with CF\n[%s]\n", 
                              pszWKT, poDS->pszProjection );
                }
            }

/* -------------------------------------------------------------------- */
/*      Look for GeoTransform Array, if not found in CF                 */
/* -------------------------------------------------------------------- */
            if ( !bGotCfGT ) {

                /* TODO read the GT values and detect for conflict with CF */
                /* this could resolve the GT precision loss issue  */

                if( pszGeoTransform != NULL ) {

                    bGotGdalGT = TRUE;
                    
                    papszGeoTransform = CSLTokenizeString2( pszGeoTransform,
                                                            " ", 
                                                            CSLT_HONOURSTRINGS );
                    adfTempGeoTransform[0] = atof( papszGeoTransform[0] );
                    adfTempGeoTransform[1] = atof( papszGeoTransform[1] );
                    adfTempGeoTransform[2] = atof( papszGeoTransform[2] );
                    adfTempGeoTransform[3] = atof( papszGeoTransform[3] );
                    adfTempGeoTransform[4] = atof( papszGeoTransform[4] );
                    adfTempGeoTransform[5] = atof( papszGeoTransform[5] );
                    
/* -------------------------------------------------------------------- */
/*      Look for corner array values                                    */
/* -------------------------------------------------------------------- */
                } else {
                    double dfNN=0.0, dfSN=0.0, dfEE=0.0, dfWE=0.0;
                    int bGotNN=FALSE, bGotSN=FALSE, bGotEE=FALSE, bGotWE=FALSE;
                    // CPLDebug( "GDAL_netCDF", "looking for geotransform corners\n" );

                    strcpy(szTemp,szGridMappingValue);
                    strcat( szTemp, "#" );
                    strcat( szTemp, "Northernmost_Northing");
                    pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);                    
                    if( pszValue != NULL ) {
                        dfNN = atof( pszValue );
                        bGotNN = TRUE;
                    }

                    strcpy(szTemp,szGridMappingValue);
                    strcat( szTemp, "#" );
                    strcat( szTemp, "Southernmost_Northing");
                    pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);                    
                    if( pszValue != NULL ) {
                        dfSN = atof( pszValue );
                        bGotSN = TRUE;
                    }
                    
                    strcpy(szTemp,szGridMappingValue);
                    strcat( szTemp, "#" );
                    strcat( szTemp, "Easternmost_Easting");
                    pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);                    
                    if( pszValue != NULL ) {
                        dfEE = atof( pszValue );
                        bGotEE = TRUE;
                    }
                    
                    strcpy(szTemp,szGridMappingValue);
                    strcat( szTemp, "#" );
                    strcat( szTemp, "Westernmost_Easting");
                    pszValue = CSLFetchNameValue(poDS->papszMetadata, szTemp);                    
                    if( pszValue != NULL ) {
                        dfWE = atof( pszValue ); 
                        bGotWE = TRUE;
                    }
                    
                    /* Only set the GeoTransform if we got all the values */
                    if (  bGotNN && bGotSN && bGotEE && bGotWE ) {

                        bGotGdalGT = TRUE;

                        adfTempGeoTransform[0] = dfWE;
                        adfTempGeoTransform[1] = (dfEE - dfWE) / 
                            ( poDS->GetRasterXSize() - 1 );
                        adfTempGeoTransform[2] = 0.0;
                        adfTempGeoTransform[3] = dfNN;
                        adfTempGeoTransform[4] = 0.0;
                        adfTempGeoTransform[5] = (dfSN - dfNN) / 
                            ( poDS->GetRasterYSize() - 1 );
                        /* compute the center of the pixel */
                        adfTempGeoTransform[0] = dfWE
                            - (adfTempGeoTransform[1] / 2);                        
                        adfTempGeoTransform[3] = dfNN
                            - (adfTempGeoTransform[5] / 2);
                    }
                } // (pszGeoTransform != NULL)
                CSLDestroy( papszGeoTransform );

                if ( bGotGdalSRS && ! bGotGdalGT )
                    CPLDebug( "GDAL_netCDF", "got SRS but not geotransform from GDAL!");

            } // if ( !bGotCfGT )

        }
    }

    /* Set GeoTransform if we got a complete one - after projection has been set */
    if ( bGotCfGT || bGotGdalGT ) {
        SetGeoTransform( adfTempGeoTransform );
    }
  
    /* debuging reports */
    CPLDebug( "GDAL_netCDF", 
              "bGotGeogCS=%d bGotCfSRS=%d bGotCfGT=%d bGotGdalSRS=%d bGotGdalGT=%d",
              bGotGeogCS, bGotCfSRS, bGotCfGT, bGotGdalSRS, bGotGdalGT );

    if ( !bGotCfGT && !bGotGdalGT )
        CPLDebug( "GDAL_netCDF", "did not get geotransform from CF nor GDAL!");      

    if ( !bGotGeogCS && !bGotCfSRS && !bGotGdalSRS && !bGotCfGT)
        CPLDebug( "GDAL_netCDF",  "did not get projection from CF nor GDAL!");   

/* -------------------------------------------------------------------- */
/*     Search for Well-known GeogCS if got only CF WKT                  */
/*     Disabled for now, as a named datum also include control points   */
/*     (see mailing list and bug#4281                                   */
/*     For example, WGS84 vs. GDA94 (EPSG:3577) - AEA in netcdf_cf.py   */
/* -------------------------------------------------------------------- */
    /* disabled for now, but could be set in a config option */
    bLookForWellKnownGCS = FALSE; 
    if ( bLookForWellKnownGCS && bGotCfSRS && ! bGotGdalSRS ) {
        /* ET - could use a more exhaustive method by scanning all EPSG codes in data/gcs.csv */
        /* as proposed by Even in the gdal-dev mailing list "help for comparing two WKT" */
        /* this code could be contributed to a new function */
        /* OGRSpatialReference * OGRSpatialReference::FindMatchingGeogCS( const OGRSpatialReference *poOther ) */ 
        CPLDebug( "GDAL_netCDF", "Searching for Well-known GeogCS" );
        const char *pszWKGCSList[] = { "WGS84", "WGS72", "NAD27", "NAD83" };
        char *pszWKGCS = NULL;
        oSRS.exportToPrettyWkt( &pszWKGCS );
        for( size_t i=0; i<sizeof(pszWKGCSList)/8; i++ ) {
            pszWKGCS = CPLStrdup( pszWKGCSList[i] );
            OGRSpatialReference oSRSTmp;
            oSRSTmp.SetWellKnownGeogCS( pszWKGCSList[i] );
            /* set datum to unknown, bug #4281 */
            if ( oSRSTmp.GetAttrNode( "DATUM" ) )
                oSRSTmp.GetAttrNode( "DATUM" )->GetChild(0)->SetValue( "unknown" );
            /* could use  OGRSpatialReference::StripCTParms() but let's keep TOWGS84 */
            oSRSTmp.GetRoot()->StripNodes( "AXIS" );
            oSRSTmp.GetRoot()->StripNodes( "AUTHORITY" );
            oSRSTmp.GetRoot()->StripNodes( "EXTENSION" );

            oSRSTmp.exportToPrettyWkt( &pszWKGCS );
            if ( oSRS.IsSameGeogCS(&oSRSTmp) ) {
                oSRS.SetWellKnownGeogCS( pszWKGCSList[i] );
                oSRS.exportToWkt( &(pszTempProjection) );
                SetProjection( pszTempProjection );
                CPLFree( pszTempProjection );
            }
        }
    }


}

/************************************************************************/
/*                          SetProjection()                           */
/************************************************************************/
CPLErr 	netCDFDataset::SetProjection( const char * pszNewProjection )
{
/* TODO look if proj. already defined, like in geotiff */
    if( pszNewProjection == NULL ) 
    {
        CPLError( CE_Failure, CPLE_AppDefined, "NULL projection." );
        return CE_Failure;
    }

    if( bSetProjection && (GetAccess() == GA_Update) ) 
    {
        CPLError( CE_Warning, CPLE_AppDefined,
                  "netCDFDataset::SetProjection() should only be called once "
                  "in update mode!\npszNewProjection=\n%s",
                  pszNewProjection );
    }

    CPLDebug( "GDAL_netCDF", "SetProjection, WKT = %s", pszNewProjection );

    if( !EQUALN(pszNewProjection,"GEOGCS",6)
        && !EQUALN(pszNewProjection,"PROJCS",6)
        && !EQUAL(pszNewProjection,"") )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
                  "Only OGC WKT GEOGCS and PROJCS Projections supported for writing to NetCDF.\n"
                  "%s not supported.",
                  pszNewProjection );
        
        return CE_Failure;
    }
        
    CPLFree( pszProjection );
    pszProjection = CPLStrdup( pszNewProjection );

    if( GetAccess() == GA_Update )
    {
        if ( bSetGeoTransform && ! bSetProjection ) {
            bSetProjection = TRUE;
            return AddProjectionVars();
        }
    }

    bSetProjection = TRUE;

    return CE_None;

}

/************************************************************************/
/*                          SetGeoTransform()                           */
/************************************************************************/

CPLErr 	netCDFDataset::SetGeoTransform ( double * padfTransform )
{
    memcpy( adfGeoTransform, padfTransform, sizeof(double)*6 );
    // bGeoTransformValid = TRUE;
    // bGeoTIFFInfoChanged = TRUE;
    
    CPLDebug( "GDAL_netCDF", 
              "SetGeoTransform(%f,%f,%f,%f,%f,%f)",
              padfTransform[0],padfTransform[1],padfTransform[2],
              padfTransform[3],padfTransform[4],padfTransform[5]);
    
    if( GetAccess() == GA_Update )
    {
        if ( bSetProjection && ! bSetGeoTransform ) {
            bSetGeoTransform = TRUE;
            return AddProjectionVars();
        }
    }

    bSetGeoTransform = TRUE;

    return CE_None;

}

/************************************************************************/
/*                          AddProjectionVars()                         */
/************************************************************************/

CPLErr netCDFDataset::AddProjectionVars( GDALProgressFunc pfnProgress, 
                                         void * pProgressData )
{
    OGRSpatialReference oSRS;
    int NCDFVarID = -1;
    double dfTemp = 0.0;
    const char  *pszValue = NULL;
    char       szTemp[ NCDF_MAX_STR_LEN ];

    char   szGeoTransform[ NCDF_MAX_STR_LEN ];
    *szGeoTransform = '\0';
    char *pszWKT = NULL;    
    const char *pszUnits = NULL;
    char   szUnits[ NCDF_MAX_STR_LEN ];    
    szUnits[0]='\0';

    int  bWriteGridMapping = FALSE;
    int  bWriteLonLat = FALSE;
    int  bWriteGDALTags = FALSE;
    int  bWriteGeoTransform = FALSE;

    nc_type eLonLatType = NC_NAT;
    int nVarLonID=-1, nVarLatID=-1;
    int nVarXID=-1, nVarYID=-1;

    char szNetcdfProjection[ NC_MAX_NAME ];
    szNetcdfProjection[0]='\0';

    bAddedProjectionVars = TRUE;

    pszWKT = (char *) pszProjection;
    oSRS.importFromWkt( &pszWKT );

    if( oSRS.IsProjected() )
        bIsProjected = TRUE;
    else if( oSRS.IsGeographic() )
        bIsGeographic = TRUE;

    CPLDebug( "GDAL_netCDF", "SetProjection, WKT now = [%s]\nprojected: %d geographic: %d", 
              pszProjection,bIsProjected,bIsGeographic );

    if ( ! bSetGeoTransform )
        CPLDebug( "GDAL_netCDF", "netCDFDataset::AddProjectionVars() called, "
                  "but GeoTransform has not yet been defined!" );

    if ( ! bSetProjection )
        CPLDebug( "GDAL_netCDF", "netCDFDataset::AddProjectionVars() called, "
                  "but Projection has not yet been defined!" );

    /* process projection options */
    if( bIsProjected ) 
    {
        int bIsCfProjection = NCDFIsCfProjection( oSRS.GetAttrValue( "PROJECTION" ) );
        bWriteGridMapping = TRUE;
        bWriteGDALTags = CSLFetchBoolean( papszCreationOptions, "WRITE_GDAL_TAGS", TRUE );
        /* force WRITE_GDAL_TAGS if is not a CF projection */
        if ( ! bWriteGDALTags && ! bIsCfProjection )
            bWriteGDALTags = TRUE;
        if ( bWriteGDALTags ) 
            bWriteGeoTransform = TRUE;

        pszValue = CSLFetchNameValueDef(papszCreationOptions,"WRITE_LONLAT", "NO");
        if ( EQUAL( pszValue, "IF_NEEDED" ) ) {
            if  ( bIsCfProjection )
                bWriteLonLat = FALSE;
            else 
                bWriteLonLat = TRUE;
        }
        else bWriteLonLat = CSLTestBoolean( pszValue );

        eLonLatType = NC_FLOAT;
        pszValue =  CSLFetchNameValueDef(papszCreationOptions,"TYPE_LONLAT", "FLOAT");
        if ( EQUAL(pszValue, "DOUBLE" ) ) 
            eLonLatType = NC_DOUBLE;
    }
    else 
    { 
        /* files without a Datum will not have a grid_mapping variable and geographic information */
        if ( bIsGeographic )  bWriteGridMapping = TRUE;
        else  bWriteGridMapping = FALSE;
        bWriteGDALTags = CSLFetchBoolean( papszCreationOptions, "WRITE_GDAL_TAGS", bWriteGridMapping );        
        if ( bWriteGDALTags ) 
            bWriteGeoTransform = TRUE;

        pszValue =  CSLFetchNameValueDef(papszCreationOptions,"WRITE_LONLAT", "YES");
        if ( EQUAL( pszValue, "IF_NEEDED" ) )  
            bWriteLonLat = TRUE;
        else bWriteLonLat = CSLTestBoolean( pszValue );
        /*  Don't write lon/lat if no source geotransform */
        if ( ! bSetGeoTransform )
            bWriteLonLat = FALSE;
        /* If we don't write lon/lat, set dimnames to X/Y and write gdal tags*/
        if ( ! bWriteLonLat ) {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "creating geographic file without lon/lat values!");
            if ( bSetGeoTransform ) {
                bWriteGDALTags = TRUE; //not desireable if no geotransform
                bWriteGeoTransform = TRUE;
            }
        }

        eLonLatType = NC_DOUBLE;
        pszValue =  CSLFetchNameValueDef(papszCreationOptions,"TYPE_LONLAT", "DOUBLE");
        if ( EQUAL(pszValue, "FLOAT" ) ) 
            eLonLatType = NC_FLOAT;
    }
    
    /* make sure we write grid_mapping if we need to write GDAL tags */
    if ( bWriteGDALTags ) bWriteGridMapping = TRUE;

    /* bottom-up value: new driver is bottom-up by default */
    /* override with WRITE_BOTTOMUP */
    bBottomUp = CSLFetchBoolean( papszCreationOptions, "WRITE_BOTTOMUP", TRUE );       
    
    CPLDebug( "GDAL_netCDF", 
              "bIsProjected=%d bIsGeographic=%d bWriteGridMapping=%d bWriteGDALTags=%d bWriteLonLat=%d bBottomUp=%d",
              bIsProjected,bIsGeographic,bWriteGridMapping,bWriteGDALTags,bWriteLonLat,bBottomUp );

    /* exit if nothing to do */
    if ( !bIsProjected && !bWriteLonLat )
        return CE_None;

/* -------------------------------------------------------------------- */
/*      Define dimension names                                          */
/* -------------------------------------------------------------------- */
    /* make sure we are in define mode */
    SetDefineMode( TRUE );


/* -------------------------------------------------------------------- */
/*      Rename dimensions if lon/lat                                    */
/* -------------------------------------------------------------------- */
    if( ! bIsProjected ) 
    {
        /* rename dims to lat/lon */
        papszDimName.Clear(); //if we add other dims one day, this has to change
        papszDimName.AddString( NCDF_DIMNAME_LAT );
        papszDimName.AddString( NCDF_DIMNAME_LON );

        status = nc_rename_dim(cdfid, nYDimID, NCDF_DIMNAME_LAT );
        NCDF_ERR(status);
        status = nc_rename_dim(cdfid, nXDimID, NCDF_DIMNAME_LON );
        NCDF_ERR(status);
    }

/* -------------------------------------------------------------------- */
/*      Write projection attributes                                     */
/* -------------------------------------------------------------------- */
    if( bWriteGridMapping == TRUE ) 
    {
    
        if( bIsProjected ) 
        {
/* -------------------------------------------------------------------- */
/*      Write CF-1.5 compliant Projected attributes                     */
/* -------------------------------------------------------------------- */
 
            const OGR_SRSNode *poPROJCS = oSRS.GetAttrNode( "PROJCS" );
            const char  *pszProjName;
            pszProjName = oSRS.GetAttrValue( "PROJECTION" );

            /* Basic Projection info (grid_mapping and datum) */
            for( int i=0; poNetcdfSRS_PT[i].WKT_SRS != NULL; i++ ) {
                if( EQUAL( poNetcdfSRS_PT[i].WKT_SRS, pszProjName ) ) {
                    CPLDebug( "GDAL_netCDF", "GDAL PROJECTION = %s , NCDF PROJECTION = %s", 
                              poNetcdfSRS_PT[i].WKT_SRS, 
                              poNetcdfSRS_PT[i].CF_SRS);
                    strcpy( szNetcdfProjection, poNetcdfSRS_PT[i].CF_SRS );
                    CPLDebug( "GDAL_netCDF", "nc_def_var(%d,%s,%d)",
                              cdfid, poNetcdfSRS_PT[i].CF_SRS, NC_CHAR ); 
                    status = nc_def_var( cdfid, 
                                         poNetcdfSRS_PT[i].CF_SRS,
                                         NC_CHAR, 
                                         0, NULL, &NCDFVarID );
                    NCDF_ERR(status);
                    break;
                }
            }
            nc_put_att_text( cdfid, NCDFVarID, CF_GRD_MAPPING_NAME,
                             strlen( szNetcdfProjection ),
                             szNetcdfProjection );
            
            /* Various projection attributes */
            // PDS: keep in synch with SetProjection function
            NCDFWriteProjAttribs(poPROJCS, pszProjName, cdfid, NCDFVarID);
            
        }
        else 
        {
/* -------------------------------------------------------------------- */
/*      Write CF-1.5 compliant Geographics attributes                   */
/*      Note: WKT information will not be preserved (e.g. WGS84)        */
/* -------------------------------------------------------------------- */

            strcpy( szNetcdfProjection, "crs" );
            CPLDebug( "GDAL_netCDF", "nc_def_var(%d,%s,%d)",
                      cdfid, szNetcdfProjection, NC_CHAR );
            nc_def_var( cdfid, szNetcdfProjection, NC_CHAR, 
                        0, NULL, &NCDFVarID );
            NCDF_ERR(status);
            nc_put_att_text( cdfid, NCDFVarID, CF_GRD_MAPPING_NAME,
                             strlen(CF_PT_LATITUDE_LONGITUDE),
                             CF_PT_LATITUDE_LONGITUDE );
        }
        
/* -------------------------------------------------------------------- */
/*      Write CF-1.5 compliant common attributes                        */
/* -------------------------------------------------------------------- */

        /* DATUM information */
        dfTemp = oSRS.GetPrimeMeridian();
        nc_put_att_double( cdfid, NCDFVarID, CF_PP_LONG_PRIME_MERIDIAN,
                           NC_DOUBLE, 1, &dfTemp );
        dfTemp = oSRS.GetSemiMajor();
        nc_put_att_double( cdfid, NCDFVarID, CF_PP_SEMI_MAJOR_AXIS,
                           NC_DOUBLE, 1, &dfTemp );
        dfTemp = oSRS.GetInvFlattening();
        nc_put_att_double( cdfid, NCDFVarID, CF_PP_INVERSE_FLATTENING,
                           NC_DOUBLE, 1, &dfTemp );

        /*  Optional GDAL custom projection tags */
        if ( bWriteGDALTags == TRUE ) {
            
            *szGeoTransform = '\0';
            for( int i=0; i<6; i++ ) {
                sprintf( szTemp, "%.16g ",
                         adfGeoTransform[i] );
                strcat( szGeoTransform, szTemp );
            }
            CPLDebug( "GDAL_netCDF", "szGeoTranform = %s", szGeoTransform );
            
            // if ( strlen(pszProj4Defn) > 0 ) {
            //     nc_put_att_text( cdfid, NCDFVarID, "proj4",
            //                      strlen( pszProj4Defn ), pszProj4Defn );
            // }
            nc_put_att_text( cdfid, NCDFVarID, NCDF_SPATIAL_REF,
                             strlen( pszProjection ), pszProjection );
            /* for now write the geotransform for back-compat or else 
               the old (1.8.1) driver overrides the CF geotransform with 
               empty values from dfNN, dfSN, dfEE, dfWE; */
            /* TODO: fix this in 1.8 branch, and then remove this here */
            if ( bWriteGeoTransform && bSetGeoTransform ) {
                nc_put_att_text( cdfid, NCDFVarID, NCDF_GEOTRANSFORM,
                                 strlen( szGeoTransform ),
                                 szGeoTransform );
            }
        }

/* -------------------------------------------------------------------- */
/*      Write Projection var in Bands                                   */
/* -------------------------------------------------------------------- */

        for( int i=1; i <= nBands; i++ ) {
            netCDFRasterBand *poSrcBand = 
                (netCDFRasterBand *) GetRasterBand( i );
            status = nc_put_att_text( cdfid, poSrcBand->nZId, 
                                      CF_GRD_MAPPING,
                                      strlen( szNetcdfProjection ),
                                      szNetcdfProjection );
            NCDF_ERR(status);
            if ( bWriteLonLat == TRUE ) {
                status = nc_put_att_text( cdfid, poSrcBand->nZId, 
                                          CF_COORDINATES,
                                          strlen( NCDF_LONLAT ), NCDF_LONLAT );
                NCDF_ERR(status);
            }
        }           

    }  /* end if( bWriteGridMapping ) */

    pfnProgress( 0.10, NULL, pProgressData );    

/* -------------------------------------------------------------------- */
/*      Write CF Projection vars                                        */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*      Write X/Y attributes                                            */
/* -------------------------------------------------------------------- */
    if( bIsProjected )
    {
        pszUnits = oSRS.GetAttrValue("PROJCS|UNIT",1);
        if ( pszUnits == NULL || EQUAL(pszUnits,"1") ) 
            strcpy(szUnits,"m");
        else if ( EQUAL(pszUnits,"1000") ) 
            strcpy(szUnits,"km");

        /* X */
        int anXDims[1];
        anXDims[0] = nXDimID;
        CPLDebug( "GDAL_netCDF", "nc_def_var(%d,%s,%d)",
                  cdfid, NCDF_DIMNAME_X, NC_DOUBLE );
        status = nc_def_var( cdfid, NCDF_DIMNAME_X, NC_DOUBLE, 
                             1, anXDims, &NCDFVarID );
        NCDF_ERR(status);
        nVarXID=NCDFVarID;
        nc_put_att_text( cdfid, NCDFVarID, CF_STD_NAME,
                         strlen(CF_PROJ_X_COORD),
                         CF_PROJ_X_COORD );
        nc_put_att_text( cdfid, NCDFVarID, CF_LNG_NAME,
                         strlen("x coordinate of projection"),
                         "x coordinate of projection" );
        nc_put_att_text( cdfid, NCDFVarID, CF_UNITS, strlen(szUnits), szUnits ); 

        /* Y */
        int anYDims[1];
        anYDims[0] = nYDimID;
        CPLDebug( "GDAL_netCDF", "nc_def_var(%d,%s,%d)",
                  cdfid, NCDF_DIMNAME_Y, NC_DOUBLE );
        status = nc_def_var( cdfid, NCDF_DIMNAME_Y, NC_DOUBLE, 
                             1, anYDims, &NCDFVarID );
        NCDF_ERR(status);
        nVarYID=NCDFVarID;
        nc_put_att_text( cdfid, NCDFVarID, CF_STD_NAME,
                         strlen(CF_PROJ_Y_COORD),
                         CF_PROJ_Y_COORD );
        nc_put_att_text( cdfid, NCDFVarID, CF_LNG_NAME,
                         strlen("y coordinate of projection"),
                         "y coordinate of projection" );
        nc_put_att_text( cdfid, NCDFVarID, CF_UNITS, strlen(szUnits), szUnits ); 
    }

/* -------------------------------------------------------------------- */
/*      Write lat/lon attributes if needed                              */
/* -------------------------------------------------------------------- */
    if ( bWriteLonLat == TRUE ) {

        /* latitude attributes */
        if ( bIsProjected ) {
            int anLatDims[2];
            anLatDims[0] =  nYDimID;
            anLatDims[1] =  nXDimID;
            CPLDebug( "GDAL_netCDF", "nc_def_var(%d,%s,%d)",
                      cdfid, NCDF_DIMNAME_LAT, eLonLatType );
            status = nc_def_var( cdfid, NCDF_DIMNAME_LAT, eLonLatType, 
                                 2, anLatDims, &NCDFVarID );
            NCDF_ERR(status);
            /* compress lon/lat to save space */
            DefVarDeflate( NCDFVarID );
        }
        else {
            int anLatDims[1];
            // anLatDims[0] = nLatDimID;
            anLatDims[0] =  nYDimID;
            CPLDebug( "GDAL_netCDF", "nc_def_var(%d,%s,%d)",
                      cdfid, NCDF_DIMNAME_LAT, eLonLatType );
            status = nc_def_var( cdfid, NCDF_DIMNAME_LAT, eLonLatType, 
                                 1, anLatDims, &NCDFVarID );  
            NCDF_ERR(status);                
        }
        nVarLatID = NCDFVarID;
        nc_put_att_text( cdfid, NCDFVarID, CF_STD_NAME,
                         8,"latitude" );
        nc_put_att_text( cdfid, NCDFVarID, CF_LNG_NAME,
                         8, "latitude" );
        nc_put_att_text( cdfid, NCDFVarID, CF_UNITS,
                         13, "degrees_north" );

        /* longitude attributes */
        if ( bIsProjected ) {
            int anLonDims[2];
            anLonDims[0] = nYDimID;
            anLonDims[1] = nXDimID;
            CPLDebug( "GDAL_netCDF", "nc_def_var(%d,%s,%d)",
                      cdfid, NCDF_DIMNAME_LON, eLonLatType );
            status = nc_def_var( cdfid, NCDF_DIMNAME_LON, eLonLatType, 
                                 2, anLonDims, &NCDFVarID );
            NCDF_ERR(status);
            /* compress lon/lat to save space */
            DefVarDeflate( NCDFVarID );
        }
        else {
            int anLonDims[1];
            // anLonDims[0] = nLonDimID;
            anLonDims[0] = nXDimID;
            CPLDebug( "GDAL_netCDF", "nc_def_var(%d,%s,%d)",
                      cdfid, NCDF_DIMNAME_LON, eLonLatType );
            status = nc_def_var( cdfid, NCDF_DIMNAME_LON, eLonLatType, 
                                 1, anLonDims, &NCDFVarID );
            NCDF_ERR(status);
        }
        nVarLonID = NCDFVarID;
        nc_put_att_text( cdfid, NCDFVarID, CF_STD_NAME,
                         9, "longitude" );
        nc_put_att_text( cdfid, NCDFVarID, CF_LNG_NAME,
                         9, "longitude" );
        nc_put_att_text( cdfid, NCDFVarID, CF_UNITS,
                         12, "degrees_east" );
    }

    pfnProgress( 0.50, NULL, pProgressData );

/* -------------------------------------------------------------------- */
/*      Get projection values                                           */
/* -------------------------------------------------------------------- */

    double dfX0, dfDX, dfY0, dfDY;
    dfX0=0.0, dfDX=0.0, dfY0=0.0, dfDY=0.0;
    double *padLonVal = NULL;
    double *padLatVal = NULL; /* should use float for projected, save space */

    if( bIsProjected )
    {
        // const char  *pszProjection;
        OGRSpatialReference oSRS;
        OGRSpatialReference *poLatLonSRS = NULL;
        OGRCoordinateTransformation *poTransform = NULL;

        char *pszWKT = (char *) pszProjection;
        oSRS.importFromWkt( &pszWKT );

        double *padYVal = NULL;
        double *padXVal = NULL;
        size_t startX[1];
        size_t countX[1];
        size_t startY[1];
        size_t countY[1];

        CPLDebug("GDAL_netCDF", "Getting (X,Y) values" );

        padXVal = (double *) CPLMalloc( nRasterXSize * sizeof( double ) );
        padYVal = (double *) CPLMalloc( nRasterYSize * sizeof( double ) );

/* -------------------------------------------------------------------- */
/*      Get Y values                                                    */
/* -------------------------------------------------------------------- */
        if ( ! bBottomUp )
            dfY0 = adfGeoTransform[3];
        else /* invert latitude values */ 
            dfY0 = adfGeoTransform[3] + ( adfGeoTransform[5] * nRasterYSize );
        dfDY = adfGeoTransform[5];
        
        for( int j=0; j<nRasterYSize; j++ ) {
            /* The data point is centered inside the pixel */
            if ( ! bBottomUp )
                padYVal[j] = dfY0 + (j+0.5)*dfDY ;
            else /* invert latitude values */ 
                padYVal[j] = dfY0 - (j+0.5)*dfDY ;            
        }
        startX[0] = 0;
        countX[0] = nRasterXSize;

/* -------------------------------------------------------------------- */
/*      Get X values                                                    */
/* -------------------------------------------------------------------- */
        dfX0 = adfGeoTransform[0];
        dfDX = adfGeoTransform[1];

        for( int i=0; i<nRasterXSize; i++ ) {
            /* The data point is centered inside the pixel */
            padXVal[i] = dfX0 + (i+0.5)*dfDX ;
        }
        startY[0] = 0;
        countY[0] = nRasterYSize;

/* -------------------------------------------------------------------- */
/*      Write X/Y values                                                */
/* -------------------------------------------------------------------- */
        /* make sure we are in data mode */
        SetDefineMode( FALSE );

        CPLDebug("GDAL_netCDF", "Writing X values" );
        status = nc_put_vara_double( cdfid, nVarXID, startX,
                                     countX, padXVal);
        NCDF_ERR(status);

        CPLDebug("GDAL_netCDF", "Writing Y values" );
        status = nc_put_vara_double( cdfid, nVarYID, startY,
                                     countY, padYVal);
        NCDF_ERR(status);

        pfnProgress( 0.20, NULL, pProgressData );

/* -------------------------------------------------------------------- */
/*      Transform (X,Y) values to (lon,lat)                             */
/* -------------------------------------------------------------------- */

        /* Get OGR transform */
        if ( bWriteLonLat == TRUE ) {
            poLatLonSRS = oSRS.CloneGeogCS();
            if ( poLatLonSRS != NULL )
                poTransform = OGRCreateCoordinateTransformation( &oSRS, poLatLonSRS );
            /* if no OGR transform, then don't write CF lon/lat */
            if( poTransform == NULL ) {
                CPLError( CE_Failure, CPLE_AppDefined, 
                          "Unable to get Coordinate Transform" );
                bWriteLonLat = FALSE;
            }
        }
            
        if ( bWriteLonLat == TRUE )  {

            CPLDebug("GDAL_netCDF", "Transforming (X,Y)->(lon,lat)" );

            padLatVal = (double *) CPLMalloc( nRasterXSize * sizeof( double ) );
            padLonVal = (double *) CPLMalloc( nRasterXSize * sizeof( double ) );
            size_t start[2], count[2];
            start[1] = 0; //X
            count[1] = nRasterXSize; 
            start[0] = 0; //Y
            count[0] = 1;

            int bOK = TRUE;
            double dfTemp;
            int i,j;
            dfTemp = 0.2;

            for( j = 0; (j < nRasterYSize) && bOK && (status == NC_NOERR); j++ ) {
                
                start[0] = j;

                /* fill values to transform */
                for( i=0; i<nRasterXSize; i++ ) {
                    padLatVal[i] = padYVal[j];
                    padLonVal[i] = padXVal[i];
                }
                
                /* do the transform */
                bOK = poTransform->Transform( nRasterXSize, 
                                              padLonVal, padLatVal, NULL );
                /* write data */
                if ( bOK ) {
                    status = nc_put_vara_double( cdfid, nVarLatID, start,
                                                 count, padLatVal);
                    NCDF_ERR(status);
                    status = nc_put_vara_double( cdfid, nVarLonID, start,
                                                 count, padLonVal);
                    NCDF_ERR(status);
                }
                else 
                    CPLError( CE_Failure, CPLE_AppDefined, 
                              "Unable to Transform (X,Y) to (lon,lat).\n" );

                if ( j % (nRasterYSize/10) == 0 ) {
                    dfTemp += 0.08;
                    pfnProgress( dfTemp , NULL, pProgressData );
                }
            }

        }

        /* Free the srs and transform objects */
        if ( poLatLonSRS != NULL ) CPLFree( poLatLonSRS );
        if ( poTransform != NULL ) CPLFree( poTransform );

        /* Free data */
        CPLFree( padXVal );
        CPLFree( padYVal );
        CPLFree( padLonVal );
        CPLFree( padLatVal);

    } // projected

    /* If not Projected assume Geographic to catch grids without Datum */
    else if ( bWriteLonLat == TRUE )  {  
        	
/* -------------------------------------------------------------------- */
/*      Get latitude values                                             */
/* -------------------------------------------------------------------- */
        if ( ! bBottomUp )
            dfY0 = adfGeoTransform[3];
        else /* invert latitude values */ 
            dfY0 = adfGeoTransform[3] + ( adfGeoTransform[5] * nRasterYSize );
        dfDY = adfGeoTransform[5];
        
        padLatVal = (double *) CPLMalloc( nRasterYSize * sizeof( double ) );
        for( int i=0; i<nRasterYSize; i++ ) {
            /* The data point is centered inside the pixel */
            if ( ! bBottomUp )
                padLatVal[i] = dfY0 + (i+0.5)*dfDY ;
            else /* invert latitude values */ 
                padLatVal[i] = dfY0 - (i+0.5)*dfDY ;
        }
        
        size_t startLat[1];
        size_t countLat[1];
        startLat[0] = 0;
        countLat[0] = nRasterYSize;
                
/* -------------------------------------------------------------------- */
/*      Get longitude values                                            */
/* -------------------------------------------------------------------- */
        dfX0 = adfGeoTransform[0];
        dfDX = adfGeoTransform[1];
        
        padLonVal = (double *) CPLMalloc( nRasterXSize * sizeof( double ) );
        for( int i=0; i<nRasterXSize; i++ ) {
            /* The data point is centered inside the pixel */
            padLonVal[i] = dfX0 + (i+0.5)*dfDX ;
        }
        
        size_t startLon[1];
        size_t countLon[1];
        startLon[0] = 0;
        countLon[0] = nRasterXSize;

/* -------------------------------------------------------------------- */
/*      Write latitude and longitude values                             */
/* -------------------------------------------------------------------- */
        /* latitude values */
        CPLDebug("GDAL_netCDF", "Writing lat values" );

        /* make sure we are in data mode */
        SetDefineMode( FALSE );

        status = nc_put_vara_double( cdfid, nVarLatID, startLat,
                                     countLat, padLatVal);
        NCDF_ERR(status);

        /* free values */
        CPLFree( padLatVal );  

        /* longitude values */
        CPLDebug("GDAL_netCDF", "Writing lon values" );
        status = nc_put_vara_double( cdfid, nVarLonID, startLon,
                                     countLon, padLonVal);
        NCDF_ERR(status);
        
        /* free values */
        CPLFree( padLonVal );  
        
    }// not projected 
            

    pfnProgress( 1.00, NULL, pProgressData );

    return CE_None;
}

/************************************************************************/
/*                          GetGeoTransform()                           */
/************************************************************************/

CPLErr netCDFDataset::GetGeoTransform( double * padfTransform )

{
    memcpy( padfTransform, adfGeoTransform, sizeof(double) * 6 );
    if( bSetGeoTransform )
        return CE_None;
    else
        return GDALPamDataset::GetGeoTransform( padfTransform );
}

/************************************************************************/
/*                                rint()                                */
/************************************************************************/

double netCDFDataset::rint( double dfX)
{
    if( dfX > 0 ) {
        int nX = (int) (dfX+0.5);
        if( nX % 2 ) {
            double dfDiff = dfX - (double)nX;
            if( dfDiff == -0.5 )
                return double( nX-1 );
        }
        return double( nX );
    } else {
        int nX= (int) (dfX-0.5);
        if( nX % 2 ) {
            double dfDiff = dfX - (double)nX;
            if( dfDiff == 0.5 )
                return double(nX+1);
        }
        return double(nX);
    }
}

/************************************************************************/
/*                        ReadAttributes()                              */
/************************************************************************/
CPLErr netCDFDataset::ReadAttributes( int cdfid, int var)

{
    char    szAttrName[ NC_MAX_NAME ];
    char    szVarName [ NC_MAX_NAME ];
    char    szMetaName[ NC_MAX_NAME * 2 ];
    char    *pszMetaTemp = NULL;
    int     nbAttr;

    nc_inq_varnatts( cdfid, var, &nbAttr );
    if( var == NC_GLOBAL ) {
        strcpy( szVarName,"NC_GLOBAL" );
    }
    else {
        nc_inq_varname( cdfid, var, szVarName );
    }

    for( int l=0; l < nbAttr; l++) {
	
        nc_inq_attname( cdfid, var, l, szAttrName);
        sprintf( szMetaName, "%s#%s", szVarName, szAttrName  );

        if ( NCDFGetAttr( cdfid, var, szAttrName, &pszMetaTemp )
             == CE_None ) {
            papszMetadata = CSLSetNameValue(papszMetadata, 
                                            szMetaName, 
                                            pszMetaTemp);
            CPLFree(pszMetaTemp);
            pszMetaTemp = NULL;
        }
        else {
            CPLDebug( "GDAL_netCDF", "invalid global metadata %s", szMetaName );
        }

    }
	
    return CE_None;

}


/************************************************************************/
/*                netCDFDataset::CreateSubDatasetList()                 */
/************************************************************************/
void netCDFDataset::CreateSubDatasetList( )
{

    char         szDim[ MAX_NC_NAME ];
    char         szTemp[ MAX_NC_NAME ];
    char         szType[ MAX_NC_NAME ];
    char         szName[ MAX_NC_NAME ];
    char         szVarStdName[ MAX_NC_NAME ];
    int          nDims;
    int          nVar;
    int          nVarCount;
    int          i;
    nc_type      nVarType;
    int          *ponDimIds;
    size_t       nDimLen;
    int          nSub;
    nc_type      nAttype;
    size_t       nAttlen;

    netCDFDataset 	*poDS;
    poDS = this;

    nSub=1;
    nc_inq_nvars ( cdfid, &nVarCount );
    for ( nVar = 0; nVar < nVarCount; nVar++ ) {

        nc_inq_varndims ( cdfid, nVar, &nDims );
        if( nDims >= 2 ) {
            ponDimIds = (int *) CPLCalloc( nDims, sizeof( int ) );
            nc_inq_vardimid ( cdfid, nVar, ponDimIds );
	    
/* -------------------------------------------------------------------- */
/*      Create Sub dataset list                                         */
/* -------------------------------------------------------------------- */
            szDim[0]='\0';
            for( i = 0; i < nDims; i++ ) {
                nc_inq_dimlen ( cdfid, ponDimIds[i], &nDimLen );
                sprintf(szTemp, "%d", (int) nDimLen);
                strcat(szTemp,  "x" );
                strcat(szDim,   szTemp);
            }

            nc_inq_vartype( cdfid, nVar, &nVarType );
/* -------------------------------------------------------------------- */
/*      Get rid of the last "x" character                               */
/* -------------------------------------------------------------------- */
            szDim[strlen(szDim) - 1] = '\0';
            switch( nVarType ) {
		
                case NC_BYTE:
#ifdef NETCDF_HAS_NC4
                case NC_UBYTE:
#endif    
                case NC_CHAR:
                    strcpy(szType, "8-bit character");
                    break;

                case NC_SHORT: 
                    strcpy(szType, "8-bit integer");
                    break;
                case NC_INT:
                    strcpy(szType, "16-bit integer");
                    break;
                case NC_FLOAT:
                    strcpy(szType, "32-bit floating-point");
                    break;
                case NC_DOUBLE:
                    strcpy(szType, "64-bit floating-point");
                    break;

                default:
                    break;
            }
            nc_inq_varname( cdfid, nVar, szName);
            nc_inq_att( cdfid, nVar, CF_STD_NAME, &nAttype, &nAttlen);
            if( nc_get_att_text ( cdfid, nVar, CF_STD_NAME, 
                                  szVarStdName ) == NC_NOERR ) {
                szVarStdName[nAttlen] = '\0';
            }
            else {
                strcpy( szVarStdName, szName );
            }
    
            sprintf( szTemp, "SUBDATASET_%d_NAME", nSub) ;
	    
            poDS->papszSubDatasets =
                CSLSetNameValue( poDS->papszSubDatasets, szTemp,
                                 CPLSPrintf( "NETCDF:\"%s\":%s",
                                             poDS->osFilename.c_str(),
                                             szName) ) ;
	    
            sprintf(  szTemp, "SUBDATASET_%d_DESC", nSub++ );

            poDS->papszSubDatasets =
                CSLSetNameValue( poDS->papszSubDatasets, szTemp,
                                 CPLSPrintf( "[%s] %s (%s)", 
                                             szDim,
                                             szVarStdName,
                                             szType ) );
            CPLFree(ponDimIds);
        }
    }

}
    
/************************************************************************/
/*                              IdentifyFormat()                      */
/************************************************************************/

int netCDFDataset::IdentifyFormat( GDALOpenInfo * poOpenInfo, bool bCheckExt = TRUE )

{
/* -------------------------------------------------------------------- */
/*      Does this appear to be a netcdf file? If so, which format?      */
/*      http://www.unidata.ucar.edu/software/netcdf/docs/faq.html#fv1_5 */
/* -------------------------------------------------------------------- */

    if( EQUALN(poOpenInfo->pszFilename,"NETCDF:",7) )
        return NCDF_FORMAT_UNKNOWN;
    if ( poOpenInfo->nHeaderBytes < 4 )
        return NCDF_FORMAT_NONE;
    if ( EQUALN((char*)poOpenInfo->pabyHeader,"CDF\001",4) )
        return NCDF_FORMAT_NC;
    else if ( EQUALN((char*)poOpenInfo->pabyHeader,"CDF\002",4) )
        return NCDF_FORMAT_NC2;
    else if ( EQUALN((char*)poOpenInfo->pabyHeader,"\211HDF\r\n\032\n",8) ) {
        /* Requires netCDF-4/HDF5 support in libnetcdf (not just libnetcdf-v4).
           If HDF5 is not supported in GDAL, this driver will try to open the file 
           Else, make sure this driver does not try to open HDF5 files 
           If user really wants to open with this driver, use NETCDF:file.h5 format. 
           This check should be relaxed, but there is no clear way to make a difference. 
        */

        /* Check for HDF5 support in GDAL */
#ifdef HAVE_HDF5
        if ( bCheckExt ) { /* Check by default */
            const char* pszExtension = CPLGetExtension( poOpenInfo->pszFilename );
            if ( ! ( EQUAL( pszExtension, "nc")  || EQUAL( pszExtension, "cdf") 
                     || EQUAL( pszExtension, "nc2") || EQUAL( pszExtension, "nc4") ) )
                return NCDF_FORMAT_HDF5;
        }
#endif

        /* Check for netcdf-4 support in libnetcdf */
#ifdef NETCDF_HAS_NC4
        return NCDF_FORMAT_NC4;
#else
        return NCDF_FORMAT_HDF5;
#endif

    }
    else if ( EQUALN((char*)poOpenInfo->pabyHeader,"\016\003\023\001",4) ) {
        /* Requires HDF4 support in libnetcdf, but if HF4 is supported by GDAL don't try to open. */
        /* If user really wants to open with this driver, use NETCDF:file.hdf syntax. */

        /* Check for HDF4 support in GDAL */
#ifdef HAVE_HDF4
        if ( bCheckExt ) { /* Check by default */
            /* Always treat as HDF4 file */
            return NCDF_FORMAT_HDF4;
        }
#endif

        /* Check for HDF4 support in libnetcdf */
#ifdef NETCDF_HAS_HDF4
        return NCDF_FORMAT_NC4; 
#else
        return NCDF_FORMAT_HDF4;
#endif
    }

    return NCDF_FORMAT_NONE;
} 

/************************************************************************/
/*                              Identify()                              */
/************************************************************************/

int netCDFDataset::Identify( GDALOpenInfo * poOpenInfo )

{
    if( EQUALN(poOpenInfo->pszFilename,"NETCDF:",7) ) {
        return TRUE;
    }
    int nTmpFormat = IdentifyFormat( poOpenInfo );
    if( NCDF_FORMAT_NC  == nTmpFormat ||
        NCDF_FORMAT_NC2  == nTmpFormat ||
        NCDF_FORMAT_NC4  == nTmpFormat ||
        NCDF_FORMAT_NC4C  == nTmpFormat )
        return TRUE;
    else
        return FALSE;
} 

/************************************************************************/
/*                                Open()                                */
/************************************************************************/

GDALDataset *netCDFDataset::Open( GDALOpenInfo * poOpenInfo )
    
{
    int          j;
    unsigned int k;
    int          nd;
    int          cdfid, dim_count, var, var_count;
    int          i = 0;
    size_t       lev_count;
    size_t       nTotLevCount = 1;
    int          nDim = 2;
    int          status;
    int          nDimID;
    char         attname[NC_MAX_NAME];
    int          ndims, nvars, ngatts, unlimdimid;
    int          nCount=0;
    int          nVarID=-1;

    int          nTmpFormat=NCDF_FORMAT_NONE;
    int          *panBandDimPos;         // X, Y, Z postion in array
    int          *panBandZLev;
    int          *paDimIds;
    size_t        xdim, ydim;
    char         szTemp[NC_MAX_NAME];

    CPLString    osSubdatasetName;
    int          bTreatAsSubdataset;

/* -------------------------------------------------------------------- */
/*      Does this appear to be a netcdf file?                           */
/* -------------------------------------------------------------------- */
    if( ! EQUALN(poOpenInfo->pszFilename,"NETCDF:",7) ) {
        nTmpFormat = IdentifyFormat( poOpenInfo );
        /* Note: not calling Identify() directly, because we want the file type */
        /* Only support NCDF_FORMAT* formats */
        if( ! ( NCDF_FORMAT_NC  == nTmpFormat ||
                NCDF_FORMAT_NC2  == nTmpFormat ||
                NCDF_FORMAT_NC4  == nTmpFormat ||
                NCDF_FORMAT_NC4C  == nTmpFormat ) )
            return NULL;
    }

    netCDFDataset 	*poDS;
    poDS = new netCDFDataset();

/* -------------------------------------------------------------------- */
/*      Disable PAM, at least temporarily. See bug #4244                */
/* -------------------------------------------------------------------- */
    poDS->nPamFlags |= GPF_DISABLED;


    poDS->SetDescription( poOpenInfo->pszFilename );
    
/* -------------------------------------------------------------------- */
/*       Check if filename start with NETCDF: tag                       */
/* -------------------------------------------------------------------- */
    if( EQUALN( poOpenInfo->pszFilename,"NETCDF:",7) )
    {
        char **papszName =
            CSLTokenizeString2( poOpenInfo->pszFilename,
                                ":", CSLT_HONOURSTRINGS|CSLT_PRESERVEESCAPES );
        
        /* -------------------------------------------------------------------- */
        /*    Check for drive name in windows NETCDF:"D:\...                    */
        /* -------------------------------------------------------------------- */
        if ( CSLCount(papszName) == 4 &&
             strlen(papszName[1]) == 1 &&
             (papszName[2][0] == '/' || papszName[2][0] == '\\') )
        {
            poDS->osFilename = papszName[1];
            poDS->osFilename += ':';
            poDS->osFilename += papszName[2];
            osSubdatasetName = papszName[3];
            bTreatAsSubdataset = TRUE;
            CSLDestroy( papszName );
        }
        else if( CSLCount(papszName) == 3 )
        {
            poDS->osFilename = papszName[1];
            osSubdatasetName = papszName[2];
            bTreatAsSubdataset = TRUE;
            CSLDestroy( papszName );
    	}
        else if( CSLCount(papszName) == 2 )
        {
            poDS->osFilename = papszName[1];
            osSubdatasetName = "";
            bTreatAsSubdataset = FALSE;
            CSLDestroy( papszName );
    	}
        else
        {
            CSLDestroy( papszName );
            delete poDS;
            CPLError( CE_Failure, CPLE_AppDefined,
                      "Failed to parse NETCDF: prefix string into expected 2, 3 or 4 fields." );
            return NULL;
        }
        /* Identify Format from real file, with bCheckExt=FALSE */ 
        GDALOpenInfo* poOpenInfo2 = new GDALOpenInfo(poDS->osFilename.c_str(), GA_ReadOnly );
        poDS->nFormat = IdentifyFormat( poOpenInfo2, FALSE );
        delete poOpenInfo2;
        if( NCDF_FORMAT_NONE == poDS->nFormat ||
            NCDF_FORMAT_UNKNOWN == poDS->nFormat ) {
            delete poDS;
            return NULL;
        }        
    }
    else 
    {
        poDS->osFilename = poOpenInfo->pszFilename;
        bTreatAsSubdataset = FALSE;
        poDS->nFormat = nTmpFormat;
    }

/* -------------------------------------------------------------------- */
/*      Try opening the dataset.                                        */
/* -------------------------------------------------------------------- */
    CPLDebug( "GDAL_netCDF", "\n=====\ncalling nc_open( %s )\n", poDS->osFilename.c_str() );
    if( nc_open( poDS->osFilename, NC_NOWRITE, &cdfid ) != NC_NOERR ) {
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Is this a real netCDF file?                                     */
/* -------------------------------------------------------------------- */
    status = nc_inq(cdfid, &ndims, &nvars, &ngatts, &unlimdimid);
    if( status != NC_NOERR ) {
        delete poDS;
        return NULL;
    }   

/* -------------------------------------------------------------------- */
/*      Get file type from netcdf                                       */
/* -------------------------------------------------------------------- */
    status = nc_inq_format (cdfid, &nTmpFormat);
    if ( status != NC_NOERR ) {
        NCDF_ERR(status);
    }
    else {
        CPLDebug( "GDAL_netCDF", 
                  "driver detected file type=%d, libnetcdf detected type=%d",
                  poDS->nFormat, nTmpFormat );
        if ( nTmpFormat != poDS->nFormat ) {
            /* warn if file detection conflicts with that from libnetcdf */
            /* except for NC4C, which we have no way of detecting initially */
            if ( nTmpFormat != NCDF_FORMAT_NC4C ) {
                CPLError( CE_Warning, CPLE_AppDefined, 
                          "NetCDF driver detected file type=%d, but libnetcdf detected type=%d",
                          poDS->nFormat, nTmpFormat );
            }
            CPLDebug( "GDAL_netCDF", "seting file type to %d, was %d", 
                      nTmpFormat, poDS->nFormat );
            poDS->nFormat = nTmpFormat;
        }
    }

/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "The NETCDF driver does not support update access to existing"
                  " datasets.\n" );
        nc_close( cdfid );
        delete poDS;
        return NULL;
    }
    
/* -------------------------------------------------------------------- */
/*      Does the request variable exist?                                */
/* -------------------------------------------------------------------- */
    if( bTreatAsSubdataset )
    {
        status = nc_inq_varid( cdfid, osSubdatasetName, &var);
        if( status != NC_NOERR ) {
            CPLError( CE_Warning, CPLE_AppDefined, 
                      "%s is a netCDF file, but %s is not a variable.",
                      poOpenInfo->pszFilename, 
                      osSubdatasetName.c_str() );
            
            nc_close( cdfid );
            delete poDS;
            return NULL;
        }
    }

    if( nc_inq_ndims( cdfid, &dim_count ) != NC_NOERR || dim_count < 2 )
    {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "%s is a netCDF file, but not in GMT configuration.",
                  poOpenInfo->pszFilename );

        nc_close( cdfid );
        delete poDS;
        return NULL;
    }

    CPLDebug( "GDAL_netCDF", "dim_count = %d", dim_count );

    if( (status = nc_get_att_text( cdfid, NC_GLOBAL, "Conventions",
                                   attname )) != NC_NOERR ) {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "No UNIDATA NC_GLOBAL:Conventions attribute");
        /* note that 'Conventions' is always capital 'C' in CF spec*/
    }


/* -------------------------------------------------------------------- */
/*      Create band information objects.                                */
/* -------------------------------------------------------------------- */
    if ( nc_inq_nvars ( cdfid, &var_count) != NC_NOERR )
    {
        delete poDS;
        return NULL;
    }    
    
    CPLDebug( "GDAL_netCDF", "var_count = %d", var_count );

/* -------------------------------------------------------------------- */
/*      Create a corresponding GDALDataset.                             */
/*      Create Netcdf Subdataset if filename as NETCDF tag              */
/* -------------------------------------------------------------------- */
    poDS->cdfid = cdfid;

    poDS->ReadAttributes( cdfid, NC_GLOBAL );	

/* -------------------------------------------------------------------- */
/*  Verify if only one variable has 2 dimensions                        */
/* -------------------------------------------------------------------- */
    for ( j = 0; j < nvars; j++ ) {

        nc_inq_varndims ( cdfid, j, &ndims );
        if( ndims >= 2 ) {
            nVarID=j;
            nCount++;
        }
    }

/* -------------------------------------------------------------------- */
/*      We have more than one variable with 2 dimensions in the         */
/*      file, then treat this as a subdataset container dataset.        */
/* -------------------------------------------------------------------- */
    if( (nCount > 1) && !bTreatAsSubdataset )
    {
        poDS->CreateSubDatasetList();
        poDS->SetMetadata( poDS->papszMetadata );
        poDS->TryLoadXML();
        return( poDS );
    }

/* -------------------------------------------------------------------- */
/*      If we are not treating things as a subdataset, then capture     */
/*      the name of the single available variable as the subdataset.    */
/* -------------------------------------------------------------------- */
    if( !bTreatAsSubdataset ) // nCount must be 1!
    {
        char szVarName[NC_MAX_NAME];

        nc_inq_varname( cdfid, nVarID, szVarName);
        osSubdatasetName = szVarName;
    }

/* -------------------------------------------------------------------- */
/*      Open the NETCDF subdataset NETCDF:"filename":subdataset         */
/* -------------------------------------------------------------------- */
    var=-1;
    nc_inq_varid( cdfid, osSubdatasetName, &var);
    nd = 0;
    nc_inq_varndims ( cdfid, var, &nd );

    paDimIds = (int *)CPLCalloc(nd, sizeof( int ) );
    panBandDimPos = ( int * ) CPLCalloc( nd, sizeof( int ) );

    nc_inq_vardimid( cdfid, var, paDimIds );
	
/* -------------------------------------------------------------------- */
/*      Check fi somebody tried to pass a variable with less than 2D    */
/* -------------------------------------------------------------------- */
    if ( nd < 2 ) {
        CPLFree( paDimIds );
        CPLFree( panBandDimPos );
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      CF-1 Convention                                                 */
/*      dimensions to appear in the relative order T, then Z, then Y,   */
/*      then X  to the file. All other dimensions should, whenever      */
/*      possible, be placed to the left of the spatiotemporal           */
/*      dimensions.                                                     */
/* -------------------------------------------------------------------- */

/* -------------------------------------------------------------------- */
/*      Get X dimensions information                                    */
/* -------------------------------------------------------------------- */
    poDS->nXDimID = paDimIds[nd-1];
    nc_inq_dimlen ( cdfid, poDS->nXDimID, &xdim );
    poDS->nRasterXSize = xdim;

/* -------------------------------------------------------------------- */
/*      Get Y dimension information                                     */
/* -------------------------------------------------------------------- */
    poDS->nYDimID = paDimIds[nd-2];
    nc_inq_dimlen ( cdfid, poDS->nYDimID, &ydim );
    poDS->nRasterYSize = ydim;


    for( j=0,k=0; j < nd; j++ ){
        if( paDimIds[j] == poDS->nXDimID ){ 
            panBandDimPos[0] = j;         // Save Position of XDim
            k++;
        }
        if( paDimIds[j] == poDS->nYDimID ){
            panBandDimPos[1] = j;         // Save Position of YDim
            k++;
        }
    }
/* -------------------------------------------------------------------- */
/*      X and Y Dimension Ids were not found!                           */
/* -------------------------------------------------------------------- */
    if( k != 2 ) {
        CPLFree( paDimIds );
        CPLFree( panBandDimPos );
        return NULL;
    }
	    
/* -------------------------------------------------------------------- */
/*      Read Metadata for this variable                                 */
/* -------------------------------------------------------------------- */
/* should disable as is also done at band level, except driver needs the 
   variables as metadata (e.g. projection) */
    poDS->ReadAttributes( cdfid, var );
	
/* -------------------------------------------------------------------- */
/*      Read Metadata for each dimension                                */
/* -------------------------------------------------------------------- */
    
    for( j=0; j < dim_count; j++ ){
        nc_inq_dimname( cdfid, j, szTemp );
        poDS->papszDimName.AddString( szTemp );
        status = nc_inq_varid( cdfid, poDS->papszDimName[j], &nDimID );
        if( status == NC_NOERR ) {
            poDS->ReadAttributes( cdfid, nDimID );
        }
    }

/* -------------------------------------------------------------------- */
/*      Set projection info                                             */
/* -------------------------------------------------------------------- */
    poDS->SetProjectionFromVar( var );

    /* override bottom-up with GDAL_NETCDF_BOTTOMUP config option */
    const char *pszValue = CPLGetConfigOption( "GDAL_NETCDF_BOTTOMUP", NULL );
    if ( pszValue ) {
        poDS->bBottomUp = CSLTestBoolean( pszValue ) != FALSE; 
        CPLDebug( "GDAL_netCDF", 
                  "set bBottomUp=%d because GDAL_NETCDF_BOTTOMUP=%s",
                  poDS->bBottomUp, pszValue );
    }

    poDS->SetMetadata( poDS->papszMetadata );

/* -------------------------------------------------------------------- */
/*      Create bands                                                    */
/* -------------------------------------------------------------------- */
    panBandZLev = (int *)CPLCalloc( nd-2, sizeof( int ) );
    
    nTotLevCount = 1;
    if ( dim_count > 2 ) {
        nDim=2;
        for( j=0; j < nd; j++ ){
            if( ( paDimIds[j] != poDS->nXDimID ) && 
                ( paDimIds[j] != poDS->nYDimID ) ){
                nc_inq_dimlen ( cdfid, paDimIds[j], &lev_count );
                nTotLevCount *= lev_count;
                panBandZLev[ nDim-2 ] = lev_count;
                panBandDimPos[ nDim++ ] = j;  //Save Position of ZDim
            }
        }
    }
    i=0;

    for ( unsigned int lev = 0; lev < nTotLevCount ; lev++ ) {
        char ** papszToken;
        papszToken=NULL;

        netCDFRasterBand *poBand =
            new netCDFRasterBand(poDS, var, nDim, lev,
                                 panBandZLev, panBandDimPos, 
                                 paDimIds, i+1 );

        poDS->SetBand( i+1, poBand );
        i++;
    } 

    CPLFree( paDimIds );
    CPLFree( panBandDimPos );
    CPLFree( panBandZLev );
    
    poDS->nBands = i;

    // Handle angular geographic coordinates here

/* -------------------------------------------------------------------- */
/*      Initialize any PAM information.                                 */
/* -------------------------------------------------------------------- */
    if( bTreatAsSubdataset )
    {
        poDS->SetPhysicalFilename( poDS->osFilename );
        poDS->SetSubdatasetName( osSubdatasetName );
    }
    
    poDS->TryLoadXML();

    if( bTreatAsSubdataset )
        poDS->oOvManager.Initialize( poDS, ":::VIRTUAL:::" );
    else
        poDS->oOvManager.Initialize( poDS, poDS->osFilename );

    return( poDS );
}


/************************************************************************/
/*                            CopyMetadata()                            */
/*                                                                      */
/*      Create a copy of metadata for NC_GLOBAL or a variable           */
/************************************************************************/

void CopyMetadata( void  *poDS, int fpImage, int CDFVarID ) {

    char       **papszMetadata;
    char       **papszFieldData;
    const char *pszField;
    char       szMetaName[ NCDF_MAX_STR_LEN ];
    char       szMetaValue[ NCDF_MAX_STR_LEN ];
    char       szTemp[ NCDF_MAX_STR_LEN ];
    int        nItems;
    int        bCopyItem;

    /* Remove the following band meta but set them later from band data */
    const char *papszIgnore[] = { CF_ADD_OFFSET, CF_SCALE_FACTOR, 
                                  "valid_range", "_Unsigned", 
                                  _FillValue, NULL };

    if( CDFVarID == NC_GLOBAL ) {
        papszMetadata = GDALGetMetadata( (GDALDataset *) poDS,"");
    } else {
        papszMetadata = GDALGetMetadata( (GDALRasterBandH) poDS, NULL );
    }

    nItems = CSLCount( papszMetadata );             
    
    for(int k=0; k < nItems; k++ ) {
        bCopyItem = TRUE;
        pszField = CSLGetField( papszMetadata, k );
        papszFieldData = CSLTokenizeString2 (pszField, "=", 
                                             CSLT_HONOURSTRINGS );
        if( papszFieldData[1] != NULL ) {
            strcpy( szMetaName,  papszFieldData[ 0 ] );
            strcpy( szMetaValue, papszFieldData[ 1 ] );

            /* Fix various issues with metadata translation */ 
            if( CDFVarID == NC_GLOBAL ) {
                /* Remove NC_GLOBAL prefix for netcdf global Metadata */ 
                if( strncmp( szMetaName, "NC_GLOBAL#", 10 ) == 0 ) {
                    strcpy( szTemp, szMetaName+10 );
                    strcpy( szMetaName, szTemp );
                } 
                /* GDAL Metadata renamed as GDAL-[meta] */
                else if ( strstr( szMetaName, "#" ) == NULL ) {
                    strcpy( szTemp, "GDAL_" );
                    strcat( szTemp, szMetaName );
                    strcpy( szMetaName, szTemp );
                }
                /* Keep time, lev and depth information for safe-keeping */
                /* Time and vertical coordinate handling need improvements */
                else if( strncmp( szMetaName, "time#", 5 ) == 0 ) {
                    szMetaName[4] = '-';
                }
                else if( strncmp( szMetaName, "lev#", 4 ) == 0 ) {
                    szMetaName[3] = '-';
                }
                else if( strncmp( szMetaName, "depth#", 6 ) == 0 ) {
                    szMetaName[5] = '-';
                }
                /* Only copy data without # (previously all data was copied)  */
                if ( strstr( szMetaName, "#" ) != NULL ) {   
                    bCopyItem = FALSE;
                }
                /* netCDF attributes do not like the '#' character. */
                // for( unsigned int h=0; h < strlen( szMetaName ) -1 ; h++ ) {
                //     if( szMetaName[h] == '#' ) szMetaName[h] = '-'; 
                // }
            }
            else {
                /* for variables, don't copy varname */
                if ( strncmp( szMetaName, "NETCDF_VARNAME", 14) == 0 ) 
                    bCopyItem = FALSE;
                /* Don't copy band statistics */
                else if ( strncmp( szMetaName, "STATISTICS_", 11) == 0 ) 
                    bCopyItem = FALSE;
                if ( CSLFindString( (char **)papszIgnore, szMetaName ) != -1 ) {
                    bCopyItem = FALSE;
                }
            }

            if ( bCopyItem ) {
                if ( NCDFPutAttr( fpImage, CDFVarID,szMetaName, 
                                  szMetaValue ) != CE_None )
                    CPLDebug( "GDAL_netCDF", "NCDFPutAttr(%d, %d, %s, %s) failed", 
                              fpImage, CDFVarID,szMetaName, szMetaValue );
            }

        }
        CSLDestroy( papszFieldData );
    }

    /* Set add_offset and scale_factor here if present */
    if( CDFVarID != NC_GLOBAL ) {

        int bGotAddOffset, bGotScale;
        GDALRasterBandH poRB = (GDALRasterBandH) poDS;
        double dfAddOffset = GDALGetRasterOffset( poRB , &bGotAddOffset );
        double dfScale = GDALGetRasterScale( poRB, &bGotScale );

        if ( bGotAddOffset && dfAddOffset != 0.0 && bGotScale && dfScale != 1.0 ) {
            GDALSetRasterOffset( poRB, dfAddOffset );
            GDALSetRasterScale( poRB, dfScale );
        }

    }

}

/*
Driver options:

FORMAT=NC/NC2/NC4/NC4C (COMPRESS=DEFLATE sets FORMAT=NC4C)
COMPRESS=NONE/DEFLATE (default: NONE)
ZLEVEL=[1-9] (default: 1)
WRITE_BOTTOMUP=YES/NO (default: YES)
WRITE_GDAL_TAGS=YES/NO (default: YES)
WRITE_LONLAT=YES/NO/IF_NEEDED (default: YES for geographic, NO for projected)
TYPE_LONLAT=float/double (default: double for geographic, float for projected)
PIXELTYPE=DEFAULT/SIGNEDBYTE (use SIGNEDBYTE to get a signed Byte Band)

Config Options:

GDAL_NETCDF_BOTTOMUP=YES/NO overrides bottom-up value on import

*/

/************************************************************************/
/*                            CreateLL()                                */
/*                                                                      */
/*      Shared functionality between netCDFDataset::Create() and        */
/*      netCDF::CreateCopy() for creating netcdf file based on a set of */
/*      options and a configuration.                                    */
/************************************************************************/

netCDFDataset *
netCDFDataset::CreateLL( const char * pszFilename,
                         int nXSize, int nYSize, int nBands,
                         char ** papszOptions )
{
    int status = NC_NOERR;
    netCDFDataset *poDS;
    
    poDS = new netCDFDataset();
    poDS->nRasterXSize = nXSize;
    poDS->nRasterYSize = nYSize;
    poDS->eAccess = GA_Update;
    poDS->osFilename = pszFilename;

    /* from gtiff driver, is this ok? */
    /*
    poDS->nBlockXSize = nXSize;
    poDS->nBlockYSize = 1;
    poDS->nBlocksPerBand =
        ((nYSize + poDS->nBlockYSize - 1) / poDS->nBlockYSize)
        * ((nXSize + poDS->nBlockXSize - 1) / poDS->nBlockXSize);
        */

    /* process options */
    poDS->papszCreationOptions = CSLDuplicate( papszOptions );
    poDS->ProcessCreationOptions( );

/* -------------------------------------------------------------------- */
/*      Create the dataset.                                             */
/* -------------------------------------------------------------------- */
    status = nc_create( pszFilename, poDS->nCreateMode,  &(poDS->cdfid) );

    /* put into define mode */
    poDS->SetDefineMode(TRUE);

    if( status != NC_NOERR )
    {
        CPLError( CE_Failure, CPLE_OpenFailed, 
                  "Unable to create netCDF file %s (Error code %d): %s .\n", 
                  pszFilename, status, nc_strerror(status) );
        delete poDS;
        return NULL;
    }

/* -------------------------------------------------------------------- */
/*      Define dimensions                                               */
/* -------------------------------------------------------------------- */
    poDS->papszDimName.AddString( NCDF_DIMNAME_Y );
    poDS->papszDimName.AddString( NCDF_DIMNAME_X );

    status = nc_def_dim( poDS->cdfid, NCDF_DIMNAME_X, nXSize, 
                         &(poDS->nXDimID) );
    NCDF_ERR(status);
    CPLDebug( "GDAL_netCDF", "status nc_def_dim %s = %d, nXDimID = %d", 
              NCDF_DIMNAME_X, status, poDS->nXDimID );   

    status = nc_def_dim( poDS->cdfid, NCDF_DIMNAME_Y, nYSize, 
                         &(poDS->nYDimID) );
    NCDF_ERR(status);
    CPLDebug( "GDAL_netCDF", "status nc_def_dim %s = %d, nYDimID = %d", 
              NCDF_DIMNAME_Y, status, poDS->nYDimID );   

    return poDS;

}

/************************************************************************/
/*                            Create()                                  */
/************************************************************************/

GDALDataset *
netCDFDataset::Create( const char * pszFilename,
                       int nXSize, int nYSize, int nBands,
                       GDALDataType eType,
                       char ** papszOptions )
{
    netCDFDataset *poDS;

    CPLDebug( "GDAL_netCDF", 
              "\n=====\nnetCDFDataset::Create( %s, ... )\n", 
              pszFilename );
    
    poDS =  netCDFDataset::CreateLL( pszFilename,
                                     nXSize, nYSize, nBands,
                                     papszOptions );

    if ( ! poDS ) 
        return NULL;

    /* should we write signed or unsigned byte? */
    /* TODO should this only be done in Create() */
    poDS->bSignedData = TRUE;
    const char *pszValue  =
        CSLFetchNameValue( papszOptions, "PIXELTYPE" );
    if( pszValue == NULL )
        pszValue = "";
    if( eType == GDT_Byte && ( ! EQUAL(pszValue,"SIGNEDBYTE") ) )
        poDS->bSignedData = FALSE;

/* -------------------------------------------------------------------- */
/*      Add Conventions, GDAL info and history                          */
/* -------------------------------------------------------------------- */
    NCDFAddGDALHistory( poDS->cdfid, pszFilename, "", "Create" );

/* -------------------------------------------------------------------- */
/*      Define bands                                                    */
/* -------------------------------------------------------------------- */
    for( int iBand = 1; iBand <= nBands; iBand++ )
    {
        poDS->SetBand( iBand, new netCDFRasterBand( poDS, eType, iBand,
                                                    poDS->bSignedData ) );
    }

    CPLDebug( "GDAL_netCDF", 
              "netCDFDataset::Create( %s, ... ) done", 
              pszFilename );
/* -------------------------------------------------------------------- */
/*      Return same dataset                                             */
/* -------------------------------------------------------------------- */
     return( poDS );

}


template <class T>
CPLErr  NCDFCopyBand( GDALRasterBand *poSrcBand, GDALRasterBand *poBand,
                      int nXSize, int nYSize,
                      GDALProgressFunc pfnProgress, void * pProgressData )
{
    GDALDataType eDT = poSrcBand->GetRasterDataType();
    CPLErr eErr = CE_None;
    T *patScanline = (T *) CPLMalloc( nXSize * sizeof(T) );
        
    for( int iLine = 0; iLine < nYSize && eErr == CE_None; iLine++ )  {                
        eErr = poSrcBand->RasterIO( GF_Read, 0, iLine, nXSize, 1, 
                                    patScanline, nXSize, 1, eDT,
                                    0,0);
        if ( eErr == CE_None )
            eErr = poBand->RasterIO( GF_Write, 0, iLine, nXSize, 1, 
                                     patScanline, nXSize, 1, eDT,
                                     0,0);

        if ( ( nYSize>10 ) && ( iLine % (nYSize/10) == 1 ) ) {
            pfnProgress( 1.0*iLine/nYSize , NULL, pProgressData );
        }
    }
           
    CPLFree( patScanline );

    pfnProgress( 1.0, NULL, pProgressData );

    return eErr;
}


/************************************************************************/
/*                            CreateCopy()                              */
/************************************************************************/

GDALDataset*
netCDFDataset::CreateCopy( const char * pszFilename, GDALDataset *poSrcDS, 
                           int bStrict, char ** papszOptions, 
                           GDALProgressFunc pfnProgress, void * pProgressData )
{
    netCDFDataset *poDS;
    void *pScaledProgress;
    GDALDataType eDT;
    CPLErr eErr = CE_None;
    int nBands, nXSize, nYSize;
    double adfGeoTransform[6];
    const char *pszWKT;
    int iBand;
    
    CPLDebug( "GDAL_netCDF", 
              "\n=====\nnetCDFDataset::CreateCopy( %s, ... )\n", 
              pszFilename );

    nBands = poSrcDS->GetRasterCount();
    nXSize = poSrcDS->GetRasterXSize();
    nYSize = poSrcDS->GetRasterYSize();
    pszWKT = poSrcDS->GetProjectionRef();
  
/* -------------------------------------------------------------------- */
/*      Check input bands for errors                                    */
/* -------------------------------------------------------------------- */

    if (nBands == 0)
    {
        CPLError( CE_Failure, CPLE_NotSupported, 
                  "NetCDF driver does not support source dataset with zero band.\n");
        return NULL;
    }

    for( iBand=1; iBand <= nBands; iBand++ )
    {
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand );
        eDT = poSrcBand->GetRasterDataType();
        if (eDT == GDT_Unknown || GDALDataTypeIsComplex(eDT))
        {
            CPLError( CE_Failure, CPLE_NotSupported, 
                      "NetCDF driver does not support source dataset with band of complex type.");
            return NULL;
        }
    }

    if( !pfnProgress( 0.0, NULL, pProgressData ) )
        return NULL;

    /* same as in Create() */
    poDS = netCDFDataset::CreateLL( pszFilename,
                                    nXSize, nYSize, nBands,
                                    papszOptions );
    if ( ! poDS ) 
        return NULL;

/* -------------------------------------------------------------------- */
/*      Copy global metadata                                            */
/*      Add Conventions, GDAL info and history                          */
/* -------------------------------------------------------------------- */
    CopyMetadata((void *) poSrcDS, poDS->cdfid, NC_GLOBAL );
    NCDFAddGDALHistory( poDS->cdfid, pszFilename,
                        poSrcDS->GetMetadataItem("NC_GLOBAL#history",""),
                        "CreateCopy" );

    pfnProgress( 0.1, NULL, pProgressData );

/* -------------------------------------------------------------------- */
/*      Define Bands                                                    */
/* -------------------------------------------------------------------- */
    for( iBand=1; iBand <= nBands; iBand++ ) {
        CPLDebug( "GDAL_netCDF", "creating band # %d/%d ",
                  iBand,nBands );

        char szBandName[ NC_MAX_NAME ];
        char szLongName[ NC_MAX_NAME ];
        const char *tmpMetadata;
        netCDFRasterBand *poBand;
        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand );
        eDT = poSrcBand->GetRasterDataType();

        /* Get var name from NETCDF_VARNAME */
        tmpMetadata = poSrcBand->GetMetadataItem("NETCDF_VARNAME");
       	if( tmpMetadata != NULL) {
            if( nBands > 1 ) sprintf(szBandName,"%s%d",tmpMetadata,iBand);
            else strcpy( szBandName, tmpMetadata );
            // poSrcBand->SetMetadataItem("NETCDF_VARNAME","");
        }
        else 
            szBandName[0]='\0';
        
        /* Get long_name from <var>#long_name */
        sprintf(szLongName,"%s#%s",
                poSrcBand->GetMetadataItem("NETCDF_VARNAME"),
                CF_LNG_NAME);
        tmpMetadata = poSrcDS->GetMetadataItem(szLongName);
        if( tmpMetadata != NULL) 
            strcpy( szLongName, tmpMetadata);
        else 
            szLongName[0]='\0';

        int bSignedData = TRUE;
        if ( eDT == GDT_Byte ) {
            /* GDAL defaults to unsigned bytes, but check if metadata says its
               signed, as NetCDF can support this for certain formats. */
            bSignedData = FALSE;
            tmpMetadata = poSrcBand->GetMetadataItem("PIXELTYPE",
                                                     "IMAGE_STRUCTURE");
            if ( tmpMetadata && EQUAL(tmpMetadata,"SIGNEDBYTE") )
                bSignedData = TRUE;
        }

        poBand = new netCDFRasterBand( poDS, eDT, iBand,
                                       bSignedData,
                                       szBandName, szLongName );
        poDS->SetBand( iBand, poBand );
        
        /* Copy Metadata for band */
        poBand->SetNoDataValue( poSrcBand->GetNoDataValue(0) );
        CopyMetadata( (void *) GDALGetRasterBand( poSrcDS, iBand ), 
                      poDS->cdfid, poBand->nZId );
        
    }

    pfnProgress( 0.2, NULL, pProgressData );

/* -------------------------------------------------------------------- */
/*      Copy GeoTransform and Projection                                */
/* -------------------------------------------------------------------- */
    int bGotGeoTransform = FALSE;
    eErr = poSrcDS->GetGeoTransform( adfGeoTransform );
    if ( eErr == CE_None ) {
        poDS->SetGeoTransform( adfGeoTransform );
        /* disable AddProjectionVars() from being called */
        bGotGeoTransform = TRUE;
        poDS->bSetGeoTransform = FALSE;
    }

    if ( pszWKT ) {
        poDS->SetProjection( pszWKT );
        /* now we can call AddProjectionVars() directly */
        poDS->bSetGeoTransform = bGotGeoTransform;
        pScaledProgress = GDALCreateScaledProgress( 0.20, 0.50, pfnProgress, 
                                                    pProgressData );
        poDS->AddProjectionVars( GDALScaledProgress, pScaledProgress );
        GDALDestroyScaledProgress( pScaledProgress );
    }

    pfnProgress( 0.5, NULL, pProgressData );
    
/* -------------------------------------------------------------------- */
/*      Write Bands                                                     */
/* -------------------------------------------------------------------- */
    /* make sure we are in data mode */
    poDS->SetDefineMode( FALSE );

    double dfTemp,dfTemp2;
    dfTemp = dfTemp2 = 0.5;

    for( iBand=1; iBand <= nBands; iBand++ ) {
        
        dfTemp2 = dfTemp + 0.4/nBands; 
        pScaledProgress = 
            GDALCreateScaledProgress( dfTemp, dfTemp2,
                                      pfnProgress, pProgressData );
        dfTemp = dfTemp2;

        GDALRasterBand *poSrcBand = poSrcDS->GetRasterBand( iBand );

        /* new */
        GDALDataType eDT;
        CPLErr eErr = CE_None;

        CPLDebug( "GDAL_netCDF", "copying band data # %d/%d ",
                  iBand,nBands );

        // hBand = GDALGetRasterBand( poSrcDS, i );
        GDALRasterBand *poBand = poDS->GetRasterBand( iBand );
        eDT = poSrcBand->GetRasterDataType();
        eErr = CE_None;

/* -------------------------------------------------------------------- */
/*      Copy Band data                                                  */
/* -------------------------------------------------------------------- */
        if( eDT == GDT_Byte ) {
            CPLDebug( "GDAL_netCDF", "GByte Band#%d", iBand );
            NCDFCopyBand<GByte>( poSrcBand, poBand, nXSize, nYSize,
                                 GDALScaledProgress, pScaledProgress );
        } 
        else if( ( eDT == GDT_UInt16 ) || ( eDT == GDT_Int16 ) ) {
            CPLDebug( "GDAL_netCDF", "GInt16 Band#%d", iBand );
            NCDFCopyBand<GInt16>( poSrcBand, poBand, nXSize, nYSize,
                                 GDALScaledProgress, pScaledProgress );
        } 
        else if( (eDT == GDT_UInt32) || (eDT == GDT_Int32) ) {
            CPLDebug( "GDAL_netCDF", "GInt16 Band#%d", iBand );
            NCDFCopyBand<GInt32>( poSrcBand, poBand, nXSize, nYSize,
                                 GDALScaledProgress, pScaledProgress );
        }
        else if( (eDT == GDT_Float32) ) {
            CPLDebug( "GDAL_netCDF", "float Band#%d", iBand);
            NCDFCopyBand<float>( poSrcBand, poBand, nXSize, nYSize,
                                 GDALScaledProgress, pScaledProgress );
        }
        else if( (eDT == GDT_Float64) ) {
            CPLDebug( "GDAL_netCDF", "double Band#%d", iBand);
            NCDFCopyBand<double>( poSrcBand, poBand, nXSize, nYSize,
                                 GDALScaledProgress, pScaledProgress );
        }
        else {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "The NetCDF driver does not support GDAL data type %d",
                      eDT );
        }
        
        GDALDestroyScaledProgress( pScaledProgress );

    }

/* -------------------------------------------------------------------- */
/*      Cleanup and close.                                              */
/* -------------------------------------------------------------------- */
    delete( poDS );
// CPLFree(pszProj4Defn );
 
    pfnProgress( 0.95, NULL, pProgressData );

/* -------------------------------------------------------------------- */
/*      Re-open dataset, and copy any auxilary pam information.         */
/*      Disable PAM, at least temporarily. See bug #4244                */
/* -------------------------------------------------------------------- */
    poDS = (netCDFDataset *) GDALOpen( pszFilename, GA_ReadOnly );

    // if( poDS )
    //     poDS->CloneInfo( poSrcDS, GCIF_PAM_DEFAULT );
    
    pfnProgress( 1.0, NULL, pProgressData );

    return poDS;
}


/* note: some logic depends on bIsProjected and bIsGeoGraphic */
/* which may not be known when Create() is called, see AddProjectionVars() */
void
netCDFDataset::ProcessCreationOptions( )
{ 
    const char *pszValue;

    /* File format */
    nFormat = NCDF_FORMAT_NC;
    pszValue = CSLFetchNameValue( papszCreationOptions, "FORMAT" );
    if ( pszValue != NULL ) {
        if ( EQUAL( pszValue, "NC" ) ) {
            nFormat = NCDF_FORMAT_NC;
        }
#ifdef NETCDF_HAS_NC2
        else if ( EQUAL( pszValue, "NC2" ) ) {
            nFormat = NCDF_FORMAT_NC2;
        }
#endif
#ifdef NETCDF_HAS_NC4
        else if ( EQUAL( pszValue, "NC4" ) ) {
            nFormat = NCDF_FORMAT_NC4;
        }    
        else if ( EQUAL( pszValue, "NC4C" ) ) {
            nFormat = NCDF_FORMAT_NC4C;
        }    
#endif
        else {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "FORMAT=%s in not supported, using the default NC format.", pszValue );        
        }
    }

    /* compression only available for NC4 */
#ifdef NETCDF_HAS_NC4

    /* COMPRESS option */
    pszValue = CSLFetchNameValue( papszCreationOptions, "COMPRESS" );
    if ( pszValue != NULL ) {
        if ( EQUAL( pszValue, "NONE" ) ) {
            nCompress = NCDF_COMPRESS_NONE;
        }       
        else if ( EQUAL( pszValue, "DEFLATE" ) ) {
            nCompress = NCDF_COMPRESS_DEFLATE;
            if ( !((nFormat == NCDF_FORMAT_NC4) || (nFormat == NCDF_FORMAT_NC4C)) ) {
                CPLError( CE_Warning, CPLE_IllegalArg,
                          "NOTICE: Format set to NC4C because compression is set to DEFLATE." );
                nFormat = NCDF_FORMAT_NC4C;
            }
        }
        else {
            CPLError( CE_Failure, CPLE_NotSupported,
                      "COMPRESS=%s is not supported.", pszValue );
        }
    }

    /* ZLEVEL option */
    pszValue = CSLFetchNameValue( papszCreationOptions, "ZLEVEL" );
    if( pszValue != NULL )
    {
        nZLevel =  atoi( pszValue );
        if (!(nZLevel >= 1 && nZLevel <= 9))
        {
            CPLError( CE_Warning, CPLE_IllegalArg, 
                    "ZLEVEL=%s value not recognised, ignoring.",
                    pszValue );
            nZLevel = NCDF_DEFLATE_LEVEL;
        }
    }

#endif

    /* set nCreateMode based on nFormat */
    switch ( nFormat ) {        
#ifdef NETCDF_HAS_NC2
        case NCDF_FORMAT_NC2:
            nCreateMode = NC_CLOBBER|NC_64BIT_OFFSET;
            break;
#endif
#ifdef NETCDF_HAS_NC4
        case NCDF_FORMAT_NC4:
            nCreateMode = NC_CLOBBER|NC_NETCDF4;
            break;
        case NCDF_FORMAT_NC4C:
            nCreateMode = NC_CLOBBER|NC_NETCDF4|NC_CLASSIC_MODEL;
            break;
#endif
        case NCDF_FORMAT_NC:
        default:
            nCreateMode = NC_CLOBBER;
            break;
    }

    CPLDebug( "GDAL_netCDF", 
              "file options: format=%d compress=%d zlevel=%d",
              nFormat, nCompress, nZLevel );

}

int netCDFDataset::DefVarDeflate( int nVarId, int bChunking )
{
#ifdef NETCDF_HAS_NC4
    if ( nCompress == NCDF_COMPRESS_DEFLATE ) {                         
        // must set chunk size to avoid huge performace hit (set bChunking=TRUE)             
        // perhaps another solution it to change the chunk cache?
        // http://www.unidata.ucar.edu/software/netcdf/docs/netcdf.html#Chunk-Cache   
        // TODO make sure this is ok
        CPLDebug( "GDAL_netCDF", 
                  "DefVarDeflate( %d, %d ) nZlevel=%d",
                  nVarId, bChunking, nZLevel );
        status = nc_def_var_deflate(cdfid,nVarId,1,1,nZLevel);
        NCDF_ERR(status);
        if ( (status == NC_NOERR) && bChunking ) {
            size_t chunksize[] = { 1, nRasterXSize };                   
            CPLDebug( "GDAL_netCDF", 
                      "DefVarDeflate() chunksize={%ld, %ld}",
                      chunksize[0], chunksize[1] );
            status = nc_def_var_chunking( cdfid, nVarId,          
                                          NC_CHUNKED, chunksize );
            NCDF_ERR(status);
        }
        return status;
    } 
#endif
    return NC_NOERR;
}


/************************************************************************/
/*                          GDALRegister_netCDF()                       */
/************************************************************************/

void GDALRegister_netCDF()

{
    if (! GDAL_CHECK_VERSION("netCDF driver"))
        return;

    if( GDALGetDriverByName( "netCDF" ) == NULL )
    {
        GDALDriver	*poDriver;
        char szCreateOptions[3072];

        poDriver = new GDALDriver( );

/* -------------------------------------------------------------------- */
/*      Build full creation option list.                                */
/* -------------------------------------------------------------------- */
    sprintf( szCreateOptions, "%s", 
"<CreationOptionList>"
"   <Option name='FORMAT' type='string-select' default='NC'>"
"     <Value>NC</Value>"
#ifdef NETCDF_HAS_NC2
"     <Value>NC2</Value>"
#endif
#ifdef NETCDF_HAS_NC4
"     <Value>NC4</Value>"
"     <Value>NC4C</Value>"
#endif
"   </Option>"
#ifdef NETCDF_HAS_NC4
"   <Option name='COMPRESS' type='string-select' default='NONE'>"
"     <Value>NONE</Value>"
"     <Value>DEFLATE</Value>"
"   </Option>"
"   <Option name='ZLEVEL' type='int' description='DEFLATE compression level 1-9' default='1'/>"
#endif
"   <Option name='WRITE_BOTTOMUP' type='boolean' default='YES'>"
"   </Option>"
"   <Option name='WRITE_GDAL_TAGS' type='boolean' default='YES'>"
"   </Option>"
"   <Option name='WRITE_LONLAT' type='string-select'>"
"     <Value>YES</Value>"
"     <Value>NO</Value>"
"     <Value>IF_NEEDED</Value>"
"   </Option>"
"   <Option name='TYPE_LONLAT' type='string-select'>"
"     <Value>float</Value>"
"     <Value>double</Value>"
"   </Option>"
"   <Option name='PIXELTYPE' type='string-select' description='only used in Create()'>"
"       <Value>DEFAULT</Value>"
"       <Value>SIGNEDBYTE</Value>"
"   </Option>"
"</CreationOptionList>" );

        
/* -------------------------------------------------------------------- */
/*      Set the driver details.                                         */
/* -------------------------------------------------------------------- */
        poDriver->SetDescription( "netCDF" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, 
                                   "Network Common Data Format" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, 
                                   "frmt_netcdf.html" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "nc" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST, 
                                   szCreateOptions );

        /* make driver config and capabilities available */
        poDriver->SetMetadataItem( "NETCDF_VERSION", nc_inq_libvers() );
        poDriver->SetMetadataItem( "NETCDF_CONVENTIONS", NCDF_CONVENTIONS_CF );
#ifdef NETCDF_HAS_NC2
        poDriver->SetMetadataItem( "NETCDF_HAS_NC2", "YES" );
#endif
#ifdef NETCDF_HAS_NC4
        poDriver->SetMetadataItem( "NETCDF_HAS_NC4", "YES" );
#endif
#ifdef NETCDF_HAS_HDF4
        poDriver->SetMetadataItem( "NETCDF_HAS_HDF4", "YES" );
#endif
#ifdef HAVE_HDF4
        poDriver->SetMetadataItem( "GDAL_HAS_HDF4", "YES" );
#endif
#ifdef HAVE_HDF5
        poDriver->SetMetadataItem( "GDAL_HAS_HDF5", "YES" );
#endif
 
        /* set pfns and register driver */
        poDriver->pfnOpen = netCDFDataset::Open;
        poDriver->pfnCreateCopy = netCDFDataset::CreateCopy;
        poDriver->pfnCreate = netCDFDataset::Create;
        poDriver->pfnIdentify = netCDFDataset::Identify;

        GetGDALDriverManager( )->RegisterDriver( poDriver );
    }
}

/************************************************************************/
/*                          New functions                               */
/************************************************************************/

/* Test for GDAL version string >= target */
int NCDFIsGDALVersionGTE(const char* pszVersion, int nTarget)
{
    int nVersion = 0;
    int nVersions [] = {0,0,0,0};
    char **papszTokens;

    /* Valid strings are "GDAL 1.9dev, released 2011/01/18" and "GDAL 1.8.1 " */
    if ( pszVersion == NULL || EQUAL( pszVersion, "" ) )
        return FALSE;
    else if ( ! EQUALN("GDAL ", pszVersion, 5) )
        return FALSE;
    else if ( EQUALN("GDAL 1.9dev", pszVersion,11 ) )
        return nTarget <= 1900;
    else if ( EQUALN("GDAL 1.8dev", pszVersion,11 ) )
        return nTarget <= 1800;

    papszTokens = CSLTokenizeString2( pszVersion+5, ".", 0 );

    for ( int iToken = 0; papszTokens && papszTokens[iToken]; iToken++ )  {
        nVersions[iToken] = atoi( papszTokens[iToken] );
    }
    /* (GDAL_VERSION_MAJOR*1000+GDAL_VERSION_MINOR*100+GDAL_VERSION_REV*10+GDAL_VERSION_BUILD) */
    nVersion = nVersions[0]*1000 + nVersions[1]*100 + 
        nVersions[2]*10 + nVersions[3]; 
    
    CSLDestroy( papszTokens );
    return nTarget <= nVersion;
}

/* Add Conventions, GDAL version and history  */ 
void NCDFAddGDALHistory( int fpImage, 
                         const char * pszFilename, const char *pszOldHist,
                         const char * pszFunctionName)
{
    char     szTemp[NC_MAX_NAME];

    nc_put_att_text( fpImage, NC_GLOBAL, "Conventions", 
                     strlen(NCDF_CONVENTIONS_CF),
                     NCDF_CONVENTIONS_CF ); 

    const char* pszNCDF_GDAL = GDALVersionInfo("--version");
    nc_put_att_text( fpImage, NC_GLOBAL, "GDAL", 
                     strlen(pszNCDF_GDAL), pszNCDF_GDAL );

    /* Add history */
#ifdef GDAL_SET_CMD_LINE_DEFINED_TMP
    if ( ! EQUAL(GDALGetCmdLine(), "" ) )
        strcpy( szTemp, GDALGetCmdLine() );
    else
        sprintf( szTemp, "GDAL %s( %s, ... )",pszFunctionName,pszFilename );
#else
    sprintf( szTemp, "GDAL %s( %s, ... )",pszFunctionName,pszFilename );
#endif
    
    NCDFAddHistory( fpImage, szTemp, pszOldHist );

}

/* code taken from cdo and libcdi, used for writing the history attribute */
//void cdoDefHistory(int fileID, char *histstring)
void NCDFAddHistory(int fpImage, const char *pszAddHist, const char *pszOldHist)
{
    char strtime[32];
    time_t tp;
    struct tm *ltime;

    char *pszNewHist = NULL;
    size_t nNewHistSize = 0;
    int disableHistory = FALSE;
    int status;

    /* Check pszOldHist - as if there was no previous history, it will be
       a null pointer - if so set as empty. */
    if (NULL == pszOldHist) {
        pszOldHist = "";
    }

    tp = time(NULL);
    if ( tp != -1 )
    {
        ltime = localtime(&tp);
        (void) strftime(strtime, sizeof(strtime), "%a %b %d %H:%M:%S %Y: ", ltime);
    }

    // status = nc_get_att_text( fpImage, NC_GLOBAL, 
    //                           "history", pszOldHist );
    // printf("status: %d pszOldHist: [%s]\n",status,pszOldHist);
    
    nNewHistSize = strlen(pszOldHist)+strlen(strtime)+strlen(pszAddHist)+1+1;
    pszNewHist = (char *) CPLMalloc(nNewHistSize * sizeof(char));
    
    strcpy(pszNewHist, strtime);
    strcat(pszNewHist, pszAddHist);

    if ( disableHistory == FALSE && pszNewHist )
    {
        if ( ! EQUAL(pszOldHist,"") )
            strcat(pszNewHist, "\n");
        strcat(pszNewHist, pszOldHist);
    }

    status = nc_put_att_text( fpImage, NC_GLOBAL, 
                              "history", strlen(pszNewHist),
                              pszNewHist ); 
    NCDF_ERR(status);

    CPLFree(pszNewHist);
}


int NCDFIsCfProjection( const char* pszProjection ) 
{
    /* Find the appropriate mapping */
    for (int iMap = 0; poNetcdfSRS_PT[iMap].WKT_SRS != NULL; iMap++ ) {
        // printf("now at %d, proj=%s\n",i, poNetcdfSRS_PT[i].GDAL_SRS);
        if ( EQUAL( pszProjection, poNetcdfSRS_PT[iMap].WKT_SRS ) )  {
            if ( poNetcdfSRS_PT[iMap].mappings != NULL )
                return TRUE;            
            else 
                return FALSE;
        }
    }
    return FALSE;
}


/* Write any needed projection attributes *
 * poPROJCS: ptr to proj crd system
 * pszProjection: name of projection system in GDAL WKT
 * fpImage: open NetCDF file in writing mode
 * NCDFVarID: NetCDF Var Id of proj system we're writing in to
 *
 * The function first looks for the oNetcdfSRS_PP mapping object
 * that corresponds to the input projection name. If none is found
 * the generic mapping is used.  In the case of specific mappings,
 * the driver looks for each attribute listed in the mapping object
 * and then looks up the value within the OGR_SRSNode. In the case
 * of the generic mapping, the lookup is reversed (projection params, 
 * then mapping).  For more generic code, GDAL->NETCDF 
 * mappings and the associated value are saved in std::map objects.
 */

/* NOTE modifications by ET to combine the specific and generic mappings */

void NCDFWriteProjAttribs( const OGR_SRSNode *poPROJCS,
                           const char* pszProjection,
                           const int fpImage, const int NCDFVarID ) 
{                            
    double dfStdP[2];
    int bFoundStdP1=FALSE,bFoundStdP2=FALSE;
    double dfValue=0.0;
    const char *pszParamStr, *pszParamVal;
    const std::string *pszNCDFAtt, *pszGDALAtt;
    static const oNetcdfSRS_PP *poMap = NULL;
    int nMapIndex = -1;
    int bWriteVal = FALSE;

    //Attribute <GDAL,NCDF> and Value <NCDF,value> mappings
    std::map< std::string, std::string > oAttMap;
    std::map< std::string, std::string >::iterator oAttIter;
    std::map< std::string, double > oValMap;
    std::map< std::string, double >::iterator oValIter, oValIter2;
    //results to write
    std::vector< std::pair<std::string,double> > oOutList;
 
    /* Find the appropriate mapping */
    for (int iMap = 0; poNetcdfSRS_PT[iMap].WKT_SRS != NULL; iMap++ ) {
        if ( EQUAL( pszProjection, poNetcdfSRS_PT[iMap].WKT_SRS ) ) {
            nMapIndex = iMap;
            poMap = poNetcdfSRS_PT[iMap].mappings;
            break;
        }
    }

    //ET TODO if projection name is not found, should we do something special?
    if ( nMapIndex == -1 ) {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "projection name %s not found in the lookup tables!!!",
                  pszProjection);
    }
    /* if no mapping was found or assigned, set the generic one */
    if ( !poMap ) {
        CPLError( CE_Warning, CPLE_AppDefined, 
                  "projection name %s in not part of the CF standard, will not be supported by CF!",
                  pszProjection);
        poMap = poGenericMappings;
    }

    /* initialize local map objects */
    for ( int iMap = 0; poMap[iMap].WKT_ATT != NULL; iMap++ ) {
        oAttMap[poMap[iMap].WKT_ATT] = poMap[iMap].CF_ATT;
    }

    for( int iChild = 0; iChild < poPROJCS->GetChildCount(); iChild++ ) {

        const OGR_SRSNode *poNode;

        poNode = poPROJCS->GetChild( iChild );
        if( !EQUAL(poNode->GetValue(),"PARAMETER") 
            || poNode->GetChildCount() != 2 )
            continue;
        pszParamStr = poNode->GetChild(0)->GetValue();
        pszParamVal = poNode->GetChild(1)->GetValue();

        oValMap[pszParamStr] = atof(pszParamVal);
    }

    /* Lookup mappings and fill output vector */
    if ( poMap != poGenericMappings ) { /* specific mapping, loop over mapping values */

        for ( oAttIter = oAttMap.begin(); oAttIter != oAttMap.end(); oAttIter++ ) {

            pszGDALAtt = &(oAttIter->first);
            pszNCDFAtt = &(oAttIter->second);
            oValIter = oValMap.find( *pszGDALAtt );

            if ( oValIter != oValMap.end() ) {

                dfValue = oValIter->second;
                bWriteVal = TRUE;

                /* special case for PS (Polar Stereographic) grid
                   See comments in netcdfdataset.h for this projection. */
                if ( EQUAL( SRS_PP_LATITUDE_OF_ORIGIN, pszGDALAtt->c_str() ) &&
                     EQUAL(pszProjection, SRS_PT_POLAR_STEREOGRAPHIC) ) {
                    double dfLatPole = 0.0;
                    if ( dfValue > 0.0) dfLatPole = 90.0;
                    else dfLatPole = -90.0;
                        oOutList.push_back( std::make_pair( CF_PP_LAT_PROJ_ORIGIN, 
                                                            dfLatPole ) );
                }              

                /* special case for LCC-1SP
                   See comments in netcdfdataset.h for this projection. */
                else if ( EQUAL( SRS_PP_SCALE_FACTOR, pszGDALAtt->c_str() ) &&
                          EQUAL(pszProjection, SRS_PT_LAMBERT_CONFORMAL_CONIC_1SP) ) {
                    /* default is to not write as it is not CF-1 */
                    bWriteVal = FALSE;
                    /* test if there is no standard_parallel1 */
                    if ( oValMap.find( std::string(CF_PP_STD_PARALLEL_1) ) == oValMap.end() ) {
                        /* if scale factor != 1.0  write value for GDAL, but this is not supported by CF-1 */
                        if ( !CPLIsEqual(dfValue,1.0) ) {
                            CPLError( CE_Failure, CPLE_NotSupported, 
                                      "NetCDF driver export of LCC-1SP with scale factor != 1.0 "
                                      "and no standard_parallel1 is not CF-1 (bug #3324).\n" 
                                      "Use the 2SP variant which is supported by CF." );   
                            bWriteVal = TRUE;
                        }
                        /* else copy standard_parallel1 from latitude_of_origin, because scale_factor=1.0 */
                        else {                      
                            oValIter2 = oValMap.find( std::string(SRS_PP_LATITUDE_OF_ORIGIN) );
                            if (oValIter2 != oValMap.end() ) {
                                oOutList.push_back( std::make_pair( CF_PP_STD_PARALLEL_1, 
                                                                    oValIter2->second) );
                            }
                            else {
                                CPLError( CE_Failure, CPLE_NotSupported, 
                                          "NetCDF driver export of LCC-1SP with no standard_parallel1 "
                                          "and no latitude_of_origin is not suported (bug #3324).");
                            }
                        }                      
                    }
                }
                if ( bWriteVal )
                    oOutList.push_back( std::make_pair( *pszNCDFAtt, dfValue ) );

            }
            // else printf("NOT FOUND!!!\n");
        }
    
    }
    else { /* generic mapping, loop over projected values */

        for ( oValIter = oValMap.begin(); oValIter != oValMap.end(); oValIter++ ) {

            pszGDALAtt = &(oValIter->first);
            dfValue = oValIter->second;

            oAttIter = oAttMap.find( *pszGDALAtt );

            if ( oAttIter != oAttMap.end() ) {
                oOutList.push_back( std::make_pair( oAttIter->second, dfValue ) );
            }
            /* for SRS_PP_SCALE_FACTOR write 2 mappings */
            else if (  EQUAL(pszGDALAtt->c_str(), SRS_PP_SCALE_FACTOR) ) {
                oOutList.push_back( std::make_pair( CF_PP_SCALE_FACTOR_MERIDIAN,
                                                    dfValue ) );
                oOutList.push_back( std::make_pair( CF_PP_SCALE_FACTOR_ORIGIN,
                                                    dfValue ) );
            }
            /* if not found insert the GDAL name */
            else {
                oOutList.push_back( std::make_pair( *pszGDALAtt, dfValue ) );
            }
        }
    }

    /* Write all the values that were found */
    // std::vector< std::pair<std::string,double> >::reverse_iterator it;
    // for (it = oOutList.rbegin();  it != oOutList.rend(); it++ ) {
    std::vector< std::pair<std::string,double> >::iterator it;
    for (it = oOutList.begin();  it != oOutList.end(); it++ ) {
        pszParamVal = (it->first).c_str();
        dfValue = it->second;
        /* Handle the STD_PARALLEL attrib */
        if( EQUAL( pszParamVal, CF_PP_STD_PARALLEL_1 ) ) {
            bFoundStdP1 = TRUE;
            dfStdP[0] = dfValue;
        }
        else if( EQUAL( pszParamVal, CF_PP_STD_PARALLEL_2 ) ) {
            bFoundStdP2 = TRUE;
            dfStdP[1] = dfValue;
        } 
        else {
            nc_put_att_double( fpImage, NCDFVarID, pszParamVal,
                               NC_DOUBLE, 1,&dfValue );
        }
    }
    /* Now write the STD_PARALLEL attrib */
    if ( bFoundStdP1 ) { 
        /* one value or equal values */
        if ( !bFoundStdP2 || dfStdP[0] ==  dfStdP[1] ) {
            nc_put_att_double( fpImage, NCDFVarID, CF_PP_STD_PARALLEL, 
                               NC_DOUBLE, 1, &dfStdP[0] );
        }
        else { /* two values */
            nc_put_att_double( fpImage, NCDFVarID, CF_PP_STD_PARALLEL, 
                               NC_DOUBLE, 2, dfStdP );
        }
    }
}

CPLErr NCDFSafeStrcat(char** ppszDest, char* pszSrc, size_t* nDestSize)
{
    /* Reallocate the data string until the content fits */
    while(*nDestSize < (strlen(*ppszDest) + strlen(pszSrc) + 1)) {
        (*nDestSize) *= 2;
        *ppszDest = (char*) CPLRealloc((void*) *ppszDest, *nDestSize);
    }
    strcat(*ppszDest, pszSrc);
    
    return CE_None;
}

/* helper function for NCDFGetAttr() */
/* sets pdfValue to first value returned */
/* and if bSetPszValue=True sets szValue with all values attribute values */
/* pszValue is the responsibility of the caller and must be freed */
CPLErr NCDFGetAttr1( int nCdfId, int nVarId, const char *pszAttrName, 
                     double *pdfValue, char **pszValue, int bSetPszValue )
{
    nc_type nAttrType = NC_NAT;
    size_t  nAttrLen = 0;
    size_t  nAttrValueSize;
    int     status = 0; /*rename this */
    size_t  m;
    char    szTemp[ NCDF_MAX_STR_LEN ];
    char    *pszAttrValue = NULL;
    double  dfValue = 0.0;

    status = nc_inq_att( nCdfId, nVarId, pszAttrName, &nAttrType, &nAttrLen);
    if ( status != NC_NOERR )
        return CE_Failure;

    /* Allocate guaranteed minimum size */
    nAttrValueSize = nAttrLen + 1;
    pszAttrValue = (char *) CPLCalloc( nAttrValueSize, sizeof( char ));
    *pszAttrValue = '\0';

    if ( nAttrLen > 1  && nAttrType != NC_CHAR )    
        NCDFSafeStrcat(&pszAttrValue, (char *)"{ ", &nAttrValueSize);

    switch (nAttrType) {
        case NC_CHAR:
            nc_get_att_text( nCdfId, nVarId, pszAttrName, pszAttrValue );
            pszAttrValue[nAttrLen]='\0';
            dfValue = 0.0;
            break;
        /* TODO support NC_UBYTE */
        case NC_BYTE:
            signed char *pscTemp;
            pscTemp = (signed char *) CPLCalloc( nAttrLen, sizeof( signed char ) );
            nc_get_att_schar( nCdfId, nVarId, pszAttrName, pscTemp );
            dfValue = (double)pscTemp[0];
            for(m=0; m < nAttrLen-1; m++) {
                sprintf( szTemp, "%d, ", pscTemp[m] );
                NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
            }
            sprintf( szTemp, "%d", pscTemp[m] );
            NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
            CPLFree(pscTemp);
            break;
        case NC_SHORT:
            short *psTemp;
            psTemp = (short *) CPLCalloc( nAttrLen, sizeof( short ) );
            nc_get_att_short( nCdfId, nVarId, pszAttrName, psTemp );
            dfValue = (double)psTemp[0];
            for(m=0; m < nAttrLen-1; m++) {
                sprintf( szTemp, "%hd, ", psTemp[m] );
                NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
            }
            sprintf( szTemp, "%hd", psTemp[m] );
            NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
            CPLFree(psTemp);
            break;
        case NC_INT:
            int *pnTemp;
            pnTemp = (int *) CPLCalloc( nAttrLen, sizeof( int ) );
            nc_get_att_int( nCdfId, nVarId, pszAttrName, pnTemp );
            dfValue = (double)pnTemp[0];
            for(m=0; m < nAttrLen-1; m++) {
                sprintf( szTemp, "%d, ", pnTemp[m] );
                NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
            }
            sprintf( szTemp, "%d", pnTemp[m] );
            NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
            CPLFree(pnTemp);
            break;
        case NC_FLOAT:
            float *pfTemp;
            pfTemp = (float *) CPLCalloc( nAttrLen, sizeof( float ) );
            nc_get_att_float( nCdfId, nVarId, pszAttrName, pfTemp );
            dfValue = (double)pfTemp[0];
            for(m=0; m < nAttrLen-1; m++) {
                sprintf( szTemp, "%.8g, ", pfTemp[m] );
                NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
            }
            sprintf( szTemp, "%.8g", pfTemp[m] );
            NCDFSafeStrcat(&pszAttrValue,szTemp, &nAttrValueSize);
            CPLFree(pfTemp);
            break;
        case NC_DOUBLE:
            double *pdfTemp;
            pdfTemp = (double *) CPLCalloc(nAttrLen, sizeof(double));
            nc_get_att_double( nCdfId, nVarId, pszAttrName, pdfTemp );
            dfValue = pdfTemp[0];
            for(m=0; m < nAttrLen-1; m++) {
                sprintf( szTemp, "%.16g, ", pdfTemp[m] );
                NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
            }
            sprintf( szTemp, "%.16g", pdfTemp[m] );
            NCDFSafeStrcat(&pszAttrValue, szTemp, &nAttrValueSize);
            CPLFree(pdfTemp);
            break;
        default:
            CPLDebug( "GDAL_netCDF", "NCDFGetAttr unsupported type %d for attribute %s",
                      nAttrType,pszAttrName);
            CPLFree( pszAttrValue );
            pszAttrValue = NULL;
            break;
    }

    if ( nAttrLen > 1  && nAttrType!= NC_CHAR )    
        NCDFSafeStrcat(&pszAttrValue, (char *)" }", &nAttrValueSize);

    // CPLDebug( "GDAL_netCDF", "NCDFGetAttr got %s=%s / %f",
    //           pszAttrName,pszAttrValue,dfValue);

    /* set return values */
    if ( bSetPszValue == TRUE ) *pszValue = pszAttrValue;
    else CPLFree ( pszAttrValue );
    if ( pdfValue ) *pdfValue = dfValue;

    return CE_None;
}


/* sets pdfValue to first value found */
CPLErr NCDFGetAttr( int nCdfId, int nVarId, const char *pszAttrName, 
                    double *pdfValue )
{
    return NCDFGetAttr1( nCdfId, nVarId, pszAttrName, pdfValue, NULL, FALSE );
}

/* pszValue is the responsibility of the caller and must be freed */
CPLErr NCDFGetAttr( int nCdfId, int nVarId, const char *pszAttrName, 
                    char **pszValue )
{
    return NCDFGetAttr1( nCdfId, nVarId, pszAttrName, FALSE, pszValue, TRUE );
}


/* By default write NC_CHAR, but detect for int/float/double */
CPLErr NCDFPutAttr( int nCdfId, int nVarId, 
                 const char *pszAttrName, const char *pszValue )
{
    nc_type nAttrType = NC_CHAR;
    nc_type nTmpAttrType = NC_CHAR;
    size_t  nAttrLen = 0;
    int     status = 0;
    size_t  i;
    char    szTemp[ NCDF_MAX_STR_LEN ];
    char    *pszTemp = NULL;
    char    **papszValues = NULL;
    
    int     bIsArray = FALSE;
    int     nValue = 0;
    float   fValue = 0.0f;
    double  dfValue = 0.0;

    if ( EQUAL( pszValue, "" ) )
        return CE_Failure;

    strcpy( szTemp,pszValue );

    /* tokenize to find multiple values */
    int last_char = strlen(pszValue) - 1;
    if ( ( szTemp[0] == '{' ) && ( szTemp[last_char] == '}' ) ) {
        bIsArray = TRUE;
        szTemp[0] = ' '; 
        szTemp[last_char] = ' ';
    }
    papszValues = CSLTokenizeString2( szTemp, ",", CSLT_STRIPLEADSPACES |
                                      CSLT_STRIPENDSPACES );
    if ( papszValues == NULL )
        return CE_Failure;

    nAttrLen = CSLCount(papszValues);
    
    /* first detect type */
    nAttrType = NC_CHAR;
    for ( i=0; i<nAttrLen; i++ ) {
        nTmpAttrType = NC_CHAR;
        errno = 0;
        nValue = strtol( papszValues[i], &pszTemp, 10 );
        /* test for int */
        /* TODO test for Byte and short - can this be done safely? */
        if ( (errno == 0) && (papszValues[i] != pszTemp) && (*pszTemp == 0) ) {
            nTmpAttrType = NC_INT;
        }
        else {
            /* test for double */
            errno = 0;
            dfValue = strtod( papszValues[i], &pszTemp );
            if ( (errno == 0) && (papszValues[i] != pszTemp) && (*pszTemp == 0) ) {
                /* test for float instead of double */
                /* strtof() is C89, which is not available in MSVC */
                /* see if we loose precision if we cast to float and write to char* */
                fValue = (float)dfValue; 
                sprintf( szTemp,"%.8g",fValue); 
                if ( EQUAL(szTemp, papszValues[i] ) )
                    nTmpAttrType = NC_FLOAT;
                else
                    nTmpAttrType = NC_DOUBLE;                   
            }
        }
        if ( nTmpAttrType > nAttrType )
            nAttrType = nTmpAttrType;
    }

    /* now write the data */
    if ( !bIsArray && nAttrType == NC_CHAR ) {
        status = nc_put_att_text( nCdfId, nVarId, pszAttrName,
                                  strlen( pszValue ), pszValue );
        NCDF_ERR(status);                        
    }
    else {
        
        switch( nAttrType ) {
            case  NC_INT:
                int *pnTemp;
                pnTemp = (int *) CPLCalloc( nAttrLen, sizeof( int ) );
                for(i=0; i < nAttrLen; i++) {
                    pnTemp[i] = strtol( papszValues[i], &pszTemp, 10 );
                }
                status = nc_put_att_int( nCdfId, nVarId, pszAttrName, 
                                         NC_INT, nAttrLen, pnTemp );  
                NCDF_ERR(status);
                CPLFree(pnTemp);
            break;
            case  NC_FLOAT:
                float *pfTemp;
                pfTemp = (float *) CPLCalloc( nAttrLen, sizeof( float ) );
                for(i=0; i < nAttrLen; i++) {
                    pfTemp[i] = (float)strtod( papszValues[i], &pszTemp );
                }
                status = nc_put_att_float( nCdfId, nVarId, pszAttrName, 
                                           NC_FLOAT, nAttrLen, pfTemp );  
                NCDF_ERR(status);
                CPLFree(pfTemp);
            break;
            case  NC_DOUBLE:
                double *pdfTemp;
                pdfTemp = (double *) CPLCalloc( nAttrLen, sizeof( double ) );
                for(i=0; i < nAttrLen; i++) {
                    pdfTemp[i] = strtod( papszValues[i], &pszTemp );
                }
                status = nc_put_att_double( nCdfId, nVarId, pszAttrName, 
                                            NC_DOUBLE, nAttrLen, pdfTemp );
                NCDF_ERR(status);
                CPLFree(pdfTemp);
            break;
        default:
            if ( papszValues ) CSLDestroy( papszValues );
            return CE_Failure;
            break;
        }   
    }

    if ( papszValues ) CSLDestroy( papszValues );

     return CE_None;
}
