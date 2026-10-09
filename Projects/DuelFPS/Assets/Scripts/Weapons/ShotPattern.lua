-- Shared deterministic pellet directions. Both the client and the host use
-- exactly this pattern, so shotgun damage cannot be claimed by the client.
local ShotPattern = {}

function ShotPattern.Directions(x,y,z,pellets,spread)
    local count=math.max(1,math.min(12,math.floor(pellets or 1)))
    local length=math.sqrt(x*x+y*y+z*z)
    if length<0.001 then return {} end
    x,y,z=x/length,y/length,z/length
    if count==1 then return {{x=x,y=y,z=z}} end

    local rx,ry,rz=-z,0,x
    local rl=math.sqrt(rx*rx+rz*rz)
    if rl<0.01 then rx,ry,rz=1,0,0
    else rx,rz=rx/rl,rz/rl end
    local ux,uy,uz=ry*z-rz*y,rz*x-rx*z,rx*y-ry*x
    local directions={}
    for i=1,count do
        local radius=math.sqrt((i-0.5)/count)*(spread or 0.055)
        local angle=i*2.399963229728653
        local a,b=math.cos(angle)*radius,math.sin(angle)*radius
        local px,py,pz=x+rx*a+ux*b,y+ry*a+uy*b,z+rz*a+uz*b
        local norm=math.sqrt(px*px+py*py+pz*pz)
        directions[i]={x=px/norm,y=py/norm,z=pz/norm}
    end
    return directions
end

return ShotPattern
