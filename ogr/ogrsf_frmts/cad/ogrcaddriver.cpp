/*******************************************************************************
 *  Project: OGR CAD Driver
 *  Purpose: Implements driver based on libopencad
 *  Author: Alexandr Borzykh, mush3d at gmail.com
 *  Author: Dmitry Baryshnikov, polimax@mail.ru
 *  Language: C++
 *******************************************************************************
 *  The MIT License (MIT)
 *
 *  Copyright (c) 2016 Alexandr Borzykh
 *  Copyright (c) 2016, NextGIS
 *
 *  Permission is hereby granted, free of charge, to any person obtaining a copy
 *  of this software and associated documentation files (the "Software"), to deal
 *  in the Software without restriction, including without limitation the rights
 *  to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 *  copies of the Software, and to permit persons to whom the Software is
 *  furnished to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be included in all
 *  copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 *  IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 *  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 *  AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 *  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 *  OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 *  SOFTWARE.
 *******************************************************************************/
#include "ogr_cad.h"
#include "vsilfileio.h"

/************************************************************************/
/*                           OGRCADDriverIdentify()                     */
/************************************************************************/

static int OGRCADDriverIdentify( GDALOpenInfo *poOpenInfo )
{
    if ( poOpenInfo->nHeaderBytes < 6 )
        return FALSE;

    if ( poOpenInfo->pabyHeader[0] != 'A' ||
        poOpenInfo->pabyHeader[1] != 'C' )
        return FALSE;

    return IdentifyCADFile ( new VSILFileIO( poOpenInfo->pszFilename ), true ) == 0 ?
        FALSE : TRUE;
}

/************************************************************************/
/*                           OGRCADDriverOpen()                         */
/************************************************************************/

static GDALDataset *OGRCADDriverOpen( GDALOpenInfo* poOpenInfo )
{
    long nSubRasterLayer = -1;
    long nSubRasterFID = -1;

    CADFileIO* pFileIO;
    if ( STARTS_WITH_CI(poOpenInfo->pszFilename, "CAD:") )
    {
        char** papszTokens = CSLTokenizeString2( poOpenInfo->pszFilename, ":", 0 );
        int nTokens = CSLCount( papszTokens );
        if( nTokens < 4 )
        {
            CSLDestroy(papszTokens);
            return nullptr;
        }

        CPLString osFilename;
        for( int i = 1; i < nTokens - 2; ++i )
        {
            if( osFilename.empty() )
                osFilename += ":";
            osFilename += papszTokens[i];
        }

        pFileIO = new VSILFileIO( osFilename );
        nSubRasterLayer = atol( papszTokens[nTokens - 2] );
        nSubRasterFID = atol( papszTokens[nTokens - 1] );

        CSLDestroy( papszTokens );
    }
    else
    {
        pFileIO = new VSILFileIO( poOpenInfo->pszFilename );
    }

    if ( IdentifyCADFile( pFileIO, false ) == FALSE )
    {
        delete pFileIO;
        return nullptr;
    }


/* -------------------------------------------------------------------- */
/*      Confirm the requested access is supported.                      */
/* -------------------------------------------------------------------- */
    if( poOpenInfo->eAccess == GA_Update )
    {
        CPLError( CE_Failure, CPLE_NotSupported,
                  "The CAD driver does not support update access to existing"
                  " datasets.\n" );
        delete pFileIO;
        return nullptr;
    }

    GDALCADDataset *poDS = new GDALCADDataset();
    if( !poDS->Open( poOpenInfo, pFileIO, nSubRasterLayer, nSubRasterFID ) )
    {
        delete poDS;
        return nullptr;
    }
    else
        return poDS;
}

/************************************************************************/
/*                           RegisterGDALCAD()                          */
/************************************************************************/

