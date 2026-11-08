param([int]$ProcId,[string]$Title)
Add-Type -AssemblyName UIAutomationClient
$AE=[Windows.Automation.AutomationElement]
$c=New-Object Windows.Automation.PropertyCondition($AE::ProcessIdProperty,$ProcId)
$t=$AE::RootElement.FindAll('Children',$c) | ? { $_.Current.Name -eq $Title } | select -First 1
foreach($d in $t.FindAll('Descendants',[Windows.Automation.Condition]::TrueCondition)){ $pt=($d.GetSupportedPatterns()|%{$_.ProgrammaticName -replace 'PatternIdentifiers.Pattern',''}) -join ','; "{0} '{1}' id={2} pat={3}" -f $d.Current.ControlType.ProgrammaticName.Replace('ControlType.',''),($d.Current.Name -replace "`n",' '),($d.Current.AutomationId -replace '^.*\.',''),$pt }
