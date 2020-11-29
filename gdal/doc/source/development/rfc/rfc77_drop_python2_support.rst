.. _rfc-77:

================================================================================
RFC 77: Drop Python 2 support in favour of Python 3.6
================================================================================

============== ============================
Author:        Idan Miara
Contact:       idan@miara.com
Started:       2020-Nov-3
Last updated:  2020-Nov-29
Status:        Adopted, implemented in GDAL 3.3
============== ============================

Summary
-------

This RFC drops Python 2 support and sets Python 3.6 as the new minimum supported Python version.

Motivation
----------

Currently GDAL Python bindings support Python 2.7 and Python 3 (so only the common between the two).
Python 2 is at End Of Life, and is no longer supported since the January 2020.
https://www.python.org/doc/sunset-python-2/

"We did not want to hurt the people using Python 2. So, in 2008, we announced that we would sunset Python 2 in 2015,
and asked people to upgrade before then. Some did, but many did not. So, in 2014, we extended that sunset till 2020."

While keeping Python 2.7 support might serve those who didn't upgrade their code to Python 3 in the 12 year transition period,
This PR suggests that the time has come and the benefits of dropping Python 2 support outnumber the drawbacks.
Virtually all supported OS and relevant programs already use Python 3.
Moreover, most of the related projects that usually is used with GDAL already dropped Python 2 support (as can be seen below).
It makes sense that people who didn't upgrade their code in 12 years are still using a much older version of GDAL anyway...

The drawbacks of keeping Python 2 support puts unnecessary maintenance burden on the GDAL maintainers,
As maintainers need to make sure their new code is backwards compatible with Python 2.
Furthermore, many important features that added in Python 3 cannot be used in GDAL to maintain backwards compatibility with Python 2.

Related projects that dropped Python 2 support:
-------------------------------------------------

* QGIS since v3.0 - February 2018. (now supports Python 3.7)

https://www.qgis.org/en/site/forusers/visualchangelog30/index.html
https://qgis.org/api/api_break.html

* GRASS GIS since v7.8 - September 2019 (now supports Python 3.7)

https://trac.osgeo.org/grass/wiki/Python3Support

* pyproj since v2.3 - August 2019 (now supports Python 3.5-3.7)

https://pyproj4.github.io/pyproj/stable/history.html

* numpy since v1.19 - June 2020 (now supports Python Python 3.6-3.8)

https://numpy.org/devdocs/release/1.19.0-notes.html

* RasterIO since v1.1 - October 2019 (now supports Python 3.6)

https://sgillies.net/2019/10/10/rasterio-1-1-0.html
https://github.com/mapbox/rasterio/issues/1813

Python 3 version status:
---------------------------

* Python 3.5 (Released: Sep 2015) is End Of Life. Many related projects already dropped support of it.
* Python 3.6 (Released: Dec 2016) will be End Of Life on Dec 2021. Python 3.6 brings many important features.
* Python 3.7 (Released: Jun 2018) will be End Of Life on Jun 2023. Python 3.7 is already widely adopted and supported by all the related projects listed above.
* Python 3.8 (Released: Oct 2019) will be End Of Life on Oct 2024. Python 3.8 is too new to be set as the minimum supported version.

https://endoflife.date/python#:~:text=The%20support%20for%20Python%202.7,dropping%20support%20for%20Python%202.7.

Python 3 OS Support:
----------------------

Linux:
++++++++

======================= =============== ===============  ===============================
Distribution            Release date     End Of Life     Python Version
Ubuntu 16.04 Xenial LTS April 2016       April 2021      Python 3.5
Ubuntu 18.04 Bionic LTS April 2018       April 2023      Python 3.6 (3.7 in universe)
Ubuntu 20.04 Xenial LTS April 2020       April 2021      Python 3.8
Debian  9.0 Stretch LTS June 2017        July 2022       Python 3.5
Debian 10.0 Buster LTS  July 2019        ~2022           Python 3.7
Centos/RHEL 8           September 2019   ?               Python 3.6 (?)
Amazon Linux                             December 2021   Python 3.6
======================= =============== ===============  ===============================

https://wiki.python.org/moin/Python3LinuxDistroPortingStatus


Windows:
+++++++++

* Conda: Python 3.6-3.9
* OSGeo4W: Python 3.7
* gisinternals: Python 2.7, 3.4-3.7

MacOS:
+++++++

It appears the even though Python 2.7 is preinstalled on Mac OS X,
Python 3.5-3.9 can be installed alongside Python 2 on Mac OS X 10.8 (Released: July 2012) and newer.

https://docs.python.org/3.6/using/mac.html.

