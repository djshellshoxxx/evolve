#pragma once
#include <juce_gui_basics/juce_gui_basics.h>
#include <functional>
#include <memory>
#include <vector>

namespace mutagen
{
/** The "phreak" cheat menu: a feature tree. Double-click or press Enter on an entry to run it. */
class PhreakOverlay : public juce::Component
{
public:
    struct Item { juce::String name; std::function<void()> action; };
    struct Category { juce::String name; std::vector<Item> items; };

    PhreakOverlay();
    ~PhreakOverlay() override;

    void setTree (std::vector<Category> categories);
    void open();
    void close();

    /** Runs entry `item` of category `category`; returns false if out of range. For tests and the tree. */
    bool run (int category, int item);
    int categoryCount() const { return (int) tree.size(); }
    int itemCount (int category) const { return category >= 0 && category < categoryCount() ? (int) tree[(size_t) category].items.size() : 0; }

    void paint (juce::Graphics&) override;
    void resized() override;
    bool keyPressed (const juce::KeyPress&) override;

private:
    class Row;
    std::vector<Category> tree;
    std::unique_ptr<Row> root;
    juce::TreeView view;
    juce::TextButton closeButton { "CLOSE" };
    juce::Label status;
};
}
