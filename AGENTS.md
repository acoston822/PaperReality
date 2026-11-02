# Repository instructions

## Releases

The maintainer requires every release's source changes to be committed and merged
into `main` as part of the release process.

- Build and validate the intended release source, app version and release notes.
- Merge the release source into `main` before publishing the release. Use a normal
  merge that preserves independent changes on `main`; never force-reset `main`.
- Verify that the release commit is an ancestor of `main` (or its tip).
- The publication workflow performs this merge and ancestry check. If branch
  protection or conflicts block it, resolve them through the normal pull-request
  process before retrying publication. Do not bypass protections.
- Publish the matching source tag and app asset. Do not report a release complete
  until both publication and source integration into `main` are verified.
- Never upload user game files, ROMs or texture packs to GitHub.
