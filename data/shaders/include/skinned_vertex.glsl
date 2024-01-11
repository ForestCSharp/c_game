
struct SkinnedVertex
{
	vec4 position;
	vec4 normal;
	vec4 color;
	vec2 uv;
	uint padding[2];
    vec4 joint_indices;
    vec4 joint_weights;
};

RegisterBuffer(std430, readonly, IndexBuffer, {
	uint indices[];
});

RegisterBuffer(std430, readonly, VertexBuffer, {
	SkinnedVertex vertices[];
});


// Loads vertex data using gl_VertexIndex and bindless buffers
// Must have included "uniforms.glsl" before this file (for object_ubo)
SkinnedVertex load_skinned_vertex()
{
	int indices_idx = gl_VertexIndex;
	uint vertices_idx = GetResource(IndexBuffer, object_ubo.index_buffer_idx).indices[indices_idx];
	SkinnedVertex vertex = GetResource(VertexBuffer, object_ubo.vertex_buffer_idx).vertices[vertices_idx];
	return vertex;
}

