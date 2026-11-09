# Local playtest only. To bind a public interface, use the server deployment guide.
param([string]$Python)

if (!$Python) {
    $launcher = Get-Command py.exe -ErrorAction SilentlyContinue
    if ($launcher) {
        $Python = $launcher.Source
    } else {
        $command = Get-Command python.exe -All -ErrorAction SilentlyContinue |
            Where-Object Source -NotLike '*\Microsoft\WindowsApps\*' |
            Select-Object -First 1
        if ($command) {
            $Python = $command.Source
        } else {
            $Python = Get-ChildItem -Path "$env:ProgramFiles\Blender Foundation\Blender *\*\python\bin\python.exe" -ErrorAction SilentlyContinue |
                Sort-Object FullName -Descending | Select-Object -First 1 -ExpandProperty FullName
        }
    }
}
if (!$Python) { throw 'Pass -Python with the path to a Python 3.10+ interpreter.' }
$server = Join-Path $PSScriptRoot '../Projects/DuelFPS/Server/lobby.py'
& $Python -B $server
exit $LASTEXITCODE
