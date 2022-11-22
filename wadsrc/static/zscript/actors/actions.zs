extend class Actor
{

	private void CheckStopped()
	{
		let player = self.player;
		if (player && player.mo == self && !(player.cheats & CF_PREDICTING) && Vel == (0, 0, 0))
		{
			player.mo.PlayIdle();
			player.Vel = (0, 0);
		}
	}

	//===========================================================================
	//
	// A_Stop
	// resets all velocity of the actor to 0
	//
	//===========================================================================
	void A_Stop()
	{
		let player = self.player;
		Vel = (0, 0, 0);
		CheckStopped();
	}

	//===========================================================================
	//
	// A_ScaleVelocity
	//
	// Scale actor's velocity.
	//
	//===========================================================================

	void A_ScaleVelocity(double scale, int ptr = AAPTR_DEFAULT)
	{

		let ref = GetPointer(ptr);

		if (ref == NULL)
		{
			return;
		}

		bool was_moving = ref.Vel != (0, 0, 0);

		ref.Vel *= scale;

		// If the actor was previously moving but now is not, and is a player,
		// update its player variables. (See A_Stop.)
		if (was_moving)
		{
			ref.CheckStopped();
		}
	}

	//===========================================================================
	//
	// A_ChangeVelocity
	//
	//===========================================================================

	action void A_ChangeVelocity(double x = 0, double y = 0, double z = 0, int flags = 0, int ptr = AAPTR_DEFAULT)
	{
		let ref = GetPointer(ptr);

		if (ref == NULL)
		{
			return;
		}

		let directionAngle = ref.Angle;
		if (player != NULL && player.mo == ref)
		{
			player.keepmomentum = true;
			let weapon = invoker == player.OffhandWeapon ? player.OffhandWeapon : player.ReadyWeapon;
			if (weapon && weapon == invoker && player.mo.OverrideAttackPosDir)
			{
				if (weapon.bOffhandWeapon)
				{
					directionAngle = player.mo.OffhandAngle + 90;
				}
				else
				{
					directionAngle = player.mo.AttackAngle + 90;
				}
			}
		}

		bool was_moving = ref.Vel != (0, 0, 0);

		let newvel = (x, y, z);
		double sina = sin(directionAngle);
		double cosa = cos(directionAngle);

		if (flags & 1)	// relative axes - make x, y relative to actor's current angle
		{
			newvel.X = x * cosa - y * sina;
			newvel.Y = x * sina + y * cosa;
		}
		if (flags & 2)	// discard old velocity - replace old velocity with new velocity
		{
			ref.Vel = newvel;
		}
		else	// add new velocity to old velocity
		{
			ref.Vel += newvel;
		}

		if (was_moving)
		{
			ref.CheckStopped();
		}
	}

	void A_SpriteOffset(double ox = 0.0, double oy = 0.0)
	{
		SpriteOffset.X = ox;
		SpriteOffset.Y = oy;
	}
}
