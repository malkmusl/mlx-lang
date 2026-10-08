# mlx-permissions

`mlx-permissions` is where the desktop asks the user whether an app may
do something: MLXIPC's permission agent (`projects/desktop/ipc`) and its
dialog, in Mlx on `std.dbus` and `std.ui.host` (no libc, no GTK). It is
also what polkit's authentication agent will stand on (below).

```sh
mlx4 projects/desktop/permissions/main.mlx -o mlx-permissions
mlx-permissions --ask org.example.Chat audio.record --sandboxed   # the dialog alone
tools/check_audio.sh                  # asking, answers, rules, revoking
tools/check_wayland_compositor.sh     # scenario 15: the dialog clicked
```

## How a question is asked

1. A service checks an app's permission with the bus
   (`org.mlx.IPC.Check`), and the policy says `ask`: by default, a
   sandboxed app wanting the microphone (`audio.record`) or the sound of
   other apps (`audio.monitor`), or a rule of `ipc.conf` such as
   `ask org.example.Chat use audio.record`.
2. The bus holds the Check and calls
   `org.mlx.PermissionAgent.Ask(s app, s permission, b sandboxed) -> (b
   allowed, b remember)`. Nobody owns that name at first: the bus starts
   `mlx-permissions` from its service file
   (`PREFIX/share/mlx/dbus-1/services/org.mlx.PermissionAgent.service`,
   installed by `tools/install_compositor_session.sh`) and asks once it
   owns the name. The agent stays for the session.
3. The agent takes Ask from the bus only (`org.freedesktop.DBus` as the
   sender, which no app can be): otherwise an app could put a question of
   its own making into the desktop's dialog. It starts itself again as
   the dialog (`--ask APP PERMISSION [--sandboxed] --answer-fd N`), one
   process per question, and reads the answer from a pipe.
4. The dialog shows the app's name and icon (from its desktop entry, its
   id otherwise), what it wants ("use the microphone"), whether it runs
   in a sandbox, a "Remember this decision" box, Deny and Allow. Closing
   it, Escape, or two minutes without an answer are a no. German when the
   locale is.
5. The bus answers every Check waiting on that question. Remembered, the
   answer is a rule of `ipc.conf`; otherwise it holds for the session.
   Either can be changed in mlx-settings' Apps category
   (`projects/desktop/settings/permissions.mlx`: every app that asked,
   each permission with Allow, Ask, Deny or Default;
   `org.mlx.IPC.ListPermissions` and `SetPermission`), and services check
   again at once (`org.mlx.IPC.PermissionsChanged`).

The dialog needs the compositor: the agent passes its own
`WAYLAND_DISPLAY`, else the Mlx compositor's socket
(`$XDG_RUNTIME_DIR/wayland-mlx`), since `mlx-session` starts the bus
before the compositor.

```
mlx-permissions                     the agent (the bus starts it)
mlx-permissions --answer ANSWER     an agent answering at once: allow, deny,
                                    allow-remember, deny-remember (tests)
mlx-permissions --ask APP PERMISSION [--sandboxed] [--answer-fd N]
                                    the dialog: "allow" or "deny", and
                                    " remember", to N or printed
```

## polkit

polkit decides what a user may do to the system (mount a disk, change the
network, install packages) and, when its rules say so, has the user
authenticate: it calls the session's authentication agent,
`org.freedesktop.PolicyKit1.AuthenticationAgent.BeginAuthentication(s
action_id, s message, s icon_name, a{ss} details, s cookie, a(sa{sv})
identities)`, on the **system** bus, and the agent answers through
`polkit-agent-helper-1` (setuid root, PAM), which hands polkitd the
cookie once the password was right. The agent registers itself with
`org.freedesktop.PolicyKit1.Authority.RegisterAuthenticationAgent` for
its session.

That is the same shape as asking here: a question from a trusted daemon,
a dialog, an answer back. The plan:

- `std.dbus` connects to the system bus as well (its address,
  `/run/dbus/system_bus_socket`); the agent registers for the session
  (`unix-session` subject, `XDG_SESSION_ID`) and owns the object
  `/org/mlx/PolicyKit1/AuthenticationAgent`.
- BeginAuthentication opens the dialog in a second form: the action's
  message and icon, the identity (the user, or a choice among the admin
  users), a password field (`std.ui.field`, hidden), Cancel and
  Authenticate.
- The password goes to `polkit-agent-helper-1 USER` on its standard
  input, with the cookie, as GNOME's and KDE's agents do; its "SUCCESS"
  or "FAILURE" ends the call (a failure lets the user try again).
  CancelAuthentication closes the dialog.
- MLXIPC's own questions stay as they are; both kinds share the dialog
  and the rule that only the bus (or polkitd, on the system bus) asks.
