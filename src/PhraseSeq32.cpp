//***********************************************************************************************
//Multi-phrase 32 step sequencer module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and AudibleInstruments plugins by Andrew Belt 
//and graphics from the Component Library by Wes Milholen 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module inspired by the SA-100 Stepper Acid sequencer by Transistor Sounds Labs
//
//Acknowledgements: please see README.md
//***********************************************************************************************


#include "PhraseSeqUtil.hpp"


struct PhraseSeq32 : Module {
	enum ParamIds {
		LEFT_PARAM,
		RIGHT_PARAM,
		RIGHT8_PARAM,// not used
		EDIT_PARAM,
		SEQUENCE_PARAM,
		RUN_PARAM,
		COPY_PARAM,
		PASTE_PARAM,
		RESET_PARAM,
		ENUMS(OCTAVE_PARAM, 7),
		GATE1_PARAM,
		GATE2_PARAM,
		SLIDE_BTN_PARAM,
		SLIDE_KNOB_PARAM,
		ATTACH_PARAM,
		AUTOSTEP_PARAM,
		ENUMS(KEY_PARAMS, 12),
		RUNMODE_PARAM,
		TRAN_ROT_PARAM,
		GATE1_KNOB_PARAM,
		GATE1_PROB_PARAM,
		TIE_PARAM,// Legato
		CPMODE_PARAM,
		ENUMS(STEP_PHRASE_PARAMS, 32),
		CONFIG_PARAM,
		KEYNOTE_PARAM,
		KEYGATE_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		WRITE_INPUT,
		CV_INPUT,
		RESET_INPUT,
		CLOCK_INPUT,
		LEFTCV_INPUT,
		RIGHTCV_INPUT,
		RUNCV_INPUT,
		SEQCV_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		CVA_OUTPUT,
		GATE1A_OUTPUT,
		GATE2A_OUTPUT,
		CVB_OUTPUT,
		GATE1B_OUTPUT,
		GATE2B_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		ENUMS(STEP_PHRASE_LIGHTS, 32 * 3),// room for GreenRedWhite
		ENUMS(OCTAVE_LIGHTS, 7),// octaves 1 to 7
		ENUMS(KEY_LIGHTS, 12 * 2),// room for GreenRed
		RUN_LIGHT,
		RESET_LIGHT,
		ENUMS(GATE1_LIGHT, 2),// room for GreenRed
		ENUMS(GATE2_LIGHT, 2),// room for GreenRed
		SLIDE_LIGHT,
		ATTACH_LIGHT,
		ENUMS(GATE1_PROB_LIGHT, 2),// room for GreenRed
		TIE_LIGHT,
		KEYNOTE_LIGHT,
		ENUMS(KEYGATE_LIGHT, 2),// room for GreenRed
		NUM_LIGHTS
	};
	
	
	
	// Expander
	float consumerMessage[5] = {};// this module must read from here
	float producerMessage[5] = {};// expander will write into here


	// Constants
	enum DisplayStateIds {DISP_NORMAL, DISP_MODE, DISP_LENGTH, DISP_TRANSPOSE, DISP_ROTATE};
	static constexpr float CONFIG_PARAM_INIT_VALUE = 0.0f;// so that module constructor is coherent with widget initialization, since module created before widget


	// Need to save
	int panelTheme = 0;
	int expansion = 0;
	bool autoseq;
	bool autostepLen;
	bool holdTiedNotes;
	int seqCVmethod;// 0 is 0-10V, 1 is C4-G6, 2 is TrigIncr
	int pulsesPerStep;// 1 means normal gate mode, alt choices are 4, 6, 12, 24 PPS (Pulses per step)
	bool running;
	SeqAttributes sequences[32];
	int runModeSong;
	int seqIndexEdit;
	int phrase[32];// This is the song (series of phases; a phrase is a patten number)
	int phrases;//1 to 32
	float cv[32][32];// [-3.0 : 3.917]. First index is patten number, 2nd index is step
	StepAttributes attributes[32][32];// First index is patten number, 2nd index is step (see enum AttributeBitMasks for details)
	bool resetOnRun;
	bool attached;
	bool stopAtEndOfSong;

	// No need to save
	int stepIndexEdit;
	int stepIndexRun[2];
	int phraseIndexEdit;
	int phraseIndexRun;
	long infoCopyPaste;// 0 when no info, positive downward step counter timer when copy, negative upward when paste
	unsigned long editingGate;// 0 when no edit gate, downward step counter timer when edit gate
	float editingGateCV;// no need to initialize, this is a companion to editingGate (output this only when editingGate > 0)
	int editingGateKeyLight;// no need to initialize, this is a companion to editingGate (use this only when editingGate > 0)
	int editingChannel;// 0 means channel A, 1 means channel B. no need to initialize, this is a companion to editingGate
	unsigned long editingType;// similar to editingGate, but just for showing remanent gate type (nothing played); uses editingGateKeyLight
	unsigned long stepIndexRunHistory;
	unsigned long phraseIndexRunHistory;
	int displayState;
	unsigned long slideStepsRemain[2];// 0 when no slide under way, downward step counter when sliding
	float slideCVdelta[2];// no need to initialize, this is a companion to slideStepsRemain
	float cvCPbuffer[32];// copy paste buffer for CVs
	StepAttributes attribCPbuffer[32];
	SeqAttributes seqAttribCPbuffer;
	bool seqCopied;
	int phraseCPbuffer[32];
	int countCP;// number of steps to paste (in case CPMODE_PARAM changes between copy and paste)
	int startCP;
	long clockIgnoreOnReset;
	unsigned long clockPeriod;// counts number of step() calls upward from last clock (reset after clock processed)
	long tiedWarning;// 0 when no warning, positive downward step counter timer when warning
	long attachedWarning;// 0 when no warning, positive downward step counter timer when warning
	int gate1Code[2];
	int gate2Code[2];
	bool attachedChanB;
	long revertDisplay;
	long editingGateLength;// 0 when no info, positive when gate1, negative when gate2
	long lastGateEdit;
	long editingPpqn;// 0 when no info, positive downward step counter timer when editing ppqn
	int ppqnCount;
	int stepConfig;
	

	int stepConfigSync = 0;// 0 means no sync requested, 1 means soft sync (no reset lengths), 2 means hard (reset lengths)
	unsigned int lightRefreshCounter = 0;
	float resetLight = 0.0f;
	int sequenceKnob = 0;
	Trigger resetTrigger;
	Trigger leftTrigger;
	Trigger rightTrigger;
	Trigger runningTrigger;
	Trigger clockTrigger;
	Trigger octTriggers[7];
	Trigger octmTrigger;
	Trigger gate1Trigger;
	Trigger gate1ProbTrigger;
	Trigger gate2Trigger;
	Trigger slideTrigger;
	Trigger keyTriggers[12];
	Trigger writeTrigger;
	Trigger attachedTrigger;
	Trigger copyTrigger;
	Trigger pasteTrigger;
	Trigger modeTrigger;
	Trigger rotateTrigger;
	Trigger transposeTrigger;
	Trigger tiedTrigger;
	Trigger stepTriggers[32];
	Trigger keyNoteTrigger;
	Trigger keyGateTrigger;
	Trigger seqCVTrigger;
	HoldDetect modeHoldDetect;
	SeqAttributes seqAttribBuffer[32];// buffer from Json for thread safety


	inline bool isEditingSequence(void) {return params[EDIT_PARAM].getValue() > 0.5f;}
	inline int getStepConfig(float paramValue) {// 1 = 2x16 = 1.0f,  2 = 1x32 = 0.0f
		return (paramValue > 0.5f) ? 1 : 2;
	}

	
	inline void fillStepIndexRunVector(int runMode, int len) {
		if (runMode != MODE_RN2) 
			stepIndexRun[1] = stepIndexRun[0];
		else
			stepIndexRun[1] = random::u32() % len;
	}
	
