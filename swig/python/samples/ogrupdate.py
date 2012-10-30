#!/usr/bin/env python
# -*- coding: utf-8 -*-
###############################################################################
# $Id$
#
# Project:  GDAL/OGR samples
# Purpose:  Update an existing datasource with features from another one
# Author:   Even Rouault <even dot rouault at mines dash paris dot org>
#
###############################################################################
# Copyright (c) 2012, Even Rouault <even dot rouault at mines dash paris dot org>
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

from osgeo import ogr
import sys

DEFAULT = 0
UPDATE_ONLY = 1
APPEND_ONLY = 2

###############################################################
# Usage()

def Usage():
    print('ogrupdate.py -src name -dst name [-srclayer name] [-dstlayer name] [-matchfield name] [-update_only | -append_new_only]')
    print('             [-compare_before_update] [-preserve_fid] [-dry_run] [-progress] [-skip_failures] [-quiet]')
    print('')
    print('Update a target datasource with the features of a source datasource. Contrary to ogr2ogr,')
    print('this script tries to match features, based on FID or field value equality, between the datasources,')
    print('to decide whether to create a new feature, or to update an existing one.')
    print('')
    print('There are 3 modes :')
    print('(default):        *update* features of the target layer that match features from the source layer')
    print('                  and *append* new features from the source layer that do not match any feature from the target layer.')
    print('-update_only:     *update* features of the target layer that match features from the source layer;')
    print('                  do *not append* new features if a source feature has no match in the target layer.')
    print('-append_new_only: do *not update* features of the target layer that match features from the source layer;')
    print('                  *append* new features from the source layer that do not match any feature from the target layer.')
    print('')
    print('Other options : ')
    print(' * If -matchfield is *not* specified, the match criterion is based on FID equality.')
    print(' * When -compare_before_update is specified, a test on all fields of the matching source and target features')
    print('   will be done before determining if an update of the target feature is really necessary.')
    print(' * When -preserve_fid is specified, new features that are appended will try to reuse the FID')
    print('   of the source feature. Note: not all drivers do actually honour that request.')
    print('')

    return 1

###############################################################
# ogrupdate_analyse_args()

