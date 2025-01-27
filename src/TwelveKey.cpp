//***********************************************************************************************
//Chain-able keyboard module for VCV Rack by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.md for all licenses
//See ./res/fonts/ for font licenses
//
//Module inspired by:
//  * the Autodafe keyboard by Antonio Grazioli 
//  * the cf mixer by Clément Foulc
//  * Twisted Electrons' KeyChain 
//
//***********************************************************************************************


#include "ImpromptuModular.hpp"
#include "comp/PianoKey.hpp"


struct TwelveKey : Module {
	enum ParamIds {
		OCTINC_PARAM,
		OCTDEC_PARAM,
		MAXVEL_PARAM,
		VELPOL_PARAM,
		NUM_PARAMS
	};
	enum InputIds {
		GATE_INPUT,
		CV_INPUT,	
		OCT_INPUT,
		VEL_INPUT,
		NUM_INPUTS
	};
	enum OutputIds {
		GATE_OUTPUT,
		CV_OUTPUT,	
		OCT_OUTPUT,
		VEL_OUTPUT,
		NUM_OUTPUTS
	};
	enum LightIds {
		PRESS_LIGHT,// no longer used
		ENUMS(KEY_LIGHTS, 12),
		ENUMS(MAXVEL_LIGHTS, 5),
		NUM_LIGHTS
	};
	
	
	// Expander
	float leftMessages[2][3] = {};// messages from TwelveKey placed to the left (Max Vel, Invert Vel, Bipol)
		
	
	// Need to save, no reset
	int panelTheme;
	
	
	// Need to save, with reset
	int octaveNum;// 0 to 9
	float cv;
	float vel;// 0 to 1.0f
	float maxVel;// in volts
	bool stateInternal;// false when pass through CV and Gate, true when CV and gate from this module
	bool invertVel;// range is inverted (min vel is end of key)
	bool linkVelSettings;
	
	
	// No need to save, with reset
	unsigned long noteLightCounter;// 0 when no key to light, downward step counter timer when key lit


	// No need to save, no reset
	RefreshCounter refresh;
	Trigger gateInputTrigger;
	Trigger octIncTrigger;
	Trigger octDecTrigger;
	Trigger maxVelTrigger;
	dsp::BooleanTrigger keyTrigger;
	PianoKeyInfo pkInfo;
	

	bool isBipol(void) {return params[VELPOL_PARAM].getValue() > 0.5f;}


	TwelveKey() {
		config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
		
		leftExpander.producerMessage = leftMessages[0];
		leftExpander.consumerMessage = leftMessages[1];

		configParam(OCTDEC_PARAM, 0.0, 1.0, 0.0, "Oct down");
		configParam(OCTINC_PARAM, 0.0, 1.0, 0.0, "Oct up");
		configParam(MAXVEL_PARAM, 0.0, 1.0, 0.0, "Max velocity");
		configParam(VELPOL_PARAM, 0.0, 1.0, 0.0, "Velocity polarity");
		
		onReset();
		
		panelTheme = (loadDarkAsDefault() ? 1 : 0);
	}

	void onReset() override {
		octaveNum = 4;
		cv = 0.0f;
		vel = 1.0f;
		pkInfo.vel = vel;
		maxVel = 10.0f;
		stateInternal = inputs[GATE_INPUT].isConnected() ? false : true;
		invertVel = false;
		linkVelSettings = false;
		resetNonJson();
	}
	void resetNonJson() {
		noteLightCounter = 0ul;
	}

	void onRandomize() override {
		octaveNum = random::u32() % 10;
		cv = ((float)(octaveNum - 4)) + ((float)(random::u32() % 12)) / 12.0f;
		vel = random::uniform();
		pkInfo.vel = vel;
		maxVel = 10.0f;
		stateInternal = inputs[GATE_INPUT].isConnected() ? false : true;
	}

