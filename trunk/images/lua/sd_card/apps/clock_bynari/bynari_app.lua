app.name = "Bynari Clock"
app.id = "dynawa.clock_bynari"

local clock = {}
local gfx = {}
local dot_size = {25,17}

local function random_color()
	return {math.random(200)+55, math.random(200)+55, math.random(200)+55}
end

local function convert_coords(x,y)
	local xx = x * 27
	local yy = y * 19
	if y > 2 then
		yy = yy + 16
	end
	return xx,yy
end

function app:display(bitmap, x, y)
	assert(bitmap and x and y)
	self.window:show_bitmap_at(bitmap, x, y)
end

function app:change_dot(x, y, gf, color)
	local recolor = dynawa.bitmap.new(dot_size[1],dot_size[2],unpack(color))
	local fg = dynawa.bitmap.mask(recolor, gfx[gf], 0,0)
	local dot = dynawa.bitmap.combine(gfx.black, fg, 0,0, true)
	self:display(dot, convert_coords(x,y))
end

function app:text(time)
	local style = self.prefs.style
	local font = "/_sys/fonts/default10.png"
	local ticks = dynawa.bitmap.text_line(tostring(time.raw), font, {0,0,0})
	local mem, w, h = dynawa.bitmap.text_line(collectgarbage("count") * 1024 .." "..time.wday, font, {0,0,0})
	local bgcolor
	if style == "red" then
		bgcolor = {255,0,0}
	elseif style == "white" then
		bgcolor = {255,255,255}
	elseif style == "blue/green" then
		bgcolor = {0,255,0}
		if time.raw % 2 == 0 then
			bgcolor = {0,0,255}
		end
	else
		bgcolor = {200,200,200}
	end
	local bg = dynawa.bitmap.new (160, h, unpack(bgcolor))
	dynawa.bitmap.combine(bg, ticks, 1,1)
	dynawa.bitmap.combine(bg, mem, 159-w,1)
	local y = math.floor(64 - h / 2)
	self:display(bg, 0, y)
end

local function to_bin (num)
	local result = {}
	for i=5,0,-1 do
		result[i] = num % 2
		num = math.floor(num / 2)
	end
	return result
end

function app:update_dots(time, status)
	local style = self.prefs.style
	local new = {}
	local clk = clock.state
	new[0] = to_bin(time.hour)
	new[1] = to_bin(time.min)
	new[2] = to_bin(time.sec)
	new[3] = to_bin(time.day)
	new[4] = to_bin(time.month)
	new[5] = to_bin(math.min(time.year % 100, 63))
	for i = 0, 5 do
		for j = 0, 5 do
			if clk[i][j].gfx ~= new[i][j] then
				local dot = clk[i][j]
				dot.gfx = new[i][j]
				if status ~= "first" then
					if style ~= "blue/green" and style ~= "white" then
						if style == "red" then
							dot.color = {255,0,0}
						else
							dot.color = random_color()
						end
					end
				end
				self:change_dot(j,i,dot.gfx,dot.color)
			end
		end
	end
	local i = math.random(6) - 1
	local j = math.random(6) - 1
	local color
	if style ~= "blue/green" and style ~= "white" then
		if style == "red" then
			local cl = math.random(40)
			color = {math.random(100)+155,cl,cl}
		else
			color = random_color()
		end
		self:change_dot(j,i,clk[i][j].gfx,color)
		clk[i][j].color = color
	end
end

function app:tick(message)
	if message.run_id ~= self.run_id or not self.window.in_front then
		return
	end
	local time_raw, msec = dynawa.time.get()
	local time = os.date("*t", time_raw)
	time.raw = time_raw
	self:update_dots(time,message.status)
	self:text(time)
	local sec, msec = dynawa.time.get()
	local when = 1100 - msec
	dynawa.devices.timers:timed_event{delay = when, receiver = self, run_id = message.run_id}
end

function app:handle_event_timed_event(event)
	self:tick(event)
