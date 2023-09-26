#include <windows.h>

#include "prjmm.h"

typedef void * MM_HANDLE_CAPA_VECTOR;
typedef TIPUS_NUMERADOR_OBJECTE MM_TIPUS_I_ELEM_CAPA_VECTOR;
typedef unsigned long int MM_TIPUS_I_ANELL_CAPA_VECTOR;
typedef TIPUS_N_VERTEXS MM_TIPUS_I_COORD_CAPA_VECTOR;
typedef double MM_TIPUS_COORD;
typedef int MM_TIPUS_SELEC_COORDZ;
typedef double MM_TIPUS_ENV_CAPA_VECTOR;
typedef int MM_TIPUS_ERROR;
typedef int MM_TIPUS_TIPUS_FITXER;
typedef BOOL MM_TIPUS_BOLEA;


MM_TIPUS_ERROR MMRecuperaUltimError(void);
MM_HANDLE_CAPA_VECTOR MMIniciaCapaVector(const char *nom_fitxer);

MM_TIPUS_I_ELEM_CAPA_VECTOR MMRecuperaNElemCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);
MM_TIPUS_ENV_CAPA_VECTOR MMRecuperaMaxXCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);
MM_TIPUS_ENV_CAPA_VECTOR MMRecuperaMinXCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);
MM_TIPUS_ENV_CAPA_VECTOR MMRecuperaMaxYCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);
MM_TIPUS_ENV_CAPA_VECTOR MMRecuperaMinYCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);
MM_TIPUS_I_ANELL_CAPA_VECTOR MMRecuperMaxNAnellsCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);
MM_TIPUS_I_COORD_CAPA_VECTOR MMRecuperaMaxNCoordCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);

#define MM32DLL_PNT 	0
#define MM32DLL_NOD 	1
#define MM32DLL_ARC 	2
#define MM32DLL_POL 	3
MM_TIPUS_TIPUS_FITXER MMTipusFitxerCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);
MM_TIPUS_BOLEA MMEs3DCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);

#define MM_SELECT_COORDZ_MES_BAIXA 	0
#define MM_SELECT_COORDZ_MES_ALTA  	1

#define MM_IN  // Defines an IN parameter
#define MM_OUT // Defines an OUT parameter
MM_TIPUS_BOLEA MMRecuperaElemCapaVector(MM_IN MM_HANDLE_CAPA_VECTOR hlayer, MM_IN MM_TIPUS_I_ELEM_CAPA_VECTOR i_elem,
				MM_OUT MM_TIPUS_COORD coord_x[], MM_OUT MM_TIPUS_COORD coord_y[], MM_OUT MM_TIPUS_COORD coord_z[],
                MM_OUT MM_TIPUS_I_COORD_CAPA_VECTOR n_vrt_ring[], MM_OUT MM_TIPUS_I_ANELL_CAPA_VECTOR *n_ring, MM_IN MM_TIPUS_SELEC_COORDZ select_coordz);

void MMFinalitzaCapaVector(MM_HANDLE_CAPA_VECTOR hlayer);


