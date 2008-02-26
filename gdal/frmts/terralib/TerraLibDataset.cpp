/*****************************************************************************
 * $Id: $
 *
 * Project:  TerraLib Raster Database schema support
 * Purpose:  Read TerraLib Raster Dataset (see TerraLib.org)
 * Author:   Ivan Lucena [ivan.lucena@pmldnet.com]
 *
 ******************************************************************************
 * Copyright (c) 2007, Ivan Lucena
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files ( the "Software" ),
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
 *****************************************************************************/

#include "TerraLibDataset.h"
#include "TerraLibRasterBand.h"

CPL_C_START
void CPL_DLL GDALRegister_TERRALIB( void );
CPL_C_END

//  ----------------------------------------------------------------------------
//                                                             TerraLibDataset()
//  ----------------------------------------------------------------------------

TerraLibDataset::TerraLibDataset()
{
    m_ProjectionRef      = NULL;
    m_adfGeoTransform[0] = 0.0;
    m_adfGeoTransform[1] = 1.0;
    m_adfGeoTransform[2] = 0.0;
    m_adfGeoTransform[3] = 0.0;
    m_adfGeoTransform[4] = 0.0;
    m_adfGeoTransform[5] = 1.0;
    m_bGeoTransformValid = false;
}

//  ----------------------------------------------------------------------------
//                                                             TerraLibDataset()
//  ----------------------------------------------------------------------------

TerraLibDataset::~TerraLibDataset()
{
    if( m_ProjectionRef )
    {
        CPLFree( m_ProjectionRef );
    }

    if( m_db )
    {
        m_db->close();
    }
}

//  ----------------------------------------------------------------------------
//                                                             TerraLibDataset()
//  ----------------------------------------------------------------------------

GDALDataset *TerraLibDataset::Open( GDALOpenInfo *poOpenInfo )
{
    //  -------------------------------------------------------------------
    //  Verify georaster prefix
    //  -------------------------------------------------------------------

    if( ! poOpenInfo->fp == NULL )
    {
        return NULL;
    }

    if( ! EQUALN( poOpenInfo->pszFilename, "terralib:", 9 ) )
    {
        return NULL;
    }

    //  -------------------------------------------------------------------
    //  Parser the arguments
    //  -------------------------------------------------------------------

    char **papszParam = CSLTokenizeString2( poOpenInfo->pszFilename + 9, ",", 
                        CSLT_HONOURSTRINGS | CSLT_ALLOWEMPTYTOKENS );

    int nArgc = CSLCount( papszParam );

    //  ------------------------------------------------------------------------
    //  Check parameters:
    //  ------------------------------------------------------------------------

    if( nArgc < 6 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Incorrect number of paramters (%d). \n"
            "TERRALIB:<rdbms>,<host>,<user>,<password>,<database>,<layer>\n", nArgc );
        CPLFree( papszParam[0] );
        return FALSE;
    }

    const char *pszRDBMS     = CPLStrdup( papszParam[0] );
    const char *pszHost      = CPLStrdup( papszParam[1] );
    const char *pszUser      = CPLStrdup( papszParam[2] );
    const char *pszPassword  = CPLStrdup( papszParam[3] );
    const char *pszDatabase  = CPLStrdup( papszParam[4] );
    const char *pszLayer     = CPLStrdup( papszParam[5] );

    CSLDestroy( papszParam );

    // -------------------------------------------------------------------- 
    // Create Dataset
    // -------------------------------------------------------------------- 

    TerraLibDataset* poDS = new TerraLibDataset;
    poDS->eAccess         = poOpenInfo->eAccess;

    //  ------------------------------------------------------------------------
    //  Create a ADO handler
    //  ------------------------------------------------------------------------

    if( EQUAL( pszRDBMS, "ADO" ) ||
        EQUAL( pszRDBMS, "SQLServer" ) ||
        EQUAL( pszRDBMS, "OracleADO" ) )
    {
        poDS->m_db = new TeAdo();
    }

    //  --------------------------------------------------------------------
    //  Create a MySQL handler
    //  --------------------------------------------------------------------

    if( EQUAL( pszRDBMS, "MySQL" ) )
    {
        if( EQUAL( pszHost, "" ) )
        {
            pszHost = CPLStrdup( "localhost" );
        }

        if( EQUAL( pszUser, "" ) )
        {
            pszUser = CPLStrdup( "localuser" );
        }

        poDS->m_db = new TeMySQL();
    }

    //  --------------------------------------------------------------------
    //  Create PostgreSQL/PostGIS handler
    //  --------------------------------------------------------------------

    if( EQUAL( pszRDBMS, "PostgreSQL" ) || 
        EQUAL( pszRDBMS, "PostGIS" ) )
    {
        poDS->m_db = new TeMySQL();
    }

    //  --------------------------------------------------------------------
    //  Connect to database
    //  --------------------------------------------------------------------

    if ( ! poDS->m_db->connect( pszHost, pszUser, pszPassword, pszDatabase ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, (CPLString) poDS->m_db->errorMessage() );
        delete poDS;
        return NULL;
    }

    //  --------------------------------------------------------------------
    //  Look for layer
    //  --------------------------------------------------------------------

    if ( ! poDS->m_db->layerExist( pszLayer ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, (CPLString) poDS->m_db->errorMessage() );
        delete poDS;
        return NULL;
    }

    poDS->m_layer = new TeLayer( pszLayer );

    if( ! poDS->m_db->loadLayer( poDS->m_layer ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, (CPLString) poDS->m_db->errorMessage() );
        delete poDS;
        return FALSE;
    }

    //  --------------------------------------------------------------------
    //  Look for raster
    //  --------------------------------------------------------------------

    poDS->m_raster = poDS->m_layer->raster();

    if( ! poDS->m_raster )
    {
        CPLError( CE_Failure, CPLE_AppDefined, (CPLString) poDS->m_db->errorMessage() );
        delete poDS;
        return NULL;
    }

    poDS->m_params = poDS->m_raster->params();

    // -------------------------------------------------------------------- 
    // Load raster parameters
    // -------------------------------------------------------------------- 

    poDS->nRasterXSize          = poDS->m_params.ncols_;
    poDS->nRasterYSize          = poDS->m_params.nlines_;
    poDS->nBands                = poDS->m_params.nBands();
    poDS->m_ProjectionRef       = CPLStrdup( TeGetWKTFromTeProjection( poDS->m_params.projection() ).c_str() );
    poDS->m_adfGeoTransform[0]  = poDS->m_params.box().x1_;
    poDS->m_adfGeoTransform[1]  = poDS->m_params.resx_;
    poDS->m_adfGeoTransform[2]  = 0.0;
    poDS->m_adfGeoTransform[3]  = poDS->m_params.box().y2_;
    poDS->m_adfGeoTransform[4]  = 0.0;
    poDS->m_adfGeoTransform[5]  = poDS->m_params.resy_;
    poDS->m_bGeoTransformValid  = true;

    // -------------------------------------------------------------------- 
    // Create Band(s) Information
    // -------------------------------------------------------------------- 

    int nBands = 0;

    do
    {
        nBands++;
        poDS->SetBand( nBands, new TerraLibRasterBand( poDS ) );
    }
    while( nBands < poDS->nBands );

    return (GDALDataset*) poDS;
}

