#!/usr/bin/env python3
# ******************************************************************************
#  $Id: loslas2ntv2.py 19102 2010-03-15 23:41:44Z warmerdam $
#
#  Project:  LOS/LAS to NTv2 Translator
#  Purpose:  Translate one or many LOS/LAS sets into an NTv2 datum shift grid
#            file.
#  Author:   Frank Warmerdam, warmerdam@pobox.com
#  Financial Support: i-cubed (http://www.i-cubed.com)
#
# ******************************************************************************
#  Copyright (c) 2010, Frank Warmerdam
#
#  Permission is hereby granted, free of charge, to any person obtaining a
#  copy of this software and associated documentation files (the "Software"),
#  to deal in the Software without restriction, including without limitation
#  the rights to use, copy, modify, merge, publish, distribute, sublicense,
#  and/or sell copies of the Software, and to permit persons to whom the
#  Software is furnished to do so, subject to the following conditions:
#
#  The above copyright notice and this permission notice shall be included
#  in all copies or substantial portions of the Software.
#
#  THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
#  OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
#  FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
#  THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
#  LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
#  FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
#  DEALINGS IN THE SOFTWARE.
# ******************************************************************************

import sys
from osgeo import gdal

# dummy object to hold options


class Options(object):
    def __init__(self):
        self.verbose_flag = 0
        self.append = 0
        self.create_self = []
        self.metadata = []
        self.negate = 0

# =============================================================================


def Usage():
    print('Usage: loslas2ntv2.py  [-a] [-auto] [-sub_name name] [-parent name]')
    print('                       [-created date] [-updated date] [-gs_type name]')
    print('                       [-system_f name] [-system_t name] [-version version]')
    print('                       [-major_f axis] [-minor_f axis]')
    print('                       [-major_t axis] [-minor_t axis]')
    print('                       [-negate] [src_file.los]* dst_file.gsb ')
    print('')
    print(' -a: append to existing NTv2 file.')
    print(' -auto: process a whole directory of nad27/hpgn los/las files.')
    print(' -negate: reverse direction of change.')
    print('')
    print('eg.')
    print(' loslas2ntv2.py -auto *.los')
    print(' loslas2ntv2.py -system_f NAD27 -system_t NAD83 -sub_name conus conus.los conus.gsb')
    return 1


# =============================================================================

def TranslateLOSLAS(los, ntv2_filename, options):

    # Open the LOS and LAS files.
    los_filename = los[:-4] + '.los'
    las_filename = los[:-4] + '.las'

    los_db = gdal.Open(los_filename)
    las_db = gdal.Open(las_filename)

    # Create (or append to) the NTv2 file.
    create_options = options.create_options
    if options.append == 1:
        create_options.append('APPEND_SUBDATASET=YES')

    ntv2_driver = gdal.GetDriverByName('NTv2')
    ntv2_db = ntv2_driver.Create(ntv2_filename,
                                 los_db.RasterXSize, los_db.RasterYSize,
                                 4, gdal.GDT_Float32, create_options)

    # Copy georeferencing
    ntv2_db.SetGeoTransform(los_db.GetGeoTransform())

    # Copy offsets.
    data = las_db.ReadAsArray()
    if options.negate:
        data = -1 * data
    ntv2_db.GetRasterBand(1).WriteArray(data)

    data = los_db.ReadAsArray()
    if options.negate:
        data = -1 * data
    ntv2_db.GetRasterBand(2).WriteArray(data)

    if options.metadata:
        ntv2_db.SetMetadata(options.metadata)

# =============================================================================
# Auto-process the normal NOAA director of los/las files, producing a
# NAD27 NTv2 file, and an HPGN NTv2 file.


