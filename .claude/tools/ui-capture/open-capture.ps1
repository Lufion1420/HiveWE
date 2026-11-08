# NOTE: -Retry re-fires the action once if no window appeared. Use ONLY for idempotent actions (a retry once created a duplicate variable in Trigger Editor).
# Usage: powershell -File open-capture.ps1 -ProcId N -Name "Trigger Editor" -Type Button [-Tab Home] [-Out trigger-editor] [-Wait 8] [-KeepOpen] [-Nth 0] [-Mode invoke|select|toggle]
# Selects optional ribbon tab, fires the UIA action in a DETACHED process (modal dialogs block Invoke), waits for new top-level windows of the process,
# PrintWindow-captures each to captures\<Out>__<title>.png, then WM_CLOSE's them unless -KeepOpen. Never sends keyboard/mouse input.
param([int]$ProcId,[string]$Name,[string]$Type,[string]$Tab,[string]$Out,[int]$Wait=8,[switch]$KeepOpen,[int]$Nth=0,[string]$Mode='invoke',[switch]$Retry)
$s = Split-Path -Parent $MyInvocation.MyCommand.Path
. "$s\capture-lib.ps1"
Add-Type -Namespace N -Name M -MemberDefinition '[DllImport("user32.dll")] public static extern bool PostMessage(System.IntPtr h,uint m,System.IntPtr w,System.IntPtr l); [DllImport("user32.dll")] public static extern bool IsWindow(System.IntPtr h);'
function Snap { Get-MyWindows $ProcId | ? { $_.Title -notmatch '\.exe$' } }
if(-not $Out){$Out=($Name -replace '[^\w]+','-').ToLower()}
$null=New-Item -ItemType Directory -Force "$s\captures"
if($Tab){ & powershell -NoProfile -File "$s\ui-act.ps1" -ProcId $ProcId -Action select -Name $Tab -Type TabItem | Out-Null; Start-Sleep -Milliseconds 500 }
$before = @(Snap | % Hwnd)
$sw=[Diagnostics.Stopwatch]::StartNew()
$args2=@('-NoProfile','-File',"$s\ui-act.ps1",'-ProcId',$ProcId,'-Action',$Mode,'-Name',"`"$Name`"",'-Nth',$Nth)
if($Type){$args2+=@('-Type',$Type)}
$act=Start-Process powershell -ArgumentList $args2 -WindowStyle Hidden -PassThru
$seen=@{}
while($sw.Elapsed.TotalSeconds -lt $Wait){
  Start-Sleep -Milliseconds 700
  foreach($w in Snap){ if($before -notcontains $w.Hwnd -and -not $seen[$w.Hwnd]){ Start-Sleep -Milliseconds 1200
     $seen[$w.Hwnd]=$true; $t=($w.Title -replace '[^\w ]','').Trim() -replace ' +','-'
     Cap-Window $w.Hwnd "$s\captures\$Out`__$t.png" } }
  if($seen.Count -gt 0 -and $sw.Elapsed.TotalSeconds -gt 3){break}
}
if($seen.Count -eq 0 -and $Retry -and -not $script:retried){ $script:retried=$true; "no window yet, retrying once"; $act2=Start-Process powershell -ArgumentList $args2 -WindowStyle Hidden -PassThru; $sw.Restart(); $before=@(Snap | % Hwnd); while($sw.Elapsed.TotalSeconds -lt $Wait -and $seen.Count -eq 0){ Start-Sleep -Milliseconds 700; foreach($w in Snap){ if($before -notcontains $w.Hwnd -and -not $seen[$w.Hwnd]){ Start-Sleep -Milliseconds 1200; $seen[$w.Hwnd]=$true; $t=($w.Title -replace '[^\w ]','').Trim() -replace ' +','-'; Cap-Window $w.Hwnd "$s\captures\$Out`__$t.png" } } } }
if($seen.Count -eq 0){ "NO NEW WINDOW after $Wait s (action exited=$($act.HasExited))"
  $m=(Get-MyWindows $ProcId | ? Title -like 'HiveWE*' | select -First 1).Hwnd; Cap-Window $m "$s\captures\$Out`__mainwindow.png" }
if(-not $KeepOpen){ foreach($h in $seen.Keys){ [N.M]::PostMessage([IntPtr][int64]$h,0x0010,[IntPtr]::Zero,[IntPtr]::Zero)|Out-Null }; Start-Sleep 1 }
"elapsed {0:n1}s; windows still open: {1}" -f $sw.Elapsed.TotalSeconds,((Snap|%{$_.Title}) -join ' | ')



