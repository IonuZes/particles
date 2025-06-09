#version 450

layout(location = 0) in vec2 i_position;
layout(location = 1) in vec2 i_velocity;

layout(location = 0) out vec2 o_velocity;

layout(set = 1, binding = 0) uniform t_params
{
    vec2 u_viewport;
};

void main()
{
    gl_Position.x = (i_position.x / u_viewport.x) * 2.0f - 1.0f;
    gl_Position.y = (i_position.y / u_viewport.y) * 2.0f - 1.0f;
    gl_Position.z = 0.0f;
    gl_Position.w = 1.0f;

    o_velocity = i_velocity;

    gl_PointSize = 1.0f;
}