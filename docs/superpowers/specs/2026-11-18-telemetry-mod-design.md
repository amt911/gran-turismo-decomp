# Telemetry mod — design

**Status:** approved by the user on 2026-11-18 ("me parece bien, continua"). User request 2026-11-18 ("Me interesan todos"; idea: on-screen telemetry: speed per wheel, slip, G forces, a simple graph). Decisions *(Claude)*.

## Goals

- A toggleable HUD panel (cheat device row, plus a hotkey):
  - speed, G forces (lateral and longitudinal, from the velocity change per step in the body frame);
  - throttle and brake bars;
  - a 5-second graph of speed and G;
  - per-wheel slip and wheel speed once the wheel record is decoded (research step 1).
- Drawn with the mod draw API (`2026-11-18-mod-draw-api-design.md`): `on_overlay`, `draw_text`, `draw_rect`, `draw_line`.
- Zero effect on the game: read-only state, frames byte-identical with the panel hidden.

## Non-goals

- Logging to files.
- AI cars.
- Lap-time analysis (a later mod).

## Approach *(Claude)*

- A world-step hook samples `car_state` into a ring buffer: 300 steps, about 5 s.
- The overlay callback draws from a snapshot copied under a lock.
- G = Δv / dt / 9.81, smoothed over 4 steps.
- New SDK: `car_wheels(car, out[4])` (named) once the 0x38-byte wheel record layout is known: ground height, surface, rotation speed, slip.

## Step 0 — research

1. A probe mod dumping the four wheel records for 300 steps on the circuit route, to correlate with speed, steering and braking. Wheel speed and slip fields, written to FACTS.

## Testing

- **Unit:** G computation, ring buffer, graph scaling.
- **In game:**
  - with the panel shown, the overlay has text where expected and the speed digits change with speed;
  - hidden: frames byte-identical to no mod.
- **Sabotage:** a constant G must fail the "G follows braking" check on the braking route.
