# This is a framework to test GIGS, Geospatial Integrity of geoscience
# software. Which is published by International Association of Oil & Gas
# Producers  as a resources for how geospatial software should give consistent
# and expected results.
#
# This could be expanded to be used with other testing frameworks.
#
# - See more at: http://www.iogp.org/Geomatics#2521115-gigs
#

#
# == REQUIREMENTS ==
#  - Python 2.7 or 3.3+
#  - pyproj  (optional but highly recommended) - read TESTNOTES.md

# == TODO list ==
# * Python 3 was not running all the test cases seemingly due to how it uses
#   an iterable version of zip.  -- FIXED
# * driver for proj4js (javascript)
#     - could be written using nodejs using subprocess
#     - could use PyExecJS (currently not advanced enough) or PyV8
# * call cs2cs directly
#      - WORKING but 2 orders of magnitude slower than pyproj, and
#        potentially insecure shelling?


import argparse
import glob
import json
import logging
import os
import platform
import sys
import subprocess
from tempfile import NamedTemporaryFile

# only for debug
# import pdb

try:
    import pyproj
except ImportError as e_pyproj:
    pass


PY_MAJOR = sys.version_info[0]
PY2 = (PY_MAJOR == 2)

# Python 2/3 Compatibility code ########################################
if PY_MAJOR >= 3:
    # list-producing versions of the major Python iterating functions
    # lzip acts like Python 2.x zip function.
    # taken from Python-Future       http://python-future.org/
    # future.utils.lzip()

    def lzip(*args, **kwargs):
        return list(zip(*args, **kwargs))

else:
    from __builtin__ import zip as lzip
########################################################################


def list_count_matches(coords, ex_coords, tolerance):
    """
    counts coordinates in lists that match and don't match.
    assumes that lists are the same length (they should be)

    coords    - coordinates
    ex_cords  - expected cooridnate
    tolerance - difference allowed between the coordinates

    returns tuple (matches, non_matches)
    """
    matches, non_matches = 0, 0
    iter_ex_coords = iter(ex_coords)
    for c in coords:
        ex_coord = next(iter_ex_coords)
        if match_func(c, ex_coord, tolerance):
            matches = matches + 1
        else:
            non_matches = non_matches + 1

    return (matches, non_matches)


def match_func(cor, exc, tolerance):
    """
    Check if coordinate matches expected coordinate within a given tolerance.

    cor - coordinate
    exc - expected coordinate
    tolerance - error rate
                float coordinate elements will be checked based on this value
                list/tuple coordinate elements will be checked based on the
                      corresponding values
    return bool
    """
    if len(exc) == 3:
        # coordinate triples
        coord_diff = abs(cor[0] - exc[0]), abs(cor[1] - exc[1]), abs(cor[2] - exc[2])
        if isinstance(tolerance, float):
            matching = coord_diff < (tolerance, tolerance, tolerance)
        elif isinstance(tolerance, (list, tuple)):  # FIXME is list's length atleast 3?
            matching = coord_diff < tuple(tolerance)
    else:
        # assume coordinate pairs
        coord_diff = abs(cor[0] - exc[0]), abs(cor[1] - exc[1])
        if isinstance(tolerance, float):
            matching = coord_diff < (tolerance, tolerance)
        elif isinstance(tolerance, (list, tuple)):  # FIXME is list's length atleast 2?
            matching = coord_diff < tuple(tolerance)

    if matching is False:
        logging.info('non-match,  calculated coordinate: {c1}\n'
                     'expected coordinate: {c2}\n difference:{res}\n'
                     'tolerance: {tol}\n'
                     ''.format(c1=cor, c2=exc, res=coord_diff, tol=tolerance))

    return matching


