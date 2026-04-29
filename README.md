# Steam Haptics Singer

This project is a fork of [Roboron3042's SteamControllerSinger](https://github.com/Roboron3042/SteamControllerSinger) which is a fork of [Pila's SteamControllerSinger](https://gitlab.com/Pilatomic/SteamControllerSinger). It attempts to add Deck compatibility but is, at the moment, not that good.

I gotta say, I'm no expert and barely know how this thing works, but I hope to actually make some improvements if possible :).

## How To

1. Turn on your Steam Controller or Steam Deck
2. Drag the midi file onto steam-haptics-singer executable
3. When prompted, press ENTER
4. Enjoy!

### Where can I find midi songs?

Songs ready to play can be found in the original guy's [personal collection](https://mega.nz/#F!BWpEWKzB!r7WPw5bZ_domN4pk-FJsjg) (as he called it). Otherwise, you can just try a MIDI and see what happens (most of the time it won't work well).

### Usage from command prompt:
	Usage: steam-haptics-singer [-p] [-y] [-d DEBUG_LEVEL] [-i INTERVAL] MIDI_FILE

	  -i INTERVAL		Player sleep interval (in microseconds). Lower generally means better song fidelity, but higher cpu usage, and at some point going lower won't improve any more. Default value is 10000
	  -d DEBUG_LEVEL	Libusb debug level. Default is 0, no debug output. max is 4, max verbosity output
	  -p	Repeat song, plays again after ending
	  -y	Legacy playback, forces usage of the old Steam Controller haptic instruction instead of the new one (causes issues)

### Midi files tips:

Midi files may need to be edited with a software such as [MidiEditor](https://www.midieditor.org/) to be correctly played with Steam Haptics Singer following the next tips:

* Notes from midi channel 0 are played on right haptic
* Notes from midi channel 1 are played on left haptic
* Notes from others channels are ignored
* **Avoid multiple notes active at the same time on the same channel**, since haptic actuators can only play one note at the time.


## Compiling

You will need libusb(-dev) and pkgconf. If you have them, just type `make`.

It's recommended to build this in a container such as [steam-runtime](https://github.com/ValveSoftware/steam-runtime?tab=readme-ov-file#building-in-the-runtime) or [holo-docker](https://github.com/SteamDeckHomebrew/holo-docker) in order for the packges to line up with the Steam Deck.

If you go the steam-runtime route, make sure to use sniper as scout is outdated.

### For a guide:
	git clone -b master https://github.com/CrazyCritic89/SteamHapticsSinger.git
	cd SteamHapticsSinger
	podman run --rm -v ./:/src -it registry.gitlab.steamos.cloud/steamrt/sniper/sdk bash
	cd src
	make
	exit


## CHANGELOG

[My v1.9 Build :> (EXCLUSIVE)]
* Badly added Deck support. No improvements.

[v1.8]
* User can now define the reclaim period with -c option.

[v1.7]
* Fixed music stopped playing after a few seconds

[v1.6]
* Fixed major bugs in playback algorithm

[v1.5]
* Changed debug level argument from -d to -l
* Added -r argument to enable demo mode
* Enhanced arguments parsing
* Does not rely on Steam Controller duration anymore
* Updated note display
* Now stops playing when interrupting the process ( on Ctrl+C )

[v1.4]
* Fixed a bug in MIDI librairie that would compute a null duration for notes when ON event and previous OFF event had the same timetick

[v1.3]
* Added -iINTERVAL argument
* Added -dDEBUG_LEVEL argument 

[v1.2]
* Fixed being stuck on "Command error" when disconnecting controller while playing. Now continue playing (even if keep failing)
* Removed the now deprecated 20ms note duration reduction
