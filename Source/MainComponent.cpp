#include "MainComponent.h"

namespace
{
juce::Colour makePanelColour() { return juce::Colour::fromFloatRGBA (0.07f, 0.10f, 0.05f, 0.92f); }
juce::Colour makeSidebarColour() { return juce::Colour::fromFloatRGBA (0.03f, 0.06f, 0.02f, 0.96f); }
juce::Colour makeTextMuted() { return juce::Colour::fromFloatRGBA (0.90f, 1.00f, 0.76f, 0.82f); }
juce::String noteNameFromPitchClass (int pitchClass)
{
    static constexpr const char* names[] = { "C", "Db", "D", "Eb", "E", "F", "F#", "G", "Ab", "A", "Bb", "B" };
    pitchClass %= 12;
    if (pitchClass < 0)
        pitchClass += 12;
    return names[pitchClass];
}
juce::String noteNameForHarmonySpace (int row, int col)
{
    auto pitchClass = (col * 4 + row * 3) % 12;
    if (pitchClass < 0)
        pitchClass += 12;
    return noteNameFromPitchClass (pitchClass);
}
juce::String harmonySpaceLayerShortName (int layer)
{
    static constexpr const char* names[] = { "MEL", "HARM", "BASS", "INNER", "PHRASE", "UPPER", "COLOR", "WORM" };
    return juce::isPositiveAndBelow (layer, (int) std::size (names)) ? names[layer] : "L";
}
juce::String harmonySpaceLayerLongName (int layer)
{
    static constexpr const char* names[] = {
        "Melody",
        "Harmony Map",
        "Bass Pedal",
        "Inner Voices",
        "Phrase Shape",
        "Upper Answer",
        "Colour",
        "Wormholes"
    };
    return juce::isPositiveAndBelow (layer, (int) std::size (names)) ? names[layer] : "Layer";
}
juce::Colour harmonySpaceLayerColour (int layer)
{
    static const std::array<juce::Colour, 8> colours {
        juce::Colour::fromFloatRGBA (0.98f, 0.90f, 0.26f, 1.0f),
        juce::Colour::fromFloatRGBA (0.16f, 0.96f, 0.88f, 1.0f),
        juce::Colour::fromFloatRGBA (0.92f, 0.54f, 0.18f, 1.0f),
        juce::Colour::fromFloatRGBA (0.90f, 0.82f, 0.58f, 1.0f),
        juce::Colour::fromFloatRGBA (0.96f, 0.64f, 0.24f, 1.0f),
        juce::Colour::fromFloatRGBA (0.68f, 0.92f, 1.0f, 1.0f),
        juce::Colour::fromFloatRGBA (0.76f, 0.48f, 1.0f, 1.0f),
        juce::Colour::fromFloatRGBA (0.58f, 1.0f, 0.42f, 1.0f)
    };
    return colours[(size_t) juce::jlimit (0, 7, layer)];
}
int resolvePreferredStrip (int preferredStrip, float midiNote) noexcept
{
    if (preferredStrip >= 0)
        return juce::jlimit (0, 2, preferredStrip);
    return midiNote < 54.0f ? 0 : (midiNote < 72.0f ? 1 : 2);
}
int parseWholeNumberOrFallback (const juce::String& text, int fallback) noexcept
{
    const auto trimmed = text.trim();
    return trimmed.isEmpty() ? fallback : trimmed.getIntValue();
}
juce::String cellularRuleName (MainComponent::CellularRule rule)
{
    switch (rule)
    {
        case MainComponent::CellularRule::bloom: return "Bloom";
        case MainComponent::CellularRule::maze:  return "Maze";
        case MainComponent::CellularRule::coral: return "Coral";
        case MainComponent::CellularRule::pulse: return "Pulse";
    }

    return "Bloom";
}
juce::String cellularRangeLabel (juce::String prefix, MainComponent::CellularRuleRange range)
{
    return prefix + " " + juce::String (range.min) + "-" + juce::String (range.max);
}
int modeIndex (MainComponent::AppMode mode) noexcept
{
    switch (mode)
    {
        case MainComponent::AppMode::glyphGrid: return 0;
        case MainComponent::AppMode::cellularGrid: return 1;
        case MainComponent::AppMode::harmonicGeometry: return 2;
        case MainComponent::AppMode::harmonySpace: return 3;
    }
    return 0;
}
int wrapPitchClass (int pc) noexcept
{
    auto wrapped = pc % 12;
    if (wrapped < 0)
        wrapped += 12;
    return wrapped;
}
float snapMidiToPitchClasses (float midiNote, const juce::Array<int>& pitchClasses) noexcept
{
    if (pitchClasses.isEmpty())
        return midiNote;

    auto bestNote = midiNote;
    auto bestDistance = std::numeric_limits<float>::max();
    const auto rounded = (int) std::round (midiNote);

    for (int candidate = rounded - 12; candidate <= rounded + 12; ++candidate)
    {
        if (! pitchClasses.contains (wrapPitchClass (candidate)))
            continue;

        const auto distance = std::abs ((float) candidate - midiNote);
        if (distance < bestDistance)
        {
            bestDistance = distance;
            bestNote = (float) candidate;
        }
    }

    return bestNote;
}
bool parseCcSpec (const juce::String& text, int& ccNumber, int& ccValue) noexcept
{
    const auto parts = juce::StringArray::fromTokens (text.trim(), "=,: ", "");
    if (parts.isEmpty())
        return false;

    ccNumber = juce::jlimit (0, 127, parts[0].getIntValue());
    ccValue = juce::jlimit (0, 127, parts.size() > 1 ? parts[1].getIntValue() : parts[0].getIntValue());
    return true;
}
bool parseParameterSpec (const juce::String& text,
                         int& explicitStripIndex,
                         juce::String& slotQualifier,
                         juce::String& parameterToken,
                         float& normalizedValue,
                         bool& hasExplicitValue) noexcept
{
    auto spec = text.trim();
    if (spec.isEmpty())
        return false;

    explicitStripIndex = -1;
    slotQualifier.clear();
    parameterToken.clear();
    normalizedValue = 0.0f;
    hasExplicitValue = false;

    auto target = spec;
    if (const auto equalsIndex = spec.indexOfChar ('='); equalsIndex >= 0)
    {
        target = spec.substring (0, equalsIndex).trim();
        normalizedValue = juce::jlimit (0.0f, 1.0f, (float) spec.substring (equalsIndex + 1).trim().getDoubleValue());
        hasExplicitValue = true;
    }

    auto remainder = target;
    if (const auto colonIndex = remainder.indexOfChar (':'); colonIndex >= 0)
    {
        auto firstToken = remainder.substring (0, colonIndex).trim().toLowerCase();
        remainder = remainder.substring (colonIndex + 1).trim();

        if (firstToken.startsWithChar ('s') && firstToken.substring (1).containsOnly ("0123456789"))
        {
            explicitStripIndex = juce::jlimit (0, 2, firstToken.substring (1).getIntValue() - 1);

            if (const auto secondColonIndex = remainder.indexOfChar (':'); secondColonIndex >= 0)
            {
                slotQualifier = remainder.substring (0, secondColonIndex).trim().toLowerCase();
                parameterToken = remainder.substring (secondColonIndex + 1).trim();
            }
            else
            {
                parameterToken = remainder.trim();
            }
        }
        else
        {
            slotQualifier = firstToken;
            parameterToken = remainder.trim();
        }
    }
    else
    {
        parameterToken = remainder.trim();
    }

    return parameterToken.isNotEmpty();
}
juce::AudioProcessorParameter* resolvePluginParameter (juce::AudioProcessor& processor, const juce::String& parameterToken) noexcept
{
    auto token = parameterToken.trim();
    if (token.isEmpty())
        return nullptr;

    if (token.startsWithChar ('#'))
    {
        const auto index = juce::jlimit (0, processor.getParameters().size() - 1, token.fromFirstOccurrenceOf ("#", false, false).getIntValue());
        return processor.getParameters()[index];
    }

    if (token.startsWithIgnoreCase ("id:"))
    {
        const auto idToken = token.fromFirstOccurrenceOf ("id:", false, true).trim();
        for (auto* parameter : processor.getParameters())
        {
            if (parameter == nullptr)
                continue;

            if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
                if (withId->paramID == idToken)
                    return parameter;
        }
    }

    for (auto* parameter : processor.getParameters())
    {
        if (parameter == nullptr)
            continue;

        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            if (withId->paramID == token)
                return parameter;
    }

    for (auto* parameter : processor.getParameters())
    {
        if (parameter == nullptr)
            continue;

        const auto name = parameter->getName (256);
        if (name.equalsIgnoreCase (token))
            return parameter;
    }

    for (auto* parameter : processor.getParameters())
    {
        if (parameter == nullptr)
            continue;

        const auto name = parameter->getName (256);
        if (name.containsIgnoreCase (token))
            return parameter;
    }

    return nullptr;
}
bool isViewToggleKey (const juce::KeyPress& key) noexcept
{
    const auto keyCode = std::tolower ((unsigned char) key.getKeyCode());
    const auto textChar = std::tolower ((unsigned char) key.getTextCharacter());
    return keyCode == 'v' || textChar == 'v';
}
float smoothStep (float value) noexcept
{
    auto x = juce::jlimit (0.0f, 1.0f, value);
    return x * x * (3.0f - 2.0f * x);
}
}

void MainComponent::CellButton::paintButton (juce::Graphics& g, bool over, bool) 
{
    auto area = getLocalBounds().toFloat();
    auto def = MainComponent::getGlyphDefinition (type);
    auto fill = def.colour.withAlpha (type == GlyphType::empty ? (hasAnyLayerContent ? 0.13f : 0.08f) : 0.18f);
    if (isSnakeHead)
        fill = fill.interpolatedWith (snakeColour.withAlpha (0.42f), 0.34f);

    if (over)
        fill = fill.brighter (0.12f);

    juce::ColourGradient cellGlow (def.colour.withAlpha (type == GlyphType::empty ? 0.08f : (over ? 0.24f : 0.16f)),
                                   area.getCentreX(), area.getY(),
                                   juce::Colour::fromFloatRGBA (0.02f, 0.05f, 0.03f, 0.0f),
                                   area.getCentreX(), area.getBottom(),
                                   false);
    g.setGradientFill (cellGlow);
    g.fillRoundedRectangle (area.expanded (1.5f), 14.0f);
    g.setColour (fill);
    g.fillRoundedRectangle (area, 12.0f);
    juce::ColourGradient innerCell (def.colour.withAlpha (type == GlyphType::empty ? 0.03f : 0.14f),
                                    area.getTopLeft(),
                                    juce::Colour::fromFloatRGBA (0.02f, 0.05f, 0.03f, 0.0f),
                                    area.getBottomRight(),
                                    false);
    g.setGradientFill (innerCell);
    g.fillRoundedRectangle (area.reduced (1.0f), 11.0f);

    if (isActiveColumn)
    {
        const auto occupancyInset = isSnakeHead ? 2.5f : 4.0f;
        auto occupancy = area.reduced (occupancyInset);
        g.setColour (snakeColour.withAlpha (isGhostSnake ? 0.08f : (isSnakeHead ? 0.24f : 0.16f)));
        g.fillRoundedRectangle (occupancy, 10.0f);
        g.setColour (snakeColour.withAlpha (isGhostSnake ? 0.22f : (isSnakeHead ? 0.78f : 0.46f)));
        g.drawRoundedRectangle (occupancy.reduced (0.5f), 10.0f, isSnakeHead ? 1.6f : 1.0f);
    }

    if (isAutomataActive)
    {
        auto automataBox = area.reduced (isAutomataNewborn ? 3.0f : 4.5f);
        const auto automataColour = isAutomataNewborn
                                        ? juce::Colour::fromFloatRGBA (0.96f, 1.0f, 0.34f, 1.0f)
                                        : juce::Colour::fromFloatRGBA (0.48f, 1.0f, 0.14f, 1.0f);
        g.setColour (automataColour.withAlpha (isAutomataNewborn ? 0.24f : 0.16f));
        g.fillRoundedRectangle (automataBox, 10.0f);
        g.setColour (automataColour.withAlpha (isAutomataNewborn ? 0.92f : 0.58f));
        g.drawRoundedRectangle (automataBox.reduced (0.5f), 10.0f, isAutomataNewborn ? 1.8f : 1.1f);
    }

    if (pluginActivity > 0.001f)
    {
        auto pluginBox = area.reduced (3.5f);
        const auto pluginGlow = juce::Colour::fromFloatRGBA (0.18f, 0.98f, 1.0f, 1.0f);
        g.setColour (pluginGlow.withAlpha (0.10f + pluginActivity * 0.22f));
        g.fillRoundedRectangle (pluginBox, 11.0f);
        g.setColour (pluginGlow.withAlpha (0.34f + pluginActivity * 0.46f));
        g.drawRoundedRectangle (pluginBox.reduced (0.5f), 11.0f, 1.1f + pluginActivity * 0.8f);
    }

    if (showsNextStep)
    {
        auto nextBox = area.reduced (6.0f, 6.0f);
        g.setColour (snakeColour.withAlpha (0.12f));
        g.fillRoundedRectangle (nextBox, 9.0f);
        g.setColour (snakeColour.withAlpha (0.58f));
        g.drawRoundedRectangle (nextBox.reduced (0.5f), 9.0f, 1.2f);
    }

    if (isSnakeHead)
    {
        auto headHalo = area.reduced (2.5f);
        g.setColour (snakeColour.withAlpha (0.16f));
        g.fillRoundedRectangle (headHalo, 12.0f);
        g.setColour (snakeColour.withAlpha (0.88f));
        g.drawRoundedRectangle (headHalo.reduced (0.5f), 12.0f, 1.8f);
    }

    g.setColour (juce::Colours::white.withAlpha (isSelected ? 0.36f : (isActiveColumn ? 0.18f : 0.10f)));
    g.drawRoundedRectangle (area.reduced (0.5f), 12.0f, isSelected ? 1.4f : 1.0f);
    g.setColour (def.colour.withAlpha (isSelected ? 0.76f : (over ? 0.54f : 0.28f)));
    g.drawRoundedRectangle (area.reduced (1.4f), 11.0f, isSelected ? 1.4f : 1.0f);

    g.setColour (makeTextMuted());
    g.setFont (juce::FontOptions (11.0f, juce::Font::plain));
    g.drawText (def.shortName + " " + juce::String (row + 1) + ":" + juce::String (col + 1),
                getLocalBounds().removeFromTop (22).reduced (8, 4),
                juce::Justification::centredLeft,
                true);

    g.setColour (juce::Colours::white.withAlpha (type == GlyphType::empty ? (hasAnyLayerContent ? 0.56f : 0.38f) : 0.88f));
    g.setFont (juce::FontOptions (12.0f, juce::Font::plain));
    g.drawFittedText (code.isEmpty() ? (hasAnyLayerContent ? "stacked" : "") : code,
                      getLocalBounds().reduced (8).withTrimmedTop (18),
                      juce::Justification::centredLeft,
                      2);

    if (! overlayBadges.isEmpty())
    {
        auto badgeArea = getLocalBounds().reduced (8, 7).removeFromRight (72);
        badgeArea = badgeArea.removeFromTop (18);

        for (int badgeIndex = 0; badgeIndex < overlayBadges.size(); ++badgeIndex)
        {
            auto pillBounds = badgeArea.removeFromRight (juce::jmin (36, 12 + overlayBadges[badgeIndex].length() * 7));
            badgeArea.removeFromRight (4);

            const auto pill = pillBounds.toFloat();
            const auto badgeColour = overlayBadgeColours[badgeIndex];
            g.setColour (badgeColour.withAlpha (0.20f));
            g.fillRoundedRectangle (pill, 7.0f);
            g.setColour (badgeColour.withAlpha (0.88f));
            g.drawRoundedRectangle (pill.reduced (0.5f), 7.0f, 1.0f);
            g.setColour (juce::Colours::white.withAlpha (0.94f));
            g.setFont (juce::FontOptions (9.5f, juce::Font::bold));
            g.drawFittedText (overlayBadges[badgeIndex], pillBounds, juce::Justification::centred, 1);
        }
    }

    if (isSnakeHead && ! isGhostSnake && (snakeDirectionX != 0 || snakeDirectionY != 0 || snakeDirectionLayer != 0))
    {
        const auto centre = area.getCentre();
        const auto arrowLength = juce::jmin (area.getWidth(), area.getHeight()) * 0.18f;
        const auto endPoint = juce::Point<float> (centre.x + snakeDirectionX * arrowLength,
                                                  centre.y + snakeDirectionY * arrowLength);
        juce::Path heading;
        heading.startNewSubPath (centre);
        heading.lineTo (endPoint);

        g.setColour (snakeColour.withAlpha (0.96f));
        g.strokePath (heading, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

        if (snakeDirectionX != 0 || snakeDirectionY != 0)
        {
            const auto angle = std::atan2 ((float) snakeDirectionY, (float) snakeDirectionX);
            const auto wing = juce::jmin (area.getWidth(), area.getHeight()) * 0.06f;
            juce::Path arrowHead;
            arrowHead.startNewSubPath (endPoint);
            arrowHead.lineTo (endPoint.x - std::cos (angle - 0.62f) * wing, endPoint.y - std::sin (angle - 0.62f) * wing);
            arrowHead.startNewSubPath (endPoint);
            arrowHead.lineTo (endPoint.x - std::cos (angle + 0.62f) * wing, endPoint.y - std::sin (angle + 0.62f) * wing);
            g.strokePath (arrowHead, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        }

        if (snakeDirectionLayer != 0)
        {
            auto tag = juce::Rectangle<float> (area.getRight() - 20.0f, area.getY() + 4.0f, 16.0f, 12.0f);
            g.setColour (juce::Colour::fromFloatRGBA (0.02f, 0.05f, 0.03f, 0.92f));
            g.fillRoundedRectangle (tag, 5.0f);
            g.setColour (snakeColour.withAlpha (0.94f));
            g.drawRoundedRectangle (tag.reduced (0.5f), 5.0f, 0.9f);
            g.setColour (juce::Colours::white.withAlpha (0.96f));
            g.setFont (juce::FontOptions (8.5f, juce::Font::bold));
            g.drawFittedText (snakeDirectionLayer > 0 ? "Z+" : "Z-",
                              tag.toNearestInt(),
                              juce::Justification::centred,
                              1);
        }
    }

    if (hasHiddenLayerContent && hiddenLayerBadges.size() > 0)
    {
        auto badgeArea = getLocalBounds().removeFromBottom (20).reduced (4, 2);
        auto x = badgeArea.getX();
        g.setFont (juce::FontOptions (9.5f, juce::Font::bold));

        for (int i = 0; i < hiddenLayerBadges.size(); ++i)
        {
            const auto badgeText = hiddenLayerBadges[i];
            const auto textWidth = juce::jlimit (16, 34, 10 + (int) badgeText.length() * 6);
            auto badge = juce::Rectangle<float> ((float) x, (float) badgeArea.getY(), (float) textWidth, (float) badgeArea.getHeight());
            const auto badgeColour = i < hiddenLayerBadgeColours.size() ? hiddenLayerBadgeColours.getReference (i) : juce::Colours::white;

            g.setColour (juce::Colour::fromFloatRGBA (0.04f, 0.07f, 0.03f, 0.86f));
            g.fillRoundedRectangle (badge, 8.0f);
            g.setColour (badgeColour.withAlpha (0.82f));
            g.drawRoundedRectangle (badge.reduced (0.5f), 8.0f, 1.0f);
            g.setColour (badgeColour.brighter (0.2f).withAlpha (0.98f));
            g.drawFittedText (badgeText, badge.toNearestInt(), juce::Justification::centred, 1);

            x += textWidth + 4;

            if (x > badgeArea.getRight() - 18)
                break;
        }
    }
}

void MainComponent::StageComponent::setState (ScoreResult resultIn)
{
    if (result.hue != 0.0f || result.energy != 0.0f)
    {
        auto targetHue = resultIn.hue;
        auto currentHue = result.hue;

        if (std::abs (targetHue - currentHue) > 0.5f)
        {
            if (targetHue < currentHue)
                targetHue += 1.0f;
            else
                currentHue += 1.0f;
        }

        resultIn.hue = std::fmod (currentHue + (targetHue - currentHue) * 0.18f, 1.0f);
        resultIn.energy = result.energy + (resultIn.energy - result.energy) * 0.22f;
    }

    for (int i = trailFrames.size(); --i >= 0;)
    {
        auto& frame = trailFrames.getReference (i);
        frame.alpha *= 0.72f;

        if (frame.alpha < 0.035f)
            trailFrames.remove (i);
    }

    if (! resultIn.snakes.isEmpty())
    {
        TrailFrame frame;
        frame.alpha = 0.28f;
        frame.snakes = resultIn.snakes;
        trailFrames.insert (0, std::move (frame));

        while (trailFrames.size() > 6)
            trailFrames.removeLast();
    }

    result = std::move (resultIn);
    repaint();
}

void MainComponent::StageComponent::setPseudoDepthEnabled (bool shouldUsePseudoDepth)
{
    usePseudoDepthView = shouldUsePseudoDepth;
    repaint();
}

void MainComponent::StageComponent::setHarmonySpaceCallbacks (std::function<void (int, int)> noteCallbackIn,
                                                              std::function<void (int)> layerCallbackIn,
                                                              std::function<void (int)> controlCallbackIn)
{
    harmonySpaceNoteCallback = std::move (noteCallbackIn);
    harmonySpaceLayerCallback = std::move (layerCallbackIn);
    harmonySpaceControlCallback = std::move (controlCallbackIn);
}

void MainComponent::StageComponent::mouseDown (const juce::MouseEvent& event)
{
    if (result.mode != MainComponent::AppMode::harmonySpace)
        return;

    auto bounds = getLocalBounds().toFloat();
    auto stageBounds = bounds.reduced (24.0f, 22.0f);
    auto innerBounds = stageBounds.reduced (24.0f);
    auto sideLeft = innerBounds.removeFromLeft (70.0f);
    innerBounds.removeFromLeft (14.0f);
    auto sideRight = innerBounds.removeFromRight (108.0f);
    innerBounds.removeFromRight (14.0f);
    auto content = innerBounds;
    auto keyboardBounds = content.removeFromTop (34.0f).reduced (8.0f, 0.0f);
    content.removeFromTop (14.0f);
    content.removeFromBottom (58.0f);
    content.removeFromBottom (12.0f);
    auto noteArea = content.reduced (24.0f, 22.0f);
    auto latticeFrame = noteArea.reduced (56.0f, 42.0f);
    const int visibleCols = 7;
    const int visibleRows = 6;
    const int colStart = 0;
    const int rowStart = 0;
    const auto spacingX = latticeFrame.getWidth() / (float) (visibleCols + 2.85f);
    const auto spacingY = latticeFrame.getHeight() / (float) (visibleRows + 2.15f);
    const auto basePointFor = [&] (int row, int col) -> juce::Point<float>
    {
        const auto localRow = (float) (row - rowStart);
        const auto localCol = (float) (col - colStart);
        return { spacingX * (0.75f + localCol + localRow * 0.5f),
                 spacingY * (0.72f + localRow) };
    };
    auto minPoint = juce::Point<float> (std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
    auto maxPoint = juce::Point<float> (std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
    for (int row = rowStart; row < rowStart + visibleRows; ++row)
    {
        for (int col = colStart; col < colStart + visibleCols; ++col)
        {
            const auto point = basePointFor (row, col);
            minPoint.x = juce::jmin (minPoint.x, point.x);
            minPoint.y = juce::jmin (minPoint.y, point.y);
            maxPoint.x = juce::jmax (maxPoint.x, point.x);
            maxPoint.y = juce::jmax (maxPoint.y, point.y);
        }
    }
    const auto latticePaddingX = spacingX * 0.75f;
    const auto latticePaddingY = spacingY * 0.58f;
    const auto latticeWidth = (maxPoint.x - minPoint.x) + latticePaddingX * 2.0f;
    const auto latticeHeight = (maxPoint.y - minPoint.y) + latticePaddingY * 2.0f;
    const auto offsetX = latticeFrame.getCentreX() - latticeWidth * 0.5f - minPoint.x + latticePaddingX;
    const auto offsetY = latticeFrame.getCentreY() - latticeHeight * 0.5f - minPoint.y + latticePaddingY;
    const auto pointFor = [&] (int row, int col) -> juce::Point<float>
    {
        auto point = basePointFor (row, col);
        point.x += offsetX;
        point.y += offsetY;
        return point;
    };

    const auto pos = event.position;

    for (int layer = 0; layer < layers; ++layer)
    {
        const auto layerGap = noteArea.getHeight() / (float) juce::jmax (layers, 1);
        const auto chipHeight = juce::jlimit (20.0f, 26.0f, layerGap - 6.0f);
        auto chip = juce::Rectangle<float> (sideLeft.getX() + 8.0f,
                                            noteArea.getY() + layerGap * layer + (layerGap - chipHeight) * 0.5f,
                                            sideLeft.getWidth() - 16.0f,
                                            chipHeight);
        if (chip.contains (pos))
        {
            if (harmonySpaceLayerCallback)
                harmonySpaceLayerCallback (layer);
            return;
        }
    }

    auto controlsBounds = juce::Rectangle<float> (sideRight.getX() + 10.0f,
                                                  noteArea.getY() + 2.0f,
                                                  sideRight.getWidth() - 20.0f,
                                                  noteArea.getHeight() - 10.0f);
    const auto controlStep = controlsBounds.getHeight() / 9.6f;
    const auto controlHeight = juce::jlimit (24.0f, 29.0f, controlStep - 7.0f);
    for (int i = 0; i < 8; ++i)
    {
        auto button = juce::Rectangle<float> (controlsBounds.getX(),
                                              controlsBounds.getY() + controlStep * i,
                                              controlsBounds.getWidth(),
                                              controlHeight);
        if (button.contains (pos))
        {
            if (harmonySpaceControlCallback)
                harmonySpaceControlCallback (i);
            return;
        }
    }

    if (keyboardBounds.contains (pos))
    {
        const auto keyWidth = keyboardBounds.getWidth() / 12.0f;
        const auto keyIndex = juce::jlimit (0, 11, (int) std::floor ((pos.x - keyboardBounds.getX()) / keyWidth));
        if (harmonySpaceControlCallback)
            harmonySpaceControlCallback (100 + keyIndex);
        return;
    }

    for (int row = rowStart; row < rowStart + visibleRows; ++row)
    {
        for (int col = colStart; col < colStart + visibleCols; ++col)
        {
            const auto point = pointFor (row, col);
            auto noteBounds = juce::Rectangle<float> (point.x - spacingX * 0.22f,
                                                      point.y - spacingY * 0.18f,
                                                      spacingX * 0.44f,
                                                      spacingY * 0.36f);
            if (noteBounds.expanded (4.0f).contains (pos))
            {
                if (harmonySpaceNoteCallback)
                    harmonySpaceNoteCallback (row, col);
                return;
            }
        }
    }
}

void MainComponent::StageComponent::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds().toFloat();
    const auto isHarmonicMode = result.mode == MainComponent::AppMode::harmonicGeometry;
    const auto isCellularMode = result.mode == MainComponent::AppMode::cellularGrid;
    const auto isHarmonySpaceMode = result.mode == MainComponent::AppMode::harmonySpace;
    juce::ColourGradient gradient (
        isHarmonicMode ? juce::Colour::fromFloatRGBA (0.20f, 0.16f, 0.42f, 1.0f)
                       : (isCellularMode ? juce::Colour::fromFloatRGBA (0.08f, 0.07f, 0.22f, 1.0f)
                       : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.08f, 0.06f, 0.02f, 1.0f)
                                             : juce::Colour::fromHSV (juce::jlimit (0.0f, 1.0f, result.hue + 0.12f), 0.95f, 0.34f, 1.0f))),
        bounds.getTopLeft(),
        isHarmonicMode ? juce::Colour::fromFloatRGBA (0.82f, 0.38f, 0.24f, 1.0f)
                       : (isCellularMode ? juce::Colour::fromFloatRGBA (0.04f, 0.34f, 0.38f, 1.0f)
                       : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.03f, 0.26f, 0.22f, 1.0f)
                                             : juce::Colour::fromHSV (std::fmod (result.hue + 0.42f, 1.0f), 1.0f, 0.18f, 1.0f))),
        bounds.getBottomRight(),
        false);
    g.setGradientFill (gradient);
    g.fillRoundedRectangle (bounds, 14.0f);

    const auto cellWidth = bounds.getWidth() / static_cast<float> (cols);
    const auto rowHeight = bounds.getHeight() / static_cast<float> (rows);

    if (usePseudoDepthView)
    {
        if (isHarmonySpaceMode)
        {
            auto stageBounds = bounds.reduced (24.0f, 22.0f);
            g.setColour (juce::Colour::fromFloatRGBA (0.05f, 0.05f, 0.02f, 0.96f));
            g.fillRoundedRectangle (stageBounds, 18.0f);

            juce::ColourGradient stageBloom (juce::Colour::fromFloatRGBA (0.92f, 0.78f, 0.18f, 0.10f),
                                             stageBounds.getTopLeft(),
                                             juce::Colour::fromFloatRGBA (0.10f, 0.62f, 0.56f, 0.08f),
                                             stageBounds.getBottomRight(),
                                             false);
            g.setGradientFill (stageBloom);
            g.fillRoundedRectangle (stageBounds.reduced (1.0f), 18.0f);

            auto innerBounds = stageBounds.reduced (24.0f);
            auto sideLeft = innerBounds.removeFromLeft (70.0f);
            innerBounds.removeFromLeft (14.0f);
            auto sideRight = innerBounds.removeFromRight (108.0f);
            innerBounds.removeFromRight (14.0f);
            auto content = innerBounds;
            auto keyboardBounds = content.removeFromTop (34.0f).reduced (8.0f, 0.0f);
            content.removeFromTop (14.0f);
            auto statusBounds = content.removeFromBottom (58.0f);
            content.removeFromBottom (12.0f);
            auto noteArea = content.reduced (24.0f, 22.0f);
            auto latticeFrame = noteArea.reduced (56.0f, 42.0f);

            g.setColour (juce::Colour::fromFloatRGBA (0.01f, 0.02f, 0.01f, 0.90f));
            g.fillRoundedRectangle (noteArea.expanded (12.0f, 10.0f), 18.0f);
            g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.90f, 0.28f, 0.16f));
            g.drawRoundedRectangle (noteArea.expanded (12.0f, 10.0f), 18.0f, 1.2f);

            const int visibleCols = 7;
            const int visibleRows = 6;
            const int colStart = 0;
            const int rowStart = 0;
            const auto spacingX = latticeFrame.getWidth() / (float) (visibleCols + 2.85f);
            const auto spacingY = latticeFrame.getHeight() / (float) (visibleRows + 2.15f);
            const auto basePointFor = [&] (int row, int col) -> juce::Point<float>
            {
                const auto localRow = (float) (row - rowStart);
                const auto localCol = (float) (col - colStart);
                return { spacingX * (0.75f + localCol + localRow * 0.5f),
                         spacingY * (0.72f + localRow) };
            };
            auto minPoint = juce::Point<float> (std::numeric_limits<float>::max(), std::numeric_limits<float>::max());
            auto maxPoint = juce::Point<float> (std::numeric_limits<float>::lowest(), std::numeric_limits<float>::lowest());
            for (int row = rowStart; row < rowStart + visibleRows; ++row)
            {
                for (int col = colStart; col < colStart + visibleCols; ++col)
                {
                    const auto point = basePointFor (row, col);
                    minPoint.x = juce::jmin (minPoint.x, point.x);
                    minPoint.y = juce::jmin (minPoint.y, point.y);
                    maxPoint.x = juce::jmax (maxPoint.x, point.x);
                    maxPoint.y = juce::jmax (maxPoint.y, point.y);
                }
            }
            const auto latticePaddingX = spacingX * 0.75f;
            const auto latticePaddingY = spacingY * 0.58f;
            const auto latticeWidth = (maxPoint.x - minPoint.x) + latticePaddingX * 2.0f;
            const auto latticeHeight = (maxPoint.y - minPoint.y) + latticePaddingY * 2.0f;
            const auto offsetX = latticeFrame.getCentreX() - latticeWidth * 0.5f - minPoint.x + latticePaddingX;
            const auto offsetY = latticeFrame.getCentreY() - latticeHeight * 0.5f - minPoint.y + latticePaddingY;
            const auto pointFor = [&] (int row, int col) -> juce::Point<float>
            {
                auto point = basePointFor (row, col);
                point.x += offsetX;
                point.y += offsetY;
                return point;
            };

            g.saveState();
            g.reduceClipRegion (latticeFrame.expanded (8.0f, 8.0f).toNearestInt());

            for (int row = rowStart; row < rowStart + visibleRows - 1; ++row)
            {
                for (int col = colStart; col < colStart + visibleCols - 1; ++col)
                {
                    juce::Path cellBand;
                    cellBand.startNewSubPath (pointFor (row, col));
                    cellBand.lineTo (pointFor (row, col + 1));
                    cellBand.lineTo (pointFor (row + 1, col + 1));
                    cellBand.lineTo (pointFor (row + 1, col));
                    cellBand.closeSubPath();
                    g.setColour (((row + col) & 1) == 0
                                     ? juce::Colour::fromFloatRGBA (1.0f, 0.84f, 0.20f, 0.035f)
                                     : juce::Colour::fromFloatRGBA (0.02f, 0.02f, 0.01f, 0.08f));
                    g.fillPath (cellBand);
                }
            }

            for (int row = rowStart; row < rowStart + visibleRows; ++row)
            {
                for (int col = colStart; col < colStart + visibleCols; ++col)
                {
                    if (col < colStart + visibleCols - 1)
                    {
                        juce::Path link;
                        link.startNewSubPath (pointFor (row, col));
                        link.lineTo (pointFor (row, col + 1));
                        g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.88f, 0.24f, 0.12f));
                        g.strokePath (link, juce::PathStrokeType (0.9f));
                    }

                    if (row < rowStart + visibleRows - 1)
                    {
                        juce::Path link;
                        link.startNewSubPath (pointFor (row, col));
                        link.lineTo (pointFor (row + 1, col));
                        g.setColour (juce::Colour::fromFloatRGBA (0.16f, 0.90f, 0.84f, 0.12f));
                        g.strokePath (link, juce::PathStrokeType (0.9f));
                    }

                    if (row < rowStart + visibleRows - 1 && col < colStart + visibleCols - 1)
                    {
                        juce::Path link;
                        link.startNewSubPath (pointFor (row, col));
                        link.lineTo (pointFor (row + 1, col + 1));
                        g.setColour (juce::Colour::fromFloatRGBA (1.0f, 1.0f, 0.88f, 0.05f));
                        g.strokePath (link, juce::PathStrokeType (0.6f));
                    }
                }
            }

            g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.90f, 0.28f, 0.46f));
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.drawText ("M3", juce::Rectangle<float> (latticeFrame.getX() + spacingX * 2.2f, latticeFrame.getY() - 18.0f, 40.0f, 16.0f).toNearestInt(), juce::Justification::centred, false);

            juce::Array<GridCellVisual> activeHarmonyCells;
            juce::Array<juce::Point<float>> triggeredHarmonyPoints;
            for (auto& gridCell : result.gridCells)
            {
                const auto isHarmonyCell = gridCell.type == GlyphType::tone
                                        || gridCell.type == GlyphType::voice
                                        || gridCell.type == GlyphType::chord
                                        || gridCell.type == GlyphType::key
                                        || gridCell.type == GlyphType::octave;

                if (isHarmonyCell && gridCell.layer == result.activeLayer
                    && gridCell.col >= colStart && gridCell.col < colStart + visibleCols
                    && gridCell.row >= rowStart && gridCell.row < rowStart + visibleRows)
                {
                    activeHarmonyCells.add (gridCell);
                    if (gridCell.isTriggered)
                        triggeredHarmonyPoints.add (pointFor (gridCell.row, gridCell.col));
                }
            }

            juce::Array<int> keyPitchClasses;
            juce::Array<int> chordPitchClasses;
            static constexpr int majorSet[] = { 0, 2, 4, 5, 7, 9, 11 };
            static constexpr int chordSets[][4] = {
                { 0, 4, 7, -1 },
                { 0, 3, 7, -1 },
                { 0, 4, 8, -1 },
                { 0, 5, 7, -1 },
                { 0, 2, 7, -1 },
                { 0, 7, 10, -1 },
                { 0, 5, 9, -1 }
            };

            for (auto degree : majorSet)
                keyPitchClasses.add ((result.harmonySpaceKeyCenter + degree) % 12);

            auto activeChordType = 0;
            for (auto& gridCell : activeHarmonyCells)
            {
                if (gridCell.type == GlyphType::chord)
                {
                    activeChordType = juce::jlimit (0, 6, gridCell.code.getIntValue() % 7);
                    break;
                }
            }

            for (auto degree : chordSets[activeChordType])
            {
                if (degree >= 0)
                    chordPitchClasses.add ((result.harmonySpaceKeyCenter + degree) % 12);
            }

            if (! chordPitchClasses.isEmpty())
            {
                juce::Array<juce::Point<float>> chordPoints;
                for (int row = rowStart; row < rowStart + visibleRows; ++row)
                {
                    for (int col = colStart; col < colStart + visibleCols; ++col)
                    {
                        auto pitchClass = (col * 4 + row * 3) % 12;
                        if (pitchClass < 0)
                            pitchClass += 12;
                        if (chordPitchClasses.contains (pitchClass))
                            chordPoints.add (pointFor (row, col));
                    }
                }

                if (! chordPoints.isEmpty())
                {
                    auto minX = std::numeric_limits<float>::max();
                    auto minY = std::numeric_limits<float>::max();
                    auto maxX = std::numeric_limits<float>::lowest();
                    auto maxY = std::numeric_limits<float>::lowest();

                    for (auto& p : chordPoints)
                    {
                        minX = juce::jmin (minX, p.x);
                        minY = juce::jmin (minY, p.y);
                        maxX = juce::jmax (maxX, p.x);
                        maxY = juce::jmax (maxY, p.y);
                    }

                    auto chordWindow = juce::Rectangle<float> (minX - spacingX * 0.24f,
                                                               minY - spacingY * 0.18f,
                                                               (maxX - minX) + spacingX * 0.48f,
                                                               (maxY - minY) + spacingY * 0.36f)
                                           .getIntersection (latticeFrame.expanded (4.0f));
                    g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.84f, 0.20f, 0.035f));
                    g.fillRoundedRectangle (chordWindow, 16.0f);
                    g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.84f, 0.20f, 0.28f));
                    g.drawRoundedRectangle (chordWindow, 16.0f, 1.0f);
                }
            }

            if (result.harmonySpaceGesturePoints.size() > 1)
            {
                juce::Path gesturePath;
                for (int i = 0; i < result.harmonySpaceGesturePoints.size(); ++i)
                {
                    const auto point = result.harmonySpaceGesturePoints.getReference (i);
                    if (point.y < colStart || point.y >= colStart + visibleCols || point.x < rowStart || point.x >= rowStart + visibleRows)
                        continue;
                    auto p = pointFor (point.x, point.y);
                    if (gesturePath.isEmpty())
                        gesturePath.startNewSubPath (p);
                    else
                        gesturePath.lineTo (p);
                }

                if (! gesturePath.isEmpty())
                {
                    g.setColour (juce::Colour::fromFloatRGBA (0.10f, 0.96f, 0.90f, 0.06f));
                    g.strokePath (gesturePath, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.90f, 0.22f, 0.34f));
                    g.strokePath (gesturePath, juce::PathStrokeType (1.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }
            }

            if (triggeredHarmonyPoints.size() > 0)
            {
                const auto pulse = 0.65f + 0.35f * std::sin (result.transportPhase * juce::MathConstants<float>::twoPi);

                if (triggeredHarmonyPoints.size() > 1)
                {
                    juce::Path triggerPath;
                    for (int i = 0; i < triggeredHarmonyPoints.size(); ++i)
                    {
                        const auto point = triggeredHarmonyPoints.getReference (i);
                        if (i == 0)
                            triggerPath.startNewSubPath (point);
                        else
                            triggerPath.lineTo (point);
                    }

                    g.setColour (juce::Colour::fromFloatRGBA (0.12f, 0.98f, 0.96f, 0.08f * pulse));
                    g.strokePath (triggerPath, juce::PathStrokeType (5.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.96f, 0.44f, 0.42f * pulse));
                    g.strokePath (triggerPath, juce::PathStrokeType (1.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                }

                for (auto& point : triggeredHarmonyPoints)
                {
                    g.setColour (juce::Colour::fromFloatRGBA (0.10f, 0.98f, 0.94f, 0.12f * pulse));
                    g.fillEllipse (point.x - spacingX * 0.26f, point.y - spacingY * 0.22f, spacingX * 0.52f, spacingY * 0.44f);
                    g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.97f, 0.66f, 0.78f * pulse));
                    g.drawEllipse (point.x - spacingX * 0.20f, point.y - spacingY * 0.17f, spacingX * 0.40f, spacingY * 0.34f, 1.4f);
                }
            }

            for (int row = rowStart; row < rowStart + visibleRows; ++row)
            {
                for (int col = colStart; col < colStart + visibleCols; ++col)
                {
                    const auto point = pointFor (row, col);
                    auto noteBounds = juce::Rectangle<float> (point.x - spacingX * 0.18f,
                                                              point.y - spacingY * 0.15f,
                                                              spacingX * 0.36f,
                                                              spacingY * 0.30f);

                    bool hasActiveCell = false;
                    bool isTriggered = false;
                    GlyphType activeType = GlyphType::empty;

                    for (auto& gridCell : activeHarmonyCells)
                    {
                        if (gridCell.row == row && gridCell.col == col)
                        {
                            hasActiveCell = true;
                            isTriggered = gridCell.isTriggered;
                            activeType = gridCell.type;
                            break;
                        }
                    }

                    const auto pitchText = noteNameForHarmonySpace (row, col);
                    auto pitchClass = (col * 4 + row * 3) % 12;
                    if (pitchClass < 0)
                        pitchClass += 12;
                    const auto inKey = keyPitchClasses.contains (pitchClass);
                    const auto inChord = chordPitchClasses.contains (pitchClass);
                    const auto nodeColour = hasActiveCell
                                                ? MainComponent::getGlyphDefinition (activeType).colour
                                                : juce::Colour::fromFloatRGBA (1.0f, 0.92f, 0.20f, 1.0f);
                    const auto baseAlpha = result.harmonySpaceConstraintMode == 2 ? (inChord ? 0.16f : 0.03f)
                                         : (result.harmonySpaceConstraintMode == 1 ? (inKey ? 0.10f : 0.03f)
                                                                                  : 0.07f);
                    g.setColour (nodeColour.withAlpha (hasActiveCell ? 0.14f : baseAlpha));
                    g.fillEllipse (noteBounds.expanded (hasActiveCell ? 3.0f : (inChord ? 2.2f : 1.4f)));
                    g.setColour (hasActiveCell ? juce::Colour::fromFloatRGBA (0.02f, 0.03f, 0.03f, 0.98f)
                                               : juce::Colour::fromFloatRGBA (0.08f, 0.08f, 0.02f, 0.94f));
                    g.fillEllipse (noteBounds);
                    g.setColour ((inChord ? juce::Colour::fromFloatRGBA (1.0f, 0.96f, 0.82f, 1.0f) : nodeColour)
                                     .withAlpha (hasActiveCell ? 0.98f : (inKey ? 0.90f : 0.42f)));
                    g.drawEllipse (noteBounds, hasActiveCell ? 1.4f : (inChord ? 1.1f : 0.9f));

                    if (isTriggered)
                    {
                        g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.98f, 0.80f, 0.18f));
                        g.fillEllipse (noteBounds.expanded (5.0f));
                        g.setColour (juce::Colour::fromFloatRGBA (0.16f, 0.98f, 0.94f, 0.92f));
                        g.drawEllipse (noteBounds.expanded (1.8f), 1.8f);
                    }

                    g.setColour (hasActiveCell ? juce::Colours::white
                                               : (inKey ? juce::Colour::fromFloatRGBA (1.0f, 0.92f, 0.20f, 0.94f)
                                                        : juce::Colour::fromFloatRGBA (1.0f, 0.92f, 0.20f, 0.34f)));
                    g.setFont (juce::FontOptions (hasActiveCell ? 9.5f : 8.5f, juce::Font::bold));
                    g.drawFittedText (pitchText, noteBounds.toNearestInt(), juce::Justification::centred, 1);
                }
            }

            for (int i = 0; i < activeHarmonyCells.size(); ++i)
            {
                for (int j = i + 1; j < activeHarmonyCells.size(); ++j)
                {
                    const auto& a = activeHarmonyCells.getReference (i);
                    const auto& b = activeHarmonyCells.getReference (j);
                    const auto dCol = std::abs (a.col - b.col);
                    const auto dRow = std::abs (a.row - b.row);

                    if ((dCol == 1 && dRow == 0) || (dCol == 0 && dRow == 1) || (dCol == 1 && dRow == 1))
                    {
                        juce::Path link;
                        link.startNewSubPath (pointFor (a.row, a.col));
                        link.lineTo (pointFor (b.row, b.col));
                        auto linkColour = juce::Colour::fromFloatRGBA (0.14f, 0.94f, 0.88f, 0.28f);
                        g.setColour (linkColour.withAlpha (0.06f));
                        g.strokePath (link, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                        g.setColour (linkColour.withAlpha (0.24f));
                        g.strokePath (link, juce::PathStrokeType (0.9f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    }
                }
            }

            for (auto& snake : result.snakes)
            {
                if (snake.segments.isEmpty())
                    continue;

                juce::Path path;
                for (int i = 0; i < snake.segments.size(); ++i)
                {
                    const auto& segment = snake.segments.getReference (i);
                    if (segment.col < colStart || segment.col >= colStart + visibleCols
                        || segment.row < rowStart || segment.row >= rowStart + visibleRows)
                        continue;

                    auto p = pointFor (segment.row, segment.col);
                    p.y -= (segment.layer - result.activeLayer) * 4.0f;

                    if (path.isEmpty())
                        path.startNewSubPath (p);
                    else
                        path.lineTo (p);
                }

                if (path.isEmpty())
                    continue;

                const auto alpha = snake.isGhost ? 0.10f : 0.40f;
                g.setColour (snake.colour.withAlpha (alpha * 0.10f));
                g.strokePath (path, juce::PathStrokeType (3.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (snake.colour.withAlpha (alpha));
                g.strokePath (path, juce::PathStrokeType (1.1f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            g.restoreState();

            const auto layerGap = noteArea.getHeight() / (float) juce::jmax (layers, 1);
            const auto chipHeight = juce::jlimit (20.0f, 26.0f, layerGap - 6.0f);
            for (int layer = 0; layer < layers; ++layer)
            {
                const auto isActiveLayer = layer == result.activeLayer;
                const auto layerColour = harmonySpaceLayerColour (layer);
                auto chip = juce::Rectangle<float> (sideLeft.getX() + 8.0f,
                                                    noteArea.getY() + layerGap * layer + (layerGap - chipHeight) * 0.5f,
                                                    sideLeft.getWidth() - 16.0f,
                                                    chipHeight);
                g.setColour (isActiveLayer
                                 ? juce::Colour::fromFloatRGBA (0.08f, 0.10f, 0.03f, 0.96f).interpolatedWith (layerColour.withAlpha (0.34f), 0.55f)
                                 : juce::Colour::fromFloatRGBA (0.06f, 0.08f, 0.03f, 0.76f));
                g.fillRoundedRectangle (chip, 8.0f);
                g.setColour (isActiveLayer ? layerColour.brighter (0.28f).withAlpha (0.96f)
                                           : layerColour.withAlpha (0.42f));
                g.drawRoundedRectangle (chip.reduced (0.5f), 8.0f, isActiveLayer ? 1.4f : 0.9f);
                auto chipTextBounds = chip.toNearestInt();
                g.setColour (juce::Colours::white.withAlpha (0.98f));
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
                g.drawFittedText (harmonySpaceLayerShortName (layer), chipTextBounds.removeFromTop (chipTextBounds.getHeight() / 2 + 2), juce::Justification::centred, 1);
                g.setColour (layerColour.withAlpha (isActiveLayer ? 0.94f : 0.72f));
                g.setFont (juce::FontOptions (8.5f, juce::Font::plain));
                g.drawFittedText ("L" + juce::String (layer + 1), chipTextBounds, juce::Justification::centred, 1);
            }

            for (int key = 0; key < 12; ++key)
            {
                auto whiteKey = juce::Rectangle<float> (keyboardBounds.getX() + key * (keyboardBounds.getWidth() / 12.0f),
                                                        keyboardBounds.getY(),
                                                        keyboardBounds.getWidth() / 12.0f - 2.0f,
                                                        keyboardBounds.getHeight());
                const auto active = key == result.harmonySpaceKeyCenter;
                g.setColour (active ? juce::Colour::fromFloatRGBA (0.96f, 0.84f, 0.18f, 0.96f)
                                    : juce::Colour::fromFloatRGBA (0.92f, 0.92f, 0.88f, 0.90f));
                g.fillRoundedRectangle (whiteKey, 5.0f);
                g.setColour (juce::Colour::fromFloatRGBA (0.08f, 0.08f, 0.02f, 0.76f));
                g.drawRoundedRectangle (whiteKey.reduced (0.5f), 5.0f, 1.0f);
                g.setColour (active ? juce::Colours::black : juce::Colour::fromFloatRGBA (0.10f, 0.10f, 0.02f, 0.88f));
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
                g.drawFittedText (noteNameFromPitchClass (key), whiteKey.toNearestInt(), juce::Justification::centred, 1);
            }

            static const juce::String controlLabels[] = { "Key-", "Key+", "Lock", "Rec", "Key", "Chord", "Lead", "Clear" };
            auto controlsBounds = juce::Rectangle<float> (sideRight.getX() + 10.0f,
                                                          noteArea.getY() + 2.0f,
                                                          sideRight.getWidth() - 20.0f,
                                                          noteArea.getHeight() - 10.0f);
            const auto controlStep = controlsBounds.getHeight() / 9.6f;
            const auto controlHeight = juce::jlimit (24.0f, 29.0f, controlStep - 7.0f);
            for (int i = 0; i < 8; ++i)
            {
                auto button = juce::Rectangle<float> (controlsBounds.getX(),
                                                      controlsBounds.getY() + controlStep * i,
                                                      controlsBounds.getWidth(),
                                                      controlHeight);
                const auto activeConstraint = i == 2 && result.harmonySpaceConstraintMode > 0;
                const auto activeRecord = i == 3 && result.harmonySpaceGestureRecordEnabled;
                g.setColour (juce::Colour::fromFloatRGBA (0.06f, 0.10f, 0.08f, 0.78f));
                g.fillRoundedRectangle (button, 9.0f);
                g.setColour ((activeConstraint || activeRecord || i == 4 || i == 5 || i == 6)
                                 ? juce::Colour::fromFloatRGBA (0.12f, 0.88f, 0.82f, 0.90f)
                                 : juce::Colour::fromFloatRGBA (0.96f, 0.82f, 0.22f, 0.84f));
                g.drawRoundedRectangle (button.reduced (0.5f), 9.0f, 1.1f);
                g.setColour (juce::Colours::white.withAlpha (0.94f));
                g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
                g.drawFittedText (controlLabels[i], button.toNearestInt(), juce::Justification::centred, 1);
            }

            auto infoBounds = statusBounds.withTrimmedLeft ((int) sideRight.getX() - statusBounds.getX())
                                         .withTrimmedRight (4)
                                         .reduced (4, 0);
            g.setColour (juce::Colours::white.withAlpha (0.80f));
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.setColour (harmonySpaceLayerColour (result.activeLayer).withAlpha (0.96f));
            g.drawFittedText (harmonySpaceLayerLongName (result.activeLayer),
                              infoBounds.removeFromTop (16).toNearestInt(),
                              juce::Justification::centred,
                              1);
            g.setColour (juce::Colours::white.withAlpha (0.80f));
            g.drawFittedText ("Key " + noteNameFromPitchClass (result.harmonySpaceKeyCenter),
                              infoBounds.removeFromTop (16).toNearestInt(),
                              juce::Justification::centred,
                              1);
            static const juce::String constraintNames[] = { "Free", "Diatonic", "Triad" };
            g.drawFittedText ("Lock " + constraintNames[result.harmonySpaceConstraintMode],
                              infoBounds.removeFromTop (16).toNearestInt(),
                              juce::Justification::centred,
                              1);
            g.drawFittedText (result.harmonySpaceGestureRecordEnabled ? "Rec On" : "Rec Off",
                              infoBounds.removeFromTop (16).toNearestInt(),
                              juce::Justification::centred,
                              1);
            g.setColour (juce::Colours::white.withAlpha (0.56f));
            g.setFont (juce::FontOptions (9.0f, juce::Font::italic));
            g.drawFittedText ("Dedicated to Simon Holland",
                              infoBounds.removeFromTop (18).toNearestInt(),
                              juce::Justification::centred,
                              1);

            return;
        }

        auto stageBounds = bounds.reduced (20.0f, 22.0f);
        g.setColour (isHarmonicMode
                         ? juce::Colour::fromFloatRGBA (0.07f, 0.04f, 0.12f, 0.78f)
                         : (isCellularMode ? juce::Colour::fromFloatRGBA (0.03f, 0.04f, 0.12f, 0.80f)
                         : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.03f, 0.03f, 0.01f, 0.82f)
                                              : juce::Colour::fromFloatRGBA (0.01f, 0.04f, 0.02f, 0.74f))));
        g.fillRoundedRectangle (stageBounds, 16.0f);
        juce::ColourGradient stageBloom (isHarmonicMode ? juce::Colour::fromFloatRGBA (0.90f, 0.70f, 0.28f, 0.16f)
                                                        : (isCellularMode ? juce::Colour::fromFloatRGBA (0.70f, 0.78f, 1.0f, 0.16f)
                                                        : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (1.0f, 0.80f, 0.18f, 0.16f)
                                                                              : juce::Colour::fromFloatRGBA (0.65f, 1.0f, 0.05f, 0.14f))),
                                         stageBounds.getX(), stageBounds.getY(),
                                         isHarmonicMode ? juce::Colour::fromFloatRGBA (0.34f, 0.56f, 1.0f, 0.15f)
                                                        : (isCellularMode ? juce::Colour::fromFloatRGBA (0.18f, 1.0f, 0.90f, 0.14f)
                                                        : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.10f, 0.92f, 0.88f, 0.14f)
                                                                              : juce::Colour::fromFloatRGBA (0.06f, 0.95f, 1.0f, 0.13f))),
                                         stageBounds.getRight(), stageBounds.getBottom(),
                                         false);
        g.setGradientFill (stageBloom);
        g.fillRoundedRectangle (stageBounds.reduced (1.0f), 16.0f);

        if (isCellularMode)
        {
            g.setColour (juce::Colours::white.withAlpha (0.84f));
            g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
            g.drawFittedText ("CA " + cellularRuleName ((MainComponent::CellularRule) result.cellularRule),
                              stageBounds.removeFromTop (18).removeFromLeft (120).toNearestInt(),
                              juce::Justification::centredLeft,
                              1);
        }

        if (result.pluginMode)
        {
            auto meterArea = juce::Rectangle<float> (stageBounds.getRight() - 28.0f, stageBounds.getY() + 28.0f, 12.0f, stageBounds.getHeight() - 56.0f);
            const std::array<juce::Colour, 3> meterColours {
                juce::Colour::fromFloatRGBA (0.98f, 0.86f, 0.22f, 1.0f),
                juce::Colour::fromFloatRGBA (0.14f, 0.96f, 1.0f, 1.0f),
                juce::Colour::fromFloatRGBA (1.0f, 0.30f, 0.86f, 1.0f)
            };

            for (int i = 0; i < 3; ++i)
            {
                auto bar = meterArea.removeFromLeft (3.0f);
                meterArea.removeFromLeft (2.0f);
                g.setColour (juce::Colours::white.withAlpha (0.06f));
                g.fillRoundedRectangle (bar, 2.0f);
                const auto filled = bar.withY (bar.getBottom() - bar.getHeight() * juce::jlimit (0.0f, 1.0f, result.mixerLevels[(size_t) i]));
                g.setColour (meterColours[(size_t) i].withAlpha (0.82f));
                g.fillRoundedRectangle (filled, 2.0f);
            }
        }

        const auto isoX = isHarmonicMode
                              ? juce::Point<float> (stageBounds.getWidth() * 0.028f, -stageBounds.getHeight() * 0.006f)
                              : (isHarmonySpaceMode ? juce::Point<float> (stageBounds.getWidth() * 0.026f, -stageBounds.getHeight() * 0.022f)
                                                    : juce::Point<float> (stageBounds.getWidth() * 0.032f, -stageBounds.getHeight() * 0.016f));
        const auto isoY = isHarmonicMode
                              ? juce::Point<float> (-stageBounds.getWidth() * 0.018f, -stageBounds.getHeight() * 0.030f)
                              : (isHarmonySpaceMode ? juce::Point<float> (-stageBounds.getWidth() * 0.026f, -stageBounds.getHeight() * 0.022f)
                                                    : juce::Point<float> (-stageBounds.getWidth() * 0.032f, -stageBounds.getHeight() * 0.016f));
        const auto isoZ = isHarmonicMode
                              ? juce::Point<float> (stageBounds.getWidth() * 0.010f, -stageBounds.getHeight() * 0.058f)
                              : (isHarmonySpaceMode ? juce::Point<float> (0.0f, -stageBounds.getHeight() * 0.048f)
                                                    : juce::Point<float> (0.0f, -stageBounds.getHeight() * 0.07f));
        const auto projectPointUncentred = [&] (float originX, float originY, int layer, float colValue, float rowValue) -> juce::Point<float>
        {
            const auto harmonicCol = isHarmonicMode ? (colValue + rowValue * 0.5f)
                                                    : (isHarmonySpaceMode ? (colValue * 0.82f + rowValue * 0.50f)
                                                                          : colValue);
            const auto harmonicRow = isHarmonicMode ? (rowValue * 0.866f)
                                                    : (isHarmonySpaceMode ? (rowValue * 0.82f - colValue * 0.12f)
                                                                          : rowValue);
            return { originX + isoX.x * harmonicCol + isoY.x * harmonicRow + isoZ.x * layer,
                     originY - (isoX.y * harmonicCol + isoY.y * harmonicRow + isoZ.y * layer) };
        };

        auto minX = std::numeric_limits<float>::max();
        auto minY = std::numeric_limits<float>::max();
        auto maxX = std::numeric_limits<float>::lowest();
        auto maxY = std::numeric_limits<float>::lowest();

        for (auto layer : { 0.0f, (float) layers })
            for (auto col : { 0.0f, (float) cols })
                for (auto row : { 0.0f, (float) rows })
                {
                    auto point = projectPointUncentred (0.0f, 0.0f, (int) layer, col, row);
                    minX = juce::jmin (minX, point.x);
                    minY = juce::jmin (minY, point.y);
                    maxX = juce::jmax (maxX, point.x);
                    maxY = juce::jmax (maxY, point.y);
                }

        const auto projectedWidth = maxX - minX;
        const auto projectedHeight = maxY - minY;
        const auto originX = stageBounds.getCentreX() - (minX + projectedWidth * 0.5f);
        const auto originY = stageBounds.getCentreY() - (minY + projectedHeight * 0.5f);
        const auto projectPoint = [&] (int layer, float colValue, float rowValue) -> juce::Point<float>
        {
            return projectPointUncentred (originX, originY, layer, colValue, rowValue);
        };

        juce::Path axisX;
        axisX.startNewSubPath (projectPoint (0, 0.0f, 0.0f));
        axisX.lineTo (projectPoint (0, (float) cols + 1.0f, 0.0f));
        g.setColour ((isHarmonicMode ? juce::Colour (0xff7aa7ff) : (isHarmonySpaceMode ? juce::Colour (0xff17e4d4) : juce::Colours::cyan)).withAlpha (0.20f));
        g.strokePath (axisX, juce::PathStrokeType (4.8f));
        g.setColour ((isHarmonicMode ? juce::Colour (0xff7aa7ff) : (isHarmonySpaceMode ? juce::Colour (0xff17e4d4) : juce::Colours::cyan)).withAlpha (0.76f));
        g.strokePath (axisX, juce::PathStrokeType (1.6f));

        juce::Path axisY;
        axisY.startNewSubPath (projectPoint (0, 0.0f, 0.0f));
        axisY.lineTo (projectPoint (0, 0.0f, (float) rows + 1.0f));
        g.setColour ((isHarmonicMode ? juce::Colour (0xffff8f6b) : (isHarmonySpaceMode ? juce::Colour (0xffffb423) : juce::Colours::magenta)).withAlpha (0.18f));
        g.strokePath (axisY, juce::PathStrokeType (4.4f));
        g.setColour ((isHarmonicMode ? juce::Colour (0xffff8f6b) : (isHarmonySpaceMode ? juce::Colour (0xffffb423) : juce::Colours::magenta)).withAlpha (0.72f));
        g.strokePath (axisY, juce::PathStrokeType (1.5f));

        juce::Path axisZ;
        axisZ.startNewSubPath (projectPoint (0, 0.0f, 0.0f));
        axisZ.lineTo (projectPoint (layers, 0.0f, 0.0f));
        g.setColour ((isHarmonicMode ? juce::Colour (0xffffefc1) : (isHarmonySpaceMode ? juce::Colour (0xfffff0c1) : juce::Colours::yellow)).withAlpha (0.16f));
        g.strokePath (axisZ, juce::PathStrokeType (4.2f));
        g.setColour ((isHarmonicMode ? juce::Colour (0xffffefc1) : (isHarmonySpaceMode ? juce::Colour (0xfffff0c1) : juce::Colours::yellow)).withAlpha (0.64f));
        g.strokePath (axisZ, juce::PathStrokeType (1.4f));

        for (int layer = layers - 1; layer >= 0; --layer)
        {
            juce::Path plane;
            if (isHarmonicMode)
            {
                plane.startNewSubPath (projectPoint (layer, 0.0f, 0.0f));
                plane.lineTo (projectPoint (layer, (float) cols, 0.0f));
                plane.lineTo (projectPoint (layer, (float) cols * 0.5f, (float) rows));
                plane.closeSubPath();
            }
            else if (isHarmonySpaceMode)
            {
                plane.startNewSubPath (projectPoint (layer, cols * 0.5f, 0.0f));
                plane.lineTo (projectPoint (layer, (float) cols, rows * 0.5f));
                plane.lineTo (projectPoint (layer, cols * 0.5f, (float) rows));
                plane.lineTo (projectPoint (layer, 0.0f, rows * 0.5f));
                plane.closeSubPath();
            }
            else
            {
                plane.startNewSubPath (projectPoint (layer, 0.0f, 0.0f));
                plane.lineTo (projectPoint (layer, (float) cols, 0.0f));
                plane.lineTo (projectPoint (layer, (float) cols, (float) rows));
                plane.lineTo (projectPoint (layer, 0.0f, (float) rows));
                plane.closeSubPath();
            }
            auto layerShade = isHarmonicMode
                                  ? juce::Colour::fromHSV (std::fmod (0.62f + layer * 0.05f, 1.0f), 0.38f, 0.42f, 0.16f)
                                  : (isHarmonySpaceMode ? juce::Colour::fromHSV (std::fmod (0.10f + layer * 0.045f, 1.0f), 0.50f, 0.56f, 0.14f)
                                                        : juce::Colour::fromHSV (std::fmod (result.hue + layer * 0.08f, 1.0f), 0.82f, 0.30f, 0.18f));
            g.setColour (layerShade);
            g.fillPath (plane);
            g.setColour (juce::Colours::white.withAlpha (0.08f + (1.0f - (float) layer / (float) juce::jmax (1, layers - 1)) * 0.03f));
            g.strokePath (plane, juce::PathStrokeType (1.0f));

            g.setColour (juce::Colours::white.withAlpha (isHarmonicMode ? 0.035f : (isHarmonySpaceMode ? 0.05f : 0.02f)));
            for (int col = 1; col < cols; ++col)
            {
                juce::Path colLine;
                colLine.startNewSubPath (projectPoint (layer, isHarmonySpaceMode ? (float) col * 0.5f : (float) col, 0.0f));
                colLine.lineTo (isHarmonicMode
                                    ? projectPoint (layer, (float) col * 0.5f, (float) rows)
                                    : (isHarmonySpaceMode ? projectPoint (layer, cols * 0.5f + (float) col * 0.5f, rows * 0.5f)
                                                          : projectPoint (layer, (float) col, (float) rows)));
                g.strokePath (colLine, juce::PathStrokeType (1.0f));
            }

            for (int row = 1; row < rows; ++row)
            {
                juce::Path rowLine;
                rowLine.startNewSubPath (projectPoint (layer, isHarmonySpaceMode ? 0.0f : row * 0.5f, (float) row));
                rowLine.lineTo (projectPoint (layer, isHarmonySpaceMode ? (float) cols : (float) cols - row * 0.5f, (float) row));
                g.strokePath (rowLine, juce::PathStrokeType (1.0f));
            }

            if (isHarmonicMode)
            {
                for (int diag = 1; diag < cols; diag += 2)
                {
                    juce::Path diagLine;
                    diagLine.startNewSubPath (projectPoint (layer, (float) diag, 0.0f));
                    diagLine.lineTo (projectPoint (layer, juce::jmax (0.0f, (float) diag - rows * 0.5f), (float) rows));
                    g.strokePath (diagLine, juce::PathStrokeType (0.8f));
                }
            }
            else if (isHarmonySpaceMode)
            {
                g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.84f, 0.26f, 0.10f));
                for (int diag = 0; diag <= cols; diag += 2)
                {
                    juce::Path diagLine;
                    diagLine.startNewSubPath (projectPoint (layer, (float) diag, 0.0f));
                    diagLine.lineTo (projectPoint (layer, juce::jmax (0.0f, (float) diag - rows), (float) rows));
                    g.strokePath (diagLine, juce::PathStrokeType (0.9f));
                }
                g.setColour (juce::Colour::fromFloatRGBA (0.12f, 0.92f, 0.88f, 0.10f));
                for (int diag = 0; diag <= cols; diag += 2)
                {
                    juce::Path diagLine;
                    diagLine.startNewSubPath (projectPoint (layer, (float) diag, (float) rows));
                    diagLine.lineTo (projectPoint (layer, juce::jmax (0.0f, (float) diag - rows), 0.0f));
                    g.strokePath (diagLine, juce::PathStrokeType (0.9f));
                }
            }

            auto labelAnchor = projectPoint (layer, -0.45f, (float) rows + 0.25f);
            auto labelBounds = juce::Rectangle<float> (labelAnchor.x - 16.0f, labelAnchor.y - 10.0f, 32.0f, 18.0f);
            const auto isActiveLayer = (layer == result.activeLayer);
            g.setColour (isActiveLayer
                             ? juce::Colour::fromFloatRGBA (0.18f, 0.32f, 0.02f, 0.96f)
                             : juce::Colour::fromFloatRGBA (0.02f, 0.05f, 0.02f, 0.86f));
            g.fillRoundedRectangle (labelBounds, 7.0f);
            g.setColour (isActiveLayer
                             ? juce::Colour::fromFloatRGBA (0.75f, 1.0f, 0.18f, 0.98f)
                             : juce::Colours::white.withAlpha (0.72f));
            g.drawRoundedRectangle (labelBounds.reduced (0.5f), 7.0f, isActiveLayer ? 1.5f : 0.9f);
            if (isActiveLayer)
            {
                g.setColour (juce::Colour::fromFloatRGBA (0.75f, 1.0f, 0.18f, 0.18f));
                g.drawRoundedRectangle (labelBounds.expanded (2.5f), 9.5f, 2.2f);
            }
            g.setColour (isActiveLayer
                             ? juce::Colour::fromFloatRGBA (0.96f, 1.0f, 0.88f, 1.0f)
                             : juce::Colours::white.withAlpha (0.92f));
            g.setFont (juce::FontOptions (isActiveLayer ? 11.5f : 10.5f, juce::Font::bold));
            g.drawFittedText ("L" + juce::String (layer + 1),
                              labelBounds.toNearestInt(),
                              juce::Justification::centred,
                              1);
        }

        if (result.mode == MainComponent::AppMode::harmonicGeometry || result.mode == MainComponent::AppMode::harmonySpace)
        {
            for (int layer = 0; layer < layers; ++layer)
            {
                juce::Array<juce::Point<float>> harmonicPoints;

                for (auto& gridCell : result.gridCells)
                {
                    const auto isHarmonicCell = gridCell.type == GlyphType::tone
                                             || gridCell.type == GlyphType::voice
                                             || gridCell.type == GlyphType::chord
                                             || gridCell.type == GlyphType::key
                                             || gridCell.type == GlyphType::octave;

                    if (! isHarmonicCell || gridCell.layer != layer)
                        continue;

                    harmonicPoints.add (projectPoint (gridCell.layer,
                                                      (float) gridCell.col + 0.5f,
                                                      (float) gridCell.row + 0.5f));
                }

                if (harmonicPoints.size() < 2)
                    continue;

                auto centroid = juce::Point<float>();
                for (auto& point : harmonicPoints)
                    centroid += point;
                centroid /= (float) harmonicPoints.size();

                if (result.mode == MainComponent::AppMode::harmonicGeometry)
                {
                    juce::Path simplex;
                    for (int i = 0; i < harmonicPoints.size(); ++i)
                    {
                        const auto& point = harmonicPoints.getReference (i);
                        if (i == 0)
                            simplex.startNewSubPath (point);
                        else
                            simplex.lineTo (point);
                    }
                    simplex.closeSubPath();

                    auto fillHue = std::fmod (0.58f + layer * 0.06f, 1.0f);
                    auto simplexFill = juce::Colour::fromHSV (fillHue, 0.22f, 0.98f, 0.08f);
                    auto simplexEdge = juce::Colour::fromHSV (fillHue, 0.35f, 1.0f, 0.22f);
                    g.setColour (simplexFill);
                    g.fillPath (simplex);
                    g.setColour (simplexEdge);
                    g.strokePath (simplex, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                    for (int i = 1; i < harmonicPoints.size(); ++i)
                    {
                        juce::Path voiceLeading;
                        voiceLeading.startNewSubPath (harmonicPoints.getReference (i - 1));
                        voiceLeading.lineTo (harmonicPoints.getReference (i));
                        auto linkHue = std::fmod (0.10f + layer * 0.07f, 1.0f);
                        auto linkColour = juce::Colour::fromHSV (linkHue, 0.42f, 0.96f, 0.18f);
                        g.setColour (linkColour);
                        g.strokePath (voiceLeading, juce::PathStrokeType (1.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                    }

                    for (auto& point : harmonicPoints)
                    {
                        juce::Path spoke;
                        spoke.startNewSubPath (centroid);
                        spoke.lineTo (point);
                        g.setColour (juce::Colour::fromFloatRGBA (0.96f, 0.92f, 0.84f, 0.12f));
                        g.strokePath (spoke, juce::PathStrokeType (0.9f));
                    }

                    g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.96f, 0.88f, 0.26f));
                    g.fillEllipse (centroid.x - 4.5f, centroid.y - 4.5f, 9.0f, 9.0f);
                }
                else
                {
                    for (int i = 0; i < harmonicPoints.size(); ++i)
                    {
                        for (int j = i + 1; j < harmonicPoints.size(); ++j)
                        {
                            auto a = harmonicPoints.getReference (i);
                            auto b = harmonicPoints.getReference (j);
                            const auto dx = std::abs (a.x - b.x);
                            const auto dy = std::abs (a.y - b.y);

                            if (dx > stageBounds.getWidth() * 0.18f || dy > stageBounds.getHeight() * 0.18f)
                                continue;

                            juce::Path lattice;
                            lattice.startNewSubPath (a);
                            lattice.lineTo (b);
                            auto link = juce::Colour::fromHSV (std::fmod (0.10f + layer * 0.04f + i * 0.02f, 1.0f), 0.46f, 1.0f, 0.20f);
                            g.setColour (link.withAlpha (0.14f));
                            g.strokePath (lattice, juce::PathStrokeType (3.8f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                            g.setColour (link.withAlpha (0.42f));
                            g.strokePath (lattice, juce::PathStrokeType (1.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                        }
                    }

                    juce::Path centroidRing;
                    centroidRing.addEllipse (centroid.x - 8.0f, centroid.y - 8.0f, 16.0f, 16.0f);
                    g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.90f, 0.34f, 0.16f));
                    g.fillPath (centroidRing);
                    g.setColour (juce::Colour::fromFloatRGBA (0.10f, 0.92f, 0.88f, 0.48f));
                    g.strokePath (centroidRing, juce::PathStrokeType (1.2f));
                }
            }
        }

        for (auto& gridCell : result.gridCells)
        {
            const auto point = projectPoint (gridCell.layer,
                                             (float) gridCell.col + 0.5f,
                                             (float) gridCell.row + 0.5f);
            const auto def = MainComponent::getGlyphDefinition (gridCell.type);
            const auto isHarmonicCell = gridCell.type == GlyphType::tone
                                     || gridCell.type == GlyphType::voice
                                     || gridCell.type == GlyphType::chord
                                     || gridCell.type == GlyphType::key
                                     || gridCell.type == GlyphType::octave;
            const auto alpha = gridCell.isTriggered ? 0.78f : ((isHarmonicMode || isHarmonySpaceMode) && ! isHarmonicCell ? 0.12f : 0.24f);
            const auto radius = (isHarmonicMode || isHarmonySpaceMode) && isHarmonicCell
                                    ? (gridCell.isTriggered ? 8.2f : 5.6f)
                                    : (gridCell.isTriggered ? 6.8f : 3.8f);

            if (gridCell.type == GlyphType::wormhole)
            {
                const auto ringRadius = radius * 1.9f;
                g.setColour (def.colour.withAlpha (0.16f + alpha * 0.24f));
                g.fillEllipse (point.x - ringRadius * 1.8f, point.y - ringRadius * 1.8f, ringRadius * 3.6f, ringRadius * 3.6f);
                g.setColour (def.colour.withAlpha (0.92f));
                g.drawEllipse (point.x - ringRadius, point.y - ringRadius, ringRadius * 2.0f, ringRadius * 2.0f, 1.8f);
                g.drawEllipse (point.x - ringRadius * 0.62f, point.y - ringRadius * 0.62f, ringRadius * 1.24f, ringRadius * 1.24f, 1.2f);
                g.setColour (juce::Colours::white.withAlpha (gridCell.isTriggered ? 0.92f : 0.42f));
                g.fillEllipse (point.x - 1.7f, point.y - 1.7f, 3.4f, 3.4f);
            }
            else
            {
                if ((isHarmonicMode || isHarmonySpaceMode) && isHarmonicCell)
                {
                    auto chordColour = isHarmonySpaceMode
                                           ? juce::Colour::fromHSV (std::fmod (0.08f + gridCell.col * 0.02f + gridCell.row * 0.015f, 1.0f), 0.55f, 1.0f, alpha)
                                           : juce::Colour::fromHSV (std::fmod (0.62f + gridCell.layer * 0.05f + gridCell.col * 0.01f, 1.0f), 0.28f, 1.0f, alpha);
                    g.setColour (chordColour.withAlpha (alpha * 0.22f));
                    g.fillEllipse (point.x - radius * 2.4f, point.y - radius * 2.4f, radius * 4.8f, radius * 4.8f);
                    g.setColour (chordColour.withAlpha (alpha));
                    g.fillEllipse (point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f);
                    g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.97f, 0.90f, gridCell.isTriggered ? 0.92f : 0.62f));
                    g.drawEllipse (point.x - radius * 1.45f, point.y - radius * 1.45f, radius * 2.9f, radius * 2.9f, gridCell.isTriggered ? 1.6f : 1.1f);
                    g.fillEllipse (point.x - 1.8f, point.y - 1.8f, 3.6f, 3.6f);
                }
                else
                {
                    g.setColour (def.colour.withAlpha (alpha * 0.36f));
                    g.fillEllipse (point.x - radius * 1.7f, point.y - radius * 1.7f, radius * 3.4f, radius * 3.4f);
                    g.setColour (def.colour.withAlpha (alpha));
                    g.fillEllipse (point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f);
                    g.setColour (juce::Colours::white.withAlpha (gridCell.isTriggered ? 0.78f : 0.24f));
                    g.drawEllipse (point.x - radius, point.y - radius, radius * 2.0f, radius * 2.0f, gridCell.isTriggered ? 1.2f : 0.8f);
                }
            }
        }

        for (auto& line : result.lines)
        {
            auto hue = std::fmod (line.hue + line.row * 0.08f, 1.0f);
            auto colour = juce::Colour::fromHSV (hue, 0.95f, 0.98f, 0.28f + line.energy * 0.58f);
            auto start = projectPoint (line.layer, 0.0f, (float) line.row + 0.5f);
            auto mid = projectPoint (line.layer, cols * 0.5f, (float) line.row + 0.5f + std::sin (line.row + result.energy * 2.0f) * 0.26f);
            auto end = projectPoint (line.layer, (float) cols, (float) line.row + 0.5f);

            juce::Path path;
            path.startNewSubPath (start);
            path.quadraticTo (mid, end);

            g.setColour (colour.withAlpha (0.20f));
            g.strokePath (path, juce::PathStrokeType (6.8f + line.energy * 10.0f));
            g.setColour (colour);
            g.strokePath (path, juce::PathStrokeType (2.4f + line.energy * 7.5f));

            auto marker = projectPoint (line.layer, (float) line.step + 0.5f, (float) line.row + 0.5f);
            g.setColour (colour.withAlpha (0.34f));
            g.fillEllipse (marker.x - (10.0f + line.energy * 18.0f),
                           marker.y - (10.0f + line.energy * 18.0f),
                           20.0f + line.energy * 36.0f,
                           20.0f + line.energy * 36.0f);
        }

        const auto drawProjectedSnake = [&] (const Snake& snake, float alphaScale, bool drawDirection)
        {
            if (snake.segments.isEmpty())
                return;

            juce::Array<juce::Point<float>> centres;
            const auto morph = smoothStep (result.transportPhase);

            for (int i = 0; i < snake.segments.size(); ++i)
            {
                auto& segment = snake.segments.getReference (i);
                const auto previousIndex = juce::jmin (i, snake.previousSegments.size() - 1);
                auto previousSegment = previousIndex >= 0 ? snake.previousSegments.getReference (previousIndex) : segment;
                auto previousCentre = projectPoint (previousSegment.layer, (float) previousSegment.col + 0.5f, (float) previousSegment.row + 0.5f);
                auto currentCentre = projectPoint (segment.layer, (float) segment.col + 0.5f, (float) segment.row + 0.5f);
                centres.add ({ juce::jmap (morph, previousCentre.x, currentCentre.x),
                               juce::jmap (morph, previousCentre.y, currentCentre.y) });
            }

            const auto previousHead = snake.previousSegments.isEmpty() ? snake.segments.getFirst() : snake.previousSegments.getFirst();
            const auto headJump = std::abs (snake.segments.getFirst().col - previousHead.col)
                                + std::abs (snake.segments.getFirst().row - previousHead.row)
                                + std::abs (snake.segments.getFirst().layer - previousHead.layer);
            if (headJump > 2)
            {
                auto from = projectPoint (previousHead.layer, (float) previousHead.col + 0.5f, (float) previousHead.row + 0.5f);
                auto to = centres.getFirst();
                juce::Path teleportArc;
                teleportArc.startNewSubPath (from);
                teleportArc.quadraticTo ((from.x + to.x) * 0.5f, juce::jmin (from.y, to.y) - 26.0f, to.x, to.y);
                g.setColour (snake.colour.withAlpha (0.18f * alphaScale));
                g.strokePath (teleportArc, juce::PathStrokeType (8.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (juce::Colours::white.withAlpha (0.72f * alphaScale));
                g.strokePath (teleportArc, juce::PathStrokeType (2.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            juce::Path spinePath;
            spinePath.startNewSubPath (centres.getFirst());

            for (int i = 1; i < centres.size(); ++i)
            {
                const auto previous = centres.getReference (juce::jmax (0, i - 1));
                const auto current = centres.getReference (i);
                const auto next = centres.getReference (juce::jmin (centres.size() - 1, i + 1));
                const auto control = juce::Point<float> ((previous.x + current.x) * 0.5f,
                                                         (previous.y + current.y) * 0.5f);
                const auto midpoint = juce::Point<float> ((current.x + next.x) * 0.5f,
                                                          (current.y + next.y) * 0.5f);
                spinePath.quadraticTo (control, midpoint);
            }

            const auto baseAlpha = (snake.isGhost ? 0.1f : 0.2f) * alphaScale;
            const auto coreAlpha = (snake.isGhost ? 0.45f : 0.9f) * alphaScale;
            g.setColour (snake.colour.withAlpha (baseAlpha * 1.9f));
            g.strokePath (spinePath, juce::PathStrokeType (juce::jmin (cellWidth, rowHeight) * (snake.isGhost ? 0.48f : 0.74f),
                                                           juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));
            g.setColour (snake.colour.withBrightness (1.0f).withAlpha (coreAlpha));
            g.strokePath (spinePath, juce::PathStrokeType (juce::jmin (cellWidth, rowHeight) * (snake.isGhost ? 0.08f : 0.14f),
                                                           juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));

            auto headCentre = centres.getFirst();
            if (snake.lengthPulse > 0.0f)
            {
                const auto tailCentre = centres.getLast();
                const auto accent = snake.lengthDelta > 0 ? juce::Colour (0xff8cff9b) : juce::Colour (0xffffb36b);
                const auto haloRadius = juce::jmin (cellWidth, rowHeight) * (0.42f + snake.lengthPulse * 0.5f);
                g.setColour (accent.withAlpha ((0.18f + snake.lengthPulse * 0.22f) * alphaScale));
                g.fillEllipse (headCentre.x - haloRadius, headCentre.y - haloRadius, haloRadius * 2.0f, haloRadius * 2.0f);
                g.setColour (accent.withAlpha (0.82f * alphaScale));
                g.drawEllipse (headCentre.x - haloRadius, headCentre.y - haloRadius, haloRadius * 2.0f, haloRadius * 2.0f, 1.4f);
                g.setColour (accent.withAlpha ((0.14f + snake.lengthPulse * 0.18f) * alphaScale));
                g.fillEllipse (tailCentre.x - haloRadius * 0.7f, tailCentre.y - haloRadius * 0.7f, haloRadius * 1.4f, haloRadius * 1.4f);
            }
            g.setColour (juce::Colours::white.withAlpha ((snake.isGhost ? 0.58f : 0.95f) * alphaScale));
            g.fillEllipse (headCentre.x - (snake.isGhost ? 3.0f : 4.0f), headCentre.y - (snake.isGhost ? 3.0f : 4.0f),
                           snake.isGhost ? 6.0f : 8.0f, snake.isGhost ? 6.0f : 8.0f);
        };

        for (auto& frame : trailFrames)
            for (auto& trailSnake : frame.snakes)
                drawProjectedSnake (trailSnake, frame.alpha, false);

        for (auto& snake : result.snakes)
            drawProjectedSnake (snake, 1.0f, true);

        return;
    }

    auto flatBounds = bounds.reduced (16.0f, 18.0f);
    flatBounds.removeFromTop (6.0f);
    flatBounds = flatBounds.withTrimmedBottom (10.0f);
    flatBounds = flatBounds.translated (0.0f, -2.0f);
    const auto flatCellWidth = flatBounds.getWidth() / static_cast<float> (cols);
    const auto flatRowHeight = flatBounds.getHeight() / static_cast<float> (rows);

    g.setColour (juce::Colour::fromFloatRGBA (0.02f, 0.03f, 0.05f, 0.38f));
    g.fillRoundedRectangle (flatBounds, 16.0f);

    for (int col = 0; col < cols; ++col)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.drawVerticalLine (juce::roundToInt (flatBounds.getX() + col * flatCellWidth), flatBounds.getY(), flatBounds.getBottom());
    }

    for (int row = 0; row <= rows; ++row)
    {
        g.setColour (juce::Colours::white.withAlpha (0.045f));
        g.drawHorizontalLine (juce::roundToInt (flatBounds.getY() + row * flatRowHeight), flatBounds.getX(), flatBounds.getRight());
    }

    g.setColour (juce::Colours::white.withAlpha (0.82f));
    g.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    g.drawText ("FLAT", flatBounds.removeFromTop (22).removeFromRight (52).toNearestInt(), juce::Justification::centredRight, false);

    {
        juce::Graphics::ScopedSaveState clipState (g);
        g.reduceClipRegion (flatBounds.toNearestInt());

        for (auto& gridCell : result.gridCells)
        {
            const auto def = MainComponent::getGlyphDefinition (gridCell.type);
            auto cellBounds = juce::Rectangle<float> (flatBounds.getX() + gridCell.col * flatCellWidth,
                                                      flatBounds.getY() + gridCell.row * flatRowHeight,
                                                       flatCellWidth,
                                                      flatRowHeight).reduced (flatCellWidth * 0.18f, flatRowHeight * 0.18f);
            if (gridCell.type == GlyphType::wormhole)
            {
                auto portal = cellBounds.reduced (cellBounds.getWidth() * 0.14f, cellBounds.getHeight() * 0.14f);
                g.setColour (def.colour.withAlpha (gridCell.isTriggered ? 0.26f : 0.12f));
                g.fillEllipse (portal);
                g.setColour (def.colour.withAlpha (0.92f));
                g.drawEllipse (portal, gridCell.isTriggered ? 1.8f : 1.2f);
                g.drawEllipse (portal.reduced (portal.getWidth() * 0.18f, portal.getHeight() * 0.18f), 1.0f);
            }
            else
            {
                g.setColour (def.colour.withAlpha (gridCell.isTriggered ? 0.34f : 0.11f));
                g.fillRoundedRectangle (cellBounds, 8.0f);
                g.setColour (def.colour.withAlpha (gridCell.isTriggered ? 0.86f : 0.3f));
                g.drawRoundedRectangle (cellBounds.reduced (0.5f), 8.0f, gridCell.isTriggered ? 1.4f : 0.9f);
            }
        }

        for (auto& line : result.lines)
        {
            auto y = flatBounds.getY() + flatRowHeight * (static_cast<float> (line.row) + 0.5f);
            auto hue = std::fmod (line.hue + line.row * 0.08f, 1.0f);
            auto colour = juce::Colour::fromHSV (hue, 0.95f, 0.98f, 0.28f + line.energy * 0.58f);

            juce::Path path;
            path.startNewSubPath (flatBounds.getX(), y);
            path.quadraticTo (flatBounds.getCentreX(), y + std::sin (line.row + result.energy * 2.0f) * 10.0f, flatBounds.getRight(), y);

            g.setColour (colour);
            g.strokePath (path, juce::PathStrokeType (2.0f + line.energy * 8.0f));

            g.setColour (colour.withAlpha (0.3f));
            g.fillEllipse (flatBounds.getX() + flatCellWidth * (static_cast<float> (line.step) + 0.5f) - (10.0f + line.energy * 20.0f),
                           y - (10.0f + line.energy * 20.0f),
                           20.0f + line.energy * 40.0f,
                           20.0f + line.energy * 40.0f);
        }

        for (auto& snake : result.snakes)
        {
            if (snake.segments.isEmpty())
                continue;

            for (int i = snake.segments.size() - 1; i >= 0; --i)
            {
                const auto& segment = snake.segments.getReference (i);
                const auto depthMix = 1.0f - (float) i / (float) juce::jmax (1, snake.segments.size() - 1);
                auto segmentCell = juce::Rectangle<float> (flatBounds.getX() + segment.col * flatCellWidth,
                                                           flatBounds.getY() + segment.row * flatRowHeight,
                                                           flatCellWidth,
                                                           flatRowHeight).reduced (flatCellWidth * 0.09f, flatRowHeight * 0.09f);

                g.setColour (snake.colour.withAlpha ((snake.isGhost ? 0.04f : 0.1f) + depthMix * (snake.isGhost ? 0.04f : 0.12f)));
                g.fillRoundedRectangle (segmentCell, 10.0f);
                g.setColour (snake.colour.withAlpha ((snake.isGhost ? 0.14f : 0.28f) + depthMix * (snake.isGhost ? 0.08f : 0.22f)));
                g.drawRoundedRectangle (segmentCell.reduced (0.5f), 10.0f, snake.isGhost ? 0.8f : 1.2f);
            }

            juce::Array<juce::Point<float>> centres;
            const auto morph = smoothStep (result.transportPhase);

            for (int i = 0; i < snake.segments.size(); ++i)
            {
                auto& segment = snake.segments.getReference (i);
                const auto previousIndex = juce::jmin (i, snake.previousSegments.size() - 1);
                auto previousSegment = previousIndex >= 0 ? snake.previousSegments.getReference (previousIndex) : segment;
                auto previousCentre = juce::Point<float> (flatBounds.getX() + (previousSegment.col + 0.5f) * flatCellWidth,
                                                          flatBounds.getY() + (previousSegment.row + 0.5f) * flatRowHeight);
                auto currentCentre = juce::Point<float> (flatBounds.getX() + (segment.col + 0.5f) * flatCellWidth,
                                                         flatBounds.getY() + (segment.row + 0.5f) * flatRowHeight);
                auto centre = juce::Point<float> (juce::jmap (morph, previousCentre.x, currentCentre.x),
                                                  juce::jmap (morph, previousCentre.y, currentCentre.y));
                centres.add (centre);
            }

            const auto previousHead = snake.previousSegments.isEmpty() ? snake.segments.getFirst() : snake.previousSegments.getFirst();
            const auto headJump = std::abs (snake.segments.getFirst().col - previousHead.col)
                                + std::abs (snake.segments.getFirst().row - previousHead.row)
                                + std::abs (snake.segments.getFirst().layer - previousHead.layer);
            if (headJump > 2)
            {
                auto from = juce::Point<float> (flatBounds.getX() + (previousHead.col + 0.5f) * flatCellWidth,
                                                flatBounds.getY() + (previousHead.row + 0.5f) * flatRowHeight);
                auto to = centres.getFirst();
                juce::Path teleportArc;
                teleportArc.startNewSubPath (from);
                teleportArc.quadraticTo ((from.x + to.x) * 0.5f, juce::jmin (from.y, to.y) - flatRowHeight * 0.8f, to.x, to.y);
                g.setColour (snake.colour.withAlpha (0.18f));
                g.strokePath (teleportArc, juce::PathStrokeType (10.0f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
                g.setColour (juce::Colours::white.withAlpha (0.82f));
                g.strokePath (teleportArc, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
            }

            juce::Path spinePath;
            spinePath.startNewSubPath (centres.getFirst());

            if (centres.size() == 2)
            {
                spinePath.lineTo (centres.getLast());
            }
            else
            {
                for (int i = 1; i < centres.size(); ++i)
                {
                    const auto previous = centres.getReference (juce::jmax (0, i - 1));
                    const auto current = centres.getReference (i);
                    const auto next = centres.getReference (juce::jmin (centres.size() - 1, i + 1));
                    const auto control = juce::Point<float> ((previous.x + current.x) * 0.5f,
                                                             (previous.y + current.y) * 0.5f);
                    const auto midpoint = juce::Point<float> ((current.x + next.x) * 0.5f,
                                                              (current.y + next.y) * 0.5f);
                    spinePath.quadraticTo (control, midpoint);
                }

                spinePath.lineTo (centres.getLast());
            }

            const auto baseAlpha = snake.isGhost ? 0.08f : 0.16f;
            const auto coreAlpha = snake.isGhost ? 0.4f : 0.86f;
            g.setColour (snake.colour.withAlpha (baseAlpha));
            g.strokePath (spinePath, juce::PathStrokeType (juce::jmin (flatCellWidth, flatRowHeight) * (snake.isGhost ? 0.48f : 0.82f),
                                                           juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));

            g.setColour (snake.colour.withBrightness (1.0f).withAlpha (coreAlpha));
            g.strokePath (spinePath, juce::PathStrokeType (juce::jmin (flatCellWidth, flatRowHeight) * (snake.isGhost ? 0.12f : 0.22f),
                                                           juce::PathStrokeType::curved,
                                                           juce::PathStrokeType::rounded));

            auto headCentre = centres.getFirst();
            const auto head = snake.segments.getFirst();
            const auto nextCol = juce::jlimit (0, cols - 1, head.col + snake.directionX);
            const auto nextRow = juce::jlimit (0, rows - 1, head.row + snake.directionY);
            auto nextCell = juce::Rectangle<float> (flatBounds.getX() + nextCol * flatCellWidth,
                                                    flatBounds.getY() + nextRow * flatRowHeight,
                                                    flatCellWidth,
                                                    flatRowHeight).reduced (flatCellWidth * 0.13f, flatRowHeight * 0.13f);

            if (! snake.isGhost)
            {
                g.setColour (snake.colour.withAlpha (0.14f));
                g.fillRoundedRectangle (nextCell, 10.0f);
                g.setColour (snake.colour.withAlpha (0.48f));
                g.drawRoundedRectangle (nextCell.reduced (0.5f), 10.0f, 1.4f);
            }

            if (! snake.isGhost && (snake.directionX != 0 || snake.directionY != 0 || snake.directionLayer != 0))
            {
                const auto arrowLength = juce::jmin (flatCellWidth, flatRowHeight) * 0.68f;
                const auto endPoint = juce::Point<float> (headCentre.x + snake.directionX * arrowLength,
                                                          headCentre.y + snake.directionY * arrowLength);
                juce::Path heading;
                heading.startNewSubPath (headCentre);
                heading.lineTo (endPoint);

                g.setColour (snake.colour.withAlpha (0.82f));
                g.strokePath (heading, juce::PathStrokeType (2.4f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                const auto angle = std::atan2 ((float) snake.directionY, (float) snake.directionX);
                const auto wing = juce::jmin (flatCellWidth, flatRowHeight) * 0.18f;
                juce::Path arrowHead;
                arrowHead.startNewSubPath (endPoint);
                arrowHead.lineTo (endPoint.x - std::cos (angle - 0.62f) * wing, endPoint.y - std::sin (angle - 0.62f) * wing);
                arrowHead.startNewSubPath (endPoint);
                arrowHead.lineTo (endPoint.x - std::cos (angle + 0.62f) * wing, endPoint.y - std::sin (angle + 0.62f) * wing);
                g.strokePath (arrowHead, juce::PathStrokeType (2.2f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));

                if (snake.directionLayer != 0)
                {
                    auto layerTag = juce::Rectangle<float> (headCentre.x + flatCellWidth * 0.12f,
                                                            headCentre.y - flatRowHeight * 0.42f,
                                                            flatCellWidth * 0.3f,
                                                            flatRowHeight * 0.22f);
                    g.setColour (juce::Colour::fromFloatRGBA (0.02f, 0.03f, 0.05f, 0.82f));
                    g.fillRoundedRectangle (layerTag, 7.0f);
                    g.setColour (snake.colour.withAlpha (0.9f));
                    g.drawRoundedRectangle (layerTag.reduced (0.5f), 7.0f, 1.0f);
                    g.setColour (juce::Colours::white.withAlpha (0.96f));
                    g.setFont (juce::FontOptions (10.0f, juce::Font::bold));
                    g.drawFittedText (snake.directionLayer > 0 ? "Z+" : "Z-",
                                      layerTag.toNearestInt(),
                                      juce::Justification::centred,
                                      1);
                }
            }

            if (snake.lengthPulse > 0.0f)
            {
                const auto tailCentre = centres.getLast();
                const auto accent = snake.lengthDelta > 0 ? juce::Colour (0xff8cff9b) : juce::Colour (0xffffb36b);
                const auto haloRadius = juce::jmin (flatCellWidth, flatRowHeight) * (0.34f + snake.lengthPulse * 0.42f);
                g.setColour (accent.withAlpha (0.18f + snake.lengthPulse * 0.22f));
                g.fillEllipse (headCentre.x - haloRadius, headCentre.y - haloRadius, haloRadius * 2.0f, haloRadius * 2.0f);
                g.setColour (accent.withAlpha (0.8f));
                g.drawEllipse (headCentre.x - haloRadius, headCentre.y - haloRadius, haloRadius * 2.0f, haloRadius * 2.0f, 1.4f);
                g.setColour (accent.withAlpha (0.14f + snake.lengthPulse * 0.16f));
                g.fillEllipse (tailCentre.x - haloRadius * 0.68f, tailCentre.y - haloRadius * 0.68f, haloRadius * 1.36f, haloRadius * 1.36f);
            }
            auto headRadius = juce::jmin (flatCellWidth, flatRowHeight) * (snake.isGhost ? 0.08f : 0.11f);
            g.setColour (juce::Colours::white.withAlpha (snake.isGhost ? 0.55f : 0.9f));
            g.fillEllipse (headCentre.x - headRadius, headCentre.y - headRadius, headRadius * 2.0f, headRadius * 2.0f);
        }
    }
}

MainComponent::MainComponent()
{
    setSize (1460, 920);
    setWantsKeyboardFocus (true);
    setFocusContainerType (juce::Component::FocusContainerType::focusContainer);
    addKeyListener (this);
    initialiseGrid (currentMode);
    initialiseSnakes();
    publishPatchSnapshot();
    createUi();
    initialisePluginHosting();
    setAudioChannels (0, 2);
    startTimerHz (60);
    usePseudoDepthStageView = true;
    stage.setPseudoDepthEnabled (true);
    grabKeyboardFocus();
}

MainComponent::~MainComponent()
{
    if (registeredKeyTarget != nullptr)
        registeredKeyTarget->removeKeyListener (this);
    shutdownAudio();
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    if (key == juce::KeyPress::escapeKey && ! showingTitleScreen)
    {
        showingTitleScreen = true;
        sidebar.setVisible (false);
        mainArea.setVisible (false);
        previewArea.setVisible (false);
        inspector.setVisible (false);
        mixerArea.setVisible (false);
        stage.setVisible (false);
        toolViewport.setVisible (false);
        sidebarToggleButton.setVisible (false);

        for (auto* button : toolButtons)
            button->setVisible (false);

        for (auto* button : cellButtons)
            button->setVisible (false);

        for (auto* label : { &bpmValue, &beatValue, &toolValue, &audioValue })
            label->setVisible (false);

        for (auto* component : { static_cast<juce::Component*> (&playButton),
                                 static_cast<juce::Component*> (&clearButton),
                                 static_cast<juce::Component*> (&spawnSnakeButton),
                                 static_cast<juce::Component*> (&mixerToggleButton),
                                 static_cast<juce::Component*> (&layerDownButton),
                                 static_cast<juce::Component*> (&layerUpButton),
                                 static_cast<juce::Component*> (&saveButton),
                                 static_cast<juce::Component*> (&loadButton),
                                 static_cast<juce::Component*> (&parameterPickerButton),
                                 static_cast<juce::Component*> (&labelEditor),
                                 static_cast<juce::Component*> (&codeEditor),
                                 static_cast<juce::Component*> (&eraseButton) })
            component->setVisible (false);

        resumeModeButton.setVisible (hasResumableSession);
        a1ModeButton.setVisible (true);
        a2ModeButton.setVisible (true);
        a3ModeButton.setVisible (true);
        a4ModeButton.setVisible (true);
        b1ModeButton.setVisible (true);
        b2ModeButton.setVisible (true);
        b3ModeButton.setVisible (true);
        b4ModeButton.setVisible (true);
        resized();
        repaint();
        return true;
    }

    if (std::tolower ((unsigned char) key.getTextCharacter()) == 'm' && ! showingTitleScreen)
    {
        mixerVisible = ! mixerVisible;
        resized();
        repaint();
        return true;
    }

    if (std::tolower ((unsigned char) key.getTextCharacter()) == 'd' && ! showingTitleScreen)
    {
        auto& presetIndex = modePresetIndices[(size_t) modeIndex (currentMode)];
        presetIndex = (presetIndex + 1) % 3;
        applyMode (currentMode, currentVariant, false);
        return true;
    }

    return isViewToggleKey (key) ? toggleStageView() : false;
}

bool MainComponent::keyPressed (const juce::KeyPress& key, juce::Component*)
{
    return keyPressed (key);
}

bool MainComponent::toggleStageView()
{
    const auto isDefaultLayout = previewSizeMode == PanelSizeMode::defaultSize && inspectorSizeMode == PanelSizeMode::defaultSize;
    const auto isVoiceFocusLayout = previewSizeMode == PanelSizeMode::hidden && inspectorSizeMode == PanelSizeMode::halfScreen;
    const auto isIsoFocusLayout = previewSizeMode == PanelSizeMode::halfScreen && inspectorSizeMode == PanelSizeMode::hidden;

    if (isDefaultLayout)
    {
        previewSizeMode = PanelSizeMode::hidden;
        inspectorSizeMode = PanelSizeMode::halfScreen;
    }
    else if (isVoiceFocusLayout)
    {
        previewSizeMode = PanelSizeMode::halfScreen;
        inspectorSizeMode = PanelSizeMode::hidden;
    }
    else if (isIsoFocusLayout)
    {
        previewSizeMode = PanelSizeMode::defaultSize;
        inspectorSizeMode = PanelSizeMode::defaultSize;
    }
    else
    {
        previewSizeMode = PanelSizeMode::defaultSize;
        inspectorSizeMode = PanelSizeMode::defaultSize;
    }

    usePseudoDepthStageView = true;
    stage.setPseudoDepthEnabled (true);
    resized();
    repaint();
    scoreLabel.setText ("Score | Iso", juce::dontSendNotification);
    return true;
}

void MainComponent::parentHierarchyChanged()
{
    if (registeredKeyTarget != nullptr)
    {
        registeredKeyTarget->removeKeyListener (this);
        registeredKeyTarget = nullptr;
    }

    if (auto* topLevel = getTopLevelComponent())
    {
        topLevel->addKeyListener (this);
        registeredKeyTarget = topLevel;
    }
}

void MainComponent::initialiseGrid()
{
    initialiseGrid (currentMode);
}

void MainComponent::initialiseGrid (AppMode mode)
{
    grid.clear();
    currentMode = mode;
    const auto presetIndex = modePresetIndices[(size_t) modeIndex (mode)] % 3;
    bpm = mode == AppMode::harmonicGeometry ? 112.0 : (mode == AppMode::harmonySpace ? 104.0 : (mode == AppMode::cellularGrid ? 132.0 : 136.0));
    if (mode == AppMode::cellularGrid)
    {
        currentCellularRule = CellularRule::bloom;
        currentCellularBirthRange = { 5, 5 };
        currentCellularSurviveRange = { 4, 5 };
        if (presetIndex == 1)
        {
            currentCellularRule = CellularRule::maze;
            currentCellularBirthRange = { 5, 6 };
            currentCellularSurviveRange = { 5, 7 };
        }
        else if (presetIndex == 2)
        {
            currentCellularRule = CellularRule::coral;
            currentCellularBirthRange = { 6, 7 };
            currentCellularSurviveRange = { 4, 6 };
        }
    }

    for (int layer = 0; layer < layers; ++layer)
        for (int row = 0; row < rows; ++row)
            for (int col = 0; col < cols; ++col)
        {
            Cell cell;
            cell.layer = layer;
            cell.row = row;
            cell.col = col;
            cell.type = GlyphType::empty;
            auto def = getGlyphDefinition (cell.type);
            cell.label = def.label;
            cell.code = def.defaultCode;
            cell.program = ExpressionEvaluator::compile (cell.code);
            grid.add (cell);
        }

    const auto stamp = [this] (int layer, int row, int col, GlyphType type, const juce::String& code)
    {
        if (auto* cell = getCell (layer, row, col))
        {
            cell->type = type;
            cell->label = getGlyphDefinition (type).label;
            cell->code = code;
            cell->program = ExpressionEvaluator::compile (cell->code);
        }
    };

    if (mode == AppMode::harmonicGeometry)
    {
        bpm = presetIndex == 0 ? 92.0 : (presetIndex == 1 ? 84.0 : 104.0);

        // Layer 1: soft lounge comping with smooth ii-V-I motion
        stamp (0, 0, 0, GlyphType::pulse, "1");
        stamp (0, 0, 1, GlyphType::voice, "50");
        stamp (0, 0, 2, GlyphType::chord, "5");
        stamp (0, 0, 3, GlyphType::decay, "1.9");
        stamp (0, 0, 4, GlyphType::audio, "0.58");
        stamp (0, 0, 8, GlyphType::pulse, "1");
        stamp (0, 0, 9, GlyphType::voice, "55");
        stamp (0, 0, 10, GlyphType::chord, "4");
        stamp (0, 0, 11, GlyphType::decay, "1.7");
        stamp (0, 0, 12, GlyphType::audio, "0.54");

        stamp (0, 1, 2, GlyphType::pulse, "1");
        stamp (0, 1, 3, GlyphType::voice, "59");
        stamp (0, 1, 4, GlyphType::chord, "2");
        stamp (0, 1, 5, GlyphType::audio, "0.46");
        stamp (0, 1, 10, GlyphType::pulse, "1");
        stamp (0, 1, 11, GlyphType::voice, "62");
        stamp (0, 1, 12, GlyphType::chord, "5");
        stamp (0, 1, 13, GlyphType::audio, "0.42");

        // Layer 2: mellow key centres and root motion
        stamp (1, 0, 1, GlyphType::key, "-2");
        stamp (1, 0, 5, GlyphType::key, "0");
        stamp (1, 0, 9, GlyphType::key, "5");
        stamp (1, 0, 13, GlyphType::key, "7");
        stamp (1, 1, 2, GlyphType::octave, "0");
        stamp (1, 1, 6, GlyphType::octave, "-1");
        stamp (1, 1, 10, GlyphType::octave, "0");
        stamp (1, 1, 14, GlyphType::octave, "1");

        // Layer 3: humanised phrasing, soft emphasis, short echoes
        stamp (2, 0, 2, GlyphType::accent, "1.18");
        stamp (2, 0, 6, GlyphType::decay, "1.55");
        stamp (2, 0, 10, GlyphType::repeat, "1");
        stamp (2, 1, 3, GlyphType::accent, "1.08");
        stamp (2, 1, 7, GlyphType::decay, "1.4");
        stamp (2, 1, 11, GlyphType::repeat, "1");
        stamp (2, 2, 5, GlyphType::accent, "1.12");
        stamp (2, 2, 13, GlyphType::decay, "1.65");

        // Layer 4: restrained wormholes and chromatic slips
        stamp (3, 0, 4, GlyphType::wormhole, "1");
        stamp (3, 1, 12, GlyphType::wormhole, "1");
        stamp (3, 2, 2, GlyphType::key, "2");
        stamp (3, 2, 8, GlyphType::bias, "-0.05");
        stamp (3, 3, 9, GlyphType::wormhole, "2");
        stamp (3, 5, 5, GlyphType::wormhole, "2");

        // Layer 5: brushed rhythm section instead of hard electronic drums
        stamp (4, 4, 0, GlyphType::pulse, "1");
        stamp (4, 4, 1, GlyphType::kick, "0.20");
        stamp (4, 4, 2, GlyphType::hat, "0.22");
        stamp (4, 4, 4, GlyphType::pulse, "1");
        stamp (4, 4, 5, GlyphType::snare, "0.28");
        stamp (4, 4, 6, GlyphType::hat, "0.24");
        stamp (4, 4, 8, GlyphType::pulse, "1");
        stamp (4, 4, 9, GlyphType::kick, "0.18");
        stamp (4, 4, 10, GlyphType::hat, "0.22");
        stamp (4, 4, 12, GlyphType::pulse, "1");
        stamp (4, 4, 13, GlyphType::clap, "0.18");
        stamp (4, 4, 14, GlyphType::hat, "0.20");
        stamp (4, 5, 3, GlyphType::ratchet, "2");
        stamp (4, 5, 11, GlyphType::repeat, "1");

        // Layer 6: vibraphone/flute-like upper voices
        stamp (5, 2, 2, GlyphType::voice, "67");
        stamp (5, 2, 3, GlyphType::decay, "2.2");
        stamp (5, 2, 4, GlyphType::audio, "0.24");
        stamp (5, 2, 10, GlyphType::voice, "71");
        stamp (5, 2, 11, GlyphType::decay, "2.0");
        stamp (5, 2, 12, GlyphType::audio, "0.22");
        stamp (5, 3, 6, GlyphType::octave, "1");
        stamp (5, 3, 7, GlyphType::accent, "1.06");
        stamp (5, 6, 4, GlyphType::visual, "0.58");
        stamp (5, 6, 12, GlyphType::visual, "0.66");
        stamp (5, 7, 3, GlyphType::hue, "0.10");
        stamp (5, 7, 11, GlyphType::hue, "0.38");

        // Layer 7: turnaround colouration and passing harmony
        stamp (6, 0, 3, GlyphType::key, "9");
        stamp (6, 1, 4, GlyphType::chord, "3");
        stamp (6, 2, 5, GlyphType::octave, "0");
        stamp (6, 3, 6, GlyphType::accent, "1.14");
        stamp (6, 4, 7, GlyphType::decay, "1.35");
        stamp (6, 5, 8, GlyphType::repeat, "1");
        stamp (6, 6, 9, GlyphType::tempo, "1.1");
        stamp (6, 7, 10, GlyphType::visual, "0.42");

        // Layer 8: airy high-register canopy
        stamp (7, 0, 6, GlyphType::pulse, "1");
        stamp (7, 0, 7, GlyphType::voice, "74");
        stamp (7, 0, 8, GlyphType::chord, "4");
        stamp (7, 0, 9, GlyphType::decay, "2.1");
        stamp (7, 0, 10, GlyphType::audio, "0.18");
        stamp (7, 1, 9, GlyphType::pulse, "1");
        stamp (7, 1, 10, GlyphType::voice, "79");
        stamp (7, 1, 11, GlyphType::chord, "5");
        stamp (7, 1, 12, GlyphType::audio, "0.16");
        stamp (7, 2, 12, GlyphType::wormhole, "3");
        stamp (2, 6, 2, GlyphType::wormhole, "3");

        if (presetIndex == 1)
        {
            stamp (0, 0, 9, GlyphType::voice, "52");
            stamp (0, 0, 10, GlyphType::chord, "1");
            stamp (4, 4, 13, GlyphType::clap, "0.12");
            stamp (5, 2, 10, GlyphType::voice, "74");
        }
        else if (presetIndex == 2)
        {
            stamp (1, 0, 13, GlyphType::key, "10");
            stamp (2, 0, 10, GlyphType::repeat, "2");
            stamp (6, 4, 7, GlyphType::decay, "1.75");
            stamp (7, 1, 10, GlyphType::voice, "82");
        }

        selectedCell = getCell (0, 0, 1);
        return;
    }

    if (mode == AppMode::harmonySpace)
    {
        bpm = presetIndex == 0 ? 76.0 : (presetIndex == 1 ? 68.0 : 88.0);
        harmonySpaceKeyCenter = presetIndex == 2 ? 0 : 7;
        harmonySpaceConstraintMode = presetIndex == 1 ? 2 : 1;
        harmonySpaceGestureRecordEnabled = false;
        harmonySpaceGesturePoints.clear();

        // Layer 1: melody phrase, four cells per bar.
        stamp (0, 0, 0, GlyphType::pulse, "1");
        stamp (0, 0, 1, GlyphType::voice, "67");
        stamp (0, 0, 2, GlyphType::chord, "0");
        stamp (0, 0, 3, GlyphType::audio, "0.78");

        stamp (0, 0, 4, GlyphType::pulse, "1");
        stamp (0, 0, 5, GlyphType::voice, "67");
        stamp (0, 0, 6, GlyphType::chord, "0");
        stamp (0, 0, 7, GlyphType::audio, "0.74");

        stamp (0, 0, 8, GlyphType::pulse, "1");
        stamp (0, 0, 9, GlyphType::voice, "69");
        stamp (0, 0, 10, GlyphType::chord, "4");
        stamp (0, 0, 11, GlyphType::audio, "0.70");

        stamp (0, 0, 12, GlyphType::pulse, "1");
        stamp (0, 0, 13, GlyphType::voice, "66");
        stamp (0, 0, 14, GlyphType::chord, "4");
        stamp (0, 0, 15, GlyphType::audio, "0.66");

        stamp (0, 1, 0, GlyphType::pulse, "1");
        stamp (0, 1, 1, GlyphType::voice, "67");
        stamp (0, 1, 2, GlyphType::chord, "0");
        stamp (0, 1, 3, GlyphType::audio, "0.72");

        stamp (0, 1, 4, GlyphType::pulse, "1");
        stamp (0, 1, 5, GlyphType::voice, "69");
        stamp (0, 1, 6, GlyphType::chord, "1");
        stamp (0, 1, 7, GlyphType::audio, "0.66");

        stamp (0, 1, 8, GlyphType::pulse, "1");
        stamp (0, 1, 9, GlyphType::voice, "71");
        stamp (0, 1, 10, GlyphType::chord, "2");
        stamp (0, 1, 11, GlyphType::audio, "0.62");

        stamp (0, 1, 12, GlyphType::pulse, "1");
        stamp (0, 1, 13, GlyphType::voice, "69");
        stamp (0, 1, 14, GlyphType::chord, "1");
        stamp (0, 1, 15, GlyphType::audio, "0.60");

        stamp (0, 2, 0, GlyphType::pulse, "1");
        stamp (0, 2, 1, GlyphType::voice, "67");
        stamp (0, 2, 2, GlyphType::chord, "0");
        stamp (0, 2, 3, GlyphType::audio, "0.68");

        stamp (0, 2, 4, GlyphType::pulse, "1");
        stamp (0, 2, 5, GlyphType::voice, "69");
        stamp (0, 2, 6, GlyphType::chord, "1");
        stamp (0, 2, 7, GlyphType::audio, "0.64");

        stamp (0, 2, 8, GlyphType::pulse, "1");
        stamp (0, 2, 9, GlyphType::voice, "71");
        stamp (0, 2, 10, GlyphType::chord, "2");
        stamp (0, 2, 11, GlyphType::audio, "0.60");

        stamp (0, 2, 12, GlyphType::pulse, "1");
        stamp (0, 2, 13, GlyphType::voice, "72");
        stamp (0, 2, 14, GlyphType::chord, "3");
        stamp (0, 2, 15, GlyphType::audio, "0.58");

        stamp (0, 3, 0, GlyphType::pulse, "1");
        stamp (0, 3, 1, GlyphType::voice, "71");
        stamp (0, 3, 2, GlyphType::chord, "2");
        stamp (0, 3, 3, GlyphType::audio, "0.62");

        stamp (0, 3, 4, GlyphType::pulse, "1");
        stamp (0, 3, 5, GlyphType::voice, "69");
        stamp (0, 3, 6, GlyphType::chord, "1");
        stamp (0, 3, 7, GlyphType::audio, "0.60");

        stamp (0, 3, 8, GlyphType::pulse, "1");
        stamp (0, 3, 9, GlyphType::voice, "67");
        stamp (0, 3, 10, GlyphType::chord, "0");
        stamp (0, 3, 11, GlyphType::audio, "0.58");

        stamp (0, 3, 12, GlyphType::pulse, "1");
        stamp (0, 3, 13, GlyphType::voice, "67");
        stamp (0, 3, 14, GlyphType::decay, "1.9");
        stamp (0, 3, 15, GlyphType::audio, "0.54");

        // Layer 2: harmonic direction in G major.
        stamp (1, 0, 0, GlyphType::key, "7");
        stamp (1, 0, 4, GlyphType::key, "7");
        stamp (1, 0, 8, GlyphType::key, "7");
        stamp (1, 0, 12, GlyphType::key, "7");
        stamp (1, 1, 2, GlyphType::chord, "0");
        stamp (1, 1, 6, GlyphType::chord, "1");
        stamp (1, 1, 10, GlyphType::chord, "2");
        stamp (1, 1, 14, GlyphType::chord, "1");
        stamp (1, 2, 2, GlyphType::octave, "0");
        stamp (1, 2, 6, GlyphType::octave, "0");
        stamp (1, 2, 10, GlyphType::octave, "0");
        stamp (1, 2, 14, GlyphType::octave, "1");

        // Layer 3: bass pedal and cadence.
        stamp (2, 5, 0, GlyphType::pulse, "1");
        stamp (2, 5, 1, GlyphType::voice, "43");
        stamp (2, 5, 2, GlyphType::octave, "-1");
        stamp (2, 5, 3, GlyphType::audio, "0.34");
        stamp (2, 5, 4, GlyphType::pulse, "1");
        stamp (2, 5, 5, GlyphType::voice, "43");
        stamp (2, 5, 6, GlyphType::audio, "0.30");
        stamp (2, 5, 8, GlyphType::pulse, "1");
        stamp (2, 5, 9, GlyphType::voice, "45");
        stamp (2, 5, 10, GlyphType::audio, "0.28");
        stamp (2, 5, 12, GlyphType::pulse, "1");
        stamp (2, 5, 13, GlyphType::voice, "38");
        stamp (2, 5, 14, GlyphType::audio, "0.30");

        // Layer 4: sustaining inner harmony.
        stamp (3, 4, 1, GlyphType::voice, "59");
        stamp (3, 4, 2, GlyphType::decay, "2.4");
        stamp (3, 4, 3, GlyphType::audio, "0.26");
        stamp (3, 4, 5, GlyphType::voice, "62");
        stamp (3, 4, 6, GlyphType::audio, "0.22");
        stamp (3, 4, 9, GlyphType::voice, "64");
        stamp (3, 4, 10, GlyphType::audio, "0.22");
        stamp (3, 4, 13, GlyphType::voice, "62");
        stamp (3, 4, 14, GlyphType::audio, "0.20");

        // Layer 5: gentle phrase shaping.
        stamp (4, 0, 3, GlyphType::accent, "1.10");
        stamp (4, 0, 7, GlyphType::decay, "1.5");
        stamp (4, 1, 11, GlyphType::repeat, "1");
        stamp (4, 2, 15, GlyphType::decay, "1.8");
        stamp (4, 3, 3, GlyphType::accent, "1.08");
        stamp (4, 3, 11, GlyphType::repeat, "1");

        // Layer 6: upper-answer tones.
        stamp (5, 6, 2, GlyphType::pulse, "1");
        stamp (5, 6, 3, GlyphType::voice, "74");
        stamp (5, 6, 4, GlyphType::audio, "0.18");
        stamp (5, 6, 10, GlyphType::pulse, "1");
        stamp (5, 6, 11, GlyphType::voice, "76");
        stamp (5, 6, 12, GlyphType::audio, "0.16");

        // Layer 7: light colour.
        stamp (6, 6, 5, GlyphType::visual, "0.52");
        stamp (6, 6, 13, GlyphType::visual, "0.62");
        stamp (6, 7, 7, GlyphType::hue, "0.18");

        // Layer 8: restrained wormholes joining equivalent cadential points.
        stamp (7, 0, 6, GlyphType::wormhole, "1");
        stamp (7, 2, 10, GlyphType::wormhole, "1");
        stamp (7, 1, 14, GlyphType::wormhole, "2");
        stamp (7, 3, 2, GlyphType::wormhole, "2");

        if (presetIndex == 1)
        {
            stamp (1, 1, 10, GlyphType::chord, "4");
            stamp (2, 5, 9, GlyphType::voice, "47");
            stamp (5, 6, 11, GlyphType::voice, "79");
        }
        else if (presetIndex == 2)
        {
            stamp (1, 0, 0, GlyphType::key, "0");
            stamp (3, 4, 9, GlyphType::voice, "67");
            stamp (4, 0, 7, GlyphType::decay, "1.9");
        }

        selectedCell = getCell (0, 0, 1);
        return;
    }

    if (mode == AppMode::cellularGrid)
    {
        bpm = presetIndex == 0 ? 108.0 : (presetIndex == 1 ? 92.0 : 124.0);

        // Layer 1: sparse low pulse islands for the automaton to grow across.
        stamp (0, 0, 0, GlyphType::pulse, "1");
        stamp (0, 0, 1, GlyphType::tone, "33");
        stamp (0, 0, 2, GlyphType::audio, "0.46");
        stamp (0, 0, 8, GlyphType::pulse, "1");
        stamp (0, 0, 9, GlyphType::tone, "36");
        stamp (0, 0, 10, GlyphType::audio, "0.42");
        stamp (0, 2, 4, GlyphType::pulse, "1");
        stamp (0, 2, 5, GlyphType::tone, "40");
        stamp (0, 2, 6, GlyphType::audio, "0.34");

        // Layer 2: chord islands rather than constant lead stacks.
        stamp (1, 1, 2, GlyphType::pulse, "1");
        stamp (1, 1, 3, GlyphType::voice, "57");
        stamp (1, 1, 4, GlyphType::chord, "1");
        stamp (1, 1, 5, GlyphType::audio, "0.30");
        stamp (1, 3, 10, GlyphType::pulse, "1");
        stamp (1, 3, 11, GlyphType::voice, "62");
        stamp (1, 3, 12, GlyphType::chord, "4");
        stamp (1, 3, 13, GlyphType::audio, "0.28");

        // Layer 3: key and octave drift to keep motion harmonic.
        stamp (2, 0, 4, GlyphType::key, "0");
        stamp (2, 2, 8, GlyphType::key, "5");
        stamp (2, 4, 12, GlyphType::key, "7");
        stamp (2, 5, 3, GlyphType::octave, "-1");
        stamp (2, 6, 11, GlyphType::octave, "0");

        // Layer 4: phrasing and amplitude shaping.
        stamp (3, 1, 6, GlyphType::accent, "1.12");
        stamp (3, 2, 7, GlyphType::decay, "1.5");
        stamp (3, 4, 5, GlyphType::mul, "0.78");
        stamp (3, 5, 9, GlyphType::repeat, "1");
        stamp (3, 6, 13, GlyphType::length, "1.4");

        // Layer 5: rhythmic melodic punctures instead of drums.
        stamp (4, 5, 1, GlyphType::pulse, "1");
        stamp (4, 5, 2, GlyphType::voice, "50");
        stamp (4, 5, 3, GlyphType::audio, "0.18");
        stamp (4, 5, 9, GlyphType::pulse, "1");
        stamp (4, 5, 10, GlyphType::voice, "55");
        stamp (4, 5, 11, GlyphType::audio, "0.16");
        stamp (4, 6, 6, GlyphType::voice, "62");
        stamp (4, 6, 7, GlyphType::audio, "0.14");
        stamp (4, 6, 12, GlyphType::ratchet, "2");

        // Layer 6: higher answer tones.
        stamp (5, 2, 1, GlyphType::pulse, "1");
        stamp (5, 2, 2, GlyphType::voice, "69");
        stamp (5, 2, 3, GlyphType::audio, "0.24");
        stamp (5, 4, 13, GlyphType::pulse, "1");
        stamp (5, 4, 14, GlyphType::voice, "74");
        stamp (5, 4, 15, GlyphType::audio, "0.20");

        // Layer 7: gentle tempo and timbral colour changes.
        stamp (6, 1, 8, GlyphType::tempo, "0.75");
        stamp (6, 3, 4, GlyphType::tempo, "1.15");
        stamp (6, 4, 7, GlyphType::noise, "0.08");
        stamp (6, 6, 5, GlyphType::visual, "0.46");
        stamp (6, 7, 10, GlyphType::hue, "0.28");

        // Layer 8: a few wormholes to create punctuated long jumps.
        stamp (7, 0, 6, GlyphType::wormhole, "1");
        stamp (7, 3, 13, GlyphType::wormhole, "1");
        stamp (7, 2, 3, GlyphType::wormhole, "2");
        stamp (7, 6, 12, GlyphType::wormhole, "2");

        if (presetIndex == 1)
        {
            stamp (1, 5, 1, GlyphType::voice, "52");
            stamp (3, 3, 9, GlyphType::repeat, "2");
            stamp (6, 1, 8, GlyphType::tempo, "0.85");
        }
        else if (presetIndex == 2)
        {
            stamp (0, 2, 5, GlyphType::tone, "45");
            stamp (1, 3, 11, GlyphType::voice, "67");
            stamp (6, 3, 4, GlyphType::tempo, "1.35");
        }

        selectedCell = getCell (0, 0, 1);
        return;
    }

    // Layer 1: foundation groove and bass motif
    stamp (0, 0, 0, GlyphType::pulse, "1");
    stamp (0, 0, 1, GlyphType::tone, "33");
    stamp (0, 0, 2, GlyphType::audio, "1");
    stamp (0, 0, 4, GlyphType::pulse, "1");
    stamp (0, 0, 5, GlyphType::tone, "38");
    stamp (0, 0, 6, GlyphType::audio, "0.95");
    stamp (0, 0, 8, GlyphType::pulse, "1");
    stamp (0, 0, 9, GlyphType::tone, "38");
    stamp (0, 0, 10, GlyphType::audio, "1");
    stamp (0, 0, 11, GlyphType::repeat, "1");
    stamp (0, 0, 12, GlyphType::pulse, "1");
    stamp (0, 0, 13, GlyphType::tone, "36");
    stamp (0, 0, 14, GlyphType::audio, "1");

    stamp (0, 1, 1, GlyphType::pulse, "1");
    stamp (0, 1, 2, GlyphType::voice, "57");
    stamp (0, 1, 3, GlyphType::audio, "0.84");
    stamp (0, 1, 5, GlyphType::pulse, "1");
    stamp (0, 1, 6, GlyphType::voice, "60");
    stamp (0, 1, 7, GlyphType::audio, "0.86");
    stamp (0, 1, 9, GlyphType::pulse, "1");
    stamp (0, 1, 10, GlyphType::voice, "62");
    stamp (0, 1, 11, GlyphType::audio, "0.88");
    stamp (0, 1, 12, GlyphType::ratchet, "2");
    stamp (0, 1, 13, GlyphType::pulse, "1");
    stamp (0, 1, 14, GlyphType::voice, "69");
    stamp (0, 1, 15, GlyphType::audio, "0.9");

    stamp (0, 2, 2, GlyphType::pulse, "1");
    stamp (0, 2, 3, GlyphType::voice, "69");
    stamp (0, 2, 4, GlyphType::audio, "0.68");
    stamp (0, 2, 6, GlyphType::pulse, "1");
    stamp (0, 2, 7, GlyphType::voice, "71");
    stamp (0, 2, 8, GlyphType::audio, "0.72");
    stamp (0, 2, 10, GlyphType::pulse, "1");
    stamp (0, 2, 11, GlyphType::voice, "76");
    stamp (0, 2, 12, GlyphType::audio, "0.7");
    stamp (0, 2, 13, GlyphType::wormhole, "10");
    stamp (0, 2, 14, GlyphType::pulse, "1");
    stamp (0, 2, 15, GlyphType::voice, "76");
    stamp (0, 2, 5, GlyphType::wormhole, "4");

    stamp (0, 3, 0, GlyphType::pulse, "1");
    stamp (0, 3, 1, GlyphType::voice, "81");
    stamp (0, 3, 2, GlyphType::audio, "0.54");
    stamp (0, 3, 8, GlyphType::pulse, "1");
    stamp (0, 3, 9, GlyphType::voice, "79");
    stamp (0, 3, 10, GlyphType::audio, "0.52");
    stamp (0, 3, 11, GlyphType::voice, "88");
    stamp (0, 3, 12, GlyphType::repeat, "1");
    stamp (0, 3, 13, GlyphType::audio, "0.46");

    stamp (0, 4, 0, GlyphType::pulse, "1");
    stamp (0, 4, 1, GlyphType::kick, "1");
    stamp (0, 4, 2, GlyphType::hat, "0.68");
    stamp (0, 4, 3, GlyphType::hat, "0.92");
    stamp (0, 4, 4, GlyphType::pulse, "1");
    stamp (0, 4, 5, GlyphType::hat, "0.85");
    stamp (0, 4, 6, GlyphType::snare, "0.44");
    stamp (0, 4, 7, GlyphType::clap, "0.38");
    stamp (0, 4, 8, GlyphType::pulse, "1");
    stamp (0, 4, 9, GlyphType::kick, "0.92");
    stamp (0, 4, 10, GlyphType::hat, "0.72");
    stamp (0, 4, 11, GlyphType::hat, "0.94");
    stamp (0, 4, 12, GlyphType::pulse, "1");
    stamp (0, 4, 13, GlyphType::hat, "0.9");
    stamp (0, 4, 14, GlyphType::clap, "0.34");
    stamp (0, 4, 15, GlyphType::snare, "0.46");

    stamp (0, 5, 2, GlyphType::pulse, "1");
    stamp (0, 5, 3, GlyphType::snare, "0.92");
    stamp (0, 5, 4, GlyphType::hat, "0.66");
    stamp (0, 5, 5, GlyphType::ratchet, "4");
    stamp (0, 5, 6, GlyphType::pulse, "1");
    stamp (0, 5, 7, GlyphType::hat, "0.72");
    stamp (0, 5, 8, GlyphType::kick, "0.54");
    stamp (0, 5, 9, GlyphType::hat, "0.86");
    stamp (0, 5, 10, GlyphType::pulse, "1");
    stamp (0, 5, 11, GlyphType::clap, "0.88");
    stamp (0, 5, 12, GlyphType::hat, "0.76");
    stamp (0, 5, 13, GlyphType::repeat, "2");
    stamp (0, 5, 14, GlyphType::pulse, "1");
    stamp (0, 5, 15, GlyphType::hat, "0.74");

    stamp (0, 6, 3, GlyphType::pulse, "1");
    stamp (0, 6, 4, GlyphType::voice, "88");
    stamp (0, 6, 5, GlyphType::audio, "0.34");
    stamp (0, 6, 11, GlyphType::pulse, "1");
    stamp (0, 6, 12, GlyphType::voice, "91");
    stamp (0, 6, 13, GlyphType::audio, "0.36");

    stamp (0, 7, 0, GlyphType::hue, "0.12");
    stamp (0, 7, 7, GlyphType::visual, "0.68");
    stamp (0, 7, 15, GlyphType::visual, "0.96");

    // Layer 2: verse harmony and key centers
    stamp (1, 0, 1, GlyphType::chord, "1");
    stamp (1, 0, 5, GlyphType::chord, "2");
    stamp (1, 0, 9, GlyphType::chord, "4");
    stamp (1, 0, 13, GlyphType::chord, "5");
    stamp (1, 0, 2, GlyphType::mul, "0.92");
    stamp (1, 0, 10, GlyphType::mul, "1.04");

    stamp (1, 1, 2, GlyphType::key, "0");
    stamp (1, 1, 6, GlyphType::key, "2");
    stamp (1, 1, 10, GlyphType::key, "5");
    stamp (1, 1, 14, GlyphType::key, "7");
    stamp (1, 1, 3, GlyphType::mul, "0.88");
    stamp (1, 1, 11, GlyphType::bias, "0.08");

    stamp (1, 2, 3, GlyphType::chord, "2");
    stamp (1, 2, 7, GlyphType::chord, "3");
    stamp (1, 2, 11, GlyphType::chord, "4");
    stamp (1, 2, 15, GlyphType::chord, "5");
    stamp (1, 2, 4, GlyphType::repeat, "1");
    stamp (1, 2, 12, GlyphType::repeat, "2");
    stamp (1, 2, 9, GlyphType::wormhole, "5");

    stamp (1, 3, 1, GlyphType::mul, "0.8");
    stamp (1, 3, 5, GlyphType::key, "12");
    stamp (1, 3, 9, GlyphType::key, "7");
    stamp (1, 3, 13, GlyphType::key, "12");
    stamp (1, 3, 14, GlyphType::repeat, "1");

    stamp (1, 4, 1, GlyphType::tempo, "1.5");
    stamp (1, 4, 5, GlyphType::ratchet, "2");
    stamp (1, 4, 7, GlyphType::repeat, "1");
    stamp (1, 4, 9, GlyphType::tempo, "1.25");
    stamp (1, 4, 13, GlyphType::ratchet, "3");
    stamp (1, 4, 14, GlyphType::repeat, "1");
    stamp (1, 4, 15, GlyphType::length, "2");
    stamp (1, 4, 3, GlyphType::tempo, "2.5");
    stamp (1, 4, 11, GlyphType::ratchet, "5");

    stamp (1, 5, 5, GlyphType::repeat, "2");
    stamp (1, 5, 6, GlyphType::tempo, "0.75");
    stamp (1, 5, 13, GlyphType::repeat, "3");
    stamp (1, 5, 14, GlyphType::tempo, "1.15");

    stamp (1, 6, 4, GlyphType::voice, "96");
    stamp (1, 6, 5, GlyphType::audio, "0.46");
    stamp (1, 6, 12, GlyphType::voice, "100");
    stamp (1, 6, 13, GlyphType::audio, "0.42");
    stamp (1, 6, 14, GlyphType::hue, "0.74");

    // Layer 3: pre-chorus acceleration and setup
    stamp (2, 0, 1, GlyphType::tempo, "1.5");
    stamp (2, 0, 5, GlyphType::repeat, "1");
    stamp (2, 0, 9, GlyphType::tempo, "0.75");
    stamp (2, 0, 13, GlyphType::repeat, "2");

    stamp (2, 1, 2, GlyphType::ratchet, "2");
    stamp (2, 1, 6, GlyphType::ratchet, "3");
    stamp (2, 1, 10, GlyphType::ratchet, "4");
    stamp (2, 1, 14, GlyphType::ratchet, "5");
    stamp (2, 1, 15, GlyphType::length, "3");
    stamp (2, 1, 4, GlyphType::tempo, "3");
    stamp (2, 1, 12, GlyphType::repeat, "3");

    stamp (2, 2, 3, GlyphType::key, "7");
    stamp (2, 2, 7, GlyphType::key, "9");
    stamp (2, 2, 11, GlyphType::key, "12");
    stamp (2, 2, 15, GlyphType::key, "14");
    stamp (2, 2, 1, GlyphType::wormhole, "4");

    stamp (2, 3, 1, GlyphType::chord, "5");
    stamp (2, 3, 5, GlyphType::repeat, "1");
    stamp (2, 3, 9, GlyphType::ratchet, "2");
    stamp (2, 3, 13, GlyphType::repeat, "2");

    stamp (2, 4, 1, GlyphType::kick, "0.9");
    stamp (2, 4, 3, GlyphType::hat, "0.74");
    stamp (2, 4, 5, GlyphType::tempo, "2");
    stamp (2, 4, 7, GlyphType::clap, "0.7");
    stamp (2, 4, 9, GlyphType::snare, "0.82");
    stamp (2, 4, 13, GlyphType::hat, "0.78");
    stamp (2, 4, 11, GlyphType::tempo, "1.5");
    stamp (2, 4, 15, GlyphType::visual, "0.44");
    stamp (2, 4, 6, GlyphType::hat, "0.95");
    stamp (2, 4, 12, GlyphType::kick, "0.62");

    stamp (2, 5, 3, GlyphType::hat, "0.82");
    stamp (2, 5, 5, GlyphType::kick, "0.58");
    stamp (2, 5, 7, GlyphType::repeat, "2");
    stamp (2, 5, 8, GlyphType::ratchet, "3");
    stamp (2, 5, 9, GlyphType::tempo, "0.66");
    stamp (2, 5, 10, GlyphType::length, "1");
    stamp (2, 5, 12, GlyphType::snare, "0.62");
    stamp (2, 5, 14, GlyphType::clap, "0.8");
    stamp (2, 5, 1, GlyphType::hat, "0.88");
    stamp (2, 5, 11, GlyphType::ratchet, "6");

    stamp (2, 6, 4, GlyphType::key, "12");
    stamp (2, 6, 8, GlyphType::repeat, "1");
    stamp (2, 6, 12, GlyphType::key, "19");
    stamp (2, 6, 13, GlyphType::visual, "0.58");
    stamp (2, 6, 14, GlyphType::length, "4");
    stamp (2, 7, 3, GlyphType::wormhole, "1");

    // Layer 4: chorus glow and hook emphasis
    stamp (3, 0, 1, GlyphType::hue, "0.16");
    stamp (3, 0, 5, GlyphType::visual, "0.42");
    stamp (3, 0, 9, GlyphType::hue, "0.22");
    stamp (3, 0, 13, GlyphType::visual, "0.5");

    stamp (3, 1, 3, GlyphType::voice, "69");
    stamp (3, 1, 4, GlyphType::audio, "0.3");
    stamp (3, 1, 11, GlyphType::visual, "0.46");
    stamp (3, 1, 12, GlyphType::voice, "101");

    stamp (3, 2, 7, GlyphType::hue, "0.48");
    stamp (3, 2, 8, GlyphType::voice, "76");
    stamp (3, 2, 9, GlyphType::audio, "0.28");
    stamp (3, 2, 15, GlyphType::visual, "0.44");
    stamp (3, 2, 13, GlyphType::wormhole, "5");
    stamp (3, 2, 4, GlyphType::voice, "98");

    stamp (3, 3, 5, GlyphType::visual, "0.38");
    stamp (3, 3, 9, GlyphType::hue, "0.74");

    stamp (3, 4, 7, GlyphType::visual, "0.34");
    stamp (3, 5, 13, GlyphType::visual, "0.48");
    stamp (3, 6, 5, GlyphType::visual, "0.42");
    stamp (3, 7, 12, GlyphType::wormhole, "1");

    // Layer 5: bridge reharmonization and crossfade
    stamp (4, 0, 0, GlyphType::chord, "3");
    stamp (4, 0, 4, GlyphType::key, "-5");
    stamp (4, 0, 8, GlyphType::chord, "2");
    stamp (4, 0, 12, GlyphType::key, "7");
    stamp (4, 0, 14, GlyphType::repeat, "1");

    stamp (4, 1, 1, GlyphType::tempo, "1.5");
    stamp (4, 1, 5, GlyphType::ratchet, "2");
    stamp (4, 1, 9, GlyphType::tempo, "0.75");
    stamp (4, 1, 13, GlyphType::ratchet, "3");
    stamp (4, 1, 15, GlyphType::length, "3");
    stamp (4, 1, 7, GlyphType::tempo, "3");

    stamp (4, 2, 2, GlyphType::voice, "81");
    stamp (4, 2, 3, GlyphType::mul, "0.74");
    stamp (4, 2, 4, GlyphType::audio, "0.42");
    stamp (4, 2, 10, GlyphType::voice, "88");
    stamp (4, 2, 11, GlyphType::mul, "0.7");
    stamp (4, 2, 12, GlyphType::audio, "0.4");
    stamp (4, 2, 14, GlyphType::wormhole, "6");

    stamp (4, 3, 1, GlyphType::key, "12");
    stamp (4, 3, 5, GlyphType::repeat, "2");
    stamp (4, 3, 9, GlyphType::chord, "5");
    stamp (4, 3, 13, GlyphType::bias, "0.12");
    stamp (4, 3, 15, GlyphType::length, "4");

    stamp (4, 4, 3, GlyphType::noise, "0.06");
    stamp (4, 4, 4, GlyphType::mul, "0.36");
    stamp (4, 4, 5, GlyphType::audio, "0.24");
    stamp (4, 4, 7, GlyphType::hat, "0.72");
    stamp (4, 4, 11, GlyphType::noise, "0.05");
    stamp (4, 4, 12, GlyphType::mul, "0.32");
    stamp (4, 4, 13, GlyphType::audio, "0.22");
    stamp (4, 4, 15, GlyphType::kick, "0.66");

    stamp (4, 5, 2, GlyphType::hue, "0.18");
    stamp (4, 5, 6, GlyphType::visual, "0.46");
    stamp (4, 5, 10, GlyphType::hue, "0.68");
    stamp (4, 5, 14, GlyphType::visual, "0.56");
    stamp (4, 7, 5, GlyphType::wormhole, "2");

    stamp (4, 6, 4, GlyphType::tempo, "2");
    stamp (4, 6, 8, GlyphType::repeat, "3");
    stamp (4, 6, 12, GlyphType::ratchet, "4");
    stamp (4, 6, 14, GlyphType::key, "19");

    // Layer 6: countermelody and turnaround
    stamp (5, 0, 1, GlyphType::key, "-12");
    stamp (5, 0, 5, GlyphType::chord, "4");
    stamp (5, 0, 9, GlyphType::key, "5");
    stamp (5, 0, 13, GlyphType::chord, "2");

    stamp (5, 1, 2, GlyphType::tempo, "0.5");
    stamp (5, 1, 6, GlyphType::ratchet, "4");
    stamp (5, 1, 10, GlyphType::repeat, "2");
    stamp (5, 1, 14, GlyphType::length, "2");
    stamp (5, 1, 4, GlyphType::tempo, "2.5");
    stamp (5, 1, 12, GlyphType::ratchet, "6");

    stamp (5, 2, 0, GlyphType::voice, "69");
    stamp (5, 2, 1, GlyphType::audio, "0.34");
    stamp (5, 2, 8, GlyphType::voice, "74");
    stamp (5, 2, 9, GlyphType::audio, "0.32");
    stamp (5, 2, 15, GlyphType::bias, "-0.08");
    stamp (5, 2, 6, GlyphType::wormhole, "6");

    stamp (5, 3, 3, GlyphType::chord, "1");
    stamp (5, 3, 7, GlyphType::key, "7");
    stamp (5, 3, 11, GlyphType::repeat, "1");
    stamp (5, 3, 15, GlyphType::tempo, "1.25");

    stamp (5, 4, 1, GlyphType::noise, "0.045");
    stamp (5, 4, 2, GlyphType::audio, "0.22");
    stamp (5, 4, 4, GlyphType::snare, "0.56");
    stamp (5, 4, 9, GlyphType::noise, "0.04");
    stamp (5, 4, 10, GlyphType::audio, "0.2");
    stamp (5, 4, 12, GlyphType::hat, "0.8");
    stamp (5, 4, 14, GlyphType::mul, "0.52");

    stamp (5, 5, 4, GlyphType::visual, "0.34");
    stamp (5, 5, 8, GlyphType::hue, "0.84");
    stamp (5, 5, 12, GlyphType::visual, "0.5");
    stamp (5, 7, 10, GlyphType::wormhole, "2");

    stamp (5, 6, 5, GlyphType::key, "24");
    stamp (5, 6, 9, GlyphType::ratchet, "2");
    stamp (5, 6, 13, GlyphType::repeat, "2");
    stamp (5, 6, 15, GlyphType::length, "1");

    // Layer 7: breakdown / suspended section
    stamp (6, 0, 2, GlyphType::chord, "5");
    stamp (6, 0, 6, GlyphType::tempo, "1.75");
    stamp (6, 0, 10, GlyphType::key, "-7");
    stamp (6, 0, 14, GlyphType::repeat, "3");

    stamp (6, 1, 1, GlyphType::voice, "93");
    stamp (6, 1, 2, GlyphType::audio, "0.28");
    stamp (6, 1, 9, GlyphType::voice, "98");
    stamp (6, 1, 10, GlyphType::audio, "0.26");
    stamp (6, 1, 14, GlyphType::bias, "0.16");

    stamp (6, 2, 3, GlyphType::tempo, "0.75");
    stamp (6, 2, 7, GlyphType::ratchet, "5");
    stamp (6, 2, 11, GlyphType::length, "4");
    stamp (6, 2, 15, GlyphType::repeat, "1");
    stamp (6, 2, 5, GlyphType::wormhole, "7");
    stamp (6, 2, 9, GlyphType::tempo, "3");

    stamp (6, 3, 0, GlyphType::key, "12");
    stamp (6, 3, 4, GlyphType::chord, "4");
    stamp (6, 3, 8, GlyphType::key, "19");
    stamp (6, 3, 12, GlyphType::chord, "2");

    stamp (6, 4, 2, GlyphType::noise, "0.06");
    stamp (6, 4, 3, GlyphType::mul, "0.26");
    stamp (6, 4, 4, GlyphType::audio, "0.18");
    stamp (6, 4, 6, GlyphType::clap, "0.64");
    stamp (6, 4, 12, GlyphType::noise, "0.07");
    stamp (6, 4, 13, GlyphType::mul, "0.28");
    stamp (6, 4, 14, GlyphType::audio, "0.18");
    stamp (6, 4, 15, GlyphType::hat, "0.78");

    stamp (6, 5, 5, GlyphType::hue, "0.34");
    stamp (6, 5, 9, GlyphType::visual, "0.58");
    stamp (6, 5, 13, GlyphType::hue, "0.92");
    stamp (6, 7, 1, GlyphType::wormhole, "3");

    stamp (6, 6, 4, GlyphType::repeat, "2");
    stamp (6, 6, 8, GlyphType::ratchet, "3");
    stamp (6, 6, 12, GlyphType::tempo, "2");
    stamp (6, 6, 15, GlyphType::length, "2");

    // Layer 8: finale lift and release
    stamp (7, 0, 0, GlyphType::tempo, "0.5");
    stamp (7, 0, 4, GlyphType::key, "24");
    stamp (7, 0, 8, GlyphType::chord, "5");
    stamp (7, 0, 12, GlyphType::repeat, "4");

    stamp (7, 1, 3, GlyphType::voice, "105");
    stamp (7, 1, 4, GlyphType::audio, "0.2");
    stamp (7, 1, 11, GlyphType::voice, "108");
    stamp (7, 1, 12, GlyphType::audio, "0.18");

    stamp (7, 2, 2, GlyphType::ratchet, "6");
    stamp (7, 2, 6, GlyphType::tempo, "2");
    stamp (7, 2, 10, GlyphType::length, "4");
    stamp (7, 2, 14, GlyphType::repeat, "2");
    stamp (7, 2, 12, GlyphType::wormhole, "7");
    stamp (7, 2, 4, GlyphType::ratchet, "8");

    stamp (0, 6, 9, GlyphType::wormhole, "8");
    stamp (3, 6, 14, GlyphType::wormhole, "8");
    stamp (2, 5, 0, GlyphType::wormhole, "9");
    stamp (6, 5, 15, GlyphType::wormhole, "9");
    stamp (1, 6, 1, GlyphType::wormhole, "10");
    stamp (7, 6, 13, GlyphType::wormhole, "10");

    stamp (7, 3, 1, GlyphType::key, "-12");
    stamp (7, 3, 5, GlyphType::chord, "3");
    stamp (7, 3, 9, GlyphType::key, "12");
    stamp (7, 3, 13, GlyphType::chord, "1");

    stamp (7, 4, 0, GlyphType::noise, "0.025");
    stamp (7, 4, 1, GlyphType::audio, "0.16");
    stamp (7, 4, 4, GlyphType::kick, "0.72");
    stamp (7, 4, 8, GlyphType::noise, "0.022");
    stamp (7, 4, 9, GlyphType::audio, "0.14");
    stamp (7, 4, 12, GlyphType::snare, "0.68");
    stamp (7, 4, 15, GlyphType::mul, "0.46");

    stamp (7, 5, 2, GlyphType::visual, "0.66");
    stamp (7, 5, 6, GlyphType::hue, "0.08");
    stamp (7, 5, 10, GlyphType::visual, "0.78");
    stamp (7, 5, 14, GlyphType::hue, "0.56");
    stamp (7, 7, 14, GlyphType::wormhole, "3");

    stamp (7, 6, 3, GlyphType::tempo, "1.5");
    stamp (7, 6, 7, GlyphType::ratchet, "4");
    stamp (7, 6, 11, GlyphType::repeat, "3");
    stamp (7, 6, 15, GlyphType::length, "3");

    if (presetIndex == 1)
    {
        bpm = 122.0;
        stamp (0, 0, 9, GlyphType::tone, "41");
        stamp (0, 1, 6, GlyphType::voice, "64");
        stamp (0, 4, 13, GlyphType::hat, "0.72");
        stamp (4, 2, 9, GlyphType::repeat, "2");
    }
    else if (presetIndex == 2)
    {
        bpm = 148.0;
        stamp (0, 2, 11, GlyphType::voice, "79");
        stamp (0, 5, 5, GlyphType::ratchet, "5");
        stamp (3, 1, 6, GlyphType::accent, "1.24");
        stamp (7, 0, 6, GlyphType::wormhole, "4");
        stamp (7, 5, 13, GlyphType::wormhole, "4");
    }

    selectedCell = getCell (0, 0, 1);
}

void MainComponent::publishPatchSnapshot()
{
    auto snapshot = std::make_shared<PatchSnapshot>();
    snapshot->bpm = bpm;

    for (int i = 0; i < cols * rows * layers; ++i)
        snapshot->cells[(size_t) i] = grid.getReference (i);

    std::atomic_store (&currentPatchSnapshot, std::shared_ptr<const PatchSnapshot> (std::move (snapshot)));
}

std::shared_ptr<const MainComponent::PatchSnapshot> MainComponent::getPatchSnapshot() const noexcept
{
    return std::atomic_load (&currentPatchSnapshot);
}

MainComponent::TransportSnapshot MainComponent::getTransportSnapshot() const noexcept
{
    TransportSnapshot snapshot;

    for (;;)
    {
        const auto before = transportSequence.load (std::memory_order_acquire);

        if ((before & 1u) != 0u)
            continue;

        snapshot = publishedTransportSnapshot;

        const auto after = transportSequence.load (std::memory_order_acquire);

        if (before == after)
            return snapshot;
    }
}

void MainComponent::applyPendingTransportCommands() noexcept
{
    if (pendingSnakeReset.exchange (false, std::memory_order_acq_rel))
    {
        if (currentMode == AppMode::cellularGrid)
        {
            if (auto patchSnapshot = getPatchSnapshot())
                resetAutomata (*patchSnapshot);
            else
            {
                automataCurrent.fill (0);
                automataPrevious.fill (0);
            }
            audioSnakeCount = 0;
        }
        else
        {
            resetAudioSnakes();
        }
    }

    auto spawnCount = pendingSpawnRequests.exchange (0, std::memory_order_acq_rel);

    while (spawnCount-- > 0)
    {
        if (currentMode == AppMode::cellularGrid)
            seedAutomataBurst();
        else
            spawnAudioSnake();
    }
}

void MainComponent::applyMode (AppMode mode, ModeVariant variant, bool showTitleAfter)
{
    currentMode = mode;
    currentVariant = variant;
    showingTitleScreen = showTitleAfter;
    if (! showTitleAfter)
        hasResumableSession = true;
    isRunning.store (false);
    playButton.setButtonText ("Play");
    spawnSnakeButton.setButtonText (mode == AppMode::cellularGrid ? "Seed Life" : "Spawn Snake");
    mixerVisible = false;
    currentTimeSeconds.store (0.0);
    lastAdvancedTick.store (-1);
    previewSizeMode = PanelSizeMode::defaultSize;
    inspectorSizeMode = PanelSizeMode::defaultSize;
    visibleLayer = 0;
    initialiseGrid (mode);
    caRuleButton.setButtonText ("Rule: " + cellularRuleName (currentCellularRule));
    presetButton.setButtonText ("Preset " + juce::String (modePresetIndices[(size_t) modeIndex (mode)] + 1));
    caBirthButton.setButtonText (cellularRangeLabel ("Birth", currentCellularBirthRange));
    caSurviveButton.setButtonText (cellularRangeLabel ("Stay", currentCellularSurviveRange));
    updateTempoControl();
    initialiseSnakes();
    resetSynthVoices();
    resetDrumVoices();
    clearPluginProcessingState();
    publishPatchSnapshot();
    renderInspector();
    updateCellButtons();
    updateMixerButtons();
    resized();
    repaint();
}

void MainComponent::createUi()
{
    addAndMakeVisible (sidebar);
    addAndMakeVisible (mainArea);
    addAndMakeVisible (previewArea);
    addAndMakeVisible (inspector);
    addAndMakeVisible (mixerArea);
    addAndMakeVisible (stage);
    sidebar.addAndMakeVisible (toolViewport);
    sidebar.addAndMakeVisible (sidebarToggleButton);
    toolViewport.setViewedComponent (&toolListContent, false);
    toolViewport.setScrollBarsShown (true, false);
    toolViewport.setScrollBarThickness (10);

    for (auto* label : { &bpmValue, &beatValue, &toolValue, &audioValue })
    {
        label->setColour (juce::Label::textColourId, juce::Colours::white);
        label->setColour (juce::Label::backgroundColourId, juce::Colours::transparentBlack);
        label->setFont (juce::FontOptions (13.0f, juce::Font::plain));
        addAndMakeVisible (*label);
    }

    bpmValue.setText ("BPM 104", juce::dontSendNotification);
    beatValue.setText ("", juce::dontSendNotification);
    toolValue.setText ("", juce::dontSendNotification);
    audioValue.setText ("", juce::dontSendNotification);
    tempoControlLabel.setText ("Tempo", juce::dontSendNotification);
    tempoControlLabel.setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.92f));
    tempoControlLabel.setJustificationType (juce::Justification::centredLeft);
    tempoControlLabel.setFont (juce::FontOptions (13.0f, juce::Font::bold));
    mainArea.addAndMakeVisible (tempoControlLabel);
    tempoSlider.setSliderStyle (juce::Slider::LinearHorizontal);
    tempoSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 64, 24);
    tempoSlider.setRange (5.0, 220.0, 1.0);
    tempoSlider.onValueChange = [this]
    {
        bpm = tempoSlider.getValue();
        publishPatchSnapshot();
        updateTempoControl();
    };
    mainArea.addAndMakeVisible (tempoSlider);
    updateTempoControl();
    playButton.addListener (this);
    clearButton.addListener (this);
    spawnSnakeButton.addListener (this);
    presetButton.addListener (this);
    caRuleButton.addListener (this);
    caBirthButton.addListener (this);
    caSurviveButton.addListener (this);
    mixerToggleButton.addListener (this);
    saveButton.addListener (this);
    loadButton.addListener (this);
    layerDownButton.addListener (this);
    layerUpButton.addListener (this);
    sidebarToggleButton.addListener (this);
    resumeModeButton.addListener (this);
    rescanPluginsButton.addListener (this);
    a1ModeButton.addListener (this);
    a2ModeButton.addListener (this);
    a3ModeButton.addListener (this);
    a4ModeButton.addListener (this);
    b1ModeButton.addListener (this);
    b2ModeButton.addListener (this);
    b3ModeButton.addListener (this);
    b4ModeButton.addListener (this);
    eraseButton.addListener (this);

    addAndMakeVisible (playButton);
    addAndMakeVisible (clearButton);
    addAndMakeVisible (spawnSnakeButton);
    addAndMakeVisible (presetButton);
    addAndMakeVisible (caRuleButton);
    addAndMakeVisible (caBirthButton);
    addAndMakeVisible (caSurviveButton);
    addAndMakeVisible (mixerToggleButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (layerDownButton);
    addAndMakeVisible (layerUpButton);
    addAndMakeVisible (eraseButton);
    addAndMakeVisible (resumeModeButton);
    addAndMakeVisible (rescanPluginsButton);
    addAndMakeVisible (a1ModeButton);
    addAndMakeVisible (a2ModeButton);
    addAndMakeVisible (a3ModeButton);
    addAndMakeVisible (a4ModeButton);
    addAndMakeVisible (b1ModeButton);
    addAndMakeVisible (b2ModeButton);
    addAndMakeVisible (b3ModeButton);
    addAndMakeVisible (b4ModeButton);

    for (int strip = 0; strip < (int) mixerStrips.size(); ++strip)
    {
        mixerStrips[(size_t) strip].name = strip == 0 ? "Bass" : (strip == 1 ? "Mid" : "Upper");

        mixerArea.addAndMakeVisible (mixerStripLabels[(size_t) strip]);
        mixerStripLabels[(size_t) strip].setText (mixerStrips[(size_t) strip].name, juce::dontSendNotification);
        mixerStripLabels[(size_t) strip].setColour (juce::Label::textColourId, juce::Colours::white);
        mixerStripLabels[(size_t) strip].setJustificationType (juce::Justification::centred);
        mixerStripLabels[(size_t) strip].setFont (juce::FontOptions (15.0f, juce::Font::bold));

        auto& midiFxButton = mixerMidiFxButtons[(size_t) strip];
        auto& midiFxGuiButton = mixerMidiFxGuiButtons[(size_t) strip];
        auto& instrumentButton = mixerInstrumentButtons[(size_t) strip];
        auto& instrumentGuiButton = mixerInstrumentGuiButtons[(size_t) strip];
        auto& effectAButton = mixerEffectAButtons[(size_t) strip];
        auto& effectAGuiButton = mixerEffectAGuiButtons[(size_t) strip];
        auto& effectBButton = mixerEffectBButtons[(size_t) strip];
        auto& effectBGuiButton = mixerEffectBGuiButtons[(size_t) strip];
        auto& effectCButton = mixerEffectCButtons[(size_t) strip];
        auto& effectCGuiButton = mixerEffectCGuiButtons[(size_t) strip];
        auto& effectDButton = mixerEffectDButtons[(size_t) strip];
        auto& effectDGuiButton = mixerEffectDGuiButtons[(size_t) strip];
        auto& midiFxBypassButton = mixerMidiFxBypassButtons[(size_t) strip];
        auto& instrumentBypassButton = mixerInstrumentBypassButtons[(size_t) strip];
        auto& effectABypassButton = mixerEffectABypassButtons[(size_t) strip];
        auto& effectBBypassButton = mixerEffectBBypassButtons[(size_t) strip];
        auto& effectCBypassButton = mixerEffectCBypassButtons[(size_t) strip];
        auto& effectDBypassButton = mixerEffectDBypassButtons[(size_t) strip];
        auto& midiFxRemoveButton = mixerMidiFxRemoveButtons[(size_t) strip];
        auto& instrumentRemoveButton = mixerInstrumentRemoveButtons[(size_t) strip];
        auto& effectARemoveButton = mixerEffectARemoveButtons[(size_t) strip];
        auto& effectBRemoveButton = mixerEffectBRemoveButtons[(size_t) strip];
        auto& effectCRemoveButton = mixerEffectCRemoveButtons[(size_t) strip];
        auto& effectDRemoveButton = mixerEffectDRemoveButtons[(size_t) strip];
        auto& effectAUpButton = mixerEffectAUpButtons[(size_t) strip];
        auto& effectBUpButton = mixerEffectBUpButtons[(size_t) strip];
        auto& effectCUpButton = mixerEffectCUpButtons[(size_t) strip];
        auto& effectDUpButton = mixerEffectDUpButtons[(size_t) strip];
        auto& effectADownButton = mixerEffectADownButtons[(size_t) strip];
        auto& effectBDownButton = mixerEffectBDownButtons[(size_t) strip];
        auto& effectCDownButton = mixerEffectCDownButtons[(size_t) strip];
        auto& effectDDownButton = mixerEffectDDownButtons[(size_t) strip];
        midiFxButton.setButtonText ("Load MIDI FX");
        midiFxGuiButton.setButtonText ("UI");
        instrumentButton.setButtonText ("Load Instrument");
        instrumentGuiButton.setButtonText ("UI");
        effectAButton.setButtonText ("Load FX A");
        effectAGuiButton.setButtonText ("UI");
        effectBButton.setButtonText ("Load FX B");
        effectBGuiButton.setButtonText ("UI");
        effectCButton.setButtonText ("Load FX C");
        effectCGuiButton.setButtonText ("UI");
        effectDButton.setButtonText ("Load FX D");
        effectDGuiButton.setButtonText ("UI");
        for (auto* bypassButton : { &midiFxBypassButton, &instrumentBypassButton, &effectABypassButton, &effectBBypassButton, &effectCBypassButton, &effectDBypassButton })
            bypassButton->setButtonText ("B");
        for (auto* removeButton : { &midiFxRemoveButton, &instrumentRemoveButton, &effectARemoveButton, &effectBRemoveButton, &effectCRemoveButton, &effectDRemoveButton })
            removeButton->setButtonText ("X");
        for (auto* upButton : { &effectAUpButton, &effectBUpButton, &effectCUpButton, &effectDUpButton })
            upButton->setButtonText ("^");
        for (auto* downButton : { &effectADownButton, &effectBDownButton, &effectCDownButton, &effectDDownButton })
            downButton->setButtonText ("v");
        midiFxButton.setComponentID ("strip" + juce::String (strip) + "_midiFx");
        midiFxGuiButton.setComponentID ("strip" + juce::String (strip) + "_midiFxGui");
        instrumentButton.setComponentID ("strip" + juce::String (strip) + "_instrument");
        instrumentGuiButton.setComponentID ("strip" + juce::String (strip) + "_instrumentGui");
        effectAButton.setComponentID ("strip" + juce::String (strip) + "_fxA");
        effectAGuiButton.setComponentID ("strip" + juce::String (strip) + "_fxAGui");
        effectBButton.setComponentID ("strip" + juce::String (strip) + "_fxB");
        effectBGuiButton.setComponentID ("strip" + juce::String (strip) + "_fxBGui");
        effectCButton.setComponentID ("strip" + juce::String (strip) + "_fxC");
        effectCGuiButton.setComponentID ("strip" + juce::String (strip) + "_fxCGui");
        effectDButton.setComponentID ("strip" + juce::String (strip) + "_fxD");
        effectDGuiButton.setComponentID ("strip" + juce::String (strip) + "_fxDGui");
        midiFxButton.addListener (this);
        midiFxGuiButton.addListener (this);
        instrumentButton.addListener (this);
        instrumentGuiButton.addListener (this);
        effectAButton.addListener (this);
        effectAGuiButton.addListener (this);
        effectBButton.addListener (this);
        effectBGuiButton.addListener (this);
        effectCButton.addListener (this);
        effectCGuiButton.addListener (this);
        effectDButton.addListener (this);
        effectDGuiButton.addListener (this);
        for (auto* extraButton : { &midiFxBypassButton, &instrumentBypassButton, &effectABypassButton, &effectBBypassButton, &effectCBypassButton, &effectDBypassButton,
                                   &midiFxRemoveButton, &instrumentRemoveButton, &effectARemoveButton, &effectBRemoveButton, &effectCRemoveButton, &effectDRemoveButton,
                                   &effectAUpButton, &effectBUpButton, &effectCUpButton, &effectDUpButton,
                                   &effectADownButton, &effectBDownButton, &effectCDownButton, &effectDDownButton })
            extraButton->addListener (this);

        for (auto pair : { std::pair<juce::TextButton*, PluginSlotKind> { &midiFxButton, PluginSlotKind::midiFx },
                           { &instrumentButton, PluginSlotKind::instrument },
                           { &effectAButton, PluginSlotKind::effectA },
                           { &effectBButton, PluginSlotKind::effectB },
                           { &effectCButton, PluginSlotKind::effectC },
                           { &effectDButton, PluginSlotKind::effectD } })
        {
            pair.first->setColour (juce::TextButton::buttonColourId, juce::Colour::fromFloatRGBA (0.08f, 0.10f, 0.11f, 0.92f));
            pair.first->setColour (juce::TextButton::buttonOnColourId, mixerSlotAccentColour (pair.second).withAlpha (0.48f));
            pair.first->setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.96f));
            pair.first->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        }
        instrumentGuiButton.setColour (juce::TextButton::buttonColourId, juce::Colour::fromFloatRGBA (0.10f, 0.14f, 0.16f, 0.92f));
        instrumentGuiButton.setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromFloatRGBA (0.26f, 0.96f, 1.0f, 0.48f));
        instrumentGuiButton.setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.96f));
        instrumentGuiButton.setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        for (auto* guiButton : { &midiFxGuiButton, &effectAGuiButton, &effectBGuiButton, &effectCGuiButton, &effectDGuiButton })
        {
            guiButton->setColour (juce::TextButton::buttonColourId, juce::Colour::fromFloatRGBA (0.10f, 0.14f, 0.16f, 0.92f));
            guiButton->setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromFloatRGBA (0.9f, 0.9f, 0.9f, 0.28f));
            guiButton->setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.96f));
            guiButton->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        }
        for (auto* utilityButton : { &midiFxBypassButton, &instrumentBypassButton, &effectABypassButton, &effectBBypassButton, &effectCBypassButton, &effectDBypassButton,
                                     &midiFxRemoveButton, &instrumentRemoveButton, &effectARemoveButton, &effectBRemoveButton, &effectCRemoveButton, &effectDRemoveButton,
                                     &effectAUpButton, &effectBUpButton, &effectCUpButton, &effectDUpButton,
                                     &effectADownButton, &effectBDownButton, &effectCDownButton, &effectDDownButton })
        {
            utilityButton->setColour (juce::TextButton::buttonColourId, juce::Colour::fromFloatRGBA (0.08f, 0.10f, 0.11f, 0.92f));
            utilityButton->setColour (juce::TextButton::buttonOnColourId, juce::Colour::fromFloatRGBA (0.95f, 0.95f, 0.95f, 0.22f));
            utilityButton->setColour (juce::TextButton::textColourOffId, juce::Colours::white.withAlpha (0.92f));
            utilityButton->setColour (juce::TextButton::textColourOnId, juce::Colours::white);
        }

        mixerArea.addAndMakeVisible (midiFxButton);
        mixerArea.addAndMakeVisible (midiFxGuiButton);
        mixerArea.addAndMakeVisible (instrumentButton);
        mixerArea.addAndMakeVisible (instrumentGuiButton);
        mixerArea.addAndMakeVisible (effectAButton);
        mixerArea.addAndMakeVisible (effectAGuiButton);
        mixerArea.addAndMakeVisible (effectBButton);
        mixerArea.addAndMakeVisible (effectBGuiButton);
        mixerArea.addAndMakeVisible (effectCButton);
        mixerArea.addAndMakeVisible (effectCGuiButton);
        mixerArea.addAndMakeVisible (effectDButton);
        mixerArea.addAndMakeVisible (effectDGuiButton);
        for (auto* extraButton : { &midiFxBypassButton, &instrumentBypassButton, &effectABypassButton, &effectBBypassButton, &effectCBypassButton, &effectDBypassButton,
                                   &midiFxRemoveButton, &instrumentRemoveButton, &effectARemoveButton, &effectBRemoveButton, &effectCRemoveButton, &effectDRemoveButton,
                                   &effectAUpButton, &effectBUpButton, &effectCUpButton, &effectDUpButton,
                                   &effectADownButton, &effectBDownButton, &effectCDownButton, &effectDDownButton })
            mixerArea.addAndMakeVisible (extraButton);

        auto& volumeSlider = mixerVolumeSliders[(size_t) strip];
        auto& panSlider = mixerPanSliders[(size_t) strip];
        const auto stripAccent = strip == 0 ? juce::Colour::fromFloatRGBA (0.92f, 0.86f, 0.22f, 1.0f)
                                            : (strip == 1 ? juce::Colour::fromFloatRGBA (0.18f, 0.96f, 1.0f, 1.0f)
                                                          : juce::Colour::fromFloatRGBA (1.0f, 0.34f, 0.84f, 1.0f));
        volumeSlider.setSliderStyle (juce::Slider::LinearVertical);
        volumeSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 18);
        volumeSlider.setRange (0.0, 10.0, 0.01);
        volumeSlider.setValue (mixerStrips[(size_t) strip].volume);
        volumeSlider.setAccentColour (stripAccent);
        volumeSlider.setColour (juce::Slider::textBoxTextColourId, juce::Colours::white.withAlpha (0.92f));
        volumeSlider.setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
        volumeSlider.setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour::fromFloatRGBA (0.08f, 0.11f, 0.14f, 0.92f));
        volumeSlider.onValueChange = [this, strip] { mixerStrips[(size_t) strip].volume = (float) mixerVolumeSliders[(size_t) strip].getValue(); };
        panSlider.setSliderStyle (juce::Slider::LinearHorizontal);
        panSlider.setTextBoxStyle (juce::Slider::TextBoxBelow, false, 52, 18);
        panSlider.setRange (-1.0, 1.0, 0.01);
        panSlider.setValue (mixerStrips[(size_t) strip].pan);
        panSlider.onValueChange = [this, strip] { mixerStrips[(size_t) strip].pan = (float) mixerPanSliders[(size_t) strip].getValue(); };
        mixerArea.addAndMakeVisible (volumeSlider);
        mixerArea.addAndMakeVisible (panSlider);

        mixerArea.addAndMakeVisible (mixerVolumeLabels[(size_t) strip]);
        mixerArea.addAndMakeVisible (mixerPanLabels[(size_t) strip]);
        mixerVolumeLabels[(size_t) strip].setText ("Vol", juce::dontSendNotification);
        mixerPanLabels[(size_t) strip].setText ("Pan", juce::dontSendNotification);
        mixerVolumeLabels[(size_t) strip].setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.84f));
        mixerPanLabels[(size_t) strip].setColour (juce::Label::textColourId, juce::Colours::white.withAlpha (0.84f));
        mixerVolumeLabels[(size_t) strip].setJustificationType (juce::Justification::centred);
        mixerPanLabels[(size_t) strip].setJustificationType (juce::Justification::centred);
    }

    stage.setHarmonySpaceCallbacks (
        [this] (int row, int col)
        {
            if (currentMode != AppMode::harmonySpace)
                return;

            auto* cell = getCell (visibleLayer, row, col);
            if (cell == nullptr)
                return;

            applySelectedToolToCell (*cell);

            const auto pitchClass = [&]
            {
                auto value = (col * 4 + row * 3) % 12;
                if (value < 0)
                    value += 12;
                return value;
            }();
            auto constrainedPitch = pitchClass;
            if (harmonySpaceConstraintMode == 1)
            {
                static constexpr int majorSet[] = { 0, 2, 4, 5, 7, 9, 11 };
                auto best = majorSet[0];
                auto bestDist = 99;
                for (auto degree : majorSet)
                {
                    const auto pc = (harmonySpaceKeyCenter + degree) % 12;
                    const auto dist = juce::jmin (std::abs (pc - pitchClass), 12 - std::abs (pc - pitchClass));
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        best = pc;
                    }
                }
                constrainedPitch = best;
            }
            else if (harmonySpaceConstraintMode == 2)
            {
                static constexpr int triadSet[] = { 0, 4, 7 };
                auto best = triadSet[0];
                auto bestDist = 99;
                for (auto degree : triadSet)
                {
                    const auto pc = (harmonySpaceKeyCenter + degree) % 12;
                    const auto dist = juce::jmin (std::abs (pc - pitchClass), 12 - std::abs (pc - pitchClass));
                    if (dist < bestDist)
                    {
                        bestDist = dist;
                        best = pc;
                    }
                }
                constrainedPitch = best;
            }

            const auto midiNote = 36 + visibleLayer * 4 + col * 4 + row * 3 + (constrainedPitch - pitchClass);

            switch (selectedTool)
            {
                case GlyphType::tone:
                case GlyphType::voice:
                    cell->code = juce::String (midiNote);
                    break;
                case GlyphType::chord:
                    cell->code = juce::String ((row + col + visibleLayer) % 7);
                    break;
                case GlyphType::key:
                    cell->code = juce::String (constrainedPitch);
                    break;
                case GlyphType::octave:
                    cell->code = juce::String (juce::jlimit (-2, 2, visibleLayer / 2 - 1 + row / 3));
                    break;
                case GlyphType::accent:
                    cell->code = juce::String (1.0 + ((row + col) % 5) * 0.06, 2);
                    break;
                case GlyphType::decay:
                    cell->code = juce::String (1.15 + row * 0.12 + visibleLayer * 0.05, 2);
                    break;
                case GlyphType::repeat:
                    cell->code = juce::String (1 + ((row + col) % 3));
                    break;
                case GlyphType::tempo:
                    cell->code = juce::String (0.75 + ((row + col) % 4) * 0.25, 2);
                    break;
                case GlyphType::wormhole:
                    cell->code = juce::String (1 + ((row + col + visibleLayer) % 6));
                    break;
                case GlyphType::audio:
                    cell->code = juce::String (0.18 + (7 - row) * 0.04, 2);
                    break;
                case GlyphType::visual:
                    cell->code = juce::String (0.28 + col * 0.03, 2);
                    break;
                case GlyphType::hue:
                    cell->code = juce::String (std::fmod (constrainedPitch / 12.0 + visibleLayer * 0.08, 1.0), 2);
                    break;
                default:
                    break;
            }

            if (harmonySpaceGestureRecordEnabled)
            {
                harmonySpaceGesturePoints.add ({ row, col });
                while (harmonySpaceGesturePoints.size() > 16)
                    harmonySpaceGesturePoints.remove (0);
            }

            recompileCell (*cell);
            selectCell (cell);
            publishPatchSnapshot();
            updateCellButtons();
            repaint();
        },
        [this] (int layer)
        {
            visibleLayer = juce::jlimit (0, layers - 1, layer);
            if (selectedCell != nullptr)
                selectedCell = getCell (visibleLayer, selectedCell->row, selectedCell->col);
            updateCellButtons();
            repaint();
        },
        [this] (int controlIndex)
        {
            if (currentMode != AppMode::harmonySpace)
                return;

            if (controlIndex >= 100)
            {
                harmonySpaceKeyCenter = (controlIndex - 100) % 12;
                if (selectedCell != nullptr)
                {
                    selectedTool = GlyphType::key;
                    auto pitch = harmonySpaceKeyCenter;
                    selectedCell->type = selectedTool;
                    selectedCell->label = getGlyphDefinition (selectedTool).label;
                    selectedCell->code = juce::String (pitch);
                    recompileCell (*selectedCell);
                    publishPatchSnapshot();
                    renderInspector();
                    updateCellButtons();
                    repaint();
                }
                return;
            }

            switch (controlIndex)
            {
                case 0: harmonySpaceKeyCenter = (harmonySpaceKeyCenter + 11) % 12; break;
                case 1: harmonySpaceKeyCenter = (harmonySpaceKeyCenter + 1) % 12; break;
                case 2: harmonySpaceConstraintMode = (harmonySpaceConstraintMode + 1) % 3; break;
                case 3: harmonySpaceGestureRecordEnabled = ! harmonySpaceGestureRecordEnabled; break;
                case 4: selectedTool = GlyphType::key; break;
                case 5: selectedTool = GlyphType::chord; break;
                case 6: selectedTool = GlyphType::voice; break;
                case 7:
                    harmonySpaceGesturePoints.clear();
                    selectedTool = GlyphType::wormhole;
                    break;
                default: break;
            }

            if (selectedCell != nullptr)
                renderInspector();
            publishPatchSnapshot();
            updateCellButtons();
            repaint();
        });

    labelEditor.addListener (this);
    codeEditor.addListener (this);
    codeEditor.setMultiLine (true);
    codeEditor.setReturnKeyStartsNewLine (true);
    parameterPickerButton.addListener (this);
    addAndMakeVisible (labelEditor);
    addAndMakeVisible (codeEditor);
    addAndMakeVisible (parameterPickerButton);

    const auto addTool = [this] (GlyphType type)
    {
        auto def = getGlyphDefinition (type);
        auto* button = toolButtons.add (new SidebarButton (def.shortName + " " + def.label, def.colour));
        button->setComponentID (glyphTypeToString (type));
        button->addListener (this);
        toolListContent.addAndMakeVisible (button);
    };

    for (auto type : { GlyphType::pulse, GlyphType::tone, GlyphType::voice, GlyphType::chord, GlyphType::key, GlyphType::octave,
                       GlyphType::route, GlyphType::channel, GlyphType::cc, GlyphType::parameter,
                       GlyphType::tempo, GlyphType::ratchet, GlyphType::repeat, GlyphType::wormhole, GlyphType::kick, GlyphType::snare,
                       GlyphType::hat, GlyphType::clap, GlyphType::length, GlyphType::accent, GlyphType::decay, GlyphType::noise, GlyphType::mul,
                       GlyphType::bias, GlyphType::hue, GlyphType::audio, GlyphType::visual })
        addTool (type);

    for (int row = 0; row < rows; ++row)
        for (int col = 0; col < cols; ++col)
    {
        auto* button = cellButtons.add (new CellButton (row, col));
        button->onClick = [this, row, col]
        {
            if (auto* cell = getCell (visibleLayer, row, col))
            {
                applySelectedToolToCell (*cell);
                selectCell (cell);
            }
        };
        addAndMakeVisible (button);
    }

    renderInspector();
    updateCellButtons();
    updateMixerButtons();
}

void MainComponent::updateTempoControl()
{
    tempoSlider.setTextValueSuffix (" BPM");

    if (std::abs (tempoSlider.getValue() - bpm) > 0.5)
        tempoSlider.setValue (bpm, juce::dontSendNotification);

    bpmValue.setText ("", juce::dontSendNotification);
}

void MainComponent::paint (juce::Graphics& g)
{
    const auto isHarmonicMode = currentMode == AppMode::harmonicGeometry;
    const auto isCellularMode = currentMode == AppMode::cellularGrid;
    const auto isHarmonySpaceMode = currentMode == AppMode::harmonySpace;
    const auto showingMixer = mixerVisible && ! showingTitleScreen;

    if (showingTitleScreen)
    {
        g.setColour (currentMode == AppMode::harmonicGeometry
                         ? juce::Colour::fromFloatRGBA (0.05f, 0.04f, 0.10f, 1.0f)
                         : (currentMode == AppMode::cellularGrid ? juce::Colour::fromFloatRGBA (0.03f, 0.02f, 0.08f, 1.0f)
                         : (currentMode == AppMode::harmonySpace ? juce::Colour::fromFloatRGBA (0.04f, 0.03f, 0.01f, 1.0f)
                                                                 : juce::Colour::fromFloatRGBA (0.01f, 0.03f, 0.02f, 1.0f))));
        g.fillAll();

        auto overlay = getLocalBounds().reduced (120, 90).toFloat();
        juce::ColourGradient titleGlow (isCellularMode ? juce::Colour::fromFloatRGBA (0.62f, 0.74f, 1.0f, 0.20f)
                                                       : juce::Colour::fromFloatRGBA (0.62f, 1.0f, 0.08f, 0.18f),
                                        overlay.getTopLeft(),
                                        isCellularMode ? juce::Colour::fromFloatRGBA (0.32f, 1.0f, 0.86f, 0.16f)
                                                       : juce::Colour::fromFloatRGBA (0.12f, 0.92f, 1.0f, 0.16f),
                                        overlay.getBottomRight(),
                                        false);
        titleGlow.addColour (0.55, isCellularMode ? juce::Colour::fromFloatRGBA (0.38f, 0.42f, 1.0f, 0.14f)
                                                  : juce::Colour::fromFloatRGBA (1.0f, 0.28f, 0.84f, 0.14f));
        g.setGradientFill (titleGlow);
        g.fillRoundedRectangle (overlay.expanded (8.0f), 30.0f);

        g.setColour (isCellularMode ? juce::Colour::fromFloatRGBA (0.04f, 0.05f, 0.11f, 1.0f)
                                    : juce::Colour::fromFloatRGBA (0.04f, 0.08f, 0.04f, 1.0f));
        g.fillRoundedRectangle (overlay, 28.0f);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawRoundedRectangle (overlay.reduced (0.5f), 28.0f, 1.2f);

        auto titleArea = overlay.reduced (42.0f, 34.0f).toNearestInt();
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (44.0f, juce::Font::bold));
        g.drawText ("Patchable Harmonic Worlds", titleArea.removeFromTop (58), juce::Justification::centred, false);
        g.setColour (juce::Colours::white.withAlpha (0.76f));
        g.setFont (juce::FontOptions (18.0f, juce::Font::plain));
        g.drawText (hasResumableSession ? "Resume or choose a mode" : "Choose a mode to enter the instrument",
                    titleArea.removeFromTop (28),
                    juce::Justification::centred,
                    false);
        return;
    }

    juce::ColourGradient gradient (
        isHarmonicMode ? juce::Colour::fromFloatRGBA (0.10f, 0.08f, 0.20f, 1.0f)
                       : (isCellularMode ? juce::Colour::fromFloatRGBA (0.06f, 0.05f, 0.18f, 1.0f)
                       : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.08f, 0.06f, 0.02f, 1.0f)
                                             : juce::Colour::fromFloatRGBA (0.04f, 0.12f, 0.03f, 1.0f))),
        0.0f, 0.0f,
        isHarmonicMode ? juce::Colour::fromFloatRGBA (0.28f, 0.14f, 0.10f, 1.0f)
                       : (isCellularMode ? juce::Colour::fromFloatRGBA (0.06f, 0.16f, 0.24f, 1.0f)
                       : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.05f, 0.18f, 0.18f, 1.0f)
                                             : juce::Colour::fromFloatRGBA (0.10f, 0.18f, 0.02f, 1.0f))),
        static_cast<float> (getWidth()), static_cast<float> (getHeight()),
        false);
    gradient.addColour (0.2, isHarmonicMode ? juce::Colour::fromFloatRGBA (0.36f, 0.48f, 1.0f, 0.24f)
                                            : (isCellularMode ? juce::Colour::fromFloatRGBA (0.72f, 0.82f, 1.0f, 0.22f)
                                            : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (1.0f, 0.78f, 0.18f, 0.20f)
                                                                  : juce::Colour::fromFloatRGBA (0.48f, 0.98f, 0.04f, 0.22f))));
    gradient.addColour (0.8, isHarmonicMode ? juce::Colour::fromFloatRGBA (1.0f, 0.62f, 0.24f, 0.20f)
                                            : (isCellularMode ? juce::Colour::fromFloatRGBA (0.18f, 1.0f, 0.88f, 0.18f)
                                            : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.12f, 0.92f, 0.88f, 0.18f)
                                                                  : juce::Colour::fromFloatRGBA (0.05f, 0.88f, 0.92f, 0.20f))));
    g.setGradientFill (gradient);
    g.fillAll();

    g.setColour (isHarmonicMode ? juce::Colour::fromFloatRGBA (0.07f, 0.06f, 0.12f, 0.97f)
                                : (isCellularMode ? juce::Colour::fromFloatRGBA (0.04f, 0.04f, 0.10f, 0.97f)
                                : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.05f, 0.04f, 0.01f, 0.97f)
                                                      : makeSidebarColour())));
    g.fillRect (sidebar.getBounds());

    g.setColour (isHarmonicMode ? juce::Colour::fromFloatRGBA (0.11f, 0.08f, 0.16f, 0.94f)
                                : (isCellularMode ? juce::Colour::fromFloatRGBA (0.08f, 0.08f, 0.16f, 0.94f)
                                : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.09f, 0.08f, 0.03f, 0.94f)
                                                      : makePanelColour())));
    g.fillRoundedRectangle (mainArea.getBounds().toFloat(), 18.0f);
    if (showingMixer)
        g.fillRoundedRectangle (mixerArea.getBounds().toFloat(), 18.0f);
    else
    {
        g.fillRoundedRectangle (previewArea.getBounds().toFloat(), 18.0f);
        g.fillRoundedRectangle (inspector.getBounds().toFloat(), 18.0f);
    }

    auto footerBounds = getLocalBounds().removeFromBottom (56).toFloat().reduced (12.0f, 4.0f);
    juce::ColourGradient footerGlow (isHarmonicMode ? juce::Colour::fromFloatRGBA (0.42f, 0.62f, 1.0f, 0.18f)
                                                    : (isCellularMode ? juce::Colour::fromFloatRGBA (0.74f, 0.80f, 1.0f, 0.18f)
                                                    : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (1.0f, 0.80f, 0.18f, 0.18f)
                                                                          : juce::Colour::fromFloatRGBA (0.62f, 1.0f, 0.08f, 0.18f))),
                                     footerBounds.getX(), footerBounds.getCentreY(),
                                     isHarmonicMode ? juce::Colour::fromFloatRGBA (1.0f, 0.62f, 0.28f, 0.16f)
                                                    : (isCellularMode ? juce::Colour::fromFloatRGBA (0.18f, 1.0f, 0.88f, 0.16f)
                                                    : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.10f, 0.92f, 0.88f, 0.16f)
                                                                          : juce::Colour::fromFloatRGBA (0.10f, 0.96f, 1.0f, 0.16f))),
                                     footerBounds.getRight(), footerBounds.getCentreY(),
                                     false);
    footerGlow.addColour (0.5, isHarmonicMode ? juce::Colour::fromFloatRGBA (1.0f, 0.92f, 0.78f, 0.12f)
                                              : (isCellularMode ? juce::Colour::fromFloatRGBA (0.86f, 0.92f, 1.0f, 0.12f)
                                              : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (1.0f, 0.94f, 0.68f, 0.14f)
                                                                    : juce::Colour::fromFloatRGBA (1.0f, 0.22f, 0.82f, 0.14f))));
    g.setGradientFill (footerGlow);
    g.fillRoundedRectangle (footerBounds.expanded (4.0f, 1.0f), 18.0f);

    juce::ColourGradient footerFill (isHarmonicMode ? juce::Colour::fromFloatRGBA (0.11f, 0.09f, 0.17f, 0.94f)
                                                    : (isCellularMode ? juce::Colour::fromFloatRGBA (0.08f, 0.09f, 0.18f, 0.94f)
                                                    : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.10f, 0.09f, 0.04f, 0.94f)
                                                                          : juce::Colour::fromFloatRGBA (0.08f, 0.12f, 0.05f, 0.92f))),
                                     footerBounds.getTopLeft(),
                                     isHarmonicMode ? juce::Colour::fromFloatRGBA (0.10f, 0.08f, 0.14f, 0.96f)
                                                    : (isCellularMode ? juce::Colour::fromFloatRGBA (0.05f, 0.12f, 0.16f, 0.94f)
                                                    : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.04f, 0.08f, 0.08f, 0.94f)
                                                                          : juce::Colour::fromFloatRGBA (0.03f, 0.08f, 0.05f, 0.94f))),
                                     footerBounds.getBottomRight(),
                                     false);
    footerFill.addColour (0.35, isHarmonicMode ? juce::Colour::fromFloatRGBA (0.36f, 0.50f, 1.0f, 0.10f)
                                               : (isCellularMode ? juce::Colour::fromFloatRGBA (0.74f, 0.84f, 1.0f, 0.10f)
                                               : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (1.0f, 0.78f, 0.18f, 0.10f)
                                                                     : juce::Colour::fromFloatRGBA (0.48f, 0.98f, 0.04f, 0.08f))));
    footerFill.addColour (0.72, isHarmonicMode ? juce::Colour::fromFloatRGBA (1.0f, 0.54f, 0.24f, 0.10f)
                                               : (isCellularMode ? juce::Colour::fromFloatRGBA (0.22f, 1.0f, 0.88f, 0.08f)
                                               : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.10f, 0.92f, 0.88f, 0.08f)
                                                                     : juce::Colour::fromFloatRGBA (0.06f, 0.92f, 1.0f, 0.08f))));
    g.setGradientFill (footerFill);
    g.fillRoundedRectangle (footerBounds, 16.0f);

    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (footerBounds.reduced (0.5f), 16.0f, 1.0f);
    g.setColour (isHarmonicMode ? juce::Colour::fromFloatRGBA (1.0f, 0.86f, 0.72f, 0.18f)
                                : (isCellularMode ? juce::Colour::fromFloatRGBA (0.78f, 0.86f, 1.0f, 0.20f)
                                : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (1.0f, 0.84f, 0.24f, 0.20f)
                                                      : juce::Colour::fromFloatRGBA (0.72f, 1.0f, 0.18f, 0.20f))));
    g.drawLine (footerBounds.getX() + 16.0f, footerBounds.getY() + 5.0f, footerBounds.getRight() - 16.0f, footerBounds.getY() + 5.0f, 1.4f);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawLine ((float) sidebar.getRight(), 0.0f, (float) sidebar.getRight(), (float) getHeight(), 1.0f);

}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    auto sidebarArea = area.removeFromLeft (isSidebarExpanded ? 240 : 52);
    sidebar.setBounds (sidebarArea);
    const auto isHarmonySpaceMode = currentMode == AppMode::harmonySpace;

    if (showingTitleScreen)
    {
        sidebar.setVisible (false);
        mainArea.setVisible (false);
        previewArea.setVisible (false);
        inspector.setVisible (false);
        mixerArea.setVisible (false);
        stage.setVisible (false);
        toolViewport.setVisible (false);
        sidebarToggleButton.setVisible (false);
        tempoControlLabel.setVisible (false);
        tempoSlider.setVisible (false);

        for (auto* button : toolButtons)
            button->setVisible (false);

        for (auto* button : cellButtons)
            button->setVisible (false);

        for (auto* label : { &bpmValue, &beatValue, &toolValue, &audioValue })
            label->setVisible (false);

        for (auto* component : { static_cast<juce::Component*> (&playButton),
                                 static_cast<juce::Component*> (&clearButton),
                                 static_cast<juce::Component*> (&spawnSnakeButton),
                                 static_cast<juce::Component*> (&presetButton),
                                 static_cast<juce::Component*> (&caRuleButton),
                                 static_cast<juce::Component*> (&caBirthButton),
                                 static_cast<juce::Component*> (&caSurviveButton),
                                 static_cast<juce::Component*> (&mixerToggleButton),
                                 static_cast<juce::Component*> (&layerDownButton),
                                 static_cast<juce::Component*> (&layerUpButton),
                                 static_cast<juce::Component*> (&saveButton),
                                 static_cast<juce::Component*> (&loadButton),
                                 static_cast<juce::Component*> (&labelEditor),
                                 static_cast<juce::Component*> (&codeEditor),
                                 static_cast<juce::Component*> (&eraseButton) })
            component->setVisible (false);

        auto titleArea = getLocalBounds().reduced (240, 210);
        auto resumeRow = titleArea.removeFromBottom (hasResumableSession ? 86 : 0);
        auto row = titleArea.removeFromBottom (140);
        resumeModeButton.setVisible (hasResumableSession);
        a1ModeButton.setVisible (true);
        a2ModeButton.setVisible (true);
        a3ModeButton.setVisible (true);
        a4ModeButton.setVisible (true);
        b1ModeButton.setVisible (true);
        b2ModeButton.setVisible (true);
        b3ModeButton.setVisible (true);
        b4ModeButton.setVisible (true);
        rescanPluginsButton.setVisible (true);
        layoutMixer ({});
        if (hasResumableSession)
            resumeModeButton.setBounds (resumeRow.reduced (180, 12));
        auto rescanRow = titleArea.removeFromBottom (72);
        rescanPluginsButton.setBounds (rescanRow.withSizeKeepingCentre (220, 42));
        auto upperRow = row.removeFromTop (row.getHeight() / 2);
        auto lowerRow = row;
        auto quarterTop = upperRow.getWidth() / 4;
        auto quarterBottom = lowerRow.getWidth() / 4;
        a1ModeButton.setBounds (upperRow.removeFromLeft (quarterTop).reduced (16, 12));
        a2ModeButton.setBounds (upperRow.removeFromLeft (quarterTop).reduced (16, 12));
        a3ModeButton.setBounds (upperRow.removeFromLeft (quarterTop).reduced (16, 12));
        a4ModeButton.setBounds (upperRow.reduced (16, 12));
        b1ModeButton.setBounds (lowerRow.removeFromLeft (quarterBottom).reduced (16, 12));
        b2ModeButton.setBounds (lowerRow.removeFromLeft (quarterBottom).reduced (16, 12));
        b3ModeButton.setBounds (lowerRow.removeFromLeft (quarterBottom).reduced (16, 12));
        b4ModeButton.setBounds (lowerRow.reduced (16, 12));
        return;
    }

    sidebar.setVisible (true);
    mainArea.setVisible (true);
    sidebarToggleButton.setVisible (true);
    tempoControlLabel.setVisible (true);
    tempoSlider.setVisible (true);

    for (auto* button : cellButtons)
        button->setVisible (true);

    for (auto* label : { &bpmValue, &beatValue, &toolValue, &audioValue })
        label->setVisible (true);
    for (auto* component : { static_cast<juce::Component*> (&playButton),
                             static_cast<juce::Component*> (&clearButton),
                             static_cast<juce::Component*> (&spawnSnakeButton),
                             static_cast<juce::Component*> (&presetButton),
                             static_cast<juce::Component*> (&caRuleButton),
                             static_cast<juce::Component*> (&caBirthButton),
                             static_cast<juce::Component*> (&caSurviveButton),
                             static_cast<juce::Component*> (&mixerToggleButton),
                             static_cast<juce::Component*> (&layerDownButton),
                             static_cast<juce::Component*> (&layerUpButton),
                             static_cast<juce::Component*> (&saveButton),
                             static_cast<juce::Component*> (&loadButton) })
        component->setVisible (true);
    a1ModeButton.setVisible (false);
    resumeModeButton.setVisible (false);
    a2ModeButton.setVisible (false);
    a3ModeButton.setVisible (false);
    a4ModeButton.setVisible (false);
    b1ModeButton.setVisible (false);
    b2ModeButton.setVisible (false);
    b3ModeButton.setVisible (false);
    b4ModeButton.setVisible (false);
    rescanPluginsButton.setVisible (false);

    auto footer = area.removeFromBottom (56);
    const auto footerButtonBounds = [] (juce::Rectangle<int> area, int xPad) { return area.reduced (xPad, 10).translated (0, 2); };
    playButton.setBounds (footerButtonBounds (footer.removeFromLeft (120), 16));
    clearButton.setBounds (footerButtonBounds (footer.removeFromLeft (140), 6));
    spawnSnakeButton.setBounds (footerButtonBounds (footer.removeFromLeft (150), 6));
    presetButton.setVisible (true);
    presetButton.setBounds (footerButtonBounds (footer.removeFromLeft (98), 6));
    caRuleButton.setVisible (currentMode == AppMode::cellularGrid);
    caBirthButton.setVisible (currentMode == AppMode::cellularGrid);
    caSurviveButton.setVisible (currentMode == AppMode::cellularGrid);
    caRuleButton.setBounds ({});
    caBirthButton.setBounds ({});
    caSurviveButton.setBounds ({});
    mixerToggleButton.setVisible (true);
    if (true)
        mixerToggleButton.setBounds (footerButtonBounds (footer.removeFromLeft (98), 6));
    auto layerControls = footer.removeFromLeft (470);
    layerDownButton.setBounds (footerButtonBounds (layerControls.removeFromLeft (96), 6));
    layerUpButton.setBounds (footerButtonBounds (layerControls.removeFromLeft (96), 6));
    saveButton.setBounds (footerButtonBounds (layerControls.removeFromLeft (92), 6));
    loadButton.setBounds (footerButtonBounds (layerControls.removeFromLeft (92), 6));
    bpmValue.setBounds ({});
    beatValue.setBounds ({});
    toolValue.setBounds ({});
    audioValue.setBounds (footer.removeFromLeft (160));

    const auto defaultRightWidth = 390;
    const auto halfWidth = juce::roundToInt (getWidth() * 0.5f);
    const auto previewWidth = isHarmonySpaceMode ? 0 : (previewSizeMode == PanelSizeMode::hidden ? 0 : (previewSizeMode == PanelSizeMode::halfScreen ? halfWidth : defaultRightWidth));
    const auto inspectorWidth = isHarmonySpaceMode ? 0 : (inspectorSizeMode == PanelSizeMode::hidden ? 0 : (inspectorSizeMode == PanelSizeMode::halfScreen ? halfWidth : defaultRightWidth));
    const auto combinedRightWidth = juce::jlimit (0, area.getWidth() - 180, juce::jmax (previewWidth, inspectorWidth));
    auto rightArea = area.removeFromRight (combinedRightWidth);
    const auto fullRightArea = rightArea;

    const auto previewOnlyFocus = previewWidth > 0 && inspectorWidth == 0 && previewSizeMode == PanelSizeMode::halfScreen;
    auto previewBounds = previewWidth > 0
        ? (previewOnlyFocus ? rightArea.reduced (16) : rightArea.removeFromTop (406).reduced (16))
        : juce::Rectangle<int>();
    auto inspectorBounds = inspectorWidth > 0 ? rightArea.reduced (16) : juce::Rectangle<int>();
    const auto showingMixer = mixerVisible;
    previewArea.setVisible (! showingMixer && previewWidth > 0);
    inspector.setVisible (! showingMixer && inspectorWidth > 0);
    mixerArea.setVisible (showingMixer);
    previewArea.setBounds (previewBounds);
    inspector.setBounds (inspectorBounds);
    mixerArea.setBounds (showingMixer ? fullRightArea.reduced (16) : juce::Rectangle<int>());
    for (auto* component : { static_cast<juce::Component*> (&labelEditor),
                             static_cast<juce::Component*> (&parameterPickerButton),
                             static_cast<juce::Component*> (&codeEditor),
                             static_cast<juce::Component*> (&eraseButton) })
        component->setVisible (! showingMixer && inspectorWidth > 0);

    mainArea.setBounds (area.reduced (16));

    auto mainAreaLocal = mainArea.getLocalBounds().reduced (14);
    auto tempoRow = mainAreaLocal.removeFromTop (34);
    tempoControlLabel.setBounds (tempoRow.removeFromLeft (72));
    tempoSlider.setBounds (tempoRow.reduced (4, 2));
    if (currentMode == AppMode::cellularGrid)
    {
        mainAreaLocal.removeFromTop (6);
        auto caRow = mainAreaLocal.removeFromTop (36);
        auto caRowAbs = caRow.translated (mainArea.getX(), mainArea.getY());
        caRuleButton.setBounds (caRowAbs.removeFromLeft (180).reduced (4, 2));
        caBirthButton.setBounds (caRowAbs.removeFromLeft (150).reduced (4, 2));
        caSurviveButton.setBounds (caRowAbs.removeFromLeft (150).reduced (4, 2));
    }
    mainAreaLocal.removeFromTop (10);
    auto gridArea = mainAreaLocal.translated (mainArea.getX(), mainArea.getY());
    const auto gap = 8;
    auto cellWidth = (gridArea.getWidth() - gap * (cols - 1)) / cols;
    auto cellHeight = 70;
    const auto totalGridHeight = rows * cellHeight + (rows - 1) * gap;
    const auto gridY = gridArea.getY() + juce::jmax (0, (gridArea.getHeight() - totalGridHeight) / 2);

    for (int i = 0; i < cellButtons.size(); ++i)
    {
        auto row = i / cols;
        auto col = i % cols;
        cellButtons[i]->setBounds (gridArea.getX() + col * (cellWidth + gap),
                                   gridY + row * (cellHeight + gap),
                                   cellWidth,
                                   cellHeight);
        cellButtons[i]->setVisible (! isHarmonySpaceMode);
    }

    stage.setVisible (! showingMixer && (isHarmonySpaceMode || previewWidth > 0));
    if (! showingMixer && isHarmonySpaceMode)
    {
        auto stageBounds = mainAreaLocal.translated (mainArea.getX(), mainArea.getY()).reduced (4);
        stage.setBounds (stageBounds);
    }
    else if (! showingMixer && previewWidth > 0)
    {
        auto stageBounds = previewArea.getLocalBounds().reduced (currentMode == AppMode::harmonicGeometry ? 6 : (currentMode == AppMode::harmonySpace ? 8 : 14));

        const auto squareSize = juce::jmin (stageBounds.getWidth(), stageBounds.getHeight());
        stageBounds = juce::Rectangle<int> (squareSize, squareSize).withCentre (previewArea.getBounds().getCentre());

        stage.setBounds (stageBounds);
    }
    else
    {
        stage.setBounds ({});
    }

    if (inspectorWidth > 0)
        layoutInspector (inspector.getBounds().reduced (16));

    if (showingMixer)
        layoutMixer (mixerArea.getLocalBounds().reduced (16));
    else
        layoutMixer ({});

    auto sidebarContent = sidebar.getLocalBounds().reduced (10);
    sidebarToggleButton.setBounds (sidebarContent.removeFromTop (34));
    sidebarContent.removeFromTop (8);
    toolViewport.setVisible (isSidebarExpanded);
    toolViewport.setBounds (sidebarContent);

    int y = 10;
    for (auto* button : toolButtons)
    {
        button->setVisible (isSidebarExpanded);
        if (isSidebarExpanded)
        {
            button->setBounds (0, y, sidebarContent.getWidth(), 34);
            y += 42;
        }
    }
    toolListContent.setSize (sidebarContent.getWidth(), juce::jmax (sidebarContent.getHeight(), y + 8));
}

void MainComponent::layoutInspector (juce::Rectangle<int> area)
{
    const auto showCaInspectorControls = currentMode == AppMode::cellularGrid && inspector.isVisible();

    if (showCaInspectorControls)
    {
        caRuleButton.setVisible (true);
        caBirthButton.setVisible (true);
        caSurviveButton.setVisible (true);
        caRuleButton.setBounds (area.removeFromTop (34));
        area.removeFromTop (6);
        caBirthButton.setBounds (area.removeFromTop (34));
        area.removeFromTop (6);
        caSurviveButton.setBounds (area.removeFromTop (34));
        area.removeFromTop (12);
    }

    glyphValueLabel.setBounds (area.removeFromTop (28));
    labelEditor.setBounds (area.removeFromTop (40));
    area.removeFromTop (8);
    parameterPickerButton.setBounds (area.removeFromTop (32));
    area.removeFromTop (8);
    codeEditor.setBounds (area.removeFromTop (260));
    area.removeFromTop (8);
    eraseButton.setBounds (area.removeFromTop (34));
}

void MainComponent::layoutMixer (juce::Rectangle<int> area)
{
    const auto showMixerControls = mixerArea.isVisible() && mixerVisible && ! showingTitleScreen;
    const auto showPluginSlots = showMixerControls && isPluginMode();
    const auto compactLayout = area.getWidth() < 520;
    auto stripWidth = area.getWidth() / (int) mixerStrips.size();
    for (int strip = 0; strip < (int) mixerStrips.size(); ++strip)
    {
        mixerStripLabels[(size_t) strip].setVisible (showMixerControls);
        mixerMidiFxButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerMidiFxGuiButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerInstrumentButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerInstrumentGuiButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectAButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectAGuiButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectBButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectBGuiButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectCButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectCGuiButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectDButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectDGuiButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerMidiFxBypassButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerInstrumentBypassButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectABypassButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectBBypassButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectCBypassButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectDBypassButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerMidiFxRemoveButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerInstrumentRemoveButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectARemoveButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectBRemoveButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectCRemoveButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectDRemoveButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectAUpButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectBUpButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectCUpButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectDUpButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectADownButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectBDownButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectCDownButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerEffectDDownButtons[(size_t) strip].setVisible (showPluginSlots);
        mixerVolumeLabels[(size_t) strip].setVisible (showMixerControls);
        mixerPanLabels[(size_t) strip].setVisible (showMixerControls);
        mixerVolumeSliders[(size_t) strip].setVisible (showMixerControls);
        mixerPanSliders[(size_t) strip].setVisible (showMixerControls);

        if (! showMixerControls)
        {
            mixerStripLabels[(size_t) strip].setBounds ({});
            mixerMidiFxButtons[(size_t) strip].setBounds ({});
            mixerMidiFxGuiButtons[(size_t) strip].setBounds ({});
            mixerInstrumentButtons[(size_t) strip].setBounds ({});
            mixerInstrumentGuiButtons[(size_t) strip].setBounds ({});
            mixerEffectAButtons[(size_t) strip].setBounds ({});
            mixerEffectAGuiButtons[(size_t) strip].setBounds ({});
            mixerEffectBButtons[(size_t) strip].setBounds ({});
            mixerEffectBGuiButtons[(size_t) strip].setBounds ({});
            mixerEffectCButtons[(size_t) strip].setBounds ({});
            mixerEffectCGuiButtons[(size_t) strip].setBounds ({});
            mixerEffectDButtons[(size_t) strip].setBounds ({});
            mixerEffectDGuiButtons[(size_t) strip].setBounds ({});
            mixerMidiFxBypassButtons[(size_t) strip].setBounds ({});
            mixerInstrumentBypassButtons[(size_t) strip].setBounds ({});
            mixerEffectABypassButtons[(size_t) strip].setBounds ({});
            mixerEffectBBypassButtons[(size_t) strip].setBounds ({});
            mixerEffectCBypassButtons[(size_t) strip].setBounds ({});
            mixerEffectDBypassButtons[(size_t) strip].setBounds ({});
            mixerMidiFxRemoveButtons[(size_t) strip].setBounds ({});
            mixerInstrumentRemoveButtons[(size_t) strip].setBounds ({});
            mixerEffectARemoveButtons[(size_t) strip].setBounds ({});
            mixerEffectBRemoveButtons[(size_t) strip].setBounds ({});
            mixerEffectCRemoveButtons[(size_t) strip].setBounds ({});
            mixerEffectDRemoveButtons[(size_t) strip].setBounds ({});
            mixerEffectAUpButtons[(size_t) strip].setBounds ({});
            mixerEffectBUpButtons[(size_t) strip].setBounds ({});
            mixerEffectCUpButtons[(size_t) strip].setBounds ({});
            mixerEffectDUpButtons[(size_t) strip].setBounds ({});
            mixerEffectADownButtons[(size_t) strip].setBounds ({});
            mixerEffectBDownButtons[(size_t) strip].setBounds ({});
            mixerEffectCDownButtons[(size_t) strip].setBounds ({});
            mixerEffectDDownButtons[(size_t) strip].setBounds ({});
            mixerVolumeLabels[(size_t) strip].setBounds ({});
            mixerPanLabels[(size_t) strip].setBounds ({});
            mixerVolumeSliders[(size_t) strip].setBounds ({});
            mixerPanSliders[(size_t) strip].setBounds ({});
            continue;
        }

        auto stripArea = area.removeFromLeft (strip == (int) mixerStrips.size() - 1 ? area.getWidth() : stripWidth).reduced (compactLayout ? 4 : 8);
        mixerStripLabels[(size_t) strip].setBounds (stripArea.removeFromTop (compactLayout ? 22 : 28));
        stripArea.removeFromTop (compactLayout ? 4 : 8);
        auto layoutSlotRow = [] (juce::Rectangle<int> row,
                                 juce::TextButton& mainButton,
                                 juce::TextButton& guiButton,
                                 juce::TextButton& bypassButton,
                                 juce::TextButton& removeButton,
                                 juce::TextButton* upButton,
                                 juce::TextButton* downButton,
                                 bool compact)
        {
            const auto auxWidth = compact ? 22 : 28;
            const auto guiWidth = compact ? 28 : 38;
            const auto gap = compact ? 2 : 4;
            if (downButton != nullptr)
            {
                downButton->setBounds (row.removeFromRight (auxWidth));
                row.removeFromRight (gap);
            }
            if (upButton != nullptr)
            {
                upButton->setBounds (row.removeFromRight (auxWidth));
                row.removeFromRight (gap);
            }
            removeButton.setBounds (row.removeFromRight (auxWidth));
            row.removeFromRight (gap);
            bypassButton.setBounds (row.removeFromRight (auxWidth));
            row.removeFromRight (gap);
            guiButton.setBounds (row.removeFromRight (guiWidth));
            row.removeFromRight (gap);
            mainButton.setBounds (row);
        };
        if (showPluginSlots)
        {
            const auto midiFxHeight = compactLayout ? 24 : 30;
            const auto instrumentHeight = compactLayout ? 26 : 34;
            const auto effectHeight = compactLayout ? 24 : 28;
            const auto sectionGap = compactLayout ? 3 : 5;
            auto midiFxRow = stripArea.removeFromTop (midiFxHeight);
            layoutSlotRow (midiFxRow, mixerMidiFxButtons[(size_t) strip], mixerMidiFxGuiButtons[(size_t) strip], mixerMidiFxBypassButtons[(size_t) strip], mixerMidiFxRemoveButtons[(size_t) strip], nullptr, nullptr, compactLayout);
            stripArea.removeFromTop (sectionGap);
            auto instrumentRow = stripArea.removeFromTop (instrumentHeight);
            layoutSlotRow (instrumentRow, mixerInstrumentButtons[(size_t) strip], mixerInstrumentGuiButtons[(size_t) strip], mixerInstrumentBypassButtons[(size_t) strip], mixerInstrumentRemoveButtons[(size_t) strip], nullptr, nullptr, compactLayout);
            stripArea.removeFromTop (sectionGap);
            auto effectARow = stripArea.removeFromTop (effectHeight);
            layoutSlotRow (effectARow, mixerEffectAButtons[(size_t) strip], mixerEffectAGuiButtons[(size_t) strip], mixerEffectABypassButtons[(size_t) strip], mixerEffectARemoveButtons[(size_t) strip], &mixerEffectAUpButtons[(size_t) strip], &mixerEffectADownButtons[(size_t) strip], compactLayout);
            stripArea.removeFromTop (compactLayout ? 2 : 4);
            auto effectBRow = stripArea.removeFromTop (effectHeight);
            layoutSlotRow (effectBRow, mixerEffectBButtons[(size_t) strip], mixerEffectBGuiButtons[(size_t) strip], mixerEffectBBypassButtons[(size_t) strip], mixerEffectBRemoveButtons[(size_t) strip], &mixerEffectBUpButtons[(size_t) strip], &mixerEffectBDownButtons[(size_t) strip], compactLayout);
            stripArea.removeFromTop (compactLayout ? 2 : 4);
            auto effectCRow = stripArea.removeFromTop (effectHeight);
            layoutSlotRow (effectCRow, mixerEffectCButtons[(size_t) strip], mixerEffectCGuiButtons[(size_t) strip], mixerEffectCBypassButtons[(size_t) strip], mixerEffectCRemoveButtons[(size_t) strip], &mixerEffectCUpButtons[(size_t) strip], &mixerEffectCDownButtons[(size_t) strip], compactLayout);
            stripArea.removeFromTop (compactLayout ? 2 : 4);
            auto effectDRow = stripArea.removeFromTop (effectHeight);
            layoutSlotRow (effectDRow, mixerEffectDButtons[(size_t) strip], mixerEffectDGuiButtons[(size_t) strip], mixerEffectDBypassButtons[(size_t) strip], mixerEffectDRemoveButtons[(size_t) strip], &mixerEffectDUpButtons[(size_t) strip], &mixerEffectDDownButtons[(size_t) strip], compactLayout);
            stripArea.removeFromTop (compactLayout ? 6 : 10);
        }
        else
        {
            stripArea.removeFromTop (compactLayout ? 8 : 18);
        }
        mixerVolumeLabels[(size_t) strip].setBounds (stripArea.removeFromTop (18));
        const auto sliderHeight = compactLayout ? juce::jmax (96, stripArea.getHeight() - 72)
                                                : juce::jmax (140, stripArea.getHeight() - 90);
        mixerVolumeSliders[(size_t) strip].setBounds (stripArea.removeFromTop (sliderHeight));
        stripArea.removeFromTop (compactLayout ? 4 : 8);
        mixerPanLabels[(size_t) strip].setBounds (stripArea.removeFromTop (18));
        mixerPanSliders[(size_t) strip].setBounds (stripArea.removeFromTop (compactLayout ? 38 : 46));
    }
}

bool MainComponent::isPluginMode() const noexcept
{
    return currentVariant == ModeVariant::b;
}

MainComponent::ModeVariant MainComponent::getModeVariantForCurrentSession() const noexcept
{
    return currentVariant;
}

juce::String MainComponent::modeVariantToString (ModeVariant variant)
{
    return variant == ModeVariant::b ? "B" : "A";
}

MainComponent::ModeVariant MainComponent::stringToModeVariant (const juce::String& text)
{
    return text.equalsIgnoreCase ("B") ? ModeVariant::b : ModeVariant::a;
}

void MainComponent::initialisePluginHosting()
{
   #if JUCE_PLUGINHOST_AU
    pluginFormatManager.addFormat (std::make_unique<juce::AudioUnitPluginFormat>());
   #endif
   #if JUCE_PLUGINHOST_VST3
    pluginFormatManager.addFormat (std::make_unique<juce::VST3PluginFormat>());
   #endif
    loadCachedOrScanPluginDescriptions();
    clearPluginProcessingState();
}

juce::File MainComponent::getPluginCacheFile() const
{
    auto dir = juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("GlyphGrid");
    dir.createDirectory();
    return dir.getChildFile ("plugin-cache.json");
}

void MainComponent::savePluginCache() const
{
    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("version", 1);
    juce::var pluginsVar { juce::Array<juce::var>() };
    auto* array = pluginsVar.getArray();
    for (const auto& description : cachedPluginDescriptions)
    {
        auto item = std::make_unique<juce::DynamicObject>();
        item->setProperty ("name", description.name);
        item->setProperty ("descriptiveName", description.descriptiveName);
        item->setProperty ("fileOrIdentifier", description.fileOrIdentifier);
        item->setProperty ("pluginFormatName", description.pluginFormatName);
        item->setProperty ("category", description.category);
        item->setProperty ("manufacturerName", description.manufacturerName);
        item->setProperty ("version", description.version);
        item->setProperty ("isInstrument", description.isInstrument);
        item->setProperty ("numInputChannels", description.numInputChannels);
        item->setProperty ("numOutputChannels", description.numOutputChannels);
        item->setProperty ("uid", (int64) description.uniqueId);
        array->add (juce::var (item.release()));
    }
    root->setProperty ("plugins", pluginsVar);
    getPluginCacheFile().replaceWithText (juce::JSON::toString (juce::var (root.release()), true));
}

bool MainComponent::loadPluginCache()
{
    const auto file = getPluginCacheFile();
    if (! file.existsAsFile())
        return false;

    const auto parsed = juce::JSON::parse (file.loadFileAsString());
    auto* root = parsed.getDynamicObject();
    auto* array = root != nullptr ? root->getProperty ("plugins").getArray() : nullptr;
    if (array == nullptr)
        return false;

    cachedPluginDescriptions.clearQuick();
    for (const auto& itemVar : *array)
    {
        auto* item = itemVar.getDynamicObject();
        if (item == nullptr)
            continue;

        juce::PluginDescription description;
        description.name = item->getProperty ("name").toString();
        description.descriptiveName = item->getProperty ("descriptiveName").toString();
        description.fileOrIdentifier = item->getProperty ("fileOrIdentifier").toString();
        description.pluginFormatName = item->getProperty ("pluginFormatName").toString();
        description.category = item->getProperty ("category").toString();
        description.manufacturerName = item->getProperty ("manufacturerName").toString();
        description.version = item->getProperty ("version").toString();
        description.isInstrument = (bool) item->getProperty ("isInstrument");
        description.numInputChannels = (int) item->getProperty ("numInputChannels");
        description.numOutputChannels = (int) item->getProperty ("numOutputChannels");
        description.uniqueId = (int) (int64) item->getProperty ("uid");
        if (description.name.isNotEmpty() && description.fileOrIdentifier.isNotEmpty())
            cachedPluginDescriptions.add (description);
    }

    return cachedPluginDescriptions.size() > 0;
}

void MainComponent::scanLogicPluginFolders()
{
    cachedPluginDescriptions.clearQuick();

   #if JUCE_PLUGINHOST_AU
    juce::Array<juce::File> componentFiles;
    const auto addComponentsFrom = [&componentFiles] (const juce::File& dir)
    {
        if (! dir.isDirectory())
            return;
        juce::Array<juce::File> found;
        dir.findChildFiles (found, juce::File::findDirectories, false, "*.component");
        componentFiles.addArray (found);
    };

    addComponentsFrom (juce::File ("/Library/Audio/Plug-Ins/Components"));
    addComponentsFrom (juce::File::getSpecialLocation (juce::File::userHomeDirectory)
                           .getChildFile ("Library/Audio/Plug-Ins/Components"));

    juce::AudioUnitPluginFormat* auFormat = nullptr;
    for (auto* format : pluginFormatManager.getFormats())
        if (dynamic_cast<juce::AudioUnitPluginFormat*> (format) != nullptr)
            auFormat = dynamic_cast<juce::AudioUnitPluginFormat*> (format);

    if (auFormat != nullptr)
    {
        for (const auto& file : componentFiles)
        {
            juce::OwnedArray<juce::PluginDescription> descriptions;
            auFormat->findAllTypesForFile (descriptions, file.getFullPathName());
            for (auto* desc : descriptions)
                if (desc != nullptr)
                {
                    auto exists = false;
                    for (const auto& existing : cachedPluginDescriptions)
                    {
                        if (existing.fileOrIdentifier == desc->fileOrIdentifier
                            && existing.name == desc->name
                            && existing.pluginFormatName == desc->pluginFormatName)
                        {
                            exists = true;
                            break;
                        }
                    }

                    if (! exists)
                        cachedPluginDescriptions.add (*desc);
                }
        }
    }
   #endif

    savePluginCache();
}

void MainComponent::loadCachedOrScanPluginDescriptions()
{
    if (! loadPluginCache())
        scanLogicPluginFolders();
}

void MainComponent::clearPluginProcessingState() noexcept
{
    for (int stripIndex = 0; stripIndex < (int) mixerStrips.size(); ++stripIndex)
    {
        auto& strip = mixerStrips[(size_t) stripIndex];
        strip.midiBuffer.clear();
        strip.pendingMidi.clear();
        strip.audioBuffer.setSize (2, 0, false, false, true);
        strip.midiFxScratchBuffer.setSize (2, 0, false, false, true);
        publishedMixerLevels[(size_t) stripIndex].store (0.0f, std::memory_order_relaxed);
    }
}

void MainComponent::preparePluginInstances (double sampleRate, int blockSize)
{
    currentPluginBlockSize = juce::jmax (32, blockSize);
    for (auto& strip : mixerStrips)
    {
        for (auto* slot : { &strip.midiFx, &strip.instrument, &strip.effectA, &strip.effectB, &strip.effectC, &strip.effectD })
        {
            if (slot->instance != nullptr)
            {
                slot->instance->releaseResources();
                slot->instance->prepareToPlay (sampleRate, currentPluginBlockSize);
            }
        }
    }
}

void MainComponent::releasePluginInstances()
{
    for (auto& strip : mixerStrips)
    {
        for (auto* slot : { &strip.midiFx, &strip.instrument, &strip.effectA, &strip.effectB, &strip.effectC, &strip.effectD })
            if (slot->instance != nullptr)
                slot->instance->releaseResources();
    }
}

MainComponent::HostedPluginSlot& MainComponent::getHostedPluginSlot (int stripIndex, PluginSlotKind slotKind) noexcept
{
    auto& strip = mixerStrips[(size_t) juce::jlimit (0, (int) mixerStrips.size() - 1, stripIndex)];
    if (slotKind == PluginSlotKind::midiFx)
        return strip.midiFx;
    if (slotKind == PluginSlotKind::instrument)
        return strip.instrument;
    if (slotKind == PluginSlotKind::effectA)
        return strip.effectA;
    if (slotKind == PluginSlotKind::effectB)
        return strip.effectB;
    if (slotKind == PluginSlotKind::effectC)
        return strip.effectC;
    return strip.effectD;
}

const MainComponent::HostedPluginSlot& MainComponent::getHostedPluginSlot (int stripIndex, PluginSlotKind slotKind) const noexcept
{
    auto& strip = mixerStrips[(size_t) juce::jlimit (0, (int) mixerStrips.size() - 1, stripIndex)];
    if (slotKind == PluginSlotKind::midiFx)
        return strip.midiFx;
    if (slotKind == PluginSlotKind::instrument)
        return strip.instrument;
    if (slotKind == PluginSlotKind::effectA)
        return strip.effectA;
    if (slotKind == PluginSlotKind::effectB)
        return strip.effectB;
    if (slotKind == PluginSlotKind::effectC)
        return strip.effectC;
    return strip.effectD;
}

bool MainComponent::loadPluginIntoSlot (int stripIndex, PluginSlotKind slotKind, const juce::File& file)
{
    const auto slotIndex = [slotKind]
    {
        switch (slotKind)
        {
            case PluginSlotKind::midiFx: return 0;
            case PluginSlotKind::instrument: return 1;
            case PluginSlotKind::effectA: return 2;
            case PluginSlotKind::effectB: return 3;
            case PluginSlotKind::effectC: return 4;
            case PluginSlotKind::effectD: return 5;
        }
        return 0;
    }();

    juce::OwnedArray<juce::PluginDescription> descriptions;
    for (auto* format : pluginFormatManager.getFormats())
    {
        if (! format->fileMightContainThisPluginType (file.getFullPathName()))
            continue;
        format->findAllTypesForFile (descriptions, file.getFullPathName());
        if (! descriptions.isEmpty())
            break;
    }

    if (descriptions.isEmpty())
        return false;

    juce::String error;
    auto instance = std::unique_ptr<juce::AudioPluginInstance> (
        pluginFormatManager.createPluginInstance (*descriptions[0], currentSampleRate, currentPluginBlockSize, error));

    if (instance == nullptr)
        return false;

    instance->prepareToPlay (currentSampleRate, currentPluginBlockSize);
    auto& slot = getHostedPluginSlot (stripIndex, slotKind);
    if (slot.instance != nullptr)
        slot.instance->releaseResources();
    mixerStrips[(size_t) stripIndex].pluginWindows[(size_t) slotIndex].reset();
    slot.instance = std::move (instance);
    slot.buttonText = descriptions[0]->name.isNotEmpty() ? descriptions[0]->name : file.getFileNameWithoutExtension();
    slot.pluginPath = file.getFullPathName();
    slot.bypassed = false;
    updateMixerButtons();
    return true;
}

bool MainComponent::loadPluginIntoSlot (int stripIndex, PluginSlotKind slotKind, const juce::PluginDescription& description)
{
    const auto slotIndex = [slotKind]
    {
        switch (slotKind)
        {
            case PluginSlotKind::midiFx: return 0;
            case PluginSlotKind::instrument: return 1;
            case PluginSlotKind::effectA: return 2;
            case PluginSlotKind::effectB: return 3;
            case PluginSlotKind::effectC: return 4;
            case PluginSlotKind::effectD: return 5;
        }
        return 0;
    }();

    juce::String error;
    auto instance = std::unique_ptr<juce::AudioPluginInstance> (
        pluginFormatManager.createPluginInstance (description, currentSampleRate, currentPluginBlockSize, error));

    if (instance == nullptr)
        return false;

    instance->prepareToPlay (currentSampleRate, currentPluginBlockSize);
    auto& slot = getHostedPluginSlot (stripIndex, slotKind);
    if (slot.instance != nullptr)
        slot.instance->releaseResources();
    mixerStrips[(size_t) stripIndex].pluginWindows[(size_t) slotIndex].reset();
    slot.instance = std::move (instance);
    slot.buttonText = description.name.isNotEmpty() ? description.name : description.descriptiveName;
    slot.pluginPath = description.fileOrIdentifier;
    slot.bypassed = false;
    updateMixerButtons();
    return true;
}

juce::String MainComponent::serialisePluginState (const HostedPluginSlot& slot)
{
    if (slot.instance == nullptr)
        return {};

    juce::MemoryBlock state;
    slot.instance->getStateInformation (state);

    if (state.getSize() == 0)
        return {};

    return juce::Base64::toBase64 (state.getData(), state.getSize());
}

void MainComponent::restorePluginState (HostedPluginSlot& slot, const juce::String& encodedState)
{
    if (slot.instance == nullptr || encodedState.isEmpty())
        return;

    juce::MemoryOutputStream output;
    if (! juce::Base64::convertFromBase64 (output, encodedState))
        return;

    const auto dataSize = output.getDataSize();
    if (dataSize == 0)
        return;

    slot.instance->setStateInformation (output.getData(), (int) dataSize);
}

void MainComponent::updateMixerButtons()
{
    for (int strip = 0; strip < (int) mixerStrips.size(); ++strip)
    {
        const auto setSlotText = [] (juce::TextButton& button, const HostedPluginSlot& slot, PluginSlotKind kind, const juce::String& emptyText)
        {
            const auto prefix = mixerSlotPrefix (kind);
            const auto body = slot.buttonText.isNotEmpty() && slot.buttonText != "Empty" ? slot.buttonText : emptyText;
            button.setButtonText (prefix + ": " + body);
            button.setTooltip (prefix + " slot");
        };

        setSlotText (mixerMidiFxButtons[(size_t) strip], getHostedPluginSlot (strip, PluginSlotKind::midiFx), PluginSlotKind::midiFx, "Load MIDI FX");
        setSlotText (mixerInstrumentButtons[(size_t) strip], getHostedPluginSlot (strip, PluginSlotKind::instrument), PluginSlotKind::instrument, "Load Instrument");
        mixerMidiFxGuiButtons[(size_t) strip].setEnabled (getHostedPluginSlot (strip, PluginSlotKind::midiFx).instance != nullptr
                                                          && getHostedPluginSlot (strip, PluginSlotKind::midiFx).instance->hasEditor());
        mixerInstrumentGuiButtons[(size_t) strip].setEnabled (getHostedPluginSlot (strip, PluginSlotKind::instrument).instance != nullptr
                                                               && getHostedPluginSlot (strip, PluginSlotKind::instrument).instance->hasEditor());
        setSlotText (mixerEffectAButtons[(size_t) strip], getHostedPluginSlot (strip, PluginSlotKind::effectA), PluginSlotKind::effectA, "Load FX A");
        setSlotText (mixerEffectBButtons[(size_t) strip], getHostedPluginSlot (strip, PluginSlotKind::effectB), PluginSlotKind::effectB, "Load FX B");
        setSlotText (mixerEffectCButtons[(size_t) strip], getHostedPluginSlot (strip, PluginSlotKind::effectC), PluginSlotKind::effectC, "Load FX C");
        setSlotText (mixerEffectDButtons[(size_t) strip], getHostedPluginSlot (strip, PluginSlotKind::effectD), PluginSlotKind::effectD, "Load FX D");
        mixerEffectAGuiButtons[(size_t) strip].setEnabled (getHostedPluginSlot (strip, PluginSlotKind::effectA).instance != nullptr
                                                           && getHostedPluginSlot (strip, PluginSlotKind::effectA).instance->hasEditor());
        mixerEffectBGuiButtons[(size_t) strip].setEnabled (getHostedPluginSlot (strip, PluginSlotKind::effectB).instance != nullptr
                                                           && getHostedPluginSlot (strip, PluginSlotKind::effectB).instance->hasEditor());
        mixerEffectCGuiButtons[(size_t) strip].setEnabled (getHostedPluginSlot (strip, PluginSlotKind::effectC).instance != nullptr
                                                           && getHostedPluginSlot (strip, PluginSlotKind::effectC).instance->hasEditor());
        mixerEffectDGuiButtons[(size_t) strip].setEnabled (getHostedPluginSlot (strip, PluginSlotKind::effectD).instance != nullptr
                                                           && getHostedPluginSlot (strip, PluginSlotKind::effectD).instance->hasEditor());

        const auto updateBypass = [] (juce::TextButton& button, const HostedPluginSlot& slot)
        {
            button.setButtonText (slot.bypassed ? "On" : "By");
            button.setEnabled (slot.instance != nullptr);
        };
        const auto updateRemove = [] (juce::TextButton& button, const HostedPluginSlot& slot)
        {
            button.setButtonText ("X");
            button.setEnabled (slot.instance != nullptr);
        };
        updateBypass (mixerMidiFxBypassButtons[(size_t) strip], getHostedPluginSlot (strip, PluginSlotKind::midiFx));
        updateBypass (mixerInstrumentBypassButtons[(size_t) strip], getHostedPluginSlot (strip, PluginSlotKind::instrument));
        updateBypass (mixerEffectABypassButtons[(size_t) strip], getHostedPluginSlot (strip, PluginSlotKind::effectA));
        updateBypass (mixerEffectBBypassButtons[(size_t) strip], getHostedPluginSlot (strip, PluginSlotKind::effectB));
        updateBypass (mixerEffectCBypassButtons[(size_t) strip], getHostedPluginSlot (strip, PluginSlotKind::effectC));
        updateBypass (mixerEffectDBypassButtons[(size_t) strip], getHostedPluginSlot (strip, PluginSlotKind::effectD));
        updateRemove (mixerMidiFxRemoveButtons[(size_t) strip], getHostedPluginSlot (strip, PluginSlotKind::midiFx));
        updateRemove (mixerInstrumentRemoveButtons[(size_t) strip], getHostedPluginSlot (strip, PluginSlotKind::instrument));
        updateRemove (mixerEffectARemoveButtons[(size_t) strip], getHostedPluginSlot (strip, PluginSlotKind::effectA));
        updateRemove (mixerEffectBRemoveButtons[(size_t) strip], getHostedPluginSlot (strip, PluginSlotKind::effectB));
        updateRemove (mixerEffectCRemoveButtons[(size_t) strip], getHostedPluginSlot (strip, PluginSlotKind::effectC));
        updateRemove (mixerEffectDRemoveButtons[(size_t) strip], getHostedPluginSlot (strip, PluginSlotKind::effectD));
        mixerEffectAUpButtons[(size_t) strip].setEnabled (false);
        mixerEffectBUpButtons[(size_t) strip].setEnabled (getHostedPluginSlot (strip, PluginSlotKind::effectB).instance != nullptr);
        mixerEffectCUpButtons[(size_t) strip].setEnabled (getHostedPluginSlot (strip, PluginSlotKind::effectC).instance != nullptr);
        mixerEffectDUpButtons[(size_t) strip].setEnabled (getHostedPluginSlot (strip, PluginSlotKind::effectD).instance != nullptr);
        mixerEffectADownButtons[(size_t) strip].setEnabled (getHostedPluginSlot (strip, PluginSlotKind::effectA).instance != nullptr);
        mixerEffectBDownButtons[(size_t) strip].setEnabled (getHostedPluginSlot (strip, PluginSlotKind::effectB).instance != nullptr);
        mixerEffectCDownButtons[(size_t) strip].setEnabled (getHostedPluginSlot (strip, PluginSlotKind::effectC).instance != nullptr);
        mixerEffectDDownButtons[(size_t) strip].setEnabled (false);
    }
}

juce::StringArray MainComponent::getParameterChoicesForSlot (int stripIndex, PluginSlotKind slotKind) const
{
    juce::StringArray result;
    const auto& slot = getHostedPluginSlot (stripIndex, slotKind);
    if (slot.instance == nullptr)
        return result;

    for (int index = 0; index < slot.instance->getParameters().size(); ++index)
    {
        auto* parameter = slot.instance->getParameters()[index];
        if (parameter == nullptr)
            continue;

        auto entry = "#" + juce::String (index) + " " + parameter->getName (256);
        if (auto* withId = dynamic_cast<juce::AudioProcessorParameterWithID*> (parameter))
            entry << " [id:" << withId->paramID << "]";
        result.add (entry);
    }

    return result;
}

void MainComponent::showPluginEditor (int stripIndex, PluginSlotKind slotKind)
{
    auto& strip = mixerStrips[(size_t) juce::jlimit (0, (int) mixerStrips.size() - 1, stripIndex)];
    auto& slot = getHostedPluginSlot (stripIndex, slotKind);
    if (slot.instance == nullptr || ! slot.instance->hasEditor())
        return;

    const auto slotIndex = [slotKind]
    {
        switch (slotKind)
        {
            case PluginSlotKind::midiFx: return 0;
            case PluginSlotKind::instrument: return 1;
            case PluginSlotKind::effectA: return 2;
            case PluginSlotKind::effectB: return 3;
            case PluginSlotKind::effectC: return 4;
            case PluginSlotKind::effectD: return 5;
        }
        return 0;
    }();

    if (strip.pluginWindows[(size_t) slotIndex] != nullptr)
    {
        strip.pluginWindows[(size_t) slotIndex]->toFront (true);
        return;
    }

    struct PluginEditorWindow final : juce::DocumentWindow
    {
        PluginEditorWindow (juce::String name, std::unique_ptr<juce::AudioProcessorEditor> editor, std::function<void()> onCloseIn)
            : juce::DocumentWindow (std::move (name),
                                    juce::Colour::fromFloatRGBA (0.08f, 0.10f, 0.12f, 0.98f),
                                    juce::DocumentWindow::closeButton),
              onClose (std::move (onCloseIn))
        {
            setUsingNativeTitleBar (true);
            setResizable (true, true);
            setContentOwned (editor.release(), true);
            centreWithSize (juce::jmax (520, getContentComponent()->getWidth()),
                            juce::jmax (340, getContentComponent()->getHeight()));
            setVisible (true);
        }

        void closeButtonPressed() override
        {
            if (onClose)
                onClose();
        }

        std::function<void()> onClose;
    };

    auto editor = std::unique_ptr<juce::AudioProcessorEditor> (slot.instance->createEditor());
    if (editor == nullptr)
        return;

    strip.pluginWindows[(size_t) slotIndex] = std::make_unique<PluginEditorWindow> (
        strip.name + " " + mixerSlotPrefix (slotKind),
        std::move (editor),
        [this, stripIndex, slotIndex]
        {
            mixerStrips[(size_t) stripIndex].pluginWindows[(size_t) slotIndex].reset();
        });
}

bool MainComponent::isMidiFxPlugin (const juce::PluginDescription& description) noexcept
{
    return description.category.containsIgnoreCase ("midi")
        || description.name.containsIgnoreCase ("midi")
        || description.descriptiveName.containsIgnoreCase ("midi");
}

bool MainComponent::isInstrumentPlugin (const juce::PluginDescription& description) noexcept
{
    return description.isInstrument;
}

juce::Array<juce::PluginDescription> MainComponent::getPluginChoicesForSlot (PluginSlotKind slotKind) const
{
    juce::Array<juce::PluginDescription> result;
    for (const auto& description : cachedPluginDescriptions)
    {
        const auto midiFx = isMidiFxPlugin (description);
        const auto instrument = isInstrumentPlugin (description);
        const auto allow = slotKind == PluginSlotKind::midiFx ? midiFx
                        : (slotKind == PluginSlotKind::instrument ? instrument
                                                                  : (! instrument));
        if (allow)
            result.add (description);
    }

    std::sort (result.begin(), result.end(), [] (const auto& a, const auto& b)
    {
        return a.name.compareIgnoreCase (b.name) < 0;
    });
    return result;
}

void MainComponent::queuePluginMessage (int stripIndex, const juce::MidiMessage& message, int absoluteSampleOffset) noexcept
{
    auto& strip = mixerStrips[(size_t) juce::jlimit (0, (int) mixerStrips.size() - 1, stripIndex)];
    if (absoluteSampleOffset < currentPluginBlockSize)
    {
        strip.midiBuffer.addEvent (message, juce::jmax (0, absoluteSampleOffset));
        return;
    }

    strip.pendingMidi.push_back ({ absoluteSampleOffset - currentPluginBlockSize, message });
}

void MainComponent::queuePluginNote (int preferredStrip,
                                     int midiChannel,
                                     float midiNote,
                                     float amplitude,
                                     float decayScale,
                                     int delaySamples,
                                     float pitchBend,
                                     float channelPressure,
                                     int ccNumber,
                                     int ccValue) noexcept
{
    const auto noteNumber = juce::jlimit (0, 127, (int) std::round (midiNote));
    const auto stripIndex = resolvePreferredStrip (preferredStrip, midiNote);
    const auto channel = juce::jlimit (1, 16, midiChannel);

    const auto velocity = juce::jlimit (1, 127, (int) std::round (amplitude * 127.0f));
    const auto noteOnOffset = currentAudioBlockSampleOffset + juce::jmax (0, delaySamples);
    const auto durationSamples = juce::jlimit (512,
                                               (int) (currentSampleRate * 4.0),
                                               (int) std::round (currentSampleRate * 0.18 * juce::jlimit (0.4f, 4.0f, (float) decayScale)));
    if (ccNumber >= 0)
        queuePluginMessage (stripIndex, juce::MidiMessage::controllerEvent (channel, juce::jlimit (0, 127, ccNumber), juce::jlimit (0, 127, ccValue)), noteOnOffset);

    if (std::abs (pitchBend) > 0.0001f)
    {
        const auto bendValue = juce::jlimit (0, 16383, (int) std::round (8192.0 + juce::jlimit (-1.0f, 1.0f, pitchBend) * 8191.0));
        queuePluginMessage (stripIndex, juce::MidiMessage::pitchWheel (channel, bendValue), noteOnOffset);
        queuePluginMessage (stripIndex, juce::MidiMessage::pitchWheel (channel, 8192), noteOnOffset + durationSamples + 1);
    }

    if (channelPressure > 0.0001f)
    {
        const auto pressureValue = juce::jlimit (0, 127, (int) std::round (juce::jlimit (0.0f, 1.0f, channelPressure) * 127.0f));
        queuePluginMessage (stripIndex, juce::MidiMessage::channelPressureChange (channel, pressureValue), noteOnOffset);
        queuePluginMessage (stripIndex, juce::MidiMessage::channelPressureChange (channel, 0), noteOnOffset + durationSamples + 1);
    }

    queuePluginMessage (stripIndex, juce::MidiMessage::noteOn (channel, noteNumber, (juce::uint8) velocity), noteOnOffset);
    queuePluginMessage (stripIndex, juce::MidiMessage::noteOff (channel, noteNumber), noteOnOffset + durationSamples);
}

void MainComponent::applyPluginParameterTarget (int preferredStrip, float midiNote, const juce::String& targetSpec, float fallbackNormalized) noexcept
{
    auto explicitStripIndex = -1;
    juce::String slotQualifier, parameterToken;
    auto normalizedValue = 0.0f;
    auto hasExplicitValue = false;
    if (! parseParameterSpec (targetSpec, explicitStripIndex, slotQualifier, parameterToken, normalizedValue, hasExplicitValue))
        return;

    if (! hasExplicitValue)
        normalizedValue = juce::jlimit (0.0f, 1.0f, fallbackNormalized);

    const auto stripIndex = explicitStripIndex >= 0 ? explicitStripIndex
                                                    : resolvePreferredStrip (preferredStrip, midiNote);
    auto tryApply = [&] (HostedPluginSlot& slot) -> bool
    {
        if (slot.instance == nullptr)
            return false;

        auto* parameter = resolvePluginParameter (*slot.instance, parameterToken);
        if (parameter == nullptr)
            return false;

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (normalizedValue);
        parameter->endChangeGesture();
        return true;
    };

    auto& strip = mixerStrips[(size_t) stripIndex];
    auto tryQualifiedSlot = [&] () -> bool
    {
        if (slotQualifier == "midi")
            return tryApply (strip.midiFx);
        if (slotQualifier == "inst")
            return tryApply (strip.instrument);
        if (slotQualifier == "fx1")
            return tryApply (strip.effectA);
        if (slotQualifier == "fx2")
            return tryApply (strip.effectB);
        if (slotQualifier == "fx3")
            return tryApply (strip.effectC);
        if (slotQualifier == "fx4")
            return tryApply (strip.effectD);
        return false;
    };

    if (slotQualifier.isNotEmpty())
    {
        tryQualifiedSlot();
        return;
    }

    for (auto* slot : { &strip.instrument, &strip.effectA, &strip.effectB, &strip.effectC, &strip.effectD, &strip.midiFx })
        if (tryApply (*slot))
            return;
}

void MainComponent::flushPendingPluginEventsForBlock (int blockSize) noexcept
{
    currentPluginBlockSize = blockSize;
    for (auto& strip : mixerStrips)
    {
        strip.midiBuffer.clear();
        std::vector<ScheduledMidiEvent> remaining;
        remaining.reserve (strip.pendingMidi.size());
        for (auto& event : strip.pendingMidi)
        {
            if (event.sampleOffset < blockSize)
                strip.midiBuffer.addEvent (event.message, event.sampleOffset);
            else
                remaining.push_back ({ event.sampleOffset - blockSize, event.message });
        }
        strip.pendingMidi.swap (remaining);
    }
}

void MainComponent::processPluginBlock (const juce::AudioSourceChannelInfo& bufferToFill,
                                        const PatchSnapshot& snapshot,
                                        double startTimeSeconds,
                                        bool running)
{
    auto* outputBuffer = bufferToFill.buffer;
    outputBuffer->clear (bufferToFill.startSample, bufferToFill.numSamples);
    flushPendingPluginEventsForBlock (bufferToFill.numSamples);
    auto timeSeconds = startTimeSeconds;
    auto transportPhase = 0.0f;

    applyPendingTransportCommands();

    for (int sample = 0; sample < bufferToFill.numSamples; ++sample)
    {
        currentAudioBlockSampleOffset = sample;
        if (running)
        {
            const auto sequencerTime = timeSeconds * (snapshot.bpm / 60.0) * 4.0;
            const auto targetTick = (int) std::floor (sequencerTime);
            if (targetTick != lastAdvancedTick.load())
            {
                applyPendingTransportCommands();
                if (currentMode == AppMode::cellularGrid)
                    advanceAutomataToTick (targetTick, snapshot);
                else
                    advanceSnakesToTick (targetTick, snapshot);
                processAudioTick (snapshot, timeSeconds);
                lastAdvancedTick.store (targetTick);
            }
            transportPhase = (float) (sequencerTime - std::floor (sequencerTime));
            timeSeconds += 1.0 / currentSampleRate;
        }
    }

    juce::MidiBuffer emptyMidi;
    for (int stripIndex = 0; stripIndex < (int) mixerStrips.size(); ++stripIndex)
    {
        auto& strip = mixerStrips[(size_t) stripIndex];
        strip.audioBuffer.setSize (2, bufferToFill.numSamples, false, false, true);
        strip.midiFxScratchBuffer.setSize (2, bufferToFill.numSamples, false, false, true);
        strip.audioBuffer.clear();
        strip.midiFxScratchBuffer.clear();

        if (strip.midiFx.instance != nullptr && ! strip.midiFx.bypassed)
            strip.midiFx.instance->processBlock (strip.midiFxScratchBuffer, strip.midiBuffer);
        if (strip.instrument.instance != nullptr && ! strip.instrument.bypassed)
            strip.instrument.instance->processBlock (strip.audioBuffer, strip.midiBuffer);

        if (strip.effectA.instance != nullptr && ! strip.effectA.bypassed)
            strip.effectA.instance->processBlock (strip.audioBuffer, emptyMidi);
        if (strip.effectB.instance != nullptr && ! strip.effectB.bypassed)
            strip.effectB.instance->processBlock (strip.audioBuffer, emptyMidi);
        if (strip.effectC.instance != nullptr && ! strip.effectC.bypassed)
            strip.effectC.instance->processBlock (strip.audioBuffer, emptyMidi);
        if (strip.effectD.instance != nullptr && ! strip.effectD.bypassed)
            strip.effectD.instance->processBlock (strip.audioBuffer, emptyMidi);

        const auto leftGain = strip.volume * (strip.pan <= 0.0f ? 1.0f : 1.0f - strip.pan);
        const auto rightGain = strip.volume * (strip.pan >= 0.0f ? 1.0f : 1.0f + strip.pan);
        auto peak = 0.0f;
        for (int channel = 0; channel < strip.audioBuffer.getNumChannels(); ++channel)
        {
            const auto* read = strip.audioBuffer.getReadPointer (channel);
            const auto gain = channel == 0 ? leftGain : rightGain;
            for (int sample = 0; sample < bufferToFill.numSamples; ++sample)
                peak = juce::jmax (peak, std::abs (read[sample] * gain));
        }
        const auto previousLevel = publishedMixerLevels[(size_t) stripIndex].load (std::memory_order_relaxed);
        const auto smoothedLevel = juce::jmax (peak, previousLevel * 0.90f);
        publishedMixerLevels[(size_t) stripIndex].store (juce::jlimit (0.0f, 1.0f, smoothedLevel), std::memory_order_relaxed);
        outputBuffer->addFrom (0, bufferToFill.startSample, strip.audioBuffer, 0, 0, bufferToFill.numSamples, leftGain);
        if (outputBuffer->getNumChannels() > 1)
            outputBuffer->addFrom (1, bufferToFill.startSample, strip.audioBuffer, juce::jmin (1, strip.audioBuffer.getNumChannels() - 1), 0, bufferToFill.numSamples, rightGain);
    }

    for (int channel = 0; channel < outputBuffer->getNumChannels(); ++channel)
        outputBuffer->applyGain (channel, bufferToFill.startSample, bufferToFill.numSamples, 2.1f);

    currentTimeSeconds.store (running ? timeSeconds : startTimeSeconds);
    transportSequence.fetch_add (1, std::memory_order_acq_rel);
    publishedTransportSnapshot.snakeCount = audioSnakeCount;
    publishedTransportSnapshot.automataMode = currentMode == AppMode::cellularGrid;
    publishedTransportSnapshot.automataCurrent = automataCurrent;
    publishedTransportSnapshot.automataPrevious = automataPrevious;
    publishedTransportSnapshot.transportPhase = transportPhase;
    for (int i = 0; i < maxSnakes; ++i)
        publishedTransportSnapshot.snakes[(size_t) i] = audioSnakes[(size_t) i];
    transportSequence.fetch_add (1, std::memory_order_release);
}

void MainComponent::prepareToPlay (int, double sampleRate)
{
    currentSampleRate = sampleRate > 0.0 ? sampleRate : 44100.0;
    resetSynthVoices();
    resetDrumVoices();
    outputFastState = 0.0f;
    outputSlowState = 0.0f;
    outputHpState = 0.0f;
    outputHpInputState = 0.0f;

    const auto fastHz = 1800.0;
    const auto slowHz = 760.0;
    const auto hpHz = 118.0;
    outputFastCoeff = 1.0f - std::exp ((float) (-juce::MathConstants<double>::twoPi * fastHz / currentSampleRate));
    outputSlowCoeff = 1.0f - std::exp ((float) (-juce::MathConstants<double>::twoPi * slowHz / currentSampleRate));
    outputHpCoeff = std::exp ((float) (-juce::MathConstants<double>::twoPi * hpHz / currentSampleRate));
    preparePluginInstances (currentSampleRate, currentPluginBlockSize);
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto patchSnapshot = getPatchSnapshot();
    if (isPluginMode() && patchSnapshot != nullptr)
    {
        processPluginBlock (bufferToFill, *patchSnapshot, currentTimeSeconds.load(), isRunning.load());
        return;
    }

    auto* left = bufferToFill.buffer->getWritePointer (0, bufferToFill.startSample);
    auto* right = bufferToFill.buffer->getNumChannels() > 1
                    ? bufferToFill.buffer->getWritePointer (1, bufferToFill.startSample)
                    : nullptr;
    auto timeSeconds = currentTimeSeconds.load();
    const auto running = isRunning.load();
    float transportPhase = 0.0f;

    applyPendingTransportCommands();

    for (int sample = 0; sample < bufferToFill.numSamples; ++sample)
    {
        float leftValue = 0.0f;
        float rightValue = 0.0f;

        if (running && patchSnapshot != nullptr)
        {
            const auto bpmSnapshot = patchSnapshot->bpm;
            const auto sequencerTime = timeSeconds * (bpmSnapshot / 60.0) * 4.0;
            const auto targetTick = (int) std::floor (sequencerTime);
            if (targetTick != lastAdvancedTick.load())
            {
                applyPendingTransportCommands();
                if (currentMode == AppMode::cellularGrid)
                    advanceAutomataToTick (targetTick, *patchSnapshot);
                else
                    advanceSnakesToTick (targetTick, *patchSnapshot);
                processAudioTick (*patchSnapshot, timeSeconds);
                lastAdvancedTick.store (targetTick);
            }

            renderInternalMix (leftValue, rightValue);
            outputFastState += (leftValue - outputFastState) * outputFastCoeff;
            outputSlowState += (outputFastState - outputSlowState) * outputSlowCoeff;
            const auto hpLeft = outputHpCoeff * (outputHpState + outputSlowState - outputHpInputState);
            outputHpInputState = outputSlowState;
            outputHpState = hpLeft;
            leftValue = std::tanh (hpLeft * 1.0f) * 2.48f;

            static float outputFastStateR = 0.0f;
            static float outputSlowStateR = 0.0f;
            static float outputHpStateR = 0.0f;
            static float outputHpInputStateR = 0.0f;
            outputFastStateR += (rightValue - outputFastStateR) * outputFastCoeff;
            outputSlowStateR += (outputFastStateR - outputSlowStateR) * outputSlowCoeff;
            const auto hpRight = outputHpCoeff * (outputHpStateR + outputSlowStateR - outputHpInputStateR);
            outputHpInputStateR = outputSlowStateR;
            outputHpStateR = hpRight;
            rightValue = std::tanh (hpRight * 1.0f) * 2.48f;
            transportPhase = (float) (sequencerTime - std::floor (sequencerTime));
            timeSeconds += 1.0 / currentSampleRate;
        }
        else
        {
            outputFastState *= 0.995f;
            outputSlowState *= 0.995f;
            outputHpState *= 0.995f;
            outputHpInputState *= 0.995f;
            leftValue = 0.0f;
            rightValue = 0.0f;
            for (int strip = 0; strip < (int) mixerStrips.size(); ++strip)
            {
                const auto previousLevel = publishedMixerLevels[(size_t) strip].load (std::memory_order_relaxed);
                publishedMixerLevels[(size_t) strip].store (previousLevel * 0.90f, std::memory_order_relaxed);
            }
        }

        left[sample] = leftValue;
        if (right != nullptr)
            right[sample] = rightValue;
    }

    if (running)
        currentTimeSeconds.store (timeSeconds);

        transportSequence.fetch_add (1, std::memory_order_acq_rel);
        publishedTransportSnapshot.snakeCount = audioSnakeCount;
        publishedTransportSnapshot.automataMode = currentMode == AppMode::cellularGrid;
        publishedTransportSnapshot.automataCurrent = automataCurrent;
        publishedTransportSnapshot.automataPrevious = automataPrevious;
        publishedTransportSnapshot.transportPhase = transportPhase;
        for (int i = 0; i < maxSnakes; ++i)
            publishedTransportSnapshot.snakes[(size_t) i] = audioSnakes[(size_t) i];
    transportSequence.fetch_add (1, std::memory_order_release);
}

void MainComponent::releaseResources()
{
    releasePluginInstances();
}

void MainComponent::timerCallback()
{
    const auto timeSeconds = currentTimeSeconds.load();
    lastScore = evaluateScore (timeSeconds);
    stage.setState (lastScore);
    const auto patchSnapshot = getPatchSnapshot();
    const auto bpmSnapshot = patchSnapshot != nullptr ? patchSnapshot->bpm : bpm;
    const auto transportSnapshot = getTransportSnapshot();
    beatValue.setText ("", juce::dontSendNotification);
    stepLabel.setText ("Snakes " + juce::String (transportSnapshot.snakeCount), juce::dontSendNotification);
    for (int strip = 0; strip < (int) mixerVolumeSliders.size(); ++strip)
        mixerVolumeSliders[(size_t) strip].setMeterLevel (publishedMixerLevels[(size_t) strip].load (std::memory_order_relaxed));
    updateCellButtons();
}

void MainComponent::buttonClicked (juce::Button* button)
{
    if (button == &playButton)
    {
        const auto running = ! isRunning.load();
        isRunning.store (running);
        if (running)
        {
            const auto bpmSnapshot = bpm;
            const auto tick = (int) std::floor (currentTimeSeconds.load() * (bpmSnapshot / 60.0) * 4.0);
            lastAdvancedTick.store (tick - 1);
            resetSynthVoices();
            resetDrumVoices();
        }
        else
        {
            resetSynthVoices();
            resetDrumVoices();
        }
        playButton.setButtonText (running ? "Stop" : "Play");
        audioValue.setText ("", juce::dontSendNotification);
        return;
    }

    if (button == &resumeModeButton)
    {
        showingTitleScreen = false;
        resized();
        repaint();
        return;
    }

    if (button == &rescanPluginsButton)
    {
        scanLogicPluginFolders();
        updateMixerButtons();
        repaint();
        return;
    }

    if (button == &a1ModeButton) { applyMode (AppMode::glyphGrid, ModeVariant::a); return; }
    if (button == &a2ModeButton) { applyMode (AppMode::cellularGrid, ModeVariant::a); return; }
    if (button == &a3ModeButton) { applyMode (AppMode::harmonicGeometry, ModeVariant::a); return; }
    if (button == &a4ModeButton) { applyMode (AppMode::harmonySpace, ModeVariant::a); return; }
    if (button == &b1ModeButton) { applyMode (AppMode::glyphGrid, ModeVariant::b); return; }
    if (button == &b2ModeButton) { applyMode (AppMode::cellularGrid, ModeVariant::b); return; }
    if (button == &b3ModeButton) { applyMode (AppMode::harmonicGeometry, ModeVariant::b); return; }
    if (button == &b4ModeButton) { applyMode (AppMode::harmonySpace, ModeVariant::b); return; }

    if (button == &clearButton)
    {
        for (auto& cell : grid)
        {
            cell.type = GlyphType::empty;
            cell.label = getGlyphDefinition (GlyphType::empty).label;
            cell.code = getGlyphDefinition (GlyphType::empty).defaultCode;
            recompileCell (cell);
        }
        initialiseSnakes();
        lastAdvancedTick.store (-1);
        resetSynthVoices();
        resetDrumVoices();
        publishPatchSnapshot();
        updateCellButtons();
        renderInspector();
        return;
    }

    if (button == &spawnSnakeButton)
    {
        if (currentMode == AppMode::cellularGrid)
            seedAutomataBurst();
        else
            spawnSnake();
        updateCellButtons();
        return;
    }

    if (button == &presetButton)
    {
        auto& presetIndex = modePresetIndices[(size_t) modeIndex (currentMode)];
        presetIndex = (presetIndex + 1) % 3;
        applyMode (currentMode, currentVariant, false);
        return;
    }

    if (button == &caRuleButton)
    {
        const auto nextRule = ((int) currentCellularRule + 1) % 4;
        currentCellularRule = (CellularRule) nextRule;
        caRuleButton.setButtonText ("Rule: " + cellularRuleName (currentCellularRule));
        repaint();
        return;
    }

    if (button == &caBirthButton)
    {
        static constexpr std::array<CellularRuleRange, 5> options {{
            { 4, 4 }, { 5, 5 }, { 5, 6 }, { 6, 7 }, { 4, 6 }
        }};
        auto currentIndex = 0;
        for (int i = 0; i < (int) options.size(); ++i)
            if (options[(size_t) i].min == currentCellularBirthRange.min && options[(size_t) i].max == currentCellularBirthRange.max)
                currentIndex = i;
        currentCellularBirthRange = options[(size_t) ((currentIndex + 1) % (int) options.size())];
        caBirthButton.setButtonText (cellularRangeLabel ("Birth", currentCellularBirthRange));
        repaint();
        return;
    }

    if (button == &caSurviveButton)
    {
        static constexpr std::array<CellularRuleRange, 5> options {{
            { 3, 4 }, { 4, 5 }, { 5, 7 }, { 4, 6 }, { 5, 6 }
        }};
        auto currentIndex = 0;
        for (int i = 0; i < (int) options.size(); ++i)
            if (options[(size_t) i].min == currentCellularSurviveRange.min && options[(size_t) i].max == currentCellularSurviveRange.max)
                currentIndex = i;
        currentCellularSurviveRange = options[(size_t) ((currentIndex + 1) % (int) options.size())];
        caSurviveButton.setButtonText (cellularRangeLabel ("Stay", currentCellularSurviveRange));
        repaint();
        return;
    }

    if (button == &mixerToggleButton)
    {
        mixerVisible = ! mixerVisible;
        resized();
        repaint();
        return;
    }

    for (int strip = 0; strip < (int) mixerStrips.size(); ++strip)
    {
        auto clearSlot = [this, strip] (PluginSlotKind slotKind)
        {
            auto& slot = getHostedPluginSlot (strip, slotKind);
            if (slot.instance != nullptr)
                slot.instance->releaseResources();

            const auto slotIndex = [slotKind]
            {
                switch (slotKind)
                {
                    case PluginSlotKind::midiFx: return 0;
                    case PluginSlotKind::instrument: return 1;
                    case PluginSlotKind::effectA: return 2;
                    case PluginSlotKind::effectB: return 3;
                    case PluginSlotKind::effectC: return 4;
                    case PluginSlotKind::effectD: return 5;
                }
                return 0;
            }();

            mixerStrips[(size_t) strip].pluginWindows[(size_t) slotIndex].reset();
            slot = {};
            slot.buttonText = "Empty";
        };

        auto swapEffects = [this, strip] (PluginSlotKind firstKind, PluginSlotKind secondKind)
        {
            auto& first = getHostedPluginSlot (strip, firstKind);
            auto& second = getHostedPluginSlot (strip, secondKind);
            if (first.instance == nullptr && second.instance == nullptr)
                return;

            const auto firstIndex = (int) firstKind - (int) PluginSlotKind::midiFx;
            const auto secondIndex = (int) secondKind - (int) PluginSlotKind::midiFx;
            mixerStrips[(size_t) strip].pluginWindows[(size_t) firstIndex].reset();
            mixerStrips[(size_t) strip].pluginWindows[(size_t) secondIndex].reset();
            std::swap (first, second);
        };

        if (button == &mixerMidiFxGuiButtons[(size_t) strip])
        {
            showPluginEditor (strip, PluginSlotKind::midiFx);
            return;
        }

        if (button == &mixerInstrumentGuiButtons[(size_t) strip])
        {
            showPluginEditor (strip, PluginSlotKind::instrument);
            return;
        }

        if (button == &mixerEffectAGuiButtons[(size_t) strip])
        {
            showPluginEditor (strip, PluginSlotKind::effectA);
            return;
        }

        if (button == &mixerEffectBGuiButtons[(size_t) strip])
        {
            showPluginEditor (strip, PluginSlotKind::effectB);
            return;
        }

        if (button == &mixerEffectCGuiButtons[(size_t) strip])
        {
            showPluginEditor (strip, PluginSlotKind::effectC);
            return;
        }

        if (button == &mixerEffectDGuiButtons[(size_t) strip])
        {
            showPluginEditor (strip, PluginSlotKind::effectD);
            return;
        }

        const auto toggleBypass = [&] (PluginSlotKind slotKind)
        {
            auto& slot = getHostedPluginSlot (strip, slotKind);
            if (slot.instance == nullptr)
                return false;
            slot.bypassed = ! slot.bypassed;
            updateMixerButtons();
            return true;
        };

        if (button == &mixerMidiFxBypassButtons[(size_t) strip] && toggleBypass (PluginSlotKind::midiFx)) return;
        if (button == &mixerInstrumentBypassButtons[(size_t) strip] && toggleBypass (PluginSlotKind::instrument)) return;
        if (button == &mixerEffectABypassButtons[(size_t) strip] && toggleBypass (PluginSlotKind::effectA)) return;
        if (button == &mixerEffectBBypassButtons[(size_t) strip] && toggleBypass (PluginSlotKind::effectB)) return;
        if (button == &mixerEffectCBypassButtons[(size_t) strip] && toggleBypass (PluginSlotKind::effectC)) return;
        if (button == &mixerEffectDBypassButtons[(size_t) strip] && toggleBypass (PluginSlotKind::effectD)) return;

        if (button == &mixerMidiFxRemoveButtons[(size_t) strip]) { clearSlot (PluginSlotKind::midiFx); updateMixerButtons(); return; }
        if (button == &mixerInstrumentRemoveButtons[(size_t) strip]) { clearSlot (PluginSlotKind::instrument); updateMixerButtons(); return; }
        if (button == &mixerEffectARemoveButtons[(size_t) strip]) { clearSlot (PluginSlotKind::effectA); updateMixerButtons(); return; }
        if (button == &mixerEffectBRemoveButtons[(size_t) strip]) { clearSlot (PluginSlotKind::effectB); updateMixerButtons(); return; }
        if (button == &mixerEffectCRemoveButtons[(size_t) strip]) { clearSlot (PluginSlotKind::effectC); updateMixerButtons(); return; }
        if (button == &mixerEffectDRemoveButtons[(size_t) strip]) { clearSlot (PluginSlotKind::effectD); updateMixerButtons(); return; }

        if (button == &mixerEffectBUpButtons[(size_t) strip]) { swapEffects (PluginSlotKind::effectA, PluginSlotKind::effectB); updateMixerButtons(); return; }
        if (button == &mixerEffectCUpButtons[(size_t) strip]) { swapEffects (PluginSlotKind::effectB, PluginSlotKind::effectC); updateMixerButtons(); return; }
        if (button == &mixerEffectDUpButtons[(size_t) strip]) { swapEffects (PluginSlotKind::effectC, PluginSlotKind::effectD); updateMixerButtons(); return; }
        if (button == &mixerEffectADownButtons[(size_t) strip]) { swapEffects (PluginSlotKind::effectA, PluginSlotKind::effectB); updateMixerButtons(); return; }
        if (button == &mixerEffectBDownButtons[(size_t) strip]) { swapEffects (PluginSlotKind::effectB, PluginSlotKind::effectC); updateMixerButtons(); return; }
        if (button == &mixerEffectCDownButtons[(size_t) strip]) { swapEffects (PluginSlotKind::effectC, PluginSlotKind::effectD); updateMixerButtons(); return; }
    }

    if (button == &parameterPickerButton && selectedCell != nullptr && selectedCell->type == GlyphType::parameter)
    {
        juce::PopupMenu menu;
        int itemId = 1;
        std::vector<juce::String> parameterCodes;

        juce::PopupMenu routedStripMenu;
        for (auto slotKind : { PluginSlotKind::instrument, PluginSlotKind::effectA, PluginSlotKind::effectB,
                               PluginSlotKind::effectC, PluginSlotKind::effectD, PluginSlotKind::midiFx })
        {
            juce::PopupMenu slotMenu;
            for (int strip = 0; strip < (int) mixerStrips.size(); ++strip)
            {
                const auto entries = getParameterChoicesForSlot (strip, slotKind);
                for (const auto& entry : entries)
                {
                    auto parameterToken = entry.upToFirstOccurrenceOf (" ", false, false);
                    if (const auto idStart = entry.indexOf ("[id:"); idStart >= 0)
                    {
                        const auto idText = entry.fromFirstOccurrenceOf ("[id:", false, false)
                                                .upToLastOccurrenceOf ("]", false, false);
                        parameterToken = "id:" + idText;
                    }

                    parameterCodes.push_back (mixerSlotPrefix (slotKind).toLowerCase() + ":" + parameterToken + "=0.50");
                    slotMenu.addItem (itemId++, "Strip " + juce::String (strip + 1) + " - " + entry);
                }
            }

            if (slotMenu.getNumItems() > 0)
                routedStripMenu.addSubMenu (mixerSlotPrefix (slotKind), slotMenu);
        }

        if (routedStripMenu.getNumItems() > 0)
            menu.addSubMenu ("Routed Strip", routedStripMenu);

        for (int strip = 0; strip < (int) mixerStrips.size(); ++strip)
        {
            juce::PopupMenu stripMenu;

            for (auto slotKind : { PluginSlotKind::instrument, PluginSlotKind::effectA, PluginSlotKind::effectB,
                                   PluginSlotKind::effectC, PluginSlotKind::effectD, PluginSlotKind::midiFx })
            {
                const auto entries = getParameterChoicesForSlot (strip, slotKind);
                if (entries.isEmpty())
                    continue;

                juce::PopupMenu slotMenu;
                for (const auto& entry : entries)
                {
                    auto parameterToken = entry.upToFirstOccurrenceOf (" ", false, false);
                    if (const auto idStart = entry.indexOf ("[id:"); idStart >= 0)
                    {
                        const auto idText = entry.fromFirstOccurrenceOf ("[id:", false, false)
                                                .upToLastOccurrenceOf ("]", false, false);
                        parameterToken = "id:" + idText;
                    }

                    parameterCodes.push_back ("s" + juce::String (strip + 1) + ":"
                                              + mixerSlotPrefix (slotKind).toLowerCase() + ":"
                                              + parameterToken + "=0.50");
                    slotMenu.addItem (itemId++, entry);
                }

                stripMenu.addSubMenu (mixerSlotPrefix (slotKind), slotMenu);
            }

            if (stripMenu.getNumItems() > 0)
                menu.addSubMenu ("Strip " + juce::String (strip + 1), stripMenu);
        }

        if (parameterCodes.empty())
        {
            menu.addItem (1, "No plugin parameters available", false, false);
            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&parameterPickerButton), [] (int) {});
            return;
        }

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&parameterPickerButton),
                            [this, parameterCodes] (int result)
                            {
                                if (result <= 0 || result > (int) parameterCodes.size() || selectedCell == nullptr)
                                    return;

                                selectedCell->code = parameterCodes[(size_t) (result - 1)];
                                recompileCell (*selectedCell);
                                codeEditor.setText (selectedCell->code, juce::dontSendNotification);
                                publishPatchSnapshot();
                                updateCellButtons();
                                renderInspector();
                            });
        return;
    }

    if (button == &saveButton)
    {
        activeFileChooser = std::make_unique<juce::FileChooser> ("Save GlyphGrid Patch",
                                                                 juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                                                                     .getChildFile (currentPatchName + ".glyphgrid.json"),
                                                                 "*.glyphgrid.json");
        activeFileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                        | juce::FileBrowserComponent::canSelectFiles
                                        | juce::FileBrowserComponent::warnAboutOverwriting,
                                        [this] (const juce::FileChooser& chooser)
                                        {
                                            const auto result = chooser.getResult();
                                            activeFileChooser.reset();

                                            if (result != juce::File())
                                                savePatchToFile (result);
                                        });
        return;
    }

    if (button == &loadButton)
    {
        activeFileChooser = std::make_unique<juce::FileChooser> ("Load GlyphGrid Patch",
                                                                 juce::File::getSpecialLocation (juce::File::userDocumentsDirectory),
                                                                 "*.glyphgrid.json");
        activeFileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                        | juce::FileBrowserComponent::canSelectFiles,
                                        [this] (const juce::FileChooser& chooser)
                                        {
                                            const auto result = chooser.getResult();
                                            activeFileChooser.reset();

                                            if (result != juce::File())
                                                loadPatchFromFile (result);
                                        });
        return;
    }

    if (button == &sidebarToggleButton)
    {
        isSidebarExpanded = ! isSidebarExpanded;
        sidebarToggleButton.setButtonText (isSidebarExpanded ? "Hide" : "...");
        resized();
        repaint();
        return;
    }

    if (button == &layerDownButton)
    {
        visibleLayer = juce::jmax (0, visibleLayer - 1);
        selectedCell = getCell (visibleLayer,
                                selectedCell != nullptr ? selectedCell->row : 0,
                                selectedCell != nullptr ? selectedCell->col : 0);
        renderInspector();
        updateCellButtons();
        return;
    }

    if (button == &layerUpButton)
    {
        visibleLayer = juce::jmin (layers - 1, visibleLayer + 1);
        selectedCell = getCell (visibleLayer,
                                selectedCell != nullptr ? selectedCell->row : 0,
                                selectedCell != nullptr ? selectedCell->col : 0);
        renderInspector();
        updateCellButtons();
        return;
    }

    if (button == &eraseButton && selectedCell != nullptr)
    {
        selectedCell->type = GlyphType::empty;
        selectedCell->label = getGlyphDefinition (GlyphType::empty).label;
        selectedCell->code = getGlyphDefinition (GlyphType::empty).defaultCode;
        recompileCell (*selectedCell);
        publishPatchSnapshot();
        updateCellButtons();
        renderInspector();
        return;
    }

    for (auto* toolButton : toolButtons)
    {
        if (button == toolButton)
        {
            selectedTool = toolIdToGlyph (toolButton->getComponentID());
            toolValue.setText ("", juce::dontSendNotification);
            return;
        }
    }

    for (int strip = 0; strip < (int) mixerStrips.size(); ++strip)
    {
        for (auto slotKind : { PluginSlotKind::midiFx, PluginSlotKind::instrument, PluginSlotKind::effectA,
                               PluginSlotKind::effectB, PluginSlotKind::effectC, PluginSlotKind::effectD })
        {
            auto* slotButton = slotKind == PluginSlotKind::midiFx ? &mixerMidiFxButtons[(size_t) strip]
                              : (slotKind == PluginSlotKind::instrument ? &mixerInstrumentButtons[(size_t) strip]
                              : (slotKind == PluginSlotKind::effectA ? &mixerEffectAButtons[(size_t) strip]
                              : (slotKind == PluginSlotKind::effectB ? &mixerEffectBButtons[(size_t) strip]
                              : (slotKind == PluginSlotKind::effectC ? &mixerEffectCButtons[(size_t) strip]
                                                                     : &mixerEffectDButtons[(size_t) strip]))));
            if (button != slotButton)
                continue;

            auto choices = getPluginChoicesForSlot (slotKind);
            juce::PopupMenu menu;
            menu.addItem (1, "Empty");
            menu.addSeparator();
            for (int i = 0; i < choices.size(); ++i)
                menu.addItem (100 + i, choices.getReference (i).name);

            menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (slotButton),
                                [this, strip, slotKind, choices] (int result)
                                {
                                    if (result <= 0)
                                        return;

                                    if (result == 1)
                                    {
                                        auto& slot = getHostedPluginSlot (strip, slotKind);
                                        if (slot.instance != nullptr)
                                            slot.instance->releaseResources();
                                        const auto slotIndex = (int) slotKind - (int) PluginSlotKind::midiFx;
                                        mixerStrips[(size_t) strip].pluginWindows[(size_t) slotIndex].reset();
                                        slot = {};
                                        slot.buttonText = "Empty";
                                        updateMixerButtons();
                                        return;
                                    }

                                    const auto choiceIndex = result - 100;
                                    if (juce::isPositiveAndBelow (choiceIndex, choices.size()))
                                        loadPluginIntoSlot (strip, slotKind, choices.getReference (choiceIndex));
                                });
            return;
        }
    }
}

void MainComponent::textEditorTextChanged (juce::TextEditor& editor)
{
    if (selectedCell == nullptr)
        return;

    if (&editor == &labelEditor)
        selectedCell->label = editor.getText();
    else if (&editor == &codeEditor)
    {
        selectedCell->code = editor.getText();
        recompileCell (*selectedCell);
    }

    publishPatchSnapshot();
    updateCellButtons();
}

void MainComponent::textEditorFocusLost (juce::TextEditor&) {}
void MainComponent::textEditorReturnKeyPressed (juce::TextEditor&) {}

int MainComponent::getCellIndex (int layer, int row, int col) const noexcept
{
    return ((juce::jlimit (0, layers - 1, layer) * rows) + juce::jlimit (0, rows - 1, row)) * cols + juce::jlimit (0, cols - 1, col);
}

MainComponent::Cell* MainComponent::getCell (int layer, int row, int col) noexcept
{
    return grid.isEmpty() ? nullptr : &grid.getReference (getCellIndex (layer, row, col));
}

const MainComponent::Cell* MainComponent::getCell (int layer, int row, int col) const noexcept
{
    return grid.isEmpty() ? nullptr : &grid.getReference (getCellIndex (layer, row, col));
}

void MainComponent::renderInspector()
{
    if (selectedCell == nullptr)
    {
        inspectorTitle.setText ("Select a cell", juce::dontSendNotification);
        glyphValueLabel.setText ("", juce::dontSendNotification);
        labelEditor.setText ("");
        codeEditor.setText ("");
        parameterPickerButton.setVisible (false);
        return;
    }

    inspectorTitle.setText (selectedCell->label + " L" + juce::String (selectedCell->layer + 1) + " "
                                + juce::String (selectedCell->row + 1) + ":" + juce::String (selectedCell->col + 1),
                            juce::dontSendNotification);
    glyphValueLabel.setText ("Glyph " + glyphTypeToString (selectedCell->type), juce::dontSendNotification);
    labelEditor.setText (selectedCell->label, juce::dontSendNotification);
    codeEditor.setText (selectedCell->code, juce::dontSendNotification);
    parameterPickerButton.setVisible (isPluginMode() && selectedCell->type == GlyphType::parameter);
}

void MainComponent::updateCellButtons()
{
    const auto transportSnapshot = getTransportSnapshot();

    for (int i = 0; i < cellButtons.size(); ++i)
    {
        auto* button = cellButtons[i];
        auto* visibleCell = getCell (visibleLayer, button->row, button->col);
        button->isSelected = (visibleCell != nullptr && visibleCell == selectedCell);
        button->type = visibleCell != nullptr ? visibleCell->type : GlyphType::empty;
        button->code = visibleCell != nullptr ? visibleCell->code : juce::String();
        button->hiddenLayerBadges.clear();
        button->hiddenLayerBadgeColours.clear();
        button->overlayBadges.clear();
        button->overlayBadgeColours.clear();
        button->hasAnyLayerContent = false;
        button->hasHiddenLayerContent = false;
        button->isActiveColumn = false;
        button->isSnakeHead = false;
        button->isGhostSnake = false;
        button->showsNextStep = false;
        button->isAutomataActive = false;
        button->isAutomataNewborn = false;
        button->pluginActivity = 0.0f;
        button->snakeDirectionX = 0;
        button->snakeDirectionY = 0;
        button->snakeDirectionLayer = 0;
        button->snakeColour = juce::Colours::transparentBlack;

        juce::Array<GlyphType> hiddenTypes;

        for (int layer = 0; layer < layers; ++layer)
        {
            if (const auto* stackedCell = getCell (layer, button->row, button->col);
                stackedCell != nullptr && stackedCell->type != GlyphType::empty)
            {
                button->hasAnyLayerContent = true;

                if (layer != visibleLayer)
                {
                    button->hasHiddenLayerContent = true;
                    hiddenTypes.addIfNotAlreadyThere (stackedCell->type);
                }
            }
        }

        if (! hiddenTypes.isEmpty())
        {
            const auto maxBadges = 3;

            for (int i = 0; i < juce::jmin (maxBadges, hiddenTypes.size()); ++i)
            {
                const auto hiddenType = hiddenTypes.getReference (i);
                button->hiddenLayerBadges.add (getGlyphDefinition (hiddenType).shortName);
                button->hiddenLayerBadgeColours.add (MainComponent::hiddenBadgeColour (hiddenType));
            }

            if (hiddenTypes.size() > maxBadges)
            {
                button->hiddenLayerBadges.add ("+" + juce::String (hiddenTypes.size() - maxBadges));
                button->hiddenLayerBadgeColours.add (juce::Colours::white.withAlpha (0.8f));
            }
        }

        for (int snakeIndex = 0; snakeIndex < transportSnapshot.snakeCount; ++snakeIndex)
        {
            const auto& snake = transportSnapshot.snakes[(size_t) snakeIndex];
            const auto snakeColour = snake.colour;
            const auto isGhost = false;

            for (int segmentIndex = 0; segmentIndex < snake.length; ++segmentIndex)
            {
                const auto& segment = snake.segments[(size_t) segmentIndex];
                if (segment.layer == visibleLayer && segment.row == button->row && segment.col == button->col)
                {
                    button->isActiveColumn = true;
                    button->snakeColour = snakeColour;
                    button->isGhostSnake = isGhost;
                    if (segmentIndex == 0)
                    {
                        button->isSnakeHead = true;
                        button->snakeDirectionX = snake.directionX;
                        button->snakeDirectionY = snake.directionY;
                        button->snakeDirectionLayer = snake.directionLayer;
                    }
                    break;
                }
            }

            const auto head = snake.segments[0];
            const auto nextCol = juce::jlimit (0, cols - 1, head.col + snake.directionX);
            const auto nextRow = juce::jlimit (0, rows - 1, head.row + snake.directionY);
            if (! isGhost
                && head.layer == visibleLayer
                && nextRow == button->row
                && nextCol == button->col
                && (snake.directionX != 0 || snake.directionY != 0))
            {
                button->showsNextStep = true;
                if (button->snakeColour.isTransparent())
                    button->snakeColour = snakeColour;
            }
        }

        if (transportSnapshot.automataMode)
        {
            const auto index = (size_t) getCellIndex (visibleLayer, button->row, button->col);
            button->isAutomataActive = transportSnapshot.automataCurrent[index] != 0;
            button->isAutomataNewborn = button->isAutomataActive && transportSnapshot.automataPrevious[index] == 0;
        }

        if (visibleCell != nullptr)
        {
            switch (visibleCell->type)
            {
                case GlyphType::route:
                {
                    const auto routeIndex = juce::jlimit (1, 3, visibleCell->code.getIntValue());
                    button->overlayBadges.add ("S" + juce::String (routeIndex));
                    button->overlayBadgeColours.add (juce::Colour (0xff7fe8ff));
                    if (isPluginMode())
                        button->pluginActivity = publishedMixerLevels[(size_t) (routeIndex - 1)].load();
                    break;
                }
                case GlyphType::channel:
                {
                    const auto channel = juce::jlimit (1, 16, visibleCell->code.getIntValue());
                    button->overlayBadges.add ("CH" + juce::String (channel));
                    button->overlayBadgeColours.add (juce::Colour (0xff57b6ff));
                    break;
                }
                case GlyphType::cc:
                {
                    int ccNumber = 0, ccValue = 0;
                    parseCcSpec (visibleCell->code, ccNumber, ccValue);
                    button->overlayBadges.add ("CC" + juce::String (ccNumber));
                    button->overlayBadgeColours.add (juce::Colour (0xff6bffd5));
                    break;
                }
                case GlyphType::parameter:
                {
                    auto explicitStripIndex = -1;
                    juce::String slotQualifier, parameterToken;
                    auto normalizedValue = 0.0f;
                    auto hasExplicitValue = false;
                    parseParameterSpec (visibleCell->code, explicitStripIndex, slotQualifier, parameterToken, normalizedValue, hasExplicitValue);
                    button->overlayBadges.add (explicitStripIndex >= 0 ? "S" + juce::String (explicitStripIndex + 1) : "AUTO");
                    if (slotQualifier.isNotEmpty())
                        button->overlayBadges.add (slotQualifier.toUpperCase());
                    button->overlayBadgeColours.add (juce::Colour (0xffffd46b));
                    if (slotQualifier.isNotEmpty())
                        button->overlayBadgeColours.add (juce::Colour (0xffffc15b));
                    if (isPluginMode())
                    {
                        if (explicitStripIndex >= 0)
                            button->pluginActivity = publishedMixerLevels[(size_t) explicitStripIndex].load();
                        else
                            button->pluginActivity = juce::jmax (publishedMixerLevels[0].load(),
                                                                 juce::jmax (publishedMixerLevels[1].load(), publishedMixerLevels[2].load()));
                    }
                    break;
                }
                default:
                    break;
            }
        }

        button->repaint();
    }
}

void MainComponent::selectCell (Cell* cell)
{
    selectedCell = cell;
    renderInspector();
    updateCellButtons();
}

void MainComponent::applySelectedToolToCell (Cell& cell)
{
    if (cell.type == selectedTool)
        return;

    cell.type = selectedTool;
    auto def = getGlyphDefinition (selectedTool);
    cell.label = def.label;
    cell.code = def.defaultCode;
    recompileCell (cell);
    publishPatchSnapshot();
}

MainComponent::ScoreResult MainComponent::evaluateScore (double timeSeconds) const noexcept
{
    auto patchSnapshot = getPatchSnapshot();

    if (patchSnapshot == nullptr)
        return {};

    auto transportSnapshot = getTransportSnapshot();
    juce::Array<Cell> gridSnapshot;
    juce::Array<Snake> snakeSnapshot;
    juce::Array<SnakeSegment> explicitTriggeredCells;
    gridSnapshot.ensureStorageAllocated (cols * rows * layers);

    for (auto& cell : patchSnapshot->cells)
        gridSnapshot.add (cell);

    for (int snakeIndex = 0; snakeIndex < transportSnapshot.snakeCount; ++snakeIndex)
    {
        const auto& runtimeSnake = transportSnapshot.snakes[(size_t) snakeIndex];
        Snake snake;
        snake.directionX = runtimeSnake.directionX;
        snake.directionY = runtimeSnake.directionY;
        snake.directionLayer = runtimeSnake.directionLayer;
        snake.lengthDelta = runtimeSnake.lengthDelta;
        snake.lengthPulse = juce::jlimit (0.0f, 1.0f, runtimeSnake.recentLengthChangeTicks / 8.0f);
        snake.colour = runtimeSnake.colour;

        for (int segmentIndex = 0; segmentIndex < runtimeSnake.length; ++segmentIndex)
        {
            snake.segments.add (runtimeSnake.segments[(size_t) segmentIndex]);
            snake.previousSegments.add (runtimeSnake.previousSegments[(size_t) segmentIndex]);
        }

        snakeSnapshot.add (snake);
    }

    if (currentMode == AppMode::cellularGrid)
    {
        for (int i = 0; i < cols * rows * layers; ++i)
        {
            if (transportSnapshot.automataCurrent[(size_t) i] == 0)
                continue;

            const auto layer = i / (rows * cols);
            const auto row = (i / cols) % rows;
            const auto col = i % cols;
            explicitTriggeredCells.add ({ layer, row, col });
        }
    }
    else if (transportSnapshot.snakeCount > 0)
    {
        Snake ghostSnake;
        ghostSnake.isGhost = true;
        ghostSnake.colour = juce::Colours::white.withAlpha (0.48f);

        for (int segmentIndex = 0; segmentIndex < snakeLength; ++segmentIndex)
        {
            std::array<int, maxSnakes> layersForSegment {};
            std::array<int, maxSnakes> rowsForSegment {};
            std::array<int, maxSnakes> colsForSegment {};
            std::array<int, maxSnakes> prevLayersForSegment {};
            std::array<int, maxSnakes> prevRowsForSegment {};
            std::array<int, maxSnakes> prevColsForSegment {};
            auto count = 0;

            for (int snakeIndex = 0; snakeIndex < transportSnapshot.snakeCount; ++snakeIndex)
            {
                const auto& runtimeSnake = transportSnapshot.snakes[(size_t) snakeIndex];

                if (segmentIndex >= runtimeSnake.length)
                    continue;

                const auto& seg = runtimeSnake.segments[(size_t) segmentIndex];
                const auto& prevSeg = runtimeSnake.previousSegments[(size_t) segmentIndex];
                layersForSegment[(size_t) count] = seg.layer;
                rowsForSegment[(size_t) count] = seg.row;
                colsForSegment[(size_t) count] = seg.col;
                prevLayersForSegment[(size_t) count] = prevSeg.layer;
                prevRowsForSegment[(size_t) count] = prevSeg.row;
                prevColsForSegment[(size_t) count] = prevSeg.col;
                ++count;
            }

            if (count == 0)
                break;

            auto orderInts = [count] (auto& values)
            {
                std::sort (values.begin(), values.begin() + count);
                return values[(size_t) (count / 2)];
            };

            ghostSnake.segments.add ({ orderInts (layersForSegment), orderInts (rowsForSegment), orderInts (colsForSegment) });
            ghostSnake.previousSegments.add ({ orderInts (prevLayersForSegment), orderInts (prevRowsForSegment), orderInts (prevColsForSegment) });
        }

        if (! ghostSnake.segments.isEmpty())
        {
            if (ghostSnake.previousSegments.size() != ghostSnake.segments.size())
                ghostSnake.previousSegments = ghostSnake.segments;
            snakeSnapshot.add (ghostSnake);
        }
    }

    auto result = evaluateScoreForGrid (gridSnapshot,
                                        snakeSnapshot,
                                        currentMode == AppMode::cellularGrid ? &explicitTriggeredCells : nullptr,
                                        patchSnapshot->bpm,
                                        timeSeconds);
    result.transportPhase = transportSnapshot.transportPhase;
    result.activeLayer = visibleLayer;
    result.cellularRule = (int) currentCellularRule;
    result.pluginMode = isPluginMode();
    for (int i = 0; i < 3; ++i)
        result.mixerLevels[(size_t) i] = publishedMixerLevels[(size_t) i].load();
    result.mode = currentMode;
    result.harmonySpaceKeyCenter = harmonySpaceKeyCenter;
    result.harmonySpaceConstraintMode = harmonySpaceConstraintMode;
    result.harmonySpaceGestureRecordEnabled = harmonySpaceGestureRecordEnabled;
    result.harmonySpaceGesturePoints = harmonySpaceGesturePoints;
    return result;
}

MainComponent::ScoreResult MainComponent::evaluateScoreForGrid (const juce::Array<Cell>& sourceGrid,
                                                                const juce::Array<Snake>& snakeSnapshot,
                                                                const juce::Array<SnakeSegment>* explicitTriggeredCells,
                                                                double bpmValue,
                                                                double timeSeconds) const noexcept
{
    ScoreResult result;
    const auto beat = timeSeconds * (bpmValue / 60.0);
    const auto stepTime = timeSeconds * (bpmValue / 60.0) * 4.0;
    const auto stepPhase = static_cast<float> (stepTime - std::floor (stepTime));
    const auto gateEnvelope = std::sin (juce::MathConstants<float>::pi * juce::jlimit (0.0f, 1.0f, stepPhase));
    const auto triggeredCells = explicitTriggeredCells != nullptr ? *explicitTriggeredCells
                                                                  : collectTriggeredCells (snakeSnapshot);
    std::array<juce::Array<SnakeSegment>, rows> activeSegmentsByRow;

    result.snakes = snakeSnapshot;
    result.transportPhase = stepPhase;

    for (int layer = 0; layer < layers; ++layer)
    {
        for (int row = 0; row < rows; ++row)
        {
            for (int col = 0; col < cols; ++col)
            {
                const auto& cell = sourceGrid.getReference (getCellIndex (layer, row, col));

                if (cell.type == GlyphType::empty)
                    continue;

                GridCellVisual visual;
                visual.layer = layer;
                visual.row = row;
                visual.col = col;
                visual.type = cell.type;
                visual.code = cell.code;
                result.gridCells.add (visual);
            }
        }
    }

    for (auto& triggeredCell : triggeredCells)
    {
        if (juce::isPositiveAndBelow (triggeredCell.row, rows)
            && juce::isPositiveAndBelow (triggeredCell.col, cols))
        {
            activeSegmentsByRow[(size_t) triggeredCell.row].add (triggeredCell);

            for (auto& gridCell : result.gridCells)
            {
                if (gridCell.layer == triggeredCell.layer
                    && gridCell.row == triggeredCell.row
                    && gridCell.col == triggeredCell.col)
                {
                    gridCell.isTriggered = true;
                    break;
                }
            }
        }
    }

    for (int row = 0; row < rows; ++row)
    {
        auto& activeSegments = activeSegmentsByRow[(size_t) row];

        if (activeSegments.isEmpty())
            continue;

        for (auto activeSegment : activeSegments)
        {
            double x = 0.0;
            float rowHue = result.hue;
            auto chainActive = false;

            for (int col = 0; col < cols; ++col)
            {
                for (int layer = 0; layer < layers; ++layer)
                {
                    auto& cell = sourceGrid.getReference (getCellIndex (layer, row, col));
                    const auto triggeredHere = (activeSegment.col == col && activeSegment.layer == layer);
                    const auto chainReady = chainActive || triggeredHere;
                    ExpressionScope scope { x, timeSeconds, (double) row, (double) col, beat, (double) activeSegment.col };
                    auto value = ExpressionEvaluator::evaluate (cell.program, scope);

                    switch (cell.type)
                    {
                        case GlyphType::pulse: x = (triggeredHere ? value * gateEnvelope : 0.0); break;
                        case GlyphType::tone:  x = chainReady ? std::sin (timeSeconds * juce::MathConstants<double>::twoPi * std::max (1.0, value)) * gateEnvelope : 0.0; break;
                        case GlyphType::voice: x = chainReady ? value * gateEnvelope : 0.0; break;
                        case GlyphType::chord:
                        case GlyphType::key:
                        case GlyphType::octave:
                        case GlyphType::route:
                        case GlyphType::channel:
                        case GlyphType::cc:
                        case GlyphType::parameter:
                        case GlyphType::tempo:
                        case GlyphType::ratchet:
                        case GlyphType::repeat:
                        case GlyphType::wormhole:
                        case GlyphType::length:
                        case GlyphType::accent:
                        case GlyphType::decay:
                            break;
                        case GlyphType::noise:
                        {
                            auto hash = std::sin ((timeSeconds * 1000.0) + row * 17.0 + col * 11.0 + layer * 7.0) * 43758.5453;
                            auto fract = hash - std::floor (hash);
                            x = chainReady ? (((fract * 2.0) - 1.0) * value) * gateEnvelope : 0.0;
                            break;
                        }
                        case GlyphType::mul:   x *= value; break;
                        case GlyphType::bias:  x += value; break;
                        case GlyphType::hue:   rowHue = juce::jlimit (0.0f, 1.0f, (float) value); break;
                        case GlyphType::audio: result.audio += (float) (x * value); break;
                        case GlyphType::kick:
                        case GlyphType::snare:
                        case GlyphType::hat:
                        case GlyphType::clap:
                        {
                            if (chainReady)
                            {
                                VisualLine line;
                                line.layer = activeSegment.layer;
                                line.row = row;
                                line.step = col;
                                line.hue = std::fmod (rowHue + 0.08f * layer, 1.0f);
                                line.energy = juce::jlimit (0.0f, 1.2f, (float) std::abs (value) * 0.85f);
                                result.lines.add (line);
                                result.energy += line.energy * 0.5f;
                            }
                            break;
                        }
                        case GlyphType::visual:
                        {
                            VisualLine line;
                            line.layer = activeSegment.layer;
                            line.row = row;
                            line.step = activeSegment.col;
                            line.hue = rowHue;
                            line.energy = juce::jlimit (0.0f, 1.4f, (float) (std::abs (x) * value));
                            result.lines.add (line);
                            result.energy += line.energy;
                            break;
                        }
                        case GlyphType::empty: break;
                    }

                    chainActive = chainReady;
                }
            }

            result.hue = rowHue;
        }
    }

    result.audio = juce::jlimit (-1.0f, 1.0f, result.audio * 0.45f);
    result.energy = juce::jlimit (0.0f, 1.5f, result.energy);
    return result;
}

float MainComponent::evaluateAudioSample (double timeSeconds) noexcept
{
    auto patchSnapshot = getPatchSnapshot();
    return patchSnapshot != nullptr ? evaluateAudioForSnapshot (*patchSnapshot, timeSeconds) : 0.0f;
}

void MainComponent::resetSynthVoices() noexcept
{
    for (auto& voice : synthVoices)
        voice = {};
}

void MainComponent::resetDrumVoices() noexcept
{
    for (auto& voice : drumVoices)
        voice = {};
}

void MainComponent::triggerSynthVoice (float midiNote,
                                       float amplitude,
                                       float noiseMix,
                                       float brightness,
                                       float waveformMix,
                                       float decayScale,
                                       int delaySamples,
                                       int preferredStrip,
                                       int midiChannel,
                                       float pitchBend,
                                       float channelPressure,
                                       int ccNumber,
                                       int ccValue) noexcept
{
    if (isPluginMode())
    {
        juce::ignoreUnused (noiseMix, brightness, waveformMix);
        queuePluginNote (preferredStrip, midiChannel, midiNote, amplitude, decayScale, delaySamples, pitchBend, channelPressure, ccNumber, ccValue);
        return;
    }

    auto* voice = &synthVoices[0];

    for (auto& candidate : synthVoices)
    {
        if (! candidate.active || candidate.env < voice->env)
        {
            voice = &candidate;

            if (! candidate.active)
                break;
        }
    }

    const auto clampedMidi = juce::jlimit (24.0f, 108.0f, midiNote);
    const auto frequency = 440.0f * std::pow (2.0f, (clampedMidi - 69.0f) / 12.0f);
    const auto harmonySpaceMode = currentMode == AppMode::harmonySpace;
    const auto decaySeconds = harmonySpaceMode
                                  ? juce::jlimit (0.28f, 3.6f, (0.55f + (96.0f - clampedMidi) / 180.0f + waveformMix * 0.18f) * juce::jlimit (0.45f, 3.0f, decayScale))
                                  : juce::jlimit (0.06f, 2.4f, (0.16f + (108.0f - clampedMidi) / 220.0f + waveformMix * 0.25f) * juce::jlimit (0.35f, 2.5f, decayScale));
    const auto decayPerSample = std::pow (0.0008f, 1.0f / juce::jmax (1.0f, decaySeconds * (float) currentSampleRate));
    const auto cutoffHz = harmonySpaceMode
                              ? juce::jmap (juce::jlimit (0.0f, 1.0f, brightness), 220.0f, 3200.0f)
                              : juce::jmap (juce::jlimit (0.0f, 1.0f, brightness), 180.0f, 5200.0f);
    const auto filterCoeff = 1.0f - std::exp ((float) (-juce::MathConstants<double>::twoPi * cutoffHz / juce::jmax (1.0, currentSampleRate)));
    const auto attackSeconds = harmonySpaceMode ? juce::jmap (juce::jlimit (0.0f, 1.0f, waveformMix), 0.012f, 0.045f)
                                                : 0.0015f;

    *voice = {};
    voice->active = true;
    voice->stripIndex = juce::jlimit (0, (int) mixerStrips.size() - 1, preferredStrip >= 0 ? preferredStrip : resolvePreferredStrip (preferredStrip, midiNote));
    voice->frequency = frequency;
    voice->amplitude = juce::jlimit (0.0f, harmonySpaceMode ? 0.26f : 0.45f, amplitude);
    voice->env = 1.0f;
    voice->attack = harmonySpaceMode ? 0.0f : 1.0f;
    voice->attackIncrement = 1.0f / juce::jmax (1.0f, attackSeconds * (float) currentSampleRate);
    voice->decayPerSample = decayPerSample;
    voice->noiseMix = juce::jlimit (0.0f, harmonySpaceMode ? 0.16f : 0.75f, noiseMix);
    voice->brightness = juce::jlimit (0.0f, 1.0f, brightness);
    voice->filterCoeff = filterCoeff;
    voice->detuneRatio = 1.0f + juce::jmap (waveformMix, harmonySpaceMode ? 0.0004f : 0.0008f, harmonySpaceMode ? 0.0042f : 0.014f);
    voice->waveformMix = juce::jlimit (0.0f, 1.0f, waveformMix);
    voice->delaySamples = juce::jmax (0, delaySamples);
}

void MainComponent::triggerDrumVoice (DrumType type,
                                      float amplitude,
                                      float tone,
                                      float decayScale,
                                      int delaySamples,
                                      int preferredStrip,
                                      int midiChannel,
                                      float pitchBend,
                                      float channelPressure,
                                      int ccNumber,
                                      int ccValue) noexcept
{
    if (isPluginMode())
    {
        const auto midiNote = type == DrumType::kick ? 36.0f
                            : (type == DrumType::snare ? 38.0f
                            : (type == DrumType::hat ? 42.0f : 39.0f));
        queuePluginNote (preferredStrip, midiChannel, midiNote, amplitude * juce::jmap (tone, 0.6f, 1.0f), decayScale * 0.5f, delaySamples, pitchBend, channelPressure, ccNumber, ccValue);
        return;
    }

    auto* voice = &drumVoices[0];

    for (auto& candidate : drumVoices)
    {
        if (! candidate.active || candidate.env < voice->env)
        {
            voice = &candidate;

            if (! candidate.active)
                break;
        }
    }

    *voice = {};
    voice->active = true;
    voice->stripIndex = juce::jlimit (0, (int) mixerStrips.size() - 1, preferredStrip >= 0 ? preferredStrip : 2);
    voice->type = type;
    voice->amplitude = juce::jlimit (0.0f, 0.4f, amplitude);
    voice->env = 1.0f;
    voice->tone = juce::jlimit (0.0f, 1.0f, tone);
    voice->delaySamples = juce::jmax (0, delaySamples);

    switch (type)
    {
        case DrumType::kick:
            voice->frequency = juce::jmap (voice->tone, 78.0f, 112.0f);
            voice->sweep = juce::jmap (voice->tone, 88.0f, 126.0f);
            voice->decayPerSample = std::pow (0.0004f, 1.0f / juce::jmax (1.0f, (0.06f + decayScale * 0.075f) * (float) currentSampleRate));
            break;
        case DrumType::snare:
            voice->frequency = juce::jmap (voice->tone, 150.0f, 240.0f);
            voice->phaseB = 0.0f;
            voice->decayPerSample = std::pow (0.0008f, 1.0f / juce::jmax (1.0f, (0.14f + decayScale * 0.18f) * (float) currentSampleRate));
            break;
        case DrumType::hat:
            voice->frequency = juce::jmap (voice->tone, 2800.0f, 5200.0f);
            voice->phaseB = 0.17f;
            voice->decayPerSample = std::pow (0.0006f, 1.0f / juce::jmax (1.0f, (0.05f + decayScale * 0.1f) * (float) currentSampleRate));
            break;
        case DrumType::clap:
            voice->frequency = juce::jmap (voice->tone, 900.0f, 1800.0f);
            voice->decayPerSample = std::pow (0.0007f, 1.0f / juce::jmax (1.0f, (0.16f + decayScale * 0.14f) * (float) currentSampleRate));
            voice->burstStage = 0;
            voice->burstCounter = 0;
            break;
    }
}

void MainComponent::processAudioTick (const PatchSnapshot& snapshot, double timeSeconds) noexcept
{
    const auto beat = timeSeconds * (snapshot.bpm / 60.0);
    const auto samplesPerTick = juce::jmax (1, (int) std::round (currentSampleRate * 60.0 / (snapshot.bpm * 4.0)));
    const auto harmonySpaceMode = currentMode == AppMode::harmonySpace;
    const auto quantizeHarmonySpacePitch = [this] (float midiNote, int chordIndex)
    {
        if (currentMode != AppMode::harmonySpace || harmonySpaceConstraintMode == 0)
            return midiNote;

        juce::Array<int> allowedPitchClasses;
        static constexpr int majorSet[] = { 0, 2, 4, 5, 7, 9, 11 };
        static constexpr int chordSets[][4] = {
            { 0, 4, 7, -1 },
            { 0, 3, 7, -1 },
            { 0, 4, 8, -1 },
            { 0, 5, 7, -1 },
            { 0, 2, 7, -1 },
            { 0, 7, 10, -1 },
            { 0, 5, 9, -1 }
        };

        if (harmonySpaceConstraintMode >= 1)
            for (auto degree : majorSet)
                allowedPitchClasses.addIfNotAlreadyThere (wrapPitchClass (harmonySpaceKeyCenter + degree));

        if (harmonySpaceConstraintMode >= 2)
        {
            allowedPitchClasses.clearQuick();
            const auto clampedChord = juce::jlimit (0, 6, chordIndex);
            for (auto degree : chordSets[clampedChord])
                if (degree >= 0)
                    allowedPitchClasses.addIfNotAlreadyThere (wrapPitchClass (harmonySpaceKeyCenter + degree));
        }

        return snapMidiToPitchClasses (midiNote, allowedPitchClasses);
    };

    if (currentMode == AppMode::cellularGrid)
    {
        const auto tickIndex = (int) std::floor (beat * 4.0);
        const auto processAutomataChain = [&] (int startLayer, int startRow, int startCol, int stepLayer, int stepRow, int stepCol, float axisLevel)
        {
            auto col = startCol;
            auto row = startRow;
            auto layer = startLayer;
            auto chainActive = true;
            double localPitch = 45.0 + row * 3.0 + layer * 1.25;
            double localAmplitude = 0.06 * axisLevel;
            double localNoiseMix = layer > 5 ? 0.015 : 0.0;
            double localBrightness = 0.24 + (rows - 1 - row) * 0.035;
            double localWaveformMix = 0.18;
            double localKeyShift = 0.0;
            double localDecayScale = 0.95;
            int localChord = 0;
            int localRatchet = 1;
            int localRepeat = 0;
            double localTempoMul = 1.0;
            int localRoute = -1;
            int localMidiChannel = 1;
            int localCcNumber = -1;
            int localCcValue = -1;
            juce::String localParameterTarget;
            float localPluginBend = 0.0f;
            float localPluginPressure = 0.0f;

            while (juce::isPositiveAndBelow (col, cols)
                   && juce::isPositiveAndBelow (row, rows)
                   && juce::isPositiveAndBelow (layer, layers))
            {
                const auto& cell = snapshot.cells[(size_t) getCellIndex (layer, row, col)];
                ExpressionScope scope { localPitch, timeSeconds, (double) row, (double) col, beat, (double) startCol };
                const auto value = ExpressionEvaluator::evaluate (cell.program, scope);

                switch (cell.type)
                {
                    case GlyphType::pulse:
                        chainActive = true;
                        break;
                    case GlyphType::tone:
                    case GlyphType::voice:
                        if (chainActive)
                        {
                            localPitch = std::abs (value) > 8.0 ? value : (50.0 + row * 2.5 + layer * 1.1 + value * 2.0);
                            localWaveformMix = cell.type == GlyphType::voice ? 0.12 : 0.22;
                            localBrightness = juce::jlimit (0.14, 0.68, localBrightness + (cell.type == GlyphType::voice ? 0.05 : 0.03));
                        }
                        break;
                    case GlyphType::chord:
                        if (chainActive)
                            localChord = juce::jlimit (0, 5, (int) std::round (std::abs (value)));
                        break;
                    case GlyphType::key:
                        if (chainActive)
                            localKeyShift = std::round (value);
                        break;
                    case GlyphType::octave:
                        if (chainActive)
                            localKeyShift += 12.0 * std::round (value);
                        break;
                    case GlyphType::route:
                        if (chainActive)
                            localRoute = juce::jlimit (0, 2, parseWholeNumberOrFallback (cell.code, (int) std::round (std::abs (value))) - 1);
                        break;
                    case GlyphType::channel:
                        if (chainActive)
                            localMidiChannel = juce::jlimit (1, 16, parseWholeNumberOrFallback (cell.code, (int) std::round (std::abs (value))));
                        break;
                    case GlyphType::cc:
                        if (chainActive)
                            parseCcSpec (cell.code, localCcNumber, localCcValue);
                        break;
                    case GlyphType::parameter:
                        if (chainActive)
                            localParameterTarget = cell.code.trim();
                        break;
                    case GlyphType::tempo:
                        if (chainActive)
                            localTempoMul = juce::jlimit (0.5, 2.5, std::abs (value));
                        break;
                    case GlyphType::ratchet:
                        if (chainActive)
                            localRatchet = juce::jlimit (1, 4, (int) std::round (std::abs (value)));
                        break;
                    case GlyphType::repeat:
                        if (chainActive)
                            localRepeat = juce::jlimit (0, 2, (int) std::round (std::abs (value)));
                        break;
                    case GlyphType::length:
                        if (chainActive)
                            localDecayScale *= juce::jlimit (0.5, 2.4, std::abs (value));
                        break;
                    case GlyphType::accent:
                        if (chainActive)
                        {
                            localAmplitude *= juce::jlimit (0.4, 2.2, std::abs (value));
                            localPluginPressure = juce::jlimit (0.0f, 1.0f, (float) (std::abs (value) / 2.2));
                        }
                        break;
                    case GlyphType::decay:
                        if (chainActive)
                            localDecayScale *= juce::jlimit (0.4, 2.6, std::abs (value));
                        break;
                    case GlyphType::noise:
                        if (chainActive)
                            localNoiseMix = juce::jlimit (0.0, 0.4, localNoiseMix + std::abs (value) * 0.3);
                        break;
                    case GlyphType::mul:
                        if (chainActive)
                            localAmplitude *= juce::jlimit (0.25, 1.8, std::abs (value));
                        break;
                    case GlyphType::bias:
                        if (chainActive)
                        {
                            localPitch += value * 5.0;
                            localPluginBend = juce::jlimit (-1.0f, 1.0f, (float) (value / 6.0));
                        }
                        break;
                    case GlyphType::audio:
                        if (chainActive)
                        {
                            static constexpr std::array<std::array<int, 4>, 6> chordIntervals {{
                                {{ 0, 0, 0, 0 }},
                                {{ 0, 3, 7, 10 }},
                                {{ 0, 4, 7, 11 }},
                                {{ 0, 2, 7, 12 }},
                                {{ 0, 5, 7, 10 }},
                                {{ 0, 7, 12, 19 }}
                            }};
                            static constexpr std::array<int, 6> chordSizes {{ 1, 3, 3, 3, 4, 3 }};
                            const auto chordIndex = juce::jlimit (0, (int) chordIntervals.size() - 1, localChord);
                            const auto basePitch = localPitch + localKeyShift;
                            const auto baseAmp = (float) (localAmplitude * juce::jmax (0.12, std::abs (value)));
                            const auto ratchetSpacing = juce::jmax (1, (int) std::round ((double) samplesPerTick / (localTempoMul * localRatchet)));
                            const auto repeatSpacing = juce::jmax (1, (int) std::round ((double) samplesPerTick / juce::jmax (1.0, localTempoMul * 1.25)));

                            for (int ratchetIndex = 0; ratchetIndex < localRatchet; ++ratchetIndex)
                            {
                                const auto ratchetDelay = ratchetIndex * ratchetSpacing;
                                const auto ratchetAmp = baseAmp * std::pow (0.82f, (float) ratchetIndex);
                                for (int repeatIndex = 0; repeatIndex <= localRepeat; ++repeatIndex)
                                {
                                    const auto repeatDelay = ratchetDelay + repeatIndex * repeatSpacing;
                                    const auto repeatAmp = ratchetAmp * std::pow (0.7f, (float) repeatIndex);
                                    if (isPluginMode() && localParameterTarget.isNotEmpty())
                                        applyPluginParameterTarget (localRoute, (float) basePitch, localParameterTarget, clamp01 ((float) std::abs (value)));
                                    for (int noteIndex = 0; noteIndex < chordSizes[(size_t) chordIndex]; ++noteIndex)
                                    {
                                        const auto interval = chordIntervals[(size_t) chordIndex][(size_t) noteIndex];
                                        triggerSynthVoice ((float) (basePitch + interval),
                                                          repeatAmp / juce::jmax (1.0f, (float) chordSizes[(size_t) chordIndex]),
                                                          (float) localNoiseMix,
                                                          (float) localBrightness,
                                                          (float) localWaveformMix,
                                                          (float) localDecayScale,
                                                          repeatDelay,
                                                          localRoute,
                                                          localMidiChannel,
                                                          localPluginBend,
                                                          localPluginPressure,
                                                          localCcNumber,
                                                          localCcValue);
                                    }
                                }
                            }
                        }
                        break;
                    case GlyphType::kick:
                    case GlyphType::snare:
                    case GlyphType::hat:
                    case GlyphType::clap:
                        if (chainActive)
                        {
                            static constexpr std::array<double, 4> melodicIntervals { 0.0, 7.0, 12.0, 16.0 };
                            const auto melodicIndex = cell.type == GlyphType::kick  ? 0
                                                    : cell.type == GlyphType::snare ? 1
                                                    : cell.type == GlyphType::hat   ? 2
                                                                                    : 3;
                            triggerSynthVoice ((float) (localPitch + localKeyShift + melodicIntervals[(size_t) melodicIndex]),
                                              (float) juce::jlimit (0.04, 0.14, localAmplitude * juce::jmax (0.5, std::abs (value)) * 0.65),
                                              0.0f,
                                              (float) juce::jlimit (0.18, 0.62, localBrightness + 0.04 * melodicIndex),
                                              0.06f,
                                              (float) juce::jlimit (0.35, 1.4, localDecayScale * 0.55),
                                              0,
                                              localRoute,
                                              localMidiChannel,
                                              localPluginBend,
                                              localPluginPressure,
                                              localCcNumber,
                                              localCcValue);
                        }
                        break;
                    default:
                        break;
                }

                col += stepCol;
                row += stepRow;
                layer += stepLayer;
            }
        };

        for (int layer = 0; layer < layers; ++layer)
            for (int row = 0; row < rows; ++row)
                for (int col = 0; col < cols; ++col)
                {
                    const auto index = (size_t) getCellIndex (layer, row, col);
                    if (automataCurrent[index] == 0)
                        continue;

                    const auto& sourceCell = snapshot.cells[index];
                    const auto isSourceGlyph = sourceCell.type == GlyphType::pulse
                                            || sourceCell.type == GlyphType::tone
                                            || sourceCell.type == GlyphType::voice
                                            || sourceCell.type == GlyphType::kick
                                            || sourceCell.type == GlyphType::snare
                                            || sourceCell.type == GlyphType::hat
                                            || sourceCell.type == GlyphType::clap
                                            || sourceCell.type == GlyphType::wormhole;
                    const auto rhythmicGate = ((col + row * 2 + layer * 3 + tickIndex) % 4) == 0;
                    const auto accentGate = ((col * 3 + row + layer * 5 + tickIndex) % 7) == 0;

                    if (! isSourceGlyph && ! accentGate)
                        continue;

                    if (! isSourceGlyph && ! rhythmicGate)
                        continue;

                    if (rhythmicGate || isSourceGlyph)
                        processAutomataChain (layer, row, col, 0, 0, ((row + layer + tickIndex) & 1) == 0 ? 1 : -1, isSourceGlyph ? 0.48f : 0.34f);

                    if (((tickIndex + col + layer) % 3) == 0)
                        processAutomataChain (layer, row, col, 0, ((col + layer) & 1) == 0 ? 1 : -1, 0, isSourceGlyph ? 0.28f : 0.18f);

                    if (isSourceGlyph && ((tickIndex + row) % 4) == 0)
                        processAutomataChain (layer, row, col, (row & 1) == 0 ? 1 : -1, 0, 0, 0.14f);
                }

        return;
    }

    for (int snakeIndex = 0; snakeIndex < audioSnakeCount; ++snakeIndex)
    {
        auto& snake = audioSnakes[(size_t) snakeIndex];

        if (! snake.active || snake.length <= 0)
            continue;

        snake.previousLength = snake.length;
        snake.lengthDelta = 0;
        snake.recentLengthChangeTicks = juce::jmax (0, snake.recentLengthChangeTicks - 1);

        const auto& head = snake.segments[0];

        if (! juce::isPositiveAndBelow (head.row, rows) || ! juce::isPositiveAndBelow (head.col, cols))
            continue;

        double gate = 1.0;
        double pitchValue = 43.0 + head.row * 5.0;
        double amplitude = 0.16;
        double noiseMix = 0.0;
        double brightness = 0.36 + (rows - 1 - head.row) * 0.05;
        double waveformMix = 0.45;
        const auto xStep = snake.directionX != 0 ? snake.directionX : 1;
        const auto yStep = snake.directionY != 0 ? snake.directionY : ((head.row % 2 == 0) ? 1 : -1);
        const auto zStep = snake.directionLayer != 0 ? snake.directionLayer : ((head.layer % 2 == 0) ? 1 : -1);

        const auto processChain = [&] (int stepCol, int stepRow, int stepLayer, float axisLevel, int axisRole)
        {
            double localGate = gate;
            double localPitch = pitchValue;
            double localAmplitude = amplitude * axisLevel;
            double localNoiseMix = noiseMix;
            double localBrightness = brightness;
            double localWaveformMix = waveformMix;
            double localKeyShift = 0.0;
            double localDecayScale = 1.0;
            int localChord = axisRole == 0 ? 1 : (axisRole == 1 ? 2 : 0);
            double localTempoMul = 1.0;
            int localRatchet = 1;
            int localRepeat = 0;
            int localSnakeLength = snake.length;
            int localRoute = -1;
            int localMidiChannel = 1;
            int localCcNumber = -1;
            int localCcValue = -1;
            juce::String localParameterTarget;
            float localPluginBend = 0.0f;
            float localPluginPressure = 0.0f;
            auto chainActive = false;

            if (harmonySpaceMode)
            {
                if (axisRole == 0)
                {
                    localPitch = 47.0 + head.row * 1.15 + head.layer * 0.35;
                    localAmplitude *= 0.22;
                    localBrightness = 0.26;
                    localWaveformMix = 0.10;
                    localNoiseMix = 0.0;
                    localDecayScale = 1.35;
                    localChord = 2;
                }
                else if (axisRole == 1)
                {
                    localPitch = 57.0 + head.row * 1.8 + head.layer * 0.45;
                    localAmplitude *= 0.34;
                    localBrightness = 0.34;
                    localWaveformMix = 0.18;
                    localNoiseMix = 0.01;
                    localDecayScale = 1.6;
                    localChord = 0;
                }
                else
                {
                    localPitch = 69.0 + head.row * 1.4 + head.layer * 0.9;
                    localAmplitude *= 0.16;
                    localBrightness = 0.48;
                    localWaveformMix = 0.28;
                    localNoiseMix = 0.02;
                    localDecayScale = 1.95;
                    localChord = 4;
                }
            }
            else if (axisRole == 0)
            {
                localPitch = 50.0 + head.row * 1.5;
                localAmplitude *= 0.32;
                localBrightness = 0.44;
                localWaveformMix = 0.34;
                localNoiseMix *= 0.35;
            }
            else if (axisRole == 1)
            {
                localPitch = 55.0 + head.row * 3.0 + head.layer * 0.5;
                localAmplitude *= 0.92;
                localBrightness = 0.48;
                localWaveformMix = 0.38;
                localNoiseMix *= 0.25;
            }
            else
            {
                localPitch = 72.0 + head.row * 2.0 + head.layer * 1.5;
                localAmplitude *= 0.6;
                localBrightness = 0.78;
                localWaveformMix = 0.18;
                localNoiseMix *= 0.18;
            }

            auto col = head.col;
            auto row = head.row;
            auto layer = head.layer;

            while (juce::isPositiveAndBelow (col, cols)
                   && juce::isPositiveAndBelow (row, rows)
                   && juce::isPositiveAndBelow (layer, layers))
            {
                const auto& cell = snapshot.cells[(size_t) getCellIndex (layer, row, col)];
                const auto triggeredHere = (col == head.col && row == head.row && layer == head.layer);
                const auto chainReady = chainActive || triggeredHere;
                ExpressionScope scope { localPitch, timeSeconds, (double) row, (double) col, beat, (double) head.col };
                const auto value = ExpressionEvaluator::evaluate (cell.program, scope);

                switch (cell.type)
                {
                    case GlyphType::pulse:
                        if (triggeredHere)
                            localGate = juce::jmax (0.2, std::abs (value));
                        break;
                    case GlyphType::tone:
                        if (chainReady)
                        {
                            if (harmonySpaceMode)
                            {
                                if (std::abs (value) > 8.0)
                                    localPitch = value + (axisRole == 2 ? 12.0 : (axisRole == 0 ? -12.0 : 0.0));
                                else if (axisRole == 0)
                                    localPitch = 43.0 + row * 1.5 + std::round (value * 1.0);
                                else if (axisRole == 1)
                                    localPitch = 55.0 + row * 2.0 + std::round (value * 2.0);
                                else
                                    localPitch = 67.0 + row * 1.5 + std::round (value * 2.0);

                                localWaveformMix = axisRole == 0 ? 0.08 : (axisRole == 1 ? 0.14 : 0.24);
                                localBrightness = juce::jlimit (0.12, 0.72, localBrightness + (axisRole == 0 ? 0.02 : (axisRole == 1 ? 0.04 : 0.08)));
                            }
                            else if (axisRole == 0)
                                localPitch = std::abs (value) > 8.0 ? value - 5.0 : (43.0 + row * 1.5 + std::round (value * 2.0));
                            else if (axisRole == 1)
                                localPitch = std::abs (value) > 8.0 ? value : (55.0 + row * 3.0 + std::round (value * 5.0));
                            else
                                localPitch = std::abs (value) > 8.0 ? value + 12.0 : (72.0 + row * 2.0 + std::round (value * 4.0));

                            localWaveformMix = axisRole == 0 ? 0.82 : (axisRole == 1 ? 0.34 : 0.12);
                            localBrightness = juce::jlimit (0.15, 0.9, localBrightness + (axisRole == 0 ? 0.03 : (axisRole == 1 ? 0.08 : 0.12)));
                        }
                        break;
                    case GlyphType::voice:
                        if (chainReady)
                        {
                            if (harmonySpaceMode)
                            {
                                if (std::abs (value) > 8.0)
                                    localPitch = value + (axisRole == 2 ? 7.0 : (axisRole == 0 ? -5.0 : 0.0));
                                else if (axisRole == 0)
                                    localPitch = 47.0 + row * 1.3 + std::round (value * 1.0);
                                else if (axisRole == 1)
                                    localPitch = 59.0 + row * 2.3 + std::round (value * 2.0);
                                else
                                    localPitch = 71.0 + row * 1.7 + std::round (value * 2.0);

                                localWaveformMix = axisRole == 0 ? 0.12 : (axisRole == 1 ? 0.22 : 0.32);
                                localBrightness = juce::jlimit (0.16, 0.82, localBrightness + (axisRole == 0 ? 0.05 : (axisRole == 1 ? 0.08 : 0.12)));
                                localDecayScale *= axisRole == 0 ? 1.25 : (axisRole == 1 ? 1.45 : 1.7);
                            }
                            else if (axisRole == 0)
                                localPitch = std::abs (value) > 8.0 ? value - 3.0 : (46.0 + row * 1.5 + std::round (value * 2.0));
                            else if (axisRole == 1)
                                localPitch = std::abs (value) > 8.0 ? value : (57.0 + row * 3.0 + std::round (value * 6.0));
                            else
                                localPitch = std::abs (value) > 8.0 ? value + 12.0 : (76.0 + row * 2.0 + std::round (value * 5.0));

                            localWaveformMix = axisRole == 0 ? 0.52 : (axisRole == 1 ? 0.46 : 0.2);
                            localBrightness = juce::jlimit (0.18, 0.95, localBrightness + (axisRole == 0 ? 0.08 : (axisRole == 1 ? 0.11 : 0.14)));
                        }
                        break;
                    case GlyphType::chord:
                        if (chainReady)
                            localChord = juce::jlimit (0, 5, (int) std::round (std::abs (value)));
                        break;
                    case GlyphType::key:
                        if (chainReady)
                            localKeyShift = std::round (value);
                        break;
                    case GlyphType::octave:
                        if (chainReady)
                            localKeyShift += 12.0 * std::round (value);
                        break;
                    case GlyphType::route:
                        if (chainReady)
                            localRoute = juce::jlimit (0, 2, parseWholeNumberOrFallback (cell.code, (int) std::round (std::abs (value))) - 1);
                        break;
                    case GlyphType::channel:
                        if (chainReady)
                            localMidiChannel = juce::jlimit (1, 16, parseWholeNumberOrFallback (cell.code, (int) std::round (std::abs (value))));
                        break;
                    case GlyphType::cc:
                        if (chainReady)
                            parseCcSpec (cell.code, localCcNumber, localCcValue);
                        break;
                    case GlyphType::parameter:
                        if (chainReady)
                            localParameterTarget = cell.code.trim();
                        break;
                    case GlyphType::tempo:
                        if (chainReady)
                            localTempoMul = juce::jlimit (harmonySpaceMode ? 0.75 : 0.5, harmonySpaceMode ? 2.0 : 4.0, std::abs (value));
                        break;
                    case GlyphType::ratchet:
                        if (chainReady)
                            localRatchet = juce::jlimit (1, harmonySpaceMode ? 3 : 8, (int) std::round (std::abs (value)));
                        break;
                    case GlyphType::repeat:
                        if (chainReady)
                            localRepeat = juce::jlimit (0, harmonySpaceMode ? 2 : 4, (int) std::round (std::abs (value)));
                        break;
                    case GlyphType::wormhole:
                        break;
                    case GlyphType::kick:
                    case GlyphType::snare:
                    case GlyphType::hat:
                    case GlyphType::clap:
                        break;
                    case GlyphType::length:
                        if (chainReady)
                            localSnakeLength = juce::jlimit (1, snakeLength, (int) std::round (std::abs (value)));
                        break;
                    case GlyphType::accent:
                        if (chainReady)
                        {
                            localAmplitude *= juce::jlimit (0.35, harmonySpaceMode ? 1.8 : 2.8, std::abs (value));
                            localPluginPressure = juce::jlimit (0.0f, 1.0f, (float) (std::abs (value) / (harmonySpaceMode ? 1.8 : 2.8)));
                        }
                        break;
                    case GlyphType::decay:
                        if (chainReady)
                            localDecayScale *= juce::jlimit (0.35, harmonySpaceMode ? 3.4 : 2.5, std::abs (value));
                        break;
                    case GlyphType::noise:
                        if (chainReady)
                            localNoiseMix = juce::jlimit (0.0, 1.0, localNoiseMix + std::abs (value) * (harmonySpaceMode ? (axisRole == 2 ? 0.18 : 0.08) : (axisRole == 2 ? 0.8 : 0.35)));
                        break;
                    case GlyphType::mul:
                        if (chainReady)
                            localAmplitude *= juce::jlimit (0.2, 2.0, std::abs (value));
                        break;
                    case GlyphType::bias:
                        if (chainReady)
                        {
                            if (harmonySpaceMode)
                            {
                                localPitch += value * (axisRole == 0 ? 2.0 : (axisRole == 1 ? 4.0 : 3.0));
                                localBrightness = juce::jlimit (0.0, 1.0, localBrightness + value * (axisRole == 0 ? 0.06 : (axisRole == 1 ? 0.10 : 0.14)));
                            }
                            else
                            {
                                localPitch += value * (axisRole == 0 ? 7.0 : (axisRole == 1 ? 12.0 : 5.0));
                                localBrightness = juce::jlimit (0.0, 1.0, localBrightness + value * (axisRole == 0 ? 0.18 : (axisRole == 1 ? 0.28 : 0.34)));
                            }
                            localPluginBend = juce::jlimit (-1.0f, 1.0f, (float) (value / (harmonySpaceMode ? 4.0 : 8.0)));
                        }
                        break;
                    case GlyphType::audio:
                        if (chainReady)
                        {
                            const auto basePitch = localPitch + localKeyShift;
                            const auto baseAmp = (float) (localAmplitude * std::abs (localGate) * juce::jmax (0.15, std::abs (value)));
                            const auto ratchetSpacing = juce::jmax (1, (int) std::round ((double) samplesPerTick / (localTempoMul * localRatchet)));
                            const auto repeatSpacing = juce::jmax (1, (int) std::round ((double) samplesPerTick / juce::jmax (1.0, localTempoMul * 1.5)));

                            if (harmonySpaceMode)
                            {
                                static constexpr std::array<std::array<int, 5>, 7> rootVoicings {{
                                    {{ 0, 7, 12, 16, -99 }},
                                    {{ 0, 7, 10, 15, -99 }},
                                    {{ 0, 8, 12, 16, -99 }},
                                    {{ 0, 7, 14, 19, -99 }},
                                    {{ 0, 5, 10, 14, -99 }},
                                    {{ 0, 10, 14, 17, -99 }},
                                    {{ 0, 9, 14, 19, -99 }}
                                }};
                                static constexpr std::array<std::array<int, 5>, 7> midVoicings {{
                                    {{ 4, 11, 14, 18, -99 }},
                                    {{ 3, 10, 14, 17, -99 }},
                                    {{ 4, 8, 14, 18, -99 }},
                                    {{ 2, 7, 14, 19, -99 }},
                                    {{ 5, 9, 14, 17, -99 }},
                                    {{ 4, 10, 14, 21, -99 }},
                                    {{ 4, 9, 14, 19, -99 }}
                                }};
                                static constexpr std::array<std::array<int, 4>, 7> upperVoicings {{
                                    {{ 11, 14, 18, -99 }},
                                    {{ 10, 14, 17, -99 }},
                                    {{ 12, 16, 20, -99 }},
                                    {{ 7, 14, 19, -99 }},
                                    {{ 9, 14, 17, -99 }},
                                    {{ 10, 14, 17, -99 }},
                                    {{ 9, 14, 19, -99 }}
                                }};

                                const auto chordIndex = juce::jlimit (0, 6, localChord);
                                const auto ratchetCount = juce::jmin (localRatchet, axisRole == 2 ? 2 : 1);

                                for (int ratchetIndex = 0; ratchetIndex < ratchetCount; ++ratchetIndex)
                                {
                                    const auto ratchetDelay = ratchetIndex * ratchetSpacing;
                                    const auto ratchetAmp = baseAmp * std::pow (axisRole == 2 ? 0.74f : 0.82f, (float) ratchetIndex);

                                    for (int repeatIndex = 0; repeatIndex <= juce::jmin (localRepeat, axisRole == 0 ? 1 : 2); ++repeatIndex)
                                    {
                                    const auto repeatDelay = ratchetDelay + repeatIndex * repeatSpacing;
                                    const auto repeatAmp = ratchetAmp * std::pow (axisRole == 0 ? 0.58f : 0.66f, (float) repeatIndex);
                                    if (isPluginMode() && localParameterTarget.isNotEmpty())
                                        applyPluginParameterTarget (localRoute, (float) basePitch, localParameterTarget, clamp01 ((float) std::abs (value)));

                                        if (axisRole == 2)
                                        {
                                            constexpr auto noteCount = 3;
                                            for (int noteIndex = 0; noteIndex < noteCount; ++noteIndex)
                                            {
                                                const auto interval = upperVoicings[(size_t) chordIndex][(size_t) noteIndex];
                                                if (interval < -50)
                                                    continue;

                                                const auto voiceAmp = repeatAmp / (float) noteCount * 0.56f;
                                                const auto voiceBrightness = juce::jlimit (0.12f, 0.78f, (float) (localBrightness + noteIndex * 0.03));
                                                const auto voiceWaveform = juce::jlimit (0.04f, 0.42f, (float) (localWaveformMix + noteIndex * 0.025));
                                                const auto voiceDecay = (float) (localDecayScale * 1.95);
                                                triggerSynthVoice (quantizeHarmonySpacePitch ((float) (basePitch + interval), chordIndex),
                                                                   voiceAmp,
                                                                   (float) localNoiseMix,
                                                                   voiceBrightness,
                                                                   voiceWaveform,
                                                                   voiceDecay,
                                                                   repeatDelay + noteIndex * juce::jmax (0, samplesPerTick / 18),
                                                                   localRoute,
                                                                   localMidiChannel,
                                                                   localPluginBend,
                                                                   localPluginPressure,
                                                                   localCcNumber,
                                                                   localCcValue);
                                            }
                                        }
                                        else
                                        {
                                            constexpr auto noteCount = 4;
                                            const auto& voicing = axisRole == 0 ? rootVoicings[(size_t) chordIndex]
                                                                                 : midVoicings[(size_t) chordIndex];
                                            for (int noteIndex = 0; noteIndex < noteCount; ++noteIndex)
                                            {
                                                const auto interval = voicing[(size_t) noteIndex];
                                                if (interval < -50)
                                                    continue;

                                                const auto voiceAmp = repeatAmp / (float) noteCount * (axisRole == 0 ? 0.95f : 0.78f);
                                                const auto voiceBrightness = juce::jlimit (0.12f, 0.78f, (float) (localBrightness + noteIndex * 0.03));
                                                const auto voiceWaveform = juce::jlimit (0.04f, 0.42f, (float) (localWaveformMix + noteIndex * 0.025));
                                                const auto voiceDecay = (float) (localDecayScale * (axisRole == 0 ? 1.45 : 1.7));
                                                triggerSynthVoice (quantizeHarmonySpacePitch ((float) (basePitch + interval), chordIndex),
                                                                   voiceAmp,
                                                                   (float) localNoiseMix,
                                                                   voiceBrightness,
                                                                   voiceWaveform,
                                                                   voiceDecay,
                                                                   repeatDelay + noteIndex * juce::jmax (0, samplesPerTick / 18),
                                                                   localRoute,
                                                                   localMidiChannel,
                                                                   localPluginBend,
                                                                   localPluginPressure,
                                                                   localCcNumber,
                                                                   localCcValue);
                                            }
                                        }
                                    }
                                }
                            }
                            else
                            {
                                static constexpr std::array<std::array<int, 4>, 6> chordIntervals {{
                                    {{ 0, 0, 0, 0 }},
                                    {{ 0, 3, 7, 10 }},
                                    {{ 0, 4, 7, 11 }},
                                    {{ 0, 2, 7, 12 }},
                                    {{ 0, 5, 7, 10 }},
                                    {{ 0, 7, 12, 19 }}
                                }};
                                static constexpr std::array<int, 6> chordSizes {{ 1, 3, 3, 3, 4, 3 }};
                                const auto chordIndex = juce::jlimit (0, (int) chordIntervals.size() - 1, localChord);

                                for (int ratchetIndex = 0; ratchetIndex < localRatchet; ++ratchetIndex)
                                {
                                    const auto ratchetDelay = ratchetIndex * ratchetSpacing;
                                    const auto ratchetAmp = baseAmp * std::pow (0.84f, (float) ratchetIndex);

                                    for (int repeatIndex = 0; repeatIndex <= localRepeat; ++repeatIndex)
                                    {
                                        const auto repeatDelay = ratchetDelay + repeatIndex * repeatSpacing;
                                        const auto repeatAmp = ratchetAmp * std::pow (0.68f, (float) repeatIndex);

                                        for (int noteIndex = 0; noteIndex < chordSizes[(size_t) chordIndex]; ++noteIndex)
                                        {
                                            const auto interval = chordIntervals[(size_t) chordIndex][(size_t) noteIndex];
                                            triggerSynthVoice ((float) (basePitch + interval),
                                                              repeatAmp / juce::jmax (1.0f, (float) chordSizes[(size_t) chordIndex]),
                                                              (float) localNoiseMix,
                                                              (float) localBrightness,
                                                              (float) localWaveformMix,
                                                              (float) localDecayScale,
                                                              repeatDelay,
                                                              localRoute,
                                                              localMidiChannel,
                                                              localPluginBend,
                                                              localPluginPressure,
                                                              localCcNumber,
                                                              localCcValue);
                                        }
                                    }
                                }
                            }
                        }
                        break;
                    case GlyphType::hue:
                        if (chainReady)
                            localBrightness = juce::jlimit (0.0, 1.0, value);
                        break;
                    case GlyphType::visual:
                        if (chainReady)
                            localAmplitude *= juce::jlimit (0.4, 1.8, 0.8 + std::abs (value) * 0.4);
                        break;
                    case GlyphType::empty:
                        break;
                }

                chainActive = chainReady;
                col += stepCol;
                row += stepRow;
                layer += stepLayer;
            }

            const auto newLength = juce::jlimit (1, snakeLength, localSnakeLength);
            if (newLength != snake.length)
            {
                snake.lengthDelta = newLength - snake.length;
                snake.length = newLength;
                snake.recentLengthChangeTicks = 8;
            }
        };

        processChain (xStep, 0, 0, 1.0f, 0);
        processChain (0, yStep, 0, 0.82f, 1);
        processChain (0, 0, zStep, 0.58f, 2);
    }

    if (audioSnakeCount > 0)
    {
        std::array<int, maxSnakes> headLayers {};
        std::array<int, maxSnakes> headRows {};
        std::array<int, maxSnakes> headCols {};
        auto dirXSum = 0;
        auto dirYSum = 0;
        auto dirZSum = 0;

        for (int snakeIndex = 0; snakeIndex < audioSnakeCount; ++snakeIndex)
        {
            const auto& snake = audioSnakes[(size_t) snakeIndex];
            headLayers[(size_t) snakeIndex] = snake.segments[0].layer;
            headRows[(size_t) snakeIndex] = snake.segments[0].row;
            headCols[(size_t) snakeIndex] = snake.segments[0].col;
            dirXSum += snake.directionX;
            dirYSum += snake.directionY;
            dirZSum += snake.directionLayer;
        }

        std::sort (headLayers.begin(), headLayers.begin() + audioSnakeCount);
        std::sort (headRows.begin(), headRows.begin() + audioSnakeCount);
        std::sort (headCols.begin(), headCols.begin() + audioSnakeCount);

        const SnakeSegment ghostHead {
            headLayers[(size_t) (audioSnakeCount / 2)],
            headRows[(size_t) (audioSnakeCount / 2)],
            headCols[(size_t) (audioSnakeCount / 2)]
        };

        const auto ghostStepX = dirXSum == 0 ? 1 : (dirXSum > 0 ? 1 : -1);
        const auto ghostStepY = dirYSum == 0 ? ((ghostHead.row % 2 == 0) ? 1 : -1) : (dirYSum > 0 ? 1 : -1);
        const auto ghostStepZ = dirZSum == 0 ? ((ghostHead.layer % 2 == 0) ? 1 : -1) : (dirZSum > 0 ? 1 : -1);

        std::array<SnakeSegment, snakeLength> ghostSegments {};
        auto ghostSegmentCount = 0;

        for (int segmentIndex = 0; segmentIndex < snakeLength; ++segmentIndex)
        {
            std::array<int, maxSnakes> segLayers {};
            std::array<int, maxSnakes> segRows {};
            std::array<int, maxSnakes> segCols {};
            auto count = 0;

            for (int snakeIndex = 0; snakeIndex < audioSnakeCount; ++snakeIndex)
            {
                const auto& snake = audioSnakes[(size_t) snakeIndex];

                if (! snake.active || segmentIndex >= snake.length)
                    continue;

                segLayers[(size_t) count] = snake.segments[(size_t) segmentIndex].layer;
                segRows[(size_t) count] = snake.segments[(size_t) segmentIndex].row;
                segCols[(size_t) count] = snake.segments[(size_t) segmentIndex].col;
                ++count;
            }

            if (count == 0)
                break;

            std::sort (segLayers.begin(), segLayers.begin() + count);
            std::sort (segRows.begin(), segRows.begin() + count);
            std::sort (segCols.begin(), segCols.begin() + count);
            ghostSegments[(size_t) ghostSegmentCount++] = {
                segLayers[(size_t) (count / 2)],
                segRows[(size_t) (count / 2)],
                segCols[(size_t) (count / 2)]
            };
        }

        const auto processGhostChain = [&] (const SnakeSegment& start, int stepCol, int stepRow, int stepLayer, float axisLevel)
        {
            auto col = start.col;
            auto row = start.row;
            auto layer = start.layer;
            auto chainActive = false;
            auto localTempoMul = 1.0;
            auto localRatchet = 1;
            auto localRepeat = 0;
            auto localDecay = 0.6;
            auto localAccent = 1.0;
            auto localRoute = 2;
            auto localMidiChannel = 10;
            auto localCcNumber = -1;
            auto localCcValue = -1;
            juce::String localParameterTarget;
            auto localPluginBend = 0.0f;
            auto localPluginPressure = 0.0f;

            while (juce::isPositiveAndBelow (col, cols)
                   && juce::isPositiveAndBelow (row, rows)
                   && juce::isPositiveAndBelow (layer, layers))
            {
                const auto& cell = snapshot.cells[(size_t) getCellIndex (layer, row, col)];
                const auto triggeredHere = (col == start.col && row == start.row && layer == start.layer);
                const auto chainReady = chainActive || triggeredHere;
                ExpressionScope scope { (double) row, timeSeconds, (double) row, (double) col, beat, (double) start.col };
                const auto value = ExpressionEvaluator::evaluate (cell.program, scope);

                switch (cell.type)
                {
                    case GlyphType::pulse:
                        chainActive = triggeredHere || chainActive;
                        break;
                    case GlyphType::tempo:
                        if (chainReady)
                            localTempoMul = juce::jlimit (0.5, 4.0, std::abs (value));
                        break;
                    case GlyphType::route:
                        if (chainReady)
                            localRoute = juce::jlimit (0, 2, parseWholeNumberOrFallback (cell.code, (int) std::round (std::abs (value))) - 1);
                        break;
                    case GlyphType::channel:
                        if (chainReady)
                            localMidiChannel = juce::jlimit (1, 16, parseWholeNumberOrFallback (cell.code, (int) std::round (std::abs (value))));
                        break;
                    case GlyphType::cc:
                        if (chainReady)
                            parseCcSpec (cell.code, localCcNumber, localCcValue);
                        break;
                    case GlyphType::parameter:
                        if (chainReady)
                            localParameterTarget = cell.code.trim();
                        break;
                    case GlyphType::ratchet:
                        if (chainReady)
                            localRatchet = juce::jlimit (1, 8, (int) std::round (std::abs (value)));
                        break;
                    case GlyphType::repeat:
                        if (chainReady)
                            localRepeat = juce::jlimit (0, 4, (int) std::round (std::abs (value)));
                        break;
                    case GlyphType::length:
                        if (chainReady)
                            localDecay = juce::jlimit (0.25, 1.2, std::abs (value) / 3.0);
                        break;
                    case GlyphType::accent:
                        if (chainReady)
                        {
                            localAccent *= juce::jlimit (0.35, 2.4, std::abs (value));
                            localPluginPressure = juce::jlimit (0.0f, 1.0f, (float) (std::abs (value) / 2.4));
                        }
                        break;
                    case GlyphType::decay:
                        if (chainReady)
                            localDecay *= juce::jlimit (0.35, 2.2, std::abs (value));
                        break;
                    case GlyphType::bias:
                        if (chainReady)
                            localPluginBend = juce::jlimit (-1.0f, 1.0f, (float) (value / 8.0));
                        break;
                    case GlyphType::kick:
                    case GlyphType::snare:
                    case GlyphType::hat:
                    case GlyphType::clap:
                        if (chainReady)
                        {
                            const auto baseAmp = juce::jlimit (0.05f, 0.42f, (float) (std::abs (value) * axisLevel * localAccent));
                            const auto ratchetSpacing = juce::jmax (1, (int) std::round ((double) samplesPerTick / (localTempoMul * localRatchet)));
                            const auto repeatSpacing = juce::jmax (1, (int) std::round ((double) samplesPerTick / juce::jmax (1.0, localTempoMul * 1.5)));
                            const auto drumType = cell.type == GlyphType::kick  ? DrumType::kick
                                                  : cell.type == GlyphType::snare ? DrumType::snare
                                                  : cell.type == GlyphType::hat   ? DrumType::hat
                                                                                  : DrumType::clap;

                            for (int ratchetIndex = 0; ratchetIndex < localRatchet; ++ratchetIndex)
                            {
                                const auto ratchetDelay = ratchetIndex * ratchetSpacing;
                                const auto ratchetAmp = baseAmp * std::pow (0.82f, (float) ratchetIndex);

                                for (int repeatIndex = 0; repeatIndex <= localRepeat; ++repeatIndex)
                                {
                                    const auto repeatDelay = ratchetDelay + repeatIndex * repeatSpacing;
                                    const auto repeatAmp = ratchetAmp * std::pow (0.7f, (float) repeatIndex);
                                    if (isPluginMode() && localParameterTarget.isNotEmpty())
                                        applyPluginParameterTarget (localRoute, (float) (36 + row), localParameterTarget, clamp01 ((float) std::abs (value)));
                                    triggerDrumVoice (drumType,
                                                      repeatAmp,
                                                      (float) juce::jlimit (0.0, 1.0, std::abs (value) * 0.25 + row / 8.0),
                                                      (float) localDecay,
                                                      repeatDelay,
                                                      localRoute,
                                                      localMidiChannel,
                                                      localPluginBend,
                                                      localPluginPressure,
                                                      localCcNumber,
                                                      localCcValue);
                                }
                            }
                        }
                        break;
                    default:
                        break;
                }

                chainActive = chainReady;
                col += stepCol;
                row += stepRow;
                layer += stepLayer;
            }
        };

        for (int i = 0; i < ghostSegmentCount; ++i)
        {
            const auto segmentLevel = juce::jmap ((float) i, 0.0f, (float) juce::jmax (1, ghostSegmentCount - 1), 1.0f, 0.42f);
            processGhostChain (ghostSegments[(size_t) i], ghostStepX, 0, 0, 0.95f * segmentLevel);
            processGhostChain (ghostSegments[(size_t) i], 0, ghostStepY, 0, 0.82f * segmentLevel);
            processGhostChain (ghostSegments[(size_t) i], 0, 0, ghostStepZ, 0.7f * segmentLevel);

            if (i == 0)
            {
                processGhostChain (ghostSegments[(size_t) i], -ghostStepX, 0, 0, 0.46f);
                processGhostChain (ghostSegments[(size_t) i], 0, -ghostStepY, 0, 0.4f);
            }
        }
    }
}

float MainComponent::renderSynthSample() noexcept
{
    auto output = 0.0f;
    const auto harmonySpaceMode = currentMode == AppMode::harmonySpace;

    for (auto& voice : synthVoices)
    {
        if (! voice.active)
            continue;

        if (voice.delaySamples > 0)
        {
            --voice.delaySamples;
            continue;
        }

        voice.phaseA += voice.frequency / (float) currentSampleRate;
        voice.phaseB += (voice.frequency * voice.detuneRatio) / (float) currentSampleRate;

        voice.phaseA -= std::floor (voice.phaseA);
        voice.phaseB -= std::floor (voice.phaseB);

        const auto sine = std::sin (voice.phaseA * juce::MathConstants<float>::twoPi);
        const auto tri = 1.0f - 4.0f * std::abs (voice.phaseB - 0.5f);
        const auto saw = voice.phaseB * 2.0f - 1.0f;
        const auto harmonic = harmonySpaceMode ? (sine * 0.70f + tri * 0.25f + saw * 0.05f)
                                               : (tri * 0.55f + saw * 0.45f);
        const auto tonal = juce::jmap (voice.waveformMix, sine, harmonic);
        const auto noise = ((random.nextFloat() * 2.0f) - 1.0f) * voice.noiseMix;
        voice.attack = juce::jmin (1.0f, voice.attack + voice.attackIncrement);
        const auto raw = (tonal * (1.0f - voice.noiseMix * (harmonySpaceMode ? 0.18f : 0.45f)) + noise) * voice.amplitude * voice.env * voice.attack;

        voice.filterState += (raw - voice.filterState) * voice.filterCoeff;
        if (harmonySpaceMode)
        {
            voice.filterStateB += (voice.filterState - voice.filterStateB) * voice.filterCoeff * 0.42f;
            output += std::tanh (voice.filterStateB * 1.12f) * 0.92f;
        }
        else
        {
            output += voice.filterState;
        }

        voice.env *= voice.decayPerSample;
        if (voice.env < 0.0005f)
            voice.active = false;
    }

    return output;
}

float MainComponent::renderDrumSample() noexcept
{
    auto output = 0.0f;

    for (auto& voice : drumVoices)
    {
        if (! voice.active)
            continue;

        if (voice.delaySamples > 0)
        {
            --voice.delaySamples;
            continue;
        }

        float sample = 0.0f;
        const auto noise = random.nextFloat() * 2.0f - 1.0f;

        switch (voice.type)
        {
            case DrumType::kick:
            {
                voice.phase += voice.frequency / (float) currentSampleRate;
                voice.phase -= std::floor (voice.phase);
                const auto pitchEnv = voice.sweep * voice.env * voice.env;
                voice.frequency = juce::jlimit (35.0f, 180.0f, voice.frequency * 0.9985f + pitchEnv * 0.0015f);
                const auto body = std::sin (voice.phase * juce::MathConstants<float>::twoPi);
                const auto click = noise * voice.env * 0.08f;
                sample = std::tanh ((body * 1.35f + click) * voice.amplitude * 1.8f);
                break;
            }
            case DrumType::snare:
            {
                voice.phase += voice.frequency / (float) currentSampleRate;
                voice.phaseB += (voice.frequency * 1.47f) / (float) currentSampleRate;
                voice.phase -= std::floor (voice.phase);
                voice.phaseB -= std::floor (voice.phaseB);
                const auto body = std::sin (voice.phase * juce::MathConstants<float>::twoPi) * 0.6f
                                + std::sin (voice.phaseB * juce::MathConstants<float>::twoPi) * 0.3f;
                sample = (noise * 0.78f + body * 0.34f) * voice.env * voice.amplitude;
                break;
            }
            case DrumType::hat:
            {
                voice.phase += voice.frequency / (float) currentSampleRate;
                voice.phaseB += (voice.frequency * 1.37f) / (float) currentSampleRate;
                voice.phase -= std::floor (voice.phase);
                voice.phaseB -= std::floor (voice.phaseB);
                const auto metal = ((voice.phase > 0.5f ? 1.0f : -1.0f)
                                  + (voice.phaseB > 0.5f ? 1.0f : -1.0f)) * 0.5f;
                sample = (noise * 0.72f + metal * 0.48f) * voice.env * voice.amplitude;
                sample -= voice.noiseState;
                voice.noiseState += (sample - voice.noiseState) * 0.16f;
                break;
            }
            case DrumType::clap:
            {
                if (voice.burstCounter <= 0)
                {
                    ++voice.burstStage;
                    voice.burstCounter = voice.burstStage < 3 ? juce::jmax (1, (int) (0.012f * currentSampleRate)) : juce::jmax (1, (int) (0.05f * currentSampleRate));
                }

                --voice.burstCounter;
                const auto burstEnv = voice.burstStage < 3 ? 1.0f : 0.55f;
                sample = noise * burstEnv * voice.env * voice.amplitude;
                sample -= voice.noiseState;
                voice.noiseState += (sample - voice.noiseState) * 0.09f;
                break;
            }
        }

        output += sample;
        voice.env *= voice.decayPerSample;

        if (voice.env < 0.0005f)
            voice.active = false;
    }

    return output;
}

void MainComponent::renderInternalMix (float& left, float& right) noexcept
{
    left = 0.0f;
    right = 0.0f;

    std::array<float, 3> stripPeaks {};

    for (auto& voice : synthVoices)
    {
        if (! voice.active)
            continue;

        if (voice.delaySamples > 0)
        {
            --voice.delaySamples;
            continue;
        }

        const auto harmonySpaceMode = currentMode == AppMode::harmonySpace;
        voice.phaseA += voice.frequency / (float) currentSampleRate;
        voice.phaseB += (voice.frequency * voice.detuneRatio) / (float) currentSampleRate;
        voice.phaseA -= std::floor (voice.phaseA);
        voice.phaseB -= std::floor (voice.phaseB);

        const auto sine = std::sin (voice.phaseA * juce::MathConstants<float>::twoPi);
        const auto tri = 1.0f - 4.0f * std::abs (voice.phaseB - 0.5f);
        const auto saw = voice.phaseB * 2.0f - 1.0f;
        const auto harmonic = harmonySpaceMode ? (sine * 0.70f + tri * 0.25f + saw * 0.05f)
                                               : (tri * 0.55f + saw * 0.45f);
        const auto tonal = juce::jmap (voice.waveformMix, sine, harmonic);
        const auto noise = ((random.nextFloat() * 2.0f) - 1.0f) * voice.noiseMix;
        voice.attack = juce::jmin (1.0f, voice.attack + voice.attackIncrement);
        const auto raw = (tonal * (1.0f - voice.noiseMix * (harmonySpaceMode ? 0.18f : 0.45f)) + noise) * voice.amplitude * voice.env * voice.attack;

        voice.filterState += (raw - voice.filterState) * voice.filterCoeff;
        float sample = 0.0f;
        if (harmonySpaceMode)
        {
            voice.filterStateB += (voice.filterState - voice.filterStateB) * voice.filterCoeff * 0.42f;
            sample = std::tanh (voice.filterStateB * 1.12f) * 0.92f;
        }
        else
        {
            sample = voice.filterState;
        }

        const auto stripIndex = juce::jlimit (0, (int) mixerStrips.size() - 1, voice.stripIndex);
        const auto& strip = mixerStrips[(size_t) stripIndex];
        const auto leftGain = strip.volume * (strip.pan <= 0.0f ? 1.0f : 1.0f - strip.pan);
        const auto rightGain = strip.volume * (strip.pan >= 0.0f ? 1.0f : 1.0f + strip.pan);
        left += sample * leftGain;
        right += sample * rightGain;
        stripPeaks[(size_t) stripIndex] = juce::jmax (stripPeaks[(size_t) stripIndex], std::abs (sample) * juce::jmax (leftGain, rightGain));

        voice.env *= voice.decayPerSample;
        if (voice.env < 0.0005f)
            voice.active = false;
    }

    for (auto& voice : drumVoices)
    {
        if (! voice.active)
            continue;

        if (voice.delaySamples > 0)
        {
            --voice.delaySamples;
            continue;
        }

        float sample = 0.0f;
        const auto noise = random.nextFloat() * 2.0f - 1.0f;

        switch (voice.type)
        {
            case DrumType::kick:
            {
                voice.phase += voice.frequency / (float) currentSampleRate;
                voice.phase -= std::floor (voice.phase);
                const auto pitchEnv = voice.sweep * voice.env * voice.env;
                voice.frequency = juce::jlimit (35.0f, 180.0f, voice.frequency * 0.9985f + pitchEnv * 0.0015f);
                const auto body = std::sin (voice.phase * juce::MathConstants<float>::twoPi);
                const auto click = noise * voice.env * 0.08f;
                sample = std::tanh ((body * 1.35f + click) * voice.amplitude * 1.8f);
                break;
            }
            case DrumType::snare:
            {
                voice.phase += voice.frequency / (float) currentSampleRate;
                voice.phaseB += (voice.frequency * 1.47f) / (float) currentSampleRate;
                voice.phase -= std::floor (voice.phase);
                voice.phaseB -= std::floor (voice.phaseB);
                const auto body = std::sin (voice.phase * juce::MathConstants<float>::twoPi) * 0.6f
                                + std::sin (voice.phaseB * juce::MathConstants<float>::twoPi) * 0.3f;
                sample = (noise * 0.78f + body * 0.34f) * voice.env * voice.amplitude;
                break;
            }
            case DrumType::hat:
            {
                voice.phase += voice.frequency / (float) currentSampleRate;
                voice.phaseB += (voice.frequency * 1.37f) / (float) currentSampleRate;
                voice.phase -= std::floor (voice.phase);
                voice.phaseB -= std::floor (voice.phaseB);
                const auto metal = ((voice.phase > 0.5f ? 1.0f : -1.0f)
                                  + (voice.phaseB > 0.5f ? 1.0f : -1.0f)) * 0.5f;
                sample = (noise * 0.72f + metal * 0.48f) * voice.env * voice.amplitude;
                sample -= voice.noiseState;
                voice.noiseState += (sample - voice.noiseState) * 0.16f;
                break;
            }
            case DrumType::clap:
            {
                if (voice.burstCounter <= 0)
                {
                    ++voice.burstStage;
                    voice.burstCounter = voice.burstStage < 3 ? juce::jmax (1, (int) (0.012f * currentSampleRate)) : juce::jmax (1, (int) (0.05f * currentSampleRate));
                }

                --voice.burstCounter;
                const auto burstEnv = voice.burstStage < 3 ? 1.0f : 0.55f;
                sample = noise * burstEnv * voice.env * voice.amplitude;
                sample -= voice.noiseState;
                voice.noiseState += (sample - voice.noiseState) * 0.09f;
                break;
            }
        }

        const auto stripIndex = juce::jlimit (0, (int) mixerStrips.size() - 1, voice.stripIndex);
        const auto& strip = mixerStrips[(size_t) stripIndex];
        const auto leftGain = strip.volume * (strip.pan <= 0.0f ? 1.0f : 1.0f - strip.pan);
        const auto rightGain = strip.volume * (strip.pan >= 0.0f ? 1.0f : 1.0f + strip.pan);
        left += sample * leftGain;
        right += sample * rightGain;
        stripPeaks[(size_t) stripIndex] = juce::jmax (stripPeaks[(size_t) stripIndex], std::abs (sample) * juce::jmax (leftGain, rightGain));

        voice.env *= voice.decayPerSample;
        if (voice.env < 0.0005f)
            voice.active = false;
    }

    for (int stripIndex = 0; stripIndex < (int) mixerStrips.size(); ++stripIndex)
    {
        const auto previousLevel = publishedMixerLevels[(size_t) stripIndex].load (std::memory_order_relaxed);
        const auto smoothedLevel = juce::jmax (stripPeaks[(size_t) stripIndex], previousLevel * 0.90f);
        publishedMixerLevels[(size_t) stripIndex].store (juce::jlimit (0.0f, 1.0f, smoothedLevel), std::memory_order_relaxed);
    }
}

float MainComponent::evaluateAudioForSnapshot (const PatchSnapshot&, double) noexcept
{
    return renderSynthSample() + renderDrumSample();
}

void MainComponent::initialiseSnakes()
{
    pendingSnakeReset.store (true, std::memory_order_release);
    pendingSpawnRequests.store (0, std::memory_order_release);

    if (! isRunning.load())
    {
        if (currentMode == AppMode::cellularGrid)
        {
            if (auto patchSnapshot = getPatchSnapshot())
                resetAutomata (*patchSnapshot);
            else
            {
                automataCurrent.fill (0);
                automataPrevious.fill (0);
            }
            audioSnakeCount = 0;
        }
        else
        {
            resetAudioSnakes();
        }
        transportSequence.fetch_add (1, std::memory_order_acq_rel);
        publishedTransportSnapshot.snakeCount = audioSnakeCount;
        publishedTransportSnapshot.automataMode = currentMode == AppMode::cellularGrid;
        publishedTransportSnapshot.automataCurrent = automataCurrent;
        publishedTransportSnapshot.automataPrevious = automataPrevious;
        publishedTransportSnapshot.transportPhase = 0.0f;
        for (int i = 0; i < maxSnakes; ++i)
            publishedTransportSnapshot.snakes[(size_t) i] = audioSnakes[(size_t) i];
        transportSequence.fetch_add (1, std::memory_order_release);
        pendingSnakeReset.store (false, std::memory_order_release);
    }
}

void MainComponent::spawnSnake()
{
    pendingSpawnRequests.fetch_add (1, std::memory_order_acq_rel);

    if (! isRunning.load())
    {
        applyPendingTransportCommands();
        transportSequence.fetch_add (1, std::memory_order_acq_rel);
        publishedTransportSnapshot.snakeCount = audioSnakeCount;
        publishedTransportSnapshot.automataMode = currentMode == AppMode::cellularGrid;
        publishedTransportSnapshot.automataCurrent = automataCurrent;
        publishedTransportSnapshot.automataPrevious = automataPrevious;
        publishedTransportSnapshot.transportPhase = 0.0f;
        for (int i = 0; i < maxSnakes; ++i)
            publishedTransportSnapshot.snakes[(size_t) i] = audioSnakes[(size_t) i];
        transportSequence.fetch_add (1, std::memory_order_release);
    }
}

void MainComponent::advanceSnakesToTick (int targetTick, const PatchSnapshot& snapshot)
{
    auto nextTick = lastAdvancedTick.load() + 1;

    if (lastAdvancedTick.load() < 0)
        nextTick = targetTick;

    while (nextTick <= targetTick)
    {
        for (int snakeIndex = 0; snakeIndex < audioSnakeCount; ++snakeIndex)
            advanceSnake (audioSnakes[(size_t) snakeIndex], snapshot);

        ++nextTick;
    }
}

void MainComponent::resetAutomata (const PatchSnapshot& snapshot) noexcept
{
    automataCurrent.fill (0);
    automataPrevious.fill (0);

    for (int layer = 0; layer < layers; ++layer)
    {
        for (int row = 0; row < rows; ++row)
        {
            for (int col = 0; col < cols; ++col)
            {
                const auto& cell = snapshot.cells[(size_t) getCellIndex (layer, row, col)];
                const auto musicallySeeded = cell.type == GlyphType::pulse
                                          || cell.type == GlyphType::tone
                                          || cell.type == GlyphType::voice
                                          || cell.type == GlyphType::chord
                                          || cell.type == GlyphType::key
                                          || cell.type == GlyphType::octave
                                          || cell.type == GlyphType::audio
                                          || cell.type == GlyphType::wormhole;

                if (musicallySeeded && (((layer * 2 + row + col) % 3) == 0 || cell.type == GlyphType::wormhole))
                    automataCurrent[(size_t) getCellIndex (layer, row, col)] = 1;
            }
        }
    }
}

void MainComponent::seedAutomataBurst() noexcept
{
    auto seeded = false;
    for (int layer = 0; layer < layers; ++layer)
    {
        for (int row = 0; row < rows; ++row)
        {
            for (int col = 0; col < cols; ++col)
            {
                const auto index = (size_t) getCellIndex (layer, row, col);
                if (automataCurrent[index] != 0)
                    continue;

                if (((layer * 7 + row * 5 + col * 3 + random.nextInt (11)) % 9) == 0)
                {
                    automataCurrent[index] = 1;
                    seeded = true;
                }
            }
        }
    }

    if (! seeded)
    {
        const auto layer = random.nextInt (layers);
        const auto row = random.nextInt (rows);
        const auto col = random.nextInt (cols);
        automataCurrent[(size_t) getCellIndex (layer, row, col)] = 1;
    }
}

void MainComponent::advanceAutomataToTick (int targetTick, const PatchSnapshot& snapshot) noexcept
{
    auto nextTick = lastAdvancedTick.load() + 1;

    if (lastAdvancedTick.load() < 0)
        nextTick = targetTick;

    while (nextTick <= targetTick)
    {
        advanceAutomataGeneration (snapshot);
        ++nextTick;
    }
}

int MainComponent::countAutomataNeighbours (const std::array<uint8_t, cols * rows * layers>& state, int layer, int row, int col) const noexcept
{
    auto count = 0;
    for (int dz = -1; dz <= 1; ++dz)
    {
        for (int dy = -1; dy <= 1; ++dy)
        {
            for (int dx = -1; dx <= 1; ++dx)
            {
                if (dx == 0 && dy == 0 && dz == 0)
                    continue;

                const auto nLayer = layer + dz;
                const auto nRow = row + dy;
                const auto nCol = col + dx;
                if (! juce::isPositiveAndBelow (nLayer, layers)
                    || ! juce::isPositiveAndBelow (nRow, rows)
                    || ! juce::isPositiveAndBelow (nCol, cols))
                    continue;

                count += state[(size_t) getCellIndex (nLayer, nRow, nCol)] != 0 ? 1 : 0;
            }
        }
    }
    return count;
}

void MainComponent::advanceAutomataGeneration (const PatchSnapshot& snapshot) noexcept
{
    automataPrevious = automataCurrent;
    std::array<uint8_t, cols * rows * layers> next {};
    auto livingCount = 0;

    for (int layer = 0; layer < layers; ++layer)
    {
        for (int row = 0; row < rows; ++row)
        {
            for (int col = 0; col < cols; ++col)
            {
                const auto index = (size_t) getCellIndex (layer, row, col);
                const auto alive = automataPrevious[index] != 0;
                const auto neighbours = countAutomataNeighbours (automataPrevious, layer, row, col);
                const auto& cell = snapshot.cells[index];
                const auto musicallyInteresting = cell.type == GlyphType::pulse
                                               || cell.type == GlyphType::tone
                                               || cell.type == GlyphType::voice
                                               || cell.type == GlyphType::chord
                                               || cell.type == GlyphType::key
                                               || cell.type == GlyphType::octave
                                               || cell.type == GlyphType::audio
                                               || cell.type == GlyphType::wormhole;
                const auto birthMin = juce::jlimit (0, 26, currentCellularBirthRange.min);
                const auto birthMax = juce::jlimit (birthMin, 26, currentCellularBirthRange.max);
                const auto surviveMin = juce::jlimit (0, 26, currentCellularSurviveRange.min);
                const auto surviveMax = juce::jlimit (surviveMin, 26, currentCellularSurviveRange.max);

                auto staysAlive = alive && neighbours >= surviveMin && neighbours <= surviveMax;
                auto becomesAlive = ! alive && neighbours >= birthMin && neighbours <= birthMax;

                // Each named rule keeps a small personality on top of the explicit ranges.
                switch (currentCellularRule)
                {
                    case CellularRule::bloom:
                        if (musicallyInteresting && ! alive && neighbours == birthMax + 1)
                            becomesAlive = true;
                        break;
                    case CellularRule::maze:
                        if (alive && neighbours == surviveMax + 1)
                            staysAlive = true;
                        break;
                    case CellularRule::coral:
                        if (! alive && musicallyInteresting && neighbours == juce::jmax (0, birthMin - 1))
                            becomesAlive = true;
                        break;
                    case CellularRule::pulse:
                        if (alive && ((layer + row + col + juce::jmax (0, lastAdvancedTick.load())) % 2) == 0 && neighbours == surviveMin)
                            staysAlive = true;
                        break;
                }

                if (musicallyInteresting && ! alive && neighbours >= 4 && neighbours <= 6)
                    becomesAlive = true;

                if (cell.type == GlyphType::wormhole && alive)
                    staysAlive = true;

                next[index] = (staysAlive || becomesAlive) ? 1 : 0;
                livingCount += next[index] != 0 ? 1 : 0;
            }
        }
    }

    if (livingCount < 18)
    {
        for (int layer = 0; layer < layers; ++layer)
        {
            for (int row = 0; row < rows; ++row)
            {
                for (int col = 0; col < cols; ++col)
                {
                    const auto index = (size_t) getCellIndex (layer, row, col);
                    const auto& cell = snapshot.cells[index];
                    const auto isAnchor = cell.type == GlyphType::pulse
                                       || cell.type == GlyphType::tone
                                       || cell.type == GlyphType::voice
                                       || cell.type == GlyphType::chord
                                       || cell.type == GlyphType::key
                                       || cell.type == GlyphType::octave
                                       || cell.type == GlyphType::audio
                                       || cell.type == GlyphType::wormhole;

                    if (! isAnchor)
                        continue;

                    if (((layer + row * 2 + col + juce::jmax (0, lastAdvancedTick.load())) % 3) == 0 || cell.type == GlyphType::wormhole)
                    {
                        if (next[index] == 0)
                        {
                            next[index] = 1;
                            ++livingCount;
                        }

                        for (int dz = -1; dz <= 1; ++dz)
                        {
                            for (int dy = -1; dy <= 1; ++dy)
                            {
                                for (int dx = -1; dx <= 1; ++dx)
                                {
                                    if (std::abs (dx) + std::abs (dy) + std::abs (dz) != 1)
                                        continue;

                                    const auto nLayer = layer + dz;
                                    const auto nRow = row + dy;
                                    const auto nCol = col + dx;
                                    if (! juce::isPositiveAndBelow (nLayer, layers)
                                        || ! juce::isPositiveAndBelow (nRow, rows)
                                        || ! juce::isPositiveAndBelow (nCol, cols))
                                        continue;

                                    const auto neighbourIndex = (size_t) getCellIndex (nLayer, nRow, nCol);
                                    if (next[neighbourIndex] == 0 && ((nLayer + nRow + nCol) % 2 == 0))
                                    {
                                        next[neighbourIndex] = 1;
                                        ++livingCount;
                                    }
                                }
                            }
                        }
                    }

                    if (livingCount >= 28)
                        break;
                }

                if (livingCount >= 28)
                    break;
            }

            if (livingCount >= 28)
                break;
        }
    }

    automataCurrent = next;
}

void MainComponent::resetAudioSnakes() noexcept
{
    for (auto& snake : audioSnakes)
        snake = {};

    audioSnakeCount = 1;
    auto& snake = audioSnakes[0];
    snake.active = true;
    snake.length = snakeLength;
    snake.previousLength = snakeLength;
    snake.directionX = 1;
    snake.directionY = 0;
    snake.directionLayer = 0;
    snake.ticksOnCurrentLayer = 0;
    snake.lengthDelta = 0;
    snake.recentLengthChangeTicks = 0;
    snake.colour = juce::Colour::fromHSV (0.14f, 0.88f, 0.98f, 1.0f);

    for (int i = 0; i < snakeLength; ++i)
    {
        snake.segments[(size_t) i] = { 0, rows / 2, juce::jmax (0, cols / 2 - i) };
        snake.previousSegments[(size_t) i] = snake.segments[(size_t) i];
    }

    spawnAudioSnake();
    spawnAudioSnake();
}

void MainComponent::spawnAudioSnake() noexcept
{
    if (audioSnakeCount >= maxSnakes)
        return;

    for (int row = 0; row < rows; ++row)
    {
        for (int col = 0; col < cols; ++col)
        {
            const auto preferredLayer = (audioSnakeCount * 2) % layers;

            for (int layerOffset = 0; layerOffset < layers; ++layerOffset)
            {
                const auto layer = (preferredLayer + layerOffset) % layers;
                auto occupied = false;

                for (int snakeIndex = 0; snakeIndex < audioSnakeCount; ++snakeIndex)
                    if (isSnakeCellOccupied (audioSnakes[(size_t) snakeIndex], layer, row, col, false))
                        occupied = true;

                if (occupied || col < snakeLength - 1)
                    continue;

                auto& snake = audioSnakes[(size_t) audioSnakeCount];
                snake = {};
                snake.active = true;
                snake.length = snakeLength;
                snake.previousLength = snakeLength;
                snake.directionX = (audioSnakeCount % 2 == 0) ? 1 : -1;
                snake.directionY = 0;
                snake.directionLayer = (audioSnakeCount % 3 == 0) ? 1 : ((audioSnakeCount % 3 == 1) ? -1 : 0);
                snake.ticksOnCurrentLayer = 0;
                snake.lengthDelta = 0;
                snake.recentLengthChangeTicks = 0;
                snake.colour = juce::Colour::fromHSV (std::fmod (0.14f + 0.17f * (float) audioSnakeCount, 1.0f), 0.88f, 0.98f, 1.0f);

                for (int i = 0; i < snakeLength; ++i)
                {
                    const auto bodyCol = juce::jlimit (0, cols - 1, col - snake.directionX * i);
                    snake.segments[(size_t) i] = { layer, row, bodyCol };
                    snake.previousSegments[(size_t) i] = snake.segments[(size_t) i];
                }

                ++audioSnakeCount;
                return;
            }
        }
    }
}

void MainComponent::advanceSnake (SnakeRuntime& snake, const PatchSnapshot& snapshot) noexcept
{
    if (! snake.active || snake.length <= 0)
        return;

    snake.previousSegments = snake.segments;

    const auto head = snake.segments[0];
    struct Move3D
    {
        int dx = 0;
        int dy = 0;
        int dz = 0;
    };

    const auto tick = juce::jmax (0, lastAdvancedTick.load());
    auto planarDirX = snake.directionX;
    auto planarDirY = snake.directionY;

    if (planarDirX == 0 && planarDirY == 0)
    {
        planarDirX = ((head.row + tick) % 2 == 0) ? 1 : -1;
        planarDirY = 0;
    }

    const auto leftX = planarDirY;
    const auto leftY = -planarDirX;
    const auto rightX = -planarDirY;
    const auto rightY = planarDirX;
    const auto preferPositiveLayer = ((head.col + tick) % 2) == 0;
    const auto verticalStep = snake.directionLayer == 0 ? (preferPositiveLayer ? 1 : -1) : snake.directionLayer;
    const auto alternateVerticalStep = verticalStep > 0 ? -1 : 1;
    const auto movementPhase = (head.row * 5 + head.col * 3 + head.layer * 7 + tick) % 4;
    const auto mustLeaveLayer = snake.ticksOnCurrentLayer >= 16 * 16;

    std::array<Move3D, 10> candidateMoves
    {{
        { planarDirX, planarDirY, movementPhase == 0 ? verticalStep : 0 },
        { leftX, leftY, movementPhase == 1 ? verticalStep : 0 },
        { rightX, rightY, movementPhase == 2 ? verticalStep : 0 },
        { planarDirX, planarDirY, 0 },
        { leftX, leftY, 0 },
        { rightX, rightY, 0 },
        { planarDirX, planarDirY, movementPhase == 3 ? alternateVerticalStep : verticalStep },
        { leftX, leftY, alternateVerticalStep },
        { rightX, rightY, alternateVerticalStep },
        { 0, 0, verticalStep }
    }};

    if (mustLeaveLayer)
    {
        candidateMoves = std::array<Move3D, 10>
        {{
            { planarDirX, planarDirY, verticalStep },
            { leftX, leftY, verticalStep },
            { rightX, rightY, verticalStep },
            { planarDirX, planarDirY, alternateVerticalStep },
            { leftX, leftY, alternateVerticalStep },
            { rightX, rightY, alternateVerticalStep },
            { 0, 0, verticalStep },
            { 0, 0, alternateVerticalStep },
            { planarDirX, planarDirY, 0 },
            { -planarDirX, -planarDirY, verticalStep }
        }};
    }

    auto chosenMove = Move3D { planarDirX, planarDirY, 0 };

    for (auto move : candidateMoves)
    {
        if (move.dx == 0 && move.dy == 0 && move.dz == 0)
            continue;

        const auto nextLayer = head.layer + move.dz;
        const auto nextRow = head.row + move.dy;
        const auto nextCol = head.col + move.dx;

        if (nextLayer < 0 || nextLayer >= layers
            || nextRow < 0 || nextRow >= rows
            || nextCol < 0 || nextCol >= cols)
            continue;

        if (isSnakeCellOccupied (snake, nextLayer, nextRow, nextCol, true))
            continue;

        chosenMove = move;
        break;
    }

    snake.directionX = chosenMove.dx;
    snake.directionY = chosenMove.dy;
    snake.directionLayer = chosenMove.dz;

    auto newHead = SnakeSegment { head.layer + snake.directionLayer, head.row + snake.directionY, head.col + snake.directionX };

    if (newHead.layer < 0 || newHead.layer >= layers
        || newHead.row < 0 || newHead.row >= rows
        || newHead.col < 0 || newHead.col >= cols)
        newHead = head;

    const auto& landingCell = snapshot.cells[(size_t) getCellIndex (newHead.layer, newHead.row, newHead.col)];
    if (landingCell.type == GlyphType::wormhole)
    {
        const auto wormholeId = juce::jmax (1, landingCell.code.getIntValue());
        auto fallbackDestination = newHead;
        auto bestDistance = -1;

        for (const auto& cell : snapshot.cells)
        {
            if (cell.type != GlyphType::wormhole
                || cell.layer == newHead.layer && cell.row == newHead.row && cell.col == newHead.col
                || juce::jmax (1, cell.code.getIntValue()) != wormholeId)
                continue;

            const auto distance = std::abs (cell.col - newHead.col)
                                + std::abs (cell.row - newHead.row)
                                + std::abs (cell.layer - newHead.layer);

            if (distance > bestDistance
                && ! isSnakeCellOccupied (snake, cell.layer, cell.row, cell.col, true))
            {
                bestDistance = distance;
                fallbackDestination = { cell.layer, cell.row, cell.col };
            }
        }

        if (bestDistance >= 0)
        {
            snake.directionX = juce::jlimit (-1, 1, fallbackDestination.col - newHead.col);
            snake.directionY = juce::jlimit (-1, 1, fallbackDestination.row - newHead.row);
            snake.directionLayer = juce::jlimit (-1, 1, fallbackDestination.layer - newHead.layer);
            newHead = fallbackDestination;
        }
    }

    for (int i = juce::jmin (snake.length - 1, snakeLength - 1); i > 0; --i)
        snake.segments[(size_t) i] = snake.segments[(size_t) (i - 1)];

    snake.segments[0] = newHead;

    if (newHead.layer == head.layer)
        ++snake.ticksOnCurrentLayer;
    else
        snake.ticksOnCurrentLayer = 0;
}

bool MainComponent::isSnakeCellOccupied (const SnakeRuntime& snake, int layer, int row, int col, bool ignoreTail) const noexcept
{
    const auto limit = ignoreTail ? snake.length - 1 : snake.length;

    for (int i = 0; i < limit; ++i)
    {
        auto& segment = snake.segments[(size_t) i];

        if (segment.layer == layer && segment.row == row && segment.col == col)
            return true;
    }

    return false;
}

juce::Array<MainComponent::SnakeSegment> MainComponent::collectTriggeredCells (const juce::Array<Snake>& snakes)
{
    juce::Array<SnakeSegment> triggered;

    for (auto& snake : snakes)
        for (auto& segment : snake.segments)
            triggered.add (segment);

    return triggered;
}

void MainComponent::recompileCell (Cell& cell)
{
    cell.program = ExpressionEvaluator::compile (cell.code);
}

bool MainComponent::savePatchToFile (const juce::File& file)
{
    auto targetFile = file;
    if (! targetFile.hasFileExtension ("glyphgrid.json"))
        targetFile = targetFile.withFileExtension (".glyphgrid.json");

    auto panelModeToString = [] (PanelSizeMode mode)
    {
        switch (mode)
        {
            case PanelSizeMode::defaultSize: return "default";
            case PanelSizeMode::halfScreen:  return "half";
            case PanelSizeMode::hidden:      return "hidden";
        }

        return "default";
    };

    auto root = std::make_unique<juce::DynamicObject>();
    root->setProperty ("format", "GlyphGridPatch");
    root->setProperty ("version", 1);
    root->setProperty ("mode", modeToString (currentMode));
    root->setProperty ("variant", modeVariantToString (currentVariant));
    root->setProperty ("bpm", bpm);
    root->setProperty ("visibleLayer", visibleLayer);
    root->setProperty ("selectedTool", glyphTypeToString (selectedTool));
    root->setProperty ("previewMode", panelModeToString (previewSizeMode));
    root->setProperty ("inspectorMode", panelModeToString (inspectorSizeMode));
    root->setProperty ("useIsoView", usePseudoDepthStageView);
    root->setProperty ("sidebarExpanded", isSidebarExpanded);
    root->setProperty ("harmonySpaceKeyCenter", harmonySpaceKeyCenter);
    root->setProperty ("harmonySpaceConstraintMode", harmonySpaceConstraintMode);
    root->setProperty ("harmonySpaceGestureRecordEnabled", harmonySpaceGestureRecordEnabled);
    root->setProperty ("cellularRule", (int) currentCellularRule);
    root->setProperty ("modePresetIndex", modePresetIndices[(size_t) modeIndex (currentMode)]);
    root->setProperty ("cellularBirthMin", currentCellularBirthRange.min);
    root->setProperty ("cellularBirthMax", currentCellularBirthRange.max);
    root->setProperty ("cellularSurviveMin", currentCellularSurviveRange.min);
    root->setProperty ("cellularSurviveMax", currentCellularSurviveRange.max);

    juce::var mixerStripsVar { juce::Array<juce::var>() };
    auto* mixerStripsArray = mixerStripsVar.getArray();
    for (const auto& strip : mixerStrips)
    {
        auto stripObject = std::make_unique<juce::DynamicObject>();
        stripObject->setProperty ("name", strip.name);
        stripObject->setProperty ("volume", strip.volume);
        stripObject->setProperty ("pan", strip.pan);
        stripObject->setProperty ("midiFxPath", strip.midiFx.pluginPath);
        stripObject->setProperty ("midiFxBypassed", strip.midiFx.bypassed);
        stripObject->setProperty ("midiFxState", serialisePluginState (strip.midiFx));
        stripObject->setProperty ("instrumentPath", strip.instrument.pluginPath);
        stripObject->setProperty ("instrumentBypassed", strip.instrument.bypassed);
        stripObject->setProperty ("instrumentState", serialisePluginState (strip.instrument));
        stripObject->setProperty ("effectAPath", strip.effectA.pluginPath);
        stripObject->setProperty ("effectABypassed", strip.effectA.bypassed);
        stripObject->setProperty ("effectAState", serialisePluginState (strip.effectA));
        stripObject->setProperty ("effectBPath", strip.effectB.pluginPath);
        stripObject->setProperty ("effectBBypassed", strip.effectB.bypassed);
        stripObject->setProperty ("effectBState", serialisePluginState (strip.effectB));
        stripObject->setProperty ("effectCPath", strip.effectC.pluginPath);
        stripObject->setProperty ("effectCBypassed", strip.effectC.bypassed);
        stripObject->setProperty ("effectCState", serialisePluginState (strip.effectC));
        stripObject->setProperty ("effectDPath", strip.effectD.pluginPath);
        stripObject->setProperty ("effectDBypassed", strip.effectD.bypassed);
        stripObject->setProperty ("effectDState", serialisePluginState (strip.effectD));
        mixerStripsArray->add (juce::var (stripObject.release()));
    }
    root->setProperty ("mixerStrips", mixerStripsVar);

    juce::var gesturePoints { juce::Array<juce::var>() };
    auto* gestureArray = gesturePoints.getArray();
    for (const auto& point : harmonySpaceGesturePoints)
    {
        auto pointObject = std::make_unique<juce::DynamicObject>();
        pointObject->setProperty ("row", point.x);
        pointObject->setProperty ("col", point.y);
        gestureArray->add (juce::var (pointObject.release()));
    }
    root->setProperty ("harmonySpaceGesturePoints", gesturePoints);

    if (selectedCell != nullptr)
    {
        auto selected = std::make_unique<juce::DynamicObject>();
        selected->setProperty ("layer", selectedCell->layer);
        selected->setProperty ("row", selectedCell->row);
        selected->setProperty ("col", selectedCell->col);
        root->setProperty ("selectedCell", juce::var (selected.release()));
    }

    juce::var cells { juce::Array<juce::var>() };
    auto* cellArray = cells.getArray();
    cellArray->ensureStorageAllocated (grid.size());

    for (const auto& cell : grid)
    {
        auto cellObject = std::make_unique<juce::DynamicObject>();
        cellObject->setProperty ("layer", cell.layer);
        cellObject->setProperty ("row", cell.row);
        cellObject->setProperty ("col", cell.col);
        cellObject->setProperty ("type", glyphTypeToString (cell.type));
        cellObject->setProperty ("label", cell.label);
        cellObject->setProperty ("code", cell.code);
        cellArray->add (juce::var (cellObject.release()));
    }

    root->setProperty ("cells", cells);

    const auto json = juce::JSON::toString (juce::var (root.release()), true);
    const auto saved = targetFile.replaceWithText (json);

    if (saved)
    {
        currentPatchName = targetFile.getFileNameWithoutExtension().upToLastOccurrenceOf (".glyphgrid", false, false);
        if (currentPatchName.isEmpty())
            currentPatchName = targetFile.getFileNameWithoutExtension();
        patchTitle.setText (currentPatchName, juce::dontSendNotification);
    }

    return saved;
}

bool MainComponent::loadPatchFromFile (const juce::File& file)
{
    const auto parsed = juce::JSON::parse (file.loadFileAsString());
    auto* root = parsed.getDynamicObject();

    if (root == nullptr)
        return false;

    auto panelModeFromString = [] (const juce::String& text)
    {
        if (text == "half")
            return PanelSizeMode::halfScreen;
        if (text == "hidden")
            return PanelSizeMode::hidden;
        return PanelSizeMode::defaultSize;
    };
    const auto getPropertyOr = [&] (const juce::Identifier& name, const juce::var& fallback) -> juce::var
    {
        return root->hasProperty (name) ? root->getProperty (name) : fallback;
    };

    grid.clearQuick();
    grid.ensureStorageAllocated (cols * rows * layers);

    for (int layer = 0; layer < layers; ++layer)
        for (int row = 0; row < rows; ++row)
            for (int col = 0; col < cols; ++col)
            {
                Cell cell;
                cell.layer = layer;
                cell.row = row;
                cell.col = col;
                cell.type = GlyphType::empty;
                cell.label = getGlyphDefinition (GlyphType::empty).label;
                cell.code = getGlyphDefinition (GlyphType::empty).defaultCode;
                cell.program = ExpressionEvaluator::compile (cell.code);
                grid.add (cell);
            }

    if (auto* cellArray = root->getProperty ("cells").getArray())
    {
        for (const auto& cellVar : *cellArray)
        {
            auto* cellObject = cellVar.getDynamicObject();
            if (cellObject == nullptr)
                continue;

            const auto layer = juce::jlimit (0, layers - 1, (int) cellObject->getProperty ("layer"));
            const auto row = juce::jlimit (0, rows - 1, (int) cellObject->getProperty ("row"));
            const auto col = juce::jlimit (0, cols - 1, (int) cellObject->getProperty ("col"));

            if (auto* cell = getCell (layer, row, col))
            {
                cell->type = toolIdToGlyph (cellObject->getProperty ("type").toString());
                cell->label = cellObject->hasProperty ("label")
                                ? cellObject->getProperty ("label").toString()
                                : getGlyphDefinition (cell->type).label;
                cell->code = cellObject->hasProperty ("code")
                                ? cellObject->getProperty ("code").toString()
                                : getGlyphDefinition (cell->type).defaultCode;
                recompileCell (*cell);
            }
        }
    }

    bpm = (double) getPropertyOr ("bpm", bpm);
    currentMode = stringToMode (getPropertyOr ("mode", modeToString (AppMode::glyphGrid)).toString());
    currentVariant = stringToModeVariant (getPropertyOr ("variant", "A").toString());
    visibleLayer = juce::jlimit (0, layers - 1, (int) getPropertyOr ("visibleLayer", 0));
    selectedTool = toolIdToGlyph (getPropertyOr ("selectedTool", glyphTypeToString (GlyphType::tone)).toString());
    previewSizeMode = panelModeFromString (getPropertyOr ("previewMode", "default").toString());
    inspectorSizeMode = panelModeFromString (getPropertyOr ("inspectorMode", "default").toString());
    usePseudoDepthStageView = true;
    showingTitleScreen = false;
    hasResumableSession = true;
    isSidebarExpanded = (bool) getPropertyOr ("sidebarExpanded", false);
    harmonySpaceKeyCenter = juce::jlimit (0, 11, (int) getPropertyOr ("harmonySpaceKeyCenter", 0));
    harmonySpaceConstraintMode = juce::jlimit (0, 2, (int) getPropertyOr ("harmonySpaceConstraintMode", 0));
    harmonySpaceGestureRecordEnabled = (bool) getPropertyOr ("harmonySpaceGestureRecordEnabled", false);
    currentCellularRule = (CellularRule) juce::jlimit (0, 3, (int) getPropertyOr ("cellularRule", (int) CellularRule::bloom));
    modePresetIndices[(size_t) modeIndex (currentMode)] = juce::jlimit (0, 2, (int) getPropertyOr ("modePresetIndex", 0));
    currentCellularBirthRange = { (int) getPropertyOr ("cellularBirthMin", 5), (int) getPropertyOr ("cellularBirthMax", 5) };
    currentCellularSurviveRange = { (int) getPropertyOr ("cellularSurviveMin", 4), (int) getPropertyOr ("cellularSurviveMax", 5) };
    caRuleButton.setButtonText ("Rule: " + cellularRuleName (currentCellularRule));
    presetButton.setButtonText ("Preset " + juce::String (modePresetIndices[(size_t) modeIndex (currentMode)] + 1));
    caBirthButton.setButtonText (cellularRangeLabel ("Birth", currentCellularBirthRange));
    caSurviveButton.setButtonText (cellularRangeLabel ("Stay", currentCellularSurviveRange));
    harmonySpaceGesturePoints.clear();
    if (auto* gestureArray = root->getProperty ("harmonySpaceGesturePoints").getArray())
    {
        for (const auto& pointVar : *gestureArray)
        {
            if (auto* pointObject = pointVar.getDynamicObject())
                harmonySpaceGesturePoints.add ({ (int) pointObject->getProperty ("row"), (int) pointObject->getProperty ("col") });
        }
    }
    sidebarToggleButton.setButtonText (isSidebarExpanded ? "Hide" : "...");
    toolValue.setText ("", juce::dontSendNotification);
    updateTempoControl();

    if (auto* stripsArray = root->getProperty ("mixerStrips").getArray())
    {
        for (int stripIndex = 0; stripIndex < juce::jmin ((int) stripsArray->size(), (int) mixerStrips.size()); ++stripIndex)
        {
            if (auto* stripObject = (*stripsArray)[(size_t) stripIndex].getDynamicObject())
            {
                mixerStrips[(size_t) stripIndex].name = stripObject->getProperty ("name").toString();
                mixerStrips[(size_t) stripIndex].volume = (float) (double) stripObject->getProperty ("volume");
                mixerStrips[(size_t) stripIndex].pan = (float) (double) stripObject->getProperty ("pan");
                mixerVolumeSliders[(size_t) stripIndex].setValue (mixerStrips[(size_t) stripIndex].volume, juce::dontSendNotification);
                mixerPanSliders[(size_t) stripIndex].setValue (mixerStrips[(size_t) stripIndex].pan, juce::dontSendNotification);
                mixerStripLabels[(size_t) stripIndex].setText (mixerStrips[(size_t) stripIndex].name, juce::dontSendNotification);

                const auto instrumentPath = stripObject->getProperty ("instrumentPath").toString();
                const auto instrumentState = stripObject->getProperty ("instrumentState").toString();
                const auto effectAPath = stripObject->getProperty ("effectAPath").toString();
                const auto effectAState = stripObject->getProperty ("effectAState").toString();
                const auto effectBPath = stripObject->getProperty ("effectBPath").toString();
                const auto effectBState = stripObject->getProperty ("effectBState").toString();
                const auto midiFxPath = stripObject->getProperty ("midiFxPath").toString();
                const auto midiFxState = stripObject->getProperty ("midiFxState").toString();
                const auto midiFxBypassed = (bool) stripObject->getProperty ("midiFxBypassed");
                const auto effectCPath = stripObject->getProperty ("effectCPath").toString();
                const auto effectCState = stripObject->getProperty ("effectCState").toString();
                const auto effectABypassed = (bool) stripObject->getProperty ("effectABypassed");
                const auto effectBBypassed = (bool) stripObject->getProperty ("effectBBypassed");
                const auto effectCBypassed = (bool) stripObject->getProperty ("effectCBypassed");
                const auto effectDPath = stripObject->getProperty ("effectDPath").toString();
                const auto effectDState = stripObject->getProperty ("effectDState").toString();
                const auto effectDBypassed = (bool) stripObject->getProperty ("effectDBypassed");
                const auto instrumentBypassed = (bool) stripObject->getProperty ("instrumentBypassed");
                if (midiFxPath.isNotEmpty())
                {
                    loadPluginIntoSlot (stripIndex, PluginSlotKind::midiFx, juce::File (midiFxPath));
                    restorePluginState (mixerStrips[(size_t) stripIndex].midiFx, midiFxState);
                    mixerStrips[(size_t) stripIndex].midiFx.bypassed = midiFxBypassed;
                }
                if (instrumentPath.isNotEmpty())
                {
                    loadPluginIntoSlot (stripIndex, PluginSlotKind::instrument, juce::File (instrumentPath));
                    restorePluginState (mixerStrips[(size_t) stripIndex].instrument, instrumentState);
                    mixerStrips[(size_t) stripIndex].instrument.bypassed = instrumentBypassed;
                }
                if (effectAPath.isNotEmpty())
                {
                    loadPluginIntoSlot (stripIndex, PluginSlotKind::effectA, juce::File (effectAPath));
                    restorePluginState (mixerStrips[(size_t) stripIndex].effectA, effectAState);
                    mixerStrips[(size_t) stripIndex].effectA.bypassed = effectABypassed;
                }
                if (effectBPath.isNotEmpty())
                {
                    loadPluginIntoSlot (stripIndex, PluginSlotKind::effectB, juce::File (effectBPath));
                    restorePluginState (mixerStrips[(size_t) stripIndex].effectB, effectBState);
                    mixerStrips[(size_t) stripIndex].effectB.bypassed = effectBBypassed;
                }
                if (effectCPath.isNotEmpty())
                {
                    loadPluginIntoSlot (stripIndex, PluginSlotKind::effectC, juce::File (effectCPath));
                    restorePluginState (mixerStrips[(size_t) stripIndex].effectC, effectCState);
                    mixerStrips[(size_t) stripIndex].effectC.bypassed = effectCBypassed;
                }
                if (effectDPath.isNotEmpty())
                {
                    loadPluginIntoSlot (stripIndex, PluginSlotKind::effectD, juce::File (effectDPath));
                    restorePluginState (mixerStrips[(size_t) stripIndex].effectD, effectDState);
                    mixerStrips[(size_t) stripIndex].effectD.bypassed = effectDBypassed;
                }
            }
        }
    }

    selectedCell = getCell (visibleLayer, 0, 0);
    if (auto* selected = root->getProperty ("selectedCell").getDynamicObject())
        selectedCell = getCell (juce::jlimit (0, layers - 1, (int) selected->getProperty ("layer")),
                                juce::jlimit (0, rows - 1, (int) selected->getProperty ("row")),
                                juce::jlimit (0, cols - 1, (int) selected->getProperty ("col")));

    currentPatchName = file.getFileNameWithoutExtension().upToLastOccurrenceOf (".glyphgrid", false, false);
    if (currentPatchName.isEmpty())
        currentPatchName = file.getFileNameWithoutExtension();
    patchTitle.setText (currentPatchName, juce::dontSendNotification);
    stage.setPseudoDepthEnabled (true);
    scoreLabel.setText ("Score | Iso", juce::dontSendNotification);
    mixerVisible = false;

    initialiseSnakes();
    lastAdvancedTick.store (-1);
    resetSynthVoices();
    resetDrumVoices();
    publishPatchSnapshot();
    renderInspector();
    updateCellButtons();
    updateMixerButtons();
    resized();
    repaint();
    return true;
}

MainComponent::GlyphDefinition MainComponent::getGlyphDefinition (GlyphType type)
{
    switch (type)
    {
        case GlyphType::pulse:  return { "gate", "G", juce::Colour (0xffffd449), "1" };
        case GlyphType::tone:   return { "note", "N", juce::Colour (0xff00e5ff), "60 + row * 2" };
        case GlyphType::voice:  return { "lead", "Ld", juce::Colour (0xff52f7d4), "48 + row * 5" };
        case GlyphType::chord:  return { "chord", "Ch", juce::Colour (0xff4bf0ff), "2" };
        case GlyphType::key:    return { "key", "Ky", juce::Colour (0xff4dffb2), "0" };
        case GlyphType::octave: return { "octave", "Oc", juce::Colour (0xff7fe0ff), "1" };
        case GlyphType::route:  return { "route", "Ro", juce::Colour (0xff8ef1ff), "1" };
        case GlyphType::channel:return { "channel", "Cn", juce::Colour (0xff56b7ff), "1" };
        case GlyphType::cc:     return { "cc", "CC", juce::Colour (0xff8dffdd), "1=96" };
        case GlyphType::parameter: return { "param", "Pm", juce::Colour (0xffffdb7d), "cutoff=0.72" };
        case GlyphType::tempo:  return { "tempo", "Tp", juce::Colour (0xffffa64d), "1.5" };
        case GlyphType::ratchet:return { "ratchet", "Rt", juce::Colour (0xffff6b6b), "3" };
        case GlyphType::repeat: return { "repeat", "Rp", juce::Colour (0xffff8cff), "2" };
        case GlyphType::wormhole: return { "wormhole", "Wh", juce::Colour (0xff6f8dff), "1" };
        case GlyphType::kick:   return { "kick", "K", juce::Colour (0xffff5f5f), "1" };
        case GlyphType::snare:  return { "snare", "S", juce::Colour (0xffffb36b), "1" };
        case GlyphType::hat:    return { "hat", "Hh", juce::Colour (0xfff5ef7a), "1" };
        case GlyphType::clap:   return { "clap", "Cp", juce::Colour (0xffff8ad8), "1" };
        case GlyphType::length: return { "length", "Ln", juce::Colour (0xfff7ff65), "3" };
        case GlyphType::accent: return { "accent", "Ac", juce::Colour (0xffffef4d), "1.6" };
        case GlyphType::decay:  return { "decay", "Dc", juce::Colour (0xff8fffa1), "1.4" };
        case GlyphType::noise:  return { "air", "Ar", juce::Colour (0xffff8a3d), "0.12" };
        case GlyphType::mul:    return { "level", "Lv", juce::Colour (0xff7cff8a), "0.8" };
        case GlyphType::bias:   return { "shift", "Sh", juce::Colour (0xffff5ca8), "0.12" };
        case GlyphType::hue:    return { "color", "Co", juce::Colour (0xffbb86ff), "0.56 + row * 0.03" };
        case GlyphType::audio:  return { "out", "Out", juce::Colour (0xff00ffd5), "1" };
        case GlyphType::visual: return { "glow", "Gl", juce::Colour (0xffff67d9), "1" };
        case GlyphType::empty:  return { "empty", ".", juce::Colours::transparentBlack, "0" };
    }

    return { "empty", ".", juce::Colours::transparentBlack, "0" };
}

juce::String MainComponent::glyphTypeToString (GlyphType type)
{
    return getGlyphDefinition (type).label;
}

juce::String MainComponent::modeToString (AppMode mode)
{
    if (mode == AppMode::cellularGrid)
        return "cellularGrid";
    if (mode == AppMode::harmonicGeometry)
        return "harmonicGeometry";
    if (mode == AppMode::harmonySpace)
        return "harmonySpace";
    return "glyphGrid";
}

MainComponent::AppMode MainComponent::stringToMode (const juce::String& text)
{
    if (text == "cellularGrid")
        return AppMode::cellularGrid;
    if (text == "harmonicGeometry")
        return AppMode::harmonicGeometry;
    if (text == "harmonySpace")
        return AppMode::harmonySpace;
    return AppMode::glyphGrid;
}

juce::Colour MainComponent::mixerSlotAccentColour (PluginSlotKind slotKind) noexcept
{
    switch (slotKind)
    {
        case PluginSlotKind::midiFx:     return juce::Colour::fromFloatRGBA (1.0f, 0.42f, 0.86f, 1.0f);
        case PluginSlotKind::instrument: return juce::Colour::fromFloatRGBA (0.26f, 0.96f, 1.0f, 1.0f);
        case PluginSlotKind::effectA:
        case PluginSlotKind::effectB:
        case PluginSlotKind::effectC:
        case PluginSlotKind::effectD:    return juce::Colour::fromFloatRGBA (1.0f, 0.82f, 0.24f, 1.0f);
    }

    return juce::Colours::white;
}

juce::String MainComponent::mixerSlotPrefix (PluginSlotKind slotKind)
{
    switch (slotKind)
    {
        case PluginSlotKind::midiFx:     return "MIDI";
        case PluginSlotKind::instrument: return "INST";
        case PluginSlotKind::effectA:    return "FX1";
        case PluginSlotKind::effectB:    return "FX2";
        case PluginSlotKind::effectC:    return "FX3";
        case PluginSlotKind::effectD:    return "FX4";
    }

    return {};
}

MainComponent::GlyphType MainComponent::toolIdToGlyph (const juce::String& id)
{
    if (id == "gate")    return GlyphType::pulse;
    if (id == "note")    return GlyphType::tone;
    if (id == "lead")    return GlyphType::voice;
    if (id == "chord")   return GlyphType::chord;
    if (id == "key")     return GlyphType::key;
    if (id == "octave")  return GlyphType::octave;
    if (id == "route")   return GlyphType::route;
    if (id == "channel") return GlyphType::channel;
    if (id == "cc")      return GlyphType::cc;
    if (id == "param")   return GlyphType::parameter;
    if (id == "tempo")   return GlyphType::tempo;
    if (id == "ratchet") return GlyphType::ratchet;
    if (id == "repeat")  return GlyphType::repeat;
    if (id == "wormhole") return GlyphType::wormhole;
    if (id == "kick")    return GlyphType::kick;
    if (id == "snare")   return GlyphType::snare;
    if (id == "hat")     return GlyphType::hat;
    if (id == "clap")    return GlyphType::clap;
    if (id == "length")  return GlyphType::length;
    if (id == "accent")  return GlyphType::accent;
    if (id == "decay")   return GlyphType::decay;
    if (id == "air")     return GlyphType::noise;
    if (id == "level")   return GlyphType::mul;
    if (id == "shift")   return GlyphType::bias;
    if (id == "color")   return GlyphType::hue;
    if (id == "out")     return GlyphType::audio;
    if (id == "glow")    return GlyphType::visual;
    return GlyphType::empty;
}

juce::Colour MainComponent::hiddenBadgeColour (GlyphType type) noexcept
{
    switch (type)
    {
        case GlyphType::tone:
        case GlyphType::voice:
        case GlyphType::chord:
        case GlyphType::key:
        case GlyphType::octave:
        case GlyphType::route:
        case GlyphType::channel:
        case GlyphType::cc:
        case GlyphType::parameter:
            return juce::Colour (0xff4dd9ff);
        case GlyphType::tempo:
        case GlyphType::ratchet:
        case GlyphType::repeat:
        case GlyphType::wormhole:
        case GlyphType::kick:
        case GlyphType::snare:
        case GlyphType::hat:
        case GlyphType::clap:
        case GlyphType::length:
        case GlyphType::pulse:
            return juce::Colour (0xffff9f43);
        case GlyphType::accent:
        case GlyphType::decay:
        case GlyphType::noise:
        case GlyphType::mul:
        case GlyphType::bias:
        case GlyphType::audio:
            return juce::Colour (0xff7dff8a);
        case GlyphType::hue:
        case GlyphType::visual:
            return juce::Colour (0xffff78da);
        case GlyphType::empty:
            break;
    }

    return juce::Colours::white;
}

float MainComponent::clamp01 (float value) noexcept
{
    return juce::jlimit (0.0f, 1.0f, value);
}
