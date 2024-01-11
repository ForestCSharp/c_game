
call data/shaders/compile_shaders.bat

set BIN_DIR=bin

del /f/s/q %BIN_DIR% > nul
rmdir /s/q %BIN_DIR%
mkdir %BIN_DIR%

call clang -g -gcodeview src/main.c -o %BIN_DIR%/game.exe -l user32.lib %VULKAN_SDK%/Lib/vulkan-1.lib -I %VULKAN_SDK%/Include -I src

call "%BIN_DIR%/game.exe"
