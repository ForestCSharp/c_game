#pragma once

#define _CRT_SECURE_NO_WARNINGS
#include "math/basic_math.h"
#include "math/matrix.h"
#include "math/quat.h"
#include "math/vec.h"
#include "types.h"
#include <assert.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

const u32 GLTF_MAGIC_NUMBER = 0x46546C67;
const u32 GLTF_CHUNK_TYPE_JSON = 0x4E4F534A;
const u32 GLTF_CHUNK_TYPE_BUFFER = 0x004E4942;

const char* STRING_TRUE = "true";
const char* STRING_FALSE = "false";

typedef struct JsonObject
{
    i32 count;
    struct JsonKeyValuePair* key_value_pairs;
} JsonObject;

typedef struct JsonArray
{
    i32 count;
    struct JsonValue* values;
} JsonArray;

typedef enum JsonValueType
{
    JSON_VALUE_TYPE_STRING,
    JSON_VALUE_TYPE_NUMBER,
    JSON_VALUE_TYPE_OBJECT,
    JSON_VALUE_TYPE_BOOLEAN,
    JSON_VALUE_TYPE_ARRAY,
} JsonValueType;

typedef struct JsonValue
{
    JsonValueType type;
    union
    {
        char* string;
        float number;
        JsonObject object;
        bool boolean;
        JsonArray array;
    } data;
} JsonValue;

typedef struct JsonKeyValuePair
{
    char* key;
    JsonValue value;
} JsonKeyValuePair;

// returns true if char c exists in string str
bool is_char_in_string(char c, const char* str)
{
    for (i32 i = 0; i < strlen(str); ++i)
    {
        if (c == str[i])
        {
            return true;
        }
    }
    return false;
}

// can modify p_string, trims p_string of leading chars_to_trim
i32 trim_characters(char** p_string, const char* chars_to_trim)
{
	i32 characters_trimmed = 0;
	while(is_char_in_string(*p_string[0], chars_to_trim))
	{
		*p_string = *p_string + 1;
		++characters_trimmed;
	}
	return characters_trimmed;
}

// can modify p_string, trims whitespace characters
i32 trim_whitespace(char** p_string)
{
    return trim_characters(p_string, " \t\v\n\r\f");
}

// can modify p_string, trims whitespace and attempts to consume char c
bool consume(char c, char** p_string)
{
    int chars_trimmed = trim_whitespace(p_string);
    if (c == *p_string[0])
    {
        *p_string = *p_string + 1;
        return true;
    }
    return false;
}

// Finds next occurrence of c in in_string
i32 find_next(char c, char* in_string)
{
    for (i32 i = 0; i < strlen(in_string); ++i)
    {
        if (c == in_string[i])
        {
            return i;
        }
    }
    return -1;
}

// returns true if the next n-chars of in_string match identifier (n ==
// strlen(identifier))
bool string_check(const char* in_string, const char* identifier)
{
    for (i32 i = 0; i < strlen(identifier); ++i)
    {
        if (i >= strlen(in_string) || in_string[i] != identifier[i])
        {
            return false;
        }
    }
    return true;
}

// can modify p_string, allocates and returns a new string
char* parse_string(char** p_string)
{
    char** old_string = p_string;
    bool found_leading_quote = consume('\"', p_string);
    if (found_leading_quote)
    {
        int key_length = find_next('\"', *p_string);
        // FIXME: check for escape char before ending quote
        if (key_length >= 0)
        {
            char* parsed_string = (char*) calloc(key_length + 1, 1);
            memcpy(parsed_string, *p_string, key_length);
            *p_string = *p_string + key_length + 1;
            return parsed_string;
        }
    }
    return NULL;
}

// Allocations: Parsed Strings, Arrays, JsonObject KeyValue pairs

bool parse_json_object(char** json_string, JsonObject* out_json_object);

bool parse_json_value(char** json_string, JsonValue* out_value)
{
    char* current_position = *json_string;

    trim_whitespace(&current_position);
    if (current_position[0] == '{')
    {
        out_value->type = JSON_VALUE_TYPE_OBJECT;
        if (!parse_json_object(&current_position, &out_value->data.object))
		{
			return false;
		}
    }
    else if (current_position[0] == '"')
    {
        out_value->type = JSON_VALUE_TYPE_STRING;
        out_value->data.string = parse_string(&current_position);
    }
    else if (isdigit(current_position[0]) || current_position[0] == '-')
    {
        char* end_ptr = NULL;
        out_value->type = JSON_VALUE_TYPE_NUMBER;
        out_value->data.number = strtof(current_position, &end_ptr);
        if (current_position != end_ptr)
        {
            current_position = end_ptr;
        }
    }
    else if (current_position[0] == '[')
    {
        out_value->type = JSON_VALUE_TYPE_ARRAY;
        out_value->data.array.count = 0;
        out_value->data.array.values = NULL;

		// consume opening brace 
        if (!consume('[', &current_position))
		{
			return false;
		}

		// check for empty array
		if (consume(']', &current_position))
		{
			*json_string = current_position;
			return true;
		}

		// process array elements
        do
        {
            out_value->data.array.count += 1;
            out_value->data.array.values = (JsonValue*) realloc(out_value->data.array.values, sizeof(JsonValue) * out_value->data.array.count);
            JsonValue* array_value = &out_value->data.array.values[out_value->data.array.count - 1];
            memset(array_value, 0, sizeof(JsonValue));
            if (!parse_json_value(&current_position, array_value))
            {
                return false;
            }

        } while (consume(',', &current_position));

		// consume closing brace
        if (!consume(']', &current_position))
		{
			return false;
		}
    }
    else if (string_check(current_position, STRING_TRUE))
    {
        out_value->type = JSON_VALUE_TYPE_BOOLEAN;
        out_value->data.boolean = true;
        current_position += strlen(STRING_TRUE);
    }
    else if (string_check(current_position, STRING_FALSE))
    {
        out_value->type = JSON_VALUE_TYPE_BOOLEAN;
        out_value->data.boolean = false;
        current_position += strlen(STRING_FALSE);
    }

    *json_string = current_position;
    return true;
}

