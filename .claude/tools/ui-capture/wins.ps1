param([int]$ProcId,[string]$SetText="",[int]$EditId=0,[int]$ClickId=0)
Add-Type @"
using System;using System.Text;using System.Collections.Generic;using System.Runtime.InteropServices;
public class WW{public delegate bool EP(IntPtr h,IntPtr l);
[DllImport("user32")]public static extern bool EnumWindows(EP f,IntPtr l);
[DllImport("user32")]public static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);
[DllImport("user32",CharSet=CharSet.Unicode)]public static extern int GetWindowText(IntPtr h,StringBuilder s,int n);
[DllImport("user32",CharSet=CharSet.Unicode)]public static extern int GetClassName(IntPtr h,StringBuilder s,int n);
[DllImport("user32")]public static extern bool EnumChildWindows(IntPtr h,EP f,IntPtr l);
[DllImport("user32")]public static extern int GetDlgCtrlID(IntPtr h);
[DllImport("user32")]public static extern bool IsWindowVisible(IntPtr h);
[DllImport("user32",CharSet=CharSet.Unicode)]public static extern IntPtr SendMessage(IntPtr h,int m,IntPtr w,string l);
[DllImport("user32")]public static extern IntPtr SendMessage(IntPtr h,int m,IntPtr w,IntPtr l);
public static List<IntPtr> tops=new List<IntPtr>();
public static List<IntPtr> kids=new List<IntPtr>();
public static void Scan(uint pid){tops.Clear();EnumWindows((h,l)=>{uint p;GetWindowThreadProcessId(h,out p);if(p==pid)tops.Add(h);return true;},IntPtr.Zero);}
public static void GetKids(IntPtr h){kids.Clear();EnumChildWindows(h,(c,l)=>{kids.Add(c);return true;},IntPtr.Zero);}
public static string T(IntPtr h){var s=new StringBuilder(300);GetWindowText(h,s,300);return s.ToString();}
public static string C(IntPtr h){var s=new StringBuilder(100);GetClassName(h,s,100);return s.ToString();}
}
"@
[WW]::Scan($ProcId)
foreach($h in @([WW]::tops)){ if(-not [WW]::IsWindowVisible($h)){continue}; "{0} [{1}] '{2}' vis={3}" -f $h,[WW]::C($h),[WW]::T($h),[WW]::IsWindowVisible($h)
 if([WW]::C($h) -eq '#32770'){
   [WW]::GetKids($h)
   foreach($k in @([WW]::kids)){ $id=[WW]::GetDlgCtrlID($k); "   child id=$id [{0}] '{1}'" -f [WW]::C($k),[WW]::T($k)
     if($SetText -and $id -eq $EditId){[void][WW]::SendMessage($k,0xC,[IntPtr]::Zero,$SetText); "   SET"}
   }
   if($ClickId){ foreach($k in @([WW]::kids)){ if([WW]::GetDlgCtrlID($k) -eq $ClickId){[void][WW]::SendMessage($k,0xF5,[IntPtr]::Zero,[IntPtr]::Zero); "   CLICKED"; break} } }
 } }
