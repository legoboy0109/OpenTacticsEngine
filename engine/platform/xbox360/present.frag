#version 450
// The engine's software-rendered frame, as it is, with alpha forced opaque (SDL's XRGB8888 leaves the top byte undefined).
layout(location = 0) in vec2 v_uv;
layout(location = 0) out vec4 o_color;
layout(set = 0, binding = 0) uniform sampler2D u_frame;

void main()
{
	o_color = vec4(texture(u_frame, v_uv).rgb, 1.0);
}
