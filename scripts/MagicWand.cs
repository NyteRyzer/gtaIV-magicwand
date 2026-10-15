// =====================================================================
//  Magic Wand & Broomstick for GTA IV
//  Requires: .NET ScriptHook (ScriptHookDotNet) - drop this file and
//  MagicWand.ini into  <GTA IV folder>\scripts\
//
//  Features
//   * Niko permanently carries a "magic wand" (a re-skinnable weapon slot,
//     the pistol by default) with infinite ammo. It is re-given after
//     death / arrest automatically.
//   * Shooting a person with the wand fires a magic blast that throws
//     them through the air.
//   * Telekinesis: with the wand selected, look at a car, person or
//     object and press the Grab key to lift it. It floats in front of
//     you and follows your aim. Shoot to fling it, Grab again to drop it.
//   * Dial BROOMSTICK (2766786255) on Niko's phone and a magic
//     broomstick is delivered to you. It is indestructible and very fast.
// =====================================================================

using System;
using System.Windows.Forms;
using GTA;
using GTA.Native;

public class MagicWandScript : Script
{
    // ---------------- settings (overridden by MagicWand.ini) ------------
    int wandWeaponId;          // native weapon id used as the wand (7 = pistol)
    Keys grabKey;
    Keys pullKey;
    Keys pushKey;
    Keys broomHotkey;
    Keys broomBoostKey;
    float blastRange;
    float blastMinDistance;
    float blastForce;
    int blastExplosionType;
    float blastRadius;
    int blastCooldownMs;
    float grabRange;
    float throwForce;
    string broomModel;
    float broomMaxSpeed;
    float broomAcceleration;
    bool broomAutoWarp;
    string broomNumber;

    // ---------------- state --------------------------------------------
    int lastBlastTime;
    int lastWandCheckTime;

    Ped heldPed;
    Vehicle heldVehicle;
    GTA.Object heldObject;
    float holdDistance = 8f;

    Vehicle broom;
    int lastBroomRepair;
    int lastBoostTime;

    string dialBuffer = "";
    int lastDigitTime;
    int pendingBroomTime = -1;

    public MagicWandScript()
    {
        LoadSettings();

        Interval = 0; // run every frame for smooth telekinesis
        this.Tick += new EventHandler(OnTick);
        this.KeyDown += new GTA.KeyEventHandler(OnKeyDown);
    }

    void LoadSettings()
    {
        wandWeaponId       = Settings.GetValueInteger("WeaponId", "Wand", 7);
        grabKey            = Settings.GetValueKey("GrabKey", "Wand", Keys.E);
        pullKey            = Settings.GetValueKey("PullCloserKey", "Wand", Keys.PageDown);
        pushKey            = Settings.GetValueKey("PushAwayKey", "Wand", Keys.PageUp);
        blastRange         = Settings.GetValueFloat("BlastRange", "Wand", 120f);
        blastMinDistance   = Settings.GetValueFloat("BlastMinDistance", "Wand", 5f);
        blastForce         = Settings.GetValueFloat("BlastForce", "Wand", 40f);
        blastExplosionType = Settings.GetValueInteger("BlastExplosionType", "Wand", 3);
        blastRadius        = Settings.GetValueFloat("BlastRadius", "Wand", 1.5f);
        blastCooldownMs    = Settings.GetValueInteger("BlastCooldownMs", "Wand", 350);
        grabRange          = Settings.GetValueFloat("GrabRange", "Wand", 60f);
        throwForce         = Settings.GetValueFloat("ThrowForce", "Wand", 45f);

        broomModel         = Settings.GetValueString("Model", "Broomstick", "NRG900");
        broomMaxSpeed      = Settings.GetValueFloat("MaxSpeed", "Broomstick", 75f);   // metres / second
        broomAcceleration  = Settings.GetValueFloat("Acceleration", "Broomstick", 30f);
        broomAutoWarp      = Settings.GetValueBool("WarpIntoBroom", "Broomstick", true);
        broomBoostKey      = Settings.GetValueKey("AccelerateKey", "Broomstick", Keys.W);
        broomHotkey        = Settings.GetValueKey("SummonHotkey", "Broomstick", Keys.None);
        broomNumber        = Settings.GetValueString("PhoneNumber", "Broomstick", "2766786255");
    }

    // ====================================================================
    //  Main loop
    // ====================================================================
    void OnTick(object sender, EventArgs e)
    {
        Ped niko = Player.Character;
        if (!Game.Exists(niko)) return;

        if (!niko.isAlive)
        {
            ReleaseHeld(false);
            return;
        }

        EnsureWand(niko);

        bool wandOut = IsWandEquipped(niko) && !niko.isInVehicle();

        if (wandOut) UpdateTelekinesis(niko);
        else ReleaseHeld(false);

        if (wandOut && !IsHolding()) UpdateBlast(niko);

        UpdatePhoneCall(niko);
        UpdateBroom(niko);
    }

