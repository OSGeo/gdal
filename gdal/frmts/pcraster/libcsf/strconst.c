#include "csf.h"

/* global header (opt.) and strconst's prototypes "" */


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
static char errorBuf[64];
/******************/
/* IMPLEMENTATION */
/******************/

/* string with cell representation in plain english or acronym
 * The string is in lower case except for INT1,INT2,UINT2 and UINT4
 *, they return an acronym. If cr is not
 * a valid constant, for example 999,  then the string is  
 * "999 is no CR constant".
 * The "no constant" message is stored in a static buffer
 * used by both RstrCellRepr and RstrValueScale.
 * returns
 * string with cell representation
 */
const char *RstrCellRepr(CSF_CR cr) /* cell representation constant */
{
 switch(cr) {
	case CR_INT1  : return "INT1";
	case CR_INT2  : return "INT2";
	case CR_INT4  : return "large integer";
	case CR_UINT1 : return "small integer";
	case CR_UINT2 : return "UINT2";
	case CR_UINT4 : return "UINT4";
	case CR_REAL4 : return "small real";
	case CR_REAL8 : return "large real";
	default       : (void)sprintf(errorBuf,"%u is no CR constant", (unsigned)cr);
                        return errorBuf;
 }
}

/* string with value scale
 * The string is in lower case. If cr is not
 * a valid constant, for example 999,  then the string is  
 * "999 is no VS constant".
 * The "no constant" message is stored in a static buffer
 * used by both RstrCellRepr and RstrValueScale.
 * returns
 * string with value scale in lower case
 */
const char *RstrValueScale(CSF_VS vs) /* value scale constant */
{
 switch(vs) {
	case VS_NOTDETERMINED : return "notdetermined";
	case VS_CLASSIFIED    : return "classified";
	case VS_CONTINUOUS    : return "continuous";
	case VS_BOOLEAN       : return "boolean";
	case VS_NOMINAL       : return "nominal";
	case VS_ORDINAL       : return "ordinal";
	case VS_SCALAR        : return "scalar";
	case VS_DIRECTION     : return "directional";
	case VS_LDD           : return "ldd";
	default       : (void)sprintf(errorBuf,"%u is no VS constant", (unsigned)vs);
                        return errorBuf;
 }
}

/* string with projection 
 * The string is in lower case. 
 * string with name of projection 
 */
const char *MstrProjection(CSF_PT p) /* projection constant, 0 is
                                     * top to bottom. non-0 is bottom
                                     * to top
                                     */
{
 	return (p) ?
 	   "y increases from bottom to top"
 	  :"y increases from top to bottom";
}
