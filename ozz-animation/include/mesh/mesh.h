
#ifndef OZZ_GAME_MESH_H_
#define OZZ_GAME_MESH_H_

#include "ozz/base/containers/vector.h"
#include "ozz/base/containers/vector_archive.h"
#include "ozz/base/io/archive_traits.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/vec_float.h"
#include "ozz/base/platform.h"

#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"

#include <dmsdk/dlib/buffer.h>


namespace game
{
    // Defines a mesh with skinning information (joint indices and weights).
    // The mesh is subdivided into parts that group vertices according to their
    // number of influencing joints. Triangle indices are shared across mesh parts.
    struct Mesh {
    // Number of triangle indices for the mesh.
    int triangle_index_count() const {
        return static_cast<int>(triangle_indices.size());
    }

    // Number of vertices for all mesh parts.
    int vertex_count() const {
        int vertex_count = 0;
        for (size_t i = 0; i < parts.size(); ++i) {
        vertex_count += parts[i].vertex_count();
        }
        return vertex_count;
    }

    // Number of vertices for all mesh parts.
    int normal_count() const {
        int normal_count = 0;
        for (size_t i = 0; i < parts.size(); ++i) {
            normal_count += parts[i].normal_count();
        }
        return normal_count;
    }

    // Number of vertices for all mesh parts.
    int uv_count() const {
        int uv_count = 0;
        for (size_t i = 0; i < parts.size(); ++i) {
            uv_count += parts[i].uv_count();
        }
        return uv_count;
    }    

    // Maximum number of joints influences for all mesh parts.
    int max_influences_count() const {
        int max_influences_count = 0;
        for (size_t i = 0; i < parts.size(); ++i) {
        const int influences_count = parts[i].influences_count();
        max_influences_count = influences_count > max_influences_count
                                    ? influences_count
                                    : max_influences_count;
        }
        return max_influences_count;
    }

    // Test if the mesh has skinning informations.
    bool skinned() const {
        return !inverse_bind_poses.empty();
    }

    // Returns the number of joints used to skin the mesh.
    int num_joints() const { return static_cast<int>(inverse_bind_poses.size()); }

    // Returns the highest joint number used in the skeleton.
    int highest_joint_index() const {
        // Takes advantage that joint_remaps is sorted.
        return joint_remaps.size() != 0 ? static_cast<int>(joint_remaps.back()) : 0;
    }

    // Defines a portion of the mesh. A mesh is subdivided in sets of vertices
    // with the same number of joint influences.
    struct Part {
        int vertex_count() const { return static_cast<int>(positions.size()) / 3; }
        int normal_count() const { return static_cast<int>(normals.size()) / 3; }
        int uv_count() const { return static_cast<int>(uvs.size()) / 2; }

        int influences_count() const {
        const int _vertex_count = vertex_count();
        if (_vertex_count == 0) {
            return 0;
        }
        return static_cast<int>(joint_indices.size()) / _vertex_count;
        }

        typedef ozz::vector<float> Positions;
        Positions positions;
        enum { kPositionsCpnts = 3 };  // x, y, z components

        typedef ozz::vector<float> Normals;
        Normals normals;
        enum { kNormalsCpnts = 3 };  // x, y, z components

        typedef ozz::vector<float> Tangents;
        Tangents tangents;
        enum { kTangentsCpnts = 4 };  // x, y, z, right or left handed.

        typedef ozz::vector<float> UVs;
        UVs uvs;  // u, v components
        enum { kUVsCpnts = 2 };

        typedef ozz::vector<uint8_t> Colors;
        Colors colors;
        enum { kColorsCpnts = 4 };  // r, g, b, a components

        typedef ozz::vector<uint16_t> JointIndices;
        JointIndices joint_indices;  // Stride equals influences_count

        typedef ozz::vector<float> JointWeights;
        JointWeights joint_weights;  // Stride equals influences_count - 1
    };
    typedef ozz::vector<Part> Parts;
    Parts parts;

    // Triangles indices. Indices are shared across all parts.
    typedef ozz::vector<uint16_t> TriangleIndices;
    TriangleIndices triangle_indices;

    // Joints remapping indices. As a skin might be influenced by a part of the
    // skeleton only, joint indices and inverse bind pose matrices are reordered
    // to contain only used ones. Note that this array is sorted.
    typedef ozz::vector<uint16_t> JointRemaps;
    JointRemaps joint_remaps;

    // Inverse bind-pose matrices. These are only available for skinned meshes.
    typedef ozz::vector<ozz::math::Float4x4> InversBindPoses;
    InversBindPoses inverse_bind_poses;

    // Used at runtime to set vert buffers - dont like this at all!
    dmBuffer::HBuffer buffer;

    };
}

namespace ozz {
namespace io {

OZZ_IO_TYPE_TAG("ozz-sample-Mesh-Part", game::Mesh::Part)
OZZ_IO_TYPE_VERSION(1, game::Mesh::Part)

template <>
struct Extern<game::Mesh::Part> {
  static void Save(OArchive& _archive, const game::Mesh::Part* _parts,
                   size_t _count);
  static void Load(IArchive& _archive, game::Mesh::Part* _parts,
                   size_t _count, uint32_t _version);
};

OZZ_IO_TYPE_TAG("ozz-sample-Mesh", game::Mesh)
OZZ_IO_TYPE_VERSION(1, game::Mesh)

template <>
struct Extern<game::Mesh> {
  static void Save(OArchive& _archive, const game::Mesh* _meshes,
                   size_t _count);
  static void Load(IArchive& _archive, game::Mesh* _meshes, size_t _count,
                   uint32_t _version);
};

}  // namespace io
}  // namespace ozz

#endif //OZZ_GAME_MESH_H_