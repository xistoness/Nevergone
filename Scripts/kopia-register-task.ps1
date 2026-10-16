# Registers (or re-registers) the scheduled task "Nevergone Kopia Snapshot".
# Needs an elevated PowerShell (Run as administrator), run by the same user that owns the project:
#   powershell -NoProfile -ExecutionPolicy Bypass -File Scripts/kopia-register-task.ps1
#
# Triggers:
#   - weekly, Sunday 12:00 (covers the disk staying connected);
#   - disk arrival: Microsoft-Windows-Partition/Diagnostic, event ID 1006, which Windows logs
#     whenever a disk or partition appears. It also fires for other devices, in bursts, and
#     at every boot and logon, so the task uses IgnoreNew (a burst becomes a single run) and
#     the script logs "skipped" when the Kopia repository is absent. With the HD attached,
#     expect one snapshot per boot.
# Runs elevated (RunLevel Highest) so Kopia can create a VSS shadow copy.
# Changes against the previous task: battery restrictions off (a backup should not depend on
# the power source) and -WaitForDisk on the action (also makes the weekly run wait 60 s when
# the disk is away, which is harmless).

$ErrorActionPreference = 'Stop'

$principal = [Security.Principal.WindowsPrincipal][Security.Principal.WindowsIdentity]::GetCurrent()
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    throw 'Run this script from an elevated PowerShell (Run as administrator).'
}

$TaskName = 'Nevergone Kopia Snapshot'
$Script = Join-Path $PSScriptRoot 'kopia-snapshot.ps1'
$UserSid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
$Query = '<QueryList><Query Id="0" Path="Microsoft-Windows-Partition/Diagnostic"><Select Path="Microsoft-Windows-Partition/Diagnostic">*[System[(EventID=1006)]]</Select></Query></QueryList>'
$QueryXml = [System.Security.SecurityElement]::Escape($Query)
$ScriptXml = [System.Security.SecurityElement]::Escape($Script)

$Xml = @"
<?xml version="1.0" encoding="UTF-16"?>
<Task version="1.3" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <RegistrationInfo>
    <URI>\$TaskName</URI>
  </RegistrationInfo>
  <Principals>
    <Principal id="Author">
      <UserId>$UserSid</UserId>
      <LogonType>InteractiveToken</LogonType>
      <RunLevel>HighestAvailable</RunLevel>
    </Principal>
  </Principals>
  <Settings>
    <DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries>
    <StopIfGoingOnBatteries>false</StopIfGoingOnBatteries>
    <ExecutionTimeLimit>PT2H</ExecutionTimeLimit>
    <MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy>
    <StartWhenAvailable>true</StartWhenAvailable>
    <IdleSettings>
      <Duration>PT10M</Duration>
      <WaitTimeout>PT1H</WaitTimeout>
      <StopOnIdleEnd>true</StopOnIdleEnd>
      <RestartOnIdle>false</RestartOnIdle>
    </IdleSettings>
    <UseUnifiedSchedulingEngine>true</UseUnifiedSchedulingEngine>
  </Settings>
  <Triggers>
    <CalendarTrigger>
      <StartBoundary>2026-10-13T12:00:00</StartBoundary>
      <ScheduleByWeek>
        <WeeksInterval>1</WeeksInterval>
        <DaysOfWeek>
          <Sunday />
        </DaysOfWeek>
      </ScheduleByWeek>
    </CalendarTrigger>
    <EventTrigger>
      <Enabled>true</Enabled>
      <Subscription>$QueryXml</Subscription>
    </EventTrigger>
  </Triggers>
  <Actions Context="Author">
    <Exec>
      <Command>powershell.exe</Command>
      <Arguments>-NoProfile -WindowStyle Hidden -ExecutionPolicy Bypass -File "$ScriptXml" -WaitForDisk</Arguments>
    </Exec>
  </Actions>
</Task>
"@

Register-ScheduledTask -TaskName $TaskName -Xml $Xml -Force | Out-Null
Get-ScheduledTask -TaskName $TaskName | Select-Object TaskName, State, @{n = 'Triggers'; e = { ($_.Triggers | ForEach-Object { $_.CimClass.CimClassName }) -join ', ' } }
