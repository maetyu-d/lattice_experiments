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

    if (showsNextStep)
    {
        auto nextBox = area.reduced (6.0f, 6.0f);
        g.setColour (snakeColour.withAlpha (0.12f));
        g.fillRoundedRectangle (nextBox, 9.0f);
        g.setColour (snakeColour.withAlpha (0.58f));
        g.drawRoundedRectangle (nextBox.reduced (0.5f), 9.0f, 1.2f);
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
    auto innerBounds = stageBounds.reduced (18.0f);
    auto sideLeft = innerBounds.removeFromLeft (58.0f);
    innerBounds.removeFromLeft (10.0f);
    auto sideRight = innerBounds.removeFromRight (92.0f);
    innerBounds.removeFromRight (10.0f);
    auto content = innerBounds;
    auto keyboardBounds = content.removeFromTop (34.0f).reduced (4.0f, 0.0f);
    content.removeFromTop (10.0f);
    content.removeFromBottom (58.0f);
    content.removeFromBottom (8.0f);
    auto noteArea = content.reduced (18.0f, 16.0f);
    auto latticeFrame = noteArea.reduced (34.0f, 28.0f);
    const int visibleCols = 7;
    const int visibleRows = 6;
    const int colStart = 0;
    const int rowStart = 0;
    const auto spacingX = latticeFrame.getWidth() / (float) (visibleCols + 2.25f);
    const auto spacingY = latticeFrame.getHeight() / (float) (visibleRows + 1.65f);
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
    const auto isHarmonySpaceMode = result.mode == MainComponent::AppMode::harmonySpace;
    juce::ColourGradient gradient (
        isHarmonicMode ? juce::Colour::fromFloatRGBA (0.20f, 0.16f, 0.42f, 1.0f)
                       : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.08f, 0.06f, 0.02f, 1.0f)
                                             : juce::Colour::fromHSV (juce::jlimit (0.0f, 1.0f, result.hue + 0.12f), 0.95f, 0.34f, 1.0f)),
        bounds.getTopLeft(),
        isHarmonicMode ? juce::Colour::fromFloatRGBA (0.82f, 0.38f, 0.24f, 1.0f)
                       : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.03f, 0.26f, 0.22f, 1.0f)
                                             : juce::Colour::fromHSV (std::fmod (result.hue + 0.42f, 1.0f), 1.0f, 0.18f, 1.0f)),
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

            auto innerBounds = stageBounds.reduced (18.0f);
            auto sideLeft = innerBounds.removeFromLeft (58.0f);
            innerBounds.removeFromLeft (10.0f);
            auto sideRight = innerBounds.removeFromRight (92.0f);
            innerBounds.removeFromRight (10.0f);
            auto content = innerBounds;
            auto keyboardBounds = content.removeFromTop (34.0f).reduced (4.0f, 0.0f);
            content.removeFromTop (10.0f);
            auto statusBounds = content.removeFromBottom (58.0f);
            content.removeFromBottom (8.0f);
            auto noteArea = content.reduced (18.0f, 16.0f);
            auto latticeFrame = noteArea.reduced (34.0f, 28.0f);

            g.setColour (juce::Colour::fromFloatRGBA (0.01f, 0.02f, 0.01f, 0.90f));
            g.fillRoundedRectangle (noteArea.expanded (12.0f, 10.0f), 18.0f);
            g.setColour (juce::Colour::fromFloatRGBA (1.0f, 0.90f, 0.28f, 0.16f));
            g.drawRoundedRectangle (noteArea.expanded (12.0f, 10.0f), 18.0f, 1.2f);

            const int visibleCols = 7;
            const int visibleRows = 6;
            const int colStart = 0;
            const int rowStart = 0;
            const auto spacingX = latticeFrame.getWidth() / (float) (visibleCols + 2.25f);
            const auto spacingY = latticeFrame.getHeight() / (float) (visibleRows + 1.65f);
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

            return;
        }

        auto stageBounds = bounds.reduced (20.0f, 22.0f);
        g.setColour (isHarmonicMode
                         ? juce::Colour::fromFloatRGBA (0.07f, 0.04f, 0.12f, 0.78f)
                         : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.03f, 0.03f, 0.01f, 0.82f)
                                               : juce::Colour::fromFloatRGBA (0.01f, 0.04f, 0.02f, 0.74f)));
        g.fillRoundedRectangle (stageBounds, 16.0f);
        juce::ColourGradient stageBloom (isHarmonicMode ? juce::Colour::fromFloatRGBA (0.90f, 0.70f, 0.28f, 0.16f)
                                                        : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (1.0f, 0.80f, 0.18f, 0.16f)
                                                                              : juce::Colour::fromFloatRGBA (0.65f, 1.0f, 0.05f, 0.14f)),
                                         stageBounds.getX(), stageBounds.getY(),
                                         isHarmonicMode ? juce::Colour::fromFloatRGBA (0.34f, 0.56f, 1.0f, 0.15f)
                                                        : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.10f, 0.92f, 0.88f, 0.14f)
                                                                              : juce::Colour::fromFloatRGBA (0.06f, 0.95f, 1.0f, 0.13f)),
                                         stageBounds.getRight(), stageBounds.getBottom(),
                                         false);
        g.setGradientFill (stageBloom);
        g.fillRoundedRectangle (stageBounds.reduced (1.0f), 16.0f);

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
    bpm = mode == AppMode::harmonicGeometry ? 112.0 : (mode == AppMode::harmonySpace ? 104.0 : 148.0);

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
        bpm = 92.0;

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

        selectedCell = getCell (0, 0, 1);
        return;
    }

    if (mode == AppMode::harmonySpace)
    {
        bpm = 76.0;
        harmonySpaceKeyCenter = 7;
        harmonySpaceConstraintMode = 1;
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

        selectedCell = getCell (0, 0, 1);
        return;
    }

    // Layer 1: foundation groove and bass motif
    stamp (0, 0, 0, GlyphType::pulse, "1");
    stamp (0, 0, 1, GlyphType::tone, "33");
    stamp (0, 0, 2, GlyphType::audio, "1");
    stamp (0, 0, 4, GlyphType::pulse, "1");
    stamp (0, 0, 5, GlyphType::tone, "36");
    stamp (0, 0, 6, GlyphType::audio, "0.95");
    stamp (0, 0, 8, GlyphType::pulse, "1");
    stamp (0, 0, 9, GlyphType::tone, "38");
    stamp (0, 0, 10, GlyphType::audio, "1");
    stamp (0, 0, 11, GlyphType::repeat, "1");
    stamp (0, 0, 12, GlyphType::pulse, "1");
    stamp (0, 0, 13, GlyphType::tone, "41");
    stamp (0, 0, 14, GlyphType::audio, "1");

    stamp (0, 1, 1, GlyphType::pulse, "1");
    stamp (0, 1, 2, GlyphType::voice, "57");
    stamp (0, 1, 3, GlyphType::audio, "0.84");
    stamp (0, 1, 5, GlyphType::pulse, "1");
    stamp (0, 1, 6, GlyphType::voice, "60");
    stamp (0, 1, 7, GlyphType::audio, "0.86");
    stamp (0, 1, 9, GlyphType::pulse, "1");
    stamp (0, 1, 10, GlyphType::voice, "64");
    stamp (0, 1, 11, GlyphType::audio, "0.88");
    stamp (0, 1, 12, GlyphType::ratchet, "2");
    stamp (0, 1, 13, GlyphType::pulse, "1");
    stamp (0, 1, 14, GlyphType::voice, "67");
    stamp (0, 1, 15, GlyphType::audio, "0.9");

    stamp (0, 2, 2, GlyphType::pulse, "1");
    stamp (0, 2, 3, GlyphType::voice, "69");
    stamp (0, 2, 4, GlyphType::audio, "0.68");
    stamp (0, 2, 6, GlyphType::pulse, "1");
    stamp (0, 2, 7, GlyphType::voice, "72");
    stamp (0, 2, 8, GlyphType::audio, "0.72");
    stamp (0, 2, 10, GlyphType::pulse, "1");
    stamp (0, 2, 11, GlyphType::voice, "74");
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
    stamp (0, 3, 11, GlyphType::voice, "91");
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
        resetAudioSnakes();

    auto spawnCount = pendingSpawnRequests.exchange (0, std::memory_order_acq_rel);

    while (spawnCount-- > 0)
        spawnAudioSnake();
}

