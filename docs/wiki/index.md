# PedalForge Wiki

Welcome to the PedalForge documentation. Use the sidebar to navigate between topics.

## Getting Started
- [[overview]] — What is PedalForge? Architecture at a glance
- [[tabs-guide]] — Tour of all tabs: Play, Board, Route, Pedal, FX, Script, Wiki, Library, Store, MIDI

## Architecture
- [[engine]] — AudioGraphEngine: the heart of the signal flow
- [[dsp-graph]] — DSPGraph: node-based effects processing
- [[pedal-design]] — PedalDesign: the pedal blueprint schema
- [[pedal-instance]] — PedalInstance: runtime pedal state
- [[midi-system]] — MIDI learn, routing, hardware devices
- [[ui-system]] — LookAndFeel, canvas overlays, theming

## DSP Node Catalog
- [[dsp-nodes-index]] — All 128+ node types at a glance
- [[audio-io]] — Audio Input/Output, Aux I/O
- [[filters]] — SVF, Ladder, LP, HP, Allpass, Tonestack, Parametric EQ
- [[effects]] — Delay, Reverb, Compressor, Noise Gate, Fuzz, Phaser, Flanger, Cabinet
- [[synthesis]] — Oscillator, Noise, ADSR, Poly Voice, Unison
- [[logic]] — AND, OR, NOT, Flip-Flops, Latches, Mux/Demux
- [[math]] — Add, Multiply, Trig, Lerp, Bitwise operators
- [[control]] — Knob, Fader, Button, XY Pad, Envelope Follower
- [[midi-nodes]] — Note, CC, Pitch Bend, Clock, Program Change, Generators
- [[memory]] — RAM, Sample & Hold, Edge Detect, Timer, Debounce
- [[display]] — LED, VU Meter, Tuner, Scope, Pixel Display, Console, Shader
- [[external]] — NAM Amp, IR Loader, Plugin Host, Faust

## Scripting
- [[expression-vm]] — ExpressionVM: bytecode virtual machine reference
- [[ui-scripts]] — UI Script mode: drawing commands & live preview
- [[dsp-expressions]] — DSP Expression mode: custom audio processing
- [[graph-builder]] — FX Graph Builder: programmatic node creation

## Reference
- [[engine-api]] — AudioGraphEngine method reference
- [[dspgraph-api]] — DSPGraph method reference
- [[pedal-design-schema]] — Full PedalDesign JSON schema
- [[color-palette]] — LookAndFeel color palette & theming
