#include <windows.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "mmrdlayr.h"
#include "giraarc.h"
#include "env.h"
#include "DefTopMM.h"
#include "libtop.h"
#include "metadata.h"
#include "NomsFitx.h" // Per a OmpleExtensio()
#include "ExtensMM.h"	//Per a ExtPoligons,...

static MM_TIPUS_ERROR lastErrorMM;
#define MIDA_MISSATGE 		512
#define MIDA_PATH 			1024
int crea_dvc(FILE *f, MM_HANDLE_CAPA_VECTOR htlmm);

MM_TIPUS_ERROR MMRecuperaUltimError(void)
{
	return lastErrorMM;
}
struct SMM_HANDLE_CAPA_VECTOR
{
	int tipus_fitxer;
	struct TREBALL_ARC_NOD an;
    char nom_fitxer[_MAX_PATH];
    char nom_fitxer_arc[_MAX_PATH];
	struct Capcalera_Top cap_top;
    struct Capcalera_Top cap_top_arc;
    BOOL es_3d;
    struct CAPCALERA_VECTOR_3D *cap_arc_3d;  // pel cas en que necessitem llegir les capçaleres 3d amb el "pic i la pala".
    FILE *pf, *pfarc;
};

MM_HANDLE_CAPA_VECTOR MMIniciaCapaVector(const char *nom_fitxer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;
char ext[MAX_MIDA_EXTENSIO_FITXER];

//Aquests defines fan que no es miri el tema de la llicència.
#define ANY_CADUCITAT 2008
#define MES_CADUCITAT 8
#define DIA_CADUCITAT 1

#if defined ANY_CADUCITAT && defined MES_CADUCITAT && defined DIA_CADUCITAT
time_t t;
   struct tm *tm_local;
#endif

#if defined ANY_CADUCITAT && defined MES_CADUCITAT && defined DIA_CADUCITAT
   //   La llicència temporal. En aquest cas no es valida la llicència basada
   //   en paràmetres.

   t = time(NULL);
   tm_local = localtime(&t);

   if (tm_local->tm_year+1900>ANY_CADUCITAT ||
       (tm_local->tm_year+1900==ANY_CADUCITAT && tm_local->tm_mon+1>MES_CADUCITAT) ||
       (tm_local->tm_year+1900==ANY_CADUCITAT && tm_local->tm_mon+1==MES_CADUCITAT && tm_local->tm_mday>=DIA_CADUCITAT))
   {
    	lastErrorMM=MalInstallat_o_CopiaIllegal;
       	return NULL;
   }
#endif


	if(!nom_fitxer || VerificaSiPucLlegirFitxerXerraire(nom_fitxer, FALSE))
    {
	    lastErrorMM=No_puc_obrir_demanat;
        return NULL;
    }

	if(NULL==(shlayer=calloc(1,sizeof(*shlayer))))
    {
	    lastErrorMM=No_mem;
        return NULL;
    }

    OmpleExtensio(ext,MAX_MIDA_EXTENSIO_FITXER, nom_fitxer);
    if(!stricmp(ext, ExtPoligons+1))
    {
    	shlayer->tipus_fitxer=MM32DLL_POL;
    }
    else if(!stricmp(ext, ExtArcs+1))
    {
    	shlayer->tipus_fitxer=MM32DLL_ARC;
    }
    else if(!stricmp(ext, ExtNodes+1))
    {
    	shlayer->tipus_fitxer=MM32DLL_NOD;
    }
    else if(!stricmp(ext, ExtPunts+1))
    {
    	shlayer->tipus_fitxer=MM32DLL_PNT;
    }
    else
    {
	    lastErrorMM=Ext_incorrecta_No_puc_obrir;
        free(shlayer);
        return NULL;
    }

	strcpy(shlayer->nom_fitxer, nom_fitxer);

    if(shlayer->tipus_fitxer==MM32DLL_POL)
    {
        DonaNomArcDelPol(shlayer->nom_fitxer_arc, shlayer->nom_fitxer);
        if ((shlayer->pfarc=fopenAO(shlayer->nom_fitxer_arc, "rb"))==0 || lect_capcalera_topo(shlayer->pfarc, ExtArcs, &(shlayer->cap_top_arc)))
        {
            lastErrorMM=No_puc_obrir_origen;
            free(shlayer);
            return NULL;
        }

        LlegeixStructTreballArcNod(shlayer->nom_fitxer, shlayer->nom_fitxer_arc, &(shlayer->an));

        if ((shlayer->pf=fopenAO(shlayer->nom_fitxer, "rb"))==0 || lect_capcalera_topo(shlayer->pf, ExtPoligons, &(shlayer->cap_top)))
        {
            lastErrorMM=No_puc_obrir_origen;
            MMFinalitzaCapaVector(shlayer);
            free(shlayer);
            return NULL;
        }
        shlayer->cap_arc_3d=NULL;
        shlayer->es_3d=(BOOL)(shlayer->cap_top.flag & AMB_INFORMACIO_ALTIMETRICA);
    }
    else if(shlayer->tipus_fitxer==MM32DLL_ARC)
    {
    	*(shlayer->nom_fitxer_arc)='\0';
        shlayer->pfarc=NULL;

        LlegeixStructTreballArcNod(NULL, shlayer->nom_fitxer, &(shlayer->an));
        if ((shlayer->pf=fopenAO(shlayer->nom_fitxer, "rb"))==0 || lect_capcalera_topo(shlayer->pf, ExtArcs, &(shlayer->cap_top)))
        {
            lastErrorMM=UltimErrorStb0;
            MMFinalitzaCapaVector(shlayer);
            free(shlayer);
            return NULL;
        }
        shlayer->es_3d=(BOOL)(shlayer->cap_top.flag & AMB_INFORMACIO_ALTIMETRICA);
    }
    else if(shlayer->tipus_fitxer==MM32DLL_NOD)
    {
        sprintf(shlayer->nom_fitxer_arc, "%s%s", TreuExtensio(nom_fitxer), ExtArcs);

        LlegeixStructTreballArcNod(NULL, shlayer->nom_fitxer_arc, &(shlayer->an));

        if ((shlayer->pfarc=fopenAO(shlayer->nom_fitxer_arc, "rb"))==0)
        {
            lastErrorMM=No_puc_obrir_origen;
            MMFinalitzaCapaVector(shlayer);
            free(shlayer);
            return NULL;
        }
        if (lect_capcalera_topo(shlayer->pfarc, ExtArcs, &(shlayer->cap_top_arc)))
        {
            lastErrorMM=UltimErrorStb0;
            MMFinalitzaCapaVector(shlayer);
            free(shlayer);
            return NULL;
        }
        if ((shlayer->pf=fopenAO(shlayer->nom_fitxer, "rb"))==0)
        {
            lastErrorMM=No_puc_obrir_origen;
            MMFinalitzaCapaVector(shlayer);
            free(shlayer);
            return NULL;
        }
        if (lect_capcalera_topo(shlayer->pf, ExtNodes, &(shlayer->cap_top)))
        {
            lastErrorMM=UltimErrorStb0;
            MMFinalitzaCapaVector(shlayer);
            free(shlayer);
            return NULL;
        }
		if(shlayer->an.arcZ)
    	    shlayer->es_3d=TRUE;
        else
	        shlayer->es_3d=FALSE;
	}
    else if(shlayer->tipus_fitxer==MM32DLL_PNT)
    {
    	*(shlayer->nom_fitxer_arc)='\0';
        shlayer->pfarc=NULL;

        if ((shlayer->pf=fopenAO(shlayer->nom_fitxer, "rb"))==0)
        {
            lastErrorMM=No_puc_obrir_origen;
            MMFinalitzaCapaVector(shlayer);
            free(shlayer);
            return NULL;
        }
        if (lect_capcalera_topo(shlayer->pf, ExtPunts, &(shlayer->cap_top)))
        {
            lastErrorMM=UltimErrorStb0;
            MMFinalitzaCapaVector(shlayer);
            free(shlayer);
            return NULL;
        }
        shlayer->es_3d=(BOOL)(shlayer->cap_top.flag & AMB_INFORMACIO_ALTIMETRICA);
	}

    if(shlayer->es_3d && shlayer->tipus_fitxer==MM32DLL_ARC)
    {
	    if(NULL==(shlayer->cap_arc_3d=Llegeix_cap_arcZ(shlayer->pf, shlayer->an.cap_arc, shlayer->an.n_arc, NULL, NULL)))
        {
        	lastErrorMM=UltimErrorStb0;
        	MMFinalitzaCapaVector(shlayer);
            free(shlayer);
  			return NULL;
        }
    }
    if(shlayer->es_3d && shlayer->tipus_fitxer==MM32DLL_NOD)
    {
    	if(NULL==(shlayer->cap_arc_3d=Llegeix_cap_arcZ(shlayer->pfarc, shlayer->an.cap_arc, shlayer->an.n_arc, NULL, NULL)))
  		{
        	lastErrorMM=UltimErrorStb0;
        	MMFinalitzaCapaVector(shlayer);
            free(shlayer);
  			return NULL;
        }
    }
    else if(shlayer->es_3d && shlayer->tipus_fitxer==MM32DLL_PNT)
    {
	    if (NULL==(shlayer->cap_arc_3d=calloc(shlayer->cap_top.n_elem, sizeof(*(shlayer->cap_arc_3d)))))
        {
        	lastErrorMM=No_mem;
        	MMFinalitzaCapaVector(shlayer);
            free(shlayer);
  			return NULL;
        }
        // Saltem la part de punts i llegim les capçaleres 3d
        fseek(shlayer->pf, MIDA_CAPCALERA_TOP+shlayer->cap_top.n_elem*sizeof(double)*2+16+sizeof(double)*2,SEEK_SET);
        // LLegim les capçaleres
        if (fread(shlayer->cap_arc_3d, sizeof(struct CAPCALERA_VECTOR_3D), shlayer->cap_top.n_elem, shlayer->pf)!=(size_t)shlayer->cap_top.n_elem)
        {
            lastErrorMM=Mida_incoherent_Corromput;
        	MMFinalitzaCapaVector(shlayer);
            free(shlayer);
  			return NULL;
        }
    }
    return (MM_HANDLE_CAPA_VECTOR)shlayer;
}

