# Generates a release keystore and a local-only credentials file under E:\Security.
# This is NOT stored in git. There is no blockchain "seed" — Android signing uses a
# Java keystore (.jks) + passwords. Back up the folder; restrict NTFS to your user.
#
# Usage: powershell -ExecutionPolicy Bypass -File scripts\generate-android-keystore.ps1
# Requires: JDK keytool on PATH (same as Android Studio / Android builds).

$ErrorActionPreference = "Stop"

$DestRoot = "E:\Security\AttuneHelperCompanion"
$KeystoreName = "attune-release.jks"
$Alias = "attune"
$CredFileName = "CREDENTIALS-READ-ONCE-STORE-SAFELY.txt"

function New-SecurePassword {
    param([int]$Length = 32)
    $r = [System.Security.Cryptography.RandomNumberGenerator]::Create()
    $chars = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
    -join (1..$Length | ForEach-Object {
        $b = New-Object byte[] 1
        do { $r.GetBytes($b) } while ($b[0] -ge 250)
        $chars[$b[0] % $chars.Length]
    })
}

New-Item -ItemType Directory -Force -Path $DestRoot | Out-Null
$KeystorePath = Join-Path $DestRoot $KeystoreName
$CredPath = Join-Path $DestRoot $CredFileName

if (Test-Path -LiteralPath $KeystorePath) {
    Write-Error "Keystore already exists: $KeystorePath`nDelete it first if you really want a new one."
    exit 1
}

$kt = Get-Command keytool -ErrorAction SilentlyContinue
if (-not $kt) {
    Write-Error "keytool not found on PATH. Install a JDK (or add Android Studio's jbr\bin to PATH) and retry."
    exit 1
}

$StorePass = New-SecurePassword
$KeyPass = New-SecurePassword

# Distinguished name: generic; you can re-generate with your own -dname if you prefer.
$dname = "CN=Attune Helper Companion, OU=Release, O=Attune, C=US"

& keytool -genkeypair -v `
    -keystore $KeystorePath `
    -alias $Alias `
    -keyalg RSA -keysize 2048 -validity 10000 `
    -storepass $StorePass -keypass $KeyPass `
    -dname $dname

if ($LASTEXITCODE -ne 0) {
    Write-Error "keytool failed with exit code $LASTEXITCODE"
    exit $LASTEXITCODE
}

$B64 = [Convert]::ToBase64String([IO.File]::ReadAllBytes($KeystorePath))

$body = @"
Attune Helper Companion — Android release signing (local only; not for git)

What this is: a standard Java KeyStore (.jks), not a cryptocurrency seed phrase.
The private key is inside the keystore file; the passwords below decrypt it for Gradle / jarsigner.

Keystore file: $KeystorePath
Key alias: $Alias

--- Passwords (treat like any high-value secret) ---
Keystore password: $StorePass
Key password:      $KeyPass
---

GitHub repository secrets (for v* tag builds), names used by this project:
  ANDROID_KEYSTORE_B64         = (paste the single line below, no newlines)
  ANDROID_KEYSTORE_PASSWORD   = $StorePass
  ANDROID_KEY_ALIAS            = $Alias
  ANDROID_KEY_PASSWORD        = $KeyPass

--- Base64 of keystore (for ANDROID_KEYSTORE_B64) ---
$B64
---

Local PowerShell for one-off release build:
  `$env:ANDROID_KEYSTORE_FILE = '$KeystorePath'
  `$env:ANDROID_KEYSTORE_PASSWORD = '$StorePass'
  `$env:ANDROID_KEY_ALIAS = '$Alias'
  `$env:ANDROID_KEY_PASSWORD = '$KeyPass'
  cd <repo>\android; .\gradlew assembleRelease

Keep this directory out of any repository. Optional: right-click E:\Security\AttuneHelperCompanion -> Properties -> Security -> limit to your Windows user.
"@

Set-Content -LiteralPath $CredPath -Value $body -Encoding utf8

Write-Host "Created:"
Write-Host "  $KeystorePath"
Write-Host "  $CredPath"
Write-Host "Do not commit these. Back up the folder to encrypted or offline storage if you ship production builds."
