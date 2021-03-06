#include <stdio.h>
#include <stdint.h>
#include <thread>
#include <vector>

#include <windows.h>
#include <mmreg.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "winmm.lib")

bool isActive = true;
void run_audio(void (*cbfunc)(float *buf, int count, int ch))
{
	auto audio = std::thread([&] {
		const int BufNum = 6;
		const int Samples = 512;
		DWORD count  = 0;
		HWAVEOUT hwo = NULL;
		HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		WAVEFORMATEX wfx = {
			WAVE_FORMAT_IEEE_FLOAT, 2, 44100 / 2, 0, 0, 32, 0
		};
		
		wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
		wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
		std::vector<WAVEHDR> whdr(BufNum);
		std::vector< std::vector<char> > soundbuffer(BufNum);
		
		waveOutOpen(&hwo, WAVE_MAPPER, &wfx, (DWORD_PTR)hEvent, 0,
			CALLBACK_EVENT);
		for(int i = 0 ; i < BufNum; i++) {
			soundbuffer[i].resize(Samples * wfx.nBlockAlign);
			WAVEHDR temp = {
				&soundbuffer[i][0], Samples * wfx.nBlockAlign,
				0, 0, 0, 0, NULL, 0
			};
			whdr[i] = temp;
			waveOutPrepareHeader(hwo, &whdr[i], sizeof(WAVEHDR));
			waveOutWrite(hwo, &whdr[i], sizeof(WAVEHDR));
		}
		for(uint64_t count = 0; isActive; ) {
			auto idx = count % BufNum;
			if(WaitForSingleObject(hEvent, 100) == WAIT_TIMEOUT)
				continue;
			if(whdr[idx].dwFlags & WHDR_DONE) {
				cbfunc((float *)whdr[idx].lpData,
					whdr[idx].dwBufferLength / sizeof(float), 2);
				waveOutWrite(hwo, &whdr[idx], sizeof(WAVEHDR));
				count++;
			}
		}

		do {
			count = 0;
			for(int i = 0; i < BufNum; i++)
				count += (whdr[i].dwFlags & WHDR_DONE) ? 0 : 1;
			if(count) Sleep(50);
		} while(count);

		for(int i = 0 ; i < BufNum ; i++)
			waveOutUnprepareHeader(hwo, &whdr[i], sizeof(WAVEHDR));

		waveOutReset(hwo);
		waveOutClose(hwo);
		CloseHandle(hEvent);
	});
	audio.join();
}

uint32_t irand() {
	static uint32_t a = 1;
	static uint32_t b = 2345678;
	static uint32_t c = 358791258;
	a += b;
	b += c;
	c += a;
	return (a >> 16);
}

float frand() {
	return (float(irand()) / float(0xFFFF)) * 2.0 - 1.0;
}

struct bar {
	int tempo;
	int key;
	int chord;
	int type;
	int variation;
	bool enable_bass;
	int notes[2][16];
};

