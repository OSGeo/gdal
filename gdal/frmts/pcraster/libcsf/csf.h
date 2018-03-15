#ifndef INCLUDED_CSF
#define INCLUDED_CSF

#ifdef CSF_V1
# error new include file used while CSF_V1 is defined
#endif

#ifndef INCLUDED_CSFTYPES
#include "csftypes.h"
#define INCLUDED_CSFTYPES
#endif

#ifdef __cplusplus
 extern "C" {
#endif 



#include <stdio.h>
#include "csfattr.h"


/*****************************************************************/
/*                                                               */
/*  RUU CROSS SYSTEM MAP FORMAT                                  */
/*  VERSION 2                                                    */
/*****************************************************************/

extern int Merrno; /* declared in csfglob.c */

/* CSF_VAR_TYPE can hold every possible */
/* data type                        */
typedef REAL8 CSF_VAR_TYPE;

/* values for CSF_MAIN_HEADER.mapType :  */
#define T_RASTER 1

/* CSF_FADDR can hold any location in the file 
 * CSF_FADDR is always an offset from the begin 
 * (0) of the file                                
 */
typedef UINT4 CSF_FADDR32;
#ifdef _WIN32
  /* long is 32 bit on Windows */
  typedef __int64 CSF_FADDR;
#else
  typedef long    CSF_FADDR;
#endif

/* value for first 27 bytes of MAIN_HEADER.signature */
#define CSF_SIG  "RUU CROSS SYSTEM MAP FORMAT"
#define CSF_SIZE_SIG (sizeof(CSF_SIG)-1)

#define CSF_SIG_SPACE ((size_t)32)

typedef struct CSF_MAIN_HEADER
{
 char        signature[CSF_SIG_SPACE];
 UINT2       version;
 UINT4       gisFileId;
 UINT2       projection;
 CSF_FADDR32 attrTable;
 UINT2       mapType;
 UINT4       byteOrder;
} CSF_MAIN_HEADER;

/******************************************************************/
/* Definition of the second header                                */
/******************************************************************/
/* CSF_MAIN_HEADER.mapType decides which structure is                */
/* used as second header                                          */
/******************************************************************/
/******************************************************************/
/* Definition of the raster header                                */
/******************************************************************/
typedef struct CSF_RASTER_HEADER
{
   /* see #def's of VS_*
    */
   UINT2    valueScale;
   /* see #def's of CR_*
    */
   UINT2    cellRepr;

  /* minVal holds a value equal or less than the
   * minimum value in the cell matrix
   */
   CSF_VAR_TYPE minVal;

  /* maxVal holds a value equal or greater than the
   * maximum value in the cell matrix
   */
   CSF_VAR_TYPE maxVal;

  /* co-ordinate of upper left corner
   */
   REAL8    xUL;
   REAL8    yUL;

   UINT4    nrRows;
   UINT4    nrCols;

  /* CSF version 1 problem: X and Y cellsize 
   * could differ, no longer the case
   * even though cellSizeX and cellSizeY
   * are stored separate, they should be equal
   * all apps. are working with square pixels
   */
   REAL8    cellSize;     /* was cellSizeX */
   REAL8    cellSizeDupl; /* was cellSizeY */

  /* new in version 2
   * rotation angle of grid
   */
   REAL8    angle;

  /* remainder is not part of
   * file header 
   */
  /* cosine and sine of
   * the angle are computed
   * when opening or creating
   * the file
   */
  REAL8  angleCos;
  REAL8  angleSin;
  CSF_PT projection;   /* copy of main header */
} CSF_RASTER_HEADER;

/*******************************************************************/
/*  mode values               */
/*******************************************************************/

/* bit-mapped values: */
enum MOPEN_PERM {
  M_READ=1,      /* open read only */
  M_WRITE=2,     /* open write only */
  M_READ_WRITE=3 /* open for both reading and writing */
};



/****************************************************************/
/* Error listing return messages              */
/****************************************************************/

/* values for errolist  */
/* happens frequently 
 * assure 0 value
 * bogs on mingw
 * # if NOERROR != 0
 * #  error EXPECT NOERROR TO BE 0
 */
# define NOERROR         0

#define OPENFAILED       1
#define NOT_CSF          2
#define BAD_VERSION      3
#define BAD_BYTEORDER    4
#define NOCORE           5
#define BAD_CELLREPR     6
#define NOACCESS         7
#define ROWNR2BIG        8
#define COLNR2BIG        9
#define NOT_RASTER      10
#define BAD_CONVERSION  11
#define NOSPACE         12
#define WRITE_ERROR     13
#define ILLHANDLE       14
#define READ_ERROR      15
#define BADACCESMODE    16
#define ATTRNOTFOUND    17
#define ATTRDUPL        18
#define ILL_CELLSIZE    19
#define CONFL_CELLREPR  20
#define BAD_VALUESCALE  21
#define XXXXXXXXXXXX    22
#define BAD_ANGLE       23
#define CANT_USE_AS_BOOLEAN    24
#define CANT_USE_WRITE_BOOLEAN 25
#define CANT_USE_WRITE_LDD     26
#define CANT_USE_AS_LDD        27
#define CANT_USE_WRITE_OLDCR   28
#define ILLEGAL_USE_TYPE       29
/* number of errors  */
#define ERRORNO                30

typedef void (*CSF_CONV_FUNC)(size_t, void *);
/* conversion function for reading
 * and writing
 */
typedef size_t (*CSF_WRITE_FUNC)(void *buf, size_t size, size_t n, FILE  *f);
typedef size_t (*CSF_READ_FUNC)(void *buf, size_t size, size_t n, FILE *f);

typedef struct MAP
{
  CSF_CONV_FUNC file2app;
  CSF_CONV_FUNC app2file;
     UINT2 appCR;
  CSF_MAIN_HEADER main;
  CSF_RASTER_HEADER raster;
  char *fileName;
  FILE *fp;
  int fileAccessMode;
  int mapListId;
  UINT2 minMaxStatus;

  CSF_WRITE_FUNC write;
  CSF_READ_FUNC read;
}MAP;

typedef CSF_RASTER_HEADER CSF_RASTER_LOCATION_ATTRIBUTES;


/************************************************************/
/*                  */
/*  PROTOTYPES OF  RUU CSF            */
/*                  */
/************************************************************/

MAP *Rcreate(const char *fileName, 
         size_t nrRows, size_t nrCols, 
         CSF_CR cellRepr, CSF_VS dataType, 
         CSF_PT projection, REAL8 xUL, REAL8 yUL, REAL8 angle, REAL8 cellSize);
MAP  *Rdup(const char *toFile , const MAP *from, 
           CSF_CR cellRepr, CSF_VS dataType);
void *Rmalloc(const MAP *m, size_t nrOfCells);
int RuseAs(MAP *m, CSF_CR useType);

MAP  *Mopen(const char *fname, enum MOPEN_PERM mode);
enum MOPEN_PERM MopenPerm(const MAP *m);

int Rcompare(const MAP *m1,const MAP *m2);
int RgetLocationAttributes(
  CSF_RASTER_LOCATION_ATTRIBUTES *l, /* fill in this struct */
  const MAP *m); /* map handle to copy from */
int RcompareLocationAttributes(
  const CSF_RASTER_LOCATION_ATTRIBUTES *m1, /* */
  const CSF_RASTER_LOCATION_ATTRIBUTES *m2); /* */

int   Mclose(MAP *map);
void  Merror(int nr);
void  Mperror(const char *userString);
void  MperrorExit(const char *userString, int exitCode);
const char *MstrError(void);
const char *MgetFileName(const MAP *m);
void  ResetMerrno(void);

int    RputAllMV(MAP *newMap);
size_t  RputRow(MAP *map, size_t rowNr, void *buf);
size_t  RputSomeCells (MAP *map, size_t somePlace, size_t nrCells, void *buf);
size_t  RputCell(MAP *map, size_t rowNr, size_t colNr, void *cellValue);
size_t  RgetRow(MAP *map, size_t rowNr, void *buf);
size_t  RgetSomeCells (MAP *map, size_t somePlace, size_t nrCells, void *buf);
size_t  RgetCell(MAP *map, size_t rowNr, size_t colNr, void *cellValue);

int    RputDoNotChangeValues(const MAP *map);
int    MnativeEndian(const MAP *map);


UINT4  MgetMapDataType(const MAP *map);
UINT4  MgetVersion(const MAP *map);
UINT4  MgetGisFileId(const MAP *map);
UINT4  MputGisFileId(MAP *map,UINT4 gisFileId);
int    IsMVcellRepr(CSF_CR cellRepr, const void *cellValue);
int    IsMV(const MAP *map, const void *cellValue);
CSF_VS RgetValueScale(const MAP *map);
CSF_VS RputValueScale(MAP *map, CSF_VS valueScale);
int    RvalueScaleIs(const MAP *m, CSF_VS vs);
int    RvalueScale2(CSF_VS vs);
CSF_CR RdefaultCellRepr(CSF_VS vs);
CSF_CR RgetCellRepr(const MAP *map);
CSF_CR RgetUseCellRepr(const MAP *map);

int    RgetMinVal(const MAP *map, void *minVal);
int    RgetMaxVal(const MAP *map, void *maxVal);
void   RputMinVal(MAP *map, const void *minVal);
void   RputMaxVal(MAP *map, const void *maxVal);
void   RdontTrackMinMax(MAP *m);

REAL8  RgetXUL(const MAP *map);
REAL8  RgetYUL(const MAP *map);
REAL8  RputXUL(MAP *map, REAL8 xUL);
REAL8  RputYUL(MAP *map, REAL8 yUL);

/* old names: 
 */
#define RgetX0 RgetXUL
#define RgetY0 RgetYUL
#define RputX0 RputXUL
#define RputY0 RputYUL
 

REAL8  RgetAngle(const MAP *map);
REAL8  RputAngle(MAP *map, REAL8 Angle);

size_t  RgetNrCols(const MAP *map);
size_t  RgetNrRows(const MAP *map);

REAL8  RgetCellSize(const MAP *map);
REAL8  RputCellSize(MAP *map, REAL8 newCellSize);

int RgetCoords( const MAP *m, int inCelPos, size_t row, size_t col, double *x, double *y);
int RrowCol2Coords(const MAP *m, double row, double col, double *x, double *y);
void RasterRowCol2Coords(const CSF_RASTER_LOCATION_ATTRIBUTES *m,
double row, double col, double *x, double *y);

CSF_PT MgetProjection(const MAP *map);
CSF_PT MputProjection(MAP *map, CSF_PT p);

void   SetMV(const MAP *m, void *cell);
void   SetMVcellRepr(CSF_CR cellRepr, void *cell);
void   SetMemMV(void *dest,size_t nrElements,CSF_CR type);
/* historical error, implemented twice 
 *  SetArrayMV => SetMemMV
 */

int MattributeAvail(MAP *m, CSF_ATTR_ID id);
CSF_ATTR_ID MdelAttribute(MAP *m, CSF_ATTR_ID id);


size_t MgetNrLegendEntries(MAP *m);
int MgetLegend(MAP *m, CSF_LEGEND *l);
int MputLegend(MAP *m, CSF_LEGEND *l, size_t nrEntries);

size_t MgetHistorySize(MAP *m);
size_t MgetDescriptionSize(MAP *m);
size_t MgetNrColourPaletteEntries(MAP *m);
size_t MgetNrGreyPaletteEntries(MAP *m);
int MgetDescription(MAP *m, char *des);
int MgetHistory(MAP *m, char *history);
int MgetColourPalette(MAP *m, UINT2 *pal);
int MgetGreyPalette(MAP *m, UINT2 *pal);
int MputDescription(MAP *m, char *des);
int MputHistory(MAP *m, char *history);
int MputColourPalette(MAP *m, UINT2 *pal, size_t nrTupels);
int MputGreyPalette(MAP *m, UINT2 *pal, size_t nrTupels);

int Rcoords2RowCol( const MAP *m,
  double x,   double y,  
  double *row, double *col);
void RasterCoords2RowCol( const CSF_RASTER_LOCATION_ATTRIBUTES *m,
  double x,   double y,  
  double *row, double *col);
int RasterCoords2RowColChecked( const CSF_RASTER_LOCATION_ATTRIBUTES *m,
  double x,   double y,  
  double *row, double *col);

int RgetRowCol(const MAP *m,
  double x,   double y,  
  size_t *row, size_t *col);

const char *RstrCellRepr(CSF_CR cr);
const char *RstrValueScale(CSF_VS vs);
const char *MstrProjection(CSF_PT p);
int   RgetValueScaleVersion(const MAP *m);

void RcomputeExtend(REAL8 *xUL, REAL8 *yUL, size_t *nrRows, size_t *nrCols, 
 double x_1, double y_1, double x_2, double y_2, CSF_PT projection, REAL8 cellSize, double rounding);

#ifdef __cplusplus
 }
#endif 

/* INCLUDED_CSF */
#endif 