void MainComponent::applyMode (AppMode mode, bool showTitleAfter)
{
    currentMode = mode;
    showingTitleScreen = showTitleAfter;
    isRunning.store (false);
    playButton.setButtonText ("Play");
    currentTimeSeconds.store (0.0);
    lastAdvancedTick.store (-1);
    previewSizeMode = PanelSizeMode::defaultSize;
    inspectorSizeMode = PanelSizeMode::defaultSize;
    visibleLayer = 0;
    initialiseGrid (mode);
    initialiseSnakes();
    resetSynthVoices();
    resetDrumVoices();
    publishPatchSnapshot();
    renderInspector();
    updateCellButtons();
    resized();
    repaint();
}

void MainComponent::createUi()
{
    addAndMakeVisible (sidebar);
    addAndMakeVisible (mainArea);
    addAndMakeVisible (previewArea);
    addAndMakeVisible (inspector);
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
    playButton.addListener (this);
    clearButton.addListener (this);
    spawnSnakeButton.addListener (this);
    saveButton.addListener (this);
    loadButton.addListener (this);
    layerDownButton.addListener (this);
    layerUpButton.addListener (this);
    sidebarToggleButton.addListener (this);
    glyphGridModeButton.addListener (this);
    harmonicGeometryModeButton.addListener (this);
    harmonySpaceModeButton.addListener (this);
    eraseButton.addListener (this);

    addAndMakeVisible (playButton);
    addAndMakeVisible (clearButton);
    addAndMakeVisible (spawnSnakeButton);
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (layerDownButton);
    addAndMakeVisible (layerUpButton);
    addAndMakeVisible (eraseButton);
    addAndMakeVisible (glyphGridModeButton);
    addAndMakeVisible (harmonicGeometryModeButton);
    addAndMakeVisible (harmonySpaceModeButton);

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
    addAndMakeVisible (labelEditor);
    addAndMakeVisible (codeEditor);

    const auto addTool = [this] (GlyphType type)
    {
        auto def = getGlyphDefinition (type);
        auto* button = toolButtons.add (new SidebarButton (def.shortName + " " + def.label, def.colour));
        button->setComponentID (glyphTypeToString (type));
        button->addListener (this);
        toolListContent.addAndMakeVisible (button);
    };

    for (auto type : { GlyphType::pulse, GlyphType::tone, GlyphType::voice, GlyphType::chord, GlyphType::key, GlyphType::octave,
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
}

void MainComponent::paint (juce::Graphics& g)
{
    const auto isHarmonicMode = currentMode == AppMode::harmonicGeometry;
    const auto isHarmonySpaceMode = currentMode == AppMode::harmonySpace;
    juce::ColourGradient gradient (
        isHarmonicMode ? juce::Colour::fromFloatRGBA (0.10f, 0.08f, 0.20f, 1.0f)
                       : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.08f, 0.06f, 0.02f, 1.0f)
                                             : juce::Colour::fromFloatRGBA (0.04f, 0.12f, 0.03f, 1.0f)),
        0.0f, 0.0f,
        isHarmonicMode ? juce::Colour::fromFloatRGBA (0.28f, 0.14f, 0.10f, 1.0f)
                       : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.05f, 0.18f, 0.18f, 1.0f)
                                             : juce::Colour::fromFloatRGBA (0.10f, 0.18f, 0.02f, 1.0f)),
        static_cast<float> (getWidth()), static_cast<float> (getHeight()),
        false);
    gradient.addColour (0.2, isHarmonicMode ? juce::Colour::fromFloatRGBA (0.36f, 0.48f, 1.0f, 0.24f)
                                            : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (1.0f, 0.78f, 0.18f, 0.20f)
                                                                  : juce::Colour::fromFloatRGBA (0.48f, 0.98f, 0.04f, 0.22f)));
    gradient.addColour (0.8, isHarmonicMode ? juce::Colour::fromFloatRGBA (1.0f, 0.62f, 0.24f, 0.20f)
                                            : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.12f, 0.92f, 0.88f, 0.18f)
                                                                  : juce::Colour::fromFloatRGBA (0.05f, 0.88f, 0.92f, 0.20f)));
    g.setGradientFill (gradient);
    g.fillAll();

    g.setColour (isHarmonicMode ? juce::Colour::fromFloatRGBA (0.07f, 0.06f, 0.12f, 0.97f)
                                : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.05f, 0.04f, 0.01f, 0.97f)
                                                      : makeSidebarColour()));
    g.fillRect (sidebar.getBounds());

    g.setColour (isHarmonicMode ? juce::Colour::fromFloatRGBA (0.11f, 0.08f, 0.16f, 0.94f)
                                : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.09f, 0.08f, 0.03f, 0.94f)
                                                      : makePanelColour()));
    g.fillRoundedRectangle (mainArea.getBounds().toFloat(), 18.0f);
    g.fillRoundedRectangle (previewArea.getBounds().toFloat(), 18.0f);
    g.fillRoundedRectangle (inspector.getBounds().toFloat(), 18.0f);

    auto footerBounds = getLocalBounds().removeFromBottom (56).toFloat().reduced (12.0f, 4.0f);
    juce::ColourGradient footerGlow (isHarmonicMode ? juce::Colour::fromFloatRGBA (0.42f, 0.62f, 1.0f, 0.18f)
                                                    : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (1.0f, 0.80f, 0.18f, 0.18f)
                                                                          : juce::Colour::fromFloatRGBA (0.62f, 1.0f, 0.08f, 0.18f)),
                                     footerBounds.getX(), footerBounds.getCentreY(),
                                     isHarmonicMode ? juce::Colour::fromFloatRGBA (1.0f, 0.62f, 0.28f, 0.16f)
                                                    : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.10f, 0.92f, 0.88f, 0.16f)
                                                                          : juce::Colour::fromFloatRGBA (0.10f, 0.96f, 1.0f, 0.16f)),
                                     footerBounds.getRight(), footerBounds.getCentreY(),
                                     false);
    footerGlow.addColour (0.5, isHarmonicMode ? juce::Colour::fromFloatRGBA (1.0f, 0.92f, 0.78f, 0.12f)
                                              : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (1.0f, 0.94f, 0.68f, 0.14f)
                                                                    : juce::Colour::fromFloatRGBA (1.0f, 0.22f, 0.82f, 0.14f)));
    g.setGradientFill (footerGlow);
    g.fillRoundedRectangle (footerBounds.expanded (4.0f, 1.0f), 18.0f);

    juce::ColourGradient footerFill (isHarmonicMode ? juce::Colour::fromFloatRGBA (0.11f, 0.09f, 0.17f, 0.94f)
                                                    : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.10f, 0.09f, 0.04f, 0.94f)
                                                                          : juce::Colour::fromFloatRGBA (0.08f, 0.12f, 0.05f, 0.92f)),
                                     footerBounds.getTopLeft(),
                                     isHarmonicMode ? juce::Colour::fromFloatRGBA (0.10f, 0.08f, 0.14f, 0.96f)
                                                    : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.04f, 0.08f, 0.08f, 0.94f)
                                                                          : juce::Colour::fromFloatRGBA (0.03f, 0.08f, 0.05f, 0.94f)),
                                     footerBounds.getBottomRight(),
                                     false);
    footerFill.addColour (0.35, isHarmonicMode ? juce::Colour::fromFloatRGBA (0.36f, 0.50f, 1.0f, 0.10f)
                                               : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (1.0f, 0.78f, 0.18f, 0.10f)
                                                                     : juce::Colour::fromFloatRGBA (0.48f, 0.98f, 0.04f, 0.08f)));
    footerFill.addColour (0.72, isHarmonicMode ? juce::Colour::fromFloatRGBA (1.0f, 0.54f, 0.24f, 0.10f)
                                               : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (0.10f, 0.92f, 0.88f, 0.08f)
                                                                     : juce::Colour::fromFloatRGBA (0.06f, 0.92f, 1.0f, 0.08f)));
    g.setGradientFill (footerFill);
    g.fillRoundedRectangle (footerBounds, 16.0f);

    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawRoundedRectangle (footerBounds.reduced (0.5f), 16.0f, 1.0f);
    g.setColour (isHarmonicMode ? juce::Colour::fromFloatRGBA (1.0f, 0.86f, 0.72f, 0.18f)
                                : (isHarmonySpaceMode ? juce::Colour::fromFloatRGBA (1.0f, 0.84f, 0.24f, 0.20f)
                                                      : juce::Colour::fromFloatRGBA (0.72f, 1.0f, 0.18f, 0.20f)));
    g.drawLine (footerBounds.getX() + 16.0f, footerBounds.getY() + 5.0f, footerBounds.getRight() - 16.0f, footerBounds.getY() + 5.0f, 1.4f);

    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawLine ((float) sidebar.getRight(), 0.0f, (float) sidebar.getRight(), (float) getHeight(), 1.0f);

    if (showingTitleScreen)
    {
        g.setColour (currentMode == AppMode::harmonicGeometry
                         ? juce::Colour::fromFloatRGBA (0.05f, 0.04f, 0.10f, 1.0f)
                         : (currentMode == AppMode::harmonySpace ? juce::Colour::fromFloatRGBA (0.04f, 0.03f, 0.01f, 1.0f)
                                                                 : juce::Colour::fromFloatRGBA (0.01f, 0.03f, 0.02f, 1.0f)));
        g.fillAll();

        auto overlay = getLocalBounds().reduced (120, 90).toFloat();
        juce::ColourGradient titleGlow (juce::Colour::fromFloatRGBA (0.62f, 1.0f, 0.08f, 0.18f),
                                        overlay.getTopLeft(),
                                        juce::Colour::fromFloatRGBA (0.12f, 0.92f, 1.0f, 0.16f),
                                        overlay.getBottomRight(),
                                        false);
        titleGlow.addColour (0.55, juce::Colour::fromFloatRGBA (1.0f, 0.28f, 0.84f, 0.14f));
        g.setGradientFill (titleGlow);
        g.fillRoundedRectangle (overlay.expanded (8.0f), 30.0f);

        g.setColour (juce::Colour::fromFloatRGBA (0.04f, 0.08f, 0.04f, 0.94f));
        g.fillRoundedRectangle (overlay, 28.0f);
        g.setColour (juce::Colours::white.withAlpha (0.16f));
        g.drawRoundedRectangle (overlay.reduced (0.5f), 28.0f, 1.2f);

        auto titleArea = overlay.reduced (42.0f, 34.0f).toNearestInt();
        g.setColour (juce::Colours::white);
        g.setFont (juce::FontOptions (44.0f, juce::Font::bold));
        g.drawText ("Patchable Harmonic Worlds", titleArea.removeFromTop (58), juce::Justification::centred, false);
        g.setColour (juce::Colours::white.withAlpha (0.76f));
        g.setFont (juce::FontOptions (18.0f, juce::Font::plain));
        g.drawText ("Choose a mode to enter the instrument", titleArea.removeFromTop (28), juce::Justification::centred, false);
    }
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
        stage.setVisible (false);
        toolViewport.setVisible (false);
        sidebarToggleButton.setVisible (false);

        for (auto* button : toolButtons)
            button->setVisible (false);

        for (auto* label : { &bpmValue, &beatValue, &toolValue, &audioValue })
            label->setVisible (false);

        for (auto* component : { static_cast<juce::Component*> (&playButton),
                                 static_cast<juce::Component*> (&clearButton),
                                 static_cast<juce::Component*> (&spawnSnakeButton),
                                 static_cast<juce::Component*> (&layerDownButton),
                                 static_cast<juce::Component*> (&layerUpButton),
                                 static_cast<juce::Component*> (&saveButton),
                                 static_cast<juce::Component*> (&loadButton),
                                 static_cast<juce::Component*> (&labelEditor),
                                 static_cast<juce::Component*> (&codeEditor),
                                 static_cast<juce::Component*> (&eraseButton) })
            component->setVisible (false);

        auto titleArea = getLocalBounds().reduced (240, 230);
        auto row = titleArea.removeFromBottom (110);
        glyphGridModeButton.setVisible (true);
        harmonicGeometryModeButton.setVisible (true);
        harmonySpaceModeButton.setVisible (true);
        auto third = row.getWidth() / 3;
        glyphGridModeButton.setBounds (row.removeFromLeft (third).reduced (16, 12));
        harmonicGeometryModeButton.setBounds (row.removeFromLeft (third).reduced (16, 12));
        harmonySpaceModeButton.setBounds (row.reduced (16, 12));
        return;
    }

    sidebar.setVisible (true);
    mainArea.setVisible (true);
    sidebarToggleButton.setVisible (true);
    for (auto* label : { &bpmValue, &beatValue, &toolValue, &audioValue })
        label->setVisible (true);
    for (auto* component : { static_cast<juce::Component*> (&playButton),
                             static_cast<juce::Component*> (&clearButton),
                             static_cast<juce::Component*> (&spawnSnakeButton),
                             static_cast<juce::Component*> (&layerDownButton),
                             static_cast<juce::Component*> (&layerUpButton),
                             static_cast<juce::Component*> (&saveButton),
                             static_cast<juce::Component*> (&loadButton) })
        component->setVisible (true);
    glyphGridModeButton.setVisible (false);
    harmonicGeometryModeButton.setVisible (false);
    harmonySpaceModeButton.setVisible (false);

    auto footer = area.removeFromBottom (56);
    const auto footerButtonBounds = [] (juce::Rectangle<int> area, int xPad) { return area.reduced (xPad, 10).translated (0, 2); };
    playButton.setBounds (footerButtonBounds (footer.removeFromLeft (120), 16));
    clearButton.setBounds (footerButtonBounds (footer.removeFromLeft (140), 6));
    spawnSnakeButton.setBounds (footerButtonBounds (footer.removeFromLeft (150), 6));
    auto layerControls = footer.removeFromLeft (470);
    layerDownButton.setBounds (footerButtonBounds (layerControls.removeFromLeft (96), 6));
    layerUpButton.setBounds (footerButtonBounds (layerControls.removeFromLeft (96), 6));
    saveButton.setBounds (footerButtonBounds (layerControls.removeFromLeft (92), 6));
    loadButton.setBounds (footerButtonBounds (layerControls.removeFromLeft (92), 6));
    bpmValue.setBounds (footer.removeFromLeft (110));
    beatValue.setBounds ({});
    toolValue.setBounds ({});
    audioValue.setBounds (footer.removeFromLeft (160));

    const auto defaultRightWidth = 390;
    const auto halfWidth = juce::roundToInt (getWidth() * 0.5f);
    const auto previewWidth = isHarmonySpaceMode ? 0 : (previewSizeMode == PanelSizeMode::hidden ? 0 : (previewSizeMode == PanelSizeMode::halfScreen ? halfWidth : defaultRightWidth));
    const auto inspectorWidth = isHarmonySpaceMode ? 0 : (inspectorSizeMode == PanelSizeMode::hidden ? 0 : (inspectorSizeMode == PanelSizeMode::halfScreen ? halfWidth : defaultRightWidth));
    const auto combinedRightWidth = juce::jlimit (0, area.getWidth() - 180, juce::jmax (previewWidth, inspectorWidth));
    auto rightArea = area.removeFromRight (combinedRightWidth);

    const auto previewOnlyFocus = previewWidth > 0 && inspectorWidth == 0 && previewSizeMode == PanelSizeMode::halfScreen;
    auto previewBounds = previewWidth > 0
        ? (previewOnlyFocus ? rightArea.reduced (16) : rightArea.removeFromTop (406).reduced (16))
        : juce::Rectangle<int>();
    auto inspectorBounds = inspectorWidth > 0 ? rightArea.reduced (16) : juce::Rectangle<int>();
    previewArea.setVisible (previewWidth > 0);
    inspector.setVisible (inspectorWidth > 0);
    previewArea.setBounds (previewBounds);
    inspector.setBounds (inspectorBounds);
    for (auto* component : { static_cast<juce::Component*> (&labelEditor),
                             static_cast<juce::Component*> (&codeEditor),
                             static_cast<juce::Component*> (&eraseButton) })
        component->setVisible (inspectorWidth > 0);

    mainArea.setBounds (area.reduced (16));

    auto gridArea = mainArea.getBounds().reduced (14);
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

    stage.setVisible (isHarmonySpaceMode || previewWidth > 0);
    if (isHarmonySpaceMode)
    {
        auto stageBounds = mainArea.getBounds().reduced (18);
        stage.setBounds (stageBounds);
    }
    else if (previewWidth > 0)
    {
        auto stageBounds = previewArea.getLocalBounds().reduced (currentMode == AppMode::harmonicGeometry ? 6 : (currentMode == AppMode::harmonySpace ? 8 : 14));

        const auto squareSize = juce::jmin (stageBounds.getWidth(), stageBounds.getHeight());
        stageBounds = juce::Rectangle<int> (squareSize, squareSize).withCentre (previewArea.getBounds().getCentre());

        stage.setBounds (stageBounds);
    }

    if (inspectorWidth > 0)
        layoutInspector (inspector.getBounds().reduced (16));

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
    glyphValueLabel.setBounds (area.removeFromTop (28));
    labelEditor.setBounds (area.removeFromTop (40));
    area.removeFromTop (8);
    codeEditor.setBounds (area.removeFromTop (260));
    area.removeFromTop (8);
    eraseButton.setBounds (area.removeFromTop (34));
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
}