void
update_audio(float *buf, int count, int ch)
{
	struct delay {
		int idx = 0;
		double b0[0x2000];
		void update(double x) {
			b0[idx] = x;
			idx = (idx + 1) % (0x2000);
		}
		double get() {
			return b0[idx];
		}
	};
	struct buffer {
		double b0 = 0;
		double get(double x, double a) {
			x = x + (b0 - x) * a;
			b0 = x;
			return x;
		}
	};
	struct voice {
		uint32_t tone = 0;
		uint32_t env = 0;
		uint32_t count = 0;
		double phase, dphase, vol, dvol;
		void reset() {
			phase = dphase = vol = dvol = 0;
		}
		void noteoff(int note) {
			dvol = 0.99;
		}
		void noteon(int note, float dp = 0.0f) {
			dphase = pow(2, (double(note) / 12.0)) / 1234.0f;
			count = 0;
			vol = 1.0;
			dvol = 0.9995 + dp;
		}
		double get() {
			double value = 0;
			double dph = 1.0f;
			for(int i = 0 ; i < 2; i++) {
				double ph = dph * phase * 0.5;
				value += sin(2.0 * ph * 3.141592653 + 0.5) > 0.2 ? 1.0 : -1.0;
				value += (ph - floor(ph)) * 2.0 - 1.0;
				value += sin(2.0 * ph * 3.141592653 + 0.75);
				dph *= 2.02;
			}
			
			double kr = frand() * 0.002;
			phase += kr;
			phase += dphase;
			vol *= dvol;
			value *= vol;
			return value;
		}
	};
	bar bars[4] = {
		{
			40, 0x34,
			1, 0, 0, true,
			{
				{0x32, 0x34, 0x3B, 0x34,  0x32, 0x3B, 0x32, 0x34,  0x39, 0x34, 0x32, 0x3B,  0x34, 0x28, 0x26, 0x28},
				{0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00},
			}
		},
		{
			40, 0x32,
			0, 0, 1, true,
			{
				{0x32, 0x34, 0x3B, 0x34,  0x32, 0x3B, 0x32, 0x34,  0x39, 0x34, 0x32, 0x3B,  0x34, 0x2A, 0x28, 0x26},
				{0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00},
			}
		},
		{
			40, 0x30,
			0, 0, 0, true,
			{
				{0x32, 0x34, 0x3B, 0x34,  0x32, 0x3B, 0x32, 0x34,  0x39, 0x34, 0x32, 0x3B,  0x34, 0x28, 0x26, 0x28},
				{0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00},
			}
		},
		{
			40, 0x2F,
			0, 0, 1, true,
			{
				{0x30, 0x33, 0x3B, 0x33,  0x34, 0x3B, 0x36, 0x34,  0x39, 0x33, 0x34, 0x3B,  0x23, 0x27, 0x28, 0x2A},
				{0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00,  0x00, 0x00, 0x00, 0x00},
			}
		},
		
	};
	static int bar_index = 0;
	static int counter = 0;
	static int note_counter = 0;
	static int note_key = 0;

	static int notetable[4][4] = {
		{0, 5, 9, -24}, //major
		{0, 5, 8, -24}, //minor
		{0, 5, 7, -24}, //minor
		{0, 4, 7, -24}, //minor
	};
	static voice chord[4];
	static voice melo[2];
	static uint32_t pattern[2][4] = {
		{
			0x9249249A, //00800080
			0x9249249A, //80008008
			0x8C408C88, //8C008C08
			0x80000080, //80C080AA
		},
		{
			0x9249F49A,
			0x8C408C88,
			0x9249249A,
			0x8000008C,
		}
	};
	auto & b0 = bars[bar_index % 4];
	for(int i = 0 ; i < count; i += ch) {
		if(counter-- < 0) {
			counter = b0.tempo * 64;
			int index = 31 - (note_counter * 2);
			int oct = 30 - (note_counter * 2);
			//printf("index=%d\n", index);
			for(int ch = 0 ; ch < 4; ch++) {
				auto & vch = chord[ch];
				auto variation = b0.variation;
				uint32_t note = b0.key + notetable[b0.chord][ch];
				uint32_t pat = pattern[variation][ch];
				int isoct = pat &  (1 << oct);
				if(pat & (1 << index)) {
					note += (isoct ? 12 : 0);
					vch.noteon(note, 0.0002);
					printf("%02X ", note);
				}
			}
			for(int ch = 0 ; ch < 2; ch++) {
				auto & vch = melo[ch];
				uint32_t note = b0.notes[ch][note_counter];
				if(note) {
					vch.noteon(note + 12, 0.0002);
				}
			}
			printf("\n");
			note_counter = (note_counter + 1) % 16;
			if(note_counter == 0) {
				printf("-------------------------------------------------------------------------\n");
				bar_index++;
			}
		}
		auto value = 0.0f;
		for(auto & vch : chord) {
			value += vch.get();
		}
		for(auto & vch : melo) {
			value += vch.get();
		}
		

		static delay fake;
		double L = value + fake.get() * 0.3;
		double R = value - fake.get() * 0.3;
		fake.update(value);
		
		buf[i + 0] = L * 0.05;
		buf[i + 1] = R * 0.05;

	}
}

int main() {
	printf("WINMM chord sound start.\n");
	printf("Exit : ESCAPE\n");
	auto thread = std::thread([&]() {
		run_audio(update_audio);
	});
	while(!(GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
		Sleep(16);
	}
	isActive = false;
	thread.join();
	return 0;
}

