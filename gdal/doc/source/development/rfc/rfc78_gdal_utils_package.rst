.. _rfc-78:

===========================================
RFC 78: gdal-utils package
===========================================

============== ============================
Author:        Idan Miara
Contact:       idan@miara.com
Started:       2020-Dec-3
Last updated:  2021-March-26
Status:        Adopted, implemented in GDAL 3.3
============== ============================

Summary
-------

This RFC suggests to put all the GDAL python modules (formly scripts), except from the GDAL core SWIG bindings,
into their own distribution on pypi.
The GDAL python sub-package `osgeo.utils` (introduced in GDAL 3.2) would be renamed into a package named `osgeo_utils`.

The standalone python scripts from GDAL <= 3.1 were transformed to `osgeo.utils` in GDAL 3.2.
For backwards compatibility these scripts still exist and function as tiny wrappers around the python modules.
Users of these scripts would not be effected from this RFC as the scripts would continue to function in GDAL 3.3
in the same way as in GDAL <= 3.2.

To allow maximum backwards compatibility, The `osgeo` package (which includes the GDAL core SWIG bindings)
and the `osgeo_utils` package will continue to be distributed in a single `sdist` named `gdal` in `pypi`.

In addition, a new pure python `wheel` distribution named `gdal-utils` will be available in `pypi` under the name
`gdal-utils`.

This will allow users who wish to upgrade the utils without upgrading the bindings to do so with
`pip install --upgrade gdal-utils` (see more details in the following sections).

Motivation
----------

