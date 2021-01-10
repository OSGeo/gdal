#!/usr/bin/env pytest
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR Test Suite
# Purpose:  gdal2tiles.py testing
# Author:   Even Rouault <even dot rouault @ spatialys dot com>
#
###############################################################################
# Copyright (c) 2015, Even Rouault <even dot rouault @ spatialys dot com>
#
# Permission is hereby granted, free of charge, to any person obtaining a
# copy of this software and associated documentation files (the "Software"),
# to deal in the Software without restriction, including without limitation
# the rights to use, copy, modify, merge, publish, distribute, sublicense,
# and/or sell copies of the Software, and to permit persons to whom the
# Software is furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included
# in all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
# OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.
###############################################################################

import os
import sys
import shutil


from osgeo import gdal      # noqa
import test_py_scripts      # noqa  # pylint: disable=E0401
import pytest

def _verify_raster_band_checksums(filename, expected_cs=[]):
    ds = gdal.Open(filename)
    if ds is None:
        pytest.fail('cannot open output file "%s"' % filename)

    num_bands = len(expected_cs)
    for i in range(num_bands):
        if ds.GetRasterBand(i + 1).Checksum() != expected_cs[i]:
            for j in range(num_bands):
                print(ds.GetRasterBand(j + 1).Checksum())
            pytest.fail('wrong checksum for band %d (file %s)' % (i + 1, filename))

    ds = None

def test_gdal2tiles_py_simple():
    script_path = test_py_scripts.get_py_script('gdal2tiles')
    if script_path is None:
        pytest.skip()

    shutil.copy(test_py_scripts.get_data_path('gdrivers')+'small_world.tif', 'tmp/out_gdal2tiles_smallworld.tif')

    os.chdir('tmp')
    test_py_scripts.run_py_script(
        script_path,
        'gdal2tiles',
        '-q out_gdal2tiles_smallworld.tif')
    os.chdir('..')

    os.unlink('tmp/out_gdal2tiles_smallworld.tif')

    _verify_raster_band_checksums(
        'tmp/out_gdal2tiles_smallworld/0/0/0.png',
        expected_cs = [31420, 32522, 16314, 17849]
    )

    for filename in ['googlemaps.html', 'leaflet.html', 'openlayers.html', 'tilemapresource.xml']:
        assert os.path.exists('tmp/out_gdal2tiles_smallworld/' + filename), \
            ('%s missing' % filename)



def test_gdal2tiles_py_zoom_option():

    script_path = test_py_scripts.get_py_script('gdal2tiles')
    if script_path is None:
        pytest.skip()

    shutil.rmtree('tmp/out_gdal2tiles_smallworld', ignore_errors=True)

    # Because of multiprocessing, run as external process, to avoid issues with
    # Ubuntu 12.04 and socket.setdefaulttimeout()
    # as well as on Windows that doesn't manage to fork
    test_py_scripts.run_py_script_as_external_script(
        script_path,
        'gdal2tiles',
        '-q --force-kml --processes=2 -z 0-1 '+test_py_scripts.get_data_path('gdrivers')+'small_world.tif tmp/out_gdal2tiles_smallworld')

    _verify_raster_band_checksums(
        'tmp/out_gdal2tiles_smallworld/1/0/0.png',
        expected_cs = [24063, 23632, 14707, 17849]
    )

    ds = gdal.Open('tmp/out_gdal2tiles_smallworld/doc.kml')
    assert ds is not None, 'did not get kml'


