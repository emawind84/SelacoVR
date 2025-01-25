// --------------------------------------------------------------------------
//
// Fist 
//
// --------------------------------------------------------------------------

class Fist : Weapon
{
	Default
	{
		Weapon.SelectionOrder 3700;
		Weapon.Kickback 100;
		Obituary "$OB_MPFIST";
		Tag "$TAG_FIST";
		+WEAPON.WIMPY_WEAPON
		+WEAPON.MELEEWEAPON
		+WEAPON.NOAUTOSWITCHTO
	}
	States
	{
	Ready:
		PUNG A 1 A_WeaponReady;
		Loop;
	Deselect:
		PUNG A 1 A_Lower;
		Loop;
	Select:
		PUNG A 1 A_Raise;
		Loop;
	Fire:
		PUNG B 4;
		PUNG C 4 A_Punch;
		PUNG D 5;
		PUNG C 4;
		PUNG B 5 A_ReFire;
		Goto Ready;
	}
}


//===========================================================================
//
// Code (must be attached to Actor)
//
//===========================================================================

extend class Actor
{	
	action void A_Punch()
	{
		bool playInVR = false;
		int laflags = LAF_ISMELEEATTACK;
		int alflags = 0;
		FTranslatedLineTarget t;

		if (player != null)
		{
			playInVR = player.PlayInVR;
			Weapon weap = invoker == player.OffhandWeapon ? player.OffhandWeapon : player.ReadyWeapon;
			if (weap != null && invoker == weap && stateinfo != null && stateinfo.mStateType == STATE_Psprite)
			{
				alflags |= weap.bOffhandWeapon ? ALF_ISOFFHAND : 0;
				laflags |= weap.bOffhandWeapon ? LAF_ISOFFHAND : 0;
				if (!weap.bDehAmmo && !weap.DepleteAmmo (weap.bAltFire))
					return;
			}
		}

		int damage = random[Punch](1, 10) << 1;

		if (FindInventory("PowerStrength"))
			damage *= 10;

		double ang = angle + Random2[Punch]() * (5.625 / 256);
		double range = MeleeRange + MELEEDELTA;
		double pitch = AimLineAttack (ang, range, null, 0., ALF_CHECK3D | alflags);

		LineAttack (ang, range, pitch, damage, 'Melee', "BulletPuff", laflags, t);

		// turn to face target
		if (!playInVR && t.linetarget)
		{
			A_StartSound ("*fist", CHAN_WEAPON);
			angle = t.angleFromSource;
		}
	}

}