def ogrupdate_analyse_args(argv, progress = None, progress_arg = None):

    src_filename = None
    dst_filename = None
    src_layername = None
    dst_layername = None
    matchfieldname = None

    # in case there's no existing matching feature in the target datasource
    # should we try to create a new feature ? 
    update_only = False

    # in case there's no existing matching feature in the target datasource
    # should we preserve the FID of the source feature that will be inserted ?
    preserve_fid = False
    
    # whether we should compare all fields from the features that are found to
    # be matching before actually updating
    compare_before_update = False

    update_mode = DEFAULT

    quiet = False

    skip_failures = False

    dry_run = False

    if len(argv) == 0:
        return Usage()

    i = 0
    while i < len(argv):
        arg = argv[i]
        if arg == '-src' and i+1 < len(argv):
            i = i + 1
            src_filename = argv[i]
        elif arg == '-dst' and i+1 < len(argv):
            i = i + 1
            dst_filename = argv[i]
        elif arg == '-srclayer' and i+1 < len(argv):
            i = i + 1
            src_layername = argv[i]
        elif arg == '-dstlayer' and i+1 < len(argv):
            i = i + 1
            dst_layername = argv[i]
        elif arg == '-matchfield' and i+1 < len(argv):
            i = i + 1
            matchfieldname = argv[i]
        elif arg == '-update_only':
            update_mode = UPDATE_ONLY
        elif arg == '-append_new_only':
            update_mode = APPEND_ONLY
        elif arg == '-preserve_fid':
            preserve_fid = True
        elif arg == '-compare_before_update':
            compare_before_update = True
        elif arg == '-dry_run':
            dry_run = True
        elif arg == '-progress':
            progress = ogr.TermProgress_nocb
            progress_arg = None
        elif arg == '-q' or arg == '-quiet':
            quiet = True
        elif arg[0:5] == '-skip':
            skip_failures = True
        else:
            print('Unrecognized argument : %s' % arg)
            return Usage()
        i = i + 1

    if src_filename is None:
        print('Missing -src')
        return 1

    if dst_filename is None:
        print('Missing -dst')
        return 1

    src_ds = ogr.Open(src_filename)
    if src_ds is None:
        print('Cannot open source datasource %s' % src_filename)
        return 1

    dst_ds = ogr.Open(dst_filename, update = 1)
    if dst_ds is None:
        print('Cannot open destination datasource %s' % dst_filename)
        return 1

    if src_layername is None:
        if src_ds.GetLayerCount() > 1:
            print('Source datasource has more than 1 layers. -srclayer must be specified')
            return 1
        src_layer = src_ds.GetLayer(0)
    else:
        src_layer = src_ds.GetLayerByName(src_layername)
    if src_layer is None:
        print('Cannot open source layer')
        return 1

    src_layername = src_layer.GetName()

    if dst_layername is None:
        if dst_ds.GetLayerCount() > 1:
            dst_layer = dst_ds.GetLayerByName(src_layername)
        else:
            dst_layer = dst_ds.GetLayer(0)
    else:
        dst_layer = dst_ds.GetLayerByName(dst_layername)
    if dst_layer is None:
        print('Cannot open destination layer')
        return 1

    if matchfieldname is None and dst_layer.TestCapability(ogr.OLCRandomRead) and not quiet:
        print('Warning: target layer does not advertize fast random read capability. Update might be slow')

    updated_count = [ 0 ]
    inserted_count = [ 0 ]

    if compare_before_update:
        src_layer_defn = src_layer.GetLayerDefn()
        dst_layer_defn = dst_layer.GetLayerDefn()
        are_comparable = False
        if src_layer_defn.GetFieldCount() == dst_layer_defn.GetFieldCount():
            are_comparable = True
            for i in range(src_layer_defn.GetFieldCount()):
                src_fld_defn = src_layer_defn.GetFieldDefn(i)
                dst_fld_defn = dst_layer_defn.GetFieldDefn(i)
                if src_fld_defn.GetName().lower() != dst_fld_defn.GetName().lower():
                    are_comparable = False
                if src_fld_defn.GetType() != dst_fld_defn.GetType():
                    are_comparable = False
        if not are_comparable:
            if not quiet:
                print('-compare_before_update ignored since layer definitions do not match')
            compare_before_update = False

    ret = ogrupdate_process(src_layer, dst_layer, matchfieldname, update_mode, \
                            preserve_fid, compare_before_update, dry_run, skip_failures, \
                            updated_count, inserted_count, \
                            progress, progress_arg)

    if not quiet:
        print('Summary :')
        print('Features updated  : %d' % updated_count[0])
        print('Features appended : %d' % inserted_count[0])

    src_ds = None
    dst_ds = None

    return ret

###############################################################
# AreFeaturesEqual()

def AreFeaturesEqual(src_feat, dst_feat):
    for i in range(src_feat.GetFieldCount()):
        src_val = src_feat.GetField(i)
        dst_val = dst_feat.GetField(i)
        if src_val != dst_val:
            return False
    src_geom = src_feat.GetGeometryRef()
    dst_geom = dst_feat.GetGeometryRef()
    if src_geom is None and dst_geom is not None:
        return False
    elif src_geom is not None and dst_geom is None:
        return False
    elif src_geom is not None and dst_geom is not None:
        return src_geom.Equals(dst_geom)
    else:
        return True

###############################################################
# ogrupdate_process()

