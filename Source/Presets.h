#pragma once
// Preset definitions, ported 1:1 from the "ריג גיבוי" web page (DEF object).
#include <juce_core/juce_core.h>
#include <vector>

namespace rig
{
struct KnobDef { const char* key; const char* label; float def; float min = 0.f; float max = 10.f; bool ms = false; };
struct SwDef   { const char* key; const char* label; bool def; };
struct FxDef   { const char* id; const char* label; bool fixed; bool on; float tap; std::vector<KnobDef> k; };

struct PresetDef
{
    const char* id;
    const char* name;
    const char* cab;
    const char* fs;
    const char* he;      // Hebrew sub-label (UTF-8)
    juce::uint32 accent; // 0xAARRGGBB
    const char* blurb;   // Hebrew description (UTF-8)
    std::vector<KnobDef> amp;
    std::vector<SwDef> sw;
    std::vector<FxDef> fx;
};

inline const std::vector<PresetDef>& presets()
{
    static const std::vector<PresetDef> P {
        { "clean", "Blackpanel", "2\xc3\x97" "12 open back", "CLEAN",
          u8"קלין פנדרי", 0xff8fd6c2,
          u8"קלין אמריקאי בסגנון Twin עם המון headroom, ספרינג רוורב של פעם, ויברטו ללחיצה אחת וכורוס עדין שפותח את הסטריאו.",
          { {"vol","VOLUME",4}, {"treb","TREBLE",6.5f}, {"mid","MIDDLE",4}, {"bass","BASS",4.5f}, {"level","LEVEL",5} },
          { {"bright","BRIGHT",true} },
          { { "spring","Spring",false,true,0, { {"rev","REVERB",3.5f}, {"dwell","DWELL",5} } },
            { "vib","Vibrato",false,false,0, { {"speed","SPEED",5}, {"inten","INTENSITY",5} } },
            { "chorus","Chorus",false,true,0, { {"crate","RATE",3}, {"cdepth","DEPTH",3.5f} } } } },

        { "sky", "NIGHTSKY", "cloud + shimmer", "SKY",
          u8"אמביאנס", 0xffbfe6ff,
          u8"ענן רוורב ארוך שנפתח לאט, שימר באוקטבה שמטפס בתוך הזנב, ודיליי דוטד-אייט׳ס בפינג-פונג. כשעוברים לפריסט אחר הזנב ממשיך לצלצל.",
          { {"decay","DECAY",7}, {"mix","MIX",5}, {"tone","TONE",5}, {"shim","SHIMMER",4}, {"mod","MOD",4}, {"level","LEVEL",5} },
          {},
          { { "dly","Delay",false,true,0.75f, { {"time","TIME",450,80,1200,true}, {"fb","FDBK",4}, {"dmix","MIX",3} } },
            { "amp","Amp",true,true,0, { {"gain","GAIN",3}, {"bass","BASS",5}, {"treb","TREBLE",5} } } } },

        { "plexi", "PLEXI WALL", "2 \xc3\x97 4\xc3\x97" "12 greenback", "PLEXI",
          u8"קיר מרשלים", 0xffff7a2e,
          u8"שני ראשי פלקסי על שני קבינטים, אחד בכל צד, כמו גיטרה מוכפלת באולפן. קראנץ׳ פתוח ומלא לפאוור קורדס, בלי להפוך למטאל.",
          { {"pres","PRESENCE",5}, {"bass","BASS",4}, {"mid","MIDDLE",7}, {"treb","TREBLE",6}, {"vol","VOLUME",6.5f}, {"level","LEVEL",5} },
          {},
          { { "wall","Wall",false,true,0, { {"width","WIDTH",8} } },
            { "room","Room",false,true,0, { {"room","MIX",3} } } } },

        { "lead", "CHROME LEAD", "4\xc3\x97" "12 V30", "LEAD",
          u8"ליד סולדנו", 0xffff2a44,
          u8"הערוץ החם של ראש אמריקאי בסגנון סולדנו: גיין חלק, מידים שרים וסאסטיין ארוך. בוסט מהודק מלפנים, דיליי ופלייט מאחור.",
          { {"pre","PREAMP",6.5f}, {"bass","BASS",5}, {"mid","MIDDLE",6.5f}, {"treb","TREBLE",5}, {"pres","PRESENCE",4}, {"level","MASTER",5} },
          {},
          { { "boost","Boost",false,true,0, { {"bdrive","DRIVE",5} } },
            { "dly","Delay",false,true,1.0f, { {"time","TIME",420,80,1200,true}, {"fb","FDBK",3.5f}, {"dmix","MIX",2.5f} } },
            { "plate","Plate",false,true,0, { {"pmix","MIX",2} } } } },
    };
    return P;
}

inline juce::String knobId (const char* preset, const char* key) { return juce::String (preset) + "_" + key; }
inline juce::String swId   (const char* preset, const char* key) { return juce::String (preset) + "_on_" + key; }

// global parameter IDs
namespace gid
{
    inline const char* preset  = "preset";
    inline const char* inDb    = "inDb";
    inline const char* gate    = "gate";
    inline const char* outDb   = "outDb";
    inline const char* mono    = "mono";
    inline const char* hq      = "hq";
    inline const char* inCh    = "inCh";
}
} // namespace rig
