#include "csf.h"
#include "csfimpl.h"

/* file name associated with map
 * returns the file name associated with map
 */
const char *MgetFileName(
	const MAP *m) /* map handle */
{
	return(m->fileName);
}
