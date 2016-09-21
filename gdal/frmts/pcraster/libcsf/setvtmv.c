#include "csf.h"
#include "csfimpl.h"

/* (LIBRARY_INTERNAL)
 */
void CsfSetVarTypeMV( CSF_VAR_TYPE *var, CSF_CR cellRepr)
{
/* assuming unions are left-alligned */
	if(IS_SIGNED(cellRepr))
		switch(LOG_CELLSIZE(cellRepr))
		{
		 case 2	:	*(INT4 *)var = MV_INT4;
		 		break;
		 case 1	:	*(INT2 *)var = MV_INT2;
		 		break;
		 default:	POSTCOND(LOG_CELLSIZE(cellRepr) == 0);
		 		*(INT1 *)var = MV_INT1;
		}
	else
	{
		((UINT4 *)var)[0] = MV_UINT4;
		((UINT4 *)var)[1] = MV_UINT4;
	}
}
