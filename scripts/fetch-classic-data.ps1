# Downloads the original Abuse data and installs it where the Original mode
# looks for it on Windows.
#
# The PowerShell twin of fetch-classic-data.sh, which is bash and does not run
# here. Same two tarballs, same SHA-256 list, same rule: nothing is written to
# the destination until every hash checks out, and the write is a rename at
# the end.
#
# Sound effects and music only. Levels, art and Lisp are in the repository and
# are public domain.
#
# Needs PowerShell 5 or newer and tar.exe, which Windows 10 1803 and later
# include. Run it from anywhere:
#
#   powershell -ExecutionPolicy Bypass -File scripts\fetch-classic-data.ps1
#
# NOTE: written on Linux and never run on Windows. If it fails, the bash
# script is the reference for what it should do.

[CmdletBinding()]
param(
    # Where the game looks: the same directory SDL_GetPrefPath names, with
    # "classic" under it.
    [string]$Destination = (Join-Path $env:APPDATA 'Abuse\classic'),

    # Mirror, for testing without the network.
    [string]$BaseUrl = $(if ($env:ABUSE_CLASSIC_URL) { $env:ABUSE_CLASSIC_URL }
                         else { 'http://abuse.zoy.org/raw-attachment/wiki/download' })
)

$ErrorActionPreference = 'Stop'

$root = Split-Path -Parent (Split-Path -Parent $MyInvocation.MyCommand.Path)
$sums = Join-Path $root 'data\classic.sha256'

if (-not (Test-Path $sums)) {
    throw "missing $sums"
}
if (-not (Get-Command tar.exe -ErrorAction SilentlyContinue)) {
    throw 'tar.exe not found. Windows 10 1803 and later ship it; older versions need it installed.'
}

$tmp = Join-Path ([System.IO.Path]::GetTempPath()) ("abuse-classic-" + [System.Guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $tmp | Out-Null

try {
    Write-Host "destination: $Destination"

    # Same file as the bash script reads: "<sha256>  <file>", with comments.
    $wanted = Get-Content $sums |
        Where-Object { $_ -notmatch '^\s*#' -and $_ -match '\S' } |
        ForEach-Object {
            $parts = $_ -split '\s+', 2
            [pscustomobject]@{ Hash = $parts[0].Trim(); File = $parts[1].Trim() }
        }

    foreach ($item in $wanted) {
        $target = Join-Path $tmp $item.File
        Write-Host "downloading $($item.File)"

        # The progress bar makes Invoke-WebRequest an order of magnitude
        # slower on some versions, and this is a 10 MB download.
        $previousProgress = $ProgressPreference
        $ProgressPreference = 'SilentlyContinue'
        try {
            Invoke-WebRequest -Uri "$($BaseUrl)/$($item.File)" -OutFile $target -UseBasicParsing
        } finally {
            $ProgressPreference = $previousProgress
        }

        $got = (Get-FileHash -Algorithm SHA256 -Path $target).Hash.ToLowerInvariant()
        if ($got -ne $item.Hash.ToLowerInvariant()) {
            Write-Error "SHA-256 of $($item.File) does not match; nothing was written"
            Write-Error "  expected: $($item.Hash)"
            Write-Error "  got:      $got"
            throw 'checksum mismatch'
        }
        Write-Host '  hash ok'
    }

    # Extracted with no transformation at all: the files go in as the tarball
    # has them. Modifying this data is forbidden (AGENTS.md rule 1).
    $stage = Join-Path $tmp 'stage'
    New-Item -ItemType Directory -Path (Join-Path $stage 'sfx') | Out-Null
    New-Item -ItemType Directory -Path (Join-Path $stage 'music') | Out-Null

    & tar.exe -xzf (Join-Path $tmp 'abuse-sfx-2.00.tar.gz') -C $tmp
    if ($LASTEXITCODE -ne 0) { throw 'could not extract abuse-sfx-2.00.tar.gz' }

    & tar.exe -xzf (Join-Path $tmp 'abuse-data-2.00.tar.gz') -C $tmp
    if ($LASTEXITCODE -ne 0) { throw 'could not extract abuse-data-2.00.tar.gz' }

    # Flattened into two directories, because that is where the game looks;
    # the tarballs carry their own layout.
    $copied = @{ wav = 0; hmi = 0 }
    foreach ($pair in @(@('*.wav', 'sfx', 'wav'), @('*.hmi', 'music', 'hmi'))) {
        Get-ChildItem -Path $tmp -Filter $pair[0] -Recurse -File |
            Where-Object { $_.FullName -notlike "$stage*" } |
            ForEach-Object {
                $to = Join-Path (Join-Path $stage $pair[1]) $_.Name
                if (-not (Test-Path $to)) {
                    Copy-Item $_.FullName $to
                    $copied[$pair[2]]++
                }
            }
    }

    Write-Host "extracted: $($copied.wav) effects, $($copied.hmi) music tracks"
    if ($copied.wav -eq 0) {
        throw 'no sound effect extracted'
    }

    # The destination only ever exists complete.
    $parent = Split-Path -Parent $Destination
    if (-not (Test-Path $parent)) {
        New-Item -ItemType Directory -Path $parent | Out-Null
    }
    if (Test-Path $Destination) {
        $previous = "$Destination.previous.$PID"
        Move-Item $Destination $previous
        Move-Item $stage $Destination
        Remove-Item -Recurse -Force $previous
    } else {
        Move-Item $stage $Destination
    }

    Write-Host "done: $Destination"
    Write-Host 'The Original mode finds this on its own; no command line needed.'
}
finally {
    Remove-Item -Recurse -Force $tmp -ErrorAction SilentlyContinue
}
