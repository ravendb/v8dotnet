
function BuildV8NetLoader ( $projPath, $buildType ) {
    write-host "Building V8.Net.Loader"
    & dotnet build /p:SourceLinkCreate=true /p:GenerateDocumentationFile=true --configuration $buildType $projPath;
    CheckLastExitCode
}

function BuildV8NetSharedTypes ( $projPath, $buildType ) {
    write-host "Building V8.Net-SharedTypes"
    & dotnet build /p:SourceLinkCreate=true /p:GenerateDocumentationFile=true --configuration $buildType $projPath;
    CheckLastExitCode
}

function BuildV8NetProxyInterface ( $projPath, $buildType ) {
    write-host "Building V8.Net-ProxyInterface"
    & dotnet build /p:SourceLinkCreate=true /p:GenerateDocumentationFile=true --configuration $buildType $projPath;
    CheckLastExitCode
}

function BuildV8DotNet ( $projPath, $buildType ) {
    write-host "Building V8.Net"
    #& dotnet build /p:SourceLinkCreate=true /p:GenerateDocumentationFile=true --no-incremental $buildType $projPath;
    & dotnet build /p:SourceLinkCreate=true /p:GenerateDocumentationFile=true --configuration $buildType $projPath;
    CheckLastExitCode
}

function BuildV8NetTest ( $projPath, $buildType ) {
    write-host "Building test $projPath"
    & dotnet build /p:SourceLinkCreate=true /p:GenerateDocumentationFile=true --configuration $buildType $projPath;
    CheckLastExitCode
}

function BuildV8NetProxy ( $srcPath, $buildType ) {
    write-host "Building V8.Net-Proxy"
    cd $srcPath
    rm -f -d -r build
    #mkdir build
    #cd build
    write-host "----------------------linux64"
    write-host "----------cmake"
    cmake -Bbuild/linux64 -GNinja -DCMAKE_TOOLCHAIN_FILE=./cmake/Toolchain_linux64_l4t.cmake -DCMAKE_BUILD_TYPE="$buildType" -S.
    write-host "----------ninja"
    ninja -C build/linux64

    write-host "----------------------win64"
    write-host "----------cmake"
    #cmake -Bbuild/win64 -GNinja -DCMAKE_TOOLCHAIN_FILE=./cmake/Toolchain_win64_l4t.cmake -DCMAKE_BUILD_TYPE="$buildType" -S.
    write-host "----------ninja"
    ninja -C build/win64

    write-host "----------------------win32"
    write-host "----------cmake"
    #cmake -Bbuild/win32 -GNinja -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain_win32_l4t.cmake -DCMAKE_BUILD_TYPE="$buildType" -S.
    write-host "----------ninja"
    ninja -C build/win32

    write-host "----------------------mac64"
    write-host "----------cmake"
    #cmake -Bbuild/mac64 -GNinja -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain_mac64_l4t.cmake -DCMAKE_BUILD_TYPE="$buildType" -S.
    write-host "----------ninja"
    ninja -C build/mac64

    write-host "----------------------arm64"
    write-host "----------cmake"
    #cmake -Bbuild/arm64 -GNinja -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain_aarch64_l4t.cmake -DCMAKE_BUILD_TYPE="$buildType" -S.
    write-host "----------ninja"
    ninja -C build/arm64

    write-host "----------------------arm32"
    write-host "----------cmake"
    #CFLAGS=-m32 CXXFLAGS=-m32 
    #cmake -Bbuild/arm32 -GNinja -DCMAKE_TOOLCHAIN_FILE=cmake/Toolchain_aarch32_l4t.cmake -DCMAKE_BUILD_TYPE="$buildType" -S.
    write-host "----------ninja"
    ninja -C build/arm32

    write-host "Building V8.Net-Proxy is completed"
    #CheckLastExitCode
}

function NpmInstall () {
    write-host "Doing npm install..."

    foreach ($i in 1..3) {
        try {
            exec { npm install }
            CheckLastExitCode
            return
        }
        catch {
            write-host "Error doing npm install... Retrying."
        }
    }

    throw "npm install failed. Please see error above."
}

