# Magic Wand & Broomstick: a GTA IV mod

Niko gets a permanent magic wand that can levitate things and fire magic blasts. Calling **BROOMSTICK** on his phone delivers a fast, indestructible magic broomstick.

## Requirements

- GTA IV PC
- [.NET ScriptHook (ScriptHookDotNet)](https://gtaforums.com/topic/392325-gtaiv-alicesscripthook-net/) and an ASI loader
  - On the Complete Edition (1.2.x), use a ScriptHook build that supports your game version. Or downgrade the game, as most script mods require.

## Install

1. Copy `scripts/MagicWand.cs` and `scripts/MagicWand.ini` into `<GTA IV folder>\scripts\`. Create the folder if it doesn't exist.
2. Start the game. ScriptHookDotNet compiles the `.cs` file automatically.

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
| Broom speed | Hold **W**. The broom accelerates far beyond normal top speed, up to about 270 km/h by default. |

The broomstick can't be damaged: it's bullet-, fire-, explosion-, collision- and melee-proof, its tyres can't burst, and the script repairs it every half second. Dialling again replaces the old broom with a new one.

All keys, speeds, forces, the phone number and the vehicle model can be changed in `MagicWand.ini`.

## Making it *look* like a wand and a broom

GTA IV has no wand or broom model, so the script uses existing game slots:

- **Wand:** the pistol (`w_glock` in `pc/models/cdimages/weapons.img`). Replace that model with any wand model from a modding site, using OpenIV.
- **Broomstick:** the `NRG900` sport bike. Replace it with a broomstick vehicle model, or set `Model=` in the ini to another vehicle you've replaced.

If you'd rather not replace the pistol, set `WeaponId` in the ini to another weapon that you re-skin.

## Notes / limitations

- The script spots the phone call by watching the number keys you type. If you dial with the arrow keys or a gamepad, use the keyboard digits instead, or set a `SummonHotkey` in the ini.
- Untested in-game. If something doesn't work, check `ScriptHookDotNet.log` in the game folder for errors.
