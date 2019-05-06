//------------//------------//------------//------------//------------//------------
//
// KP Str0ng ref :: https://ccrma.stanford.edu/~jos/pasp/Karplus_Strong_Algorithm.html
//
// cl cbevent.cpp /EHsc /Ox
//
//------------//------------//------------//------------//------------//------------
#include <vector>
#include <stdio.h>
#include <math.h>
#include <windows.h>
#include <process.h>
#include <mmsystem.h>
#include <mmreg.h>

//------------//------------//------------//------------//------------//------------
// comment
//------------//------------//------------//------------//------------//------------
#pragma comment(lib,     "winmm.lib")
#pragma comment(lib,     "user32.lib")
#pragma comment(lib,     "ole32.lib")

//------------//------------//------------//------------//------------//------------
// Def
//------------//------------//------------//------------//------------//------------
typedef float t_snd;
typedef void (*SoundFillFunc)(void *dest, int samples);

//------------//------------//------------//------------//------------//------------
// Config
//------------//------------//------------//------------//------------//------------
enum {
  Bits         = (sizeof(t_snd) * 8),
  Channel      = (2),
  Freq         = (44100),
  Align        = ((Channel * Bits) / 8),
  BytePerSec   = (Freq * Align),
  BufNum       = 6,
  Samples      = 512,
};

