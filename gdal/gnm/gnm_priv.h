/******************************************************************************
 * $Id$
 *
 * Project:  GDAL/OGR Geography Network support (Geographic Network Model)
 * Purpose:  GNM private and internal declarations.
 * Authors:  Mikhail Gusev (gusevmihs at gmail dot com)
 *           Dmitry Baryshnikov, polimax@mail.ru
 *
 ******************************************************************************
 * Copyright (c) 2014, Mikhail Gusev
 * Copyright (c) 2014-2015, NextGIS <info@nextgis.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ****************************************************************************/

#ifndef GNM_PRIV
#define GNM_PRIV

#include "gdal_priv.h"

/* -------------------------------------------------------------------- */
/*              Common constants for internal use                       */
/* -------------------------------------------------------------------- */

// General constants.
#define GNM_VERSION     "1.0"
#define GNM_VERSION_NUM 100 //major * 100 + minor

// Obligatory system layers.
#define GNM_SYSLAYER_META       "_gnm_meta"
#define GNM_SYSLAYER_GRAPH      "_gnm_graph"
#define GNM_SYSLAYER_FEATURES   "_gnm_features"

// System field names.
// FORMAT NOTE: Shapefile driver does not support field names more than 10
//              characters.

#define GNM_SYSFIELD_PARAMNAME  "key"
#define GNM_SYSFIELD_PARAMVALUE "val"
#define GNM_SYSFIELD_SOURCE     "source"
#define GNM_SYSFIELD_TARGET     "target"
#define GNM_SYSFIELD_CONNECTOR  "connector"
#define GNM_SYSFIELD_COST       "cost"
#define GNM_SYSFIELD_INVCOST    "inv_cost"
#define GNM_SYSFIELD_DIRECTION  "direction"
#define GNM_SYSFIELD_GFID       "gnm_fid"
#define GNM_SYSFIELD_LAYERNAME  "ogrlayer"
#define GNM_SYSFIELD_BLOCKED    "blocked"
#define GNM_SYSFIELD_PATHNUM    "path_num"
#define GNM_SYSFIELD_TYPE       "ftype"

// Rule strings key-words.
#define GNM_RULEKW_CONNECTS "CONNECTS"
#define GNM_RULEKW_WITH "WITH"
#define GNM_RULEKW_VIA "VIA"
#define GNM_RULEKW_ALLOW "ALLOW"
#define GNM_RULEKW_DENY "DENY"
#define GNM_RULEKW_ANY "ANY"

// Graph constants
#define GNM_EDGE_DIR_BOTH       0   // bidirectional
#define GNM_EDGE_DIR_SRCTOTGT   1   // from source to target
#define GNM_EDGE_DIR_TGTTOSRC   2   // from target to source

#define GNM_BLOCK_NONE 0x0000  // no blocking (vertex or edge)
#define GNM_BLOCK_SRC  0x0001  // the source vertex is blocked
#define GNM_BLOCK_TGT  0x0002  // the target vertext is blocked
#define GNM_BLOCK_CONN 0x0004  // the connection edge is blocked
#define GNM_BLOCK_ALL GNM_BLOCK_SRC | GNM_BLOCK_TGT | GNM_BLOCK_CONN


// Other string constants.
#define GNM_SRSFILENAME "_gnm_srs.prj"

// Alias for some big data type to store identificators.
#define GNMGFID GIntBig
// Corespondent datatype for identificator
// (i.e. int -> OFTInteger -> GetFieldAsInteger and
//       GUIntBig -> OFTInteger64 -> GetFieldAsInteger64)
#define GNMGFIDInt OFTInteger64
#define GetFieldAsGNMGFID GetFieldAsInteger64
#define GNMGFIDFormat "%lld"

#endif // GNM_PRIV