// Modifies json_string (necessary for recursion)
bool parse_json_object(char** json_string, JsonObject* out_json_object)
{
    char* current_position = *json_string;
    // 1. Consume leading whitespace and '{'
    if (!consume('{', &current_position))
    {
        return false;
    }

	// 2. Check for empty object
	if (consume('}', &current_position))
	{
		*json_string = current_position;
		return true;
	}

    // 3. Iterate over key/value pairs
    out_json_object->count = 0;
    out_json_object->key_value_pairs = NULL;
    do
    {
        out_json_object->count += 1;
        out_json_object->key_value_pairs = (JsonKeyValuePair*) realloc(out_json_object->key_value_pairs, sizeof(JsonKeyValuePair) * out_json_object->count);
        JsonKeyValuePair* key_value = &out_json_object->key_value_pairs[out_json_object->count - 1];
        memset(key_value, 0, sizeof(JsonKeyValuePair));

        // Key (string)
        key_value->key = parse_string(&current_position);
        if (key_value->key == NULL)
        {
            return false;
        }

        if (!consume(':', &current_position))
        {
            return false;
        }
        if (!parse_json_value(&current_position, &key_value->value))
        {
            return false;
        }
    } while (consume(',', &current_position));

    // Consume final closing bracket
    if (!consume('}', &current_position))
    {
        return false;
    }
    *json_string = current_position;
    return true;
}

bool json_value_as_float(const JsonValue* value, float* out_float)
{
    if (value && value->type == JSON_VALUE_TYPE_NUMBER && out_float)
    {
        *out_float = value->data.number;
        return true;
    }
    return false;
}

bool json_value_as_i32(const JsonValue* value, i32* out_int)
{
    if (value && value->type == JSON_VALUE_TYPE_NUMBER && out_int)
    {
        *out_int = (i32) value->data.number;
        return true;
    }
    return false;
}

bool json_value_as_bool(const JsonValue* value, bool* out_bool)
{
    if (value && value->type == JSON_VALUE_TYPE_BOOLEAN && out_bool)
    {
        *out_bool = value->data.boolean;
        return true;
    }
    return false;
}

bool json_value_as_string(const JsonValue* value, const char** out_string)
{
    if (value && value->type == JSON_VALUE_TYPE_STRING && out_string)
    {
        *out_string = value->data.string;
        return true;
    }
    return false;
}

bool json_value_as_array(const JsonValue* value, const JsonArray** out_array)
{
    if (value && value->type == JSON_VALUE_TYPE_ARRAY)
    {
        *out_array = &value->data.array;
        return true;
    }
    return false;
}

bool json_value_as_object(const JsonValue* value, const JsonObject** out_object)
{
    if (value && value->type == JSON_VALUE_TYPE_OBJECT)
    {
        *out_object = &value->data.object;
        return true;
    }
    return false;
}

const JsonValue* json_object_get_value(const JsonObject* object, const char* key)
{
    for (i32 i = 0; i < object->count; ++i)
    {
        JsonKeyValuePair* key_value = &object->key_value_pairs[i];
        if (strcmp(key_value->key, key) == 0)
        {
            return &key_value->value;
        }
    }
    return NULL;
}

const JsonValue* json_array_get_value(const JsonArray* array, i32 index)
{
    if (index < array->count)
    {
        return &array->values[index];
    }
    return NULL;
}

void json_copy_float_array(float* dst, const JsonArray* array, i32 count)
{
    assert(count <= array->count);

    for (i32 i = 0; i < count; ++i)
    {
        float value;
        assert(json_value_as_float(json_array_get_value(array, i), &value));
        dst[i] = value;
    }
}

// 	Convenience Functions Below
//  the JsonObject and JsonArray getters return pointers
//  TODO: the other value getters return true on success and set the output via
//  an argument

const JsonObject* json_object_get_object(const JsonObject* object, const char* key)
{
    const JsonObject* out_object = NULL;
    json_value_as_object(json_object_get_value(object, key), &out_object);
    return out_object;
}

const JsonArray* json_object_get_array(const JsonObject* object, const char* key)
{
    const JsonArray* out_array = NULL;
    json_value_as_array(json_object_get_value(object, key), &out_array);
    return out_array;
}

const JsonObject* json_array_get_object(const JsonArray* array, i32 index)
{
    const JsonObject* out_object = NULL;
    json_value_as_object(json_array_get_value(array, index), &out_object);
    return out_object;
}

