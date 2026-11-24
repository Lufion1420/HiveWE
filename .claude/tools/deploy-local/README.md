# deploy-local

`deploy.ps1 [-Target <folder>]` replaces a local HiveWE install with the current `build/Agent/Release` output. It never builds.

Steps: refuse if a HiveWE.exe runs from the target; stage like `.github/workflows/release.yml` (no data symlink, tests exe, lib/exp/pdb/ilk; real `data/` copy); smoke-test the staged copy with `HiveWE.exe --resave` on a scratch copy of `data/test map` (needs exit 0 and "Saving took"); refuse if the target holds anything that is not a package item; delete the target's contents, copy the package in, repeat the smoke test from the target; remove scratch files.

Default target: `D:\Stuff\Warcraft_3_All_Assets\Tools\Warcraft 3 - Tools\HiveWE`. The orchestrator checks `Get-Process HiveWE` itself and runs it after every QA-PASS commit (standing order 2026-11-24), skipping with one line when HiveWE runs; it never asks the user to close HiveWE. The script never kills processes.
