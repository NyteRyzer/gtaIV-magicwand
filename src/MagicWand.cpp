// =====================================================================
//  Magic Wand & Broomstick for GTA IV - standalone .asi plugin
//
//  Built on Zolika1351's IV-SDK, which is compiled straight into the .asi,
//  so no ScriptHook / .NET ScriptHook is needed - only an ASI loader.
//  Supports GTA IV 1.0.7.0 and 1.0.8.0 (the versions IV-SDK supports).
//
//  Features
//   * Niko permanently carries a "magic wand" (the pistol slot by default)
//     with infinite ammo. It is re-given after death / arrest.
//   * Shooting a person with the wand fires a magic blast that throws them.
//   * Telekinesis: with the wand selected, look at a car, person or object
//     and press the Grab key to lift it. It floats in front of you and
//     follows your aim. Shoot to fling it, press Grab again to drop it.
//   * Dial BROOMSTICK (2766786255) on Niko's phone and a magic broomstick
//     is delivered. It is indestructible and very fast.
// =====================================================================

#include <cmath>
#include "IVSDK.cpp"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

using namespace Scripting;

namespace
{
	// ------------------------------------------------------------------
	//  Settings (MagicWand.ini next to the .asi)
	// ------------------------------------------------------------------
	struct Settings
	{
		int   wandWeapon        = 7;            // pistol
		int   grabKey           = KEY_E;
		int   pullKey           = KEY_PAGEDOWN;
		int   pushKey           = KEY_PAGEUP;
		float throwSpeed        = 45.0f;
		float grabRange         = 60.0f;
		float blastRange        = 120.0f;
		float blastMinDistance  = 5.0f;
		float blastForce        = 40.0f;
		int   blastExplosion    = 3;
		float blastRadius       = 1.5f;
		int   blastCooldownMs   = 350;

		std::string phoneNumber = "2766786255";
		std::string broomModel  = "NRG900";
		float broomMaxSpeed     = 75.0f;
		float broomAccel        = 30.0f;
		bool  broomWarp         = true;
		int   broomHotkey       = 0;
	} cfg;

	struct KeyName { const char* name; int key; };

	const KeyName kKeyNames[] = {
		{ "A", 30 }, { "B", 48 }, { "C", 46 }, { "D", 32 }, { "E", 18 }, { "F", 33 },
		{ "G", 34 }, { "H", 35 }, { "I", 23 }, { "J", 36 }, { "K", 37 }, { "L", 38 },
		{ "M", 50 }, { "N", 49 }, { "O", 24 }, { "P", 25 }, { "Q", 16 }, { "R", 19 },
		{ "S", 31 }, { "T", 20 }, { "U", 22 }, { "V", 47 }, { "W", 17 }, { "X", 45 },
		{ "Y", 21 }, { "Z", 44 },
		{ "F1", 59 }, { "F2", 60 }, { "F3", 61 }, { "F4", 62 }, { "F5", 63 }, { "F6", 64 },
		{ "F7", 65 }, { "F8", 66 }, { "F9", 67 }, { "F10", 68 }, { "F11", 87 }, { "F12", 88 },
		{ "PAGEUP", 201 }, { "PAGEDOWN", 209 }, { "HOME", 199 }, { "END", 207 },
		{ "INSERT", 210 }, { "DELETE", 211 },
	};

	int ParseKey(const char* text, int fallback)
	{
		std::string s;
		for (const char* p = text; *p; ++p)
			if (*p != ' ') s += (char)toupper((unsigned char)*p);

		if (s.empty()) return fallback;
		if (s == "NONE") return 0;
		for (const KeyName& k : kKeyNames)
			if (s == k.name) return k.key;
		if (isdigit((unsigned char)s[0])) return atoi(s.c_str()); // raw DirectInput scan code
		return fallback;
	}

	std::string IniPath()
	{
		char path[MAX_PATH] = {};
		GetModuleFileNameA(plugin::GetCurrentModule(), path, MAX_PATH);
		std::string p = path;
		size_t dot = p.find_last_of('.');
		if (dot != std::string::npos) p = p.substr(0, dot);
		return p + ".ini";
	}

	std::string IniString(const std::string& ini, const char* sec, const char* key, const char* def)
	{
		char buf[256] = {};
		GetPrivateProfileStringA(sec, key, def, buf, sizeof(buf), ini.c_str());
		return buf;
	}

