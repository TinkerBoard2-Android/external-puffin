// Copyright 2017 The Chromium OS Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "puffin/src/include/puffin/stream.h"

#include <numeric>

#include "gtest/gtest.h"

#include "puffin/src/include/puffin/huffer.h"
#include "puffin/src/include/puffin/puffer.h"
#include "puffin/src/puffin_stream.h"
#include "puffin/src/unittest_common.h"

namespace puffin {

using std::string;
using std::shared_ptr;

class StreamTest : public ::testing::Test {
 public:
  // |data| is the content of stream as a buffer.
  void TestRead(StreamInterface* stream, const Buffer& data) {
    // Test read
    Buffer buf(data.size(), 0);

    ASSERT_TRUE(stream->Seek(0));
    ASSERT_TRUE(stream->Read(buf.data(), buf.size()));
    for (size_t idx = 0; idx < buf.size(); idx++) {
      ASSERT_EQ(buf[idx], data[idx]);
    }

    // No reading out of data boundary.
    Buffer tmp(100);
    size_t size;
    ASSERT_TRUE(stream->GetSize(&size));
    ASSERT_TRUE(stream->Seek(size));
    ASSERT_TRUE(stream->Read(tmp.data(), 0));
    ASSERT_FALSE(stream->Read(tmp.data(), 1));
    ASSERT_FALSE(stream->Read(tmp.data(), 2));
    ASSERT_FALSE(stream->Read(tmp.data(), 3));
    ASSERT_FALSE(stream->Read(tmp.data(), 100));

    ASSERT_TRUE(stream->Seek(size - 1));
    ASSERT_TRUE(stream->Read(tmp.data(), 0));
    ASSERT_TRUE(stream->Read(tmp.data(), 1));

    ASSERT_TRUE(stream->Seek(size - 1));
    ASSERT_FALSE(stream->Read(tmp.data(), 2));
    ASSERT_FALSE(stream->Read(tmp.data(), 3));
    ASSERT_FALSE(stream->Read(tmp.data(), 100));

    // Read the entire buffer one byte at a time.
    ASSERT_TRUE(stream->Seek(0));
    for (size_t idx = 0; idx < size; idx++) {
      uint8_t u;
      ASSERT_TRUE(stream->Read(&u, 1));
      ASSERT_EQ(u, buf[idx]);
    }

    // Read the entire buffer one byte at a time and set offset for each read.
    for (size_t idx = 0; idx < size; idx++) {
      uint8_t u;
      ASSERT_TRUE(stream->Seek(idx));
      ASSERT_TRUE(stream->Read(&u, 1));
      ASSERT_EQ(u, buf[idx]);
    }

    // Read random lengths from random offsets.
    tmp.resize(buf.size());
    srand(time(nullptr));
    uint32_t rand_seed;
    for (size_t idx = 0; idx < 10000; idx++) {
      // zero to full size available.
      size_t size = rand_r(&rand_seed) % (buf.size() + 1);
      size_t max_start = buf.size() - size;
      size_t start = rand_r(&rand_seed) % (max_start + 1);
      ASSERT_TRUE(stream->Seek(start));
      ASSERT_TRUE(stream->Read(tmp.data(), size));
      for (size_t idx = 0; idx < size; idx++) {
        ASSERT_EQ(tmp[idx], buf[start + idx]);
      }
    }
  }

  void TestWriteBoundary(StreamInterface* stream) {
    Buffer buf(10);
    // Writing out of boundary is fine.
    size_t size;
    ASSERT_TRUE(stream->GetSize(&size));
    ASSERT_TRUE(stream->Seek(size));
    ASSERT_TRUE(stream->Write(buf.data(), 0));
    ASSERT_TRUE(stream->Write(buf.data(), 1));
    ASSERT_TRUE(stream->Write(buf.data(), 2));
    ASSERT_TRUE(stream->Write(buf.data(), 3));
    ASSERT_TRUE(stream->Write(buf.data(), 10));

    ASSERT_TRUE(stream->GetSize(&size));
    ASSERT_TRUE(stream->Seek(size - 1));
    ASSERT_TRUE(stream->Write(buf.data(), 0));
    ASSERT_TRUE(stream->Write(buf.data(), 1));

    ASSERT_TRUE(stream->GetSize(&size));
    ASSERT_TRUE(stream->Seek(size - 1));
    ASSERT_TRUE(stream->Write(buf.data(), 2));
    ASSERT_TRUE(stream->Write(buf.data(), 3));
    ASSERT_TRUE(stream->Write(buf.data(), 10));
  }

