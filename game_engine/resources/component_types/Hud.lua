Hud = {
	seconds_elapsed = 0,
	finish = false,

	OnStart = function(self)
		Event.Subscribe("event_victory", self, self.OnVictory)
	end,

	OnUpdate = function(self)
		if Application.GetFrame() > 0 and Application.GetFrame() % 60 == 0 and self.finish == false then
			self.seconds_elapsed = self.seconds_elapsed + 1
		end

		local msg = "Time : " .. self.seconds_elapsed

		if self.finish then
			msg = "FINISH! Time : " .. self.seconds_elapsed .. " seconds!"
		end

		Text.Draw(msg, 10, 10, "NotoSans-Regular", 24, 0, 0, 0, 255)
	end,

	OnVictory = function(self)
		self.finish = true
	end
}
