$ErrorActionPreference = "Stop"

$root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location $root

$sourceFiles = Get-ChildItem -Recurse -Include *.hpp, *.cpp |
    Where-Object { $_.FullName -notmatch "\\build\\" }
$lineCount = ($sourceFiles | Get-Content | Measure-Object -Line).Lines
Write-Host "Строк C++:" $lineCount
if ($lineCount -lt 5000 -or $lineCount -gt 10000) {
    throw "Объём должен находиться в диапазоне 5000–10000 строк"
}

$longLines = rg -n '.{81}' include src tests tools
if ($LASTEXITCODE -eq 0) {
    $longLines
    throw "Найдены строки длиннее 80 символов"
}

$rawAllocation = rg -n '\bnew\s|\bdelete\s' include src tests tools
if ($LASTEXITCODE -eq 0) {
    $rawAllocation
    throw "Найдено явное управление динамической памятью"
}

& .\build.bat
if ($LASTEXITCODE -ne 0) {
    throw "Сборка завершилась с ошибкой"
}

& .\tests.bat
if ($LASTEXITCODE -ne 0) {
    throw "Тесты завершились с ошибкой"
}

Write-Host "Проверка качества завершена успешно."
