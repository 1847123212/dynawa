local function open_popup(event)
	local bgbmp = event.background
	if type(bgbmp) == "table" then
		bgbmp = dynawa.bitmap.new(dynawa.display.size.width, dynawa.display.size.height, unpack(bgbmp))
	end
	if not bgbmp then
		bgbmp = event.sender.app.screen
		log("Sender screen = "..tostring(event.sender.screen))
	end
	if not bgbmp then
		bgbmp = dynawa.bitmap.new(dynawa.display.size.width, dynawa.display.size.height, 0,0,0)
	end
	my.globals.sender = event.sender.app
	dynawa.event.send {type = "display_bitmap", bitmap = bgbmp}
	dynawa.event.send {type = "me_to_front"}
	
	local textbmp = dynawa.bitmap.text_line(event.text,nil)
	local txtw,txth = dynawa.bitmap.info(textbmp)
	local w,h = txtw + 8, txth + 8
	local bmp = dynawa.bitmap.new(w,h, 0,40,0)
	dynawa.bitmap.border(bmp,2,{40,255,40})
	dynawa.bitmap.border(bmp,1,{0,0,0})
	dynawa.bitmap.combine(bmp, textbmp, 4, 4)
	local start = {math.floor((dynawa.display.size.width - w) / 2), math.floor((dynawa.display.size.height - h) / 2)}
	
	dynawa.event.send {type = "display_bitmap", at = start, bitmap = bmp}
end

local function button(event)
	if event.type == "button_down" then
		local sender = my.globals.sender
		assert (sender ~= my.app)
		if sender.screen then
			dynawa.event.send{type = "app_to_front", app=sender}
		else
			dynawa.event.send{type = "default_app_to_front"}
		end
		dynawa.event.send {type = "popup_done"}
		dynawa.event.send {type = "display_bitmap", bitmap = nil}
	end
end

my.app.name = "Popup"
dynawa.event.send{type = "set_flags", flags = {ignore_app_switch = true, ignore_menu_open = true}}
dynawa.event.receive{event="open_popup",callback=open_popup}
dynawa.event.receive{events={"button_down","button_up","button_hold"}, callback = button}

