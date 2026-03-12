#pragma once

#include <JuceHeader.h>
#include <atomic>
#include <array>
#include <memory>
#include <vector>
#include "ExpressionEvaluator.h"

class MainComponent final : public juce::AudioAppComponent,
                            private juce::Timer,
                            private juce::Button::Listener,
                            private juce::TextEditor::Listener,
                            private juce::KeyListener
{
public:
    enum class ModeVariant
    {
        a,
        b
    };

    enum class PanelSizeMode
    {
        defaultSize,
        halfScreen,
        hidden
    };

    enum class AppMode
    {
        glyphGrid,
        cellularGrid,
        harmonicGeometry,
        harmonySpace
    };

    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress& key) override;
    void parentHierarchyChanged() override;
    bool toggleStageView();

    void prepareToPlay (int samplesPerBlockExpected, double sampleRate) override;
    void getNextAudioBlock (const juce::AudioSourceChannelInfo& bufferToFill) override;
    void releaseResources() override;

private:
    enum class GlyphType
    {
        empty,
        pulse,
        tone,
        voice,
        chord,
        key,
        octave,
        tempo,
        ratchet,
        repeat,
        wormhole,
        kick,
        snare,
        hat,
        clap,
        length,
        accent,
        decay,
        noise,
        mul,
        bias,
        hue,
        audio,
        visual
    };

    struct GlyphDefinition
    {
        juce::String label;
        juce::String shortName;
        juce::Colour colour;
        juce::String defaultCode;
    };

    struct Cell
    {
        int layer = 0;
        int row = 0;
        int col = 0;
        GlyphType type = GlyphType::empty;
        juce::String label;
        juce::String code;
        std::shared_ptr<const ExpressionEvaluator::Program> program;
    };

    struct VisualLine
    {
        int layer = 0;
        int row = 0;
        int step = 0;
        float hue = 0.58f;
        float energy = 0.0f;
    };

    struct GridCellVisual
    {
        int layer = 0;
        int row = 0;
        int col = 0;
        GlyphType type = GlyphType::empty;
        juce::String code;
        bool isTriggered = false;
    };

    struct SnakeSegment
    {
        int layer = 0;
        int row = 0;
        int col = 0;
    };

    struct Snake
    {
        juce::Array<SnakeSegment> segments;
        juce::Array<SnakeSegment> previousSegments;
        int directionX = 1;
        int directionY = 0;
        int directionLayer = 0;
        int lengthDelta = 0;
        float lengthPulse = 0.0f;
        juce::Colour colour = juce::Colours::white;
        bool isGhost = false;
    };

    static constexpr int cols = 16;
    static constexpr int rows = 8;
    static constexpr int layers = 8;
    static constexpr int maxSnakes = 8;
    static constexpr int snakeLength = 4;
    static constexpr int maxSynthVoices = 48;
    static constexpr int maxDrumVoices = 24;

    struct SnakeRuntime
    {
        std::array<SnakeSegment, snakeLength> segments {};
        std::array<SnakeSegment, snakeLength> previousSegments {};
        int length = 0;
        int previousLength = 0;
        int directionX = 1;
        int directionY = 0;
        int directionLayer = 0;
        int ticksOnCurrentLayer = 0;
        int lengthDelta = 0;
        int recentLengthChangeTicks = 0;
        juce::Colour colour = juce::Colours::white;
        bool active = false;
    };

    struct PatchSnapshot
    {
        std::array<Cell, cols * rows * layers> cells {};
        double bpm = 92.0;
    };

    struct SynthVoice
    {
        bool active = false;
        float frequency = 220.0f;
        float amplitude = 0.0f;
        float phaseA = 0.0f;
        float phaseB = 0.0f;
        float env = 0.0f;
        float attack = 1.0f;
        float attackIncrement = 0.0f;
        float decayPerSample = 0.9995f;
        float noiseMix = 0.0f;
        float brightness = 0.4f;
        float filterState = 0.0f;
        float filterStateB = 0.0f;
        float filterCoeff = 0.1f;
        float detuneRatio = 1.0f;
        float waveformMix = 0.5f;
        int delaySamples = 0;
    };

    enum class DrumType
    {
        kick,
        snare,
        hat,
        clap
    };

    struct DrumVoice
    {
        bool active = false;
        DrumType type = DrumType::kick;
        float amplitude = 0.0f;
        float env = 0.0f;
        float phase = 0.0f;
        float phaseB = 0.0f;
        float frequency = 60.0f;
        float sweep = 0.0f;
        float decayPerSample = 0.999f;
        float tone = 0.5f;
        float noiseState = 0.0f;
        int burstStage = 0;
        int burstCounter = 0;
        int delaySamples = 0;
    };

    struct TransportSnapshot
    {
        std::array<SnakeRuntime, maxSnakes> snakes {};
        int snakeCount = 0;
        bool automataMode = false;
        std::array<uint8_t, cols * rows * layers> automataCurrent {};
        std::array<uint8_t, cols * rows * layers> automataPrevious {};
        float transportPhase = 0.0f;
    };

    struct ScoreResult
    {
        float audio = 0.0f;
        float hue = 0.58f;
        float energy = 0.0f;
        float transportPhase = 0.0f;
        int activeLayer = 0;
        int harmonySpaceKeyCenter = 0;
        int harmonySpaceConstraintMode = 0;
        bool harmonySpaceGestureRecordEnabled = false;
        AppMode mode = AppMode::glyphGrid;
        juce::Array<GridCellVisual> gridCells;
        juce::Array<VisualLine> lines;
        juce::Array<Snake> snakes;
        juce::Array<juce::Point<int>> harmonySpaceGesturePoints;
    };

    enum class PluginSlotKind
    {
        midiFx,
        instrument,
        effectA,
        effectB,
        effectC,
        effectD
    };

    struct ScheduledMidiEvent
    {
        int sampleOffset = 0;
        juce::MidiMessage message;
    };

    struct HostedPluginSlot
    {
        std::unique_ptr<juce::AudioPluginInstance> instance;
        juce::String buttonText { "Empty" };
        juce::String pluginPath;
    };

    struct MixerStripState
    {
        juce::String name;
        HostedPluginSlot midiFx;
        HostedPluginSlot instrument;
        HostedPluginSlot effectA;
        HostedPluginSlot effectB;
        HostedPluginSlot effectC;
        HostedPluginSlot effectD;
        float volume = 0.80f;
        float pan = 0.0f;
        juce::AudioBuffer<float> audioBuffer;
        juce::AudioBuffer<float> midiFxScratchBuffer;
        juce::MidiBuffer midiBuffer;
        std::vector<ScheduledMidiEvent> pendingMidi;
    };

    class SidebarButton final : public juce::TextButton
    {
    public:
        SidebarButton (juce::String text, juce::Colour accent)
            : juce::TextButton (std::move (text)), accentColour (accent)
        {
        }

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto bounds = getLocalBounds().toFloat();
            auto fill = juce::Colour::fromFloatRGBA (0.12f, 0.22f, 0.05f, 0.92f)
                            .interpolatedWith (accentColour.withAlpha (0.35f), down ? 0.7f : (over ? 0.5f : 0.28f));
            juce::ColourGradient glow (accentColour.withAlpha (down ? 0.34f : 0.24f),
                                       bounds.getCentreX(), bounds.getY(),
                                       juce::Colour::fromFloatRGBA (0.03f, 0.06f, 0.02f, 0.0f),
                                       bounds.getCentreX(), bounds.getBottom(),
                                       false);
            g.setGradientFill (glow);
            g.fillRoundedRectangle (bounds.expanded (1.5f), 14.0f);
            g.setColour (fill);
            g.fillRoundedRectangle (bounds, 12.0f);
            juce::ColourGradient inner (accentColour.withAlpha (over ? 0.2f : 0.12f),
                                        bounds.getTopLeft(),
                                        juce::Colour::fromFloatRGBA (0.02f, 0.05f, 0.02f, 0.0f),
                                        bounds.getBottomRight(),
                                        false);
            g.setGradientFill (inner);
            g.fillRoundedRectangle (bounds.reduced (1.0f), 11.0f);
            g.setColour (juce::Colours::white.withAlpha (over ? 0.35f : 0.18f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 12.0f, 1.0f);
            g.setColour (accentColour.withAlpha (over ? 0.88f : 0.56f));
            g.drawRoundedRectangle (bounds.reduced (1.4f), 11.0f, over ? 1.3f : 1.0f);
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
            g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred, true);
        }

    private:
        juce::Colour accentColour;
    };

    class FooterButton final : public juce::TextButton
    {
    public:
        FooterButton (juce::String text, juce::Colour accent)
            : juce::TextButton (std::move (text)), accentColour (accent)
        {
        }

        void paintButton (juce::Graphics& g, bool over, bool down) override
        {
            auto bounds = getLocalBounds().toFloat();
            const auto glowAlpha = down ? 0.34f : (over ? 0.24f : 0.16f);
            const auto fillMix = down ? 0.62f : (over ? 0.48f : 0.32f);

            juce::ColourGradient outerGlow (accentColour.withAlpha (glowAlpha),
                                            bounds.getCentreX(), bounds.getY(),
                                            juce::Colour::fromFloatRGBA (0.02f, 0.04f, 0.02f, 0.0f),
                                            bounds.getCentreX(), bounds.getBottom(),
                                            false);
            g.setGradientFill (outerGlow);
            g.fillRoundedRectangle (bounds.expanded (2.5f, 2.0f), 14.0f);

            auto fill = juce::Colour::fromFloatRGBA (0.10f, 0.14f, 0.09f, 0.94f)
                            .interpolatedWith (accentColour.withAlpha (0.50f), fillMix);
            g.setColour (fill);
            g.fillRoundedRectangle (bounds, 11.0f);

            juce::ColourGradient innerSheen (juce::Colours::white.withAlpha (over ? 0.18f : 0.10f),
                                             bounds.getTopLeft(),
                                             juce::Colour::fromFloatRGBA (0.03f, 0.06f, 0.03f, 0.0f),
                                             bounds.getBottomLeft().translated (0.0f, -2.0f),
                                             false);
            g.setGradientFill (innerSheen);
            g.fillRoundedRectangle (bounds.reduced (1.0f), 10.0f);

            g.setColour (juce::Colours::white.withAlpha (0.14f));
            g.drawRoundedRectangle (bounds.reduced (0.5f), 11.0f, 1.0f);
            g.setColour (accentColour.withAlpha (down ? 0.98f : (over ? 0.82f : 0.62f)));
            g.drawRoundedRectangle (bounds.reduced (1.4f), 10.0f, down ? 1.8f : 1.2f);

            g.setColour (accentColour.withAlpha (0.22f));
            g.drawLine (bounds.getX() + 10.0f, bounds.getBottom() - 3.0f, bounds.getRight() - 10.0f, bounds.getBottom() - 3.0f, 2.0f);

            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (14.0f, juce::Font::bold));
            g.drawText (getButtonText(), getLocalBounds(), juce::Justification::centred, true);
        }

    private:
        juce::Colour accentColour;
    };

    class CellButton final : public juce::Button
    {
    public:
        CellButton (int cellRow, int cellCol) : juce::Button ({ }), row (cellRow), col (cellCol) {}

        int row = 0;
        int col = 0;
        juce::String code;
        juce::StringArray hiddenLayerBadges;
        juce::Array<juce::Colour> hiddenLayerBadgeColours;
        GlyphType type = GlyphType::empty;
        bool hasAnyLayerContent = false;
        bool hasHiddenLayerContent = false;
        bool isSelected = false;
        bool isActiveColumn = false;
        bool isSnakeHead = false;
        bool isGhostSnake = false;
        bool showsNextStep = false;
        bool isAutomataActive = false;
        bool isAutomataNewborn = false;
        int snakeDirectionX = 0;
        int snakeDirectionY = 0;
        int snakeDirectionLayer = 0;
        juce::Colour snakeColour = juce::Colours::transparentBlack;

        void paintButton (juce::Graphics& g, bool over, bool) override;
    };

    class StageComponent final : public juce::Component
    {
    public:
        void setState (ScoreResult resultIn);
        void setPseudoDepthEnabled (bool shouldUsePseudoDepth);
        void setHarmonySpaceCallbacks (std::function<void (int, int)> noteCallbackIn,
                                       std::function<void (int)> layerCallbackIn,
                                       std::function<void (int)> controlCallbackIn);
        void paint (juce::Graphics&) override;
        void mouseDown (const juce::MouseEvent&) override;

    private:
        struct TrailFrame
        {
            juce::Array<Snake> snakes;
            float alpha = 0.0f;
        };

        ScoreResult result;
        bool usePseudoDepthView = false;
        juce::Array<TrailFrame> trailFrames;
        std::function<void (int, int)> harmonySpaceNoteCallback;
        std::function<void (int)> harmonySpaceLayerCallback;
        std::function<void (int)> harmonySpaceControlCallback;
    };

    void timerCallback() override;
    void buttonClicked (juce::Button*) override;
    bool keyPressed (const juce::KeyPress& key, juce::Component* originatingComponent) override;
    void textEditorTextChanged (juce::TextEditor&) override;
    void textEditorFocusLost (juce::TextEditor&) override;
    void textEditorReturnKeyPressed (juce::TextEditor&) override;

    void initialiseGrid();
    void initialiseGrid (AppMode mode);
    void applyMode (AppMode mode, ModeVariant variant, bool showTitleAfter = false);
    void createUi();
    void layoutSidebar (juce::Rectangle<int>);
    void layoutMainArea (juce::Rectangle<int>);
    void layoutInspector (juce::Rectangle<int>);
    void layoutMixer (juce::Rectangle<int>);

    void renderInspector();
    void updateCellButtons();
    void selectCell (Cell*);
    void applySelectedToolToCell (Cell&);
    void recompileCell (Cell&);
    bool savePatchToFile (const juce::File& file);
    bool loadPatchFromFile (const juce::File& file);
    Cell* getCell (int layer, int row, int col) noexcept;
    const Cell* getCell (int layer, int row, int col) const noexcept;
    int getCellIndex (int layer, int row, int col) const noexcept;
    void publishPatchSnapshot();
    std::shared_ptr<const PatchSnapshot> getPatchSnapshot() const noexcept;
    TransportSnapshot getTransportSnapshot() const noexcept;
    void applyPendingTransportCommands() noexcept;
    ScoreResult evaluateScore (double timeSeconds) const noexcept;
    float evaluateAudioSample (double timeSeconds) noexcept;
    float evaluateAudioForSnapshot (const PatchSnapshot& snapshot, double timeSeconds) noexcept;
    ScoreResult evaluateScoreForGrid (const juce::Array<Cell>& sourceGrid,
                                      const juce::Array<Snake>& snakeSnapshot,
                                      const juce::Array<SnakeSegment>* explicitTriggeredCells,
                                      double bpmValue,
                                      double timeSeconds) const noexcept;
    void resetSynthVoices() noexcept;
    void resetDrumVoices() noexcept;
    void processAudioTick (const PatchSnapshot& snapshot, double timeSeconds) noexcept;
    void triggerSynthVoice (float midiNote, float amplitude, float noiseMix, float brightness, float waveformMix, float decayScale, int delaySamples = 0) noexcept;
    void triggerDrumVoice (DrumType type, float amplitude, float tone, float decayScale, int delaySamples = 0) noexcept;
    float renderSynthSample() noexcept;
    float renderDrumSample() noexcept;
    void initialisePluginHosting();
    void clearPluginProcessingState() noexcept;
    void preparePluginInstances (double sampleRate, int blockSize);
    void releasePluginInstances();
    bool loadPluginIntoSlot (int stripIndex, PluginSlotKind slotKind, const juce::File& file);
    HostedPluginSlot& getHostedPluginSlot (int stripIndex, PluginSlotKind slotKind) noexcept;
    const HostedPluginSlot& getHostedPluginSlot (int stripIndex, PluginSlotKind slotKind) const noexcept;
    void updateMixerButtons();
    void queuePluginNote (int preferredStrip, float midiNote, float amplitude, float decayScale, int delaySamples) noexcept;
    void queuePluginMessage (int stripIndex, const juce::MidiMessage& message, int absoluteSampleOffset) noexcept;
    void flushPendingPluginEventsForBlock (int blockSize) noexcept;
    void processPluginBlock (const juce::AudioSourceChannelInfo& bufferToFill,
                             const PatchSnapshot& snapshot,
                             double startTimeSeconds,
                             bool running);
    bool isPluginMode() const noexcept;
    ModeVariant getModeVariantForCurrentSession() const noexcept;
    static juce::String modeVariantToString (ModeVariant);
    static ModeVariant stringToModeVariant (const juce::String&);
    void initialiseSnakes();
    void spawnSnake();
    void advanceSnakesToTick (int targetTick, const PatchSnapshot& snapshot);
    void resetAudioSnakes() noexcept;
    void spawnAudioSnake() noexcept;
    void advanceSnake (SnakeRuntime&, const PatchSnapshot&) noexcept;
    void resetAutomata(const PatchSnapshot&) noexcept;
    void seedAutomataBurst() noexcept;
    void advanceAutomataToTick (int targetTick, const PatchSnapshot& snapshot) noexcept;
    void advanceAutomataGeneration (const PatchSnapshot& snapshot) noexcept;
    int countAutomataNeighbours (const std::array<uint8_t, cols * rows * layers>& state, int layer, int row, int col) const noexcept;
    bool isSnakeCellOccupied (const SnakeRuntime&, int layer, int row, int col, bool ignoreTail) const noexcept;
    static juce::Array<SnakeSegment> collectTriggeredCells (const juce::Array<Snake>& snakes);
    static GlyphDefinition getGlyphDefinition (GlyphType);
    static juce::String glyphTypeToString (GlyphType);
    static GlyphType toolIdToGlyph (const juce::String&);
    static juce::String modeToString (AppMode);
    static AppMode stringToMode (const juce::String&);
    static juce::Colour mixerSlotAccentColour (PluginSlotKind) noexcept;
    static juce::String mixerSlotPrefix (PluginSlotKind);
    static juce::Colour hiddenBadgeColour (GlyphType) noexcept;
    static float clamp01 (float value) noexcept;

    juce::OwnedArray<CellButton> cellButtons;
    juce::OwnedArray<SidebarButton> toolButtons;
    juce::OwnedArray<juce::Label> labels;

    juce::Component sidebar;
    juce::Viewport toolViewport;
    juce::Component toolListContent;
    juce::Component mainArea;
    juce::Component inspector;
    juce::Component previewArea;
    juce::Component mixerArea;
    StageComponent stage;
    FooterButton playButton { "Play", juce::Colour::fromFloatRGBA (0.66f, 1.0f, 0.10f, 1.0f) },
                 clearButton { "Clear Grid", juce::Colour::fromFloatRGBA (0.12f, 0.96f, 1.0f, 1.0f) },
                 spawnSnakeButton { "Spawn Snake", juce::Colour::fromFloatRGBA (1.0f, 0.32f, 0.88f, 1.0f) },
                 mixerToggleButton { "Mixer", juce::Colour::fromFloatRGBA (0.96f, 0.90f, 0.22f, 1.0f) },
                 saveButton { "Save", juce::Colour::fromFloatRGBA (1.0f, 0.68f, 0.16f, 1.0f) },
                 loadButton { "Load", juce::Colour::fromFloatRGBA (0.72f, 0.48f, 1.0f, 1.0f) },
                 layerDownButton { "Layer -", juce::Colour::fromFloatRGBA (0.22f, 1.0f, 0.68f, 1.0f) },
                 layerUpButton { "Layer +", juce::Colour::fromFloatRGBA (0.22f, 1.0f, 0.68f, 1.0f) };
    juce::TextButton sidebarToggleButton { "..." };
    FooterButton a1ModeButton { "A1", juce::Colour::fromFloatRGBA (0.12f, 0.96f, 1.0f, 1.0f) },
                 resumeModeButton { "Resume", juce::Colour::fromFloatRGBA (0.92f, 1.0f, 0.28f, 1.0f) },
                 a2ModeButton { "A2", juce::Colour::fromFloatRGBA (0.78f, 1.0f, 0.22f, 1.0f) },
                 a3ModeButton { "A3", juce::Colour::fromFloatRGBA (1.0f, 0.26f, 0.82f, 1.0f) },
                 a4ModeButton { "A4", juce::Colour::fromFloatRGBA (1.0f, 0.84f, 0.22f, 1.0f) },
                 b1ModeButton { "B1", juce::Colour::fromFloatRGBA (0.28f, 0.96f, 1.0f, 1.0f) },
                 b2ModeButton { "B2", juce::Colour::fromFloatRGBA (0.62f, 1.0f, 0.28f, 1.0f) },
                 b3ModeButton { "B3", juce::Colour::fromFloatRGBA (1.0f, 0.48f, 0.96f, 1.0f) },
                 b4ModeButton { "B4", juce::Colour::fromFloatRGBA (1.0f, 0.94f, 0.36f, 1.0f) };
    juce::Label patchTitle, scoreLabel, stepLabel, bpmValue, beatValue, toolValue, audioValue, inspectorTitle, layerValue;
    juce::TextEditor labelEditor, codeEditor;
    juce::Label glyphValueLabel;
    juce::Label helpLabel;
    juce::TextButton eraseButton { "Erase Cell" };
    std::array<juce::Label, 3> mixerStripLabels;
    std::array<juce::TextButton, 3> mixerMidiFxButtons;
    std::array<juce::TextButton, 3> mixerInstrumentButtons;
    std::array<juce::TextButton, 3> mixerEffectAButtons;
    std::array<juce::TextButton, 3> mixerEffectBButtons;
    std::array<juce::TextButton, 3> mixerEffectCButtons;
    std::array<juce::TextButton, 3> mixerEffectDButtons;
    std::array<juce::Slider, 3> mixerVolumeSliders;
    std::array<juce::Slider, 3> mixerPanSliders;
    std::array<juce::Label, 3> mixerVolumeLabels;
    std::array<juce::Label, 3> mixerPanLabels;

    juce::Array<Cell> grid;
    juce::Array<Snake> snakes;
    GlyphType selectedTool = GlyphType::tone;
    Cell* selectedCell = nullptr;
    int visibleLayer = 0;
    bool usePseudoDepthStageView = false;
    bool isSidebarExpanded = false;
    bool showingTitleScreen = true;
    bool hasResumableSession = false;
    bool mixerVisible = false;
    PanelSizeMode previewSizeMode = PanelSizeMode::defaultSize;
    PanelSizeMode inspectorSizeMode = PanelSizeMode::defaultSize;
    juce::String currentPatchName { "GlyphGrid" };
    AppMode currentMode = AppMode::glyphGrid;
    ModeVariant currentVariant = ModeVariant::a;
    int harmonySpaceKeyCenter = 0;
    int harmonySpaceConstraintMode = 0;
    bool harmonySpaceGestureRecordEnabled = false;
    juce::Array<juce::Point<int>> harmonySpaceGesturePoints;

    double bpm = 92.0;
    double currentSampleRate = 44100.0;
    float outputFastState = 0.0f;
    float outputSlowState = 0.0f;
    float outputFastCoeff = 0.0f;
    float outputSlowCoeff = 0.0f;
    float outputHpState = 0.0f;
    float outputHpInputState = 0.0f;
    float outputHpCoeff = 0.0f;
    std::atomic<double> currentTimeSeconds { 0.0 };
    std::atomic<bool> isRunning { false };
    std::atomic<int> lastAdvancedTick { -1 };
    std::atomic<int> pendingSpawnRequests { 0 };
    std::atomic<bool> pendingSnakeReset { false };
    std::atomic<float> publishedTransportPhase { 0.0f };
    std::atomic<uint64_t> transportSequence { 0 };
    ScoreResult lastScore;
    std::shared_ptr<const PatchSnapshot> currentPatchSnapshot;
    std::array<SnakeRuntime, maxSnakes> audioSnakes {};
    std::array<uint8_t, cols * rows * layers> automataCurrent {};
    std::array<uint8_t, cols * rows * layers> automataPrevious {};
    std::array<SynthVoice, maxSynthVoices> synthVoices {};
    std::array<DrumVoice, maxDrumVoices> drumVoices {};
    std::array<MixerStripState, 3> mixerStrips {};
    juce::AudioPluginFormatManager pluginFormatManager;
    int audioSnakeCount = 0;
    int currentPluginBlockSize = 512;
    int currentAudioBlockSampleOffset = 0;
    TransportSnapshot publishedTransportSnapshot;
    std::unique_ptr<juce::FileChooser> activeFileChooser;
    juce::Component* registeredKeyTarget = nullptr;
    juce::Random random;

    juce::CriticalSection stateLock;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
