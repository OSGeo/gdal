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

#ifndef OGR_GMLAS_CONSTS_INCLUDED_REDEFINABLE
#define OGR_GMLAS_CONSTS_INCLUDED_REDEFINABLE

#ifdef CONSTANT_DEFINITION
#define STRING_CONST(x,y)    const char* const x = y
#define BOOL_CONST(x,y)      const bool x = y
#define INT_CONST(x,y)       const int x = y
#else
#define STRING_CONST(x,y)    extern const char* const x
#define BOOL_CONST(x,y)      extern const bool x
#define INT_CONST(x,y)       extern const int x
#endif

namespace GMLASConstants
{
    // Note the default values mentioned here should be kept
    // consistent with what is documented in gmlasconf.xsd
    BOOL_CONST(DEFAULT_RESOLUTION_ENABLED_DEFAULT, false);
    BOOL_CONST(ALLOW_REMOTE_DOWNLOAD_DEFAULT, true);
    BOOL_CONST(CACHE_RESULTS_DEFAULT, false);
    BOOL_CONST(INTERNAL_XLINK_RESOLUTION_DEFAULT, false);

    BOOL_CONST(ALLOW_REMOTE_SCHEMA_DOWNLOAD_DEFAULT, true);
    BOOL_CONST(ALWAYS_GENERATE_OGR_ID_DEFAULT, false);
    BOOL_CONST(REMOVE_UNUSED_LAYERS_DEFAULT, false);
    BOOL_CONST(REMOVE_UNUSED_FIELDS_DEFAULT, false);
    BOOL_CONST(USE_ARRAYS_DEFAULT, true);
    BOOL_CONST(USE_NULL_STATE_DEFAULT, false);
    BOOL_CONST(INCLUDE_GEOMETRY_XML_DEFAULT, false);
    BOOL_CONST(INSTANTIATE_GML_FEATURES_ONLY_DEFAULT, true);
    BOOL_CONST(ALLOW_XSD_CACHE_DEFAULT, true);
    BOOL_CONST(SCHEMA_FULL_CHECKING_DEFAULT, true);
    BOOL_CONST(HANDLE_MULTIPLE_IMPORTS_DEFAULT, false);
    BOOL_CONST(VALIDATE_DEFAULT, false);
    BOOL_CONST(FAIL_IF_VALIDATION_ERROR_DEFAULT, false);
    BOOL_CONST(EXPOSE_METADATA_LAYERS_DEFAULT, false);
    BOOL_CONST(SWE_PROCESS_DATA_RECORD_DEFAULT, true);
    BOOL_CONST(SWE_PROCESS_DATA_ARRAY_DEFAULT, true);
    BOOL_CONST(WARN_IF_EXCLUDED_XPATH_FOUND_DEFAULT, true);
    BOOL_CONST(CASE_INSENSITIVE_IDENTIFIER_DEFAULT, true);
    BOOL_CONST(PG_IDENTIFIER_LAUNDERING_DEFAULT, true);
    INT_CONST(MAXIMUM_FIELDS_FLATTENING_DEFAULT, 10);
    INT_CONST(MIN_VALUE_OF_MAX_IDENTIFIER_LENGTH, 10);
    INT_CONST(INDENT_SIZE_DEFAULT, 2);
    INT_CONST(INDENT_SIZE_MIN, 0);
    INT_CONST(INDENT_SIZE_MAX, 8);
    INT_CONST(MAX_FILE_SIZE_DEFAULT, 1024 * 1024);


    INT_CONST(IDX_COMPOUND_FOLDED, -2);

// Pseudo index to indicate that this xpath is a part of a more detailed
// xpath that is folded into the main type, hence we shouldn't warn about it
// to be unexpected
// Would for example be the case of "element_compound_simplifiable" for :
//             <xs:element name="element_compound_simplifiable">
//                <xs:complexType><xs:sequence>
//                        <xs:element name="subelement" type="xs:string"/>
//                </xs:sequence></xs:complexType>
//            </xs:element>

    INT_CONST(MAXOCCURS_UNLIMITED, -2);

    STRING_CONST(szXML_URI, "http://www.w3.org/XML/1998/namespace");
    STRING_CONST(szXS_URI, "http://www.w3.org/2001/XMLSchema");

    STRING_CONST(szXSI_URI, "http://www.w3.org/2001/XMLSchema-instance");
    STRING_CONST(szXSI_PREFIX, "xsi");
    STRING_CONST(szNIL, "nil");
    STRING_CONST(szAT_XSI_NIL, "@xsi:nil");

