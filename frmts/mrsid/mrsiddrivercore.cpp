/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  MrSID driver
 * Author:   Even Rouault, <even.rouault at spatialys.com>
 *
 ******************************************************************************
 * Copyright (c) 2023, Even Rouault, <even.rouault at spatialys.com>
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

#include "mrsiddrivercore.h"

#include "mrsiddataset_headers_include.h"

/************************************************************************/
/*                         MrSIDIdentify()                              */
/*                                                                      */
/*          Identify method that only supports MrSID files.             */
/************************************************************************/

int MrSIDIdentify(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->nHeaderBytes < 32)
        return FALSE;

    if (!STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader, "msid"))
        return FALSE;

    return TRUE;
}

#ifdef MRSID_J2K

static const unsigned char jpc_header[] = {0xff, 0x4f};

/************************************************************************/
/*                         JP2Identify()                                */
/*                                                                      */
/*        Identify method that only supports JPEG2000 files.            */
/************************************************************************/

int MrSIDJP2Identify(GDALOpenInfo *poOpenInfo)
{
    if (poOpenInfo->nHeaderBytes < 32)
        return FALSE;

    if (memcmp(poOpenInfo->pabyHeader, jpc_header, sizeof(jpc_header)) == 0)
    {
        const char *pszExtension = CPLGetExtension(poOpenInfo->pszFilename);

        if (!EQUAL(pszExtension, "jpc") && !EQUAL(pszExtension, "j2k") &&
            !EQUAL(pszExtension, "jp2") && !EQUAL(pszExtension, "jpx") &&
            !EQUAL(pszExtension, "j2c") && !EQUAL(pszExtension, "ntf"))
            return FALSE;
    }
    else if (!STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader + 4, "jP  "))
        return FALSE;

    return TRUE;
}

#endif

/************************************************************************/
/*                     MrSIDDriverSetCommonMetadata()                   */
/************************************************************************/

void MrSIDDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(MRSID_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "Multi-resolution Seamless Image Database "
                              "(MrSID)");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/mrsid.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "sid");

#ifdef MRSID_ESDK
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte Int16 UInt16 Int32 UInt32 "
                              "Float32 Float64");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        // Version 2 Options
        "   <Option name='COMPRESSION' type='double' description='Set "
        "compression ratio (0.0 default is meant to be lossless)'/>"
        // Version 3 Options
        "   <Option name='TWOPASS' type='int' description='Use twopass "
        "optimizer algorithm'/>"
        "   <Option name='FILESIZE' type='int' description='Set target file "
        "size (0 implies lossless compression)'/>"
        // Version 2 and 3 Option
        "   <Option name='WORLDFILE' type='boolean' description='Write out "
        "world file'/>"
        // Version Type
        "   <Option name='VERSION' type='int' description='Valid versions are "
        "2 and 3, default = 3'/>"
        "</CreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
#else
    // In read-only mode, we support VirtualIO. I don't think this is the case
    // for MrSIDCreateCopy().
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
#endif
    poDriver->pfnIdentify = MrSIDIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

/************************************************************************/
/*                  JP2MrSIDDriverSetCommonMetadata()                   */
/************************************************************************/

#ifdef MRSID_J2K

void JP2MrSIDDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(JP2MRSID_DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "MrSID JPEG2000");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/jp2mrsid.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "jp2");

#ifdef MRSID_ESDK
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte Int16 UInt16");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "   <Option name='COMPRESSION' type='double' description='Set "
        "compression ratio (0.0 default is meant to be lossless)'/>"
        "   <Option name='WORLDFILE' type='boolean' description='Write out "
        "world file'/>"
        "   <Option name='XMLPROFILE' type='string' description='Use named xml "
        "profile file'/>"
        "</CreationOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
#else
    /* In read-only mode, we support VirtualIO. I don't think this is the case
     */
    /* for JP2CreateCopy() */
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
#endif
    poDriver->pfnIdentify = MrSIDJP2Identify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
}

#endif

/************************************************************************/
/*                     DeclareDeferredMrSIDPlugin()                     */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredMrSIDPlugin()
{
    if (GDALGetDriverByName(MRSID_DRIVER_NAME) != nullptr)
    {
        return;
    }
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        MrSIDDriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
#ifdef MRSID_J2K
    {
        auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
        poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                                  PLUGIN_INSTALLATION_MESSAGE);
#endif
        JP2MrSIDDriverSetCommonMetadata(poDriver);
        GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
    }
#endif
}
#endif