Making gdal Python developers life easier on Windows (and maybe other platforms):
    The straightforward way of cloning `gdal` and adding `gdal/swig/python` to Python path
    (In PyCharm: marking the it as a `Source Root`) won't work because the `pyc` files are missing from
    `gdal/swig/python/osgeo`, thus by adding `osgeo` to Python path we would be masking a binary installation of gdal
    that might be already installed (i.e. `osgeo4w` or Christoph Gohlke's binary Windows wheels).
    Workarounds, like copying the `pyc` files to the `osgeo` dir, cause their own problems, like:
    * Switching interpreters that have different versions of `gdal` causes more problems.
    * Non clean git working tree so committing the changes to git is harder.
    By moving the `gdal-utils` into another root this problem is completely avoid.

Allow mixing `gdal` and `gdal-utils` versions
    As the Python code evolves semi-independently of the GDAL core and is not directly dependent on a specific
    GDAL version, one might want use the latest `gdal-utils` package with an older version of `gdal` core bindings,
    or vice versa. Currently, One would need to mix the contents different `gdal` packages to do so.

    As the `gdal` package is platform specific, and requires compilation, In some distributions upgrading
    to a new GDAL version might take more time, so one could upgrade the `gdal-utils` package easily
    and independently of upgrading the `gdal` package, using `pip install --upgrade gdal-utils`.

    On the other hand, if we drop another Python version, like we just did with Python 2.7,
    One might be still be able to use newer gdal core bindings (which might still support an older version of Python)
    with an older `gdal-utils`, Which might drop Python versions sooner.

    Reasons why users wouldn't be able to upgrade to recent GDAL:

    * They use some LTS distribution (like Debian), or application (like QGIS LTS).
    * Recent GDAL is not available for their platform or distribution (even if not LTS).
    * Their code dependents on some binary gdal-plugin which is available for a specific GDAL version.

    A concrete example would be someone who uses QGIS (or QGIS LTS) that currently comes with GDAL 3.1,
    and wants to use the neat stuff added to `gdal_calc`, With this RFC he could just `pip install --upgrade gdal-utils`.
    Because this package package is Pure Python, upgrades should be easy.

    Although each version of `gdal-utils` would only be tested against the equivalent version of `gdal`,
    In most cases different versions would still be compatible, but without guarantee.

    This RFC would make further testing the compatibility between different versions of these packages easier and
    could be considered the first step into making the `gdal-utils` package completely independent of the `gdal` package.
    It would also potentially allow forward compatibility in case we ever decide to remove the utils from the `gdal` wheel
    and keep them only in the `gdal-utils` wheel.


Package Names and PyPi release:
----------------------------------

This RFC suggests to keep the all-in-one package and in addition to distribute a utils only package,
In order to keep the required changes from users to a minimum and at the same time allow mixing versions:

* https://pypi.org/project/GDAL/

Distribution of a single `pypi.org` wheel named `gdal` will be retained and will includes both packages.
This will insure smooth transition and maximum backwards compatibility.

* https://pypi.org/project/gdal-utils/

A new `pypi.org` package is introduced to include with just the `osgeo_utils` package, which will allow
upgrading the utils without upgrading the bindings.

To be consistent with the `gdal` `pypi` package name, the utils `pypi` package is named `gdal-utils`.

the name `osgeo_utils` is consistent with the `osgeo` namespace and module names.

.. code-block::

    pip install gdal
    pip install gdal-utils

.. code-block:: Python

    from osgeo import gdal
    from osgeo_utils import gdal_calc, ogr_foo, osr_bar



How to upgrade the utils without upgrading the bindings:
----------------------------------------------------------

> If someone installs the "gdal" all-in-one package and the "gdal-utils" one. Wouldn't that conflict ?

`pip install` a wheel overwrites whichever files already exist (even if installed by a different package)
If you `pip install gdal` then `pip install gdal-utils` you'd get the utils from `gdal-utils`.
If later you do again `pip install gdal` with a different version then you'd get the utils from `gdal` again, and so on.
(it doesn't seem that it matters which version is a bigger number, just which one you installed later)

If you `pip install gdal` then `pip install gdal-utils` and then `pip uninstall gdal-utils` then the
utils would be uninstalled and you'd be left with gdal without utils.
Then you could `pip install gdal-utils` or `pip install gdal --ignore-installed` to get them back again
(`--ignore-installed` is not required if you install a different version)

Limitations and scope
---------------------

The scope of this RFC is the GDAL Python code, except for the SWIG bindings.
There is no effect on any other language supported by GDAL.
Because gdal core is tested using the Python SWIG bindings - this RFC does not suggest changing them in any way.
Binary wheel distribution - discussed in the past and related to ideas in this RFC.


`gdal` and `gdal-utils` Compatibility
----------------------------------------

This RFC suggests that `gdal-utils` would continue to be only tested against the same version of `gdal`.
In most cases different versions would still be compatible, but without guarantee.

A minimum energy approach might keep `gdal-utils` compatible with some `gdal` versions != `x.y`.
`gdal-utils` might officially drop support of some too old version of GDAL
by specifying a newer minimum version of GDAL in the `setup.py` of `gdal-utils`.

For maximum backwards compatibility and because we would only test `gdal` against the same version of `gdal-utils` -
`osgeo` and `osgeo_utils` will continue to be distributed inside a single wheel in addition
to the new separate wheel for the utils only.

In cases were an `gdal-utils` module or function actually does need a minimum specific version of `gdal`
(i.e. dependence on a new GDAL C API) Compatibility could be checked at runtime by comparing to `osgeo.__version__`.

Versioning
------------

As development of `gdal-utils` will be still tied with the development of GDAL and will be
released together with the same `x.y.z` version number.
In case a hotfix to `gdal-utils` is required for some reason, a `x.y.z.p` version might be used,
Which will not effect the distribution of version `z+1`, i.e. `3.3.0` < `3.3.0.1` < `3.3.1`.


Backward compatibility issues:
--------------------------------

* `osgeo.utils` will need be replaced with `osgeo_utils`
    This is the only breaking change, only for GDAL=3.2, and only a single character.
* `swig/python/scripts` - users of the gdal scripts (which are thin wrappers around the utils) wouldn't be effected.


Folder structure change
--------------------------

* `gdal/swig/python/osgeo/utils` -> `gdal/swig/python/gdal-utils/osgeo_utils`

* `gdal/swig/python/osgeo/setup.py` - was updated to include the utils from the new location under the `gdal-utils` folder.

* `gdal/swig/python/gdal-utils/setup.py` - additional setup was added for `gdal-utils`.

CI Impacts:
------------

`gdal-utils` wheel building could be added to the CI, i.e. like in https://github.com/OSGeo/gdal/pull/3579
No other CI Impacts.

Impacts on GDAL core
--------------------

None.

SWIG binding changes
--------------------

None.

Security implications
---------------------

None.

Performance impact
------------------

None.

Documentation
-------------

Implications of this change shell documented in the README.

Testing
-------

Minor changes were made to pytest.

Previous discussions
--------------------

This topic has been discussed in the past in :

- http://osgeo-org.1560.x6.nabble.com/gdal-dev-Call-for-discussion-on-RFC77-Drop-Python-2-support-td5449659.html

Related PRs:
-------------

- https://gdal.org/development/rfc/rfc77_drop_python2_support.html
- https://github.com/OSGeo/gdal/pull/3131
- https://github.com/OSGeo/gdal/pull/3117
- https://github.com/OSGeo/gdal/pull/3247

Voting history
--------------

- http://osgeo-org.1560.x6.nabble.com/gdal-dev-Motion-RFC-78-gdal-utils-package-td5482707.html

* +1 from EvenR, HowardB
* +0 from KurtS, JukkaR
* -0 from SeanG

Credits
-------

* Implemented by the author of this RFC, Idan Miara.
