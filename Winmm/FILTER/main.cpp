#include <stdio.h>
#include <stdint.h>
#include <thread>
#include <vector>
#include <algorithm>

#include <windows.h>
#include <mmreg.h>

#pragma comment(lib, "user32.lib")
#pragma comment(lib, "winmm.lib")

void
run_audio(
	DWORD sample_rate,
	void (*cbfunc)(float *buf, int count, int ch),
	bool (*is_active)())
{
	auto audio = std::thread([&] {
		const int BufNum = 6;
		const int Samples = 1024;
		DWORD count  = 0;
		HWAVEOUT hwo = NULL;
		HANDLE hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
		WAVEFORMATEX wfx = {
			WAVE_FORMAT_IEEE_FLOAT, 2, sample_rate, 0, 0, 32, 0
		};

		wfx.nBlockAlign = (wfx.nChannels * wfx.wBitsPerSample) / 8;
		wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;
		std::vector<WAVEHDR> whdr(BufNum);
		std::vector< std::vector<char> > soundbuffer(BufNum);

		waveOutOpen(&hwo, WAVE_MAPPER, &wfx, (DWORD_PTR)hEvent, 0,
			CALLBACK_EVENT);
		for (int i = 0 ; i < BufNum; i++)
		{
			soundbuffer[i].resize(Samples * wfx.nBlockAlign);
			WAVEHDR temp = {
				&soundbuffer[i][0], Samples * wfx.nBlockAlign,
				0, 0, 0, 0, NULL, 0
			};
			whdr[i] = temp;
			waveOutPrepareHeader(hwo, &whdr[i], sizeof(WAVEHDR));
			waveOutWrite(hwo, &whdr[i], sizeof(WAVEHDR));
		}
		for (uint64_t count = 0; is_active(); )
		{
			if (WaitForSingleObject(hEvent, INFINITE) == WAIT_TIMEOUT) {
				continue;
			}
			for (int q = 0; q < BufNum; q++) {
				auto & hdr = whdr[q];
				if (!(hdr.dwFlags & WHDR_DONE))
					continue;
				cbfunc((float *)hdr.lpData,
					hdr.dwBufferLength / sizeof(float), 2);
				waveOutWrite(hwo, &whdr[q], sizeof(WAVEHDR));
			}
		}

		do
		{
			count = 0;
			for (int i = 0; i < BufNum; i++)
				count += (whdr[i].dwFlags & WHDR_DONE) ? 0 : 1;
			if (count) Sleep(50);
		} while (count);

		for (int i = 0 ; i < BufNum ; i++)
			waveOutUnprepareHeader(hwo, &whdr[i], sizeof(WAVEHDR));

		waveOutReset(hwo);
		waveOutClose(hwo);
		CloseHandle(hEvent);
	});
	audio.join();
}

uint32_t
irand()
{
	static uint32_t a = 1;
	static uint32_t b = 2345678;
	static uint32_t c = 358791258;
	a += b;
	b += c;
	c += a;
	return (a >> 16);
}

float
frand()
{
	return (float(irand()) / float(0xFFFF)) * 2.0 - 1.0;
}


enum {
	FILTER_LPF = 0,
	FILTER_HPF,
	FILTER_PEAKING,
	FILTER_MAX,
};

struct filter_param {
	double b0, a0;
	double b1, a1;
	double b2, a2;
	double b3, a3;
};

static std::vector<filter_param> filter_param_tbl[FILTER_MAX];

void
filter_init(double sample_rate, const int freq_num)
{
	double PI = 3.14159265358979323846;
	for (int i = 0 ; i < freq_num; i++) {
		filter_param param {};
		double fc = double(i);
		double fa = 1.0 / (2.0 * PI) * tan(PI * fc / sample_rate);
		double pfc = 2.0 * PI * fa;
		double tmp = (1.0 + 2.0 * pfc + 2.0 * pfc * pfc + pfc * pfc * pfc);

		param.b0 = 1.0;
		param.b1 = (-3.0 - 2.0 * pfc + 2.0 * pfc * pfc + 3.0 * pfc * pfc * pfc) / tmp;
		param.b2 = (3.0 - 2.0 * pfc - 2.0 * pfc * pfc + 3.0 * pfc * pfc * pfc) / tmp;
		param.b3 = (-1.0 + 2.0 * pfc - 2.0 * pfc * pfc + pfc * pfc * pfc) / tmp;

		param.a0 = (pfc * pfc * pfc) / tmp;
		param.a1 = 3.0 * (pfc * pfc * pfc) / tmp;
		param.a2 = 3.0 * (pfc * pfc * pfc) / tmp;
		param.a3 = (pfc * pfc * pfc) / tmp;
		filter_param_tbl[FILTER_LPF].push_back(param);

		param.a0 = 1.0 / tmp;
		param.a1 = -3.0 / tmp;
		param.a2 = 3.0 / tmp;
		param.a3 = -1.0 / tmp;
		filter_param_tbl[FILTER_HPF].push_back(param);
		{
			double A, omega, sinval, cosval, alpha;

			filter_param param {};
			float Q = 2.0;
			A = pow(10.0, (15.0 / 40));
			omega = 2.0 * PI * fc / sample_rate;
			sinval = sin(omega);
			cosval = cos(omega);
			alpha = sinval / (2.0 * Q);

			param.a0 = 1 + alpha * A;
			param.a1 = -2.0 * cosval;
			param.a2 = 1 - alpha * A;
			param.b0 = 1 + alpha / A;
			param.b1 = -2.0 * cosval;
			param.b2 = 1 - alpha / A;

			auto inv_b = 1.0 / param.b0;

			param.b0 *= inv_b;
			param.b1 *= inv_b;
			param.b2 *= inv_b;
			filter_param_tbl[FILTER_PEAKING].push_back(param);
		}
	}
}

