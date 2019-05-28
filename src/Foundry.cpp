//***********************************************************************************************
//Multi-track sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Acknowledgements: please see README.md
//***********************************************************************************************

#include <algorithm>
#include <time.h>
#include "FoundrySequencer.hpp"


struct Foundry : Module {	
	enum ParamIds {
		UNUSED1_PARAM,// no longer used
		PHRASE_PARAM,
		SEQUENCE_PARAM,
		RUN_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		RESET_PARAM,
		ENUMS(OCTAVE_PARAM, 7),
		GATE_PARAM,
		SLIDE_BTN_PARAM,
		AUTOSTEP_PARAM,
		ENUMS(KEY_PARAMS, 12),
		MODE_PARAM,
		CLKRES_PARAM,
		TRAN_ROT_PARAM,
		GATE_PROB_PARAM,
		TIE_PARAM,// Legato
		CPMODE_PARAM,
		ENUMS(STEP_PHRASE_PARAMS, SequencerKernel::MAX_STEPS),
		TRACKDOWN_PARAM,
		TRACKUP_PARAM,
		VEL_KNOB_PARAM,
		SEL_PARAM,
		ALLTRACKS_PARAM,
		REP_LEN_PARAM,
		BEGIN_PARAM,
		END_PARAM,
		KEY_GATE_PARAM,
		ATTACH_PARAM,
		VEL_EDIT_PARAM,
		EDIT_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		WRITE_INPUT,
		ENUMS(CV_INPUTS, Sequencer::NUM_TRACKS),
		RESET_INPUT,
		ENUMS(CLOCK_INPUTS, Sequencer::NUM_TRACKS),
		UNUSED1_INPUT,// no longer used
		UNUSED2_INPUT,// no longer used
		RUNCV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(CV_OUTPUTS, Sequencer::NUM_TRACKS),
		ENUMS(VEL_OUTPUTS, Sequencer::NUM_TRACKS),
		ENUMS(GATE_OUTPUTS, Sequencer::NUM_TRACKS),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_PHRASE_LIGHTS, SequencerKernel::MAX_STEPS * 3),// room for GreenRedWhite
		ENUMS(OCTAVE_LIGHTS, 7),// octaves 1 to 7
		ENUMS(KEY_LIGHTS, 12 * 2),// room for GreenRed
		RUN_LIGHT,
		RESET_LIGHT,
		ENUMS(GATE_LIGHT, 2),// room for GreenRed
		SLIDE_LIGHT,
		ENUMS(GATE_PROB_LIGHT, 2),// room for GreenRed
		TIE_LIGHT,
		ATTACH_LIGHT,
		ENUMS(VEL_PROB_LIGHT, 2),// room for GreenRed
		VEL_SLIDE_LIGHT,
		ENUMS(WRITECV_LIGHTS, Sequencer::NUM_TRACKS),
		NUM_LIGHTS
	};
	
	// Expander
	static const int messageSize = 	Sequencer::NUM_TRACKS + // VEL_INPUTS with connected
									Sequencer::NUM_TRACKS + // SEQCV_INPUTS with connected
									1 + // TRKCV_INPUT with connected
									7 + // GATECV_INPUT, GATEPCV_INPUT, TIEDCV_INPUT, SLIDECV_INPUT, WRITE_SRC_INPUT, LEFTCV_INPUT, RIGHTCV_INPUT
									2; // SYNC_SEQCV_PARAM, WRITEMODE_PARAM
	float consumerMessage[messageSize] = {};// this module must read from here
	float producerMessage[messageSize] = {};// expander will write into here
		
	// Constants
	enum EditPSDisplayStateIds {DISP_NORMAL, DISP_MODE_SEQ, DISP_MODE_SONG, DISP_LEN, DISP_REPS, DISP_TRANSPOSE, DISP_ROTATE, DISP_PPQN, DISP_DELAY, DISP_COPY_SEQ, DISP_PASTE_SEQ, DISP_COPY_SONG, DISP_PASTE_SONG, DISP_COPY_SONG_CUST};
	static constexpr float warningTime = 0.7f;// seconds

	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	int expansion;
	int velocityMode;
	bool velocityBipol;
	bool autostepLen;
	bool multiTracks;
	bool autoseq;
	bool holdTiedNotes;
	bool showSharp;
	int seqCVmethod;// 0 is 0-10V, 1 is C2-D7#, 2 is TrigIncr
	bool running;
	bool resetOnRun;
	bool attached;
	int velEditMode;// 0 is velocity (aka CV2), 1 is gate-prob, 2 is slide-rate
	int writeMode;// 0 is both, 1 is CV only, 2 is CV2 only
	int stopAtEndOfSong;// 0 to 3 is YES stop on song end of that track, 4 is NO (off)
	Sequencer seq;

	// No need to save, with reset
	int displayState;
	long clockIgnoreOnReset;
	long tiedWarning;// 0 when no warning, positive downward step counter timer when warning
	long attachedWarning;// 0 when no warning, positive downward step counter timer when warning
	long revertDisplay;
	bool multiSteps;
	int clkInSources[Sequencer::NUM_TRACKS];// first index is always 0 and will never change
	int cpSeqLength;
	
	// No need to save, no reset
	int cpSongStart;// no need to initialize
	RefreshCounter refresh;
	float resetLight = 0.0f;
	int sequenceKnob = 0;
	int velocityKnob = 0;
	int phraseKnob = 0;
	Trigger resetTrigger;
	Trigger leftTrigger;
	Trigger rightTrigger;
	Trigger runningTrigger;
	Trigger clockTriggers[SequencerKernel::MAX_STEPS];
	Trigger keyTriggers[12];
	Trigger octTriggers[7];
	Trigger gate1Trigger;
	Trigger tiedTrigger;
	Trigger gateProbTrigger;
	Trigger slideTrigger;
	Trigger writeTrigger;
	Trigger copyTrigger;
	Trigger pasteTrigger;
	Trigger modeTrigger;
	Trigger rotateTrigger;
	Trigger transposeTrigger;
	Trigger stepTriggers[SequencerKernel::MAX_STEPS];
	Trigger clkResTrigger;
	Trigger trackIncTrigger;
	Trigger trackDecTrigger;	
	Trigger beginTrigger;
	Trigger endTrigger;
	Trigger repLenTrigger;
	Trigger attachedTrigger;
	Trigger seqCVTriggers[Sequencer::NUM_TRACKS];
	Trigger selTrigger;
	Trigger allTrigger;
	Trigger velEditTrigger;
	Trigger writeModeTrigger;

	
	inline bool isEditingSequence(void) {return params[EDIT_PARAM].getValue() > 0.5f;}
	inline bool isEditingGates(void) {return params[KEY_GATE_PARAM].getValue() < 0.5f;}
	inline int getCPMode(void) {
		if (params[CPMODE_PARAM].getValue() > 1.5f) return 2000;// this means end, and code should never loop up to this count. This value should be bigger than max(MAX_STEPS, MAX_PHRASES)
		if (params[CPMODE_PARAM].getValue() < 0.5f) return 4;
		return 8;
	}

	
	Foundry() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		rightExpander.producerMessage = producerMessage;
		rightExpander.consumerMessage = consumerMessage;

		char strBuf[32];
		const int numX = SequencerKernel::MAX_STEPS / 2;
		for (int x = 0; x < numX; x++) {
			// First row
			snprintf(strBuf, 32, "Step %i", x + 1);
			configParam(STEP_PHRASE_PARAMS + x, 0.0f, 1.0f, 0.0f, strBuf);
			// Second row
			snprintf(strBuf, 32, "Step %i", x + numX + 1);
			configParam(STEP_PHRASE_PARAMS + x + numX, 0.0f, 1.0f, 0.0f, strBuf);
		}
		configParam(SEL_PARAM, 0.0f, 1.0f, 0.0f, "Select multi steps");
		configParam(CPMODE_PARAM, 0.0f, 2.0f, 0.0f, "Copy-paste mode");	// 0.0f is top position
		configParam(EDIT_PARAM, 0.0f, 1.0f, 1.0f, "Seq/song mode");// 1.0f is top position
		for (int i = 0; i < 7; i++) {
			snprintf(strBuf, 32, "Octave %i", i + 1);
			configParam(OCTAVE_PARAM + i, 0.0f, 1.0f, 0.0f, strBuf);
		}
		configParam(KEY_PARAMS + 1, 0.0, 1.0, 0.0, "C# key");
		configParam(KEY_PARAMS + 3, 0.0, 1.0, 0.0, "D# key");
		configParam(KEY_PARAMS + 6, 0.0, 1.0, 0.0, "F# key");
		configParam(KEY_PARAMS + 8, 0.0, 1.0, 0.0, "G# key");
		configParam(KEY_PARAMS + 10, 0.0, 1.0, 0.0, "A# key");

		configParam(KEY_PARAMS + 0, 0.0, 1.0, 0.0, "C key");
		configParam(KEY_PARAMS + 2, 0.0, 1.0, 0.0, "D key");
		configParam(KEY_PARAMS + 4, 0.0, 1.0, 0.0, "E key");
		configParam(KEY_PARAMS + 5, 0.0, 1.0, 0.0, "F key");
		configParam(KEY_PARAMS + 7, 0.0, 1.0, 0.0, "G key");
		configParam(KEY_PARAMS + 9, 0.0, 1.0, 0.0, "A key");
		configParam(KEY_PARAMS + 11, 0.0, 1.0, 0.0, "B key");

		configParam(VEL_KNOB_PARAM, -INFINITY, INFINITY, 0.0f, "CV2/p/r knob");	
		configParam(VEL_EDIT_PARAM, 0.0f, 1.0f, 0.0f, "CV2/p/r select");
		configParam(SEQUENCE_PARAM, -INFINITY, INFINITY, 0.0f, "Sequence");		
		configParam(TRAN_ROT_PARAM, 0.0f, 1.0f, 0.0f, "Transpose / rotate");
		configParam(PHRASE_PARAM, -INFINITY, INFINITY, 0.0f, "Phrase");		
		configParam(BEGIN_PARAM, 0.0f, 1.0f, 0.0f, "Begin");
		configParam(END_PARAM, 0.0f, 1.0f, 0.0f, "End");
		configParam(TRACKUP_PARAM, 0.0f, 1.0f, 0.0f, "Track next");
		configParam(TRACKDOWN_PARAM, 0.0f, 1.0f, 0.0f, "Track prev");
		configParam(ALLTRACKS_PARAM, 0.0f, 1.0f, 0.0f, "All tracks");
		configParam(COPY_PARAM, 0.0f, 1.0f, 0.0f, "Copy");
		configParam(PASTE_PARAM, 0.0f, 1.0f, 0.0f, "Paste");
		configParam(ATTACH_PARAM, 0.0f, 1.0f, 0.0f, "Attach");
		configParam(KEY_GATE_PARAM, 0.0f, 1.0f, 1.0f, "Keyboard mode");
		configParam(GATE_PARAM, 0.0f, 1.0f, 0.0f, "Gate");
		configParam(TIE_PARAM, 0.0f, 1.0f, 0.0f, "Tied");
		configParam(GATE_PROB_PARAM, 0.0f, 1.0f, 0.0f, "Gate probability");
		configParam(SLIDE_BTN_PARAM, 0.0f, 1.0f, 0.0f, "CV Slide");
		configParam(MODE_PARAM, 0.0f, 1.0f, 0.0f, "Run mode");
		configParam(REP_LEN_PARAM, 0.0f, 1.0f, 0.0f, "Repetition / length");
		configParam(CLKRES_PARAM, 0.0f, 1.0f, 0.0f, "Clock resolution / delay");
		configParam(RUN_PARAM, 0.0f, 1.0f, 0.0f, "Run");
		configParam(RESET_PARAM, 0.0f, 1.0f, 0.0f, "Reset");
		configParam(AUTOSTEP_PARAM, 0.0f, 1.0f, 1.0f, "Autostep");		
		
		seq.construct(&holdTiedNotes, &velocityMode, &stopAtEndOfSong);
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}

	
	// widgets are not yet created when module is created (and when onReset() is called by constructor)
	// onReset() is also called when right-click initialization of module
	void onReset() override {
		expansion = 0;
		velocityMode = 0;
		velocityBipol = false;
		autostepLen = false;
		multiTracks = false;
		autoseq = false;
		holdTiedNotes = true;
		showSharp = true;
		seqCVmethod = 0;
		running = true;
		resetOnRun = false;
		attached = false;
		velEditMode = 0;
		writeMode = 0;
		stopAtEndOfSong = 4;// this means option is turned off (0-3 is on)
		seq.onReset(isEditingSequence());
		resetNonJson();
	}
	void resetNonJson() {
		displayState = DISP_NORMAL;
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * APP->engine->getSampleRate());
		tiedWarning = 0l;
		attachedWarning = 0l;
		revertDisplay = 0l;
		multiSteps = false;
		for (int trkn = 0; trkn < Sequencer::NUM_TRACKS; trkn++) {
			clkInSources[trkn] = 0;
		}
		cpSeqLength = getCPMode();
	}
	
	
	void onRandomize() override {
		if (isEditingSequence())
			seq.onRandomize(isEditingSequence());
	}
	
	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// expansion
		json_object_set_new(rootJ, "expansion", json_integer(expansion));

		// velocityMode
		json_object_set_new(rootJ, "velocityMode", json_integer(velocityMode));

		// velocityBipol
		json_object_set_new(rootJ, "velocityBipol", json_integer(velocityBipol));

		// autostepLen
		json_object_set_new(rootJ, "autostepLen", json_boolean(autostepLen));
		
		// multiTracks
		json_object_set_new(rootJ, "multiTracks", json_boolean(multiTracks));
		
		// autoseq
		json_object_set_new(rootJ, "autoseq", json_boolean(autoseq));
		
		// holdTiedNotes
		json_object_set_new(rootJ, "holdTiedNotes", json_boolean(holdTiedNotes));
		
		// showSharp
		json_object_set_new(rootJ, "showSharp", json_boolean(showSharp));
		
		// seqCVmethod
		json_object_set_new(rootJ, "seqCVmethod", json_integer(seqCVmethod));

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
		// resetOnRun
		json_object_set_new(rootJ, "resetOnRun", json_boolean(resetOnRun));
		
		// attached
		json_object_set_new(rootJ, "attached", json_boolean(attached));

		// velEditMode
		json_object_set_new(rootJ, "velEditMode", json_integer(velEditMode));

		// writeMode
		json_object_set_new(rootJ, "writeMode", json_integer(writeMode));

		// stopAtEndOfSong
		json_object_set_new(rootJ, "stopAtEndOfSong", json_integer(stopAtEndOfSong));

		// seq
		seq.dataToJson(rootJ);
		
		return rootJ;
	}

	
	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ) {
			panelTheme = json_integer_value(panelThemeJ);
			if (panelTheme == 2)// metal was deprecated 
				panelTheme = 1;
		}

		// expansion
		json_t *expansionJ = json_object_get(rootJ, "expansion");
		if (expansionJ)
			expansion = json_integer_value(expansionJ);

		// velocityMode
		json_t *velocityModeJ = json_object_get(rootJ, "velocityMode");
		if (velocityModeJ)
			velocityMode = json_integer_value(velocityModeJ);

		// velocityBipol
		json_t *velocityBipolJ = json_object_get(rootJ, "velocityBipol");
		if (velocityBipolJ)
			velocityBipol = json_integer_value(velocityBipolJ);

		// autostepLen
		json_t *autostepLenJ = json_object_get(rootJ, "autostepLen");
		if (autostepLenJ)
			autostepLen = json_is_true(autostepLenJ);

		// multiTracks
		json_t *multiTracksJ = json_object_get(rootJ, "multiTracks");
		if (multiTracksJ)
			multiTracks = json_is_true(multiTracksJ);

		// autoseq
		json_t *autoseqJ = json_object_get(rootJ, "autoseq");
		if (autoseqJ)
			autoseq = json_is_true(autoseqJ);

		// holdTiedNotes
		json_t *holdTiedNotesJ = json_object_get(rootJ, "holdTiedNotes");
		if (holdTiedNotesJ)
			holdTiedNotes = json_is_true(holdTiedNotesJ);
		
		// showSharp
		json_t *showSharpJ = json_object_get(rootJ, "showSharp");
		if (showSharpJ)
			showSharp = json_is_true(showSharpJ);
		
		// seqCVmethod
		json_t *seqCVmethodJ = json_object_get(rootJ, "seqCVmethod");
		if (seqCVmethodJ)
			seqCVmethod = json_integer_value(seqCVmethodJ);

		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);
		
		// resetOnRun
		json_t *resetOnRunJ = json_object_get(rootJ, "resetOnRun");
		if (resetOnRunJ)
			resetOnRun = json_is_true(resetOnRunJ);

		// attached
		json_t *attachedJ = json_object_get(rootJ, "attached");
		if (attachedJ)
			attached = json_is_true(attachedJ);
		
		// velEditMode
		json_t *velEditModeJ = json_object_get(rootJ, "velEditMode");
		if (velEditModeJ)
			velEditMode = json_integer_value(velEditModeJ);

		// writeMode
		json_t *writeModeJ = json_object_get(rootJ, "writeMode");
		if (writeModeJ)
			writeMode = json_integer_value(writeModeJ);

		// stopAtEndOfSong
		json_t *stopAtEndOfSongJ = json_object_get(rootJ, "stopAtEndOfSong");
		if (stopAtEndOfSongJ)
			stopAtEndOfSong = json_integer_value(stopAtEndOfSongJ);

		// seq
		seq.dataFromJson(rootJ, isEditingSequence());
		
		resetNonJson();
		seq.initRun(isEditingSequence());// TODO why is this needed? follow this
	}


	void process(const ProcessArgs &args) override {
		const float sampleRate = args.sampleRate;
		static const float revertDisplayTime = 0.7f;// seconds
		
		bool expanderPresent = (rightExpander.module && rightExpander.module->model == modelFoundryExpander);
		
		
		//********** Buttons, knobs, switches and inputs **********
		
		bool editingSequence = isEditingSequence();
		
		// Run button
		if (runningTrigger.process(params[RUN_PARAM].getValue() + inputs[RUNCV_INPUT].getVoltage())) {// no input refresh here, don't want to introduce startup skew
			running = !running;
			if (running) {
				if (resetOnRun)
					seq.initRun(editingSequence);
				if (resetOnRun || clockIgnoreOnRun)
					clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * sampleRate);
			}
			else
				seq.initDelayedSeqNumberRequest();
			displayState = DISP_NORMAL;
		}

		if (refresh.processInputs()) {
			// Track CV input
			float trkCVin = consumerMessage[Sequencer::NUM_TRACKS * 2 + 0];
			if (expanderPresent && !std::isnan(trkCVin)) {
				int newTrk = (int)( trkCVin * (2.0f * (float)Sequencer::NUM_TRACKS - 1.0f) / 10.0f + 0.5f );
				seq.setTrackIndexEdit(abs(newTrk));
				multiTracks = (newTrk > 3);
			}
			
			// Seq CV input
			for (int trkn = 0; trkn < Sequencer::NUM_TRACKS; trkn++) {
				float seqCVin = consumerMessage[Sequencer::NUM_TRACKS + trkn];
				if (expanderPresent && !std::isnan(seqCVin)) {
					int newSeq = -1;
					if (seqCVmethod == 0) {// 0-10 V
						newSeq = (int)(seqCVin * ((float)SequencerKernel::MAX_SEQS - 1.0f) / 10.0f + 0.5f );
						newSeq = clamp(newSeq, 0, SequencerKernel::MAX_SEQS - 1);
					}
					else if (seqCVmethod == 1) {// C2-D7#
						newSeq = (int)( (seqCVin + 2.0f) * 12.0f + 0.5f );
						newSeq = clamp(newSeq, 0, SequencerKernel::MAX_SEQS - 1);
					}
					else if (seqCVTriggers[trkn].process(seqCVin)) {// TrigIncr
						newSeq = clamp(seq.getSeqIndexEdit(trkn) + 1, 0, SequencerKernel::MAX_SEQS - 1);
					}
					if (newSeq >= 0) {
						if (consumerMessage[Sequencer::NUM_TRACKS * 2 + 1 + 7] > 0.5f && running)
							seq.requestDelayedSeqChange(trkn, newSeq);
						else
							seq.setSeqIndexEdit(newSeq, trkn);				
					}
				}
			}
			
			// Attach button
			if (attachedTrigger.process(params[ATTACH_PARAM].getValue())) {
				attached = !attached;
				displayState = DISP_NORMAL;			
				multiSteps = false;
				multiTracks = false;
			}
			if (running && attached) {
				seq.attach(editingSequence);
			}
			
	
			// Copy 
			if (copyTrigger.process(params[COPY_PARAM].getValue())) {
				if (!attached) {
					multiTracks = false;
					if (editingSequence) {
						seq.copySequence(cpSeqLength);					
						displayState = DISP_COPY_SEQ;
					}
					else {
						int cpMode = getCPMode();
						if (cpMode == 2000) {
							if (displayState != DISP_COPY_SONG_CUST) {// first click to set cpSongStart
								cpSongStart = seq.getPhraseIndexEdit();
								displayState = DISP_COPY_SONG_CUST;
							}
							else {// second click do the copy 
								seq.copySong(cpSongStart, std::max(1, seq.getPhraseIndexEdit() - cpSongStart + 1));
								displayState = DISP_COPY_SONG;
							}
						}
						else {
							seq.copySong(seq.getPhraseIndexEdit(), cpMode);
							displayState = DISP_COPY_SONG;
						}
					}
					if (displayState != DISP_COPY_SONG_CUST)
						revertDisplay = (long) (revertDisplayTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
				}
				else
					attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
			}
			// Paste 
			if (pasteTrigger.process(params[PASTE_PARAM].getValue())) {
				if (!attached) {
					if (editingSequence) {
						seq.pasteSequence(multiTracks);
						displayState = DISP_PASTE_SEQ;
					}
					else {
						if (displayState != DISP_COPY_SONG_CUST) {
							seq.pasteSong(multiTracks);
							displayState = DISP_PASTE_SONG;
						}
					}
					if (displayState != DISP_COPY_SONG_CUST)
						revertDisplay = (long) (revertDisplayTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
				}
				else
					attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
			}			
			

			// Write input (must be before Left and Right in case route gate simultaneously to Right and Write for example)
			//  (write must be to correct step)
			bool writeTrig = writeTrigger.process(inputs[WRITE_INPUT].getVoltage());
			if (writeTrig) {
				if (editingSequence) {
					int multiStepsCount = multiSteps ? cpSeqLength : 1;
					for (int trkn = 0; trkn < Sequencer::NUM_TRACKS; trkn++) {
						if (trkn == seq.getTrackIndexEdit() || multiTracks) {
							float velCVin = consumerMessage[0 + trkn];
							if (expanderPresent && !std::isnan(velCVin) && ((writeMode & 0x1) == 0)) {	// must be before seq.writeCV() below, so that editing CV2 can be grabbed
								float maxVel = (velocityMode > 0 ? 127.0f : 200.0f);
								float capturedCV = velCVin + (velocityBipol ? 5.0f : 0.0f);
								int intVel = (int)(capturedCV * maxVel / 10.0f + 0.5f);
								seq.setVelocityVal(trkn, clamp(intVel, 0, 200), multiStepsCount, false);
							}
							if (inputs[CV_INPUTS + trkn].isConnected() && ((writeMode & 0x2) == 0)) {
								seq.writeCV(trkn, clamp(inputs[CV_INPUTS + trkn].getVoltage(), -10.0f, 10.0f), multiStepsCount, sampleRate, false);
							}
						}
					}
					seq.setEditingGateKeyLight(-1);
					if (params[AUTOSTEP_PARAM].getValue() > 0.5f) {
						bool seqConnected = (expanderPresent && !std::isnan(consumerMessage[Sequencer::NUM_TRACKS + seq.getTrackIndexEdit()]));
						seq.autostep(autoseq && !seqConnected, autostepLen, multiTracks);
					}
				}
			}
			// Left and right CV inputs in expander module
			if (expanderPresent) {
				int delta = 0;
				if (leftTrigger.process(consumerMessage[Sequencer::NUM_TRACKS * 2 + 1 + 5])) {
					delta = -1;
				}
				if (rightTrigger.process(consumerMessage[Sequencer::NUM_TRACKS * 2 + 1 + 6])) {
					delta = +1;
				}
				if (delta != 0) {
					if (!running || !attached) {// don't move heads when attach and running
						if (editingSequence) {
							seq.moveStepIndexEditWithEditingGate(delta, writeTrig, sampleRate);
						}
						else {
							seq.movePhraseIndexEdit(delta);
							if (!running)
								seq.bringPhraseIndexRunToEdit();
						}
					}
				}
			}

			// Step button presses
			int stepPressed = -1;
			for (int i = 0; i < SequencerKernel::MAX_STEPS; i++) {
				if (stepTriggers[i].process(params[STEP_PHRASE_PARAMS + i].getValue()))
					stepPressed = i;
			}
			if (stepPressed != -1) {
				if (displayState == DISP_LEN) {
					if (editingSequence) {
						seq.setLength(stepPressed + 1, multiTracks);
					}
					revertDisplay = (long) (revertDisplayTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
				}
				else {
					if (!running || !attached) {// not running or detached
						if (editingSequence) {
							if (multiSteps && (getCPMode() == 2000) && (stepPressed >= seq.getStepIndexEdit())) {
								cpSeqLength = stepPressed - seq.getStepIndexEdit() + 1;
							}
							else {
								seq.setStepIndexEdit(stepPressed, sampleRate);
								displayState = DISP_NORMAL; // leave this here, the if has it also, but through the revert mechanism
								if (multiSteps && (getCPMode() == 2000)) {
									multiSteps = false;
									cpSeqLength = 2000;
								}
							}
						}
					}
					else if (attached)
						attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
				}
			}
			
			// Mode button
			if (modeTrigger.process(params[MODE_PARAM].getValue())) {
				if (!attached) {
					if (displayState != DISP_MODE_SEQ && displayState != DISP_MODE_SONG)
						displayState = editingSequence ? DISP_MODE_SEQ : DISP_MODE_SONG;
					else
						displayState = DISP_NORMAL;
				}
				else
					attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
			}
			
			// Clk res/delay button
			if (clkResTrigger.process(params[CLKRES_PARAM].getValue())) {
				if (!attached) {
					if (displayState != DISP_PPQN && displayState != DISP_DELAY)	
						displayState = DISP_PPQN;
					else if (displayState == DISP_PPQN)
						displayState = DISP_DELAY;
					else
						displayState = DISP_NORMAL;
				}
				else
					attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
			}
			
			// Transpose/Rotate button
			if (transposeTrigger.process(params[TRAN_ROT_PARAM].getValue())) {
				if (editingSequence && !attached) {
					if (displayState != DISP_TRANSPOSE && displayState != DISP_ROTATE) {
						displayState = DISP_TRANSPOSE;
					}
					else if (displayState == DISP_TRANSPOSE) {
						displayState = DISP_ROTATE;
					}
					else 
						displayState = DISP_NORMAL;
				}
				else if (attached)
					attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
			}			

			// Begin/End buttons
			if (beginTrigger.process(params[BEGIN_PARAM].getValue())) {
				if (!editingSequence && !attached) {
					seq.setBegin(multiTracks);
					displayState = DISP_NORMAL;
				}
				else if (attached)
					attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
			}	
			if (endTrigger.process(params[END_PARAM].getValue())) {
				if (!editingSequence && !attached) {
					seq.setEnd(multiTracks);
					displayState = DISP_NORMAL;
				}
				else if (attached)
					attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
			}	

			// Rep/Len button
			if (repLenTrigger.process(params[REP_LEN_PARAM].getValue())) {
				if (!attached) {
					if (displayState != DISP_LEN && displayState != DISP_REPS)
						displayState = editingSequence ? DISP_LEN : DISP_REPS;
					else
						displayState = DISP_NORMAL;
				}
				else
					attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
			}	

			// Track Inc/Dec buttons
			if (trackIncTrigger.process(params[TRACKUP_PARAM].getValue())) {
				if (!expanderPresent || std::isnan(consumerMessage[Sequencer::NUM_TRACKS * 2 + 0])) {
					seq.incTrackIndexEdit();
				}
			}
			if (trackDecTrigger.process(params[TRACKDOWN_PARAM].getValue())) {
				if (!expanderPresent || std::isnan(consumerMessage[Sequencer::NUM_TRACKS * 2 + 0])) {
					seq.decTrackIndexEdit();
				}
			}
			// All button
			if (allTrigger.process(params[ALLTRACKS_PARAM].getValue())) {
				if (!expanderPresent || std::isnan(consumerMessage[Sequencer::NUM_TRACKS * 2 + 0])) {
					if (!attached) {
						multiTracks = !multiTracks;
					}
					else {
						multiTracks = false;
						attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
					}
				}
			}	
			
			// Sel button
			if (selTrigger.process(params[SEL_PARAM].getValue())) {
				if (editingSequence && !attached)
					multiSteps = !multiSteps;
				else if (attached) {
					multiSteps = false;
					attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
				}
			}	
			
			// Vel mode button
			if (velEditTrigger.process(params[VEL_EDIT_PARAM].getValue())) {
				if (editingSequence || (attached && running)) {
					if (velEditMode < 2)
						velEditMode++;
					else {
						velEditMode = 0;
					}
				}
			}
			
			// Write mode button
			if (expanderPresent) {
				if (writeModeTrigger.process(consumerMessage[Sequencer::NUM_TRACKS * 2 + 1 + 7 + 1] + consumerMessage[Sequencer::NUM_TRACKS * 2 + 1 + 4])) {//WRITE_SRC_INPUT
					if (editingSequence) {
						if (++writeMode > 2)
							writeMode =0;
					}
				}
			}
		
			// Velocity edit knob 
			float velParamValue = params[VEL_KNOB_PARAM].getValue();
			int newVelocityKnob = (int)std::round(velParamValue * 30.0f);
			if (velParamValue == 0.0f)// true when constructor or dataFromJson() occured
				velocityKnob = newVelocityKnob;
			int deltaVelKnob = newVelocityKnob - velocityKnob;
			if (deltaVelKnob != 0) {
				if (abs(deltaVelKnob) <= 3) {// avoid discontinuous step (initialize for example)
					// any changes in here should may also require right click behavior to be updated in the knob's onMouseDown()
					if (editingSequence) {
						displayState = DISP_NORMAL;
						int mutliStepsCount = multiSteps ? cpSeqLength : 1;
						if (velEditMode == 2) {
							seq.modSlideVal(deltaVelKnob, mutliStepsCount, multiTracks);
						}
						else if (velEditMode == 1) {
							seq.modGatePVal(deltaVelKnob, mutliStepsCount, multiTracks);
						}
						else {
							seq.modVelocityVal(deltaVelKnob, mutliStepsCount, multiTracks);
						}
					}
				}
				velocityKnob = newVelocityKnob;
			}	

						
			// Sequence edit knob 
			float seqParamValue = params[SEQUENCE_PARAM].getValue();
			int newSequenceKnob = (int)std::round(seqParamValue * 7.0f);
			if (seqParamValue == 0.0f)// true when constructor or dataFromJson() occured
				sequenceKnob = newSequenceKnob;
			int deltaSeqKnob = newSequenceKnob - sequenceKnob;
			if (deltaSeqKnob != 0) {
				if (abs(deltaSeqKnob) <= 3) {// avoid discontinuous step (initialize for example)
					// any changes in here should may also require right click behavior to be updated in the knob's onMouseDown()
					if (displayState == DISP_LEN) {
						seq.modLength(deltaSeqKnob, multiTracks);
					}
					else if (displayState == DISP_TRANSPOSE) {
						seq.transposeSeq(deltaSeqKnob, multiTracks);
					}
					else if (displayState == DISP_ROTATE) {
						seq.rotateSeq(deltaSeqKnob, multiTracks);
					}							
					else if (displayState == DISP_REPS) {
						seq.modPhraseReps(deltaSeqKnob, multiTracks);
					}
					else if (displayState == DISP_PPQN || displayState == DISP_DELAY) {
					}
					else {// DISP_NORMAL
						if (editingSequence) {
							int activeTrack = seq.getTrackIndexEdit();
							if (!expanderPresent || std::isnan(consumerMessage[Sequencer::NUM_TRACKS + activeTrack])) {
								seq.moveSeqIndexEdit(deltaSeqKnob);
								if (multiTracks) {
									int newSeq = seq.getSeqIndexEdit();
									for (int trkn = 0; trkn < Sequencer::NUM_TRACKS; trkn++) {
										if (trkn == activeTrack) continue;
										if (!expanderPresent || std::isnan(consumerMessage[Sequencer::NUM_TRACKS + trkn])) {
											seq.setSeqIndexEdit(newSeq, trkn);
										}
									}
								}
							}
						}
						else {// editing song
							if (!attached || (attached && !running))
								seq.modPhraseSeqNum(deltaSeqKnob, multiTracks);
							else
								attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
						}
					}	
				}
				sequenceKnob = newSequenceKnob;
			}	
		

			// Phrase edit knob 
			float phraseParamValue = params[PHRASE_PARAM].getValue();
			int newPhraseKnob = (int)std::round(phraseParamValue * 7.0f);
			if (phraseParamValue == 0.0f)// true when constructor or dataFromJson() occured
				phraseKnob = newPhraseKnob;
			int deltaPhrKnob = newPhraseKnob - phraseKnob;
			if (deltaPhrKnob != 0) {
				if (abs(deltaPhrKnob) <= 3) {// avoid discontinuous step (initialize for example)
					// any changes in here should may also require right click behavior to be updated in the knob's onMouseDown()
					if (displayState == DISP_MODE_SEQ) {
						seq.modRunModeSeq(deltaPhrKnob, multiTracks);
					}
					else if (displayState == DISP_PPQN) {
						seq.modPulsesPerStep(deltaPhrKnob, multiTracks);
					}
					else if (displayState == DISP_DELAY) {
						seq.modDelay(deltaPhrKnob, multiTracks);
					}
					else if (displayState == DISP_MODE_SONG) {
						seq.modRunModeSong(deltaPhrKnob, multiTracks);
					}
					else {
						if (!attached || !running) {
							if (!editingSequence) {
								if (displayState != DISP_PPQN && displayState != DISP_DELAY) {
									seq.movePhraseIndexEdit(deltaPhrKnob);
									if (displayState != DISP_REPS && displayState != DISP_COPY_SONG_CUST)
										displayState = DISP_NORMAL;
									if (!running)
										seq.bringPhraseIndexRunToEdit();							
								}	
							}
						}
						else if (attached)
							attachedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
					}
				}
				phraseKnob = newPhraseKnob;
			}	
				
	
			// Octave buttons
			for (int octn = 0; octn < 7; octn++) {
				if (octTriggers[octn].process(params[OCTAVE_PARAM + octn].getValue())) {
					if (editingSequence) {
						displayState = DISP_NORMAL;
						if (seq.applyNewOctave(6 - octn, multiSteps ? cpSeqLength : 1, sampleRate, multiTracks))
							tiedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
					}
				}
			}
			
			// Keyboard buttons
			for (int keyn = 0; keyn < 12; keyn++) {
				if (keyTriggers[keyn].process(params[KEY_PARAMS + keyn].getValue())) {
					if (editingSequence) {
						displayState = DISP_NORMAL;
						bool autostepClick = paramQuantities[KEY_PARAMS + keyn]->getMaxValue() > 1.5f;// if double-click
						if (isEditingGates()) {
							if (!seq.setGateType(keyn, multiSteps ? cpSeqLength : 1, sampleRate, autostepClick, multiTracks))
								displayState = DISP_PPQN;
						}
						else {
							if (seq.applyNewKey(keyn, multiSteps ? cpSeqLength : 1, sampleRate, autostepClick, multiTracks))
								tiedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
						}							
					}
				}
			}
			
			// Gate, GateProb, Slide and Tied buttons
			if (gate1Trigger.process(params[GATE_PARAM].getValue() + (expanderPresent ? consumerMessage[Sequencer::NUM_TRACKS * 2 + 1 + 0] : 0.0f))) {
				if (editingSequence) {
					displayState = DISP_NORMAL;
					seq.toggleGate(multiSteps ? cpSeqLength : 1, multiTracks);
				}
			}		
			if (gateProbTrigger.process(params[GATE_PROB_PARAM].getValue() + (expanderPresent ? consumerMessage[Sequencer::NUM_TRACKS * 2 + 1 + 1] : 0.0f))) {
				if (editingSequence) {
					displayState = DISP_NORMAL;
					if (seq.toggleGateP(multiSteps ? cpSeqLength : 1, multiTracks)) 
						tiedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
					else if (seq.getAttribute(true).getGateP())
						velEditMode = 1;
				}
			}		
			if (slideTrigger.process(params[SLIDE_BTN_PARAM].getValue() + (expanderPresent ? consumerMessage[Sequencer::NUM_TRACKS * 2 + 1 + 3] : 0.0f))) {
				if (editingSequence) {
					displayState = DISP_NORMAL;
					if (seq.toggleSlide(multiSteps ? cpSeqLength : 1, multiTracks))
						tiedWarning = (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
					else if (seq.getAttribute(true).getSlide())
						velEditMode = 2;
				}
			}		
			if (tiedTrigger.process(params[TIE_PARAM].getValue() + (expanderPresent ? consumerMessage[Sequencer::NUM_TRACKS * 2 + 1 + 2] : 0.0f))) {
				if (editingSequence) {
					displayState = DISP_NORMAL;
					seq.toggleTied(multiSteps ? cpSeqLength : 1, multiTracks);// will clear other attribs if new state is on
				}
			}		
			
			calcClkInSources();
		}// userInputs refresh
		
		
		
		//********** Clock and reset **********
		
		// Clock
		if (running && clockIgnoreOnReset == 0l) {
			bool clockTrigged[Sequencer::NUM_TRACKS];
			for (int trkn = 0; trkn < Sequencer::NUM_TRACKS; trkn++) {
				clockTrigged[trkn] = clockTriggers[trkn].process(inputs[CLOCK_INPUTS + trkn].getVoltage());
				if (clockTrigged[clkInSources[trkn]]) {
					bool stopRequested = seq.clockStep(trkn, editingSequence);
					if (stopRequested) {
						running = false;
						break;
					}
				}
			}
			seq.process();
		}
				
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage() + params[RESET_PARAM].getValue())) {
			seq.initRun(editingSequence);
			resetLight = 1.0f;
			displayState = DISP_NORMAL;
			clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * sampleRate);
			for (int trkn = 0; trkn < Sequencer::NUM_TRACKS; trkn++) {
				clockTriggers[trkn].reset();	
				if (expanderPresent && !std::isnan(consumerMessage[Sequencer::NUM_TRACKS + trkn]) && seqCVmethod == 2)
					seq.setSeqIndexEdit(0, trkn);
			}
		}


		
		//********** Outputs and lights **********
				
		
		
		// CV, gate and velocity outputs
		for (int trkn = 0; trkn < Sequencer::NUM_TRACKS; trkn++) {
			outputs[CV_OUTPUTS + trkn].setVoltage(seq.calcCvOutputAndDecSlideStepsRemain(trkn, running, editingSequence));
			bool retriggingOnReset = (clockIgnoreOnReset != 0l && retrigGatesOnReset);
			outputs[GATE_OUTPUTS + trkn].setVoltage(seq.calcGateOutput(trkn, running && !retriggingOnReset, clockTriggers[clkInSources[trkn]], sampleRate));
			outputs[VEL_OUTPUTS + trkn].setVoltage(seq.calcVelOutput(trkn, running && !retriggingOnReset, editingSequence) - (velocityBipol ? 5.0f : 0.0f));			
		}

		// lights
		if (refresh.processLights()) {
			// Prepare values to visualize
			StepAttributes attributesVisual;
			attributesVisual.clear();
			float cvVisual = 0.0f;
			if (editingSequence || (attached && running)) {
				attributesVisual = seq.getAttribute(editingSequence);
				cvVisual = seq.getCV(editingSequence);
			}
			bool editingGates = isEditingGates();
			
			// Step lights
			for (int stepn = 0; stepn < SequencerKernel::MAX_STEPS; stepn++) {
				float red = 0.0f;
				float green = 0.0f;	
				float white = 0.0f;
				if ((displayState == DISP_COPY_SEQ) || (displayState == DISP_PASTE_SEQ)) {
					int startCP = seq.getStepIndexEdit();
					if (stepn >= startCP && stepn < (startCP + seq.getLengthSeqCPbuf()))
						green = 0.71f;
				}
				else if (displayState == DISP_TRANSPOSE) {
					red = 0.71f;
				}
				else if (displayState == DISP_ROTATE) {
					red = (stepn == seq.getStepIndexEdit() ? 1.0f : (stepn < seq.getLength() ? 0.45f : 0.0f));
				}
				else if (displayState == DISP_LEN) {
					int seqEnd = seq.getLength() - 1;
					if (stepn < seqEnd)
						green = 0.32f;
					else if (stepn == seqEnd)
						green =  1.0f;
				}				


				else {// normal led display (i.e. not length)
					int stepIndexRun = seq.getStepIndexRun(seq.getTrackIndexEdit());
					int stepIndexEdit = seq.getStepIndexEdit();
					if (multiSteps) {
						if (stepn >= stepIndexEdit && stepn < (stepIndexEdit + cpSeqLength))
							red = 0.45f;
					}

					// Run cursor (green)
					if (running) {
						for (int trkn = 0; trkn < Sequencer::NUM_TRACKS; trkn++) {
							bool trknIsUsed = outputs[CV_OUTPUTS + trkn].isConnected() || outputs[GATE_OUTPUTS + trkn].isConnected() || outputs[VEL_OUTPUTS + trkn].isConnected();
							if (stepn == seq.getStepIndexRun(trkn) && trknIsUsed) 
								green = 0.32f;	
						}
						if (green > 0.45f) 
							green = 0.45f;
						if (stepn == stepIndexRun) {
							green = 1.0f;
						}
					}

					// Edit cursor (red)
					if (editingSequence) {
						if (red == 0.0f) // don't overwrite light multistep with full red
							red = (stepn == stepIndexEdit ? 1.0f : 0.0f);
					}

					bool gate = false;
					if (editingSequence)
						gate = seq.getAttribute(true, stepn).getGate();
					else if (!editingSequence && (attached && running))
						gate = seq.getAttribute(false, stepn).getGate();
					white = ((green == 0.0f && red == 0.0f && gate && displayState != DISP_MODE_SEQ && displayState != DISP_PPQN && displayState != DISP_DELAY) ? 0.2f : 0.0f);
					if (editingSequence && white != 0.0f) {
						green = 0.14f; white = 0.0f;
					}
				}

				setGreenRed(STEP_PHRASE_LIGHTS + stepn * 3, green, red);
				lights[STEP_PHRASE_LIGHTS + stepn * 3 + 2].setBrightness(white);
			}
			
			
			// Octave lights
			int octLightIndex = (int) std::floor(cvVisual + 3.0f);
			for (int i = 0; i < 7; i++) {
				float red = 0.0f;
				if (editingSequence || (attached && running)) {
					if (tiedWarning > 0l) {
						bool warningFlashState = calcWarningFlash(tiedWarning, (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips));
						red = (warningFlashState && (i == (6 - octLightIndex))) ? 1.0f : 0.0f;
					}
					else				
						red = (i == (6 - octLightIndex) ? 1.0f : 0.0f);// no lights when outside of range
				}
				lights[OCTAVE_LIGHTS + i].setBrightness(red);
			}
			
			// Keyboard lights
			float keyCV = cvVisual + 10.0f;// to properly handle negative note voltages
			int keyLightIndex = clamp( (int)((keyCV - std::floor(keyCV)) * 12.0f + 0.5f),  0,  11);
			for (int i = 0; i < 12; i++) {
				float green = 0.0f;
				float red = 0.0f;
				if (displayState == DISP_PPQN) {
					if (seq.keyIndexToGateTypeEx(i) != -1) {
						green =	1.0f;
						red = 1.0f;
					}
				}
				else if (editingSequence || (attached && running)) {			
					if (editingGates) {
						green = 1.0f;
						red = 0.45f;
						unsigned long editingType = seq.getEditingType();
						if (editingType > 0ul) {
							if (i == seq.getEditingGateKeyLight()) {
								float dimMult = ((float) editingType / (float)(Sequencer::gateTime * sampleRate / RefreshCounter::displayRefreshStepSkips));
								green *= dimMult;
								red *= dimMult;
							}
							else {
								green = 0.0f; 
								red = 0.0f;
							}
						}
						else {
							int modeLightIndex = attributesVisual.getGateType();
							if (i != modeLightIndex) {// show dim note if gatetype is different than note
								green = 0.0f;
								red = (i == keyLightIndex ? 0.32f : 0.0f);
							}
						}
					}
					else {
						if (tiedWarning > 0l) {
							bool warningFlashState = calcWarningFlash(tiedWarning, (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips));
							red = (warningFlashState && i == keyLightIndex) ? 1.0f : 0.0f;
						}
						else {
							red = seq.calcKeyLightWithEditing(i, keyLightIndex, sampleRate);
						}
					}
				}
				setGreenRed(KEY_LIGHTS + i * 2, green, red);
			}

			// Gate, Tied, GateProb, and Slide lights 
			if (!attributesVisual.getGate())
				setGreenRed(GATE_LIGHT, 0.0f, 0.0f);
			else 
				setGreenRed(GATE_LIGHT, editingGates ? 1.0f : 0.0f, editingGates ? 0.45f : 1.0f);
			if (tiedWarning > 0l) {
				bool warningFlashState = calcWarningFlash(tiedWarning, (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips));
				lights[TIE_LIGHT].setBrightness(warningFlashState ? 1.0f : 0.0f);
			}
			else
				lights[TIE_LIGHT].setBrightness(attributesVisual.getTied() ? 1.0f : 0.0f);			
			if (attributesVisual.getGateP())
				setGreenRed(GATE_PROB_LIGHT, 1.0f, 1.0f);
			else 
				setGreenRed(GATE_PROB_LIGHT, 0.0f, 0.0f);
			lights[SLIDE_LIGHT].setBrightness(attributesVisual.getSlide() ? 1.0f : 0.0f);
			
			// Reset light
			lights[RESET_LIGHT].setSmoothBrightness(resetLight, args.sampleTime * (RefreshCounter::displayRefreshStepSkips >> 2));
			resetLight = 0.0f;
			
			// Run light
			lights[RUN_LIGHT].setBrightness(running ? 1.0f : 0.0f);

			// Attach light
			if (attachedWarning > 0l) {
				bool warningFlashState = calcWarningFlash(attachedWarning, (long) (warningTime * sampleRate / RefreshCounter::displayRefreshStepSkips));
				lights[ATTACH_LIGHT].setBrightness(warningFlashState ? 1.0f : 0.0f);
			}
			else
				lights[ATTACH_LIGHT].setBrightness(attached ? 1.0f : 0.0f);
				
			// Velocity edit mode lights
			if (editingSequence || (attached && running)) {
				setGreenRed(VEL_PROB_LIGHT, velEditMode == 1 ? 1.0f : 0.0f, velEditMode == 1 ? 1.0f : 0.0f);
				lights[VEL_SLIDE_LIGHT].setBrightness(velEditMode == 2 ? 1.0f : 0.0f);
			}
			else {
				setGreenRed(VEL_PROB_LIGHT, 0.0f, 0.0f);
				lights[VEL_SLIDE_LIGHT].setBrightness(0.0f);
			}
			
			// CV writing lights (CV only, CV2 done below for exp panel)
			for (int trkn = 0; trkn < Sequencer::NUM_TRACKS; trkn++) {
				lights[WRITECV_LIGHTS + trkn].setBrightness((editingSequence && ((writeMode & 0x2) == 0) && (multiTracks || seq.getTrackIndexEdit() == trkn)) ? 1.0f : 0.0f);
			}	
			
			
				
			seq.stepEditingGate();// also steps editingType
			if (tiedWarning > 0l)
				tiedWarning--;
			if (attachedWarning > 0l)
				attachedWarning--;
			if (revertDisplay > 0l) {
				if (revertDisplay == 1)
					displayState = DISP_NORMAL;
				revertDisplay--;
			}
			
			// To Expander
			if (rightExpander.module && rightExpander.module->model == modelFoundryExpander) {
				float *producerMessage = reinterpret_cast<float*>(rightExpander.module->leftExpander.producerMessage);
				producerMessage[0] = (float)panelTheme;
				producerMessage[1] = (((writeMode & 0x2) == 0) && editingSequence) ? 1.0f : 0.0f;// lights[WRITE_SEL_LIGHTS + 0].setBrightness()
				producerMessage[2] = (((writeMode & 0x1) == 0) && editingSequence) ? 1.0f : 0.0f;// lights[WRITE_SEL_LIGHTS + 1].setBrightness()
				for (int trkn = 0; trkn < Sequencer::NUM_TRACKS; trkn++) {
					producerMessage[3 + trkn] = (editingSequence && ((writeMode & 0x1) == 0) && (multiTracks || seq.getTrackIndexEdit() == trkn)) ? 1.0f : 0.0f;
				}	
				// no flip request needed here since expander will regularly call flips
			}
		}// lightRefreshCounter
				
		if (clockIgnoreOnReset > 0l)
			clockIgnoreOnReset--;
		
	}// process()
	

	inline void setGreenRed(int id, float green, float red) {
		lights[id + 0].setBrightness(green);
		lights[id + 1].setBrightness(red);
	}
	
	inline void calcClkInSources() {
		// index 0 is always 0 so nothing to do for it
		for (int trkn = 1; trkn < Sequencer::NUM_TRACKS; trkn++) {
			if (inputs[CLOCK_INPUTS + trkn].isConnected())
				clkInSources[trkn] = trkn;
			else 
				clkInSources[trkn] = clkInSources[trkn - 1];
		}
	}
	
	
};



