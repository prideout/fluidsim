#include "Utility.h"
#include <cmath>
#include <cstdio>

using namespace vmath;
using std::string;

static struct {
    SlabPod Velocity;
    SlabPod Density;
    SlabPod Pressure;
    SlabPod Temperature;
} Slabs;

static struct {
    SurfacePod Divergence;
    SurfacePod Obstacles;
    SurfacePod HiresObstacles;
    SurfacePod LightCache;
    SurfacePod BlurredDensity;
} Surfaces;

static struct {
    Matrix4 Projection;
    Matrix4 Modelview;
    Matrix4 View;
    Matrix4 ModelviewProjection;
} Matrices;

static struct {
    GLuint CubeCenter;
    GLuint FullscreenQuad;
} Vbos;

static const Point3 EyePosition = Point3(0, 0, 2);
static GLuint RaycastProgram;
static GLuint LightProgram;
static GLuint TextProgram;
static GLuint BlurProgram;
static float FieldOfView = 0.7f;
static bool SimulateFluid = true;
static const float DefaultThetaX = 0;
static const float DefaultThetaY = 0.75f;
static float ThetaX = DefaultThetaX;
static float ThetaY = DefaultThetaY;
static int ViewSamples = GridWidth*2;
static int LightSamples = GridWidth;
static float Fips = -4;

PezConfig PezGetConfig()
{
    PezConfig config;
    config.Title = "Fluid3d";
    config.Width = 853;
    config.Height = 480;
    config.Multisampling = 0;
    config.VerticalSync = 0;
    return config;
}

void PezInitialize()
{
    PezConfig cfg = PezGetConfig();

    RaycastProgram = LoadProgram("Raycast.VS", "Raycast.GS", "Raycast.FS");
    LightProgram = LoadProgram("Fluid.Vertex", "Fluid.PickLayer", "Light.Cache");
    BlurProgram = LoadProgram("Fluid.Vertex", "Fluid.PickLayer", "Light.Blur");
    TextProgram = LoadProgram("Text.VS", "Text.GS", "Text.FS");
    Vbos.CubeCenter = CreatePointVbo(0, 0, 0);
    Vbos.FullscreenQuad = CreateQuadVbo();

    Slabs.Velocity = CreateSlab(GridWidth, GridHeight, GridDepth, 3);
    Slabs.Density = CreateSlab(GridWidth, GridHeight, GridDepth, 1);
    Slabs.Pressure = CreateSlab(GridWidth, GridHeight, GridDepth, 1);
    Slabs.Temperature = CreateSlab(GridWidth, GridHeight, GridDepth, 1);
    Surfaces.Divergence = CreateVolume(GridWidth, GridHeight, GridDepth, 3);
    Surfaces.LightCache = CreateVolume(GridWidth, GridHeight, GridDepth, 1);
    Surfaces.BlurredDensity = CreateVolume(GridWidth, GridHeight, GridDepth, 1);
    InitSlabOps();
    Surfaces.Obstacles = CreateVolume(GridWidth, GridHeight, GridDepth, 3);
    CreateObstacles(Surfaces.Obstacles);
    ClearSurface(Slabs.Temperature.Ping, AmbientTemperature);

    if (!SimulateFluid)
        ReadFromFile("Density96.dat", Slabs.Density.Ping);

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glEnableVertexAttribArray(SlotPosition);
}

void PezRender()
{
    PezConfig cfg = PezGetConfig();

    // Blur and brighten the density map:
    bool BlurAndBrighten = true;
    if (BlurAndBrighten) {
        glDisable(GL_BLEND);
        glBindFramebuffer(GL_FRAMEBUFFER, Surfaces.BlurredDensity.FboHandle);
        glViewport(0, 0, Slabs.Density.Ping.Width, Slabs.Density.Ping.Height);
        glBindBuffer(GL_ARRAY_BUFFER, Vbos.FullscreenQuad);
        glVertexAttribPointer(SlotPosition, 2, GL_SHORT, GL_FALSE, 2 * sizeof(short), 0);
        glBindTexture(GL_TEXTURE_3D, Slabs.Density.Ping.ColorTexture);
        glUseProgram(BlurProgram);
        SetUniform("DensityScale", 5.0f);
        SetUniform("StepSize", sqrtf(2.0) / float(ViewSamples));
        SetUniform("InverseSize", recipPerElem(Vector3(float(GridWidth), float(GridHeight), float(GridDepth))));
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, GridDepth);
    }

    // Generate the light cache:
    bool CacheLights = true;
    if (CacheLights) {
        glDisable(GL_BLEND);
        glBindFramebuffer(GL_FRAMEBUFFER, Surfaces.LightCache.FboHandle);
        glViewport(0, 0, Surfaces.LightCache.Width, Surfaces.LightCache.Height);
        glBindBuffer(GL_ARRAY_BUFFER, Vbos.FullscreenQuad);
        glVertexAttribPointer(SlotPosition, 2, GL_SHORT, GL_FALSE, 2 * sizeof(short), 0);
        glBindTexture(GL_TEXTURE_3D, Surfaces.BlurredDensity.ColorTexture);
        glUseProgram(LightProgram);
        SetUniform("LightStep", sqrtf(2.0) / float(LightSamples));
        SetUniform("LightSamples", LightSamples);
        SetUniform("InverseSize", recipPerElem(Vector3(float(GridWidth), float(GridHeight), float(GridDepth))));
        glDrawArraysInstanced(GL_TRIANGLE_STRIP, 0, 4, GridDepth);
    }

    // Perform raycasting:
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    glViewport(0, 0, cfg.Width, cfg.Height);
    glClearColor(0, 0, 0, 0);
    //glClearColor(1, 1, 1, 1);
    glClear(GL_COLOR_BUFFER_BIT);
    glEnable(GL_BLEND);
    glBindBuffer(GL_ARRAY_BUFFER, Vbos.CubeCenter);
    glVertexAttribPointer(SlotPosition, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), 0);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_3D, Surfaces.BlurredDensity.ColorTexture);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, Surfaces.LightCache.ColorTexture);
    glUseProgram(RaycastProgram);
    SetUniform("ModelviewProjection", Matrices.ModelviewProjection);
    SetUniform("Modelview", Matrices.Modelview);
    SetUniform("ViewMatrix", Matrices.View);
    SetUniform("ProjectionMatrix", Matrices.Projection);
    SetUniform("ViewSamples", ViewSamples);
    SetUniform("EyePosition", EyePosition);
    SetUniform("Density", 0);
    SetUniform("LightCache", 1);
    SetUniform("RayOrigin", Vector4(transpose(Matrices.Modelview) * EyePosition).getXYZ());
    SetUniform("FocalLength", 1.0f / std::tan(FieldOfView / 2));
    SetUniform("WindowSize", float(cfg.Width), float(cfg.Height));
    SetUniform("StepSize", sqrtf(2.0) / float(ViewSamples));
    glDrawArrays(GL_POINTS, 0, 1);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_3D, 0);
    glActiveTexture(GL_TEXTURE0);
}

