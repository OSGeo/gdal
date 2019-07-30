.. _gnmanalyse:

================================================================================
gnmanalyse
================================================================================

.. only:: html

    Analyses networks

.. Index:: gnmanalyse

Synopsis
--------

.. code-block::

    gnmanalyse [--help][-q][-quiet][--long-usage]
            [dijkstra <start_gfid> <end_gfid> [[-alo NAME=VALUE] ...]]]
            [kpaths <start_gfid> <end_gfid> <k> [[-alo NAME=VALUE] ...]]]
            [resource [[-alo NAME=VALUE] ...]]]
            [-ds <ds_name>][-f <ds_format>][-l <layer_name>]
            [[-dsco NAME=VALUE] ...][-lco NAME=VALUE]
            <gnm_name>

Description
-----------

The :program:`gnmanalyse` program provides analysing capabilities of geographical networks in GDAL. The results of calculations are return in an OGRLayer format or as a console text output if such layer is undefined. All calculations are made considering the blocking state of features.

.. program:: gnmanalyse

.. option:: dijkstra <start_gfid> <end_gfid>

    Calculates the best path between two points using Dijkstra algorithm from start_gfid point to end_gfid point.

.. option:: kpaths <start_gfid> <end_gfid>

    Calculates K shortest paths between two points using Yen's algorithm (which internally uses Dijkstra algorithm for single path calculating) from start_gfid point to end_gfid point.

.. option:: resource

    Calculates the "resource distribution". The connected components search is performed using breadth-first search and starting from that features which are marked by rules as 'EMITTERS'.

.. option:: -d <ds_name>

    The name and path of the dataset to save the layer with resulting paths. Not need to be existed dataset.

.. option:: -f <ds_format>

    Define this to set the format of newly created dataset.

.. option:: -l <layer_name>

    The name of the resulting layer. If the layer exist already - it will be rewritten.

.. option:: <gnm_name>

    The network to work with (path and name).

.. option:: -dsco NAME=VALUE

    Dataset creation option (format specific)

.. option:: -lco NAME=VALUE

    Layer creation option (format specific)

.. option:: -alo NAME=VALUE

    Algorithm option (format specific)