	json_t *dataToJson() override {
		json_t *rootJ = json_object();
		
		// panelTheme
		json_object_set_new(rootJ, "panelTheme", json_integer(panelTheme));

		// octave
		json_object_set_new(rootJ, "octave", json_integer(octaveNum));
		
		// cv
		json_object_set_new(rootJ, "cv", json_real(cv));
		
		// vel
		json_object_set_new(rootJ, "vel", json_real(vel));
		
		// maxVel
		json_object_set_new(rootJ, "maxVel", json_real(maxVel));
		
		// stateInternal
		json_object_set_new(rootJ, "stateInternal", json_boolean(stateInternal));

		// invertVel
		json_object_set_new(rootJ, "invertVel", json_boolean(invertVel));

		// linkVelSettings
		json_object_set_new(rootJ, "linkVelSettings", json_boolean(linkVelSettings));

		return rootJ;
	}

	void dataFromJson(json_t *rootJ) override {
		// panelTheme
		json_t *panelThemeJ = json_object_get(rootJ, "panelTheme");
		if (panelThemeJ)
			panelTheme = json_integer_value(panelThemeJ);

		// octave
		json_t *octaveJ = json_object_get(rootJ, "octave");
		if (octaveJ)
			octaveNum = json_integer_value(octaveJ);

		// cv
		json_t *cvJ = json_object_get(rootJ, "cv");
		if (cvJ)
			cv = json_number_value(cvJ);
		
		// vel
		json_t *velJ = json_object_get(rootJ, "vel");
		if (velJ) {
			vel = json_number_value(velJ);
			pkInfo.vel = vel;
		}
		
		// maxVel
		json_t *maxVelJ = json_object_get(rootJ, "maxVel");
		if (maxVelJ)
			maxVel = json_number_value(maxVelJ);
		
		// stateInternal
		json_t *stateInternalJ = json_object_get(rootJ, "stateInternal");
		if (stateInternalJ)
			stateInternal = json_is_true(stateInternalJ);
		
		// invertVel
		json_t *invertVelJ = json_object_get(rootJ, "invertVel");
		if (invertVelJ)
			invertVel = json_is_true(invertVelJ);
		
		// linkVelSettings
		json_t *linkVelSettingsJ = json_object_get(rootJ, "linkVelSettings");
		if (linkVelSettingsJ)
			linkVelSettings = json_is_true(linkVelSettingsJ);
		
		resetNonJson();
	}

	
	void process(const ProcessArgs &args) override {		
		static const float noteLightTime = 0.5f;// seconds
		
		//********** Buttons, knobs, switches and inputs **********
		
		bool upOctTrig = false;
		bool downOctTrig = false;
		
		if (refresh.processInputs()) {
			// From previous TwelveKey to the left
			if (linkVelSettings && leftExpander.module && leftExpander.module->model == modelTwelveKey) {
				// Get consumer message
				float *message = (float*) leftExpander.consumerMessage;
				maxVel = message[0];
				invertVel = message[1] > 0.5f;
				params[VELPOL_PARAM].setValue(message[2]);
			}

			// Octave buttons and input
			upOctTrig = octIncTrigger.process(params[OCTINC_PARAM].getValue());
			downOctTrig = octDecTrigger.process(params[OCTDEC_PARAM].getValue());
			
			if (maxVelTrigger.process(params[MAXVEL_PARAM].getValue())) {
				if (maxVel > 7.5f) maxVel = 5.0f;
				else if (maxVel > 3.0f) maxVel = 1.0f;
				else if (maxVel > 0.5f) maxVel = 2.0f/12.0f;
				else if (maxVel > 1.5f/12.0f) maxVel = 1.0f/12.0f;
				else maxVel = 10.0f;
			}
			
			pkInfo.showVelRange = outputs[VEL_OUTPUT].isConnected();
		}// userInputs refresh

		// Keyboard buttons and gate input (don't put in refresh scope or else trigger will go out to next module before cv and cv)
		if (keyTrigger.process(pkInfo.gate)) {
			cv = ((float)(octaveNum - 4)) + ((float) pkInfo.key) / 12.0f;
			stateInternal = true;
			noteLightCounter = (unsigned long) (noteLightTime * args.sampleRate / RefreshCounter::displayRefreshStepSkips);
		}
		if (gateInputTrigger.process(inputs[GATE_INPUT].getVoltage())) {// no input refresh here, don't want propagation lag in long 12-key chain
			cv = inputs[CV_INPUT].getVoltage();
			stateInternal = false;
		}
		
		// octave buttons or input
		if (inputs[OCT_INPUT].isConnected())
			octaveNum = ((int) std::floor(inputs[OCT_INPUT].getVoltage()));
		else {
			if (upOctTrig)
				octaveNum++;
			else if (downOctTrig)
				octaveNum--;
		}
		octaveNum = clamp(octaveNum, 0, 9);
		
		
		
		//********** Outputs and lights **********
		
		// cv output
		outputs[CV_OUTPUT].setVoltage(cv);
		
		// velocity output
		if (stateInternal == false) {// if receiving a key from left chain
			outputs[VEL_OUTPUT].setVoltage(inputs[VEL_INPUT].getVoltage());
		}
		else {// key from this
			vel = invertVel ? (1.0f - pkInfo.vel) : pkInfo.vel;
			float velVolt = vel * maxVel;
			if (isBipol()) {
				velVolt = velVolt * 2.0f - maxVel;
			}
			outputs[VEL_OUTPUT].setVoltage(velVolt);
		}
		

		// Octave output
		outputs[OCT_OUTPUT].setVoltage(std::round( (float)(octaveNum + 1) ));
		
		
		// gate output
		if (stateInternal == false) {// if receiving a key from left chain
			outputs[GATE_OUTPUT].setVoltage(inputs[GATE_INPUT].getVoltage());
		}
		else {// key from this
			outputs[GATE_OUTPUT].setVoltage(pkInfo.gate ? 10.0f : 0.0f);
		}


		// lights
		if (refresh.processLights()) {
			// Key lights
			for (int i = 0; i < 12; i++) {
				lights[KEY_LIGHTS + i].setBrightness(( i == pkInfo.key && (noteLightCounter > 0ul || pkInfo.gate)) ? 1.0f : 0.0f);
			}
			
			// Max velocity lights
			if (maxVel > 7.5f) setMaxVelLights(0);
			else if (maxVel > 3.0f) setMaxVelLights(1);
			else if (maxVel > 0.5f) setMaxVelLights(2);
			else if (maxVel > 1.5f/12.0f) setMaxVelLights(3);
			else setMaxVelLights(4);
			
			if (noteLightCounter > 0ul)
				noteLightCounter--;
			
			// To next TweleveKey to the right
			if (rightExpander.module && rightExpander.module->model == modelTwelveKey) {
				float *messageToExpander = (float*)(rightExpander.module->leftExpander.producerMessage);
				messageToExpander[0] = maxVel;
				messageToExpander[1] = (float)invertVel;
				messageToExpander[2] = params[VELPOL_PARAM].getValue();
				rightExpander.module->leftExpander.messageFlipRequested = true;
			}
		}// processLights()
	}
	
