#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdint.h>
#include <windows.h>
#include <vector>

#include <xaudio2.h>

#pragma comment(lib, "xaudio2.lib")
#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")

int main() {
	enum {
		Bits         = (sizeof(float) * 8),
		Channel      = (2),
		SampleRate   = (44100),
		Align        = ((Channel * Bits) / 8),
		BytePerSec   = (SampleRate * Align),
		MaxVoice     = 32,
	};
	WAVEFORMATEX wfx = { WAVE_FORMAT_IEEE_FLOAT, Channel, SampleRate, BytePerSec, Align, Bits, 0 };
	IXAudio2 *pXAudio2 = nullptr;
	IXAudio2MasteringVoice *pMasterVoice = nullptr;
	IXAudio2SourceVoice *vSourceVoices[MaxVoice] = {};

	CoInitializeEx( NULL, COINIT_MULTITHREADED ) ;
	XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	pXAudio2->CreateMasteringVoice(&pMasterVoice);
	for(int i = 0; i < MaxVoice; i++) {
		IXAudio2SourceVoice* pSourceVoice = nullptr;
		pXAudio2->CreateSourceVoice(&pSourceVoice, (WAVEFORMATEX *)&wfx, 0, XAUDIO2_MAX_FREQ_RATIO);
		vSourceVoices[i] = pSourceVoice;
	}
	std::vector<float> vWaveData;
	
	{
		auto frand = []() {
			return (float(rand()) / float(0x7FFF)) * 2.0f - 1.0f;
		};
		size_t SampleNumber = SampleRate;
		float Volume = 1.0f;
		float VolumeDecay = 1.0f / float(SampleNumber);
		float phase = 0.0;
		float base_freq = 440.0f;
		float phase_delta = base_freq / float(SampleRate);
		for(int i = 0 ; i < SampleNumber; i++) {
			phase += phase_delta;
			float data = 0.0f;

			//generate wave data.
			data += sin(phase * 2.0f * 3.141592f); //sine
			//data += (phase - floor(phase)) * 2.0f - 1.0f; //saw
			//data += (phase - floor(phase)) > 0.5f ? 1.0f : -1.0f; //square
			//data += (float(rand()) / float(0x7FFF)) * 2.0f - 1.0f; //noise
			data *= Volume * 0.1f;
			vWaveData.push_back(data); //L
			vWaveData.push_back(data); //R
			Volume -= VolumeDecay;
		}
	}

	printf("SOUND ON : SPACE KEY\nQUIT : ESCAPE KEY\n");
	while ((GetAsyncKeyState(VK_ESCAPE) & 0x8000) == 0) {
		if (GetAsyncKeyState(VK_SPACE) & 0x0001) {
			static int index = 0;
			auto ref = vSourceVoices[index++ % MaxVoice];
			XAUDIO2_BUFFER buffer = {};
			buffer.pAudioData = (const BYTE *)vWaveData.data();
			buffer.AudioBytes = static_cast<UINT32>(vWaveData.size() * sizeof(float));
			ref->Stop(0);
			ref->FlushSourceBuffers();
			ref->SubmitSourceBuffer(&buffer);
			ref->Start(0);
		}
		Sleep(16);
	}
	pXAudio2->Release();
	return 0;
}
