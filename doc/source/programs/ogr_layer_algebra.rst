.. _ogr_layer_algebra:

================================================================================
ogr_layer_algebra.py
================================================================================

.. only:: html

    Performs various Vector layer algebraic operations.

.. Index:: ogr_layer_algebra

Synopsis
--------

.. code-block::

    ogr_layer_algebra.py Union|Intersection|SymDifference|Identity|Update|Clip|Erase
                        -input_ds name [-input_lyr name]
                        -method_ds [-method_lyr name]
                        -output_ds name [-output_lyr name] [-overwrite]
                        [-opt NAME=VALUE]*
                        [-f format_name] [-dsco NAME=VALUE]* [-lco NAME=VALUE]*
                        [-input_fields NONE|ALL|fld1,fl2,...fldN] [-method_fields NONE|ALL|fld1,fl2,...fldN]
                        [-nlt geom_type] [-a_srs srs_def]

Description
-----------

The :program:`ogr_layer_algebra.py` provides a command line utility to perform various vector layer algebraic operations. The utility takes a vector 
input source , a method source and generates the output of the operation in the specified output file

.. program:: ogr_layer_algebra

.. option:: <mode>

    Where <mode> is one of the seven available modes:

    * ``Union``

        A union is a set of features, which represent areas that are in either of the operand layers. 

    * ``Intersection``

        An intersection is a set of features, which represent the common areas of two layers. 

    * ``SymDifference``

        A symmetric difference is a set of features, which represent areas that are in operand layers but which do not intersect. 

    * ``Identity``

        The identity method identifies features in the input layer with features in the method layer possibly splitting features into several features. 
        By default the result layer has attributes from both operand layers. 

    * ``Update``

        The update method creates a layer, which add features into the input layer from the method layer possibly cutting features in the input layer. 
        By default the result layer has attributes only from the input layer. 

    * ``Clip``

        The clip method creates a layer, which has features from the input layer clipped to the areas of the features in the method layer. 
        By default the result layer has attributes of the input layer. 

    * ``Erase``

        The erase method creates a layer, which has features from the input layer whose areas are erased by the features in the method layer. 
        By default the result layer has attributes of the input layer. 


