/******************************************************************************
 *
 * Project:  GDAL
 * Purpose:  PDF driver
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

#include "pdfdrivercore.h"

static const char *const szOpenOptionList =
    "<OpenOptionList>"
#if defined(HAVE_POPPLER) || defined(HAVE_PDFIUM)
    "  <Option name='RENDERING_OPTIONS' type='string-select' "
    "description='Which graphical elements to render' "
    "default='RASTER,VECTOR,TEXT' "
    "alt_config_option='GDAL_PDF_RENDERING_OPTIONS'>"
    "     <Value>RASTER,VECTOR,TEXT</Value>\n"
    "     <Value>RASTER,VECTOR</Value>\n"
    "     <Value>RASTER,TEXT</Value>\n"
    "     <Value>RASTER</Value>\n"
    "     <Value>VECTOR,TEXT</Value>\n"
    "     <Value>VECTOR</Value>\n"
    "     <Value>TEXT</Value>\n"
    "  </Option>"
#endif
    "  <Option name='DPI' type='float' description='Resolution in Dot Per "
    "Inch' default='72' alt_config_option='GDAL_PDF_DPI'/>"
    "  <Option name='USER_PWD' type='string' description='Password' "
    "alt_config_option='PDF_USER_PWD'/>"
#ifdef HAVE_MULTIPLE_PDF_BACKENDS
    "  <Option name='PDF_LIB' type='string-select' description='Which "
    "underlying PDF library to use' "
#if defined(HAVE_PDFIUM)
    "default='PDFIUM'"
#elif defined(HAVE_POPPLER)
    "default='POPPLER'"
#elif defined(HAVE_PODOFO)
    "default='PODOFO'"
#endif  // ~ default PDF_LIB
    " alt_config_option='GDAL_PDF_LIB'>"
#if defined(HAVE_POPPLER)
    "     <Value>POPPLER</Value>\n"
#endif  // HAVE_POPPLER
#if defined(HAVE_PODOFO)
    "     <Value>PODOFO</Value>\n"
#endif  // HAVE_PODOFO
#if defined(HAVE_PDFIUM)
    "     <Value>PDFIUM</Value>\n"
#endif  // HAVE_PDFIUM
    "  </Option>"
#endif  // HAVE_MULTIPLE_PDF_BACKENDS
    "  <Option name='LAYERS' type='string' description='List of layers (comma "
    "separated) to turn ON (or ALL to turn all layers ON)' "
    "alt_config_option='GDAL_PDF_LAYERS'/>"
    "  <Option name='LAYERS_OFF' type='string' description='List of layers "
    "(comma separated) to turn OFF' alt_config_option='GDAL_PDF_LAYERS_OFF'/>"
    "  <Option name='BANDS' type='string-select' description='Number of raster "
    "bands' default='3' alt_config_option='GDAL_PDF_BANDS'>"
    "     <Value>3</Value>\n"
    "     <Value>4</Value>\n"
    "  </Option>"
    "  <Option name='NEATLINE' type='string' description='The name of the "
    "neatline to select' alt_config_option='GDAL_PDF_NEATLINE'/>"
    "</OpenOptionList>";

const char *PDFGetOpenOptionList()
{
    return szOpenOptionList;
}

/************************************************************************/
/*                        PDFDatasetIdentify()                          */
/************************************************************************/

int PDFDatasetIdentify(GDALOpenInfo *poOpenInfo)
{
    if (STARTS_WITH(poOpenInfo->pszFilename, "PDF:"))
        return TRUE;
    if (STARTS_WITH(poOpenInfo->pszFilename, "PDF_IMAGE:"))
        return TRUE;

    if (poOpenInfo->nHeaderBytes < 128)
        return FALSE;

    return STARTS_WITH((const char *)poOpenInfo->pabyHeader, "%PDF");
}

/************************************************************************/
/*                      PDFDriverSetCommonMetadata()                    */
/************************************************************************/

void PDFDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_VECTOR, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE_LAYER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Geospatial PDF");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC, "drivers/raster/pdf.html");
    poDriver->SetMetadataItem(GDAL_DMD_EXTENSION, "pdf");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES, "Byte");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONFIELDDATATYPES,
        "Integer Integer64 Real String Date DateTime Time");

#if defined(HAVE_POPPLER) || defined(HAVE_PDFIUM)
    poDriver->SetMetadataItem(GDAL_DCAP_VIRTUALIO, "YES");
#endif

    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_FEATURE_STYLES_READ, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUPPORTED_SQL_DIALECTS, "OGRSQL SQLITE");

#ifdef HAVE_POPPLER
    poDriver->SetMetadataItem("HAVE_POPPLER", "YES");
#endif  // HAVE_POPPLER
#ifdef HAVE_PODOFO
    poDriver->SetMetadataItem("HAVE_PODOFO", "YES");
#endif  // HAVE_PODOFO
#ifdef HAVE_PDFIUM
    poDriver->SetMetadataItem("HAVE_PDFIUM", "YES");
