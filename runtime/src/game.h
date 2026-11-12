// Which game this runtime is built for. The runtime started as the WET recomp
// and was carried over to Conan, then Fight Night Champion; each game's hooks
// (guest functions overridden by address, its globals) are compiled only for
// that game: an override at another game's address would replace an unrelated
// function.
#pragma once
#define GAME_WET 0
#define GAME_CONAN 0
#define GAME_FNC 1
