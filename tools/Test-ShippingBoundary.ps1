[CmdletBinding()]
param()

$ErrorActionPreference = 'Stop'
$repoRoot = Split-Path -Parent $PSScriptRoot

function Get-ShippingSource {
	param([Parameter(Mandatory)][string]$Path)

	$active = $true
	$frames = [System.Collections.Generic.Stack[object]]::new()
	$output = [System.Collections.Generic.List[string]]::new()

	foreach ($line in Get-Content -LiteralPath $Path -Encoding UTF8) {
		if ($line -match '^\s*#if\s+!UE_BUILD_SHIPPING\s*$') {
			$frames.Push([pscustomobject]@{ Parent = $active; NonShipping = $true })
			$active = $false
			continue
		}
		if ($line -match '^\s*#if') {
			$frames.Push([pscustomobject]@{ Parent = $active; NonShipping = $false })
			continue
		}
		if ($line -match '^\s*#else\s*$' -and $frames.Count -gt 0) {
			$frame = $frames.Peek()
			if ($frame.NonShipping) {
				$active = $frame.Parent
			}
			continue
		}
		if ($line -match '^\s*#endif' -and $frames.Count -gt 0) {
			$frame = $frames.Pop()
			$active = $frame.Parent
			continue
		}
		if ($active) {
			$output.Add($line)
		}
	}

	return $output -join "`n"
}

$checks = @(
	@{
		Path = 'UE/Graytail/Source/Graytail/UI/GT_SettingsWidget.h'
		Patterns = @('GetDebugSubsystem')
	},
	@{
		Path = 'UE/Graytail/Source/Graytail/UI/GT_SettingsWidget.cpp'
		Patterns = @('作弊模式', 'SetCheatModeEnabled')
	},
	@{
		Path = 'UE/Graytail/Source/Graytail/UI/GT_PauseMenuWidget.h'
		Patterns = @('BuildCheatBox', 'OnCheatApplied')
	},
	@{
		Path = 'UE/Graytail/Source/Graytail/UI/GT_PauseMenuWidget.cpp'
		Patterns = @('作弊面板', 'DebugSetGodMode', 'DebugAddGold')
	},
	@{
		Path = 'UE/Graytail/Source/Graytail/UI/GT_GameHudWidget.cpp'
		Patterns = @('IsCheatModeEnabled', 'OnCheatApplied', 'OnOpenCheatPanel')
	},
	@{
		Path = 'UE/Graytail/Source/Graytail/Debug/GT_DebugSubsystem.h'
		Patterns = @('DebugSetGodMode', 'SetCheatModeEnabled')
	},
	@{
		Path = 'UE/Graytail/Source/Graytail/Debug/GT_EditorManualPlayCommands.cpp'
		Patterns = @('FAutoConsoleCommand', 'gt.HUD')
	},
	@{
		Path = 'UE/Graytail/Source/Graytail/Debug/GT_MetaDebugCommands.cpp'
		Patterns = @('FAutoConsoleCommand', 'gt.MetaReset')
	},
	@{
		Path = 'UE/Graytail/Source/Graytail/Domains/Meta/GT_MetaProgressSubsystem.h'
		Patterns = @('GMGrantItem', 'GMReset')
	}
)

$violations = [System.Collections.Generic.List[string]]::new()
foreach ($check in $checks) {
	$fullPath = Join-Path $repoRoot $check.Path
	$shippingSource = Get-ShippingSource -Path $fullPath
	foreach ($pattern in $check.Patterns) {
		if ($shippingSource -match [regex]::Escape($pattern)) {
			$violations.Add("$($check.Path): Shipping source still contains '$pattern'.")
		}
	}
}

if ($violations.Count -gt 0) {
	$violations | ForEach-Object { Write-Error $_ }
	exit 1
}

Write-Output 'GRAYTAIL_SHIPPING_BOUNDARY|Result=Pass'
