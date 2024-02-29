/******************************************************************************
 *
 * Name:     georaster_dataset.cpp
 * Project:  Oracle Spatial GeoRaster Driver
 * Purpose:  Implement GeoRasterDataset Methods
 * Author:   Ivan Lucena [ivan.lucena at oracle.com]
 *
 ******************************************************************************
 * Copyright (c) 2008, Ivan Lucena <ivan dot lucena at oracle dot com>
 * Copyright (c) 2013, Even Rouault <even dot rouault at spatialys.com>
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

#include "georasterdrivercore.h"

/************************************************************************/
/*                     GEORDriverIdentify()                             */
/************************************************************************/

int GEORDriverIdentify(GDALOpenInfo *poOpenInfo)

{
    //  -------------------------------------------------------------------
    //  Verify georaster prefix
    //  -------------------------------------------------------------------

    char *pszFilename = poOpenInfo->pszFilename;

    if (STARTS_WITH_CI(pszFilename, "georaster:") == false &&
        STARTS_WITH_CI(pszFilename, "geor:") == false)
    {
        return false;
    }

    return true;
}

/************************************************************************/
/*                      GEORDriverSetCommonMetadata()                   */
/************************************************************************/

void GEORDriverSetCommonMetadata(GDALDriver *poDriver)
{
    poDriver->SetDescription(DRIVER_NAME);
    poDriver->SetMetadataItem(GDAL_DCAP_RASTER, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_LONGNAME, "Oracle Spatial GeoRaster");
    poDriver->SetMetadataItem(GDAL_DMD_HELPTOPIC,
                              "drivers/raster/georaster.html");
    poDriver->SetMetadataItem(GDAL_DMD_SUBDATASETS, "YES");
    poDriver->SetMetadataItem(GDAL_DMD_CREATIONDATATYPES,
                              "Byte UInt16 Int16 UInt32 Int32 Float32 "
                              "Float64 CFloat32 CFloat64");
    poDriver->SetMetadataItem(
        GDAL_DMD_CREATIONOPTIONLIST,
        "<CreationOptionList>"
        "  <Option name='DESCRIPTION' type='string' description='Table "
        "Description'/>"
        "  <Option name='INSERT'      type='string' description='Column "
        "Values'/>"
        "  <Option name='BLOCKXSIZE'  type='int'    description='Column Block "
        "Size' "
        "default='512'/>"
        "  <Option name='BLOCKYSIZE'  type='int'    description='Row Block "
        "Size' "
        "default='512'/>"
        "  <Option name='BLOCKBSIZE'  type='int'    description='Band Block "
        "Size'/>"
        "  <Option name='BLOCKING'    type='string-select' default='YES'>"
        "       <Value>YES</Value>"
        "       <Value>NO</Value>"
        "       <Value>OPTIMALPADDING</Value>"
        "  </Option>"
        "  <Option name='SRID'        type='int'    description='Overwrite "
        "EPSG code'/>"
        "  <Option name='GENPYRAMID'  type='string-select' "
        " description='Generate Pyramid, inform resampling method'>"
        "       <Value>NN</Value>"
        "       <Value>BILINEAR</Value>"
        "       <Value>BIQUADRATIC</Value>"
        "       <Value>CUBIC</Value>"
        "       <Value>AVERAGE4</Value>"
        "       <Value>AVERAGE16</Value>"
        "  </Option>"
        "  <Option name='GENPYRLEVELS'  type='int'  description='Number of "
        "pyramid level to generate'/>"
        " <Option name='GENSTATS' type='boolean' "
        "description='Generate statistics from the given rasters' "
        "default='FALSE' />"
        " <Option name='GENSTATS_SAMPLINGFACTOR' type='int' "
        "description='Number of cells skipped in both row and column "
        "dimensions when "
        "the statistics are computed' "
        "default='1' />"
        " <Option name='GENSTATS_SAMPLINGWINDOW' type='string' "
        "description='Coordinates (4 numbers) of a rectangular "
        "window to be used to sample the raster when generating statistics' />"
        " <Option name='GENSTATS_HISTOGRAM' type='boolean' "
        "description='Compute a histogram for the raster' default='FALSE' />"
        " <Option name='GENSTATS_LAYERNUMBERS' type='string' "
        "description='Layer numbers and/or ranges for which to compute "
        "the statistics' />"
        " <Option name='GENSTATS_USEBIN' type='boolean' "
        "description='Specifies if the statistics should use the bin function "
        "provided by GENSTATS_BINFUNCTION "
        "to compute the statistics' default='TRUE' />"
        " <Option name='GENSTATS_BINFUNCTION' type='string' "
        "description='Array to specify the bin function (type, total number of "
        "bins, first bin number, minimum, cell value, maximum cell value)' />"
        " <Option name='GENSTATS_NODATA' type='boolean' "
        "description='Whether or not to compare each cell values "
        "with NODATA values defined in the metadata'"
        " default='FALSE' />"
        "  <Option name='OBJECTTABLE' type='boolean' "
        "description='Create RDT as object table'/>"
        "  <Option name='SPATIALEXTENT' type='boolean' "
        "description='Generate Spatial Extent' "
        "default='TRUE'/>"
        "  <Option name='EXTENTSRID'  type='int'    description='Spatial "
        "ExtentSRID code'/>"
        "  <Option name='COORDLOCATION'    type='string-select' "
        "default='CENTER'>"
        "       <Value>CENTER</Value>"
        "       <Value>UPPERLEFT</Value>"
        "  </Option>"
        "  <Option name='VATNAME'     type='string' description='Value "
        "Attribute Table Name'/>"
        "  <Option name='NBITS'       type='int'    description='BITS for "
        "sub-byte "
        "data types (1,2,4) bits'/>"
        "  <Option name='INTERLEAVE'  type='string-select'>"
        "       <Value>BSQ</Value>"
        "       <Value>BIP</Value>"
        "       <Value>BIL</Value>"
        "   </Option>"
        "  <Option name='COMPRESS'    type='string-select'>"
        "       <Value>NONE</Value>"
        "       <Value>JPEG-F</Value>"
        "       <Value>JP2-F</Value>"
        "       <Value>DEFLATE</Value>"
        "  </Option>"
        "  <Option name='QUALITY'     type='int'    description='JPEG quality "
        "0..100' "
        "default='75'/>"
        "  <Option name='JP2_QUALITY'     type='string' description='For JP2-F "
        "compression, single quality value or comma separated list "
        "of increasing quality values for several layers, each in the 0-100 "
        "range' default='25'/>"
        "  <Option name='JP2_BLOCKXSIZE'  type='int' description='For JP2 "
        "compression, tile Width' default='1024'/>"
        "  <Option name='JP2_BLOCKYSIZE'  type='int' description='For JP2 "
        "compression, tile Height' default='1024'/>"
        "  <Option name='JP2_REVERSIBLE'  type='boolean' description='For "
        "JP2-F compression, True if the compression is reversible' "
        "default='false'/>"
        "  <Option name='JP2_RESOLUTIONS' type='int' description='For JP2-F "
        "compression, Number of resolutions.' min='1' max='30'/>"
        "  <Option name='JP2_PROGRESSION' type='string-select' "
        "description='For JP2-F compression, progression order' default='LRCP'>"
        "    <Value>LRCP</Value>"
        "    <Value>RLCP</Value>"
        "    <Value>RPCL</Value>"
        "    <Value>PCRL</Value>"
        "    <Value>CPRL</Value>"
        "  </Option>"
        "</CreationOptionList>");

    poDriver->pfnIdentify = GEORDriverIdentify;
    poDriver->SetMetadataItem(GDAL_DCAP_OPEN, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATE, "YES");
    poDriver->SetMetadataItem(GDAL_DCAP_CREATECOPY, "YES");
}

/************************************************************************/
/*                     DeclareDeferredGEORPlugin()                      */
/************************************************************************/

#ifdef PLUGIN_FILENAME
void DeclareDeferredGEORPlugin()
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
    GEORDriverSetCommonMetadata(poDriver);
    GetGDALDriverManager()->DeclareDeferredPluginDriver(poDriver);
}
#endif
