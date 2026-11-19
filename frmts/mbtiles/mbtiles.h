/******************************************************************************
 *
 * Project:  GDAL MBTiles driver
 * Purpose:  Implement GDAL MBTiles support using OGR SQLite driver
 * Author:   Even Rouault, Even Rouault <even.rouault at spatialys.com>
 *
 **********************************************************************
 * Copyright (c) 2012-2016, Even Rouault <even.rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef MBTILES_H_INCLUDED
#define MBTILES_H_INCLUDED

#define MBTILES_COMPRESSION_OPTIONS                                            \
    "  <Option name='TILE_FORMAT' scope='raster' type='string-select' "        \
    "description='Format to use to create tiles' default='PNG'>"               \
    "    <Value>PNG</Value>"                                                   \
    "    <Value>PNG8</Value>"                                                  \
    "    <Value>JPEG</Value>"                                                  \
    "    <Value>WEBP</Value>"                                                  \
    "  </Option>"                                                              \
    "  <Option name='QUALITY' scope='raster' type='int' min='1' max='100' "    \
    "description='Quality for JPEG and WEBP tiles' default='75'/>"             \
    "  <Option name='ZLEVEL' scope='raster' type='int' min='1' max='9' "       \
    "description='DEFLATE compression level for PNG tiles' default='6'/>"      \
    "  <Option name='DITHER' scope='raster' type='boolean' "                   \
    "description='Whether to apply Floyd-Steinberg dithering (for "            \
    "TILE_FORMAT=PNG8)' default='NO'/>"

#define MBTILES_RASTER_CREATION_OPTIONS                                        \
    "  <Option name='NAME' scope='raster,vector' type='string' "               \
    "description='Tileset name'/>"                                             \
    "  <Option name='DESCRIPTION' scope='raster,vector' type='string' "        \
    "description='A description of the layer'/>"                               \
    "  <Option name='TYPE' scope='raster,vector' type='string-select' "        \
    "description='Layer type' default='overlay'>"                              \
    "    <Value>overlay</Value>"                                               \
    "    <Value>baselayer</Value>"                                             \
    "  </Option>"                                                              \
    "  <Option name='ELEVATION_TYPE' scope='raster' type='string-select' "     \
    "description='Type of elevation encoding' default=''>"                     \
    "    <Value></Value>"                                                      \
    "    <Value>terrain-rgb</Value>"                                           \
    "  </Option>"                                                              \
    "  <Option name='VERSION' scope='raster' type='string' "                   \
    "description='The version of the tileset, as a plain number' "             \
    "default='1.1'/>"                                                          \
    "  <Option name='BLOCKSIZE' scope='raster' type='int' "                    \
    "description='Block size in pixels' default='256' min='64' "               \
    "max='8192'/>" MBTILES_COMPRESSION_OPTIONS                                 \
    "  <Option name='ZOOM_LEVEL_STRATEGY' scope='raster' "                     \
    "type='string-select' description='Strategy to determine zoom level.' "    \
    "default='AUTO'>"                                                          \
    "    <Value>AUTO</Value>"                                                  \
    "    <Value>LOWER</Value>"                                                 \
    "    <Value>UPPER</Value>"                                                 \
    "  </Option>"                                                              \
    "  <Option name='RESAMPLING' scope='raster' type='string-select' "         \
    "description='Resampling algorithm.' default='BILINEAR'>"                  \
    "    <Value>NEAREST</Value>"                                               \
    "    <Value>BILINEAR</Value>"                                              \
    "    <Value>CUBIC</Value>"                                                 \
    "    <Value>CUBICSPLINE</Value>"                                           \
    "    <Value>LANCZOS</Value>"                                               \
    "    <Value>MODE</Value>"                                                  \
    "    <Value>AVERAGE</Value>"                                               \
    "  </Option>"                                                              \
    "  <Option name='WRITE_BOUNDS' scope='raster' type='boolean' "             \
    "description='Whether to write the bounds metadata' default='YES'/>"       \
    "  <Option name='WRITE_MINMAXZOOM' scope='raster' type='boolean' "         \
    "description='Whether to write the minzoom and maxzoom metadata' "         \
    "default='YES'/>"                                                          \
    "  <Option name='BOUNDS' scope='raster,vector' type='string' "             \
    "description='Override default value for bounds metadata item'/>"          \
    "  <Option name='CENTER' scope='raster,vector' type='string' "             \
    "description='Override default value for center metadata item'/>"

#endif  // MBTILES_H_INCLUDED
