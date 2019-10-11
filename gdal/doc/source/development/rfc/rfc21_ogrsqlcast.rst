.. _rfc-21:

================================================================================
RFC 21: OGR SQL type cast and field name alias
================================================================================

Author: Tamas Szekeres

Contact: szekerest@gmail.com

Status: Adopted

Summary
-------

This proposal provides support for changing the column name and the
column type in the OGR SQL select list.

The main motivation of this change is to provide better control when
transferring the OGR_STYLE special field from each data source to the
other. For example we can use ogr2ogr for this purpose using the
following command line:

::

   ogr2ogr -f "ESRI Shapefile" -sql "select *, OGR_STYLE from rivers" rivers.shp rivers.tab

The shape driver will truncate the OGR_STYLE field to 80 characters by
default in length that may not be enough to hold the actual value. So as
to fix this issue we might want to specify the desired length in the
select list, like:

::

   ogr2ogr -f "ESRI Shapefile" -sql "select *, CAST(OGR_STYLE AS character(255)) from rivers" rivers.shp rivers.tab

In some cases it would also be useful to change the name of the field in
the target data source:

::

   ogr2ogr -f "ESRI Shapefile" -sql "select *, CAST(OGR_STYLE AS character(255)) AS 'STYLE' from rivers" rivers.shp rivers.tab

Main concepts
-------------

To support these new features we will extend the current OGR SQL syntax.
The proposed addition will keep the syntax conform to the SQL92
specification:

::

   SELECT <field-list> FROM <table_def>
        [LEFT JOIN <table_def> 
         ON [<table_ref>.]<key_field> = [<table_ref>.].<key_field>]*
        [WHERE <where-expr>] 
        [ORDER BY <sort specification list>]

   <field-list> ::= <column-spec> [ { , <column-spec> }... ]

   <column-spec> ::= <field-spec> [ <as clause> ]
                    | CAST ( <field-spec> AS <data type> ) [ <as clause> ]

   <field-spec> ::= [DISTINCT] <field_ref>
                    | <field_func> ( [DISTINCT] <field-ref> )
                    | Count(*)

   <as clause> ::= [ AS ] <column_name>

   <data type> ::= character [ ( field_length ) ]
                   | float [ ( field_length ) ]
                   | numeric [ ( field_length [, field_precision ] ) ]
                   | integer [ ( field_length ) ]
                   | date [ ( field_length ) ]
                   | time [ ( field_length ) ]
                   | timestamp [ ( field_length ) ]

   <field-func> ::= AVG | MAX | MIN | SUM | COUNT

   <field_ref>  ::= [<table_ref>.]field_name

   <sort specification list> ::=
                 <sort specification> [ { <comma> <sort specification> }... ]

   <sort specification> ::= <sort key> [ <ordering specification> ]

   <sort key> ::=  <field_ref>

   <ordering specification> ::= ASC | DESC

   <table_def> ::= ['<datasource name>'.]table_name [table_alias]

   <table_ref> ::= table_name | table_alias

This RFC doesn't address implementing conversion to the 'integer list',
'double list' and 'string list' OGR data types, which doesn't conform to
the SQL92 specification and the necessary conversion routines are
missing in the OGR code.

Implementation
--------------

To implement the addition the following changes should be made in the
OGR codebase:

1. In ogr_swq.h I'll have to add to 4 fields to swq_col_def to hold the
   field_alias the target_type, the field_length, and field_precision

2. In ogr_swq.h SWQ_DATE, SWQ_TIME, SWQ_TIMESTAMP will be added to
   swq_field_type enum.

3. In swq.c I'll have to change swq_select_preparse to take care of the
   field alias and the CAST specification.

4. A new function (swq_parse_typename) will be added to parse the
   supported typenames.

5. In ogr_gensql.cpp the .ctor of OGRGenSQLResultsLayer will be changed
   to set up the field name and the field length to the target data
   source

6. In ogr_gensql.cpp TranslateFeature will be modified to take care of
   the type change if specified.

Backward Compatibility
----------------------

The backward compatibility for the current SQL syntax will be retained.
This addition doesn't require changes in the OGR C and SWIG API.

Documentation
-------------

The OGR SQL document will be updated to reflect the new features. I'll
take care of updating the OGR Style Documentation with the support of
transferring the styles between the data sources.

Implementation Staffing
-----------------------

Tamas Szekeres will implement the RFC in the development version.

Frank Warmerdam will implement the regression test scripts according to
this new functionality.

References
----------

-  Tracking bug for this feature (containing all of the proposed code
   changes): #2171

Voting History
--------------

Frank Warmerdam +1

Daniel Morissette +1

Howard Butler +1

Even Rouault +1

Tamas Szekeres +1

Andrey Kiselev +0
