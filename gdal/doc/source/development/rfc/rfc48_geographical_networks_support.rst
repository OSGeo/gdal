.. _rfc-48:

=======================================================================================
RFC 48: Geographical networks support
=======================================================================================

Author: Mikhail Gusev, Dmitry Baryshnikov

Contact: gusevmihs at gmail dot com, polimax@mail.ru

Status: adopted, implemented in GDAL 2.1

Introduction
------------

This document proposes the integration of the results of GSoC 2014
project “GDAL/OGR Geography Network support” into GDAL library. GNM
(Geographical Network Model) intends to bring the capabilities to
create, manage and analyse networks built over spatial data in GDAL.

GSoC project description:
`http://trac.osgeo.org/gdal/wiki/geography_network_support <http://trac.osgeo.org/gdal/wiki/geography_network_support>`__

GDAL fork with all changes in trunk:
`https://github.com/MikhanGusev/gdal <https://github.com/MikhanGusev/gdal>`__

GSoC blog:
`http://gsoc2014gnm.blogspot.ru/ <http://gsoc2014gnm.blogspot.ru/>`__

Purpose and description
-----------------------

There is a need to have an instrument in GDAL which on the one hand
provides an abstraction for different existed network formats
(pgRouting, OSRM, GraphHopper, SpatiaLite networks, etc.), like GDAL
(previously OGR) provides one for spatial vector formats, and on the
other hand provides a network functionality to those spatial formats
which does not have it at all (Shapefiles).

Such instrument is implemented as a separate set of C++ classes, called
GNM. The two main of them represent an abstract network (GNMNetwork
class) and the network of ”GDAL-native” or generic format
(GNMGenericNetwork class). An abstract network is used by user as a
common interface to manage his network data. The list of underlying
format-specific classes can be extended anytime like a list of GDAL
drivers to support more network formats. The ”GDAL-native” format
implements the abstract network and is used to provide the network
functionality to the spatial formats which are already supported by
GDAL. All the network data of this format is stored in the special set
of layers along with spatial data in a spatial dataset (internally
GDALDataset and OGRLayer are widely used).

What does the interface of working with networks include:

::

   * Creating/removing networks
   * Creating network topology over spatial data manually or automatically
   * Reading resulting connections in a common way
   * Adding/removing  spatial layers/features to the network
   * Defining business logic of the networks (e.g. the way of apply or deny connections with different layer features)
   * Several methods of network analysis

See the class architecture document (gdal/gnm/gnm_arch.dox) for more
details and how this set of classes internally works.

Bindings
--------

The C API wrapper functions are declared in gdal/gnm/gnm_api.h. All
current python bindings are implemented in a swig interface file and use
these C functions.

Set of applications
-------------------

It is proposed to include the two following apps which use the GNM into
GDAL source tree:

::

   * gnmmanage. Similar to gdalmanage purposes. Manages the networks of “GDAL-native” format: creates, removes networks, builds topology manually and automatically (as the GNMNetwork inherited from GDALDataset, the gdalmanage can be used with GNMNetwork)
   * gnmanalyse. Uses the analysing capabilities of GNM. Currently: shortest path(s) and connected components searches

See the description of these applications in according documentation for
more details.

Implementation
--------------

There is already a pull request on github
(`https://github.com/OSGeo/gdal/pull/60 <https://github.com/OSGeo/gdal/pull/60>`__)
that implements this RFC.

Building GDAL with GNM support
------------------------------

By default the building of GNM support is disabled. To build GNM support
one have to add --with-gnm key to configure or uncomment the appropriate
line in nmake.opt.

Set of tests
------------

All public methods of GNMNetwork tested in autotest gnm tests. The
several tests for GNMGenericNetwork added. The console applications
(gnmmanage and gnmanalyse) tested in autotest/utilities.

All tests were implemented according to the general rules: they are
written on Python and situated in /autotest folder:

::

   * GNM basic tests. Tests the basic “GDAL-network” functionality, using some small test shapefiles
   * GNM utilities tests. Simple tests of the gnmmanage and gnmanalyse utilities, similarly to ogrinfo tests

Documentation structure
-----------------------

All new methods and GNM classes are documented. GDAL documentation is
updated when necessary.

The following new Doxyfiles in /gnm and /apps directories will be
automatically built into the main auto-generated html into the “Related
pages” section. All them are similar to OGR docs:

::

   * GNM Architecture. The purpose and description of all GNM C++ classes
   * GNM Tutorial. The guide how to use the C++ GNM classes
   * GNM Utility Programs. The references to two GNM utilities
   * gnmmanage. Description and usage of gnmmanage utility
   * gnmanalyse. Description and usage of gnmanalyse utility

Source code tree organization
-----------------------------

*What is being added:*

The integration will cause the *addition* of new folders with header,
source, make and doc files:

::

   * gcore/gdal.h - add new driver type GNM
       * gdal/gnm – the main folder of GNM
   * Source code and documentation files of applications at gdal/apps
   * Testing python scripts at autotest/gnm and autotest/utilities
   * Two testing shapefiles at autotest/gnm/data (~7 Kb)
   * Swig interface file at gdal/swig/include

*What is being modified:*

The *changing* of the existed GDAL files *will be insignificant*:

::

   * GNUMakefile, makefile.vc and their configurations at /gdal and /gdal/apps
       * /autotest/run_all.py to add gnm tests
   * /autotest/pymod/test_cli_utilities.py to add the utility testing command
       * /autotest/utilities/test_gnmutils.py
   * /swig/python/setup.py and setup.cfg to add gnm module
       * GNUMakefile and makefile.vc at /swig/java

Future ideas
------------

I see many useful and interesting ways of GNM expending in future:

-  More formats support. The important thing, which must be firstly
   implemented in future, while the GNM intends to work with as many
   network formats as possible. It includes not only the support of
   GNMGdalNetwork formats – i.e. the testing to work with other GDAL
   spatial formats (currently tested only for Shapefiles and PostGIS).
   For example:

   -  GNMPGRoutingNetwork. Works with pgRouting tables. Some ideas:

      -  GNMPGRoutingNetwork::ConnectFeatures() will add to "source" and
         "target" columns according values via OGRFeature::setField()
      -  GNMPGRoutingNetwork::AutoConnect() will internally call
         pgr_createTopology method

   -  GNMSQLiteNetwork. Works with SpatiaLite VirtualNetwork networks.
      Some ideas:

      -  write all network data to the Roads_net_data table and to
         corresponding NodeFrom and NodeTo columns

   -  GNMGMLNetwork. Works with the GML topology. Some ideas:

      -  write network data to the
         `gml::TopoComplex <gml::TopoComplex>`__,
         `gml::Node <gml::Node>`__ and `gml::Edge <gml::Edge>`__
         directly

-  More effective algorithm of topology building in GDAL-networks. The
   current one is implemented as the default for any network format and
   can connect any amount of line and point layers but is not so
   efficient – the large networks are being connected too long.
   GNMGenericNetwork can have more effective default algorithm.

-  More rules in GDAL-networks, i.e. more complex syntax describing the
   following:

   -  costs extracted from geometrical lengths of lines
   -  turn restriction roles of features
   -  more complex connection rules: set the limit of features can be
      connected and more complex expressions

-  Applications. May be one of the most useful application which can be
   build with GNM is *network2network*, which converts the network and
   spatial data of the dataset from the one format to another (for
   example from pgRouting to Oracle Spatial networks);

-  Analysis. The support of different graph types and the algorithms
   working with them, for different routing and even engineering
   purposes. For example:

   -  Boost library
   -  Contraction Hierarchies technology (for large graphs)

Voting history
--------------

+1 from JukkaR, TamasS and EvenR