MM_TIPUS_I_ELEM_CAPA_VECTOR MMRecuperaNElemCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;
    return shlayer->cap_top.n_elem;
}


MM_TIPUS_ENV_CAPA_VECTOR MMRecuperaMinXCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;

  	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;
    return shlayer->cap_top.envolupant[VX_MIN];
}

MM_TIPUS_ENV_CAPA_VECTOR MMRecuperaMaxXCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;
    return shlayer->cap_top.envolupant[VX_MAX];
}

MM_TIPUS_ENV_CAPA_VECTOR MMRecuperaMinYCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;
    return shlayer->cap_top.envolupant[VY_MIN];
}

MM_TIPUS_ENV_CAPA_VECTOR MMRecuperaMaxYCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;
    return shlayer->cap_top.envolupant[VY_MAX];
}

MM_TIPUS_I_ANELL_CAPA_VECTOR MMRecuperMaxNAnellsCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;
unsigned long int i;
MM_TIPUS_I_ANELL_CAPA_VECTOR max=0;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;
    if(shlayer->tipus_fitxer!=MM32DLL_POL)
        return 1;

	for (i=0;i<shlayer->an.n_pol;i++)
    {
    	if(max<shlayer->an.cap_pol[i].npol_per_polip)
			max=shlayer->an.cap_pol[i].npol_per_polip;
    }
    return max;
}

