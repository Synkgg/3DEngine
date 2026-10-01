window.DOCS=[
{group:"Start Here",slug:"welcome",title:"Welcome",html:`
<div class="eyebrow">Engine documentation</div><h1>Build games, not engine plumbing.</h1><p class="lead">This engine is a C++ 3D editor/runtime with Lua 5.4 gameplay scripting. The engine provides reusable scene, rendering, input, physics, UI, audio, networking and player primitives; your project owns the game rules.</p>
<div class="callout good">These docs are generated from the <code>ui-rewrite</code> implementation. Examples below use APIs that exist in the current source.</div>
<div class="cards"><a class="card" href="#/hour-game"><b>Make a game in one hour →</b><span>Build a small first-person collection game using scenes, camera, movement, interactions and UI.</span></a><a class="card" href="#/lua"><b>Lua scripting</b><span>Lifecycle, script components, properties and engine globals.</span></a><a class="card" href="#/editor"><b>Editor tour</b><span>Scene document, Hierarchy, Details, Assets, Console and UI documents.</span></a><a class="card" href="#/api"><b>Lua API reference</b><span>Input, Scene, Camera, Physics, UI, Controller, Network and more.</span></a></div>
<h2>Engine philosophy</h2><p>Reusable capability belongs in the engine. Game-specific behavior belongs in project Lua. A <code>CameraComponent</code> knows FOV and clipping; it does not know what “aim down sights” means. A <code>PawnComponent</code> can be possessed; it does not know the rules of a duel. This keeps projects free to build different games on the same primitives.</p>
<h2>Current technology</h2><table><tr><th>Area</th><th>Implementation</th></tr><tr><td>Language</td><td>C++20 engine, Lua 5.4 project scripts through sol2</td></tr><tr><td>Window/input</td><td>SDL3</td></tr><tr><td>Rendering</td><td>OpenGL via GLAD, editor with Dear ImGui + ImGuizmo</td></tr><tr><td>Models</td><td>Built-in OBJ loading and primitive meshes</td></tr><tr><td>UI</td><td>Canvas/widget system with a dedicated Widget Blueprint-style editor</td></tr><tr><td>Projects</td><td>Portable project descriptor + project-owned Assets and Saved folders</td></tr></table>
<h2>Known boundaries</h2><p>Skeletal animation is not implemented in the current model pipeline. Lua audio currently exposes volume controls, while native button/interaction sounds are supported; there is not yet a general Lua <code>Audio.PlaySound</code> binding. Treat those as roadmap areas, not available APIs.</p>`},
{group:"Start Here",slug:"editor",title:"Editor & projects",html:`
<div class="eyebrow">Getting started</div><h1>Editor & project workflow</h1><p class="lead">The editor uses a persistent shell with document pages. The Scene is permanent; UI assets open as closeable documents without destroying the running scene.</p>
<h2>Project structure</h2><pre><code>MyGame/
├─ MyGame.project
├─ ProjectSettings.cfg
├─ Assets/
│  ├─ Scenes/
│  ├─ Scripts/
│  ├─ UI/
│  ├─ Textures/
│  ├─ Models/
│  ├─ Materials/
│  ├─ Audio/
│  ├─ Fonts/
│  └─ Prefabs/
└─ Saved/</code></pre><p>New projects create these folders automatically and start with <code>Assets/Scenes/Main.scene</code>. <code>Saved/</code> is intended for generated/runtime state and is ignored by the generated project <code>.gitignore</code>.</p>
<h2>Scene workspace</h2><p>The default scene layout places Hierarchy on the left, Scene viewport in the center, Details on the right, and Assets/Console along the bottom. The viewport toolbar keeps Play/Stop centered; secondary controls live under the Viewport dropdown.</p>
<h3>Hierarchy</h3><p>Entities can form parent/child hierarchies. Local transforms compose into world transforms. This is especially useful for a player entity with a child camera or attached meshes.</p>
<h3>Details</h3><p>Use Details to edit Transform and add/remove supported components. Script components can attach multiple Lua scripts and store per-entity property overrides.</p>
<h3>Assets</h3><p>Project assets are resolved relative to the active project. Scenes, scripts, UI documents, prefabs, textures, models, audio and fonts remain project-owned.</p>
<h2>UI documents</h2><p>Opening a <code>.ui</code> asset creates a separate top-level document tab. Each UI document has its own editing canvas and editor state. The UI workspace provides Palette, Hierarchy, Designer and UI Details panels. Runtime UI uses a separate canvas, so editing and game UI do not share state.</p>
<h2>Play mode</h2><p>Play starts the runtime systems and Lua scripts. <code>OnCreate</code> is called after a script loads, <code>OnUpdate(deltaTime)</code> runs each update, and <code>OnDestroy</code> runs when the script/runtime is torn down. Interactable scripts can also implement <code>OnInteract</code>.</p>`},
{group:"Tutorials",slug:"hour-game",title:"Make a game in one hour",html:`
<div class="eyebrow">60 minute tutorial</div><h1>Make “Core Escape” in one hour</h1><p class="lead">Build a tiny first-person game: explore a room, collect three energy cores, watch the HUD update, then unlock the exit. The goal is breadth—learn the engine’s normal workflow in one sitting.</p>
<div class="callout">You do not need networking for this tutorial. Once this works, the Networking page shows how the same project architecture extends to multiplayer.</div>
<h2>0–5 min · Create the project</h2><div class="steps"><div class="step"><h3>Create CoreEscape</h3><p>Create a new project named <b>CoreEscape</b>. Open its generated <code>Assets/Scenes/Main.scene</code> and save it as <code>Assets/Scenes/Arena.scene</code>. Keep project paths relative, such as <code>Assets/Scripts/Player.lua</code>.</p></div><div class="step"><h3>Block out a room</h3><p>Add primitive mesh entities for a floor and walls. Give solid level geometry Collider components. Add a Directional Light; point/spot lights are also available.</p></div></div>
<h2>5–15 min · Player, collision and camera</h2><div class="steps"><div class="step"><h3>Create the Player entity</h3><p>Add an entity named <code>Player</code> with Transform, Character Controller, Collider, Pawn and Script components. The Character Controller exposes horizontal movement, jumping and grounded state.</p></div><div class="step"><h3>Add a child camera</h3><p>Create <code>PlayerCamera</code> as a child of Player. Give it a Camera component, set it active, use roughly 60–90° FOV, and move its local Y position to eye height. Because scene hierarchy transforms compose, the camera follows its parent.</p></div></div>
<h2>15–25 min · WASD, mouse look, sprint and jump</h2><p>Create <code>Assets/Scripts/Player.lua</code> and attach it to Player:</p>
<pre><code>local walkSpeed = 5.0
local sprintSpeed = 8.0
local sensitivity = 0.01
local cameraID = 0

function OnCreate()
    local camera = Scene.FindEntity("PlayerCamera")
    cameraID = camera.id
    if cameraID ~= 0 then Camera.SetActive(cameraID) end
    Input.SetCursorVisible(false)
end

function OnUpdate(dt)
    if cameraID ~= 0 then
        Camera.RotateEntity(
            cameraID,
            Input.GetMouseDeltaX() * sensitivity,
            Input.GetMouseDeltaY() * -sensitivity
        )
    end

    local x, z = 0.0, 0.0
    if Input.IsKeyDown("W") then z = z + 1 end
    if Input.IsKeyDown("S") then z = z - 1 end
    if Input.IsKeyDown("D") then x = x + 1 end
    if Input.IsKeyDown("A") then x = x - 1 end

    local forward, right = Camera.GetForward(), Camera.GetRight()
    local mx = right.x * x + forward.x * z
    local mz = right.z * x + forward.z * z
    local length = math.sqrt(mx * mx + mz * mz)
    local speed = Input.IsKeyDown("Left Shift") and sprintSpeed or walkSpeed

    if length > 0 then
        mx, mz = mx / length * speed, mz / length * speed
    end

    CharacterController.Move(mx, mz)
    if Input.IsKeyPressed("Space") then CharacterController.Jump() end
end</code></pre>
<div class="callout"><b>Key names:</b> <code>Input.IsKeyDown</code> and <code>Input.IsKeyPressed</code> pass their strings through SDL's scancode-name lookup. Names such as <code>W</code>, <code>Space</code>, <code>Escape</code> and <code>Left Shift</code> are used by the included projects.</div>
<h2>25–35 min · Interactable cores</h2><p>Create three visible core entities. Add Collider + Interactable to each and set the prompt to <code>Collect core</code>. Put this script on each core as <code>Assets/Scripts/Core.lua</code>:</p>
<pre><code>function OnInteract()
    local count = State.GetNumber("cores", 0)
    State.SetNumber("cores", count + 1)
    self:Destroy()
end</code></pre><p>The interaction system owns detection/prompt flow; project code decides what interaction means. <code>self</code> is the entity handle for the entity running the script.</p>
<h2>35–45 min · HUD and pause UI</h2><p>Create <code>Assets/UI/HUD.ui</code> in the UI editor. Add text widgets named <code>Objective</code>, <code>CoreCount</code>, and <code>InteractText</code>. Add a pause panel named <code>PauseMenu</code> with a <code>ResumeButton</code>. Then add a game-controller entity with this script:</p>
<pre><code>local paused = false

function OnCreate()
    State.SetNumber("cores", 0)
    UI.Load("Assets/UI/HUD.ui")
    UI.SetVisible("PauseMenu", false)
    Input.SetCursorVisible(false)
end

function OnUpdate(dt)
    local count = State.GetNumber("cores", 0)
    UI.SetText("CoreCount", string.format("%d / 3 CORES", count))
    UI.SetText("Objective", count >= 3 and "REACH THE EXIT" or "FIND THE ENERGY CORES")

    local prompt = Scene.GetInteractionPrompt()
    UI.SetVisible("InteractText", prompt ~= nil and prompt ~= "" and not paused)
    if prompt ~= nil and prompt ~= "" then UI.SetText("InteractText", "[E] " .. prompt) end

    if Input.IsKeyPressed("Escape") then
        paused = not paused
        Scene.SetPaused(paused)
        UI.SetVisible("PauseMenu", paused)
        Input.SetCursorVisible(paused)
    end

    if paused and UI.WasClicked("ResumeButton") then
        paused = false
        Scene.SetPaused(false)
        UI.SetVisible("PauseMenu", false)
        Input.SetCursorVisible(false)
    end
end</code></pre>
<h2>45–52 min · Exit and scene transition</h2><p>Add an Interactable exit entity with prompt <code>Open exit</code>. Attach:</p><pre><code>function OnInteract()
    if State.GetNumber("cores", 0) >= 3 then
        Scene.Load("Assets/Scenes/Win.scene")
    else
        print("The exit is still locked.")
    end
end</code></pre><p>Create a minimal <code>Win.scene</code> with a script that loads a UI containing a Play Again button. Use <code>Scene.Load("Assets/Scenes/Arena.scene")</code> when clicked.</p>
<h2>52–57 min · Polish</h2><p>Adjust lights and project graphics. Use the editor Settings window or the Graphics Lua API for AA, shadows, fog, bloom, view distance and exposure. UI buttons support authored click-sound paths, and the native interaction path has an interaction sound hook. General arbitrary sound playback is not yet exposed as a Lua function.</p>
<h2>57–60 min · Test the complete loop</h2><p>Press Play and verify: mouse look → WASD → Shift sprint → Space jump → collect three cores → HUD updates → Escape pauses and releases cursor → exit loads Win scene → Play Again returns to Arena.</p>
<div class="callout good"><b>You just used:</b> projects, assets, scenes, entities, hierarchy, components, Lua lifecycle, keybinds, mouse input, character movement, child camera, collisions/interactions, runtime state, UI documents, buttons, pause state, scene loading, lighting and graphics.</div>
<h2>Where to go next</h2><p>Turn each core into a prefab and spawn it with <code>Scene.InstantiatePrefab</code>; use <code>Physics.Raycast</code> for a tool or weapon; expose designer values through a global <code>Properties</code> table; or add multiplayer using Network + Controller/Pawn primitives.</p>`},
{group:"Core Concepts",slug:"lua",title:"Lua scripting",html:`
<div class="eyebrow">Gameplay scripting</div><h1>Lua scripting</h1><p class="lead">Project behavior is authored in Lua 5.4. Each attached script receives its own environment populated with engine APIs and an entity-local <code>self</code> handle.</p>
<h2>Lifecycle</h2><pre><code>function OnCreate()
    print("Created")
end

function OnUpdate(deltaTime)
    -- per-frame gameplay
end

function OnInteract()
    -- called for an interacted entity
end

function OnDestroy()
    print("Destroyed")
end</code></pre><p>Only define callbacks you need. The engine opens Lua base, math, table, string and package libraries. <code>print</code> is redirected to the engine Logger/Console.</p>
<h2>Script properties</h2><p>Scripts can declare a global <code>Properties</code> table. The Script component stores per-script property overrides and injects them after the file executes but before <code>OnCreate</code>. Supported property value types are Number, Boolean, String and Entity.</p>
<h2>Modules</h2><p>Project modules can be loaded with <code>require()</code>. The runtime adds project-root and Assets patterns to <code>package.path</code>, keeping shared game modules project-owned.</p>
<h2>Entity-local vs scene APIs</h2><p><code>transform</code> edits the entity running the script. <code>self</code> is an object-style entity handle. <code>Scene</code> operates on arbitrary entities/IDs and handles scene-wide actions.</p><pre><code>local p = transform.GetPosition()
transform.SetPosition(p.x, p.y + 1, p.z)

local door = Scene.FindEntity("Door")
if door:IsValid() then
    door:SetInteractablePrompt("Open door")
end</code></pre>`},
{group:"Core Concepts",slug:"scene",title:"Scenes, entities & prefabs",html:`
<div class="eyebrow">World model</div><h1>Scenes, entities & prefabs</h1><p class="lead">A Scene owns entities, component storage, parent relationships, prefab-source metadata, environment state and deferred hierarchy destruction.</p>
<h2>Hierarchy</h2><p><code>Scene.SetParent(childID, parentID, keepWorld)</code> and <code>Scene.ClearParent(entityID, keepWorld)</code> manage relationships. World transforms are composed through the hierarchy. Destruction can remove an entire hierarchy safely after Lua updates finish.</p>
<h2>Finding and spawning</h2><pre><code>local door = Scene.FindEntity("ExitDoor")
local spawnedID = Scene.InstantiatePrefab("Assets/Prefabs/Crate.prefab", 0)
local copyID = Scene.DuplicateEntity(door.id, true)</code></pre>
<h2>Components currently serialized</h2><p>Scene serialization supports Transform, Mesh, Color, Name, Pawn, PlayerStart, CharacterController, Light, Camera, Collider, Texture, Material, Script and Interactable data, along with hierarchy/prefab metadata.</p>
<h2>Prefabs</h2><p>Prefabs are reusable entity hierarchies. Runtime instantiation returns the root entity ID; pass a nonzero parent ID to attach the new hierarchy under an existing entity.</p>`},
{group:"Systems",slug:"camera-input",title:"Camera & input",html:`
<div class="eyebrow">Systems</div><h1>Camera & input</h1><p class="lead">Input uses SDL scancodes and mouse state. Cameras can be controlled globally through the renderer or as real scene entities with Camera components.</p>
<h2>Input patterns</h2><table><tr><th>API</th><th>Use</th></tr><tr><td><code>Input.IsKeyDown(name)</code></td><td>Continuous movement/holding</td></tr><tr><td><code>Input.IsKeyPressed(name)</code></td><td>One-shot actions such as jump or pause</td></tr><tr><td><code>Input.IsMouseButtonDown(button)</code></td><td>Continuous mouse button state; button 1 is used for primary fire in DuelFPS</td></tr><tr><td><code>Input.GetMouseDeltaX/Y()</code></td><td>Relative mouse look while captured</td></tr><tr><td><code>Input.SetCursorVisible(bool)</code></td><td>Runtime cursor intent; use visible for menus, hidden for mouse-look gameplay</td></tr></table>
<h2>Entity cameras</h2><p>A Camera component stores FOV, near/far clip and active state. Its Transform supplies local pose; scene hierarchy supplies world pose. <code>Camera.RotateEntity</code> updates the entity transform and clamps pitch, then syncs the renderer when that camera is active.</p><pre><code>local camera = Scene.FindEntity("FirstPersonCamera")
Camera.SetActive(camera.id)
Camera.SetEntityFOV(camera.id, 90)
Camera.RotateEntity(camera.id, yawDelta, pitchDelta)</code></pre>
<h2>Renderer camera helpers</h2><p><code>Camera.Move</code>, <code>Rotate</code>, <code>Get/SetPosition</code>, <code>SetRotation</code>, <code>Reset</code>, <code>GetForward</code> and <code>GetRight</code> operate through the renderer camera and remain useful for simple controllers.</p>`},
{group:"Systems",slug:"physics",title:"Movement, collision & physics",html:`
<div class="eyebrow">Systems</div><h1>Movement, collision & physics</h1><p class="lead">The current gameplay layer provides box colliders, a character controller, collision/interaction systems and script raycasts.</p>
<h2>Character controller</h2><p>The component stores gravity, jump force, vertical/horizontal velocity, jump request and grounded state. Lua intentionally exposes a small gameplay surface:</p><pre><code>CharacterController.Move(moveX, moveZ)
if Input.IsKeyPressed("Space") then CharacterController.Jump() end
if CharacterController.IsGrounded() then ... end</code></pre>
<h2>Colliders</h2><p>Collider components are axis-aligned dimensions with an enabled flag. Lua can query/toggle the collider attached to the current script entity with <code>Collider.IsEnabled()</code> and <code>Collider.SetEnabled(bool)</code>.</p>
<h2>Raycasts</h2><pre><code>local hit = Physics.Raycast(
    origin.x, origin.y, origin.z,
    direction.x, direction.y, direction.z,
    100.0,
    self.id
)
if hit.hit then
    print("Hit entity " .. hit.entityID)
end</code></pre><p>The optional final ID ignores an entity. Use raycasts for weapons, tools, selection and line-of-sight logic.</p>`},
{group:"Systems",slug:"ui",title:"UI system",html:`
<div class="eyebrow">Systems</div><h1>UI system</h1><p class="lead">UI assets are authored in a dedicated document editor and loaded into the runtime canvas from Lua.</p>
<h2>Widgets</h2><p>The current widget types are Panel, Text, Image, Button, TextInput and Slider. Widgets support hierarchy, anchors, pivot, position/size, color, visibility, enabled state, hit testing and Z order. UI assets also support gradients; buttons have normal/hovered/pressed/disabled colors and optional click-sound paths.</p>
<h2>Runtime workflow</h2><pre><code>function OnCreate()
    UI.Load("Assets/UI/HUD.ui")
    UI.SetText("Score", "0")
    UI.SetVisible("PauseMenu", false)
end

function OnUpdate(dt)
    if UI.WasClicked("PauseButton") then
        UI.SetVisible("PauseMenu", true)
    end
end</code></pre>
<h2>Text input and sliders</h2><p>Native TextInput owns typing, cursor movement, selection and clipboard behavior. Lua can read/write text and test focus. Sliders use normalized values from 0 to 1 through <code>UI.SetValue</code>/<code>GetValue</code>.</p>
<h2>Runtime/editor separation</h2><p>The UI editor canvas is separate from the runtime <code>UICanvas</code>. Opening or editing a UI document does not replace game UI state.</p>`},
{group:"Systems",slug:"rendering",title:"Rendering & assets",html:`
<div class="eyebrow">Systems</div><h1>Rendering & assets</h1><p class="lead">The renderer supports primitive/external meshes, materials, lights, environment settings and runtime graphics controls.</p>
<h2>Meshes and materials</h2><p>Mesh components can reference engine primitives or an external model path. The built-in loader currently documents OBJ support. Material data includes metallic, roughness, ambient occlusion, emissive and normal/metallic/roughness/AO/emissive map paths.</p>
<h2>Lights</h2><p>Light types are Directional, Point and Spot. Light components contain color, direction, intensity, range, spot inner/outer angles and shadow casting. Lua can adjust intensity/color for an entity.</p>
<h2>Graphics API</h2><p>Runtime scripts can control anti-aliasing, MSAA samples, shadows, shadow quality/distance, fog, bloom, view distance, exposure, fog density, bloom strength and camera FOV. <code>Graphics.Save()</code> persists project rendering settings.</p>
<h2>Not yet: skeletal animation</h2><div class="callout warn">The current mesh/model path does not provide an Animator/Skeleton/AnimationClip system. Do not assume FBX/glTF skeletal playback or Lua animation methods exist yet.</div>`},
{group:"Systems",slug:"networking",title:"Networking & players",html:`
<div class="eyebrow">Systems</div><h1>Networking & player primitives</h1><p class="lead">The engine provides transport/state primitives; the project owns replication policy and game rules.</p>
<h2>Session flow</h2><pre><code>if Network.Host(7777) then
    Scene.Load("Assets/Scenes/Arena.scene")
end

if Network.Join("127.0.0.1", 7777) then
    Scene.Load("Assets/Scenes/Arena.scene")
end</code></pre>
<h2>Messages and transforms</h2><p><code>Network.SendMessage(channel, payload)</code> sends project-defined messages. <code>ConsumeMessages()</code> returns sender ID, channel and payload. Transform helpers send a local transform and expose remote transform states; interpolation and gameplay authority remain project code.</p>
<h2>Controller, Pawn and PlayerStart</h2><p><code>PawnComponent</code> stores a controller ID. <code>Controller.Possess</code>/<code>Unpossess</code> manage ownership, <code>IsLocallyControlled</code> gates local input, and <code>GetPlayerStart(slot)</code> finds generic spawn markers. This mirrors the “engine provides primitives, game defines rules” architecture.</p>
<h2>Authority</h2><p>The included DuelFPS project demonstrates host-owned match state and project-level message channels. The engine does not hard-code rounds, health, weapons or scoring.</p>`},
{group:"Reference",slug:"api",title:"Lua API reference",html:`
<div class="eyebrow">Reference</div><h1>Lua API reference</h1><p class="lead">Globals injected into each Lua script environment by the current engine bindings.</p>
<h2>Transform & time</h2>
<details class="api" open><summary><code>transform</code> — current entity</summary><div class="body"><code>GetPosition()</code> · <code>SetPosition(x,y,z)</code> · <code>Translate(x,y,z)</code> · <code>GetScale()</code> · <code>SetScale(x,y,z)</code> · <code>GetRotation()</code> · <code>SetRotation(x,y,z)</code>. Rotation getters/setters use degrees at the Lua boundary.</div></details>
<details class="api"><summary><code>Time</code></summary><div class="body"><code>GetDeltaTime()</code>. <code>OnUpdate</code> also receives delta time directly.</div></details>
<h2>Input & camera</h2>
<details class="api" open><summary><code>Input</code></summary><div class="body"><code>IsKeyDown(name)</code> · <code>IsKeyPressed(name)</code> · <code>IsMouseButtonDown(button)</code> · <code>GetMouseDeltaX()</code> · <code>GetMouseDeltaY()</code> · <code>SetCursorVisible(bool)</code> · <code>GetClipboardText()</code> · <code>SetClipboardText(text)</code>.</div></details>
<details class="api"><summary><code>Camera</code></summary><div class="body"><code>Move(forward,right,up,dt)</code> · <code>Rotate(yaw,pitch)</code> · <code>GetPosition()</code> · <code>SetPosition(x,y,z)</code> · <code>SetRotation(yaw,pitch)</code> · <code>Reset()</code> · <code>GetForward()</code> · <code>GetRight()</code> · <code>SetActive(entityID)</code> · <code>GetActive()</code> · <code>RotateEntity(entityID,yawDelta,pitchDelta)</code> · <code>SetEntityFOV(entityID,fov)</code>.</div></details>
<h2>Scene & entities</h2>
<details class="api" open><summary><code>Scene</code></summary><div class="body"><code>Load(path)</code> · <code>FindEntity(name)</code> · <code>InstantiatePrefab(path,parentID)</code> · <code>DuplicateEntity(id,includeChildren)</code> · <code>DestroyEntity(id)</code> · <code>GetParent(id)</code> · <code>SetParent(child,parent,keepWorld)</code> · <code>ClearParent(id,keepWorld)</code> · <code>GetPosition(id)</code> · <code>SetPosition(id,x,y,z)</code> · <code>SetRotation(id,x,y,z)</code> · <code>SetScale(id,x,y,z)</code> · <code>SetInteractableEnabled</code> · <code>SetInteractablePrompt</code> · <code>SetLightIntensity</code> · <code>SetLightColor</code> · <code>GetInteractionPrompt()</code> · <code>SetPaused(bool)</code> · <code>IsPaused()</code>.</div></details>
<details class="api"><summary><code>self</code> entity handle</summary><div class="body"><code>id</code> · <code>IsValid()</code> · <code>GetID()</code> · <code>GetPosition/Rotation/Scale()</code> · <code>SetPosition/Rotation/Scale(...)</code> · <code>Translate(...)</code> · <code>ClearParent(keepWorld)</code> · <code>Destroy()</code> · interaction/light setters.</div></details>
<h2>Gameplay systems</h2>
<details class="api"><summary><code>CharacterController</code></summary><div class="body"><code>Move(x,z)</code> · <code>Jump()</code> · <code>IsGrounded()</code>.</div></details>
<details class="api"><summary><code>Physics</code></summary><div class="body"><code>Raycast(ox,oy,oz, dx,dy,dz, maxDistance [, ignoreEntityID])</code>.</div></details>
<details class="api"><summary><code>Collider</code></summary><div class="body"><code>IsEnabled()</code> · <code>SetEnabled(bool)</code> for the current entity.</div></details>
<details class="api"><summary><code>Mesh</code></summary><div class="body"><code>GetRotation()</code> · <code>SetRotation(x,y,z)</code> for the current entity mesh offset rotation.</div></details>
<details class="api"><summary><code>Interactable</code></summary><div class="body"><code>SetPrompt(text)</code> for the current entity.</div></details>
<details class="api"><summary><code>Debug</code></summary><div class="body"><code>DrawLine(sx,sy,sz, ex,ey,ez [,r,g,b,duration])</code>.</div></details>
<h2>UI, state & preferences</h2>
<details class="api" open><summary><code>UI</code></summary><div class="body"><code>Load(path)</code> · <code>Clear()</code> · <code>IsLoaded(name)</code> · <code>SetVisible(name,bool)</code> · <code>SetEnabled(name,bool)</code> · <code>SetText/GetText</code> · <code>IsFocused</code> · <code>SetColor</code> · <code>SetPosition</code> · <code>SetValue/GetValue</code> · <code>WasClicked</code> · <code>IsHovered</code>.</div></details>
<details class="api"><summary><code>State</code></summary><div class="body"><code>SetNumber/GetNumber</code> · <code>SetBool/GetBool</code>. Runtime shared state for project systems.</div></details>
<details class="api"><summary><code>Preferences</code></summary><div class="body"><code>LoadString(key,fallback)</code> · <code>SaveString(key,value)</code>. Small per-project string storage under <code>Saved/</code>; keys are sanitized.</div></details>
<h2>Graphics & audio</h2>
<details class="api"><summary><code>Graphics</code></summary><div class="body"><code>SetAntiAliasing</code> · <code>SetShadows</code> · <code>SetAntiAliasingSamples</code> · <code>SetShadowQuality</code> · <code>SetShadowDistance</code> · <code>SetFog</code> · <code>SetBloom</code> · <code>SetViewDistance</code> · <code>SetExposure</code> · <code>SetFogDensity</code> · <code>SetBloomStrength</code> · <code>GetViewDistance</code> · <code>GetAntiAliasingSamples</code> · <code>GetFog</code> · <code>GetBloom</code> · <code>GetShadowQuality</code> · <code>Save</code> · <code>SetFOV/GetFOV</code>.</div></details>
<details class="api"><summary><code>Audio</code></summary><div class="body"><code>SetMasterVolume/GetMasterVolume</code> · <code>SetSFXVolume/GetSFXVolume</code> · <code>SetUIVolume/GetUIVolume</code>. No general Lua PlaySound binding currently.</div></details>
<h2>Networking & ownership</h2>
<details class="api"><summary><code>Network</code></summary><div class="body"><code>Host</code> · <code>Join</code> · <code>Disconnect</code> · <code>IsHost</code> · <code>IsConnected</code> · <code>IsReady</code> · <code>WasKickedByHost</code> · <code>GetPlayerCount</code> · <code>GetLastError</code> · <code>GetLocalPlayerID</code> · <code>SendMessage</code> · <code>ConsumeMessages</code> · <code>SendTransform</code> · <code>GetRemoteTransforms</code>.</div></details>
<details class="api"><summary><code>Controller</code></summary><div class="body"><code>GetLocalID()</code> · <code>Possess(entityID [,controllerID])</code> · <code>Unpossess(entityID)</code> · <code>GetPawnControllerID(entityID)</code> · <code>IsLocallyControlled(entityID)</code> · <code>GetPlayerStart([slot])</code>.</div></details>`},
{group:"Reference",slug:"limitations",title:"Current limitations",html:`
<div class="eyebrow">Reference</div><h1>Current limitations & roadmap edges</h1><p class="lead">Knowing what is not implemented is part of useful documentation.</p>
<h2>Animation</h2><p>There is no skeletal animation pipeline, Animator component, animation clip player or skinning API in the current source.</p>
<h2>Audio scripting</h2><p>The native AudioEngine can play sounds and UI buttons can carry click-sound paths, but Lua currently exposes volume controls only. A generic <code>Audio.PlaySound</code> call would require a new binding.</p>
<h2>Model import</h2><p>The current Mesh component explicitly documents OBJ support for external models. Do not rely on skeletal FBX/glTF behavior.</p>
<h2>Gameplay abstraction</h2><p>There is intentionally no engine-level “health”, “weapon”, “round”, “inventory” or “win condition” system. Those are project rules built from engine primitives.</p>
<h2>Documentation versioning</h2><p>This site describes the <code>ui-rewrite</code> branch at the time it was generated. When bindings/components change, update the API page alongside the engine change.</p>`}
];