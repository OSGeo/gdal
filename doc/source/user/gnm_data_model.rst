.. _gnm_data_model:

================================================================================
Geographic Networks Data Model
================================================================================

This document is intended to describe the purpose and the structure of Geographic Network Model classes. GNM is the part of GDAL and provides the methods of creating, managing and analysing geographical networks.

The key purpose of GNM classes:
- To provide an abstraction for different existed network formats, like GDAL (previously OGR) provides one for spatial vector formats;
- To provide a network functionality to those spatial formats which does not have it at all.

General concept
---------------

Any real-world network can be represented as a set of vector data, which can be itself represented in GDAL as a GDALDataset. In GNM this data consists of two parts. Network's topology (graph), network's metadata (name/description), set of special feature identifiers, etc. belong to the "network part", while the common for GDAL layers, features, geometries belong to the "spatial/attribute part". In order to work with the datasets of different formats the following classes were designed in GNM.

Network
+++++++

:cpp:class:`GNMNetwork` represents an abstract network. The network data and spatial/attribute data in a dataset of some format in fact can be not separable (just additional layers/fields/tags), while the concrete implementation of GNMNetwork "knows" which data from the whole dataset refers to "network part" and is able to operate it. GNMNetwork allows user the following:

-Setting/unsetting connections. These generic methods of building the network topology (automatically and manually) receive the identifiers of features being connected in a common way, while the concrete implementation knows where and how to store and build the topology;
-Reading connections. The generic methods return the connections in the common way;
-Adding/removing layers/features. When the feature or layer is being added to the network some actions can be initiated (weights change in a graph, cascade changes in connected features). Concrete GNMNetwork describes how it is done.
-Defining network's business logic or behavior. It can be expressed in network rules or constraints/restrictions. Expected that each rule can be set from a string and each concrete GNMNetwork will transform it to the internal look.

Format
++++++

GNMNetwork inherits GDALDataset and looks like OGRDatasource with additional functionality. There are a set of GDAL drivers for networks. The generic network implementation in GDAL provides additional functionality like rules, virtual edges and vertices. Also, while editing the feature the network control the network rules and other specific, and can deny saving edits. The other network drivers (pgRouting, OSRM, GraphHopper, etc.) should provide the basic functionality via the GNMNetwork class.

Network formats
---------------

To add a ``native`` support of the existed network format (like PostGIS pgRouting, Oracle Spatial Networks, topology in GML, etc.) to GNM the developer should implement the corresponding GNMDriver-GNMNetwork interface. But there is also a capability to use the ``generic`` network format, which is already implemented in GNM as a special class. It can be extremely useful when there is a need to create and use a network in the format that initially does not have its "network part" (like ESRI Shapefile) directly.

GNMGenericNetwork
+++++++++++++++++

:cpp:class:`GNMGenericNetwork` is a concrete implementation of the GNMNetwork. GNMGenericNetwork intends to support the most GDALDataset drivers (depends on the corresponding driver capabilities). Technically the network format abstraction is achieved with the help of GDAL abstraction: datasets and layers approach. GNMGdalNetwork aggregates a GDALDataset instance where the "network part" is represented as a set of "system layers" (wkbNone geometry, specific attribute fields) and the spatial/attribute data is regarded as the set of "class layers" or "classes" (layers with geometries and attributes, as usual). The "network part" is created and maintained by GNMGenericNetwork automatically and provides methods to work with it.

The way of describing real-world networks by GNMGenericNetwork intends to be a generic, because:
-The most general type of graph is used, which holds every useful information: directions of edges (directed/undirected), edge costs (weighted/unweighted). This graph is stored as an incidence list: source vertex feature id, target vertex feature id, edge feature id, direct cost, inverse cost, direction of edge;
-Any feature with any geometry can be the vertex or the edge in a graph. Also, it may be no feature “under” the connection's edge at all (actually the virtual edge is created for this case). All this means that user operates with the feature identifiers, while the GNMGenericNetwork guaranties the connections integrity among features; 
-Any feature in the network will gain the unique identifier – Global Feature Identifier (GFID) which allows unify any amount of "class layers" under one network;
-GNMGenericNetwork uses its own way to determine the network's business logic. See :cpp:func:`GNMGenericNetwork::CreateRule` for more details.

See the :cpp:class:`GNMGenericNetwork` class documentation for more details.

The network of common format has also the following important features:
-The single spatial reference system is used in the network, that means that each feature which appears in the network will be transformed to this SRS;
-The network always created void and there is a need to import or create features;
-It is not possible to remove the "network part" from the dataset – only delete the whole network with all data. The deletion is made layer by layer and deletes only system and class layers which registered in the network.

Network analysis
----------------

The network analysis in GNM is implemented in :cpp:class:`GNMNetwork` object.

:cpp:class:`GNMGenericNetwork` holds the graph in memory in STL containers and provides basic algorithms which return the results in the array-form (e.g. std::vector full of path's edges and vertices GFIDs). But the caller get a result as OGRLayer there features get from layers consist the network. Also some additional fields created (VERTEX/EDGE indicator field, GFID, layer name, etc.). The caller have to free the result OGRLayer via :cpp:func:`GDALDataset::ReleaseResultSet`
