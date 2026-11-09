-- Native implementations that need state. Derived from script-check/impl.lua + ev.lua, Forgotten Hollow bits removed.
local H=__H
function Player(i) if not __players[i] then __players[i]=H("player",{id=i,lbl="player#"..tostring(i)}) end return __players[i] end
function GetPlayerId(p) if p==nil then return 0 end return p.id or 0 end
function GetLocalPlayer() return Player(0) end
function I2S(i) return string.format("%d",math.tointeger(i) or math.floor(i)) end
function R2S(r) return string.format("%.3f",r) end
function R2SW(r,w,p) return string.format("%"..w.."."..p.."f",r) end
function S2I(s) return math.tointeger(tonumber(s) or 0) or 0 end
function S2R(s) return tonumber(s) or 0.0 end
function I2R(i) return i+0.0 end
function R2I(r) return math.tointeger(r>=0 and math.floor(r) or math.ceil(r)) end
function SubString(s,a,b) return string.sub(s,a+1,b) end
function StringLength(s) return #s end
function StringCase(s,u) return u and s:upper() or s:lower() end
function SquareRoot(x) return math.sqrt(x) end
function Sin(x) return math.sin(x) end function Cos(x) return math.cos(x) end function Tan(x) return math.tan(x) end
function Atan2(y,x) return math.atan(y,x) end function Atan(x) return math.atan(x) end
function Asin(x) return math.asin(x) end function Acos(x) return math.acos(x) end
function Pow(a,b) return a^b end
math.randomseed(12345)
function GetRandomInt(a,b) if a>b then a,b=b,a end return math.random(a,b) end
function GetRandomReal(a,b) return a+(b-a)*math.random() end
function Deg2Rad(d) return d*math.pi/180 end function Rad2Deg(r) return r*180/math.pi end
function ModuloInteger(a,b) return a % b end function ModuloReal(a,b) return a % b end
function IMinBJ(a,b) return math.min(a,b) end function IMaxBJ(a,b) return math.max(a,b) end
function RMinBJ(a,b) return math.min(a,b) end function RMaxBJ(a,b) return math.max(a,b) end
function IAbsBJ(a) return math.abs(a) end function RAbsBJ(a) return math.abs(a) end
function BlzGetLocalClientWidth() return 1920 end function BlzGetLocalClientHeight() return 1080 end
function GetHandleId(h) return h and h.hid or 0 end
function Location(x,y) return H("location",{x=x,y=y}) end
function GetLocationX(l) return l and l.x or 0.0 end function GetLocationY(l) return l and l.y or 0.0 end
function Rect(a,b,c,d) return H("rect",{a=a,b=b,c=c,d=d}) end
function GetRectMinX(r) return r and r.a or 0.0 end function GetRectMinY(r) return r and r.b or 0.0 end
function GetRectMaxX(r) return r and r.c or 0.0 end function GetRectMaxY(r) return r and r.d or 0.0 end
function GetRectCenterX(r) return r and r.a and (r.a+r.c)/2 or 0.0 end function GetRectCenterY(r) return r and r.b and (r.b+r.d)/2 or 0.0 end
function GetTerrainCliffLevel() return 2 end
function FourCC(s) return string.unpack(">I4",s) end
function Condition(f) return H("conditionfunc",{f=f}) end Filter=Condition

-- camera bounds / playable area (seeded from war3map.w3i when given, updated by SetCameraBounds in main)
local M={[0]=512.0,[1]=512.0,[2]=256.0,[3]=256.0} -- left,right,top,bottom; verified against Forgotten Hollow w3i vs main()
local cam=__CFG.cam or {-7680.0+512,-7680.0+256,7680.0-512,7680.0-256}
function GetCameraMargin(m) return M[m] or 0.0 end
function SetCameraBounds(x1,y1,x2,y2,x3,y3,x4,y4)
  cam={math.min(x1,x2,x3,x4),math.min(y1,y2,y3,y4),math.max(x1,x2,x3,x4),math.max(y1,y2,y3,y4)} end
function GetCameraBoundMinX() return cam[1] end function GetCameraBoundMinY() return cam[2] end
function GetCameraBoundMaxX() return cam[3] end function GetCameraBoundMaxY() return cam[4] end
local function setareas()
  bj_mapInitialPlayableArea=H("rect",{a=cam[1]-M[0],b=cam[2]-M[3],c=cam[3]+M[1],d=cam[4]+M[2],lbl="bj_mapInitialPlayableArea"})
  bj_mapInitialCameraBounds=H("rect",{a=cam[1],b=cam[2],c=cam[3],d=cam[4],lbl="bj_mapInitialCameraBounds"})
