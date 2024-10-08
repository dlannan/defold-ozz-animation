// ozz.cpp
// Extension lib defines
#define LIB_NAME "ozzanim"
#define MODULE_NAME "ozzanim"

// include the Defold SDK
#include <dmsdk/sdk.h>
#include <string>
#include <vector>

#include "mesh/mesh.h"
#include "controller/controller.h"

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
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

// --------------------------------------------------------------------------------------------------------
// TODO: 
//    Make this more of a reference container so meshes, animations and skeletons
//    are only loaded once rather than for each instance.

typedef struct animObj
{
    std::string                             skeleton_filename;
    std::string                             animation_filename;
    std::string                             mesh_filename;

    int                                     num_joints;

    ozz::animation::Skeleton                skeleton;
    ozz::animation::Animation               animations;
    ozz::vector<game::Mesh>                 meshes;

    game::PlaybackController                controller;    
    ozz::animation::SamplingJob::Context    context;

    ozz::vector<ozz::math::SoaTransform>    locals;
    
    ozz::vector<ozz::math::Float4x4>        models;    
    ozz::vector<ozz::math::Float4x4>        skinning_matrices;
} _animObj;

// --------------------------------------------------------------------------------------------------------
    
static std::vector<animObj *> g_anims;
static uint64_t g_last_time = 0;

// --------------------------------------------------------------------------------------------------------
extern bool LoadSkeleton(const char* _filename, ozz::animation::Skeleton* _skeleton);
extern bool LoadAnimation(const char* _filename, ozz::animation::Animation* _animation);
extern bool LoadMeshes(const char* _filename, ozz::vector<game::Mesh>* _meshes);

extern bool DrawDefoldSkinnedMesh(const game::Mesh &_mesh, const ozz::span<ozz::math::Float4x4> _skinning_matrices, const ozz::math::Float4x4 &_transform);

// --------------------------------------------------------------------------------------------------------

static int LoadOzz(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);

    const char *skeleton_filename = (char *)luaL_checkstring(L, 1);
    if(skeleton_filename == nullptr)
    {
        printf("[LoadOzz Error] Invalid skeleton filename\n");
        lua_pushnil(L);
        return 1;    
    }

    const char *animation_filename = (char *)luaL_checkstring(L, 2);
    if(animation_filename == nullptr)
    {
        printf("[LoadOzz Error] Invalid animation filename\n");
        lua_pushnil(L);
        return 1;    
    }

    animObj *anim = new animObj();
    anim->skeleton_filename = skeleton_filename;
    anim->animation_filename = animation_filename;

    // Reading skeleton.
    if (!LoadSkeleton(anim->skeleton_filename.c_str(), &anim->skeleton)) {
        printf("[LoadOzz Error] cannot load skeleton: %s.\n", anim->skeleton_filename.c_str());
        lua_pushnil(L);
        return 1;
    }

    // Reading animation
    if (!LoadAnimation(anim->animation_filename.c_str(), &anim->animations)) {
        printf("[LoadOzz Error] cannot load animation: %s.\n", anim->animation_filename.c_str());
        lua_pushnil(L);
        return 1;
    }

    // Allocates runtime buffers.
    const int num_soa_joints = anim->skeleton.num_soa_joints();
    anim->locals.resize(num_soa_joints);
    anim->num_joints = anim->skeleton.num_joints();
    anim->models.resize(anim->num_joints);

    // Allocates a context that matches animation requirements.
    anim->context.Resize(anim->num_joints);

    // Skeleton and animation needs to match.
    if (anim->skeleton.num_joints() != anim->animations.num_tracks()) {
        printf("[LoadOzz Error] joints and tracks do not match.\n");
        lua_pushnil(L);
        return 1;
    }    

    printf("----------------------------------------\n");
    printf("-- Animation Data --\n");
    printf("Numjoints: %d\n", anim->skeleton.num_joints());
    printf("NumTracks: %d\n", anim->animations.num_tracks());
    printf("Animations: %d\n", (uint32_t)anim->animations.size());
    
    int idx = g_anims.size();
    g_anims.push_back(anim);

    lua_pushnumber(L, idx);
    return 1;
}

// --------------------------------------------------------------------------------------------------------

