app.name = "SuperMan"
app.id = "dynawa.superman"

function app:open_menu_by_url(url)
	local url0, urlarg = url:match("(.+):(.*)")
	if not urlarg then
		url0 = url
	end
	if urlarg == "" then
		urlarg = nil
	end
	local builder = self.menu_builders[url0]
	if not builder then
		error("Cannot get builder for url: "..url)
	end
	local menu = assert(builder(self,urlarg),"Builder didn't return menu nor menu descriptor")
	if not menu.is_menu then
		menu = self:new_menuwindow(menu).menu
	end
	menu.outer_color = {255,99,99}
	menu.url = url
	self:open_menu(menu)
	return menu
end

function app:open_menu(menu)
	assert(menu.window)
	self:push_window(menu.window)
	return menu
end

function app:menu_cancelled(menu)
	local popwin = dynawa.window_manager:pop()
	assert (popwin == menu.window)
	popwin:_delete()
	local window = dynawa.window_manager:peek()
	if window then
		if window.menu:requires_render() then
			log(window.menu .. "requires re-render")
		else
			log(window.menu .. "does not require re-render")
		end
	else
		dynawa.window_manager:show_default()
	end
end

function app:switching_to_front()
	self:open_menu_by_url("root")
end

function app:switching_to_back()
	dynawa.window_manager:pop_and_delete_menuwindows()
end

function app:menu_item_selected(args)
	local menu = args.menu
	assert (menu.window.app == self)
--	log("selected item "..args.item)
	local value = args.item.value
	if not value then
		return
	end
	if value.go_to_url then
		local newmenu = self:open_menu_by_url(value.go_to_url)
		return
	end
end

function app:start()
	dynawa.superman = self
end

app.menu_builders = {}

function app.menu_builders:root()
	local menu_def = {
		banner = {
			text="SuperMan root menu"
			},
		items = {
			{text = "Shortcuts", value = {go_to_url = "shortcuts"}},
			{text = "Apps", value = {go_to_url = "apps"}},
			{text = "File browser", value = {go_to_url = "file_browser"}},
			{text = "Display settings", value = {go_to_url = "adjust_display"}},
			{text = "Time and date settings", value = {go_to_url = "adjust_time_date"}},
		},
	}
	return menu_def
end

function app.menu_builders:adjust_display()
	local menudesc = {
		banner = "Adjust display", items = {
			{text = "Default font size", value = {go_to_url = "default_font_size"}},
			{text = "Display brightness", value = {go_to_url = "display_brightness"}},
			{text = "Display autosleep", selected =
				function()
					local sandman = assert(dynawa.app_manager:app_by_id("dynawa.sandman"))
					sandman:switching_to_front()
				end
			},
		}
	}
	local menu = self:new_menuwindow(menudesc).menu
	return menu
end

function app.menu_builders:display_brightness()
	local choices = {[0] = "Auto brightness", "Min. brightness", "Normal brightness", "Max. brightness"}
	local menudesc = {banner = "Display brightness: "..choices[dynawa.settings.display.brightness], items = {}}
	for i = 0,3 do
		table.insert(menudesc.items,{
			text = choices[i],
			selected = function()
				dynawa.devices.display.brightness(i)
				dynawa.settings.display.brightness = i
				dynawa.file.save_settings()
			end
		})
	end
	return menudesc
end

function app.menu_builders:default_font_size()
	local menudesc = {banner = "Select default font size:", items = {}}
	local font_sizes = {7,10,15}
	for i, size in ipairs(font_sizes) do
		local item = {text = "Quick brown fox jumps over the lazy dog ("..size.." px)",
				value = {font_size = size, font_name = "/_sys/fonts/default"..size..".png"}}
		item.render = function(_self,args)
			return dynawa.bitmap.text_lines{text = _self.text, font = assert(_self.value.font_name), width = assert(args.max_size.w)}
		end
		table.insert(menudesc.items, item)
	end
	menudesc.item_selected = function(self,args)
		dynawa.settings.default_font = args.item.value.font_name
		dynawa.file.save_settings()
		dynawa.popup:info("Default font changed to size "..args.item.value.font_size)
	end
	return menudesc
