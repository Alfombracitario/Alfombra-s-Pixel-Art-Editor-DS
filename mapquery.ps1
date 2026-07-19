#este código fué hecho por IA, lo uso para obtener cierta información sin usar el .map
$MAP     = "build\A-Pix.map"
$ELF     = "build\A-Pix.elf"
$OBJDUMP = "C:\msys64\opt\wonderful\toolchain\gcc-arm-none-eabi\bin\arm-none-eabi-objdump.exe"

if (-not (Test-Path $MAP)) {
    Write-Host "ERROR: No se encontro $MAP" -ForegroundColor Red
    Read-Host "Presiona ENTER para salir"
    exit 1
}

$elfAvailable = (Test-Path $ELF) -and (Test-Path $OBJDUMP)

# Leer todas las lineas con direccion + simbolo
$mapLines = Get-Content $MAP
$symbols = @()
$currentSection = ""

foreach ($line in $mapLines) {
    if ($line -match '^\s*(\.\S+)\s+0x[0-9a-fA-F]+') {
        $currentSection = $Matches[1]
    }
    if ($line -match '^\s+(0x[0-9a-fA-F]{8,})\s+(\S+)\s*$') {
        $symbols += [PSCustomObject]@{
            Addr    = [uint32]$Matches[1]
            Name    = $Matches[2]
            Section = $currentSection
        }
    }
}

$sorted = $symbols | Sort-Object Addr

Write-Host ""
Write-Host "  Map cargado: $($symbols.Count) simbolos encontrados" -ForegroundColor DarkGray
if (-not $elfAvailable) {
    Write-Host "  (objdump no disponible: no se mostrara padding)" -ForegroundColor DarkGray
}
Write-Host ""

function Get-Padding($sym, $totalSize) {
    # Llamar a objdump y obtener el disassembly de la funcion
    $addrHex = $sym.Addr.ToString('X')
    $raw = & $OBJDUMP -d --start-address="0x$addrHex" `
                      --stop-address="0x$(($sym.Addr + $totalSize).ToString('X'))" `
                      $ELF 2>$null

    if (-not $raw) { return $null }

    # Extraer lineas con instrucciones: "   1000028:   e3..."
    $instrLines = $raw | Where-Object { $_ -match '^\s+[0-9a-f]+:\s+[0-9a-f]' }
    if (-not $instrLines) { return $null }

    # Primera y ultima instruccion
    $first = $instrLines[0]
    $last  = $instrLines[-1]

    $first -match '^\s+([0-9a-f]+):\s+([0-9a-f ]+?)\s' | Out-Null
    $firstAddr = [uint32]("0x" + $Matches[1])
    $firstBytes = ($Matches[2].Trim() -split '\s+').Count

    $last -match '^\s+([0-9a-f]+):\s+([0-9a-f ]+?)\s' | Out-Null
    $lastAddr  = [uint32]("0x" + $Matches[1])
    $lastBytes = ($Matches[2].Trim() -split '\s+').Count

    $paddingBefore = $firstAddr - $sym.Addr
    $codeEnd       = $lastAddr + $lastBytes
    $paddingAfter  = ($sym.Addr + $totalSize) - $codeEnd

    return [PSCustomObject]@{
        PaddingBefore = $paddingBefore
        CodeSize      = $codeEnd - $firstAddr
        PaddingAfter  = $paddingAfter
    }
}

