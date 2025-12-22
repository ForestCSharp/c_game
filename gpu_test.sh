rm -r ./bin/
mkdir ./bin/

./compile_shaders.sh data/shaders

export MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=1
export MVK_CONFIG_LOG_LEVEL=3
export MVK_DEBUG=1

if [ "$1" = "metal" ] || [ "$1" = "mtl" ]; then 
	render_backend_define=GPU_IMPLEMENTATION_METAL
elif [ "$1" = "vulkan" ] || [ "$1" = "vk" ]; then
	render_backend_define=GPU_IMPLEMENTATION_VULKAN
else
	render_backend_define=GPU_IMPLEMENTATION_VULKAN
	echo "No render backend specified, defaulting to GPU_IMPLEMENTATION_VULKAN"
fi

unameOut="$(uname -s)"
case "${unameOut}" in
    Linux*)     machine=Linux;;
    Darwin*)    machine=Mac;;
    CYGWIN*)    machine=Cygwin;;
    MINGW*)     machine=MinGW;;
    MSYS_NT*)   machine=Git;;
    *)          machine="UNKNOWN:${unameOut}"
esac
echo ${machine}

if [ $machine = Mac ]; then

	# -fno-objc-arc \

	#Build and Run for Mac
	cp /usr/local/lib/libvulkan.dylib ./bin/
	clang -ObjC -g ./src/gpu/gpu_test.c ./bin/libvulkan.dylib \
		-o bin/gpu_test \
		-I /usr/local/include/vulkan \
		-I ./src/ \
		-framework Cocoa \
		-framework Metal \
		-framework MetalKit \
		-framework Quartz \
		-rpath /usr/local/lib \
		-D $render_backend_define

	./bin/gpu_test

elif [ $machine = MinGW ]; then

	#Build and Run for Windows
	clang -g -gcodeview src/gpu/gpu_test.c \
		-o ./bin/gpu_test.exe \
		-l user32.lib ${VULKAN_SDK}/Lib/vulkan-1.lib \
		-I ${VULKAN_SDK}/Include \
		-I src \
		-D $render_backend_define

	./bin/gpu_test.exe
fi