struct FoundryWidget : ModuleWidget {
	SvgPanel* darkPanel;
	
	template <int NUMCHAR>
	struct DisplayWidget : TransparentWidget {// a centered display, must derive from this
		Foundry *module;
		std::shared_ptr<Font> font;
		char displayStr[NUMCHAR + 1];
		static const int textFontSize = 15;
		static constexpr float textOffsetY = 19.9f; // 18.2f for 14 pt, 19.7f for 15pt
		
		void runModeToStr(int num) {
			if (num >= 0 && num < SequencerKernel::NUM_MODES)
				snprintf(displayStr, 4, "%s", SequencerKernel::modeLabels[num].c_str());
		}

		DisplayWidget(Vec _pos, Vec _size, Foundry *_module) {
			box.size = _size;
			box.pos = _pos.minus(_size.div(2));
			module = _module;
			font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}
		
		void draw(const DrawArgs &args) override {
			NVGcolor textColor = prepareDisplay(args.vg, &box, textFontSize);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, -0.4);

			Vec textPos = Vec(5.7f, textOffsetY);
			nvgFillColor(args.vg, nvgTransRGBA(textColor, displayAlpha));
			std::string initString(NUMCHAR,'~');
			nvgText(args.vg, textPos.x, textPos.y, initString.c_str(), NULL);
			nvgFillColor(args.vg, textColor);
			char overlayChar = printText();
			nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
			if (overlayChar != 0) {
				displayStr[0] = overlayChar;
				displayStr[1] = 0;
				nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
			}
		}
		