//  ----------------------------------------------------------------------------
//                                                                      Create()
//  ----------------------------------------------------------------------------

GDALDataset *TerraLibDataset::Create( const char *pszFilename,
    int nXSize,
    int nYSize,
    int nBands, 
    GDALDataType eType,
    char **papszOptions )
{
    return NULL;
}

//  ----------------------------------------------------------------------------
//                                                                  CreateCopy()
//  ----------------------------------------------------------------------------

GDALDataset *TerraLibDataset::CreateCopy( const char *pszFilename, 
    GDALDataset *poSrcDS,
    int bStrict,
    char **papszOptions,
    GDALProgressFunc pfnProgress, 
    void * pProgressData )
{
    return NULL;
}

//  ----------------------------------------------------------------------------
//                                                             GetGeoTransform()
//  ----------------------------------------------------------------------------

CPLErr TerraLibDataset::GetGeoTransform( double *padfTransform )
{
    memcpy( padfTransform, m_adfGeoTransform, sizeof(double) * 6 );

    if( ! m_bGeoTransformValid )
        return CE_Failure;
    else
        return CE_None;
}

//  ----------------------------------------------------------------------------
//                                                             SetGeoTransform()
//  ----------------------------------------------------------------------------

CPLErr TerraLibDataset::SetGeoTransform( double *padfTransform )
{
    return CE_None;
}

//  ----------------------------------------------------------------------------
//                                                            GetProjectionRef()
//  ----------------------------------------------------------------------------

const char* TerraLibDataset::GetProjectionRef( void )
{
    return m_ProjectionRef;
}

//  ----------------------------------------------------------------------------
//                                                               SetProjection()
//  ----------------------------------------------------------------------------

CPLErr TerraLibDataset::SetProjection( const char *pszProjString )
{
    return CE_None;
}


//  ----------------------------------------------------------------------------
//                                                       GDALRegister_TERRALIB()
//  ----------------------------------------------------------------------------

void CPL_DLL GDALRegister_TERRALIB( void )
{
    GDALDriver  *poDriver;

    if( GDALGetDriverByName( "TerraLib" ) == NULL )
    {
        poDriver = new GDALDriver();

        poDriver->SetDescription( "TerraLib" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "TerraLib Raster RDMS Schema" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "frmt_TerraLib.html" );
        poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES, 
            "Byte Int16 Int32 Float32 Float64" );
        
        poDriver->pfnOpen       = TerraLibDataset::Open;
        poDriver->pfnCreate     = TerraLibDataset::Create;
        poDriver->pfnCreateCopy = TerraLibDataset::CreateCopy;

        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
