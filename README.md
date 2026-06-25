# Morse Flipper

Morse Flipper is a CW trainer, keyer, hardware adapter and small emergency Morse transmitter for the Flipper Zero.

It is built around one opinion: do not learn Morse by staring at dots and dashes. Learn the sound. Hear the character, type the character, and keep the counting part of your brain out of it.

It can be a toy, a contest trainer, a portable-radio helper, or a small piece of prepper kit. CW has always been oddly good at surviving bad conditions.

This started as a 2024 experiment, was abandoned for a while, then dragged into launch shape with LLM-assisted cleanup in 2026. The bulk of the design and the important decisions are still human-made and human-reviewed: timing, keying, GPIO behaviour, RF compromises, tests, and the stubborn refusal to make CW look like a barcode for the ears.

## What it does

- LCWO-style copy practice, with the answer hidden until you type it.
- Straight-key timing practice for learning a cleaner fist.
- Five-character sending drills for spacing and rhythm.
- Free practice with straight key, paddle, Flipper buttons, USB, MIDI, mouse or keyboard input.
- Straight keys and paddles through a simple 6.35/6.5 mm jack adapter.
- Iambic and oddball keying modes: keyahead, Elekey-A, Elekey-B, Ultimatic and bug.
- Flipper buttons as either a straight key or a usable paddle. Paddle mode uses OK and Back so your fingers are not forced into a silly little claw.
- A field keyer/logger for POTA/SOTA-style portable operating. Send canned replies like `UR 5NN HW?` or `P2P RO 0038`; if you send manually with paddles, it sends and logs the text at the same time.
- Ham-rig keying on GPIO, with `P15` as key and `P16` as PTT in Ham Keyer mode.
- USB adapter modes for Vail, V-Band and similar practice tools: MIDI, mouse and keyboard.
- Vail-style MIDI control, so the browser can talk back to the Flipper for speed, tone and keyer mode.
- Buzzer, vibration and cleaner P2/A7 PWM audio output for discreet or external sidetone.
- Sub-GHz Morse TX for experiments and last-resort signalling, where legal and sensible.
- Extended help inside the app, because CW has enough folklore without making users hunt for the manual.

It also saves its settings, supports custom training character files on the SD card, warns about suspicious GPIO shorts at startup, and falls back sensibly when a straight key is plugged into the paddle jack.

## Why another CW app?

There are already Flipper Morse apps, but many of them teach the most common bad habit first: looking at dots and dashes. That is fine for a code table and rubbish for copying real CW.

Morse Flipper tries to train the useful reflex instead. Keep the character speed high, widen the gaps if needed, and let the sound become the letter. The app has a small LCWO-style trainer because LCWO is still the gold standard for this approach, but the Flipper version is useful when the laptop is not.

The other half of the project is hardware. A Flipper is already a pocket full of GPIO, USB and RF mischief; this app turns that into a CW adapter, a paddle interface, a portable keyer, and a way to reuse old keys or scrap hardware without building a whole dedicated box first.

## Hardware

### None required

Morse Flipper works out of the box with no extra hardware. Use the joystick as a straight key, switch to the built-in keyers when you want paddle-style timing, hear the sidetone on the internal buzzer, and use the Flipper radio for its built-in Morse TX path where that is legal and sensible. The adapters below make it nicer, sturdier, or more useful with real keys and rigs; they are comfort upgrades, not a hard requirement.

### Simple key and paddle jack

<p align="center"><img src="docs/images/howto-basic.webp" alt="Simple 6.5 mm jack adapter wiring" height="540"></p>

The simplest adapter is just a 6.35/6.5 mm female jack soldered to header pins. It plugs straight into the Flipper GPIO row and gives you a real key without a PCB.

Default wiring:

| Jack / key contact | Flipper GPIO |
| ------------------ | ------------ |
| Dit / straight key | `P7`         |
| Dah                | `P5`         |
| Ground             | `P3`         |

Why is ground `P3` and not `GND`/`P8`? Because the GPIO is assignable. Use whatever pins make sense for your key, and one pin can even pretend to be ground when that makes the hardware less silly. `P3` is the default because a 6.5 mm audio jack lands neatly on every second header pin. Since that ground is under software control, the app can do useful little tricks, such as spotting `P5` and `P3` shorted together when a straight key has been shoved into a paddle jack, then falling back accordingly. If you prefer a real ground, use `GND`; the app will cope.

A small GPIO board / Flipper add-on board is planned, but this ugly little jack adapter is the correct first build. It is cheap, obvious, and hard to debug incorrectly.

### Expanded keyer board

<p align="center"><img src="docs/images/howto-full.webp" alt="Expanded GPIO audio, PTT and keying board" height="540"></p>

The second board simply brings more Flipper GPIO out to sensible connectors: key input, sidetone audio, rig keying and PTT.

For audio, `P2/A7` carries the P2 PWM sidetone. It is generated on a 256kHz square carrier, with the tone modulated as PWM. Wire it through a series AC coupling capacitor to a 3.5 mm audio jack, tie left and right together, and use ground for the sleeve. The exact capacitor value is not encoded in the project yet; pick it for the input impedance you are driving and verify the level before plugging it into anything expensive. You can probably not filter it at all, since most speakers will do it anyway for you. If you hear a constant high pitched whine, your speaker does not filter it. Headphones are especially guilty of this.

For rig control, the keying outputs are simple low-side switches. A `BC817` NPN transistor shorts the rig signal to ground when the Flipper asserts the output: emitter to ground, collector to the rig key/PTT line, base driven from the Flipper GPIO through a resistor. Ham Keyer mode uses `P15` for key and `P16` for PTT. Check your rig first; if you do not know what the key/PTT line expects, add proper isolation rather than letting optimism be the smoke test.

## Learning CW

Small daily practice beats the grand weekly binge. Morse likes repetition, not theatre.

Start by copying by ear, not by diagram. If you catch yourself thinking `dash-dot-dash` and then deciding it is `K`, slow down the *gaps*, not the character. Farnsworth spacing exists for this reason. The letter should arrive as one sound.

Use the Flipper for quick sessions, pocket practice, button/key experiments and portable operating. Use LCWO for longer keyboard drills. Use Vail or V-Band when you want live humans to make things less tidy. Use a real key as soon as you can; the Flipper buttons work, but they are still rubber buttons on a toy dolphin.

## Build

Build it against 1.4.3 with fbt.

## Notes

- RF TX is experimental and jurisdiction-dependent. Know what you are transmitting, where, and why.
- The Flipper radio is not a replacement for a proper HF CW rig. It is a useful emergency and experimentation path, not magic.
- Ham-rig keying deserves boring electrical caution. Verify levels, polarity and isolation before connecting to equipment you like.
- The project is intentionally small enough to inspect, but not just a demo: there are host tests for the timing, keyers, training logic, GPIO behaviour, RF timing helpers and layout helpers, plus a real firmware build path.

Idea and prototype by Richard, YO3GND.
