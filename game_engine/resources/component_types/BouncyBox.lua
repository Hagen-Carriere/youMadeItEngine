BouncyBox = {

	OnCollisionEnter = function(self, collision)
		if collision.other:GetName() == "player" then
			local rb = collision.other:GetComponent("Rigidbody")
			local vel = rb:GetVelocity()
			rb:SetVelocity(Vector2(vel.x, -12))
		end
	end
}