const JsonArray* json_array_get_array(const JsonArray* array, i32 index)
{
    const JsonArray* out_array = NULL;
    json_value_as_array(json_array_get_value(array, index), &out_array);
    return out_array;
}

void free_json_object(JsonObject* in_object);

void free_json_value(JsonValue* in_value)
{
    switch (in_value->type)
    {
    case JSON_VALUE_TYPE_OBJECT:
        free_json_object(&in_value->data.object);
        break;
    case JSON_VALUE_TYPE_ARRAY:
        for (i32 i = 0; i < in_value->data.array.count; ++i)
        {
            free_json_value(&in_value->data.array.values[i]);
        }
        free(in_value->data.array.values);
        break;
    case JSON_VALUE_TYPE_STRING:
        free(in_value->data.string);
        break;
    default:
        break;
    }
}

void free_json_object(JsonObject* in_object)
{
    for (i32 i = 0; i < in_object->count; ++i)
    {
        JsonKeyValuePair* key_value = &in_object->key_value_pairs[i];
        free_json_value(&key_value->value);
    }
    free(in_object->key_value_pairs);
}

static inline void indent(FILE* out_file, int n)
{
    for (int i = 0; i < n; i++)
    {
        fprintf(out_file, "\t");
    }
}

static inline void indent_printf(FILE* out_file, int n, const char* format, ...)
{
    indent(out_file, n);
    va_list args;
    va_start(args, format);
    vfprintf(out_file, format, args);
    va_end(args);
}

void print_json_object(const JsonObject* in_object, int depth, FILE* out_file);

void print_json_value(const JsonValue* in_value, int depth, bool leading_indent, FILE* out_file)
{
    if (leading_indent)
    {
        indent(out_file, depth);
    }

    switch (in_value->type)
    {
    case JSON_VALUE_TYPE_OBJECT:
        print_json_object(&in_value->data.object, depth, out_file);
        break;
    case JSON_VALUE_TYPE_NUMBER:
        fprintf(out_file, "%f", in_value->data.number);
        break;
    case JSON_VALUE_TYPE_BOOLEAN:
        fprintf(out_file, "%s", in_value->data.boolean ? "true" : "false");
        break;
    case JSON_VALUE_TYPE_ARRAY:
        fprintf(out_file, "[");
        for (i32 i = 0; i < in_value->data.array.count; ++i)
        {
            fprintf(out_file, "\n");
            print_json_value(&in_value->data.array.values[i], depth + 1, true, out_file);
            if (i < in_value->data.array.count - 1)
            {
                fprintf(out_file, ", ");
            }
        }
        fprintf(out_file, "\n");
        indent_printf(out_file, depth, "]");
        break;
    case JSON_VALUE_TYPE_STRING:
        fprintf(out_file, "\"%s\"", in_value->data.string);
        break;
    default:
        break;
    }
}

void print_json_object(const JsonObject* in_object, int depth, FILE* out_file)
{

    fprintf(out_file, "{\n");

    for (i32 i = 0; i < in_object->count; ++i)
    {
        JsonKeyValuePair* key_value = &in_object->key_value_pairs[i];
        indent_printf(out_file, depth + 1, "\"%s\" : ", key_value->key);

        print_json_value(&key_value->value, depth + 1, false, out_file);
        if (i < in_object->count - 1)
        {
            fprintf(out_file, ",");
        }
        fprintf(out_file, "\n");
    }
    indent_printf(out_file, depth, "}");

    if (depth == 0)
    {
        fprintf(out_file, "\n");
    }
}

typedef struct GltfBuffer
{
    i32 byte_length;
    u8* data;
} GltfBuffer;

typedef struct GltfBufferView
{
    i32 byte_length;
    i32 byte_offset;
    GltfBuffer* buffer;
} GltfBufferView;

typedef enum GltfComponentType
{
    GLTF_COMPONENT_TYPE_BYTE = 5120,
    GLTF_COMPONENT_TYPE_UNSIGNED_BYTE = 5121,
    GLTF_COMPONENT_TYPE_SHORT = 5122,
    GLTF_COMPONENT_TYPE_UNSIGNED_SHORT = 5123,
    GLTF_COMPONENT_TYPE_UNSIGNED_INT = 5125,
    GLTF_COMPONENT_TYPE_FLOAT = 5126,
} GltfComponentType;

i32 gltf_component_type_size(GltfComponentType type)
{
    switch (type)
    {
    case GLTF_COMPONENT_TYPE_BYTE:
    case GLTF_COMPONENT_TYPE_UNSIGNED_BYTE:
        return 1;
    case GLTF_COMPONENT_TYPE_SHORT:
    case GLTF_COMPONENT_TYPE_UNSIGNED_SHORT:
        return 2;
    case GLTF_COMPONENT_TYPE_UNSIGNED_INT:
    case GLTF_COMPONENT_TYPE_FLOAT:
        return 4;
    default:
        printf("GLTF ERROR: INVALID COMPONENT TYPE SIZE\n");
        exit(0);
        return 0;
    }
}