MM_TIPUS_I_COORD_CAPA_VECTOR MMRecuperaMaxNCoordCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;
MM_TIPUS_I_COORD_CAPA_VECTOR maxncoord;
TIPUS_N_VERTEXS *n_vrt_ring;
TIPUS_N_POLIGONS n_mini_pol;
TIPUS_NUMERADOR_OBJECTE i_elem;
TIPUS_N_VERTEXS n_ring;
long int j;
struct VARIABLES_LLEGEIX_POLS var_pols;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;

    if(shlayer->tipus_fitxer==MM32DLL_PNT || shlayer->tipus_fitxer==MM32DLL_NOD)
    	return 1;

    maxncoord=0;
    if(shlayer->tipus_fitxer==MM32DLL_ARC)
    {
    	maxncoord=0;
        for(i_elem=0; i_elem<shlayer->an.n_arc;i_elem++)
        {
            if(maxncoord<shlayer->an.cap_arc[i_elem].nomb_vertex)
                maxncoord=shlayer->an.cap_arc[i_elem].nomb_vertex;
        }
        return maxncoord;
    }

    if(shlayer->tipus_fitxer==MM32DLL_POL)
    {
        maxncoord=0;
        n_vrt_ring=NULL;
        for(i_elem=0; i_elem<shlayer->an.n_pol;i_elem++)
        {
            LlegeixPoliPoligon3D_POL(NULL, &n_vrt_ring, &n_mini_pol, NULL, NULL, NULL, NULL,
               NULL, NULL, shlayer->pf, shlayer->pfarc,
               shlayer->an.cap_arc, NULL, shlayer->an.cap_pol, i_elem, FALSE, 0L, &var_pols);

            n_ring=0;
            for(j=0;j<(int)n_mini_pol;j++)
                (n_ring)+=n_vrt_ring[j];

            if(maxncoord<n_ring)
                maxncoord=n_ring;
        }
		AlliberaIAnullaPoliPoligon3D_POL(NULL, &n_vrt_ring, &n_mini_pol, NULL, NULL, NULL,
               NULL, NULL, &var_pols);

        return maxncoord;
    }
	return maxncoord;
}