void MainComponent::getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill)
{
    auto* left = bufferToFill.buffer->getWritePointer (0, bufferToFill.startSample);
    auto* right = bufferToFill.buffer->getNumChannels() > 1
                    ? bufferToFill.buffer->getWritePointer (1, bufferToFill.startSample)
                    : nullptr;
    auto patchSnapshot = getPatchSnapshot();
    auto timeSeconds = currentTimeSeconds.load();
    const auto running = isRunning.load();
    float transportPhase = 0.0f;

    applyPendingTransportCommands();

    for (int sample = 0; sample < bufferToFill.numSamples; ++sample)
    {
        float value = 0.0f;

        if (running && patchSnapshot != nullptr)
        {
            const auto bpmSnapshot = patchSnapshot->bpm;
            const auto sequencerTime = timeSeconds * (bpmSnapshot / 60.0) * 4.0;
            const auto targetTick = (int) std::floor (sequencerTime);
            if (targetTick != lastAdvancedTick.load())
            {
                applyPendingTransportCommands();
                advanceSnakesToTick (targetTick, *patchSnapshot);
                processAudioTick (*patchSnapshot, timeSeconds);
                lastAdvancedTick.store (targetTick);
            }

            const auto rawValue = evaluateAudioForSnapshot (*patchSnapshot, timeSeconds);
            outputFastState += (rawValue - outputFastState) * outputFastCoeff;
            outputSlowState += (outputFastState - outputSlowState) * outputSlowCoeff;
            const auto hp = outputHpCoeff * (outputHpState + outputSlowState - outputHpInputState);
            outputHpInputState = outputSlowState;
            outputHpState = hp;
            value = std::tanh (hp * 1.0f) * 0.62f;
            transportPhase = (float) (sequencerTime - std::floor (sequencerTime));
            timeSeconds += 1.0 / currentSampleRate;
        }
        else
        {
            outputFastState *= 0.995f;
            outputSlowState *= 0.995f;
            outputHpState *= 0.995f;
            outputHpInputState *= 0.995f;
            value = 0.0f;
        }

        left[sample] = value;
        if (right != nullptr)
            right[sample] = value;
    }

    if (running)
        currentTimeSeconds.store (timeSeconds);

    transportSequence.fetch_add (1, std::memory_order_acq_rel);
    publishedTransportSnapshot.snakeCount = audioSnakeCount;
    publishedTransportSnapshot.transportPhase = transportPhase;
    for (int i = 0; i < maxSnakes; ++i)
        publishedTransportSnapshot.snakes[(size_t) i] = audioSnakes[(size_t) i];
    transportSequence.fetch_add (1, std::memory_order_release);
}

