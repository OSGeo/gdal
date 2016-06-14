#include "csf.h"
#include "csfimpl.h"

/* test if a value is missing value
 * returns 0 if not, nonzero if it is a missing value
 */
int  IsMV(
	const MAP *map, /* map handle */
	const void *cellValue) /* value to be tested */
{
	return(IsMVcellRepr(map->appCR, cellValue));
}

/* test if a value is missing value
 * returns 0 if not, nonzero if it is a missing value
 */
int  IsMVcellRepr(
	CSF_CR cellRepr,        /* cell representation of argument cellValue.
	                        * That is one of the constants prefixed by CR_.
	                        */
	const void *cellValue) /* value to be tested */
{

	if (IS_SIGNED(cellRepr))
	 	switch ( (cellRepr & CSF_SIZE_MV_MASK ) >> CSF_POS_SIZE_MV_MASK)
		{
		case 0:	return(*((const INT1 *)cellValue) == MV_INT1);
		case 1:	return(*((const INT2 *)cellValue) == MV_INT2);
		default:return(*((const INT4 *)cellValue) == MV_INT4);
		}
	else
		if (IS_REAL(cellRepr))
		{
			if (cellRepr == CR_REAL4)
				return(IS_MV_REAL4(cellValue));
			else
				return(IS_MV_REAL8(cellValue));
		}
		else
		{
			switch ( (cellRepr & CSF_SIZE_MV_MASK ) >> CSF_POS_SIZE_MV_MASK)
			{
			case 0:    return(*((const UINT1 *)cellValue) == MV_UINT1);
			case 1:	   return(*((const UINT2 *)cellValue) == MV_UINT2);
			default:   return(*((const UINT4 *)cellValue) == MV_UINT4);
			}
		}
}
