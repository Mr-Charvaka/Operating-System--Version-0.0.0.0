$ErrorActionPreference = "Stop"
Write-Host "Running build in WSL..."
wsl bash build.sh
if ($LASTEXITCODE -ne 0) { Write-Host "Build Failed"; exit 1 }
Write-Host "Build Complete"
