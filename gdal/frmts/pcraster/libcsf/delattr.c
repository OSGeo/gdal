/*
 * delattr.c
$Log$
Revision 1.3  2006/02/07 10:17:15  kdejong
Fixed endian compile problem
some rcs issues of Kor, I guess
Checked in by cees (cees@pcraster.nl) on account of Kor

Revision 1.2  2005/10/03 07:23:00  kor
Removed rcs id string

Revision 1.1.1.1  2000/01/04 21:04:36  cees
Initial import Cees

Revision 2.1  1996/12/29 19:35:21  cees
src tree clean up

Revision 2.0  1996/05/23 13:16:26  cees
csf2clean

Revision 1.1  1996/05/23 13:11:49  cees
Initial revision

Revision 1.3  1995/11/01 17:23:03  cees
.

 * Revision 1.2  1994/09/08  17:16:23  cees
 * added c2man docs + small code changes
 *
 * Revision 1.1  1994/08/26  13:33:23  cees
 * Initial revision
 *
 */
#include "csf.h"
#include "csfimpl.h"

/* delete attribute from map
 * MdelAttribute deletes an attribute
 * from a map, if the attribute is available.
 * returns
 * the id argument if the attribute is succesfully deleted,
 * or 0 in case of error or if the attribute is not found.
 *
 * Merrno
 * NOACCESS
 * WRITE_ERROR
 */
CSF_ATTR_ID MdelAttribute(
	MAP *m,     /* map handle */
	CSF_ATTR_ID id)   /* identification of attribute */
{
	ATTR_CNTRL_BLOCK b;
	CSF_FADDR pos;

	if (! WRITE_ENABLE(m))
	{
		M_ERROR(NOACCESS);
		goto error;
	}

	pos = CsfGetAttrBlock(m, id, &b);  
	if (pos == 0)
		goto error;

	b.attrs[CsfGetAttrIndex(id, &b)].attrId = ATTR_NOT_USED;
	if (CsfWriteAttrBlock(m, pos, &b))
	{
		M_ERROR(WRITE_ERROR);
		goto error;
	}

	return id ;

error:	return 0 ;	/* not found or an error */
}