    STRING_CONST(szXMLNS_URI, "http://www.w3.org/2000/xmlns/");
    STRING_CONST(szXMLNS_PREFIX, "xmlns");
    STRING_CONST(szSCHEMA_LOCATION, "schemaLocation");
    STRING_CONST(szNO_NAMESPACE_SCHEMA_LOCATION,
                                                "noNamespaceSchemaLocation");

    STRING_CONST(szXLINK_URI, "http://www.w3.org/1999/xlink");
    STRING_CONST(szXLINK_PREFIX, "xlink");
    STRING_CONST(szTYPE, "type");
    STRING_CONST(szHREF, "href");
    STRING_CONST(szOWNS, "owns");

    STRING_CONST(szSWE_URI, "http://www.opengis.net/swe/2.0");

    STRING_CONST(szOPENGIS_URL, "http://www.opengis.net/");

    STRING_CONST(szGML_URI, "http://www.opengis.net/gml");
    STRING_CONST(szGML32_URI, "http://www.opengis.net/gml/3.2");
    STRING_CONST(szGML_PREFIX, "gml");
    STRING_CONST(szSRS_NAME, "srsName");

    STRING_CONST(szWFS_URI, "http://www.opengis.net/wfs");
    STRING_CONST(szWFS20_URI, "http://www.opengis.net/wfs/2.0");
    STRING_CONST(szWFS_PREFIX, "wfs");
    STRING_CONST(szWFS20_SCHEMALOCATION,
                                "http://schemas.opengis.net/wfs/2.0/wfs.xsd");
    STRING_CONST(szMEMBER, "member");

    STRING_CONST(szOGRGMLAS_URI, "http://gdal.org/ogr/gmlas");
    STRING_CONST(szOGRGMLAS_PREFIX, "ogr_gmlas");

    STRING_CONST(szOGR_FIELDS_METADATA, "_ogr_fields_metadata");
    STRING_CONST(szOGR_LAYERS_METADATA, "_ogr_layers_metadata");
    STRING_CONST(szOGR_LAYER_RELATIONSHIPS, "_ogr_layer_relationships");
    STRING_CONST(szOGR_OTHER_METADATA, "_ogr_other_metadata");

// Fields of szOGR_FIELDS_METADATA
    STRING_CONST(szLAYER_NAME, "layer_name");
    STRING_CONST(szFIELD_INDEX, "field_index");
    STRING_CONST(szFIELD_NAME, "field_name");
    STRING_CONST(szFIELD_XPATH, "field_xpath");
    STRING_CONST(szFIELD_TYPE, "field_type");
    STRING_CONST(szFIELD_IS_LIST, "field_is_list");
    STRING_CONST(szFIELD_MIN_OCCURS, "field_min_occurs");
    STRING_CONST(szFIELD_MAX_OCCURS, "field_max_occurs");
    STRING_CONST(szFIELD_REPETITION_ON_SEQUENCE, "field_repetition_on_sequence");
    STRING_CONST(szFIELD_DEFAULT_VALUE, "field_default_value");
    STRING_CONST(szFIELD_FIXED_VALUE, "field_fixed_value");
    STRING_CONST(szFIELD_CATEGORY, "field_category");
    STRING_CONST(szFIELD_RELATED_LAYER, "field_related_layer");
    STRING_CONST(szFIELD_JUNCTION_LAYER, "field_junction_layer");
    STRING_CONST(szFIELD_DOCUMENTATION, "field_documentation");

// Fields of szOGR_LAYERS_METADATA
// szLAYER_NAME
    STRING_CONST(szLAYER_XPATH, "layer_xpath");
    STRING_CONST(szLAYER_CATEGORY, "layer_category");
    STRING_CONST(szLAYER_PKID_NAME, "layer_pkid_name");
    STRING_CONST(szLAYER_PARENT_PKID_NAME, "layer_parent_pkid_name");
    STRING_CONST(szLAYER_DOCUMENTATION, "layer_documentation");

// Fields of szOGR_LAYER_RELATIONSHIPS
    STRING_CONST(szPARENT_LAYER, "parent_layer");
    STRING_CONST(szPARENT_PKID, "parent_pkid");
    STRING_CONST(szPARENT_ELEMENT_NAME, "parent_element_name");
    STRING_CONST(szCHILD_LAYER, "child_layer");
    STRING_CONST(szCHILD_PKID, "child_pkid");

// Fields of szOGR_OTHER_METADATA
    STRING_CONST(szKEY, "key");
    STRING_CONST(szVALUE, "value");

    STRING_CONST(szCONFIGURATION_FILENAME, "configuration_filename");
    STRING_CONST(szCONFIGURATION_INLINED, "configuration_inlined");
    STRING_CONST(szDOCUMENT_FILENAME, "document_filename");

