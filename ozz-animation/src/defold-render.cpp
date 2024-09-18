
#include "mesh/mesh.h"
#include "controller/controller.h"

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/geometry/runtime/skinning_job.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/log.h"
#include "ozz/base/span.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/containers/vector_archive.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/vec_float.h"
#include "ozz/base/maths/box.h"
#include "ozz/base/span.h"
#include "ozz/options/options.h"

using namespace ozz;
using namespace game;

bool DrawDefoldSkinnedMesh(const game::Mesh &_mesh, const span<math::Float4x4> _skinning_matrices, const ozz::math::Float4x4 &_transform)
{
    const int vertex_count = _mesh.vertex_count();

    // Positions and normals are interleaved to improve caching while executing
    // skinning job.

    const int32_t positions_offset = 0;
    const int32_t normals_offset = 0;

    // Reallocate vertex buffer.
    float* vert_bytes = 0x0;
    uint32_t vert_count = 0;
    uint32_t vert_comp = 0;
    uint32_t positions_stride = 0;
    dmBuffer::Result r = dmBuffer::GetStream(_mesh.buffer, dmHashString64("position"), (void**)&vert_bytes, &vert_count, &vert_comp, &positions_stride);
    if(vert_comp == 0 || vert_count == 0) return false;
    printf("pos stride: %d %d %d %d\n", (int)r, vert_count, vert_comp, positions_stride);
    float *vertptr = (float *)calloc(vertex_count * 3, sizeof(float));
    
    float* norm_bytes = 0x0;
    uint32_t norm_count = 0;
    uint32_t norm_comp = 0;
    uint32_t normals_stride = 0;
    r = dmBuffer::GetStream(_mesh.buffer, dmHashString64("normal"), (void**)&norm_bytes, &norm_count, &norm_comp, &normals_stride);
    if(norm_comp == 0 || norm_count == 0) return false;
    float *normptr = (float *)calloc(vertex_count * 3, sizeof(float));

    // Iterate mesh parts and fills vbo.
    // Runs a skinning job per mesh part. Triangle indices are shared
    // across parts.
    size_t processed_vertex_count = 0;
    for (size_t i = 0; i < _mesh.parts.size(); ++i)
    {
        const game::Mesh::Part &part = _mesh.parts[i];

        // Skip this iteration if no vertex.
        const size_t part_vertex_count = part.positions.size() / 3;
        if (part_vertex_count == 0)
        {
            continue;
        }

        // Fills the job.
        ozz::geometry::SkinningJob skinning_job;
        skinning_job.vertex_count = static_cast<int>(part_vertex_count);
        const int part_influences_count = part.influences_count();

        // Clamps joints influence count according to the option.
        skinning_job.influences_count = part_influences_count;

        // Setup skinning matrices, that came from the animation stage before being
        // multiplied by inverse model-space bind-pose.
        skinning_job.joint_matrices = _skinning_matrices;

        // Setup joint's indices.
        skinning_job.joint_indices = make_span(part.joint_indices);
        skinning_job.joint_indices_stride = sizeof(uint16_t) * part_influences_count;

        // Setup joint's weights.
        if (part_influences_count > 1)
        {
            skinning_job.joint_weights = make_span(part.joint_weights);
            skinning_job.joint_weights_stride = sizeof(float) * (part_influences_count - 1);
        }

        // Setup input positions, coming from the loaded mesh.
        skinning_job.in_positions = make_span(part.positions);
        skinning_job.in_positions_stride = sizeof(float) * game::Mesh::Part::kPositionsCpnts;

        // Setup output positions, coming from the rendering output mesh buffers.
        // We need to offset the buffer every loop.
        float *out_positions_begin = reinterpret_cast<float *>(ozz::PointerStride(vertptr, positions_offset + processed_vertex_count * positions_stride));
        float *out_positions_end = ozz::PointerStride(out_positions_begin, part_vertex_count * positions_stride);
        skinning_job.out_positions = {out_positions_begin, out_positions_end};
        skinning_job.out_positions_stride = positions_stride;

        // Setup normals if input are provided.
        float *out_normal_begin = reinterpret_cast<float *>(ozz::PointerStride(normptr, normals_offset + processed_vertex_count * normals_stride));
        float *out_normal_end = ozz::PointerStride(out_normal_begin, part_vertex_count * normals_stride);

        if (part.normals.size() / game::Mesh::Part::kNormalsCpnts == part_vertex_count)
        {
            // Setup input normals, coming from the loaded mesh.
            skinning_job.in_normals = make_span(part.normals);
            skinning_job.in_normals_stride = sizeof(float) * game::Mesh::Part::kNormalsCpnts;

            // Setup output normals, coming from the rendering output mesh buffers.
            // We need to offset the buffer every loop.
            skinning_job.out_normals = {out_normal_begin, out_normal_end};
            skinning_job.out_normals_stride = normals_stride;
        }
        else
        {
            // Fills output with default normals.
            for (float *normal = out_normal_begin; normal < out_normal_end; normal = ozz::PointerStride(normal, normals_stride))
            {
                normal[0] = 0.f;
                normal[1] = 1.f;
                normal[2] = 0.f;
            }
        }

        // Execute the job, which should succeed unless a parameter is invalid.
        if (!skinning_job.Run())
        {
            printf("Bad juju\n");
            return false;
        }

        // After skinning put the output data back into our rendering 
        for (int i = 0; i < vertex_count; ++i) {
            for (int c = 0; c < vert_comp; ++c) {
                vert_bytes[c] = skinning_job.out_positions[_mesh.triangle_indices[i] * vert_comp + c];
                norm_bytes[c] = skinning_job.out_normals[_mesh.triangle_indices[i] * vert_comp + c];
            }
            vert_bytes += positions_stride;
            norm_bytes += normals_stride;
        }
        free(vertptr);
        free(normptr);
                        
        printf("Good juju\n");
        processed_vertex_count += part_vertex_count;
    }
    return true;
}