	inline void moveStepIndexEdit(int delta, bool _autostepLen) {// 2nd param is for rotate that uses this method also
		if (stepConfig == 2 || !_autostepLen) // 32
			stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + delta, _autostepLen ? sequences[seqIndexEdit].getLength() : 32);
		else {// here 1x16 and _autostepLen limit wanted
			if (stepIndexEdit < 16) {
				stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + delta, sequences[seqIndexEdit].getLength());
				if (stepIndexEdit == 0) stepIndexEdit = 16;
			}
			else
				stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + delta, sequences[seqIndexEdit].getLength() + 16);
		}
	}
	
		
	PhraseSeq32() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		rightProducerMessage = producerMessage;
		rightConsumerMessage = consumerMessage;

		configParam(CONFIG_PARAM, 0.0f, 1.0f, PhraseSeq32::CONFIG_PARAM_INIT_VALUE, "Configuration (1, 2 chan)");
		char strBuf[32];
		for (int x = 0; x < 16; x++) {
			snprintf(strBuf, 32, "Step/phrase %i", x + 1);
			configParam(STEP_PHRASE_PARAMS + x, 0.0f, 1.0f, 0.0f, strBuf);
			snprintf(strBuf, 32, "Step/phrase %i", x + 16 + 1);
			configParam(STEP_PHRASE_PARAMS + x + 16, 0.0f, 1.0f, 0.0f, strBuf);
		}
		configParam(ATTACH_PARAM, 0.0f, 1.0f, 0.0f, "Attach");
		configParam(KEYNOTE_PARAM, 0.0f, 1.0f, 0.0f, "Keyboard note mode");
		configParam(KEYGATE_PARAM, 0.0f, 1.0f, 0.0f, "Keyboard gate-type mode");
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
		
		configParam(EDIT_PARAM, 0.0f, 1.0f, 1.0f, "Seq/song mode");
		configParam(RUNMODE_PARAM, 0.0f, 1.0f, 0.0f, "Length / run mode");
		configParam(RUN_PARAM, 0.0f, 1.0f, 0.0f, "Run");
		configParam(SEQUENCE_PARAM, -INFINITY, INFINITY, 0.0f, "Sequence");		
		configParam(TRAN_ROT_PARAM, 0.0f, 1.0f, 0.0f, "Transpose / rotate");
		
		configParam(RESET_PARAM, 0.0f, 1.0f, 0.0f, "Reset");
		configParam(COPY_PARAM, 0.0f, 1.0f, 0.0f, "Copy");
		configParam(PASTE_PARAM, 0.0f, 1.0f, 0.0f, "Paste");
		configParam(CPMODE_PARAM, 0.0f, 2.0f, 2.0f, "Copy-paste mode");	// 0.0f is top position

		configParam(GATE1_PARAM, 0.0f, 1.0f, 0.0f, "Gate 1");
		configParam(GATE2_PARAM, 0.0f, 1.0f, 0.0f, "Gate 2");
		configParam(TIE_PARAM, 0.0f, 1.0f, 0.0f, "Tied");

		configParam(GATE1_PROB_PARAM, 0.0f, 1.0f, 0.0f, "Gate 1 probability");
		configParam(GATE1_KNOB_PARAM, 0.0f, 1.0f, 1.0f, "Probability");
		configParam(SLIDE_BTN_PARAM, 0.0f, 1.0f, 0.0f, "CV slide");
		configParam(SLIDE_KNOB_PARAM, 0.0f, 2.0f, 0.2f, "Slide rate");
		configParam(AUTOSTEP_PARAM, 0.0f, 1.0f, 1.0f, "Autostep");
		
		for (int i = 0; i < 32; i++)
			seqAttribBuffer[i].init(16, MODE_FWD);
		onReset();
	}

	
	// widgets are not yet created when module is created (and when onReset() is called by constructor)
	// onReset() is also called when right-click initialization of module
	void onReset() override {
		stepConfig = getStepConfig(CONFIG_PARAM_INIT_VALUE);
		autoseq = false;
		autostepLen = false;
		holdTiedNotes = true;
		seqCVmethod = 0;// 0 is 0-10V, 1 is C4-G6, 2 is TrigIncr
		pulsesPerStep = 1;
		running = true;
		runModeSong = MODE_FWD;
		stepIndexEdit = 0;
		phraseIndexEdit = 0;
		seqIndexEdit = 0;
		phrases = 4;
		for (int i = 0; i < 32; i++) {
			for (int s = 0; s < 32; s++) {
				cv[i][s] = 0.0f;
				attributes[i][s].init();
			}
			sequences[i].init(16 * stepConfig, MODE_FWD);
			phrase[i] = 0;
			cvCPbuffer[i] = 0.0f;
			attribCPbuffer[i].init();
			phraseCPbuffer[i] = 0;
		}
		initRun();
		seqAttribCPbuffer.init(32, MODE_FWD);
		seqCopied = true;
		countCP = 32;
		startCP = 0;
		editingGate = 0ul;
		editingType = 0ul;
		infoCopyPaste = 0l;
		displayState = DISP_NORMAL;
		slideStepsRemain[0] = 0ul;
		slideStepsRemain[1] = 0ul;
		attached = false;
		stopAtEndOfSong = false;
		clockPeriod = 0ul;
		tiedWarning = 0ul;
		attachedWarning = 0l;
		attachedChanB = false;
		revertDisplay = 0l;
		resetOnRun = false;
		editingGateLength = 0l;
		lastGateEdit = 1l;
		editingPpqn = 0l;
		clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * APP->engine->getSampleRate());
	}
	
	
	void onRandomize() override {
		if (isEditingSequence()) {
			for (int s = 0; s < 32; s++) {
				cv[seqIndexEdit][s] = ((float)(random::u32() % 7)) + ((float)(random::u32() % 12)) / 12.0f - 3.0f;
				attributes[seqIndexEdit][s].randomize();
				// if (attributes[seqIndexEdit][s].getTied()) {
					// activateTiedStep(seqIndexEdit, s);
				// }
			}
			sequences[seqIndexEdit].randomize(16 * stepConfig, NUM_MODES);// ok to use stepConfig since CONFIG_PARAM is not randomizable		
		}
	}
	
	
	void initRun() {// run button activated or run edge in run input jack
		phraseIndexRun = (runModeSong == MODE_REV ? phrases - 1 : 0);
		phraseIndexRunHistory = 0;

		int seq = (isEditingSequence() ? seqIndexEdit : phrase[phraseIndexRun]);
		stepIndexRun[0] = (sequences[seq].getRunMode() == MODE_REV ? sequences[seq].getLength() - 1 : 0);
		fillStepIndexRunVector(sequences[seq].getRunMode(), sequences[seq].getLength());
		stepIndexRunHistory = 0;

		ppqnCount = 0;
		for (int i = 0; i < 2; i += stepConfig) {
			gate1Code[i] = calcGate1Code(attributes[seq][(i * 16) + stepIndexRun[i]], 0, pulsesPerStep, params[GATE1_KNOB_PARAM].getValue());
			gate2Code[i] = calcGate2Code(attributes[seq][(i * 16) + stepIndexRun[i]], 0, pulsesPerStep);
		}
		slideStepsRemain[0] = 0ul;
		slideStepsRemain[1] = 0ul;
	}	

	
	json_t *dataToJson() override {
		json_t *rootJ = json_object();

		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// expansion
		json_object_set_new(rootJ, "expansion", json_integer(expansion));

		// autostepLen
		json_object_set_new(rootJ, "autostepLen", json_boolean(autostepLen));
		
		// autoseq
		json_object_set_new(rootJ, "autoseq", json_boolean(autoseq));
		
		// holdTiedNotes
		json_object_set_new(rootJ, "holdTiedNotes", json_boolean(holdTiedNotes));
		
		// seqCVmethod
		json_object_set_new(rootJ, "seqCVmethod", json_integer(seqCVmethod));

		// pulsesPerStep
		json_object_set_new(rootJ, "pulsesPerStep", json_integer(pulsesPerStep));

		// running
		json_object_set_new(rootJ, "running", json_boolean(running));
		
		// runModeSong
		json_object_set_new(rootJ, "runModeSong3", json_integer(runModeSong));

		// seqIndexEdit
		json_object_set_new(rootJ, "sequence", json_integer(seqIndexEdit));

		// phrase 
		json_t *phraseJ = json_array();
		for (int i = 0; i < 32; i++)
			json_array_insert_new(phraseJ, i, json_integer(phrase[i]));
		json_object_set_new(rootJ, "phrase", phraseJ);

		// phrases
		json_object_set_new(rootJ, "phrases", json_integer(phrases));

		// CV
		json_t *cvJ = json_array();
		for (int i = 0; i < 32; i++)
			for (int s = 0; s < 32; s++) {
				json_array_insert_new(cvJ, s + (i * 32), json_real(cv[i][s]));
			}
		json_object_set_new(rootJ, "cv", cvJ);

		// attributes
		json_t *attributesJ = json_array();
		for (int i = 0; i < 32; i++)
			for (int s = 0; s < 32; s++) {
				json_array_insert_new(attributesJ, s + (i * 32), json_integer(attributes[i][s].getAttribute()));
			}
		json_object_set_new(rootJ, "attributes", attributesJ);

		// attached
		json_object_set_new(rootJ, "attached", json_boolean(attached));

		// stopAtEndOfSong
		json_object_set_new(rootJ, "stopAtEndOfSong", json_boolean(stopAtEndOfSong));

		// resetOnRun
		json_object_set_new(rootJ, "resetOnRun", json_boolean(resetOnRun));
		
		// stepIndexEdit
		json_object_set_new(rootJ, "stepIndexEdit", json_integer(stepIndexEdit));
	
		// phraseIndexEdit
		json_object_set_new(rootJ, "phraseIndexEdit", json_integer(phraseIndexEdit));

		// sequences
		json_t *sequencesJ = json_array();
		for (int i = 0; i < 32; i++)
			json_array_insert_new(sequencesJ, i, json_integer(sequences[i].getSeqAttrib()));
		json_object_set_new(rootJ, "sequences", sequencesJ);

		return rootJ;
	}

	
	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// expansion
		json_t *expansionJ = json_object_get(rootJ, "expansion");
		if (expansionJ)
			expansion = json_integer_value(expansionJ);

		// autostepLen
		json_t *autostepLenJ = json_object_get(rootJ, "autostepLen");
		if (autostepLenJ)
			autostepLen = json_is_true(autostepLenJ);

		// autoseq
		json_t *autoseqJ = json_object_get(rootJ, "autoseq");
		if (autoseqJ)
			autoseq = json_is_true(autoseqJ);

		// holdTiedNotes
		json_t *holdTiedNotesJ = json_object_get(rootJ, "holdTiedNotes");
		if (holdTiedNotesJ)
			holdTiedNotes = json_is_true(holdTiedNotesJ);
		else
			holdTiedNotes = false;// legacy
		
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

		// sequences
		json_t *sequencesJ = json_object_get(rootJ, "sequences");
		if (sequencesJ) {
			for (int i = 0; i < 32; i++)
			{
				json_t *sequencesArrayJ = json_array_get(sequencesJ, i);
				if (sequencesArrayJ)
					seqAttribBuffer[i].setSeqAttrib(json_integer_value(sequencesArrayJ));
			}			
		}
		else {// legacy
			int lengths[32];//1 to 32
			int runModeSeq[32]; 
			int transposeOffsets[32];	
			
			// runModeSeq
			json_t *runModeSeqJ = json_object_get(rootJ, "runModeSeq3");
			if (runModeSeqJ) {
				for (int i = 0; i < 32; i++)
				{
					json_t *runModeSeqArrayJ = json_array_get(runModeSeqJ, i);
					if (runModeSeqArrayJ)
						runModeSeq[i] = json_integer_value(runModeSeqArrayJ);
				}			
			}		
			else {// legacy
				runModeSeqJ = json_object_get(rootJ, "runModeSeq2");
				if (runModeSeqJ) {
					for (int i = 0; i < 32; i++)// bug, should be 32 but keep since legacy patches were written with 16
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
			if (lengthsJ)
				for (int i = 0; i < 32; i++)
				{
					json_t *lengthsArrayJ = json_array_get(lengthsJ, i);
					if (lengthsArrayJ)
						lengths[i] = json_integer_value(lengthsArrayJ);
				}
			// transposeOffsets
			json_t *transposeOffsetsJ = json_object_get(rootJ, "transposeOffsets");
			if (transposeOffsetsJ) {
				for (int i = 0; i < 32; i++)
				{
					json_t *transposeOffsetsArrayJ = json_array_get(transposeOffsetsJ, i);
					if (transposeOffsetsArrayJ)
						transposeOffsets[i] = json_integer_value(transposeOffsetsArrayJ);
				}			
			}
			
			// now write into new object
			for (int i = 0; i < 32; i++) {
				seqAttribBuffer[i].init(lengths[i], runModeSeq[i]);
				seqAttribBuffer[i].setTranspose(transposeOffsets[i]);
			}
		}
		
		// runModeSong
		json_t *runModeSongJ = json_object_get(rootJ, "runModeSong3");
		if (runModeSongJ)
			runModeSong = json_integer_value(runModeSongJ);
		else {// legacy
			json_t *runModeSongJ = json_object_get(rootJ, "runModeSong");
			if (runModeSongJ) {
				runModeSong = json_integer_value(runModeSongJ);
				if (runModeSong >= MODE_PEN)// this mode was not present in original version
					runModeSong++;
			}
		}
		
		// seqIndexEdit
		json_t *sequenceJ = json_object_get(rootJ, "sequence");
		if (sequenceJ)
			seqIndexEdit = json_integer_value(sequenceJ);
		
		// phrase
		json_t *phraseJ = json_object_get(rootJ, "phrase");
		if (phraseJ)
			for (int i = 0; i < 32; i++)
			{
				json_t *phraseArrayJ = json_array_get(phraseJ, i);
				if (phraseArrayJ)
					phrase[i] = json_integer_value(phraseArrayJ);
			}
		
		// phrases
		json_t *phrasesJ = json_object_get(rootJ, "phrases");
		if (phrasesJ)
			phrases = json_integer_value(phrasesJ);
		
		// CV
		json_t *cvJ = json_object_get(rootJ, "cv");
		if (cvJ) {
			for (int i = 0; i < 32; i++)
				for (int s = 0; s < 32; s++) {
					json_t *cvArrayJ = json_array_get(cvJ, s + (i * 32));
					if (cvArrayJ)
						cv[i][s] = json_number_value(cvArrayJ);
				}
		}
		
		// attributes
		json_t *attributesJ = json_object_get(rootJ, "attributes");
		if (attributesJ) {
			for (int i = 0; i < 32; i++)
				for (int s = 0; s < 32; s++) {
					json_t *attributesArrayJ = json_array_get(attributesJ, s + (i * 32));
					if (attributesArrayJ)
						attributes[i][s].setAttribute((unsigned short)json_integer_value(attributesArrayJ));
				}
		}
		
		// attached
		json_t *attachedJ = json_object_get(rootJ, "attached");
		if (attachedJ)
			attached = json_is_true(attachedJ);
		
		// stopAtEndOfSong
		json_t *stopAtEndOfSongJ = json_object_get(rootJ, "stopAtEndOfSong");
		if (stopAtEndOfSongJ)
			stopAtEndOfSong = json_is_true(stopAtEndOfSongJ);
		
		// resetOnRun
		json_t *resetOnRunJ = json_object_get(rootJ, "resetOnRun");
		if (resetOnRunJ)
			resetOnRun = json_is_true(resetOnRunJ);

		// stepIndexEdit
		json_t *stepIndexEditJ = json_object_get(rootJ, "stepIndexEdit");
		if (stepIndexEditJ)
			stepIndexEdit = json_integer_value(stepIndexEditJ);
		
		// phraseIndexEdit
		json_t *phraseIndexEditJ = json_object_get(rootJ, "phraseIndexEdit");
		if (phraseIndexEditJ)
			phraseIndexEdit = json_integer_value(phraseIndexEditJ);
		
		stepConfigSync = 1;// signal a sync from dataFromJson so that step will get lengths from seqAttribBuffer
	}

	void rotateSeq(int seqNum, bool directionRight, int seqLength, bool chanB_16) {
		// set chanB_16 to false to rotate chan A in 2x16 config (length will be <= 16) or single chan in 1x32 config (length will be <= 32)
		// set chanB_16 to true  to rotate chan B in 2x16 config (length must be <= 16)
		float rotCV;
		StepAttributes rotAttributes;
		int iStart = chanB_16 ? 16 : 0;
		int iEnd = iStart + seqLength - 1;
		int iRot = iStart;
		int iDelta = 1;
		if (directionRight) {
			iRot = iEnd;
			iDelta = -1;
		}
		rotCV = cv[seqNum][iRot];
		rotAttributes = attributes[seqNum][iRot];
		for ( ; ; iRot += iDelta) {
			if (iDelta == 1 && iRot >= iEnd) break;
			if (iDelta == -1 && iRot <= iStart) break;
			cv[seqNum][iRot] = cv[seqNum][iRot + iDelta];
			attributes[seqNum][iRot] = attributes[seqNum][iRot + iDelta];
		}
		cv[seqNum][iRot] = rotCV;
		attributes[seqNum][iRot] = rotAttributes;
	}
	

	void process(const ProcessArgs &args) override {
		float sampleRate = args.sampleRate;
		static const float gateTime = 0.4f;// seconds
		static const float revertDisplayTime = 0.7f;// seconds
		static const float warningTime = 0.7f;// seconds
		static const float holdDetectTime = 2.0f;// seconds
		static const float editGateLengthTime = 3.5f;// seconds
		
		bool expanderPresent = (rightModule && rightModule->model == modelPhraseSeqExpander);

		
		//********** Buttons, knobs, switches and inputs **********
		
		// Edit mode
		bool editingSequence = isEditingSequence();// true = editing sequence, false = editing song
		
		// Run button
		if (runningTrigger.process(params[RUN_PARAM].getValue() + inputs[RUNCV_INPUT].getVoltage())) {// no input refresh here, don't want to introduce startup skew
			running = !running;
			if (running) {
				if (resetOnRun)
					initRun();
				if (resetOnRun || clockIgnoreOnRun)
					clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * sampleRate);
				attachedChanB = stepIndexEdit >= 16;
			}
			displayState = DISP_NORMAL;
		}

		if ((lightRefreshCounter & userInputsStepSkipMask) == 0) {

			// Config switch
			if (stepConfigSync != 0) {
				stepConfig = getStepConfig(params[CONFIG_PARAM].getValue());
				if (stepConfigSync == 1) {// sync from dataFromJson, so read lengths from seqAttribBuffer
					for (int i = 0; i < 32; i++)
						sequences[i].setSeqAttrib(seqAttribBuffer[i].getSeqAttrib());
				}
				else if (stepConfigSync == 2) {// sync from a real mouse drag event on the switch itself, so init lengths
					for (int i = 0; i < 32; i++)
						sequences[i].setLength(16 * stepConfig);
				}
				initRun();			
				attachedChanB = false;
				stepConfigSync = 0;
			}
			
			// Seq CV input
			if (inputs[SEQCV_INPUT].isConnected()) {
				if (seqCVmethod == 0) {// 0-10 V
					int newSeq = (int)( inputs[SEQCV_INPUT].getVoltage() * (32.0f - 1.0f) / 10.0f + 0.5f );
					seqIndexEdit = clamp(newSeq, 0, 32 - 1);
				}
				else if (seqCVmethod == 1) {// C4-G6
					int newSeq = (int)( (inputs[SEQCV_INPUT].getVoltage()) * 12.0f + 0.5f );
					seqIndexEdit = clamp(newSeq, 0, 32 - 1);
				}
				else {// TrigIncr
					if (seqCVTrigger.process(inputs[SEQCV_INPUT].getVoltage()))
						seqIndexEdit = clamp(seqIndexEdit + 1, 0, 32 - 1);
				}	
			}
			
			// Mode CV input
			float modeCVin = consumerMessage[4];
			if (expanderPresent && !std::isnan(modeCVin)) {
				if (editingSequence)
					sequences[seqIndexEdit].setRunMode((int) clamp( std::round(modeCVin * ((float)NUM_MODES - 1.0f) / 10.0f), 0.0f, (float)NUM_MODES - 1.0f ));
			}
			
			// Attach button
			if (attachedTrigger.process(params[ATTACH_PARAM].getValue())) {
				attached = !attached;
				if (running && attached && editingSequence && stepConfig == 1 ) 
					attachedChanB = (stepIndexEdit >= 16);
				displayState = DISP_NORMAL;			
			}
			if (running && attached) {
				if (editingSequence) {
					if (attachedChanB && stepConfig == 1)
						stepIndexEdit = stepIndexRun[1] + 16;
					else
						stepIndexEdit = stepIndexRun[0] + 0;
				}
				else
					phraseIndexEdit = phraseIndexRun;
			}
			
			// Copy button
			if (copyTrigger.process(params[COPY_PARAM].getValue())) {
				if (!attached) {
					startCP = editingSequence ? stepIndexEdit : phraseIndexEdit;
					countCP = 32;
					if (params[CPMODE_PARAM].getValue() > 1.5f)// all
						startCP = 0;
					else if (params[CPMODE_PARAM].getValue() < 0.5f)// 4
						countCP = std::min(4, 32 - startCP);
					else// 8
						countCP = std::min(8, 32 - startCP);
					if (editingSequence) {
						for (int i = 0, s = startCP; i < countCP; i++, s++) {
							cvCPbuffer[i] = cv[seqIndexEdit][s];
							attribCPbuffer[i] = attributes[seqIndexEdit][s];
						}
						seqAttribCPbuffer.setSeqAttrib(sequences[seqIndexEdit].getSeqAttrib());
						seqCopied = true;
					}
					else {
						for (int i = 0, p = startCP; i < countCP; i++, p++)
							phraseCPbuffer[i] = phrase[p];
						seqCopied = false;// so that a cross paste can be detected
					}
					infoCopyPaste = (long) (revertDisplayTime * sampleRate / displayRefreshStepSkips);
					displayState = DISP_NORMAL;
				}
				else
					attachedWarning = (long) (warningTime * sampleRate / displayRefreshStepSkips);
			}
			// Paste button
			if (pasteTrigger.process(params[PASTE_PARAM].getValue())) {
				if (!attached) {
					infoCopyPaste = (long) (-1 * revertDisplayTime * sampleRate / displayRefreshStepSkips);
					startCP = 0;
					if (countCP <= 8) {
						startCP = editingSequence ? stepIndexEdit : phraseIndexEdit;
						countCP = std::min(countCP, 32 - startCP);
					}
					// else nothing to do for ALL

					if (editingSequence) {
						if (seqCopied) {// non-crossed paste (seq vs song)
							for (int i = 0, s = startCP; i < countCP; i++, s++) {
								cv[seqIndexEdit][s] = cvCPbuffer[i];
								attributes[seqIndexEdit][s] = attribCPbuffer[i];
							}
							if (params[CPMODE_PARAM].getValue() > 1.5f) {// all
								sequences[seqIndexEdit].setSeqAttrib(seqAttribCPbuffer.getSeqAttrib());
								if (sequences[seqIndexEdit].getLength() > 16 * stepConfig)
									sequences[seqIndexEdit].setLength(16 * stepConfig);
							}
						}
						else {// crossed paste to seq (seq vs song)
							if (params[CPMODE_PARAM].getValue() > 1.5f) { // ALL (init steps)
								for (int s = 0; s < 32; s++) {
									//cv[seqIndexEdit][s] = 0.0f;
									//attributes[seqIndexEdit][s].init();
									attributes[seqIndexEdit][s].toggleGate1();
								}
								sequences[seqIndexEdit].setTranspose(0);
								sequences[seqIndexEdit].setRotate(0);
							}
							else if (params[CPMODE_PARAM].getValue() < 0.5f) {// 4 (randomize CVs)
								for (int s = 0; s < 32; s++)
									cv[seqIndexEdit][s] = ((float)(random::u32() % 7)) + ((float)(random::u32() % 12)) / 12.0f - 3.0f;
								sequences[seqIndexEdit].setTranspose(0);
								sequences[seqIndexEdit].setRotate(0);
							}
							else {// 8 (randomize gate 1)
								for (int s = 0; s < 32; s++)
									if ( (random::u32() & 0x1) != 0)
										attributes[seqIndexEdit][s].toggleGate1();
							}
							startCP = 0;
							countCP = 32;
							infoCopyPaste *= 2l;
						}
					}
					else {
						if (!seqCopied) {// non-crossed paste (seq vs song)
							for (int i = 0, p = startCP; i < countCP; i++, p++)
								phrase[p] = phraseCPbuffer[i];
						}
						else {// crossed paste to song (seq vs song)
							if (params[CPMODE_PARAM].getValue() > 1.5f) { // ALL (init phrases)
								for (int p = 0; p < 32; p++)
									phrase[p] = 0;
							}
							else if (params[CPMODE_PARAM].getValue() < 0.5f) {// 4 (phrases increase from 1 to 32)
								for (int p = 0; p < 32; p++)
									phrase[p] = p;						
							}
							else {// 8 (randomize phrases)
								for (int p = 0; p < 32; p++)
									phrase[p] = random::u32() % 32;
							}
							startCP = 0;
							countCP = 32;
							infoCopyPaste *= 2l;
						}					
					}
					displayState = DISP_NORMAL;
				}
				else
					attachedWarning = (long) (warningTime * sampleRate / displayRefreshStepSkips);
			}
			
			// Write input (must be before Left and Right in case route gate simultaneously to Right and Write for example)
			//  (write must be to correct step)
			bool writeTrig = writeTrigger.process(inputs[WRITE_INPUT].getVoltage());
			if (writeTrig) {
				if (editingSequence) {
					if (!attributes[seqIndexEdit][stepIndexEdit].getTied()) {
						cv[seqIndexEdit][stepIndexEdit] = inputs[CV_INPUT].getVoltage();
						propagateCVtoTied(seqIndexEdit, stepIndexEdit);
					}
					editingGate = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
					editingGateCV = inputs[CV_INPUT].getVoltage();// cv[seqIndexEdit][stepIndexEdit];
					editingGateKeyLight = -1;
					editingChannel = (stepIndexEdit >= 16 * stepConfig) ? 1 : 0;
					// Autostep (after grab all active inputs)
					if (params[AUTOSTEP_PARAM].getValue() > 0.5f) {
						moveStepIndexEdit(1, autostepLen);
						if (stepIndexEdit == 0 && autoseq && !inputs[SEQCV_INPUT].isConnected())
							seqIndexEdit = moveIndex(seqIndexEdit, seqIndexEdit + 1, 32);
					}
				}
				displayState = DISP_NORMAL;
			}
			// Left and right CV inputs
			int delta = 0;
			if (leftTrigger.process(inputs[LEFTCV_INPUT].getVoltage())) { 
				delta = -1;
				if (displayState != DISP_LENGTH)
					displayState = DISP_NORMAL;
			}
			if (rightTrigger.process(inputs[RIGHTCV_INPUT].getVoltage())) {
				delta = +1;
				if (displayState != DISP_LENGTH)
					displayState = DISP_NORMAL;
			}
			if (delta != 0) {
				if (!running || !attached) {// don't move heads when attach and running
					if (editingSequence) {
						stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + delta, 32);
						if (!attributes[seqIndexEdit][stepIndexEdit].getTied()) {// play if non-tied step
							if (!writeTrig) {// in case autostep when simultaneous writeCV and stepCV (keep what was done in Write Input block above)
								editingGate = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
								editingGateCV = cv[seqIndexEdit][stepIndexEdit];
								editingGateKeyLight = -1;
								editingChannel = (stepIndexEdit >= 16 * stepConfig) ? 1 : 0;
							}
						}
					}
					else {
						phraseIndexEdit = moveIndex(phraseIndexEdit, phraseIndexEdit + delta, 32);
						if (!running)
							phraseIndexRun = phraseIndexEdit;	
					}						
				}
			}

			// Step button presses
			int stepPressed = -1;
			for (int i = 0; i < 32; i++) {
				if (stepTriggers[i].process(params[STEP_PHRASE_PARAMS + i].getValue()))
					stepPressed = i;
			}
			if (stepPressed != -1) {
				if (displayState == DISP_LENGTH) {
					if (editingSequence)
						sequences[seqIndexEdit].setLength((stepPressed % (16 * stepConfig)) + 1);
					else
						phrases = stepPressed + 1;
					revertDisplay = (long) (revertDisplayTime * sampleRate / displayRefreshStepSkips);
				}
				else {
					if (!running || !attached) {// not running or detached
						if (editingSequence) {
							stepIndexEdit = stepPressed;
							if (!attributes[seqIndexEdit][stepIndexEdit].getTied()) {// play if non-tied step
								editingGate = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
								editingGateCV = cv[seqIndexEdit][stepIndexEdit];
								editingGateKeyLight = -1;
								editingChannel = (stepIndexEdit >= 16 * stepConfig) ? 1 : 0;
							}
						}
						else {
							phraseIndexEdit = stepPressed;
							if (!running)
								phraseIndexRun = phraseIndexEdit;
						}
					}
					else {// attached and running
						if (attached)
							attachedWarning = (long) (warningTime * sampleRate / displayRefreshStepSkips);
						if (editingSequence) {
							if ((stepPressed < 16) && attachedChanB)
								attachedChanB = false;
							if ((stepPressed >= 16) && !attachedChanB)
								attachedChanB = true;					
						}
					}
					displayState = DISP_NORMAL;
				}
			} 
			
			// Mode/Length button
			if (modeTrigger.process(params[RUNMODE_PARAM].getValue())) {
				if (!attached) {
					if (editingPpqn != 0l)
						editingPpqn = 0l;			
					if (displayState == DISP_NORMAL || displayState == DISP_TRANSPOSE || displayState == DISP_ROTATE)
						displayState = DISP_LENGTH;
					else if (displayState == DISP_LENGTH)
						displayState = DISP_MODE;
					else
						displayState = DISP_NORMAL;
					modeHoldDetect.start((long) (holdDetectTime * sampleRate / displayRefreshStepSkips));
				}
				else
					attachedWarning = (long) (warningTime * sampleRate / displayRefreshStepSkips);
			}
			
			// Transpose/Rotate button
			if (transposeTrigger.process(params[TRAN_ROT_PARAM].getValue())) {
				if (editingSequence && !attached) {
					if (displayState == DISP_NORMAL || displayState == DISP_MODE || displayState == DISP_LENGTH) {
						displayState = DISP_TRANSPOSE;
					}
					else if (displayState == DISP_TRANSPOSE) {
						displayState = DISP_ROTATE;
					}
					else 
						displayState = DISP_NORMAL;
				}
				else if (attached)
					attachedWarning = (long) (warningTime * sampleRate / displayRefreshStepSkips);
			}			
			
			// Sequence knob 
			float seqParamValue = params[SEQUENCE_PARAM].getValue();
			int newSequenceKnob = (int)std::round(seqParamValue * 7.0f);
			if (seqParamValue == 0.0f)// true when constructor or dataFromJson() occured
				sequenceKnob = newSequenceKnob;
			int deltaKnob = newSequenceKnob - sequenceKnob;
			if (deltaKnob != 0) {
				if (abs(deltaKnob) <= 3) {// avoid discontinuous step (initialize for example)
					// any changes in here should may also require right click behavior to be updated in the knob's onMouseDown()
					if (editingPpqn != 0) {
						pulsesPerStep = indexToPps(ppsToIndex(pulsesPerStep) + deltaKnob);// indexToPps() does clamping
						editingPpqn = (long) (editGateLengthTime * sampleRate / displayRefreshStepSkips);
					}
					else if (displayState == DISP_MODE) {
						if (editingSequence) {
							if (!expanderPresent || std::isnan(consumerMessage[4])) {
								sequences[seqIndexEdit].setRunMode(clamp(sequences[seqIndexEdit].getRunMode() + deltaKnob, 0, NUM_MODES - 1));
							}
						}
						else {
							runModeSong = clamp(runModeSong + deltaKnob, 0, 6 - 1);
						}
					}
					else if (displayState == DISP_LENGTH) {
						if (editingSequence) {
							sequences[seqIndexEdit].setLength(clamp(sequences[seqIndexEdit].getLength() + deltaKnob, 1, (16 * stepConfig)));
						}
						else {
							phrases = clamp(phrases + deltaKnob, 1, 32);
						}
					}
					else if (displayState == DISP_TRANSPOSE) {
						if (editingSequence) {
							sequences[seqIndexEdit].setTranspose(clamp(sequences[seqIndexEdit].getTranspose() + deltaKnob, -99, 99));
							float transposeOffsetCV = ((float)(deltaKnob))/12.0f;// Tranpose by deltaKnob number of semi-tones
							if (stepConfig == 1){ // 2x16 (transpose only the 16 steps corresponding to row where stepIndexEdit is located)
								int offset = stepIndexEdit < 16 ? 0 : 16;
								for (int s = offset; s < offset + 16; s++) 
									cv[seqIndexEdit][s] += transposeOffsetCV;
							}
							else { // 1x32 (transpose all 32 steps)
								for (int s = 0; s < 32; s++) 
									cv[seqIndexEdit][s] += transposeOffsetCV;
							}
						}
					}
					else if (displayState == DISP_ROTATE) {
						if (editingSequence) {
							int slength = sequences[seqIndexEdit].getLength();
							bool rotChanB = (stepConfig == 1 && stepIndexEdit >= 16);
							sequences[seqIndexEdit].setRotate(clamp(sequences[seqIndexEdit].getRotate() + deltaKnob, -99, 99));
							if (deltaKnob > 0 && deltaKnob < 201) {// Rotate right, 201 is safety
								for (int i = deltaKnob; i > 0; i--) {
									rotateSeq(seqIndexEdit, true, slength, rotChanB);
									if ((stepConfig == 2 || !rotChanB ) && (stepIndexEdit < slength))
										stepIndexEdit = (stepIndexEdit + 1) % slength;
									if (rotChanB && (stepIndexEdit < (slength + 16)) && (stepIndexEdit >= 16))
										stepIndexEdit = ((stepIndexEdit - 16 + 1) % slength) + 16;
								}
							}
							if (deltaKnob < 0 && deltaKnob > -201) {// Rotate left, 201 is safety
								for (int i = deltaKnob; i < 0; i++) {
									rotateSeq(seqIndexEdit, false, slength, rotChanB);
									if ((stepConfig == 2 || !rotChanB ) && (stepIndexEdit < slength))
										stepIndexEdit = (stepIndexEdit + (stepConfig * 16 - 1) ) % slength;
									if (rotChanB && (stepIndexEdit < (slength + 16)) && (stepIndexEdit >= 16))
										stepIndexEdit = ((stepIndexEdit - 1 ) % slength) + 16;
								}
							}
						}						
					}
					else {// DISP_NORMAL
						if (editingSequence) {
							if (!inputs[SEQCV_INPUT].isConnected()) {
								seqIndexEdit = clamp(seqIndexEdit + deltaKnob, 0, 32 - 1);
							}
						}
						else {
							if (!attached || (attached && !running)) {
								int newPhrase = phrase[phraseIndexEdit] + deltaKnob;
								if (newPhrase < 0)
									newPhrase += (1 - newPhrase / 32) * 32;// newPhrase now positive
								newPhrase = newPhrase % 32;
								phrase[phraseIndexEdit] = newPhrase;
							}
							else 
								attachedWarning = (long) (warningTime * sampleRate / displayRefreshStepSkips);
						}
					}
				}
				sequenceKnob = newSequenceKnob;
			}	
			
			// Octave buttons
			for (int i = 0; i < 7; i++) {
				if (octTriggers[i].process(params[OCTAVE_PARAM + i].getValue())) {
					if (editingSequence) {
						displayState = DISP_NORMAL;
						if (attributes[seqIndexEdit][stepIndexEdit].getTied())
							tiedWarning = (long) (warningTime * sampleRate / displayRefreshStepSkips);
						else {			
							cv[seqIndexEdit][stepIndexEdit] = applyNewOct(cv[seqIndexEdit][stepIndexEdit], 6 - i);
							propagateCVtoTied(seqIndexEdit, stepIndexEdit);
							editingGate = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
							editingGateCV = cv[seqIndexEdit][stepIndexEdit];
							editingGateKeyLight = -1;
							editingChannel = (stepIndexEdit >= 16 * stepConfig) ? 1 : 0;
						}
					}
				}
			}
			
			// Keyboard buttons
			for (int i = 0; i < 12; i++) {
				if (keyTriggers[i].process(params[KEY_PARAMS + i].getValue())) {
					if (editingSequence) {
						displayState = DISP_NORMAL;
						if (editingGateLength != 0l) {
							int newMode = keyIndexToGateMode(i, pulsesPerStep);
							if (newMode != -1) {
								editingPpqn = 0l;
								attributes[seqIndexEdit][stepIndexEdit].setGateMode(newMode, editingGateLength > 0l);
								if (paramQuantities[KEY_PARAMS + i]->getMaxValue() > 1.5f) {// if double-click
									stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, 32);
									editingType = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
									editingGateKeyLight = i;
									if ((APP->window->getMods() & RACK_MOD_MASK) == RACK_MOD_CTRL)
										attributes[seqIndexEdit][stepIndexEdit].setGateMode(newMode, editingGateLength > 0l);
								}
							}
							else
								editingPpqn = (long) (editGateLengthTime * sampleRate / displayRefreshStepSkips);
						}
						else if (attributes[seqIndexEdit][stepIndexEdit].getTied()) {
							if (paramQuantities[KEY_PARAMS + i]->getMaxValue() > 1.5f)// if double-click
								stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, 32);
							else
								tiedWarning = (long) (warningTime * sampleRate / displayRefreshStepSkips);
						}
						else {			
							float newCV = std::floor(cv[seqIndexEdit][stepIndexEdit]) + ((float) i) / 12.0f;
							cv[seqIndexEdit][stepIndexEdit] = newCV;
							propagateCVtoTied(seqIndexEdit, stepIndexEdit);
							editingGate = (unsigned long) (gateTime * sampleRate / displayRefreshStepSkips);
							editingGateCV = cv[seqIndexEdit][stepIndexEdit];
							editingGateKeyLight = -1;
							editingChannel = (stepIndexEdit >= 16 * stepConfig) ? 1 : 0;
							if (paramQuantities[KEY_PARAMS + i]->getMaxValue() > 1.5f) {// if double-click
								stepIndexEdit = moveIndex(stepIndexEdit, stepIndexEdit + 1, 32);
								editingGateKeyLight = i;
								if ((APP->window->getMods() & RACK_MOD_MASK) == RACK_MOD_CTRL)
									cv[seqIndexEdit][stepIndexEdit] = newCV;
							}
						}						
					}
				}
			}
			
			// Keyboard mode (note or gate type)
			if (keyNoteTrigger.process(params[KEYNOTE_PARAM].getValue())) {
				editingGateLength = 0l;
			}
			if (keyGateTrigger.process(params[KEYGATE_PARAM].getValue())) {
				if (editingGateLength == 0l) {
					editingGateLength = lastGateEdit;
				}
				else {
					editingGateLength *= -1l;
					lastGateEdit = editingGateLength;
				}
			}

			// Gate1, Gate1Prob, Gate2, Slide and Tied buttons
			if (gate1Trigger.process(params[GATE1_PARAM].getValue() + (expanderPresent ? consumerMessage[0] : 0.0f))) {
				if (editingSequence) {
					displayState = DISP_NORMAL;
					attributes[seqIndexEdit][stepIndexEdit].toggleGate1();
				}
			}		
			if (gate1ProbTrigger.process(params[GATE1_PROB_PARAM].getValue())) {
				if (editingSequence) {
					displayState = DISP_NORMAL;
					if (attributes[seqIndexEdit][stepIndexEdit].getTied())
						tiedWarning = (long) (warningTime * sampleRate / displayRefreshStepSkips);
					else
						attributes[seqIndexEdit][stepIndexEdit].toggleGate1P();
				}
			}		
			if (gate2Trigger.process(params[GATE2_PARAM].getValue() + (expanderPresent ? consumerMessage[1] : 0.0f))) {
				if (editingSequence) {
					displayState = DISP_NORMAL;
					attributes[seqIndexEdit][stepIndexEdit].toggleGate2();
				}
			}		
			if (slideTrigger.process(params[SLIDE_BTN_PARAM].getValue() + (expanderPresent ? consumerMessage[3] : 0.0f))) {
				if (editingSequence) {
					displayState = DISP_NORMAL;
					if (attributes[seqIndexEdit][stepIndexEdit].getTied())
						tiedWarning = (long) (warningTime * sampleRate / displayRefreshStepSkips);
					else
						attributes[seqIndexEdit][stepIndexEdit].toggleSlide();
				}
			}		
			if (tiedTrigger.process(params[TIE_PARAM].getValue() + (expanderPresent ? consumerMessage[2] : 0.0f))) {
				if (editingSequence) {
					displayState = DISP_NORMAL;
					if (attributes[seqIndexEdit][stepIndexEdit].getTied()) {
						deactivateTiedStep(seqIndexEdit, stepIndexEdit);
					}
					else {
						activateTiedStep(seqIndexEdit, stepIndexEdit);
					}
				}
			}		
			
		}// userInputs refresh
		
		
		
		//********** Clock and reset **********
		
		// Clock
		if (running && clockIgnoreOnReset == 0l) {
			if (clockTrigger.process(inputs[CLOCK_INPUT].getVoltage())) {
				ppqnCount++;
				if (ppqnCount >= pulsesPerStep)
					ppqnCount = 0;

				int newSeq = seqIndexEdit;// good value when editingSequence, overwrite if not editingSequence
				if (ppqnCount == 0) {
					float slideFromCV[2] = {0.0f, 0.0f};
					if (editingSequence) {
						for (int i = 0; i < 2; i += stepConfig)
							slideFromCV[i] = cv[seqIndexEdit][(i * 16) + stepIndexRun[i]];
						moveIndexRunMode(&stepIndexRun[0], sequences[seqIndexEdit].getLength(), sequences[seqIndexEdit].getRunMode(), &stepIndexRunHistory);
					}
					else {
						for (int i = 0; i < 2; i += stepConfig)
							slideFromCV[i] = cv[phrase[phraseIndexRun]][(i * 16) + stepIndexRun[i]];
						int oldStepIndexRun0 = stepIndexRun[0];
						int oldStepIndexRun1 = stepIndexRun[1];
						if (moveIndexRunMode(&stepIndexRun[0], sequences[phrase[phraseIndexRun]].getLength(), sequences[phrase[phraseIndexRun]].getRunMode(), &stepIndexRunHistory)) {
							int oldPhraseIndexRun = phraseIndexRun;
							bool songLoopOver = moveIndexRunMode(&phraseIndexRun, phrases, runModeSong, &phraseIndexRunHistory);
							// check for end of song if needed
							if (songLoopOver && stopAtEndOfSong) {
								running = false;
								stepIndexRun[0] = oldStepIndexRun0;
								stepIndexRun[1] = oldStepIndexRun1;
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

					// Slide
					for (int i = 0; i < 2; i += stepConfig) {
						if (attributes[newSeq][(i * 16) + stepIndexRun[i]].getSlide()) {
							slideStepsRemain[i] = (unsigned long) (((float)clockPeriod  * pulsesPerStep) * params[SLIDE_KNOB_PARAM].getValue() / 2.0f);
							if (slideStepsRemain[i] != 0ul) {
								float slideToCV = cv[newSeq][(i * 16) + stepIndexRun[i]];
								slideCVdelta[i] = (slideToCV - slideFromCV[i])/(float)slideStepsRemain[i];
							}
						}
						else
							slideStepsRemain[i] = 0ul;
					}
				}
				else {
					if (!editingSequence)
						newSeq = phrase[phraseIndexRun];
				}
				for (int i = 0; i < 2; i += stepConfig) {
					if (gate1Code[i] != -1 || ppqnCount == 0)
						gate1Code[i] = calcGate1Code(attributes[newSeq][(i * 16) + stepIndexRun[i]], ppqnCount, pulsesPerStep, params[GATE1_KNOB_PARAM].getValue());
					gate2Code[i] = calcGate2Code(attributes[newSeq][(i * 16) + stepIndexRun[i]], ppqnCount, pulsesPerStep);	
				}
				clockPeriod = 0ul;
			}
			clockPeriod++;
		}
		
		// Reset
		if (resetTrigger.process(inputs[RESET_INPUT].getVoltage() + params[RESET_PARAM].getValue())) {
			initRun();// must be before SEQCV_INPUT below
			resetLight = 1.0f;
			displayState = DISP_NORMAL;
			clockIgnoreOnReset = (long) (clockIgnoreOnResetDuration * sampleRate);
			clockTrigger.reset();
			if (inputs[SEQCV_INPUT].isConnected() && seqCVmethod == 2)
				seqIndexEdit = 0;
		}
		
		
		//********** Outputs and lights **********
				
		// CV and gates outputs
		int seq = editingSequence ? seqIndexEdit : phrase[phraseIndexRun];
		int step0 = (editingSequence && !running) ? stepIndexEdit : stepIndexRun[0];
		if (running) {
			bool muteGate1A = !editingSequence && ((params[GATE1_PARAM].getValue() + (expanderPresent ? consumerMessage[0] : 0.0f)) > 0.5f);// live mute
			bool muteGate1B = muteGate1A;
			bool muteGate2A = !editingSequence && ((params[GATE2_PARAM].getValue() + (expanderPresent ? consumerMessage[1] : 0.0f)) > 0.5f);// live mute
			bool muteGate2B = muteGate2A;
			if (!attached && (muteGate1B || muteGate2B) && stepConfig == 1) {
				// if not attached in 2x16, mute only the channel where phraseIndexEdit is located (hack since phraseIndexEdit's row has no relation to channels)
				if (phraseIndexEdit < 16) {
					muteGate1B = false;
					muteGate2B = false;
				}
				else {
					muteGate1A = false;
					muteGate2A = false;
				}
			}
			float slideOffset[2];
			for (int i = 0; i < 2; i += stepConfig)
				slideOffset[i] = (slideStepsRemain[i] > 0ul ? (slideCVdelta[i] * (float)slideStepsRemain[i]) : 0.0f);
			outputs[CVA_OUTPUT].setVoltage(cv[seq][step0] - slideOffset[0]);
			bool retriggingOnReset = (clockIgnoreOnReset != 0l && retrigGatesOnReset);
			outputs[GATE1A_OUTPUT].setVoltage((calcGate(gate1Code[0], clockTrigger, clockPeriod, sampleRate) && !muteGate1A && !retriggingOnReset) ? 10.0f : 0.0f);
			outputs[GATE2A_OUTPUT].setVoltage((calcGate(gate2Code[0], clockTrigger, clockPeriod, sampleRate) && !muteGate2A && !retriggingOnReset) ? 10.0f : 0.0f);
			if (stepConfig == 1) {// 2x16
				int step1 = (editingSequence && !running) ? stepIndexEdit : stepIndexRun[1];
				outputs[CVB_OUTPUT].setVoltage(cv[seq][16 + step1] - slideOffset[1]);
				outputs[GATE1B_OUTPUT].setVoltage((calcGate(gate1Code[1], clockTrigger, clockPeriod, sampleRate) && !muteGate1B && !retriggingOnReset) ? 10.0f : 0.0f);
				outputs[GATE2B_OUTPUT].setVoltage((calcGate(gate2Code[1], clockTrigger, clockPeriod, sampleRate) && !muteGate2B && !retriggingOnReset) ? 10.0f : 0.0f);
			} 
			else {// 1x32
				outputs[CVB_OUTPUT].setVoltage(0.0f);
				outputs[GATE1B_OUTPUT].setVoltage(0.0f);
				outputs[GATE2B_OUTPUT].setVoltage(0.0f);
			}
		}
		else {// not running 
			if (stepConfig > 1) {// 1x32
				outputs[CVA_OUTPUT].setVoltage((editingGate > 0ul) ? editingGateCV : cv[seq][step0]);
				outputs[GATE1A_OUTPUT].setVoltage((editingGate > 0ul) ? 10.0f : 0.0f);
				outputs[GATE2A_OUTPUT].setVoltage((editingGate > 0ul) ? 10.0f : 0.0f);
				outputs[CVB_OUTPUT].setVoltage(0.0f);
				outputs[GATE1B_OUTPUT].setVoltage(0.0f);
				outputs[GATE2B_OUTPUT].setVoltage(0.0f);
			}
			else {// 2x16
				float cvA = 0.0f;
				float cvB = 0.0f;
				if (editingSequence) {
					cvA = (step0 >= 16 ? cv[seq][step0 - 16] : cv[seq][step0]);
					cvB = (step0 >= 16 ? cv[seq][step0] : cv[seq][step0 + 16]);
				}
				else {
					cvA = cv[seq][step0];
					cvB = cv[seq][stepIndexRun[1]];
				}
				if (editingChannel == 0) {
					outputs[CVA_OUTPUT].setVoltage((editingGate > 0ul) ? editingGateCV : cvA);
					outputs[GATE1A_OUTPUT].setVoltage((editingGate > 0ul) ? 10.0f : 0.0f);
					outputs[GATE2A_OUTPUT].setVoltage((editingGate > 0ul) ? 10.0f : 0.0f);
					outputs[CVB_OUTPUT].setVoltage(cvB);
					outputs[GATE1B_OUTPUT].setVoltage(0.0f);
					outputs[GATE2B_OUTPUT].setVoltage(0.0f);
				}
				else {
					outputs[CVA_OUTPUT].setVoltage(cvA);
					outputs[GATE1A_OUTPUT].setVoltage(0.0f);
					outputs[GATE2A_OUTPUT].setVoltage(0.0f);
					outputs[CVB_OUTPUT].setVoltage((editingGate > 0ul) ? editingGateCV : cvB);
					outputs[GATE1B_OUTPUT].setVoltage((editingGate > 0ul) ? 10.0f : 0.0f);
					outputs[GATE2B_OUTPUT].setVoltage((editingGate > 0ul) ? 10.0f : 0.0f);
				}
			}	
		}
		for (int i = 0; i < 2; i++)
			if (slideStepsRemain[i] > 0ul)
				slideStepsRemain[i]--;

		
		lightRefreshCounter++;
		if (lightRefreshCounter >= displayRefreshStepSkips) {
			lightRefreshCounter = 0;
		
			// Step/phrase lights
			for (int i = 0; i < 32; i++) {
				int col = (stepConfig == 1 ? (i & 0xF) : i);//i % (16 * stepConfig);// optimized
				float red = 0.0f;
				float green = 0.0f;
				float white = 0.0f;
				if (infoCopyPaste != 0l) {
					if (i >= startCP && i < (startCP + countCP))
						green = 0.71f;
				}
				else if (displayState == DISP_LENGTH) {
					if (editingSequence) {
						if (col < (sequences[seqIndexEdit].getLength() - 1))
							green = 0.32f;
						else if (col == (sequences[seqIndexEdit].getLength() - 1))
							green = 1.0f;
					}
					else {
						if (i < phrases - 1)
							green = 0.32f;
						else
							green = (i == phrases - 1) ? 1.0f : 0.0f;
					}
				}
				else if (displayState == DISP_TRANSPOSE) {
					red = 0.71f;
				}
				else if (displayState == DISP_ROTATE) {
					red = (i == stepIndexEdit ? 1.0f : (col < sequences[seqIndexEdit].getLength() ? 0.45f : 0.0f));
				}
				else {// normal led display (i.e. not length)
					int row = i >> (3 + stepConfig);//i / (16 * stepConfig);// optimized (not equivalent code, but in this case has same effect)
					// Run cursor (green)
					if (editingSequence)
						green = ((running && (col == stepIndexRun[row])) ? 1.0f : 0.0f);
					else {
						green = ((running && (i == phraseIndexRun)) ? 1.0f : 0.0f);
						green += ((running && (col == stepIndexRun[row]) && i != phraseIndexEdit) ? 0.32f : 0.0f);
						green = std::min(green, 1.0f);
					}
					// Edit cursor (red)
					if (editingSequence)
						red = (i == stepIndexEdit ? 1.0f : 0.0f);
					else
						red = (i == phraseIndexEdit ? 1.0f : 0.0f);
					bool gate = false;
					if (editingSequence)
						gate = attributes[seqIndexEdit][i].getGate1();
					else if (!editingSequence && (attached && running))
						gate = attributes[phrase[phraseIndexRun]][i].getGate1();
					white = ((green == 0.0f && red == 0.0f && gate && displayState != DISP_MODE) ? 0.2f : 0.0f);
					if (editingSequence && white != 0.0f) {
						green = 0.14f; white = 0.0f;
					}
				}
				setGreenRed(STEP_PHRASE_LIGHTS + i * 3, green, red);
				lights[STEP_PHRASE_LIGHTS + i * 3 + 2].setBrightness(white);
			}
		
			// Octave lights
			float octCV = 0.0f;
			if (editingSequence)
				octCV = cv[seqIndexEdit][stepIndexEdit];
			else
				octCV = cv[phrase[phraseIndexEdit]][stepIndexRun[0]];
			int octLightIndex = (int) std::floor(octCV + 3.0f);
			for (int i = 0; i < 7; i++) {
				if (!editingSequence && (!attached || !running || (stepConfig == 1)))// no oct lights when song mode and either (detached [1] or stopped [2] or 2x16config [3])
												// [1] makes no sense, can't mod steps and stepping though seq that may not be playing
												// [2] CV is set to 0V when not running and in song mode, so cv[][] makes no sense to display
												// [3] makes no sense, which sequence would be displayed, top or bottom row!
					lights[OCTAVE_LIGHTS + i].setBrightness(0.0f);
				else {
					if (tiedWarning > 0l) {
						bool warningFlashState = calcWarningFlash(tiedWarning, (long) (warningTime * sampleRate / displayRefreshStepSkips));
						lights[OCTAVE_LIGHTS + i].setBrightness((warningFlashState && (i == (6 - octLightIndex))) ? 1.0f : 0.0f);
					}
					else				
						lights[OCTAVE_LIGHTS + i].setBrightness(i == (6 - octLightIndex) ? 1.0f : 0.0f);
				}
			}
			
			// Keyboard lights (can only show channel A when running attached in 1x16 mode, does not pose problem for all other situations)
			float cvValOffset;
			if (editingSequence) 
				cvValOffset = cv[seqIndexEdit][stepIndexEdit] + 10.0f;//to properly handle negative note voltages
			else	
				cvValOffset = cv[phrase[phraseIndexEdit]][stepIndexRun[0]] + 10.0f;//to properly handle negative note voltages
			int keyLightIndex = clamp( (int)((cvValOffset - std::floor(cvValOffset)) * 12.0f + 0.5f),  0,  11);
			if (editingPpqn != 0) {
				for (int i = 0; i < 12; i++) {
					if (keyIndexToGateMode(i, pulsesPerStep) != -1) {
						setGreenRed(KEY_LIGHTS + i * 2, 1.0f, 1.0f);
					}
					else {
						setGreenRed(KEY_LIGHTS + i * 2, 0.0f, 0.0f);
					}
				}
			} 
			else if (editingGateLength != 0l && editingSequence) {
				int modeLightIndex = gateModeToKeyLightIndex(attributes[seqIndexEdit][stepIndexEdit], editingGateLength > 0l);
				for (int i = 0; i < 12; i++) {
					float green = editingGateLength > 0l ? 1.0f : 0.45f;
					float red = editingGateLength > 0l ? 0.45f : 1.0f;
					if (editingType > 0ul) {
						if (i == editingGateKeyLight) {
							float dimMult = ((float) editingType / (float)(gateTime * sampleRate / displayRefreshStepSkips));
							setGreenRed(KEY_LIGHTS + i * 2, green * dimMult, red * dimMult);
						}
						else
							setGreenRed(KEY_LIGHTS + i * 2, 0.0f, 0.0f);
					}
					else {
						if (i == modeLightIndex) {
							setGreenRed(KEY_LIGHTS + i * 2, green, red);
						}
						else { // show dim note if gatetype is different than note
							setGreenRed(KEY_LIGHTS + i * 2, 0.0f, (i == keyLightIndex ? 0.32f : 0.0f));
						}
					}
				}
			}
			else {
				for (int i = 0; i < 12; i++) {
					lights[KEY_LIGHTS + i * 2 + 0].setBrightness(0.0f);
					if (!editingSequence && (!attached || !running || (stepConfig == 1)))// no oct lights when song mode and either (detached [1] or stopped [2] or 2x16config [3])
													// [1] makes no sense, can't mod steps and stepping though seq that may not be playing
													// [2] CV is set to 0V when not running and in song mode, so cv[][] makes no sense to display
													// [3] makes no sense, which sequence would be displayed, top or bottom row!
						lights[KEY_LIGHTS + i * 2 + 1].setBrightness(0.0f);
					else {
						if (tiedWarning > 0l) {
							bool warningFlashState = calcWarningFlash(tiedWarning, (long) (warningTime * sampleRate / displayRefreshStepSkips));
							lights[KEY_LIGHTS + i * 2 + 1].setBrightness((warningFlashState && i == keyLightIndex) ? 1.0f : 0.0f);
						}
						else {
							if (editingGate > 0ul && editingGateKeyLight != -1)
								lights[KEY_LIGHTS + i * 2 + 1].setBrightness(i == editingGateKeyLight ? ((float) editingGate / (float)(gateTime * sampleRate / displayRefreshStepSkips)) : 0.0f);
							else
								lights[KEY_LIGHTS + i * 2 + 1].setBrightness(i == keyLightIndex ? 1.0f : 0.0f);
						}
					}
				}
			}		

			// Key mode light (note or gate type)
			lights[KEYNOTE_LIGHT].setBrightness(editingGateLength == 0l ? 1.0f : 0.0f);
			if (editingGateLength == 0l)
				setGreenRed(KEYGATE_LIGHT, 0.0f, 0.0f);
			else if (editingGateLength > 0l)
				setGreenRed(KEYGATE_LIGHT, 1.0f, 0.45f);
			else
				setGreenRed(KEYGATE_LIGHT, 0.45f, 1.0f);
			
			// Gate1, Gate1Prob, Gate2, Slide and Tied lights (can only show channel A when running attached in 1x32 mode, does not pose problem for all other situations)
			if (!editingSequence && (!attached || !running || (stepConfig == 1))) {// no oct lights when song mode and either (detached [1] or stopped [2] or 2x16config [3])
											// [1] makes no sense, can't mod steps and stepping though seq that may not be playing
											// [2] CV is set to 0V when not running and in song mode, so cv[][] makes no sense to display
											// [3] makes no sense, which sequence would be displayed, top or bottom row!
				setGateLight(false, GATE1_LIGHT);
				setGateLight(false, GATE2_LIGHT);
				setGreenRed(GATE1_PROB_LIGHT, 0.0f, 0.0f);
				lights[SLIDE_LIGHT].setBrightness(0.0f);
				lights[TIE_LIGHT].setBrightness(0.0f);
			}
			else {
				StepAttributes attributesVal = attributes[seqIndexEdit][stepIndexEdit];
				if (!editingSequence)
					attributesVal = attributes[phrase[phraseIndexEdit]][stepIndexRun[0]];
				//
				setGateLight(attributesVal.getGate1(), GATE1_LIGHT);
				setGateLight(attributesVal.getGate2(), GATE2_LIGHT);
				setGreenRed(GATE1_PROB_LIGHT, attributesVal.getGate1P() ? 1.0f : 0.0f, attributesVal.getGate1P() ? 1.0f : 0.0f);
				lights[SLIDE_LIGHT].setBrightness(attributesVal.getSlide() ? 1.0f : 0.0f);
				if (tiedWarning > 0l) {
					bool warningFlashState = calcWarningFlash(tiedWarning, (long) (warningTime * sampleRate / displayRefreshStepSkips));
					lights[TIE_LIGHT].setBrightness(warningFlashState ? 1.0f : 0.0f);
				}
				else
					lights[TIE_LIGHT].setBrightness(attributesVal.getTied() ? 1.0f : 0.0f);
			}
			
			// Attach light
			if (attachedWarning > 0l) {
				bool warningFlashState = calcWarningFlash(attachedWarning, (long) (warningTime * sampleRate / displayRefreshStepSkips));
				lights[ATTACH_LIGHT].setBrightness(warningFlashState ? 1.0f : 0.0f);
			}
			else
				lights[ATTACH_LIGHT].setBrightness(attached ? 1.0f : 0.0f);
			
			// Reset light
			lights[RESET_LIGHT].setSmoothBrightness(resetLight, args.sampleTime * displayRefreshStepSkips);
			resetLight = 0.0f;
			
			// Run light
			lights[RUN_LIGHT].setBrightness(running ? 1.0f : 0.0f);

			if (editingGate > 0ul)
				editingGate--;
			if (editingType > 0ul)
				editingType--;
			if (infoCopyPaste != 0l) {
				if (infoCopyPaste > 0l)
					infoCopyPaste --;
				if (infoCopyPaste < 0l)
					infoCopyPaste ++;
			}
			if (editingPpqn > 0l)
				editingPpqn--;
			if (tiedWarning > 0l)
				tiedWarning--;
			if (attachedWarning > 0l)
				attachedWarning--;
			if (modeHoldDetect.process(params[RUNMODE_PARAM].getValue())) {
				displayState = DISP_NORMAL;
				editingPpqn = (long) (editGateLengthTime * sampleRate / displayRefreshStepSkips);
			}
			if (revertDisplay > 0l) {
				if (revertDisplay == 1)
					displayState = DISP_NORMAL;
				revertDisplay--;
			}
			
			// To Expander
			if (rightModule && rightModule->model == modelPhraseSeqExpander) {
				float *producerMessage = reinterpret_cast<float*>(rightModule->leftProducerMessage);
				producerMessage[0] = (float)panelTheme;
			}
		}// lightRefreshCounter
				
		if (clockIgnoreOnReset > 0l)
			clockIgnoreOnReset--;
	}// process()
	

	inline void setGreenRed(int id, float green, float red) {
		lights[id + 0].setBrightness(green);
		lights[id + 1].setBrightness(red);
	}

	inline void propagateCVtoTied(int seqn, int stepn) {
		for (int i = stepn + 1; i < 32; i++) {
			if (!attributes[seqn][i].getTied())
				break;
			cv[seqn][i] = cv[seqn][i - 1];
		}	
	}

	void activateTiedStep(int seqn, int stepn) {
		attributes[seqn][stepn].setTied(true);
		if (stepn > 0) 
			propagateCVtoTied(seqn, stepn - 1);
		
		if (holdTiedNotes) {// new method
			attributes[seqn][stepn].setGate1(true);
			for (int i = std::max(stepn, 1); i < 32 && attributes[seqn][i].getTied(); i++) {
				attributes[seqn][i].setGate1Mode(attributes[seqn][i - 1].getGate1Mode());
				attributes[seqn][i - 1].setGate1Mode(5);
				attributes[seqn][i - 1].setGate1(true);
			}
		}
		else {// old method
			if (stepn > 0) {
				attributes[seqn][stepn] = attributes[seqn][stepn - 1];
				attributes[seqn][stepn].setTied(true);
			}
		}
	}
	
	void deactivateTiedStep(int seqn, int stepn) {
		attributes[seqn][stepn].setTied(false);
		if (holdTiedNotes) {// new method
			int lastGateType = attributes[seqn][stepn].getGate1Mode();
			for (int i = stepn + 1; i < 32 && attributes[seqn][i].getTied(); i++)
				lastGateType = attributes[seqn][i].getGate1Mode();
			if (stepn > 0)
				attributes[seqn][stepn - 1].setGate1Mode(lastGateType);
		}
		//else old method, nothing to do
	}
	
	inline void setGateLight(bool gateOn, int lightIndex) {
		if (!gateOn) {
			lights[lightIndex + 0].setBrightness(0.0f);
			lights[lightIndex + 1].setBrightness(0.0f);
		}
		else if (editingGateLength == 0l) {
			lights[lightIndex + 0].setBrightness(0.0f);
			lights[lightIndex + 1].setBrightness(1.0f);
		}
		else {
			lights[lightIndex + 0].setBrightness(lightIndex == GATE1_LIGHT ? 1.0f : 0.45f);
			lights[lightIndex + 1].setBrightness(lightIndex == GATE1_LIGHT ? 0.45f : 1.0f);
		}
	}

};



struct PhraseSeq32Widget : ModuleWidget {
	SvgPanel* lightPanel;
	SvgPanel* darkPanel;
	
	struct SequenceDisplayWidget : TransparentWidget {
		PhraseSeq32 *module;
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
					if (module->infoCopyPaste > 0l)
						snprintf(displayStr, 4, "CPY");
					else {
						float cpMode = module->params[PhraseSeq32::CPMODE_PARAM].getValue();
						if (editingSequence && !module->seqCopied) {// cross paste to seq
							if (cpMode > 1.5f)// All = toggle gate 1
								snprintf(displayStr, 4, "TG1");
							else if (cpMode < 0.5f)// 4 = random CV
								snprintf(displayStr, 4, "RCV");
							else// 8 = random gate 1
								snprintf(displayStr, 4, "RG1");
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
				else if (module->editingPpqn != 0ul) {
					snprintf(displayStr, 4, "x%2u", (unsigned) module->pulsesPerStep);
				}
				else if (module->displayState == PhraseSeq32::DISP_MODE) {
					if (editingSequence)
						runModeToStr(module->sequences[module->seqIndexEdit].getRunMode());
					else
						runModeToStr(module->runModeSong);
				}
				else if (module->displayState == PhraseSeq32::DISP_LENGTH) {
					if (editingSequence)
						snprintf(displayStr, 4, "L%2u", (unsigned) module->sequences[module->seqIndexEdit].getLength());
					else
						snprintf(displayStr, 4, "L%2u", (unsigned) module->phrases);
				}
				else if (module->displayState == PhraseSeq32::DISP_TRANSPOSE) {
					snprintf(displayStr, 4, "+%2u", (unsigned) abs(module->sequences[module->seqIndexEdit].getTranspose()));
					if (module->sequences[module->seqIndexEdit].getTranspose() < 0)
						displayStr[0] = '-';
				}
				else if (module->displayState == PhraseSeq32::DISP_ROTATE) {
					snprintf(displayStr, 4, ")%2u", (unsigned) abs(module->sequences[module->seqIndexEdit].getRotate()));
					if (module->sequences[module->seqIndexEdit].getRotate() < 0)
						displayStr[0] = '(';
				}
				else {// DISP_NORMAL
					snprintf(displayStr, 4, " %2u", (unsigned) (editingSequence ? 
						module->seqIndexEdit : module->phrase[module->phraseIndexEdit]) + 1 );
				}
			}
			nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};		
	
	struct PanelThemeItem : MenuItem {
		PhraseSeq32 *module;
		int theme;
		void onAction(const event::Action &e) override {
			module->panelTheme = theme;
		}
		void step() override {
			rightText = (module->panelTheme == theme) ? "✔" : "";
		}
	};
	struct ResetOnRunItem : MenuItem {
		PhraseSeq32 *module;
		void onAction(const event::Action &e) override {
			module->resetOnRun = !module->resetOnRun;
		}
	};
	struct AutoStepLenItem : MenuItem {
		PhraseSeq32 *module;
		void onAction(const event::Action &e) override {
			module->autostepLen = !module->autostepLen;
		}
	};
	struct AutoseqItem : MenuItem {
		PhraseSeq32 *module;
		void onAction(const event::Action &e) override {
			module->autoseq = !module->autoseq;
		}
	};
	struct HoldTiedItem : MenuItem {
		PhraseSeq32 *module;
		void onAction(const event::Action &e) override {
			module->holdTiedNotes = !module->holdTiedNotes;
		}
	};
	struct StopAtEndOfSongItem : MenuItem {
		PhraseSeq32 *module;
		void onAction(const event::Action &e) override {
			module->stopAtEndOfSong = !module->stopAtEndOfSong;
		}
	};
	struct SeqCVmethodItem : MenuItem {
		struct SeqCVmethodSubItem : MenuItem {
			PhraseSeq32 *module;
			int setVal = 2;
			void onAction(const event::Action &e) override {
				module->seqCVmethod = setVal;
			}
		};
		PhraseSeq32 *module;
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

		PhraseSeq32 *module = dynamic_cast<PhraseSeq32*>(this->module);
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

		menu->addChild(new MenuLabel());// empty line
		
		MenuLabel *settingsLabel = new MenuLabel();
		settingsLabel->text = "Settings";
		menu->addChild(settingsLabel);
		
		ResetOnRunItem *rorItem = createMenuItem<ResetOnRunItem>("Reset on run", CHECKMARK(module->resetOnRun));
		rorItem->module = module;
		menu->addChild(rorItem);

		AutoStepLenItem *astlItem = createMenuItem<AutoStepLenItem>("AutoStep write bounded by seq length", CHECKMARK(module->autostepLen));
		astlItem->module = module;
		menu->addChild(astlItem);

		AutoseqItem *aseqItem = createMenuItem<AutoseqItem>("AutoSeq when writing via CV inputs", CHECKMARK(module->autoseq));
		aseqItem->module = module;
		menu->addChild(aseqItem);

		HoldTiedItem *holdItem = createMenuItem<HoldTiedItem>("Hold tied notes", CHECKMARK(module->holdTiedNotes));
		holdItem->module = module;
		menu->addChild(holdItem);

		StopAtEndOfSongItem *loopItem = createMenuItem<StopAtEndOfSongItem>("Stop at end of song", CHECKMARK(module->stopAtEndOfSong));
		loopItem->module = module;
		menu->addChild(loopItem);

		SeqCVmethodItem *seqcvItem = createMenuItem<SeqCVmethodItem>("Seq CV in level", RIGHT_ARROW);
		seqcvItem->module = module;
		menu->addChild(seqcvItem);
	}	
	
	struct CKSSNotify : CKSS {// Not randomizable
		CKSSNotify() {}
		void randomize() override {}
		void onDragStart(const event::DragStart &e) override {
			Switch::onDragStart(e);
			if (paramQuantity) {
				PhraseSeq32* module = dynamic_cast<PhraseSeq32*>(paramQuantity->module);
				module->stepConfigSync = 2;// signal a sync from switch so that steps get initialized
			}
		}	
	};
	
	struct SequenceKnob : IMBigKnobInf {
		SequenceKnob() {};		
		void onDoubleClick(const event::DoubleClick &e) override {
			if (paramQuantity) {
				PhraseSeq32* module = dynamic_cast<PhraseSeq32*>(paramQuantity->module);
				// same code structure below as in sequence knob in main step()
				if (module->editingPpqn != 0) {
					module->pulsesPerStep = 1;
					//editingPpqn = (long) (editGateLengthTime * sampleRate / displayRefreshStepSkips);
				}
				else if (module->displayState == PhraseSeq32::DISP_MODE) {
					if (module->isEditingSequence()) {
						bool expanderPresent = (module->rightModule && module->rightModule->model == modelPhraseSeqExpander);
						if (!expanderPresent || std::isnan(module->consumerMessage[4])) {
							module->sequences[module->seqIndexEdit].setRunMode(MODE_FWD);
						}
					}
					else {
						module->runModeSong = MODE_FWD;
					}
				}
				else if (module->displayState == PhraseSeq32::DISP_LENGTH) {
					if (module->isEditingSequence()) {
						module->sequences[module->seqIndexEdit].setLength(16 * module->stepConfig);
					}
					else {
						module->phrases = 4;
					}
				}
				else if (module->displayState == PhraseSeq32::DISP_TRANSPOSE) {
					// nothing
				}
				else if (module->displayState == PhraseSeq32::DISP_ROTATE) {
					// nothing			
				}
				else {// DISP_NORMAL
					if (module->isEditingSequence()) {
						if (!module->inputs[PhraseSeq32::SEQCV_INPUT].isConnected()) {
							module->seqIndexEdit = 0;
						}
					}
					else {
						module->phrase[module->phraseIndexEdit] = 0;
					}
				}
			}
			ParamWidget::onDoubleClick(e);
		}
	};		
	
	
	PhraseSeq32Widget(PhraseSeq32 *module) {
		setModule(module);
		
		// Main panels from Inkscape
        lightPanel = new SvgPanel();
        lightPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/PhraseSeq32.svg")));
        box.size = lightPanel->box.size;
        addChild(lightPanel);
        darkPanel = new SvgPanel();
		darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/PhraseSeq32_dark.svg")));
		darkPanel->visible = false;
		addChild(darkPanel);

		// Screws
		addChild(createDynamicWidget<IMScrew>(Vec(15, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(15, 365), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 365), module ? &module->panelTheme : NULL));

		
		
		// ****** Top row ******
		
		static const int rowRulerT0 = 48;
		static const int columnRulerT0 = 18;//30;// Step/Phase LED buttons
		static const int columnRulerT3 = 377;// Attach 
		static const int columnRulerT4 = 430;// Config 

		// Step/Phrase LED buttons
		int posX = columnRulerT0;
		static int spacingSteps = 20;
		static int spacingSteps4 = 4;
		for (int x = 0; x < 16; x++) {
			// First row
			addParam(createParam<LEDButton>(Vec(posX, rowRulerT0 - 10 + 3 - 4.4f), module, PhraseSeq32::STEP_PHRASE_PARAMS + x));
			addChild(createLight<MediumLight<GreenRedWhiteLight>>(Vec(posX + 4.4f, rowRulerT0 - 10 + 3), module, PhraseSeq32::STEP_PHRASE_LIGHTS + (x * 3)));
			// Second row
			addParam(createParam<LEDButton>(Vec(posX, rowRulerT0 + 10 + 3 - 4.4f), module, PhraseSeq32::STEP_PHRASE_PARAMS + x + 16));
			addChild(createLight<MediumLight<GreenRedWhiteLight>>(Vec(posX + 4.4f, rowRulerT0 + 10 + 3), module, PhraseSeq32::STEP_PHRASE_LIGHTS + ((x + 16) * 3)));
			// step position to next location and handle groups of four
			posX += spacingSteps;
			if ((x + 1) % 4 == 0)
				posX += spacingSteps4;
		}
		// Attach button and light
		addParam(createDynamicParam<IMPushButton>(Vec(columnRulerT3 - 4, rowRulerT0 - 6 + 2 + offsetTL1105), module, PhraseSeq32::ATTACH_PARAM, module ? &module->panelTheme : NULL));
		addChild(createLight<MediumLight<RedLight>>(Vec(columnRulerT3 + 12 + offsetMediumLight, rowRulerT0 - 6 + offsetMediumLight), module, PhraseSeq32::ATTACH_LIGHT));		
		// Config switch
		addParam(createParam<CKSSNotify>(Vec(columnRulerT4 + hOffsetCKSS + 1, rowRulerT0 - 6 + vOffsetCKSS), module, PhraseSeq32::CONFIG_PARAM));

		
		
		// ****** Octave and keyboard area ******
		
		// Octave LED buttons
		static const float octLightsIntY = 20.0f;
		for (int i = 0; i < 7; i++) {
			addParam(createParam<LEDButton>(Vec(15 + 3, 82 + 24 + i * octLightsIntY- 4.4f), module, PhraseSeq32::OCTAVE_PARAM + i));
			addChild(createLight<MediumLight<RedLight>>(Vec(15 + 3 + 4.4f, 82 + 24 + i * octLightsIntY), module, PhraseSeq32::OCTAVE_LIGHTS + i));
		}
		// Keys and Key lights
		static const int keyNudgeX = 7;
		static const int KeyBlackY = 103;
		static const int KeyWhiteY = 141;
		static const int offsetKeyLEDx = 6;
		static const int offsetKeyLEDy = 16;
		// Black keys and lights
		addParam(createParam<InvisibleKeySmall>(			Vec(65+keyNudgeX, KeyBlackY), module, PhraseSeq32::KEY_PARAMS + 1));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(65+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 1 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(93+keyNudgeX, KeyBlackY), module, PhraseSeq32::KEY_PARAMS + 3));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(93+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 3 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(150+keyNudgeX, KeyBlackY), module, PhraseSeq32::KEY_PARAMS + 6));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(150+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 6 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(178+keyNudgeX, KeyBlackY), module, PhraseSeq32::KEY_PARAMS + 8));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(178+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 8 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(206+keyNudgeX, KeyBlackY), module, PhraseSeq32::KEY_PARAMS + 10));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(206+keyNudgeX+offsetKeyLEDx, KeyBlackY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 10 * 2));
		// White keys and lights
		addParam(createParam<InvisibleKeySmall>(			Vec(51+keyNudgeX, KeyWhiteY), module, PhraseSeq32::KEY_PARAMS + 0));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(51+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 0 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(79+keyNudgeX, KeyWhiteY), module, PhraseSeq32::KEY_PARAMS + 2));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(79+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 2 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(107+keyNudgeX, KeyWhiteY), module, PhraseSeq32::KEY_PARAMS + 4));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(107+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 4 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(136+keyNudgeX, KeyWhiteY), module, PhraseSeq32::KEY_PARAMS + 5));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(136+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 5 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(164+keyNudgeX, KeyWhiteY), module, PhraseSeq32::KEY_PARAMS + 7));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(164+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 7 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(192+keyNudgeX, KeyWhiteY), module, PhraseSeq32::KEY_PARAMS + 9));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(192+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 9 * 2));
		addParam(createParam<InvisibleKeySmall>(			Vec(220+keyNudgeX, KeyWhiteY), module, PhraseSeq32::KEY_PARAMS + 11));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(220+keyNudgeX+offsetKeyLEDx, KeyWhiteY+offsetKeyLEDy), module, PhraseSeq32::KEY_LIGHTS + 11 * 2));
		
		// Key mode LED buttons	
		static const int colRulerKM = 267;
		addParam(createParam<LEDButton>(Vec(colRulerKM, KeyBlackY + offsetKeyLEDy - 4.4f), module, PhraseSeq32::KEYNOTE_PARAM));
		addChild(createLight<MediumLight<RedLight>>(Vec(colRulerKM + 4.4f,  KeyBlackY + offsetKeyLEDy), module, PhraseSeq32::KEYNOTE_LIGHT));
		addParam(createParam<LEDButton>(Vec(colRulerKM, KeyWhiteY + offsetKeyLEDy - 4.4f), module, PhraseSeq32::KEYGATE_PARAM));
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(colRulerKM + 4.4f,  KeyWhiteY + offsetKeyLEDy), module, PhraseSeq32::KEYGATE_LIGHT));

		
		
		// ****** Right side control area ******

		static const int rowRulerMK0 = 101;// Edit mode row
		static const int rowRulerMK1 = rowRulerMK0 + 56; // Run row
		static const int rowRulerMK2 = rowRulerMK1 + 54; // Copy-paste Tran/rot row
		static const int columnRulerMK0 = 307;// Edit mode column
		static const int columnRulerMK2 = columnRulerT4;// Mode/Len column
		static const int columnRulerMK1 = 366;// Display column 
		
		// Edit mode switch
		addParam(createParam<CKSSNoRandom>(Vec(columnRulerMK0 + 2 + hOffsetCKSS, rowRulerMK0 + vOffsetCKSS), module, PhraseSeq32::EDIT_PARAM));
		// Sequence display
		SequenceDisplayWidget *displaySequence = new SequenceDisplayWidget();
		displaySequence->box.pos = Vec(columnRulerMK1-15, rowRulerMK0 + 3 + vOffsetDisplay);
		displaySequence->box.size = Vec(55, 30);// 3 characters
		displaySequence->module = module;
		addChild(displaySequence);
		// Len/mode button
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerMK2 + offsetCKD6b, rowRulerMK0 + 0 + offsetCKD6b), module, PhraseSeq32::RUNMODE_PARAM, module ? &module->panelTheme : NULL));

		// Autostep
		addParam(createParam<CKSSNoRandom>(Vec(columnRulerMK0 + 2 + hOffsetCKSS, rowRulerMK1 + 7 + vOffsetCKSS), module, PhraseSeq32::AUTOSTEP_PARAM));		
		// Sequence knob
		addParam(createDynamicParam<SequenceKnob>(Vec(columnRulerMK1 + 1 + offsetIMBigKnob, rowRulerMK0 + 55 + offsetIMBigKnob), module, PhraseSeq32::SEQUENCE_PARAM, module ? &module->panelTheme : NULL));		
		// Transpose/rotate button
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerMK2 + offsetCKD6b, rowRulerMK1 + 4 + offsetCKD6b), module, PhraseSeq32::TRAN_ROT_PARAM, module ? &module->panelTheme : NULL));
		
		// Reset LED bezel and light
		addParam(createParam<LEDBezel>(Vec(columnRulerMK0 - 43 + offsetLEDbezel, rowRulerMK2 + 5 + offsetLEDbezel), module, PhraseSeq32::RESET_PARAM));
		addChild(createLight<MuteLight<GreenLight>>(Vec(columnRulerMK0 - 43 + offsetLEDbezel + offsetLEDbezelLight, rowRulerMK2 + 5 + offsetLEDbezel + offsetLEDbezelLight), module, PhraseSeq32::RESET_LIGHT));
		// Run LED bezel and light
		addParam(createParam<LEDBezel>(Vec(columnRulerMK0 + 3 + offsetLEDbezel, rowRulerMK2 + 5 + offsetLEDbezel), module, PhraseSeq32::RUN_PARAM));
		addChild(createLight<MuteLight<GreenLight>>(Vec(columnRulerMK0 + 3 + offsetLEDbezel + offsetLEDbezelLight, rowRulerMK2 + 5 + offsetLEDbezel + offsetLEDbezelLight), module, PhraseSeq32::RUN_LIGHT));
		// Copy/paste buttons
		addParam(createDynamicParam<IMPushButton>(Vec(columnRulerMK1 - 10, rowRulerMK2 + 5 + offsetTL1105), module, PhraseSeq32::COPY_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParam<IMPushButton>(Vec(columnRulerMK1 + 20, rowRulerMK2 + 5 + offsetTL1105), module, PhraseSeq32::PASTE_PARAM, module ? &module->panelTheme : NULL));
		// Copy-paste mode switch (3 position)
		addParam(createParam<CKSSThreeInvNoRandom>(Vec(columnRulerMK2 + hOffsetCKSS + 1, rowRulerMK2 - 3 + vOffsetCKSSThree), module, PhraseSeq32::CPMODE_PARAM));	// 0.0f is top position

		
		
		// ****** Gate and slide section ******
		
		static const int rowRulerMB0 = 214;
		static const int columnRulerMBspacing = 70;
		static const int columnRulerMB2 = 130;// Gate2
		static const int columnRulerMB1 = columnRulerMB2 - columnRulerMBspacing;// Gate1 
		static const int columnRulerMB3 = columnRulerMB2 + columnRulerMBspacing;// Tie
		static const int posLEDvsButton = + 25;
		
		// Gate 1 light and button
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(columnRulerMB1 + posLEDvsButton + offsetMediumLight, rowRulerMB0 + 4 + offsetMediumLight), module, PhraseSeq32::GATE1_LIGHT));		
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerMB1 + offsetCKD6b, rowRulerMB0 + 4 + offsetCKD6b), module, PhraseSeq32::GATE1_PARAM, module ? &module->panelTheme : NULL));
		// Gate 2 light and button
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(columnRulerMB2 + posLEDvsButton + offsetMediumLight, rowRulerMB0 + 4 + offsetMediumLight), module, PhraseSeq32::GATE2_LIGHT));		
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerMB2 + offsetCKD6b, rowRulerMB0 + 4 + offsetCKD6b), module, PhraseSeq32::GATE2_PARAM, module ? &module->panelTheme : NULL));
		// Tie light and button
		addChild(createLight<MediumLight<RedLight>>(Vec(columnRulerMB3 + posLEDvsButton + offsetMediumLight, rowRulerMB0 + 4 + offsetMediumLight), module, PhraseSeq32::TIE_LIGHT));		
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerMB3 + offsetCKD6b, rowRulerMB0 + 4 + offsetCKD6b), module, PhraseSeq32::TIE_PARAM, module ? &module->panelTheme : NULL));

						
		
		// ****** Bottom two rows ******
		
		static const int inputJackSpacingX = 54;
		static const int outputJackSpacingX = 45;
		static const int rowRulerB0 = 323;
		static const int rowRulerB1 = 269;
		static const int columnRulerB0 = 22;
		static const int columnRulerB1 = columnRulerB0 + inputJackSpacingX;
		static const int columnRulerB2 = columnRulerB1 + inputJackSpacingX;
		static const int columnRulerB3 = columnRulerB2 + inputJackSpacingX;
		static const int columnRulerB4 = columnRulerB3 + inputJackSpacingX;
		static const int columnRulerB8 = columnRulerMK2 + 1;
		static const int columnRulerB7 = columnRulerB8 - outputJackSpacingX;
		static const int columnRulerB6 = columnRulerB7 - outputJackSpacingX;
		static const int columnRulerB5 = columnRulerB6 - outputJackSpacingX - 4;// clock and reset

		
		// Gate 1 probability light and button
		addChild(createLight<MediumLight<GreenRedLight>>(Vec(columnRulerB0 + posLEDvsButton + offsetMediumLight, rowRulerB1 + offsetMediumLight), module, PhraseSeq32::GATE1_PROB_LIGHT));		
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerB0 + offsetCKD6b, rowRulerB1 + offsetCKD6b), module, PhraseSeq32::GATE1_PROB_PARAM, module ? &module->panelTheme : NULL));
		// Gate 1 probability knob
		addParam(createDynamicParam<IMSmallKnob>(Vec(columnRulerB1 + offsetIMSmallKnob, rowRulerB1 + offsetIMSmallKnob), module, PhraseSeq32::GATE1_KNOB_PARAM, module ? &module->panelTheme : NULL));
		// Slide light and button
		addChild(createLight<MediumLight<RedLight>>(Vec(columnRulerB2 + posLEDvsButton + offsetMediumLight, rowRulerB1 + offsetMediumLight), module, PhraseSeq32::SLIDE_LIGHT));		
		addParam(createDynamicParam<IMBigPushButton>(Vec(columnRulerB2 + offsetCKD6b, rowRulerB1 + offsetCKD6b), module, PhraseSeq32::SLIDE_BTN_PARAM, module ? &module->panelTheme : NULL));
		// Slide knob
		addParam(createDynamicParam<IMSmallKnob>(Vec(columnRulerB3 + offsetIMSmallKnob, rowRulerB1 + offsetIMSmallKnob), module, PhraseSeq32::SLIDE_KNOB_PARAM, module ? &module->panelTheme : NULL));
		// CV in
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB4, rowRulerB1), true, module, PhraseSeq32::CV_INPUT, module ? &module->panelTheme : NULL));
		// Clock input
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB5, rowRulerB1), true, module, PhraseSeq32::CLOCK_INPUT, module ? &module->panelTheme : NULL));
		// Channel A outputs
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB6, rowRulerB1), false, module, PhraseSeq32::CVA_OUTPUT, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB7, rowRulerB1), false, module, PhraseSeq32::GATE1A_OUTPUT, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB8, rowRulerB1), false, module, PhraseSeq32::GATE2A_OUTPUT, module ? &module->panelTheme : NULL));


		// CV control Inputs 
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB0, rowRulerB0), true, module, PhraseSeq32::LEFTCV_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB1, rowRulerB0), true, module, PhraseSeq32::RIGHTCV_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB2, rowRulerB0), true, module, PhraseSeq32::SEQCV_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB3, rowRulerB0), true, module, PhraseSeq32::RUNCV_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB4, rowRulerB0), true, module, PhraseSeq32::WRITE_INPUT, module ? &module->panelTheme : NULL));
		// Reset input
		addInput(createDynamicPort<IMPort>(Vec(columnRulerB5, rowRulerB0), true, module, PhraseSeq32::RESET_INPUT, module ? &module->panelTheme : NULL));
		// Channel B outputs
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB6, rowRulerB0), false, module, PhraseSeq32::CVB_OUTPUT, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB7, rowRulerB0), false, module, PhraseSeq32::GATE1B_OUTPUT, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPort<IMPort>(Vec(columnRulerB8, rowRulerB0), false, module, PhraseSeq32::GATE2B_OUTPUT, module ? &module->panelTheme : NULL));
	}
	
	void step() override {
		if (module) {
			lightPanel->visible = ((((PhraseSeq32*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((PhraseSeq32*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelPhraseSeq32 = createModel<PhraseSeq32, PhraseSeq32Widget>("Phrase-Seq-32");

/*CHANGE LOG

1.0.0:
expansion panel replaced by a separate expander module
right-click keys to autostep replaced by double click
add menu option to stop at end of song

*/