typedef enum GltfAccessorType
{
    GLTF_ACCESSOR_TYPE_SCALAR,
    GLTF_ACCESSOR_TYPE_VEC2,
    GLTF_ACCESSOR_TYPE_VEC3,
    GLTF_ACCESSOR_TYPE_VEC4,
    GLTF_ACCESSOR_TYPE_MAT2,
    GLTF_ACCESSOR_TYPE_MAT3,
    GLTF_ACCESSOR_TYPE_MAT4,
} GltfAccessorType;

i32 gltf_accessor_type_size(GltfAccessorType type)
{
    switch (type)
    {
    case GLTF_ACCESSOR_TYPE_SCALAR:
        return 1;
    case GLTF_ACCESSOR_TYPE_VEC2:
        return 2;
    case GLTF_ACCESSOR_TYPE_VEC3:
        return 3;
    case GLTF_ACCESSOR_TYPE_VEC4:
    case GLTF_ACCESSOR_TYPE_MAT2:
        return 4;
    case GLTF_ACCESSOR_TYPE_MAT3:
        return 9;
    case GLTF_ACCESSOR_TYPE_MAT4:
        return 16;
    default:
        printf("GLTF ERROR: INVALID ACCESSOR TYPE\n");
        exit(0);
        return 0;
    }
}

