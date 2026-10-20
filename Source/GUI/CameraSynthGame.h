#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <juce_video/juce_video.h>
#include <juce_audio_basics/juce_audio_basics.h>
#include <memory>
#include <cmath>

namespace mutagen
{
    // Camera access is strictly opt-in. The game also works with mouse movement.
    class CameraSynthGame final : public juce::Component,
                                  private juce::CameraDevice::Listener,
                                  private juce::Timer
    {
    public:
        explicit CameraSynthGame (juce::MidiKeyboardState& keys) : keyboard (keys)
        {
            addAndMakeVisible (cameraButton);
            addAndMakeVisible (closeButton);
            cameraButton.setButtonText ("ENABLE CAMERA");
            closeButton.setButtonText ("CLOSE");
            cameraButton.onClick = [this] { openCamera(); };
            closeButton.onClick = [this] { setVisible (false); };
            setVisible (false);
            setWantsKeyboardFocus (true);
        }

        ~CameraSynthGame() override { stop(); }

        void show()
        {
            setVisible (true);
            toFront (true);
            startTimerHz (20);
        }

        void visibilityChanged() override
        {
            if (! isVisible()) stop();
        }

        void resized() override
        {
            auto r = getLocalBounds().reduced (20);
            auto buttons = r.removeFromBottom (44);
            closeButton.setBounds (buttons.removeFromRight (105));
            buttons.removeFromRight (12);
            cameraButton.setBounds (buttons.removeFromRight (170));
        }

        void mouseMove (const juce::MouseEvent& e) override { updateMouse (e.position); }
        void mouseDrag (const juce::MouseEvent& e) override { updateMouse (e.position); }

        void paint (juce::Graphics& g) override
        {
            const auto bounds = getLocalBounds().toFloat();
            g.fillAll (juce::Colour (0xff080d19));
            auto view = bounds.reduced (20.0f).withTrimmedTop (72.0f).withTrimmedBottom (55.0f);
            if (cameraEnabled && ! latestFrame.isNull())
                g.drawImageWithin (latestFrame, (int) view.getX(), (int) view.getY(),
                                   (int) view.getWidth(), (int) view.getHeight(),
                                   juce::RectanglePlacement::centred | juce::RectanglePlacement::onlyReduceInSize);
            g.setColour (juce::Colours::white);
            g.setFont (juce::Font (24.0f));
            g.drawText ("CHROMATIC PORTAL", 20, 14, getWidth() - 40, 32, juce::Justification::centred);
            g.setFont (14.0f);
            g.drawText (cameraEnabled ? "Move through the frame to play notes."
                                      : "Move the mouse to play. Camera access is optional.",
                        20, 47, getWidth() - 40, 22, juce::Justification::centred);
            const float x = view.getX() + horizontal * view.getWidth();
            const float y = view.getY() + vertical * view.getHeight();
            for (int i = 6; i >= 0; --i)
            {
                const float radius = 12.0f + i * 13.0f;
                g.setColour (juce::Colour::fromHSV (std::fmod (horizontal + i * 0.065f, 1.0f),
                             0.85f, 0.98f, 0.15f + (6 - i) * 0.10f));
                g.drawEllipse (x - radius, y - radius, radius * 2.0f, radius * 2.0f, 2.5f);
            }
        }

    private:
        void updateMouse (juce::Point<float> p)
        {
            if (cameraEnabled) return;
            horizontal = juce::jlimit (0.0f, 1.0f, p.x / juce::jmax (1.0f, (float) getWidth()));
            vertical = juce::jlimit (0.0f, 1.0f, p.y / juce::jmax (1.0f, (float) getHeight()));
        }

        void openCamera()
        {
            if (cameraEnabled) { closeCamera(); return; }
            auto names = juce::CameraDevice::getAvailableDevices();
            if (names.isEmpty()) { cameraButton.setButtonText ("NO CAMERA - USE MOUSE"); return; }
            camera.reset (juce::CameraDevice::openDevice (0));
            if (camera == nullptr) { cameraButton.setButtonText ("CAMERA UNAVAILABLE"); return; }
            camera->addListener (this);
            cameraEnabled = true;
            cameraButton.setButtonText ("DISABLE CAMERA");
        }

        void closeCamera()
        {
            cameraEnabled = false;
            if (camera != nullptr) camera->removeListener (this);
            camera.reset();
            latestFrame = {};
            cameraButton.setButtonText ("ENABLE CAMERA");
        }

        void imageReceived (const juce::Image& frame) override
        {
            // JUCE delivers frames asynchronously; only retain the latest image.
            const juce::ScopedLock lock (frameLock);
            pendingFrame = frame;
        }

        void timerCallback() override
        {
            if (cameraEnabled)
            {
                juce::Image frame;
                {
                    const juce::ScopedLock lock (frameLock);
                    frame = pendingFrame;
                    pendingFrame = {};
                }
                if (! frame.isNull())
                {
                    // Sample a coarse grid: cheap enough for the message thread.
                    float total = 0.0f, weightedX = 0.0f, weightedY = 0.0f;
                    for (int y = 0; y < 8; ++y)
                        for (int x = 0; x < 12; ++x)
                        {
                            const auto pixel = frame.getPixelAt (x * (frame.getWidth() - 1) / 11,
                                                                 y * (frame.getHeight() - 1) / 7);
                            const float b = pixel.getBrightness();
                            total += b;
                            weightedX += b * (x / 11.0f);
                            weightedY += b * (y / 7.0f);
                        }
                    if (total > 0.1f)
                    {
                        horizontal = juce::jlimit (0.0f, 1.0f, weightedX / total);
                        vertical = juce::jlimit (0.0f, 1.0f, weightedY / total);
                    }
                    latestFrame = frame;
                }
            }
            const int note = 48 + juce::jlimit (0, 24, (int) (horizontal * 24.99f));
            const float velocity = 0.25f + 0.7f * (1.0f - vertical);
            if (note != activeNote)
            {
                releaseNote();
                keyboard.noteOn (1, note, velocity);
                activeNote = note;
            }
            repaint();
        }

        void releaseNote()
        {
            if (activeNote >= 0) keyboard.noteOff (1, activeNote, 0.0f);
            activeNote = -1;
        }

        void stop()
        {
            stopTimer();
            releaseNote();
            closeCamera();
        }

        juce::MidiKeyboardState& keyboard;
        juce::TextButton cameraButton, closeButton;
        std::unique_ptr<juce::CameraDevice> camera;
        juce::CriticalSection frameLock;
        juce::Image pendingFrame, latestFrame;
        bool cameraEnabled = false;
        float horizontal = 0.5f, vertical = 0.5f;
        int activeNote = -1;
    };
}
