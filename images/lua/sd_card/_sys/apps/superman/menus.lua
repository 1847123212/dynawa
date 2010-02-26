--menus for SuperMan
my.globals.menus = {}
my.globals.results = {}

my.globals.menus.root = function()
	local menu = {banner = "SuperMan root"}
	menu.items = {
		{text = "Bluetooth", after_select = {go_to="app_menu:/_sys/apps/bluetooth/"}},
		{text = "File Browser", after_select = {go_to="file_browser"}},
		{text = "Adjust time and date", after_select = {go_to = "adjust_time_date"}},
		{text = "Default font size", after_select = {go_to = "default_font_size"}},
		{text = "Bynari Clock app menu", after_select = {go_to = "app_menu:/apps/clock_bynari/"}},
		{text = "Core app menu (test)", after_select = {go_to = "app_menu:/_sys/apps/core/"}},
		{text = "Apps (not yet)"},
		{text = "Shortcuts (not yet)"},
	}
	return menu
end

local function app_menu2(event)
	if event.reply then
		local menu = event.reply
		menu.proxy = assert(event.sender.app)
		dynawa.event.send{type = "open_my_menu", menu = menu}
	else
		dynawa.event.send{type="open_popup", text="This app has no menu", style = "error"}
	end
end

my.globals.menus.app_menu = function(app_id)
	local app = assert(dynawa.apps[app_id],"There is no active app with id '"..app_id.."'")
	dynawa.event.send{type = "your_menu", receiver = app, reply_callback = app_menu2}
end

my.globals.menus.file_browser = function(dir)
	if not dir then
		dir = "/"
	end
	log("opening dir "..dir)
	local dirstat = dynawa.file.dir_stat(dir)
	local menu = {banner = "File browser: "..dir, items={}}
	if not dirstat then
		table.insert(menu.items,{text="[Invalid directory]"})
	else
		if next(dirstat) then
			for k,v in pairs(dirstat) do
				local txt = k.." ["..v.."]"
				--log("Adding dirstat item: "..txt)
				local location = nil
				if v == "dir" then
					location = "file_browser:"..dir..k.."/"
				end
				table.insert(menu.items,{text=txt,after_select={go_to = location}})
			end
		else
			table.insert(menu,items,{text="[Empty directory]"})
		end
		table.sort(menu.items,function(it1,it2)
			return it1.text < it2.text
		end)
	end
	return menu
end

my.globals.menus.default_font_size = function(dir)
	local dir = dynawa.dir.sys.."fonts/"
	local dirstat = assert(dynawa.file.dir_stat(dir),"Cannot open fonts directory")
	local menu = {banner = "Select default system font:", items = {}}
	for k,v in pairs(dirstat) do
		if type(v) == "number" then
			local font_id = dir..k
			local bitmap,w,h = dynawa.bitmap.text_lines{text = "Quick brown fox jumped over the lazy dog", font = font_id}
			table.insert(menu.items,{bitmap = bitmap, fontsize = h, value = {result = "default_font_changed", font_id = font_id}, 
				after_select = {close_menu = true, popup = "Default system font changed"}})
		end
	end
	table.sort(menu.items, function(a,b)
		return (a.fontsize < b.fontsize)
	end)
	return menu
end

my.globals.results.default_font_changed = function(value)
	local font_id = assert(value.font_id)
	dynawa.settings.default_font = font_id
	dynawa.file.save_settings()
end

my.globals.menus.adjust_time_date = function(what)
	local date = assert(os.date("*t"))
	--log("what = "..tostring(what))
	if not what then
		local menu = {banner = "Adjust time & date", always_refresh = true}
		menu.items = {
			{text = "Day of month: "..date.day, after_select = {go_to="adjust_time_date:day"}},
			{text = "Month: "..date.month, after_select = {go_to="adjust_time_date:month"}},
			{text = "Year: "..date.year, after_select = {go_to="adjust_time_date:year"}},			
			{text = "Hours: "..date.hour, after_select = {go_to="adjust_time_date:hour"}},
			{text = "Minutes: "..date.min, after_select = {go_to="adjust_time_date:min"}},			
		}
		return menu
	end
	local limit = {from=2001, to=2060, name = "year"} --year
	if what=="month" then
		limit = {from = 1, to = 12, name = "month"}
	elseif what=="day" then
		limit = {from = 1, to = 31, name = "day of month"}
	elseif what == "hour" then
		limit = {from = 0, to = 23, name = "hours"}
	elseif what == "min" then
		limit = {from = 0, to = 59, name = "minutes"}
	end
	local menu = {banner = "Please adjust the "..limit.name.." value (this also automatically sets seconds to 00)", items = {}}
	for i = limit.from, limit.to do
		local item = {text = tostring(i), value = {what = what, number = i, result = "adjusted_time_date"}, 
			after_select = {popup = "Value adjusted", go_back = true}}
		table.insert(menu.items,item)
		if i == date[what] then
			menu.active_value = item.value
		end
	end
	return menu
end

my.globals.results.adjusted_time_date = function(event)
	local date = assert(os.date("*t"))
	date[event.what] = event.number
	date.sec = 0
	local secs = assert(os.time(date))
	dynawa.time.set(secs)
end
