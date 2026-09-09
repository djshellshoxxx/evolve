#include "CultureChamber.h"
#include "MutagenLookAndFeel.h"
#include "../PluginProcessor.h"
#include <cmath>

namespace mutagen
{
    using namespace theme;

    CultureChamber::CultureChamber (MutagenProcessor& p) : processor (p)
    {
        setOpaque (true);
        setWantsKeyboardFocus (false);
    }

    void CultureChamber::resized()
    {
        field = getLocalBounds().toFloat().reduced (18.0f);
    }

    juce::Point<float> CultureChamber::toPixels (float nx, float ny) const
    {
        auto f = field.isEmpty() ? getLocalBounds().toFloat().reduced (18.0f) : field;
        return { f.getX() + nx * f.getWidth(), f.getY() + ny * f.getHeight() };
    }

    void CultureChamber::update (const EngineSnapshot& s, double dt)
    {
        snap = s;
        phase += dt;

        for (auto& r : ripples) { r.r += (float) dt * 120.0f; r.life -= (float) dt; }
        ripples.erase (std::remove_if (ripples.begin(), ripples.end(),
                        [] (const Ripple& r) { return r.life <= 0.0f; }), ripples.end());

        repaint();
    }

    // ---------------------------------------------------------------------

    void CultureChamber::paint (juce::Graphics& g)
    {
        auto b = getLocalBounds().toFloat();
        field = b.reduced (18.0f);

        // ---- ground ----
        juce::ColourGradient bgGrad (bg1.brighter (0.04f), b.getCentre(),
                                     bg0, b.getBottomRight(), true);
        g.setGradientFill (bgGrad);
        g.fillRect (b);

        // imaging rings
        g.setColour (stroke.withAlpha (0.10f));
        for (int i = 1; i <= 6; ++i)
        {
            const float rr = field.getWidth() * 0.5f * (float) i / 6.0f;
            g.drawEllipse (juce::Rectangle<float> (rr * 2.0f, rr * 2.0f).withCentre (field.getCentre()), 1.0f);
        }
        g.setColour (stroke.withAlpha (0.07f));
        for (int i = 0; i <= 8; ++i)
        {
            const float xx = field.getX() + field.getWidth() * (float) i / 8.0f;
            g.drawVerticalLine ((int) xx, field.getY(), field.getBottom());
        }

        // ---- environmental fields ----
        const float breathe = 0.5f + 0.5f * std::sin ((float) phase * 0.7f);

        // nutrient bloom
        {
            juce::ColourGradient ng (nutrient.withAlpha (0.10f + 0.06f * snap.nutrientField * breathe),
                                     field.getCentre(),
                                     juce::Colours::transparentBlack, field.getBottomRight(), true);
            g.setGradientFill (ng);
            g.fillEllipse (field);
        }
        // temperature speckle
        if (snap.temperatureField > 0.02f)
        {
            juce::Random r ((int) (phase * 30.0));
            g.setColour (accent.withAlpha (0.05f * snap.temperatureField));
            for (int i = 0; i < (int) (snap.temperatureField * 120.0f); ++i)
                g.fillRect (field.getX() + r.nextFloat() * field.getWidth(),
                            field.getY() + r.nextFloat() * field.getHeight(), 1.4f, 1.4f);
        }
        // infection haze
        if (snap.infectionField > 0.03f)
        {
            g.setColour (infection.withAlpha (0.06f * snap.infectionField * (0.6f + 0.4f * breathe)));
            g.fillRect (field);
        }
        // selection pressure front sweeping across
        if (snap.pressureFront > 0.02f)
        {
            const float sweep = std::fmod ((float) phase * 0.6f, 1.0f);
            const float sx = field.getX() + sweep * field.getWidth();
            juce::ColourGradient pg (juce::Colours::transparentBlack, sx - 40.0f, 0,
                                     accent.withAlpha (0.25f * snap.pressureFront), sx, 0, false);
            pg.addColour (1.0, juce::Colours::transparentBlack);
            g.setGradientFill (pg);
            g.fillRect (juce::Rectangle<float> (sx - 40.0f, field.getY(), 80.0f, field.getHeight()));
        }
        // whole-frame flashes
        if (snap.geneTransferFlash > 0.02f)
        { g.setColour (accent.withAlpha (0.05f * snap.geneTransferFlash)); g.fillRect (b); }
        if (snap.extinctionFlash > 0.02f)
        { g.setColour (infection.withAlpha (0.06f * snap.extinctionFlash)); g.fillRect (b); }

        // ---- symbiotic links ----
        g.setColour (resonator.withAlpha (0.20f));
        for (int i = 0; i < snap.count; ++i)
        {
            const auto& c = snap.cells[i];
            if (c.linkTo < 0) continue;
            for (int j = 0; j < snap.count; ++j)
            {
                // linkTo is a slot in the engine, not this list; match by proximity of ids
                if (snap.cells[j].familyId == c.linkTo || j == c.linkTo)
                {
                    g.drawLine ({ toPixels (c.x, c.y), toPixels (snap.cells[j].x, snap.cells[j].y) }, 1.0f);
                    break;
                }
            }
        }

        // ---- gene-transfer / infection / division arcs ----
        for (int i = 0; i < snap.arcCount; ++i)
        {
            const auto& a = snap.arcs[i];
            if (a.life <= 0.0f) continue;
            const auto p1 = toPixels (a.x1, a.y1);
            const auto p2 = toPixels (a.x2, a.y2);
            const auto mid = (p1 + p2) * 0.5f + juce::Point<float> (0, -30.0f);
            juce::Path path;
            path.startNewSubPath (p1);
            path.quadraticTo (mid, p2);

            juce::Colour ac = a.kind == 1 ? accent : a.kind == 2 ? infection : spectral;
            g.setColour (ac.withAlpha (juce::jlimit (0.0f, 0.8f, a.life)));
            g.strokePath (path, juce::PathStrokeType (1.6f));

            // travelling pulse
            const float t = 1.0f - juce::jlimit (0.0f, 1.0f, a.life);
            juce::Point<float> dot = p1 + (p2 - p1) * t;
            g.setColour (ac.withAlpha (a.life));
            g.fillEllipse (juce::Rectangle<float> (5.0f, 5.0f).withCentre (dot));
        }

        // ---- ripples from interaction ----
        for (const auto& r : ripples)
        {
            g.setColour (r.c.withAlpha (juce::jlimit (0.0f, 0.5f, r.life / r.maxLife * 0.5f)));
            g.drawEllipse (juce::Rectangle<float> (r.r * 2.0f, r.r * 2.0f).withCentre ({ r.x, r.y }), 1.6f);
        }

        // ---- cells ----
        const float baseR = juce::jmin (field.getWidth(), field.getHeight());
        for (int i = 0; i < snap.count; ++i)
        {
            const auto& c = snap.cells[i];
            const auto pos = toPixels (c.x, c.y);
            const float rad = juce::jlimit (3.0f, 60.0f, c.radius * baseR + 4.0f);
            const bool dying = c.stage >= 5;
            const bool germ  = c.stage == 0;

            juce::Colour col = speciesColour (c.species);
            if (c.species == 1) col = spectral.interpolatedWith (spectralV, juce::jlimit (0.0f, 1.0f, c.fitness));
            float alpha = juce::jlimit (0.15f, 1.0f, 0.3f + 0.7f * c.energy) * (dying ? 0.35f : 1.0f);
            if (c.muted) alpha *= 0.3f;

            // halo
            g.setColour (col.withAlpha (0.12f * alpha));
            g.fillEllipse (juce::Rectangle<float> (rad * 3.2f, rad * 3.2f).withCentre (pos));

            if (c.species == 0) // grain: fragment cluster
            {
                juce::Random fr (c.familyId * 977 + (int) (phase * 8.0));
                for (int k = 0; k < 4; ++k)
                {
                    const float ang = fr.nextFloat() * juce::MathConstants<float>::twoPi;
                    const float dr  = rad * (0.4f + 0.9f * fr.nextFloat());
                    juce::Point<float> fp (pos.x + std::cos (ang) * dr, pos.y + std::sin (ang) * dr);
                    g.setColour (col.withAlpha (alpha * (0.5f + 0.5f * fr.nextFloat())));
                    g.fillEllipse (juce::Rectangle<float> (rad * 0.5f, rad * 0.5f).withCentre (fp));
                }
                g.setColour (col.withAlpha (alpha));
                juce::Path diamond;
                diamond.addPolygon (pos, 4, rad * 0.8f, (float) phase * 0.5f);
                g.fillPath (diamond);
            }
            else if (c.species == 1) // spectral: glowing orb
            {
                juce::ColourGradient og (col.withAlpha (alpha), pos.x, pos.y,
                                         col.withAlpha (0.0f), pos.x + rad, pos.y + rad, true);
                g.setGradientFill (og);
                g.fillEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (pos));
                g.setColour (accent.withAlpha (alpha * 0.7f));
                g.fillEllipse (juce::Rectangle<float> (rad * 0.5f, rad * 0.5f).withCentre (pos));
            }
            else // resonator: concentric membranes
            {
                for (int k = 3; k >= 1; --k)
                {
                    g.setColour (col.withAlpha (alpha * (0.15f + 0.15f * k)));
                    g.drawEllipse (juce::Rectangle<float> (rad * 0.8f * k, rad * 0.8f * k).withCentre (pos), 1.4f);
                }
                g.setColour (col.withAlpha (alpha));
                g.fillEllipse (juce::Rectangle<float> (rad * 0.7f, rad * 0.7f).withCentre (pos));
            }

            // germinating sprout
            if (germ)
            {
                g.setColour (accent.withAlpha (0.6f));
                g.drawLine (pos.x, pos.y, pos.x, pos.y - rad * 1.6f, 1.4f);
            }

            // infection ring
            if (c.infectionType > 0 && c.infection > 0.05f)
            {
                const float flick = 0.5f + 0.5f * std::sin ((float) phase * 20.0f + i);
                g.setColour (infectionColour (c.infectionType).withAlpha (alpha * c.infection * flick));
                g.drawEllipse (juce::Rectangle<float> (rad * 2.4f, rad * 2.4f).withCentre (pos), 1.8f);
            }

            // pulse (division / gene transfer)
            if (c.pulse > 0.02f)
            {
                g.setColour (accent.withAlpha (c.pulse * 0.6f));
                g.drawEllipse (juce::Rectangle<float> (rad * (2.0f + 3.0f * (1.0f - c.pulse)),
                                                       rad * (2.0f + 3.0f * (1.0f - c.pulse))).withCentre (pos), 1.5f);
            }

            // preserved marker
            if (c.preserved)
            {
                g.setColour (selectRing.withAlpha (0.9f));
                g.drawEllipse (juce::Rectangle<float> (rad * 2.0f, rad * 2.0f).withCentre (pos), 1.0f);
            }

            // selection highlight
            const bool sel = selection.active &&
                ((selection.level == ScopeLevel::species && selection.id == c.species)
              || (selection.level == ScopeLevel::family  && selection.id == c.familyId)
              || (selection.level == ScopeLevel::cell    && selection.id == i));
            if (sel)
            {
                g.setColour (selectRing);
                g.drawEllipse (juce::Rectangle<float> (rad * 2.6f, rad * 2.6f).withCentre (pos), 2.0f);
            }

            if (i == hoverCell)
            {
                g.setColour (accent.withAlpha (0.7f));
                g.drawEllipse (juce::Rectangle<float> (rad * 2.9f, rad * 2.9f).withCentre (pos), 1.0f);
            }
        }

