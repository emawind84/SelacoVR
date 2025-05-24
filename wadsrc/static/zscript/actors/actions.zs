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

		bool was_moving = ref.Vel != (0, 0, 0);
		let newvel = (x, y, z);

		if (player != NULL && player.mo == ref)
		{
			if (!vr_recoil) return;
			player.keepmomentum = true;
			let weapon = invoker == player.OffhandWeapon ? player.OffhandWeapon : player.ReadyWeapon;
			if (weapon && weapon == invoker && flags & CVF_RELATIVETOWEAPON && player.mo.OverrideAttackPosDir)
			{
				Vector3 dir;
				Vector3 yoffsetDir;
				Vector3 zoffsetDir;
				if (weapon.bOffhandWeapon)
				{
					dir = player.mo.OffhandDir(self, angle, pitch);
					yoffsetDir = player.mo.OffhandDir(self, angle - 90, pitch);
					zoffsetDir = player.mo.OffhandDir(self, angle, pitch + 90);
				}
				else
				{
					dir = player.mo.AttackDir(self, angle, pitch);
					yoffsetDir = player.mo.AttackDir(self, angle - 90, pitch);
					zoffsetDir = player.mo.AttackDir(self, angle, pitch + 90);
				}

				newvel = (0, 0, 0);
				newvel += (
					x * cos(dir.x) * cos(dir.y),
					x * sin(dir.x) * cos(dir.y),
					x * -sin(dir.y)
				);

				newvel += (
					y * cos(yoffsetDir.x) * cos(yoffsetDir.y),
					y * sin(yoffsetDir.x) * cos(yoffsetDir.y),
					y * -sin(yoffsetDir.y)
				);

				newvel += (
					z * cos(zoffsetDir.y) * cos(zoffsetDir.x), 
					z * cos(zoffsetDir.y) * sin(zoffsetDir.x),
					z * -sin(zoffsetDir.y)
				);
			}
		}
		double sina = sin(ref.Angle);
		double cosa = cos(ref.Angle);
		if (flags & CVF_RELATIVE)	// relative axes - make x, y relative to actor's current angle
		{
			newvel.X = x * cosa - y * sina;
			newvel.Y = x * sina + y * cosa;
		}
		if (flags & CVF_REPLACE)	// discard old velocity - replace old velocity with new velocity
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
