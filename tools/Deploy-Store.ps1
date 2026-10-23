<#
.SYNOPSIS
  Ship the current version to the Microsoft Store in one command.

.DESCRIPTION
  Builds the package, clones the last published submission into a draft, swaps
  in the new package with "What's new" from the version's CHANGELOG.md section,
  uploads and commits it. Committing starts certification.

  Bump <Version> in app\panel-winui\Directory.Build.props, and add its
  CHANGELOG.md section, first.

.PARAMETER Mandatory
  Every installed panel then installs it as soon as it is downloaded, even with
  its window open (Panel\Setup\StoreUpdates.cs). Use it for a release that
  supports a new Edge build: until it lands, every feature that calls into
  msedge.dll stays paused.

.PARAMETER CredentialsFile
  JSON with tenantId, clientId, clientSecret and appId; defaults to
  tools\store-credentials.json (gitignored). Each may instead come from
  AZURE_TENANT_ID, AZURE_CLIENT_ID, AZURE_CLIENT_SECRET or STORE_APP_ID. appId
  is the 12-character Store ID, not the package name.

.PARAMETER SkipBuild
  Upload the package already in dist\ instead of rebuilding.

.PARAMETER WhatIf
  Stop before committing, leaving a draft in Partner Center to review.

.NOTES
  The Entra application is a one-time Partner Center step: Account settings >
  User management > Azure AD applications > add, with the Manager role.
  Docs: https://learn.microsoft.com/windows/uwp/monetize/create-and-manage-submissions-using-windows-store-services

.EXAMPLE
  pwsh tools\Deploy-Store.ps1 -WhatIf
  pwsh tools\Deploy-Store.ps1 -Mandatory
#>
[CmdletBinding()]
param(
    [switch] $Mandatory,
    [string] $CredentialsFile,
    [switch] $SkipBuild,
    [switch] $WhatIf
)

$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Import-Module (Join-Path $PSScriptRoot 'FixEdge.Build.psm1') -Force

$repo = Get-RepoRoot
$dist = Join-Path $repo 'dist'
$apiRoot = 'https://manage.devcenter.microsoft.com/v1.0/my'
$releaseNotesLimit = 1500

function Get-Credentials([string] $path) {
    $fromFile = @{}
    if (Test-Path $path) {
        (Get-Content $path -Raw | ConvertFrom-Json).PSObject.Properties |
            ForEach-Object { $fromFile[$_.Name] = $_.Value }
    }
    $resolved = @{
        TenantId     = $env:AZURE_TENANT_ID     ?? $fromFile['tenantId']
        ClientId     = $env:AZURE_CLIENT_ID     ?? $fromFile['clientId']
        ClientSecret = $env:AZURE_CLIENT_SECRET ?? $fromFile['clientSecret']
        AppId        = $env:STORE_APP_ID        ?? $fromFile['appId']
    }
    $missing = $resolved.GetEnumerator() | Where-Object { -not $_.Value } | ForEach-Object { $_.Key }
    if ($missing) {
        throw "Missing Store credentials: $($missing -join ', '). Put them in $path " +
              '(tenantId, clientId, clientSecret, appId) or the matching AZURE_*/STORE_APP_ID env vars.'
    }
    return $resolved
}

function Get-ReleaseNotes([string] $version) {
    $changelog = Join-Path $repo 'CHANGELOG.md'
    $lines = Get-Content $changelog
    $start = [array]::IndexOf($lines, "## $version")
    if ($start -lt 0) {
        throw "CHANGELOG.md has no '## $version' section. Write what changed before shipping it."
    }
    $body = $lines[($start + 1)..($lines.Count - 1)] |
        ForEach-Object -Begin { $isDone = $false } -Process {
            if ($_ -match '^## ') {
                $isDone = $true
            }
            if (-not $isDone) {
                $_
            }
        }
    $notes = ($body -join "`n").Trim()
    if ($notes.Length -gt $releaseNotesLimit) {
        throw "The $version notes are $($notes.Length) characters; the Store takes $releaseNotesLimit."
    }
    return $notes
}

