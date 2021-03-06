// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_INTERPROCESS_FRAME_POOL_H_
#define COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_INTERPROCESS_FRAME_POOL_H_

#include <utility>
#include <vector>

#include "base/callback.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/weak_ptr.h"
#include "base/sequence_checker.h"
#include "base/time/time.h"
#include "components/viz/service/viz_service_export.h"
#include "media/base/video_frame.h"
#include "media/base/video_types.h"
#include "mojo/public/cpp/system/buffer.h"
#include "ui/gfx/geometry/size.h"

namespace viz {

// A pool of VideoFrames backed by shared memory that can be transported
// efficiently across mojo service boundaries.
class VIZ_SERVICE_EXPORT InterprocessFramePool {
 public:
  using BufferAndSize = std::pair<mojo::ScopedSharedBufferHandle, size_t>;

  // |capacity| is the maximum number of pooled VideoFrames; but they can be of
  // any byte size.
  explicit InterprocessFramePool(int capacity);
  ~InterprocessFramePool();

  // Reserves a buffer from the pool and creates a VideoFrame to wrap its shared
  // memory. When the ref-count of the returned VideoFrame goes to zero, the
  // reservation is released and the frame becomes available for re-use. Returns
  // null if the pool is fully utilized.
  scoped_refptr<media::VideoFrame> ReserveVideoFrame(
      media::VideoPixelFormat format,
      const gfx::Size& size);

  // Finds the last VideoFrame delivered, and if it has been returned back to
  // this pool already, re-materializes it. Otherwise, null is returned. This is
  // used when the client knows the content of the video frame has not changed
  // and is trying to avoid having to re-populate a new VideoFrame with the same
  // content.
  scoped_refptr<media::VideoFrame> ResurrectLastVideoFrame(
      media::VideoPixelFormat expected_format,
      const gfx::Size& expected_size);

  // Returns a cloned handle to the shared memory backing |frame| and its size
  // in bytes. Note that the client should not allow the ref-count of the
  // VideoFrame to reach zero until downstream consumers are finished using it,
  // as this would allow the shared memory to be re-used for a later frame.
  //
  // Calling this method is a signal that |frame| should be considered the
  // last-delivered frame, for the purposes of ResurrectLastVideoFrame().
  BufferAndSize CloneHandleForDelivery(const media::VideoFrame* frame);

  // Returns the current pool utilization, based on the number of VideoFrames
  // being held by the client.
  float GetUtilization() const;

 private:
  // Tracking data for pooled buffers.
  struct PooledBuffer {
    mojo::ScopedSharedBufferHandle buffer;
    size_t bytes_allocated;
    mojo::ScopedSharedBufferMapping mapping;

    PooledBuffer();
    PooledBuffer(PooledBuffer&&) noexcept;
    PooledBuffer& operator=(PooledBuffer&&) noexcept;
    ~PooledBuffer();
  };

  // Creates a media::VideoFrame backed by a specific pooled buffer.
  scoped_refptr<media::VideoFrame> WrapBuffer(PooledBuffer pooled_buffer,
                                              media::VideoPixelFormat format,
                                              const gfx::Size& size);

  // Called when a reserved/resurrected VideoFrame goes out of scope, to remove
  // the entry from |utilized_buffers_| and place the PooledBuffer back into
  // |available_buffers_|.
  void OnFrameWrapperDestroyed(const media::VideoFrame* frame,
                               mojo::ScopedSharedBufferMapping mapping);

  // Returns true if a shared memory failure can be logged. This is a rate
  // throttle, to ensure the logs aren't spammed in chronically low-memory
  // environments.
  bool CanLogSharedMemoryFailure();

  // The maximum number of buffers. However, the buffers themselves can be of
  // any byte size.
  const size_t capacity_;

  // Buffers available for immediate re-use. Generally, it is best to push and
  // pop from the back of this vector so that the most-recently-used buffers are
  // re-used. This will help prevent excessive operating system paging in low-
  // memory situations.
  std::vector<PooledBuffer> available_buffers_;

  // A map of externally-owned VideoFrames and the tracking information about
  // the shared memory buffer backing them.
  base::flat_map<const media::VideoFrame*, PooledBuffer> utilized_buffers_;

  // The handle of the buffer that was last delivered, along with its format and
  // size. ResurrectLastVideoFrame() uses this information to locate and confirm
  // that a prior frame can be resurrected.
  MojoHandle resurrectable_handle_ = MOJO_HANDLE_INVALID;
  media::VideoPixelFormat last_delivered_format_ = media::PIXEL_FORMAT_UNKNOWN;
  gfx::Size last_delivered_size_;

  // The time at which the last shared memory allocation or mapping failed.
  base::TimeTicks last_fail_log_time_;

  // The amount of time that should elapsed between log warnings about shared
  // memory allocation/mapping failures.
  static constexpr base::TimeDelta kMinLoggingPeriod =
      base::TimeDelta::FromSeconds(10);

  SEQUENCE_CHECKER(sequence_checker_);

  base::WeakPtrFactory<InterprocessFramePool> weak_factory_;

  DISALLOW_COPY_AND_ASSIGN(InterprocessFramePool);
};

}  // namespace viz

#endif  // COMPONENTS_VIZ_SERVICE_FRAME_SINKS_VIDEO_CAPTURE_INTERPROCESS_FRAME_POOL_H_
