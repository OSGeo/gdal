.. _rfc-62:

=======================================================================================
RFC 62 : Raster algebra
=======================================================================================

Author: Ari Jolma

Contact: ari.jolma at gmail.com

Status: Development

Implementation in version:

Summary
-------

It is proposed that a set of functions or methods are written for raster
band objects to support "raster algebra", i.e., a set of operations,
which modify bands or compute values from bands. An example of a
modification is adding a value to all the cells of the band. An example
of a computation is the maximum cell value in the band. Operations may
or may not take arguments, in addition to the band itself, and if they
take, the argument may be a numeric value, a data structure, or another
band. Similarly, the computed value may be a simple numeric value, a
data structure, or another band.

Rationale
---------

Raster algebra is a well known branch of geospatial science and
technology and an often needed tool. Currently GDAL does not have
comprehensive support for raster algebra in core.

Related work
------------

Python bindings: raster bands or parts of raster bands can be read into
/ written from numpy array objects. Huge rasters can be iterated block
by block. numpy methods allow efficient implementation of many raster
algebra methods using python. There is some support for parallel
processing using numpy.

Perl bindings: raster bands or parts of raster bands can be read into /
written from PDL objects. Huge rasters can be iterated block by block.
PDL methods allow efficient implementation of many raster algebra
methods using perl. There is some support for parallel processing using
PDL.

QGIS raster calculator: raster calculation string is parsed into an
expression tree and output band is computed row by row from the inputs.
All computations are done with double precision floating point numbers.
The calculation does not support parallel processing.

PostGIS raster: raster algebra is supported by callback functions.

There is existing research, which may be exploited:

Parallel Priority-Flood depression filling for trillion cell digital
elevation models on desktops or clusters
`http://www.sciencedirect.com/science/article/pii/S0098300416301704 <http://www.sciencedirect.com/science/article/pii/S0098300416301704>`__

Parallel Non-divergent Flow Accumulation For Trillion Cell Digital
Elevation Models On Desktops Or Clusters
`https://arxiv.org/abs/1608.04431 <https://arxiv.org/abs/1608.04431>`__

Requirements (Goals)
--------------------

The implementation should be data type aware. This may mean code written
with templates.

The implementation should be parallel processing friendly.

The implementation should allow a relatively easy to use C++ / C API.
This may mean interface, which does not use templates.

The implementation should allow arbitrary functions on cell values.
I.e., be extensible by the user.

The implementation should allow focal methods. I.e., methods, where the
value of a cell depends on its neighborhood.

Approach
--------

The implementation does not need to be tightly integrated with the core.
This means an "add-on" type solution is ok.

GDAL design sets some constraints/requirements to raster algebra
implementation: 1) the access to data is based on blocks, 2) GDAL
supports several datatypes, even complex values, 3) there is no
immediate support for the not-simple data structures needed by some
methods (by "method" I refer to functions of raster algebra in this
text), 4) data can be read from a band in parallel but writing needs to
be exclusive.

Changes
-------

Drivers
-------

Drivers are not affected.

Bindings
--------

The functionality will be added to the bindings.

Utilities
---------

Existing utilities are not affected but new utilities may be written
taking advantage of the new functionality.

Documentation
-------------

Must be written.

Test Suite
----------

Must be written.

Compatibility Issues
--------------------

Related tickets
---------------

Implementation
--------------

A proposed implementation is developed at
`https://github.com/ajolma/raster_algebra <https://github.com/ajolma/raster_algebra>`__

This code attempts to solve the problem as follows. (The source is in
transition from an old approach, which was based on operators as
methods, while the new approach is based on operator classes)

Classes 'operand' and 'operator' are defined. An operand is an object,
which holds data and an operator is an object, which computes a result
(essentially an operand) from operands.

Raster algebra computation is a tree of operand and operator objects,
which is executed in a recursive fashion.

There are interface classes and templated concrete classes. The concrete
classes inherit from the interface classes.

Two operand classes are defined: a number and a band. There is a need
for other types of operands. For example a classifier would map integer
values or real number ranges into numbers. Code for such exists in the
source but it is not organized to reflect the new approach.

A central method is 'compute' in band class, which is basically the
effective block loop code presented in the documentation for
GDALRasterBand::ReadBlock.

Multiple data types are supported by template concrete class for bands
and by overloaded get_value method, which returns the value in required
data type.

Voting history
--------------