function Get-AccessToken($credentials) {
    $response = Invoke-RestMethod -Method Post `
        -Uri "https://login.microsoftonline.com/$($credentials.TenantId)/oauth2/token" `
        -ContentType 'application/x-www-form-urlencoded' `
        -Body @{
            grant_type    = 'client_credentials'
            client_id     = $credentials.ClientId
            client_secret = $credentials.ClientSecret
            resource      = 'https://manage.devcenter.microsoft.com'
        }
    return $response.access_token
}

function Invoke-StoreApi([string] $method, [string] $path, $body) {
    $arguments = @{
        Method      = $method
        Uri         = "$apiRoot$path"
        Headers     = @{ Authorization = "Bearer $script:token" }
        ContentType = 'application/json'
    }
    if ($null -ne $body) {
        $arguments.Body = ($body | ConvertTo-Json -Depth 40)
    }
    return Invoke-RestMethod @arguments
}

# The API allows one draft at a time, so one left by a failed run is deleted
# rather than left to block every later release.
function New-Submission([string] $appId) {
    $application = Invoke-StoreApi 'GET' "/applications/$appId"
    if ($application.PSObject.Properties['pendingApplicationSubmission']) {
        $pendingId = $application.pendingApplicationSubmission.id
        Write-Host "  removing an existing draft submission ($pendingId)"
        Invoke-StoreApi 'DELETE' "/applications/$appId/submissions/$pendingId" $null | Out-Null
    }
    return Invoke-StoreApi 'POST' "/applications/$appId/submissions" $null
}

function Set-Package($submission, [string] $fileName) {
    foreach ($package in $submission.applicationPackages) {
        $package.fileStatus = 'PendingDelete'
    }
    $submission.applicationPackages += [pscustomobject]@{
        fileName   = $fileName
        fileStatus = 'PendingUpload'
    }
}

function Set-ReleaseNotes($submission, [string] $notes) {
    foreach ($listing in $submission.listings.PSObject.Properties) {
        $listing.Value.baseListing.releaseNotes = $notes
    }
}

# Windows ignores the flag; the panel's StoreUpdates is what acts on it.
function Set-Mandatory($submission) {
    $submission.packageDeliveryOptions.isMandatoryUpdate = $true
    $submission.packageDeliveryOptions.mandatoryUpdateEffectiveDate =
        [DateTime]::UtcNow.ToString('yyyy-MM-ddTHH:mm:ssZ')
}

# The upload is a zip whose entry names match the fileName fields, PUT to the
# pre-authorised blob URL the draft hands back.
function Send-Package([string] $uploadUrl, [string] $msixPath, [string] $fileNameInZip) {
    $zipPath = Join-Path $env:TEMP "edgeplus-store-$(Get-Random).zip"
    $staging = Join-Path $env:TEMP "edgeplus-store-$(Get-Random)"
    try {
        New-Item -ItemType Directory -Force $staging | Out-Null
        Copy-Item $msixPath (Join-Path $staging $fileNameInZip)
        Compress-Archive -Path (Join-Path $staging '*') -DestinationPath $zipPath -Force
        Invoke-RestMethod -Method Put -Uri $uploadUrl -InFile $zipPath `
            -Headers @{ 'x-ms-blob-type' = 'BlockBlob' } | Out-Null
    } finally {
        Remove-Item $zipPath, $staging -Recurse -Force -ErrorAction SilentlyContinue
    }
}

# Waits for the commit only; certification takes hours and is watched in
# Partner Center.
function Wait-ForCommit([string] $appId, [string] $submissionId) {
    while ($true) {
        Start-Sleep -Seconds 15
        $status = (Invoke-StoreApi 'GET' "/applications/$appId/submissions/$submissionId/status").status
        Write-Host "  status: $status"
        if ($status -ne 'CommitStarted') {
            return $status
        }
    }
}

if (-not $CredentialsFile) {
    $CredentialsFile = Join-Path $PSScriptRoot 'store-credentials.json'
}
$credentials = Get-Credentials $CredentialsFile
$version = Get-ProductVersion
$releaseNotes = Get-ReleaseNotes $version

if (-not $SkipBuild) {
    Write-Step "Building the $version package"
    & (Join-Path $PSScriptRoot 'Package-Msix.ps1')
}

$msix = Join-Path $dist "EdgePlus-$version-x64.msix"
if (-not (Test-Path $msix)) {
    throw "No $msix. Build it first, or drop -SkipBuild."
}
$msixName = Split-Path -Leaf $msix

Write-Step 'Signing in to Partner Center'
$script:token = Get-AccessToken $credentials

Write-Step 'Creating the submission'
$submission = New-Submission $credentials.AppId
Set-Package $submission $msixName
Set-ReleaseNotes $submission $releaseNotes
if ($Mandatory) {
    Set-Mandatory $submission
}
Invoke-StoreApi 'PUT' "/applications/$($credentials.AppId)/submissions/$($submission.id)" $submission | Out-Null

Write-Step 'Uploading the package'
Send-Package $submission.fileUploadUrl $msix $msixName

$partnerCenterUrl =
    "https://partner.microsoft.com/dashboard/products/$($credentials.AppId)/submissions/$($submission.id)"

if ($WhatIf) {
    Write-Step 'Draft ready (not committed)'
    Write-Host "  Review and submit it here: $partnerCenterUrl" -ForegroundColor Yellow
    return
}

Write-Step 'Committing'
Invoke-StoreApi 'POST' "/applications/$($credentials.AppId)/submissions/$($submission.id)/commit" $null | Out-Null
$finalStatus = Wait-ForCommit $credentials.AppId $submission.id

Write-Step 'Handed off'
Write-Host "  Submission $($submission.id): $finalStatus"
Write-Host "  Certification runs now: $partnerCenterUrl" -ForegroundColor Cyan
if ($finalStatus -eq 'CommitFailed') {
    throw 'The commit failed; open the submission above for the reason.'
}
