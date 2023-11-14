@echo off

pushd %~p0\vertex
call :compile_shader_extension "vert"
call :compile_shader_extension "frag"
popd

pushd %~p0\fragment
call :compile_shader_extension "vert"
call :compile_shader_extension "frag"
popd

EXIT /B 0

:compile_shader_extension
    for %%i in (*.%1) do (
        echo compile %%i
        DEL /f /q %%i.spv
        call glslc %%i -o %%i.spv
    )
EXIT /B 0