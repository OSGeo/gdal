/*
Copyright 2015 Esri

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

A local copy of the license and additional notices are located with the
source distribution at:

http://github.com/Esri/lerc/

Contributors:  Thomas Maurer
*/

#ifndef IMAGE_H
#define IMAGE_H

// ---- includes ------------------------------------------------------------ ;
#include "Defines.h"
#include <string>

NAMESPACE_LERC_START

/**     Base class for all image classes
 *
 */

class Image
{
public:

  virtual ~Image()    {}

  enum Type
  {
    BYTE,
    RGB,
    SHORT,
    LONG,
    FLOAT,
    DOUBLE,
    COMPLEX,
    POINT3F,
    CNT_Z,
    CNT_ZXY,
    Last_Type_
  };

  bool isType(Type t) const      { return t == type_; }
  Type getType() const           { return type_; }
  int getWidth() const          { return width_; }
  int getHeight() const         { return height_; }
  int getSize() const           { return width_ * height_; }

  bool isInside(int row, int col) const
  {
      return row >= 0 && row < height_ && col >= 0 && col < width_;
  }

  const virtual std::string getTypeString() const = 0;

protected:

  Image() : type_(Last_Type_), width_(0), height_(0)   {}

  /// compare
  bool operator == (const Image& img) const
  {
      return type_ == img.type_ && width_ == img.width_ && height_ == img.height_;
  }
  bool operator != (const Image& img) const     { return !operator==(img); }

  Type type_;
  int width_, height_;
};

NAMESPACE_LERC_END
#endif
