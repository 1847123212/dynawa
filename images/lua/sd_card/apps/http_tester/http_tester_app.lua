app.name = "HTTP Tester"
app.id = "dynawa.http_tester"

--When this app runs, it runs EVEN WHEN NOT IN FRONT!!!
function app:display(bitmap, x, y)
	assert(bitmap and x and y)
	self.window:show_bitmap_at(bitmap, x, y)
end

function app:switching_to_back()
	self.window:pop()
end

function app:switching_to_front()
	self.window:push()
	end

function app:make_request(id)
	if not self.running then
		return
	end
	local srv_data = assert(self.servers[id])
	local request = {address = srv_data.server}
	request.callback = function(result)
		self:response(result,id)
	end
	if id == "maps" then
		local x = 51.477222 + math.random()
		local y = 0 - math.random()
		local url = "/maps/api/staticmap?center="..x..","..y.."&zoom=14&size=160x110&sensor=false"
		request.path = url
		request.address, request.path = "www.fuxoft.cz","/smap2.png"
	end
	local http_app = dynawa.app_manager:app_by_id("dynawa.http_request")
	if not http_app then
		log("HTTP Request app not available!")
		return
	end
	http_app:make_request(request)
	self:indicator(srv_data.index,"waiting")
end

function app:handle_event_timed_event(ev)
	if not self.running then
		return
	end
	assert(ev.make_request)
	self:make_request(ev.make_request)
end

function app:response(response,id)
	log("Got response from "..id)
	local server = assert(self.servers[id])
	if id == "#todo maps" then --All of this is temporary hack until Dyno transfer gets fixed.
		log("PNG has "..#(response.body).." bytes")
		if #response.body < 100 then
			for i=1,#response.body do
				log("byte at $"..string.format("%02x",i).." = $"..string.format("%02x",string.byte(response.body:sub(i))))
			end
		end
		local bmp = assert(dynawa.bitmap.from_png(response.body),"Cannot parse PNG")
		self.window:show_bitmap_at(bmp,0,0)
	end
	if response.status == "200 OK" then
		self:indicator(server.index,"ok")
	else
		log("Bad response for "..id..": "..tostring(response.status))
		self:indicator(server.index,"error")
	end
	if not self.running then
		return
	end
	dynawa.devices.timers:timed_event{delay = 3000, receiver = self, make_request = id}
end

function app:indicator(index,block)
	assert (index > 0)
	local x = index * 15 -15
	local y = 128-10
	local bmp = assert(self.blocks[block])
	self.window:show_bitmap_at(bmp,x,y)
end

function app:start_requests()
	local servers = {}
	self.servers = servers
	servers.maps = {server = "maps.google.com", index = 1}
--	servers.yahoo = {server = "www.yahoo.com", index = 2}
--	servers.kompost = {server = "www.kompost.cz", index = 3}
--	servers.fuxoft = {server = "www.fuxoft.cz", index = 4}
--	servers.novinky = {server = "www.novinky.cz", index = 5}
	self.running = true
	for id, tbl in pairs(self.servers) do
		self:make_request(id)
	end
end

function app:stop_requests()
	self.running = nil
end

function app:going_to_sleep()
	return "remember"
end

function app:start()
	self:gfx_init()
	local txtbmp = dynawa.bitmap.text_line("HTTP Request tester","/_sys/fonts/default10.png") 
	self.window:show_bitmap_at(txtbmp,0,0) 
	local txtbmp = dynawa.bitmap.text_line("TOP=Start, BOTTOM=Stop","/_sys/fonts/default10.png") 
	self.window:show_bitmap_at(txtbmp,0,15)
end

function app:handle_event_button(event)
	if event.action == "button_down" and event.button == "bottom" then
		if self.running then
			self:stop_requests()
		end
	end
	if event.action == "button_down" and event.button == "top" then
		if not self.running then
			self:start_requests()
		end
	end
	getmetatable(self).handle_event_button(self,event) --Parent's handler
end

function app:gfx_init()
	self.window = self:new_window()
	self.window:fill()
	self.blocks = {}
	local x,y = 10,10
	self.blocks.error = dynawa.bitmap.new(x,y,255,0,0)
	self.blocks.ok = dynawa.bitmap.new(x,y,0,255,0)
	self.blocks.waiting = dynawa.bitmap.new(x,y,255,255,0)
	self.blocks.clear = dynawa.bitmap.new(x,y,0,0,0)
end