void PezUpdate(float seconds)
{
    PezConfig cfg = PezGetConfig();

    float dt = seconds * 0.001f;
    Vector3 up(1, 0, 0); Point3 target(0);
    Matrices.View = Matrix4::lookAt(EyePosition, target, up);
    Matrix4 modelMatrix = Matrix4::identity();
    modelMatrix *= Matrix4::rotationX(ThetaX);
    modelMatrix *= Matrix4::rotationY(ThetaY);
    Matrices.Modelview = Matrices.View * modelMatrix;
    Matrices.Projection = Matrix4::perspective(
        FieldOfView,
        float(cfg.Width) / cfg.Height, // Aspect Ratio
        0.0f,   // Near Plane
        1.0f);  // Far Plane
    Matrices.ModelviewProjection = Matrices.Projection * Matrices.Modelview;

    float fips = 1.0f / dt;
    float alpha = 0.05f;
    if (Fips < 0) Fips++;
    else if (Fips == 0) Fips = fips;
    else  Fips = fips * alpha + Fips * (1.0f - alpha);

    if (SimulateFluid) {
        glBindBuffer(GL_ARRAY_BUFFER, Vbos.FullscreenQuad);
        glVertexAttribPointer(SlotPosition, 2, GL_SHORT, GL_FALSE, 2 * sizeof(short), 0);
        glViewport(0, 0, GridWidth, GridHeight);
        Advect(Slabs.Velocity.Ping, Slabs.Velocity.Ping, Surfaces.Obstacles, Slabs.Velocity.Pong, VelocityDissipation);
        SwapSurfaces(&Slabs.Velocity);
        Advect(Slabs.Velocity.Ping, Slabs.Temperature.Ping, Surfaces.Obstacles, Slabs.Temperature.Pong, TemperatureDissipation);
        SwapSurfaces(&Slabs.Temperature);
        Advect(Slabs.Velocity.Ping, Slabs.Density.Ping, Surfaces.Obstacles, Slabs.Density.Pong, DensityDissipation);
        SwapSurfaces(&Slabs.Density);
        ApplyBuoyancy(Slabs.Velocity.Ping, Slabs.Temperature.Ping, Slabs.Density.Ping, Slabs.Velocity.Pong);
        SwapSurfaces(&Slabs.Velocity);
        ApplyImpulse(Slabs.Temperature.Ping, ImpulsePosition, ImpulseTemperature);
        ApplyImpulse(Slabs.Density.Ping, ImpulsePosition, ImpulseDensity);
        ComputeDivergence(Slabs.Velocity.Ping, Surfaces.Obstacles, Surfaces.Divergence);
        ClearSurface(Slabs.Pressure.Ping, 0);
        for (int i = 0; i < NumJacobiIterations; ++i) {
            Jacobi(Slabs.Pressure.Ping, Surfaces.Divergence, Surfaces.Obstacles, Slabs.Pressure.Pong);
            SwapSurfaces(&Slabs.Pressure);
        }
        SubtractGradient(Slabs.Velocity.Ping, Slabs.Pressure.Ping, Surfaces.Obstacles, Slabs.Velocity.Pong);
        SwapSurfaces(&Slabs.Velocity);
    }
}

void PezHandleMouse(int x, int y, int action)
{
    static int StartX, StartY;
    static const float Speed = 0.005f;
    if (action & PEZ_DOWN) {
        StartX = x;
        StartY = y;
    } else if (action & PEZ_MOVE) {
        ThetaX = DefaultThetaX + Speed * (x - StartX);
        ThetaY = DefaultThetaY + Speed * (y - StartY);
    } else if (action & PEZ_DOUBLECLICK) {
        ThetaX = DefaultThetaX;
        ThetaY = DefaultThetaY;
    }
}

void PezHandleKey(char c)
{
    SimulateFluid = !SimulateFluid;
    //WriteToFile("Density96.dat", Slabs.Density.Ping);
}
