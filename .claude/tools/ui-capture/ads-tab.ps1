# Usage: powershell -File ads-tab.ps1 -ProcId N -Window "Object Editor" -Index 4 [-List]
# ADS dock tabs (ads::CDockWidgetTab) expose Invoke but no Name; address them by index in left-to-right order.
param([int]$ProcId,[string]$Window,[int]$Index=0,[switch]$List)
Add-Type -AssemblyName UIAutomationClient
$AE=[Windows.Automation.AutomationElement]
$c=New-Object Windows.Automation.PropertyCondition($AE::ProcessIdProperty,$ProcId)
$top=$AE::RootElement.FindAll('Children',$c) | ? { $_.Current.Name -eq $Window } | select -First 1
$tabs=@($top.FindAll('Descendants',[Windows.Automation.Condition]::TrueCondition) | ? { $_.Current.AutomationId -like '*ads::CDockWidgetTab' })
if($List){ $i=0; foreach($t in $tabs){ $lbl=$t.FindFirst('Descendants',(New-Object Windows.Automation.PropertyCondition($AE::AutomationIdProperty,'dwl'))); "[$i] rect=$($t.Current.BoundingRectangle) label=" + (($t.FindAll('Descendants',[Windows.Automation.Condition]::TrueCondition)|?{$_.Current.Name}|%{$_.Current.Name}) -join '/'); $i++ }; return }
$tabs[$Index].GetCurrentPattern([Windows.Automation.InvokePattern]::Pattern).Invoke(); "invoked tab $Index of $($tabs.Count)"
