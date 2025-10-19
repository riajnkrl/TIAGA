# Secure removal of keystore and key.properties from repository

This project currently contains signing artefacts in the workspace (keystore files and `key.properties`). These are secrets and should not be stored in the git repo. The repository in this environment is not a git repository, so I could not rewrite history here. Follow the instructions below on your local machine (where the repository is a git checkout) to remove the secrets safely.

Two approaches are provided:

- Option A — Non-destructive (recommended if you're unsure)
  - Removes the tracked files from the working tree and adds them to `.gitignore` so they are no longer committed.
  - Does NOT rewrite history.
  - Safe for teams and does not require force-pushing.

- Option B — Full purge (recommended if you want the secret removed from all commits)
  - Rewrites git history to remove files from all commits using `git-filter-repo` (faster and recommended) or BFG.
  - Requires force-pushing and coordination with all collaborators.

---

Prerequisites
- Run these commands from the repository root (where `.git` exists).
- Make a local backup branch before rewriting history.

Option A — Non-destructive removal

PowerShell commands (safe):
```powershell
# 1) Remove the files from the index (stop tracking) and keep them locally
git rm --cached android/key.properties
git rm --cached android/app/tiaga_upload_key_jks.jks
git rm --cached android/app/tiaga_upload_key.jks

# 2) Commit the change and push
git commit -m "Remove keystore and key.properties from repo; added to .gitignore"
git push origin $(git rev-parse --abbrev-ref HEAD)
```

What this does
- The files remain on your local disk, are removed from the repository index, and future commits won't include them because `.gitignore` was already updated.
- The secret still exists in prior commit history (it is not purged). If you need it removed from history, see Option B.

Option B — Full purge from git history (destructive)

Use `git-filter-repo` (recommended) or BFG as an alternative. This will rewrite history locally. Do NOT run this if you cannot coordinate with other repo users.

Steps (PowerShell):
```powershell
# 0) Create a safety branch (backup)
git checkout -b backup-before-key-purge

# 1) Install git-filter-repo if you don't have it. On Windows you can install with pip:
python -m pip install --user git-filter-repo

# 2) Make sure you are on the branch you want to rewrite (e.g., main)
git checkout main

# 3) Run git-filter-repo to remove the paths (this rewrites history locally)
git filter-repo --path android/app/tiaga_upload_key_jks.jks --path android/app/tiaga_upload_key.jks --path android/key.properties --invert-paths

# 4) Inspect the repo and ensure the secret is gone. Search commit history for the filename:
git log --all --name-only --pretty=format:%H | Select-String "tiaga_upload_key|key.properties" -SimpleMatch

# 5) When satisfied, force-push the rewritten history (destructive!). Coordinate with collaborators first.
git push --force origin main
```

Notes and warnings for Option B
- Rewriting history invalidates commit hashes. All collaborators must re-clone or reset to the new history.
- Any forks or mirrors may still contain the secret; you may need to contact the hosting provider if full deletion is required.
- If the key was actually exposed publicly, consider rotating the signing key (create a new keystore) and updating Play Console settings. If you are using Play App Signing, follow Google's instructions for key rotation.

Alternative: BFG Repo-Cleaner
- BFG is a user-friendly alternative to remove files from history. See https://rtyley.github.io/bfg-repo-cleaner/ for usage.

Remediation checklist (after purge)
- Rotate the keystore if it's believed to be exposed widely.
- Put the keystore into a secure store (password manager, cloud secret manager, or CI secret vault).
- Add usage documentation for CI (how to inject the keystore at build time), and remove any `key.properties` committed to the repo.

If you want me to proceed with the actual purge here, I need the repo to be a git repository in this workspace (i.e., `.git` present) or remote access/permissions to force-push. Tell me which workflow you prefer and I will either:
- provide a step-by-step interactive script for you to run locally, or
- run the purge here (and show results) and wait for your approval before force-pushing.

---

Contact me with your choice and I'll continue. If you want the non-destructive removal applied here, I can also delete the keystore files from the working tree right away (they'd still exist for any remote clones until you purge history).
