/******************************************************************************
 *
 * Project:  NITF Read/Write Translator
 * Purpose:  NITFDataset and driver related implementations.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2002, Frank Warmerdam
 * Copyright (c) 2007-2013, Even Rouault <even dot rouault at spatialys.com>
 *
 * Portions Copyright (c) Her majesty the Queen in right of Canada as
 * represented by the Minister of National Defence, 2006.
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

#include "nitfdrivercore.h"

/************************************************************************/
/*                     NITFDriverIdentify()                             */
/************************************************************************/

int NITFDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    const char *pszFilename = poOpenInfo->pszFilename;

    /* -------------------------------------------------------------------- */
    /*      Is this a dataset selector? If so, it is obviously NITF.        */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszFilename, "NITF_IM:"))
        return TRUE;

    /* -------------------------------------------------------------------- */
    /*      Avoid that on Windows, JPEG_SUBFILE:x,y,z,data/../tmp/foo.ntf   */
    /*      to be recognized by the NITF driver, because                    */
    /*      'JPEG_SUBFILE:x,y,z,data' is considered as a (valid) directory  */
    /*      and thus the whole filename is evaluated as tmp/foo.ntf         */
    /* -------------------------------------------------------------------- */
    if (STARTS_WITH_CI(pszFilename, "JPEG_SUBFILE:"))
        return FALSE;

    /* -------------------------------------------------------------------- */
    /*      First we check to see if the file has the expected header       */
    /*      bytes.                                                          */
    /* -------------------------------------------------------------------- */
    if (poOpenInfo->nHeaderBytes < 4)
        return FALSE;

    if (!STARTS_WITH_CI((char *)poOpenInfo->pabyHeader, "NITF") &&
        !STARTS_WITH_CI((char *)poOpenInfo->pabyHeader, "NSIF") &&
        !STARTS_WITH_CI((char *)poOpenInfo->pabyHeader, "NITF"))
        return FALSE;

    /* Check that it is not in fact a NITF A.TOC file, which is handled by the
     * RPFTOC driver */
    for (int i = 0; i < static_cast<int>(poOpenInfo->nHeaderBytes) -
                            static_cast<int>(strlen("A.TOC"));
         i++)
    {
        if (STARTS_WITH_CI((const char *)poOpenInfo->pabyHeader + i, "A.TOC"))
            return FALSE;
    }

    return TRUE;
}

/************************************************************************/
/*                     NITFDriverSetCommonMetadata()                    */
/************************************************************************/

void NITFDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME,
                              "National Imagery Transmission Format");

    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/nitf.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "ntf");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte UInt16 Int16 UInt32 Int32 Float32");

    poDriver->SetMetadataItem(
        GDAL_DMD_OPENOPTIONLIST,
        "<OpenOptionList>"
        "  <Option name='VALIDATE' type='boolean' description='Whether "
        "validation of metadata should be done' default='NO' />"
        "  <Option name='FAIL_IF_VALIDATION_ERROR' type='boolean' "
        "description='Whether a validation error should cause dataset opening "
        "to fail' default='NO' />"
        "</OpenOptionList>");

    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");

    poDriver->pfnIdentify = NITFDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                    DeclareDeferredNITFPlugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredNITFPlugin()
{
    if (GDALGetDriverByName(DRIVER_NAME) != nullptr)
    {
        return;
    }
    auto poDriver = new GDALPluginDriverProxy(PLUGIN_FILENAME);
#ifdef PLUGIN_INSTALLATION_MESSAGE
    poDriver->SetMetadataItem(GDAL_DMD_PLUGIN_INSTALLATION_MESSAGE,
                              PLUGIN_INSTALLATION_MESSAGE);
#endif
    NITFDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
