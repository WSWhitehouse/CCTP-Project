#extension GL_EXT_control_flow_attributes : require
#extension GL_KHR_memory_scope_semantics  : require
#extension GL_EXT_shader_atomic_float     : require

#extension GL_EXT_shader_explicit_arithmetic_types_int8 : require
#extension GL_EXT_shader_8bit_storage : require

#define EPSILON 1e-30f
#define EPSILON3 vec3(EPSILON, EPSILON, EPSILON)

#define CLAMP01(x) clamp(x, 0.0, 1.0)

#define INDEX_3D_TO_1D(index, count) ((index).x + (index).y * (count).x + (index).z * (count).x * (count).y)
#define INDEX_1D_TO_3D(index, count) ivec3((index) % (count).x, ((index) / (count).x) % (count).y, (index) / ((count).x * (count).y))

#define INDEX_FORMAT_16 0
#define INDEX_FORMAT_32 1

