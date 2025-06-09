#version 450

layout(location = 0) in vec2 o_velocity;

layout(location = 0) out vec4 o_color;

void main()
{
    float speed = clamp(length(o_velocity) / 2.0f, 0.0f, 1.0f);
    float hue = speed * 360.0f;
    float c = 1.0f;
    float x = c * (1.0 - abs(mod(hue / 60.0f, 2.0f) - 1.0f));
    float m = 0.0f;

    vec3 rgb;
    if (hue < 60.0f)
    {
        rgb = vec3(c, x, 0.0f);
    }
    else if (hue < 120.0f)
    {
        rgb = vec3(x, c, 0.0f);
    }
    else if (hue < 180.0f)
    {
        rgb = vec3(0.0f, c, x);
    }
    else if (hue < 240.0f)
    {
        rgb = vec3(0.0f, x, c);
    }
    else if (hue < 300.0f)
    {
        rgb = vec3(x, 0.0f, c);
    }
    else
    {
        rgb = vec3(c, 0.0f, x);
    }

    o_color = vec4(rgb + m, 1.0f - speed * 0.8f);
}