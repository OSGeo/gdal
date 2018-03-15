#include "csf.h"
#include "csfimpl.h"
#include <string.h>

/* get the size of an attribute (LIBRARY_INTERNAL)
 * returns
 * 0 if the attribute is not available,
 * or the nonzero size if the attribute is available.
 */
size_t CsfAttributeSize(
	 MAP   *m,    /* map handle */
	 CSF_ATTR_ID id)    /* identification of attribute */
{
	ATTR_CNTRL_BLOCK b;
        memset(&b, 0, sizeof(b));

	if (CsfGetAttrBlock(m, id, &b) != 0)
		return b.attrs[CsfGetAttrIndex(id, &b)].attrSize;
        return 0;
}