def test_gdal2tiles_py_resampling_option():

    script_path = test_py_scripts.get_py_script('gdal2tiles')
    if script_path is None:
        pytest.skip()

    resampling_list = [
        'average', 'near', 'bilinear', 'cubic', 'cubicspline', 'lanczos',
        'antialias', 'mode', 'max', 'min', 'med', 'q1', 'q3']
    try:
        from PIL import Image
        import numpy
        import osgeo.gdal_array as gdalarray
        del Image, numpy, gdalarray
    except ImportError:
        # 'antialias' resampling is not available
        resampling_list.remove('antialias')

    out_dir = 'tmp/out_gdal2tiles_smallworld'

    for resample in resampling_list:

        shutil.rmtree(out_dir, ignore_errors=True)

        test_py_scripts.run_py_script_as_external_script(
            script_path,
            'gdal2tiles',
            '-q --resampling={0} {1} {2}'.format(
                resample, test_py_scripts.get_data_path('gdrivers')+'small_world.tif', out_dir))

        # very basic check
        ds = gdal.Open('tmp/out_gdal2tiles_smallworld/0/0/0.png')
        if ds is None:
            pytest.fail('resample option {0!r} failed'.format(resample))
        ds = None

    shutil.rmtree(out_dir, ignore_errors=True)


def test_gdal2tiles_py_xyz():
    script_path = test_py_scripts.get_py_script('gdal2tiles')
    if script_path is None:
        pytest.skip()

    try:
        shutil.copy(test_py_scripts.get_data_path('gdrivers')+'small_world.tif', 'tmp/out_gdal2tiles_smallworld_xyz.tif')

        os.chdir('tmp')
        ret = test_py_scripts.run_py_script(
            script_path,
            'gdal2tiles',
            '-q --xyz --zoom=0-1 out_gdal2tiles_smallworld_xyz.tif')
        os.chdir('..')

        assert 'ERROR ret code' not in ret

        os.unlink('tmp/out_gdal2tiles_smallworld_xyz.tif')

        _verify_raster_band_checksums(
            'tmp/out_gdal2tiles_smallworld_xyz/0/0/0.png',
            expected_cs = [31747, 33381, 18447, 17849]
        )
        _verify_raster_band_checksums(
            'tmp/out_gdal2tiles_smallworld_xyz/1/0/0.png',
            expected_cs = [15445, 16942, 13681, 17849]
        )

        for filename in ['googlemaps.html', 'leaflet.html', 'openlayers.html']:
            assert os.path.exists('tmp/out_gdal2tiles_smallworld_xyz/' + filename), \
                ('%s missing' % filename)
        assert not os.path.exists('tmp/out_gdal2tiles_smallworld_xyz/tilemapresource.xml')
    finally:
        shutil.rmtree('tmp/out_gdal2tiles_smallworld_xyz')

def test_gdal2tiles_py_invalid_srs():
    """
    Case where the input image is not georeferenced, i.e. it's missing the SRS info,
    and no --s_srs option is provided. The script should fail validation and terminate.
    """
    script_path = test_py_scripts.get_py_script('gdal2tiles')
    if script_path is None:
        pytest.skip()

    shutil.copy(test_py_scripts.get_data_path('gdrivers')+'test_nosrs.vrt', 'tmp/out_gdal2tiles_test_nosrs.vrt')
    shutil.copy(test_py_scripts.get_data_path('gdrivers')+'byte.tif', 'tmp/byte.tif')

    os.chdir('tmp')
    # try running on image with missing SRS
    ret = test_py_scripts.run_py_script(
        script_path,
        'gdal2tiles',
        '-q --zoom=0-1 out_gdal2tiles_test_nosrs.vrt')

    # this time pass the spatial reference system via cli options
    ret2 = test_py_scripts.run_py_script(
        script_path,
        'gdal2tiles',
        '-q --zoom=0-1 --s_srs EPSG:4326 out_gdal2tiles_test_nosrs.vrt')
    os.chdir('..')

    os.unlink('tmp/out_gdal2tiles_test_nosrs.vrt')
    os.unlink('tmp/byte.tif')
    shutil.rmtree('tmp/out_gdal2tiles_test_nosrs')

    assert 'ERROR ret code = 2' in ret
    assert 'ERROR ret code' not in ret2

