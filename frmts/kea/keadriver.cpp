/*
 *  keadriver.cpp
 *
 *  Created by Pete Bunting on 01/08/2012.
 *  Copyright 2012 LibKEA. All rights reserved.
 *
 *  This file is part of LibKEA.
 *
 *  Permission is hereby granted, free of charge, to any person
 *  obtaining a copy of this software and associated documentation
 *  files (the "Software"), to deal in the Software without restriction,
 *  including without limitation the rights to use, copy, modify,
 *  merge, publish, distribute, sublicense, and/or sell copies of the
 *  Software, and to permit persons to whom the Software is furnished
 *  to do so, subject to the following conditions:
 *
 *  The above copyright notice and this permission notice shall be
 *  included in all copies or substantial portions of the Software.
 *
 *  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 *  EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 *  OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 *  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR
 *  ANY CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF
 *  CONTRACT, TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION
 *  WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 *
 */

#include "gdal_frmts.h"
#include "keadataset.h"

CPL_CVSID("$Id$")

// method to register this driver
void GDALRegister_KEA()
{
    if( !GDAL_CHECK_VERSION( "KEA" ) )
        return;

    if( GDALGetDriverByName( "KEA" ) != nullptr )
      return;

    GDALDriver *poDriver = new GDALDriver();

    poDriver->SetDescription( "KEA" );
    poDriver->SetMetadataItem( GDAL_DCAP_RASTER, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME, "KEA Image Format (.kea)" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSION, "kea" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drivers/raster/kea.html" );
    poDriver->SetMetadataItem( GDAL_DMD_CREATIONDATATYPES,
                               "Byte Int16 UInt16 Int32 UInt32 "
                               "Float32 Float64" );
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        CPLSPrintf(
"<CreationOptionList> "
"<Option name='IMAGEBLOCKSIZE' type='int' description='The size of each block for image data' default='%d'/> "
"<Option name='ATTBLOCKSIZE' type='int' description='The size of each block for attribute data' default='%d'/> "
"<Option name='MDC_NELMTS' type='int' description='Number of elements in the meta data cache' default='%d'/> "
"<Option name='RDCC_NELMTS' type='int' description='Number of elements in the raw data chunk cache' default='%d'/> "
"<Option name='RDCC_NBYTES' type='int' description='Total size of the raw data chunk cache, in bytes' default='%d'/> "
"<Option name='RDCC_W0' type='float' min='0' max='1' description='Preemption policy' default='%.2f'/> "
"<Option name='SIEVE_BUF' type='int' description='Sets the maximum size of the data sieve buffer' default='%d'/> "
"<Option name='META_BLOCKSIZE' type='int' description='Sets the minimum size of metadata block allocations' default='%d'/> "
"<Option name='DEFLATE' type='int' description='0 (no compression) to 9 (max compression)' default='%d'/> "
"<Option name='THEMATIC' type='boolean' description='If YES then all bands are set to thematic' default='NO'/> "
"</CreationOptionList>",
        static_cast<int>(kealib::KEA_IMAGE_CHUNK_SIZE),
        static_cast<int>(kealib::KEA_ATT_CHUNK_SIZE),
        static_cast<int>(kealib::KEA_MDC_NELMTS),
        static_cast<int>(kealib::KEA_RDCC_NELMTS),
        static_cast<int>(kealib::KEA_RDCC_NBYTES),
        kealib::KEA_RDCC_W0,
        static_cast<int>(kealib::KEA_SIEVE_BUF),
        static_cast<int>(kealib::KEA_META_BLOCKSIZE),
        kealib::KEA_DEFLATE ) );
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = KEADataset::Open;
    poDriver->pfnIdentify = KEADataset::Identify;
    poDriver->pfnCreate = KEADataset::Create;
    poDriver->pfnCreateCopy = KEADataset::CreateCopy;
    poDriver->pfnUnloadDriver = KEADatasetDriverUnload;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
