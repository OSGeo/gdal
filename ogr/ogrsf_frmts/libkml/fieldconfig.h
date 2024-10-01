/******************************************************************************
 *
 * Project:  KML Translator
 * Purpose:  Implements OGRLIBKMLDriver
 * Author:   Brian Case, rush at winkey dot org
 *
 ******************************************************************************
 * Copyright (c) 2010, Brian Case
 * Copyright (c) 2014, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 *****************************************************************************/

#ifndef OGRLIBKMLFIELDCONFIG_H_INCLUDED
#define OGRLIBKMLFIELDCONFIG_H_INCLUDED

/*******************************************************************************
 Function to fetch the field config options.
*******************************************************************************/

struct fieldconfig
{
    const char *idfield;
    const char *namefield;
    const char *descfield;
    const char *tsfield;
    const char *beginfield;
    const char *endfield;
    const char *altitudeModefield;
    const char *tessellatefield;
    const char *extrudefield;
    const char *visibilityfield;
    const char *drawOrderfield;
    const char *iconfield;
    const char *headingfield;
    const char *tiltfield;
    const char *rollfield;
    const char *snippetfield;
    const char *modelfield;
    const char *scalexfield;
    const char *scaleyfield;
    const char *scalezfield;
    const char *networklinkfield;
    const char *networklink_refreshvisibility_field;
    const char *networklink_flytoview_field;
    const char *networklink_refreshMode_field;
    const char *networklink_refreshInterval_field;
    const char *networklink_viewRefreshMode_field;
    const char *networklink_viewRefreshTime_field;
    const char *networklink_viewBoundScale_field;
    const char *networklink_viewFormat_field;
    const char *networklink_httpQuery_field;
    const char *camera_longitude_field;
    const char *camera_latitude_field;
    const char *camera_altitude_field;
    const char *camera_altitudemode_field;
    const char *photooverlayfield;
    const char *leftfovfield;
    const char *rightfovfield;
    const char *bottomfovfield;
    const char *topfovfield;
    const char *nearfield;
    const char *photooverlay_shape_field;
    const char *imagepyramid_tilesize_field;
    const char *imagepyramid_maxwidth_field;
    const char *imagepyramid_maxheight_field;
    const char *imagepyramid_gridorigin_field;
};

void get_fieldconfig(struct fieldconfig *oFC);

#endif
