.. _gdal_python_utilities:

================================================================================
GDAL Python Utilities
================================================================================

The GDAL python utilities are included with GDAL. If you've installed
GDAL you already have them. However you may want to use a newer or older
version of the utilities without changing GDAL. This is where
**gdal-utils** comes in.

**gdal-utils**: is the GDAL Python Utilities *distribution*. This is
what you install. It's home page is https://pypi.org/project/gdal-utils/
. Install it with ``pip install gdal-utils`` and it will up or downgrade
the utilities in place.

**osgeo_utils**: is the python *package*. This is what you use in your
code after installing, e.g. ``from osgeo_utils import ...``. If you're
not writing code, ignore it.

Commonly used utilities include:

-  gdal_merge
-  gdal_edit
-  gdal_calc
-  ogrmerge

For the full list see
:ref:`Programs <programs>` and note the ones that end in ``.py``.

Developers
----------

Read the gdal-utils project charter, `RFC-78 <https://gdal.org/development/rfc/rfc78_gdal_utils_package.html>`__.

Clone or download the gdal project: https://github.com/OSGeo/gdal/

In your IDE set gdal-utils as the root folder,
`.../swig/python/gdal-utils <https://github.com/OSGeo/gdal/tree/master/swig/python/gdal-utils/>`__.

   **./osgeo_utils** - contains the Programs (those scripts that have
   launch wrappers created by pip and added to PYTHONHOME/Scripts)

   **./osgeo_utils/samples** - working python scripts but not typically
   avaialble in path (run them with ``python
   path/to/samples/something.py``)

Discuss via `gdal-dev mailing
list <https://lists.osgeo.org/mailman/listinfo/gdal-dev>`__.

Improve the docs by editing the RST pages in
`.../doc/source <https://github.com/OSGeo/gdal/tree/master/doc/source>`__
which generate the web pages:

-  https://gdal.org/api/index.html#python-api
-  https://gdal.org/programs/index.html#programs
-  https://gdal.org/api/python_samples.html

Contribute changes with `Pull
Requests <https://github.com/OSGeo/gdal/pulls>`__ from your fork to main
GDAL project and use *gdal-utils* label.