MM_TIPUS_BOLEA MMRecuperaElemCapaVector(MM_HANDLE_CAPA_VECTOR hlayer, MM_TIPUS_I_ELEM_CAPA_VECTOR i_elem,
				MM_TIPUS_COORD coord_x[], MM_TIPUS_COORD coord_y[], MM_TIPUS_COORD coord_z[],
                MM_TIPUS_I_COORD_CAPA_VECTOR n_vrt_ring[], MM_TIPUS_I_ANELL_CAPA_VECTOR *n_ring, MM_TIPUS_SELEC_COORDZ select_coordz)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;
unsigned long int flag_z;
size_t k, i_coord_acumulat;
size_t i_ring;
struct POINT_DOUBLE_2D *coord=NULL;
struct POINT_DOUBLE_2D un_punt;
double *z=NULL;
int num;
double *punts_z, cz;
struct POINT_DOUBLE_2D *punts;
TIPUS_N_VERTEXS *n_vrt_ring_interna=NULL;
struct VARIABLES_LLEGEIX_POLS var_pols;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;

    if (select_coordz==MM_SELECT_COORDZ_MES_ALTA)
    	flag_z=ARC_ALCADA_MES_ALTA;
	else if (select_coordz==MM_SELECT_COORDZ_MES_BAIXA)
        flag_z=ARC_ALCADA_MES_BAIXA;
    else
        flag_z=0L;

    if(shlayer->tipus_fitxer==MM32DLL_PNT)
    {
    	fseek(shlayer->pf, MIDA_CAPCALERA_TOP+sizeof(double)*2*i_elem, SEEK_SET);
        // LLegim els punts
        if (1!=fread(&un_punt, sizeof(double)*2, 1, shlayer->pf))
        {
            lastErrorMM=Error_lectura_Corromput;
            return FALSE;
        }
        coord_x[0]=un_punt.x;
        coord_y[0]=un_punt.y;

        if(shlayer->es_3d && coord_z)
        {
            num=ARC_N_TOTAL_ALCADES_DISC(shlayer->cap_arc_3d[i_elem].n_alcades, 1);
            if(num==0)coord_z[0]=NODATA_COORD_Z;
            else
            {
                punts_z=calloc(num, sizeof(double));
                if((size_t)num!=fread(punts_z,sizeof(*punts_z),num,shlayer->pf))
                {
                    lastErrorMM=Error_lectura_Corromput;
                    return FALSE;
                }

                if (flag_z==ARC_ALCADA_MES_ALTA)
                    cz=shlayer->cap_arc_3d[i_elem].zmax;
                else if (flag_z==ARC_ALCADA_MES_BAIXA)
                    cz=shlayer->cap_arc_3d[i_elem].zmin;
                else
                    cz=punts_z[0];

                if(!DOUBLES_DIFERENTS_DJ(cz, NODATA_COORD_Z))
                    coord_z[0]=-DBL_MAX;
                else coord_z[0]=cz; // Només ens quedem el primer.

                free(punts_z);
            }
        }
        if(n_ring)*n_ring=1;
        if(n_vrt_ring)n_vrt_ring[0]=1;
        return TRUE;
    }

    if(shlayer->tipus_fitxer==MM32DLL_NOD)
    {
    	if(shlayer->es_3d && coord_z)
        {
            DonaPosicioNode(&un_punt, &cz, i_elem,
                shlayer->an.cap_nod, shlayer->cap_top.n_elem,
                shlayer->pf, shlayer->an.cap_arc, shlayer->cap_arc_3d,
                shlayer->cap_top_arc.n_elem, shlayer->pfarc, flag_z);
            coord_z[0]=cz;
        }
        else
        {
            DonaPosicioNode(&un_punt, NULL, i_elem,
                shlayer->an.cap_nod, shlayer->cap_top.n_elem,
                shlayer->pf, shlayer->an.cap_arc, NULL,
                shlayer->cap_top_arc.n_elem, shlayer->pfarc, flag_z);
        }
        coord_x[0]=un_punt.x;
        coord_y[0]=un_punt.y;

        if(n_ring)*n_ring=1;
        if(n_vrt_ring)n_vrt_ring[0]=1;
        return TRUE;
    }

    if(shlayer->tipus_fitxer==MM32DLL_ARC)
    {
    	fseek(shlayer->pf, shlayer->an.cap_arc[i_elem].offset_0, SEEK_SET);
        // LLegim els vertexs de l'arc
        punts=calloc(shlayer->an.cap_arc[i_elem].nomb_vertex, sizeof(struct POINT_DOUBLE_2D));
        if((size_t)shlayer->an.cap_arc[i_elem].nomb_vertex!=fread(punts,sizeof(struct POINT_DOUBLE_2D),shlayer->an.cap_arc[i_elem].nomb_vertex,shlayer->pf))
        {
            lastErrorMM=Mida_incoherent_Corromput;
            return FALSE;
        }

        for(k=0;k<shlayer->an.cap_arc[i_elem].nomb_vertex;k++)
        {
        	coord_x[k]=punts[k].x;
	        coord_y[k]=punts[k].y;
        }
        free(punts);

        if(shlayer->es_3d && coord_z)
        {
            punts_z=calloc(shlayer->an.cap_arc[i_elem].nomb_vertex, sizeof(double));
            DonaAlcadesDArc(punts_z, shlayer->pf, shlayer->an.cap_arc[i_elem].nomb_vertex,
                &shlayer->cap_arc_3d[i_elem], flag_z);

            for(k=0; k<(shlayer->an.cap_arc[i_elem].nomb_vertex); k++)
            {
                if(!DOUBLES_DIFERENTS_DJ(punts_z[k], NODATA_COORD_Z))
                    cz=-DBL_MAX;
                else cz=punts_z[k]; // Només ens quedem el primer.

                coord_z[k]=cz;
            }
            free(punts_z);
        }
        if(n_ring)*n_ring=1;
        n_vrt_ring[0]=shlayer->an.cap_arc[i_elem].nomb_vertex;
        return TRUE;
    }

    // Ara he llegir la capçalera de l'arc i llavors l'offset del primer vèrtex, llegirlos i afegirlos
    // a la llista que anirà al final.
    if(shlayer->es_3d && coord_z)
    {
        LlegeixPoliPoligon3D_POL(&coord, (TIPUS_N_VERTEXS **)&n_vrt_ring_interna, (TIPUS_N_POLIGONS *)n_ring, NULL, &z, NULL, NULL,
            NULL, NULL, shlayer->pf, shlayer->pfarc,
            shlayer->an.cap_arc, shlayer->an.arcZ, shlayer->an.cap_pol, i_elem, FALSE, flag_z, &var_pols);
    }
    else
    {
        LlegeixPoliPoligon3D_POL(&coord, (TIPUS_N_VERTEXS **)&n_vrt_ring_interna, (TIPUS_N_POLIGONS *)n_ring, NULL,	NULL, NULL, NULL,
            NULL, NULL, shlayer->pf, shlayer->pfarc,
            shlayer->an.cap_arc, NULL, shlayer->an.cap_pol, i_elem, FALSE, flag_z, &var_pols);
    }

    for(i_ring=0, i_coord_acumulat=0; i_ring<*n_ring; i_ring++)
    {
        for(k=0; k<n_vrt_ring[i_ring]; k++, i_coord_acumulat++)
        {
            coord_x[i_coord_acumulat]=coord[i_coord_acumulat].x;
            coord_y[i_coord_acumulat]=coord[i_coord_acumulat].y;
            if (coord_z)
            {
	            if(shlayer->es_3d && z)
    	        	coord_z[i_coord_acumulat]=z[i_coord_acumulat];
        	    else
            		coord_z[i_coord_acumulat]=NODATA_COORD_Z;
            }
        }
        n_vrt_ring[i_ring]=n_vrt_ring_interna[i_ring];
    }

    if(shlayer->es_3d && coord_z)
    {
        AlliberaIAnullaPoliPoligon3D_POL(&coord, (TIPUS_N_VERTEXS **)&n_vrt_ring_interna, (TIPUS_N_POLIGONS *)n_ring,	&z, NULL, NULL,
            NULL, NULL, &var_pols);
    }
    else
    {
        AlliberaIAnullaPoliPoligon3D_POL(&coord, (TIPUS_N_VERTEXS **)&n_vrt_ring_interna, (TIPUS_N_POLIGONS *)n_ring,	NULL, NULL, NULL,
            NULL, NULL, &var_pols);
    }

	return TRUE;
}

