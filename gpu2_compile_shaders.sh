#!/bin/bash
if [ $# -eq 0 ]; then
  echo "Please specify a shader directory"
  exit 1
fi

SHADER_DIR=$1
mkdir -p ./bin/shaders

echo "Compiling shaders in: $SHADER_DIR"
echo "Include path: $SHADER_DIR/include"

compile_shader_extension() {
  find "$SHADER_DIR" -name '*.'"$1" | while read f; do
    echo "Compiling $f to ./bin/shaders/"
    filename=$(basename "$f")
    spv_filename="./bin/shaders/$filename.spv"
    msl_filename="./bin/shaders/$filename.msl"
    
    glslc "$f" -o "$spv_filename"

	# Notes:
	# --msl-force-active-argument-buffer-resources is used to make sure our argbuffers match on shader stages, even if some resources are unused for that stage

    spirv-cross "$spv_filename" \
    	--msl \
    	--msl-version 300000 \
    	--msl-argument-buffers \
    	--msl-decoration-binding \
		--msl-argument-buffer-tier 1 \
		--msl-force-active-argument-buffer-resources \
    	--output "$msl_filename"

  done
}

compile_shader_extension vert
compile_shader_extension frag