void MainComponent::releaseResources() {}

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

    if (button == &glyphGridModeButton)
    {
        applyMode (AppMode::glyphGrid);
        return;
    }

    if (button == &harmonicGeometryModeButton)
    {
        applyMode (AppMode::harmonicGeometry);
        return;
    }

    if (button == &harmonySpaceModeButton)
    {
        applyMode (AppMode::harmonySpace);
        return;
    }

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
        spawnSnake();
        updateCellButtons();
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
        return;
    }

    inspectorTitle.setText (selectedCell->label + " L" + juce::String (selectedCell->layer + 1) + " "
                                + juce::String (selectedCell->row + 1) + ":" + juce::String (selectedCell->col + 1),
                            juce::dontSendNotification);
    glyphValueLabel.setText ("Glyph " + glyphTypeToString (selectedCell->type), juce::dontSendNotification);
    labelEditor.setText (selectedCell->label, juce::dontSendNotification);
    codeEditor.setText (selectedCell->code, juce::dontSendNotification);
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
        button->hasAnyLayerContent = false;
        button->hasHiddenLayerContent = false;
        button->isActiveColumn = false;
        button->isSnakeHead = false;
        button->isGhostSnake = false;
        button->showsNextStep = false;
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

    if (transportSnapshot.snakeCount > 0)
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

    auto result = evaluateScoreForGrid (gridSnapshot, snakeSnapshot, patchSnapshot->bpm, timeSeconds);
    result.transportPhase = transportSnapshot.transportPhase;
    result.activeLayer = visibleLayer;
    result.mode = currentMode;
    result.harmonySpaceKeyCenter = harmonySpaceKeyCenter;
    result.harmonySpaceConstraintMode = harmonySpaceConstraintMode;
    result.harmonySpaceGestureRecordEnabled = harmonySpaceGestureRecordEnabled;
    result.harmonySpaceGesturePoints = harmonySpaceGesturePoints;
    return result;
}