# parse multiple tests and call TransformTest
# TODO: needs some awareness of the driver, so driver_info function in
#       TranformTest classes can be called, could allow a dummy instance of
#       Driver and move all the initization code to another function?  Or allow
#       dipatch function to check if everything is in order do a transform.
#       Not an elegant solution.
class TransformRunner(object):
    def __init__(self, fn_pattern, driver, **kwargs):
        """
        fn_pattern - file name or file name pattern (example "*.json")
        driver - this is the type of driver to run
        kwargs - parameters passed to the respective driver TransfromTest class
        """
        self.driver = driver
        json_input = []
        if os.path.isfile(fn_pattern):
            with open(fn_pattern, 'rt') as fp:
                logging.info("Reading json from file '{0}'".format(fn_pattern))
                json_dict = json.load(fp, parse_int=float)
                json_dict['filename'] = fn_pattern
                json_input = [json_dict]
        # is this a glob/fnmatch style pattern?
        elif '*' in fn_pattern or '?' in fn_pattern:
            filename_iter = glob.iglob(fn_pattern)
            for filename in filename_iter:
                with open(filename, 'rt') as fp:
                    logging.info("Reading json from file '{0}'".format(filename))
                    j_input = json.load(fp, parse_int=float)
                    if isinstance(j_input, dict):
                        j_input['filename'] = filename
                        json_input.append(j_input)
                    elif isinstance(j_input, list):
                        # FIXME could build a new list with the filename dict
                        logging.warning("json file is a list, not quite supported - FIXME")
                        json_input.extend(j_input)
                    else:
                        raise ValueError('json input in an unknown type')
        else:
            raise TypeError('filename_pattern must be a valid filename or pattern')

        self.runs = json_input
        self.kwargs = kwargs

    def dispatch(self):
        """
        main loop to run all the tests
        """
        total_matches, total_no_matches, success_code = 0, 0, 0
        for run in self.runs:
            if self.driver == 'pyproj':
                trantst = TransformTestPyProj(run, self.kwargs)
            elif self.driver == 'cs2cs':
                trantst = TransformTestCs2cs(run, self.kwargs)
            else:
                raise ValueError("driver {0} is not a valid test driver".format(self.driver))
            matches, no_matches = trantst.dispatch()
            total_matches += matches
            total_no_matches += no_matches
            success_code += no_matches

        return total_matches, total_no_matches, success_code


# parses and runs a single GIGS test case
class TransformTestBase(object):
    """
    TransformTest common code for testing framework.
    """
    def __init__(self, json_dict, kwargs):
        """
        json_dict must dictonary from json
        """
        if not isinstance(json_dict, dict):  # must be a json dictionary
            raise TypeError("json_source must be a dictionary type not {0}"
                            "".format(type(json_dict)))

        # require keys 'coordinates' and 'projections'
        if 'coordinates' not in json_dict:
            raise KeyError("TransformTest.__init__ requires 'coordinates' key"
                           " in json source input")

        if 'projections' not in json_dict:
            raise KeyError("TransformTest.__init__ requires 'projections' key"
                           " in json source input")

        logging.info('Number of coordinate pairs to test: {0}'.format(
                     len(json_dict['coordinates'])))

        self.run_test_args = kwargs.get('test')

        # unpack coordinates
        self.coords_left, self.coords_right = lzip(*json_dict['coordinates'])

        self.testobj = json_dict

    def runner_conversion(self, **kwargs):
        """
        tests a single conversion

        return tuple (num_matches, num_no_matches)
        """

        # get tolerance, if not set tolerance to a precise value
        tolerances = kwargs.get('tolerances', [0.0000000000001, 0.0000000000001])

        test_right = self.transform(self.proj_left, self.proj_right, self.coords_left)
        test_left = self.transform(self.proj_right, self.proj_left, self.coords_right)

        results1 = list_count_matches(test_right, self.coords_right, tolerances[1])
        results2 = list_count_matches(test_left, self.coords_left, tolerances[0])

        return (results1[0] + results2[0], results1[1] + results2[1])

    def runner_roundtrip(self, **kwargs):
        """
        rountrip test using pyproj.

        times - number roundtrips to perform
        tolerance - TODO explain the structure of why this is a list

        return tuple (num_matches, num_no_matches)
        """
        times = None

        # get variables
        times = int(kwargs.get('times'))
        tolerances = kwargs.get('tolerances', [0.0000000000001, 0.0000000000001])

        # keep the transformations separate, so as to not cross contaminate the
        # results.

        # process roundtrip for the left coordinates - Test 1
        test1_left = self.coords_left
        for i in range(times):
            test1_right = self.transform(self.proj_left, self.proj_right, test1_left)
            test1_left = self.transform(self.proj_right, self.proj_left, test1_right)

        # process roundtrip for the right coordinates - Test 2
        test2_right = self.coords_right
        for i in range(times):
            test2_left = self.transform(self.proj_right, self.proj_left, test2_right)
            test2_right = self.transform(self.proj_left, self.proj_right, test2_left)

        results = (
            list_count_matches(test1_right, self.coords_right, tolerances[1]),
            list_count_matches(test1_left, self.coords_left, tolerances[0]),
            list_count_matches(test2_right, self.coords_right, tolerances[1]),
            list_count_matches(test2_left, self.coords_left, tolerances[0])
        )

        return tuple(sum(x) for x in lzip(*results))

    # TODO: Untested.  Not useful for GIGS.
    def runner_onedirection(self, **kwargs):
        """
        Perform a conversion in only one direction, not both.  Useful for
        testing convergence of a coordinate system.

        return tuple (num_matches, num_no_matches)
        """
        # get variables
        direction = kwargs.get('direction')
        # get tolerance, if not set tolerance to a precise value
        tolerances = kwargs.get('tolerances', [0.0000000000001, 0.0000000000001])

        if direction not in ('left-to-right', 'right-to-left'):
            raise ValueError('direction must be left-to-right or right-to-left, not: {0}'.format(direction))

        if direction == 'left-to-right':
            test_dest_right = self.transform(self.proj_left, self.proj_right, self.coords_left)
            results = list_count_matches(test_dest_right, self.coords_right, tolerances[1])
        elif direction == 'right-to-left':
            test_dest_left = self.transform(self.proj_right, self.proj_left, self.coords_right)
            results = list_count_matches(test_dest_left, self.coords_left, tolerances[0])
        else:
            raise RuntimeError('Unexpected state of value direction "{0}" in runner_onedirection'.format(direction))

        return results

    # placeholder function
    def transform(self, src_crs, dst_crs, coords):
        pass

    def dispatch(self):
        """
        main
        """
        matches, no_matches = 0, 0

        # convert to tuple
        run_tests = self.run_test_args,
        if self.run_test_args is None:
            run_tests = ('conversion', 'roundtrip')

        logging.info('Testing: {0}'.format(self.testobj['description']))

        for test in self.testobj['tests']:
            m_res, nm_res = None, None
            if test['type'] not in run_tests:
                # skip test
                continue

            if test['type'] == 'conversion':
                m_res, nm_res = self.runner_conversion(**test)
            elif test['type'] == 'roundtrip':
                m_res, nm_res = self.runner_roundtrip(**test)

            if nm_res == 0:
                logging.info("   {0}... All {1} match.".format(test['type'], m_res))
            else:
                logging.info("   {0}... matches: {1}   doesn't match: {2}"
                             "".format(test['type'], m_res, nm_res))

            matches += m_res
            no_matches += nm_res

        return matches, no_matches

    # placeholder function  -- TODO How should this be exposed?
    def driver_info(self):
        return "base class"


