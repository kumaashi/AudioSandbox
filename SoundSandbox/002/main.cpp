#include <stdio.h>
#include <vector>
#include <stdint.h>
#include <math.h>
#include <windows.h>
#include <process.h>
#include <mmsystem.h>
#include <mmreg.h>

#pragma comment(lib, "winmm.lib")
#pragma comment(lib, "user32.lib")
#pragma comment(lib, "ole32.lib")

class Sound {
	HANDLE hThread = nullptr;
	enum {
		SIGNAL_END,
		SIGNAL_TASK,
		SIGNAL_MAX,
	};
	HANDLE hEvent[SIGNAL_MAX] = {};
	static void Thread(void *ptr) {
		Sound *snd = (Sound *)ptr;
		snd->Start();
	}
public:
	int Bits         = (sizeof(float) * 8);
	int Channel      = (2);
	int Freq         = (44100);
	int Align        = ((Channel * Bits) / 8);
	int BytePerSec   = (Freq * Align);
	int BufNum       = 8;
	int Samples      = 512;
	Sound() {
		printf("%s : start\n", __FUNCTION__);
		for(int i = 0; i < SIGNAL_MAX; i++) {
			hEvent[i] = CreateEvent(NULL, FALSE, FALSE, NULL);
		}
		hThread = (HANDLE)_beginthread(Thread, 0, this);
	}
	virtual ~Sound() {
		Term();
	}
	virtual void Fill(void *dest, int samples) {}

	void Term() {
		SetEvent(hEvent[SIGNAL_END]);
		WaitForSingleObject(hThread, 5000); //max5s
		CloseHandle(hThread);
	}

	void Start() {
		DWORD count  = 0;
		HWAVEOUT hwo = NULL;
		WAVEFORMATEX wfx = {
			WAVE_FORMAT_IEEE_FLOAT, Channel, Freq, BytePerSec, Align, Bits, 0
		};
		std::vector<WAVEHDR> whdr;
		whdr.resize(BufNum);

		if(waveOutOpen(&hwo, WAVE_MAPPER, &wfx, (DWORD_PTR)hEvent[SIGNAL_TASK], 0, CALLBACK_EVENT)) {
			printf("Cant open audio.\n");
			return;
		}

		std::vector< std::vector<char> > soundbuffer;
		soundbuffer.resize(BufNum);
		for(int i = 0 ; i < BufNum; i++) {
			soundbuffer[i].resize(Samples * wfx.nBlockAlign);
			WAVEHDR temp = { &soundbuffer[i][0], Samples * wfx.nBlockAlign, 0, 0, 0, 0, NULL, 0 };
			whdr[i] = temp;
			waveOutPrepareHeader(hwo, &whdr[i], sizeof(WAVEHDR));
			waveOutWrite(hwo, &whdr[i], sizeof(WAVEHDR));
		}
		printf("%s : start.\n", __FUNCTION__);
		MSG msg;
		bool isDone = false;
		while(!isDone) {
			DWORD signal = WaitForMultipleObjects(SIGNAL_MAX, hEvent, FALSE, 5000);
			if(signal == WAIT_TIMEOUT) continue;
			if(signal == WAIT_OBJECT_0 + SIGNAL_END) {
				break;
			}
			if(whdr[count].dwFlags & WHDR_DONE) {
				Fill((void *)whdr[count].lpData, Samples);
				waveOutWrite(hwo, &whdr[count], sizeof(WAVEHDR));
				count = (count + 1) % BufNum;
			}
		}

		do {
			count = 0;
			for(int i = 0; i < BufNum; i++) {
				count += (whdr[i].dwFlags & WHDR_DONE) ? 0 : 1;
			}
			if(count) Sleep(50);
		} while(count);

		for(int i = 0 ; i < BufNum ; i++) {
			waveOutUnprepareHeader(hwo, &whdr[i], sizeof(WAVEHDR));
		}

		waveOutReset(hwo);
		waveOutClose(hwo);
		if(hEvent) CloseHandle(hEvent);
		printf("%s : end.\n", __FUNCTION__);
	}
};


namespace {
	struct Random {
		uint32_t a = 123456, b = 197864796, c = 128931293;
		Random() {
			Init();
		}
		void Init() {
			a = 123456, b = 197864796, c = 128931293;
		}
		uint32_t Get(uint32_t v = 0) {
			a += b + v;
			b += c;
			c += a;
			return (a >> 16);
		}
		float Getf() {
			return float(Get()) / float(0xFFFF);
		}
		float Getfn() {
			return Getf() * 2.0 - 1.0;
		}
	};

	struct DelayBuffer {
		enum {
			Max = 100000,
		};
		float Buf[Max] = {};
		unsigned long   Rate;
		unsigned int   Index;
		void Init(unsigned long m) {
			Rate = m;
		}
		void Update(float a) {
			Buf[Index++ % Rate ] = a;
		}
		float Sample(unsigned long n = 0) {
			return Buf[ (Index + n) % Rate];
		}
	};

