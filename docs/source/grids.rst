.. _grids:

================================================================================
Grids
================================================================================

.. contents:: Contents
   :depth: 3
   :backlinks: none


Grid files are important for shifting and transforming between datums

US, Canadian, French and New Zealand
--------------------------------------------------------------------------------

* http://download.osgeo.org/proj/proj-datumgrid-1.5.zip: US, Canadian, French
  and New Zealand datum shift grids - unzip in the `nad` directory before
  configuring to add NAD27/NAD83 and NZGD49 datum conversion

Switzerland
--------------------------------------------------------------------------------

Background in ticket `#145 <https://github.com/OSGeo/proj.4/issues/145>`__

We basically have two shift grids available. An official here:

`Swiss CHENyx06 dataset in NTv2 format <https://shop.swisstopo.admin.ch/en/products/geo_software/GIS_info>`__

And a derived in a temporary location which is probably going to disappear soon.

Main problem seems to be there's no mention of distributivity of the grid from
the official website.  It just tells: "you can use freely".  The "contact" link
is also broken, but maybe someone could make a phone call to ask for rephrasing
that.

HARN
--------------------------------------------------------------------------------

With the support of `i-cubed <http://www.i-cubed.com>`__, Frank Warmerdam has
written tools to translate the HPGN grids from NOAA/NGS from ``.los/.las`` format
into NTv2 format for convenient use with PROJ.4.  This project included
implementing a `.los/.las reader <https://github.com/OSGeo/gdal/tree/trunk/gdal/frmts/raw/loslasdataset.cpp>`__
for GDAL, and an `NTv2 reader/writer <https://github.com/OSGeo/gdal/tree/trunk/gdal/frmts/raw/ntv2dataset.cpp>`__.
Also, a script to do the bulk translation was implemented in
https://github.com/OSGeo/gdal/tree/trunk/gdal/swig/python/samples/loslas2ntv2.py.
The command to do the translation was:

::

    loslas2ntv2.py -auto *hpgn.los

As GDAL uses NAD83/WGS84 as a pivot datum, the sense of the HPGN datum shift offsets were negated to map from HPGN to NAD83 instead of the other way.  The files can be used with PROJ.4 like this:

::

      cs2cs +proj=latlong +datum=NAD83
            +to +proj=latlong +nadgrids=./azhpgn.gsb +ellps=GRS80

::

    # input:
    -112 34

::

    # output:
    111d59'59.996"W 34d0'0.006"N -0.000

This was confirmed against the `NGS HPGN calculator
<http://www.ngs.noaa.gov/cgi-bin/nadcon2.prl>`__.

The grids are available at http://download.osgeo.org/proj/hpgn_ntv2.zip

.. seealso::
    :ref:`htpd` describes similar grid shifting

HTDP
--------------------------------------------------------------------------------

:ref:`htpd` describes the situation with HTDP grids based on NOAA/NGS HTDP Model.


Non-Free Grids
--------------------------------------------------------------------------------

Not all grid shift files have licensing that allows them to be freely
distributed, but can be obtained by users through free and legal methods.

Canada NTv2.0
................................................................................
Although NTv1 grid shifts are provided freely with PROJ.4, the higher-quality
NTv2.0 file needs to be downloaded from Natural Resources Canada. More info:
http://www.geod.nrcan.gc.ca/tools-outils/ntv2_e.php.

Procedure:

1. Visit the `NTv2 <http://webapp.geod.nrcan.gc.ca/geod/tools-outils/applications.php?locale=en#ntv2>`__, and register/login
2. Follow the Download NTv2 link near the bottom of the page.
3. Unzip `ntv2_100325.zip` (or similar), and move the grid shift file `NTV2_0.GSB` to the proj directory (be sure to change the name to lowercase for consistency)
   * e.g.: `mv NTV2_0.GSB /usr/local/share/proj/ntv2_0.gsb`
4. Test it using:
    ::

        cs2cs +proj=latlong +ellps=clrk66 +nadgrids=@ntv2_0.gsb +to +proj=latlong +ellps=GRS80 +datum=NAD83
        -111 50

    ::

        111d0'3.006"W   50d0'0.103"N 0.000  # correct answer

Australia
................................................................................

`Geocentric Datum of Australia AGD66/AGD84 <http://www.icsm.gov.au/gda/tech.html>`__

Canada
................................................................................

`Canadian NTv2 grid shift binary <http://open.canada.ca/data/en/dataset/b3534942-31ea-59cf-bcc3-f8dc4875081a>`__ for NAD27 <=> NAD83.

Germany
................................................................................

`German BeTA2007 DHDN GK3 => ETRS89/UTM <http://crs.bkg.bund.de/crseu/crs/descrtrans/BeTA/de_dhdn2etrs_beta.php>`__

Great Britain
................................................................................

`Great Britain's OSTN02_NTv2: OSGB 1936 => ETRS89 <http://www.ordnancesurvey.co.uk/business-and-government/help-and-support/navigation-technology/os-net/ostn02-ntv2-format.html>`__

Austria
................................................................................

`Austrian Grid <http://www.bev.gv.at/portal/page?_pageid=713,2204753&_dad=portal&_schema=PORTAL>`__ for MGI

Spain
................................................................................

`Spanish grids <http://www.ign.es/ign/layoutIn/herramientas.do#DATUM>`__ for ED50.

Portugal
................................................................................

`Portuguese grids <http://www.fc.up.pt/pessoas/jagoncal/coordenadas/index.htm>`__ for ED50, Lisbon 1890, Lisbon 1937 and Datum 73

Brazil
................................................................................

`Brazilian grids <http://www.ibge.gov.br/home/geociencias/geodesia/param_transf/default_param_transf.shtm>`__ for datums Corrego Alegre 1961, Corrego Alegre 1970-72, SAD69 and SAD69(96)

South Africa
................................................................................

`South African grid <http://eepublishers.co.za/article/datum-transformations-using-the-ntv2-grid.html>`__ (Cape to Hartebeesthoek94 or WGS84)

Netherlands
................................................................................

`Dutch grid <https://www.kadaster.nl/web/Themas/Registraties/Rijksdriehoeksmeting/Transformatie-van-coordinaten.htm>`__ (Registration required before download)

Hungary
................................................................................

`Hungarian grid <https://github.com/OSGeoLabBp/eov2etrs/>`__ ETRS89 - HD72/EOV (epsg:23700), both horizontal and elevation grids


