/******************************************************************************
 * $Id$
 *
 * Project:  High Performance Image Reprojector
 * Purpose:  Implementation of the GDALWarpOperation class.
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
 * Revision 1.1  2003/02/18 17:25:50  warmerda
 * New
 *
 */

#include "gdalwarper.h"
#include "cpl_string.h"

CPL_CVSID("$Id$");

/************************************************************************/
/* ==================================================================== */
/*                          GDALWarpOperation                           */
/* ==================================================================== */
/************************************************************************/

/**
 * \class GDALWarpOperation "gdalwarper.h"
 *
 * High level image warping class. 

<h2>Warper Design</h2>

The overall GDAL high performance image warper is split into a few components.

 - The transformation between input and output file coordinates is handled
via GDALTransformerFunc() implementations such as the one returned by
GDALCreateGenImgProjTransformer().  The transformers are ultimately responsible
for translating pixel/line locations on the destination image to pixel/line
locations on the source image. 

 - In order to handle images too large to hold in RAM, the warper needs to
segment large images.  This is the responsibility of the GDALWarpOperation
class.  The GDALWarpOperation::ChunkAndWarpImage() invokes 
GDALWarpOperation::WarpRegion() on chunks of output and input image that
are small enough to hold in the amount of memory allowed by the application. 
This process is described in greater detail in the <b>Image Chunking</b> 
section. 

 - The GDALWarpOperation::WarpRegion() function creates and loads an output 
image buffer, and then calls WarpRegionToBuffer(). 

 - GDALWarpOperation::WarpRegionToBuffer() is responsible for loading the 
source imagery corresponding to a particular output region, and generating
masks and density masks from the source and destination imagery using
the generator functions found in the GDALWarpOptions structure.  Binds this
all into an instance of GDALWarpKernel on which the 
GDALWarpKernel::PerformWarp() method is called. 

 - GDALWarpKernel does the actual image warping, but is given an input image
and an output image to operate on.  The GDALWarpKernel does no IO, and in
fact knows nothing about GDAL.  It invokes the transformation function to 
get sample locations, builds output values based on the resampling algorithm
in use.  It also takes any validity and density masks into account during
this operation.  

<h3>Chunk Size Selection</h3>

The GDALWarpOptions ChunkAndWarpImage() method is responsible for invoking
the WarpRegion() method on appropriate sized output chunks such that the
memory required for the output image buffer, input image buffer and any
required density and validity buffers is less than or equal to the application
defined maximum memory available for use.  

It checks the memory requrired by walking the edges of the output region, 
transforming the locations back into source pixel/line coordinates and 
establishing a bounding rectangle of source imagery that would be required
for the output area.  This is actually accomplished by the private
GDALWarpOperation::ComputeSourceWindow() method. 

Then memory requirements are used by totaling the memory required for all
output bands, input bands, validity masks and density masks.  If this is
greater than the GDALWarpOptions::dfWarpMemoryLimit then the destination
region is divided in two (splitting the longest dimension), and 
ChunkAndWarpImage() recursively invoked on each destination subregion. 

<h3>Validity and Density Masks Generation</h3>

Fill in ways in which the validity and density masks may be generated here. 
Note that detailed semantics of the masks should be found in
GDALWarpKernel. 

*/

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

/**
 * \fn CPLErr GDALWarpOperation::Initialize( GDALWarpOptions * );
 *
 * This method initializes the GDALWarpOperation's concept of the warp
 * options in effect.  It creates an internal copy of the GDALWarpOptions
 * structure and defaults a variety of additional fields in the internal
 * copy if not set in the provides warp options.
 *
 * @param psOptions input set of warp options.  These are copied and may
 * be destroyed after this call by the application. 
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

/************************************************************************/
/*                             Initialize()                             */
/************************************************************************/

/**
 * \fn CPLErr GDALWarpOperation::Initialize( GDALWarpOptions * );
 *
 * This method initializes the GDALWarpOperation's concept of the warp
 * options in effect.  It creates an internal copy of the GDALWarpOptions
 * structure and defaults a variety of additional fields in the internal
 * copy if not set in the provides warp options.
 *
 * Defaulting operations include:
 *  - If the nBandCount is 0, it will be set to the number of bands in the
 *    source image (which must match the output image) and the panSrcBands
 *    and panDstBands will be populated. 
 *
 * @param psOptions input set of warp options.  These are copied and may
 * be destroyed after this call by the application. 
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

/************************************************************************/
/*                         ChunkAndWarpImage()                          */
/************************************************************************/