end

function app:switched_to_front(window)
	if not self.window then
		self.window = self:new_window()
		self.window:show_bitmap(dynawa.bitmap.new(160,128))
	end
	self.window:to_front()
end

function app:window_in_front(window)
	assert(window == self.window)
	self.run_id = dynawa.unique_id()
	self:tick({run_id = self.run_id})
end

function app:init_colors()
	local style = self.prefs.style
	clock = {state={}}
	for i = 0, 5 do
		clock.state[i] = {}
		for j = 0, 5 do
			local color
			if style == "red" then
				color = {150,0,0}
			elseif style == "white" then
				color = {255,255,255}
			elseif style == "blue/green" then
				color = {0,0,255}
				if i >= 3 then
					color = {0,255,0}
				end
			else
				color = random_color()
			end
			clock.state[i][j] = {gfx = "empty", color = color}
			--change_dot(i,j,"empty",color)
		end
	end
end

local function overview()-------------- #todo
	if not my.globals.logo then
		my.globals.logo = assert(dynawa.bitmap.from_png_file(my.dir.."logo.png"))
	end
	return my.globals.logo
end

function app:gfx_init()
	local bmap = assert(dynawa.bitmap.from_png_file(self.dir.."gfx.png"))
	local b_copy = dynawa.bitmap.copy
	gfx[0] = b_copy(bmap, 0,0, unpack(dot_size))
	gfx[1] = b_copy(bmap, 25,0, unpack(dot_size))
	gfx.empty = b_copy(bmap, 50,0, unpack(dot_size))
	gfx.black = dynawa.bitmap.new(unpack(dot_size))
end

local after_select = {popup = "Color scheme changed", close_menu = true}

function app:xxxxxxxxxxxxxxxxxmenu_result(message)
	assert(message.value)
	if self.prefs.style ~= message.value then
		self.prefs.style = message.value
		dynawa.file.save_data(self.prefs)
	end
end

function app:xxxxxxxxxxxxyour_menu (message)
	local menu = {
		callback = menu_result,
		banner = "Bynari color schemes",
		active_value = assert(self.prefs.style),
		items = {
			{
				text = "Rainbow", value = "default", after_select = after_select
			},
			{
				text = "Planet Earth", value = "blue/green", after_select = after_select
			},
			{
				text = "Pure snow", value = "white", after_select = after_select
			},
			{
				text = "Inferno", value = "red", after_select = after_select
			},

			{
				text = "Rainbow", value = "default", after_select = after_select
			},
			{
				text = "Planet Earth", value = "blue/green", after_select = after_select
			},
			{
				text = "Pure snow", value = "white", after_select = after_select
			},
			{
				text = "Inferno", value = "red", after_select = after_select
			},
			{
				text = "Rainbow", value = "default", after_select = after_select
			},
			{
				text = "Planet Earth", value = "blue/green", after_select = after_select
			},
			{
				text = "Pure snow", value = "white", after_select = after_select
			},
			{
				text = "Inferno", value = "red", after_select = after_select
			},
			{
				text = "Rainbow"
			},
			{
				text = "Planet Earth"
			},
			{
				text = "Pure snow", value = "white", after_select = after_select
			},
			{
				bitmap = dynawa.bitmap.new(99,25,255,0,0), value = "red", after_select = after_select
			},
			{
				text = "Rainbow", value = "default", after_select = after_select
			},
			{
				text = "Planet Earth", value = "blue/green", after_select = after_select
			},
			{
				text = "Pure snow", value = "white", after_select = after_select
			},
			{
				text = "Inferno", value = "red", after_select = after_select
			},

		},
	}
	return menu
end

function app:start()
	self.prefs = dynawa.file.load_data(self)
	if not self.prefs or self.prefs.revision < 2 then
		self.prefs =  {style = "default", revision = 2}
	end
	self:gfx_init()
	self:init_colors()
	--dofile(my.dir.."bynari_prefs.lua")
end

