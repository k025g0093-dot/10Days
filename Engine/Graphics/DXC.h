#pragma once
#include <Windows.h>
#include <dxcapi.h>
#include <string>
#pragma comment(lib,"dxcompiler.lib")

void DxcCompilerInclude(HRESULT& hr,
    IDxcUtils*& dxcUtils,
    IDxcCompiler3*& dxcCompiler,
    IDxcIncludeHandler*& includeHandler);

IDxcBlob* CompileShader(
    const std::wstring& filePath,
    const wchar_t* profile,
    IDxcUtils* dxcUtils,
    IDxcCompiler3* dxcCompiler,
    IDxcIncludeHandler* includeHandler);