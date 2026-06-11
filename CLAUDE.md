# CLAUDE.md — Peach Party project context

> **Purpose:** persistent project memory for Claude across sessions. Read this first.
> **Maintenance rule (user request, 2026-06-11):** update this file on **every** project change —
> add a Changelog entry and adjust the relevant section. Keep it accurate; it's the source of truth
> when a session resets.

---

## 1. What this is

**Peach Party** — a UE5 multiplayer party game. Players spawn in a school hub, sit at PCs to ready up,
play 2 short minigames as 1v1s, gain team advantages, then enter a first-person combat finale.

- **Engine:** Unreal Engine **5.7** (user builds/tests on **5.7.4**, Windows + Visual Studio 2022).
- **Multiplayer:** Listen Server, **2–8 players**.
- **Repo:** GitHub `DriveThruG/Peach-Party` (branch `main`), SSH push set up from the dev box.
- **Module:** single runtime module `PeachParty`, flat folders `Core/ Interaction/ Minigame/`.

## 2. Dev environment — IMPORTANT constraints

- The working dir (`/home/luis/luis_projekt`, a headless Linux LXC container) has **NO engine and no
  GPU**. Claude **cannot compile or run** here. All code is written against stable UE 5.7 APIs and
  reviewed by hand; the user builds on their Windows machine and pastes back errors.
- User works from a **GitHub ZIP download** (not a git clone), so pushes only reach them after they
  re-download. Give them either the re-download step or the exact local one-line edit.

## 3. Build setup — hard-won gotchas (do not regress these)

These cost several build rounds. They live in `Source/*.Target.cs` and `Source/PeachParty/PeachParty.Build.cs`:

1. **`DefaultBuildSettings = BuildSettingsVersion.V6`** in BOTH `PeachParty.Target.cs` and
   `PeachPartyEditor.Target.cs`. V5 caused `PeachPartyEditor modifies the values of properties
   [UndefinedIdentifierWarningLevel] ... not allowed` (shared build env with UnrealEditor).
2. **`PublicIncludePaths.Add(ModuleDirectory);`** in `PeachParty.Build.cs`. V6 disables legacy include
   paths, so folder-relative includes like `"Core/PPTypes.h"` fail with C1083 without this.
3. **`bUseUnity = false;`** in `PeachParty.Build.cs`. Cuts peak compiler memory (avoids
   `C3859 Failed to create virtual memory for PCH` / `C1076 internal heap limit` on lower-RAM machines)
   and surfaces all real errors at once instead of unity-masking them.
4. **Warnings are errors.** `C4458 'X' hides class member` WILL fail the build. Never name a local/param
   after a base member (`Owner` on AActor; `Result`/`Player1`/`Player2`/`Team`/`Duration` etc. on our
   own classes). Use `In`-prefixed names (`InOwner`, `InTeam`) or distinct names (`Outcome`).
5. The PCH/heap "Low on memory" kill loop is **environmental** (machine out of RAM), not code — reboot,
   close apps, only run VS, optionally grow the Windows pagefile.
6. **`.gitignore` must not eat committed configs.** A `Config/*Editor*User*.ini` pattern silently
   ignored `Config/DefaultEditorPerProjectUserSettings.ini` (PIE play settings) so it never reached the
   user. Only `Saved/Config/` is per-user; everything in `Config/Default*.ini` is a committed default.
   When a file "doesn't arrive", run `git check-ignore -v <file>`.

## 4. Architecture

Authority on the listen-server host; clients are view + input. **Clients request → server decides →
result replicates.**

### Phases (`EMatchPhase` in `Core/PPTypes.h`)
`None → Lobby → Minigame → Reward → Final → PostMatch`, driven by `APPGameMode` (server-only),
replicated via `APPGameState::CurrentPhase` (`OnRep_Phase`).

### Core classes (`Source/PeachParty/Core/`)
| Class | Lives on | Role |
|---|---|---|
| `APPGameMode` | server only | Phase state machine, ready-gate + countdown, team assignment, **minigame round matchmaker**, scoring. |
| `APPGameState` | server+clients | Replicated truth: phase, team scores, `PhaseEndServerTime` (countdown = end-time minus `GetServerWorldTimeSeconds`), `ActiveMinigames` list. |
| `APPPlayerState` | server+clients | `Team`, `bIsReady`, `MatchScore`, `CurrentMinigame` (gates spectating). |
| `APPPlayerController` | server+owner | Interaction RPCs, camera (`ServerViewTarget` owner-only), `ServerCycleSpectate`, `ServerMinigameInput`. |
| `APPCharacter` | server+clients | **First-person** pawn: walk/sprint/jump/crouch (networked via CharacterMovement), interaction trace, minigame input forwarding. |

