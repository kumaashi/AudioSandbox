#include <stdio.h>
#include <stdint.h>
#include <thread>
#include <vector>

#include <windows.h>
#include <mmreg.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "winmm.lib")

bool isActive = true;
void run_audio(void (*update_audio)(float *buf, int count, int ch))
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
				update_audio((float *)whdr[idx].lpData, whdr[idx].dwBufferLength / sizeof(float), 2);
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

void
update_audio(float *buf, int count, int ch)
{
	static double phase = 0;
	static int note = 0x20;
	static double dphase = pow(2, ((note * 1.0) / 12.0)) / 256.0f;
	for(int i = 0 ; i < count; i += ch) {
		auto value = 0.0f;
		auto ph = phase;
		for(int k = 0 ; k < 4; k++) {
			value += sin(ph);
			ph *= 1.51;
		}
		phase += dphase;

		buf[i + 0] = value * 0.02;
		buf[i + 1] = buf[i + 0];
	}
}

int main() {
	printf("WINMM basic sound start.\n");
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

