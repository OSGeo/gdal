.. _gnmmanage:

================================================================================
gnmmanage
================================================================================

.. only:: html

    Manages networks

.. Index:: gnmmanage

Synopsis
--------

.. code-block::

    gnmmanage [--help][-q][-quiet][--long-usage]
            [info]
            [create [-f <format_name>] [-t_srs <srs_name>] [-dsco NAME=VALUE]... ]
            [import src_dataset_name] [-l layer_name]
            [connect <gfid_src> <gfid_tgt> <gfid_con> [-c <cost>] [-ic <inv_cost>] [-dir <dir>]]
            [disconnect <gfid_src> <gfid_tgt> <gfid_con>]
            [rule <rule_str>]
            [autoconnect <tolerance>]
            [delete]
            [change [-bl gfid][-unbl gfid][-unblall]]
            <gnm_name> [<layer> [<layer> ...]]


Description
-----------

The :program:`gnmmanage` program can perform various managing operations on geographical networks in GDAL. In addition to creating and deleting networks this includes capabilities of managing network's features, topology and rules.

.. program:: gnmmanage

.. option:: -info

    Different information about network: system and class layers, network metadata, network spatial reference.

.. option:: create

    Create network.

    .. option:: -f <format_name>

        Output file format name.

    .. option:: -t_srs <srs_name>

        Spatial reference input.

    .. option:: -dsco NAME=VALUE

        Network creation option set as pair name=value.

.. option:: import <src_dataset_name>

    Import layer with dataset name to copy.

    .. option:: -l layer_name

    Layer name in dataset. If unset, 0 layer is copied.

.. option:: connect <gfid_src> <gfid_tgt> <gfid_con>

    Make a topological connection, where the gfid_src and gfid_tgt are vertices and gfid_con is edge (gfid_con can be -1, so the system edge will be inserted).

    Manually assign the following values: 

    .. option:: -c <cost>

        Cost / weight

    .. option:: -ic <invcost>

        Inverse cost

    .. option:: -dir <dir>

        Direction of the edge.

.. option:: disconnect <gfid_src> <gfid_tgt> <gfid_con>

    Removes the connection from the graph.

.. option:: rule <rule_str>

    Creates a rule in the network by the given rule_str string.

.. option:: autoconnect <tolerance>

    Create topology automatically with the given double tolerance and layer names. In no layer name provided all layers of network will be used.

.. option:: delete

    Delete network.

.. option:: change

    Change blocking state of network edges or vertices.

    .. option:: -bl <gfid>

        Block feature before the main operation. Blocking features are saved in the special layer.

    .. option:: -unbl <gfid>

        Unblock feature before the main operation.

    .. option:: -unblall

        Unblock all blocked features before the main operation.

.. option:: <gnm_name>

    The network to work with (path and name).

.. option:: <layer>

    The network layer name.
