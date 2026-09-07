# Test-ResourceRefresh.ps1
$ErrorActionPreference='Stop'
$repo=[IO.Path]::GetFullPath((Join-Path $PSScriptRoot '../..'))
$build=Join-Path $repo 'Obj/ResourceRefreshTests'
[IO.Directory]::CreateDirectory($build)|Out-Null
[xml]$game=[IO.File]::ReadAllText((Join-Path $repo 'Game.vcxproj'))
$settings=@($game.Project.ItemDefinitionGroup | Where-Object { $_.Condition -like '*Debug*' })[0]
function Escape([string]$Value) { [Security.SecurityElement]::Escape($Value) }
function Paths([string]$Value) {
    (@($Value.Split(';') | Where-Object { $_ -and !$_.StartsWith('%') } | ForEach-Object {
        [IO.Path]::GetFullPath((Join-Path $repo $_.Replace('$(PlatformTarget)','x64').Replace('$(Configuration)','Debug')))
    }) -join ';')
}
$includes=Escape (Paths $settings.ClCompile.AdditionalIncludeDirectories)
$libraries=Escape (Paths $settings.Link.AdditionalLibraryDirectories)
$objects=@($game.Project.ItemGroup.ClCompile | Where-Object { $_.Include -and $_.Include -notlike '*Bootstrap\Main.cpp' } | ForEach-Object {
    $object=Join-Path $repo ('Obj/Debug/Game/'+[IO.Path]::GetFileNameWithoutExtension($_.Include)+'.obj')
    if(!(Test-Path -LiteralPath $object)){throw "Build Game Debug first: $object"}
    '"'+$object+'"'
})
$dependencies=Escape (($objects -join ';')+';'+$settings.Link.AdditionalDependencies)
$template=@"
<Project DefaultTargets="Build" xmlns="http://schemas.microsoft.com/developer/msbuild/2003">
  <ItemGroup Label="ProjectConfigurations"><ProjectConfiguration Include="Debug|x64"><Configuration>Debug</Configuration><Platform>x64</Platform></ProjectConfiguration></ItemGroup>
  <PropertyGroup Label="Globals"><ProjectGuid>{667DA882-5F4E-4D67-A41E-BA151CAAC39B}</ProjectGuid><WindowsTargetPlatformVersion>10.0</WindowsTargetPlatformVersion></PropertyGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.Default.props" />
  <PropertyGroup Label="Configuration"><ConfigurationType>Application</ConfigurationType><PlatformToolset>v143</PlatformToolset><UseDebugLibraries>true</UseDebugLibraries></PropertyGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.props" />
  <PropertyGroup><OutDir>$(Escape $build)\</OutDir><IntDir>$(Escape $build)\obj\</IntDir><TargetName>ResourceRefreshTest</TargetName></PropertyGroup>
  <ItemDefinitionGroup>
    <ClCompile><LanguageStandard>stdcpp20</LanguageStandard><RuntimeLibrary>MultiThreadedDebug</RuntimeLibrary><PreprocessorDefinitions>_DEBUG;NOMINMAX;UNICODE;_UNICODE</PreprocessorDefinitions><AdditionalIncludeDirectories>$includes</AdditionalIncludeDirectories></ClCompile>
    <Link><SubSystem>Console</SubSystem><AdditionalDependencies>$dependencies;%(AdditionalDependencies)</AdditionalDependencies><AdditionalLibraryDirectories>$libraries</AdditionalLibraryDirectories><AdditionalOptions>/IGNORE:4099 /OPT:REF</AdditionalOptions></Link>
  </ItemDefinitionGroup>
  <ItemGroup><ClCompile Include="$(Escape (Join-Path $PSScriptRoot 'ResourceRefreshTest.cpp'))" /></ItemGroup>
  <Import Project="`$(VCTargetsPath)\Microsoft.Cpp.targets" />
</Project>
"@
$project=Join-Path $build 'ResourceRefreshTest.vcxproj'
[IO.File]::WriteAllText($project,$template,[Text.UTF8Encoding]::new($true))
$start=[Diagnostics.ProcessStartInfo]::new('C:/Program Files/Microsoft Visual Studio/2022/Community/MSBuild/Current/Bin/MSBuild.exe')
$start.UseShellExecute=$false
$start.Environment.Clear()
foreach($entry in [Environment]::GetEnvironmentVariables().GetEnumerator()){ $start.Environment[$entry.Key.ToUpperInvariant()]=$entry.Value }
foreach($argument in @($project,'/p:Configuration=Debug','/p:Platform=x64','/v:minimal','/nologo')){ $start.ArgumentList.Add($argument) }
$process=[Diagnostics.Process]::Start($start)
$process.WaitForExit()
if($process.ExitCode){throw "Test build failed: $($process.ExitCode)"}
$fixture=Join-Path ([IO.Path]::GetTempPath()) ('VVV-ResourceRefresh-'+[guid]::NewGuid())
$start.FileName=Join-Path $build 'ResourceRefreshTest.exe'
$start.ArgumentList.Clear()
$start.ArgumentList.Add($fixture)
$start.Environment['PATH']=(Join-Path $repo 'Bin/Debug')+';'+$env:PATH
$process=[Diagnostics.Process]::Start($start)
$process.WaitForExit()
if($process.ExitCode){throw "Test failed: $($process.ExitCode)"}
Write-Output "Fixtures: $fixture"
