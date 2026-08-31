#include "DXC.h"
#include <cassert>
#include "LogSistem.h"
#include "ConvertString.h"
#include <format>

// DXCコンパイラの初期化
// シェーダーのコンパイルに必要なオブジェクトを生成する
void DxcCompilerInclude(HRESULT& hr,
    IDxcUtils*& dxcUtils,
    IDxcCompiler3*& dxcCompiler,
    IDxcIncludeHandler*& includeHandler) {

    // DXCユーティリティの生成（ファイル読み込みなどに使用）
    dxcUtils = nullptr;
    dxcCompiler = nullptr;
    hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
    assert(SUCCEEDED(hr));

    // DXCコンパイラの生成
    hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
    assert(SUCCEEDED(hr));

    // インクルードハンドラの生成（シェーダー内の#includeを処理する）
    includeHandler = nullptr;
    hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
    assert(SUCCEEDED(hr));
}

// シェーダーのコンパイル
// HLSLファイルを読み込んでGPUが実行できる形式に変換する
IDxcBlob* CompileShader(
    const std::wstring& filePath,
    const wchar_t* profile,
    IDxcUtils* dxcUtils,
    IDxcCompiler3* dxcCompiler,
    IDxcIncludeHandler* includeHandler) {

    Log(logStream, ConvertString(std::format(L"Begin CompileShader, path:{}, profile:{}\n", filePath, profile)));

    // シェーダーファイルの読み込み
    IDxcBlobEncoding* shaderSource = nullptr;
    HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
    assert(SUCCEEDED(hr));

    // 読み込んだファイルの情報を設定
    DxcBuffer shaderSourceBuffer{};
    shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
    shaderSourceBuffer.Size = shaderSource->GetBufferSize();
    shaderSourceBuffer.Encoding = DXC_CP_UTF8;

    // コンパイルオプションの設定
    LPCWSTR arguments[] = {
        L"-E", L"main",
        L"-T", profile,
        L"-I", L"shaders",        // ★ これを追加
        L"-Zi", L"-Qembed_debug", L"-Od", L"-Zpr"
    };

    // シェーダーのコンパイル実行
    IDxcResult* shaderResult = nullptr;
    hr = dxcCompiler->Compile(
        &shaderSourceBuffer,
        arguments,
        _countof(arguments),
        includeHandler,
        IID_PPV_ARGS(&shaderResult));
    assert(SUCCEEDED(hr));

    // コンパイルエラーの確認
    IDxcBlobUtf8* shaderError = nullptr;
    shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
    if (shaderError != nullptr && shaderError->GetStringLength() != 0) {
        Log(logStream, shaderError->GetStringPointer());
        assert(false);
    }

    // コンパイル済みシェーダーバイナリの取得
    IDxcBlob* shaderBlob = nullptr;
    hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
    assert(SUCCEEDED(hr));

    Log(logStream, ConvertString(std::format(L"Compile Succeeded, path:{}, profile:{}\n", filePath, profile)));

    
    return shaderBlob;
}