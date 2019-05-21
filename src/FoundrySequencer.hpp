//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//***********************************************************************************************

#ifndef FOUNDRY_SEQUENCER_HPP
#define FOUNDRY_SEQUENCER_HPP


#include "FoundrySequencerKernel.hpp"


class Sequencer {
	public: 
	
	// Sequencer dimensions
	static const int NUM_TRACKS = 4;
	static constexpr float gateTime = 0.4f;// seconds


	private:
	
	// Need to save
	int stepIndexEdit;
	int phraseIndexEdit;
	int trackIndexEdit;
	SequencerKernel sek[NUM_TRACKS];
	
	// No need to save	
	unsigned long editingType;// similar to editingGate, but just for showing remanent gate type (nothing played); uses editingGateKeyLight
	unsigned long editingGate[NUM_TRACKS];// 0 when no edit gate, downward step counter timer when edit gate
	float editingGateCV[NUM_TRACKS];// no need to initialize, this goes with editingGate (output this only when editingGate > 0)
	int editingGateCV2[NUM_TRACKS];// no need to initialize, this goes with editingGate (output this only when editingGate > 0)
	int editingGateKeyLight;// no need to initialize, this goes with editingGate (use this only when editingGate > 0)
	int delayedSeqNumberRequest[NUM_TRACKS];
	SeqCPbuffer seqCPbuf;
	SongCPbuffer songCPbuf;
	int* velocityModePtr;
	
	
	public: 
	
	void construct(bool* _holdTiedNotesPtr, int* _velocityModePtr, int* _stopAtEndOfSongPtr);

