# Peach Party

UE 5.7 · Listen-Server multiplayer · 2–8 players · modular party game.

Hub (school) → ready up at PCs → 2 short 2D minigames → team rewards → first-person final combat.

> **Status: CORE slice.** Lobby + Ready + Team assignment are fully implemented and networked.
> Minigame / Reward / Final phases are wired into the state machine as **stubs** (they set the
> phase, log, and run a timer) so the whole flow is exercisable today.

---

## Build

This repo is **source only** — it does not contain the engine.

1. Install Unreal Engine **5.7**.
2. Right-click `PeachParty.uproject` → *Generate Project Files* (or `GenerateProjectFiles`).
3. Build the `PeachPartyEditor` target (IDE or `Build.bat`).
4. Open in the editor. **Play → Net Mode: *Play As Listen Server*, Players: 2–8.**

Default GameMode is set in `Config/DefaultEngine.ini`, so any map boots straight into the flow.
Controls (legacy mappings in `Config/DefaultInput.ini`): **WASD** move, **mouse** look, **E** interact / sit / stand.

---

## Architecture

Single runtime module `PeachParty`. Authority lives on the listen-server host; clients are
view + input only. **Golden rule:** clients *request*, the server *decides*, the result *replicates*.

```
Source/PeachParty/
  Core/
    PPTypes.h            EMatchPhase, EPPTeam
    PPGameMode.*         SERVER-ONLY brain: phase state machine, ready-gate, team balancing
    PPGameState.*        Replicated match truth: phase, scores, countdown end-time
    PPPlayerState.*      Replicated per-player: team, ready, score
    PPPlayerController.* Interaction RPCs + local camera switch (sit/stand)
    PPCharacter.*        3D-hub pawn; client-side interaction trace
  Interaction/
    PPInteractable.h     Interface: client focus (cosmetic) + ServerInteract (authoritative)
    PPPCStation.*        The "PC" actor — ready-seat AND minigame-seat, with a screen camera
```

### Class responsibilities

| Class | Lives on | Responsibility |
|---|---|---|
| `APPGameMode` | **Server only** | Owns the match. Advances phases, detects all-ready, runs the start countdown, assigns teams, drives minigame/reward/final timers. Never trust clients here. |
| `APPGameState` | Server + all clients | The replicated source of truth clients read for HUD: current phase, minigame index, team scores, phase end-time. |
| `APPPlayerState` | Server + all clients | Per-player replicated data: `Team`, `bIsReady`, `MatchScore`. Survives pawn death. |
| `APPPlayerController` | Server + owning client | Sends interaction requests to the server; performs the **local** camera blend to/from a PC based on replicated `SeatedStation`. |
| `APPCharacter` | Server + clients | Pawn in the 3D hub. Owning client traces for interactables and forwards the hit to the controller. |
| `APPPCStation` | Server + all clients | The PC actor. Server-authoritative occupancy; in Lobby sitting = ready. Carries the camera that frames the 2D screen. |
| `IPPInteractable` | — | Contract for "walk up and press E". Focus is cosmetic/client; `ServerInteract` is authoritative. |

### Replication strategy (per system)

**Phase flow** — `APPGameState::CurrentPhase` (`ReplicatedUsing=OnRep_Phase`). Only the server's
GameMode writes it (via `SetPhase`, which also fires the OnRep path on the host). Clients react in
`OnRep_Phase → BP_OnPhaseChanged` to swap HUD / enable input.

**Ready system** —
1. Owning client traces (`APPCharacter::UpdateFocus`) and on **E** calls
   `APPPlayerController::ServerRequestInteract(Station)` (Server RPC).
2. Server validates distance + `CanInteract`, then `APPPCStation::ServerInteract` seats the player
   and (in Lobby) sets `APPPlayerState::bIsReady = true`, then calls `GameMode::NotifyReadyStateChanged`.
3. `bIsReady` replicates (`OnRep_Ready`) → every client updates the player's ready UI.
4. When `NumReadyPlayers == NumPlayers` (≥ `MinPlayersToStart`), the server arms a countdown
   (published as `PhaseEndServerTime`); if anyone un-readies first, it cancels. On elapse → teams + Minigame.

