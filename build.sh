#TODO: Remove .bat files once this is working on windows

./data/shaders/compile_shaders.sh

# TODO: Store binaries in bin
rm -f bin/game.*

platform_name="$(uname -s)"

echo $platform_name

case $platform_name in
	Darwin*)  ;;
esac

export MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=1
export MVK_CONFIG_LOG_LEVEL=3
export MVK_DEBUG=1

rm -r ./bin/
mkdir ./bin/
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


./bin/game