	inline int getStepIndexEdit() {return stepIndexEdit;}
	inline int getSeqIndexEdit() {return sek[trackIndexEdit].getSeqIndexEdit();}
	inline int getSeqIndexEdit(int trkn) {return sek[trkn].getSeqIndexEdit();}
	inline int getPhraseIndexEdit() {return phraseIndexEdit;}
	inline int getTrackIndexEdit() {return trackIndexEdit;}
	inline int getStepIndexRun(int trkn) {return sek[trkn].getStepIndexRun();}
	inline int getLength() {return sek[trackIndexEdit].getLength();}
	inline float getCV(bool editingSequence) {
		if (editingSequence)
			return sek[trackIndexEdit].getCV(stepIndexEdit);
		return sek[trackIndexEdit].getCV(editingSequence);
	}
	inline StepAttributes getAttribute(bool editingSequence) {
		if (editingSequence)
			return sek[trackIndexEdit].getAttribute(stepIndexEdit);
		return sek[trackIndexEdit].getAttribute(editingSequence);
	}
	inline StepAttributes getAttribute(bool editingSequence, int stepn) {return sek[trackIndexEdit].getAttributei(editingSequence, stepn);}
	inline int keyIndexToGateTypeEx(int keyn) {return sek[trackIndexEdit].keyIndexToGateTypeEx(keyn);}
	inline int getPulsesPerStep() {return sek[trackIndexEdit].getPulsesPerStep();}
	inline int getDelay() {return sek[trackIndexEdit].getDelay();}
	inline int getRunModeSong() {return sek[trackIndexEdit].getRunModeSong();}
	inline int getRunModeSeq() {return sek[trackIndexEdit].getRunModeSeq();}
	inline int getPhraseReps() {return sek[trackIndexEdit].getPhraseReps(phraseIndexEdit);}
	inline int getBegin() {return sek[trackIndexEdit].getBegin();}
	inline int getEnd() {return sek[trackIndexEdit].getEnd();}
	inline int getTransposeOffset() {return sek[trackIndexEdit].getTransposeOffset();}
	inline int getRotateOffset() {return sek[trackIndexEdit].getRotateOffset();}
	inline int getPhraseSeq() {return sek[trackIndexEdit].getPhraseSeq(phraseIndexEdit);}
	inline int getEditingGateKeyLight() {return editingGateKeyLight;}
	inline unsigned long getEditingType() {return editingType;}
	
	
	inline void setEditingGateKeyLight(int _editingGateKeyLight) {editingGateKeyLight = _editingGateKeyLight;}
	inline void setStepIndexEdit(int _stepIndexEdit, int sampleRate) {
		stepIndexEdit = _stepIndexEdit;
		StepAttributes stepAttrib = sek[trackIndexEdit].getAttribute(stepIndexEdit);
		if (!stepAttrib.getTied()) {// play if non-tied step
			editingGate[trackIndexEdit] = (unsigned long) (gateTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
			editingGateCV[trackIndexEdit] = sek[trackIndexEdit].getCV(stepIndexEdit);
			editingGateCV2[trackIndexEdit] = stepAttrib.getVelocityVal();
			editingGateKeyLight = -1;
		}
	}
	inline void setSeqIndexEdit(int _seqIndexEdit, int trkn) {
		sek[trkn].setSeqIndexEdit(_seqIndexEdit);
	}
	inline void setPhraseIndexEdit(int _phraseIndexEdit) {phraseIndexEdit = _phraseIndexEdit;}
	inline void bringPhraseIndexRunToEdit() {sek[trackIndexEdit].setPhraseIndexRun(phraseIndexEdit);}
	inline void setTrackIndexEdit(int _trackIndexEdit) {trackIndexEdit = _trackIndexEdit % NUM_TRACKS;}
	void setVelocityVal(int trkn, int intVel, int multiStepsCount, bool multiTracks);
	void setLength(int length, bool multiTracks);
	void setBegin(bool multiTracks);
	void setEnd(bool multiTracks);
	bool setGateType(int keyn, int multiSteps, float sampleRate, bool autostepClick, bool multiTracks); // Third param is for right-click autostep. Returns success
	
	
	void initSlideVal(int multiStepsCount, bool multiTracks);
	void initGatePVal(int multiStepsCount, bool multiTracks);
	void initVelocityVal(int multiStepsCount, bool multiTracks);
	void initPulsesPerStep(bool multiTracks);
	void initDelay(bool multiTracks);
	void initRunModeSong(bool multiTracks);
	void initRunModeSeq(bool multiTracks);
	void initLength(bool multiTracks);
	void initPhraseReps(bool multiTracks);
	void initPhraseSeqNum(bool multiTracks);
	
	
	inline void incTrackIndexEdit() {
		if (trackIndexEdit < (NUM_TRACKS - 1)) trackIndexEdit++;
		else trackIndexEdit = 0;
	}
	inline void decTrackIndexEdit() {
		if (trackIndexEdit > 0) trackIndexEdit--;
		else trackIndexEdit = NUM_TRACKS - 1;
	}
	
	int getLengthSeqCPbuf() {return seqCPbuf.storedLength;}
	void copySequence(int countCP);
	void pasteSequence(bool multiTracks);
	void copySong(int startCP, int countCP);
	void pasteSong(bool multiTracks);
	
	
	void writeCV(int trkn, float cvVal, int multiStepsCount, float sampleRate, bool multiTracks);
	void autostep(bool autoseq, bool autostepLen, bool multiTracks);
	bool applyNewOctave(int octn, int multiSteps, float sampleRate, bool multiTracks); // returns true if tied
	bool applyNewKey(int keyn, int multiSteps, float sampleRate, bool autostepClick, bool multiTracks); // returns true if tied

	inline void moveStepIndexEdit(int delta, bool loopOnLength) {
		stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + delta, loopOnLength ? getLength() : SequencerKernel::MAX_STEPS);
	}
	
	void moveStepIndexEditWithEditingGate(int delta, bool writeTrig, float sampleRate);
	
	inline void moveSeqIndexEdit(int delta) {
		sek[trackIndexEdit].modSeqIndexEdit(delta);
	}
	
	inline void movePhraseIndexEdit(int deltaPhrKnob) {
		phraseIndexEdit = moveIndex(phraseIndexEdit, phraseIndexEdit + deltaPhrKnob, SequencerKernel::MAX_PHRASES);
	}

	
	void modSlideVal(int deltaVelKnob, int mutliStepsCount, bool multiTracks);
	void modGatePVal(int deltaVelKnob, int mutliStepsCount, bool multiTracks);
	void modVelocityVal(int deltaVelKnob, int mutliStepsCount, bool multiTracks);
	void modRunModeSong(int deltaPhrKnob, bool multiTracks);
	void modPulsesPerStep(int deltaSeqKnob, bool multiTracks);
	void modDelay(int deltaSeqKnob, bool multiTracks);
	void modRunModeSeq(int deltaSeqKnob, bool multiTracks);
	void modLength(int deltaSeqKnob, bool multiTracks);
	void modPhraseReps(int deltaSeqKnob, bool multiTracks);
	void modPhraseSeqNum(int deltaSeqKnob, bool multiTracks);
	void transposeSeq(int deltaSeqKnob, bool multiTracks);
	void unTransposeSeq(bool multiTracks);
	void rotateSeq(int deltaSeqKnob, bool multiTracks);
	void unRotateSeq(bool multiTracks);

