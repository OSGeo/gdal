/*
Copyright 2016-2021 Esri
Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.

Contributors:  Lucian Plesea
*/

//
// Implements an RLE codec packer.  The RLE uses a dedicated marker code
// This particular packer picks the least used byte value as marker,
// which makes the worst case data expansion more reasonable, at the expense
// of taking two passes over the input data
//

// For memset
#include <cstring>
#include <vector>
#include <algorithm>

#include "marfa.h"
#include "Packer_RLE.h"

NAMESPACE_MRF_START
//
// RLE yarn codec, uses a dedicated code as marker, default value is 0xC3
// This is the C implementation, there is also a C++ one
// For yarnball compression, it performs better than using a double char marker
//
// Includes both encoder and decoder
// Worst case input:output ratio is 1:2, when the input is 1,2 or three marker codes
// For long input streams, the input:output ratio is between
// 13260.6:1 (best) and 1:1.75 (worst)
//
// Could be a byte stream filter which needs very little local storage
//

constexpr int MAX_RUN = 768 + 0xffff;
typedef unsigned char Byte;

#define UC(X) static_cast<Byte>(X)

// Encode helper function
// It returns how many times the byte at *s is repeated
// a value between 1 and min(max_count, MAX_RUN)
inline static int run_length(const Byte *s, int max_count)
{
  if (max_count > MAX_RUN)
    max_count = MAX_RUN;
  const Byte c = *s++;
  for (int count = 1; count < max_count; count++)
    if (c != *s++)
      return count;
  return max_count;
}

#define RET_NOW do { return static_cast<size_t>(next - reinterpret_cast<Byte *>(obuf)); } while(0)

//
// C compress function, returns compressed size
// len is the size of the input buffer
// caller should ensure that output buffer is at least 2 * N to be safe, 
// dropping to N * 7/4 for larger input
// If the Code is chosen to be the least present value in input, the
// output size requirement is bound by N / 256 + N
//
static size_t toYarn(const char *ibuffer, char *obuf, size_t len, Byte CODE = 0xC3) {
  Byte *next = reinterpret_cast<Byte *>(obuf);

  while (len) {
    Byte b = static_cast<Byte>(*ibuffer);
    int run = run_length(reinterpret_cast<const Byte *>(ibuffer), static_cast<int>(len));
    if (run < 4) { // Encoded as single bytes, stored as such, CODE followed by a zero
      run = 1;
      *next++ = b;
      if (CODE == b)
        *next++ = 0;
    }
    else { // Encoded as a sequence
      *next++ = CODE; // Start with Marker code, always present

      if (run > 767) { // long sequence
        ibuffer += 768; // May be unsafe to read *ibuffer
        len -= 768;
        run -= 768;
        *next++ = 3;
        *next++ = UC(run >> 8); // Forced high count
      }
      else if (run > 255) { // Between 256 and 767, medium sequence
        *next++ = UC(run >> 8); // High count
      }

      *next++ = UC(run & 0xff); // Low count, always present
      *next++ = b;   // End with Value, always present
    }
    ibuffer += run;
    len -= run;
  }
  RET_NOW;
}

// Check that another input byte can be read, return now otherwise
// Adjusts the input length too
#define CHECK_INPUT if (ilen == 0) RET_NOW
// Reads a byte and adjust the input pointer
#define NEXT_BYTE UC(*ibuffer++)

//
// C decompress function, returns actual decompressed size
// Stops when either olen is reached or when ilen is exhausted
// returns the number of output bytes written
//
static size_t fromYarn(const char *ibuffer, size_t ilen, char *obuf, size_t olen, Byte CODE = 0xC3) {
  Byte *next = reinterpret_cast<Byte *>(obuf);
  while (ilen > 0 && olen > 0) {
    // It is safe to read and write one byte
    Byte b = NEXT_BYTE;
    ilen--;
    if (b != CODE) { // Copy single chars
      *next++ = b;
      olen--;
    }
    else { // Marker found, which type of sequence is it?
      CHECK_INPUT;
      b = NEXT_BYTE;
      ilen--;
      if (b == 0) { // Emit one code
        *next++ = CODE;
        olen--;
      }
      else { // Sequence
        size_t run = 0;
        if (b < 4) {
          run = 256 * b;
          if (3 == b) { // Second byte of high count
            CHECK_INPUT;
            run += 256 * NEXT_BYTE;
            ilen--;
          }
          CHECK_INPUT;
          run += NEXT_BYTE;
          ilen--;
        }
        else { // Single byte count
          run = b;
        }

        // Write the sequence out, after checking
        if (olen < run) RET_NOW;
        CHECK_INPUT;
        b = NEXT_BYTE;
        ilen--;
        memset(next, b, run);

        next += run;
        olen -= run;
      }
    }
  }
  RET_NOW;
}


// Returns the least used byte value from a buffer
static Byte getLeastUsed(const Byte *src, size_t len) {
  std::vector<unsigned int> hist(256, 0);
  while (len)
  {
    --len;
    hist[*src++]++;
  }
  return UC(std::min_element(hist.begin(), hist.end()) - hist.begin());
}

// Read from a packed source until the src is exhausted
// Returns true if all output buffer was filled, 0 otherwise
int RLEC3Packer::load(storage_manager *src, storage_manager *dst)
{
  // Use the first char in the input buffer as marker code
  return dst->size == fromYarn(src->buffer + 1, src->size - 1,
    dst->buffer, dst->size, *src->buffer);
}

//
// Picks the least use value as the marker code and stores it first in the data
// This choice improves the worst case expansion, which becomes (1 + N / 256 + N) : N
// It also improves compression in general
// Makes best case marginally worse because the chosen code adds one byte to the output
//

int RLEC3Packer::store(storage_manager *src, storage_manager *dst)
{
  size_t N = src->size;
  if (dst->size < 1 + N + N / 256)
    return 0; // Failed, destination might overflow
  Byte c = getLeastUsed(reinterpret_cast<const Byte *>(src->buffer), src->size);
  *dst->buffer++ = static_cast<char>(c);
  dst->size = 1 + toYarn(src->buffer, dst->buffer, src->size, c);
  return 1; // Success, size is in dst->size
}

NAMESPACE_MRF_END
