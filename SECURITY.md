# Security Policy

## Supported Versions

Security fixes target the latest published release of Voltura Worlds. Please update to the latest release before reporting an issue where possible. Older versions may require an upgrade to receive a fix.

## Reporting a Vulnerability

Please report suspected security vulnerabilities privately through GitHub:

https://github.com/voltura/voltura-worlds/security/advisories/new

Do not disclose vulnerabilities, exploit code, or sensitive information in public issues or pull requests before coordinated disclosure.

Include as much of the following as possible:

- A description of the vulnerability and its potential impact.
- The affected version or commit.
- Your Windows version and relevant configuration.
- Steps to reproduce the issue.
- A minimal proof of concept or sample file, where appropriate.
- Any suggested mitigation or fix.

Remove personal information, credentials, and unrelated private data from reports and attachments.

## Security Scope

Reports may concern any component maintained in this repository, including:

- The Windows application and desktop integration.
- Video imports and `.world` preset packages.
- Installation, automatic updates, and update verification.
- Build, packaging, signing, and release workflows.
- The product website.

Examples of relevant issues include arbitrary code execution, unintended file access or overwrites, path traversal, privilege escalation, disclosure of private data, and bypasses of update integrity or publisher verification.

For dependency vulnerabilities, describe how the affected dependency is used by Voltura Worlds and how the issue could affect its users.

## Responsible Research

Only investigate systems and data you own or have explicit permission to use. Avoid accessing other people's data, disrupting services, or modifying shared infrastructure.

Use the smallest proof of concept needed to demonstrate the issue, and stop if you encounter sensitive information.

## Handling and Disclosure

Reports will be assessed based on reproducibility, realistic impact, and affected components. We may request additional information.

Please coordinate public disclosure with the maintainers so users have an opportunity to install a fix or apply a mitigation. Response and resolution times depend on the issue and maintainer availability.
