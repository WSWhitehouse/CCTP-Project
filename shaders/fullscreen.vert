#version 450

// Fullscreen "Quad": https://stackoverflow.com/a/59739538/13195883

layout(location = 0) out vec2 out_uv;

vec2 vertices[3] = vec2[] (
  vec2(-1, -1),
  vec2( 3, -1),
  vec2(-1,  3)
);

void main()
{
  gl_Position = vec4(vertices[gl_VertexIndex], 0.0, 1.0);
  out_uv      = 0.5 * gl_Position.xy + vec2(0.5);
}