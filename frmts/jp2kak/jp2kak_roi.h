/******************************************************************************
 * $Id$
 *
 * Project:  JPEG-2000
 * Purpose:  Classes related to ROI (region of interest) handling, derived
 *           very directly from Kakadu roi_sources.h.
 * Author:   Frank Warmerdam, warmerdam@pobox.com
 *
 ******************************************************************************
 * Copyright (c) 2003, Frank Warmerdam <warmerdam@pobox.com>
 *
 * Permission is hereby granted, free of charge, to any person obtaining a
 * copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation
 * the rights to use, copy, modify, merge, publish, distribute, sublicense,
 * and/or sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
 * THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 ******************************************************************************
 * 
 * $Log$
 * Revision 1.1  2003/04/17 16:47:27  warmerda
 * New
 *
 */


/*****************************************************************************/
/*                              kd_roi_rect_node                             */
/*****************************************************************************/

class kd_roi_rect_node : public kdu_roi_node {
  public: // Member functions
    kd_roi_rect_node(kdu_dims tile_region, kdu_dims roi_region)
      { tile_dims = tile_region;
        roi_dims = roi_region & tile_region; }
    void release()
      /* In this simplest incarnation of an ROI source node, the granting
         agent (`kdu_roi_rect') does not keep track of the nodes it grants
         via its `acquire_node' interface; therefore, the node's own `release'
         function must destroy the resource.  More sophisticated ROI sources
         will not usually do this. */
      { delete this; }
    void pull(kdu_byte buf[], int width);
  private: // Data
    kdu_dims tile_dims; // Vert coord advances as lines are pulled
    kdu_dims roi_dims; // Vert coord advances when intersecting line pulled
  };

/*****************************************************************************/
/*                                kdu_roi_rect                               */
/*****************************************************************************/

class kdu_roi_rect : public kdu_roi_image {
  /* [SYNOPSIS]
     Serves ROI mask information through the standardized interfaces offered
     by the base class, `kdu_roi_image', for the simple case in which there
     is only one foreground region, having a rectangular geometry.  Although
     somewhat useful in its own right, the main purpose of this object is
     to provide the simplest possible example of implementing the services
     advertised by the abstract base class, `kdu_roi_image'.
     [//]
     For a more sophisticated example, refer to `kdu_roi_graphics'.
  */
  public: // Member functions
    kdu_roi_rect(kdu_codestream codestream, kdu_dims region);
      /* [SYNOPSIS]
           Accepts a single rectangular region, specified relative to the
           high resolution code-stream canvas coordinate system.  This will
           be the foreground region associated with all ROI mask generation
           activities.  It is transformed into a region on each of the
           individual image components by applying the usual coordinate
           transformation rules.
         [ARG: codestream]
           Master interface to the internal code-stream management
           machinery associated with the image being compressed.
         [ARG: region]
           The supplied region should incorporate the effects of any
           prevailing geometric transformations, which may have been set
           up by calls to `codestream.change_appearance'.  As a result,
           if `region' were set to that returned by `codestream.get_dims'
           (called with a negative `comp_idx' argument), the foreground
           would correspond exactly to the full image region on the canvas.
      */
    ~kdu_roi_rect()
      { if (comp_regions != NULL) delete[] comp_regions; }
    kdu_roi_node *acquire_node(int comp_idx, kdu_dims tile_region)
      { /* [SYNOPSIS] See the description of `kdu_roi_image::acquire_node'. */
        assert((comp_idx >= 0) && (comp_idx < num_components));
        return new kd_roi_rect_node(tile_region,comp_regions[comp_idx]);
      }
  private: // Data
    int num_components;
    kdu_dims *comp_regions;
  };