void RegisterOGRCAD()
{
    GDALDriver  *poDriver;

    if ( GDALGetDriverByName( "CAD" ) == nullptr )
    {
        poDriver = new GDALDriver();
        poDriver->SetDescription( "CAD" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "AutoCAD Driver" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "dwg" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/vector/cad.html" );
        poDriver->SetMetadataItem( GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES" );

        poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST, "<OpenOptionList>"
"  <Option name='MODE' type='string' description='Open mode. READ_ALL - read all data (slow), READ_FAST - read main data (fast), READ_FASTEST - read less data' default='READ_FAST'/>"
"  <Option name='ADD_UNSUPPORTED_GEOMETRIES_DATA' type='string' description='Add unsupported geometries data (color, attributes) to the layer (YES/NO). They will have no geometrical representation.' default='NO'/>"
"</OpenOptionList>");


        poDriver->pfnOpen = OGRCADDriverOpen;
        poDriver->pfnIdentify = OGRCADDriverIdentify;
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        poDriver->SetMetadataItem( GDAL_DCAP_FEATURE_STYLES, "YES" );
        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}

CPLString CADRecode( const CPLString& sString, int CADEncoding )
{
    const char* const apszSource[] = {
        /* 0 UNDEFINED */ "",
        /* 1 ASCII */ "US-ASCII",
        /* 2 8859_1 */ "ISO-8859-1",
        /* 3 8859_2 */ "ISO-8859-2",
        /* 4 UNDEFINED */ "",
        /* 5 8859_4 */ "ISO-8859-4",
        /* 6 8859_5 */ "ISO-8859-5",
        /* 7 8859_6 */ "ISO-8859-6",
        /* 8 8859_7 */ "ISO-8859-7",
        /* 9 8859_8 */ "ISO-8859-8",
        /* 10 8859_9 */ "ISO-8859-9",
        /* 11 DOS437 */ "CP437",
        /* 12 DOS850 */ "CP850",
        /* 13 DOS852 */ "CP852",
        /* 14 DOS855 */ "CP855",
        /* 15 DOS857 */ "CP857",
        /* 16 DOS860 */ "CP860",
        /* 17 DOS861 */ "CP861",
        /* 18 DOS863 */ "CP863",
        /* 19 DOS864 */ "CP864",
        /* 20 DOS865 */ "CP865",
        /* 21 DOS869 */ "CP869",
        /* 22 DOS932 */ "CP932",
        /* 23 MACINTOSH */ "MACINTOSH",
        /* 24 BIG5 */ "BIG5",
        /* 25 KSC5601 */ "CP949",
        /* 26 JOHAB */ "JOHAB",
        /* 27 DOS866 */ "CP866",
        /* 28 ANSI_1250 */ "CP1250",
        /* 29 ANSI_1251 */ "CP1251",
        /* 30 ANSI_1252 */ "CP1252",
        /* 31 GB2312 */ "GB2312",
        /* 32 ANSI_1253 */ "CP1253",
        /* 33 ANSI_1254 */ "CP1254",
        /* 34 ANSI_1255 */ "CP1255",
        /* 35 ANSI_1256 */ "CP1256",
        /* 36 ANSI_1257 */ "CP1257",
        /* 37 ANSI_874 */ "CP874",
        /* 38 ANSI_932 */ "CP932",
        /* 39 ANSI_936 */ "CP936",
        /* 40 ANSI_949 */ "CP949",
        /* 41 ANSI_950 */ "CP950",
        /* 42 ANSI_1361 */ "CP1361",
        /* 43 ANSI_1200 */ "UTF-16",
        /* 44 ANSI_1258 */ "CP1258"
    };

    if( CADEncoding > 0 &&
        CADEncoding < static_cast<int>(CPL_ARRAYSIZE(apszSource)) &&
        CADEncoding != 4 )
    {
        char* pszRecoded = CPLRecode( sString, apszSource[CADEncoding], CPL_ENC_UTF8 );
        CPLString soRecoded(pszRecoded);
        CPLFree(pszRecoded);
        return soRecoded;
    }
    CPLError( CE_Failure, CPLE_NotSupported,
            "CADRecode() function does not support provided CADEncoding." );
    return CPLString("");
}
