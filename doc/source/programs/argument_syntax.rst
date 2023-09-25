.. _argument_syntax:

================================================================================
Syntax of arguments of command-line utilities
================================================================================

The following table describes the notation used to document the syntax of
arguments of command-line utilities.

.. list-table::
   :header-rows: 1

   * - Notation
     - Description

   * - Text without brackets or braces
     - Text that must be typed as shown

   * - ``<value>``
     - Placeholder for which a value must be substituted.

   * - ``[`` Text inside square brackets ``]``
     - Optional item.

   * - ``{`` Text inside braces  ``}``
     - Set of required items, separated by a vertical bar.

   * - Vertical bar (``|``)
     - Separator between mutually exclusive items. At least one must be specified.

   * - Ellipsis (``...``)
     - Placed after an item that can be repeated.


Example
+++++++

Given the following synopsis:

.. code-block::

    my_utility [-wkt_format {WKT1|WKT2}] [-oo <NAME>=<VALUE>]...
               <datasetname>

The following command lines are valid:

.. code-block::

    my_utility foo.tif


.. code-block::

    my_utility -wkt_format WKT1 foo.tif


.. code-block::

    my_utility -oo FOO=BAR -oo BAR=BAZ foo.tif