  void TestWrite(StreamInterface* stream) {
    size_t size;
    ASSERT_TRUE(stream->GetSize(&size));
    Buffer buf1(size);
    Buffer buf2(size);
    std::iota(buf1.begin(), buf1.end(), 0);

    // Make sure the write works.
    ASSERT_TRUE(stream->Seek(0));
    ASSERT_TRUE(stream->Write(buf1.data(), buf1.size()));
    ASSERT_TRUE(stream->Seek(0));
    ASSERT_TRUE(stream->Read(buf2.data(), buf2.size()));
    ASSERT_EQ(buf1, buf2);

    std::fill(buf2.begin(), buf2.end(), 0);

    // Write entire buffer one byte at a time. (all zeros).
    ASSERT_TRUE(stream->Seek(0));
    for (size_t idx = 0; idx < buf2.size(); idx++) {
      ASSERT_TRUE(stream->Write(&buf2[idx], 1));
    }

    ASSERT_TRUE(stream->Seek(0));
    ASSERT_TRUE(stream->Read(buf1.data(), buf1.size()));
    ASSERT_EQ(buf1, buf2);
  }

  // Call this at the end before |TestClose|.
  void TestSeek(StreamInterface* stream, bool seek_end_is_fine) {
    size_t size, offset;
    ASSERT_TRUE(stream->GetSize(&size));
    ASSERT_TRUE(stream->Seek(size));
    ASSERT_TRUE(stream->GetOffset(&offset));
    ASSERT_EQ(offset, size);
    ASSERT_TRUE(stream->Seek(10));
    ASSERT_TRUE(stream->GetOffset(&offset));
    ASSERT_EQ(offset, 10);
    ASSERT_TRUE(stream->Seek(0));
    ASSERT_TRUE(stream->GetOffset(&offset));
    ASSERT_EQ(offset, 0);
    // Test end of stream offset.
    ASSERT_EQ(stream->Seek(size + 1), seek_end_is_fine);
  }

  void TestClose(StreamInterface* stream) { ASSERT_TRUE(stream->Close()); }
};

TEST_F(StreamTest, TestMemoryStream) {
  SharedBufferPtr buf(new Buffer(105));
  std::iota(buf->begin(), buf->end(), 0);

  ASSERT_FALSE(MemoryStream::Create(buf, false, false));
  auto stream = MemoryStream::Create(buf, true, true);

  TestRead(stream.get(), *buf);
  TestWrite(stream.get());
  TestWriteBoundary(stream.get());
  TestSeek(stream.get(), false);
  TestClose(stream.get());
}

TEST_F(StreamTest, TestFileStream) {
  string filepath("/tmp/test_filepath");
  ScopedPathUnlinker scoped_unlinker(filepath);
  ASSERT_FALSE(FileStream::Open(filepath, false, false));

  auto stream = FileStream::Open(filepath, true, true);
  ASSERT_TRUE(stream.get() != nullptr);
  // Doesn't matter if it is not initialized. I will be overridden.
  Buffer buf(105);
  std::iota(buf.begin(), buf.end(), 0);

  ASSERT_TRUE(stream->Write(buf.data(), buf.size()));

  TestRead(stream.get(), buf);
  TestWrite(stream.get());
  TestWriteBoundary(stream.get());
  TestSeek(stream.get(), true);
  TestClose(stream.get());
}

TEST_F(StreamTest, PuffinStreamTest) {
  SharedBufferPtr buf(new Buffer(kDeflates8));
  shared_ptr<Puffer> puffer(new Puffer());
  auto read_stream = PuffinStream::CreateForPuff(
      MemoryStream::Create(buf, true, false), puffer, kPuffs8.size(),
      kSubblockDeflateExtents8, kPuffExtents8);

  TestRead(read_stream.get(), kPuffs8);
  TestSeek(read_stream.get(), false);
  TestClose(read_stream.get());

  SharedBufferPtr buf1(new Buffer(kDeflates8.size()));
  shared_ptr<Huffer> huffer(new Huffer());
  auto write_stream = PuffinStream::CreateForHuff(
      MemoryStream::Create(buf1, false, true), huffer, kPuffs8.size(),
      kSubblockDeflateExtents8, kPuffExtents8);

  ASSERT_TRUE(write_stream->Seek(0));
  for (size_t idx = 0; idx < kPuffs8.size(); idx++) {
    ASSERT_TRUE(write_stream->Write(&kPuffs8[idx], 1));
  }
  // Make sure the write works
  ASSERT_EQ(*buf1, kDeflates8);

  std::fill(buf1->begin(), buf1->end(), 0);
  ASSERT_TRUE(write_stream->Seek(0));
  ASSERT_TRUE(write_stream->Write(kPuffs8.data(), kPuffs8.size()));
  // Check its correctness.
  ASSERT_EQ(*buf1, kDeflates8);

  // Write entire buffer one byte at a time. (all zeros).
  std::fill(buf1->begin(), buf1->end(), 0);
  ASSERT_TRUE(write_stream->Seek(0));
  for (const auto& byte : kPuffs8) {
    ASSERT_TRUE(write_stream->Write(&byte, 1));
  }
  // Check its correctness.
  ASSERT_EQ(*buf1, kDeflates8);

  // No TestSeek is needed as PuffinStream is not supposed to seek to anywhere
  // except 0.
  TestClose(write_stream.get());
}

}  // namespace puffin