end

function app.menu_builders:file_browser(dir)
	if not dir then
		dir = "/"
	end
	--log("opening dir "..dir)
	local dirstat = dynawa.file.dir_stat(dir)
	local menu = {banner = "File browser: "..dir, items={}, always_refresh = true, allow_shortcut = "Dir: "..dir}
	if not dirstat then
		table.insert(menu.items,{text="[Invalid directory]"})
	else
		if next(dirstat) then
			for k,v in pairs(dirstat) do
				local txt = k.." ["..v.." bytes]"
				local sort = "2"..txt
				if v == "dir" then 
					txt = "= "..k
					sort = "1"..txt
				end
				--log("Adding dirstat item: "..txt)
				local location = "file:"..dir..k
				if v == "dir" then
					location = "file_browser:"..dir..k.."/"
				end
				table.insert(menu.items,{text = txt, sort = sort, value={go_to_url = location}})
			end
			table.sort(menu.items,function(it1,it2)
				return it1.sort < it2.sort
			end)
		else
			table.insert(menu.items,{text="[Empty directory]"})
		end
	end
	return menu
end

local adjust_time_selected = function(self,args)
	local menu = args.menu
	assert(menu == self)
	local value = assert(args.item.value)

	local date = assert(os.date("*t"))
	date[value.what] = value.number
	if value.what == "min" then
		date.sec = 0
	end
	local secs = assert(os.time(date))
	dynawa.time.set(secs)
	
	menu.window:pop()
	menu:_delete()
	local win = dynawa.window_manager:pop()
	win:_delete()
	dynawa.superman:open_menu_by_url("adjust_time_date")
	local msg = "Adjusted "..value.name
	if value.what == "min" then
		msg = msg.." and set seconds to zero."
	else
		msg = msg.."."
	end
	dynawa.popup:open{text = msg}
end

