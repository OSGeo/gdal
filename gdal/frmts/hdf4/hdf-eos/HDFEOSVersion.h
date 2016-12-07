#include <stdlib.h>
 
/* define strings that will be displayed by the using the UNIX "what" command
   on a file containing these strings */
 
#define  HDFEOSd_BANNER  "@(#)## =================  HDFEOS  ================"
#ifdef __GNUC__
#define  HDFEOSd_HDFEOS_VER  "@(#)## HDFEOS Version: "HDFEOSVERSION1
/* #define  HDFEOSd_DATE    "@(#)## Build date: "__DATE__" @ "__TIME__ */
#else
#define  HDFEOSd_HDFEOS_VER  "@(#)## HDFEOS Version: "##HDFEOSVERSION1
/* #define  HDFEOSd_DATE    "@(#)## Build date: "##__DATE__##" @ "##__TIME__ */
#endif
 
const char *hdfeosg_LibraryVersionString01 = HDFEOSd_BANNER;
const char *hdfeosg_LibraryVersionString02 = HDFEOSd_HDFEOS_VER;
/*const char *hdfeosg_LibraryVersionString03 = HDFEOSd_DATE; */
const char *hdfeosg_LibraryVersionString04 = HDFEOSd_BANNER;
