# Usage: powershell -File ui-act.ps1 -ProcId N -Action invoke|select|toggle|list|expand -Name "Terrain" [-Nth 0] [-Type Button] [-Id substr]
# Finds element by exact Name (or -Like) among all top-level windows of the process; runs the UIA pattern. Run it as a separate process (Invoke can block on modal dialogs).
param([int]$ProcId,[string]$Action,[string]$Name,[int]$Nth=0,[string]$Type,[string]$IdLike,[string]$Like)
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
$AE=[Windows.Automation.AutomationElement]
$c=New-Object Windows.Automation.PropertyCondition($AE::ProcessIdProperty,$ProcId)
$tops=$AE::RootElement.FindAll([Windows.Automation.TreeScope]::Children,$c)
$hits=@()
foreach($t in $tops){
  foreach($e in $t.FindAll([Windows.Automation.TreeScope]::Descendants,[Windows.Automation.Condition]::TrueCondition)){
    $ci=$e.Current
    $nm=$ci.Name -replace "`r?`n",' '
    $ok = if($Like){$nm -like $Like}elseif($Name){$nm -eq $Name}else{$true}
    if($ok -and $Type -and $ci.ControlType.ProgrammaticName -notlike "*$Type"){$ok=$false}
    if($ok -and $IdLike -and $ci.AutomationId -notlike "*$IdLike*"){$ok=$false}
    if($ok){$hits+=$e}
  }
}
if($Action -eq 'list'){ $hits | % { $ci=$_.Current; "[{0}] '{1}' id={2} off={3}" -f $ci.ControlType.ProgrammaticName.Replace('ControlType.',''),($ci.Name -replace "`r?`n",' '),$ci.AutomationId,$ci.IsOffscreen }; return }
if($hits.Count -le $Nth){ "NOTFOUND '$Name' (hits=$($hits.Count))"; exit 2 }
$e=$hits[$Nth]
switch($Action){
 'invoke' { $e.GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke(); 'invoked' }
 'select' { $e.GetCurrentPattern([Windows.Automation.SelectionItemPattern]::Pattern).Select(); 'selected' }
 'toggle' { $e.GetCurrentPattern([Windows.Automation.TogglePattern]::Pattern).Toggle(); 'toggled' }
 'expand' { $e.GetCurrentPattern([Windows.Automation.ExpandCollapsePattern]::Pattern).Expand(); 'expanded' }
}
