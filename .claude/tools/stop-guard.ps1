# Stop hook: nudge the orchestrator once when it tries to end a turn.
# Allows the stop when: stop_hook_active (already nudged this stop), or HANDOFF.md was refreshed in the last 15 minutes (a real stop).
$raw = [Console]::In.ReadToEnd()
try { $in = $raw | ConvertFrom-Json } catch { exit 0 }
if ($in.stop_hook_active) { exit 0 }
$handoff = Join-Path $PSScriptRoot '..\HANDOFF.md'
if (Test-Path $handoff) {
  $age = (Get-Date) - (Get-Item $handoff).LastWriteTime
  if ($age.TotalMinutes -lt 15) { exit 0 }
}
$reason = 'Stop guard: do not end the turn on text alone while work remains. Take the next tool call now (next subagent, commit/push, or the next eligible Asana Next up ticket). Only stop if a subagent is still running and nothing else can be done (say so in one line), the user asked a question you just answered, or you hit a stop condition (no eligible ticket, spawn budget reached, user said stop); at a real stop refresh .claude/HANDOFF.md first.'
@{ decision = 'block'; reason = $reason } | ConvertTo-Json -Compress
exit 0