end
setareas()
function GetWorldBounds() local p=bj_mapInitialPlayableArea return Rect(p.a-512,p.b-512,p.c+512,p.d+512) end
function InitBlizzardGlobals() setareas() end
function MarkGameStarted() bj_gameStarted=true end
function InitBlizzard() InitBlizzardGlobals() bj_gameStartedTimer=CreateTimer() TimerStart(bj_gameStartedTimer, 0.01, false, MarkGameStarted) end
function SetPlayerRaceSkin() end function SetHDWaterParamsEx() end
if rawget(_G,"RACE_PREF_FORSAKEN")==nil then RACE_PREF_FORSAKEN=H("racepreference",{lbl="RACE_PREF_FORSAKEN"}) end
bj_CAMPAIGN_INDEX_RU=7

-- triggers
function CreateTrigger() return H("trigger",{acts={},conds={},on=true}) end
function TriggerAddAction(t,f) if not t then return end table.insert(t.acts,f) return H("triggeraction") end
function TriggerAddCondition(t,c) if not t then return end table.insert(t.conds,c) return H("triggercondition") end
function EnableTrigger(t) if t then t.on=true end end function DisableTrigger(t) if t then t.on=false end end
function IsTriggerEnabled(t) return t and t.on or false end
__cur=nil
function GetTriggeringTrigger() return __cur end
function TriggerEvaluate(t) if not t then return false end for _,c in ipairs(t.conds) do if c.f and not c.f() then return false end end return true end
function __runacts(t,label) for _,f in ipairs(t.acts) do __spawn(function() local p=__cur __cur=t f() __cur=p end, label) end end
function TriggerExecute(t) if t==nil then return end __runacts(t,"TriggerExecute") end
function TriggerExecuteBJ(t,c) if not t then return false end if c and not TriggerEvaluate(t) then return false end TriggerExecute(t) return true end
function ConditionalTriggerExecute(t) if t==nil then return end if TriggerEvaluate(t) then __runacts(t,"CTE") end end
function TriggerRegisterTimerEvent(t,d,per)
  if not t then return H("event") end
  local function fire() if t.on and TriggerEvaluate(t) then __runacts(t,"timer-event") end if per then __at(math.max(d,0.01),fire) end end
  __at(d,fire) return H("event") end
function TriggerRegisterTimerEventSingle(t,d) return TriggerRegisterTimerEvent(t,d,false) end
function TriggerRegisterTimerEventPeriodic(t,d) return TriggerRegisterTimerEvent(t,d,true) end
function CreateTimer() return H("timer",{}) end
function TimerStart(tm,d,per,f) if tm==nil then return end tm.gen=(tm.gen or 0)+1 local g=tm.gen
  if f then local function fire() if tm.gen~=g then return end __exp=tm __spawn(f,"timer cb") if per then __at(math.max(d,0.01),fire) end end __at(d,fire) end end
function PauseTimer(tm) if tm then tm.gen=(tm.gen or 0)+1 end end
function DestroyTimer(tm) if tm then tm.gen=(tm.gen or 0)+1 end end
function GetExpiredTimer() return __exp end
function TriggerSleepAction(d) __sleep(d) end
function PolledWait(d) __sleep(d) end
function ForForce(f,cb) if cb then __enum=Player(0) __spawn(cb,"ForForce") end end
function GetEnumPlayer() return __enum end
function BlzLoadTOCFile() return true end

-- events are registered but never fired (no player input in the sweep)
local function ev() return H("event") end
TriggerRegisterPlayerUnitEvent=ev TriggerRegisterAnyUnitEventBJ=ev TriggerRegisterUnitEvent=ev