def test_does_not_error_when_source_bounds_close_to_tiles_bound():
    """
    Case where the border coordinate of the input file is inside a tile T but the first pixel is
    actually assigned to the tile next to T (nearest neighbour), meaning that when the query is done
    to get the content of T, nothing is returned from the raster.
    """
    in_files = ['./data/test_bounds_close_to_tile_bounds_x.vrt',
                './data/test_bounds_close_to_tile_bounds_y.vrt']
    out_folder = 'tmp/out_gdal2tiles_bounds_approx'
    try:
        shutil.rmtree(out_folder)
    except Exception:
        pass

    script_path = test_py_scripts.get_py_script('gdal2tiles')
    if script_path is None:
        pytest.skip()

    try:
        for in_file in in_files:
            test_py_scripts.run_py_script(
                script_path,
                'gdal2tiles',
                '-q -z 21-21 %s %s' % (in_file, out_folder))
    except TypeError:
        pytest.fail(
            'Case of tile not getting any data not handled properly '
            '(tiles at the border of the image)')



def test_does_not_error_when_nothing_to_put_in_the_low_zoom_tile():
    """
    Case when the highest zoom level asked is actually too low for any pixel of the raster to be
    selected
    """
    in_file = './data/test_bounds_close_to_tile_bounds_x.vrt'
    out_folder = 'tmp/out_gdal2tiles_bounds_approx'
    try:
        shutil.rmtree(out_folder)
    except OSError:
        pass

    script_path = test_py_scripts.get_py_script('gdal2tiles')
    if script_path is None:
        pytest.skip()

    try:
        test_py_scripts.run_py_script(
            script_path,
            'gdal2tiles',
            '-q -z 10 %s %s' % (in_file, out_folder))
    except TypeError:
        pytest.fail(
            'Case of low level tile not getting any data not handled properly '
            '(tile at a zoom level too low)')



def test_python2_handles_utf8_by_default():
    if sys.version_info[0] >= 3:
        pytest.skip()

    return _test_utf8(should_raise_unicode=False)


@pytest.mark.skip("This behaviour doesn't actually work as expected")
def test_python2_gives_warning_if_bad_lc_ctype_and_non_ascii_chars():
    if sys.version_info[0] >= 3:
        pytest.skip()

    lc_ctype = os.environ.get("LC_CTYPE", "")
    os.environ['LC_CTYPE'] = 'fr_FR.latin-1'

    ret = _test_utf8(should_raise_unicode=False, quiet=False, should_display_warning=True)

    os.environ['LC_CTYPE'] = lc_ctype

    return ret


def test_python2_does_not_give_warning_if_bad_lc_ctype_and_all_ascii_chars():
    if sys.version_info[0] >= 3:
        pytest.skip()

    lc_ctype = os.environ.get("LC_CTYPE", "")
    os.environ['LC_CTYPE'] = 'fr_FR.latin-1'

    ret = _test_utf8(should_raise_unicode=False,
                     quiet=False, should_display_warning=False,
                     input_file='./data/test_bounds_close_to_tile_bounds_x.vrt')

    os.environ['LC_CTYPE'] = lc_ctype

    return ret


def test_python2_does_not_give_warning_if_bad_lc_ctype_and_non_ascii_chars_in_folder():
    if sys.version_info[0] >= 3:
        pytest.skip()

    lc_ctype = os.environ.get("LC_CTYPE", "")
    os.environ['LC_CTYPE'] = 'fr_FR.latin-1'

    ret = _test_utf8(should_raise_unicode=False,
                     quiet=False, should_display_warning=False,
                     input_file='./data/漢字/test_bounds_close_to_tile_bounds_x.vrt')

    os.environ['LC_CTYPE'] = lc_ctype

    return ret


def test_python3_handle_utf8_by_default():
    if sys.version_info[0] < 3:
        pytest.skip()

    return _test_utf8(should_raise_unicode=False)