    STRING_CONST(szNAMESPACE_URI_FMT, "namespace_uri_%d");
    STRING_CONST(szNAMESPACE_LOCATION_FMT, "namespace_location_%d");
    STRING_CONST(szNAMESPACE_PREFIX_FMT, "namespace_prefix_%d");
    STRING_CONST(szGML_VERSION, "gml_version");
    STRING_CONST(szSCHEMA_NAME_FMT, "schema_name_%d");

// Fields of a junction table
    STRING_CONST(szOCCURRENCE, "occurrence");
// szPARENT_PKID
// szCHILD_PKID

    STRING_CONST(szOGR_PKID, "ogr_pkid");
    STRING_CONST(szPKID_SUFFIX, "_pkid");
    STRING_CONST(szPARENT_PREFIX, "parent_");
    STRING_CONST(szXML_SUFFIX, "_xml");
    STRING_CONST(szRAW_CONTENT_SUFFIX, "_rawcontent");
    STRING_CONST(szAT_XLINK_HREF, "@xlink:href");
    STRING_CONST(szHREF_SUFFIX, "_href");

// Values of layer_category
    STRING_CONST(szTOP_LEVEL_ELEMENT, "TOP_LEVEL_ELEMENT");
    STRING_CONST(szNESTED_ELEMENT, "NESTED_ELEMENT");
    STRING_CONST(szJUNCTION_TABLE, "JUNCTION_TABLE");
    STRING_CONST(szSWE_DATA_ARRAY, "SWE_DATA_ARRAY");

// Values of field_category
    STRING_CONST(szREGULAR, "REGULAR");
    STRING_CONST(szPATH_TO_CHILD_ELEMENT_NO_LINK,
                                    "PATH_TO_CHILD_ELEMENT_NO_LINK");
    STRING_CONST(szPATH_TO_CHILD_ELEMENT_WITH_LINK,
                                    "PATH_TO_CHILD_ELEMENT_WITH_LINK");
    STRING_CONST(szPATH_TO_CHILD_ELEMENT_WITH_JUNCTION_TABLE,
                                    "PATH_TO_CHILD_ELEMENT_WITH_JUNCTION_TABLE");
    STRING_CONST(szGROUP, "GROUP");
    STRING_CONST(szSWE_FIELD, "SWE_FIELD");

    STRING_CONST(szGMLAS_PREFIX, "GMLAS:");
    STRING_CONST(szDEFAULT_CONF_FILENAME, "gmlasconf.xml");

// Open options
    STRING_CONST(szCONFIG_FILE_OPTION, "CONFIG_FILE");
    STRING_CONST(szXSD_OPTION, "XSD");
    STRING_CONST(szREFRESH_CACHE_OPTION, "REFRESH_CACHE");
    STRING_CONST(szEXPOSE_METADATA_LAYERS_OPTION,
                                                "EXPOSE_METADATA_LAYERS");
    STRING_CONST(szSWAP_COORDINATES_OPTION, "SWAP_COORDINATES");
    STRING_CONST(szVALIDATE_OPTION, "VALIDATE");
    STRING_CONST(szREMOVE_UNUSED_LAYERS_OPTION, "REMOVE_UNUSED_LAYERS");
    STRING_CONST(szREMOVE_UNUSED_FIELDS_OPTION, "REMOVE_UNUSED_FIELDS");
    STRING_CONST(szFAIL_IF_VALIDATION_ERROR_OPTION,
                                                "FAIL_IF_VALIDATION_ERROR");
    STRING_CONST(szKEEP_RELATIVE_PATHS_FOR_METADATA_OPTION,
                                        "KEEP_RELATIVE_PATHS_FOR_METADATA");
    STRING_CONST(szEXPOSE_CONFIGURATION_IN_METADATA_OPTION,
                                        "EXPOSE_CONFIGURATION_IN_METADATA");
    STRING_CONST(szEXPOSE_SCHEMAS_NAME_IN_METADATA_OPTION,
                                        "EXPOSE_SCHEMAS_NAME_IN_METADATA");
    STRING_CONST(szSCHEMA_FULL_CHECKING_OPTION, "SCHEMA_FULL_CHECKING");
    STRING_CONST(szHANDLE_MULTIPLE_IMPORTS_OPTION,
                                            "HANDLE_MULTIPLE_IMPORTS");

// Creation options
    STRING_CONST(szINPUT_XSD_OPTION, "INPUT_XSD");
    STRING_CONST(szLAYERS_OPTION, "LAYERS");
    STRING_CONST(szSRSNAME_FORMAT_OPTION, "SRSNAME_FORMAT");
    STRING_CONST(szINDENT_SIZE_OPTION, "INDENT_SIZE");
    STRING_CONST(szCOMMENT_OPTION, "COMMENT");
    STRING_CONST(szLINEFORMAT_OPTION, "LINEFORMAT");
    STRING_CONST(szWRAPPING_OPTION, "WRAPPING");
    STRING_CONST(szTIMESTAMP_OPTION, "TIMESTAMP");
    STRING_CONST(szWFS20_SCHEMALOCATION_OPTION, "WFS20_SCHEMALOCATION");
    STRING_CONST(szGENERATE_XSD_OPTION, "GENERATE_XSD");
    STRING_CONST(szOUTPUT_XSD_FILENAME_OPTION, "OUTPUT_XSD_FILENAME");

// Values for SRSNAME_FORMAT option
    STRING_CONST(szSHORT, "SHORT");
    STRING_CONST(szOGC_URN, "OGC_URN");
    STRING_CONST(szOGC_URL, "OGC_URL");
    STRING_CONST(szSRSNAME_DEFAULT, "OGC_URL");

// Values for LINEFORMAT option
    STRING_CONST(szCRLF, "CRLF");
    STRING_CONST(szLF, "LF");

// Value for WRAPPING option
    STRING_CONST(szWFS2_FEATURECOLLECTION, "WFS2_FEATURECOLLECTION");
    STRING_CONST(szGMLAS_FEATURECOLLECTION, "GMLAS_FEATURECOLLECTION");

