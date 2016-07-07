#ifndef DERIVEDLIST_H_INCLUDEd
#define DERIVEDLIST_H_INCLUDEd

typedef struct
{
  const char * pszDatasetName;
  const char * pszDatasetDescritpion;
  const char * pszPixelFunction;
} DerivedDatasetDescription;

static const DerivedDatasetDescription asDDSDesc [] =
{
  { "AMPLITUDE", "Amplitude of input bands", "mod"},
  { "PHASE", "Phase of input bands", "phase"},
  { "REAL", "Real part of input bands", "real"},
  { "IMAG", "Imaginary part of input bands", "imag"},
  { "CONJ", "Conjugate of input bands", "conj"},
  { "INTENSITY", "Intensity (squared amplitude) of input bands", "intensity"},
  { "LOGAMPLITUDE", "log10 of amplitude of input bands", "log10"}
};

#define NB_DERIVED_DATASETS (sizeof(asDDSDesc)/sizeof(asDDSDesc[0]))


#endif
