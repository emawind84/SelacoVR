
// Punch Dagger -------------------------------------------------------------

class PunchDagger : StrifeWeapon
{
	Default
	{
		Weapon.SelectionOrder 3900;
		+WEAPON.NOALERT
		Obituary "$OB_MPPUNCHDAGGER";
		Tag "$TAG_PUNCHDAGGER";
	}
	
	States
	{
	Ready:
		PNCH A 1 A_WeaponReady;
		Loop;
	Deselect:
		PNCH A 1 A_Lower;
		Loop;
	Select:
		PNCH A 1 A_Raise;
		Loop;
	Fire:
		PNCH B 4;
		PNCH C 4 A_JabDagger;
		PNCH D 5;
		PNCH C 4;
		PNCH B 5 A_ReFire;
		Goto Ready;
	}

	//============================================================================
	//
	// A_JabDagger
	//
	//============================================================================

	action void A_JabDagger ()
	{
		FTranslatedLineTarget t;
		int damage;

		if (player == null)
		{
			return;
		}

		int alflags = 0;
		int laflags = LAF_ISMELEEATTACK;
		Weapon weapon = invoker == player.OffhandWeapon ? player.OffhandWeapon : player.ReadyWeapon;
		if (weapon != null && weapon.bOffhandWeapon)
		{
			alflags |= ALF_ISOFFHAND;
			laflags |= LAF_ISOFFHAND;
		}
		
		if (FindInventory("SVETalismanPowerup"))
		{
			damage = 1000;
		}
		else
		{
			int power = MIN(10, stamina / 10);
			damage = random[JabDagger](0, power + 7) * (power + 2);

			if (FindInventory("PowerStrength"))
			{
				damage *= 10;
			}
		}

		double ang = angle + random2[JabDagger]() * (5.625 / 256);
		double pitch = AimLineAttack (ang, 80., flags: alflags);
		LineAttack (ang, 80., pitch, damage, 'Melee', "StrifeSpark", laflags, t);

		// turn to face target
		if ((!player.PlayInVR || vanilla_melee_attack) && t.linetarget)
		{
			A_StartSound (t.linetarget.bNoBlood ? sound("misc/metalhit") : sound("misc/meathit"), CHAN_WEAPON);
			angle = t.angleFromSource;
			bJustAttacked = true;
			t.linetarget.DaggerAlert (self);
			player.resetDoomYaw = true;
		}
		else
		{
			A_StartSound ("misc/swish", CHAN_WEAPON);
		}
	}
}	
