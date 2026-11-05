// The display: the Meter and Home screens, the MENU tree with its settings screens, the EFFECT screen, the song
// list, the recorder's screens (AUTO PUNCH, VAMP, TRACK EDIT), SD PLAY, pop-up messages (p.26-73).

#include "Desk.h"

namespace trackdesk
{
using F = Lcd::Font;

namespace
{
    constexpr int kRowTop = 11, kRowH = 11, kRows = 4, kBarY = 55;
    const juce::StringArray kOffOn { "OFF", "ON" };

    juce::String two (int v) { return juce::String (v).paddedLeft ('0', 2); }

    const char* kPalette = " !#$%&'()+,-.;=@[]^_{}~0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz";
    constexpr int kPaletteCols = 20;
} // namespace

// ======================================================================== the stack

void Desk::push (Screen s)
{
    stack.push_back (std::move (s));
    sdMode = false;
    for (auto& x : stack)
        if (x.id == ScreenId::SdPlay || x.id == ScreenId::SdPlaying)
            sdMode = true;
}

void Desk::pop()
{
    if (stack.size() > 1)
        stack.pop_back();
    else
        openHome();
    bool sd = false;
    for (auto& x : stack)
        if (x.id == ScreenId::SdPlay || x.id == ScreenId::SdPlaying)
            sd = true;
    if (sdMode && ! sd)
        sdPlayStop();
    sdMode = sd;
}

void Desk::openHome()
{
    if (sdMode)
        sdPlayStop();
    sdMode = false;
    stack.clear();
    Screen s;
    s.id = ScreenId::Home;
    s.title = "HOME";
    stack.push_back (s);
}

void Desk::openMeter()
{
    if (sdMode)
        sdPlayStop();
    sdMode = false;
    stack.clear();
    Screen s;
    s.id = ScreenId::Meter;
    s.title = "METER";
    stack.push_back (s);
}

Desk::ScreenId Desk::screen() const { return top().id; }

juce::String Desk::screenTitle() const { return top().title; }

juce::String Desk::cursorText() const
{
    const auto& s = top();
    switch (s.id)
    {
        case ScreenId::List: return s.cursor < (int) s.items.size() ? s.items[(size_t) s.cursor].label : juce::String();
        case ScreenId::Choice: return s.options[s.value];
        case ScreenId::Song: return s.cursor < (int) songCache.size() ? songCache[(size_t) s.cursor].name : juce::String();
        case ScreenId::Import:
        case ScreenId::SdPlay: return s.cursor < (int) s.entries.size() ? s.entries[(size_t) s.cursor].name : juce::String();
        case ScreenId::TrackClear: return trackName (s.cursor);
        case ScreenId::SendPoint:
        case ScreenId::Phase: return stripName (s.cursor);
        default: return {};
    }
}

bool Desk::vampScreenOpen() const
{
    for (auto& s : stack)
        if (s.id == ScreenId::VampEdit || s.id == ScreenId::VampPlay)
            return true;
    return false;
}

// ======================================================================== generic screens

void Desk::listScreen (const juce::String& title, std::vector<Screen::Item> items)
{
    Screen s;
    s.id = ScreenId::List;
    s.title = title;
    s.items = std::move (items);
    push (std::move (s));
}

void Desk::choiceScreen (const juce::String& title, const juce::StringArray& options, std::function<int()> get, std::function<void (int)> set,
                         bool live)
{
    Screen s;
    s.id = ScreenId::Choice;
    s.title = title;
    s.options = options;
    s.get = std::move (get);
    s.set = std::move (set);
    s.live = live;
    s.value = s.original = juce::jlimit (0, options.size() - 1, s.get());
    push (std::move (s));
}

void Desk::numberScreen (const juce::String& title, int lo, int hi, std::function<int()> get, std::function<void (int)> set,
                         std::function<juce::String (int)> format, bool live, int step)
{
    Screen s;
    s.id = ScreenId::Number;
    s.title = title;
    s.minimum = lo;
    s.maximum = hi;
    s.step = step;
    s.get = std::move (get);
    s.set = std::move (set);
    s.format = std::move (format);
    s.live = live;
    s.value = s.original = juce::jlimit (lo, hi, s.get());
    push (std::move (s));
}

void Desk::confirmScreen (const juce::String& title, const juce::StringArray& lines, std::function<void()> action)
{
    Screen s;
    s.id = ScreenId::Confirm;
    s.title = title;
    s.lines = lines;
    s.action = std::move (action);
    push (std::move (s));
}

void Desk::textScreen (const juce::String& title, const juce::String& text, int maxLength, std::function<void (const juce::String&)> done)
{
    Screen s;
    s.id = ScreenId::TextEdit;
    s.title = title;
    s.text = text.substring (0, maxLength);
    s.maximum = maxLength;
    s.cursor = std::min (s.text.length(), maxLength - 1);
    s.value = 0; // the palette position
    s.done = std::move (done);
    push (std::move (s));
}

// ======================================================================== the menus

void Desk::openMenu()
{
    auto sub = [] (const juce::String& label, std::function<void()> open) { return Screen::Item { label, {}, std::move (open) }; };
    listScreen ("MENU", {
                            sub ("SONG", [this] { openSongList(); }),
                            sub ("VAMP", [this] { openVampMenu(); }),
                            sub ("CLICK", [this] { openClickMenu(); }),
                            sub ("MTR", [this] { openMtrMenu(); }),
                            sub ("MIDI", [this] { openMidiMenu(); }),
                            sub ("MIXER", [this] { openMixerMenu(); }),
                            sub ("STEREO MIX EXPORT",
                                 [this] {
                                     Screen s;
                                     s.id = ScreenId::MixExport;
                                     s.title = "STEREO MIX EXPORT";
                                     push (s);
                                 }),
                            sub ("SD PLAY", [this] { openMusicBrowser (ScreenId::SdPlay, {}); }),
                            sub ("STORAGE",
                                 [this] {
                                     Screen s;
                                     s.id = ScreenId::StorageMode;
                                     s.title = "STORAGE";
                                     push (s);
                                 }),
                            sub ("DAW CONTROLLER", [this] { message ("DAW CONTROL", "not available"); }),
                            sub ("SYSTEM", [this] { openSystemMenu(); }),
                        });
}

void Desk::openVampMenu()
{
    listScreen ("VAMP", {
                            { "VAMP OFF", {},
                              [this] {
                                  // disables the VAMP function and opens the Home Screen (p.59)
                                  recorder.info.vampOn = false;
                                  recorder.leaveVamp();
                                  recorder.markDirty();
                                  openHome();
                              } },
                            { "I/O EDIT", {},
                              [this] {
                                  Screen s;
                                  s.id = ScreenId::VampEdit;
                                  s.title = "VAMP EDIT";
                                  push (s);
                              } },
                            { "I/O ALL CLR", {},
                              [this] {
                                  confirmScreen ("I/O ALL CLR", { "Clear all VAMP", "points?" }, [this] {
                                      recorder.info.vamps.clear();
                                      recorder.leaveVamp();
                                      recorder.markDirty();
                                      pop();
                                  });
                              } },
                            { "VAMP PLAY", {},
                              [this] {
                                  Screen s;
                                  s.id = ScreenId::VampPlay;
                                  s.title = "VAMP PLAY";
                                  push (s);
                              } },
                        });
}

void Desk::openClickMenu()
{
    auto& mt = recorder.info.metronome;
    auto dirty = [this] { recorder.markDirty(); };
    auto volume = [this, dirty] (const juce::String& title, int* v) {
        numberScreen (title, 0, 50, [v] { return *v; }, [v, dirty] (int x) { *v = x; dirty(); }, [] (int x) { return juce::String (x); }, true);
    };
    auto routeMenu = [this, &mt, dirty, volume] (const juce::String& title, int route) {
        listScreen (title, {
                               { "OUTPUT", [this, route] { return kOffOn[recorder.info.metronome.routeOn[(size_t) route] ? 1 : 0]; },
                                 [this, route, dirty] {
                                     choiceScreen ("OUTPUT", kOffOn, [this, route] { return recorder.info.metronome.routeOn[(size_t) route] ? 1 : 0; },
                                                   [this, route, dirty] (int v) {
                                                       recorder.info.metronome.routeOn[(size_t) route] = v == 1;
                                                       dirty();
                                                   });
                                 } },
                               { "VOLUME", [this, route] { return juce::String (recorder.info.metronome.routeVolume[(size_t) route]); },
                                 [this, route, volume] { volume ("VOLUME", &recorder.info.metronome.routeVolume[(size_t) route]); } },
                           });
        juce::ignoreUnused (mt);
    };
    listScreen ("CLICK", {
        { "SETTING", {},
          [this, routeMenu, volume, dirty] {
              listScreen ("CLICK SETTING", {
                  { "CLICK OUT", {},
                    [this, volume, dirty] {
                        listScreen ("CLICK OUT", {
                            { "MODE", [this] { return juce::StringArray { "OFF", "COUNT IN+REC", "REC", "REC&PLAY" }[(int) recorder.info.metronome.clickOut]; },
                              [this, dirty] {
                                  choiceScreen ("MODE", { "OFF", "COUNT IN+REC", "REC", "REC&PLAY" }, [this] { return (int) recorder.info.metronome.clickOut; },
                                                [this, dirty] (int v) {
                                                    recorder.info.metronome.clickOut = (ClickOut) v;
                                                    dirty();
                                                });
                              } },
                            { "VOLUME", [this] { return juce::String (recorder.info.metronome.clickVolume); },
                              [this, volume] { volume ("VOLUME", &recorder.info.metronome.clickVolume); } },
                        });
                    } },
                  { "PHONES1", {}, [routeMenu] { routeMenu ("PHONES1", 0); } },
                  { "PHONES2", {}, [routeMenu] { routeMenu ("PHONES2", 1); } },
                  { "SUB BUS", {}, [routeMenu] { routeMenu ("SUB BUS", 2); } },
                  { "MAIN BUS", {}, [routeMenu] { routeMenu ("MAIN BUS", 3); } },
                  { "AUX1", {}, [routeMenu] { routeMenu ("AUX1", 4); } },
                  { "AUX2", {}, [routeMenu] { routeMenu ("AUX2", 5); } },
              });
          } },
        { "METRONOME", {}, [this] { openMetronomeMenu(); } },
        { "COUNT IN", [this] { return juce::StringArray { "1 SIGNATURE", "2 SIGNATURES", "4 SIGNATURES", "2+4 BEAT", "2 BEAT", "3 BEAT", "4 BEAT" }[(int) recorder.info.metronome.countIn]; },
          [this, dirty] {
              choiceScreen ("COUNT IN", { "1 SIGNATURE", "2 SIGNATURES", "4 SIGNATURES", "2+4 BEAT", "2 BEAT", "3 BEAT", "4 BEAT" },
                            [this] { return (int) recorder.info.metronome.countIn; },
                            [this, dirty] (int v) {
                                recorder.info.metronome.countIn = (CountIn) v;
                                dirty();
                            });
          } },
    });
}

void Desk::openMetronomeMenu()
{
    auto dirty = [this] { recorder.markDirty(); };
    listScreen ("METRONOME", {
                  { "BAR DISP", [this] { return kOffOn[recorder.info.metronome.barDisplay ? 1 : 0]; },
                    [this, dirty] {
                        choiceScreen ("BAR DISP", kOffOn, [this] { return recorder.info.metronome.barDisplay ? 1 : 0; },
                                      [this, dirty] (int v) {
                                          recorder.info.metronome.barDisplay = v == 1;
                                          dirty();
                                      });
                    } },
                  { "TEMPO", [this] { return juce::String ((int) recorder.info.metronome.tempo); },
                    [this] {
                        Screen s;
                        s.id = ScreenId::Tempo;
                        s.title = "TEMPO";
                        s.value = s.original = (int) recorder.info.metronome.tempo;
                        taps.clear();
                        push (s);
                    } },
                  { "SIGNATURE", [this] { return juce::String (recorder.info.metronome.sigNum) + "/" + juce::String (recorder.info.metronome.sigDen); },
                    [this] {
                        Screen s;
                        s.id = ScreenId::Signature;
                        s.title = "SIGNATURE";
                        push (s);
                    } },
                  { "SOUND", [this] { return juce::String (clickSoundName (recorder.info.metronome.sound)); },
                    [this, dirty] {
                        juce::StringArray o;
                        for (int i = 0; i < 8; ++i)
                            o.add (clickSoundName ((ClickSound) i));
                        choiceScreen ("SOUND", o, [this] { return (int) recorder.info.metronome.sound; },
                                      [this, dirty] (int v) {
                                          recorder.info.metronome.sound = (ClickSound) v;
                                          dirty();
                                      }, true);
                    } },
                  { "ACCENT", [this] { return juce::StringArray { "OFF", "LOW", "MID", "HIGH" }[(int) recorder.info.metronome.accent]; },
                    [this, dirty] {
                        choiceScreen ("ACCENT", { "OFF", "LOW", "MID", "HIGH" }, [this] { return (int) recorder.info.metronome.accent; },
                                      [this, dirty] (int v) {
                                          recorder.info.metronome.accent = (Accent) v;
                                          dirty();
                                      }, true);
                    } },
              });
}

void Desk::openMtrMenu()
{
    listScreen ("MTR", {
        { "TRACK EDIT", {},
          [this] {
              listScreen ("TRACK EDIT", {
                  { "TRACK CLEAR", {},
                    [this] {
                        Screen s;
                        s.id = ScreenId::TrackClear;
                        s.title = "TRACK CLEAR";
                        push (s);
                    } },
                  { "IMPORT", {}, [this] { openMusicBrowser (ScreenId::Import, {}); } },
                  { "TRACK SWAP", {},
                    [this] {
                        Screen s;
                        s.id = ScreenId::TrackSwap;
                        s.title = "TRACK SWAP";
                        push (s);
                    } },
              });
          } },
        { "AUTO PUNCH", [this] { return kOffOn[recorder.info.autoPunch ? 1 : 0]; },
          [this] {
              Screen s;
              s.id = ScreenId::AutoPunch;
              s.title = "AUTO PUNCH";
              push (s);
          } },
        { "A.PUNCH PRE ROLL", [this] { return settings.preRollSec == 0 ? juce::String ("OFF") : juce::String (settings.preRollSec) + "sec"; },
          [this] {
              juce::StringArray o { "OFF" };
              for (int i = 1; i <= 10; ++i)
                  o.add (juce::String (i) + "sec.");
              choiceScreen ("A.PUNCH PRE ROLL", o, [this] { return settings.preRollSec; },
                            [this] (int v) {
                                settings.preRollSec = v;
                                recorder.preRollSec = v;
                                settingsChanged();
                            }, true);
          } },
    });
}

void Desk::openMidiMenu()
{
    listScreen ("MIDI", {
        { "MIDI TIME CODE", [this] { return kOffOn[settings.midiTimecode ? 1 : 0]; },
          [this] {
              choiceScreen ("MIDI TIME CODE", kOffOn, [this] { return settings.midiTimecode ? 1 : 0; },
                            [this] (int v) {
                                settings.midiTimecode = v == 1;
                                needFullFrame = true;
                                settingsChanged();
                            });
          } },
        { "MIDI CLOCK/SPP", [this] { return kOffOn[settings.midiClock ? 1 : 0]; },
          [this] {
              choiceScreen ("MIDI CLOCK/SPP", kOffOn, [this] { return settings.midiClock ? 1 : 0; },
                            [this] (int v) {
                                settings.midiClock = v == 1;
                                needSpp = true;
                                settingsChanged();
                            });
          } },
    });
}

void Desk::openMixerMenu()
{
    listScreen ("MIXER", {
        { "MTR/USB SEND POINT", {},
          [this] {
              Screen s;
              s.id = ScreenId::SendPoint;
              s.title = "MTR/USB SEND POINT";
              push (s);
          } },
        { "PHASE", {},
          [this] {
              Screen s;
              s.id = ScreenId::Phase;
              s.title = "PHASE";
              push (s);
          } },
        { "AUX1 SEND", [this] { return juce::String (settings.aux1PostFader ? "POST" : "PRE"); },
          [this] {
              choiceScreen ("AUX1 SEND", { "PRE FADER", "POST FADER" }, [this] { return settings.aux1PostFader ? 1 : 0; },
                            [this] (int v) {
                                settings.aux1PostFader = v == 1;
                                settingsChanged();
                            });
          } },
        { "SOLO MODE", [this] { return juce::String (settings.soloMode == SoloMode::Pfl ? "PFL" : "INPLACE"); },
          [this] {
              choiceScreen ("SOLO MODE", { "PFL", "INPLACE SOLO" }, [this] { return (int) settings.soloMode; },
                            [this] (int v) {
                                settings.soloMode = (SoloMode) v;
                                settingsChanged();
                            });
          } },
        { "PHONES MAIN SEL", [this] { return juce::String (settings.phonesMainPost ? "POST" : "PRE"); },
          [this] {
              choiceScreen ("PHONES MAIN SEL", { "MAIN MIX PRE", "MAIN MIX POST" }, [this] { return settings.phonesMainPost ? 1 : 0; },
                            [this] (int v) {
                                settings.phonesMainPost = v == 1;
                                settingsChanged();
                            });
          } },
    });
}

void Desk::openSystemMenu()
{
    auto changed = [this] { settingsChanged(); };
    listScreen ("SYSTEM", {
        { "INFORMATION", {},
          [this] {
              Screen s;
              s.id = ScreenId::Information;
              s.title = "INFORMATION";
              if (storage != nullptr)
                  songCache = storage->songs();
              push (s);
          } },
        { "DATE/TIME", {},
          [this] {
              Screen s;
              s.id = ScreenId::DateTime;
              s.title = "DATE/TIME";
              push (s);
          } },
        { "SONG NAME", [this] { return juce::String (settings.nameByDate ? "DATE" : "WORD"); },
          [this, changed] {
              std::vector<Screen::Item> items {
                  { "TYPE", [this] { return juce::String (settings.nameByDate ? "DATE" : "WORD"); },
                    [this, changed] {
                        choiceScreen ("TYPE", { "DATE", "WORD" }, [this] { return settings.nameByDate ? 0 : 1; },
                                      [this, changed] (int v) {
                                          settings.nameByDate = v == 0;
                                          changed();
                                      });
                    } },
                  { "EDIT", [this] { return settings.nameWord; },
                    [this, changed] {
                        if (settings.nameByDate)
                            return;
                        textScreen ("WORD EDIT", settings.nameWord, 6, [this, changed] (const juce::String& w) {
                            settings.nameWord = w.trim().isEmpty() ? juce::String ("SONG") : w.trim();
                            changed();
                        });
                    } },
              };
              listScreen ("SONG NAME", items);
          } },
        { "DISPLAY", {},
          [this] {
              Screen s;
              s.id = ScreenId::Display;
              s.title = "DISPLAY";
              push (s);
          } },
        { "KEY FUNCTION", [this] { return juce::String (settings.f4 == F4Fn::Mark ? "MARK" : "CLICK"); },
          [this, changed] {
              listScreen ("KEY FUNCTION", { { "F4 KEY", [this] { return juce::String (settings.f4 == F4Fn::Mark ? "MARK" : "CLICK"); },
                                              [this, changed] {
                                                  choiceScreen ("F4 KEY", { "MARK", "CLICK" }, [this] { return (int) settings.f4; },
                                                                [this, changed] (int v) {
                                                                    settings.f4 = (F4Fn) v;
                                                                    changed();
                                                                });
                                              } } });
          } },
        { "FOOTSW", {},
          [this, changed] {
              auto foot = [this, changed] (int i) {
                  juce::StringArray fns;
                  for (int f = 0; f <= (int) FootFn::VampIOSet; ++f)
                      fns.add (footFnName ((FootFn) f));
                  listScreen ("FOOTSW" + juce::String (i + 1),
                              { { "MODE", [this, i] { return juce::String (footFnName (settings.footFn[(size_t) i])); },
                                  [this, i, fns, changed] {
                                      choiceScreen ("MODE", fns, [this, i] { return (int) settings.footFn[(size_t) i]; },
                                                    [this, i, changed] (int v) {
                                                        settings.footFn[(size_t) i] = (FootFn) v;
                                                        changed();
                                                    });
                                  } },
                                { "POLARITY", [this, i] { return juce::String (settings.footNormallyClosed[(size_t) i] ? "N.CLOSE" : "N.OPEN"); },
                                  [this, i, changed] {
                                      choiceScreen ("POLARITY", { "NORMALLY OPEN", "NORMALLY CLOSED" },
                                                    [this, i] { return settings.footNormallyClosed[(size_t) i] ? 1 : 0; },
                                                    [this, i, changed] (int v) {
                                                        settings.footNormallyClosed[(size_t) i] = v == 1;
                                                        changed();
                                                    }, true);
                                  } } });
              };
              listScreen ("FOOTSW", { { "FOOTSW1", [this] { return juce::String (footFnName (settings.footFn[0])); }, [foot] { foot (0); } },
                                      { "FOOTSW2", [this] { return juce::String (footFnName (settings.footFn[1])); }, [foot] { foot (1); } } });
          } },
        { "USB AUDIO", [this] { return juce::String (settings.usbStereoMix ? "STEREO MIX" : "MULTI INPUT"); },
          [this, changed] {
              choiceScreen ("USB AUDIO", { "MULTI INPUT", "STEREO MIX" }, [this] { return settings.usbStereoMix ? 1 : 0; },
                            [this, changed] (int v) {
                                settings.usbStereoMix = v == 1;
                                changed();
                            });
          } },
        { "OUTPUT DELAY", [this] { return juce::String (settings.outputDelayOn ? "ENABLE" : "DISABLE"); },
          [this, changed] {
              listScreen ("OUTPUT DELAY", {
                  { "MAIN OUTPUT", [this] { return kOffOn[settings.outputDelayOnMain ? 1 : 0]; },
                    [this, changed] {
                        choiceScreen ("MAIN OUTPUT", kOffOn, [this] { return settings.outputDelayOnMain ? 1 : 0; },
                                      [this, changed] (int v) {
                                          settings.outputDelayOnMain = v == 1;
                                          changed();
                                      });
                    } },
                  { "DELAY", [this] { return juce::String (settings.outputDelayOn ? "ENABLE" : "DISABLE"); },
                    [this, changed] {
                        choiceScreen ("DELAY", { "DISABLE", "ENABLE" }, [this] { return settings.outputDelayOn ? 1 : 0; },
                                      [this, changed] (int v) {
                                          settings.outputDelayOn = v == 1;
                                          changed();
                                      });
                    } },
                  { "DELAY TIME", [this] { return juce::String (settings.outputDelayMs) + "ms"; },
                    [this] {
                        Screen s;
                        s.id = ScreenId::DelayTime;
                        s.title = "DELAY TIME";
                        s.value = s.original = settings.outputDelayMs;
                        s.step = 1;
                        push (s);
                    } },
              });
          } },
        { "OUTPUT PAD", {},
          [this, changed] {
              auto pad = [this, changed] (const juce::String& title, int* v) {
                  numberScreen (title, -40, 0, [v] { return *v; },
                                [v, changed] (int x) {
                                    *v = x;
                                    changed();
                                },
                                [] (int x) { return juce::String (x) + "dB"; }, true);
              };
              listScreen ("OUTPUT PAD", { { "MAIN OUT", [this] { return juce::String (settings.padMainDb) + "dB"; }, [this, pad] { pad ("MAIN OUT", &settings.padMainDb); } },
                                          { "SUB OUT", [this] { return juce::String (settings.padSubDb) + "dB"; }, [this, pad] { pad ("SUB OUT", &settings.padSubDb); } } });
          } },
        { "INITIALIZE", {},
          [this] {
              confirmScreen ("INITIALIZE", { "Restore factory", "settings?" }, [this] {
                  settings = Settings();
                  const auto lib = fx.library;
                  fx = FxSettings();
                  fx.library = lib;
                  recorder.vampAuto = settings.vampAuto;
                  settingsChanged();
                  pop();
              });
          } },
        { "MEDIA FORMAT", {},
          [this] {
              listScreen ("MEDIA FORMAT", {
                  { "QUICK", {},
                    [this] {
                        confirmScreen ("QUICK", { "Format card?", "All data erased" }, [this] {
                            if (storage != nullptr && storage->format())
                            {
                                loaded = false;
                                recorder.clearSong();
                                startUp ({});
                            }
                            openHome();
                        });
                    } },
                  { "ERASE", {},
                    [this] {
                        confirmScreen ("ERASE", { "Erase card?", "All data erased" }, [this] {
                            if (storage != nullptr && storage->format())
                            {
                                loaded = false;
                                recorder.clearSong();
                                startUp ({});
                            }
                            openHome();
                        });
                    } },
              });
          } },
    });
}

void Desk::openSongList()
{
    if (! stopped())
        return;
    Screen s;
    s.id = ScreenId::Song;
    s.title = "SONG";
    if (storage != nullptr)
        songCache = storage->songs();
    for (size_t i = 0; i < songCache.size(); ++i)
        if (songCache[i].name == recorder.info.name)
            s.cursor = (int) i;
    push (s);
}

void Desk::openSongPopup (int index)
{
    if (index < 0 || index >= (int) songCache.size())
        return;
    const auto song = songCache[(size_t) index];
    const bool current = loaded && song.name == recorder.info.name;
    std::vector<Screen::Item> items;
    items.push_back ({ current ? "SAVE" : "LOAD", {}, [this, song, current] {
                          if (current)
                          {
                              saveSong();
                              recorder.forgetUndo(); // (p.36)
                          }
                          else
                              loadSong (song.name);
                          pop();
                          songCache = storage != nullptr ? storage->songs() : songCache;
                      } });
    items.push_back ({ "INFORMATION", {}, [this, song] {
                          Screen s;
                          s.id = ScreenId::SongInfo;
                          s.title = "INFORMATION";
                          s.text = song.name;
                          push (s);
                      } });
    items.push_back ({ "CLR ALL MARKS", {}, [this, song, current] {
                          confirmScreen ("CLR ALL MARKS", { song.name, "Clear all marks?" }, [this, song, current] {
                              if (current)
                              {
                                  recorder.info.marks.clear();
                                  saveSong();
                              }
                              else if (storage != nullptr)
                                  storage->clearMarks (song.name);
                              pop();
                              pop();
                          });
                      } });
    items.push_back ({ "DELETE", {}, [this, song, current] {
                          if (current)
                          {
                              message ("Current Song");
                              return;
                          }
                          if (song.protect)
                          {
                              message ("Song Protected");
                              return;
                          }
                          confirmScreen ("DELETE", { song.name, "Delete song?" }, [this, song] {
                              if (storage != nullptr)
                                  storage->deleteSong (song.name);
                              songCache = storage != nullptr ? storage->songs() : songCache;
                              pop();
                              pop();
                              if (top().id == ScreenId::Song)
                                  top().cursor = juce::jlimit (0, std::max (0, (int) songCache.size() - 1), top().cursor);
                          });
                      } });
    items.push_back ({ "PROTECT", {}, [this, song, current] {
                          if (song.protect)
                          {
                              message ("Already protected");
                              return;
                          }
                          confirmScreen ("PROTECT", { song.name, "Protect song?" }, [this, song, current] {
                              if (storage != nullptr)
                                  storage->setProtected (song.name, true);
                              if (current)
                                  recorder.info.protect = true;
                              songCache = storage != nullptr ? storage->songs() : songCache;
                              pop();
                              pop();
                          });
                      } });
    items.push_back ({ "UNPROTECT", {}, [this, song, current] {
                          if (! song.protect)
                          {
                              message ("Already unprotected");
                              return;
                          }
                          confirmScreen ("UNPROTECT", { song.name, "Unprotect song?" }, [this, song, current] {
                              if (storage != nullptr)
                                  storage->setProtected (song.name, false);
                              if (current)
                                  recorder.info.protect = false;
                              songCache = storage != nullptr ? storage->songs() : songCache;
                              pop();
                              pop();
                          });
                      } });
    items.push_back ({ "RENAME", {}, [this, song, current] {
                          if (song.protect)
                          {
                              message ("Song Protected");
                              return;
                          }
                          textScreen ("RENAME", song.name, 11, [this, song, current] (const juce::String& n) {
                              // when it's done, the SONG Screen reopens (p.37)
                              if (top().id == ScreenId::List)
                                  pop();
                              const auto name = n.trim();
                              if (name.isEmpty() || name == song.name)
                                  return;
                              for (auto& s : songCache)
                                  if (s.name == name)
                                  {
                                      message ("Dup File Name");
                                      return;
                                  }
                              if (storage != nullptr && storage->renameSong (song.name, name))
                              {
                                  if (current)
                                      recorder.info.name = name;
                                  songCache = storage->songs();
                                  settingsChanged();
                              }
                          });
                      } });
    listScreen (song.name, items);
}

void Desk::openMusicBrowser (ScreenId id, const juce::String& folder)
{
    if (! stopped())
        return;
    Screen s;
    s.id = id;
    s.title = id == ScreenId::SdPlay ? "SD PLAY" : "TRACK IMPORT";
    s.folder = folder;
    if (storage != nullptr)
        s.entries = storage->music (folder);
    if (s.entries.empty() && folder.isEmpty())
    {
        message ("No File");
        if (id != ScreenId::SdPlay)
            return;
    }
    push (s);
}

void Desk::openEffect()
{
    if (top().id == ScreenId::Effect)
        return;
    Screen s;
    s.id = ScreenId::Effect;
    s.title = "EFFECT";
    s.field = 0;
    fxTurned = false;
    taps.clear();
    push (s);
}

// ======================================================================== keys on the screens

void Desk::screenKey (Key k)
{
    auto& s = top();
    if (k == Key::Menu)
    {
        // Meter -> Home -> MENU; anywhere else back to Home (p.23)
        if (s.id == ScreenId::Meter)
            openHome();
        else if (s.id == ScreenId::Home && s.field == 0)
        {
            if (stopped())
                openMenu();
        }
        else
            openHome();
        return;
    }
    const bool rec = recorder.isRecording();
    switch (s.id)
    {
        case ScreenId::Meter:
            if (k == Key::F1)
                s.param = (s.param + 1) % 4; // SEL: channel inputs, LIVE INPUT, PC RETURN, MTR RETURN
            else if (k == Key::F2)
                s.field = 0;
            else if (k == Key::F3)
                s.field = 1;
            return;

        case ScreenId::Home:
            if (s.field > 0) // direct locate (p.43)
            {
                if (k == Key::Jog)
                {
                    if (++s.field > 3)
                    {
                        recorder.locate ((int64_t) ((double) s.value * sampleRate));
                        needSpp = needFullFrame = true;
                        s.field = 0;
                    }
                }
                else if (k == Key::F1)
                    s.field = 0;
                return;
            }
            if (k == Key::F1)
                openMeter();
            else if (k == Key::F2)
            {
                recorder.repeat = (Repeat) (((int) recorder.repeat + 1) % 3);
            }
            else if (k == Key::F3 && stopped())
            {
                if (recorder.canUndo())
                    confirm ({ "Undo last", "operation?" }, [this] {
                        recorder.undo();
                        saveSong();
                    });
                else if (recorder.canRedo())
                    confirm ({ "Redo last", "operation?" }, [this] {
                        recorder.redo();
                        saveSong();
                    });
            }
            else if (k == Key::F4)
            {
                if (settings.f4 == F4Fn::Mark)
                    addOrClearMark();
                else if (stopped())
                    openMetronomeMenu(); // (F4 set to CLICK: p.52)
            }
            else if (k == Key::Jog && stopped() && recorder.getState() == Recorder::State::Stopped)
            {
                s.field = 1;
                s.value = (int) ((double) recorder.getPosition() / sampleRate);
            }
            return;

        case ScreenId::List:
            if (k == Key::F1)
                pop();
            else if (k == Key::Jog && s.cursor < (int) s.items.size() && s.items[(size_t) s.cursor].open)
            {
                if (rec)
                    return;
                auto open = s.items[(size_t) s.cursor].open;
                open();
            }
            return;

        case ScreenId::Choice:
        case ScreenId::Number:
            if (k == Key::F1)
                pop(); // (cancelled, unless it applied as it was turned)
            else if (k == Key::Jog)
            {
                if (s.set)
                    s.set (s.value);
                pop();
            }
            return;

        case ScreenId::Confirm:
            if (k == Key::F1)
                pop();
            else if (k == Key::F4 || (k == Key::F3 && s.param == 1))
            {
                auto fn = s.action;
                if (fn)
                    fn();
                else
                    pop();
            }
            return;

        case ScreenId::TextEdit:
        {
            const juce::String pal (kPalette);
            if (k == Key::F1)
                pop();
            else if (k == Key::F4)
            {
                auto done = s.done;
                const auto text = s.text.trimEnd();
                pop();
                if (done)
                    done (text);
            }
            else if (k == Key::F2)
                s.cursor = std::max (0, s.cursor - 1);
            else if (k == Key::F3)
                s.cursor = std::min ({ s.maximum - 1, s.text.length(), s.cursor + 1 });
            else if (k == Key::Jog)
            {
                if (s.value >= pal.length()) // DEL
                {
                    if (s.cursor < s.text.length())
                        s.text = s.text.substring (0, s.cursor) + s.text.substring (s.cursor + 1);
                    else if (s.text.isNotEmpty())
                    {
                        s.text = s.text.dropLastCharacters (1);
                        s.cursor = std::max (0, s.cursor - 1);
                    }
                    return;
                }
                const auto c = pal.substring (s.value, s.value + 1);
                s.text = (s.text.substring (0, s.cursor) + c + s.text.substring (s.cursor + 1)).substring (0, s.maximum);
                s.cursor = std::min (s.cursor + 1, s.maximum - 1);
            }
            return;
        }

        case ScreenId::Effect:
            if (k == Key::F1)
                openHome(); // (p.42)
            else if (k == Key::F2)
            {
                if (aux2JackUsed)
                    return;
                fx.on = ! fx.on;
                settingsChanged();
            }
            else if (k == Key::F4)
            {
                // UNDO after turning the dial, otherwise DEFAULT (p.42)
                if (fxTurned && fxUndoValue >= 0.0f)
                {
                    if (s.field == 0)
                        fx.type = (int) fxUndoValue;
                    else
                        fx.param[(size_t) fx.type] = fxUndoValue;
                    fxTurned = false;
                }
                else if (s.field == 1)
                    fx.param[(size_t) fx.type] = 0.5f;
                else if (s.field == 0)
                    fx.type = 0;
                updateLibrary();
                settingsChanged();
            }
            else if (k == Key::Jog)
            {
                s.field = (s.field + 1) % 3; // TYPE, PARAMETER, LIB
                fxTurned = false;
                taps.clear();
            }
            return;

        case ScreenId::Song:
            if (k == Key::F1)
                pop();
            else if (k == Key::F2)
                s.param = s.param == 1 ? 0 : 1; // dates
            else if (k == Key::F3)
                s.param = s.param == 2 ? 0 : 2; // sizes
            else if (k == Key::F4)
            {
                if ((int) songCache.size() >= 100)
                {
                    message ("Song number full");
                    return;
                }
                Screen n;
                n.id = ScreenId::SongNew;
                n.title = "NEW SONG";
                n.value = sampleRate > 46000.0 ? 2 : 0; // 44.1kHz-16bit (the default), at the device's rate
                n.text = storage != nullptr ? storage->newSongName (settings) : juce::String ("SONG");
                push (n);
            }
            else if (k == Key::Jog)
                openSongPopup (s.cursor);
            return;

        case ScreenId::SongNew:
            if (k == Key::F1)
                pop();
            else if (k == Key::F2)
            {
                textScreen ("NAME EDIT", s.text, 11, [this] (const juce::String& t) {
                    if (top().id == ScreenId::SongNew && t.isNotEmpty())
                        top().text = t;
                });
            }
            else if (k == Key::F4)
            {
                const double rate = s.value >= 2 ? 48000.0 : 44100.0;
                const int bits = s.value % 2 == 0 ? 16 : 24;
                for (auto& e : songCache)
                    if (e.name == s.text)
                    {
                        message ("Dup File Name");
                        return;
                    }
                const auto name = s.text;
                newSong (rate, bits, name);
                pop();
                if (top().id == ScreenId::Song)
                {
                    songCache = storage != nullptr ? storage->songs() : songCache;
                    for (size_t i = 0; i < songCache.size(); ++i)
                        if (songCache[i].name == name)
                            top().cursor = (int) i;
                }
            }
            return;

        case ScreenId::SongInfo:
        case ScreenId::Information:
        case ScreenId::DateTime:
            if (k == Key::F1 || k == Key::F4)
                pop();
            return;

        case ScreenId::AutoPunch:
        {
            auto& info = recorder.info;
            const auto pos = recorder.getPosition();
            if (k == Key::F1)
                pop();
            else if (k == Key::F2)
            {
                info.punchIn = info.punchOut = -1;
                info.autoPunch = false;
                s.field = 0;
                recorder.markDirty();
            }
            else if (k == Key::F3)
                s.field = 1 - s.field;
            else if (k == Key::F4)
            {
                if (! info.autoPunch && info.punchIn < 0 && info.punchOut < 0)
                {
                    message ("Need to set", "I/O point.");
                    return;
                }
                info.autoPunch = ! info.autoPunch;
                recorder.markDirty();
            }
            else if (k == Key::Jog && ! rec)
            {
                auto& point = s.field == 0 ? info.punchIn : info.punchOut;
                const auto other = s.field == 0 ? info.punchOut : info.punchIn;
                if (other >= 0 && std::abs (pos - other) < (int64_t) sampleRate)
                {
                    message ("I/O Too Short");
                    return;
                }
                if ((s.field == 0 && other >= 0 && pos > other) || (s.field == 1 && other >= 0 && pos < other))
                {
                    message ("Invalid I/O point");
                    return;
                }
                point = pos;
                s.field = 1;
                recorder.markDirty();
            }
            return;
        }

        case ScreenId::VampEdit:
        {
            auto& v = recorder.info.vamps;
            const auto pos = recorder.getPosition();
            const auto st = recorder.getState();
            if (k == Key::F1)
            {
                s.value = -1;
                pop();
            }
            else if (k == Key::F4)
            {
                recorder.info.vampOn = ! recorder.info.vampOn;
                recorder.markDirty();
            }
            else if (k == Key::F2 || k == Key::F3)
            {
                // on an existing point (stopped or paused): clear that pair (p.58)
                if (st != Recorder::State::Playing)
                    for (size_t i = 0; i < v.size(); ++i)
                        if ((k == Key::F2 && v[i].first == pos) || (k == Key::F3 && v[i].second == pos))
                        {
                            v.erase (v.begin() + (long) i);
                            recorder.leaveVamp();
                            recorder.markDirty();
                            return;
                        }
                if (st != Recorder::State::Playing && st != Recorder::State::Paused)
                    return;
                if (k == Key::F2)
                {
                    if ((int) v.size() >= kMaxVamps)
                    {
                        message ("VAMP full");
                        return;
                    }
                    s.value = (int) (pos / 16); // (pending in point, in 16-sample units to fit an int)
                    s.param = 1;
                    s.minimum = (int) (pos & 15);
                }
                else if (s.param == 1)
                {
                    const int64_t in = (int64_t) s.value * 16 + s.minimum;
                    if (pos - in < (int64_t) (2.0 * sampleRate))
                    {
                        message ("I/O Too Short");
                        return;
                    }
                    if (pos > recorder.songLength())
                    {
                        message ("Invalid I/O point");
                        return;
                    }
                    for (auto& r : v)
                        if (in < r.second && pos > r.first)
                        {
                            message ("Invalid I/O point"); // (ranges can't overlap)
                            return;
                        }
                    v.push_back ({ in, pos });
                    std::sort (v.begin(), v.end());
                    s.param = 0;
                    recorder.markDirty();
                }
            }
            return;
        }

        case ScreenId::VampPlay:
            if (k == Key::F1)
                openMeter();
            else if (k == Key::F2)
                recorder.repeat = (Repeat) (((int) recorder.repeat + 1) % 3);
            else if (k == Key::F3)
            {
                settings.vampAuto = ! settings.vampAuto;
                recorder.vampAuto = settings.vampAuto;
                settingsChanged();
            }
            else if (k == Key::F4)
            {
                recorder.vampToggle();
                recorder.markDirty();
            }
            return;

        case ScreenId::TrackClear:
            if (k == Key::F1)
                pop();
            else if (k == Key::F2)
                confirmScreen ("TRACK CLEAR", { "Clear all", "tracks?" }, [this] {
                    if (recorder.info.protect)
                        message ("Song Protected");
                    else
                    {
                        recorder.clearTrack (-1);
                        saveSong();
                    }
                    pop();
                });
            else if (k == Key::Jog)
            {
                const int t = s.cursor;
                confirmScreen ("TRACK CLEAR", { "Clear track " + trackName (t) + "?" }, [this, t] {
                    if (recorder.info.protect)
                        message ("Song Protected");
                    else
                    {
                        recorder.clearTrack (t);
                        saveSong();
                    }
                    pop();
                });
            }
            return;

        case ScreenId::Import:
        case ScreenId::SdPlay:
        {
            const bool sd = s.id == ScreenId::SdPlay;
            if (k == Key::F1)
            {
                pop();
                return;
            }
            if (k == Key::F2 && s.folder.isNotEmpty())
            {
                const auto up = s.folder.upToLastOccurrenceOf ("/", false, false);
                s.folder = up;
                s.entries = storage != nullptr ? storage->music (up) : std::vector<MusicEntry>();
                s.cursor = 0;
                return;
            }
            if (s.cursor >= (int) s.entries.size())
                return;
            const auto e = s.entries[(size_t) s.cursor];
            if (k == Key::Jog && e.folder)
            {
                s.folder = e.path;
                s.entries = storage != nullptr ? storage->music (e.path) : std::vector<MusicEntry>();
                s.cursor = 0;
                return;
            }
            const bool choose = k == Key::F4 || (sd && k == Key::Jog);
            if (! choose || e.folder)
                return;
            if (sd)
            {
                sdFiles.clear();
                int index = 0;
                for (auto& x : s.entries)
                    if (! x.folder)
                    {
                        if (x.path == e.path)
                            index = (int) sdFiles.size();
                        sdFiles.push_back (x);
                    }
                Screen p;
                p.id = ScreenId::SdPlaying;
                p.title = "SD PLAY";
                push (p);
                sdPlayStart (index);
                return;
            }
            // IMPORT: same sample rate, an empty track (p.46)
            if (std::abs (e.sampleRate - recorder.info.sampleRate) > 1.0)
            {
                message ("Sample rate", "Unmatch");
                return;
            }
            bool any = false;
            for (int t = 0; t < kTracks; ++t)
                any = any || ! recorder.hasAudio (t);
            if (! any)
            {
                message ("Import error.", "No track");
                return;
            }
            auto chooseTrack = [this, e] {
                Screen t;
                t.id = ScreenId::ImportTrack;
                t.title = "TRACK IMPORT";
                t.text = e.path;
                t.entries = { e };
                push (t);
            };
            if (e.bits != recorder.info.bits)
                confirmScreen ("TRACK IMPORT", { "Bit length differs.", "Convert to " + juce::String (recorder.info.bits) + "bit?" }, [this, chooseTrack] {
                    pop();
                    chooseTrack();
                });
            else
                chooseTrack();
            return;
        }

        case ScreenId::ImportTrack:
        {
            if (k == Key::F1)
            {
                pop();
                return;
            }
            if (k != Key::F4 && k != Key::Jog)
                return;
            if (recorder.info.protect)
            {
                message ("Song Protected");
                return;
            }
            // the choices: the empty tracks, and for a stereo file the empty pairs
            const auto e = s.entries.empty() ? MusicEntry() : s.entries.front();
            std::vector<std::pair<int, bool>> choices;
            for (int t = 0; t < kTracks; ++t)
                if (! recorder.hasAudio (t))
                    choices.push_back ({ t, false });
            if (e.channels > 1)
                for (int t : { 6, 8, 10 })
                    if (! recorder.hasAudio (t) && ! recorder.hasAudio (t + 1))
                        choices.push_back ({ t, true });
            if (s.cursor >= (int) choices.size() || storage == nullptr)
                return;
            const auto c = choices[(size_t) s.cursor];
            recorder.rememberForUndo();
            bool ok = true;
            for (int ch = 0; ch < (c.second ? 2 : 1); ++ch)
            {
                auto audio = storage->importAudio (recorder.info, e.path, c.second ? ch : (e.channels > 1 ? -1 : 0));
                ok = ok && audio != nullptr && recorder.importTrack (c.first + ch, audio);
            }
            if (! ok)
                message ("File Error");
            saveSong();
            pop();
            return;
        }

        case ScreenId::TrackSwap:
        {
            static const char* names[] = { "1", "2", "3", "4", "5", "6", "7", "8", "7/8", "9", "10", "9/10", "11", "12", "11/12" };
            static const int track[] = { 0, 1, 2, 3, 4, 5, 6, 7, 6, 8, 9, 8, 10, 11, 10 };
            static const bool pair[] = { false, false, false, false, false, false, false, false, true, false, false, true, false, false, true };
            juce::ignoreUnused (names);
            if (k == Key::F1)
                pop();
            else if (k == Key::Jog)
                s.field = 1 - s.field;
            else if (k == Key::F4)
            {
                const int a = s.value, b = s.param;
                if (a == b || pair[a] != pair[b] || (pair[a] && track[a] == track[b]))
                {
                    message ("Invalid track");
                    return;
                }
                if (pair[a] == false && pair[b] == false && track[a] == track[b])
                {
                    message ("Invalid track");
                    return;
                }
                if (recorder.info.protect)
                {
                    message ("Song Protected");
                    return;
                }
                recorder.swapTracks (track[a], track[b], pair[a]);
                saveSong();
            }
            return;
        }

        case ScreenId::MixExport:
            if (k == Key::F1)
                pop();
            else if (k == Key::F4 && storage != nullptr)
            {
                if (! recorder.hasAudio (kMixTrack) && ! recorder.hasAudio (kMixTrack + 1))
                {
                    message ("No MAIN MIX", "track recorded");
                    return;
                }
                auto run = [this] {
                    // when it's done, the MENU Screen reopens (p.48)
                    if (! storage->exportMix (recorder))
                        message ("Writing Failed");
                    while (stack.size() > 1 && top().title != "MENU")
                        pop();
                };
                if (storage->exportMixExists (recorder.info))
                {
                    confirmScreen ("STEREO MIX EXPORT", { "File exists.", "Overwrite? (F3)" }, [this, run] {
                        pop();
                        run();
                    });
                    top().param = 1; // (F3 confirms, as on the original)
                }
                else
                    run();
            }
            return;

        case ScreenId::SdPlaying:
            if (k == Key::F1)
            {
                sdRolling = false;
                pop(); // back to the file list
            }
            else if (k == Key::F2)
                sdRepeat = (sdRepeat + 1) % 3;
            else if (k == Key::F3)
                locateKey (-1);
            else if (k == Key::F4)
            {
                if (! sdRolling && sdIndex >= 0)
                    sdRolling = true, sdPaused = false;
                else
                    locateKey (1);
            }
            return;

        case ScreenId::StorageMode:
            if (k == Key::F1)
            {
                s.param = 0;
                openHome();
            }
            else if (k == Key::F4 && storage != nullptr)
            {
                s.param = 1;
                storage->reveal();
            }
            return;

        case ScreenId::Display:
            if (k == Key::F1)
                pop();
            else if (k == Key::Jog)
                s.field = 1 - s.field;
            return;

        case ScreenId::DelayTime:
            if (k == Key::F1)
            {
                settings.outputDelayMs = s.original;
                settingsChanged();
                pop();
            }
            else if (k == Key::F3)
                s.step = 1;
            else if (k == Key::F4)
                s.step = 100;
            else if (k == Key::Jog)
            {
                settingsChanged();
                pop();
            }
            return;

        case ScreenId::SendPoint:
        case ScreenId::Phase:
        {
            const bool phase = s.id == ScreenId::Phase;
            if (k == Key::F1)
                pop();
            else if (k == Key::Jog)
            {
                const int ch = s.cursor;
                if (phase)
                    choiceScreen ("PHASE " + stripName (ch), { "NORMAL", "INVERT" }, [this, ch] { return strips[(size_t) ch].phaseInvert ? 1 : 0; },
                                  [this, ch] (int v) {
                                      strips[(size_t) ch].phaseInvert = v == 1;
                                      settingsChanged();
                                  });
                else
                    choiceScreen ("SEND POINT " + stripName (ch), { "PRE COMP", "POST COMP", "POST EQ" },
                                  [this, ch] { return (int) strips[(size_t) ch].sendPoint; },
                                  [this, ch] (int v) {
                                      strips[(size_t) ch].sendPoint = (SendPoint) v;
                                      settingsChanged();
                                  });
            }
            else if (k == Key::F4 && ! phase)
                choiceScreen ("ALL CHANNELS", { "PRE COMP", "POST COMP", "POST EQ" }, [this] { return (int) strips[0].sendPoint; },
                              [this] (int v) {
                                  for (auto& c : strips)
                                      c.sendPoint = (SendPoint) v;
                                  settingsChanged();
                              });
            return;
        }

        case ScreenId::Tempo:
            if (k == Key::F1)
            {
                recorder.info.metronome.tempo = s.original;
                pop();
            }
            else if (k == Key::F4)
            {
                metroOn = ! metroOn; // the click on/off while setting the tempo (p.51)
                beatIndex = -1;
                freeBeatPhase = 0.0;
            }
            else if (k == Key::Jog)
            {
                recorder.markDirty();
                pop();
            }
            return;

        case ScreenId::Signature:
            if (k == Key::F1)
                pop();
            else if (k == Key::Jog)
            {
                if (++s.field > 1)
                {
                    recorder.markDirty();
                    pop();
                }
            }
            return;
    }
}

void Desk::screenJog (int d)
{
    auto& s = top();
    auto scrollTo = [&s] (int count) {
        s.cursor = juce::jlimit (0, std::max (0, count - 1), s.cursor);
        if (s.cursor < s.scroll)
            s.scroll = s.cursor;
        if (s.cursor >= s.scroll + kRows)
            s.scroll = s.cursor - kRows + 1;
    };
    switch (s.id)
    {
        case ScreenId::Meter: return;
        case ScreenId::Home:
            if (s.field > 0)
            {
                // direct locate: seconds, minutes, hours
                const int unit = s.field == 1 ? 1 : s.field == 2 ? 60 : 3600;
                s.value = juce::jlimit (0, 23 * 3600 + 59 * 60 + 59, s.value + d * unit);
                return;
            }
            if (! recorder.isRecording() && countInLeft == 0)
            {
                // the dial moves the position: a second a click (a beat with the bar display)
                const auto& mt = recorder.info.metronome;
                const double step = mt.barDisplay ? 60.0 / mt.tempo : 1.0;
                recorder.locate (recorder.getPosition() + (int64_t) (d * step * sampleRate));
                needSpp = needFullFrame = true;
            }
            return;
        case ScreenId::List:
            s.cursor += d;
            scrollTo ((int) s.items.size());
            return;
        case ScreenId::Choice:
            s.value = juce::jlimit (0, s.options.size() - 1, s.value + d);
            s.cursor = s.value;
            if (s.live && s.set)
                s.set (s.value);
            return;
        case ScreenId::Number:
            s.value = juce::jlimit (s.minimum, s.maximum, s.value + d * s.step);
            if (s.live && s.set)
                s.set (s.value);
            return;
        case ScreenId::TextEdit:
        {
            const int count = juce::String (kPalette).length() + 1; // (+ DEL)
            s.value = (s.value + d % count + count) % count;
            return;
        }
        case ScreenId::Effect:
        {
            if (! fxTurned)
            {
                fxUndoValue = s.field == 0 ? (float) fx.type : fx.param[(size_t) fx.type];
                fxTurned = true;
            }
            if (s.field == 0)
                fx.type = juce::jlimit (0, kFxTypes - 1, fx.type + d);
            else if (s.field == 1)
                fx.param[(size_t) fx.type] = juce::jlimit (0.0f, 1.0f, fx.param[(size_t) fx.type] + (float) d * 0.01f);
            else
            {
                // LIB: a used slot loads, an empty one stores the current setting (p.42)
                fx.libSlot = juce::jlimit (-1, kFxLibrary - 1, fx.libSlot + d);
                if (fx.libSlot >= 0)
                {
                    auto& slot = fx.library[(size_t) fx.libSlot];
                    if (slot.used)
                    {
                        fx.type = slot.type;
                        fx.param[(size_t) fx.type] = slot.param;
                    }
                }
                fxTurned = false;
            }
            if (s.field != 2)
                updateLibrary();
            else if (fx.libSlot >= 0)
                updateLibrary();
            settingsChanged();
            return;
        }
        case ScreenId::Song:
            s.cursor += d;
            scrollTo ((int) songCache.size());
            return;
        case ScreenId::SongNew: s.value = juce::jlimit (0, 3, s.value + d); return;
        case ScreenId::Information: s.field = (s.field + (d > 0 ? 1 : 2)) % 3; return;
        case ScreenId::AutoPunch:
        {
            auto& p = s.field == 0 ? recorder.info.punchIn : recorder.info.punchOut;
            if (p < 0)
                p = recorder.getPosition();
            p = std::max<int64_t> (0, p + (int64_t) (d * 0.1 * sampleRate));
            recorder.markDirty();
            return;
        }
        case ScreenId::TrackClear:
            s.cursor += d;
            scrollTo (kTracks);
            return;
        case ScreenId::Import:
        case ScreenId::SdPlay:
            s.cursor += d;
            scrollTo ((int) s.entries.size());
            return;
        case ScreenId::ImportTrack:
        {
            int count = 0;
            for (int t = 0; t < kTracks; ++t)
                count += recorder.hasAudio (t) ? 0 : 1;
            if (! s.entries.empty() && s.entries.front().channels > 1)
                for (int t : { 6, 8, 10 })
                    count += ! recorder.hasAudio (t) && ! recorder.hasAudio (t + 1) ? 1 : 0;
            s.cursor += d;
            scrollTo (count);
            return;
        }
        case ScreenId::TrackSwap:
            if (s.field == 0)
                s.value = juce::jlimit (0, 14, s.value + d);
            else
                s.param = juce::jlimit (0, 14, s.param + d);
            return;
        case ScreenId::Display:
            if (s.field == 0)
                settings.contrast = juce::jlimit (10, 40, settings.contrast + d);
            else
                settings.dimmerHigh = d < 0 ? true : false;
            settingsChanged();
            return;
        case ScreenId::DelayTime:
            settings.outputDelayMs = juce::jlimit (0, 2000, settings.outputDelayMs + d * s.step);
            s.value = settings.outputDelayMs;
            return;
        case ScreenId::SendPoint:
        case ScreenId::Phase:
            s.cursor += d;
            scrollTo (kStrips);
            return;
        case ScreenId::Tempo:
            recorder.info.metronome.tempo = juce::jlimit (20.0, 250.0, std::round (recorder.info.metronome.tempo) + d);
            s.value = (int) recorder.info.metronome.tempo;
            return;
        case ScreenId::Signature:
        {
            auto& mt = recorder.info.metronome;
            if (s.field == 0)
                mt.sigNum = juce::jlimit (1, 12, mt.sigNum + d);
            else
            {
                static const int dens[] = { 1, 2, 4, 8 };
                int i = 0;
                while (i < 3 && dens[i] != mt.sigDen)
                    ++i;
                mt.sigDen = dens[juce::jlimit (0, 3, i + d)];
            }
            return;
        }
        case ScreenId::VampEdit:
        case ScreenId::VampPlay:
        case ScreenId::Confirm:
        case ScreenId::SongInfo:
        case ScreenId::MixExport:
        case ScreenId::SdPlaying:
        case ScreenId::StorageMode:
        case ScreenId::DateTime:
            return;
    }
}

// ======================================================================== drawing

juce::String Desk::timeText (int64_t samples, bool frames) const
{
    const double secs = std::max (0.0, (double) samples / sampleRate);
    const int total = (int) secs;
    auto t = two (total / 3600) + ":" + two ((total / 60) % 60) + ":" + two (total % 60);
    if (frames)
        t << ":" << two ((int) ((secs - total) * 30.0));
    return t;
}

juce::String Desk::counterText() const
{
    const int total = (int) ((double) recorder.getPosition() / sampleRate);
    return two (total / 3600) + "h" + two ((total / 60) % 60) + "m" + two (total % 60) + "s";
}

void Desk::render (Lcd& lcd) const
{
    lcd.clear();
    drawScreen (lcd);
    if (pullUpText.isNotEmpty() && clock < pullUpUntil)
    {
        const int w = Lcd::textWidth (pullUpText) + 8;
        lcd.fill ((Lcd::W - w) / 2 - 1, 41, w + 2, 13, false);
        lcd.frame ((Lcd::W - w) / 2 - 1, 41, w + 2, 13);
        lcd.textCentred (Lcd::W / 2, 44, pullUpText);
    }
    if (popup.active)
        drawPopup (lcd);
}

void Desk::drawTitle (Lcd& lcd, const juce::String& title) const
{
    lcd.fill (0, 0, Lcd::W, 9);
    lcd.textCentred (Lcd::W / 2, 1, title, F::Normal, false);
}

void Desk::drawFunctionBar (Lcd& lcd, const juce::String& f1, const juce::String& f2, const juce::String& f3, const juce::String& f4) const
{
    const juce::String labels[] = { f1, f2, f3, f4 };
    for (int i = 0; i < 4; ++i)
    {
        if (labels[i].isEmpty())
            continue;
        const int x = i * 32;
        lcd.fill (x, kBarY + 1, 31, 8);
        lcd.textCentred (x + 15, kBarY + 3, labels[i], F::Small, false);
    }
}

void Desk::drawTransportIcon (Lcd& lcd, int x, int y) const
{
    switch (recorder.getState())
    {
        case Recorder::State::Stopped: lcd.fill (x + 1, y + 1, 7, 7); break;
        case Recorder::State::Paused:
            lcd.fill (x + 1, y + 1, 2, 7);
            lcd.fill (x + 5, y + 1, 2, 7);
            break;
        case Recorder::State::Recording:
            lcd.icon (x, y, { "..###..", ".#####.", "#######", "#######", "#######", ".#####.", "..###.." });
            break;
        case Recorder::State::Playing:
            lcd.icon (x + 1, y, { "#....", "##...", "###..", "####.", "#####", "####.", "###..", "##...", "#...." });
            break;
    }
}

void Desk::drawPopup (Lcd& lcd) const
{
    const int lines = popup.lines.size();
    const int h = lines * 9 + 8 + (popup.kind != Popup::Message ? 9 : 0);
    const int y = std::max (2, (Lcd::H - h) / 2);
    lcd.fill (6, y, Lcd::W - 12, h, false);
    lcd.frame (6, y, Lcd::W - 12, h);
    lcd.frame (7, y + 1, Lcd::W - 14, h - 2);
    for (int i = 0; i < lines; ++i)
        lcd.textCentred (Lcd::W / 2, y + 4 + i * 9, popup.lines[i]);
    if (popup.kind == Popup::Phantom)
        lcd.textCentred (Lcd::W / 2, y + 4 + lines * 9, "PUSH JOG: ON", F::Small);
    else if (popup.kind == Popup::Confirm)
        lcd.textCentred (Lcd::W / 2, y + 4 + lines * 9, "F1:NO   F4:YES", F::Small);
}

void Desk::drawHome (Lcd& lcd) const
{
    const auto& info = recorder.info;
    const auto& s = top();
    // the status line: repeat, format, auto punch, output delay, card, USB
    int x = 0;
    if (recorder.repeat == Repeat::PlayStop)
        x = lcd.label (0, 0, "RPT1", F::Small) + 2;
    else if (recorder.repeat == Repeat::Loop)
        x = lcd.label (0, 0, "RPT", F::Small) + 2;
    const juce::String fmt = loaded ? (info.sampleRate > 46000.0 ? "48/" : "44/") + juce::String (info.bits)
                                    : (sampleRate > 46000.0 ? "48/24" : "44/24");
    x = lcd.label (x, 0, fmt, F::Small) + 2;
    if (info.autoPunch)
        x = lcd.label (x, 0, "A.PUNCH", F::Small) + 2;
    if (settings.outputDelayOn)
        x = lcd.label (x, 0, "DLY", F::Small) + 2;
    juce::ignoreUnused (x);
    lcd.label (usbConnected ? 100 : 116, 0, "SD", F::Small);
    if (usbConnected)
        lcd.label (112, 0, "USB", F::Small);
    // the song: protected / unsaved marks, its name; the effect
    int nx = 0;
    if (info.protect)
        nx = lcd.text (0, 10, "#", F::Normal);
    else if (marksUnsaved)
        nx = lcd.text (0, 10, "*", F::Normal);
    lcd.text (nx + (nx > 0 ? 2 : 0), 10, loaded ? info.name : juce::String ("NO SONG"));
    const juce::String fxText = aux2JackUsed ? "FX.N/A" : ! fx.on ? "FX.MUTE" : "FX" + juce::String (fx.type + 1);
    lcd.textRight (Lcd::W, 11, fxText, F::Small);
    // the counter (or bars and beats)
    drawTransportIcon (lcd, 0, 22);
    if (s.field > 0)
    {
        // direct locate: the digit being changed is marked
        const int v = s.value;
        const juce::String parts[] = { two (v / 3600), two ((v / 60) % 60), two (v % 60) };
        int cx = 14;
        const char* units[] = { "h", "m", "s" };
        for (int i = 0; i < 3; ++i)
        {
            const int start = cx;
            cx = lcd.text (cx, 19, parts[i], F::Big);
            if (3 - i == s.field)
                lcd.invert (start - 1, 18, cx - start, 16);
            cx = lcd.text (cx, 26, units[i]) + 1;
        }
    }
    else if (info.metronome.barDisplay)
    {
        const auto& mt = info.metronome;
        const double beats = (double) recorder.getPosition() / sampleRate * mt.tempo / 60.0;
        const int64_t b = (int64_t) beats;
        const int bar = (int) std::min<int64_t> (9999, b / std::max (1, mt.sigNum) + 1), beat = (int) (b % std::max (1, mt.sigNum)) + 1;
        int cx = lcd.text (14, 19, juce::String (bar).paddedLeft ('0', 4), F::Big);
        cx = lcd.text (cx + 2, 26, "-");
        lcd.text (cx + 2, 19, two (beat), F::Big);
        lcd.textRight (Lcd::W, 19, juce::String ((int) mt.tempo), F::Small);
        lcd.textRight (Lcd::W, 26, juce::String (mt.sigNum) + "/" + juce::String (mt.sigDen), F::Small);
    }
    else
    {
        const int total = (int) ((double) recorder.getPosition() / sampleRate);
        int cx = lcd.text (14, 19, two (total / 3600), F::Big);
        cx = lcd.text (cx, 26, "h") + 1;
        cx = lcd.text (cx, 19, two ((total / 60) % 60), F::Big);
        cx = lcd.text (cx, 26, "m") + 1;
        cx = lcd.text (cx, 19, two (total % 60), F::Big);
        lcd.text (cx, 26, "s");
    }
    // the position bar, punch points and marks
    const int64_t len = std::max<int64_t> (recorder.songLength(), (int64_t) sampleRate);
    auto px = [len] (int64_t p) { return (int) juce::jlimit<int64_t> (0, 125, p * 125 / len); };
    lcd.frame (0, 36, 128, 4);
    lcd.hline (1, 37, px (recorder.getPosition()));
    lcd.hline (1, 38, px (recorder.getPosition()));
    if (info.autoPunch)
    {
        if (info.punchIn >= 0)
            lcd.icon (px (info.punchIn), 32, { "..#", ".##", "###" });
        if (info.punchOut >= 0)
            lcd.icon (px (info.punchOut), 32, { "#..", "##.", "###" });
    }
    for (auto m : info.marks)
        lcd.vline (px (m) + 1, 41, 3);
    // the time left on the card, and the song's length
    if (storage != nullptr)
    {
        const double bytesPerSecond = info.sampleRate * (info.bits / 8) * kTracks;
        const int64_t remain = (int64_t) ((double) const_cast<Storage*> (storage)->freeBytes() / std::max (1.0, bytesPerSecond));
        const int r = (int) std::min<int64_t> (remain, 99 * 3600);
        lcd.text (0, 46, "REMAIN:" + two (r / 3600) + "H" + two ((r / 60) % 60) + "M" + two (r % 60) + "S", F::Small);
    }
    lcd.textRight (Lcd::W, 46, timeText (recorder.songLength()), F::Small);
    // the function keys
    const juce::String f3 = recorder.canUndo() ? "UNDO" : recorder.canRedo() ? "REDO" : "";
    drawFunctionBar (lcd, "METR", "RPT", f3, settings.f4 == F4Fn::Mark ? "MARK" : "CLIC");
}

void Desk::drawMeters (Lcd& lcd) const
{
    const auto& s = top();
    // the top line: transport and counter, or the source's name
    if (s.param == 0)
    {
        drawTransportIcon (lcd, 0, 0);
        lcd.frame (12, 0, 46, 9);
        lcd.text (14, 2, counterText(), F::Small);
    }
    else
        drawTitle (lcd, s.param == 1 ? "LIVE INPUT" : s.param == 2 ? "PC RETURN" : "MTR RETURN");
    auto bar = [&lcd] (int x, int w, float level) {
        const float db = juce::Decibels::gainToDecibels (level, -60.0f);
        const int h = (int) juce::jlimit (0.0f, 33.0f, (db + 60.0f) / 60.0f * 33.0f);
        lcd.fill (x, 44 - h, w, h);
    };
    lcd.dottedH (0, 44 - (int) ((60.0f - 12.0f) / 60.0f * 33.0f), 128); // the -12 dB guide
    auto levelOf = [this, &s] (int input) {
        switch (s.param)
        {
            case 1: return meters.live[(size_t) input];
            case 2: return meters.pc[(size_t) input];
            case 3: return meters.mtr[(size_t) input];
            default:
            {
                const int strip = stripOfInput (input);
                const int side = isStereo (strip) ? (input - firstInputOf (strip)) : 0;
                return meters.channel[(size_t) (strip * 2 + side)];
            }
        }
    };
    if (s.field == 0) // 1-8
    {
        for (int i = 0; i < 8; ++i)
            bar (i * 16 + 3, 10, levelOf (i));
        for (int i = 0; i < 6; ++i)
        {
            const bool data = s.param != 3 || recorder.hasAudio (i);
            if (data)
                lcd.label (i * 16 + 4, 46, juce::String (i + 1), F::Small);
            else
            {
                lcd.frame (i * 16 + 3, 46, 9, 7);
                lcd.text (i * 16 + 6, 47, juce::String (i + 1), F::Small);
            }
        }
        const bool data = s.param != 3 || recorder.hasAudio (6) || recorder.hasAudio (7);
        if (data)
            lcd.label (100, 46, "7/8", F::Small);
        else
        {
            lcd.frame (99, 46, 15, 7);
            lcd.text (101, 47, "7/8", F::Small);
        }
    }
    else // 9-12: 9/10 and the MAIN MIX
    {
        bar (6, 20, levelOf (8));
        bar (34, 20, levelOf (9));
        const float mL = s.param == 3 ? meters.mtr[(size_t) kMixTrack] : meters.mixL;
        const float mR = s.param == 3 ? meters.mtr[(size_t) kMixTrack + 1] : meters.mixR;
        bar (70, 20, mL);
        bar (98, 20, mR);
        lcd.label (20, 46, "9/10", F::Small);
        lcd.label (88, 46, "MAIN", F::Small);
    }
    drawFunctionBar (lcd, "SEL.", "1-8", "9-12", "");
}

void Desk::drawScreen (Lcd& lcd) const
{
    const auto& s = top();
    auto rowText = [&lcd] (int row, const juce::String& left, const juce::String& right, bool selected) {
        const int y = kRowTop + row * kRowH;
        lcd.text (2, y + 2, left);
        if (right.isNotEmpty())
            lcd.textRight (124, y + 2, right, F::Small);
        if (selected)
            lcd.invert (0, y, 125, kRowH - 1);
    };
    auto scrollBar = [&lcd] (int count, int first) {
        if (count <= kRows)
            return;
        const int track = kRows * kRowH - 2;
        const int h = std::max (4, track * kRows / count);
        const int y = kRowTop + (track - h) * first / std::max (1, count - kRows);
        lcd.vline (126, kRowTop, track);
        lcd.fill (125, y, 3, h);
    };
    switch (s.id)
    {
        case ScreenId::Meter: drawMeters (lcd); return;
        case ScreenId::Home: drawHome (lcd); return;
        case ScreenId::List:
        {
            drawTitle (lcd, s.title);
            for (int r = 0; r < kRows; ++r)
            {
                const int i = s.scroll + r;
                if (i >= (int) s.items.size())
                    break;
                const auto& item = s.items[(size_t) i];
                rowText (r, item.label, item.value ? item.value() : juce::String(), i == s.cursor);
            }
            scrollBar ((int) s.items.size(), s.scroll);
            drawFunctionBar (lcd, "BACK", "", "", "");
            return;
        }
        case ScreenId::Choice:
        {
            drawTitle (lcd, s.title);
            const int first = juce::jlimit (0, std::max (0, s.options.size() - kRows), s.value - kRows + 1 > 0 ? s.value - kRows + 1 : 0);
            for (int r = 0; r < kRows; ++r)
            {
                const int i = first + r;
                if (i >= s.options.size())
                    break;
                rowText (r, s.options[i], {}, i == s.value);
            }
            scrollBar (s.options.size(), first);
            drawFunctionBar (lcd, "BACK", "", "", "");
            return;
        }
        case ScreenId::Number:
        {
            drawTitle (lcd, s.title);
            const auto t = s.format ? s.format (s.value) : juce::String (s.value);
            lcd.textCentred (Lcd::W / 2, 22, t, F::Big);
            drawFunctionBar (lcd, "BACK", "", "", "");
            return;
        }
        case ScreenId::Confirm:
        {
            drawTitle (lcd, s.title);
            for (int i = 0; i < s.lines.size(); ++i)
                lcd.textCentred (Lcd::W / 2, 16 + i * 11, s.lines[i]);
            drawFunctionBar (lcd, "NO", "", s.param == 1 ? "YES" : "", s.param == 1 ? "" : "YES");
            return;
        }
        case ScreenId::TextEdit:
        {
            drawTitle (lcd, s.title);
            const juce::String pal (kPalette);
            int x = 2;
            for (int i = 0; i < s.maximum; ++i)
            {
                const auto c = i < s.text.length() ? s.text.substring (i, i + 1) : juce::String ("_");
                lcd.text (x, 12, c);
                if (i == s.cursor)
                    lcd.invert (x - 1, 11, 7, 9);
                x += 7;
            }
            // the character palette: three rows around the selection, DEL at the end
            const int count = pal.length() + 1;
            const int row = s.value / kPaletteCols, rows = (count + kPaletteCols - 1) / kPaletteCols;
            const int firstRow = juce::jlimit (0, std::max (0, rows - 3), row - 1);
            for (int r = 0; r < 3; ++r)
                for (int c = 0; c < kPaletteCols; ++c)
                {
                    const int i = (firstRow + r) * kPaletteCols + c;
                    if (i >= count)
                        break;
                    const int px = 2 + c * 6, py = 24 + r * 10;
                    if (i == pal.length())
                        lcd.text (px, py, "DEL", F::Small);
                    else
                        lcd.text (px, py, pal.substring (i, i + 1));
                    if (i == s.value)
                        lcd.invert (px - 1, py - 1, i == pal.length() ? 13 : 7, 9);
                }
            drawFunctionBar (lcd, "CANCEL", "<", ">", "OK");
            return;
        }
        case ScreenId::Effect:
        {
            drawTitle (lcd, "EFFECT");
            lcd.text (2, 12, "A:");
            lcd.text (16, 12, aux2JackUsed ? "N/A" : fx.on ? "ON" : "OFF");
            const juce::String rows[] = { "TYPE", fxParamName (fx.type), "LIB" };
            const juce::String values[] = { juce::String (fx.type + 1) + "." + fxName (fx.type), fxParamText (fx.type, fx.param[(size_t) fx.type]),
                                            fx.libSlot < 0 ? juce::String ("--") : juce::String (fx.libSlot + 1) + (fx.library[(size_t) fx.libSlot].used ? "" : " (EMPTY)") };
            for (int i = 0; i < 3; ++i)
            {
                const int y = 22 + i * 10;
                lcd.text (2, y + 1, rows[i], F::Small);
                lcd.textRight (124, y, values[i]);
                if (i == s.field)
                    lcd.invert (0, y - 1, 126, 9);
            }
            const bool delay = (fx.type >= 7 && fx.type <= 9) || fx.type == 12 || fx.type == 13;
            if (delay && s.field == 1)
                lcd.label (40, 12, "TAP", F::Small);
            drawFunctionBar (lcd, "HOME", "ON/OFF", "", fxTurned ? "UNDO" : "DEFAULT");
            return;
        }
        case ScreenId::Song:
        {
            drawTitle (lcd, "SONG");
            for (int r = 0; r < kRows; ++r)
            {
                const int i = s.scroll + r;
                if (i >= (int) songCache.size())
                    break;
                const auto& song = songCache[(size_t) i];
                const juce::String mark = (loaded && song.name == recorder.info.name) ? ">" : song.protect ? "#" : " ";
                juce::String right;
                if (s.param == 1)
                    right = song.written.formatted ("%y/%m/%d");
                else if (s.param == 2)
                    right = juce::String ((double) song.bytes / (1024.0 * 1024.0), 1) + "MB";
                rowText (r, mark + song.name, right, i == s.cursor);
            }
            if (songCache.empty())
                lcd.textCentred (Lcd::W / 2, 26, "No song");
            scrollBar ((int) songCache.size(), s.scroll);
            drawFunctionBar (lcd, "BACK", "DATE", "SIZE", "NEW");
            return;
        }
        case ScreenId::SongNew:
        {
            drawTitle (lcd, "NEW SONG");
            static const char* fmts[] = { "44.1kHz-16bit", "44.1kHz-24bit", "48kHz-16bit", "48kHz-24bit" };
            lcd.text (2, 14, "FORMAT", F::Small);
            lcd.textRight (124, 13, fmts[s.value]);
            lcd.invert (0, 12, 126, 9);
            lcd.text (2, 28, "NAME", F::Small);
            lcd.textRight (124, 27, s.text);
            drawFunctionBar (lcd, "BACK", "NAME", "", "CREATE");
            return;
        }
        case ScreenId::SongInfo:
        {
            drawTitle (lcd, "INFORMATION");
            for (auto& song : songCache)
                if (song.name == s.text)
                {
                    lcd.text (2, 12, song.name);
                    lcd.text (2, 22, juce::String (song.sampleRate > 46000.0 ? "48kHz" : "44.1kHz") + " " + juce::String (song.bits) + "bit");
                    lcd.text (2, 32, juce::String ((double) song.bytes / (1024.0 * 1024.0), 1) + "MB");
                    lcd.text (2, 42, song.written.formatted ("%Y/%m/%d %H:%M"), F::Small);
                }
            drawFunctionBar (lcd, "BACK", "", "", "");
            return;
        }
        case ScreenId::AutoPunch:
        {
            drawTitle (lcd, "AUTO PUNCH");
            const auto& info = recorder.info;
            lcd.text (2, 13, "IN");
            lcd.text (2, 25, "OUT");
            lcd.textRight (124, 13, info.punchIn >= 0 ? timeText (info.punchIn, true) : juce::String ("--:--:--:--"));
            lcd.textRight (124, 25, info.punchOut >= 0 ? timeText (info.punchOut, true) : juce::String ("--:--:--:--"));
            lcd.text (22, 13 + s.field * 12, "<");
            drawTransportIcon (lcd, 0, 37);
            lcd.text (12, 38, timeText (recorder.getPosition(), true));
            if (info.autoPunch)
                lcd.label (92, 37, "A.PUNCH", F::Small);
            drawFunctionBar (lcd, "BACK", "CLEAR", "IN/OUT", info.autoPunch ? "OFF" : "ON");
            return;
        }
        case ScreenId::VampEdit:
        {
            drawTitle (lcd, "VAMP EDIT");
            lcd.label (0, 11, recorder.info.vampOn ? "VAMP ON" : "VAMP OFF", F::Small);
            drawTransportIcon (lcd, 60, 10);
            lcd.text (72, 12, counterText(), F::Small);
            const auto& v = recorder.info.vamps;
            int row = 0;
            for (size_t i = 0; i < v.size() && row < 3; ++i, ++row)
                lcd.text (2, 21 + row * 9, juce::String ((int) i + 1) + " " + timeText (v[i].first) + "-" + timeText (v[i].second), F::Small);
            if (s.param == 1 && row < 3)
                lcd.text (2, 21 + row * 9, "> IN " + timeText ((int64_t) s.value * 16 + s.minimum) + "-", F::Small);
            drawFunctionBar (lcd, "BACK", "IN", "OUT", recorder.info.vampOn ? "OFF" : "ON");
            return;
        }
        case ScreenId::VampPlay:
        {
            drawTitle (lcd, "VAMP PLAY");
            lcd.label (0, 11, recorder.vampAuto ? "AUTO" : "MANUAL", F::Small);
            lcd.label (40, 11, recorder.info.vampOn ? "ON" : "OFF", F::Small);
            if (recorder.repeat != Repeat::Continue)
                lcd.label (60, 11, recorder.repeat == Repeat::Loop ? "RPT" : "RPT1", F::Small);
            drawTransportIcon (lcd, 0, 24);
            lcd.text (12, 25, counterText());
            const int v = recorder.currentVamp();
            if (v >= 0)
                lcd.text (2, 38, "VAMP-" + juce::String (v + 1) + " LOOP");
            else
                lcd.text (2, 38, juce::String ((int) recorder.info.vamps.size()) + " VAMP POINTS", F::Small);
            drawFunctionBar (lcd, "METR", "RPT", recorder.vampAuto ? "MANU" : "AUTO", "VAMP");
            return;
        }
        case ScreenId::TrackClear:
        {
            drawTitle (lcd, "TRACK CLEAR");
            for (int r = 0; r < kRows; ++r)
            {
                const int t = s.scroll + r;
                if (t >= kTracks)
                    break;
                rowText (r, "TRACK " + trackName (t) + (t >= kMixTrack ? " (MIX)" : ""), recorder.hasAudio (t) ? "REC" : "-", t == s.cursor);
            }
            scrollBar (kTracks, s.scroll);
            drawFunctionBar (lcd, "BACK", "ALL", "", "");
            return;
        }
        case ScreenId::Import:
        case ScreenId::SdPlay:
        {
            drawTitle (lcd, s.title);
            for (int r = 0; r < kRows; ++r)
            {
                const int i = s.scroll + r;
                if (i >= (int) s.entries.size())
                    break;
                const auto& e = s.entries[(size_t) i];
                rowText (r, e.folder ? "[" + e.name + "]" : e.name, {}, i == s.cursor);
            }
            if (s.entries.empty())
                lcd.textCentred (Lcd::W / 2, 26, "No File");
            scrollBar ((int) s.entries.size(), s.scroll);
            drawFunctionBar (lcd, "BACK", s.folder.isNotEmpty() ? "UP" : "", "", s.id == ScreenId::SdPlay ? "PLAY" : "SELECT");
            return;
        }
        case ScreenId::ImportTrack:
        {
            drawTitle (lcd, "TRACK IMPORT");
            const auto e = s.entries.empty() ? MusicEntry() : s.entries.front();
            std::vector<juce::String> names;
            for (int t = 0; t < kTracks; ++t)
                if (! recorder.hasAudio (t))
                    names.push_back ("TRACK " + trackName (t) + (e.channels > 1 ? " (L+R)" : ""));
            if (e.channels > 1)
                for (int t : { 6, 8, 10 })
                    if (! recorder.hasAudio (t) && ! recorder.hasAudio (t + 1))
                        names.push_back ("TRACK " + trackName (t) + "/" + trackName (t + 1));
            for (int r = 0; r < kRows; ++r)
            {
                const int i = s.scroll + r;
                if (i >= (int) names.size())
                    break;
                rowText (r, names[(size_t) i], {}, i == s.cursor);
            }
            scrollBar ((int) names.size(), s.scroll);
            drawFunctionBar (lcd, "BACK", "", "", "IMPORT");
            return;
        }
        case ScreenId::TrackSwap:
        {
            static const char* names[] = { "1", "2", "3", "4", "5", "6", "7", "8", "7/8", "9", "10", "9/10", "11", "12", "11/12" };
            drawTitle (lcd, "TRACK SWAP");
            lcd.text (2, 15, "A");
            lcd.textRight (124, 15, names[s.value]);
            lcd.text (2, 29, "B");
            lcd.textRight (124, 29, names[s.param]);
            lcd.invert (0, 13 + s.field * 14, 126, 11);
            drawFunctionBar (lcd, "BACK", "", "", "SWAP");
            return;
        }
        case ScreenId::MixExport:
        {
            drawTitle (lcd, "STEREO MIX EXPORT");
            lcd.textCentred (Lcd::W / 2, 16, recorder.info.name);
            lcd.textCentred (Lcd::W / 2, 28, "TRACK 11/12 -> WAV", F::Small);
            drawFunctionBar (lcd, "BACK", "", "", "EXEC");
            return;
        }
        case ScreenId::SdPlaying:
        {
            drawTitle (lcd, "SD PLAY");
            const auto name = sdIndex >= 0 && sdIndex < (int) sdFiles.size() ? sdFiles[(size_t) sdIndex].name : juce::String();
            lcd.text (2, 12, name);
            const int64_t len = sdAudio.empty() ? 0 : sdAudio[0]->length();
            lcd.text (2, 24, timeText (std::min (sdPos, len)) + " / " + timeText (len), F::Small);
            lcd.frame (0, 32, 128, 4);
            if (len > 0)
                lcd.hline (1, 33, (int) (std::min (sdPos, len) * 125 / len), true), lcd.hline (1, 34, (int) (std::min (sdPos, len) * 125 / len), true);
            if (sdRepeat > 0)
                lcd.label (0, 40, sdRepeat == 1 ? "RPT1" : "RPT", F::Small);
            lcd.text (40, 40, sdRolling ? (sdPaused ? "PAUSE" : "PLAY") : "STOP", F::Small);
            drawFunctionBar (lcd, "BACK", "RPT", "|<<", ">>|");
            return;
        }
        case ScreenId::StorageMode:
            drawTitle (lcd, "STORAGE");
            if (s.param == 1)
            {
                lcd.textCentred (Lcd::W / 2, 18, "Connected");
                lcd.textCentred (Lcd::W / 2, 30, "SONG FOLDER OPEN", F::Small);
            }
            else
            {
                lcd.textCentred (Lcd::W / 2, 18, "USB storage");
                lcd.textCentred (Lcd::W / 2, 30, "F4: CONNECT", F::Small);
            }
            drawFunctionBar (lcd, "BACK", "", "", s.param == 1 ? "" : "CONNECT");
            return;
        case ScreenId::Information:
        {
            drawTitle (lcd, "INFORMATION");
            static const char* pages[] = { "CARD", "SONG", "FIRMWARE" };
            lcd.label (0, 11, pages[s.field], F::Small);
            lcd.text (100, 11, juce::String (s.field + 1) + "/3", F::Small);
            if (s.field == 0)
            {
                const int64_t total = storage != nullptr ? const_cast<Storage*> (storage)->totalBytes() : 0;
                const int64_t free = storage != nullptr ? const_cast<Storage*> (storage)->freeBytes() : 0;
                auto gb = [] (int64_t b) { return juce::String ((double) b / (1024.0 * 1024.0 * 1024.0), 2) + "GB"; };
                lcd.text (2, 20, "TOTAL SIZE", F::Small);
                lcd.textRight (124, 20, gb (total), F::Small);
                lcd.text (2, 30, "USED SIZE", F::Small);
                lcd.textRight (124, 30, gb (total - free), F::Small);
                lcd.text (2, 40, "REMAIN SIZE", F::Small);
                lcd.textRight (124, 40, gb (free), F::Small);
            }
            else if (s.field == 1)
            {
                lcd.text (2, 24, "TOTAL SONG", F::Small);
                lcd.textRight (124, 24, juce::String ((int) songCache.size()), F::Small);
            }
            else
            {
                lcd.text (2, 24, "VERSION", F::Small);
                lcd.textRight (124, 24, "1.00", F::Small);
            }
            drawFunctionBar (lcd, "BACK", "", "", "");
            return;
        }
        case ScreenId::DateTime:
        {
            drawTitle (lcd, "DATE/TIME");
            const auto now = juce::Time::getCurrentTime();
            lcd.textCentred (Lcd::W / 2, 16, now.formatted ("%Y/%m/%d"));
            lcd.textCentred (Lcd::W / 2, 28, now.formatted ("%H:%M"));
            lcd.textCentred (Lcd::W / 2, 41, "FROM THE COMPUTER", F::Small);
            drawFunctionBar (lcd, "BACK", "", "", "");
            return;
        }
        case ScreenId::Display:
            drawTitle (lcd, "DISPLAY");
            rowText (0, "CONTRAST", juce::String (settings.contrast), s.field == 0);
            rowText (1, "DIMMER", settings.dimmerHigh ? "HIGH" : "LOW", s.field == 1);
            drawFunctionBar (lcd, "BACK", "", "", "");
            return;
        case ScreenId::DelayTime:
            drawTitle (lcd, "DELAY TIME");
            lcd.textCentred (Lcd::W / 2, 20, juce::String (settings.outputDelayMs) + "ms", F::Big);
            lcd.textCentred (Lcd::W / 2, 42, "STEP " + juce::String (s.step) + "MS", F::Small);
            drawFunctionBar (lcd, "BACK", "", "x1ms", "x100ms");
            return;
        case ScreenId::SendPoint:
        case ScreenId::Phase:
        {
            const bool phase = s.id == ScreenId::Phase;
            drawTitle (lcd, s.title);
            static const char* sp[] = { "PRE COMP", "POST COMP", "POST EQ" };
            for (int r = 0; r < kRows; ++r)
            {
                const int ch = s.scroll + r;
                if (ch >= kStrips)
                    break;
                const auto& c = strips[(size_t) ch];
                rowText (r, "CH " + stripName (ch), phase ? (c.phaseInvert ? "INVERT" : "NORMAL") : sp[(int) c.sendPoint], ch == s.cursor);
            }
            scrollBar (kStrips, s.scroll);
            drawFunctionBar (lcd, "BACK", "", "", phase ? "" : "ALL");
            return;
        }
        case ScreenId::Tempo:
            drawTitle (lcd, "TEMPO");
            lcd.textCentred (Lcd::W / 2, 18, juce::String ((int) recorder.info.metronome.tempo), F::Big);
            lcd.textCentred (Lcd::W / 2, 38, "BPM  (TAP: CLICK)", F::Small);
            drawFunctionBar (lcd, "BACK", "", "", metroOn ? "CLK OFF" : "CLK ON");
            return;
        case ScreenId::Signature:
        {
            drawTitle (lcd, "SIGNATURE");
            const auto& mt = recorder.info.metronome;
            const int x0 = lcd.text (40, 20, juce::String (mt.sigNum).paddedLeft (' ', 2), F::Big);
            if (s.field == 0)
                lcd.invert (39, 19, x0 - 39, 16);
            const int x1 = lcd.text (x0 + 2, 20, "/", F::Big);
            const int x2 = lcd.text (x1 + 2, 20, juce::String (mt.sigDen), F::Big);
            if (s.field == 1)
                lcd.invert (x1 + 1, 19, x2 - x1, 16);
            drawFunctionBar (lcd, "BACK", "", "", "");
            return;
        }
    }
}
} // namespace trackdesk
