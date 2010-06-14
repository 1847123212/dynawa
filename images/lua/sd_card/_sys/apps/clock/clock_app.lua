app.name = "System Clock"
app.id = "dynawa.clock"
app.window = nil
local fonts, icons

function app:display(bitmap, x, y)
	assert(bitmap)
	assert(x)
	assert(y)
	self.window:show_bitmap_at(bitmap, x, y)
end

function app:small_print(chars, x)
	local width, height = 13, 25
	local y = 127 - height
	for i, char in ipairs(chars) do
		if char >= 0 then --negative char == space
			self:display(fonts.small[char], x, y)
		end
		x = x + width + 2
	end
	return x
end

function app:render_date(time)
	self:small_print({time.wday * 2 + 10, time.wday * 2 + 11}, 1) --day of week
	local day1 = math.floor(time.day / 10)
	local day2 = time.day % 10

	local month1 = 0 --space
	local month2 = time.month % 10
	if time.month >= 10 then
		month1 = 1
	end

	local year1 = math.floor((time.year % 100) / 10)
	local year2 = time.year % 10

	self:small_print({day1, day2, 10}, 42)
	self:small_print({month1, month2, 10}, 78)
	self:small_print({11, year1, year2}, 161 - (3*15))
end

function app:render(time, full)
	if full == "no_time" then
		render_date(time)
		return
	end
	local top = 40
	local sec1 = math.floor(time.sec / 10)
	local sec2 = time.sec % 10
	local mm_hh = full

	self:display(fonts.medium[sec2], 160 - 17, top)
	self:display(fonts.dot,58,40+11)
	self:display(fonts.dot,58,40+31)
	if full or sec2 == 0 then
		self:display(fonts.medium[sec1], 160 - 17 - 18, top)
		if sec1 == 0 then
			mm_hh = true
		end
	end
	
	if mm_hh or full then
		local min1 = math.floor(time.min / 10)
		local min2 = time.min % 10
		local hour1 = math.floor(time.hour / 10)
		local hour2 = time.hour % 10
		self:display(fonts.large[hour1], 0, top)
		self:display(fonts.large[hour2], 27, top)
		self:display(fonts.large[min1], 69, top)
		self:display(fonts.large[min2], 96, top)
		if time.hour + time.min == 0 then --Midnight
			full = true
		end
	end
	
	if full then
		self:render_date(time)
	end
	
end

function app:remove_dots(message)
	--[[if not my.app.in_front then
		return
	end]]
	local black = dynawa.bitmap.new(5,5,0,0,0)
	self.window:show_bitmap_at(black, 58,40+11)
	self.window:show_bitmap_at(black, 58,40+31)
end

function app:tick(message)
	if (self.run_id ~= message.run_id) then
		return
	end
	local sec,msec = dynawa.time.get()
	--local t = WristOS.time.ticks()
	self:render(os.date("*t",sec), message.full_render)
	--log("Ticks: "..WristOS.time.ticks() - t)
	local sec2,msec2 = dynawa.time.get()
	local when = 1000 - msec2
	if message.full_render == "no_time" then
		message.full_render = true
	elseif message.full_render then
		message.full_render = nil
	end
	dynawa.devices.timers:timed_event{delay = when, receiver = self, method = "tick", run_id = self.run_id, full_render = message.full_render}
	if when > 600 then
		dynawa.devices.timers:timed_event{delay = 500, receiver = self, method = "remove_dots"}
	end
end

function app:handle_event_timed_event(event)
	self[event.method](self,event)
end

function app:switching_to_back()
	self.run_id = nil
	Class.App.switching_to_back(self)
end

function app:switching_to_front()
	self.run_id = dynawa.unique_id()
	if not self.window then
		self.window = self:new_window()
		self.window:show_bitmap(dynawa.bitmap.new(160,128))
		self.window:show_bitmap_at(icons,1,1) --#todo
	end		
	self.window:push()
	self:tick{run_id = self.run_id, full_render = true}
end

function app:gfx_init()
	local bmap = assert(dynawa.bitmap.from_png_file(self.dir.."digits.png"))
	fonts={small={},medium={},large={}}
	local b_copy = dynawa.bitmap.copy
	for i=0,9 do
		fonts.large[i] = b_copy(bmap,i*26,70,24,47)
	end
	for i=0,9 do
		fonts.medium[i] = b_copy(bmap,i*18,30,16,31)
	end
	for i=0,25 do
		fonts.small[i] = b_copy(bmap,i*15,0,13,25)
	end
	fonts.dot = b_copy(bmap,0,65,5,5)
	fonts.black = b_copy(bmap,5,65,5,5)
	local bmap = assert(dynawa.bitmap.from_png_file(self.dir.."notify_icons.png"))
	--cut them up
	icons = bmap
end

--[[local function overview()
	local time = os.date("*t")
	
	local hour1 = math.floor(time.hour / 10)
	local hour2 = time.hour % 10

	local min1 = math.floor(time.min / 10)
	local min2 = time.min % 10

	local day1 = math.floor(time.day / 10)
	local day2 = time.day % 10

	local month1 = 0 --space
	local month2 = time.month % 10
	if time.month >= 10 then
		month1 = 1
	end
	
	local width, height = 13, 25
	local chars = {hour1, hour2, -1, min1, min2, -1, day1, day2, 10, month1, month2}
	local bg = dynawa.bitmap.new(156, height + 2, 0, 0, 0, 0)
	local b_comb = dynawa.bitmap.combine
	for i, char in ipairs(chars) do
		if char >= 0 then
			b_comb(bg, fonts.small[char], (i-1) * (width + 1) + 1, 1)
		end
	end
	return bg
end]]

function app:start()
	self:gfx_init()
end
--self:new_page()
--self:receive_message_types{"you_are_now_in_front","you_are_now_in_back"}
--dynawa.message.receive {message="your_overview", callback=overview}

