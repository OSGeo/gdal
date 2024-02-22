.. include:: ../substitutions.rst

.. _dev_documentation:

================================================================================
Building documentation
================================================================================

Documentation overview
###########################

GDAL's documentation includes C and C++ :ref:`API documentation <api>` built
automatically from source comments using doxygen and reStructuredText (rst)
files containing manually-edited content.

|Sphinx| is used to combine the above components into a complete set of documentation in HTML, PDF, and other formats.

|Sphinx| and extensions used by GDAL can be installed by running ``python3 -m pip install -r requirements.txt`` from
the ``doc`` subdirectory.

Building documentation
######################

HTML documentation can be built by running ``make html`` in the ``doc`` subdirectory of the GDAL source repository.
The generated files will be output to ``doc/build`` where they can be viewed using a web browser.
Doxygen content that is incorporated into the output is not automatically rebuilt but can be regenerated using ``make doxygen``.

To visualize documentation changes while editing, it may be useful to install the |sphinx-autobuild| python package.
Once installed, running ``sphinx-autobuild -b html source build`` from the ``doc`` subdirectory will build documentation
and serve it on a local web server at ``http://127.0.0.1:8000``. The pages served will be automatically refreshed as changes
are made to underlying ``rst`` documentation files.

Python API documentation
------------------------

Sphinx uses the `autodoc <https://www.sphinx-doc.org/en/master/usage/extensions/autodoc.html>`_ extension
to generate documentation for the Python API from Python function docstrings.
To be correctly parsed by ``autodoc``, docstrings should follow the `numpydoc Style guide <https://numpydoc.readthedocs.io/en/latest/format.html>`_.
Docstrings may be found in two locations. If the function was defined in Python
(i.e., using a ``%pythoncode`` SWIG directive), then the docstring must be
placed within the function definition. If the function is defined in C++ only,
then the docstring should be placed in a separate file
containing only docstrings (located in :source_file:`swig/include/python/docs`).
Sphinx loads the Python bindings when generating documentation, so for it to see any changes
the following steps must be completed:

- rebuild the Python bindings from the build directory (``cmake --build . --target python_binding``)
- make the updated Python bindings visible to Python, either by installing them, or by running ``scripts/setdevenv.sh``
  from the build directory
- update the timestamp of the ``rst`` files associated with the page where the documentation appears (e.g., ``touch doc/source/api/python/osgeo.ogr.rst``)


.. _rst_style:

Sphinx RST Style guide
######################

This section contains syntax rules, tips, and tricks for using Sphinx and reStructuredText.  For more information, please see this  `comprehensive guide to reStructuredText <http://docutils.sourceforge.net/docs/ref/rst/restructuredtext.html>`_, as well as the `Sphinx reStructuredText Primer <http://sphinx.pocoo.org/rest.html>`_.

Basic markup
------------

A reStructuredText document is written in plain text.  Without the need for complex formatting, one can be composed simply, just like one would any plain text document.  For basic formatting, see this table:


.. list-table::
   :widths: 30 40 30

   * - **Format**
     - **Syntax**
     - **Output**
   * - Italics
     - ``*italics*`` (single asterisk)
     - *italics*
   * - Bold
     - ``**bold**`` (double asterisk)
     - **bold**
   * - Monospace
     - `` ``monospace`` `` (double back quote)
     - ``monospace``

.. warning:: Use of basic markup is **not recommend**! Where possible use sphinx inline directives (described below) to logically mark commands, parameters, options, input, and files. By using directives consistently these items can be styled appropriately.

Lists
-----

There are two types of lists, bulleted lists and numbered lists.  A **bulleted list** looks like this:

* An item
* Another item
* Yet another item

This is accomplished with the following code::

   * An item
   * Another item
   * Yet another item

A **numbered list** looks like this:

#. First item
#. Second item
#. Third item

This is accomplished with the following code::

   #. First item
   #. Second item
   #. Third item

Note that numbers are automatically generated, making it easy to add/remove items.

List-tables
-----------

Bulleted lists can sometimes be cumbersome and hard to follow.  When dealing with a long list of items, use list-tables.  For example, to talk about a list of options, create a table that looks like this:

.. list-table::
   :widths: 20 80
   :header-rows: 1

   * - Shapes
     - Description
   * - Square
     - Four sides of equal length, 90 degree angles
   * - Rectangle
     - Four sides, 90 degree angles

This is done with the following code::

   .. list-table::
      :widths: 20 80
      :header-rows: 1

      * - Shapes
        - Description
      * - Square
        - Four sides of equal length, 90 degree angles
      * - Rectangle
        - Four sides, 90 degree angles

Page labels
-----------

**Ensure every page has a label that matches the name of the file.** For example if the page is named ``foo_bar.rst`` then the page should have the label::

   ..  _foo_bar:

Other pages can then link to that page by using the following code::

   :ref:`foo_bar`

.. _linking:

Linking
-------

Links to other pages should never be titled as "here".  Sphinx makes this easy by automatically inserting the title of the linked document.

