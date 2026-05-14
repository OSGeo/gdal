#!/usr/bin/env pytest
###############################################################################
#
# Project:  GDAL/OGR Test Suite
# Purpose:  Test some MITAB specific translation issues.
# Author:   Even Rouault, <even dot rouault at spatialys.com>
#
###############################################################################
# Copyright (c) 2010, Even Rouault <even dot rouault at spatialys.com>
#
# SPDX-License-Identifier: MIT
###############################################################################


import pytest

from osgeo import osr

###############################################################################
# Test the osr.SpatialReference.ImportFromMICoordSys() function.
#


def test_osr_micoordsys_1():

    srs = osr.SpatialReference()
    srs.ImportFromMICoordSys(
        'Earth Projection 3, 62, "m", -117.474542888889, 33.7644620277778, 33.9036340277778, 33.6252900277778, 0, 0'
    )

    if (
        srs.GetProjParm(osr.SRS_PP_STANDARD_PARALLEL_1)
        != pytest.approx(33.9036340277778, abs=0.0000005)
        or srs.GetProjParm(osr.SRS_PP_STANDARD_PARALLEL_2)
        != pytest.approx(33.6252900277778, abs=0.0000005)
        or srs.GetProjParm(osr.SRS_PP_LATITUDE_OF_ORIGIN)
        != pytest.approx(33.7644620277778, abs=0.0000005)
        or srs.GetProjParm(osr.SRS_PP_CENTRAL_MERIDIAN)
        != pytest.approx((-117.474542888889), abs=0.0000005)
        or srs.GetProjParm(osr.SRS_PP_FALSE_EASTING)
        != pytest.approx(0.0, abs=0.0000005)
        or srs.GetProjParm(osr.SRS_PP_FALSE_NORTHING)
        != pytest.approx(0.0, abs=0.0000005)
    ):
        print(srs.ExportToPrettyWkt())
        pytest.fail("Can not export Lambert Conformal Conic projection.")


###############################################################################
# Test the osr.SpatialReference.ExportToMICoordSys() function.
#


def test_osr_micoordsys_2():

    srs = osr.SpatialReference()
    srs.ImportFromWkt("""PROJCS["unnamed",GEOGCS["NAD27",\
    DATUM["North_American_Datum_1927",\
    SPHEROID["Clarke 1866",6378206.4,294.9786982139006,\
    AUTHORITY["EPSG","7008"]],AUTHORITY["EPSG","6267"]],\
    PRIMEM["Greenwich",0],UNIT["degree",0.0174532925199433],\
    AUTHORITY["EPSG","4267"]],PROJECTION["Lambert_Conformal_Conic_2SP"],\
    PARAMETER["standard_parallel_1",33.90363402777778],\
    PARAMETER["standard_parallel_2",33.62529002777778],\
    PARAMETER["latitude_of_origin",33.76446202777777],\
    PARAMETER["central_meridian",-117.4745428888889],\
    PARAMETER["false_easting",0],PARAMETER["false_northing",0],\
    UNIT["metre",1,AUTHORITY["EPSG","9001"]]]""")

    proj = srs.ExportToMICoordSys()

    assert (
        proj
        == 'Earth Projection 3, 62, "m", -117.474542888889, 33.7644620277778, 33.9036340277778, 33.6252900277778, 0, 0'
    ), "Can not import Lambert Conformal Conic projection."


###############################################################################
# Test EPSG:3857
#


def test_osr_micoordsys_3():

    srs = osr.SpatialReference()
    srs.ImportFromEPSG(3857)

    proj = srs.ExportToMICoordSys()

    assert proj == 'Earth Projection 10, 157, "m", 0'

    srs = osr.SpatialReference()
    srs.ImportFromMICoordSys('Earth Projection 10, 157, "m", 0')
    wkt = srs.ExportToWkt()
    assert 'EXTENSION["PROJ4"' in wkt

    # Transform again to MITAB (we no longer have the EPSG code, so we rely on PROJ4 extension node)
    proj = srs.ExportToMICoordSys()

    assert proj == 'Earth Projection 10, 157, "m", 0'