**Team assignment** — server-only `AssignTeams()` alternates A/B over `PlayerArray` (size-balanced,
deterministic). Writes `APPPlayerState::Team`; replication + `OnRep_Team` updates each client.

**Camera / seating** — `APPPlayerController::SeatedStation` replicates **`COND_OwnerOnly`** (camera is
local). `OnRep_SeatedStation` blends the view to the station's `UCameraComponent` (sit) or back to the
pawn (stand). The pawn never leaves the world → standing is an instant return to the 3D hub. Occupancy
that *other* players need to see is the separate, everyone-replicated `APPPCStation::OccupantPlayerState`.

**Countdowns** — never tick a replicated counter. The server publishes one float
`PhaseEndServerTime`; clients compute `remaining = PhaseEndServerTime - GetServerWorldTimeSeconds()`.

### The 2D-minigame model (your design, for when you build it out)

No level travel. The minigame is **sprites in the 3D world** on each PC's `ScreenMesh`; sitting just
blends the camera to `MinigameCamera`. Plan: an `AMinigameInstance` actor spawned by the GameMode
simulates authoritatively on the server and replicates entity transforms; seated clients send input
through the controller. Add `"Paper2D"` to `PeachParty.Build.cs` when you start it.

---

## Minimal Blueprint integration points

Pure C++ runs headless. These `BlueprintImplementableEvent`s are the **only** things you must hook up
for visuals/UI — override them in BP child classes:

- `APPGameState::BP_OnPhaseChanged(NewPhase)` — show phase banner / swap HUD.
- `APPPlayerState::BP_OnTeamChanged(NewTeam)` / `BP_OnReadyChanged(bReady)` — name-plate color, ready tick.
- `APPPCStation::BP_OnOccupantChanged(NewOccupant)` — screen on/off, sit animation, name plate.
- `APPPCStation::BP_OnFocusChanged(bFocused)` — interaction outline/prompt.

Assets to author (no code change needed):
1. **BP_PCStation** (child of `APPPCStation`): assign desk/screen static meshes, position `MinigameCamera`/`SeatPoint`.
2. **BP_PPCharacter**: skeletal mesh + anim BP.
3. A **HUD/UMG widget** that reads `APPGameState` (phase, countdown via `GetPhaseTimeRemaining`, scores)
   and the local `APPPlayerState` (team/ready).
4. Place several **BP_PCStation** actors in the hub map. Done.

---

## Minigame system (per-round 1v1 matchmaker)

The Minigame phase is **two rounds**: round 1 = **Peach Basket** (real-time physics), round 2 =
**Peach Artillery** (turn-based, health). **Opponents are reshuffled before each round** — you face a
different enemy in Basket than in Artillery. **Teams stay fixed** (assigned in Lobby) for the whole
match incl. the Final phase. **Each individual 1v1 win = +1 point for the winner's team.**

The 1v1 *match* is the unit (`APPMinigameBase`) — there is no multi-game container.

```
Source/PeachParty/Minigame/
  PPMinigameTypes.h        EMinigameType, EMatchResult
  PPMinigameBase.*         One 1v1 match: 2 players, camera, scores, Duration timer, result reporting
  PPPeachBasketGame.*      Score-based (base ForceResolve fits) — gameplay TODO marked
  PPPeachArtilleryGame.*   Health-based (overrides ForceResolve, adds ApplyDamage) — gameplay TODO
```

### Flow (the matchmaker lives in APPGameMode)

```
StartMinigamePhase
  └─ StartRound(0 = Basket):
        ├─ split players by FIXED team, SHUFFLE each team
        ├─ pair A[i] vs B[i]  -> SpawnMatch() each at a recycled arena slot (slot*ArenaSpacing apart)
        │     SpawnMatch: spawn match actor, bind both players, point their cameras at it, count it live
        └─ odd one out (bigger team) -> WaitingPlayer, idles at PC (may spectate)
  match ends -> NotifyMinigameFinished:
        ├─ winner team += 1   (draw scores nobody)
        ├─ release both players (camera back to PC; may spectate)
        ├─ destroy match, free arena slot, live-- 
        ├─ if WaitingPlayer set: pull the just-finished OPPOSITE-team player -> bonus match (both count)
        └─ when live == 0 and no one waiting -> OnRoundComplete
OnRoundComplete: round 0 -> StartRound(1 = Artillery, reshuffled);  round 1 -> FinishMinigamePhase
FinishMinigamePhase (or global timeout): team scores are already final -> StartRewardPhase
```

