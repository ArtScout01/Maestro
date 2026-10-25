#pragma once
#include "PluginProcessor.h"

// GUI nach "App Style Guide Aquarell v2": weisse Buehne, ein Shore-Hero mit Korn, Ink #0b0b0a,
// Pillen-Chips (aktiv = Ink + weisser Text), 1,5-px-Umriss fuer Zweit-Button, keine harten Schatten.
namespace aq
{
const juce::Colour ink (0xff0b0b0a), fg (0xff2a2622), muted (0xff6b6b65), line (0xffe6e6e1), surface (0xfff5f5f2),
                   chip (0xffdfe6f3), chipHover (0xffcfd9ee), peachDeep (0xffc2304f), peachRed (0xffe4476a),
                   honey (0xfff9b21b), horizon (0xff4c88ca);

inline juce::Image makeGrain()
{
    juce::Image im (juce::Image::ARGB, 160, 160, true);
    juce::Random r (7);
    for (int y = 0; y < 160; ++y)
        for (int x = 0; x < 160; ++x)
            im.setPixelAt (x, y, juce::Colour::fromFloatRGBA (r.nextFloat(), r.nextFloat(), r.nextFloat(), 1.0f));
    return im;
}

class Laf : public juce::LookAndFeel_V4
{
public:
    Laf() { setColour (juce::Slider::textBoxTextColourId, muted);
            setColour (juce::Slider::textBoxOutlineColourId, juce::Colours::transparentBlack);
            setColour (juce::Label::textColourId, ink); }

    juce::Font getTextButtonFont (juce::TextButton&, int) override { return juce::Font (juce::FontOptions (15.f)); }

