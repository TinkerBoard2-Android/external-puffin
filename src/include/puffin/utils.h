// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef SRC_INCLUDE_PUFFIN_UTILS_H_
#define SRC_INCLUDE_PUFFIN_UTILS_H_

#include <string>
#include <vector>

#include "puffin/common.h"
#include "puffin/stream.h"

namespace puffin {

// Counts the number of bytes in a list of |ByteExtent|s.
PUFFIN_EXPORT
size_t BytesInByteExtents(const std::vector<ByteExtent>& extents);

// Converts an array of |ByteExtens| or |BitExtents| to a string. Each extent
// has format "offset:length" and are comma separated.
template <typename T>
PUFFIN_EXPORT std::string ExtentsToString(const T& extents) {
  std::string str;
  for (const auto& extent : extents) {
    str += std::to_string(extent.offset) + ":" + std::to_string(extent.length) +
           ",";
  }
  return str;
}

// Locates deflate buffer locations for a set of zlib buffers |zlibs| in
// |src|. It performs by removing header and footer bytes from the zlib stream.
PUFFIN_EXPORT
bool LocateDeflatesInZlibBlocks(const UniqueStreamPtr& src,
                                const std::vector<ByteExtent>& zlibs,
                                std::vector<BitExtent>* deflates);

// Reads the deflates in from |deflates| and returns a list of its subblock
// locations. Each subblock in practice is a deflate stream by itself.
// Assumption is that the first subblock in each deflate in |deflates| start in
// byte boundary.
PUFFIN_EXPORT
bool FindDeflateSubBlocks(const UniqueStreamPtr& src,
                          const std::vector<ByteExtent>& deflates,
                          std::vector<BitExtent>* subblock_deflates);

// Finds the location of puffs in the deflate stream |src| based on the location
// of |deflates| and populates the |puffs|. We assume |deflates| are sorted by
// their offset value. |out_puff_size| will be the size of the puff stream.
PUFFIN_EXPORT
bool FindPuffLocations(const UniqueStreamPtr& src,
                       const std::vector<BitExtent>& deflates,
                       std::vector<ByteExtent>* puffs,
                       size_t* out_puff_size);

}  // namespace puffin

#endif  // SRC_INCLUDE_PUFFIN_UTILS_H_
