param([string]$Root,[switch]$Interactive)
$ErrorActionPreference='Stop';$ProgressPreference='SilentlyContinue'
if(!$Interactive){
 $name='NexoraM7Input-'+[Guid]::NewGuid().ToString('N')
 $entry="& ([scriptblock]::Create([IO.File]::ReadAllText('$Root\input.ps1',[Text.Encoding]::UTF8))) -Root '$Root' -Interactive"
 $encoded=[Convert]::ToBase64String([Text.Encoding]::Unicode.GetBytes($entry))
 $action=New-ScheduledTaskAction -Execute "$PSHOME\powershell.exe" -Argument "-NoProfile -NonInteractive -WindowStyle Hidden -EncodedCommand $encoded"
 $principal=New-ScheduledTaskPrincipal -UserId ([Security.Principal.WindowsIdentity]::GetCurrent().Name) -LogonType Interactive -RunLevel Limited
 $settings=New-ScheduledTaskSettingsSet -ExecutionTimeLimit (New-TimeSpan -Minutes 4)
 Register-ScheduledTask -TaskName $name -Action $action -Principal $principal -Settings $settings | Out-Null
 try{
  Start-ScheduledTask -TaskName $name;$clock=[Diagnostics.Stopwatch]::StartNew()
  do{Start-Sleep -Milliseconds 500;$task=Get-ScheduledTask -TaskName $name;$info=Get-ScheduledTaskInfo -TaskName $name}while(($info.LastRunTime.Year -lt 2001 -or $task.State -eq 'Running') -and $clock.Elapsed.TotalSeconds -lt 250)
  if($task.State -eq 'Running' -or $info.LastTaskResult -ne 0){throw "interactive input task failed: $($info.LastTaskResult)"}
  Get-Content -LiteralPath "$Root\input-result.json" -Raw
 }finally{if((Get-ScheduledTask -TaskName $name).State -ne 'Running'){Unregister-ScheduledTask -TaskName $name -Confirm:$false}}
 exit 0
}
try{
 if([Diagnostics.Process]::GetCurrentProcess().SessionId -le 0){throw 'interactive desktop required'}
 . ([scriptblock]::Create([IO.File]::ReadAllText("$Root\definitions.ps1",[Text.Encoding]::UTF8))) -DefinitionsOnly -Root $Root -Cycle 1
 $env:Path='C:\Windows\System32;C:\Windows;C:\Windows\System32\WindowsPowerShell\v1.0'
 foreach($key in @('VULKAN_SDK','ZIG_LIB_DIR','VSINSTALLDIR','VCINSTALLDIR','WindowsSdkDir','INCLUDE','LIB')){[Environment]::SetEnvironmentVariable($key,$null,'Process')}
 Add-Type -AssemblyName System.IO.Compression.FileSystem
 [IO.Compression.ZipFile]::ExtractToDirectory("$Root\host.zip","$Root\Unicode 封裝")
 $package="$Root\Unicode 封裝";[void](Assert-Inventory $package)
 Add-Type -TypeDefinition @'
using System;using System.Runtime.InteropServices;
public static class M7VmInput {
 [StructLayout(LayoutKind.Sequential)]public struct Point{public int X,Y;}
 [DllImport("user32.dll")]public static extern bool SetForegroundWindow(IntPtr w);
 [DllImport("user32.dll")]public static extern bool SetWindowPos(IntPtr w,IntPtr a,int x,int y,int cx,int cy,uint f);
 [DllImport("user32.dll")]public static extern bool ClientToScreen(IntPtr w,ref Point p);
 [DllImport("user32.dll")]public static extern bool SetCursorPos(int x,int y);
 [DllImport("user32.dll")]public static extern void mouse_event(uint f,uint x,uint y,uint d,UIntPtr e);
 [DllImport("user32.dll")]public static extern void keybd_event(byte key,byte scan,uint f,UIntPtr e);
}
'@
 $saves="$Root\test-saves"
 # Exercise the exported zero-argument launcher from an unrelated Unicode working directory.
 [void][IO.Directory]::CreateDirectory("$Root\unrelated 進度")
 $process=New-Object Diagnostics.Process
 $process.StartInfo.FileName='C:\Windows\System32\cmd.exe'
 $process.StartInfo.WorkingDirectory="$Root\unrelated 進度"
 $process.StartInfo.Arguments='/d /c ""'+$package+'\Run.cmd" --save-directory "'+$saves+'""'
 $process.StartInfo.UseShellExecute=$false;$process.StartInfo.RedirectStandardOutput=$true;$process.StartInfo.RedirectStandardError=$true
 [void]$process.Start();$held=$process.Handle
 $outTask=$process.StandardOutput.ReadToEndAsync();$errTask=$process.StandardError.ReadToEndAsync()
 $clock=[Diagnostics.Stopwatch]::StartNew();$player=$null
 do{
  $players=@(Get-Process NexoraTwoDPlayer -ErrorAction SilentlyContinue)
  if($players.Count -eq 1){$player=$players[0];$player.Refresh();if($player.MainWindowHandle -ne [IntPtr]::Zero){break}}
  if($process.HasExited){throw 'launcher exited before native window'}
  Start-Sleep -Milliseconds 100
 }while($clock.Elapsed.TotalSeconds -lt 60)
 if(!$player -or $player.MainWindowHandle -eq [IntPtr]::Zero){throw 'native window unavailable'}
 [void][M7VmInput]::SetWindowPos($player.MainWindowHandle,[IntPtr]::new(-1),0,0,0,0,0x43)
 [void][M7VmInput]::SetForegroundWindow($player.MainWindowHandle)
 Start-Sleep -Seconds 3
 try{[M7VmInput]::keybd_event(0x44,0,0,[UIntPtr]::Zero);Start-Sleep -Milliseconds 1500}finally{[M7VmInput]::keybd_event(0x44,0,2,[UIntPtr]::Zero)}
 foreach($x in @(80,280)){
  $point=New-Object M7VmInput+Point;$point.X=$x;$point.Y=120
  if(![M7VmInput]::ClientToScreen($player.MainWindowHandle,[ref]$point)){throw 'client coordinate conversion failed'}
  [void][M7VmInput]::SetCursorPos($point.X,$point.Y)
  try{[M7VmInput]::mouse_event(2,0,0,0,[UIntPtr]::Zero);Start-Sleep -Milliseconds 400}finally{[M7VmInput]::mouse_event(4,0,0,0,[UIntPtr]::Zero)}
  Start-Sleep -Milliseconds 800
 }
 Add-Type -AssemblyName System.Drawing
 $origin=New-Object M7VmInput+Point
 [void][M7VmInput]::ClientToScreen($player.MainWindowHandle,[ref]$origin)
 $bitmap=New-Object Drawing.Bitmap(640,480);$graphics=[Drawing.Graphics]::FromImage($bitmap)
 $graphics.CopyFromScreen($origin.X,$origin.Y,0,0,[Drawing.Size]::new(640,480))
 $bitmap.Save("$Root\host-native.png");$graphics.Dispose();$bitmap.Dispose()
 if(!$player.CloseMainWindow()){throw 'normal close rejected'}
 if(!$process.WaitForExit(30000)){throw 'launcher close deadline exceeded'}
 $stdout=$outTask.GetAwaiter().GetResult();$stderr=$errTask.GetAwaiter().GetResult()
 Write-DurableText "$Root\input.stdout" $stdout;Write-DurableText "$Root\input.stderr" $stderr
 $report=$stdout|ConvertFrom-Json
 $line=$stderr.Replace([string][char]13,'').Split([char]10)|Where-Object{$_.StartsWith('host_fixture ')}|Select-Object -Last 1
 $fixture=$line.Substring(13)|ConvertFrom-Json
 if($process.ExitCode -ne 0 -or !$report.allocations_balanced -or $report.native_frames -lt 1 -or $report.mouse_press_events -lt 2 -or $fixture.keyboard_movement_frames -lt 1 -or $fixture.saves -lt 1 -or $fixture.opens -lt 1 -or !$fixture.final_save_ok){throw 'native input/save/open/teardown evidence incomplete'}
 $resumed=Invoke-CapturedProcess -Executable "$package\NexoraTwoDPlayer.exe" -Arguments @('--run-2d-project',"$package\game.nx2game",'--frames','3','--save-directory',$saves) -LogPrefix "$Root\resume"
 if($resumed.exit -ne 0 -or !$resumed.stderr.Contains(('host fixture loaded position={0:F6}' -f $fixture.position))){throw 'independent resume failed'}
 [void](Assert-Inventory $package)
 $remaining=@(Get-Process NexoraTwoDPlayer -ErrorAction SilentlyContinue).Count
 if($remaining -ne 0){throw 'residual Player'}
 $roots=@{old='C:\NexoraAcceptance620';vm='C:\NexoraVmAcceptance';previous='C:\NexoraM7VM';user=(Join-Path $env:LOCALAPPDATA 'Nexora\Games')};$savedFiles=@{}
 foreach($tag in $roots.Keys){$r=$roots[$tag];if(Test-Path -LiteralPath $r){Get-ChildItem -LiteralPath $r -Filter '*.nxsav' -Recurse -File | ForEach-Object{$savedFiles[$tag+'/'+$_.FullName.Substring($r.Length).TrimStart([char]92).Replace([char]92,[char]47)]=@{length=$_.Length;sha256=(Get-FileHash -LiteralPath $_.FullName).Hash.ToLowerInvariant()}}}}
 $preservation=@{saves=$savedFiles;system_vulkan_loader=(Test-Path -LiteralPath 'C:\Windows\System32\vulkan-1.dll')}
 @{status='PASS';scope='Existing VM, automated native keyboard/mouse; not physical input or audible acceptance';desktop_session=[Diagnostics.Process]::GetCurrentProcess().SessionId;launcher_exit=$process.ExitCode;launcher_system_path_only=$true;unrelated_unicode_cwd=$true;inventory_verified=$true;report=$report;fixture=$fixture;resumed=$true;residual_players=$remaining;preservation=$preservation} | ConvertTo-Json -Depth 10 | Set-Content -LiteralPath "$Root\input-result.json" -Encoding UTF8
 $process.Dispose();$player.Dispose()
}catch{
 @{status='FAILED';error=$_.ToString();stack=$_.ScriptStackTrace}|ConvertTo-Json|Set-Content -LiteralPath "$Root\input-failure.json" -Encoding UTF8
 exit 1
}