		virtual char printText() = 0;
	};
	
	struct VelocityDisplayWidget : DisplayWidget<4> {
		VelocityDisplayWidget(Vec _pos, Vec _size, Foundry *_module) : DisplayWidget(_pos, _size, _module) {};

		void draw(const DrawArgs &args) override {
			static const float offsetXfrac = 3.5f;
			NVGcolor textColor = prepareDisplay(args.vg, &box, textFontSize);
			nvgFontFaceId(args.vg, font->handle);
			nvgTextLetterSpacing(args.vg, -0.4);

			Vec textPos = Vec(6.3f, textOffsetY);
			char useRed = printText();
			if (useRed == 1)
				textColor = nvgRGB(0xE0, 0xD0, 0x30);
			nvgFillColor(args.vg, nvgTransRGBA(textColor, displayAlpha));
			nvgText(args.vg, textPos.x, textPos.y, "~", NULL);
			std::string initString(".~~");
			nvgText(args.vg, textPos.x + offsetXfrac, textPos.y, initString.c_str(), NULL);
			if (useRed == 1)
				textColor = nvgRGB(0xFF, 0x2C, 0x20);
			nvgFillColor(args.vg, textColor);
			nvgText(args.vg, textPos.x + offsetXfrac, textPos.y, &displayStr[1], NULL);
			displayStr[1] = 0;
			nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
		}

		char printText() override {
			char ret = 0;// used for a color instead of overlay char. 0 = default (green), 1 = red
			if (module == NULL) {
				snprintf(displayStr, 5, "%3.2f", 5.0f);// Three-wide, two positions after the decimal, left-justified
				displayStr[1] = '.';// in case locals in printf				
			}
			else {
				StepAttributes attributesVisual;
				attributesVisual.clear();
				bool editingSequence = module->isEditingSequence();
				if (editingSequence || (module->attached && module->running)) {
					attributesVisual = module->seq.getAttribute(editingSequence);
				}
				if (editingSequence || (module->attached && module->running)) {
					if (module->velEditMode == 2) {
						int slide = attributesVisual.getSlideVal();						
						if ( slide >= 100)
							snprintf(displayStr, 5, "   1");
						else if (slide >= 1)
							snprintf(displayStr, 5, "0.%02u", (unsigned) slide);
						else
							snprintf(displayStr, 5, "   0");
					}
					else if (module->velEditMode == 1) {
						int prob = attributesVisual.getGatePVal();
						if ( prob >= 100)
							snprintf(displayStr, 5, "   1");
						else if (prob >= 1)
							snprintf(displayStr, 5, "0.%02u", (unsigned) prob);
						else
							snprintf(displayStr, 5, "   0");
					}
					else {
						unsigned int velocityDisp = (unsigned)(attributesVisual.getVelocityVal());
						if (module->velocityMode > 0) {// velocity is 0-127 or semitone
							if (module->velocityMode == 2)// semitone
								printNote(((float)velocityDisp)/12.0f - (module->velocityBipol ? 5.0f : 0.0f), &displayStr[1], true);// given str pointer must be 4 chars (3 display and one end of string)
							else// 0-127
								snprintf(displayStr, 5, " %3u", std::min(velocityDisp, (unsigned int)127));
							displayStr[0] = displayStr[1];
							displayStr[1] = ' ';
						}
						else {// velocity is 0-10V
							float cvValPrint = (float)velocityDisp;
							cvValPrint /= 20.0f;
							if (module->velocityBipol) {						
								if (cvValPrint < 5.0f)
									ret = 1;
								cvValPrint = std::fabs(cvValPrint - 5.0f);
							}
							if (cvValPrint > 9.975f)
								snprintf(displayStr, 5, "  10");
							else if (cvValPrint < 0.025f)
								snprintf(displayStr, 5, "   0");
							else {
								snprintf(displayStr, 5, "%3.2f", cvValPrint);// Three-wide, two positions after the decimal, left-justified
								displayStr[1] = '.';// in case locals in printf
							}							
						}
					}
				}
				else 				
					snprintf(displayStr, 5, "  - ");
			}
			return ret;
		}
	};
	
	// Sequence edit display
	struct SeqEditDisplayWidget : DisplayWidget<3> {
		SeqEditDisplayWidget(Vec _pos, Vec _size, Foundry *_module) : DisplayWidget(_pos, _size, _module) {};
		
		char printText() override {
			if (module == NULL) {
				snprintf(displayStr, 4, "  1");
			}
			else {
				switch (module->displayState) {
				
					case Foundry::DISP_PPQN :
					case Foundry::DISP_DELAY :
						snprintf(displayStr, 4, " - ");
					break;
					case Foundry::DISP_REPS :
						snprintf(displayStr, 4, "R%2u", (unsigned) module->seq.getPhraseReps());
					break;
					case Foundry::DISP_COPY_SEQ :
						snprintf(displayStr, 4, "CPY");
					break;
					case Foundry::DISP_PASTE_SEQ :
						snprintf(displayStr, 4, "PST");
					break;
					case Foundry::DISP_LEN :
						snprintf(displayStr, 4, "L%2u", (unsigned) module->seq.getLength());
					break;
					case Foundry::DISP_TRANSPOSE :
					{
						int tranOffset = module->seq.getTransposeOffset();
						snprintf(displayStr, 4, "+%2u", (unsigned) abs(tranOffset));
						if (tranOffset < 0)
							displayStr[0] = '-';
					}
					break;
					case Foundry::DISP_ROTATE :
					{
						int rotOffset = module->seq.getRotateOffset();
						snprintf(displayStr, 4, ")%2u", (unsigned) abs(rotOffset));
						if (rotOffset < 0)
							displayStr[0] = '(';
					}
					break;
					default :
					{
						if (module->isEditingSequence()) {
							snprintf(displayStr, 4, " %2u", (unsigned)(module->seq.getSeqIndexEdit() + 1) );
						}
						else {
							snprintf(displayStr, 4, " %2u", (unsigned)(module->seq.getPhraseSeq() + 1) );
						}
					}
				}
			}
			return 0;
		}
	};
	
	// Phrase edit display
	struct PhrEditDisplayWidget : DisplayWidget<3> {
		PhrEditDisplayWidget(Vec _pos, Vec _size, Foundry *_module) : DisplayWidget(_pos, _size, _module) {};

		char printText() override {
			char overlayChar = 0;// extra char to print an end symbol overlaped (begin symbol done in here)
			if (module == NULL) {
				snprintf(displayStr, 4, " - ");
			}
			else {
				if (module->displayState == Foundry::DISP_COPY_SONG) {
					snprintf(displayStr, 4, "CPY");
				}
				else if (module->displayState == Foundry::DISP_PASTE_SONG) {
					snprintf(displayStr, 4, "PST");
				}
				else if (module->displayState == Foundry::DISP_MODE_SONG) {
					runModeToStr(module->seq.getRunModeSong());
				}
				else if (module->displayState == Foundry::DISP_PPQN) {
					snprintf(displayStr, 4, "x%2u", (unsigned) module->seq.getPulsesPerStep());
				}
				else if (module->displayState == Foundry::DISP_DELAY) {
					snprintf(displayStr, 4, "D%2u", (unsigned) module->seq.getDelay());
				}
				else if (module->displayState == Foundry::DISP_MODE_SEQ) {
					runModeToStr(module->seq.getRunModeSeq());
				}
				else { 
					if (module->isEditingSequence()) {
						snprintf(displayStr, 4, " - ");
					}
					else { // editing song
						int phrn = module->seq.getPhraseIndexEdit(); // good whether attached or not
						int phrBeg = module->seq.getBegin();
						int phrEnd = module->seq.getEnd();
						snprintf(displayStr, 4, " %2u", (unsigned)(phrn + 1));
						bool begHere = (phrn == phrBeg);
						bool endHere = (phrn == phrEnd);
						if (begHere) {
							displayStr[0] = '{';
							if (endHere)
								overlayChar = '}';
						}
						else if (endHere) {
							displayStr[0] = '}';
							overlayChar = '_';
						}
						else if (phrn < phrEnd && phrn > phrBeg)
							displayStr[0] = '_';
						if (module->displayState == Foundry::DISP_COPY_SONG_CUST) {
							overlayChar = 0;
							displayStr[0] = (time(0) & 0x1) ? 'C' : ' ';
						}
					}
				}
			}
			return overlayChar;
		}
	};
	
	
	struct TrackDisplayWidget : DisplayWidget<2> {
		TrackDisplayWidget(Vec _pos, Vec _size, Foundry *_module) : DisplayWidget(_pos, _size, _module) {};
		char printText() override {
			if (module == NULL) {
				snprintf(displayStr, 3, " A");
			}
			else {
				int trkn = module->seq.getTrackIndexEdit();
				if (module->multiTracks)
					snprintf(displayStr, 3, "%c%c", (unsigned)(trkn + 0x41), ((module->multiTracks && (time(0) & 0x1)) ? '*' : ' '));
				else {
					snprintf(displayStr, 3, " %c", (unsigned)(trkn + 0x41));
				}
			}
			return 0;
		}
	};
	

	struct PanelThemeItem : MenuItem {
		Foundry *module;
		int theme;
		void onAction(const event::Action &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};
	struct ResetOnRunItem : MenuItem {
		Foundry *module;
		void onAction(const event::Action &e) override {
			module->resetOnRun = !module->resetOnRun;
		}
	};
	struct AutoStepLenItem : MenuItem {
		Foundry *module;
		void onAction(const event::Action &e) override {
			module->autostepLen = !module->autostepLen;
		}
	};
	struct AutoseqItem : MenuItem {
		Foundry *module;
		void onAction(const event::Action &e) override {
			module->autoseq = !module->autoseq;
		}
	};
	struct SeqCVmethodItem : MenuItem {
		struct SeqCVmethodSubItem : MenuItem {
			Foundry *module;
			int setVal = 2;
			void onAction(const event::Action &e) override {
				module->seqCVmethod = setVal;
			}
		};
		Foundry *module;
		Menu *createChildMenu() override {
			Menu *menu = new Menu;

			SeqCVmethodSubItem *seqcv0Item = createMenuItem<SeqCVmethodSubItem>("0-10V", CHECKMARK(module->seqCVmethod == 0));
			seqcv0Item->module = this->module;
			seqcv0Item->setVal = 0;
			menu->addChild(seqcv0Item);

			SeqCVmethodSubItem *seqcv1Item = createMenuItem<SeqCVmethodSubItem>("C2-D7#", CHECKMARK(module->seqCVmethod == 1));
			seqcv1Item->module = this->module;
			seqcv1Item->setVal = 1;
			menu->addChild(seqcv1Item);

			SeqCVmethodSubItem *seqcv2Item = createMenuItem<SeqCVmethodSubItem>("Trig-Incr", CHECKMARK(module->seqCVmethod == 2));
			seqcv2Item->module = this->module;
			menu->addChild(seqcv2Item);

			return menu;
		}
	};
	struct VelModeItem : MenuItem {
		struct VelModeSubItem : MenuItem {
			Foundry *module;
			int setVal = 2;
			void onAction(const event::Action &e) override {
				module->velocityMode = setVal;
			}
		};
		Foundry *module;
		Menu *createChildMenu() override {
			Menu *menu = new Menu;

			VelModeSubItem *velMode0Item = createMenuItem<VelModeSubItem>("Volts", CHECKMARK(module->velocityMode == 0));
			velMode0Item->module = this->module;
			velMode0Item->setVal = 0;
			menu->addChild(velMode0Item);

			VelModeSubItem *velMode1Item = createMenuItem<VelModeSubItem>("0-127", CHECKMARK(module->velocityMode == 1));
			velMode1Item->module = this->module;
			velMode1Item->setVal = 1;
			menu->addChild(velMode1Item);

			VelModeSubItem *velMode2Item = createMenuItem<VelModeSubItem>("Notes", CHECKMARK(module->velocityMode == 2));
			velMode2Item->module = this->module;
			menu->addChild(velMode2Item);

			return menu;
		}
	};
	struct VelBipolItem : MenuItem {
		Foundry *module;
		void onAction(const event::Action &e) override {
			module->velocityBipol = !module->velocityBipol;
		}
	};
	struct HoldTiedItem : MenuItem {
		Foundry *module;
		void onAction(const event::Action &e) override {
			module->holdTiedNotes = !module->holdTiedNotes;
		}
	};
	
	struct StopAtEndOfSongItem : MenuItem {
		struct StopAtEndOfSongSubItem : MenuItem {
			Foundry *module;
			int setVal = 4;
			void onAction(const event::Action &e) override {
				module->stopAtEndOfSong = setVal;
			}
		};
		Foundry *module;
		Menu *createChildMenu() override {
			Menu *menu = new Menu;

			StopAtEndOfSongSubItem *stopOItem = createMenuItem<StopAtEndOfSongSubItem>("Off", CHECKMARK(module->stopAtEndOfSong == 4));
			stopOItem->module = this->module;
			menu->addChild(stopOItem);

			StopAtEndOfSongSubItem *stopAItem = createMenuItem<StopAtEndOfSongSubItem>("Track A", CHECKMARK(module->stopAtEndOfSong == 0));
			stopAItem->module = this->module;
			stopAItem->setVal = 0;
			menu->addChild(stopAItem);
			
			StopAtEndOfSongSubItem *stopBItem = createMenuItem<StopAtEndOfSongSubItem>("Track B", CHECKMARK(module->stopAtEndOfSong == 1));
			stopBItem->module = this->module;
			stopBItem->setVal = 1;
			menu->addChild(stopBItem);

			StopAtEndOfSongSubItem *stopCItem = createMenuItem<StopAtEndOfSongSubItem>("Track C", CHECKMARK(module->stopAtEndOfSong == 2));
			stopCItem->module = this->module;
			stopCItem->setVal = 2;
			menu->addChild(stopCItem);

			StopAtEndOfSongSubItem *stopDItem = createMenuItem<StopAtEndOfSongSubItem>("Track D", CHECKMARK(module->stopAtEndOfSong == 3));
			stopDItem->module = this->module;
			stopDItem->setVal = 3;
			menu->addChild(stopDItem);

			return menu;
		}
	};
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		Foundry *module = dynamic_cast<Foundry*>(this->module);
		assert(module);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		PanelThemeItem *lightItem = new PanelThemeItem();
		lightItem->text = lightPanelID;// ImpromptuModular.hpp
		lightItem->module = module;
		lightItem->theme = 0;
		menu->addChild(lightItem);

		PanelThemeItem *darkItem = new PanelThemeItem();
		darkItem->text = darkPanelID;// ImpromptuModular.hpp
		darkItem->module = module;
		darkItem->theme = 1;
		menu->addChild(darkItem);
		
		menu->addChild(createMenuItem<DarkDefaultItem>("Dark as default", CHECKMARK(loadDarkAsDefault())));

		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *settingsLabel = new MenuLabel();
		settingsLabel->text = "Settings";
		menu->addChild(settingsLabel);
		
		ResetOnRunItem *rorItem = createMenuItem<ResetOnRunItem>("Reset on run", CHECKMARK(module->resetOnRun));
		rorItem->module = module;
		menu->addChild(rorItem);

		HoldTiedItem *holdItem = createMenuItem<HoldTiedItem>("Hold tied notes", CHECKMARK(module->holdTiedNotes));
		holdItem->module = module;
		menu->addChild(holdItem);

		StopAtEndOfSongItem *loopItem = createMenuItem<StopAtEndOfSongItem>("Stop at end of song", RIGHT_ARROW);
		loopItem->module = module;
		menu->addChild(loopItem);

		VelBipolItem *bipolItem = createMenuItem<VelBipolItem>("CV2 bipolar", CHECKMARK(module->velocityBipol));
		bipolItem->module = module;
		menu->addChild(bipolItem);
		
		VelModeItem *velItem = createMenuItem<VelModeItem>("CV2 mode", RIGHT_ARROW);
		velItem->module = module;
		menu->addChild(velItem);
		
		SeqCVmethodItem *seqcvItem = createMenuItem<SeqCVmethodItem>("Seq CV in level", RIGHT_ARROW);
		seqcvItem->module = module;
		menu->addChild(seqcvItem);

		AutoStepLenItem *astlItem = createMenuItem<AutoStepLenItem>("AutoStep write bounded by seq length", CHECKMARK(module->autostepLen));
		astlItem->module = module;
		menu->addChild(astlItem);

		AutoseqItem *aseqItem = createMenuItem<AutoseqItem>("AutoSeq when writing via CV inputs", CHECKMARK(module->autoseq));
		aseqItem->module = module;
		menu->addChild(aseqItem);
	}	
		
	struct CKSSNotify : CKSSNoRandom {
		CKSSNotify() {}
		void onChange(const event::Change &e) override {
			if (paramQuantity) {
				Foundry* module = dynamic_cast<Foundry*>(paramQuantity->module);
				module->displayState = Foundry::DISP_NORMAL;
				module->seq.initDelayedSeqNumberRequest();
				if (paramQuantity->paramId != Foundry::KEY_GATE_PARAM) {
					module->multiSteps = false;
				}
			}
			SvgSwitch::onChange(e);		
		}
	};
	struct CPModeSwitch : CKSSThreeInvNoRandom {
		CPModeSwitch() {};
		void onChange(const event::Change &e) override {
			SvgSwitch::onChange(e);	
			if (paramQuantity) {
				Foundry* module = dynamic_cast<Foundry*>(paramQuantity->module);			
				module->cpSeqLength = module->getCPMode();
				if (module->displayState == Foundry::DISP_COPY_SONG_CUST)
					module->displayState = Foundry::DISP_NORMAL;
			}
		}
	};
	// Velocity edit knob
	struct VelocityKnob : IMMediumKnobInf {
		VelocityKnob() {};		
		void onDoubleClick(const event::DoubleClick &e) override {
			if (paramQuantity) {
				Foundry* module = dynamic_cast<Foundry*>(paramQuantity->module);
				// same code structure below as in velocity knob in main step()
				if (module->isEditingSequence()) {
					module->displayState = Foundry::DISP_NORMAL;
					int multiStepsCount = module->multiSteps ? module->getCPMode() : 1;
					if (module->velEditMode == 2) {
						module->seq.initSlideVal(multiStepsCount, module->multiTracks);
					}
					else if (module->velEditMode == 1) {
						module->seq.initGatePVal(multiStepsCount, module->multiTracks);
					}
					else {
						module->seq.initVelocityVal(multiStepsCount, module->multiTracks);
					}
				}
			}
			ParamWidget::onDoubleClick(e);
		}
	};
	// Sequence edit knob
	struct SequenceKnob : IMMediumKnobInf {
		SequenceKnob() {};		
		void onDoubleClick(const event::DoubleClick &e) override {
			if (paramQuantity) {
				Foundry* module = dynamic_cast<Foundry*>(paramQuantity->module);
				// same code structure below as in sequence knob in main step()
				if (module->displayState == Foundry::DISP_LEN) {
					module->seq.initLength(module->multiTracks);
				}
				else if (module->displayState == Foundry::DISP_TRANSPOSE) {
					module->seq.unTransposeSeq(module->multiTracks);
				}
				else if (module->displayState == Foundry::DISP_ROTATE) {
					module->seq.unRotateSeq(module->multiTracks);
				}							
				else if (module->displayState == Foundry::DISP_REPS) {
					module->seq.initPhraseReps(module->multiTracks);
				}
				else if (module->displayState == Foundry::DISP_PPQN || module->displayState == Foundry::DISP_DELAY) {
				}
				else {// DISP_NORMAL
					if (module->isEditingSequence()) {
						for (int trkn = 0; trkn < Sequencer::NUM_TRACKS; trkn++) {
							bool expanderPresent = (module->rightExpander.module && module->rightExpander.module->model == modelFoundryExpander);
							if (!expanderPresent || std::isnan(module->consumerMessage[Sequencer::NUM_TRACKS + trkn])) {
								if (module->multiTracks || (trkn == module->seq.getTrackIndexEdit())) {
									module->seq.setSeqIndexEdit(0, trkn);
								}
							}
						}
					}
					else {// editing song
						if (!module->attached || (module->attached && !module->running))
							module->seq.initPhraseSeqNum(module->multiTracks);
					}
				}	
			}
			ParamWidget::onDoubleClick(e);
		}
	};
	// Phrase edit knob
	struct PhraseKnob : IMMediumKnobInf {
		PhraseKnob() {};		
		void onDoubleClick(const event::DoubleClick &e) override {
			if (paramQuantity) {
				Foundry* module = dynamic_cast<Foundry*>(paramQuantity->module);
				// same code structure below as in phrase knob in main step()
				if (module->displayState == Foundry::DISP_MODE_SEQ) {
					module->seq.initRunModeSeq(module->multiTracks);
				}
				else if (module->displayState == Foundry::DISP_PPQN) {
					module->seq.initPulsesPerStep(module->multiTracks);
				}
				else if (module->displayState == Foundry::DISP_DELAY) {
					module->seq.initDelay(module->multiTracks);
				}
				else if (module->displayState == Foundry::DISP_MODE_SONG) {
					module->seq.initRunModeSong(module->multiTracks);
				}
				else {
					if (!module->attached || !module->running) {
						if (!module->isEditingSequence()) {
							if (module->displayState != Foundry::DISP_PPQN && module->displayState != Foundry::DISP_DELAY) {
								module->seq.setPhraseIndexEdit(0);
								if (module->displayState != Foundry::DISP_REPS && module->displayState != Foundry::DISP_COPY_SONG_CUST)
									module->displayState = Foundry::DISP_NORMAL;
								if (!module->running)
									module->seq.bringPhraseIndexRunToEdit();							
							}	
						}
					}
				}
			}
			ParamWidget::onDoubleClick(e);
		}
	};
		
	FoundryWidget(Foundry *module) {
		setModule(module);
		
		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/Foundry.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/Foundry_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(Vec(15, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(15, 365), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 365), module ? &module->panelTheme : NULL));

		
		
		// ****** Top row ******
		
		static const int rowRulerT0 = 56;
		static const int columnRulerT0 = 25;// Step/Phase LED buttons
		static const int columnRulerT1 = 373;// Select (multi-steps) 
		static const int columnRulerT2 = 426;// Copy-paste-select mode switch
		//static const int columnRulerT3 = 463;// Copy paste buttons (not needed when align to track display)
		static const int columnRulerT5 = 538;// Edit mode switch (and overview switch also)
		static const int stepsOffsetY = 10;
		static const int posLEDvsButton = 26;

		// Step LED buttons
		int posX = columnRulerT0;
		static int spacingSteps = 20;
		static int spacingSteps4 = 4;
		const int numX = SequencerKernel::MAX_STEPS / 2;
		for (int x = 0; x < numX; x++) {
			// First row
			addParam(createParamCentered<LEDButton>(Vec(posX, rowRulerT0 - stepsOffsetY), module, Foundry::STEP_PHRASE_PARAMS + x));
			addChild(createLightCentered<MediumLight<GreenRedWhiteLight>>(Vec(posX, rowRulerT0 - stepsOffsetY), module, Foundry::STEP_PHRASE_LIGHTS + (x * 3)));
			// Second row
			addParam(createParamCentered<LEDButton>(Vec(posX, rowRulerT0 + stepsOffsetY), module, Foundry::STEP_PHRASE_PARAMS + x + numX));
			addChild(createLightCentered<MediumLight<GreenRedWhiteLight>>(Vec(posX, rowRulerT0 + stepsOffsetY), module, Foundry::STEP_PHRASE_LIGHTS + ((x + numX) * 3)));
			// step position to next location and handle groups of four
			posX += spacingSteps;
			if ((x + 1) % 4 == 0)
				posX += spacingSteps4;
		}
		// Sel button
		addParam(createDynamicParamCentered<IMPushButton>(Vec(columnRulerT1, rowRulerT0), module, Foundry::SEL_PARAM, module ? &module->panelTheme : NULL));
		
		// Copy-paste and select mode switch (3 position)
		addParam(createParamCentered<CPModeSwitch>(Vec(columnRulerT2, rowRulerT0), module, Foundry::CPMODE_PARAM));	// 0.0f is top position
		
		// Copy/paste buttons
		// see under Track display
		
		// Main switch
		addParam(createParamCentered<CKSSNotify>(Vec(columnRulerT5, rowRulerT0 + 3), module, Foundry::EDIT_PARAM));// 1.0f is top position

		
		
		// ****** Octave and keyboard area ******
		
		// Octave LED buttons
		static const int octLightsIntY = 20;
		static const int rowRulerOct = 111;
		for (int i = 0; i < 7; i++) {
			addParam(createParamCentered<LEDButton>(Vec(columnRulerT0, rowRulerOct + i * octLightsIntY), module, Foundry::OCTAVE_PARAM + i));
			addChild(createLightCentered<MediumLight<RedLight>>(Vec(columnRulerT0, rowRulerOct + i * octLightsIntY), module, Foundry::OCTAVE_LIGHTS + i));
		}
		
		// Keys and Key lights
		static const int keyNudgeX = 2;
		static const int KeyBlackY = 103;
		static const int KeyWhiteY = 141;
		static const int offsetKeyLEDx = 6;
		static const int offsetKeyLEDy = 16;
		// Black keys and lights
		addParam(createParam<InvisibleKeySmall>(			Vec(65+keyNudgeX, KeyBlackY), module, Foundry::KEY_PARAMS + 1));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(65+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, Foundry::KEY_LIGHTS + 1 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(93+keyNudgeX, KeyBlackY), module, Foundry::KEY_PARAMS + 3));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(93+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, Foundry::KEY_LIGHTS + 3 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(150+keyNudgeX, KeyBlackY), module, Foundry::KEY_PARAMS + 6));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(150+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, Foundry::KEY_LIGHTS + 6 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(178+keyNudgeX, KeyBlackY), module, Foundry::KEY_PARAMS + 8));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(178+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, Foundry::KEY_LIGHTS + 8 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(206+keyNudgeX, KeyBlackY), module, Foundry::KEY_PARAMS + 10));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(206+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, Foundry::KEY_LIGHTS + 10 * 2));
		// White keys and lights
		addParam(createParam<InvisibleKeySmall>(			Vec(51+keyNudgeX, KeyWhiteY), module, Foundry::KEY_PARAMS + 0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(51+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, Foundry::KEY_LIGHTS + 0 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(79+keyNudgeX, KeyWhiteY), module, Foundry::KEY_PARAMS + 2));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(79+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, Foundry::KEY_LIGHTS + 2 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(107+keyNudgeX, KeyWhiteY), module, Foundry::KEY_PARAMS + 4));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(107+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, Foundry::KEY_LIGHTS + 4 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(136+keyNudgeX, KeyWhiteY), module, Foundry::KEY_PARAMS + 5));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(136+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, Foundry::KEY_LIGHTS + 5 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(164+keyNudgeX, KeyWhiteY), module, Foundry::KEY_PARAMS + 7));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(164+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, Foundry::KEY_LIGHTS + 7 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(192+keyNudgeX, KeyWhiteY), module, Foundry::KEY_PARAMS + 9));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(192+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, Foundry::KEY_LIGHTS + 9 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(220+keyNudgeX, KeyWhiteY), module, Foundry::KEY_PARAMS + 11));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(220+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, Foundry::KEY_LIGHTS + 11 * 2));



		// ****** Right side control area ******
		
		static const int rowRulerDisp = 110;
		static const int rowRulerKnobs = 145;
		static const int rowRulerSmallButtons = 189;
		static const int displayWidths = 48; // 43 for 14pt, 46 for 15pt
		static const int displayHeights = 24; // 22 for 14pt, 24 for 15pt
		static const int displaySpacingX = 62;

		// Velocity display
		static const int colRulerVel = 289;
		static const int trkButtonsOffsetX = 14;
		addChild(new VelocityDisplayWidget(Vec(colRulerVel, rowRulerDisp), Vec(displayWidths + 4, displayHeights), module));// 3 characters
		// Velocity knob
		addParam(createDynamicParamCentered<VelocityKnob>(Vec(colRulerVel, rowRulerKnobs), module, Foundry::VEL_KNOB_PARAM, module ? &module->panelTheme : NULL));	
		// Veocity mode button and lights
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerVel - trkButtonsOffsetX - 2, rowRulerSmallButtons), module, Foundry::VEL_EDIT_PARAM, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<MediumLight<GreenRedLight>>(Vec(colRulerVel + 4, rowRulerSmallButtons), module, Foundry::VEL_PROB_LIGHT));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(colRulerVel + 20, rowRulerSmallButtons), module, Foundry::VEL_SLIDE_LIGHT));
		

		// Seq edit display 
		static const int colRulerEditSeq = colRulerVel + displaySpacingX + 3;
		addChild(new SeqEditDisplayWidget(Vec(colRulerEditSeq, rowRulerDisp), Vec(displayWidths, displayHeights), module));// 5 characters
		// Sequence-edit knob
		addParam(createDynamicParamCentered<SequenceKnob>(Vec(colRulerEditSeq, rowRulerKnobs), module, Foundry::SEQUENCE_PARAM, module ? &module->panelTheme : NULL));		
		// Transpose/rotate button
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerEditSeq, rowRulerSmallButtons), module, Foundry::TRAN_ROT_PARAM, module ? &module->panelTheme : NULL));
	
			
		// Phrase edit display 
		static const int colRulerEditPhr = colRulerEditSeq + displaySpacingX + 1;
		addChild(new PhrEditDisplayWidget(Vec(colRulerEditPhr, rowRulerDisp), Vec(displayWidths, displayHeights), module));// 5 characters
		// Phrase knob
		addParam(createDynamicParamCentered<PhraseKnob>(Vec(colRulerEditPhr, rowRulerKnobs), module, Foundry::PHRASE_PARAM, module ? &module->panelTheme : NULL));		
		// Begin/end buttons
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerEditPhr - trkButtonsOffsetX, rowRulerSmallButtons), module, Foundry::BEGIN_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerEditPhr + trkButtonsOffsetX, rowRulerSmallButtons), module, Foundry::END_PARAM, module ? &module->panelTheme : NULL));

				
		// Track display
		static const int colRulerTrk = colRulerEditPhr + displaySpacingX;
		addChild(new TrackDisplayWidget(Vec(colRulerTrk, rowRulerDisp), Vec(displayWidths - 13, displayHeights), module));// 2 characters
		// Track buttons
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerTrk + trkButtonsOffsetX, rowRulerKnobs), module, Foundry::TRACKUP_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerTrk - trkButtonsOffsetX, rowRulerKnobs), module, Foundry::TRACKDOWN_PARAM, module ? &module->panelTheme : NULL));
		// AllTracks button
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerTrk, rowRulerSmallButtons - 12), module, Foundry::ALLTRACKS_PARAM, module ? &module->panelTheme : NULL));
		// Copy/paste buttons
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerTrk - trkButtonsOffsetX, rowRulerT0), module, Foundry::COPY_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<IMPushButton>(Vec(colRulerTrk + trkButtonsOffsetX, rowRulerT0), module, Foundry::PASTE_PARAM, module ? &module->panelTheme : NULL));
	
	
		// Attach button and light
		addParam(createDynamicParamCentered<IMPushButton>(Vec(columnRulerT5 - 10, rowRulerDisp + 14), module, Foundry::ATTACH_PARAM, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(columnRulerT5 + 10, rowRulerDisp + 14), module, Foundry::ATTACH_LIGHT));
	
	
		// ****** Gate and slide section ******
		
		static const int rowRulerMB0 = rowRulerOct + 6 * octLightsIntY;
		static const int columnRulerMB3 = colRulerVel - displaySpacingX;
		static const int columnRulerMB2 = colRulerVel - 2 * displaySpacingX;
		static const int columnRulerMB1 = colRulerVel - 3 * displaySpacingX;
		
		// Key mode LED buttons	
		static const int colRulerKM = 61;
		addParam(createParamCentered<CKSSNotify>(Vec(colRulerKM, rowRulerMB0), module, Foundry::KEY_GATE_PARAM));
		
		// Gate 1 light and button
		addChild(createLightCentered<MediumLight<GreenRedLight>>(Vec(columnRulerMB1 + posLEDvsButton, rowRulerMB0), module, Foundry::GATE_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(columnRulerMB1, rowRulerMB0), module, Foundry::GATE_PARAM, module ? &module->panelTheme : NULL));
		// Tie light and button
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(columnRulerMB2 + posLEDvsButton, rowRulerMB0), module, Foundry::TIE_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(columnRulerMB2, rowRulerMB0), module, Foundry::TIE_PARAM, module ? &module->panelTheme : NULL));
		// Gate 1 probability light and button
		addChild(createLightCentered<MediumLight<GreenRedLight>>(Vec(columnRulerMB3 + posLEDvsButton, rowRulerMB0), module, Foundry::GATE_PROB_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(columnRulerMB3, rowRulerMB0), module, Foundry::GATE_PROB_PARAM, module ? &module->panelTheme : NULL));
		
		// Slide light and button
		addChild(createLightCentered<MediumLight<RedLight>>(Vec(colRulerVel + posLEDvsButton, rowRulerMB0), module, Foundry::SLIDE_LIGHT));		
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(colRulerVel, rowRulerMB0), module, Foundry::SLIDE_BTN_PARAM, module ? &module->panelTheme : NULL));
		// Mode button
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(colRulerEditPhr, rowRulerMB0), module, Foundry::MODE_PARAM, module ? &module->panelTheme : NULL));
		// Rep/Len button
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(colRulerEditSeq, rowRulerMB0), module, Foundry::REP_LEN_PARAM, module ? &module->panelTheme : NULL));
		// Clk res
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(colRulerTrk, rowRulerMB0), module, Foundry::CLKRES_PARAM, module ? &module->panelTheme : NULL));
		
		// Reset and run LED buttons
		static const int colRulerResetRun = columnRulerT5;
		// Run LED bezel and light
		addParam(createParamCentered<LEDBezel>(Vec(colRulerResetRun, rowRulerSmallButtons - 6), module, Foundry::RUN_PARAM));
		addChild(createLightCentered<MuteLight<GreenLight>>(Vec(colRulerResetRun, rowRulerSmallButtons - 6), module, Foundry::RUN_LIGHT));
		// Reset LED bezel and light
		addParam(createParamCentered<LEDBezel>(Vec(colRulerResetRun, rowRulerMB0), module, Foundry::RESET_PARAM));
		addChild(createLightCentered<MuteLight<GreenLight>>(Vec(colRulerResetRun, rowRulerMB0), module, Foundry::RESET_LIGHT));
		


		
		// ****** Bottom two rows ******
		
		static const int rowRulerBLow = 335;
		static const int rowRulerBHigh = 286;
		
		static const int bottomJackSpacingX = 46;
		static const int columnRulerB0 = 32;
		static const int columnRulerB1 = columnRulerB0 + bottomJackSpacingX;
		static const int columnRulerB2 = columnRulerB1 + bottomJackSpacingX;
		static const int columnRulerB3 = columnRulerB2 + bottomJackSpacingX;
		static const int columnRulerB4 = columnRulerB3 + bottomJackSpacingX;
		static const int columnRulerB5 = columnRulerB4 + bottomJackSpacingX;
		
		static const int columnRulerB6 = columnRulerB5 + bottomJackSpacingX;
		static const int columnRulerB7 = columnRulerB6 + bottomJackSpacingX;
		static const int columnRulerB8 = columnRulerB7 + bottomJackSpacingX;
		static const int columnRulerB9 = columnRulerB8 + bottomJackSpacingX;
		static const int columnRulerB10 = columnRulerB9 + bottomJackSpacingX;
		static const int columnRulerB11 = columnRulerB10 + bottomJackSpacingX;
		

		// Autostep and write
		addParam(createParamCentered<CKSSNoRandom>(Vec(columnRulerB0, rowRulerBHigh), module, Foundry::AUTOSTEP_PARAM));		
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB0, rowRulerBLow), true, module, Foundry::WRITE_INPUT, module ? &module->panelTheme : NULL));
	
		// CV IN inputs
		static const int writeLEDoffsetX = 16;
		static const int writeLEDoffsetY = 18;
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB1, rowRulerBHigh), true, module, Foundry::CV_INPUTS + 0, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<RedLight>>(Vec(columnRulerB1 + writeLEDoffsetX, rowRulerBHigh + writeLEDoffsetY), module, Foundry::WRITECV_LIGHTS + 0));
		
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB2, rowRulerBHigh), true, module, Foundry::CV_INPUTS + 2, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<RedLight>>(Vec(columnRulerB2 - writeLEDoffsetX, rowRulerBHigh + writeLEDoffsetY), module, Foundry::WRITECV_LIGHTS + 2));

		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB1, rowRulerBLow), true, module, Foundry::CV_INPUTS + 1, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<RedLight>>(Vec(columnRulerB1 + writeLEDoffsetX, rowRulerBLow - writeLEDoffsetY), module, Foundry::WRITECV_LIGHTS + 1));

		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB2, rowRulerBLow), true, module, Foundry::CV_INPUTS + 3, module ? &module->panelTheme : NULL));
		addChild(createLightCentered<SmallLight<RedLight>>(Vec(columnRulerB2 - writeLEDoffsetX, rowRulerBLow - writeLEDoffsetY), module, Foundry::WRITECV_LIGHTS + 3));
		
		// Clock+CV+Gate+Vel outputs
		// Track A
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB3, rowRulerBHigh), true, module, Foundry::CLOCK_INPUTS + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB4, rowRulerBHigh), false, module, Foundry::CV_OUTPUTS + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB5, rowRulerBHigh), false, module, Foundry::GATE_OUTPUTS + 0, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB6, rowRulerBHigh), false, module, Foundry::VEL_OUTPUTS + 0, module ? &module->panelTheme : NULL));
		// Track C
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB7, rowRulerBHigh), true, module, Foundry::CLOCK_INPUTS + 2, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB8, rowRulerBHigh), false, module, Foundry::CV_OUTPUTS + 2, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB9, rowRulerBHigh), false, module, Foundry::GATE_OUTPUTS + 2, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB10, rowRulerBHigh), false, module, Foundry::VEL_OUTPUTS + 2, module ? &module->panelTheme : NULL));
		//
		// Track B
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB3, rowRulerBLow), true, module, Foundry::CLOCK_INPUTS + 1, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB4, rowRulerBLow), false, module, Foundry::CV_OUTPUTS + 1, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB5, rowRulerBLow), false, module, Foundry::GATE_OUTPUTS + 1, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB6, rowRulerBLow), false, module, Foundry::VEL_OUTPUTS + 1, module ? &module->panelTheme : NULL));
		// Track D
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB7, rowRulerBLow), true, module, Foundry::CLOCK_INPUTS + 3, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB8, rowRulerBLow), false, module, Foundry::CV_OUTPUTS + 3, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB9, rowRulerBLow), false, module, Foundry::GATE_OUTPUTS + 3, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerB10, rowRulerBLow), false, module, Foundry::VEL_OUTPUTS + 3, module ? &module->panelTheme : NULL));

		// Run and reset inputs
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB11, rowRulerBHigh), true, module, Foundry::RUNCV_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerB11, rowRulerBLow), true, module, Foundry::RESET_INPUT, module ? &module->panelTheme : NULL));	
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((Foundry*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((Foundry*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelFoundry = createModel<Foundry, FoundryWidget>("Foundry");

/*CHANGE LOG

1.0.0:
expansion panel replaced by a separate expander module
right-click keys to autostep replaced by double click
add menu option to stop at end of song

*/
