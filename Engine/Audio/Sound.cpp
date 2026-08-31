#include "Sound.h"


Sound::Sound() {
    CoInitializeEx(nullptr, COINIT_MULTITHREADED);  
    MFStartup(MF_VERSION, MFSTARTUP_NOSOCKET);      

    XAudio2Create(&xAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
    xAudio2->CreateMasteringVoice(&masterVoice);
    isInitialized = true;
}


Sound::~Sound() {
    xAudio2.Reset();
    MFShutdown();       // ← 追加
    CoUninitialize();   // ← 追加
}

SoundData Sound::SoundLoad(const char* filename)
{
    // 拡張子を取得
    std::string path(filename);
    std::string ext = path.substr(path.find_last_of('.'));

    if (ext == ".wav" || ext == ".WAV") {
        return SoundLoadWave(filename);
    }
    else if (ext == ".mp3" || ext == ".MP3") {
        return SoundLoadMP3(filename);
    }
    else {
        assert(false && "未対応のフォーマットです");
        return {};
    }
}

SoundData Sound::SoundLoadWave(
    const char* filename)
{

    //ファイルオープン処理

    std::ifstream file;
    file.open(filename, std::ios_base::binary);
    assert(file.is_open());

    //データ読み込み
    RiffHander riff;
    file.read((char*)&riff, sizeof(riff));

    //リーフかの確認
    if (strncmp(riff.chunk.id, "RIFF", 4) != 0) {
        assert(0);
    }
    //WAVEかの確認処理
    if (strncmp(riff.type, "WAVE", 4) != 0) {
        assert(0);
    }

    //フォーマット読み込み
    FormatChunk format = {};
    file.read((char*)&format, sizeof(ChunkHeader));
    if (strncmp(format.chunk.id, "fmt ", 4) != 0) {
        assert(0);
    }
    assert(format.chunk.size <= sizeof(format.fmt));
    file.read((char*)&format.fmt, format.chunk.size);

    ChunkHeader data;
    file.read((char*)&data, sizeof(data));

    //JUNKチャンクを検出した場合
    if (strncmp(data.id, "JUNK", 4) == 0) {
        //読み取り位置をJUNKチャンクの終わりまですすめる
        file.seekg(data.size, std::ios_base::cur);
        //再読み込み
        file.read((char*)&data, sizeof(data));
    }

    if (strncmp(data.id, "data", 4) != 0) {
        assert(0);
    }
    //データのチャンク部分の読み込み
    char* pBuffer = new char[data.size];
    file.read(pBuffer, data.size);
    //WAVEファイルを閉じる
    file.close();

    SoundData soundData = {};

    soundData.wfex = format.fmt;
    soundData.pBuffer = reinterpret_cast<BYTE*>(pBuffer);
    soundData.bufferSize = data.size;

    return soundData;

}


SoundData Sound::SoundLoadMP3(const char* filename)
{
    // wchar_t に変換
    wchar_t wFilename[256];
    MultiByteToWideChar(CP_ACP, 0, filename, -1, wFilename, 256);

    // SourceReaderを作成
    ComPtr<IMFSourceReader> pReader;
    MFCreateSourceReaderFromURL(wFilename, nullptr, &pReader);

    // 出力フォーマットをPCMに指定
    ComPtr<IMFMediaType> pType;
    MFCreateMediaType(&pType);
    pType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Audio);
    pType->SetGUID(MF_MT_SUBTYPE, MFAudioFormat_PCM);
    pReader->SetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, nullptr, pType.Get());

    // デコード後のフォーマット取得 → wfex に詰める
    ComPtr<IMFMediaType> pOutType;
    pReader->GetCurrentMediaType(MF_SOURCE_READER_FIRST_AUDIO_STREAM, &pOutType);

    WAVEFORMATEX* pwfex = nullptr;
    UINT32 wfexSize = 0;
    MFCreateWaveFormatExFromMFMediaType(pOutType.Get(), &pwfex, &wfexSize);

    // サンプルを全部読んでバッファに結合
    std::vector<BYTE> audioData;
    while (true) {
        DWORD dwFlags = 0;
        ComPtr<IMFSample> pSample;
        pReader->ReadSample(MF_SOURCE_READER_FIRST_AUDIO_STREAM,
            0, nullptr, &dwFlags, nullptr, &pSample);

        if (dwFlags & MF_SOURCE_READERF_ENDOFSTREAM) break;
        if (!pSample) continue;

        ComPtr<IMFMediaBuffer> pBuffer;
        pSample->ConvertToContiguousBuffer(&pBuffer);

        BYTE* pAudioData = nullptr;
        DWORD cbBuffer = 0;
        pBuffer->Lock(&pAudioData, nullptr, &cbBuffer);
        audioData.insert(audioData.end(), pAudioData, pAudioData + cbBuffer);
        pBuffer->Unlock();
    }

    // SoundDataに詰めて返す
    SoundData soundData = {};
    soundData.wfex = *pwfex;
    soundData.bufferSize = (UINT32)audioData.size();
    soundData.pBuffer = new BYTE[soundData.bufferSize];
    memcpy(soundData.pBuffer, audioData.data(), soundData.bufferSize);

    CoTaskMemFree(pwfex);
    return soundData;
}


void Sound::SoundPlayer(

    const SoundData& soundData)
{

    HRESULT result;
    IXAudio2SourceVoice* pSourceVoice = nullptr;
    result = xAudio2->CreateSourceVoice(
        &pSourceVoice, &soundData.wfex
    );
    assert(SUCCEEDED(result));

    XAUDIO2_BUFFER buf{};
    buf.pAudioData = soundData.pBuffer;
    buf.AudioBytes = soundData.bufferSize;
    buf.Flags = XAUDIO2_END_OF_STREAM;

    result = pSourceVoice->SubmitSourceBuffer(&buf);
    result = pSourceVoice->Start();
}

void Sound::SoundUnLoad(
    SoundData* soundData)
{

    delete[]soundData->pBuffer;
    soundData->pBuffer = 0;
    soundData->bufferSize = 0;
    soundData->wfex = {};

}
