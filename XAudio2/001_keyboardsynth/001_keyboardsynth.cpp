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
	std::vector<float> vWaveData;

	CoInitializeEx( NULL, COINIT_MULTITHREADED ) ;
	XAudio2Create(&pXAudio2, 0, XAUDIO2_DEFAULT_PROCESSOR);
	pXAudio2->CreateMasteringVoice(&pMasterVoice);
	for(int i = 0; i < MaxVoice; i++) {
		IXAudio2SourceVoice* pSourceVoice = nullptr;
		pXAudio2->CreateSourceVoice(&pSourceVoice, (WAVEFORMATEX *)&wfx, 0, XAUDIO2_MAX_FREQ_RATIO);
		vSourceVoices[i] = pSourceVoice;
	}
	
	auto Reset = [&]() {
		for(auto & ref : vSourceVoices) {
			ref->Stop(0);
			ref->FlushSourceBuffers();
		}
	};

	auto Play = [&](int pitch) {
		static int index = 0;
		auto ref = vSourceVoices[index++ % MaxVoice];
		XAUDIO2_BUFFER buffer = {};
		buffer.pAudioData = (const BYTE *)vWaveData.data();
		buffer.AudioBytes = static_cast<UINT32>(vWaveData.size() * sizeof(float));

		ref->Stop(0);
		ref->FlushSourceBuffers();
		ref->SubmitSourceBuffer(&buffer);
		ref->SetFrequencyRatio(pow(2.0f, float(pitch) / 12.0f) * 0.125f);
		ref->Start(0);
	};
	struct Envelope {
		int index = 0;
		int count = 0;
		float volume = 0.0f;
		float delta = 0.0f;
		uint32_t adsr[4] = {};
		int GetLength() {
			int ret = 0;
			for(int i = 0 ;  i < 4; i++) {
				ret += adsr[i];
			}
			return ret;
		}

		void Reset(uint32_t d) {
			index = 0;
			count = 0;
			volume = 0.0f;
			adsr[0] = ((d & 0xFF000000) >> 24) << 8;
			adsr[1] = ((d & 0x00FF0000) >> 16) << 8;
			adsr[2] = ((d & 0x0000FF00) >>  8) << 8;
			adsr[3] = ((d & 0x000000FF) >>  0) << 8;
		}

		float GetVolume() {
			count--;
			if(count <= 0 && index < 4) {
				count = adsr[index];
				const float fadsr[4] = {1.0f, -0.5f, -0.0f, -0.5f};
				delta = fadsr[index] / float(count);
				index++;
			}
			volume += delta;
			return volume;
		}
	};
	auto UpdateWaveData = [&](uint16_t seed = 0x7FFF) {
		auto frand = []() {
			return (float(rand()) / float(0x7FFF)) * 2.0f - 1.0f;
		};
		auto triangle = [](float x, float d = 0.5f) {
			x += d;
			return (x - floor(x)) * 2.0f - 1.0f;
		};
		auto square = [&](float x, float d = 0.5f) {
			return triangle(x) > d ? 1.0f : -1.0f;
		};

		Reset();
		vWaveData.clear();
		Envelope env;
		auto xor32 = []() -> uint32_t {
			static uint32_t y = 2463534242;
			y = y ^ (y << 13); y = y ^ (y >> 17);
			return y = y ^ (y << 5);
		};

		env.Reset(xor32());
		int detune = ((seed & 0x0300) >>  8);
		int oct    = ((seed & 0x0C00) >> 10) + 1;
		int noize  = ((seed & 0xF000) >> 12);
		printf("oct = %d, detune = %d, noize = %d, a=%d, d=%d, s=%d, r=%d\n",
			oct, detune, noize, env.adsr[0], env.adsr[1], env.adsr[2], env.adsr[3]);
		
		struct Lfo {
			float phase = 0.0f;
			float Get() {
				return sin((phase += 0.0001) * 3.141592653 * 2.0);
			}
		};
		Lfo lfo;
	
		size_t SampleNumber = env.GetLength();
		
		float phase = 0.0;
		float Lv = 1.0 + frand() * 0.5f;
		float Rv = 1.0 + frand() * 0.5f;
		float buffer = 0.0f;
		for(int i = 0 ; i < SampleNumber; i++) {
			auto lfo_value = lfo.Get();
			float base_freq = 110.0f;
			base_freq += frand() * 2.0f * 3.141592f * float(noize);
			float phase_delta = base_freq / float(SampleRate);
			phase += phase_delta;
			//float data = triangle(phase * 2.0f * 3.141592f);
			float data = triangle(phase) + triangle(phase + 0.5);

			float temp_phase = phase;
			for(int i = 0; i < oct; i++) {
				float detfact = 1.0;
				for(int di = 0; di < detune; di++) {
					/*
					data += ((seed & (1 << 0)) >> 0) * square(temp_phase * detfact, 0.25f * detfact);
					data += ((seed & (1 << 1)) >> 1) * triangle(temp_phase * detfact, 0.75f * detfact);
					data += ((seed & (1 << 2)) >> 2) * sin(temp_phase * 2.0f * 3.141592f * detfact);
					*/
					data += triangle(temp_phase * detfact, 0.75f * detfact);
					detfact *= 1.015f;
				}
				temp_phase *= 2.01f;
			}
			auto vol = env.GetVolume();
			data *= vol * vol;
			buffer = buffer + (data - buffer) * (0.333333 + lfo_value * 0.1);
			vWaveData.push_back(buffer * Lv); //L
			vWaveData.push_back(buffer * Rv); //R
		}
		auto maxvalue = *std::max_element(vWaveData.begin(), vWaveData.end());
		std::for_each(vWaveData.begin(), vWaveData.end(), [&](auto & x) {
			x /= maxvalue;
		});
	};

	UpdateWaveData();
	printf("SOUND ON : A S D F Keys\nCHANGE SOUND TONE PRESET : SPACE KEY\nQUIT : ESCAPE KEY\n");

	auto base = 0x20;
	while((GetAsyncKeyState(VK_ESCAPE) & 0x8000) == 0) {
		if( GetAsyncKeyState('A') & 0x0001) Play(base + 0 - 12);
		if( GetAsyncKeyState('S') & 0x0001) Play(base + 4);
		if( GetAsyncKeyState('D') & 0x0001) Play(base + 7);
		if( GetAsyncKeyState('F') & 0x0001) Play(base + 11);
		if( GetAsyncKeyState(VK_UP) & 0x0001) base += 1;
		if( GetAsyncKeyState(VK_DOWN) & 0x0001) base -= 1;
		if( GetAsyncKeyState(VK_SPACE) & 0x0001) {
			auto presetNum = rand();
			printf("Set Preset Num = %08X\n", presetNum);
			UpdateWaveData(presetNum);
		}
		Sleep(16);
	}
	pMasterVoice->DestroyVoice();
	pXAudio2->Release();
	CoUninitialize();
	return 0;
}
