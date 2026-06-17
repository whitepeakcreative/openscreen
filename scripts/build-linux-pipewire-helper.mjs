import { spawnSync } from "node:child_process";
import fs from "node:fs";
import path from "node:path";
import { fileURLToPath } from "node:url";

const __dirname = path.dirname(fileURLToPath(import.meta.url));
const ROOT = path.join(__dirname, "..");
const SOURCE_DIR = path.join(ROOT, "electron", "native", "pipewire-capture");
const BUILD_DIR = path.join(SOURCE_DIR, "build");
const BIN_DIR = path.join(ROOT, "electron", "native", "bin", "linux-x64");
const HELPER_NAME = "openscreen-pipewire-capture";
const CMAKE = process.env.CMAKE_EXE ?? "cmake";

if (process.platform !== "linux") {
	console.log("Skipping PipeWire helper build: host platform is not Linux.");
	process.exit(0);
}

// Check for required build dependencies
const pkgCheck = spawnSync("pkg-config", ["--exists", "glib-2.0", "gio-2.0", "gstreamer-1.0"], {
	encoding: "utf8",
});

if (pkgCheck.status !== 0) {
	console.log("WARNING: Required build dependencies not found (glib-2.0, gio-2.0, gstreamer-1.0).");
	console.log("Install them with:");
	console.log(
		"  Ubuntu/Debian: sudo apt-get install -y libglib2.0-dev libgstreamer1.0-dev libgstreamer1.0-plugins-base-dev",
	);
	console.log(
		"  Fedora: sudo dnf install -y glib2-devel gstreamer1-devel gstreamer1-plugins-base-devel",
	);
	console.log("Skipping PipeWire helper build.");
	process.exit(0);
}

fs.mkdirSync(BUILD_DIR, { recursive: true });

// Configure
const configure = spawnSync(
	CMAKE,
	["-S", SOURCE_DIR, "-B", BUILD_DIR, "-G", "Ninja", "-DCMAKE_BUILD_TYPE=Release"],
	{
		encoding: "utf8",
		stdio: "inherit",
	},
);
if (configure.status !== 0) {
	console.error("ERROR: CMake configure failed");
	process.exit(1);
}

// Build
const build = spawnSync(CMAKE, ["--build", BUILD_DIR, "--config", "Release"], {
	encoding: "utf8",
	stdio: "inherit",
});
if (build.status !== 0) {
	console.error("ERROR: CMake build failed");
	process.exit(1);
}

const outputPath = path.join(BUILD_DIR, HELPER_NAME);
if (!fs.existsSync(outputPath)) {
	throw new Error(`PipeWire helper build completed but ${outputPath} was not found.`);
}

fs.mkdirSync(BIN_DIR, { recursive: true });
const distributablePath = path.join(BIN_DIR, HELPER_NAME);
fs.copyFileSync(outputPath, distributablePath);
fs.chmodSync(distributablePath, 0o755);

console.log(`Built ${outputPath}`);
console.log(`Copied ${distributablePath}`);
