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
CSF_FADDR32 CsfGetAttrBlock(
	MAP *m,     /* map handle */
	CSF_ATTR_ID id,   /* identification of the attribute */
	ATTR_CNTRL_BLOCK *b) /* write-only, attribute control block containing
	                      * the id information.
	                      */
{
	CSF_FADDR32 next;

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
CSF_FADDR32 CsfGetAttrPosSize(
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
