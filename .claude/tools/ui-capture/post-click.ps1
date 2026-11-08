# Usage: powershell -File post-click.ps1 -ProcId N -Window "Trigger Editor" -Name "Melee Initialization" [-Type TreeItem] [-Double] [-Right] [-Nth 0]
# Finds the element by UIA, then POSTS WM_LBUTTON* messages to the top-level HWND (client coords).
# No SendInput, no real cursor move, no focus needed. Only ever targets the given process's own window.
param([int]$ProcId,[string]$Window,[string]$Name,[string]$Type,[switch]$Double,[switch]$Right,[int]$Nth=0)
Add-Type -AssemblyName UIAutomationClient
Add-Type -Namespace P -Name U -MemberDefinition @"
[DllImport("user32.dll")] public static extern bool PostMessage(IntPtr h,uint m,IntPtr w,IntPtr l);
[DllImport("user32.dll")] public static extern bool ScreenToClient(IntPtr h,ref POINT p);
[StructLayout(LayoutKind.Sequential)] public struct POINT{public int X,Y;}
"@
$AE=[Windows.Automation.AutomationElement]
$c=New-Object Windows.Automation.PropertyCondition($AE::ProcessIdProperty,$ProcId)
$top=$AE::RootElement.FindAll('Children',$c) | ? { $_.Current.Name -eq $Window } | select -First 1
$hits=@($top.FindAll('Descendants',[Windows.Automation.Condition]::TrueCondition) | ? { ($_.Current.Name -replace "`n",' ') -eq $Name -and (-not $Type -or $_.Current.ControlType.ProgrammaticName -like "*$Type") })
if($hits.Count -le $Nth){ "NOTFOUND"; exit 2 }
$e=$hits[$Nth]; $r=$e.Current.BoundingRectangle
$hwnd=[IntPtr]$top.Current.NativeWindowHandle
$pt=New-Object P.U+POINT; $pt.X=[int]($r.X+$r.Width/2); $pt.Y=[int]($r.Y+$r.Height/2)
[P.U]::ScreenToClient($hwnd,[ref]$pt)|Out-Null
$l=[IntPtr](($pt.Y -shl 16) -bor ($pt.X -band 0xFFFF))
$dn=if($Right){0x204}else{0x201}; $up=if($Right){0x205}else{0x202}; $dbl=if($Right){0x206}else{0x203}; $mk=if($Right){2}else{1}
[P.U]::PostMessage($hwnd,0x200,[IntPtr]0,$l)|Out-Null
[P.U]::PostMessage($hwnd,$dn,[IntPtr]$mk,$l)|Out-Null; [P.U]::PostMessage($hwnd,$up,[IntPtr]0,$l)|Out-Null
if($Double){ [P.U]::PostMessage($hwnd,$dbl,[IntPtr]$mk,$l)|Out-Null; [P.U]::PostMessage($hwnd,$up,[IntPtr]0,$l)|Out-Null }
"posted to hwnd $($hwnd.ToInt64()) at client $($pt.X),$($pt.Y)"
