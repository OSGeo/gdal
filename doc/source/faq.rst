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

`Listen <https://soundcloud.com/danabauer/how-do-you-pronounce-gdal#t=00:02:58>`__ how Frank Warmerdam prounounces it and the history behind the acronym.

What is this OGR stuff?
+++++++++++++++++++++++++

OGR used to be a separate vector IO library inspired by OpenGIS Simple Features which was separated from GDAL. With the GDAL 2.0 release, the GDAL and OGR components were integrated together.

What does OGR stand for?
+++++++++++++++++++++++++

OGR used to stand for OpenGIS Simple Features Reference Implementation. However, since OGR is not fully compliant with the OpenGIS Simple Feature specification and is not approved as a reference implementation of the spec the name was changed to OGR Simple Features Library. The only meaning of OGR in this name is historical. OGR is also the prefix used everywhere in the source of the library for class names, filenames, etc.

What does CPL stand for?
+++++++++++++++++++++++++

Common Portability Library. Think of it as GDAL internal cross-platform standard library. Back in the early days of GDAL development, when cross-platform development as well as compatibility and standard conformance of compilers was a challenge (or PITA), CPL proved necessary for smooth portability of GDAL/OGR.

CPL, or parts of it, is used by some projects external to GDAL (eg. MITAB, libgeotiff).

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

Is GDAL thread-safe ?
+++++++++++++++++++++

See :ref:`multithreading`

Does GDAL provide a Section 508 information?
++++++++++++++++++++++++++++++++++++++++++++

No, GDAL itself is an open-source software and project, not a Vendor. If your organization considers they need a `VPAT or Section 508 <https://www.section508.gov/sell/acr/>`_ form to be able to use GDAL, it is their responsibility to complete the needed steps themselves.

How do I cite GDAL ?
++++++++++++++++++++

See `CITATION`_

.. _`CITATION`: https://github.com/OSGeo/gdal/blob/master/CITATION