def ogrupdate_process(src_layer, dst_layer, matchfieldname = None, update_mode = DEFAULT, \
                      preserve_fid = False, compare_before_update = False, \
                      dry_run = False, skip_failures = False, \
                      updated_count_out = None, inserted_count_out = None, \
                      progress = None, progress_arg = None):

    dst_layer_defn = dst_layer.GetLayerDefn()

    if matchfieldname is not None:
        src_idx = src_layer.GetLayerDefn().GetFieldIndex(matchfieldname)
        if src_idx < 0:
            print('Cannot find field to match in source layer')
            return 1
        src_type = src_layer.GetLayerDefn().GetFieldDefn(src_idx).GetType()
        dst_idx = dst_layer_defn.GetFieldIndex(matchfieldname)
        if dst_idx < 0:
            print('Cannot find field to match in destination layer')
            return 1 
        dst_type = dst_layer_defn.GetFieldDefn(dst_idx).GetType()

    if progress is not None:
        src_featurecount = src_layer.GetFeatureCount()

    updated_count = 0
    inserted_count = 0

    iter_src_feature = 0
    while True:
        src_feat = src_layer.GetNextFeature()
        if src_feat is None:
            break
        src_fid = src_feat.GetFID()

        iter_src_feature = iter_src_feature + 1
        if progress is not None:
            if progress(iter_src_feature * 1.0 / src_featurecount, "", progress_arg) != 1:
                return 1

        # Do we match on the FID ?
        if matchfieldname is None:
            dst_feat = dst_layer.GetFeature(src_fid)

            if dst_feat is None:
                if update_mode == UPDATE_ONLY:
                    continue
                dst_feat = ogr.Feature(dst_layer_defn)
                dst_feat.SetFrom(src_feat)
                if preserve_fid:
                    dst_feat.SetFID(src_fid)
                if dry_run:
                    ret = 0
                else:
                    ret = dst_layer.CreateFeature(dst_feat)
                if ret == 0:
                    inserted_count = inserted_count + 1

            elif update_mode == APPEND_ONLY:
                continue

            else:
                dst_fid = dst_feat.GetFID()
                assert(dst_fid == src_fid)
                if compare_before_update and AreFeaturesEqual(src_feat, dst_feat):
                    continue
                dst_feat.SetFrom(src_feat) # resets the FID
                dst_feat.SetFID(dst_fid)
                if dry_run:
                    ret = 0
                else:
                    ret = dst_layer.SetFeature(dst_feat)
                if ret == 0:
                    updated_count = updated_count + 1

        # Or on a field ?
        else:
            dst_layer.ResetReading()
            if src_type == dst_type and src_type == ogr.OFTReal:
                val = src_feat.GetFieldAsDouble(src_idx)
                dst_layer.SetAttributeFilter("%s = %.18g" % (matchfieldname, val))
            elif src_type == dst_type and src_type == ogr.OFTInteger:
                val = src_feat.GetFieldAsInteger(src_idx)
                dst_layer.SetAttributeFilter("%s = %d" % (matchfieldname, val))
            else:
                val = src_feat.GetFieldAsString(src_idx)
                dst_layer.SetAttributeFilter("%s = '%s'" % (matchfieldname, val))

            dst_feat = dst_layer.GetNextFeature()
            if dst_feat is None:
                if update_mode == UPDATE_ONLY:
                    continue
                dst_feat = ogr.Feature(dst_layer_defn)
                dst_feat.SetFrom(src_feat)
                if preserve_fid:
                    dst_feat.SetFID(src_fid)
                if dry_run:
                    ret = 0
                else:
                    ret = dst_layer.CreateFeature(dst_feat)
                if ret == 0:
                    inserted_count = inserted_count + 1

            elif update_mode == APPEND_ONLY:
                continue

            else:
                if compare_before_update and AreFeaturesEqual(src_feat, dst_feat):
                    continue
                dst_fid = dst_feat.GetFID()
                dst_feat.SetFrom(src_feat)
                dst_feat.SetFID(dst_fid)
                if dry_run:
                    ret = 0
                else:
                    ret = dst_layer.SetFeature(dst_feat)
                if ret == 0:
                    updated_count = updated_count + 1

        if ret != 0 and not skip_failures:
            return 1

    if updated_count_out is not None and len(updated_count_out) == 1:
        updated_count_out[0] = updated_count

    if inserted_count_out is not None and len(inserted_count_out) == 1:
        inserted_count_out[0] = inserted_count

    return 0

###############################################################
# Entry point

if __name__ == '__main__':
    argv = ogr.GeneralCmdLineProcessor( sys.argv )
    sys.exit(ogrupdate_analyse_args(argv[1:]))
