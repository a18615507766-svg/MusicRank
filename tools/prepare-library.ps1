param(
    [Parameter(Mandatory = $true)][string]$SourceA,
    [Parameter(Mandatory = $true)][string]$SourceB
)

$ErrorActionPreference = "Stop"
$projectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path
$mediaDir = Join-Path $projectRoot "media"
$dataDir = Join-Path $projectRoot "data"
New-Item -ItemType Directory -Force -Path $mediaDir, $dataDir | Out-Null

$files = @(Get-ChildItem -LiteralPath $SourceA, $SourceB -Recurse -File -Filter *.mp3 |
    Sort-Object FullName)
if ($files.Count -ne 104) {
    throw "Expected 104 MP3 files, found $($files.Count)."
}

$rows = foreach ($file in $files) {
    $base = [IO.Path]::GetFileNameWithoutExtension($file.Name)
    $separator = $base.LastIndexOf(" - ")
    if ($separator -gt 0) {
        $title = $base.Substring(0, $separator).Trim()
        $performers = $base.Substring($separator + 3).Trim()
    } else {
        $title = $base.Trim()
        $performers = ([char]0x672a)+([char]0x77e5)+([char]0x6f14)+([char]0x5531)+([char]0x8005)
    }

    $targetName = $file.Name
    $targetPath = Join-Path $mediaDir $targetName
    if ((Test-Path -LiteralPath $targetPath) -and
        ((Get-FileHash -LiteralPath $targetPath -Algorithm SHA256).Hash -ne
         (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash)) {
        $hash = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.Substring(0, 8)
        $targetName = "$([IO.Path]::GetFileNameWithoutExtension($file.Name))-$hash.mp3"
        $targetPath = Join-Path $mediaDir $targetName
    }
    Copy-Item -LiteralPath $file.FullName -Destination $targetPath -Force

    $combined = "$title $performers"
    $isGame = $combined -match "HOYO-MiX|STUDIO"
    $isJapanese = $combined -match "YOASOBI|RADWIMPS|fripSide|Reol" -or
        $combined.ToCharArray().Where({ [int]$_ -ge 0x3040 -and [int]$_ -le 0x30ff }).Count -gt 0
    $isEnglish = @($title.ToCharArray() | Where-Object { [int]$_ -gt 127 }).Count -eq 0 -and
        @($performers.ToCharArray() | Where-Object { [int]$_ -gt 127 }).Count -eq 0

    [pscustomobject]@{
        title        = $title
        performers   = $performers
        lyricist     = ""
        composer     = ""
        album        = ""
        release_date = ""
        genre        = if ($isGame) { ([char]0x6e38)+([char]0x620f)+([char]0x97f3)+([char]0x4e50) } elseif ($isJapanese) { "J-Pop" } elseif ($isEnglish) { "Pop" } else { ([char]0x6d41)+([char]0x884c) }
        language     = if ($isJapanese) { ([char]0x65e5)+([char]0x8bed) } elseif ($isEnglish) { ([char]0x82f1)+([char]0x8bed) } else { ([char]0x4e2d)+([char]0x6587) }
        duration_ms  = 0
        audio_path   = "media/$targetName"
    }
}

$csvPath = Join-Path $dataDir "songs.csv"
$rows | Export-Csv -LiteralPath $csvPath -NoTypeInformation -Encoding UTF8

$copied = @(Get-ChildItem -LiteralPath $mediaDir -File -Filter *.mp3)
if ($copied.Count -ne 104) {
    throw "Expected 104 copied MP3 files, found $($copied.Count)."
}

$sizeMb = [math]::Round(($copied | Measure-Object Length -Sum).Sum / 1MB, 2)
Write-Host "songs=$($rows.Count)"
Write-Host "media=$($copied.Count)"
Write-Host "size_mb=$sizeMb"
Write-Host "csv=$csvPath"
