
-- Vertex

in vec4 Position;
out int vInstance;

void main()
{
    gl_Position = Position;
    vInstance = gl_InstanceID;
}

-- Fill

out vec3 FragColor;

void main()
{
    FragColor = vec3(1, 0, 0);
}

-- PickLayer

layout(triangles) in;
layout(triangle_strip, max_vertices = 3) out;
 
in int vInstance[3];
out float gLayer;
 
uniform float InverseSize;
 
void main()
{
    gl_Layer = vInstance[0];
    gLayer = float(gl_Layer) + 0.5;
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();
    gl_Position = gl_in[1].gl_Position;
    EmitVertex();
    gl_Position = gl_in[2].gl_Position;
    EmitVertex();
    EndPrimitive();
}

-- Advect

out vec4 FragColor;

uniform sampler3D VelocityTexture;
uniform sampler3D SourceTexture;
uniform sampler3D Obstacles;

uniform vec3 InverseSize;
uniform float TimeStep;
uniform float Dissipation;

in float gLayer;

void main()
{
    vec3 fragCoord = vec3(gl_FragCoord.xy, gLayer);
    float solid = texture(Obstacles, InverseSize * fragCoord).x;
    if (solid > 0) {
        FragColor = vec4(0);
        return;
    }

    vec3 u = texture(VelocityTexture, InverseSize * fragCoord).xyz;

    vec3 coord = InverseSize * (fragCoord - TimeStep * u);
    FragColor = Dissipation * texture(SourceTexture, coord);
}

-- Jacobi

out vec4 FragColor;

uniform sampler3D Pressure;
uniform sampler3D Divergence;
uniform sampler3D Obstacles;

uniform float Alpha;
uniform float InverseBeta;

in float gLayer;

void main()
{
    ivec3 T = ivec3(gl_FragCoord.xy, gLayer);

    // Find neighboring pressure:
    vec4 pN = texelFetchOffset(Pressure, T, 0, ivec3(0, 1, 0));
    vec4 pS = texelFetchOffset(Pressure, T, 0, ivec3(0, -1, 0));
    vec4 pE = texelFetchOffset(Pressure, T, 0, ivec3(1, 0, 0));
    vec4 pW = texelFetchOffset(Pressure, T, 0, ivec3(-1, 0, 0));
    vec4 pU = texelFetchOffset(Pressure, T, 0, ivec3(0, 0, 1));
    vec4 pD = texelFetchOffset(Pressure, T, 0, ivec3(0, 0, -1));
    vec4 pC = texelFetch(Pressure, T, 0);

    // Find neighboring obstacles:
    vec3 oN = texelFetchOffset(Obstacles, T, 0, ivec3(0, 1, 0)).xyz;
    vec3 oS = texelFetchOffset(Obstacles, T, 0, ivec3(0, -1, 0)).xyz;
    vec3 oE = texelFetchOffset(Obstacles, T, 0, ivec3(1, 0, 0)).xyz;
    vec3 oW = texelFetchOffset(Obstacles, T, 0, ivec3(-1, 0, 0)).xyz;
    vec3 oU = texelFetchOffset(Obstacles, T, 0, ivec3(0, 0, 1)).xyz;
    vec3 oD = texelFetchOffset(Obstacles, T, 0, ivec3(0, 0, -1)).xyz;

    // Use center pressure for solid cells:
    if (oN.x > 0) pN = pC;
    if (oS.x > 0) pS = pC;
    if (oE.x > 0) pE = pC;
    if (oW.x > 0) pW = pC;
    if (oU.x > 0) pU = pC;
    if (oD.x > 0) pD = pC;

    vec4 bC = texelFetch(Divergence, T, 0);
    FragColor = (pW + pE + pS + pN + pU + pD + Alpha * bC) * InverseBeta;
}

-- SubtractGradient

out vec3 FragColor;

uniform sampler3D Velocity;
uniform sampler3D Pressure;
uniform sampler3D Obstacles;
uniform float GradientScale;

in float gLayer;