	//http://www.ari-web.com/service/soft/reverb-2.htm
	struct Reverb {
		enum {
			CombMax = 4,
		};
		DelayBuffer comb[CombMax];
		float Sample(float a, int index = 0, int character = 0, int lpfnum = 4) {
			const int tau[][4][4] = {
				{
					{2063, 1847, 1523, 1277}, {3089, 2927, 2801, 2111}, {5479, 5077, 4987, 4057}, {9929, 7411, 4951, 1063},
				},
				{
					{2053, 1867, 1531, 1259}, {3109, 2939, 2803, 2113}, {5477, 5059, 4993, 4051}, {9949, 7393, 4957, 1097},
				}
			};

			const double gain[] = {
				-(0.8733), -(0.8223), -(0.8513), -(0.8503),
			};
			float D = a * 0.5;
			float E = 0;
			for(int i = 0 ; i < CombMax; i++) {
				DelayBuffer *reb = &comb[i];
				reb->Init(tau[character % 2][index % 4][i]);
				float k = 0;
				float c = 0;
				int LerpMax = lpfnum + 1;
				for(int h = 0 ; h < LerpMax; h++)
					k += reb->Sample(h * 2);
				k /= float(LerpMax);
				c = a + k;
				reb->Update(c * gain[i]);
				E += c;
			}
			D = (D + E) * 0.3;
			return D;
		}
	};

	float fract(float a) {
		return (float)(a - int(a));
	}

	struct Data {
		float value[2];
	};

	struct Envelope {
		float volume = 0.0;
		float volume_dt = 0.0;
		int state = 3;
		int count = 0;
		int env[3]  = {0x1000, 0x2000, 0x10000};
		double vvalue[3]  = {1.0, 0.0, -1.0};
		void Reset() {
			volume = 0.0;
			volume_dt = 0.0;
			state = -1;
			count = 0;
		}
		float Update() {
			if(state >= 3) return 0.0;
			volume += volume_dt;
			count--;
			if(count > 0) return volume;
			state++;
			volume_dt = vvalue[state] / double(env[state]);
			count = env[state];
			return volume;
		}
	};

	struct Voice {
		enum {
			MaxTap   = 3,
		};
		int count = 0;
		int pitch = 0;
		double phase = 0.0;
		double tm = 0.0;
		float delta = 0.0;
		float buf[2] = {0.0};
		float tap[2][MaxTap] = {};
		Envelope env;

		void On(int p) {
			count = 0;
			phase = 0.0;
			tm = 0.0;
			pitch = p;
			delta = pow(2.0, (1.0 / 12.0) * p) * 0.0004;
			env.Reset();
		}

		Data Get() {
			count++;
			auto volume = env.Update();
			const double pi = 3.14159265358f;
			phase += delta;
			float value = 0;
			auto piphase = phase * pi;
			float oct = 0.125;

			value += sin(oct * piphase * 0.999 + 0.00) > 0.0 ? -1.0 : 1.0;
			value += sin(oct * piphase * 1.001 + 0.50) > 0.0 ? -1.0 : 1.0;
			value += sin(oct * piphase * 0.500 + 0.25) > 0.0 ? -1.0 : 1.0;

			//value += fract(oct * phase * 0.999 + 0.00) * 2.0 - 1.0;
			//value += fract(oct * phase * 1.001 + 0.50) * 2.0 - 1.0;
			//value += fract(oct * phase * 0.500 + 0.25) * 2.0 - 1.0;

			//noise
			static uint32_t a = 123456, b = 197864796, c = 128931293;
			a += b; b += c; c += a;
			float noise = (float((a >> 16)) / float(0xFFFF)) * 2.0 - 1.0;
			value += noise * 0.1;
			
			//fake stereo
			float fm = 2.5;
			float ret[2] = {
				sin(value * fm * 1.0) * volume,
				sin(value * fm * 1.5) * volume,
			};
			
			//cheap lpf
			static float mult[MaxTap] = {
				0.9, -0.4, 0.1
			};
			for(int i = 0; i < MaxTap; i++) {
				for(int ch = 0; ch < 2; ch++) {
					auto temp = ret[ch];
					temp = temp + (tap[ch][i] - temp) * mult[i];
					tap[ch][i] = temp;
					ret[ch] = temp;
				}
			}
			Data data = {ret[0], ret[1]};
			return data;
		}
	};

	class TestSound : public Sound {
		enum {
			MaxVoice = 8,
		};
		Voice voice[MaxVoice];
	public:
		void On(int ch, int pitch) {
			voice[ch % MaxVoice].On(pitch);
		}
		TestSound() {
		}
		virtual void Fill(void *dest, int samples) {
			float *buf = (float *)dest;
			for(int index = 0 ; index < samples; index++) {
				float vsL = 0.0;
				float vsR = 0.0;
				for(int i = 0; i < MaxVoice; i++) {
					auto data = voice[i].Get();
					vsL += data.value[0];
					vsR += data.value[1];
				}
				buf[index * 2 + 0] = vsL;
				buf[index * 2 + 1] = vsR;
			}
		}
	};
}

#define GetKey(x) (GetAsyncKeyState(x) & 0x0001)
int main() {
	{
		auto sound = new TestSound;
		int ch = 0;
		int kbase = 0;
		printf("Press : ZXCVBNMK\n");
		while( !GetKey(VK_ESCAPE) ) {
			int base = 80 + kbase;
			if(GetKey(VK_UP)) kbase += 1;
			if(GetKey(VK_DOWN)) kbase -= 1;
			if(GetKey('Z')) sound->On(ch++, base + 0);
			if(GetKey('X')) sound->On(ch++, base + 2);
			if(GetKey('C')) sound->On(ch++, base + 4);
			if(GetKey('V')) sound->On(ch++, base + 5);
			if(GetKey('B')) sound->On(ch++, base + 7);
			if(GetKey('N')) sound->On(ch++, base + 9);
			if(GetKey('M')) sound->On(ch++, base + 11);
			if(GetKey('K')) sound->On(ch++, base + 12);
			Sleep(16);
		}
		delete sound;
	}
}


