# Backup Rig (ריג גיבוי): VST3 / AU / Standalone

פלאגין גיטרה שנבנה מהאתר "ריג גיבוי": אותם ארבעה ריגים, אותו עיבוד ואותו מראה.

| פריסט | מה זה |
|---|---|
| **CLEAN**: Blackpanel | קלין פנדרי, ספרינג רוורב, ויברטו, כורוס |
| **SKY**: NIGHTSKY | ענן רוורב עם שימר באוקטבה ודיליי פינג-פונג |
| **PLEXI**: PLEXI WALL | שני ראשי פלקסי בסטריאו, חדר |
| **LEAD**: CHROME LEAD | ליד בסגנון סולדנו, בוסט, דיליי, פלייט |

כולל: נויז גייט, טיונר, כיול כניסה, TAP לדיליי, מצב מהיר/איכות, יציאת מונו, ריף דמו.
כל כפתור הוא פרמטר שאפשר לעשות לו אוטומציה ב-DAW. החלפת פריסט משאירה את הזנב של הקודם מצלצל.

## גרסת דפדפן (אייפון, אייפד, כל מחשב)
הריג המקורי רץ בדפדפן בכתובת https://benish665.github.io/backup-rig/, בלי התקנה.
באייפון ובאייפד פותחים ב-Safari עם iRig, ואפשר להוסיף למסך הבית דרך שיתוף > הוספה למסך הבית.

## התקנה

**Windows:** מעתיקים את `Backup Rig.vst3` אל `C:\Program Files\Common Files\VST3\`.
את `Backup Rig.exe` מפעילים ישירות (אפליקציה עצמאית). ב-Options > Audio Settings בוחרים כרטיס קול עם ASIO ובאפר 64 או 128.

**macOS:** `Backup Rig.vst3` אל `~/Library/Audio/Plug-Ins/VST3/`, ו-`Backup Rig.component` אל `~/Library/Audio/Plug-Ins/Components/`.
הקבצים לא חתומים, אז בפעם הראשונה מריצים בטרמינל:
`xattr -cr ~/Library/Audio/Plug-Ins/VST3/"Backup Rig.vst3" ~/Library/Audio/Plug-Ins/Components/"Backup Rig.component"`

**Linux:** `Backup Rig.vst3` אל `~/.vst3/`.

## בנייה

### אוטומטית ב-GitHub (Windows, Mac ו-Linux)
מעלים את התיקייה הזו לריפו ב-GitHub. ה-workflow שב-`.github/workflows/build.yml` רץ לבד בכל push.
בסוף הריצה, בלשונית **Actions** > הריצה האחרונה > **Artifacts**, מורידים את `BackupRig-Windows` או `BackupRig-macOS`.

### ידנית
צריך CMake 3.22 ומעלה ומהדר: Visual Studio 2022 ב-Windows, Xcode ב-Mac. JUCE יורד אוטומטית.
```
cmake -S . -B build
cmake --build build --config Release
```
התוצרים נמצאים ב-`build/BackupRig_artefacts/Release/`.

## מבנה הקוד
- `Source/Presets.h`: הגדרות הפריסטים, כמו אובייקט `DEF` באתר.
- `Source/Dsp.h`: אבני הבניין. פילטרים בנוסחאות של Web Audio, waveshaper עם oversampling, דיליי, כורוס, פינג-פונג, אוקטבה, קונבולוציה, ו-IR שנבנים בקוד.
- `Source/Rigs.h`: ארבעת הריגים והגרף הגלובלי (גייט, crossfade בין פריסטים, מאסטר וקליפר).
- `Source/PluginEditor.*`: הממשק, בכל פריסט עם הסקין שלו.
- `tests/`: רינדור אופליין, השוואה לאתר, ובדיקת host.