MainComponent::ScoreResult MainComponent::evaluateScoreForGrid (const juce::Array<Cell>& sourceGrid,
                                                                const juce::Array<Snake>& snakeSnapshot,
                                                                double bpmValue,
                                                                double timeSeconds) const noexcept
{
    ScoreResult result;
    const auto beat = timeSeconds * (bpmValue / 60.0);
    const auto stepTime = timeSeconds * (bpmValue / 60.0) * 4.0;
    const auto stepPhase = static_cast<float> (stepTime - std::floor (stepTime));
    const auto gateEnvelope = std::sin (juce::MathConstants<float>::pi * juce::jlimit (0.0f, 1.0f, stepPhase));
    const auto triggeredCells = collectTriggeredCells (snakeSnapshot);
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

void MainComponent::triggerSynthVoice (float midiNote, float amplitude, float noiseMix, float brightness, float waveformMix, float decayScale, int delaySamples) noexcept
{
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

void MainComponent::triggerDrumVoice (DrumType type, float amplitude, float tone, float decayScale, int delaySamples) noexcept
{
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
                            localAmplitude *= juce::jlimit (0.35, harmonySpaceMode ? 1.8 : 2.8, std::abs (value));
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
                                                triggerSynthVoice ((float) (basePitch + interval),
                                                                   voiceAmp,
                                                                   (float) localNoiseMix,
                                                                   voiceBrightness,
                                                                   voiceWaveform,
                                                                   voiceDecay,
                                                                   repeatDelay + noteIndex * juce::jmax (0, samplesPerTick / 18));
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
                                                triggerSynthVoice ((float) (basePitch + interval),
                                                                   voiceAmp,
                                                                   (float) localNoiseMix,
                                                                   voiceBrightness,
                                                                   voiceWaveform,
                                                                   voiceDecay,
                                                                   repeatDelay + noteIndex * juce::jmax (0, samplesPerTick / 18));
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
                                                              repeatDelay);
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
                            localAccent *= juce::jlimit (0.35, 2.4, std::abs (value));
                        break;
                    case GlyphType::decay:
                        if (chainReady)
                            localDecay *= juce::jlimit (0.35, 2.2, std::abs (value));
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
                                    triggerDrumVoice (drumType, repeatAmp, (float) juce::jlimit (0.0, 1.0, std::abs (value) * 0.25 + row / 8.0), (float) localDecay, repeatDelay);
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
        resetAudioSnakes();
        transportSequence.fetch_add (1, std::memory_order_acq_rel);
        publishedTransportSnapshot.snakeCount = audioSnakeCount;
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
    visibleLayer = juce::jlimit (0, layers - 1, (int) getPropertyOr ("visibleLayer", 0));
    selectedTool = toolIdToGlyph (getPropertyOr ("selectedTool", glyphTypeToString (GlyphType::tone)).toString());
    previewSizeMode = panelModeFromString (getPropertyOr ("previewMode", "default").toString());
    inspectorSizeMode = panelModeFromString (getPropertyOr ("inspectorMode", "default").toString());
    usePseudoDepthStageView = true;
    showingTitleScreen = false;
    isSidebarExpanded = (bool) getPropertyOr ("sidebarExpanded", false);
    harmonySpaceKeyCenter = juce::jlimit (0, 11, (int) getPropertyOr ("harmonySpaceKeyCenter", 0));
    harmonySpaceConstraintMode = juce::jlimit (0, 2, (int) getPropertyOr ("harmonySpaceConstraintMode", 0));
    harmonySpaceGestureRecordEnabled = (bool) getPropertyOr ("harmonySpaceGestureRecordEnabled", false);
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
    bpmValue.setText ("BPM " + juce::String ((int) std::round (bpm)), juce::dontSendNotification);

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

    initialiseSnakes();
    lastAdvancedTick.store (-1);
    resetSynthVoices();
    resetDrumVoices();
    publishPatchSnapshot();
    renderInspector();
    updateCellButtons();
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
    if (mode == AppMode::harmonicGeometry)
        return "harmonicGeometry";
    if (mode == AppMode::harmonySpace)
        return "harmonySpace";
    return "glyphGrid";
}

MainComponent::AppMode MainComponent::stringToMode (const juce::String& text)
{
    if (text == "harmonicGeometry")
        return AppMode::harmonicGeometry;
    if (text == "harmonySpace")
        return AppMode::harmonySpace;
    return AppMode::glyphGrid;
}

MainComponent::GlyphType MainComponent::toolIdToGlyph (const juce::String& id)
{
    if (id == "gate")    return GlyphType::pulse;
    if (id == "note")    return GlyphType::tone;
    if (id == "lead")    return GlyphType::voice;
    if (id == "chord")   return GlyphType::chord;
    if (id == "key")     return GlyphType::key;
    if (id == "octave")  return GlyphType::octave;
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
