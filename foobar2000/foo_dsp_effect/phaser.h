#ifndef		PHASER_H
#define		PHASER_H

#include "../SDK/foobar2000.h"

class Phaser
{
private:
   float freq;
   float startphase;
   float fb;
   int depth;
   int stages;
   int drywet;
   unsigned long skipcount;
   audio_sample old[24];
   float gain;
   float fbout;
   float lfoskip;
   float phase;
public:
	Phaser();
	~Phaser();
	audio_sample Process(audio_sample input);
	void init(int samplerate);
	void SetLFOFreq(float val);
	void SetLFOStartPhase(float val);
	void SetFeedback(float val);
	void SetDepth(int val);
	void SetStages(int val);
	void SetDryWet(int val);
};


#endif		//ECHO_H
