#include "csf.h" 
#include "csfimpl.h" 


/* headers of this app. modules called */ 

/***************/
/* EXTERNALS   */
/***************/

/**********************/ 
/* LOCAL DECLARATIONS */
/**********************/ 

/*********************/ 
/* LOCAL DEFINITIONS */
/*********************/ 

/******************/
/* IMPLEMENTATION */
/******************/

/* returns default cell representation of a value scale/cellRepr
 * returns
 *  the appropriate cell representation constant (CR_something)
 *  or CR_UNDEFINED if vs is not a csf2 datatype
 *
 */
CSF_CR RdefaultCellRepr(
	CSF_VS     vs) /* value scale */
{
	switch(vs) {
	 case VS_LDD: 
	 case VS_BOOLEAN: return CR_UINT1;
	 case VS_NOMINAL:
	 case VS_ORDINAL: return CR_INT4;
	 case VS_SCALAR: 
	 case VS_DIRECTION: return CR_REAL4;
         case VS_CLASSIFIED: return CR_UINT1;
         case VS_CONTINUOUS: return CR_REAL4;
         case VS_NOTDETERMINED:
         default:
         	 return  CR_UNDEFINED;
      }
}
