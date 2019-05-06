//***********************************************************************************************
//Impromptu Modular: Modules for VCV Rack by Marc Boulé
//
//See ./LICENSE.txt for all licenses
//***********************************************************************************************


#include "IMWidgets.hpp"



// Dynamic SVGScrew

void ScrewCircle::draw(const DrawArgs &args) {
	NVGcolor backgroundColor = nvgRGB(0x72, 0x72, 0x72); 
	NVGcolor borderColor = nvgRGB(0x72, 0x72, 0x72);
	nvgBeginPath(args.vg);
	nvgCircle(args.vg, box.size.x/2.0f, box.size.y/2.0f, 1.2f);// box, radius
	nvgFillColor(args.vg, backgroundColor);
	nvgFill(args.vg);
	nvgStrokeWidth(args.vg, 1.0);
	nvgStrokeColor(args.vg, borderColor);
	nvgStroke(args.vg);
}

DynamicSVGScrew::DynamicSVGScrew() {
	sw = new SvgWidget();
	sw->setSvg(APP->window->loadSvg(asset::system("res/ComponentLibrary/ScrewSilver.svg")));
	
	ScrewCircle *sc = new ScrewCircle();
	sc->box.size = sw->box.size;
	
	TransformWidget *tw = new TransformWidget();
	tw->addChild(sw);
	tw->addChild(sc);
	addChild(tw);
	
	box.size = sw->box.size;
	tw->box.size = sw->box.size; 
	// Rotate SVG
	Vec center = sw->box.getCenter();
	tw->identity();
	tw->translate(center);
	tw->rotate(M_PI/4.0f);
	tw->translate(center.neg());	

	// for fixed svg screw used in alternate mode
	// **********
	swAlt = new SvgWidget();
	swAlt->visible = false;
    addChild(swAlt);
}

void DynamicSVGScrew::addSVGalt(std::shared_ptr<Svg> svg) {
    if(!swAlt->svg) {
        swAlt->setSvg(svg);
    }
}

void DynamicSVGScrew::step() {
    if(mode != NULL && *mode != oldMode) {
        if ((*mode) == 0) {
			sw->visible = true;
			swAlt->visible = false;
		}
		else {
			sw->visible = false;
			swAlt->visible = true;
		}
        oldMode = *mode;
        dirty = true;
    }
	FramebufferWidget::step();
}



// Dynamic SVGPort

void DynamicSVGPort::addFrame(std::shared_ptr<Svg> svg) {
    frames.push_back(svg);
    if(frames.size() == 1) {
        SvgPort::setSvg(svg);
	}
}

void DynamicSVGPort::step() {
    if(mode != NULL && *mode != oldMode) {
        sw->setSvg(frames[*mode]);
        oldMode = *mode;
        fb->dirty = true;
    }
	PortWidget::step();
}



// Dynamic SVGSwitch

void DynamicSVGSwitch::addFrameAll(std::shared_ptr<Svg> svg) {
    framesAll.push_back(svg);
	if (framesAll.size() == 2) {
		addFrame(framesAll[0]);
		addFrame(framesAll[1]);
	}
}

void DynamicSVGSwitch::step() {
    if(mode != NULL && *mode != oldMode) {
        if ((*mode) == 0) {
			frames[0]=framesAll[0];
			frames[1]=framesAll[1];
		}
		else {
			frames[0]=framesAll[2];
			frames[1]=framesAll[3];
		}
        oldMode = *mode;
		onChange(*(new event::Change()));// required because of the way SVGSwitch changes images, we only change the frames above.
		fb->dirty = true;// dirty is not sufficient when changing via frames assignments above (i.e. onChange() is required)
    }
	SvgSwitch::step();
}



// Dynamic SVGKnob

void DynamicSVGKnob::addFrameAll(std::shared_ptr<Svg> svg) {
    framesAll.push_back(svg);
	if (framesAll.size() == 1) {
		setSvg(svg);
	}
}

void DynamicSVGKnob::addEffect(std::shared_ptr<Svg> svg) {
    effect = new SvgWidget();
	effect->setSvg(svg);
	effect->visible = false;
	addChild(effect);
}

void DynamicSVGKnob::step() {
    if(mode != NULL && *mode != oldMode) {
        if ((*mode) == 0) {
			setSvg(framesAll[0]);
			effect->visible = false;
		}
		else {
			setSvg(framesAll[1]);
			effect->visible = true;
		}
        oldMode = *mode;
		fb->dirty = true;
    }
	SvgKnob::step();
}

