# scripts/serve.ps1 - Build the website and serve the static export locally.
#
# Usage:
#   pwsh scripts/serve.ps1            # http://localhost:3000
#   pwsh scripts/serve.ps1 -Port 4000

[CmdletBinding()]
param(
    [int]$Port = 3000
)

$ErrorActionPreference = 'Stop'

$siteDir  = Join-Path (Split-Path -Parent $PSScriptRoot) 'website'
$buildDir = Join-Path $siteDir 'build'

Push-Location $siteDir
try {
    npm run build
    if ($LASTEXITCODE -ne 0) { throw "next build failed (exit $LASTEXITCODE)" }

    Write-Host "Serving website/build/ on http://localhost:$Port" -ForegroundColor Green
    npx --yes serve@latest $buildDir -l $Port
}
finally {
    Pop-Location
}
