#include "ParamControl.h"
#include "MutagenLookAndFeel.h"
#include "../PluginProcessor.h"

namespace mutagen::paramMenu
{
    void tag (juce::Component& c, const juce::String& paramID)
    {
        c.getProperties().set (idProperty, paramID);
    }

    juce::String tagOf (const juce::Component& c)
    {
        return c.getProperties().getWithDefault (idProperty, juce::String()).toString();
    }

    /*  Walk up to the editor to reach this instance's processor. A global
        would be wrong here: a host can and does open several instances of the
        plugin, and each one owns its own MIDI map.  */
    static MutagenProcessor* processorFor (juce::Component& c)
    {
        for (auto* p = &c; p != nullptr; p = p->getParentComponent())
            if (auto* editor = dynamic_cast<juce::AudioProcessorEditor*> (p))
                return dynamic_cast<MutagenProcessor*> (editor->getAudioProcessor());
        return nullptr;
    }

    static juce::RangedAudioParameter* parameterFor (juce::Component& c, MutagenProcessor* proc)
    {
        const auto id = tagOf (c);
        if (proc == nullptr || id.isEmpty()) return nullptr;
        return proc->apvts.getParameter (id);
    }

    // -----------------------------------------------------------------
    //  "Set the control to a specific number"
    // -----------------------------------------------------------------

    static void askForValue (juce::Component& anchor,
                             const juce::String& title,
                             const juce::String& currentText,
                             std::function<void (const juce::String&)> onAccept)   // NOLINT
    {
        auto* window = new juce::AlertWindow (title, "Enter a value:",
                                              juce::MessageBoxIconType::NoIcon, &anchor);
        window->setLookAndFeel (&anchor.getLookAndFeel());
        window->addTextEditor ("value", currentText, {});
        window->addButton ("Set",    1, juce::KeyPress (juce::KeyPress::returnKey));
        window->addButton ("Cancel", 0, juce::KeyPress (juce::KeyPress::escapeKey));

        window->enterModalState (true, juce::ModalCallbackFunction::create (
            [window, accept = std::move (onAccept)] (int result)
            {
                if (result == 1)
                    accept (window->getTextEditorContents ("value"));

                window->setLookAndFeel (nullptr);
                delete window;
            }), false);
    }

    // -----------------------------------------------------------------

    enum MenuIds
    {
        idSetValue = 1,
        idReset,
        idMidiLearn,
        idMidiForget,
        idMidiClearAll
    };

    /*  Builds the shared part of the menu. `param` may be null - a control
        that is not bound to a plugin parameter still gets the value items,
        it just cannot be MIDI mapped.  */
    static juce::PopupMenu buildMenu (MutagenProcessor* proc,
                                      juce::RangedAudioParameter* param,
                                      bool allowSetValue,
                                      const juce::String& header)
    {
        juce::PopupMenu m;
        m.addSectionHeader (header);

        if (allowSetValue)
            m.addItem (idSetValue, "Set Value...");

        m.addItem (idReset, "Reset to Default");

        if (proc != nullptr && param != nullptr)
        {
            m.addSeparator();

            const int cc = proc->midiLearn.ccFor (param->paramID);
            if (proc->midiLearn.isLearning()
                && proc->midiLearn.learningParamID() == param->paramID)
            {
                m.addItem (idMidiLearn, "Listening for a CC... (click to cancel)");
            }
            else if (cc >= 0)
            {
                m.addItem (idMidiLearn,  "Re-map MIDI (currently CC " + juce::String (cc) + ")");
                m.addItem (idMidiForget, "Forget CC " + juce::String (cc));
            }
            else
            {
                m.addItem (idMidiLearn, "Map to MIDI...");
            }

            if (proc->midiLearn.mappingCount() > 0)
                m.addItem (idMidiClearAll, "Clear All MIDI Mappings ("
                                           + juce::String (proc->midiLearn.mappingCount()) + ")");
        }

        return m;
    }

