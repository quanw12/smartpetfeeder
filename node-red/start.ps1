# Nạp biến môi trường từ node-red/.env rồi chạy Node-RED.
# Cách chạy: powershell -ExecutionPolicy Bypass -File node-red\start.ps1
$envFile = Join-Path $PSScriptRoot '.env'
if (-not (Test-Path $envFile)) {
    Write-Error "Chua co $envFile. Copy tu .env.example roi dien thong tin."
    exit 1
}
Get-Content $envFile -Encoding UTF8 | ForEach-Object {
    if ($_ -match '^\s*([A-Za-z_][A-Za-z0-9_]*)\s*=\s*(.*?)\s*$') {
        Set-Item -Path "env:$($Matches[1])" -Value $Matches[2]
    }
}
node-red