function Get-MemoryRegions($mapLines) {
    # Tamanios fijos de hardware del ARM9 en NDS/DSi
    $ITCM_SIZE = 32KB
    $DTCM_SIZE = 16KB
    $RAM_SIZE  = 4MB   # asume modo NTR (4MB). Si el build usa RAM extendida de DSi, este total no aplica.

    $regions = [ordered]@{
        "ITCM"       = @{ Total = $ITCM_SIZE; Used = 0; Sections = @{} }
        "DTCM"       = @{ Total = $DTCM_SIZE; Used = 0; Sections = @{} }
        "RAM (main)" = @{ Total = $RAM_SIZE;  Used = 0; Sections = @{} }
    }

    $itcmSections = @('.itcm', '.vectors')
    $dtcmSections = @('.dtcm', '.sbss')
    $skipSections = @('.secure', '.comment', '.ARM.attributes', '.symtab', '.strtab', '.shstrtab')

    foreach ($line in $mapLines) {
        # Solo lineas de "seccion total" (sin indentacion), no las contribuciones de cada .o
        if ($line -match '^(\.[A-Za-z0-9_.]+)\s+0x([0-9a-fA-F]+)\s+0x([0-9a-fA-F]+)') {
            $secName = $Matches[1]
            $secSize = [Convert]::ToUInt64($Matches[3], 16)

            if ($secSize -eq 0) { continue }
            if ($secName -match '^\.debug_' -or ($skipSections -contains $secName)) { continue }

            if ($itcmSections -contains $secName) {
                $regions["ITCM"].Used += $secSize
                $regions["ITCM"].Sections[$secName] = $secSize
            } elseif ($dtcmSections -contains $secName) {
                $regions["DTCM"].Used += $secSize
                $regions["DTCM"].Sections[$secName] = $secSize
            } else {
                $regions["RAM (main)"].Used += $secSize
                $regions["RAM (main)"].Sections[$secName] = $secSize
            }
        }
    }

    return $regions
}

function Show-MemoryUsage($regions) {
    Write-Host "  === Uso de memoria por region (ARM9) ===" -ForegroundColor Cyan
    foreach ($key in $regions.Keys) {
        $r = $regions[$key]
        $usedKB  = [math]::Round($r.Used / 1KB, 2)
        $totalKB = [math]::Round($r.Total / 1KB, 2)
        $pct     = if ($r.Total -gt 0) { [math]::Round(($r.Used / $r.Total) * 100, 1) } else { 0 }

        $color = if ($pct -ge 90) { "Red" } elseif ($pct -ge 70) { "Yellow" } else { "Green" }
        Write-Host ("    {0,-11}: {1,9} KB / {2,-9} KB  ({3}%)" -f $key, $usedKB, $totalKB, $pct) -ForegroundColor $color

        foreach ($sec in $r.Sections.Keys) {
            $secKB = [math]::Round($r.Sections[$sec] / 1KB, 2)
            Write-Host ("        $sec : $secKB KB") -ForegroundColor DarkGray
        }
    }
    Write-Host "  (RAM (main) asume 4MB, modo NTR. Escribe 'mem' para volver a ver esta tabla.)" -ForegroundColor DarkGray
    Write-Host ""
}

Show-MemoryUsage (Get-MemoryRegions $mapLines)

while ($true) {
    $query = Read-Host "Function"
    if ($query -eq "") { break }

    if ($query -eq "mem") {
        Show-MemoryUsage (Get-MemoryRegions $mapLines)
        continue
    }

    $matches_ = $sorted | Where-Object { $_.Name -like "*$query*" }

    if (-not $matches_) {
        Write-Host "  No se encontro: $query" -ForegroundColor Yellow
        Write-Host ""
        continue
    }

    foreach ($sym in $matches_) {
        $next = $sorted | Where-Object { $_.Addr -gt $sym.Addr } | Select-Object -First 1

        Write-Host ""
        Write-Host "  $($sym.Name)" -ForegroundColor Cyan
        Write-Host "    Seccion  : $($sym.Section)"
        Write-Host "    Direccion: 0x$($sym.Addr.ToString('X8'))"

        if ($next) {
            $size      = $next.Addr - $sym.Addr
            $lines     = [math]::Floor($size / 32)
            $remainder = $size % 32
            Write-Host "    Tamanio  : $size bytes  ($lines lineas cache + $remainder bytes)"

            if ($elfAvailable) {
                $pad = Get-Padding $sym $size
                if ($pad) {
                    Write-Host "    Codigo   : $($pad.CodeSize) bytes" -ForegroundColor Green
                    if ($pad.PaddingBefore -gt 0) {
                        Write-Host "    Padding antes : $($pad.PaddingBefore) bytes" -ForegroundColor Yellow
                    }
                    if ($pad.PaddingAfter -gt 0) {
                        Write-Host "    Padding despues: $($pad.PaddingAfter) bytes" -ForegroundColor Yellow
                    }
                    if ($pad.PaddingBefore -eq 0 -and $pad.PaddingAfter -eq 0) {
                        Write-Host "    Sin padding (perfectamente alineada)" -ForegroundColor Green
                    }
                }
            }
        } else {
            Write-Host "    Tamanio  : desconocido (ultimo simbolo del map)"
        }
    }

    Write-Host ""
}