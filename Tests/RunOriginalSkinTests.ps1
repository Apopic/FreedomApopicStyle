$ErrorActionPreference='Stop'
Add-Type -AssemblyName System.Drawing
$root=Split-Path $PSScriptRoot -Parent
$default=Join-Path $root 'Android/app/src/main/assets/Default'
$fixture=Join-Path $PSScriptRoot ('build/fas-original-'+[guid]::NewGuid().ToString('N'))
[IO.Directory]::CreateDirectory($fixture)|Out-Null
$json=Get-Content (Join-Path $default 'skinconfig.json') -Raw|ConvertFrom-Json
function ClearFiles($node){
    if($node -is [Management.Automation.PSCustomObject]){foreach($p in $node.PSObject.Properties){if($p.Name-eq 'FilePath'){$p.Value=''}else{ClearFiles $p.Value}}}
    elseif($node-is [array]){foreach($item in $node){ClearFiles $item}}
}
ClearFiles $json
$json.Info.Name='FAS-original-test'
$pl=$json.Base.Playing
foreach($file in @('ProgressBar.png','KeyMapBox.png')){Copy-Item (Join-Path $default ('Image/Playing/'+$file)) (Join-Path $fixture $file)}
$pl.Image.ProgressBar.FilePath='ProgressBar.png';$pl.Image.KeyMapBox.FilePath='KeyMapBox.png'
$bitmap=[Drawing.Bitmap]::new(64,16);$graphics=[Drawing.Graphics]::FromImage($bitmap)
try{$graphics.Clear([Drawing.Color]::Orange);$bitmap.Save((Join-Path $fixture 'player.png'),[Drawing.Imaging.ImageFormat]::Png)}finally{$graphics.Dispose();$bitmap.Dispose()}
$pl.Image.PlayerChar.FilePath='player.png';$pl.Image.PlayerChar.Div.X=4;$pl.Config.PlayerCharGoGoFrames=@(2,3)
$wav=[IO.MemoryStream]::new();$writer=[IO.BinaryWriter]::new($wav)
$writer.Write([Text.Encoding]::ASCII.GetBytes('RIFF'));$writer.Write([int]196);$writer.Write([Text.Encoding]::ASCII.GetBytes('WAVEfmt '));$writer.Write([int]16);$writer.Write([int16]1);$writer.Write([int16]1);$writer.Write([int]8000);$writer.Write([int]16000);$writer.Write([int16]2);$writer.Write([int16]16);$writer.Write([Text.Encoding]::ASCII.GetBytes('data'));$writer.Write([int]160);$writer.Write([byte[]]::new(160));[IO.File]::WriteAllBytes((Join-Path $fixture 'voice.wav'),$wav.ToArray());$writer.Dispose();$wav.Dispose()
foreach($key in @('DonBig','KaBig','FullComboVoice')){$pl.SE.$key.FilePath='voice.wav'}
$pl.SE.ComboVoice=@(@{Combo=50;FilePath='voice.wav'},@{Combo=100;FilePath='voice.wav'})
[IO.File]::WriteAllText((Join-Path $fixture 'skinconfig.json'),($json|ConvertTo-Json -Depth 40),[Text.UTF8Encoding]::new($false))
$bar=[Drawing.Bitmap]::new((Join-Path $fixture 'ProgressBar.png'))
try{if($bar.Width-ne 700 -or $bar.Height-ne 42){throw 'gauge dimensions'};foreach($y in @(0,20,21,41)){if($bar.GetPixel(524,$y).ToArgb()-eq $bar.GetPixel(525,$y).ToArgb()-or $bar.GetPixel(629,$y).ToArgb()-eq $bar.GetPixel(630,$y).ToArgb()){throw '75/90 percent colour transitions'}}}finally{$bar.Dispose()}
$box=[Drawing.Bitmap]::new((Join-Path $fixture 'KeyMapBox.png'));try{if($box.GetPixel(50,20).ToArgb()-eq $box.GetPixel(50,64).ToArgb()){throw 'pressed box state must differ'}}finally{$box.Dispose()}
& $env:ComSpec /c "`"$PSScriptRoot\RunSkinSwitchProbe.cmd`" `"$fixture`""
if($LASTEXITCODE-ne 0){throw 'original FAS skin resource test failed'}
Write-Output 'PASS: FAS original character/key view, dedicated big sounds, voices, 75/90 percent gauge boundaries'
