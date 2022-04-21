./data/shaders/compile_shaders.ps1

if (Test-Path -Path "game.*") 
{ 
    Remove-Item "game.*" -ErrorAction Ignore 
}

clang -g -gcodeview src/main.c -o game.exe -l user32.lib $Env:VULKAN_SDK/Lib/vulkan-1.lib -I  $Env:VULKAN_SDK/Include

./game.exe