Which version should be the new minimum version ?
-----------------------------------------------------

This RFC suggest the new minimum supported Python version should be 3.6 for the following reasons:

* Python < 3.6 is End of Life.
* Long list of great new features that were introduced in Python 3.6, Several of which are immediately useful to simplify code or improve testing:
    * f-strings
    * builtin pathlib
    * underscores in numeric literals
    * type annotations
    * malloc debugging
* Python 3.6 is supported out of the box in virtually every relevant OS.
* Python 3.6 is probably the safest choice for now in respect to other related projects.
* Python 3.7 (and newer) isn't available seamlessly in some popular LTS Linux distributions.
* We want to make the transition as smooth and easy as possible. Setting the minimum to Python 3.7 might make the transition harder for the CI because of the above reason.
* Dropping Python 3.6 in favour of Python 3.7 or newer in future versions shouldn't be as hard as this drop (see next section for a suggested approach).

GDAL Release cycle and regular Python version dropping
++++++++++++++++++++++++++++++++++++++++++++++++++++++++

When releasing GDAL 3.1.0, Even Rouault suggested GDAL would use fixed release cycles of 6 months between major versions:

http://osgeo-org.1560.x6.nabble.com/gdal-dev-Reconsidering-release-cycle-length-td5436163.html#a5436242

Projecting from that suggestion, GDAL 3.3.0 should be released around April-May 2021.

We could potentially synchronize with NEP 29 -
Recommend Python and Numpy version support as a community policy standard.
Which suggests when to drop each Python version.

https://numpy.org/neps/nep-0029-deprecation_policy.html

NEP 29 suggests to drop support for Python 3.6 support on Jun 23, 2020 (in favour of Python 3.7).

We could potentially discuss similar/more conservative approaches and delay each drop by a few more months,
or only drop Python versions that have reached End Of Life (As of today, Python < 3.6 have reached End Of Life).
Further discussion on the matter of dropping other Python versions is a subject for another RFC.

Backward compatibility
----------------------

Currently, GDAL Python code itself is compatible with Python 2 and Python 3.
Once this PR is accepted, GDAL 3.3.0 would not be compatible with Python 2.
Thus any "Python 2 only" code that uses GDAL would need to be upgraded to Python 3 and
at the same time the respective Python interpreter would need to be upgraded
to a supported Python version.

Will GDAL 3.2 be a LTS?
++++++++++++++++++++++++++

Currently - No.
So far, nobody has stepped up to make a LTS, So there won't be one unless someone takes it up upon themselves or raise funds to make it happen.
GDAL only provide bugfix releases of the current stable branch for 6 months.

CI Impacts:
------------

Impacts on our CI should be analyzed.
It seems that all our CI builds use Python 2.7 or 3.5, so all of them would need to be adjusted.
In particular, builds that use older Linux distributions would need to be upgraded.

Impacts on GDAL core
--------------------

There should be no impacts on GDAL core,
As the Python bindings are generated by SWIG on top of the binary form of GDAL.

Limitations and scope
---------------------

The scope of this RFC should be the GDAL Python code alone. There shouldn't be effect on any other language supported by GDAL.

SWIG binding changes
--------------------

To begin with, the SWIG Python bindings already support Python 3.6.
Dropping Python 2 support might allow us to use a newer SWIG version or to make some improvements to the bindings,
but it doesn't have to be in the first step.

Security implications
---------------------

Python 3.6 is the minimum Python version that is not End Of Life,
thus still receiving security updates.

Performance impact
------------------

There might be some performance gain for this upgrade for some uses as there were many performance improvements between Python 2.7-3.6.
The scope of the improvements could be limited because most of GDAL Python code is a thin wrapper around the C++ code.

Documentation
-------------

The GDAL Python documentation is generated automatically in should already support Python 3.
If there are sections in the documentation that are Python 2 specific, they should be removed or refactored.

Testing
-------

While upgrading the CI, Python 2 tests should be removed or upgraded.
A simple test that fails on Python < 3.6 should be added.
No any additional tests should be needed.

Previous discussions
--------------------

This topic has been discussed in the past in :

- https://github.com/OSGeo/gdal/issues/3114
- https://github.com/OSGeo/gdal/pull/3142

Related PRs:
-------------

Adding a deprecation warning if running a Python version that is known to be unsupported in the the next GDAL version:

- https://github.com/OSGeo/gdal/pull/3165

Voting history
--------------

https://lists.osgeo.org/pipermail/gdal-dev/2020-November/053039.html

* +1 from EvenR, HowardB, KurtS, JukkaR, DanielM

Credits
-------

* implemented by Even Rouault, Robert Coup and Idan Miara