const dmBuffer::StreamDeclaration streams_decl[] = {
    {dmHashString64("position"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
    {dmHashString64("normal"), dmBuffer::VALUE_TYPE_FLOAT32, 3},
    {dmHashString64("texcoord0"), dmBuffer::VALUE_TYPE_FLOAT32, 2},
};

static int SetBufferFromMesh(lua_State* L)
{
    DM_LUA_STACK_CHECK(L, 1);
    int vertcount = luaL_checknumber(L, 1);
    int animid = luaL_checknumber(L, 2);
    int meshid = luaL_checknumber(L, 3);

    animObj *anim = g_anims[animid];
    game::Mesh &mesh = anim->meshes[meshid];
    dmBuffer::Result r = dmBuffer::Create(vertcount, streams_decl, 3, &mesh.buffer);

    float* bytes = 0x0;
    uint32_t count = 0;
    uint32_t components = 0;
    uint32_t stride = 0;
    r = dmBuffer::GetStream(mesh.buffer, dmHashString64("position"), (void**)&bytes, &count, &components, &stride);
    if(components == 0 || count == 0) { lua_pushnil(L); return 1; }
    if (r == dmBuffer::RESULT_OK) {

        size_t floatslen = mesh.vertex_count() * 3;
        float *floatdata = (float *)calloc(floatslen, sizeof(float));    
        int ctr = 0;
        for (size_t i = 0; i < mesh.parts.size(); i++) {
            for (size_t j = 0; j < mesh.parts[i].positions.size(); j++) {
                floatdata[ctr++] = mesh.parts[i].positions[j];
            }
        }

        for (int i = 0; i < count; ++i)
        {
            for (int c = 0; c < components; ++c)
            {
                bytes[c] = floatdata[mesh.triangle_indices[i] * components + c];
            }
            bytes += stride;
        }

        free(floatdata);
    } else {
        // handle error
    }
    
    r = dmBuffer::GetStream(mesh.buffer, dmHashString64("normal"), (void**)&bytes, &count, &components, &stride);
    if(components == 0 || count == 0) { lua_pushnil(L); return 1; }
    if (r == dmBuffer::RESULT_OK) {
        size_t floatslen = mesh.normal_count() * 3;
        float *floatdata = (float *)calloc(floatslen, sizeof(float));    
        int ctr = 0;
        for (size_t i = 0; i < mesh.parts.size(); i++) {
            for (size_t j = 0; j < mesh.parts[i].normals.size(); j++) {
                floatdata[ctr++] = mesh.parts[i].normals[j];
            }
        }

        if (r == dmBuffer::RESULT_OK) {
            for (int i = 0; i < count; ++i)
            {
                for (int c = 0; c < components; ++c)
                {
                    bytes[c] = floatdata[mesh.triangle_indices[i] * components + c];
                }
                bytes += stride;
            }
        } else {
            // handle error
        }
        free(floatdata);
    }

    r = dmBuffer::GetStream(mesh.buffer, dmHashString64("texcoord0"), (void**)&bytes, &count, &components, &stride);
    if(components == 0 || count == 0) { lua_pushnil(L); return 1; }
    if (r == dmBuffer::RESULT_OK) {
        size_t floatslen = mesh.uv_count() * 2;
        float *floatdata = (float *)calloc(floatslen, sizeof(float));    
        int ctr = 0;
        for (size_t i = 0; i < mesh.parts.size(); i++) {
            for (size_t j = 0; j < mesh.parts[i].uvs.size(); j++) {
                floatdata[ctr++] = mesh.parts[i].uvs[j];
            }
        }

        if (r == dmBuffer::RESULT_OK) {
            for (int i = 0; i < count; ++i)
            {
                for (int c = 0; c < components; ++c)
                {
                    if(c == 1) 
                        bytes[c] = 1.0 - floatdata[mesh.triangle_indices[i] * components + c];
                    else
                        bytes[c] = floatdata[mesh.triangle_indices[i] * components + c];
                }
                bytes += stride;
            }
        } else {
            // handle error
        }
        free(floatdata);
    }    
    
    dmBuffer::ValidateBuffer(mesh.buffer);
    dmScript::LuaHBuffer luabuf(mesh.buffer, dmScript::OWNER_C);
    dmScript::PushBuffer(L, luabuf);
    return 1;
}

// --------------------------------------------------------------------------------------------------------

static int LoadMeshes( lua_State *L)
{
    DM_LUA_STACK_CHECK(L, 1);

    int idx = luaL_checknumber(L,1);
    if( idx < 0 || idx >= g_anims.size()) {
        printf("[LoadOzz Error] Invalid anim index: %d\n", idx);
        lua_pushnil(L);
        return 1;    
    }

    const char *mesh_filename = (char *)luaL_checkstring(L, 2);
    if(mesh_filename == nullptr)
    {
        printf("[LoadOzz Error] Invalid mesh filename\n");
        lua_pushnil(L);
        return 1;    
    }

    // dmGameObject::HInstance collection = dmScript::CheckCollection(L, 3);
    // const char *go_proto = (char *)luaL_checkstring(L, 4);
    
    animObj *anim = g_anims[idx];
    anim->mesh_filename = mesh_filename;

    // Reading skinned meshes.
    if (!LoadMeshes(anim->mesh_filename.c_str(), &anim->meshes)) {
        printf("[LoadOzz Error] Cannot load mesh: %s\n", mesh_filename);
        lua_pushnil(L);
        return 1;    
    }

    // Computes the number of skinning matrices required to skin all meshes.
    // A mesh is skinned by only a subset of joints, so the number of skinning
    // matrices might be less that the number of skeleton joints.
    // Mesh::joint_remaps is used to know how to order skinning matrices. So
    // the number of matrices required is the size of joint_remaps.
    size_t num_skinning_matrices = 0;
    for (const game::Mesh& mesh : anim->meshes) {
      num_skinning_matrices =
          ozz::math::Max(num_skinning_matrices, mesh.joint_remaps.size());
    }

    // Allocates skinning matrices.
    anim->skinning_matrices.resize(num_skinning_matrices);

    // Check the skeleton matches with the mesh, especially that the mesh
    // doesn't expect more joints than the skeleton has.
    for (const game::Mesh& mesh : anim->meshes) {
      if (anim->num_joints < mesh.highest_joint_index()) {
        printf("[LoadOzz Error] The provided mesh doesn't match skeleton (joint count mismatch).\n");
        lua_pushnil(L);
        return 1; 
      }
    }

    lua_newtable(L);
    int parent = lua_gettop(L);
    int i = 1;
    for (const game::Mesh& mesh : anim->meshes) 
    {
        // printf("----------------------------------------\n");
        // printf("Vertices: %d\n", mesh.vertex_count());
        // printf("Indices: %d\n", mesh.triangle_index_count());
        // printf("Max Influences: %d\n", mesh.max_influences_count());
        // printf("Skinned: %d\n", (mesh.skinned() == true)?1:0);
        // printf("Number of Joints: %d\n", mesh.num_joints());      
        // printf("----------------------------------------\n");

        lua_pushnumber(L, i++); 
        lua_newtable(L);

        lua_pushstring(L, "vertex_count");   
        lua_pushnumber(L, mesh.vertex_count()); 
        lua_rawset(L, -3);      
        lua_pushstring(L, "triangle_index_count");   
        lua_pushnumber(L, mesh.triangle_index_count()); 
        lua_rawset(L, -3);  
        lua_pushstring(L, "joint_count");   
        lua_pushnumber(L, mesh.num_joints()); 
        lua_rawset(L, -3);      

        lua_rawset(L, -3);   
    }

    return 1;
}

// --------------------------------------------------------------------------------------------------------

int DrawSkinnedMeshInternal( animObj * anim )
{
    const ozz::math::Float4x4 transform = ozz::math::Float4x4::identity();
    
    // Builds skinning matrices, based on the output of the animation stage.
    // The mesh might not use (aka be skinned by) all skeleton joints. We
    // use the joint remapping table (available from the mesh object) to
    // reorder model-space matrices and build skinning ones.
    for (const game::Mesh& mesh : anim->meshes) {
        for (size_t i = 0; i < mesh.joint_remaps.size(); ++i) {
            anim->skinning_matrices[i] = anim->models[mesh.joint_remaps[i]] * mesh.inverse_bind_poses[i];
        }

        // Renders skin.
        bool ok = DrawDefoldSkinnedMesh(mesh, make_span(anim->skinning_matrices), transform);
        if(ok == false) printf("Bad Draw juju\n");
    }

    return 0;
}    

// --------------------------------------------------------------------------------------------------------

static int DrawSkinnedMesh(lua_State *L)
{
    int idx = luaL_checknumber(L,1);
    if( idx < 0 || idx >= g_anims.size()) {
        printf("[LoadOzz Error] Invalid anim index: %d\n", idx);
        lua_pushnil(L);
        return 1;    
    }

    animObj *anim = g_anims[idx];
    int ok = DrawSkinnedMeshInternal(anim);
    lua_pushnumber(L, ok);
    return 1;
}

// --------------------------------------------------------------------------------------------------------

static int UpdateAnimation(lua_State *L)
{
    double dt = (double)(( dmTime::GetTime() - g_last_time ) / 1000000.0 );
    g_last_time = dmTime::GetTime();
 
    for(size_t i=0; i<g_anims.size(); ++i)
    {
        // printf("Test dt: %g  %d\n", dt, (int)i);
        animObj *anim = g_anims[i];
        // Updates current animation time.
        anim->controller.Update(anim->animations, dt);

        // Samples optimized animation at t = animation_time_.
        ozz::animation::SamplingJob sampling_job;
        sampling_job.animation = &anim->animations;
        sampling_job.context = &anim->context;
        sampling_job.ratio = anim->controller.time_ratio();
        sampling_job.output = make_span(anim->locals);
        if (!sampling_job.Run()) {
            dmExtension::RESULT_INIT_ERROR  ;
        }

        // Converts from local space to model space matrices.
        ozz::animation::LocalToModelJob ltm_job;
        ltm_job.skeleton = &anim->skeleton;
        ltm_job.input = make_span(anim->locals);
        ltm_job.output = make_span(anim->models);
        if (!ltm_job.Run()) {
            dmExtension::RESULT_INIT_ERROR  ;
        }
    }
    return 0;
}

static int SetAnimationTime(lua_State *L) 
{
    int idx = luaL_checknumber(L,1);
    if( idx < 0 || idx >= g_anims.size()) {
        printf("[LoadOzz Error] Invalid anim index: %d\n", idx);
        lua_pushnil(L);
        return 1;    
    }

    float ratio = luaL_checknumber(L, 2);

    animObj *anim = g_anims[idx];
    anim->controller.set_time_ratio(ratio);

    lua_pushnumber(L, ratio);
    return 1;
}

// --------------------------------------------------------------------------------------------------------

static int GetMeshBounds(lua_State *L)
{
    int idx = luaL_checknumber(L,1);
    if( idx < 0 || idx >= g_anims.size()) {
        printf("[LoadOzz Error] Invalid anim index: %d\n", idx);
        lua_pushnil(L);
        return 1;    
    }

    animObj *anim = g_anims[idx];

    // Set a default box.
    ozz::math::Box _bound =  ozz::math::Box();   
    if (anim->models.empty()) {
        printf("[LoadOzz Error] GetMeshBounds: No models in anim!\n");
        lua_pushnil(L);
        return 1;
    }

    // Loops through matrices and stores min/max.
    // Matrices array cannot be empty, it was checked at the beginning of the
    // function.
    auto current = anim->models.begin();
    ozz::math::SimdFloat4 min = current->cols[3];
    ozz::math::SimdFloat4 max = current->cols[3];
    ++current;
    while (current < anim->models.end()) {
        min = ozz::math::Min(min, current->cols[3]);
        max = ozz::math::Max(max, current->cols[3]);
        ++current;
    }

    // Stores in math::Box structure.
    ozz::math::Store3PtrU(min, &_bound.min.x);
    ozz::math::Store3PtrU(max, &_bound.max.x);

    lua_pushnumber(L, _bound.min.x);
    lua_pushnumber(L, _bound.min.y);
    lua_pushnumber(L, _bound.min.z);
    lua_pushnumber(L, _bound.max.x);
    lua_pushnumber(L, _bound.max.y);
    lua_pushnumber(L, _bound.max.z);
    return 6;
}

// --------------------------------------------------------------------------------------------------------

static int GetSkinnedBounds(lua_State *L)
{
    int idx = luaL_checknumber(L,1);
    if( idx < 0 || idx >= g_anims.size()) {
        printf("[LoadOzz Error] Invalid anim index: %d\n", idx);
        lua_pushnil(L);
        return 1;    
    }

    animObj *anim = g_anims[idx];

    // Set a default box.
    ozz::math::Box _bound =  ozz::math::Box();   
    if (anim->skinning_matrices.empty()) {
        printf("[LoadOzz Error] GetSkinnedBounds: No skinned meshes in anim!\n");
        lua_pushnil(L);
        return 1;
    }

    // Loops through matrices and stores min/max.
    // Matrices array cannot be empty, it was checked at the beginning of the
    // function.
    auto current = anim->skinning_matrices.begin();
    ozz::math::SimdFloat4 min = current->cols[3];
    ozz::math::SimdFloat4 max = current->cols[3];
    ++current;
    while (current < anim->skinning_matrices.end()) {
        min = ozz::math::Min(min, current->cols[3]);
        max = ozz::math::Max(max, current->cols[3]);
        ++current;
    }

    // Stores in math::Box structure.
    ozz::math::Store3PtrU(min, &_bound.min.x);
    ozz::math::Store3PtrU(max, &_bound.max.x);

    lua_pushnumber(L, _bound.min.x);
    lua_pushnumber(L, _bound.min.y);
    lua_pushnumber(L, _bound.min.z);
    lua_pushnumber(L, _bound.max.x);
    lua_pushnumber(L, _bound.max.y);
    lua_pushnumber(L, _bound.max.z);
    return 6;
}

// --------------------------------------------------------------------------------------------------------
// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"loadozz", LoadOzz},
    {"loadmesh", LoadMeshes},
    {"getmeshbounds", GetMeshBounds},
    {"getskinnedbounds", GetSkinnedBounds},
    {"createbuffers", SetBufferFromMesh},
    {"updateanimation", UpdateAnimation},
    {"drawskinnedmesh", DrawSkinnedMesh},
    {"setanimationtime", SetAnimationTime},
    {0, 0}
};

// --------------------------------------------------------------------------------------------------------

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);

    // Register lua names
    luaL_register(L, MODULE_NAME, Module_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

// --------------------------------------------------------------------------------------------------------

static dmExtension::Result AppInitializeozzanim(dmExtension::AppParams* params)
{
    dmLogInfo("AppInitializeozz");
    return dmExtension::RESULT_OK;
}

// --------------------------------------------------------------------------------------------------------

static dmExtension::Result Initializeozzanim(dmExtension::Params* params)
{
    // Init Lua
    LuaInit(params->m_L);
    g_last_time = dmTime::GetTime();
    dmLogInfo("Registered %s Extension", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

// --------------------------------------------------------------------------------------------------------

static dmExtension::Result AppFinalizeozzanim(dmExtension::AppParams* params)
{
    dmLogInfo("AppFinalizeozz");
    for(int i=0; i<g_anims.size(); i++)
    {
        delete g_anims[i];
    }
    return dmExtension::RESULT_OK;
}

// --------------------------------------------------------------------------------------------------------

static dmExtension::Result Finalizeozzanim(dmExtension::Params* params)
{
    dmLogInfo("Finalizeozz");
    return dmExtension::RESULT_OK;
}

// --------------------------------------------------------------------------------------------------------

static dmExtension::Result OnUpdateozzanim(dmExtension::Params* params)
{

    return dmExtension::RESULT_OK;
}

// --------------------------------------------------------------------------------------------------------

static void OnEventozzanim(dmExtension::Params* params, const dmExtension::Event* event)
{
    switch(event->m_Event)
    {
        case dmExtension::EVENT_ID_ACTIVATEAPP:
        dmLogInfo("OnEventozzanim - EVENT_ID_ACTIVATEAPP");
            break;
        case dmExtension::EVENT_ID_DEACTIVATEAPP:
        dmLogInfo("OnEventozzanim - EVENT_ID_DEACTIVATEAPP");
            break;
        case dmExtension::EVENT_ID_ICONIFYAPP:
        dmLogInfo("OnEventozzanim - EVENT_ID_ICONIFYAPP");
            break;
        case dmExtension::EVENT_ID_DEICONIFYAPP:
        dmLogInfo("OnEventozzanim - EVENT_ID_DEICONIFYAPP");
            break;
        default:
        dmLogWarning("OnEventozzanim - Unknown event id");
            break;
    }
}

// Defold SDK uses a macro for setting up extension entry points:
//
// DM_DECLARE_EXTENSION(symbol, name, app_init, app_final, init, update, on_event, final)

// ozz is the C++ symbol that holds all relevant extension data.
// It must match the name field in the `ext.manifest`
DM_DECLARE_EXTENSION(ozzanim, LIB_NAME, AppInitializeozzanim, AppFinalizeozzanim, Initializeozzanim, OnUpdateozzanim, OnEventozzanim, Finalizeozzanim)