	void setMaxVelLights(int toSet) {
		for (int i = 0; i < 5; i++) {
			lights[MAXVEL_LIGHTS + i].setBrightness(i == toSet ? 1.0f : 0.0f);
		}
	}
};


struct TwelveKeyWidget : ModuleWidget {
	SvgPanel* darkPanel;

	struct OctaveNumDisplayWidget : TransparentWidget {
		int *octaveNum;
		std::shared_ptr<Font> font;
		
		OctaveNumDisplayWidget() {
			font = APP->window->loadFont(asset::plugin(pluginInstance, "res/fonts/Segment14.ttf"));
		}

		void draw(const DrawArgs &args) override {
			NVGcolor textColor = prepareDisplay(args.vg, &box, 18);
			nvgFontFaceId(args.vg, font->handle);
			//nvgTextLetterSpacing(args.vg, 2.5);

			Vec textPos = Vec(6, 24);
			nvgFillColor(args.vg, nvgTransRGBA(textColor, displayAlpha));
			nvgText(args.vg, textPos.x, textPos.y, "~", NULL);
			nvgFillColor(args.vg, textColor);
			char displayStr[2];
			displayStr[0] = 0x30 + (char) (octaveNum != NULL ? *octaveNum : 4);
			displayStr[1] = 0;
			nvgText(args.vg, textPos.x, textPos.y, displayStr, NULL);
		}
	};
	
	
	struct PanelThemeItem : MenuItem {
		TwelveKey *module;
		void onAction(const event::Action &e) override {
			module->panelTheme ^= 0x1;
		}
	};
	struct InvertVelItem : MenuItem {
		TwelveKey *module;
		void onAction(const event::Action &e) override {
			module->invertVel = !module->invertVel;
		}
	};
	struct LinkVelItem : MenuItem {
		TwelveKey *module;
		void onAction(const event::Action &e) override {
			module->linkVelSettings = !module->linkVelSettings;
		}
	};
	void appendContextMenu(Menu *menu) override {
		MenuLabel *spacerLabel = new MenuLabel();
		menu->addChild(spacerLabel);

		TwelveKey *module = dynamic_cast<TwelveKey*>(this->module);
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
		
		LinkVelItem *linkItem = createMenuItem<LinkVelItem>("Link velocity settings from left", CHECKMARK(module->linkVelSettings));
		linkItem->module = module;
		menu->addChild(linkItem);	
		
		InvertVelItem *invertItem = createMenuItem<InvertVelItem>("Inverted velocity range", CHECKMARK(module->invertVel));
		invertItem->module = module;
		invertItem->disabled = (module->linkVelSettings && module->leftExpander.module && module->leftExpander.module->model == modelTwelveKey);
		menu->addChild(invertItem);	
	}	
	
	
	TwelveKeyWidget(TwelveKey *module) {
		setModule(module);
		
		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/TwelveKey.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/TwelveKey_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(Vec(15, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(15, 365), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 365), module ? &module->panelTheme : NULL));



