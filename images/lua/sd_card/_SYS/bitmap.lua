--BITMAP system init (also printing)

dynawa.display={flipped=false}
dynawa.display.size={width=160,height=128}

dynawa.bitmap.info = function(bmap)
	assert(type(bmap)=="userdata")
	local peek=dynawa.peek
	local width=peek(2,bmap) + (256*peek(3,bmap))
	local height=peek(4,bmap) + (256*peek(5,bmap))
	
	--log("Bmap bytes:"..res)
	--log("peek(5,bmp) = "..peek(5,bmp))
	return width, height
end

dynawa.bitmap.pixel = function(bmap, x, y)
	local w,h = dynawa.bitmap.info(bmap)
	assert (x >= 0)
	assert (y >= 0)
	assert (x < w)
	assert (y < h)
	local offset = (w * y + x) * 4 + 8
	local peek = dynawa.peek
	--log("offset = "..offset)
	return peek (offset, bmap), peek (offset + 1, bmap), peek (offset + 2, bmap), peek (offset + 3, bmap)
end

dynawa.bitmap.parse_font = function (bmap)
	local white = dynawa.bitmap.new(20,20,255,255,255)
	local mask = assert(dynawa.bitmap.mask)
	local chars={}
	local widths={}
	local char=32
	local x=0
	local lastx=-1
	local total_width,height = dynawa.bitmap.info(bmap)
	local done = false
	repeat
		x=x+1
		local r,g,b,a = dynawa.bitmap.pixel(bmap,x,0)
		if r+g+b+a == 1020 then
			--log ("Char "..char.." x="..x)
			local width = x-lastx-1
			assert(width >= 1)
			--log("Char dimensions: "..width.."x"..height)
			local char_str = string.char(char)
			local char_bmp = dynawa.bitmap.copy(bmap,lastx+1,0,width,height)
			char_bmp = mask(char_bmp,white,0,0)
			chars[char_str] = char_bmp
			widths[char_str] = width
			lastx = x
			char = char + 1
			if char % 8 == 0 then
				boot_anim()
			end
			if char > 128 or x > total_width then error("FUCK") end
			r,g,b,a = dynawa.bitmap.pixel(bmap,x,1)
			if r+g+b+a == 1020 then
				done = true
			end
		end
	until done
	assert (char==128)
	return {chars=chars,widths=widths,height=height}
end

--[[
--Load and parse font
dynawa.bitmap.default_font = dynawa.bitmap.parse_font(dynawa.bitmap.from_png_file("/_sys/fonts/default10.png"))

-- Transparent bitmap for printing
local transparent_background = dynawa.bitmap.new(160,128,0,0,0,0)

-- Printing characters
dynawa.bitmap.text_lines = function(lines,font)
	assert(type(lines)=="string","First parameter is not string")
	font = font or assert(dynawa.bitmap.default_font)
	local x,y=0,0
	local combine = dynawa.bitmap.combine
	local result
	local start = true
	local max_x = 0
	(lines.."\n"):gsub("(.-)\n",function(line)
		--log("line="..line)
		for i=1, #line do
			local chr = line:sub(i,i)
			if start then
				start = false
				result = combine(transparent_background,font.chars[chr],0,0,true)
			else
				combine(result,font.chars[chr],x,y)
			end
			x = x + font.widths[chr] + 1
		end
		y = y + font.height
		--log("x is "..x)
		if x > max_x then
			max_x = x
		end
		x = 0
	end)
	--log ("Dimensions: "..(max_x-1).."x"..y)
	return dynawa.bitmap.copy(result,0,0,max_x-1,y)
end

local screen = dynawa.bitmap.new(160,128,0,0,0)
local text = [=[
TCH1 from Dynawa is the
first completely customizable
wrist computer system with
full hardware documentation
and open source code.
This micro-computer is
equipped with RISC processor,
expandable storage, full-
color display, Bluetooth radio,
accelerometer, micro-USB
connector and sound output.
TCH1 is primarily intended to
be used as a smart watch /
terminal communicating with
your mobile phone via Bluetooth.
]=]
dynawa.bitmap.combine(screen,dynawa.bitmap.text_lines(text),0,0)
dynawa.bitmap.show(screen)
]]
