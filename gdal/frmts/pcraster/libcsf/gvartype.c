#include "csf.h"
#include "csfimpl.h"

/* perform a simple byte-copy of 8,4,2 or 1 byte (LIBRARY_INTERNAL)
 */
void CsfGetVarType( void *dest, const CSF_VAR_TYPE *src, CSF_CR cellRepr)
{
	switch (LOG_CELLSIZE(cellRepr))  /* 2log size */
	{
		case 3 : ((UINT4 *)dest)[1] = ((const UINT4 *)src)[1];
		         ((UINT4 *)dest)[0] = ((const UINT4 *)src)[0];
			 break;
		case 2 : (*(UINT4 *)dest)    = (*(const UINT4 *)src);
			break;
		case 1 : (*(UINT2 *)dest)    = (*(const UINT2 *)src);
			break;
		default: POSTCOND(LOG_CELLSIZE(cellRepr) == 0);
			 (*(UINT1 *)dest)    = (*(const UINT1 *)src);
	}
}
