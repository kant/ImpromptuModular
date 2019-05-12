//***********************************************************************************************
//Expander module for Clocked, by Marc Boulé
//
//Based on code from the Fundamental and Audible Instruments plugins by Andrew Belt and graphics  
//  from the Component Library by Wes Milholen. 
//See ./LICENSE.txt for all licenses
//See ./res/fonts/ for font licenses
//
//Module concept and design by Marc Boulé, Nigel Sixsmith, Xavier Belmont and Steve Baker
//
//***********************************************************************************************


#include "ImpromptuModular.hpp"


struct ClockedExpander : Module {
	enum InputIds {
		ENUMS(PW_INPUTS, 4),// fourth element not used
		ENUMS(SWING_INPUTS, 4),// fourth element not used
		NUM_INPUTS
	};


	// Expander
	float consumerMessage[1] = {};// this module must read from here
	float producerMessage[1] = {};// mother will write into here


	// No need to save
	int panelTheme = 0;


	ClockedExpander() {
		config(0, NUM_INPUTS, 0, 0);
		
		leftProducerMessage = producerMessage;
		leftConsumerMessage = consumerMessage;
	}


	void process(const ProcessArgs &args) override {		
		bool motherPresent = (leftModule && leftModule->model == modelClocked);
		if (motherPresent) {
			// To Mother
			float *producerMessage = reinterpret_cast<float*>(leftModule->rightProducerMessage);
			for (int i = 0; i < 8; i++) {
				producerMessage[i] = inputs[i].getVoltage();
			}
			// INFO("MOTHER PRESENT");
		}		
			
		// From Mother
		panelTheme = (motherPresent ? clamp((int)(consumerMessage[0] + 0.5f), 0, 1) : 0);
	}// process()
};


struct ClockedExpanderWidget : ModuleWidget {
	SvgPanel* darkPanel;
	
	ClockedExpanderWidget(ClockedExpander *module) {
		setModule(module);
	
		// Main panels from Inkscape
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/light/ClockedExpander.svg")));
        if (module) {
			darkPanel = new SvgPanel();
			darkPanel->setBackground(APP->window->loadSvg(asset::plugin(pluginInstance, "res/dark/ClockedExpander_dark.svg")));
			darkPanel->visible = false;
			addChild(darkPanel);
		}
		
		// Screws
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 0), module ? &module->panelTheme : NULL));
		addChild(createDynamicWidget<IMScrew>(Vec(box.size.x-30, 365), module ? &module->panelTheme : NULL));

		// Expansion module
		static const int rowRulerExpTop = 60;
		static const int rowSpacingExp = 50;
		static const int colRulerExp = 497 - 30 -150 - 300;// Clocked is (2+10)HP less than PS32
		addInput(createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 0), true, module, ClockedExpander::PW_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 1), true, module, ClockedExpander::PW_INPUTS + 1, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 2), true, module, ClockedExpander::PW_INPUTS + 2, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 3), true, module, ClockedExpander::SWING_INPUTS + 0, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 4), true, module, ClockedExpander::SWING_INPUTS + 1, module ? &module->panelTheme : NULL));
		addInput(createDynamicPort<IMPort>(Vec(colRulerExp, rowRulerExpTop + rowSpacingExp * 5), true, module, ClockedExpander::SWING_INPUTS + 2, module ? &module->panelTheme : NULL));
	}
	
	void step() override {
		if (module) {
			panel->visible = ((((ClockedExpander*)module)->panelTheme) == 0);
			darkPanel->visible  = ((((ClockedExpander*)module)->panelTheme) == 1);
		}
		Widget::step();
	}
};

Model *modelClockedExpander = createModel<ClockedExpander, ClockedExpanderWidget>("Clocked-Expander");

/*CHANGE LOG

*/