    void OnKeyDown(object sender, GTA.KeyEventArgs e)
    {
        // ---- phone dialling: remember the digits Niko types -------------
        int digit = KeyToDigit(e.Key);
        if (digit >= 0)
        {
            if (Game.GameTime - lastDigitTime > 15000) dialBuffer = "";
            lastDigitTime = Game.GameTime;
            dialBuffer += digit.ToString();
            if (dialBuffer.Length > 20) dialBuffer = dialBuffer.Substring(dialBuffer.Length - 20);

            if (broomNumber.Length > 0 && dialBuffer.EndsWith(broomNumber))
            {
                dialBuffer = "";
                Game.DisplayText("Calling BROOMSTICK...", 2000);
                pendingBroomTime = Game.GameTime + 2000; // let it "ring"
            }
        }
        else if (e.Key == Keys.Back && dialBuffer.Length > 0)
        {
            dialBuffer = dialBuffer.Substring(0, dialBuffer.Length - 1);
        }

        if (broomHotkey != Keys.None && e.Key == broomHotkey)
            SummonBroom(Player.Character);

        // ---- telekinesis grab / drop -----------------------------------
        if (e.Key == grabKey)
        {
            Ped niko = Player.Character;
            if (!Game.Exists(niko) || !niko.isAlive || niko.isInVehicle() || !IsWandEquipped(niko)) return;

            if (IsHolding()) ReleaseHeld(false);
            else TryGrab(niko);
        }
    }

    // ====================================================================
    //  Wand
    // ====================================================================
    void EnsureWand(Ped niko)
    {
        if (Game.GameTime - lastWandCheckTime < 1000) return;
        lastWandCheckTime = Game.GameTime;

        try
        {
            if (!Function.Call<bool>("HAS_CHAR_GOT_WEAPON", niko, wandWeaponId))
                Function.Call("GIVE_WEAPON_TO_CHAR", niko, wandWeaponId, 9999, false);

            // infinite magic
            Function.Call("SET_CHAR_AMMO", niko, wandWeaponId, 9999);
        }
        catch (Exception) { }
    }

    bool IsWandEquipped(Ped niko)
    {
        try
        {
            Pointer p = new Pointer(typeof(int));
            Function.Call("GET_CURRENT_CHAR_WEAPON", niko, p);
            return (int)p == wandWeaponId;
        }
        catch (Exception) { return false; }
    }

    void UpdateBlast(Ped niko)
    {
        if (Game.GameTime - lastBlastTime < blastCooldownMs) return;

        bool shooting = false;
        try { shooting = Function.Call<bool>("IS_CHAR_SHOOTING", niko); }
        catch (Exception) { }
        if (!shooting) return;

        Vector3 camPos = Game.CurrentCamera.Position;
        Vector3 camDir = Normalize(Game.CurrentCamera.Direction);

        Ped target = FindPedOnRay(niko, camPos, camDir, blastRange);
        if (target == null) return;

        lastBlastTime = Game.GameTime;

        Vector3 hit = target.Position;
        if (Distance(hit, niko.Position) >= blastMinDistance)
        {
            try
            {
                Function.Call("ADD_EXPLOSION", hit.X, hit.Y, hit.Z, blastExplosionType,
                              blastRadius, true, false, 0.3f);
            }
            catch (Exception) { }
        }

        // throw the victim away from Niko
        Vector3 push = Normalize(hit - niko.Position);
        push.Z = 0.6f;
        try
        {
            target.ForceRagdoll(4000, true);
            target.ApplyForce(push * blastForce);
        }
        catch (Exception) { }
    }

    // ====================================================================
    //  Telekinesis
    // ====================================================================
    bool IsHolding()
    {
        return (heldPed != null && Game.Exists(heldPed))
            || (heldVehicle != null && Game.Exists(heldVehicle))
            || (heldObject != null && Game.Exists(heldObject));
    }

