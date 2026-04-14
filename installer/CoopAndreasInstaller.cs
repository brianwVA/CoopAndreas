using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.IO.Compression;
using System.Linq;
using System.Net;
using System.Runtime.InteropServices;
using System.Text;
using System.Threading;
using Microsoft.Win32;

namespace CoopAndreasInstaller
{
    class Program
    {
        // ── Config ──
        const string RepoOwner = "brianwVA";
        const string RepoName = "CoopAndreas";
        const string Branch = "main";
        const string UserAgent = "CoopAndreas-Installer/1.0";

        // ── Colors ──
        static void WriteColor(string text, ConsoleColor color)
        {
            var prev = Console.ForegroundColor;
            Console.ForegroundColor = color;
            Console.Write(text);
            Console.ForegroundColor = prev;
        }
        static void WriteLineColor(string text, ConsoleColor color)
        {
            WriteColor(text + "\n", color);
        }
        static void Info(string msg) { WriteLineColor("[INFO] " + msg, ConsoleColor.Cyan); }
        static void Ok(string msg) { WriteLineColor("[OK]   " + msg, ConsoleColor.Green); }
        static void Err(string msg) { WriteLineColor("[ERR]  " + msg, ConsoleColor.Red); }
        static void Warn(string msg) { WriteLineColor("[WARN] " + msg, ConsoleColor.Yellow); }

        // ── ASCII Banner ──
        static void PrintBanner()
        {
            try { Console.Clear(); } catch { }
            var banner = @"
   ██████╗ ████████╗ █████╗     ███████╗ █████╗      ██████╗ ██████╗  ██████╗ ██████╗ 
  ██╔════╝ ╚══██╔══╝██╔══██╗    ██╔════╝██╔══██╗    ██╔════╝██╔═══██╗██╔═══██╗██╔══██╗
  ██║  ███╗   ██║   ███████║    ███████╗███████║    ██║     ██║   ██║██║   ██║██████╔╝
  ██║   ██║   ██║   ██╔══██║    ╚════██║██╔══██║    ██║     ██║   ██║██║   ██║██╔═══╝ 
  ╚██████╔╝   ██║   ██║  ██║    ███████║██║  ██║    ╚██████╗╚██████╔╝╚██████╔╝██║     
   ╚═════╝    ╚═╝   ╚═╝  ╚═╝    ╚══════╝╚═╝  ╚═╝     ╚═════╝ ╚═════╝  ╚═════╝╚═╝     
                                                                              BY BEWU   
";
            WriteLineColor(banner, ConsoleColor.Yellow);
            WriteLineColor("  ══════════════════════════════════════════════════════════════════", ConsoleColor.DarkYellow);
            WriteLineColor("     CoopAndreas Installer / Updater / Repair", ConsoleColor.White);
            WriteLineColor("  ══════════════════════════════════════════════════════════════════\n", ConsoleColor.DarkYellow);
        }

        // ── Progress bar ──
        static void DrawProgress(string label, long current, long total)
        {
            if (total <= 0) return;
            int barWidth = 40;
            double pct = (double)current / total;
            int filled = (int)(pct * barWidth);
            if (filled > barWidth) filled = barWidth;

            Console.Write("\r  ");
            WriteColor(label, ConsoleColor.Cyan);
            Console.Write(" [");
            WriteColor(new string('█', filled), ConsoleColor.Green);
            Console.Write(new string('░', barWidth - filled));
            Console.Write("] ");
            WriteColor(string.Format("{0,5:F1}%", pct * 100), ConsoleColor.White);
            Console.Write(string.Format("  ({0}/{1} KB)   ", current / 1024, total / 1024));
        }
        static void FinishProgress()
        {
            Console.WriteLine();
        }

        // ── GTA dir resolution ──
        static string ResolveGtaDir(string startDir)
        {
            string current = Path.GetFullPath(startDir);
            for (int i = 0; i < 6; i++)
            {
                if (File.Exists(Path.Combine(current, "gta_sa.exe")))
                    return current;
                string parent = Path.GetDirectoryName(current);
                if (string.IsNullOrEmpty(parent) || parent == current) break;
                current = parent;
            }
            return null;
        }

