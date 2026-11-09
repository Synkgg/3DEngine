"""Multiplayer lobby and field bench. Widget IDs are shared with both Lua armories."""
def build(UI,A,q,weapons,alternatives):
    ink=(.06,.105,.13,.90);rail=(.13,.21,.25,.93);mint=(.40,.83,.68,1);white=(.94,.97,.95,1);muted=(.70,.78,.79,1)
    def pic(u,n,asset,x,y,w,h,z=0,depth=0):u.w(depth,2,n,x,y,w,h,(1,1,1,1),z,tail=q('Assets/UI/Art/'+asset+'.png'))
    def bg(asset):
        u=UI();pic(u,'Backdrop',asset,0,0,1920,1080);u.panel('TopBand',0,0,1920,100,ink,z=2);u.panel('BottomBand',0,975,1920,105,ink,z=2);return u
    def btn(u,n,label,x,y,w,h,cb,sel=False,depth=0,script='MainMenu.lua',z=5):
        color=mint if sel else rail;fg=(.04,.095,.09,1) if sel else white
        tail=' '.join(map(str,[*color,.39,.61,.60,1,.19,.42,.42,1,.08,.11,.13,1,1,*fg,*fg,*white,*muted]))+f' "Assets/Audio/Breakbulk/interface.wav" "Assets/Scripts/{script}" {q(cb)} "" "" "" ""'
        u.w(depth,3,n,x,y,w,h,color,z,hit=True,tail=tail);u.text(n.replace('Button','Label') if 'Button' in n else n+'Label',label,20,0,w-40,h,23,fg,depth+1,z+1)
    def brand(u,title):u.text('Brand',title,60,12,570,75,37,white,z=4);u.text('Edition','BREAKBULK   /   FIELD OPERATIONS',1330,20,540,60,18,muted,z=4,align=2)
    def status(u):u.text('NetworkStatus','',570,988,1250,68,22,white,z=4)
    u=bg('Lobby');u.text('Brand','BREAKBULK',75,12,610,75,39,white,z=4)
    u.panel('MenuPanel',75,325,540,521,ink,z=2)
    u.text('MenuHeading','Multiplayer',105,360,470,76,43,white,z=4)
    for i,(key,label,cb) in enumerate([('PlayButton','Play','OnQuickPlay'),('CustomButton','Custom game','OnPlayClicked'),('DrillButton','Training range','OnDrillSetup'),('LoadoutButton','Loadouts','OnLoadoutClicked'),('SettingsButton','Settings','OnSettingsClicked')]):
        btn(u,key,label,105,458+i*78,312 if i==0 else 480,68,cb,i==0)
    btn(u,'ModeButton','Mode: 1v1',428,458,157,68,'OnModeClicked')
    btn(u,'QuitButton','Quit game',75,992,220,58,'OnQuitClicked');status(u);u.save('MainMenu.ui')
    u=bg('Lobby');brand(u,'CUSTOM GAME');u.panel('SessionPanel',72,190,700,572,ink,z=2);u.text('ModeHeading','Custom game',104,220,635,70,39,white,z=4)
    u.text('TransportCaption','Host on',104,330,196,68,23,muted,z=4)
    btn(u,'TransportButton','DuelFPS server',304,330,440,68,'OnTransportClicked')
    btn(u,'HostButton','CREATE SERVER GAME',104,438,640,76,'OnHostSetup',True)
    btn(u,'BrowseButton','FIND LAN GAMES',104,562,640,68,'OnBrowseClicked')
    u.text('AddressTitle','JOIN WITH A LOBBY CODE',104,554,620,38,19,muted,z=4)
    u.w(0,4,'AddressField',104,606,405,65,rail,5,hit=True,tail='"" "8-digit code" 26 8 0')
    btn(u,'JoinButton','JOIN',527,606,217,65,'OnJoinClicked')
    btn(u,'BackButton','BACK',65,992,205,58,'OnPlayBack');status(u);u.save('Play.ui')
    for host in [True,False]:
        u=bg('Lobby');brand(u,'PRIVATE MATCH' if host else 'TRAINING RANGE');u.panel('SetupPanel',72,155,740,750,ink,z=2);u.text('Heading','Host a game' if host else 'Practice solo',105,184,670,75,42,white,z=4)
        rows=[('Map','Map','OnMapClicked')]+([('Limit','Rounds to win','OnLimitClicked'),('Time','Round duration','OnTimeClicked')] if host else [])
        for i,(key,label,cb) in enumerate(rows):
            y=310+i*111;u.text(key+'Caption',label,105,y,263,70,23,muted,z=4);btn(u,key+'Button','',365,y,407,70,cb)
        btn(u,'StartButton' if host else 'PracticeButton','START HOSTING' if host else 'ENTER RANGE',104,792,666,76,'OnStartHost' if host else 'OnPracticeClicked',True)
        if host:btn(u,'ModeButton','Mode: 1v1',365,643,407,70,'OnModeClicked')
        u.panel('MapPreview',1390,610,365,300,ink,z=2)
        for n in ['Terminal','Depot']:pic(u,'Map'+n,'Radar'+n,1460,635,220,220,4)
        btn(u,'BackButton','BACK',65,992,205,58,'OnPlayClicked' if host else 'OnPlayBack');status(u);u.save('HostSetup.ui' if host else 'DrillSetup.ui')
    # One decision per page: edit the equipped pair, or choose one replacement.
    u=UI();u.panel('Canvas',0,0,1920,1080,(.12,.17,.22,1))
    u.panel('Overview',0,0,1920,1080,(0,0,0,0))
    u.text('Title','Your loadouts',110,80,1600,100,55,white,1)
    u.text('PresetHeading','Presets',114,236,400,50,26,muted,1)
    for i in range(1,6):
        y=316+(i-1)*95;btn(u,f'Profile{i}Button',f'Loadout {i}',110,y,360,72,f'OnProfile{i}Clicked',depth=1,script='Player.lua')
        u.panel(f'Profile{i}Plate',110,y,6,72,mint,depth=1,z=8)
    u.text('SlotsTitle','Loadout 1',550,219,1180,78,39,white,1)
    for i,slot in enumerate(['Primary','Secondary']):
        x=550+i*625
        u.panel(slot+'Card',x,316,595,430,(.18,.245,.30,1),depth=1)
        u.text(slot+'Heading',slot+' weapon',30,25,530,42,23,muted,2)
        u.text(slot+'Value','',30,80,530,59,32,white,2)
        for id,*_ in weapons:pic(u,slot+'Icon_'+id,id,42,154,510,182,3,2)
        btn(u,slot+'Tab','Change weapon',30,341,535,62,'On'+slot+'SlotClicked',depth=2,script='Player.lua')
    u.panel('FooterLine',110,884,1680,2,(.28,.35,.41,1),depth=1)
    btn(u,'BackButton','Cancel',110,927,230,72,'OnLoadoutBack',depth=1,script='Player.lua')
    btn(u,'SaveButton','Save loadout',1470,927,320,72,'OnLoadoutSaved',True,1,'Player.lua')
    u.panel('Picker',0,0,1920,1080,(0,0,0,0),visible=False)
    u.text('PickerTitle','Choose a weapon',110,80,1700,100,55,white,1)
    u.text('TypeHeading','Weapon type',114,236,400,50,26,muted,1)
    for i,(base,cb,category,label) in enumerate(weapons[:6]):
        y=316+i*79;btn(u,'Category_'+base+'_Button',category.title(),110,y,360,62,'OnCategory'+cb,depth=1,script='Player.lua')
        u.panel('Category_'+base+'_Plate',110,y,6,62,mint,depth=1,z=8)
        alt=next(v for v in alternatives if v[2]==base)
        u.panel('Choices_'+base,550,219,1220,580,(0,0,0,0),depth=1,visible=i==0)
        u.text('Heading_'+base,category.title(),0,0,1100,78,39,white,2)
        for j,(id,action,name) in enumerate([(base,cb,label),(alt[0],alt[1],alt[3])]):
            x=j*625;u.panel(id+'Card',x,97,595,430,(.18,.245,.30,1),depth=2)
            u.text(id+'Name',name,30,30,535,52,30,white,3)
            u.text(id+'Details','',30,94,535,45,22,muted,3)
            pic(u,id+'Pic',id,42,154,510,182,3,3)
            btn(u,id+'Button','Equip weapon',30,341,535,62,'On'+action+'Selected',True,3,'Player.lua')
    u.panel('PickerLine',110,884,1680,2,(.28,.35,.41,1),depth=1)
    btn(u,'PickerBackButton','Back to loadout',110,927,325,72,'OnPickerBack',depth=1,script='Player.lua')
    u.save('Loadout.ui')