def _test_utf8(should_raise_unicode=False,
               quiet=True,
               should_display_warning=False,
               input_file="data/test_utf8_漢字.vrt"):
    script_path = test_py_scripts.get_py_script('gdal2tiles')
    if script_path is None:
        pytest.skip()

    out_folder = 'tmp/utf8_test'

    try:
        shutil.rmtree(out_folder)
    except OSError:
        pass

    args = '-z 21 %s %s' % (input_file, out_folder)
    if quiet:
        args = "-q " + args

    try:
        ret = test_py_scripts.run_py_script(script_path, 'gdal2tiles', args)
        print(ret)
    except UnicodeEncodeError:
        if should_raise_unicode:
            return
        pytest.fail('Should be handling filenames with utf8 characters in this context')

    assert not should_raise_unicode, \
        'Should not be handling filenames with utf8 characters in this context'

    if should_display_warning:
        assert "WARNING" in ret and "LC_CTYPE" in ret, \
            'Should display a warning message about LC_CTYPE variable'
    else:
        assert not ("WARNING" in ret and "LC_CTYPE" in ret), \
            'Should not display a warning message about LC_CTYPE variable'

    try:
        shutil.rmtree(out_folder)
    except OSError:
        pass


def test_gdal2tiles_py_cleanup():

    lst = ['tmp/out_gdal2tiles_smallworld', 'tmp/out_gdal2tiles_bounds_approx']
    for filename in lst:
        try:
            shutil.rmtree(filename)
        except Exception:
            pass



def test_exclude_transparent_tiles():
    script_path = test_py_scripts.get_py_script('gdal2tiles')
    if script_path is None:
        pytest.skip()

    output_folder = 'tmp/test_exclude_transparent_tiles'
    os.makedirs(output_folder)

    try:
        test_py_scripts.run_py_script_as_external_script(
            script_path,
            'gdal2tiles',
            '-x -z 14-16 data/test_gdal2tiles_exclude_transparent.tif %s' % output_folder)

        # First row totally transparent - no tiles
        tiles_folder = os.path.join(output_folder, '15', '21898')
        dir_files = os.listdir(tiles_folder)
        assert not dir_files, ('Generated empty tiles for row 21898: %s' % dir_files)

        # Second row - only 2 non-transparent tiles
        tiles_folder = os.path.join(output_folder, '15', '21899')
        dir_files = sorted(os.listdir(tiles_folder))
        assert ['22704.png', '22705.png'] == dir_files, \
            ('Generated empty tiles for row 21899: %s' % dir_files)

        # Third row - only 1 non-transparent tile
        tiles_folder = os.path.join(output_folder, '15', '21900')
        dir_files = os.listdir(tiles_folder)
        assert ['22705.png'] == dir_files, \
            ('Generated empty tiles for row 21900: %s' % dir_files)

    finally:
        shutil.rmtree(output_folder)


def test_gdal2tiles_py_profile_raster():

    script_path = test_py_scripts.get_py_script('gdal2tiles')
    if script_path is None:
        pytest.skip()

    shutil.rmtree('tmp/out_gdal2tiles_smallworld', ignore_errors=True)

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        'gdal2tiles',
        '-q -p raster -z 0-1 '+test_py_scripts.get_data_path('gdrivers')+'small_world.tif tmp/out_gdal2tiles_smallworld')

    if sys.platform != 'win32':
        # For some reason, the checksums on the kml file on Windows are the ones of the below png
        _verify_raster_band_checksums(
            'tmp/out_gdal2tiles_smallworld/0/0/0.kml',
            expected_cs = [29839, 34244, 42706, 64319]
        )
    _verify_raster_band_checksums(
        'tmp/out_gdal2tiles_smallworld/0/0/0.png',
        expected_cs = [10125, 10802, 27343, 48852]
    )
    _verify_raster_band_checksums(
        'tmp/out_gdal2tiles_smallworld/1/0/0.png',
        expected_cs = [62125, 59756, 43894, 38539]
    )

    shutil.rmtree('tmp/out_gdal2tiles_smallworld', ignore_errors=True)


