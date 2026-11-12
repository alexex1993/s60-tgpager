---
name: s60-n70-dev
description: S60 2nd Edition FP3 / Symbian OS 8.1a C++ development for Nokia N70, N72, N90 on Windows. Use when the user wants to create, build, package or deploy .sis apps for S60 2nd / Symbian 8.1, scaffold a new Symbian project, or asks about bldmake/abld/makesis/EPOCROOT/ActivePerl setup. Triggers: "N70", "S60 2nd", "Symbian", ".sis", "SIS", "abld", "bldmake", "epoc.exe".
---

# S60 2nd FP3 (Symbian OS 8.1a) development for Nokia N70 — portable guide

Portable version: **no absolute paths are hardcoded**. First discover the
environment on the user's PC (see step 0), substitute `<SDK>`, `<PERL_BIN>`,
`<DRIVE>` below, then follow the workflow.

## 0. Discover / verify the environment

The user needs two things installed (both are abandonware, hosted on archive.org):

| Component | What to install | Check |
|---|---|---|
| S60 2nd Edition FP3 SDK for C++ | `s60_2nd_sdk_fp3.zip` from [archive.org/details/nokia_sdks_n_dev_tools](https://archive.org/details/nokia_sdks_n_dev_tools) | dir contains `Epoc32\tools\bldmake.bat` |
| ActivePerl **5.6.1.638** (exact version!) | old ActiveState builds; must end up in a path without spaces | `perl -v` prints v5.6.1 build 638 |

Discovery commands (adjust if user knows the paths):

```powershell
# typical SDK locations: C:\Symbian\8.1a\S60_2nd_FP3, D:\Symbian\8.1a\S60_2nd_FP3 ...
Get-ChildItem -Path C:\,D:\ -Filter 'S60_2nd_FP3' -Recurse -Depth 3 -Directory -ErrorAction SilentlyContinue
# typical perl: C:\Perl\bin\perl.exe
```

Hard requirements (2004-era tooling, non-negotiable):

1. `<SDK>` path must contain **no spaces**.
2. SDK and all projects must be on the **same drive** (see EPOCROOT below).
3. Perl 5.6.1 must be **first** in PATH during builds.

If requirements are violated, the correct fix is to move/reinstall the SDK
to a path like `C:\Symbian\8.1a\S60_2nd_FP3` before anything else.

## 1. Environment for a build session

PowerShell (per session; replace `<SDK>` with the real path, keep the format!):

```powershell
$env:EPOCROOT='<SDK>'            # e.g. '\Symbian\8.1a\S60_2nd_FP3\'
                                 # NO drive letter, LEADING and TRAILING backslash
$env:PATH="<PERL_BIN>;<SDK>\Epoc32\gcc\bin;<SDK>\Epoc32\tools;$env:PATH"
# e.g. "C:\Perl\bin;C:\Symbian\8.1a\S60_2nd_FP3\Epoc32\gcc\bin;C:\Symbian\8.1a\S60_2nd_FP3\Epoc32\tools;..."
```

Why each piece matters:

| Piece | Without it |
|---|---|
| `EPOCROOT` (no drive letter!) | build tools can't locate Epoc32; they prepend the *current* drive letter, hence the same-drive rule |
| `<PERL_BIN>` first | any newer perl breaks the 2001-era build scripts |
| `Epoc32\gcc\bin` | no compiler: device builds (thumb/armi) use the GNU ARM cross-compiler `thumb-epoc-pe-gcc.exe` bundled here |
| `Epoc32\tools` | no bldmake/abld/makesis |

**Do not** set these globally in the system: `Epoc32\gcc\bin` contains a 2004
`cygwin1.dll` that can break modern Git for Windows / MSYS2 / Cygwin via PATH
DLL shadowing. Keep the environment inside a per-project `build.cmd`.

## 2. Scaffold a new app (from the SDK's own example)

There is no installer-independent template in this portable skill, so derive
one from the SDK example (every SDK copy has it):
`<SDK>\Series60Ex\helloworldbasic`.

Steps (script them — PowerShell 5.1 friendly):

1. **Copy** `<SDK>\Series60Ex\helloworldbasic` to `<PROJECT>\<Name>` where
   `<Name>` is CamelCase alphanumeric (e.g. `MyCoolApp`).
2. **Delete** generated files: `group\ABLD.BAT`, `sis\*.sis`, `sis\*.SIS`.
3. **Rename files** containing `helloworldbasic` (lowercase on disk) to
   `<namelower>` equivalents: `helloworldbasic.mmp` → `<namelower>.mmp`, etc.
4. **Replace inside all text files** (`*.cpp *.h *.rss *.loc *.hrh *.pan *.mmp *.inf`),
   case-sensitively, in this order:
   - `HelloWorldBasic` → `<Name>`
   - `HelloworldBasic` → `<namelower>` (appears once, in the mmp AIF line)
   - `HELLOWORLDBASIC` → `<NAMEUPPER>`
   - `helloworldbasic` → `<namelower>`
   - `0x10005B91` → `<Uid>` (the new UID3, see below)
   - `"HelloWorld"` → `"<Caption>"` (display name in `data\*.loc`)
   - `"HW"` → `"<Caption>"` (short caption, same file)
   Write files back as ASCII (`[Text.Encoding]::ASCII`) — old parsers break on BOM.
5. **UID3 choice**: must be `< 0x80000000` (see Gotchas). Suggest a random
   `0x7000xxxx` test UID; nothing validates it on S60 2nd. The UID must match
   in FOUR places (step 4 handles them all via `0x10005B91` replacement):
   `.mmp` UID line, `aif\*.rss` app_uid, `src\*application.cpp` KUid constant,
   `sis\*.pkg` header.
6. **Write `sis\<namelower>.pkg`** with ABSOLUTE paths (project lives outside
   the SDK tree; artifacts always land inside it):

```
&EN

#{"<Caption>"},(<Uid>),1,0,0

(0x101F7960), 0, 0, 0, {"Series60ProductID"}

"<SDK>\Epoc32\Release\THUMB\UREL\<Name>.APP"            -"!:\system\apps\<Name>\<Name>.app"
"<SDK>\Epoc32\Data\Z\System\Apps\<namelower>\<Name>.RSC"    -"!:\system\apps\<Name>\<Name>.rsc"
"<SDK>\Epoc32\Data\Z\System\Apps\<namelower>\<Name>_CAPTION.RSC" -"!:\system\apps\<Name>\<Name>_caption.rsc"
"<SDK>\Epoc32\Data\Z\System\Apps\<namelower>\<Name>.AIF"    -"!:\system\apps\<Name>\<Name>.aif"
```

7. **Generate `build.cmd`** in the project root (encapsulates the environment):

```bat
@echo off
set EPOCROOT=<SDK with leading+trailing backslash, no drive letter>
set PATH=<PERL_BIN>;<SDK>\Epoc32\gcc\bin;<SDK>\Epoc32\tools;%PATH%
set PRJ=<PROJECT>\<Name>
cd /d "%PRJ%\group"
cmd /c bldmake bldfiles || goto :eof
cmd /c abld.bat build thumb urel || goto :eof
cd /d "%PRJ%\sis"
<SDK>\Epoc32\tools\makesis.exe <namelower>.pkg || goto :eof
copy /y <Name>.sis <PROJECT>\ >nul
echo === SIS ready: <PROJECT>\<Name>.sis ===
```

Result: a minimal GUI app (menu: Options → Hello shows a note; Exit works).
On-screen text: edit `src\<namelower>appview.cpp` `Draw()`.

## 3. Build & artifacts

```
cd <PROJECT>\<Name>\group
bldmake bldfiles        # regenerate after any .mmp change
abld build thumb urel   # device build (armi urel also works; wins*/emulator needs old MSVC/CodeWarrior — NOT available)
```

Artifacts always land in the SDK tree:

| Artifact | Path |
|---|---|
| ARM binary `<Name>.APP` | `<SDK>\Epoc32\Release\THUMB\UREL\` |
| Resources `.rsc` / `.aif` | `<SDK>\Epoc32\Data\Z\System\Apps\<namelower>\` |
| Generated `<NAMEUPPER>.RSG` | `<SDK>\Epoc32\INCLUDE\` |

Package: `makesis <namelower>.pkg` in `sis\` (output `<Name>.SIS`, ~4-5 KB).

## 4. Draw text on screen (common first customization)

In `Draw()` of the AppView:

```cpp
CWindowGc& gc = SystemGc();
TRect rect = Rect();
gc.Clear( rect );
_LIT( KText, "<your text>" );
const CFont* font = iCoeEnv->NormalFont();
TInt w = font->TextWidthInPixels( KText );
gc.UseFont( font );
gc.SetPenColor( KRgbBlack );
gc.DrawText( KText, TPoint( ( rect.Width() - w ) / 2,
                            rect.Height() / 2 + font->AscentInPixels() / 2 ) );
gc.DiscardFont();
```

UI strings live in `data\<namelower>.loc`.

## 5. Install on the phone

Send the `.sis` via Bluetooth or Nokia PC Suite → open the received message →
install. **No signing, no certificates** — S60 2nd has no platform security.
Same UID = silent upgrade. App shows in the phone Menu under `<Caption>`.

## 6. Gotchas (all verified in practice)

1. **UID3 must be < 0x80000000**: the SDK `makesis` parses UIDs as *signed*
   32-bit; `0xE...` "dev range" values fail with `pkg(4): error: number out of range`.
2. EPOCROOT: no drive letter, leading AND trailing backslash.
3. Project + SDK on the same drive, no spaces anywhere in paths.
4. Perl 5.6.1 only, first in PATH.
5. Emulator targets (`wins`/`winscw`) require VC6 / VS.NET 2003 / CodeWarrior 3.1
   (abandonware, trial-only today: [archive.org/details/cw_mobile](https://archive.org/details/cw_mobile)).
   The prebuilt emulator `<SDK>\Epoc32\release\winscw\udeb\epoc.exe` runs but
   cannot host self-built apps. Test on real hardware instead.
6. The bundled 2004 GNU toolchain works fine on modern x64 Windows (WoW64).
7. `PETRAN - PE file preprocessor` in build logs is normal.
8. After editing `.mmp`, re-run `bldmake bldfiles`.
9. Save edited sources as plain ASCII/ANSI, no UTF-8 BOM.
10. Console output of the old tools looks garbled under non-OEM codepages — cosmetic only.

## 7. Verification checklist for a fresh machine

```
perl -v                    → v5.6.1, build 638
bldmake                    → prints usage (tools + perl OK)
bldmake plat               → lists ARM4/ARMI/THUMB/WINSCW/... platforms
thumb-epoc-pe-gcc -dumpmachine → thumb-epoc-pe
```

Then build the **unmodified** `Series60Ex\helloworldbasic` (thumb urel + makesis)
BEFORE scaffolding custom apps — it isolates environment problems from code
problems.

## 8. Learning resources inside the SDK

- 30+ examples: `<SDK>\Series60Ex\` (listbox, grid, form, sockets, Bluetooth,
  SMS/MMS, sound, OpenGL ES...)
- Docs (CHM): `<SDK>\Series60Doc\` — `*_gsg.chm` (Getting Started),
  `*_apiguide.chm`, `*_api_refguide.chm`
