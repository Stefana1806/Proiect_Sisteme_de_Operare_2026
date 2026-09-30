# Operating Systems Project 

A systems programming project developed in **C** and **Bash**, focusing on file operations, database indexing, process management, and concurrency.

## Features

* **File Operations & Indexing:**
  * File indexing and indexing management (`fileops_indexer`, `fileops_manager`, `fileops_worker`).
  * Automated tools and utilities for file system auditing.
* **Database Management:**
  * Custom database formats and diff tools (`db_diff`).
  * Structured data persistence (`inventory.db`, database indexes).
* **Process & Control Plane:**
  * System process snapshots and management (`proc_snapshot`).
  * Concurrency and inter-process communication (IPC) handling using scripts and signals.

## Project Structure

* `src/` - Source code files in C implementing core modules (file operations, database diffing, process snapshots).
* `include/` - Header files for inter-process communication and shared utilities.
* `tests/` - Bash scripts for testing concurrency, inventory, and environment initialization.
* `tools/` - Utility and audit shell scripts.
* `doc/` - Documentation covering formats, protocols, and control plane specifications.
