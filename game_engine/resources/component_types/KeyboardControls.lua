KeyboardControls = {
	speed = 4,
	jump_power = 300,

	OnStart = function(self)
		self.rb = self.actor:GetComponent("Rigidbody")
	end,

	OnUpdate = function(self)
		local move = 0

		if Input.GetKey("right") then
			move = self.speed
		end
		if Input.GetKey("left") then
			move = -self.speed
		end

		local jump = 0
		local ground_hit = Physics.Raycast(self.rb:GetPosition(), Vector2(0, 1), 1)

		if Input.GetKeyDown("up") or Input.GetKeyDown("space") then
			if ground_hit ~= nil then
				jump = -self.jump_power
			end
		end

		self.rb:AddForce(Vector2(move, jump))
	end
}
