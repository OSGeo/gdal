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
    FreeLibrary( (HMODULE) pLibrary );
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

    char **papszParam = CSLTokenizeString2( poOpenInfo->pszFilename, ":", 
                        CSLT_HONOURSTRINGS | CSLT_ALLOWEMPTYTOKENS );

    int nArgc = CSLCount( papszParam );

    //  -------------------------------------------------------------------
    //  Load TerraLib.dll
    //  -------------------------------------------------------------------
/*
    void *pLibrary = LoadLibrary( "terralib.dll" );

    if( pLibrary == NULL )
    {
        return NULL;
    }
*/
    //  ------------------------------------------------------------------------
    //  Check parameters:
    //  ------------------------------------------------------------------------

    if( nArgc < 7 )
    {
        CPLError( CE_Failure, CPLE_AppDefined,
            "Incorrect number of paramters (%d). \n"
            "TERRALIB:<rdbms>:<host>:<user>:<password>:<database>:<layer>.\n", nArgc );
        CPLFree( papszParam[0] );
        return FALSE;
    }

    const char *pszRDBMS     = CPLStrdup( papszParam[1] );
    const char *pszHost      = CPLStrdup( papszParam[2] );
    const char *pszUser      = CPLStrdup( papszParam[3] );
    const char *pszPassword  = CPLStrdup( papszParam[4] );
    const char *pszDatabase  = CPLStrdup( papszParam[5] );
    const char *pszLayer     = CPLStrdup( papszParam[6] );

    if( EQUAL( pszHost, "" ) )
        pszHost = CPLStrdup( "localhost" );

//    if( EQUAL( pszUser, "" ) )
//        pszUser = CPLStrdup( "localuser" );

    CPLFree( papszParam[0] );

    //  ------------------------------------------------------------------------
    //  Select RBDMS:
    //  ------------------------------------------------------------------------

    TeDatabase*     db;

    if( EQUAL( pszRDBMS, "ADO" ) )
    {
        db = new TeAdo();

        if ( ! db->connect( pszHost, pszUser, pszPassword, pszDatabase ) )
        {
            CPLError( CE_Failure, CPLE_AppDefined, 
                (CPLString) db->errorMessage() );
            return FALSE;
        }
    }

    return NULL;

    //  --------------------------------------------------------------------
    //  Open the database:
    //  --------------------------------------------------------------------

    db = new TeMySQL();

    if ( ! db->connect( pszHost, pszUser, pszPassword, pszDatabase ) )
    {
        return NULL;
    }

    //  --------------------------------------------------------------------
    //  Look for layer:
    //  --------------------------------------------------------------------

    if ( ! db->layerExist( pszLayer ) )
    {
        db->close();
        return NULL;
    }

    return NULL;
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
