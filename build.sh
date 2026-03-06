

rm -r ./bin/
mkdir ./bin/

./compile_shaders.sh data/shaders

# Metal Debugging Options 
export MTL_DEBUG_LAYER=1

# Molten VK Options
export MVK_CONFIG_USE_METAL_ARGUMENT_BUFFERS=1
export MVK_CONFIG_LOG_LEVEL=3
export MVK_CONFIG_LOG_SHADER_CONVERSION=1
export MVK_CONFIG_LOG_METAL_SHADER_COMPILE=1
export MVK_DEBUG=1

mac_backend="vk_molten"

while [[ $# -gt 0 ]]; do
    case $1 in
        -mb|--mac_backend)
            mac_backend="$2"
            shift 2
            ;;
        *)
            echo "Unknown option: $1"
            echo "Usage: $0 [-mb|--mac_backend <metal|vk_molten|vk_kosmic>]"
            exit 1
            ;;
    esac
done

case $mac_backend in
    metal|mtl)
		mac_backend="metal"
        render_backend_define=GPU_IMPLEMENTATION_METAL
        ;;
    vk_molten)
        render_backend_define=GPU_IMPLEMENTATION_VULKAN
		mac_vulkan_backend_define=MAC_VULKAN_BACKEND_MOLTEN
        vulkan_lib="libvulkan.dylib"
        portability_define="-DVK_NEEDS_PORTABILITY_EXTENSIONS"
        ;;
    vk_kosmic|vulkan|vk)
        render_backend_define=GPU_IMPLEMENTATION_VULKAN
		mac_vulkan_backend_define=MAC_VULKAN_BACKEND_KOSMIC
        vulkan_lib="libvulkan.dylib"
        ;;
    *)
        echo "Invalid mac_backend: $mac_backend"
        echo "Valid options: metal, vk_molten, vk_kosmic"
        exit 1
        ;;
esac

if [ "$mac_backend" = "vk_kosmic" ]; then
    export VK_DRIVER_FILES=/usr/local/share/vulkan/icd.d/libkosmickrisp_icd.json
    export VK_ICD_FILENAMES=/usr/local/share/vulkan/icd.d/libkosmickrisp_icd.json
	#export MESA_DEBUG=1
	#export MESA_KK_DEBUG=msl
	#export MESA_LOG_FILE=bin/mesa_log.txt
fi

#export VK_LOADER_DEBUG=all

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

	#Build and Run for Mac
	if [ -n "$vulkan_lib" ]; then
	    cp /usr/local/lib/$vulkan_lib ./bin/
	    vulkan_link="./bin/$vulkan_lib"
	fi
	
	if [ "$mac_backend" = "metal" ]; then
		clang -ObjC -g ./src/main.c \
			-o bin/game \
			-I /usr/local/include/vulkan \
			-I ./src/ \
			-framework Cocoa \
			-framework Metal \
			-framework MetalKit \
			-framework Quartz \
			-rpath /usr/local/lib \
			-D $render_backend_define
	else
		clang -ObjC -g ./src/main.c \
			$vulkan_link \
			-o bin/game \
			-I /usr/local/include/vulkan \
			-I ./src/ \
			-framework Cocoa \
			-framework Metal \
			-framework MetalKit \
			-framework Quartz \
			-rpath /usr/local/lib \
			-D $render_backend_define \
			-D $mac_vulkan_backend_define \
			$portability_define
	fi

	./bin/game

elif [ $machine = MinGW ]; then

	#Build and Run for Windows
	clang -g -gcodeview src/main.c \
		-o ./bin/game.exe \
		-l user32.lib ${VULKAN_SDK}/Lib/vulkan-1.lib \
		-I ${VULKAN_SDK}/Include \
		-I src \
		-D $render_backend_define

	./bin/game.exe
fi