        // ── Download with progress ──
        static void DownloadFile(string url, string destPath, string label)
        {
            using (var client = new WebClient())
            {
                client.Headers.Add("User-Agent", UserAgent);
                long totalBytes = 0;
                bool gotTotal = false;

                client.DownloadProgressChanged += (s, e) =>
                {
                    if (!gotTotal && e.TotalBytesToReceive > 0)
                    {
                        totalBytes = e.TotalBytesToReceive;
                        gotTotal = true;
                    }
                    DrawProgress(label, e.BytesReceived, gotTotal ? totalBytes : e.BytesReceived * 2);
                };

                var done = new ManualResetEvent(false);
                Exception error = null;

                client.DownloadFileCompleted += (s, e) =>
                {
                    if (e.Error != null) error = e.Error;
                    done.Set();
                };

                client.DownloadFileAsync(new Uri(url), destPath);
                done.WaitOne();
                FinishProgress();

                if (error != null) throw error;
            }
        }

        // ── GitHub API ──
        static string GetRemoteCommitSha()
        {
            string url = string.Format("https://api.github.com/repos/{0}/{1}/commits/{2}", RepoOwner, RepoName, Branch);
            using (var client = new WebClient())
            {
                client.Headers.Add("User-Agent", UserAgent);
                client.Headers.Add("Accept", "application/vnd.github+json");
                string json = client.DownloadString(url);
                // Simple JSON parse for "sha"
                int idx = json.IndexOf("\"sha\"");
                if (idx < 0) return null;
                int start = json.IndexOf('"', idx + 5);
                int end = json.IndexOf('"', start + 1);
                return json.Substring(start + 1, end - start - 1);
            }
        }

        static string GetLocalCommitSha(string gameDir)
        {
            string versionPath = Path.Combine(gameDir, "VERSION.txt");
            if (!File.Exists(versionPath)) return null;
            foreach (var line in File.ReadAllLines(versionPath))
            {
                if (line.StartsWith("commit="))
                    return line.Substring(7).Trim();
            }
            return null;
        }

        // ── Integrity check ──
        struct RequiredFile
        {
            public string RelativePath;
            public bool Critical; // if missing, forces reinstall
            public RequiredFile(string path, bool critical) { RelativePath = path; Critical = critical; }
        }

        static RequiredFile[] GetRequiredFiles()
        {
            return new RequiredFile[]
            {
                new RequiredFile("CoopAndreasSA.dll", true),
                new RequiredFile("server.exe", true),
                new RequiredFile("EAX.dll", true),
                new RequiredFile("proxy.dll", false),
                new RequiredFile("stream.ini", true),
                new RequiredFile("VERSION.txt", true),
                new RequiredFile("dinput8.dll", true),
                new RequiredFile(@"CoopAndreas\main.scm", true),
                new RequiredFile(@"CoopAndreas\main.txt", false),
                new RequiredFile(@"CoopAndreas\script.img", true),
                new RequiredFile(@"CoopAndreas\Launcher\CoopAndreasInstaller.exe", false),
                new RequiredFile(@"CoopAndreas\Launcher\CoopAndreasUninstaller.exe", false),
            };
        }

        static List<string> CheckIntegrity(string gameDir)
        {
            var missing = new List<string>();
            foreach (var f in GetRequiredFiles())
            {
                string fullPath = Path.Combine(gameDir, f.RelativePath);
                if (!File.Exists(fullPath))
                {
                    if (f.Critical)
                        missing.Add(f.RelativePath);
                }
                else if (f.Critical)
                {
                    var fi = new FileInfo(fullPath);
                    if (fi.Length == 0)
                        missing.Add(f.RelativePath + " (pusty plik!)");
                }
            }
            return missing;
        }

