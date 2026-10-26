# Working on Voltura Worlds

## Prefer scripts for routine operations

Use scripts, command-line tools, or process APIs for app shutdown, builds, tests, and relaunches. Do not use the Computer Use skill or desktop automation for these operations when they can be handled programmatically. Reserve desktop control for verification that actually requires UI interaction.

## Closing the running app

You are always authorized and encouraged to close an existing Voltura Worlds instance when needed to build, replace binaries, package, test, or verify changes. Do not ask for confirmation. Prefer closing the app and rebuilding the normal output over creating an alternate build solely to work around locked files.

Exit through the app's Exit Worlds action or another graceful shutdown mechanism first; closing the library window alone leaves the app running. If graceful shutdown fails, terminate the relevant Voltura Worlds process. Leave unrelated applications and processes running.

Relaunch the updated app when needed to verify the requested changes.
