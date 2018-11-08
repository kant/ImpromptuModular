//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************

#include "dsp/digital.hpp"
#include "PhraseSeq32ExUtil.hpp"

using namespace rack;


int moveIndexEx(int index, int indexNext, int numSteps) {
	if (indexNext < 0)
		index = numSteps - 1;
	else
	{
		if (indexNext - index >= 0) { // if moving right or same place
			if (indexNext >= numSteps)
				index = 0;
			else
				index = indexNext;
		}
		else { // moving left 
			if (indexNext >= numSteps)
				index = numSteps - 1;
			else
				index = indexNext;
		}
	}
	return index;
}


// TODO: move non trivial methods here

const int SequencerKernel::ppsValues[NUM_PPS_VALUES] = {1, 4, 6, 8, 12, 16, 18, 20, 24, 28, 30, 32, 36, 40, 42, 44, 48, 52, 54, 56, 60, 64, 66, 68, 72, 76, 78, 80, 84, 88, 90, 92, 96};
const std::string SequencerKernel::modeLabels[NUM_MODES] = {"FWD","REV","PPG","PEN","BRN","RND"};


const uint64_t SequencerKernel::advGateHitMaskLow[NUM_GATES] = 
{0x0000000000FFFFFF, 0x0000FFFF0000FFFF, 0x0000FFFFFFFFFFFF, 0x0000FFFF00000000, 0xFFFFFFFFFFFFFFFF, 0xFFFFFFFFFFFFFFFF, 
//				25%					TRI		  			50%					T23		  			75%					FUL		
 0x000000000000FFFF, 0xFFFF000000FFFFFF, 0x0000FFFF00000000, 0xFFFF000000000000, 0x0000000000000000, 0};
//  			TR1 				DUO		  			TR2 	     		D2		  			TR3  TRIG		
const uint64_t SequencerKernel::advGateHitMaskHigh[NUM_GATES] = 
{0x0000000000000000, 0x000000000000FFFF, 0x0000000000000000, 0x000000000000FFFF, 0x00000000000000FF, 0x00000000FFFFFFFF, 
//				25%					TRI		  			50%					T23		  			75%					FUL		
 0x0000000000000000, 0x00000000000000FF, 0x0000000000000000, 0x00000000000000FF, 0x000000000000FFFF, 0};
//  			TR1 				DUO		  			TR2 	     		D2		  			TR3  TRIG		



