param(
    [ValidateSet('Install', 'Uninstall')]
    [string]$Mode = 'Install'
)

$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
$title = 'DaBa Setup'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (-not $principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
    try {
        $arguments = '-NoProfile -ExecutionPolicy Bypass -File "{0}" -Mode {1}' -f $PSCommandPath, $Mode
        $process = Start-Process -FilePath "$PSHOME\powershell.exe" -Verb RunAs -ArgumentList $arguments -Wait -PassThru
        exit $process.ExitCode
    } catch {
        [System.Windows.Forms.MessageBox]::Show('Administrator approval was cancelled or could not be obtained.', $title, 'OK', 'Warning') | Out-Null
        exit 1
    }
}

# Always use the machine-wide 64-bit locations, including from a 32-bit shell.
$programFiles = [Environment]::GetEnvironmentVariable('ProgramW6432')
if (-not $programFiles) { $programFiles = [Environment]::GetFolderPath('ProgramFiles') }
$target = Join-Path $programFiles 'DaBa'
$installedExe = Join-Path $target 'DaBa.exe'
$uninstallScript = Join-Path $target 'setup.ps1'
$shortcut = Join-Path ([Environment]::GetFolderPath('CommonPrograms')) 'DaBa.lnk'
$registryPath = 'SOFTWARE\Microsoft\Windows\CurrentVersion\Uninstall\DaBa'
$registry = $null
$key = $null

try {
    $registry = [Microsoft.Win32.RegistryKey]::OpenBaseKey('LocalMachine', 'Registry64')
    # Do not overwrite or remove files while the installed player is running.
    $running = Get-Process -Name DaBa -ErrorAction SilentlyContinue | Where-Object {
        $_.Path -and [string]::Equals($_.Path, $installedExe, [StringComparison]::OrdinalIgnoreCase)
    }
    if ($running) { throw 'Close the installed DaBa player and try again.' }

    if ($Mode -eq 'Install') {
        $source = $PSScriptRoot
        if (-not (Test-Path -LiteralPath (Join-Path $source 'DaBa.exe'))) {
            $source = Join-Path $PSScriptRoot 'bin'
        }
        foreach ($name in @('DaBa.exe', 'libmpv-2.dll')) {
            if (-not (Test-Path -LiteralPath (Join-Path $source $name) -PathType Leaf)) {
                throw "Required installation file is missing: $name"
            }
        }
        New-Item -ItemType Directory -Path $target -Force | Out-Null
        foreach ($name in @('DaBa.exe', 'libmpv-2.dll')) {
            $from = Join-Path $source $name
            $to = Join-Path $target $name
            if (-not [string]::Equals([IO.Path]::GetFullPath($from), $to, [StringComparison]::OrdinalIgnoreCase)) {
                Copy-Item -LiteralPath $from -Destination $to -Force
            }
        }
        if (-not [string]::Equals($PSCommandPath, $uninstallScript, [StringComparison]::OrdinalIgnoreCase)) {
            Copy-Item -LiteralPath $PSCommandPath -Destination $uninstallScript -Force
        }
        $shell = New-Object -ComObject WScript.Shell
        $link = $shell.CreateShortcut($shortcut)
        $link.TargetPath = $installedExe
        $link.WorkingDirectory = $target
        $link.IconLocation = "$installedExe,0"
        $link.Save()
        $key = $registry.CreateSubKey($registryPath)
        $key.SetValue('DisplayName', 'DaBa')
        $key.SetValue('DisplayVersion', '0.0.2')
        $key.SetValue('DisplayIcon', $installedExe)
        $key.SetValue('InstallLocation', $target)
        $powershell = Join-Path $env:SystemRoot 'System32\WindowsPowerShell\v1.0\powershell.exe'
        $key.SetValue('UninstallString', ('"{0}" -NoProfile -ExecutionPolicy Bypass -File "{1}" -Mode Uninstall' -f $powershell, $uninstallScript))
        $key.SetValue('NoModify', 1, [Microsoft.Win32.RegistryValueKind]::DWord)
        $key.SetValue('NoRepair', 1, [Microsoft.Win32.RegistryValueKind]::DWord)
        $key.SetValue('EstimatedSize', [int]((Get-Item -LiteralPath $installedExe).Length / 1024 + (Get-Item -LiteralPath (Join-Path $target 'libmpv-2.dll')).Length / 1024), [Microsoft.Win32.RegistryValueKind]::DWord)
        [System.Windows.Forms.MessageBox]::Show("DaBa was installed to $target", $title, 'OK', 'Information') | Out-Null
    } else {
        $key = $registry.OpenSubKey($registryPath)
        if (-not $key -or $key.GetValue('InstallLocation') -ne $target) {
            throw 'A registered DaBa installation was not found.'
        }
        $answer = [System.Windows.Forms.MessageBox]::Show('Remove DaBa? Personal settings and playback history will be kept.', $title, 'YesNo', 'Question')
        if ($answer -ne [System.Windows.Forms.DialogResult]::Yes) { exit 0 }
        # Delete only installer-owned files. Never recursively delete this folder.
        foreach ($name in @('DaBa.exe', 'libmpv-2.dll')) {
            $file = Join-Path $target $name
            if (Test-Path -LiteralPath $file) { Remove-Item -LiteralPath $file -Force }
        }
        if (Test-Path -LiteralPath $shortcut) { Remove-Item -LiteralPath $shortcut -Force }
        if (Test-Path -LiteralPath $uninstallScript) { Remove-Item -LiteralPath $uninstallScript -Force }
        if ((Get-ChildItem -LiteralPath $target -Force | Measure-Object).Count -eq 0) {
            Remove-Item -LiteralPath $target
        }
        $key.Dispose()
        $key = $null
        $registry.DeleteSubKeyTree($registryPath, $false)
        [System.Windows.Forms.MessageBox]::Show('DaBa was removed. Personal settings and playback history were kept.', $title, 'OK', 'Information') | Out-Null
    }
} catch {
    [System.Windows.Forms.MessageBox]::Show($_.Exception.Message, $title, 'OK', 'Error') | Out-Null
    exit 1
} finally {
    if ($key) { $key.Dispose() }
    if ($registry) { $registry.Dispose() }
}
