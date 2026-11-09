-- Map-generic WC3 Lua runtime for the differential sweep.
-- Derived from .claude/tools/script-check/pre.lua (handles, timers, scheduler, type checks).
-- Log record format (tab separated): KIND, ctx, t, text.  KIND: C call, W warning, E error, I info.
__log={} local logn=0 __dropped=0
local LOGCAP=__CFG.logcap or 600000
local CTXCAP=__CFG.ctxcap or 6000
local ctxcount={} __ctxdropped={}
local rawtype=type
__rawtype=rawtype
local function rawlog(s) if logn<LOGCAP then logn=logn+1 __log[logn]=s else __dropped=__dropped+1 end end
__now=0 __ticks=0
-- determinism: the map sees simulated time only; caps use the real CPU clock
local realclock=os.clock
os.clock=function() return __now end
os.time=function() return 1700000000+math.floor(__now) end
os.date=function(f) return "2023-11-14 22:13:20" end

-- context: innermost wrapped map function per coroutine; anonymous coroutines inherit "<spawner>~async"
local CTX=setmetatable({},{__mode="k"})
local BASE=setmetatable({},{__mode="k"})
__mainctx="setup"
local function curctx()
  local co,ismain=coroutine.running()
  local s=CTX[co] if s and #s>0 then return s[#s] end
  if ismain then return __mainctx end
  return BASE[co] or "?"
end
__curctx=curctx
-- error handler: append the war3map.lua line chain once, at the deepest wrapped frame
function __eh(e)
  if rawtype(e)~="string" or e:find("[stack ",1,true) then return e end
  local tb={} for ln in debug.traceback("",2):gmatch("war3map.lua:(%d+):") do if #tb<8 then tb[#tb+1]=ln end end
  if #tb>0 then e=e.." [stack "..table.concat(tb,"<").."]" end
  return e
end
function __wrapctx(name,f)
  return function(...)
    local co=coroutine.running() local s=CTX[co] if not s then s={} CTX[co]=s end
    local n=#s+1 s[n]=name
    local r=table.pack(xpcall(f,__eh,...))
    for i=#s,n,-1 do s[i]=nil end
    if not r[1] then error(r[2],0) end
    return table.unpack(r,2,r.n)
  end
end
local function logrec(kind,text)
  local c=curctx()
  if kind=="C" then local n=(ctxcount[c] or 0)+1 ctxcount[c]=n
    if n>CTXCAP then __ctxdropped[c]=(__ctxdropped[c] or 0)+1 return end end
  rawlog(kind.."\t"..c.."\t"..string.format("%.2f",__now).."\t"..text)
end
__logrec=logrec
function __logf(s) logrec("I",s) end

-- handles: label = type@creatingctx#n (stable across scripts that create other handles elsewhere)
local hid=0 local hcount={}
__HMT={__tostring=function(h) return h.t..": "..h.lbl end}
function type(v) if rawtype(v)=="table" and getmetatable(v)==__HMT then return "userdata" end return rawtype(v) end
function __H(t,o) hid=hid+1 o=o or {} o.t=t o.hid=hid
  if not o.lbl then local k=t.."@"..curctx() local n=(hcount[k] or 0)+1 hcount[k]=n o.lbl=k.."#"..n end
  return setmetatable(o,__HMT) end
local function ish(v) return rawtype(v)=="table" and getmetatable(v)==__HMT end
__ish=ish
__players={}
local prim={integer=1,real=1,boolean=1,string=1,code=1,nothing=1}
local nilok={boolexpr=1,conditionfunc=1,filterfunc=1,code=1,string=1,handle=1,agent=1}

local warned={}
function __warn(msg,kind)
  local tb=debug.traceback("",3) local ln=tb:match("war3map.lua:(%d+)") or "?"
  local c=curctx() local k=msg.."|"..c.."|"..ln if warned[k] then return end warned[k]=1
  rawlog("W\t"..c.."\t"..string.format("%.2f",__now).."\t"..(kind or "WARN").." line "..ln..": "..msg)
end
local function chk(name,params,...)
  for i,p in ipairs(params) do local v=select(i,...)
    local bad
    if p=="integer" then bad = v==nil or math.type(v)~="integer"
    elseif p=="real" then bad = rawtype(v)~="number"
    elseif p=="string" then bad = v~=nil and rawtype(v)~="string"
    elseif p=="boolean" then bad = rawtype(v)~="boolean"
    elseif p=="code" then bad = v~=nil and rawtype(v)~="function"
    else
      if v==nil and not nilok[p] then __warn(name.." arg"..i.." ("..p..") is nil","NILH") end
      bad = v~=nil and not ish(v) end
    if bad then __warn(name.." arg"..i.." expects "..p.." got "..type(v).."("..tostring(v)..")","TYPE") end
  end
end
__chk=chk
local function dflt(ret)
  if ret=="integer" then return 0 elseif ret=="real" then return 0.0 elseif ret=="boolean" then return false
  elseif ret=="string" then return "" elseif ret=="nothing" or ret=="code" then return nil else return __H(ret) end
