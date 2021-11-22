Function compile_shader_extension($in_extenstion) {
    Get-ChildItem -Path .\ -Filter *.$in_extenstion -Recurse -File -Name| ForEach-Object {
        glslc "$_" -o "$_.spv"
    }
} 

compile_shader_extension("vert")
compile_shader_extension("frag")