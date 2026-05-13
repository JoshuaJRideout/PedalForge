#include "CableComponent.h"
#include "LookAndFeel.h"

//==============================================================================
CableComponent::CableComponent (juce::Colour colour)
    : cableColour (colour)
{
    setInterceptsMouseClicks (false, false);
    setAnimated (true);
}

//==============================================================================
void CableComponent::paint (juce::Graphics& g)
{
    auto path = getCablePath();

    if (path.isEmpty()) return;

    // Cable shadow
    auto shadowPath = path;
    shadowPath.applyTransform (juce::AffineTransform::translation (1.5f, 2.0f));
    g.setColour (juce::Colours::black.withAlpha (0.3f));
    g.strokePath (shadowPath, juce::PathStrokeType (4.0f, juce::PathStrokeType::curved,
                                                     juce::PathStrokeType::rounded));

    // Main cable
    g.setColour (cableColour.withAlpha (0.8f));
    g.strokePath (path, juce::PathStrokeType (3.0f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Cable highlight (inner glow)
    g.setColour (cableColour.brighter (0.4f).withAlpha (0.3f));
    g.strokePath (path, juce::PathStrokeType (1.5f, juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Animated signal pulse
    if (animated)
    {
        float pathLength = path.getLength();
        if (pathLength > 0)
        {
            float pulsePos = animationPhase * pathLength;
            auto point = path.getPointAlongPath (pulsePos);

            g.setColour (cableColour.brighter (0.8f).withAlpha (0.6f));
            g.fillEllipse (point.x - 3.0f, point.y - 3.0f, 6.0f, 6.0f);

            g.setColour (cableColour.brighter (0.8f).withAlpha (0.2f));
            g.fillEllipse (point.x - 6.0f, point.y - 6.0f, 12.0f, 12.0f);
        }
    }
}

//==============================================================================
bool CableComponent::hitTest (int x, int y)
{
    auto path = getCablePath();
    juce::PathStrokeType strokeType (8.0f); // Generous hit zone

    juce::Path strokedPath;
    strokeType.createStrokedPath (strokedPath, path);

    return strokedPath.contains (static_cast<float> (x), static_cast<float> (y));
}

//==============================================================================
void CableComponent::timerCallback()
{
    animationPhase += 0.015f;
    if (animationPhase > 1.0f)
        animationPhase -= 1.0f;

    repaint();
}

//==============================================================================
void CableComponent::setEndpoints (juce::Point<float> start, juce::Point<float> end)
{
    startPoint = start;
    endPoint = end;

    // Set bounds to encompass both points with margin
    auto minX = juce::jmin (start.x, end.x) - 20.0f;
    auto minY = juce::jmin (start.y, end.y) - 40.0f;
    auto maxX = juce::jmax (start.x, end.x) + 20.0f;
    auto maxY = juce::jmax (start.y, end.y) + 60.0f;

    setBounds ((int) minX, (int) minY,
               (int) (maxX - minX), (int) (maxY - minY));
    repaint();
}

void CableComponent::setAnimated (bool shouldAnimate)
{
    animated = shouldAnimate;
    if (animated)
        startTimerHz (30);
    else
        stopTimer();
}

//==============================================================================
juce::Path CableComponent::getCablePath() const
{
    // Convert from parent coords to local coords
    auto localStart = getLocalPoint (getParentComponent(), startPoint.toInt()).toFloat();
    auto localEnd   = getLocalPoint (getParentComponent(), endPoint.toInt()).toFloat();

    juce::Path path;

    // Calculate Bézier control points with gravity droop
    float dx = localEnd.x - localStart.x;
    float dy = localEnd.y - localStart.y;
    float dist = std::sqrt (dx * dx + dy * dy);
    float droop = juce::jmin (dist * 0.3f, 80.0f); // Gravity droop

    auto cp1 = juce::Point<float> (localStart.x + dx * 0.3f,
                                    localStart.y + droop);
    auto cp2 = juce::Point<float> (localEnd.x - dx * 0.3f,
                                    localEnd.y + droop);

    path.startNewSubPath (localStart);
    path.cubicTo (cp1, cp2, localEnd);

    return path;
}
