# CLAUDE.md — Peach Party project context

> **Purpose:** persistent project memory for Claude across sessions. Read this first.
> **Maintenance rule (user request, 2026-06-11):** update this file on **every** project change —
> add a Changelog entry and adjust the relevant section. Keep it accurate; it's the source of truth
> when a session resets.

---

> ## ⚠️ SCOPE RESET — 2026-06-13 (READ FIRST)
> The project was **stripped down to ONLY the Peach Basket minigame** (user request, they kept a full
> backup). **Deleted:** the 3D hub/first-person character, Peach Artillery, the old 3D basket, the Final
> combat phase (`Final/`), interaction/PC-stations (`Interaction/`), the menu + hosting/join flow
> (`Menu/`, `PPGameInstance`), placeholder hub, team player-starts, lobby/ready/rounds/reward systems.
> **Online removed entirely** — no menus, no host/join. **Flow now:** 2 players connect (PIE: Number of
> Players = 2) → both are auto-assigned Team A/B and **dropped straight into one Peach Basket match**;
> when it ends it auto-restarts after `RestartDelaySeconds`. Still a **listen server** (PIE multiplayer),
> just no connection UI — two separate PCs would need a direct-connect added later.
> **Sections 4–8, 9 below are HISTORICAL** (describe the deleted systems) — trust §9c "CURRENT STATE
> (post-reset)" and §9b file map instead.

## 1. What this is

**Peach Basket** (was "Peach Party") — a UE5 **local 2-player** minigame. Two players, one button each
(Space), play a 2D basket match rendered by a UMG widget. Server-authoritative on a listen server.

- **Engine:** Unreal Engine **5.7** (user builds/tests on **5.7.4**, Windows + Visual Studio 2022).
- **Multiplayer:** Listen Server, **2 players, local/PIE only** (online/hosting removed 2026-06-13).
- **Repo:** GitHub `DriveThruG/Peach-Party` (branch `main`), SSH push set up from the dev box.
- **Module:** single runtime module `PeachParty`, folders `Core/ Minigame/` only.

## 2. Dev environment — IMPORTANT constraints

- The working dir (`/home/luis/luis_projekt`, a headless Linux LXC container) has **NO engine and no
  GPU**. Claude **cannot compile or run** here. All code is written against stable UE 5.7 APIs and
  reviewed by hand; the user builds on their Windows machine and pastes back errors.
- **Workflow is git-clone** (HTTPS, since the SSH key only lives on the Linux box). User's clone:
  `C:\Users\tamar\Downloads\Peach-Party-Main`. After a push, user runs `git pull` in that folder, then
  right-click `.uproject` → Generate VS Files → open VS → Build. If "no changes appear", verify the
  folder is the *clone* (`-Main`) not the old ZIP (`-main`); when in doubt, delete+re-clone.
- Claude **cannot author binary `.uasset`/`.umap` files**. Anything content-side (levels, sprite assets,
  materials, BPs, UMG widgets) must be created/saved by the user in-editor. Code references content by
  PATH and resolves it at runtime.

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

## 6b. Final phase — frontline combat (`Final/`)

Attackers vs Defenders over **3 sequential objective rooms** (`APPObjectiveRoom`, placed in the level
with `RoomIndex` 1/2/3). Server-authoritative throughout.

- **Roles:** the minigame winner attacks first (`GameState.AttackingTeam`); a player's role = their
  fixed team vs the attacking team. Teams stay fixed.
