#ifndef OZZ_GAME_CONTROLLER_H_
#define OZZ_GAME_CONTROLLER_H_

#include "ozz/base/containers/vector.h"
#include "ozz/base/platform.h"
#include "ozz/base/span.h"

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"

namespace game 
{

// Utility class that helps with controlling animation playback time. Time is
// computed every update according to the dt given by the caller, playback speed
// and "play" state.
// Internally time is stored as a ratio in unit interval [0,1], as expected by
// ozz runtime animation jobs.
// OnGui function allows to tweak controller parameters through the application
// Gui.
class PlaybackController {
 public:
    // Constructor.
    PlaybackController();

    // Sets animation current time.
    void set_time_ratio(float _time);

    // Gets animation current time.
    float time_ratio() const;

    // Gets animation time ratio of last update. Useful when the range between
    // previous and current frame needs to pe processed.
    float previous_time_ratio() const;

    // Sets playback speed.
    void set_playback_speed(float _speed) { playback_speed_ = _speed; }

    // Gets playback speed.
    float playback_speed() const { return playback_speed_; }

    // Sets loop modes. If true, animation time is always clamped between 0 and 1.
    void set_loop(bool _loop) { loop_ = _loop; }

    // Gets loop mode.
    bool loop() const { return loop_; }

    // Updates animation time if in "play" state, according to playback speed and
    // given frame time _dt.
    // Returns true if animation has looped during update
    void Update(const ozz::animation::Animation& _animation, float _dt);

    // Resets all parameters to their default value.
    void Reset();

 private:
    // Current animation time ratio, in the unit interval [0,1], where 0 is the
    // beginning of the animation, 1 is the end.
    float time_ratio_;

    // Time ratio of the previous update.
    float previous_time_ratio_;

    // Playback speed, can be negative in order to play the animation backward.
    float playback_speed_;

    // Animation play mode state: play/pause.
    bool play_;

    // Animation loop mode.
    bool loop_;
};

}


#endif // OZZ_GAME_CONTROLLER_H_
