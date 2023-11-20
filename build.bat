
call data/shaders/compile_shaders.bat

for %%i in (game.*) do (
    DEL /f /q %%i
)

call clang -g -gcodeview src/main.c -o game.exe -l user32.lib %VULKAN_SDK%/Lib/vulkan-1.lib -I %VULKAN_SDK%/Include -I src

call game.exe
