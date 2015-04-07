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
	CSF_FADDR32 pos;

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
