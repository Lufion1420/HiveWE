# Deploys the current build/Agent/Release output to a local HiveWE tools folder.
# Mirrors the packaging in .github/workflows/release.yml. Never builds anything.
param(
	[string]$Target = 'D:\Stuff\Warcraft_3_All_Assets\Tools\Warcraft 3 - Tools\HiveWE',
	[string]$Repo = (Resolve-Path (Join-Path $PSScriptRoot '..\..\..')).Path,
	[string]$Scratch = (Join-Path $env:TEMP 'hivewe-deploy')
)
$ErrorActionPreference = 'Stop'
$build = Join-Path $Repo 'build\Agent\Release'
$stage = Join-Path $Scratch 'stage'
$testMap = Join-Path $Repo 'data\test map'

function Test-Resave($exeDir, $label) {
	$run = Join-Path $Scratch "resave-$label"
	if (Test-Path $run) { Remove-Item $run -Recurse -Force }
	New-Item -ItemType Directory -Force $run | Out-Null
	Copy-Item $testMap (Join-Path $run 'in') -Recurse
	$out = Join-Path $run 'out'
	$log = Join-Path $run 'stdout.txt'
	$err = Join-Path $run 'stderr.txt'
	$p = Start-Process (Join-Path $exeDir 'HiveWE.exe') -ArgumentList '--resave', "`"$run\in`"", "`"$out`"" `
		-WorkingDirectory $exeDir -Wait -PassThru -NoNewWindow -RedirectStandardOutput $log -RedirectStandardError $err
	$text = (Get-Content $log, $err -ErrorAction SilentlyContinue) -join "`n"
	Write-Host "[$label] resave exit $($p.ExitCode)"
	if ($p.ExitCode -ne 0 -or $text -notmatch 'Saving took') { throw "[$label] resave smoke test failed (exit $($p.ExitCode)): $text" }
	Write-Host "[$label] 'Saving took' found"
}

# Refuse if a HiveWE runs from the target.
$targetFull = [IO.Path]::GetFullPath($Target).TrimEnd('\')
$running = Get-Process HiveWE -ErrorAction SilentlyContinue | Where-Object { $_.Path -and $_.Path.StartsWith($targetFull + '\', 'OrdinalIgnoreCase') }
if ($running) { throw "HiveWE is running from $targetFull (PID $($running.Id)). Close it yourself; this script never kills it." }
if (-not (Test-Path (Join-Path $build 'HiveWE.exe'))) { throw "No build at $build" }

# Stage (release.yml: skip data symlink, tests exe, lib/exp/pdb/ilk; real copy of data/).
if (Test-Path $Scratch) { Remove-Item $Scratch -Recurse -Force }
New-Item -ItemType Directory -Force $stage | Out-Null
$exclude = '*.lib', '*.exp', '*.pdb', '*.ilk', 'HiveWE_tests.exe'
Get-ChildItem $build -Force | Where-Object { $_.Name -ne 'data' } | ForEach-Object {
	if ($_.PSIsContainer) { Copy-Item $_.FullName $stage -Recurse -Force }
	else {
		$name = $_.Name
		if (-not ($exclude | Where-Object { $name -like $_ })) { Copy-Item $_.FullName $stage -Force }
	}
}
Copy-Item (Join-Path $Repo 'data') (Join-Path $stage 'data') -Recurse -Force
Test-Resave $stage 'staged'

# Target must hold only a HiveWE package (known names) before anything is deleted.
if (Test-Path $targetFull) {
	$known = (Get-ChildItem $stage -Force).Name + 'HiveWE.exe'
	$foreign = Get-ChildItem $targetFull -Force | Where-Object { $known -notcontains $_.Name }
	if ($foreign) { throw "Target holds non-package items, not deleting: $($foreign.Name -join ', ')" }
	Get-ChildItem $targetFull -Force | Remove-Item -Recurse -Force
} else { New-Item -ItemType Directory -Force $targetFull | Out-Null }
Copy-Item (Join-Path $stage '*') $targetFull -Recurse -Force
Test-Resave $targetFull 'target'

Remove-Item $Scratch -Recurse -Force
$exe = Get-Item (Join-Path $targetFull 'HiveWE.exe')
Write-Host "Deployed: $($exe.FullName) $($exe.Length) bytes, $($exe.LastWriteTime)"