    void showForSlider (juce::Slider& s)
    {
        auto* proc  = processorFor (s);
        auto* param = parameterFor (s, proc);

        const juce::String name = param != nullptr ? param->getName (40)
                                                   : s.getName().isNotEmpty() ? s.getName()
                                                                              : "Control";
        const juce::String valueText = param != nullptr
            ? param->getCurrentValueAsText()
            : juce::String (s.getValue(), 4);

        auto menu = buildMenu (proc, param, true, name + "  -  " + valueText);
        menu.setLookAndFeel (&s.getLookAndFeel());

        // The menu is modal-async: the slider can be gone by the time it
        // closes (the editor may be torn down under it), so everything the
        // callback needs is re-resolved from a SafePointer rather than
        // captured raw.
        juce::Component::SafePointer<juce::Slider> safe (&s);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&s),
            [safe] (int result)
            {
                auto* slider = safe.getComponent();
                if (slider == nullptr || result == 0) return;

                auto* proc  = processorFor (*slider);
                auto* param = parameterFor (*slider, proc);

                switch (result)
                {
                    case idSetValue:
                    {
                        const auto currentText = param != nullptr
                            ? param->getCurrentValueAsText()
                            : juce::String (slider->getValue(), 4);

                        askForValue (*slider, "Set Value", currentText,
                            [safe] (const juce::String& entered)
                            {
                                auto* sl = safe.getComponent();
                                if (sl == nullptr || entered.isEmpty()) return;

                                auto* pr = parameterFor (*sl, processorFor (*sl));
                                if (pr != nullptr)
                                {
                                    // Go through the parameter's own text
                                    // conversion so units and choice names
                                    // behave the way the host shows them.
                                    pr->beginChangeGesture();
                                    pr->setValueNotifyingHost (pr->getValueForText (entered));
                                    pr->endChangeGesture();
                                }
                                else
                                {
                                    sl->setValue (entered.getDoubleValue(),
                                                  juce::sendNotificationSync);
                                }
                            });
                        break;
                    }

                    case idReset:
                        if (param != nullptr)
                        {
                            param->beginChangeGesture();
                            param->setValueNotifyingHost (param->getDefaultValue());
                            param->endChangeGesture();
                        }
                        else if (slider->isDoubleClickReturnEnabled())
                        {
                            slider->setValue (slider->getDoubleClickReturnValue(),
                                              juce::sendNotificationSync);
                        }
                        break;

                    case idMidiLearn:
                        if (proc != nullptr && param != nullptr)
                        {
                            if (proc->midiLearn.isLearning()
                                && proc->midiLearn.learningParamID() == param->paramID)
                                proc->midiLearn.cancelLearn();
                            else
                                proc->midiLearn.beginLearn (param->paramID);
                        }
                        break;

                    case idMidiForget:
                        if (proc != nullptr && param != nullptr)
                            proc->midiLearn.clearMapping (param->paramID);
                        break;

                    case idMidiClearAll:
                        if (proc != nullptr) proc->midiLearn.clearAll();
                        break;

                    default: break;
                }
            });
    }

    void showForComponent (juce::Component& c)
    {
        auto* proc  = processorFor (c);
        auto* param = parameterFor (c, proc);
        if (param == nullptr && proc == nullptr) return;

        const juce::String name = param != nullptr ? param->getName (40) : c.getName();
        const juce::String valueText = param != nullptr ? param->getCurrentValueAsText()
                                                        : juce::String();

        // A toggle or a choice has no "specific number" worth typing, so the
        // value item is dropped rather than shown doing nothing useful.
        auto menu = buildMenu (proc, param, false,
                               valueText.isEmpty() ? name : name + "  -  " + valueText);
        menu.setLookAndFeel (&c.getLookAndFeel());

        juce::Component::SafePointer<juce::Component> safe (&c);

        menu.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (&c),
            [safe] (int result)
            {
                auto* comp = safe.getComponent();
                if (comp == nullptr || result == 0) return;

                auto* proc  = processorFor (*comp);
                auto* param = parameterFor (*comp, proc);
                if (param == nullptr || proc == nullptr) return;

                switch (result)
                {
                    case idReset:
                        param->beginChangeGesture();
                        param->setValueNotifyingHost (param->getDefaultValue());
                        param->endChangeGesture();
                        break;

                    case idMidiLearn:
                        if (proc->midiLearn.isLearning()
                            && proc->midiLearn.learningParamID() == param->paramID)
                            proc->midiLearn.cancelLearn();
                        else
                            proc->midiLearn.beginLearn (param->paramID);
                        break;

                    case idMidiForget:   proc->midiLearn.clearMapping (param->paramID); break;
                    case idMidiClearAll: proc->midiLearn.clearAll(); break;
                    default: break;
                }
            });
    }
}