#define CHECK_ACCESSOR_TYPE(str, suffix)    \
    if (strcmp(str, #suffix) == 0)          \
    {                                       \
        return GLTF_ACCESSOR_TYPE_##suffix; \
    }

GltfAccessorType str_to_gltf_accessor_type(const char* str)
{
    CHECK_ACCESSOR_TYPE(str, SCALAR);
    CHECK_ACCESSOR_TYPE(str, VEC2);
    CHECK_ACCESSOR_TYPE(str, VEC3);
    CHECK_ACCESSOR_TYPE(str, VEC4);
    CHECK_ACCESSOR_TYPE(str, MAT2);
    CHECK_ACCESSOR_TYPE(str, MAT3);
    CHECK_ACCESSOR_TYPE(str, MAT4);
    return (GltfAccessorType) -1; // Should Never Hit
}

// Stores info for Accessor/View/Buffer
typedef struct GltfAccessor
{
    GltfComponentType component_type;
    GltfAccessorType accessor_type;
    i32 count;
    i32 byte_offset;
    GltfBufferView* buffer_view;
} GltfAccessor;

i32 gltf_accessor_get_initial_offset(GltfAccessor* accessor)
{
    return accessor->byte_offset + accessor->buffer_view->byte_offset;
}

i32 gltf_accessor_get_stride(GltfAccessor* accessor)
{
    return gltf_accessor_type_size(accessor->accessor_type) * gltf_component_type_size(accessor->component_type);
}

typedef struct GltfPrimitive
{
    GltfAccessor* positions;
    GltfAccessor* normals;
    // TODO: TangentAccessor
    GltfAccessor* texcoord0;
    // TODO: Color Accessor
    GltfAccessor* joint_indices;
    GltfAccessor* joint_weights;
    GltfAccessor* indices;
} GltfPrimitive;

typedef struct GltfMesh
{
    const char* name;
    i32 num_primitives;
    GltfPrimitive* primitives;
} GltfMesh;

typedef struct GltfSkin
{
    GltfAccessor* inverse_bind_matrices;
    i32 num_joints;
    struct GltfNode** joints;
} GltfSkin;

//FCS TODO: GltfTransform (either matrix or TRS)
//FCS TODO: When a node is targeted for animation (referenced by an animation.channel.target), 
// 			only TRS properties may be present; matrix will not be present.

typedef enum GltfTransformType
{
	GLTF_TRANSFORM_TYPE_MATRIX = 0,
	GLTF_TRANSFORM_TYPE_TRS = 1,
} GltfTransformType;

typedef struct GltfTransform
{
	GltfTransformType type;
	union
	{
		Mat4 matrix;
		struct
		{
			Vec3 scale;
			Quat rotation;
			Vec3 translation;
		};
	};
} GltfTransform;

Mat4 gltf_transform_to_mat4(const GltfTransform* in_transform)
{
	if (in_transform->type == GLTF_TRANSFORM_TYPE_MATRIX)
	{
		return in_transform->matrix;
	}
	else
	{
		assert(in_transform->type == GLTF_TRANSFORM_TYPE_TRS);
		Mat4 scale_matrix = mat4_scale(in_transform->scale);
		Mat4 rotation_matrix = quat_to_mat4(in_transform->rotation);
		Mat4 translation_matrix = mat4_translation(in_transform->translation);
		return mat4_mul_mat4(mat4_mul_mat4(scale_matrix, rotation_matrix), translation_matrix);
	}
}

typedef struct GltfNode
{
    const char* name;
    GltfTransform transform;

    struct GltfNode* parent;

    i32 num_children;
    struct GltfNode** children;

    GltfMesh* mesh;
    GltfSkin* skin;
} GltfNode;

typedef enum GltfInterpolation
{
    GLTF_INTERPOLATION_LINEAR		= 0,
    GLTF_INTERPOLATION_STEP			= 1,
    GLTF_INTERPOLATION_CUBIC_SPLINE	= 2,
} GltfInterpolation;

typedef struct GltfAnimationSampler
{
    GltfInterpolation interpolation;
    GltfAccessor* input;
    GltfAccessor* output;

} GltfAnimationSampler;

typedef enum GltfAnimationPath
{
    GLTF_ANIMATION_PATH_TRANSLATION,
    GLTF_ANIMATION_PATH_ROTATION,
    GLTF_ANIMATION_PATH_SCALE,
    GLTF_ANIMATION_PATH_MORPH_WEIGHTS,
} GltfAnimationPath;

typedef struct GltfAnimationTarget
{
    GltfNode* node;
    GltfAnimationPath path;
} GltfAnimationTarget;

typedef struct GltfAnimationChannel
{
    GltfAnimationTarget target;
    GltfAnimationSampler* sampler;
} GltfAnimationChannel;

typedef struct GltfAnimation
{
    i32 num_channels;
    GltfAnimationChannel* channels;

    i32 num_samplers;
    GltfAnimationSampler* samplers;

} GltfAnimation;

typedef struct GltfAsset
{
    JsonObject json;

    i32 num_buffers;
    GltfBuffer* buffers;

    i32 num_buffer_views;
    GltfBufferView* buffer_views;

    i32 num_accessors;
    GltfAccessor* accessors;

    i32 num_meshes;
    GltfMesh* meshes;

    i32 num_skins;
    GltfSkin* skins;

    i32 num_nodes;
    GltfNode* nodes;

    i32 num_animations;
    GltfAnimation* animations;
} GltfAsset;

void print_gltf_asset(GltfAsset* in_asset)
{
    print_json_object(&in_asset->json, 0, stdout);
}

bool gltf_load_asset(const char* filename, GltfAsset* out_asset)
{
	printf("gltf_load_asset: %s\n", filename);
    // TODO: check extension, add functions for GLB and GLTF (only GLB is currently supported)

    FILE* file = fopen(filename, "rb");

    if (file && out_asset)
    {
        // HEADER
        {
            u32 magic;
            if (!(fread(&magic, 4, 1, file) == 1 && magic == GLTF_MAGIC_NUMBER))
            {
            	return false;
			}

			u32 version;
            if (!(fread(&version, 4, 1, file) == 1))
            {
            	return false;
            }

			u32 length;
            if (!(fread(&length, 4, 1, file) == 1))
            {
            	return false;
            }
        }

        // JSON
        {
            i32 json_length;
            if (!(fread(&json_length, 4, 1, file) == 1))
            {
				return false;
            }

			i32 json_type;
            if (!(fread(&json_type, 4, 1, file) == 1 && json_type == GLTF_CHUNK_TYPE_JSON))
            {
				return false;
            }

            char* json_string = (char*) malloc(json_length + 1);
            json_string[json_length] = 0; // Null-terminate string

            if (fread(json_string, json_length, 1, file) != 0)
            {
                char* modified_json_string = json_string;
                if (!parse_json_object(&modified_json_string, &out_asset->json))
				{
					free(json_string);
					return false;
				}
            }

            free(json_string);
        }

        // BUFFERS
        {
            out_asset->num_buffers = 0;
            out_asset->buffers = NULL;

            i32 buffer_length;
            while (fread(&buffer_length, 4, 1, file) == 1)
            {
                // Increment num_buffers and realloc
                out_asset->num_buffers++;
                out_asset->buffers = (GltfBuffer*) realloc(out_asset->buffers, sizeof(GltfBuffer) * out_asset->num_buffers);

                i32 buffer_type = 0;
                if (fread(&buffer_type, 4, 1, file) != 1 || buffer_type != GLTF_CHUNK_TYPE_BUFFER)
                {
                    return false;
                }

                u8* buffer_data = (u8*) malloc(buffer_length);
                if (fread(buffer_data, buffer_length, 1, file) == 1)
                {
                    out_asset->buffers[out_asset->num_buffers - 1].byte_length = buffer_length;
                    out_asset->buffers[out_asset->num_buffers - 1].data = buffer_data;
                }
                else
                {
                    return false;
                }
            }
        }

        // BUFFER VIEWS
        {
            const JsonArray* json_buffer_views = json_object_get_array(&out_asset->json, "bufferViews");
            if (!json_buffer_views)
            {
                return false;
            }

            out_asset->num_buffer_views = json_buffer_views->count;
            out_asset->buffer_views = (GltfBufferView*) calloc(out_asset->num_buffer_views, sizeof(GltfBufferView));

            for (i32 i = 0; i < out_asset->num_buffer_views; ++i)
            {
                GltfBufferView* buffer_view = &out_asset->buffer_views[i];
                const JsonObject* json_buffer_view = json_array_get_object(json_buffer_views, i);

                i32 buffer_index = 0;
                if (json_value_as_i32(json_object_get_value(json_buffer_view, "buffer"), &buffer_index) && buffer_index < out_asset->num_buffers)
                {
                    buffer_view->buffer = &out_asset->buffers[buffer_index];
                }
                else
                {
                    return false;
                }

                if (!json_value_as_i32(json_object_get_value(json_buffer_view, "byteLength"), &buffer_view->byte_length))
                {
                    return false;
                }

                json_value_as_i32(json_object_get_value(json_buffer_view, "byteOffset"), &buffer_view->byte_offset);
            }
        }

        // ACCESSORS
        {
            const JsonArray* json_accessors = json_object_get_array(&out_asset->json, "accessors");
            if (json_accessors)
            {
                out_asset->num_accessors = json_accessors->count;
                out_asset->accessors = (GltfAccessor*) calloc(out_asset->num_accessors, sizeof(GltfAccessor));

                for (i32 i = 0; i < out_asset->num_accessors; ++i)
                {
                    GltfAccessor* accessor = &out_asset->accessors[i];
                    const JsonObject* json_accessor = json_array_get_object(json_accessors, i);

                    i32 buffer_view_index = 0;
                    if (json_value_as_i32(json_object_get_value(json_accessor, "bufferView"), &buffer_view_index) && buffer_view_index < out_asset->num_buffer_views)
                    {
                        accessor->buffer_view = &out_asset->buffer_views[buffer_view_index];
                    }
                    else
                    {
                        return false;
                    }

                    if (!json_value_as_i32(json_object_get_value(json_accessor, "componentType"), (i32*) &accessor->component_type))
                    {
                        return false;
                    }
                    if (!json_value_as_i32(json_object_get_value(json_accessor, "count"), &accessor->count))
                    {
                        return false;
                    }
                    const char* type_string = NULL;
                    if (json_value_as_string(json_object_get_value(json_accessor, "type"), &type_string))
                    {
                        accessor->accessor_type = str_to_gltf_accessor_type(type_string);
                    }

                    json_value_as_i32(json_object_get_value(json_accessor, "byteOffset"), &accessor->byte_offset);
                }
            }
        }

        // MESHES
        {
            const JsonArray* json_meshes = json_object_get_array(&out_asset->json, "meshes");
            if (json_meshes)
            {
                out_asset->num_meshes = json_meshes->count;
                out_asset->meshes = (GltfMesh*) calloc(out_asset->num_meshes, sizeof(GltfMesh));

                for (i32 i = 0; i < out_asset->num_meshes; ++i)
                {
                    GltfMesh* mesh = &out_asset->meshes[i];
                    const JsonObject* json_mesh = json_array_get_object(json_meshes, i);
                    const JsonArray* json_primitives = json_object_get_array(json_mesh, "primitives");

                    // We keep the json alive for the duration of the gltf asset, so just point to the json string
                    json_value_as_string(json_object_get_value(json_mesh, "name"), &mesh->name);

                    if (!json_primitives)
                    {
                        return false;
                    }

                    mesh->num_primitives = json_primitives->count;
                    mesh->primitives = (GltfPrimitive*) calloc(mesh->num_primitives, sizeof(GltfPrimitive));

                    for (i32 j = 0; j < mesh->num_primitives; ++j)
                    {
                        GltfPrimitive* primitive = &mesh->primitives[j];
                        const JsonObject* json_primitive = json_array_get_object(json_primitives, j);
                        const JsonObject* json_attributes = json_object_get_object(json_primitive, "attributes");

                        // TODO: Primitive Topology (Triangle (4) is default, but check for
                        // others)

                        i32 positions_index = 0;
                        if (json_value_as_i32(json_object_get_value(json_attributes, "POSITION"), &positions_index) && positions_index < out_asset->num_accessors)
                        {
                            primitive->positions = &out_asset->accessors[positions_index];
                        }

                        i32 normals_index = 0;
                        if (json_value_as_i32(json_object_get_value(json_attributes, "NORMAL"), &normals_index) && normals_index < out_asset->num_accessors)
                        {
                            primitive->normals = &out_asset->accessors[normals_index];
                        }

                        i32 texcoord0_index = 0;
                        if (json_value_as_i32(json_object_get_value(json_attributes, "TEXCOORD_0"), &texcoord0_index) && texcoord0_index < out_asset->num_accessors)
                        {
                            primitive->texcoord0 = &out_asset->accessors[texcoord0_index];
                        }

                        i32 joints_index = 0;
                        if (json_value_as_i32(json_object_get_value(json_attributes, "JOINTS_0"), &joints_index) && joints_index < out_asset->num_accessors)
                        {
                            primitive->joint_indices = &out_asset->accessors[joints_index];
                        }

                        i32 weights_index = 0;
                        if (json_value_as_i32(json_object_get_value(json_attributes, "WEIGHTS_0"), &weights_index) && weights_index < out_asset->num_accessors)
                        {
                            primitive->joint_weights = &out_asset->accessors[weights_index];
                        }

                        i32 indices_index = 0;
                        if (json_value_as_i32(json_object_get_value(json_primitive, "indices"), &indices_index) && indices_index < out_asset->num_accessors)
                        {
                            primitive->indices = &out_asset->accessors[indices_index];
                        }
                    }
                }
            }
        }

        // NODES
        {
            const JsonArray* json_nodes = json_object_get_array(&out_asset->json, "nodes");
            if (json_nodes)
            {
                out_asset->num_nodes = json_nodes->count;
                out_asset->nodes = (GltfNode*) calloc(out_asset->num_nodes, sizeof(GltfNode));

                // SKINS (we set up skins here because they reference the nodes we just allocated above)
                {
                    const JsonArray* json_skins = json_object_get_array(&out_asset->json, "skins");
                    if (json_skins)
                    {
                        out_asset->num_skins = json_skins->count;
                        out_asset->skins = (GltfSkin*) calloc(out_asset->num_skins, sizeof(GltfSkin));

                        for (i32 skin_index = 0; skin_index < out_asset->num_skins; ++skin_index)
                        {
                            GltfSkin* skin = &out_asset->skins[skin_index];
                            const JsonObject* json_skin = json_array_get_object(json_skins, skin_index);

                            i32 inverse_bind_matrices_accessor_index;
                            if (json_value_as_i32(json_object_get_value(json_skin, "inverseBindMatrices"), &inverse_bind_matrices_accessor_index))
                            {
                                skin->inverse_bind_matrices = &out_asset->accessors[inverse_bind_matrices_accessor_index];
                            }

                            const JsonArray* json_joints = json_object_get_array(json_skin, "joints");
                            if (json_joints)
                            {
                                skin->num_joints = json_joints->count;
                                skin->joints = calloc(skin->num_joints, sizeof(GltfNode*));
                                for (i32 joint_array_index = 0; joint_array_index < skin->num_joints; ++joint_array_index)
                                {
                                    i32 joint_node_index;
                                    if (json_value_as_i32(json_array_get_value(json_joints, joint_array_index), &joint_node_index))
                                    {
                                        skin->joints[joint_array_index] = &out_asset->nodes[joint_node_index];
                                    }
                                }
                            }
                        }
                    }
                }

                for (i32 i = 0; i < out_asset->num_nodes; ++i)
                {
                    GltfNode* node = &out_asset->nodes[i];
                    const JsonObject* json_node = json_array_get_object(json_nodes, i);

                    // We keep the json alive for the duration of the gltf asset, so just point to the json string
                    json_value_as_string(json_object_get_value(json_node, "name"), &node->name);

                    // Set up pointer array to our children nodes
                    const JsonArray* json_children_nodes = json_object_get_array(json_node, "children");
                    if (json_children_nodes)
                    {
                        node->num_children = json_children_nodes->count;
                        node->children = calloc(node->num_children, sizeof(GltfNode*));
                        for (i32 index_in_children_array = 0; index_in_children_array < node->num_children; ++index_in_children_array)
                        {
                            i32 index_in_nodes_array;
                            if (json_value_as_i32(json_array_get_value(json_children_nodes, index_in_children_array), &index_in_nodes_array))
                            {
                                // Children may not be set up yet, but we can still point to their memory which we've already allocated
                                node->children[index_in_children_array] = &out_asset->nodes[index_in_nodes_array];
                                node->children[index_in_children_array]->parent = node;
                            }
                        }
                    }

					node->transform.type = GLTF_TRANSFORM_TYPE_MATRIX;
					node->transform.matrix = mat4_identity;
 
					// 1. search for "matrix"
                    // 2. If no matrix, search for translation, rotation, scale
                    const JsonArray* json_matrix = json_object_get_array(json_node, "matrix");
                    if (json_matrix)
                    {
						node->transform.type = GLTF_TRANSFORM_TYPE_MATRIX; 
                        json_copy_float_array((float*)node->transform.matrix.d, json_matrix, 16);
                    }
                    else
                    {
						node->transform.type = GLTF_TRANSFORM_TYPE_TRS;

                        Vec3 scale = {1.f, 1.f, 1.f};
                        const JsonArray* json_scale = json_object_get_array(json_node, "scale");
                        if (json_scale)
                        {
                            json_copy_float_array((float*)&scale, json_scale, 3);
                        }
						node->transform.scale = scale; 

						Quat rotation = quat_identity;
                        const JsonArray* json_rotation = json_object_get_array(json_node, "rotation");
                        if (json_rotation)
                        {
                            json_copy_float_array((float*)&rotation, json_rotation, 4);
                        }
						node->transform.rotation = quat_normalize(rotation);

						Vec3 translation = vec3_new(0.f, 0.f, 0.f);
                        const JsonArray* json_translation = json_object_get_array(json_node, "translation");
                        if (json_translation)
                        {
                            json_copy_float_array((float*)&translation, json_translation, 3);
                        }
						node->transform.translation = translation;
                    }

                    // Optional Node Mesh
                    i32 node_mesh_index;
                    if (json_value_as_i32(json_object_get_value(json_node, "mesh"), &node_mesh_index))
                    {
                        node->mesh = &out_asset->meshes[node_mesh_index];
                    }

                    // Optional Node Skin
                    i32 skin_index;
                    if (json_value_as_i32(json_object_get_value(json_node, "skin"), &skin_index))
                    {
                        // Skins are already allocated so just point directly to the correct skin here
                        node->skin = &out_asset->skins[skin_index];
                    }
                }
            }
        }

        // ANIMATIONS
        {
            const JsonArray* json_animations = json_object_get_array(&out_asset->json, "animations");
            if (json_animations)
            {
                out_asset->num_animations = json_animations->count;
                out_asset->animations = (GltfAnimation*) calloc(out_asset->num_animations, sizeof(GltfAnimation));

                for (i32 anim_index = 0; anim_index < out_asset->num_animations; ++anim_index)
                {
                    GltfAnimation* animation = &out_asset->animations[anim_index];
                    const JsonObject* json_animation = json_array_get_object(json_animations, anim_index);
                    assert(json_animation);

                    // SAMPLERS
                    const JsonArray* json_samplers = json_object_get_array(json_animation, "samplers");
                    if (json_samplers)
                    {
                        animation->num_samplers = json_samplers->count;
                        animation->samplers = (GltfAnimationSampler*) calloc(animation->num_samplers, sizeof(GltfAnimationSampler));

                        for (i32 sampler_index = 0; sampler_index < animation->num_samplers; ++sampler_index)
                        {
                            GltfAnimationSampler* sampler = &animation->samplers[sampler_index];
                            const JsonObject* json_sampler = json_array_get_object(json_samplers, sampler_index);
                            assert(json_sampler);

                            const char* interpolation_string = NULL;
                            if (json_value_as_string(json_object_get_value(json_sampler, "interpolation"), &interpolation_string))
                            {
                                if (strcmp(interpolation_string, "LINEAR") == 0)
                                {
                                    sampler->interpolation = GLTF_INTERPOLATION_LINEAR;
                                }
                                else if (strcmp(interpolation_string, "STEP") == 0)
                                {
                                    sampler->interpolation = GLTF_INTERPOLATION_STEP;
                                }
                                else if (strcmp(interpolation_string, "CUBICSPLINE") == 0)
                                {
                                    sampler->interpolation = GLTF_INTERPOLATION_CUBIC_SPLINE;
                                }
                                else
                                {
                                    // Invalid data...
                                    return false;
                                }
                            }

                            i32 input_accessor_index;
                            if (json_value_as_i32(json_object_get_value(json_sampler, "input"), &input_accessor_index))
                            {
                                assert(input_accessor_index < out_asset->num_accessors);
                                sampler->input = &out_asset->accessors[input_accessor_index];
                            }


                            i32 output_accessor_index;
                            if (json_value_as_i32(json_object_get_value(json_sampler, "output"), &output_accessor_index))
                            {
                                assert(output_accessor_index < out_asset->num_accessors);
                                sampler->output = &out_asset->accessors[output_accessor_index];
                            }
                        }
                    }

                    // CHANNELS
                    const JsonArray* json_channels = json_object_get_array(json_animation, "channels");
                    if (json_channels)
                    {
                        animation->num_channels = json_channels->count;
                        animation->channels = (GltfAnimationChannel*) calloc(animation->num_channels, sizeof(GltfAnimationChannel));

                        for (i32 channel_index = 0; channel_index < animation->num_samplers; ++channel_index)
                        {
                            GltfAnimationChannel* channel = &animation->channels[channel_index];
                            const JsonObject* json_channel = json_array_get_object(json_channels, channel_index);
                            assert(json_channel);

                            const JsonObject* json_target = json_object_get_object(json_channel, "target");
                            if (json_target)
                            {
                                i32 node_index;
                                if (json_value_as_i32(json_object_get_value(json_target, "node"), &node_index))
                                {
                                    assert(node_index < out_asset->num_nodes);
                                    channel->target.node = &out_asset->nodes[node_index];

									// Animated Nodes must use TRS transform type
									assert(channel->target.node->transform.type == GLTF_TRANSFORM_TYPE_TRS);
                                }

                                const char* path_string;
                                if (json_value_as_string(json_object_get_value(json_target, "path"), &path_string))
                                {
                                    if (strcmp(path_string, "translation") == 0)
                                    {
                                        channel->target.path = GLTF_ANIMATION_PATH_TRANSLATION;
                                    }
                                    else if (strcmp(path_string, "rotation") == 0)
                                    {
                                        channel->target.path = GLTF_ANIMATION_PATH_ROTATION;
                                    }
                                    else if (strcmp(path_string, "scale") == 0)
                                    {
                                        channel->target.path = GLTF_ANIMATION_PATH_SCALE;
                                    }
                                    else if (strcmp(path_string, "weights") == 0)
                                    {
                                        channel->target.path = GLTF_ANIMATION_PATH_MORPH_WEIGHTS;
                                    }
                                }
                            }

                            i32 sampler_index;
                            if (json_value_as_i32(json_object_get_value(json_channel, "sampler"), &sampler_index))
                            {
                                assert(sampler_index < animation->num_samplers);
                                channel->sampler = &animation->samplers[sampler_index];
                            }
                        }
                    }
                }
            }
        }

        // TODO: SCENES
        // TODO: TEXTURES
        // TODO: MATERIALS

        return true;
    }

    return false;
}

// FIXME: better checks above
// FIXME: Outline guarantees (i.e. For a successfully loaded asset, an accessors
// buffer view is non-null, a buffer_views bufer is non-null, etc.)

void gltf_free_asset(GltfAsset* asset)
{
    for (i32 i = 0; i < asset->num_meshes; ++i)
    {
        free(asset->meshes[i].primitives);
    }
    free(asset->meshes);

    free(asset->accessors);
    free(asset->buffer_views);

    for (i32 i = 0; i < asset->num_buffers; ++i)
    {
        free(asset->buffers[i].data);
    }
    free(asset->buffers);

    for (i32 i = 0; i < asset->num_skins; ++i)
    {
        free(asset->skins[i].joints);
    }
    free(asset->skins);

    for (i32 i = 0; i < asset->num_nodes; ++i)
    {
        free(asset->nodes[i].children);
    }
    free(asset->nodes);

    for (i32 i = 0; i < asset->num_animations; ++i)
    {
        free(asset->animations[i].channels);
        free(asset->animations[i].samplers);
    }
    free(asset->animations);

    free_json_object(&asset->json);
}
