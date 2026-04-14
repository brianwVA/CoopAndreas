using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.IO;
using System.Linq;
using System.Text;

namespace CoopAndreasUninstaller
{
    class Program
    {
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
            WriteLineColor("     CoopAndreas — Odinstalowanie", ConsoleColor.White);
            WriteLineColor("  ══════════════════════════════════════════════════════════════════\n", ConsoleColor.DarkYellow);
        }

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

        static void KillIfRunning(params string[] names)
        {
            foreach (var name in names)
            {
                foreach (var proc in Process.GetProcessesByName(name))
                {
                    try
                    {
                        Info("Zatrzymuje: " + name);
                        proc.Kill();
                        proc.WaitForExit(3000);
                    }
                    catch { }
                }
            }
        }

        static void RestoreSamp(string gameDir)
        {
            string[] files = { "samp.dll", "samp.exe", "samp.saa" };
            foreach (var f in files)
            {
                string disabled = Path.Combine(gameDir, f + ".coop_disabled");
                string original = Path.Combine(gameDir, f);
                if (File.Exists(disabled) && !File.Exists(original))
                {
                    File.Move(disabled, original);
                    Ok("Przywrocono SA:MP: " + f);
                }
            }
        }

        static int Main(string[] args)
        {
            try
            {
                Console.OutputEncoding = Encoding.UTF8;
                PrintBanner();

                string startDir = Path.GetDirectoryName(System.Reflection.Assembly.GetExecutingAssembly().Location);
                string gtaDir = ResolveGtaDir(startDir);
                if (gtaDir == null)
                {
                    Err("Nie znaleziono gta_sa.exe!");
                    Console.ReadKey();
                    return 1;
                }

                Info("Katalog GTA: " + gtaDir);
                Console.WriteLine();
                Warn("Czy na pewno chcesz odinstalowac CoopAndreas?");
                Console.Write("  Wpisz T aby potwierdzic: ");
                var key = Console.ReadKey();
                Console.WriteLine();
                if (key.KeyChar != 't' && key.KeyChar != 'T')
                {
                    Info("Anulowano.");
                    Console.ReadKey();
                    return 0;
                }

                KillIfRunning("server", "gta_sa");

                Console.WriteLine();
                Info("Usuwanie plikow CoopAndreas...");

                // Files to delete
                string[] modFiles = {
                    "CoopAndreasSA.dll",
                    "server.exe",
                    "proxy.dll",
                    "VERSION.txt",
                    "stream.ini",
                    "server-config.ini",
                    "CoopAndreas_SAMP_INFO.txt",
                    "Uruchom CoopAndreas.bat",
                    "Uruchom CoopAndreas Server.bat",
                    "Odinstaluj CoopAndreas.bat",
                    "Przelacz na CoopAndreas.bat",
                    "Przelacz na SA-MP.bat",
                };

                int deleted = 0;
                foreach (var f in modFiles)
                {
                    string path = Path.Combine(gtaDir, f);
                    if (File.Exists(path))
                    {
                        File.Delete(path);
                        deleted++;
                        Ok("Usunieto: " + f);
                    }
                }

                // Restore original EAX.dll
                string eaxOrig = Path.Combine(gtaDir, "eax_orig.dll");
                string eaxDll = Path.Combine(gtaDir, "EAX.dll");
                if (File.Exists(eaxOrig))
                {
                    if (File.Exists(eaxDll)) File.Delete(eaxDll);
                    File.Move(eaxOrig, eaxDll);
                    Ok("Przywrocono oryginalny eax.dll");
                    deleted++;
                }
                else if (File.Exists(eaxDll))
                {
                    File.Delete(eaxDll);
                    deleted++;
                    Ok("Usunieto: EAX.dll (proxy)");
                }

                // Remove CoopAndreas folder (but keep installer/uninstaller EXEs)
                string coopDir = Path.Combine(gtaDir, "CoopAndreas");
                if (Directory.Exists(coopDir))
                {
                    // Keep these files — user needs them to reinstall
                    string selfPath = Path.GetFullPath(System.Reflection.Assembly.GetExecutingAssembly().Location);
                    string installerPath = Path.GetFullPath(Path.Combine(Path.GetDirectoryName(selfPath), "CoopAndreasInstaller.exe"));
                    var keepFiles = new HashSet<string>(StringComparer.OrdinalIgnoreCase) { selfPath, installerPath };

                    foreach (var f in Directory.GetFiles(coopDir, "*", SearchOption.AllDirectories))
                    {
                        if (keepFiles.Contains(Path.GetFullPath(f)))
                            continue;
                        try { File.Delete(f); deleted++; } catch { }
                    }
                    // Clean empty subdirectories
                    foreach (var d in Directory.GetDirectories(coopDir, "*", SearchOption.AllDirectories).Reverse())
                    {
                        try
                        {
                            if (Directory.GetFileSystemEntries(d).Length == 0)
                                Directory.Delete(d);
                        }
                        catch { }
                    }
                    Ok("Wyczyszczono folder CoopAndreas");
                }

                // Remove backup folders
                var backups = Directory.GetDirectories(gtaDir, "CoopAndreas_backup_*");
                foreach (var b in backups)
                {
                    try
                    {
                        Directory.Delete(b, true);
                        deleted++;
                        Ok("Usunieto backup: " + Path.GetFileName(b));
                    }
                    catch { }
                }

                // Remove crash logs
                string crashDir = Path.Combine(gtaDir, "CoopAndreas_crashes");
                if (Directory.Exists(crashDir))
                {
                    try { Directory.Delete(crashDir, true); Ok("Usunieto logi crash."); } catch { }
                }

                // Restore SA:MP if was disabled
                RestoreSamp(gtaDir);

                Console.WriteLine();
                WriteLineColor("  ══════════════════════════════════════════════════════════", ConsoleColor.Green);
                WriteLineColor(string.Format("     Odinstalowanie zakonczone ({0} plikow usunietych)", deleted), ConsoleColor.Green);
                WriteLineColor("  ══════════════════════════════════════════════════════════", ConsoleColor.Green);
                Console.WriteLine();
                Info("Pliki gry (gta_sa.exe, dinput8.dll, SilentPatch) nie zostaly usuniete.");
                Info("Aby usunac tez ASI Loader — usun dinput8.dll recznie.");
                Console.WriteLine();
                Ok("Nacisnij dowolny klawisz aby zamknac...");
                Console.ReadKey();

                // Schedule self-deletion
                string selfExe = System.Reflection.Assembly.GetExecutingAssembly().Location;
                string coopDirFinal = Path.Combine(gtaDir, "CoopAndreas");
                string batchPath = Path.Combine(Path.GetTempPath(), "coop_cleanup_" + Guid.NewGuid().ToString("N").Substring(0, 8) + ".bat");
                File.WriteAllText(batchPath, string.Format(
                    "@echo off\nping -n 2 127.0.0.1 >nul\ndel /q \"{0}\" 2>nul\nrd /q \"{1}\" 2>nul\nrd /q \"{2}\" 2>nul\ndel /q \"%~f0\"\n",
                    selfExe,
                    Path.GetDirectoryName(selfExe),
                    coopDirFinal),
                    Encoding.ASCII);
                Process.Start(new ProcessStartInfo
                {
                    FileName = batchPath,
                    WindowStyle = ProcessWindowStyle.Hidden,
                    UseShellExecute = true
                });

                return 0;
            }
            catch (Exception ex)
            {
                Err("Blad: " + ex.Message);
                Console.ReadKey();
                return 1;
            }
        }
    }
}
