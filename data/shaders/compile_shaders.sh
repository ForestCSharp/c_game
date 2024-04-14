
SCRIPT_DIR=$(dirname $0)
echo $SCRIPT_DIR
echo $SCRIPT_DIR/include

compile_shader_extension() {
	find $SCRIPT_DIR -name '*.'$1 | while read f; do
		echo compiling $f to $f.spv
		rm -f $f.spv
		glslc $f -o $f.spv 
	done
}

compile_shader_extension vert
compile_shader_extension frag
