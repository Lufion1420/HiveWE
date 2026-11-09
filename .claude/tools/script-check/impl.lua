local H=__H
function Player(i) __players[i]=__players[i] or H("player",{id=i}) return __players[i] end
function GetPlayerId(p) if p==nil then return 0 end return p.id or 0 end
function GetLocalPlayer() return Player(1) end
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
function GetWorldBounds() return Rect(-16384.,-16384.,16384.,16384.) end
function GetCameraBoundMinX() return -16000. end function GetCameraBoundMinY() return -16000. end
function GetCameraBoundMaxX() return 16000. end function GetCameraBoundMaxY() return 16000. end
function GetTerrainCliffLevel() return 2 end
function FourCC(s) return string.unpack(">I4",s) end
function Condition(f) return H("conditionfunc",{f=f}) end Filter=Condition
function CreateTrigger() return H("trigger",{acts={},conds={},on=true}) end
function TriggerAddAction(t,f) if not t then __warn("TriggerAddAction nil trigger") return end table.insert(t.acts,f) return H("triggeraction") end
function TriggerAddCondition(t,c) if not t then return end table.insert(t.conds,c) return H("triggercondition") end
function EnableTrigger(t) if t then t.on=true end end function DisableTrigger(t) if t then t.on=false end end
function IsTriggerEnabled(t) return t and t.on or false end
__cur=nil
function GetTriggeringTrigger() return __cur end
function TriggerEvaluate(t) for _,c in ipairs(t.conds) do if c.f and not c.f() then return false end end return true end
function __runacts(t,label) for _,f in ipairs(t.acts) do __spawn(function() local p=__cur __cur=t f() __cur=p end, label) end end
function TriggerExecute(t) if t==nil then __warn("TriggerExecute(nil)") return end __runacts(t,"TriggerExecute") end
function TriggerExecuteBJ(t,c) if c and not TriggerEvaluate(t) then return false end TriggerExecute(t) return true end
function ConditionalTriggerExecute(t) if t==nil then __warn("ConditionalTriggerExecute(nil)") return end if TriggerEvaluate(t) then __runacts(t,"CTE") end end
function TriggerRegisterTimerEvent(t,d,per) __at(d,function() if t.on and TriggerEvaluate(t) then __runacts(t,"timer-event "..tostring(d)) end end) return H("event") end
function TriggerRegisterTimerEventSingle(t,d) return TriggerRegisterTimerEvent(t,d,false) end
function CreateTimer() return H("timer",{}) end
function TimerStart(tm,d,per,f) if tm==nil then __warn("TimerStart(nil timer)") return end tm.gen=(tm.gen or 0)+1 local g=tm.gen
  if f then local function fire() if tm.gen~=g then return end __exp=tm __spawn(f,"timer cb") if per and __ticks<4000 then __at(math.max(d,0.01),fire) end end __at(d,fire) end end
function PauseTimer(tm) if tm then tm.gen=(tm.gen or 0)+1 end end
function DestroyTimer(tm) if tm then tm.gen=(tm.gen or 0)+1 end end
function GetExpiredTimer() return __exp end
function TriggerSleepAction(d) __sleep(d) end
function PolledWait(d) __sleep(d) end
function ForForce(f,cb) __enum=Player(1) __spawn(cb,"ForForce") end
function GetEnumPlayer() return __enum end
function InitBlizzardGlobals() end
function MarkGameStarted() bj_gameStarted=true end
function InitBlizzard() InitBlizzardGlobals() bj_gameStartedTimer=CreateTimer() TimerStart(bj_gameStartedTimer, 0.01, false, MarkGameStarted) end
function SetPlayerRaceSkin() end function SetHDWaterParamsEx() end
RACE_PREF_FORSAKEN=__H("racepreference") bj_CAMPAIGN_INDEX_RU=7
