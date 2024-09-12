// ozz.cpp
// Extension lib defines
#define LIB_NAME "ozzanim"
#define MODULE_NAME "ozzanim"

// include the Defold SDK
#include <dmsdk/sdk.h>
#include <string>
#include <vector>

#include "mesh/mesh.h"

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/log.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/containers/vector_archive.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/math_ex.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/vec_float.h"
#include "ozz/options/options.h"

// TODO: 
//    Make this more of a reference container so meshes, animations and skeletons
//    are only loaded once rather than for each instance.

typedef struct animObj
{
    std::string     skeleton_filename;
    std::string     animation_filename;
    std::string     mesh_filename;

    int             num_joints;

    ozz::vector<game::Mesh>                 meshes;
    
    ozz::animation::Skeleton                skeleton;
    ozz::animation::Animation               animations;
    ozz::animation::SamplingJob::Context    context;
    ozz::vector<ozz::math::SoaTransform>    locals;
    ozz::vector<ozz::math::Float4x4>        models;    
    ozz::vector<ozz::math::Float4x4>        skinning_matrices;
} _animObj;
    
static std::vector<animObj *> g_anims;

extern bool LoadSkeleton(const char* _filename, ozz::animation::Skeleton* _skeleton);
extern bool LoadAnimation(const char* _filename, ozz::animation::Animation* _animation);
extern bool LoadMeshes(const char* _filename, ozz::vector<game::Mesh>* _meshes);

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

    int idx = g_anims.size();
    g_anims.push_back(anim);

    lua_pushnumber(L, idx);
    return 1;
}

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

    lua_pushnumber(L, idx);
    return 1;
}

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"loadozz", LoadOzz},
    {"loadmesh", LoadMeshes},
    {0, 0}
};

static void LuaInit(lua_State* L)
{
    int top = lua_gettop(L);

    // Register lua names
    luaL_register(L, MODULE_NAME, Module_methods);

    lua_pop(L, 1);
    assert(top == lua_gettop(L));
}

static dmExtension::Result AppInitializeozzanim(dmExtension::AppParams* params)
{
    dmLogInfo("AppInitializeozz");
    return dmExtension::RESULT_OK;
}

static dmExtension::Result Initializeozzanim(dmExtension::Params* params)
{
    // Init Lua
    LuaInit(params->m_L);
    dmLogInfo("Registered %s Extension", MODULE_NAME);
    return dmExtension::RESULT_OK;
}

static dmExtension::Result AppFinalizeozzanim(dmExtension::AppParams* params)
{
    dmLogInfo("AppFinalizeozz");
    for(int i=0; i<g_anims.size(); i++)
    {
        delete g_anims[i];
    }
    return dmExtension::RESULT_OK;
}

static dmExtension::Result Finalizeozzanim(dmExtension::Params* params)
{
    dmLogInfo("Finalizeozz");
    return dmExtension::RESULT_OK;
}

static dmExtension::Result OnUpdateozzanim(dmExtension::Params* params)
{
    for(size_t i=0; i<g_anims.size(); ++i)
    {
        // Updates current animation time.
        controller_.Update(animation_, _dt);

        // Samples optimized animation at t = animation_time_.
        ozz::animation::SamplingJob sampling_job;
        sampling_job.animation = &animation_;
        sampling_job.context = &context_;
        sampling_job.ratio = controller_.time_ratio();
        sampling_job.output = make_span(locals_);
        if (!sampling_job.Run()) {
        return false;
        }

        // Converts from local space to model space matrices.
        ozz::animation::LocalToModelJob ltm_job;
        ltm_job.skeleton = &skeleton_;
        ltm_job.input = make_span(locals_);
        ltm_job.output = make_span(models_);
        if (!ltm_job.Run()) {
        return false;
        }
    }
    return dmExtension::RESULT_OK;
}

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
