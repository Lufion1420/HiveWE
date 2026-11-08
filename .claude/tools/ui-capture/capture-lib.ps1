# Usage: . capture-lib.ps1 ; Cap-Window <hwnd> <outpng>
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices; using System.Text; using System.Collections.Generic;
public static class W {
 [StructLayout(LayoutKind.Sequential)] public struct RECT{public int L,T,R,B;}
 [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h,out RECT r);
 [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h,IntPtr dc,uint f);
 [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
 [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h,out uint pid);
 [DllImport("user32.dll")] public static extern bool IsWindowVisible(IntPtr h);
 [DllImport("user32.dll")] public static extern int GetWindowText(IntPtr h,StringBuilder s,int n);
 [DllImport("user32.dll")] public static extern bool EnumWindows(EnumProc cb,IntPtr l);
 [DllImport("user32.dll")] public static extern bool SetProcessDPIAware();
 [DllImport("user32.dll")] public static extern bool SetProcessDpiAwarenessContext(IntPtr c);
 [DllImport("user32.dll")] public static extern uint GetDpiForWindow(IntPtr h);
 public delegate bool EnumProc(IntPtr h,IntPtr l);
 public static List<IntPtr> TopWindows(uint pid){ var r=new List<IntPtr>(); EnumWindows((h,l)=>{uint p; GetWindowThreadProcessId(h,out p); if(p==pid && IsWindowVisible(h)) r.Add(h); return true;},IntPtr.Zero); return r;}
 public static string Title(IntPtr h){var sb=new StringBuilder(256);GetWindowText(h,sb,256);return sb.ToString();}
 public static uint FgPid(){uint p; GetWindowThreadProcessId(GetForegroundWindow(),out p); return p;}
}
"@
[W]::SetProcessDpiAwarenessContext([IntPtr](-4)) | Out-Null  # per-monitor v2: rect is physical; PrintWindow paints Qt logical px top-left, so crop to physical*96/dpi
Add-Type -ReferencedAssemblies System.Drawing -TypeDefinition @"
using System; using System.Drawing; using System.Drawing.Imaging; using System.Runtime.InteropServices;
public static class Cap {
 [StructLayout(LayoutKind.Sequential)] public struct RECT{public int L,T,R,B;}
 [DllImport("user32.dll")] static extern bool GetWindowRect(IntPtr h,out RECT r);
 [DllImport("user32.dll")] static extern bool PrintWindow(IntPtr h,IntPtr dc,uint f);
 public static string Run(IntPtr h, string path) {
  RECT r; GetWindowRect(h, out r);
  int cw = (int)((r.R-r.L)*1.5)+16, ch = (int)((r.B-r.T)*1.5)+16;   // oversize canvas: PrintWindow paints Qt content at its own scale, rect units are unreliable across DPI contexts
  using (var bmp = new Bitmap(cw, ch, PixelFormat.Format32bppArgb)) {
   using (var g = Graphics.FromImage(bmp)) { g.Clear(Color.FromArgb(255,255,0,255)); IntPtr dc = g.GetHdc(); PrintWindow(h, dc, 2); g.ReleaseHdc(dc); }
   var bd = bmp.LockBits(new Rectangle(0,0,cw,ch), ImageLockMode.ReadOnly, PixelFormat.Format32bppArgb);
   int maxX=-1, maxY=-1; int key = unchecked((int)0xFFFF00FF);
   {
    int[] row = new int[cw];
    for (int y=0;y<ch;y++){ Marshal.Copy(IntPtr.Add(bd.Scan0, y*bd.Stride), row, 0, cw);
      for (int x=cw-1;x>=0;x--){ if (row[x]!=key){ if(x>maxX)maxX=x; if(y>maxY)maxY=y; break; } } }
   }
   bmp.UnlockBits(bd);
   if (maxX<0) return "BLANK";
   using (var crop = bmp.Clone(new Rectangle(0,0,maxX+1,maxY+1), bmp.PixelFormat)) crop.Save(path, ImageFormat.Png);
   return (maxX+1)+"x"+(maxY+1)+" (rect "+(r.R-r.L)+"x"+(r.B-r.T)+")";
  }
 }
}
"@
function Cap-Window($h,$out){ "$out " + [Cap]::Run([IntPtr]$h,$out) }
function Cap-Screen($h,$out){  # fallback: needs window visible on top
  $r=New-Object W+RECT; [W]::GetWindowRect([IntPtr]$h,[ref]$r)|Out-Null
  $w=$r.R-$r.L;$ht=$r.B-$r.T
  $bmp=New-Object Drawing.Bitmap $w,$ht
  $g=[Drawing.Graphics]::FromImage($bmp);$g.CopyFromScreen($r.L,$r.T,0,0,$bmp.Size);$g.Dispose()
  $bmp.Save($out,[Drawing.Imaging.ImageFormat]::Png);$bmp.Dispose();"$out ${w}x$ht (screen)"
}
function Get-MyWindows($procId){ [W]::TopWindows([uint32]$procId) | % { [pscustomobject]@{Hwnd=$_.ToInt64();Title=[W]::Title($_)} } }



