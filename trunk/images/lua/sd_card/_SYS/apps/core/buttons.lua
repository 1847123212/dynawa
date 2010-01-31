require("dynawa")

local switch

local function receive(event)
	log("Buttons.lua got "..event.type.." "..event.button)
	if event.button=="CONFIRM" and event.type=="button_hold" then
		switch = not switch
		if switch then
			dynawa.event.stop_receiving{event="button_down"}
		else
			dynawa.event.receive{event="button_down", callback=receive}
		end
	end
end

dynawa.event.receive{events={"button_up","button_down","button_hold"}, callback=receive}