void main()
{
    ivec3 T = ivec3(gl_FragCoord.xy, gLayer);

    vec3 oC = texelFetch(Obstacles, T, 0).xyz;
    if (oC.x > 0) {
        FragColor = oC.yzx;
        return;
    }

    // Find neighboring pressure:
    float pN = texelFetchOffset(Pressure, T, 0, ivec3(0, 1, 0)).r;
    float pS = texelFetchOffset(Pressure, T, 0, ivec3(0, -1, 0)).r;
    float pE = texelFetchOffset(Pressure, T, 0, ivec3(1, 0, 0)).r;
    float pW = texelFetchOffset(Pressure, T, 0, ivec3(-1, 0, 0)).r;
    float pU = texelFetchOffset(Pressure, T, 0, ivec3(0, 0, 1)).r;
    float pD = texelFetchOffset(Pressure, T, 0, ivec3(0, 0, -1)).r;
    float pC = texelFetch(Pressure, T, 0).r;

    // Find neighboring obstacles:
    vec3 oN = texelFetchOffset(Obstacles, T, 0, ivec3(0, 1, 0)).xyz;
    vec3 oS = texelFetchOffset(Obstacles, T, 0, ivec3(0, -1, 0)).xyz;
    vec3 oE = texelFetchOffset(Obstacles, T, 0, ivec3(1, 0, 0)).xyz;
    vec3 oW = texelFetchOffset(Obstacles, T, 0, ivec3(-1, 0, 0)).xyz;
    vec3 oU = texelFetchOffset(Obstacles, T, 0, ivec3(0, 0, 1)).xyz;
    vec3 oD = texelFetchOffset(Obstacles, T, 0, ivec3(0, 0, -1)).xyz;

    // Use center pressure for solid cells:
    vec3 obstV = vec3(0);
    vec3 vMask = vec3(1);

    if (oN.x > 0) { pN = pC; obstV.y = oN.z; vMask.y = 0; }
    if (oS.x > 0) { pS = pC; obstV.y = oS.z; vMask.y = 0; }
    if (oE.x > 0) { pE = pC; obstV.x = oE.y; vMask.x = 0; }
    if (oW.x > 0) { pW = pC; obstV.x = oW.y; vMask.x = 0; }
    if (oU.x > 0) { pU = pC; obstV.z = oU.x; vMask.z = 0; }
    if (oD.x > 0) { pD = pC; obstV.z = oD.x; vMask.z = 0; }

    // Enforce the free-slip boundary condition:
    vec3 oldV = texelFetch(Velocity, T, 0).xyz;
    vec3 grad = vec3(pE - pW, pN - pS, pU - pD) * GradientScale;
    vec3 newV = oldV - grad;
    FragColor = (vMask * newV) + obstV;  
}

-- ComputeDivergence

out float FragColor;

uniform sampler3D Velocity;
uniform sampler3D Obstacles;
uniform float HalfInverseCellSize;

in float gLayer;

void main()
{
    ivec3 T = ivec3(gl_FragCoord.xy, gLayer);

    // Find neighboring velocities:
    vec3 vN = texelFetchOffset(Velocity, T, 0, ivec3(0, 1, 0)).xyz;
    vec3 vS = texelFetchOffset(Velocity, T, 0, ivec3(0, -1, 0)).xyz;
    vec3 vE = texelFetchOffset(Velocity, T, 0, ivec3(1, 0, 0)).xyz;
    vec3 vW = texelFetchOffset(Velocity, T, 0, ivec3(-1, 0, 0)).xyz;
    vec3 vU = texelFetchOffset(Velocity, T, 0, ivec3(0, 0, 1)).xyz;
    vec3 vD = texelFetchOffset(Velocity, T, 0, ivec3(0, 0, -1)).xyz;

    // Find neighboring obstacles:
    vec3 oN = texelFetchOffset(Obstacles, T, 0, ivec3(0, 1, 0)).xyz;
    vec3 oS = texelFetchOffset(Obstacles, T, 0, ivec3(0, -1, 0)).xyz;
    vec3 oE = texelFetchOffset(Obstacles, T, 0, ivec3(1, 0, 0)).xyz;
    vec3 oW = texelFetchOffset(Obstacles, T, 0, ivec3(-1, 0, 0)).xyz;
    vec3 oU = texelFetchOffset(Obstacles, T, 0, ivec3(0, 0, 1)).xyz;
    vec3 oD = texelFetchOffset(Obstacles, T, 0, ivec3(0, 0, -1)).xyz;

    // Use obstacle velocities for solid cells:
    if (oN.x > 0) vN = oN.yzx;
    if (oS.x > 0) vS = oS.yzx;
    if (oE.x > 0) vE = oE.yzx;
    if (oW.x > 0) vW = oW.yzx;
    if (oU.x > 0) vU = oU.yzx;
    if (oD.x > 0) vD = oD.yzx;

    FragColor = HalfInverseCellSize * (vE.x - vW.x + vN.y - vS.y + vU.z - vD.z);
}

-- Splat

out vec4 FragColor;

uniform vec3 Point;
uniform float Radius;
uniform vec3 FillColor;

in float gLayer;

void main()
{
    float d = distance(Point, vec3(gl_FragCoord.xy, gLayer));
    if (d < Radius) {
        float a = (Radius - d) * 0.5;
        a = min(a, 1.0);
        FragColor = vec4(FillColor, a);
    } else {
        FragColor = vec4(0);
    }
}

-- Buoyancy

out vec3 FragColor;
uniform sampler3D Velocity;
uniform sampler3D Temperature;
uniform sampler3D Density;
uniform float AmbientTemperature;
uniform float TimeStep;
uniform float Sigma;
uniform float Kappa;

in float gLayer;

void main()
{
    ivec3 TC = ivec3(gl_FragCoord.xy, gLayer);
    float T = texelFetch(Temperature, TC, 0).r;
    vec3 V = texelFetch(Velocity, TC, 0).xyz;

    FragColor = V;

    if (T > AmbientTemperature) {
        float D = texelFetch(Density, TC, 0).x;
        FragColor += (TimeStep * (T - AmbientTemperature) * Sigma - D * Kappa ) * vec3(0, -1, 0);
    }
}
