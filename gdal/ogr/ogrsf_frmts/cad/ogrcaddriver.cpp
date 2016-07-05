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
    if( STARTS_WITH_CI(poOpenInfo->pszFilename, "CAD:") )
        return TRUE;
        
    return IdentifyCADFile( new VSILFileIO( poOpenInfo->pszFilename ) ) == 0 ? 
        FALSE : TRUE;
}

/************************************************************************/
/*                           OGRCADDriverOpen()                         */
/************************************************************************/

static GDALDataset *OGRCADDriverOpen( GDALOpenInfo* poOpenInfo )
{
    CADFileIO* pFileIO = new VSILFileIO( poOpenInfo->pszFilename);
    if ( IdentifyCADFile( pFileIO, false ) == FALSE)
    {
        delete pFileIO;
        return( NULL );
    }
    
    OGRCADDataSource *poDS = new OGRCADDataSource();
    if( !poDS->Open( poOpenInfo, pFileIO ) )
    {
        delete poDS;
        return( NULL );
    }
    else
        return( poDS );
}

/************************************************************************/
/*                           RegisterOGRCAD()                           */
/************************************************************************/

void RegisterOGRCAD()
{
    GDALDriver  *poDriver;
    
    if ( GDALGetDriverByName( "CAD" ) == NULL )
    {
        poDriver = new GDALDriver();
        poDriver->SetDescription( "CAD" );
        poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
        poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_SUBDATASETS, "YES" );
        poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "AutoCAD Driver" );
        poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "dwg" );
        poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_cad.html" );        
        
        poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST, "<OpenOptionList>"
"  <Option name='MODE' type='int' min='1' max='3' description='Open mode. 1 - read all data (slow), 2 - read main data (fast), 3 - read less data' default='2'/>"
"</OpenOptionList>"); 

        
        poDriver->pfnOpen = OGRCADDriverOpen;
        poDriver->pfnIdentify = OGRCADDriverIdentify;
        poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );
        GetGDALDriverManager()->RegisterDriver( poDriver );
    }
}
