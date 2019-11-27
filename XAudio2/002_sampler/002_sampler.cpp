#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>
#include <vector>
#include <algorithm>

#include <xaudio2.h>
#include <xaudio2fx.h>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")

int main(int argc, char *argv[]) {
	std::vector<uint8_t> vdata;
	auto fp = fopen("samples", "rb");
	if(fp) {
		fseek(fp, 0, SEEK_END);
		auto size = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		vdata.resize(size);
		fread(vdata.data(), 1, vdata.size(), fp);
		fclose(fp);
	}
	enum {
		MaxVoice = 32,
		MaxPoly = 8,
	};
	IXAudio2 *pXAudio2 = nullptr;
	IXAudio2MasteringVoice *pMasterVoice = nullptr;
	IXAudio2SourceVoice *vSourceVoices[MaxVoice][MaxPoly] = {};
	std::vector<XAUDIO2_BUFFER> vVoiceBuffer;
	std::vector<float> vWaveData;

	CoInitializeEx( NULL, COINIT_MULTITHREADED ) ;
	XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	pXAudio2->CreateMasteringVoice(&pMasterVoice);

	{
		int index = 0;
		uint8_t *source = vdata.data();
		uint8_t *endptr = source + vdata.size();
		uint8_t *cur = source;
		bool done = false;

		while(index < MaxVoice && !done) {
			while(*(uint32_t *)cur != FOURCC_RIFF) {
				if(cur >= endptr) {
					done = true;
					break;
				}
				cur++;
			}
			if (done) break;
			WAVEFORMATEX *lpwfx = (WAVEFORMATEX *)&cur[20];
			LPVOID data = (LPVOID)&cur[44];
			DWORD size = *(DWORD *)&cur[40];
			printf("ptr=%p, size=%zd\n", data, size);
			for (int poly = 0; poly < MaxPoly; poly++) {
				IXAudio2SourceVoice* pSourceVoice = nullptr;
				pXAudio2->CreateSourceVoice(&pSourceVoice, lpwfx, 0, XAUDIO2_MAX_FREQ_RATIO);
				vSourceVoices[index][poly] = pSourceVoice;
			}

			XAUDIO2_BUFFER buffer = {};
			buffer.pAudioData = (const BYTE *)data;
			buffer.AudioBytes = size;
			vVoiceBuffer.push_back(buffer);
			index++;
			cur += size;
		}
	}

	auto Play = [&](uint32_t index, uint32_t pitch) {
		static uint32_t polyidx = 0;
		auto ref = vSourceVoices[index % MaxVoice][polyidx++ % MaxPoly];
		auto ref_buffer = &vVoiceBuffer[index % MaxVoice];
		if(!ref) return;
		XAUDIO2_VOICE_STATE voice_state;
		ref->GetState(&voice_state);
		printf("voice_state.BuffersQueued=%d\n", voice_state.BuffersQueued);
		printf("voice_state.pCurrentBufferContext=%p\n", voice_state.pCurrentBufferContext);
		printf("voice_state.SamplesPlayed=%p\n", voice_state.SamplesPlayed);
		ref->Stop(0);
		ref->FlushSourceBuffers();
		ref->SubmitSourceBuffer(ref_buffer);
		ref->SetFrequencyRatio(pow(2.0f, float(pitch) / 12.0f) * 0.125f);
		ref->Start(0);
	};

	printf("SOUND ON : A S D F Keys\n");

	auto base = 0x20;
	auto inst = 0;
	while((GetAsyncKeyState(VK_ESCAPE) & 0x8000) == 0) {
		if( GetAsyncKeyState('A') & 0x0001) Play(inst, base + 0 - 12);
		if( GetAsyncKeyState('S') & 0x0001) Play(inst, base + 4);
		if( GetAsyncKeyState('D') & 0x0001) Play(inst, base + 7);
		if( GetAsyncKeyState('F') & 0x0001) Play(inst, base + 11);
		if( GetAsyncKeyState(VK_UP) & 0x0001) base += 1;
		if (GetAsyncKeyState(VK_DOWN) & 0x0001) base -= 1;
		if (GetAsyncKeyState(VK_LEFT) & 0x0001) inst -= 1;
		if (GetAsyncKeyState(VK_RIGHT) & 0x0001) inst += 1;
		inst = (std::max)(inst, 0);
		inst = (std::min)(inst, (int)vVoiceBuffer.size() - 1);

		Sleep(16);
	}
	pMasterVoice->DestroyVoice();
	pXAudio2->Release();
	CoUninitialize();
	return 0;
}
