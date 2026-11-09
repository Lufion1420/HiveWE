-- Call logging. Wraps every common.j/blizzard.j name (stub or implementation) once runtime is set up.
local rawtype=__rawtype local ish=__ish local logrec=__logrec local chk=__chk
__FN={}  -- function -> map function name (for code args)
local function fmt(v,p)
  local tv=rawtype(v)
  if v==nil then return "nil" end
  if tv=="number" then
    if p=="real" then return string.format("%.3f",v) end
    if math.type(v)=="integer" then
      if v>=0x20202020 and v<=0x7e7e7e7e then local s=string.pack(">I4",v) if s:match("^[%w_]+$") then return "'"..s.."'" end end
      return tostring(v) end
    return string.format("%.3f",v)
  end
  if tv=="string" then if #v>160 then v=v:sub(1,160).."..." end return string.format("%q",v):gsub("\n","n") end
  if tv=="boolean" then return tostring(v) end
  if tv=="function" then return "fn:"..(__FN[v] or "anon") end
  if ish(v) then return "<"..v.lbl..">" end
  return tv
end
local function wrapnat(name,params,f)
  return function(...)
    if __innat then return f(...) end
    chk(name,params,...)
    local n=select("#",...) if n<#params then n=#params end
    local parts={} for i=1,n do parts[i]=fmt((select(i,...)),params[i]) end
    logrec("C",name.."("..table.concat(parts,",")..")")
    __innat=true
    local r=table.pack(pcall(f,...))
    __innat=false
    if not r[1] then error(r[2],0) end
    return table.unpack(r,2,r.n)
  end
end
__wrapnat=wrapnat
-- names that are pure helpers / scheduler plumbing: run but do not log
local quiet={}
for _,k in ipairs(__CFG.quiet or {}) do quiet[k]=true end
__NATIVE={}
for name,params in pairs(__SIG) do
  local f=rawget(_G,name)
  if rawtype(f)=="function" then __NATIVE[f]=true
    if not quiet[name] then local w=wrapnat(name,params,f) __NATIVE[w]=true rawset(_G,name,w) end
  end
end

-- after the chunk ran: wrap every global function defined in war3map.lua so calls are attributed to it
function __wrapmap()
  local n=0
  local names={}
  for k,v in pairs(_G) do
    if rawtype(k)=="string" and rawtype(v)=="function" and not __NATIVE[v] and not __SIG[k] and not k:match("^__") then
      local info=debug.getinfo(v,"S")
      if info.source=="=war3map.lua" then names[#names+1]=k end
    end
  end
  for _,k in ipairs(names) do local f=rawget(_G,k) local w=__wrapctx(k,f) __FN[f]=k __FN[w]=k rawset(_G,k,w) n=n+1 end
  return n
end
-- gg_ globals: alias handle label -> variable name (applied in post-processing)
function __aliases()
  local out={}
  for k,v in pairs(_G) do if rawtype(k)=="string" and (k:match("^gg_")) and ish(v) then out[#out+1]=v.lbl.."\t"..k end end
  return out
end
