# NAM Support Overview
I've successfully integrated Neural Amp Modeler (NAM) Core into PedalForge!

## What was done:
1. **Integrated `sdatkinson/NeuralAmpModelerCore` via CMake**: Set it up to automatically fetch the NAM inference engine source files and build them directly into PedalForge.
2. **Created `NAMNode`**: A C/C++ node that inherits from `DSPNode`. It hosts the loaded `.nam` model and buffers the processing.
3. **Graph Editor Integration**: Added "NAM Amp" to the Node properties dropdowns, and patched the `FileChooser` so that right-clicking a NAMNode presents a "Load File..." button configured specifically for `.nam` extension models.
4. **Added Factory Pedal**: Added "NAM Amp" to the Factory Pedal Library. It has an Input knob, an Output knob, and routes audio directly through the internal `NAMNode`.

The application has restarted. You can now add the "NAM Amp" from the play tab inventory, double-click it to open the properties panel, and select a `.nam` model file to evaluate!