//------------//------------//------------//------------//------------//------------
// SoundThread
//------------//------------//------------//------------//------------//------------
static VOID SoundThread(void *ptr) {

  DWORD          count  = 0;
  SoundFillFunc  fill   = (SoundFillFunc)ptr;
  HANDLE         hEvent = CreateEvent(NULL, FALSE, FALSE, NULL);
  HWAVEOUT       hwo    = NULL;
  WAVEFORMATEX   wfx    = {
    sizeof(t_snd) == sizeof(float) ? WAVE_FORMAT_IEEE_FLOAT : WAVE_FORMAT_PCM,
    Channel, Freq, BytePerSec, Align, Bits, 0
  };
  WAVEHDR        whdr[BufNum];

  if(waveOutOpen(&hwo, WAVE_MAPPER, &wfx, (DWORD)hEvent, 0, CALLBACK_EVENT)) {
    return;
  }

  std::vector< std::vector<char> > soundbuffer;
  soundbuffer.resize(BufNum);
  for(int i = 0 ; i < BufNum; i++) {
    soundbuffer[i].resize(Samples * wfx.nBlockAlign);
    WAVEHDR temp = { &soundbuffer[i][0], Samples * wfx.nBlockAlign, 0, 0, 0, 0, NULL, 0 };
    whdr[i]      = temp;
    waveOutPrepareHeader(hwo, &whdr[i], sizeof(WAVEHDR));
    waveOutWrite(hwo, &whdr[i], sizeof(WAVEHDR));
  }

  MSG   msg;
  while(TRUE) {
    if(PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
      if(msg.message == WM_QUIT) break;
    if(WaitForSingleObject(hEvent, 1000) == WAIT_TIMEOUT)
      continue;
    if(whdr[count].dwFlags & WHDR_DONE) {
      fill((void *)whdr[count].lpData, whdr[count].dwBufferLength);
      waveOutWrite(hwo, &whdr[count], sizeof(WAVEHDR));
      count = (count + 1) % BufNum;
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
  if(hEvent) CloseHandle(hEvent);
}

//------------//------------//------------//------------//------------//------------
// SoundInit
//------------//------------//------------//------------//------------//------------
HANDLE SoundInit(SoundFillFunc func) {
  return (HANDLE)_beginthread(SoundThread, 0, func);
}

//------------//------------//------------//------------//------------//------------
// SoundTerm
//------------//------------//------------//------------//------------//------------
void SoundTerm(HANDLE hThread) {
  if(!hThread) return;
  PostThreadMessage(GetThreadId(hThread), WM_QUIT, 0, 0);
  WaitForSingleObject(hThread, 5000); //max5s
  CloseHandle(hThread);
}


//------------//------------//------------//------------//------------//------------
//
// KP Str0ng
//
//------------//------------//------------//------------//------------//------------

int random() {
  //static int a = 12347;
  static int a = 1237;
  static int b = 234567;
  static int c = 890123;
  a += b;  b += c;  c += a;
  return (a >> 16);
}


struct Osc {
  #define LenMax  (Freq * 2)
  int Idx, Len, Inter;
  t_snd buf0[LenMax];
  
  void On(int note, int it = 2) {
    Len = LenMax / (int)pow(2.0, double(note) / 12.00);
    for(int i = 0 ; i < Len; i++)
      buf0[i] = float(random()) / 32768.0;
    Inter = it;
  }

  t_snd Get() {
    t_snd g = 0;
    if(Len <= 0) return 0;
    Idx++;
    
    //Interppp
    for(int i = 0 ; i < Inter; i++)
      g += buf0[(Idx + i) % Len] * 0.997;
    g /= Inter;
    buf0[Idx % Len] = g;
    return g;
  }
};


//------------//------------//------------//------------//------------//------------
//
// Reverb
//
//------------//------------//------------//------------//------------//------------
struct DelayBuffer
{
  enum
  {
    Max = 100000,
  };
  float Buf[Max];
  unsigned long   Rate;
  unsigned int   Index;
  void Init(unsigned long m)        { Rate = m;  }
  void Update(float a)              { Buf[Index++ % Rate ] = a;  }
  float Sample(unsigned long n = 0) { return Buf[ (Index + n) % Rate]; }
};

//http://www.ari-web.com/service/soft/reverb-2.htm
struct Reverb
{
  enum
  {
    CombMax = 4,
    AllMax  = 2,
  };
  DelayBuffer comb[CombMax];
  DelayBuffer all[AllMax];

  float Sample(float a, int index = 0, int character = 0, int lpfnum = 4)
  {
    const int tau[][4][4] = 
    {
      //prime
      {
        {2063, 1847, 1523, 1277}, {3089, 2927, 2801, 2111}, {5479, 5077, 4987, 4057}, {9929, 7411, 4951, 1063},
      },
      {
        {2053, 1867, 1531, 1259}, {3109, 2939, 2803, 2113}, {5477, 5059, 4993, 4051}, {9949, 7393, 4957, 1097},
      }
    };

    const float gain[] = 
    {
       -(0.8733), -(0.8223), -(0.8513), -(0.8503),
    };
    float D = a * 0.5;
    float E = 0;
    
    //Comb
    for(int i = 0 ; i < CombMax; i++)
    {
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



//------------//------------//------------//------------//------------//------------
//
// User
//
//------------//------------//------------//------------//------------//------------
Reverb rebL;
Reverb rebR;

#define OSC_MAX 8
Osc osc[OSC_MAX];


//fill callback
void FillFunc(void *dest, int samples)
{
  t_snd *b = (t_snd *)dest;
  samples /= sizeof(t_snd);
  samples /= Channel;
  for(int i = 0; i < samples; i++)
  {
    t_snd v = 0;
    for(int o = 0 ; o < OSC_MAX; o++)
      v += osc[o].Get();

    //koko kara tekitou
    static t_snd mult[] = {
      0.3, -0.5, 0.7
    };
    static t_snd z[3] = {0};
    for(int tap = 0; tap < 3; tap++) {
      v = v + (z[tap] - v) * mult[tap];
      z[tap] = v;
    }

    b[i * 2 + 0] = 0.3 * v + (rebL.Sample(v, 0, 0, 2));
    b[i * 2 + 1] = 0.3 * v + (rebR.Sample(v, 0, 1, 2));
  }
}


//------------//------------//------------//------------//------------//------------
// ep
//------------//------------//------------//------------//------------//------------
int main(int argc, char *argv[])
{
  HANDLE h = SoundInit(FillFunc);
  printf("    Play : Z, X, C, V, B, N, M, K\nAutoPlay : Hold SpaceKey.\n");
  while(h && !(GetAsyncKeyState(VK_ESCAPE) & 0x8000)) {
    Sleep(0);
    static int root  = 0x6d;
    
    //AutoPlay
    if( GetAsyncKeyState(VK_SPACE) & 0x8000)
    {
      static unsigned long start = timeGetTime();
      unsigned long now = timeGetTime();
      static int count = 0;
      if( (now - start) > 6 )
      {
        //printf("%d\n", (now - start));
        start = now;
        static int count = 0;
        static int bass  = 1;
        count--;
        if(count < 0)
        {
          int table[] = { 10, 7, 3, 0, -7, -12, -24};
          for(int i = 0; i < 7; i++)
            if( (random() % 7) == 0) osc[i].On(root + table[i], i + 2);
          count = 16;
          bass++;
          if( (bass % 32) == 0)
          {
            root += (random() & 0x8) ? 4 : 0;
            root -= (random() & 0x8) ? 4 : 0;
          }
        }
      } else {
        Sleep(1);
      }
    }
    
    //kbd.
    if(GetAsyncKeyState('Z') & 0x0001) osc[0].On(root -24, 12);
    if(GetAsyncKeyState('X') & 0x0001) osc[1].On(root -12,  9);
    if(GetAsyncKeyState('C') & 0x0001) osc[2].On(root -5,   7);
    if(GetAsyncKeyState('V') & 0x0001) osc[3].On(root +0,   5);
    if(GetAsyncKeyState('B') & 0x0001) osc[4].On(root +3,   4);
    if(GetAsyncKeyState('N') & 0x0001) osc[5].On(root +7,   3);
    if(GetAsyncKeyState('M') & 0x0001) osc[6].On(root +10,  2);
    if(GetAsyncKeyState('K') & 0x0001) osc[7].On(root +12,  2);
  }
  SoundTerm(h);
  return 0;
}