Bad
   More information about linking can be found :ref:`here <linking>`.
Good
   For more information, please see the section on :ref:`linking`.

To insert a link to an external website::

   `Text of the link <http://example.com>`__

The resulting link would look like this: `Text of the link <http://example.com>`__

.. warning:: It is very easy to have two links with the same text resulting in the following error::

   **(WARNING/2) Duplicate explicit target name:foo**

   To avoid these warnings use of a double `__` generates an anonymous link.


Sections
--------

Use sections to break up long pages and to help Sphinx generate tables of contents.

::

    ================================================================================
    Document title
    ================================================================================

    First level
    -----------

    Second level
    ++++++++++++

    Third level
    ***********

    Fourth level
    ~~~~~~~~~~~~

Notes and warnings
------------------

When it is beneficial to have a section of text stand out from the main text, Sphinx has two such boxes, the note and the warning.  They function identically, and only differ in their coloring.  You should use notes and warnings sparingly, however, as adding emphasis to everything makes the emphasis less effective.

Here is an example of a note:

.. note:: This is a note.

This note is generated with the following code::

   .. note:: This is a note.

Similarly, here is an example of a warning:

.. warning:: Beware of dragons.

This warning is generated by the following code::

   .. warning:: Beware of dragons.

Images
------

Add images to your documentation when possible.  Images, such as screenshots, are a very helpful way of making documentation understandable.  When making screenshots, try to crop out unnecessary content (browser window, desktop, etc).  Avoid scaling the images, as the Sphinx theme automatically resizes large images.  It is also helpful to include a caption underneath the image.::

  .. figure:: image.png
     :align: center

     *Caption*

In this example, the image file exists in the same directory as the source page.  If this is not the case, you can insert path information in the above command. The root :file:`/` is the directory of the :file:`conf.py` file.::

  .. figure:: /../images/gdalicon.png

External files
--------------

Text snippets, large blocks of downloadable code, and even zip files or other binary sources can all be included as part of the documentation.

To include link to sample file, use the ``download`` directive::

   :download:`An external file <example.txt>`

The result of this code will generate a standard link to an :download:`external file <example.txt>`

To include the contents of a file, use ``literalinclude`` directive::

   Example of :command:`gdalinfo` use:

   .. literalinclude:: example.txt

Example of :command:`gdalinfo` use:

.. literalinclude:: example.txt

The ``literalinclude`` directive has options for syntax highlighting, line numbers and extracting just a snippet::

   Example of :command:`gdalinfo` use:

   .. literalinclude:: example.txt
      :language: txt
      :linenos:
      :emphasize-lines: 2-6
      :start-after: Coordinate System is:
      :end-before: Origin =

Reference files and paths
-------------------------

Use the following syntax to reference files and paths::

   :file:`myfile.txt`

This will output: :file:`myfile.txt`.

You can reference paths in the same way::

   :file:`path/to/myfile.txt`

This will output: :file:`path/to/myfile.txt`.

For Windows paths, use double backslashes::

   :file:`C:\\myfile.txt`

This will output: :file:`C:\\myfile.txt`.

If you want to reference a non-specific path or file name::

   :file:`{your/own/path/to}/myfile.txt`

This will output: :file:`{your/own/path/to}/myfile.txt`

To reference a file in the GDAL source tree, use::

    :source_file:`gcore/gdaldriver.cpp`

This will output a link to the file on GitHub: :source_file:`gcore/gdaldriver.cpp`

Reference code
--------------

To reference a class::

  :cpp:class:`MyClass`

To reference a method or function::

  :cpp:func:`MyClass::MyMethod`
  :cpp:func:`MyFunction`

.. _config_option_syntax:

Define and reference configuration options
------------------------------------------

To define a configuration option, use::

   .. config:: OPTION_NAME
      :choices: COMMA, SEPARATED, LIST
      :default: DEFAULT_VALUE
      :since: GDAL.MIN.VERSION

      Narrative about the option.

Similar syntax can be used to define opening options (``.. oo::``),
creation options (``.. co::``), dataset creation options (``.. dsco::``), or layer creation options
(``.. lco::``).

To reference a configuration option, such as **GDAL_CACHEMAX**,
use the syntax in the table below.

.. list-table::
   :header-rows: 1

   * - Option type
     - Syntax
   * - Configuration option
     - ::

       :config:`option_name`
   * - Creation option
     - ::

       :co:`option_name`
   * - Open option
     - ::

       :oo:`option_name`
   * - Dataset creation option
     - ::

       :dsco:`option_name`
   * - Layer creation option
     - ::

       :lco:`option_name`


Reference commands
------------------

Reference commands (such as :program:`gdalinfo`) with the following syntax::

  :program:`gdalinfo`

Use ``option`` directive for command line options::

  .. option:: -json

     Display the output in json format.

Use ``describe`` to document create parameters::

  .. describe:: WORLDFILE=YES

     Force the generation of an associated ESRI world file (with the extension .wld).
