.. _rfc-36:

================================================================================
RFC 36: Allow specification of intended driver on GDALOpen
================================================================================

Authors: Ivan Lucena

Contact: ivan.lucena@pmldnet.com

Status: Proposed

Summary
-------

This document proposes a mechanism to explicitly tell GDAL what driver
should open a particular dataset.

Justification
-------------

By selecting the driver, users can optimize processing time and avoid
incorrect or undesirable driver selection due to the driver probing
mechanism.

Concept
-------

The idea is to pass to GDALOpen a string containing the token "driver="
followed by the driver name and a comma separating it from the
file-name.

[driver=driver-name,]file-name

Examples:

$ gdalinfo driver=nitf:imagefile01.ntf

In that case no probing is necessary, since the user has indicated to
use the specific driver. If for some reason that process fails the
function returns NULL and no other attempt is made to open the file by
another driver.

Implementation
--------------

The amount of code is minimal and there is already a proposed patch on
ticket #3043.

Utilization
-----------

Any application that uses GDAL API or any GDAL command line tool's user
that, at one point, wants to force the use of a particular driver to
open a datasets.

Backward Compatibility Issues
-----------------------------

That optional entry on GDALOpen process should not affect the current
logic.

Testing
-------

-  Extra tests would be added to the test script

Issues
------

For gdalbuildvrt and gdaltindex it will not be possible to use the
driver selection with wildcard, as in "driver=gtiff,*.tif".
