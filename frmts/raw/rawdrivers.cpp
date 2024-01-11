#include "gdal_frmts.h"

extern "C" void CPL_DLL GDALRegister_raw();
extern "C" void CPL_DLL GDALRegister_raw_no_sidecar();
extern "C" void CPL_DLL GDALRegister_raw_with_sidecar();

void GDALRegister_raw_no_sidecar()
{
    GDALRegister_PNM();
    GDALRegister_DOQ1();
    GDALRegister_DOQ2();
    GDALRegister_PAux();
    GDALRegister_MFF();
    GDALRegister_HKV();
    GDALRegister_GSC();
    GDALRegister_FAST();
    GDALRegister_BT();
    GDALRegister_LAN();
    GDALRegister_CPG();
    GDALRegister_NDF();
    GDALRegister_EIR();
    GDALRegister_DIPEx();
    GDALRegister_LCP();
    GDALRegister_GTX();
    GDALRegister_LOSLAS();
    GDALRegister_NTv2();
    GDALRegister_CTable2();
    GDALRegister_ACE2();
    GDALRegister_SNODAS();
    GDALRegister_KRO();
    GDALRegister_ROIPAC();
    GDALRegister_RRASTER();
    GDALRegister_BYN();
    GDALRegister_NOAA_B();
    GDALRegister_NSIDCbin();
}

void GDALRegister_raw_with_sidecar()
{
    // Drivers that that absolutely need to look for side car files in their
    // Identify()/Open() procedure.
    GDALRegister_GenBin();
    GDALRegister_ENVI();
    GDALRegister_EHdr();
    GDALRegister_ISCE();
}

void GDALRegister_raw()
{
    GDALRegister_raw_no_sidecar();
    GDALRegister_raw_with_sidecar();
}
