# ojpkg

**The OJLinux Package Manager**

Minimal by design. Explicit by choice.

`ojpkg` is the native package manager being developed for OJLinux. It is written in C and built around a straightforward idea: package management should be understandable, inspectable, and controlled by the user.

OJLinux does not aim to provide everything out of the box. `ojpkg` follows the same philosophy. It provides the foundations for managing software without hiding its internal state behind unnecessary complexity.

> **It boots. It's your problem now.**

---

## Design Philosophy

`ojpkg` follows three principles:

* **Minimalism** — Keep the implementation and its dependencies small where practical.
* **Transparency** — Store package metadata in readable files that can be inspected and diagnosed with ordinary tools.
* **User control** — Make software installation an explicit decision rather than an automatic background process.

The goal is not to reproduce an existing package manager feature for feature. `ojpkg` takes inspiration from the simplicity of traditional Linux package management while developing its own approach to package history, integrity, updates, and recovery.

## Current Status

`ojpkg` is an early-stage project under active development.

The current implementation provides a working foundation for:

* Installing packages from local repositories or package archives.
* Resolving package dependencies and detecting dependency cycles.
* Querying installed packages and displaying package information.
* Removing packages while checking reverse dependencies.
* Reading package metadata from a simple archive format.
* Running optional installation hooks.

The current test suite exercises the basic package lifecycle, including dependency installation, queries, and removal.

**This is not yet a production-ready package manager.** Transaction recovery, reliable rollback, repository authentication, and several other safeguards remain future work. Do not use the current implementation to manage an important system without understanding its limitations.

## Architecture

The codebase is divided into focused components:

| Component         | Responsibility                                            |
| ----------------- | --------------------------------------------------------- |
| `pkg.c`           | Package metadata parsing, writing, and version comparison |
| `archive_ojpkg.c` | Package archive inspection and extraction                 |
| `db.c`            | Installed-package database                                |
| `repo.c`          | Discovery of packages in local repositories               |
| `resolve.c`       | Dependency resolution and cycle detection                 |
| `install.c`       | Installation, removal, conflict checks, and hooks         |
| `main.c`          | Command-line interface                                    |
| `util.c`          | Shared utility functions                                  |

Public interfaces are defined in `include/`.

This separation is intended to keep the implementation maintainable as the project gains transaction handling, package history, and update support.

## Package Format

An `.ojpkg` package is a compressed tar archive containing package metadata and the files to be installed.

A package can contain:

* **`.PKGINFO`** — Metadata such as package name, version, description, architecture, size, dependencies, provided features, and conflicts.
* **`.INSTALL`** — An optional shell script defining installation hooks.
* **Package files** — Files stored under their intended relative paths, such as `usr/bin/example`.

The metadata format uses readable `key = value` entries.

Installation hooks execute shell code and must therefore be treated as trusted code. Only install packages from sources you trust.

## Installed-Package Database

The local database is stored beneath:

```text
/var/lib/ojpkg/local/
```

Each installed package has its own directory containing files such as:

```text
desc
files
```

* `desc` stores package metadata.
* `files` records files associated with the installed package.

The database uses plain files rather than requiring an SQL database engine. This keeps the format inspectable with ordinary command-line tools and makes manual diagnosis possible.

Readability is not a substitute for integrity, however. Atomic database updates, locking, transaction recovery, and stronger consistency guarantees are still development goals.

## Building from Source

Building from source is the primary installation path during early development.

### Requirements

* A C compiler compatible with the project's C11 build.
* GNU Make.
* The `libarchive` development library and headers.

### Build

Clone the repository and compile:

```sh
git clone https://github.com/ABO7GAG/OJPKG.git
cd ojpkg
make
```

The resulting executable is:

```text
ojpkg
```

Review the source code before building or installing software with elevated privileges.

## Usage

### Search a local repository

```sh
./ojpkg --repo ./myrepo search foo
```

A repository is currently a directory containing `.ojpkg` package archives.

### Install a package by name

```sh
./ojpkg --repo ./myrepo install foo
```

The resolver attempts to locate and install required dependencies from the configured local repositories.

### Install a package archive directly

```sh
./ojpkg install ./foo-1.0.0-1.ojpkg
```

Direct archive installation does not perform the same repository-based dependency resolution as installation by package name.

### Query installed packages

```sh
./ojpkg query
```

Display information about a specific installed package:

```sh
./ojpkg query foo
```

### Remove a package

```sh
./ojpkg remove foo
```

Removal is refused when another installed package depends on the target package.

### Use an isolated test root

```sh
./ojpkg --root /tmp/ojpkg-root \
  --repo ./myrepo \
  install foo
```

Use an isolated root while experimenting. Do not assume that a test root reproduces every behavior of a real system installation.

## Testing

The repository includes a basic end-to-end test:

```sh
sh test/run_test.sh
```

The test exercises a local repository containing example packages, dependency resolution, installation hooks, installed-package queries, dependency protection during removal, and successful removal.

Passing these tests demonstrates that the tested paths work. It does not establish security against malicious packages, crash consistency, or production readiness.

## Current Limitations

The following capabilities are not yet complete:

* **Transactions:** Installation and removal do not yet provide full atomicity or reliable recovery from every failure.
* **Rollback:** Previous package states cannot yet be restored through a complete rollback mechanism.
* **Update management:** A complete update-checking and bulk-upgrade workflow is still planned.
* **Package history:** Download, installation, and update timestamps are not yet maintained as a complete history.
* **Repository security:** Cryptographic repository authentication and package-signature verification are not yet implemented.
* **Database integrity:** Locking, atomic writes, and comprehensive consistency checks remain necessary.
* **Version semantics:** Version comparison is intentionally limited and does not implement every distribution-specific versioning convention.
* **Network synchronization:** Repositories are local directories; automatic remote repository synchronization is not yet implemented.
* **Removal hooks:** A complete lifecycle for pre-removal and post-removal hooks remains future work.

These limitations are documented so that users can distinguish existing behavior from planned functionality.

## Roadmap

Development priorities are focused on correctness before feature expansion:

1. Strengthen archive extraction and filesystem safety.
2. Establish strict database invariants and safe update procedures.
3. Introduce a transaction journal and recovery from interrupted operations.
4. Record package installation, download, and update history.
5. Implement update discovery and an `upg-list` command.
6. Add integrity verification and repository authentication.
7. Develop a reliable package rollback mechanism.
8. Evaluate remote repositories and additional distribution tooling.

The priority is not to maximize the number of commands. It is to make the operations that exist predictable and safe.

## Project

**OJLinux (OJL)**
Package manager: `ojpkg`
Creator: [ABO-7GAG](https://github.com/ABO7GAG)
another profile: [ABO-7GAG](https://codeberg.org/ABO-7GAG)

Source code of OJLinux: [github.com/ABO7GAG/OJLinux](https://github.com/ABO7GAG/OJLinux)

`ojpkg` is part of the OJLinux project and follows its goal of providing a minimal Linux foundation with explicit user control.

---

*Small foundation. Clear rules. Your system, your decisions.*

