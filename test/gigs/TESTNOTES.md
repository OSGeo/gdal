## TEST JSON ###########################################################

This is the testing framework that uses JSON formated files that tests
PROJ.4 using Geospatial Integrity of Geoscience Sofware (GIGS) calculations.  
This could be used as a framework for testing projections from other
sources.


For more information about the Geospatial Integrity of Geoscience Software
(GIGS) at 
http://www.iogp.org/Geomatics#2521115-gigs

## Geospatial Integrity of Geoscience Software License #################

The disclaimer and copyright **only** applies to JSON files that originate
from GIGS tests, which is a reformatting material provided by the
International Association of Oil & Gas Producers.

**Disclaimer**

Whilst every effort has been made to ensure the accuracy of the information contained in this publication,
neither the OGP nor any of its members past present or future warrants its accuracy or will, regardless
of its or their negligence, assume liability for any foreseeable or unforeseeable use made thereof, which
liability is hereby excluded. Consequently, such use is at the recipient’s own risk on the basis that any use
by the recipient constitutes agreement to the terms of this disclaimer. The recipient is obliged to inform
any subsequent recipient of such terms.

This document may provide guidance supplemental to the requirements of local legislation. Nothing
herein, however, is intended to replace, amend, supersede or otherwise depart from such requirements. In
the event of any conflict or contradiction between the provisions of this document and local legislation,
applicable laws shall prevail.

**Copyright notice**

The contents of these pages are © The International Association of Oil & Gas Producers. Permission
is given to reproduce this report in whole or in part provided (i) that the copyright of OGP and (ii)
the source are acknowledged. All other rights are reserved.” Any other use requires the prior written
permission of the OGP.

These Terms and Conditions shall be governed by and construed in accordance with the laws of
England and Wales. Disputes arising here from shall be exclusively subject to the jurisdiction of the
courts of England and Wales.


## INSTALLING ##########################################################

 * Requires: Python 2.7 or 3.3+
 * pyproj (optional but highly recommended), this speeds up tests, makes
    results more precise but has the trade-off of making installation a
    little more complicated.



### Installing pyproj ##################################################

 1) Install `pip` (usually `pip3`) if not installed, should already be installed on new Python 
        versions (Python 3 >=3.4) or if using a virtual environment for python.
        see https://pip.pypa.io/en/stable/installing/
        * **Note**: if you have Python 2.x and 3.x installed,  `pip3` is for Python 3.x.
            `pip` could be an alias for either one.
 2) Upgrade `pip` (possibly not needed)
        https://pip.pypa.io/en/stable/installing/#upgrading-pip
 3) Install pyproj
    * requires a C/C++ compiler be usuable by python
    * repository version requires Cython to be installed, releases do not require this

```
         $ pip install cython
```

    * install latest release
        * set PROJ_DIR environment variable to an installed version of PROJ4
          library.  This should have the directories include/  lib/ & 
          share/proj/ underneath it.

    * installing on Linux  (default ./configure settings for PROJ.4)

```
         $ PROJ_DIR=/usr/local pip install pyproj
```

## Running Tests #######################################################

When calling test_json.py it defaults to using pyproj driver.

```
    $ python test_json.py 5*.json
```

There is a driver to directly use cs2cs instead of installing pyproj, but
it is much slower and not recommended.

Here is how you run the cs2cs driver:

```
    $ python test_json.py -d cs2cs -e path/to/repo/bin/cs2cs 5201.json
```


## GIGS Test ###########################################################

Tests that were meant to test for out of grid errors were removed from
testing.  PROJ.4 cs2cs provides coordinates seemingly without warnings
or errors.

### Drastic Tolerance Errors ###########################################

These are errors that might indicate the wrong projection is being used, 
wrong parameters are being used in the projection, or problems with the 
projection itself.  It could point to problems with the provided model.

 * 5101 part 4 - Transverse Mercator
    - EPSG code will need to be redefined to make sure that etmerc version
      is used, should be done before next release (4.9.3 / 4.10.0 ?)
    - Temporarily use test "5101.4-jhs-etmerc.json", perhaps remove file
      when etmerc/tmerc aliasing issue has been fixed.
 * 5102 part 2 -  Lambert Conic Conformal (1SP)
    - This one seems to have some problems.
 * 5105 part 1 - Oblique Mercator (variant B)
    - There are some drastically different answers.