function app.menu_builders:adjust_time_date(what)
	local date = assert(os.date("*t"))
	--log("what = "..tostring(what))
	if not what then
		local menu = {banner = "Adjust time & date"}
		menu.items = {
			{text = "Day of month: "..date.day, value = {go_to_url="adjust_time_date:day"}},
			{text = "Month: "..date.month, value = {go_to_url="adjust_time_date:month"}},
			{text = "Year: "..date.year, value = {go_to_url="adjust_time_date:year"}},			
			{text = "Hours: "..date.hour, value = {go_to_url="adjust_time_date:hour"}},
			{text = "Minutes: "..date.min, value = {go_to_url="adjust_time_date:min"}},			
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
	local menu = {banner = "Please adjust the "..limit.name.." value", items = {}, item_selected = adjust_time_selected}
	for i = limit.from, limit.to do
		local item = {text = tostring(i), value = {what = what, number = i, name = limit.name}}
		table.insert(menu.items,item)
		if i == date[what] then
			menu.active_value = item.value
		end
	end
	return menu
end

function app.menu_builders:apps()
	local menudesc = {
		banner = "Apps", items = {
			{text = "Running Apps", value = {go_to_url = "apps_running"}},
			{text = "Non-running Apps on SD card", value = {go_to_url = "apps_on_card"}},
			{text = "Auto-starting Apps", value = {go_to_url = "apps_autostart"}},
			{text = "Switchable Apps (cycled using SWITCH button)", value = {go_to_url = "apps_switchable"}},
		}
	}
	return menudesc
end

function app.menu_builders:apps_running()
	local menudesc = {banner = "Running Apps", items = {}}
	local apps = {}
	for id, app in pairs(dynawa.app_manager.all_apps) do
		local name = "> "..id
		if app.name then
			name = name.." ("..app.name..")"
		end
		table.insert(menudesc.items, {text = name, value = {go_to_url = "app:"..id}})
	end
	table.sort(menudesc.items, function (a,b)
		return (a.text < b.text)
	end)
	return menudesc
end

function app.menu_builders:apps_on_card()
	local menudesc = {banner = "Apps on SD card (select to run)", items = {}}
	local apps = dynawa.app_manager:sd_card_apps()
	if not next(apps) then
		menudesc.items = {
			{text = "No non-running Apps found on SD card"}
		}
		return menudesc
	end
	table.sort(apps)
	for i,fname in ipairs(apps) do
		local nicefname = fname:gsub("/"," /")
		table.insert(menudesc.items,{text=">"..nicefname, value = {filename = fname}})
	end
	menudesc.item_selected = function(_self,args)
		local fname = assert(args.item.value.filename)
		local app,app2 = dynawa.app_manager:start_app(fname)
		if not app then
			app = assert(app2)
		end
		self:open_menu_by_url("app:"..app.id)
	end
	return menudesc
end

function app.menu_builders:apps_autostart()
	local menudesc = {banner = "Auto-starting apps", items = {}}
	for i,fname in ipairs(dynawa.app_manager:all_autostarting_apps()) do
		local app = dynawa.app_manager:app_by_filename(fname)
		if app then
			local name = app.name
			if name ~= app.id then
				name = name .. " ("..app.id..")"
			end
			if dynawa.app_manager:is_required(app) then
				name = "(REQUIRED) ".. name
			end
			local item = {text = "> "..name, value = {go_to_url = "app:"..app.id}}
			table.insert(menudesc.items,item)
		else
			log("App "..fname.." is listed as autostarting but is not running")
		end
	end
	if not next(menudesc.items) then
		table.insert(menudesc.items,{text="No autostarting Apps"})
	end
	return menudesc
end

function app.menu_builders:app(id)
	assert(id,"No App id provided")
	local app = dynawa.app_manager:app_by_id(id)
	if not app then
		local menudesc = {banner = "App with id: "..id, items = {}}
		table.insert(menudesc.items, {"This App is not running"})
		return menudesc
	end
	local menudesc = {banner = "App: "..app.name, items = {}}
	table.insert(menudesc.items,{text = "Switch to this app", selected = function(_self,args)
		log("Switching to front")
		app:switching_to_front()
	end})
	table.insert(menudesc.items,{text = "Id: "..app.id})
	local nicefname = app.filename:gsub("/"," /")
	table.insert(menudesc.items,{text = "Filename:"..nicefname})	
	local is_autostarting = dynawa.app_manager:is_autostarting(app)
	local auto = "disabled"
	if dynawa.app_manager:is_autostarting(app) then
		auto = "enabled"
	end
	if dynawa.app_manager:is_required(app) then
		table.insert(menudesc.items,{text = "Autostart: enabled (required App)"})
	else
		table.insert(menudesc.items,{text = "Autostart: "..auto, selected = function(_self,args)
			local menu = assert(args.menu)
			menu.window:pop()
			if auto == "disabled" then --set to autostart
				dynawa.app_manager:enable_autostart(app)
			else -- cancel autostart
				dynawa.app_manager:disable_autostart(app)
			end
			self:open_menu_by_url("app:"..app.id)
		end})
	end
	return menudesc
end

function app.menu_builders:apps_switchable()
	local menudesc = {banner = "Switchable Apps", items = {}}
	for i,fname in ipairs(dynawa.settings.switchable) do
		local app = assert(dynawa.app_manager:app_by_id(fname))
		local name = app.name
		if app.name ~= app.id then
			name = name.." ("..app.id..")"
		end
		table.insert(menudesc.items,{text=name})
	end
	if not next(menudesc.items) then
		table.insert(menudesc.items,{text="No switchable Apps"})
	else
		table.insert(menudesc.items,{text="#todo: re-order & disable", value={val="xxx"}})
	end
	return menudesc
end