class TransformTestPyProj(TransformTestBase):
    """
    TransformTest uses pyproj to run tests.
    """
    def __init__(self, json_dict, kwargs):
        # call super class
        TransformTestBase.__init__(self, json_dict, kwargs)

        # setup projections
        try:
            self.proj_left = pyproj.Proj(json_dict['projections'][0], preserve_units=True)
        except RuntimeError as e:
            logging.error('pyproj raised a RuntimeError for projection string:'
                          ' "{0}"'.format(json_dict['projections'][0]))
            raise RuntimeError(e)
        try:
            self.proj_right = pyproj.Proj(json_dict['projections'][1], preserve_units=True)
        except RuntimeError as e:
            logging.error('pyproj raised a RuntimeError for projection string:'
                          ' "{0}"'.format(json_dict['projections'][1]))
            raise RuntimeError(e)

    def transform(self, src_crs, dst_crs, coords):
        return self.pyproj_transform_ex(src_crs, dst_crs, coords)

    def driver_info(self):
        return 'pyproj {0}\nproj4 {1}\n'.format(
            pyproj.__version__, self.proj4_version())

    # TODO: currently dead code, unneeded for the pyproj repo. version
    #       as of 2016-05-24.
    def proj4_version():
        """
        Gives the proj.4 library's version number. (requires pyproj)
        returns string, so proj.4 version 4.9.3 will return "4.9.3"
        """
        try:
            return pyproj.proj_version_str
        except AttributeError:
            # for pyproj versions 1.9.5.1 and before, this will run
            # Get PROJ4 version in a floating point number
            proj4_ver_num = pyproj.Proj(proj='latlong').proj_version

            # convert float into a version string (4.90 becomes '4.9.0')
            return '.'.join(str(int(proj4_ver_num * 100)))

    def pyproj_transform_ex(self, proj_src, proj_dst, coords):
        """
        wrapper for pyproj.transform to do all the zipping of the coordinates

        returns coordinate list
        """
        # are these coordinate triples?
        if len(coords[0]) == 3:
            xi, yi, zi = lzip(*coords)
            xo, yo, zo = pyproj.transform(proj_src, proj_dst, xi, yi, zi)
            return lzip(xo, yo, zo)

        # assume list of coordinate pairs
        xi, yi = lzip(*coords)
        xo, yo = pyproj.transform(proj_src, proj_dst, xi, yi)
        return lzip(xo, yo)