/**
 * \fn CPLErr GDALWarpOperation::ChunkAndWarpImage(
                int nDstXOff, int nDstYOff,  int nDstXSize, int nDstYSize );
 *
 * This method does a complete warp of the source image to the destination
 * image for the indicated region with the current warp options in effect.  
 * Progress is reported to the installed progress monitor, if any.  
 *
 * This function will subdivide the region and recursively call itself 
 * until the total memory required to process a region chunk will all fit
 * in the memory pool defined by GDALWarpOptions::dfWarpMemoryLimit.  
 *
 * Once an appropriate region is selected GDALWarpOperation::WarpRegion()
 * is invoked to do the actual work. 
 *
 * @param nDstXOff X offset to window of destination data to be produced.
 * @param nDstYOff Y offset to window of destination data to be produced.
 * @param nDstXSize Width of output window on destination file to be produced.
 * @param nDstYSize Height of output window on destination file to be produced.
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */


/************************************************************************/
/*                             WarpRegion()                             */
/************************************************************************/

/**
 * \fn CPLErr GDALWarpOperation::WarpRegion(int nDstXOff, int nDstYOff, 
                                            int nDstXSize, int nDstYSize,
                                            int nSrcXOff=0, int nSrcYOff=0,
                                            int nSrcXSize=0, int nSrcYSize=0 );
 *
 * This method requests the indicated region of the output file be generated.
 * 
 * Note that WarpRegion() will produce the requested area in one low level warp
 * operation without verifying that this does not exceed the stated memory
 * limits for the warp operation.  Applications should take care not to call
 * WarpRegion() on too large a region!  This function 
 * is normally called by ChunkAndWarpImage(), the normal entry point for 
 * applications.  Use it instead if staying within memory constraints is
 * desired. 
 *
 * Progress is reported from 0.0 to 1.0 for the indicated region. 
 *
 * @param nDstXOff X offset to window of destination data to be produced.
 * @param nDstYOff Y offset to window of destination data to be produced.
 * @param nDstXSize Width of output window on destination file to be produced.
 * @param nDstYSize Height of output window on destination file to be produced.
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */

/************************************************************************/
/*                            WarpToBuffer()                            */
/************************************************************************/

/**
 * \fn CPLErr GDALWarpOperation::WarpRegionToBuffer( 
                                  int nDstXOff, int nDstYOff, 
                                  int nDstXSize, int nDstYSize, 
                                  void *pDataBuf, 
                                  GDALDataType eBufDataType,
                                  int nSrcXOff=0, int nSrcYOff=0,
                                  int nSrcXSize=0, int nSrcYSize=0 );
 * 
 * This method requests that a particular window of the output dataset
 * be warped and the result put into the provided data buffer.  The output
 * dataset doesn't even really have to exist to use this method as long as
 * the transformation function in the GDALWarpOptions is setup to map to
 * a virtual pixel/line space. 
 *
 * This method will do the whole region in one chunk, so be wary of the
 * amount of memory that might be used. 
 *
 * @param nDstXOff X offset to window of destination data to be produced.
 * @param nDstYOff Y offset to window of destination data to be produced.
 * @param nDstXSize Width of output window on destination file to be produced.
 * @param nDstYSize Height of output window on destination file to be produced.
 * @param pDataBuf the data buffer to place result in, of type eBufDataType.
 * @param eBufDataType the type of the output data buffer.  For now this
 * must match GDALWarpOptions::eWorkingDataType. 
 * @param nSrcXOff source window X offset (computed if window all zero)
 * @param nSrcYOff source window Y offset (computed if window all zero)
 * @param nSrcXSize source window X size (computed if window all zero)
 * @param nSrcYSize source window Y size (computed if window all zero)
 *
 * @return CE_None on success or CE_Failure if an error occurs.
 */
                                  
