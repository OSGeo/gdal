/*
 * gattrblk.c
$Log$
Revision 1.3  2006/02/07 10:17:15  kdejong
Fixed endian compile problem
some rcs issues of Kor, I guess
Checked in by cees (cees@pcraster.nl) on account of Kor

Revision 1.2  2005/10/03 07:23:00  kor
Removed rcs id string

Revision 1.1.1.1  2000/01/04 21:04:37  cees
Initial import Cees

Revision 2.1  1996/12/29 19:35:21  cees
src tree clean up

Revision 2.0  1996/05/23 13:16:26  cees
csf2clean

Revision 1.1  1996/05/23 13:11:49  cees
Initial revision

Revision 1.4  1995/11/01 17:23:03  cees
.

 * Revision 1.3  1995/01/11  14:14:44  cees
 * added soem sinternal stuff
 *
 * Revision 1.2  1994/09/08  17:16:23  cees
 * added c2man docs + small code changes
 *
 * Revision 1.1  1994/08/26  13:33:23  cees
 * Initial revision
 *
 */
#include "csf.h"
#include "csfimpl.h"

/* get the attribute control block (LIBRARY_INTERNAL)
 * GetAttrBlock searches for the attribute control block
 * that keeps the information for the given id.
 * returns
 * 0 if attribute is not found,
 * or if found, the file position of the attribute
 * control block.
 */
CSF_FADDR CsfGetAttrBlock(
	MAP *m,     /* map handle */
	CSF_ATTR_ID id,   /* identification of the attribute */
	ATTR_CNTRL_BLOCK *b) /* write-only, attribute control block containing
	                      * the id information.
	                      */
{
	CSF_FADDR next;

	next = m->main.attrTable;
	while (next != 0 )
	{
		CsfReadAttrBlock(m, next, b);
		if (CsfGetAttrIndex(id, b) != NR_ATTR_IN_BLOCK)
			break;
		next = b->next;
	}
	return(next);
}

/* get the attribute position and size (LIBRARY_INTERNAL)
 * CsfGetAttrPosSize searches the attribute control block list
 * that keeps the information for the given id.
 * returns
 * 0 if attribute is not found,
 * or if found, the file position of the attribute.
 */
CSF_FADDR CsfGetAttrPosSize(
	MAP *m,     /* map handle */
	CSF_ATTR_ID id,   /* identification of the attribute */
	size_t *size) /* write-only the size of the attribute */
{
	ATTR_CNTRL_BLOCK b;
	int i;

	if (CsfGetAttrBlock(m,id, &b) == 0)
		return 0;

	i = CsfGetAttrIndex(id, &b);
	*size =	b.attrs[i].attrSize;
	return b.attrs[i].attrOffset;
}