#endif  // HAVE_PDFIUM

    poDriver->SetMetadataItem(GDAL_DS_LAYER_CREATIONOPTIONLIST,
                              "<LayerCreationOptionList/>");

    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>\n"
        "   <Option name='COMPRESS' type='string-select' "
        "description='Compression method for raster data' default='DEFLATE'>\n"
        "     <Value>NONE</Value>\n"
        "     <Value>DEFLATE</Value>\n"
        "     <Value>JPEG</Value>\n"
        "     <Value>JPEG2000</Value>\n"
        "   </Option>\n"
        "   <Option name='STREAM_COMPRESS' type='string-select' "
        "description='Compression method for stream objects' "
        "default='DEFLATE'>\n"
        "     <Value>NONE</Value>\n"
        "     <Value>DEFLATE</Value>\n"
        "   </Option>\n"
        "   <Option name='GEO_ENCODING' type='string-select' "
        "description='Format of geo-encoding' default='ISO32000'>\n"
        "     <Value>NONE</Value>\n"
        "     <Value>ISO32000</Value>\n"
        "     <Value>OGC_BP</Value>\n"
        "     <Value>BOTH</Value>\n"
        "   </Option>\n"
        "   <Option name='NEATLINE' type='string' description='Neatline'/>\n"
        "   <Option name='DPI' type='float' description='DPI' default='72'/>\n"
        "   <Option name='WRITE_USERUNIT' type='boolean' description='Whether "
        "the UserUnit parameter must be written'/>\n"
        "   <Option name='PREDICTOR' type='int' description='Predictor Type "
        "(for DEFLATE compression)'/>\n"
        "   <Option name='JPEG_QUALITY' type='int' description='JPEG quality "
        "1-100' default='75'/>\n"
        "   <Option name='JPEG2000_DRIVER' type='string'/>\n"
        "   <Option name='TILED' type='boolean' description='Switch to tiled "
        "format' default='NO'/>\n"
        "   <Option name='BLOCKXSIZE' type='int' description='Block Width'/>\n"
        "   <Option name='BLOCKYSIZE' type='int' description='Block Height'/>\n"
        "   <Option name='LAYER_NAME' type='string' description='Layer name "
        "for raster content'/>\n"
        "   <Option name='CLIPPING_EXTENT' type='string' description='Clipping "
        "extent for main and extra rasters. Format: xmin,ymin,xmax,ymax'/>\n"
        "   <Option name='EXTRA_RASTERS' type='string' description='List of "
        "extra (georeferenced) rasters.'/>\n"
        "   <Option name='EXTRA_RASTERS_LAYER_NAME' type='string' "
        "description='List of layer names for the extra (georeferenced) "
        "rasters.'/>\n"
        "   <Option name='EXTRA_STREAM' type='string' description='Extra data "
        "to insert into the page content stream'/>\n"
        "   <Option name='EXTRA_IMAGES' type='string' description='List of "
        "image_file_name,x,y,scale[,link=some_url] (possibly repeated)'/>\n"
        "   <Option name='EXTRA_LAYER_NAME' type='string' description='Layer "
        "name for extra content'/>\n"
        "   <Option name='MARGIN' type='int' description='Margin around image "
        "in user units'/>\n"
        "   <Option name='LEFT_MARGIN' type='int' description='Left margin in "
        "user units'/>\n"
        "   <Option name='RIGHT_MARGIN' type='int' description='Right margin "
        "in user units'/>\n"
        "   <Option name='TOP_MARGIN' type='int' description='Top margin in "
        "user units'/>\n"
        "   <Option name='BOTTOM_MARGIN' type='int' description='Bottom margin "
        "in user units'/>\n"
        "   <Option name='OGR_DATASOURCE' type='string' description='Name of "
        "OGR datasource to display on top of the raster layer'/>\n"
        "   <Option name='OGR_DISPLAY_FIELD' type='string' description='Name "
        "of field to use as the display field in the feature tree'/>\n"
        "   <Option name='OGR_DISPLAY_LAYER_NAMES' type='string' "
        "description='Comma separated list of OGR layer names to display in "
        "the feature tree'/>\n"
        "   <Option name='OGR_WRITE_ATTRIBUTES' type='boolean' "
        "description='Whether to write attributes of OGR features' "
        "default='YES'/>\n"
        "   <Option name='OGR_LINK_FIELD' type='string' description='Name of "
        "field to use as the URL field to make objects clickable.'/>\n"
        "   <Option name='XMP' type='string' description='xml:XMP metadata'/>\n"
        "   <Option name='WRITE_INFO' type='boolean' description='to control "
        "whether a Info block must be written' default='YES'/>\n"
        "   <Option name='AUTHOR' type='string'/>\n"
        "   <Option name='CREATOR' type='string'/>\n"
        "   <Option name='CREATION_DATE' type='string'/>\n"
        "   <Option name='KEYWORDS' type='string'/>\n"
        "   <Option name='PRODUCER' type='string'/>\n"
        "   <Option name='SUBJECT' type='string'/>\n"
        "   <Option name='TITLE' type='string'/>\n"
        "   <Option name='OFF_LAYERS' type='string' description='Comma "
        "separated list of layer names that should be initially hidden'/>\n"
        "   <Option name='EXCLUSIVE_LAYERS' type='string' description='Comma "
        "separated list of layer names, such that only one of those layers can "
        "be ON at a time.'/>\n"
        "   <Option name='JAVASCRIPT' type='string' description='Javascript "
        "script to embed and run at file opening'/>\n"
        "   <Option name='JAVASCRIPT_FILE' type='string' description='Filename "
        "of the Javascript script to embed and run at file opening'/>\n"
        "   <Option name='COMPOSITION_FILE' type='string' description='XML "
        "file describing how the PDF should be composed'/>\n"
        "</CreationOptionList>\n");

#ifdef HAVE_PDF_READ_SUPPORT
    poDriver->SetMetadataItem(GDAL_DMD_OPENOPTIONLIST, szOpenOptionList);
    poDriver->pfnIdentify = PDFDatasetIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_MULTIPLE_VECTOR_LAYERS, "YES");
#endif

    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                     DeclareDeferredPDFPlugin()                       */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredPDFPlugin()
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
    PDFDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