	float IniFloat(const std::string& ini, const char* sec, const char* key, float def)
	{
		char d[32];
		snprintf(d, sizeof(d), "%g", def);
		return (float)atof(IniString(ini, sec, key, d).c_str());
	}

	int IniInt(const std::string& ini, const char* sec, const char* key, int def)
	{
		return (int)GetPrivateProfileIntA(sec, key, def, ini.c_str());
	}

	bool IniBool(const std::string& ini, const char* sec, const char* key, bool def)
	{
		std::string v = IniString(ini, sec, key, def ? "true" : "false");
		return v == "1" || v == "true" || v == "True" || v == "TRUE" || v == "yes";
	}

	void LoadSettings()
	{
		std::string ini = IniPath();

		cfg.wandWeapon       = IniInt(ini, "Wand", "WeaponId", cfg.wandWeapon);
		cfg.grabKey          = ParseKey(IniString(ini, "Wand", "GrabKey", "E").c_str(), cfg.grabKey);
		cfg.pullKey          = ParseKey(IniString(ini, "Wand", "PullCloserKey", "PageDown").c_str(), cfg.pullKey);
		cfg.pushKey          = ParseKey(IniString(ini, "Wand", "PushAwayKey", "PageUp").c_str(), cfg.pushKey);
		cfg.throwSpeed       = IniFloat(ini, "Wand", "ThrowForce", cfg.throwSpeed);
		cfg.grabRange        = IniFloat(ini, "Wand", "GrabRange", cfg.grabRange);
		cfg.blastRange       = IniFloat(ini, "Wand", "BlastRange", cfg.blastRange);
		cfg.blastMinDistance = IniFloat(ini, "Wand", "BlastMinDistance", cfg.blastMinDistance);
		cfg.blastForce       = IniFloat(ini, "Wand", "BlastForce", cfg.blastForce);
		cfg.blastExplosion   = IniInt(ini, "Wand", "BlastExplosionType", cfg.blastExplosion);
		cfg.blastRadius      = IniFloat(ini, "Wand", "BlastRadius", cfg.blastRadius);
		cfg.blastCooldownMs  = IniInt(ini, "Wand", "BlastCooldownMs", cfg.blastCooldownMs);

		cfg.phoneNumber      = IniString(ini, "Broomstick", "PhoneNumber", cfg.phoneNumber.c_str());
		cfg.broomModel       = IniString(ini, "Broomstick", "Model", cfg.broomModel.c_str());
		cfg.broomMaxSpeed    = IniFloat(ini, "Broomstick", "MaxSpeed", cfg.broomMaxSpeed);
		cfg.broomAccel       = IniFloat(ini, "Broomstick", "Acceleration", cfg.broomAccel);
		cfg.broomWarp        = IniBool(ini, "Broomstick", "WarpIntoBroom", cfg.broomWarp);
		cfg.broomHotkey      = ParseKey(IniString(ini, "Broomstick", "SummonHotkey", "None").c_str(), 0);
	}

	// ------------------------------------------------------------------
	//  Small vector helpers
	// ------------------------------------------------------------------
	struct V3 { float x, y, z; };