        // ---- corner legend / stats ----
        g.setColour (text.withAlpha (0.85f));
        g.setFont (12.0f);
        auto legend = field.removeFromTop (0); // no-op, keep field intact
        juce::ignoreUnused (legend);
        auto hud = juce::Rectangle<float> (b.getX() + 12, b.getBottom() - 58, 260, 46);
        g.setColour (bg0.withAlpha (0.55f));
        g.fillRoundedRectangle (hud, 5.0f);
        g.setColour (text);
        g.drawText ("POP " + juce::String (snap.population)
                    + "   GEN " + juce::String (snap.generation)
                    + (snap.preservedMode ? "   [PRESERVED]" : ""),
                    hud.reduced (8, 4).removeFromTop (18), juce::Justification::centredLeft);
        auto barR = hud.reduced (8, 4).removeFromBottom (16);
        g.setColour (panelHi); g.fillRoundedRectangle (barR, 2.0f);
        g.setColour (grain.withAlpha (0.9f));
        g.fillRoundedRectangle (barR.withWidth (barR.getWidth() * juce::jlimit (0.0f, 1.0f, snap.avgFitness)), 2.0f);
        g.setColour (spectral.withAlpha (0.8f));
        g.drawText ("fitness", barR, juce::Justification::centredRight);

        // hover label
        if (hoverCell >= 0 && hoverCell < snap.count)
        {
            const auto& c = snap.cells[hoverCell];
            const char* sp = c.species == 0 ? "grain" : c.species == 1 ? "spectral" : "resonator";
            juce::String txt;
            txt << sp << "  fam#" << c.familyId << "  gen " << c.generation
                << "  e" << juce::String (c.energy, 2);
            auto tb = juce::Rectangle<float> (mousePos.x + 12, mousePos.y - 8, 190, 20);
            g.setColour (bg0.withAlpha (0.85f));
            g.fillRoundedRectangle (tb, 4.0f);
            g.setColour (accent);
            g.setFont (11.5f);
            g.drawText (txt, tb.reduced (6, 2), juce::Justification::centredLeft);
        }
    }

    // ---------------------------------------------------------------------

    int CultureChamber::hitTestCell (juce::Point<float> p) const
    {
        int best = -1;
        float bestD = 1.0e9f;
        const float baseR = juce::jmin (field.getWidth(), field.getHeight());
        for (int i = 0; i < snap.count; ++i)
        {
            const auto pos = toPixels (snap.cells[i].x, snap.cells[i].y);
            const float rad = juce::jlimit (5.0f, 60.0f, snap.cells[i].radius * baseR + 6.0f);
            const float d = pos.getDistanceFrom (p);
            if (d < rad * 1.4f && d < bestD) { bestD = d; best = i; }
        }
        return best;
    }

    void CultureChamber::emitSelection()
    {
        if (onSelectionChanged) onSelectionChanged (selection);
        repaint();
    }

    void CultureChamber::mouseDown (const juce::MouseEvent& e)
    {
        const int hit = hitTestCell (e.position);

        if (e.mods.isPopupMenu())
        {
            showContextMenu (hit);
            return;
        }

        if (hit < 0)
        {
            selection.active = false;
            selection.level = ScopeLevel::colony;
            selection.cellSlot = -1;
            ripples.push_back ({ e.position.x, e.position.y, 4.0f, 0.6f, 0.6f, nutrient });
            emitSelection();
            return;
        }

        const auto& c = snap.cells[hit];
        selection.active = true;
        selection.cellSlot = hit;
        if (e.mods.isAltDown() || e.mods.isCommandDown())
        { selection.level = ScopeLevel::species; selection.id = c.species; }
        else if (e.mods.isShiftDown())
        { selection.level = ScopeLevel::cell; selection.id = hit; }
        else
        { selection.level = ScopeLevel::family; selection.id = c.familyId; }

        ripples.push_back ({ toPixels (c.x, c.y).x, toPixels (c.x, c.y).y, 6.0f, 0.5f, 0.5f,
                             speciesColour (c.species) });
        emitSelection();
    }

    void CultureChamber::mouseDrag (const juce::MouseEvent& e)
    {
        mousePos = e.position;
        if (! e.mods.isPopupMenu() && (e.getDistanceFromDragStart() % 9 == 0))
            ripples.push_back ({ e.position.x, e.position.y, 3.0f, 0.4f, 0.4f, nutrient.withAlpha (0.6f) });
    }

    void CultureChamber::mouseUp (const juce::MouseEvent&) {}

    void CultureChamber::mouseMove (const juce::MouseEvent& e)
    {
        mousePos = e.position;
        const int h = hitTestCell (e.position);
        if (h != hoverCell) { hoverCell = h; repaint(); }
    }

    void CultureChamber::mouseExit (const juce::MouseEvent&)
    {
        hoverCell = -1;
        repaint();
    }

    void CultureChamber::mouseDoubleClick (const juce::MouseEvent& e)
    {
        const int hit = hitTestCell (e.position);
        if (hit >= 0)
        {
            const auto& c = snap.cells[hit];
            Selection s;
            s.active = true;
            s.level = ScopeLevel::family;
            s.id = c.familyId;
            s.cellSlot = hit;
            if (onSendToBreedingLab) onSendToBreedingLab (s);
        }
        else
        {
            // drop a seed: spawn a little burst on a random pitch
            EngineCommand cmd;
            cmd.type = CommandType::noteBurst;
            cmd.ia = 48 + juce::Random::getSystemRandom().nextInt (24);
            cmd.fa = 0.9f;
            processor.pushCommand (cmd);
            ripples.push_back ({ e.position.x, e.position.y, 8.0f, 0.9f, 0.9f, accent });
        }
    }

    // ---------------------------------------------------------------------

    void CultureChamber::showContextMenu (int cellIndex)
    {
        juce::PopupMenu m;

        Selection target = selection;
        if (cellIndex >= 0)
        {
            const auto& c = snap.cells[cellIndex];
            target.active = true;
            target.cellSlot = cellIndex;
            if (! selection.active || selection.level == ScopeLevel::colony)
            { target.level = ScopeLevel::family; target.id = c.familyId; }
        }

        const juce::String what = target.describe();
        m.addSectionHeader (what);
        m.addItem (1, "Inspect genome");
        m.addSeparator();
        m.addItem (2, "Isolate (solo)");
        m.addItem (3, "Clear isolation");
        m.addItem (4, "Mute");
        m.addItem (5, "Unmute");
        m.addSeparator();
        m.addItem (6, "Preserve");
        m.addItem (7, "Release (un-preserve)");
        m.addItem (8, "Eliminate now");
        m.addSeparator();

        juce::PopupMenu infect;
        infect.addItem (20, "Metallize");
        infect.addItem (21, "Reverse");
        infect.addItem (22, "Vocalise");
        infect.addItem (23, "Destabilise");
        infect.addItem (24, "Cure infection");
        m.addSubMenu ("Infect", infect);

        m.addItem (9, "Send to Breeding Lab");
        m.addSeparator();
        m.addItem (30, "Select whole colony");
        if (cellIndex >= 0)
        {
            m.addItem (31, "Select this species");
            m.addItem (32, "Select this family");
            m.addItem (33, "Select just this cell");
        }

        auto self = juce::Component::SafePointer<CultureChamber> (this);
        m.showMenuAsync (juce::PopupMenu::Options().withTargetComponent (this),
            [self, target, cellIndex] (int r) mutable
            {
                if (self == nullptr || r == 0) return;
                auto& proc = self->processor;

                auto send = [&] (CommandType t, int ib = 0, float fa = 0.0f, int ia = 0)
                {
                    EngineCommand c;
                    c.type = t;
                    c.scope = target.active ? target.level : ScopeLevel::colony;
                    c.scopeId = target.id;
                    c.ib = ib; c.fa = fa; c.ia = ia;
                    proc.pushCommand (c);
                };

                switch (r)
                {
                    case 1: if (self->onInspect) self->onInspect (target); break;
                    case 2: send (CommandType::isolate); break;
                    case 3: send (CommandType::unisolate); break;
                    case 4: send (CommandType::muteScope, 1); break;
                    case 5: send (CommandType::muteScope, 0); break;
                    case 6: send (CommandType::preserveScope, 1); break;
                    case 7: send (CommandType::preserveScope, 0); break;
                    case 8: send (CommandType::eliminateScope); break;
                    case 9: if (self->onSendToBreedingLab) self->onSendToBreedingLab (target); break;
                    case 20: send (CommandType::infect, 0, 0.0f, 1); break;
                    case 21: send (CommandType::infect, 0, 0.0f, 2); break;
                    case 22: send (CommandType::infect, 0, 0.0f, 3); break;
                    case 23: send (CommandType::infect, 0, 0.0f, 4); break;
                    case 24: send (CommandType::cure); break;
                    case 30: self->selection.active = false; self->selection.level = ScopeLevel::colony; self->emitSelection(); break;
                    case 31: if (cellIndex >= 0) { self->selection.active = true; self->selection.level = ScopeLevel::species; self->selection.id = self->snap.cells[cellIndex].species; self->emitSelection(); } break;
                    case 32: if (cellIndex >= 0) { self->selection.active = true; self->selection.level = ScopeLevel::family; self->selection.id = self->snap.cells[cellIndex].familyId; self->emitSelection(); } break;
                    case 33: if (cellIndex >= 0) { self->selection.active = true; self->selection.level = ScopeLevel::cell; self->selection.id = cellIndex; self->emitSelection(); } break;
                    default: break;
                }
            });
    }
}
