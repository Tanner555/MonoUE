These are some basic notes on upgrading MonoUE to a new upstream engine version.

MonoUE branches are based on the upstream release branches. Unfortunately upstream branches are independently synced from perforce so it's tricky to merg/sync between upstream branches e.g. `upstream/master`, `upstream/4.18` and `upstream/release`. As a consequence, all MonoUE branches are based on `upstream/release`.

To update Mono UE e.g. from 4.17 to 4.18:

List engine patches between current MonoUE and upstream release, excluding MonoUE directory.

```bash
git fetch --all
git log --oneline upstream/release..origin/monoue-4.17 -- . ":^Engine/Plugins/MonoUE" > patches.txt
```

Find the commit ID of the first patch in the last patch branch

```bash
git log --oneline upstream/release..origin/monoue-patches-4.17 | tail -n 1
```

Open the `patches.txt` file. Remove all commits after the one you found in  the previous command. Also remove any merge commits and any patches that are known to have been merged upstream.

Now you have the list of patches that comprise the engine patchset.

Checkout the upstream release branch and use it to create a new patch branch

```bash
git checkout -t upstream/release
git checkout -b monoue-patches-4.18
```

Bottom first, cherry-pick all the commits from the engine patch onto this branch and fix them up as you go.

Next, do an interactive rebase and squash any fixup commits into the commits they're fixing.

Check the branch builds. If it doesn't, fix it, commit the fixes, and rebase to squash fixes into the patches they fix.

Now check out the latest MonoUE branch and merge the new patch branch into it. The patch branch contains the latest version of the engine and the cleaned up patchset. It should merge pretty cleanly, and if there are conflicts, you should generally pick the incoming changes.

```
git checkout monoue-4.17
git merge monoue-patches-4.18
```

Fix any merge errors, commit the result. Build and fix any errors. You now have an upgraded MonoUE branch.

Don't forget to upstream any patches in the patchset that haven't yet been upstreamed.