    void drawRotarySlider (juce::Graphics& g, int x, int y, int w, int h, float pos, float a0, float a1, juce::Slider&) override
    {
        const float d = (float) juce::jmin (w, h) - 10.f, r = d * 0.5f;
        const float cx = (float) x + (float) w * 0.5f, cy = (float) y + (float) h * 0.5f;
        g.setColour (surface);
        g.fillEllipse (cx - r + 7.f, cy - r + 7.f, d - 14.f, d - 14.f);
        juce::Path track, val;
        track.addCentredArc (cx, cy, r, r, 0.f, a0, a1, true);
        g.setColour (line);
        g.strokePath (track, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        const float a = a0 + pos * (a1 - a0);
        val.addCentredArc (cx, cy, r, r, 0.f, a0, a, true);
        g.setColour (ink);
        g.strokePath (val, juce::PathStrokeType (4.f, juce::PathStrokeType::curved, juce::PathStrokeType::rounded));
        g.setColour (peachDeep);
        g.fillEllipse (cx + (r - 7.f) * std::sin (a) - 5.f, cy - (r - 7.f) * std::cos (a) - 5.f, 10.f, 10.f);
    }

    void drawButtonBackground (juce::Graphics& g, juce::Button& b, const juce::Colour&, bool hi, bool) override
    {
        auto r = b.getLocalBounds().toFloat().reduced (1.f);
        const float rad = r.getHeight() * 0.5f;
        if (b.getProperties().contains ("secondary"))
        {
            if (hi) { g.setColour (surface); g.fillRoundedRectangle (r, rad); }
            g.setColour (ink);
            g.drawRoundedRectangle (r.reduced (0.75f), rad, 1.5f);
            return;
        }
        g.setColour (b.getToggleState() ? ink : (hi ? chipHover : chip));
        g.fillRoundedRectangle (r, rad);
    }

    void drawButtonText (juce::Graphics& g, juce::TextButton& b, bool, bool) override
    {
        auto r = b.getLocalBounds().toFloat();
        const bool secondary = b.getProperties().contains ("secondary");
        g.setColour (b.getToggleState() && ! secondary ? juce::Colours::white : ink);
        g.setFont (getTextButtonFont (b, b.getHeight()));
        if (b.getProperties().contains ("dot"))
        {
            const auto dot = juce::Colour ((juce::uint32) (int) b.getProperties()["dot"]);
            const float dx = r.getX() + 16.f, dy = r.getCentreY() - 5.f;
            g.setColour (dot); g.fillEllipse (dx, dy, 10.f, 10.f);
            g.setColour (b.getToggleState() ? juce::Colours::white.withAlpha (0.8f) : muted.withAlpha (0.6f));
            g.drawEllipse (dx, dy, 10.f, 10.f, 1.f);
            g.setColour (b.getToggleState() ? juce::Colours::white : ink);
            g.drawText (b.getButtonText(), r.withTrimmedLeft (32.f).withTrimmedRight (8.f), juce::Justification::centredLeft);
        }
        else
            g.drawText (b.getButtonText(), r, juce::Justification::centred);
    }
};
} // namespace aq

class G2Editor : public juce::AudioProcessorEditor
{
public:
    explicit G2Editor (G2Processor& p) : juce::AudioProcessorEditor (p), proc (p), grain (aq::makeGrain())
    {
        using APVTS = juce::AudioProcessorValueTreeState;
        setLookAndFeel (&laf);

        struct K { const char* id; const char* label; };
        static const K ks[] = { { "sens", "Sensitivity" }, { "bongoAdj", "Bongo" }, { "echoWidth", "Perc Adj" },
                                { "percVol", "Percussion" }, { "bassVol", "Bass" }, { "trebleVol", "Volume" },
                                { "trebleTone", "Tone" }, { "wowSpeed", "Wow Wow" }, { "echoSpeed", "Echo Repeat" } };
        for (auto& k : ks)
        {
            auto kn = std::make_unique<Knob>();
            kn->s.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
            kn->s.setRotaryParameters (1.25f * juce::MathConstants<float>::pi, 2.75f * juce::MathConstants<float>::pi, true);
            kn->s.setTextBoxStyle (juce::Slider::TextBoxBelow, true, 64, 20);
            kn->s.textFromValueFunction = [] (double v) { return juce::String (v * 10.0, 1); };   // Poti-Skala 0-10
            kn->l.setText (k.label, juce::dontSendNotification);
            kn->l.setJustificationType (juce::Justification::centred);
            kn->l.setFont (juce::Font (juce::FontOptions (14.f)));
            kn->a = std::make_unique<APVTS::SliderAttachment> (proc.apvts, k.id, kn->s);
            addAndMakeVisible (kn->s);
            addAndMakeVisible (kn->l);
            knobs.push_back (std::move (kn));
        }

        // Reihenfolge = G2Processor::tabIds
        const char* labels[] = { "Natural Amp", "Fuzz Tone", "Color Tones 1", "Color Tones 2", "Wow Wow", "Echo Repeat",
                                 "Bongo", "Brush", "Clave", "Tambourine", "String Bass" };
        const juce::uint32 dots[] = { 0xfff9b21b, 0xfff9b21b, 0xffffffff, 0xffffffff, 0xff0b0b0a, 0xff0b0b0a,
                                      0xffe4476a, 0xffe4476a, 0xffe4476a, 0xffe4476a, 0xff4c88ca };
        for (int i = 0; i < G2Processor::tabIds.size(); ++i)
        {
            auto t = std::make_unique<Tab>();
            t->b.setButtonText (labels[i]);
            t->b.setClickingTogglesState (true);
            t->b.getProperties().set ("dot", (int) dots[i]);
            t->a = std::make_unique<APVTS::ButtonAttachment> (proc.apvts, G2Processor::tabIds[i], t->b);
            addAndMakeVisible (t->b);
            tabs.push_back (std::move (t));
        }

        cancel.setButtonText ("Cancel");
        cancel.getProperties().set ("secondary", true);
        cancel.onClick = [this]
        {
            for (auto& id : G2Processor::tabIds)
                if (auto* prm = proc.apvts.getParameter (id))
                {
                    prm->beginChangeGesture();
                    prm->setValueNotifyingHost (0.f);
                    prm->endChangeGesture();
                }
        };
        addAndMakeVisible (cancel);
        setSize (1040, 580);
    }

    ~G2Editor() override { setLookAndFeel (nullptr); }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colours::white);

        // Hero "Shore" (vertikaler Verlauf) mit Korn
        auto hero = heroArea.toFloat();
        juce::ColourGradient grad (juce::Colour (0xff3d86a8), 0.f, hero.getY(), juce::Colour (0xfff4b49c), 0.f, hero.getBottom(), false);
        grad.addColour (0.34, juce::Colour (0xff9fd3d9));
        grad.addColour (0.56, juce::Colour (0xffeef4f0));
        grad.addColour (0.76, juce::Colour (0xfff5a58e));
        grad.addColour (0.90, juce::Colour (0xffee6f5f));
        g.setGradientFill (grad);
        g.fillRoundedRectangle (hero, 10.f);
        {
            juce::Graphics::ScopedSaveState s (g);
            juce::Path clip; clip.addRoundedRectangle (hero, 10.f);
            g.reduceClipRegion (clip);
            g.setTiledImageFill (grain, 0, 0, 0.16f);
            g.fillRect (hero);
        }