end
__dflt=dflt
__SIG={}  -- native/bj name -> param type list
function __mkfn(name,ret,ps) local params={} for p in string.gmatch(ps,"[^,]+") do params[#params+1]=p end
  __SIG[name]=params
  rawset(_G,name,function(...) return dflt(ret) end) end
function __jarray(d) return setmetatable({},{__index=function() return d end}) end
function __mkglob(typ,arr,name,val)
  if arr then rawset(_G,name, prim[typ] and __jarray(dflt(typ)) or {}) return end
  local v
  if typ=="integer" then v=math.tointeger(tonumber(val)) or 0 elseif typ=="real" then v=tonumber(val) or 0.0
  elseif typ=="boolean" then v=(val=="true") elseif typ=="string" then v=val:match("^\"(.*)\"$") or ""
  else v=__H(typ,{lbl=name}) end  -- blizzard.j null handles are set by InitBlizzardGlobals in the game
  rawset(_G,name,v)
end

-- scheduler with wall-clock / instruction caps (hook per coroutine)
local Q={} local seq=0
__deadline=realclock()+(__CFG.wall or 120) __icount=0 __imax=__CFG.imax or 4000 -- units of 1e6 instructions
__capped=false
local function hook()
  __icount=__icount+1
  if realclock()>__deadline then __capped="wall" error("SIMCAP wall-clock cap",2) end
  if __icount>__imax then __capped="instr" error("SIMCAP instruction cap",2) end
end
function __at(d,f) seq=seq+1 Q[#Q+1]={t=__now+(d or 0),f=f,s=seq} end
local function step(co,label)
  local saved=__innat __innat=false
  local ok,err=coroutine.resume(co)
  __innat=saved
  if not ok then
    local c=BASE[co] or label
    local msg=tostring(err) msg=msg:gsub("\n.*","")
    rawlog("E\t"..c.."\t"..string.format("%.2f",__now).."\t"..label..": "..msg)
  end
  if coroutine.status(co)=="suspended" then local d=__pending or 0 __pending=nil __at(d,function() step(co,label) end) end
end
function __run(f,label,base)
  if f==nil then rawlog("E\t"..label.."\t0\t"..label.." is nil") return end
  local co=coroutine.create(f)
  local pc=curctx() BASE[co]=base or (pc:gsub("~async$","").."~async")
  debug.sethook(co,hook,"",1000000)
  step(co,label)
end
function __spawn(f,label) __run(f,label) end
function __sleep(d) if coroutine.isyieldable() then __pending=d local s=__innat __innat=false coroutine.yield() __innat=s end end
function __loop(limit)
  while #Q>0 and __ticks<(__CFG.maxticks or 300000) do
    if realclock()>__deadline then __capped="wall" rawlog("I\tsim\t"..__now.."\tSIMCAP wall-clock cap in loop") break end
    if __icount>__imax then __capped="instr" rawlog("I\tsim\t"..__now.."\tSIMCAP instruction cap in loop") break end
    __ticks=__ticks+1
    local bi=1 local b=Q[1]
    for i=2,#Q do local e=Q[i] if e.t<b.t or (e.t==b.t and e.s<b.s) then b=e bi=i end end
    if b.t>limit then break end
    table.remove(Q,bi) __now=b.t b.f()
  end
end

-- undefined globals / auto stubs
local auto={}
setmetatable(_G,{__index=function(t,k)
  if rawtype(k)~="string" then return nil end
  if k:match("^Blz") or k:match("^Set%u%a+$") or k:match("^Get%u%a+$") or k:match("^Create%u%a+$") or k:match("^Is%u%a+$") then
    if not auto[k] then __warn("auto-stub native "..k,"AUTOSTUB") local f
      if k:match("^Is%u") or k:match("^BlzIs%u") then f=function() return false end else f=function() return __H("auto") end end
      auto[k]=__wrapnat and __wrapnat(k,{},f) or f end return auto[k] end
  if k:match("^[A-Z][A-Z0-9_]+$") and not k:match("^QM_") and not k:match("^FK_") then
    if not auto[k] then __warn("auto-stub constant "..k,"AUTOSTUB") local h=5381 for i=1,#k do h=(h*33+k:byte(i))%65521 end auto[k]=65536+h end return auto[k] end  -- unknown (newer-than-common.j) constants become name-derived integers
  if k~="Debug" and not k:match("^__") then __warn("read of undefined global "..k,"UNDEF") end
  return nil end})

-- deterministic pairs(): Lua 5.3 seeds its string hash per run, so raw next() order differs between runs.
-- Keys are visited sorted (numbers, strings, booleans, handles by creation order); other key types last.
local rawnext=next
local rank={number=1,string=2,boolean=3}
local oid=setmetatable({},{__mode="k"}) local on=0
local function okey(k) if ish(k) then return 4,k.hid end local r=rank[rawtype(k)] if r then return r,k end
  if not oid[k] then on=on+1 oid[k]=on end return 5,oid[k] end
local function cmp(a,b) local ra,va=okey(a) local rb,vb=okey(b)
  if ra~=rb then return ra<rb end if ra==3 then return (not va) and vb end return va<vb end
local snap=setmetatable({},{__mode="k"})
function next(t,k)
  local s,start
  if k==nil then
    if rawnext(t)==nil then return nil end
    local ks={} local n=0
    for key in rawnext,t do n=n+1 ks[n]=key end
    if n>1 then table.sort(ks,cmp) end
    local idx={} for j=1,n do idx[ks[j]]=j end
    s={ks=ks,idx=idx} snap[t]=s start=1
  else
    s=snap[t] local j=s and s.idx[k]
    if not j then return rawnext(t,k) end
    start=j+1
  end
  local ks=s.ks
  for j=start,#ks do local key=ks[j] local v=rawget(t,key) if v~=nil then return key,v end end
  return nil
end
local detnext=next
function pairs(t)
  local mt=getmetatable(t)
  if mt and mt.__pairs then return mt.__pairs(t) end
  return detnext,t,nil
end
