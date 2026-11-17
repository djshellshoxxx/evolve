#include "PhreakOverlay.h"

namespace mutagen
{
class PhreakOverlay::Row : public juce::TreeViewItem
{
public:
    Row (PhreakOverlay& o, juce::String label, int cat, int item) : owner (o), text (std::move (label)), category (cat), index (item) {}
    bool mightContainSubItems() override { return index < 0; }
    bool canBeSelected() const override { return true; }
    int getItemHeight() const override { return 24; }
    void paintItem (juce::Graphics& g, int w, int h) override
    {
        if (isSelected()) { g.setColour (juce::Colour (0xff24406a)); g.fillRect (0, 0, w, h); }
        g.setColour (index < 0 ? juce::Colour (0xff7eead0) : juce::Colours::white);
        g.setFont (index < 0 ? juce::Font (juce::FontOptions (15.0f, juce::Font::bold)) : juce::Font (juce::FontOptions (14.0f)));
        g.drawText (text, 6, 0, w - 6, h, juce::Justification::centredLeft, true);
    }
    void itemOpennessChanged (bool open) override
    {
        if (open && getNumSubItems() == 0)
            for (int i = 0; i < owner.itemCount (category); ++i)
                addSubItem (new Row (owner, owner.tree[(size_t) category].items[(size_t) i].name, category, i));
    }
    void itemDoubleClicked (const juce::MouseEvent&) override { activate(); }
    void itemClicked (const juce::MouseEvent&) override { if (index < 0) setOpen (! isOpen()); }
    void activate()
    {
        if (index < 0) setOpen (! isOpen());
        else owner.run (category, index);
    }
    juce::String getUniqueName() const override { return juce::String (category) + "/" + juce::String (index); }
private:
    PhreakOverlay& owner;
    juce::String text;
    int category, index;
};

PhreakOverlay::PhreakOverlay()
{
    setWantsKeyboardFocus (true);
    addAndMakeVisible (view);
    view.setRootItemVisible (false);
    view.setDefaultOpenness (false);
    view.setColour (juce::TreeView::backgroundColourId, juce::Colour (0xff0d1422));
    addAndMakeVisible (closeButton);
    closeButton.onClick = [this] { close(); };
    addAndMakeVisible (status);
    status.setColour (juce::Label::textColourId, juce::Colours::lightgrey);
    status.setText ("Double-click or press Enter to run. Arrow keys move, Right/Left open and close a branch, Escape closes.", juce::dontSendNotification);
    setVisible (false);
}

PhreakOverlay::~PhreakOverlay() { view.setRootItem (nullptr); }

void PhreakOverlay::setTree (std::vector<Category> categories)
{
    view.setRootItem (nullptr);
    tree = std::move (categories);
    root = std::make_unique<Row> (*this, "PHREAK", -1, -2);
    view.setRootItem (root.get());
    root->setOpen (true);
    // Row::mightContainSubItems is keyed on index < 0, so the hidden root and the categories both open.
    for (int c = 0; c < categoryCount(); ++c)
        root->addSubItem (new Row (*this, tree[(size_t) c].name + "  (" + juce::String (itemCount (c)) + ")", c, -1));
}

void PhreakOverlay::open()
{
    setVisible (true); toFront (true); view.grabKeyboardFocus(); repaint();
}

void PhreakOverlay::close()
{
    setVisible (false);
    if (auto* p = getParentComponent()) p->grabKeyboardFocus();
}

bool PhreakOverlay::run (int category, int item)
{
    if (category < 0 || category >= categoryCount() || item < 0 || item >= itemCount (category)) return false;
    auto action = tree[(size_t) category].items[(size_t) item].action;
    close();                       // the action may open another overlay, so close this one first
    if (action) action();
    return true;
}

void PhreakOverlay::resized()
{
    auto r = getLocalBounds().reduced (24);
    closeButton.setBounds (r.removeFromTop (28).removeFromRight (90));
    status.setBounds (r.removeFromBottom (24));
    view.setBounds (r.reduced (0, 6));
}

void PhreakOverlay::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xf0111827));
    g.setColour (juce::Colour (0xff7eead0));
    g.setFont (juce::Font (juce::FontOptions (22.0f, juce::Font::bold)));
    g.drawText ("PHREAK  -  FEATURE TREE", 24, 24, getWidth() - 160, 28, juce::Justification::centredLeft);
}

bool PhreakOverlay::keyPressed (const juce::KeyPress& k)
{
    if (k == juce::KeyPress::escapeKey) { close(); return true; }
    if (k == juce::KeyPress::returnKey)
        if (auto* row = dynamic_cast<Row*> (view.getSelectedItem (0))) { row->activate(); return true; }
    return false;
}
}