-- destructables (enumerable)
__dests={}
local function mk(id,x,y) local d=H("destructable",{tid=id,x=x,y=y}) __dests[#__dests+1]=d return d end
function BlzCreateDestructableWithSkinPitchRollColor(id,x,y) return mk(id,x,y) end
function BlzCreateDestructableZWithSkinPitchRollColor(id,x,y) return mk(id,x,y) end
function BlzCreateDestructableZWithSkin(id,x,y) return mk(id,x,y) end
function BlzCreateDestructableWithSkin(id,x,y) return mk(id,x,y) end
function CreateDestructable(id,x,y) return mk(id,x,y) end
function CreateDestructableZ(id,x,y) return mk(id,x,y) end
function EnumDestructablesInRect(r,f,cb) if not cb then return end for _,d in ipairs(__dests) do __ed=d cb() end end
function GetEnumDestructable() return __ed end
function GetDestructableTypeId(d) return d and d.tid or 0 end
function GetWidgetX(w) return w and w.x or 0.0 end function GetWidgetY(w) return w and w.y or 0.0 end
-- units / items keep their type and position
local function mku(t,id,x,y,p) return H(t,{tid=id,x=x,y=y,owner=p}) end
__units={}
local function mkunit(p,id,x,y) local u=mku("unit",id,x,y,p) __units[#__units+1]=u return u end
function CreateUnit(p,id,x,y) return mkunit(p,id,x,y) end
function BlzCreateUnitWithSkin(p,id,x,y) return mkunit(p,id,x,y) end
function CreateUnitAtLoc(p,id,l) return mkunit(p,id,l and l.x or 0.0,l and l.y or 0.0) end
function GetOwningPlayer(u) return u and u.owner or Player(0) end
function RemoveUnit(u) if u then u.removed=true end end
function SetUnitX(u,x) if u then u.x=x end end function SetUnitY(u,y) if u then u.y=y end end
function SetUnitPosition(u,x,y) if u then u.x=x u.y=y end end
-- groups hold real members so "repeat FirstOfGroup ... until nil" loops terminate
local function gl(g) if not g then return {} end g.l=g.l or {} return g.l end
function CreateGroup() return H("group",{l={}}) end
function DestroyGroup(g) end
function GroupAddUnit(g,u) if not g or not u then return false end local l=gl(g) for _,v in ipairs(l) do if v==u then return false end end l[#l+1]=u return true end
function GroupRemoveUnit(g,u) local l=gl(g) for i,v in ipairs(l) do if v==u then table.remove(l,i) return true end end return false end
function GroupClear(g) if g then g.l={} end end
function FirstOfGroup(g) return gl(g)[1] end
function CountUnitsInGroup(g) return #gl(g) end BlzGroupGetSize=CountUnitsInGroup
function BlzGroupUnitAt(g,i) return gl(g)[i+1] end
function IsUnitInGroup(u,g) for _,v in ipairs(gl(g)) do if v==u then return true end end return false end
function ForGroup(g,cb) if not cb then return end local c={table.unpack(gl(g))} local p=__enumu for _,u in ipairs(c) do __enumu=u cb() end __enumu=p end
function GetEnumUnit() return __enumu end
function GetFilterUnit() return __filtu end
local function enum(g,filt,pred) if not g then return end g.l={}
  for _,u in ipairs(__units) do if not u.removed and pred(u) then
    local ok=true if filt and filt.f then local p=__filtu __filtu=u ok=filt.f() __filtu=p end
    if ok then g.l[#g.l+1]=u end end end end
function GroupEnumUnitsInRect(g,r,f) enum(g,f,function(u) return r and u.x>=r.a and u.x<=r.c and u.y>=r.b and u.y<=r.d end) end
function GroupEnumUnitsInRange(g,x,y,d,f) enum(g,f,function(u) return (u.x-x)^2+(u.y-y)^2<=d*d end) end
function GroupEnumUnitsInRangeOfLoc(g,l,d,f) if l then GroupEnumUnitsInRange(g,l.x,l.y,d,f) end end
function GroupEnumUnitsOfPlayer(g,p,f) enum(g,f,function(u) return u.owner==p end) end
function GroupEnumUnitsOfType(g,n,f) enum(g,f,function(u) return false end) end
function CreateItem(id,x,y) return mku("item",id,x,y) end
function BlzCreateItemWithSkin(id,x,y) return mku("item",id,x,y) end
function GetUnitTypeId(u) return u and u.tid or 0 end
function GetItemTypeId(i) return i and i.tid or 0 end
function GetUnitX(u) return u and u.x or 0.0 end function GetUnitY(u) return u and u.y or 0.0 end
function GetItemX(i) return i and i.x or 0.0 end function GetItemY(i) return i and i.y or 0.0 end
-- orders are accepted (true), so order-based pathing probes see open terrain instead of failing everywhere
for name in pairs(__SIG) do if name:match("^Issue") then local f=rawget(_G,name) rawset(_G,name,function(...) f(...) return true end) end end
