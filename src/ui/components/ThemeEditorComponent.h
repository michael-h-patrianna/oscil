/**
 * @file ThemeEditorComponent.h
 * @brief Theme creation and editing interface for custom color schemes
 *
 * This component provides a comprehensive theme editor interface allowing users
 * to create, modify, and save custom color themes for the Oscil oscilloscope plugin.
 * It features real-time preview, accessibility validation, and integration with
 * the theme management system.
 *
 * Key Features:
 * - Visual color picker interface for all theme properties
 * - Real-time theme preview with live oscilloscope simulation
 * - Automatic accessibility validation (WCAG 2.1 AA compliance)
 * - Theme import/export functionality via JSON
 * - Built-in theme loading and customization
 * - Undo/redo functionality for theme editing
 *
 * Performance Characteristics:
 * - Lightweight modal dialog design
 * - Responsive color picker controls
 * - Real-time validation feedback
 * - Efficient theme preview updates
 * - Minimal memory footprint
 *
 * Usage Patterns:
 * - Modal dialog for focused theme editing
 * - Integration with main theme management system
 * - Non-destructive editing with preview mode
 * - Export/share custom themes between sessions
 *
 * @author Oscil Development Team
 * @version 1.0
 * @date 2024
 */

#pragma once

#include "../../theme/ThemeManager.h"
#include "../../theme/ColorTheme.h"
#include <juce_gui_basics/juce_gui_basics.h>
#include <memory>
#include <functional>

