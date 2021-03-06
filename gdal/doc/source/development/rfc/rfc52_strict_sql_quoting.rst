.. _rfc-52:

=======================================================================================
RFC 52: Strict OGR SQL quoting
=======================================================================================

Author: Even Rouault

Contact: even dot rouault at spatialys dot com

Status: adopted, implemented in GDAL 2.0

Summary
-------

This RFC proposes that OGR SQL enforces strict quoting rules for SQL
literals and identifiers

Details
-------

Currently the OGR SQL engine deals indifferently with single quote
characters (') and double quote characters ("), although they can be
used in 2 different contexts :

-  to specify string literals
-  to specify column or table names (when they need quoting)

SQL 92 mandates that string literals are surrounded with single quote
characters whereas quoted identifiers (column, table names) are
surrounded with double quote characters.

From
`http://savage.net.au/SQL/sql-92.bnf.html <http://savage.net.au/SQL/sql-92.bnf.html>`__:

::

   <delimited identifier>     ::= <double quote> <delimited identifier body> <double quote>
   <character string literal> ::= <quote> [ <character representation> ... ] <quote>

Current OGR behavior is generally fine, except in some situations when
specifying columns in a SELECT statement or in a WHERE expression. The
OGR SQL engine tries to determine the intent with the following rule :
if the quoted string matches a column name, then it is assumed to be a
column identifier, otherwise a string literal. But sometimes (see
`http://trac.osgeo.org/gdal/ticket/5428 <http://trac.osgeo.org/gdal/ticket/5428>`__),
there are situations where filters such as "MyField" = 'MYFIELD' are
needed, and currently they always evaluate to TRUE.

To avoid any ambiguities and more conformant to SQL, string literals
should be single-quoted, and identifiers (column/table names) unquoted
or surrounded by double-quote character if conflicting with reserved
keywords.

Implementation
--------------

Implementation will be done by Even Rouault
(`Spatialys <http://spatialys.com>`__).

The proposed implementation lies in the "rfc52_stricter_sql" branch of
the
`https://github.com/rouault/gdal2/tree/rfc52_stricter_sql <https://github.com/rouault/gdal2/tree/rfc52_stricter_sql>`__
repository.

The list of changes :
`https://github.com/rouault/gdal2/compare/rfc52_stricter_sql <https://github.com/rouault/gdal2/compare/rfc52_stricter_sql>`__

Compatibility
-------------

This change affects compatibility in the sense that users that relied on
the current permissive behavior will have to adapt their SQL
expressions. This might be quite tricky to detect as it will not always
cause obvious runtime failures. For example " SELECT
'i_thought_this_would_be_interpreted_as_a_column_name_but_now_it_is_a_string_litteral'
FROM atable " will run without error but not produce the previously
expected result. So applications will have to review how they build SQL
filters ( SetAttributeFilter() ) and expressions ( ExecuteSQL() )

Expressions fixed to be compliant with stricter rules will also work
with older GDAL versions.

A prominent warning should also be put on the OGR SQL documentation page
to advertise the change of behavior

Discussion
----------

Tests done with SQLite show that it is sometimes tolerant to misuse of
quote characters, but in the reverse way as current OGR. So it will
accept SELECT "non_existing_column" FROM 'atable' and interpret it as
SELECT 'non_existing_column' FROM "atable". On the contrary, PostgreSQL
will not accept it. I'd be rather inclined to follow PostgreSQL stricter
approach to keep things simple and understandable: "string literals
should be single-quoted, and identifiers (column/table names) unquoted
or surrounded by double-quote character"

Testing
-------

The Python autotest suite will be fixed to still pass.

Voting history
--------------

+1 from JukkaR, TamasS, DanielM and EvenR
