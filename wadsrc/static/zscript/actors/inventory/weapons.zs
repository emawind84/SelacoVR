class WeaponBase : StateProvider
{
	enum EFireMode
	{
		PrimaryFire,
		AltFire,
		EitherFire
	};

	const ZOOM_INSTANT = 1;
	const ZOOM_NOSCALETURNING = 2;
	
	deprecated("3.7") uint WeaponFlags;		// not to be used directly.
	class<Ammo> AmmoType1, AmmoType2;		// Types of ammo used by self weapon
	int AmmoGive1, AmmoGive2;				// Amount of each ammo to get when picking up weapon
	deprecated("3.7") int MinAmmo1, MinAmmo2;	// not used anywhere and thus deprecated.
	int AmmoUse1, AmmoUse2;					// How much ammo to use with each shot
	int Kickback;
	double YAdjust;							// For viewing the weapon fullscreen
	sound UpSound, ReadySound;				// Sounds when coming up and idle
	class<WeaponBase> SisterWeaponType;			// Another weapon to pick up with self one
	int SelectionOrder;						// Lower-numbered weapons get picked first
	int MinSelAmmo1, MinSelAmmo2;			// Ignore in BestWeapon() if inadequate ammo
	int ReloadCounter;						// For A_CheckForReload
	int BobStyle;							// [XA] Bobbing style. Defines type of bobbing (e.g. Normal, Alpha)  (visual only so no need to be a double)
	float BobSpeed;							// [XA] Bobbing speed. Defines how quickly a weapon bobs.
	float BobRangeX, BobRangeY;				// [XA] Bobbing range. Defines how far a weapon bobs in either direction.
	double WeaponScaleX, WeaponScaleY;		// [XA] Weapon scale. Defines the scale for the held weapon sprites (PSprite). Defaults to (1.0, 1.2) since that's what Doom does.
	Ammo Ammo1, Ammo2;						// In-inventory instance variables
	WeaponBase SisterWeapon;
	double FOVScale;
	double LookScale;						// Multiplier for look sensitivity (like FOV scaling but without the zooming)
	int Crosshair;							// 0 to use player's crosshair
	bool GivenAsMorphWeapon;
	bool bAltFire;							// Set when this weapon's alternate fire is used.
	double UseRange;						// [NS] Distance at which player can +use
	readonly bool bDehAmmo;					// Uses Doom's original amount of ammo for the respective attack functions so that old DEHACKED patches work as intended.
											// AmmoUse1 will be set to the first attack's ammo use so that checking for empty weapons still works
	meta int SlotNumber;
	meta double SlotPriority;

	Vector3 BobPivot3D;	// Pivot used for BobWeapon3D

	virtual ui Vector2 ModifyBobLayer(Vector2 Bob, int layer, double ticfrac) { return Bob; }

	virtual ui Vector3, Vector3 ModifyBobLayer3D(Vector3 Translation, Vector3 Rotation, int layer, double ticfrac) { return Translation, Rotation; }

	virtual ui Vector3 ModifyBobPivotLayer3D(int layer, double ticfrac) { return BobPivot3D; }
	
	property AmmoGive: AmmoGive1;
	property AmmoGive1: AmmoGive1;
	property AmmoGive2: AmmoGive2;
	property AmmoUse: AmmoUse1;
	property AmmoUse1: AmmoUse1;
	property AmmoUse2: AmmoUse2;
	property AmmoType: AmmoType1;
	property AmmoType1: AmmoType1;
	property AmmoType2: AmmoType2;
	property Kickback: Kickback;
	property ReadySound: ReadySound;
	property SelectionOrder: SelectionOrder;
	property MinSelectionAmmo1: MinSelAmmo1;
	property MinSelectionAmmo2: MinSelAmmo2;
	property SisterWeapon: SisterWeaponType;
	property UpSound: UpSound;
	property YAdjust: YAdjust;
	property BobSpeed: BobSpeed;
	property BobRangeX: BobRangeX;
	property BobRangeY: BobRangeY;
	property WeaponScaleX: WeaponScaleX;
	property WeaponScaleY: WeaponScaleY;
	property SlotNumber: SlotNumber;
	property SlotPriority: SlotPriority;
	property LookScale: LookScale;
	property BobPivot3D : BobPivot3D;
	property UseRange: UseRange;

	flagdef NoAutoFire: WeaponFlags, 0;			// weapon does not autofire
	flagdef ReadySndHalf: WeaponFlags, 1;		// ready sound is played ~1/2 the time
	flagdef DontBob: WeaponFlags, 2;			// don't bob the weapon
	flagdef AxeBlood: WeaponFlags, 3;			// weapon makes axe blood on impact
	flagdef NoAlert: WeaponFlags, 4;			// weapon does not alert monsters
	flagdef Ammo_Optional: WeaponFlags, 5;		// weapon can use ammo but does not require it
	flagdef Alt_Ammo_Optional: WeaponFlags, 6;	// alternate fire can use ammo but does not require it
	flagdef Primary_Uses_Both: WeaponFlags, 7;	// primary fire uses both ammo
	flagdef Alt_Uses_Both: WeaponFlags, 8;		// alternate fire uses both ammo
	flagdef Wimpy_Weapon:WeaponFlags, 9;		// change away when ammo for another weapon is replenished
	flagdef Powered_Up: WeaponFlags, 10;		// this is a tome-of-power'ed version of its sister
	flagdef Ammo_CheckBoth: WeaponFlags, 11;	// check for both primary and secondary fire before switching it off
	flagdef No_Auto_Switch: WeaponFlags, 12;	// never switch to this weapon when it's picked up
	flagdef Staff2_Kickback: WeaponFlags, 13;	// the powered-up Heretic staff has special kickback
	flagdef NoAutoaim: WeaponFlags, 14;			// this weapon never uses autoaim (useful for ballistic projectiles)
	flagdef MeleeWeapon: WeaponFlags, 15;		// melee weapon. Used by monster AI with AVOIDMELEE.
	flagdef NoDeathDeselect: WeaponFlags, 16;	// Don't jump to the Deselect state when the player dies
	flagdef NoDeathInput: WeaponFlags, 17;		// The weapon cannot be fired/reloaded/whatever when the player is dead
	flagdef CheatNotWeapon: WeaponFlags, 18;	// Give cheat considers this not a weapon (used by Sigil)
	flagdef NoAutoSwitchTo : WeaponFlags, 19;	// do not auto switch to this weapon ever!
	flagdef OffhandWeapon: WeaponFlags, 20;		// weapon is an offhand weapon
	flagdef NoHandSwitch: WeaponFlags, 21;		// weapon cannot be moved from one hand to another
	flagdef TwoHanded: WeaponFlags, 22;			// two handed weapon
	flagdef NoAutoReverse: WeaponFlags, 23;		// prevent auto reverse of model and sprite when switching to offhand

	// no-op flags
	flagdef NoLMS: none, 0;
	flagdef Allow_With_Respawn_Invul: none, 0;
	flagdef BFG: none, 0;
	flagdef Explosive: none, 0;

	Default
	{
		Inventory.PickupSound "misc/w_pkup";
		WeaponBase.DefaultKickback;
		WeaponBase.BobSpeed 1.0;
		WeaponBase.BobRangeX 1.0;
		WeaponBase.BobRangeY 1.0;
		WeaponBase.WeaponScaleX 1.0;
		WeaponBase.WeaponScaleY 1.2;
		WeaponBase.SlotNumber -1;
		WeaponBase.SlotPriority 32767;
		WeaponBase.BobPivot3D (0.0, 0.0, 0.0);
		WeaponBase.UseRange 48;
		+WEAPONSPAWN
		DefaultStateUsage SUF_ACTOR|SUF_OVERLAY|SUF_WEAPON;
	}
	States
	{
	LightDone:
		SHTG E 0 A_Light0;
		Stop;
	}

	//===========================================================================
	//
	// Weapon :: MarkPrecacheSounds
	//
	//===========================================================================

	override void MarkPrecacheSounds()
	{
		Super.MarkPrecacheSounds();
		MarkSound(UpSound);
		MarkSound(ReadySound);
	}
	
	virtual int, int CheckAddToSlots()
	{
		if (GetReplacement(GetClass()) == GetClass() && !bPowered_Up)
		{
			return SlotNumber, int(SlotPriority*65536);
		}
		return -1, 0;
	}
	
	virtual State GetReadyState ()
	{
		return FindState('Ready');
	}
	
	virtual State GetUpState ()
	{
		return FindState('Select');
	}

	virtual State GetDownState ()
	{
		return FindState('Deselect');
	}

	virtual State GetAtkState (bool hold)
	{
		State s = null;
		if (hold) s = FindState('Hold');
		if (s == null) s = FindState('Fire');
		return s;
	}
	
	virtual State GetAltAtkState (bool hold)
	{
		State s = null;
		if (hold) s = FindState('AltHold');
		if (s == null) s = FindState('AltFire');
		return s;
	}
	
	virtual void PlayUpSound(Actor origin)
	{
		if (UpSound)
		{
			origin.A_StartSound(UpSound, CHAN_WEAPON);
		}
	}
	
	override String GetObituary(Actor victim, Actor inflictor, Name mod, bool playerattack)
	{
		// Weapons may never return HitObituary by default. Override this if it is needed.
		return Obituary;
	}
	
	action void A_GunFlash(statelabel flashlabel = null, int flags = 0)
	{
		let player = player;

		if (null == player)
		{
			return;
		}
		if (!(flags & GFF_NOEXTCHANGE))
		{
			player.mo.PlayAttacking2 ();
		}

		WeaponBase weapon = invoker == player.OffhandWeapon ? player.OffhandWeapon : player.ReadyWeapon;
		if (weapon == null)
		{
			return;
		}
		state flashstate = null;

		if (flashlabel == null)
		{
			if (weapon.bAltFire)
			{
				flashstate = weapon.FindState('AltFlash');
			}
			if (flashstate == null)
			{
				flashstate = weapon.FindState('Flash');
			}
		}
		else
		{
			flashstate = weapon.FindState(flashlabel);
		}
		player.SetPsprite(PSP_FLASH, flashstate, false, weapon);
	}
	
	//---------------------------------------------------------------------------
	//
	// PROC A_Lower
	//
	//---------------------------------------------------------------------------

	action void ResetPSprite(PSprite psp)
	{
		if (!psp)	return;
		psp.rotation = 0;
		psp.baseScale.x = invoker.WeaponScaleX;
		psp.baseScale.y = invoker.WeaponScaleY;
		psp.scale.x = 1;
		psp.scale.y = 1;
		psp.pivot.x = 0;
		psp.pivot.y = 0;
		psp.valign = 0;
		psp.halign = 0;
		psp.Coord0 = (0,0);
		psp.Coord1 = (0,0);
		psp.Coord2 = (0,0);
		psp.Coord3 = (0,0);
	}

	action void A_Lower(int lowerspeed = 6)
	{
		let player = player;

		if (null == player)
		{
			return;
		}
		let weapon = invoker == player.OffhandWeapon ? player.OffhandWeapon : player.ReadyWeapon;
		if (null == weapon)
		{
			player.mo.BringUpWeapon();
			return;
		}
		let psp = player.GetPSprite(weapon.bOffhandWeapon ? PSP_OFFHANDWEAPON : PSP_WEAPON);
		if (!psp) return;
		if (Alternative || player.cheats & CF_INSTANTWEAPSWITCH)
		{
			psp.y = WEAPONBOTTOM;
		}
		else
		{
			psp.y += lowerspeed;
		}
		if (psp.y < WEAPONBOTTOM)
		{ // Not lowered all the way yet
			return;
		}
		ResetPSprite(psp);
		
		if (player.playerstate == PST_DEAD)
		{ // Player is dead, so don't bring up a pending weapon
			// Player is dead, so keep the weapon off screen
			player.SetPsprite(PSP_FLASH, null);
			psp.SetState(weapon.FindState('DeadLowered'));
			return;
		}
		// [RH] Clear the flash state. Only needed for Strife.
		player.SetPsprite(PSP_FLASH, null);
		player.mo.BringUpWeapon ();
		return;
	}

	//---------------------------------------------------------------------------
	//
	// PROC A_Raise
	//
	//---------------------------------------------------------------------------

	action void A_Raise(int raisespeed = 6)
	{
		let player = player;

		if (null == player)
		{
			return;
		}
		let weapon = invoker == player.OffhandWeapon ? player.OffhandWeapon : player.ReadyWeapon;
		if (weapon == null)
		{
			return;
		}
		if (player.PendingWeapon != WP_NOCHANGE && 
			player.PendingWeapon.bOffhandWeapon == weapon.bOffhandWeapon)
		{
			player.mo.DropWeapon(weapon.bOffhandWeapon);
			return;
		}
		let psp = player.GetPSprite(weapon.bOffhandWeapon ? PSP_OFFHANDWEAPON : PSP_WEAPON);
		if (!psp) return;

		if (psp.y <= WEAPONBOTTOM)
		{
			ResetPSprite(psp);
		}
		psp.y -= raisespeed;
		if (psp.y > WEAPONTOP)
		{ // Not raised all the way yet
			return;
		}
		psp.y = WEAPONTOP;
		
		psp.SetState(weapon.GetReadyState());
		return;
	}

	//============================================================================
	//
	// PROC A_WeaponReady
	//
	// Readies a weapon for firing or bobbing with its three ancillary functions,
	// DoReadyWeaponToSwitch(), DoReadyWeaponToFire() and DoReadyWeaponToBob().
	// [XA] Added DoReadyWeaponToReload() and DoReadyWeaponToZoom()
	//
	//============================================================================

	static void DoReadyWeaponToSwitch (PlayerInfo player, bool switchable, int hand = 0)
	{
		int switchok = hand ? WF_OFFHANDSWITCHOK : WF_WEAPONSWITCHOK;
		int refireok = hand ? WF_OFFHANDREFIRESWITCHOK : WF_REFIRESWITCHOK;
		// Prepare for switching action.
		if (switchable)
		{
			player.WeaponState |= switchok | refireok;
		}
		else
		{
			// WF_WEAPONSWITCHOK is automatically cleared every tic by P_SetPsprite().
			player.WeaponState &= ~refireok;
		}
	}

	static void DoReadyWeaponDisableSwitch (PlayerInfo player, int disable, int hand = 0)
	{
		int disableswitch = hand ? WF_OFFHANDDISABLESWITCH : WF_DISABLESWITCH;
		int refireok = hand ? WF_OFFHANDREFIRESWITCHOK : WF_REFIRESWITCHOK;
		// Discard all switch attempts?
		if (disable)
		{
			player.WeaponState |= disableswitch;
			player.WeaponState &= ~refireok;
		}
		else
		{
			player.WeaponState &= ~disableswitch;
		}
	}

	static void DoReadyWeaponToFire (PlayerPawn pawn, bool prim, bool alt, int hand = 0)
	{
		let player = pawn.player;
		let weapon = hand ? player.OffhandWeapon : player.ReadyWeapon;
		if (!weapon)
		{
			return;
		}

		// Change player from attack state
		if (pawn.InStateSequence(pawn.curstate, pawn.MissileState) ||
			pawn.InStateSequence(pawn.curstate, pawn.MeleeState))
		{
			pawn.PlayIdle ();
		}

		// Play ready sound, if any.
		let psp = player.GetPSprite(hand ? PSP_OFFHANDWEAPON : PSP_WEAPON);
		if (weapon.ReadySound && psp && psp.curState == weapon.FindState('Ready'))
		{
			if (!weapon.bReadySndHalf || random[WpnReadySnd]() < 128)
			{
				pawn.A_StartSound(weapon.ReadySound, CHAN_WEAPON);
			}
		}

		if (hand == 1 && player.WeaponState & WF_TWOHANDSTABILIZED)
		{
			return;
		}

		// Prepare for firing action.
		int prim_state = hand ? WF_OFFHANDREADY : WF_WEAPONREADY;
		int alt_state = hand ? WF_OFFHANDREADYALT : WF_WEAPONREADYALT;
		player.WeaponState |= ((prim ? prim_state : 0) | (alt ? alt_state : 0));
		return;
	}

	static void DoReadyWeaponToBob (PlayerInfo player, int hand = 0)
	{
		let weap = hand ? player.OffhandWeapon : player.ReadyWeapon;
		if (weap)
		{
			// Prepare for bobbing action.
			player.WeaponState |= hand ? WF_OFFHANDBOBBING : WF_WEAPONBOBBING;
			let pspr = player.GetPSprite(hand ? PSP_OFFHANDWEAPON : PSP_WEAPON);
			if (pspr)
			{
				pspr.x = 0;
				pspr.y = WEAPONTOP;
			}
		}
	}

	static int GetButtonStateFlags(int flags, int hand = 0)
	{
		// Rewritten for efficiency and clarity
		int outflags = 0;
		if (flags & WRF_AllowZoom) outflags |= hand ? WF_OFFHANDZOOMOK : WF_WEAPONZOOMOK;
		if (flags & WRF_AllowReload) outflags |= hand ? WF_OFFHANDRELOADOK : WF_WEAPONRELOADOK;
		if (flags & WRF_AllowUser1) outflags |= hand ? WF_OFFHANDUSER1OK : WF_USER1OK;
		if (flags & WRF_AllowUser2) outflags |= hand ? WF_OFFHANDUSER2OK : WF_USER2OK;
		if (flags & WRF_AllowUser3) outflags |= hand ? WF_OFFHANDUSER3OK : WF_USER3OK;
		if (flags & WRF_AllowUser4) outflags |= hand ? WF_OFFHANDUSER4OK : WF_USER4OK;
		return outflags;
	}
	
	action void A_WeaponReady(int flags = 0)
	{
		if (!player) return;
		int hand = invoker == player.OffhandWeapon ? 1 : 0;
														DoReadyWeaponToSwitch(player, !(flags & WRF_NoSwitch), hand);
		if ((flags & WRF_NoFire) != WRF_NoFire)			DoReadyWeaponToFire(player.mo, !(flags & WRF_NoPrimary), !(flags & WRF_NoSecondary), hand);
		if (!(flags & WRF_NoBob))						DoReadyWeaponToBob(player, hand);

		player.WeaponState |= GetButtonStateFlags(flags, hand);														
		DoReadyWeaponDisableSwitch(player, flags & WRF_DisableSwitch, hand);
	}

	//---------------------------------------------------------------------------
	//
	// PROC A_CheckReload
	//
	// Present in Doom, but unused. Also present in Strife, and actually used.
	//
	//---------------------------------------------------------------------------

	action void A_CheckReload()
	{
		let player = self.player;
		if (!player) return;
		let weap = invoker == player.OffhandWeapon ? player.OffhandWeapon : player.ReadyWeapon;
		if (weap != NULL)
		{
			weap.CheckAmmo (weap.bAltFire ? WeaponBase.AltFire : WeaponBase.PrimaryFire, true);
		}
	}
		
	//===========================================================================
	//
	// A_ZoomFactor
	//
	//===========================================================================

	action void A_ZoomFactor(double zoom = 1, int flags = 0)
	{
		let player = self.player;
		if (!player) return;
		let weap = invoker == player.OffhandWeapon ? player.OffhandWeapon : player.ReadyWeapon;
		if (weap != NULL)
		{
			zoom = 1 / clamp(zoom, 0.1, 50.0);
			if (flags & 1)
			{ // Make the zoom instant.
				player.FOV = player.DesiredFOV * zoom;
				player.cheats |= CF_NOFOVINTERP;
			}
			if (flags & 2)
			{ // Disable pitch/yaw scaling.
				zoom = -zoom;
			}
			weap.FOVScale = zoom;
		}
	}

	//===========================================================================
	//
	// A_SetCrosshair
	//
	//===========================================================================

	action void A_SetCrosshair(int xhair)
	{
		let player = self.player;
		if (!player) return;
		let weap = invoker == player.OffhandWeapon ? player.OffhandWeapon : player.ReadyWeapon;
		if (weap != NULL)
		{
			weap.Crosshair = xhair;
		}
	}
	
	//===========================================================================
	//
	// Weapon :: TryPickup
	//
	// If you can't see the weapon when it's active, then you can't pick it up.
	//
	//===========================================================================

	override bool TryPickupRestricted (in out Actor toucher)
	{
		// Wrong class, but try to pick up for ammo
		if (ShouldStay())
		{ // Can't pick up weapons for other classes in coop netplay
			return false;
		}

		bool gaveSome = (NULL != AddAmmo (toucher, AmmoType1, AmmoGive1));
		gaveSome |= (NULL != AddAmmo (toucher, AmmoType2, AmmoGive2));
		if (gaveSome)
		{
			GoAwayAndDie ();
		}
		return gaveSome;
	}

	//===========================================================================
	//
	// Weapon :: TryPickup
	//
	//===========================================================================

	override bool TryPickup (in out Actor toucher)
	{
		State ReadyState = FindState('Ready');
		if (ReadyState != NULL && ReadyState.ValidateSpriteFrame())
		{
			return Super.TryPickup (toucher);
		}
		return false;
	}

	//===========================================================================
	//
	// WeaponBase :: Use
	//
	// Make the player switch to self weapon.
	//
	//===========================================================================

	override bool Use (bool pickup)
	{
		WeaponBase useweap = self;

		// Powered up weapons cannot be used directly.
		if (bPowered_Up) return false;

		// If the player is powered-up, use the alternate version of the
		// weapon, if one exists.
		if (SisterWeapon != NULL &&
			SisterWeapon.bPowered_Up &&
			Owner.FindInventory ("PowerWeaponLevel2", true))
		{
			useweap = SisterWeapon;
		}
		if (Owner.player != NULL)
		{
			WeaponBase weap = useweap.bOffhandWeapon ? Owner.player.OffhandWeapon : Owner.player.ReadyWeapon;
			if (weap != useweap)
			{
				Owner.player.PendingWeapon = useweap;
			}
		}
		// Return false so that the weapon is not removed from the inventory.
		return false;
	}

	//===========================================================================
	//
	// WeaponBase :: Destroy
	//
	//===========================================================================

	override void OnDestroy()
	{
		let sister = SisterWeapon;

		if (sister != NULL)
		{
			// avoid recursion
			sister.SisterWeapon = NULL;
			if (sister != self)
			{ // In case we are our own sister, don't crash.
				sister.Destroy();
			}
		}
		Super.OnDestroy();
	}


	//===========================================================================
	//
	// WeaponBase :: HandlePickup
	//
	// Try to leach ammo from the weapon if you have it already.
	//
	//===========================================================================

	override bool HandlePickup (Inventory item)
	{
		if (item.GetClass() == GetClass())
		{
			if (WeaponBase(item).PickupForAmmo (self))
			{
				item.bPickupGood = true;
			}
			if (MaxAmount > 1) //[SP] If amount<maxamount do another pickup test of the weapon itself!
			{
				return Super.HandlePickup (item);
			}
			return true;
		}
		return false;
	}

	//===========================================================================
	//
	// WeaponBase :: PickupForAmmo
	//
	// The player already has self weapon, so try to pick it up for ammo.
	//
	//===========================================================================

	protected bool PickupForAmmo (WeaponBase ownedWeapon)
	{
		bool gotstuff = false;

		// Don't take ammo if the weapon sticks around.
		if (!ShouldStay ())
		{
			int oldamount1 = 0;
			int oldamount2 = 0;
			if (ownedWeapon.Ammo1 != NULL) oldamount1 = ownedWeapon.Ammo1.Amount;
			if (ownedWeapon.Ammo2 != NULL) oldamount2 = ownedWeapon.Ammo2.Amount;

			if (AmmoGive1 > 0) gotstuff = AddExistingAmmo (ownedWeapon.Ammo1, AmmoGive1);
			if (AmmoGive2 > 0) gotstuff |= AddExistingAmmo (ownedWeapon.Ammo2, AmmoGive2);

			let Owner = ownedWeapon.Owner;
			if (gotstuff && Owner != NULL && Owner.player != NULL)
			{
				if (ownedWeapon.Ammo1 != NULL && oldamount1 == 0)
				{
					PlayerPawn(Owner).CheckWeaponSwitch(ownedWeapon.Ammo1.GetClass());
				}
				else if (ownedWeapon.Ammo2 != NULL && oldamount2 == 0)
				{
					PlayerPawn(Owner).CheckWeaponSwitch(ownedWeapon.Ammo2.GetClass());
				}
			}
		}
		return gotstuff;
	}

	//===========================================================================
	//
	// WeaponBase :: CreateCopy
	//
	//===========================================================================

	override Inventory CreateCopy (Actor other)
	{
		let copy = WeaponBase(Super.CreateCopy (other));
		if (copy != self && copy != null)
		{
			copy.AmmoGive1 = AmmoGive1;
			copy.AmmoGive2 = AmmoGive2;
		}
		return copy;
	}

	//===========================================================================
	//
	// WeaponBase :: CreateTossable
	//
	// A weapon that's tossed out should contain no ammo, so you can't cheat
	// by dropping it and then picking it back up.
	//
	//===========================================================================

	override Inventory CreateTossable (int amt)
	{
		// Only drop the weapon that is meant to be placed in a level. That is,
		// only drop the weapon that normally gives you ammo.
		if (SisterWeapon != NULL && 
			Default.AmmoGive1 == 0 && Default.AmmoGive2 == 0 &&
			(SisterWeapon.Default.AmmoGive1 > 0 || SisterWeapon.Default.AmmoGive2 > 0))
		{
			return SisterWeapon.CreateTossable (amt);
		}
		let copy = WeaponBase(Super.CreateTossable (-1));

		if (copy != NULL)
		{
			// If self weapon has a sister, remove it from the inventory too.
			if (SisterWeapon != NULL)
			{
				SisterWeapon.SisterWeapon = NULL;
				SisterWeapon.Destroy ();
			}
			// To avoid exploits, the tossed weapon must not have any ammo.
			copy.AmmoGive1 = 0;
			copy.AmmoGive2 = 0;
		}
		return copy;
	}

	//===========================================================================
	//
	// WeaponBase :: AttachToOwner
	//
	//===========================================================================

	override void AttachToOwner (Actor other)
	{
		Super.AttachToOwner (other);

		Ammo1 = AddAmmo (Owner, AmmoType1, AmmoGive1);
		Ammo2 = AddAmmo (Owner, AmmoType2, AmmoGive2);
		SisterWeapon = AddWeapon (SisterWeaponType);
		if (Owner.player != NULL)
		{
			if (!Owner.player.GetNeverSwitch() && !bNo_Auto_Switch)
			{
				Owner.player.PendingWeapon = self;
			}
			if (Owner.player.mo == players[consoleplayer].camera)
			{
				StatusBar.ReceivedWeapon (self);
			}
		}
		GivenAsMorphWeapon = false; // will be set explicitly by morphing code
	}

	//===========================================================================
	//
	// WeaponBase :: AddAmmo
	//
	// Give some ammo to the owner, even if it's just 0.
	//
	//===========================================================================

	protected Ammo AddAmmo (Actor other, Class<Ammo> ammotype, int amount)
	{
		Ammo ammoitem;

		if (ammotype == NULL)
		{
			return NULL;
		}

		// [BC] This behavior is from the original Doom. Give 5/2 times as much ammoitem when
		// we pick up a weapon in deathmatch.
		if (( deathmatch && !sv_noextraammo ) && ( gameinfo.gametype & GAME_DoomChex ))
			amount = amount * 5 / 2;

		// extra ammoitem in baby mode and nightmare mode
		if (!bIgnoreSkill)
		{
			amount = int(amount * (G_SkillPropertyFloat(SKILLP_AmmoFactor) * sv_ammofactor));
		}
		ammoitem = Ammo(other.FindInventory (ammotype));
		if (ammoitem == NULL)
		{
			ammoitem = Ammo(Spawn (ammotype));
			ammoitem.Amount = MIN (amount, ammoitem.MaxAmount);
			ammoitem.AttachToOwner (other);
		}
		else if (ammoitem.Amount < ammoitem.MaxAmount || sv_unlimited_pickup)
		{
			ammoitem.Amount += amount;
			if (ammoitem.Amount > ammoitem.MaxAmount && !sv_unlimited_pickup)
			{
				ammoitem.Amount = ammoitem.MaxAmount;
			}
		}
		return ammoitem;
	}

	//===========================================================================
	//
	// WeaponBase :: AddExistingAmmo
	//
	// Give the owner some more ammo he already has.
	//
	//===========================================================================

	protected bool AddExistingAmmo (Inventory ammo, int amount)
	{
		if (ammo != NULL && (ammo.Amount < ammo.MaxAmount || sv_unlimited_pickup))
		{
			// extra ammo in baby mode and nightmare mode
			if (!bIgnoreSkill)
			{
				amount = int(amount * (G_SkillPropertyFloat(SKILLP_AmmoFactor) * sv_ammofactor));
			}
			ammo.Amount += amount;
			if (ammo.Amount > ammo.MaxAmount && !sv_unlimited_pickup)
			{
				ammo.Amount = ammo.MaxAmount;
			}
			return true;
		}
		return false;
	}

	//===========================================================================
	//
	// WeaponBase :: AddWeapon
	//
	// Give the owner a weapon if they don't have it already.
	//
	//===========================================================================

	protected WeaponBase AddWeapon (Class<WeaponBase> weapontype)
	{
		WeaponBase weap;

		if (weapontype == NULL)
		{
			return NULL;
		}
		weap = WeaponBase(Owner.FindInventory (weapontype));
		if (weap == NULL)
		{
			weap = WeaponBase(Spawn (weapontype));
			weap.AttachToOwner (Owner);
		}
		return weap;
	}

	//===========================================================================
	//
	// WeaponBase :: ShouldStay
	//
	//===========================================================================

	override bool ShouldStay ()
	{
		if (((multiplayer &&
			(!deathmatch && !alwaysapplydmflags)) || sv_weaponstay) &&
			!bDropped)
		{
			return true;
		}
		return false;
	}


	//===========================================================================
	//
	// WeaponBase :: EndPowerUp
	//
	// The Tome of Power just expired.
	//
	//===========================================================================

	virtual void EndPowerup ()
	{
		let player = Owner.player;
		if (SisterWeapon != NULL && bPowered_Up)
		{
			SisterWeapon.bOffhandWeapon = self.bOffhandWeapon;
			int hand = SisterWeapon.bOffhandWeapon ? 1 : 0;
			let ready = GetReadyState();
			if (ready != SisterWeapon.GetReadyState())
			{
				if (player.PendingWeapon == NULL ||	player.PendingWeapon == WP_NOCHANGE)
				{
					player.refire = 0;
					if (hand == 0) player.ReadyWeapon = SisterWeapon;
					if (hand == 1) player.OffhandWeapon = SisterWeapon;
					player.SetPsprite(hand ? PSP_OFFHANDWEAPON : PSP_WEAPON, SisterWeapon.GetReadyState());
				}
			}
			else
			{
				let psp = player.FindPSprite(hand ? PSP_OFFHANDWEAPON : PSP_WEAPON);
				let weap = hand ? player.OffhandWeapon : player.ReadyWeapon;
				if (psp != null && psp.Caller == weap && psp.CurState.InStateSequence(ready))
				{
					// If the weapon changes but the state does not, we have to manually change the PSprite's caller here.
					psp.Caller = SisterWeapon;
					if (hand == 0) player.ReadyWeapon = SisterWeapon;
					if (hand == 1) player.OffhandWeapon = SisterWeapon;
				}
				else 
				{
					if (player.PendingWeapon == NULL || player.PendingWeapon == WP_NOCHANGE)
					{
						// Something went wrong. Initiate a regular weapon change.
						player.PendingWeapon = SisterWeapon;
					}
				}
			}
		}
	}

	
	//===========================================================================
	//
	// WeaponBase :: PostMorphWeapon
	//
	// Bring this weapon up after a player unmorphs.
	//
	//===========================================================================

	void PostMorphWeapon ()
	{
		if (Owner == null)
		{
			return;
		}
		let p = owner.player;
		p.PendingWeapon = WP_NOCHANGE;
		if (self.bOffhandWeapon)
		{
			p.OffhandWeapon = self;
		}
		else
		{
			p.ReadyWeapon = self;
		}
		p.refire = 0;

		let pspr = p.GetPSprite(self.bOffhandWeapon ? PSP_OFFHANDWEAPON : PSP_WEAPON);
		if (pspr)
		{
			pspr.y = WEAPONBOTTOM;
			pspr.ResetInterpolation();
			pspr.SetState(GetUpState());
		}
	}

	//===========================================================================
	//
	// WeaponBase :: CheckAmmo
	//
	// Returns true if there is enough ammo to shoot.  If not, selects the
	// next weapon to use.
	//
	//===========================================================================
	
	virtual bool CheckAmmo(int fireMode, bool autoSwitch, bool requireAmmo = false, int ammocount = -1)
	{
		int count1, count2;
		int enough, enoughmask;
		int lAmmoUse1;
        int lAmmoUse2 = AmmoUse2;

		if (sv_infiniteammo || (Owner.FindInventory ('PowerInfiniteAmmo', true) != null))
		{
			return true;
		}
		if (fireMode == EitherFire)
		{
			bool gotSome = CheckAmmo (PrimaryFire, false) || CheckAmmo (AltFire, false);
			if (!gotSome && autoSwitch)
			{
				PlayerPawn(Owner).PickNewWeapon (null, bOffhandWeapon);
			}
			return gotSome;
		}
		let altFire = (fireMode == AltFire);
		let optional = (altFire? bAlt_Ammo_Optional : bAmmo_Optional);
		let useboth = (altFire? bAlt_Uses_Both : bPrimary_Uses_Both);

		if (!requireAmmo && optional)
		{
			return true;
		}
		count1 = (Ammo1 != null) ? Ammo1.Amount : 0;
		count2 = (Ammo2 != null) ? Ammo2.Amount : 0;

		if (bDehAmmo && Ammo1 == null)
		{
			lAmmoUse1 = 0;
		}
		else if (ammocount >= 0)
		{
			lAmmoUse1 = ammocount;
			lAmmoUse2 = ammocount;
		}
		else
		{
			lAmmoUse1 = AmmoUse1;
		}

		enough = (count1 >= lAmmoUse1) | ((count2 >= lAmmoUse2) << 1);
		if (useboth)
		{
			enoughmask = 3;
		}
		else
		{
			enoughmask = 1 << altFire;
		}
		if (altFire && FindState('AltFire') == null)
		{ // If this weapon has no alternate fire, then there is never enough ammo for it
			enough &= 1;
		}
		if (((enough & enoughmask) == enoughmask) || (enough && bAmmo_CheckBoth))
		{
			return true;
		}
		// out of ammo, pick a weapon to change to
		if (autoSwitch)
		{
			PlayerPawn(Owner).PickNewWeapon (null, bOffhandWeapon);
		}
		return false;
	}

		
	//===========================================================================
	//
	// WeaponBase :: DepleteAmmo
	//
	// Use up some of the weapon's ammo. Returns true if the ammo was successfully
	// depleted. If checkEnough is false, then the ammo will always be depleted,
	// even if it drops below zero.
	//
	//===========================================================================

	virtual bool DepleteAmmo(bool altFire, bool checkEnough = true, int ammouse = -1, bool forceammouse = false)
	{
		if (!(sv_infiniteammo || (Owner.FindInventory ('PowerInfiniteAmmo', true) != null)))
		{
			if (checkEnough && !CheckAmmo (altFire ? AltFire : PrimaryFire, false, false, ammouse))
			{
				return false;
			}
			if (!altFire)
			{
				if (Ammo1 != null)
				{
					if (ammouse >= 0 && (bDehAmmo || forceammouse))
					{
						Ammo1.Amount -= ammouse;
					}
					else
					{
						Ammo1.Amount -= AmmoUse1;
					}
				}
				if (bPRIMARY_USES_BOTH && Ammo2 != null)
				{
					Ammo2.Amount -= AmmoUse2;
				}
			}
			else
			{
				if (Ammo2 != null)
				{
					Ammo2.Amount -= AmmoUse2;
				}
				if (bALT_USES_BOTH && Ammo1 != null)
				{
					Ammo1.Amount -= AmmoUse1;
				}
			}
			if (Ammo1 != null && Ammo1.Amount < 0)
				Ammo1.Amount = 0;
			if (Ammo2 != null && Ammo2.Amount < 0)
				Ammo2.Amount = 0;
		}
		return true;
	}


	//---------------------------------------------------------------------------
	//
	// Modifies the drop amount of this item according to the current skill's
	// settings (also called by ADehackedPickup::TryPickup)
	//
	//---------------------------------------------------------------------------
	override void ModifyDropAmount(int dropamount)
	{
		bool ignoreskill = true;
		double dropammofactor = G_SkillPropertyFloat(SKILLP_DropAmmoFactor);
		// Default drop amount is half of regular amount * regular ammo multiplication
		if (dropammofactor == -1) 
		{
			dropammofactor = 0.5;
			ignoreskill = false;
		}

		if (dropamount > 0)
		{
			self.Amount = dropamount;
		}
		// Adjust the ammo given by this weapon
		AmmoGive1 = int(AmmoGive1 * dropammofactor);
		AmmoGive2 = int(AmmoGive2 * dropammofactor);
		bIgnoreSkill = ignoreskill;
	}
	
}

