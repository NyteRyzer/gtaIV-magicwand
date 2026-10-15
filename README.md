# Magic Wand & Broomstick: a GTA IV mod

Niko gets a permanent magic wand that can levitate things and fire magic blasts. Calling **BROOMSTICK** on his phone delivers a fast, indestructible magic broomstick.

The mod is a standalone `.asi` plugin. It does **not** need ScriptHook or .NET ScriptHook. Everything it needs is compiled into `MagicWand.asi`, built with [Zolika1351's IV-SDK](https://github.com/Zolika1351/iv-sdk).

## Requirements

- **GTA IV version 1.0.7.0 or 1.0.8.0.** These are the only versions IV-SDK supports. The Steam/Rockstar "Complete Edition" (1.2.x) must be downgraded first. Most GTA IV mods require that anyway.
- **An ASI loader.** Most GTA IV mod setups already have one, for example ZolikaPatch or Ultimate ASI Loader (`dinput8.dll`).

## Install

1. Download `MagicWand.asi` and `MagicWand.ini`:
   - Open the repo's **Actions** tab on GitHub.
   - Click the latest green **Build MagicWand.asi** run.
   - Download the **MagicWand** artifact at the bottom of the page and unzip it.
2. Copy both files into your GTA IV folder, next to `GTAIV.exe`. If your ASI loader uses a `plugins` folder, put them there instead.
3. Start the game.

## How to use

| What | How |
|---|---|
| **Magic wand** | Always in Niko's inventory, in the pistol slot, with infinite ammo. You get it back automatically after you die or get busted. |
| **Magic blast** | Select the wand and shoot a person. A magic explosion goes off and throws them away. |
| **Levitate** | Select the wand, aim at a car, person or object, and press **E**. It floats in front of you and follows your aim. |
| Move it closer or further | **Page Down** / **Page Up** |
| Throw it | Shoot (left click) |
| Drop it | Press **E** again |
| **Broomstick** | Open the phone and dial **2766786255** (B‑R‑O‑O‑M‑S‑T‑I‑C‑K) with your keyboard's number keys. |
| Broom speed | Hold accelerate (**W** or the right trigger). The broom accelerates far beyond normal top speed, up to about 270 km/h by default. |

The broomstick can't be damaged: it's bullet-, fire-, explosion-, collision- and melee-proof, its tyres can't burst, and the mod repairs it every half second. Dialling again replaces the old broom with a new one.

All keys, speeds, forces, the phone number and the vehicle model can be changed in `MagicWand.ini`.

## Making it *look* like a wand and a broom

GTA IV has no wand or broom model, so the mod uses existing game slots:

- **Wand:** the pistol (`w_glock` in `pc/models/cdimages/weapons.img`). Replace that model with any wand model from a modding site, using OpenIV.
- **Broomstick:** the `NRG900` sport bike. Replace it with a broomstick vehicle model, or set `Model=` in the ini to another vehicle you've replaced.

## Building it yourself

GitHub Actions builds the mod automatically on every push (see `.github/workflows/build.yml`).

To build it locally on Windows with Visual Studio 2019 or later, run this from an **x86** Native Tools command prompt:

```
git clone https://github.com/Zolika1351/iv-sdk.git
cl /std:c++17 /O2 /MT /EHsc /LD /I src\shim /I iv-sdk\include src\MagicWand.cpp /link /OUT:MagicWand.asi user32.lib version.lib
```

## Notes / limitations

- The mod spots the phone call by watching the number keys you type while dialling. If you dial with the arrow keys or a gamepad, use the keyboard digits instead, or set a `SummonHotkey` in the ini.
- Untested in-game.