		// ****** Top portion (keys) ******

		static const int offsetKeyLEDx = 12;
		static const int offsetKeyLEDy = 41;// 32
		
		static const int posWhiteY = 115;
		static const int posBlackY = 40;

		// Black keys
		addChild(createPianoKey<PianoKeyBig>(Vec(30, posBlackY), 1, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(30+offsetKeyLEDx, posBlackY+offsetKeyLEDy), module, TwelveKey::KEY_LIGHTS + 1));
		addChild(createPianoKey<PianoKeyBig>(Vec(71, posBlackY), 3, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(71+offsetKeyLEDx, posBlackY+offsetKeyLEDy), module, TwelveKey::KEY_LIGHTS + 3));
		addChild(createPianoKey<PianoKeyBig>(Vec(154, posBlackY), 6, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(154+offsetKeyLEDx, posBlackY+offsetKeyLEDy), module, TwelveKey::KEY_LIGHTS + 6));
		addChild(createPianoKey<PianoKeyBig>(Vec(195, posBlackY), 8, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(195+offsetKeyLEDx, posBlackY+offsetKeyLEDy), module, TwelveKey::KEY_LIGHTS + 8));
		addChild(createPianoKey<PianoKeyBig>(Vec(236, posBlackY), 10, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(236+offsetKeyLEDx, posBlackY+offsetKeyLEDy), module, TwelveKey::KEY_LIGHTS + 10));

		// White keys
		addChild(createPianoKey<PianoKeyBig>(Vec(10, posWhiteY), 0, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(10+offsetKeyLEDx, posWhiteY+offsetKeyLEDy), module, TwelveKey::KEY_LIGHTS + 0));
		addChild(createPianoKey<PianoKeyBig>(Vec(51, posWhiteY), 2, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(51+offsetKeyLEDx, posWhiteY+offsetKeyLEDy), module, TwelveKey::KEY_LIGHTS + 2));
		addChild(createPianoKey<PianoKeyBig>(Vec(92, posWhiteY), 4, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(92+offsetKeyLEDx, posWhiteY+offsetKeyLEDy), module, TwelveKey::KEY_LIGHTS + 4));
		addChild(createPianoKey<PianoKeyBig>(Vec(133, posWhiteY), 5, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(133+offsetKeyLEDx, posWhiteY+offsetKeyLEDy), module, TwelveKey::KEY_LIGHTS + 5));
		addChild(createPianoKey<PianoKeyBig>(Vec(174, posWhiteY), 7, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(174+offsetKeyLEDx, posWhiteY+offsetKeyLEDy), module, TwelveKey::KEY_LIGHTS + 7));
		addChild(createPianoKey<PianoKeyBig>(Vec(215, posWhiteY), 9, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(215+offsetKeyLEDx, posWhiteY+offsetKeyLEDy), module, TwelveKey::KEY_LIGHTS + 9));
		addChild(createPianoKey<PianoKeyBig>(Vec(256, posWhiteY), 11, module ? &module->pkInfo : NULL));
		addChild(createLight<MediumLight<GreenLight>>(Vec(256+offsetKeyLEDx, posWhiteY+offsetKeyLEDy), module, TwelveKey::KEY_LIGHTS + 11));
		
		
		// ****** Bottom portion ******