	V3 operator+(V3 a, V3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
	V3 operator-(V3 a, V3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
	V3 operator*(V3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
	float Dot(V3 a, V3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
	float Len(V3 a) { return sqrtf(Dot(a, a)); }
	float Dist(V3 a, V3 b) { return Len(a - b); }
	V3 Norm(V3 a) { float l = Len(a); return l < 0.0001f ? V3{ 0, 1, 0 } : a * (1.0f / l); }
	V3 Lerp(V3 a, V3 b, float t) { return a + (b - a) * t; }
	V3 ClampLen(V3 a, float m) { float l = Len(a); return l > m ? a * (m / l) : a; }
	float Clampf(float v, float lo, float hi) { return v < lo ? lo : (v > hi ? hi : v); }

	// ------------------------------------------------------------------
	//  Game helpers
	// ------------------------------------------------------------------
	unsigned int Now()
	{
		unsigned int t = 0;
		GET_GAME_TIMER(&t);
		return t;
	}

	void Notify(const char* text, unsigned int ms = 2000)
	{
		PRINT_STRING_WITH_LITERAL_STRING_NOW("STRING", text, ms, true);
	}

	Ped PlayerPed()
	{
		Ped ped = 0;
		GET_PLAYER_CHAR(CONVERT_INT_TO_PLAYERINDEX(GET_PLAYER_ID()), &ped);
		return ped;
	}

	V3 PedPos(Ped p)      { V3 v{}; GET_CHAR_COORDINATES(p, &v.x, &v.y, &v.z); return v; }
	V3 CarPos(Vehicle c)  { V3 v{}; GET_CAR_COORDINATES(c, &v.x, &v.y, &v.z); return v; }
	V3 ObjPos(Object o)   { V3 v{}; GET_OBJECT_COORDINATES(o, &v.x, &v.y, &v.z); return v; }

	V3 EntityPos(CEntity* e)
	{
		if (!e || !e->m_pMatrix) return { 0, 0, -10000.0f };
		return { e->m_pMatrix->pos.x, e->m_pMatrix->pos.y, e->m_pMatrix->pos.z };
	}

	// Gameplay camera position and the direction it looks in.
	void GetAim(V3& pos, V3& dir)
	{
		int cam = 0;
		GET_GAME_CAM(&cam);
		V3 rot{};
		GET_CAM_POS(cam, &pos.x, &pos.y, &pos.z);
		GET_CAM_ROT(cam, &rot.x, &rot.y, &rot.z);

		const float d2r = 3.14159265f / 180.0f;
		float pitch = rot.x * d2r, yaw = rot.z * d2r;
		dir = Norm({ -sinf(yaw) * cosf(pitch), cosf(yaw) * cosf(pitch), sinf(pitch) });
	}

	bool PadDown(int control)
	{
		CPad* pad = CPad::GetPad();
		return pad && pad->m_aValues[control].m_nCurrentValue > 0;
	}

	// Lower is better; HUGE_VALF means "not under the crosshair".
	float RayScore(V3 origin, V3 dir, V3 point, float range, float tolerance)
	{
		V3 to = point - origin;
		float along = Dot(to, dir);
		if (along < 1.0f || along > range) return HUGE_VALF;

		float off = Dist(origin + dir * along, point);
		float allowed = tolerance + along * 0.03f;
		if (off > allowed) return HUGE_VALF;
		return off / allowed + along * 0.01f;
	}

	void SetCarVelocity(Vehicle car, V3 v)
	{
		CVehicle* veh = CPools::ms_pVehiclePool->GetAt(car);
		if (!veh) return;
		auto collider = veh->GetConstrainedCollider();
		if (!collider || !collider->m_pUnkParameters || !collider->m_pUnkParameters->m_pParams2) return;
		auto& vel = collider->m_pUnkParameters->m_pParams2->m_vVelocity;
		vel.x = v.x; vel.y = v.y; vel.z = v.z;
	}

	void Ragdoll(Ped p, int ms)
	{
		SWITCH_PED_TO_RAGDOLL(p, ms, ms, false, true, true, false);
	}

	// ------------------------------------------------------------------
	//  State
	// ------------------------------------------------------------------
	unsigned int lastWandCheck = 0;
	unsigned int lastBlast = 0;

	enum class Held { None, Ped, Car, Object };
	Held heldType = Held::None;
	int heldHandle = 0;
	float holdDistance = 8.0f;

	Vehicle broom = 0;
	unsigned int lastBroomFix = 0;
	unsigned int lastFrame = 0;

	std::string dialBuffer;
	unsigned int lastDigit = 0;
	unsigned int pendingBroomAt = 0;

	// ------------------------------------------------------------------
	//  Wand
	// ------------------------------------------------------------------
	void EnsureWand(Ped niko)
	{
		unsigned int t = Now();
		if (t - lastWandCheck < 1000) return;
		lastWandCheck = t;

		if (!HAS_CHAR_GOT_WEAPON(niko, cfg.wandWeapon))
			GIVE_WEAPON_TO_CHAR(niko, cfg.wandWeapon, 9999, false);
		SET_CHAR_AMMO(niko, cfg.wandWeapon, 9999); // infinite magic
	}

	bool WandEquipped(Ped niko)
	{
		unsigned int w = 0;
		GET_CURRENT_CHAR_WEAPON(niko, &w);
		return (int)w == cfg.wandWeapon;
	}

	Ped FindPedOnRay(Ped niko, V3 origin, V3 dir, float range)
	{
		Ped best = 0;
		float bestScore = HUGE_VALF;
		auto pool = CPools::ms_pPedPool;
		for (int i = 0; i < (int)pool->m_nCount; i++)
		{
			CPed* p = pool->Get(i);
			if (!p) continue;
			Ped h = pool->GetIndex(p);
			if (h == niko || IS_CHAR_DEAD(h)) continue;
			float s = RayScore(origin, dir, EntityPos(p), range, 1.5f);
			if (s < bestScore) { bestScore = s; best = h; }
		}
		return best;
	}

	void UpdateBlast(Ped niko)
	{
		unsigned int t = Now();
		if (t - lastBlast < (unsigned int)cfg.blastCooldownMs) return;
		if (!IS_CHAR_SHOOTING(niko)) return;

		V3 camPos, camDir;
		GetAim(camPos, camDir);

		Ped target = FindPedOnRay(niko, camPos, camDir, cfg.blastRange);
		if (!target) return;
		lastBlast = t;

		V3 hit = PedPos(target);
		V3 me = PedPos(niko);
		if (Dist(hit, me) >= cfg.blastMinDistance)
			ADD_EXPLOSION(hit.x, hit.y, hit.z, cfg.blastExplosion, cfg.blastRadius, true, false, 0.3f);

		V3 push = Norm(hit - me);
		push.z = 0.6f;
		push = push * cfg.blastForce;
		Ragdoll(target, 4000);
		APPLY_FORCE_TO_PED(target, 3, push.x, push.y, push.z, 0, 0, 0, 0, 1, 1, 1);
	}

	// ------------------------------------------------------------------
	//  Telekinesis
	// ------------------------------------------------------------------
	bool HeldExists()
	{
		switch (heldType)
		{
		case Held::Ped:    return DOES_CHAR_EXIST(heldHandle);
		case Held::Car:    return DOES_VEHICLE_EXIST(heldHandle);
		case Held::Object: return DOES_OBJECT_EXIST(heldHandle);
		default:           return false;
		}
	}

	void ClearHeld() { heldType = Held::None; heldHandle = 0; }

	void ReleaseHeld()
	{
		if (HeldExists() && heldType == Held::Car)
			SetCarVelocity(heldHandle, { 0, 0, -1 });
		ClearHeld();
	}

	void TryGrab(Ped niko)
	{
		V3 camPos, camDir;
		GetAim(camPos, camDir);
		V3 me = PedPos(niko);

		float best = HUGE_VALF;
		Held type = Held::None;
		int handle = 0;
		V3 where{};

		auto vehPool = CPools::ms_pVehiclePool;
		for (int i = 0; i < (int)vehPool->m_nCount; i++)
		{
			CVehicle* v = vehPool->Get(i);
			if (!v) continue;
			V3 p = EntityPos(v);
			if (Dist(p, me) > cfg.grabRange) continue;
			int h = vehPool->GetIndex(v);
			if (IS_CHAR_IN_CAR(niko, h)) continue;
			float s = RayScore(camPos, camDir, p, cfg.grabRange, 3.0f);
			if (s < best) { best = s; type = Held::Car; handle = h; where = p; }
		}

		auto pedPool = CPools::ms_pPedPool;
		for (int i = 0; i < (int)pedPool->m_nCount; i++)
		{
			CPed* ped = pedPool->Get(i);
			if (!ped) continue;
			V3 p = EntityPos(ped);
			if (Dist(p, me) > cfg.grabRange) continue;
			int h = pedPool->GetIndex(ped);
			if (h == niko) continue;
			float s = RayScore(camPos, camDir, p, cfg.grabRange, 1.5f);
			if (s < best) { best = s; type = Held::Ped; handle = h; where = p; }
		}

		auto objPool = CPools::ms_pObjectPool;
		for (int i = 0; i < (int)objPool->m_nCount; i++)
		{
			CObject* o = objPool->Get(i);
			if (!o) continue;
			V3 p = EntityPos(o);
			if (Dist(p, me) > cfg.grabRange) continue;
			float s = RayScore(camPos, camDir, p, cfg.grabRange, 1.2f);
			if (s < best) { best = s; type = Held::Object; handle = objPool->GetIndex(o); where = p; }
		}

		if (type == Held::None)
		{
			Notify("Nothing to levitate there.", 1000);
			return;
		}

		heldType = type;
		heldHandle = handle;
		holdDistance = Clampf(Dist(camPos, where), 4.0f, 25.0f);

		if (type == Held::Object)
		{
			FREEZE_OBJECT_POSITION(handle, false);
			SET_OBJECT_DYNAMIC(handle, true);
		}
		Notify("Wingardium... levitating!", 1000);
	}

	void Throw(V3 dir)
	{
		V3 v = dir * cfg.throwSpeed;
		switch (heldType)
		{
		case Held::Car:
			SetCarVelocity(heldHandle, v);
			break;
		case Held::Ped:
			Ragdoll(heldHandle, 5000);
			APPLY_FORCE_TO_PED(heldHandle, 3, v.x, v.y, v.z, 0, 0, 0, 0, 1, 1, 1);
			break;
		case Held::Object:
			SET_OBJECT_INITIAL_VELOCITY(heldHandle, v.x, v.y, v.z);
			APPLY_FORCE_TO_OBJECT(heldHandle, 3, v.x, v.y, v.z, 0, 0, 0, 0, 1, 1, 1);
			break;
		default:
			break;
		}
		lastBlast = Now(); // don't also blast on the same click
		ClearHeld();
	}

	void UpdateTelekinesis(Ped niko)
	{
		if (heldType == Held::None) return;
		if (!HeldExists()) { ClearHeld(); return; }

		if (cfg.pullKey && IS_GAME_KEYBOARD_KEY_PRESSED(cfg.pullKey)) holdDistance = fmaxf(3.0f, holdDistance - 0.3f);
		if (cfg.pushKey && IS_GAME_KEYBOARD_KEY_PRESSED(cfg.pushKey)) holdDistance = fminf(40.0f, holdDistance + 0.3f);

		V3 camPos, camDir;
		GetAim(camPos, camDir);
		V3 target = camPos + camDir * holdDistance;

		if (IS_CHAR_SHOOTING(niko) || PadDown(INPUT_ATTACK))
		{
			Throw(camDir);
			return;
		}

		switch (heldType)
		{
		case Held::Car:
			SetCarVelocity(heldHandle, ClampLen((target - CarPos(heldHandle)) * 6.0f, 40.0f));
			break;
		case Held::Ped:
		{
			V3 p = Lerp(PedPos(heldHandle), target, 0.25f);
			SET_CHAR_COORDINATES(heldHandle, p.x, p.y, p.z);
			break;
		}
		case Held::Object:
		{
			V3 p = Lerp(ObjPos(heldHandle), target, 0.25f);
			SET_OBJECT_COORDINATES(heldHandle, p.x, p.y, p.z);
			break;
		}
		default:
			break;
		}
	}

	// ------------------------------------------------------------------
	//  Broomstick
	// ------------------------------------------------------------------
	void MakeIndestructible(Vehicle v)
	{
		SET_CAR_PROOFS(v, true, true, true, true, true);
		SET_CAR_CAN_BE_DAMAGED(v, false);
		SET_CAR_CAN_BE_VISIBLY_DAMAGED(v, false);
		SET_CAN_BURST_CAR_TYRES(v, false);
	}

	void SummonBroom(Ped niko)
	{
		if (!niko || IS_CHAR_DEAD(niko)) return;

		if (broom && DOES_VEHICLE_EXIST(broom))
		{
			if (IS_CHAR_IN_CAR(niko, broom))
			{
				FIX_CAR(broom);
				Notify("You're already on your broomstick!");
				return;
			}
			DELETE_CAR(&broom);
		}
		broom = 0;

		unsigned int model = GET_HASH_KEY(cfg.broomModel.c_str());
		CStreaming::ScriptRequestModel(model);
		CStreaming::LoadAllRequestedModels(false);
		if (!HAS_MODEL_LOADED(model))
		{
			Notify("The broomstick got lost on the way... (check Model in MagicWand.ini)", 3000);
			return;
		}

		float heading = 0;
		GET_CHAR_HEADING(niko, &heading);
		float h = heading * 3.14159265f / 180.0f;
		V3 pos = PedPos(niko) + V3{ -sinf(h), cosf(h), 0 } * 4.0f;

		CREATE_CAR(model, pos.x, pos.y, pos.z, &broom, true);
		MARK_MODEL_AS_NO_LONGER_NEEDED(model);
		if (!broom || !DOES_VEHICLE_EXIST(broom))
		{
			broom = 0;
			Notify("The broomstick got lost on the way...", 3000);
			return;
		}

		SET_CAR_HEADING(broom, heading);
		MakeIndestructible(broom);
		if (cfg.broomWarp) WARP_CHAR_INTO_CAR(niko, broom);
		Notify("Your magic broomstick has arrived!", 3000);
	}

	void UpdatePhone(Ped niko)
	{
		static const int digitKeys[10] = { KEY_0, KEY_1, KEY_2, KEY_3, KEY_4, KEY_5, KEY_6, KEY_7, KEY_8, KEY_9 };
		static const int numpadKeys[10] = { KEY_NUMPAD_0, KEY_NUMPAD_1, KEY_NUMPAD_2, KEY_NUMPAD_3, KEY_NUMPAD_4,
		                                    KEY_NUMPAD_5, KEY_NUMPAD_6, KEY_NUMPAD_7, KEY_NUMPAD_8, KEY_NUMPAD_9 };
		unsigned int t = Now();

		for (int d = 0; d < 10; d++)
		{
			if (!IS_GAME_KEYBOARD_KEY_JUST_PRESSED(digitKeys[d]) && !IS_GAME_KEYBOARD_KEY_JUST_PRESSED(numpadKeys[d]))
				continue;

			if (t - lastDigit > 15000) dialBuffer.clear();
			lastDigit = t;
			dialBuffer += (char)('0' + d);
			if (dialBuffer.size() > 20) dialBuffer.erase(0, dialBuffer.size() - 20);

			const std::string& n = cfg.phoneNumber;
			if (!n.empty() && dialBuffer.size() >= n.size() &&
			    dialBuffer.compare(dialBuffer.size() - n.size(), n.size(), n) == 0)
			{
				dialBuffer.clear();
				Notify("Calling BROOMSTICK...", 2000);
				pendingBroomAt = t + 2000; // let it "ring"
			}
		}
		if (IS_GAME_KEYBOARD_KEY_JUST_PRESSED(KEY_BACKSPACE) && !dialBuffer.empty())
			dialBuffer.pop_back();

		if (pendingBroomAt && t >= pendingBroomAt)
		{
			pendingBroomAt = 0;
			SummonBroom(niko);
		}

		if (cfg.broomHotkey && IS_GAME_KEYBOARD_KEY_JUST_PRESSED(cfg.broomHotkey))
			SummonBroom(niko);
	}

	void UpdateBroom(Ped niko)
	{
		unsigned int t = Now();
		float dt = Clampf((t - lastFrame) / 1000.0f, 0.001f, 0.1f);
		lastFrame = t;

		if (!broom) return;
		if (!DOES_VEHICLE_EXIST(broom)) { broom = 0; return; }

		// keep it pristine no matter what
		if (t - lastBroomFix > 500)
		{
			lastBroomFix = t;
			unsigned int health = 0;
			GET_CAR_HEALTH(broom, &health);
			if (health < 1000 || IS_CAR_DEAD(broom))
			{
				FIX_CAR(broom);
				SET_CAR_HEALTH(broom, 1000);
				SET_ENGINE_HEALTH(broom, 1000.0f);
			}
			MakeIndestructible(broom);
		}

		// magic speed boost (works with keyboard W or a controller trigger)
		if (IS_CHAR_IN_CAR(niko, broom) && PadDown(INPUT_VEH_ACCELERATE))
		{
			float speed = 0;
			GET_CAR_SPEED(broom, &speed);
			if (speed < cfg.broomMaxSpeed)
				SET_CAR_FORWARD_SPEED(broom, fminf(cfg.broomMaxSpeed, speed + cfg.broomAccel * dt));
		}
	}

	// ------------------------------------------------------------------
	//  Main loop - runs every frame while in game
	// ------------------------------------------------------------------
	void MagicWandLoop()
	{
		Ped niko = PlayerPed();
		if (!niko || !DOES_CHAR_EXIST(niko)) return;

		if (IS_CHAR_DEAD(niko))
		{
			ClearHeld();
			return;
		}

		EnsureWand(niko);

		bool wandOut = WandEquipped(niko) && !IS_CHAR_IN_ANY_CAR(niko);
		if (wandOut)
		{
			if (cfg.grabKey && IS_GAME_KEYBOARD_KEY_JUST_PRESSED(cfg.grabKey))
			{
				if (heldType != Held::None) ReleaseHeld();
				else TryGrab(niko);
			}
			UpdateTelekinesis(niko);
			if (heldType == Held::None) UpdateBlast(niko);
		}
		else if (heldType != Held::None)
		{
			ReleaseHeld();
		}

		UpdatePhone(niko);
		UpdateBroom(niko);
	}
}

void plugin::gameStartupEvent()
{
	LoadSettings();
	plugin::processScriptsEvent::Add(MagicWandLoop);
}