double
filter_sample(double v, int fc, int type, double *z)
{
	auto filter = filter_param_tbl[type][fc];
	auto ret =
		v * filter.a0 + z[0] * filter.a1 + z[1] * filter.a2 +
		z[2] * filter.a3 - z[3] * filter.b1 - z[4] * filter.b2 - z[5] * filter.b3;
	z[2] = z[1];
	z[1] = z[0];
	z[0] = v;
	z[5] = z[4];
	z[4] = z[3];
	z[3] = ret;
	return ret;
}

struct delay {
	int idx = 0;
	double b0[0x2000];
	void update(double x)
	{
		b0[idx] = x;
		idx = (idx + 1) % (0x2000);
	}
	double get()
	{
		return b0[idx];
	}
};

struct envelope {
	int state = -1;
	int32_t count = 0;
	int32_t count_all = 0;
	uint32_t param = 0;
	uint32_t param_buffer = 0;
	double value = 0.0;
	double dvalue = 0.0;
	void reset(uint32_t p)
	{
		param = p;
		param_buffer = param;
		count = 0;
		value = 0;
		state = -1;
		count_all = 0;
	}
	void off()
	{
		double remain = 1.0 - value;
		count = 1024;
		dvalue = remain / float(count);
		state = 4;
	}
	int32_t get_count()
	{
		return count_all;
	}
	double update()
	{
		count_all++;
		value += dvalue;
		count--;
		if (count <= 0) {
			double gain[4] = { 1.0, -0.5, 0.0, -0.5, };
			dvalue = 0;
			if (param_buffer == 0) {
				value = 0;
				return value;
			}
			state++;
			dvalue = 0;
			count = ((0xFF000000 & param_buffer) >> 24) * 256;
			param_buffer <<= 8;
			dvalue = gain[state] / double(count);
		}
		return value;
	}
};

struct voice {
	uint32_t tone = 0;
	uint64_t count = 0;
	double phase, dphase;
	envelope env;
	double z0[8];
	double z1[8];

	void reset()
	{
		phase = dphase = 0;
	}

	void noteoff(int note)
	{
		env.off();
	}

	void noteon(int note)
	{
		dphase = pow(2, (double(note) / 12.0)) / 5120.0f;
		count = 0;
		//env.reset(0x04020130);
		env.reset(0x20808080);
	}

	double get()
	{
		double value = 0;
		double dph = 1.0f;
		double kr = frand() * 0.001;
		phase += kr;
		phase += dphase;
		for (int oct = 1 ; oct < 2; oct++) {
			for (int i = 0 ; i < 3; i++) {
				double ph = phase * dph;
				double pph = 2.0 * ph * 3.141592653;
				//value += sin(pph + 0.5) > 0.2 ? 1.0 : -1.0;
				//value += sin(pph + 0.3);
				value += (ph - floor(ph)) * 2.0 - 1.0;
				dph *= (1.005 + i);
			}
		}
		float vol = env.update();
		value *= vol;
		value = filter_sample(value, 4000, FILTER_LPF, z0);
		//value = filter_sample(value, 1000 + 5000 * vol * vol * vol, FILTER_PEAKING, z1);
		count++;
		return value;
	}
};


void
update_audio(float *buf, int count, int ch)
{
	struct bar {
		int tempo;
		int key;
		int chord;
		int type;
		int variation;
		bool enable_bass;
		int notes[2][16];
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
	for (int i = 0 ; i < count; i += ch) {
		if (counter-- < 0) {
			counter = b0.tempo * 512;
			int index = 31 - (note_counter * 2);
			int oct = 30 - (note_counter * 2);
			for (int ch = 0 ; ch < 4; ch++) {
				auto & vch = chord[ch];
				auto variation = b0.variation;
				uint32_t note = b0.key + notetable[b0.chord][ch];
				uint32_t pat = pattern[variation][ch];
				int isoct = pat &  (1 << oct);
				if (pat & (1 << index)) {
					note += (isoct ? 12 : 0);
					vch.noteon(note);
					printf("%02X ", note);
				}
			}
			for (int ch = 0 ; ch < 2; ch++) {
				auto & vch = melo[ch];
				uint32_t note = b0.notes[ch][note_counter];
				if (note) {
					vch.noteon(note + 12);
				}
			}
			printf("\n");
			note_counter = (note_counter + 1) % 16;
			if (note_counter == 0) {
				printf("-------------------------------------------------------------------------\n");
				bar_index++;
			}
		}
		auto value = 0.0f;

		for (auto & vch : chord) {
			value += vch.get();
		}


		for (auto & vch : melo) {
			value += vch.get();
		}


		double L = 0.0;
		double R = 0.0;
		{
			static delay fake;
			L += value - fake.get() * 0.5;
			R += value + fake.get() * 0.3;
			fake.update(value + L);
		}
		{
			static delay fake2;
			L += value - fake2.get() * 0.2;
			R += value + fake2.get() * 0.5;
			fake2.update(value + R);
		}
		buf[i + 0] = L * 0.01;
		buf[i + 1] = R * 0.01;
	}
}

int
main()
{
	printf("WINMM chord sound start.\n");
	printf("Exit : ESCAPE\n");
	filter_init(44100, 20000);
	auto thread = std::thread([&]() {
		run_audio(44100, update_audio, []() {
			if( (GetAsyncKeyState(VK_ESCAPE) & 0x8000) )
				return false;
			return true;
		});
	});
	while (!(GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
		Sleep(16);
	}
	thread.join();
	return 0;
}