- **Frontline:** `APPGameMode` opens exactly ONE room at a time (`ActivateRoom`). Capture in order only.
  `APPObjectiveRoom::Tick` (server, active room) sums attackers vs defenders in its zone (distance
  check, no overlap bookkeeping): attackers with no defender present fill the bar (faster with more
  attackers + `CaptureSpeedMul`, Engineers high); a defender stalls/reverses it. At 100% → 
  `NotifyRoomCaptured` → lock room, `ActivateRoom(next)` → **timer resets** (`RoomTimeLimit`) + both
  teams reposition (`RespawnNow` at the new room's attacker/defender spawn points). Past the last room
  → attackers win; room timer expires → defenders win (`EndFinalPhase`).
- **Classes** (`Final/PPClassTypes.h`): `EPPClass {Sprayer,Punisher,Engineer,Runner}` + `FPPClassStats`
  (move/fire/spread/ammo/wetness/capture/refill). Server defaults in `PPClass::GetDefaults`. Stored on
  `APPPlayerState::SelectedClass` (replicated); chosen via `PlayerController::ServerSelectClass`, which
  the PlayerState gates to the **slipping/respawn window only** (no mid-life switching). Applied on
  spawn in `APPCharacter::ApplyClassStats` (movement + ammo) and read live by fire/capture.
- **Combat:** `APPCharacter::ServerFire` (fire-rate + ammo gated, server aim via `GetBaseAimRotation`)
  spawns `APPWaterProjectile` (projectile, not hitscan; movement-replicated; **team-coloured via
  replicated `InstigatorTeam` → `OnRep_Team` tint**; `MulticastImpact` → `BP_OnImpact(Loc, Team)` splash
  hook). On hitting an ENEMY → `Character::ApplyWetness` → `PlayerState::AddWetness`; at 100 →
  `bIsSlipping`, `MulticastSlip` (DisableMovement + DisableInput + random launch impulse + `BP_OnSlip`),
  `GameMode::ScheduleRespawn`. Friendly water does nothing.
- **Gravity object (`APPGrabbableObject`):** RMB (`Grab` input) traces forward for an object; pick up =
  physics off, attach to character's `HoldPoint` (in front of camera). RMB again drops; LMB while
  holding throws (`ServerThrow`, impulse along aim). Fast enemy hit converts impact speed → wetness
  (clamped, friendly-safe) + knockback. `bIsHolding` replicated; **blocks `ServerFire`**.
- **Refill stations (`APPRefillStation`):** proximity (`Radius`) ammo top-up on a server timer; no
  manual reload anywhere else. Runner refills faster (`RefillSpeedMul`). `AddAmmo` clamps to the
  player's `GetEffectiveStats().AmmoCapacity`.
- **Caveats:** real ragdoll needs a skeletal mesh + physics asset (slip currently locks + impulses the
  capsule and triggers `BP_OnSlip`). 3 `APPObjectiveRoom` actors must be placed in-level with
  `RoomIndex` 1/2/3; `APPRefillStation` and `APPGrabbableObject` actors must be placed in the arena.

## 7. First-person character (`APPCharacter`)

- Eye-height `FirstPersonCamera`, body yaws with controller. WASD + mouse, **Shift** = **toggle**
  sprint (replicated flag + `ServerSetSprint`), **Space** jump, **C/Ctrl** crouch. All networked via
  `CharacterMovementComponent` (server-authoritative + client prediction). Capsule blocks geometry.
- Sprint is a TOGGLE (tap, not hold) on purpose: holding W+Shift+Space jams on many keyboards
  (3-key ghosting) and the jump won't register. Toggle removes Shift from the held set.
- Space is context-split: jump in hub, minigame "Primary" while playing (gated by `IsInMinigame()`).
- `BodyMesh` = placeholder cylinder, `SetOwnerNoSee(true)` so others see you but your own FP view doesn't.

## 8. Config

- `Config/DefaultEngine.ini` — `GlobalDefaultGameMode=APPGameMode`, `GameInstanceClass=PPGameInstance`,
  `OnlineSubsystem=Null` (LAN/local), `EditorStartupMap`/`GameDefaultMap=/Game/Maps/PeachPartyHub`.
- `Config/DefaultInput.ini` — legacy action/axis mappings. Action verbs in use:
  `Jump, Sprint (toggle), Crouch, Fire (LMB), Grab (RMB), Interact (E), SpectateNext/Prev (←/→),
  MGPrimary (Space), MGLeft/Right/Up/Down, MGPowerUp/Down, MGWeapon`.
- `Config/DefaultEditorPerProjectUserSettings.ini` — PIE defaults: **listen server, 4 players, one
  process**. (Was once eaten by `.gitignore` — see §3.6.)
- **Modules**: `Source/PeachParty/PeachParty.Build.cs` declares `Paper2D, OnlineSubsystem,
  OnlineSubsystemUtils`; `.uproject` enables `EnhancedInput, Paper2D, OnlineSubsystemNull`.

### Levels the user must create in-editor (Claude can't author `.umap`)
- `/Game/Maps/PeachPartyHub` — the empty default map. Runtime-spawned placeholder floor + 8 PC stations
  cover it for now. PIE errors out until this asset exists at that exact path.
- A final arena level. Must contain 3 `APPObjectiveRoom` actors (`RoomIndex` 1, 2, 3) with
  attacker/defender spawn points, plus `APPRefillStation` and `APPGrabbableObject` actors as desired.

### Placeholder test hub (runtime-built — no level art needed)
`APPGameMode::BuildPlaceholderHub()` (server, `BeginPlay`, gated by `bSpawnPlaceholderHub`) spawns a
floor (`APPPlaceholderBlock`) and a row of `NumPlaceholderStations` PC stations.
`ChoosePlayerStart_Implementation` spawns spread-out `APlayerStart`s when the level has none. All
placeholder geometry uses engine `BasicShapes`. Turn `bSpawnPlaceholderHub=false` once a real level
exists. (Claude can't author/delete `.umap` files from the headless box, hence runtime-spawning.)
An optional directional light is gated by `bSpawnPlaceholderLight` (**default false** — the level's own
lighting otherwise "competes" with it; only enable on a truly empty level).

## 9. Current state  — END OF SESSION 2026-06-12 (READ THIS)

**The project compiles & runs.** Lots was verified live this session (menu, lobby, teams, minigame
start). Workflow unchanged: Claude can't compile here; user builds on Windows, pastes errors, `git
pull`/build. **Always `git pull` first** — the user pushes `.uasset`/`.umap` from Windows between turns.

### What works now
- ✅ **Menu flow:** `WBP_MainMenu` → `WBP_MultiplayerMenu` (Host + an IP textbox + Connect). **Host works**
  (listens on 7777). **LAN server-list discovery does NOT work on the user's network** (firewall blocks
  the NULL beacon → `FindGames` returns `found=0`). **Use `UPPGameInstance::JoinByIP`** + `127.0.0.1`
  (local 2-proc) or the host's IP. `WBP_ServerBrowser`/`WBP_ServerEntry` exist but are unused (can delete).
- ✅ **MainMenu map** uses `Menu/PPMenuGameMode` (no pawn). `APPCharacter::PawnClientRestart` restores
  game-input + clears leftover menu widgets on lobby entry.
- ✅ **Teams assigned ON JOIN** (`PickJoinTeam`, balanced: 1st→A, 2nd→B…) so lobby spawns are correct.
  Start now requires **≥1 ready player per team** (a minigame is 1v1 → both sides must be filled).
- ✅ **UMG Peach Basket** — the big new thing (see below). GameMode round 0 spawns it.
- ✅ **Placeable level actors** (user builds the level by hand; `bSpawnPlaceholderHub=false`):
  `APPTeamAPlayerStart` (blue arrow) / `APPTeamBPlayerStart` (red) for spawns; `APPPCStation` (placeholder
  desk+screen cubes, optional `StationMesh` + `bHidePlaceholderBlocks`). Seated PC = fade-to-black hold;
  PC seat cam = perspective, minigames = ortho.
- ✅ Fixed "players half in floor" (`BodyMesh` Z 0). Note: imported floor meshes need collision added
  (Static Mesh editor → Add Box Simplified Collision) or players fall through.

### UMG Peach Basket (minimal-BP minigame) — current focus
- `Minigame/PPPeachBasketUMGGame` runs the whole 1v1 server-side in normalised 2D (no PhysX/3D), replicates
  `FPPBasketState` (`PPBasketUMGTypes.h`). Mechanics: one input (Primary) = jump-along-lean (pendulum →
  sideways) + raise arms; release = auto-aim throw at enemy hoop scaled by arm height; hands grab/steal
  ALWAYS; 5 to win / 120s. `GetState()` for the widget.
- **`WBP_BasketGame`** (user built, at `/Game/PeachParty/UI/`) — Tick reads `GetState()` and positions
  Images via `UPPBasketWidgetLib::SetCanvasPos` (normalised→canvas). Auto-shown by
  `APPPlayerController::PlayerTick`/`UpdateMinigameHud` while viewing a UMG basket (`GetViewedMinigame`).
- **`BP_PeachBasketUMG`** — user creates this as a BP child of `PPPeachBasketUMGGame` at
  `/Game/PeachParty/Blueprints/`; GameMode `BeginPlay` auto-prefers it (LoadClass). It exposes **all**
  tunables for LIVE editor tuning (no rebuild): `GroundY`, `HoopLeftPos/HoopRightPos`,
  `CharStartPositions[4]`, `BallStartPos`, `JumpImpulse`, `SlideFriction/AirDrag/MaxLean`, `ThrowFlightTime`,
  `GrabRange/ScoreRange`, `TargetScore`. **Next:** user tunes these to match their `WBP_BasketGame` layout.
- **LIVE TUNING (no compile, no PIE restart):** the basket tunables are now `EditAnywhere`, so you select
  the RUNNING match actor in the PIE World Outliner and drag the values in Details. Feel values
  (`JumpImpulse`/`SlideFriction`/`MaxLean`/`Gravity`/…) are read every Tick → apply INSTANTLY. Layout/start
  values (`HoopLeft/RightPos`/`CharStartPositions`/`BallStartPos`/`GroundY`) take effect on the next score
  reset or when you click the **`DebugResetField`** button (a CallInEditor button in Details). When happy,
  bake the values into `BP_PeachBasketUMG` defaults so they persist.
- **SOLO preview** (`bDebugSoloBasket=True` in DefaultGame.ini, PIE players = 1): drops ONE player straight
  into a FREE-PLAY basket (no opponent, no 120s timer, scoring just resets) so you tune without launching
  two PIE processes. You drive the LEFT chars (team A, shoot the RIGHT hoop); the right chars stand idle as
  targets. Implemented via `bFreePlay` on the basket + `APPGameMode::StartSoloBasketPreview`.
- Config flags (`Config/DefaultGame.ini`): `bDebugSkipToBasket=True` (straight into basket on PIE; still
  needs 2 players), `bDebugSoloBasket` (1-player free-play tuning, default off), `bSpawnPlaceholderHub` false.

## 9c. CURRENT STATE (post-reset 2026-06-13) — READ THIS

**Scope = Peach Basket only, local 2-player.** All code above the basket was deleted (see banner). The
remaining flow is tiny and lives entirely in `Core/` + `Minigame/`:

- `APPGameMode` (≈230 lines): sets the 4 default classes; `PostLogin` assigns Team A/B (smaller team) +
  calls `TryStartMatch`; when one player is on each team it spawns ONE basket and binds both (their
  `CurrentMinigame` + camera view target). `NotifyMinigameFinished` scores the winning team then
  auto-restarts after `RestartDelaySeconds` (3s). `bDebugSoloBasket` → `StartSoloBasketPreview` (1 player,
  free-play). Prefers `BP_PeachBasketUMG` if it exists.
- `APPCharacter` = **input-only pawn**: no movement/look, capsule has NO collision + `AlwaysSpawn` (so the
  2nd pawn never fails to spawn at the origin). Binds only `MGPrimary` (Space) → `ServerMinigameInput`.
- `APPPlayerController`: forwards `ServerMinigameInput`, sets the view target, auto-shows `WBP_BasketGame`.
- `APPPlayerState`: just `Team` + `CurrentMinigame`. `APPGameState`: `CurrentPhase`
  (None/WaitingForPlayers/Playing) + team scores + active match.
- The basket itself (`APPPeachBasketUMGGame` + `PPBasketUMGTypes` + `PPBasketWidgetLib` + `PPMinigameBase`)
  is **unchanged** — same live-tuning (EditAnywhere tunables, `DebugResetField` button, `bFreePlay`).

**USER MUST DO (content, Claude can't author .umap):** create ONE near-empty map at
`/Game/PeachParty/Maps/BasketMap` (set as Editor Startup + Game Default; DefaultEngine.ini already points
there). No actors needed in it — the match is a full-screen UMG widget. Optionally re-create
`BP_PeachBasketUMG` (child of `APPPeachBasketUMGGame`) at `/Game/PeachParty/Blueprints/` for persistent
tuning. PIE: **Number of Players = 2**, Net Mode = Listen Server → both drop straight into the basket.

## 9b. Where things live (file map — post-reset)

```
Source/PeachParty/
  Core/       PPTypes, PPGameMode, PPGameState, PPPlayerState, PPPlayerController, PPCharacter
  Minigame/   PPMinigameBase, PPMinigameTypes,
              PPPeachBasketUMGGame + PPBasketUMGTypes.h + PPBasketWidgetLib   (the whole game)
Build.cs deps: Core/CoreUObject/Engine/InputCore/UMG/Slate/SlateCore (Online + Paper2D module deps removed).
.uproject plugins: EnhancedInput (+ Paper2D still enabled but unused). PPGameMode is UCLASS(Config=Game).
Kept content (repo): WBP_BasketGame + Minigames/BasketPeach/Graphics/*. User creates BasketMap (+ optional
BP_PeachBasketUMG). DefaultEngine.ini default map = /Game/PeachParty/Maps/BasketMap.
```

## 10. Conventions

- Class prefix `APP*` / `UPP*` / `IPP*` / `EPP*` (`PP` = Peach Party).
- Server-authoritative everything; replicate state, not commands. `OnRep_*` mirrors are called manually
  on the host (listen server) since OnRep doesn't fire on the authority.
- Prefer simple/stable over flexible (user's explicit priority). Proximity checks over collision
  channels; kinematic over physics where turn-based; spatial separation over channel filtering.
- After meaningful changes: `git add -A && git commit && git push` (commit trailer: Co-Authored-By Claude).

## 10b. Next-session start checklist

Read this file first, then before answering:

0. **`git pull --no-edit origin main` FIRST** — the user pushes `.uasset`/`.umap` between turns. Then
   read the **SCOPE-RESET banner** + **§9c CURRENT STATE** — the project is now **Peach Basket only,
   local 2-player** (everything else deleted 2026-06-13). Ignore the historical §4–§9.
1. **Most likely asks:** (a) compile errors from the reset → fix the named Core file, push, ask to
   pull+build (this is the expected first follow-up); (b) "value X feels wrong in the basket"
   (jump/slide/throw/grab/layout) → tunable on `PPPeachBasketUMGGame`, drag it live in the running actor's
   Details (EditAnywhere) or `BP_PeachBasketUMG`; (c) widget node help → `GetState()` +
   `UPPBasketWidgetLib::SetCanvasPos`; (d) "both players don't start" → need PIE Number of Players = 2 and
   the `BasketMap` to exist.
2. **Can't read `.uasset`/`.umap`** (binary). For widget/layout debugging, ask the user to paste the graph
   as T3D text (select nodes → Ctrl+C) or send a screenshot.
3. **Don't try to compile.** No engine on this box. Push and let the user build. New `.h`/`.cpp` files →
   tell them to **Generate VS project files** before building; edits-only → just build.
4. **Repo:** `git@github.com:DriveThruG/Peach-Party.git` (SSH from this box). User pulls via HTTPS.
   Close-out: `git add -A && git commit -m "…" && git push origin main`. If their push is rejected, they
   need `git pull --no-edit` then push.
5. **Honour the build gotchas in §3** — V6, `PublicIncludePaths.Add(ModuleDirectory)`, `bUseUnity=false`,
   **no member-shadow names** (e.g. `SpriteScale` clashed with `AActor`), and **LWC**: `FVector`/`FVector2D`
   components are `double` — cast to `float` before `FMath::Max`/float assignment or it won't compile.

## 11. Changelog

- **2026-06-13** — **Console tuning commands** (`Minigame/PPPeachBasketUMGGame.cpp`): `pp.basket <key>
  <v> [v2]`, `pp.basket.reset`, `pp.basket.dump` (FAutoConsoleCommandWithWorldAndArgs). Live-tune the
  basket from the `~` console with NO outliner / F8 / world juggling — the prior Details-panel path
  failed because the World Outliner shows the editor world during PIE, not the runtime actor. Backends
  `DebugSetTunable`/`DebugDumpTunables` on the basket (authority-only; `FindLiveBasket` picks the server
  instance, warns if run on a client window). Also fixed: client black screen (PawnClientRestart was
  nuking the basket HUD) and enabled `bDebugSoloBasket` for 1-player tuning.
- **2026-06-13** — **SCOPE RESET to Peach-Basket-only, local 2-player** (user request; full backup kept).
  Deleted ~22 source files + 130+ content assets: 3D/FP character internals, Peach Artillery
  (`PPTank/Projectile/Artillery`), old 3D basket (`PPPeachBasketGame/Ball/Character/Basket/PPVisual`),
  Final phase (`Final/`: water combat/rooms/classes/reward), `Interaction/` (PC-station), `Menu/`
  (`PPGameInstance` host/join + `PPMenuGameMode`), `PPPlaceholderBlock`, `PPTeamPlayerStart`, + all menu
  widgets / 3D school assets / ThirdPerson / maps. **Online removed entirely** (Build.cs Online+Paper2D
  deps gone; `.uproject` OnlineSubsystemNull gone; DefaultEngine `GameInstanceClass`/`[OnlineSubsystem]`
  gone). Rewrote the 6 Core classes minimal: `APPGameMode` = 2 players → assign A/B → spawn one basket →
  bind both → auto-restart on finish (`bDebugSoloBasket` keeps the 1-player free-play tuning path);
  `APPCharacter` = input-only pawn (Space → `ServerMinigameInput`, no collision, AlwaysSpawn);
  `APPPlayerController`/`State`/`GameState` trimmed to view-target + input + team/score. Basket game itself
  unchanged. DefaultEngine default map → `/Game/PeachParty/Maps/BasketMap` (**user must create it**).
  UNVERIFIED — user compiles next; expect a couple of error round-trips.
- **2026-06-12** — **Live basket tuning** (kill the edit→compile→PIE→close loop): (1) all
  `PPPeachBasketUMGGame` tunables `EditDefaultsOnly`→`EditAnywhere` so the RUNNING actor is editable in the
  PIE Details panel — feel values are read every Tick (instant), layout/start values apply via the new
  `DebugResetField` CallInEditor button or a score reset. (2) **Solo free-play preview**: new
  `bDebugSoloBasket` (config) + `APPGameMode::StartSoloBasketPreview` drops ONE PIE player into a
  no-opponent/no-timer basket (basket `bFreePlay`: skips the Duration timer in `OnMinigameStarted`, never
  finishes on score) so tuning needs only one PIE process. UNVERIFIED — user compiles next.
- **2026-06-12** — Session close-out. UMG basket polish: `WBP_BasketGame` built by user (works);
  `UPPBasketWidgetLib::SetCanvasPos`/`RadToDeg` helpers; `BP_PeachBasketUMG` auto-preferred by GameMode
  for live tuning; exposed `GroundY`/`HoopLeftPos`/`HoopRightPos`/`CharStartPositions[4]`/`BallStartPos`
  + feel (`JumpImpulse` 0.6, `SlideFriction`, `AirDrag`, `MaxLean` 0.4). `bDebugSkipToBasket=True`.
  Build.cs += UMG/Slate/SlateCore. Also this session: team-on-join + per-team-ready gate, placeable
  TeamA/B PlayerStarts, station reverted to placeholder cubes, `bSpawnPlaceholderHub=false`, fixed
  half-in-floor + seated fade-to-black, `JoinByIP` (LAN discovery is firewall-blocked → use it).
- **2026-06-12** — UMG Peach Basket (minimal-BP minigame): new `Minigame/PPPeachBasketUMGGame` runs the
  whole game server-side in normalised 2D (no PhysX/3D placement) and replicates `FPPBasketState`
  (`PPBasketUMGTypes.h`): pendulum-lean jump (jump along the lean = sideways move), arms-up grab/steal
  (always active), auto-aim throw scaled by arm height, walls, score, 5-to-win / 120s. `GetState()` for
  the widget. `APPPlayerController::PlayerTick` auto-spawns the widget at
  `/Game/PeachParty/UI/WBP_BasketGame` while viewing such a match (`GetViewedMinigame()`). GameMode round
  0 now spawns this UMG version (revert `BasketGameClass` to `APPPeachBasketGame` for the old 3D one).
  USER builds `WBP_BasketGame` and binds images to GetState(). UNVERIFIED — compile errors next.
- **2026-06-12** — Teams assigned on JOIN now (`PickJoinTeam` = smaller team, in `PostLogin` before the
  pawn spawns) so the lobby spawn uses the right team's start; old alternate-`AssignTeams` removed
  (`AssignTeams` now only fills teamless players). `AreAllPlayersReady` additionally requires ≥1 ready
  player on EACH team (1v1 needs both sides). Diagnostic log kept in `NotifyReadyStateChanged`.
- **2026-06-12** — User takes over level building: reverted PC station to placeholder desk+screen cubes
  (removed the PP_PC_Station path-load/100×/BeginPlay-load; `StationMesh` is now an empty optional slot,
  `bHidePlaceholderBlocks` default false). `bSpawnPlaceholderHub` default **false** (no runtime hub).
  Added two ready-to-place spawn classes: **`APPTeamAPlayerStart`** (blue) and **`APPTeamBPlayerStart`**
  (red), subclasses of `APPTeamPlayerStart` with Team preset — drop from Place Actors; GameMode spawns
  each player at a matching one.
- **2026-06-12** — Station/seat/collision pass: (1) `StationMesh` scaled 100× (imported model was tiny).
  (2) Fixed "players half in floor" — `BodyMesh` cylinder pivot is its centre, so origin at -88 sank it
  88u below the capsule; moved to Z=0 (spans the capsule, base on floor). (3) PC-station `MinigameCamera`
  back to **Perspective** (only minigames use ortho). (4) Sitting at a PC now fades to black and HOLDS
  (`PlayTransitionFade(bHold)`); fades back in when the minigame starts / you stand up.
- **2026-06-12** — PC station uses the user's real model: `APPPCStation` loads
  `/Game/PeachParty/Interactables/PP_PC_Station` into a new `StationMesh` component by path;
  `bHidePlaceholderBlocks` now defaults true (desk/screen cubes hidden, via `OnConstruction`). Per-instance
  override still possible in Details. (Asset lives in the user's Content — must be pushed to the repo.)
- **2026-06-12** — Placeable level actors (user builds the level by hand instead of runtime-spawn):
  new `Core/PPTeamPlayerStart` (APlayerStart + `Team` enum, editor arrow tinted blue/red). `APPGameMode`
  `ChooseTeamStart` spawns each player at a placed start matching their team (falls back to plain
  PlayerStarts → spread spawn). `APPPCStation` was already a placeable Actor — drop it from Place Actors.
  `bSpawnPlaceholderHub` is now `config` (DefaultGame.ini) — uncomment `bSpawnPlaceholderHub=False` once
  the user's own stations/spawns are placed so the runtime hub doesn't spawn on top.
- **2026-06-11** — Added `UPPGameInstance::JoinByIP(FString)` (BlueprintCallable) — direct ClientTravel
  to a host IP, bypassing the flaky NULL LAN beacon (firewall-blocked across machines). Reliable demo
  path: host hosts, joiner types the host's IP (or 127.0.0.1 for local 2-process) → connect.
- **2026-06-11** — Menu flow bugfixes (host→stuck on menu, join→black, host-twice→black): (1) viewport
  widgets survive level travel, so `APPCharacter::PawnClientRestart` now `RemoveAllViewportWidgets()` on
  lobby entry (clears leftover menu UI). (2) `UPPGameInstance::HostGame` now destroys a stale session
  and chains `CreateSession` onto destroy-complete (`HandleDestroyForHost`/`CreateSessionNow`) — fixes
  the NULL "already in session" race on repeated hosting; also broadcasts `OnCreateComplete(false)` on
  early-out. (3) `LobbyMapName` → full path `/Game/PeachParty/Maps/PeachPartyHub` (short name was
  ambiguous). (4) Cleaned stray map lines (wrong path) out of `DefaultEngine.ini [OnlineSubsystem]`.
- **2026-06-11** — Menu/lobby input: new `Menu/PPMenuGameMode` (no pawn, no hub, plain PC) for the
  MainMenu map — user must set it as the MainMenu level's **GameMode Override** (World Settings). And
  `APPCharacter::PawnClientRestart` forces Game-Only input + hides the cursor on the owning client;
  since the menu has no APPCharacter, this only fires in lobby/game = clean undo of the menu's UI mode.
- **2026-06-11** — Basket sizing pass: camera 2× closer (`OrthoWidth` 1400→700), players +20% (new
  `APPBasketCharacter::SpriteScale`=1.2, shoulder pivot scales with it), hoops +20% (`HoopScale`
  1.3→1.56), background bleed 1.05→1.0. SpriteScale is visual-only (HandPoint/capsule reach unchanged).
- **2026-06-11** — Full-screen for real: the basket render was letterboxed (camera in a centred
  rectangle, HUD text full-width). Fix: `SetConstraintAspectRatio(false)` on both ortho cameras
  (GameCamera + station MinigameCamera) so the view fills the whole viewport. Background fit is now
  LIVE (`ScaleBackgroundToView()` each Tick, reads the camera's real `OrthoWidth`) so it self-corrects
  on any resolution/resize. Also `bSpawnPlaceholderLight` default → false (kills the "Multiple
  directional lights competing" warning; Paper2D sprites are unlit). If gray borders REMAIN after this,
  it's a windowed-PIE thing → play fullscreen / set the PIE window to the monitor resolution.
- **2026-06-11** — Background now GUARANTEED full-screen: under the orthographic camera the view shows
  exactly `OrthoWidth` world-units wide, so `FitBackgroundToScreen()` measures the sprite's real world
  size (`CalcLocalBounds`, independent of pixels-per-unit) and cover-fits it. `BackgroundScale` is now a
  bleed margin (1.05), not a guess. Also added GameMode `bDebugSkipToBasket` (EditDefaultsOnly, Debug):
  PIE polls for ≥2 players then jumps straight into the Peach Basket round — skips lobby/ready for fast
  visual iteration. `TryDebugAutoStart` + `DebugStartTimer`.
- **2026-06-11** — Basket visual tuning: background +10% (`BackgroundScale` 2.4→2.64); hoops +30%
  (`APPBasket::HoopScale`) and pushed back in depth VISUAL-only (`VisualDepthOffsetY`, Mouth/scoring
  unaffected). **Arm rig reworked**: arm textures are full-canvas layers, so at rest the arm sprite is
  offset −`ShoulderZ` to overlay the body exactly (fixes "arms not where they belong"); arms now scale
  1:1 with the body (dropped the 1.3 Z stretch); raise angle is a tunable `ArmRaisedDeg` (75→155, "at
  least double"). `ShoulderZ` is purely the rotation pivot now.
- **2026-06-11** — Cameras: minigame `GameCamera` and PC-station `MinigameCamera` are now
  **orthographic** (flat 2D look); the FP hub camera stays perspective, so projection auto-switches with
  the view target — no runtime toggling. Zoom knobs: `OrthoWidth` (base, 1800; basket override 1400) and
  `SeatedOrthoWidth` (station, 300). Tune by eye.
- **2026-06-11** — Basket layout pass (from in-game screenshot, first-guess values to iterate):
  flipped ALL players (inverted `FlipX` in `ApplySprites`), hoops pulled in + down
  (`±440/Z250 → ±330/Z150` in `SpawnPlay`), arm shoulder pivot made explicit via new tunables
  `ShoulderZ`/`ArmDropZ` (arm sprite now hangs straight down from the shoulder joint). NOTE: the
  PIE screenshot also showed *"Multiple directional lights are competing… adjust ForwardShadingPriority"*
  → the placeholder light competes with the real level's light; set `bSpawnPlaceholderLight=false`
  (GameMode) now that a lit level exists.
- **2026-06-11** — Build fix: `PPGameInstance.cpp` `C2065 'SEARCH_PRESENCE': undeclared identifier`.
  Adding `Online/OnlineSessionNames.h` did NOT resolve it on UE 5.7 (constant moved/renamed again).
  Final fix: **removed the `SEARCH_PRESENCE` query block entirely** — it was dead code (project runs on
  `OnlineSubsystem Null` + LAN where presence is unused), so we no longer depend on the moving constant.
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
- **2026-06-11** — Transitions + reward system. Camera fade-to-black-and-back on phase/camera changes
  (`PlayTransitionFade`, no widget). 4 team rewards (`Final/PPRewardTypes.h`, +10% speed/ammo/wetness/
  capacity) picked in the Reward phase by the winning team(s) (`ServerSelectReward`, gated), applied via
  `Character::GetEffectiveStats` + the wetness slip threshold. Per-minigame result screen
  (`LastRoundWinner` + `RoundResultSeconds` pause + `BP_OnRoundResult`). UMG widgets (ready count /
  score / result / reward menu) must be built in-editor and bound to the replicated GameState data.
- **2026-06-11** — Final phase implemented (backbone): class system (4 classes, replicated, respawn-
  gated switching), wetness/slip + respawn, frontline 3-room objective progression (`APPObjectiveRoom`
  + GameMode front-shift + per-room timer reset), water-gun projectile combat (team-aware wetness).
  New `Final/` folder. Object/gravity-gun + refill stations designed but not yet built.
- **2026-06-11** — Combat spec match: gravity-object grab/throw (`APPGrabbableObject`, RMB grab/drop,
  LMB throw, impact-speed→wetness + knockback, friendly-safe), proximity refill stations
  (`APPRefillStation`, Runner refills faster, no manual reload), fuller slip (DisableMovement +
  DisableInput + random launch impulse). Character: `HoldPoint`, replicated `bIsHolding` (blocks fire),
  `AddAmmo` (clamped to class capacity), `OnGrab/ServerGrabOrDrop/ServerThrow`. Water projectile now
  team-coloured (`InstigatorTeam` replicated → `OnRep_Team` tint) with a `MulticastImpact`/`BP_OnImpact`
  team-coloured splash hook. `Grab` (RMB) added to `DefaultInput.ini`. Place `APPRefillStation` and
  `APPGrabbableObject` actors in the final arena level. Unverified — no local compiler.
- **2026-06-11** — Basket arena: hid the floor/walls (collision-only, `SetVisibility(false)`) so the
  visible "box" is gone; added a full-screen `Background` Paper2D sprite (from `Background.uasset`)
  behind the action (`BackgroundOffset`/`BackgroundScale` tunable). Aiming for the reference court look.
- **2026-06-11** — User committed their basket textures to the repo at
  `Content/PeachParty/Minigames/BasketPeach/Graphics/` (Texture2D `.uasset`, incl. the `PLayer01_Arm`
  typo). Switched to a **git-clone workflow** (Windows, HTTPS — SSH key only existed on the Linux box).
  Code now references those TEXTURES by exact path and builds Paper2D sprites from them at runtime
  (`PPVisual::SpriteFromTexture`, **editor-only** `InitializeSprite` — fine for PIE, needs real sprite
  assets for a packaged build). No hand-created sprites needed from the user.
- **2026-06-11** — Session handoff: rewrote §1, §2, §6b, §8, §9 to reflect actual state (Final +
  Reward + Menu are built, no longer "TODO"); added §9b file map and §10b next-session checklist.
  Last verified compile predates the combat round (grab/throw, refill, team-coloured water/splash,
  fuller slip) — user will paste compile errors in the next session.
