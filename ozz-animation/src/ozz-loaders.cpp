
#include <cassert>
#include <chrono>
#include <limits>

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


bool LoadSkeleton(const char* _filename, ozz::animation::Skeleton* _skeleton) {
  assert(_filename && _skeleton);
  ozz::log::Out() << "Loading skeleton archive " << _filename << "."
                  << std::endl;
  ozz::io::File file(_filename, "rb");
  if (!file.opened()) {
    ozz::log::Err() << "Failed to open skeleton file " << _filename << "."
                    << std::endl;
    return false;
  }
  ozz::io::IArchive archive(&file);
  if (!archive.TestTag<ozz::animation::Skeleton>()) {
    ozz::log::Err() << "Failed to load skeleton instance from file "
                    << _filename << "." << std::endl;
    return false;
  }

  // Once the tag is validated, reading cannot fail.
  {
    archive >> *_skeleton;
  }
  return true;
}

bool LoadAnimation(const char* _filename, ozz::animation::Animation* _animation) {
  assert(_filename && _animation);
  ozz::log::Out() << "Loading animation archive: " << _filename << "."
                  << std::endl;
  ozz::io::File file(_filename, "rb");
  if (!file.opened()) {
    ozz::log::Err() << "Failed to open animation file " << _filename << "."
                    << std::endl;
    return false;
  }
  ozz::io::IArchive archive(&file);
  if (!archive.TestTag<ozz::animation::Animation>()) {
    ozz::log::Err() << "Failed to load animation instance from file "
                    << _filename << "." << std::endl;
    return false;
  }

  // Once the tag is validated, reading cannot fail.
  {
    archive >> *_animation;
  }

  return true;
}

bool LoadMeshes(const char* _filename, ozz::vector<game::Mesh>* _meshes) {
  assert(_filename && _meshes);
  ozz::log::Out() << "Loading meshes archive: " << _filename << "."
                  << std::endl;
  ozz::io::File file(_filename, "rb");
  if (!file.opened()) {
    ozz::log::Err() << "Failed to open mesh file " << _filename << "."
                    << std::endl;
    return false;
  }
  ozz::io::IArchive archive(&file);  
  {
    // ProfileFctLog profile{"Meshes loading time"};
    while (archive.TestTag<game::Mesh>()) {
      _meshes->resize(_meshes->size() + 1);
      archive >> _meshes->back();
    }
  }
  return true;
}


namespace ozz {
namespace io {

void Extern<game::Mesh::Part>::Save(OArchive& _archive,
                                      const game::Mesh::Part* _parts,
                                      size_t _count) {
  for (size_t i = 0; i < _count; ++i) {
    const game::Mesh::Part& part = _parts[i];
    _archive << part.positions;
    _archive << part.normals;
    _archive << part.tangents;
    _archive << part.uvs;
    _archive << part.colors;
    _archive << part.joint_indices;
    _archive << part.joint_weights;
  }
}

void Extern<game::Mesh::Part>::Load(IArchive& _archive,
                                      game::Mesh::Part* _parts, size_t _count,
                                      uint32_t _version) {
  (void)_version;
  for (size_t i = 0; i < _count; ++i) {
    game::Mesh::Part& part = _parts[i];
    _archive >> part.positions;
    _archive >> part.normals;
    _archive >> part.tangents;
    _archive >> part.uvs;
    _archive >> part.colors;
    _archive >> part.joint_indices;
    _archive >> part.joint_weights;
  }
}

void Extern<game::Mesh>::Save(OArchive& _archive, const game::Mesh* _meshes,
                                size_t _count) {
  for (size_t i = 0; i < _count; ++i) {
    const game::Mesh& mesh = _meshes[i];
    _archive << mesh.parts;
    _archive << mesh.triangle_indices;
    _archive << mesh.joint_remaps;
    _archive << mesh.inverse_bind_poses;
  }
}

void Extern<game::Mesh>::Load(IArchive& _archive, game::Mesh* _meshes,
                                size_t _count, uint32_t _version) {
  (void)_version;
  for (size_t i = 0; i < _count; ++i) {
    game::Mesh& mesh = _meshes[i];
    _archive >> mesh.parts;
    _archive >> mesh.triangle_indices;
    _archive >> mesh.joint_remaps;
    _archive >> mesh.inverse_bind_poses;
  }
}
}  // namespace io
}  // namespace ozz