MM_TIPUS_BOLEA MMEs3DCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;
 	return (MM_TIPUS_BOLEA)shlayer->es_3d;
}

MM_TIPUS_TIPUS_FITXER MMTipusFitxerCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;
 	return (MM_TIPUS_TIPUS_FITXER)(shlayer->tipus_fitxer);
}

void MMFinalitzaCapaVector(MM_HANDLE_CAPA_VECTOR hlayer)
{
struct SMM_HANDLE_CAPA_VECTOR *shlayer;

	shlayer=(struct SMM_HANDLE_CAPA_VECTOR *)hlayer;
	AlliberaStructTreballArcNod(&(shlayer->an));
    if(shlayer->pf)fclose(shlayer->pf);
    if(shlayer->pfarc)fclose(shlayer->pfarc);
    if(shlayer->cap_arc_3d)free(shlayer->cap_arc_3d);
}

#ifdef KK
#ifndef __DLL__

char missatge_local[MIDA_MISSATGE], missatge_local2[MIDA_MISSATGE];
int main(int argc, char *argv[])
{
MM_HANDLE_CAPA_VECTOR hltmm;
MM_TIPUS_COORD *coord_x, *coord_y, *coord_z;
MM_TIPUS_I_COORD_CAPA_VECTOR maxNcoord;
MM_TIPUS_I_ANELL_CAPA_VECTOR maxNRings;
MM_TIPUS_I_ELEM_CAPA_VECTOR nelem;
MM_TIPUS_I_ELEM_CAPA_VECTOR i_elem;
MM_TIPUS_I_COORD_CAPA_VECTOR *n_vrt_ring;
MM_TIPUS_I_ANELL_CAPA_VECTOR n_ring;
MM_TIPUS_BOLEA es_3d;
unsigned long int i_acum, i, j;
char nom_vec[MIDA_PATH];
char nom_dvc[MIDA_PATH];
FILE *f, *fdvc;

	if(argc>2)
    {
	    sprintf(missatge_local, "Nombre d'arguments incorrecte");
    	puts(missatge_local);
        return 1;
    }

    hltmm=MMIniciaCapaVector(argv[1]);
    if (hltmm==NULL)
    {
        sprintf(missatge_local, "Error a mm32dll amb codi: %d", MMRecuperaUltimError());
    	puts(missatge_local);
        return 1;
    }

    maxNcoord=MMRecuperaMaxNCoordCapaVector(hltmm);
    maxNRings=MMRecuperMaxNAnellsCapaVector(hltmm);

    if(maxNcoord==0)
    {
	    sprintf(missatge_local, "Fitxer buit");
    	puts(missatge_local);
    	return 0;
    }

    if(NULL==(coord_x=calloc(maxNcoord, sizeof(*coord_x))))
    {
	    sprintf(missatge_local, "Falta memòria");
    	puts(missatge_local);
    	return 1;
    }
    if(NULL==(coord_y=calloc(maxNcoord, sizeof(*coord_y))))
    {
	    sprintf(missatge_local, "Falta memòria");
    	puts(missatge_local);
    	return 1;
    }
    if(NULL==(coord_z=calloc(maxNcoord, sizeof(*coord_z))))
    {
	    sprintf(missatge_local, "Falta memòria");
    	puts(missatge_local);
	    return 1;
    }

    if(NULL==(n_vrt_ring=calloc(maxNRings, sizeof(*n_vrt_ring))))
    {
	    sprintf(missatge_local, "Falta memòria");
    	puts(missatge_local);
	    return 1;
    }

    // Fitxer per a la creació d'un VEC de MiraMon
    strcpy(nom_vec, "vec_exemple_MM.vec");
    if ((f=fopen(nom_vec, "wt"))==NULL)
    {
    	sprintf(missatge_local, "No puc escriure el fitxer %s", nom_vec);
        puts(missatge_local);
        return 1;
    }
    strcpy(nom_dvc, "vec_exemple_MM.dvc");
    if ((fdvc=fopen(nom_dvc, "wt"))==NULL)
    {
    	sprintf(missatge_local, "No puc escriure el fitxer %s", nom_dvc);
        puts(missatge_local);
        return 1;
    }

    es_3d=MMEs3DCapaVector(hltmm);

    nelem=MMRecuperaNElemCapaVector(hltmm);
    sprintf(missatge_local, "Tenim %d elements", nelem);
    puts(missatge_local);
    for(i_elem=0; i_elem<nelem; i_elem++)
    {
        if(!MMRecuperaElemCapaVector(hltmm, i_elem, coord_x, coord_y, coord_z,
                n_vrt_ring, &n_ring, MM_SELECT_COORDZ_MES_ALTA))
        {
            sprintf(missatge_local, "Error amb codi (%d)\n", MMRecuperaUltimError());
            puts(missatge_local);
            return 1;
        }

        // Vèrtexs per pantalla
        printf("Elements (%d) de (%d) anells\n", i_elem, n_ring);
        for(i=0, i_acum=0; i<n_ring; i++)
        {
            printf("Nombre de vèrtex per a l'anell (%d) de l'element (%d): (%d)\n", i, i_elem, n_vrt_ring[i]);
            for(j=0; j<n_vrt_ring[i]; j++,i_acum++)
            {
                printf("\tVèrtex (%d): x(%f) y(%f) ", i_acum, coord_x[i_acum], coord_y[i_acum]);
                if(es_3d)
                    printf("z(%f) ", coord_z[i_acum]);

                printf(" ");
                printf("\n");
            }
            printf("\n");
        }

        // Vèrtexs per a un VEC de MiraMon
        for(i=0, i_acum=0; i<n_ring; i++)
        {
            fprintf(f, "%d %d\n", i_elem, n_vrt_ring[i]);
            for(j=0; j<n_vrt_ring[i]; j++,i_acum++)
            {
                fprintf(f, "%f %f", coord_x[i_acum], coord_y[i_acum]);
                if(es_3d)
                    fprintf(f, " %f", coord_z[i_acum]);
                fprintf(f, "\n");
            }
        }
    }
    fprintf(f, "0 0");

    if(crea_dvc(fdvc, hltmm))
    {
	    sprintf(missatge_local, "No es pot crear la dvf del fitxer VEC");
        puts(missatge_local);
	    return 1;
    }

    fclose(f);
    fclose(fdvc);

    free(coord_x);
    free(coord_y);
    free(coord_z);
    free(n_vrt_ring);

    MMFinalitzaCapaVector(hltmm);
    free(hltmm);
    return 0;
}


