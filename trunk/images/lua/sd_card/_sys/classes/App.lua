local class = Class("App")
class.is_app = true

function class:_init(id)
	self.id = assert(id)
end

function class:start(id)
	log("Start method not defined for "..self)
end

function class:new_window()
	local win = Class.Window()
	win.app = self
	return win
end

function class:new_menuwindow(menudef)
	local win = Class.Window()
	win.app = self
	local menu = Class.Menu(menudef)
	menu.window = win
	win.menu = menu
	return win
end

function class:push_window(window)
	assert(window.is_window,"Not a window")
	window.app = self
	return dynawa.window_manager:push(window)
end

function class:_del()
	error("Attempt to delete an App: "..self)
end

--[[function class:window_in_front(window)
end]]

function class:switching_to_front()
end

function class:switching_to_back()
	local win = dynawa.window_manager:pop_and_delete_menuwindows()
	assert(win:pop().app == self, "The first popped non-menu window does not belong to this app.")
	local peek = dynawa.window_manager:peek()
	assert (not peek or peek.app ~= self, "After popping one non-menu window, there are still other windows of mine on stack")
end

function class:handle_event_button()
end

function class:handle_event_do_menu()
end

function class:menu_cancelled(menu)
	local win = menu.window:pop()
	win:_delete()
end

function class:load_data()
	return dynawa.file.load_data(self.dir.."my.data")
end

function class:save_data(data)
	return dynawa.file.save_data(data, self.dir.."my.data")
end

return class

