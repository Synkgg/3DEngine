local Profiles=require('Scripts.Weapons.LoadoutProfiles')
local A={}
local models={rifle='Rifle',smg='SMG',shotgun='Shotgun',pistol='Pistol',sniper='Sniper',marksman='Marksman'}
for _,v in ipairs(require('Scripts.Weapons.Variants').entries) do models[v.id]=v.module end
local guns={};for id,module in pairs(models) do guns[id]=require('Scripts.Weapons.'..module) end
local function name(id)return (guns[id].displayName:match('^(.-)%s*/') or guns[id].displayName) end
function A.refresh(ui,index,primary,secondary,slot,category,picking)
    ui.SetVisible('Overview',not picking);ui.SetVisible('Picker',picking)
    ui.SetText('SlotsTitle','Loadout '..index);ui.SetText('PickerTitle','Choose your '..slot..' weapon')
    ui.SetText('PrimaryValue',name(primary));ui.SetText('SecondaryValue',name(secondary))
    for i=1,Profiles.count do ui.SetVisible('Profile'..i..'Plate',index==i) end
    for base in pairs(models) do
        if Profiles.categoryOf[base]==base then
            local active=Profiles.categoryOf[category]==base
            ui.SetVisible('Choices_'..base,active);ui.SetVisible('Category_'..base..'_Plate',active)
        end
    end
    for id,def in pairs(guns) do
        ui.SetVisible('PrimaryIcon_'..id,id==primary);ui.SetVisible('SecondaryIcon_'..id,id==secondary)
        local current=(slot=='primary' and primary or secondary)==id
        local other=(slot=='primary' and secondary or primary)==id
        ui.SetText(id..'Label',current and 'Keep equipped' or other and 'Swap slots' or 'Equip weapon')
        ui.SetText(id..'Details',string.format('%d rounds   /   %s',def.magSize,def.boltTime and 'Bolt action' or def.automatic and 'Automatic' or def.pellets>1 and 'Pump action' or 'Semi-auto'))
    end
end
return A
