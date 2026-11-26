param(
    [ValidateSet('SplitLifecycleProbe','EditorVisualProbe','GimmickProbe','RecordResultProbe','RuntimeTests','SkinReloadProbe')]
    [string]$Probe = 'SplitLifecycleProbe',
    [switch]$ToolchainReady
)
if(!$ToolchainReady) {
    $vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    $install=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
    $vsdev=Join-Path $install 'Common7\Tools\VsDevCmd.bat'
    $command='call "'+$vsdev+'" -no_logo -arch=x64 -host_arch=x64 && powershell.exe -NoProfile -ExecutionPolicy Bypass -File "'+$PSCommandPath+'" -ToolchainReady -Probe '+$Probe
    & $env:ComSpec /d /c $command
    exit $LASTEXITCODE
}
$ErrorActionPreference='Stop'
$workspace=Split-Path $PSScriptRoot -Parent
$build=Join-Path $PSScriptRoot 'build'
[IO.Directory]::CreateDirectory($build) | Out-Null
[xml]$project=Get-Content (Join-Path $workspace 'FreedomApopicStyle.vcxproj') -Raw
$ns=[Xml.XmlNamespaceManager]::new($project.NameTable)
$ns.AddNamespace('m','http://schemas.microsoft.com/developer/msbuild/2003')
foreach($node in @($project.SelectNodes('//m:ClCompile[@Include]|//m:ClInclude[@Include]|//m:ResourceCompile[@Include]',$ns))) {
    if($node.Include -eq 'Main.cpp' -or $node.LocalName -eq 'ResourceCompile') { $node.ParentNode.RemoveChild($node) | Out-Null }
    else { $node.SetAttribute('Include',(Join-Path $workspace $node.Include)) }
}
$group=$project.CreateElement('ItemGroup',$project.DocumentElement.NamespaceURI)
$source=$project.CreateElement('ClCompile',$project.DocumentElement.NamespaceURI)
$source.SetAttribute('Include',(Join-Path $PSScriptRoot "$Probe.cpp"))
$group.AppendChild($source) | Out-Null
$project.DocumentElement.AppendChild($group) | Out-Null
foreach($node in $project.SelectNodes('//m:SubSystem',$ns)) { $node.InnerText='Console' }
foreach($node in $project.SelectNodes('//m:GenerateDebugInformation',$ns)) { $node.InnerText='false' }
foreach($node in @($project.SelectNodes('//m:Target[@Name="InstallDefaultSkin"]|//m:AdditionalManifestFiles',$ns))) {
    $node.ParentNode.RemoveChild($node) | Out-Null
}
$project.SelectSingleNode('//m:Import[@Project="Build\WindowsDependencies.props"]',$ns).SetAttribute('Project',(Join-Path $workspace 'Build/WindowsDependencies.props'))
$generated=Join-Path $build 'SplitLifecycleProbe.vcxproj'
$project.Save($generated)
$vswhere=Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
$install=& $vswhere -latest -products '*' -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
$msbuild=Join-Path $install 'MSBuild\Current\Bin\MSBuild.exe'
$out=(Join-Path $PSScriptRoot "lifecycle-bin\$Probe") + [IO.Path]::DirectorySeparatorChar
$intermediate=Join-Path $PSScriptRoot 'lifecycle-obj\'
& $msbuild $generated /p:Configuration=Release /p:Platform=x64 /p:RunCodeAnalysis=false "/p:OutDir=$out" "/p:IntDir=$intermediate" "/p:TargetName=$Probe" /m:1 /v:minimal
if($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
Push-Location $PSScriptRoot
try { & (Join-Path $out "$Probe.exe"); exit $LASTEXITCODE }
finally { Pop-Location }