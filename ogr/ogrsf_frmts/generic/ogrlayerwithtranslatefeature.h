/******************************************************************************
 *
 * Project:  OpenGIS Simple Features Reference Implementation
 * Purpose:  Defines OGRLayerWithTranslateFeature class
 * Author:   Even Rouault, even dot rouault at spatialys.com
 *
 ******************************************************************************
 * Copyright (c) 2025, Even Rouault <even dot rouault at spatialys.com>
 *
 * SPDX-License-Identifier: MIT
 ****************************************************************************/

#ifndef OGRLAYERWITHTRANSLATEFEATURE_H_INCLUDED
#define OGRLAYERWITHTRANSLATEFEATURE_H_INCLUDED

#ifndef DOXYGEN_SKIP

#include "ogrsf_frmts.h"

/** Class that just extends by OGRLayer by mandating a pure virtual method
 * TranslateFeature() to be implemented.
 */
class OGRLayerWithTranslateFeature /* non final */ : virtual public OGRLayer
{
  public:
    virtual ~OGRLayerWithTranslateFeature();

    /** Translate the source feature into one or several output features */
    virtual void TranslateFeature(
        std::unique_ptr<OGRFeature> poSrcFeature,
        std::vector<std::unique_ptr<OGRFeature>> &apoOutFeatures) = 0;
};

#endif /* DOXYGEN_SKIP */

#endif /* OGRLAYERWITHTRANSLATEFEATURE_H_INCLUDED */
