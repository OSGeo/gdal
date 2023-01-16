The details of how it is taken into account depends on the resampling kernel:

- for nearest resampling, for each target pixel, one of the potential contributing
  source pixels is selected (in an implementation specific way). Its value is used
  as it, be it valid or invalid.

- for bilinear, cubic, cubicspline and lanczos, for each target pixel, the weights
  of contributing source pixels is set to zero to ignore them when they are masked.
  There is an extra specificity for cubic: given that some of the weights in the
  kernel are negative, such strategy could lead to strong overshoot/undershoot
  when there is an alternance of valid and invalid pixels. Consequently, if any
  of the horizontal or vertical direction, if the maximum number of valid source
  pixels in each dimension is less than the radius of the resampling kernel,
  the target pixel is considered as nodata.

- for the other resampling methods, source pixels contributing to the target pixel
  are ignored if masked. Only the valid ones are taken into account. If there are
  none, the target pixel is considered as nodata.
