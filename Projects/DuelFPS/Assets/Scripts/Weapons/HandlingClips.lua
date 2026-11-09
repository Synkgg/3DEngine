-- Normalized time, grip offset (xyz), wrist pitch, weapon pitch, weapon roll.
-- Endpoints return to the carry pose. Reload timing follows each gun's reload time.
local clips={
 reload={{0,0,0,0,0,0,0},{.15,.025,-.02,.07,-6,8,-14},{.33,.05,-.14,.20,-28,14,-24},{.50,.03,-.25,.22,-36,16,-24},{.69,.04,-.035,.19,-14,14,-21},{.78,.035,.015,.18,4,10,-16},{.92,.02,.035,.06,12,4,-6},{1,0,0,0,0,0,0}},
 bolt={{0,0,0,0,0,0,0},{.20,.018,.04,.15,16,3,-6},{.44,.03,.045,.23,23,6,-9},{.72,.012,.025,.10,9,2,-5},{1,0,0,0,0,0,0}},
 draw={{0,0,0,0,0,-22,18},{.48,0,0,0,0,-10,7},{.84,0,0,0,0,1,-1},{1,0,0,0,0,0,0}}
}
return function(name,t)
    local clip=clips[name];local a=clip[1];local b=clip[#clip]
    for i=2,#clip do b=clip[i];if t<=b[1] then break end;a=b end
    local f=math.max(0,math.min(1,(t-a[1])/math.max(.001,b[1]-a[1])));f=f*f*(3-2*f)
    local function v(i)return a[i]+(b[i]-a[i])*f end
    return v(2),v(3),v(4),v(5),v(6),v(7)
end