    STRING_CONST(szFEATURE_COLLECTION, "FeatureCollection");
    STRING_CONST(szFEATURE_MEMBER, "featureMember");

// XML types
    STRING_CONST(szXS_STRING, "string");
    STRING_CONST(szXS_TOKEN, "token");
    STRING_CONST(szXS_NMTOKEN, "NMTOKEN");
    STRING_CONST(szXS_NCNAME, "NCName");
    STRING_CONST(szXS_QNAME, "QName");
    STRING_CONST(szXS_ID, "ID");
    STRING_CONST(szXS_IDREF, "IDREF");
    STRING_CONST(szXS_BOOLEAN, "boolean");
    STRING_CONST(szXS_BYTE, "byte");
    STRING_CONST(szXS_SHORT, "short");
    STRING_CONST(szXS_INT, "int");
    STRING_CONST(szXS_LONG, "long");
    STRING_CONST(szXS_INTEGER, "integer");
    STRING_CONST(szXS_NEGATIVE_INTEGER, "negativeInteger");
    STRING_CONST(szXS_NON_NEGATIVE_INTEGER, "nonNegativeInteger");
    STRING_CONST(szXS_NON_POSITIVE_INTEGER, "nonPositiveInteger");
    STRING_CONST(szXS_POSITIVE_INTEGER, "positiveInteger");
    STRING_CONST(szXS_UNSIGNED_BYTE, "unsignedByte");
    STRING_CONST(szXS_UNSIGNED_SHORT, "unsignedShort");
    STRING_CONST(szXS_UNSIGNED_INT, "unsignedInt");
    STRING_CONST(szXS_UNSIGNED_LONG, "unsignedLong");
    STRING_CONST(szXS_FLOAT, "float");
    STRING_CONST(szXS_DOUBLE, "double");
    STRING_CONST(szXS_DECIMAL, "decimal");
    STRING_CONST(szXS_DATE, "date");
    STRING_CONST(szXS_GYEAR, "gYear");
    STRING_CONST(szXS_GYEAR_MONTH, "gYearMonth");
    STRING_CONST(szXS_TIME, "time");
    STRING_CONST(szXS_DATETIME, "dateTime");
    STRING_CONST(szXS_ANY_URI, "anyURI");
    STRING_CONST(szXS_ANY_TYPE, "anyType");
    STRING_CONST(szXS_ANY_SIMPLE_TYPE, "anySimpleType");
    STRING_CONST(szXS_DURATION, "duration");
    STRING_CONST(szXS_BASE64BINARY, "base64Binary");
    STRING_CONST(szXS_HEXBINARY, "hexBinary");
// Extensions to XML types
    STRING_CONST(szFAKEXS_GEOMETRY, "geometry");
    STRING_CONST(szFAKEXS_JSON_DICT, "json_dict");

    STRING_CONST(szAT_ANY_ATTR, "@*");
    STRING_CONST(szMATCH_ALL, "/*");
    STRING_CONST(szEXTRA_SUFFIX, ";extra=");
}

#undef STRING_CONST
#undef INT_CONST
#undef BOOL_CONST

using namespace GMLASConstants;

#endif // OGR_GMLAS_CONSTS_INCLUDED_REDEFINABLE
