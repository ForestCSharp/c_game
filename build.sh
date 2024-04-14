
rm -r ./bin/
mkdir ./bin/

#TODO: Remove once GPU2 is complete
./data/shaders/compile_shaders.sh

./gpu2_compile_shaders.sh data/shaders

platform_name="$(uname -s)"
echo $platform_name

export MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=1
#export MVK_CONFIG_LOG_LEVEL=3
#export MVK_DEBUG=1


if [ "$1" = "metal" ]; then 
	render_backend_define=GPU2_IMPLEMENTATION_METAL
elif [ "$1" = "vulkan" ]; then
	render_backend_define=GPU2_IMPLEMENTATION_VULKAN
else
	render_backend_define=GPU2_IMPLEMENTATION_VULKAN
	echo "No render backend specified, defaulting to GPU2_IMPLEMENTATION_VULKAN"
fi

cp /usr/local/lib/libvulkan.dylib ./bin/
clang -ObjC -g ./src/main.c ./bin/libvulkan.dylib \
	-o bin/game \
	-I /usr/local/include/vulkan \
	-I ./src/ \
	-framework Cocoa \
	-framework Metal \
	-framework MetalKit \
	-framework Quartz \
	-rpath /usr/local/lib \
	-D $render_backend_define


./bin/game
