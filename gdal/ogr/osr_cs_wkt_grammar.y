%{
/******************************************************************************
 * $Id$
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  CS WKT parser grammar
 * Author:   Even Rouault, <even dot rouault at mines dash paris dot org>
 *
 ******************************************************************************
 * Copyright (c) 2013 Even Rouault, <even dot rouault at mines dash paris dot org>
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

#include "osr_cs_wkt.h"

%}

%define api.pure
/* if the next %define is commented out, Bison 2.4 should be sufficient */
/* but will produce less prettier error messages */
%define parse.error verbose
%require "3.0"

%parse-param {osr_cs_wkt_parse_context *context}
%lex-param {osr_cs_wkt_parse_context *context}

%token T_PARAM_MT               "PARAM_MT"
%token T_CONCAT_MT              "CONCAT_MT"
%token T_INVERSE_MT             "INVERSE_MT"
%token T_PASSTHROUGH_MT         "PASSTHROUGH_MT"
%token T_PROJCS                 "PROJCS"
%token T_PROJECTION             "PROJECTION"
%token T_GEOGCS                 "GEOGCS"
%token T_DATUM                  "DATUM"
%token T_SPHEROID               "SPHEROID"
%token T_PRIMEM                 "PRIMEM"
%token T_UNIT                   "UNIT"
%token T_GEOCCS                 "GEOCCS"
%token T_AUTHORITY              "AUTHORITY"
%token T_VERT_CS                "VERT_CS"
%token T_VERT_DATUM             "VERT_DATUM"
%token T_COMPD_CS               "COMPD_CS"
%token T_AXIS                   "AXIS"
%token T_TOWGS84                "TOWGS84"
%token T_FITTED_CS              "FITTED_CS"
%token T_LOCAL_CS               "LOCAL_CS"
%token T_LOCAL_DATUM            "LOCAL_DATUM"
%token T_PARAMETER              "PARAMETER"

%token T_EXTENSION              "EXTENSION"

%token T_STRING                 "string"
%token T_NUMBER                 "number"
%token T_IDENTIFIER             "identifier"

%token END 0                    "end of string"

%%

input:
    coordinate_system

/* Derived from BNF grammar in OGC 01-009 OpenGIS Implementation */
/* Coordinate Transformation Services Revision 1.00 */
/* with the following additions : */
/* - accept an EXTENSION node at the end of GEOGCS, PROJCS, COMPD_CS, VERT_DATUM */
/* - accept 3 parameters in TOWGS84 */

/* 7.1 Math Transform WKT */

begin_node:
    '['

begin_node_name:
    begin_node T_STRING

end_node:
    ']'

math_transform:
    param_mt | concat_mt | inv_mt | passthrough_mt

param_mt:
    T_PARAM_MT begin_node_name opt_parameter_list end_node

parameter:
    T_PARAMETER begin_node_name ',' T_NUMBER end_node

opt_parameter_list:
    ',' parameter
    | ',' parameter opt_parameter_list

concat_mt:
    T_CONCAT_MT begin_node math_transform opt_math_transform_list end_node

opt_math_transform_list:
  | math_transform
  | math_transform ',' opt_math_transform_list

inv_mt:
    T_INVERSE_MT begin_node math_transform end_node

passthrough_mt:
    T_PASSTHROUGH_MT begin_node integer ',' math_transform end_node

/* FIXME */
integer:
    T_NUMBER

/* 7.2 Coordinate System WKT */

coordinate_system:
    horz_cs | geocentric_cs | vert_cs | compd_cs | fitted_cs | local_cs

horz_cs:
    geographic_cs | projected_cs

/* opt_extension is an extension of the CT spec */
projected_cs:
    T_PROJCS begin_node_name ',' geographic_cs ',' projection ','
                    opt_parameter_list_linear_unit opt_twin_axis_authority_extension end_node

opt_parameter_list_linear_unit:
    linear_unit
  | parameter_list_linear_unit

parameter_list_linear_unit:
    parameter ',' parameter_list_linear_unit
  | parameter ',' linear_unit

opt_twin_axis_authority_extension:
    | ',' twin_axis opt_authority_extension
    | ',' authority opt_extension
    | ',' extension

opt_extension:
    | ',' extension

extension:
    T_EXTENSION begin_node_name ',' T_STRING end_node

projection:
    T_PROJECTION begin_node_name opt_authority end_node

opt_authority:
    | ',' authority

geographic_cs:
    T_GEOGCS begin_node_name',' datum ',' prime_meridian ','
                    angular_unit opt_twin_axis_authority_extension end_node

datum:
    T_DATUM begin_node_name ',' spheroid opt_towgs84_authority_extension end_node

opt_towgs84_authority_extension:
    | ',' towgs84 opt_authority_extension
    | ',' authority opt_extension
    | ',' extension

spheroid:
    T_SPHEROID begin_node_name ',' semi_major_axis ','
                        inverse_flattening opt_authority end_node

semi_major_axis:
    T_NUMBER

inverse_flattening:
    T_NUMBER

prime_meridian:
    T_PRIMEM begin_node_name ',' longitude opt_authority end_node

longitude:
    T_NUMBER

angular_unit:
    unit

linear_unit:
    unit

unit:
    T_UNIT begin_node_name ',' conversion_factor opt_authority end_node

conversion_factor:
    T_NUMBER

geocentric_cs:
    T_GEOCCS begin_node_name ',' datum ',' prime_meridian ','
                        linear_unit opt_three_axis_authority end_node

opt_three_axis_authority:
    | ',' three_axis opt_authority
    | ',' authority

three_axis:
    axis ',' axis ',' axis

authority:
    T_AUTHORITY begin_node_name ',' T_STRING end_node

vert_cs:
    T_VERT_CS begin_node_name ',' vert_datum ',' linear_unit opt_axis_authority end_node

opt_axis_authority:
    | ',' axis opt_authority
    | ',' authority

vert_datum:
    T_VERT_DATUM begin_node_name ',' datum_type opt_authority_extension end_node

opt_authority_extension:
    | ',' authority opt_extension
    | ',' extension

datum_type:
    T_NUMBER

compd_cs:
    T_COMPD_CS begin_node_name ',' head_cs ',' tail_cs opt_authority_extension end_node

head_cs:
    coordinate_system

tail_cs:
    coordinate_system

twin_axis: axis ',' axis

axis:
    T_AXIS begin_node_name ',' T_IDENTIFIER end_node
/* Extension of the CT spec */
/*    | T_AXIS '[' T_STRING ',' T_STRING ']'*/

towgs84:
    T_TOWGS84 begin_node towgs84_parameters end_node

towgs84_parameters:
    seven_parameters
/* Extension of the CT spec */
  | three_parameters

three_parameters:
    dx ',' dy ',' dz

seven_parameters:
    dx ',' dy ',' dz ',' ex ',' ey ',' ez ',' ppm

dx:
    T_NUMBER

dy:
    T_NUMBER

dz:
    T_NUMBER

ex:
    T_NUMBER

ey:
    T_NUMBER

ez:
    T_NUMBER

ppm:
    T_NUMBER

fitted_cs:
    T_FITTED_CS begin_node_name ',' to_base ',' base_cs end_node

to_base:
    math_transform

base_cs:
    coordinate_system

local_cs:
    T_LOCAL_CS begin_node_name ',' local_datum ',' unit ',' axis opt_axis_list_authority end_node

opt_axis_list_authority:
    | ',' authority
    | ',' axis opt_axis_list_authority

local_datum:
    T_LOCAL_DATUM begin_node_name ',' datum_type opt_authority end_node
