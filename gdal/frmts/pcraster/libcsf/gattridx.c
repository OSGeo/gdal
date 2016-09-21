#include "csf.h"
#include "csfimpl.h"

/* search attribute index in block (LIBRARY_INTERNAL)
 * returns index in block where id is found, NR_ATTR_IN_BLOCK if not found 
 */
int CsfGetAttrIndex(
	CSF_ATTR_ID id,  /* id to be found */
	const ATTR_CNTRL_BLOCK *b) /* block to inspect */
{
	int i = 0;

	while(i < NR_ATTR_IN_BLOCK)
	{
		if (b->attrs[i].attrId == id) 
			break;
		i++;
	}
	return(i);
}
