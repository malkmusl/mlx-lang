# Prompt: Login und Init fertigstellen

Ein Arbeitsauftrag für einen externen Assistenten (ChatGPT): den Login
(mlx-login, Greeter, Sperrbildschirm) und mlx-init fertig zu
implementieren. Alles unter dem Strich wird so eingefügt.

---

Du arbeitest am Repository https://github.com/malkmusl/mlx-lang, Branch `claude/linux-wayland-implementation-qf4044`. Es ist eine eigene Programmiersprache (Mlx) mit eigenem Compiler und darauf ein eigenes Linux-Userland: Init, Shell, Wayland-Compositor, Desktop-Apps, Audio, Netzwerk. Ziel ist ein System ohne GNU und ohne fremde Bibliotheken. Deine Aufgabe: **den Login (mlx-login, Greeter, Sperrbildschirm) und mlx-init fertig implementieren.**

## Feste Regeln

1. Alles in Mlx, ohne libc, ohne PAM, ohne systemd-logind, ohne D-Bus. Systemaufrufe direkt über `std.linux.syscall*`, wie im restlichen Code.
2. Der Systembus ist MLXIPC: `mlx-ipcd --system` unter `/run/mlx/bus` (projects/desktop/ipc, README dort). Nur root darf dort Namen besitzen. Rechte pro App laufen über `org.mlx.IPC.Check(sender, permission)`. Vorbild für asynchrone Prüfung: projects/network/bus.mlx (`ask`, `answered`).
3. Stil wie im bestehenden Code: Kommentare als englische Prosa, die erklären warum. Nicht genutzte Ergebnisse als `const ignored = ...`. Dateien unter 1500 Zeilen. Jedes Projekt hat ein README, das du mitpflegst.
4. Bauen: `mlx-out/bin/compiler/mlx4 --quiet DATEI.mlx -o PROGRAMM` (oder `tools/ensure_compiler.sh`). Jede Funktion bekommt einen Check in `tools/check_*.sh`. Die bestehenden Checks müssen grün bleiben: check_login.sh, check_init.sh, check_ipc.sh, check_network.sh, check_wayland_compositor.sh, check_desktop_clients.sh, check_compositor_session.sh.
5. Installer arbeiten mit `--destdir DIR` (Staging) und `--enable`, wie tools/install_network.sh. Ohne `--destdir` nur als root.
6. Lies vor dem Schreiben die vorhandenen Dateien. Gute Vorlagen: projects/network (Daemon, Bus, Installer, Check), projects/desktop/settings/network.mlx (Passwortfeld mit Sternen, Löschen nach Gebrauch), projects/desktop/compositor/lock.mlx (ext-session-lock-v1).

## Bekannte Lücken im Compiler (umgehen, nicht reparieren)

- `&wert.feld` hat den Typ `*const T`. Für einen veränderbaren Zeiger: `@ptrCast(*T, &wert.feld)`.
- Schreiben über einen Zeiger auf ein Array (`text.*[i] = x`) scheitert. Stattdessen ein `[*]u8` per `@ptrCast` bzw. `@ptrFromInt(adresse + @offsetOf(...))`, wie `bytesAt` im Code.
- Strukturtypen vor ihrer Verwendung deklarieren. Ein Modul-Typ aus einem Enum desselben Moduls löst nicht auf.
- `@intCast(u64, @bitCast(u32, negatives_i32))` erweitert mit Vorzeichen. Mit `& 4294967295` maskieren.
- Verschachtelte Array-Literale (`[2][3]u16{...}`) stürzen ab. Keine Gleitkomma-Konstanten auf Modulebene.
- Mehr als sechs Parameter mit gemischtem usize/f64 lösen nicht auf. Lange Funktionen können die Adresse lokaler Structs verlieren: Funktionen kurz halten.
- Doppelte Feldnamen in Structs werden still angenommen: aufpassen.

## Teil A: mlx-login fertigstellen (projects/desktop/login)

Der Daemon läuft schon: er besitzt `org.mlx.Login`, `ListUsers` antwortet, `Login` und `Unlock` antworten mit `NotConfigured`. Es fehlen die beiden markierten Funktionen `authenticate` und `startSession` in main.mlx. Wie sie gebaut werden, steht in den Kommentaren der beiden Stubs und im README unter "The two privileged steps". `std.crypt.verify` (std/src/crypt.mlx) prüft yescrypt, `$6$` und `$5$` bereits in konstanter Zeit.