### Interaction (`Source/PeachParty/Interaction/`)
- `IPPInteractable` — interface: `ServerInteract` (authoritative) + client focus hooks.
- `APPPCStation` — the PC actor. Sit = ready (Lobby). Server-authoritative `OccupantPlayerState`.
  Carries a camera; seating switches the player's view via `ServerViewTarget`.

## 5. Lobby / Ready / Teams

- Sitting at a `APPPCStation` in Lobby sets `bIsReady`. When all connected players (≥ `MinPlayersToStart`)
  are ready, `APPGameMode` arms a countdown (`PhaseEndServerTime`); un-ready cancels it.
- `AssignTeams()` alternates A/B over `PlayerArray` (size-balanced). Teams are **fixed** for the whole
  match (minigames + final).

## 6. Minigame system — per-round 1v1 matchmaker (in `APPGameMode`)

Two rounds, opponents **reshuffled each round** (teams stay fixed): Round 0 = Peach Basket, Round 1 =
Peach Artillery. **Each individual 1v1 win = +1 team point.**

- `StartRound` shuffles each team, pairs A[i] vs B[i], spawns one `APPMinigameBase` per pair at a
  **recycled arena slot** `slot * ArenaSpacing` apart (spatial separation = player isolation, no
  collision channels). Match actors are `bAlwaysRelevant` (far arenas would otherwise be net-culled).