### Slight Tolerance Errors ############################################

These tests have results with rounding errors that are slightly out of
tolerance.   This could be due to a lack of precision in the JSON file
or due to the formula's precision.  Most of these tests fail with some
point in testing 1,000 round trip coordinate conversions.  A few of these
might require a little bit of tuning with cs2cs.  There are some rather
concerning differences between Python 2.7 and Python 3.4 in testing, which
needs to be pinpointed.
 
 * 5101 part 1 - Transverse Mercator
    - roundtrip tests fail with very slight tolerance issues
 * 5104 - Oblique stereographic
    - roundtrip tests fail with very slight tolerance issues
 * 5105 part 2 - Oblique Mercator (variant B)
    - roundtrip tests fail with tolerance issues
 * 5106 - Hotline Oblique Mercator (variant A)
    - roundtrip tests fail with very slight tolerance issues
 * 5108 - Cassini-Soldner
    - roundtrip tests seem to accumulate errors
 * 5110 - Lambert Azimuthal Equal Area
    - roundtrip tests have some slight errors
 * 5111 part 1 - Mercator (variant A)
    - roundtrip tests fail with very slight tolerance issues
 * 5111 part 2 - Mercator (variant A)
 * 5203 part 1 - Position Vector 7-parameter transformation
    - most seem to be rounding errors.  Some results cross longitude
      line 180/-180 
 * 5204 part 1 - Coordinate Frame 7-parameter transformation
    - most seem to be rounding errors.  Some results cross longitude 
      line 180/-180
 * 5205 part 1 - Molodensky-Badekas 10-parameter transformation
    - most seem to be rounding errors.  Some results cross longitude
      line 180/-180
 * 5206 - NADCON transformation
 * 5207 parts 1 & 2 - NTv2 transformation


### Other Issues with Tests ############################################
 * 5201 - Geographic Geocentric conversions
    - EPSG code 4979, does not exist in PROJ.4 substituted EPGS code 4326.
    - The test passes.
 * 5206 and 5207 parts 1 & 2 - NADCON Transformation and NTv2 Transformation
    - These tests have cases that are out of grid, which have been omitted.
      The GIGS tests expectations, "[n]ote 1: This location is out of
      transformation grid area - the attempted transformation should fail
      and application notify user."


## Passing Tests #######################################################
 * 5101 part 2 - Transverse Mercator
 * 5101 part 3 - Transverse Mercator
 * 5102 part 1 - Lambert Conic Conformal (1SP)
 * 5103 part 1 - Lambert Conic Conformal (2SP)
 * 5103 part 2 - Lambert Conic Conformal (2SP)
 * 5103 part 3 - Lambert Conic Conformal (2SP) 
 * 5107 - American Polyconic
 * 5109 - Albers Equal Area
 * 5112 - Mercator (variant B)
 * 5113 - Transverse Mercator (South Oriented)
 * 5208 - Longitude Rotation



## Benchmarks ##########################################################

Benchmarks were made using Micah Cochran's circa 2008 Desktop computer 
using LXLE Ubuntu Linux 14.04.  The pattern "5*.json" was used for testing.

This is the computer time used for the process, not the actual run time.
 * pyproj driver testing 5 seconds
 * cs2cs driver testing - using Python 2.7.6 - 4 min 36 seconds  
 * cs2cs driver testing - using Python 3.4.3 - 6 min 23 seconds  



## Random Notes #######################################################
Roundtrip testing has been fixed, it now checks both resulting coordinates.

Some tests in the 5100/5200 series have not been converted.  The most of
the 3d coordinate test in the 5200 series have not been converted.

This is designed to use decimal degrees over Sexagesimal degree or degree
minutes seconds, which is a decision that might need to be revisited.
Decimal degrees were chosen because it was easier interface with the
Python pyproj library.  This might be the cause of some testing tolerance
issues.

pyproj and cs2cs drivers do not quite work the same.  Different tests
fail depending on the driver.  

Other drivers could be written to interface with other code.

There could be some precision issues with cs2cs, perhaps some adjustment
of the "-f format" parameter could help.  This could be done based either
on expected output or extra info from the JSON file.

A TODO list is located source code.

Conversion tests the output coordinate, causing 2 test results per
coordinate pair.

Roundtrip tests the input coordinate and the output coordinate of each
 pair, causing 4 test results per coordinate pair.
