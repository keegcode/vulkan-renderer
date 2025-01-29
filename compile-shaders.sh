for file in ./shaders/*.vert.glsl; do
    glslc -fshader-stage=vertex "${file}" -o "${file}.spv"
done

for file in ./shaders/*.frag.glsl; do
    glslc -fshader-stage=fragment "${file}" -o "${file}.spv"
done