- **Odd headcount:** the unmatched player on the bigger team waits/spectates; when the first
  opposite-team player finishes, those two play a **bonus match** (both results count — user's choice).
- `NotifyMinigameFinished` scores the win, releases players, may spawn the bonus, advances the round.
  Global timeout force-resolves everything. Then `FinishMinigamePhase`: it releases ALL PC stations
  (`ServerReleaseOccupant` on every `APPPCStation` → screens off + each player's `SeatedStation`
  cleared → camera blends back to their FP pawn = **back in the 3D world**) → `StartRewardPhase`.
- **Spectator** = camera-only: `ServerCycleSpectate` (←/→) points `ServerViewTarget` at another live
  match. Server-gated ("not mid-match"). Cannot interfere by construction.

### `Minigame/` actors
- `APPMinigameBase` (abstract `AActor`) — one 1v1 match: 2 players, scores, `Duration` timer, camera,
  `Result`. Reports to GameMode via `FinishWithResult`. `HandleInput(player, action, bPressed)` is the
  routed-input hook. Subclass override points: `OnMinigameStarted/Finished`, `ForceResolve`.
- `APPPeachBasketGame` (+ `APPBasketBall`, `APPBasketCharacter`, `APPBasket`).
- `APPPeachArtilleryGame` (+ `APPTank`, `APPProjectile`, `PPArtilleryTypes.h`).

### Paper2D sprites (Peach Basket)
Paper2D is enabled (plugin + `Paper2D` in Build.cs). The basket **character** now uses 3
`UPaperSpriteComponent` layers (back arm = `Arm_Left`, body = `Player0X_Body`, front arm =
`Player0X_Arm`) under a shoulder `ArmPivot` that pitches to raise the arms. `SpriteVariant` (1-4,
replicated) picks the Player0X art (textures already team-coloured). Ball + hoop also have sprite
components (hide the placeholder mesh when the sprite resolves). Physics bodies (basket char + ball)
are locked to the **X-Z plane** (`BodyInstance.DOFMode = EDOFMode::XZPlane`) → a true 2D side view;
chars face the camera (yaw 0), jump along their tilted up-vector, throw toward the enemy (FacingSign).
Sprites loaded by path from `/Game/PeachParty/Minigames/BasketPeach/Graphics/<Tex>_Sprite` (user must
create the sprites via right-click texture → Create Sprite; **those assets live in the user's Content,
not the repo** — they must be preserved on update). `SpriteFacing` (EditDefaultsOnly) tunes camera
facing if sprites look edge-on. Artillery still uses the flat-quad filler below.

### Minigame visuals (filler, 2D look — artillery + fallback)
Until real Paper2D sprite assets exist (Claude can't author `.uasset`/textures headless), visuals are
**flat camera-facing quads** (cubes flattened along Y, the camera-depth axis) tinted by a dynamic
material via `Minigame/PPVisual.h` (`Tint` + `TeamColor`: A=blue, B=red; ball=orange, hoop=red,
shell=yellow, terrain=brown). The basket character is two parts as requested: a **body+head quad** and
a **separate arms quad** that rotates up while charging (eased locally from the replicated `bCharging`).
`Team` is replicated on the basket character + tank so clients tint correctly (`OnRep_Team`).
**Caveat:** tint needs the mesh material to expose a `Color`/`BaseColor` param; if the engine BasicShapes
material doesn't, shapes show default-grey — assign a coloured material/sprite in a BP child. Swap each
visual `UStaticMeshComponent` for a `UPaperSpriteComponent` later (call sites only set the tint).

### Peach Basket (real-time physics)
- One input (**Space**). Each player drives **two** wobbly server-simulated physics capsules.
- **Space = charge:** press → jump (impulse along current tilted facing) + arms start rising slowly
  (`ArmAngleDeg` climbs server-side). **Release angle sets throw elevation** — too low = short, sweet
  spot = into basket. Horizontal dir = body facing (so orientation matters too). Speed fixed + spread.
- Grab/steal/score are **server proximity checks in Tick** (no collision channels). First to
  `TargetScore`; reset positions after each basket; time-out falls back to score.
- Replication: ball + characters server-simulated with movement replication; `bCharging` replicates
  (clients animate arms locally); the precise angle is server-only.

### Peach Artillery (turn-based)
- Strictly turn-based. `ActiveSlot` + `bTurnInProgress` replicated → everyone agrees whose turn / input
  locked. Only the active player's input is accepted.
- Discrete keys: **A/D** move (fuel, non-regen), **W/S** aim, **R/F** power, **Q** weapon, **Space** fire.
- Tanks are **kinematic** (deterministic, easy to replicate). Shell uses `UProjectileMovementComponent`
  (gravity arc, movement-replicated). Impact = area damage with linear falloff. KO wins; time-out =
  higher remaining health. A lifespan-expiry safety reports a miss so a lost shell can't hang the turn.
- Weapons: small fixed set (`Shell`, `Heavy`). No terrain destruction / wind / multi-shot (out of scope).

### Input plumbing
Client `APPCharacter` action handlers → `APPPlayerController::ServerMinigameInput(Action, bPressed)` →
`PlayerState->CurrentMinigame->HandleInput(...)`. Forwarded only while in a match (client-side gate).
Verbs: `Primary, Left, Right, Up, Down, Power+, Power-, Weapon`.

## 7. First-person character (`APPCharacter`)

- Eye-height `FirstPersonCamera`, body yaws with controller. WASD + mouse, **Shift** = **toggle**
  sprint (replicated flag + `ServerSetSprint`), **Space** jump, **C/Ctrl** crouch. All networked via
  `CharacterMovementComponent` (server-authoritative + client prediction). Capsule blocks geometry.
- Sprint is a TOGGLE (tap, not hold) on purpose: holding W+Shift+Space jams on many keyboards
  (3-key ghosting) and the jump won't register. Toggle removes Shift from the held set.
- Space is context-split: jump in hub, minigame "Primary" while playing (gated by `IsInMinigame()`).
- `BodyMesh` = placeholder cylinder, `SetOwnerNoSee(true)` so others see you but your own FP view doesn't.

## 8. Config

- `Config/DefaultEngine.ini` — sets `GlobalDefaultGameMode = APPGameMode`, net settings.
- `Config/DefaultInput.ini` — legacy action/axis mappings (movement, interact, spectate, minigame, MG_*).
- `Config/DefaultEditorPerProjectUserSettings.ini` — PIE defaults: **listen server, 4 players, one process**.

### Placeholder test hub (runtime-built — no level art needed)
`APPGameMode::BuildPlaceholderHub()` (server, `BeginPlay`, gated by `bSpawnPlaceholderHub`) spawns a
floor (`APPPlaceholderBlock`) and a row of `NumPlaceholderStations` PC stations.
`ChoosePlayerStart_Implementation` spawns spread-out `APlayerStart`s when the level has none. All
placeholder geometry uses engine `BasicShapes`. Turn `bSpawnPlaceholderHub=false` once a real level
exists. (Claude can't author/delete `.umap` files from the headless box, hence runtime-spawning.)
An optional directional light is gated by `bSpawnPlaceholderLight` (**default false** — the level's own
lighting otherwise "competes" with it; only enable on a truly empty level).

## 9. Current state

- ✅ Compiles + links on UE 5.7.4 (user confirmed project opens). Not yet play-tested by the user.
- ✅ Implemented: phases, ready/teams, full minigame matchmaker, both minigames' gameplay, FP character.
- 🔲 Stubs / TODO: **Reward phase** (grant winning team a Final-phase advantage), **Final first-person
  combat phase** (weapons, combat rules), HUD/UMG, real art (currently engine `BasicShapes` placeholders).

## 10. Conventions

- Class prefix `APP*` / `UPP*` / `IPP*` / `EPP*` (`PP` = Peach Party).
- Server-authoritative everything; replicate state, not commands. `OnRep_*` mirrors are called manually
  on the host (listen server) since OnRep doesn't fire on the authority.
- Prefer simple/stable over flexible (user's explicit priority). Proximity checks over collision
  channels; kinematic over physics where turn-based; spatial separation over channel filtering.
- After meaningful changes: `git add -A && git commit && git push` (commit trailer: Co-Authored-By Claude).

## 11. Changelog

- **2026-06-10** — Initial scaffold: project files, phases, ready system, team assignment, PC stations.
- **2026-06-10** — Reworked minigames to per-round reshuffled 1v1s (dropped the `MatchInstance` class);
  GameMode matchmaker + spectator + odd-player bonus match.
- **2026-06-10** — Implemented both minigames' gameplay (Basket physics/charge-throw, Artillery turns).
- **2026-06-10/11** — GitHub repo + SSH push set up.
- **2026-06-11** — Build fixes: V6 build settings, `PublicIncludePaths.Add(ModuleDirectory)`,
  `bUseUnity=false`, fixed C4458 shadows (`Owner`, `Result`). Project now compiles.
- **2026-06-11** — First-person character (walk/sprint/jump/crouch, networked); PIE defaults to
  listen server + 4 players. Created this CLAUDE.md.
- **2026-06-11** — Filler visuals so functions are visible without art: player `BodyMesh` (cylinder,
  owner-hidden), PC stations get desk+screen cubes, runtime placeholder hub (floor/stations/light) +
  spread player spawns via `ChoosePlayerStart`. Sprint changed to a toggle (fixes W+Shift+Space
  jump jam from keyboard ghosting). New class `APPPlaceholderBlock`.
- **2026-06-11** — Fixed `.gitignore` eating `Config/DefaultEditorPerProjectUserSettings.ini` (PIE
  settings never reached the user). Placeholder light now OFF by default (`bSpawnPlaceholderLight`)
  to stop competing with the level's own lights.
- **2026-06-11** — Default map set to `/Game/Maps/PeachPartyHub` (user must create that empty level
  once in-editor — Claude can't author `.umap`). Placeholder light back ON by default for that empty
  level. NOTE: if `PeachPartyHub` doesn't exist yet, PIE will error until it's created at that path.
- **2026-06-11** — Minigame 2D filler visuals: flat camera-facing quads tinted by team/role via new
  `Minigame/PPVisual.h`. Basket character split into body quad + rotating arms quad. `Team` replicated
  on basket character + tank for client tinting. Tanks/bodies flattened along the camera-depth axis.
- **2026-06-11** — Flow close-out: after both minigames, `FinishMinigamePhase` releases every PC
  station so players stand up (PCs off) and return to the 3D world. Placeholder stations bumped to 8
  (one per max player) so everyone can sit / ready up.
- **2026-06-11** — Paper2D enabled; Peach Basket switched to real sprites referencing the user's
  imported textures by path (body + 2 arms layered, ball, hoop). Physics locked to X-Z plane (2D side
  view), chars face camera, 4 art variants via replicated `SpriteVariant`.
- **2026-06-11** — User committed their basket textures to the repo at
  `Content/PeachParty/Minigames/BasketPeach/Graphics/` (Texture2D `.uasset`, incl. the `PLayer01_Arm`
  typo). Switched to a **git-clone workflow** (Windows, HTTPS — SSH key only existed on the Linux box).
  Code now references those TEXTURES by exact path and builds Paper2D sprites from them at runtime
  (`PPVisual::SpriteFromTexture`, **editor-only** `InitializeSprite` — fine for PIE, needs real sprite
  assets for a packaged build). No hand-created sprites needed from the user.
