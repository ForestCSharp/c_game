echo off
pushd %~dp0

for /r %%i in (*frag *.vert) do (
    echo compiling %%i to %%i.spv
    glslc %%i -o %%i.spv
)
popd