    void TryGrab(Ped niko)
    {
        Vector3 camPos = Game.CurrentCamera.Position;
        Vector3 camDir = Normalize(Game.CurrentCamera.Direction);

        float bestScore = float.MaxValue;
        Ped bestPed = null;
        Vehicle bestVeh = null;
        GTA.Object bestObj = null;

        foreach (Vehicle v in World.GetVehicles(niko.Position, grabRange))
        {
            if (!Game.Exists(v) || (broom != null && v == broom && niko.isInVehicle(v))) continue;
            float s = RayScore(camPos, camDir, v.Position, grabRange, 3.0f);
            if (s < bestScore) { bestScore = s; bestVeh = v; bestPed = null; bestObj = null; }
        }
        foreach (Ped p in World.GetPeds(niko.Position, grabRange))
        {
            if (!Game.Exists(p) || p == niko) continue;
            float s = RayScore(camPos, camDir, p.Position, grabRange, 1.5f);
            if (s < bestScore) { bestScore = s; bestPed = p; bestVeh = null; bestObj = null; }
        }
        try
        {
            foreach (GTA.Object o in World.GetAllObjects())
            {
                if (!Game.Exists(o) || Distance(o.Position, niko.Position) > grabRange) continue;
                float s = RayScore(camPos, camDir, o.Position, grabRange, 1.2f);
                if (s < bestScore) { bestScore = s; bestObj = o; bestPed = null; bestVeh = null; }
            }
        }
        catch (Exception) { }

        if (bestScore == float.MaxValue)
        {
            Game.DisplayText("Nothing to levitate there.", 1000);
            return;
        }

        Vector3 targetPos = bestVeh != null ? bestVeh.Position
                          : bestPed != null ? bestPed.Position
                          : bestObj.Position;
        holdDistance = Clamp(Distance(camPos, targetPos), 4f, 25f);

        heldVehicle = bestVeh;
        heldPed = bestPed;
        heldObject = bestObj;

        try
        {
            if (heldPed != null) heldPed.ForceRagdoll(10000, false);
            if (heldObject != null) heldObject.FreezePosition = false;
        }
        catch (Exception) { }

        Game.DisplayText("Wingardium... levitating!", 1000);
    }

    void UpdateTelekinesis(Ped niko)
    {
        if (!IsHolding()) { ClearHeld(); return; }

        if (isKeyPressed(pullKey)) holdDistance = Math.Max(3f, holdDistance - 0.3f);
        if (isKeyPressed(pushKey)) holdDistance = Math.Min(40f, holdDistance + 0.3f);

        Vector3 camPos = Game.CurrentCamera.Position;
        Vector3 camDir = Normalize(Game.CurrentCamera.Direction);
        Vector3 target = camPos + camDir * holdDistance;

        // shooting while holding something flings it
        bool shooting = false;
        try { shooting = Function.Call<bool>("IS_CHAR_SHOOTING", niko); } catch (Exception) { }
        if (shooting || isKeyPressed(Keys.LButton))
        {
            Throw(camDir);
            return;
        }

        try
        {
            if (heldVehicle != null)
            {
                Vector3 vel = (target - heldVehicle.Position) * 6f;
                heldVehicle.Velocity = ClampLength(vel, 40f);
            }
            else if (heldPed != null)
            {
                if (Game.GameTime % 3000 < 20) heldPed.ForceRagdoll(10000, false);
                heldPed.Position = Lerp(heldPed.Position, target, 0.25f);
            }
            else if (heldObject != null)
            {
                heldObject.Position = Lerp(heldObject.Position, target, 0.25f);
            }
        }
        catch (Exception) { ClearHeld(); }
    }

    void Throw(Vector3 dir)
    {
        try
        {
            if (heldVehicle != null && Game.Exists(heldVehicle))
                heldVehicle.Velocity = dir * throwForce;
            else if (heldPed != null && Game.Exists(heldPed))
            {
                heldPed.ForceRagdoll(5000, true);
                heldPed.ApplyForce(dir * throwForce);
            }
            else if (heldObject != null && Game.Exists(heldObject))
                heldObject.ApplyForce(dir * throwForce);
        }
        catch (Exception) { }

        lastBlastTime = Game.GameTime; // don't also blast on the same click
        ClearHeld();
    }

    void ReleaseHeld(bool unused)
    {
        if (!IsHolding()) { ClearHeld(); return; }
        try
        {
            if (heldVehicle != null && Game.Exists(heldVehicle))
                heldVehicle.Velocity = new Vector3(0f, 0f, -1f);
        }
        catch (Exception) { }
        ClearHeld();
    }

    void ClearHeld()
    {
        heldPed = null;
        heldVehicle = null;
        heldObject = null;
    }

    // ====================================================================
    //  Broomstick
    // ====================================================================
    void UpdatePhoneCall(Ped niko)
    {
        if (pendingBroomTime < 0 || Game.GameTime < pendingBroomTime) return;
        pendingBroomTime = -1;
        SummonBroom(niko);
    }

