/******************************************************************************
 * Project:  OGR
 * Purpose:  OGRGMLASDriver implementation
 * Author:   Even Rouault, <even dot rouault at spatialys dot com>
 *
 * Initial development funded by the European Earth observation programme
 * Copernicus
 *
 ******************************************************************************
 * Copyright (c) 2016, Even Rouault, <even dot rouault at spatialys dot com>
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

// Must be first for DEBUG_BOOL case
#include "ogr_gmlas.h"

// g++ -I/usr/include/json -DxDEBUG_VERBOSE -DDEBUG   -g -DDEBUG -ftrapv  -Wall -Wextra -Winit-self -Wunused-parameter -Wformat -Werror=format-security -Wno-format-nonliteral -Wlogical-op -Wshadow -Werror=vla -Wmissing-declarations -Wnon-virtual-dtor -Woverloaded-virtual -fno-operator-names ogr/ogrsf_frmts/gmlas/*.cpp -fPIC -shared -o ogr_GMLAS.so -Iport -Igcore -Iogr -Iogr/ogrsf_frmts -Iogr/ogrsf_frmts/mem  -L. -lgdal   -I/home/even/spatialys/eea/inspire_gml/install-xerces-c-3.1.3/include

CPL_CVSID("$Id$")

/************************************************************************/
/*                        OGRGMLASDriverIdentify()                      */
/************************************************************************/

static int OGRGMLASDriverIdentify( GDALOpenInfo* poOpenInfo )

{
    return STARTS_WITH_CI(poOpenInfo->pszFilename, szGMLAS_PREFIX);
}

/************************************************************************/
/*                           OGRGMLASDriverOpen()                       */
/************************************************************************/

static GDALDataset *OGRGMLASDriverOpen( GDALOpenInfo* poOpenInfo )

{
    OGRGMLASDataSource    *poDS;

    if( poOpenInfo->eAccess == GA_Update )
        return NULL;

    if( OGRGMLASDriverIdentify( poOpenInfo ) == FALSE )
        return NULL;

    poDS = new OGRGMLASDataSource();

    if( !poDS->Open(  poOpenInfo ) )
    {
        delete poDS;
        return NULL;
    }
    else
        return poDS;
}

/************************************************************************/
/*                          RegisterOGRGMLAS()                          */
/************************************************************************/

void RegisterOGRGMLAS()