        // Display: klein geschrieben, leicht
        g.setColour (aq::ink);
        g.setFont (juce::Font (juce::FontOptions (46.f)));
        g.drawFittedText ("maestro\ng-2", heroArea.reduced (28).withTrimmedRight (30), juce::Justification::centredLeft, 2);
        g.setFont (juce::Font (juce::FontOptions (15.f)));
        g.drawFittedText ("rhythm 'n sound\nnach dem service manual", heroArea.reduced (28).removeFromBottom (60),
                          juce::Justification::bottomLeft, 2);

        // Kurzlabel senkrecht (nur Dekor)
        {
            juce::Graphics::ScopedSaveState s (g);
            const auto lab = juce::String ("RHYTHM 'N SOUND FOR GUITAR");
            g.setFont (juce::Font (juce::FontOptions (12.f).withKerningFactor (0.2f)));
            const float x = (float) heroArea.getRight() - 40.f, y = (float) heroArea.getY() + 24.f;
            g.addTransform (juce::AffineTransform::rotation (juce::MathConstants<float>::halfPi).translated (x + 16.f, y));
            g.drawText (lab, 0, 0, 260, 16, juce::Justification::centredLeft);
        }

        // Gruppen-Beschriftungen (Kurzlabel: 12 px, Versalien, weit gesperrt)
        g.setColour (aq::muted);
        g.setFont (juce::Font (juce::FontOptions (12.f).withKerningFactor (0.2f)));
        for (auto& c : captions) g.drawText (c.first, c.second, juce::Justification::centredLeft);
    }

    void resized() override
    {
        auto r = getLocalBounds().reduced (20);
        heroArea = r.removeFromLeft (210);
        r.removeFromLeft (28);

        // Regler
        auto top = r.removeFromTop (190);
        const int kw = top.getWidth() / (int) knobs.size();
        for (auto& k : knobs)
        {
            auto c = top.removeFromLeft (kw);
            k->l.setBounds (c.removeFromTop (22));
            k->s.setBounds (c);
        }
        r.removeFromTop (14);

        // Tab-Chips in Gruppen (jede Zielflaeche >= 44 px hoch, 8 px Abstand)
        captions.clear();
        constexpr int cw = 120, ch = 48, gap = 8, grp = 26;
        auto rowA = r.removeFromTop (ch + 24);
        auto rowB = r.removeFromTop (ch + 24).withTrimmedTop (16);
        auto place = [&] (juce::Rectangle<int>& row, const juce::String& cap, std::initializer_list<int> ids)
        {
            const int w = (int) ids.size() * cw + ((int) ids.size() - 1) * gap;
            auto g = row.removeFromLeft (w);
            captions.push_back ({ cap, g.removeFromTop (20) });
            auto line = g.removeFromTop (ch);
            for (int id : ids) { tabs[(size_t) id]->b.setBounds (line.removeFromLeft (cw)); line.removeFromLeft (gap); }
            row.removeFromLeft (grp);
        };
        place (rowA, "KLANG", { 0, 1 });
        place (rowA, "FILTER", { 2, 3, 4, 5 });
        place (rowB, "PERCUSSION", { 6, 7, 8, 9 });
        place (rowB, "BASS", { 10 });
        rowB.removeFromTop (20);
        cancel.setBounds (rowB.removeFromLeft (cw).withHeight (ch));
    }

private:
    struct Knob { juce::Slider s; juce::Label l; std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> a; };
    struct Tab  { juce::TextButton b; std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> a; };

    aq::Laf laf;
    G2Processor& proc;
    juce::Image grain;
    juce::Rectangle<int> heroArea;
    std::vector<std::pair<juce::String, juce::Rectangle<int>>> captions;
    std::vector<std::unique_ptr<Knob>> knobs;
    std::vector<std::unique_ptr<Tab>> tabs;
    juce::TextButton cancel;
};
