param([switch]$Editor)
$root = Split-Path -Parent $PSScriptRoot
$engine = Join-Path $root 'out/build/x64-Release/VelcrynEditor.exe'
$project = Join-Path $root 'Projects/Emberwild/Emberwild.project'
if (!(Test-Path -LiteralPath $engine)) { throw 'Build the engine in x64-Release first.' }
if ($Editor) { & $engine $project } else { & $engine --game $project }