		// Column rulers (horizontal positions)
		float colRulerCenter = box.size.x / 2.0f;
		static const int columnRulerL1 = 42;
		static const int columnRulerR1 = box.size.x - columnRulerL1;
		static const int columnRulerL2 = 96;
		static const int columnRulerR2 = box.size.x - columnRulerL2;
		
		// Row rulers (vertical positions)
		static const int rowRuler0 = 232;
		static const int rowRulerStep = 49;
		static const int rowRuler1 = rowRuler0 + rowRulerStep;
		static const int rowRuler2 = rowRuler1 + rowRulerStep;
		
		// Left side inputs
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerL1, rowRuler0), true, module, TwelveKey::OCT_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerL1, rowRuler1), true, module, TwelveKey::CV_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerL1, rowRuler2), true, module, TwelveKey::GATE_INPUT, module ? &module->panelTheme : NULL));
		addInput(createDynamicPortCentered<IMPort>(Vec(columnRulerL2, rowRuler2), true, module, TwelveKey::VEL_INPUT, module ? &module->panelTheme : NULL));

		// Octave buttons
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(columnRulerL2, rowRuler0), module, TwelveKey::OCTDEC_PARAM, module ? &module->panelTheme : NULL));
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(colRulerCenter, rowRuler0), module, TwelveKey::OCTINC_PARAM, module ? &module->panelTheme : NULL));
		
		// Octave display
		OctaveNumDisplayWidget *octaveNumDisplay = new OctaveNumDisplayWidget();
		octaveNumDisplay->box.size = Vec(24, 30);// 1 character
		octaveNumDisplay->box.pos = Vec(columnRulerR2 - octaveNumDisplay->box.size.x / 2, rowRuler0 - octaveNumDisplay->box.size.y / 2);
		octaveNumDisplay->octaveNum = module ? &module->octaveNum : NULL;
		addChild(octaveNumDisplay);
		
		// Max velocity button and lights
		addParam(createDynamicParamCentered<IMBigPushButton>(Vec(columnRulerL2, rowRuler1), module, TwelveKey::MAXVEL_PARAM, module ? &module->panelTheme : NULL));
		for (int i = 0; i < 5; i++) {
			addChild(createLightCentered<MediumLight<GreenLight>>(Vec(colRulerCenter - 15 + 19 * i, rowRuler1), module, TwelveKey::MAXVEL_LIGHTS + i));	
		}		


		// Velocity polarity
		addParam(createParamCentered<CKSSNoRandom>(Vec(colRulerCenter, rowRuler2), module, TwelveKey::VELPOL_PARAM));
		
		// Right side outputs
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerR1, rowRuler0), false, module, TwelveKey::OCT_OUTPUT, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerR1, rowRuler1), false, module, TwelveKey::CV_OUTPUT, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerR1, rowRuler2), false, module, TwelveKey::GATE_OUTPUT, module ? &module->panelTheme : NULL));
		addOutput(createDynamicPortCentered<IMPort>(Vec(columnRulerR2, rowRuler2), false, module, TwelveKey::VEL_OUTPUT, module ? &module->panelTheme : NULL));
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((TwelveKey*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((TwelveKey*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelTwelveKey = createModel<TwelveKey, TwelveKeyWidget>("Twelve-Key");
