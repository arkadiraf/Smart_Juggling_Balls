// Juggling.h

#ifndef _JUGGLING_h
#define _JUGGLING_h

#if defined(ARDUINO) && ARDUINO >= 100
	#include "Arduino.h"
#else
	#include "WProgram.h"
#endif

class JugglingClass
{
 protected:


 public:
	void init();
};

extern JugglingClass Juggling;

#endif

