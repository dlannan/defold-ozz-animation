// ozz.cpp
// Extension lib defines
#define LIB_NAME "ozzanim"
#define MODULE_NAME "ozzanim"

// include the Defold SDK
#include <dmsdk/sdk.h>
#include <string>
#include <vector>

#include "ozz/animation/runtime/animation.h"
#include "ozz/animation/runtime/local_to_model_job.h"
#include "ozz/animation/runtime/sampling_job.h"
#include "ozz/animation/runtime/skeleton.h"
#include "ozz/base/io/archive.h"
#include "ozz/base/io/stream.h"
#include "ozz/base/log.h"
#include "ozz/base/containers/vector.h"
#include "ozz/base/maths/simd_math.h"
#include "ozz/base/maths/soa_transform.h"
#include "ozz/base/maths/vec_float.h"
#include "ozz/options/options.h"

typedef struct animObj
{
    std::string     skeleton_filename;
    std::string     animation_filename;

    ozz::animation::Skeleton skeleton;
    ozz::animation::Animation animations;
    ozz::animation::SamplingJob::Context context;
    ozz::vector<ozz::math::SoaTransform> locals;
    ozz::vector<ozz::math::Float4x4> models;    
} _animObj;
    
static std::vector<animObj *> g_anims;

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
    const int num_joints = anim->skeleton.num_joints();
    anim->models.resize(num_joints);

    // Allocates a context that matches animation requirements.
    anim->context.Resize(num_joints);

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

// Functions exposed to Lua
static const luaL_reg Module_methods[] =
{
    {"loadozz", LoadOzz},
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
    dmLogInfo("OnUpdateozz");
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