class TransformTestCs2cs(TransformTestBase):
    def __init__(self, json_dict, kwargs):
        # call super class
        TransformTestBase.__init__(self, json_dict, kwargs)

        # copy proj4 projection strings
        self.proj_left, self.proj_right = json_dict['projections']

        self.exe = kwargs.get('exe', 'cs2cs')

        # when the exe is not the default, check if the file exists
        if self.exe == 'cs2cs' or not os.path.isfile(self.exe):
            raise RuntimeError('cannot find cs2cs executable file: {}'
                               ''.format(self.exe))

    def transform(self, src_crs, dst_crs, coords):
        # send points to a temporary file
        # TODO Should this use with statement?
        tmpfn = NamedTemporaryFile(mode='w+t', delete=False)
        for point in coords:
            # convert list of float values into a list of strings
            point = [str(e) for e in point]
            # print('POINT: {}'.format(point))
            tmpfn.write(' '.join(point) + '\n')

        tmpfn.flush()
        command = "{exe} {proj_from} +to {proj_to} -f %.13f {filename}".format(
            exe=self.exe, proj_from=src_crs, proj_to=dst_crs, filename=tmpfn.name)
        tmpfn.close()

        logging.debug('Running Popen on command "{0}"'.format(command))

        if platform.system() == 'Windows':
            shell = False
        else:
            # shell=True according to the subprocess documentation has some
            # security implications
            # Linux seems to need this
            shell = True

        # call cs2cs
        outs = subprocess.check_output(command, shell=shell)

        # delete temporary filename
        os.unlink(tmpfn.name)

        # print('RESULTS OUTS:  {}\n'.format(outs))
        # print('RESULTS ERRS:  {}\n'.format(errs))

        # outs
        # print('RESULTS LINE: {}\n'.format([line.split() for line in outs]))
        coords = []

        # process output
        for line in outs.splitlines():
            # print("LINE: {}".format(line))
            coord = []
            for e in line.split():
                coord.append(float(e))

            coords.append(coord)

        # print('COORDS: {}\n'.format(coords))
        return coords

    def driver_info(self):
        shell = True      # see transform() for info.
        if platform.system() == 'Windows':
            shell = False

        outs = subprocess.check_output(self.exe, shell=shell)
        return 'PROJ.4 version: ' + outs.splitlines()[0] + '\n'


if __name__ == '__main__':
    # logging.basicConfig(level=logging.DEBUG)
    logging.basicConfig(level=logging.INFO)

    parser = argparse.ArgumentParser(description='Test PROJ.4 using a JSON file.')
    parser.add_argument('-e', '--exe',
                        help="executable with path default: 'cs2cs' (needed for cs2cs driver)")

    parser.add_argument('-d', '--driver', default='pyproj',
                        help='driver to test')

    parser.add_argument('-t', '--test',
                        help='only run these test types (valid values: conversion or roundtrip)')

    # get json file names and/or glob patterns
    parser.add_argument('testnames_pat_json', nargs=argparse.REMAINDER,
                        help='single filename or glob wildcard patern')

    args = parser.parse_args()

    # test that the arguments have sensible values
    if args.driver not in ('cs2cs', 'pyproj'):
        raise ValueError('driver "{}" is not a valid driver'.format(args.driver))

    logging.info('Python {}'.format(sys.version))
    logging.info('using driver: {}'.format(args.driver))

    # there could be a version command for the TransformRunner TODO

    match_results, nonmatch_results, success_code = 0, 0, 0
    for test_name in args.testnames_pat_json:
        tratst = TransformRunner(test_name, driver=args.driver, exe=args.exe,
                                 test=args.test)
        m_res, nm_res, success_cd = tratst.dispatch()
        match_results += m_res
        nonmatch_results += nm_res
        success_code += success_cd

    logging.info("----------------------------------------")
    logging.info("TOTAL: matches: {0}   non-matching: {1}"
                 "".format(match_results, nonmatch_results))

    # exit status code is the number of non-matching results
    # This should play nicely with Travis and similar testing.
    sys.exit(success_code)
