#include "precompiled.h"
#include "libGLESv2/renderer/d3d/HLSLCompiler.h"
#include "libGLESv2/Program.h"
#include "libGLESv2/main.h"

#include "common/utilities.h"

#include "third_party/trace_event/trace_event.h"

namespace rx
{

HLSLCompiler::HLSLCompiler()
    : mD3DCompilerModule(NULL),
      mD3DCompileFunc(NULL),
	  mHasCompiler(true)
{
}

HLSLCompiler::~HLSLCompiler()
{
    release();
}

bool HLSLCompiler::initialize()
{
    TRACE_EVENT0("gpu", "initializeCompiler");
#if defined(ANGLE_PLATFORM_WP8)
    mD3DCompilerModule = NULL;
    mD3DCompileFunc = NULL;
    mHasCompiler = false;
    return true;
#endif //#if defined(ANGLE_PLATFORM_WP8)

#if defined(ANGLE_PLATFORM_WINRT) && (_MSC_VER >= 1800) && !defined(ANGLE_PLATFORM_WP8)
    mD3DCompilerModule = NULL;
    mD3DCompileFunc = reinterpret_cast<CompileFuncPtr>(D3DCompile);
    mHasCompiler = true;
    return true;
#endif // #if defined(ANGLE_PLATFORM_WINRT) && (_MSC_VER >= 1800)

#if defined(ANGLE_PRELOADED_D3DCOMPILER_MODULE_NAMES)
    // Find a D3DCompiler module that had already been loaded based on a predefined list of versions.
    static const char *d3dCompilerNames[] = ANGLE_PRELOADED_D3DCOMPILER_MODULE_NAMES;

    for (size_t i = 0; i < ArraySize(d3dCompilerNames); ++i)
    {
        if (GetModuleHandleExA(0, d3dCompilerNames[i], &mD3DCompilerModule))
        {
            break;
        }
    }
#endif  // ANGLE_PRELOADED_D3DCOMPILER_MODULE_NAMES

    if (!mD3DCompilerModule)
    {
        // Load the version of the D3DCompiler DLL associated with the Direct3D version ANGLE was built with.
#if defined(ANGLE_PLATFORM_WINRT)
#if WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
        mD3DCompilerModule = LoadLibrary(D3DCOMPILER_DLL);
#elif !defined(ANGLE_PLATFORM_WP8)
        mD3DCompilerModule = LoadPackagedLibrary(D3DCOMPILER_DLL, 0);
#endif //WINAPI_FAMILY_PARTITION(WINAPI_PARTITION_DESKTOP)
    }
#else
	    mD3DCompilerModule = LoadLibrary(D3DCOMPILER_DLL);
    }
#endif // defined(ANGLE_PLATFORM_WINRT)

    if (!mD3DCompilerModule)
    {
        ERR("No D3D compiler module found - aborting!\n");
		mHasCompiler = false;
        return false;
    }

    mD3DCompileFunc = reinterpret_cast<CompileFuncPtr>(GetProcAddress(mD3DCompilerModule, "D3DCompile"));
    ASSERT(mD3DCompileFunc);

    return mD3DCompileFunc != NULL;
}

void HLSLCompiler::release()
{
    if (mD3DCompilerModule)
    {
        FreeLibrary(mD3DCompilerModule);
        mD3DCompilerModule = NULL;
        mD3DCompileFunc = NULL;
    }
}

#if !defined(ANGLE_PLATFORM_WP8)
ShaderBlob *HLSLCompiler::compileToBinary(gl::InfoLog &infoLog, const char *hlsl, const char *profile,
                                          const UINT optimizationFlags[], const char *flagNames[], int attempts) const
{
#if !defined(ANGLE_PLATFORM_WINRT)
    ASSERT(mD3DCompilerModule && mD3DCompileFunc);
#endif

    if (!hlsl)
    {
        return NULL;
    }

    pD3DCompile compileFunc = reinterpret_cast<pD3DCompile>(mD3DCompileFunc);
    for (int i = 0; i < attempts; ++i)
    {
        ID3DBlob *errorMessage = NULL;
        ID3DBlob *binary = NULL;

        HRESULT result = compileFunc(hlsl, strlen(hlsl), gl::g_fakepath, NULL, NULL, "main", profile, optimizationFlags[i], 0, &binary, &errorMessage);

        if (errorMessage)
        {
            const char *message = (const char*)errorMessage->GetBufferPointer();

            infoLog.appendSanitized(message);
            TRACE("\n%s", hlsl);
            TRACE("\n%s", message);

            SafeRelease(errorMessage);
        }

        if (SUCCEEDED(result))
        {
            return (ShaderBlob*)binary;
        }
        else
        {
            if (result == E_OUTOFMEMORY)
            {
                return gl::error(GL_OUT_OF_MEMORY, (ShaderBlob*)NULL);
            }

            infoLog.append("Warning: D3D shader compilation failed with %s flags.", flagNames[i]);

            if (i + 1 < attempts)
            {
                infoLog.append(" Retrying with %s.\n", flagNames[i + 1]);
            }
        }
    }

    return NULL;
}
#endif // #if !defined(ANGLE_PLATFORM_WP8)

}