    void SummonBroom(Ped niko)
    {
        if (!Game.Exists(niko) || !niko.isAlive) return;

        // get rid of the previous broom unless Niko is sitting on it
        if (broom != null && Game.Exists(broom))
        {
            if (niko.isInVehicle(broom))
            {
                broom.Repair();
                Game.DisplayText("You're already on your broomstick!", 2000);
                return;
            }
            broom.Delete();
        }

        double h = niko.Heading * Math.PI / 180.0;
        Vector3 forward = new Vector3((float)-Math.Sin(h), (float)Math.Cos(h), 0f);
        Vector3 pos = niko.Position + forward * 4f;
        broom = World.CreateVehicle(new Model(broomModel), pos);
        if (broom == null || !Game.Exists(broom))
        {
            Game.DisplayText("The broomstick got lost on the way... (bad model in MagicWand.ini?)", 3000);
            return;
        }

        broom.Heading = niko.Heading;
        MakeIndestructible(broom);

        if (broomAutoWarp) niko.WarpIntoVehicle(broom, VehicleSeat.Driver);
        Game.DisplayText("Your magic broomstick has arrived!", 3000);
    }

    void MakeIndestructible(Vehicle v)
    {
        try
        {
            v.CanBeDamaged = false;
            v.CanBeVisiblyDamaged = false;
            v.CanTiresBurst = false;
        }
        catch (Exception) { }
        try
        {
            Function.Call("SET_CAR_PROOFS", v, true, true, true, true, true);
            Function.Call("SET_CAR_CAN_BE_DAMAGED", v, false);
            Function.Call("SET_CAN_BURST_CAR_TYRES", v, false);
            Function.Call("SET_CAR_CAN_BE_VISIBLY_DAMAGED", v, false);
        }
        catch (Exception) { }
    }

    void UpdateBroom(Ped niko)
    {
        if (broom == null || !Game.Exists(broom)) return;

        // keep it pristine no matter what
        if (Game.GameTime - lastBroomRepair > 500)
        {
            lastBroomRepair = Game.GameTime;
            try
            {
                if (broom.Health < 1000 || broom.EngineHealth < 1000f || !broom.isAlive)
                {
                    broom.Repair();
                    broom.Health = 1000;
                    broom.EngineHealth = 1000f;
                }
                MakeIndestructible(broom);
            }
            catch (Exception) { }
        }

        // magic speed boost
        int now = Game.GameTime;
        if (now - lastBoostTime > 200) lastBoostTime = now - 16;
        if (niko.isInVehicle(broom) && isKeyPressed(broomBoostKey))
        {
            try
            {
                float speed = broom.Speed;
                if (speed < broomMaxSpeed)
                {
                    float dt = Clamp((Game.GameTime - lastBoostTime) / 1000f, 0.001f, 0.1f);
                    broom.Speed = Math.Min(broomMaxSpeed, speed + broomAcceleration * dt);
                }
            }
            catch (Exception) { }
        }
        lastBoostTime = now;
    }

    // ====================================================================
    //  Helpers
    // ====================================================================
    Ped FindPedOnRay(Ped niko, Vector3 origin, Vector3 dir, float range)
    {
        Ped best = null;
        float bestScore = float.MaxValue;
        foreach (Ped p in World.GetPeds(niko.Position, range))
        {
            if (!Game.Exists(p) || p == niko || !p.isAlive) continue;
            float s = RayScore(origin, dir, p.Position, range, 1.5f);
            if (s < bestScore) { bestScore = s; best = p; }
        }
        return best;
    }

    // Lower is better; float.MaxValue means "not under the crosshair".
    static float RayScore(Vector3 origin, Vector3 dir, Vector3 point, float range, float tolerance)
    {
        Vector3 toPoint = point - origin;
        float along = Dot(toPoint, dir);
        if (along < 1f || along > range) return float.MaxValue;

        Vector3 closest = origin + dir * along;
        float off = Distance(closest, point);
        float allowed = tolerance + along * 0.03f; // a little more forgiving far away
        if (off > allowed) return float.MaxValue;

        return off / allowed + along * 0.01f;
    }

    static int KeyToDigit(Keys k)
    {
        if (k >= Keys.D0 && k <= Keys.D9) return (int)(k - Keys.D0);
        if (k >= Keys.NumPad0 && k <= Keys.NumPad9) return (int)(k - Keys.NumPad0);
        return -1;
    }

    static float Dot(Vector3 a, Vector3 b) { return a.X * b.X + a.Y * b.Y + a.Z * b.Z; }

    static float Length(Vector3 v) { return (float)Math.Sqrt(Dot(v, v)); }

    static float Distance(Vector3 a, Vector3 b) { return Length(a - b); }

    static Vector3 Normalize(Vector3 v)
    {
        float len = Length(v);
        return len < 0.0001f ? new Vector3(0f, 1f, 0f) : v * (1f / len);
    }

    static Vector3 ClampLength(Vector3 v, float max)
    {
        float len = Length(v);
        return len > max ? v * (max / len) : v;
    }

    static Vector3 Lerp(Vector3 a, Vector3 b, float t) { return a + (b - a) * t; }

    static float Clamp(float v, float min, float max) { return v < min ? min : (v > max ? max : v); }
}
