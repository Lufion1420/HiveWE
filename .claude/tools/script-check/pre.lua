__log={} local function log(s) __log[#__log+1]=s end __logf=log
local hid=0
__HMT={__tostring=function(h) return h.t..": 0x"..string.format("%08x",h.hid) end}
local rawtype=type
function type(v) if rawtype(v)=="table" and getmetatable(v)==__HMT then return "userdata" end return rawtype(v) end
__rawtype=rawtype
function __H(t,o) hid=hid+1 o=o or {} o.t=t o.hid=hid return setmetatable(o,__HMT) end
__players={}
local prim={integer=1,real=1,boolean=1,string=1,code=1,nothing=1}
local function chk(name,params,...)
  for i,p in ipairs(params) do local v=select(i,...)
    local bad
    if p=="integer" then bad = v==nil or math.type(v)~="integer"
    elseif p=="real" then bad = type(v)~="number"
    elseif p=="string" then bad = v~=nil and type(v)~="string"
    elseif p=="boolean" then bad = type(v)~="boolean"
    elseif p=="code" then bad = v~=nil and type(v)~="function"
    else bad = v~=nil and not (rawtype(v)=="table" and getmetatable(v)==__HMT) end
    if bad then __warn(name.." arg"..i.." expects "..p.." got "..type(v).."("..tostring(v)..")") end
  end
end
local function dflt(ret)
  if ret=="integer" then return 0 elseif ret=="real" then return 0.0 elseif ret=="boolean" then return false
  elseif ret=="string" then return "" elseif ret=="nothing" or ret=="code" then return nil else return __H(ret) end
end
__dflt=dflt
function __mkfn(name,ret,ps) local params={} for p in string.gmatch(ps,"[^,]+") do params[#params+1]=p end
  rawset(_G,name,function(...) chk(name,params,...) return dflt(ret) end) end
function __mkglob(typ,arr,name,val)
  if arr then rawset(_G,name, prim[typ] and __jarray(dflt(typ)) or {}) return end
  local v
  if typ=="integer" then v=math.tointeger(tonumber(val)) or 0 elseif typ=="real" then v=tonumber(val) or 0.0
  elseif typ=="boolean" then v=(val=="true") elseif typ=="string" then v=val:match("^\"(.*)\"$") or ""
  else v=__H(typ) end
  rawset(_G,name,v)
end
function __jarray(d) return setmetatable({},{__index=function() return d end}) end
local warned={}
function __warn(msg) local tb=debug.traceback("",3) local ln=tb:match("war3map.lua:(%d+)") or "?" local k=msg..ln if warned[k] then return end warned[k]=1 log("WARN line "..ln..": "..msg) end
local Q={} __now=0 __ticks=0 local seq=0
function __at(d,f) seq=seq+1 Q[#Q+1]={t=__now+(d or 0),f=f,s=seq} end
local function step(co,label,first)
  local ok,err=coroutine.resume(co)
  if not ok then log("ERROR in "..label.." @t="..__now..": "..tostring(err).."\n"..debug.traceback(co)) end
  if coroutine.status(co)=="suspended" then local d=__pending or 0 __at(d,function() step(co,label) end) end
end
function __run(f,label) if f==nil then log("ERROR: "..label.." is nil") return end step(coroutine.create(f),label) end
__spawn=__run
function __sleep(d) if coroutine.isyieldable() then __pending=d coroutine.yield() end end
function __loop(limit) while #Q>0 and __ticks<400000 do __ticks=__ticks+1
  table.sort(Q,function(a,b) if a.t==b.t then return a.s<b.s end return a.t<b.t end)
  local e=table.remove(Q,1) if e.t>limit then break end __now=e.t e.f() end end
setmetatable(_G,{__index=function(t,k) if type(k)=="string" and not k:match("^gg_") and not k:match("^udg_") then __warn("read of undefined global "..k) end return nil end})
local mtG=getmetatable(_G)
local auto={}
mtG.__index=function(t,k)
  if type(k)~="string" then return nil end
  if k:match("^Blz") or k:match("^Set%u%a+$") or k:match("^Get%u%a+$") or k:match("^Create%u%a+$") or k:match("^Is%u%a+$") then
    if not auto[k] then __warn("auto-stub native "..k) auto[k]=function() return __H("auto") end end return auto[k] end
  if k:match("^[A-Z][A-Z0-9_]+$") and not k:match("^QM_") and not k:match("^FK_") then
    if not auto[k] then __warn("auto-stub constant "..k) auto[k]=__H("autoconst") end return auto[k] end
  if not k:match("^gg_") and not k:match("^udg_") and k~="Debug" and k~="__cur" then __warn("read of undefined global "..k) end
  return nil end