        // ── Dependency checks ──
        static bool IsVCRedistInstalled()
        {
            string[] regPaths = {
                @"SOFTWARE\WOW6432Node\Microsoft\VisualStudio\14.0\VC\Runtimes\X86",
                @"SOFTWARE\Microsoft\VisualStudio\14.0\VC\Runtimes\X86"
            };
            foreach (var p in regPaths)
            {
                using (var key = Registry.LocalMachine.OpenSubKey(p))
                {
                    if (key != null)
                    {
                        var val = key.GetValue("Installed");
                        if (val != null && val.ToString() == "1") return true;
                    }
                }
            }
            // Fallback: check DLL
            string sysDir = Environment.Is64BitOperatingSystem
                ? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "SysWOW64")
                : Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "System32");
            return File.Exists(Path.Combine(sysDir, "vcruntime140.dll"));
        }

        static bool IsDirectX9Installed()
        {
            string sysDir = Environment.Is64BitOperatingSystem
                ? Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "SysWOW64")
                : Path.Combine(Environment.GetFolderPath(Environment.SpecialFolder.Windows), "System32");
            return File.Exists(Path.Combine(sysDir, "d3dx9_43.dll"));
        }

        static void InstallVCRedist()
        {
            string url = "https://aka.ms/vs/17/release/vc_redist.x86.exe";
            string tmp = Path.Combine(Path.GetTempPath(), "vc_redist_x86.exe");
            try
            {
                DownloadFile(url, tmp, "VC++ Redist x86");
                Info("Instalowanie VC++ Redistributable (moze wymagac uprawnien admin)...");
                var proc = Process.Start(new ProcessStartInfo
                {
                    FileName = tmp,
                    Arguments = "/install /passive /norestart",
                    UseShellExecute = true
                });
                proc.WaitForExit();
                if (proc.ExitCode == 0 || proc.ExitCode == 3010)
                    Ok("Visual C++ Redistributable zainstalowany.");
                else
                    Warn("VC++ Redist zwrocil kod: " + proc.ExitCode + ". Pobierz recznie: " + url);
            }
            catch (Exception ex)
            {
                Err("Nie udalo sie zainstalowac VC++ Redist: " + ex.Message);
                Info("Pobierz recznie: " + url);
            }
            finally { try { File.Delete(tmp); } catch { } }
        }

        static void InstallDirectX9()
        {
            string url = "https://download.microsoft.com/download/8/4/A/84A35BF1-DAFE-4AE8-82AF-AD2AE20B6B14/directx_Jun2010_redist.exe";
            string tmp = Path.Combine(Path.GetTempPath(), "directx_redist.exe");
            string extractDir = Path.Combine(Path.GetTempPath(), "directx_extract_" + Guid.NewGuid().ToString("N").Substring(0, 8));
            try
            {
                DownloadFile(url, tmp, "DirectX 9 Runtime");
                Info("Rozpakowywanie DirectX Runtime...");
                Directory.CreateDirectory(extractDir);
                var proc1 = Process.Start(new ProcessStartInfo
                {
                    FileName = tmp,
                    Arguments = "/Q /T:" + extractDir,
                    UseShellExecute = false
                });
                proc1.WaitForExit();

                string dxSetup = Path.Combine(extractDir, "DXSETUP.exe");
                if (File.Exists(dxSetup))
                {
                    Info("Instalowanie DirectX 9 Runtime...");
                    var proc2 = Process.Start(new ProcessStartInfo
                    {
                        FileName = dxSetup,
                        Arguments = "/silent",
                        UseShellExecute = true
                    });
                    proc2.WaitForExit();
                    if (proc2.ExitCode == 0)
                        Ok("DirectX 9 Runtime zainstalowany.");
                    else
                        Warn("DirectX Setup zwrocil kod: " + proc2.ExitCode);
                }
            }
            catch (Exception ex)
            {
                Err("Nie udalo sie zainstalowac DirectX 9: " + ex.Message);
                Info("Pobierz recznie: https://www.microsoft.com/en-us/download/details.aspx?id=8109");
            }
            finally
            {
                try { File.Delete(tmp); } catch { }
                try { Directory.Delete(extractDir, true); } catch { }
            }
        }

        // ── Unblock files ──
        [DllImport("kernel32.dll", SetLastError = true)]
        static extern bool DeleteFile(string lpFileName);

        static void UnblockFile(string path)
        {
            // Remove Zone.Identifier ADS (alternate data stream)
            DeleteFile(path + ":Zone.Identifier");
        }

        static void UnblockModFiles(string gameDir)
        {
            string[] files = { "CoopAndreasSA.dll", "EAX.dll", "proxy.dll", "dinput8.dll", "server.exe" };
            foreach (var f in files)
            {
                string path = Path.Combine(gameDir, f);
                if (File.Exists(path))
                    UnblockFile(path);
            }
            // Also scripts dir
            string scriptsDir = Path.Combine(gameDir, "scripts");
            if (Directory.Exists(scriptsDir))
            {
                foreach (var f in Directory.GetFiles(scriptsDir, "*.asi"))
                    UnblockFile(f);
            }
        }

        // ── SAMP detection ──
        static bool IsSampInstalled(string gameDir)
        {
            string[] markers = { "samp.dll", "samp.exe", "samp.saa" };
            return markers.Any(m => File.Exists(Path.Combine(gameDir, m)));
        }

        static void DisableSamp(string gameDir)
        {
            string[] files = { "samp.dll", "samp.exe", "samp.saa" };
            foreach (var f in files)
            {
                string src = Path.Combine(gameDir, f);
                string dst = src + ".coop_disabled";
                if (File.Exists(src) && !File.Exists(dst))
                {
                    File.Move(src, dst);
                    Ok("Wylaczono: " + f);
                }
            }
        }

        // ── EAX proxy setup ──
        static void SetupEaxProxy(string gameDir, string proxyDllSource)
        {
            string eaxOrig = Path.Combine(gameDir, "eax_orig.dll");
            string eaxDll = Path.Combine(gameDir, "EAX.dll");

            // Backup original eax.dll if not done yet
            if (!File.Exists(eaxOrig) && File.Exists(eaxDll))
            {
                var fi = new FileInfo(eaxDll);
                if (fi.Length > 100000) // Original is ~188KB, proxy is ~85KB
                {
                    File.Move(eaxDll, eaxOrig);
                    Ok("Backup oryginalnego eax.dll -> eax_orig.dll");
                }
            }

            // Copy proxy as EAX.dll
            if (File.Exists(proxyDllSource))
            {
                File.Copy(proxyDllSource, eaxDll, true);
                Ok("Zainstalowano proxy.dll jako EAX.dll");
                File.Copy(proxyDllSource, Path.Combine(gameDir, "proxy.dll"), true);
            }
        }

        // ── ASI Loader (from GitHub) ──
        static void EnsureAsiLoader(string gameDir)
        {
            string dst = Path.Combine(gameDir, "dinput8.dll");
            if (File.Exists(dst))
            {
                Ok("ASI Loader (dinput8.dll) juz zainstalowany.");
                return;
            }

            Info("Pobieram Ultimate ASI Loader...");
            string tmpDir = Path.Combine(Path.GetTempPath(), "asi_loader_" + Guid.NewGuid().ToString("N").Substring(0, 8));
            string zipDst = Path.Combine(tmpDir, "asi_loader.zip");
            try
            {
                Directory.CreateDirectory(tmpDir);
                // Try to get from ThirteenAG releases
                string apiUrl = "https://api.github.com/repos/ThirteenAG/Ultimate-ASI-Loader/releases/latest";
                using (var client = new WebClient())
                {
                    client.Headers.Add("User-Agent", UserAgent);
                    client.Headers.Add("Accept", "application/vnd.github+json");
                    string json = client.DownloadString(apiUrl);

                    // Find x86 zip asset URL
                    string assetUrl = null;
                    int searchIdx = 0;
                    while (true)
                    {
                        int urlIdx = json.IndexOf("\"browser_download_url\"", searchIdx);
                        if (urlIdx < 0) break;
                        int start = json.IndexOf('"', urlIdx + 22);
                        int end = json.IndexOf('"', start + 1);
                        string url = json.Substring(start + 1, end - start - 1);
                        if (url.ToLower().Contains("x86") && url.EndsWith(".zip"))
                        {
                            assetUrl = url;
                            break;
                        }
                        searchIdx = end;
                    }

                    if (assetUrl == null)
                    {
                        Warn("Nie znaleziono ASI Loader do pobrania.");
                        return;
                    }

                    DownloadFile(assetUrl, zipDst, "ASI Loader     ");
                    ZipFile.ExtractToDirectory(zipDst, tmpDir);

                    // Find dinput8.dll in extracted files
                    var found = Directory.GetFiles(tmpDir, "dinput8.dll", SearchOption.AllDirectories);
                    if (found.Length > 0)
                    {
                        // Prefer x86 version
                        string best = found.FirstOrDefault(f => f.ToLower().Contains("x86") || f.ToLower().Contains("win32")) ?? found[0];
                        File.Copy(best, dst, true);
                        Ok("Zainstalowano ASI Loader (dinput8.dll).");
                    }
                    else
                    {
                        Warn("Pobrano ASI Loader, ale brak dinput8.dll w archiwum.");
                    }
                }
            }
            catch (Exception ex)
            {
                Warn("Nie udalo sie pobrac ASI Loader: " + ex.Message);
            }
            finally
            {
                try { Directory.Delete(tmpDir, true); } catch { }
            }
        }

        // ── Kill processes ──
        static void KillIfRunning(params string[] names)
        {
            foreach (var name in names)
            {
                foreach (var proc in Process.GetProcessesByName(name))
                {
                    try
                    {
                        Info("Zatrzymuje: " + name + " (PID " + proc.Id + ")");
                        proc.Kill();
                        proc.WaitForExit(3000);
                        Ok("Zatrzymano: " + name);
                    }
                    catch { }
                }
            }
        }

        // ── Copy directory recursively ──
        static void CopyDir(string src, string dst)
        {
            Directory.CreateDirectory(dst);
            foreach (var f in Directory.GetFiles(src))
                File.Copy(f, Path.Combine(dst, Path.GetFileName(f)), true);
            foreach (var d in Directory.GetDirectories(src))
                CopyDir(d, Path.Combine(dst, Path.GetFileName(d)));
        }

        // ── Full mod install from extracted repo ──
        static void InstallMod(string repoRoot, string gameDir)
        {
            Info("Instalacja pelnego moda CoopAndreas...");

            // Find latest release package
            string releaseDir = Path.Combine(repoRoot, "release");
            if (!Directory.Exists(releaseDir))
                throw new Exception("Brak katalogu release w pobranym repo.");

            var packages = Directory.GetDirectories(releaseDir, "old-*")
                .Select(d => new { Path = d, Name = Path.GetFileName(d) })
                .OrderByDescending(d => {
                    string ver = d.Name.Replace("old-", "");
                    Version v;
                    return Version.TryParse(ver, out v) ? v : new Version(0, 0);
                })
                .ToList();

            if (packages.Count == 0)
                throw new Exception("Brak paczki release w repo.");

            string pkgDir = packages[0].Path;
            string channelName = packages[0].Name;
            Info("Paczka: " + channelName);

            // Core files from release package
            string[] coreFiles = { "CoopAndreasSA.dll", "server.exe", "proxy.dll", "VERSION.txt" };
            int installed = 0;
            foreach (var f in coreFiles)
            {
                string src = Path.Combine(pkgDir, f);
                if (File.Exists(src))
                {
                    File.Copy(src, Path.Combine(gameDir, f), true);
                    installed++;
                    Ok("Zainstalowano: " + f);
                }
            }

            // SCM / mission scripts
            string coopDir = Path.Combine(gameDir, "CoopAndreas");
            Directory.CreateDirectory(coopDir);

            string scmDir = Path.Combine(repoRoot, "scm");
            if (Directory.Exists(scmDir))
            {
                string[] scmFiles = { "main.scm", "main.txt", "script.img" };
                foreach (var f in scmFiles)
                {
                    string src = Path.Combine(scmDir, f);
                    if (File.Exists(src))
                    {
                        File.Copy(src, Path.Combine(coopDir, f), true);
                        installed++;
                        Ok("Zainstalowano: CoopAndreas\\" + f);
                    }
                }
            }

            // Proxy -> EAX.dll setup
            string proxyFile = Path.Combine(pkgDir, "proxy.dll");
            SetupEaxProxy(gameDir, proxyFile);

            // stream.ini (always update)
            string streamSrc = Path.Combine(repoRoot, "stream.ini");
            if (File.Exists(streamSrc))
            {
                File.Copy(streamSrc, Path.Combine(gameDir, "stream.ini"), true);
                Ok("Zainstalowano: stream.ini");
            }

            // server-config.ini (only if missing)
            string srvCfgDst = Path.Combine(gameDir, "server-config.ini");
            if (!File.Exists(srvCfgDst))
            {
                string srvCfgSrc = Path.Combine(repoRoot, "server-config.ini");
                if (File.Exists(srvCfgSrc))
                {
                    File.Copy(srvCfgSrc, srvCfgDst, false);
                    Ok("Zainstalowano: server-config.ini");
                }
            }

            // Launcher files
            string launcherDir = Path.Combine(coopDir, "Launcher");
            Directory.CreateDirectory(launcherDir);

            // Copy installer/uninstaller EXEs if present in repo
            string installerDir = Path.Combine(repoRoot, "release", "launcher");
            if (Directory.Exists(installerDir))
            {
                foreach (var f in Directory.GetFiles(installerDir))
                {
                    string dst = Path.Combine(launcherDir, Path.GetFileName(f));
                    File.Copy(f, dst, true);
                }
                Ok("Zaktualizowano pliki launchera.");
            }

            // Copy self to Launcher dir
            string selfPath = System.Reflection.Assembly.GetExecutingAssembly().Location;
            if (!string.IsNullOrEmpty(selfPath) && File.Exists(selfPath))
            {
                string selfDst = Path.Combine(launcherDir, Path.GetFileName(selfPath));
                if (!string.Equals(Path.GetFullPath(selfPath), Path.GetFullPath(selfDst), StringComparison.OrdinalIgnoreCase))
                {
                    File.Copy(selfPath, selfDst, true);
                }
            }

            Ok(string.Format("Zainstalowano {0} plikow moda.", installed));
        }

        // ── Write VERSION.txt ──
        static void WriteVersionFile(string gameDir, string commitSha, string channelName)
        {
            string path = Path.Combine(gameDir, "VERSION.txt");
            File.WriteAllText(path, string.Format("channel={0}\ncommit={1}\nsource_branch=main\n", channelName, commitSha), Encoding.UTF8);
        }

        // ── Write BAT launchers ──
        static void WriteLaunchers(string gameDir)
        {
            string serverBat = Path.Combine(gameDir, "Uruchom CoopAndreas Server.bat");
            File.WriteAllText(serverBat, @"@echo off
setlocal
set ""GTA_DIR=%~dp0""
set ""SERVER_EXE=%GTA_DIR%server.exe""
if not exist ""%SERVER_EXE%"" (
  echo [BLAD] Nie znaleziono: ""%SERVER_EXE%""
  pause
  exit /b 1
)
cd /d ""%GTA_DIR%""
echo [INFO] Uruchamiam serwer CoopAndreas...
start ""CoopAndreas Server"" cmd /k ""%SERVER_EXE%""
exit /b 0
", Encoding.ASCII);

            string runBat = Path.Combine(gameDir, "Uruchom CoopAndreas.bat");
            File.WriteAllText(runBat, @"@echo off
setlocal
set ""LAUNCHER_DIR=%~dp0CoopAndreas\Launcher""
if exist ""%LAUNCHER_DIR%\CoopAndreasInstaller.exe"" (
  ""%LAUNCHER_DIR%\CoopAndreasInstaller.exe"" --run-after
) else (
  echo [INFO] Uruchamiam gre...
  start """" ""%~dp0gta_sa.exe""
)
exit /b 0
", Encoding.ASCII);

            string uninstBat = Path.Combine(gameDir, "Odinstaluj CoopAndreas.bat");
            File.WriteAllText(uninstBat, @"@echo off
setlocal
set ""LAUNCHER_DIR=%~dp0CoopAndreas\Launcher""
if exist ""%LAUNCHER_DIR%\CoopAndreasUninstaller.exe"" (
  ""%LAUNCHER_DIR%\CoopAndreasUninstaller.exe""
) else (
  echo [BLAD] Brak CoopAndreasUninstaller.exe
  pause
)
exit /b 0
", Encoding.ASCII);

            Ok("Launchery BAT odswiezone.");
        }

        // ── Main ──
        static int Main(string[] args)
        {
            // TLS 1.2 for GitHub
            ServicePointManager.SecurityProtocol = SecurityProtocolType.Tls12;
            try { Console.OutputEncoding = Encoding.UTF8; } catch { }

            bool runAfter = args.Contains("--run-after");
            bool forceReinstall = args.Contains("--repair");

            try
            {
                PrintBanner();

                // Resolve GTA dir
                string startDir = Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location);
                string gtaDir = ResolveGtaDir(startDir);
                if (gtaDir == null)
                {
                    Err("Nie znaleziono gta_sa.exe!");
                    Err("Umiesc installer w katalogu GTA San Andreas lub podkatalogu.");
                    Console.ReadKey();
                    return 1;
                }

                Info("Katalog GTA: " + gtaDir);

                // SAMP check
                if (IsSampInstalled(gtaDir))
                {
                    Warn("Wykryto SA:MP — moze kolidowac z CoopAndreas.");
                    Console.Write("  Wylaczac SA:MP? [T/N]: ");
                    var key = Console.ReadKey();
                    Console.WriteLine();
                    if (key.KeyChar == 't' || key.KeyChar == 'T')
                        DisableSamp(gtaDir);
                }

                // Check for updates via commit SHA
                string localSha = GetLocalCommitSha(gtaDir);
                string remoteSha = null;
                bool needsUpdate = true;

                try
                {
                    Info("Sprawdzam najnowsza wersje na GitHub...");
                    remoteSha = GetRemoteCommitSha();
                    string shortLocal = localSha != null ? localSha.Substring(0, 7) : "brak";
                    string shortRemote = remoteSha.Substring(0, 7);
                    Info("Zainstalowany: " + shortLocal);
                    Info("Najnowszy:     " + shortRemote);

                    if (localSha == remoteSha && !forceReinstall)
                    {
                        needsUpdate = false;
                        Ok("Masz najnowsza wersje!");
                    }
                    else if (localSha != remoteSha)
                    {
                        Info("Dostepna nowa wersja — aktualizacja...");
                    }
                }
                catch (Exception ex)
                {
                    Warn("Nie udalo sie sprawdzic wersji: " + ex.Message);
                    remoteSha = "unknown";
                }

                // Integrity check (even if version matches)
                var missing = CheckIntegrity(gtaDir);
                if (missing.Count > 0)
                {
                    Err("Brakujace pliki (" + missing.Count + "):");
                    foreach (var m in missing)
                        WriteLineColor("   - " + m, ConsoleColor.Red);
                    needsUpdate = true;
                }
                else if (!needsUpdate)
                {
                    Ok("Wszystkie pliki moda sa kompletne.");
                }

                if (!needsUpdate && !forceReinstall)
                {
                    WriteLaunchers(gtaDir);
                    if (runAfter)
                        LaunchGame(gtaDir);
                    Console.WriteLine();
                    Ok("Brak zmian do zainstalowania. Nacisnij dowolny klawisz...");
                    Console.ReadKey();
                    return 0;
                }

                // Kill running processes
                KillIfRunning("server", "gta_sa");

                // Download repo ZIP
                string tempDir = Path.Combine(Path.GetTempPath(), "coopandreas_" + Guid.NewGuid().ToString("N").Substring(0, 8));
                string zipPath = Path.Combine(tempDir, "repo.zip");
                Directory.CreateDirectory(tempDir);

                try
                {
                    string zipUrl = string.Format("https://codeload.github.com/{0}/{1}/zip/refs/heads/{2}", RepoOwner, RepoName, Branch);
                    DownloadFile(zipUrl, zipPath, "Repo ZIP       ");

                    Info("Rozpakowywanie...");
                    string extractDir = Path.Combine(tempDir, "extract");
                    ZipFile.ExtractToDirectory(zipPath, extractDir);

                    string repoRoot = Directory.GetDirectories(extractDir)
                        .FirstOrDefault(d => Path.GetFileName(d).StartsWith(RepoName));
                    if (repoRoot == null)
                        throw new Exception("Nie znaleziono katalogu repo po rozpakowaniu.");

                    // Backup
                    string backupDir = Path.Combine(gtaDir, "CoopAndreas_backup_" + DateTime.Now.ToString("yyyyMMdd_HHmmss"));
                    Directory.CreateDirectory(backupDir);
                    string[] backupFiles = { "CoopAndreasSA.dll", "server.exe", "proxy.dll", "VERSION.txt", "EAX.dll" };
                    int backed = 0;
                    foreach (var f in backupFiles)
                    {
                        string src = Path.Combine(gtaDir, f);
                        if (File.Exists(src))
                        {
                            File.Copy(src, Path.Combine(backupDir, f), true);
                            backed++;
                        }
                    }
                    if (backed > 0) Ok("Backup: " + Path.GetFileName(backupDir));

                    // Install mod
                    InstallMod(repoRoot, gtaDir);

                    // ASI Loader
                    EnsureAsiLoader(gtaDir);

                    // Unblock all files
                    Info("Odblokowywanie plikow...");
                    UnblockModFiles(gtaDir);
                    Ok("Pliki odblokowane.");

                    // Dependencies
                    Console.WriteLine();
                    Info("Sprawdzam zależności systemowe...");
                    if (!IsVCRedistInstalled())
                    {
                        Err("Brak Visual C++ Redistributable x86!");
                        InstallVCRedist();
                    }
                    else Ok("Visual C++ Redistributable x86 — OK");

                    if (!IsDirectX9Installed())
                    {
                        Err("Brak DirectX 9 Runtime (d3dx9_43.dll)!");
                        InstallDirectX9();
                    }
                    else Ok("DirectX 9 Runtime — OK");

                    // Write VERSION.txt
                    var packages = Directory.GetDirectories(Path.Combine(repoRoot, "release"), "old-*")
                        .Select(d => Path.GetFileName(d))
                        .OrderByDescending(n => {
                            string ver = n.Replace("old-", "");
                            Version v;
                            return Version.TryParse(ver, out v) ? v : new Version(0, 0);
                        })
                        .ToList();
                    string channelName = packages.Count > 0 ? packages[0] : "unknown";
                    WriteVersionFile(gtaDir, remoteSha, channelName);

                    // BAT launchers
                    WriteLaunchers(gtaDir);

                    // Final integrity check
                    Console.WriteLine();
                    Info("Weryfikacja instalacji...");
                    var postMissing = CheckIntegrity(gtaDir);
                    if (postMissing.Count > 0)
                    {
                        Warn("Nadal brakuje " + postMissing.Count + " plikow:");
                        foreach (var m in postMissing)
                            WriteLineColor("   - " + m, ConsoleColor.Yellow);
                    }
                    else
                    {
                        Ok("Wszystkie pliki zweryfikowane — instalacja kompletna!");
                    }

                    Console.WriteLine();
                    WriteLineColor("  ══════════════════════════════════════════════════════════", ConsoleColor.Green);
                    WriteLineColor("     Instalacja CoopAndreas zakonczona pomyslnie!", ConsoleColor.Green);
                    WriteLineColor("  ══════════════════════════════════════════════════════════", ConsoleColor.Green);
                }
                finally
                {
                    try { Directory.Delete(tempDir, true); } catch { }
                }

                if (runAfter)
                    LaunchGame(gtaDir);

                Console.WriteLine();
                Ok("Nacisnij dowolny klawisz aby zamknac...");
                Console.ReadKey();
                return 0;
            }
            catch (Exception ex)
            {
                Err("Blad krytyczny: " + ex.Message);
                Console.ReadKey();
                return 1;
            }
        }

        static void LaunchGame(string gtaDir)
        {
            Console.WriteLine();
            string serverExe = Path.Combine(gtaDir, "server.exe");
            string gtaExe = Path.Combine(gtaDir, "gta_sa.exe");

            if (File.Exists(serverExe))
            {
                Info("Uruchamiam serwer...");
                Process.Start(new ProcessStartInfo { FileName = serverExe, WorkingDirectory = gtaDir });
                Thread.Sleep(1000);
            }

            if (File.Exists(gtaExe))
            {
                Info("Uruchamiam GTA San Andreas...");
                Process.Start(new ProcessStartInfo { FileName = gtaExe, WorkingDirectory = gtaDir });
            }
        }
    }
}
