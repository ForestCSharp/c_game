// filename: bindless.glsl
// Add non-uniform qualifier extension here;
// so, we do not add it for all our shaders
// that use bindless resources
#extension GL_EXT_nonuniform_qualifier : enable

// We always bind the bindless descriptor set
// to set = 0
#define BindlessDescriptorSet 0
// These are the bindings that we defined
// in bindless descriptor layout
#define BindlessStorageBinding 0

#define GetLayoutVariableName(Name) u##Name##Register

// Register storage buffer
#define RegisterBuffer(Layout, BufferAccess, Name, Struct) \
  layout(Layout, set = BindlessDescriptorSet, binding = BindlessStorageBinding) \
    BufferAccess buffer Name Struct GetLayoutVariableName(Name)[]

// Access a specific resource
#define GetResource(Name, Index) \
  GetLayoutVariableName(Name)[Index]

// Register empty resources
// to be compliant with the pipeline layout
// even if the shader does not use all the descriptors
RegisterBuffer(std430, readonly, DummyBuffer, { uint ignore; });
