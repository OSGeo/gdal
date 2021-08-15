#include "csf.h"
#include "csfimpl.h"
#include <string.h>

static void UINT1tLdd(size_t nrCells, void *Buf)
{
	size_t i;
	UINT1 *buf = (UINT1 *)Buf;

	for(i=0; i < (size_t)nrCells; i++)
		if (buf[i] != MV_UINT1)
		{
			buf[i] %= (UINT1)10;
			if (buf[i] == 0)
				buf[i] = MV_UINT1;
		}
}

static void INT2tLdd(size_t nrCells, void *Buf)
{
	size_t i;
	INT2  *inBuf  = (INT2  *)Buf;
	UINT1 *outBuf = (UINT1 *)Buf;
	for(i=0; i < (size_t)nrCells; i++)
		if (inBuf[i] != MV_INT2)
		{
			outBuf[i] = (UINT1)(ABS(inBuf[i]) % 10);
			if (outBuf[i] == 0)
				outBuf[i] = MV_UINT1;
		}
		else
			outBuf[i] = MV_UINT1;
}

#define TOBOOL(nr, buf, srcType)\
{\
	size_t i;\
/* loop must be upward to prevent overwriting of values \
 * not yet converted \
 */ \
 	PRECOND(sizeof(srcType) >= sizeof(UINT1));\
	for(i=0; i < (size_t)nr; i++)\
		if (IS_MV_##srcType( ((srcType *)buf)+i) )\
			((UINT1 *)buf)[i] = MV_UINT1;\
		else\
			((UINT1 *)buf)[i] = ((srcType *)buf)[i] != ZERO_##srcType;\
}

static void INT1tBoolean(size_t nrCells, void *buf)
{ TOBOOL(nrCells, buf, INT1); }

static void INT2tBoolean(size_t nrCells, void *buf)
{ TOBOOL(nrCells, buf, INT2); }

static void INT4tBoolean(size_t nrCells, void *buf)
{ TOBOOL(nrCells, buf, INT4); }

static void UINT1tBoolean(size_t nrCells, void *buf)
{ TOBOOL(nrCells, buf, UINT1); }

static void UINT2tBoolean(size_t nrCells, void *buf)
{ TOBOOL(nrCells, buf, UINT2); }

static void UINT4tBoolean(size_t nrCells, void *buf)
{ TOBOOL(nrCells, buf, UINT4); }

static void REAL4tBoolean(size_t nrCells, void *buf)
{ TOBOOL(nrCells, buf, REAL4); }

static void REAL8tBoolean(size_t nrCells, void *buf)
{ TOBOOL(nrCells, buf, REAL8); }

#define CONV_BIG_TO_SMALL(nr, buf, destType, srcType)\
{\
	size_t i;\
/* loop must be upward to prevent overwriting of values \
 * not yet converted: \
 */ \
 	PRECOND(sizeof(srcType) >= sizeof(destType)); /* upward loop OK */ \
	for(i=0; i < (size_t)nr; i++) {\
		if (IS_MV_##srcType( ((srcType *)buf)+i) )\
		    SET_MV_##destType( ((destType *)buf)+i);\
		else {\
			destType CBTS_temp = (destType)(((srcType *)buf)[i]); \
			((destType *)buf)[i] = CBTS_temp; \
		} \
	} \
}

#define CONV_SMALL_TO_BIG(nr, buf, destType, srcType)\
{\
	size_t i = (size_t)nr;\
/* loop must be downward to prevent overwriting of values \
 * not yet converted: \
 */ \
 	PRECOND(sizeof(srcType) <= sizeof(destType)); /* downward loop OK */ \
	do { i--;\
		if (IS_MV_##srcType( ((srcType *)buf)+i) )\
		    SET_MV_##destType( ((destType *)buf)+i);\
		else {\
			destType CSTB_temp = (destType)(((srcType *)buf)[i]); \
			((destType *)buf)[i] = CSTB_temp; \
		} \
	}while ( i != 0);\
}

static void UINT1tINT4(size_t nrCells, void *buf)
{ CONV_SMALL_TO_BIG(nrCells, buf, INT4, UINT1);}

static void UINT1tREAL4(size_t nrCells, void *buf)
{ CONV_SMALL_TO_BIG(nrCells, buf, REAL4, UINT1);}

static void UINT1tREAL8(size_t nrCells, void *buf)
{ CONV_SMALL_TO_BIG(nrCells, buf, REAL8, UINT1);}

static void INT4tUINT1(size_t nrCells, void *buf)
{ CONV_BIG_TO_SMALL(nrCells, buf, UINT1, INT4);}

static void INT2tUINT1(size_t nrCells, void *buf)
{ CONV_BIG_TO_SMALL(nrCells, buf, UINT1, INT2);}

static void UINT2tUINT1(size_t nrCells, void *buf)
{ CONV_BIG_TO_SMALL(nrCells, buf, UINT1, UINT2);}

static void INT4tREAL4(size_t nrCells, void *buf)
{ CONV_BIG_TO_SMALL(nrCells, buf, REAL4, INT4);}

static void INT4tREAL8(size_t nrCells, void *buf)
{ CONV_SMALL_TO_BIG(nrCells, buf, REAL8, INT4);}

static void REAL4tUINT1(size_t nrCells, void *buf)
{ CONV_BIG_TO_SMALL(nrCells, buf, UINT1, REAL4);}

static void REAL4tINT4(size_t nrCells, void *buf)
{ CONV_BIG_TO_SMALL(nrCells, buf, INT4, REAL4);}

static void REAL4tREAL8(size_t nrCells, void *buf)
{ CONV_SMALL_TO_BIG(nrCells, buf, REAL8, REAL4);}

static void REAL8tUINT1(size_t nrCells, void *buf)
{ CONV_BIG_TO_SMALL(nrCells, buf, UINT1, REAL8);}

static void REAL8tINT4(size_t nrCells, void *buf)
{ CONV_BIG_TO_SMALL(nrCells, buf, INT4, REAL8);} 

static void REAL8tREAL4(size_t nrCells, void *buf)
{ CONV_BIG_TO_SMALL(nrCells, buf, REAL4, REAL8);}

static void Transform2( size_t nrCells, void *buf, CSF_CR destCellRepr, CSF_CR currCellRepr);

static void INT1tINT4(size_t nrCells, void *buf)
{ Transform2(nrCells, buf, CR_INT4, CR_INT1);}

static void INT1tREAL4(size_t nrCells, void *buf)
{ Transform2(nrCells, buf, CR_REAL4, CR_INT1);}

static void INT1tREAL8(size_t nrCells, void *buf)
{ Transform2(nrCells, buf, CR_REAL8, CR_INT1);}

static void INT2tINT4(size_t nrCells, void *buf)
{ Transform2(nrCells, buf, CR_INT4, CR_INT2);}

static void INT2tREAL4(size_t nrCells, void *buf)
{ Transform2(nrCells, buf, CR_REAL4, CR_INT2);}

static void INT2tREAL8(size_t nrCells, void *buf)
{ Transform2(nrCells, buf, CR_REAL8, CR_INT2);}

static void UINT2tINT4(size_t nrCells, void *buf)
{ Transform2(nrCells, buf, CR_INT4, CR_UINT2);}

static void UINT2tREAL4(size_t nrCells, void *buf)
{ Transform2(nrCells, buf, CR_REAL4, CR_UINT2);}

static void UINT2tREAL8(size_t nrCells, void *buf)
{ Transform2(nrCells, buf, CR_REAL8, CR_UINT2);}

static void UINT4tREAL4(size_t nrCells, void *buf)
{ Transform2(nrCells, buf, CR_REAL4, CR_UINT4);}

static void UINT4tREAL8(size_t nrCells, void *buf)
{ Transform2(nrCells, buf, CR_REAL8, CR_UINT4);}


static void ConvertToINT2( size_t nrCells, void *buf, CSF_CR src)
{
	if (IS_SIGNED(src))
	{
		POSTCOND(src == CR_INT1);
		CONV_SMALL_TO_BIG(nrCells,buf, INT2, INT1);
	}
	else  
	{
		POSTCOND(src == CR_UINT1);
		CONV_SMALL_TO_BIG(nrCells,buf, INT2, UINT1);
	}
}

static void ConvertToINT4( size_t nrCells, void *buf, CSF_CR src)
{
	if (IS_SIGNED(src))
	{
		POSTCOND(src == CR_INT2);
		CONV_SMALL_TO_BIG(nrCells,buf, INT4, INT2);
	}
	else  
	{
		POSTCOND(src == CR_UINT2);
		CONV_SMALL_TO_BIG(nrCells,buf, INT4, UINT2);
	}
}


static void UINT1tUINT2(
size_t nrCells,
void *buf)
{
	CONV_SMALL_TO_BIG(nrCells, buf, UINT2, UINT1);
}

static void UINT2tUINT4(
size_t nrCells,
void *buf)     
{
	CONV_SMALL_TO_BIG(nrCells, buf, UINT4, UINT2);
}

static void ConvertToREAL4( size_t nrCells, void *buf, CSF_CR src)
{
	size_t i;

	i = (size_t)nrCells;

	if (IS_SIGNED(src))
	{
		POSTCOND(src == CR_INT4);
		INT4tREAL4(nrCells, buf);
	}
	else
	{
		POSTCOND(src == CR_UINT4);
		{
			do {
				i--;
				if ( ((UINT4 *)buf)[i] == MV_UINT4 )
					((UINT4 *)buf)[i] = MV_UINT4;
				else {
					UINT4 u = ((UINT4 *)buf)[i];
					REAL4 f = (REAL4)u;
					memcpy(&((REAL4 *)buf)[i], &f, sizeof(f));
				}
			}
			while(i != 0);
		}
	}
}

static void Transform2(
	size_t nrCells,
	void  *buf,
	CSF_CR destCellRepr, /* the output representation */
	CSF_CR currCellRepr)  /* at start of while this is the representation
				read in the MAP-file */
{
  /* subsequent looping changes the to the new
   * converted type
   */
	while(currCellRepr != destCellRepr)
	{
		switch(currCellRepr)
		{
			case CR_INT1:    ConvertToINT2(nrCells, buf,
						currCellRepr);
					currCellRepr = CR_INT2;
					break;
			case CR_INT2:    ConvertToINT4(nrCells, buf, 
						currCellRepr);
					currCellRepr = CR_INT4;
					break;
			case CR_INT4:    ConvertToREAL4(nrCells, buf,
						currCellRepr);
					currCellRepr = CR_REAL4;
					break;
			case CR_UINT1: 	if (IS_SIGNED(destCellRepr))
					{
						ConvertToINT2(nrCells, buf,
							currCellRepr);
						currCellRepr = CR_INT2;
					}
					else
					{
						UINT1tUINT2(nrCells, buf);
						currCellRepr = CR_UINT2;
					}
					break;
			case CR_UINT2:   if (destCellRepr == CR_INT4)
					{
						ConvertToINT4(nrCells, buf,
							currCellRepr);
					 	currCellRepr = CR_INT4;
					 }
					 else
					 {
						UINT2tUINT4(nrCells, buf);
						currCellRepr = CR_UINT4;
					 }
					 break;
			case CR_UINT4:   ConvertToREAL4(nrCells, buf,
						currCellRepr);
					currCellRepr = CR_REAL4;
					break;
			default       :	POSTCOND(currCellRepr == CR_REAL4);
					REAL4tREAL8(nrCells, buf);
					currCellRepr = CR_REAL8;
		}
	}
}

/* OLD STUFF 
void TransForm(
	const MAP *map,
	size_t nrCells,
	void *buf)
{
	Transform2(nrCells, buf, map->types[READ_AS], map->types[STORED_AS]);
}
*/

#define illegal NULL
#define same    CsfDummyConversion

static const CSF_CONV_FUNC ConvTable[8][8] = {
/* ConvTable[source][destination] */
/* INT1     , INT2      , INT4      , UINT1     , UINT2     , UINT4     , REAL4     , REAL8        */
/* 0        , 1         , 2         , 3         , 4         , 5         , 6         , 7      ind   */
/* 0x04     , 0x05      , 0x06      , 0x00      , 0x01      , 0x02      , 0x0A      , 0x0B low-nib */
{  same     , illegal   , INT1tINT4 , illegal   , illegal   , illegal   , INT1tREAL4, INT1tREAL8}, /* INT1 */
{ illegal   ,  same     , INT2tINT4 ,INT2tUINT1 , illegal   , illegal   , INT2tREAL4, INT2tREAL8}, /* INT2 */
{ illegal   , illegal   ,  same     ,INT4tUINT1 , illegal   , illegal   , INT4tREAL4, INT4tREAL8}, /* INT4 */
{ illegal   , illegal   ,UINT1tINT4 ,  same     ,UINT1tUINT2, illegal   ,UINT1tREAL4,UINT1tREAL8}, /* UINT1 */
{ illegal   , illegal   ,UINT2tINT4 , UINT2tUINT1 ,  same     ,UINT2tUINT4,UINT2tREAL4,UINT2tREAL8}, /* UINT2 */
{ illegal   , illegal   , illegal   , illegal   , illegal   ,  same     ,UINT4tREAL4,UINT4tREAL8}, /* UINT4 */
{ illegal   , illegal   ,REAL4tINT4 ,REAL4tUINT1, illegal   , illegal   ,  same     ,REAL4tREAL8}, /* REAL4 */
{ illegal   , illegal   ,REAL8tINT4 ,REAL8tUINT1, illegal   , illegal   ,REAL8tREAL4,  same     }  /* REAL8 */
};

static const CSF_CONV_FUNC boolConvTable[8] = {
INT1tBoolean, INT2tBoolean, INT4tBoolean,
UINT1tBoolean, UINT2tBoolean, UINT4tBoolean,
REAL4tBoolean, REAL8tBoolean };


static const char  convTableIndex[12] = {
	   3 /* UINT1 */,  4 /* UINT2 */,  5 /* UINT4 */, -1 /* 0x03  */,
	   0 /*  INT1 */,  1 /*  INT2 */,  2 /*  INT4 */, -1 /* 0x07  */,
	  -1 /*  0x08 */, -1 /*  0x09 */,  6 /* REAL4 */,  7 /* REAL8 */
};

static CSF_CONV_FUNC ConvFuncBool(CSF_CR cr)
{
	PRECOND(CSF_UNIQ_CR_MASK(cr) < 12);
	PRECOND(convTableIndex[CSF_UNIQ_CR_MASK(cr)] != -1);
	
	return boolConvTable[(int)convTableIndex[CSF_UNIQ_CR_MASK(cr)]];
}

static CSF_CONV_FUNC ConvFunc(CSF_CR destType, CSF_CR srcType)
{

	PRECOND(CSF_UNIQ_CR_MASK(destType) < 12);
	PRECOND(CSF_UNIQ_CR_MASK(srcType) < 12);
	PRECOND(convTableIndex[CSF_UNIQ_CR_MASK(srcType)] != -1);
	PRECOND(convTableIndex[CSF_UNIQ_CR_MASK(destType)] != -1);
	/* don't complain on illegal, it can be attached
	 * to a app2file while there's no WRITE_MODE
	 * if it is an error then it is cached in RputSomeCells
	 */
	return 
         ConvTable[(int)convTableIndex[CSF_UNIQ_CR_MASK(srcType)]]
 	          [(int)convTableIndex[CSF_UNIQ_CR_MASK(destType)]];
}

static int HasInFileCellReprType2(CSF_CR cr)
{
	char  type2[12] = {
	   1 /* UINT1 */,  0 /* UINT2 */,  0 /* UINT4 */,  0 /* 0x03  */,
	   0 /*  INT1 */,  0 /*  INT2 */,  1 /*  INT4 */,  0 /* 0x07  */,
	   0 /*  0x08 */,  0 /*  0x09 */,  1 /* REAL4 */,  1 /* REAL8 */};

	PRECOND(CSF_UNIQ_CR_MASK(cr) < 12);
	
	return (int)type2[CSF_UNIQ_CR_MASK(cr)];
}

/* set the cell representation the application will use
 * RuseAs enables an application to use cell values
 * in a different format than they are stored in the map.
 * Cell values are converted when getting (Rget*-functions) and
 * putting (Rput*-functions) cells if necessary.
 * Thus no conversions are applied if cell representation and/or 
 * value scale already match.
 * Any conversions between the version 2 cell representations, 
 * (CR_UINT1, CR_INT4, CR_REAL4 and CR_REAL8) is possible. 
 * Conversion from a non version 2 cell representation to a version
 * 2 cell representation is only possible when you don't
 * have write access to the cells.
 * Conversion rules are exactly as described in K&R 2nd edition section A6.
 * 
 * Two special conversions are possible if you don't
 * have write access to the cells or if the in-file cell representation is
 * UINT1:
 * (1) VS_BOOLEAN: successive calls to the Rget*-functions returns the result of
 * value != 0 
 * , that is 0 or 1 in UINT1 format. The in-file cell representation can be
 * anything, except if the value scale is VS_DIRECTION or VS_LDD.
 * (2) VS_LDD: successive calls to the Rget*-functions returns the result of
 * value % 10 
 * , that is 1 to 9 in UINT1 format (0's are set to MV_UINT1). 
 * The in-file cell representation must be CR_UINT1 or CR_INT2 and 
 * the value scale must be VS_LDD, VS_CLASSIFIED or VS_NOTDETERMINED.
 *
 * NOTE
 * that you must use Rmalloc() to get enough space for both the in-file and
 * app cell representation. 
 *
 * returns 
 * 0 if conversion obeys rules given here. 1 if not (conversions 
 * will not take place).
 *
 * Merrno
 * CANT_USE_AS_BOOLEAN CANT_USE_WRITE_BOOLEAN
 * CANT_USE_WRITE_LDD
 * CANT_USE_AS_LDD
 * CANT_USE_WRITE_OLDCR
 * ILLEGAL_USE_TYPE
 *
 * EXAMPLE
 * .so examples/maskdump.tr
 */

int RuseAs(
	MAP *m,          /* map handle */
	CSF_CR useType)   /* CR_UINT1,CR_INT4, CR_REAL4, CR_REAL8, VS_BOOLEAN or VS_LDD */
{ 

  CSF_CR inFileCR = RgetCellRepr(m);
  CSF_VS inFileVS = RgetValueScale(m);
  int hasInFileCellReprType2 =  HasInFileCellReprType2(inFileCR);
  int useTypeNoEnumIn;
  int useTypeNoEnum;

  /* It is very inconvenient that both, VS and CR are taken as arguments
   * for this function, and previously were used in the switch statement
   * now, at least 'special conversions' handled first.
   */

  /* Hopefully Coverity will no longer detect that useTypeNoEnum */
  /* comes from useType */
  useTypeNoEnumIn = useType;
  memcpy(&useTypeNoEnum, &useTypeNoEnumIn, sizeof(int));

  if(useTypeNoEnum == VS_BOOLEAN){
    switch(inFileVS) {
      case VS_LDD:
      case VS_DIRECTION: {
        M_ERROR(CANT_USE_AS_BOOLEAN);
        return 1;
      }
      case VS_BOOLEAN: {
        POSTCOND(inFileCR == CR_UINT1);
        m->appCR = CR_UINT1;
        m->file2app = same;
        m->app2file = same;
        return 0;
      }
      default: {
        if((!hasInFileCellReprType2) && WRITE_ENABLE(m)) {
          /* cellrepr is old one, we can't write that */
          M_ERROR(CANT_USE_WRITE_BOOLEAN);
          return 1;
        }
        m->appCR = CR_UINT1;
        m->file2app  = ConvFuncBool(inFileCR);
        m->app2file = ConvFunc(inFileCR, CR_UINT1);
        return 0;
      }
    }
  }
  else if (useTypeNoEnum == VS_LDD){
    switch(inFileVS) {
      case VS_LDD: {
        POSTCOND(inFileCR == CR_UINT1);
        m->appCR = CR_UINT1;
        m->file2app = same;
        m->app2file = same;
        return 0;
      }
      case VS_CLASSIFIED: 
      case VS_NOTDETERMINED: {
        switch(inFileCR) {
          case CR_UINT1: {
            m->appCR = CR_UINT1;
            m->file2app  = UINT1tLdd;
            m->app2file = same;
            return 0;
          }
          case CR_INT2: {
            if(WRITE_ENABLE(m)) {
              M_ERROR(CANT_USE_WRITE_LDD);
              return 1;
            }
            m->appCR = CR_UINT1;
            m->file2app  = INT2tLdd;
            m->app2file = illegal;
            return 0;
          }
          default: {
            /* This should never happen.
             * Shut up compiler.
             */
            assert(0);
          }
        }
      }
      default: {
        M_ERROR(CANT_USE_AS_LDD);
        return 1;
      }
    }
  }

  switch(useType) {
    case CR_UINT1:
    case CR_INT4 :
    case CR_REAL4:
    case CR_REAL8: {
      if((!hasInFileCellReprType2) && WRITE_ENABLE(m)) {
        /* cellrepr is old one, we can't write that */
        M_ERROR(CANT_USE_WRITE_OLDCR);
        return 1;
      }
      m->appCR = useType;
      m->file2app  = ConvFunc(useType, inFileCR);
      m->app2file = ConvFunc(inFileCR, useType);
      POSTCOND(m->file2app != NULL);
      return 0;
    }
    default: {
      M_ERROR(ILLEGAL_USE_TYPE);
      return 1;
    }
  }
  /* NOTREACHED */
}