def test_gdal2tiles_py_profile_raster_xyz():

    script_path = test_py_scripts.get_py_script('gdal2tiles')
    if script_path is None:
        pytest.skip()

    shutil.rmtree('tmp/out_gdal2tiles_smallworld', ignore_errors=True)

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        'gdal2tiles',
        '-q -p raster --xyz -z 0-1 '+test_py_scripts.get_data_path('gdrivers')+'small_world.tif tmp/out_gdal2tiles_smallworld')

    if sys.platform != 'win32':
        # For some reason, the checksums on the kml file on Windows are the ones of the below png
        _verify_raster_band_checksums(
            'tmp/out_gdal2tiles_smallworld/0/0/0.kml',
            expected_cs = [27644, 31968, 38564, 64301]
        )
    _verify_raster_band_checksums(
        'tmp/out_gdal2tiles_smallworld/0/0/0.png',
        expected_cs = [11468, 10719, 27582, 48827]
    )
    _verify_raster_band_checksums(
        'tmp/out_gdal2tiles_smallworld/1/0/0.png',
        expected_cs = [60550, 62572, 46338, 38489]
    )

    shutil.rmtree('tmp/out_gdal2tiles_smallworld', ignore_errors=True)


def test_gdal2tiles_py_profile_geodetic_tmscompatible_xyz():

    script_path = test_py_scripts.get_py_script('gdal2tiles')
    if script_path is None:
        pytest.skip()

    shutil.rmtree('tmp/out_gdal2tiles_smallworld', ignore_errors=True)

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        'gdal2tiles',
        '-q -p geodetic --tmscompatible --xyz -z 0-1 '+test_py_scripts.get_data_path('gdrivers')+'small_world.tif tmp/out_gdal2tiles_smallworld')

    if sys.platform != 'win32':
        # For some reason, the checksums on the kml file on Windows are the ones of the below png
        _verify_raster_band_checksums(
            'tmp/out_gdal2tiles_smallworld/0/0/0.kml',
            expected_cs = [12361, 18212, 21827, 5934]
        )
    _verify_raster_band_checksums(
        'tmp/out_gdal2tiles_smallworld/0/0/0.png',
        expected_cs = [8560, 8031, 7209, 17849]
    )
    _verify_raster_band_checksums(
        'tmp/out_gdal2tiles_smallworld/1/0/0.png',
        expected_cs = [2799, 3468, 8686, 17849]
    )

    shutil.rmtree('tmp/out_gdal2tiles_smallworld', ignore_errors=True)


def test_gdal2tiles_py_mapml():

    script_path = test_py_scripts.get_py_script('gdal2tiles')
    if script_path is None:
        pytest.skip()

    shutil.rmtree('tmp/out_gdal2tiles_mapml', ignore_errors=True)

    gdal.Translate('tmp/byte_APS.tif', test_py_scripts.get_data_path('gcore') + 'byte.tif',
                   options='-a_srs EPSG:5936 -a_ullr 0 40 40 0')

    test_py_scripts.run_py_script_as_external_script(
        script_path,
        'gdal2tiles',
        '-q -p APSTILE -w mapml -z 16-18 --url "https://foo" tmp/byte_APS.tif tmp/out_gdal2tiles_mapml')

    mapml = open('tmp/out_gdal2tiles_mapml/mapml.mapml', 'rb').read().decode('utf-8')
    #print(mapml)
    assert '<extent units="APSTILE">' in mapml
    assert '<input name="z" type="zoom" value="18" min="16" max="18" />' in mapml
    assert '<input name="x" type="location" axis="column" units="tilematrix" min="122496" max="122496" />' in mapml
    assert '<input name="y" type="location" axis="row" units="tilematrix" min="139647" max="139647" />' in mapml
    assert '<link tref="https://foo/out_gdal2tiles_mapml/{z}/{x}/{y}.png" rel="tile" />' in mapml

    shutil.rmtree('tmp/out_gdal2tiles_mapml', ignore_errors=True)
    gdal.Unlink('tmp/byte_APS.tif')
