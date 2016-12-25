/* Projection System: default list of projections
** Use local definition of PJ_LIST_H for subset.
*/

#define USE_PJ_LIST_H 1
#include "projects.h"



#define PASTE(a,b) a##b

/* Generate prototypes for projection functions */
#define PROJ_HEAD(id, name) struct PJconsts *pj_##id(struct PJconsts*);
#include "pj_list.h"
#undef PROJ_HEAD

/* Generate prototypes for projection selftest functions */
#define PROJ_HEAD(id, name) int PASTE(pj_##id, _selftest) (void);
#include "pj_list.h"
#undef PROJ_HEAD


/* Generate extern declarations for description strings */
#define PROJ_HEAD(id, name) extern char * const pj_s_##id;
#include "pj_list.h"
#undef PROJ_HEAD


/* Generate the null-terminated list of projection functions with associated mnemonics and descriptions */
#define PROJ_HEAD(id, name) {#id, pj_##id, &pj_s_##id},
struct PJ_LIST pj_list[] = {
#include "pj_list.h"
		{0,     0,  0},
	};
#undef PROJ_HEAD


/* Generate the null-terminated list of projection selftest functions with associated mnemonics */
#define PROJ_HEAD(id, name) {#id, PASTE(pj_##id, _selftest)},
struct PJ_SELFTEST_LIST pj_selftest_list[] = {
#include "pj_list.h"
		{0,     0},
	};
#undef PROJ_HEAD
#undef PASTE


struct PJ_LIST  *pj_get_list_ref (void) {
    return pj_list;
}


struct PJ_SELFTEST_LIST  *pj_get_selftest_list_ref (void) {
    return pj_selftest_list;
}