def auto_noaa(options, loslas_list):

    options.append = 0
    options.verbose_flag = 0

    original_metadata = options.metadata

    have_nad27 = 0
    # have_hpgn = 0

    for los in loslas_list:

        options.create_options = []
        options.metadata = original_metadata
        if los.find('hpgn') != -1:
            ntv2_filename = los[:-4] + '.gsb'
            options.append = 0
            options.negate = 1
            options.metadata.append('SUB_NAME=' + los[:2])
            options.metadata.append('MAJOR_F=6378137.0')
            options.metadata.append('MINOR_F=6356752.31414')
            options.metadata.append('MAJOR_T=6378137.0')
            options.metadata.append('MINOR_T=6356752.31414')
            options.metadata.append('SYSTEM_F=HARN')
            options.metadata.append('SYSTEM_T=NAD83')

        else:
            ntv2_filename = 'nad27_usa.gsb'
            options.metadata.append('SUB_NAME=' + los[:-4])
            if have_nad27 == 0:
                options.append = 0
                options.metadata.append('MAJOR_F=6378206.4')
                options.metadata.append('MINOR_F=6356583.8')
                options.metadata.append('MAJOR_T=6378137.0')
                options.metadata.append('MINOR_T=6356752.31414')
                options.metadata.append('SYSTEM_F=NAD27')
                options.metadata.append('SYSTEM_T=NAD83')
            else:
                options.append = 1
            have_nad27 = 1

        print('Integrate %s into %s.' % (los, ntv2_filename))

        TranslateLOSLAS(los, ntv2_filename, options)

    return 0


def main(argv):
    ntv2_filename = None
    loslas_list = []
    auto_flag = 0

    options = Options()

    argv = gdal.GeneralCmdLineProcessor(argv)
    if argv is None:
        return 0

    # Parse command line arguments.

    i = 1
    while i < len(argv):
        arg = argv[i]

        if arg == '-v':
            options.verbose_flag = 1

        elif arg == '-version' and i < len(argv) - 1:
            options.metadata.append('VERSION=' + argv[i + 1])
            i = i + 1

        elif arg == '-created' and i < len(argv) - 1:
            options.metadata.append('CREATED=' + argv[i + 1])
            i = i + 1

        elif arg == '-updated' and i < len(argv) - 1:
            options.metadata.append('UPDATED=' + argv[i + 1])
            i = i + 1

        elif arg == '-system_f' and i < len(argv) - 1:
            options.metadata.append('SYSTEM_F=' + argv[i + 1])
            i = i + 1

        elif arg == '-system_t' and i < len(argv) - 1:
            options.metadata.append('SYSTEM_T=' + argv[i + 1])
            i = i + 1

        elif arg == '-parent' and i < len(argv) - 1:
            options.metadata.append('PARENT=' + argv[i + 1])
            i = i + 1

        elif arg == '-sub_name' and i < len(argv) - 1:
            options.metadata.append('SUB_NAME=' + argv[i + 1])
            i = i + 1

        elif arg == '-gs_type' and i < len(argv) - 1:
            options.metadata.append('GS_TYPE=' + argv[i + 1])
            i = i + 1

        elif arg == '-major_f' and i < len(argv) - 1:
            options.metadata.append('MAJOR_F=' + argv[i + 1])
            i = i + 1

        elif arg == '-minor_f' and i < len(argv) - 1:
            options.metadata.append('MINOR_F=' + argv[i + 1])
            i = i + 1

        elif arg == '-major_t' and i < len(argv) - 1:
            options.metadata.append('MAJOR_T=' + argv[i + 1])
            i = i + 1

        elif arg == '-minor_t' and i < len(argv) - 1:
            options.metadata.append('MINOR_T=' + argv[i + 1])
            i = i + 1

        elif arg == '-negate':
            options.negate = 1

        elif arg == '-auto':
            auto_flag = 1

        elif arg == '-a':
            options.append = 1

        elif arg[0] == '-':
            return Usage()

        elif arg[-4:] == '.los' or arg[-4:] == '.las':
            loslas_list.append(arg)

        elif arg[-4:] == '.gsb' and ntv2_filename is None:
            ntv2_filename = arg

        else:
            print('Unrecognized argument: ', arg)
            return Usage()

        i = i + 1

    if not loslas_list:
        print('No .los/.las files specified as input.')
        return Usage()

    if auto_flag == 1:
        auto_noaa(options, loslas_list)

    if ntv2_filename is None:
        print('No NTv2 file specified.')
        return Usage()

    # Process loslas files.

    for los in loslas_list:

        TranslateLOSLAS(los, ntv2_filename, options)
        options.append = 1

    return 0


if __name__ == '__main__':
    sys.exit(main(sys.argv))

