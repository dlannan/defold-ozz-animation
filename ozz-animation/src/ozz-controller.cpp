#include "ozz/animation/offline/raw_animation.h"
#include "ozz/animation/offline/raw_skeleton.h"
#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/animation/runtime/track.h"
#include "ozz/base/containers/vector_archive.h"
#include "ozz/base/memory/allocator.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/log.h"
#include "ozz/base/maths/box.h"
#include "ozz/base/maths/simd_math_archive.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/simd_quaternion.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/memory/allocator.h"
#include "ozz/geometry/runtime/skinning_job.h"

#include "mesh/mesh.h"
#include "controller/controller.h"

namespace game {

PlaybackController::PlaybackController()
    : time_ratio_(0.f),
      previous_time_ratio_(0.f),
      playback_speed_(1.f),
      play_(true),
      loop_(true) {}

void PlaybackController::Update(const ozz::animation::Animation& _animation,
                                float _dt) {
  float new_time = time_ratio_;

  if (play_) {
    new_time = time_ratio_ + _dt * playback_speed_ / _animation.duration();
  }

  // Must be called even if time doesn't change, in order to update previous
  // frame time ratio. Uses set_time_ratio function in order to update
  // previous_time_ and wrap time value in the unit interval (depending on loop
  // mode).
  set_time_ratio(new_time);
}

void PlaybackController::set_time_ratio(float _ratio) {
  previous_time_ratio_ = time_ratio_;
  if (loop_) {
    // Wraps in the unit interval [0:1], even for negative values (the reason
    // for using floorf).
    time_ratio_ = _ratio - floorf(_ratio);
  } else {
    // Clamps in the unit interval [0:1].
    time_ratio_ = ozz::math::Clamp(0.f, _ratio, 1.f);
  }
}

// Gets animation current time.
float PlaybackController::time_ratio() const { return time_ratio_; }

// Gets animation time of last update.
float PlaybackController::previous_time_ratio() const {
  return previous_time_ratio_;
}

void PlaybackController::Reset() {
  previous_time_ratio_ = time_ratio_ = 0.f;
  playback_speed_ = 1.f;
  play_ = true;
}

}