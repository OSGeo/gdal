.. _rfc-28:

================================================================================
RFC 28: OGR SQL Generalized Expressions
================================================================================

Author: Frank Warmerdam

Contact: warmerdam@pobox.com

Status: Adopted, Implemented

Summary
-------

The OGR SQL evaluation engine currently does not allow general purpose
functions to be applied to columns in SELECT statements. Some special
purpose functions are supported (ie. CAST, COUNT, AVG, MAX, MIN, and
SUM), but not as part of more general expressions and generally in very
constrained arrangements. It is the intent of this work item to extend
the OGR SQL engine to support fairly general purpose expression
evaluation in the output field list of OGR SQL SELECT statements and to
implement a few preliminary processing functions in a fashion compatible
with standard SQL. As well, expressions used in WHERE clauses will be
generalized to support evaluation of non-logical operations, such as
math and functions. For example, after implementation it is intended the
following could be evaluated.

::

   SELECT CONCAT(first_name, ' ', last_name) AS full_name FROM customers
   SELECT id, "Regional Road" AS roadtypename FROM roads where roadtype=3
   SELECT (subtotal+salestax) as totalcost from invoice_info where 100 <= (subtotal+salestax)

A prototype implementation is now available for review in
`http://svn.osgeo.org/gdal/sandbox/warmerdam/gdal-rfc28 <http://svn.osgeo.org/gdal/sandbox/warmerdam/gdal-rfc28>`__

Technical Approach
------------------

Currently logical expressions take a very constrained format with the
base elements having to be of the form * <constant_value>*. As part of
the generalization non-logical expressions will be supported and the
left and right side of operators will be equally treated. The current
OGR SQL parser is ad hoc and cannot be practically extended to this
generalized form of expression. So at this point we will move to a
yacc/bison based parser grammar for expressions.

Since it is not really practical to continue to use the existing ad hoc
SELECT parsing when parts of the SELECT statement are expressions, the
yacc/bison based parser will also be used to parse the whole SELECT
statement.

The current expression node will be generalized to have 0-n children
(for arguments to functions), and to treat field references and constant
values as distinct leaf nodes rather than embedding this information in
a node defining an operation.

It should be noted that as a side effect WHERE clauses will also support
more general expressions - not just logical comparisons. For instance:

SELECT \* WHERE (subtotal+salestax) > 100.0

New Functions
-------------

-  Math: +, -, \*, /, \*\*
-  String: CONCAT, SUBSTR

SELECT Rules
------------

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
                    | <cumm-field-func> ( [DISTINCT] <field-ref> )
                    | <field-expr>
                    | Count(*)

   <field-expr> ::= <field_ref>
                    | <constant-value>
                    | <field-expr> <field-operator> <field-expr>
                    | <field-func> ( <field-expr-list> )
                    | ( <field-expr> )

   <field-expr-list> ::= field-expr
                    |  field-expr , field-expr-list
                    |  <empty>

   <as clause> ::= [ AS ] <column_name>

   <data type> ::= character [ ( field_length ) ]
                   | float [ ( field_length ) ]
                   | numeric [ ( field_length [, field_precision ] ) ]
                   | integer [ ( field_length ) ]
                   | date [ ( field_length ) ]
                   | time [ ( field_length ) ]
                   | timestamp [ ( field_length ) ]

   <cumm-field-func> ::= AVG | MAX | MIN | SUM | COUNT

   <field-operator> ::= '+' | '-' | '/' | '*' | '||'

   <field-func> ::= CONCAT | SUBSTR

   <field_ref>  ::= [<table_ref>.]field_name

Special Notes
-------------

The existing CAST, and column summary functions COUNT, AVG, MIN, MAX and
SUM will be treated more-or-less as functions but constrained to be root
operations on column definitions and treated as a special case (still).

Compatibility Implications
--------------------------

Some identifiers that were previously allowed as unquoted field names
will likely now have to be quoted as they will be keywords in the
grammar. The keyword set is:

-  IN
-  LIKE
-  NULL
-  IS
-  SELECT
-  LEFT
-  JOIN
-  WHERE
-  ON
-  ORDER
-  BY
-  FROM
-  AS
-  ASC
-  DESC
-  DISTINCT
-  CAST

The previous implementation was written in C and avoided all use of
GDAL/OGR services so that it could be easily used in other contexts,
including as the where clause evaluator of the OGDI library. After this
update the code is C++, and direct use of CPL error and other services
has been directly incorporated. This means the implementation used by
GDAL and OGDI will diverge.

For the most part the change results in some OGR SQL statements to work
that would previously have generated an error.

Performance Implications
------------------------

I am hopeful that the evaluation speed will not be significantly
different for simple selections, but each output field will need to be
evaluated as an expression (with presumably one value-from-field node).

Implementation Plan
-------------------

Frank Warmerdam will implement, test and document for the GDAL/OGR 1.8
release.

Testing
-------

All existing OGR SQL test suite tests should pass. A new
autotest/ogr/ogr_sql_rfc28.py script will be introduced to test new
functionality.

Documentation
-------------

The :ref:`OGR SQL <ogr_sql_dialect>` document will be
extended to describe the new capabilities.
