"""End-to-end tests of the Project Zomboid backend with a synthetic game install.

The fixture game has the same layout as a Windows Project Zomboid folder
(ProjectZomboid64.json, media/, java/zombie, DLLs and jre64) but contains no
game code; no game is executed.
"""
import io
import json
import os
from pathlib import Path
import shlex
import shutil
import struct
import subprocess
import tempfile
import unittest
import zipfile

REPO = Path(__file__).resolve().parents[2]
JDK = Path(os.environ.get("JAVA_HOME", str(REPO / ".deps/host-jdk/jdk-21.0.8+9")))
JAVA = Path(os.environ["JAVA"]) if "JAVA" in os.environ else next(
    (p for p in (JDK / "bin/java.exe", JDK / "bin/java") if p.is_file()),
    Path(shutil.which("java") or "java"))
JAVAC = next((p for p in (JDK / "bin/javac.exe", JDK / "bin/javac") if p.is_file()),
             Path(shutil.which("javac") or "javac"))
JAR = REPO / "artifacts/launcher/prosperopz-launcher.jar"

B41_DESCRIPTOR = """{
\t"mainClass": "zombie/gameStates/MainScreenState",
\t"classpath": [
\t\t".",
\t\t"istack-commons-runtime.jar",
\t\t"lwjgl.jar",
\t],
\t"vmArgs": [
\t\t"-Djava.awt.headless=true",
\t\t"-Xmx3072m",
\t\t"-Dzomboid.steam=1",
\t\t"-Dzomboid.znetlog=1",
\t\t"-Djava.library.path=win64/;.",
\t\t"-Djava.security.egd=file:/dev/urandom",
\t\t"-XX:+UseZGC",
\t\t"-XX:-OmitStackTraceInFastThrow"
\t]
}
"""


def pe64(machine=0x8664):
    """Minimal PE header fixture; never executes as a host program."""
    data = bytearray(512)
    data[0:2] = b"MZ"
    struct.pack_into("<I", data, 60, 64)
    data[64:68] = b"PE\0\0"
    struct.pack_into("<H", data, 68, machine)
    struct.pack_into("<H", data, 84, 240)
    struct.pack_into("<H", data, 88, 0x20B)
    return bytes(data)


def props(path):
    return dict(line.split("=", 1) for line in path.read_text(encoding="utf-8").splitlines()
                if line and not line.startswith("#"))


def fake_game(folder, build=41, jre=True):
    folder.mkdir(parents=True)
    # Build 41 ships game classes in the folder and a bare "." classpath entry;
    # Build 42 adds projectzomboid.jar. Both list "." first.
    (folder / "ProjectZomboid64.json").write_text(
        B41_DESCRIPTOR if build == 41 else B41_DESCRIPTOR.replace('"lwjgl.jar",', '"projectzomboid.jar",\n\t\t"lwjgl.jar",'),
        encoding="utf-8")
    (folder / "media/lua").mkdir(parents=True)
    (folder / "zombie/gameStates").mkdir(parents=True)        # classes in the game root
    (folder / "lwjgl.jar").write_bytes(b"PK\5\6" + bytes(18))  # a real classpath jar
    if build != 41:
        (folder / "projectzomboid.jar").write_bytes(b"PK\5\6" + bytes(18))
    (folder / "win64").mkdir()
    (folder / "win64/lwjgl64.dll").write_bytes(pe64())
    if jre:
        (folder / "jre64/bin/server").mkdir(parents=True)
        (folder / "jre64/release").write_text('JAVA_VERSION="17.0.1"\nOS_NAME="Windows"\nOS_ARCH="amd64"\n')
        (folder / "jre64/bin/java.exe").write_bytes(pe64())
        (folder / "jre64/bin/server/jvm.dll").write_bytes(pe64())


def zipped(entries):
    out = io.BytesIO()
    with zipfile.ZipFile(out, "w") as z:
        for name, content in entries.items():
            z.writestr(name, content)
    return out.getvalue()


class LauncherTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        assert JAR.is_file(), "Build the launcher (scripts/build-launcher.sh) before running tests"

    def setUp(self):
        (REPO / "artifacts").mkdir(exist_ok=True)
        self.temporary = tempfile.TemporaryDirectory(dir=REPO / "artifacts", prefix="pz-launcher-")
        self.root = Path(self.temporary.name) / "data"
        self.root.mkdir()
        (self.root / "launcher").mkdir()
        shutil.copy(JAR, self.root / "launcher/prosperopz-launcher.jar")

    def tearDown(self):
        self.temporary.cleanup()

    def run_request(self, expect_ok=True, **fields):
        request = self.root.parent / "request.properties"
        request.write_text("".join(f"{k}={v}\n" for k, v in fields.items()), encoding="utf-8")
        result = subprocess.run([str(JAVA), "-jar", str(JAR), "--root", str(self.root), "--request", str(request)],
                                capture_output=True, text=True, timeout=120)
        response = props(self.root / "response.properties")
        self.assertEqual(response["ok"], "true" if expect_ok else "false",
                         response.get("message", "") + result.stdout + result.stderr)
        return response, props(self.root / "launcher-view.properties")

    def import_game(self, build=41, jre=True):
        fake_game(self.root / "imports/ProjectZomboid", build, jre)
        _, view = self.run_request(command="catalog")
        self.assertEqual(view["source.count"], "1")
        response, view = self.run_request(command="import-game", source=view["source.0.path"])
        return response, view

    def test_import_creates_first_profile_and_moves_files(self):
        response, view = self.import_game()
        self.assertIn("first profile", response["message"])
        self.assertFalse((self.root / "imports/ProjectZomboid").exists(), "FTP upload should be moved, not copied")
        self.assertEqual(view["game.count"], "1")
        self.assertEqual(view["game.0.id"], "build41")
        self.assertEqual(view["game.0.build"], "41")
        self.assertEqual(view["game.0.valid"], "true")
        self.assertEqual(view["game.0.java"], "17")
        self.assertEqual(view["instance.count"], "1")
        self.assertEqual(view["instance.0.loader"], "client")
        self.assertEqual(view["instance.0.installed"], "true")

    def test_incomplete_game_is_refused(self):
        folder = self.root / "imports/Broken"
        fake_game(folder)
        shutil.rmtree(folder / "media")
        _, view = self.run_request(command="catalog")
        response, _ = self.run_request(False, command="import-game", source=view["source.0.path"])
        self.assertIn("media folder missing", response["message"])

    def test_client_launch_plan_uses_ps5_jvm_options(self):
        self.import_game()
        self.run_request(command="settings", memoryMb="6144", resolution="1920x1080", debugMode="on")
        response, _ = self.run_request(command="prepare-launch", id="survivor")
        self.assertIn("Java 17", response["message"])
        profile = (self.root / "profiles/launch.profile").read_text()
        self.assertIn("id=zomboid", profile)
        self.assertIn("prefix=zomboid", profile)
        self.assertIn("graphics=zink", profile)
        self.assertIn("desktop=1920x1080", profile)
        self.assertIn("mode=xinput", profile)
        self.assertIn("jre64\\bin\\java.exe", profile)
        arguments = next(line for line in profile.splitlines() if line.startswith("arguments="))
        for wanted in ('"-Xmx6144m"', '"-XX:+UseParallelGC"', '"-XX:-UseCompressedOops"', '"-Dzomboid.steam=0"',
                       '"-Dzomboid.znetlog=1"', '"-XX:-OmitStackTraceInFastThrow"', '"org.prosperopz.Main"',
                       "prosperopz-launcher.jar"):
            self.assertIn(wanted, arguments)
        for unwanted in ("UseZGC", "-Xmx3072m", "zomboid.steam=1", '"-Djava.library.path=win64/;."'):
            self.assertNotIn(unwanted, arguments)
        self.assertIn("win64", arguments)
        plan = json.loads((self.root / "profiles/launch-plan.json").read_text())
        self.assertEqual(plan["mainClass"], "zombie.gameStates.MainScreenState")
        self.assertEqual(plan["arguments"][0], "-nosteam")
        self.assertTrue(plan["arguments"][1].startswith("-cachedir="))
        self.assertIn("-debug", plan["arguments"])
        options = (self.root / "instances/survivor/Zomboid/options.ini").read_text()
        self.assertIn("width=1920", options)
        self.assertIn("fullScreen=true", options)

    def test_existing_options_are_preserved(self):
        self.import_game()
        options = self.root / "instances/survivor/Zomboid/options.ini"
        options.write_text("version=7\nwidth=800\nheight=600\nfullScreen=false\nmusicVolume=3\n")
        self.run_request(command="prepare-launch", id="survivor")
        text = options.read_text()
        self.assertIn("musicVolume=3", text)
        self.assertIn("version=7", text)
        self.assertIn("width=1920", text)
        self.assertNotIn("width=800", text)
        self.assertEqual(text.count("width="), 1)

    def test_missing_bundled_jre_requires_runtime(self):
        self.import_game(jre=False)
        self.run_request(command="settings", javaApi="http://127.0.0.1:9/unreachable")
        response, _ = self.run_request(False, command="prepare-launch", id="survivor")
        self.assertIn("Java", response["message"])

    def test_dedicated_server_profile(self):
        self.import_game()
        response, view = self.run_request(command="create", mode="server", name="PS5 Server")
        index = next(i for i in range(int(view["instance.count"])) if view[f"instance.{i}.loader"] == "server")
        server_id = view[f"instance.{index}.id"]
        password = view[f"instance.{index}.password"]
        self.assertRegex(password, r"^[a-z0-9]{10}$")
        self.run_request(command="prepare-launch", id=server_id)
        plan = json.loads((self.root / "profiles/launch-plan.json").read_text())
        self.assertEqual(plan["mainClass"], "zombie.network.GameServer")
        self.assertIn("-servername", plan["arguments"])
        self.assertIn(password, plan["arguments"])
        profile = (self.root / "profiles/launch.profile").read_text()
        self.assertIn("-Djava.awt.headless=true", profile)
        self.assertNotIn(password, profile, "admin password must stay in the private plan")
        ini = (self.root / f"instances/{server_id}/Zomboid/Server/{view[f'instance.{index}.server']}.ini").read_text()
        self.assertIn("PauseEmpty=true", ini)
        self.assertIn("Map=Muldraugh, KY", ini)
        self.run_request(command="server-password", id=server_id)
        _, view = self.run_request(command="view")
        self.assertNotEqual(view[f"instance.{index}.password"], password)

    def test_mods_import_toggle_dependencies_and_server_sync(self):
        self.import_game()
        mods = self.root / "imports/mods"
        mods.mkdir(parents=True)
        # Steam workshop layout: ITEM/mods/NAME/mod.info
        (mods / "2392709985.zip").write_bytes(zipped({
            "2392709985/mods/tsarslib/mod.info": "name=Tsar Common Library\nid=tsarslib\n",
            "2392709985/mods/tsarslib/media/lua/shared/a.lua": "",
            "2392709985/mods/Bedford Falls/mod.info": "name=Bedford Falls\nid=BedfordFalls\nrequire=tsarslib\nposter=poster.png\n",
            "2392709985/mods/Bedford Falls/media/maps/Bedford Falls, KY/map.info": "title=Bedford\n",
        }))
        response, view = self.run_request(command="content-import", id="survivor", content_kind="mods",
                                          source=str(mods / "2392709985.zip"))
        self.assertIn("Imported 2 mod folders", response["message"])
        self.assertEqual(view["mod.count"], "2")
        ids = {view[f"mod.{i}.name"]: view[f"mod.{i}.enabled"] for i in range(2)}
        self.assertEqual(ids, {"BedfordFalls": "false", "tsarslib": "false"})
        response, view = self.run_request(command="content-toggle", id="survivor", content_kind="mods",
                                          mod="BedfordFalls", enabled="true")
        default = (self.root / "instances/survivor/Zomboid/mods/default.txt").read_text()
        self.assertLess(default.index("mod = tsarslib,"), default.index("mod = BedfordFalls,"))
        self.assertIn("maps\n{", default)
        self.run_request(command="content-toggle", id="survivor", content_kind="mods", mod="BedfordFalls",
                         enabled="false")
        default = (self.root / "instances/survivor/Zomboid/mods/default.txt").read_text()
        self.assertNotIn("BedfordFalls", default)
        self.assertIn("tsarslib", default)
        # The same mods feed a dedicated server's ini.
        response, view = self.run_request(command="create", id="srv", mode="server")
        shutil.copytree(self.root / "instances/survivor/Zomboid/mods", self.root / "instances/srv/Zomboid/mods",
                        dirs_exist_ok=True)
        self.run_request(command="content-toggle", id="srv", content_kind="mods", mod="BedfordFalls", enabled="true")
        self.run_request(command="prepare-launch", id="srv")
        ini = (self.root / "instances/srv/Zomboid/Server/srv.ini").read_text()
        self.assertIn("Mods=tsarslib;BedfordFalls", ini)
        self.assertIn("Map=Bedford Falls, KY;Muldraugh, KY", ini)

    def test_build42_mod_layout_and_prefix(self):
        self.import_game(build=42)
        mods = self.root / "imports/mods/ModernMod"
        (mods / "42").mkdir(parents=True)
        (mods / "common").mkdir()
        (mods / "42/mod.info").write_text("name=Modern\nid=ModernMod\n")
        self.run_request(command="content-import", id="survivor", content_kind="mods", source=str(mods))
        self.run_request(command="content-toggle", id="survivor", content_kind="mods", mod="ModernMod", enabled="true")
        default = (self.root / "instances/survivor/Zomboid/mods/default.txt").read_text()
        self.assertIn("mod = \\ModernMod,", default)
        _, view = self.run_request(command="content-list", id="survivor", content_kind="mods")
        self.assertEqual(view["mod.0.enabled"], "true")

    def test_saves_backup_delete_and_restore(self):
        self.import_game()
        world = self.root / "instances/survivor/Zomboid/Saves/Sandbox/My World"
        world.mkdir(parents=True)
        (world / "map_ver.bin").write_bytes(b"\0\0\0\xc3")
        (world / "map_1_1.bin").write_bytes(b"x" * 100)
        _, view = self.run_request(command="content-list", id="survivor", content_kind="saves")
        self.assertEqual(view["mod.count"], "1")
        self.assertEqual(view["mod.0.name"], "Sandbox/My World")
        response, _ = self.run_request(command="content-backup", id="survivor", content_kind="saves",
                                       mod="Sandbox/My World")
        backups = list((self.root / "backups/survivor").glob("*.zip"))
        self.assertEqual(len(backups), 1)
        with zipfile.ZipFile(backups[0]) as z:
            self.assertIn("Sandbox/My World/map_ver.bin", z.namelist())
        self.run_request(command="content-delete", id="survivor", content_kind="saves", mod="Sandbox/My World")
        self.assertFalse(world.exists())
        (self.root / "imports/saves").mkdir(parents=True)
        shutil.copy(backups[0], self.root / "imports/saves/restore.zip")
        response, view = self.run_request(command="content-import", id="survivor", content_kind="saves",
                                          source=str(self.root / "imports/saves/restore.zip"))
        self.assertIn("Sandbox/My_World", response["message"])
        self.assertTrue((self.root / "instances/survivor/Zomboid/Saves/Sandbox/My_World/map_ver.bin").is_file())

    def test_settings_validation_and_merge(self):
        self.run_request(command="settings", themeColor="blood", memoryMb="8192", controllerMode="emulated")
        self.run_request(False, command="settings", themeColor="grass")
        self.run_request(False, command="settings", graphics="vulkan")
        self.run_request(False, command="settings", memoryMb="256")
        (self.root / "settings.properties").write_text(
            (self.root / "settings.properties").read_text() + "vrrMode=stable\n")
        _, view = self.run_request(command="settings", resolution="2560x1440")
        self.assertEqual(view["settings.themeColor"], "blood")
        self.assertEqual(view["settings.memoryMb"], "8192")
        self.assertEqual(view["settings.vrrMode"], "stable")
        self.assertEqual(view["settings.resolution"], "2560x1440")

    def test_emulated_controller_bindings(self):
        self.import_game()
        self.run_request(command="settings", controllerMode="emulated")
        self.run_request(command="prepare-launch", id="survivor")
        profile = (self.root / "profiles/launch.profile").read_text()
        self.assertIn("mode=keyboard", profile)
        self.assertIn("r2=mouse_left", profile)
        self.assertIn("options=escape", profile)

    def test_delete_profile_keeps_worlds_in_backups(self):
        self.import_game()
        (self.root / "instances/survivor/Zomboid/Saves/Survival/W").mkdir(parents=True)
        self.run_request(command="delete-instance", id="survivor")
        self.assertFalse((self.root / "instances/survivor").exists())
        kept = list((self.root / "backups").glob("profile-survivor-*/Zomboid/Saves/Survival/W"))
        self.assertEqual(len(kept), 1)

    def test_game_in_use_cannot_be_deleted(self):
        self.import_game()
        response, _ = self.run_request(False, command="delete-game", game="build41")
        self.assertIn("still uses this game", response["message"])

    def test_generated_jvm_flags_start_a_real_jvm_and_plan_runs_game_main(self):
        self.import_game()
        self.run_request(command="prepare-launch", id="survivor")
        profile = (self.root / "profiles/launch.profile").read_text()
        line = next(l for l in profile.splitlines() if l.startswith("arguments="))[len("arguments="):]
        words = shlex.split(line)
        flags = words[:words.index("-cp")]
        flags = [f for f in flags if not f.startswith("-Djava.library.path=")]
        result = subprocess.run([str(JAVA), *flags, "-version"], capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        self.assertNotIn("Unrecognized", result.stderr)
        # A stub game main records the arguments the plan passes it.
        classes = self.root.parent / "game-classes"
        source = self.root.parent / "MainScreenState.java"
        source.write_text("package zombie.gameStates; public class MainScreenState { public static void main(String[] a)"
                          " throws Exception { java.nio.file.Files.write(java.nio.file.Paths.get(System.getProperty(\"out\")),"
                          " String.join(\"\\n\", a).getBytes()); } }")
        subprocess.run([str(JAVAC), "-d", str(classes), str(source)], check=True, capture_output=True)
        out = self.root.parent / "args.txt"
        result = subprocess.run([str(JAVA), f"-Dout={out}", "-cp", os.pathsep.join([str(classes), str(JAR)]),
                                 "org.prosperopz.Main", "--root", str(self.root), "--execute-plan",
                                 str(self.root / "profiles/launch-plan.json")], capture_output=True, text=True, timeout=60)
        self.assertEqual(result.returncode, 0, result.stdout + result.stderr)
        received = out.read_text().splitlines()
        self.assertEqual(received[0], "-nosteam")
        self.assertTrue(received[1].startswith("-cachedir="))

    def test_path_escape_is_refused(self):
        self.import_game()
        response, _ = self.run_request(False, command="content-import", id="survivor", content_kind="mods",
                                       source=str(self.root.parent))
        response, _ = self.run_request(False, command="content-delete", id="survivor", content_kind="saves",
                                       mod="../../launcher")
        response, _ = self.run_request(False, command="prepare-launch", id="../x")


if __name__ == "__main__":
    unittest.main()