namespace oscil::ui::components {

/**
 * @brief Comprehensive theme editor interface for creating and modifying color themes
 *
 * The ThemeEditorComponent provides a full-featured interface for theme creation
 * and customization. It includes color pickers for all theme properties, real-time
 * preview functionality, accessibility validation, and import/export capabilities.
 *
 * Key Components:
 * - Color picker grid for core UI colors
 * - Waveform color palette editor (8 base colors)
 * - Theme metadata editor (name, description)
 * - Real-time preview with simulated oscilloscope display
 * - Accessibility validation indicators
 * - Import/export functionality
 *
 * Thread Safety:
 * - All operations are performed on the message thread
 * - Theme changes are applied atomically via ThemeManager
 * - No blocking operations during user interaction
 *
 * Performance Considerations:
 * - Efficient color picker updates
 * - Lightweight preview rendering
 * - Minimal recomputation during editing
 * - Responsive user interface under all conditions
 */
class ThemeEditorComponent : public juce::Component,
                            public juce::Button::Listener,
                            public juce::TextEditor::Listener {
public:
    /**
     * @brief Callback type for theme editor events
     */
    using ThemeEditorCallback = std::function<void(const theme::ColorTheme&)>;

    /**
     * @brief Constructor
     * @param themeManager Reference to the global theme manager
     */
    explicit ThemeEditorComponent(std::shared_ptr<theme::ThemeManager> themeManager);

    /**
     * @brief Destructor
     */
    ~ThemeEditorComponent() override;

    // Prevent copying and moving
    ThemeEditorComponent(const ThemeEditorComponent&) = delete;
    ThemeEditorComponent& operator=(const ThemeEditorComponent&) = delete;
    ThemeEditorComponent(ThemeEditorComponent&&) = delete;
    ThemeEditorComponent& operator=(ThemeEditorComponent&&) = delete;

    /**
     * @brief Sets the theme to edit (creates a working copy)
     * @param theme The theme to load for editing
     */
    void setThemeToEdit(const theme::ColorTheme& theme);

    /**
     * @brief Gets the currently edited theme
     * @return Current theme state including all modifications
     */
    const theme::ColorTheme& getCurrentTheme() const { return workingTheme; }

    /**
     * @brief Sets callback for theme change events
     * @param callback Function to call when theme is modified
     */
    void setThemeChangeCallback(ThemeEditorCallback callback) {
        themeChangeCallback = std::move(callback);
    }

    /**
     * @brief Sets callback for theme save events
     * @param callback Function to call when user saves theme
     */
    void setThemeSaveCallback(ThemeEditorCallback callback) {
        themeSaveCallback = std::move(callback);
    }

    /**
     * @brief Validates the current theme for accessibility compliance
     * @return true if theme meets WCAG 2.1 AA standards
     */
    bool validateCurrentTheme() const;

    /**
     * @brief Exports current theme to JSON string
     * @return JSON representation of the current theme
     */
    juce::String exportThemeToJson() const;

    /**
     * @brief Imports theme from JSON string
     * @param jsonString JSON representation of theme to import
     * @return true if import was successful
     */
    bool importThemeFromJson(const juce::String& jsonString);

    /**
     * @brief Resets theme to original state (undo all changes)
     */
    void resetToOriginal();

    // JUCE Component overrides
    void paint(juce::Graphics& g) override;
    void resized() override;

    // Button::Listener overrides
    void buttonClicked(juce::Button* button) override;

    // TextEditor::Listener overrides
    void textEditorTextChanged(juce::TextEditor& editor) override;

private:
    /**
     * @brief Color picker button component for theme properties
     */
    class ColorPickerButton : public juce::TextButton {
    public:
        ColorPickerButton(const juce::String& name, juce::Colour initialColor);
        void setColor(const juce::Colour& newColor);
        juce::Colour getColor() const { return currentColor; }
        void paint(juce::Graphics& g) override;

    private:
        juce::Colour currentColor;
    };

    /**
     * @brief Waveform color palette editor
     */
    class WaveformColorPalette : public juce::Component {
    public:
        WaveformColorPalette();
        void setColors(const std::array<juce::Colour, 8>& colors);
        std::array<juce::Colour, 8> getColors() const;
        void paint(juce::Graphics& g) override;
        void resized() override;
        void mouseDown(const juce::MouseEvent& event) override;

        std::function<void(int, juce::Colour)> onColorChanged;

    private:
        std::array<juce::Colour, 8> waveformColors;
        std::vector<std::unique_ptr<ColorPickerButton>> colorButtons;
    };

    /**
     * @brief Theme preview component showing simulated oscilloscope
     */
    class ThemePreviewComponent : public juce::Component {
    public:
        ThemePreviewComponent();
        void setTheme(const theme::ColorTheme& theme);
        void paint(juce::Graphics& g) override;

    private:
        theme::ColorTheme previewTheme;
        void drawSimulatedWaveforms(juce::Graphics& g, juce::Rectangle<int> bounds);
    };

    // Component initialization
    void initializeComponents();
    void setupLayout();
    void connectCallbacks();

    // Event handlers
    void handleColorChange();
    void handleMetadataChange();
    void handleSaveButtonClick();
    void handleCancelButtonClick();
    void handleResetButtonClick();
    void handleImportButtonClick();
    void handleExportButtonClick();

    // Color picker management
    void showColorPicker(juce::Component* parent, juce::Colour currentColor,
                        std::function<void(juce::Colour)> onColorSelected);

    // Theme operations
    void updateWorkingTheme();
    void updatePreview();
    void notifyThemeChanged();

    // Validation
    void updateValidationStatus();
    void showValidationWarnings();

    // UI layout
    void layoutMainComponents();
    void layoutColorPickers(juce::Rectangle<int> area);
    void layoutMetadataEditors();
    void layoutActionButtons(juce::Rectangle<int> area);

    // Member variables
    std::shared_ptr<theme::ThemeManager> themeManager_;
    theme::ColorTheme originalTheme;
    theme::ColorTheme workingTheme;

    // Callbacks
    ThemeEditorCallback themeChangeCallback;
    ThemeEditorCallback themeSaveCallback;

    // UI Components
    std::unique_ptr<juce::Label> titleLabel;
    std::unique_ptr<juce::TextEditor> nameEditor;
    std::unique_ptr<juce::TextEditor> descriptionEditor;

    // Core color pickers
    std::unique_ptr<ColorPickerButton> backgroundColorButton;
    std::unique_ptr<ColorPickerButton> surfaceColorButton;
    std::unique_ptr<ColorPickerButton> textColorButton;
    std::unique_ptr<ColorPickerButton> textSecondaryColorButton;
    std::unique_ptr<ColorPickerButton> accentColorButton;
    std::unique_ptr<ColorPickerButton> borderColorButton;
    std::unique_ptr<ColorPickerButton> gridColorButton;

    // Waveform color palette
    std::unique_ptr<WaveformColorPalette> waveformPalette;

    // Preview
    std::unique_ptr<ThemePreviewComponent> previewComponent;

    // Action buttons
    std::unique_ptr<juce::TextButton> saveButton;
    std::unique_ptr<juce::TextButton> cancelButton;
    std::unique_ptr<juce::TextButton> resetButton;
    std::unique_ptr<juce::TextButton> importButton;
    std::unique_ptr<juce::TextButton> exportButton;

    // Validation indicators
    std::unique_ptr<juce::Label> validationStatusLabel;
    std::unique_ptr<juce::TextButton> validationDetailsButton;

    // Layout constants
    static constexpr int COMPONENT_MARGIN = 8;
    static constexpr int BUTTON_HEIGHT = 32;
    static constexpr int COLOR_BUTTON_SIZE = 40;
    static constexpr int PREVIEW_HEIGHT = 200;
    static constexpr int MIN_WIDTH = 600;
    static constexpr int MIN_HEIGHT = 800;
};

} // namespace oscil::ui::components
