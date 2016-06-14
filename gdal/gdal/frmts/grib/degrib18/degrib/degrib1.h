#ifndef DEGRIB1_H
#define DEGRIB1_H

#include <stdio.h>
#include "type.h"
#include "meta.h"
#include "degrib2.h"
#include "inventory.h"

typedef struct {
    const char *name, *comment, *unit;
    unit_convert convert;
} GRIB1ParmTable;

typedef struct {
   const char *name, *comment, *unit;
   char f_twoPart;
} GRIB1SurfTable;

extern const GRIB1ParmTable parm_table_ncep_opn[256];
extern const GRIB1ParmTable parm_table_ncep_reanal[256];
extern const GRIB1ParmTable parm_table_ncep_tdl[256];
extern const GRIB1ParmTable parm_table_ncep_mdl[256];
extern const GRIB1ParmTable parm_table_omb[256];
extern const GRIB1ParmTable parm_table_nceptab_129[256];
extern const GRIB1ParmTable parm_table_nceptab_130[256];
extern const GRIB1ParmTable parm_table_nceptab_131[256];

extern const GRIB1ParmTable parm_table_nohrsc[256];

extern const GRIB1ParmTable parm_table_cptec_254[256];

extern const GRIB1ParmTable parm_table_afwa_000[256];
extern const GRIB1ParmTable parm_table_afwa_001[256];
extern const GRIB1ParmTable parm_table_afwa_002[256];
extern const GRIB1ParmTable parm_table_afwa_003[256];
extern const GRIB1ParmTable parm_table_afwa_010[256];
extern const GRIB1ParmTable parm_table_afwa_011[256];

extern const GRIB1ParmTable parm_table_dwd_002[256];
extern const GRIB1ParmTable parm_table_dwd_201[256];
extern const GRIB1ParmTable parm_table_dwd_202[256];
extern const GRIB1ParmTable parm_table_dwd_203[256];

extern const GRIB1ParmTable parm_table_ecmwf_128[256];
extern const GRIB1ParmTable parm_table_ecmwf_129[256];
extern const GRIB1ParmTable parm_table_ecmwf_130[256];
extern const GRIB1ParmTable parm_table_ecmwf_131[256];
extern const GRIB1ParmTable parm_table_ecmwf_140[256];
extern const GRIB1ParmTable parm_table_ecmwf_150[256];
extern const GRIB1ParmTable parm_table_ecmwf_160[256];
extern const GRIB1ParmTable parm_table_ecmwf_170[256];
extern const GRIB1ParmTable parm_table_ecmwf_180[256];

extern const GRIB1ParmTable parm_table_athens[256];

extern const GRIB1ParmTable parm_table_cmc[256];

extern const GRIB1ParmTable parm_table_undefined[256];

extern const GRIB1SurfTable GRIB1Surface[256];


int GRIB1_Inventory (DataSource &fp, uInt4 gribLen, inventoryType * inv);

int GRIB1_RefTime (DataSource &fp, uInt4 gribLen, double *refTime);

int ReadGrib1Record (DataSource &fp, sChar f_unit, double **Grib_Data,
                     uInt4 *grib_DataLen, grib_MetaData * meta,
                     IS_dataType * IS, sInt4 sect0[SECT0LEN_WORD],
                     uInt4 gribLen, double majEarth, double minEarth);
#endif
