#!/bin/sh
# Build the Java 8-compatible ProsperoPZ launcher backend JAR.
set -eu
repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
jdk=${JDK_HOME:-}
out=${1:-"$repo/artifacts/launcher"}
classes="$out/plain-classes"
rm -rf "$classes"
mkdir -p "$classes"
find "$repo/launcher/src" -name '*.java' > "$out/sources.txt"
if [ -n "$jdk" ]; then javac="$jdk/bin/javac"; jar="$jdk/bin/jar"; else javac=javac; jar=jar; fi
"$javac" --release 8 -Xlint:-options -encoding UTF-8 -d "$classes" @"$out/sources.txt"
find "$repo/launcher/src18" -name '*.java' > "$out/modern-sources.txt"
"$javac" --release 18 -encoding UTF-8 -cp "$classes" -d "$classes" @"$out/modern-sources.txt"
# Steam Workshop downloader: Java 17 code on JavaSteam. Its libraries ship
# beside the jar in launcher/workshop and are only loaded by the download run.
libs="$out/workshop"
if ! ls "$libs"/javasteam-*.jar >/dev/null 2>&1; then
  mkdir -p "$libs"
  mvn -q -B -f "$repo/launcher/workshop/pom.xml" dependency:copy-dependencies \
    -DoutputDirectory="$libs" -DincludeScope=runtime
fi
find "$repo/launcher/src17" -name '*.java' > "$out/workshop-sources.txt"
"$javac" --release 17 -encoding UTF-8 -cp "$classes:$libs/*" -d "$classes" @"$out/workshop-sources.txt"
rm -f "$out/prosperopz-launcher.jar"
"$jar" "-J-Djava.io.tmpdir=$out" --create --file "$out/prosperopz-launcher.jar" --manifest "$repo/launcher/agent-manifest.txt" --main-class org.prosperopz.Main -C "$classes" .
"$jar" "-J-Djava.io.tmpdir=$out" --update --file "$out/prosperopz-launcher.jar" -C "$repo/launcher/resources" .
echo "$out/prosperopz-launcher.jar"
