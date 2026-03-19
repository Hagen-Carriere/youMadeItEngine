GameManager = {

	-- TILE LEGEND --
	-- 0 : empty
	-- 1 : wall (kinematic)
	-- 2 : player spawn
	-- 3 : spring pad
	-- 4 : goal zone

	level = {
		{1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
		{1, 0, 0, 0, 0, 0, 0, 4, 4, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 1, 1, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 0, 0, 1, 1, 0, 1},
		{1, 0, 0, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 3, 0, 0, 0, 0, 0, 0, 1},
		{1, 0, 0, 0, 1, 1, 0, 0, 0, 1},
		{1, 0, 2, 0, 0, 0, 0, 0, 0, 1},
		{1, 1, 1, 1, 1, 1, 1, 1, 1, 1},
	},

	OnStart = function(self)
		for row = 1, #self.level do
			for col = 1, #self.level[row] do
				local code = self.level[row][col]
				local pos = Vector2(col, row)

				if code == 2 then
					local p = Actor.Instantiate("Player")
					local rb = p:GetComponent("Rigidbody")
					rb.x = pos.x
					rb.y = pos.y

				elseif code == 1 then
					local w = Actor.Instantiate("KinematicBox")
					local rb = w:GetComponent("Rigidbody")
					rb.x = pos.x
					rb.y = pos.y

				elseif code == 3 then
					local s = Actor.Instantiate("BouncyBox")
					local rb = s:GetComponent("Rigidbody")
					rb.x = pos.x
					rb.y = pos.y

				elseif code == 4 then
					local g = Actor.Instantiate("VictoryBox")
					local rb = g:GetComponent("Rigidbody")
					rb.x = pos.x
					rb.y = pos.y
				end
			end
		end
	end,

	OnUpdate = function(self)

	end
}
