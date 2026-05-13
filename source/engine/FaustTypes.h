#pragma once

/************************************************************************
 PedalForge — FAUST Base Types

 Extracted from the FAUST architecture file so they can be shared
 between the generated DSP headers and the FaustPedal adapter.
 ************************************************************************/

#include <algorithm>
#include <cmath>
#include <cstring>

#ifndef FAUSTFLOAT
#define FAUSTFLOAT float
#endif

// Minimal Meta class for FAUST metadata
struct Meta {
    virtual void declare(const char* key, const char* value) = 0;
    virtual ~Meta() = default;
};

// Minimal UI base class for FAUST parameter discovery
struct UI {
    virtual ~UI() = default;
    virtual void openTabBox(const char* label) = 0;
    virtual void openHorizontalBox(const char* label) = 0;
    virtual void openVerticalBox(const char* label) = 0;
    virtual void closeBox() = 0;
    virtual void addButton(const char* label, FAUSTFLOAT* zone) = 0;
    virtual void addCheckButton(const char* label, FAUSTFLOAT* zone) = 0;
    virtual void addVerticalSlider(const char* label, FAUSTFLOAT* zone,
                                   FAUSTFLOAT init, FAUSTFLOAT min,
                                   FAUSTFLOAT max, FAUSTFLOAT step) = 0;
    virtual void addHorizontalSlider(const char* label, FAUSTFLOAT* zone,
                                     FAUSTFLOAT init, FAUSTFLOAT min,
                                     FAUSTFLOAT max, FAUSTFLOAT step) = 0;
    virtual void addNumEntry(const char* label, FAUSTFLOAT* zone,
                             FAUSTFLOAT init, FAUSTFLOAT min,
                             FAUSTFLOAT max, FAUSTFLOAT step) = 0;
    virtual void addHorizontalBargraph(const char* label, FAUSTFLOAT* zone,
                                       FAUSTFLOAT min, FAUSTFLOAT max) = 0;
    virtual void addVerticalBargraph(const char* label, FAUSTFLOAT* zone,
                                     FAUSTFLOAT min, FAUSTFLOAT max) = 0;
    virtual void declare(FAUSTFLOAT* zone, const char* key, const char* val) {}
};

// Minimal dsp base class
class dsp {
public:
    virtual ~dsp() = default;
    virtual int getNumInputs() = 0;
    virtual int getNumOutputs() = 0;
    virtual void buildUserInterface(UI* ui_interface) = 0;
    virtual int getSampleRate() = 0;
    virtual void init(int sample_rate) = 0;
    virtual void instanceInit(int sample_rate) = 0;
    virtual void instanceConstants(int sample_rate) = 0;
    virtual void instanceResetUserInterface() = 0;
    virtual void instanceClear() = 0;
    virtual dsp* clone() = 0;
    virtual void metadata(Meta* m) = 0;
    virtual void compute(int count, FAUSTFLOAT** inputs, FAUSTFLOAT** outputs) = 0;
};
