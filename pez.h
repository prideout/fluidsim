#pragma once

typedef struct PezConfigRec
{
    const char* Title;
    int Width;
    int Height;
    int Multisampling;
    int VerticalSync;
} PezConfig;

#ifdef __cplusplus
extern "C" {
#endif

// Implemented by the application code:
PezConfig PezGetConfig();
void PezInitialize();
void PezRender();
void PezUpdate(unsigned int microseconds);
void PezHandleMouse(int x, int y, int action);
void PezHandleKey(char c);

// Implemented by the platform layer:
const char* PezResourcePath();
void PezDebugString(const char* pStr, ...);
void PezDebugStringW(const wchar_t* pStr, ...);
void PezFatalError(const char* pStr, ...);
void PezFatalErrorW(const wchar_t* pStr, ...);
void PezCheckCondition(int condition, ...);
void PezCheckConditionW(int condition, ...);
const char* PezGetAssetsFolder();

#ifdef __cplusplus
}
#endif

// Various constants and macros:
#define TwoPi (6.28318531f)
#define Pi (3.14159265f)
#define countof(A) (sizeof(A) / sizeof(A[0]))
enum MouseFlag {
    PEZ_DOWN  = 1 << 0,
    PEZ_UP    = 1 << 1,
    PEZ_MOVE  = 1 << 2,
    PEZ_LEFT  = 1 << 3,
    PEZ_RIGHT = 1 << 4,
    PEZ_DOUBLECLICK = 1 << 5,
};
