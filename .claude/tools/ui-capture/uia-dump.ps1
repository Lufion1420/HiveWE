param([int]$ProcId,[int]$Depth=6,[string]$Out)
Add-Type -AssemblyName UIAutomationClient,UIAutomationTypes
$root=[Windows.Automation.AutomationElement]::RootElement
$c=New-Object Windows.Automation.PropertyCondition([Windows.Automation.AutomationElement]::ProcessIdProperty,$ProcId)
$tops=$root.FindAll([Windows.Automation.TreeScope]::Children,$c)
$sb=New-Object Text.StringBuilder
function Walk($e,$d){
  if($d -gt $Depth){return}
  $ci=$e.Current
  $null=$sb.AppendLine(("{0}[{1}] '{2}' id='{3}' cls={4} off={5}" -f ('  '*$d),$ci.ControlType.ProgrammaticName.Replace('ControlType.',''),$ci.Name,$ci.AutomationId,$ci.ClassName,$ci.IsOffscreen))
  $ch=$e.FindAll([Windows.Automation.TreeScope]::Children,[Windows.Automation.Condition]::TrueCondition)
  foreach($x in $ch){Walk $x ($d+1)}
}
foreach($t in $tops){Walk $t 0}
$sb.ToString() | Set-Content $Out
$sb.Length
