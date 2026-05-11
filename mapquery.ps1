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
$symbols = @()
$currentSection = ""

foreach ($line in Get-Content $MAP) {
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

while ($true) {
    $query = Read-Host "Function"
    if ($query -eq "") { break }

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