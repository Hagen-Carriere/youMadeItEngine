CameraManager = {
	ease_factor = 0.08,
	locked_on = false,

	OnUpdate = function(self)
		local target = Actor.Find("player")

		if target == nil then
			self.locked_on = false
			return

		elseif self.locked_on == false then
			self.locked_on = true
			local rb = target:GetComponent("Rigidbody")
			local pos = rb:GetPosition()
			Camera.SetPosition(pos.x, pos.y)
			return
		end

		local rb = target:GetComponent("Rigidbody")
		local desired = rb:GetPosition()
		local current = Vector2(Camera.GetPositionX(), Camera.GetPositionY())
		local smoothed = current + (desired - current) * self.ease_factor

		Camera.SetPosition(smoothed.x, smoothed.y)
	end
}
