@echo off
for %%f in (.\shaders\*.vert.glsl) do (
    glslc -fshader-stage=vertex "%%f" -o "%%f.spv" -g
)
for %%f in (.\shaders\*.frag.glsl) do (
    glslc -fshader-stage=fragment "%%f" -o "%%f.spv" -g
)
