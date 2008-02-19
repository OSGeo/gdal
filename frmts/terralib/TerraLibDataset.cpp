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

CPL_C_START
void CPL_DLL GDALRegister_TERRALIB( void );
CPL_C_END

//  ----------------------------------------------------------------------------
//                                                             TerraLibDataset()
//  ----------------------------------------------------------------------------

TerraLibDataset::TerraLibDataset()
{
}

//  ----------------------------------------------------------------------------
//                                                             TerraLibDataset()
//  ----------------------------------------------------------------------------

TerraLibDataset::~TerraLibDataset()
{
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
    poDS->fp              = NULL;

    //  ------------------------------------------------------------------------
    //  Open from Access "database" file
    //  ------------------------------------------------------------------------

    if( EQUAL( pszRDBMS, "ADO" ) ||
        EQUAL( pszRDBMS, "SQLServer" ) ||
        EQUAL( pszRDBMS, "OracleADO" ) )
    {
        poDS->m_db = new TeAdo();
    }

    //  --------------------------------------------------------------------
    //  Open MySQL database
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
    //  Open PostgreSQL
    //  --------------------------------------------------------------------

    if( EQUAL( pszRDBMS, "PostgreSQL" ) || 
        EQUAL( pszRDBMS, "PostGIS" ) )
    {
        poDS->m_db = new TeMySQL();
    }

    //  --------------------------------------------------------------------
    //  Connect to database server
    //  --------------------------------------------------------------------

    if ( ! poDS->m_db->connect( pszHost, pszUser, pszPassword, pszDatabase ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, (CPLString) poDS->m_db->errorMessage() );
        return FALSE;
    }

    //  --------------------------------------------------------------------
    //  Look for layer
    //  --------------------------------------------------------------------

    if ( ! poDS->m_db->layerExist( pszLayer ) )
    {
        return NULL;
    }

    TeLayer* layer = new TeLayer( pszLayer );

    if( ! poDS->m_db->loadLayer( layer ) )
    {
        CPLError( CE_Failure, CPLE_AppDefined, (CPLString) poDS->m_db->errorMessage() );
        return FALSE;
    }

    //  --------------------------------------------------------------------
    //  Look for raster
    //  --------------------------------------------------------------------

    TeRaster* raster      = layer->raster();

    if( ! raster )
    {
        return NULL;
    }

    TeRasterParams params = raster->params();

    // -------------------------------------------------------------------- 
    // Load raster parameters
    // -------------------------------------------------------------------- 

    poDS->nRasterXSize = params.nlines_;
    poDS->nRasterXSize = params.ncols_;
    poDS->nBands       = params.nBands();

    // -------------------------------------------------------------------- 
    // Create Band Information
    // -------------------------------------------------------------------- 

    int i = 0;
/*
    for( i = 0; i < poDS->nBands; i++ )
    {
        poDS->SetBand( i + 1, new TerraLibRasterBand( poDS, i + 1 ) );
    }
*/
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
