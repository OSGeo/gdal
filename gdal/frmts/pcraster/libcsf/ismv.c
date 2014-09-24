/*
 * ismv.c
$Log$
Revision 1.3  2006/02/07 10:17:15  kdejong
Fixed endian compile problem
some rcs issues of Kor, I guess
Checked in by cees (cees@pcraster.nl) on account of Kor

Revision 1.2  2005/10/03 07:23:00  kor
Removed rcs id string

Revision 1.1.1.1  2000/01/04 21:04:44  cees
Initial import Cees

Revision 2.0  1996/05/23 13:16:26  cees
csf2clean

Revision 1.1  1996/05/23 13:11:49  cees
Initial revision

Revision 1.3  1995/11/01 17:23:03  cees
.

 * Revision 1.2  1994/09/05  13:27:26  cees
 * changed for appCR field
 *
 * Revision 1.1  1994/08/26  13:33:23  cees
 * Initial revision
 *
 */
/********************************************************/
/*							*/
/*	IsMV						*/
/*							*/
/********************************************************/
/* check if cellValue is a missing value		*/
/********************************************************/


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
