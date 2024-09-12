# Defold Ozz-animation extension

** This is under construction - bugs/perf/issues beware! **

## Why? Defold has animation! 

Yes Defold does. But again, I have a few 'outside the box' features I need. These are mainly (no specific order):

- Runtime loading and mod (for my Flix app that is on pause atm)
- IK runtime modification - for terrain foot placement, and for hand/arm placement
- Blend trees, events and parameterized bone manipulation (relates kinda to IK but more to state control)
- Animation editor (want a standalone way to make/edit/control anims and blend trees)

Much of this is a large amount to build myself (would take months/years). Thus I intend to leverage the excellent ozz-animation library available here:

https://guillaumeblanc.github.io/ozz-animation/

The aim will be to provide some (not necessarily all) of the features ozz-animation has built in and couple it to Defold mesh rendering and possibly use Defold material shaders for skinning. 

An extended bonus would be to have GPU accelerated bone anim/blend via texture data into uv streams similar to:

https://github.com/piti6/UnityGpuInstancedAnimation

This will be something I will tackle once I have the basics working. 

Additionally, the standalone Animation editor I want to build with Defold. The project will be a derivative of this extension project since it will use it :)

## Status

12-09-2024: The current status is that I have a basic skeleton, animation set and mesh set loadable from script.



## Todo

- [ ] Load in skeletons, anims and meshes
- [ ] Create defold mesh from ozz mesh
- [ ] Editor tool to convert gltf to ozz skeleton, anim and mesh
- [ ] Editor tool to convert fbx to ozz skeleton, anim and mesh
- [ ] Runtime loadable ozz data
- [ ] Map materials to the defold mesh
- [ ] Generate mesh buffers from ozz mesh (maybe in editor?)
- [ ] Prototype Anim Editor