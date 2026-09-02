# Client update publication

Publishing a client update is complete only after all of the following steps succeed:

1. Publish the validated Windows and macOS ZIPs in the matching `NarutoOldWar-Client` GitHub Release.
2. Update `updates/latest.json` with the same version, changelog, and exact URLs of both published assets.
3. Validate the JSON and confirm both Release asset URLs exist before committing.
4. Commit and push the manifest to `NarutoOldWar-Launcher` `main`.
5. Read the raw production manifest after the push and confirm it exposes the new version.

Never report that an update is available in the launcher after creating only the client Release. Do not rebuild or publish the launcher application when only `updates/latest.json` needs to change.