{
    if( GDALGetDriverByName( "GMLAS" ) != NULL )
        return;

    GDALDriver  *poDriver = new GDALDriver();

    poDriver->SetDescription( "GMLAS" );
    poDriver->SetMetadataItem( GDAL_DCAP_VECTOR, "YES" );
    poDriver->SetMetadataItem( GDAL_DMD_LONGNAME,
                               "Geography Markup Language (GML) "
                               "driven by application schemas" );
    poDriver->SetMetadataItem( GDAL_DMD_EXTENSIONS, "gml xml" );
    poDriver->SetMetadataItem( GDAL_DMD_HELPTOPIC, "drv_gmlas.html" );

    poDriver->SetMetadataItem( GDAL_DMD_CONNECTION_PREFIX, szGMLAS_PREFIX );

    poDriver->SetMetadataItem( GDAL_DMD_OPENOPTIONLIST,
"<OpenOptionList>"
"  <Option name='XSD' type='string' description='Space separated list of "
            "filenames of XML schemas that apply to the data file'/>"
"  <Option name='CONFIG_FILE' type='string' "
            "description='Filename of the configuration file'/>"
"  <Option name='EXPOSE_METADATA_LAYERS' type='boolean' "
        "description='Whether metadata layers should be reported by default.' "
        "default='NO'/>"
"  <Option name='SCHEMA_FULL_CHECKING' type='boolean' description="
        "'Whether the full schema constraint checking should be enabled.' "
        "default='YES'/>"
"  <Option name='HANDLE_MULTIPLE_IMPORTS' type='boolean' description='Whether "
        "multiple imports of the same namespace can be done.' default='NO'/>"
"  <Option name='VALIDATE' type='boolean' description='Whether validation "
        "against the schema should be done' default='NO'/>"
"  <Option name='FAIL_IF_VALIDATION_ERROR' type='boolean' "
    "description='Whether a validation error should cause dataset opening "
    "to fail' "
    "default='NO'/>"
"  <Option name='REFRESH_CACHE' type='boolean' "
    "description='Whether remote schemas and resolved xlink resources should "
    "be downloaded from the server' "
    "default='NO'/>"
"  <Option name='SWAP_COORDINATES' type='string-select' "
    "description='Whether the order of geometry coordinates should be "
    "inverted.' "
    "default='AUTO'>"
"    <Value>AUTO</Value>"
"    <Value>YES</Value>"
"    <Value>NO</Value>"
"  </Option>"
"  <Option name='REMOVE_UNUSED_LAYERS' type='boolean' "
    "description='Whether unused layers should be removed' " "default='NO'/>"
"  <Option name='REMOVE_UNUSED_FIELDS' type='boolean' "
    "description='Whether unused fields should be removed' " "default='NO'/>"
"</OpenOptionList>" );

    poDriver->SetMetadataItem( GDAL_DMD_CREATIONOPTIONLIST,
(CPLString("<CreationOptionList>") +
"  <Option name='" + szINPUT_XSD_OPTION + "' type='string' description='"
"Space separated list of filenames of XML schemas that apply to the data file'/>"
"  <Option name='" + szCONFIG_FILE_OPTION + "' type='string' "
            "description='Filename of the configuration file'/>"
            "'Space separated list of filenames of XML schemas that apply'/>"
"  <Option name='" +szLAYERS_OPTION + "' type='string' "
            "description='Comma separated list of layer names to export'/>"
"  <Option name='" + szSRSNAME_FORMAT_OPTION + "' type='string-select' "
                    "description='Format of srsName' "
                    "default='" + szSRSNAME_DEFAULT +"'>"
"    <Value>" + szSHORT + "</Value>"
"    <Value>" + szOGC_URN + "</Value>"
"    <Value>" + szOGC_URL + "</Value>"
"  </Option>"
"  <Option name='" +szINDENT_SIZE_OPTION + "' type='int' min='0' max='8' "
    "description='Number of spaces for each indentation level' default='2'/>"
"  <Option name='" +szCOMMENT_OPTION  +"' type='string' description='"
        "Comment to add at top of generated XML file'/>"
"  <Option name='"+ szLINEFORMAT_OPTION + "' type='string-select' "
                            "description='end-of-line sequence' "
#ifdef WIN32
                            "default='" + szCRLF + "'>"
#else
                            "default='" +szLF + "'>"
#endif
"    <Value>" +szCRLF + "</Value>"
"    <Value>" +szLF + "</Value>"
"  </Option>"
"  <Option name='" + szWRAPPING_OPTION + "' type='string-select' "
          "description='How to wrap features' "
          "default='" +szWFS2_FEATURECOLLECTION + "'>"
"    <Value>" + szWFS2_FEATURECOLLECTION + "</Value>"
"    <Value>" + szGMLAS_FEATURECOLLECTION + "</Value>"
"  </Option>"
"  <Option name='" + szTIMESTAMP_OPTION +"' type='string' "
    "description='User-specified XML "
    "dateTime value for timestamp to use in wfs:FeatureCollection attribute."
    "Only valid for " +szWRAPPING_OPTION + "=" + szWFS2_FEATURECOLLECTION +"'/>"
"  <Option name='" + szWFS20_SCHEMALOCATION_OPTION + "' type='string' "
    "description='Path or URL to wfs.xsd. Only valid for " +
    szWRAPPING_OPTION + "=" + szWFS2_FEATURECOLLECTION +"'/>"
"  <Option name='" + szGENERATE_XSD_OPTION + "' type='boolean' "
          "description='Whether to generate a .xsd file. Only valid for "
          + szWRAPPING_OPTION + "=" + szGMLAS_FEATURECOLLECTION +"' "
          "default='YES'/>"
"  <Option name='" + szOUTPUT_XSD_FILENAME_OPTION + "' type='string' "
          "description='Wrapping .xsd filename. If not specified, same "
          "basename as output file with .xsd extension. Only valid for "
          + szWRAPPING_OPTION + "=" + szGMLAS_FEATURECOLLECTION +"'/>"
"</CreationOptionList>").c_str() );

    poDriver->SetMetadataItem( GDAL_DCAP_VIRTUALIO, "YES" );

    poDriver->pfnOpen = OGRGMLASDriverOpen;
    poDriver->pfnCreateCopy = OGRGMLASDriverCreateCopy;
    poDriver->pfnIdentify = OGRGMLASDriverIdentify;

    GetGDALDriverManager()->RegisterDriver( poDriver );
}
