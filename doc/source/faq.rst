.. _faq:

================================================================================
FAQ
================================================================================

.. TODO maybe migrate the chapters 2 and following of http://web.archive.org/web/https://trac.osgeo.org/gdal/wiki/FAQ

.. only:: not latex

    .. contents::
       :depth: 3
       :backlinks: none

What does GDAL stand for?
+++++++++++++++++++++++++

GDAL - Geospatial Data Abstraction Library

It is sometimes pronounced "goo-doll" (a bit like goo-gle), while others pronounce it "gee-doll," and others pronounce it "gee-dall."

`Listen <https://soundcloud.com/danabauer/how-do-you-pronounce-gdal#t=00:02:58>`__ how Frank Warmerdam pronounces it and the history behind the acronym.

What is this OGR stuff?
+++++++++++++++++++++++++

OGR used to be a separate vector IO library inspired by OpenGIS Simple Features which was separated from GDAL. With the GDAL 2.0 release, the GDAL and OGR components were integrated together.

What does OGR stand for?
+++++++++++++++++++++++++

See :term:`OGR`.

What does CPL stand for?
+++++++++++++++++++++++++

See :term:`CPL`.

What does VSI stand for?
+++++++++++++++++++++++++

See :term:`VSI`.

When was the GDAL project started?
++++++++++++++++++++++++++++++++++

In late 1998, Frank Warmerdam started to work as independent professional on the GDAL/OGR library.

Is GDAL/OGR proprietary software?
+++++++++++++++++++++++++++++++++

No, GDAL/OGR is a Free and Open Source Software.

What license does GDAL/OGR use?
+++++++++++++++++++++++++++++++

See :ref:`license`

What operating systems does GDAL-OGR run on?
++++++++++++++++++++++++++++++++++++++++++++

You can use GDAL/OGR on all modern flavors of Unix: Linux, FreeBSD, Mac OS X; all supported versions of Microsoft Windows; mobile environments (Android and iOS). Both 32-bit and 64-bit architectures are supported.

Is there a graphical user interface to GDAL/OGR?
++++++++++++++++++++++++++++++++++++++++++++++++

Plenty! Among the available options, `QGIS <https://qgis.org>`__ is an excellent
free and open source application with a rich graphical user interface. It
allows users to display most GDAL raster and vector supported formats and
provides access to most GDAL utilities through its Processing toolbox.

You can also consult the :ref:`list of software using GDAL <software_using_gdal>`.

.. toctree::
   :hidden:

   software_using_gdal

What compiler can I use to build GDAL/OGR?
++++++++++++++++++++++++++++++++++++++++++++++++

GDAL/OGR must be compiled with a C++17 capable compiler since GDAL 3.9 (C++11 in previous versions)

Build requirements are described in :ref:`build_requirements`.

I have a question that's not answered here. Where can I get more information?
+++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++

See :ref:`community`

Keep in mind, the quality of the answer you get does bear some relation to the quality of the question. If you need more detailed explanation of this, you can find it in essay `How To Ask Questions The Smart Way <http://www.catb.org/~esr/faqs/smart-questions.html>`_ by Eric S. Raymond.

How do I add support for a new format?
++++++++++++++++++++++++++++++++++++++

To some extent this is now covered by the :ref:`raster_driver_tut` and :ref:`vector_driver_tut`

What about file formats for 3D models?
++++++++++++++++++++++++++++++++++++++

While some vector drivers in GDAL support 3D geometries (e.g. reading CityGML or
AIXM through the GML driver, or 3D geometries in GeoPackage), 3-dimensional scenes or
models formats (such as glTF, OBJ, 3DS, etc.) are out-of-scope for GDAL.
Users may consider a library such as `ASSIMP (Open-Asset-Importer-Library) <https://github.com/assimp/assimp>`__
for such formats.

Is GDAL thread-safe ?
+++++++++++++++++++++

See :ref:`multithreading`

Does GDAL provide a Section 508 information?
++++++++++++++++++++++++++++++++++++++++++++

No, GDAL itself is an open-source software and project, not a Vendor. If your organization considers they need a `VPAT or Section 508 <https://www.section508.gov/sell/acr/>`_ form to be able to use GDAL, it is their responsibility to complete the needed steps themselves.

How do I cite GDAL ?
++++++++++++++++++++

See `CITATION`_

.. developer note: do not use :source_file:, as it breaks latexpdf output

.. _`CITATION`: https://github.com/OSGeo/gdal/blob/master/CITATION


.. below is an allow-list for spelling checker.

.. spelling:word-list::
    glTF
