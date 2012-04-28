-- VS

in vec4 Position;

void main()
{
    gl_Position = Position;
}

-- GS

layout(points) in;
layout(triangle_strip, max_vertices = 4) out;

out vec2 gTexCoord;

void main()
{
    gTexCoord = vec2(0, 0);
    gl_Position = vec4(-1, +1, 0, 1);
    EmitVertex();
    gTexCoord = vec2(0, 1);
    gl_Position = vec4(-1, -1, 0, 1);
    EmitVertex();
    gTexCoord = vec2(1, 0);
    gl_Position = vec4(+1, +1, 0, 1);
    EmitVertex();
    gTexCoord = vec2(1, 1);
    gl_Position = vec4(+1, -1, 0, 1);
    EmitVertex();
    EndPrimitive();
}

-- FS

in vec2 gTexCoord;
out vec4 FragColor;
uniform sampler2D Sampler;

void main()
{
    FragColor = texture(Sampler, gTexCoord);
}
