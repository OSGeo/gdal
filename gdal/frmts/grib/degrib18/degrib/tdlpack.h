#ifndef TDLPACK_H
#define TDLPACK_H

#include <stdio.h>
#include "inventory.h"
#include "degrib2.h"

#ifndef GRIB2BIT_ENUM
#define GRIB2BIT_ENUM
/* See rule (8) bit 1 is most significant, bit 8 least significant. */
enum {GRIB2BIT_1=128, GRIB2BIT_2=64, GRIB2BIT_3=32, GRIB2BIT_4=16,
      GRIB2BIT_5=8, GRIB2BIT_6=4, GRIB2BIT_7=2, GRIB2BIT_8=1};
#endif

typedef struct {
   int index;
   const char *data;
} TDLP_TableType;

int TDLP_Inventory (DataSource &fp, sInt4 tdlpLen, inventoryType * inv);
int TDLP_RefTime (DataSource &fp, sInt4 tdlpLen, double * refTime);
int ReadTDLPRecord (DataSource &fp, double **TDLP_Data, uInt4 *tdlp_DataLen,
                    grib_MetaData * meta, IS_dataType * IS,
                    sInt4 sect0[SECT0LEN_WORD], uInt4 tdlpLen,
                    double majEarth, double minEarth);
void PrintPDS_TDLP (pdsTDLPType * pds);
int WriteTDLPRecord (FILE * fp, double *Data, sInt4 DataLen,
                     int DSF, int BSF, char f_primMiss, double primMiss,
                     char f_secMiss, double secMiss, gdsType *gds,
                     char *comment, double refTime, sInt4 ID1,
                     sInt4 ID2, sInt4 ID3, sInt4 ID4,
                     sInt4 projSec, sInt4 processNum, sInt4 seqNum);

#endif