class WeaponGiver : WeaponBase
{
	double AmmoFactor;
	
	Default
	{
		WeaponBase.AmmoGive1 -1;
		WeaponBase.AmmoGive2 -1;
	}
	
	override bool TryPickup(in out Actor toucher)
	{
		DropItem di = GetDropItems();
		WeaponBase weap;

		if (di != NULL)
		{
			Class<WeaponBase> ti = di.Name;
			if (ti != NULL)
			{
				if (master == NULL)
				{
					// save the spawned weapon in 'master' to avoid constant respawning if it cannot be picked up.
					master = weap = WeaponBase(Spawn(di.Name));
					if (weap != NULL)
					{
						weap.bAlwaysPickup = false;	// use the flag of self item only.
						weap.bDropped = bDropped;

						// If our ammo gives are non-negative, transfer them to the real weapon.
						if (AmmoGive1 >= 0) weap.AmmoGive1 = AmmoGive1;
						if (AmmoGive2 >= 0) weap.AmmoGive2 = AmmoGive2;

						// If AmmoFactor is non-negative, modify the given ammo amounts.
						if (AmmoFactor > 0)
						{
							weap.AmmoGive1 = int(weap.AmmoGive1 * AmmoFactor);
							weap.AmmoGive2 = int(weap.AmmoGive2 * AmmoFactor);
						}
						weap.BecomeItem();
					}
					else return false;
				}

				weap = WeaponBase(master);
				bool res = false;
				if (weap != null)
				{
					res = weap.CallTryPickup(toucher);
					if (res)
					{
						GoAwayAndDie();
						master = NULL;
					}
				}
				return res;
			}
		}
		return false;
	}
	
	//---------------------------------------------------------------------------
	//
	// Modifies the drop amount of this item according to the current skill's
	// settings (also called by ADehackedPickup::TryPickup)
	//
	//---------------------------------------------------------------------------

	override void ModifyDropAmount(int dropamount)
	{
		bool ignoreskill = true;
		double dropammofactor = G_SkillPropertyFloat(SKILLP_DropAmmoFactor);
		// Default drop amount is half of regular amount * regular ammo multiplication
		if (dropammofactor == -1) 
		{
			dropammofactor = 0.5;
			ignoreskill = false;
		}

		AmmoFactor = dropammofactor;
		bIgnoreSkill = ignoreskill;
	}
	
	
}

struct WeaponSlots native
{
	native bool, int, int LocateWeapon(class<WeaponBase> weap) const;
	native static void SetupWeaponSlots(PlayerPawn pp);
	native class<WeaponBase> GetWeapon(int slot, int index) const;
	native int SlotSize(int slot) const;
}