Danach:
- Merken, wer den Sitz hat (für `Unlock`). Endet die Sitzung, startet der Greeter wieder.
- Sicherheitsanforderungen: Passwörter nie loggen und nach Gebrauch überschreiben. Die Verzögerung nach Fehlversuchen bleibt. Gesperrte oder leere Konten werden abgelehnt. Kann die Hash-Datei nicht gelesen werden, wird abgelehnt, nie durchgelassen. Wer `Login`/`Unlock` aufrufen darf, entscheidet ein MLXIPC-Recht (z. B. `login.session`, standardmäßig nur der Greeter und der Sperrbildschirm).
- Tests lesen niemals die echte `/etc/shadow` und kein echtes Passwort. Gib dem Daemon Optionen wie `--passwd DATEI` und `--shadow DATEI` und erzeuge im Check Testkonten mit bekannten Testpasswörtern. `startSession` testest du in einem Benutzer-Namespace (`unshare`), ohne echte Rechte.
- Phase 2: die Teilmenge von `org.freedesktop.login1`, die der Compositor nutzt (projects/desktop/compositor/logind.mlx: TakeControl, TakeDevice, ReleaseDevice, Pause/ResumeDevice, VT-Wechsel), auf dem MLXIPC-Systembus beantworten.

## Teil B: Greeter und Sperrbildschirm

- Compositor im Greeter-Modus (z. B. `--greeter PROGRAMM`): nur der Greeter, kein Dock, keine Leiste, keine Hotkeys außer VT-Wechsel.
- `mlx-greeter` (neues Projekt, auf std.ui.host wie mlx-settings): Benutzerliste aus `ListUsers`, Passwortfeld mit Sternen, Sitzungsauswahl aus den `wayland-sessions`-Einträgen, Aufruf von `Login`, Fehlermeldung auf Deutsch und Englisch, Hell/Dunkel über std.ui.appearance, Optik wie die übrigen Apps.
- `mlx-lock`: eigener Sperrbildschirm über ext-session-lock-v1, entsperrt mit `Unlock`. Er ersetzt `swaylock` als Standard für `lock-command`.
- Checks mit dem verschachtelten Compositor und tools/wayland-test-host, wie in check_wayland_compositor.sh.

## Teil C: Installation

- `tools/install_login.sh` mit `--destdir` und `--enable`: mlx-login, mlx-greeter, mlx-lock, eine systemd-Unit `mlx-login.service` (braucht `mlx-ipcd.service`, als `display-manager.service`), dazu Dienste für mlx-init.
- Mit `--enable` den bisherigen Display-Manager maskieren und rückgängig machbar halten. Im Check nur mit `--destdir` arbeiten.

## Teil D: mlx-init (projects/init, README-Abschnitt "Not yet")

In dieser Reihenfolge:
1. `user = NAME` und `group = NAME` für Dienste.
2. Geordnetes Herunterfahren: Abhängige Dienste zuerst stoppen und auf sie warten.
3. Dauerhafte Logs unter `/var/log/mlx` mit Rotation, sobald das Dateisystem schreibbar ist.
4. Geräte: uevents über Netlink lesen, beim Start `/sys` durchgehen (Coldplug), Rechte und Gruppen für `/dev/dri`, `/dev/input` und `/dev/snd` setzen.
5. Bereitschaft auch über einen Socket oder eine Nachricht, nicht nur über einen Pfad.
6. `/etc/fstab` vollständig auswerten und eine Dateisystemprüfung einhängen.
7. Ein Beispiel-Dienstsatz, der vom Boot bis zum Greeter alles startet: mlx-ipcd, mlx-netd, mlx-login.

Jeder Schritt bekommt Tests in `tools/check_init.sh`, im Namespace wie die bestehenden.

## Arbeitsweise und Ausgabe

- Arbeite in kleinen Schritten, einer pro Antwort. Zu jedem Schritt: was er macht, die vollständigen neuen oder geänderten Dateien (oder einen sauberen Diff), die Ergänzung im Check und die Befehle zum Bauen und Prüfen.
- Ich führe die Befehle aus und schicke dir die Ausgabe. Behaupte nie, dass etwas läuft, das du nicht gesehen hast.
- Wenn ein Compiler-Fehler kommt, umgehe ihn im Stil der Liste oben und nenne die Lücke.
- Beginne mit Teil A. Lies zuerst projects/desktop/login/README.md, main.mlx, users.mlx, tools/check_login.sh und std/src/crypt.mlx und fasse in fünf Sätzen zusammen, was du vorhast.
