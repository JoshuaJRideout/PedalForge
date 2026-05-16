# File Browser Grid Fix
I've updated the code so the file loader behaves correctly and displays a visually distinct button directly on the pedalboard grid!

## Changes Made:
1. **Grid File Browser Rendering**: Updated `PedalPainter::paintDesign()` to recognize `"file_loader"` controls. Instead of just writing text at the bottom, it now explicitly draws a styled rounded rectangle button in dark grey with white bold text (matching the label, e.g., "Load Model...").
2. **Grid Interaction Support**: Updated `PedalComponent::mouseDown()` to listen for clicks directly on `file_loader` boundaries.
3. **Direct File Inject**: When clicking the button on the grid, a `FileChooser` is asynchronously spun up, exactly like in the detail panel. Upon selecting a valid file, it securely targets the underlying `GraphPedalProcessor` and executes `setNodeFilePath`, updating the model in real time.

The standalone app has compiled and relaunched. You can now visually see the "Load Model..." button on the pedal grid itself, and you can click it to swap NAM models without ever needing to open the detail properties panel!
