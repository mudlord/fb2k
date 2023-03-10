#include "echo.h"

#include "../SDK/foobar2000.h"


Echo::Echo()
{

	history = NULL;
	rate = 44100;
	SetDelay(200);		
	SetAmp(128);		
	SetFeedback(50);
	pos = 0;
}


Echo::~Echo()
{
	delete [] history;
	history = NULL;
}


void Echo::SetDelay( int ms )
{
	int newDelay = ms * rate / 1000;
	audio_sample* newHistory = new audio_sample[newDelay];
	memset( newHistory, 0, newDelay * sizeof(audio_sample) );
	if ( history )
	{
		int howMuch = delay - pos;
		howMuch = min( howMuch, newDelay );
		memcpy( newHistory, history + pos, howMuch * sizeof(audio_sample));
		if ( howMuch < newDelay )
		{
			int i = howMuch;
			howMuch = newDelay - howMuch;
			howMuch = min( howMuch, delay );
			howMuch = min( howMuch, pos );
			memcpy( newHistory + i, history, howMuch * sizeof(audio_sample));
		}
		delete [] history;
	}
	history = newHistory;
	pos = 0;
	delay = newDelay;
	this->ms = ms;
}

void Echo::SetAmp( int amp )
{
	this->amp = amp;
	f_amp = ( float ) amp / 256.0f;
}


void Echo::SetSampleRate( int rate )
{
	if ( this->rate != rate )
	{
		this->rate = rate;
		SetDelay( ms );
	}
}

void Echo::SetFeedback(int feedback)
{
	this->feedback = feedback;
	f_feedback = (float)feedback/256.0;
}


int Echo::GetDelay() const
{
	return ms;
}

int Echo::GetFeedback() const
{
	return feedback;
}


int Echo::GetAmp() const
{
	return amp;
}


int Echo::GetSampleRate() const
{
	return rate;
}



audio_sample Echo::Process(audio_sample in)
{
	audio_sample smp = history[pos];
	smp *= f_amp;			
	smp += in;                
	history[pos] = smp  * f_feedback;
	pos = ( pos + 1 ) % delay;
	return smp;
}
