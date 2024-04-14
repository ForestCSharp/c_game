#!/bin/bash
if [ $# -eq 0 ]; then
 echo "Please specify a shader directory"
 exit 1
fi

mkdir -p ./bin/shaders

SCRIPT_DIR=$(dirname $1)
echo $SCRIPT_DIR
echo $SCRIPT_DIR/include

compile_shader_extension() {
	find $SCRIPT_DIR -name '*.'$1 | while read f; do
		echo compiling $f to ./bin/$f.spv
		filename=$(basename "$f")
		spv_filename="./bin/shaders/$filename.spv"
		msl_filename="./bin/shaders/$filename.msl"
		glslc $f -o $spv_filename
		spirv-cross $spv_filename --msl --output $msl_filename --msl-argument-buffers --msl-version 200000
	done
}

compile_shader_extension vert
compile_shader_extension frag
