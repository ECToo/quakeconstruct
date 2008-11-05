function vAdd(v1,v2)
	local out = {}
	if(type(v2) == "table") then
		out.x = v1.x + v2.x
		out.y = v1.y + v2.y
		out.z = v1.z + v2.z
	elseif(type(v2) == "number") then
		out.x = v1.x + v2
		out.y = v1.y + v2
		out.z = v1.z + v2
	end
	return out
end

function vSub(v1,v2)
	local out = {}
	if(type(v2) == "table") then
		out.x = v1.x - v2.x
		out.y = v1.y - v2.y
		out.z = v1.z - v2.z
	elseif(type(v2) == "number") then
		out.x = v1.x - v2
		out.y = v1.y - v2
		out.z = v1.z - v2
	end
	return out
end

function vMul(v1,v2)
	local out = {}
	if(type(v2) == "table") then
		out.x = v1.x * v2.x
		out.y = v1.y * v2.y
		out.z = v1.z * v2.z
	elseif(type(v2) == "number") then
		out.x = v1.x * v2
		out.y = v1.y * v2
		out.z = v1.z * v2
	end
	return out
end

function vAbs(v)
	local out = {}
	out.x = math.abs(v.x)
	out.y = math.abs(v.y)
	out.z = math.abs(v.z)
	return out
end

function Vector(x,y,z)
	x = x or 0
	y = y or 0
	z = z or 0
	return {x=x,y=y,z=z}
end

function Vectorv(tab)
	return {x=tab.x,y=tab.y,z=tab.z}
end