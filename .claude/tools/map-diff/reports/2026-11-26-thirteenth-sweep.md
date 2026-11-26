# Thirteenth sweep, 2026-11-26: unknown object-data fields kept (tree on 82fb258)
Old-vs-new --resave bytes. Old: 9af0421 exe (sha 304F181F4E78) with HEAD data/overrides; new: sha B7038C93DD7C. 237-map stock subset + 7 user/test copies (NCOW Arena, NCOW Main, test map, 4 other).
Exits equal old/new on all 244: subset 49 ok, 188 script_compile_error (as eleventh); user maps 7 ok. 14343 files compared.
239 identical (incl. all 7 user maps). 5 differ, war3map.w3u only; new minus added records == old byte for byte:
- stock_0154: n006 upor "units\demon\Pitlord\Pitlord_Portrait.mdl"
- stock_0180: n003 upor "units\critters\VillagerKid\VillagerKid_Portrait.mdl"
- stock_0239: Ugrk upor "...\heroforsakengarekupgrade_portrait.mdl", u000 upor "buildings\undead\SacrificialPit\SacrificialPit.mdl"
- stock_0240: hgry uatg int 0, spgr uatg int 0
- stock_0465: UC13 upor "units\demon\ChaosWarlord\ChaosWarlord_portrait.mdl"

All 7: input in war3mapSkin.w3u, same type and value, dropped by old, kept by new in war3map.w3u (existing rule for known skin fields; not game-tested).
Parser check: 76058 input record keys, none kept by old and lost by new, 0 value differences. Keys absent from both (mostly greq, udty, uabi) not examined.
A first run was invalid and discarded: the old sandbox had the new SLKs, so all 244 matched.