int crea_dvc(FILE *f, MM_HANDLE_CAPA_VECTOR htlmm)
{
	fprintf(f,"file title  : \n");
	fprintf(f,"id type     : integer\n");
	fprintf(f,"file type   : ascii\n");
    switch ((int)MMTipusFitxerCapaVector(htlmm))
    {
    	case MM32DLL_POL:
        	fprintf(f,"object type : polygon\n");
        break;
        case MM32DLL_ARC:
        	fprintf(f,"object type : line\n");
        break;
        case MM32DLL_NOD:
        case MM32DLL_PNT:
        	fprintf(f,"object type : point\n");
        break;

    }
	fprintf(f,"ref. system : plane\n");
	fprintf(f,"ref. units  : píxels\n");
	fprintf(f,"unit dist.  : 1.000000\n");
	fprintf(f,"min. X      : %f\n", MMRecuperaMinXCapaVector(htlmm));
	fprintf(f,"max. X      : %f\n", MMRecuperaMaxXCapaVector(htlmm));
	fprintf(f,"min. Y      : %f\n", MMRecuperaMinYCapaVector(htlmm));
	fprintf(f,"max. Y      : %f\n", MMRecuperaMaxYCapaVector(htlmm));
	fprintf(f,"pos'n error : unknown\n");
	fprintf(f,"resolution  : unknown");
    return 0;
}


#endif
#endif //KK