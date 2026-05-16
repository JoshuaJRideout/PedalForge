# NAM File Browser Update
I have implemented the custom file browser UI element specifically for the play tab. 

## Changes Made:
1. **Added `file_loader` Control Type**: The `PedalDesign` system now natively understands `"file_loader"` (or `"file_browser"`) control types. 
2. **Updated Pedal Detail Panel**: The `PedalDetailPanel` (the panel that opens when you double-click a pedal in the Play tab) now parses these controls. It creates a native JUCE button right where you specified in the design layout.
3. **Graph Interface Extension**: Updated `GraphPedalProcessor` with a `setNodeFilePath` interface that bypasses the normal parameter automation system, allowing the play tab to inject string payloads (like file paths) straight into the internal nodes.
4. **Updated NAM Factory Pedal**: Modified the "NAM Amp" factory design to include a "Load Model..." button positioned neatly beneath the Input/Output knobs. It targets the `2_filepath` pseudo-parameter, which routes the selected file path correctly into the internal `NAMNode`.

The application has successfully recompiled and launched. If you go to the play tab, instantiate a "NAM Amp", and double click it, you should see your new "Load Model..." button ready to go!
