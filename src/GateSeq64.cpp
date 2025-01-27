//***********************************************************************************************
//Gate sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept by Nigel Sixsmith and Marc Boulé
//
//Acknowledgements: please see README.md
//***********************************************************************************************


#include "GateSeq64Util.hpp"


struct GateSeq64 : Module {
	enum ParamIds {
		ENUMS(STEP_PARAMS, 64),
		MODES_PARAM,
		RUN_PARAM,
		CONFIG_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		RESET_PARAM,
		PROB_PARAM,
		EDIT_PARAM,
		SEQUENCE_PARAM,
		CPMODE_PARAM,
		GMODELEFT_PARAM,// no longer used
		GMODERIGHT_PARAM,// no longer used
		ENUMS(GMODE_PARAMS, 8),
		NUM_PARAMS
	};
	enum InputIds {
		CLOCK_INPUT,
		RESET_INPUT,
		RUNCV_INPUT,
		SEQCV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		ENUMS(GATE_OUTPUTS, 4),
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_LIGHTS, 64 * 3),// room for GreenRedWhite
		P_LIGHT,
		RUN_LIGHT,
		RESET_LIGHT,
		ENUMS(GMODE_LIGHTS, 8 * 2),// room for GreenRed
		NUM_LIGHTS
	};
	
	
	// Expander
	float rightMessages[2][6] = {};// messages from expander
		

	// Constants
	enum DisplayStateIds {DISP_GATE, DISP_LENGTH, DISP_MODES};
	static const int MAX_SEQS = 32;
	static const int blinkNumInit = 15;// init number of blink cycles for cursor

	// Need to save, no reset
	int panelTheme;
	
	// Need to save, with reset
	bool autoseq;
	int seqCVmethod;// 0 is 0-10V, 1 is C4-D5#, 2 is TrigIncr
	int pulsesPerStep;// 1 means normal gate mode, alt choices are 4, 6, 12, 24 PPS (Pulses per step)
	bool running;
	int runModeSong;
	int stepIndexEdit;
	int phraseIndexEdit;	
	int sequence;
	int phrases;// 1 to 64
	StepAttributesGS attributes[MAX_SEQS][64];
	SeqAttributesGS sequences[MAX_SEQS];
	int phrase[64];// This is the song (series of phases; a phrase is a patten number)
	bool resetOnRun;
	bool stopAtEndOfSong;

	// No need to save, with reset
	int displayState;
	SeqAttributesGS seqAttribCPbuffer;
	StepAttributesGS attribCPbuffer[64];
	int phraseCPbuffer[64];
	bool seqCopied;
	int countCP;// number of steps to paste (in case CPMODE_PARAM changes between copy and paste)
	int startCP;
	long displayProbInfo;// downward step counter for displayProb feedback
	long infoCopyPaste;// 0 when no info, positive downward step counter timer when copy, negative upward when paste
	long revertDisplay;
	long editingPpqn;// 0 when no info, positive downward step counter timer when editing ppqn
	long blinkCount;// positive upward counter, reset to 0 when max reached
	int blinkNum;// number of blink cycles to do, downward counter
	long editingPhraseSongRunning;// downward step counter
	int stepConfig;
	long clockIgnoreOnReset;
	int phraseIndexRun;
	unsigned long phraseIndexRunHistory;
	int stepIndexRun[4];
	unsigned long stepIndexRunHistory;
	int ppqnCount;
	int gateCode[4];

	// No need to save, no reset
	int stepConfigSync = 0;// 0 means no sync requested, 1 means soft sync (no reset lengths), 2 means hard (reset lengths)
	RefreshCounter refresh;
	float resetLight = 0.0f;
	int sequenceKnob = 0;
	Trigger modesTrigger;
	Trigger stepTriggers[64];
	Trigger copyTrigger;
	Trigger pasteTrigger;
	Trigger runningTrigger;
	Trigger clockTrigger;
	Trigger resetTrigger;
	Trigger writeTrigger;
	Trigger write0Trigger;
	Trigger write1Trigger;
	Trigger stepLTrigger;
	Trigger gModeTriggers[8];
	Trigger probTrigger;
	Trigger seqCVTrigger;
	dsp::BooleanTrigger editingSequenceTrigger;
	HoldDetect modeHoldDetect;
	SeqAttributesGS seqAttribBuffer[MAX_SEQS];// buffer for dataFromJson for thread safety
	int lastStep = 0;// for mouse painting
	bool lastValue = false;// for mouse painting

	
	bool isEditingSequence(void) {return params[EDIT_PARAM].getValue() > 0.5f;}
	int getStepConfig() {// 1 = 4x16 = 0.0f,  2 = 2x32 = 1.0f,  4 = 1x64 = 2.0f
		float paramValue = params[CONFIG_PARAM].getValue();
		if (paramValue < 0.5f) return 1;
		else if (paramValue < 1.5f) return 2;
		return 4;
	}
	
	void fillStepIndexRunVector(int runMode, int len) {
		if (runMode != MODE_RN2) {
			stepIndexRun[1] = stepIndexRun[0];
			stepIndexRun[2] = stepIndexRun[0];
			stepIndexRun[3] = stepIndexRun[0];
		}
		else {
			stepIndexRun[1] = random::u32() % len;
			stepIndexRun[2] = random::u32() % len;
			stepIndexRun[3] = random::u32() % len;
		}
	}
	bool ppsRequirementMet(int gateButtonIndex) {
		return !( (pulsesPerStep < 2) || (pulsesPerStep == 4 && gateButtonIndex > 2) || (pulsesPerStep == 6 && gateButtonIndex <= 2) ); 
	}
		

	GateSeq64() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		rightExpander.producerMessage = rightMessages[0];
		rightExpander.consumerMessage = rightMessages[1];
		
		// must init those that have no-connect info to non-connected, or else mother may read 0.0 init value if ever refresh limiters make it such that after a connection of expander the mother reads before the first pass through the expander's writing code, and this may do something undesired (ex: change track in Foundry on expander connected while track CV jack is empty)
		rightMessages[1][0] = std::numeric_limits<float>::quiet_NaN();
		rightMessages[1][1] = std::numeric_limits<float>::quiet_NaN();
		
		char strBuf[32];
		// Step LED buttons and GateMode lights
		for (int y = 0; y < 4; y++) {
			for (int x = 0; x < 16; x++) {
				snprintf(strBuf, 32, "Step/phrase %i", y * 16 + x + 1);
				configParam(STEP_PARAMS + y * 16 + x, 0.0f, 1.0f, 0.0f, strBuf);
			}
		}
		configParam(GMODE_PARAMS + 2, 0.0f, 1.0f, 0.0f, "Gate type 3");
		configParam(GMODE_PARAMS + 1, 0.0f, 1.0f, 0.0f, "Gate type 2");
		configParam(GMODE_PARAMS + 0, 0.0f, 1.0f, 0.0f, "Gate type 1");
		for (int x = 1; x < 6; x++) {
			snprintf(strBuf, 32, "Gate type %i", 2 + x + 1);
			configParam(GMODE_PARAMS + 2 + x, 0.0f, 1.0f, 0.0f, strBuf);
		}
		configParam(PROB_PARAM, 0.0f, 1.0f, 0.0f, "Probability");
		configParam(RESET_PARAM, 0.0f, 1.0f, 0.0f, "Reset");
		configParam(SEQUENCE_PARAM, -INFINITY, INFINITY, 0.0f, "Sequence");		
		configParam(RUN_PARAM, 0.0f, 1.0f, 0.0f, "Run");
		configParam(MODES_PARAM, 0.0f, 1.0f, 0.0f, "Length / run mode");
		configParam(COPY_PARAM, 0.0f, 1.0f, 0.0f, "Copy");
		configParam(PASTE_PARAM, 0.0f, 1.0f, 0.0f, "Paste");
		configParam(EDIT_PARAM, 0.0f, 1.0f, 1.0f, "Seq/song mode");
		configParam(CONFIG_PARAM, 0.0f, 2.0f, 0.0f, "Configuration (1, 2, 4 chan)");// 0.0f is top position
		configParam(CPMODE_PARAM, 0.0f, 2.0f, 2.0f, "Copy-paste mode");		
		
		for (int i = 0; i < MAX_SEQS; i++)
			seqAttribBuffer[i].init(16, MODE_FWD);
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}

	
	void onReset() override {
		autoseq = false;
		seqCVmethod = 0;
		pulsesPerStep = 1;
		running = true;
		runModeSong = MODE_FWD;
		stepIndexEdit = 0;
		phraseIndexEdit = 0;
		sequence = 0;
		phrases = 4;
		for (int i = 0; i < MAX_SEQS; i++) {
			for (int s = 0; s < 64; s++) {
				attributes[i][s].init();
			}
			sequences[i].init(16 * getStepConfig(), MODE_FWD);
		}
		for (int i = 0; i < 64; i++) {
			phrase[i] = 0;
		}
		resetOnRun = false;
		stopAtEndOfSong = false;
		resetNonJson(false);
	}
	void resetNonJson(bool delayed) {// delay thread sensitive parts (i.e. schedule them so that process() will do them)
		displayState = DISP_GATE;
		seqAttribCPbuffer.init(16, MODE_FWD);
		for (int i = 0; i < 64; i++) {
			attribCPbuffer[i].init();
			phraseCPbuffer[i] = 0;
		}
		seqCopied = true;
		countCP = 64;
		startCP = 0;
		displayProbInfo = 0l;
		infoCopyPaste = 0l;
		revertDisplay = 0l;
		editingPpqn = 0l;
		blinkCount = 0l;
		blinkNum = blinkNumInit;
		editingPhraseSongRunning = 0l;
		if (delayed) {
			stepConfigSync = 1;// signal a sync from dataFromJson so that step will get lengths from seqAttribBuffer			
		}
		else {
			stepConfig = getStepConfig();
			initRun();
		}
	}
	void initRun() {// run button activated, or run edge in run input jack, or stepConfig switch changed, or fromJson()
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * APP->engine->getSampleRate());
		phraseIndexRun = (runModeSong == MODE_REV ? phrases - 1 : 0);
		phraseIndexRunHistory = 0;

		int seq = (isEditingSequence() ? sequence : phrase[phraseIndexRun]);
		stepIndexRun[0] = (sequences[seq].getRunMode() == MODE_REV ? sequences[seq].getLength() - 1 : 0);
		fillStepIndexRunVector(sequences[seq].getRunMode(), sequences[seq].getLength());
		stepIndexRunHistory = 0;

		ppqnCount = 0;
		for (int i = 0; i < 4; i += stepConfig)
			gateCode[i] = calcGateCode(attributes[seq][(i * 16) + stepIndexRun[i]], 0, pulsesPerStep);
	}
	
	
	void onRandomize() override {
		if (isEditingSequence()) {
			for (int s = 0; s < 64; s++) {
				attributes[sequence][s].randomize();
			}
			sequences[sequence].randomize(16 * stepConfig, NUM_MODES);// ok to use stepConfig since CONFIG_PARAM is not randomizable		
		}
	}
	
	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// autoseq
		json_object_set_new(rootJ, "autoseq", json_boolean(autoseq));
		
		// seqCVmethod
		json_object_set_new(rootJ, "seqCVmethod", json_integer(seqCVmethod));

		// pulsesPerStep
		json_object_set_new(rootJ, "pulsesPerStep", json_integer(pulsesPerStep));

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
		// runModeSong
		json_object_set_new(rootJ, "runModeSong3", json_integer(runModeSong));

		// stepIndexEdit
		json_object_set_new(rootJ, "stepIndexEdit", json_integer(stepIndexEdit));
	
		// phraseIndexEdit
		json_object_set_new(rootJ, "phraseIndexEdit", json_integer(phraseIndexEdit));
		
		// sequence
		json_object_set_new(rootJ, "sequence", json_integer(sequence));

		// phrases
		json_object_set_new(rootJ, "phrases", json_integer(phrases));

		// attributes
		json_t *attributesJ = json_array();
		for (int i = 0; i < MAX_SEQS; i++)
			for (int s = 0; s < 64; s++) {
				json_array_insert_new(attributesJ, s + (i * 64), json_integer(attributes[i][s].getAttribute()));
			}
		json_object_set_new(rootJ, "attributes2", attributesJ);// "2" appended so no break patches
		
		// sequences
		json_t *sequencesJ = json_array();
		for (int i = 0; i < MAX_SEQS; i++)
			json_array_insert_new(sequencesJ, i, json_integer(sequences[i].getSeqAttrib()));
		json_object_set_new(rootJ, "sequences", sequencesJ);

		// phrase 
		json_t *phraseJ = json_array();
		for (int i = 0; i < 64; i++)
			json_array_insert_new(phraseJ, i, json_integer(phrase[i]));
		json_object_set_new(rootJ, "phrase2", phraseJ);// "2" appended so no break patches

		// resetOnRun
		json_object_set_new(rootJ, "resetOnRun", json_boolean(resetOnRun));
		
		// stopAtEndOfSong
		json_object_set_new(rootJ, "stopAtEndOfSong", json_boolean(stopAtEndOfSong));

		return rootJ;
	}

	
	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);
		
		// autoseq
		json_t *autoseqJ = json_object_get(rootJ, "autoseq");
		if (autoseqJ)
			autoseq = json_is_true(autoseqJ);

		// seqCVmethod
		json_t *seqCVmethodJ = json_object_get(rootJ, "seqCVmethod");
		if (seqCVmethodJ)
			seqCVmethod = json_integer_value(seqCVmethodJ);

		// pulsesPerStep
		json_t *pulsesPerStepJ = json_object_get(rootJ, "pulsesPerStep");
		if (pulsesPerStepJ)
			pulsesPerStep = json_integer_value(pulsesPerStepJ);

		// running
		json_t *runningJ = json_object_get(rootJ, "running");
		if (runningJ)
			running = json_is_true(runningJ);
		
		// runModeSong
		json_t *runModeSongJ = json_object_get(rootJ, "runModeSong3");
		if (runModeSongJ)
			runModeSong = json_integer_value(runModeSongJ);
		else {// legacy
			runModeSongJ = json_object_get(rootJ, "runModeSong");
			if (runModeSongJ) {
				runModeSong = json_integer_value(runModeSongJ);
				if (runModeSong >= MODE_PEN)// this mode was not present in original version
					runModeSong++;
			}
		}
		
		// stepIndexEdit
		json_t *stepIndexEditJ = json_object_get(rootJ, "stepIndexEdit");
		if (stepIndexEditJ)
			stepIndexEdit = json_integer_value(stepIndexEditJ);
		
		// phraseIndexEdit
		json_t *phraseIndexEditJ = json_object_get(rootJ, "phraseIndexEdit");
		if (phraseIndexEditJ)
			phraseIndexEdit = json_integer_value(phraseIndexEditJ);
		
		// sequence
		json_t *sequenceJ = json_object_get(rootJ, "sequence");
		if (sequenceJ)
			sequence = json_integer_value(sequenceJ);
		
		// phrases
		json_t *phrasesJ = json_object_get(rootJ, "phrases");
		if (phrasesJ)
			phrases = json_integer_value(phrasesJ);
	
		// attributes
		json_t *attributesJ = json_object_get(rootJ, "attributes2");
		if (attributesJ) {
			for (int i = 0; i < MAX_SEQS; i++)
				for (int s = 0; s < 64; s++) {
					json_t *attributesArrayJ = json_array_get(attributesJ, s + (i * 64));
					if (attributesArrayJ)
						attributes[i][s].setAttribute((unsigned short)json_integer_value(attributesArrayJ));
				}
		}
		else {
			attributesJ = json_object_get(rootJ, "attributes");
			if (attributesJ) {
				for (int i = 0; i < 16; i++) {
					for (int s = 0; s < 64; s++) {
						json_t *attributesArrayJ = json_array_get(attributesJ, s + (i * 64));
						if (attributesArrayJ)
							attributes[i][s].setAttribute((unsigned short)json_integer_value(attributesArrayJ));
					}
				}
				for (int i = 16; i < MAX_SEQS; i++) {
					for (int s = 0; s < 64; s++)
						attributes[i][s].init();
				}
			}
		}
		
		// sequences
		json_t *sequencesJ = json_object_get(rootJ, "sequences");
		if (sequencesJ) {
			for (int i = 0; i < MAX_SEQS; i++)
			{
				json_t *sequencesArrayJ = json_array_get(sequencesJ, i);
				if (sequencesArrayJ)
					seqAttribBuffer[i].setSeqAttrib(json_integer_value(sequencesArrayJ));
			}			
		}
		else {// legacy
			int lengths[16];// 1 to 16
			int runModeSeq[16]; 
		
			// runModeSeq
			json_t *runModeSeqJ = json_object_get(rootJ, "runModeSeq3");
			if (runModeSeqJ) {
				for (int i = 0; i < 16; i++)
				{
					json_t *runModeSeqArrayJ = json_array_get(runModeSeqJ, i);
					if (runModeSeqArrayJ)
						runModeSeq[i] = json_integer_value(runModeSeqArrayJ);
				}			
			}		
			else {// legacy
				runModeSeqJ = json_object_get(rootJ, "runModeSeq2");
				if (runModeSeqJ) {
					for (int i = 0; i < 16; i++)
					{
						json_t *runModeSeqArrayJ = json_array_get(runModeSeqJ, i);
						if (runModeSeqArrayJ) {
							runModeSeq[i] = json_integer_value(runModeSeqArrayJ);
							if (runModeSeq[i] >= MODE_PEN)// this mode was not present in version runModeSeq2
								runModeSeq[i]++;
						}
					}			
				}		
			}
			// lengths
			json_t *lengthsJ = json_object_get(rootJ, "lengths");
			if (lengthsJ) {
				for (int i = 0; i < 16; i++)
				{
					json_t *lengthsArrayJ = json_array_get(lengthsJ, i);
					if (lengthsArrayJ)
						lengths[i] = json_integer_value(lengthsArrayJ);
				}			
			}
			
			// now write into new object
			for (int i = 0; i < 16; i++) 
					seqAttribBuffer[i].init(lengths[i], runModeSeq[i]);
			for (int i = 16; i < MAX_SEQS; i++)
					seqAttribBuffer[i].init(16, MODE_FWD);
		}
		
		
		// phrase
		json_t *phraseJ = json_object_get(rootJ, "phrase2");// "2" appended so no break patches
		if (phraseJ) {
			for (int i = 0; i < 64; i++)
			{
				json_t *phraseArrayJ = json_array_get(phraseJ, i);
				if (phraseArrayJ)
					phrase[i] = json_integer_value(phraseArrayJ);
			}
		}
		else {// legacy
			phraseJ = json_object_get(rootJ, "phrase");
			if (phraseJ) {
				for (int i = 0; i < 16; i++)
				{
					json_t *phraseArrayJ = json_array_get(phraseJ, i);
					if (phraseArrayJ)
						phrase[i] = json_integer_value(phraseArrayJ);
				}
				for (int i = 16; i < 64; i++)
					phrase[i] = 0;
			}
		}
		
		// resetOnRun
		json_t *resetOnRunJ = json_object_get(rootJ, "resetOnRun");
		if (resetOnRunJ)
			resetOnRun = json_is_true(resetOnRunJ);

		// stopAtEndOfSong
		json_t *stopAtEndOfSongJ = json_object_get(rootJ, "stopAtEndOfSong");
		if (stopAtEndOfSongJ)
			stopAtEndOfSong = json_is_true(stopAtEndOfSongJ);
		
		resetNonJson(true);
	}

	
	void process(const ProcessArgs &args) override {
		static const float displayProbInfoTime = 3.0f;// seconds
		static const float revertDisplayTime = 0.7f;// seconds
		static const float holdDetectTime = 2.0f;// seconds
		static const float editingPhraseSongRunningTime = 4.0f;// seconds
		static const float editingPpqnTime = 3.5f;// seconds
		float sampleRate = args.sampleRate;
		

		
		//********** Buttons, knobs, switches and inputs **********

		// Edit mode		
		bool editingSequence = isEditingSequence();// true = editing sequence, false = editing song
		
		// Run state button
		if (runningTrigger.process(params[RUN_PARAM].getValue() + inputs[RUNCV_INPUT].getVoltage())) {// no input refresh here, don't want to introduce startup skew
			running = !running;
			if (running) {
				if (resetOnRun) {
					initRun();
				}
			}
			else
				blinkNum = blinkNumInit;
			displayState = DISP_GATE;
		}
		
		if (refresh.processInputs()) {
			// Edit mode blink when change
			if (editingSequenceTrigger.process(editingSequence))
				blinkNum = blinkNumInit;

			// Config switch
			if (stepConfigSync != 0) {
				stepConfig = getStepConfig();
				if (stepConfigSync == 1) {// sync from dataFromJson, so read lengths from seqAttribBuffer
					for (int i = 0; i < MAX_SEQS; i++)
						sequences[i].setSeqAttrib(seqAttribBuffer[i].getSeqAttrib());
				}
				else if (stepConfigSync == 2) {// sync from a real mouse drag event on the switch itself, so init lengths
					for (int i = 0; i < MAX_SEQS; i++)
						sequences[i].setLength(16 * stepConfig);
				}
				initRun();	
				stepConfigSync = 0;
			}
			
			// Seq CV input
			if (inputs[SEQCV_INPUT].isConnected()) {
				if (seqCVmethod == 0) {// 0-10 V
					int newSeq = (int)( inputs[SEQCV_INPUT].getVoltage() * (((float)MAX_SEQS) - 1.0f) / 10.0f + 0.5f );
					sequence = clamp(newSeq, 0, MAX_SEQS - 1);
				}
				else if (seqCVmethod == 1) {// C4-G6
					int newSeq = (int)( (inputs[SEQCV_INPUT].getVoltage()) * 12.0f + 0.5f );
					sequence = clamp(newSeq, 0, MAX_SEQS - 1);
				}
				else {// TrigIncr
					if (seqCVTrigger.process(inputs[SEQCV_INPUT].getVoltage()))
						sequence = clamp(sequence + 1, 0, MAX_SEQS - 1);
				}	
			}
			
			// Copy button
			if (copyTrigger.process(params[COPY_PARAM].getValue())) {
				startCP = editingSequence ? stepIndexEdit : phraseIndexEdit;
				countCP = 64;
				if (params[CPMODE_PARAM].getValue() > 1.5f)// ALL
					startCP = 0;			
				else if (params[CPMODE_PARAM].getValue() < 0.5f)// 4
					countCP = std::min(4, 64 - startCP);
				else// 8
					countCP = std::min(8, 64 - startCP);
				if (editingSequence) {	
					for (int i = 0, s = startCP; i < countCP; i++, s++)
						attribCPbuffer[i] = attributes[sequence][s];
					seqAttribCPbuffer.setSeqAttrib(sequences[sequence].getSeqAttrib());
					seqCopied = true;
				}
				else {
					for (int i = 0, p = startCP; i < countCP; i++, p++)
						phraseCPbuffer[i] = phrase[p];
					seqCopied = false;// so that a cross paste can be detected
				}
				infoCopyPaste = (long) (revertDisplayTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
				displayState = DISP_GATE;
				blinkNum = blinkNumInit;
			}
			// Paste button
			if (pasteTrigger.process(params[PASTE_PARAM].getValue())) {
				infoCopyPaste = (long) (-1 * revertDisplayTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
				startCP = 0;
				if (countCP <= 8) {
					startCP = editingSequence ? stepIndexEdit : phraseIndexEdit;
					countCP = std::min(countCP, 64 - startCP);
				}
				// else nothing to do for ALL
					
				if (editingSequence) {
					if (seqCopied) {// non-crossed paste (seq vs song)
						for (int i = 0, s = startCP; i < countCP; i++, s++)
							attributes[sequence][s] = attribCPbuffer[i];
						if (params[CPMODE_PARAM].getValue() > 1.5f) {// all
							sequences[sequence].setSeqAttrib(seqAttribCPbuffer.getSeqAttrib());
							if (sequences[sequence].getLength() > 16 * stepConfig)
								sequences[sequence].setLength(16 * stepConfig);
						}
					}
					else {// crossed paste to seq (seq vs song)
						if (params[CPMODE_PARAM].getValue() > 1.5f) { // ALL (init steps)
							for (int s = 0; s < 64; s++)
								attributes[sequence][s].init();
						}
						else if (params[CPMODE_PARAM].getValue() < 0.5f) {// 4 (randomize gates)
							for (int s = 0; s < 64; s++)
								if ( (random::u32() & 0x1) != 0)
									attributes[sequence][s].toggleGate();
						}
						else {// 8 (randomize probs)
							for (int s = 0; s < 64; s++) {
								attributes[sequence][s].setGateP((random::u32() & 0x1) != 0);
								attributes[sequence][s].setGatePVal(random::u32() % 101);
							}
						}
						startCP = 0;
						countCP = 64;
						infoCopyPaste *= 2l;
					}
				}
				else {// song
					if (!seqCopied) {// non-crossed paste (seq vs song)
						for (int i = 0, p = startCP; i < countCP; i++, p++)
							phrase[p] = phraseCPbuffer[i] & 0xF;
					}
					else {// crossed paste to song (seq vs song)
						if (params[CPMODE_PARAM].getValue() > 1.5f) { // ALL (init phrases)
							for (int p = 0; p < 64; p++)
								phrase[p] = 0;
						}
						else if (params[CPMODE_PARAM].getValue() < 0.5f) {// 4 (phrases increase from 1 to 64)
							for (int p = 0; p < 64; p++)
								phrase[p] = p;						
						}
						else {// 8 (randomize phrases)
							for (int p = 0; p < 64; p++)
								phrase[p] = random::u32() % 64;
						}
						startCP = 0;
						countCP = 64;
						infoCopyPaste *= 2l;
					}
				}
				displayState = DISP_GATE;
				blinkNum = blinkNumInit;
			}
			
			// Write CV inputs 
			bool expanderPresent = (rightExpander.module && rightExpander.module->model == modelGateSeq64Expander);
			float *messagesFromExpander = (float*)rightExpander.consumerMessage;// could be invalid pointer when !expanderPresent, so read it only when expanderPresent
			if (expanderPresent) {
				bool writeTrig = writeTrigger.process(messagesFromExpander[2]);
				bool write0Trig = write0Trigger.process(messagesFromExpander[4]);
				bool write1Trig = write1Trigger.process(messagesFromExpander[3]);
				if (writeTrig || write0Trig || write1Trig) {
					if (editingSequence) {
						blinkNum = blinkNumInit;
						if (writeTrig) {// higher priority than write0 and write1
							if (!std::isnan(messagesFromExpander[1])) {
								attributes[sequence][stepIndexEdit].setGatePVal(clamp( (int)std::round(messagesFromExpander[1] * 10.0f), 0, 100) );
								attributes[sequence][stepIndexEdit].setGateP(true);
							}
							else{
								attributes[sequence][stepIndexEdit].setGateP(false);
							}
							if (!std::isnan(messagesFromExpander[0]))
								attributes[sequence][stepIndexEdit].setGate(messagesFromExpander[0] >= 1.0f);
						}
						else {// write1 or write0			
							attributes[sequence][stepIndexEdit].setGate(write1Trig);
						}
						// Autostep (after grab all active inputs)
						stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, 64);
						if (stepIndexEdit == 0 && autoseq && !inputs[SEQCV_INPUT].isConnected())
							sequence = moveIndex(sequence, sequence + 1, MAX_SEQS);			
					}
				}
			}

			// Step left CV input
			if (expanderPresent && stepLTrigger.process(messagesFromExpander[5])) {
				if (editingSequence) {
					blinkNum = blinkNumInit;
					stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit - 1, 64);					
				}
			}

			// Step LED button presses
			int stepPressed = -1;
			for (int i = 0; i < 64; i++) {
				if (stepTriggers[i].process(params[STEP_PARAMS + i].getValue()))
					stepPressed = i;
			}		
			if (stepPressed != -1) {
				if (editingSequence) {
					if (displayState == DISP_LENGTH) {
						sequences[sequence].setLength(stepPressed % (16 * stepConfig) + 1);
						revertDisplay = (long) (revertDisplayTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
					}
					else if (displayState == DISP_MODES) {
					}
					else {						
						// version 1
						/*if (!attributes[sequence][stepPressed].getGate()) {// clicked inactive, so turn gate on
							attributes[sequence][stepPressed].setGate(true);
							if (attributes[sequence][stepPressed].getGateP())
								displayProbInfo = (long) (displayProbInfoTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
							else
								displayProbInfo = 0l;
						}
						else {// clicked active
							if (stepIndexEdit == stepPressed && blinkNum != 0) {// only if coming from current step, turn off
								attributes[sequence][stepPressed].setGate(false);
								displayProbInfo = 0l;
							}
							else {
								if (attributes[sequence][stepPressed].getGateP())
									displayProbInfo = (long) (displayProbInfoTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
								else
									displayProbInfo = 0l;
							}
						}*/
						
						// version 2
						if (!attributes[sequence][stepPressed].getGate()) {// clicked inactive, so turn gate on
							attributes[sequence][stepPressed].setGate(true);
							if (attributes[sequence][stepPressed].getGateP())
								displayProbInfo = (long) (displayProbInfoTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
							else
								displayProbInfo = 0l;
						}
						else {// clicked active
							attributes[sequence][stepPressed].setGate(false);
							displayProbInfo = 0l;
						}				
						
						stepIndexEdit = stepPressed;
					}
					blinkNum = blinkNumInit;
				}
				else {// editing song
					if (displayState == DISP_LENGTH) {
						phrases = stepPressed + 1;
						if (phrases > 64) phrases = 64;
						if (phrases < 1 ) phrases = 1;
						revertDisplay = (long) (revertDisplayTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
					}
					else if (displayState == DISP_MODES) {
					}
					else {
						phraseIndexEdit = stepPressed;
						if (running)
							editingPhraseSongRunning = (long) (editingPhraseSongRunningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
						else
							phraseIndexRun = stepPressed;
					}
				}
			}

			// Mode/Length button
			if (modesTrigger.process(params[MODES_PARAM].getValue())) {
				blinkNum = blinkNumInit;
				if (editingPpqn != 0l)
					editingPpqn = 0l;			
				if (displayState == DISP_GATE)
					displayState = DISP_LENGTH;
				else if (displayState == DISP_LENGTH)
					displayState = DISP_MODES;
				else
					displayState = DISP_GATE;
				modeHoldDetect.start((long) (holdDetectTime * sampleRate / RefreshCounter::displayRefreshStepSkips));
			}

			// Prob button
			if (probTrigger.process(params[PROB_PARAM].getValue())) {
				blinkNum = blinkNumInit;
				
				// version 1
				/*if (editingSequence && attributes[sequence][stepIndexEdit].getGate()) {
					if (attributes[sequence][stepIndexEdit].getGateP()) {
						displayProbInfo = 0l;
						attributes[sequence][stepIndexEdit].setGateP(false);
					}
					else {
						displayProbInfo = (long) (displayProbInfoTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
						attributes[sequence][stepIndexEdit].setGateP(true);
					}
				}*/
				
				// version 2
				if (editingSequence) {
					if (attributes[sequence][stepIndexEdit].getGate()) {// gate is on and pressed gatep
						if (attributes[sequence][stepIndexEdit].getGateP()) {
							displayProbInfo = 0l;
							attributes[sequence][stepIndexEdit].setGateP(false);
						}
						else {
							displayProbInfo = (long) (displayProbInfoTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
							attributes[sequence][stepIndexEdit].setGateP(true);							
						}
					}
					else {// gate is off and pressed gatep button
						attributes[sequence][stepIndexEdit].setGate(true);
						displayProbInfo = (long) (displayProbInfoTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
						attributes[sequence][stepIndexEdit].setGateP(true);
					}
				}
				
			}
			
			// GateMode buttons
			for (int i = 0; i < 8; i++) {
				if (gModeTriggers[i].process(params[GMODE_PARAMS + i].getValue())) {
					blinkNum = blinkNumInit;
					if (editingSequence && attributes[sequence][stepIndexEdit].getGate()) {
						if (ppsRequirementMet(i)) {
							editingPpqn = 0l;
							attributes[sequence][stepIndexEdit].setGateMode(i);
						}
						else {
							editingPpqn = (long) (editingPpqnTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
						}
					}
				}
			}

			
			// Sequence knob (Main knob)
			float seqParamValue = params[SEQUENCE_PARAM].getValue();
			int newSequenceKnob = (int)std::round(seqParamValue * 7.0f);
			if (seqParamValue == 0.0f)// true when constructor or dataFromJson() occured
				sequenceKnob = newSequenceKnob;
			int deltaKnob = newSequenceKnob - sequenceKnob;
			if (deltaKnob != 0) {
				if (abs(deltaKnob) <= 3) {// avoid discontinuous step (initialize for example)
					if (displayProbInfo != 0l && editingSequence) {
						blinkNum = blinkNumInit;
						int pval = attributes[sequence][stepIndexEdit].getGatePVal();
						pval += deltaKnob * 2;
						if (pval > 100)
							pval = 100;
						if (pval < 0)
							pval = 0;
						attributes[sequence][stepIndexEdit].setGatePVal(pval);
						displayProbInfo = (long) (displayProbInfoTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
					}
					else if (editingPpqn != 0) {
						pulsesPerStep = indexToPpsGS(ppsToIndexGS(pulsesPerStep) + deltaKnob);// indexToPps() does clamping
						editingPpqn = (long) (editingPpqnTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
					}
					else if (displayState == DISP_MODES) {
						if (editingSequence) {
							sequences[sequence].setRunMode(clamp(sequences[sequence].getRunMode() + deltaKnob, 0, NUM_MODES - 1));
						}
						else {
							runModeSong = clamp(runModeSong + deltaKnob, 0, 6 - 1);
						}
					}
					else if (displayState == DISP_LENGTH) {
						if (editingSequence) {
							sequences[sequence].setLength(clamp(sequences[sequence].getLength() + deltaKnob, 1, (16 * stepConfig)));
						}
						else {
							phrases = clamp(phrases + deltaKnob, 1, 64);
						}
					}
					else {
						if (editingSequence) {
							blinkNum = blinkNumInit;
							if (!inputs[SEQCV_INPUT].isConnected()) {
								sequence = clamp(sequence + deltaKnob, 0, MAX_SEQS - 1);
							}
						}
						else {
							if (editingPhraseSongRunning > 0l || !running) {
								int newPhrase = phrase[phraseIndexEdit] + deltaKnob;
								if (newPhrase < 0)
									newPhrase += (1 - newPhrase / MAX_SEQS) * MAX_SEQS;// newPhrase now positive
								newPhrase = newPhrase % MAX_SEQS;
								phrase[phraseIndexEdit] = newPhrase;
								if (running)
									editingPhraseSongRunning = (long) (editingPhraseSongRunningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
							}
						}	
					}					
				}
				sequenceKnob = newSequenceKnob;
			}		
		}// userInputs refresh
		
		
		
		//********** Clock and reset **********
		
		// Clock
		if (running && clockIgnoreOnReset == 0l) {
			if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
				ppqnCount++;
				if (ppqnCount >= pulsesPerStep)
					ppqnCount = 0;
				
				int newSeq = sequence;// good value when editingSequence, overwrite if not editingSequence
				if (ppqnCount == 0) {
					if (editingSequence) {
						moveIndexRunMode(&stepIndexRun[0], sequences[sequence].getLength(), sequences[sequence].getRunMode(), &stepIndexRunHistory);
					}
					else {
						int oldStepIndexRun0 = stepIndexRun[0];
						int oldStepIndexRun1 = stepIndexRun[1];
						int oldStepIndexRun2 = stepIndexRun[2];
						int oldStepIndexRun3 = stepIndexRun[3];
						if (moveIndexRunMode(&stepIndexRun[0], sequences[phrase[phraseIndexRun]].getLength(), sequences[phrase[phraseIndexRun]].getRunMode(), &stepIndexRunHistory)) {
							int oldPhraseIndexRun = phraseIndexRun;
							bool songLoopOver = moveIndexRunMode(&phraseIndexRun, phrases, runModeSong, &phraseIndexRunHistory);
							// check for end of song if needed
							if (songLoopOver && stopAtEndOfSong) {
								running = false;
								stepIndexRun[0] = oldStepIndexRun0;
								stepIndexRun[1] = oldStepIndexRun1;
								stepIndexRun[2] = oldStepIndexRun2;
								stepIndexRun[3] = oldStepIndexRun3;
								phraseIndexRun = oldPhraseIndexRun;
							}
							else {							
								stepIndexRun[0] = (sequences[phrase[phraseIndexRun]].getRunMode() == MODE_REV ? sequences[phrase[phraseIndexRun]].getLength() - 1 : 0);// must always refresh after phraseIndexRun has changed
							}
						}
						newSeq = phrase[phraseIndexRun];
					}
					if (running)// end of song may have stopped it
						fillStepIndexRunVector(sequences[newSeq].getRunMode(), sequences[newSeq].getLength());
				}
				else {
					if (!editingSequence)
						newSeq = phrase[phraseIndexRun];
				}
				for (int i = 0; i < 4; i += stepConfig) { 
					if (gateCode[i] != -1 || ppqnCount == 0)
						gateCode[i] = calcGateCode(attributes[newSeq][(i * 16) + stepIndexRun[i]], ppqnCount, pulsesPerStep);
				}
			}
		}	
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage() + params[RESET_PARAM].getValue())) {
			initRun();// must be before SEQCV_INPUT below
			resetLight = 1.0f;
			displayState = DISP_GATE;
			clockTrigger.reset();
			if (inputs[SEQCV_INPUT].isConnected() && seqCVmethod == 2)
				sequence = 0;
		}
	
		
		//********** Outputs and lights **********
				
		// Gate outputs
		if (running) {
			bool retriggingOnReset = (clockIgnoreOnReset != 0l && retrigGatesOnReset);
			for (int i = 0; i < 4; i++)
				outputs[GATE_OUTPUTS + i].setVoltage((calcGate(gateCode[i], clockTrigger) && !retriggingOnReset)  ? 10.0f : 0.0f);
		}
		else {// not running (no gates, no need to hear anything)
			for (int i = 0; i < 4; i++)
				outputs[GATE_OUTPUTS + i].setVoltage(0.0f);	
		}

		// lights
		if (refresh.processLights()) {
			// Step LED button lights
			if (infoCopyPaste != 0l) {
				for (int i = 0; i < 64; i++) {
					if (i >= startCP && i < (startCP + countCP))
						setGreenRed3(STEP_LIGHTS + i * 3, 0.71f, 0.0f);
					else
						setGreenRed3(STEP_LIGHTS + i * 3, 0.0f, 0.0f);
				}
			}
			else {
				int row = -1;
				int col = -1;
				for (int i = 0; i < 64; i++) {
					row = i >> (3 + stepConfig);//i / (16 * stepConfig);// optimized (not equivalent code, but in this case has same effect)
					if (stepConfig == 2 && row == 1) 
						row++;
					col = (((stepConfig - 1) << 4) | 0xF) & i;//i % (16 * stepConfig);// optimized			
					if (editingSequence) {
						if (displayState == DISP_LENGTH) {
							if (col < (sequences[sequence].getLength() - 1))
								setGreenRed3(STEP_LIGHTS + i * 3, 0.32f, 0.0f);
							else if (col == (sequences[sequence].getLength() - 1))
								setGreenRed3(STEP_LIGHTS + i * 3, 1.0f, 0.0f);
							else 
								setGreenRed3(STEP_LIGHTS + i * 3, 0.0f, 0.0f);
						}
						else {
							float stepHereOffset = ((stepIndexRun[row] == col) && running) ? 0.71f : 1.0f;
							long blinkCountMarker = (long) (0.67f * sampleRate / RefreshCounter::displayRefreshStepSkips);							
							if (attributes[sequence][i].getGate()) {
								bool blinkEnableOn = (displayState != DISP_MODES) && (blinkCount < blinkCountMarker);
								if (attributes[sequence][i].getGateP()) {
									if (i == stepIndexEdit)// more orange than yellow
										setGreenRed3(STEP_LIGHTS + i * 3, blinkEnableOn ? 1.0f : 0.0f, blinkEnableOn ? 1.0f : 0.0f);
									else// more yellow
										setGreenRed3(STEP_LIGHTS + i * 3, stepHereOffset, stepHereOffset);
								}
								else {
									if (i == stepIndexEdit)
										setGreenRed3(STEP_LIGHTS + i * 3, blinkEnableOn ? 1.0f : 0.0f, 0.0f);
									else
										setGreenRed3(STEP_LIGHTS + i * 3, stepHereOffset, 0.0f);
								}
							}
							else {
								if (i == stepIndexEdit && blinkCount > blinkCountMarker && displayState != DISP_MODES)
									setGreenRed3(STEP_LIGHTS + i * 3, 0.22f, 0.0f);
								else
									setGreenRed3(STEP_LIGHTS + i * 3, ((stepIndexRun[row] == col) && running) ? 0.32f : 0.0f, 0.0f);
							}
						}
					}
					else {// editing Song
						if (displayState == DISP_LENGTH) {
							col = i & 0xF;//i % 16;// optimized
							if (i < (phrases - 1))
								setGreenRed3(STEP_LIGHTS + i * 3, 0.32f, 0.0f);
							else if (i == (phrases - 1))
								setGreenRed3(STEP_LIGHTS + i * 3, 1.0f, 0.0f);
							else 
								setGreenRed3(STEP_LIGHTS + i * 3, 0.0f, 0.0f);
						}
						else {
							float green = (i == (phraseIndexRun) && running) ? 1.0f : 0.0f;
							green += ((running && (col == stepIndexRun[row]) && i != (phraseIndexEdit)) ? 0.32f : 0.0f);
							float red = (i == (phraseIndexEdit) && ((editingPhraseSongRunning > 0l) || !running)) ? 1.0f : 0.0f;
							float white = 0.0f;
							if (green == 0.0f && red == 0.0f && displayState != DISP_MODES){
								white = (attributes[phrase[phraseIndexRun]][i].getGate() ? 0.45f : 0.0f);
								if (attributes[phrase[phraseIndexRun]][i].getGateP())
									white = 0.14f;
							}
							setGreenRed(STEP_LIGHTS + i * 3, std::min(green, 1.0f), red);
							lights[STEP_LIGHTS + i * 3 + 2].setBrightness(white);
						}				
					}
				}
			}
			
			// GateType lights
			if (pulsesPerStep != 1 && editingSequence && attributes[sequence][stepIndexEdit].getGate()) {
				if (editingPpqn != 0) {
					for (int i = 0; i < 8; i++) {
						if (ppsRequirementMet(i))
							setGreenRed(GMODE_LIGHTS + i * 2, 1.0f, 0.0f);
						else 
							setGreenRed(GMODE_LIGHTS + i * 2, 0.0f, 0.0f);
					}
				}
				else {		
					int gmode = attributes[sequence][stepIndexEdit].getGateMode();
					for (int i = 0; i < 8; i++) {
						if (i == gmode) {
							if ( (pulsesPerStep == 4 && i > 2) || (pulsesPerStep == 6 && i <= 2) ) // pps requirement not met
								setGreenRed(GMODE_LIGHTS + i * 2, 0.0f, 1.0f);
							else
								setGreenRed(GMODE_LIGHTS + i * 2, 1.0f, 0.0f);
						}
						else
							setGreenRed(GMODE_LIGHTS + i * 2, 0.0f, 0.0f);
					}
				}
			}
			else {
				for (int i = 0; i < 8; i++) 
					setGreenRed(GMODE_LIGHTS + i * 2, 0.0f, 0.0f);
			}
		
			// Reset light
			lights[RESET_LIGHT].setSmoothBrightness(resetLight, args.sampleTime * (RefreshCounter::displayRefreshStepSkips >> 2));	
			resetLight = 0.0f;

			// Run lights
			lights[RUN_LIGHT].setBrightness(running ? 1.0f : 0.0f);
		
			if (infoCopyPaste != 0l) {
				if (infoCopyPaste > 0l)
					infoCopyPaste --;
				if (infoCopyPaste < 0l)
					infoCopyPaste ++;
			}
			if (displayProbInfo > 0l)
				displayProbInfo--;
			if (modeHoldDetect.process(params[MODES_PARAM].getValue())) {
				displayState = DISP_GATE;
				editingPpqn = (long) (editingPpqnTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
			}
			if (editingPpqn > 0l)
				editingPpqn--;
			if (editingPhraseSongRunning > 0l)
				editingPhraseSongRunning--;
			if (revertDisplay > 0l) {
				if (revertDisplay == 1)
					displayState = DISP_GATE;
				revertDisplay--;
			}
			if (blinkNum > 0) {
				blinkCount++;
				if (blinkCount >= (long) (1.0f * sampleRate / RefreshCounter::displayRefreshStepSkips)) {
					blinkCount = 0l;
					blinkNum--;
				}
			}		
			// To Expander
			if (rightExpander.module && rightExpander.module->model == modelGateSeq64Expander) {
				float *messagesToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
				messagesToExpander[0] = (float)panelTheme;
				rightExpander.module->leftExpander.messageFlipRequested = true;
			}
		}// lightRefreshCounter

		if (clockIgnoreOnReset > 0l)
			clockIgnoreOnReset--;
	}// process()
	
	inline void setGreenRed(int id, float green, float red) {
		lights[id + 0].setBrightness(green);
		lights[id + 1].setBrightness(red);
	}
	inline void setGreenRed3(int id, float green, float red) {
		setGreenRed(id, green, red);
		lights[id + 2].setBrightness(0.0f);
	}

};// GateSeq64 : module

struct GateSeq64Widget : ModuleWidget {
	SvgPanel* darkPanel;
		
	struct SequenceDisplayWidget : TransparentWidget {
		GateSeq64 *module;
		std::shared_ptr<Font> font;
		char displayStr[4];
		
		SequenceDisplayWidget() {
			font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}
		
		void runModeToStr(int num) {
			if (num >= 0 && num < NUM_MODES)
				snprintf(displayStr, 4, "%s", modeLabels[num].c_str());
		}

		void draw(const DrawArgs &args) override {
			NVGcolor textColor = prepareDisplay(args.vg, &box, 18);
			nvgFontFaceId(args.vg, font->handle);
			Vec textPos = Vec(6, 24);
			nvgFillColor(args.vg, nvgTransRGBA(textColor, displayAlpha));
			nvgText(args.vg, textPos.x, textPos.y, "~~~", NULL);
			nvgFillColor(args.vg, textColor);				
			
			if (module == NULL) {
				snprintf(displayStr, 4, "  1");
			}
			else {
				bool editingSequence = module->isEditingSequence();
				if (module->infoCopyPaste != 0l) {
					if (module->infoCopyPaste > 0l)// if copy display "CPY"
						snprintf(displayStr, 4, "CPY");
					else {
						float cpMode = module->params[GateSeq64::CPMODE_PARAM].getValue();
						if (editingSequence && !module->seqCopied) {// cross paste to seq
							if (cpMode > 1.5f)// All = init
								snprintf(displayStr, 4, "CLR");
							else if (cpMode < 0.5f)// 4 = random gate
								snprintf(displayStr, 4, "RGT");
							else// 8 = random probs
								snprintf(displayStr, 4, "RPR");
						}
						else if (!editingSequence && module->seqCopied) {// cross paste to song
							if (cpMode > 1.5f)// All = init
								snprintf(displayStr, 4, "CLR");
							else if (cpMode < 0.5f)// 4 = increase by 1
								snprintf(displayStr, 4, "INC");
							else// 8 = random phrases
								snprintf(displayStr, 4, "RPH");
						}
						else
							snprintf(displayStr, 4, "PST");
					}
				}
				else if (module->displayProbInfo != 0l) {
					int prob = module->attributes[module->sequence][module->stepIndexEdit].getGatePVal();
					if ( prob>= 100)
						snprintf(displayStr, 4, "1,0");
					else if (prob >= 1)
						snprintf(displayStr, 4, ",%02u", (unsigned) prob);
					else
						snprintf(displayStr, 4, "  0");
				}
				else if (module->editingPpqn != 0ul) {
					snprintf(displayStr, 4, "x%2u", (unsigned) module->pulsesPerStep);
				}
				else if (module->displayState == GateSeq64::DISP_LENGTH) {
					if (editingSequence)
						snprintf(displayStr, 4, "L%2u", (unsigned) module->sequences[module->sequence].getLength());
					else
						snprintf(displayStr, 4, "L%2u", (unsigned) module->phrases);
				}
				else if (module->displayState == GateSeq64::DISP_MODES) {
					if (editingSequence)
						runModeToStr(module->sequences[module->sequence].getRunMode());
					else
						runModeToStr(module->runModeSong);
				}
				else {
					int dispVal = 0;
					char specialCode = ' ';
					if (editingSequence)
						dispVal = module->sequence;
					else {
						if (module->editingPhraseSongRunning > 0l || !module->running) {
							dispVal = module->phrase[module->phraseIndexEdit];
							if (module->editingPhraseSongRunning > 0l)
								specialCode = '*';
						}
						else
							dispVal = module->phrase[module->phraseIndexRun];
					}
					snprintf(displayStr, 4, "%c%2u", specialCode, (unsigned)(dispVal) + 1 );
				}
			}
			nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};	
		
	struct PanelThemeItem : MenuItem {
		GateSeq64 *module;
		void onAction(const event::Action &e) override {
			module->panelTheme ^= 0x1;
		}
	};

	struct ResetOnRunItem : MenuItem {
		GateSeq64 *module;
		void onAction(const event::Action &e) override {
			module->resetOnRun = !module->resetOnRun;
		}
	};
	struct StopAtEndOfSongItem : MenuItem {
		GateSeq64 *module;
		void onAction(const event::Action &e) override {
			module->stopAtEndOfSong = !module->stopAtEndOfSong;
		}
	};
	struct AutoseqItem : MenuItem {
		GateSeq64 *module;
		void onAction(const event::Action &e) override {
			module->autoseq = !module->autoseq;
		}
	};
	struct SeqCVmethodItem : MenuItem {
		struct SeqCVmethodSubItem : MenuItem {
			GateSeq64 *module;
			int setVal = 2;
			void onAction(const event::Action &e) override {
				module->seqCVmethod = setVal;
			}
		};
		GateSeq64 *module;
		Menu *createChildMenu() override {
			Menu *menu = new Menu;

			SeqCVmethodSubItem *seqcv0Item = createMenuItem<SeqCVmethodSubItem>("0-10V", CHECKMARK(module->seqCVmethod == 0));
			seqcv0Item->module = this->module;
			seqcv0Item->setVal = 0;
			menu->addChild(seqcv0Item);

			SeqCVmethodSubItem *seqcv1Item = createMenuItem<SeqCVmethodSubItem>("C4-G6", CHECKMARK(module->seqCVmethod == 1));
			seqcv1Item->module = this->module;
			seqcv1Item->setVal = 1;
			menu->addChild(seqcv1Item);

			SeqCVmethodSubItem *seqcv2Item = createMenuItem<SeqCVmethodSubItem>("Trig-Incr", CHECKMARK(module->seqCVmethod == 2));
			seqcv2Item->module = this->module;
			menu->addChild(seqcv2Item);

			return menu;
		}
	};
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		GateSeq64 *module = dynamic_cast<GateSeq64*>(this->module);
		assert(module);

		MenuLabel *themeLabel = new MenuLabel();
		themeLabel->text = "Panel Theme";
		menu->addChild(themeLabel);

		PanelThemeItem *darkItem = createMenuItem<PanelThemeItem>(darkPanelID, CHECKMARK(module->panelTheme));
		darkItem->module = module;
		menu->addChild(darkItem);
		
		menu->addChild(createMenuItem<DarkDefaultItem>("Dark as default", CHECKMARK(loadDarkAsDefault())));

		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *settingsLabel = new MenuLabel();
		settingsLabel->text = "Settings";
		menu->addChild(settingsLabel);
		
		ResetOnRunItem *rorItem = createMenuItem<ResetOnRunItem>("Reset on run", CHECKMARK(module->resetOnRun));
		rorItem->module = module;
		menu->addChild(rorItem);

		StopAtEndOfSongItem *loopItem = createMenuItem<StopAtEndOfSongItem>("Stop at end of song", CHECKMARK(module->stopAtEndOfSong));
		loopItem->module = module;
		menu->addChild(loopItem);

		SeqCVmethodItem *seqcvItem = createMenuItem<SeqCVmethodItem>("Seq CV in level", RIGHT_ARROW);
		seqcvItem->module = module;
		menu->addChild(seqcvItem);
		
		AutoseqItem *aseqItem = createMenuItem<AutoseqItem>("AutoSeq when writing via CV inputs", CHECKMARK(module->autoseq));
		aseqItem->module = module;
		menu->addChild(aseqItem);

		menu->addChild(new MenuLabel());// empty line

		MenuLabel *expLabel = new MenuLabel();
		expLabel->text = "Expander module";
		menu->addChild(expLabel);
		

		InstantiateExpanderItem *expItem = createMenuItem<InstantiateExpanderItem>("Add expander (4HP right side)", "");
		expItem->model = modelGateSeq64Expander;
		expItem->posit = box.pos.plus(math::Vec(box.size.x,0));
		menu->addChild(expItem);	
	}	
	
	struct CKSSThreeInvNotify : CKSSThreeInvNoRandom {
		CKSSThreeInvNotify() {}
		void onDragStart(const event::DragStart &e) override {
			Switch::onDragStart(e);
			if (paramQuantity) {
				GateSeq64* module = dynamic_cast<GateSeq64*>(paramQuantity->module);
				module->stepConfigSync = 2;// signal a sync from switch so that steps get initialized
			}
		}	
	};
	
	struct SequenceKnob : IMBigKnobInf {
		SequenceKnob() {};		
		void onDoubleClick(const event::DoubleClick &e) override {
			if (paramQuantity) {
				GateSeq64* module = dynamic_cast<GateSeq64*>(paramQuantity->module);
				// same code structure below as in sequence knob in main step()
				bool editingSequence = module->isEditingSequence();
				if (module->displayProbInfo != 0l && editingSequence) {
					//blinkNum = blinkNumInit;
					module->attributes[module->sequence][module->stepIndexEdit].setGatePVal(50);
					//displayProbInfo = (long) (displayProbInfoTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
				}
				else if (module->editingPpqn != 0) {
					module->pulsesPerStep = 1;
					//editingPpqn = (long) (editingPpqnTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
				}
				else if (module->displayState == GateSeq64::DISP_MODES) {
					if (editingSequence) {
						module->sequences[module->sequence].setRunMode(MODE_FWD);
					}
					else {
						module->runModeSong = MODE_FWD;
					}
				}
				else if (module->displayState == GateSeq64::DISP_LENGTH) {
					if (editingSequence) {
						module->sequences[module->sequence].setLength(16 * module->stepConfig);
					}
					else {
						module->phrases = 4;
					}
				}
				else {
					if (editingSequence) {
						//blinkNum = blinkNumInit;
						if (!module->inputs[GateSeq64::SEQCV_INPUT].isConnected()) {
							module->sequence = 0;
						}
					}
					else {
						if (module->editingPhraseSongRunning > 0l || !module->running) {
							module->phrase[module->phraseIndexEdit] = 0;
							// if (running)
								// editingPhraseSongRunning = (long) (editingPhraseSongRunningTime * sampleRate / RefreshCounter::displayRefreshStepSkips);
						}
					}	
				}			
			}
			ParamWidget::onDoubleClick(e);
		}
	};		

	struct LEDButtonGS : LEDButton {
		LEDButtonGS() {};
		void onDragStart(const event::DragStart &e) override {
			if (paramQuantity) {
				GateSeq64 *module = dynamic_cast<GateSeq64*>(paramQuantity->module);
				if (module->isEditingSequence() && module->displayState != GateSeq64::DISP_LENGTH && module->displayState != GateSeq64::DISP_MODES) {
					int step = paramQuantity->paramId - GateSeq64::STEP_PARAMS;
					if ( (step >= 0) && (step < 64) ) {
						module->lastStep = step;
						module->lastValue = !module->attributes[module->sequence][step].getGate();// negate since event to toggle has yet to occur
					}
				}
			}
			LEDButton::onDragStart(e);
		}
		void onDragEnter(const event::DragEnter &e) override {
			LEDButtonGS *orig = dynamic_cast<LEDButtonGS*>(e.origin);
			if (orig && paramQuantity) {
				GateSeq64 *module = dynamic_cast<GateSeq64*>(paramQuantity->module);
				if (module->isEditingSequence() && module->displayState != GateSeq64::DISP_LENGTH && module->displayState != GateSeq64::DISP_MODES) {
					int step = paramQuantity->paramId - GateSeq64::STEP_PARAMS;
					if ( (step != module->lastStep) && (step >= 0) && (step < 64)) {
						module->attributes[module->sequence][step].setGate(module->lastValue);
					}
				}
			}
			LEDButton::onDragEnter(e);
		};
	};

	GateSeq64Widget(GateSeq64 *module) {
		setModule(module);		

		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/GateSeq64.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/GateSeq64_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(Vec(15, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(15, 365), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 365), module ? &module->panelTheme : NULL));
		
		
		// ****** Top portion (LED button array and gate type LED buttons) ******
		
		static const int rowRuler0 = 32;
		static const int spacingRows = 32;
		static const int colRulerSteps = 15;
		static const int spacingSteps = 20;
		static const int spacingSteps4 = 4;
		
		
		// Step LED buttons and GateMode lights
		for (int y = 0; y < 4; y++) {
			int posX = colRulerSteps;
			for (int x = 0; x < 16; x++) {
				addParam(createParam<LEDButtonGS>(Vec(posX, rowRuler0 + 8 + y * spacingRows - 4.4f), module, GateSeq64::STEP_PARAMS + y * 16 + x));
				addChild(createLight<MediumLight<GreenRedWhiteLight>>(Vec(posX + 4.4f, rowRuler0 + 8 + y * spacingRows), module, GateSeq64::STEP_LIGHTS + (y * 16 + x) * 3));
				posX += spacingSteps;
				if ((x + 1) % 4 == 0)
					posX += spacingSteps4;
			}
		}
					
		// Gate type LED buttons (bottom left to top left to top right)
		static const int rowRulerG0 = 166;
		static const int rowSpacingG = 26;
		static const int colSpacingG = 56;
		static const int colRulerG0 = 15 + 28;
		
		addParam(createParam<LEDButton>(Vec(colRulerG0, rowRulerG0 + rowSpacingG * 2 - 4.4f), module, GateSeq64::GMODE_PARAMS + 2));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(colRulerG0 + 4.4f, rowRulerG0 + rowSpacingG * 2), module, GateSeq64::GMODE_LIGHTS + 2 * 2));
		addParam(createParam<LEDButton>(Vec(colRulerG0, rowRulerG0 + rowSpacingG - 4.4f), module, GateSeq64::GMODE_PARAMS + 1));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(colRulerG0 + 4.4f, rowRulerG0 + rowSpacingG), module, GateSeq64::GMODE_LIGHTS + 1 * 2));
		addParam(createParam<LEDButton>(Vec(colRulerG0, rowRulerG0 - 4.4f), module, GateSeq64::GMODE_PARAMS + 0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(colRulerG0 + 4.4f, rowRulerG0), module, GateSeq64::GMODE_LIGHTS + 0 * 2));		
		for (int x = 1; x < 6; x++) {
			addParam(createParam<LEDButton>(Vec(colRulerG0 + colSpacingG * x, rowRulerG0 - 4.4f), module, GateSeq64::GMODE_PARAMS + 2 + x));
			addChild(createLight<MediumLight<GreenRedLight>>(Vec(colRulerG0 + colSpacingG * x + 4.4f, rowRulerG0), module, GateSeq64::GMODE_LIGHTS + (2 + x) * 2));
		}
		
		
		
		
		// ****** 5x3 Main bottom half Control section ******
		
		static const int colRulerC0 = 25;
		static const int colRulerC1 = 78;
		static const int colRulerC2 = 126;
		static const int colRulerC3 = 189;
		static const int colRulerC4 = 241;
		static const int rowRulerC0 = 206; 
		static const int rowRulerSpacing = 58;
		static const int rowRulerC1 = rowRulerC0 + rowRulerSpacing;
		static const int rowRulerC2 = rowRulerC1 + rowRulerSpacing;
				
		
		// Clock input
		addInput(createDynamicPort<IMPort>(Vec(colRulerC0, rowRulerC1), true, module, GateSeq64::CLOCK_INPUT, module ? &module->panelTheme : NULL));
		// Reset CV
		addInput(createDynamicPort<IMPort>(Vec(colRulerC0, rowRulerC2), true, module, GateSeq64::RESET_INPUT, module ? &module->panelTheme : NULL));
		
				
		// Prob button
		addParam(createDynamicParam<IMBigPushButton>(Vec(colRulerC1 + offsetCKD6b, rowRulerC0 + offsetCKD6b), module, GateSeq64::PROB_PARAM, module ? &module->panelTheme : NULL));
		// Reset LED bezel and light
		addParam(createParam<LEDBezel>(Vec(colRulerC1 + offsetLEDbezel, rowRulerC1 + offsetLEDbezel), module, GateSeq64::RESET_PARAM));
		addChild(createLight<MuteLight<GreenLight>>(Vec(colRulerC1 + offsetLEDbezel + offsetLEDbezelLight, rowRulerC1 + offsetLEDbezel + offsetLEDbezelLight), module, GateSeq64::RESET_LIGHT));
		// Seq CV
		addInput(createDynamicPort<IMPort>(Vec(colRulerC1, rowRulerC2), true, module, GateSeq64::SEQCV_INPUT, module ? &module->panelTheme : NULL));
		
		// Sequence knob
		addParam(createDynamicParam<SequenceKnob>(Vec(colRulerC2 + 1 + offsetIMBigKnob, rowRulerC0 + offsetIMBigKnob), module, GateSeq64::SEQUENCE_PARAM, module ? &module->panelTheme : NULL));		
		// Run LED bezel and light
		addParam(createParam<LEDBezel>(Vec(colRulerC2 + offsetLEDbezel, rowRulerC1 + offsetLEDbezel), module, GateSeq64::RUN_PARAM));
		addChild(createLight<MuteLight<GreenLight>>(Vec(colRulerC2 + offsetLEDbezel + offsetLEDbezelLight, rowRulerC1 + offsetLEDbezel + offsetLEDbezelLight), module, GateSeq64::RUN_LIGHT));
		// Run CV
		addInput(createDynamicPort<IMPort>(Vec(colRulerC2, rowRulerC2), true, module, GateSeq64::RUNCV_INPUT, module ? &module->panelTheme : NULL));

		
		// Sequence display
		SequenceDisplayWidget *displaySequence = new SequenceDisplayWidget();
		displaySequence->box.pos = Vec(colRulerC3 - 15, rowRulerC0 + vOffsetDisplay);
		displaySequence->box.size = Vec(55, 30);// 3 characters
		displaySequence->module = module;
		addChild(displaySequence);
		// Modes button
		addParam(createDynamicParam<IMBigPushButton>(Vec(colRulerC3 + offsetCKD6b, rowRulerC1 + offsetCKD6b), module, GateSeq64::MODES_PARAM, module ? &module->panelTheme : NULL));
		// Copy/paste buttons
		addParam(createDynamicParam<IMPushButton>(Vec(colRulerC3 - 10, rowRulerC2 + offsetTL1105), module, GateSeq64::COPY_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<IMPushButton>(Vec(colRulerC3 + 20, rowRulerC2 + offsetTL1105), module, GateSeq64::PASTE_PARAM, module ? &module->panelTheme : NULL));
		

		// Seq/Song selector
		addParam(createParam<CKSSNoRandom>(Vec(colRulerC4 + 2 + hOffsetCKSS, rowRulerC0 + vOffsetCKSS), module, GateSeq64::EDIT_PARAM));
		// Config switch (3 position)
		addParam(createParam<CKSSThreeInvNotify>(Vec(colRulerC4 + 2 + hOffsetCKSS, rowRulerC1 - 2 + vOffsetCKSSThree), module, GateSeq64::CONFIG_PARAM));// 0.0f is top position
		// Copy paste mode
		addParam(createParam<CKSSThreeInvNoRandom>(Vec(colRulerC4 + 2 + hOffsetCKSS, rowRulerC2 + vOffsetCKSSThree), module, GateSeq64::CPMODE_PARAM));

		// Outputs
		for (int iSides = 0; iSides < 4; iSides++)
			addOutput(createDynamicPort<IMPort>(Vec(311, rowRulerC0 + iSides * 40), false, module, GateSeq64::GATE_OUTPUTS + iSides, module ? &module->panelTheme : NULL));
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((GateSeq64*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((GateSeq64*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelGateSeq64 = createModel<GateSeq64, GateSeq64Widget>("Gate-Seq-64");