### Class responsibilities

| Class | Lives on | Responsibility |
|---|---|---|
| `APPGameMode` (matchmaker) | Server only | Shuffles + pairs each round, spawns/recycles arena slots, handles the odd-player bonus match, the global timeout, and scoring. The whole 1v1 orchestration lives here. |
| `APPMinigameBase` (`AActor`) | Server + all clients | One 1v1 match: its 2 players, both scores, the camera, a `Duration` timer; reports its result to the GameMode. Self-contained so spectators just aim a camera at it. |
| `APPPeachBasketGame` | " | Real-time scoring; winner = higher score (base rule). |
| `APPPeachArtilleryGame` | " | Health-based; overrides `ForceResolve` (higher HP wins), `ApplyDamage` auto-finishes on KO. |

### Replication strategy (minigames)

- **`APPGameState::ActiveMinigames`** — replicated `TArray<APPMinigameBase*>` of currently-live matches;
  the spectator camera + scoreboard read it. Server adds on spawn, removes on finish.
- **Each `APPMinigameBase`** replicates its 2 players, both scores and `Result` (`OnRep_Result` drives
  end-of-match UI). Mutated only on the server.
- **`APPPlayerState::CurrentMinigame`** — replicated; null when in lobby / waiting / done. Used to gate
  spectating ("you can't peek while still playing").
- **`APPMinigameBase` is `bAlwaysRelevant = true`.** Required: arenas are `ArenaSpacing` apart, so
  distance relevancy would cull a match for players in the hub — and then the replicated view-target
  pointer could never resolve. Cheap at ≤5 concurrent matches.
- **Camera/spectator** — one owner-only `APPPlayerController::ServerViewTarget`. The GameMode points it
  at a player's match on spawn and clears it on finish; finished/waiting players press ←/→ to cycle
  `ServerCycleSpectate`, which the server gates ("not mid-match") and aims at a live match. Camera-only
  ⇒ a spectator can never affect a live match.

### Safely running many matches in one world — and the tradeoffs

| Concern | Choice | Tradeoff |
|---|---|---|
| **Player isolation** | **Spatial separation** — arenas `ArenaSpacing` apart, slots recycled to keep coords bounded | Dead simple, impossible to cross-contaminate. Cost: a large world; for *hundreds* of arenas, switch to collision channels + net cull distance. |
| **Replication relevancy** | `bAlwaysRelevant` on matches | Everyone replicates every live match (fine for ≤5 / 8 players). At larger scale, per-arena `NetCullDistanceSquared` + move relevancy with the view target. |
| **Spectating** | Camera-only switch | Zero interference. No free-fly spectator pawn (not needed). |
| **Time limit** | One global timer for the whole phase + per-match `Duration`; `ForceResolve` from current state | Phase always ends, even if a match hangs. On timeout, bonus matches + round advance are suppressed; everything force-resolves then → Reward. |
| **Odd headcount** | Lone player gets a bonus match vs the first opposite-team finisher; **both results count** | Matches your spec; the double-playing opponent can earn 2 points (slight edge to the smaller team, but consistent). Extreme imbalance (>1 leftover, e.g. mid-match disconnects) leaves extras spectating with no match that round. |
| **Result tie** | Match draw scores neither team; round draw → both rewarded later | Keeps scoring a simple running count. |

### Blueprint integration points (minigame)

- `APPMinigameBase::BP_OnMinigameStarted` / `BP_OnMinigameFinished` — spawn visuals, results screen.
- Subclass `APPPeachBasketGame` / `APPPeachArtilleryGame` in BP to drop in meshes/VFX; the C++
  `OnMinigameStarted` hooks are where the actual playfield gets spawned (currently `TODO` stubs).
