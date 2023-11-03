
SCRIPT_DIR=$(dirname $0)
echo $SCRIPT_DIR

compile_shader_extension() {
	for f in $SCRIPT_DIR/*.$1
	do
		echo compiling $f to $f.spv
		rm $f.spv
		glslc $f -o $f.spv
	done
}

compile_shader_extension vert
compile_shader_extension frag
