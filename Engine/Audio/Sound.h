#pragma once
#include <xaudio2.h>
#include <xaudio2fx.h>
#pragma comment(lib,"xaudio2.lib")
#include <fstream>
#include <wrl.h>
#include <cassert>
#include <vector>

#include <mfapi.h>          // MFの基本機能
#include <mfidl.h>          // MFのインターフェース定義
#include <mfreadwrite.h>    // ソースリーダー（ファイル読み込み用）
#include <mferror.h>        // エラーコード定義

#pragma comment(lib, "mf.lib")
#pragma comment(lib, "mfplat.lib")
#pragma comment(lib, "mfuuid.lib")
#pragma comment(lib, "mfreadwrite.lib")

using Microsoft::WRL::ComPtr;

//ストラクト群
struct ChunkHeader
{
	char id[4];
	int32_t size;
};

struct RiffHander
{

	ChunkHeader chunk;
	char type[4];

};

struct FormatChunk
{
	ChunkHeader chunk;
	WAVEFORMATEX fmt;
};

struct SoundData
{
	WAVEFORMATEX wfex;
	BYTE* pBuffer;
	unsigned int bufferSize;
};

class Sound
{
public:

	Sound();
	~Sound();

	SoundData SoundLoad(const char* filename);

	void SoundUnLoad(SoundData* soundData);
	void SoundPlayer(
		const SoundData& soundData);
	ComPtr<IXAudio2>xAudio2;

private:

	SoundData SoundLoadWave(const char* filename);
	SoundData SoundLoadMP3(const char* filename);

	IXAudio2MasteringVoice* masterVoice = nullptr;;

	bool isInitialized = false;  // 初期化完了フラグ
	float masterVolume = 1.0f;

};