	void toggleGate(int multiSteps, bool multiTracks);
	bool toggleGateP(int multiSteps, bool multiTracks); // returns true if tied
	bool toggleSlide(int multiSteps, bool multiTracks); // returns true if tied
	void toggleTied(int multiSteps, bool multiTracks);


	inline float calcCvOutputAndDecSlideStepsRemain(int trkn, bool running, bool editingSequence) {
		float cvout = 0.0f;
		if (editingSequence && !running)
			cvout = (editingGate[trkn] > 0ul) ? editingGateCV[trkn] : sek[trkn].getCV(stepIndexEdit);
		else
			cvout = sek[trkn].getCV(editingSequence) - (running ? sek[trkn].calcSlideOffset() : 0.0f);
		sek[trkn].decSlideStepsRemain();
		return cvout;
	}
	inline float calcGateOutput(int trkn, bool running, Trigger clockTrigger, float sampleRate) {
		if (running) 
			return (sek[trkn].calcGate(clockTrigger, sampleRate) ? 10.0f : 0.0f);
		return (editingGate[trkn] > 0ul) ? 10.0f : 0.0f;
	}
	inline float calcVelOutput(int trkn, bool running, bool editingSequence) {
		int vVal = 0;
		if (editingSequence && !running)
			vVal = (editingGate[trkn] > 0ul) ? editingGateCV2[trkn] : sek[trkn].getAttribute(stepIndexEdit).getVelocityVal();
		else 
			vVal = sek[trkn].getAttribute(editingSequence).getVelocityVal();

		float velRet = (float)vVal;
		if (*velocityModePtr == 0)
			velRet = velRet * 10.0f / 200.0f;
		else if (*velocityModePtr == 1)
			velRet = velRet * 10.0f / 127.0f;
		else
			velRet = velRet / 12.0f;
		return std::min(velRet, 10.0f);
	}
	inline float calcKeyLightWithEditing(int keyScanIndex, int keyLightIndex, float sampleRate) {
		if (editingGate[trackIndexEdit] > 0ul && editingGateKeyLight != -1)
			return (keyScanIndex == editingGateKeyLight ? ((float) editingGate[trackIndexEdit] / (float)(gateTime * sampleRate / RefreshCounter::displayRefreshStepSkips)) : 0.0f);
		return (keyScanIndex == keyLightIndex ? 1.0f : 0.0f);
	}
	
	
	inline void attach(bool editingSequence) {
		if (editingSequence) {
			stepIndexEdit = sek[trackIndexEdit].getStepIndexRun();
		}
		else {
			phraseIndexEdit = sek[trackIndexEdit].getPhraseIndexRun();
		}		
	}
	
	
	inline void stepEditingGate() {// also steps editingType 
		for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
			if (editingGate[trkn] > 0ul)
				editingGate[trkn]--;
		}
		if (editingType > 0ul)
			editingType--;
	}
	
	
	void dataToJson(json_t *rootJ);
	void dataFromJson(json_t *rootJ);
	
	void reset(bool editingSequence);
	inline void initDelayedSeqNumberRequest() {
		for (int trkn = 0; trkn < NUM_TRACKS; trkn++) {
			delayedSeqNumberRequest[trkn] = -1;
		}
	};
	inline void requestDelayedSeqChange(int trkn, int seqn) {
		delayedSeqNumberRequest[trkn] = seqn;
	};
	
	inline void randomize(bool editingSequence) {sek[trackIndexEdit].randomize(editingSequence);}
	
	inline void initRun(bool editingSequence) {
		initDelayedSeqNumberRequest();
		for (int trkn = 0; trkn < NUM_TRACKS; trkn++)
			sek[trkn].initRun(editingSequence);
	}

	bool clockStep(int trkn, bool editingSequence);// returns true to signal that run should be turned off
	
	inline void process() {
		for (int trkn = 0; trkn < NUM_TRACKS; trkn++)
			sek[trkn].process();
	}
	
};// class Sequencer 


#endif