#ifndef		WAHWAH_H
#define		WAHWAH_H

#include "../SDK/foobar2000.h"

class WahWah
{
private:
   float phase;
   float lfoskip;
   unsigned long skipcount;
   audio_sample xn1, xn2, yn1, yn2;
   audio_sample b0, b1, b2, a0, a1, a2;
   float freq, startphase;
   float depth, freqofs, res;
public:   
   WahWah();
   ~WahWah();
   audio_sample Process(audio_sample samp);
	void init(int samplerate);
	void SetLFOFreq(float val);
	void SetLFOStartPhase(float val);
	void SetDepth(float val);
	void SetFreqOffset(float val);
	void SetResonance(float val);
};


#endif		//ECHO_H
